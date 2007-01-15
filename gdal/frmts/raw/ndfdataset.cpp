/******************************************************************************
 * $Id$
 *
 * Project:  NDF Driver
 * Purpose:  Implementation of NLAPS Data Format read support.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.4  2005/05/05 13:55:42  fwarmerdam
 * PAM Enable
 *
 * Revision 1.3  2005/03/05 17:27:00  fwarmerdam
 * updated to support NDF1 and rotated datasets
 *
 * Revision 1.2  2005/03/05 17:07:30  fwarmerdam
 * Fixed up an off-by-half-a-pixel error in geotransform.
 *
 * Revision 1.1  2005/01/06 20:27:22  fwarmerdam
 * New
 *
 */

#include "rawdataset.h"
#include "ogr_spatialref.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*				NDFDataset				*/
/* ==================================================================== */
/************************************************************************/

class NDFDataset : public RawDataset
{
    double      adfGeoTransform[6];

    char	*pszProjection;

    char        **papszHeader;
    const char  *Get( const char *pszKey, const char *pszDefault);

    int         ReadHeader( FILE * );

  public:
    		NDFDataset();
    	        ~NDFDataset();

    virtual CPLErr  GetGeoTransform( double * padfTransform );
    virtual const char *GetProjectionRef(void);

    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/*                            NDFDataset()                             */
/************************************************************************/

NDFDataset::NDFDataset()
{
    pszProjection = CPLStrdup("");

    papszHeader = NULL;

    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~NDFDataset()                            */
/************************************************************************/

NDFDataset::~NDFDataset()

{
    FlushCache();
    CPLFree( pszProjection );
    CSLDestroy( papszHeader );

    for( int i = 0; i < GetRasterCount(); i++ )
    {
        VSIFCloseL( ((RawRasterBand *) GetRasterBand(i+1))->GetFP() );
    }
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *NDFDataset::GetProjectionRef()

{
    return pszProjection;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr NDFDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
    return CE_None;
}

/************************************************************************/
/*                                Get()                                 */
/*                                                                      */
/*      Fetch a value from the header by keyword.                       */
/************************************************************************/

const char *NDFDataset::Get( const char *pszKey, const char *pszDefault )

{
    const char *pszResult = CSLFetchNameValue( papszHeader, pszKey );

    if( pszResult == NULL )
        return pszDefault;
    else
        return pszResult;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *NDFDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      The user must select the header file (ie. .H1).                 */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->fp == NULL )
        return NULL;

    if( poOpenInfo->nHeaderBytes < 50 )
        return NULL;

    if( !EQUALN((const char *)poOpenInfo->pabyHeader,"NDF_REVISION=2",14) 
        && !EQUALN((const char *)poOpenInfo->pabyHeader,"NDF_REVISION=0",14) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Read and process the header into a local name/value             */
/*      stringlist.  We just take off the trailing semicolon.  The      */
/*      keyword is already seperated from the value by an equal         */
/*      sign.                                                           */
/* -------------------------------------------------------------------- */
    const char *pszLine;
    const int nHeaderMax = 1000;
    int nHeaderLines = 0;
    char **papszHeader = (char **) CPLMalloc(sizeof(char *) * (nHeaderMax+1));

    VSIRewind( poOpenInfo->fp );
    while( nHeaderLines < nHeaderMax 
           && (pszLine = CPLReadLine( poOpenInfo->fp )) != NULL 
           && !EQUAL(pszLine,"END_OF_HDR;") )
    {
        char *pszFixed;

        if( strstr(pszLine,"=") == NULL )
            break;

        pszFixed = CPLStrdup( pszLine );
        if( pszFixed[strlen(pszFixed)-1] == ';' )
            pszFixed[strlen(pszFixed)-1] = '\0';
        
        papszHeader[nHeaderLines++] = pszFixed;
        papszHeader[nHeaderLines] = NULL;
    }
    
    if( CSLFetchNameValue( papszHeader, "PIXELS_PER_LINE" ) == NULL 
        || CSLFetchNameValue( papszHeader, "LINES_PER_DATA_FILE" ) == NULL 
        || CSLFetchNameValue( papszHeader, "BITS_PER_PIXEL" ) == NULL 
        || CSLFetchNameValue( papszHeader, "PIXEL_FORMAT" ) == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
              "Dataset appears to be NDF but is missing a required field.");
        CSLDestroy( papszHeader );
        return NULL;
    }

    if( !EQUAL(CSLFetchNameValue( papszHeader, "PIXEL_FORMAT"),
               "BYTE" ) 
        || !EQUAL(CSLFetchNameValue( papszHeader, "BITS_PER_PIXEL"),"8") )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Currently NDF driver supports only 8bit BYTE format." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    NDFDataset 	*poDS;

    poDS = new NDFDataset();
    poDS->papszHeader = papszHeader;

    poDS->nRasterXSize = atoi(poDS->Get("PIXELS_PER_LINE",""));
    poDS->nRasterYSize = atoi(poDS->Get("LINES_PER_DATA_FILE",""));

/* -------------------------------------------------------------------- */
/*      Create a raw raster band for each file.                         */
/* -------------------------------------------------------------------- */
    int iBand;
    int nBands = atoi(CSLFetchNameValue(papszHeader, 
                                        "NUMBER_OF_BANDS_IN_VOLUME"));

    for( iBand = 0; iBand < nBands; iBand++ )
    {
        char szKey[100];
        const char *pszFilename;

        sprintf( szKey, "BAND%d_FILENAME", iBand+1 );
        pszFilename = poDS->Get(szKey,NULL);

        // NDF1 file do not include the band filenames.
        if( pszFilename == NULL )
        {
            char szBandExtension[15];
            sprintf( szBandExtension, "I%d", iBand+1 );
            pszFilename = CPLResetExtension( poOpenInfo->pszFilename, 
                                             szBandExtension );
        }

        FILE *fpRaw = VSIFOpenL( pszFilename, "rb" );
        if( fpRaw == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Failed to open band file: %s", 
                      pszFilename );
            delete poDS;
            return NULL;
        }

        RawRasterBand *poBand = 
            new RawRasterBand( poDS, iBand+1, fpRaw, 0, 1, poDS->nRasterXSize,
                               GDT_Byte, TRUE, TRUE );
        
        sprintf( szKey, "BAND%d_NAME", iBand+1 );
        poBand->SetDescription( poDS->Get(szKey, "") );

        sprintf( szKey, "BAND%d_WAVELENGTHS", iBand+1 );
        poBand->SetMetadataItem( "WAVELENGTHS", poDS->Get(szKey,"") );

        sprintf( szKey, "BAND%d_RADIOMETRIC_GAINS/BIAS", iBand+1 );
        poBand->SetMetadataItem( "RADIOMETRIC_GAINS_BIAS", 
                                 poDS->Get(szKey,"") );

        poDS->SetBand( iBand+1, poBand );
    }

/* -------------------------------------------------------------------- */
/*      Minimal georef support ... should add full USGS style           */
/*      support at some point.                                          */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS;

    if( EQUAL(poDS->Get( "USGS_PROJECTION_NUMBER", "" ),"1") )
    {
        oSRS.SetUTM( atoi(poDS->Get("USGS_MAP_ZONE","0")) );
        oSRS.SetWellKnownGeogCS( "WGS84" );
    }

    if( oSRS.GetRoot() != NULL )
    {
        CPLFree( poDS->pszProjection );
        poDS->pszProjection = NULL;
        oSRS.exportToWkt( &(poDS->pszProjection) );
    }

/* -------------------------------------------------------------------- */
/*      Get geotransform.                                               */
/* -------------------------------------------------------------------- */
    char **papszUL = CSLTokenizeString2( 
        poDS->Get("UPPER_LEFT_CORNER",""), ",", 0 );
    char **papszUR = CSLTokenizeString2( 
        poDS->Get("UPPER_RIGHT_CORNER",""), ",", 0 );
    char **papszLL = CSLTokenizeString2( 
        poDS->Get("LOWER_LEFT_CORNER",""), ",", 0 );
    
    if( CSLCount(papszUL) == 4 
        && CSLCount(papszUR) == 4 
        && CSLCount(papszLL) == 4 )
    {
        poDS->adfGeoTransform[0] = atof(papszUL[2]);
        poDS->adfGeoTransform[1] = 
            (atof(papszUR[2]) - atof(papszUL[2])) / (poDS->nRasterXSize-1);
        poDS->adfGeoTransform[2] = 
            (atof(papszUR[3]) - atof(papszUL[3])) / (poDS->nRasterXSize-1);

        poDS->adfGeoTransform[3] = atof(papszUL[3]);
        poDS->adfGeoTransform[4] = 
            (atof(papszLL[2]) - atof(papszUL[2])) / (poDS->nRasterYSize-1);
        poDS->adfGeoTransform[5] = 
            (atof(papszLL[3]) - atof(papszUL[3])) / (poDS->nRasterYSize-1);

        // Move origin up-left half a pixel.
        poDS->adfGeoTransform[0] -= poDS->adfGeoTransform[1] * 0.5;
        poDS->adfGeoTransform[0] -= poDS->adfGeoTransform[4] * 0.5;
        poDS->adfGeoTransform[3] -= poDS->adfGeoTransform[2] * 0.5;
        poDS->adfGeoTransform[3] -= poDS->adfGeoTransform[5] * 0.5;
    }

    CSLDestroy( papszUL );
    CSLDestroy( papszLL );
    CSLDestroy( papszUR );
                      
/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

    return( poDS );
}

/************************************************************************/
/*                          GDALRegister_NDF()                          */
/************************************************************************/

void GDALRegister_NDF()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "NDF" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "NDF" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "NLAPS Data Format" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#NDF" );

        poDriver->pfnOpen = NDFDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

