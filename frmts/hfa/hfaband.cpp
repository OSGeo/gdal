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
 * Revision 1.12  2000/10/31 19:13:53  warmerda
 * Look for overviews even if RRDNames exists and fails
 *
 * Revision 1.11  2000/10/31 18:02:32  warmerda
 * Added external and unnamed overview support
 *
 * Revision 1.10  2000/10/31 14:41:30  warmerda
 * avoid memory leaks and warnings
 *
 * Revision 1.9  2000/10/20 04:18:15  warmerda
 * added overviews, stateplane, and u4
 *
 * Revision 1.8  2000/10/12 19:30:32  warmerda
 * substantially improved write support
 *
 * Revision 1.7  2000/09/29 21:42:38  warmerda
 * preliminary write support implemented
 *
 * Revision 1.6  2000/08/10 14:39:47  warmerda
 * implemented compressed block support
 *
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

    nOverviews = 0;
    papoOverviews = NULL;

/* -------------------------------------------------------------------- */
/*      Does this band have overviews?  Try to find them.               */
/* -------------------------------------------------------------------- */
    HFAEntry	*poRRDNames = poNode->GetNamedChild( "RRDNamesList" );

    if( poRRDNames != NULL )
    {
        for( int iName = 0; TRUE; iName++ )
        {
            char	szField[128], *pszPath, *pszFilename, *pszEnd;
            const char *pszName;
            CPLErr      eErr;
            HFAEntry   *poOvEntry;
            int         i;
            HFAInfo_t	*psHFA;

            sprintf( szField, "nameList[%d].string", iName );

            pszName = poRRDNames->GetStringField( szField, &eErr );
            if( pszName == NULL || eErr != CE_None )
                break;

            pszFilename = CPLStrdup(pszName);
            pszEnd = strstr(pszFilename,"(:");
            if( pszEnd == NULL )
            {
                CPLFree( pszFilename );
                continue;
            }

            pszName = pszEnd + 2;
            pszEnd[0] = '\0';

            char	*pszJustFilename;

            pszJustFilename = CPLStrdup(CPLGetFilename(pszFilename));
            psHFA = HFAGetDependent( psInfo, pszJustFilename );
            CPLFree( pszJustFilename );
            if( psHFA == NULL )
            {
                CPLFree( pszFilename );
                continue;
            }

            pszPath = pszEnd + 2;
            if( pszPath[strlen(pszPath)-1] == ')' )
                pszPath[strlen(pszPath)-1] = '\0';

            for( i=0; pszPath[i] != '\0'; i++ )
            {
                if( pszPath[i] == ':' )
                    pszPath[i] = '.';
            }

            poOvEntry = psHFA->poRoot->GetNamedChild( pszPath );
            CPLFree( pszFilename );

            if( poOvEntry == NULL )
                continue;

            /* 
             * We have an overview node.  Instanatiate a HFABand from it, 
             * and add to the list.
             */
            papoOverviews = (HFABand **) 
                CPLRealloc(papoOverviews, sizeof(void*) * ++nOverviews );
            papoOverviews[nOverviews-1] = new HFABand( psHFA, poOvEntry );
        }
    }

/* -------------------------------------------------------------------- */
/*      If there are no named overviews, try looking for unnamed        */
/*      overviews within the same layer, as occurs in floodplain.img    */
/*      for instance.                                                   */
/* -------------------------------------------------------------------- */
    if( nOverviews == 0 )
    {
        HFAEntry	*poChild;

        for( poChild = poNode->GetChild(); 
             poChild != NULL;
             poChild = poChild->GetNext() ) 
        {
            if( EQUAL(poChild->GetType(),"Eimg_Layer_SubSample") )
            {
                papoOverviews = (HFABand **) 
                    CPLRealloc(papoOverviews, sizeof(void*) * ++nOverviews );
                papoOverviews[nOverviews-1] = new HFABand( psInfo, poChild );
            }
        }
    }
}

/************************************************************************/
/*                              ~HFABand()                              */
/************************************************************************/

HFABand::~HFABand()

{
    for( int iOverview = 0; iOverview < nOverviews; iOverview++ )
        delete papoOverviews[iOverview];

    if( nOverviews > 0 )
        CPLFree( papoOverviews );

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
/*                          UncompressBlock()                           */
/*                                                                      */
/*      Uncompress ESRI Grid compression format block.                  */
/************************************************************************/

static CPLErr UncompressBlock( GByte *pabyCData, int nSrcBytes,
                               GByte *pabyDest, int nMaxPixels, 
                               int nDataType )

{
    GUInt32  nDataMin, nDataOffset;
    int      nNumBits, nPixelsOutput=0;			
    GInt32   nNumRuns;

    memcpy( &nDataMin, pabyCData, 4 );
    nDataMin = CPL_LSBWORD32( nDataMin );
        
    memcpy( &nNumRuns, pabyCData+4, 4 );
    nNumRuns = CPL_LSBWORD32( nNumRuns );
        
    memcpy( &nDataOffset, pabyCData+8, 4 );
    nDataOffset = CPL_LSBWORD32( nDataOffset );

    nNumBits = pabyCData[12];

/* -------------------------------------------------------------------- */
/*	Establish data pointers.					*/    
/* -------------------------------------------------------------------- */
    GByte *pabyCounter, *pabyValues;
    int   nValueBitOffset;

    pabyCounter = pabyCData + 13;
    pabyValues = pabyCData + nDataOffset;
    nValueBitOffset = 0;
    
/* ==================================================================== */
/*      Loop over runs.                                                 */
/* ==================================================================== */
    int    iRun;

    for( iRun = 0; iRun < nNumRuns; iRun++ )
    {
        int	nRepeatCount = 0;
        int	nDataValue;

/* -------------------------------------------------------------------- */
/*      Get the repeat count.  This can be stored as one, two, three    */
/*      or four bytes depending on the low order two bits of the        */
/*      first byte.                                                     */
/* -------------------------------------------------------------------- */
        if( ((*pabyCounter) & 0xc0) == 0x00 )
        {
            nRepeatCount = (*(pabyCounter++)) & 0x3f;
        }
        else if( ((*pabyCounter) & 0xc0) == 0x40 )
        {
            nRepeatCount = (*(pabyCounter++)) & 0x3f;
            nRepeatCount = nRepeatCount * 256 + (*(pabyCounter++));
        }
        else if( ((*pabyCounter) & 0xc0) == 0x80 )
        {
            nRepeatCount = (*(pabyCounter++)) & 0x3f;
            nRepeatCount = nRepeatCount * 256 + (*(pabyCounter++));
            nRepeatCount = nRepeatCount * 256 + (*(pabyCounter++));
        }
        else if( ((*pabyCounter) & 0xc0) == 0xc0 )
        {
            nRepeatCount = (*(pabyCounter++)) & 0x3f;
            nRepeatCount = nRepeatCount * 256 + (*(pabyCounter++));
            nRepeatCount = nRepeatCount * 256 + (*(pabyCounter++));
            nRepeatCount = nRepeatCount * 256 + (*(pabyCounter++));
        }

/* -------------------------------------------------------------------- */
/*      Extract the data value in a way that depends on the number      */
/*      of bits in it.                                                  */
/* -------------------------------------------------------------------- */
        if( nNumBits == 0 )
        {
            nDataValue = 0;
        }
        else if( nNumBits == 1 )
        {
            nDataValue =
                (pabyValues[nValueBitOffset>>3] >> (nValueBitOffset&7)) & 0x1;
            nValueBitOffset++;
        }
        else if( nNumBits == 2 )
        {
            nDataValue =
                (pabyValues[nValueBitOffset>>3] >> (nValueBitOffset&7)) & 0x3;
            nValueBitOffset += 2;
        }
        else if( nNumBits == 4 )
        {
            nDataValue =
                (pabyValues[nValueBitOffset>>3] >> (nValueBitOffset&7)) & 0xf;
            nValueBitOffset += 4;
        }
        else if( nNumBits == 8 )
        {
            nDataValue = *pabyValues;
            pabyValues++;
        }
        else if( nNumBits == 16 )
        {
            nDataValue = 256 * *(pabyValues++);
            nDataValue += *(pabyValues++);
        }
        else if( nNumBits == 32 )
        {
            nDataValue = 256 * 256 * 256 * *(pabyValues++);
            nDataValue = 256 * 256 * *(pabyValues++);
            nDataValue = 256 * *(pabyValues++);
            nDataValue = *(pabyValues++);
        }
        else
        {
            printf( "nNumBits = %d\n", nNumBits );
            CPLAssert( FALSE );
            nDataValue = 0;
        }

/* -------------------------------------------------------------------- */
/*      Offset by the minimum value.                                    */
/* -------------------------------------------------------------------- */
        nDataValue += nDataMin;

/* -------------------------------------------------------------------- */
/*      Now apply to the output buffer in a type specific way.          */
/* -------------------------------------------------------------------- */
        if( nPixelsOutput + nRepeatCount > nMaxPixels )
        {
            CPLAssert( FALSE );
            nRepeatCount = nMaxPixels - nPixelsOutput;
        }
        
        if( nDataType == EPT_u8 )
        {
            int		i;
            
            for( i = 0; i < nRepeatCount; i++ )
            {
                CPLAssert( nDataValue < 256 );
                ((GByte *) pabyDest)[nPixelsOutput++] = nDataValue;
            }
        }
        else if( nDataType == EPT_u16 )
        {
            int		i;
            
            for( i = 0; i < nRepeatCount; i++ )
            {
                ((GUInt16 *) pabyDest)[nPixelsOutput++] = nDataValue;
            }
        }
        else if( nDataType == EPT_s16 )
        {
            int		i;
            
            for( i = 0; i < nRepeatCount; i++ )
            {
                ((GInt16 *) pabyDest)[nPixelsOutput++] = nDataValue;
            }
        }
        else if( nDataType == EPT_f32 )
        {
            int		i;

            for( i = 0; i < nRepeatCount; i++ )
            {
                ((float *) pabyDest)[nPixelsOutput++] = (float) nDataValue;
            }
        }
        else
        {
            CPLAssert( FALSE );
        }
    }

    return CE_None;
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
    if( !panBlockFlag[iBlock] & BFLG_VALID )
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
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Seek to %d failed.\n", panBlockStart[iBlock] );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*	If the block is compressed, read into an intermediate buffer	*/
/*	and convert.							*/
/* -------------------------------------------------------------------- */
    if( panBlockFlag[iBlock] & BFLG_COMPRESSED )
    {
        GByte 	*pabyCData;
        CPLErr  eErr;

        pabyCData = (GByte *) CPLMalloc(panBlockSize[iBlock]);

        if( VSIFRead( pabyCData, panBlockSize[iBlock], 1, psInfo->fp ) != 1 )
        {
            CPLError( CE_Failure, CPLE_FileIO, 
                      "Read of %d bytes at %d failed.\n", 
                      panBlockSize[iBlock],
                      panBlockStart[iBlock] );

            return CE_Failure;
        }

        eErr = UncompressBlock( pabyCData, panBlockSize[iBlock],
                                (GByte *) pData, nBlockXSize*nBlockYSize, 
                                nDataType );

        CPLFree( pabyCData );

        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Read uncompressed data directly into the return buffer.         */
/* -------------------------------------------------------------------- */
    if( VSIFRead( pData, panBlockSize[iBlock], 1, psInfo->fp ) != 1 )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Byte swap to local byte order if required.  It appears that     */
/*      raster data is always stored in Intel byte order in Imagine     */
/*      files.                                                          */
/* -------------------------------------------------------------------- */

#ifdef CPL_MSB             
    if( HFAGetDataTypeBits(nDataType) == 16 )
    {
        for( int ii = 0; ii < nBlockXSize*nBlockYSize; ii++ )
            CPL_SWAP16PTR( ((unsigned char *) pData) + ii*2 );
    }
    else if( HFAGetDataTypeBits(nDataType) == 32 )
    {
        for( int ii = 0; ii < nBlockXSize*nBlockYSize; ii++ )
            CPL_SWAP32PTR( ((unsigned char *) pData) + ii*4 );
    }
    else if( nDataType == EPT_f64 )
    {
        for( int ii = 0; ii < nBlockXSize*nBlockYSize; ii++ )
            CPL_SWAP64PTR( ((unsigned char *) pData) + ii*8 );
    }
    else if( nDataType == EPT_c64 )
    {
        for( int ii = 0; ii < nBlockXSize*nBlockYSize*2; ii++ )
            CPL_SWAP32PTR( ((unsigned char *) pData) + ii*4 );
    }
    else if( nDataType == EPT_c128 )
    {
        for( int ii = 0; ii < nBlockXSize*nBlockYSize*2; ii++ )
            CPL_SWAP64PTR( ((unsigned char *) pData) + ii*8 );
    }
#endif /* def CPL_MSB */

    return( CE_None );
}

/************************************************************************/
/*                           SetRasterBlock()                           */
/************************************************************************/

CPLErr HFABand::SetRasterBlock( int nXBlock, int nYBlock, void * pData )

{
    int		iBlock;

    if( LoadBlockInfo() != CE_None )
        return CE_Failure;

    iBlock = nXBlock + nYBlock * nBlocksPerRow;
    
    if( (panBlockFlag[iBlock] & (BFLG_VALID|BFLG_COMPRESSED)) == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
          "Attempt to write to invalid, or compressed tile.  This\n"
          "operation currently unsupported by HFABand::SetRasterBlock().\n" );

        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Move to the location that the data sits.                        */
/* -------------------------------------------------------------------- */
    if( VSIFSeek( psInfo->fp, panBlockStart[iBlock], SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Seek to %d failed.\n", panBlockStart[iBlock] );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Byte swap to local byte order if required.  It appears that     */
/*      raster data is always stored in Intel byte order in Imagine     */
/*      files.                                                          */
/* -------------------------------------------------------------------- */

#ifdef CPL_MSB             
    if( HFAGetDataTypeBits(nDataType) == 16 )
    {
        for( int ii = 0; ii < nBlockXSize*nBlockYSize; ii++ )
            CPL_SWAP16PTR( ((unsigned char *) pData) + ii*2 );
    }
    else if( HFAGetDataTypeBits(nDataType) == 32 )
    {
        for( int ii = 0; ii < nBlockXSize*nBlockYSize; ii++ )
            CPL_SWAP32PTR( ((unsigned char *) pData) + ii*4 );
    }
    else if( nDataType == EPT_f64 )
    {
        for( int ii = 0; ii < nBlockXSize*nBlockYSize; ii++ )
            CPL_SWAP64PTR( ((unsigned char *) pData) + ii*8 );
    }
    else if( nDataType == EPT_c64 )
    {
        for( int ii = 0; ii < nBlockXSize*nBlockYSize*2; ii++ )
            CPL_SWAP32PTR( ((unsigned char *) pData) + ii*4 );
    }
    else if( nDataType == EPT_c128 )
    {
        for( int ii = 0; ii < nBlockXSize*nBlockYSize*2; ii++ )
            CPL_SWAP64PTR( ((unsigned char *) pData) + ii*8 );
    }
#endif /* def CPL_MSB */

/* -------------------------------------------------------------------- */
/*      Write uncompressed data.				        */
/* -------------------------------------------------------------------- */
    if( VSIFWrite( pData, panBlockSize[iBlock], 1, psInfo->fp ) != 1 )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Swap back, since we don't really have permission to change      */
/*      the callers buffer.                                             */
/* -------------------------------------------------------------------- */

#ifdef CPL_MSB             
    if( HFAGetDataTypeBits(nDataType) == 16 )
    {
        for( int ii = 0; ii < nBlockXSize*nBlockYSize; ii++ )
            CPL_SWAP16PTR( ((unsigned char *) pData) + ii*2 );
    }
    else if( HFAGetDataTypeBits(nDataType) == 32 )
    {
        for( int ii = 0; ii < nBlockXSize*nBlockYSize; ii++ )
            CPL_SWAP32PTR( ((unsigned char *) pData) + ii*4 );
    }
    else if( nDataType == EPT_f64 )
    {
        for( int ii = 0; ii < nBlockXSize*nBlockYSize; ii++ )
            CPL_SWAP64PTR( ((unsigned char *) pData) + ii*8 );
    }
    else if( nDataType == EPT_c64 )
    {
        for( int ii = 0; ii < nBlockXSize*nBlockYSize*2; ii++ )
            CPL_SWAP32PTR( ((unsigned char *) pData) + ii*4 );
    }
    else if( nDataType == EPT_c128 )
    {
        for( int ii = 0; ii < nBlockXSize*nBlockYSize*2; ii++ )
            CPL_SWAP64PTR( ((unsigned char *) pData) + ii*8 );
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

/************************************************************************/
/*                               SetPCT()                               */
/*                                                                      */
/*      Set the PCT information for this band.                          */
/************************************************************************/

CPLErr HFABand::SetPCT( int nColors,
                        double *padfRed,
                        double *padfGreen,
                        double *padfBlue )

{

    if( nColors == 0 )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Create the Descriptor table.                                    */
/* -------------------------------------------------------------------- */
    if( TRUE )
    {
        HFAEntry	*poEdsc_Table;

        poEdsc_Table = new HFAEntry( psInfo, "Descriptor_Table", "Edsc_Table",
                                     poNode );

        poEdsc_Table->SetIntField( "numrows", nColors );

/* -------------------------------------------------------------------- */
/*      Create the Binning function node.  I am not sure that we        */
/*      really need this though.                                        */
/* -------------------------------------------------------------------- */
        HFAEntry       *poEdsc_BinFunction;

        poEdsc_BinFunction = 
            new HFAEntry( psInfo, "#Bin_Function#", "Edsc_BinFunction",
                          poEdsc_Table );

        poEdsc_BinFunction->SetIntField( "numBins", 256 );
        poEdsc_BinFunction->SetStringField( "binFunction", "direct" );
        poEdsc_BinFunction->SetDoubleField( "minLimit", 0.0 );
        poEdsc_BinFunction->SetDoubleField( "maxLimit", 255.0 );

/* -------------------------------------------------------------------- */
/*      Process each color component                                    */
/* -------------------------------------------------------------------- */
        for( int iColumn = 0; iColumn < 3; iColumn++ )
        {
            HFAEntry        *poEdsc_Column;
            double	    *padfValues=NULL;
            const char      *pszName=NULL;
            
            if( iColumn == 0 )
            {
                pszName = "Red";
                padfValues = padfRed;
            }
            else if( iColumn == 1 )
            {
                pszName = "Green";
                padfValues = padfGreen;
            }
            else if( iColumn == 2 )
            {
                pszName = "Blue";
                padfValues = padfBlue;
                
            }

/* -------------------------------------------------------------------- */
/*      Create the Edsc_Column.                                         */
/* -------------------------------------------------------------------- */
            poEdsc_Column = new HFAEntry( psInfo, pszName, "Edsc_Column", 
                                          poEdsc_Table );
            poEdsc_Column->SetIntField( "numRows", nColors );
            poEdsc_Column->SetStringField( "dataType", "real" );
            poEdsc_Column->SetIntField( "maxNumChars", 0 );

/* -------------------------------------------------------------------- */
/*      Write the data out.                                             */
/* -------------------------------------------------------------------- */
            int		nOffset = HFAAllocateSpace( psInfo, 8*nColors);
            double      *padfFileData;

            poEdsc_Column->SetIntField( "columnDataPtr", nOffset );

            padfFileData = (double *) CPLMalloc(nColors*sizeof(double));
            for( int iColor = 0; iColor < nColors; iColor++ )
            {
                padfFileData[iColor] = padfValues[iColor];
                HFAStandard( 8, padfFileData + iColor );
            }
            VSIFSeek( psInfo->fp, nOffset, SEEK_SET );
            VSIFWrite( padfFileData, 8, nColors, psInfo->fp );
            CPLFree( padfFileData );
        }

/* -------------------------------------------------------------------- */
/*      Update the layer type to be thematic.                           */
/* -------------------------------------------------------------------- */
        poNode->SetStringField( "layerType", "thematic" );
    }

    return( CE_None );
}
