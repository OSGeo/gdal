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
 * Revision 1.4  2002/10/21 18:06:57  warmerda
 * fixed multi-component write support
 *
 * Revision 1.3  2002/10/10 21:07:06  warmerda
 * added support for writing GeoJP2 section
 *
 * Revision 1.2  2002/10/08 23:02:39  warmerda
 * added create support, and GeoJP2 read support
 *
 * Revision 1.1  2002/09/23 12:46:55  warmerda
 * New
 *
 */

#include "gdal_priv.h"
#include "cpl_string.h"
#include "jp2_local.h"

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
transfer_bytes(kdu_byte *dest, kdu_line_buf &src, int gap, int precision);

static const unsigned int jp2_uuid_box_type = 0x75756964;

static unsigned char msi_uuid2[16] =
	{0xb1,0x4b,0xf8,0xbd,0x08,0x3d,0x4b,0x43,
         0xa5,0xae,0x8c,0xd7,0xd5,0xa6,0xce,0x03}; 

/************************************************************************/
/* ==================================================================== */
/*				JP2KAKDataset				*/
/* ==================================================================== */
/************************************************************************/

class JP2KAKDataset : public GDALDataset
{
    kdu_codestream oCodeStream;
    kdu_compressed_source *poInput;
    kdu_dims dims; 

    char	   *pszProjection;
    double	   adfGeoTransform[6];
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


    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            JP2KAKRasterBand                          */
/* ==================================================================== */
/************************************************************************/

class JP2KAKRasterBand : public GDALRasterBand
{
    friend class JP2KAKDataset;

    int         nDiscardLevels; 

    kdu_dims 	band_dims; 

    int		nOverviewCount;
    JP2KAKRasterBand **papoOverviewBand;

    kdu_codestream oCodeStream;

  public:

    		JP2KAKRasterBand( int, int, kdu_codestream );
    		~JP2KAKRasterBand();
    
    virtual CPLErr IReadBlock( int, int, void * );

    virtual int    GetOverviewCount();
    virtual GDALRasterBand *GetOverview( int );

    // internal

    void        ProcessTileYCbCr(kdu_tile tile, GByte *pabyBuffer, 
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
                                    kdu_codestream oCodeStream )

{
    this->nBand = nBand;

    this->eDataType = GDT_Byte;

    this->nDiscardLevels = nDiscardLevels;
    this->oCodeStream = oCodeStream;

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
/*      Do we have any overviews?  Only check if we are the full res    */
/*      image.                                                          */
/* -------------------------------------------------------------------- */
    nOverviewCount = 0;
    papoOverviewBand = 0;

    if( nDiscardLevels == 0 )
    {
        int  nXSize = nRasterXSize, nYSize = nRasterYSize;

        for( int nDiscard = 1; nDiscard < 5; nDiscard++ )
        {
            kdu_dims  dims;

            nXSize = (nXSize+1) / 2;
            nYSize = (nYSize+1) / 2;

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
                    new JP2KAKRasterBand( nBand, nDiscard, oCodeStream );
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
/*      Now we are ready to walk through the tiles processing them      */
/*      one-by-one.                                                     */
/* -------------------------------------------------------------------- */
        kdu_dims tile_indices; 
        kdu_coords tpos;
    
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

                pabyDest = ((GByte *)pImage) + offset.x + offset.y*nBlockXSize;

                ProcessTile( tile, pabyDest, offset.x, offset.y );
            
                tile.close();
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
    pszProjection = NULL;
    nGCPCount = 0;
    pasGCPList = NULL;

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
    CPLFree( pszProjection );

    if( poInput != NULL )
    {
        oCodeStream.destroy();
        poInput->close();
        delete poInput;
    }
}


/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *JP2KAKDataset::GetProjectionRef()

{
    return( pszProjection );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr JP2KAKDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );

    return CE_None;
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

    try
    {
        if( EQUAL(pszExtension,"jp2") || EQUAL(pszExtension,"jpx") )
        {
            jp2_source *jp2_src;
            
            jp2_src = new jp2_source;
            jp2_src->open( poOpenInfo->pszFilename, true );
            poInput = jp2_src;
        }
        else
            poInput = new kdu_simple_file_source( poOpenInfo->pszFilename );
    }
    catch( ... )
    {
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
            poDS->SetBand( iBand, 
                           new JP2KAKRasterBand(iBand,0,poDS->oCodeStream) );
        }

/* -------------------------------------------------------------------- */
/*      Try to read any available georeferencing info in a "geotiff"    */
/*      box".                                                           */
/* -------------------------------------------------------------------- */
        jp2_input_box   oBox;
        unsigned char  *pabyGTData = NULL;
        int		nGTDataSize = 0;

        VSIFSeek( poOpenInfo->fp, 0, SEEK_SET );
        while( oBox.open(poOpenInfo->fp).exists() 
               && oBox.get_remaining_bytes() != -1 )
        {
            if( oBox.get_box_type() == 0x75756964 /* UUID */ )
            {
                unsigned char uuid2[16];
                oBox.read( uuid2, 16 );
                if( memcmp( uuid2, msi_uuid2, 16 ) == 0 )
                {
                    nGTDataSize = oBox.get_remaining_bytes();

                    pabyGTData = (unsigned char *) CPLMalloc(nGTDataSize);
                    oBox.read( pabyGTData, nGTDataSize );
                }
            }
            oBox.close();
        }

        oBox.close();

/* -------------------------------------------------------------------- */
/*      Try to turn the geotiff block into projection and geotransform. */
/* -------------------------------------------------------------------- */
        if( pabyGTData != NULL )
        {
            GTIFWktFromMemBuf( nGTDataSize, pabyGTData, 
                               &(poDS->pszProjection), poDS->adfGeoTransform,
                               &(poDS->nGCPCount), &(poDS->pasGCPList) );
            CPLDebug("GDAL", "Got projection: %s", poDS->pszProjection );
            CPLFree( pabyGTData );
            pabyGTData = NULL;
        }

        if( poDS->pszProjection == NULL )
            poDS->pszProjection = CPLStrdup("");
        
        return( poDS );
    }

/* -------------------------------------------------------------------- */
/*      Catch all fatal kakadu errors and cleanup a bit.                */
/* -------------------------------------------------------------------- */
    catch( ... )
    {
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
/*                       JP2KAKWriteGeoTIFFInfo()                       */
/************************************************************************/

void JP2KAKWriteGeoTIFFInfo( jp2_target jp2_out, GDALDataset *poSrcDS )

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
    jp2_output_box &uuid_box = jp2_out.open_box( jp2_uuid_box_type );

    uuid_box.write( (kdu_byte *) msi_uuid2, sizeof(msi_uuid2) );

    uuid_box.write( (kdu_byte *) pabyGTBuf, nGTBufSize );

    uuid_box.close();

    CPLFree( pabyGTBuf );
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

static GDALDataset *
JP2KAKCopyCreate( const char * pszFilename, GDALDataset *poSrcDS, 
                  int bStrict, char ** papszOptions, 
                  GDALProgressFunc pfnProgress, void * pProgressData )

{
    int	   nXSize = poSrcDS->GetRasterXSize();
    int    nYSize = poSrcDS->GetRasterYSize();
    bool   bReversible = false;

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
/*	Establish how many bytes of data we want for each layer.  	*/
/*	We take the quality as a percentage, so if QUALITY of 50 is	*/
/*	selected, we will set the base layer to 50% the default size.   */
/*	We let the other layers be computed internally.                 */
/* -------------------------------------------------------------------- */
    kdu_long layer_bytes[12] = {0,0,0,0,0,0,0,0,0,0,0,0};
    double   dfQuality = 20.0;

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
        layer_bytes[11] = (kdu_long) (nXSize * nYSize * dfQuality / 100.0);
    }
    else
        bReversible = true;

/* -------------------------------------------------------------------- */
/*      Establish the general image parameters.                         */
/* -------------------------------------------------------------------- */
    siz_params  oSizeParams;

    oSizeParams.set( Scomponents, 0, 0, poSrcDS->GetRasterCount() );
    oSizeParams.set( Sdims, 0, 0, nYSize );
    oSizeParams.set( Sdims, 0, 1, nXSize );
    oSizeParams.set( Sprecision, 0, 0, 8 );
    oSizeParams.set( Ssigned, 0, 0, false );

    kdu_params *poSizeRef = &oSizeParams; poSizeRef->finalize();

/* -------------------------------------------------------------------- */
/*      Open output file, and setup codestream.                         */
/* -------------------------------------------------------------------- */
    kdu_compressed_target *poOutputFile = NULL;
    jp2_target             jp2_out;
    kdu_simple_file_target jpc_out;
    int                    bIsJP2 = !EQUAL(CPLGetExtension(pszFilename),"jpc");
    kdu_codestream         oCodeStream;

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

    try
    {
        if( bIsJP2 )
        {
            jp2_out.open( pszFilename );
            poOutputFile = &jp2_out;
        }
        else
        {
            jpc_out.open( pszFilename );
            poOutputFile = &jp2_out;
        }

        oCodeStream.create(&oSizeParams, poOutputFile );
    }
    catch( ... )
    {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Set some additional parameters.                                 */
/* -------------------------------------------------------------------- */
    oCodeStream.access_siz()->parse_string("Clayers=12");
    oCodeStream.access_siz()->parse_string("Cycc=no");
    if( bReversible )
        oCodeStream.access_siz()->parse_string("Creversible=yes");
    else
        oCodeStream.access_siz()->parse_string("Creversible=no");
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
        
        if( poSrcDS->GetRasterCount() == 3 )
            colour.init( JP2_sRGB_SPACE );
        else
            colour.init( JP2_sLUM_SPACE );
    }

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
        JP2KAKWriteGeoTIFFInfo( jp2_out, poSrcDS );
    }

/* -------------------------------------------------------------------- */
/*      Create the image as one big tile.                               */
/* -------------------------------------------------------------------- */
    kdu_tile oTile = oCodeStream.open_tile(kdu_coords(0,0));
    GByte *pabyBuffer = (GByte *) CPLMalloc(nXSize);

    int c, num_components = oTile.get_num_components(); 

    CPLAssert( !oTile.get_ycc() );

    for (c=0; c < num_components; c++)
    {
        kdu_dims dims;
        kdu_sample_allocator allocator;
        kdu_tile_comp comp;
        kdu_line_buf line;
        kdu_push_ifc engine;
        int          iLine;
        GDALRasterBand *poBand = poSrcDS->GetRasterBand( c+1 );

        comp = oTile.access_component(c);

        kdu_resolution res = comp.access_resolution(); // Get top resolution
        res.get_dims(dims);

        CPLAssert( dims.size.y == nYSize );
        CPLAssert( dims.size.x == nXSize );

        line.pre_create(&allocator,dims.size.x,bReversible,bReversible);

        //engine = kdu_encoder(res.access_subband(LL_BAND),&allocator,true);
        engine = kdu_analysis(res,&allocator,bReversible);

        allocator.finalize(); // Actually creates buffering resources

        line.create(); // Grabs resources from the allocator.

        // Now walk through the lines of the buffer, pushing them into the
        // relevant tile-component processing engines.
        for( iLine = 0; iLine < nYSize; iLine++ )
        {
            poBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                              (void *) pabyBuffer, nXSize, 1, GDT_Byte,
                              0, 0 );

            if( bReversible )
            {
                kdu_sample16 *dest = line.get_buf16();
                kdu_byte *sp = pabyBuffer;
                
                for (int n=dims.size.x; n > 0; n--, dest++, sp++)
                    dest->ival = ((kdu_int16)(*sp)) - 128;
            }
            else
            {
                kdu_sample32 *dest = line.get_buf32();
                kdu_byte *sp = pabyBuffer;
                
                for (int n=dims.size.x; n > 0; n--, dest++, sp++)
                    dest->fval = ((((kdu_int16)(*sp))-128.0) * 0.00390625);
            }

            engine.push(line,true);

            if( !pfnProgress( (c*nYSize+iLine)
                              / ((double)num_components*nYSize),
                              NULL, pProgressData ) )
            {
                oCodeStream.destroy();
                poOutputFile->close();
                VSIUnlink( pszFilename );
                return NULL;
            }
        }

        engine.destroy();
    }

/* -------------------------------------------------------------------- */
/*      Finish flushing out results.                                    */
/* -------------------------------------------------------------------- */
    CPLFree( pabyBuffer );

    oTile.close();
    
    oCodeStream.flush(layer_bytes, 12);
    oCodeStream.destroy();

    poOutputFile->close();

    if( !pfnProgress( 1.0, NULL, pProgressData ) )
        return NULL;

    return (GDALDataset *) GDALOpen( pszFilename, GA_ReadOnly );;
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
        poDriver->pfnCreateCopy = JP2KAKCopyCreate;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
