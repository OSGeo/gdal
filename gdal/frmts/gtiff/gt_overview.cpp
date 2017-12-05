/******************************************************************************
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Code to build overviews of external databases as a TIFF file.
 *           Only used by the GDALDefaultOverviews::BuildOverviews() method.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_port.h"
#include "gt_overview.h"

#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "gtiff.h"
#include "tiff.h"
#include "tiffvers.h"
#include "tifvsi.h"
#include "xtiffio.h"

CPL_CVSID("$Id$")

// TODO(schwehr): Explain why 128 and not 127.
static const int knMaxOverviews = 128;

/************************************************************************/
/*                         GTIFFWriteDirectory()                        */
/*                                                                      */
/*      Create a new directory, without any image data for an overview  */
/*      or a mask                                                       */
/*      Returns offset of newly created directory, but the              */
/*      current directory is reset to be the one in used when this      */
/*      function is called.                                             */
/************************************************************************/

toff_t GTIFFWriteDirectory( TIFF *hTIFF, int nSubfileType,
                            int nXSize, int nYSize,
                            int nBitsPerPixel, int nPlanarConfig, int nSamples,
                            int nBlockXSize, int nBlockYSize,
                            int bTiled, int nCompressFlag, int nPhotometric,
                            int nSampleFormat,
                            int nPredictor,
                            unsigned short *panRed,
                            unsigned short *panGreen,
                            unsigned short *panBlue,
                            int nExtraSamples,
                            unsigned short *panExtraSampleValues,
                            const char *pszMetadata,
                            const char* pszJPEGQuality,
                            const char* pszJPEGTablesMode,
                            const char* pszNoData )

{
    const toff_t nBaseDirOffset = TIFFCurrentDirOffset( hTIFF );

    // This is a bit of a hack to cause (*tif->tif_cleanup)(tif); to be called.
    // See https://trac.osgeo.org/gdal/ticket/2055
    TIFFSetField( hTIFF, TIFFTAG_COMPRESSION, COMPRESSION_NONE );

#if defined(TIFFLIB_VERSION) && TIFFLIB_VERSION >= 20051201  // 3.8.0
    TIFFFreeDirectory( hTIFF );
#endif

    TIFFCreateDirectory( hTIFF );

/* -------------------------------------------------------------------- */
/*      Setup TIFF fields.                                              */
/* -------------------------------------------------------------------- */
    TIFFSetField( hTIFF, TIFFTAG_IMAGEWIDTH, nXSize );
    TIFFSetField( hTIFF, TIFFTAG_IMAGELENGTH, nYSize );
    if( nSamples == 1 )
        TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG );
    else
        TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG, nPlanarConfig );

    TIFFSetField( hTIFF, TIFFTAG_BITSPERSAMPLE, nBitsPerPixel );
    TIFFSetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, nSamples );
    TIFFSetField( hTIFF, TIFFTAG_COMPRESSION, nCompressFlag );
    TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, nPhotometric );
    TIFFSetField( hTIFF, TIFFTAG_SAMPLEFORMAT, nSampleFormat );

    if( bTiled )
    {
        TIFFSetField( hTIFF, TIFFTAG_TILEWIDTH, nBlockXSize );
        TIFFSetField( hTIFF, TIFFTAG_TILELENGTH, nBlockYSize );
    }
    else
    {
        TIFFSetField( hTIFF, TIFFTAG_ROWSPERSTRIP, nBlockYSize );
    }

    TIFFSetField( hTIFF, TIFFTAG_SUBFILETYPE, nSubfileType );

    if( panExtraSampleValues != NULL )
    {
        TIFFSetField( hTIFF, TIFFTAG_EXTRASAMPLES, nExtraSamples,
                      panExtraSampleValues );
    }

    if( nCompressFlag == COMPRESSION_LZW ||
        nCompressFlag == COMPRESSION_ADOBE_DEFLATE )
        TIFFSetField( hTIFF, TIFFTAG_PREDICTOR, nPredictor );

/* -------------------------------------------------------------------- */
/*      Write color table if one is present.                            */
/* -------------------------------------------------------------------- */
    if( panRed != NULL )
    {
        TIFFSetField( hTIFF, TIFFTAG_COLORMAP, panRed, panGreen, panBlue );
    }

/* -------------------------------------------------------------------- */
/*      Write metadata if we have some.                                 */
/* -------------------------------------------------------------------- */
    if( pszMetadata && strlen(pszMetadata) > 0 )
        TIFFSetField( hTIFF, TIFFTAG_GDAL_METADATA, pszMetadata );


/* -------------------------------------------------------------------- */
/*      Write JPEG tables if needed.                                    */
/* -------------------------------------------------------------------- */
    if( nCompressFlag == COMPRESSION_JPEG )
    {
        GTiffWriteJPEGTables( hTIFF,
                              (nPhotometric == PHOTOMETRIC_RGB) ? "RGB" :
                              (nPhotometric == PHOTOMETRIC_YCBCR) ? "YCBCR" :
                                                                "MINISBLACK",
                              pszJPEGQuality,
                              pszJPEGTablesMode );

        if( nPhotometric == PHOTOMETRIC_YCBCR )
        {
            // Explicitly register the subsampling so that JPEGFixupTags
            // is a no-op (helps for cloud optimized geotiffs)
            TIFFSetField( hTIFF, TIFFTAG_YCBCRSUBSAMPLING, 2, 2 );
        }
    }

/* -------------------------------------------------------------------- */
/*      Write no data value if we have one.                             */
/* -------------------------------------------------------------------- */
    if( pszNoData != NULL )
    {
        TIFFSetField( hTIFF, TIFFTAG_GDAL_NODATA, pszNoData );
    }

/* -------------------------------------------------------------------- */
/*      Write directory, and return byte offset.                        */
/* -------------------------------------------------------------------- */
    if( TIFFWriteCheck( hTIFF, bTiled, "GTIFFWriteDirectory" ) == 0 )
    {
        TIFFSetSubDirectory( hTIFF, nBaseDirOffset );
        return 0;
    }

    TIFFWriteDirectory( hTIFF );
    TIFFSetDirectory( hTIFF,
                      static_cast<tdir_t>(TIFFNumberOfDirectories(hTIFF) - 1) );

    const toff_t nOffset = TIFFCurrentDirOffset( hTIFF );

    TIFFSetSubDirectory( hTIFF, nBaseDirOffset );

    return nOffset;
}

/************************************************************************/
/*                     GTIFFBuildOverviewMetadata()                     */
/************************************************************************/

void GTIFFBuildOverviewMetadata( const char *pszResampling,
                                 GDALDataset *poBaseDS,
                                 CPLString &osMetadata )

{
    osMetadata = "<GDALMetadata>";

    if( pszResampling && STARTS_WITH_CI(pszResampling, "AVERAGE_BIT2") )
        osMetadata +=
            "<Item name=\"RESAMPLING\" sample=\"0\">"
            "AVERAGE_BIT2GRAYSCALE</Item>";

    if( poBaseDS->GetMetadataItem( "INTERNAL_MASK_FLAGS_1" ) )
    {
        for( int iBand = 0; iBand < 200; iBand++ )
        {
            CPLString osItem;
            CPLString osName;

            osName.Printf( "INTERNAL_MASK_FLAGS_%d", iBand + 1 );
            if( poBaseDS->GetMetadataItem( osName ) )
            {
                osItem.Printf( "<Item name=\"%s\">%s</Item>",
                               osName.c_str(),
                               poBaseDS->GetMetadataItem( osName ) );
                osMetadata += osItem;
            }
        }
    }

    const char* pszNoDataValues = poBaseDS->GetMetadataItem("NODATA_VALUES");
    if( pszNoDataValues )
    {
        CPLString osItem;
        osItem.Printf( "<Item name=\"NODATA_VALUES\">%s</Item>",
                       pszNoDataValues );
        osMetadata += osItem;
    }

    if( !EQUAL(osMetadata,"<GDALMetadata>") )
        osMetadata += "</GDALMetadata>";
    else
        osMetadata = "";
}

/************************************************************************/
/*                        GTIFFBuildOverviews()                         */
/************************************************************************/

CPLErr
GTIFFBuildOverviews( const char * pszFilename,
                     int nBands, GDALRasterBand **papoBandList,
                     int nOverviews, int * panOverviewList,
                     const char * pszResampling,
                     GDALProgressFunc pfnProgress, void * pProgressData )

{
    if( nBands == 0 || nOverviews == 0 )
        return CE_None;

    if( !GTiffOneTimeInit() )
        return CE_Failure;

    TIFF *hOTIFF = NULL;
    int nBitsPerPixel = 0;
    int nCompression = COMPRESSION_NONE;
    int nPhotometric = 0;
    int nSampleFormat = 0;
    int nPlanarConfig = 0;
    int iOverview = 0;
    int nXSize = 0;
    int nYSize = 0;

/* -------------------------------------------------------------------- */
/*      Verify that the list of bands is suitable for emitting in       */
/*      TIFF file.                                                      */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        int nBandBits = 0;
        int nBandFormat = 0;
        GDALRasterBand *hBand = papoBandList[iBand];

        switch( hBand->GetRasterDataType() )
        {
          case GDT_Byte:
            nBandBits = 8;
            nBandFormat = SAMPLEFORMAT_UINT;
            break;

          case GDT_UInt16:
            nBandBits = 16;
            nBandFormat = SAMPLEFORMAT_UINT;
            break;

          case GDT_Int16:
            nBandBits = 16;
            nBandFormat = SAMPLEFORMAT_INT;
            break;

          case GDT_UInt32:
            nBandBits = 32;
            nBandFormat = SAMPLEFORMAT_UINT;
            break;

          case GDT_Int32:
            nBandBits = 32;
            nBandFormat = SAMPLEFORMAT_INT;
            break;

          case GDT_Float32:
            nBandBits = 32;
            nBandFormat = SAMPLEFORMAT_IEEEFP;
            break;

          case GDT_Float64:
            nBandBits = 64;
            nBandFormat = SAMPLEFORMAT_IEEEFP;
            break;

          case GDT_CInt16:
            nBandBits = 32;
            nBandFormat = SAMPLEFORMAT_COMPLEXINT;
            break;

          case GDT_CInt32:
            nBandBits = 64;
            nBandFormat = SAMPLEFORMAT_COMPLEXINT;
            break;

          case GDT_CFloat32:
            nBandBits = 64;
            nBandFormat = SAMPLEFORMAT_COMPLEXIEEEFP;
            break;

          case GDT_CFloat64:
            nBandBits = 128;
            nBandFormat = SAMPLEFORMAT_COMPLEXIEEEFP;
            break;

          default:
            CPLAssert( false );
            return CE_Failure;
        }

        if( hBand->GetMetadataItem( "NBITS", "IMAGE_STRUCTURE" ) )
        {
            nBandBits =
                atoi(hBand->GetMetadataItem("NBITS", "IMAGE_STRUCTURE"));

            if( nBandBits == 1
                && STARTS_WITH_CI(pszResampling, "AVERAGE_BIT2") )
                nBandBits = 8;
        }

        if( iBand == 0 )
        {
            nBitsPerPixel = nBandBits;
            nSampleFormat = nBandFormat;
            nXSize = hBand->GetXSize();
            nYSize = hBand->GetYSize();
        }
        else if( nBitsPerPixel != nBandBits || nSampleFormat != nBandFormat )
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "GTIFFBuildOverviews() doesn't support a mixture of band"
                      " data types." );
            return CE_Failure;
        }
        else if( hBand->GetColorTable() != NULL )
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "GTIFFBuildOverviews() doesn't support building"
                      " overviews of multiple colormapped bands." );
            return CE_Failure;
        }
        else if( hBand->GetXSize() != nXSize
                 || hBand->GetYSize() != nYSize )
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "GTIFFBuildOverviews() doesn't support building"
                      " overviews of different sized bands." );
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Use specified compression method.                               */
/* -------------------------------------------------------------------- */
    const char *pszCompress = CPLGetConfigOption( "COMPRESS_OVERVIEW", NULL );

    if( pszCompress != NULL && pszCompress[0] != '\0' )
    {
        nCompression =
            GTIFFGetCompressionMethod(pszCompress, "COMPRESS_OVERVIEW");
        if( nCompression < 0 )
            return CE_Failure;
    }

    if( nCompression == COMPRESSION_JPEG && nBitsPerPixel > 8 )
    {
        if( nBitsPerPixel > 16 )
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "GTIFFBuildOverviews() doesn't support building"
                      " JPEG compressed overviews of nBitsPerPixel > 16." );
            return CE_Failure;
        }

        nBitsPerPixel = 12;
    }

/* -------------------------------------------------------------------- */
/*      Figure out the planar configuration to use.                     */
/* -------------------------------------------------------------------- */
    if( nBands == 1 )
        nPlanarConfig = PLANARCONFIG_CONTIG;
    else
        nPlanarConfig = PLANARCONFIG_SEPARATE;

    const char* pszInterleave =
        CPLGetConfigOption( "INTERLEAVE_OVERVIEW", NULL );
    if( pszInterleave != NULL && pszInterleave[0] != '\0' )
    {
        if( EQUAL( pszInterleave, "PIXEL" ) )
            nPlanarConfig = PLANARCONFIG_CONTIG;
        else if( EQUAL( pszInterleave, "BAND" ) )
            nPlanarConfig = PLANARCONFIG_SEPARATE;
        else
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "INTERLEAVE_OVERVIEW=%s unsupported, "
                "value must be PIXEL or BAND. ignoring",
                pszInterleave );
        }
    }

/* -------------------------------------------------------------------- */
/*      Figure out the photometric interpretation to use.               */
/* -------------------------------------------------------------------- */
    if( nBands == 3 )
        nPhotometric = PHOTOMETRIC_RGB;
    else if( papoBandList[0]->GetColorTable() != NULL
             && !STARTS_WITH_CI(pszResampling, "AVERAGE_BIT2") )
    {
        nPhotometric = PHOTOMETRIC_PALETTE;
        // Should set the colormap up at this point too!
    }
    else
        nPhotometric = PHOTOMETRIC_MINISBLACK;

    const char* pszPhotometric =
        CPLGetConfigOption( "PHOTOMETRIC_OVERVIEW", NULL );
    if( pszPhotometric != NULL && pszPhotometric[0] != '\0' )
    {
        if( EQUAL( pszPhotometric, "MINISBLACK" ) )
            nPhotometric = PHOTOMETRIC_MINISBLACK;
        else if( EQUAL( pszPhotometric, "MINISWHITE" ) )
            nPhotometric = PHOTOMETRIC_MINISWHITE;
        else if( EQUAL( pszPhotometric, "RGB" ))
        {
            nPhotometric = PHOTOMETRIC_RGB;
        }
        else if( EQUAL( pszPhotometric, "CMYK" ))
        {
            nPhotometric = PHOTOMETRIC_SEPARATED;
        }
        else if( EQUAL( pszPhotometric, "YCBCR" ))
        {
            nPhotometric = PHOTOMETRIC_YCBCR;

            // Because of subsampling, setting YCBCR without JPEG compression
            // leads to a crash currently. Would need to make
            // GTiffRasterBand::IWriteBlock() aware of subsampling so that it
            // doesn't overrun buffer size returned by libtiff.
            if( nCompression != COMPRESSION_JPEG )
            {
                CPLError(
                    CE_Failure, CPLE_NotSupported,
                    "Currently, PHOTOMETRIC_OVERVIEW=YCBCR requires "
                    "COMPRESS_OVERVIEW=JPEG" );
                return CE_Failure;
            }

            if( pszInterleave != NULL &&
                pszInterleave[0] != '\0' &&
                nPlanarConfig == PLANARCONFIG_SEPARATE )
            {
                CPLError(
                    CE_Failure, CPLE_NotSupported,
                    "PHOTOMETRIC_OVERVIEW=YCBCR requires "
                    "INTERLEAVE_OVERVIEW=PIXEL" );
                return CE_Failure;
            }
            else
            {
                nPlanarConfig = PLANARCONFIG_CONTIG;
            }

            // YCBCR strictly requires 3 bands. Not less, not more
            // Issue an explicit error message as libtiff one is a bit cryptic:
            // JPEGLib:Bogus input colorspace.
            if( nBands != 3 )
            {
                CPLError(
                    CE_Failure, CPLE_NotSupported,
                    "PHOTOMETRIC_OVERVIEW=YCBCR requires a source raster "
                    "with only 3 bands (RGB)" );
                return CE_Failure;
            }
        }
        else if( EQUAL( pszPhotometric, "CIELAB" ))
        {
            nPhotometric = PHOTOMETRIC_CIELAB;
        }
        else if( EQUAL( pszPhotometric, "ICCLAB" ))
        {
            nPhotometric = PHOTOMETRIC_ICCLAB;
        }
        else if( EQUAL( pszPhotometric, "ITULAB" ))
        {
            nPhotometric = PHOTOMETRIC_ITULAB;
        }
        else
        {
            CPLError(
                CE_Warning, CPLE_IllegalArg,
                "PHOTOMETRIC_OVERVIEW=%s value not recognised, ignoring.",
                pszPhotometric );
        }
    }

/* -------------------------------------------------------------------- */
/*      Figure out the predictor value to use.                          */
/* -------------------------------------------------------------------- */
    int nPredictor = PREDICTOR_NONE;
    if( nCompression == COMPRESSION_LZW ||
        nCompression == COMPRESSION_ADOBE_DEFLATE )
    {
        const char* pszPredictor =
            CPLGetConfigOption( "PREDICTOR_OVERVIEW", NULL );
        if( pszPredictor != NULL )
        {
            nPredictor = atoi( pszPredictor );
        }
    }

/* -------------------------------------------------------------------- */
/*      Create the file, if it does not already exist.                  */
/* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;
    VSILFILE* fpL = NULL;

    if( VSIStatExL( pszFilename, &sStatBuf, VSI_STAT_EXISTS_FLAG ) != 0 )
    {
    /* -------------------------------------------------------------------- */
    /*      Compute the uncompressed size.                                  */
    /* -------------------------------------------------------------------- */
        double dfUncompressedOverviewSize = 0;
        int nDataTypeSize =
            GDALGetDataTypeSizeBytes(papoBandList[0]->GetRasterDataType());

        for( iOverview = 0; iOverview < nOverviews; iOverview++ )
        {
            const int nOXSize = (nXSize + panOverviewList[iOverview] - 1)
                / panOverviewList[iOverview];
            const int nOYSize = (nYSize + panOverviewList[iOverview] - 1)
                / panOverviewList[iOverview];

            dfUncompressedOverviewSize +=
                nOXSize * static_cast<double>(nOYSize) * nBands * nDataTypeSize;
        }

        if( nCompression == COMPRESSION_NONE
            && dfUncompressedOverviewSize > 4200000000.0 )
        {
    #ifndef BIGTIFF_SUPPORT
            CPLError(
                CE_Failure, CPLE_NotSupported,
                "The overview file would be larger than 4GB, "
                "but this is the largest size a TIFF can be, "
                "and BigTIFF is unavailable.  "
                "Creation failed." );
            return CE_Failure;
    #endif
        }
    /* -------------------------------------------------------------------- */
    /*      Should the file be created as a bigtiff file?                   */
    /* -------------------------------------------------------------------- */
        const char *pszBIGTIFF = CPLGetConfigOption( "BIGTIFF_OVERVIEW", NULL );

        if( pszBIGTIFF == NULL )
            pszBIGTIFF = "IF_SAFER";

        bool bCreateBigTIFF = false;
        if( EQUAL(pszBIGTIFF,"IF_NEEDED") )
        {
            if( nCompression == COMPRESSION_NONE
                && dfUncompressedOverviewSize > 4200000000.0 )
                bCreateBigTIFF = true;
        }
        else if( EQUAL(pszBIGTIFF,"IF_SAFER") )
        {
            // Look at the size of the base image and suppose that
            // the added overview levels won't be more than 1/2 of
            // the size of the base image. The theory says 1/3 of the
            // base image size if the overview levels are 2, 4, 8, 16.
            // Thus take 1/2 as the security margin for 1/3.
            const double dfUncompressedImageSize =
                nXSize * static_cast<double>(nYSize) * nBands * nDataTypeSize;
            if( dfUncompressedImageSize * 0.5 > 4200000000.0 )
                bCreateBigTIFF = true;
        }
        else
        {
            bCreateBigTIFF = CPLTestBool( pszBIGTIFF );
            if( !bCreateBigTIFF && nCompression == COMPRESSION_NONE
                && dfUncompressedOverviewSize > 4200000000.0 )
            {
                CPLError(
                    CE_Failure, CPLE_NotSupported,
                    "The overview file will be larger than 4GB, "
                    "so BigTIFF is necessary.  "
                    "Creation failed.");
                return CE_Failure;
            }
        }

    #ifndef BIGTIFF_SUPPORT
        if( bCreateBigTIFF )
        {
            CPLError(
                CE_Warning, CPLE_NotSupported,
                "BigTIFF requested, but GDAL built without BigTIFF "
                "enabled libtiff, request ignored." );
            bCreateBigTIFF = false;
        }
    #endif

        if( bCreateBigTIFF )
            CPLDebug( "GTiff", "File being created as a BigTIFF." );

        fpL = VSIFOpenL( pszFilename, "w+" );
        if( fpL == NULL )
            hOTIFF = NULL;
        else
            hOTIFF =
               VSI_TIFFOpen( pszFilename, bCreateBigTIFF ? "w+8" : "w+", fpL );
        if( hOTIFF == NULL )
        {
            if( CPLGetLastErrorNo() == 0 )
                CPLError( CE_Failure, CPLE_OpenFailed,
                          "Attempt to create new tiff file `%s' "
                          "failed in VSI_TIFFOpen().",
                          pszFilename );
            if( fpL != NULL )
                CPL_IGNORE_RET_VAL(VSIFCloseL(fpL));
            return CE_Failure;
        }
    }
/* -------------------------------------------------------------------- */
/*      Otherwise just open it for update access.                       */
/* -------------------------------------------------------------------- */
    else
    {
        fpL = VSIFOpenL( pszFilename, "r+" );
        if( fpL == NULL )
            hOTIFF = NULL;
        else
            hOTIFF = VSI_TIFFOpen( pszFilename, "r+", fpL );
        if( hOTIFF == NULL )
        {
            if( CPLGetLastErrorNo() == 0 )
                CPLError( CE_Failure, CPLE_OpenFailed,
                          "Attempt to create new tiff file `%s' "
                          "failed in VSI_TIFFOpen().",
                          pszFilename );
            if( fpL != NULL )
                CPL_IGNORE_RET_VAL(VSIFCloseL(fpL));
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we have a palette?  If so, create a TIFF compatible version. */
/* -------------------------------------------------------------------- */
    unsigned short *panRed = NULL;
    unsigned short *panGreen = NULL;
    unsigned short *panBlue = NULL;

    if( nPhotometric == PHOTOMETRIC_PALETTE )
    {
        GDALColorTable *poCT = papoBandList[0]->GetColorTable();
        int nColorCount = 65536;

        if( nBitsPerPixel <= 8 )
            nColorCount = 256;

        panRed = static_cast<unsigned short *>(
            CPLCalloc(nColorCount, sizeof(unsigned short)) );
        panGreen = static_cast<unsigned short *>(
            CPLCalloc(nColorCount, sizeof(unsigned short)) );
        panBlue = static_cast<unsigned short *>(
            CPLCalloc(nColorCount, sizeof(unsigned short)) );

        for( int iColor = 0; iColor < nColorCount; iColor++ )
        {
          GDALColorEntry sRGB = { 0, 0, 0, 0 };

            if( poCT->GetColorEntryAsRGB( iColor, &sRGB ) )
            {
                // TODO(schwehr): Check for underflow.
                // Going from signed short to unsigned short.
                panRed[iColor] = static_cast<unsigned short>(257 * sRGB.c1);
                panGreen[iColor] = static_cast<unsigned short>(257 * sRGB.c2);
                panBlue[iColor] = static_cast<unsigned short>(257 * sRGB.c3);
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we need some metadata for the overviews?                     */
/* -------------------------------------------------------------------- */
    CPLString osMetadata;
    GDALDataset *poBaseDS = papoBandList[0]->GetDataset();

    GTIFFBuildOverviewMetadata( pszResampling, poBaseDS, osMetadata );

/* -------------------------------------------------------------------- */
/*      Loop, creating overviews.                                       */
/* -------------------------------------------------------------------- */
    int nOvrBlockXSize = 0;
    int nOvrBlockYSize = 0;
    GTIFFGetOverviewBlockSize(&nOvrBlockXSize, &nOvrBlockYSize);

    CPLString osNoData; // don't move this in inner scope
    const char* pszNoData = NULL;
    int bNoDataSet = FALSE;
    const double dfNoDataValue = papoBandList[0]->GetNoDataValue(&bNoDataSet);
    if( bNoDataSet )
    {
        osNoData = GTiffFormatGDALNoDataTagValue(dfNoDataValue);
        pszNoData = osNoData.c_str();
    }

    for( iOverview = 0; iOverview < nOverviews; iOverview++ )
    {
        const int nOXSize = (nXSize + panOverviewList[iOverview] - 1)
            / panOverviewList[iOverview];
        const int nOYSize = (nYSize + panOverviewList[iOverview] - 1)
            / panOverviewList[iOverview];

        GTIFFWriteDirectory( hOTIFF, FILETYPE_REDUCEDIMAGE,
                             nOXSize, nOYSize, nBitsPerPixel,
                             nPlanarConfig, nBands,
                             nOvrBlockXSize, nOvrBlockYSize, TRUE, nCompression,
                             nPhotometric, nSampleFormat, nPredictor,
                             panRed, panGreen, panBlue,
                             0,
                             NULL, // TODO: How can we fetch extrasamples?
                             osMetadata,
                             CPLGetConfigOption( "JPEG_QUALITY_OVERVIEW", NULL ),
                             CPLGetConfigOption( "JPEG_TABLESMODE_OVERVIEW", NULL ),
                             pszNoData
                           );
    }

    if( panRed )
    {
        CPLFree(panRed);
        CPLFree(panGreen);
        CPLFree(panBlue);
        panRed = NULL;
        panGreen = NULL;
        panBlue = NULL;
    }

    XTIFFClose( hOTIFF );
    if( VSIFCloseL(fpL) != 0 )
        return CE_Failure;
    fpL = NULL;

/* -------------------------------------------------------------------- */
/*      Open the overview dataset so that we can get at the overview    */
/*      bands.                                                          */
/* -------------------------------------------------------------------- */
    GDALDataset *hODS =
        static_cast<GDALDataset *>( GDALOpen( pszFilename, GA_Update ) );
    if( hODS == NULL )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Do we need to set the jpeg quality?                             */
/* -------------------------------------------------------------------- */
    TIFF *hTIFF = static_cast<TIFF *>( hODS->GetInternalHandle(NULL) );

    if( nCompression == COMPRESSION_JPEG
        && CPLGetConfigOption( "JPEG_QUALITY_OVERVIEW", NULL ) != NULL )
    {
        const int nJpegQuality =
            atoi(CPLGetConfigOption("JPEG_QUALITY_OVERVIEW","75"));
        TIFFSetField( hTIFF, TIFFTAG_JPEGQUALITY,
                      nJpegQuality );
        GTIFFSetJpegQuality((GDALDatasetH)hODS, nJpegQuality);
    }

    if( nCompression == COMPRESSION_JPEG
        && CPLGetConfigOption( "JPEG_TABLESMODE_OVERVIEW", NULL ) != NULL )
    {
        const int nJpegTablesMode =
            atoi(CPLGetConfigOption("JPEG_TABLESMODE_OVERVIEW",
                            CPLSPrintf("%d", knGTIFFJpegTablesModeDefault)));
        TIFFSetField( hTIFF, TIFFTAG_JPEGTABLESMODE,
                      nJpegTablesMode );
        GTIFFSetJpegTablesMode((GDALDatasetH)hODS, nJpegTablesMode);
    }

/* -------------------------------------------------------------------- */
/*      Loop writing overview data.                                     */
/* -------------------------------------------------------------------- */

    int *panOverviewListSorted =
        static_cast<int*>(CPLMalloc(sizeof(int) * nOverviews));
    memcpy( panOverviewListSorted, panOverviewList, sizeof(int) * nOverviews);
    std::sort(panOverviewListSorted, panOverviewListSorted + nOverviews);

    GTIFFSetInExternalOvr(true);

    CPLErr eErr = CE_None;

    if( nCompression != COMPRESSION_NONE &&
        nPlanarConfig == PLANARCONFIG_CONTIG &&
        !GDALDataTypeIsComplex(papoBandList[0]->GetRasterDataType()) &&
        papoBandList[0]->GetColorTable() == NULL &&
        (STARTS_WITH_CI(pszResampling, "NEAR") ||
         EQUAL(pszResampling, "AVERAGE") ||
         EQUAL(pszResampling, "GAUSS") ||
         EQUAL(pszResampling, "CUBIC") ||
         EQUAL(pszResampling, "CUBICSPLINE") ||
         EQUAL(pszResampling, "LANCZOS") ||
         EQUAL(pszResampling, "BILINEAR")))
    {
        // In the case of pixel interleaved compressed overviews, we want to
        // generate the overviews for all the bands block by block, and not
        // band after band, in order to write the block once and not loose
        // space in the TIFF file.
        GDALRasterBand ***papapoOverviewBands =
            static_cast<GDALRasterBand ***>(
                CPLCalloc(sizeof(void *), nBands) );
        for( int iBand = 0; iBand < nBands && eErr == CE_None; iBand++ )
        {
            GDALRasterBand *poSrcBand = papoBandList[iBand];
            GDALRasterBand *poDstBand = hODS->GetRasterBand( iBand + 1 );
            papapoOverviewBands[iBand] =
                static_cast<GDALRasterBand **>(
                    CPLCalloc(sizeof(void *), nOverviews) );

            int bHasNoData = FALSE;
            const double noDataValue = poSrcBand->GetNoDataValue(&bHasNoData);
            if( bHasNoData )
                poDstBand->SetNoDataValue(noDataValue);

            for( int i = 0; i < nOverviews && eErr == CE_None; i++ )
            {
                for( int j = -1; j < poDstBand->GetOverviewCount() &&
                                 eErr == CE_None; j++ )
                {
                    GDALRasterBand * poOverview =
                            (j < 0 ) ? poDstBand : poDstBand->GetOverview( j );
                    if( poOverview == NULL )
                    {
                        eErr = CE_Failure;
                        continue;
                    }

                    const int nOvFactor =
                        GDALComputeOvFactor(poOverview->GetXSize(),
                                            poSrcBand->GetXSize(),
                                            poOverview->GetYSize(),
                                            poSrcBand->GetYSize());

                    if( nOvFactor == panOverviewListSorted[i]
                        || nOvFactor == GDALOvLevelAdjust2(
                                            panOverviewListSorted[i],
                                            poSrcBand->GetXSize(),
                                            poSrcBand->GetYSize() ) )
                    {
                        papapoOverviewBands[iBand][i] = poOverview;
                        if( bHasNoData )
                            poOverview->SetNoDataValue(noDataValue);
                        break;
                    }
                }
                CPLAssert( papapoOverviewBands[iBand][i] != NULL );
            }
        }

        if( eErr == CE_None )
            eErr =
                GDALRegenerateOverviewsMultiBand(
                    nBands, papoBandList,
                    nOverviews, papapoOverviewBands,
                    pszResampling, pfnProgress, pProgressData );

        for( int iBand = 0; iBand < nBands; iBand++ )
        {
            CPLFree(papapoOverviewBands[iBand]);
        }
        CPLFree(papapoOverviewBands);
    }
    else
    {
        GDALRasterBand **papoOverviews =
            static_cast<GDALRasterBand **>(
                CPLCalloc( sizeof(void*), knMaxOverviews ) );

        for( int iBand = 0; iBand < nBands && eErr == CE_None; iBand++ )
        {
            GDALRasterBand *hSrcBand = papoBandList[iBand];
            GDALRasterBand *hDstBand = hODS->GetRasterBand( iBand + 1 );

            int bHasNoData = FALSE;
            const double noDataValue = hSrcBand->GetNoDataValue(&bHasNoData);
            if( bHasNoData )
                hDstBand->SetNoDataValue(noDataValue);

            // FIXME: this logic regenerates all overview bands, not only the
            // ones requested.

            papoOverviews[0] = hDstBand;
            int nDstOverviews = hDstBand->GetOverviewCount() + 1;
            CPLAssert( nDstOverviews < knMaxOverviews );
            nDstOverviews = std::min(knMaxOverviews, nDstOverviews);

            // TODO(schwehr): Convert to starting with i = 1 and remove +1.
            for( int i = 0; i < nDstOverviews - 1 && eErr == CE_None; i++ )
            {
                papoOverviews[i+1] = hDstBand->GetOverview(i);
                if( papoOverviews[i+1] == NULL )
                {
                    eErr = CE_Failure;
                }
                else
                {
                    if( bHasNoData )
                        papoOverviews[i+1]->SetNoDataValue(noDataValue);
                }
            }

            void *pScaledProgressData =
                GDALCreateScaledProgress(
                    iBand / static_cast<double>( nBands ),
                    (iBand + 1) / static_cast<double>( nBands ),
                    pfnProgress, pProgressData );

            if( eErr == CE_None )
                eErr =
                    GDALRegenerateOverviews(
                        hSrcBand,
                        nDstOverviews,
                        reinterpret_cast<GDALRasterBandH *>( papoOverviews ),
                        pszResampling,
                        GDALScaledProgress,
                        pScaledProgressData );

            GDALDestroyScaledProgress( pScaledProgressData );
        }

        CPLFree( papoOverviews );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    if( eErr == CE_None )
        hODS->FlushCache();
    delete hODS;

    GTIFFSetInExternalOvr(false);

    CPLFree(panOverviewListSorted);

    pfnProgress( 1.0, NULL, pProgressData );

    return eErr;
}
