/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Implementation for NSIDC binary format.
 * Author:   Michael Sumner, mdsumner@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2022, Michael Sumner
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
#include "ogr_spatialref.h"
#include "rawdataset.h"

#include <cmath>
#include <algorithm>

/***********************************************************************/
/* ====================================================================*/
/*                              NSIDCbinDataset                        */
/* ====================================================================*/
/***********************************************************************/

class NSIDCbinDataset final : public GDALPamDataset
{
    friend class NSIDCbinRasterBand;

    struct NSIDCbinHeader
    {

        // page 7, User Guide https://nsidc.org/data/nsidc-0051
        // 1.3.2 File Contents
        // The file format consists of a 300-byte descriptive header followed by
        // a two-dimensional array of one-byte values containing the data. The
        // file header is composed of:
        // - a 21-element array of 6-byte character strings that contain
        // information such as polar stereographic grid characteristics
        // - a 24-byte character string containing the file name
        // - a 80-character string containing an optional image title
        // - a 70-byte character string containing ancillary information such as
        // data origin, data set creation date, etc. For compatibility with ANSI
        // C, IDL, and other languages, character strings are terminated with a
        // NULL byte.
        // Example file:
        // ftp://sidads.colorado.edu/pub/DATASETS/nsidc0051_gsfc_nasateam_seaice/final-gsfc/south/daily/2010/nt_20100918_f17_v1.1_s.bin

        char missing_int[6] = {0};       // "00255"
        char columns[6] = {0};           // "  316"
        char rows[6] = {0};              // "  332"
        char internal1[6] = {0};         // "1.799"
        char latitude[6] = {0};          // "-51.3"
        char greenwich[6] = {0};         // "270.0"
        char internal2[6] = {0};         // "558.4"
        char jpole[6] = {0};             // "158.0"
        char ipole[6] = {0};             // "174.0"
        char instrument[6] = {0};        // "SSMIS"
        char data_descriptors[6] = {0};  // "17 cn"
        char julian_start[6] = {0};      // "  261"
        char hour_start[6] = {0};        // "-9999"
        char minute_start[6] = {0};      // "-9999"
        char julian_end[6] = {0};        // "  261"
        char hour_end[6] = {0};          // "-9999"
        char minute_end[6] = {0};        // "-9999"
        char year[6] = {0};              // " 2010"
        char julian[6] = {0};            // "  261"
        char channel[6] = {0};           // "  000"
        char scaling[6] = {0};           // "00250"

        // 121-126 Integer scaling factor
        // 127-150 24-character file name (without file-name extension)
        // 151-230 80-character image title
        // 231-300 70-character data information (creation date, data source,
        // etc.)
        char filename[24] = {0};    // "  nt_20100918_f17_v1.1_s"
        char imagetitle[80] = {0};  // "ANTARCTIC  SMMR  TOTAL ICE CONCENTRATION
                                    // NIMBUSN07     DAY 299 10/26/1978"
        char data_information[70] = {0};  // "ANTARCTIC  SMMR ONSSMIGRID CON
                                          // Coast253Pole251Land254 06/27/1996"
    };

    VSILFILE *fp = nullptr;
    CPLString osSRS{};
    NSIDCbinHeader sHeader{};

    double adfGeoTransform[6];
    CPL_DISALLOW_COPY_ASSIGN(NSIDCbinDataset)
    OGRSpatialReference m_oSRS{};

  public:
    NSIDCbinDataset();
    ~NSIDCbinDataset() override;
    CPLErr GetGeoTransform(double *) override;

    const OGRSpatialReference *GetSpatialRef() const override;
    static GDALDataset *Open(GDALOpenInfo *);
    static int Identify(GDALOpenInfo *);
};

static const char *stripLeadingSpaces_nsidc(const char *buf)
{
    const char *ptr = buf;
    /* Go until we run out of characters  or hit something non-zero */
    while (*ptr == ' ')
    {
        ptr++;
    }
    return ptr;
}

/************************************************************************/
/* ==================================================================== */
/*                           NSIDCbinRasterBand                         */
/* ==================================================================== */
/************************************************************************/

class NSIDCbinRasterBand final : public RawRasterBand
{
    friend class NSIDCbinDataset;

    CPL_DISALLOW_COPY_ASSIGN(NSIDCbinRasterBand)

  public:
    NSIDCbinRasterBand(GDALDataset *poDS, int nBand, VSILFILE *fpRaw,
                       vsi_l_offset nImgOffset, int nPixelOffset,
                       int nLineOffset, GDALDataType eDataType);
    ~NSIDCbinRasterBand() override;

    double GetNoDataValue(int *pbSuccess = nullptr) override;
    double GetScale(int *pbSuccess = nullptr) override;
    const char *GetUnitType() override;
};

/************************************************************************/
/*                         NSIDCbinRasterBand()                         */
/************************************************************************/

NSIDCbinRasterBand::NSIDCbinRasterBand(GDALDataset *poDSIn, int nBandIn,
                                       VSILFILE *fpRawIn,
                                       vsi_l_offset nImgOffsetIn,
                                       int nPixelOffsetIn, int nLineOffsetIn,
                                       GDALDataType eDataTypeIn)
    : RawRasterBand(poDSIn, nBandIn, fpRawIn, nImgOffsetIn, nPixelOffsetIn,
                    nLineOffsetIn, eDataTypeIn,
                    RawRasterBand::ByteOrder::ORDER_LITTLE_ENDIAN,
                    RawRasterBand::OwnFP::NO)
{
}

/************************************************************************/
/*                           ~NSIDCbinRasterBand()                      */
/************************************************************************/

NSIDCbinRasterBand::~NSIDCbinRasterBand()
{
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double NSIDCbinRasterBand::GetNoDataValue(int *pbSuccess)

{
    if (pbSuccess != nullptr)
        *pbSuccess = TRUE;
    // we might check this if other format variants can be different
    // or if we change the Band type, or if we generalize to choosing Byte vs.
    // Float type but for now it's constant
    // https://lists.osgeo.org/pipermail/gdal-dev/2022-August/056144.html
    // const char  *pszLine = poPDS->sHeader.missing_int;
    return 255.0;  // CPLAtof(pszLine);
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double NSIDCbinRasterBand::GetScale(int *pbSuccess)
{
    if (pbSuccess != nullptr)
        *pbSuccess = TRUE;
    // again just use a constant unless we see other file variants
    // also, this might be fraction rather than percentage
    // atof(reinterpret_cast<NSIDCbinDataset*>(poDS)->sHeader.scaling)/100;
    return 0.4;
}

/************************************************************************/
/*                            NSIDCbinDataset()                         */
/************************************************************************/

NSIDCbinDataset::NSIDCbinDataset() : fp(nullptr), m_oSRS(OGRSpatialReference())
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~NSIDCbinDataset()                        */
/************************************************************************/

NSIDCbinDataset::~NSIDCbinDataset()

{
    if (fp)
        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
    fp = nullptr;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *NSIDCbinDataset::Open(GDALOpenInfo *poOpenInfo)

{

    // Confirm that the header is compatible with a NSIDC dataset.
    if (!Identify(poOpenInfo))
        return nullptr;

    // Confirm the requested access is supported.
    if (poOpenInfo->eAccess == GA_Update)
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "The NSIDCbin driver does not support update access to existing "
            "datasets.");
        return nullptr;
    }

    // Check that the file pointer from GDALOpenInfo* is available
    if (poOpenInfo->fpL == nullptr)
    {
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    auto poDS = std::make_unique<NSIDCbinDataset>();

    poDS->eAccess = poOpenInfo->eAccess;
    std::swap(poDS->fp, poOpenInfo->fpL);

    /* -------------------------------------------------------------------- */
    /*      Read the header information.                                    */
    /* -------------------------------------------------------------------- */
    if (VSIFReadL(&(poDS->sHeader), 300, 1, poDS->fp) != 1)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Attempt to read 300 byte header filed on file %s\n",
                 poOpenInfo->pszFilename);
        return nullptr;
    }

    // avoid unused warnings
    CPL_IGNORE_RET_VAL(poDS->sHeader.missing_int);
    CPL_IGNORE_RET_VAL(poDS->sHeader.internal1);
    CPL_IGNORE_RET_VAL(poDS->sHeader.latitude);
    CPL_IGNORE_RET_VAL(poDS->sHeader.greenwich);
    CPL_IGNORE_RET_VAL(poDS->sHeader.internal2);
    CPL_IGNORE_RET_VAL(poDS->sHeader.jpole);
    CPL_IGNORE_RET_VAL(poDS->sHeader.ipole);
    CPL_IGNORE_RET_VAL(poDS->sHeader.julian_start);
    CPL_IGNORE_RET_VAL(poDS->sHeader.hour_start);
    CPL_IGNORE_RET_VAL(poDS->sHeader.minute_start);
    CPL_IGNORE_RET_VAL(poDS->sHeader.julian_end);
    CPL_IGNORE_RET_VAL(poDS->sHeader.hour_end);
    CPL_IGNORE_RET_VAL(poDS->sHeader.minute_end);
    CPL_IGNORE_RET_VAL(poDS->sHeader.channel);
    CPL_IGNORE_RET_VAL(poDS->sHeader.scaling);

    /* -------------------------------------------------------------------- */
    /*      Extract information of interest from the header.                */
    /* -------------------------------------------------------------------- */

    poDS->nRasterXSize = atoi(poDS->sHeader.columns);
    poDS->nRasterYSize = atoi(poDS->sHeader.rows);

    const char *psHeader = reinterpret_cast<char *>(poOpenInfo->pabyHeader);
    bool south = STARTS_WITH(psHeader + 230, "ANTARCTIC");

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Extract metadata from the header.                               */
    /* -------------------------------------------------------------------- */

    poDS->SetMetadataItem("INSTRUMENT", poDS->sHeader.instrument);
    poDS->SetMetadataItem("YEAR", stripLeadingSpaces_nsidc(poDS->sHeader.year));
    poDS->SetMetadataItem("JULIAN_DAY",
                          stripLeadingSpaces_nsidc(poDS->sHeader.julian));
    poDS->SetMetadataItem(
        "DATA_DESCRIPTORS",
        stripLeadingSpaces_nsidc(poDS->sHeader.data_descriptors));
    poDS->SetMetadataItem("IMAGE_TITLE", poDS->sHeader.imagetitle);
    poDS->SetMetadataItem("FILENAME",
                          stripLeadingSpaces_nsidc(poDS->sHeader.filename));
    poDS->SetMetadataItem("DATA_INFORMATION", poDS->sHeader.data_information);

    /* -------------------------------------------------------------------- */
    /*      Create band information objects.                                */
    /* -------------------------------------------------------------------- */
    int nBytesPerSample = 1;

    auto poBand = std::make_unique<NSIDCbinRasterBand>(
        poDS.get(), 1, poDS->fp, 300, nBytesPerSample, poDS->nRasterXSize,
        GDT_Byte);
    if (!poBand->IsValid())
        return nullptr;
    poDS->SetBand(1, std::move(poBand));

    /* -------------------------------------------------------------------- */
    /*      Geotransform, we simply know this from the documentation.       */
    /*       If we have similar binary files (at 12.5km for example) then   */
    /*        need more nuanced handling                                    */
    /*      Projection,  this is not technically enough, because the old    */
    /*       stuff is Hughes 1980.                                          */
    /*      FIXME: old or new epsg codes based on header info, or jul/year  */
    /* -------------------------------------------------------------------- */

    int epsg = -1;
    if (south)
    {
        poDS->adfGeoTransform[0] = -3950000.0;
        poDS->adfGeoTransform[1] = 25000;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = 4350000.0;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = -25000;

        epsg = 3976;
    }
    else
    {
        poDS->adfGeoTransform[0] = -3837500;
        poDS->adfGeoTransform[1] = 25000;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = 5837500;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = -25000;

        epsg = 3413;
    }

    if (poDS->m_oSRS.importFromEPSG(epsg) != OGRERR_NONE)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unknown error initializing SRS from ESPG code. ");
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Initialize any PAM information.                                 */
    /* -------------------------------------------------------------------- */
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->TryLoadXML();

    return poDS.release();
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/
int NSIDCbinDataset::Identify(GDALOpenInfo *poOpenInfo)
{

    // -------------------------------------------------------------------- /
    //      Works for daily and monthly, north and south NSIDC binary files /
    //      north and south are different dimensions, different extents but /
    //      both are 25000m resolution.
    //
    //      First we check to see if the file has the expected header       /
    //      bytes.                                                          /
    // -------------------------------------------------------------------- /

    if (poOpenInfo->nHeaderBytes < 300 || poOpenInfo->fpL == nullptr)
        return FALSE;

    const char *psHeader = reinterpret_cast<char *>(poOpenInfo->pabyHeader);
    // Check if century values seem reasonable.
    if (!(EQUALN(psHeader + 103, "20", 2) || EQUALN(psHeader + 103, "19", 2) ||
          // the first files 1978 don't have a space at the start
          EQUALN(psHeader + 102, "20", 2) || EQUALN(psHeader + 102, "19", 2)))
    {
        return FALSE;
    }

    // Check if descriptors reasonable.
    if (!(STARTS_WITH(psHeader + 230, "ANTARCTIC") ||
          STARTS_WITH(psHeader + 230, "ARCTIC")))
    {

        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

const OGRSpatialReference *NSIDCbinDataset::GetSpatialRef() const
{
    return &m_oSRS;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr NSIDCbinDataset::GetGeoTransform(double *padfTransform)

{
    memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6);

    return CE_None;
}

/************************************************************************/
/*                             GetUnitType()                            */
/************************************************************************/

const char *NSIDCbinRasterBand::GetUnitType()
{
    // undecided, atm stick with Byte but may switch to Float and lose values >
    // 250 or generalize to non-raw driver
    // https://lists.osgeo.org/pipermail/gdal-dev/2022-August/056144.html
    // if (eDataType == GDT_Float32)
    //     return "Percentage";  // or "Fraction [0,1]"

    // Byte values don't have a clear unit type
    return "";
}

/************************************************************************/
/*                          GDALRegister_NSIDCbin()                        */
/************************************************************************/

void GDALRegister_NSIDCbin()

{
    if (GDALGetDriverByName("NSIDCbin") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("NSIDCbin");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "NSIDC Sea Ice Concentrations binary (.bin)");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/raster/nsidcbin.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "bin");

    poDriver->pfnOpen = NSIDCbinDataset::Open;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
