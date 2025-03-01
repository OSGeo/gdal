/******************************************************************************
 *
 * Project:  Horizontal Datum Formats
 * Purpose:  Implementation of NTv2 datum shift format used in Canada, France,
 *           Australia and elsewhere.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 * Financial Support: i-cubed (http://www.i-cubed.com)
 *
 ******************************************************************************
 * Copyright (c) 2010, Frank Warmerdam
 * Copyright (c) 2010-2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

// TODO(schwehr): There are a lot of magic numbers in this driver that should
// be changed to constants and documented.

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "ogr_srs_api.h"
#include "rawdataset.h"

#include <algorithm>

// Format documentation: https://github.com/Esri/ntv2-file-routines
// Original archived specification:
// https://web.archive.org/web/20091227232322/http://www.mgs.gov.on.ca/stdprodconsume/groups/content/@mgs/@iandit/documents/resourcelist/stel02_047447.pdf

/**
 * The header for the file, and each grid consists of 11 16byte records.
 * The first half is an ASCII label, and the second half is the value
 * often in a little endian int or float.
 *
 * Example:

00000000  4e 55 4d 5f 4f 52 45 43  0b 00 00 00 00 00 00 00  |NUM_OREC........|
00000010  4e 55 4d 5f 53 52 45 43  0b 00 00 00 00 00 00 00  |NUM_SREC........|
00000020  4e 55 4d 5f 46 49 4c 45  01 00 00 00 00 00 00 00  |NUM_FILE........|
00000030  47 53 5f 54 59 50 45 20  53 45 43 4f 4e 44 53 20  |GS_TYPE SECONDS |
00000040  56 45 52 53 49 4f 4e 20  49 47 4e 30 37 5f 30 31  |VERSION IGN07_01|
00000050  53 59 53 54 45 4d 5f 46  4e 54 46 20 20 20 20 20  |SYSTEM_FNTF     |
00000060  53 59 53 54 45 4d 5f 54  52 47 46 39 33 20 20 20  |SYSTEM_TRGF93   |
00000070  4d 41 4a 4f 52 5f 46 20  cd cc cc 4c c2 54 58 41  |MAJOR_F ...L.TXA|
00000080  4d 49 4e 4f 52 5f 46 20  00 00 00 c0 88 3f 58 41  |MINOR_F .....?XA|
00000090  4d 41 4a 4f 52 5f 54 20  00 00 00 40 a6 54 58 41  |MAJOR_T ...@.TXA|
000000a0  4d 49 4e 4f 52 5f 54 20  27 e0 1a 14 c4 3f 58 41  |MINOR_T '....?XA|
000000b0  53 55 42 5f 4e 41 4d 45  46 52 41 4e 43 45 20 20  |SUB_NAMEFRANCE  |
000000c0  50 41 52 45 4e 54 20 20  4e 4f 4e 45 20 20 20 20  |PARENT  NONE    |
000000d0  43 52 45 41 54 45 44 20  33 31 2f 31 30 2f 30 37  |CREATED 31/10/07|
000000e0  55 50 44 41 54 45 44 20  20 20 20 20 20 20 20 20  |UPDATED         |
000000f0  53 5f 4c 41 54 20 20 20  00 00 00 00 80 04 02 41  |S_LAT   .......A|
00000100  4e 5f 4c 41 54 20 20 20  00 00 00 00 00 da 06 41  |N_LAT   .......A|
00000110  45 5f 4c 4f 4e 47 20 20  00 00 00 00 00 94 e1 c0  |E_LONG  ........|
00000120  57 5f 4c 4f 4e 47 20 20  00 00 00 00 00 56 d3 40  |W_LONG  .....V.@|
00000130  4c 41 54 5f 49 4e 43 20  00 00 00 00 00 80 76 40  |LAT_INC ......v@|
00000140  4c 4f 4e 47 5f 49 4e 43  00 00 00 00 00 80 76 40  |LONG_INC......v@|
00000150  47 53 5f 43 4f 55 4e 54  a4 43 00 00 00 00 00 00  |GS_COUNT.C......|
00000160  94 f7 c1 3e 70 ee a3 3f  2a c7 84 3d ff 42 af 3d  |...>p..?*..=.B.=|

the actual grid data is a raster with 4 float32 bands (lat offset, long
offset, lat error, long error).  The offset values are in arc seconds.
The grid is flipped in the x and y axis from our usual GDAL orientation.
That is, the first pixel is the south east corner with scanlines going
east to west, and rows from south to north.  As a GDAL dataset we represent
these both in the more conventional orientation.
 */

constexpr size_t knREGULAR_RECORD_SIZE = 16;
// This one is for velocity grids such as the NAD83(CRSR)v7 / NAD83v70VG.gvb
// which is the only example I know actually of that format variant.
constexpr size_t knMAX_RECORD_SIZE = 24;

/************************************************************************/
/* ==================================================================== */
/*                              NTv2Dataset                             */
/* ==================================================================== */
/************************************************************************/

class NTv2Dataset final : public RawDataset
{
  public:
    RawRasterBand::ByteOrder m_eByteOrder = RawRasterBand::NATIVE_BYTE_ORDER;
    bool m_bMustSwap = false;
    VSILFILE *fpImage = nullptr;  // image data file.

    size_t nRecordSize = 0;
    vsi_l_offset nGridOffset = 0;

    OGRSpatialReference m_oSRS{};
    double adfGeoTransform[6];

    void CaptureMetadataItem(const char *pszItem);

    bool OpenGrid(const char *pachGridHeader, vsi_l_offset nDataStart);

    CPL_DISALLOW_COPY_ASSIGN(NTv2Dataset)

    CPLErr Close() override;

  public:
    NTv2Dataset();
    ~NTv2Dataset() override;

    CPLErr GetGeoTransform(double *padfTransform) override;

    const OGRSpatialReference *GetSpatialRef() const override
    {
        return &m_oSRS;
    }

    static GDALDataset *Open(GDALOpenInfo *);
    static int Identify(GDALOpenInfo *);
};

/************************************************************************/
/* ==================================================================== */
/*                              NTv2Dataset                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             NTv2Dataset()                          */
/************************************************************************/

NTv2Dataset::NTv2Dataset()
{
    m_oSRS.SetFromUserInput(SRS_WKT_WGS84_LAT_LONG);
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 0.0;  // TODO(schwehr): Should this be 1.0?
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 0.0;  // TODO(schwehr): Should this be 1.0?
}

/************************************************************************/
/*                            ~NTv2Dataset()                            */
/************************************************************************/

NTv2Dataset::~NTv2Dataset()

{
    NTv2Dataset::Close();
}

/************************************************************************/
/*                              Close()                                 */
/************************************************************************/

CPLErr NTv2Dataset::Close()
{
    CPLErr eErr = CE_None;
    if (nOpenFlags != OPEN_FLAGS_CLOSED)
    {
        if (fpImage)
        {
            if (VSIFCloseL(fpImage) != 0)
            {
                CPLError(CE_Failure, CPLE_FileIO, "I/O error");
                eErr = CE_Failure;
            }
        }

        if (GDALPamDataset::Close() != CE_None)
            eErr = CE_Failure;
    }
    return eErr;
}

/************************************************************************/
/*                        SwapPtr32IfNecessary()                        */
/************************************************************************/

static void SwapPtr32IfNecessary(bool bMustSwap, void *ptr)
{
    if (bMustSwap)
    {
        CPL_SWAP32PTR(static_cast<GByte *>(ptr));
    }
}

/************************************************************************/
/*                        SwapPtr64IfNecessary()                        */
/************************************************************************/

static void SwapPtr64IfNecessary(bool bMustSwap, void *ptr)
{
    if (bMustSwap)
    {
        CPL_SWAP64PTR(static_cast<GByte *>(ptr));
    }
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int NTv2Dataset::Identify(GDALOpenInfo *poOpenInfo)

{
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "NTv2:"))
        return TRUE;

    if (poOpenInfo->nHeaderBytes < 64)
        return FALSE;

    const char *pszHeader =
        reinterpret_cast<const char *>(poOpenInfo->pabyHeader);
    if (!STARTS_WITH_CI(pszHeader + 0, "NUM_OREC"))
        return FALSE;

    if (!STARTS_WITH_CI(pszHeader + knREGULAR_RECORD_SIZE, "NUM_SREC") &&
        !STARTS_WITH_CI(pszHeader + knMAX_RECORD_SIZE, "NUM_SREC"))
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *NTv2Dataset::Open(GDALOpenInfo *poOpenInfo)

{
    if (!Identify(poOpenInfo) || poOpenInfo->eAccess == GA_Update)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Are we targeting a particular grid?                             */
    /* -------------------------------------------------------------------- */
    CPLString osFilename;
    int iTargetGrid = -1;

    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "NTv2:"))
    {
        const char *pszRest = poOpenInfo->pszFilename + 5;

        iTargetGrid = atoi(pszRest);
        while (*pszRest != '\0' && *pszRest != ':')
            pszRest++;

        if (*pszRest == ':')
            pszRest++;

        osFilename = pszRest;
    }
    else
    {
        osFilename = poOpenInfo->pszFilename;
    }

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    auto poDS = std::make_unique<NTv2Dataset>();
    poDS->eAccess = poOpenInfo->eAccess;

    /* -------------------------------------------------------------------- */
    /*      Open the file.                                                  */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->eAccess == GA_ReadOnly)
        poDS->fpImage = VSIFOpenL(osFilename, "rb");
    else
        poDS->fpImage = VSIFOpenL(osFilename, "rb+");

    if (poDS->fpImage == nullptr)
    {
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Read the file header.                                           */
    /* -------------------------------------------------------------------- */
    char achHeader[11 * knMAX_RECORD_SIZE] = {0};

    if (VSIFSeekL(poDS->fpImage, 0, SEEK_SET) != 0 ||
        VSIFReadL(achHeader, 1, 64, poDS->fpImage) != 64)
    {
        return nullptr;
    }

    poDS->nRecordSize =
        STARTS_WITH_CI(achHeader + knMAX_RECORD_SIZE, "NUM_SREC")
            ? knMAX_RECORD_SIZE
            : knREGULAR_RECORD_SIZE;
    if (VSIFReadL(achHeader + 64, 1, 11 * poDS->nRecordSize - 64,
                  poDS->fpImage) != 11 * poDS->nRecordSize - 64)
    {
        return nullptr;
    }

    const bool bIsLE = achHeader[8] == 11 && achHeader[9] == 0 &&
                       achHeader[10] == 0 && achHeader[11] == 0;
    const bool bIsBE = achHeader[8] == 0 && achHeader[9] == 0 &&
                       achHeader[10] == 0 && achHeader[11] == 11;
    if (!bIsLE && !bIsBE)
    {
        return nullptr;
    }
    poDS->m_eByteOrder = bIsLE ? RawRasterBand::ByteOrder::ORDER_LITTLE_ENDIAN
                               : RawRasterBand::ByteOrder::ORDER_BIG_ENDIAN;
    poDS->m_bMustSwap = poDS->m_eByteOrder != RawRasterBand::NATIVE_BYTE_ORDER;

    SwapPtr32IfNecessary(poDS->m_bMustSwap,
                         achHeader + 2 * poDS->nRecordSize + 8);
    GInt32 nSubFileCount = 0;
    memcpy(&nSubFileCount, achHeader + 2 * poDS->nRecordSize + 8, 4);
    if (nSubFileCount <= 0 || nSubFileCount >= 1024)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid value for NUM_FILE : %d",
                 nSubFileCount);
        return nullptr;
    }

    poDS->CaptureMetadataItem(achHeader + 3 * poDS->nRecordSize);
    poDS->CaptureMetadataItem(achHeader + 4 * poDS->nRecordSize);
    poDS->CaptureMetadataItem(achHeader + 5 * poDS->nRecordSize);
    poDS->CaptureMetadataItem(achHeader + 6 * poDS->nRecordSize);

    double dfValue = 0.0;
    memcpy(&dfValue, achHeader + 7 * poDS->nRecordSize + 8, 8);
    SwapPtr64IfNecessary(poDS->m_bMustSwap, &dfValue);
    CPLString osFValue;
    osFValue.Printf("%.15g", dfValue);
    poDS->SetMetadataItem("MAJOR_F", osFValue);

    memcpy(&dfValue, achHeader + 8 * poDS->nRecordSize + 8, 8);
    SwapPtr64IfNecessary(poDS->m_bMustSwap, &dfValue);
    osFValue.Printf("%.15g", dfValue);
    poDS->SetMetadataItem("MINOR_F", osFValue);

    memcpy(&dfValue, achHeader + 9 * poDS->nRecordSize + 8, 8);
    SwapPtr64IfNecessary(poDS->m_bMustSwap, &dfValue);
    osFValue.Printf("%.15g", dfValue);
    poDS->SetMetadataItem("MAJOR_T", osFValue);

    memcpy(&dfValue, achHeader + 10 * poDS->nRecordSize + 8, 8);
    SwapPtr64IfNecessary(poDS->m_bMustSwap, &dfValue);
    osFValue.Printf("%.15g", dfValue);
    poDS->SetMetadataItem("MINOR_T", osFValue);

    /* ==================================================================== */
    /*      Loop over grids.                                                */
    /* ==================================================================== */
    vsi_l_offset nGridOffset = 11 * poDS->nRecordSize;

    for (int iGrid = 0; iGrid < nSubFileCount; iGrid++)
    {
        if (VSIFSeekL(poDS->fpImage, nGridOffset, SEEK_SET) < 0 ||
            VSIFReadL(achHeader, 11, poDS->nRecordSize, poDS->fpImage) !=
                poDS->nRecordSize)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot read header for subfile %d", iGrid);
            return nullptr;
        }

        for (int i = 4; i <= 9; i++)
            SwapPtr64IfNecessary(poDS->m_bMustSwap,
                                 achHeader + i * poDS->nRecordSize + 8);

        SwapPtr32IfNecessary(poDS->m_bMustSwap,
                             achHeader + 10 * poDS->nRecordSize + 8);

        GUInt32 nGSCount = 0;
        memcpy(&nGSCount, achHeader + 10 * poDS->nRecordSize + 8, 4);

        CPLString osSubName;
        osSubName.assign(achHeader + 8, 8);
        osSubName.Trim();

        // If this is our target grid, open it as a dataset.
        if (iTargetGrid == iGrid || (iTargetGrid == -1 && iGrid == 0))
        {
            if (!poDS->OpenGrid(achHeader, nGridOffset))
            {
                return nullptr;
            }
        }

        // If we are opening the file as a whole, list subdatasets.
        if (iTargetGrid == -1)
        {
            CPLString osKey;
            CPLString osValue;
            osKey.Printf("SUBDATASET_%d_NAME", iGrid);
            osValue.Printf("NTv2:%d:%s", iGrid, osFilename.c_str());
            poDS->SetMetadataItem(osKey, osValue, "SUBDATASETS");

            osKey.Printf("SUBDATASET_%d_DESC", iGrid);
            osValue.Printf("%s", osSubName.c_str());
            poDS->SetMetadataItem(osKey, osValue, "SUBDATASETS");
        }

        nGridOffset +=
            (11 + static_cast<vsi_l_offset>(nGSCount)) * poDS->nRecordSize;
    }

    /* -------------------------------------------------------------------- */
    /*      Initialize any PAM information.                                 */
    /* -------------------------------------------------------------------- */
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->TryLoadXML();

    /* -------------------------------------------------------------------- */
    /*      Check for overviews.                                            */
    /* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize(poDS.get(), poOpenInfo->pszFilename);

    return poDS.release();
}

/************************************************************************/
/*                              OpenGrid()                              */
/*                                                                      */
/*      Note that the caller will already have byte swapped needed      */
/*      portions of the header.                                         */
/************************************************************************/

bool NTv2Dataset::OpenGrid(const char *pachHeader, vsi_l_offset nGridOffsetIn)

{
    nGridOffset = nGridOffsetIn;

    /* -------------------------------------------------------------------- */
    /*      Read the grid header.                                           */
    /* -------------------------------------------------------------------- */
    CaptureMetadataItem(pachHeader + 0 * nRecordSize);
    CaptureMetadataItem(pachHeader + 1 * nRecordSize);
    CaptureMetadataItem(pachHeader + 2 * nRecordSize);
    CaptureMetadataItem(pachHeader + 3 * nRecordSize);

    double s_lat, n_lat, e_long, w_long, lat_inc, long_inc;
    memcpy(&s_lat, pachHeader + 4 * nRecordSize + 8, 8);
    memcpy(&n_lat, pachHeader + 5 * nRecordSize + 8, 8);
    memcpy(&e_long, pachHeader + 6 * nRecordSize + 8, 8);
    memcpy(&w_long, pachHeader + 7 * nRecordSize + 8, 8);
    memcpy(&lat_inc, pachHeader + 8 * nRecordSize + 8, 8);
    memcpy(&long_inc, pachHeader + 9 * nRecordSize + 8, 8);

    e_long *= -1;
    w_long *= -1;

    if (long_inc == 0.0 || lat_inc == 0.0)
        return false;
    const double dfXSize = floor((e_long - w_long) / long_inc + 1.5);
    const double dfYSize = floor((n_lat - s_lat) / lat_inc + 1.5);
    if (!(dfXSize >= 0 && dfXSize < INT_MAX) ||
        !(dfYSize >= 0 && dfYSize < INT_MAX))
        return false;
    nRasterXSize = static_cast<int>(dfXSize);
    nRasterYSize = static_cast<int>(dfYSize);

    const int l_nBands = nRecordSize == knREGULAR_RECORD_SIZE ? 4 : 6;
    const int nPixelSize = l_nBands * 4;

    if (!GDALCheckDatasetDimensions(nRasterXSize, nRasterYSize))
        return false;
    if (nRasterXSize > INT_MAX / nPixelSize)
        return false;

    /* -------------------------------------------------------------------- */
    /*      Create band information object.                                 */
    /*                                                                      */
    /*      We use unusual offsets to remap from bottom to top, to top      */
    /*      to bottom orientation, and also to remap east to west, to       */
    /*      west to east.                                                   */
    /* -------------------------------------------------------------------- */
    for (int iBand = 0; iBand < l_nBands; iBand++)
    {
        auto poBand = RawRasterBand::Create(
            this, iBand + 1, fpImage,
            nGridOffset + 4 * iBand + 11 * nRecordSize +
                static_cast<vsi_l_offset>(nRasterXSize - 1) * nPixelSize +
                static_cast<vsi_l_offset>(nRasterYSize - 1) * nPixelSize *
                    nRasterXSize,
            -nPixelSize, -nPixelSize * nRasterXSize, GDT_Float32, m_eByteOrder,
            RawRasterBand::OwnFP::NO);
        if (!poBand)
            return false;
        SetBand(iBand + 1, std::move(poBand));
    }

    if (l_nBands == 4)
    {
        GetRasterBand(1)->SetDescription("Latitude Offset (arc seconds)");
        GetRasterBand(2)->SetDescription("Longitude Offset (arc seconds)");
        GetRasterBand(2)->SetMetadataItem("positive_value", "west");
        GetRasterBand(3)->SetDescription("Latitude Error");
        GetRasterBand(4)->SetDescription("Longitude Error");
    }
    else
    {
        // A bit surprising that the order is easting, northing here, contrary
        // to the classic NTv2 order.... Verified on NAD83v70VG.gvb
        // (https://webapp.geod.nrcan.gc.ca/geod/process/download-helper.php?file_id=NAD83v70VG)
        // against the TRX software
        // (https://webapp.geod.nrcan.gc.ca/geod/process/download-helper.php?file_id=trx)
        // https://webapp.geod.nrcan.gc.ca/geod/tools-outils/nad83-docs.php
        // Unfortunately I couldn't find an official documentation of the format
        // !
        GetRasterBand(1)->SetDescription("East velocity (mm/year)");
        GetRasterBand(2)->SetDescription("North velocity (mm/year)");
        GetRasterBand(3)->SetDescription("Up velocity (mm/year)");
        GetRasterBand(4)->SetDescription("East velocity Error (mm/year)");
        GetRasterBand(5)->SetDescription("North velocity Error (mm/year)");
        GetRasterBand(6)->SetDescription("Up velocity Error (mm/year)");
    }

    /* -------------------------------------------------------------------- */
    /*      Setup georeferencing.                                           */
    /* -------------------------------------------------------------------- */
    adfGeoTransform[0] = (w_long - long_inc * 0.5) / 3600.0;
    adfGeoTransform[1] = long_inc / 3600.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = (n_lat + lat_inc * 0.5) / 3600.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = (-1 * lat_inc) / 3600.0;

    return true;
}

/************************************************************************/
/*                        CaptureMetadataItem()                         */
/************************************************************************/

void NTv2Dataset::CaptureMetadataItem(const char *pszItem)

{
    CPLString osKey;
    CPLString osValue;

    osKey.assign(pszItem, 8);
    osValue.assign(pszItem + 8, 8);

    SetMetadataItem(osKey.Trim(), osValue.Trim());
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr NTv2Dataset::GetGeoTransform(double *padfTransform)

{
    memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6);
    return CE_None;
}

/************************************************************************/
/*                         GDALRegister_NTv2()                          */
/************************************************************************/

void GDALRegister_NTv2()

{
    if (GDALGetDriverByName("NTv2") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("NTv2");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "NTv2 Datum Grid Shift");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "gsb gvb");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");

    poDriver->pfnOpen = NTv2Dataset::Open;
    poDriver->pfnIdentify = NTv2Dataset::Identify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
