/******************************************************************************
 * $Id$
 *
 * Project:  Vertical Datum Transformation
 * Purpose:  Implementation of NOAA .gtx vertical datum shift file format.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Frank Warmerdam
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

NOAA .GTX Vertical Datum Grid Shift Format

All values are bigendian

Header
------

float64  latitude_of_origin
float64  longitude_of_origin (0-360)
float64  cell size (x?y?)
float64  cell size (x?y?)
int32    length in pixels
int32    width in pixels

Data
----

float32  * width in pixels * length in pixels 

Values are an offset in meters between two vertical datums.

**/

/************************************************************************/
/* ==================================================================== */
/*				GTXDataset				*/
/* ==================================================================== */
/************************************************************************/

class GTXDataset : public RawDataset
{
  public:
    VSILFILE	*fpImage;	// image data file.
    
    double      adfGeoTransform[6];

  public:
    		GTXDataset();
    	        ~GTXDataset();
    
    virtual CPLErr GetGeoTransform( double * padfTransform );
    virtual CPLErr SetGeoTransform( double * padfTransform );
    virtual const char *GetProjectionRef();

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszOptions );
};

/************************************************************************/
/* ==================================================================== */
/*				GTXDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             GTXDataset()                             */
/************************************************************************/

GTXDataset::GTXDataset()
{
    fpImage = NULL;
}

/************************************************************************/
/*                            ~GTXDataset()                             */
/************************************************************************/

GTXDataset::~GTXDataset()

{
    FlushCache();

    if( fpImage != NULL )
        VSIFCloseL( fpImage );
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int GTXDataset::Identify( GDALOpenInfo *poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes < 40 )
        return FALSE;

    if( !EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"gtx") )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GTXDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify( poOpenInfo ) )
        return NULL;
        
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GTXDataset 	*poDS;

    poDS = new GTXDataset();

    poDS->eAccess = poOpenInfo->eAccess;

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_ReadOnly )
        poDS->fpImage = VSIFOpenL( poOpenInfo->pszFilename, "rb" );
    else
        poDS->fpImage = VSIFOpenL( poOpenInfo->pszFilename, "rb+" );

    if( poDS->fpImage == NULL )
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[4] = 0.0;

    VSIFReadL( poDS->adfGeoTransform+3, 8, 1, poDS->fpImage );
    VSIFReadL( poDS->adfGeoTransform+0, 8, 1, poDS->fpImage );
    VSIFReadL( poDS->adfGeoTransform+5, 8, 1, poDS->fpImage );
    VSIFReadL( poDS->adfGeoTransform+1, 8, 1, poDS->fpImage );

    VSIFReadL( &(poDS->nRasterYSize), 4, 1, poDS->fpImage );
    VSIFReadL( &(poDS->nRasterXSize), 4, 1, poDS->fpImage );

    CPL_MSBPTR32( &(poDS->nRasterYSize) );
    CPL_MSBPTR32( &(poDS->nRasterXSize) );

    CPL_MSBPTR64( poDS->adfGeoTransform + 0 );
    CPL_MSBPTR64( poDS->adfGeoTransform + 1 );
    CPL_MSBPTR64( poDS->adfGeoTransform + 3 );
    CPL_MSBPTR64( poDS->adfGeoTransform + 5 );

    poDS->adfGeoTransform[3] += 
        poDS->adfGeoTransform[5] * (poDS->nRasterYSize-1);

    poDS->adfGeoTransform[0] -= poDS->adfGeoTransform[1] * 0.5;
    poDS->adfGeoTransform[3] += poDS->adfGeoTransform[5] * 0.5;

    poDS->adfGeoTransform[5] *= -1;

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Guess the data type. Since October 1, 2009, it should be        */
/*      Float32. Before it was double.                                  */
/* -------------------------------------------------------------------- */
    GDALDataType eDT = GDT_Float32;
    VSIFSeekL(poDS->fpImage, 0, SEEK_END);
    vsi_l_offset nSize = VSIFTellL(poDS->fpImage);
    if( nSize == 40 + 8 * (vsi_l_offset)poDS->nRasterXSize * poDS->nRasterYSize )
        eDT = GDT_Float64;
    int nDTSize = GDALGetDataTypeSize(eDT) / 8;

/* -------------------------------------------------------------------- */
/*      Create band information object.                                 */
/* -------------------------------------------------------------------- */
    RawRasterBand *poBand = new RawRasterBand( poDS, 1, poDS->fpImage, 
                              (poDS->nRasterYSize-1)*poDS->nRasterXSize*nDTSize + 40,
                              nDTSize, poDS->nRasterXSize * -nDTSize,
                              eDT,
                              !CPL_IS_LSB, TRUE, FALSE );
    if (eDT == GDT_Float64)
      poBand->SetNoDataValue( -88.8888 );
    else
      /* GDT_Float32 */
      poBand->SetNoDataValue( (double)-88.8888f );
    poDS->SetBand( 1, poBand );

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

CPLErr GTXDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );
    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr GTXDataset::SetGeoTransform( double * padfTransform )

{
    if( padfTransform[2] != 0.0 || padfTransform[4] != 0.0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to write skewed or rotated geotransform to gtx." );
        return CE_Failure;
    }

    memcpy( adfGeoTransform, padfTransform, sizeof(double)*6 );

    unsigned char header[32];
    double dfXOrigin, dfYOrigin, dfWidth, dfHeight;

    dfXOrigin = adfGeoTransform[0] + 0.5 * adfGeoTransform[1];
    dfYOrigin = adfGeoTransform[3] + (nRasterYSize-0.5) * adfGeoTransform[5];
    dfWidth = adfGeoTransform[1];
    dfHeight = - adfGeoTransform[5];
    

    memcpy( header + 0, &dfYOrigin, 8 );
    CPL_MSBPTR64( header + 0 );

    memcpy( header + 8, &dfXOrigin, 8 );
    CPL_MSBPTR64( header + 8 );

    memcpy( header + 16, &dfHeight, 8 );
    CPL_MSBPTR64( header + 16 );

    memcpy( header + 24, &dfWidth, 8 );
    CPL_MSBPTR64( header + 24 );

    if( VSIFSeekL( fpImage, SEEK_SET, 0 ) != 0 
        || VSIFWriteL( header, 32, 1, fpImage ) != 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to write geotrasform header to gtx failed." );
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *GTXDataset::GetProjectionRef()

{
    return SRS_WKT_WGS84;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *GTXDataset::Create( const char * pszFilename,
                                 int nXSize, int nYSize, CPL_UNUSED int nBands,
                                 GDALDataType eType,
                                 CPL_UNUSED char ** papszOptions )

{
    if( eType != GDT_Float32 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to create gtx file with unsupported data type '%s'.",
                  GDALGetDataTypeName( eType ) );
        return NULL;
    }

    if( !EQUAL(CPLGetExtension(pszFilename),"gtx") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to create gtx file with extension other than gtx." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try to create the file.                                         */
/* -------------------------------------------------------------------- */
    VSILFILE	*fp;

    fp = VSIFOpenL( pszFilename, "wb" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file `%s' failed.\n",
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Write out the header with stub georeferencing.                  */
/* -------------------------------------------------------------------- */
    unsigned char header[40];
    double dfXOrigin=0, dfYOrigin=0, dfXSize=0.01, dfYSize=0.01;
    GInt32 nXSize32 = nXSize, nYSize32 = nYSize;

    memcpy( header + 0, &dfYOrigin, 8 );
    CPL_MSBPTR64( header + 0 );

    memcpy( header + 8, &dfXOrigin, 8 );
    CPL_MSBPTR64( header + 8 );

    memcpy( header + 16, &dfYSize, 8 );
    CPL_MSBPTR64( header + 16 );

    memcpy( header + 24, &dfXSize, 8 );
    CPL_MSBPTR64( header + 24 );

    memcpy( header + 32, &nYSize32, 4 );
    CPL_MSBPTR32( header + 32 );
    memcpy( header + 36, &nXSize32, 4 );
    CPL_MSBPTR32( header + 36 );

    VSIFWriteL( header, 40, 1, fp );
    VSIFCloseL( fp );

    return (GDALDataset *) GDALOpen( pszFilename, GA_Update );
}


/************************************************************************/
/*                          GDALRegister_GTX()                          */
/************************************************************************/

void GDALRegister_GTX()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "GTX" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "GTX" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "NOAA Vertical Datum .GTX" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "gtx" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
//        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
//                                   "frmt_various.html#GTX" );
        
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Float32" );

        poDriver->pfnOpen = GTXDataset::Open;
        poDriver->pfnIdentify = GTXDataset::Identify;
        poDriver->pfnCreate = GTXDataset::Create;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
