/******************************************************************************
 *
 * Project:  Horizontal Datum Formats
 * Purpose:  Implementation of the CTable2 format, a PROJ.4 specific format
 *           that is more compact (due to a lack of unused error band) than NTv2
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2012, Frank Warmerdam
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

/************************************************************************/
/* ==================================================================== */
/*                              CTable2Dataset                          */
/* ==================================================================== */
/************************************************************************/

class CTable2Dataset : public RawDataset
{
  public:
    VSILFILE    *fpImage;  // image data file.

    double      adfGeoTransform[6];

  public:
                CTable2Dataset();
    virtual ~CTable2Dataset();

    virtual CPLErr SetGeoTransform( double * padfTransform ) override;
    virtual CPLErr GetGeoTransform( double * padfTransform ) override;
    virtual const char *GetProjectionRef() override;
    virtual void   FlushCache(void) override;

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszOptions );
};

/************************************************************************/
/* ==================================================================== */
/*                              CTable2Dataset                          */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             CTable2Dataset()                          */
/************************************************************************/

CTable2Dataset::CTable2Dataset() :
    fpImage(NULL)
{
    memset( adfGeoTransform, 0, sizeof(adfGeoTransform) );
}

/************************************************************************/
/*                            ~CTable2Dataset()                          */
/************************************************************************/

CTable2Dataset::~CTable2Dataset()

{
    FlushCache();

    if( fpImage != NULL )
    {
        if( VSIFCloseL( fpImage ) != 0 )
        {
            CPLError(CE_Failure, CPLE_FileIO, "I/O error");
        }
    }
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void CTable2Dataset::FlushCache()

{
    RawDataset::FlushCache();
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int CTable2Dataset::Identify( GDALOpenInfo *poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes < 64 )
        return FALSE;

    if( !STARTS_WITH_CI(
           reinterpret_cast<const char *>( poOpenInfo->pabyHeader + 0 ),
           "CTABLE V2") )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *CTable2Dataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify( poOpenInfo ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    CTable2Dataset *poDS = new CTable2Dataset();
    poDS->eAccess = poOpenInfo->eAccess;

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    CPLString osFilename = poOpenInfo->pszFilename;

    if( poOpenInfo->eAccess == GA_ReadOnly )
        poDS->fpImage = VSIFOpenL( osFilename, "rb" );
    else
        poDS->fpImage = VSIFOpenL( osFilename, "rb+" );

    if( poDS->fpImage == NULL )
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the file header.                                           */
/* -------------------------------------------------------------------- */

    CPL_IGNORE_RET_VAL(VSIFSeekL( poDS->fpImage, 0, SEEK_SET ));

    char achHeader[160] = { '\0' };
    CPL_IGNORE_RET_VAL(VSIFReadL( achHeader, 1, 160, poDS->fpImage ));
    achHeader[16+79] = '\0';

    CPLString osDescription = reinterpret_cast<const char *>( achHeader + 16 );
    osDescription.Trim();
    poDS->SetMetadataItem( "DESCRIPTION", osDescription );

/* -------------------------------------------------------------------- */
/*      Convert from LSB to local machine byte order.                   */
/* -------------------------------------------------------------------- */
    CPL_LSBPTR64( achHeader + 96 );
    CPL_LSBPTR64( achHeader + 104 );
    CPL_LSBPTR64( achHeader + 112 );
    CPL_LSBPTR64( achHeader + 120 );
    CPL_LSBPTR32( achHeader + 128 );
    CPL_LSBPTR32( achHeader + 132 );

/* -------------------------------------------------------------------- */
/*      Extract size, and geotransform.                                 */
/* -------------------------------------------------------------------- */
    int nRasterXSize, nRasterYSize;
    memcpy( &nRasterXSize, achHeader + 128, 4 );
    memcpy( &nRasterYSize, achHeader + 132, 4 );
    if (!GDALCheckDatasetDimensions(nRasterXSize, nRasterYSize) ||
        /* to avoid overflow in later -8 * nRasterXSize computation */
        nRasterXSize >= INT_MAX / 8 )
    {
        delete poDS;
        return NULL;
    }

    poDS->nRasterXSize = nRasterXSize;
    poDS->nRasterYSize = nRasterYSize;

    double adfValues[4];
    memcpy( adfValues, achHeader + 96, sizeof(double)*4 );

    for( int i = 0; i < 4; i++ )
        adfValues[i] *= 180/M_PI; // Radians to degrees.

    poDS->adfGeoTransform[0] = adfValues[0] - adfValues[2]*0.5;
    poDS->adfGeoTransform[1] = adfValues[2];
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[3] = adfValues[1] + adfValues[3]*(nRasterYSize-0.5);
    poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[5] = -adfValues[3];

/* -------------------------------------------------------------------- */
/*      Setup the bands.                                                */
/* -------------------------------------------------------------------- */
    RawRasterBand *poBand =
        new RawRasterBand( poDS, 1, poDS->fpImage,
                           160 + 4 + static_cast<vsi_l_offset>(nRasterXSize) *
                                (nRasterYSize-1) * 2 * 4,
                           8, -8 * nRasterXSize,
                           GDT_Float32, CPL_IS_LSB, TRUE, FALSE );
    poBand->SetDescription( "Latitude Offset (radians)" );
    poDS->SetBand( 1, poBand );

    poBand =
        new RawRasterBand( poDS, 2, poDS->fpImage,
                           160 + static_cast<vsi_l_offset>(nRasterXSize) *
                                (nRasterYSize-1) * 2 * 4,
                           8, -8 * nRasterXSize,
                           GDT_Float32, CPL_IS_LSB, TRUE, FALSE );
    poBand->SetDescription( "Longitude Offset (radians)" );
    poDS->SetBand( 2, poBand );

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

CPLErr CTable2Dataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );
    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr CTable2Dataset::SetGeoTransform( double * padfTransform )

{
    if( eAccess == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Unable to update geotransform on readonly file." );
        return CE_Failure;
    }

    if( padfTransform[2] != 0.0 || padfTransform[4] != 0.0 )
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Rotated and sheared geotransforms not supported for CTable2." );
        return CE_Failure;
    }

    memcpy( adfGeoTransform, padfTransform, sizeof(double)*6 );

/* -------------------------------------------------------------------- */
/*      Update grid header.                                             */
/* -------------------------------------------------------------------- */
    const double dfDegToRad = M_PI / 180.0;

    // read grid header
    CPL_IGNORE_RET_VAL(VSIFSeekL( fpImage, 0, SEEK_SET ));

    char achHeader[160] = { '\0' };
    CPL_IGNORE_RET_VAL(VSIFReadL( achHeader, 1, sizeof(achHeader), fpImage ));

    // lower left origin (longitude, center of pixel, radians)
    double dfValue = (adfGeoTransform[0] + adfGeoTransform[1]*0.5) * dfDegToRad;
    CPL_LSBPTR64( &dfValue );
    memcpy( achHeader + 96, &dfValue, 8 );

    // lower left origin (latitude, center of pixel, radians)
    dfValue = (adfGeoTransform[3] + adfGeoTransform[5] * (nRasterYSize-0.5))
        * dfDegToRad;
    CPL_LSBPTR64( &dfValue );
    memcpy( achHeader + 104, &dfValue, 8 );

    // pixel width (radians)
    dfValue = adfGeoTransform[1] * dfDegToRad;
    CPL_LSBPTR64( &dfValue );
    memcpy( achHeader + 112, &dfValue, 8 );

    // pixel height (radians)
    dfValue = adfGeoTransform[5] * -1 * dfDegToRad;
    CPL_LSBPTR64( &dfValue );
    memcpy( achHeader + 120, &dfValue, 8 );

    // write grid header.
    CPL_IGNORE_RET_VAL(VSIFSeekL( fpImage, 0, SEEK_SET ));
    CPL_IGNORE_RET_VAL(VSIFWriteL( achHeader, 11, 16, fpImage ));

    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *CTable2Dataset::GetProjectionRef()

{
    return SRS_WKT_WGS84;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *CTable2Dataset::Create( const char * pszFilename,
                                     int nXSize,
                                     int nYSize,
                                     CPL_UNUSED int nBands,
                                     GDALDataType eType,
                                     char ** papszOptions )
{
    if( eType != GDT_Float32 )
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Attempt to create CTable2 file with unsupported data type '%s'.",
            GDALGetDataTypeName( eType ) );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try to open or create file.                                     */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL( pszFilename, "wb" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file `%s' failed.\n",
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a file header, with a defaulted georeferencing.          */
/* -------------------------------------------------------------------- */
    char achHeader[160] = { '\0' };

    memset( achHeader, 0, sizeof(achHeader));

    memcpy( achHeader+0, "CTABLE V2.0     ", 16 );

    if( CSLFetchNameValue( papszOptions, "DESCRIPTION" ) != NULL )
        strncpy( achHeader + 16,
                 CSLFetchNameValue( papszOptions, "DESCRIPTION" ),
                 80 );

    // lower left origin (longitude, center of pixel, radians)
    double dfValue = 0;
    CPL_LSBPTR64( &dfValue );
    memcpy( achHeader + 96, &dfValue, 8 );

    // lower left origin (latitude, center of pixel, radians)
    dfValue = 0;
    CPL_LSBPTR64( &dfValue );
    memcpy( achHeader + 104, &dfValue, 8 );

    // pixel width (radians)
    dfValue = 0.01 * M_PI / 180.0;
    CPL_LSBPTR64( &dfValue );
    memcpy( achHeader + 112, &dfValue, 8 );

    // pixel height (radians)
    dfValue = 0.01 * M_PI / 180.0;
    CPL_LSBPTR64( &dfValue );
    memcpy( achHeader + 120, &dfValue, 8 );

    // raster width in pixels
    int nValue32 = nXSize;
    CPL_LSBPTR32( &nValue32 );
    memcpy( achHeader + 128, &nValue32, 4 );

    // raster width in pixels
    nValue32 = nYSize;
    CPL_LSBPTR32( &nValue32 );
    memcpy( achHeader + 132, &nValue32, 4 );

    CPL_IGNORE_RET_VAL(VSIFWriteL( achHeader, 1, sizeof(achHeader), fp ));

/* -------------------------------------------------------------------- */
/*      Write zeroed grid data.                                         */
/* -------------------------------------------------------------------- */
    float *pafLine = (float *) CPLCalloc(sizeof(float)*2,nXSize);

    for( int i = 0; i < nYSize; i++ )
    {
        if( static_cast<int>( VSIFWriteL(
               pafLine, sizeof(float)*2, nXSize, fp ) ) != nXSize )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Write failed at line %d, perhaps the disk is full?",
                      i );
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup and return.                                             */
/* -------------------------------------------------------------------- */
    CPLFree( pafLine );

    if( VSIFCloseL( fp ) != 0 )
    {
        CPLError(CE_Failure, CPLE_FileIO, "I/O error");
        return NULL;
    }

    return (GDALDataset *) GDALOpen( pszFilename, GA_Update );
}

/************************************************************************/
/*                         GDALRegister_CTable2()                       */
/************************************************************************/

void GDALRegister_CTable2()

{
    if( GDALGetDriverByName( "CTable2" ) != NULL )
      return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "CTable2" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "CTable2 Datum Grid Shift" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Float32" );

    poDriver->pfnOpen = CTable2Dataset::Open;
    poDriver->pfnIdentify = CTable2Dataset::Identify;
    poDriver->pfnCreate = CTable2Dataset::Create;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
