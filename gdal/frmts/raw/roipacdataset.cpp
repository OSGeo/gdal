/******************************************************************************
 * $Id$
 *
 * Project:  ROI_PAC Raster Reader
 * Purpose:  Implementation of the ROI_PAC raster reader
 * Author:   Matthieu Volat, matthieu.volat@ujf-grenoble.fr
 *
 ******************************************************************************
 * Copyright (c) 2014, Matthieu Volat <matthieu.volat@ujf-grenoble.fr>
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

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_ROIPAC(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*                             ROIPACDataset                            */
/* ==================================================================== */
/************************************************************************/

class ROIPACRasterBand;

class ROIPACDataset : public RawDataset
{
    friend class ROIPACRasterBand;

    VSILFILE    *fpImage;
    VSILFILE    *fpRsc;
    
    char        **papszRsc;

    double      adfGeoTransform[6];
    char        *pszProjection;
    bool        bValidGeoTransform;

  public:
                ROIPACDataset();
                ~ROIPACDataset();
    
    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
    CPLErr              GetGeoTransform( double * padfTransform );
    const char         *GetProjectionRef();
};

/************************************************************************/
/* ==================================================================== */
/*                           ROIPACRasterBand                           */
/* ==================================================================== */
/************************************************************************/

class ROIPACRasterBand : public RawRasterBand
{
    public:
                ROIPACRasterBand( GDALDataset *poDS, int nBand, void * fpRaw,
                                  vsi_l_offset nImgOffset, int nPixelOffset,
                                  int nLineOffset,
                                  GDALDataType eDataType, int bNativeOrder,
                                  int bIsVSIL = FALSE, int bOwnsFP = FALSE );
};

/************************************************************************/
/*                           getRscFilename()                           */
/************************************************************************/

static CPLString getRscFilename( GDALOpenInfo *poOpenInfo )
{
    CPLString osRscFilename;

    char **papszSiblingFiles = poOpenInfo->GetSiblingFiles();
    if ( papszSiblingFiles == NULL )
    {
        osRscFilename = CPLFormFilename( NULL, poOpenInfo->pszFilename, 
                                        "rsc" );
        VSIStatBufL psRscStatBuf;
        if ( VSIStatL( osRscFilename, &psRscStatBuf ) != 0 ) 
        {
            osRscFilename = "";
        }
    }
    else
    {
        /* ------------------------------------------------------------ */
        /*      We need to tear apart the filename to form a .rsc       */
        /*      filename.                                               */
        /* ------------------------------------------------------------ */
        CPLString osPath = CPLGetPath( poOpenInfo->pszFilename );
        CPLString osName = CPLGetFilename( poOpenInfo->pszFilename );

        int iFile = CSLFindString( papszSiblingFiles,
                                   CPLFormFilename( NULL, osName, "rsc" ) );
        if( iFile >= 0 )
        {
            osRscFilename = CPLFormFilename( osPath,
                                             papszSiblingFiles[iFile],
                                             NULL );
        }
    }

    return osRscFilename;
}

/************************************************************************/
/*                            ROIPACDataset()                           */
/************************************************************************/

ROIPACDataset::ROIPACDataset()
{
    fpImage = NULL;
    fpRsc = NULL;
    papszRsc = NULL;
    adfGeoTransform[0] =  0.0;
    adfGeoTransform[1] =  1.0;
    adfGeoTransform[2] =  0.0;
    adfGeoTransform[3] =  0.0;
    adfGeoTransform[4] =  0.0;
    adfGeoTransform[5] =  1.0;
    pszProjection = NULL;
    bValidGeoTransform = false;
}

/************************************************************************/
/*                            ~ROIPACDataset()                          */
/************************************************************************/

ROIPACDataset::~ROIPACDataset()
{
    FlushCache();
    if ( fpRsc != NULL )
    {
        VSIFCloseL( fpRsc );
    }
    if ( fpImage != NULL )
    {
        VSIFCloseL( fpImage );
    }
    CSLDestroy( papszRsc );
    CPLFree( pszProjection );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *ROIPACDataset::Open( GDALOpenInfo * poOpenInfo )
{
/* -------------------------------------------------------------------- */
/*      Confirm that the header is compatible with a ROIPAC dataset.    */
/* -------------------------------------------------------------------- */
    if ( !Identify(poOpenInfo) )
    {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if ( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The ROI_PAC driver does not support update access to "
                  "existing datasets.\n" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Open the .rsc file                                              */
/* -------------------------------------------------------------------- */
    CPLString osRscFilename = getRscFilename( poOpenInfo );
    if ( osRscFilename.empty() )
    {
        return NULL;
    }
    VSILFILE *fpRsc = VSIFOpenL( osRscFilename, "r" );
    if ( fpRsc == NULL )
    {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Load the .rsc information.                                      */
/* -------------------------------------------------------------------- */
    char **papszRsc = NULL;
    while ( true )
    {
        const char *pszLine;
        char **papszTokens;

        pszLine = CPLReadLineL( fpRsc );
        if (pszLine == NULL)
        {
            break;
        }

        papszTokens = CSLTokenizeString2( pszLine, " \t",
                                          CSLT_STRIPLEADSPACES
                                            | CSLT_STRIPENDSPACES
                                            | CSLT_PRESERVEQUOTES
                                            | CSLT_PRESERVEESCAPES );
        if ( papszTokens == NULL 
             || papszTokens[0] == NULL || papszTokens[1] == NULL )
        {
            CSLDestroy ( papszTokens );
            break;
        }
        papszRsc = CSLSetNameValue( papszRsc,
                                       papszTokens[0], papszTokens[1] );

        CSLDestroy ( papszTokens );
    }

/* -------------------------------------------------------------------- */
/*      Fetch required fields.                                          */
/* -------------------------------------------------------------------- */
    int nWidth = 0, nFileLength = 0;
    if ( CSLFetchNameValue( papszRsc, "WIDTH" ) == NULL
        || CSLFetchNameValue( papszRsc, "FILE_LENGTH" ) == NULL )
    {
        CSLDestroy( papszRsc );
        VSIFCloseL( fpRsc );
        return NULL;
    }
    nWidth = atoi( CSLFetchNameValue( papszRsc, "WIDTH" ) ); 
    nFileLength = atoi( CSLFetchNameValue( papszRsc, "FILE_LENGTH" ) ); 

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    ROIPACDataset *poDS;
    poDS = new ROIPACDataset();
    poDS->nRasterXSize = nWidth;
    poDS->nRasterYSize = nFileLength;
    poDS->fpImage = poOpenInfo->fpL;
    poOpenInfo->fpL = NULL;
    poDS->fpRsc = fpRsc;
    poDS->papszRsc = papszRsc;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    GDALDataType eDataType;
    int nBands;
    enum Interleave { LINE, PIXEL } eInterleave;
    const char *pszExtension = CPLGetExtension(poOpenInfo->pszFilename);
    if ( strcmp( pszExtension, "raw" ) == 0 )
    {
        /* ------------------------------------------------------------ */
        /* TODO: ROI_PAC raw images are what would be GDT_CInt8 typed,  */
        /* but since that type do not exist, we will have to implement  */
        /* a specific case in the RasterBand to convert it to           */
        /* GDT_CInt16 for example                                       */
        /* ------------------------------------------------------------ */
#if 0
        eDataType = GDT_CInt8;
        nBands = 1;
        eInterleave = PIXEL;
#else
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Reading ROI_PAC raw files is not supported yet." );
        delete poDS;
        return NULL;
#endif
    }
    else if ( strcmp( pszExtension, "int" ) == 0
                || strcmp( pszExtension, "slc" ) == 0 )
    {
        eDataType = GDT_CFloat32;
        nBands = 1;
        eInterleave = PIXEL;
    }
    else if ( strcmp( pszExtension, "amp" ) == 0 )
    {
        eDataType = GDT_Float32;
        nBands = 2;
        eInterleave = PIXEL;
    }
    else if ( strcmp( pszExtension, "cor" ) == 0 
                || strcmp( pszExtension, "hgt" ) == 0 
                || strcmp( pszExtension, "unw" ) == 0 
                || strcmp( pszExtension, "msk" ) == 0 
                || strcmp( pszExtension, "trans" ) == 0 )
    {
        eDataType = GDT_Float32;
        nBands = 2;
        eInterleave = LINE;
    }
    else if ( strcmp( pszExtension, "dem" ) == 0 )
    {
        eDataType = GDT_Int16;
        nBands = 1;
        eInterleave = PIXEL;
    }
    else { /* Eeek */
        delete poDS;
        return NULL;
    }
    int nPixelOffset;
    int nLineOffset;
    int nBandOffset;
    if (eInterleave == LINE)
    {
        nPixelOffset = GDALGetDataTypeSize(eDataType)/8;
        nLineOffset = nPixelOffset * nWidth * nBands;
        nBandOffset = GDALGetDataTypeSize(eDataType)/8 * nWidth;
    }
    else { /* PIXEL */
        nPixelOffset = GDALGetDataTypeSize(eDataType)/8 * nBands;
        nLineOffset = nPixelOffset * nWidth * nBands;
        nBandOffset = GDALGetDataTypeSize(eDataType)/8;
    }
    poDS->nBands = nBands;
    for (int b = 0; b < nBands; b++)
    {
        poDS->SetBand( b + 1,
                       new ROIPACRasterBand( poDS, b + 1, poDS->fpImage,
                                             nBandOffset * b,
                                             nPixelOffset, nLineOffset,
                                             eDataType, TRUE,
                                             TRUE, FALSE ) );
    }

/* -------------------------------------------------------------------- */
/*      Set all the header metadata into the ROI_PAC domain             */
/* -------------------------------------------------------------------- */
    for (int i = 0; i < CSLCount( papszRsc ); i++)
    {
        char **papszTokens;
        papszTokens = CSLTokenizeString2( papszRsc[i],
                                          "=",
                                          CSLT_STRIPLEADSPACES
                                            | CSLT_STRIPENDSPACES);
        poDS->SetMetadataItem(papszTokens[0], papszTokens[1], "ROI_PAC");
        CSLDestroy( papszTokens );
    }

/* -------------------------------------------------------------------- */
/*      Interpret georeferencing, if present.                           */
/* -------------------------------------------------------------------- */
    if ( CSLFetchNameValue( papszRsc, "X_FIRST" ) != NULL
          && CSLFetchNameValue( papszRsc, "X_STEP" ) != NULL
          && CSLFetchNameValue( papszRsc, "Y_FIRST" ) != NULL
          && CSLFetchNameValue( papszRsc, "Y_STEP" ) != NULL )
    {
        poDS->adfGeoTransform[0] = CPLAtof( CSLFetchNameValue( papszRsc, 
                                                               "X_FIRST" ) );
        poDS->adfGeoTransform[1] = CPLAtof( CSLFetchNameValue( papszRsc, 
                                                               "X_STEP" ) );
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = CPLAtof( CSLFetchNameValue( papszRsc, 
                                                               "Y_FIRST" ) );
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = CPLAtof( CSLFetchNameValue( papszRsc, 
                                                               "Y_STEP" ) );
        poDS->bValidGeoTransform = true;
    }
    if ( CSLFetchNameValue( papszRsc, "PROJECTION" ) != NULL )
    {
        /* ------------------------------------------------------------ */
        /* In ROI_PAC, images are georeferenced either with lat/long or */
        /* UTM projection. However, using UTM projection is dangerous   */
        /* because there is no North/South field, or use of latitude    */
        /* bands!                                                       */
        /* ------------------------------------------------------------ */
        OGRSpatialReference oSRS;
        if ( strcmp( CSLFetchNameValue( papszRsc, "PROJECTION" ),
                     "LL" ) == 0 )
        {
            if ( CSLFetchNameValue( papszRsc, "DATUM" ) != NULL )
            {
                oSRS.SetWellKnownGeogCS( CSLFetchNameValue( papszRsc, 
                                                            "DATUM" ) );
            }
            else {
                oSRS.SetWellKnownGeogCS( "WGS84" );
            }
        }
        else if( strncmp( CSLFetchNameValue( papszRsc, "PROJECTION" ),
                          "UTM", 3 ) == 0 )
        {
            const char *pszZone = CSLFetchNameValue( papszRsc, 
                                                     "PROJECTION" ) + 3;
            oSRS.SetUTM( atoi( pszZone ), TRUE ); /* FIXME: north/south? */
            if ( CSLFetchNameValue( papszRsc, "DATUM" ) != NULL )
            {
                oSRS.SetWellKnownGeogCS( CSLFetchNameValue( papszRsc, 
                                                            "DATUM" ) );
            }
            else {
                oSRS.SetWellKnownGeogCS( "NAD27" );
            }
        }
        oSRS.exportToWkt( &poDS->pszProjection );
    }

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
/*                             Identify()                               */
/************************************************************************/

int ROIPACDataset::Identify( GDALOpenInfo * poOpenInfo )
{
/* -------------------------------------------------------------------- */
/*      Check if:                                                       */
/*      * 1. The data file extension is known                           */
/* -------------------------------------------------------------------- */
    const char *pszExtension = CPLGetExtension(poOpenInfo->pszFilename);
    if ( strcmp( pszExtension, "raw" ) == 0 )
    {
        /* Since gdal do not read natively CInt8, more work is needed
         * to read raw files */
        return false;
    }
    bool bExtensionIsValid = strcmp( pszExtension, "int" ) == 0
                               || strcmp( pszExtension, "slc" ) == 0
                               || strcmp( pszExtension, "amp" ) == 0
                               || strcmp( pszExtension, "cor" ) == 0
                               || strcmp( pszExtension, "hgt" ) == 0
                               || strcmp( pszExtension, "unw" ) == 0
                               || strcmp( pszExtension, "msk" ) == 0
                               || strcmp( pszExtension, "trans" ) == 0
                               || strcmp( pszExtension, "dem" ) == 0;
    if ( !bExtensionIsValid )
    {
        return false;
    }

/* -------------------------------------------------------------------- */
/*      * 2. there is a .rsc file                                      */
/* -------------------------------------------------------------------- */
    CPLString osRscFilename = getRscFilename( poOpenInfo );
    if ( osRscFilename.empty() )
    {
        return false;
    }

    return true;
}

/************************************************************************/
/*                         GetGeoTransform()                            */
/************************************************************************/

CPLErr ROIPACDataset::GetGeoTransform( double *padfTransform )
{
    memcpy(padfTransform, adfGeoTransform, sizeof(adfGeoTransform));
    return (bValidGeoTransform) ? CE_None : CE_Failure;
}

/************************************************************************/
/*                         GetProjectionRef()                           */
/************************************************************************/

const char *ROIPACDataset::GetProjectionRef()
{
    return (pszProjection != NULL) ? pszProjection : "";
}

/************************************************************************/
/*                         ROIPACRasterBand()                           */
/************************************************************************/

ROIPACRasterBand::ROIPACRasterBand( GDALDataset *poDS, int nBand, void * fpRaw,
                                    vsi_l_offset nImgOffset, int nPixelOffset,
                                    int nLineOffset,
                                    GDALDataType eDataType, int bNativeOrder,
                                    int bIsVSIL, int bOwnsFP ) :
        RawRasterBand(poDS, nBand, fpRaw, nImgOffset, nPixelOffset,
                      nLineOffset, eDataType, bNativeOrder, bIsVSIL, bOwnsFP)
{
}

/************************************************************************/
/*                         GDALRegister_ENVI()                          */
/************************************************************************/

void GDALRegister_ROIPAC()
{
    GDALDriver  *poDriver;

    if (!GDAL_CHECK_VERSION("ROI_PAC"))
    {
        return;
    }

    if ( GDALGetDriverByName( "ROI_PAC" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "ROI_PAC" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "ROI_PAC raster" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#ROI_PAC" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = ROIPACDataset::Open;
        poDriver->pfnIdentify = ROIPACDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
