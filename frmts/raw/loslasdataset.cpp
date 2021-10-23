/******************************************************************************
 *
 * Project:  Horizontal Datum Formats
 * Purpose:  Implementation of NOAA/NADCON los/las datum shift format.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 * Financial Support: i-cubed (http://www.i-cubed.com)
 *
 ******************************************************************************
 * Copyright (c) 2010, Frank Warmerdam
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
#include "ogr_srs_api.h"
#include "rawdataset.h"

CPL_CVSID("$Id$")

/**

NOAA .LOS/.LAS Datum Grid Shift Format

Also used for .geo file from https://geodesy.noaa.gov/GEOID/MEXICO97/

All values are little endian

Header
------

char[56] "NADCON EXTRACTED REGION" or
         "GEOID EXTRACTED REGION "
char[8]  "NADGRD  " or
         "GEOGRD  "
int32    grid width
int32    grid height
int32    z count (1)
float32  origin longitude
float32  grid cell width longitude
float32  origin latitude
float32  grid cell height latitude
float32  angle (0.0)

Data
----

int32   ? always 0
float32*gridwidth offset in arcseconds (or in metres for geoids)

Note that the record length is always gridwidth*4 + 4, and
even the header record is this length though it means some waste.

**/

/************************************************************************/
/* ==================================================================== */
/*                              LOSLASDataset                           */
/* ==================================================================== */
/************************************************************************/

class LOSLASDataset final: public RawDataset
{
    VSILFILE    *fpImage;  // image data file.

    int         nRecordLength;

    double      adfGeoTransform[6];

    CPL_DISALLOW_COPY_ASSIGN(LOSLASDataset)

  public:
    LOSLASDataset();
    ~LOSLASDataset() override;

    CPLErr GetGeoTransform( double * padfTransform ) override;
    const char *_GetProjectionRef() override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                              LOSLASDataset                           */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             LOSLASDataset()                          */
/************************************************************************/

LOSLASDataset::LOSLASDataset() : fpImage(nullptr), nRecordLength(0)
{
    memset( adfGeoTransform, 0, sizeof(adfGeoTransform) );
}

/************************************************************************/
/*                            ~LOSLASDataset()                          */
/************************************************************************/

LOSLASDataset::~LOSLASDataset()

{
    FlushCache(true);

    if( fpImage != nullptr )
        CPL_IGNORE_RET_VAL(VSIFCloseL( fpImage ));
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int LOSLASDataset::Identify( GDALOpenInfo *poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes < 64 )
        return FALSE;

#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    const char* pszExt = CPLGetExtension(poOpenInfo->pszFilename);
    if( !EQUAL(pszExt,"las") && !EQUAL(pszExt,"los") && !EQUAL(pszExt,"geo") )
        return FALSE;
#endif

    if( !STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader + 56, "NADGRD") &&
        !STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader + 56, "GEOGRD") )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *LOSLASDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify( poOpenInfo ) || poOpenInfo->fpL == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The LOSLAS driver does not support update access to existing"
                  " datasets." );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    LOSLASDataset *poDS = new LOSLASDataset();
    poDS->fpImage = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;

/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */
    CPL_IGNORE_RET_VAL(VSIFSeekL( poDS->fpImage, 64, SEEK_SET ));

    CPL_IGNORE_RET_VAL(VSIFReadL( &(poDS->nRasterXSize), 4, 1, poDS->fpImage ));
    CPL_IGNORE_RET_VAL(VSIFReadL( &(poDS->nRasterYSize), 4, 1, poDS->fpImage ));

    CPL_LSBPTR32( &(poDS->nRasterXSize) );
    CPL_LSBPTR32( &(poDS->nRasterYSize) );

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize) ||
        poDS->nRasterXSize > (INT_MAX - 4) / 4 )
    {
        delete poDS;
        return nullptr;
    }

    CPL_IGNORE_RET_VAL(VSIFSeekL( poDS->fpImage, 76, SEEK_SET ));

    float min_lon, min_lat, delta_lon, delta_lat;

    CPL_IGNORE_RET_VAL(VSIFReadL( &min_lon, 4, 1, poDS->fpImage ));
    CPL_IGNORE_RET_VAL(VSIFReadL( &delta_lon, 4, 1, poDS->fpImage ));
    CPL_IGNORE_RET_VAL(VSIFReadL( &min_lat, 4, 1, poDS->fpImage ));
    CPL_IGNORE_RET_VAL(VSIFReadL( &delta_lat, 4, 1, poDS->fpImage ));

    CPL_LSBPTR32( &min_lon );
    CPL_LSBPTR32( &delta_lon );
    CPL_LSBPTR32( &min_lat );
    CPL_LSBPTR32( &delta_lat );

    poDS->nRecordLength = poDS->nRasterXSize * 4 + 4;

/* -------------------------------------------------------------------- */
/*      Create band information object.                                 */
/*                                                                      */
/*      Note we are setting up to read from the last image record to    */
/*      the first since the data comes with the southern most record    */
/*      first, not the northernmost like we would want.                 */
/* -------------------------------------------------------------------- */
    poDS->SetBand(
        1, new RawRasterBand( poDS, 1, poDS->fpImage,
                              static_cast<vsi_l_offset>(poDS->nRasterYSize) *
                                    poDS->nRecordLength + 4,
                              4, -1 * poDS->nRecordLength,
                              GDT_Float32,
                              CPL_IS_LSB,
                              RawRasterBand::OwnFP::NO ) );

    if( EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"las") )
    {
        poDS->GetRasterBand(1)->SetDescription( "Latitude Offset (arc seconds)" );
    }
    else if( EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"los") )
    {
        poDS->GetRasterBand(1)->SetDescription( "Longitude Offset (arc seconds)" );
        poDS->GetRasterBand(1)->SetMetadataItem("positive_value", "west");
    }
    else if( EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"geo") )
    {
        poDS->GetRasterBand(1)->SetDescription( "Geoid undulation (meters)" );
    }

/* -------------------------------------------------------------------- */
/*      Setup georeferencing.                                           */
/* -------------------------------------------------------------------- */
    poDS->adfGeoTransform[0] = min_lon - delta_lon*0.5;
    poDS->adfGeoTransform[1] = delta_lon;
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[3] = min_lat + (poDS->nRasterYSize-0.5) * delta_lat;
    poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[5] = -1 * delta_lat;

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return poDS;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr LOSLASDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );
    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *LOSLASDataset::_GetProjectionRef()

{
    return SRS_WKT_WGS84_LAT_LONG;
}

/************************************************************************/
/*                        GDALRegister_LOSLAS()                         */
/************************************************************************/

void GDALRegister_LOSLAS()

{
    if( GDALGetDriverByName( "LOSLAS" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "LOSLAS" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "NADCON .los/.las Datum Grid Shift" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = LOSLASDataset::Open;
    poDriver->pfnIdentify = LOSLASDataset::Identify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
