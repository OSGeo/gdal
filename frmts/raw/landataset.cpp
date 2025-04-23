/******************************************************************************
 *
 * Project:  eCognition
 * Purpose:  Implementation of Erdas .LAN / .GIS format.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam
 * Copyright (c) 2008-2011, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "ogr_spatialref.h"
#include "rawdataset.h"

#include <cmath>

#include <algorithm>

/**

Erdas Header format: "HEAD74"

Offset   Size    Type      Description
------   ----    ----      -----------
0          6     char      magic cookie / version (i.e. HEAD74).
6          2    Int16      Pixel type, 0=8bit, 1=4bit, 2=16bit
8          2    Int16      Number of Bands.
10         6     char      Unknown.
16         4    Int32      Width
20         4    Int32      Height
24         4    Int32      X Start (offset in original file?)
28         4    Int32      Y Start (offset in original file?)
32        56     char      Unknown.
88         2    Int16      0=LAT, 1=UTM, 2=StatePlane, 3- are projections?
90         2    Int16      Classes in coverage.
92        14     char      Unknown.
106        2    Int16      Area Unit (0=none, 1=Acre, 2=Hectare, 3=Other)
108        4  Float32      Pixel area.
112        4  Float32      Upper Left corner X (center of pixel?)
116        4  Float32      Upper Left corner Y (center of pixel?)
120        4  Float32      Width of a pixel.
124        4  Float32      Height of a pixel.

Erdas Header format: "HEADER"

Offset   Size    Type      Description
------   ----    ----      -----------
0          6     char      magic cookie / version (i.e. HEAD74).
6          2    Int16      Pixel type, 0=8bit, 1=4bit, 2=16bit
8          2    Int16      Number of Bands.
10         6     char      Unknown.
16         4  Float32      Width
20         4  Float32      Height
24         4    Int32      X Start (offset in original file?)
28         4    Int32      Y Start (offset in original file?)
32        56     char      Unknown.
88         2    Int16      0=LAT, 1=UTM, 2=StatePlane, 3- are projections?
90         2    Int16      Classes in coverage.
92        14     char      Unknown.
106        2    Int16      Area Unit (0=none, 1=Acre, 2=Hectare, 3=Other)
108        4  Float32      Pixel area.
112        4  Float32      Upper Left corner X (center of pixel?)
116        4  Float32      Upper Left corner Y (center of pixel?)
120        4  Float32      Width of a pixel.
124        4  Float32      Height of a pixel.

All binary fields are in the same byte order but it may be big endian or
little endian depending on what platform the file was written on.  Usually
this can be checked against the number of bands though this test won't work
if there are more than 255 bands.

There is also some information on .STA and .TRL files at:

  http://www.pcigeomatics.com/cgi-bin/pcihlp/ERDASWR%7CTRAILER+FORMAT

**/

constexpr int ERD_HEADER_SIZE = 128;

/************************************************************************/
/* ==================================================================== */
/*                         LAN4BitRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class LANDataset;

class LAN4BitRasterBand final : public GDALPamRasterBand
{
    GDALColorTable *poCT;
    GDALColorInterp eInterp;

    CPL_DISALLOW_COPY_ASSIGN(LAN4BitRasterBand)

  public:
    LAN4BitRasterBand(LANDataset *, int);
    ~LAN4BitRasterBand() override;

    GDALColorTable *GetColorTable() override;
    GDALColorInterp GetColorInterpretation() override;
    CPLErr SetColorTable(GDALColorTable *) override;
    CPLErr SetColorInterpretation(GDALColorInterp) override;

    CPLErr IReadBlock(int, int, void *) override;
};

/************************************************************************/
/* ==================================================================== */
/*                              LANDataset                              */
/* ==================================================================== */
/************************************************************************/

class LANDataset final : public RawDataset
{
    CPL_DISALLOW_COPY_ASSIGN(LANDataset)

  public:
    VSILFILE *fpImage;  // Image data file.

    char pachHeader[ERD_HEADER_SIZE];

    OGRSpatialReference *m_poSRS = nullptr;

    double adfGeoTransform[6];

    CPLString osSTAFilename{};
    void CheckForStatistics(void);

    char **GetFileList() override;

    CPLErr Close() override;

  public:
    LANDataset();
    ~LANDataset() override;

    CPLErr GetGeoTransform(double *padfTransform) override;

    const OGRSpatialReference *GetSpatialRef() const override;

    static GDALDataset *Open(GDALOpenInfo *);
};

/************************************************************************/
/* ==================================================================== */
/*                         LAN4BitRasterBand                            */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                         LAN4BitRasterBand()                          */
/************************************************************************/

LAN4BitRasterBand::LAN4BitRasterBand(LANDataset *poDSIn, int nBandIn)
    : poCT(nullptr), eInterp(GCI_Undefined)
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = GDT_Byte;

    nBlockXSize = poDSIn->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                         ~LAN4BitRasterBand()                         */
/************************************************************************/

LAN4BitRasterBand::~LAN4BitRasterBand()

{
    if (poCT)
        delete poCT;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr LAN4BitRasterBand::IReadBlock(CPL_UNUSED int nBlockXOff, int nBlockYOff,
                                     void *pImage)

{
    LANDataset *poLAN_DS = reinterpret_cast<LANDataset *>(poDS);
    CPLAssert(nBlockXOff == 0);

    /* -------------------------------------------------------------------- */
    /*      Seek to profile.                                                */
    /* -------------------------------------------------------------------- */
    const vsi_l_offset nOffset =
        ERD_HEADER_SIZE +
        (static_cast<vsi_l_offset>(nBlockYOff) * nRasterXSize *
         poLAN_DS->GetRasterCount()) /
            2 +
        (static_cast<vsi_l_offset>(nBand - 1) * nRasterXSize) / 2;

    if (VSIFSeekL(poLAN_DS->fpImage, nOffset, SEEK_SET) != 0)
    {
        CPLError(CE_Failure, CPLE_FileIO, "LAN Seek failed:%s",
                 VSIStrerror(errno));
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Read the profile.                                               */
    /* -------------------------------------------------------------------- */
    if (VSIFReadL(pImage, 1, nRasterXSize / 2, poLAN_DS->fpImage) !=
        static_cast<size_t>(nRasterXSize) / 2)
    {
        CPLError(CE_Failure, CPLE_FileIO, "LAN Read failed:%s",
                 VSIStrerror(errno));
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Convert 4bit to 8bit.                                           */
    /* -------------------------------------------------------------------- */
    for (int i = nRasterXSize - 1; i >= 0; i--)
    {
        if ((i & 0x01) != 0)
            reinterpret_cast<GByte *>(pImage)[i] =
                reinterpret_cast<GByte *>(pImage)[i / 2] & 0x0f;
        else
            reinterpret_cast<GByte *>(pImage)[i] =
                (reinterpret_cast<GByte *>(pImage)[i / 2] & 0xf0) / 16;
    }

    return CE_None;
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr LAN4BitRasterBand::SetColorTable(GDALColorTable *poNewCT)

{
    if (poCT)
        delete poCT;
    if (poNewCT == nullptr)
        poCT = nullptr;
    else
        poCT = poNewCT->Clone();

    return CE_None;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *LAN4BitRasterBand::GetColorTable()

{
    if (poCT != nullptr)
        return poCT;

    return GDALPamRasterBand::GetColorTable();
}

/************************************************************************/
/*                       SetColorInterpretation()                       */
/************************************************************************/

CPLErr LAN4BitRasterBand::SetColorInterpretation(GDALColorInterp eNewInterp)

{
    eInterp = eNewInterp;

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp LAN4BitRasterBand::GetColorInterpretation()

{
    return eInterp;
}

/************************************************************************/
/* ==================================================================== */
/*                              LANDataset                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             LANDataset()                             */
/************************************************************************/

LANDataset::LANDataset() : fpImage(nullptr)
{
    memset(pachHeader, 0, sizeof(pachHeader));
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 0.0;  // TODO(schwehr): Should this be 1.0?
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 0.0;  // TODO(schwehr): Should this be 1.0?
}

/************************************************************************/
/*                            ~LANDataset()                             */
/************************************************************************/

LANDataset::~LANDataset()

{
    LANDataset::Close();
}

/************************************************************************/
/*                              Close()                                 */
/************************************************************************/

CPLErr LANDataset::Close()
{
    CPLErr eErr = CE_None;
    if (nOpenFlags != OPEN_FLAGS_CLOSED)
    {
        if (LANDataset::FlushCache(true) != CE_None)
            eErr = CE_Failure;

        if (fpImage)
        {
            if (VSIFCloseL(fpImage) != 0)
            {
                CPLError(CE_Failure, CPLE_FileIO, "I/O error");
                eErr = CE_Failure;
            }
        }

        if (m_poSRS)
            m_poSRS->Release();

        if (GDALPamDataset::Close() != CE_None)
            eErr = CE_Failure;
    }
    return eErr;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *LANDataset::Open(GDALOpenInfo *poOpenInfo)

{
    /* -------------------------------------------------------------------- */
    /*      We assume the user is pointing to the header (.pcb) file.       */
    /*      Does this appear to be a pcb file?                              */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->nHeaderBytes < ERD_HEADER_SIZE ||
        poOpenInfo->fpL == nullptr)
        return nullptr;

    if (!STARTS_WITH_CI(reinterpret_cast<char *>(poOpenInfo->pabyHeader),
                        "HEADER") &&
        !STARTS_WITH_CI(reinterpret_cast<char *>(poOpenInfo->pabyHeader),
                        "HEAD74"))
        return nullptr;

    if (memcmp(poOpenInfo->pabyHeader + 16, "S LAT   ", 8) == 0)
    {
        // NTV1 format
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    auto poDS = std::make_unique<LANDataset>();

    poDS->eAccess = poOpenInfo->eAccess;
    std::swap(poDS->fpImage, poOpenInfo->fpL);

    /* -------------------------------------------------------------------- */
    /*      Do we need to byte swap the headers to local machine order?     */
    /* -------------------------------------------------------------------- */
    const RawRasterBand::ByteOrder eByteOrder =
        poOpenInfo->pabyHeader[8] == 0
            ? RawRasterBand::ByteOrder::ORDER_BIG_ENDIAN
            : RawRasterBand::ByteOrder::ORDER_LITTLE_ENDIAN;

    memcpy(poDS->pachHeader, poOpenInfo->pabyHeader, ERD_HEADER_SIZE);

    if (eByteOrder != RawRasterBand::NATIVE_BYTE_ORDER)
    {
        CPL_SWAP16PTR(poDS->pachHeader + 6);
        CPL_SWAP16PTR(poDS->pachHeader + 8);

        CPL_SWAP32PTR(poDS->pachHeader + 16);
        CPL_SWAP32PTR(poDS->pachHeader + 20);
        CPL_SWAP32PTR(poDS->pachHeader + 24);
        CPL_SWAP32PTR(poDS->pachHeader + 28);

        CPL_SWAP16PTR(poDS->pachHeader + 88);
        CPL_SWAP16PTR(poDS->pachHeader + 90);

        CPL_SWAP16PTR(poDS->pachHeader + 106);
        CPL_SWAP32PTR(poDS->pachHeader + 108);
        CPL_SWAP32PTR(poDS->pachHeader + 112);
        CPL_SWAP32PTR(poDS->pachHeader + 116);
        CPL_SWAP32PTR(poDS->pachHeader + 120);
        CPL_SWAP32PTR(poDS->pachHeader + 124);
    }

    /* -------------------------------------------------------------------- */
    /*      Capture some information from the file that is of interest.     */
    /* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(poDS->pachHeader, "HEADER"))
    {
        float fTmp = 0.0;
        memcpy(&fTmp, poDS->pachHeader + 16, 4);
        poDS->nRasterXSize = static_cast<int>(fTmp);
        memcpy(&fTmp, poDS->pachHeader + 20, 4);
        poDS->nRasterYSize = static_cast<int>(fTmp);
    }
    else
    {
        GInt32 nTmp = 0;
        memcpy(&nTmp, poDS->pachHeader + 16, 4);
        poDS->nRasterXSize = nTmp;
        memcpy(&nTmp, poDS->pachHeader + 20, 4);
        poDS->nRasterYSize = nTmp;
    }

    GInt16 nTmp16 = 0;
    memcpy(&nTmp16, poDS->pachHeader + 6, 2);

    int nPixelOffset = 0;
    GDALDataType eDataType = GDT_Unknown;
    if (nTmp16 == 0)
    {
        eDataType = GDT_Byte;
        nPixelOffset = 1;
    }
    else if (nTmp16 == 1)  // 4 bit
    {
        eDataType = GDT_Byte;
        nPixelOffset = -1;
    }
    else if (nTmp16 == 2)
    {
        nPixelOffset = 2;
        eDataType = GDT_Int16;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unsupported pixel type (%d).",
                 nTmp16);
        return nullptr;
    }

    memcpy(&nTmp16, poDS->pachHeader + 8, 2);
    const int nBandCount = nTmp16;

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize) ||
        !GDALCheckBandCount(nBandCount, FALSE))
    {
        return nullptr;
    }

    // cppcheck-suppress knownConditionTrueFalse
    if (nPixelOffset != -1 &&
        poDS->nRasterXSize > INT_MAX / (nPixelOffset * nBandCount))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Int overflow occurred.");
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Create band information object.                                 */
    /* -------------------------------------------------------------------- */
    for (int iBand = 1; iBand <= nBandCount; iBand++)
    {
        if (nPixelOffset == -1) /* 4 bit case */
            poDS->SetBand(iBand, new LAN4BitRasterBand(poDS.get(), iBand));
        else
        {
            auto poBand = RawRasterBand::Create(
                poDS.get(), iBand, poDS->fpImage,
                ERD_HEADER_SIZE +
                    (iBand - 1) * nPixelOffset * poDS->nRasterXSize,
                nPixelOffset, poDS->nRasterXSize * nPixelOffset * nBandCount,
                eDataType, eByteOrder, RawRasterBand::OwnFP::NO);
            if (!poBand)
                return nullptr;
            poDS->SetBand(iBand, std::move(poBand));
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Initialize any PAM information.                                 */
    /* -------------------------------------------------------------------- */
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->CheckForStatistics();
    poDS->TryLoadXML();

    /* -------------------------------------------------------------------- */
    /*      Check for overviews.                                            */
    /* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize(poDS.get(), poOpenInfo->pszFilename);

    /* -------------------------------------------------------------------- */
    /*      Try to interpret georeferencing.                                */
    /* -------------------------------------------------------------------- */
    float fTmp = 0.0;

    memcpy(&fTmp, poDS->pachHeader + 112, 4);
    poDS->adfGeoTransform[0] = fTmp;
    memcpy(&fTmp, poDS->pachHeader + 120, 4);
    poDS->adfGeoTransform[1] = fTmp;
    poDS->adfGeoTransform[2] = 0.0;
    memcpy(&fTmp, poDS->pachHeader + 116, 4);
    poDS->adfGeoTransform[3] = fTmp;
    poDS->adfGeoTransform[4] = 0.0;
    memcpy(&fTmp, poDS->pachHeader + 124, 4);
    poDS->adfGeoTransform[5] = -fTmp;

    // adjust for center of pixel vs. top left corner of pixel.
    poDS->adfGeoTransform[0] -= poDS->adfGeoTransform[1] * 0.5;
    poDS->adfGeoTransform[3] -= poDS->adfGeoTransform[5] * 0.5;

    /* -------------------------------------------------------------------- */
    /*      If we didn't get any georeferencing, try for a worldfile.       */
    /* -------------------------------------------------------------------- */
    if (poDS->adfGeoTransform[1] == 0.0 || poDS->adfGeoTransform[5] == 0.0)
    {
        if (!GDALReadWorldFile(poOpenInfo->pszFilename, nullptr,
                               poDS->adfGeoTransform))
            GDALReadWorldFile(poOpenInfo->pszFilename, ".wld",
                              poDS->adfGeoTransform);
    }

    /* -------------------------------------------------------------------- */
    /*      Try to come up with something for the coordinate system.        */
    /* -------------------------------------------------------------------- */
    memcpy(&nTmp16, poDS->pachHeader + 88, 2);
    int nCoordSys = nTmp16;

    poDS->m_poSRS = new OGRSpatialReference();
    poDS->m_poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    if (nCoordSys == 0)
    {
        poDS->m_poSRS->SetFromUserInput(SRS_WKT_WGS84_LAT_LONG);
    }
    else if (nCoordSys == 1)
    {
        poDS->m_poSRS->SetFromUserInput(
            "LOCAL_CS[\"UTM - Zone Unknown\",UNIT[\"Meter\",1]]");
    }
    else if (nCoordSys == 2)
    {
        poDS->m_poSRS->SetFromUserInput(
            "LOCAL_CS[\"State Plane - Zone Unknown\","
            "UNIT[\"US survey foot\",0.3048006096012192]]");
    }
    else
    {
        poDS->m_poSRS->SetFromUserInput(
            "LOCAL_CS[\"Unknown\",UNIT[\"Meter\",1]]");
    }

    /* -------------------------------------------------------------------- */
    /*      Check for a trailer file with a colormap in it.                 */
    /* -------------------------------------------------------------------- */
    char *pszPath = CPLStrdup(CPLGetPathSafe(poOpenInfo->pszFilename).c_str());
    char *pszBasename =
        CPLStrdup(CPLGetBasenameSafe(poOpenInfo->pszFilename).c_str());
    const std::string osTRLFilename =
        CPLFormCIFilenameSafe(pszPath, pszBasename, "trl");
    VSILFILE *fpTRL = VSIFOpenL(osTRLFilename.c_str(), "rb");
    if (fpTRL != nullptr)
    {
        char szTRLData[896] = {'\0'};

        CPL_IGNORE_RET_VAL(VSIFReadL(szTRLData, 1, 896, fpTRL));
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpTRL));

        GDALColorTable oCT;
        for (int iColor = 0; iColor < 256; iColor++)
        {
            GDALColorEntry sEntry = {0, 0, 0, 0};

            sEntry.c2 = reinterpret_cast<GByte *>(szTRLData)[iColor + 128];
            sEntry.c1 =
                reinterpret_cast<GByte *>(szTRLData)[iColor + 128 + 256];
            sEntry.c3 =
                reinterpret_cast<GByte *>(szTRLData)[iColor + 128 + 512];
            sEntry.c4 = 255;
            oCT.SetColorEntry(iColor, &sEntry);

            // Only 16 colors in 4bit files.
            if (nPixelOffset == -1 && iColor == 15)
                break;
        }

        poDS->GetRasterBand(1)->SetColorTable(&oCT);
        poDS->GetRasterBand(1)->SetColorInterpretation(GCI_PaletteIndex);
    }

    CPLFree(pszPath);
    CPLFree(pszBasename);

    return poDS.release();
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr LANDataset::GetGeoTransform(double *padfTransform)

{
    if (adfGeoTransform[1] != 0.0 && adfGeoTransform[5] != 0.0)
    {
        memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6);
        return CE_None;
    }

    return GDALPamDataset::GetGeoTransform(padfTransform);
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/*                                                                      */
/*      Use PAM coordinate system if available in preference to the     */
/*      generally poor value derived from the file itself.              */
/************************************************************************/

const OGRSpatialReference *LANDataset::GetSpatialRef() const

{
    const auto poSRS = GDALPamDataset::GetSpatialRef();
    if (poSRS)
        return poSRS;

    return m_poSRS;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **LANDataset::GetFileList()

{
    // Main data file, etc.
    char **papszFileList = GDALPamDataset::GetFileList();

    if (!osSTAFilename.empty())
        papszFileList = CSLAddString(papszFileList, osSTAFilename);

    return papszFileList;
}

/************************************************************************/
/*                         CheckForStatistics()                         */
/************************************************************************/

void LANDataset::CheckForStatistics()

{
    /* -------------------------------------------------------------------- */
    /*      Do we have a statistics file?                                   */
    /* -------------------------------------------------------------------- */
    osSTAFilename = CPLResetExtensionSafe(GetDescription(), "sta");

    VSILFILE *fpSTA = VSIFOpenL(osSTAFilename, "r");

    if (fpSTA == nullptr && VSIIsCaseSensitiveFS(osSTAFilename))
    {
        osSTAFilename = CPLResetExtensionSafe(GetDescription(), "STA");
        fpSTA = VSIFOpenL(osSTAFilename, "r");
    }

    if (fpSTA == nullptr)
    {
        osSTAFilename = "";
        return;
    }

    /* -------------------------------------------------------------------- */
    /*      Read it one band at a time.                                     */
    /* -------------------------------------------------------------------- */
    GByte abyBandInfo[1152] = {'\0'};

    for (int iBand = 0; iBand < nBands; iBand++)
    {
        if (VSIFReadL(abyBandInfo, 1152, 1, fpSTA) != 1)
            break;

        const int nBandNumber = abyBandInfo[7];
        GDALRasterBand *poBand = GetRasterBand(nBandNumber);
        if (poBand == nullptr)
            break;

        GInt16 nMin = 0;
        GInt16 nMax = 0;

        if (poBand->GetRasterDataType() != GDT_Byte)
        {
            memcpy(&nMin, abyBandInfo + 28, 2);
            memcpy(&nMax, abyBandInfo + 30, 2);
            CPL_LSBPTR16(&nMin);
            CPL_LSBPTR16(&nMax);
        }
        else
        {
            nMin = abyBandInfo[9];
            nMax = abyBandInfo[8];
        }

        float fMean = 0.0;
        float fStdDev = 0.0;
        memcpy(&fMean, abyBandInfo + 12, 4);
        memcpy(&fStdDev, abyBandInfo + 24, 4);
        CPL_LSBPTR32(&fMean);
        CPL_LSBPTR32(&fStdDev);

        poBand->SetStatistics(nMin, nMax, fMean, fStdDev);
    }

    CPL_IGNORE_RET_VAL(VSIFCloseL(fpSTA));
}

/************************************************************************/
/*                          GDALRegister_LAN()                          */
/************************************************************************/

void GDALRegister_LAN()

{
    if (GDALGetDriverByName("LAN") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("LAN");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Erdas .LAN/.GIS");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/lan.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = LANDataset::Open;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
