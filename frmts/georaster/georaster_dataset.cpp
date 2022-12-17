/******************************************************************************
 *
 * Name:     georaster_dataset.cpp
 * Project:  Oracle Spatial GeoRaster Driver
 * Purpose:  Implement GeoRasterDataset Methods
 * Author:   Ivan Lucena [ivan.lucena at oracle.com]
 *
 ******************************************************************************
 * Copyright (c) 2008, Ivan Lucena <ivan dot lucena at oracle dot com>
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files ( the "Software" ),
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
 *****************************************************************************/

#include "cpl_error.h"
#include "cpl_vsi_virtual.h"
#include "gdaljp2metadata.h"
#include "cpl_list.h"

#include "gdal.h"
#include "gdal_frmts.h"
#include "gdal_priv.h"
#include "ogr_spatialref.h"

#include "georaster_priv.h"

#include <memory>

//  ---------------------------------------------------------------------------
//                                                           GeoRasterDataset()
//  ---------------------------------------------------------------------------

GeoRasterDataset::GeoRasterDataset()
{
    bGeoTransform = false;
    bForcedSRID = false;
    poGeoRaster = nullptr;
    papszSubdatasets = nullptr;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    poMaskBand = nullptr;
    bApplyNoDataArray = false;
    poJP2Dataset = nullptr;
}

//  ---------------------------------------------------------------------------
//                                                          ~GeoRasterDataset()
//  ---------------------------------------------------------------------------

GeoRasterDataset::~GeoRasterDataset()
{
    GeoRasterDataset::FlushCache(true);

    poGeoRaster->FlushMetadata();

    delete poGeoRaster;

    if (poMaskBand)
    {
        delete poMaskBand;
    }

    if (poJP2Dataset)
    {
        delete poJP2Dataset;
    }

    CSLDestroy(papszSubdatasets);
}

//  ---------------------------------------------------------------------------
//                                                                   Identify()
//  ---------------------------------------------------------------------------

int GeoRasterDataset::Identify(GDALOpenInfo *poOpenInfo)
{
    //  -------------------------------------------------------------------
    //  Verify georaster prefix
    //  -------------------------------------------------------------------

    char *pszFilename = poOpenInfo->pszFilename;

    if (STARTS_WITH_CI(pszFilename, "georaster:") == false &&
        STARTS_WITH_CI(pszFilename, "geor:") == false)
    {
        return false;
    }

    return true;
}

//  ---------------------------------------------------------------------------
//                                                                       Open()
//  ---------------------------------------------------------------------------

GDALDataset *GeoRasterDataset::Open(GDALOpenInfo *poOpenInfo)
{
    //  -------------------------------------------------------------------
    //  It should not have an open file pointer.
    //  -------------------------------------------------------------------

    if (poOpenInfo->fpL != nullptr)
    {
        return nullptr;
    }

    //  -------------------------------------------------------------------
    //  Check identification string and usage
    //  -------------------------------------------------------------------

    if (!Identify(poOpenInfo))
    {
        return nullptr;
    }

    //  -------------------------------------------------------------------
    //  Create a GeoRaster wrapper object
    //  -------------------------------------------------------------------

    GeoRasterWrapper *poGRW = GeoRasterWrapper::Open(
        poOpenInfo->pszFilename, poOpenInfo->eAccess == GA_Update);

    if (!poGRW)
    {
        return nullptr;
    }

    //  -------------------------------------------------------------------
    //  Create a corresponding GDALDataset
    //  -------------------------------------------------------------------

    GeoRasterDataset *poGRD = new GeoRasterDataset();

    if (!poGRD)
    {
        return nullptr;
    }

    poGRD->eAccess = poOpenInfo->eAccess;
    poGRD->poGeoRaster = poGRW;

    //  -------------------------------------------------------------------
    //  List Subdatasets
    //  -------------------------------------------------------------------

    if (!poGRW->bUniqueFound)
    {
        if (poGRD->eAccess == GA_ReadOnly)
        {
            poGRD->SetSubdatasets(poGRW);

            if (CSLCount(poGRD->papszSubdatasets) == 0)
            {
                delete poGRD;
                poGRD = nullptr;
            }
        }
        return (GDALDataset *)poGRD;
    }

    //  -------------------------------------------------------------------
    //  Assign GeoRaster information
    //  -------------------------------------------------------------------

    poGRD->poGeoRaster = poGRW;
    poGRD->nRasterXSize = poGRW->nRasterColumns;
    poGRD->nRasterYSize = poGRW->nRasterRows;
    poGRD->nBands = poGRW->nRasterBands;

    if (poGRW->bIsReferenced)
    {
        poGRD->adfGeoTransform[1] = poGRW->dfXCoefficient[0];
        poGRD->adfGeoTransform[2] = poGRW->dfXCoefficient[1];
        poGRD->adfGeoTransform[0] = poGRW->dfXCoefficient[2];
        poGRD->adfGeoTransform[4] = poGRW->dfYCoefficient[0];
        poGRD->adfGeoTransform[5] = poGRW->dfYCoefficient[1];
        poGRD->adfGeoTransform[3] = poGRW->dfYCoefficient[2];
    }

    //  -------------------------------------------------------------------
    //  Copy RPC values to RPC metadata domain
    //  -------------------------------------------------------------------

    if (poGRW->phRPC)
    {
        char **papszRPC_MD = RPCInfoV2ToMD(poGRW->phRPC);
        char **papszSanitazed = nullptr;

        int i = 0;
        int n = CSLCount(papszRPC_MD);

        for (i = 0; i < n; i++)
        {
            if (STARTS_WITH_CI(papszRPC_MD[i], "MIN_LAT") ||
                STARTS_WITH_CI(papszRPC_MD[i], "MIN_LONG") ||
                STARTS_WITH_CI(papszRPC_MD[i], "MAX_LAT") ||
                STARTS_WITH_CI(papszRPC_MD[i], "MAX_LONG"))
            {
                continue;
            }
            papszSanitazed = CSLAddString(papszSanitazed, papszRPC_MD[i]);
        }

        poGRD->SetMetadata(papszSanitazed, "RPC");

        CSLDestroy(papszRPC_MD);
        CSLDestroy(papszSanitazed);
    }

    //  -------------------------------------------------------------------
    //  Open for JPEG 2000 compression for reading
    //  -------------------------------------------------------------------

    if (EQUAL(poGRW->sCompressionType.c_str(), "JP2-F") &&
        poGRD->eAccess == GA_ReadOnly)
    {
        poGRD->JP2_Open(poOpenInfo->eAccess);

        if (!poGRD->poJP2Dataset)
        {
            delete poGRD;
            return nullptr;
        }
    }

    //  -------------------------------------------------------------------
    //  Load mask band
    //  -------------------------------------------------------------------

    poGRW->bHasBitmapMask = EQUAL(
        "TRUE", CPLGetXMLValue(poGRW->phMetadata,
                               "layerInfo.objectLayer.bitmapMask", "FALSE"));

    if (poGRW->bHasBitmapMask)
    {
        poGRD->poMaskBand = new GeoRasterRasterBand(poGRD, 0, DEFAULT_BMP_MASK);
    }

    //  -------------------------------------------------------------------
    //  Check for filter Nodata environment variable, default is YES
    //  -------------------------------------------------------------------

    const char *pszGEOR_FILTER_NODATA =
        CPLGetConfigOption("GEOR_FILTER_NODATA_VALUES", "NO");

    if (!EQUAL(pszGEOR_FILTER_NODATA, "NO"))
    {
        poGRD->bApplyNoDataArray = true;
    }

    //  -------------------------------------------------------------------
    //  Create bands
    //  -------------------------------------------------------------------

    int i = 0;

    for (i = 1; i <= poGRD->nBands; i++)
    {
        poGRD->SetBand(
            i, new GeoRasterRasterBand(poGRD, i, 0, poGRD->poJP2Dataset));
    }

    //  -------------------------------------------------------------------
    //  Set IMAGE_STRUCTURE metadata information
    //  -------------------------------------------------------------------

    if (poGRW->nBandBlockSize == 1)
    {
        poGRD->SetMetadataItem("INTERLEAVE", "BSQ", "IMAGE_STRUCTURE");
    }
    else
    {
        if (EQUAL(poGRW->sInterleaving.c_str(), "BSQ"))
        {
            poGRD->SetMetadataItem("INTERLEAVE", "BSQ", "IMAGE_STRUCTURE");
        }
        else if (EQUAL(poGRW->sInterleaving.c_str(), "BIP"))
        {
            poGRD->SetMetadataItem("INTERLEAVE", "PIB", "IMAGE_STRUCTURE");
        }
        else if (EQUAL(poGRW->sInterleaving.c_str(), "BIL"))
        {
            poGRD->SetMetadataItem("INTERLEAVE", "BIL", "IMAGE_STRUCTURE");
        }
    }

    poGRD->SetMetadataItem("COMPRESSION",
                           CPLGetXMLValue(poGRW->phMetadata,
                                          "rasterInfo.compression.type",
                                          "NONE"),
                           "IMAGE_STRUCTURE");

    if (STARTS_WITH_CI(poGRW->sCompressionType.c_str(), "JPEG"))
    {
        poGRD->SetMetadataItem("COMPRESSION_QUALITY",
                               CPLGetXMLValue(poGRW->phMetadata,
                                              "rasterInfo.compression.quality",
                                              "undefined"),
                               "IMAGE_STRUCTURE");
    }

    if (EQUAL(poGRW->sCellDepth.c_str(), "1BIT"))
    {
        poGRD->SetMetadataItem("NBITS", "1", "IMAGE_STRUCTURE");
    }

    if (EQUAL(poGRW->sCellDepth.c_str(), "2BIT"))
    {
        poGRD->SetMetadataItem("NBITS", "2", "IMAGE_STRUCTURE");
    }

    if (EQUAL(poGRW->sCellDepth.c_str(), "4BIT"))
    {
        poGRD->SetMetadataItem("NBITS", "4", "IMAGE_STRUCTURE");
    }

    //  -------------------------------------------------------------------
    //  Set Metadata on "ORACLE" domain
    //  -------------------------------------------------------------------

    char *pszDoc = CPLSerializeXMLTree(poGRW->phMetadata);

    poGRD->SetMetadataItem(
        "TABLE_NAME",
        CPLSPrintf("%s%s", poGRW->sSchema.c_str(), poGRW->sTable.c_str()),
        "ORACLE");

    poGRD->SetMetadataItem("COLUMN_NAME", poGRW->sColumn.c_str(), "ORACLE");

    poGRD->SetMetadataItem("RDT_TABLE_NAME", poGRW->sDataTable.c_str(),
                           "ORACLE");

    poGRD->SetMetadataItem("RASTER_ID", CPLSPrintf("%lld", poGRW->nRasterId),
                           "ORACLE");

    poGRD->SetMetadataItem("SRID", CPLSPrintf("%lld", poGRW->nSRID), "ORACLE");

    poGRD->SetMetadataItem("WKT", poGRW->sWKText.c_str(), "ORACLE");

    poGRD->SetMetadataItem("COMPRESSION", poGRW->sCompressionType.c_str(),
                           "ORACLE");

    poGRD->SetMetadataItem("METADATA", pszDoc, "ORACLE");

    CPLFree(pszDoc);

    //  -------------------------------------------------------------------
    //  Return a GDALDataset
    //  -------------------------------------------------------------------

    return (GDALDataset *)poGRD;
}

//  ---------------------------------------------------------------------------
//                                                                    JP2Open()
//  ---------------------------------------------------------------------------

void GeoRasterDataset::JP2_Open(GDALAccess /* eAccess */)
{
    GDALDriver *poJP2Driver = nullptr;

    static const char *const apszDrivers[] = {
        "JP2OPENJPEG", "JP2ECW", "JP2MRSID", "JPEG2000", "JP2KAK", nullptr};

    // Find at least one available JP2 driver

    for (int iDriver = 0; apszDrivers[iDriver] != nullptr; iDriver++)
    {
        poJP2Driver = (GDALDriver *)GDALGetDriverByName(apszDrivers[iDriver]);

        if (poJP2Driver)
        {
            break;
        }
    }

    // If JP2 driver is installed, try to open the LOB via VSIOCILOB handler

    poJP2Dataset = nullptr;

    if (poJP2Driver)
    {
        CPLString osDSName;

        osDSName.Printf("/vsiocilob/%s,%s,%s,%s,%lld,noext",
                        poGeoRaster->poConnection->GetUser(),
                        poGeoRaster->poConnection->GetPassword(),
                        poGeoRaster->poConnection->GetServer(),
                        poGeoRaster->sDataTable.c_str(),
                        poGeoRaster->nRasterId);

        CPLPushErrorHandler(CPLQuietErrorHandler);

        poJP2Dataset = (GDALDataset *)GDALOpenEx(
            osDSName.c_str(), GDAL_OF_RASTER, apszDrivers, nullptr, nullptr);

        CPLPopErrorHandler();

        if (!poJP2Dataset)
        {
            CPLString osLastErrorMsg(CPLGetLastErrorMsg());
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Unable to open JPEG2000 image within GeoRaster dataset.\n%s",
                osLastErrorMsg.c_str());
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to open JPEG2000 image within GeoRaster dataset.\n%s",
                 "No JPEG2000 capable driver (JP2OPENJPEG, "
                 "JP2ECW, JP2MRSID, etc...) is available.");
    }
}

//  ---------------------------------------------------------------------------
//                                                              JP2CreateCopy()
//  ---------------------------------------------------------------------------

void GeoRasterDataset::JP2_CreateCopy(GDALDataset *poJP2DS, char **papszOptions,
                                      int *pnResolutions,
                                      GDALProgressFunc pfnProgress,
                                      void *pProgressData)
{
    GDALDriver *poJP2Driver = nullptr;

    static const char *const apszDrivers[] = {
        "JP2OPENJPEG", "JP2ECW", "JP2MRSID", "JPEG2000", "JP2KAK", nullptr};

    // Find at least one available JP2 driver

    for (int iDriver = 0; apszDrivers[iDriver] != nullptr; iDriver++)
    {
        poJP2Driver = (GDALDriver *)GDALGetDriverByName(apszDrivers[iDriver]);

        if (poJP2Driver)
        {
            break;
        }
    }

    // If a JP2 driver is installed calls driver's CreateCopy

    poJP2Dataset = nullptr;

    if (poJP2Driver)
    {
        char **papszOpt = nullptr;

        const char *pszFetched =
            CSLFetchNameValue(papszOptions, "JP2_BLOCKXSIZE");

        if (pszFetched)
        {
            papszOpt = CSLAddNameValue(papszOpt, "BLOCKXSIZE", pszFetched);
            papszOpt = CSLAddNameValue(papszOpt, "TILE_HEIGHT", pszFetched);
        }

        CPLDebug("GEOR", "JP2_BLOCKXSIZE %s", pszFetched);

        pszFetched = CSLFetchNameValue(papszOptions, "JP2_BLOCKYSIZE");

        if (pszFetched)
        {
            papszOpt = CSLAddNameValue(papszOpt, "BLOCKYSIZE", pszFetched);
            papszOpt = CSLAddNameValue(papszOpt, "TILE_WIDTH", pszFetched);
        }

        pszFetched = CSLFetchNameValue(papszOptions, "JP2_QUALITY");

        if (pszFetched)
        {
            papszOpt = CSLAddNameValue(papszOpt, "QUALITY", pszFetched);

            if (STARTS_WITH_CI(pszFetched, "100"))
            {
                papszOpt = CSLAddNameValue(papszOpt, "REVERSIBLE", "TRUE");
            }

            poGeoRaster->nCompressQuality = atoi(pszFetched);
        }
        else
        {
            poGeoRaster->nCompressQuality = 25;  // JP2OpenJPEG default...
        }

        pszFetched = CSLFetchNameValue(papszOptions, "JP2_REVERSIBLE");

        if (pszFetched)
        {
            papszOpt = CSLAddNameValue(papszOpt, "REVERSIBLE", pszFetched);
        }

        pszFetched = CSLFetchNameValue(papszOptions, "JP2_RESOLUTIONS");

        if (pszFetched)
        {
            papszOpt = CSLAddNameValue(papszOpt, "RESOLUTIONS", pszFetched);
            papszOpt =
                CSLAddNameValue(papszOpt, "RESOLUTIONS_LEVELS", pszFetched);
            papszOpt = CSLAddNameValue(papszOpt, "LAYERS", pszFetched);
        }

        pszFetched = CSLFetchNameValue(papszOptions, "JP2_PROGRESSION");

        if (pszFetched)
        {
            papszOpt = CSLAddNameValue(papszOpt, "PROGRESSION", pszFetched);
        }

        papszOpt = CSLAddNameValue(papszOpt, "CODEC", "JP2");
        papszOpt = CSLAddNameValue(papszOpt, "GeoJP2", "NO");
        papszOpt = CSLAddNameValue(papszOpt, "GMLJP2", "NO");
        papszOpt = CSLAddNameValue(papszOpt, "YCBCR420", "NO");
        papszOpt = CSLAddNameValue(papszOpt, "TARGET", "0");

        CPLPushErrorHandler(CPLQuietErrorHandler);

        CPLString osDSName;

        osDSName.Printf("/vsiocilob/%s,%s,%s,%s,%lld,noext",
                        poGeoRaster->poConnection->GetUser(),
                        poGeoRaster->poConnection->GetPassword(),
                        poGeoRaster->poConnection->GetServer(),
                        poGeoRaster->sDataTable.c_str(),
                        poGeoRaster->nRasterId);

        poJP2Dataset = (GDALDataset *)GDALCreateCopy(
            poJP2Driver, osDSName.c_str(), poJP2DS, false, (char **)papszOpt,
            pfnProgress, pProgressData);

        CPLPopErrorHandler();

        CSLDestroy(papszOpt);

        if (!poJP2Dataset)
        {
            CPLString osLastErrorMsg(CPLGetLastErrorMsg());
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Unable to copy JPEG2000 image within GeoRaster dataset.\n%s",
                osLastErrorMsg.c_str());
            return;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to copy JPEG2000 image within GeoRaster dataset.\n%s",
                 "No JPEG2000 capable driver (JP2OPENJPEG, "
                 "JP2ECW, JP2MRSID, etc...) is available.");
        return;
    }

    // Retrieve the number of resolutions based on the number of overviews

    CPLPushErrorHandler(CPLQuietErrorHandler);

    *pnResolutions = poJP2Dataset->GetRasterBand(1)->GetOverviewCount() + 1;

    delete poJP2Dataset;

    CPLPopErrorHandler();  // Avoid showing warning regards writing aux.xml file

    poJP2Dataset = nullptr;
}

//  ---------------------------------------------------------------------------
//                                                             JP2_CopyDirect()
//  ---------------------------------------------------------------------------

boolean GeoRasterDataset::JP2_CopyDirect(const char *pszJP2Filename,
                                         int *pnResolutions,
                                         GDALProgressFunc pfnProgress,
                                         void *pProgressData)
{
    char **papszFileList = GetFileList();

    if (CSLCount(papszFileList) == 0)
    {
        CSLDestroy(papszFileList);
        return false;
    }

    VSILFILE *fpInput = VSIFOpenL(pszJP2Filename, "r");
    VSILFILE *fpOutput = VSIFOpenL(papszFileList[0], "wb");

    size_t nCache = (size_t)(GDALGetCacheMax() * 0.25);

    void *pBuffer = (GByte *)VSIMalloc(sizeof(GByte) * nCache);

    GDALJP2Box oBox(fpInput);

    (void)oBox.ReadFirst();

    GUInt32 nLBox;
    GUInt32 nTBox;

    int nBoxCount = 0;

    while (strlen(oBox.GetType()) > 0)
    {
        nBoxCount++;

        if (EQUAL(oBox.GetType(), "jp  ") || EQUAL(oBox.GetType(), "ftyp") ||
            EQUAL(oBox.GetType(), "jp2h"))
        {
            size_t nDataLength = (size_t)oBox.GetDataLength();

            size_t nSize = VSIFReadL(pBuffer, 1, nDataLength, fpInput);

            if (nSize != nDataLength)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "amount read differs from JP2 Box data length");
            }

            nLBox = CPL_MSBWORD32((int)nDataLength + 8);

            memcpy(&nTBox, oBox.GetType(), 4);

            VSIFWriteL(&nLBox, 4, 1, fpOutput);
            VSIFWriteL(&nTBox, 4, 1, fpOutput);
            VSIFWriteL(pBuffer, 1, nSize, fpOutput);
        }

        if (EQUAL(oBox.GetType(), "jp2c"))
        {
            size_t nCount = 0;
            const size_t nDataLength = oBox.GetDataLength();

            nLBox = CPL_MSBWORD32((int)nDataLength + 8);

            memcpy(&nTBox, oBox.GetType(), 4);

            VSIFWriteL(&nLBox, 4, 1, fpOutput);
            VSIFWriteL(&nTBox, 4, 1, fpOutput);

            while (nCount < nDataLength)
            {
                const size_t nChunk = (size_t)MIN(nCache, nDataLength - nCount);

                const size_t nSize = VSIFReadL(pBuffer, 1, nChunk, fpInput);

                if (nSize != nChunk)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "amount read differs from JP2 data length");
                }

                VSIFWriteL(pBuffer, 1, nSize, fpOutput);

                nCount += nSize;

                pfnProgress((float)nCount / (float)nDataLength, nullptr,
                            pProgressData);
            }
        }

        if (!oBox.ReadNext())
        {
            break;
        }
    }

    VSIFCloseL(fpInput);
    VSIFCloseL(fpOutput);

    CSLDestroy(papszFileList);
    CPLFree(pBuffer);

    // Retrieve the number of resolutions based on the number of overviews

    JP2_Open(GA_ReadOnly);

    if (poJP2Dataset)
    {
        *pnResolutions = poJP2Dataset->GetRasterBand(1)->GetOverviewCount() + 1;

        delete poJP2Dataset;
        poJP2Dataset = nullptr;
    }

    return (nBoxCount > 0);
}

//  ---------------------------------------------------------------------------
//                                                             JPG_CopyDirect()
//  ---------------------------------------------------------------------------

boolean GeoRasterDataset::JPEG_CopyDirect(const char *pszJPGFilename,
                                          GDALProgressFunc pfnProgress,
                                          void *pProgressData)
{
    OWConnection *poConnection = poGeoRaster->poConnection;
    OCILobLocator *poLocator;

    OWStatement *poStmt = poConnection->CreateStatement(
        CPLSPrintf("select rasterblock from %s where rasterid = %lld "
                   "and rownum = 1 for update",
                   poGeoRaster->sDataTable.c_str(), poGeoRaster->nRasterId));

    poStmt->Define(&poLocator);

    if (poStmt->Execute())
    {
        VSILFILE *fpInput = VSIFOpenL(pszJPGFilename, "r");

        size_t nCache = (size_t)(GDALGetCacheMax() * 0.25);

        void *pBuffer = (GByte *)VSIMalloc(sizeof(GByte) * nCache);

        VSIFSeekL(fpInput, 0L, SEEK_END);

        size_t nCount = 0;
        const size_t nDataLength = VSIFTellL(fpInput);

        VSIFSeekL(fpInput, 0L, SEEK_SET);

        GUIntBig nCurOff = 0;

        while (nCount < nDataLength)
        {
            size_t nChunk = (size_t)MIN(nCache, nDataLength - nCount);

            size_t nSize = VSIFReadL(pBuffer, 1, nChunk, fpInput);

            if (nSize != nChunk)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "amount read differs from JPG length");
            }

            const auto nWrite =
                poStmt->WriteBlob(poLocator, (void *)pBuffer,
                                  static_cast<unsigned long>(nCurOff + 1),
                                  static_cast<unsigned long>(nSize));

            nCurOff += nWrite;
            nCount += nSize;

            pfnProgress((float)nCount / (float)nDataLength, nullptr,
                        pProgressData);
        }

        VSIFCloseL(fpInput);

        CPLFree(pBuffer);

        delete poStmt;

        return true;
    }

    if (poLocator)
    {
        OWStatement::Free(&poLocator, 1);
    }

    delete poStmt;

    return false;
}

//  ---------------------------------------------------------------------------
//                                                                GetFileList()
//  ---------------------------------------------------------------------------

char **GeoRasterDataset::GetFileList()
{
    char **papszFileList = nullptr;

    if (EQUAL(poGeoRaster->sCompressionType.c_str(), "JP2-F"))
    {
        CPLString osDSName;

        osDSName.Printf("/vsiocilob/%s,%s,%s,%s,%lld,noext",
                        this->poGeoRaster->poConnection->GetUser(),
                        this->poGeoRaster->poConnection->GetPassword(),
                        this->poGeoRaster->poConnection->GetServer(),
                        this->poGeoRaster->sDataTable.c_str(),
                        this->poGeoRaster->nRasterId);

        papszFileList = CSLAddString(papszFileList, osDSName.c_str());
    }

    return papszFileList;
}

//  ---------------------------------------------------------------------------
//                                                                     Create()
//  ---------------------------------------------------------------------------

GDALDataset *GeoRasterDataset::Create(const char *pszFilename, int nXSize,
                                      int nYSize, int nBands,
                                      GDALDataType eType, char **papszOptions)
{
    //  -------------------------------------------------------------------
    //  Check for supported Data types
    //  -------------------------------------------------------------------

    const char *pszCellDepth = OWSetDataType(eType);

    if (EQUAL(pszCellDepth, "Unknown"))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Attempt to create GeoRaster with unsupported data type (%s)",
                 GDALGetDataTypeName(eType));
        return nullptr;
    }

    //  -------------------------------------------------------------------
    //  Open the Dataset
    //  -------------------------------------------------------------------

    GeoRasterDataset *poGRD =
        (GeoRasterDataset *)GDALOpen(pszFilename, GA_Update);

    if (!poGRD)
    {
        return nullptr;
    }

    //  -------------------------------------------------------------------
    //  Get the GeoRaster
    //  -------------------------------------------------------------------

    GeoRasterWrapper *poGRW = poGRD->poGeoRaster;

    if (!poGRW)
    {
        delete poGRD;
        return nullptr;
    }

    //  -------------------------------------------------------------------
    //  Set basic information and default values
    //  -------------------------------------------------------------------

    poGRW->nRasterColumns = nXSize;
    poGRW->nRasterRows = nYSize;
    poGRW->nRasterBands = nBands;
    poGRW->sCellDepth = pszCellDepth;
    poGRW->nRowBlockSize = DEFAULT_BLOCK_ROWS;
    poGRW->nColumnBlockSize = DEFAULT_BLOCK_COLUMNS;
    poGRW->nBandBlockSize = 1;

    if (poGRW->bUniqueFound)
    {
        poGRW->PrepareToOverwrite();
    }

    //  -------------------------------------------------------------------
    //  Check the create options to use in initialization
    //  -------------------------------------------------------------------

    const char *pszFetched = "";
    CPLCharUniquePtr pszDescription;
    CPLCharUniquePtr pszInsert;
    int nQuality = -1;

    if (!poGRW->sTable.empty())
    {
        pszFetched = CSLFetchNameValue(papszOptions, "DESCRIPTION");

        if (pszFetched)
        {
            pszDescription.reset(CPLStrdup(pszFetched));
        }
    }

    if (poGRW->sTable.empty())
    {
        poGRW->sTable = "GDAL_IMPORT";
        poGRW->sDataTable = "GDAL_RDT";
    }

    if (poGRW->sColumn.empty())
    {
        poGRW->sColumn = "RASTER";
    }

    pszFetched = CSLFetchNameValue(papszOptions, "INSERT");

    if (pszFetched)
    {
        pszInsert.reset(CPLStrdup(pszFetched));
    }

    pszFetched = CSLFetchNameValue(papszOptions, "BLOCKXSIZE");

    if (pszFetched)
    {
        poGRW->nColumnBlockSize = atoi(pszFetched);
    }

    pszFetched = CSLFetchNameValue(papszOptions, "BLOCKYSIZE");

    if (pszFetched)
    {
        poGRW->nRowBlockSize = atoi(pszFetched);
    }

    pszFetched = CSLFetchNameValue(papszOptions, "NBITS");

    if (pszFetched != nullptr)
    {
        poGRW->sCellDepth = CPLSPrintf("%dBIT", atoi(pszFetched));
    }

    pszFetched = CSLFetchNameValue(papszOptions, "COMPRESS");

    if (pszFetched != nullptr &&
        (EQUAL(pszFetched, "JPEG-F") || EQUAL(pszFetched, "JP2-F") ||
         EQUAL(pszFetched, "DEFLATE")))
    {
        poGRW->sCompressionType = pszFetched;
    }
    else
    {
        poGRW->sCompressionType = "NONE";
    }

    pszFetched = CSLFetchNameValue(papszOptions, "QUALITY");

    if (pszFetched)
    {
        poGRW->nCompressQuality = atoi(pszFetched);
        nQuality = poGRW->nCompressQuality;
    }

    pszFetched = CSLFetchNameValue(papszOptions, "INTERLEAVE");

    bool bInterleve_ind = false;

    if (pszFetched)
    {
        bInterleve_ind = true;

        if (EQUAL(pszFetched, "BAND") || EQUAL(pszFetched, "BSQ"))
        {
            poGRW->sInterleaving = "BSQ";
        }
        if (EQUAL(pszFetched, "LINE") || EQUAL(pszFetched, "BIL"))
        {
            poGRW->sInterleaving = "BIL";
        }
        if (EQUAL(pszFetched, "PIXEL") || EQUAL(pszFetched, "BIP"))
        {
            poGRW->sInterleaving = "BIP";
        }
    }
    else
    {
        if (EQUAL(poGRW->sCompressionType.c_str(), "NONE") == false)
        {
            poGRW->sInterleaving = "BIP";
        }
    }

    pszFetched = CSLFetchNameValue(papszOptions, "BLOCKBSIZE");

    if (pszFetched)
    {
        poGRW->nBandBlockSize = atoi(pszFetched);
    }
    else
    {
        if (nBands == 3 || nBands == 4)
        {
            poGRW->nBandBlockSize = nBands;
        }
    }

    if (bInterleve_ind == false &&
        (poGRW->nBandBlockSize == 3 || poGRW->nBandBlockSize == 4))
    {
        poGRW->sInterleaving = "BIP";
    }

    if (STARTS_WITH_CI(poGRW->sCompressionType.c_str(), "JPEG"))
    {
        if (!EQUAL(poGRW->sInterleaving.c_str(), "BIP"))
        {
            CPLError(CE_Warning, CPLE_IllegalArg,
                     "compress=JPEG assumes interleave=BIP");
            poGRW->sInterleaving = "BIP";
        }
    }

    pszFetched = CSLFetchNameValue(papszOptions, "BLOCKING");

    if (pszFetched)
    {
        if (EQUAL(pszFetched, "NO"))
        {
            poGRW->bBlocking = false;
        }

        if (EQUAL(pszFetched, "OPTIMALPADDING"))
        {
            if (poGRW->poConnection->GetVersion() < 11)
            {
                CPLError(CE_Warning, CPLE_IllegalArg,
                         "BLOCKING=OPTIMALPADDING not supported on Oracle "
                         "older than 11g");
            }
            else
            {
                poGRW->bAutoBlocking = true;
                poGRW->bBlocking = true;
            }
        }
    }

    //  -------------------------------------------------------------------
    //  Validate options
    //  -------------------------------------------------------------------

    if (pszDescription.get() && poGRW->bUniqueFound)
    {
        CPLError(
            CE_Failure, CPLE_IllegalArg,
            "Option (DESCRIPTION) cannot be used on a existing GeoRaster.");
        delete poGRD;
        return nullptr;
    }

    if (pszInsert && poGRW->bUniqueFound)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "Option (INSERT) cannot be used on a existing GeoRaster.");
        delete poGRD;
        return nullptr;
    }

    /* Compression JPEG-B is deprecated. It should be able to read but not
     * to create new GeoRaster on databases with that compression option.
     *
     * TODO: Remove that options on next release.
     */

    if (EQUAL(poGRW->sCompressionType.c_str(), "JPEG-B"))
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "Option (COMPRESS=%s) is deprecated and cannot be used.",
                 poGRW->sCompressionType.c_str());
        delete poGRD;
        return nullptr;
    }

    if (EQUAL(poGRW->sCompressionType.c_str(), "JPEG-F"))
    {
        /* JPEG-F can only compress byte data type
         */
        if (eType != GDT_Byte)
        {
            CPLError(
                CE_Failure, CPLE_IllegalArg,
                "Option (COMPRESS=%s) can only be used with Byte data type.",
                poGRW->sCompressionType.c_str());
            delete poGRD;
            return nullptr;
        }

        /* JPEG-F can compress one band per block or 3 for RGB
         * or 4 for RGBA.
         */
        if ((poGRW->nBandBlockSize != 1 && poGRW->nBandBlockSize != 3 &&
             poGRW->nBandBlockSize != 4) ||
            ((poGRW->nBandBlockSize != 1 &&
              (poGRW->nBandBlockSize != poGRW->nRasterBands))))
        {
            CPLError(
                CE_Failure, CPLE_IllegalArg,
                "Option (COMPRESS=%s) requires BLOCKBSIZE to be 1 (for any "
                "number of bands), 3 (for 3 bands RGB) and 4 (for 4 bands "
                "RGBA).",
                poGRW->sCompressionType.c_str());
            delete poGRD;
            return nullptr;
        }

        // There is a limit on how big a compressed block can be.
        if ((poGRW->nColumnBlockSize * poGRW->nRowBlockSize *
             poGRW->nBandBlockSize * (GDALGetDataTypeSize(eType) / 8)) >
            (50 * 1024 * 1024))
        {
            CPLError(
                CE_Failure, CPLE_IllegalArg,
                "Option (COMPRESS=%s) each data block must not exceed 50Mb. "
                "Consider reducing BLOCK{X,Y,B}XSIZE.",
                poGRW->sCompressionType.c_str());
            delete poGRD;
            return nullptr;
        }
    }

    if (EQUAL(poGRW->sCompressionType.c_str(), "DEFLATE"))
    {
        if ((poGRW->nColumnBlockSize * poGRW->nRowBlockSize *
             poGRW->nBandBlockSize * (GDALGetDataTypeSize(eType) / 8)) >
            (1024 * 1024 * 1024))
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "For (COMPRESS=%s) each data block must not exceed 1Gb. "
                     "Consider reducing BLOCK{X,Y,B}XSIZE.",
                     poGRW->sCompressionType.c_str());
            delete poGRD;
            return nullptr;
        }
    }

    // When the compression is JP2-F it should be just one block

    if (EQUAL(poGRW->sCompressionType.c_str(), "JP2-F"))
    {
        poGRW->nRowBlockSize = poGRW->nRasterRows;
        poGRW->nColumnBlockSize = poGRW->nRasterColumns;
        poGRW->nBandBlockSize = poGRW->nRasterBands;
        poGRW->bBlocking = false;
    }

    pszFetched = CSLFetchNameValue(papszOptions, "OBJECTTABLE");

    if (pszFetched)
    {
        int nVersion = poGRW->poConnection->GetVersion();
        if (nVersion <= 11)
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "Driver create-option OBJECTTABLE not "
                     "supported on Oracle %d",
                     nVersion);
            delete poGRD;
            return nullptr;
        }
    }

    poGRD->poGeoRaster->bCreateObjectTable =
        CPLFetchBool(papszOptions, "OBJECTTABLE", false);

    //  -------------------------------------------------------------------
    //  Create a SDO_GEORASTER object on the server
    //  -------------------------------------------------------------------

    const bool bSuccess = poGRW->Create(pszDescription.get(), pszInsert.get(),
                                        poGRW->bUniqueFound);

    if (!bSuccess)
    {
        delete poGRD;
        return nullptr;
    }

    //  -------------------------------------------------------------------
    //  Prepare an identification string
    //  -------------------------------------------------------------------

    char szStringId[OWTEXT];

    snprintf(szStringId, sizeof(szStringId), "georaster:%s,%s,%s,%s,%lld",
             poGRW->poConnection->GetUser(), poGRW->poConnection->GetPassword(),
             poGRW->poConnection->GetServer(), poGRW->sDataTable.c_str(),
             poGRW->nRasterId);

    delete poGRD;

    poGRD = (GeoRasterDataset *)GDALOpen(szStringId, GA_Update);

    if (!poGRD)
    {
        return nullptr;
    }

    //  -------------------------------------------------------------------
    //  Load additional options
    //  -------------------------------------------------------------------

    pszFetched = CSLFetchNameValue(papszOptions, "VATNAME");

    if (pszFetched)
    {
        poGRW->sValueAttributeTab = pszFetched;
    }

    pszFetched = CSLFetchNameValue(papszOptions, "SRID");

    if (pszFetched)
    {
        poGRD->bForcedSRID = true;
        poGRD->poGeoRaster->SetGeoReference(atoi(pszFetched));
    }

    poGRD->poGeoRaster->bGenSpatialExtent =
        CPLFetchBool(papszOptions, "SPATIALEXTENT", TRUE);

    pszFetched = CSLFetchNameValue(papszOptions, "EXTENTSRID");

    if (pszFetched)
    {
        poGRD->poGeoRaster->nExtentSRID = atoi(pszFetched);
    }

    pszFetched = CSLFetchNameValue(papszOptions, "COORDLOCATION");

    if (pszFetched)
    {
        if (EQUAL(pszFetched, "CENTER"))
        {
            poGRD->poGeoRaster->eModelCoordLocation = MCL_CENTER;
        }
        else if (EQUAL(pszFetched, "UPPERLEFT"))
        {
            poGRD->poGeoRaster->eModelCoordLocation = MCL_UPPERLEFT;
        }
        else
        {
            CPLError(CE_Warning, CPLE_IllegalArg,
                     "Incorrect COORDLOCATION (%s)", pszFetched);
        }
    }

    if (nQuality > 0)
    {
        poGRD->poGeoRaster->nCompressQuality = nQuality;
    }

    pszFetched = CSLFetchNameValue(papszOptions, "GENPYRAMID");

    if (pszFetched != nullptr)
    {
        if (!(EQUAL(pszFetched, "NN") || EQUAL(pszFetched, "BILINEAR") ||
              EQUAL(pszFetched, "BIQUADRATIC") || EQUAL(pszFetched, "CUBIC") ||
              EQUAL(pszFetched, "AVERAGE4") || EQUAL(pszFetched, "AVERAGE16")))
        {
            CPLError(CE_Warning, CPLE_IllegalArg,
                     "Wrong resample method for pyramid (%s)", pszFetched);
        }

        poGRD->poGeoRaster->bGenPyramid = true;
        poGRD->poGeoRaster->sPyramidResampling = pszFetched;
    }

    pszFetched = CSLFetchNameValue(papszOptions, "GENPYRLEVELS");

    if (pszFetched != nullptr)
    {
        poGRD->poGeoRaster->bGenPyramid = true;
        poGRD->poGeoRaster->nPyramidLevels = atoi(pszFetched);
    }

    //  -------------------------------------------------------------------
    //  Return a new Dataset
    //  -------------------------------------------------------------------

    return (GDALDataset *)poGRD;
}

//  ---------------------------------------------------------------------------
//                                                                 CreateCopy()
//  ---------------------------------------------------------------------------

GDALDataset *GeoRasterDataset::CreateCopy(const char *pszFilename,
                                          GDALDataset *poSrcDS, int bStrict,
                                          char **papszOptions,
                                          GDALProgressFunc pfnProgress,
                                          void *pProgressData)
{
    (void)bStrict;

    int nBands = poSrcDS->GetRasterCount();
    if (nBands == 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GeoRaster driver does not support source dataset with zero "
                 "band.\n");
        return nullptr;
    }

    GDALRasterBand *poBand = poSrcDS->GetRasterBand(1);
    GDALDataType eType = poBand->GetRasterDataType();

    //  -----------------------------------------------------------
    //  Create a GeoRaster on the server or select one to overwrite
    //  -----------------------------------------------------------

    GeoRasterDataset *poDstDS = (GeoRasterDataset *)GeoRasterDataset::Create(
        pszFilename, poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize(),
        poSrcDS->GetRasterCount(), eType, papszOptions);

    if (poDstDS == nullptr)
    {
        return nullptr;
    }

    //  -----------------------------------------------------------
    //  Copy information to the dataset
    //  -----------------------------------------------------------

    double adfTransform[6];

    if (poSrcDS->GetGeoTransform(adfTransform) == CE_None)
    {
        if (!(adfTransform[0] == 0.0 && adfTransform[1] == 1.0 &&
              adfTransform[2] == 0.0 && adfTransform[3] == 0.0 &&
              adfTransform[4] == 0.0 && adfTransform[5] == 1.0))
        {
            poDstDS->SetGeoTransform(adfTransform);

            if (!poDstDS->bForcedSRID) /* forced by create option SRID */
            {
                poDstDS->SetSpatialRef(poSrcDS->GetSpatialRef());
            }
        }
    }

    // --------------------------------------------------------------------
    //      Copy GCPs
    // --------------------------------------------------------------------

    if (poSrcDS->GetGCPCount() > 0)
    {
        poDstDS->SetGCPs(poSrcDS->GetGCPCount(), poSrcDS->GetGCPs(),
                         poSrcDS->GetGCPSpatialRef());
    }

    // --------------------------------------------------------------------
    //      Copy RPC
    // --------------------------------------------------------------------

    char **papszRPCMetadata = GDALGetMetadata(poSrcDS, "RPC");

    if (papszRPCMetadata != nullptr)
    {
        poDstDS->poGeoRaster->phRPC =
            (GDALRPCInfoV2 *)VSICalloc(1, sizeof(GDALRPCInfoV2));
        CPL_IGNORE_RET_VAL(GDALExtractRPCInfoV2(papszRPCMetadata,
                                                poDstDS->poGeoRaster->phRPC));
    }

    // --------------------------------------------------------------------
    //      Copy information to the raster bands
    // --------------------------------------------------------------------

    for (int iBand = 1; iBand <= poSrcDS->GetRasterCount(); iBand++)
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand(iBand);
        GeoRasterRasterBand *poDstBand =
            (GeoRasterRasterBand *)poDstDS->GetRasterBand(iBand);

        // ----------------------------------------------------------------
        //  Copy Color Table
        // ----------------------------------------------------------------

        GDALColorTable *poColorTable = poSrcBand->GetColorTable();

        if (poColorTable)
        {
            poDstBand->SetColorTable(poColorTable);
        }

        // ----------------------------------------------------------------
        //  Copy statistics information, without median and mode.
        // ----------------------------------------------------------------

        {
            double dfMin = 0.0;
            double dfMax = 0.0;
            double dfMean = 0.0;
            double dfStdDev = 0.0;
            if (poSrcBand->GetStatistics(false, false, &dfMin, &dfMax, &dfMean,
                                         &dfStdDev) == CE_None)
            {
                poDstBand->SetStatistics(dfMin, dfMax, dfMean, dfStdDev);

                /* That will not be recorded in the GeoRaster metadata since it
                 * doesn't have median and mode, so those values are only useful
                 * at runtime.
                 */
            }
        }

        // ----------------------------------------------------------------
        //  Copy statistics metadata information, including median and mode.
        // ----------------------------------------------------------------

        const char *pszMin = poSrcBand->GetMetadataItem("STATISTICS_MINIMUM");
        const char *pszMax = poSrcBand->GetMetadataItem("STATISTICS_MAXIMUM");
        const char *pszMean = poSrcBand->GetMetadataItem("STATISTICS_MEAN");
        const char *pszMedian = poSrcBand->GetMetadataItem("STATISTICS_MEDIAN");
        const char *pszMode = poSrcBand->GetMetadataItem("STATISTICS_MODE");
        const char *pszStdDev = poSrcBand->GetMetadataItem("STATISTICS_STDDEV");
        const char *pszSkipFX =
            poSrcBand->GetMetadataItem("STATISTICS_SKIPFACTORX");
        const char *pszSkipFY =
            poSrcBand->GetMetadataItem("STATISTICS_SKIPFACTORY");

        if (pszMin != nullptr && pszMax != nullptr && pszMean != nullptr &&
            pszMedian != nullptr && pszMode != nullptr && pszStdDev != nullptr)
        {
            const double dfMin = CPLScanDouble(pszMin, MAX_DOUBLE_STR_REP);
            const double dfMax = CPLScanDouble(pszMax, MAX_DOUBLE_STR_REP);
            const double dfMean = CPLScanDouble(pszMean, MAX_DOUBLE_STR_REP);
            const double dfMedian =
                CPLScanDouble(pszMedian, MAX_DOUBLE_STR_REP);
            const double dfMode = CPLScanDouble(pszMode, MAX_DOUBLE_STR_REP);

            if (!((dfMin > dfMax) || (dfMean > dfMax) || (dfMean < dfMin) ||
                  (dfMedian > dfMax) || (dfMedian < dfMin) ||
                  (dfMode > dfMax) || (dfMode < dfMin)))
            {
                if (!pszSkipFX)
                {
                    pszSkipFX = pszSkipFY != nullptr ? pszSkipFY : "1";
                }

                poDstBand->poGeoRaster->SetStatistics(
                    iBand, pszMin, pszMax, pszMean, pszMedian, pszMode,
                    pszStdDev, pszSkipFX);
            }
        }

        // ----------------------------------------------------------------
        //  Copy Raster Attribute Table (RAT)
        // ----------------------------------------------------------------

        GDALRasterAttributeTableH poRAT = GDALGetDefaultRAT(poSrcBand);

        if (poRAT != nullptr)
        {
            poDstBand->SetDefaultRAT((GDALRasterAttributeTable *)poRAT);
        }

        // ----------------------------------------------------------------
        //  Copy NoData Value
        // ----------------------------------------------------------------
        int bHasNoDataValue = FALSE;
        const double dfNoDataValue =
            poSrcBand->GetNoDataValue(&bHasNoDataValue);

        if (bHasNoDataValue)
        {
            poDstBand->SetNoDataValue(dfNoDataValue);
        }
    }

    // --------------------------------------------------------------------
    //  Copy actual imagery.
    // --------------------------------------------------------------------

    int nXSize = poDstDS->GetRasterXSize();
    int nYSize = poDstDS->GetRasterYSize();

    int nBlockXSize = 0;
    int nBlockYSize = 0;

    poDstDS->GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);

    // --------------------------------------------------------------------
    //  JP2-F has one block with full image size. Use tile size instead
    // --------------------------------------------------------------------

    const char *pszFetched = CSLFetchNameValue(papszOptions, "COMPRESS");

    if (pszFetched != nullptr && EQUAL(pszFetched, "JP2-F"))
    {
        nBlockXSize = DEFAULT_JP2_TILE_COLUMNS;
        nBlockYSize = DEFAULT_JP2_TILE_ROWS;
        pszFetched = CSLFetchNameValue(papszOptions, "JP2_BLOCKXSIZE");
        if (pszFetched != nullptr)
        {
            nBlockXSize = atoi(pszFetched);
        }
        pszFetched = CSLFetchNameValue(papszOptions, "JP2_BLOCKYSIZE");
        if (pszFetched != nullptr)
        {
            nBlockYSize = atoi(pszFetched);
        }
    }

    // --------------------------------------------------------------------
    //  Allocate memory buffer to read one block from one band
    // --------------------------------------------------------------------

    void *pData = VSI_MALLOC3_VERBOSE(nBlockXSize, nBlockYSize,
                                      GDALGetDataTypeSizeBytes(eType));

    if (pData == nullptr)
    {
        delete poDstDS;
        return nullptr;
    }

    CPLErr eErr = CE_None;

    int nPixelSize =
        GDALGetDataTypeSize(poSrcDS->GetRasterBand(1)->GetRasterDataType()) / 8;

    if (EQUAL(poDstDS->poGeoRaster->sCompressionType.c_str(), "JPEG-F") &&
        nBlockXSize == nXSize && nBlockYSize == nYSize)
    {
        // --------------------------------------------------------------------
        // Load JPEG avoiding decompression/compression - direct copy
        // --------------------------------------------------------------------

        const char *pszDriverName = poSrcDS->GetDriverName();

        if (EQUAL(pszDriverName, "JPEG"))
        {
            char **papszFileList = poSrcDS->GetFileList();

            if (poDstDS->JPEG_CopyDirect(papszFileList[0], pfnProgress,
                                         pProgressData))
            {
                CPLDebug("GEOR", "JPEG Direct copy succeed");
            }
        }
    }
    else if (EQUAL(poDstDS->poGeoRaster->sCompressionType.c_str(), "JP2-F"))
    {
        // --------------------------------------------------------------------
        // Load JP2K avoiding decompression/compression - direct copy
        // --------------------------------------------------------------------

        boolean bJP2CopyDirectSucceed = false;

        const char *pszDriverName = poSrcDS->GetDriverName();

        int nJP2Resolution = -1;

        if (EQUAL(pszDriverName, "JP2OpenJPEG") &&
            poSrcDS->GetRasterBand(1)->GetColorTable() == nullptr)
        {
            //  ---------------------------------------------------------------
            //  Try to load the JP2 file directly
            //  ---------------------------------------------------------------

            char **papszFileList = poSrcDS->GetFileList();

            bJP2CopyDirectSucceed = poDstDS->JP2_CopyDirect(
                papszFileList[0], &nJP2Resolution, pfnProgress, pProgressData);
        }

        if (!bJP2CopyDirectSucceed)
        {
            //  ---------------------------------------------------------------
            //  Use VSIOCILOB to load using a resident JP2 driver
            //  ---------------------------------------------------------------

            poDstDS->JP2_CreateCopy(poSrcDS,         /* JP2 dataset */
                                    papszOptions,    /* options list */
                                    &nJP2Resolution, /* returned resolution */
                                    pfnProgress,     /* progress function */
                                    pProgressData);  /* progress data */
        }

        // Number of pyramid levels is the number of resolutions - 1

        poDstDS->poGeoRaster->SetMaxLevel(MAX(1, nJP2Resolution - 1));
    }
    else if (poDstDS->poGeoRaster->nBandBlockSize == 1)
    {
        // ----------------------------------------------------------------
        //  Band order
        // ----------------------------------------------------------------

        int nBandCount = poSrcDS->GetRasterCount();

        for (int iBand = 1; iBand <= nBandCount; iBand++)
        {
            GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand(iBand);
            GDALRasterBand *poDstBand = poDstDS->GetRasterBand(iBand);

            for (int iYOffset = 0, iYBlock = 0; iYOffset < nYSize;
                 iYOffset += nBlockYSize, iYBlock++)
            {
                const int nBlockRows = MIN(nBlockYSize, nYSize - iYOffset);
                for (int iXOffset = 0, iXBlock = 0; iXOffset < nXSize;
                     iXOffset += nBlockXSize, iXBlock++)
                {

                    const int nBlockCols = MIN(nBlockXSize, nXSize - iXOffset);

                    eErr = poSrcBand->RasterIO(
                        GF_Read, iXOffset, iYOffset, nBlockCols, nBlockRows,
                        pData, nBlockCols, nBlockRows, eType, nPixelSize,
                        nPixelSize * nBlockXSize, nullptr);

                    if (eErr != CE_None)
                    {
                        return nullptr;
                    }

                    eErr = poDstBand->WriteBlock(iXBlock, iYBlock, pData);

                    if (eErr != CE_None)
                    {
                        return nullptr;
                    }
                }

                if ((eErr == CE_None) &&
                    (!pfnProgress(((iBand - 1) / (float)nBandCount) +
                                      (iYOffset + nBlockRows) /
                                          (float)(nYSize * nBandCount),
                                  nullptr, pProgressData)))
                {
                    eErr = CE_Failure;
                    CPLError(CE_Failure, CPLE_UserInterrupt,
                             "User terminated CreateCopy()");
                }
            }
        }
    }
    else
    {
        // ----------------------------------------------------------------
        //  Block order
        // ----------------------------------------------------------------

        poDstDS->poGeoRaster->SetWriteOnly(true);

        for (int iYOffset = 0, iYBlock = 0; iYOffset < nYSize;
             iYOffset += nBlockYSize, iYBlock++)
        {
            const int nBlockRows = MIN(nBlockYSize, nYSize - iYOffset);
            for (int iXOffset = 0, iXBlock = 0; iXOffset < nXSize;
                 iXOffset += nBlockXSize, iXBlock++)
            {
                const int nBlockCols = MIN(nBlockXSize, nXSize - iXOffset);

                for (int iBand = 1; iBand <= poSrcDS->GetRasterCount(); iBand++)
                {
                    GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand(iBand);
                    GDALRasterBand *poDstBand = poDstDS->GetRasterBand(iBand);

                    eErr = poSrcBand->RasterIO(
                        GF_Read, iXOffset, iYOffset, nBlockCols, nBlockRows,
                        pData, nBlockCols, nBlockRows, eType, nPixelSize,
                        nPixelSize * nBlockXSize, nullptr);

                    if (eErr != CE_None)
                    {
                        return nullptr;
                    }

                    eErr = poDstBand->WriteBlock(iXBlock, iYBlock, pData);

                    if (eErr != CE_None)
                    {
                        return nullptr;
                    }
                }
            }

            if ((eErr == CE_None) &&
                (!pfnProgress((iYOffset + nBlockRows) / (double)nYSize, nullptr,
                              pProgressData)))
            {
                eErr = CE_Failure;
                CPLError(CE_Failure, CPLE_UserInterrupt,
                         "User terminated CreateCopy()");
            }
        }
    }

    CPLFree(pData);

    // --------------------------------------------------------------------
    //      Finalize
    // --------------------------------------------------------------------

    poDstDS->FlushCache(false);

    if (pfnProgress)
    {
        CPLDebug("GEOR",
                 "Output dataset: (georaster:%s/%s@%s,%s,%lld) on %s%s,%s",
                 poDstDS->poGeoRaster->poConnection->GetUser(),
                 poDstDS->poGeoRaster->poConnection->GetPassword(),
                 poDstDS->poGeoRaster->poConnection->GetServer(),
                 poDstDS->poGeoRaster->sDataTable.c_str(),
                 poDstDS->poGeoRaster->nRasterId,
                 poDstDS->poGeoRaster->sSchema.c_str(),
                 poDstDS->poGeoRaster->sTable.c_str(),
                 poDstDS->poGeoRaster->sColumn.c_str());
    }

    return poDstDS;
}

//  ---------------------------------------------------------------------------
//                                                                  IRasterIO()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterDataset::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                                   int nXSize, int nYSize, void *pData,
                                   int nBufXSize, int nBufYSize,
                                   GDALDataType eBufType, int nBandCount,
                                   int *panBandMap, GSpacing nPixelSpace,
                                   GSpacing nLineSpace, GSpacing nBandSpace,
                                   GDALRasterIOExtraArg *psExtraArg)

{
    if (EQUAL(poGeoRaster->sCompressionType.c_str(), "JP2-F"))
    {
        if (poJP2Dataset)
        {
            return poJP2Dataset->RasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                          pData, nBufXSize, nBufYSize, eBufType,
                                          nBandCount, panBandMap, nPixelSpace,
                                          nLineSpace, nBandSpace, psExtraArg);
        }
        else
        {
            return CE_Failure;
        }
    }
    else
    {
        if (poGeoRaster->nBandBlockSize > 1)
        {
            return GDALDataset::BlockBasedRasterIO(
                eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize,
                nBufYSize, eBufType, nBandCount, panBandMap, nPixelSpace,
                nLineSpace, nBandSpace, psExtraArg);
        }
        else
        {
            return GDALDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                          pData, nBufXSize, nBufYSize, eBufType,
                                          nBandCount, panBandMap, nPixelSpace,
                                          nLineSpace, nBandSpace, psExtraArg);
        }
    }
}

//  ---------------------------------------------------------------------------
//                                                  FlushCache(bool bAtClosing)
//  ---------------------------------------------------------------------------

void GeoRasterDataset::FlushCache(bool bAtClosing)
{
    GDALDataset::FlushCache(bAtClosing);
}

//  ---------------------------------------------------------------------------
//                                                            GetGeoTransform()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterDataset::GetGeoTransform(double *padfTransform)
{
    if (poGeoRaster->phRPC)
    {
        return CE_Failure;
    }

    if (poGeoRaster->nSRID == 0)
    {
        return CE_Failure;
    }

    memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6);

    bGeoTransform = true;

    return CE_None;
}

//  ---------------------------------------------------------------------------
//                                                              GetSpatialRef()
//  ---------------------------------------------------------------------------

const OGRSpatialReference *GeoRasterDataset::GetSpatialRef() const
{
    if (poGeoRaster->phRPC)
    {
        return nullptr;
    }

    if (!poGeoRaster->bIsReferenced)
    {
        return nullptr;
    }

    if (poGeoRaster->nSRID == UNKNOWN_CRS || poGeoRaster->nSRID == 0)
    {
        return nullptr;
    }

    if (!m_oSRS.IsEmpty())
    {
        return &m_oSRS;
    }

    OGRSpatialReference oSRS;
    oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    // --------------------------------------------------------------------
    // Check if the SRID is a valid EPSG code
    // --------------------------------------------------------------------

    CPLPushErrorHandler(CPLQuietErrorHandler);

    if (oSRS.importFromEPSG(static_cast<int>(poGeoRaster->nSRID)) ==
        OGRERR_NONE)
    {
        CPLPopErrorHandler();

        /*
         * Ignores the WKT from Oracle and use the one from GDAL's
         * EPSG tables. That would ensure that other drivers/software
         * will recognize the parameters.
         */
        m_oSRS = oSRS;
        return &m_oSRS;
    }

    CPLPopErrorHandler();

    // --------------------------------------------------------------------
    // Try to interpreter the WKT text
    // --------------------------------------------------------------------

    poGeoRaster->QueryWKText();

    if (!(oSRS.importFromWkt(poGeoRaster->sWKText) == OGRERR_NONE &&
          oSRS.GetRoot()))
    {
        m_oSRS = oSRS;
        return &m_oSRS;
    }

    // ----------------------------------------------------------------
    // Decorate with Authority name
    // ----------------------------------------------------------------

    if (strlen(poGeoRaster->sAuthority) > 0)
    {
        oSRS.SetAuthority(oSRS.GetRoot()->GetValue(),
                          poGeoRaster->sAuthority.c_str(),
                          static_cast<int>(poGeoRaster->nSRID));
    }

    int nSpher = OWParseEPSG(oSRS.GetAttrValue("GEOGCS|DATUM|SPHEROID"));

    if (nSpher > 0)
    {
        oSRS.SetAuthority("GEOGCS|DATUM|SPHEROID", "EPSG", nSpher);
    }

    int nDatum = OWParseEPSG(oSRS.GetAttrValue("GEOGCS|DATUM"));

    if (nDatum > 0)
    {
        oSRS.SetAuthority("GEOGCS|DATUM", "EPSG", nDatum);
    }

    // ----------------------------------------------------------------
    // Checks for Projection info
    // ----------------------------------------------------------------

    const char *pszProjName = oSRS.GetAttrValue("PROJECTION");

    if (pszProjName)
    {
        int nProj = OWParseEPSG(pszProjName);

        // ----------------------------------------------------------------
        // Decorate with EPSG Authority
        // ----------------------------------------------------------------

        if (nProj > 0)
        {
            oSRS.SetAuthority("PROJECTION", "EPSG", nProj);
        }

        // ----------------------------------------------------------------
        // Translate projection names to GDAL's standards
        // ----------------------------------------------------------------

        if (EQUAL(pszProjName, "Transverse Mercator"))
        {
            oSRS.SetProjection(SRS_PT_TRANSVERSE_MERCATOR);
        }
        else if (EQUAL(pszProjName, "Albers Conical Equal Area"))
        {
            oSRS.SetProjection(SRS_PT_ALBERS_CONIC_EQUAL_AREA);
        }
        else if (EQUAL(pszProjName, "Azimuthal Equidistant"))
        {
            oSRS.SetProjection(SRS_PT_AZIMUTHAL_EQUIDISTANT);
        }
        else if (EQUAL(pszProjName, "Miller Cylindrical"))
        {
            oSRS.SetProjection(SRS_PT_MILLER_CYLINDRICAL);
        }
        else if (EQUAL(pszProjName, "Hotine Oblique Mercator"))
        {
            oSRS.SetProjection(SRS_PT_HOTINE_OBLIQUE_MERCATOR);
        }
        else if (EQUAL(pszProjName, "Wagner IV"))
        {
            oSRS.SetProjection(SRS_PT_WAGNER_IV);
        }
        else if (EQUAL(pszProjName, "Wagner VII"))
        {
            oSRS.SetProjection(SRS_PT_WAGNER_VII);
        }
        else if (EQUAL(pszProjName, "Eckert IV"))
        {
            oSRS.SetProjection(SRS_PT_ECKERT_IV);
        }
        else if (EQUAL(pszProjName, "Eckert VI"))
        {
            oSRS.SetProjection(SRS_PT_ECKERT_VI);
        }
        else if (EQUAL(pszProjName, "New Zealand Map Grid"))
        {
            oSRS.SetProjection(SRS_PT_NEW_ZEALAND_MAP_GRID);
        }
        else if (EQUAL(pszProjName, "Lambert Conformal Conic"))
        {
            oSRS.SetProjection(SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP);
        }
        else if (EQUAL(pszProjName, "Lambert Azimuthal Equal Area"))
        {
            oSRS.SetProjection(SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA);
        }
        else if (EQUAL(pszProjName, "Van der Grinten"))
        {
            oSRS.SetProjection(SRS_PT_VANDERGRINTEN);
        }
        else if (EQUAL(pszProjName, "Lambert Conformal Conic (Belgium 1972)"))
        {
            oSRS.SetProjection(SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM);
        }
        else if (EQUAL(pszProjName, "Cylindrical Equal Area"))
        {
            oSRS.SetProjection(SRS_PT_CYLINDRICAL_EQUAL_AREA);
        }
        else if (EQUAL(pszProjName, "Interrupted Goode Homolosine"))
        {
            oSRS.SetProjection(SRS_PT_GOODE_HOMOLOSINE);
        }
    }

    m_oSRS = oSRS;
    return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
}

//  ---------------------------------------------------------------------------
//                                                            SetGeoTransform()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterDataset::SetGeoTransform(double *padfTransform)
{
    memcpy(adfGeoTransform, padfTransform, sizeof(double) * 6);

    poGeoRaster->dfXCoefficient[0] = adfGeoTransform[1];
    poGeoRaster->dfXCoefficient[1] = adfGeoTransform[2];
    poGeoRaster->dfXCoefficient[2] = adfGeoTransform[0];
    poGeoRaster->dfYCoefficient[0] = adfGeoTransform[4];
    poGeoRaster->dfYCoefficient[1] = adfGeoTransform[5];
    poGeoRaster->dfYCoefficient[2] = adfGeoTransform[3];

    bGeoTransform = true;

    return CE_None;
}

//  ---------------------------------------------------------------------------
//                                                              SetSpatialRef()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterDataset::SetSpatialRef(const OGRSpatialReference *poSRS)
{
    m_oSRS.Clear();
    if (poSRS == nullptr)
    {
        poGeoRaster->SetGeoReference(UNKNOWN_CRS);

        return CE_Failure;
    }

    // --------------------------------------------------------------------
    // Try to extract EPGS authority code
    // --------------------------------------------------------------------

    const char *pszAuthName = nullptr;
    const char *pszAuthCode = nullptr;

    if (poSRS->IsGeographic())
    {
        pszAuthName = poSRS->GetAuthorityName("GEOGCS");
        pszAuthCode = poSRS->GetAuthorityCode("GEOGCS");
    }
    else if (poSRS->IsProjected())
    {
        pszAuthName = poSRS->GetAuthorityName("PROJCS");
        pszAuthCode = poSRS->GetAuthorityCode("PROJCS");
    }

    if (pszAuthName != nullptr && pszAuthCode != nullptr)
    {
        if (EQUAL(pszAuthName, "ORACLE") || EQUAL(pszAuthName, "EPSG"))
        {
            poGeoRaster->SetGeoReference(atoi(pszAuthCode));
            m_oSRS = *poSRS;
            return CE_None;
        }
    }

    // ----------------------------------------------------------------
    // Convert SRS into old style format (SF-SQL 1.0)
    // ----------------------------------------------------------------

    std::unique_ptr<OGRSpatialReference> poSRS2(poSRS->Clone());

    double dfAngularUnits = poSRS2->GetAngularUnits(nullptr);

    if (fabs(dfAngularUnits - 0.0174532925199433) < 0.0000000000000010)
    {
        /* match the precision used on Oracle for that particular value */

        poSRS2->SetAngularUnits("Decimal Degree", 0.0174532925199433);
    }

    char *pszCloneWKT = nullptr;

    const char *const apszOptions[] = {"FORMAT=SFSQL", nullptr};
    if (poSRS2->exportToWkt(&pszCloneWKT, apszOptions) != OGRERR_NONE)
    {
        CPLFree(pszCloneWKT);
        return CE_Failure;
    }

    const char *pszProjName = poSRS2->GetAttrValue("PROJECTION");

    if (pszProjName)
    {
        // ----------------------------------------------------------------
        // Translate projection names to Oracle's standards
        // ----------------------------------------------------------------

        if (EQUAL(pszProjName, SRS_PT_TRANSVERSE_MERCATOR))
        {
            poSRS2->SetProjection("Transverse Mercator");
        }
        else if (EQUAL(pszProjName, SRS_PT_ALBERS_CONIC_EQUAL_AREA))
        {
            poSRS2->SetProjection("Albers Conical Equal Area");
        }
        else if (EQUAL(pszProjName, SRS_PT_AZIMUTHAL_EQUIDISTANT))
        {
            poSRS2->SetProjection("Azimuthal Equidistant");
        }
        else if (EQUAL(pszProjName, SRS_PT_MILLER_CYLINDRICAL))
        {
            poSRS2->SetProjection("Miller Cylindrical");
        }
        else if (EQUAL(pszProjName, SRS_PT_HOTINE_OBLIQUE_MERCATOR))
        {
            poSRS2->SetProjection("Hotine Oblique Mercator");
        }
        else if (EQUAL(pszProjName, SRS_PT_WAGNER_IV))
        {
            poSRS2->SetProjection("Wagner IV");
        }
        else if (EQUAL(pszProjName, SRS_PT_WAGNER_VII))
        {
            poSRS2->SetProjection("Wagner VII");
        }
        else if (EQUAL(pszProjName, SRS_PT_ECKERT_IV))
        {
            poSRS2->SetProjection("Eckert IV");
        }
        else if (EQUAL(pszProjName, SRS_PT_ECKERT_VI))
        {
            poSRS2->SetProjection("Eckert VI");
        }
        else if (EQUAL(pszProjName, SRS_PT_NEW_ZEALAND_MAP_GRID))
        {
            poSRS2->SetProjection("New Zealand Map Grid");
        }
        else if (EQUAL(pszProjName, SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP))
        {
            poSRS2->SetProjection("Lambert Conformal Conic");
        }
        else if (EQUAL(pszProjName, SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA))
        {
            poSRS2->SetProjection("Lambert Azimuthal Equal Area");
        }
        else if (EQUAL(pszProjName, SRS_PT_VANDERGRINTEN))
        {
            poSRS2->SetProjection("Van der Grinten");
        }
        else if (EQUAL(pszProjName, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM))
        {
            poSRS2->SetProjection("Lambert Conformal Conic (Belgium 1972)");
        }
        else if (EQUAL(pszProjName, SRS_PT_CYLINDRICAL_EQUAL_AREA))
        {
            poSRS2->SetProjection("Cylindrical Equal Area");
        }
        else if (EQUAL(pszProjName, SRS_PT_GOODE_HOMOLOSINE))
        {
            poSRS2->SetProjection("Interrupted Goode Homolosine");
        }

        // ----------------------------------------------------------------
        // Translate projection's parameters to Oracle's standards
        // ----------------------------------------------------------------

        char *pszStart = nullptr;

        CPLFree(pszCloneWKT);
        pszCloneWKT = nullptr;

        if (poSRS2->exportToWkt(&pszCloneWKT) != OGRERR_NONE)
        {
            CPLFree(pszCloneWKT);
            return CE_Failure;
        }

        if ((pszStart = strstr(pszCloneWKT, SRS_PP_AZIMUTH)) != nullptr)
        {
            memcpy(pszStart, "Azimuth", strlen(SRS_PP_AZIMUTH));
        }

        if ((pszStart = strstr(pszCloneWKT, SRS_PP_CENTRAL_MERIDIAN)) !=
            nullptr)
        {
            memcpy(pszStart, "Central_Meridian",
                   strlen(SRS_PP_CENTRAL_MERIDIAN));
        }

        if ((pszStart = strstr(pszCloneWKT, SRS_PP_FALSE_EASTING)) != nullptr)
        {
            memcpy(pszStart, "False_Easting", strlen(SRS_PP_FALSE_EASTING));
        }

        if ((pszStart = strstr(pszCloneWKT, SRS_PP_FALSE_NORTHING)) != nullptr)
        {
            memcpy(pszStart, "False_Northing", strlen(SRS_PP_FALSE_NORTHING));
        }

        if ((pszStart = strstr(pszCloneWKT, SRS_PP_LATITUDE_OF_CENTER)) !=
            nullptr)
        {
            memcpy(pszStart, "Latitude_Of_Center",
                   strlen(SRS_PP_LATITUDE_OF_CENTER));
        }

        if ((pszStart = strstr(pszCloneWKT, SRS_PP_LATITUDE_OF_ORIGIN)) !=
            nullptr)
        {
            memcpy(pszStart, "Latitude_Of_Origin",
                   strlen(SRS_PP_LATITUDE_OF_ORIGIN));
        }

        if ((pszStart = strstr(pszCloneWKT, SRS_PP_LONGITUDE_OF_CENTER)) !=
            nullptr)
        {
            memcpy(pszStart, "Longitude_Of_Center",
                   strlen(SRS_PP_LONGITUDE_OF_CENTER));
        }

        if ((pszStart = strstr(pszCloneWKT, SRS_PP_PSEUDO_STD_PARALLEL_1)) !=
            nullptr)
        {
            memcpy(pszStart, "Pseudo_Standard_Parallel_1",
                   strlen(SRS_PP_PSEUDO_STD_PARALLEL_1));
        }

        if ((pszStart = strstr(pszCloneWKT, SRS_PP_SCALE_FACTOR)) != nullptr)
        {
            memcpy(pszStart, "Scale_Factor", strlen(SRS_PP_SCALE_FACTOR));
        }

        if ((pszStart = strstr(pszCloneWKT, SRS_PP_STANDARD_PARALLEL_1)) !=
            nullptr)
        {
            memcpy(pszStart, "Standard_Parallel_1",
                   strlen(SRS_PP_STANDARD_PARALLEL_1));
        }

        if ((pszStart = strstr(pszCloneWKT, SRS_PP_STANDARD_PARALLEL_2)) !=
            nullptr)
        {
            memcpy(pszStart, "Standard_Parallel_2",
                   strlen(SRS_PP_STANDARD_PARALLEL_2));
        }

        if ((pszStart = strstr(pszCloneWKT, SRS_PP_STANDARD_PARALLEL_2)) !=
            nullptr)
        {
            memcpy(pszStart, "Standard_Parallel_2",
                   strlen(SRS_PP_STANDARD_PARALLEL_2));
        }

        // ----------------------------------------------------------------
        // Fix Unit name
        // ----------------------------------------------------------------

        if ((pszStart = strstr(pszCloneWKT, "metre")) != nullptr)
        {
            memcpy(pszStart, SRS_UL_METER, strlen(SRS_UL_METER));
        }
    }

    // --------------------------------------------------------------------
    // Tries to find a SRID compatible with the WKT
    // --------------------------------------------------------------------

    OWConnection *poConnection = poGeoRaster->poConnection;
    OWStatement *poStmt = nullptr;

    int nNewSRID = 0;

    const char *pszFuncName = "FIND_GEOG_CRS";

    if (poSRS2->IsProjected())
    {
        pszFuncName = "FIND_PROJ_CRS";
    }

    poStmt = poConnection->CreateStatement(
        CPLSPrintf("DECLARE\n"
                   "  LIST SDO_SRID_LIST;"
                   "BEGIN\n"
                   "  SELECT SDO_CS.%s('%s', null) into LIST FROM DUAL;\n"
                   "  IF LIST.COUNT() > 0 then\n"
                   "    SELECT LIST(1) into :out from dual;\n"
                   "  ELSE\n"
                   "    SELECT 0 into :out from dual;\n"
                   "  END IF;\n"
                   "END;",
                   pszFuncName, pszCloneWKT));

    poStmt->BindName(":out", &nNewSRID);

    CPLPushErrorHandler(CPLQuietErrorHandler);

    if (poStmt->Execute())
    {
        CPLPopErrorHandler();

        if (nNewSRID > 0)
        {
            poGeoRaster->SetGeoReference(nNewSRID);
            CPLFree(pszCloneWKT);
            delete poStmt;
            m_oSRS = *poSRS;
            return CE_None;
        }
    }
    delete poStmt;

    // --------------------------------------------------------------------
    // Search by simplified WKT or insert it as a user defined SRS
    // --------------------------------------------------------------------

    int nCounter = 0;

    poStmt = poConnection->CreateStatement(CPLSPrintf(
        "SELECT COUNT(*) FROM MDSYS.CS_SRS WHERE WKTEXT = '%s'", pszCloneWKT));

    poStmt->Define(&nCounter);

    CPLPushErrorHandler(CPLQuietErrorHandler);

    if (poStmt->Execute() && nCounter > 0)
    {
        delete poStmt;

        poStmt = poConnection->CreateStatement(CPLSPrintf(
            "SELECT SRID FROM MDSYS.CS_SRS WHERE WKTEXT = '%s'", pszCloneWKT));

        poStmt->Define(&nNewSRID);

        if (poStmt->Execute())
        {
            CPLPopErrorHandler();

            poGeoRaster->SetGeoReference(nNewSRID);
            CPLFree(pszCloneWKT);
            delete poStmt;
            m_oSRS = *poSRS;
            return CE_None;
        }
    }

    CPLPopErrorHandler();

    delete poStmt;

    poStmt = poConnection->CreateStatement(
        CPLSPrintf("DECLARE\n"
                   "  MAX_SRID NUMBER := 0;\n"
                   "BEGIN\n"
                   "  SELECT MAX(SRID) INTO MAX_SRID FROM MDSYS.CS_SRS;\n"
                   "  MAX_SRID := MAX_SRID + 1;\n"
                   "  INSERT INTO MDSYS.CS_SRS (SRID, WKTEXT, CS_NAME)\n"
                   "        VALUES (MAX_SRID, '%s', '%s');\n"
                   "  SELECT MAX_SRID INTO :out FROM DUAL;\n"
                   "END;",
                   pszCloneWKT, poSRS->GetRoot()->GetChild(0)->GetValue()));

    poStmt->BindName(":out", &nNewSRID);

    CPLErr eError = CE_None;

    CPLPushErrorHandler(CPLQuietErrorHandler);

    if (poStmt->Execute())
    {
        CPLPopErrorHandler();

        poGeoRaster->SetGeoReference(nNewSRID);
    }
    else
    {
        CPLPopErrorHandler();

        poGeoRaster->SetGeoReference(UNKNOWN_CRS);

        CPLError(CE_Warning, CPLE_UserInterrupt,
                 "Insufficient privileges to insert reference system to "
                 "table MDSYS.CS_SRS.");

        eError = CE_Warning;
    }

    CPLFree(pszCloneWKT);

    delete poStmt;

    if (eError == CE_None)
        m_oSRS = *poSRS;

    return eError;
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **GeoRasterDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALDataset::GetMetadataDomainList(), TRUE,
                                   "SUBDATASETS", nullptr);
}

//  ---------------------------------------------------------------------------
//                                                                GetMetadata()
//  ---------------------------------------------------------------------------

char **GeoRasterDataset::GetMetadata(const char *pszDomain)
{
    if (pszDomain != nullptr && STARTS_WITH_CI(pszDomain, "SUBDATASETS"))
        return papszSubdatasets;
    else
        return GDALDataset::GetMetadata(pszDomain);
}

//  ---------------------------------------------------------------------------
//                                                                     Delete()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterDataset::Delete(const char *pszFilename)
{
    (void)pszFilename;
    /***
        GeoRasterDataset* poGRD = nullptr;

        poGRD = (GeoRasterDataset*) GDALOpen( pszFilename, GA_Update );

        if( ! poGRD )
        {
            return CE_Failure;
        }

        if( ! poGRD->poGeoRaster->Delete() )
        {
            return CE_Failure;
        }
    ***/
    return CE_None;
}

//  ---------------------------------------------------------------------------
//                                                             SetSubdatasets()
//  ---------------------------------------------------------------------------

void GeoRasterDataset::SetSubdatasets(GeoRasterWrapper *poGRW)
{
    OWConnection *poConnection = poGRW->poConnection;

    //  -----------------------------------------------------------
    //  List all the GeoRaster Tables of that User/Database
    //  -----------------------------------------------------------

    if (poGRW->sTable.empty() && poGRW->sColumn.empty())
    {
        OWStatement *poStmt = poConnection->CreateStatement(
            "SELECT   DISTINCT TABLE_NAME, OWNER FROM ALL_SDO_GEOR_SYSDATA\n"
            "  ORDER  BY TABLE_NAME ASC");

        char szTable[OWNAME];
        char szOwner[OWNAME];

        poStmt->Define(szTable);
        poStmt->Define(szOwner);

        if (poStmt->Execute())
        {
            int nCount = 1;

            do
            {
                papszSubdatasets = CSLSetNameValue(
                    papszSubdatasets, CPLSPrintf("SUBDATASET_%d_NAME", nCount),
                    CPLSPrintf("geor:%s/%s@%s,%s.%s", poConnection->GetUser(),
                               poConnection->GetPassword(),
                               poConnection->GetServer(), szOwner, szTable));

                papszSubdatasets = CSLSetNameValue(
                    papszSubdatasets, CPLSPrintf("SUBDATASET_%d_DESC", nCount),
                    CPLSPrintf("%s.Table=%s", szOwner, szTable));

                nCount++;
            } while (poStmt->Fetch());
        }

        delete poStmt;

        return;
    }

    //  -----------------------------------------------------------
    //  List all the GeoRaster Columns of that Table
    //  -----------------------------------------------------------

    if (!poGRW->sTable.empty() && poGRW->sColumn.empty())
    {
        OWStatement *poStmt = poConnection->CreateStatement(CPLSPrintf(
            "SELECT   DISTINCT COLUMN_NAME, OWNER FROM ALL_SDO_GEOR_SYSDATA\n"
            "  WHERE  OWNER = UPPER('%s') AND TABLE_NAME = UPPER('%s')\n"
            "  ORDER  BY COLUMN_NAME ASC",
            poGRW->sOwner.c_str(), poGRW->sTable.c_str()));

        char szColumn[OWNAME];
        char szOwner[OWNAME];

        poStmt->Define(szColumn);
        poStmt->Define(szOwner);

        if (poStmt->Execute())
        {
            int nCount = 1;

            do
            {
                papszSubdatasets = CSLSetNameValue(
                    papszSubdatasets, CPLSPrintf("SUBDATASET_%d_NAME", nCount),
                    CPLSPrintf(
                        "geor:%s/%s@%s,%s.%s,%s", poConnection->GetUser(),
                        poConnection->GetPassword(), poConnection->GetServer(),
                        szOwner, poGRW->sTable.c_str(), szColumn));

                papszSubdatasets = CSLSetNameValue(
                    papszSubdatasets, CPLSPrintf("SUBDATASET_%d_DESC", nCount),
                    CPLSPrintf("Table=%s.%s Column=%s", szOwner,
                               poGRW->sTable.c_str(), szColumn));

                nCount++;
            } while (poStmt->Fetch());
        }

        delete poStmt;

        return;
    }

    //  -----------------------------------------------------------
    //  List all the rows that contains GeoRaster on Table/Column/Where
    //  -----------------------------------------------------------

    CPLString osAndWhere = "";

    if (!poGRW->sWhere.empty())
    {
        osAndWhere = CPLSPrintf("AND %s", poGRW->sWhere.c_str());
    }

    OWStatement *poStmt = poConnection->CreateStatement(CPLSPrintf(
        "SELECT T.%s.RASTERDATATABLE, T.%s.RASTERID, \n"
        "  extractValue(t.%s.metadata, "
        "'/georasterMetadata/rasterInfo/dimensionSize[@type=\"ROW\"]/"
        "size','%s'),\n"
        "  extractValue(t.%s.metadata, "
        "'/georasterMetadata/rasterInfo/dimensionSize[@type=\"COLUMN\"]/"
        "size','%s'),\n"
        "  extractValue(t.%s.metadata, "
        "'/georasterMetadata/rasterInfo/dimensionSize[@type=\"BAND\"]/"
        "size','%s'),\n"
        "  extractValue(t.%s.metadata, "
        "'/georasterMetadata/rasterInfo/cellDepth','%s'),\n"
        "  extractValue(t.%s.metadata, "
        "'/georasterMetadata/spatialReferenceInfo/SRID','%s')\n"
        "  FROM   %s%s T\n"
        "  WHERE  %s IS NOT NULL %s\n"
        "  ORDER  BY T.%s.RASTERDATATABLE ASC,\n"
        "            T.%s.RASTERID ASC",
        poGRW->sColumn.c_str(), poGRW->sColumn.c_str(), poGRW->sColumn.c_str(),
        OW_XMLNS, poGRW->sColumn.c_str(), OW_XMLNS, poGRW->sColumn.c_str(),
        OW_XMLNS, poGRW->sColumn.c_str(), OW_XMLNS, poGRW->sColumn.c_str(),
        OW_XMLNS, poGRW->sSchema.c_str(), poGRW->sTable.c_str(),
        poGRW->sColumn.c_str(), osAndWhere.c_str(), poGRW->sColumn.c_str(),
        poGRW->sColumn.c_str()));

    char szDataTable[OWNAME];
    char szRasterId[OWNAME];
    char szRows[OWNAME];
    char szColumns[OWNAME];
    char szBands[OWNAME];
    char szCellDepth[OWNAME];
    char szSRID[OWNAME];

    poStmt->Define(szDataTable);
    poStmt->Define(szRasterId);
    poStmt->Define(szRows);
    poStmt->Define(szColumns);
    poStmt->Define(szBands);
    poStmt->Define(szCellDepth);
    poStmt->Define(szSRID);

    if (poStmt->Execute())
    {
        int nCount = 1;

        do
        {
            papszSubdatasets = CSLSetNameValue(
                papszSubdatasets, CPLSPrintf("SUBDATASET_%d_NAME", nCount),
                CPLSPrintf("geor:%s/%s@%s,%s,%s", poConnection->GetUser(),
                           poConnection->GetPassword(),
                           poConnection->GetServer(), szDataTable, szRasterId));

            const char *pszXBands = "";

            if (!EQUAL(szBands, ""))
            {
                pszXBands = CPLSPrintf("x%s", szBands);
            }

            papszSubdatasets = CSLSetNameValue(
                papszSubdatasets, CPLSPrintf("SUBDATASET_%d_DESC", nCount),
                CPLSPrintf("[%sx%s%s] CellDepth=%s SRID=%s", szRows, szColumns,
                           pszXBands, szCellDepth, szSRID));

            nCount++;
        } while (poStmt->Fetch());
    }
    delete poStmt;
}

int GeoRasterDataset::GetGCPCount()
{
    if (poGeoRaster)
    {
        return poGeoRaster->nGCPCount;
    }

    return 0;
}

//  ---------------------------------------------------------------------------
//                                                                    SetGCPs()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterDataset::SetGCPs(int nGCPCountIn, const GDAL_GCP *pasGCPListIn,
                                 const OGRSpatialReference *poSRS)
{
    if (GetAccess() == GA_Update)
    {
        poGeoRaster->SetGCP(nGCPCountIn, pasGCPListIn);
        SetSpatialRef(poSRS);
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetGCPs() is only supported on GeoRaster insert or update.");
        return CE_Failure;
    }

    return CE_None;
}

const GDAL_GCP *GeoRasterDataset::GetGCPs()
{
    if (poGeoRaster->nGCPCount > 0 && poGeoRaster->pasGCPList)
    {
        return poGeoRaster->pasGCPList;
    }

    return nullptr;
}

//  ---------------------------------------------------------------------------
//                                                           GetGCPSpatialRef()
//  ---------------------------------------------------------------------------

const OGRSpatialReference *GeoRasterDataset::GetGCPSpatialRef() const
{
    if (!m_oSRS.IsEmpty() && poGeoRaster && poGeoRaster->nGCPCount > 0)
        return &m_oSRS;
    else
        return nullptr;
}

//  ---------------------------------------------------------------------------
//                                                            IBuildOverviews()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterDataset::IBuildOverviews(
    const char *pszResampling, int nOverviews, const int *panOverviewList,
    int nListBands, const int *panBandList, GDALProgressFunc pfnProgress,
    void *pProgressData, CSLConstList papszOptions)
{
    (void)panBandList;
    (void)nListBands;

    if (EQUAL(poGeoRaster->sCompressionType.c_str(), "JP2-F"))
    {
        return CE_None;  // Ignore it, JP2 automatically has overviews
    }

    //  ---------------------------------------------------------------
    //  Can't update on read-only access mode
    //  ---------------------------------------------------------------

    if (GetAccess() != GA_Update)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Can't build overviews/pyramids on read-only access.");
        return CE_Failure;
    }

    //  ---------------------------------------------------------------
    //  Uses internal sdo_generatePyramid at PL/SQL?
    //  ---------------------------------------------------------------

    bool bInternal = true;

    const char *pszGEOR_INTERNAL_PYR =
        CPLGetConfigOption("GEOR_INTERNAL_PYR", "YES");

    if (EQUAL(pszGEOR_INTERNAL_PYR, "NO"))
    {
        bInternal = false;
    }

    //  -----------------------------------------------------------
    //  Pyramids applies to the whole dataset not to a specific band
    //  -----------------------------------------------------------

    if (nBands < GetRasterCount())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid GeoRaster Pyramids band selection");
        return CE_Failure;
    }

    //  ---------------------------------------------------------------
    //  Initialize progress reporting
    //  ---------------------------------------------------------------

    if (!pfnProgress(0.1, nullptr, pProgressData))
    {
        CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
        return CE_Failure;
    }

    //  ---------------------------------------------------------------
    //  Clear existing overviews
    //  ---------------------------------------------------------------

    if (nOverviews == 0)
    {
        poGeoRaster->DeletePyramid();
        return CE_None;
    }

    //  -----------------------------------------------------------
    //  Pyramids levels can not be treated individually
    //  -----------------------------------------------------------

    if (nOverviews > 0)
    {
        int i;
        for (i = 1; i < nOverviews; i++)
        {
            //  -----------------------------------------------------------
            //  Power of 2, starting on 2, e.g. 2, 4, 8, 16, 32, 64, 128
            //  -----------------------------------------------------------

            if (panOverviewList[0] != 2 ||
                (panOverviewList[i] != panOverviewList[i - 1] * 2))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid GeoRaster Pyramids levels.");
                return CE_Failure;
            }
        }
    }

    //  -----------------------------------------------------------
    //  Re-sampling method:
    //    NN, BILINEAR, AVERAGE4, AVERAGE16 and CUBIC
    //  -----------------------------------------------------------

    char szMethod[OWNAME];

    if (EQUAL(pszResampling, "NEAREST"))
    {
        strcpy(szMethod, "NN");
    }
    else if (STARTS_WITH_CI(pszResampling, "AVERAGE"))
    {
        strcpy(szMethod, "AVERAGE4");
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid resampling method");
        return CE_Failure;
    }

    //  -----------------------------------------------------------
    //  Generate pyramids on poGeoRaster
    //  -----------------------------------------------------------

    if (!poGeoRaster->GeneratePyramid(nOverviews, szMethod, bInternal))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error generating pyramid");
        return CE_Failure;
    }

    //  -----------------------------------------------------------
    //  If Pyramid was done internally on the server exit here
    //  -----------------------------------------------------------

    if (bInternal)
    {
        pfnProgress(1, nullptr, pProgressData);
        return CE_None;
    }

    //  -----------------------------------------------------------
    //  Load the pyramids data using GDAL methods
    //  -----------------------------------------------------------

    CPLErr eErr = CE_None;

    int i = 0;

    for (i = 0; i < nBands; i++)
    {
        GeoRasterRasterBand *poBand = (GeoRasterRasterBand *)papoBands[i];

        //  -------------------------------------------------------
        //  Clean up previous overviews
        //  -------------------------------------------------------

        int j = 0;

        if (poBand->nOverviewCount && poBand->papoOverviews)
        {
            for (j = 0; j < poBand->nOverviewCount; j++)
            {
                delete poBand->papoOverviews[j];
            }
            CPLFree(poBand->papoOverviews);
        }

        //  -------------------------------------------------------
        //  Create new band's overviews list
        //  -------------------------------------------------------

        poBand->nOverviewCount = poGeoRaster->nPyramidMaxLevel;
        poBand->papoOverviews = (GeoRasterRasterBand **)VSIMalloc(
            sizeof(GeoRasterRasterBand *) * poBand->nOverviewCount);

        for (j = 0; j < poBand->nOverviewCount; j++)
        {
            poBand->papoOverviews[j] = new GeoRasterRasterBand(
                (GeoRasterDataset *)this, (i + 1), (j + 1));
        }
    }

    //  -----------------------------------------------------------
    //  Load band's overviews
    //  -----------------------------------------------------------

    for (i = 0; i < nBands; i++)
    {
        GeoRasterRasterBand *poBand = (GeoRasterRasterBand *)papoBands[i];

        void *pScaledProgressData = GDALCreateScaledProgress(
            i / (double)nBands, (i + 1) / (double)nBands, pfnProgress,
            pProgressData);

        eErr = GDALRegenerateOverviewsEx(
            (GDALRasterBandH)poBand, poBand->nOverviewCount,
            (GDALRasterBandH *)poBand->papoOverviews, pszResampling,
            GDALScaledProgress, pScaledProgressData, papszOptions);

        GDALDestroyScaledProgress(pScaledProgressData);
    }

    return eErr;
}

//  ---------------------------------------------------------------------------
//                                                             CreateMaskBand()
//  ---------------------------------------------------------------------------

CPLErr GeoRasterDataset::CreateMaskBand(int /*nFlags*/)
{
    if (!poGeoRaster->InitializeMask(
            DEFAULT_BMP_MASK, poGeoRaster->nRowBlockSize,
            poGeoRaster->nColumnBlockSize, poGeoRaster->nTotalRowBlocks,
            poGeoRaster->nTotalColumnBlocks, poGeoRaster->nTotalBandBlocks))
    {
        return CE_Failure;
    }

    poGeoRaster->bHasBitmapMask = true;

    return CE_None;
}

/*****************************************************************************/
/*                          GDALRegister_GEOR                                */
/*****************************************************************************/

void CPL_DLL GDALRegister_GEOR()

{
    if (!GDAL_CHECK_VERSION("GeoRaster driver"))
        return;

    if (GDALGetDriverByName("GeoRaster") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("GeoRaster");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Oracle Spatial GeoRaster");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/raster/georaster.html");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte UInt16 Int16 UInt32 Int32 Float32 "
                              "Float64 CFloat32 CFloat64");
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "  <Option name='DESCRIPTION' type='string' description='Table "
        "Description'/>"
        "  <Option name='INSERT'      type='string' description='Column "
        "Values'/>"
        "  <Option name='BLOCKXSIZE'  type='int'    description='Column Block "
        "Size' "
        "default='512'/>"
        "  <Option name='BLOCKYSIZE'  type='int'    description='Row Block "
        "Size' "
        "default='512'/>"
        "  <Option name='BLOCKBSIZE'  type='int'    description='Band Block "
        "Size'/>"
        "  <Option name='BLOCKING'    type='string-select' default='YES'>"
        "       <Value>YES</Value>"
        "       <Value>NO</Value>"
        "       <Value>OPTIMALPADDING</Value>"
        "  </Option>"
        "  <Option name='SRID'        type='int'    description='Overwrite "
        "EPSG code'/>"
        "  <Option name='GENPYRAMID'  type='string-select' "
        " description='Generate Pyramid, inform resampling method'>"
        "       <Value>NN</Value>"
        "       <Value>BILINEAR</Value>"
        "       <Value>BIQUADRATIC</Value>"
        "       <Value>CUBIC</Value>"
        "       <Value>AVERAGE4</Value>"
        "       <Value>AVERAGE16</Value>"
        "  </Option>"
        "  <Option name='GENPYRLEVELS'  type='int'  description='Number of "
        "pyramid level to generate'/>"
        "  <Option name='OBJECTTABLE' type='boolean' "
        "description='Create RDT as object table'/>"
        "  <Option name='SPATIALEXTENT' type='boolean' "
        "description='Generate Spatial Extent' "
        "default='TRUE'/>"
        "  <Option name='EXTENTSRID'  type='int'    description='Spatial "
        "ExtentSRID code'/>"
        "  <Option name='COORDLOCATION'    type='string-select' "
        "default='CENTER'>"
        "       <Value>CENTER</Value>"
        "       <Value>UPPERLEFT</Value>"
        "  </Option>"
        "  <Option name='VATNAME'     type='string' description='Value "
        "Attribute Table Name'/>"
        "  <Option name='NBITS'       type='int'    description='BITS for "
        "sub-byte "
        "data types (1,2,4) bits'/>"
        "  <Option name='INTERLEAVE'  type='string-select'>"
        "       <Value>BSQ</Value>"
        "       <Value>BIP</Value>"
        "       <Value>BIL</Value>"
        "   </Option>"
        "  <Option name='COMPRESS'    type='string-select'>"
        "       <Value>NONE</Value>"
        "       <Value>JPEG-F</Value>"
        "       <Value>JP2-F</Value>"
        "       <Value>DEFLATE</Value>"
        "  </Option>"
        "  <Option name='QUALITY'     type='int'    description='JPEG quality "
        "0..100' "
        "default='75'/>"
        "  <Option name='JP2_QUALITY'     type='string' description='For JP2-F "
        "compression, single quality value or comma separated list "
        "of increasing quality values for several layers, each in the 0-100 "
        "range' default='25'/>"
        "  <Option name='JP2_BLOCKXSIZE'  type='int' description='For JP2 "
        "compression, tile Width' default='1024'/>"
        "  <Option name='JP2_BLOCKYSIZE'  type='int' description='For JP2 "
        "compression, tile Height' default='1024'/>"
        "  <Option name='JP2_REVERSIBLE'  type='boolean' description='For "
        "JP2-F compression, True if the compression is reversible' "
        "default='false'/>"
        "  <Option name='JP2_RESOLUTIONS' type='int' description='For JP2-F "
        "compression, Number of resolutions.' min='1' max='30'/>"
        "  <Option name='JP2_PROGRESSION' type='string-select' "
        "description='For JP2-F compression, progression order' default='LRCP'>"
        "    <Value>LRCP</Value>"
        "    <Value>RLCP</Value>"
        "    <Value>RPCL</Value>"
        "    <Value>PCRL</Value>"
        "    <Value>CPRL</Value>"
        "  </Option>"
        "</CreationOptionList>");

    poDriver->pfnOpen = GeoRasterDataset::Open;
    poDriver->pfnCreate = GeoRasterDataset::Create;
    poDriver->pfnCreateCopy = GeoRasterDataset::CreateCopy;
    poDriver->pfnIdentify = GeoRasterDataset::Identify;
    poDriver->pfnDelete = GeoRasterDataset::Delete;

    GetGDALDriverManager()->RegisterDriver(poDriver);

    VSIInstallOCILobHandler();
}
