/******************************************************************************
 * $Id$
 *
 * Project:  JPEG2000 driver based on OpenJPEG library
 * Purpose:  JPEG2000 driver based on OpenJPEG library
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault, <even dot rouault at mines dash paris dot org>
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

/* This file is to be used with openjpeg 2.0 */

#include <stdio.h> /* openjpeg.h needs FILE* */
#include <openjpeg-2.0/openjpeg.h>
#include <vector>

#include "gdaljp2abstractdataset.h"
#include "cpl_string.h"
#include "gdaljp2metadata.h"
#include "cpl_multiproc.h"
#include "cpl_atomic_ops.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                  JP2OpenJPEGDataset_ErrorCallback()                  */
/************************************************************************/

static void JP2OpenJPEGDataset_ErrorCallback(const char *pszMsg, void *unused)
{
    CPLError(CE_Failure, CPLE_AppDefined, "%s", pszMsg);
}

/************************************************************************/
/*               JP2OpenJPEGDataset_WarningCallback()                   */
/************************************************************************/

static void JP2OpenJPEGDataset_WarningCallback(const char *pszMsg, void *unused)
{
    if( strcmp(pszMsg, "Empty SOT marker detected: Psot=12.\n") == 0 )
    {
        static int bWarningEmitted = FALSE;
        if( bWarningEmitted )
            return;
        bWarningEmitted = TRUE;
    }
    if( strcmp(pszMsg, "JP2 box which are after the codestream will not be read by this function.\n") != 0 )
        CPLError(CE_Warning, CPLE_AppDefined, "%s", pszMsg);
}

/************************************************************************/
/*                 JP2OpenJPEGDataset_InfoCallback()                    */
/************************************************************************/

static void JP2OpenJPEGDataset_InfoCallback(const char *pszMsg, void *unused)
{
    char* pszMsgTmp = CPLStrdup(pszMsg);
    int nLen = (int)strlen(pszMsgTmp);
    while( nLen > 0 && pszMsgTmp[nLen-1] == '\n' )
    {
        pszMsgTmp[nLen-1] = '\0';
        nLen --;
    }
    CPLDebug("OPENJPEG", "info: %s", pszMsgTmp);
    CPLFree(pszMsgTmp);
}

/************************************************************************/
/*                      JP2OpenJPEGDataset_Read()                       */
/************************************************************************/

static OPJ_SIZE_T JP2OpenJPEGDataset_Read(void* pBuffer, OPJ_SIZE_T nBytes,
                                       void *pUserData)
{
    int nRet = VSIFReadL(pBuffer, 1, nBytes, (VSILFILE*)pUserData);
#ifdef DEBUG_IO
    CPLDebug("OPENJPEG", "JP2OpenJPEGDataset_Read(%d) = %d", (int)nBytes, nRet);
#endif
    if (nRet == 0)
        nRet = -1;
    return nRet;
}

/************************************************************************/
/*                      JP2OpenJPEGDataset_Write()                      */
/************************************************************************/

static OPJ_SIZE_T JP2OpenJPEGDataset_Write(void* pBuffer, OPJ_SIZE_T nBytes,
                                       void *pUserData)
{
    int nRet = VSIFWriteL(pBuffer, 1, nBytes, (VSILFILE*)pUserData);
#ifdef DEBUG_IO
    CPLDebug("OPENJPEG", "JP2OpenJPEGDataset_Write(%d) = %d", (int)nBytes, nRet);
#endif
    return nRet;
}

/************************************************************************/
/*                       JP2OpenJPEGDataset_Seek()                      */
/************************************************************************/

static OPJ_BOOL JP2OpenJPEGDataset_Seek(OPJ_OFF_T nBytes, void * pUserData)
{
#ifdef DEBUG_IO
    CPLDebug("OPENJPEG", "JP2OpenJPEGDataset_Seek(%d)", (int)nBytes);
#endif
    return VSIFSeekL((VSILFILE*)pUserData, nBytes, SEEK_SET) == 0;
}

/************************************************************************/
/*                     JP2OpenJPEGDataset_Skip()                        */
/************************************************************************/

static OPJ_OFF_T JP2OpenJPEGDataset_Skip(OPJ_OFF_T nBytes, void * pUserData)
{
    vsi_l_offset nOffset = VSIFTellL((VSILFILE*)pUserData);
    nOffset += nBytes;
#ifdef DEBUG_IO
    CPLDebug("OPENJPEG", "JP2OpenJPEGDataset_Skip(%d -> " CPL_FRMT_GUIB ")",
             (int)nBytes, (GUIntBig)nOffset);
#endif
    VSIFSeekL((VSILFILE*)pUserData, nOffset, SEEK_SET);
    return nBytes;
}

/************************************************************************/
/* ==================================================================== */
/*                           JP2OpenJPEGDataset                         */
/* ==================================================================== */
/************************************************************************/

class JP2OpenJPEGRasterBand;

class JP2OpenJPEGDataset : public GDALJP2AbstractDataset
{
    friend class JP2OpenJPEGRasterBand;

    VSILFILE   *fp; /* Large FILE API */

    OPJ_CODEC_FORMAT eCodecFormat;
    OPJ_COLOR_SPACE eColorSpace;

    int         bIs420;

    int         iLevel;
    int         nOverviewCount;
    JP2OpenJPEGDataset** papoOverviewDS;
    int         bUseSetDecodeArea;

    int         nThreads;
    int         GetNumThreads();
    int         bEnoughMemoryToLoadOtherBands;

  protected:
    virtual int         CloseDependentDatasets();

  public:
                JP2OpenJPEGDataset();
                ~JP2OpenJPEGDataset();
    
    static int Identify( GDALOpenInfo * poOpenInfo );
    static GDALDataset  *Open( GDALOpenInfo * );
    static GDALDataset  *CreateCopy( const char * pszFilename,
                                           GDALDataset *poSrcDS, 
                                           int bStrict, char ** papszOptions, 
                                           GDALProgressFunc pfnProgress,
                                           void * pProgressData );

    virtual CPLErr  IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType, 
                               int nBandCount, int *panBandMap,
                               int nPixelSpace, int nLineSpace, int nBandSpace);

    static void         WriteBox(VSILFILE* fp, GDALJP2Box* poBox);

    CPLErr      ReadBlock( int nBand, VSILFILE* fp,
                           int nBlockXOff, int nBlockYOff, void * pImage,
                           int nBandCount, int *panBandMap );

    int         PreloadBlocks( JP2OpenJPEGRasterBand* poBand,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               int nBandCount, int *panBandMap );
};

/************************************************************************/
/* ==================================================================== */
/*                         JP2OpenJPEGRasterBand                        */
/* ==================================================================== */
/************************************************************************/

class JP2OpenJPEGRasterBand : public GDALPamRasterBand
{
    friend class JP2OpenJPEGDataset;
    int bPromoteTo8Bit;

  public:

                JP2OpenJPEGRasterBand( JP2OpenJPEGDataset * poDS, int nBand,
                                       GDALDataType eDataType, int nBits,
                                       int bPromoteTo8Bit,
                                       int nBlockXSize, int nBlockYSize );
                ~JP2OpenJPEGRasterBand();

    virtual CPLErr          IReadBlock( int, int, void * );
    virtual CPLErr          IRasterIO( GDALRWFlag eRWFlag,
                                  int nXOff, int nYOff, int nXSize, int nYSize,
                                  void * pData, int nBufXSize, int nBufYSize,
                                  GDALDataType eBufType,
                                  int nPixelSpace, int nLineSpace );

    virtual GDALColorInterp GetColorInterpretation();

    virtual int             GetOverviewCount();
    virtual GDALRasterBand* GetOverview(int iOvrLevel);
    
    virtual int HasArbitraryOverviews() { return TRUE; }
};


/************************************************************************/
/*                        JP2OpenJPEGRasterBand()                       */
/************************************************************************/

JP2OpenJPEGRasterBand::JP2OpenJPEGRasterBand( JP2OpenJPEGDataset *poDS, int nBand,
                                              GDALDataType eDataType, int nBits,
                                              int bPromoteTo8Bit,
                                              int nBlockXSize, int nBlockYSize )

{
    this->poDS = poDS;
    this->nBand = nBand;
    this->eDataType = eDataType;
    this->nBlockXSize = nBlockXSize;
    this->nBlockYSize = nBlockYSize;
    this->bPromoteTo8Bit = bPromoteTo8Bit;

    if( (nBits % 8) != 0 )
        SetMetadataItem("NBITS",
                        CPLString().Printf("%d",nBits),
                        "IMAGE_STRUCTURE" );
}

/************************************************************************/
/*                      ~JP2OpenJPEGRasterBand()                        */
/************************************************************************/

JP2OpenJPEGRasterBand::~JP2OpenJPEGRasterBand()
{
}

/************************************************************************/
/*                            CLAMP_0_255()                             */
/************************************************************************/

static CPL_INLINE GByte CLAMP_0_255(int val)
{
    if (val < 0)
        return 0;
    else if (val > 255)
        return 255;
    else
        return (GByte)val;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr JP2OpenJPEGRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                          void * pImage )
{
    JP2OpenJPEGDataset *poGDS = (JP2OpenJPEGDataset *) poDS;
    if ( poGDS->bEnoughMemoryToLoadOtherBands )
        return poGDS->ReadBlock(nBand, poGDS->fp, nBlockXOff, nBlockYOff, pImage,
                                poGDS->nBands, NULL);
    else
        return poGDS->ReadBlock(nBand, poGDS->fp, nBlockXOff, nBlockYOff, pImage,
                                1, &nBand);
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr JP2OpenJPEGRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                         int nXOff, int nYOff, int nXSize, int nYSize,
                                         void * pData, int nBufXSize, int nBufYSize,
                                         GDALDataType eBufType,
                                         int nPixelSpace, int nLineSpace )
{
    JP2OpenJPEGDataset *poGDS = (JP2OpenJPEGDataset *) poDS;

    if( eRWFlag != GF_Read )
        return CE_Failure;

/* ==================================================================== */
/*      Do we have overviews that would be appropriate to satisfy       */
/*      this request?                                                   */
/* ==================================================================== */
    if( (nBufXSize < nXSize || nBufYSize < nYSize)
        && GetOverviewCount() > 0 && eRWFlag == GF_Read )
    {
        int         nOverview;

        nOverview =
            GDALBandGetBestOverviewLevel(this, nXOff, nYOff, nXSize, nYSize,
                                        nBufXSize, nBufYSize);
        if (nOverview >= 0)
        {
            GDALRasterBand* poOverviewBand = GetOverview(nOverview);
            if (poOverviewBand == NULL)
                return CE_Failure;

            return poOverviewBand->RasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                            pData, nBufXSize, nBufYSize, eBufType,
                                            nPixelSpace, nLineSpace );
        }
    }

    poGDS->bEnoughMemoryToLoadOtherBands = poGDS->PreloadBlocks(this, nXOff, nYOff, nXSize, nYSize, 0, NULL);

    CPLErr eErr = GDALPamRasterBand::IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                         pData, nBufXSize, nBufYSize, eBufType,
                                         nPixelSpace, nLineSpace );

    poGDS->bEnoughMemoryToLoadOtherBands = TRUE;
    return eErr;
}

/************************************************************************/
/*                            GetNumThreads()                           */
/************************************************************************/

int JP2OpenJPEGDataset::GetNumThreads()
{
    if( nThreads >= 1 )
        return nThreads;

    const char* pszThreads = CPLGetConfigOption("GDAL_NUM_THREADS", "ALL_CPUS");
    if (EQUAL(pszThreads, "ALL_CPUS"))
        nThreads = CPLGetNumCPUs();
    else
        nThreads = atoi(pszThreads);
    if (nThreads > 128)
        nThreads = 128;
    if (nThreads <= 0)
        nThreads = 1;
    return nThreads;
}

/************************************************************************/
/*                   JP2OpenJPEGReadBlockInThread()                     */
/************************************************************************/

class JobStruct
{
public:

    JP2OpenJPEGDataset* poGDS;
    int                 nBand;
    std::vector< std::pair<int, int> > oPairs;
    volatile int        nCurPair;
    int                 nBandCount;
    int                *panBandMap;
};

static void JP2OpenJPEGReadBlockInThread(void* userdata)
{
    int nPair;
    JobStruct* poJob = (JobStruct*) userdata;
    JP2OpenJPEGDataset* poGDS = poJob->poGDS;
    int nBand = poJob->nBand;
    int nPairs = (int)poJob->oPairs.size();
    int nBandCount = poJob->nBandCount;
    int* panBandMap = poJob->panBandMap;
    VSILFILE* fp = VSIFOpenL(poGDS->GetDescription(), "rb");
    if( fp == NULL )
    {
        CPLDebug("OPENJPEG", "Cannot open %s", poGDS->GetDescription());
        return;
    }

    while( (nPair = CPLAtomicInc(&(poJob->nCurPair))) < nPairs )
    {
        int nBlockXOff = poJob->oPairs[nPair].first;
        int nBlockYOff = poJob->oPairs[nPair].second;
        GDALRasterBlock* poBlock = poGDS->GetRasterBand(nBand)->
                GetLockedBlockRef(nBlockXOff,nBlockYOff, TRUE);
        if (poBlock == NULL)
            break;

        void* pDstBuffer = poBlock->GetDataRef();
        if (!pDstBuffer)
        {
            poBlock->DropLock();
            break;
        }

        poGDS->ReadBlock(nBand, fp, nBlockXOff, nBlockYOff, pDstBuffer,
                         nBandCount, panBandMap);

        poBlock->DropLock();
    }

    VSIFCloseL(fp);
}

/************************************************************************/
/*                           PreloadBlocks()                            */
/************************************************************************/

int JP2OpenJPEGDataset::PreloadBlocks(JP2OpenJPEGRasterBand* poBand,
                                      int nXOff, int nYOff, int nXSize, int nYSize,
                                      int nBandCount, int *panBandMap)
{
    int bRet = TRUE;
    int nXStart = nXOff / poBand->nBlockXSize;
    int nXEnd = (nXOff + nXSize - 1) / poBand->nBlockXSize;
    int nYStart = nYOff / poBand->nBlockYSize;
    int nYEnd = (nYOff + nYSize - 1) / poBand->nBlockYSize;
    GIntBig nReqMem = (GIntBig)(nXEnd - nXStart + 1) * (nYEnd - nYStart + 1) *
                      poBand->nBlockXSize * poBand->nBlockYSize * (GDALGetDataTypeSize(poBand->eDataType) / 8);

    int nMaxThreads = GetNumThreads();
    if( !bUseSetDecodeArea && nMaxThreads > 1 )
    {
        if( nReqMem > GDALGetCacheMax64() / (nBandCount == 0 ? 1 : nBandCount) )
            return FALSE;

        int nBlocksToLoad = 0;
        std::vector< std::pair<int,int> > oPairs;
        for(int nBlockXOff = nXStart; nBlockXOff <= nXEnd; ++nBlockXOff)
        {
            for(int nBlockYOff = nYStart; nBlockYOff <= nYEnd; ++nBlockYOff)
            {
                GDALRasterBlock* poBlock = poBand->TryGetLockedBlockRef(nBlockXOff,nBlockYOff);
                if (poBlock != NULL)
                {
                    poBlock->DropLock();
                    continue;
                }
                oPairs.push_back( std::pair<int,int>(nBlockXOff, nBlockYOff) );
                nBlocksToLoad ++;
            }
        }

        if( nBlocksToLoad > 1 )
        {
            int nThreads = MIN(nBlocksToLoad, nMaxThreads);
            void** pahThreads = (void**) CPLMalloc( sizeof(void*) * nThreads );
            int i;

            CPLDebug("OPENJPEG", "%d blocks to load", nBlocksToLoad);

            JobStruct oJob;
            oJob.poGDS = this;
            oJob.nBand = poBand->GetBand();
            oJob.oPairs = oPairs;
            oJob.nCurPair = -1;
            if( nBandCount > 0 )
            {
                oJob.nBandCount = nBandCount;
                oJob.panBandMap = panBandMap;
            }
            else
            {
                if( nReqMem <= GDALGetCacheMax64() / nBands )
                {
                    oJob.nBandCount = nBands;
                    oJob.panBandMap = NULL;
                }
                else
                {
                    bRet = FALSE;
                    oJob.nBandCount = 1;
                    oJob.panBandMap = &oJob.nBand;
                }
            }

            for(i=0;i<nThreads;i++)
                pahThreads[i] = CPLCreateJoinableThread(JP2OpenJPEGReadBlockInThread, &oJob);
            for(i=0;i<nThreads;i++)
                CPLJoinThread( pahThreads[i] );
            CPLFree(pahThreads);
        }
    }

    return bRet;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr  JP2OpenJPEGDataset::IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType, 
                               int nBandCount, int *panBandMap,
                               int nPixelSpace, int nLineSpace, int nBandSpace)
{
    if( eRWFlag != GF_Read )
        return CE_Failure;

    if( nBandCount < 1 )
        return CE_Failure;

    JP2OpenJPEGRasterBand* poBand = (JP2OpenJPEGRasterBand*) GetRasterBand(panBandMap[0]);

/* ==================================================================== */
/*      Do we have overviews that would be appropriate to satisfy       */
/*      this request?                                                   */
/* ==================================================================== */

    if( (nBufXSize < nXSize || nBufYSize < nYSize)
        && poBand->GetOverviewCount() > 0 && eRWFlag == GF_Read )
    {
        int         nOverview;

        nOverview =
            GDALBandGetBestOverviewLevel(poBand, nXOff, nYOff, nXSize, nYSize,
                                        nBufXSize, nBufYSize);
        if (nOverview >= 0)
        {
            return papoOverviewDS[nOverview]->RasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                                        pData, nBufXSize, nBufYSize, eBufType,
                                                        nBandCount, panBandMap,
                                                        nPixelSpace, nLineSpace, nBandSpace );
        }
    }

    bEnoughMemoryToLoadOtherBands = PreloadBlocks(poBand, nXOff, nYOff, nXSize, nYSize, nBandCount, panBandMap);

    CPLErr eErr = GDALPamDataset::IRasterIO(   eRWFlag,
                                        nXOff, nYOff, nXSize, nYSize,
                                        pData, nBufXSize, nBufYSize,
                                        eBufType, 
                                        nBandCount, panBandMap,
                                        nPixelSpace, nLineSpace, nBandSpace );

    bEnoughMemoryToLoadOtherBands = TRUE;
    return eErr;
}

/************************************************************************/
/*                             ReadBlock()                              */
/************************************************************************/

CPLErr JP2OpenJPEGDataset::ReadBlock( int nBand, VSILFILE* fp,
                                      int nBlockXOff, int nBlockYOff, void * pImage,
                                      int nBandCount, int* panBandMap )
{
    CPLErr          eErr = CE_None;
    opj_codec_t*    pCodec;
    opj_stream_t *  pStream;
    opj_image_t *   psImage;
    
    JP2OpenJPEGRasterBand* poBand = (JP2OpenJPEGRasterBand*) GetRasterBand(nBand);
    int nBlockXSize = poBand->nBlockXSize;
    int nBlockYSize = poBand->nBlockYSize;
    GDALDataType eDataType = poBand->eDataType;

    int nDataTypeSize = (GDALGetDataTypeSize(eDataType) / 8);

    int nTileNumber = nBlockXOff + nBlockYOff * poBand->nBlocksPerRow;
    int nWidthToRead = MIN(nBlockXSize, nRasterXSize - nBlockXOff * nBlockXSize);
    int nHeightToRead = MIN(nBlockYSize, nRasterYSize - nBlockYOff * nBlockYSize);

    pCodec = opj_create_decompress(eCodecFormat);

    opj_set_info_handler(pCodec, JP2OpenJPEGDataset_InfoCallback,NULL);
    opj_set_warning_handler(pCodec, JP2OpenJPEGDataset_WarningCallback, NULL);
    opj_set_error_handler(pCodec, JP2OpenJPEGDataset_ErrorCallback,NULL);

    opj_dparameters_t parameters;
    opj_set_default_decoder_parameters(&parameters);

    if (! opj_setup_decoder(pCodec,&parameters))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "opj_setup_decoder() failed");
        return CE_Failure;
    }

    pStream = opj_stream_create(1024, TRUE); // Default 1MB is way too big for some datasets

    VSIFSeekL(fp, 0, SEEK_END);
    opj_stream_set_user_data_length(pStream, VSIFTellL(fp));
    /* Reseek to file beginning */
    VSIFSeekL(fp, 0, SEEK_SET);

    opj_stream_set_read_function(pStream, JP2OpenJPEGDataset_Read);
    opj_stream_set_seek_function(pStream, JP2OpenJPEGDataset_Seek);
    opj_stream_set_skip_function(pStream, JP2OpenJPEGDataset_Skip);
    opj_stream_set_user_data(pStream, fp);

    if(!opj_read_header(pStream,pCodec,&psImage))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "opj_read_header() failed");
        return CE_Failure;
    }

    if (!opj_set_decoded_resolution_factor( pCodec, iLevel ))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "opj_set_decoded_resolution_factor() failed");
        eErr = CE_Failure;
        goto end;
    }

    if (bUseSetDecodeArea)
    {
        if (!opj_set_decode_area(pCodec,psImage,
                                nBlockXOff*nBlockXSize,nBlockYOff*nBlockYSize,
                                (nBlockXOff+1)*nBlockXSize,(nBlockYOff+1)*nBlockYSize))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "opj_set_decode_area() failed");
            eErr = CE_Failure;
            goto end;
        }
        if (!opj_decode(pCodec,pStream, psImage))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "opj_decode() failed");
            eErr = CE_Failure;
            goto end;
        }
    }
    else
    {
        if (!opj_get_decoded_tile( pCodec, pStream, psImage, nTileNumber ))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "opj_get_decoded_tile() failed");
            eErr = CE_Failure;
            goto end;
        }
    }

    for(int xBand = 0; xBand < nBandCount; xBand ++)
    {
        void* pDstBuffer;
        GDALRasterBlock *poBlock = NULL;
        int iBand = (panBandMap) ? panBandMap[xBand] : xBand + 1;
        int bPromoteTo8Bit = ((JP2OpenJPEGRasterBand*)GetRasterBand(iBand))->bPromoteTo8Bit;

        if (iBand == nBand)
            pDstBuffer = pImage;
        else
        {
            poBlock = ((JP2OpenJPEGRasterBand*)GetRasterBand(iBand))->
                TryGetLockedBlockRef(nBlockXOff,nBlockYOff);
            if (poBlock != NULL)
            {
                poBlock->DropLock();
                continue;
            }

            poBlock = GetRasterBand(iBand)->
                GetLockedBlockRef(nBlockXOff,nBlockYOff, TRUE);
            if (poBlock == NULL)
            {
                continue;
            }

            pDstBuffer = poBlock->GetDataRef();
            if (!pDstBuffer)
            {
                poBlock->DropLock();
                continue;
            }
        }

        if (bIs420)
        {
            CPLAssert((int)psImage->comps[0].w >= nWidthToRead);
            CPLAssert((int)psImage->comps[0].h >= nHeightToRead);
            CPLAssert(psImage->comps[1].w == (psImage->comps[0].w + 1) / 2);
            CPLAssert(psImage->comps[1].h == (psImage->comps[0].h + 1) / 2);
            CPLAssert(psImage->comps[2].w == (psImage->comps[0].w + 1) / 2);
            CPLAssert(psImage->comps[2].h == (psImage->comps[0].h + 1) / 2);

            OPJ_INT32* pSrcY = psImage->comps[0].data;
            OPJ_INT32* pSrcCb = psImage->comps[1].data;
            OPJ_INT32* pSrcCr = psImage->comps[2].data;
            GByte* pDst = (GByte*)pDstBuffer;
            for(int j=0;j<nHeightToRead;j++)
            {
                for(int i=0;i<nWidthToRead;i++)
                {
                    int Y = pSrcY[j * psImage->comps[0].w + i];
                    int Cb = pSrcCb[(j/2) * psImage->comps[1].w + (i/2)];
                    int Cr = pSrcCr[(j/2) * psImage->comps[2].w + (i/2)];
                    if (iBand == 1)
                        pDst[j * nBlockXSize + i] = CLAMP_0_255((int)(Y + 1.402 * (Cr - 128)));
                    else if (iBand == 2)
                        pDst[j * nBlockXSize + i] = CLAMP_0_255((int)(Y - 0.34414 * (Cb - 128) - 0.71414 * (Cr - 128)));
                    else
                        pDst[j * nBlockXSize + i] = CLAMP_0_255((int)(Y + 1.772 * (Cb - 128)));
                }
            }
        }
        else
        {
            CPLAssert((int)psImage->comps[iBand-1].w >= nWidthToRead);
            CPLAssert((int)psImage->comps[iBand-1].h >= nHeightToRead);
            
            if( bPromoteTo8Bit )
            {
                for(int j=0;j<nHeightToRead;j++)
                {
                    for(int i=0;i<nWidthToRead;i++)
                    {
                        psImage->comps[iBand-1].data[j * psImage->comps[iBand-1].w + i] *= 255;
                    }
                }
            }

            if ((int)psImage->comps[iBand-1].w == nBlockXSize &&
                (int)psImage->comps[iBand-1].h == nBlockYSize)
            {
                GDALCopyWords(psImage->comps[iBand-1].data, GDT_Int32, 4,
                            pDstBuffer, eDataType, nDataTypeSize, nBlockXSize * nBlockYSize);
            }
            else
            {
                for(int j=0;j<nHeightToRead;j++)
                {
                    GDALCopyWords(psImage->comps[iBand-1].data + j * psImage->comps[iBand-1].w, GDT_Int32, 4,
                                (GByte*)pDstBuffer + j * nBlockXSize * nDataTypeSize, eDataType, nDataTypeSize,
                                nWidthToRead);
                }
            }
        }

        if (poBlock != NULL)
            poBlock->DropLock();
    }

end:
    opj_end_decompress(pCodec,pStream);
    opj_stream_destroy(pStream);
    opj_destroy_codec(pCodec);
    opj_image_destroy(psImage);

    return eErr;
}


/************************************************************************/
/*                         GetOverviewCount()                           */
/************************************************************************/

int JP2OpenJPEGRasterBand::GetOverviewCount()
{
    JP2OpenJPEGDataset *poGDS = (JP2OpenJPEGDataset *) poDS;
    return poGDS->nOverviewCount;
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand* JP2OpenJPEGRasterBand::GetOverview(int iOvrLevel)
{
    JP2OpenJPEGDataset *poGDS = (JP2OpenJPEGDataset *) poDS;
    if (iOvrLevel < 0 || iOvrLevel >= poGDS->nOverviewCount)
        return NULL;

    return poGDS->papoOverviewDS[iOvrLevel]->GetRasterBand(nBand);
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp JP2OpenJPEGRasterBand::GetColorInterpretation()
{
    JP2OpenJPEGDataset *poGDS = (JP2OpenJPEGDataset *) poDS;

    if (poGDS->eColorSpace == OPJ_CLRSPC_GRAY)
        return GCI_GrayIndex;
    else if (poGDS->nBands == 3 || poGDS->nBands == 4)
    {
        switch(nBand)
        {
            case 1:
                return GCI_RedBand;
            case 2:
                return GCI_GreenBand;
            case 3:
                return GCI_BlueBand;
            case 4:
                return GCI_AlphaBand;
            default:
                return GCI_Undefined;
        }
    }

    return GCI_Undefined;
}

/************************************************************************/
/* ==================================================================== */
/*                           JP2OpenJPEGDataset                         */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                        JP2OpenJPEGDataset()                          */
/************************************************************************/

JP2OpenJPEGDataset::JP2OpenJPEGDataset()
{
    fp = NULL;
    nBands = 0;
    eCodecFormat = OPJ_CODEC_UNKNOWN;
    eColorSpace = OPJ_CLRSPC_UNKNOWN;
    bIs420 = FALSE;
    iLevel = 0;
    nOverviewCount = 0;
    papoOverviewDS = NULL;
    bUseSetDecodeArea = FALSE;
    nThreads = -1;
    bEnoughMemoryToLoadOtherBands = TRUE;
}

/************************************************************************/
/*                         ~JP2OpenJPEGDataset()                        */
/************************************************************************/

JP2OpenJPEGDataset::~JP2OpenJPEGDataset()

{
    FlushCache();

    if( fp != NULL )
        VSIFCloseL( fp );

    CloseDependentDatasets();
}

/************************************************************************/
/*                      CloseDependentDatasets()                        */
/************************************************************************/

int JP2OpenJPEGDataset::CloseDependentDatasets()
{
    int bRet = GDALPamDataset::CloseDependentDatasets();
    if ( papoOverviewDS )
    {
        for( int i = 0; i < nOverviewCount; i++ )
            delete papoOverviewDS[i];
        CPLFree( papoOverviewDS );
        papoOverviewDS = NULL;
        bRet = TRUE;
    }
    return bRet;
}

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

int JP2OpenJPEGDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    static const unsigned char jpc_header[] = {0xff,0x4f};
    static const unsigned char jp2_box_jp[] = {0x6a,0x50,0x20,0x20}; /* 'jP  ' */
        
    if( poOpenInfo->nHeaderBytes >= 16 
        && (memcmp( poOpenInfo->pabyHeader, jpc_header, 
                    sizeof(jpc_header) ) == 0
            || memcmp( poOpenInfo->pabyHeader + 4, jp2_box_jp, 
                    sizeof(jp2_box_jp) ) == 0
           ) )
        return TRUE;
    
    else
        return FALSE;
}
/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *JP2OpenJPEGDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if (!Identify(poOpenInfo))
        return NULL;

    VSILFILE* fp = VSIFOpenL(poOpenInfo->pszFilename, "rb");
    if (!fp)
        return NULL;

    OPJ_CODEC_FORMAT eCodecFormat;

    /* Detect which codec to use : J2K or JP2 ? */
    static const unsigned char jpc_header[] = {0xff,0x4f};
    if (memcmp( poOpenInfo->pabyHeader, jpc_header, 
                    sizeof(jpc_header) ) == 0)
        eCodecFormat = OPJ_CODEC_J2K;
    else
        eCodecFormat = OPJ_CODEC_JP2;

    opj_codec_t* pCodec;

    pCodec = opj_create_decompress(eCodecFormat);

    opj_set_info_handler(pCodec, JP2OpenJPEGDataset_InfoCallback,NULL);
    opj_set_warning_handler(pCodec, JP2OpenJPEGDataset_WarningCallback, NULL);
    opj_set_error_handler(pCodec, JP2OpenJPEGDataset_ErrorCallback,NULL);

    opj_dparameters_t parameters;
    opj_set_default_decoder_parameters(&parameters);

    if (! opj_setup_decoder(pCodec,&parameters))
    {
        VSIFCloseL(fp);
        return NULL;
    }

    opj_stream_t * pStream;
    pStream = opj_stream_create(1024, TRUE); // Default 1MB is way too big for some datasets

    VSIFSeekL(fp, 0, SEEK_END);
    opj_stream_set_user_data_length(pStream, VSIFTellL(fp));
    /* Reseek to file beginning */
    VSIFSeekL(fp, 0, SEEK_SET);

    opj_stream_set_read_function(pStream, JP2OpenJPEGDataset_Read);
    opj_stream_set_seek_function(pStream, JP2OpenJPEGDataset_Seek);
    opj_stream_set_skip_function(pStream, JP2OpenJPEGDataset_Skip);
    opj_stream_set_user_data(pStream, fp);

    opj_image_t * psImage = NULL;
    OPJ_INT32  nX0,nY0;
    OPJ_UINT32 nTileW,nTileH,nTilesX,nTilesY;
    if(!opj_read_header(pStream,pCodec,&psImage))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "opj_read_header() failed");
        opj_destroy_codec(pCodec);
        opj_stream_destroy(pStream);
        opj_image_destroy(psImage);
        VSIFCloseL(fp);
        return NULL;
    }

    opj_codestream_info_v2_t* pCodeStreamInfo = opj_get_cstr_info(pCodec);
    nX0 = pCodeStreamInfo->tx0;
    nY0 = pCodeStreamInfo->ty0;
    nTileW = pCodeStreamInfo->tdx;
    nTileH = pCodeStreamInfo->tdy;
    nTilesX = pCodeStreamInfo->tw;
    nTilesY = pCodeStreamInfo->th;
    int numResolutions = pCodeStreamInfo->m_default_tile_info.tccp_info[0].numresolutions;
    opj_destroy_cstr_info(&pCodeStreamInfo);

    if (psImage == NULL)
    {
        opj_destroy_codec(pCodec);
        opj_stream_destroy(pStream);
        opj_image_destroy(psImage);
        VSIFCloseL(fp);
        return NULL;
    }

#ifdef DEBUG
    int i;
    CPLDebug("OPENJPEG", "nX0 = %d", nX0);
    CPLDebug("OPENJPEG", "nY0 = %d", nY0);
    CPLDebug("OPENJPEG", "nTileW = %d", nTileW);
    CPLDebug("OPENJPEG", "nTileH = %d", nTileH);
    CPLDebug("OPENJPEG", "psImage->x0 = %d", psImage->x0);
    CPLDebug("OPENJPEG", "psImage->y0 = %d", psImage->y0);
    CPLDebug("OPENJPEG", "psImage->x1 = %d", psImage->x1);
    CPLDebug("OPENJPEG", "psImage->y1 = %d", psImage->y1);
    CPLDebug("OPENJPEG", "psImage->numcomps = %d", psImage->numcomps);
    CPLDebug("OPENJPEG", "psImage->color_space = %d", psImage->color_space);
    CPLDebug("OPENJPEG", "numResolutions = %d", numResolutions);
    for(i=0;i<(int)psImage->numcomps;i++)
    {
        CPLDebug("OPENJPEG", "psImage->comps[%d].dx = %d", i, psImage->comps[i].dx);
        CPLDebug("OPENJPEG", "psImage->comps[%d].dy = %d", i, psImage->comps[i].dy);
        CPLDebug("OPENJPEG", "psImage->comps[%d].x0 = %d", i, psImage->comps[i].x0);
        CPLDebug("OPENJPEG", "psImage->comps[%d].y0 = %d", i, psImage->comps[i].y0);
        CPLDebug("OPENJPEG", "psImage->comps[%d].w = %d", i, psImage->comps[i].w);
        CPLDebug("OPENJPEG", "psImage->comps[%d].h = %d", i, psImage->comps[i].h);
        CPLDebug("OPENJPEG", "psImage->comps[%d].resno_decoded = %d", i, psImage->comps[i].resno_decoded);
        CPLDebug("OPENJPEG", "psImage->comps[%d].factor = %d", i, psImage->comps[i].factor);
        CPLDebug("OPENJPEG", "psImage->comps[%d].prec = %d", i, psImage->comps[i].prec);
        CPLDebug("OPENJPEG", "psImage->comps[%d].sgnd = %d", i, psImage->comps[i].sgnd);
    }
#endif

    if (psImage->x1 <= psImage->x0 ||
        psImage->y1 <= psImage->y0 ||
        psImage->numcomps == 0 ||
        psImage->comps[0].w != psImage->x1 - psImage->x0 ||
        psImage->comps[0].h != psImage->y1 - psImage->y0)
    {
        CPLDebug("OPENJPEG", "Unable to handle that image (1)");
        opj_destroy_codec(pCodec);
        opj_stream_destroy(pStream);
        opj_image_destroy(psImage);
        VSIFCloseL(fp);
        return NULL;
    }

    GDALDataType eDataType = GDT_Byte;
    if (psImage->comps[0].prec > 16)
    {
        if (psImage->comps[0].sgnd)
            eDataType = GDT_Int32;
        else
            eDataType = GDT_UInt32;
    }
    else if (psImage->comps[0].prec > 8)
    {
        if (psImage->comps[0].sgnd)
            eDataType = GDT_Int16;
        else
            eDataType = GDT_UInt16;
    }

    int bIs420  =  (psImage->color_space != OPJ_CLRSPC_SRGB &&
                    eDataType == GDT_Byte &&
                    psImage->numcomps == 3 &&
                    psImage->comps[1].w == psImage->comps[0].w / 2 &&
                    psImage->comps[1].h == psImage->comps[0].h / 2 &&
                    psImage->comps[2].w == psImage->comps[0].w / 2 &&
                    psImage->comps[2].h == psImage->comps[0].h / 2);

    if (bIs420)
    {
        CPLDebug("OPENJPEG", "420 format");
    }
    else
    {
        int iBand;
        for(iBand = 2; iBand <= (int)psImage->numcomps; iBand ++)
        {
            if( psImage->comps[iBand-1].w != psImage->comps[0].w ||
                psImage->comps[iBand-1].h != psImage->comps[0].h )
            {
                CPLDebug("OPENJPEG", "Unable to handle that image (2)");
                opj_destroy_codec(pCodec);
                opj_stream_destroy(pStream);
                opj_image_destroy(psImage);
                VSIFCloseL(fp);
                return NULL;
            }
        }
    }


/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    JP2OpenJPEGDataset     *poDS;
    int                 iBand;

    poDS = new JP2OpenJPEGDataset();
    poDS->eCodecFormat = eCodecFormat;
    poDS->eColorSpace = psImage->color_space;
    poDS->nRasterXSize = psImage->x1 - psImage->x0;
    poDS->nRasterYSize = psImage->y1 - psImage->y0;
    poDS->nBands = psImage->numcomps;
    poDS->fp = fp;
    poDS->bIs420 = bIs420;

    poDS->bUseSetDecodeArea = 
        (poDS->nRasterXSize == (int)nTileW &&
         poDS->nRasterYSize == (int)nTileH &&
         (poDS->nRasterXSize > 1024 ||
          poDS->nRasterYSize > 1024));

    if (poDS->bUseSetDecodeArea)
    {
        if (nTileW > 1024) nTileW = 1024;
        if (nTileH > 1024) nTileH = 1024;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( iBand = 1; iBand <= poDS->nBands; iBand++ )
    {
        int bPromoteTo8Bit = (
            iBand == 4 && poDS->nBands == 4 &&
            psImage->comps[0].prec == 8 &&
            psImage->comps[1].prec == 8 &&
            psImage->comps[2].prec == 8 &&
            psImage->comps[3].prec == 1 && 
            CSLTestBoolean(CPLGetConfigOption("JP2OPENJPEG_PROMOTE_1BIT_ALPHA_AS_8BIT", "YES")) );

        poDS->SetBand( iBand, new JP2OpenJPEGRasterBand( poDS, iBand, eDataType,
                                                         bPromoteTo8Bit ? 8: psImage->comps[iBand-1].prec,
                                                         bPromoteTo8Bit,
                                                         nTileW, nTileH) );
    }

/* -------------------------------------------------------------------- */
/*      Create overview datasets.                                       */
/* -------------------------------------------------------------------- */
    int nW = poDS->nRasterXSize;
    int nH = poDS->nRasterYSize;
    while (poDS->nOverviewCount+1 < numResolutions &&
           (nW > 256 || nH > 256) &&
           (poDS->bUseSetDecodeArea || ((nTileW % 2) == 0 && (nTileH % 2) == 0)))
    {
        nW /= 2;
        nH /= 2;

        VSILFILE* fpOvr = VSIFOpenL(poOpenInfo->pszFilename, "rb");
        if (!fpOvr)
            break;

        poDS->papoOverviewDS = (JP2OpenJPEGDataset**) CPLRealloc(
                    poDS->papoOverviewDS,
                    (poDS->nOverviewCount + 1) * sizeof(JP2OpenJPEGDataset*));
        JP2OpenJPEGDataset* poODS = new JP2OpenJPEGDataset();
        poODS->SetDescription( poOpenInfo->pszFilename );
        poODS->iLevel = poDS->nOverviewCount + 1;
        poODS->bUseSetDecodeArea = poDS->bUseSetDecodeArea;
        if (!poDS->bUseSetDecodeArea)
        {
            nTileW /= 2;
            nTileH /= 2;
        }
        else
        {
            if (nW < (int)nTileW || nH < (int)nTileH)
            {
                nTileW = nW;
                nTileH = nH;
                poODS->bUseSetDecodeArea = FALSE;
            }
        }

        poODS->eCodecFormat = poDS->eCodecFormat;
        poODS->eColorSpace = poDS->eColorSpace;
        poODS->nRasterXSize = nW;
        poODS->nRasterYSize = nH;
        poODS->nBands = poDS->nBands;
        poODS->fp = fpOvr;
        poODS->bIs420 = bIs420;
        for( iBand = 1; iBand <= poDS->nBands; iBand++ )
        {
            int bPromoteTo8Bit = (
                iBand == 4 && poDS->nBands == 4 &&
                psImage->comps[0].prec == 8 &&
                psImage->comps[1].prec == 8 &&
                psImage->comps[2].prec == 8 &&
                psImage->comps[3].prec == 1 && 
                CSLTestBoolean(CPLGetConfigOption("JP2OPENJPEG_PROMOTE_1BIT_ALPHA_AS_8BIT", "YES")) );

            poODS->SetBand( iBand, new JP2OpenJPEGRasterBand( poODS, iBand, eDataType,
                                                              bPromoteTo8Bit ? 8: psImage->comps[iBand-1].prec,
                                                              bPromoteTo8Bit,
                                                              nTileW, nTileH ) );
        }

        poDS->papoOverviewDS[poDS->nOverviewCount ++] = poODS;

    }

    opj_destroy_codec(pCodec);
    opj_stream_destroy(pStream);
    opj_image_destroy(psImage);
    pCodec = NULL;
    pStream = NULL;
    psImage = NULL;

/* -------------------------------------------------------------------- */
/*      More metadata.                                                  */
/* -------------------------------------------------------------------- */
    if( poDS->nBands > 1 )
    {
        poDS->SetMetadataItem( "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE" );
    }

    poDS->LoadJP2Metadata(poOpenInfo);

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    //poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/*                           WriteBox()                                 */
/************************************************************************/

void JP2OpenJPEGDataset::WriteBox(VSILFILE* fp, GDALJP2Box* poBox)
{
    GUInt32   nLBox;
    GUInt32   nTBox;

    nLBox = (int) poBox->GetDataLength() + 8;
    nLBox = CPL_MSBWORD32( nLBox );

    memcpy(&nTBox, poBox->GetType(), 4);

    VSIFWriteL( &nLBox, 4, 1, fp );
    VSIFWriteL( &nTBox, 4, 1, fp );
    VSIFWriteL(poBox->GetWritableData(), 1, (int) poBox->GetDataLength(), fp);
}

/************************************************************************/
/*                          CreateCopy()                                */
/************************************************************************/

GDALDataset * JP2OpenJPEGDataset::CreateCopy( const char * pszFilename,
                                           GDALDataset *poSrcDS, 
                                           int bStrict, char ** papszOptions, 
                                           GDALProgressFunc pfnProgress,
                                           void * pProgressData )

{
    int  nBands = poSrcDS->GetRasterCount();
    int  nXSize = poSrcDS->GetRasterXSize();
    int  nYSize = poSrcDS->GetRasterYSize();

    if( nBands != 1 && nBands != 3 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Unable to export files with %d bands.", nBands );
        return NULL;
    }

    if (poSrcDS->GetRasterBand(1)->GetColorTable() != NULL)
    {
        CPLError( (bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported, 
                  "JP2OpenJPEG driver ignores color table. "
                  "The source raster band will be considered as grey level.\n"
                  "Consider using color table expansion (-expand option in gdal_translate)\n");
        if (bStrict)
            return NULL;
    }

    GDALDataType eDataType = poSrcDS->GetRasterBand(1)->GetRasterDataType();
    int nDataTypeSize = (GDALGetDataTypeSize(eDataType) / 8);
    if (eDataType != GDT_Byte && eDataType != GDT_Int16 && eDataType != GDT_UInt16
        && eDataType != GDT_Int32 && eDataType != GDT_UInt32)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "JP2OpenJPEG driver only supports creating Byte, GDT_Int16, GDT_UInt16, GDT_Int32, GDT_UInt32");
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Analyze creation options.                                       */
/* -------------------------------------------------------------------- */
    OPJ_CODEC_FORMAT eCodecFormat = OPJ_CODEC_J2K;
    const char* pszCodec = CSLFetchNameValueDef(papszOptions, "CODEC", NULL);
    if (pszCodec)
    {
        if (EQUAL(pszCodec, "JP2"))
            eCodecFormat = OPJ_CODEC_JP2;
        else if (EQUAL(pszCodec, "J2K"))
            eCodecFormat = OPJ_CODEC_J2K;
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                    "Unsupported value for CODEC : %s. Defaulting to J2K",
                    pszCodec);
        }
    }
    else
    {
        if (strlen(pszFilename) > 4 &&
            EQUAL(pszFilename + strlen(pszFilename) - 4, ".JP2"))
        {
            eCodecFormat = OPJ_CODEC_JP2;
        }
    }

    int nBlockXSize =
        atoi(CSLFetchNameValueDef(papszOptions, "BLOCKXSIZE", "1024"));
    int nBlockYSize =
        atoi(CSLFetchNameValueDef(papszOptions, "BLOCKYSIZE", "1024"));
    if (nBlockXSize < 32 || nBlockYSize < 32)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid block size");
        return NULL;
    }

    if (nXSize < nBlockXSize)
        nBlockXSize = nXSize;
    if (nYSize < nBlockYSize)
        nBlockYSize = nYSize;

    OPJ_PROG_ORDER eProgOrder = OPJ_LRCP;
    const char* pszPROGORDER =
            CSLFetchNameValueDef(papszOptions, "PROGRESSION", "LRCP");
    if (EQUAL(pszPROGORDER, "LRCP"))
        eProgOrder = OPJ_LRCP;
    else if (EQUAL(pszPROGORDER, "RLCP"))
        eProgOrder = OPJ_RLCP;
    else if (EQUAL(pszPROGORDER, "RPCL"))
        eProgOrder = OPJ_RPCL;
    else if (EQUAL(pszPROGORDER, "PCRL"))
        eProgOrder = OPJ_PCRL;
    else if (EQUAL(pszPROGORDER, "CPRL"))
        eProgOrder = OPJ_CPRL;
    else
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Unsupported value for PROGRESSION : %s. Defaulting to LRCP",
                 pszPROGORDER);
    }

    int bIsIrreversible =
            ! (CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "REVERSIBLE", "NO")));

    double dfRate = 100. / 25;
    const char* pszQuality = CSLFetchNameValueDef(papszOptions, "QUALITY", NULL);
    if (pszQuality)
    {
        double dfQuality = atof(pszQuality);
        if (dfQuality > 0 && dfQuality <= 100)
        {
            dfRate = 100 / dfQuality;
        }
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                 "Unsupported value for QUALITY : %s. Defaulting to 25",
                 pszQuality);
        }
    }

    int nNumResolutions = 6;
    const char* pszResolutions = CSLFetchNameValueDef(papszOptions, "RESOLUTIONS", NULL);
    if (pszResolutions)
    {
        nNumResolutions = atoi(pszResolutions);
        if (nNumResolutions < 1 || nNumResolutions > 7)
        {
            nNumResolutions = 6;
            CPLError(CE_Warning, CPLE_NotSupported,
                 "Unsupported value for RESOLUTIONS : %s. Defaulting to 6",
                 pszResolutions);
        }
    }
    
    int bSOP = CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "SOP", "FALSE"));
    int bEPH = CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "EPH", "FALSE"));
    
    int bResample = nBands == 3 && eDataType == GDT_Byte &&
            CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "YCBCR420", "FALSE"));
    if (bResample && !((nXSize % 2) == 0 && (nYSize % 2) == 0 && (nBlockXSize % 2) == 0 && (nBlockYSize % 2) == 0))
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "YCBCR420 unsupported when image size and/or tile size are not multiple of 2");
        bResample = FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Setup encoder                                                  */
/* -------------------------------------------------------------------- */

    opj_cparameters_t parameters;
    opj_set_default_encoder_parameters(&parameters);
    if (bSOP)
        parameters.csty |= 0x02;
    if (bEPH)
        parameters.csty |= 0x04;
    parameters.cp_disto_alloc = 1;
    parameters.tcp_numlayers = 1;
    parameters.tcp_rates[0] = (float) dfRate;
    parameters.cp_tx0 = 0;
    parameters.cp_ty0 = 0;
    parameters.tile_size_on = TRUE;
    parameters.cp_tdx = nBlockXSize;
    parameters.cp_tdy = nBlockYSize;
    parameters.irreversible = bIsIrreversible;
    parameters.numresolution = nNumResolutions;
    parameters.prog_order = eProgOrder;

    opj_image_cmptparm_t* pasBandParams =
            (opj_image_cmptparm_t*)CPLMalloc(nBands * sizeof(opj_image_cmptparm_t));
    int iBand;
    for(iBand=0;iBand<nBands;iBand++)
    {
        pasBandParams[iBand].x0 = 0;
        pasBandParams[iBand].y0 = 0;
        if (bResample && iBand > 0)
        {
            pasBandParams[iBand].dx = 2;
            pasBandParams[iBand].dy = 2;
            pasBandParams[iBand].w = nXSize / 2;
            pasBandParams[iBand].h = nYSize / 2;
        }
        else
        {
            pasBandParams[iBand].dx = 1;
            pasBandParams[iBand].dy = 1;
            pasBandParams[iBand].w = nXSize;
            pasBandParams[iBand].h = nYSize;
        }
        pasBandParams[iBand].sgnd = (eDataType == GDT_Int16 || eDataType == GDT_Int32);
        pasBandParams[iBand].prec = 8 * nDataTypeSize;
    }

    opj_codec_t* pCodec = opj_create_compress(eCodecFormat);
    if (pCodec == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "opj_create_compress() failed");
        CPLFree(pasBandParams);
        return NULL;
    }

    opj_set_info_handler(pCodec, JP2OpenJPEGDataset_InfoCallback,NULL);
    opj_set_warning_handler(pCodec, JP2OpenJPEGDataset_WarningCallback,NULL);
    opj_set_error_handler(pCodec, JP2OpenJPEGDataset_ErrorCallback,NULL);

    OPJ_COLOR_SPACE eColorSpace = (bResample) ? OPJ_CLRSPC_SYCC : (nBands == 3) ? OPJ_CLRSPC_SRGB : OPJ_CLRSPC_GRAY;

    opj_image_t* psImage = opj_image_tile_create(nBands,pasBandParams,
                                                 eColorSpace);


    CPLFree(pasBandParams);
    pasBandParams = NULL;
    if (psImage == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "opj_image_tile_create() failed");
        opj_destroy_codec(pCodec);
        return NULL;
    }

    psImage->x0 = 0;
    psImage->y0 = 0;
    psImage->x1 = nXSize;
    psImage->y1 = nYSize;
    psImage->color_space = eColorSpace;
    psImage->numcomps = nBands;

    if (!opj_setup_encoder(pCodec,&parameters,psImage))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "opj_setup_encoder() failed");
        opj_image_destroy(psImage);
        opj_destroy_codec(pCodec);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */

    const char* pszAccess = EQUALN(pszFilename, "/vsisubfile/", 12) ? "r+b" : "w+b";
    VSILFILE* fp = VSIFOpenL(pszFilename, pszAccess);
    if (fp == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create file");
        opj_image_destroy(psImage);
        opj_destroy_codec(pCodec);
        return NULL;
    }

    opj_stream_t * pStream;
    pStream = opj_stream_create(1024*1024, FALSE);
    opj_stream_set_write_function(pStream, JP2OpenJPEGDataset_Write);
    opj_stream_set_seek_function(pStream, JP2OpenJPEGDataset_Seek);
    opj_stream_set_skip_function(pStream, JP2OpenJPEGDataset_Skip);
    opj_stream_set_user_data(pStream, fp);

    if (!opj_start_compress(pCodec,psImage,pStream))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "opj_start_compress() failed");
        opj_stream_destroy(pStream);
        opj_image_destroy(psImage);
        opj_destroy_codec(pCodec);
        VSIFCloseL(fp);
        return NULL;
    }

    int nTilesX = (nXSize + nBlockXSize - 1) / nBlockXSize;
    int nTilesY = (nYSize + nBlockYSize - 1) / nBlockYSize;

    GByte* pTempBuffer =(GByte*)VSIMalloc(nBlockXSize * nBlockYSize *
                                          nBands * nDataTypeSize);
    if (pTempBuffer == NULL)
    {
        opj_stream_destroy(pStream);
        opj_image_destroy(psImage);
        opj_destroy_codec(pCodec);
        VSIFCloseL(fp);
        return NULL;
    }

    GByte* pYUV420Buffer = NULL;
    if (bResample)
    {
        pYUV420Buffer =(GByte*)VSIMalloc(3 * nBlockXSize * nBlockYSize / 2);
        if (pYUV420Buffer == NULL)
        {
            opj_stream_destroy(pStream);
            opj_image_destroy(psImage);
            opj_destroy_codec(pCodec);
            CPLFree(pTempBuffer);
            VSIFCloseL(fp);
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Setup GML and GeoTIFF information.                              */
/* -------------------------------------------------------------------- */
    GDALJP2Metadata oJP2MD;

    int bWriteExtraBoxes = FALSE;
    int bGeoreferencingCompatOfGMLJP2 = FALSE;
    if( eCodecFormat == OPJ_CODEC_JP2 &&
        (CSLFetchBoolean( papszOptions, "GMLJP2", TRUE ) ||
         CSLFetchBoolean( papszOptions, "GeoJP2", TRUE )) )
    {
        if( poSrcDS->GetGCPCount() > 0 )
        {
            bWriteExtraBoxes = TRUE;
            oJP2MD.SetGCPs( poSrcDS->GetGCPCount(),
                            poSrcDS->GetGCPs() );
            oJP2MD.SetProjection( poSrcDS->GetGCPProjection() );
        }
        else
        {
            const char* pszWKT = poSrcDS->GetProjectionRef();
            if( pszWKT != NULL && pszWKT[0] != '\0' )
            {
                bGeoreferencingCompatOfGMLJP2 = TRUE;
                bWriteExtraBoxes = TRUE;
                oJP2MD.SetProjection( pszWKT );
            }
            double adfGeoTransform[6];
            if( poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None )
            {
                bGeoreferencingCompatOfGMLJP2 = TRUE;
                bWriteExtraBoxes = TRUE;
                oJP2MD.SetGeoTransform( adfGeoTransform );
            }
        }
    }

#define PIXELS_PER_INCH 2
#define PIXELS_PER_CM   3

    // Resolution
    double dfXRes = 0, dfYRes = 0;
    int nResUnit = 0;
    if( eCodecFormat == OPJ_CODEC_JP2
        && poSrcDS->GetMetadataItem("TIFFTAG_XRESOLUTION") != NULL
        && poSrcDS->GetMetadataItem("TIFFTAG_YRESOLUTION") != NULL
        && poSrcDS->GetMetadataItem("TIFFTAG_RESOLUTIONUNIT") != NULL )
    {
        dfXRes =
            CPLAtof(poSrcDS->GetMetadataItem("TIFFTAG_XRESOLUTION"));
        dfYRes =
            CPLAtof(poSrcDS->GetMetadataItem("TIFFTAG_YRESOLUTION"));
        nResUnit = atoi(poSrcDS->GetMetadataItem("TIFFTAG_RESOLUTIONUNIT"));

        if( nResUnit == PIXELS_PER_INCH )
        {
            // convert pixels per inch to pixels per cm.
            dfXRes = dfXRes * 39.37 / 100.0;
            dfYRes = dfYRes * 39.37 / 100.0;
            nResUnit = PIXELS_PER_CM;
        }

        if( nResUnit == PIXELS_PER_CM &&
            dfXRes > 0 && dfYRes > 0 &&
            dfXRes < 65535 && dfYRes < 65535 )
        {
            bWriteExtraBoxes = TRUE;
        }
    }

    /* The file pointer should have been set 8 bytes after the */
    /* last written bytes, because openjpeg has reserved it */
    /* for the jp2c header, but still not written. */
    vsi_l_offset nPosOriginalJP2C = 0;
    vsi_l_offset nPosRealJP2C = 0;
    GByte abyBackupWhatShouldHaveBeenTheJP2CBoxHeader[8];

    if( bWriteExtraBoxes )
    {
        nPosOriginalJP2C = VSIFTellL(fp) - 8;

        char szBoxName[4+1];
        int nLBoxJP2H = 0;

        /* If we must write a Res/Resd box, */
        /* read the box header at offset 32 */
        if ( nResUnit == PIXELS_PER_CM )
        {
            VSIFSeekL(fp, 32, SEEK_SET);
            VSIFReadL(&nLBoxJP2H, 1, 4, fp);
            nLBoxJP2H = CPL_MSBWORD32( nLBoxJP2H );
            VSIFReadL(szBoxName, 1, 4, fp);
            szBoxName[4] = '\0';
        }

        VSIFSeekL(fp, nPosOriginalJP2C, SEEK_SET);

        /* And check that it is the jp2h box before */
        /* writing the res box */
        if ( nResUnit == PIXELS_PER_CM && EQUAL(szBoxName, "jp2h") )
        {
            /* Format a resd box and embed it inside a res box */
            GDALJP2Box oResd;
            oResd.SetType("resd");
            GByte aby[10];

            int nYDenom = 1;
            while (nYDenom < 32767 && dfYRes < 32767)
            {
                dfYRes *= 2;
                nYDenom *= 2;
            }
            int nXDenom = 1;
            while (nXDenom < 32767 && dfXRes < 32767)
            {
                dfXRes *= 2;
                nXDenom *= 2;
            }

            aby[0] = (GByte) (((int)dfYRes) / 256);
            aby[1] = ((int)dfYRes) % 256;
            aby[2] = (GByte) (nYDenom / 256);
            aby[3] = nYDenom % 256;
            aby[4] = (GByte) (((int)dfXRes) / 256);
            aby[5] = ((int)dfXRes) % 256;
            aby[6] = (GByte) (nXDenom / 256);
            aby[7] = nXDenom % 256;
            aby[8] = 2;
            aby[9] = 2;
            oResd.SetWritableData(10, aby);
            GDALJP2Box* poResd = &oResd;
            GDALJP2Box* poRes = GDALJP2Box::CreateAsocBox( 1, &poResd );
            poRes->SetType("res ");

            /* Now let's extend the jp2c box header so that the */
            /* res box becomes a sub-box of it */
            nLBoxJP2H += (int)poRes->GetDataLength() + 8;
            nLBoxJP2H = CPL_MSBWORD32( nLBoxJP2H );
            VSIFSeekL(fp, 32, SEEK_SET);
            VSIFWriteL(&nLBoxJP2H, 1, 4, fp);

            /* Write the box at the end of the file */
            VSIFSeekL(fp, nPosOriginalJP2C, SEEK_SET);
            WriteBox(fp, poRes);

            delete poRes;
        }

        if( CSLFetchBoolean( papszOptions, "GMLJP2", TRUE ) &&
            bGeoreferencingCompatOfGMLJP2 )
        {
            GDALJP2Box* poBox = oJP2MD.CreateGMLJP2(nXSize,nYSize);
            WriteBox(fp, poBox);
            delete poBox;
        }
        if( CSLFetchBoolean( papszOptions, "GeoJP2", TRUE ) )
        {
            GDALJP2Box* poBox = oJP2MD.CreateJP2GeoTIFF();
            WriteBox(fp, poBox);
            delete poBox;
        }

        nPosRealJP2C = VSIFTellL(fp);

        /* Backup the res, GMLJP2 or GeoJP2 box header */
        /* that will be overwritten by opj_end_compress() */
        VSIFSeekL(fp, nPosOriginalJP2C, SEEK_SET);
        VSIFReadL(abyBackupWhatShouldHaveBeenTheJP2CBoxHeader, 1, 8, fp);

        VSIFSeekL(fp, nPosRealJP2C + 8, SEEK_SET);
    }

/* -------------------------------------------------------------------- */
/*      Iterate over the tiles                                          */
/* -------------------------------------------------------------------- */
    pfnProgress( 0.0, NULL, pProgressData );

    CPLErr eErr = CE_None;
    int nBlockXOff, nBlockYOff;
    int iTile = 0;
    for(nBlockYOff=0;eErr == CE_None && nBlockYOff<nTilesY;nBlockYOff++)
    {
        for(nBlockXOff=0;eErr == CE_None && nBlockXOff<nTilesX;nBlockXOff++)
        {
            int nWidthToRead = MIN(nBlockXSize, nXSize - nBlockXOff * nBlockXSize);
            int nHeightToRead = MIN(nBlockYSize, nYSize - nBlockYOff * nBlockYSize);
            eErr = poSrcDS->RasterIO(GF_Read,
                                     nBlockXOff * nBlockXSize,
                                     nBlockYOff * nBlockYSize,
                                     nWidthToRead, nHeightToRead,
                                     pTempBuffer, nWidthToRead, nHeightToRead,
                                     eDataType,
                                     nBands, NULL,
                                     0,0,0);
            if (eErr == CE_None)
            {
                if (bResample)
                {
                    int j, i;
                    for(j=0;j<nHeightToRead;j++)
                    {
                        for(i=0;i<nWidthToRead;i++)
                        {
                            int R = pTempBuffer[j*nWidthToRead+i];
                            int G = pTempBuffer[nHeightToRead*nWidthToRead + j*nWidthToRead+i];
                            int B = pTempBuffer[2*nHeightToRead*nWidthToRead + j*nWidthToRead+i];
                            int Y = (int) (0.299 * R + 0.587 * G + 0.114 * B);
                            int Cb = CLAMP_0_255((int) (-0.1687 * R - 0.3313 * G + 0.5 * B  + 128));
                            int Cr = CLAMP_0_255((int) (0.5 * R - 0.4187 * G - 0.0813 * B  + 128));
                            pYUV420Buffer[j*nWidthToRead+i] = (GByte) Y;
                            pYUV420Buffer[nHeightToRead * nWidthToRead + ((j/2) * ((nWidthToRead)/2) + i/2) ] = (GByte) Cb;
                            pYUV420Buffer[5 * nHeightToRead * nWidthToRead / 4 + ((j/2) * ((nWidthToRead)/2) + i/2) ] = (GByte) Cr;
                        }
                    }

                    if (!opj_write_tile(pCodec,
                                        iTile,
                                        pYUV420Buffer,
                                        3 * nWidthToRead * nHeightToRead / 2,
                                        pStream))
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                "opj_write_tile() failed");
                        eErr = CE_Failure;
                    }
                }
                else
                {
                    if (!opj_write_tile(pCodec,
                                        iTile,
                                        pTempBuffer,
                                        nWidthToRead * nHeightToRead * nBands * nDataTypeSize,
                                        pStream))
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                "opj_write_tile() failed");
                        eErr = CE_Failure;
                    }
                }
            }

            if( !pfnProgress( (iTile + 1) * 1.0 / (nTilesX * nTilesY), NULL, pProgressData ) )
                eErr = CE_Failure;

            iTile ++;
        }
    }

    VSIFree(pTempBuffer);
    VSIFree(pYUV420Buffer);

    if (eErr != CE_None)
    {
        opj_stream_destroy(pStream);
        opj_image_destroy(psImage);
        opj_destroy_codec(pCodec);
        VSIFCloseL(fp);
        return NULL;
    }

    if (!opj_end_compress(pCodec,pStream))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "opj_end_compress() failed");
        opj_stream_destroy(pStream);
        opj_image_destroy(psImage);
        opj_destroy_codec(pCodec);
        VSIFCloseL(fp);
        return NULL;
    }

    opj_stream_destroy(pStream);
    opj_image_destroy(psImage);
    opj_destroy_codec(pCodec);

    /* Move the jp2c box header at its real position */
    /* and restore the res, GMLJP2 or GeoJP2 box header that */
    /* has been overwritten */
    if( bWriteExtraBoxes )
    {
        GByte abyJP2CHeader[8];

        VSIFSeekL(fp, nPosOriginalJP2C, SEEK_SET);
        VSIFReadL(abyJP2CHeader, 1, 8, fp);

        VSIFSeekL(fp, nPosOriginalJP2C, SEEK_SET);
        VSIFWriteL(abyBackupWhatShouldHaveBeenTheJP2CBoxHeader, 1, 8, fp);

        VSIFSeekL(fp, nPosRealJP2C, SEEK_SET);
        VSIFWriteL(abyJP2CHeader, 1, 8, fp);
    }

    VSIFCloseL(fp);
/* -------------------------------------------------------------------- */
/*      Re-open dataset, and copy any auxilary pam information.         */
/* -------------------------------------------------------------------- */

    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);
    JP2OpenJPEGDataset *poDS = (JP2OpenJPEGDataset*) JP2OpenJPEGDataset::Open(&oOpenInfo);

    if( poDS )
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );

    return poDS;
}

/************************************************************************/
/*                      GDALRegister_JP2OpenJPEG()                      */
/************************************************************************/

void GDALRegister_JP2OpenJPEG()

{
    GDALDriver  *poDriver;
    
    if (! GDAL_CHECK_VERSION("JP2OpenJPEG driver"))
        return;

    if( GDALGetDriverByName( "JP2OpenJPEG" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "JP2OpenJPEG" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "JPEG-2000 driver based on OpenJPEG library" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_jp2openjpeg.html" );
        poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/jp2" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "jp2" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte Int16 UInt16 Int32 UInt32" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='CODEC' type='string-select' default='according to file extension. If unknown, default to J2K'>"
"       <Value>JP2</Value>"
"       <Value>J2K</Value>"
"   </Option>"
"   <Option name='GeoJP2' type='boolean' description='defaults to ON'/>"
"   <Option name='GMLJP2' type='boolean' description='defaults to ON'/>"
"   <Option name='QUALITY' type='float' description='Quality. 0-100' default='25'/>"
"   <Option name='REVERSIBLE' type='boolean' description='True if the compression is reversible' default='false'/>"
"   <Option name='RESOLUTIONS' type='int' description='Number of resolutions. 1-7' default='6'/>"
"   <Option name='BLOCKXSIZE' type='int' description='Tile Width' default='1024'/>"
"   <Option name='BLOCKYSIZE' type='int' description='Tile Height' default='1024'/>"
"   <Option name='PROGRESSION' type='string-select' default='LRCP'>"
"       <Value>LRCP</Value>"
"       <Value>RLCP</Value>"
"       <Value>RPCL</Value>"
"       <Value>PCRL</Value>"
"       <Value>CPRL</Value>"
"   </Option>"
"   <Option name='SOP' type='boolean' description='True to insert SOP markers' default='false'/>"
"   <Option name='EPH' type='boolean' description='True to insert EPH markers' default='false'/>"
"   <Option name='YCBCR420' type='boolean' description='if RGB must be resampled to YCbCr 4:2:0' default='false'/>"
"</CreationOptionList>"  );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnIdentify = JP2OpenJPEGDataset::Identify;
        poDriver->pfnOpen = JP2OpenJPEGDataset::Open;
        poDriver->pfnCreateCopy = JP2OpenJPEGDataset::CreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

