/******************************************************************************
 * $Id$
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

#include "rawdataset.h"
#include "cpl_string.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$");

/**

NOAA .LOS/.LAS Datum Grid Shift Format

All values are little endian

Header
------

char[56] "NADCON EXTRACTED REGION"
char[8]  "NADGRD  "
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
float32*gridwidth offset in arcseconds.

Note that the record length is always gridwidth*4 + 4, and
even the header record is this length though it means some waste.

**/

/************************************************************************/
/* ==================================================================== */
/*				LOSLASDataset				*/
/* ==================================================================== */
/************************************************************************/

class LOSLASDataset : public RawDataset
{
  public:
    VSILFILE	*fpImage;	// image data file.

    int         nRecordLength;
    
    double      adfGeoTransform[6];

  public:
    		LOSLASDataset();
    	        ~LOSLASDataset();
    
    virtual CPLErr GetGeoTransform( double * padfTransform );
    virtual const char *GetProjectionRef();

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*				LOSLASDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             LOSLASDataset()                          */
/************************************************************************/

LOSLASDataset::LOSLASDataset()
{
    fpImage = NULL;
}

/************************************************************************/
/*                            ~LOSLASDataset()                          */
/************************************************************************/

LOSLASDataset::~LOSLASDataset()

{
    FlushCache();

    if( fpImage != NULL )
        VSIFCloseL( fpImage );
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int LOSLASDataset::Identify( GDALOpenInfo *poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes < 64 )
        return FALSE;

    if( !EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"las") 
        && !EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"los") )
        return FALSE;

    if( !EQUALN((const char *)poOpenInfo->pabyHeader + 56, "NADGRD", 6 ) )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *LOSLASDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify( poOpenInfo ) )
        return NULL;
        
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    LOSLASDataset 	*poDS;

    poDS = new LOSLASDataset();

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    poDS->fpImage = VSIFOpenL( poOpenInfo->pszFilename, "rb" );

    if( poDS->fpImage == NULL )
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */
    VSIFSeekL( poDS->fpImage, 64, SEEK_SET );

    VSIFReadL( &(poDS->nRasterXSize), 4, 1, poDS->fpImage );
    VSIFReadL( &(poDS->nRasterYSize), 4, 1, poDS->fpImage );

    CPL_LSBPTR32( &(poDS->nRasterXSize) );
    CPL_LSBPTR32( &(poDS->nRasterYSize) );

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        delete poDS;
        return NULL;
    }

    VSIFSeekL( poDS->fpImage, 76, SEEK_SET );

    float min_lon, min_lat, delta_lon, delta_lat;

    VSIFReadL( &min_lon, 4, 1, poDS->fpImage );
    VSIFReadL( &delta_lon, 4, 1, poDS->fpImage );
    VSIFReadL( &min_lat, 4, 1, poDS->fpImage );
    VSIFReadL( &delta_lat, 4, 1, poDS->fpImage );

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
                              poDS->nRasterYSize * poDS->nRecordLength + 4,
                              4, -1 * poDS->nRecordLength,
                              GDT_Float32,
                              CPL_IS_LSB, TRUE, FALSE ) );

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

    return( poDS );
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

const char *LOSLASDataset::GetProjectionRef()

{
    return SRS_WKT_WGS84;
}

/************************************************************************/
/*                        GDALRegister_LOSLAS()                         */
/************************************************************************/

void GDALRegister_LOSLAS()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "LOSLAS" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "LOSLAS" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "NADCON .los/.las Datum Grid Shift" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = LOSLASDataset::Open;
        poDriver->pfnIdentify = LOSLASDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

