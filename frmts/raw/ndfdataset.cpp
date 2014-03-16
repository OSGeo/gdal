/******************************************************************************
 * $Id$
 *
 * Project:  NDF Driver
 * Purpose:  Implementation of NLAPS Data Format read support.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam
 * Copyright (c) 2008-2011, Even Rouault <even dot rouault at mines-paris dot org>
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
    char       **papszExtraFiles;

    char        **papszHeader;
    const char  *Get( const char *pszKey, const char *pszDefault);

  public:
    		NDFDataset();
    	        ~NDFDataset();

    virtual CPLErr  GetGeoTransform( double * padfTransform );
    virtual const char *GetProjectionRef(void);
    virtual char **GetFileList(void);

    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/*                            NDFDataset()                             */
/************************************************************************/

NDFDataset::NDFDataset()
{
    pszProjection = CPLStrdup("");

    papszHeader = NULL;
    papszExtraFiles = NULL;

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
    CSLDestroy( papszExtraFiles );

    for( int i = 0; i < GetRasterCount(); i++ )
    {
        VSIFCloseL( ((RawRasterBand *) GetRasterBand(i+1))->GetFPL() );
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
/*                            GetFileList()                             */
/************************************************************************/

char **NDFDataset::GetFileList()

{
    char **papszFileList = NULL;

    // Main data file, etc. 
    papszFileList = GDALPamDataset::GetFileList();

    // Header file.
    papszFileList = CSLInsertStrings( papszFileList, -1,
                                      papszExtraFiles );

    return papszFileList;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *NDFDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      The user must select the header file (ie. .H1).                 */
/* -------------------------------------------------------------------- */
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

    VSILFILE* fp = VSIFOpenL(poOpenInfo->pszFilename, "rb");
    if (fp == NULL)
        return NULL;

    const char *pszLine;
    const int nHeaderMax = 1000;
    int nHeaderLines = 0;
    char **papszHeader = (char **) CPLMalloc(sizeof(char *) * (nHeaderMax+1));

    while( nHeaderLines < nHeaderMax
           && (pszLine = CPLReadLineL( fp )) != NULL
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
    VSIFCloseL(fp);
    fp = NULL;
    
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
        CSLDestroy( papszHeader );
        return NULL;
    }
        
/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CSLDestroy( papszHeader );
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The NDF driver does not support update access to existing"
                  " datasets.\n" );
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
    const char* pszBand = CSLFetchNameValue(papszHeader, 
                                        "NUMBER_OF_BANDS_IN_VOLUME");
    if (pszBand == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find band count");
        delete poDS;
        return NULL;
    }
    int nBands = atoi(pszBand);

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize) ||
        !GDALCheckBandCount(nBands, FALSE))
    {
        delete poDS;
        return NULL;
    }

    for( iBand = 0; iBand < nBands; iBand++ )
    {
        char szKey[100];
        CPLString osFilename;

        sprintf( szKey, "BAND%d_FILENAME", iBand+1 );
        osFilename = poDS->Get(szKey,"");

        // NDF1 file do not include the band filenames.
        if( osFilename.size() == 0 )
        {
            char szBandExtension[15];
            sprintf( szBandExtension, "I%d", iBand+1 );
            osFilename = CPLResetExtension( poOpenInfo->pszFilename, 
                                            szBandExtension );
        }
        else
        {      
            CPLString osBasePath = CPLGetPath(poOpenInfo->pszFilename);
            osFilename = CPLFormFilename( osBasePath, osFilename, NULL);
        }

        VSILFILE *fpRaw = VSIFOpenL( osFilename, "rb" );
        if( fpRaw == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Failed to open band file: %s", 
                      osFilename.c_str() );
            delete poDS;
            return NULL;
        }
        poDS->papszExtraFiles = 
            CSLAddString( poDS->papszExtraFiles, 
                          osFilename );

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
/*      Fetch and parse USGS projection parameters.                     */
/* -------------------------------------------------------------------- */
    double adfUSGSParms[15] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    char **papszParmTokens = 
        CSLTokenizeStringComplex( poDS->Get( "USGS_PROJECTION_NUMBER", "" ),
                                  ",", FALSE, TRUE );

    if( CSLCount( papszParmTokens ) >= 15 )
    {
        int i;
        for( i = 0; i < 15; i++ )
            adfUSGSParms[i] = atof(papszParmTokens[i]);
    }
    CSLDestroy(papszParmTokens);
    papszParmTokens = NULL;

/* -------------------------------------------------------------------- */
/*      Minimal georef support ... should add full USGS style           */
/*      support at some point.                                          */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS;
    int nUSGSProjection = atoi(poDS->Get( "USGS_PROJECTION_NUMBER", "" ));
    int nZone = atoi(poDS->Get("USGS_MAP_ZONE","0"));

    oSRS.importFromUSGS( nUSGSProjection, nZone, adfUSGSParms, 12 );

    CPLString osDatum = poDS->Get( "HORIZONTAL_DATUM", "" );
    if( EQUAL(osDatum,"WGS84") || EQUAL(osDatum,"NAD83") 
        || EQUAL(osDatum,"NAD27") )
    {
        oSRS.SetWellKnownGeogCS( osDatum );
    }
    else if( EQUALN(osDatum,"NAD27",5) )
    {
        oSRS.SetWellKnownGeogCS( "NAD27" );
    }
    else
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Unrecognised datum name in NLAPS/NDF file:%s, assuming WGS84.", 
                  osDatum.c_str() );
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
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = NDFDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

