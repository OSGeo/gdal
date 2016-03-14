/******************************************************************************
 * $Id$
 *
 * Project:  ROI_PAC Raster Reader
 * Purpose:  Implementation of the ROI_PAC raster reader
 * Author:   Matthieu Volat (ISTerre), matthieu.volat@ujf-grenoble.fr
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
    
    char        *pszRscFilename;

    double      adfGeoTransform[6];
    bool        bValidGeoTransform;
    char        *pszProjection;

  public:
                ROIPACDataset( void );
                ~ROIPACDataset( void );
    
    static GDALDataset *Open( GDALOpenInfo *poOpenInfo );
    static int          Identify( GDALOpenInfo *poOpenInfo );
    static GDALDataset *Create( const char *pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char **papszOptions );

    virtual void        FlushCache( void );
    CPLErr              GetGeoTransform( double *padfTransform );
    virtual CPLErr      SetGeoTransform( double *padfTransform );
    const char         *GetProjectionRef( void );
    virtual CPLErr      SetProjection( const char *pszNewProjection );
    virtual char      **GetFileList( void );
};

/************************************************************************/
/* ==================================================================== */
/*                           ROIPACRasterBand                           */
/* ==================================================================== */
/************************************************************************/

class ROIPACRasterBand : public RawRasterBand
{
    public:
                ROIPACRasterBand( GDALDataset *poDS, int nBand, void *fpRaw,
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

ROIPACDataset::ROIPACDataset( void )
{
    fpImage = NULL;
    fpRsc = NULL;
    pszRscFilename = NULL;
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

ROIPACDataset::~ROIPACDataset( void )
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
    CPLFree( pszRscFilename );
    CPLFree( pszProjection );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *ROIPACDataset::Open( GDALOpenInfo *poOpenInfo )
{
/* -------------------------------------------------------------------- */
/*      Confirm that the header is compatible with a ROIPAC dataset.    */
/* -------------------------------------------------------------------- */
    if ( !Identify(poOpenInfo) )
    {
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
    VSILFILE *fpRsc;
    if ( poOpenInfo->eAccess == GA_Update )
    {
        fpRsc = VSIFOpenL( osRscFilename, "r+" );
    }
    else
    {
        fpRsc = VSIFOpenL( osRscFilename, "r" );
    }
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
    poDS->eAccess = poOpenInfo->eAccess;
    poDS->fpRsc = fpRsc;
    poDS->pszRscFilename = CPLStrdup( osRscFilename.c_str() );

/* -------------------------------------------------------------------- */
/*      Reopen file in update mode if necessary.                        */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        poDS->fpImage = VSIFOpenL( poOpenInfo->pszFilename, "rb+" );
    }
    else
    {
        poDS->fpImage = VSIFOpenL( poOpenInfo->pszFilename, "rb" );
    }
    if( poDS->fpImage == NULL )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to re-open %s within ROI_PAC driver.\n",
                  poOpenInfo->pszFilename );
        CSLDestroy( papszRsc );
        return NULL;
    }

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
        CSLDestroy( papszRsc );
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
        CSLDestroy( papszRsc );
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
/*      Set all the other header metadata into the ROI_PAC domain       */
/* -------------------------------------------------------------------- */
    for (int i = 0; i < CSLCount( papszRsc ); i++)
    {
        char **papszTokens;
        papszTokens = CSLTokenizeString2( papszRsc[i],
                                          "=",
                                          CSLT_STRIPLEADSPACES
                                            | CSLT_STRIPENDSPACES);
        if ( strcmp( papszTokens[0], "WIDTH" ) == 0
              || strcmp( papszTokens[0], "FILE_LENGTH" ) == 0
              || strcmp( papszTokens[0], "X_FIRST" ) == 0
              || strcmp( papszTokens[0], "X_STEP" ) == 0
              || strcmp( papszTokens[0], "Y_FIRST" ) == 0
              || strcmp( papszTokens[0], "Y_STEP" ) == 0
              || strcmp( papszTokens[0], "PROJECTION" ) == 0 
              || strcmp( papszTokens[0], "DATUM" ) == 0 )
        {
            CSLDestroy( papszTokens );
            continue;
        }
        poDS->SetMetadataItem(papszTokens[0], papszTokens[1], "ROI_PAC");
        CSLDestroy( papszTokens );
    }

/* -------------------------------------------------------------------- */
/*      Free papszRsc                                                   */
/* -------------------------------------------------------------------- */
    CSLDestroy( papszRsc );

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

int ROIPACDataset::Identify( GDALOpenInfo *poOpenInfo )
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
/*                              Create()                                */
/************************************************************************/

GDALDataset *ROIPACDataset::Create( const char *pszFilename,
                                    int nXSize, int nYSize, int nBands,
                                    GDALDataType eType,
                                    char **papszOptions )
{
/* -------------------------------------------------------------------- */
/*      Verify input options.                                           */
/* -------------------------------------------------------------------- */
    const char *pszExtension = CPLGetExtension(pszFilename);
    if ( strcmp( pszExtension, "int" ) == 0
                || strcmp( pszExtension, "slc" ) == 0 )
    {
        if ( nBands != 1 || eType != GDT_CFloat32 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Attempt to create ROI_PAC %s dataset with an illegal\n"
                          "number of bands (%d) and/or data type (%s).\n",
                      pszExtension, nBands, GDALGetDataTypeName(eType) );
            return NULL;
        }
    }
    else if ( strcmp( pszExtension, "amp" ) == 0 )
    {
        if ( nBands != 2 || eType != GDT_Float32 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Attempt to create ROI_PAC %s dataset with an illegal\n"
                          "number of bands (%d) and/or data type (%s).\n",
                      pszExtension, nBands, GDALGetDataTypeName(eType) );
            return NULL;
        }
    }
    else if ( strcmp( pszExtension, "cor" ) == 0 
                || strcmp( pszExtension, "hgt" ) == 0 
                || strcmp( pszExtension, "unw" ) == 0 
                || strcmp( pszExtension, "msk" ) == 0 
                || strcmp( pszExtension, "trans" ) == 0 )
    {
        if ( nBands != 2 || eType != GDT_Float32 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Attempt to create ROI_PAC %s dataset with an illegal\n"
                          "number of bands (%d) and/or data type (%s).\n",
                      pszExtension, nBands, GDALGetDataTypeName(eType) );
            return NULL;
        }
    }
    else if ( strcmp( pszExtension, "dem" ) == 0 )
    {
        if ( nBands != 1 || eType != GDT_Int16 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Attempt to create ROI_PAC %s dataset with an illegal\n"
                          "number of bands (%d) and/or data type (%s).\n",
                      pszExtension, nBands, GDALGetDataTypeName(eType) );
            return NULL;
        }
    }
    else { /* Eeek */
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to create ROI_PAC dataset with an unknown type (%s)\n",
                  pszExtension );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try to create the file.                                         */
/* -------------------------------------------------------------------- */
    VSILFILE *fp;
    fp = VSIFOpenL( pszFilename, "wb" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file `%s' failed.\n",
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Just write out a couple of bytes to establish the binary        */
/*      file, and then close it.                                        */
/* -------------------------------------------------------------------- */
    VSIFWriteL( (void *) "\0\0", 2, 1, fp );
    VSIFCloseL( fp );

/* -------------------------------------------------------------------- */
/*      Open the RSC file.                                              */
/* -------------------------------------------------------------------- */
    const char  *pszRSCFilename;
    pszRSCFilename = CPLFormFilename( NULL, pszFilename, "rsc" );
    fp = VSIFOpenL( pszRSCFilename, "wt" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file `%s' failed.\n",
                  pszRSCFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Write out the header.                                           */
/* -------------------------------------------------------------------- */
    VSIFPrintfL( fp, "%-40s %d\n", "WIDTH", nXSize );
    VSIFPrintfL( fp, "%-40s %d\n", "FILE_LENGTH", nYSize );
    VSIFCloseL( fp );

    return (GDALDataset *) GDALOpen( pszFilename, GA_Update );
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void ROIPACDataset::FlushCache( void )
{
    RawDataset::FlushCache();

    GDALRasterBand *band = (GetRasterCount() > 0) ? GetRasterBand(1) : NULL;

    if ( eAccess == GA_ReadOnly || band == NULL )
        return;

    // If opening an existing file in Update mode (i.e. "r+") we need to make
    // sure any existing content is cleared, otherwise the file may contain
    // trailing content from the previous write.
    VSIFTruncateL( fpRsc, 0 );

    VSIFSeekL( fpRsc, 0, SEEK_SET );
/* -------------------------------------------------------------------- */
/*      Rewrite out the header.                                         */
/* -------------------------------------------------------------------- */
/* -------------------------------------------------------------------- */
/*      Raster dimensions.                                              */
/* -------------------------------------------------------------------- */
    VSIFPrintfL( fpRsc, "%-40s %d\n", "WIDTH", nRasterXSize );
    VSIFPrintfL( fpRsc, "%-40s %d\n", "FILE_LENGTH", nRasterYSize );

/* -------------------------------------------------------------------- */
/*      Georeferencing.                                                 */
/* -------------------------------------------------------------------- */
    if ( pszProjection != NULL )
    {
        char *pszProjectionTmp = pszProjection;
        OGRSpatialReference oSRS;
        if( oSRS.importFromWkt( &pszProjectionTmp ) == OGRERR_NONE )
        {
            int bNorth;
            int iUTMZone;

            iUTMZone = oSRS.GetUTMZone( &bNorth );
            if ( iUTMZone != 0 )
            {
                VSIFPrintfL( fpRsc, "%-40s %s%d\n", "PROJECTION", "UTM", iUTMZone );
            }
            else if ( oSRS.IsGeographic() )
            {
                VSIFPrintfL( fpRsc, "%-40s %s\n", "PROJECTION", "LL" );
            }
            else
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "ROI_PAC format only support Latitude/Longitude and "
                              "UTM projections, discarding projection.");
            }

            if ( oSRS.GetAttrValue( "DATUM" ) != NULL )
            {
                if ( strcmp( oSRS.GetAttrValue( "DATUM" ), "WGS_1984" ) == 0 )
                {
                    VSIFPrintfL( fpRsc, "%-40s %s\n", "DATUM", "WGS84" );
                }
                else
                {
                    CPLError( CE_Warning, CPLE_AppDefined,
                              "Datum \"%s\" probably not supported in the "
                                  "ROI_PAC format, saving it anyway",
                                  oSRS.GetAttrValue( "DATUM" ) );
                    VSIFPrintfL( fpRsc, "%-40s %s\n", "DATUM", oSRS.GetAttrValue( "DATUM" ) );
                }
            }
            if ( oSRS.GetAttrValue( "UNIT" ) != NULL )
            {
                VSIFPrintfL( fpRsc, "%-40s %s\n", "X_UNIT", oSRS.GetAttrValue( "UNIT" ) );
                VSIFPrintfL( fpRsc, "%-40s %s\n", "Y_UNIT", oSRS.GetAttrValue( "UNIT" ) );
            }
        }
    }
    if( bValidGeoTransform )
    {
        if ( adfGeoTransform[2] != 0 || adfGeoTransform[4] != 0 )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "ROI_PAC format do not support geotransform with "
                          "rotation, discarding info.");
        }
        else
        {
            VSIFPrintfL( fpRsc, "%-40s %.16g\n", "X_FIRST", adfGeoTransform[0] );
            VSIFPrintfL( fpRsc, "%-40s %.16g\n", "X_STEP", adfGeoTransform[1] );
            VSIFPrintfL( fpRsc, "%-40s %.16g\n", "Y_FIRST", adfGeoTransform[3] );
            VSIFPrintfL( fpRsc, "%-40s %.16g\n", "Y_STEP", adfGeoTransform[5] );
        }
    }

/* -------------------------------------------------------------------- */
/*      Metadata stored in the ROI_PAC domain.                          */
/* -------------------------------------------------------------------- */
    char** papszROIPACMetadata = GetMetadata( "ROI_PAC" );
    for (int i = 0; i < CSLCount( papszROIPACMetadata ); i++)
    {   
        char **papszTokens;

        /* Get the tokens from the metadata item */
        papszTokens = CSLTokenizeString2( papszROIPACMetadata[i], 
                                          "=", 
                                          CSLT_STRIPLEADSPACES 
                                            | CSLT_STRIPENDSPACES);
        if ( CSLCount( papszTokens ) != 2 )
        {   
            CPLDebug("ROI_PAC",
                     "Line of header file could not be split at = into two elements: %s",
                     papszROIPACMetadata[i]);
            CSLDestroy( papszTokens );
            continue;
        }

        /* Don't write it out if it is one of the bits of metadata that is
         * written out elsewhere in this routine */
        if ( strcmp( papszTokens[0], "WIDTH" ) == 0
              || strcmp( papszTokens[0], "FILE_LENGTH" ) == 0 )
        {   
            CSLDestroy( papszTokens );
            continue;
        }
        VSIFPrintfL( fpRsc, "%-40s %s\n", papszTokens[0], papszTokens[1] );
        CSLDestroy( papszTokens );
    }
}

/************************************************************************/
/*                         GetGeoTransform()                            */
/************************************************************************/

CPLErr ROIPACDataset::GetGeoTransform( double *padfTransform )
{
    memcpy( padfTransform, adfGeoTransform, sizeof(adfGeoTransform) );
    return (bValidGeoTransform) ? CE_None : CE_Failure;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr ROIPACDataset::SetGeoTransform( double *padfTransform )
{
    memcpy( adfGeoTransform, padfTransform, sizeof(adfGeoTransform) );
    bValidGeoTransform = true;
    return CE_None;
}

/************************************************************************/
/*                         GetProjectionRef()                           */
/************************************************************************/

const char *ROIPACDataset::GetProjectionRef( void )
{
    return (pszProjection != NULL) ? pszProjection : "";
}

/************************************************************************/
/*                          SetProjection()                             */
/************************************************************************/

CPLErr ROIPACDataset::SetProjection( const char *pszNewProjection )

{
    CPLFree( pszProjection );
    pszProjection = (pszNewProjection) ? CPLStrdup( pszNewProjection ) : NULL;
    return CE_None;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **ROIPACDataset::GetFileList()
{
    char **papszFileList = NULL;

    // Main data file, etc.  
    papszFileList = RawDataset::GetFileList();

    // RSC file. 
    papszFileList = CSLAddString( papszFileList, pszRscFilename );

    return papszFileList;
}

/************************************************************************/
/*                         ROIPACRasterBand()                           */
/************************************************************************/

ROIPACRasterBand::ROIPACRasterBand( GDALDataset *poDS, int nBand, void *fpRaw,
                                    vsi_l_offset nImgOffset, int nPixelOffset,
                                    int nLineOffset,
                                    GDALDataType eDataType, int bNativeOrder,
                                    int bIsVSIL, int bOwnsFP ) :
        RawRasterBand(poDS, nBand, fpRaw, nImgOffset, nPixelOffset,
                      nLineOffset, eDataType, bNativeOrder, bIsVSIL, bOwnsFP)
{
}

/************************************************************************/
/*                        GDALRegister_ROIPAC()                         */
/************************************************************************/

void GDALRegister_ROIPAC( void )
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
        poDriver->pfnCreate = ROIPACDataset::Create;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
