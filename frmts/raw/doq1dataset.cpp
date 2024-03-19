/******************************************************************************
 *
 * Project:  USGS DOQ Driver (First Generation Format)
 * Purpose:  Implementation of DOQ1Dataset
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2009-2011, Even Rouault <even dot rouault at spatialys.com>
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

#include "gdal_frmts.h"
#include "cpl_string.h"
#include "rawdataset.h"

#include <algorithm>

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
/*                            DOQGetField()                             */
/************************************************************************/

static double DOQGetField(unsigned char *pabyData, int nBytes)

{
    char szWork[128] = {'\0'};

    memcpy(szWork, reinterpret_cast<const char *>(pabyData), nBytes);
    szWork[nBytes] = '\0';

    for (int i = 0; i < nBytes; i++)
    {
        if (szWork[i] == 'D' || szWork[i] == 'd')
            szWork[i] = 'E';
    }

    return CPLAtof(szWork);
}

/************************************************************************/
/*                         DOQGetDescription()                          */
/************************************************************************/

static void DOQGetDescription(GDALDataset *poDS, unsigned char *pabyData)

{
    char szWork[128] = {' '};

    const char *pszDescBegin = "USGS GeoTIFF DOQ 1:12000 Q-Quad of ";
    memcpy(szWork, pszDescBegin, strlen(pszDescBegin));
    memcpy(szWork + strlen(pszDescBegin),
           reinterpret_cast<const char *>(pabyData + 0), 38);

    int i = 0;
    while (*(szWork + 72 - i) == ' ')
    {
        i++;
    }
    i--;

    memcpy(szWork + 73 - i, reinterpret_cast<const char *>(pabyData + 38), 2);
    memcpy(szWork + 76 - i, reinterpret_cast<const char *>(pabyData + 44), 2);
    szWork[77 - i] = '\0';

    poDS->SetMetadataItem("DOQ_DESC", szWork);
}

/************************************************************************/
/* ==================================================================== */
/*                              DOQ1Dataset                             */
/* ==================================================================== */
/************************************************************************/

class DOQ1Dataset final : public RawDataset
{
    VSILFILE *fpImage = nullptr;  // Image data file.

    double dfULX = 0;
    double dfULY = 0;
    double dfXPixelSize = 0;
    double dfYPixelSize = 0;

    OGRSpatialReference m_oSRS{};

    CPL_DISALLOW_COPY_ASSIGN(DOQ1Dataset)

    CPLErr Close() override;

  public:
    DOQ1Dataset();
    ~DOQ1Dataset();

    CPLErr GetGeoTransform(double *padfTransform) override;

    const OGRSpatialReference *GetSpatialRef() const override
    {
        return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
    }

    static GDALDataset *Open(GDALOpenInfo *);
};

/************************************************************************/
/*                            DOQ1Dataset()                             */
/************************************************************************/

DOQ1Dataset::DOQ1Dataset()
{
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
}

/************************************************************************/
/*                            ~DOQ1Dataset()                            */
/************************************************************************/

DOQ1Dataset::~DOQ1Dataset()

{
    DOQ1Dataset::Close();
}

/************************************************************************/
/*                              Close()                                 */
/************************************************************************/

CPLErr DOQ1Dataset::Close()
{
    CPLErr eErr = CE_None;
    if (nOpenFlags != OPEN_FLAGS_CLOSED)
    {
        if (DOQ1Dataset::FlushCache(true) != CE_None)
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

CPLErr DOQ1Dataset::GetGeoTransform(double *padfTransform)

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

GDALDataset *DOQ1Dataset::Open(GDALOpenInfo *poOpenInfo)

{
    /* -------------------------------------------------------------------- */
    /*      We assume the user is pointing to the binary (i.e. .bil) file.  */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->nHeaderBytes < 212 || poOpenInfo->fpL == nullptr)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Attempt to extract a few key values from the header.            */
    /* -------------------------------------------------------------------- */
    const double dfWidth = DOQGetField(poOpenInfo->pabyHeader + 150, 6);
    const double dfHeight = DOQGetField(poOpenInfo->pabyHeader + 144, 6);
    const double dfBandStorage = DOQGetField(poOpenInfo->pabyHeader + 162, 3);
    const double dfBandTypes = DOQGetField(poOpenInfo->pabyHeader + 156, 3);

    /* -------------------------------------------------------------------- */
    /*      Do these values look coherent for a DOQ file?  It would be      */
    /*      nice to do a more comprehensive test than this!                 */
    /* -------------------------------------------------------------------- */
    if (dfWidth < 500 || dfWidth > 25000 || CPLIsNan(dfWidth) ||
        dfHeight < 500 || dfHeight > 25000 || CPLIsNan(dfHeight) ||
        dfBandStorage < 0 || dfBandStorage > 4 || CPLIsNan(dfBandStorage) ||
        dfBandTypes < 1 || dfBandTypes > 9 || CPLIsNan(dfBandTypes))
        return nullptr;

    const int nWidth = static_cast<int>(dfWidth);
    const int nHeight = static_cast<int>(dfHeight);
    /*const int nBandStorage = static_cast<int>(dfBandStorage);*/
    const int nBandTypes = static_cast<int>(dfBandTypes);

    /* -------------------------------------------------------------------- */
    /*      Check the configuration.  We don't currently handle all         */
    /*      variations, only the common ones.                               */
    /* -------------------------------------------------------------------- */
    if (nBandTypes > 5)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "DOQ Data Type (%d) is not a supported configuration.",
                 nBandTypes);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Confirm the requested access is supported.                      */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->eAccess == GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The DOQ1 driver does not support update access to existing "
                 "datasets.");
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    auto poDS = std::make_unique<DOQ1Dataset>();

    /* -------------------------------------------------------------------- */
    /*      Capture some information from the file that is of interest.     */
    /* -------------------------------------------------------------------- */
    poDS->nRasterXSize = nWidth;
    poDS->nRasterYSize = nHeight;

    std::swap(poDS->fpImage, poOpenInfo->fpL);

    /* -------------------------------------------------------------------- */
    /*      Compute layout of data.                                         */
    /* -------------------------------------------------------------------- */
    int nBytesPerPixel = 0;

    if (nBandTypes < 5)
        nBytesPerPixel = 1;
    else /* if( nBandTypes == 5 ) */
        nBytesPerPixel = 3;

    const int nBytesPerLine = nBytesPerPixel * nWidth;
    const int nSkipBytes = 4 * nBytesPerLine;

    /* -------------------------------------------------------------------- */
    /*      Create band information objects.                                */
    /* -------------------------------------------------------------------- */
    for (int i = 0; i < nBytesPerPixel; i++)
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

    /* -------------------------------------------------------------------- */
    /*      Set the description.                                            */
    /* -------------------------------------------------------------------- */
    DOQGetDescription(poDS.get(), poOpenInfo->pabyHeader);

    /* -------------------------------------------------------------------- */
    /*      Establish the projection string.                                */
    /* -------------------------------------------------------------------- */
    if (static_cast<int>(DOQGetField(poOpenInfo->pabyHeader + 195, 3)) == 1)
    {
        int nZone =
            static_cast<int>(DOQGetField(poOpenInfo->pabyHeader + 198, 6));
        if (nZone < 0 || nZone > 60)
            nZone = 0;

        const char *pszUnits = nullptr;
        if (static_cast<int>(DOQGetField(poOpenInfo->pabyHeader + 204, 3)) == 1)
            pszUnits = "UNIT[\"US survey foot\",0.304800609601219]";
        else
            pszUnits = "UNIT[\"metre\",1]";

        const char *pszDatumLong = nullptr;
        const char *pszDatumShort = nullptr;
        switch (static_cast<int>(DOQGetField(poOpenInfo->pabyHeader + 167, 2)))
        {
            case 1:
                pszDatumLong = NAD27_DATUM;
                pszDatumShort = "NAD 27";
                break;

            case 2:
                pszDatumLong = WGS72_DATUM;
                pszDatumShort = "WGS 72";
                break;

            case 3:
                pszDatumLong = WGS84_DATUM;
                pszDatumShort = "WGS 84";
                break;

            case 4:
                pszDatumLong = NAD83_DATUM;
                pszDatumShort = "NAD 83";
                break;

            default:
                pszDatumLong = "DATUM[\"unknown\"]";
                pszDatumShort = "unknown";
                break;
        }

        poDS->m_oSRS.importFromWkt(CPLSPrintf(UTM_FORMAT, pszDatumShort, nZone,
                                              pszDatumLong, nZone * 6 - 183,
                                              pszUnits));
    }

    /* -------------------------------------------------------------------- */
    /*      Read the georeferencing information.                            */
    /* -------------------------------------------------------------------- */
    unsigned char abyRecordData[500] = {'\0'};

    if (VSIFSeekL(poDS->fpImage, nBytesPerLine * 2, SEEK_SET) != 0 ||
        VSIFReadL(abyRecordData, sizeof(abyRecordData), 1, poDS->fpImage) != 1)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Header read error on %s.",
                 poOpenInfo->pszFilename);
        return nullptr;
    }

    poDS->dfULX = DOQGetField(abyRecordData + 288, 24);
    poDS->dfULY = DOQGetField(abyRecordData + 312, 24);

    if (VSIFSeekL(poDS->fpImage, nBytesPerLine * 3, SEEK_SET) != 0 ||
        VSIFReadL(abyRecordData, sizeof(abyRecordData), 1, poDS->fpImage) != 1)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Header read error on %s.",
                 poOpenInfo->pszFilename);
        return nullptr;
    }

    poDS->dfXPixelSize = DOQGetField(abyRecordData + 59, 12);
    poDS->dfYPixelSize = DOQGetField(abyRecordData + 71, 12);

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

void GDALRegister_DOQ1()

{
    if (GDALGetDriverByName("DOQ1") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("DOQ1");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "USGS DOQ (Old Style)");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/doq1.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = DOQ1Dataset::Open;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
