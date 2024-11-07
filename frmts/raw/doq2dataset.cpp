/******************************************************************************
 *
 * Project:  USGS DOQ Driver (Second Generation Format)
 * Purpose:  Implementation of DOQ2Dataset
 * Author:   Derrick J Brashear, shadow@dementia.org
 *
 ******************************************************************************
 * Copyright (c) 2000, Derrick J Brashear
 * Copyright (c) 2009-2011, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "rawdataset.h"

#ifndef UTM_FORMAT_defined
#define UTM_FORMAT_defined

static const char UTM_FORMAT[] =
    "PROJCS[\"%s / UTM zone %dN\",GEOGCS[%s,PRIMEM[\"Greenwich\",0],"
    "UNIT[\"degree\",0.0174532925199433]],PROJECTION[\"Transverse_Mercator\"],"
    "PARAMETER[\"latitude_of_origin\",0],PARAMETER[\"central_meridian\",%d],"
    "PARAMETER[\"scale_factor\",0.9996],PARAMETER[\"false_easting\",500000],"
    "PARAMETER[\"false_northing\",0],%s]";

static const char WGS84_DATUM[] =
    "\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563]]";

static const char WGS72_DATUM[] =
    "\"WGS 72\",DATUM[\"WGS_1972\",SPHEROID[\"NWL 10D\",6378135,298.26]]";

static const char NAD27_DATUM[] =
    "\"NAD27\",DATUM[\"North_American_Datum_1927\","
    "SPHEROID[\"Clarke 1866\",6378206.4,294.978698213901]]";

static const char NAD83_DATUM[] =
    "\"NAD83\",DATUM[\"North_American_Datum_1983\","
    "SPHEROID[\"GRS 1980\",6378137,298.257222101]]";

#endif

/************************************************************************/
/* ==================================================================== */
/*                              DOQ2Dataset                             */
/* ==================================================================== */
/************************************************************************/

class DOQ2Dataset final : public RawDataset
{
    VSILFILE *fpImage = nullptr;  // Image data file.

    double dfULX = 0;
    double dfULY = 0;
    double dfXPixelSize = 0;
    double dfYPixelSize = 0;

    OGRSpatialReference m_oSRS{};

    CPL_DISALLOW_COPY_ASSIGN(DOQ2Dataset)

    CPLErr Close() override;

  public:
    DOQ2Dataset();
    ~DOQ2Dataset();

    CPLErr GetGeoTransform(double *padfTransform) override;

    const OGRSpatialReference *GetSpatialRef() const override
    {
        return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
    }

    static GDALDataset *Open(GDALOpenInfo *);
};

/************************************************************************/
/*                            DOQ2Dataset()                             */
/************************************************************************/

DOQ2Dataset::DOQ2Dataset()
{
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
}

/************************************************************************/
/*                            ~DOQ2Dataset()                            */
/************************************************************************/

DOQ2Dataset::~DOQ2Dataset()

{
    DOQ2Dataset::Close();
}

/************************************************************************/
/*                              Close()                                 */
/************************************************************************/

CPLErr DOQ2Dataset::Close()
{
    CPLErr eErr = CE_None;
    if (nOpenFlags != OPEN_FLAGS_CLOSED)
    {
        if (DOQ2Dataset::FlushCache(true) != CE_None)
            eErr = CE_Failure;

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
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr DOQ2Dataset::GetGeoTransform(double *padfTransform)

{
    padfTransform[0] = dfULX;
    padfTransform[1] = dfXPixelSize;
    padfTransform[2] = 0.0;
    padfTransform[3] = dfULY;
    padfTransform[4] = 0.0;
    padfTransform[5] = -1 * dfYPixelSize;

    return CE_None;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *DOQ2Dataset::Open(GDALOpenInfo *poOpenInfo)

{
    /* -------------------------------------------------------------------- */
    /*      We assume the user is pointing to the binary (i.e. .bil) file.  */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->nHeaderBytes < 212 || poOpenInfo->fpL == nullptr)
        return nullptr;

    if (!STARTS_WITH_CI(reinterpret_cast<char *>(poOpenInfo->pabyHeader),
                        "BEGIN_USGS_DOQ_HEADER"))
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Confirm the requested access is supported.                      */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->eAccess == GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The DOQ2 driver does not support update access to existing "
                 "datasets.");
        return nullptr;
    }

    int nBytesPerPixel = 0;
    int nWidth = 0;
    int nHeight = 0;
    int nBandStorage = 0;
    int nBandTypes = 0;
    const char *pszDatumLong = nullptr;
    const char *pszDatumShort = nullptr;
    const char *pszUnits = nullptr;
    int nZone = 0;
    int nProjType = 0;
    int nSkipBytes = 0;
    int nBandCount = 0;
    double dfULXMap = 0.0;
    double dfULYMap = 0.0;
    double dfXDim = 0.0;
    double dfYDim = 0.0;
    char **papszMetadata = nullptr;

    /* read and discard the first line */
    CPL_IGNORE_RET_VAL(CPLReadLineL(poOpenInfo->fpL));

    const char *pszLine = nullptr;
    while ((pszLine = CPLReadLineL(poOpenInfo->fpL)) != nullptr)
    {
        if (EQUAL(pszLine, "END_USGS_DOQ_HEADER"))
            break;

        char **papszTokens = CSLTokenizeString(pszLine);
        if (CSLCount(papszTokens) < 2)
        {
            CSLDestroy(papszTokens);
            break;
        }

        if (EQUAL(papszTokens[0], "SAMPLES_AND_LINES") &&
            CSLCount(papszTokens) >= 3)
        {
            nWidth = atoi(papszTokens[1]);
            nHeight = atoi(papszTokens[2]);
        }
        else if (EQUAL(papszTokens[0], "BYTE_COUNT"))
        {
            nSkipBytes = atoi(papszTokens[1]);
        }
        else if (EQUAL(papszTokens[0], "XY_ORIGIN") &&
                 CSLCount(papszTokens) >= 3)
        {
            dfULXMap = CPLAtof(papszTokens[1]);
            dfULYMap = CPLAtof(papszTokens[2]);
        }
        else if (EQUAL(papszTokens[0], "HORIZONTAL_RESOLUTION"))
        {
            dfXDim = CPLAtof(papszTokens[1]);
            dfYDim = dfXDim;
        }
        else if (EQUAL(papszTokens[0], "BAND_ORGANIZATION"))
        {
            if (EQUAL(papszTokens[1], "SINGLE FILE"))
                nBandStorage = 1;
            if (EQUAL(papszTokens[1], "BSQ"))
                nBandStorage = 1;
            if (EQUAL(papszTokens[1], "BIL"))
                nBandStorage = 1;
            if (EQUAL(papszTokens[1], "BIP"))
                nBandStorage = 4;
        }
        else if (EQUAL(papszTokens[0], "BAND_CONTENT"))
        {
            if (EQUAL(papszTokens[1], "BLACK&WHITE"))
                nBandTypes = 1;
            else if (EQUAL(papszTokens[1], "COLOR"))
                nBandTypes = 5;
            else if (EQUAL(papszTokens[1], "RGB"))
                nBandTypes = 5;
            else if (EQUAL(papszTokens[1], "RED"))
                nBandTypes = 5;
            else if (EQUAL(papszTokens[1], "GREEN"))
                nBandTypes = 5;
            else if (EQUAL(papszTokens[1], "BLUE"))
                nBandTypes = 5;

            nBandCount++;
        }
        else if (EQUAL(papszTokens[0], "BITS_PER_PIXEL"))
        {
            nBytesPerPixel = atoi(papszTokens[1]) / 8;
        }
        else if (EQUAL(papszTokens[0], "HORIZONTAL_COORDINATE_SYSTEM"))
        {
            if (EQUAL(papszTokens[1], "UTM"))
                nProjType = 1;
            else if (EQUAL(papszTokens[1], "SPCS"))
                nProjType = 2;
            else if (EQUAL(papszTokens[1], "GEOGRAPHIC"))
                nProjType = 0;
        }
        else if (EQUAL(papszTokens[0], "COORDINATE_ZONE"))
        {
            nZone = atoi(papszTokens[1]);
        }
        else if (EQUAL(papszTokens[0], "HORIZONTAL_UNITS"))
        {
            if (EQUAL(papszTokens[1], "METERS"))
                pszUnits = "UNIT[\"metre\",1]";
            else if (EQUAL(papszTokens[1], "FEET"))
                pszUnits = "UNIT[\"US survey foot\",0.304800609601219]";
        }
        else if (EQUAL(papszTokens[0], "HORIZONTAL_DATUM"))
        {
            if (EQUAL(papszTokens[1], "NAD27"))
            {
                pszDatumLong = NAD27_DATUM;
                pszDatumShort = "NAD 27";
            }
            else if (EQUAL(papszTokens[1], " WGS72"))
            {
                pszDatumLong = WGS72_DATUM;
                pszDatumShort = "WGS 72";
            }
            else if (EQUAL(papszTokens[1], "WGS84"))
            {
                pszDatumLong = WGS84_DATUM;
                pszDatumShort = "WGS 84";
            }
            else if (EQUAL(papszTokens[1], "NAD83"))
            {
                pszDatumLong = NAD83_DATUM;
                pszDatumShort = "NAD 83";
            }
            else
            {
                pszDatumLong = "DATUM[\"unknown\"]";
                pszDatumShort = "unknown";
            }
        }
        else
        {
            /* we want to generically capture all the other metadata */
            CPLString osMetaDataValue;

            for (int iToken = 1; papszTokens[iToken] != nullptr; iToken++)
            {
                if (EQUAL(papszTokens[iToken], "*"))
                    continue;

                if (iToken > 1)
                    osMetaDataValue += " ";
                osMetaDataValue += papszTokens[iToken];
            }
            papszMetadata =
                CSLAddNameValue(papszMetadata, papszTokens[0], osMetaDataValue);
        }

        CSLDestroy(papszTokens);
    }

    CPLReadLineL(nullptr);

    /* -------------------------------------------------------------------- */
    /*      Do these values look coherent for a DOQ file?  It would be      */
    /*      nice to do a more comprehensive test than this!                 */
    /* -------------------------------------------------------------------- */
    if (nWidth < 500 || nWidth > 25000 || nHeight < 500 || nHeight > 25000 ||
        nBandStorage < 0 || nBandStorage > 4 || nBandTypes < 1 ||
        nBandTypes > 9 || nBytesPerPixel < 0)
    {
        CSLDestroy(papszMetadata);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Check the configuration.  We don't currently handle all         */
    /*      variations, only the common ones.                               */
    /* -------------------------------------------------------------------- */
    if (nBandTypes > 5)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "DOQ Data Type (%d) is not a supported configuration.",
                 nBandTypes);
        CSLDestroy(papszMetadata);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Confirm the requested access is supported.                      */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->eAccess == GA_Update)
    {
        CSLDestroy(papszMetadata);
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The DOQ2 driver does not support update access to existing"
                 " datasets.");
        return nullptr;
    }
    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    auto poDS = std::make_unique<DOQ2Dataset>();

    poDS->nRasterXSize = nWidth;
    poDS->nRasterYSize = nHeight;

    poDS->SetMetadata(papszMetadata);
    CSLDestroy(papszMetadata);
    papszMetadata = nullptr;

    poDS->fpImage = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;

    /* -------------------------------------------------------------------- */
    /*      Compute layout of data.                                         */
    /* -------------------------------------------------------------------- */
    if (nBandCount < 2)
    {
        nBandCount = nBytesPerPixel;
        if (!GDALCheckBandCount(nBandCount, FALSE))
        {
            return nullptr;
        }
    }
    else
    {
        if (nBytesPerPixel > INT_MAX / nBandCount)
        {
            return nullptr;
        }
        nBytesPerPixel *= nBandCount;
    }

    if (nBytesPerPixel > INT_MAX / nWidth)
    {
        return nullptr;
    }
    const int nBytesPerLine = nBytesPerPixel * nWidth;

    /* -------------------------------------------------------------------- */
    /*      Create band information objects.                                */
    /* -------------------------------------------------------------------- */
    for (int i = 0; i < nBandCount; i++)
    {
        auto poBand = RawRasterBand::Create(
            poDS.get(), i + 1, poDS->fpImage, nSkipBytes + i, nBytesPerPixel,
            nBytesPerLine, GDT_Byte,
            RawRasterBand::ByteOrder::ORDER_LITTLE_ENDIAN,
            RawRasterBand::OwnFP::NO);
        if (!poBand)
            return nullptr;
        poDS->SetBand(i + 1, std::move(poBand));
    }

    if (nProjType == 1)
    {
        poDS->m_oSRS.importFromWkt(
            CPLSPrintf(UTM_FORMAT, pszDatumShort ? pszDatumShort : "", nZone,
                       pszDatumLong ? pszDatumLong : "",
                       nZone >= 1 && nZone <= 60 ? nZone * 6 - 183 : 0,
                       pszUnits ? pszUnits : ""));
    }

    poDS->dfULX = dfULXMap;
    poDS->dfULY = dfULYMap;

    poDS->dfXPixelSize = dfXDim;
    poDS->dfYPixelSize = dfYDim;

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
/*                         GDALRegister_DOQ1()                          */
/************************************************************************/

void GDALRegister_DOQ2()

{
    if (GDALGetDriverByName("DOQ2") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("DOQ2");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "USGS DOQ (New Style)");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/doq2.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = DOQ2Dataset::Open;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
