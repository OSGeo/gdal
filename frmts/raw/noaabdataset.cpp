/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Implementation of NOAA .b format used for GEOCON / NADCON5 grids
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "gdal_frmts.h"
#include "rawdataset.h"
#include "ogr_srs_api.h"

#include <limits>

// Specification of the format is at "paragraph 10.2 ".b" grids (GEOCON and
// NADCON 5.0)" of "NOAA Technical Report NOS NGS 63" at
// https://geodesy.noaa.gov/library/pdfs/NOAA_TR_NOS_NGS_0063.pdf

constexpr int HEADER_SIZE = 52;
constexpr int FORTRAN_HEADER_SIZE = 4;
constexpr int FORTRAN_TRAILER_SIZE = 4;

/************************************************************************/
/* ==================================================================== */
/*                          NOAA_B_Dataset                              */
/* ==================================================================== */
/************************************************************************/

class NOAA_B_Dataset final : public RawDataset
{
    OGRSpatialReference m_oSRS{};
    double m_adfGeoTransform[6];

    CPL_DISALLOW_COPY_ASSIGN(NOAA_B_Dataset)

    static int IdentifyEx(GDALOpenInfo *poOpenInfo, bool &bBigEndianOut);

    CPLErr Close() override
    {
        return GDALPamDataset::Close();
    }

  public:
    NOAA_B_Dataset()
    {
        m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        m_adfGeoTransform[0] = 0.0;
        m_adfGeoTransform[1] = 1.0;
        m_adfGeoTransform[2] = 0.0;
        m_adfGeoTransform[3] = 0.0;
        m_adfGeoTransform[4] = 0.0;
        m_adfGeoTransform[5] = 1.0;
    }

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
/*                          NOAA_B_Dataset                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           GetHeaderValues()                          */
/************************************************************************/

static void GetHeaderValues(const GDALOpenInfo *poOpenInfo, double &dfSWLat,
                            double &dfSWLon, double &dfDeltaLat,
                            double &dfDeltaLon, int32_t &nRows, int32_t &nCols,
                            int32_t &iKind, bool bBigEndian)
{
    const auto ReadFloat64 = [bBigEndian](const GByte *&ptr)
    {
        double v;
        memcpy(&v, ptr, sizeof(v));
        ptr += sizeof(v);
        if (bBigEndian)
            CPL_MSBPTR64(&v);
        else
            CPL_LSBPTR64(&v);
        return v;
    };

    const auto ReadInt32 = [bBigEndian](const GByte *&ptr)
    {
        int32_t v;
        memcpy(&v, ptr, sizeof(v));
        ptr += sizeof(v);
        if (bBigEndian)
            CPL_MSBPTR32(&v);
        else
            CPL_LSBPTR32(&v);
        return v;
    };

    const GByte *ptr = poOpenInfo->pabyHeader + FORTRAN_HEADER_SIZE;

    dfSWLat = ReadFloat64(ptr);
    dfSWLon = ReadFloat64(ptr);
    dfDeltaLat = ReadFloat64(ptr);
    dfDeltaLon = ReadFloat64(ptr);

    nRows = ReadInt32(ptr);
    nCols = ReadInt32(ptr);
    iKind = ReadInt32(ptr);
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int NOAA_B_Dataset::IdentifyEx(GDALOpenInfo *poOpenInfo, bool &bBigEndianOut)

{
    if (poOpenInfo->nHeaderBytes < HEADER_SIZE)
        return FALSE;

#if !defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
    if (!EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "b"))
        return FALSE;
#endif

    // Sanity checks on header
    double dfSWLat;
    double dfSWLon;
    double dfDeltaLat;
    double dfDeltaLon;
    int32_t nRows;
    int32_t nCols;
    int32_t iKind;

    // Fun... nadcon5 files are encoded in big-endian, but vertcon3 files...
    // in little-endian. We could probably figure that out directly from the
    // 4 bytes which are 0x00 0x00 0x00 0x2C for nadcon5, and the reverse for
    // vertcon3, but the semantics of those 4 bytes is undocumented.
    // So try both possibilities and rely on sanity checks.
    for (int i = 0; i < 2; ++i)
    {
        const bool bBigEndian = i == 0 ? true : false;
        GetHeaderValues(poOpenInfo, dfSWLat, dfSWLon, dfDeltaLat, dfDeltaLon,
                        nRows, nCols, iKind, bBigEndian);
        if (!(fabs(dfSWLat) <= 90))
            continue;
        if (!(fabs(dfSWLon) <=
              360))  // NADCON5 grids typically have SWLon > 180
            continue;
        if (!(dfDeltaLat > 0 && dfDeltaLat <= 1))
            continue;
        if (!(dfDeltaLon > 0 && dfDeltaLon <= 1))
            continue;
        if (!(nRows > 0 && dfSWLat + (nRows - 1) * dfDeltaLat <= 90))
            continue;
        if (!(nCols > 0 && (nCols - 1) * dfDeltaLon <= 360))
            continue;
        if (!(iKind >= -1 && iKind <= 2))
            continue;

        bBigEndianOut = bBigEndian;
        return TRUE;
    }
    return FALSE;
}

int NOAA_B_Dataset::Identify(GDALOpenInfo *poOpenInfo)

{
    bool bBigEndian = false;
    return IdentifyEx(poOpenInfo, bBigEndian);
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr NOAA_B_Dataset::GetGeoTransform(double *padfTransform)

{
    memcpy(padfTransform, m_adfGeoTransform, sizeof(double) * 6);
    return CE_None;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *NOAA_B_Dataset::Open(GDALOpenInfo *poOpenInfo)

{
    bool bBigEndian = false;
    if (!IdentifyEx(poOpenInfo, bBigEndian) || poOpenInfo->fpL == nullptr ||
        poOpenInfo->eAccess == GA_Update)
    {
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Read the header.                                                */
    /* -------------------------------------------------------------------- */
    double dfSWLat;
    double dfSWLon;
    double dfDeltaLat;
    double dfDeltaLon;
    int32_t nRows;
    int32_t nCols;
    int32_t iKind;
    GetHeaderValues(poOpenInfo, dfSWLat, dfSWLon, dfDeltaLat, dfDeltaLon, nRows,
                    nCols, iKind, bBigEndian);

    if (iKind == -1)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "KIND = -1 in NOAA .b dataset not supported");
        return nullptr;
    }

    const GDALDataType eDT =
        // iKind == -1 ? GDT_Int16 :
        iKind == 0   ? GDT_Int32
        : iKind == 1 ? GDT_Float32
                     : GDT_Int16;
    const int nDTSize = GDALGetDataTypeSizeBytes(eDT);
    if (!GDALCheckDatasetDimensions(nCols, nRows) ||
        (nDTSize > 0 && static_cast<vsi_l_offset>(nCols) * nRows >
                            std::numeric_limits<vsi_l_offset>::max() / nDTSize))
    {
        return nullptr;
    }
    if (nDTSize > 0 && nCols > (std::numeric_limits<int>::max() -
                                FORTRAN_HEADER_SIZE - FORTRAN_TRAILER_SIZE) /
                                   nDTSize)
    {
        return nullptr;
    }
    const int nLineSize =
        FORTRAN_HEADER_SIZE + nCols * nDTSize + FORTRAN_TRAILER_SIZE;

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    auto poDS = std::make_unique<NOAA_B_Dataset>();

    poDS->nRasterXSize = nCols;
    poDS->nRasterYSize = nRows;

    // Adjust longitude > 180 to [-180, 180] range
    if (dfSWLon > 180)
        dfSWLon -= 360;

    // Convert from south-west center-of-pixel convention to
    // north-east pixel-corner convention
    poDS->m_adfGeoTransform[0] = dfSWLon - dfDeltaLon / 2;
    poDS->m_adfGeoTransform[1] = dfDeltaLon;
    poDS->m_adfGeoTransform[2] = 0.0;
    poDS->m_adfGeoTransform[3] =
        dfSWLat + (nRows - 1) * dfDeltaLat + dfDeltaLat / 2;
    poDS->m_adfGeoTransform[4] = 0.0;
    poDS->m_adfGeoTransform[5] = -dfDeltaLat;

    /* -------------------------------------------------------------------- */
    /*      Create band information object.                                 */
    /* -------------------------------------------------------------------- */

    // Borrow file handle
    VSILFILE *fpImage = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;

    // Records are presented from the southern-most to the northern-most
    auto poBand = RawRasterBand::Create(
        poDS.get(), 1, fpImage,
        // skip to beginning of northern-most line
        HEADER_SIZE +
            static_cast<vsi_l_offset>(poDS->nRasterYSize - 1) * nLineSize +
            FORTRAN_HEADER_SIZE,
        nDTSize, -nLineSize, eDT,
        bBigEndian ? RawRasterBand::ByteOrder::ORDER_BIG_ENDIAN
                   : RawRasterBand::ByteOrder::ORDER_LITTLE_ENDIAN,
        RawRasterBand::OwnFP::YES);
    if (!poBand)
        return nullptr;
    poDS->SetBand(1, std::move(poBand));

    /* -------------------------------------------------------------------- */
    /*      Guess CRS from filename.                                        */
    /* -------------------------------------------------------------------- */
    const std::string osFilename(CPLGetFilename(poOpenInfo->pszFilename));

    static const struct
    {
        const char *pszPrefix;
        int nEPSGCode;
    }
    // Cf https://geodesy.noaa.gov/pub/nadcon5/20160901release/Builds/
    asFilenameToCRS[] = {
        {"nadcon5.nad27.", 4267},       // NAD27
        {"nadcon5.pr40.", 4139},        // Puerto Rico (1940)
        {"nadcon5.ohd.", 4135},         // Old Hawaian
        {"nadcon5.sl1952.", 4136},      // Saint Lawrence Island (1952)
        {"nadcon5.sp1952.", 4137},      // Saint Paul Island (1952)
        {"nadcon5.sg1952.", 4138},      // Saint George Island (1952)
        {"nadcon5.as62.", 4169},        // American Samoa 1962
        {"nadcon5.gu63.", 4675},        // Guam 1963
        {"nadcon5.nad83_1986.", 4269},  // NAD83
        {"nadcon5.nad83_harn.", 4152},  // NAD83(HARN)
        {"nadcon5.nad83_1992.",
         4152},  // NAD83(1992) for Alaska is NAD83(HARN) in EPSG
        {"nadcon5.nad83_1993.",
         4152},  // NAD83(1993) for American Samoa, PRVI, Guam and Hawaii is
                 // NAD83(HARN) in EPSG
        {"nadcon5.nad83_1997.", 8545},  // NAD83(HARN Corrected)
        {"nadcon5.nad83_fbn.", 8860},   // NAD83(FBN)
        {"nadcon5.nad83_2002.",
         8860},  // NAD83(2002) for Alaska, PRVI and Guam is NAD83(FBN) in EPSG
        {"nadcon5.nad83_2007.", 4759},  // NAD83(NSRS2007)
    };

    for (const auto &sPair : asFilenameToCRS)
    {
        if (STARTS_WITH_CI(osFilename.c_str(), sPair.pszPrefix))
        {
            poDS->m_oSRS.importFromEPSG(sPair.nEPSGCode);
            break;
        }
    }
    if (poDS->m_oSRS.IsEmpty())
    {
        poDS->m_oSRS.importFromWkt(
            "GEOGCRS[\"Unspecified geographic CRS\",DATUM[\"Unspecified datum "
            "based on GRS80 ellipsoid\",ELLIPSOID[\"GRS "
            "1980\",6378137,298.257222101]],CS[ellipsoidal,2],AXIS[\"geodetic "
            "latitude (Lat)\",north,ANGLEUNIT[\"degree\",0.0174532925199433]], "
            "       AXIS[\"geodetic longitude "
            "(Lon)\",east,ORDER[2],ANGLEUNIT[\"degree\",0.0174532925199433]]]");
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
/*                        GDALRegister_NOAA_B()                         */
/************************************************************************/

void GDALRegister_NOAA_B()
{
    if (GDALGetDriverByName("NOAA_B") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("NOAA_B");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "NOAA GEOCON/NADCON5 .b format");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "b");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/noaa_b.html");

    poDriver->pfnIdentify = NOAA_B_Dataset::Identify;
    poDriver->pfnOpen = NOAA_B_Dataset::Open;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
