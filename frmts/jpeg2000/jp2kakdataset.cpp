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
 * Revision 1.1  2002/09/23 12:46:55  warmerda
 * New
 *
 */

#include "gdal_priv.h"

// Kakadu core includes
#include "kdu_elementary.h"
#include "kdu_messaging.h"
#include "kdu_params.h"
#include "kdu_compressed.h"
#include "kdu_sample_processing.h"

// Application level includes
#include "kdu_file_io.h"
#include "jp2.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_JP2KAK(void);
CPL_C_END

static int kakadu_initialized = FALSE;

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

    void flush(bool end_of_message=false) 
    {
        if( m_pszError == NULL )
            return;
        if( m_pszError[strlen(m_pszError)-1] == '\n' )
            m_pszError[strlen(m_pszError)-1] = '\0';

        CPLError( m_eErrClass, CPLE_AppDefined, "%s", m_pszError );
        CPLFree( m_pszError );
        m_pszError = NULL;
    }

private:
    CPLErr m_eErrClass;
    char *m_pszError;
};

/*****************************************************************************/
/*                              transfer_bytes                               */
/*****************************************************************************/

void
  transfer_bytes(kdu_byte *dest, kdu_line_buf &src, int gap, int precision)

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
      if (!src.is_absolute())
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
              kdu_int16 downshift = KDU_FIX_POINT-precision;
              kdu_int16 upshift = 8-precision;
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
              kdu_int16 downshift = precision-8;
              kdu_int16 offset = (1<<downshift)>>1;
              
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
              kdu_int16 upshift = 8-precision;

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
/* ==================================================================== */
/*				JP2KAKDataset				*/
/* ==================================================================== */
/************************************************************************/

class JP2KAKDataset : public GDALDataset
{
    friend class JP2KAKRasterBand;

    kdu_codestream oCodeStream;
    kdu_compressed_source *poInput;
    kdu_dims dims; 

  public:
                JP2KAKDataset();
		~JP2KAKDataset();
    
    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            JP2KAKRasterBand                        */
/* ==================================================================== */
/************************************************************************/

class JP2KAKRasterBand : public GDALRasterBand
{
    friend class JP2KAKDataset;

  public:

    		JP2KAKRasterBand( JP2KAKDataset *, int );
    		~JP2KAKRasterBand();
    
    virtual CPLErr IReadBlock( int, int, void * );

    void        ProcessTileYCbCr(kdu_tile tile, GByte *pabyBuffer, 
                                 int nTileXOff, int nTileYOff );
    void        ProcessTile(kdu_tile tile, GByte *pabyBuffer, 
                            int nTileXOff, int nTileYOff );
};

/************************************************************************/
/*                           JP2KAKRasterBand()                       */
/************************************************************************/

JP2KAKRasterBand::JP2KAKRasterBand( JP2KAKDataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;

    this->eDataType = GDT_Byte;

/* -------------------------------------------------------------------- */
/*      Use a 512x128 "virtual" block size unless the file is small.    */
/* -------------------------------------------------------------------- */
    if( poDS->GetRasterXSize() >= 1024 )
        nBlockXSize = 512;
    else
        nBlockXSize = poDS->GetRasterXSize();
    
    if( poDS->GetRasterYSize() >= 256 )
        nBlockYSize = 128;
    else
        nBlockYSize = poDS->GetRasterYSize();
}

/************************************************************************/
/*                         ~JP2KAKRasterBand()                        */
/************************************************************************/

JP2KAKRasterBand::~JP2KAKRasterBand()

{
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr JP2KAKRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                      void * pImage )
{
    JP2KAKDataset *poGDS = (JP2KAKDataset *) poDS;

    fprintf( stderr, "." );

/* -------------------------------------------------------------------- */
/*      Setup a ROI matching the block requested.                       */
/* -------------------------------------------------------------------- */
    kdu_dims dims = poGDS->dims;

    dims.pos.x = dims.pos.x + nBlockXOff * nBlockXSize;
    dims.pos.y = dims.pos.y + nBlockYOff * nBlockYSize;
    dims.size.x = nBlockXSize;
    dims.size.y = nBlockYSize;
    
    kdu_dims dims_roi;
    poGDS->oCodeStream.map_region( 0, dims, dims_roi );
    poGDS->oCodeStream.apply_input_restrictions( 0, 0, 0, 0, &dims_roi );
    
/* -------------------------------------------------------------------- */
/*      Now we are ready to walk through the tiles processing them      */
/*      one-by-one.                                                     */
/* -------------------------------------------------------------------- */
    kdu_dims tile_indices; 
    kdu_coords tpos;
    
    poGDS->oCodeStream.get_valid_tiles(tile_indices);
    
    for (tpos.y=0; tpos.y < tile_indices.size.y; tpos.y++)
    {
        for (tpos.x=0; tpos.x < tile_indices.size.x; tpos.x++)
        {
            kdu_tile tile = 
                poGDS->oCodeStream.open_tile(tpos+tile_indices.pos);

            kdu_resolution res = tile.access_component(0).access_resolution();
            kdu_dims tile_dims; res.get_dims(tile_dims);
            kdu_coords offset = tile_dims.pos - dims.pos;

            GByte *pabyDest;

            pabyDest = ((GByte *) pImage) + offset.x + offset.y * nBlockXSize;

            ProcessTile( tile, pabyDest, offset.x, offset.y );
            
            tile.close();
        }
    }

    return CE_None;
}

/************************************************************************/
/*                            ProcessTile()                             */
/*                                                                      */
/*      Process data from one component of one tile into working        */
/*      buffer.                                                         */
/************************************************************************/

void JP2KAKRasterBand::ProcessTile( kdu_tile tile, GByte *pabyDest, 
                                    int nTileXOff, int nTileYOff )

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
        transfer_bytes(pabyDest + y * nBlockXSize,line,1,bit_depth);
    }

    engine.destroy();
}

/************************************************************************/
/*                          ProcessTileYCbCr()                          */
/*                                                                      */
/*      Process a YCbCr tile.                                           */
/************************************************************************/

void JP2KAKRasterBand::ProcessTileYCbCr( kdu_tile tile, GByte *pabyDest, 
                                         int nTileXOff, int nTileYOff )

{
#ifdef notdef
    int c, num_components = tile.get_num_components(); 
    bool use_ycc = tile.get_ycc();
    JP2KAKDataset *poGDS = (JP2KAKDataset *) poDS;

    CPLAssert( num_components == poDS->GetRasterCount() );

    // Open tile-components and create processing engines and resources
    kdu_dims dims;
    kdu_sample_allocator allocator;
    kdu_tile_comp comps[3];
    kdu_line_buf lines[3];
    kdu_pull_ifc engines[3];
    bool reversible[3]; // Some components may be reversible and others not.
    int bit_depths[3]; // Original bit-depth may be quite different from 8.

    for (c=0; c < num_components; c++)
    {
        comps[c] = tile.access_component(c);
        reversible[c] = comps[c].get_reversible();
        bit_depths[c] = comps[c].get_bit_depth();
        kdu_resolution res = comps[c].access_resolution(); // Get top resolution
        kdu_dims comp_dims; res.get_dims(comp_dims);
        if (c == 0)
            dims = comp_dims;
        else
            assert(dims == comp_dims); // Safety check; the caller has ensured this
        bool use_shorts = (comps[c].get_bit_depth(true) <= 16);
        lines[c].pre_create(&allocator,dims.size.x,reversible[c],use_shorts);
        if (res.which() == 0) // No DWT levels used
            engines[c] =
                kdu_decoder(res.access_subband(LL_BAND),&allocator,use_shorts);
        else
            engines[c] = kdu_synthesis(res,&allocator,use_shorts);
    }

    allocator.finalize(); // Actually creates buffering resources
    for (c=0; c < num_components; c++)
        lines[c].create(); // Grabs resources from the allocator.

    // Now walk through the lines of the buffer, recovering them from the
    // relevant tile-component processing engines.

    for( int y = 0; y < dims.size.y; y++ )
    {
        for (c=0; c < num_components; c++)
            engines[c].pull(lines[c],true);

        if ((num_components >= 3) && use_ycc)
            kdu_convert_ycc_to_rgb(lines[0],lines[1],lines[2]);

        
        for (c=0; c < num_components; c++)
        {
            GDALRasterBandH hBand = GDALGetRasterBand( hDS, c+1 );

            transfer_bytes(pabyLine,lines[c],1,bit_depths[c]);

            GDALRasterIO( hBand, GF_Write, 
                          nTileXOff, nTileYOff + y, lines[c].get_width(), 1, 
                          pabyLine, lines[c].get_width(), 1, GDT_Byte, 0, 0 );
        }
    }

    // Cleanup
    for (c=0; c < num_components; c++)
        engines[c].destroy(); // engines are interfaces; no default destructors

    CPLFree( pabyLine );
#endif    
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
}

/************************************************************************/
/*                            ~JP2KAKDataset()                         */
/************************************************************************/

JP2KAKDataset::~JP2KAKDataset()

{
    if( poInput != NULL )
    {
        oCodeStream.destroy();
        poInput->close();
        delete poInput;
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *JP2KAKDataset::Open( GDALOpenInfo * poOpenInfo )

{
    const char  *pszExtension;

    if( poOpenInfo->fp == NULL )
        return NULL;
    
    pszExtension = CPLGetExtension( poOpenInfo->pszFilename );
    if( !EQUAL(pszExtension,"jpc") && !EQUAL(pszExtension,"j2k") 
        && !EQUAL(pszExtension,"jp2") && !EQUAL(pszExtension,"jpx") )
        return NULL;

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
/*      Try to open the file in a manner depending on the extension.    */
/* -------------------------------------------------------------------- */
    kdu_compressed_source *poInput;

    if( EQUAL(pszExtension,"jp2") || EQUAL(pszExtension,"jpx") )
    {
        jp2_source *jp2_src;

        jp2_src = new jp2_source;
        jp2_src->open( poOpenInfo->pszFilename, true );
        poInput = jp2_src;
    }
    else
        poInput = new kdu_simple_file_source( poOpenInfo->pszFilename );

    // we need to trap errors and handle here. 

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    JP2KAKDataset 	*poDS;

    poDS = new JP2KAKDataset();

    poDS->poInput = poInput;
    poDS->oCodeStream.create( poInput );
    poDS->oCodeStream.set_fussy();
    poDS->oCodeStream.set_persistent();

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
                poDS->oCodeStream.apply_input_restrictions(0,1,0,0,NULL);
                break;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int iBand;
    
    for( iBand = 1; iBand <= poDS->nBands; iBand++ )
    {
        poDS->SetBand( iBand, new JP2KAKRasterBand( poDS, iBand ) );
    }

    return( poDS );
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

        poDriver->pfnOpen = JP2KAKDataset::Open;
//        poDriver->pfnCreateCopy = JP2KAKCreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

