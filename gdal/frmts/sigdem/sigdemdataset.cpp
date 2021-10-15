/******************************************************************************
 *
 * Project:    Scaled Integer Gridded DEM .sigdem Driver
 * Purpose:    Implementation of Scaled Integer Gridded DEM
 * Author:     Paul Austin, paul.austin@revolsys.com
 *
 ******************************************************************************
 * Copyright (c) 2018, Paul Austin <paul.austin@revolsys.com>
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

#include "sigdemdataset.h"
#include "rawdataset.h"

CPL_CVSID("$Id$")

#ifdef CPL_IS_LSB
#define SWAP_SIGDEM_HEADER(abyHeader) { \
CPL_SWAP16PTR(abyHeader + 6); \
CPL_SWAP32PTR(abyHeader + 8); \
CPL_SWAP64PTR(abyHeader + 12); \
CPL_SWAP64PTR(abyHeader + 20); \
CPL_SWAP64PTR(abyHeader + 28); \
CPL_SWAP64PTR(abyHeader + 36); \
CPL_SWAP64PTR(abyHeader + 44); \
CPL_SWAP64PTR(abyHeader + 52); \
CPL_SWAP64PTR(abyHeader + 60); \
CPL_SWAP64PTR(abyHeader + 68); \
CPL_SWAP64PTR(abyHeader + 76); \
CPL_SWAP64PTR(abyHeader + 84); \
CPL_SWAP64PTR(abyHeader + 92); \
CPL_SWAP64PTR(abyHeader + 100); \
CPL_SWAP32PTR(abyHeader + 108); \
CPL_SWAP32PTR(abyHeader + 112); \
CPL_SWAP64PTR(abyHeader + 116); \
CPL_SWAP64PTR(abyHeader + 124); \
}
#else
#define SWAP_SIGDEM_HEADER(abyHeader)
#endif

constexpr int CELL_SIZE_FILE = 4;

constexpr int CELL_SIZE_MEM = 8;

constexpr vsi_l_offset HEADER_LENGTH = 132;

constexpr int32_t NO_DATA = 0x80000000;

constexpr char SIGDEM_FILE_TYPE[6] = { 'S', 'I', 'G', 'D', 'E', 'M' };

static OGRSpatialReference* BuildSRS(const char* pszWKT) {
    OGRSpatialReference* poSRS = new OGRSpatialReference(pszWKT);
    if (poSRS->morphFromESRI() != OGRERR_NONE) {
        delete poSRS;
        return nullptr;
    } else {
        if (poSRS->AutoIdentifyEPSG() != OGRERR_NONE) {
            int nEntries = 0;
            int* panConfidence = nullptr;
            OGRSpatialReferenceH* pahSRS = poSRS->FindMatches(nullptr,
                    &nEntries, &panConfidence);
            if (nEntries == 1 && panConfidence[0] == 100) {
                poSRS->Release();
                poSRS = reinterpret_cast<OGRSpatialReference*>(pahSRS[0]);
                CPLFree(pahSRS);
            } else {
                OSRFreeSRSArray(pahSRS);
            }
            CPLFree(panConfidence);
        }
        return poSRS;
    }
}

void GDALRegister_SIGDEM() {
    if (GDALGetDriverByName("SIGDEM") == nullptr) {
        GDALDriver *poDriver = new GDALDriver();

        poDriver->SetDescription("SIGDEM");
        poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
        poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                "Scaled Integer Gridded DEM .sigdem");
        poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                "drivers/raster/sigdem.html");
        poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "sigdem");

        poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
        poDriver->pfnCreateCopy = SIGDEMDataset::CreateCopy;
        poDriver->pfnIdentify = SIGDEMDataset::Identify;
        poDriver->pfnOpen = SIGDEMDataset::Open;

        GetGDALDriverManager()->RegisterDriver(poDriver);
    }
}

static int32_t GetCoordinateSystemId(const char* pszProjection) {
    int32_t coordinateSystemId = 0;
    OGRSpatialReference* poSRS = BuildSRS(pszProjection);
    if (poSRS != nullptr) {
        std::string pszRoot;
        if (poSRS->IsProjected()) {
            pszRoot = "PROJCS";
        } else {
            pszRoot = "GEOCS";
        }
        const char *pszAuthName = poSRS->GetAuthorityName(pszRoot.c_str());
        const char *pszAuthCode = poSRS->GetAuthorityCode(pszRoot.c_str());
        if (pszAuthName != nullptr && EQUAL(pszAuthName, "EPSG")
                && pszAuthCode != nullptr) {
            coordinateSystemId = atoi(pszAuthCode);
        }
    }
    delete poSRS;
    return coordinateSystemId;
}

SIGDEMDataset::SIGDEMDataset(const SIGDEMHeader& sHeaderIn) :
        fpImage(nullptr),
        pszProjection(CPLStrdup("")),
        sHeader(sHeaderIn) {
    this->nRasterXSize = sHeader.nCols;
    this->nRasterYSize = sHeader.nRows;

    this->adfGeoTransform[0] = sHeader.dfMinX;
    this->adfGeoTransform[1] = sHeader.dfXDim;
    this->adfGeoTransform[2] = 0.0;
    this->adfGeoTransform[3] = sHeader.dfMaxY;
    this->adfGeoTransform[4] = 0.0;
    this->adfGeoTransform[5] = -sHeader.dfYDim;
}

SIGDEMDataset::~SIGDEMDataset() {
    FlushCache(true);

    if (fpImage != nullptr) {
        if (VSIFCloseL(fpImage) != 0) {
            CPLError(CE_Failure, CPLE_FileIO, "I/O error");
        }
    }

    CPLFree(pszProjection);
}

GDALDataset* SIGDEMDataset::CreateCopy(
    const char * pszFilename,
    GDALDataset * poSrcDS,
    int /*bStrict*/,
    char ** /*papszOptions*/,
    GDALProgressFunc pfnProgress,
    void * pProgressData) {
    const int nBands = poSrcDS->GetRasterCount();
    double adfGeoTransform[6] = { };
    if (poSrcDS->GetGeoTransform(adfGeoTransform) != CE_None) {
        CPLError(CE_Failure, CPLE_NotSupported,
                "SIGDEM driver requires a valid GeoTransform.");
        return nullptr;
    }

    if (nBands != 1) {
        CPLError(CE_Failure, CPLE_NotSupported,
                "SIGDEM driver doesn't support %d bands.  Must be 1 band.",
                nBands);
        return nullptr;
    }

    VSILFILE *fp = VSIFOpenL(pszFilename, "wb");
    if (fp == nullptr) {
        CPLError(CE_Failure, CPLE_OpenFailed,
                "Attempt to create file `%s' failed.", pszFilename);
        return nullptr;
    }

    GDALRasterBand* band = poSrcDS->GetRasterBand(1);
    const char* pszProjection = poSrcDS->GetProjectionRef();

    int32_t nCols = poSrcDS->GetRasterXSize();
    int32_t nRows = poSrcDS->GetRasterYSize();
    int32_t nCoordinateSystemId = GetCoordinateSystemId(pszProjection);

    SIGDEMHeader sHeader;
    sHeader.nCoordinateSystemId = nCoordinateSystemId;
    sHeader.dfMinX = adfGeoTransform[0];
    const char* pszMin = band->GetMetadataItem("STATISTICS_MINIMUM");
    if (pszMin == nullptr) {
        sHeader.dfMinZ = -10000;
    } else {
        sHeader.dfMinZ = CPLAtof(pszMin);
    }
    sHeader.dfMaxY = adfGeoTransform[3];
    const char* pszMax = band->GetMetadataItem("STATISTICS_MAXIMUM");
    if (pszMax == nullptr) {
        sHeader.dfMaxZ = 10000;
    } else {
        sHeader.dfMaxZ = CPLAtof(pszMax);
    }
    sHeader.nCols = poSrcDS->GetRasterXSize();
    sHeader.nRows = poSrcDS->GetRasterYSize();
    sHeader.dfXDim = adfGeoTransform[1];
    sHeader.dfYDim = -adfGeoTransform[5];
    sHeader.dfMaxX = sHeader.dfMinX + sHeader.nCols * sHeader.dfXDim;
    sHeader.dfMinY = sHeader.dfMaxY - sHeader.nRows * sHeader.dfYDim;
    sHeader.dfOffsetX = sHeader.dfMinX;
    sHeader.dfOffsetY = sHeader.dfMinY;

    if( !sHeader.Write(fp) )
    {
        VSIUnlink(pszFilename);
        VSIFCloseL(fp);
        return nullptr;
    }

    // Write fill with all NO_DATA values
    int32_t* row = static_cast<int32_t*>(VSI_MALLOC2_VERBOSE(nCols, sizeof(int32_t)));
    if( !row ) {
        VSIUnlink(pszFilename);
        VSIFCloseL(fp);
        return nullptr;
    }
    std::fill(row, row + nCols, CPL_MSBWORD32(NO_DATA));
    for (int i = 0; i < nRows; i++) {
        if( VSIFWriteL(row, CELL_SIZE_FILE, nCols, fp) !=
                                            static_cast<size_t>(nCols) )
        {
            VSIFree(row);
            VSIUnlink(pszFilename);
            VSIFCloseL(fp);
            return nullptr;
        }
    }
    VSIFree(row);

    if (VSIFCloseL(fp) != 0) {
        return nullptr;
    }

    if (nCoordinateSystemId <= 0) {
        if (!EQUAL(pszProjection, "")) {
            CPLString osPrjFilename = CPLResetExtension(pszFilename, "prj");
            VSILFILE *fpProj = VSIFOpenL(osPrjFilename, "wt");
            if (fpProj != nullptr) {
                OGRSpatialReference oSRS;
                oSRS.importFromWkt(pszProjection);
                oSRS.morphToESRI();
                char *pszESRIProjection = nullptr;
                oSRS.exportToWkt(&pszESRIProjection);
                CPL_IGNORE_RET_VAL(
                        VSIFWriteL(pszESRIProjection, 1,
                                strlen(pszESRIProjection), fpProj));

                CPL_IGNORE_RET_VAL(VSIFCloseL(fpProj));
                CPLFree(pszESRIProjection);
            } else {
                CPLError(CE_Failure, CPLE_FileIO, "Unable to create file %s.",
                        osPrjFilename.c_str());
            }
        }
    }
    GDALOpenInfo oOpenInfo(pszFilename, GA_Update);
    GDALDataset * poDstDS = Open(&oOpenInfo);
    if (poDstDS != nullptr &&
        GDALDatasetCopyWholeRaster(poSrcDS, poDstDS, nullptr, pfnProgress,
            pProgressData) == OGRERR_NONE) {
        return poDstDS;
    } else {
        VSIUnlink(pszFilename);
        return nullptr;
    }
}

CPLErr SIGDEMDataset::GetGeoTransform(double * padfTransform) {
    memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6);
    return CE_None;
}

const char* SIGDEMDataset::_GetProjectionRef() {
    return pszProjection;
}

int SIGDEMDataset::Identify(GDALOpenInfo* poOpenInfo) {
    if (poOpenInfo->nHeaderBytes < static_cast<int>(HEADER_LENGTH)) {
        return FALSE;
    }
    return memcmp(poOpenInfo->pabyHeader, SIGDEM_FILE_TYPE,
                  sizeof(SIGDEM_FILE_TYPE)) == 0;
}

GDALDataset *SIGDEMDataset::Open(GDALOpenInfo * poOpenInfo) {
    VSILFILE* fp = poOpenInfo->fpL;

    SIGDEMHeader sHeader;
    if (SIGDEMDataset::Identify(poOpenInfo) != TRUE || fp == nullptr) {
        return nullptr;
    }

    sHeader.Read(poOpenInfo->pabyHeader);

    if (!GDALCheckDatasetDimensions(sHeader.nCols, sHeader.nRows)) {
        return nullptr;
    }

    OGRSpatialReference oSRS;

    if (sHeader.nCoordinateSystemId > 0) {
        if (oSRS.importFromEPSG(sHeader.nCoordinateSystemId) != OGRERR_NONE) {
            CPLError(CE_Failure, CPLE_NotSupported,
                    "SIGDEM unable to find coordinateSystemId=%d.",
                    sHeader.nCoordinateSystemId);
            return nullptr;
        }
    } else {
        CPLString osPrjFilename = CPLResetExtension(poOpenInfo->pszFilename,
                "prj");
        VSIStatBufL sStatBuf;
        int nRet = VSIStatL(osPrjFilename, &sStatBuf);
        if (nRet != 0 && VSIIsCaseSensitiveFS(osPrjFilename)) {
            osPrjFilename = CPLResetExtension(poOpenInfo->pszFilename, "PRJ");
            nRet = VSIStatL(osPrjFilename, &sStatBuf);
        }

        if (nRet == 0) {
            char** papszPrj = CSLLoad(osPrjFilename);
            if (oSRS.importFromESRI(papszPrj) != OGRERR_NONE) {
                CPLError(CE_Failure, CPLE_NotSupported,
                        "SIGDEM unable to read projection from %s.",
                        osPrjFilename.c_str());
                CSLDestroy(papszPrj);
                return nullptr;
            }
            CSLDestroy(papszPrj);
        } else {
            CPLError(CE_Failure, CPLE_NotSupported,
                    "SIGDEM unable to find projection.");
            return nullptr;
        }
    }

    if (sHeader.nCols > std::numeric_limits<int>::max() / (CELL_SIZE_MEM)) {
        CPLError(CE_Failure, CPLE_AppDefined, "Int overflow occurred.");
        return nullptr;
    }

    if( !RAWDatasetCheckMemoryUsage(sHeader.nCols, sHeader.nRows, 1,
                    4, 4, 4 * sHeader.nCols, 0, 0, poOpenInfo->fpL) )
    {
        return nullptr;
    }
    SIGDEMDataset *poDS = new SIGDEMDataset(sHeader);

    CPLFree(poDS->pszProjection);
    oSRS.exportToWkt(&(poDS->pszProjection));

    poDS->fpImage = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;
    poDS->eAccess = poOpenInfo->eAccess;

    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->PamInitialize();

    poDS->nBands = 1;
    CPLErrorReset();
    SIGDEMRasterBand *poBand = new SIGDEMRasterBand(poDS, poDS->fpImage,
            sHeader.dfMinZ, sHeader.dfMaxZ);

    poDS->SetBand(1, poBand);
    if (CPLGetLastErrorType() != CE_None) {
        poDS->nBands = 1;
        delete poDS;
        return nullptr;
    }

// Initialize any PAM information.
    poDS->TryLoadXML();

// Check for overviews.
    poDS->oOvManager.Initialize(poDS, poOpenInfo->pszFilename);

    return poDS;
}

SIGDEMHeader::SIGDEMHeader() {
}

bool SIGDEMHeader::Read(const GByte* pabyHeader) {
    GByte abyHeader[HEADER_LENGTH];
    memcpy(abyHeader, pabyHeader, HEADER_LENGTH);

    SWAP_SIGDEM_HEADER(abyHeader);
    memcpy(&(this->version), abyHeader + 6, 2);
    memcpy(&(this->nCoordinateSystemId), abyHeader + 8, 4);
    memcpy(&(this->dfOffsetX), abyHeader + 12, 8);
    memcpy(&(this->dfScaleFactorX), abyHeader + 20, 8);
    memcpy(&(this->dfOffsetY), abyHeader + 28, 8);
    memcpy(&(this->dfScaleFactorY), abyHeader + 36, 8);
    memcpy(&(this->dfOffsetZ), abyHeader + 44, 8);
    memcpy(&(this->dfScaleFactorZ), abyHeader + 52, 8);
    memcpy(&(this->dfMinX), abyHeader + 60, 8);
    memcpy(&(this->dfMinY), abyHeader + 68, 8);
    memcpy(&(this->dfMinZ), abyHeader + 76, 8);
    memcpy(&(this->dfMaxX), abyHeader + 84, 8);
    memcpy(&(this->dfMaxY), abyHeader + 92, 8);
    memcpy(&(this->dfMaxZ), abyHeader + 100, 8);
    memcpy(&(this->nCols), abyHeader + 108, 4);
    memcpy(&(this->nRows), abyHeader + 112, 4);
    memcpy(&(this->dfXDim), abyHeader + 116, 8);
    memcpy(&(this->dfYDim), abyHeader + 124, 8);

    return true;
}

bool SIGDEMHeader::Write(VSILFILE *fp) {
    GByte abyHeader[HEADER_LENGTH];

    memcpy(abyHeader, &(SIGDEM_FILE_TYPE), 6);
    memcpy(abyHeader + 6, &(this->version), 2);
    memcpy(abyHeader + 8, &(this->nCoordinateSystemId), 4);
    memcpy(abyHeader + 12, &(this->dfOffsetX), 8);
    memcpy(abyHeader + 20, &(this->dfScaleFactorX), 8);
    memcpy(abyHeader + 28, &(this->dfOffsetY), 8);
    memcpy(abyHeader + 36, &(this->dfScaleFactorY), 8);
    memcpy(abyHeader + 44, &(this->dfOffsetZ), 8);
    memcpy(abyHeader + 52, &(this->dfScaleFactorZ), 8);
    memcpy(abyHeader + 60, &(this->dfMinX), 8);
    memcpy(abyHeader + 68, &(this->dfMinY), 8);
    memcpy(abyHeader + 76, &(this->dfMinZ), 8);
    memcpy(abyHeader + 84, &(this->dfMaxX), 8);
    memcpy(abyHeader + 92, &(this->dfMaxY), 8);
    memcpy(abyHeader + 100, &(this->dfMaxZ), 8);
    memcpy(abyHeader + 108, &(this->nCols), 4);
    memcpy(abyHeader + 112, &(this->nRows), 4);
    memcpy(abyHeader + 116, &(this->dfXDim), 8);
    memcpy(abyHeader + 124, &(this->dfYDim), 8);
    SWAP_SIGDEM_HEADER(abyHeader);
    return VSIFWriteL(&abyHeader, HEADER_LENGTH, 1, fp) == 1;
}

SIGDEMRasterBand::SIGDEMRasterBand(
    SIGDEMDataset *poDSIn,
    VSILFILE *fpRawIn,
    double dfMinZ,
    double dfMaxZ) :
        dfOffsetZ(poDSIn->sHeader.dfOffsetZ),
        dfScaleFactorZ(poDSIn->sHeader.dfScaleFactorZ),
        fpRawL(fpRawIn) {
    this->poDS = poDSIn;
    this->nBand = 1;
    this->nRasterXSize = poDSIn->GetRasterXSize();
    this->nRasterYSize = poDSIn->GetRasterYSize();
    this->nBlockXSize = this->nRasterXSize;
    this->nBlockYSize = 1;
    this->eDataType = GDT_Float64;

    this->nBlockSizeBytes = nRasterXSize * CELL_SIZE_FILE;

    this->pBlockBuffer = static_cast<int32_t*>(
        VSI_MALLOC2_VERBOSE(nRasterXSize, sizeof(int32_t)));
    SetNoDataValue(-9999);
    CPLString osValue;
    SetMetadataItem("STATISTICS_MINIMUM", osValue.Printf("%.15g", dfMinZ));
    SetMetadataItem("STATISTICS_MAXIMUM", osValue.Printf("%.15g", dfMaxZ));
}

CPLErr SIGDEMRasterBand::IReadBlock(
    int /*nBlockXOff*/,
    int nBlockYOff,
    void *pImage) {

    const int nBlockIndex = nRasterYSize - nBlockYOff - 1;

    if (nLoadedBlockIndex == nBlockIndex) {
        return CE_None;
    }
    const vsi_l_offset nReadStart = HEADER_LENGTH
            + static_cast<vsi_l_offset>(nBlockSizeBytes) * nBlockIndex;

    // Seek to the correct line.
    if (VSIFSeekL(fpRawL, nReadStart, SEEK_SET) == -1) {
        if (poDS != nullptr && poDS->GetAccess() == GA_ReadOnly) {
            CPLError(CE_Failure, CPLE_FileIO,
                    "Failed to seek to block %d @ " CPL_FRMT_GUIB ".",
                    nBlockIndex, nReadStart);
            return CE_Failure;
        } else {
            std::fill(pBlockBuffer, pBlockBuffer + nRasterXSize, 0);
            nLoadedBlockIndex = nBlockIndex;
            return CE_None;
        }
    }
    const size_t nCellReadCount = VSIFReadL(pBlockBuffer, CELL_SIZE_FILE,
            nRasterXSize, fpRawL);
    if (nCellReadCount < static_cast<size_t>(nRasterXSize)) {
        if (poDS != nullptr && poDS->GetAccess() == GA_ReadOnly) {
            CPLError(CE_Failure, CPLE_FileIO, "Failed to read block %d.",
                    nBlockIndex);
            return CE_Failure;
        }
        std::fill(pBlockBuffer + nCellReadCount, pBlockBuffer + nRasterXSize,
                NO_DATA);
    }

    nLoadedBlockIndex = nBlockIndex;

    const int32_t* pnSourceValues = pBlockBuffer;
    double* padfDestValues = static_cast<double*>(pImage);
    double dfOffset = this->dfOffsetZ;
    const double dfInvScaleFactor = dfScaleFactorZ != 0.0 ? 1.0 / dfScaleFactorZ : 0.0;
    int nCellCount = this->nRasterXSize;
    for (int i = 0; i < nCellCount; i++) {
        int32_t nValue = CPL_MSBWORD32(*pnSourceValues);
        if (nValue == NO_DATA) {
            *padfDestValues = -9999;
        } else {
            *padfDestValues = dfOffset + nValue * dfInvScaleFactor;
        }

        pnSourceValues++;
        padfDestValues++;
    }

    return CE_None;
}

CPLErr SIGDEMRasterBand::IWriteBlock(
    int /*nBlockXOff*/,
    int nBlockYOff,
    void *pImage) {

    const int nBlockIndex = nRasterYSize - nBlockYOff - 1;

    const double* padfSourceValues = static_cast<double*>(pImage);
    int32_t* pnDestValues = pBlockBuffer;
    double dfOffset = this->dfOffsetZ;
    double dfScaleFactor = this->dfScaleFactorZ;
    int nCellCount = this->nRasterXSize;
    for (int i = 0; i < nCellCount; i++) {
        double dfValue = *padfSourceValues;
        int32_t nValue;
        if (dfValue == -9999) {
            nValue = NO_DATA;
        } else {
            nValue = static_cast<int32_t>(
                std::round((dfValue - dfOffset) * dfScaleFactor));
        }
        *pnDestValues = CPL_MSBWORD32(nValue);
        padfSourceValues++;
        pnDestValues++;
    }

    const vsi_l_offset nWriteStart = HEADER_LENGTH
            + static_cast<vsi_l_offset>(nBlockSizeBytes) * nBlockIndex;

    if (VSIFSeekL(fpRawL, nWriteStart, SEEK_SET) == -1 ||
        VSIFWriteL(pBlockBuffer, CELL_SIZE_FILE, nRasterXSize, fpRawL)
            < static_cast<size_t>(nRasterXSize)) {
        CPLError(CE_Failure, CPLE_FileIO, "Failed to write block %d to file.",
                nBlockIndex);

        return CE_Failure;
    }
    return CE_None;
}

SIGDEMRasterBand::~SIGDEMRasterBand() {
    SIGDEMRasterBand::FlushCache(true);
    VSIFree(pBlockBuffer);
}
