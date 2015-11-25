/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  Sentinel2 products
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 * Funded by: Centre National d'Etudes Spatiales (CNES)
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault, <even.rouault at spatialys.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_minixml.h"
#include "cpl_string.h"
#include "gdal_pam.h"
#include "gdal_proxy.h"
#include "ogr_spatialref.h"
#include "../vrt/vrtdataset.h"
#include <set>
#include <map>
#include <vector>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef STARTS_WITH_CI
#define STARTS_WITH_CI(a,b) EQUALN(a,b,strlen(b))
#endif

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_SENTINEL2(void);
CPL_C_END


typedef struct
{
    const char* pszBandName;
    int         nResolution; /* meters */
    int         nWaveLength; /* nanometers */
    int         nBandWidth;  /* nanometers */
    GDALColorInterp eColorInterp;
} SENTINEL2BandDescription;

static const SENTINEL2BandDescription asBandDesc[] =
{
    { "B1", 60, 443, 20, GCI_Undefined },
    { "B2", 10, 490, 65, GCI_BlueBand },
    { "B3", 10, 560, 35, GCI_GreenBand },
    { "B4", 10, 665, 30, GCI_RedBand },
    { "B5", 20, 705, 15, GCI_Undefined },
    { "B6", 20, 740, 15, GCI_Undefined },
    { "B7", 20, 783, 20, GCI_Undefined },
    { "B8", 10, 842, 115, GCI_Undefined },
    { "B8A", 20, 865, 20, GCI_Undefined },
    { "B9", 60, 945, 20, GCI_Undefined },
    { "B10", 60, 1375, 30, GCI_Undefined },
    { "B11", 20, 1610, 90, GCI_Undefined },
    { "B12", 20, 2190, 180, GCI_Undefined },
};

#define NB_BANDS (sizeof(asBandDesc)/sizeof(asBandDesc[0]))

/************************************************************************/
/*                           SENTINEL2GranuleInfo                       */
/************************************************************************/

class SENTINEL2GranuleInfo
{
    public:
        CPLString osPath;
        double    dfMinX, dfMinY, dfMaxX, dfMaxY;
        int       nWidth, nHeight;
};

/************************************************************************/
/* ==================================================================== */
/*                         SENTINEL2Dataset                             */
/* ==================================================================== */
/************************************************************************/

class SENTINEL2DatasetContainer: public GDALPamDataset
{
    public:
        SENTINEL2DatasetContainer() {}
};

class SENTINEL2Dataset : public VRTDataset
{
        std::vector<CPLString>   aosNonJP2Files;

    public:
                    SENTINEL2Dataset(int nXSize, int nYSize);
                    ~SENTINEL2Dataset();

        virtual char** GetFileList();

        static GDALDataset *Open( GDALOpenInfo * );
        static GDALDataset *OpenL1C( GDALOpenInfo * );
        static GDALDataset *OpenL1CSubdataset( GDALOpenInfo * );

        static int Identify( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                         SENTINEL2AlphaBand                           */
/* ==================================================================== */
/************************************************************************/

class SENTINEL2AlphaBand: public VRTSourcedRasterBand
{
                    int m_nSaturatedVal;
                    int m_nNodataVal;

    public:
                     SENTINEL2AlphaBand( GDALDataset *poDS, int nBand, 
                                         GDALDataType eType, 
                                         int nXSize, int nYSize,
                                         int nSaturatedVal, int nNodataVal );

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
#ifdef GDAL_DCAP_RASTER
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg
#else
                              int nPixelSpace, int nLineSpace
#endif
                              );

};

/************************************************************************/
/*                         SENTINEL2AlphaBand()                         */
/************************************************************************/

SENTINEL2AlphaBand::SENTINEL2AlphaBand( GDALDataset *poDSIn, int nBandIn, 
                                        GDALDataType eType, 
                                        int nXSize, int nYSize,
                                        int nSaturatedVal, int nNodataVal ) :
                            VRTSourcedRasterBand(poDSIn, nBandIn, eType,
                                                 nXSize, nYSize),
                            m_nSaturatedVal(nSaturatedVal),
                            m_nNodataVal(nNodataVal)
{
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr SENTINEL2AlphaBand::IRasterIO( GDALRWFlag eRWFlag,
                                      int nXOff, int nYOff, int nXSize, int nYSize,
                                      void * pData, int nBufXSize, int nBufYSize,
                                      GDALDataType eBufType,
#ifdef GDAL_DCAP_RASTER
                                      GSpacing nPixelSpace, GSpacing nLineSpace,
                                      GDALRasterIOExtraArg* psExtraArg
#else
                                      int nPixelSpace, int nLineSpace
#endif
                                      )
{
    // Query the first band. Quite arbitrary, but hopefully all bands have
    // the same nodata/saturated pixels.
    CPLErr eErr = poDS->GetRasterBand(1)->RasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                            pData, nBufXSize, nBufYSize,
                                            eBufType, nPixelSpace, nLineSpace
#ifdef GDAL_DCAP_RASTER
                                            ,psExtraArg
#endif
                                            );
    if( eErr == CE_None )
    {
        const char* pszNBITS = GetMetadataItem("NBITS", "IMAGE_STRUCTURE");
        const int nBits = (pszNBITS) ? atoi(pszNBITS) : 16;
        const GUInt16 nMaxVal = (GUInt16)((1 << nBits) - 1);

        // Replace pixels matching m_nSaturatedVal and m_nNodataVal by 0
        // and others by the maxVal.
        for(int iY = 0; iY < nBufYSize; iY ++)
        {
            for(int iX = 0; iX < nBufXSize; iX ++)
            {
                // Optimized path for likely most common case
                if( eBufType == GDT_UInt16 )
                {
                    GUInt16* panPtr = (GUInt16*)
                           ((GByte*)pData + iY * nLineSpace + iX * nPixelSpace);
                    if( *panPtr == 0 ||
                        *panPtr == m_nSaturatedVal || *panPtr == m_nNodataVal )
                    {
                        *panPtr = 0;
                    }
                    else
                        *panPtr = nMaxVal;
                }
                // Generic path for other datatypes
                else
                {
                    double dfVal;
                    GDALCopyWords((GByte*)pData + iY * nLineSpace + iX * nPixelSpace,
                                   eBufType, 0,
                                   &dfVal, GDT_Float64, 0,
                                   1);
                    if( dfVal == 0.0 || dfVal == m_nSaturatedVal ||
                        dfVal == m_nNodataVal )
                    {
                        dfVal = 0;
                    }
                    else
                        dfVal = nMaxVal;
                    GDALCopyWords(&dfVal, GDT_Float64, 0,
                                  (GByte*)pData + iY * nLineSpace + iX * nPixelSpace,
                                  eBufType, 0,
                                  1);
                }
            }
        }
    }

    return eErr;
}

/************************************************************************/
/*                          SENTINEL2Dataset()                          */
/************************************************************************/

SENTINEL2Dataset::SENTINEL2Dataset(int nXSize, int nYSize) : VRTDataset(nXSize,nYSize)
{
    poDriver = NULL;
    SetWritable(FALSE);
}

/************************************************************************/
/*                         ~SENTINEL2Dataset()                          */
/************************************************************************/

SENTINEL2Dataset::~SENTINEL2Dataset()
{
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char** SENTINEL2Dataset::GetFileList()
{
    CPLStringList aosList;
    for(size_t i=0;i<aosNonJP2Files.size();i++)
        aosList.AddString(aosNonJP2Files[i]);
    char** papszFileList = VRTDataset::GetFileList();
    for(char** papszIter = papszFileList; papszIter && *papszIter; ++papszIter)
        aosList.AddString(*papszIter);
    CSLDestroy(papszFileList);
    return aosList.StealList();
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int SENTINEL2Dataset::Identify( GDALOpenInfo *poOpenInfo ) 
{
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "SENTINEL2_L1C:") )
        return TRUE;

    /* Accept directly .zip as provided by https://scihub.esa.int/ */
    const char* pszJustFilename = CPLGetFilename(poOpenInfo->pszFilename);
    if( (STARTS_WITH_CI(pszJustFilename, "S2A_OPER_PRD_MSI") ||
         STARTS_WITH_CI(pszJustFilename, "S2B_OPER_PRD_MSI") ) &&
         EQUAL(CPLGetExtension(pszJustFilename), "zip") )
    {
        return TRUE;
    }

    if( poOpenInfo->nHeaderBytes < 100 )
        return FALSE;

    const char* pszHeader = reinterpret_cast<const char*>(poOpenInfo->pabyHeader);
    if( strstr(pszHeader,  "<n1:Level-1C_User_Product" ) != NULL &&
        strstr(pszHeader,
               "http://pdgs.s2.esa.int/PSD/User_Product_Level-1C.xsd" ) != NULL )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                         SENTINEL2_CPLXMLNodeHolder                   */
/************************************************************************/

class SENTINEL2_CPLXMLNodeHolder
{
    CPLXMLNode* m_psNode;
    public:
        SENTINEL2_CPLXMLNodeHolder(CPLXMLNode* psNode) : m_psNode(psNode) {}
       ~SENTINEL2_CPLXMLNodeHolder() { CPLDestroyXMLNode(m_psNode); }
};

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *SENTINEL2Dataset::Open( GDALOpenInfo * poOpenInfo )
{
    if ( !Identify( poOpenInfo ) )
    {
        return NULL;
    }

    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "SENTINEL2_L1C:") )
        return OpenL1CSubdataset(poOpenInfo);

    const char* pszJustFilename = CPLGetFilename(poOpenInfo->pszFilename);
    if( (STARTS_WITH_CI(pszJustFilename, "S2A_OPER_PRD_MSI") ||
         STARTS_WITH_CI(pszJustFilename, "S2B_OPER_PRD_MSI") ) &&
         EQUAL(CPLGetExtension(pszJustFilename), "zip") )
    {
        CPLString osBasename(CPLGetBasename(pszJustFilename));
        CPLString osFilename(poOpenInfo->pszFilename);
        CPLString osMTD(osBasename);
        osMTD[9] = 'M';
        osMTD[10] = 'T';
        osMTD[11] = 'D';
        osMTD[13] = 'S';
        osMTD[14] = 'A';
        osMTD[15] = 'F';
        CPLString osSAFE(CPLString(osBasename) + ".SAFE");
        osFilename = CPLFormFilename(
            CPLFormFilename(osFilename, osSAFE, NULL), osMTD, ".xml");
        if( strncmp(osFilename, "/vsizip/", strlen("/vsizip/")) != 0 )
            osFilename = "/vsizip/" + osFilename;
        CPLDebug("SENTINEL2", "Trying %s", osFilename.c_str());
        GDALOpenInfo oOpenInfo(osFilename, GA_ReadOnly);
        return Open(&oOpenInfo);
    }

    const char* pszHeader = reinterpret_cast<const char*>(poOpenInfo->pabyHeader);
    if( strstr(pszHeader,  "<n1:Level-1C_User_Product" ) != NULL &&
        strstr(pszHeader,
               "http://pdgs.s2.esa.int/PSD/User_Product_Level-1C.xsd" ) != NULL )
    {
        return OpenL1C(poOpenInfo);
    }
    return NULL;
}

/************************************************************************/
/*                        SENTINEL2GetBandDesc()                        */
/************************************************************************/

static const SENTINEL2BandDescription* SENTINEL2GetBandDesc(const char* pszBandName)
{
    for(size_t i=0; i < NB_BANDS; i++)
    {
        if( EQUAL(asBandDesc[i].pszBandName, pszBandName) )
            return &(asBandDesc[i]);
    }
    return NULL;
}

/************************************************************************/
/*                      SENTINEL2_L1C_GetGranuleInfo()                  */
/************************************************************************/

static bool SENTINEL2_L1C_GetGranuleInfo(const CPLString& osGranuleMTDPath,
                                         int nDesiredResolution,
                                         int* pnEPSGCode = NULL,
                                         double* pdfULX = NULL,
                                         double* pdfULY = NULL,
                                         int* pnResolution = NULL,
                                         int* pnWidth = NULL,
                                         int* pnHeight = NULL)
{
    static bool bTryOptimization = true;
    CPLXMLNode *psRoot = NULL;

    if( bTryOptimization )
    {
        /* Small optimization: in practice the interesting info are in the */
        /* first bytes of the Granule MTD, which can be very long sometimes */
        /* so only read them, and hack the buffer a bit to form a valid XML */
        char szBuffer[3072];
        VSILFILE* fp = VSIFOpenL( osGranuleMTDPath, "rb" );
        size_t nRead = 0;
        if( fp == NULL ||
            (nRead = VSIFReadL( szBuffer, 1, sizeof(szBuffer)-1, fp )) == 0 )
        {
            if( fp )
                VSIFCloseL(fp);
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot read %s",
                     osGranuleMTDPath.c_str());
            return false;
        }
        szBuffer[nRead] = 0;
        VSIFCloseL(fp);
        char* pszTileGeocoding = strstr(szBuffer, "</Tile_Geocoding>");
        if( pszTileGeocoding != NULL &&
            strstr(szBuffer, "<n1:Level-1C_Tile_ID") != NULL &&
            strstr(szBuffer, "<n1:Geometric_Info") != NULL &&
            static_cast<size_t>(pszTileGeocoding - szBuffer) <
                sizeof(szBuffer) - strlen("</Tile_Geocoding></n1:Geometric_Info></n1:Level-1C_Tile_ID>") - 1 )
        {
            strcpy(pszTileGeocoding,
                "</Tile_Geocoding></n1:Geometric_Info></n1:Level-1C_Tile_ID>");
            psRoot = CPLParseXMLString( szBuffer );
        }
        else
            bTryOptimization = false;
    }

    // If the above doesn't work, then read the whole file...
    if( psRoot == NULL )
        psRoot = CPLParseXMLFile( osGranuleMTDPath );
    if( psRoot == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot XML parse %s",
                 osGranuleMTDPath.c_str());
        return false;
    }
    SENTINEL2_CPLXMLNodeHolder oXMLHolder(psRoot);
    CPLStripXMLNamespace(psRoot, NULL, TRUE);

    CPLXMLNode* psTileGeocoding = CPLGetXMLNode(psRoot,
                            "=Level-1C_Tile_ID.Geometric_Info.Tile_Geocoding");
    if( psTileGeocoding == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s in %s",
                 "=Level-1C_Tile_ID.Geometric_Info.Tile_Geocoding",
                 osGranuleMTDPath.c_str());
        return false;
    }

    const char* pszCSCode = CPLGetXMLValue(psTileGeocoding, "HORIZONTAL_CS_CODE", NULL);
    if( pszCSCode == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s in %s",
                 "HORIZONTAL_CS_CODE",
                 osGranuleMTDPath.c_str());
        return false;
    }
    if( !STARTS_WITH_CI(pszCSCode, "EPSG:") )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid CS code (%s) for %s",
                 pszCSCode,
                 osGranuleMTDPath.c_str());
        return false;
    }
    int nEPSGCode = atoi(pszCSCode + strlen("EPSG:"));
    if( pnEPSGCode != NULL )
        *pnEPSGCode = nEPSGCode;
    
    for(CPLXMLNode* psIter = psTileGeocoding->psChild; psIter != NULL;
                                                       psIter = psIter->psNext)
    {
        if( psIter->eType != CXT_Element )
            continue;
        if( EQUAL(psIter->pszValue, "Size") &&
            (nDesiredResolution == 0 ||
             atoi(CPLGetXMLValue(psIter, "resolution", "")) == nDesiredResolution) )
        {
            nDesiredResolution = atoi(CPLGetXMLValue(psIter, "resolution", ""));
            const char* pszRows = CPLGetXMLValue(psIter, "NROWS", NULL);
            if( pszRows == NULL )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s in %s",
                        "NROWS",
                        osGranuleMTDPath.c_str());
                return false;
            }
            const char* pszCols = CPLGetXMLValue(psIter, "NCOLS", NULL);
            if( pszCols == NULL )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s in %s",
                        "NCOLS",
                        osGranuleMTDPath.c_str());
                return false;
            }
            if( pnResolution )
                *pnResolution = nDesiredResolution;
            if( pnWidth )
                *pnWidth = atoi(pszCols);
            if( pnHeight )
                *pnHeight = atoi(pszRows);
        }
        else if( EQUAL(psIter->pszValue, "Geoposition") &&
                 (nDesiredResolution == 0 ||
                  atoi(CPLGetXMLValue(psIter, "resolution", "")) == nDesiredResolution) )
        {
            nDesiredResolution = atoi(CPLGetXMLValue(psIter, "resolution", ""));
            const char* pszULX = CPLGetXMLValue(psIter, "ULX", NULL);
            if( pszULX == NULL )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s in %s",
                        "ULX",
                        osGranuleMTDPath.c_str());
                return false;
            }
            const char* pszULY = CPLGetXMLValue(psIter, "ULY", NULL);
            if( pszULY == NULL )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s in %s",
                        "ULY",
                        osGranuleMTDPath.c_str());
                return false;
            }
            if( pnResolution )
                *pnResolution = nDesiredResolution;
            if( pdfULX )
                *pdfULX = CPLAtof(pszULX);
            if( pdfULY )
                *pdfULY = CPLAtof(pszULY);
        }
    }

    return true;
}

/************************************************************************/
/*                      SENTINEL2_L1C_GetGranuleList()                  */
/************************************************************************/

static bool SENTINEL2_L1C_GetGranuleList(CPLXMLNode* psRoot,
                                         const char* pszFilename,
                                         std::vector<CPLString>& osList)
{
    CPLXMLNode* psProductInfo = CPLGetXMLNode(psRoot,
                            "=Level-1C_User_Product.General_Info.Product_Info");
    if( psProductInfo == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s",
                 "=Level-1C_User_Product.General_Info.Product_Info");
        return false;
    }

    CPLXMLNode* psProductOrganisation =
                        CPLGetXMLNode(psProductInfo, "Product_Organisation");
    if( psProductOrganisation == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s",
                 "Product_Organisation");
        return false;
    }

    CPLString osDirname( CPLGetDirname(pszFilename) );
#ifdef HAVE_READLINK
    char szPointerFilename[2048];
    int nBytes = static_cast<int>(readlink(pszFilename, szPointerFilename,
                                           sizeof(szPointerFilename)));
    if (nBytes != -1)
    {
        szPointerFilename[MIN(nBytes,
                            static_cast<int>(sizeof(szPointerFilename)-1))] = 0;
        osDirname = CPLGetDirname(szPointerFilename);
    }
#endif

    for(CPLXMLNode* psIter = psProductOrganisation->psChild; psIter != NULL;
                                                    psIter = psIter->psNext )
    {
        if( psIter->eType != CXT_Element ||
            !EQUAL(psIter->pszValue, "Granule_List") )
        {
            continue;
        }
        for(CPLXMLNode* psIter2 = psIter->psChild; psIter2 != NULL;
                                                     psIter2 = psIter2->psNext )
        {
            if( psIter2->eType != CXT_Element ||
                !EQUAL(psIter2->pszValue, "Granules") )
            {
                continue;
            }
            const char* pszGranuleId = CPLGetXMLValue(psIter2, "granuleIdentifier", NULL);
            if( pszGranuleId == NULL )
            {
                CPLDebug("SENTINEL2", "Missing granuleIdentifier attribute");
                continue;
            }

            /* S2A_OPER_MSI_L1C_TL_SGS__20151024T023555_A001758_T53JLJ_N01.04 --> */
            /* S2A_OPER_MTD_L1C_TL_SGS__20151024T023555_A001758_T53JLJ */
            CPLString osGranuleMTD = pszGranuleId;
            if( osGranuleMTD.size() > strlen("S2A_OPER_MSI_") &&
                osGranuleMTD[8] == '_' && osGranuleMTD[12] == '_' &&
                osGranuleMTD[osGranuleMTD.size()-7] == '_' &&
                osGranuleMTD[osGranuleMTD.size()-6] == 'N' )
            {
                osGranuleMTD[9] = 'M';
                osGranuleMTD[10] = 'T';
                osGranuleMTD[11] = 'D';
                osGranuleMTD.resize(osGranuleMTD.size()-7);
            }
            else
            {
                CPLDebug("SENTINEL2", "Invalid granule ID: %s", pszGranuleId);
                continue;
            }
            osGranuleMTD += ".xml";

            CPLString osGranuleMTDPath = osDirname + "/GRANULE/" +
                                CPLString(pszGranuleId) + "/" + osGranuleMTD;
            osList.push_back(osGranuleMTDPath);
        }
    }

    return true;
}

/************************************************************************/
/*                          SENTINEL2GetL1CMetadata()                   */
/************************************************************************/

static
char** SENTINEL2GetL1CMetadata( CPLXMLNode* psMainMTD )
{
    CPLStringList aosList;

    CPLXMLNode* psProductInfo = CPLGetXMLNode(psMainMTD,
                            "=Level-1C_User_Product.General_Info.Product_Info");
    int nDataTakeCounter = 1;
    for( CPLXMLNode* psIter = (psProductInfo ? psProductInfo->psChild : NULL);
                     psIter != NULL;
                     psIter = psIter->psNext )
    {
        if( psIter->eType != CXT_Element )
            continue;
        if( psIter->psChild != NULL && psIter->psChild->eType == CXT_Text )
        {
            aosList.AddNameValue( psIter->pszValue,
                                  psIter->psChild->pszValue );
        }
        else if( EQUAL(psIter->pszValue, "Datatake") )
        {
            CPLString osPrefix(CPLSPrintf("DATATAKE_%d_", nDataTakeCounter));
            nDataTakeCounter ++;
            const char* pszId = CPLGetXMLValue(psIter, "datatakeIdentifier", NULL);
            if( pszId )
                aosList.AddNameValue( (osPrefix + "ID").c_str(), pszId );
            for( CPLXMLNode* psIter2 = psIter->psChild;
                     psIter2 != NULL;
                     psIter2 = psIter2->psNext )
            {
                if( psIter2->eType != CXT_Element )
                    continue;
                if( psIter2->psChild != NULL && psIter2->psChild->eType == CXT_Text )
                {
                    aosList.AddNameValue( (osPrefix + psIter2->pszValue).c_str(),
                                          psIter2->psChild->pszValue );
                }
            }
        }
    }

    CPLXMLNode* psIC = CPLGetXMLNode(psMainMTD,
            "=Level-1C_User_Product.General_Info.Product_Image_Characteristics");
    if( psIC != NULL )
    {
        for( CPLXMLNode* psIter = psIC->psChild; psIter != NULL;
                                                 psIter = psIter->psNext )
        {
            if( psIter->eType != CXT_Element ||
                !EQUAL(psIter->pszValue, "Special_Values") )
            {
                continue;
            }
            const char* pszText = CPLGetXMLValue(psIter, "SPECIAL_VALUE_TEXT", NULL);
            const char* pszIndex = CPLGetXMLValue(psIter, "SPECIAL_VALUE_INDEX", NULL);
            if( pszText && pszIndex )
            {
                aosList.AddNameValue( (CPLString("SPECIAL_VALUE_") + pszText).c_str(),
                                       pszIndex );
            }
        }

        const char* pszQuantValue =
            CPLGetXMLValue(psIC, "QUANTIFICATION_VALUE", NULL);
        if( pszQuantValue != NULL )
            aosList.AddNameValue("QUANTIFICATION_VALUE", pszQuantValue);

        const char* pszRCU =
            CPLGetXMLValue(psIC, "Reflectance_Conversion.U", NULL);
        if( pszRCU != NULL )
            aosList.AddNameValue("REFLECTANCE_CONVERSION_U", pszRCU);

        const char* pszRefBand =
            CPLGetXMLValue(psIC, "REFERENCE_BAND", NULL);
        if( pszRefBand != NULL )
        {
            int nIdx = atoi(pszRefBand);
            if( nIdx >= 0 || nIdx < (int)NB_BANDS )
                aosList.AddNameValue("REFERENCE_BAND", asBandDesc[nIdx].pszBandName );
        }
    }

    CPLXMLNode* psQII = CPLGetXMLNode(psMainMTD,
                            "=Level-1C_User_Product.Quality_Indicators_Info");
    if( psQII != NULL )
    {
        const char* pszCC = CPLGetXMLValue(psQII, "Cloud_Coverage_Assessment", NULL);
        if( pszCC )
            aosList.AddNameValue("CLOUD_COVERAGE_ASSESSMENT",
                                 pszCC);

        const char* pszDegradedAnc = CPLGetXMLValue(psQII,
            "Technical_Quality_Assessment.DEGRADED_ANC_DATA_PERCENTAGE", NULL);
        if( pszDegradedAnc )
            aosList.AddNameValue("DEGRADED_ANC_DATA_PERCENTAGE", pszDegradedAnc);

        const char* pszDegradedMSI = CPLGetXMLValue(psQII,
            "Technical_Quality_Assessment.DEGRADED_MSI_DATA_PERCENTAGE", NULL);
        if( pszDegradedMSI )
            aosList.AddNameValue("DEGRADED_MSI_DATA_PERCENTAGE", pszDegradedMSI);

        CPLXMLNode* psQualInspect = CPLGetXMLNode(psQII,
                            "Quality_Control_Checks.Quality_Inspections");
        for( CPLXMLNode* psIter = (psQualInspect ? psQualInspect->psChild : NULL);
                     psIter != NULL;
                     psIter = psIter->psNext )
        {
            if( psIter->eType != CXT_Element )
                continue;
            if( psIter->psChild != NULL && psIter->psChild->eType == CXT_Text )
            {
                aosList.AddNameValue( psIter->pszValue,
                                    psIter->psChild->pszValue );
            }
        }
    }

    return aosList.StealList();
}


/************************************************************************/
/*                               OpenL1C()                              */
/************************************************************************/

GDALDataset *SENTINEL2Dataset::OpenL1C( GDALOpenInfo * poOpenInfo )
{
    CPLXMLNode *psRoot = CPLParseXMLFile( poOpenInfo->pszFilename );
    if( psRoot == NULL )
        return NULL;

    char* pszOriginalXML = CPLSerializeXMLTree(psRoot);
    CPLString osOriginalXML;
    if( pszOriginalXML )
        osOriginalXML = pszOriginalXML;
    CPLFree(pszOriginalXML);

    SENTINEL2_CPLXMLNodeHolder oXMLHolder(psRoot);
    CPLStripXMLNamespace(psRoot, NULL, TRUE);

    CPLXMLNode* psProductInfo = CPLGetXMLNode(psRoot,
                            "=Level-1C_User_Product.General_Info.Product_Info");
    if( psProductInfo == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s",
                 "=Level-1C_User_Product.General_Info.Product_Info");
        return NULL;
    }

    CPLXMLNode* psBandList = CPLGetXMLNode(psProductInfo, "Query_Options.Band_List");
    if( psBandList == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s",
                 "Query_Options.Band_List");
        return NULL;
    }

    std::set<int> oSetResolutions;
    std::map<int, std::set<CPLString> > oMapResolutionsToBands;
    for(CPLXMLNode* psIter = psBandList->psChild; psIter != NULL; psIter = psIter->psNext )
    {
        if( psIter->eType != CXT_Element || !EQUAL(psIter->pszValue, "BAND_NAME") )
            continue;
        const char* pszBandName = CPLGetXMLValue(psIter, NULL, "");
        const SENTINEL2BandDescription* psBandDesc = SENTINEL2GetBandDesc(pszBandName);
        if( psBandDesc == NULL )
        {
            CPLDebug("SENTINEL2", "Unknown band name %s", pszBandName);
            continue;
        }
        oSetResolutions.insert( psBandDesc->nResolution );
        CPLString osName = psBandDesc->pszBandName + 1; /* skip B character */
        if( atoi(osName) < 10 )
            osName = "0" + osName;
        oMapResolutionsToBands[psBandDesc->nResolution].insert(osName);
    }
    if( oSetResolutions.size() == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find any band");
        return NULL;
    }

    std::vector<CPLString> osGranuleList;
    if( !SENTINEL2_L1C_GetGranuleList(psRoot, poOpenInfo->pszFilename,
                                      osGranuleList) )
    {
        return NULL;
    }

    std::set<int> oSetEPSGCodes;
    for(size_t i=0;i<osGranuleList.size();i++)
    {
        int nEPSGCode = 0;
        if( SENTINEL2_L1C_GetGranuleInfo(osGranuleList[i],
                    *(oSetResolutions.begin()), &nEPSGCode) )
        {
            oSetEPSGCodes.insert(nEPSGCode);
        }
    }

    SENTINEL2DatasetContainer* poDS = new SENTINEL2DatasetContainer();
    char** papszMD = SENTINEL2GetL1CMetadata(psRoot);
    poDS->GDALDataset::SetMetadata(papszMD);
    CSLDestroy(papszMD);

    if( osOriginalXML.size() )
    {
        char* apszXMLMD[2];
        apszXMLMD[0] = const_cast<char*>(osOriginalXML.c_str());
        apszXMLMD[1] = NULL;
        poDS->GDALDataset::SetMetadata(apszXMLMD, "xml:SENTINEL2");
    }

    /* Create subdatsets per resolution (10, 20, 60m) and EPSG codes */
    int iSubDSNum = 1;
    for(std::set<int>::const_iterator oIterRes = oSetResolutions.begin();
                                oIterRes != oSetResolutions.end();
                              ++oIterRes )
    {
        const int nResolution = *oIterRes;

        for(std::set<int>::const_iterator oIterEPSG = oSetEPSGCodes.begin();
                                    oIterEPSG != oSetEPSGCodes.end();
                                  ++oIterEPSG )
        {
            const int nEPSGCode = *oIterEPSG;
            poDS->GDALDataset::SetMetadataItem(
                CPLSPrintf("SUBDATASET_%d_NAME", iSubDSNum),
                CPLSPrintf("SENTINEL2_L1C:%s:%dm:EPSG_%d",
                            poOpenInfo->pszFilename, nResolution, nEPSGCode),
                "SUBDATASETS");
            const std::set<CPLString>& oBandnames = oMapResolutionsToBands[nResolution];
            CPLString osBandNames;
            for(std::set<CPLString>::const_iterator oIterBandnames = oBandnames.begin();
                                                    oIterBandnames != oBandnames.end();
                                                ++oIterBandnames)
            {
                if( osBandNames.size() )
                    osBandNames += ", ";
                const char* pszName = *oIterBandnames;
                if( *pszName == '0' )
                    pszName ++;
                osBandNames += "B" + CPLString(pszName);
            }

            CPLString osDesc(CPLSPrintf("Bands %s with %dm resolution",
                                        osBandNames.c_str(), nResolution));
            if( nEPSGCode >= 32601 && nEPSGCode <= 32660 )
                osDesc += CPLSPrintf(", UTM %dN", nEPSGCode - 32600);
            else if( nEPSGCode >= 32701 && nEPSGCode <= 32760 )
                osDesc += CPLSPrintf(", UTM %dS", nEPSGCode - 32700);
            else
                osDesc += CPLSPrintf(", EPSG:%d", nEPSGCode);
            poDS->GDALDataset::SetMetadataItem(
                CPLSPrintf("SUBDATASET_%d_DESC", iSubDSNum),
                osDesc.c_str(),
                "SUBDATASETS");

            iSubDSNum ++;
        }
    }

    /* Expose PREVIEW subdatasets */
    for(std::set<int>::const_iterator oIterEPSG = oSetEPSGCodes.begin();
                                    oIterEPSG != oSetEPSGCodes.end();
                                  ++oIterEPSG )
    {
        const int nEPSGCode = *oIterEPSG;
        poDS->GDALDataset::SetMetadataItem(
            CPLSPrintf("SUBDATASET_%d_NAME", iSubDSNum),
            CPLSPrintf("SENTINEL2_L1C:%s:PREVIEW:EPSG_%d",
                        poOpenInfo->pszFilename, nEPSGCode),
            "SUBDATASETS");

        CPLString osDesc("RGB preview");
        if( nEPSGCode >= 32601 && nEPSGCode <= 32660 )
            osDesc += CPLSPrintf(", UTM %dN", nEPSGCode - 32600);
        else if( nEPSGCode >= 32701 && nEPSGCode <= 32760 )
            osDesc += CPLSPrintf(", UTM %dS", nEPSGCode - 32700);
        else
            osDesc += CPLSPrintf(", EPSG:%d", nEPSGCode);
        poDS->GDALDataset::SetMetadataItem(
            CPLSPrintf("SUBDATASET_%d_DESC", iSubDSNum),
            osDesc.c_str(),
            "SUBDATASETS");

        iSubDSNum ++;
    }

    const char* pszExtent = CPLGetXMLValue(psRoot,
        "=Level-1C_User_Product.Geometric_Info.Product_Footprint."
        "Product_Footprint.Global_Footprint.EXT_POS_LIST", NULL);
    if( pszExtent != NULL )
    {
        char** papszTokens = CSLTokenizeString(pszExtent);
        if( (CSLCount(papszTokens) % 2) == 0 )
        {
            CPLString osPolygon = "POLYGON((";
            for(char** papszIter = papszTokens; *papszIter; papszIter += 2 )
            {
                if( papszIter != papszTokens )
                    osPolygon += ", ";
                osPolygon += papszIter[1];
                osPolygon += " ";
                osPolygon += papszIter[0];
            }
            osPolygon += "))";
            poDS->GDALDataset::SetMetadataItem("FOOTPRINT", osPolygon.c_str());
        }
        CSLDestroy(papszTokens);
    }

    return poDS;
}

/************************************************************************/
/*                     SENTINEL2GetSpecialValueIndices()                */
/************************************************************************/

static void SENTINEL2GetSpecialValueIndices(CPLXMLNode* psRoot,
                                            int& nNodataVal,
                                            int& nSaturatedVal)
{
    CPLXMLNode* psProductImageCharacteristics = CPLGetXMLNode(psRoot,
                "=Level-1C_User_Product.General_Info.Product_Image_Characteristics");
    if( psProductImageCharacteristics != NULL )
    {
        for(CPLXMLNode* psIter = psProductImageCharacteristics->psChild;
                        psIter != NULL; psIter = psIter->psNext )
        {
            if( psIter->eType != CXT_Element ||
                !EQUAL(psIter->pszValue, "Special_Values") )
            {
                continue;
            }
            const char* pszSpecialValueText =
                        CPLGetXMLValue(psIter, "SPECIAL_VALUE_TEXT", "");
            if( EQUAL(pszSpecialValueText, "NODATA") )
            {
                nNodataVal = atoi(CPLGetXMLValue(psIter, "SPECIAL_VALUE_INDEX", "-1"));
                //CPLDebug("SENTINEL2", "NODATA value: %d", nNodataVal);
            }
            else if( EQUAL(pszSpecialValueText, "SATURATED") )
            {
                nSaturatedVal = atoi(CPLGetXMLValue(psIter, "SPECIAL_VALUE_INDEX", "-1"));
                //CPLDebug("SENTINEL2", "SATURATED value: %d", nSaturatedVal);
            }
        }
    }
    else
    {
        CPLDebug("SENTINEL2", "Cannot find %s",
                 "=Level-1C_User_Product.General_Info.Product_Image_Characteristics");
    }
}

/************************************************************************/
/*                         SENTINEL2GetOption()                         */
/************************************************************************/

static
const char* SENTINEL2GetOption( GDALOpenInfo* poOpenInfo,
                                const char* pszName,
                                const char* pszDefaultVal = NULL )
{
#ifdef GDAL_DMD_OPENOPTIONLIST
    const char* pszVal = CSLFetchNameValue(poOpenInfo->papszOpenOptions, pszName);
    if( pszVal != NULL )
        return pszVal;
#else
    (void) poOpenInfo;
#endif
    return CPLGetConfigOption( CPLSPrintf("SENTINEL2_%s", pszName), pszDefaultVal );
}

/************************************************************************/
/*                            LaunderUnit()                             */
/************************************************************************/

static CPLString LaunderUnit(const char* pszUnit)
{
    CPLString osUnit;
    for(int i=0; pszUnit[i] != '\0'; )
    {
        if( strncmp(pszUnit + i, "\xC2" "\xB2", 2) == 0 ) /* square / 2 */
        {
            i += 2;
            osUnit += "2";
        }
        else if( strncmp(pszUnit + i, "\xC2" "\xB5", 2) == 0 ) /* micro */
        {
            i += 2;
            osUnit += "u";
        }
        else
        {
            osUnit += pszUnit[i];
            i ++;
        }
    }
    return osUnit;
}

/************************************************************************/
/*                         OpenL1CSubdataset()                          */
/************************************************************************/

GDALDataset *SENTINEL2Dataset::OpenL1CSubdataset( GDALOpenInfo * poOpenInfo )
{
    CPLString osFilename;
    CPLAssert( STARTS_WITH_CI(poOpenInfo->pszFilename, "SENTINEL2_L1C:") );
    osFilename = poOpenInfo->pszFilename + strlen("SENTINEL2_L1C:");
    const char* pszEPSGCode = strrchr(osFilename.c_str(), ':');
    if( pszEPSGCode == NULL || pszEPSGCode == osFilename.c_str() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid syntax for SENTINEL2_L1C:");
        return NULL;
    }
    osFilename[ (size_t)(pszEPSGCode - osFilename.c_str()) ] = '\0';
    const char* pszPrecision = strrchr(osFilename.c_str(), ':');
    if( pszPrecision == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid syntax for SENTINEL2_L1C:");
        return NULL;
    }

    if( !STARTS_WITH_CI(pszEPSGCode+1, "EPSG_") )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid syntax for SENTINEL2_L1C:");
        return NULL;
    }

    const int nSubDSEPSGCode = atoi(pszEPSGCode + 1 + strlen("EPSG_"));
    const bool bIsPreview = STARTS_WITH_CI(pszPrecision + 1, "PREVIEW");
    const int nSubDSPrecision = (bIsPreview) ? 320 : atoi(pszPrecision + 1);
    if( !bIsPreview &&
        nSubDSPrecision != 10 && nSubDSPrecision != 20 && nSubDSPrecision != 60 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported precision: %d",
                 nSubDSPrecision);
        return NULL;
    }

    osFilename.resize( pszPrecision - osFilename.c_str() );
    std::vector<CPLString> aosNonJP2Files;
    aosNonJP2Files.push_back(osFilename);

    CPLXMLNode *psRoot = CPLParseXMLFile( osFilename );
    if( psRoot == NULL )
        return NULL;

    char* pszOriginalXML = CPLSerializeXMLTree(psRoot);
    CPLString osOriginalXML;
    if( pszOriginalXML )
        osOriginalXML = pszOriginalXML;
    CPLFree(pszOriginalXML);

    SENTINEL2_CPLXMLNodeHolder oXMLHolder(psRoot);
    CPLStripXMLNamespace(psRoot, NULL, TRUE);

    std::vector<CPLString> osGranuleList;
    if( !SENTINEL2_L1C_GetGranuleList(psRoot,osFilename, osGranuleList) )
    {
        return NULL;
    }

    /* Iterate over granule metadata to know the layer extent */
    /* and the location of each granule */
    double dfMinX = 1e20, dfMinY = 1e20, dfMaxX = -1e20, dfMaxY = -1e20;
    std::vector<SENTINEL2GranuleInfo> aosGranuleInfoList;
    const int nDesiredResolution = (bIsPreview) ? 0 : nSubDSPrecision;
    for(size_t i=0;i<osGranuleList.size();i++)
    {
        int nEPSGCode = 0;
        double dfULX = 0.0, dfULY = 0.0;
        int nResolution = 0;
        int nWidth = 0, nHeight = 0;
        if( SENTINEL2_L1C_GetGranuleInfo(osGranuleList[i],
                        nDesiredResolution,
                        &nEPSGCode,
                        &dfULX, &dfULY,
                        &nResolution,
                        &nWidth, &nHeight) &&
            nSubDSEPSGCode == nEPSGCode )
        {
            aosNonJP2Files.push_back(osGranuleList[i]);

            if( dfULX < dfMinX ) dfMinX = dfULX;
            if( dfULY > dfMaxY ) dfMaxY = dfULY;
            const double dfLRX = dfULX + nResolution * nWidth;
            const double dfLRY = dfULY - nResolution * nHeight;
            if( dfLRX > dfMaxX ) dfMaxX = dfLRX;
            if( dfLRY < dfMinY ) dfMinY = dfLRY;

            SENTINEL2GranuleInfo oGranuleInfo;
            oGranuleInfo.osPath = CPLGetPath(osGranuleList[i]);
            oGranuleInfo.dfMinX = dfULX;
            oGranuleInfo.dfMinY = dfLRY;
            oGranuleInfo.dfMaxX = dfLRX;
            oGranuleInfo.dfMaxY = dfULY;
            oGranuleInfo.nWidth = nWidth / (nSubDSPrecision / nResolution);
            oGranuleInfo.nHeight = nHeight / (nSubDSPrecision / nResolution);
            aosGranuleInfoList.push_back(oGranuleInfo);
        }
    }
    if( dfMinX > dfMaxX )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "No granule found for EPSG code %d",
                 nSubDSEPSGCode);
        return NULL;
    }

    int nSaturatedVal = -1;
    int nNodataVal = -1;
    SENTINEL2GetSpecialValueIndices(psRoot, nNodataVal, nSaturatedVal);

    std::vector<CPLString> aosBands;
    if( bIsPreview )
    {
        aosBands.push_back("04");
        aosBands.push_back("03");
        aosBands.push_back("02");
    }
    else
    {
        CPLXMLNode* psBandList = CPLGetXMLNode(psRoot,
                "=Level-1C_User_Product.General_Info.Product_Info.Query_Options.Band_List");
        if( psBandList == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s",
                    "Query_Options.Band_List");
            return NULL;
        }

        std::set<CPLString> oSetBands;
        for(CPLXMLNode* psIter = psBandList->psChild; psIter != NULL;
                                                      psIter = psIter->psNext )
        {
            if( psIter->eType != CXT_Element ||
                !EQUAL(psIter->pszValue, "BAND_NAME") )
                continue;
            const char* pszBandName = CPLGetXMLValue(psIter, NULL, "");
            const SENTINEL2BandDescription* psBandDesc =
                                SENTINEL2GetBandDesc(pszBandName);
            if( psBandDesc == NULL )
            {
                CPLDebug("SENTINEL2", "Unknown band name %s", pszBandName);
                continue;
            }
            if( psBandDesc->nResolution != nSubDSPrecision )
                continue;
            CPLString osName = psBandDesc->pszBandName + 1; /* skip B character */
            if( atoi(osName) < 10 )
                osName = "0" + osName;
            oSetBands.insert(osName);
        }
        if( oSetBands.size() == 0 )
            return NULL;
        for(std::set<CPLString>::const_iterator oIterBandnames = oSetBands.begin();
                                                oIterBandnames != oSetBands.end();
                                            ++oIterBandnames)
        {
            aosBands.push_back(*oIterBandnames);
        }
        /* Put 2=Blue, 3=Green, 4=Band bands in RGB order for conveniency */
        if( aosBands.size() >= 3 &&
            aosBands[0] == "02" &&
            aosBands[1] == "03" &&
            aosBands[2] == "04" )
        {
            aosBands[0] = "04";
            aosBands[2] = "02";
        }
    }

/* -------------------------------------------------------------------- */
/*      Create dataset.                                                 */
/* -------------------------------------------------------------------- */
    const int nRasterXSize = static_cast<int>
                                    ((dfMaxX - dfMinX) / nSubDSPrecision + 0.5);
    const int nRasterYSize = static_cast<int>
                                    ((dfMaxY - dfMinY) / nSubDSPrecision + 0.5);
    SENTINEL2Dataset* poDS = new SENTINEL2Dataset(nRasterXSize, nRasterYSize);
    
    OGRSpatialReference oSRS;
    char* pszProjection = NULL;
    if( oSRS.importFromEPSG(nSubDSEPSGCode) == OGRERR_NONE &&
        oSRS.exportToWkt(&pszProjection) == OGRERR_NONE )
    {
        poDS->SetProjection(pszProjection);
        CPLFree(pszProjection);
    }
    else
    {
        CPLDebug("SENTINEL2", "Invalid EPSG code %d", nSubDSEPSGCode);
    }

    double adfGeoTransform[6];
    adfGeoTransform[0] = dfMinX;
    adfGeoTransform[1] = nSubDSPrecision;
    adfGeoTransform[2] = 0;
    adfGeoTransform[3] = dfMaxY;
    adfGeoTransform[4] = 0;
    adfGeoTransform[5] = -nSubDSPrecision;
    poDS->SetGeoTransform(adfGeoTransform);
    poDS->GDALDataset::SetMetadataItem("COMPRESSION", "JPEG2000", "IMAGE_STRUCTURE");
    if( bIsPreview )
        poDS->GDALDataset::SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");

    char** papszMD = SENTINEL2GetL1CMetadata(psRoot);
    poDS->GDALDataset::SetMetadata(papszMD);
    CSLDestroy(papszMD);

    if( osOriginalXML.size() )
    {
        char* apszXMLMD[2];
        apszXMLMD[0] = const_cast<char*>(osOriginalXML.c_str());
        apszXMLMD[1] = NULL;
        poDS->GDALDataset::SetMetadata(apszXMLMD, "xml:SENTINEL2");
    }

    poDS->aosNonJP2Files = aosNonJP2Files;

    const int nBits = (bIsPreview) ? 8 : atoi(CPLGetConfigOption("NBITS", "12"));
    const int nValMax = (1 << nBits) - 1;
    const bool bAlpha =
        CSLTestBoolean(SENTINEL2GetOption(poOpenInfo, "ALPHA", "FALSE")) != FALSE;
    const int nBands = (bIsPreview) ? 3 : ((bAlpha) ? 1 : 0) + static_cast<int>(aosBands.size());
    const int nAlphaBand = (bIsPreview || !bAlpha) ? 0 : nBands;
    const GDALDataType eDT = (bIsPreview) ? GDT_Byte: GDT_UInt16;

    std::map<CPLString, GDALProxyPoolDataset*> oMapPVITile;

    for(int nBand=1;nBand<=nBands;nBand++)
    {
        VRTSourcedRasterBand* poBand;

        if( nBand != nAlphaBand || aosGranuleInfoList.size() == 1 )
        {
            poBand = new VRTSourcedRasterBand( poDS, nBand, eDT,
                                               poDS->nRasterXSize,
                                               poDS->nRasterYSize );
        }
        else
        {
            poBand = new SENTINEL2AlphaBand ( poDS, nBand, eDT,
                                              poDS->nRasterXSize,
                                              poDS->nRasterYSize,
                                              nSaturatedVal,
                                              nNodataVal );
        }

        poDS->SetBand(nBand, poBand);

        if( !bIsPreview )
        {
            if( nBand == nAlphaBand )
                poBand->SetColorInterpretation(GCI_AlphaBand);
            poBand->SetMetadataItem("NBITS",
                                CPLSPrintf("%d", nBits), "IMAGE_STRUCTURE");
        }

        CPLString osBandName;
        if( nBand != nAlphaBand )
        {
            osBandName = aosBands[nBand-1];

            CPLString osLookupBandName(osBandName);
            if( osLookupBandName[0] == '0' )
                osLookupBandName = osLookupBandName.substr(1);
            osLookupBandName = "B" + osLookupBandName;

            CPLString osBandDesc(osLookupBandName);
            const SENTINEL2BandDescription* psBandDesc =
                                    SENTINEL2GetBandDesc(osLookupBandName);
            if( psBandDesc != NULL )
            {
                osBandDesc += CPLSPrintf(", central wavelength %d nm",
                                         psBandDesc->nWaveLength);
                poBand->SetColorInterpretation(psBandDesc->eColorInterp);
                poBand->SetMetadataItem("BANDNAME", psBandDesc->pszBandName);
                poBand->SetMetadataItem("BANDWIDTH", CPLSPrintf("%d",
                                                        psBandDesc->nBandWidth));
                poBand->SetMetadataItem("BANDWIDTH_UNIT", "nm");
                poBand->SetMetadataItem("WAVELENGTH", CPLSPrintf("%d",
                                                        psBandDesc->nWaveLength));
                poBand->SetMetadataItem("WAVELENGTH_UNIT", "nm");
            }
            poBand->SetDescription(osBandDesc);

        }
        else
            osBandName = aosBands[0];

        for(size_t iSrc=0;iSrc<aosGranuleInfoList.size();iSrc++)
        {
            const SENTINEL2GranuleInfo& oGranuleInfo = aosGranuleInfoList[iSrc];
            CPLString osJPEG2000Name(CPLGetFilename(oGranuleInfo.osPath));
            if( osJPEG2000Name.size() > 7 &&
                osJPEG2000Name[osJPEG2000Name.size()-7] == '_' &&
                osJPEG2000Name[osJPEG2000Name.size()-6] == 'N' )
            {
                osJPEG2000Name.resize(osJPEG2000Name.size()-7);
            }
            else
            {
                CPLDebug("SENTINEL2", "Invalid granule path: %s",
                         oGranuleInfo.osPath.c_str());
            }

            CPLString osTile(oGranuleInfo.osPath);
            if( bIsPreview )
            {
                osTile += "/QI_DATA/";
                if( osJPEG2000Name.size() > 12 &&
                    osJPEG2000Name[8] == '_' && osJPEG2000Name[12] == '_' )
                {
                    osJPEG2000Name[9] = 'P';
                    osJPEG2000Name[10] = 'V';
                    osJPEG2000Name[11] = 'I';
                }
                else
                {
                    CPLDebug("SENTINEL2", "Invalid granule path: %s",
                             oGranuleInfo.osPath.c_str());
                }
                osTile += osJPEG2000Name;
            }
            else
            {
                osTile += "/IMG_DATA/";
                osTile += osJPEG2000Name;
                osTile += "_B";
                if( osBandName.size() == 3 && osBandName[0] == '0' )
                    osBandName = osBandName.substr(1);
                osTile += osBandName;
            }
            osTile += ".jp2";

            VSIStatBufL sStat;
            if( VSIStatExL(osTile, &sStat, VSI_STAT_EXISTS_FLAG) != 0 )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Tile %s not found on filesystem. Skipping it",
                         CPLGetFilename(osTile));
                continue;
            }

            GDALProxyPoolDataset* proxyDS;
            if( bIsPreview )
            {
                proxyDS = oMapPVITile[osTile];
                if( proxyDS == NULL )
                {
                    proxyDS = new GDALProxyPoolDataset(     osTile,
                                                            oGranuleInfo.nWidth,
                                                            oGranuleInfo.nHeight,
                                                            GA_ReadOnly,
                                                            TRUE);
                    for(int j=0;j<nBands;j++)
                        proxyDS->AddSrcBandDescription(eDT, 128, 128);
                    oMapPVITile[osTile] = proxyDS;
                }
                else
                    proxyDS->Reference();
            }
            else
            {
                proxyDS = new GDALProxyPoolDataset(         osTile,
                                                            oGranuleInfo.nWidth,
                                                            oGranuleInfo.nHeight,
                                                            GA_ReadOnly,
                                                            TRUE);
                proxyDS->AddSrcBandDescription(eDT, 128, 128);
            }

            const int nDstXOff = static_cast<int>(
                    (oGranuleInfo.dfMinX - dfMinX) / nSubDSPrecision + 0.5);
            const int nDstYOff = static_cast<int>(
                    (dfMaxY - oGranuleInfo.dfMaxY) / nSubDSPrecision + 0.5);

            if( nBand != nAlphaBand )
            {
                poBand->AddSimpleSource( proxyDS->GetRasterBand((bIsPreview) ? nBand : 1),
                                        0, 0,
                                        oGranuleInfo.nWidth,
                                        oGranuleInfo.nHeight,
                                        nDstXOff,
                                        nDstYOff,
                                        oGranuleInfo.nWidth,
                                        oGranuleInfo.nHeight);
            }
            else
            {
                poBand->AddComplexSource( proxyDS->GetRasterBand(1),
                                          0, 0,
                                          oGranuleInfo.nWidth,
                                          oGranuleInfo.nHeight,
                                          nDstXOff,
                                          nDstYOff,
                                          oGranuleInfo.nWidth,
                                          oGranuleInfo.nHeight,
                                          nValMax /* offset */,
                                          0 /* scale */);
            }

            proxyDS->Dereference();
        }
    }

/* -------------------------------------------------------------------- */
/*      Add extra band metadata.                                        */
/* -------------------------------------------------------------------- */
    CPLXMLNode* psIC = CPLGetXMLNode(psRoot,
            "=Level-1C_User_Product.General_Info.Product_Image_Characteristics");
    if( psIC != NULL )
    {
        CPLXMLNode* psSIL = CPLGetXMLNode(psIC,
                            "Reflectance_Conversion.Solar_Irradiance_List");
        if( psSIL != NULL )
        {
            for(CPLXMLNode* psIter = psSIL->psChild; psIter != NULL;
                                                     psIter = psIter->psNext )
            {
                if( psIter->eType != CXT_Element ||
                    !EQUAL(psIter->pszValue, "SOLAR_IRRADIANCE") )
                {
                    continue;
                }
                const char* pszBandId = CPLGetXMLValue(psIter, "bandId", NULL);
                const char* pszUnit = CPLGetXMLValue(psIter, "unit", NULL);
                const char* pszValue = CPLGetXMLValue(psIter, NULL, NULL);
                if( pszBandId != NULL && pszUnit != NULL && pszValue != NULL )
                {
                    int nIdx = atoi(pszBandId);
                    if( nIdx >= 0 || nIdx < (int)NB_BANDS )
                    {
                        for(int i=0;i<poDS->nBands;i++)
                        {
                            GDALRasterBand* poBand = poDS->GetRasterBand(i+1);
                            const char* pszBandName =
                                poBand->GetMetadataItem("BANDNAME");
                            if( pszBandName != NULL &&
                                EQUAL(asBandDesc[nIdx].pszBandName, pszBandName) )
                            {
                                poBand->GDALRasterBand::SetMetadataItem(
                                    "SOLAR_IRRADIANCE", pszValue);
                                poBand->GDALRasterBand::SetMetadataItem(
                                    "SOLAR_IRRADIANCE_UNIT",
                                     LaunderUnit(pszUnit));
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Initialize overview information.                                */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    CPLString osOverviewFile;
    if( bIsPreview )
        osOverviewFile = CPLSPrintf("%s_PREVIEW_EPSG_%d.tif.ovr",
                                    osFilename.c_str(), nSubDSEPSGCode);
    else
        osOverviewFile = CPLSPrintf("%s_%dm_EPSG_%d.tif.ovr",
                                    osFilename.c_str(), nSubDSPrecision,
                                    nSubDSEPSGCode);
    poDS->SetMetadataItem( "OVERVIEW_FILE", osOverviewFile, "OVERVIEWS" );
    poDS->oOvManager.Initialize( poDS, ":::VIRTUAL:::" );

    return poDS;
}

/************************************************************************/
/*                      GDALRegister_SENTINEL2()                        */
/************************************************************************/

void GDALRegister_SENTINEL2()
{
    if( GDALGetDriverByName( "SENTINEL2" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "SENTINEL2" );
#ifdef GDAL_DCAP_RASTER
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
#endif
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Sentinel 2" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_sentinel2.html" );
    poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );

#ifdef GDAL_DMD_OPENOPTIONLIST
    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='ALPHA' type='boolean' description='Whether to expose an alpha band' default='NO'/>"
"</OpenOptionList>" );
#endif

    poDriver->pfnOpen = SENTINEL2Dataset::Open;
    poDriver->pfnIdentify = SENTINEL2Dataset::Identify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
