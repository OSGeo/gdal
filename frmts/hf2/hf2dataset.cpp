/******************************************************************************
 *
 * Project:  HF2 driver
 * Purpose:  GDALDataset driver for HF2/HFZ dataset.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2010-2012, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "ogr_spatialref.h"

#include <cstdlib>
#include <cmath>

#include <algorithm>
#include <limits>

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                              HF2Dataset                              */
/* ==================================================================== */
/************************************************************************/

class HF2RasterBand;

class HF2Dataset final: public GDALPamDataset
{
    friend class HF2RasterBand;

    VSILFILE   *fp;
    double      adfGeoTransform[6];
    char       *pszWKT;
    vsi_l_offset    *panBlockOffset; // tile 0 is a the bottom left

    int         nTileSize;
    int         bHasLoaderBlockMap;
    int         LoadBlockMap();

  public:
                 HF2Dataset();
    virtual     ~HF2Dataset();

    virtual CPLErr GetGeoTransform( double * ) override;
    virtual const char* _GetProjectionRef() override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
    static GDALDataset *CreateCopy( const char * pszFilename, GDALDataset *poSrcDS,
                                    int bStrict, char ** papszOptions,
                                    GDALProgressFunc pfnProgress, void * pProgressData );
};

/************************************************************************/
/* ==================================================================== */
/*                            HF2RasterBand                             */
/* ==================================================================== */
/************************************************************************/

class HF2RasterBand final: public GDALPamRasterBand
{
    friend class HF2Dataset;

    float*  pafBlockData;
    int     nLastBlockYOffFromBottom;

  public:

                HF2RasterBand( HF2Dataset *, int, GDALDataType );
    virtual ~HF2RasterBand();

    virtual CPLErr IReadBlock( int, int, void * ) override;
};

/************************************************************************/
/*                           HF2RasterBand()                            */
/************************************************************************/

HF2RasterBand::HF2RasterBand( HF2Dataset *poDSIn, int nBandIn, GDALDataType eDT ) :
    pafBlockData(nullptr),
    nLastBlockYOffFromBottom(-1)
{
    poDS = poDSIn;
    nBand = nBandIn;

    eDataType = eDT;

    nBlockXSize = poDSIn->nTileSize;
    nBlockYSize = 1;
}

/************************************************************************/
/*                          ~HF2RasterBand()                            */
/************************************************************************/

HF2RasterBand::~HF2RasterBand()
{
    CPLFree(pafBlockData);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr HF2RasterBand::IReadBlock( int nBlockXOff, int nLineYOff,
                                  void * pImage )

{
    HF2Dataset *poGDS = (HF2Dataset *) poDS;

    const int nXBlocks = DIV_ROUND_UP(nRasterXSize, poGDS->nTileSize);

    if (!poGDS->LoadBlockMap())
        return CE_Failure;

    const int nMaxTileHeight= std::min(poGDS->nTileSize, nRasterYSize);
    if (pafBlockData == nullptr)
    {
        if( nMaxTileHeight > 10*1024*1024 / nRasterXSize )
        {
            VSIFSeekL( poGDS->fp, 0, SEEK_END );
            vsi_l_offset nSize = VSIFTellL(poGDS->fp);
            if( nSize < static_cast<vsi_l_offset>(nMaxTileHeight) * nRasterXSize )
            {
                CPLError(CE_Failure, CPLE_FileIO, "File too short");
                return CE_Failure;
            }
        }
        pafBlockData = (float*)VSIMalloc3(sizeof(float), nRasterXSize, nMaxTileHeight);
        if (pafBlockData == nullptr)
            return CE_Failure;
    }

    const int nLineYOffFromBottom = nRasterYSize - 1 - nLineYOff;

    const int nBlockYOffFromBottom = nLineYOffFromBottom / nBlockXSize;
    const int nYOffInTile = nLineYOffFromBottom % nBlockXSize;

    if (nBlockYOffFromBottom != nLastBlockYOffFromBottom)
    {
        nLastBlockYOffFromBottom = nBlockYOffFromBottom;

        memset(pafBlockData, 0, sizeof(float) * nRasterXSize * nMaxTileHeight);

        /* 4 * nBlockXSize is the upper bound */
        void* pabyData = CPLMalloc( 4 * nBlockXSize );

        for(int nxoff = 0; nxoff < nXBlocks; nxoff++)
        {
            VSIFSeekL(poGDS->fp, poGDS->panBlockOffset[nBlockYOffFromBottom * nXBlocks + nxoff], SEEK_SET);
            float fScale, fOff;
            VSIFReadL(&fScale, 4, 1, poGDS->fp);
            VSIFReadL(&fOff, 4, 1, poGDS->fp);
            CPL_LSBPTR32(&fScale);
            CPL_LSBPTR32(&fOff);

            const int nTileWidth =
                std::min(nBlockXSize, nRasterXSize - nxoff * nBlockXSize);
            const int nTileHeight =
                std::min(nBlockXSize, nRasterYSize - nBlockYOffFromBottom * nBlockXSize);

            for(int j=0;j<nTileHeight;j++)
            {
                GByte nWordSize;
                VSIFReadL(&nWordSize, 1, 1, poGDS->fp);
                if (nWordSize != 1 && nWordSize != 2 && nWordSize != 4)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Unexpected word size : %d", (int)nWordSize);
                    break;
                }

                GInt32 nVal;
                VSIFReadL(&nVal, 4, 1, poGDS->fp);
                CPL_LSBPTR32(&nVal);
                const size_t nToRead = static_cast<size_t>(nWordSize * (nTileWidth - 1));
                const size_t nRead = VSIFReadL(pabyData, 1, nToRead, poGDS->fp);
                if( nRead != nToRead )
                {
                    CPLError(CE_Failure, CPLE_FileIO,
                             "File too short: got %d, expected %d",
                             static_cast<int>(nRead),
                             static_cast<int>(nToRead));
                    CPLFree(pabyData);
                    return CE_Failure;
                }
#if defined(CPL_MSB)
                if (nWordSize > 1)
                    GDALSwapWords(pabyData, nWordSize, nTileWidth - 1, nWordSize);
#endif

                double dfVal = nVal * (double)fScale + fOff;
                if( dfVal > std::numeric_limits<float>::max() )
                    dfVal = std::numeric_limits<float>::max();
                else if( dfVal < std::numeric_limits<float>::min() )
                    dfVal = std::numeric_limits<float>::min();
                pafBlockData[nxoff * nBlockXSize + j * nRasterXSize + 0] = static_cast<float>(dfVal);
                for(int i=1;i<nTileWidth;i++)
                {
                    int nInc;
                    if (nWordSize == 1)
                        nInc = ((signed char*)pabyData)[i-1];
                    else if (nWordSize == 2)
                        nInc = ((GInt16*)pabyData)[i-1];
                    else
                        nInc = ((GInt32*)pabyData)[i-1];
                    if( (nInc >= 0 && nVal > INT_MAX - nInc) ||
                        (nInc == INT_MIN && nVal < 0) ||
                        (nInc < 0 && nVal < INT_MIN - nInc ) )
                    {
                        CPLError(CE_Failure, CPLE_FileIO, "int32 overflow");
                        CPLFree(pabyData);
                        return CE_Failure;
                    }
                    nVal += nInc;
                    dfVal = nVal * (double)fScale + fOff;
                    if( dfVal > std::numeric_limits<float>::max() )
                        dfVal = std::numeric_limits<float>::max();
                    else if( dfVal < std::numeric_limits<float>::min() )
                        dfVal = std::numeric_limits<float>::min();
                    pafBlockData[nxoff * nBlockXSize + j * nRasterXSize + i] = static_cast<float>(dfVal);
                }
            }
        }

        CPLFree(pabyData);
    }

    const int nTileWidth =
        std::min(nBlockXSize, nRasterXSize - nBlockXOff * nBlockXSize);
    memcpy(pImage, pafBlockData + nBlockXOff * nBlockXSize +
                                  nYOffInTile * nRasterXSize,
           nTileWidth * sizeof(float));

    return CE_None;
}

/************************************************************************/
/*                            ~HF2Dataset()                            */
/************************************************************************/

HF2Dataset::HF2Dataset() :
    fp(nullptr),
    pszWKT(nullptr),
    panBlockOffset(nullptr),
    nTileSize(0),
    bHasLoaderBlockMap(FALSE)
{
    adfGeoTransform[0] = 0;
    adfGeoTransform[1] = 1;
    adfGeoTransform[2] = 0;
    adfGeoTransform[3] = 0;
    adfGeoTransform[4] = 0;
    adfGeoTransform[5] = 1;
}

/************************************************************************/
/*                            ~HF2Dataset()                            */
/************************************************************************/

HF2Dataset::~HF2Dataset()

{
    FlushCache(true);
    CPLFree(pszWKT);
    CPLFree(panBlockOffset);
    if (fp)
        VSIFCloseL(fp);
}

/************************************************************************/
/*                            LoadBlockMap()                            */
/************************************************************************/

int HF2Dataset::LoadBlockMap()
{
    if (bHasLoaderBlockMap)
        return panBlockOffset != nullptr;

    bHasLoaderBlockMap = TRUE;

    const int nXBlocks = (nRasterXSize + nTileSize - 1) / nTileSize;
    const int nYBlocks = (nRasterYSize + nTileSize - 1) / nTileSize;
    if( nXBlocks * nYBlocks > 1000000 )
    {
        vsi_l_offset nCurOff = VSIFTellL(fp);
        VSIFSeekL( fp, 0, SEEK_END );
        vsi_l_offset nSize = VSIFTellL(fp);
        VSIFSeekL( fp, nCurOff, SEEK_SET );
        // Check that the file is big enough to have 8 bytes per block
        if( static_cast<vsi_l_offset>(nXBlocks) * nYBlocks > nSize / 8 )
        {
            return FALSE;
        }
    }
    panBlockOffset = (vsi_l_offset*) VSIMalloc3(sizeof(vsi_l_offset), nXBlocks, nYBlocks);
    if (panBlockOffset == nullptr)
    {
        return FALSE;
    }
    for(int j = 0; j < nYBlocks; j++)
    {
        for(int i = 0; i < nXBlocks; i++)
        {
            vsi_l_offset nOff = VSIFTellL(fp);
            panBlockOffset[j * nXBlocks + i] = nOff;
            //VSIFSeekL(fp, 4 + 4, SEEK_CUR);
            float fScale, fOff;
            VSIFReadL(&fScale, 4, 1, fp);
            VSIFReadL(&fOff, 4, 1, fp);
            CPL_LSBPTR32(&fScale);
            CPL_LSBPTR32(&fOff);
            //printf("fScale = %f, fOff = %f\n", fScale, fOff);
            const int nCols = std::min(nTileSize, nRasterXSize - nTileSize * i);
            const int nLines =
                std::min(nTileSize, nRasterYSize - nTileSize * j);
            for(int k = 0; k < nLines; k++)
            {
                GByte nWordSize;
                if( VSIFReadL(&nWordSize, 1, 1, fp) != 1 )
                {
                    CPLError(CE_Failure, CPLE_FileIO, "File too short");
                    VSIFree(panBlockOffset);
                    panBlockOffset = nullptr;
                    return FALSE;
                }
                //printf("nWordSize=%d\n", nWordSize);
                if (nWordSize == 1 || nWordSize == 2 || nWordSize == 4)
                    VSIFSeekL(fp, static_cast<vsi_l_offset>(4 + nWordSize * (nCols - 1)), SEEK_CUR);
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                            "Got unexpected byte depth (%d) for block (%d, %d) line %d",
                            (int)nWordSize, i, j, k);
                    VSIFree(panBlockOffset);
                    panBlockOffset = nullptr;
                    return FALSE;
                }
            }
        }
    }

    return TRUE;
}

/************************************************************************/
/*                     GetProjectionRef()                               */
/************************************************************************/

const char* HF2Dataset::_GetProjectionRef()
{
    if (pszWKT)
        return pszWKT;
    return GDALPamDataset::_GetProjectionRef();
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int HF2Dataset::Identify( GDALOpenInfo * poOpenInfo)
{

    GDALOpenInfo* poOpenInfoToDelete = nullptr;
    /*  GZipped .hf2 files are common, so automagically open them */
    /*  if the /vsigzip/ has not been explicitly passed */
    CPLString osFilename; // keep in that scope
    if ((EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "hfz") ||
        (strlen(poOpenInfo->pszFilename) > 6 &&
         EQUAL(poOpenInfo->pszFilename + strlen(poOpenInfo->pszFilename) - 6, "hf2.gz"))) &&
         !STARTS_WITH_CI(poOpenInfo->pszFilename, "/vsigzip/"))
    {
        osFilename = "/vsigzip/";
        osFilename += poOpenInfo->pszFilename;
        poOpenInfo = poOpenInfoToDelete =
                new GDALOpenInfo(osFilename.c_str(), GA_ReadOnly,
                                 poOpenInfo->GetSiblingFiles());
    }

    if (poOpenInfo->nHeaderBytes < 28)
    {
        delete poOpenInfoToDelete;
        return FALSE;
    }

    if (memcmp(poOpenInfo->pabyHeader, "HF2\0\0\0\0", 6) != 0)
    {
        delete poOpenInfoToDelete;
        return FALSE;
    }

    delete poOpenInfoToDelete;
    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *HF2Dataset::Open( GDALOpenInfo * poOpenInfo )

{
    CPLString osOriginalFilename(poOpenInfo->pszFilename);

    if (!Identify(poOpenInfo))
        return nullptr;

    GDALOpenInfo* poOpenInfoToDelete = nullptr;
    /*  GZipped .hf2 files are common, so automagically open them */
    /*  if the /vsigzip/ has not been explicitly passed */
    CPLString osFilename(poOpenInfo->pszFilename);
    if ((EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "hfz") ||
        (strlen(poOpenInfo->pszFilename) > 6 &&
         EQUAL(poOpenInfo->pszFilename + strlen(poOpenInfo->pszFilename) - 6, "hf2.gz"))) &&
         !STARTS_WITH_CI(poOpenInfo->pszFilename, "/vsigzip/"))
    {
        osFilename = "/vsigzip/";
        osFilename += poOpenInfo->pszFilename;
        poOpenInfo = poOpenInfoToDelete =
                new GDALOpenInfo(osFilename.c_str(), GA_ReadOnly,
                                 poOpenInfo->GetSiblingFiles());
    }

/* -------------------------------------------------------------------- */
/*      Parse header                                                    */
/* -------------------------------------------------------------------- */

    int nXSize, nYSize;
    memcpy(&nXSize, poOpenInfo->pabyHeader + 6, 4);
    CPL_LSBPTR32(&nXSize);
    memcpy(&nYSize, poOpenInfo->pabyHeader + 10, 4);
    CPL_LSBPTR32(&nYSize);

    GUInt16 nTileSize;
    memcpy(&nTileSize, poOpenInfo->pabyHeader + 14, 2);
    CPL_LSBPTR16(&nTileSize);

    float fVertPres, fHorizScale;
    memcpy(&fVertPres, poOpenInfo->pabyHeader + 16, 4);
    CPL_LSBPTR32(&fVertPres);
    memcpy(&fHorizScale, poOpenInfo->pabyHeader + 20, 4);
    CPL_LSBPTR32(&fHorizScale);

    GUInt32 nExtendedHeaderLen;
    memcpy(&nExtendedHeaderLen, poOpenInfo->pabyHeader + 24, 4);
    CPL_LSBPTR32(&nExtendedHeaderLen);

    delete poOpenInfoToDelete;
    poOpenInfoToDelete = nullptr;

    if (nTileSize < 8)
        return nullptr;
    if (nXSize <= 0 || nXSize > INT_MAX - nTileSize ||
        nYSize <= 0 || nYSize > INT_MAX - nTileSize)
        return nullptr;
    /* To avoid later potential int overflows */
    if (nExtendedHeaderLen > 1024 * 65536)
        return nullptr;

    if (!GDALCheckDatasetDimensions(nXSize, nYSize))
    {
        return nullptr;
    }
    const int nXBlocks = (nXSize + nTileSize - 1) / nTileSize;
    const int nYBlocks = (nYSize + nTileSize - 1) / nTileSize;
    if( nXBlocks > INT_MAX / nYBlocks )
    {
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Parse extended blocks                                           */
/* -------------------------------------------------------------------- */

    VSILFILE* fp = VSIFOpenL(osFilename.c_str(), "rb");
    if (fp == nullptr)
        return nullptr;

    VSIFSeekL(fp, 28, SEEK_SET);

    int bHasExtent = FALSE;
    double dfMinX = 0.0;
    double dfMaxX = 0.0;
    double dfMinY = 0.0;
    double dfMaxY = 0.0;
    int bHasUTMZone = FALSE;
    GInt16 nUTMZone = 0;
    int bHasEPSGDatumCode = FALSE;
    GInt16 nEPSGDatumCode = 0;
    int bHasEPSGCode = FALSE;
    GInt16 nEPSGCode = 0;
    int bHasRelativePrecision = FALSE;
    float fRelativePrecision = 0.0f;
    char szApplicationName[256] = { 0 };

    GUInt32 nExtendedHeaderOff = 0;
    while(nExtendedHeaderOff < nExtendedHeaderLen)
    {
        char pabyBlockHeader[24];
        VSIFReadL(pabyBlockHeader, 24, 1, fp);

        char szBlockName[16 + 1];
        memcpy(szBlockName, pabyBlockHeader + 4, 16);
        szBlockName[16] = 0;
        GUInt32 nBlockSize;
        memcpy(&nBlockSize, pabyBlockHeader + 20, 4);
        CPL_LSBPTR32(&nBlockSize);
        if (nBlockSize > 65536)
            break;

        nExtendedHeaderOff += 24 + nBlockSize;

        if (strcmp(szBlockName, "georef-extents") == 0 &&
            nBlockSize == 34)
        {
            char pabyBlockData[34];
            VSIFReadL(pabyBlockData, 34, 1, fp);

            memcpy(&dfMinX, pabyBlockData + 2, 8);
            CPL_LSBPTR64(&dfMinX);
            memcpy(&dfMaxX, pabyBlockData + 2 + 8, 8);
            CPL_LSBPTR64(&dfMaxX);
            memcpy(&dfMinY, pabyBlockData + 2 + 8 + 8, 8);
            CPL_LSBPTR64(&dfMinY);
            memcpy(&dfMaxY, pabyBlockData + 2 + 8 + 8 + 8, 8);
            CPL_LSBPTR64(&dfMaxY);

            bHasExtent = TRUE;
        }
        else if (strcmp(szBlockName, "georef-utm") == 0 &&
                nBlockSize == 2)
        {
            VSIFReadL(&nUTMZone, 2, 1, fp);
            CPL_LSBPTR16(&nUTMZone);
            CPLDebug("HF2", "UTM Zone = %d", nUTMZone);

            bHasUTMZone = TRUE;
        }
        else if (strcmp(szBlockName, "georef-datum") == 0 &&
                 nBlockSize == 2)
        {
            VSIFReadL(&nEPSGDatumCode, 2, 1, fp);
            CPL_LSBPTR16(&nEPSGDatumCode);
            CPLDebug("HF2", "EPSG Datum Code = %d", nEPSGDatumCode);

            bHasEPSGDatumCode = TRUE;
        }
        else if (strcmp(szBlockName, "georef-epsg-prj") == 0 &&
                 nBlockSize == 2)
        {
            VSIFReadL(&nEPSGCode, 2, 1, fp);
            CPL_LSBPTR16(&nEPSGCode);
            CPLDebug("HF2", "EPSG Code = %d", nEPSGCode);

            bHasEPSGCode = TRUE;
        }
        else if (strcmp(szBlockName, "precis-rel") == 0 &&
                 nBlockSize == 4)
        {
            VSIFReadL(&fRelativePrecision, 4, 1, fp);
            CPL_LSBPTR32(&fRelativePrecision);

            bHasRelativePrecision = TRUE;
        }
        else if (strcmp(szBlockName, "app-name") == 0 &&
                 nBlockSize < sizeof(szApplicationName))
        {
            VSIFReadL(szApplicationName, nBlockSize, 1, fp);
            szApplicationName[nBlockSize] = 0;
        }
        else
        {
            CPLDebug("HF2", "Skipping block %s", szBlockName);
            VSIFSeekL(fp, nBlockSize, SEEK_CUR);
        }
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    HF2Dataset *poDS = new HF2Dataset();
    poDS->fp = fp;
    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->nTileSize = nTileSize;
    CPLDebug("HF2", "nXSize = %d, nYSize = %d, nTileSize = %d", nXSize, nYSize, nTileSize);
    if (bHasExtent)
    {
        poDS->adfGeoTransform[0] = dfMinX;
        poDS->adfGeoTransform[3] = dfMaxY;
        poDS->adfGeoTransform[1] = (dfMaxX - dfMinX) / nXSize;
        poDS->adfGeoTransform[5] = -(dfMaxY - dfMinY) / nYSize;
    }
    else
    {
        poDS->adfGeoTransform[1] = fHorizScale;
        poDS->adfGeoTransform[5] = fHorizScale;
    }

    if (bHasEPSGCode)
    {
        OGRSpatialReference oSRS;
        if (oSRS.importFromEPSG(nEPSGCode) == OGRERR_NONE)
            oSRS.exportToWkt(&poDS->pszWKT);
    }
    else
    {
        bool bHasSRS = false;
        OGRSpatialReference oSRS;
        oSRS.SetGeogCS("unknown", "unknown", "unknown", SRS_WGS84_SEMIMAJOR, SRS_WGS84_INVFLATTENING);
        if (bHasEPSGDatumCode)
        {
            if (nEPSGDatumCode == 23 || nEPSGDatumCode == 6326)
            {
                bHasSRS = true;
                oSRS.SetWellKnownGeogCS("WGS84");
            }
            else if (nEPSGDatumCode >= 6000)
            {
                char szName[32];
                snprintf( szName, sizeof(szName), "EPSG:%d", nEPSGDatumCode-2000 );
                oSRS.SetWellKnownGeogCS( szName );
                bHasSRS = true;
            }
        }

        if (bHasUTMZone && std::abs(nUTMZone) >= 1 && std::abs(nUTMZone) <= 60)
        {
            bHasSRS = true;
            oSRS.SetUTM(std::abs(static_cast<int>(nUTMZone)), nUTMZone > 0);
        }
        if (bHasSRS)
            oSRS.exportToWkt(&poDS->pszWKT);
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->nBands = 1;
    for( int i = 0; i < poDS->nBands; i++ )
    {
        poDS->SetBand( i+1, new HF2RasterBand( poDS, i+1, GDT_Float32 ) );
        poDS->GetRasterBand(i+1)->SetUnitType("m");
    }

    if (szApplicationName[0] != '\0')
        poDS->SetMetadataItem("APPLICATION_NAME", szApplicationName);
    poDS->SetMetadataItem("VERTICAL_PRECISION", CPLString().Printf("%f", fVertPres));
    if (bHasRelativePrecision)
        poDS->SetMetadataItem("RELATIVE_VERTICAL_PRECISION", CPLString().Printf("%f", fRelativePrecision));

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( osOriginalFilename.c_str() );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Support overviews.                                              */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, osOriginalFilename.c_str() );
    return poDS;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr HF2Dataset::GetGeoTransform( double * padfTransform )

{
    memcpy(padfTransform, adfGeoTransform, 6 * sizeof(double));

    return CE_None;
}

static void WriteShort(VSILFILE* fp, GInt16 val)
{
    CPL_LSBPTR16(&val);
    VSIFWriteL(&val, 2, 1, fp);
}

static void WriteInt(VSILFILE* fp, GInt32 val)
{
    CPL_LSBPTR32(&val);
    VSIFWriteL(&val, 4, 1, fp);
}

static void WriteFloat(VSILFILE* fp, float val)
{
    CPL_LSBPTR32(&val);
    VSIFWriteL(&val, 4, 1, fp);
}

static void WriteDouble(VSILFILE* fp, double val)
{
    CPL_LSBPTR64(&val);
    VSIFWriteL(&val, 8, 1, fp);
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset* HF2Dataset::CreateCopy( const char * pszFilename,
                                     GDALDataset *poSrcDS,
                                     int bStrict, char ** papszOptions,
                                     GDALProgressFunc pfnProgress,
                                     void * pProgressData )
{
/* -------------------------------------------------------------------- */
/*      Some some rudimentary checks                                    */
/* -------------------------------------------------------------------- */
    int nBands = poSrcDS->GetRasterCount();
    if (nBands == 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "HF2 driver does not support source dataset with zero band.\n");
        return nullptr;
    }

    if (nBands != 1)
    {
        CPLError( (bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported,
                  "HF2 driver only uses the first band of the dataset.\n");
        if (bStrict)
            return nullptr;
    }

    if( pfnProgress && !pfnProgress( 0.0, nullptr, pProgressData ) )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Get source dataset info                                         */
/* -------------------------------------------------------------------- */

    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();
    double adfGeoTransform[6];
    poSrcDS->GetGeoTransform(adfGeoTransform);
    int bHasGeoTransform = !(adfGeoTransform[0] == 0 &&
                             adfGeoTransform[1] == 1 &&
                             adfGeoTransform[2] == 0 &&
                             adfGeoTransform[3] == 0 &&
                             adfGeoTransform[4] == 0 &&
                             adfGeoTransform[5] == 1);
    if (adfGeoTransform[2] != 0 || adfGeoTransform[4] != 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "HF2 driver does not support CreateCopy() from skewed or rotated dataset.\n");
        return nullptr;
    }

    GDALDataType eSrcDT = poSrcDS->GetRasterBand(1)->GetRasterDataType();
    GDALDataType eReqDT;
    float fVertPres = (float) 0.01;
    if (eSrcDT == GDT_Byte || eSrcDT == GDT_Int16)
    {
        fVertPres = 1;
        eReqDT = GDT_Int16;
    }
    else
        eReqDT = GDT_Float32;

/* -------------------------------------------------------------------- */
/*      Read creation options                                           */
/* -------------------------------------------------------------------- */
    const char* pszCompressed = CSLFetchNameValue(papszOptions, "COMPRESS");
    bool bCompress = false;
    if( pszCompressed )
        bCompress = CPLTestBool(pszCompressed);

    const char* pszVerticalPrecision = CSLFetchNameValue(papszOptions, "VERTICAL_PRECISION");
    if (pszVerticalPrecision)
    {
        fVertPres = (float) CPLAtofM(pszVerticalPrecision);
        if (fVertPres <= 0)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unsupported value for VERTICAL_PRECISION. Defaulting to 0.01");
            fVertPres = (float) 0.01;
        }
        if (eReqDT == GDT_Int16 && fVertPres > 1)
            eReqDT = GDT_Float32;
    }

    const char* pszBlockSize = CSLFetchNameValue(papszOptions, "BLOCKSIZE");
    int nTileSize = 256;
    if (pszBlockSize)
    {
        nTileSize = atoi(pszBlockSize);
        if (nTileSize < 8 || nTileSize > 4096)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unsupported value for BLOCKSIZE. Defaulting to 256");
            nTileSize = 256;
        }
    }

/* -------------------------------------------------------------------- */
/*      Parse source dataset georeferencing info                        */
/* -------------------------------------------------------------------- */

    int nExtendedHeaderLen = 0;
    if (bHasGeoTransform)
        nExtendedHeaderLen += 58;
    const char* pszProjectionRef = poSrcDS->GetProjectionRef();
    int nDatumCode = -2;
    int nUTMZone = 0;
    int bNorth = FALSE;
    int nEPSGCode = 0;
    int nExtentUnits = 1;
    if (pszProjectionRef != nullptr && pszProjectionRef[0] != '\0')
    {
        OGRSpatialReference oSRS;
        if (oSRS.importFromWkt(pszProjectionRef) == OGRERR_NONE)
        {
            const char* pszValue = nullptr;
            if( oSRS.GetAuthorityName( "GEOGCS|DATUM" ) != nullptr
                && EQUAL(oSRS.GetAuthorityName( "GEOGCS|DATUM" ),"EPSG") )
                nDatumCode = atoi(oSRS.GetAuthorityCode( "GEOGCS|DATUM" ));
            else if ((pszValue = oSRS.GetAttrValue("GEOGCS|DATUM")) != nullptr)
            {
                if (strstr(pszValue, "WGS") && strstr(pszValue, "84"))
                    nDatumCode = 6326;
            }

            nUTMZone = oSRS.GetUTMZone(&bNorth);
        }
        if( oSRS.GetAuthorityName( "PROJCS" ) != nullptr
            && EQUAL(oSRS.GetAuthorityName( "PROJCS" ),"EPSG") )
            nEPSGCode = atoi(oSRS.GetAuthorityCode( "PROJCS" ));

        if( oSRS.IsGeographic() )
        {
            nExtentUnits = 0;
        }
        else
        {
            const double dfLinear = oSRS.GetLinearUnits();

            if( std::abs(dfLinear - 0.3048) < 0.0000001 )
                nExtentUnits = 2;
            else if( std::abs(dfLinear - CPLAtof(SRS_UL_US_FOOT_CONV)) <
                     0.00000001 )
                nExtentUnits = 3;
            else
                nExtentUnits = 1;
        }
    }
    if (nDatumCode != -2)
        nExtendedHeaderLen += 26;
    if (nUTMZone != 0)
        nExtendedHeaderLen += 26;
    if (nEPSGCode)
        nExtendedHeaderLen += 26;

/* -------------------------------------------------------------------- */
/*      Create target file                                              */
/* -------------------------------------------------------------------- */

    CPLString osFilename;
    if (bCompress)
    {
        osFilename = "/vsigzip/";
        osFilename += pszFilename;
    }
    else
        osFilename = pszFilename;
    VSILFILE* fp = VSIFOpenL(osFilename.c_str(), "wb");
    if (fp == nullptr)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Cannot create %s", pszFilename );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Write header                                                    */
/* -------------------------------------------------------------------- */

    VSIFWriteL("HF2\0", 4, 1, fp);
    WriteShort(fp, 0);
    WriteInt(fp, nXSize);
    WriteInt(fp, nYSize);
    WriteShort(fp, (GInt16) nTileSize);
    WriteFloat(fp, fVertPres);
    const float fHorizScale
        = (float) ((fabs(adfGeoTransform[1]) + fabs(adfGeoTransform[5])) / 2);
    WriteFloat(fp, fHorizScale);
    WriteInt(fp, nExtendedHeaderLen);

/* -------------------------------------------------------------------- */
/*      Write extended header                                           */
/* -------------------------------------------------------------------- */

    char szBlockName[16 + 1];
    if (bHasGeoTransform)
    {
        VSIFWriteL("bin\0", 4, 1, fp);
        memset(szBlockName, 0, 16 + 1);
        strcpy(szBlockName, "georef-extents");
        VSIFWriteL(szBlockName, 16, 1, fp);
        WriteInt(fp, 34);
        WriteShort(fp, (GInt16) nExtentUnits);
        WriteDouble(fp, adfGeoTransform[0]);
        WriteDouble(fp, adfGeoTransform[0] + nXSize * adfGeoTransform[1]);
        WriteDouble(fp, adfGeoTransform[3] + nYSize * adfGeoTransform[5]);
        WriteDouble(fp, adfGeoTransform[3]);
    }
    if (nUTMZone != 0)
    {
        VSIFWriteL("bin\0", 4, 1, fp);
        memset(szBlockName, 0, 16 + 1);
        strcpy(szBlockName, "georef-utm");
        VSIFWriteL(szBlockName, 16, 1, fp);
        WriteInt(fp, 2);
        WriteShort(fp, (GInt16) ((bNorth) ? nUTMZone : -nUTMZone));
    }
    if (nDatumCode != -2)
    {
        VSIFWriteL("bin\0", 4, 1, fp);
        memset(szBlockName, 0, 16 + 1);
        strcpy(szBlockName, "georef-datum");
        VSIFWriteL(szBlockName, 16, 1, fp);
        WriteInt(fp, 2);
        WriteShort(fp, (GInt16) nDatumCode);
    }
    if (nEPSGCode != 0)
    {
        VSIFWriteL("bin\0", 4, 1, fp);
        memset(szBlockName, 0, 16 + 1);
        strcpy(szBlockName, "georef-epsg-prj");
        VSIFWriteL(szBlockName, 16, 1, fp);
        WriteInt(fp, 2);
        WriteShort(fp, (GInt16) nEPSGCode);
    }

/* -------------------------------------------------------------------- */
/*      Copy imagery                                                    */
/* -------------------------------------------------------------------- */
    const int nXBlocks = (nXSize + nTileSize - 1) / nTileSize;
    const int nYBlocks = (nYSize + nTileSize - 1) / nTileSize;

    void* pTileBuffer = (void*) VSI_MALLOC_VERBOSE(nTileSize * nTileSize * (GDALGetDataTypeSize(eReqDT) / 8));
    if (pTileBuffer == nullptr)
    {
        VSIFCloseL(fp);
        return nullptr;
    }

    CPLErr eErr = CE_None;
    for(int j=0;j<nYBlocks && eErr == CE_None;j++)
    {
        for(int i=0;i<nXBlocks && eErr == CE_None;i++)
        {
            const int nReqXSize = std::min(nTileSize, nXSize - i * nTileSize);
            const int nReqYSize = std::min(nTileSize, nYSize - j * nTileSize);
            eErr = poSrcDS->GetRasterBand(1)->RasterIO(
                GF_Read,
                i * nTileSize, std::max(0, nYSize - (j + 1) * nTileSize),
                nReqXSize, nReqYSize,
                pTileBuffer, nReqXSize, nReqYSize,
                eReqDT, 0, 0, nullptr);
            if (eErr != CE_None)
                break;

            if (eReqDT == GDT_Int16)
            {
                WriteFloat(fp, 1); /* scale */
                WriteFloat(fp, 0); /* offset */
                for(int k=0;k<nReqYSize;k++)
                {
                    int nLastVal = ((short*)pTileBuffer)[(nReqYSize - k - 1) * nReqXSize + 0];
                    GByte nWordSize = 1;
                    for(int l=1;l<nReqXSize;l++)
                    {
                        const int nVal = ((short*)pTileBuffer)[(nReqYSize - k - 1) * nReqXSize + l];
                        const int nDiff = nVal - nLastVal;
                        if (nDiff < -32768 || nDiff > 32767)
                        {
                            nWordSize = 4;
                            break;
                        }
                        if (nDiff < -128 || nDiff > 127)
                            nWordSize = 2;
                        nLastVal = nVal;
                    }

                    VSIFWriteL(&nWordSize, 1, 1, fp);
                    nLastVal = ((short*)pTileBuffer)[(nReqYSize - k - 1) * nReqXSize + 0];
                    WriteInt(fp, nLastVal);
                    for(int l=1;l<nReqXSize;l++)
                    {
                        const int nVal = ((short*)pTileBuffer)[(nReqYSize - k - 1) * nReqXSize + l];
                        const int nDiff = nVal - nLastVal;
                        if (nWordSize == 1)
                        {
                            CPLAssert(nDiff >= -128 && nDiff <= 127);
                            signed char chDiff = (signed char)nDiff;
                            VSIFWriteL(&chDiff, 1, 1, fp);
                        }
                        else if (nWordSize == 2)
                        {
                            CPLAssert(nDiff >= -32768 && nDiff <= 32767);
                            WriteShort(fp, (short)nDiff);
                        }
                        else
                        {
                            WriteInt(fp, nDiff);
                        }
                        nLastVal = nVal;
                    }
                }
            }
            else
            {
                float fMinVal = ((float*)pTileBuffer)[0];
                float fMaxVal = fMinVal;
                for(int k=1;k<nReqYSize*nReqXSize;k++)
                {
                    float fVal = ((float*)pTileBuffer)[k];
                    if( CPLIsNan(fVal) )
                    {
                        CPLError(CE_Failure, CPLE_NotSupported,
                                 "NaN value found");
                        eErr = CE_Failure;
                        break;
                    }
                    if (fVal < fMinVal) fMinVal = fVal;
                    if (fVal > fMaxVal) fMaxVal = fVal;
                }
                if( eErr == CE_Failure )
                    break;

                float fIntRange = (fMaxVal - fMinVal) / fVertPres;
                if( fIntRange > static_cast<float>(std::numeric_limits<int>::max()) )
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "VERTICAL_PRECISION too small regarding actual range of values");
                    eErr = CE_Failure;
                    break;
                }
                float fScale = (fMinVal == fMaxVal) ? 1 : (fMaxVal - fMinVal) / fIntRange;
                if( fScale == 0.0f )
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Scale == 0.0f");
                    eErr = CE_Failure;
                    break;
                }
                float fOffset = fMinVal;
                WriteFloat(fp, fScale); /* scale */
                WriteFloat(fp, fOffset); /* offset */
                for(int k=0;k<nReqYSize;k++)
                {
                    float fLastVal = ((float*)pTileBuffer)[(nReqYSize - k - 1) * nReqXSize + 0];
                    float fIntLastVal = (fLastVal - fOffset) / fScale;
                    CPLAssert(fIntLastVal <= static_cast<float>(std::numeric_limits<int>::max()));
                    int nLastVal = (int)fIntLastVal;
                    GByte nWordSize = 1;
                    for(int l=1;l<nReqXSize;l++)
                    {
                        float fVal = ((float*)pTileBuffer)[(nReqYSize - k - 1) * nReqXSize + l];
                        float fIntVal = (fVal - fOffset) / fScale;
                        CPLAssert(fIntVal <= static_cast<float>(std::numeric_limits<int>::max()));
                        const int nVal = (int)fIntVal;
                        const int nDiff = nVal - nLastVal;
                        CPLAssert((int)((GIntBig)nVal - nLastVal) == nDiff);
                        if (nDiff < -32768 || nDiff > 32767)
                        {
                            nWordSize = 4;
                            break;
                        }
                        if (nDiff < -128 || nDiff > 127)
                            nWordSize = 2;
                        nLastVal = nVal;
                    }

                    VSIFWriteL(&nWordSize, 1, 1, fp);
                    fLastVal = ((float*)pTileBuffer)[(nReqYSize - k - 1) * nReqXSize + 0];
                    fIntLastVal = (fLastVal - fOffset) / fScale;
                    nLastVal = (int)fIntLastVal;
                    WriteInt(fp, nLastVal);
                    for(int l=1;l<nReqXSize;l++)
                    {
                        float fVal = ((float*)pTileBuffer)[(nReqYSize - k - 1) * nReqXSize + l];
                        float fIntVal = (fVal - fOffset) / fScale;
                        int nVal = (int)fIntVal;
                        int nDiff = nVal - nLastVal;
                        CPLAssert((int)((GIntBig)nVal - nLastVal) == nDiff);
                        if (nWordSize == 1)
                        {
                            CPLAssert(nDiff >= -128 && nDiff <= 127);
                            signed char chDiff = (signed char)nDiff;
                            VSIFWriteL(&chDiff, 1, 1, fp);
                        }
                        else if (nWordSize == 2)
                        {
                            CPLAssert(nDiff >= -32768 && nDiff <= 32767);
                            WriteShort(fp, (short)nDiff);
                        }
                        else
                        {
                            WriteInt(fp, nDiff);
                        }
                        nLastVal = nVal;
                    }
                }
            }

            if( pfnProgress && !pfnProgress( (j * nXBlocks + i + 1) * 1.0 / (nXBlocks * nYBlocks), nullptr, pProgressData ) )
            {
                eErr = CE_Failure;
                break;
            }
        }
    }

    CPLFree(pTileBuffer);

    VSIFCloseL(fp);

    if (eErr != CE_None)
        return nullptr;

    GDALOpenInfo oOpenInfo(osFilename.c_str(), GA_ReadOnly);
    HF2Dataset* poDS = reinterpret_cast<HF2Dataset*>(Open(&oOpenInfo));

    if( poDS )
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );

    return poDS;
}

/************************************************************************/
/*                         GDALRegister_HF2()                           */
/************************************************************************/

void GDALRegister_HF2()

{
    if( GDALGetDriverByName( "HF2" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "HF2" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "HF2/HFZ heightfield raster" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/hf2.html" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "hf2" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='VERTICAL_PRECISION' type='float' default='0.01' description='Vertical precision.'/>"
"   <Option name='COMPRESS' type='boolean' default='false' description='Set to true to produce a GZip compressed file.'/>"
"   <Option name='BLOCKSIZE' type='int' default='256' description='Tile size.'/>"
"</CreationOptionList>");

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = HF2Dataset::Open;
    poDriver->pfnIdentify = HF2Dataset::Identify;
    poDriver->pfnCreateCopy = HF2Dataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
