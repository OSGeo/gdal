/******************************************************************************
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

#include "gdal_frmts.h"
#include "ogr_spatialref.h"
#include "rawdataset.h"

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                             ROIPACDataset                            */
/* ==================================================================== */
/************************************************************************/

class ROIPACRasterBand;

class ROIPACDataset final: public RawDataset
{
    friend class ROIPACRasterBand;

    VSILFILE    *fpImage;
    VSILFILE    *fpRsc;

    char        *pszRscFilename;

    double      adfGeoTransform[6];
    bool        bValidGeoTransform;
    char        *pszProjection;

    CPL_DISALLOW_COPY_ASSIGN(ROIPACDataset)

  public:
    ROIPACDataset();
    ~ROIPACDataset() override;

    static GDALDataset *Open( GDALOpenInfo *poOpenInfo );
    static int          Identify( GDALOpenInfo *poOpenInfo );
    static GDALDataset *Create( const char *pszFilename,
                                int nXSize, int nYSize, int nBandsIn,
                                GDALDataType eType, char **papszOptions );

    void        FlushCache(bool bAtClosing) override;
    CPLErr              GetGeoTransform( double *padfTransform ) override;
    CPLErr      SetGeoTransform( double *padfTransform ) override;
    const char *_GetProjectionRef( void ) override;
    CPLErr      _SetProjection( const char *pszNewProjection ) override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }
    CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override {
        return OldSetProjectionFromSetSpatialRef(poSRS);
    }

    char      **GetFileList() override;
};

/************************************************************************/
/* ==================================================================== */
/*                           ROIPACRasterBand                           */
/* ==================================================================== */
/************************************************************************/

class ROIPACRasterBand final: public RawRasterBand
{
    CPL_DISALLOW_COPY_ASSIGN(ROIPACRasterBand)

    public:
                ROIPACRasterBand( GDALDataset *poDS, int nBand, VSILFILE *fpRaw,
                                  vsi_l_offset nImgOffset, int nPixelOffset,
                                  int nLineOffset,
                                  GDALDataType eDataType, int bNativeOrder );
};

/************************************************************************/
/*                           getRscFilename()                           */
/************************************************************************/

static CPLString getRscFilename( GDALOpenInfo *poOpenInfo )
{
    char **papszSiblingFiles = poOpenInfo->GetSiblingFiles();
    if ( papszSiblingFiles == nullptr )
    {
        const CPLString osRscFilename =
            CPLFormFilename( nullptr, poOpenInfo->pszFilename,
                             "rsc" );
        VSIStatBufL psRscStatBuf;
        if ( VSIStatL( osRscFilename, &psRscStatBuf ) != 0 )
        {
            return "";
        }
        return osRscFilename;
    }

    /* ------------------------------------------------------------ */
    /*      We need to tear apart the filename to form a .rsc       */
    /*      filename.                                               */
    /* ------------------------------------------------------------ */
    const CPLString osPath = CPLGetPath( poOpenInfo->pszFilename );
    const CPLString osName = CPLGetFilename( poOpenInfo->pszFilename );

    int iFile = CSLFindString( papszSiblingFiles,
                               CPLFormFilename( nullptr, osName, "rsc" ) );
    if( iFile >= 0 )
    {
        return CPLFormFilename( osPath,
                                papszSiblingFiles[iFile],
                                nullptr );
    }

    return "";
}

/************************************************************************/
/*                            ROIPACDataset()                           */
/************************************************************************/

ROIPACDataset::ROIPACDataset() :
    fpImage(nullptr),
    fpRsc(nullptr),
    pszRscFilename(nullptr),
    bValidGeoTransform(false),
    pszProjection(nullptr)
{
    adfGeoTransform[0] =  0.0;
    adfGeoTransform[1] =  1.0;
    adfGeoTransform[2] =  0.0;
    adfGeoTransform[3] =  0.0;
    adfGeoTransform[4] =  0.0;
    adfGeoTransform[5] =  1.0;
}

/************************************************************************/
/*                            ~ROIPACDataset()                          */
/************************************************************************/

ROIPACDataset::~ROIPACDataset()
{
    ROIPACDataset::FlushCache(true);
    if ( fpRsc != nullptr && VSIFCloseL( fpRsc ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO, "I/O error" );
    }
    if ( fpImage != nullptr && VSIFCloseL( fpImage ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO, "I/O error" );
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
    if ( !Identify(poOpenInfo) || poOpenInfo->fpL == nullptr )
    {
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Open the .rsc file                                              */
/* -------------------------------------------------------------------- */
    CPLString osRscFilename = getRscFilename( poOpenInfo );
    if ( osRscFilename.empty() )
    {
        return nullptr;
    }
    VSILFILE *fpRsc = nullptr;
    if ( poOpenInfo->eAccess == GA_Update )
    {
        fpRsc = VSIFOpenL( osRscFilename, "r+" );
    }
    else
    {
        fpRsc = VSIFOpenL( osRscFilename, "r" );
    }
    if ( fpRsc == nullptr )
    {
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Load the .rsc information.                                      */
/* -------------------------------------------------------------------- */
    char **papszRsc = nullptr;
    while ( true )
    {
        const char *pszLine = CPLReadLineL( fpRsc );
        if (pszLine == nullptr)
        {
            break;
        }

        char **papszTokens = CSLTokenizeString2( pszLine, " \t",
                                                 CSLT_STRIPLEADSPACES
                                                 | CSLT_STRIPENDSPACES
                                                 | CSLT_PRESERVEQUOTES
                                                 | CSLT_PRESERVEESCAPES );
        if ( papszTokens == nullptr
             || papszTokens[0] == nullptr || papszTokens[1] == nullptr )
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
    if( CSLFetchNameValue( papszRsc, "WIDTH" ) == nullptr
        || CSLFetchNameValue( papszRsc, "FILE_LENGTH" ) == nullptr )
    {
        CSLDestroy( papszRsc );
        CPL_IGNORE_RET_VAL(VSIFCloseL( fpRsc ));
        return nullptr;
    }
    const int nWidth = atoi( CSLFetchNameValue( papszRsc, "WIDTH" ) );
    const int nFileLength =
        atoi( CSLFetchNameValue( papszRsc, "FILE_LENGTH" ) );

    if (!GDALCheckDatasetDimensions(nWidth, nFileLength) )
    {
        CSLDestroy( papszRsc );
        CPL_IGNORE_RET_VAL(VSIFCloseL( fpRsc ));
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    ROIPACDataset *poDS = new ROIPACDataset();
    poDS->nRasterXSize = nWidth;
    poDS->nRasterYSize = nFileLength;
    poDS->eAccess = poOpenInfo->eAccess;
    poDS->fpRsc = fpRsc;
    poDS->pszRscFilename = CPLStrdup( osRscFilename.c_str() );
    poDS->fpImage = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    GDALDataType eDataType = GDT_Unknown;
    int nBands = 0;
    enum Interleave { UNKNOWN, LINE, PIXEL } eInterleave = UNKNOWN;
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
        return nullptr;
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
    else if ( strcmp( pszExtension, "flg" ) == 0 )
    {
        eDataType = GDT_Byte;
        nBands = 1;
        eInterleave = PIXEL;
    }
    else { /* Eeek */
        delete poDS;
        CSLDestroy( papszRsc );
        return nullptr;
    }

    int nPixelOffset = 0;
    int nLineOffset = 0;
    vsi_l_offset nBandOffset = 0;
    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
    bool bIntOverflow =  false;
    if (eInterleave == LINE)
    {
        nPixelOffset = nDTSize;
        if( nWidth > INT_MAX / (nPixelOffset * nBands) )
            bIntOverflow = true;
        else
        {
            nLineOffset = nPixelOffset * nWidth * nBands;
            nBandOffset = static_cast<vsi_l_offset>(nDTSize) * nWidth;
        }
    }
    else { /* PIXEL */
        nPixelOffset = nDTSize * nBands;
        if( nWidth > INT_MAX / nPixelOffset )
            bIntOverflow = true;
        else
        {
            nLineOffset = nPixelOffset * nWidth;
            nBandOffset = nDTSize;

            if( nBands > 1 )
            {
                // GDAL 2.0.[0-3] and 2.1.0  had a value of nLineOffset that was
                // equal to the theoretical nLineOffset multiplied by nBands.
                VSIFSeekL( poDS->fpImage, 0, SEEK_END );
                const GUIntBig nWrongFileSize =
                    nDTSize *
                    nWidth * (static_cast<GUIntBig>(nFileLength - 1) *
                            nBands * nBands + nBands);
                if( VSIFTellL( poDS->fpImage ) == nWrongFileSize )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                            "This file has been incorrectly generated by an older "
                            "GDAL version whose line offset computation was "
                            "erroneous.  Taking that into account, "
                            "but the file should be re-encoded ideally.");
                    nLineOffset = nLineOffset * nBands;
                }
            }
        }
    }

    if (bIntOverflow)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Int overflow occurred." );
        delete poDS;
        CSLDestroy( papszRsc );
        return nullptr;
    }

#ifdef CPL_LSB
    const bool bNativeOrder = true;
#else
    const bool bNativeOrder = false;
#endif
    poDS->nBands = nBands;
    for( int b = 0; b < nBands; b++ )
    {
        poDS->SetBand( b + 1,
                       new ROIPACRasterBand( poDS, b + 1, poDS->fpImage,
                                             nBandOffset * b,
                                             nPixelOffset, nLineOffset,
                                             eDataType, bNativeOrder ) );
    }

/* -------------------------------------------------------------------- */
/*      Interpret georeferencing, if present.                           */
/* -------------------------------------------------------------------- */
    if ( CSLFetchNameValue( papszRsc, "X_FIRST" ) != nullptr
         && CSLFetchNameValue( papszRsc, "X_STEP" ) != nullptr
         && CSLFetchNameValue( papszRsc, "Y_FIRST" ) != nullptr
         && CSLFetchNameValue( papszRsc, "Y_STEP" ) != nullptr )
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
    if ( CSLFetchNameValue( papszRsc, "PROJECTION" ) != nullptr )
    {
        /* ------------------------------------------------------------ */
        /* In ROI_PAC, images are georeferenced either with lat/long or */
        /* UTM projection. However, using UTM projection is dangerous   */
        /* because there is no North/South field, or use of latitude    */
        /* bands!                                                       */
        /* ------------------------------------------------------------ */
        OGRSpatialReference oSRS;
        if ( strcmp( CSLFetchNameValue( papszRsc, "PROJECTION" ), "LL" ) == 0 )
        {
            if ( CSLFetchNameValue( papszRsc, "DATUM" ) != nullptr )
            {
                oSRS.SetWellKnownGeogCS( CSLFetchNameValue( papszRsc,
                                                            "DATUM" ) );
            }
            else {
                oSRS.SetWellKnownGeogCS( "WGS84" );
            }
        }
        else if( STARTS_WITH(CSLFetchNameValue( papszRsc, "PROJECTION" ),
                             "UTM") )
        {
            const char *pszZone = CSLFetchNameValue( papszRsc,
                                                     "PROJECTION" ) + 3;
            oSRS.SetUTM( atoi( pszZone ), TRUE ); /* FIXME: north/south? */
            if ( CSLFetchNameValue( papszRsc, "DATUM" ) != nullptr )
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
    if ( CSLFetchNameValue( papszRsc, "Z_OFFSET" ) != nullptr )
    {
        const double dfOffset =
            strtod( CSLFetchNameValue( papszRsc, "Z_OFFSET" ), nullptr);
        for( int b = 1; b <= nBands; b++ )
        {
            GDALRasterBand *poBand = poDS->GetRasterBand(b);
            poBand->SetOffset( dfOffset );
        }
    }
    if ( CSLFetchNameValue( papszRsc, "Z_SCALE" ) != nullptr )
    {
        const double dfScale =
            strtod( CSLFetchNameValue( papszRsc, "Z_SCALE" ), nullptr);
        for( int b = 1; b <= nBands; b++ )
        {
            GDALRasterBand *poBand = poDS->GetRasterBand(b);
            poBand->SetScale( dfScale );
        }
    }

/* -------------------------------------------------------------------- */
/*      Set all the other header metadata into the ROI_PAC domain       */
/* -------------------------------------------------------------------- */
    for( int i = 0; papszRsc != nullptr && papszRsc[i] != nullptr; i++ )
    {
        char **papszTokens = CSLTokenizeString2( papszRsc[i],
                                                 "=",
                                                 CSLT_STRIPLEADSPACES
                                                 | CSLT_STRIPENDSPACES);
        if ( CSLCount(papszTokens) < 2
              || strcmp( papszTokens[0], "WIDTH" ) == 0
              || strcmp( papszTokens[0], "FILE_LENGTH" ) == 0
              || strcmp( papszTokens[0], "X_FIRST" ) == 0
              || strcmp( papszTokens[0], "X_STEP" ) == 0
              || strcmp( papszTokens[0], "Y_FIRST" ) == 0
              || strcmp( papszTokens[0], "Y_STEP" ) == 0
              || strcmp( papszTokens[0], "PROJECTION" ) == 0
              || strcmp( papszTokens[0], "DATUM" ) == 0
              || strcmp( papszTokens[0], "Z_OFFSET" ) == 0
              || strcmp( papszTokens[0], "Z_SCALE" ) == 0 )
        {
            CSLDestroy( papszTokens );
            continue;
        }
        poDS->SetMetadataItem( papszTokens[0], papszTokens[1], "ROI_PAC" );
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

    return poDS;
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
    const char *pszExtension = CPLGetExtension( poOpenInfo->pszFilename );
    if ( strcmp( pszExtension, "raw" ) == 0 )
    {
        /* Since gdal do not read natively CInt8, more work is needed
         * to read raw files */
        return false;
    }
    const bool bExtensionIsValid =
        strcmp( pszExtension, "int" ) == 0
        || strcmp( pszExtension, "slc" ) == 0
        || strcmp( pszExtension, "amp" ) == 0
        || strcmp( pszExtension, "cor" ) == 0
        || strcmp( pszExtension, "hgt" ) == 0
        || strcmp( pszExtension, "unw" ) == 0
        || strcmp( pszExtension, "msk" ) == 0
        || strcmp( pszExtension, "trans" ) == 0
        || strcmp( pszExtension, "dem" ) == 0
        || strcmp( pszExtension, "flg" ) == 0;
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
                                    int nXSize, int nYSize, int nBandsIn,
                                    GDALDataType eType,
                                    char ** /* papszOptions */ )
{
/* -------------------------------------------------------------------- */
/*      Verify input options.                                           */
/* -------------------------------------------------------------------- */
    const char *pszExtension = CPLGetExtension(pszFilename);
    if ( strcmp( pszExtension, "int" ) == 0
                || strcmp( pszExtension, "slc" ) == 0 )
    {
        if ( nBandsIn != 1 || eType != GDT_CFloat32 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Attempt to create ROI_PAC %s dataset with an illegal "
                      "number of bands (%d) and/or data type (%s).",
                      pszExtension, nBandsIn, GDALGetDataTypeName(eType) );
            return nullptr;
        }
    }
    else if ( strcmp( pszExtension, "amp" ) == 0 )
    {
        if ( nBandsIn != 2 || eType != GDT_Float32 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Attempt to create ROI_PAC %s dataset with an illegal "
                      "number of bands (%d) and/or data type (%s).",
                      pszExtension, nBandsIn, GDALGetDataTypeName(eType) );
            return nullptr;
        }
    }
    else if ( strcmp( pszExtension, "cor" ) == 0
                || strcmp( pszExtension, "hgt" ) == 0
                || strcmp( pszExtension, "unw" ) == 0
                || strcmp( pszExtension, "msk" ) == 0
                || strcmp( pszExtension, "trans" ) == 0 )
    {
        if ( nBandsIn != 2 || eType != GDT_Float32 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Attempt to create ROI_PAC %s dataset with an illegal "
                          "number of bands (%d) and/or data type (%s).",
                      pszExtension, nBandsIn, GDALGetDataTypeName(eType) );
            return nullptr;
        }
    }
    else if ( strcmp( pszExtension, "dem" ) == 0 )
    {
        if ( nBandsIn != 1 || eType != GDT_Int16 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Attempt to create ROI_PAC %s dataset with an illegal "
                      "number of bands (%d) and/or data type (%s).",
                      pszExtension, nBandsIn, GDALGetDataTypeName(eType) );
            return nullptr;
        }
    }
    else if ( strcmp( pszExtension, "flg" ) == 0 )
    {
        if ( nBandsIn != 1 || eType != GDT_Byte )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Attempt to create ROI_PAC %s dataset with an illegal "
                      "number of bands (%d) and/or data type (%s).",
                      pszExtension, nBandsIn, GDALGetDataTypeName(eType) );
            return nullptr;
        }
    }
    else { /* Eeek */
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to create ROI_PAC dataset with an unknown type (%s)",
                  pszExtension );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Try to create the file.                                         */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL( pszFilename, "wb" );
    if( fp == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file `%s' failed.",
                  pszFilename );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Just write out a couple of bytes to establish the binary        */
/*      file, and then close it.                                        */
/* -------------------------------------------------------------------- */
    CPL_IGNORE_RET_VAL(VSIFWriteL( "\0\0", 2, 1, fp ));
    CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));

/* -------------------------------------------------------------------- */
/*      Open the RSC file.                                              */
/* -------------------------------------------------------------------- */
    const char *pszRSCFilename = CPLFormFilename( nullptr, pszFilename, "rsc" );
    fp = VSIFOpenL( pszRSCFilename, "wt" );
    if( fp == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file `%s' failed.",
                  pszRSCFilename );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Write out the header.                                           */
/* -------------------------------------------------------------------- */
    CPL_IGNORE_RET_VAL(VSIFPrintfL( fp, "%-40s %d\n", "WIDTH", nXSize ));
    CPL_IGNORE_RET_VAL(VSIFPrintfL( fp, "%-40s %d\n", "FILE_LENGTH", nYSize ));
    CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));

    return reinterpret_cast<GDALDataset *>(
        GDALOpen( pszFilename, GA_Update ) );
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void ROIPACDataset::FlushCache(bool bAtClosing)
{
    RawDataset::FlushCache(bAtClosing);

    GDALRasterBand *band = (GetRasterCount() > 0) ? GetRasterBand(1) : nullptr;

    if ( eAccess == GA_ReadOnly || band == nullptr )
        return;

    // If opening an existing file in Update mode (i.e. "r+") we need to make
    // sure any existing content is cleared, otherwise the file may contain
    // trailing content from the previous write.
    CPL_IGNORE_RET_VAL(VSIFTruncateL( fpRsc, 0 ));

    CPL_IGNORE_RET_VAL(VSIFSeekL( fpRsc, 0, SEEK_SET ));
/* -------------------------------------------------------------------- */
/*      Rewrite out the header.                                         */
/* -------------------------------------------------------------------- */
/* -------------------------------------------------------------------- */
/*      Raster dimensions.                                              */
/* -------------------------------------------------------------------- */
    CPL_IGNORE_RET_VAL(
        VSIFPrintfL( fpRsc, "%-40s %d\n", "WIDTH", nRasterXSize ));
    CPL_IGNORE_RET_VAL(
        VSIFPrintfL( fpRsc, "%-40s %d\n", "FILE_LENGTH", nRasterYSize ));

/* -------------------------------------------------------------------- */
/*      Georeferencing.                                                 */
/* -------------------------------------------------------------------- */
    if ( pszProjection != nullptr )
    {
        OGRSpatialReference oSRS;
        if( oSRS.importFromWkt( pszProjection ) == OGRERR_NONE )
        {
            int bNorth = FALSE;
            int iUTMZone = oSRS.GetUTMZone( &bNorth );
            if ( iUTMZone != 0 )
            {
                CPL_IGNORE_RET_VAL(
                    VSIFPrintfL( fpRsc, "%-40s %s%d\n", "PROJECTION", "UTM",
                                 iUTMZone ) );
            }
            else if ( oSRS.IsGeographic() )
            {
                CPL_IGNORE_RET_VAL(
                    VSIFPrintfL( fpRsc, "%-40s %s\n", "PROJECTION", "LL" ) );
            }
            else
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "ROI_PAC format only support Latitude/Longitude and "
                          "UTM projections, discarding projection.");
            }

            if ( oSRS.GetAttrValue( "DATUM" ) != nullptr )
            {
                if ( strcmp( oSRS.GetAttrValue( "DATUM" ), "WGS_1984" ) == 0 )
                {
                    CPL_IGNORE_RET_VAL(
                        VSIFPrintfL( fpRsc, "%-40s %s\n", "DATUM", "WGS84" ) );
                }
                else
                {
                    CPLError( CE_Warning, CPLE_AppDefined,
                              "Datum \"%s\" probably not supported in the "
                              "ROI_PAC format, saving it anyway",
                                  oSRS.GetAttrValue( "DATUM" ) );
                    CPL_IGNORE_RET_VAL(
                        VSIFPrintfL( fpRsc, "%-40s %s\n", "DATUM",
                                     oSRS.GetAttrValue( "DATUM" ) ) );
                }
            }
            if ( oSRS.GetAttrValue( "UNIT" ) != nullptr )
            {
                CPL_IGNORE_RET_VAL(
                    VSIFPrintfL( fpRsc, "%-40s %s\n", "X_UNIT",
                                 oSRS.GetAttrValue( "UNIT" ) ));
                CPL_IGNORE_RET_VAL(
                    VSIFPrintfL( fpRsc, "%-40s %s\n", "Y_UNIT",
                                 oSRS.GetAttrValue( "UNIT" ) ));
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
            CPL_IGNORE_RET_VAL(
                VSIFPrintfL( fpRsc, "%-40s %.16g\n",
                             "X_FIRST", adfGeoTransform[0] ));
            CPL_IGNORE_RET_VAL(
                VSIFPrintfL( fpRsc, "%-40s %.16g\n",
                             "X_STEP", adfGeoTransform[1] ));
            CPL_IGNORE_RET_VAL(VSIFPrintfL( fpRsc, "%-40s %.16g\n",
                                            "Y_FIRST", adfGeoTransform[3] ));
            CPL_IGNORE_RET_VAL(
                VSIFPrintfL( fpRsc, "%-40s %.16g\n",
                             "Y_STEP", adfGeoTransform[5] ));
            CPL_IGNORE_RET_VAL(
                VSIFPrintfL( fpRsc, "%-40s %.16g\n",
                             "Z_OFFSET", band->GetOffset(nullptr) ));
            CPL_IGNORE_RET_VAL(
                VSIFPrintfL( fpRsc, "%-40s %.16g\n",
                             "Z_SCALE", band->GetScale(nullptr) ));
        }
    }

/* -------------------------------------------------------------------- */
/*      Metadata stored in the ROI_PAC domain.                          */
/* -------------------------------------------------------------------- */
    char** papszROIPACMetadata = GetMetadata( "ROI_PAC" );
    for( int i = 0; i < CSLCount( papszROIPACMetadata ); i++ )
    {
        /* Get the tokens from the metadata item */
        char **papszTokens = CSLTokenizeString2( papszROIPACMetadata[i],
                                                 "=",
                                                 CSLT_STRIPLEADSPACES
                                                 | CSLT_STRIPENDSPACES);
        if ( CSLCount( papszTokens ) != 2 )
        {
            CPLDebug( "ROI_PAC",
                      "Line of header file could not be split at = "
                      "into two elements: %s",
                      papszROIPACMetadata[i] );
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
        CPL_IGNORE_RET_VAL(
            VSIFPrintfL( fpRsc, "%-40s %s\n",
                         papszTokens[0], papszTokens[1] ));
        CSLDestroy( papszTokens );
    }
}

/************************************************************************/
/*                         GetGeoTransform()                            */
/************************************************************************/

CPLErr ROIPACDataset::GetGeoTransform( double *padfTransform )
{
    memcpy( padfTransform, adfGeoTransform, sizeof(adfGeoTransform) );
    return bValidGeoTransform ? CE_None : CE_Failure;
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

const char *ROIPACDataset::_GetProjectionRef( void )
{
    return pszProjection != nullptr ? pszProjection : "";
}

/************************************************************************/
/*                          SetProjection()                             */
/************************************************************************/

CPLErr ROIPACDataset::_SetProjection( const char *pszNewProjection )

{
    CPLFree( pszProjection );
    pszProjection = (pszNewProjection) ? CPLStrdup( pszNewProjection ) : nullptr;
    return CE_None;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **ROIPACDataset::GetFileList()
{
    // Main data file, etc.
    char **papszFileList = RawDataset::GetFileList();

    // RSC file.
    papszFileList = CSLAddString( papszFileList, pszRscFilename );

    return papszFileList;
}

/************************************************************************/
/*                         ROIPACRasterBand()                           */
/************************************************************************/

ROIPACRasterBand::ROIPACRasterBand( GDALDataset *poDSIn, int nBandIn, VSILFILE *fpRawIn,
                                    vsi_l_offset nImgOffsetIn, int nPixelOffsetIn,
                                    int nLineOffsetIn,
                                    GDALDataType eDataTypeIn, int bNativeOrderIn ) :
    RawRasterBand(poDSIn, nBandIn, fpRawIn, nImgOffsetIn, nPixelOffsetIn,
                  nLineOffsetIn, eDataTypeIn, bNativeOrderIn,
                  RawRasterBand::OwnFP::NO)
{}

/************************************************************************/
/*                        GDALRegister_ROIPAC()                         */
/************************************************************************/

void GDALRegister_ROIPAC()
{
    if( GDALGetDriverByName( "ROI_PAC" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "ROI_PAC" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "ROI_PAC raster" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                               "drivers/raster/roi_pac.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = ROIPACDataset::Open;
    poDriver->pfnIdentify = ROIPACDataset::Identify;
    poDriver->pfnCreate = ROIPACDataset::Create;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
