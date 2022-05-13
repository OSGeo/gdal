/******************************************************************************
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
#include "ogr_spatialref.h"
#include "ogr_geometry.h"
#include "gdaljp2metadata.h"
#include "../vrt/vrtdataset.h"

#include <algorithm>
#include <map>
#include <set>
#include <vector>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef STARTS_WITH_CI
#define STARTS_WITH_CI(a,b) EQUALN(a,b,strlen(b))
#endif

#define DIGIT_ZERO '0'

CPL_CVSID("$Id$")

CPL_C_START
// TODO: Leave this declaration while Sentinel2 folks use this as a
// plugin with GDAL 1.x.
void GDALRegister_SENTINEL2();
CPL_C_END

typedef enum
{
    SENTINEL2_L1B,
    SENTINEL2_L1C,
    SENTINEL2_L2A
} SENTINEL2Level;

typedef enum
{
    MSI2Ap,
    MSI2A
} SENTINEL2ProductType;

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

typedef enum
{
    TL_IMG_DATA,                /* Tile is located in IMG_DATA/ */
    TL_IMG_DATA_Rxxm,           /* Tile is located in IMG_DATA/Rxxm/ */
    TL_QI_DATA                  /* Tile is located in QI_DATA/ */
} SENTINEL2_L2A_Tilelocation;

typedef struct
{
    const char* pszBandName;
    const char* pszBandDescription;
    int         nResolution;    /* meters */
    SENTINEL2_L2A_Tilelocation eLocation;
} SENTINEL2_L2A_BandDescription;

class L1CSafeCompatGranuleDescription
{
public:
    CPLString osMTDTLPath; // GRANULE/L1C_T30TXT_A007999_20170102T111441/MTD_TL.xml
    CPLString osBandPrefixPath; // GRANULE/L1C_T30TXT_A007999_20170102T111441/IMG_DATA/T30TXT_20170102T111442_
};

static const char* L2A_BandDescription_AOT = "Aerosol Optical Thickness map (at 550nm)";
static const char* L2A_BandDescription_WVP = "Scene-average Water Vapour map";
static const char* L2A_BandDescription_SCL = "Scene Classification";
static const char* L2A_BandDescription_CLD = "Raster mask values range from 0 for high confidence clear sky to 100 for high confidence cloudy";
static const char* L2A_BandDescription_SNW = "Raster mask values range from 0 for high confidence NO snow/ice to 100 for high confidence snow/ice";

static const SENTINEL2_L2A_BandDescription asL2ABandDesc[] =
{
    { "AOT", L2A_BandDescription_AOT,20, TL_IMG_DATA_Rxxm },
    { "AOT", L2A_BandDescription_AOT,60, TL_IMG_DATA_Rxxm },
    { "WVP", L2A_BandDescription_WVP,20, TL_IMG_DATA_Rxxm },
    { "WVP", L2A_BandDescription_WVP,60, TL_IMG_DATA_Rxxm },
    { "SCL", L2A_BandDescription_SCL,20, TL_IMG_DATA_Rxxm },
    { "SCL", L2A_BandDescription_SCL,60, TL_IMG_DATA_Rxxm },
    { "CLD", L2A_BandDescription_CLD,20, TL_QI_DATA },
    { "CLD", L2A_BandDescription_CLD,60, TL_QI_DATA },
    { "SNW", L2A_BandDescription_SNW,20, TL_QI_DATA },
    { "SNW", L2A_BandDescription_SNW,60, TL_QI_DATA },
};

#define NB_L2A_BANDS (sizeof(asL2ABandDesc)/sizeof(asL2ABandDesc[0]))

static bool SENTINEL2isZipped(const char* pszHeader, int nHeaderBytes);
static
const char* SENTINEL2GetOption( GDALOpenInfo* poOpenInfo,
                                const char* pszName,
                                const char* pszDefaultVal = nullptr );
static bool SENTINEL2GetTileInfo(const char* pszFilename,
                                 int* pnWidth, int* pnHeight, int *pnBits);

/************************************************************************/
/*                           SENTINEL2GranuleInfo                       */
/************************************************************************/

class SENTINEL2GranuleInfo
{
    public:
        CPLString osPath;
        CPLString osBandPrefixPath; // for Sentinel 2C SafeCompact
        double    dfMinX, dfMinY, dfMaxX, dfMaxY;
        int       nWidth, nHeight;
};

/************************************************************************/
/* ==================================================================== */
/*                         SENTINEL2Dataset                             */
/* ==================================================================== */
/************************************************************************/

class SENTINEL2DatasetContainer final: public GDALPamDataset
{
    public:
        SENTINEL2DatasetContainer() {}
};

class SENTINEL2Dataset final: public VRTDataset
{
        std::vector<CPLString>   aosNonJP2Files;

        void   AddL1CL2ABandMetadata(SENTINEL2Level eLevel,
                                     CPLXMLNode* psRoot,
                                     const std::vector<CPLString>& aosBands);

        static SENTINEL2Dataset *CreateL1CL2ADataset(
                SENTINEL2Level eLevel,
                SENTINEL2ProductType pType,
                bool bIsSafeCompact,
                const std::vector<CPLString>& aosGranuleList,
                const std::vector<L1CSafeCompatGranuleDescription>& aoL1CSafeCompactGranuleList,
                std::vector<CPLString>& aosNonJP2Files,
                int nSubDSPrecision,
                bool bIsPreview,
                bool bIsTCI,
                int nSubDSEPSGCode,
                bool bAlpha,
                const std::vector<CPLString>& aosBands,
                int nSaturatedVal,
                int nNodataVal,
                const CPLString& osProductURI);
    public:
                    SENTINEL2Dataset(int nXSize, int nYSize);
        virtual ~SENTINEL2Dataset();

        virtual char** GetFileList() override;

        static GDALDataset *Open( GDALOpenInfo * );
        static GDALDataset *OpenL1BUserProduct( GDALOpenInfo * );
        static GDALDataset *OpenL1BGranule( const char* pszFilename,
                                            CPLXMLNode** ppsRoot = nullptr,
                                            int nResolutionOfInterest = 0,
                                            std::set<CPLString> *poBandSet = nullptr);
        static GDALDataset *OpenL1BSubdataset( GDALOpenInfo * );
        static GDALDataset *OpenL1C_L2A( const char* pszFilename,
                                         SENTINEL2Level eLevel );
        static GDALDataset *OpenL1CTile( const char* pszFilename,
                                         CPLXMLNode** ppsRootMainMTD = nullptr,
                                         int nResolutionOfInterest = 0,
                                         std::set<CPLString>* poBandSet = nullptr);
        static GDALDataset *OpenL1CTileSubdataset( GDALOpenInfo * );
        static GDALDataset *OpenL1C_L2ASubdataset( GDALOpenInfo *,
                                                   SENTINEL2Level eLevel );

        static int Identify( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                         SENTINEL2AlphaBand                           */
/* ==================================================================== */
/************************************************************************/

class SENTINEL2AlphaBand final: public VRTSourcedRasterBand
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
                              ) override;
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
{}

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

SENTINEL2Dataset::SENTINEL2Dataset( int nXSize, int nYSize ) :
    VRTDataset(nXSize, nYSize)
{
    poDriver = nullptr;
    SetWritable(FALSE);
}

/************************************************************************/
/*                         ~SENTINEL2Dataset()                          */
/************************************************************************/

SENTINEL2Dataset::~SENTINEL2Dataset() {}

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
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "SENTINEL2_L1B:") )
        return TRUE;
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "SENTINEL2_L1C:") )
        return TRUE;
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "SENTINEL2_L1C_TILE:") )
        return TRUE;
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "SENTINEL2_L2A:") )
        return TRUE;

    const char* pszJustFilename = CPLGetFilename(poOpenInfo->pszFilename);

    // We don't handle direct tile access for L1C SafeCompact products
    // We could, but this isn't just done yet.
    if( EQUAL( pszJustFilename, "MTD_TL.xml") )
        return FALSE;

    /* Accept directly .zip as provided by https://scihub.esa.int/
     * First we check just by file name as it is faster than looking
     * inside to detect content. */
    if( (STARTS_WITH_CI(pszJustFilename, "S2A_MSIL1C_") ||
         STARTS_WITH_CI(pszJustFilename, "S2B_MSIL1C_") ||
         STARTS_WITH_CI(pszJustFilename, "S2A_MSIL2A_") ||
         STARTS_WITH_CI(pszJustFilename, "S2B_MSIL2A_") ||
         STARTS_WITH_CI(pszJustFilename, "S2A_OPER_PRD_MSI") ||
         STARTS_WITH_CI(pszJustFilename, "S2B_OPER_PRD_MSI") ||
         STARTS_WITH_CI(pszJustFilename, "S2A_USER_PRD_MSI") ||
         STARTS_WITH_CI(pszJustFilename, "S2B_USER_PRD_MSI") ) &&
         EQUAL(CPLGetExtension(pszJustFilename), "zip") )
    {
        return TRUE;
    }

    if( poOpenInfo->nHeaderBytes < 100 )
        return FALSE;

    const char* pszHeader = reinterpret_cast<const char*>(poOpenInfo->pabyHeader);

    if( strstr(pszHeader,  "<n1:Level-1B_User_Product" ) != nullptr &&
        strstr(pszHeader, "User_Product_Level-1B.xsd" ) != nullptr )
        return TRUE;

    if( strstr(pszHeader,  "<n1:Level-1B_Granule_ID" ) != nullptr &&
        strstr(pszHeader, "S2_PDI_Level-1B_Granule_Metadata.xsd" ) != nullptr )
        return TRUE;

    if( strstr(pszHeader,  "<n1:Level-1C_User_Product" ) != nullptr &&
        strstr(pszHeader, "User_Product_Level-1C.xsd" ) != nullptr )
        return TRUE;

    if( strstr(pszHeader,  "<n1:Level-1C_Tile_ID" ) != nullptr &&
        strstr(pszHeader, "S2_PDI_Level-1C_Tile_Metadata.xsd" ) != nullptr )
        return TRUE;

    if( strstr(pszHeader,  "<n1:Level-2A_User_Product" ) != nullptr &&
        strstr(pszHeader, "User_Product_Level-2A" ) != nullptr )
        return TRUE;

    if( SENTINEL2isZipped(pszHeader, poOpenInfo->nHeaderBytes) )
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
        explicit SENTINEL2_CPLXMLNodeHolder(CPLXMLNode* psNode) : m_psNode(psNode) {}
       ~SENTINEL2_CPLXMLNodeHolder() { if(m_psNode) CPLDestroyXMLNode(m_psNode); }

       CPLXMLNode* Release() {
           CPLXMLNode* psRet = m_psNode;
           m_psNode = nullptr;
           return psRet;
       }
};

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *SENTINEL2Dataset::Open( GDALOpenInfo * poOpenInfo )
{
    if ( !Identify( poOpenInfo ) )
    {
        return nullptr;
    }

    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "SENTINEL2_L1B:") )
    {
        CPLDebug("SENTINEL2", "Trying OpenL1BSubdataset");
        return OpenL1BSubdataset(poOpenInfo);
    }

    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "SENTINEL2_L1C:") )
    {
        CPLDebug("SENTINEL2", "Trying OpenL1C_L2ASubdataset");
        return OpenL1C_L2ASubdataset(poOpenInfo, SENTINEL2_L1C);
    }

    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "SENTINEL2_L1C_TILE:") )
    {
        CPLDebug("SENTINEL2", "Trying OpenL1CTileSubdataset");
        return OpenL1CTileSubdataset(poOpenInfo);
    }

    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "SENTINEL2_L2A:") )
    {
        CPLDebug("SENTINEL2", "Trying OpenL1C_L2ASubdataset");
        return OpenL1C_L2ASubdataset(poOpenInfo, SENTINEL2_L2A);
    }

    const char* pszJustFilename = CPLGetFilename(poOpenInfo->pszFilename);
    if( (STARTS_WITH_CI(pszJustFilename, "S2A_OPER_PRD_MSI") ||
         STARTS_WITH_CI(pszJustFilename, "S2B_OPER_PRD_MSI") ||
         STARTS_WITH_CI(pszJustFilename, "S2A_USER_PRD_MSI") ||
         STARTS_WITH_CI(pszJustFilename, "S2B_USER_PRD_MSI") ) &&
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
        osFilename = osFilename + "/" + osSAFE +"/" + osMTD + ".xml";
        if( strncmp(osFilename, "/vsizip/", strlen("/vsizip/")) != 0 )
            osFilename = "/vsizip/" + osFilename;
        CPLDebug("SENTINEL2", "Trying %s", osFilename.c_str());
        GDALOpenInfo oOpenInfo(osFilename, GA_ReadOnly);
        return Open(&oOpenInfo);
    }
    else if( (STARTS_WITH_CI(pszJustFilename, "S2A_MSIL1C_") ||
              STARTS_WITH_CI(pszJustFilename, "S2B_MSIL1C_") ) &&
         EQUAL(CPLGetExtension(pszJustFilename), "zip") )
    {
        CPLString osBasename(CPLGetBasename(pszJustFilename));
        CPLString osFilename(poOpenInfo->pszFilename);
        CPLString osSAFE(osBasename);
        // S2B_MSIL1C_20171004T233419_N0206_R001_T54DWM_20171005T001811.SAFE.zip
        // has .SAFE.zip extension, but other products have just a .zip
        // extension. So for the subdir in the zip only add .SAFE when needed
        if( !EQUAL(CPLGetExtension(osSAFE), "SAFE") )
            osSAFE += ".SAFE";
        osFilename = osFilename + "/" + osSAFE + "/MTD_MSIL1C.xml";
        if( strncmp(osFilename, "/vsizip/", strlen("/vsizip/")) != 0 )
            osFilename = "/vsizip/" + osFilename;
        CPLDebug("SENTINEL2", "Trying %s", osFilename.c_str());
        GDALOpenInfo oOpenInfo(osFilename, GA_ReadOnly);
        return Open(&oOpenInfo);
    }
    else if( (STARTS_WITH_CI(pszJustFilename, "S2A_MSIL2A_") ||
              STARTS_WITH_CI(pszJustFilename, "S2B_MSIL2A_") ) &&
         EQUAL(CPLGetExtension(pszJustFilename), "zip") )
    {
        CPLString osBasename(CPLGetBasename(pszJustFilename));
        CPLString osFilename(poOpenInfo->pszFilename);
        CPLString osSAFE(osBasename);
        // S2B_MSIL1C_20171004T233419_N0206_R001_T54DWM_20171005T001811.SAFE.zip
        // has .SAFE.zip extension, but other products have just a .zip
        // extension. So for the subdir in the zip only add .SAFE when needed
        if( !EQUAL(CPLGetExtension(osSAFE), "SAFE") )
            osSAFE += ".SAFE";
        osFilename = osFilename + "/" + osSAFE + "/MTD_MSIL2A.xml";
        if( strncmp(osFilename, "/vsizip/", strlen("/vsizip/")) != 0 )
            osFilename = "/vsizip/" + osFilename;
        CPLDebug("SENTINEL2", "Trying %s", osFilename.c_str());
        GDALOpenInfo oOpenInfo(osFilename, GA_ReadOnly);
        return Open(&oOpenInfo);
    }

    const char* pszHeader = reinterpret_cast<const char*>(poOpenInfo->pabyHeader);

    if( strstr(pszHeader,  "<n1:Level-1B_User_Product" ) != nullptr &&
        strstr(pszHeader, "User_Product_Level-1B.xsd" ) != nullptr )
    {
        CPLDebug("SENTINEL2", "Trying OpenL1BUserProduct");
        return OpenL1BUserProduct(poOpenInfo);
    }

    if( strstr(pszHeader,  "<n1:Level-1B_Granule_ID" ) != nullptr &&
        strstr(pszHeader, "S2_PDI_Level-1B_Granule_Metadata.xsd" ) != nullptr )
    {
        CPLDebug("SENTINEL2", "Trying OpenL1BGranule");
        return OpenL1BGranule(poOpenInfo->pszFilename);
    }

    if( strstr(pszHeader,  "<n1:Level-1C_User_Product" ) != nullptr &&
        strstr(pszHeader, "User_Product_Level-1C.xsd" ) != nullptr )
    {
        CPLDebug("SENTINEL2", "Trying OpenL1C_L2A");
        return OpenL1C_L2A(poOpenInfo->pszFilename, SENTINEL2_L1C);
    }

    if( strstr(pszHeader,  "<n1:Level-1C_Tile_ID" ) != nullptr &&
        strstr(pszHeader, "S2_PDI_Level-1C_Tile_Metadata.xsd" ) != nullptr )
    {
        CPLDebug("SENTINEL2", "Trying OpenL1CTile");
        return OpenL1CTile(poOpenInfo->pszFilename);
    }

    if( strstr(pszHeader,  "<n1:Level-2A_User_Product" ) != nullptr &&
        strstr(pszHeader, "User_Product_Level-2A" ) != nullptr )
    {
        CPLDebug("SENTINEL2", "Trying OpenL1C_L2A");
        return OpenL1C_L2A(poOpenInfo->pszFilename, SENTINEL2_L2A);
    }

    if( SENTINEL2isZipped(pszHeader, poOpenInfo->nHeaderBytes) )
    {
        CPLString osFilename(poOpenInfo->pszFilename);
        if( strncmp(osFilename, "/vsizip/", strlen("/vsizip/")) != 0 )
            osFilename = "/vsizip/" + osFilename;

        auto psDir = VSIOpenDir(osFilename.c_str(), 1, nullptr);
        if( psDir == nullptr ) {
            CPLError(CE_Failure, CPLE_AppDefined, "SENTINEL2: Cannot open ZIP file %s",
                     osFilename.c_str());
            return nullptr;
        }
        while ( const VSIDIREntry* psEntry = VSIGetNextDirEntry(psDir) )
        {
            const char* pszInsideFilename = CPLGetFilename(psEntry->pszName);
            if( VSI_ISREG(psEntry->nMode) && (
                STARTS_WITH_CI(pszInsideFilename, "MTD_MSIL2A") ||
                STARTS_WITH_CI(pszInsideFilename, "MTD_MSIL1C") ||
                STARTS_WITH_CI(pszInsideFilename, "S2A_OPER_MTD_SAFL1B") ||
                STARTS_WITH_CI(pszInsideFilename, "S2B_OPER_MTD_SAFL1B") ||
                STARTS_WITH_CI(pszInsideFilename, "S2A_OPER_MTD_SAFL1C") ||
                STARTS_WITH_CI(pszInsideFilename, "S2B_OPER_MTD_SAFL1C") ||
                STARTS_WITH_CI(pszInsideFilename, "S2A_USER_MTD_SAFL2A") ||
                STARTS_WITH_CI(pszInsideFilename, "S2B_USER_MTD_SAFL2A")) )
            {
                osFilename = osFilename + "/" + psEntry->pszName;
                CPLDebug("SENTINEL2", "Trying %s", osFilename.c_str());
                GDALOpenInfo oOpenInfo(osFilename, GA_ReadOnly);
                VSICloseDir(psDir);
                return Open(&oOpenInfo);
            }
        }
        VSICloseDir(psDir);
    }

    return nullptr;
}

/************************************************************************/
/*                        SENTINEL2isZipped()                           */
/************************************************************************/

static bool SENTINEL2isZipped(const char* pszHeader, int nHeaderBytes)
{
    if( nHeaderBytes < 50 )
        return FALSE;

    /* According to Sentinel-2 Products Specification Document,
     * all files are located inside a folder with a specific name pattern
     * Ref: S2-PDGS-TAS-DI-PSD Issue: 14.6.
     */
    return (
        // inside a ZIP file
        memcmp(pszHeader, "\x50\x4b", 2) == 0 && (
        // a "4.2.1 Compact Naming Convention" confirming file
        ( memcmp(pszHeader + 34, "MSIL2A", 6) == 0 ||
          memcmp(pszHeader + 34, "MSIL1C", 6) == 0 ) ||
        // a "4.2 S2 User Product Naming Convention" confirming file
        ( memcmp(pszHeader + 34, "OPER_PRD_MSIL2A", 15) == 0 ||
          memcmp(pszHeader + 34, "OPER_PRD_MSIL1B", 15) == 0 ||
          memcmp(pszHeader + 34, "OPER_PRD_MSIL1C", 15) == 0 ) ||
        // some old / validation naming convention
        ( memcmp(pszHeader + 34, "USER_PRD_MSIL2A", 15) == 0 ||
          memcmp(pszHeader + 34, "USER_PRD_MSIL1B", 15) == 0 ||
          memcmp(pszHeader + 34, "USER_PRD_MSIL1C", 15) == 0 ) )
    );
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
    return nullptr;
}

/************************************************************************/
/*                       SENTINEL2GetL2ABandDesc()                      */
/************************************************************************/

static const SENTINEL2_L2A_BandDescription* SENTINEL2GetL2ABandDesc(const char* pszBandName)
{
    for(size_t i=0; i < NB_L2A_BANDS; i++)
    {
        if( EQUAL(asL2ABandDesc[i].pszBandName, pszBandName) )
            return &(asL2ABandDesc[i]);
    }
    return nullptr;
}

/************************************************************************/
/*                        SENTINEL2GetGranuleInfo()                     */
/************************************************************************/

static bool SENTINEL2GetGranuleInfo(SENTINEL2Level eLevel,
                                    const CPLString& osGranuleMTDPath,
                                    int nDesiredResolution,
                                    int* pnEPSGCode = nullptr,
                                    double* pdfULX = nullptr,
                                    double* pdfULY = nullptr,
                                    int* pnResolution = nullptr,
                                    int* pnWidth = nullptr,
                                    int* pnHeight = nullptr)
{
    static bool bTryOptimization = true;
    CPLXMLNode *psRoot = nullptr;

    if( bTryOptimization )
    {
        /* Small optimization: in practice the interesting info are in the */
        /* first bytes of the Granule MTD, which can be very long sometimes */
        /* so only read them, and hack the buffer a bit to form a valid XML */
        char szBuffer[3072];
        VSILFILE* fp = VSIFOpenL( osGranuleMTDPath, "rb" );
        size_t nRead = 0;
        if( fp == nullptr ||
            (nRead = VSIFReadL( szBuffer, 1, sizeof(szBuffer)-1, fp )) == 0 )
        {
            if( fp )
                VSIFCloseL(fp);
            CPLError(CE_Failure, CPLE_AppDefined, "SENTINEL2GetGranuleInfo: Cannot read %s",
                     osGranuleMTDPath.c_str());
            return false;
        }
        szBuffer[nRead] = 0;
        VSIFCloseL(fp);
        char* pszTileGeocoding = strstr(szBuffer, "</Tile_Geocoding>");
        if( eLevel == SENTINEL2_L1C &&
            pszTileGeocoding != nullptr &&
            strstr(szBuffer, "<n1:Level-1C_Tile_ID") != nullptr &&
            strstr(szBuffer, "<n1:Geometric_Info") != nullptr &&
            static_cast<size_t>(pszTileGeocoding - szBuffer) <
                sizeof(szBuffer) - strlen("</Tile_Geocoding></n1:Geometric_Info></n1:Level-1C_Tile_ID>") - 1 )
        {
            strcpy(pszTileGeocoding,
                "</Tile_Geocoding></n1:Geometric_Info></n1:Level-1C_Tile_ID>");
            psRoot = CPLParseXMLString( szBuffer );
        }
        else if( eLevel == SENTINEL2_L2A &&
            pszTileGeocoding != nullptr &&
            strstr(szBuffer, "<n1:Level-2A_Tile_ID") != nullptr &&
            strstr(szBuffer, "<n1:Geometric_Info") != nullptr &&
            static_cast<size_t>(pszTileGeocoding - szBuffer) <
                sizeof(szBuffer) - strlen("</Tile_Geocoding></n1:Geometric_Info></n1:Level-2A_Tile_ID>") - 1 )
        {
            strcpy(pszTileGeocoding,
                "</Tile_Geocoding></n1:Geometric_Info></n1:Level-2A_Tile_ID>");
            psRoot = CPLParseXMLString( szBuffer );
        }
        else
            bTryOptimization = false;
    }

    // If the above doesn't work, then read the whole file...
    if( psRoot == nullptr )
        psRoot = CPLParseXMLFile( osGranuleMTDPath );
    if( psRoot == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot XML parse %s",
                 osGranuleMTDPath.c_str());
        return false;
    }
    SENTINEL2_CPLXMLNodeHolder oXMLHolder(psRoot);
    CPLStripXMLNamespace(psRoot, nullptr, TRUE);

    const char* pszNodePath =
        (eLevel == SENTINEL2_L1C ) ?
             "=Level-1C_Tile_ID.Geometric_Info.Tile_Geocoding" :
             "=Level-2A_Tile_ID.Geometric_Info.Tile_Geocoding";
    CPLXMLNode* psTileGeocoding = CPLGetXMLNode(psRoot, pszNodePath);
    if( psTileGeocoding == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s in %s",
                 pszNodePath,
                 osGranuleMTDPath.c_str());
        return false;
    }

    const char* pszCSCode = CPLGetXMLValue(psTileGeocoding, "HORIZONTAL_CS_CODE", nullptr);
    if( pszCSCode == nullptr )
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
    if( pnEPSGCode != nullptr )
        *pnEPSGCode = nEPSGCode;

    for(CPLXMLNode* psIter = psTileGeocoding->psChild; psIter != nullptr;
                                                       psIter = psIter->psNext)
    {
        if( psIter->eType != CXT_Element )
            continue;
        if( EQUAL(psIter->pszValue, "Size") &&
            (nDesiredResolution == 0 ||
             atoi(CPLGetXMLValue(psIter, "resolution", "")) == nDesiredResolution) )
        {
            nDesiredResolution = atoi(CPLGetXMLValue(psIter, "resolution", ""));
            const char* pszRows = CPLGetXMLValue(psIter, "NROWS", nullptr);
            if( pszRows == nullptr )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s in %s",
                        "NROWS",
                        osGranuleMTDPath.c_str());
                return false;
            }
            const char* pszCols = CPLGetXMLValue(psIter, "NCOLS", nullptr);
            if( pszCols == nullptr )
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
            const char* pszULX = CPLGetXMLValue(psIter, "ULX", nullptr);
            if( pszULX == nullptr )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s in %s",
                        "ULX",
                        osGranuleMTDPath.c_str());
                return false;
            }
            const char* pszULY = CPLGetXMLValue(psIter, "ULY", nullptr);
            if( pszULY == nullptr )
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
/*                      SENTINEL2GetPathSeparator()                     */
/************************************************************************/

// For the sake of simplifying our unit tests, we limit the use of \\ to when
// it is strictly necessary. Otherwise we could use CPLFormFilename()...
static char SENTINEL2GetPathSeparator(const char* pszBasename)
{
    if( STARTS_WITH_CI(pszBasename, "\\\\?\\") )
        return '\\';
    else
        return '/';
}

/************************************************************************/
/*                      SENTINEL2GetGranuleList()                       */
/************************************************************************/

static bool SENTINEL2GetGranuleList(CPLXMLNode* psMainMTD,
                                    SENTINEL2Level eLevel,
                                    const char* pszFilename,
                                    std::vector<CPLString>& osList,
                                    std::set<int>* poSetResolutions = nullptr,
                                    std::map<int, std::set<CPLString> >*
                                                poMapResolutionsToBands = nullptr)
{
    const char* pszNodePath =
        (eLevel == SENTINEL2_L1B ) ? "Level-1B_User_Product" :
        (eLevel == SENTINEL2_L1C ) ? "Level-1C_User_Product" :
                                     "Level-2A_User_Product";

    CPLXMLNode* psRoot =  CPLGetXMLNode(psMainMTD,
                                        CPLSPrintf("=%s", pszNodePath));
    if( psRoot == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find =%s", pszNodePath);
        return false;
    }
    pszNodePath = "General_Info.Product_Info";
    CPLXMLNode* psProductInfo = CPLGetXMLNode(psRoot, pszNodePath);
    if( psProductInfo == nullptr && eLevel == SENTINEL2_L2A )
    {
        pszNodePath = "General_Info.L2A_Product_Info";
        psProductInfo = CPLGetXMLNode(psRoot, pszNodePath);
    }
    if( psProductInfo == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s", pszNodePath);
        return false;
    }

    pszNodePath = "Product_Organisation";
    CPLXMLNode* psProductOrganisation =
                        CPLGetXMLNode(psProductInfo, pszNodePath);
    if( psProductOrganisation == nullptr && eLevel == SENTINEL2_L2A )
    {
        pszNodePath = "L2A_Product_Organisation";
        psProductOrganisation =
                        CPLGetXMLNode(psProductInfo, pszNodePath);
    }
    if( psProductOrganisation == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s", pszNodePath);
        return false;
    }

    CPLString osDirname( CPLGetDirname(pszFilename) );
#ifdef HAVE_READLINK
    char szPointerFilename[2048];
    int nBytes = static_cast<int>(readlink(pszFilename, szPointerFilename,
                                           sizeof(szPointerFilename)));
    if (nBytes != -1)
    {
        const int nOffset =
            std::min(nBytes, static_cast<int>(sizeof(szPointerFilename)-1));
        szPointerFilename[nOffset] = '\0';
        osDirname = CPLGetDirname(szPointerFilename);
    }
#endif

    const bool bIsMSI2Ap = EQUAL(CPLGetXMLValue(psProductInfo, "PRODUCT_TYPE", ""),
                                "S2MSI2Ap");
    const bool bIsCompact = EQUAL(CPLGetXMLValue(psProductInfo, "PRODUCT_FORMAT", ""),
                                "SAFE_COMPACT");
    CPLString oGranuleId("L2A_");
    std::set<CPLString> aoSetGranuleId;
    for(CPLXMLNode* psIter = psProductOrganisation->psChild; psIter != nullptr;
                                                    psIter = psIter->psNext )
    {
        if( psIter->eType != CXT_Element ||
            !EQUAL(psIter->pszValue, "Granule_List") )
        {
            continue;
        }
        for(CPLXMLNode* psIter2 = psIter->psChild; psIter2 != nullptr;
                                                     psIter2 = psIter2->psNext )
        {
            if( psIter2->eType != CXT_Element ||
                (!EQUAL(psIter2->pszValue, "Granule") &&
                 !EQUAL(psIter2->pszValue, "Granules")) )
            {
                continue;
            }
            const char* pszGranuleId = CPLGetXMLValue(psIter2, "granuleIdentifier", nullptr);
            if( pszGranuleId == nullptr )
            {
                CPLDebug("SENTINEL2", "Missing granuleIdentifier attribute");
                continue;
            }

            if( eLevel == SENTINEL2_L2A )
            {
                for(CPLXMLNode* psIter3 = psIter2->psChild; psIter3 != nullptr;
                                                     psIter3 = psIter3->psNext )
                {
                    if( psIter3->eType != CXT_Element ||
                        ( !EQUAL(psIter3->pszValue, "IMAGE_ID_2A") &&
                          !EQUAL(psIter3->pszValue, "IMAGE_FILE") &&
                          !EQUAL(psIter3->pszValue, "IMAGE_FILE_2A") ) )
                    {
                        continue;
                    }
                    const char* pszTileName = CPLGetXMLValue(psIter3, nullptr, "");
                    size_t nLen = strlen(pszTileName);
                    // If granule name ends with resolution: _60m
                    if( nLen > 4 && pszTileName[nLen-4] == '_' &&
                        pszTileName[nLen-1] == 'm' )
                    {
                        int nResolution = atoi(pszTileName + nLen - 3);
                        if( poSetResolutions != nullptr )
                            (*poSetResolutions).insert(nResolution);
                        if( poMapResolutionsToBands != nullptr )
                        {
                            nLen -= 4;
                            if( nLen > 4 && pszTileName[nLen-4] == '_' &&
                                pszTileName[nLen-3] == 'B' )
                            {
                                (*poMapResolutionsToBands)[nResolution].
                                    insert(CPLString(pszTileName).substr(nLen-2,2));
                            }
                            else if ( nLen > strlen("S2A_USER_MSI_") &&
                                      pszTileName[8] == '_' &&
                                      pszTileName[12] == '_' &&
                                      !EQUALN(pszTileName+9, "MSI", 3) )
                            {
                                (*poMapResolutionsToBands)[nResolution].
                                    insert(CPLString(pszTileName).substr(9,3));
                            }
                        }
                    }
                }
            }

            // For L2A we can have several time the same granuleIdentifier
            // for the different resolutions
            if( aoSetGranuleId.find(pszGranuleId) != aoSetGranuleId.end() )
                continue;
            aoSetGranuleId.insert(pszGranuleId);

            /* S2A_OPER_MSI_L1C_TL_SGS__20151024T023555_A001758_T53JLJ_N01.04 --> */
            /* S2A_OPER_MTD_L1C_TL_SGS__20151024T023555_A001758_T53JLJ */
            // S2B_OPER_MSI_L2A_TL_MPS__20180823T122014_A007641_T34VFJ_N02.08
            CPLString osGranuleMTD = pszGranuleId;
            if( bIsCompact == 0 &&
                osGranuleMTD.size() > strlen("S2A_OPER_MSI_") &&
                osGranuleMTD[8] == '_' && osGranuleMTD[12] == '_' &&
                osGranuleMTD[osGranuleMTD.size()-7] == '_' &&
                osGranuleMTD[osGranuleMTD.size()-6] == 'N' &&
                osGranuleMTD[7] == 'R' )
            {
                osGranuleMTD[9] = 'M';
                osGranuleMTD[10] = 'T';
                osGranuleMTD[11] = 'D';
                osGranuleMTD.resize(osGranuleMTD.size()-7);
            }
            else if( bIsMSI2Ap )
            {
                osGranuleMTD = "MTD_TL";
                oGranuleId = "L2A_";
                // S2A_MSIL2A_20170823T094031_N0205_R036_T34VFJ_20170823T094252.SAFE
                // S2A_USER_MSI_L2A_TL_SGS__20170823T133142_A011330_T34VFJ_N02.05 -->
                // L2A_T34VFJ_A011330_20170823T094252
                const char* pszProductURI = CPLGetXMLValue(psProductInfo, "PRODUCT_URI_2A", nullptr);
                if( pszProductURI != nullptr )
                {
                    CPLString psProductURI(pszProductURI);
                    if( psProductURI.size() < 60 )
                    {
                        CPLDebug("SENTINEL2", "Invalid PRODUCT_URI_2A");
                        continue;
                    }
                    oGranuleId += psProductURI.substr(38, 7);
                    oGranuleId += CPLString(pszGranuleId).substr(41, 8).c_str();
                    oGranuleId += psProductURI.substr(45, 15);
                    pszGranuleId = oGranuleId.c_str();
                }
            }
            else
            {
                CPLDebug("SENTINEL2", "Invalid granule ID: %s", pszGranuleId);
                continue;
            }
            osGranuleMTD += ".xml";

            const char chSeparator = SENTINEL2GetPathSeparator(osDirname);
            CPLString osGranuleMTDPath = osDirname;
            osGranuleMTDPath += chSeparator;
            osGranuleMTDPath += "GRANULE";
            osGranuleMTDPath += chSeparator;
            osGranuleMTDPath += pszGranuleId;
            osGranuleMTDPath += chSeparator;
            osGranuleMTDPath += osGranuleMTD;
            osList.push_back(osGranuleMTDPath);
        }
    }

    return true;
}

/************************************************************************/
/*                     SENTINEL2GetUserProductMetadata()                */
/************************************************************************/

static
char** SENTINEL2GetUserProductMetadata( CPLXMLNode* psMainMTD,
                                    const char* pszRootNode )
{
    CPLStringList aosList;

    CPLXMLNode* psRoot =  CPLGetXMLNode(psMainMTD,
                                        CPLSPrintf("=%s", pszRootNode));
    if( psRoot == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find =%s", pszRootNode);
        return nullptr;
    }
    const char* psPIPath = "General_Info.Product_Info";
    CPLXMLNode* psProductInfo = CPLGetXMLNode(psRoot, psPIPath);
    if( psProductInfo == nullptr &&
        EQUAL(pszRootNode, "Level-2A_User_Product"))
    {
        psPIPath = "General_Info.L2A_Product_Info";
        psProductInfo = CPLGetXMLNode(psRoot, psPIPath);
    }
    if( psProductInfo == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find =%s", psPIPath);
        return nullptr;
    }
    int nDataTakeCounter = 1;
    for( CPLXMLNode* psIter = psProductInfo->psChild;
                     psIter != nullptr;
                     psIter = psIter->psNext )
    {
        if( psIter->eType != CXT_Element )
            continue;
        if( psIter->psChild != nullptr && psIter->psChild->eType == CXT_Text )
        {
            aosList.AddNameValue( psIter->pszValue,
                                  psIter->psChild->pszValue );
        }
        else if( EQUAL(psIter->pszValue, "Datatake") )
        {
            CPLString osPrefix(CPLSPrintf("DATATAKE_%d_", nDataTakeCounter));
            nDataTakeCounter ++;
            const char* pszId = CPLGetXMLValue(psIter, "datatakeIdentifier", nullptr);
            if( pszId )
                aosList.AddNameValue( (osPrefix + "ID").c_str(), pszId );
            for( CPLXMLNode* psIter2 = psIter->psChild;
                     psIter2 != nullptr;
                     psIter2 = psIter2->psNext )
            {
                if( psIter2->eType != CXT_Element )
                    continue;
                if( psIter2->psChild != nullptr && psIter2->psChild->eType == CXT_Text )
                {
                    aosList.AddNameValue( (osPrefix + psIter2->pszValue).c_str(),
                                          psIter2->psChild->pszValue );
                }
            }
        }
    }

    const char* psICPath = "General_Info.Product_Image_Characteristics";
    CPLXMLNode* psIC = CPLGetXMLNode(psRoot, psICPath);
    if( psIC == nullptr )
    {
        psICPath = "General_Info.L2A_Product_Image_Characteristics";
        psIC = CPLGetXMLNode(psRoot, psICPath);
    }
    if( psIC != nullptr )
    {
        for( CPLXMLNode* psIter = psIC->psChild; psIter != nullptr;
                                                 psIter = psIter->psNext )
        {
            if( psIter->eType != CXT_Element ||
                !EQUAL(psIter->pszValue, "Special_Values") )
            {
                continue;
            }
            const char* pszText = CPLGetXMLValue(psIter, "SPECIAL_VALUE_TEXT", nullptr);
            const char* pszIndex = CPLGetXMLValue(psIter, "SPECIAL_VALUE_INDEX", nullptr);
            if( pszText && pszIndex )
            {
                aosList.AddNameValue( (CPLString("SPECIAL_VALUE_") + pszText).c_str(),
                                       pszIndex );
            }
        }

        const char* pszQuantValue =
            CPLGetXMLValue(psIC, "QUANTIFICATION_VALUE", nullptr);
        if( pszQuantValue != nullptr )
            aosList.AddNameValue("QUANTIFICATION_VALUE", pszQuantValue);

        const char* pszRCU =
            CPLGetXMLValue(psIC, "Reflectance_Conversion.U", nullptr);
        if( pszRCU != nullptr )
            aosList.AddNameValue("REFLECTANCE_CONVERSION_U", pszRCU);

        // L2A specific
        CPLXMLNode* psQVL = CPLGetXMLNode(psIC, "L1C_L2A_Quantification_Values_List");
        if( psQVL == nullptr )
        {
            psQVL = CPLGetXMLNode(psIC, "Quantification_Values_List");
        }
        for( CPLXMLNode* psIter = psQVL ? psQVL->psChild : nullptr; psIter != nullptr;
                                                 psIter = psIter->psNext )
        {
            if( psIter->eType != CXT_Element )
            {
                continue;
            }
            aosList.AddNameValue( psIter->pszValue,
                                  CPLGetXMLValue(psIter, nullptr, nullptr));
            const char* pszUnit = CPLGetXMLValue(psIter, "unit", nullptr);
            if( pszUnit )
                aosList.AddNameValue( CPLSPrintf("%s_UNIT", psIter->pszValue), pszUnit);
        }

        const char* pszRefBand =
            CPLGetXMLValue(psIC, "REFERENCE_BAND", nullptr);
        if( pszRefBand != nullptr )
        {
            int nIdx = atoi(pszRefBand);
            if( nIdx >= 0 && nIdx < (int)NB_BANDS )
                aosList.AddNameValue("REFERENCE_BAND", asBandDesc[nIdx].pszBandName );
        }
    }

    CPLXMLNode* psQII = CPLGetXMLNode(psRoot, "Quality_Indicators_Info");
    if( psQII != nullptr )
    {
        const char* pszCC = CPLGetXMLValue(psQII, "Cloud_Coverage_Assessment", nullptr);
        if( pszCC )
            aosList.AddNameValue("CLOUD_COVERAGE_ASSESSMENT",
                                 pszCC);

        const char* pszDegradedAnc = CPLGetXMLValue(psQII,
            "Technical_Quality_Assessment.DEGRADED_ANC_DATA_PERCENTAGE", nullptr);
        if( pszDegradedAnc )
            aosList.AddNameValue("DEGRADED_ANC_DATA_PERCENTAGE", pszDegradedAnc);

        const char* pszDegradedMSI = CPLGetXMLValue(psQII,
            "Technical_Quality_Assessment.DEGRADED_MSI_DATA_PERCENTAGE", nullptr);
        if( pszDegradedMSI )
            aosList.AddNameValue("DEGRADED_MSI_DATA_PERCENTAGE", pszDegradedMSI);

        CPLXMLNode* psQualInspect = CPLGetXMLNode(psQII,
                            "Quality_Control_Checks.Quality_Inspections");
        for( CPLXMLNode* psIter = (psQualInspect ? psQualInspect->psChild : nullptr);
                     psIter != nullptr;
                     psIter = psIter->psNext )
        {
            // MSIL2A approach
            if( psIter->psChild != nullptr &&
                psIter->psChild->psChild != nullptr &&
                psIter->psChild->psNext != nullptr &&
                psIter->psChild->psChild->eType == CXT_Text &&
                psIter->psChild->psNext->eType == CXT_Text )
            {
                aosList.AddNameValue( psIter->psChild->psChild->pszValue,
                                      psIter->psChild->psNext->pszValue);
                continue;
            }

            if( psIter->eType != CXT_Element )
                continue;
            if( psIter->psChild != nullptr && psIter->psChild->eType == CXT_Text )
            {
                aosList.AddNameValue( psIter->pszValue,
                                    psIter->psChild->pszValue );
            }
        }

        CPLXMLNode* psICCQI = CPLGetXMLNode(psQII, "Image_Content_QI");
        if( psICCQI == nullptr )
        {
            CPLXMLNode* psL2A_QII = CPLGetXMLNode(psRoot, "L2A_Quality_Indicators_Info");
            if( psL2A_QII != nullptr )
            {
                psICCQI = CPLGetXMLNode(psL2A_QII, "Image_Content_QI");
            }
        }
        if( psICCQI != nullptr )
        {
            for( CPLXMLNode* psIter = psICCQI->psChild;
                psIter != nullptr;
                psIter = psIter->psNext )
            {
                if( psIter->eType != CXT_Element )
                    continue;
                if( psIter->psChild != nullptr && psIter->psChild->eType == CXT_Text )
                {
                    aosList.AddNameValue( psIter->pszValue,
                                        psIter->psChild->pszValue );
                }
            }
        }
    }

    return aosList.StealList();
}

/************************************************************************/
/*                        SENTINEL2GetResolutionSet()                   */
/************************************************************************/

static bool SENTINEL2GetResolutionSet(CPLXMLNode* psProductInfo,
                                      std::set<int>& oSetResolutions,
                                      std::map<int, std::set<CPLString> >&
                                                        oMapResolutionsToBands)
{

    CPLXMLNode* psBandList = CPLGetXMLNode(psProductInfo,
                                           "Query_Options.Band_List");
    if( psBandList == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s",
                 "Query_Options.Band_List");
        return false;
    }

    for(CPLXMLNode* psIter = psBandList->psChild; psIter != nullptr;
                                                  psIter = psIter->psNext )
    {
        if( psIter->eType != CXT_Element ||
            !EQUAL(psIter->pszValue, "BAND_NAME") )
        {
            continue;
        }
        const char* pszBandName = CPLGetXMLValue(psIter, nullptr, "");
        const SENTINEL2BandDescription* psBandDesc =
                                        SENTINEL2GetBandDesc(pszBandName);
        if( psBandDesc == nullptr )
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
    if( oSetResolutions.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find any band");
        return false;
    }
    return true;
}

/************************************************************************/
/*                  SENTINEL2GetPolygonWKTFromPosList()                 */
/************************************************************************/

static CPLString SENTINEL2GetPolygonWKTFromPosList(const char* pszPosList)
{
    CPLString osPolygon;
    char** papszTokens = CSLTokenizeString(pszPosList);
    int nTokens = CSLCount(papszTokens);
    int nDim = 2;
    if( (nTokens % 3) == 0 && nTokens >= 3 * 4 &&
        EQUAL(papszTokens[0], papszTokens[nTokens-3]) &&
        EQUAL(papszTokens[1], papszTokens[nTokens-2]) &&
        EQUAL(papszTokens[2], papszTokens[nTokens-1]) )
    {
        nDim = 3;
    }
    if( (nTokens % nDim) == 0 )
    {
        osPolygon = "POLYGON((";
        for(char** papszIter = papszTokens; *papszIter; papszIter += nDim )
        {
            if( papszIter != papszTokens )
                osPolygon += ", ";
            osPolygon += papszIter[1];
            osPolygon += " ";
            osPolygon += papszIter[0];
            if( nDim == 3 )
            {
                osPolygon += " ";
                osPolygon += papszIter[2];
            }
        }
        osPolygon += "))";
    }
    CSLDestroy(papszTokens);
    return osPolygon;
}

/************************************************************************/
/*                    SENTINEL2GetBandListForResolution()               */
/************************************************************************/

static CPLString SENTINEL2GetBandListForResolution(
                                        const std::set<CPLString>& oBandnames)
{
    CPLString osBandNames;
    for(std::set<CPLString>::const_iterator oIterBandnames = oBandnames.begin();
                                            oIterBandnames != oBandnames.end();
                                        ++oIterBandnames)
    {
        if( !osBandNames.empty() )
            osBandNames += ", ";
        const char* pszName = *oIterBandnames;
        if( *pszName == DIGIT_ZERO )
            pszName ++;
        if( atoi(pszName) > 0 )
            osBandNames += "B" + CPLString(pszName);
        else
            osBandNames += pszName;
    }
    return osBandNames;
}

/************************************************************************/
/*                         OpenL1BUserProduct()                         */
/************************************************************************/

GDALDataset *SENTINEL2Dataset::OpenL1BUserProduct( GDALOpenInfo * poOpenInfo )
{
    CPLXMLNode *psRoot = CPLParseXMLFile( poOpenInfo->pszFilename );
    if( psRoot == nullptr )
    {
        CPLDebug("SENTINEL2", "Cannot XML parse %s", poOpenInfo->pszFilename);
        return nullptr;
    }

    char* pszOriginalXML = CPLSerializeXMLTree(psRoot);
    CPLString osOriginalXML;
    if( pszOriginalXML )
        osOriginalXML = pszOriginalXML;
    CPLFree(pszOriginalXML);

    SENTINEL2_CPLXMLNodeHolder oXMLHolder(psRoot);
    CPLStripXMLNamespace(psRoot, nullptr, TRUE);

    CPLXMLNode* psProductInfo = CPLGetXMLNode(psRoot,
                            "=Level-1B_User_Product.General_Info.Product_Info");
    if( psProductInfo == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s",
                 "=Level-1B_User_Product.General_Info.Product_Info");
        return nullptr;
    }

    std::set<int> oSetResolutions;
    std::map<int, std::set<CPLString> > oMapResolutionsToBands;
    if( !SENTINEL2GetResolutionSet(psProductInfo,
                                   oSetResolutions,
                                   oMapResolutionsToBands) )
    {
        CPLDebug("SENTINEL2", "Failed to get resolution set");
        return nullptr;
    }

    std::vector<CPLString> aosGranuleList;
    if( !SENTINEL2GetGranuleList(psRoot,
                                 SENTINEL2_L1B,
                                 poOpenInfo->pszFilename,
                                 aosGranuleList) )
    {
        CPLDebug("SENTINEL2", "Failed to get granule list");
        return nullptr;
    }

    SENTINEL2DatasetContainer* poDS = new SENTINEL2DatasetContainer();
    char** papszMD = SENTINEL2GetUserProductMetadata(psRoot,
                                                 "Level-1B_User_Product");
    poDS->GDALDataset::SetMetadata(papszMD);
    CSLDestroy(papszMD);

    if( !osOriginalXML.empty() )
    {
        char* apszXMLMD[2] = { nullptr };
        apszXMLMD[0] = const_cast<char*>(osOriginalXML.c_str());
        apszXMLMD[1] = nullptr;
        poDS->GDALDataset::SetMetadata(apszXMLMD, "xml:SENTINEL2");
    }

    /* Create subdatsets per granules and resolution (10, 20, 60m) */
    int iSubDSNum = 1;
    for(size_t i = 0; i < aosGranuleList.size(); i++ )
    {
        for(std::set<int>::const_iterator oIterRes = oSetResolutions.begin();
                                    oIterRes != oSetResolutions.end();
                                ++oIterRes )
        {
            const int nResolution = *oIterRes;

            poDS->GDALDataset::SetMetadataItem(
                CPLSPrintf("SUBDATASET_%d_NAME", iSubDSNum),
                CPLSPrintf("SENTINEL2_L1B:%s:%dm",
                           aosGranuleList[i].c_str(),
                           nResolution),
                "SUBDATASETS");

            CPLString osBandNames = SENTINEL2GetBandListForResolution(
                                            oMapResolutionsToBands[nResolution]);

            CPLString osDesc(CPLSPrintf("Bands %s of granule %s with %dm resolution",
                                        osBandNames.c_str(),
                                        CPLGetFilename(aosGranuleList[i]),
                                        nResolution));
            poDS->GDALDataset::SetMetadataItem(
                CPLSPrintf("SUBDATASET_%d_DESC", iSubDSNum),
                osDesc.c_str(),
                "SUBDATASETS");

            iSubDSNum ++;
        }
    }

    const char* pszPosList = CPLGetXMLValue(psRoot,
        "=Level-1B_User_Product.Geometric_Info.Product_Footprint."
        "Product_Footprint.Global_Footprint.EXT_POS_LIST", nullptr);
    if( pszPosList != nullptr )
    {
        CPLString osPolygon = SENTINEL2GetPolygonWKTFromPosList(pszPosList);
        if( !osPolygon.empty() )
            poDS->GDALDataset::SetMetadataItem("FOOTPRINT", osPolygon.c_str());
    }

    return poDS;
}

/************************************************************************/
/*                    SENTINEL2GetL1BGranuleMetadata()                  */
/************************************************************************/

static
char** SENTINEL2GetL1BGranuleMetadata( CPLXMLNode* psMainMTD )
{
    CPLStringList aosList;

    CPLXMLNode* psRoot =  CPLGetXMLNode(psMainMTD,
                                        "=Level-1B_Granule_ID");
    if( psRoot == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find =Level-1B_Granule_ID");
        return nullptr;
    }
    CPLXMLNode* psGeneralInfo = CPLGetXMLNode(psRoot,
                                              "General_Info");
    for( CPLXMLNode* psIter = (psGeneralInfo ? psGeneralInfo->psChild : nullptr);
                     psIter != nullptr;
                     psIter = psIter->psNext )
    {
        if( psIter->eType != CXT_Element )
            continue;
        const char* pszValue = CPLGetXMLValue(psIter, nullptr, nullptr);
        if( pszValue != nullptr )
        {
            aosList.AddNameValue( psIter->pszValue, pszValue );
        }
    }

    CPLXMLNode* psGeometryHeader = CPLGetXMLNode(psRoot,
                        "Geometric_Info.Granule_Position.Geometric_Header");
    if( psGeometryHeader != nullptr )
    {
        const char* pszVal = CPLGetXMLValue(psGeometryHeader,
                                            "Incidence_Angles.ZENITH_ANGLE", nullptr);
        if( pszVal )
            aosList.AddNameValue( "INCIDENCE_ZENITH_ANGLE", pszVal );

        pszVal = CPLGetXMLValue(psGeometryHeader,
                                            "Incidence_Angles.AZIMUTH_ANGLE", nullptr);
        if( pszVal )
            aosList.AddNameValue( "INCIDENCE_AZIMUTH_ANGLE", pszVal );

        pszVal = CPLGetXMLValue(psGeometryHeader,
                                            "Solar_Angles.ZENITH_ANGLE", nullptr);
        if( pszVal )
            aosList.AddNameValue( "SOLAR_ZENITH_ANGLE", pszVal );

        pszVal = CPLGetXMLValue(psGeometryHeader,
                                            "Solar_Angles.AZIMUTH_ANGLE", nullptr);
        if( pszVal )
            aosList.AddNameValue( "SOLAR_AZIMUTH_ANGLE", pszVal );
    }

    CPLXMLNode* psQII = CPLGetXMLNode(psRoot, "Quality_Indicators_Info");
    if( psQII != nullptr )
    {
        CPLXMLNode* psICCQI = CPLGetXMLNode(psQII, "Image_Content_QI");
        for( CPLXMLNode* psIter = (psICCQI ? psICCQI->psChild : nullptr);
                     psIter != nullptr;
                     psIter = psIter->psNext )
        {
            if( psIter->eType != CXT_Element )
                continue;
            if( psIter->psChild != nullptr && psIter->psChild->eType == CXT_Text )
            {
                aosList.AddNameValue( psIter->pszValue,
                                    psIter->psChild->pszValue );
            }
        }
    }

    return aosList.StealList();
}

/************************************************************************/
/*                        SENTINEL2GetTilename()                        */
/************************************************************************/

static CPLString SENTINEL2GetTilename(const CPLString& osGranulePath,
                                      const CPLString& osGranuleName,
                                      const CPLString& osBandName,
                                      const CPLString& osProductURI = CPLString(),
                                      bool bIsPreview = false,
                                      int nPrecisionL2A = 0)
{
    bool granuleNameMatchTilename = true;
    CPLString osJPEG2000Name(osGranuleName);
    if( osJPEG2000Name.size() > 7 &&
        osJPEG2000Name[osJPEG2000Name.size()-7] == '_' &&
        osJPEG2000Name[osJPEG2000Name.size()-6] == 'N' )
    {
        osJPEG2000Name.resize(osJPEG2000Name.size()-7);
    }

    const SENTINEL2_L2A_BandDescription* psL2ABandDesc =
                    (nPrecisionL2A) ? SENTINEL2GetL2ABandDesc(osBandName): nullptr;

    CPLString osTile(osGranulePath);
    const char chSeparator = SENTINEL2GetPathSeparator(osTile);
    if( !osTile.empty() )
        osTile += chSeparator;
    bool procBaseLineIs1 = false;
    if( osJPEG2000Name.size() > 12 && osJPEG2000Name[8] == '_' && osJPEG2000Name[12] == '_' )
        procBaseLineIs1 = true;
    if( bIsPreview ||
        (psL2ABandDesc != nullptr &&
            (psL2ABandDesc->eLocation == TL_QI_DATA ) ) )
    {
        osTile += "QI_DATA";
        osTile += chSeparator;
        if( procBaseLineIs1 )
        {
            if( atoi(osBandName) > 0 )
            {
                osJPEG2000Name[9] = 'P';
                osJPEG2000Name[10] = 'V';
                osJPEG2000Name[11] = 'I';
            }
            else if( nPrecisionL2A && osBandName.size() == 3 )
            {
                osJPEG2000Name[9] = osBandName[0];
                osJPEG2000Name[10] = osBandName[1];
                osJPEG2000Name[11] = osBandName[2];
            }
            osTile += osJPEG2000Name;
        }
        else
        {
            osTile += "MSK_";
            osTile += osBandName;
            osTile += "PRB";
        }
        if( nPrecisionL2A && !bIsPreview )
            osTile += CPLSPrintf("_%02dm", nPrecisionL2A);
    }
    else
    {
        osTile += "IMG_DATA";
        osTile += chSeparator;
        if( ( (psL2ABandDesc != nullptr && psL2ABandDesc->eLocation == TL_IMG_DATA_Rxxm) ||
              (psL2ABandDesc == nullptr && nPrecisionL2A != 0) ) &&
            (!procBaseLineIs1 || osBandName!="SCL") )
        {
            osTile += CPLSPrintf("R%02dm", nPrecisionL2A);
            osTile += chSeparator;
        }
        if( procBaseLineIs1 )
        {
            if( atoi(osBandName) > 0 )
            {
                osJPEG2000Name[9] = 'M';
                osJPEG2000Name[10] = 'S';
                osJPEG2000Name[11] = 'I';
            }
            else if( nPrecisionL2A && osBandName.size() == 3 )
            {
                osJPEG2000Name[9] = osBandName[0];
                osJPEG2000Name[10] = osBandName[1];
                osJPEG2000Name[11] = osBandName[2];
            }
        }
        else if( osProductURI.size() > 44 &&
                 osProductURI.substr(3, 8) == "_MSIL2A_" )
        {
            osTile += osProductURI.substr(38, 6);
            osTile += osProductURI.substr(10, 16);
            granuleNameMatchTilename = false;
        }
        else
        {
            CPLDebug("SENTINEL2", "Invalid granule path: %s",
                     osGranulePath.c_str());
        }
        if( granuleNameMatchTilename )
            osTile += osJPEG2000Name;
        if( atoi(osBandName) > 0 )
        {
            osTile += "_B";
            if( osBandName.size() == 3 && osBandName[0] == '0' )
                osTile += osBandName.substr(1);
            else
                osTile += osBandName;
        }
        else
        if( !procBaseLineIs1 )
        {
            osTile += "_";
            osTile += osBandName;
        }
        if( nPrecisionL2A )
            osTile += CPLSPrintf("_%02dm", nPrecisionL2A);
    }
    osTile += ".jp2";
    return osTile;
}

/************************************************************************/
/*                 SENTINEL2GetMainMTDFilenameFromGranuleMTD()          */
/************************************************************************/

static CPLString SENTINEL2GetMainMTDFilenameFromGranuleMTD(const char* pszFilename)
{
    // Look for product MTD file
    CPLString osTopDir(CPLFormFilename(
        CPLFormFilename( CPLGetDirname(pszFilename), "..", nullptr ),
        "..", nullptr ));

    // Workaround to avoid long filenames on Windows
    if( CPLIsFilenameRelative(pszFilename) )
    {
        // GRANULE/bla/bla.xml
        const char* pszPath = CPLGetPath(pszFilename);
        if( strchr(pszPath, '/') || strchr(pszPath, '\\') )
        {
            osTopDir = CPLGetPath(CPLGetPath(pszPath));
            if( osTopDir == "" )
                osTopDir = ".";
        }
    }

    char** papszContents = VSIReadDir(osTopDir);
    CPLString osMainMTD;
    for(char** papszIter = papszContents; papszIter && *papszIter; ++papszIter)
    {
        if( strlen(*papszIter) >= strlen("S2A_XXXX_MTD") &&
            (STARTS_WITH_CI(*papszIter, "S2A_") ||
             STARTS_WITH_CI(*papszIter, "S2B_")) &&
             EQUALN(*papszIter + strlen("S2A_XXXX"), "_MTD", 4) )
        {
            osMainMTD = CPLFormFilename(osTopDir, *papszIter, nullptr);
            break;
        }
    }
    CSLDestroy(papszContents);
    return osMainMTD;
}

/************************************************************************/
/*            SENTINEL2GetResolutionSetAndMainMDFromGranule()           */
/************************************************************************/

static void SENTINEL2GetResolutionSetAndMainMDFromGranule(
                    const char* pszFilename,
                    const char* pszRootPathWithoutEqual,
                    int nResolutionOfInterest,
                    std::set<int>& oSetResolutions,
                    std::map<int, std::set<CPLString> >& oMapResolutionsToBands,
                    char**& papszMD,
                    CPLXMLNode** ppsRootMainMTD)
{
    CPLString osMainMTD(SENTINEL2GetMainMTDFilenameFromGranuleMTD(pszFilename));

    // Parse product MTD if available
    papszMD = nullptr;
    if( !osMainMTD.empty() &&
        /* env var for debug only */
        CPLTestBool(CPLGetConfigOption("SENTINEL2_USE_MAIN_MTD", "YES")) )
    {
        CPLXMLNode *psRootMainMTD = CPLParseXMLFile( osMainMTD );
        if( psRootMainMTD != nullptr )
        {
            CPLStripXMLNamespace(psRootMainMTD, nullptr, TRUE);

            CPLXMLNode* psProductInfo = CPLGetXMLNode(psRootMainMTD,
                CPLSPrintf("=%s.General_Info.Product_Info", pszRootPathWithoutEqual));
            if( psProductInfo != nullptr )
            {
                SENTINEL2GetResolutionSet(psProductInfo,
                                          oSetResolutions,
                                          oMapResolutionsToBands);
            }

            papszMD = SENTINEL2GetUserProductMetadata(psRootMainMTD,
                                                      pszRootPathWithoutEqual);
            if( ppsRootMainMTD != nullptr )
                *ppsRootMainMTD = psRootMainMTD;
            else
                CPLDestroyXMLNode(psRootMainMTD);
        }
    }
    else
    {
        // If no main MTD file found, then probe all bands for resolution (of
        // interest if there's one, or all resolutions otherwise)
        for(size_t i=0;i<NB_BANDS;i++)
        {
            if( nResolutionOfInterest != 0 &&
                asBandDesc[i].nResolution != nResolutionOfInterest )
            {
                continue;
            }
            CPLString osBandName = asBandDesc[i].pszBandName + 1; /* skip B character */
            if( atoi(osBandName) < 10 )
                osBandName = "0" + osBandName;

            CPLString osTile(SENTINEL2GetTilename(CPLGetPath(pszFilename),
                                                  CPLGetBasename(pszFilename),
                                                  osBandName));
            VSIStatBufL sStat;
            if( VSIStatExL(osTile, &sStat, VSI_STAT_EXISTS_FLAG) == 0 )
            {
                oMapResolutionsToBands[asBandDesc[i].nResolution].insert(osBandName);
                oSetResolutions.insert(asBandDesc[i].nResolution);
            }
        }
    }
}

/************************************************************************/
/*                           OpenL1BGranule()                           */
/************************************************************************/

GDALDataset *SENTINEL2Dataset::OpenL1BGranule( const char* pszFilename,
                                               CPLXMLNode** ppsRoot,
                                               int nResolutionOfInterest,
                                               std::set<CPLString> *poBandSet )
{
    CPLXMLNode *psRoot = CPLParseXMLFile( pszFilename );
    if( psRoot == nullptr )
    {
        CPLDebug("SENTINEL2", "Cannot XML parse %s", pszFilename);
        return nullptr;
    }

    char* pszOriginalXML = CPLSerializeXMLTree(psRoot);
    CPLString osOriginalXML;
    if( pszOriginalXML )
        osOriginalXML = pszOriginalXML;
    CPLFree(pszOriginalXML);

    SENTINEL2_CPLXMLNodeHolder oXMLHolder(psRoot);
    CPLStripXMLNamespace(psRoot, nullptr, TRUE);

    SENTINEL2DatasetContainer* poDS = new SENTINEL2DatasetContainer();

    if( !osOriginalXML.empty() )
    {
        char* apszXMLMD[2];
        apszXMLMD[0] = const_cast<char*>(osOriginalXML.c_str());
        apszXMLMD[1] = nullptr;
        poDS->GDALDataset::SetMetadata(apszXMLMD, "xml:SENTINEL2");
    }

    std::set<int> oSetResolutions;
    std::map<int, std::set<CPLString> > oMapResolutionsToBands;
    char** papszMD = nullptr;
    SENTINEL2GetResolutionSetAndMainMDFromGranule(pszFilename,
                                                  "Level-1B_User_Product",
                                                  nResolutionOfInterest,
                                                  oSetResolutions,
                                                  oMapResolutionsToBands,
                                                  papszMD,
                                                  nullptr);
    if( poBandSet != nullptr )
        *poBandSet = oMapResolutionsToBands[nResolutionOfInterest];

    char** papszGranuleMD = SENTINEL2GetL1BGranuleMetadata(psRoot);
    papszMD = CSLMerge(papszMD, papszGranuleMD);
    CSLDestroy(papszGranuleMD);

    // Remove CLOUD_COVERAGE_ASSESSMENT that comes from main metadata, if granule
    // CLOUDY_PIXEL_PERCENTAGE is present.
    if( CSLFetchNameValue(papszMD, "CLOUDY_PIXEL_PERCENTAGE") != nullptr &&
        CSLFetchNameValue(papszMD, "CLOUD_COVERAGE_ASSESSMENT") != nullptr )
    {
        papszMD = CSLSetNameValue(papszMD, "CLOUD_COVERAGE_ASSESSMENT", nullptr);
    }

    poDS->GDALDataset::SetMetadata(papszMD);
    CSLDestroy(papszMD);

    // Get the footprint
    const char* pszPosList = CPLGetXMLValue(psRoot,
        "=Level-1B_Granule_ID.Geometric_Info.Granule_Footprint."
        "Granule_Footprint.Footprint.EXT_POS_LIST", nullptr);
    if( pszPosList != nullptr )
    {
        CPLString osPolygon = SENTINEL2GetPolygonWKTFromPosList(pszPosList);
        if( !osPolygon.empty() )
            poDS->GDALDataset::SetMetadataItem("FOOTPRINT", osPolygon.c_str());
    }

    /* Create subdatsets per resolution (10, 20, 60m) */
    int iSubDSNum = 1;
    for(std::set<int>::const_iterator oIterRes = oSetResolutions.begin();
                                oIterRes != oSetResolutions.end();
                            ++oIterRes )
    {
        const int nResolution = *oIterRes;

        poDS->GDALDataset::SetMetadataItem(
            CPLSPrintf("SUBDATASET_%d_NAME", iSubDSNum),
            CPLSPrintf("SENTINEL2_L1B:%s:%dm",
                        pszFilename,
                        nResolution),
            "SUBDATASETS");

        CPLString osBandNames = SENTINEL2GetBandListForResolution(
                                        oMapResolutionsToBands[nResolution]);

        CPLString osDesc(CPLSPrintf("Bands %s with %dm resolution",
                                    osBandNames.c_str(),
                                    nResolution));
        poDS->GDALDataset::SetMetadataItem(
            CPLSPrintf("SUBDATASET_%d_DESC", iSubDSNum),
            osDesc.c_str(),
            "SUBDATASETS");

        iSubDSNum ++;
    }

    if( ppsRoot != nullptr )
    {
        *ppsRoot = oXMLHolder.Release();
    }

    return poDS;
}

/************************************************************************/
/*                     SENTINEL2SetBandMetadata()                       */
/************************************************************************/

static void SENTINEL2SetBandMetadata(GDALRasterBand* poBand,
                                     const CPLString& osBandName)
{
    CPLString osLookupBandName(osBandName);
    if( osLookupBandName[0] == '0' )
        osLookupBandName = osLookupBandName.substr(1);
    if( atoi(osLookupBandName) > 0 )
        osLookupBandName = "B" + osLookupBandName;

    CPLString osBandDesc(osLookupBandName);
    const SENTINEL2BandDescription* psBandDesc =
                            SENTINEL2GetBandDesc(osLookupBandName);
    if( psBandDesc != nullptr )
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
    else
    {
        const SENTINEL2_L2A_BandDescription* psL2ABandDesc =
                                        SENTINEL2GetL2ABandDesc(osBandName);
        if(psL2ABandDesc != nullptr )
        {
            osBandDesc += ", ";
            osBandDesc += psL2ABandDesc->pszBandDescription;
        }

        poBand->SetMetadataItem("BANDNAME", osBandName);
    }
    poBand->SetDescription(osBandDesc);
}

/************************************************************************/
/*                         OpenL1BSubdataset()                          */
/************************************************************************/

GDALDataset *SENTINEL2Dataset::OpenL1BSubdataset( GDALOpenInfo * poOpenInfo )
{
    CPLString osFilename;
    CPLAssert( STARTS_WITH_CI(poOpenInfo->pszFilename, "SENTINEL2_L1B:") );
    osFilename = poOpenInfo->pszFilename + strlen("SENTINEL2_L1B:");
    const char* pszPrecision = strrchr(osFilename.c_str(), ':');
    if( pszPrecision == nullptr || pszPrecision == osFilename.c_str() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid syntax for SENTINEL2_L1B:");
        return nullptr;
    }
    const int nSubDSPrecision = atoi(pszPrecision + 1);
    if( nSubDSPrecision != 10 && nSubDSPrecision != 20 && nSubDSPrecision != 60 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported precision: %d",
                 nSubDSPrecision);
        return nullptr;
    }
    osFilename.resize( pszPrecision - osFilename.c_str() );

    CPLXMLNode* psRoot = nullptr;
    std::set<CPLString> oSetBands;
    GDALDataset* poTmpDS = OpenL1BGranule( osFilename, &psRoot,
                                           nSubDSPrecision, &oSetBands);
    if( poTmpDS == nullptr )
    {
        CPLDebug("SENTINEL2", "Failed to open L1B granule %s", osFilename.c_str());
        return nullptr;
    }

    SENTINEL2_CPLXMLNodeHolder oXMLHolder(psRoot);

    std::vector<CPLString> aosBands;
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

    int nBits = 0; /* 0 = unknown yet*/
    int nValMax = 0; /* 0 = unknown yet*/
    int nRows = 0;
    int nCols = 0;
    CPLXMLNode* psGranuleDimensions =
        CPLGetXMLNode(psRoot, "=Level-1B_Granule_ID.Geometric_Info.Granule_Dimensions");
    if( psGranuleDimensions == nullptr )
    {
        for( size_t i=0; i<aosBands.size(); i++ )
        {
            CPLString osTile(SENTINEL2GetTilename(CPLGetPath(osFilename),
                                                  CPLGetBasename(osFilename),
                                                  aosBands[i]));
            if( SENTINEL2GetTileInfo(osTile, &nCols, &nRows, &nBits) )
            {
                if( nBits <= 16 )
                    nValMax = (1 << nBits) - 1;
                else
                {
                    CPLDebug("SENTINEL2", "Unexpected bit depth %d", nBits);
                    nValMax = 65535;
                }
                break;
            }
        }
    }
    else
    {
        for(CPLXMLNode* psIter = psGranuleDimensions->psChild; psIter != nullptr;
                                                        psIter = psIter->psNext)
        {
            if( psIter->eType != CXT_Element )
                continue;
            if( EQUAL(psIter->pszValue, "Size") &&
                atoi(CPLGetXMLValue(psIter, "resolution", "")) == nSubDSPrecision )
            {
                const char* pszRows = CPLGetXMLValue(psIter, "NROWS", nullptr);
                if( pszRows == nullptr )
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s",
                            "NROWS");
                    delete poTmpDS;
                    return nullptr;
                }
                const char* pszCols = CPLGetXMLValue(psIter, "NCOLS", nullptr);
                if( pszCols == nullptr )
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s",
                            "NCOLS");
                    delete poTmpDS;
                    return nullptr;
                }
                nRows = atoi(pszRows);
                nCols = atoi(pszCols);
                break;
            }
        }
    }
    if( nRows <= 0 || nCols <= 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find granule dimension");
        delete poTmpDS;
        return nullptr;
    }

    SENTINEL2Dataset* poDS = new SENTINEL2Dataset(nCols, nRows);
    poDS->aosNonJP2Files.push_back(osFilename);

    // Transfer metadata
    poDS->GDALDataset::SetMetadata( poTmpDS->GetMetadata() );
    poDS->GDALDataset::SetMetadata( poTmpDS->GetMetadata("xml:SENTINEL2"), "xml:SENTINEL2" );

    delete poTmpDS;

/* -------------------------------------------------------------------- */
/*      Initialize bands.                                               */
/* -------------------------------------------------------------------- */

    int nSaturatedVal = atoi(CSLFetchNameValueDef(poDS->GetMetadata(), "SPECIAL_VALUE_SATURATED", "-1"));
    int nNodataVal = atoi(CSLFetchNameValueDef(poDS->GetMetadata(), "SPECIAL_VALUE_NODATA", "-1"));

    const bool bAlpha =
        CPLTestBool(SENTINEL2GetOption(poOpenInfo, "ALPHA", "FALSE"));
    const int nBands = ((bAlpha) ? 1 : 0) + static_cast<int>(aosBands.size());
    const int nAlphaBand = (!bAlpha) ? 0 : nBands;
    const GDALDataType eDT = GDT_UInt16;

    for(int nBand=1;nBand<=nBands;nBand++)
    {
        VRTSourcedRasterBand* poBand = nullptr;

        if( nBand != nAlphaBand )
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
        if( nBand == nAlphaBand )
            poBand->SetColorInterpretation(GCI_AlphaBand);

        CPLString osBandName;
        if( nBand != nAlphaBand )
        {
            osBandName = aosBands[nBand-1];
            SENTINEL2SetBandMetadata( poBand, osBandName);
        }
        else
            osBandName = aosBands[0];

        CPLString osTile(SENTINEL2GetTilename(CPLGetPath(osFilename),
                                              CPLGetBasename(osFilename),
                                              osBandName));

        bool bTileFound = false;
        if( nValMax == 0 )
        {
            /* It is supposed to be 12 bits, but some products have 15 bits */
            if( SENTINEL2GetTileInfo(osTile, nullptr, nullptr, &nBits) )
            {
                bTileFound = true;
                if( nBits <= 16 )
                    nValMax = (1 << nBits) - 1;
                else
                {
                    CPLDebug("SENTINEL2", "Unexpected bit depth %d", nBits);
                    nValMax = 65535;
                }
            }
        }
        else
        {
            VSIStatBufL sStat;
            if( VSIStatExL(osTile, &sStat, VSI_STAT_EXISTS_FLAG) == 0 )
                bTileFound = true;
        }
        if( !bTileFound )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Tile %s not found on filesystem. Skipping it",
                    osTile.c_str());
            continue;
        }

        if( nBand != nAlphaBand )
        {
            poBand->AddSimpleSource( osTile, 1,
                                    0, 0,
                                    poDS->nRasterXSize,
                                    poDS->nRasterYSize,
                                    0,
                                    0,
                                    poDS->nRasterXSize,
                                    poDS->nRasterYSize);
        }
        else
        {
            poBand->AddComplexSource( osTile, 1,
                                        0, 0,
                                        poDS->nRasterXSize,
                                        poDS->nRasterYSize,
                                        0,
                                        0,
                                        poDS->nRasterXSize,
                                        poDS->nRasterYSize,
                                        nValMax /* offset */,
                                        0 /* scale */);
        }

        if( (nBits % 8) != 0 )
        {
            poBand->SetMetadataItem("NBITS",
                                CPLSPrintf("%d", nBits), "IMAGE_STRUCTURE");
        }
    }

/* -------------------------------------------------------------------- */
/*      Add georeferencing.                                             */
/* -------------------------------------------------------------------- */
    //const char* pszDirection = poDS->GetMetadataItem("DATATAKE_1_SENSING_ORBIT_DIRECTION");
    const char* pszFootprint = poDS->GetMetadataItem("FOOTPRINT");
    if( pszFootprint != nullptr )
    {
        // For descending orbits, we have observed that the order of points in
        // the polygon is UL, LL, LR, UR. That might not be true for ascending orbits
        // but let's assume it...
        OGRGeometry* poGeom = nullptr;
        if( OGRGeometryFactory::createFromWkt( pszFootprint, nullptr, &poGeom)
                                                                == OGRERR_NONE &&
            poGeom != nullptr &&
            wkbFlatten(poGeom->getGeometryType()) == wkbPolygon )
        {
            OGRLinearRing* poRing =
                reinterpret_cast<OGRPolygon*>(poGeom)->getExteriorRing();
            if( poRing != nullptr && poRing->getNumPoints() == 5 )
            {
                GDAL_GCP    asGCPList[5];
                memset( asGCPList, 0, sizeof(asGCPList) );
                for(int i=0;i<4;i++)
                {
                    asGCPList[i].dfGCPX = poRing->getX(i);
                    asGCPList[i].dfGCPY = poRing->getY(i);
                    asGCPList[i].dfGCPZ = poRing->getZ(i);
                }
                asGCPList[0].dfGCPPixel = 0;
                asGCPList[0].dfGCPLine = 0;
                asGCPList[1].dfGCPPixel = 0;
                asGCPList[1].dfGCPLine = poDS->nRasterYSize;
                asGCPList[2].dfGCPPixel = poDS->nRasterXSize;
                asGCPList[2].dfGCPLine = poDS->nRasterYSize;
                asGCPList[3].dfGCPPixel = poDS->nRasterXSize;
                asGCPList[3].dfGCPLine = 0;

                int nGCPCount = 4;
                CPLXMLNode* psGeometryHeader =
                    CPLGetXMLNode(psRoot,
                                  "=Level-1B_Granule_ID.Geometric_Info."
                                  "Granule_Position.Geometric_Header");
                if( psGeometryHeader != nullptr )
                {
                    const char* pszGC =
                        CPLGetXMLValue(psGeometryHeader, "GROUND_CENTER", nullptr);
                    const char* pszQLCenter =
                        CPLGetXMLValue(psGeometryHeader, "QL_CENTER", nullptr);
                    if( pszGC != nullptr && pszQLCenter != nullptr && EQUAL(pszQLCenter, "0 0") )
                    {
                        char** papszTokens = CSLTokenizeString(pszGC);
                        if( CSLCount(papszTokens) >= 2 )
                        {
                            nGCPCount = 5;
                            asGCPList[4].dfGCPX = CPLAtof(papszTokens[1]);
                            asGCPList[4].dfGCPY = CPLAtof(papszTokens[0]);
                            if( CSLCount(papszTokens) >= 3 )
                                asGCPList[4].dfGCPZ = CPLAtof(papszTokens[2]);
                            asGCPList[4].dfGCPLine = poDS->nRasterYSize / 2.0;
                            asGCPList[4].dfGCPPixel = poDS->nRasterXSize / 2.0;
                        }
                        CSLDestroy(papszTokens);
                    }
                }

                poDS->SetGCPs( nGCPCount, asGCPList, SRS_WKT_WGS84_LAT_LONG );
                GDALDeinitGCPs( nGCPCount, asGCPList );
            }
        }
        delete poGeom;
    }

/* -------------------------------------------------------------------- */
/*      Initialize overview information.                                */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    CPLString osOverviewFile;
    osOverviewFile = CPLSPrintf("%s_%dm.tif.ovr",
                                osFilename.c_str(), nSubDSPrecision);
    poDS->SetMetadataItem( "OVERVIEW_FILE", osOverviewFile, "OVERVIEWS" );
    poDS->oOvManager.Initialize( poDS, ":::VIRTUAL:::" );

    return poDS;
}

/************************************************************************/
/*                 SENTINEL2GetGranuleList_L1CSafeCompact()             */
/************************************************************************/

static bool SENTINEL2GetGranuleList_L1CSafeCompact(CPLXMLNode* psMainMTD,
                                    const char* pszFilename,
                                    std::vector<L1CSafeCompatGranuleDescription>& osList)
{
    CPLXMLNode* psProductInfo = CPLGetXMLNode(psMainMTD,
                                "=Level-1C_User_Product.General_Info.Product_Info");
    if( psProductInfo == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s",
                        "=Level-1C_User_Product.General_Info.Product_Info");
        return false;
    }

    CPLXMLNode* psProductOrganisation =
                        CPLGetXMLNode(psProductInfo, "Product_Organisation");
    if( psProductOrganisation == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s", "Product_Organisation");
        return false;
    }

    CPLString osDirname( CPLGetDirname(pszFilename) );
#ifdef HAVE_READLINK
    char szPointerFilename[2048];
    int nBytes = static_cast<int>(readlink(pszFilename, szPointerFilename,
                                           sizeof(szPointerFilename)));
    if (nBytes != -1)
    {
        const int nOffset =
            std::min(nBytes, static_cast<int>(sizeof(szPointerFilename)-1));
        szPointerFilename[nOffset] = '\0';
        osDirname = CPLGetDirname(szPointerFilename);
    }
#endif

    const char chSeparator = SENTINEL2GetPathSeparator(osDirname);
    for(CPLXMLNode* psIter = psProductOrganisation->psChild; psIter != nullptr;
                                                    psIter = psIter->psNext )
    {
        if( psIter->eType != CXT_Element ||
            !EQUAL(psIter->pszValue, "Granule_List") )
        {
            continue;
        }
        for(CPLXMLNode* psIter2 = psIter->psChild; psIter2 != nullptr;
                                                     psIter2 = psIter2->psNext )
        {
            if( psIter2->eType != CXT_Element ||
                !EQUAL(psIter2->pszValue, "Granule") )
            {
                continue;
            }

            const char* pszImageFile = CPLGetXMLValue(psIter2, "IMAGE_FILE", nullptr);
            if( pszImageFile == nullptr || strlen(pszImageFile) < 3 )
            {
                CPLDebug("SENTINEL2", "Missing IMAGE_FILE element");
                continue;
            }
            L1CSafeCompatGranuleDescription oDesc;
            oDesc.osBandPrefixPath = osDirname + chSeparator + pszImageFile;
            oDesc.osBandPrefixPath.resize( oDesc.osBandPrefixPath.size() - 3 ); // strip B12
            // GRANULE/L1C_T30TXT_A007999_20170102T111441/IMG_DATA/T30TXT_20170102T111442_B12 -->
            // GRANULE/L1C_T30TXT_A007999_20170102T111441/MTD_TL.xml
            oDesc.osMTDTLPath = osDirname + chSeparator +
                                CPLGetDirname(CPLGetDirname(pszImageFile)) +
                                chSeparator + "MTD_TL.xml";
            osList.push_back(oDesc);
        }
    }

    return true;
}

/************************************************************************/
/*                 SENTINEL2GetGranuleList_L2ASafeCompact()             */
/************************************************************************/

static bool SENTINEL2GetGranuleList_L2ASafeCompact(CPLXMLNode* psMainMTD,
                                    const char* pszFilename,
                                    std::vector<L1CSafeCompatGranuleDescription>& osList)
{
    const char* pszNodePath = "=Level-2A_User_Product.General_Info.Product_Info";
    CPLXMLNode* psProductInfo = CPLGetXMLNode(psMainMTD, pszNodePath);
    if( psProductInfo == nullptr )
    {
        pszNodePath = "=Level-2A_User_Product.General_Info.L2A_Product_Info";
        psProductInfo = CPLGetXMLNode(psMainMTD, pszNodePath);
    }
    if( psProductInfo == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s",
                        pszNodePath);
        return false;
    }

    CPLXMLNode* psProductOrganisation =
                        CPLGetXMLNode(psProductInfo, "Product_Organisation");
    if( psProductOrganisation == nullptr )
    {
        psProductOrganisation =
                        CPLGetXMLNode(psProductInfo, "L2A_Product_Organisation");
        if( psProductOrganisation == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s", "Product_Organisation");
            return false;
        }
    }

    CPLString osDirname( CPLGetDirname(pszFilename) );
#ifdef HAVE_READLINK
    char szPointerFilename[2048];
    int nBytes = static_cast<int>(readlink(pszFilename, szPointerFilename,
                                           sizeof(szPointerFilename)));
    if (nBytes != -1)
    {
        const int nOffset =
            std::min(nBytes, static_cast<int>(sizeof(szPointerFilename)-1));
        szPointerFilename[nOffset] = '\0';
        osDirname = CPLGetDirname(szPointerFilename);
    }
#endif

    const char chSeparator = SENTINEL2GetPathSeparator(osDirname);
    for(CPLXMLNode* psIter = psProductOrganisation->psChild; psIter != nullptr;
                                                    psIter = psIter->psNext )
    {
        if( psIter->eType != CXT_Element ||
            !EQUAL(psIter->pszValue, "Granule_List") )
        {
            continue;
        }
        for(CPLXMLNode* psIter2 = psIter->psChild; psIter2 != nullptr;
                                                     psIter2 = psIter2->psNext )
        {
            if( psIter2->eType != CXT_Element ||
                !EQUAL(psIter2->pszValue, "Granule") )
            {
                continue;
            }

            const char* pszImageFile = CPLGetXMLValue(psIter2, "IMAGE_FILE", nullptr);
            if( pszImageFile == nullptr )
            {
                pszImageFile = CPLGetXMLValue(psIter2, "IMAGE_FILE_2A", nullptr);
                if( pszImageFile == nullptr || strlen(pszImageFile) < 3 )
                {
                    CPLDebug("SENTINEL2", "Missing IMAGE_FILE element");
                    continue;
                }
            }
            L1CSafeCompatGranuleDescription oDesc;
            oDesc.osBandPrefixPath = osDirname + chSeparator + pszImageFile;
            if( oDesc.osBandPrefixPath.size() < 36 )
            {
                CPLDebug("SENTINEL2", "Band prefix path too short");
                continue;
            }
            oDesc.osBandPrefixPath.resize( oDesc.osBandPrefixPath.size() - 36 );
            // GRANULE/L1C_T30TXT_A007999_20170102T111441/IMG_DATA/T30TXT_20170102T111442_B12_60m -->
            // GRANULE/L1C_T30TXT_A007999_20170102T111441/MTD_TL.xml
            oDesc.osMTDTLPath = osDirname + chSeparator +
                                CPLGetDirname(CPLGetDirname(pszImageFile));
            if( oDesc.osMTDTLPath.size() < 9 )
            {
                CPLDebug("SENTINEL2", "MTDTL path too short");
                continue;
            }
            oDesc.osMTDTLPath.resize( oDesc.osMTDTLPath.size() - 9 );
            oDesc.osMTDTLPath = oDesc.osMTDTLPath +
                                chSeparator + "MTD_TL.xml";
            osList.push_back(oDesc);
        }
    }

    return true;
}

/************************************************************************/
/*                           OpenL1C_L2A()                              */
/************************************************************************/

GDALDataset *SENTINEL2Dataset::OpenL1C_L2A( const char* pszFilename,
                                            SENTINEL2Level eLevel )
{
    CPLXMLNode *psRoot = CPLParseXMLFile( pszFilename );
    if( psRoot == nullptr )
    {
        CPLDebug("SENTINEL2", "Cannot XML parse %s", pszFilename);
        return nullptr;
    }

    char* pszOriginalXML = CPLSerializeXMLTree(psRoot);
    CPLString osOriginalXML;
    if( pszOriginalXML )
        osOriginalXML = pszOriginalXML;
    CPLFree(pszOriginalXML);

    SENTINEL2_CPLXMLNodeHolder oXMLHolder(psRoot);
    CPLStripXMLNamespace(psRoot, nullptr, TRUE);

    const char* pszNodePath = (eLevel == SENTINEL2_L1C ) ?
                "=Level-1C_User_Product.General_Info.Product_Info" :
                "=Level-2A_User_Product.General_Info.Product_Info";
    CPLXMLNode* psProductInfo = CPLGetXMLNode(psRoot, pszNodePath);
    if( psProductInfo == nullptr && eLevel == SENTINEL2_L2A )
    {
        pszNodePath = "=Level-2A_User_Product.General_Info.L2A_Product_Info";
        psProductInfo = CPLGetXMLNode(psRoot, pszNodePath);
    }
    if( psProductInfo == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s", pszNodePath);
        return nullptr;
    }

    const bool bIsSafeCompact =
        EQUAL(CPLGetXMLValue(psProductInfo, "Query_Options.PRODUCT_FORMAT", ""),
              "SAFE_COMPACT");

    std::set<int> oSetResolutions;
    std::map<int, std::set<CPLString> > oMapResolutionsToBands;
    if( bIsSafeCompact )
    {
        for(unsigned int i = 0; i < NB_BANDS; ++i)
        {
            // L2 does not contain B10
            if( i == 10 && eLevel == SENTINEL2_L2A )
                continue;
            const SENTINEL2BandDescription * psBandDesc = &asBandDesc[i];
            oSetResolutions.insert( psBandDesc->nResolution );
            CPLString osName = psBandDesc->pszBandName + 1; /* skip B character */
            if( atoi(osName) < 10 )
                osName = "0" + osName;
            oMapResolutionsToBands[psBandDesc->nResolution].insert(osName);
        }
        if (eLevel == SENTINEL2_L2A )
        {
            for( const auto& sL2ABandDesc: asL2ABandDesc)
            {
                oSetResolutions.insert( sL2ABandDesc.nResolution );
                oMapResolutionsToBands[sL2ABandDesc.nResolution].insert(sL2ABandDesc.pszBandName);
            }
        }
    }
    else if( eLevel == SENTINEL2_L1C &&
        !SENTINEL2GetResolutionSet(psProductInfo,
                                   oSetResolutions,
                                   oMapResolutionsToBands) )
    {
        CPLDebug("SENTINEL2", "Failed to get resolution set");
        return nullptr;
    }

    std::vector<CPLString> aosGranuleList;
    if( bIsSafeCompact )
    {
        std::vector<L1CSafeCompatGranuleDescription> aoL1CSafeCompactGranuleList;
        if( eLevel == SENTINEL2_L1C &&
            !SENTINEL2GetGranuleList_L1CSafeCompact(psRoot, pszFilename,
                                                    aoL1CSafeCompactGranuleList) )
        {
            CPLDebug("SENTINEL2", "Failed to get granule list");
            return nullptr;
        }
        else if ( eLevel == SENTINEL2_L2A &&
            !SENTINEL2GetGranuleList_L2ASafeCompact(psRoot, pszFilename,
                                                    aoL1CSafeCompactGranuleList) )
        {
            CPLDebug("SENTINEL2", "Failed to get granule list");
            return nullptr;
        }
        for(size_t i=0;i<aoL1CSafeCompactGranuleList.size();++i)
        {
            aosGranuleList.push_back(
                aoL1CSafeCompactGranuleList[i].osMTDTLPath);
        }
    }
    else if( !SENTINEL2GetGranuleList(psRoot,
                                 eLevel,
                                 pszFilename,
                                 aosGranuleList,
                                 (eLevel == SENTINEL2_L1C) ? nullptr :
                                                    &oSetResolutions,
                                 (eLevel == SENTINEL2_L1C) ? nullptr :
                                                    &oMapResolutionsToBands) )
    {
        CPLDebug("SENTINEL2", "Failed to get granule list");
        return nullptr;
    }
    if( oSetResolutions.empty() )
    {
        CPLDebug("SENTINEL2", "Resolution set is empty");
        return nullptr;
    }

    std::set<int> oSetEPSGCodes;
    for(size_t i=0;i<aosGranuleList.size();i++)
    {
        int nEPSGCode = 0;
        if( SENTINEL2GetGranuleInfo(eLevel,
                                    aosGranuleList[i],
                                    *(oSetResolutions.begin()), &nEPSGCode) )
        {
            oSetEPSGCodes.insert(nEPSGCode);
        }
    }

    SENTINEL2DatasetContainer* poDS = new SENTINEL2DatasetContainer();
    char** papszMD = SENTINEL2GetUserProductMetadata(psRoot,
        (eLevel == SENTINEL2_L1C ) ? "Level-1C_User_Product" :
                                     "Level-2A_User_Product");
    poDS->GDALDataset::SetMetadata(papszMD);
    CSLDestroy(papszMD);

    if( !osOriginalXML.empty() )
    {
        char* apszXMLMD[2];
        apszXMLMD[0] = const_cast<char*>(osOriginalXML.c_str());
        apszXMLMD[1] = nullptr;
        poDS->GDALDataset::SetMetadata(apszXMLMD, "xml:SENTINEL2");
    }

    const char* pszPrefix = (eLevel == SENTINEL2_L1C) ? "SENTINEL2_L1C" :
                                                        "SENTINEL2_L2A";

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
                CPLSPrintf("%s:%s:%dm:EPSG_%d",
                            pszPrefix, pszFilename, nResolution, nEPSGCode),
                "SUBDATASETS");

            CPLString osBandNames = SENTINEL2GetBandListForResolution(
                                            oMapResolutionsToBands[nResolution]);

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

    /* Expose TCI or PREVIEW subdatasets */
    if( bIsSafeCompact )
    {
        for(std::set<int>::const_iterator oIterEPSG = oSetEPSGCodes.begin();
                                        oIterEPSG != oSetEPSGCodes.end();
                                    ++oIterEPSG )
        {
            const int nEPSGCode = *oIterEPSG;
            poDS->GDALDataset::SetMetadataItem(
                CPLSPrintf("SUBDATASET_%d_NAME", iSubDSNum),
                CPLSPrintf("%s:%s:TCI:EPSG_%d",
                            pszPrefix, pszFilename, nEPSGCode),
                "SUBDATASETS");

            CPLString osDesc("True color image");
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
    else
    {
        for(std::set<int>::const_iterator oIterEPSG = oSetEPSGCodes.begin();
                                        oIterEPSG != oSetEPSGCodes.end();
                                    ++oIterEPSG )
        {
            const int nEPSGCode = *oIterEPSG;
            poDS->GDALDataset::SetMetadataItem(
                CPLSPrintf("SUBDATASET_%d_NAME", iSubDSNum),
                CPLSPrintf("%s:%s:PREVIEW:EPSG_%d",
                            pszPrefix, pszFilename, nEPSGCode),
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
    }

    pszNodePath = (eLevel == SENTINEL2_L1C ) ?
        "=Level-1C_User_Product.Geometric_Info.Product_Footprint."
        "Product_Footprint.Global_Footprint.EXT_POS_LIST" :
        "=Level-2A_User_Product.Geometric_Info.Product_Footprint."
        "Product_Footprint.Global_Footprint.EXT_POS_LIST";
    const char* pszPosList = CPLGetXMLValue(psRoot, pszNodePath, nullptr);
    if( pszPosList != nullptr )
    {
        CPLString osPolygon = SENTINEL2GetPolygonWKTFromPosList(pszPosList);
        if( !osPolygon.empty() )
            poDS->GDALDataset::SetMetadataItem("FOOTPRINT", osPolygon.c_str());
    }

    return poDS;
}

/************************************************************************/
/*                    SENTINEL2GetL1BCTileMetadata()                    */
/************************************************************************/

static
char** SENTINEL2GetL1BCTileMetadata( CPLXMLNode* psMainMTD )
{
    CPLStringList aosList;

    CPLXMLNode* psRoot =  CPLGetXMLNode(psMainMTD,
                                        "=Level-1C_Tile_ID");
    if( psRoot == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find =Level-1C_Tile_ID");
        return nullptr;
    }
    CPLXMLNode* psGeneralInfo = CPLGetXMLNode(psRoot,
                                              "General_Info");
    for( CPLXMLNode* psIter = (psGeneralInfo ? psGeneralInfo->psChild : nullptr);
                     psIter != nullptr;
                     psIter = psIter->psNext )
    {
        if( psIter->eType != CXT_Element )
            continue;
        const char* pszValue = CPLGetXMLValue(psIter, nullptr, nullptr);
        if( pszValue != nullptr )
        {
            aosList.AddNameValue( psIter->pszValue, pszValue );
        }
    }

    CPLXMLNode* psQII = CPLGetXMLNode(psRoot, "Quality_Indicators_Info");
    if( psQII != nullptr )
    {
        CPLXMLNode* psICCQI = CPLGetXMLNode(psQII, "Image_Content_QI");
        for( CPLXMLNode* psIter = (psICCQI ? psICCQI->psChild : nullptr);
                     psIter != nullptr;
                     psIter = psIter->psNext )
        {
            if( psIter->eType != CXT_Element )
                continue;
            if( psIter->psChild != nullptr && psIter->psChild->eType == CXT_Text )
            {
                aosList.AddNameValue( psIter->pszValue,
                                    psIter->psChild->pszValue );
            }
        }
    }

    return aosList.StealList();
}

/************************************************************************/
/*                              OpenL1CTile()                           */
/************************************************************************/

GDALDataset *SENTINEL2Dataset::OpenL1CTile( const char* pszFilename,
                                            CPLXMLNode** ppsRootMainMTD,
                                            int nResolutionOfInterest,
                                            std::set<CPLString>* poBandSet )
{
    CPLXMLNode *psRoot = CPLParseXMLFile( pszFilename );
    if( psRoot == nullptr )
    {
        CPLDebug("SENTINEL2", "Cannot XML parse %s", pszFilename);
        return nullptr;
    }

    char* pszOriginalXML = CPLSerializeXMLTree(psRoot);
    CPLString osOriginalXML;
    if( pszOriginalXML )
        osOriginalXML = pszOriginalXML;
    CPLFree(pszOriginalXML);

    SENTINEL2_CPLXMLNodeHolder oXMLHolder(psRoot);
    CPLStripXMLNamespace(psRoot, nullptr, TRUE);

    std::set<int> oSetResolutions;
    std::map<int, std::set<CPLString> > oMapResolutionsToBands;
    char** papszMD = nullptr;
    SENTINEL2GetResolutionSetAndMainMDFromGranule(pszFilename,
                                                  "Level-1C_User_Product",
                                                  nResolutionOfInterest,
                                                  oSetResolutions,
                                                  oMapResolutionsToBands,
                                                  papszMD,
                                                  ppsRootMainMTD);
    if( poBandSet != nullptr )
        *poBandSet = oMapResolutionsToBands[nResolutionOfInterest];

    SENTINEL2DatasetContainer* poDS = new SENTINEL2DatasetContainer();

    char** papszGranuleMD = SENTINEL2GetL1BCTileMetadata(psRoot);
    papszMD = CSLMerge(papszMD, papszGranuleMD);
    CSLDestroy(papszGranuleMD);

    // Remove CLOUD_COVERAGE_ASSESSMENT that comes from main metadata, if granule
    // CLOUDY_PIXEL_PERCENTAGE is present.
    if( CSLFetchNameValue(papszMD, "CLOUDY_PIXEL_PERCENTAGE") != nullptr &&
        CSLFetchNameValue(papszMD, "CLOUD_COVERAGE_ASSESSMENT") != nullptr )
    {
        papszMD = CSLSetNameValue(papszMD, "CLOUD_COVERAGE_ASSESSMENT", nullptr);
    }

    poDS->GDALDataset::SetMetadata(papszMD);
    CSLDestroy(papszMD);

    if( !osOriginalXML.empty() )
    {
        char* apszXMLMD[2];
        apszXMLMD[0] = const_cast<char*>(osOriginalXML.c_str());
        apszXMLMD[1] = nullptr;
        poDS->GDALDataset::SetMetadata(apszXMLMD, "xml:SENTINEL2");
    }

    /* Create subdatsets per resolution (10, 20, 60m) */
    int iSubDSNum = 1;
    for(std::set<int>::const_iterator oIterRes = oSetResolutions.begin();
                                oIterRes != oSetResolutions.end();
                              ++oIterRes )
    {
        const int nResolution = *oIterRes;

        poDS->GDALDataset::SetMetadataItem(
            CPLSPrintf("SUBDATASET_%d_NAME", iSubDSNum),
            CPLSPrintf("%s:%s:%dm",
                        "SENTINEL2_L1C_TILE", pszFilename, nResolution),
                        "SUBDATASETS");

        CPLString osBandNames = SENTINEL2GetBandListForResolution(
                                        oMapResolutionsToBands[nResolution]);

        CPLString osDesc(CPLSPrintf("Bands %s with %dm resolution",
                                    osBandNames.c_str(), nResolution));
        poDS->GDALDataset::SetMetadataItem(
            CPLSPrintf("SUBDATASET_%d_DESC", iSubDSNum),
            osDesc.c_str(),
            "SUBDATASETS");

        iSubDSNum ++;
    }

    /* Expose PREVIEW subdataset */
    poDS->GDALDataset::SetMetadataItem(
        CPLSPrintf("SUBDATASET_%d_NAME", iSubDSNum),
        CPLSPrintf("%s:%s:PREVIEW",
                    "SENTINEL2_L1C_TILE", pszFilename),
        "SUBDATASETS");

    CPLString osDesc("RGB preview");
    poDS->GDALDataset::SetMetadataItem(
        CPLSPrintf("SUBDATASET_%d_DESC", iSubDSNum),
        osDesc.c_str(),
        "SUBDATASETS");

    return poDS;
}

/************************************************************************/
/*                         SENTINEL2GetOption()                         */
/************************************************************************/

static
const char* SENTINEL2GetOption( GDALOpenInfo* poOpenInfo,
                                const char* pszName,
                                const char* pszDefaultVal )
{
#ifdef GDAL_DMD_OPENOPTIONLIST
    const char* pszVal = CSLFetchNameValue(poOpenInfo->papszOpenOptions, pszName);
    if( pszVal != nullptr )
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
/*                       SENTINEL2GetTileInfo()                         */
/************************************************************************/

static bool SENTINEL2GetTileInfo(const char* pszFilename,
                                int* pnWidth, int* pnHeight, int *pnBits)
{
    static const unsigned char jp2_box_jp[] = {0x6a,0x50,0x20,0x20}; /* 'jP  ' */
    VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
    if( fp == nullptr )
        return false;
    GByte abyHeader[8];
    if( VSIFReadL(abyHeader, 8, 1, fp) != 1 )
    {
        VSIFCloseL(fp);
        return false;
    }
    if( memcmp(abyHeader + 4, jp2_box_jp, 4) == 0 )
    {
        bool bRet = false;
        /* Just parse the ihdr box instead of doing a full dataset opening */
        GDALJP2Box oBox( fp );
        if( oBox.ReadFirst() )
        {
            while( strlen(oBox.GetType()) > 0 )
            {
                if( EQUAL(oBox.GetType(),"jp2h") )
                {
                    GDALJP2Box oChildBox( fp );
                    if( !oChildBox.ReadFirstChild( &oBox ) )
                        break;

                    while( strlen(oChildBox.GetType()) > 0 )
                    {
                        if( EQUAL(oChildBox.GetType(),"ihdr") )
                        {
                            GByte* pabyData = oChildBox.ReadBoxData();
                            GIntBig nLength = oChildBox.GetDataLength();
                            if( pabyData != nullptr && nLength >= 4 + 4 + 2 + 1 )
                            {
                                bRet = true;
                                if( pnHeight )
                                {
                                    memcpy(pnHeight, pabyData, 4);
                                    CPL_MSBPTR32(pnHeight);
                                }
                                if( pnWidth != nullptr )
                                {
                                    //cppcheck-suppress nullPointer
                                    memcpy(pnWidth, pabyData+4, 4);
                                    CPL_MSBPTR32(pnWidth);
                                }
                                if( pnBits )
                                {
                                    GByte byPBC = pabyData[4+4+2];
                                    if( byPBC != 255 )
                                    {
                                        *pnBits = 1 + (byPBC & 0x7f);
                                    }
                                    else
                                        *pnBits = 0;
                                }
                            }
                            CPLFree(pabyData);
                            break;
                        }
                        if( !oChildBox.ReadNextChild( &oBox ) )
                            break;
                    }
                    break;
                }

                if (!oBox.ReadNext())
                    break;
            }
        }
        VSIFCloseL(fp);
        return bRet;
    }
    else /* for unit tests, we use TIFF */
    {
        VSIFCloseL(fp);
        GDALDataset* poDS = (GDALDataset*)GDALOpen(pszFilename, GA_ReadOnly);
        bool bRet = false;
        if( poDS != nullptr )
        {
            if( poDS->GetRasterCount() != 0 )
            {
                bRet = true;
                if( pnWidth )
                    *pnWidth = poDS->GetRasterXSize();
                if( pnHeight )
                    *pnHeight = poDS->GetRasterYSize();
                if( pnBits )
                {
                    const char* pszNBits = poDS->GetRasterBand(1)->GetMetadataItem(
                                                            "NBITS", "IMAGE_STRUCTURE");
                    if( pszNBits == nullptr )
                    {
                        GDALDataType eDT = poDS->GetRasterBand(1)->GetRasterDataType();
                        pszNBits = CPLSPrintf( "%d", GDALGetDataTypeSize(eDT) );
                    }
                    *pnBits = atoi(pszNBits);
                }
            }
            GDALClose(poDS);
        }
        return bRet;
    }
}

/************************************************************************/
/*                         OpenL1C_L2ASubdataset()                      */
/************************************************************************/

GDALDataset *SENTINEL2Dataset::OpenL1C_L2ASubdataset( GDALOpenInfo * poOpenInfo,
                                                      SENTINEL2Level eLevel )
{
    CPLString osFilename;
    const char* pszPrefix = (eLevel == SENTINEL2_L1C) ? "SENTINEL2_L1C" :
                                                        "SENTINEL2_L2A";
    CPLAssert( STARTS_WITH_CI(poOpenInfo->pszFilename, pszPrefix) );
    osFilename = poOpenInfo->pszFilename + strlen(pszPrefix) + 1;
    const char* pszEPSGCode = strrchr(osFilename.c_str(), ':');
    if( pszEPSGCode == nullptr || pszEPSGCode == osFilename.c_str() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid syntax for %s:", pszPrefix);
        return nullptr;
    }
    osFilename[ (size_t)(pszEPSGCode - osFilename.c_str()) ] = '\0';
    const char* pszPrecision = strrchr(osFilename.c_str(), ':');
    if( pszPrecision == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid syntax for %s:", pszPrefix);
        return nullptr;
    }

    if( !STARTS_WITH_CI(pszEPSGCode+1, "EPSG_") )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid syntax for %s:", pszPrefix);
        return nullptr;
    }

    const int nSubDSEPSGCode = atoi(pszEPSGCode + 1 + strlen("EPSG_"));
    const bool bIsPreview = STARTS_WITH_CI(pszPrecision + 1, "PREVIEW");
    const bool bIsTCI = STARTS_WITH_CI(pszPrecision + 1, "TCI");
    const int nSubDSPrecision = (bIsPreview) ? 320 : (bIsTCI) ? 10 : atoi(pszPrecision + 1);
    if( !bIsTCI && !bIsPreview &&
        nSubDSPrecision != 10 && nSubDSPrecision != 20 && nSubDSPrecision != 60 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported precision: %d",
                 nSubDSPrecision);
        return nullptr;
    }

    osFilename.resize( pszPrecision - osFilename.c_str() );
    std::vector<CPLString> aosNonJP2Files;
    aosNonJP2Files.push_back(osFilename);

    CPLXMLNode *psRoot = CPLParseXMLFile( osFilename );
    if( psRoot == nullptr )
    {
        CPLDebug("SENTINEL2", "Cannot XML parse %s", osFilename.c_str());
        return nullptr;
    }

    char* pszOriginalXML = CPLSerializeXMLTree(psRoot);
    CPLString osOriginalXML;
    if( pszOriginalXML )
        osOriginalXML = pszOriginalXML;
    CPLFree(pszOriginalXML);

    SENTINEL2_CPLXMLNodeHolder oXMLHolder(psRoot);
    CPLStripXMLNamespace(psRoot, nullptr, TRUE);

    CPLXMLNode* psProductInfo = eLevel == SENTINEL2_L1C ?
        CPLGetXMLNode(psRoot, "=Level-1C_User_Product.General_Info.Product_Info") :
        CPLGetXMLNode(psRoot, "=Level-2A_User_Product.General_Info.Product_Info");
    if( psProductInfo == nullptr && eLevel == SENTINEL2_L2A )
    {
        psProductInfo = CPLGetXMLNode(psRoot, "=Level-2A_User_Product.General_Info.L2A_Product_Info");
    }
    if( psProductInfo == nullptr )
    {
        CPLDebug("SENTINEL2", "Product Info not found");
        return nullptr;
    }

    const bool bIsSafeCompact = EQUAL(CPLGetXMLValue(psProductInfo,
                                    "Query_Options.PRODUCT_FORMAT", ""),
                                    "SAFE_COMPACT");

    const char* pszProductURI = CPLGetXMLValue(psProductInfo,
                                    "PRODUCT_URI", nullptr);
    SENTINEL2ProductType pType = MSI2A;
    if( pszProductURI == nullptr )
    {
        pszProductURI = CPLGetXMLValue(psProductInfo,
                            "PRODUCT_URI_2A", nullptr);
        pType = MSI2Ap;
    }
    if( pszProductURI == nullptr )
        pszProductURI = "";

    std::vector<CPLString> aosGranuleList;
    std::map<int, std::set<CPLString> > oMapResolutionsToBands;
    std::vector<L1CSafeCompatGranuleDescription> aoL1CSafeCompactGranuleList;
    if( bIsSafeCompact )
    {
        for(unsigned int i = 0; i < NB_BANDS; ++i)
        {
            // L2 does not contain B10
            if( i == 10 && eLevel == SENTINEL2_L2A )
                continue;
            const SENTINEL2BandDescription * psBandDesc = &asBandDesc[i];
            CPLString osName = psBandDesc->pszBandName + 1; /* skip B character */
            if( atoi(osName) < 10 )
                osName = "0" + osName;
            oMapResolutionsToBands[psBandDesc->nResolution].insert(osName);
        }
        if (eLevel == SENTINEL2_L2A )
        {
            for( const auto& sL2ABandDesc: asL2ABandDesc)
            {
                oMapResolutionsToBands[sL2ABandDesc.nResolution].insert(sL2ABandDesc.pszBandName);
            }
        }
        if( eLevel == SENTINEL2_L1C &&
            !SENTINEL2GetGranuleList_L1CSafeCompact(psRoot, osFilename,
                                                    aoL1CSafeCompactGranuleList) )
        {
            CPLDebug("SENTINEL2", "Failed to get granule list");
            return nullptr;
        }
        if( eLevel == SENTINEL2_L2A &&
            !SENTINEL2GetGranuleList_L2ASafeCompact(psRoot, osFilename,
                                                    aoL1CSafeCompactGranuleList) )
        {
            CPLDebug("SENTINEL2", "Failed to get granule list");
            return nullptr;
        }
        for(size_t i=0;i<aoL1CSafeCompactGranuleList.size();++i)
        {
            aosGranuleList.push_back(
                aoL1CSafeCompactGranuleList[i].osMTDTLPath);
        }
    }
    else if( !SENTINEL2GetGranuleList(psRoot,
                                 eLevel,
                                 osFilename,
                                 aosGranuleList,
                                 nullptr,
                                 (eLevel == SENTINEL2_L1C) ? nullptr :
                                                    &oMapResolutionsToBands) )
    {
        CPLDebug("SENTINEL2", "Failed to get granule list");
        return nullptr;
    }

    std::vector<CPLString> aosBands;
    std::set<CPLString> oSetBands;
    if( bIsPreview || bIsTCI )
    {
        aosBands.push_back("04");
        aosBands.push_back("03");
        aosBands.push_back("02");
    }
    else if( eLevel == SENTINEL2_L1C && !bIsSafeCompact )
    {
        CPLXMLNode* psBandList = CPLGetXMLNode(psRoot,
            "=Level-1C_User_Product.General_Info.Product_Info.Query_Options.Band_List");
        if( psBandList == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s",
                    "Query_Options.Band_List");
            return nullptr;
        }

        for(CPLXMLNode* psIter = psBandList->psChild; psIter != nullptr;
                                                      psIter = psIter->psNext )
        {
            if( psIter->eType != CXT_Element ||
                !EQUAL(psIter->pszValue, "BAND_NAME") )
                continue;
            const char* pszBandName = CPLGetXMLValue(psIter, nullptr, "");
            const SENTINEL2BandDescription* psBandDesc =
                                SENTINEL2GetBandDesc(pszBandName);
            if( psBandDesc == nullptr )
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
        if( oSetBands.empty() )
        {
            CPLDebug("SENTINEL2", "Band set is empty");
            return nullptr;
        }
    }
    else
    {
        oSetBands = oMapResolutionsToBands[nSubDSPrecision];
    }

    if( aosBands.empty() )
    {
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

    char** papszMD = SENTINEL2GetUserProductMetadata(psRoot,
        (eLevel == SENTINEL2_L1C ) ? "Level-1C_User_Product" : "Level-2A_User_Product");

    const int nSaturatedVal = atoi(CSLFetchNameValueDef(papszMD,
                                                  "SPECIAL_VALUE_SATURATED", "-1"));
    const int nNodataVal = atoi(CSLFetchNameValueDef(papszMD,
                                               "SPECIAL_VALUE_NODATA", "-1"));

    const bool bAlpha =
        CPLTestBool(SENTINEL2GetOption(poOpenInfo, "ALPHA", "FALSE"));

    SENTINEL2Dataset* poDS = CreateL1CL2ADataset(eLevel,
                                                 pType,
                                                 bIsSafeCompact,
                                                 aosGranuleList,
                                                 aoL1CSafeCompactGranuleList,
                                                 aosNonJP2Files,
                                                 nSubDSPrecision,
                                                 bIsPreview,
                                                 bIsTCI,
                                                 nSubDSEPSGCode,
                                                 bAlpha,
                                                 aosBands,
                                                 nSaturatedVal,
                                                 nNodataVal,
                                                 CPLString(pszProductURI));
    if( poDS == nullptr )
    {
        CSLDestroy(papszMD);
        return nullptr;
    }

    if( !osOriginalXML.empty() )
    {
        char* apszXMLMD[2] = { nullptr };
        apszXMLMD[0] = const_cast<char*>(osOriginalXML.c_str());
        apszXMLMD[1] = nullptr;
        poDS->GDALDataset::SetMetadata(apszXMLMD, "xml:SENTINEL2");
    }

    poDS->GDALDataset::SetMetadata(papszMD);
    CSLDestroy(papszMD);

/* -------------------------------------------------------------------- */
/*      Add extra band metadata.                                        */
/* -------------------------------------------------------------------- */
    poDS->AddL1CL2ABandMetadata(eLevel, psRoot, aosBands);

/* -------------------------------------------------------------------- */
/*      Initialize overview information.                                */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    CPLString osOverviewFile;
    if( bIsPreview )
        osOverviewFile = CPLSPrintf("%s_PREVIEW_EPSG_%d.tif.ovr",
                                    osFilename.c_str(), nSubDSEPSGCode);
    else if( bIsTCI )
        osOverviewFile = CPLSPrintf("%s_TCI_EPSG_%d.tif.ovr",
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
/*                         AddL1CL2ABandMetadata()                      */
/************************************************************************/

void SENTINEL2Dataset::AddL1CL2ABandMetadata(SENTINEL2Level eLevel,
                                             CPLXMLNode* psRoot,
                                             const std::vector<CPLString>& aosBands)
{
    CPLXMLNode* psIC = CPLGetXMLNode(psRoot,
        (eLevel == SENTINEL2_L1C) ?
            "=Level-1C_User_Product.General_Info.Product_Image_Characteristics" :
            "=Level-2A_User_Product.General_Info.Product_Image_Characteristics");
    if( psIC == nullptr )
    {
        psIC = CPLGetXMLNode(psRoot, "=Level-2A_User_Product.General_Info.L2A_Product_Image_Characteristics");
    }
    if( psIC != nullptr )
    {
        CPLXMLNode* psSIL = CPLGetXMLNode(psIC,
                            "Reflectance_Conversion.Solar_Irradiance_List");
        if( psSIL != nullptr )
        {
            for(CPLXMLNode* psIter = psSIL->psChild; psIter != nullptr;
                                                     psIter = psIter->psNext )
            {
                if( psIter->eType != CXT_Element ||
                    !EQUAL(psIter->pszValue, "SOLAR_IRRADIANCE") )
                {
                    continue;
                }
                const char* pszBandId = CPLGetXMLValue(psIter, "bandId", nullptr);
                const char* pszUnit = CPLGetXMLValue(psIter, "unit", nullptr);
                const char* pszValue = CPLGetXMLValue(psIter, nullptr, nullptr);
                if( pszBandId != nullptr && pszUnit != nullptr && pszValue != nullptr )
                {
                    int nIdx = atoi(pszBandId);
                    if( nIdx >= 0 && nIdx < (int)NB_BANDS )
                    {
                        for(int i=0;i<nBands;i++)
                        {
                            GDALRasterBand* poBand = GetRasterBand(i+1);
                            const char* pszBandName =
                                poBand->GetMetadataItem("BANDNAME");
                            if( pszBandName != nullptr &&
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
/*      Add Scene Classification category values (L2A)                  */
/* -------------------------------------------------------------------- */
    CPLXMLNode* psSCL = CPLGetXMLNode(psRoot,
            "=Level-2A_User_Product.General_Info."
            "Product_Image_Characteristics.Scene_Classification_List");
    if( psSCL == nullptr)
    {
        psSCL = CPLGetXMLNode(psRoot,
            "=Level-2A_User_Product.General_Info."
            "L2A_Product_Image_Characteristics.L2A_Scene_Classification_List");
    }
    int nSCLBand = 0;
    for(int nBand=1;nBand<=static_cast<int>(aosBands.size());nBand++)
    {
        if( EQUAL(aosBands[nBand-1], "SCL") )
        {
            nSCLBand = nBand;
            break;
        }
    }
    if( psSCL != nullptr && nSCLBand > 0 )
    {
        std::vector<CPLString> osCategories;
        for(CPLXMLNode* psIter = psSCL->psChild; psIter != nullptr;
                                                     psIter = psIter->psNext )
        {
            if( psIter->eType != CXT_Element ||
                (!EQUAL(psIter->pszValue, "L2A_Scene_Classification_ID") &&
                 !EQUAL(psIter->pszValue, "Scene_Classification_ID") ) )
            {
                continue;
            }
            const char* pszText = CPLGetXMLValue(psIter,
                                        "SCENE_CLASSIFICATION_TEXT", nullptr);
            if( pszText == nullptr)
                pszText = CPLGetXMLValue(psIter,
                                        "L2A_SCENE_CLASSIFICATION_TEXT", nullptr);
            const char* pszIdx = CPLGetXMLValue(psIter,
                                        "SCENE_CLASSIFICATION_INDEX", nullptr);
            if( pszIdx == nullptr )
                pszIdx = CPLGetXMLValue(psIter,
                                        "L2A_SCENE_CLASSIFICATION_INDEX", nullptr);
            if( pszText && pszIdx && atoi(pszIdx) >= 0 && atoi(pszIdx) < 100 )
            {
                int nIdx = atoi(pszIdx);
                if( nIdx >= (int)osCategories.size() )
                    osCategories.resize(nIdx + 1);
                if( STARTS_WITH_CI(pszText, "SC_") )
                    osCategories[nIdx] = pszText + 3;
                else
                    osCategories[nIdx] = pszText;
            }
        }
        char** papszCategories =
                    (char**)CPLCalloc(osCategories.size()+1,sizeof(char*));
        for(size_t i=0;i<osCategories.size();i++)
            papszCategories[i] = CPLStrdup(osCategories[i]);
        GetRasterBand(nSCLBand)->SetCategoryNames(papszCategories);
        CSLDestroy(papszCategories);
    }
}

/************************************************************************/
/*                         CreateL1CL2ADataset()                        */
/************************************************************************/

SENTINEL2Dataset* SENTINEL2Dataset::CreateL1CL2ADataset(
                SENTINEL2Level eLevel,
                SENTINEL2ProductType pType,
                bool bIsSafeCompact,
                const std::vector<CPLString>& aosGranuleList,
                const std::vector<L1CSafeCompatGranuleDescription>& aoL1CSafeCompactGranuleList,
                std::vector<CPLString>& aosNonJP2Files,
                int nSubDSPrecision,
                bool bIsPreview,
                bool bIsTCI,
                int nSubDSEPSGCode, /* or -1 if not known at this point */
                bool bAlpha,
                const std::vector<CPLString>& aosBands,
                int nSaturatedVal,
                int nNodataVal,
                const CPLString& osProductURI)
{

    /* Iterate over granule metadata to know the layer extent */
    /* and the location of each granule */
    double dfMinX = 1.0e20;
    double dfMinY = 1.0e20;
    double dfMaxX = -1.0e20;
    double dfMaxY = -1.0e20;
    std::vector<SENTINEL2GranuleInfo> aosGranuleInfoList;
    const int nDesiredResolution = (bIsPreview || bIsTCI) ? 0 : nSubDSPrecision;

    if( bIsSafeCompact )
    {
        CPLAssert( aosGranuleList.size() ==
                   aoL1CSafeCompactGranuleList.size() );
    }

    for(size_t i=0;i<aosGranuleList.size();i++)
    {
        int nEPSGCode = 0;
        double dfULX = 0.0;
        double dfULY = 0.0;
        int nResolution = 0;
        int nWidth = 0;
        int nHeight = 0;
        if( SENTINEL2GetGranuleInfo(eLevel,
                                    aosGranuleList[i],
                                    nDesiredResolution,
                                    &nEPSGCode,
                                    &dfULX, &dfULY,
                                    &nResolution,
                                    &nWidth, &nHeight) &&
            (nSubDSEPSGCode == nEPSGCode || nSubDSEPSGCode < 0) &&
            nResolution != 0 )
        {
            nSubDSEPSGCode = nEPSGCode;
            aosNonJP2Files.push_back(aosGranuleList[i]);

            if( dfULX < dfMinX ) dfMinX = dfULX;
            if( dfULY > dfMaxY ) dfMaxY = dfULY;
            const double dfLRX = dfULX + nResolution * nWidth;
            const double dfLRY = dfULY - nResolution * nHeight;
            if( dfLRX > dfMaxX ) dfMaxX = dfLRX;
            if( dfLRY < dfMinY ) dfMinY = dfLRY;

            SENTINEL2GranuleInfo oGranuleInfo;
            oGranuleInfo.osPath = CPLGetPath(aosGranuleList[i]);
            if( bIsSafeCompact )
            {
                oGranuleInfo.osBandPrefixPath =
                            aoL1CSafeCompactGranuleList[i].osBandPrefixPath;
            }
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
        CPLError(CE_Failure, CPLE_NotSupported,
                 "No granule found for EPSG code %d",
                 nSubDSEPSGCode);
        return nullptr;
    }

    const int nRasterXSize = static_cast<int>
                                    ((dfMaxX - dfMinX) / nSubDSPrecision + 0.5);
    const int nRasterYSize = static_cast<int>
                                    ((dfMaxY - dfMinY) / nSubDSPrecision + 0.5);
    SENTINEL2Dataset* poDS = new SENTINEL2Dataset(nRasterXSize, nRasterYSize);

    poDS->aosNonJP2Files = aosNonJP2Files;

    OGRSpatialReference oSRS;
    char* pszProjection = nullptr;
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

    double adfGeoTransform[6] = { 0 };
    adfGeoTransform[0] = dfMinX;
    adfGeoTransform[1] = nSubDSPrecision;
    adfGeoTransform[2] = 0;
    adfGeoTransform[3] = dfMaxY;
    adfGeoTransform[4] = 0;
    adfGeoTransform[5] = -nSubDSPrecision;
    poDS->SetGeoTransform(adfGeoTransform);
    poDS->GDALDataset::SetMetadataItem("COMPRESSION", "JPEG2000", "IMAGE_STRUCTURE");
    if( bIsPreview || bIsTCI )
        poDS->GDALDataset::SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");

    int nBits = (bIsPreview || bIsTCI) ? 8 : 0 /* 0 = unknown yet*/;
    int nValMax = (bIsPreview || bIsTCI) ? 255 : 0 /* 0 = unknown yet*/;
    const int nBands = (bIsPreview || bIsTCI) ? 3 : ((bAlpha) ? 1 : 0) + static_cast<int>(aosBands.size());
    const int nAlphaBand = (bIsPreview || bIsTCI || !bAlpha) ? 0 : nBands;
    const GDALDataType eDT = (bIsPreview || bIsTCI) ? GDT_Byte: GDT_UInt16;

    for( int nBand = 1; nBand <= nBands; nBand++ )
    {
        VRTSourcedRasterBand* poBand = nullptr;

        if( nBand != nAlphaBand )
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
        if( nBand == nAlphaBand )
            poBand->SetColorInterpretation(GCI_AlphaBand);

        CPLString osBandName;
        if( nBand != nAlphaBand )
        {
            osBandName = aosBands[nBand-1];
            SENTINEL2SetBandMetadata( poBand, osBandName);
        }
        else
            osBandName = aosBands[0];

        for(size_t iSrc=0;iSrc<aosGranuleInfoList.size();iSrc++)
        {
            const SENTINEL2GranuleInfo& oGranuleInfo = aosGranuleInfoList[iSrc];
            CPLString osTile;

            if( bIsSafeCompact && eLevel != SENTINEL2_L2A )
            {
                if( bIsTCI )
                {
                    osTile = oGranuleInfo.osBandPrefixPath + "TCI.jp2";
                }
                else
                {
                    osTile = oGranuleInfo.osBandPrefixPath + "B";
                    if( osBandName.size() == 1 )
                        osTile += "0" + osBandName;
                    else if( osBandName.size() == 3 )
                        osTile += osBandName.substr(1);
                    else
                        osTile += osBandName;
                    osTile += ".jp2";
                }
            }
            else
            {
                osTile = SENTINEL2GetTilename(
                        oGranuleInfo.osPath,
                        CPLGetFilename(oGranuleInfo.osPath),
                        osBandName,
                        osProductURI,
                        bIsPreview,
                        (eLevel == SENTINEL2_L1C) ? 0 : nSubDSPrecision);
                if( bIsSafeCompact && eLevel == SENTINEL2_L2A &&
                    pType == MSI2Ap && osTile.size() >= 34 &&
                    osTile.substr(osTile.size()-18,3)!="MSK" )
                {
                    osTile.insert(osTile.size() - 34, "L2A_");
                }
                if( bIsTCI && osTile.size() >= 14 )
                {
                    osTile.replace(osTile.size() - 11, 3, "TCI");
                }
            }

            bool bTileFound = false;
            if( nValMax == 0 )
            {
                /* It is supposed to be 12 bits, but some products have 15 bits */
                if( SENTINEL2GetTileInfo(osTile, nullptr, nullptr, &nBits) )
                {
                    bTileFound = true;
                    if( nBits <= 16 )
                        nValMax = (1 << nBits) - 1;
                    else
                    {
                        CPLDebug("SENTINEL2", "Unexpected bit depth %d", nBits);
                        nValMax = 65535;
                    }
                }
            }
            else
            {
                VSIStatBufL sStat;
                if( VSIStatExL(osTile, &sStat, VSI_STAT_EXISTS_FLAG) == 0 )
                    bTileFound = true;
            }
            if( !bTileFound )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Tile %s not found on filesystem. Skipping it",
                        osTile.c_str());
                continue;
            }

            const int nDstXOff = static_cast<int>(
                    (oGranuleInfo.dfMinX - dfMinX) / nSubDSPrecision + 0.5);
            const int nDstYOff = static_cast<int>(
                    (dfMaxY - oGranuleInfo.dfMaxY) / nSubDSPrecision + 0.5);

            if( nBand != nAlphaBand )
            {
                poBand->AddSimpleSource( osTile, (bIsPreview || bIsTCI) ? nBand : 1,
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
                poBand->AddComplexSource( osTile, 1,
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
        }

        if( (nBits % 8) != 0 )
        {
            poBand->SetMetadataItem("NBITS",
                                CPLSPrintf("%d", nBits), "IMAGE_STRUCTURE");
        }
    }

    return poDS;
}

/************************************************************************/
/*                      OpenL1CTileSubdataset()                         */
/************************************************************************/

GDALDataset* SENTINEL2Dataset::OpenL1CTileSubdataset( GDALOpenInfo * poOpenInfo )
{
    CPLString osFilename;
    CPLAssert( STARTS_WITH_CI(poOpenInfo->pszFilename, "SENTINEL2_L1C_TILE:") );
    osFilename = poOpenInfo->pszFilename + strlen("SENTINEL2_L1C_TILE:");
    const char* pszPrecision = strrchr(osFilename.c_str(), ':');
    if( pszPrecision == nullptr || pszPrecision == osFilename.c_str() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid syntax for SENTINEL2_L1C_TILE:");
        return nullptr;
    }
    const bool bIsPreview = STARTS_WITH_CI(pszPrecision + 1, "PREVIEW");
    const int nSubDSPrecision = (bIsPreview) ? 320 : atoi(pszPrecision + 1);
    if( !bIsPreview && nSubDSPrecision != 10 && nSubDSPrecision != 20 && nSubDSPrecision != 60 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported precision: %d",
                 nSubDSPrecision);
        return nullptr;
    }
    osFilename.resize( pszPrecision - osFilename.c_str() );

    std::set<CPLString> oSetBands;
    CPLXMLNode* psRootMainMTD = nullptr;
    GDALDataset* poTmpDS = OpenL1CTile( osFilename, &psRootMainMTD, nSubDSPrecision, &oSetBands);
    SENTINEL2_CPLXMLNodeHolder oXMLHolder(psRootMainMTD);
    if( poTmpDS == nullptr )
        return nullptr;

    std::vector<CPLString> aosBands;
    if( bIsPreview )
    {
        aosBands.push_back("04");
        aosBands.push_back("03");
        aosBands.push_back("02");
    }
    else
    {
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

    std::vector<CPLString> aosGranuleList;
    aosGranuleList.push_back(osFilename);

    const int nSaturatedVal = atoi(CSLFetchNameValueDef(poTmpDS->GetMetadata(),
                                                  "SPECIAL_VALUE_SATURATED", "-1"));
    const int nNodataVal = atoi(CSLFetchNameValueDef(poTmpDS->GetMetadata(),
                                               "SPECIAL_VALUE_NODATA", "-1"));

    const bool bAlpha =
        CPLTestBool(SENTINEL2GetOption(poOpenInfo, "ALPHA", "FALSE"));

    std::vector<CPLString> aosNonJP2Files;
    SENTINEL2Dataset* poDS = CreateL1CL2ADataset(SENTINEL2_L1C,
                                                 MSI2A,
                                                 false, // bIsSafeCompact
                                                 aosGranuleList,
                                                 std::vector<L1CSafeCompatGranuleDescription>(),
                                                 aosNonJP2Files,
                                                 nSubDSPrecision,
                                                 bIsPreview,
                                                 false, // bIsTCI
                                                 -1 /*nSubDSEPSGCode*/,
                                                 bAlpha,
                                                 aosBands,
                                                 nSaturatedVal,
                                                 nNodataVal,
                                                 CPLString());
    if( poDS == nullptr )
    {
        delete poTmpDS;
        return nullptr;
    }

    // Transfer metadata
    poDS->GDALDataset::SetMetadata( poTmpDS->GetMetadata() );
    poDS->GDALDataset::SetMetadata( poTmpDS->GetMetadata("xml:SENTINEL2"), "xml:SENTINEL2" );

    delete poTmpDS;

/* -------------------------------------------------------------------- */
/*      Add extra band metadata.                                        */
/* -------------------------------------------------------------------- */
    if( psRootMainMTD != nullptr )
        poDS->AddL1CL2ABandMetadata(SENTINEL2_L1C, psRootMainMTD, aosBands);

/* -------------------------------------------------------------------- */
/*      Initialize overview information.                                */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    CPLString osOverviewFile;
    if( bIsPreview )
        osOverviewFile = CPLSPrintf("%s_PREVIEW.tif.ovr",
                                    osFilename.c_str());
    else
        osOverviewFile = CPLSPrintf("%s_%dm.tif.ovr",
                                    osFilename.c_str(), nSubDSPrecision);
    poDS->SetMetadataItem( "OVERVIEW_FILE", osOverviewFile, "OVERVIEWS" );
    poDS->oOvManager.Initialize( poDS, ":::VIRTUAL:::" );

    return poDS;
}

/************************************************************************/
/*                      GDALRegister_SENTINEL2()                        */
/************************************************************************/

void GDALRegister_SENTINEL2()
{
    if( GDALGetDriverByName( "SENTINEL2" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "SENTINEL2" );
#ifdef GDAL_DCAP_RASTER
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
#endif
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Sentinel 2" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/sentinel2.html" );
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
