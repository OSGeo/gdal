/******************************************************************************
 * $Id$
 *
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Implementation of the HFABand, for accessing one Eimg_Layer.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Intergraph Corporation
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
 * Revision 1.5  1999/04/23 13:43:09  warmerda
 * Fixed up MSB case
 *
 * Revision 1.4  1999/03/08 19:23:18  warmerda
 * Added logic to byte swap block data on MSB systems.  Note that only 16 bit
 * data is currently handled.  This should be extended to larger data types
 * (32bit float, 64 bit float).
 *
 * Revision 1.3  1999/02/15 19:32:34  warmerda
 * Zero out compressed or invalid blocks.
 *
 * Revision 1.2  1999/01/28 18:02:42  warmerda
 * Byte swapping fix with PCTs
 *
 * Revision 1.1  1999/01/22 17:41:34  warmerda
 * New
 *
 */

#include "hfa_p.h"
#include "cpl_conv.h"

/************************************************************************/
/*                              HFABand()                               */
/************************************************************************/

HFABand::HFABand( HFAInfo_t * psInfoIn, HFAEntry * poNodeIn )

{
    psInfo = psInfoIn;
    poNode = poNodeIn;

    nBlockXSize = poNodeIn->GetIntField( "blockWidth" );
    nBlockYSize = poNodeIn->GetIntField( "blockHeight" );
    nDataType = poNodeIn->GetIntField( "pixelType" );

    nWidth = poNodeIn->GetIntField( "width" );
    nHeight = poNodeIn->GetIntField( "height" );

    nBlocksPerRow = (nWidth + nBlockXSize - 1) / nBlockXSize;
    nBlocksPerColumn = (nHeight + nBlockYSize - 1) / nBlockYSize;

    nBlocks = nBlocksPerRow * nBlocksPerColumn;
    panBlockStart = NULL;
    panBlockSize = NULL;
    panBlockFlag = NULL;

    nPCTColors = -1;
    apadfPCT[0] = apadfPCT[1] = apadfPCT[2] = NULL;
}

/************************************************************************/
/*                              ~HFABand()                              */
/************************************************************************/

HFABand::~HFABand()

{
    CPLFree( panBlockStart );
    CPLFree( panBlockSize );
    CPLFree( panBlockFlag );

    CPLFree( apadfPCT[0] );
    CPLFree( apadfPCT[1] );
    CPLFree( apadfPCT[2] );
}

/************************************************************************/
/*                            LoadBlockMap()                            */
/************************************************************************/

CPLErr	HFABand::LoadBlockInfo()

{
    int		iBlock;
    HFAEntry	*poDMS;
    
    if( panBlockStart != NULL )
        return( CE_None );

    poDMS = poNode->GetNamedChild( "RasterDMS" );
    if( poDMS == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
               "Can't find RasterDMS field in Eimg_Layer with block list.\n");
        return CE_Failure;
    }

    panBlockStart = (int *) CPLMalloc(sizeof(int) * nBlocks);
    panBlockSize = (int *) CPLMalloc(sizeof(int) * nBlocks);
    panBlockFlag = (int *) CPLMalloc(sizeof(int) * nBlocks);

    for( iBlock = 0; iBlock < nBlocks; iBlock++ )
    {
        char	szVarName[64];
        int	nLogvalid, nCompressType;

        sprintf( szVarName, "blockinfo[%d].offset", iBlock );
        panBlockStart[iBlock] = poDMS->GetIntField( szVarName );
        
        sprintf( szVarName, "blockinfo[%d].size", iBlock );
        panBlockSize[iBlock] = poDMS->GetIntField( szVarName );
        
        sprintf( szVarName, "blockinfo[%d].logvalid", iBlock );
        nLogvalid = poDMS->GetIntField( szVarName );
        
        sprintf( szVarName, "blockinfo[%d].compressionType", iBlock );
        nCompressType = poDMS->GetIntField( szVarName );

        panBlockFlag[iBlock] = 0;
        if( nLogvalid )
            panBlockFlag[iBlock] |= BFLG_VALID;
        if( nCompressType != 0 )
            panBlockFlag[iBlock] |= BFLG_COMPRESSED;
    }

    return( CE_None );
}

/************************************************************************/
/*                           GetRasterBlock()                           */
/************************************************************************/

CPLErr HFABand::GetRasterBlock( int nXBlock, int nYBlock, void * pData )

{
    int		iBlock;

    if( LoadBlockInfo() != CE_None )
        return CE_Failure;

    iBlock = nXBlock + nYBlock * nBlocksPerRow;
    
/* -------------------------------------------------------------------- */
/*      If the block isn't valid, or is compressed we just return       */
/*      all zeros, and an indication of failure.                        */
/* -------------------------------------------------------------------- */
    if( !(panBlockFlag[iBlock] & (BFLG_VALID|BFLG_COMPRESSED)) )
    {
        int	nBytes;

        nBytes = HFAGetDataTypeBits(nDataType)*nBlockXSize*nBlockYSize/8;

        while( nBytes > 0 )
            ((GByte *) pData)[--nBytes] = 0;
        
        return( CE_Failure );
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we really read the data.                              */
/* -------------------------------------------------------------------- */
    if( VSIFSeek( psInfo->fp, panBlockStart[iBlock], SEEK_SET ) != 0 )
        return CE_Failure;

    if( VSIFRead( pData, panBlockSize[iBlock], 1, psInfo->fp ) == 0 )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Byte swap to local byte order if required.  It appears that     */
/*      raster data is always stored in Intel byte order in Imagine     */
/*      files.                                                          */
/* -------------------------------------------------------------------- */

#ifdef CPL_MSB             
    if( HFAGetDataTypeBits(nDataType) == 16 )
    {
        int		ii;

        for( ii = 0; ii < nBlockXSize*nBlockYSize; ii++ )
        {
            unsigned char *pabyData = (unsigned char *) pData;
            int		nTemp;

            nTemp = pabyData[ii*2];
            pabyData[ii*2] = pabyData[ii*2+1];
            pabyData[ii*2+1] = nTemp;
        }
    }
#endif /* def CPL_MSB */

    return( CE_None );
}

/************************************************************************/
/*                               GetPCT()                               */
/*                                                                      */
/*      Return PCT information, if any exists.                          */
/************************************************************************/

CPLErr HFABand::GetPCT( int * pnColors,
                        double **ppadfRed,
                        double **ppadfGreen,
                        double **ppadfBlue )

{
    *pnColors = 0;
    *ppadfRed = NULL;
    *ppadfGreen = NULL;
    *ppadfBlue = NULL;
        
/* -------------------------------------------------------------------- */
/*      If we haven't already tried to load the colors, do so now.      */
/* -------------------------------------------------------------------- */
    if( nPCTColors == -1 )
    {
        HFAEntry	*poColumnEntry;
        int		i, iColumn;

        nPCTColors = 0;

        poColumnEntry = poNode->GetNamedChild("Descriptor_Table.Red");
        if( poColumnEntry == NULL )
            return( CE_Failure );

        nPCTColors = poColumnEntry->GetIntField( "numRows" );
        for( iColumn = 0; iColumn < 3; iColumn++ )
        {
            apadfPCT[iColumn] = (double *)CPLMalloc(sizeof(double)*nPCTColors);
            if( iColumn == 0 )
                poColumnEntry = poNode->GetNamedChild("Descriptor_Table.Red");
            else if( iColumn == 1 )
                poColumnEntry= poNode->GetNamedChild("Descriptor_Table.Green");
            else if( iColumn == 2 )
                poColumnEntry = poNode->GetNamedChild("Descriptor_Table.Blue");


            VSIFSeek( psInfo->fp, poColumnEntry->GetIntField("columnDataPtr"),
                      SEEK_SET );
            VSIFRead( apadfPCT[iColumn], sizeof(double), nPCTColors,
                      psInfo->fp);

            for( i = 0; i < nPCTColors; i++ )
                HFAStandard( 8, apadfPCT[iColumn] + i );
        }
    }

/* -------------------------------------------------------------------- */
/*      Return the values.                                              */
/* -------------------------------------------------------------------- */
    if( nPCTColors == 0 )
        return( CE_Failure );

    *pnColors = nPCTColors;
    *ppadfRed = apadfPCT[0];
    *ppadfGreen = apadfPCT[1];
    *ppadfBlue = apadfPCT[2];
    
    return( CE_None );
}
