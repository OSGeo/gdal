/******************************************************************************
 * $Id$
 *
 * Project:  GTA read/write Driver
 * Purpose:  GDAL bindings over GTA library.
 * Author:   Martin Lambers, marlam@marlam.de
 *
 ******************************************************************************
 * Copyright (c) 2010, 2011, Martin Lambers <marlam@marlam.de>
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

/*
 * This driver supports reading and writing GTAs (Generic Tagged Arrays). See
 * http://www.nongnu.org/gta/ for details on this format.
 *
 * Supported Features:
 * - CreateCopy().
 * - GTA compression can be set.
 * - Raster data is updatable for uncompressed GTAs.
 * - All input/output is routed through the VSIF*L functions
 *   (GDAL_DCAP_VIRTUALIO is set to "YES").
 * - All kinds of metadata are supported (see tag list below).
 *
 * Limitations:
 * - Only uncompressed GTAs can be updated.
 * - Only raster data updates are possible; metadata cannot be changed.
 * - Color palettes are not supported.
 * - CInt16 is stored as gta::cfloat32, and CInt32 as gta::cfloat64.
 * - GDAL metadata is assumed to be in UTF-8 encoding, so that no conversion is
 *   necessary to store it in GTA tags. I'm not sure that this is correct, but
 *   since some metadata might not be representable in the local encoding (e.g.
 *   a chinese description in latin1), using UTF-8 seems reasonable.
 *
 * The following could be implemented, but currently is not:
 * - Allow metadata updates by using a special GDAL/METADATA_BUFFER tag that
 *   contains a number of spaces as a placeholder for additional metadata, so
 *   that the header size on disk can be kept constant.
 * - Implement Create().
 * - Implement AddBand() for uncompressed GTAs. But this would be inefficient:
 *   the old data would need to be copied to a temporary file, and then copied
 *   back while extending it with the new band.
 * - When strict conversion is requested, store CInt16 in 2 x gta::int16 and
 *   CInt32 in 2 x gta::int32, and mark these components with special flags so
 *   that this is reverted when opening the GTA.
 * - Support color palettes by storing the palette in special tags.
 *
 * This driver supports the following standard GTA tags:
 * DESCRIPTION
 * INTERPRETATION
 * NO_DATA_VALUE
 * MIN_VALUE
 * MAX_VALUE
 * UNIT
 *
 * Additionally, the following tags are used for GDAL-specific metadata:
 * GDAL/PROJECTION      (WKT)
 * GDAL/GEO_TRANSFORM   (6 doubles)
 * GDAL/OFFSET          (1 double)
 * GDAL/SCALE           (1 double)
 * GDAL/GCP_PROJECTION  (WKT)
 * GDAL/GCP_COUNT       (1 int > 0)
 * GDAL/GCP%d           (5 doubles)
 * GDAL/GCP%d_INFO      (String)
 * GDAL/CATEGORY_COUNT  (1 int > 0)
 * GDAL/CATEGORY%d      (String)
 * GDAL/META/DEFAULT/%s (String)
 * GDAL/META/RCP/%s     (String)
 */

#include <limits.h>
#include "cpl_port.h" // for snprintf for MSVC
#include <gta/gta.hpp>
#include "gdal_pam.h"

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_GTA(void);
CPL_C_END


/************************************************************************/
/* Helper functions                                                     */
/************************************************************************/

static void ScanDoubles( const char *pszString, double *padfDoubles, int nCount )

{
    char *pszRemainingString = (char *)pszString;
    for( int i = 0; i < nCount; i++ )
    {
        padfDoubles[i] = 0.0;   // fallback value
        padfDoubles[i] = CPLStrtod( pszRemainingString, &pszRemainingString );
    }
}

static CPLString PrintDoubles( const double *padfDoubles, int nCount )

{
    CPLString oString;
    for( int i = 0; i < nCount; i++ )
    {
        oString.FormatC( padfDoubles[i], "%.16g" );
        if( i < nCount - 1)
        {
            oString += ' ';
        }
    }
    return oString;
}

/************************************************************************/
/* ==================================================================== */
/* GTA custom IO class using GDAL's IO abstraction layer                */
/* ==================================================================== */
/************************************************************************/

class GTAIO : public gta::custom_io
{
  private:
    VSILFILE *fp;

  public:
    GTAIO( ) throw ()
        : fp( NULL )
    {
    }
    ~GTAIO( )
    {
        close( );
    }

    int open( const char *pszFilename, const char *pszMode )
    {
        fp = VSIFOpenL( pszFilename, pszMode );
        return ( fp == NULL ? -1 : 0 );
    }

    void close( )
    {
        if( fp != NULL )
        {
            VSIFCloseL( fp );
            fp = NULL;
        }
    }

    vsi_l_offset tell( )
    {
        return VSIFTellL( fp );
    }

    virtual size_t read(void *buffer, size_t size, bool *error) throw ()
    {
        size_t s;
        s = VSIFReadL( buffer, 1, size, fp );
        if( s != size )
        {
            errno = EIO;
            *error = true;
        }
        return size;
    }

    virtual size_t write(const void *buffer, size_t size, bool *error) throw ()
    {
        size_t s;
        s = VSIFWriteL( buffer, 1, size, fp );
        if( s != size )
        {
            errno = EIO;
            *error = true;
        }
        return size;
    }

    virtual bool seekable() throw ()
    {
        return true;
    }

    virtual void seek(intmax_t offset, int whence, bool *error) throw ()
    {
        int r;
        r = VSIFSeekL( fp, offset, whence );
        if( r != 0 )
        {
            errno = EIO;
            *error = true;
        }
    }
};

/************************************************************************/
/* ==================================================================== */
/*                              GTADataset                              */
/* ==================================================================== */
/************************************************************************/

class GTARasterBand;

class GTADataset : public GDALPamDataset
{
    friend class GTARasterBand;

  private:
    // GTA input/output via VSIF*L functions
    GTAIO       oGTAIO;
    // GTA information
    gta::header oHeader;
    vsi_l_offset DataOffset;
    // Metadata
    bool        bHaveGeoTransform;
    double      adfGeoTransform[6];
    int         nGCPs;
    char        *pszGCPProjection;
    GDAL_GCP    *pasGCPs;
    // Cached data block for block-based input/output
    int         nLastBlockXOff, nLastBlockYOff;
    void        *pBlock;

    // Block-based input/output of all bands at once. This is used
    // by the GTARasterBand input/output functions.
    CPLErr      ReadBlock( int, int );
    CPLErr      WriteBlock( );

  public:
                GTADataset();
                ~GTADataset();

    static GDALDataset *Open( GDALOpenInfo * );

    CPLErr      GetGeoTransform( double * padfTransform );
    CPLErr      SetGeoTransform( double * padfTransform );

    const char *GetProjectionRef( );
    CPLErr      SetProjection( const char *pszProjection );

    int         GetGCPCount( );
    const char *GetGCPProjection( );
    const GDAL_GCP *GetGCPs( );
    CPLErr      SetGCPs( int, const GDAL_GCP *, const char * );
};

/************************************************************************/
/* ==================================================================== */
/*                            GTARasterBand                             */
/* ==================================================================== */
/************************************************************************/

class GTARasterBand : public GDALPamRasterBand
{
    friend class GTADataset;
  private:
    // Size of the component represented by this band
    size_t      sComponentSize;
    // Offset of the component represented by this band inside a GTA element
    size_t      sComponentOffset;
    // StringList for category names
    char      **papszCategoryNames;
    // StringList for metadata
    char      **papszMetaData;

  public:
                GTARasterBand( GTADataset *, int );
                ~GTARasterBand( );

    CPLErr      IReadBlock( int, int, void * );
    CPLErr      IWriteBlock( int, int, void * );

    char      **GetCategoryNames( );
    CPLErr      SetCategoryNames( char ** );

    double      GetMinimum( int * );
    double      GetMaximum( int * );

    double      GetNoDataValue( int * );
    CPLErr      SetNoDataValue( double );
    double      GetOffset( int * );
    CPLErr      SetOffset( double );
    double      GetScale( int * );
    CPLErr      SetScale( double );
    const char *GetUnitType( );
    CPLErr      SetUnitType( const char * );
    GDALColorInterp GetColorInterpretation( );
    CPLErr      SetColorInterpretation( GDALColorInterp );
};

/************************************************************************/
/*                           GTARasterBand()                            */
/************************************************************************/

GTARasterBand::GTARasterBand( GTADataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;

    // Data type
    switch( poDS->oHeader.component_type( nBand-1 ) )
    {
    case gta::int8:
        eDataType = GDT_Byte;
        SetMetadataItem("PIXELTYPE", "SIGNEDBYTE", "IMAGE_STRUCTURE");
        break;
    case gta::uint8:
        eDataType = GDT_Byte;
        break;
    case gta::int16:
        eDataType = GDT_Int16;
        break;
    case gta::uint16:
        eDataType = GDT_UInt16;
        break;
    case gta::int32:
        eDataType = GDT_Int32;
        break;
    case gta::uint32:
        eDataType = GDT_UInt32;
        break;
    case gta::float32:
        eDataType = GDT_Float32;
        break;
    case gta::float64:
        eDataType = GDT_Float64;
        break;
    case gta::cfloat32:
        eDataType = GDT_CFloat32;
        break;
    case gta::cfloat64:
        eDataType = GDT_CFloat64;
        break;
    default:
        // cannot happen because we checked this in GTADataset::Open()
        break;
    }

    // Block size
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

    // Component information
    sComponentSize = poDS->oHeader.component_size( nBand-1 );
    sComponentOffset = 0;
    for( int i = 0; i < nBand-1; i++ )
    {
        sComponentOffset += poDS->oHeader.component_size( i );
    }

    // Metadata
    papszCategoryNames = NULL;
    papszMetaData = NULL;
    if( poDS->oHeader.component_taglist( nBand-1 ).get( "DESCRIPTION" ) )
    {
        SetDescription( poDS->oHeader.component_taglist( nBand-1 ).get( "DESCRIPTION" ) );
    }
    for( uintmax_t i = 0; i < poDS->oHeader.component_taglist( nBand-1 ).tags(); i++)
    {
        const char *pszTagName = poDS->oHeader.component_taglist( nBand-1 ).name( i );
        if( strncmp( pszTagName, "GDAL/META/", 10 ) == 0 )
        {
            const char *pDomainEnd = strchr( pszTagName + 10, '/' );
            if( pDomainEnd && pDomainEnd - (pszTagName + 10) > 0 )
            {
                char *pszDomain = (char *)VSIMalloc( pDomainEnd - (pszTagName + 10) + 1 );
                if( !pszDomain )
                {
                    continue;
                }
                int j;
                for( j = 0; j < pDomainEnd - (pszTagName + 10); j++ )
                {
                    pszDomain[j] = pszTagName[10 + j];
                }
                pszDomain[j] = '\0';
                const char *pszName = pszTagName + 10 + j + 1;
                const char *pszValue = poDS->oHeader.component_taglist( nBand-1 ).value( i );
                SetMetadataItem( pszName, pszValue,
                        strcmp( pszDomain, "DEFAULT" ) == 0 ? NULL : pszDomain );
                VSIFree( pszDomain );
            }
        }
    }
}

/************************************************************************/
/*                           ~GTARasterBand()                           */
/************************************************************************/

GTARasterBand::~GTARasterBand( )

{
    CSLDestroy( papszCategoryNames );
    CSLDestroy( papszMetaData );
}

/************************************************************************/
/*                             GetCategoryNames()                       */
/************************************************************************/

char **GTARasterBand::GetCategoryNames( )

{
    if( !papszCategoryNames )
    {
        GTADataset *poGDS = (GTADataset *) poDS;
        const char *pszCatCount = poGDS->oHeader.component_taglist( nBand-1 ).get( "GDAL/CATEGORY_COUNT" );
        int nCatCount = 0;
        if( pszCatCount )
        {
            nCatCount = atoi( pszCatCount );
        }
        if( nCatCount > 0 )
        {
            for( int i = 0; i < nCatCount; i++ )
            {
                const char *pszCatName = poGDS->oHeader.component_taglist( nBand-1 ).get(
                        CPLSPrintf( "GDAL/CATEGORY%d", i ) );
                papszCategoryNames = CSLAddString( papszCategoryNames, pszCatName ? pszCatName : "" );
            }
        }
    }
    return papszCategoryNames;
}

/************************************************************************/
/*                             SetCategoryName()                        */
/************************************************************************/

CPLErr GTARasterBand::SetCategoryNames( char ** )

{
    CPLError( CE_Warning, CPLE_NotSupported,
            "The GTA driver does not support metadata updates.\n" );
    return CE_Failure;
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double GTARasterBand::GetMinimum( int *pbSuccess )

{
    GTADataset *poGDS = (GTADataset *) poDS;
    const char *pszValue = poGDS->oHeader.component_taglist( nBand-1 ).get( "MIN_VALUE" );
    if( pszValue )
    {
        if( pbSuccess )
            *pbSuccess = true;
        return CPLAtof( pszValue );
    }
    else
    {
        return GDALRasterBand::GetMinimum( pbSuccess );
    }
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double GTARasterBand::GetMaximum( int *pbSuccess  )

{
    GTADataset *poGDS = (GTADataset *) poDS;
    const char *pszValue = poGDS->oHeader.component_taglist( nBand-1 ).get( "MAX_VALUE" );
    if( pszValue )
    {
        if( pbSuccess )
            *pbSuccess = true;
        return CPLAtof( pszValue );
    }
    else
    {
        return GDALRasterBand::GetMaximum( pbSuccess );
    }
}

/************************************************************************/
/*                             GetNoDataValue()                         */
/************************************************************************/

double GTARasterBand::GetNoDataValue( int *pbSuccess )

{
    GTADataset *poGDS = (GTADataset *) poDS;
    const char *pszValue = poGDS->oHeader.component_taglist( nBand-1 ).get( "NO_DATA_VALUE" );
    if( pszValue )
    {
        if( pbSuccess )
            *pbSuccess = true;
        return CPLAtof( pszValue );
    }
    else
    {
        return GDALRasterBand::GetNoDataValue( pbSuccess );
    }
}

/************************************************************************/
/*                             SetNoDataValue()                         */
/************************************************************************/

CPLErr GTARasterBand::SetNoDataValue( double )

{
    CPLError( CE_Warning, CPLE_NotSupported,
            "The GTA driver does not support metadata updates.\n" );
    return CE_Failure;
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

double GTARasterBand::GetOffset( int *pbSuccess )

{
    GTADataset *poGDS = (GTADataset *) poDS;
    const char *pszValue = poGDS->oHeader.component_taglist( nBand-1 ).get( "GDAL/OFFSET" );
    if( pszValue )
    {
        if( pbSuccess )
            *pbSuccess = true;
        return CPLAtof( pszValue );
    }
    else
    {
        return GDALRasterBand::GetOffset( pbSuccess );
    }
}

/************************************************************************/
/*                             SetOffset()                              */
/************************************************************************/

CPLErr GTARasterBand::SetOffset( double )

{
    CPLError( CE_Warning, CPLE_NotSupported,
            "The GTA driver does not support metadata updates.\n" );
    return CE_Failure;
}

/************************************************************************/
/*                             GetScale()                               */
/************************************************************************/

double GTARasterBand::GetScale( int *pbSuccess )

{
    GTADataset *poGDS = (GTADataset *) poDS;
    const char *pszValue = poGDS->oHeader.component_taglist( nBand-1 ).get( "GDAL/SCALE" );
    if( pszValue )
    {
        if( pbSuccess )
            *pbSuccess = true;
        return CPLAtof( pszValue );
    }
    else
    {
        return GDALRasterBand::GetScale( pbSuccess );
    }
}

/************************************************************************/
/*                             SetScale()                               */
/************************************************************************/

CPLErr GTARasterBand::SetScale( double )

{
    CPLError( CE_Warning, CPLE_NotSupported,
            "The GTA driver does not support metadata updates.\n" );
    return CE_Failure;
}

/************************************************************************/
/*                             GetUnitType()                            */
/************************************************************************/

const char *GTARasterBand::GetUnitType( )

{
    GTADataset *poGDS = (GTADataset *) poDS;
    const char *pszValue = poGDS->oHeader.component_taglist( nBand-1 ).get( "UNIT" );
    return pszValue ? pszValue : "";
}

/************************************************************************/
/*                             SetUnitType()                            */
/************************************************************************/

CPLErr GTARasterBand::SetUnitType( const char * )

{
    CPLError( CE_Warning, CPLE_NotSupported,
            "The GTA driver does not support metadata updates.\n" );
    return CE_Failure;
}

/************************************************************************/
/*                             GetColorInterpretation()                 */
/************************************************************************/

GDALColorInterp GTARasterBand::GetColorInterpretation( )

{
    GTADataset *poGDS = (GTADataset *) poDS;
    const char *pszColorInterpretation =
        poGDS->oHeader.component_taglist( nBand-1 ).get(
                "INTERPRETATION" );
    if( pszColorInterpretation )
    {
        if( EQUAL( pszColorInterpretation, "GRAY" ) )
            return GCI_GrayIndex ;
        else if ( EQUAL( pszColorInterpretation, "RED" ) )
            return GCI_RedBand ;
        else if ( EQUAL( pszColorInterpretation, "GREEN" ) )
            return GCI_GreenBand ;
        else if ( EQUAL( pszColorInterpretation, "BLUE" ) )
            return GCI_BlueBand ;
        else if ( EQUAL( pszColorInterpretation, "ALPHA" ) )
            return GCI_AlphaBand ;
        else if ( EQUAL( pszColorInterpretation, "HSL/H" ) )
            return GCI_HueBand ;
        else if ( EQUAL( pszColorInterpretation, "HSL/S" ) )
            return GCI_SaturationBand ;
        else if ( EQUAL( pszColorInterpretation, "HSL/L" ) )
            return GCI_LightnessBand ;
        else if ( EQUAL( pszColorInterpretation, "CMYK/C" ) )
            return GCI_CyanBand ;
        else if ( EQUAL( pszColorInterpretation, "CMYK/M" ) )
            return GCI_MagentaBand ;
        else if ( EQUAL( pszColorInterpretation, "CMYK/Y" ) )
            return GCI_YellowBand ;
        else if ( EQUAL( pszColorInterpretation, "CMYK/K" ) )
            return GCI_BlackBand ;
        else if ( EQUAL( pszColorInterpretation, "YCBCR/Y" ) )
            return GCI_YCbCr_YBand;
        else if ( EQUAL( pszColorInterpretation, "YCBCR/CB" ) )
            return GCI_YCbCr_CbBand;
        else if ( EQUAL( pszColorInterpretation, "YCBCR/CR" ) )
            return GCI_YCbCr_CrBand;
    }
    return GCI_Undefined;
}

/************************************************************************/
/*                             SetColorInterpretation()                 */
/************************************************************************/

CPLErr GTARasterBand::SetColorInterpretation( GDALColorInterp )

{
    CPLError( CE_Warning, CPLE_NotSupported,
            "The GTA driver does not support metadata updates.\n" );
    return CE_Failure;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GTARasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    GTADataset *poGDS = (GTADataset *) poDS;

    // Read and cache block containing all bands at once
    if( poGDS->ReadBlock( nBlockXOff, nBlockYOff ) != CE_None )
    {
        return CE_Failure;
    }

    char *pBlock = (char *)poGDS->pBlock;
    if( poGDS->oHeader.compression() != gta::none )
    {
        // pBlock contains the complete data set. Add the offset into the
        // requested block. This assumes that nBlockYSize == 1 and
        // nBlockXSize == nRasterXSize.
        pBlock += nBlockYOff * nBlockXSize * poGDS->oHeader.element_size();
    }

    // Copy the data for this band from the cached block
    for( int i = 0; i < nBlockXSize; i++ )
    {
        char *pSrc = pBlock + i * poGDS->oHeader.element_size() + sComponentOffset;
        char *pDst = (char *) pImage + i * sComponentSize;
        memcpy( (void *) pDst, (void *) pSrc, sComponentSize );
    }

    return CE_None;
}

/************************************************************************/
/*                             IWriteBlock()                            */
/************************************************************************/

CPLErr GTARasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    GTADataset *poGDS = (GTADataset *) poDS;

    if( poGDS->oHeader.compression() != gta::none )
    {
        CPLError( CE_Warning, CPLE_NotSupported,
                "The GTA driver cannot update compressed GTAs.\n" );
        return CE_Failure;
    }

    // Read and cache block containing all bands at once
    if( poGDS->ReadBlock( nBlockXOff, nBlockYOff ) != CE_None )
    {
        return CE_Failure;
    }
    char *pBlock = (char *)poGDS->pBlock;

    // Copy the data for this band into the cached block
    for( int i = 0; i < nBlockXSize; i++ )
    {
        char *pSrc = (char *) pImage + i * sComponentSize;
        char *pDst = pBlock + i * poGDS->oHeader.element_size() + sComponentOffset;
        memcpy( (void *) pDst, (void *) pSrc, sComponentSize );
    }

    // Write the block that conatins all bands at once
    if( poGDS->WriteBlock( ) != CE_None )
    {
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                              GTADataset                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            GTADataset()                              */
/************************************************************************/

GTADataset::GTADataset()

{
    // Initialize Metadata
    bHaveGeoTransform = false;
    nGCPs = 0;
    pszGCPProjection = NULL;
    pasGCPs = NULL;
    // Initialize block-based input/output
    nLastBlockXOff = -1;
    nLastBlockYOff = -1;
    pBlock = NULL;
}

/************************************************************************/
/*                            ~GTADataset()                             */
/************************************************************************/

GTADataset::~GTADataset()

{
    FlushCache();
    VSIFree( pszGCPProjection );
    for( int i = 0; i < nGCPs; i++ )
    {
        VSIFree( pasGCPs[i].pszId );
        VSIFree( pasGCPs[i].pszInfo );
    }
    VSIFree( pasGCPs );
    VSIFree( pBlock );
}

/************************************************************************/
/*                             ReadBlock()                              */
/************************************************************************/

CPLErr GTADataset::ReadBlock( int nBlockXOff, int nBlockYOff )

{
    /* Compressed data sets must be read into memory completely.
     * Uncompressed data sets are read block-wise. */

    if( oHeader.compression() != gta::none )
    {
        if( pBlock == NULL )
        {
            if( oHeader.data_size() > (size_t)(-1)
                    || ( pBlock = VSIMalloc( oHeader.data_size() ) ) == NULL )
            {
                CPLError( CE_Failure, CPLE_OutOfMemory,
                        "Cannot allocate buffer for the complete data set.\n"
                        "Try to uncompress the data set to allow block-wise "
                        "reading.\n" );
                return CE_Failure;
            }

            try
            {
                oHeader.read_data( oGTAIO, pBlock );
            }
            catch( gta::exception &e )
            {
                CPLError( CE_Failure, CPLE_FileIO, "GTA error: %s\n", e.what() );
                return CE_Failure;
            }
        }
    }
    else
    {
        // This has to be the same as in the RasterBand constructor!
        int nBlockXSize = GetRasterXSize();
        int nBlockYSize = 1;

        if( nLastBlockXOff == nBlockXOff && nLastBlockYOff == nBlockYOff )
            return CE_None;

        if( pBlock == NULL )
        {
            pBlock = VSIMalloc2( oHeader.element_size(), nBlockXSize );
            if( pBlock == NULL )
            {
                CPLError( CE_Failure, CPLE_OutOfMemory,
                        "Cannot allocate scanline buffer" );
                return CE_Failure;
            }
        }

        try
        {
            uintmax_t lo[2] = { (uintmax_t)nBlockXOff * nBlockXSize, (uintmax_t)nBlockYOff * nBlockYSize};
            uintmax_t hi[2] = { lo[0] + nBlockXSize - 1, lo[1] + nBlockYSize - 1 };
            oHeader.read_block( oGTAIO, DataOffset, lo, hi, pBlock );
        }
        catch( gta::exception &e )
        {
            CPLError( CE_Failure, CPLE_FileIO, "GTA error: %s\n", e.what() );
            return CE_Failure;
        }

        nLastBlockXOff = nBlockXOff;
        nLastBlockYOff = nBlockYOff;
    }
    return CE_None;
}

/************************************************************************/
/*                             WriteBlock()                             */
/************************************************************************/

CPLErr GTADataset::WriteBlock( )

{
    // This has to be the same as in the RasterBand constructor!
    int nBlockXSize = GetRasterXSize();
    int nBlockYSize = 1;

    // Write the block (nLastBlockXOff, nLastBlockYOff) stored in pBlock.
    try
    {
        uintmax_t lo[2] = { (uintmax_t)nLastBlockXOff * nBlockXSize, (uintmax_t)nLastBlockYOff * nBlockYSize};
        uintmax_t hi[2] = { lo[0] + nBlockXSize - 1, lo[1] + nBlockYSize - 1 };
        oHeader.write_block( oGTAIO, DataOffset, lo, hi, pBlock );
    }
    catch( gta::exception &e )
    {
        CPLError( CE_Failure, CPLE_FileIO, "GTA error: %s\n", e.what() );
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GTADataset::GetGeoTransform( double * padfTransform )

{
    if( bHaveGeoTransform )
    {
        memcpy( padfTransform, adfGeoTransform, 6*sizeof(double) );
        return CE_None;
    }
    else
    {
        return CE_Failure;
    }
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr GTADataset::SetGeoTransform( double * )

{
    CPLError( CE_Warning, CPLE_NotSupported,
            "The GTA driver does not support metadata updates.\n" );
    return CE_Failure;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *GTADataset::GetProjectionRef()

{
    const char *p = oHeader.global_taglist().get("GDAL/PROJECTION");
    return ( p ? p : "" );
}

/************************************************************************/
/*                          SetProjection()                             */
/************************************************************************/

CPLErr GTADataset::SetProjection( const char * )

{
    CPLError( CE_Warning, CPLE_NotSupported,
            "The GTA driver does not support metadata updates.\n" );
    return CE_Failure;
}

/************************************************************************/
/*                          GetGCPCount()                               */
/************************************************************************/

int GTADataset::GetGCPCount( )

{
    return nGCPs;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char * GTADataset::GetGCPProjection( )

{
    return pszGCPProjection ? pszGCPProjection : "";
}

/************************************************************************/
/*                          GetGCPs()                                   */
/************************************************************************/

const GDAL_GCP * GTADataset::GetGCPs( )

{
    return pasGCPs;
}

/************************************************************************/
/*                          SetGCPs()                                   */
/************************************************************************/

CPLErr GTADataset::SetGCPs( int, const GDAL_GCP *, const char * )

{
    CPLError( CE_Warning, CPLE_NotSupported,
            "The GTA driver does not support metadata updates.\n" );
    return CE_Failure;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GTADataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes < 5 )
        return NULL;

    if( !EQUALN((char *)poOpenInfo->pabyHeader,"GTA",3) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GTADataset  *poDS;

    poDS = new GTADataset();

    if( poDS->oGTAIO.open( poOpenInfo->pszFilename,
            poOpenInfo->eAccess == GA_Update ? "r+" : "r" ) != 0 )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, "Cannot open file.\n" );
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */

    try
    {
        poDS->oHeader.read_from( poDS->oGTAIO );
    }
    catch( gta::exception &e )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, "GTA error: %s\n", e.what() );
        delete poDS;
        return NULL;
    }
    poDS->DataOffset = poDS->oGTAIO.tell();
    poDS->eAccess = poOpenInfo->eAccess;

    if( poDS->oHeader.compression() != gta::none
            && poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The GTA driver does not support update access to compressed "
                  "data sets.\nUncompress the data set first.\n" );
        delete poDS;
        return NULL;
    }

    if( poDS->oHeader.dimensions() != 2 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                "The GTA driver does not support GTAs with %s than 2 "
                "dimensions.\n",
                poDS->oHeader.dimensions() < 2 ? "less" : "more" );
        delete poDS;
        return NULL;
    }

    // We know the dimensions are > 0 (guaranteed by libgta), but they may be
    // unrepresentable in GDAL.
    if( poDS->oHeader.dimension_size(0) > INT_MAX
            || poDS->oHeader.dimension_size(1) > INT_MAX )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                "The GTA driver does not support the size of this data set.\n" );
        delete poDS;
        return NULL;
    }
    poDS->nRasterXSize = poDS->oHeader.dimension_size(0);
    poDS->nRasterYSize = poDS->oHeader.dimension_size(1);

    // Check the number of bands (called components in GTA)
    if( poDS->oHeader.components() > INT_MAX-1
            || poDS->oHeader.element_size() > ((size_t)-1) )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                "The GTA driver does not support the number or size of bands "
                "in this data set.\n" );
        delete poDS;
        return NULL;
    }
    poDS->nBands = poDS->oHeader.components();

    // Check the data types (called component types in GTA)
    for( int iBand = 0; iBand < poDS->nBands; iBand++ )
    {
        if( poDS->oHeader.component_type(iBand) != gta::uint8
                && poDS->oHeader.component_type(iBand) != gta::int8
                && poDS->oHeader.component_type(iBand) != gta::uint16
                && poDS->oHeader.component_type(iBand) != gta::int16
                && poDS->oHeader.component_type(iBand) != gta::uint32
                && poDS->oHeader.component_type(iBand) != gta::int32
                && poDS->oHeader.component_type(iBand) != gta::float32
                && poDS->oHeader.component_type(iBand) != gta::float64
                && poDS->oHeader.component_type(iBand) != gta::cfloat32
                && poDS->oHeader.component_type(iBand) != gta::cfloat64 )
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                    "The GTA driver does not support some of the data types "
                    "used in this data set.\n" );
            delete poDS;
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Read and set meta information.                                  */
/* -------------------------------------------------------------------- */

    if( poDS->oHeader.global_taglist().get("GDAL/GEO_TRANSFORM") )
    {
        poDS->bHaveGeoTransform = true;
        ScanDoubles( poDS->oHeader.global_taglist().get( "GDAL/GEO_TRANSFORM" ),
                poDS->adfGeoTransform, 6 );
    }
    else
    {
        poDS->bHaveGeoTransform = false;
    }

    if( poDS->oHeader.global_taglist().get("GDAL/GCP_PROJECTION") )
    {
        poDS->pszGCPProjection = VSIStrdup( poDS->oHeader.global_taglist().get("GDAL/GCP_PROJECTION") );
    }
    if( poDS->oHeader.global_taglist().get("GDAL/GCP_COUNT") )
    {
        poDS->nGCPs = atoi( poDS->oHeader.global_taglist().get("GDAL/GCP_COUNT") );
        if( poDS->nGCPs < 1 )
        {
            poDS->nGCPs = 0;
        }
        else
        {
            poDS->pasGCPs = (GDAL_GCP *)VSIMalloc2( poDS->nGCPs, sizeof(GDAL_GCP) );
            if( poDS->pasGCPs == NULL )
            {
                CPLError( CE_Failure, CPLE_OutOfMemory, "Cannot allocate GCP list" );
                delete poDS;
                return NULL;
            }
            for( int i = 0; i < poDS->nGCPs; i++ )
            {
                poDS->pasGCPs[i].pszInfo = NULL;
                poDS->pasGCPs[i].dfGCPPixel = 0.0;
                poDS->pasGCPs[i].dfGCPLine = 0.0;
                poDS->pasGCPs[i].dfGCPX = 0.0;
                poDS->pasGCPs[i].dfGCPY = 0.0;
                poDS->pasGCPs[i].dfGCPZ = 0.0;
                poDS->pasGCPs[i].pszId = VSIStrdup( CPLSPrintf( "%d", i ) );
                char pszGCPTagName[64];
                char pszGCPInfoTagName[64];
                strcpy( pszGCPTagName, CPLSPrintf( "GDAL/GCP%d", i ) );
                strcpy( pszGCPInfoTagName, CPLSPrintf( "GDAL/GCP%d_INFO", i ) );
                if( poDS->oHeader.global_taglist().get(pszGCPInfoTagName) )
                {
                    poDS->pasGCPs[i].pszInfo = VSIStrdup( poDS->oHeader.global_taglist().get(pszGCPInfoTagName) );
                }
                else
                {
                    poDS->pasGCPs[i].pszInfo = VSIStrdup( "" );
                }
                if( poDS->oHeader.global_taglist().get(pszGCPTagName) )
                {
                    double adfTempDoubles[5];
                    ScanDoubles( poDS->oHeader.global_taglist().get(pszGCPTagName), adfTempDoubles, 5 );
                    poDS->pasGCPs[i].dfGCPPixel = adfTempDoubles[0];
                    poDS->pasGCPs[i].dfGCPLine = adfTempDoubles[1];
                    poDS->pasGCPs[i].dfGCPX = adfTempDoubles[2];
                    poDS->pasGCPs[i].dfGCPY = adfTempDoubles[3];
                    poDS->pasGCPs[i].dfGCPZ = adfTempDoubles[4];
                }
            }
        }
    }

    if( poDS->oHeader.global_taglist().get("DESCRIPTION") )
    {
        poDS->SetDescription( poDS->oHeader.global_taglist().get("DESCRIPTION") );
    }
    for( uintmax_t i = 0; i < poDS->oHeader.global_taglist().tags(); i++)
    {
        const char *pszTagName = poDS->oHeader.global_taglist().name( i );
        if( strncmp( pszTagName, "GDAL/META/", 10 ) == 0 )
        {
            const char *pDomainEnd = strchr( pszTagName + 10, '/' );
            if( pDomainEnd && pDomainEnd - (pszTagName + 10) > 0 )
            {
                char *pszDomain = (char *)VSIMalloc( pDomainEnd - (pszTagName + 10) + 1 );
                if( !pszDomain )
                {
                    CPLError( CE_Failure, CPLE_OutOfMemory, "Cannot allocate metadata buffer" );
                    delete poDS;
                    return NULL;
                }
                int j;
                for( j = 0; j < pDomainEnd - (pszTagName + 10); j++ )
                {
                    pszDomain[j] = pszTagName[10 + j];
                }
                pszDomain[j] = '\0';
                const char *pszName = pszTagName + 10 + j + 1;
                const char *pszValue = poDS->oHeader.global_taglist().value( i );
                poDS->SetMetadataItem( pszName, pszValue,
                        strcmp( pszDomain, "DEFAULT" ) == 0 ? NULL : pszDomain );
                VSIFree( pszDomain );
            }
        }
    }

    if( poDS->nBands > 0 )
    {
        poDS->SetMetadataItem( "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE" );
    }
    if( poDS->oHeader.compression() == gta::bzip2 )
        poDS->SetMetadataItem( "COMPRESSION", "BZIP2", "IMAGE_STRUCTURE" );
    else if( poDS->oHeader.compression() == gta::xz )
        poDS->SetMetadataItem( "COMPRESSION", "XZ", "IMAGE_STRUCTURE" );
    else if( poDS->oHeader.compression() == gta::zlib )
        poDS->SetMetadataItem( "COMPRESSION", "ZLIB", "IMAGE_STRUCTURE" );
    else if( poDS->oHeader.compression() == gta::zlib1 )
        poDS->SetMetadataItem( "COMPRESSION", "ZLIB1", "IMAGE_STRUCTURE" );
    else if( poDS->oHeader.compression() == gta::zlib2 )
        poDS->SetMetadataItem( "COMPRESSION", "ZLIB2", "IMAGE_STRUCTURE" );
    else if( poDS->oHeader.compression() == gta::zlib3 )
        poDS->SetMetadataItem( "COMPRESSION", "ZLIB3", "IMAGE_STRUCTURE" );
    else if( poDS->oHeader.compression() == gta::zlib4 )
        poDS->SetMetadataItem( "COMPRESSION", "ZLIB4", "IMAGE_STRUCTURE" );
    else if( poDS->oHeader.compression() == gta::zlib5 )
        poDS->SetMetadataItem( "COMPRESSION", "ZLIB5", "IMAGE_STRUCTURE" );
    else if( poDS->oHeader.compression() == gta::zlib6 )
        poDS->SetMetadataItem( "COMPRESSION", "ZLIB6", "IMAGE_STRUCTURE" );
    else if( poDS->oHeader.compression() == gta::zlib7 )
        poDS->SetMetadataItem( "COMPRESSION", "ZLIB7", "IMAGE_STRUCTURE" );
    else if( poDS->oHeader.compression() == gta::zlib8 )
        poDS->SetMetadataItem( "COMPRESSION", "ZLIB8", "IMAGE_STRUCTURE" );
    else if( poDS->oHeader.compression() == gta::zlib9 )
        poDS->SetMetadataItem( "COMPRESSION", "ZLIB9", "IMAGE_STRUCTURE" );

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < poDS->nBands; iBand++ )
    {
        poDS->SetBand( iBand+1, new GTARasterBand( poDS, iBand+1 ) );
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
/*                             CreateCopy()                             */
/************************************************************************/

static GDALDataset*
GTACreateCopy( const char * pszFilename, GDALDataset *poSrcDS,
                int bStrict, char ** papszOptions,
                GDALProgressFunc pfnProgress, void * pProgressData )

{
    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a GTA header                                             */
/* -------------------------------------------------------------------- */

    gta::compression eGTACompression = gta::none;
    const char *pszCompressionValue = CSLFetchNameValue( papszOptions,
            "COMPRESS" );
    if( pszCompressionValue != NULL )
    {
        if( EQUAL( pszCompressionValue, "NONE" ) )
            eGTACompression = gta::none;
        else if( EQUAL( pszCompressionValue, "BZIP2" ) )
            eGTACompression = gta::bzip2;
        else if( EQUAL( pszCompressionValue, "XZ" ) )
            eGTACompression = gta::xz;
        else if( EQUAL( pszCompressionValue, "ZLIB" ))
            eGTACompression = gta::zlib;
        else if( EQUAL( pszCompressionValue, "ZLIB1" ))
            eGTACompression = gta::zlib1;
        else if( EQUAL( pszCompressionValue, "ZLIB2" ))
            eGTACompression = gta::zlib2;
        else if( EQUAL( pszCompressionValue, "ZLIB3" ))
            eGTACompression = gta::zlib3;
        else if( EQUAL( pszCompressionValue, "ZLIB4" ))
            eGTACompression = gta::zlib4;
        else if( EQUAL( pszCompressionValue, "ZLIB5" ))
            eGTACompression = gta::zlib5;
        else if( EQUAL( pszCompressionValue, "ZLIB6" ))
            eGTACompression = gta::zlib6;
        else if( EQUAL( pszCompressionValue, "ZLIB7" ))
            eGTACompression = gta::zlib7;
        else if( EQUAL( pszCompressionValue, "ZLIB8" ))
            eGTACompression = gta::zlib8;
        else if( EQUAL( pszCompressionValue, "ZLIB9" ))
            eGTACompression = gta::zlib9;
        else
            CPLError( CE_Warning, CPLE_IllegalArg,
                      "COMPRESS=%s value not recognised, ignoring.",
                      pszCompressionValue );
    }

    gta::type *peGTATypes = (gta::type *)VSIMalloc2( poSrcDS->GetRasterCount(), sizeof(gta::type) );
    if( peGTATypes == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, "Cannot allocate GTA type list" );
        return NULL;
    }
    for( int i = 0; i < poSrcDS->GetRasterCount(); i++ )
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( i+1 );
        if( poSrcBand->GetColorInterpretation() == GCI_PaletteIndex )
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                    "The GTA driver does not support color palettes.\n" );
            VSIFree( peGTATypes );
            return NULL;
        }
        GDALDataType eDT = poSrcBand->GetRasterDataType();
        switch( eDT )
        {
        case GDT_Byte:
        {
            const char *pszPixelType = poSrcBand->GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
            if (pszPixelType && EQUAL(pszPixelType, "SIGNEDBYTE"))
                peGTATypes[i] = gta::int8;
            else
                peGTATypes[i] = gta::uint8;
            break;
        }
        case GDT_UInt16:
            peGTATypes[i] = gta::uint16;
            break;
        case GDT_Int16:
            peGTATypes[i] = gta::int16;
            break;
        case GDT_UInt32:
            peGTATypes[i] = gta::uint32;
            break;
        case GDT_Int32:
            peGTATypes[i] = gta::int32;
            break;
        case GDT_Float32:
            peGTATypes[i] = gta::float32;
            break;
        case GDT_Float64:
            peGTATypes[i] = gta::float64;
            break;
        case GDT_CInt16:
            if( bStrict )
            {
                CPLError( CE_Failure, CPLE_NotSupported,
                        "The GTA driver does not support the CInt16 data "
                        "type.\n"
                        "(If no strict copy is required, the driver can "
                        "use CFloat32 instead.)\n" );
                VSIFree( peGTATypes );
                return NULL;
            }
            peGTATypes[i] = gta::cfloat32;
            break;
        case GDT_CInt32:
            if( bStrict )
            {
                CPLError( CE_Failure, CPLE_NotSupported,
                        "The GTA driver does not support the CInt32 data "
                        "type.\n"
                        "(If no strict copy is required, the driver can "
                        "use CFloat64 instead.)\n" );
                VSIFree( peGTATypes );
                return NULL;
            }
            peGTATypes[i] = gta::cfloat64;
            break;
        case GDT_CFloat32:
            peGTATypes[i] = gta::cfloat32;
            break;
        case GDT_CFloat64:
            peGTATypes[i] = gta::cfloat64;
            break;
        default:
            CPLError( CE_Failure, CPLE_NotSupported,
                    "The GTA driver does not support source data sets using "
                    "unknown data types.\n");
            VSIFree( peGTATypes );
            return NULL;
        }
    }

    gta::header oHeader;
    try
    {
        oHeader.set_compression( eGTACompression );
        oHeader.set_dimensions( poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize() );
        oHeader.set_components( poSrcDS->GetRasterCount(), peGTATypes );
        const char *pszDescription = poSrcDS->GetDescription();
        // Metadata from GDALMajorObject
        if( pszDescription && pszDescription[0] != '\0' )
        {
            oHeader.global_taglist().set( "DESCRIPTION", pszDescription );
        }
        const char *papszMetadataDomains[] = { NULL /* default */, "RPC" };
        size_t nMetadataDomains = sizeof( papszMetadataDomains ) / sizeof( papszMetadataDomains[0] );
        for( size_t iDomain = 0; iDomain < nMetadataDomains; iDomain++ )
        {
            char **papszMetadata = poSrcDS->GetMetadata( papszMetadataDomains[iDomain] );
            if( papszMetadata )
            {
                for( int i = 0; papszMetadata[i]; i++ )
                {
                    char *pEqualSign = strchr( papszMetadata[i], '=' );
                    if( pEqualSign && pEqualSign - papszMetadata[i] > 0 )
                    {
                        *pEqualSign = '\0';
                        oHeader.global_taglist().set(
                                CPLSPrintf( "GDAL/META/%s/%s",
                                    papszMetadataDomains[iDomain] ? papszMetadataDomains[iDomain] : "DEFAULT",
                                    papszMetadata[i] ),
                                pEqualSign + 1 );
                        *pEqualSign = '=';
                    }
                }
            }
        }
        // Projection and transformation
        const char *pszWKT = poSrcDS->GetProjectionRef();
        if( pszWKT && pszWKT[0] != '\0' )
        {
            oHeader.global_taglist().set( "GDAL/PROJECTION", pszWKT );
        }
        double adfTransform[6];
        if( poSrcDS->GetGeoTransform( adfTransform ) == CE_None )
        {
            oHeader.global_taglist().set( "GDAL/GEO_TRANSFORM",
                    PrintDoubles( adfTransform, 6 ).c_str() );
        }
        // GCPs
        if( poSrcDS->GetGCPCount() > 0 )
        {
            oHeader.global_taglist().set( "GDAL/GCP_COUNT", CPLSPrintf( "%d", poSrcDS->GetGCPCount() ) );
            oHeader.global_taglist().set( "GDAL/GCP_PROJECTION", poSrcDS->GetGCPProjection() );
            const GDAL_GCP *pasGCPs = poSrcDS->GetGCPs();
            for( int i = 0; i < poSrcDS->GetGCPCount(); i++ )
            {
                char pszGCPTagName[64];
                char pszGCPInfoTagName[64];
                strcpy( pszGCPTagName, CPLSPrintf( "GDAL/GCP%d", i ) );
                strcpy( pszGCPInfoTagName, CPLSPrintf( "GDAL/GCP%d_INFO", i ) );
                if( pasGCPs[i].pszInfo && pasGCPs[i].pszInfo[0] != '\0' )
                {
                    oHeader.global_taglist().set( pszGCPInfoTagName, pasGCPs[i].pszInfo );
                }
                double adfTempDoubles[5];
                adfTempDoubles[0] = pasGCPs[i].dfGCPPixel;
                adfTempDoubles[1] = pasGCPs[i].dfGCPLine;
                adfTempDoubles[2] = pasGCPs[i].dfGCPX;
                adfTempDoubles[3] = pasGCPs[i].dfGCPY;
                adfTempDoubles[4] = pasGCPs[i].dfGCPZ;
                oHeader.global_taglist().set( pszGCPTagName, PrintDoubles( adfTempDoubles, 5 ).c_str() );
            }
        }
        // Now the bands
        for( int iBand = 0; iBand < poSrcDS->GetRasterCount(); iBand++ )
        {
            GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( iBand+1 );
            // Metadata from GDALMajorObject
            const char *pszBandDescription = poSrcBand->GetDescription();
            if( pszBandDescription && pszBandDescription[0] != '\0' )
            {
                oHeader.component_taglist( iBand ).set( "DESCRIPTION", pszBandDescription );
            }
            for( size_t iDomain = 0; iDomain < nMetadataDomains; iDomain++ )
            {
                char **papszBandMetadata = poSrcBand->GetMetadata( papszMetadataDomains[iDomain] );
                if( papszBandMetadata )
                {
                    for( int i = 0; papszBandMetadata[i]; i++ )
                    {
                        char *pEqualSign = strchr( papszBandMetadata[i], '=' );
                        if( pEqualSign && pEqualSign - papszBandMetadata[i] > 0 )
                        {
                            *pEqualSign = '\0';
                            oHeader.component_taglist( iBand ).set(
                                    CPLSPrintf( "GDAL/META/%s/%s",
                                        papszMetadataDomains[iDomain] ? papszMetadataDomains[iDomain] : "DEFAULT",
                                        papszBandMetadata[i] ),
                                    pEqualSign + 1 );
                            *pEqualSign = '=';
                        }
                    }
                }
            }
            // Category names
            char **papszCategoryNames = poSrcBand->GetCategoryNames( );
            if( papszCategoryNames )
            {
                int i;
                for( i = 0; papszCategoryNames[i]; i++ )
                {
                    oHeader.component_taglist( iBand ).set(
                            CPLSPrintf( "GDAL/CATEGORY%d", i ),
                            papszCategoryNames[i] );
                }
                oHeader.component_taglist( iBand ).set(
                        "GDAL/CATEGORY_COUNT", CPLSPrintf( "%d", i ) );
            }
            // No data value
            int bHaveNoDataValue;
            double dfNoDataValue;
            dfNoDataValue = poSrcBand->GetNoDataValue( &bHaveNoDataValue );
            if( bHaveNoDataValue )
                oHeader.component_taglist( iBand ).set( "NO_DATA_VALUE",
                        PrintDoubles( &dfNoDataValue, 1 ).c_str() );
            // Min/max values
            int bHaveMinValue;
            double dfMinValue;
            dfMinValue = poSrcBand->GetMinimum( &bHaveMinValue );
            if( bHaveMinValue )
                oHeader.component_taglist( iBand ).set( "MIN_VALUE",
                        PrintDoubles( &dfMinValue, 1 ).c_str() );
            int bHaveMaxValue;
            double dfMaxValue;
            dfMaxValue = poSrcBand->GetMaximum( &bHaveMaxValue );
            if( bHaveMaxValue )
                oHeader.component_taglist( iBand ).set( "MAX_VALUE",
                        PrintDoubles( &dfMaxValue, 1 ).c_str() );
            // Offset/scale values
            int bHaveOffsetValue;
            double dfOffsetValue;
            dfOffsetValue = poSrcBand->GetOffset( &bHaveOffsetValue );
            if( bHaveOffsetValue )
                oHeader.component_taglist( iBand ).set( "GDAL/OFFSET",
                        PrintDoubles( &dfOffsetValue, 1 ).c_str() );
            int bHaveScaleValue;
            double dfScaleValue;
            dfScaleValue = poSrcBand->GetScale( &bHaveScaleValue );
            if( bHaveScaleValue )
                oHeader.component_taglist( iBand ).set( "GDAL/SCALE",
                        PrintDoubles( &dfScaleValue, 1 ).c_str() );
            // Unit
            const char *pszUnit = poSrcBand->GetUnitType( );
            if( pszUnit != NULL && pszUnit[0] != '\0' )
                oHeader.component_taglist( iBand ).set( "UNIT", pszUnit );
            // Color interpretation
            GDALColorInterp eCI = poSrcBand->GetColorInterpretation();
            if( eCI == GCI_GrayIndex )
                oHeader.component_taglist( iBand ).set( "INTERPRETATION", "GRAY" );
            else if( eCI == GCI_RedBand )
                oHeader.component_taglist( iBand ).set( "INTERPRETATION", "RED" );
            else if( eCI == GCI_GreenBand )
                oHeader.component_taglist( iBand ).set( "INTERPRETATION", "GREEN" );
            else if( eCI == GCI_BlueBand )
                oHeader.component_taglist( iBand ).set( "INTERPRETATION", "BLUE" );
            else if( eCI == GCI_AlphaBand )
                oHeader.component_taglist( iBand ).set( "INTERPRETATION", "ALPHA" );
            else if( eCI == GCI_HueBand )
                oHeader.component_taglist( iBand ).set( "INTERPRETATION", "HSL/H" );
            else if( eCI == GCI_SaturationBand )
                oHeader.component_taglist( iBand ).set( "INTERPRETATION", "HSL/S" );
            else if( eCI == GCI_LightnessBand )
                oHeader.component_taglist( iBand ).set( "INTERPRETATION", "HSL/L" );
            else if( eCI == GCI_CyanBand )
                oHeader.component_taglist( iBand ).set( "INTERPRETATION", "CMYK/C" );
            else if( eCI == GCI_MagentaBand )
                oHeader.component_taglist( iBand ).set( "INTERPRETATION", "CMYK/M" );
            else if( eCI == GCI_YellowBand )
                oHeader.component_taglist( iBand ).set( "INTERPRETATION", "CMYK/Y" );
            else if( eCI == GCI_BlackBand )
                oHeader.component_taglist( iBand ).set( "INTERPRETATION", "CMYK/K" );
            else if( eCI == GCI_YCbCr_YBand )
                oHeader.component_taglist( iBand ).set( "INTERPRETATION", "YCBCR/Y" );
            else if( eCI == GCI_YCbCr_CbBand )
                oHeader.component_taglist( iBand ).set( "INTERPRETATION", "YCBCR/CB" );
            else if( eCI == GCI_YCbCr_CrBand )
                oHeader.component_taglist( iBand ).set( "INTERPRETATION", "YCBCR/CR" );
        }
    }
    catch( gta::exception &e )
    {
        CPLError( CE_Failure, CPLE_NotSupported, "GTA error: %s\n", e.what() );
        VSIFree( peGTATypes );
        return NULL;
    }
    VSIFree( peGTATypes );

/* -------------------------------------------------------------------- */
/*      Write header and data to the file                               */
/* -------------------------------------------------------------------- */

    GTAIO oGTAIO;
    if( oGTAIO.open( pszFilename, "w" ) != 0 )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                "Cannot create GTA file %s.\n", pszFilename );
        return NULL;
    }

    void *pLine = VSIMalloc2( oHeader.element_size(), oHeader.dimension_size(0) );
    if( pLine == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, "Cannot allocate scanline buffer.\n" );
        VSIFree( pLine );
        return NULL;
    }

    try
    {
        // Write header
        oHeader.write_to( oGTAIO );
        // Write data line by line
        gta::io_state oGTAIOState;
        for( int iLine = 0; iLine < poSrcDS->GetRasterYSize(); iLine++ )
        {
            size_t nComponentOffset = 0;
            for( int iBand = 0; iBand < poSrcDS->GetRasterCount(); iBand++ )
            {
                GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( iBand+1 );
                GDALDataType eDT = poSrcBand->GetRasterDataType();
                if( eDT == GDT_CInt16 )
                {
                    eDT = GDT_CFloat32;
                }
                else if( eDT == GDT_CInt32 )
                {
                    eDT = GDT_CFloat64;
                }
                char *pDst = (char *)pLine + nComponentOffset;
                CPLErr eErr = poSrcBand->RasterIO( GF_Read, 0, iLine,
                        poSrcDS->GetRasterXSize(), 1,
                        pDst, poSrcDS->GetRasterXSize(), 1, eDT,
                        oHeader.element_size(), 0 );
                if( eErr != CE_None )
                {
                    CPLError( CE_Failure, CPLE_FileIO, "Cannot read source data set.\n" );
                    VSIFree( pLine );
                    return NULL;
                }
                nComponentOffset += oHeader.component_size( iBand );
            }
            oHeader.write_elements( oGTAIOState, oGTAIO, poSrcDS->GetRasterXSize(), pLine );
            if( !pfnProgress( (iLine+1) / (double) poSrcDS->GetRasterYSize(),
                        NULL, pProgressData ) )
            {
                CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated CreateCopy()" );
                VSIFree( pLine );
                return NULL;
            }
        }
    }
    catch( gta::exception &e )
    {
        CPLError( CE_Failure, CPLE_FileIO, "GTA write error: %s\n", e.what() );
        VSIFree( pLine );
        return NULL;
    }
    VSIFree( pLine );

    oGTAIO.close();

/* -------------------------------------------------------------------- */
/*      Re-open dataset, and copy any auxilary pam information.         */
/* -------------------------------------------------------------------- */

    GTADataset *poDS = (GTADataset *) GDALOpen( pszFilename,
            eGTACompression == gta::none ? GA_Update : GA_ReadOnly );

    if( poDS )
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );

    return poDS;
}

/************************************************************************/
/*                          GDALRegister_GTA()                          */
/************************************************************************/

void GDALRegister_GTA()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "GTA" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "GTA" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                "Generic Tagged Arrays (.gta)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                "frmt_gta.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "gta" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                "Byte UInt16 Int16 UInt32 Int32 Float32 Float64 "
                "CInt16 CInt32 CFloat32 CFloat64" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
                "<CreationOptionList>"
                "  <Option name='COMPRESS' type='string-select'>"
                "    <Value>NONE</Value>"
                "    <Value>BZIP2</Value>"
                "    <Value>XZ</Value>"
                "    <Value>ZLIB</Value>"
                "    <Value>ZLIB1</Value>"
                "    <Value>ZLIB2</Value>"
                "    <Value>ZLIB3</Value>"
                "    <Value>ZLIB4</Value>"
                "    <Value>ZLIB5</Value>"
                "    <Value>ZLIB6</Value>"
                "    <Value>ZLIB7</Value>"
                "    <Value>ZLIB8</Value>"
                "    <Value>ZLIB9</Value>"
                "  </Option>"
                "</CreationOptionList>" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = GTADataset::Open;
        poDriver->pfnCreateCopy = GTACreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
