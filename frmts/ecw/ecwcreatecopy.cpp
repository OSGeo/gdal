/******************************************************************************
 * $Id$
 *
 * Project:  GDAL ECW Driver
 * Purpose:  ECW CreateCopy method implementation.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, 2004, Frank Warmerdam <warmerdam@pobox.com>
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
 *****************************************************************************
 *
 * $Log$
 * Revision 1.2  2004/12/10 22:13:58  fwarmerdam
 * convert to unix format
 *
 * Revision 1.1  2004/12/10 19:15:52  fwarmerdam
 * New
 *
 */

#include "gdal_priv.h"
#include "ogr_spatialref.h"
#include "cpl_string.h"
#include "cpl_conv.h"
#include "vsiiostream.h"

CPL_CVSID("$Id$");

#if defined(FRMT_ecw) && defined(HAVE_COMPRESS)

class GDALECWCompressor : public CNCSFile {

public:
    GDALECWCompressor();
    virtual ~GDALECWCompressor();

    virtual CNCSError WriteReadLine(UINT32 nNextLine, void **ppInputArray);
    virtual void WriteStatus(UINT32 nCurrentLine);
    virtual bool WriteCancel();

    // Data

    GDALDataset *m_poSrcDS;

    VSIIOStream m_OStream;
    int m_nPercentComplete;

    int m_bCancelled;

    GDALProgressFunc  pfnProgress;
    void             *pProgressData;

    NCSFileViewFileInfoEx sFileInfo;
};

/************************************************************************/
/*                         GDALECWCompressor()                          */
/************************************************************************/

GDALECWCompressor::GDALECWCompressor()

{
    m_poSrcDS = NULL;
    m_nPercentComplete = -1;
    m_bCancelled = FALSE;
    pfnProgress = GDALDummyProgress;
    pProgressData = NULL;
}

/************************************************************************/
/*                         ~GDALECWCompressor()                         */
/************************************************************************/

GDALECWCompressor::~GDALECWCompressor()

{
}

/************************************************************************/
/*                           WriteReadLine()                            */
/************************************************************************/

CNCSError GDALECWCompressor::WriteReadLine( UINT32 nNextLine, 
                                            void **ppInputArray )

{
    int                  iBand;

    for( iBand = 0; iBand < (int) sFileInfo.nBands; iBand++ )
    {
        GDALRasterBand      *poBand;

        poBand = m_poSrcDS->GetRasterBand( iBand+1 );

        if( poBand->RasterIO( GF_Read, 0, nNextLine, poBand->GetXSize(), 1, 
                              ppInputArray[iBand], poBand->GetXSize(), 1, 
                              GDT_Byte, 0, 0 ) != CE_None )
            return NCS_FILE_IO_ERROR;
    }

    return NCS_SUCCESS;
}

/************************************************************************/
/*                            WriteStatus()                             */
/************************************************************************/

void GDALECWCompressor::WriteStatus( UINT32 nCurrentLine )

{
    m_bCancelled = 
        !pfnProgress( nCurrentLine / (float) sFileInfo.nSizeY, 
                      NULL, pProgressData );
}

/************************************************************************/
/*                            WriteCancel()                             */
/************************************************************************/

bool GDALECWCompressor::WriteCancel()

{
    return (bool) m_bCancelled;
}

/************************************************************************/
/*                           ECWCreateCopy()                            */
/************************************************************************/

GDALDataset *
ECWCreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                 int bStrict, char ** papszOptions, 
                 GDALProgressFunc pfnProgress, void * pProgressData )

{
    int  nBands = poSrcDS->GetRasterCount();
    int  nXSize = poSrcDS->GetRasterXSize();
    int  nYSize = poSrcDS->GetRasterYSize();

/* -------------------------------------------------------------------- */
/*      Do some rudimentary checking in input.                          */
/* -------------------------------------------------------------------- */
    if( nBands == 0 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "ECW driver requires at least one band as input." );
        return NULL;
    }

    if( nXSize < 128 || nYSize < 128 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "ECW driver requires image to be at least 128x128,\n"
                  "the source image is %dx%d.\n", 
                  nXSize, nYSize );
        return NULL;
    }

    if( poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte 
        && bStrict )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "CW driver doesn't support data type %s. "
                  "Only eight bit bands supported.\n", 
                  GDALGetDataTypeName( 
                      poSrcDS->GetRasterBand(1)->GetRasterDataType()) );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Parse out some known options.                                   */
/* -------------------------------------------------------------------- */
    float      fTargetCompression = 75.0;

    if( CSLFetchNameValue(papszOptions, "TARGET") != NULL )
    {
        fTargetCompression = (float) 
            atof(CSLFetchNameValue(papszOptions, "TARGET"));
        
        if( fTargetCompression < 1.1 || fTargetCompression > 100.0 )
        {
            CPLError( CE_Failure, CPLE_NotSupported, 
                      "TARGET compression of %.3f invalid, should be a\n"
                      "value between 1 and 100 percent.\n", 
                      (double) fTargetCompression );
            return NULL;
        }
    }
        
/* -------------------------------------------------------------------- */
/*      Create and initialize compressor.                               */
/* -------------------------------------------------------------------- */
    GDALECWCompressor         oCompressor;
    NCSFileViewFileInfoEx    *psClient = &(oCompressor.sFileInfo);
    
    psClient->nBands = nBands;
    psClient->nSizeX = nXSize;
    psClient->nSizeY = nYSize;
    psClient->nCompressionRate = (int) MAX(1,100 / (100-fTargetCompression));
    psClient->szDatum = "RAW";
    psClient->szProjection = "RAW";
    psClient->eCellSizeUnits = ECW_CELL_UNITS_METERS;

    psClient->eCellType = NCSCT_UINT8;
    if( nBands == 1 )
        psClient->eColorSpace = NCSCS_GREYSCALE;
    else if( nBands == 3 )
        psClient->eColorSpace = NCSCS_sRGB;
    else
        psClient->eColorSpace = NCSCS_MULTIBAND;

    oCompressor.pfnProgress = pfnProgress;
    oCompressor.pProgressData = pProgressData;
    oCompressor.m_poSrcDS = poSrcDS;

/* -------------------------------------------------------------------- */
/*      Create band information structures.                             */
/* -------------------------------------------------------------------- */
    NCSFileBandInfo asBandInfos[200];
    int iBand;

    psClient->pBands = asBandInfos;
    for( iBand = 0; iBand < nBands; iBand++ )
    {
        asBandInfos[iBand].nBits = 8;
        asBandInfos[iBand].bSigned = 0;
        asBandInfos[iBand].szDesc = CPLStrdup(
            CPLSPrintf("Band%d",iBand+1) );
    }

/* -------------------------------------------------------------------- */
/*      Allow CNCSFile::SetParameter() requests.                        */
/* -------------------------------------------------------------------- */
    const char *pszOption;

    pszOption = CSLFetchNameValue(papszOptions, "PROFILE");
    if( pszOption != NULL && EQUAL(pszOption,"BASELINE_0") ) 
        oCompressor.SetParameter( 
            CNCSJP2FileView::JP2_COMPRESS_PROFILE_BASELINE_0 );
    else if( pszOption != NULL && EQUAL(pszOption,"BASELINE_1") ) 
        oCompressor.SetParameter( 
            CNCSJP2FileView::JP2_COMPRESS_PROFILE_BASELINE_1 );
    else if( pszOption != NULL && EQUAL(pszOption,"BASELINE_2") )
        oCompressor.SetParameter( 
            CNCSJP2FileView::JP2_COMPRESS_PROFILE_BASELINE_2 );
    else if( pszOption != NULL && EQUAL(pszOption,"NPJE") ) 
        oCompressor.SetParameter( 
            CNCSJP2FileView::JP2_COMPRESS_PROFILE_NITF_BIIF_NPJE );
    else if( pszOption != NULL && EQUAL(pszOption,"EPJE") ) 
        oCompressor.SetParameter( 
            CNCSJP2FileView::JP2_COMPRESS_PROFILE_NITF_BIIF_EPJE );

    pszOption = CSLFetchNameValue(papszOptions, "CODESTREAM_ONLY" );
    if( pszOption != NULL ) 
        oCompressor.SetParameter(
            CNCSJP2FileView::JP2_COMPRESS_CODESTREAM_ONLY, 
            (bool) CSLTestBoolean( pszOption ) );

    pszOption = CSLFetchNameValue(papszOptions, "LEVELS");
    if( pszOption != NULL )
        oCompressor.SetParameter( CNCSJP2FileView::JP2_COMPRESS_LEVELS, 
                                  (UINT32) atoi(pszOption) );

    pszOption = CSLFetchNameValue(papszOptions, "LAYERS");
    if( pszOption != NULL )
        oCompressor.SetParameter( CNCSJP2FileView::JP2_COMPRESS_LAYERS, 
                                  (UINT32) atoi(pszOption) );

    pszOption = CSLFetchNameValue(papszOptions, "PRECINCT_WIDTH");
    if( pszOption != NULL )
        oCompressor.SetParameter( CNCSJP2FileView::JP2_COMPRESS_PRECINCT_WIDTH,
                                  (UINT32) atoi(pszOption) );

    pszOption = CSLFetchNameValue(papszOptions, "PRECINCT_HEIGHT");
    if( pszOption != NULL )
        oCompressor.SetParameter(CNCSJP2FileView::JP2_COMPRESS_PRECINCT_HEIGHT, 
                                 (UINT32) atoi(pszOption) );

    pszOption = CSLFetchNameValue(papszOptions, "TILE_WIDTH");
    if( pszOption != NULL )
        oCompressor.SetParameter( CNCSJP2FileView::JP2_COMPRESS_TILE_WIDTH, 
                                  (UINT32) atoi(pszOption) );

    pszOption = CSLFetchNameValue(papszOptions, "TILE_HEIGHT");
    if( pszOption != NULL )
        oCompressor.SetParameter( CNCSJP2FileView::JP2_COMPRESS_TILE_HEIGHT, 
                                  (UINT32) atoi(pszOption) );

    pszOption = CSLFetchNameValue(papszOptions, "INCLUDE_SOP");
    if( pszOption != NULL )
        oCompressor.SetParameter( CNCSJP2FileView::JP2_COMPRESS_INCLUDE_SOP, 
                                  (bool) CSLTestBoolean( pszOption ) );
    
    pszOption = CSLFetchNameValue(papszOptions, "INCLUDE_EPH");
    if( pszOption != NULL )
        oCompressor.SetParameter( CNCSJP2FileView::JP2_COMPRESS_INCLUDE_EPH, 
                                  (bool) CSLTestBoolean( pszOption ) );
    
    pszOption = CSLFetchNameValue(papszOptions, "PROGRESSION");
    if( pszOption != NULL && EQUAL(pszOption,"LRCP") )
        oCompressor.SetParameter( 
            CNCSJP2FileView::JP2_COMPRESS_PROGRESSION_LRCP );
                                  
    else if( pszOption != NULL && EQUAL(pszOption,"RLCP") )
        oCompressor.SetParameter( 
            CNCSJP2FileView::JP2_COMPRESS_PROGRESSION_RLCP );

    else if( pszOption != NULL && EQUAL(pszOption,"RPCL") )
        oCompressor.SetParameter( 
            CNCSJP2FileView::JP2_COMPRESS_PROGRESSION_RPCL );

    // JP2_GEODATA_USAGE? 

    pszOption = CSLFetchNameValue(papszOptions, "DECOMPRESS_LAYERS");
    if( pszOption != NULL )
        oCompressor.SetParameter( 
            CNCSJP2FileView::JP2_DECOMPRESS_LAYERS, 
            (UINT32) atoi(pszOption) );

    pszOption = CSLFetchNameValue(papszOptions, 
                                  "DECOMPRESS_RECONSTRUCTION_PARAMETER");
    if( pszOption != NULL )
        oCompressor.SetParameter( 
            CNCSJP2FileView::JPC_DECOMPRESS_RECONSTRUCTION_PARAMETER, 
            (IEEE4) atof(pszOption) );
                                  
/* -------------------------------------------------------------------- */
/*      Set block size if desired.                                      */
/* -------------------------------------------------------------------- */
#ifdef notdef
    psClient->nBlockSizeX = 256;
    psClient->nBlockSizeY = 256;
    if( CSLFetchNameValue(papszOptions, "BLOCKSIZE") != NULL )
    {
        psClient->nBlockSizeX = atoi(CSLFetchNameValue(papszOptions, 
                                                       "BLOCKSIZE"));
        psClient->nBlockSizeY = atoi(CSLFetchNameValue(papszOptions, 
                                                       "BLOCKSIZE"));
    }
        
    if( CSLFetchNameValue(papszOptions, "BLOCKXSIZE") != NULL )
        psClient->nBlockSizeX = atoi(CSLFetchNameValue(papszOptions, 
                                                       "BLOCKXSIZE"));
        
    if( CSLFetchNameValue(papszOptions, "BLOCKYSIZE") != NULL )
        psClient->nBlockSizeX = atoi(CSLFetchNameValue(papszOptions, 
                                                       "BLOCKYSIZE"));
#endif        
/* -------------------------------------------------------------------- */
/*      Georeferencing.                                                 */
/* -------------------------------------------------------------------- */
    double      adfGeoTransform[6];

    psClient->fOriginX = 0.0;
    psClient->fOriginY = psClient->nSizeY;
    psClient->fCellIncrementX = 1.0;
    psClient->fCellIncrementY = -1.0;
    psClient->fCWRotationDegrees = 0.0;
    if( poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None )
    {
        if( adfGeoTransform[2] != 0.0 || adfGeoTransform[4] != 0.0 )
            CPLError( CE_Warning, CPLE_NotSupported, 
                      "Rotational coefficients ignored, georeferencing of\n"
                      "output ECW file will be incorrect.\n" );
        else
        {
            psClient->fOriginX = adfGeoTransform[0];
            psClient->fOriginY = adfGeoTransform[3];
            psClient->fCellIncrementX = adfGeoTransform[1];
            psClient->fCellIncrementY = adfGeoTransform[5];
        }
    }

/* -------------------------------------------------------------------- */
/*      Projection.                                                     */
/* -------------------------------------------------------------------- */
    /* TODO */

/* -------------------------------------------------------------------- */
/*      Handle special case of a JPEG2000 data stream in another file.  */
/* -------------------------------------------------------------------- */
    FILE *fpVSIL = NULL;

    if( EQUALN(pszFilename,"J2K_SUBFILE:",12) )
    {
        int  subfile_offset=-1, subfile_size=-1;
        char *real_filename = NULL;

        if( sscanf( pszFilename, "J2K_SUBFILE:%d,%d", 
                    &subfile_offset, &subfile_size ) != 2 )
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Failed to parse J2K_SUBFILE specification." );
            return NULL;
        }

        real_filename = strstr(pszFilename,",");
        if( real_filename != NULL )
            real_filename = strstr(real_filename+1,",");
        if( real_filename != NULL )
            real_filename = real_filename++;
        else
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Failed to parse J2K_SUBFILE specification." );
            return NULL;
        }

        fpVSIL = VSIFOpenL( real_filename, "rb+" );
        if( fpVSIL == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Failed to open %s.",  real_filename );
            return NULL;
        }

        oCompressor.m_OStream.Access( fpVSIL, TRUE, real_filename,
                                      subfile_offset, subfile_size );
    }

/* -------------------------------------------------------------------- */
/*      Start the compression.                                          */
/* -------------------------------------------------------------------- */
    CNCSError oError;

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

    oError = oCompressor.SetFileInfo( oCompressor.sFileInfo );

    if( oError.GetErrorNumber() == NCS_SUCCESS )
    {
        if( fpVSIL == NULL )
            oError = oCompressor.Open( (char *) pszFilename, false, true );
        else
            oError = oCompressor.CNCSJP2FileView::Open( 
                &(oCompressor.m_OStream) );
    }

    if( oError.GetErrorNumber() == NCS_SUCCESS )
        oCompressor.Write();

    oCompressor.Close( true );

/* -------------------------------------------------------------------- */
/*      Cleanup, and return read-only handle.                           */
/* -------------------------------------------------------------------- */
    if( fpVSIL != NULL )
        VSIFCloseL( fpVSIL );

    if( oError.GetErrorNumber() != NCS_SUCCESS )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "ECW Driver file write failed.\n%s", 
                  oError.GetErrorMessage() );
        return NULL;
    }
    else
    {
        pfnProgress( 1.001, NULL, pProgressData );
        return (GDALDataset *) GDALOpen( pszFilename, GA_ReadOnly );
    }
}

#endif /* def FRMT_ecw && def HAVE_COMPRESS */
