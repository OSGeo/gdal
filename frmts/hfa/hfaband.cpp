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
    apadfPCT[0] = apadfPCT[1] = apadfPCT[2] = apadfPCT[3] = NULL;

    nOverviews = 0;
    papoOverviews = NULL;

    fpExternal = NULL;

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
    else
    {
        bNoDataSet = FALSE;
        dfNoData = 0.0;
    }

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
                    CPLStrdup(CPLGetBasename(psInfoIn->pszFilename));
                
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

    if( fpExternal != NULL )
        VSIFCloseL( fpExternal );
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

    panBlockStart = (vsi_l_offset *) CPLMalloc(sizeof(vsi_l_offset) * nBlocks);
    panBlockSize = (int *) CPLMalloc(sizeof(int) * nBlocks);
    panBlockFlag = (int *) CPLMalloc(sizeof(int) * nBlocks);

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
    panBlockFlag = (int *) CPLMalloc(sizeof(int) * nBlocks);

/* -------------------------------------------------------------------- */
/*      Load the validity bitmap.                                       */
/* -------------------------------------------------------------------- */
    unsigned char *pabyBlockMap;
    int		  nBytesPerRow;

    nBytesPerRow = (nBlocksPerRow + 7) / 8;
    pabyBlockMap = (unsigned char *) 
        CPLMalloc(nBytesPerRow*nBlocksPerColumn+20);

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

static CPLErr UncompressBlock( GByte *pabyCData, int /* nSrcBytes */,
                               GByte *pabyDest, int nMaxPixels, 
                               int nDataType )

{
    GUInt32  nDataMin, nDataOffset;
    int      nNumBits, nPixelsOutput=0;			
    GInt32   nNumRuns;
    GByte *pabyCounter, *pabyValues;
    int   nValueBitOffset;

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

        for( nPixelsOutput = 0; nPixelsOutput < nMaxPixels; nPixelsOutput++ )
        {
            int	nDataValue;

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
                if( (nPixelsOutput & 0x1) == 0 )
                    pabyDest[nPixelsOutput>>2] = (GByte) nDataValue;
                else if( (nPixelsOutput & 0x1) == 1 )
                    pabyDest[nPixelsOutput>>2] |= (GByte) (nDataValue<<2);
                else if( (nPixelsOutput & 0x1) == 2 )
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
            else if( nDataType == EPT_u16 )
            {
                ((GUInt16 *) pabyDest)[nPixelsOutput] = (GUInt16) nDataValue;
            }
            else if( nDataType == EPT_s16 )
            {
                ((GInt16 *) pabyDest)[nPixelsOutput] = (GInt16) nDataValue;
            }
            else if( nDataType == EPT_f32 )
            {
                ((float *) pabyDest)[nPixelsOutput] = (float) nDataValue;
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
    pabyCounter = pabyCData + 13;
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
            nDataValue += 256 * 256 * *(pabyValues++);
            nDataValue += 256 * *(pabyValues++);
            nDataValue += *(pabyValues++);
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
}


/************************************************************************/
/*                           GetRasterBlock()                           */
/************************************************************************/

CPLErr HFABand::GetRasterBlock( int nXBlock, int nYBlock, void * pData )

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
    if( !panBlockFlag[iBlock] & BFLG_VALID )
    {
        memset( pData, 0, 
                HFAGetDataTypeBits(nDataType)*nBlockXSize*nBlockYSize/8 );

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

        pabyCData = (GByte *) CPLMalloc( (size_t) nBlockSize );

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
        poDMS->SetIntField( szVarName, panBlockStart[iBlock] );
		
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

    if( LoadBlockInfo() != CE_None )
        return CE_Failure;

    iBlock = nXBlock + nYBlock * nBlocksPerRow;
    
/* -------------------------------------------------------------------- */
/*      For now we don't support write invalid uncompressed blocks.     */
/*      To do so we will need logic to make space at the end of the     */
/*      file in the right size.                                         */
/* -------------------------------------------------------------------- */
    if( (panBlockFlag[iBlock] & BFLG_VALID) == 0
        && !(panBlockFlag[iBlock] & BFLG_COMPRESSED) )
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
/*                               GetPCT()                               */
/*                                                                      */
/*      Return PCT information, if any exists.                          */
/************************************************************************/

CPLErr HFABand::GetPCT( int * pnColors,
                        double **ppadfRed,
                        double **ppadfGreen,
                        double **ppadfBlue,
                        double **ppadfAlpha )

{
    *pnColors = 0;
    *ppadfRed = NULL;
    *ppadfGreen = NULL;
    *ppadfBlue = NULL;
    *ppadfAlpha = NULL;
        
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
        for( iColumn = 0; iColumn < 4; iColumn++ )
        {
            apadfPCT[iColumn] = (double *)CPLMalloc(sizeof(double)*nPCTColors);
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
                VSIFSeekL( psInfo->fp, poColumnEntry->GetIntField("columnDataPtr"),
                           SEEK_SET );
                VSIFReadL( apadfPCT[iColumn], sizeof(double), nPCTColors,
                           psInfo->fp);
                
                for( i = 0; i < nPCTColors; i++ )
                    HFAStandard( 8, apadfPCT[iColumn] + i );
            }
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

        // Because of the BaseData we have to hardcode the size. 
        poEdsc_BinFunction->MakeData( 30 );

        poEdsc_BinFunction->SetIntField( "numBins", nColors );
        poEdsc_BinFunction->SetStringField( "binFunction", "direct" );
        poEdsc_BinFunction->SetDoubleField( "minLimit", 0.0 );
        poEdsc_BinFunction->SetDoubleField( "maxLimit", nColors - 1.0 );

/* -------------------------------------------------------------------- */
/*      Process each color component                                    */
/* -------------------------------------------------------------------- */
        for( int iColumn = 0; iColumn < 4; iColumn++ )
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
            else if( iColumn == 3 )
            {
                pszName = "Opacity";
                padfValues = padfAlpha;
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
            VSIFSeekL( psInfo->fp, nOffset, SEEK_SET );
            VSIFWriteL( padfFileData, 8, nColors, psInfo->fp );
            CPLFree( padfFileData );
        }

/* -------------------------------------------------------------------- */
/*      Update the layer type to be thematic.                           */
/* -------------------------------------------------------------------- */
        poNode->SetStringField( "layerType", "thematic" );
    }

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
/*      Eventually we need to decide on the whether to use the spill    */
/*      file, primarily on the basis of whether the new overview        */
/*      will drive our .img file size near 4BG.  For now, just base     */
/*      it on the config options.                                       */
/* -------------------------------------------------------------------- */
    int bCreateLargeRaster = CSLTestBoolean(
        CPLGetConfigOption("USE_SPILL","NO") );
    GIntBig nValidFlagsOffset = 0, nDataOffset = 0;

    if( (psInfo->nEndOfFile 
         + (nOXSize * (double) nOYSize)
         * (HFAGetDataTypeBits(nDataType) / 8)) > 2000000000.0 )
        bCreateLargeRaster = TRUE;

    if( bCreateLargeRaster )
    {
        if( !HFACreateSpillStack( psInfo, nOXSize, nOYSize, 1, 
                                  64, nDataType, 
                                  &nValidFlagsOffset, &nDataOffset ) )
	{
	    return -1;
	}
    }

/* -------------------------------------------------------------------- */
/*      Do we want to use a dependent file (.rrd) for the overviews?    */
/*      Or just create them directly in this file?                      */
/* -------------------------------------------------------------------- */
    HFAInfo_t *psRRDInfo = psInfo;
    HFAEntry *poParent = poNode;

    if( !bCreateLargeRaster 
        && CSLTestBoolean( CPLGetConfigOption( "HFA_USE_RRD", "NO" ) ) )
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
        return -1;

/* -------------------------------------------------------------------- */
/*      Add to the list of overviews for this band.                     */
/* -------------------------------------------------------------------- */
    papoOverviews = (HFABand **) 
        CPLRealloc(papoOverviews, sizeof(void*) * ++nOverviews );
    papoOverviews[nOverviews-1] = new HFABand( psRRDInfo, poOverLayer );

    return nOverviews-1;
}
