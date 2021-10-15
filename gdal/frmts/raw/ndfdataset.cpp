/******************************************************************************
 *
 * Project:  NDF Driver
 * Purpose:  Implementation of NLAPS Data Format read support.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam
 * Copyright (c) 2008-2011, Even Rouault <even dot rouault at spatialys.com>
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

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                              NDFDataset                              */
/* ==================================================================== */
/************************************************************************/

class NDFDataset final: public RawDataset
{
    double      adfGeoTransform[6];

    char        *pszProjection;
    char       **papszExtraFiles;

    char        **papszHeader;
    const char  *Get( const char *pszKey, const char *pszDefault);

    CPL_DISALLOW_COPY_ASSIGN(NDFDataset)

  public:
    NDFDataset();
    ~NDFDataset() override;

    CPLErr  GetGeoTransform( double * padfTransform ) override;
    const char *_GetProjectionRef(void) override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }
    char **GetFileList(void) override;

    static GDALDataset *Open( GDALOpenInfo * );
    static int Identify( GDALOpenInfo * );
};

/************************************************************************/
/*                            NDFDataset()                             */
/************************************************************************/

NDFDataset::NDFDataset() :
    pszProjection(CPLStrdup("")),
    papszExtraFiles(nullptr),
    papszHeader(nullptr)
{
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
    FlushCache(true);
    CPLFree( pszProjection );
    CSLDestroy( papszHeader );
    CSLDestroy( papszExtraFiles );
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *NDFDataset::_GetProjectionRef()

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

    if( pszResult == nullptr )
        return pszDefault;

    return pszResult;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **NDFDataset::GetFileList()

{
    // Main data file, etc.
    char **papszFileList = GDALPamDataset::GetFileList();

    // Header file.
    papszFileList = CSLInsertStrings( papszFileList, -1,
                                      papszExtraFiles );

    return papszFileList;
}

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

int NDFDataset::Identify( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      The user must select the header file (i.e. .H1).                */
/* -------------------------------------------------------------------- */
    return poOpenInfo->nHeaderBytes >= 50 &&
           (STARTS_WITH_CI(reinterpret_cast<const char *>( poOpenInfo->pabyHeader ), "NDF_REVISION=2")
            || STARTS_WITH_CI(reinterpret_cast<const char *>( poOpenInfo->pabyHeader ), "NDF_REVISION=0")) ;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *NDFDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      The user must select the header file (i.e. .H1).                */
/* -------------------------------------------------------------------- */
    if( !Identify(poOpenInfo) || poOpenInfo->fpL == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The NDF driver does not support update access to existing"
                  " datasets." );
        return nullptr;
    }
/* -------------------------------------------------------------------- */
/*      Read and process the header into a local name/value             */
/*      stringlist.  We just take off the trailing semicolon.  The      */
/*      keyword is already separated from the value by an equal         */
/*      sign.                                                           */
/* -------------------------------------------------------------------- */

    const char *pszLine = nullptr;
    const int nHeaderMax = 1000;
    int nHeaderLines = 0;
    char **papszHeader = static_cast<char **>(
        CPLMalloc( sizeof(char *) * (nHeaderMax+1) ) );

    while( nHeaderLines < nHeaderMax
           && (pszLine = CPLReadLineL( poOpenInfo->fpL )) != nullptr
           && !EQUAL(pszLine,"END_OF_HDR;") )
    {
        if( strstr(pszLine,"=") == nullptr )
            break;

        char *pszFixed = CPLStrdup( pszLine );
        if( pszFixed[strlen(pszFixed)-1] == ';' )
            pszFixed[strlen(pszFixed)-1] = '\0';

        papszHeader[nHeaderLines++] = pszFixed;
        papszHeader[nHeaderLines] = nullptr;
    }
    CPL_IGNORE_RET_VAL(VSIFCloseL(poOpenInfo->fpL));
    poOpenInfo->fpL = nullptr;

    if( CSLFetchNameValue( papszHeader, "PIXELS_PER_LINE" ) == nullptr
        || CSLFetchNameValue( papszHeader, "LINES_PER_DATA_FILE" ) == nullptr
        || CSLFetchNameValue( papszHeader, "BITS_PER_PIXEL" ) == nullptr
        || CSLFetchNameValue( papszHeader, "PIXEL_FORMAT" ) == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
              "Dataset appears to be NDF but is missing a required field.");
        CSLDestroy( papszHeader );
        return nullptr;
    }

    if( !EQUAL(CSLFetchNameValue( papszHeader, "PIXEL_FORMAT"), "BYTE" )
        || !EQUAL(CSLFetchNameValue( papszHeader, "BITS_PER_PIXEL"),"8") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Currently NDF driver supports only 8bit BYTE format." );
        CSLDestroy( papszHeader );
        return nullptr;
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
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    NDFDataset *poDS = new NDFDataset();
    poDS->papszHeader = papszHeader;

    poDS->nRasterXSize = atoi(poDS->Get("PIXELS_PER_LINE",""));
    poDS->nRasterYSize = atoi(poDS->Get("LINES_PER_DATA_FILE",""));

/* -------------------------------------------------------------------- */
/*      Create a raw raster band for each file.                         */
/* -------------------------------------------------------------------- */
    const char* pszBand = CSLFetchNameValue(papszHeader,
                                            "NUMBER_OF_BANDS_IN_VOLUME");
    if (pszBand == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find band count");
        delete poDS;
        return nullptr;
    }
    const int nBands = atoi(pszBand);

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize) ||
        !GDALCheckBandCount(nBands, FALSE))
    {
        delete poDS;
        return nullptr;
    }

    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        char szKey[100];
        snprintf( szKey, sizeof(szKey), "BAND%d_FILENAME", iBand+1 );
        CPLString osFilename = poDS->Get(szKey,"");

        // NDF1 file do not include the band filenames.
        if( osFilename.empty() )
        {
            char szBandExtension[15];
            snprintf( szBandExtension, sizeof(szBandExtension), "I%d", iBand+1 );
            osFilename = CPLResetExtension( poOpenInfo->pszFilename,
                                            szBandExtension );
        }
        else
        {
            CPLString osBasePath = CPLGetPath(poOpenInfo->pszFilename);
            osFilename = CPLFormFilename( osBasePath, osFilename, nullptr);
        }

        VSILFILE *fpRaw = VSIFOpenL( osFilename, "rb" );
        if( fpRaw == nullptr )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Failed to open band file: %s",
                      osFilename.c_str() );
            delete poDS;
            return nullptr;
        }
        poDS->papszExtraFiles =
            CSLAddString( poDS->papszExtraFiles,
                          osFilename );

        RawRasterBand *poBand =
            new RawRasterBand( poDS, iBand+1, fpRaw, 0, 1, poDS->nRasterXSize,
                               GDT_Byte, TRUE, RawRasterBand::OwnFP::YES );

        snprintf( szKey, sizeof(szKey), "BAND%d_NAME", iBand+1 );
        poBand->SetDescription( poDS->Get(szKey, "") );

        snprintf( szKey, sizeof(szKey), "BAND%d_WAVELENGTHS", iBand+1 );
        poBand->SetMetadataItem( "WAVELENGTHS", poDS->Get(szKey,"") );

        snprintf( szKey, sizeof(szKey), "BAND%d_RADIOMETRIC_GAINS/BIAS", iBand+1 );
        poBand->SetMetadataItem( "RADIOMETRIC_GAINS_BIAS",
                                 poDS->Get(szKey,"") );

        poDS->SetBand( iBand+1, poBand );
    }

/* -------------------------------------------------------------------- */
/*      Fetch and parse USGS projection parameters.                     */
/* -------------------------------------------------------------------- */
    double adfUSGSParams[15] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    char **papszParamTokens =
        CSLTokenizeStringComplex( poDS->Get( "USGS_PROJECTION_NUMBER", "" ),
                                  ",", FALSE, TRUE );

    if( CSLCount( papszParamTokens ) >= 15 )
    {
        for( int i = 0; i < 15; i++ )
            adfUSGSParams[i] = CPLAtof(papszParamTokens[i]);
    }
    CSLDestroy(papszParamTokens);
    papszParamTokens = nullptr;

/* -------------------------------------------------------------------- */
/*      Minimal georef support ... should add full USGS style           */
/*      support at some point.                                          */
/* -------------------------------------------------------------------- */
    const int nUSGSProjection = atoi(poDS->Get( "USGS_PROJECTION_NUMBER", "" ));
    const int nZone = atoi(poDS->Get("USGS_MAP_ZONE","0"));

    OGRSpatialReference oSRS;
    oSRS.importFromUSGS( nUSGSProjection, nZone, adfUSGSParams, 12 );

    const CPLString osDatum = poDS->Get( "HORIZONTAL_DATUM", "" );
    if( EQUAL(osDatum,"WGS84") || EQUAL(osDatum,"NAD83")
        || EQUAL(osDatum,"NAD27") )
    {
        oSRS.SetWellKnownGeogCS( osDatum );
    }
    else if( STARTS_WITH_CI(osDatum, "NAD27") )
    {
        oSRS.SetWellKnownGeogCS( "NAD27" );
    }
    else
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Unrecognized datum name in NLAPS/NDF file:%s, "
                  "assuming WGS84.",
                  osDatum.c_str() );
        oSRS.SetWellKnownGeogCS( "WGS84" );
    }

    if( oSRS.GetRoot() != nullptr )
    {
        CPLFree( poDS->pszProjection );
        poDS->pszProjection = nullptr;
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
        poDS->adfGeoTransform[0] = CPLAtof(papszUL[2]);
        poDS->adfGeoTransform[1] =
            (CPLAtof(papszUR[2]) - CPLAtof(papszUL[2])) / (poDS->nRasterXSize-1);
        poDS->adfGeoTransform[2] =
            (CPLAtof(papszUR[3]) - CPLAtof(papszUL[3])) / (poDS->nRasterXSize-1);

        poDS->adfGeoTransform[3] = CPLAtof(papszUL[3]);
        poDS->adfGeoTransform[4] =
            (CPLAtof(papszLL[2]) - CPLAtof(papszUL[2])) / (poDS->nRasterYSize-1);
        poDS->adfGeoTransform[5] =
            (CPLAtof(papszLL[3]) - CPLAtof(papszUL[3])) / (poDS->nRasterYSize-1);

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

    return poDS;
}

/************************************************************************/
/*                          GDALRegister_NDF()                          */
/************************************************************************/

void GDALRegister_NDF()

{
    if( GDALGetDriverByName( "NDF" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "NDF" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "NLAPS Data Format" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/ndf.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnIdentify = NDFDataset::Identify;
    poDriver->pfnOpen = NDFDataset::Open;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
