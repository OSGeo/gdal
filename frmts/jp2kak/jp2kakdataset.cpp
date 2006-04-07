/******************************************************************************
 * $Id$
 *
 * Project:  JPEG-2000
 * Purpose:  Implementation of the ISO/IEC 15444-1 standard based on Kakadu.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.37  2006/04/07 05:36:26  fwarmerdam
 * use new ReadAndParse() method
 *
 * Revision 1.36  2006/03/22 20:05:26  fwarmerdam
 * changed default tilesize rules, now tilexsize is at most 20K
 *
 * Revision 1.35  2006/03/22 19:50:38  fwarmerdam
 * Trim history.
 *
 * Revision 1.34  2006/03/22 19:50:00  fwarmerdam
 * Preliminary support for output with tiling.
 *
 * Revision 1.33  2006/03/16 17:11:39  fwarmerdam
 * Catch exceptions in allocator.finalize() to catch common out of memory
 * condition.
 *
 * Revision 1.32  2006/03/08 15:35:18  fwarmerdam
 * Fixed problem with bIsJPX test.
 *
 * Revision 1.31  2006/02/28 18:04:50  fwarmerdam
 * Make sure tiles get closed in case of an exception.
 *
 * Revision 1.30  2006/02/19 22:37:52  fwarmerdam
 * avoid warning with kakadu 4
 *
 * Revision 1.29  2005/07/05 22:09:50  fwarmerdam
 * use GDALJP2Metadata for geoinfo, better pam support
 *
 * Revision 1.28  2005/05/05 15:54:48  fwarmerdam
 * PAM Enabled
 *
 * Revision 1.27  2004/10/07 20:18:27  fwarmerdam
 * added support for greyscale+alpha
 *
 * Revision 1.26  2004/10/07 16:48:32  fwarmerdam
 * added simple CopyCreate RGBA output support
 *
 * Revision 1.25  2004/09/08 15:36:12  warmerda
 * fixed up initialization issues
 *
 * Revision 1.24  2004/07/29 16:38:28  warmerda
 * fixed up error trapping when opening subfile
 *
 * Revision 1.23  2004/07/28 14:39:48  warmerda
 * added jp2 subfile support
 *
 * Revision 1.22  2004/07/23 19:13:41  warmerda
 * Added J2K_SUBFILE support
 *
 * Revision 1.21  2004/04/02 17:26:46  warmerda
 * Added creation options.
 *
 * Revision 1.20  2004/01/15 21:21:04  warmerda
 * added lossless compression of 16bit values
 *
 * Revision 1.19  2004/01/12 19:50:09  warmerda
 * Disable jpip support on 4.1 till I get around to fixing it up.
 */

#include "gdal_pam.h"
#include "gdaljp2metadata.h"
#include "cpl_string.h"
#include "cpl_multiproc.h"
#include "jp2_local.h"

// Kakadu core includes
#include "kdu_elementary.h"
#include "kdu_messaging.h"
#include "kdu_params.h"
#include "kdu_compressed.h"
#include "kdu_sample_processing.h"

#ifdef FILEIO_DEBUG
#include "dbg_file_source.h"
#endif
#include "subfile_source.h"

// Application level includes
#include "kdu_file_io.h"
#include "jp2.h"

// ROI related.
#include "kdu_roi_processing.h"
#include "kdu_image.h"
#include "jp2kak_roi.h"

#ifdef J2_INPUT_MAX_BUFFER_BYTES
#  define KAKADU4

#  ifdef JP2_COMPRESSION_TYPE_NONE
#    define KAKADU41
#  endif
#endif									

#if defined(KAKADU4) && defined(WIN32) && !defined(KAKADU41)
#  define USE_JPIP
#endif

#ifdef USE_JPIP
#  include "kdu_client.h" 
#else
#  define kdu_client void
#endif

CPL_CVSID("$Id$");

CPL_C_START
CPLErr CPL_DLL GTIFMemBufFromWkt( const char *pszWKT, 
                                  const double *padfGeoTransform,
                                  int nGCPCount, const GDAL_GCP *pasGCPList,
                                  int *pnSize, unsigned char **ppabyBuffer );
CPLErr CPL_DLL GTIFWktFromMemBuf( int nSize, unsigned char *pabyBuffer, 
                          char **ppszWKT, double *padfGeoTransform,
                          int *pnGCPCount, GDAL_GCP **ppasGCPList );
CPL_C_END

static int kakadu_initialized = FALSE;

static void
transfer_bytes(kdu_byte *dest, kdu_line_buf &src, int gap, int precision,
               GDALDataType eOutType );

static const unsigned int jp2_uuid_box_type = 0x75756964;

static unsigned char msi_uuid2[16] =
{0xb1,0x4b,0xf8,0xbd,0x08,0x3d,0x4b,0x43,
 0xa5,0xae,0x8c,0xd7,0xd5,0xa6,0xce,0x03}; 

static unsigned char jp2_header[] = 
{0x00,0x00,0x00,0x0c,0x6a,0x50,0x20,0x20,0x0d,0x0a,0x87,0x0a};

static unsigned char jpc_header[] = 
{0xff,0x4f};


/************************************************************************/
/* ==================================================================== */
/*				JP2KAKDataset				*/
/* ==================================================================== */
/************************************************************************/

class JP2KAKDataset : public GDALPamDataset
{
    kdu_codestream oCodeStream;
    kdu_compressed_source *poInput;
    kdu_compressed_source *poRawInput;
#ifdef KAKADU4
    jp2_family_src  *family;
#endif
    kdu_client      *jpip_client;
    kdu_dims dims; 

    char	   *pszProjection;
    double	   adfGeoTransform[6];
    int            bGeoTransformValid;

    int		   nGCPCount;
    GDAL_GCP       *pasGCPList;

  public:
                JP2KAKDataset();
		~JP2KAKDataset();
    
    virtual CPLErr GetGeoTransform( double * );
    virtual const char *GetProjectionRef(void);
    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();

    static void KakaduInitialize();
    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            JP2KAKRasterBand                          */
/* ==================================================================== */
/************************************************************************/

class JP2KAKRasterBand : public GDALPamRasterBand
{
    friend class JP2KAKDataset;

    int         nDiscardLevels; 

    kdu_dims 	band_dims; 

    int		nOverviewCount;
    JP2KAKRasterBand **papoOverviewBand;

    kdu_client      *jpip_client;

    kdu_codestream oCodeStream;

    GDALColorTable oCT;

    int         bYCbCrReported;
    
    GDALColorInterp eInterp;

  public:

    		JP2KAKRasterBand( int, int, kdu_codestream, int, kdu_client *,
                                  jp2_channels );
    		~JP2KAKRasterBand();
    
    virtual CPLErr IReadBlock( int, int, void * );

    virtual int    GetOverviewCount();
    virtual GDALRasterBand *GetOverview( int );

    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();

    // internal

    void        ApplyPalette( jp2_palette oJP2Palette );
    void        ProcessYCbCrTile(kdu_tile tile, GByte *pabyBuffer, 
                                 int nTileXOff, int nTileYOff );
    void        ProcessTile(kdu_tile tile, GByte *pabyBuffer, 
                            int nTileXOff, int nTileYOff );
};

/************************************************************************/
/* ==================================================================== */
/*                     Set up messaging services                        */
/* ==================================================================== */
/************************************************************************/

class kdu_cpl_error_message : public kdu_message 
{
public: // Member classes
    kdu_cpl_error_message( CPLErr eErrClass ) 
    {
        m_eErrClass = eErrClass;
        m_pszError = NULL;
    }

    void put_text(const char *string)
    {
        if( m_pszError == NULL )
            m_pszError = CPLStrdup( string );
        else
        {
            m_pszError = (char *) 
                CPLRealloc(m_pszError, strlen(m_pszError) + strlen(string)+1 );
            strcat( m_pszError, string );
        }
    }

    class JP2KAKException
    {
    };

    void flush(bool end_of_message=false) 
    {
        if( m_pszError == NULL )
            return;
        if( m_pszError[strlen(m_pszError)-1] == '\n' )
            m_pszError[strlen(m_pszError)-1] = '\0';

        CPLError( m_eErrClass, CPLE_AppDefined, "%s", m_pszError );
        CPLFree( m_pszError );
        m_pszError = NULL;

        if( end_of_message && m_eErrClass == CE_Failure )
        {
            throw new JP2KAKException();
        }
    }

private:
    CPLErr m_eErrClass;
    char *m_pszError;
};

/************************************************************************/
/* ==================================================================== */
/*                            JP2KAKRasterBand                          */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           JP2KAKRasterBand()                         */
/************************************************************************/

JP2KAKRasterBand::JP2KAKRasterBand( int nBand, int nDiscardLevels,
                                    kdu_codestream oCodeStream,
                                    int nResCount, kdu_client *jpip_client,
                                    jp2_channels oJP2Channels )

{
    this->nBand = nBand;
    bYCbCrReported = FALSE;

    if( oCodeStream.get_bit_depth(nBand-1) == 16
        && oCodeStream.get_signed(nBand-1) )
        this->eDataType = GDT_Int16;
    else if( oCodeStream.get_bit_depth(nBand-1) == 16
        && !oCodeStream.get_signed(nBand-1) )
        this->eDataType = GDT_UInt16;
    else if( oCodeStream.get_bit_depth(nBand-1) == 32 )
        this->eDataType = GDT_Float32;
    else
        this->eDataType = GDT_Byte;

    this->nDiscardLevels = nDiscardLevels;
    this->oCodeStream = oCodeStream;

    this->jpip_client = jpip_client;

    oCodeStream.apply_input_restrictions( 0, 0, nDiscardLevels, 0, NULL );
    oCodeStream.get_dims( 0, band_dims );

    this->nRasterXSize = band_dims.size.x;
    this->nRasterYSize = band_dims.size.y;

/* -------------------------------------------------------------------- */
/*      Use a 512x128 "virtual" block size unless the file is small.    */
/* -------------------------------------------------------------------- */
    if( nRasterXSize >= 1024 )
        nBlockXSize = 512;
    else
        nBlockXSize = nRasterXSize;
    
    if( nRasterYSize >= 256 )
        nBlockYSize = 128;
    else
        nBlockYSize = nRasterYSize;

/* -------------------------------------------------------------------- */
/*      Figure out the color interpretation for this band.              */
/* -------------------------------------------------------------------- */
    
    eInterp = GCI_Undefined;

    if( oJP2Channels.exists() )
    {
        int nRedIndex=-1, nGreenIndex=-1, nBlueIndex=-1, nLutIndex;
#ifdef KAKADU41
        int nCSI;
#endif

        if( oJP2Channels.get_num_colours() == 3 )
        {
#ifdef KAKADU41 
            oJP2Channels.get_colour_mapping( 0, nRedIndex, nLutIndex, nCSI );
            oJP2Channels.get_colour_mapping( 1, nGreenIndex, nLutIndex, nCSI );
            oJP2Channels.get_colour_mapping( 2, nBlueIndex, nLutIndex, nCSI );
#else
            oJP2Channels.get_colour_mapping( 0, nRedIndex, nLutIndex );
            oJP2Channels.get_colour_mapping( 1, nGreenIndex, nLutIndex );
            oJP2Channels.get_colour_mapping( 2, nBlueIndex, nLutIndex );
#endif
        }
        else
        {
#ifdef KAKADU41 
            oJP2Channels.get_colour_mapping( 0, nRedIndex, nLutIndex, nCSI );
#else
            oJP2Channels.get_colour_mapping( 0, nRedIndex, nLutIndex );
#endif
            if( nBand == 1 )
                eInterp = GCI_GrayIndex;
        }

        if( eInterp != GCI_Undefined )
            /* nothing to do */;

        // If we have LUT info, it is a palette image.
        else if( nLutIndex != -1 )
            eInterp = GCI_PaletteIndex;

        // Establish color band this is. 
        else if( nRedIndex == nBand-1 )
            eInterp = GCI_RedBand;
        else if( nGreenIndex == nBand-1 )
            eInterp = GCI_GreenBand;
        else if( nBlueIndex == nBand-1 )
            eInterp = GCI_BlueBand;
        else
            eInterp = GCI_Undefined;

        // Could this band be an alpha band?
        if( eInterp == GCI_Undefined )
        {
            int color_idx, opacity_idx, lut_idx;

            for( color_idx = 0; 
                 color_idx < oJP2Channels.get_num_colours(); color_idx++ )
            {
#ifdef KAKADU41 
                if( oJP2Channels.get_opacity_mapping( color_idx, opacity_idx,
                                                      lut_idx, nCSI ) )
                {
                    if( opacity_idx == nBand - 1 )
                        eInterp = GCI_AlphaBand;
                }
                if( oJP2Channels.get_premult_mapping( color_idx, opacity_idx,
                                                      lut_idx, nCSI ) )
                {
                    if( opacity_idx == nBand - 1 )
                        eInterp = GCI_AlphaBand;
                }
#else
                if( oJP2Channels.get_opacity_mapping( color_idx, opacity_idx,
                                                      lut_idx ) )
                {
                    if( opacity_idx == nBand - 1 )
                        eInterp = GCI_AlphaBand;
                }
                if( oJP2Channels.get_premult_mapping( color_idx, opacity_idx,
                                                      lut_idx ) )
                {
                    if( opacity_idx == nBand - 1 )
                        eInterp = GCI_AlphaBand;
                }
#endif
            }
        }
    }
    else if( nBand == 1 )
        eInterp = GCI_RedBand;
    else if( nBand == 2 )
        eInterp = GCI_GreenBand;
    else if( nBand == 3 )
        eInterp = GCI_BlueBand;
    else
        eInterp = GCI_GrayIndex;
        
/* -------------------------------------------------------------------- */
/*      Do we have any overviews?  Only check if we are the full res    */
/*      image.                                                          */
/* -------------------------------------------------------------------- */
    nOverviewCount = 0;
    papoOverviewBand = 0;

    if( nDiscardLevels == 0 )
    {
        int  nXSize = nRasterXSize, nYSize = nRasterYSize;

        for( int nDiscard = 1; nDiscard < nResCount; nDiscard++ )
        {
            kdu_dims  dims;

            nXSize = (nXSize+1) / 2;
            nYSize = (nYSize+1) / 2;

            if( (nXSize+nYSize) < 128 || nXSize < 4 || nYSize < 4 )
                continue; /* skip super reduced resolution layers */

            oCodeStream.apply_input_restrictions( 0, 0, nDiscard, 0, NULL );
            oCodeStream.get_dims( 0, dims );

            if( (dims.size.x == nXSize || dims.size.x == nXSize-1)
                && (dims.size.y == nYSize || dims.size.y == nYSize-1) )
            {
                nOverviewCount++;
                papoOverviewBand = (JP2KAKRasterBand **) 
                    CPLRealloc( papoOverviewBand, 
                                sizeof(void*) * nOverviewCount );
                papoOverviewBand[nOverviewCount-1] = 
                    new JP2KAKRasterBand( nBand, nDiscard, oCodeStream, 0,
                                          jpip_client, oJP2Channels );
            }
            else
            {
                CPLDebug( "GDAL", "Discard %dx%d JPEG2000 overview layer,\n"
                          "expected %dx%d.", 
                          dims.size.x, dims.size.y, nXSize, nYSize );
            }
        }
    }
}

/************************************************************************/
/*                         ~JP2KAKRasterBand()                          */
/************************************************************************/

JP2KAKRasterBand::~JP2KAKRasterBand()

{
    for( int i = 0; i < nOverviewCount; i++ )
        delete papoOverviewBand[i];

    CPLFree( papoOverviewBand );
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int JP2KAKRasterBand::GetOverviewCount()

{
    return nOverviewCount;
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *JP2KAKRasterBand::GetOverview( int iOverviewIndex )

{
    if( iOverviewIndex < 0 || iOverviewIndex >= nOverviewCount )
        return NULL;
    else
        return papoOverviewBand[iOverviewIndex];
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr JP2KAKRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                      void * pImage )
{
    try
    {
/* -------------------------------------------------------------------- */
/*      Setup a ROI matching the block requested.                       */
/* -------------------------------------------------------------------- */
        kdu_dims dims = band_dims;

        dims.pos.x = dims.pos.x + nBlockXOff * nBlockXSize;
        dims.pos.y = dims.pos.y + nBlockYOff * nBlockYSize;
        dims.size.x = nBlockXSize;
        dims.size.y = nBlockYSize;
    
        kdu_dims dims_roi;

        oCodeStream.apply_input_restrictions( 0, 0, nDiscardLevels, 0, NULL );
        oCodeStream.map_region( 0, dims, dims_roi );
        oCodeStream.apply_input_restrictions( 0, 0, nDiscardLevels, 0, 
                                              &dims_roi );

/* -------------------------------------------------------------------- */
/*      Handle JPIP Protocol request if we are using it.                */
/* -------------------------------------------------------------------- */
#ifdef USE_JPIP
        if( jpip_client != NULL )
        {
            kdu_window  window;

            window.region = dims_roi;
            window.resolution.x = nRasterXSize;
            window.resolution.y = nRasterYSize;
            window.set_num_components(1);
            window.components[0] = nBand - 1;
            window.max_layers = 0;

            jpip_client->post_window( &window );

            while( !jpip_client->is_idle() )
                CPLSleep( 0.25 );
            
            if( !jpip_client->is_alive() )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "JPIP failure some time after post_window()." );
                return CE_Failure;
            }
        }
#endif
    
/* -------------------------------------------------------------------- */
/*      Now we are ready to walk through the tiles processing them      */
/*      one-by-one.                                                     */
/* -------------------------------------------------------------------- */
        kdu_dims tile_indices; 
        kdu_coords tpos;
        int  nWordSize = GDALGetDataTypeSize( eDataType ) / 8;
    
        oCodeStream.get_valid_tiles(tile_indices);
    
        for (tpos.y=0; tpos.y < tile_indices.size.y; tpos.y++)
        {
            for (tpos.x=0; tpos.x < tile_indices.size.x; tpos.x++)
            {
                kdu_tile tile = oCodeStream.open_tile(tpos+tile_indices.pos);

                kdu_resolution res = 
                    tile.access_component(0).access_resolution();
                kdu_dims tile_dims; res.get_dims(tile_dims);
                kdu_coords offset = tile_dims.pos - dims.pos;

                GByte *pabyDest;

                pabyDest = ((GByte *)pImage) 
                    + (offset.x + offset.y*nBlockXSize) * nWordSize;

                try 
                {

                    if( tile.get_ycc() && nBand < 4 )
                        ProcessYCbCrTile( tile, pabyDest, offset.x, offset.y );
                    else
                        ProcessTile( tile, pabyDest, offset.x, offset.y );
                    tile.close();
                }
                catch( ... )
                {
                    tile.close();
                    return CE_Failure;
                }

            }
        }

        return CE_None;
    }
/* -------------------------------------------------------------------- */
/*      Catch interal Kakadu errors.                                    */
/* -------------------------------------------------------------------- */
    catch( ... )
    {
        return CE_Failure;
    }
}

/************************************************************************/
/*                            ProcessTile()                             */
/*                                                                      */
/*      Process data from one component of one tile into working        */
/*      buffer.                                                         */
/************************************************************************/

void JP2KAKRasterBand::ProcessTile( kdu_tile tile, GByte *pabyDest, 
                                    int /*nTileXOff*/, int /*nTileYOff*/ )

{
    // Open tile-components and create processing engines and resources
    kdu_dims dims;
    kdu_sample_allocator allocator;
    kdu_tile_comp comp = tile.access_component(nBand-1);
    kdu_line_buf line;
    kdu_pull_ifc engine;
    bool reversible = comp.get_reversible();
    int bit_depth = comp.get_bit_depth();
    kdu_resolution res = comp.access_resolution();
    int  nWordSize = GDALGetDataTypeSize( eDataType ) / 8;

    res.get_dims(dims);
    
    bool use_shorts = (comp.get_bit_depth(true) <= 16);

    line.pre_create(&allocator,dims.size.x,reversible,use_shorts);

    if (res.which() == 0) // No DWT levels used
        engine =
            kdu_decoder(res.access_subband(LL_BAND),&allocator,use_shorts);
    else
        engine = kdu_synthesis(res,&allocator,use_shorts);

    allocator.finalize(); // Actually creates buffering resources

    line.create(); // Grabs resources from the allocator.

    // Now walk through the lines of the buffer, recovering them from the
    // relevant tile-component processing engines.

    for( int y = 0; y < dims.size.y; y++ )
    {
        engine.pull(line,true);
        transfer_bytes(pabyDest + y * nBlockXSize * nWordSize,
                       line, nWordSize, bit_depth, eDataType );
    }

    engine.destroy();
}

/************************************************************************/
/*                          ProcessYCbCrTile()                          */
/*                                                                      */
/*      Process data from the Y, Cb and Cr components of a tile into    */
/*      RGB and then copy the desired red, green or blue portion out    */
/*      into the working buffer.                                        */
/************************************************************************/

void JP2KAKRasterBand::ProcessYCbCrTile( kdu_tile tile, GByte *pabyDest, 
                                         int /*nTileXOff*/, int /*nTileYOff*/ )

{
    // Open tile-components and create processing engines and resources
    kdu_dims dims;
    kdu_sample_allocator allocator;
    kdu_tile_comp comp[3] = {tile.access_component(0),
                             tile.access_component(1),
                             tile.access_component(2)};
    kdu_line_buf line[3];
    kdu_pull_ifc engine[3];
    bool reversible = comp[0].get_reversible();
    int bit_depth = comp[0].get_bit_depth();
    kdu_resolution res = comp[0].access_resolution();
    int  nWordSize = GDALGetDataTypeSize( eDataType ) / 8;
    int  iComp;

    if( !bYCbCrReported && nBand == 1 )
    {
        bYCbCrReported = TRUE;
        CPLDebug( "JP2KAK", "Using ProcessYCbCrTile() for this dataset." );
    }

    res.get_dims(dims);
    
    bool use_shorts = (comp[0].get_bit_depth(true) <= 16);

    for( iComp = 0; iComp < 3; iComp++ )
    {
        line[iComp].pre_create(&allocator,dims.size.x,reversible,use_shorts);

        if (res.which() == 0) // No DWT levels used
            engine[iComp] =
                kdu_decoder(comp[iComp].access_resolution().access_subband(LL_BAND),
                            &allocator,use_shorts);
        else
            engine[iComp] = 
                kdu_synthesis(comp[iComp].access_resolution(),&allocator,use_shorts);
    }

    allocator.finalize(); // Actually creates buffering resources

    for( iComp = 0; iComp < 3; iComp++ )
        line[iComp].create(); // Grabs resources from the allocator.

    // Now walk through the lines of the buffer, recovering them from the
    // relevant tile-component processing engines.

    for( int y = 0; y < dims.size.y; y++ )
    {
        engine[0].pull(line[0],true);
        engine[1].pull(line[1],true);
        engine[2].pull(line[2],true);

        kdu_convert_ycc_to_rgb(line[0], line[1], line[2]);

        transfer_bytes(pabyDest + y * nBlockXSize * nWordSize,
                       line[nBand-1], nWordSize, bit_depth, eDataType );
    }

    engine[0].destroy();
    engine[1].destroy();
    engine[2].destroy();
}

/************************************************************************/
/*                            ApplyPalette()                            */
/************************************************************************/

void JP2KAKRasterBand::ApplyPalette( jp2_palette oJP2Palette )

{
/* -------------------------------------------------------------------- */
/*      Do we have a reasonable LUT configuration?  RGB or RGBA?        */
/* -------------------------------------------------------------------- */
    if( !oJP2Palette.exists() )
        return;

    if( oJP2Palette.get_num_luts() == 0 || oJP2Palette.get_num_entries() == 0 )
        return;

    if( oJP2Palette.get_num_luts() < 3 )
    {
        CPLDebug( "JP2KAK", "JP2KAKRasterBand::ApplyPalette()\n"
                  "Odd get_num_luts() value (%d)", 
                  oJP2Palette.get_num_luts() );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Fetch the lut entries.  Note that they are normalized in the    */
/*      -0.5 to 0.5 range.                                              */
/* -------------------------------------------------------------------- */
    float *pafLUT;
    int   iColor, nCount = oJP2Palette.get_num_entries();

    pafLUT = (float *) CPLCalloc(sizeof(float)*4, nCount);

    oJP2Palette.get_lut(0, pafLUT + 0);
    oJP2Palette.get_lut(1, pafLUT + nCount);
    oJP2Palette.get_lut(2, pafLUT + nCount*2);

    if( oJP2Palette.get_num_luts() == 4 )
    {
        oJP2Palette.get_lut( 2, pafLUT + nCount*3 );
    }
    else
    {
        for( iColor = 0; iColor < nCount; iColor++ )
        {
            pafLUT[nCount*3 + iColor] = 0.5;
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Apply to GDAL colortable.                                       */
/* -------------------------------------------------------------------- */
    for( iColor=0; iColor < nCount; iColor++ )
    {
        GDALColorEntry sEntry;

        sEntry.c1 = (short) MAX(0,MIN(255,pafLUT[iColor + nCount*0]*256+128));
        sEntry.c2 = (short) MAX(0,MIN(255,pafLUT[iColor + nCount*1]*256+128));
        sEntry.c3 = (short) MAX(0,MIN(255,pafLUT[iColor + nCount*2]*256+128));
        sEntry.c4 = (short) MAX(0,MIN(255,pafLUT[iColor + nCount*3]*256+128));

        oCT.SetColorEntry( iColor, &sEntry );
    }

    CPLFree( pafLUT );

    eInterp = GCI_PaletteIndex;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp JP2KAKRasterBand::GetColorInterpretation()

{
    return eInterp;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *JP2KAKRasterBand::GetColorTable()

{
    if( oCT.GetColorEntryCount() > 0 )
        return &oCT;
    else
        return NULL;
}

/************************************************************************/
/* ==================================================================== */
/*				JP2KAKDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           JP2KAKDataset()                           */
/************************************************************************/

JP2KAKDataset::JP2KAKDataset()

{
    poInput = NULL;
    poRawInput = NULL;
    pszProjection = NULL;
    nGCPCount = 0;
    pasGCPList = NULL;
#ifdef KAKADU4
    family = NULL;
#endif

    bGeoTransformValid = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~JP2KAKDataset()                         */
/************************************************************************/

JP2KAKDataset::~JP2KAKDataset()

{
    FlushCache();

    CPLFree( pszProjection );
    
    if( poInput != NULL )
    {
        oCodeStream.destroy();
        poInput->close();
        delete poInput;
#ifdef KAKADU4
        if( family )
        {
            family->close();
            delete family;
        }
#endif
        if( poRawInput != NULL )
            delete poRawInput;
#ifdef USE_JPIP
        if( jpip_client != NULL )
        {
            jpip_client->close();
            delete jpip_client;
        }
#endif
    }
}


/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *JP2KAKDataset::GetProjectionRef()

{
    if( pszProjection && *pszProjection )
        return( pszProjection );
    else
        return GDALPamDataset::GetProjectionRef();
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr JP2KAKDataset::GetGeoTransform( double * padfTransform )

{
    if( bGeoTransformValid )
    {
        memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );
    
        return CE_None;
    }
    else
        return GDALPamDataset::GetGeoTransform( padfTransform );
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int JP2KAKDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *JP2KAKDataset::GetGCPProjection()

{
    if( nGCPCount > 0 )
        return pszProjection;
    else
        return "";
}

/************************************************************************/
/*                               GetGCP()                               */
/************************************************************************/

const GDAL_GCP *JP2KAKDataset::GetGCPs()

{
    return pasGCPList;
}

/************************************************************************/
/*                          KakaduInitialize()                          */
/************************************************************************/

void JP2KAKDataset::KakaduInitialize()

{
/* -------------------------------------------------------------------- */
/*      Initialize Kakadu warning/error reporting subsystem.            */
/* -------------------------------------------------------------------- */
    if( !kakadu_initialized )
    {
        kakadu_initialized = TRUE;

        kdu_cpl_error_message oErrHandler( CE_Failure );
        kdu_cpl_error_message oWarningHandler( CE_Warning );
        
        kdu_customize_warnings(new kdu_cpl_error_message( CE_Warning ) );
        kdu_customize_errors(new kdu_cpl_error_message( CE_Failure ) );
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *JP2KAKDataset::Open( GDALOpenInfo * poOpenInfo )

{
    subfile_source *poRawInput = NULL;
    const char  *pszExtension = NULL;
    int         bIsJPIP = FALSE;
    int         bIsSubfile = FALSE;
    GByte      *pabyHeader = NULL;

/* -------------------------------------------------------------------- */
/*      Check header                                                    */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->fp == NULL )
    {
        pszExtension = CPLGetExtension( poOpenInfo->pszFilename );
        if( (EQUALN(poOpenInfo->pszFilename,"http://",7)
             || EQUALN(poOpenInfo->pszFilename,"https://",8)
             || EQUALN(poOpenInfo->pszFilename,"jpip://",7))
            && EQUAL(pszExtension,"jp2") )
        {
            bIsJPIP = TRUE;
        }
        else if( EQUALN(poOpenInfo->pszFilename,"J2K_SUBFILE:",12) )
        {
            static GByte abySubfileHeader[16];

            KakaduInitialize();
            try
            {
                poRawInput = new subfile_source( poOpenInfo->pszFilename );
                poRawInput->seek( 0 );

                poRawInput->read( abySubfileHeader, 16 );
                poRawInput->seek( 0 );
            }
            catch( ... )
            {
                return NULL;
            }

            pabyHeader = abySubfileHeader;

            bIsSubfile = TRUE;
        }
        else
            return NULL;
    }
    else
    {
        if( poOpenInfo->nHeaderBytes < 16 )
            return NULL;

        pabyHeader = poOpenInfo->pabyHeader;
    }

/* -------------------------------------------------------------------- */
/*      Any extension is supported for JP2 files.  Only selected        */
/*      extensions are supported for JPC files since the standard       */
/*      prefix is so short (two bytes).                                 */
/* -------------------------------------------------------------------- */
    if( !bIsJPIP )
    {
        if( memcmp(pabyHeader,jp2_header,sizeof(jp2_header)) == 0 )
            pszExtension = "jp2";
        else if( memcmp( pabyHeader, jpc_header, sizeof(jpc_header) ) == 0 )
        {
            pszExtension = CPLGetExtension( poOpenInfo->pszFilename );
            if( !EQUAL(pszExtension,"jpc") && !EQUAL(pszExtension,"j2k") 
                && !EQUAL(pszExtension,"jp2") && !EQUAL(pszExtension,"jpx") 
                && !EQUAL(pszExtension,"j2c") )
                pszExtension = "jpc";
        }
        else
            return NULL;
    }

    KakaduInitialize();
        
/* -------------------------------------------------------------------- */
/*      Try to open the file in a manner depending on the extension.    */
/* -------------------------------------------------------------------- */
    kdu_compressed_source *poInput = NULL;
    kdu_client      *jpip_client = NULL;
    jp2_palette oJP2Palette;
    jp2_channels oJP2Channels;

#ifdef KAKADU4
    jp2_family_src *family = NULL;
#endif

    try
    {
        if( bIsJPIP )
        {
#ifdef USE_JPIP
            jp2_source *jp2_src;
            char *pszWrk = CPLStrdup(strstr(poOpenInfo->pszFilename,"://")+3);
            char *pszRequest = strstr(pszWrk,"/");
     
            if( pszRequest == NULL )
            {
                CPLDebug( "JP2KAK", 
                          "Failed to parse JPIP server and request." );
                CPLFree( pszWrk );
                return NULL;
            }

            *(pszRequest++) = '\0';

            CPLDebug( "JP2KAK", "server=%s, request=%s", 
                      pszWrk, pszRequest );

            CPLSleep( 15.0 );
            jpip_client = new kdu_client;
            jpip_client->connect( pszWrk, NULL, pszRequest, "http-tcp", 
                                  "" );
            
            CPLDebug( "JP2KAK", "After connect()" );

            bool bin0_complete = false;

            while( jpip_client->get_databin_length(KDU_META_DATABIN,0,0,
                                                   &bin0_complete) <= 0 
                   || !bin0_complete )
                CPLSleep( 0.25 );

            family = new jp2_family_src;
            family->open( jpip_client );

            jp2_src = new jp2_source;
            jp2_src->open( family );
            jp2_src->read_header();

            while( !jpip_client->is_idle() )
                CPLSleep( 0.25 );

            if( jpip_client->is_alive() )
                CPLDebug( "JP2KAK", "connect() seems to be complete." );
            else
            {
                CPLDebug( "JP2KAK", "connect() seems to have failed." );
                return NULL;
            }

            oJP2Channels = jp2_src->access_channels();

            poInput = jp2_src;
#else
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "JPIP Protocol not supported by GDAL with Kakadu 3.4 or on Unix." );
#endif
        }
        else if( EQUAL(pszExtension,"jp2") || EQUAL(pszExtension,"jpx") )
        {
            jp2_source *jp2_src;

#ifdef KAKADU4
            family = new jp2_family_src;
            if( poRawInput != NULL )
                family->open( poRawInput );
            else
                family->open( poOpenInfo->pszFilename, true );
            jp2_src = new jp2_source;
            jp2_src->open( family );
            jp2_src->read_header();
#else
            jp2_src = new jp2_source;
            jp2_src->open( poOpenInfo->pszFilename, true );
#endif
            poInput = jp2_src;

            oJP2Palette = jp2_src->access_palette();
            oJP2Channels = jp2_src->access_channels();
        }
        else if( poRawInput == NULL )
        {
#ifndef FILEIO_DEBUG
            poInput = new kdu_simple_file_source( poOpenInfo->pszFilename );
#else
            poInput = new dbg_simple_file_source( poOpenInfo->pszFilename );
#endif								       
        }
        else
        {
            poInput = poRawInput;
            poRawInput = NULL;
        }
    }
    catch( ... )
    {
        CPLDebug( "JP2KAK", "Trapped Kakadu exception." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    JP2KAKDataset 	*poDS = NULL;

    try
    {
        poDS = new JP2KAKDataset();

        poDS->poInput = poInput;
        poDS->poRawInput = poRawInput;
        poDS->oCodeStream.create( poInput );
        poDS->oCodeStream.set_fussy();
//        poDS->oCodeStream.set_resilient();
        poDS->oCodeStream.set_persistent();

        poDS->jpip_client = jpip_client;

#ifdef KAKADU4
        poDS->family = family;
#endif
/* -------------------------------------------------------------------- */
/*      Get overall image size.                                         */
/* -------------------------------------------------------------------- */
        poDS->oCodeStream.get_dims( 0, poDS->dims );
        
        poDS->nRasterXSize = poDS->dims.size.x;
        poDS->nRasterYSize = poDS->dims.size.y;

/* -------------------------------------------------------------------- */
/*      Ensure that all the components have the same dimensions.  If    */
/*      not, just process the first dimension.                          */
/* -------------------------------------------------------------------- */
        poDS->nBands = poDS->oCodeStream.get_num_components();

        if (poDS->nBands > 1 )
        { 
            int iDim;
        
            for( iDim = 1; iDim < poDS->nBands; iDim++ )
            {
                kdu_dims  dim_this_comp;
            
                poDS->oCodeStream.get_dims(iDim, dim_this_comp);

                if( dim_this_comp != poDS->dims )
                {
                    CPLError( CE_Warning, CPLE_AppDefined,
                              "Some components have mismatched dimensions, "
                              "ignoring all but first." );
                    poDS->nBands = 1;
                    break;
                }
            }
        }

/* -------------------------------------------------------------------- */
/*      find out how many resolutions levels are available.             */
/* -------------------------------------------------------------------- */
        kdu_dims tile_indices; 
        poDS->oCodeStream.get_valid_tiles(tile_indices);

        kdu_tile tile = poDS->oCodeStream.open_tile(tile_indices.pos);
        int nResCount = tile.access_component(0).get_num_resolutions();
        tile.close();

        CPLDebug( "JP2KAK", "nResCount=%d", nResCount );

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
        int iBand;
    
        for( iBand = 1; iBand <= poDS->nBands; iBand++ )
        {
            JP2KAKRasterBand *poBand = 
                new JP2KAKRasterBand(iBand,0,poDS->oCodeStream, nResCount,
                                     jpip_client, oJP2Channels );

            if( iBand == 1 && oJP2Palette.exists() )
                poBand->ApplyPalette( oJP2Palette );

            poDS->SetBand( iBand, poBand );
        }

/* -------------------------------------------------------------------- */
/*      Look for supporting coordinate system information.              */
/* -------------------------------------------------------------------- */
        if( poOpenInfo->fp != NULL )
        {
            GDALJP2Metadata oJP2Geo;
        
            if( oJP2Geo.ReadAndParse( poOpenInfo->pszFilename ) )
            {
                poDS->pszProjection = CPLStrdup(oJP2Geo.pszProjection);
                poDS->bGeoTransformValid = TRUE;
                memcpy( poDS->adfGeoTransform, oJP2Geo.adfGeoTransform, 
                        sizeof(double) * 6 );
                poDS->nGCPCount = oJP2Geo.nGCPCount;
                poDS->pasGCPList = oJP2Geo.pasGCPList;
                oJP2Geo.pasGCPList = NULL;
            }
        }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
        poDS->SetDescription( poOpenInfo->pszFilename );
        poDS->TryLoadXML();

        return( poDS );
    }

/* -------------------------------------------------------------------- */
/*      Catch all fatal kakadu errors and cleanup a bit.                */
/* -------------------------------------------------------------------- */
    catch( ... )
    {
        CPLDebug( "JP2KAK", "JP2KAKDataset::Open() - caught exception." );
        if( poDS != NULL )
            delete poDS;

        return NULL;
    }
}

/************************************************************************/
/*                           transfer_bytes()                           */
/*                                                                      */
/*      Support function for JP2KAKRasterBand::ProcessTile().           */
/************************************************************************/

static void
transfer_bytes(kdu_byte *dest, kdu_line_buf &src, int gap, int precision,
               GDALDataType eOutType )

  /* Transfers source samples from the supplied line buffer into the output
     byte buffer, spacing successive output samples apart by `gap' bytes
     (to allow for interleaving of colour components).  The function performs
     all necessary level shifting, type conversion, rounding and truncation. */
{
    int width = src.get_width();
    if (src.get_buf32() != NULL)
    { // Decompressed samples have a 32-bit representation (integer or float)
        assert(precision >= 8); // Else would have used 16 bit representation
        kdu_sample32 *sp = src.get_buf32();
        if (!src.is_absolute() && eOutType != GDT_Byte )
        { // Transferring normalized floating point data.
            float scale16 = (float)(1<<16);
            int val;

            for (; width > 0; width--, sp++, dest+=gap)
            {
                if( eOutType == GDT_Int16 )
                {
                    val = (int) (sp->fval*scale16);
                    *((GInt16 *) dest) = (GInt16) MAX(MIN(val,32767),-32768);
                }
                else if( eOutType == GDT_UInt16 )
                {
                    val = (int) (sp->fval*scale16) + 32768;
                    *((GUInt16 *) dest) = (GUInt16) MAX(MIN(val,65535),0);
                }
                else if( eOutType == GDT_Float32 )
                    *((float *) dest) = sp->fval;
            }
        }
        else if (!src.is_absolute())
        { // Transferring normalized floating point data.
            float scale16 = (float)(1<<16);
            kdu_int32 val;

            for (; width > 0; width--, sp++, dest+=gap)
            {
                val = (kdu_int32)(sp->fval*scale16);
                val = (val+128)>>8; // May be faster than true rounding
                val += 128;
                if (val & ((-1)<<8))
                    val = (val<0)?0:255;
                *dest = (kdu_byte) val;
            }
        }
        else if( eOutType == GDT_Int16 || eOutType == GDT_UInt16 )
        { // Transferring 32-bit absolute integers.
            kdu_int32 val;
              
            for (; width > 0; width--, sp++, dest+=gap)
            {
                val = sp->ival;

                if( eOutType == GDT_Int16 )
                {
                    *((GInt16 *) dest) = (GInt16) MAX(MIN(val,32767),-32768);
                }
                else
                {
                    assert( eOutType == GDT_UInt16 );
                    *((GUInt16 *) dest) = (GUInt16) MAX(MIN(val,65535),0);
                }
            }
        }
        else
        { // Transferring 32-bit absolute integers.
            kdu_int32 val;
            kdu_int32 downshift = precision-8;
            kdu_int32 offset = (1<<downshift)>>1;

            for (; width > 0; width--, sp++, dest+=gap)
            {
                val = sp->ival;
                val = (val+offset)>>downshift;
                val += 128;

                if (val & ((-1)<<8))
                    val = (val<0)?0:255;
                *dest = (kdu_byte) val;
            }
        }
    }
    else
    { // Source data is 16 bits.
        kdu_sample16 *sp = src.get_buf16();
        if (!src.is_absolute())
        { // Transferring 16-bit fixed point quantities
            kdu_int16 val;

            if (precision >= 8)
            { // Can essentially ignore the bit-depth.
                for (; width > 0; width--, sp++, dest+=gap)
                {
                    val = sp->ival;
                    val += (1<<(KDU_FIX_POINT-8))>>1;
                    val >>= (KDU_FIX_POINT-8);
                    val += 128;
                    if (val & ((-1)<<8))
                        val = (val<0)?0:255;
                    *dest = (kdu_byte) val;
                }
            }
            else
            { // Need to force zeros into one or more least significant bits.
                kdu_int16 downshift = (kdu_int16) (KDU_FIX_POINT-precision);
                kdu_int16 upshift = (kdu_int16) (8-precision);
                kdu_int16 offset = 1<<(downshift-1);

                for (; width > 0; width--, sp++, dest+=gap)
                {
                    val = sp->ival;
                    val = (val+offset)>>downshift;
                    val <<= upshift;
                    val += 128;
                    if (val & ((-1)<<8))
                        val = (val<0)?0:(256-(1<<upshift));
                    *dest = (kdu_byte) val;
                }
            }
        }
        else
        { // Transferring 16-bit absolute integers.
            kdu_int16 val;

            if (precision >= 8)
            {
                kdu_int16 downshift = (kdu_int16) (precision-8);
                kdu_int16 offset = (kdu_int16) ((1<<downshift)>>1);
              
                for (; width > 0; width--, sp++, dest+=gap)
                {
                    val = sp->ival;
                    val = (val+offset)>>downshift;
                    val += 128;
                    if (val & ((-1)<<8))
                        val = (val<0)?0:255;
                    *dest = (kdu_byte) val;
                }
            }
            else
            {
                kdu_int16 upshift = (kdu_int16) (8-precision);

                for (; width > 0; width--, sp++, dest+=gap)
                {
                    val = sp->ival;
                    val <<= upshift;
                    val += 128;
                    if (val & ((-1)<<8))
                        val = (val<0)?0:(256-(1<<upshift));
                    *dest = (kdu_byte) val;
                }
            }
        }
    }
}

/************************************************************************/
/*                       JP2KAKWriteGeoTIFFInfo()                       */
/************************************************************************/

void JP2KAKWriteGeoTIFFInfo( jp2_target *jp2_out, GDALDataset *poSrcDS )

{
/* -------------------------------------------------------------------- */
/*      Prepare the memory buffer containing the degenerate GeoTIFF     */
/*      file.                                                           */
/* -------------------------------------------------------------------- */
    const char *pszWKT;
    double	adfGeoTransform[6];
    int         nGTBufSize = 0;
    unsigned char *pabyGTBuf = NULL;

    if( GDALGetGCPCount( poSrcDS ) > 0 )
        pszWKT = poSrcDS->GetGCPProjection();
    else
        pszWKT = poSrcDS->GetProjectionRef();

    poSrcDS->GetGeoTransform(adfGeoTransform);

    if( GTIFMemBufFromWkt( pszWKT, adfGeoTransform, 
                           poSrcDS->GetGCPCount(), poSrcDS->GetGCPs(), 
                           &nGTBufSize, &pabyGTBuf ) != CE_None )
        return;

    if( nGTBufSize == 0 )
        return;

/* -------------------------------------------------------------------- */
/*      Write to a box on the JP2 file.                                 */
/* -------------------------------------------------------------------- */
#ifdef KAKADU4
    jp2_out->open_next( jp2_uuid_box_type );

    jp2_out->write( (kdu_byte *) msi_uuid2, sizeof(msi_uuid2) );

    jp2_out->write( (kdu_byte *) pabyGTBuf, nGTBufSize );

    jp2_out->close();
#else
    jp2_output_box &uuid_box = jp2_out->open_box( jp2_uuid_box_type );

    uuid_box.write( (kdu_byte *) msi_uuid2, sizeof(msi_uuid2) );

    uuid_box.write( (kdu_byte *) pabyGTBuf, nGTBufSize );

    uuid_box.close();
#endif

    CPLFree( pabyGTBuf );
}

/************************************************************************/
/*                     JP2KAKCreateCopy_WriteTile()                     */
/************************************************************************/

static int 
JP2KAKCreateCopy_WriteTile( GDALDataset *poSrcDS, kdu_tile &oTile,
                            kdu_roi_image *poROIImage, 
                            int nXOff, int nYOff, int nXSize, int nYSize,
                            int bReversible, GDALDataType eType,
                            kdu_codestream &oCodeStream, int bFlushEnabled,
                            kdu_long *layer_bytes, int layer_count,
                            GDALProgressFunc pfnProgress, void * pProgressData)

{                                       
/* -------------------------------------------------------------------- */
/*      Create one big tile, and a compressing engine, and line         */
/*      buffer for each component.                                      */
/* -------------------------------------------------------------------- */
    int c, num_components = oTile.get_num_components(); 
    kdu_push_ifc  *engines = new kdu_push_ifc[num_components];
    kdu_line_buf  *lines = new kdu_line_buf[num_components];
    kdu_sample_allocator allocator;
    for (c=0; c < num_components; c++)
    {
        kdu_resolution res = oTile.access_component(c).access_resolution(); 
        kdu_roi_node *roi_node = NULL;

        if( poROIImage != NULL )
        {
            kdu_dims  dims;
            
            res.get_dims(dims);
            roi_node = poROIImage->acquire_node(c,dims);
        }

        lines[c].pre_create(&allocator,nXSize,bReversible,bReversible);
        engines[c] = kdu_analysis(res,&allocator,bReversible,1.0F,roi_node);
    }

    try
    {
        allocator.finalize();

        for (c=0; c < num_components; c++)
            lines[c].create();
    }
    catch( ... )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "allocate.finalize() failed, likely out of memory for compression information." );
        return FALSE;
    }
        
/* -------------------------------------------------------------------- */
/*      Write whole image.  Write 1024 lines of each component, then    */
/*      go back to the first, and do again.  This gives the rate        */
/*      computing machine all components to make good estimates.        */
/* -------------------------------------------------------------------- */
    int  iLine, iLinesWritten = 0;
#define CHUNK_SIZE  1024

    GByte *pabyBuffer = (GByte *) 
        CPLMalloc(nXSize * (GDALGetDataTypeSize(eType)/8) );

    CPLAssert( !oTile.get_ycc() );

    for( iLine = 0; iLine < nYSize; iLine += CHUNK_SIZE )
    {
        for (c=0; c < num_components; c++)
        {
            GDALRasterBand *poBand = poSrcDS->GetRasterBand( c+1 );
            int iSubline = 0;
        
            for( iSubline = iLine; 
                 iSubline < iLine+CHUNK_SIZE && iSubline < nYSize;
                 iSubline++ )
            {
                if( poBand->RasterIO( GF_Read, 
                                      nXOff, nYOff+iSubline, nXSize, 1, 
                                      (void *) pabyBuffer, nXSize, 1, eType,
                                      0, 0 ) == CE_Failure )
                    return FALSE;

                if( bReversible && eType == GDT_Byte )
                {
                    kdu_sample16 *dest = lines[c].get_buf16();
                    kdu_byte *sp = pabyBuffer;
                
                    for (int n=nXSize; n > 0; n--, dest++, sp++)
                        dest->ival = ((kdu_int16)(*sp)) - 128;
                }
                else if( bReversible && eType == GDT_Int16 )
                {
                    kdu_sample16 *dest = lines[c].get_buf16();
                    GInt16 *sp = (GInt16 *) pabyBuffer;
                
                    for (int n=nXSize; n > 0; n--, dest++, sp++)
                        dest->ival = *sp;
                }
                else if( bReversible && eType == GDT_UInt16 )
                {
                    kdu_sample16 *dest = lines[c].get_buf16();
                    GUInt16 *sp = (GUInt16 *) pabyBuffer;
                
                    for (int n=nXSize; n > 0; n--, dest++, sp++)
                        dest->ival = *sp - 32767;
                }
                else if( eType == GDT_Byte )
                {
                    kdu_sample32 *dest = lines[c].get_buf32();
                    kdu_byte *sp = pabyBuffer;
                
                    for (int n=nXSize; n > 0; n--, dest++, sp++)
                        dest->fval = (float) 
                            ((((kdu_int16)(*sp))-128.0) * 0.00390625);
                }
                else if( eType == GDT_Int16 )
                {
                    kdu_sample32 *dest = lines[c].get_buf32();
                    GInt16  *sp = (GInt16 *) pabyBuffer;
                
                    for (int n=nXSize; n > 0; n--, dest++, sp++)
                        dest->fval = (float) 
                            (((kdu_int16)(*sp)) * 0.0000152588);
                }
                else if( eType == GDT_UInt16 )
                {
                    kdu_sample32 *dest = lines[c].get_buf32();
                    GUInt16  *sp = (GUInt16 *) pabyBuffer;
                
                    for (int n=nXSize; n > 0; n--, dest++, sp++)
                        dest->fval = (float) 
                            (((int)(*sp) - 32768) * 0.0000152588);
                }
                else if( eType == GDT_Float32 )
                {
                    kdu_sample32 *dest = lines[c].get_buf32();
                    float  *sp = (float *) pabyBuffer;
                
                    for (int n=nXSize; n > 0; n--, dest++, sp++)
                        dest->fval = *sp;  /* scale it? */
                }

                engines[c].push(lines[c],true);

                iLinesWritten++;

                if( !pfnProgress( iLinesWritten 
                                  / (double) (num_components * nYSize),
                                  NULL, pProgressData ) )
                {
                    return FALSE;
                }
            }
        }

        if( oCodeStream.ready_for_flush() && bFlushEnabled )
        {
            CPLDebug( "JP2KAK", 
                      "Calling oCodeStream.flush() at line %d",
                      MIN(nYSize,iLine+CHUNK_SIZE) );
            
            oCodeStream.flush( layer_bytes, layer_count );
        }
        else if( bFlushEnabled )
            CPLDebug( "JP2KAK", 
                      "read_for_flush() is false at line %d.",
                      iLine );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup resources.                                              */
/* -------------------------------------------------------------------- */
    for( c = 0; c < num_components; c++ )
        engines[c].destroy();

    delete[] engines;
    delete[] lines;

    CPLFree( pabyBuffer );

    if( poROIImage != NULL )
        delete poROIImage;

    return TRUE;
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

static GDALDataset *
JP2KAKCreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                  int bStrict, char ** papszOptions, 
                  GDALProgressFunc pfnProgress, void * pProgressData )

{
    int	   nXSize = poSrcDS->GetRasterXSize();
    int    nYSize = poSrcDS->GetRasterYSize();
    int    nTileXSize = nXSize;
    int    nTileYSize = nYSize;
    bool   bReversible = false;
    int    bFlushEnabled = CSLFetchBoolean( papszOptions, "FLUSH", TRUE );

/* -------------------------------------------------------------------- */
/*      Initialize Kakadu warning/error reporting subsystem.            */
/* -------------------------------------------------------------------- */
    if( !kakadu_initialized )
    {
        kakadu_initialized = TRUE;

        kdu_cpl_error_message oErrHandler( CE_Failure );
        kdu_cpl_error_message oWarningHandler( CE_Warning );
        
        kdu_customize_warnings(new kdu_cpl_error_message( CE_Warning ) );
        kdu_customize_errors(new kdu_cpl_error_message( CE_Failure ) );
    }

/* -------------------------------------------------------------------- */
/*      What data type should we use?  We assume all datatypes match    */
/*      the first band.                                                 */
/* -------------------------------------------------------------------- */
    GDALDataType eType;
    GDALRasterBand *poPrototypeBand = poSrcDS->GetRasterBand(1);

    eType = poPrototypeBand->GetRasterDataType();
    if( eType != GDT_Byte && eType != GDT_Int16 && eType != GDT_UInt16
        && eType != GDT_Float32 )
    {
        if( bStrict )
        {
            CPLError(CE_Failure, CPLE_AppDefined, 
                     "JP2KAK (JPEG2000) driver does not support data type %s.",
                     GDALGetDataTypeName( eType ) );
            return NULL;
        }

        CPLError(CE_Warning, CPLE_AppDefined, 
                 "JP2KAK (JPEG2000) driver does not support data type %s, forcing to Float32.",
                 GDALGetDataTypeName( eType ) );
        
        eType = GDT_Float32;
    }

/* -------------------------------------------------------------------- */
/*      Do we want to write a pseudo-colored image?                     */
/* -------------------------------------------------------------------- */
    int bHaveCT = poPrototypeBand->GetColorTable() != NULL
        && poSrcDS->GetRasterCount() == 1;

/* -------------------------------------------------------------------- */
/*      How many layers?                                                */
/* -------------------------------------------------------------------- */
    int      layer_count = 12;

    if( CSLFetchNameValue(papszOptions,"LAYERS") != NULL )
        layer_count = atoi(CSLFetchNameValue(papszOptions,"LAYERS"));
    else if( CSLFetchNameValue(papszOptions,"Clayers") != NULL )
        layer_count = atoi(CSLFetchNameValue(papszOptions,"Clayers"));
    
/* -------------------------------------------------------------------- */
/*	Establish how many bytes of data we want for each layer.  	*/
/*	We take the quality as a percentage, so if QUALITY of 50 is	*/
/*	selected, we will set the base layer to 50% the default size.   */
/*	We let the other layers be computed internally.                 */
/* -------------------------------------------------------------------- */
    kdu_long *layer_bytes;
    double   dfQuality = 20.0;

    layer_bytes = (kdu_long *) CPLCalloc(sizeof(kdu_long),layer_count);

    if( CSLFetchNameValue(papszOptions,"QUALITY") != NULL )
    {
        dfQuality = atof(CSLFetchNameValue(papszOptions,"QUALITY"));
    }

    if( dfQuality < 1.0 || dfQuality > 100.0 )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "QUALITY=%s is not a legal value in the range 1-100.",
                  CSLFetchNameValue(papszOptions,"QUALITY") );
        return NULL;
    }

    if( dfQuality < 99.5 )
    {
        double dfLayerBytes = 
            (nXSize * ((double) nYSize) * dfQuality / 100.0);

        dfLayerBytes *= (GDALGetDataTypeSize(eType) / 8);
        dfLayerBytes *= GDALGetRasterCount(poSrcDS);

        if( dfLayerBytes > 2000000000.0 && sizeof(kdu_long) == 4 )
        {
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "Trimmming maximum size of file 2GB from %.1fGB\n"
                      "to avoid overflow of kdu_long layer size.",
                      dfLayerBytes / 1000000000.0 );
            dfLayerBytes = 2000000000.0;
        }

        layer_bytes[layer_count-1] = (kdu_long) dfLayerBytes;

        CPLDebug( "JP2KAK", "layer_bytes[] = %g\n", 
                  (double) layer_bytes[layer_count-1] );
    }
    else
        bReversible = true;

/* -------------------------------------------------------------------- */
/*      Do we want to use more than one tile?                           */
/* -------------------------------------------------------------------- */
    if( nTileXSize > 25000 )
    {
        // Don't generate tiles that are terrible wide by default, as
        // they consume alot of memory for the compression engine.
        nTileXSize = 20000;
    }

    if( CSLFetchNameValue( papszOptions, "BLOCKXSIZE" ) != NULL )
        nTileXSize = MIN(nXSize,
                         atoi(CSLFetchNameValue( papszOptions, "BLOCKXSIZE")));

    if( CSLFetchNameValue( papszOptions, "BLOCKYSIZE" ) != NULL )
        nTileYSize = MIN(nYSize,
                         atoi(CSLFetchNameValue( papszOptions, "BLOCKYSIZE")));

/* -------------------------------------------------------------------- */
/*      Establish the general image parameters.                         */
/* -------------------------------------------------------------------- */
    siz_params  oSizeParams;

    oSizeParams.set( Scomponents, 0, 0, poSrcDS->GetRasterCount() );
    oSizeParams.set( Sdims, 0, 0, nYSize );
    oSizeParams.set( Sdims, 0, 1, nXSize );
    oSizeParams.set( Sprecision, 0, 0, GDALGetDataTypeSize(eType) );
    if( eType == GDT_UInt16 || eType == GDT_Byte )
        oSizeParams.set( Ssigned, 0, 0, false );
    else
        oSizeParams.set( Ssigned, 0, 0, true );

    if( nTileXSize != nXSize || nTileYSize != nYSize )
    {
        oSizeParams.set( Stiles, 0, 0, nTileYSize );
        oSizeParams.set( Stiles, 0, 1, nTileXSize );
        
        CPLDebug( "JP2KAK", "Stiles=%d,%d", nTileYSize, nTileXSize );
    }

    kdu_params *poSizeRef = &oSizeParams; poSizeRef->finalize();

/* -------------------------------------------------------------------- */
/*      Open output file, and setup codestream.                         */
/* -------------------------------------------------------------------- */
#ifdef KAKADU4
    jp2_family_tgt         family;
#endif
#ifdef KAKADU42
    jpx_family_tgt         jpx_family;
    jpx_target             jpx_out;
    int			   bIsJPX = !EQUAL(CPLGetExtension(pszFilename),"jpf");
#else
    int                    bIsJPX = FALSE;
#endif

    kdu_compressed_target *poOutputFile = NULL;
    jp2_target             jp2_out;
    kdu_simple_file_target jpc_out;
    int                    bIsJP2 = !EQUAL(CPLGetExtension(pszFilename),"jpc")
        && !bIsJPX;
    kdu_codestream         oCodeStream;

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

    try
    {
        if( bIsJP2 )
        {
#ifdef KAKADU4
            family.open( pszFilename );

            jp2_out.open( &family );
#else
            jp2_out.open( pszFilename );
#endif
            poOutputFile = &jp2_out;
        }
#ifdef KAKADU42
        else if( bIsJPX )
        {
            jpx_family.open( pszFilename );

            jpx_out.open( &jpx_family );
            jpx_out.add_codestream();
        }
#endif
        else
        {
            jpc_out.open( pszFilename );
            poOutputFile = &jpc_out;
        }

        oCodeStream.create(&oSizeParams, poOutputFile );
    }
    catch( ... )
    {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Do we have a high res region of interest?                       */
/* -------------------------------------------------------------------- */
    kdu_roi_image  *poROIImage = NULL;
    char **papszROIDefs = CSLFetchNameValueMultiple( papszOptions, "ROI" );
    int iROI;
    
    for( iROI = 0; papszROIDefs != NULL && papszROIDefs[iROI] != NULL; iROI++ )
    {
        kdu_dims region;
        char **papszTokens = CSLTokenizeStringComplex( papszROIDefs[iROI], ",",
                                                       FALSE, FALSE );

        if( CSLCount(papszTokens) != 4 )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Skipping corrupt ROI def = \n%s", 
                      papszROIDefs[iROI] );
            continue;
        }

        region.pos.x = atoi(papszTokens[0]);
        region.pos.y = atoi(papszTokens[1]);
        region.size.x = atoi(papszTokens[2]);
        region.size.y = atoi(papszTokens[3]);

        CSLDestroy( papszTokens );

        poROIImage = new kdu_roi_rect(oCodeStream,region);
    }

/* -------------------------------------------------------------------- */
/*      Set some particular parameters.                                 */
/* -------------------------------------------------------------------- */
    oCodeStream.access_siz()->parse_string(
        CPLSPrintf("Clayers=%d",layer_count));
    oCodeStream.access_siz()->parse_string("Cycc=no");
    if( eType == GDT_Int16 || eType == GDT_UInt16 )
        oCodeStream.access_siz()->parse_string("Qstep=0.0000152588");
        
    if( bReversible )
        oCodeStream.access_siz()->parse_string("Creversible=yes");
    else
        oCodeStream.access_siz()->parse_string("Creversible=no");

/* -------------------------------------------------------------------- */
/*      Set some user-overridable parameters.                           */
/* -------------------------------------------------------------------- */
    int iParm;
    char *apszParms[] = { "Corder", "PCRL", 
                          "Cprecincts", "{512,512},{256,512},{128,512},{64,512},{32,512},{16,512},{8,512},{4,512},{2,512}",
                          "ORGgen_plt", "yes", 
                          "Cmodes", NULL, 
                          "Clevels", NULL,
                          "Rshift", NULL,
                          "Rlevels", NULL,
                          "Rweight", NULL,
                          NULL, NULL };

    for( iParm = 0; apszParms[iParm] != NULL; iParm += 2 )
    {
        const char *pszValue = 
            CSLFetchNameValue( papszOptions, apszParms[iParm] );
        
        if( pszValue == NULL )
            pszValue = apszParms[iParm+1];

        if( pszValue != NULL )
        {
            const char *pszOpt = 
                CPLSPrintf( "%s=%s", apszParms[iParm], pszValue );
            oCodeStream.access_siz()->parse_string( pszOpt );

            CPLDebug( "JP2KAK", "parse_string(%s)", pszOpt );
        }
    }

    oCodeStream.access_siz()->finalize_all();

/* -------------------------------------------------------------------- */
/*      Some JP2 specific parameters.                                   */
/* -------------------------------------------------------------------- */
    if( bIsJP2 )
    {
        // Set dimensional information (all redundant with the SIZ marker segment)
        jp2_dimensions dims = jp2_out.access_dimensions();
        dims.init(&oSizeParams);
        
        // Set colour space information (mandatory)
        jp2_colour colour = jp2_out.access_colour();

        if( bHaveCT || poSrcDS->GetRasterCount() == 3 )
            colour.init( JP2_sRGB_SPACE );
        else if( poSrcDS->GetRasterCount() >= 4 
                 && poSrcDS->GetRasterBand(4)->GetColorInterpretation() 
                 == GCI_AlphaBand )
        {
            colour.init( JP2_sRGB_SPACE );
            jp2_out.access_channels().init( 3 );
            jp2_out.access_channels().set_colour_mapping(0,0);
            jp2_out.access_channels().set_colour_mapping(1,1);
            jp2_out.access_channels().set_colour_mapping(2,2);
            jp2_out.access_channels().set_opacity_mapping(0,3);
            jp2_out.access_channels().set_opacity_mapping(1,3);
            jp2_out.access_channels().set_opacity_mapping(2,3);
        }
        else if( poSrcDS->GetRasterCount() >= 2
                 && poSrcDS->GetRasterBand(2)->GetColorInterpretation() 
                 == GCI_AlphaBand )
        {
            colour.init( JP2_sLUM_SPACE );
            jp2_out.access_channels().init( 1 );
            jp2_out.access_channels().set_colour_mapping(0,0);
            jp2_out.access_channels().set_opacity_mapping(0,1);
        }
        else
            colour.init( JP2_sLUM_SPACE );
    }

/* -------------------------------------------------------------------- */
/*      Write JP2 pseudocolor table if available.                       */
/* -------------------------------------------------------------------- */
    if( bIsJP2 && bHaveCT )
    {
        jp2_palette oJP2Palette;
        GDALColorTable *poCT = poPrototypeBand->GetColorTable();
        int  iColor, nCount = poCT->GetColorEntryCount();
        kdu_int32 *panLUT = (kdu_int32 *) 
            CPLMalloc(sizeof(kdu_int32) * nCount * 3);
        
        oJP2Palette = jp2_out.access_palette();
        oJP2Palette.init( 3, nCount );

        for( iColor = 0; iColor < nCount; iColor++ )
        {
            GDALColorEntry sEntry;

            poCT->GetColorEntryAsRGB( iColor, &sEntry );
            panLUT[iColor + nCount * 0] = sEntry.c1;
            panLUT[iColor + nCount * 1] = sEntry.c2;
            panLUT[iColor + nCount * 2] = sEntry.c3;
        }

        oJP2Palette.set_lut( 0, panLUT + nCount * 0, 8, false );
        oJP2Palette.set_lut( 1, panLUT + nCount * 1, 8, false );
        oJP2Palette.set_lut( 2, panLUT + nCount * 2, 8, false );

        CPLFree( panLUT );

        jp2_channels oJP2Channels = jp2_out.access_channels();

        oJP2Channels.init( 3 );
        oJP2Channels.set_colour_mapping( 0, 0, 0 );
        oJP2Channels.set_colour_mapping( 1, 0, 1 );
        oJP2Channels.set_colour_mapping( 2, 0, 2 );
    }

#ifdef KAKADU4
    if( bIsJP2 )
    {
        jp2_out.write_header();
    }
#endif

/* -------------------------------------------------------------------- */
/*      Set the GeoTIFF box if georeferencing is available, and this    */
/*      is a JP2 file.                                                  */
/* -------------------------------------------------------------------- */
    double	adfGeoTransform[6];
    if( bIsJP2
        && ((poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None
             && (adfGeoTransform[0] != 0.0 
                 || adfGeoTransform[1] != 1.0 
                 || adfGeoTransform[2] != 0.0 
                 || adfGeoTransform[3] != 0.0 
                 || adfGeoTransform[4] != 0.0 
                 || ABS(adfGeoTransform[5]) != 1.0))
            || poSrcDS->GetGCPCount() > 0) )
    {
        JP2KAKWriteGeoTIFFInfo( &jp2_out, poSrcDS );
    }

/* -------------------------------------------------------------------- */
/*      Open codestream box.                                            */
/* -------------------------------------------------------------------- */
#ifdef KAKADU4
    if( bIsJP2 )
        jp2_out.open_codestream();
#endif

/* -------------------------------------------------------------------- */
/*      Create one big tile, and a compressing engine, and line         */
/*      buffer for each component.                                      */
/* -------------------------------------------------------------------- */
    int iTileXOff, iTileYOff;
    double dfPixelsDone = 0.0;
    double dfPixelsTotal = nXSize * (double) nYSize;
    
    for( iTileYOff = 0; iTileYOff < nYSize; iTileYOff += nTileYSize )
    {
        for( iTileXOff = 0; iTileXOff < nXSize; iTileXOff += nTileXSize )
        {
            kdu_tile oTile = oCodeStream.open_tile(
                kdu_coords(iTileXOff/nTileXSize,iTileYOff/nTileYSize));
            int nThisTileXSize, nThisTileYSize;

            // ---------------------------------------------------------------
            // Is this a partial tile on the right or bottom?
            if( iTileXOff + nTileXSize < nXSize )
                nThisTileXSize = nTileXSize;
            else
                nThisTileXSize = nXSize - iTileXOff;
    
            if( iTileYOff + nTileYSize < nYSize )
                nThisTileYSize = nTileYSize;
            else
                nThisTileYSize = nYSize - iTileYOff;

            // ---------------------------------------------------------------
            // Setup scaled progress monitor

            void *pScaledProgressData;
            double dfPixelsDoneAfter = 
                dfPixelsDone + (nThisTileXSize * (double) nThisTileYSize);

            pScaledProgressData = 
                GDALCreateScaledProgress( dfPixelsDone / dfPixelsTotal,
                                          dfPixelsDoneAfter / dfPixelsTotal,
                                          pfnProgress, pProgressData );
            if( !JP2KAKCreateCopy_WriteTile( poSrcDS, oTile, poROIImage, 
                                             iTileXOff, iTileYOff, 
                                             nThisTileXSize, nThisTileYSize, 
                                             bReversible, eType,
                                             oCodeStream, bFlushEnabled,
                                             layer_bytes, layer_count,
                                             GDALScaledProgress, 
                                             pScaledProgressData ) )
            {
                GDALDestroyScaledProgress( pScaledProgressData );

                oCodeStream.destroy();
                poOutputFile->close();
                VSIUnlink( pszFilename );
                return NULL;
            }

            GDALDestroyScaledProgress( pScaledProgressData );
            dfPixelsDone = dfPixelsDoneAfter;

            oTile.close();
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Finish flushing out results.                                    */
/* -------------------------------------------------------------------- */
    oCodeStream.flush(layer_bytes, layer_count);
    oCodeStream.destroy();

    CPLFree( layer_bytes );

    if( bIsJP2 )
    {
        jp2_out.close();
#ifdef KAKADU4
        family.close();
#endif
    }
    else
    {
        poOutputFile->close();
    }

    if( !pfnProgress( 1.0, NULL, pProgressData ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Re-open dataset, and copy any auxilary pam information.         */
/* -------------------------------------------------------------------- */
    GDALPamDataset *poDS = (GDALPamDataset *) 
        GDALOpen( pszFilename, GA_ReadOnly );

    if( poDS )
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );

    return poDS;
}

/************************************************************************/
/*                        GDALRegister_JP2KAK()				*/
/************************************************************************/

void GDALRegister_JP2KAK()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "JP2KAK" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "JP2KAK" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "JPEG-2000 (based on Kakadu)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_jpeg2000.html" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte Int16 UInt16" );
        poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/jp2" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "jp2" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, 
"<CreationOptionList>"
"   <Option name='QUALITY' type='integer' description='1-100, 100 is lossless'/>"
"   <Option name='BLOCKXSIZE' type='int' description='Tile Width'/>"
"   <Option name='BLOCKYSIZE' type='int' description='Tile Height'/>"
"   <Option name='LAYERS' type='integer'/>"
"   <Option name='ROI' type='string'/>"
"   <Option name='Corder' type='string'/>"
"   <Option name='Cprecincts' type='string'/>"
"   <Option name='ORGgen_plt' type='string'/>"
"   <Option name='Cmodes' type='string'/>"
"   <Option name='Clevels' type='string'/>"
"   <Option name='Rshift' type='string'/>"
"   <Option name='Rlevels' type='string'/>"
"   <Option name='Rweight' type='string'/>"
"</CreationOptionList>" );

        poDriver->pfnOpen = JP2KAKDataset::Open;
        poDriver->pfnCreateCopy = JP2KAKCreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
