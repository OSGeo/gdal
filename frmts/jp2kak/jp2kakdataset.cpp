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
 * Revision 1.1  2003/01/30 19:09:24  warmerda
 * New
 *
 * Revision 1.9  2002/11/30 16:54:46  warmerda
 * ensure we don't initialize from missing resolution levels
 *
 * Revision 1.8  2002/11/23 18:54:17  warmerda
 * added CREATIONDATATYPES metadata for drivers
 *
 * Revision 1.7  2002/11/21 15:34:09  warmerda
 * Made substantial improvements to improve flushing ability.
 *
 * Revision 1.6  2002/11/03 01:38:20  warmerda
 * Implemented YCbCr read support.
 *
 * Revision 1.5  2002/10/31 18:51:05  warmerda
 * added 16bit and 32bit floating point support
 *
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
transfer_bytes(kdu_byte *dest, kdu_line_buf &src, int gap, int precision,
               GDALDataType eOutType );

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

    		JP2KAKRasterBand( int, int, kdu_codestream, int );
    		~JP2KAKRasterBand();
    
    virtual CPLErr IReadBlock( int, int, void * );

    virtual int    GetOverviewCount();
    virtual GDALRasterBand *GetOverview( int );

    // internal

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
                                    int nResCount )

{
    this->nBand = nBand;

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
                    new JP2KAKRasterBand( nBand, nDiscard, oCodeStream, 0 );
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

                if( tile.get_ycc() && nBand < 4 )
                    ProcessYCbCrTile( tile, pabyDest, offset.x, offset.y );
                else
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
                                         int nTileXOff, int nTileYOff )

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
            poDS->SetBand( iBand, 
                           new JP2KAKRasterBand(iBand,0,poDS->oCodeStream,
                                                nResCount) );
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
                    *((GInt16 *) dest) = MAX(MIN(val,32767),-32768);
                }
                else if( eOutType == GDT_UInt16 )
                {
                    val = (int) (sp->fval*scale16) + 32768;
                    *((GUInt16 *) dest) = MAX(MIN(val,65535),0);
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
/*      How many layers?                                                */
/* -------------------------------------------------------------------- */
    int      layer_count;

    if( CSLFetchNameValue(papszOptions,"LAYERS") == NULL )
        layer_count = 12;
    else
        layer_count = atoi(CSLFetchNameValue(papszOptions,"LAYERS"));
    
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

    if( dfQuality < 99.5 || eType != GDT_Byte )
    {
        layer_bytes[layer_count-1] = 
            (kdu_long) (nXSize * nYSize * dfQuality / 100.0);
        layer_bytes[layer_count-1] *= (GDALGetDataTypeSize(eType) / 8);
        layer_bytes[layer_count-1] *= GDALGetRasterCount(poSrcDS);
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
    oSizeParams.set( Sprecision, 0, 0, GDALGetDataTypeSize(eType) );
    if( eType == GDT_UInt16 || eType == GDT_Byte )
        oSizeParams.set( Ssigned, 0, 0, false );
    else
        oSizeParams.set( Ssigned, 0, 0, true );

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
    oCodeStream.access_siz()->parse_string(
        CPLSPrintf("Clayers=%d",layer_count));
    oCodeStream.access_siz()->parse_string("Cycc=no");
    if( eType == GDT_Int16 || eType == GDT_UInt16 )
        oCodeStream.access_siz()->parse_string("Qstep=0.0000152588");
        
    if( bReversible )
        oCodeStream.access_siz()->parse_string("Creversible=yes");
    else
        oCodeStream.access_siz()->parse_string("Creversible=no");

    oCodeStream.access_siz()->parse_string("ORGgen-plt=yes");
    oCodeStream.access_siz()->parse_string("Corder=PCRL");
    oCodeStream.access_siz()->parse_string("Cprecincts={512,512},{256,512},{128,512},{64,512},{32,512},{16,512},{8,512},{4,512},{2,512}");

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
/*      Create one big tile, and a compressing engine, and line         */
/*      buffer for each component.                                      */
/* -------------------------------------------------------------------- */
    kdu_tile oTile = oCodeStream.open_tile(kdu_coords(0,0));
    int c, num_components = oTile.get_num_components(); 
    kdu_push_ifc  *engines = new kdu_push_ifc[num_components];
    kdu_line_buf  *lines = new kdu_line_buf[num_components];
    kdu_sample_allocator allocator;

    for (c=0; c < num_components; c++)
    {
        kdu_resolution res = oTile.access_component(c).access_resolution(); 

        lines[c].pre_create(&allocator,nXSize,bReversible,bReversible);
        engines[c] = kdu_analysis(res,&allocator,bReversible);
    }

    allocator.finalize();

    for (c=0; c < num_components; c++)
        lines[c].create();

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
                poBand->RasterIO( GF_Read, 0, iSubline, nXSize, 1, 
                                  (void *) pabyBuffer, nXSize, 1, eType,
                                  0, 0 );

                if( bReversible )
                {
                    kdu_sample16 *dest = lines[c].get_buf16();
                    kdu_byte *sp = pabyBuffer;
                
                    for (int n=nXSize; n > 0; n--, dest++, sp++)
                        dest->ival = ((kdu_int16)(*sp)) - 128;
                }
                else if( eType == GDT_Byte )
                {
                    kdu_sample32 *dest = lines[c].get_buf32();
                    kdu_byte *sp = pabyBuffer;
                
                    for (int n=nXSize; n > 0; n--, dest++, sp++)
                        dest->fval = ((((kdu_int16)(*sp))-128.0) * 0.00390625);
                }
                else if( eType == GDT_Int16 )
                {
                    kdu_sample32 *dest = lines[c].get_buf32();
                    GInt16  *sp = (GInt16 *) pabyBuffer;
                
                    for (int n=nXSize; n > 0; n--, dest++, sp++)
                        dest->fval = (((kdu_int16)(*sp)) * 0.0000152588);
                }
                else if( eType == GDT_UInt16 )
                {
                    kdu_sample32 *dest = lines[c].get_buf32();
                    GUInt16  *sp = (GUInt16 *) pabyBuffer;
                
                    for (int n=nXSize; n > 0; n--, dest++, sp++)
                        dest->fval = (((int)(*sp) - 32768) * 0.0000152588);
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
                    oCodeStream.destroy();
                    poOutputFile->close();
                    VSIUnlink( pszFilename );
                    return NULL;
                }
            }
        }

        if( oCodeStream.ready_for_flush() )			
        {
            CPLDebug( "JP2KAK", 
                      "Calling oCodeStream.flush() at line %d",
                      MIN(nYSize,iLine+CHUNK_SIZE) );
            
            oCodeStream.flush( layer_bytes, layer_count );
        }
        else
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

/* -------------------------------------------------------------------- */
/*      Finish flushing out results.                                    */
/* -------------------------------------------------------------------- */
    oTile.close();
    
    oCodeStream.flush(layer_bytes, layer_count);
    oCodeStream.destroy();

    CPLFree( layer_bytes );

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
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte Int16 UInt16" );

        poDriver->pfnOpen = JP2KAKDataset::Open;
        poDriver->pfnCreateCopy = JP2KAKCopyCreate;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
