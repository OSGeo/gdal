/******************************************************************************
 * $Id$
 *
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Implementation of the HFABand, for accessing one Eimg_Layer.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 ****************************************************************************/

#include "hfa_p.h"
#include "cpl_conv.h"

/* include the compression code */

CPL_CVSID("$Id$");

/************************************************************************/
/*                              HFABand()                               */
/************************************************************************/

HFABand::HFABand( HFAInfo_t * psInfoIn, HFAEntry * poNodeIn )

{
    psInfo = psInfoIn;
    poNode = poNodeIn;

    bOverviewsPending = TRUE;

    nBlockXSize = poNodeIn->GetIntField( "blockWidth" );
    nBlockYSize = poNodeIn->GetIntField( "blockHeight" );
    nDataType = poNodeIn->GetIntField( "pixelType" );

    nWidth = poNodeIn->GetIntField( "width" );
    nHeight = poNodeIn->GetIntField( "height" );

    panBlockStart = NULL;
    panBlockSize = NULL;
    panBlockFlag = NULL;

    nPCTColors = -1;
    apadfPCT[0] = apadfPCT[1] = apadfPCT[2] = apadfPCT[3] = NULL;
    padfPCTBins = NULL;

    nOverviews = 0;
    papoOverviews = NULL;

    fpExternal = NULL;

    bNoDataSet = FALSE;
    dfNoData = 0.0;

    if (nWidth <= 0 || nHeight <= 0 || nBlockXSize <= 0 || nBlockYSize <= 0)
    {
        nWidth = nHeight = 0;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "HFABand::HFABand : (nWidth <= 0 || nHeight <= 0 || nBlockXSize <= 0 || nBlockYSize <= 0)");
        return;
    }
    if (HFAGetDataTypeBits(nDataType) == 0)
    {
        nWidth = nHeight = 0;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "HFABand::HFABand : nDataType=%d unhandled", nDataType);
        return;
    }

    /* FIXME? : risk of overflow in additions and multiplication */
    nBlocksPerRow = (nWidth + nBlockXSize - 1) / nBlockXSize;
    nBlocksPerColumn = (nHeight + nBlockYSize - 1) / nBlockYSize;
    nBlocks = nBlocksPerRow * nBlocksPerColumn;

/* -------------------------------------------------------------------- */
/*      Check for nodata.  This is really an RDO (ESRI Raster Data      */
/*      Objects?), not used by Imagine itself.                          */
/* -------------------------------------------------------------------- */
    HFAEntry	*poNDNode = poNode->GetNamedChild("Eimg_NonInitializedValue");
    
    if( poNDNode != NULL )
    {
        bNoDataSet = TRUE;
        dfNoData = poNDNode->GetDoubleField( "valueBD" );
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

    if ( panBlockStart )
        CPLFree( panBlockStart );
    if ( panBlockSize )
        CPLFree( panBlockSize );
    if ( panBlockFlag )
        CPLFree( panBlockFlag );

    CPLFree( apadfPCT[0] );
    CPLFree( apadfPCT[1] );
    CPLFree( apadfPCT[2] );
    CPLFree( apadfPCT[3] );
    CPLFree( padfPCTBins );

    if( fpExternal != NULL )
        VSIFCloseL( fpExternal );
}

/************************************************************************/
/*                           LoadOverviews()                            */
/************************************************************************/

CPLErr HFABand::LoadOverviews()

{
    if( !bOverviewsPending )
        return CE_None;

    bOverviewsPending = FALSE;

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

            // Try finding the dependent file as this file with the
            // extension .rrd.  This is intended to address problems
            // with users changing the names of their files. 
            if( psHFA == NULL )
            {
                char *pszBasename = 
                    CPLStrdup(CPLGetBasename(psInfo->pszFilename));
                
                pszJustFilename = 
                    CPLStrdup(CPLFormFilename(NULL, pszBasename, "rrd"));
                CPLDebug( "HFA", "Failed to find overview file with expected name,\ntry %s instead.", 
                          pszJustFilename );
                psHFA = HFAGetDependent( psInfo, pszJustFilename );
                CPLFree( pszJustFilename );
                CPLFree( pszBasename );
            }

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
            if (papoOverviews[nOverviews-1]->nWidth == 0)
            {
                nWidth = nHeight = 0;
                delete papoOverviews[nOverviews-1];
                papoOverviews[nOverviews-1] = NULL;
                return CE_None;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      If there are no overviews mentioned in this file, probe for     */
/*      an .rrd file anyways.                                           */
/* -------------------------------------------------------------------- */
    HFAEntry *poBandProxyNode = poNode;
    HFAInfo_t *psOvHFA = psInfo;

    if( nOverviews == 0 
        && EQUAL(CPLGetExtension(psInfo->pszFilename),"aux") )
    {
        CPLString osRRDFilename = CPLResetExtension( psInfo->pszFilename,"rrd");
        CPLString osFullRRD = CPLFormFilename( psInfo->pszPath, osRRDFilename,
                                               NULL );
        VSIStatBufL sStatBuf;

        if( VSIStatL( osFullRRD, &sStatBuf ) == 0 )
        {
            psOvHFA = HFAGetDependent( psInfo, osRRDFilename );
            if( psOvHFA )
                poBandProxyNode = 
                    psOvHFA->poRoot->GetNamedChild( poNode->GetName() );
            else
                psOvHFA = psInfo;
        }
    }

/* -------------------------------------------------------------------- */
/*      If there are no named overviews, try looking for unnamed        */
/*      overviews within the same layer, as occurs in floodplain.img    */
/*      for instance, or in the not-referenced rrd mentioned in #3463.  */
/* -------------------------------------------------------------------- */
    if( nOverviews == 0 && poBandProxyNode != NULL )
    {
        HFAEntry	*poChild;

        for( poChild = poBandProxyNode->GetChild(); 
             poChild != NULL;
             poChild = poChild->GetNext() ) 
        {
            if( EQUAL(poChild->GetType(),"Eimg_Layer_SubSample") )
            {
                papoOverviews = (HFABand **) 
                    CPLRealloc(papoOverviews, sizeof(void*) * ++nOverviews );
                papoOverviews[nOverviews-1] = new HFABand( psOvHFA, poChild );
                if (papoOverviews[nOverviews-1]->nWidth == 0)
                {
                    nWidth = nHeight = 0;
                    delete papoOverviews[nOverviews-1];
                    papoOverviews[nOverviews-1] = NULL;
                    return CE_None;
                }
            }
        }

        int i1, i2; 
        
        // bubble sort into biggest to smallest order.
        for( i1 = 0; i1 < nOverviews; i1++ )
        {
            for( i2 = 0; i2 < nOverviews-1; i2++ )
            {
                if( papoOverviews[i2]->nWidth < 
                    papoOverviews[i2+1]->nWidth )
                {
                    HFABand *poTemp = papoOverviews[i2+1];
                    papoOverviews[i2+1] = papoOverviews[i2];
                    papoOverviews[i2] = poTemp;
                }
            }
        }
    }
    return CE_None;
}

/************************************************************************/
/*                           LoadBlockInfo()                            */
/************************************************************************/

CPLErr	HFABand::LoadBlockInfo()

{
    int		iBlock;
    HFAEntry	*poDMS;
    
    if( panBlockFlag != NULL )
        return( CE_None );

    poDMS = poNode->GetNamedChild( "RasterDMS" );
    if( poDMS == NULL )
    {
        if( poNode->GetNamedChild( "ExternalRasterDMS" ) != NULL )
            return LoadExternalBlockInfo();

        CPLError( CE_Failure, CPLE_AppDefined,
               "Can't find RasterDMS field in Eimg_Layer with block list.\n");

        return CE_Failure;
    }

    panBlockStart = (vsi_l_offset *)VSIMalloc2(sizeof(vsi_l_offset), nBlocks);
    panBlockSize = (int *) VSIMalloc2(sizeof(int), nBlocks);
    panBlockFlag = (int *) VSIMalloc2(sizeof(int), nBlocks);

    if (panBlockStart == NULL || panBlockSize == NULL || panBlockFlag == NULL)
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                 "HFABand::LoadBlockInfo : Out of memory\n");

        CPLFree(panBlockStart);
        CPLFree(panBlockSize);
        CPLFree(panBlockFlag);
        panBlockStart = NULL;
        panBlockSize = NULL;
        panBlockFlag = NULL;
        return CE_Failure;
    }

    for( iBlock = 0; iBlock < nBlocks; iBlock++ )
    {
        char	szVarName[64];
        int	nLogvalid, nCompressType;

        sprintf( szVarName, "blockinfo[%d].offset", iBlock );
        panBlockStart[iBlock] = (GUInt32)poDMS->GetIntField( szVarName );
        
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
/*                       LoadExternalBlockInfo()                        */
/************************************************************************/

CPLErr	HFABand::LoadExternalBlockInfo()

{
    int		iBlock;
    HFAEntry	*poDMS;
    
    if( panBlockFlag != NULL )
        return( CE_None );

/* -------------------------------------------------------------------- */
/*      Get the info structure.                                         */
/* -------------------------------------------------------------------- */
    poDMS = poNode->GetNamedChild( "ExternalRasterDMS" );
    CPLAssert( poDMS != NULL );

    nLayerStackCount = poDMS->GetIntField( "layerStackCount" );
    nLayerStackIndex = poDMS->GetIntField( "layerStackIndex" );

/* -------------------------------------------------------------------- */
/*      Open raw data file.                                             */
/* -------------------------------------------------------------------- */
    const char *pszRawFilename = poDMS->GetStringField( "fileName.string" );
    const char *pszFullFilename;

    pszFullFilename = CPLFormFilename( psInfo->pszPath, pszRawFilename, NULL );

    if( psInfo->eAccess == HFA_ReadOnly )
	fpExternal = VSIFOpenL( pszFullFilename, "rb" );
    else
	fpExternal = VSIFOpenL( pszFullFilename, "r+b" );
    if( fpExternal == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Unable to open external data file:\n%s\n", 
                  pszFullFilename );
        return CE_Failure;
    }
   
/* -------------------------------------------------------------------- */
/*      Verify header.                                                  */
/* -------------------------------------------------------------------- */
    char	szHeader[49];

    VSIFReadL( szHeader, 49, 1, fpExternal );

    if( strncmp( szHeader, "ERDAS_IMG_EXTERNAL_RASTER", 26 ) != 0 )
    {
        VSIFCloseL( fpExternal );
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Raw data file %s appears to be corrupt.\n",
                  pszFullFilename );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate blockmap.                                              */
/* -------------------------------------------------------------------- */
    panBlockFlag = (int *) VSIMalloc2(sizeof(int), nBlocks);
    if (panBlockFlag == NULL)
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                 "HFABand::LoadExternalBlockInfo : Out of memory\n");
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Load the validity bitmap.                                       */
/* -------------------------------------------------------------------- */
    unsigned char *pabyBlockMap;
    int		  nBytesPerRow;

    nBytesPerRow = (nBlocksPerRow + 7) / 8;
    pabyBlockMap = (unsigned char *) 
        VSIMalloc(nBytesPerRow*nBlocksPerColumn+20);
    if (pabyBlockMap == NULL)
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                 "HFABand::LoadExternalBlockInfo : Out of memory\n");
        return CE_Failure;
    }

    VSIFSeekL( fpExternal, 
               poDMS->GetBigIntField( "layerStackValidFlagsOffset" ),  
               SEEK_SET );

    if( VSIFReadL( pabyBlockMap, nBytesPerRow * nBlocksPerColumn + 20, 1, 
                   fpExternal ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to read block validity map." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Establish block information.  Block position is computed        */
/*      from data base address.  Blocks are never compressed.           */
/*      Validity is determined from the validity bitmap.                */
/* -------------------------------------------------------------------- */
    nBlockStart = poDMS->GetBigIntField( "layerStackDataOffset" );
    nBlockSize = (nBlockXSize*nBlockYSize*HFAGetDataTypeBits(nDataType)+7) / 8;

    for( iBlock = 0; iBlock < nBlocks; iBlock++ )
    {
        int	nRow, nColumn, nBit;

        nColumn = iBlock % nBlocksPerRow;
        nRow = iBlock / nBlocksPerRow;
        nBit = nRow * nBytesPerRow * 8 + nColumn + 20 * 8;

        if( (pabyBlockMap[nBit>>3] >> (nBit&7)) & 0x1 )
            panBlockFlag[iBlock] = BFLG_VALID;
        else
            panBlockFlag[iBlock] = 0;
    }

    CPLFree( pabyBlockMap );

    return( CE_None );
}

/************************************************************************/
/*                          UncompressBlock()                           */
/*                                                                      */
/*      Uncompress ESRI Grid compression format block.                  */
/************************************************************************/

#define CHECK_ENOUGH_BYTES(n) \
    if (nSrcBytes < (n)) goto not_enough_bytes;

static CPLErr UncompressBlock( GByte *pabyCData, int nSrcBytes,
                               GByte *pabyDest, int nMaxPixels, 
                               int nDataType )

{
    GUInt32  nDataMin;
    int      nNumBits, nPixelsOutput=0;			
    GInt32   nNumRuns, nDataOffset;
    GByte *pabyCounter, *pabyValues;
    int   nValueBitOffset;
    int nCounterOffset;
    
    CHECK_ENOUGH_BYTES(13);

    memcpy( &nDataMin, pabyCData, 4 );
    nDataMin = CPL_LSBWORD32( nDataMin );
        
    memcpy( &nNumRuns, pabyCData+4, 4 );
    nNumRuns = CPL_LSBWORD32( nNumRuns );
        
    memcpy( &nDataOffset, pabyCData+8, 4 );
    nDataOffset = CPL_LSBWORD32( nDataOffset );

    nNumBits = pabyCData[12];

/* ==================================================================== */
/*      If this is not run length encoded, but just reduced             */
/*      precision, handle it now.                                       */
/* ==================================================================== */
    if( nNumRuns == -1 )
    {
        pabyValues = pabyCData + 13;
        nValueBitOffset = 0;
        
        if (nNumBits > INT_MAX / nMaxPixels ||
            nNumBits * nMaxPixels > INT_MAX - 7 ||
            (nNumBits * nMaxPixels + 7)/8 > INT_MAX - 13)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Integer overflow : nNumBits * nMaxPixels + 7");
            return CE_Failure;
        }
        CHECK_ENOUGH_BYTES(13 + (nNumBits * nMaxPixels + 7)/8);

/* -------------------------------------------------------------------- */
/*      Loop over block pixels.                                         */
/* -------------------------------------------------------------------- */
        for( nPixelsOutput = 0; nPixelsOutput < nMaxPixels; nPixelsOutput++ )
        {
            int	nDataValue, nRawValue;

/* -------------------------------------------------------------------- */
/*      Extract the data value in a way that depends on the number      */
/*      of bits in it.                                                  */
/* -------------------------------------------------------------------- */
            if( nNumBits == 0 )
            {
                nRawValue = 0;
            }
            else if( nNumBits == 1 )
            {
                nRawValue =
                    (pabyValues[nValueBitOffset>>3] >> (nValueBitOffset&7)) & 0x1;
                nValueBitOffset++;
            }
            else if( nNumBits == 2 )
            {
                nRawValue =
                    (pabyValues[nValueBitOffset>>3] >> (nValueBitOffset&7)) & 0x3;
                nValueBitOffset += 2;
            }
            else if( nNumBits == 4 )
            {
                nRawValue =
                    (pabyValues[nValueBitOffset>>3] >> (nValueBitOffset&7)) & 0xf;
                nValueBitOffset += 4;
            }
            else if( nNumBits == 8 )
            {
                nRawValue = *pabyValues;
                pabyValues++;
            }
            else if( nNumBits == 16 )
            {
                nRawValue = 256 * *(pabyValues++);
                nRawValue += *(pabyValues++);
            }
            else if( nNumBits == 32 )
            {
                nRawValue = 256 * 256 * 256 * *(pabyValues++);
                nRawValue += 256 * 256 * *(pabyValues++);
                nRawValue += 256 * *(pabyValues++);
                nRawValue += *(pabyValues++);
            }
            else
            {
                printf( "nNumBits = %d\n", nNumBits );
                CPLAssert( FALSE );
                nRawValue = 0;
            }

/* -------------------------------------------------------------------- */
/*      Offset by the minimum value.                                    */
/* -------------------------------------------------------------------- */
            nDataValue = nRawValue + nDataMin;

/* -------------------------------------------------------------------- */
/*      Now apply to the output buffer in a type specific way.          */
/* -------------------------------------------------------------------- */
            if( nDataType == EPT_u8 )
            {
                ((GByte *) pabyDest)[nPixelsOutput] = (GByte) nDataValue;
            }
            else if( nDataType == EPT_u1 )
            {
                if( nDataValue == 1 )
                    pabyDest[nPixelsOutput>>3] |= (1 << (nPixelsOutput & 0x7));
                else
                    pabyDest[nPixelsOutput>>3] &= ~(1<<(nPixelsOutput & 0x7));
            }
            else if( nDataType == EPT_u2 )
            {
                if( (nPixelsOutput & 0x3) == 0 )
                    pabyDest[nPixelsOutput>>2] = (GByte) nDataValue;
                else if( (nPixelsOutput & 0x3) == 1 )
                    pabyDest[nPixelsOutput>>2] |= (GByte) (nDataValue<<2);
                else if( (nPixelsOutput & 0x3) == 2 )
                    pabyDest[nPixelsOutput>>2] |= (GByte) (nDataValue<<4);
                else
                    pabyDest[nPixelsOutput>>2] |= (GByte) (nDataValue<<6);
            }
            else if( nDataType == EPT_u4 )
            {
                if( (nPixelsOutput & 0x1) == 0 )
                    pabyDest[nPixelsOutput>>1] = (GByte) nDataValue;
                else
                    pabyDest[nPixelsOutput>>1] |= (GByte) (nDataValue<<4);
            }
            else if( nDataType == EPT_s8 ) 
            { 
                ((GByte *) pabyDest)[nPixelsOutput] = (GByte) nDataValue; 
            } 
            else if( nDataType == EPT_u16 )
            {
                ((GUInt16 *) pabyDest)[nPixelsOutput] = (GUInt16) nDataValue;
            }
            else if( nDataType == EPT_s16 )
            {
                ((GInt16 *) pabyDest)[nPixelsOutput] = (GInt16) nDataValue;
            }
            else if( nDataType == EPT_s32 )
            {
                ((GInt32 *) pabyDest)[nPixelsOutput] = nDataValue;
            }
            else if( nDataType == EPT_u32 )
            {
                ((GUInt32 *) pabyDest)[nPixelsOutput] = nDataValue;
            }
            else if( nDataType == EPT_f32 )
            {
/* -------------------------------------------------------------------- */
/*      Note, floating point values are handled as if they were signed  */
/*      32-bit integers (bug #1000).                                    */
/* -------------------------------------------------------------------- */
                ((float *) pabyDest)[nPixelsOutput] = *((float*)( &nDataValue ));
            }
            else
            {
                CPLAssert( FALSE );
            }
        }

        return CE_None;
    }

/* ==================================================================== */
/*      Establish data pointers for runs.                               */
/* ==================================================================== */
    if (nNumRuns < 0 || nDataOffset < 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "nNumRuns=%d, nDataOffset=%d",
                 nNumRuns, nDataOffset);
        return CE_Failure;
    }
    
    if (nNumBits > INT_MAX / nNumRuns ||
        nNumBits * nNumRuns > INT_MAX - 7 ||
        (nNumBits * nNumRuns + 7)/8 > INT_MAX - nDataOffset)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Integer overflow : nDataOffset + (nNumBits * nNumRuns + 7)/8");
        return CE_Failure;
    }
    CHECK_ENOUGH_BYTES(nDataOffset + (nNumBits * nNumRuns + 7)/8);
    
    pabyCounter = pabyCData + 13;
    nCounterOffset = 13;
    pabyValues = pabyCData + nDataOffset;
    nValueBitOffset = 0;
    
/* -------------------------------------------------------------------- */
/*      Loop over runs.                                                 */
/* -------------------------------------------------------------------- */
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
        CHECK_ENOUGH_BYTES(nCounterOffset+1);
        if( ((*pabyCounter) & 0xc0) == 0x00 )
        {
            nRepeatCount = (*(pabyCounter++)) & 0x3f;
            nCounterOffset ++;
        }
        else if( ((*pabyCounter) & 0xc0) == 0x40 )
        {
            CHECK_ENOUGH_BYTES(nCounterOffset + 2);
            nRepeatCount = (*(pabyCounter++)) & 0x3f;
            nRepeatCount = nRepeatCount * 256 + (*(pabyCounter++));
            nCounterOffset += 2;
        }
        else if( ((*pabyCounter) & 0xc0) == 0x80 )
        {
            CHECK_ENOUGH_BYTES(nCounterOffset + 3);
            nRepeatCount = (*(pabyCounter++)) & 0x3f;
            nRepeatCount = nRepeatCount * 256 + (*(pabyCounter++));
            nRepeatCount = nRepeatCount * 256 + (*(pabyCounter++));
            nCounterOffset += 3;
        }
        else if( ((*pabyCounter) & 0xc0) == 0xc0 )
        {
            CHECK_ENOUGH_BYTES(nCounterOffset + 4);
            nRepeatCount = (*(pabyCounter++)) & 0x3f;
            nRepeatCount = nRepeatCount * 256 + (*(pabyCounter++));
            nRepeatCount = nRepeatCount * 256 + (*(pabyCounter++));
            nRepeatCount = nRepeatCount * 256 + (*(pabyCounter++));
            nCounterOffset += 4;
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
            nDataValue += 256 * 256 * *(pabyValues++);
            nDataValue += 256 * *(pabyValues++);
            nDataValue += *(pabyValues++);
        }
        else
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "nNumBits = %d", nNumBits );
            return CE_Failure;
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
            CPLDebug("HFA", "Repeat count too big : %d", nRepeatCount);
            nRepeatCount = nMaxPixels - nPixelsOutput;
        }
        
        if( nDataType == EPT_u8 )
        {
            int		i;
            
            for( i = 0; i < nRepeatCount; i++ )
            {
                CPLAssert( nDataValue < 256 );
                ((GByte *) pabyDest)[nPixelsOutput++] = (GByte)nDataValue;
            }
        }
        else if( nDataType == EPT_u16 )
        {
            int		i;
            
            for( i = 0; i < nRepeatCount; i++ )
            {
                ((GUInt16 *) pabyDest)[nPixelsOutput++] = (GUInt16)nDataValue;
            }
        }
        else if( nDataType == EPT_s8 )
        {
            int		i;
            
            for( i = 0; i < nRepeatCount; i++ )
            {
                CPLAssert( nDataValue < 256 );
                ((GByte *) pabyDest)[nPixelsOutput++] = (GByte)nDataValue;
            }
        }
        else if( nDataType == EPT_s16 )
        {
            int		i;
            
            for( i = 0; i < nRepeatCount; i++ )
            {
                ((GInt16 *) pabyDest)[nPixelsOutput++] = (GInt16)nDataValue;
            }
        }
        else if( nDataType == EPT_u32 )
        {
            int		i;
            
            for( i = 0; i < nRepeatCount; i++ )
            {
                ((GUInt32 *) pabyDest)[nPixelsOutput++] = (GUInt32)nDataValue;
            }
        }
        else if( nDataType == EPT_s32 )
        {
            int		i;
            
            for( i = 0; i < nRepeatCount; i++ )
            {
                ((GInt32 *) pabyDest)[nPixelsOutput++] = (GInt32)nDataValue;
            }
        }
        else if( nDataType == EPT_f32 )
        {
            int		i;
            float fDataValue;

            memcpy( &fDataValue, &nDataValue, 4);
            for( i = 0; i < nRepeatCount; i++ )
            {
                ((float *) pabyDest)[nPixelsOutput++] = fDataValue;
            }
        }
        else if( nDataType == EPT_u1 )
        {
            int		i;

            CPLAssert( nDataValue == 0 || nDataValue == 1 );
            
            if( nDataValue == 1 )
            {
                for( i = 0; i < nRepeatCount; i++ )
                {
                    pabyDest[nPixelsOutput>>3] |= (1 << (nPixelsOutput & 0x7));
                    nPixelsOutput++;
                }
            }
            else
            {
                for( i = 0; i < nRepeatCount; i++ )
                {
                    pabyDest[nPixelsOutput>>3] &= ~(1<<(nPixelsOutput & 0x7));
                    nPixelsOutput++;
                }
            }
        }
        else if( nDataType == EPT_u4 )
        {
            int		i;

            CPLAssert( nDataValue >= 0 && nDataValue < 16 );
            
            for( i = 0; i < nRepeatCount; i++ )
            {
                if( (nPixelsOutput & 0x1) == 0 )
                    pabyDest[nPixelsOutput>>1] = (GByte) nDataValue;
                else
                    pabyDest[nPixelsOutput>>1] |= (GByte) (nDataValue<<4);

                nPixelsOutput++;
            }
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Attempt to uncompress an unsupported pixel data type.");
            return CE_Failure;
        }
    }

    return CE_None;
    
not_enough_bytes:

    CPLError(CE_Failure, CPLE_AppDefined, "Not enough bytes in compressed block");
    return CE_Failure;
}

/************************************************************************/
/*                             NullBlock()                              */
/*                                                                      */
/*      Set the block buffer to zero or the nodata value as             */
/*      appropriate.                                                    */
/************************************************************************/

void HFABand::NullBlock( void *pData )

{
    if( !bNoDataSet )
        memset( pData, 0, 
                HFAGetDataTypeBits(nDataType)*nBlockXSize*nBlockYSize/8 );
    else
            
    {
        double adfND[2];
        int nChunkSize = MAX(1,HFAGetDataTypeBits(nDataType)/8);
        int nWords = nBlockXSize * nBlockYSize;
        int i;

        switch( nDataType )
        {
          case EPT_u1:
          {
              nWords = (nWords + 7)/8;
              if( dfNoData != 0.0 )
                  ((unsigned char *) &adfND)[0] = 0xff;
              else
                  ((unsigned char *) &adfND)[0] = 0x00;
          }
          break;

          case EPT_u2:
          {
              nWords = (nWords + 3)/4;
              if( dfNoData == 0.0 )
                  ((unsigned char *) &adfND)[0] = 0x00;
              else if( dfNoData == 1.0 )
                  ((unsigned char *) &adfND)[0] = 0x55;
              else if( dfNoData == 2.0 )
                  ((unsigned char *) &adfND)[0] = 0xaa;
              else
                  ((unsigned char *) &adfND)[0] = 0xff;
          }
          break;

          case EPT_u4:
          {
              unsigned char byVal = 
                  (unsigned char) MAX(0,MIN(15,(int)dfNoData));

              nWords = (nWords + 1)/2;
                  
              ((unsigned char *) &adfND)[0] = byVal + (byVal << 4);
          }
          break;

          case EPT_u8:
            ((unsigned char *) &adfND)[0] = 
                (unsigned char) MAX(0,MIN(255,(int)dfNoData));
            break;

          case EPT_s8:
            ((signed char *) &adfND)[0] = 
                (signed char) MAX(-128,MIN(127,(int)dfNoData));
            break;

          case EPT_u16:
            ((GUInt16 *) &adfND)[0] = (GUInt16) dfNoData;
            break;

          case EPT_s16:
            ((GInt16 *) &adfND)[0] = (GInt16) dfNoData;
            break;

          case EPT_u32:
            ((GUInt32 *) &adfND)[0] = (GUInt32) dfNoData;
            break;

          case EPT_s32:
            ((GInt32 *) &adfND)[0] = (GInt32) dfNoData;
            break;

          case EPT_f32:
            ((float *) &adfND)[0] = (float) dfNoData;
            break;

          case EPT_f64:
            ((double *) &adfND)[0] = dfNoData;
            break;

          case EPT_c64:
            ((float *) &adfND)[0] = (float) dfNoData;
            ((float *) &adfND)[1] = 0;
            break;

          case EPT_c128:
            ((double *) &adfND)[0] = dfNoData;
            ((double *) &adfND)[1] = 0;
            break;
        }
            
        for( i = 0; i < nWords; i++ )
            memcpy( ((GByte *) pData) + nChunkSize * i, 
                    &adfND[0], nChunkSize );
    }

}

/************************************************************************/
/*                           GetRasterBlock()                           */
/************************************************************************/

CPLErr HFABand::GetRasterBlock( int nXBlock, int nYBlock, void * pData, int nDataSize )

{
    int		iBlock;
    FILE	*fpData;

    if( LoadBlockInfo() != CE_None )
        return CE_Failure;

    iBlock = nXBlock + nYBlock * nBlocksPerRow;

/* -------------------------------------------------------------------- */
/*      If the block isn't valid, we just return all zeros, and an	*/
/*	indication of success.                        			*/
/* -------------------------------------------------------------------- */
    if( (panBlockFlag[iBlock] & BFLG_VALID) == 0 )
    {
        NullBlock( pData );
        return( CE_None );
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we really read the data.                              */
/* -------------------------------------------------------------------- */
    vsi_l_offset    nBlockOffset;

    // Calculate block offset in case we have spill file. Use predefined
    // block map otherwise.
    if ( fpExternal )
    {
        fpData = fpExternal;
        nBlockOffset = nBlockStart + nBlockSize * iBlock * nLayerStackCount
            + nLayerStackIndex * nBlockSize;
    }
    else
    {
        fpData = psInfo->fp;
        nBlockOffset = panBlockStart[iBlock];
        nBlockSize = panBlockSize[iBlock];
    }

    if( VSIFSeekL( fpData, nBlockOffset, SEEK_SET ) != 0 )
    {
        // XXX: We will not report error here, because file just may be
	// in update state and data for this block will be available later
        if ( psInfo->eAccess == HFA_Update )
        {
            memset( pData, 0, 
                    HFAGetDataTypeBits(nDataType)*nBlockXSize*nBlockYSize/8 );
            return CE_None;
        }
        else
        {
            CPLError( CE_Failure, CPLE_FileIO, 
                      "Seek to %x:%08x on %p failed\n%s",
                      (int) (nBlockOffset >> 32),
                      (int) (nBlockOffset & 0xffffffff), 
                      fpData, VSIStrerror(errno) );
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*	If the block is compressed, read into an intermediate buffer	*/
/*	and convert.							*/
/* -------------------------------------------------------------------- */
    if( panBlockFlag[iBlock] & BFLG_COMPRESSED )
    {
        GByte 	*pabyCData;
        CPLErr  eErr;

        pabyCData = (GByte *) VSIMalloc( (size_t) nBlockSize );
        if (pabyCData == NULL)
        {
            CPLError( CE_Failure, CPLE_OutOfMemory,
                      "HFABand::GetRasterBlock : Out of memory\n");
            return CE_Failure;
        }

        if( VSIFReadL( pabyCData, (size_t) nBlockSize, 1, fpData ) != 1 )
        {
            CPLFree( pabyCData );

	    // XXX: Suppose that file in update state
            if ( psInfo->eAccess == HFA_Update )
            {
                memset( pData, 0, 
                    HFAGetDataTypeBits(nDataType)*nBlockXSize*nBlockYSize/8 );
                return CE_None;
            }
            else
            {
                CPLError( CE_Failure, CPLE_FileIO,
                          "Read of %d bytes at %x:%08x on %p failed.\n%s", 
                          (int) nBlockSize, 
                          (int) (nBlockOffset >> 32),
                          (int) (nBlockOffset & 0xffffffff), 
                          fpData, VSIStrerror(errno) );
                return CE_Failure;
            }
        }

        eErr = UncompressBlock( pabyCData, (int) nBlockSize,
                                (GByte *) pData, nBlockXSize*nBlockYSize,
                                nDataType );

        CPLFree( pabyCData );

        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Read uncompressed data directly into the return buffer.         */
/* -------------------------------------------------------------------- */
    if ( nDataSize != -1 && (nBlockSize > INT_MAX ||
                             (int)nBlockSize > nDataSize) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "Invalid block size : %d", (int)nBlockSize);
        return CE_Failure;
    }

    if( VSIFReadL( pData, (size_t) nBlockSize, 1, fpData ) != 1 )
    {
	memset( pData, 0, 
	    HFAGetDataTypeBits(nDataType)*nBlockXSize*nBlockYSize/8 );

        if( fpData != fpExternal )
            CPLDebug( "HFABand", 
                      "Read of %x:%08x bytes at %d on %p failed.\n%s", 
                      (int) nBlockSize, 
                      (int) (nBlockOffset >> 32),
                      (int) (nBlockOffset & 0xffffffff), 
                      fpData, VSIStrerror(errno) );

	return CE_None;
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

    return( CE_None );
}

/************************************************************************/
/*                           ReAllocBlock()                           */
/************************************************************************/

void HFABand::ReAllocBlock( int iBlock, int nSize )
{
    /* For compressed files - need to realloc the space for the block */
	
    // TODO: Should check to see if panBlockStart[iBlock] is not zero then do a HFAFreeSpace()
    // but that doesn't exist yet.
    // Instead as in interim measure it will reuse the existing block if
    // the new data will fit in.
    if( ( panBlockStart[iBlock] != 0 ) && ( nSize <= panBlockSize[iBlock] ) )
    {
        panBlockSize[iBlock] = nSize;
        //fprintf( stderr, "Reusing block %d\n", iBlock );
    }
    else
    {
        panBlockStart[iBlock] = HFAAllocateSpace( psInfo, nSize );
	
        panBlockSize[iBlock] = nSize;
	
        // need to re - write this info to the RasterDMS node
        HFAEntry	*poDMS = poNode->GetNamedChild( "RasterDMS" );
	 	
        char	szVarName[64];
        sprintf( szVarName, "blockinfo[%d].offset", iBlock );
        poDMS->SetIntField( szVarName, (int) panBlockStart[iBlock] );
		
        sprintf( szVarName, "blockinfo[%d].size", iBlock );
        poDMS->SetIntField( szVarName, panBlockSize[iBlock] );
    }

}


/************************************************************************/
/*                           SetRasterBlock()                           */
/************************************************************************/

CPLErr HFABand::SetRasterBlock( int nXBlock, int nYBlock, void * pData )

{
    int		iBlock;
    FILE	*fpData;

    if( psInfo->eAccess == HFA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Attempt to write block to read-only HFA file failed." );
        return CE_Failure;
    }

    if( LoadBlockInfo() != CE_None )
        return CE_Failure;

    iBlock = nXBlock + nYBlock * nBlocksPerRow;
    
/* -------------------------------------------------------------------- */
/*      For now we don't support write invalid uncompressed blocks.     */
/*      To do so we will need logic to make space at the end of the     */
/*      file in the right size.                                         */
/* -------------------------------------------------------------------- */
    if( (panBlockFlag[iBlock] & BFLG_VALID) == 0
        && !(panBlockFlag[iBlock] & BFLG_COMPRESSED) 
        && panBlockStart[iBlock] == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Attempt to write to invalid tile with number %d "
                  "(X position %d, Y position %d).  This\n operation currently "
                  "unsupported by HFABand::SetRasterBlock().\n",
                  iBlock, nXBlock, nYBlock );

        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Move to the location that the data sits.                        */
/* -------------------------------------------------------------------- */
    vsi_l_offset    nBlockOffset;

    // Calculate block offset in case we have spill file. Use predefined
    // block map otherwise.
    if ( fpExternal )
    {
        fpData = fpExternal;
        nBlockOffset = nBlockStart + nBlockSize * iBlock * nLayerStackCount
            + nLayerStackIndex * nBlockSize;
    }
    else
    {
        fpData = psInfo->fp;
        nBlockOffset = panBlockStart[iBlock];
        nBlockSize = panBlockSize[iBlock];
    }

/* ==================================================================== */
/*      Compressed Tile Handling.                                       */
/* ==================================================================== */
    if( panBlockFlag[iBlock] & BFLG_COMPRESSED )
    {
        /* ------------------------------------------------------------ */
        /*      Write compressed data.				        */
        /* ------------------------------------------------------------ */
        int nInBlockSize = (nBlockXSize * nBlockYSize * HFAGetDataTypeBits(nDataType) + 7 ) / 8;

        /* create the compressor object */
        HFACompress compress( pData, nInBlockSize, nDataType );
     
        /* compress the data */
        if( compress.compressBlock() )
        {
            /* get the data out of the object */
            GByte *pCounts      = compress.getCounts();
            GUInt32 nSizeCount  = compress.getCountSize();
            GByte *pValues      = compress.getValues();
            GUInt32 nSizeValues = compress.getValueSize();
            GUInt32 nMin        = compress.getMin();
            GUInt32 nNumRuns    = compress.getNumRuns();
            GByte nNumBits      = compress.getNumBits();
     
            /* Compensate for the header info */
            GUInt32 nDataOffset = nSizeCount + 13;
            int nTotalSize  = nSizeCount + nSizeValues + 13;
     
            //fprintf( stderr, "sizecount = %d sizevalues = %d min = %d numruns = %d numbits = %d\n", nSizeCount, nSizeValues, nMin, nNumRuns, (int)nNumBits );

            // Allocate space for the compressed block and seek to it.
            ReAllocBlock( iBlock, nTotalSize );
	     	
            nBlockOffset = panBlockStart[iBlock];
            nBlockSize = panBlockSize[iBlock];
	     	
            // Seek to offset
            if( VSIFSeekL( fpData, nBlockOffset, SEEK_SET ) != 0 )
            {
                CPLError( CE_Failure, CPLE_FileIO, "Seek to %x:%08x on %p failed\n%s",
                          (int) (nBlockOffset >> 32),
                          (int) (nBlockOffset & 0xffffffff), 
                          fpData, VSIStrerror(errno) );
                return CE_Failure;
            }
     	
   /* -------------------------------------------------------------------- */
   /*      Byte swap to local byte order if required.  It appears that     */
   /*      raster data is always stored in Intel byte order in Imagine     */
   /*      files.                                                          */
   /* -------------------------------------------------------------------- */
     
#ifdef CPL_MSB
 
            CPL_SWAP32PTR( &nMin );
            CPL_SWAP32PTR( &nNumRuns );
            CPL_SWAP32PTR( &nDataOffset );
     
#endif /* def CPL_MSB */
     
       /* Write out the Minimum value */
            VSIFWriteL( &nMin, (size_t) sizeof( nMin ), 1, fpData );
       
            /* the number of runs */
            VSIFWriteL( &nNumRuns, (size_t) sizeof( nNumRuns ), 1, fpData );
       
            /* The offset to the data */
            VSIFWriteL( &nDataOffset, (size_t) sizeof( nDataOffset ), 1, fpData );
       
            /* The number of bits */
            VSIFWriteL( &nNumBits, (size_t) sizeof( nNumBits ), 1, fpData );
       
            /* The counters - MSB stuff handled in HFACompress */
            VSIFWriteL( pCounts, (size_t) sizeof( GByte ), nSizeCount, fpData );
       
            /* The values - MSB stuff handled in HFACompress */
            VSIFWriteL( pValues, (size_t) sizeof( GByte ), nSizeValues, fpData );
       
            /* Compressed data is freed in the HFACompress destructor */
        }
        else
        {
            /* If we have actually made the block bigger - ie does not compress well */
            panBlockFlag[iBlock] ^= BFLG_COMPRESSED;
            // alloc more space for the uncompressed block
            ReAllocBlock( iBlock, nInBlockSize );
			 
            nBlockOffset = panBlockStart[iBlock];
            nBlockSize = panBlockSize[iBlock];

            /* Need to change the RasterDMS entry */
            HFAEntry	*poDMS = poNode->GetNamedChild( "RasterDMS" );
 	
            char	szVarName[64];
            sprintf( szVarName, "blockinfo[%d].compressionType", iBlock );
            poDMS->SetIntField( szVarName, 0 );
        }

/* -------------------------------------------------------------------- */
/*      If the block was previously invalid, mark it as valid now.      */
/* -------------------------------------------------------------------- */
        if( (panBlockFlag[iBlock] & BFLG_VALID) == 0 )
        {
            char	szVarName[64];
            HFAEntry	*poDMS = poNode->GetNamedChild( "RasterDMS" );

            sprintf( szVarName, "blockinfo[%d].logvalid", iBlock );
            poDMS->SetStringField( szVarName, "true" );

            panBlockFlag[iBlock] |= BFLG_VALID;
        }
    }
 
/* ==================================================================== */
/*      Uncompressed TILE handling.                                     */
/* ==================================================================== */
    if( ( panBlockFlag[iBlock] & BFLG_COMPRESSED ) == 0 )
    {

        if( VSIFSeekL( fpData, nBlockOffset, SEEK_SET ) != 0 )
        {
            CPLError( CE_Failure, CPLE_FileIO, "Seek to %x:%08x on %p failed\n%s",
                      (int) (nBlockOffset >> 32),
                      (int) (nBlockOffset & 0xffffffff), 
                      fpData, VSIStrerror(errno) );
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
        if( VSIFWriteL( pData, (size_t) nBlockSize, 1, fpData ) != 1 )
        {
            CPLError( CE_Failure, CPLE_FileIO, 
                      "Write of %d bytes at %x:%08x on %p failed.\n%s",
                      (int) nBlockSize, 
                      (int) (nBlockOffset >> 32),
                      (int) (nBlockOffset & 0xffffffff), 
                      fpData, VSIStrerror(errno) );
            return CE_Failure;
        }

/* -------------------------------------------------------------------- */
/*      If the block was previously invalid, mark it as valid now.      */
/* -------------------------------------------------------------------- */
        if( (panBlockFlag[iBlock] & BFLG_VALID) == 0 )
        {
            char	szVarName[64];
            HFAEntry	*poDMS = poNode->GetNamedChild( "RasterDMS" );

            sprintf( szVarName, "blockinfo[%d].logvalid", iBlock );
            poDMS->SetStringField( szVarName, "true" );

            panBlockFlag[iBlock] |= BFLG_VALID;
        }
    }
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
/*                         GetBandName()                                */
/*                                                                      */
/*      Return the Layer Name                                           */
/************************************************************************/
 
const char * HFABand::GetBandName()
{
    return( poNode->GetName() );
}

/************************************************************************/
/*                         SetBandName()                                */
/*                                                                      */
/*      Set the Layer Name                                              */
/************************************************************************/
 
void HFABand::SetBandName(const char *pszName)
{
    if( psInfo->eAccess == HFA_Update )
    {
        poNode->SetName(pszName);
    }
}

/************************************************************************/ 
/*                         SetNoDataValue()                             */ 
/*                                                                      */ 
/*      Set the band no-data value                                      */ 
/************************************************************************/ 

CPLErr HFABand::SetNoDataValue( double dfValue ) 
{ 
    CPLErr eErr = CE_Failure; 
    
    if ( psInfo->eAccess == HFA_Update ) 
    { 
        HFAEntry *poNDNode = poNode->GetNamedChild( "Eimg_NonInitializedValue" ); 
        
        if ( poNDNode == NULL ) 
        { 
            poNDNode = new HFAEntry( psInfo, 
                                     "Eimg_NonInitializedValue",
                                     "Eimg_NonInitializedValue",
                                     poNode ); 
        } 
        
        poNDNode->MakeData( 8 + 12 + 8 ); 
        poNDNode->SetPosition(); 

        poNDNode->SetIntField( "valueBD[-3]", EPT_f64 );
        poNDNode->SetIntField( "valueBD[-2]", 1 );
        poNDNode->SetIntField( "valueBD[-1]", 1 );
        if ( poNDNode->SetDoubleField( "valueBD[0]", dfValue) != CE_Failure ) 
        { 
            bNoDataSet = TRUE; 
            dfNoData = dfValue; 
            eErr = CE_None; 
        } 
    } 
    
    return eErr;     
}

/************************************************************************/
/*                        HFAReadBFUniqueBins()                         */
/*                                                                      */
/*      Attempt to read the bins used for a PCT or RAT from a           */
/*      BinFunction node.  On failure just return NULL.                 */
/************************************************************************/

double *HFAReadBFUniqueBins( HFAEntry *poBinFunc, int nPCTColors )

{
/* -------------------------------------------------------------------- */
/*      First confirm this is a "BFUnique" bin function.  We don't      */
/*      know what to do with any other types.                           */
/* -------------------------------------------------------------------- */
    const char *pszBinFunctionType = 
        poBinFunc->GetStringField( "binFunction.type.string" );

    if( pszBinFunctionType == NULL 
        || !EQUAL(pszBinFunctionType,"BFUnique") )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Process dictionary.                                             */
/* -------------------------------------------------------------------- */
    const char *pszDict = 
        poBinFunc->GetStringField( "binFunction.MIFDictionary.string" );
    if( pszDict == NULL )
        poBinFunc->GetStringField( "binFunction.MIFDictionary" );

    HFADictionary oMiniDict( pszDict );

    HFAType *poBFUnique = oMiniDict.FindType( "BFUnique" );
    if( poBFUnique == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Field the MIFObject raw data pointer.                           */
/* -------------------------------------------------------------------- */
    const GByte *pabyMIFObject = (const GByte *) 
        poBinFunc->GetStringField("binFunction.MIFObject");
    
    if( pabyMIFObject == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Confirm that this is a 64bit floating point basearray.          */
/* -------------------------------------------------------------------- */
    if( pabyMIFObject[20] != 0x0a || pabyMIFObject[21] != 0x00 )
    {
        CPLDebug( "HFA", "HFAReadPCTBins(): The basedata does not appear to be EGDA_TYPE_F64." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Decode bins.                                                    */
/* -------------------------------------------------------------------- */
    double *padfBins = (double *) CPLCalloc(sizeof(double),nPCTColors);
    int i;

    memcpy( padfBins, pabyMIFObject + 24, sizeof(double) * nPCTColors );
    
    for( i = 0; i < nPCTColors; i++ )
    {
        HFAStandard( 8, padfBins + i );
//        CPLDebug( "HFA", "Bin[%d] = %g", i, padfBins[i] );
    }
    
    return padfBins;
}

/************************************************************************/
/*                               GetPCT()                               */
/*                                                                      */
/*      Return PCT information, if any exists.                          */
/************************************************************************/

CPLErr HFABand::GetPCT( int * pnColors,
                        double **ppadfRed,
                        double **ppadfGreen,
                        double **ppadfBlue,
                        double **ppadfAlpha,
                        double **ppadfBins )

{
    *pnColors = 0;
    *ppadfRed = NULL;
    *ppadfGreen = NULL;
    *ppadfBlue = NULL;
    *ppadfAlpha = NULL;
    *ppadfBins = NULL;
        
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

        /* FIXME? : we could also check that nPCTColors is not too big */
        nPCTColors = poColumnEntry->GetIntField( "numRows" );
        for( iColumn = 0; iColumn < 4; iColumn++ )
        {
            apadfPCT[iColumn] = (double *)VSIMalloc2(sizeof(double),nPCTColors);
            if (apadfPCT[iColumn] == NULL)
            {
                CPLError(CE_Failure, CPLE_OutOfMemory, "Color palette will be ignored");
                return CE_Failure;
            }

            if( iColumn == 0 )
                poColumnEntry = poNode->GetNamedChild("Descriptor_Table.Red");
            else if( iColumn == 1 )
                poColumnEntry= poNode->GetNamedChild("Descriptor_Table.Green");
            else if( iColumn == 2 )
                poColumnEntry = poNode->GetNamedChild("Descriptor_Table.Blue");
            else if( iColumn == 3 ) {
                poColumnEntry = poNode->GetNamedChild("Descriptor_Table.Opacity");
	    }

            if( poColumnEntry == NULL )
            {
                double  *pdCol = apadfPCT[iColumn];
                for( i = 0; i < nPCTColors; i++ )
                    pdCol[i] = 1.0;
            }
            else
            {
                if (VSIFSeekL( psInfo->fp, poColumnEntry->GetIntField("columnDataPtr"),
                               SEEK_SET ) < 0)
                {
                    CPLError( CE_Failure, CPLE_FileIO,
                              "VSIFSeekL() failed in HFABand::GetPCT()." );
                    return CE_Failure;
                }
                if (VSIFReadL( apadfPCT[iColumn], sizeof(double), nPCTColors,
                               psInfo->fp) != (size_t)nPCTColors)
                {
                    CPLError( CE_Failure, CPLE_FileIO,
                              "VSIFReadL() failed in HFABand::GetPCT()." );
                    return CE_Failure;
                }
                
                for( i = 0; i < nPCTColors; i++ )
                    HFAStandard( 8, apadfPCT[iColumn] + i );
            }
        }

/* -------------------------------------------------------------------- */
/*      Do we have a custom binning function? If so, try reading it.    */
/* -------------------------------------------------------------------- */
        HFAEntry *poBinFunc = 
            poNode->GetNamedChild("Descriptor_Table.#Bin_Function840#");
        
        if( poBinFunc != NULL )
        {
            padfPCTBins = HFAReadBFUniqueBins( poBinFunc, nPCTColors );
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
    *ppadfAlpha = apadfPCT[3];
    *ppadfBins = padfPCTBins;
    
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
                        double *padfBlue ,
			double *padfAlpha)

{
    static const char *apszColNames[4] = {"Red", "Green", "Blue", "Opacity"};
    HFAEntry	*poEdsc_Table;
    int          iColumn;

/* -------------------------------------------------------------------- */
/*      Do we need to try and clear any existing color table?           */
/* -------------------------------------------------------------------- */
    if( nColors == 0 )
    {
        poEdsc_Table = poNode->GetNamedChild( "Descriptor_Table" );
        if( poEdsc_Table == NULL )
            return CE_None;
        
        for( iColumn = 0; iColumn < 4; iColumn++ )
        {
            HFAEntry        *poEdsc_Column;

            poEdsc_Column = poEdsc_Table->GetNamedChild(apszColNames[iColumn]);
            if( poEdsc_Column )
                poEdsc_Column->RemoveAndDestroy();
        }

        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Create the Descriptor table.                                    */
/* -------------------------------------------------------------------- */
    poEdsc_Table = poNode->GetNamedChild( "Descriptor_Table" );
    if( poEdsc_Table == NULL 
        || !EQUAL(poEdsc_Table->GetType(),"Edsc_Table") )
        poEdsc_Table = new HFAEntry( psInfo, "Descriptor_Table", 
                                     "Edsc_Table", poNode );

    poEdsc_Table->SetIntField( "numrows", nColors );

/* -------------------------------------------------------------------- */
/*      Create the Binning function node.  I am not sure that we        */
/*      really need this though.                                        */
/* -------------------------------------------------------------------- */
    HFAEntry       *poEdsc_BinFunction;

    poEdsc_BinFunction = poEdsc_Table->GetNamedChild( "#Bin_Function#" );
    if( poEdsc_BinFunction == NULL 
        || !EQUAL(poEdsc_BinFunction->GetType(),"Edsc_BinFunction") )
        poEdsc_BinFunction = new HFAEntry( psInfo, "#Bin_Function#", 
                                           "Edsc_BinFunction", 
                                           poEdsc_Table );

    // Because of the BaseData we have to hardcode the size. 
    poEdsc_BinFunction->MakeData( 30 );

    poEdsc_BinFunction->SetIntField( "numBins", nColors );
    poEdsc_BinFunction->SetStringField( "binFunction", "direct" );
    poEdsc_BinFunction->SetDoubleField( "minLimit", 0.0 );
    poEdsc_BinFunction->SetDoubleField( "maxLimit", nColors - 1.0 );

/* -------------------------------------------------------------------- */
/*      Process each color component                                    */
/* -------------------------------------------------------------------- */
    for( iColumn = 0; iColumn < 4; iColumn++ )
    {
        HFAEntry        *poEdsc_Column;
        double	    *padfValues=NULL;
        const char      *pszName = apszColNames[iColumn];

        if( iColumn == 0 )
            padfValues = padfRed;
        else if( iColumn == 1 )
            padfValues = padfGreen;
        else if( iColumn == 2 )
            padfValues = padfBlue;
        else if( iColumn == 3 )
            padfValues = padfAlpha;

/* -------------------------------------------------------------------- */
/*      Create the Edsc_Column.                                         */
/* -------------------------------------------------------------------- */
        poEdsc_Column = poEdsc_Table->GetNamedChild( pszName );
        if( poEdsc_Column == NULL 
            || !EQUAL(poEdsc_Column->GetType(),"Edsc_Column") )
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
        VSIFSeekL( psInfo->fp, nOffset, SEEK_SET );
        VSIFWriteL( padfFileData, 8, nColors, psInfo->fp );
        CPLFree( padfFileData );
    }

/* -------------------------------------------------------------------- */
/*      Update the layer type to be thematic.                           */
/* -------------------------------------------------------------------- */
    poNode->SetStringField( "layerType", "thematic" );

    return( CE_None );
}

/************************************************************************/
/*                           CreateOverview()                           */
/************************************************************************/

int HFABand::CreateOverview( int nOverviewLevel )

{

    CPLString osLayerName;
    int    nOXSize, nOYSize;

    nOXSize = (psInfo->nXSize + nOverviewLevel - 1) / nOverviewLevel;
    nOYSize = (psInfo->nYSize + nOverviewLevel - 1) / nOverviewLevel;

/* -------------------------------------------------------------------- */
/*      Do we want to use a dependent file (.rrd) for the overviews?    */
/*      Or just create them directly in this file?                      */
/* -------------------------------------------------------------------- */
    HFAInfo_t *psRRDInfo = psInfo;
    HFAEntry *poParent = poNode;

    if( CSLTestBoolean( CPLGetConfigOption( "HFA_USE_RRD", "NO" ) ) )
    {
        psRRDInfo = HFACreateDependent( psInfo );

        poParent = psRRDInfo->poRoot->GetNamedChild( poNode->GetName() );

        // Need to create layer object.
        if( poParent == NULL )
        {
            poParent = 
                new HFAEntry( psRRDInfo, poNode->GetName(), 
                              "Eimg_Layer", psRRDInfo->poRoot );
        }
    }

/* -------------------------------------------------------------------- */
/*      Eventually we need to decide on the whether to use the spill    */
/*      file, primarily on the basis of whether the new overview        */
/*      will drive our .img file size near 4BG.  For now, just base     */
/*      it on the config options.                                       */
/* -------------------------------------------------------------------- */
    int bCreateLargeRaster = CSLTestBoolean(
        CPLGetConfigOption("USE_SPILL","NO") );
    GIntBig nValidFlagsOffset = 0, nDataOffset = 0;

    if( (psRRDInfo->nEndOfFile 
         + (nOXSize * (double) nOYSize)
         * (HFAGetDataTypeBits(nDataType) / 8)) > 2000000000.0 )
        bCreateLargeRaster = TRUE;

    if( bCreateLargeRaster )
    {
        if( !HFACreateSpillStack( psRRDInfo, nOXSize, nOYSize, 1, 
                                  64, nDataType, 
                                  &nValidFlagsOffset, &nDataOffset ) )
	{
	    return -1;
	}
    }

/* -------------------------------------------------------------------- */
/*      Create the layer.                                               */
/* -------------------------------------------------------------------- */
    osLayerName.Printf( "_ss_%d_", nOverviewLevel );

    if( !HFACreateLayer( psRRDInfo, poParent, osLayerName, 
                         TRUE, 64, FALSE, bCreateLargeRaster, FALSE,
                         nOXSize, nOYSize, nDataType, NULL,
                         nValidFlagsOffset, nDataOffset, 1, 0 ) )
        return -1;
    
    HFAEntry *poOverLayer = poParent->GetNamedChild( osLayerName );
    if( poOverLayer == NULL )
        return -1;

/* -------------------------------------------------------------------- */
/*      Create RRDNamesList list if it does not yet exist.              */
/* -------------------------------------------------------------------- */
    HFAEntry *poRRDNamesList = poNode->GetNamedChild("RRDNamesList");
    if( poRRDNamesList == NULL )
    {
        poRRDNamesList = new HFAEntry( psInfo, "RRDNamesList", 
                                       "Eimg_RRDNamesList", 
                                       poNode );
        poRRDNamesList->MakeData( 23+16+8+ 3000 /* hack for growth room*/ );

        /* we need to hardcode file offset into the data, so locate it now */
        poRRDNamesList->SetPosition();

        poRRDNamesList->SetStringField( "algorithm.string", 
                                        "IMAGINE 2X2 Resampling" );
    }

/* -------------------------------------------------------------------- */
/*      Add new overview layer to RRDNamesList.                         */
/* -------------------------------------------------------------------- */
    int iNextName = poRRDNamesList->GetFieldCount( "nameList" );
    char szName[50];

    sprintf( szName, "nameList[%d].string", iNextName );

    osLayerName.Printf( "%s(:%s:_ss_%d_)", 
                        psRRDInfo->pszFilename, poNode->GetName(), 
                        nOverviewLevel );

    // TODO: Need to add to end of array (thats pretty hard).
    if( poRRDNamesList->SetStringField( szName, osLayerName ) != CE_None )
    {
        poRRDNamesList->MakeData( poRRDNamesList->GetDataSize() + 3000 );
        if( poRRDNamesList->SetStringField( szName, osLayerName ) != CE_None )
            return -1;
    }

/* -------------------------------------------------------------------- */
/*      Add to the list of overviews for this band.                     */
/* -------------------------------------------------------------------- */
    papoOverviews = (HFABand **) 
        CPLRealloc(papoOverviews, sizeof(void*) * ++nOverviews );
    papoOverviews[nOverviews-1] = new HFABand( psRRDInfo, poOverLayer );

/* -------------------------------------------------------------------- */
/*      If there is a nodata value, copy it to the overview band.       */
/* -------------------------------------------------------------------- */
    if( bNoDataSet )
        papoOverviews[nOverviews-1]->SetNoDataValue( dfNoData );

    return nOverviews-1;
}
