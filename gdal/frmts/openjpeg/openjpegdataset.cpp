/******************************************************************************
 * $Id$
 *
 * Project:  JPEG2000 driver based on OpenJPEG library
 * Purpose:  JPEG2000 driver based on OpenJPEG library
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at spatialys dot com>
 * Copyright (c) 2015, European Union (European Environment Agency)
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

#if defined(OPENJPEG_VERSION) && OPENJPEG_VERSION >= 20100
#include <openjpeg-2.1/openjpeg.h>
#else
#include <stdio.h> /* openjpeg.h needs FILE* */
#include <openjpeg-2.0/openjpeg.h>
#endif
#include <vector>

#include "gdaljp2abstractdataset.h"
#include "cpl_string.h"
#include "gdaljp2metadata.h"
#include "cpl_multiproc.h"
#include "cpl_atomic_ops.h"
#include "vrt/vrtdataset.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                  JP2OpenJPEGDataset_ErrorCallback()                  */
/************************************************************************/

static void JP2OpenJPEGDataset_ErrorCallback(const char *pszMsg, CPL_UNUSED void *unused)
{
    CPLError(CE_Failure, CPLE_AppDefined, "%s", pszMsg);
}

/************************************************************************/
/*               JP2OpenJPEGDataset_WarningCallback()                   */
/************************************************************************/

static void JP2OpenJPEGDataset_WarningCallback(const char *pszMsg, CPL_UNUSED void *unused)
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

static void JP2OpenJPEGDataset_InfoCallback(const char *pszMsg, CPL_UNUSED void *unused)
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

typedef struct
{
    VSILFILE*    fp;
    vsi_l_offset nBaseOffset;
} JP2OpenJPEGFile;

/************************************************************************/
/*                      JP2OpenJPEGDataset_Read()                       */
/************************************************************************/

static OPJ_SIZE_T JP2OpenJPEGDataset_Read(void* pBuffer, OPJ_SIZE_T nBytes,
                                       void *pUserData)
{
    JP2OpenJPEGFile* psJP2OpenJPEGFile = (JP2OpenJPEGFile* )pUserData;
    int nRet = VSIFReadL(pBuffer, 1, nBytes, psJP2OpenJPEGFile->fp);
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
    JP2OpenJPEGFile* psJP2OpenJPEGFile = (JP2OpenJPEGFile* )pUserData;
    int nRet = VSIFWriteL(pBuffer, 1, nBytes, psJP2OpenJPEGFile->fp);
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
    JP2OpenJPEGFile* psJP2OpenJPEGFile = (JP2OpenJPEGFile* )pUserData;
#ifdef DEBUG_IO
    CPLDebug("OPENJPEG", "JP2OpenJPEGDataset_Seek(%d)", (int)nBytes);
#endif
    return VSIFSeekL(psJP2OpenJPEGFile->fp, psJP2OpenJPEGFile->nBaseOffset +nBytes,
                     SEEK_SET) == 0;
}

/************************************************************************/
/*                     JP2OpenJPEGDataset_Skip()                        */
/************************************************************************/

static OPJ_OFF_T JP2OpenJPEGDataset_Skip(OPJ_OFF_T nBytes, void * pUserData)
{
    JP2OpenJPEGFile* psJP2OpenJPEGFile = (JP2OpenJPEGFile* )pUserData;
    vsi_l_offset nOffset = VSIFTellL(psJP2OpenJPEGFile->fp);
    nOffset += nBytes;
#ifdef DEBUG_IO
    CPLDebug("OPENJPEG", "JP2OpenJPEGDataset_Skip(%d -> " CPL_FRMT_GUIB ")",
             (int)nBytes, (GUIntBig)nOffset);
#endif
    VSIFSeekL(psJP2OpenJPEGFile->fp, nOffset, SEEK_SET);
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
    vsi_l_offset nCodeStreamStart;
    vsi_l_offset nCodeStreamLength;

    OPJ_COLOR_SPACE eColorSpace;
    int         nRedIndex;
    int         nGreenIndex;
    int         nBlueIndex;
    int         nAlphaIndex;

    int         bIs420;

    int         iLevel;
    int         nOverviewCount;
    JP2OpenJPEGDataset** papoOverviewDS;
    int         bUseSetDecodeArea;

    int         nThreads;
    int         GetNumThreads();
    int         bEnoughMemoryToLoadOtherBands;
    int         bRewrite;
    int         bHasGeoreferencingAtOpening;

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

    virtual CPLErr SetProjection( const char * );
    virtual CPLErr SetGeoTransform( double* );
    virtual CPLErr SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                            const char *pszGCPProjection );
    virtual CPLErr      SetMetadata( char ** papszMetadata,
                             const char * pszDomain = "" );
    virtual CPLErr      SetMetadataItem( const char * pszName,
                                 const char * pszValue,
                                 const char * pszDomain = "" );

    virtual CPLErr  IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType, 
                               int nBandCount, int *panBandMap,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GSpacing nBandSpace,
                               GDALRasterIOExtraArg* psExtraArg);

    static void         WriteBox(VSILFILE* fp, GDALJP2Box* poBox);
    static void         WriteGDALMetadataBox( VSILFILE* fp, GDALDataset* poSrcDS,
                                       char** papszOptions );
    static void         WriteXMLBoxes( VSILFILE* fp, GDALDataset* poSrcDS,
                                       char** papszOptions );
    static void         WriteXMPBox( VSILFILE* fp, GDALDataset* poSrcDS,
                                     char** papszOptions );
    static void         WriteIPRBox( VSILFILE* fp, GDALDataset* poSrcDS,
                                     char** papszOptions );

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
    int             bPromoteTo8Bit;
    GDALColorTable* poCT;

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
                                  GSpacing nPixelSpace, GSpacing nLineSpace,
                                  GDALRasterIOExtraArg* psExtraArg);

    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable* GetColorTable() { return poCT; }

    virtual int             GetOverviewCount();
    virtual GDALRasterBand* GetOverview(int iOvrLevel);
    
    virtual int HasArbitraryOverviews() { return poCT == NULL; }
};


/************************************************************************/
/*                        JP2OpenJPEGRasterBand()                       */
/************************************************************************/

JP2OpenJPEGRasterBand::JP2OpenJPEGRasterBand( JP2OpenJPEGDataset *poDS, int nBand,
                                              GDALDataType eDataType, int nBits,
                                              int bPromoteTo8Bit,
                                              int nBlockXSize, int nBlockYSize )

{
    this->eDataType = eDataType;
    this->nBlockXSize = nBlockXSize;
    this->nBlockYSize = nBlockYSize;
    this->bPromoteTo8Bit = bPromoteTo8Bit;
    poCT = NULL;

    if( (nBits % 8) != 0 )
        GDALRasterBand::SetMetadataItem("NBITS",
                        CPLString().Printf("%d",nBits),
                        "IMAGE_STRUCTURE" );
    GDALRasterBand::SetMetadataItem("COMPRESSION", "JPEG2000",
                    "IMAGE_STRUCTURE" );
    this->poDS = poDS;
    this->nBand = nBand;
}

/************************************************************************/
/*                      ~JP2OpenJPEGRasterBand()                        */
/************************************************************************/

JP2OpenJPEGRasterBand::~JP2OpenJPEGRasterBand()
{
    delete poCT;
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
                                         GSpacing nPixelSpace, GSpacing nLineSpace,
                                         GDALRasterIOExtraArg* psExtraArg )
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
        GDALRasterIOExtraArg sExtraArg;
    
        GDALCopyRasterIOExtraArg(&sExtraArg, psExtraArg);

        nOverview =
            GDALBandGetBestOverviewLevel2(this, nXOff, nYOff, nXSize, nYSize,
                                        nBufXSize, nBufYSize, &sExtraArg);
        if (nOverview >= 0)
        {
            GDALRasterBand* poOverviewBand = GetOverview(nOverview);
            if (poOverviewBand == NULL)
                return CE_Failure;

            return poOverviewBand->RasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                            pData, nBufXSize, nBufYSize, eBufType,
                                            nPixelSpace, nLineSpace, &sExtraArg );
        }
    }

    poGDS->bEnoughMemoryToLoadOtherBands = poGDS->PreloadBlocks(this, nXOff, nYOff, nXSize, nYSize, 0, NULL);

    CPLErr eErr = GDALPamRasterBand::IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                         pData, nBufXSize, nBufYSize, eBufType,
                                         nPixelSpace, nLineSpace, psExtraArg );

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
            CPLJoinableThread** pahThreads = (CPLJoinableThread**) CPLMalloc( sizeof(CPLJoinableThread*) * nThreads );
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
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GSpacing nBandSpace,
                               GDALRasterIOExtraArg* psExtraArg)
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
        GDALRasterIOExtraArg sExtraArg;
    
        GDALCopyRasterIOExtraArg(&sExtraArg, psExtraArg);

        nOverview =
            GDALBandGetBestOverviewLevel2(poBand, nXOff, nYOff, nXSize, nYSize,
                                          nBufXSize, nBufYSize, &sExtraArg);
        if (nOverview >= 0)
        {
            return papoOverviewDS[nOverview]->RasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                                        pData, nBufXSize, nBufYSize, eBufType,
                                                        nBandCount, panBandMap,
                                                        nPixelSpace, nLineSpace, nBandSpace,
                                                        &sExtraArg);
        }
    }

    bEnoughMemoryToLoadOtherBands = PreloadBlocks(poBand, nXOff, nYOff, nXSize, nYSize, nBandCount, panBandMap);

    CPLErr eErr = GDALPamDataset::IRasterIO(   eRWFlag,
                                        nXOff, nYOff, nXSize, nYSize,
                                        pData, nBufXSize, nBufYSize,
                                        eBufType, 
                                        nBandCount, panBandMap,
                                        nPixelSpace, nLineSpace, nBandSpace,
                                        psExtraArg );

    bEnoughMemoryToLoadOtherBands = TRUE;
    return eErr;
}

/************************************************************************/
/*                    JP2OpenJPEGCreateReadStream()                     */
/************************************************************************/

static opj_stream_t* JP2OpenJPEGCreateReadStream(JP2OpenJPEGFile* psJP2OpenJPEGFile,
                                                 vsi_l_offset nSize)
{
    opj_stream_t *pStream = opj_stream_create(1024, TRUE); // Default 1MB is way too big for some datasets

    VSIFSeekL(psJP2OpenJPEGFile->fp, psJP2OpenJPEGFile->nBaseOffset, SEEK_SET);
    opj_stream_set_user_data_length(pStream, nSize);

    opj_stream_set_read_function(pStream, JP2OpenJPEGDataset_Read);
    opj_stream_set_seek_function(pStream, JP2OpenJPEGDataset_Seek);
    opj_stream_set_skip_function(pStream, JP2OpenJPEGDataset_Skip);
#if defined(OPENJPEG_VERSION) && OPENJPEG_VERSION >= 20100
    opj_stream_set_user_data(pStream, psJP2OpenJPEGFile, NULL);
#else
    opj_stream_set_user_data(pStream, psJP2OpenJPEGFile);
#endif

    return pStream;
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

    pCodec = opj_create_decompress(OPJ_CODEC_J2K);

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

    JP2OpenJPEGFile sJP2OpenJPEGFile;
    sJP2OpenJPEGFile.fp = fp;
    sJP2OpenJPEGFile.nBaseOffset = nCodeStreamStart;
    pStream = JP2OpenJPEGCreateReadStream(&sJP2OpenJPEGFile, nCodeStreamLength);

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
                                 nBlockXOff*nBlockXSize,
                                 nBlockYOff*nBlockYSize,
                                 nBlockXOff*nBlockXSize+nWidthToRead,
                                 nBlockYOff*nBlockYSize+nHeightToRead))
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
            if( nBands == 4 )
            {
                CPLAssert((int)psImage->comps[3].w >= nWidthToRead);
                CPLAssert((int)psImage->comps[3].h >= nHeightToRead);
            }

            OPJ_INT32* pSrcY = psImage->comps[0].data;
            OPJ_INT32* pSrcCb = psImage->comps[1].data;
            OPJ_INT32* pSrcCr = psImage->comps[2].data;
            OPJ_INT32* pSrcA = (nBands == 4) ? psImage->comps[3].data : NULL;
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
                    else if (iBand == 3)
                        pDst[j * nBlockXSize + i] = CLAMP_0_255((int)(Y + 1.772 * (Cb - 128)));
                    else if (iBand == 4)
                        pDst[j * nBlockXSize + i] = pSrcA[j * psImage->comps[0].w + i];
                }
            }
            
            if( bPromoteTo8Bit )
            {
                for(int j=0;j<nHeightToRead;j++)
                {
                    for(int i=0;i<nWidthToRead;i++)
                    {
                        pDst[j * nBlockXSize + i] *= 255;
                    }
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

    if( poCT )
        return GCI_PaletteIndex;

    if( nBand == poGDS->nAlphaIndex + 1 )
        return GCI_AlphaBand;

    if (poGDS->nBands <= 2 && poGDS->eColorSpace == OPJ_CLRSPC_GRAY)
        return GCI_GrayIndex;
    else if (poGDS->eColorSpace == OPJ_CLRSPC_SRGB ||
             poGDS->eColorSpace == OPJ_CLRSPC_SYCC)
    {
        if( nBand == poGDS->nRedIndex + 1 )
            return GCI_RedBand;
        if( nBand == poGDS->nGreenIndex + 1 )
            return GCI_GreenBand;
        if( nBand == poGDS->nBlueIndex + 1 )
            return GCI_BlueBand;
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
    nCodeStreamStart = 0;
    nCodeStreamLength = 0;
    nBands = 0;
    eColorSpace = OPJ_CLRSPC_UNKNOWN;
    nRedIndex = 0;
    nGreenIndex = 1;
    nBlueIndex = 2;
    nAlphaIndex = -1;
    bIs420 = FALSE;
    iLevel = 0;
    nOverviewCount = 0;
    papoOverviewDS = NULL;
    bUseSetDecodeArea = FALSE;
    nThreads = -1;
    bEnoughMemoryToLoadOtherBands = TRUE;
    bRewrite = FALSE;
    bHasGeoreferencingAtOpening = FALSE;
}

/************************************************************************/
/*                         ~JP2OpenJPEGDataset()                        */
/************************************************************************/

JP2OpenJPEGDataset::~JP2OpenJPEGDataset()

{
    FlushCache();

    if( iLevel == 0 && fp != NULL )
    {
        if( bRewrite )
        {
            GDALJP2Box oBox( fp );
            vsi_l_offset nOffsetJP2C = 0, nLengthJP2C = 0,
                         nOffsetXML = 0, nOffsetASOC = 0, nOffsetUUID = 0,
                         nOffsetIHDR = 0, nLengthIHDR = 0;
            int bMSIBox = FALSE, bGMLData = FALSE;
            int bUnsupportedConfiguration = FALSE;
            if( oBox.ReadFirst() )
            {
                while( strlen(oBox.GetType()) > 0 )
                {
                    if( EQUAL(oBox.GetType(),"jp2c") )
                    {
                        if( nOffsetJP2C == 0 )
                        {
                            nOffsetJP2C = VSIFTellL(fp);
                            nLengthJP2C = oBox.GetDataLength();
                        }
                        else
                            bUnsupportedConfiguration = TRUE;
                    }
                    else if( EQUAL(oBox.GetType(),"jp2h") )
                    {
                        GDALJP2Box oSubBox( fp );
                        if( oSubBox.ReadFirstChild( &oBox ) &&
                            EQUAL(oSubBox.GetType(),"ihdr") )
                        {
                            nOffsetIHDR = VSIFTellL(fp);
                            nLengthIHDR = oSubBox.GetDataLength();
                        }
                    }
                    else if( EQUAL(oBox.GetType(),"xml ") )
                    {
                        if( nOffsetXML == 0 )
                            nOffsetXML = VSIFTellL(fp);
                    }
                    else if( EQUAL(oBox.GetType(),"asoc") )
                    {
                        if( nOffsetASOC == 0 )
                            nOffsetASOC = VSIFTellL(fp);

                        GDALJP2Box oSubBox( fp );
                        if( oSubBox.ReadFirstChild( &oBox ) &&
                            EQUAL(oSubBox.GetType(),"lbl ") )
                        {
                            char *pszLabel = (char *) oSubBox.ReadBoxData();
                            if( pszLabel != NULL && EQUAL(pszLabel,"gml.data") )
                            {
                                bGMLData = TRUE;
                            }
                            else
                                bUnsupportedConfiguration = TRUE;
                            CPLFree( pszLabel );
                        }
                        else
                            bUnsupportedConfiguration = TRUE;
                    }
                    else if( EQUAL(oBox.GetType(),"uuid") )
                    {
                        if( nOffsetUUID == 0 )
                            nOffsetUUID = VSIFTellL(fp);
                        if( GDALJP2Metadata::IsUUID_MSI(oBox.GetUUID()) )
                            bMSIBox = TRUE;
                        else if( !GDALJP2Metadata::IsUUID_XMP(oBox.GetUUID()) )
                            bUnsupportedConfiguration = TRUE;
                    }
                    else if( !EQUAL(oBox.GetType(),"jP  ") &&
                             !EQUAL(oBox.GetType(),"ftyp") &&
                             !EQUAL(oBox.GetType(),"rreq") &&
                             !EQUAL(oBox.GetType(),"jp2h") &&
                             !EQUAL(oBox.GetType(),"jp2i") )
                    {
                        bUnsupportedConfiguration = TRUE;
                    }

                    if (bUnsupportedConfiguration || !oBox.ReadNext())
                        break;
                }
            }

            const char* pszGMLJP2;
            int bGeoreferencingCompatOfGMLJP2 =
                       ((pszProjection != NULL && pszProjection[0] != '\0' ) &&
                        bGeoTransformValid && nGCPCount == 0);
            if( bGeoreferencingCompatOfGMLJP2 &&
                ((bHasGeoreferencingAtOpening && bGMLData) ||
                 (!bHasGeoreferencingAtOpening)) )
                pszGMLJP2 = "GMLJP2=YES";
            else
                pszGMLJP2 = "GMLJP2=NO";

            const char* pszGeoJP2;
            int bGeoreferencingCompatOfGeoJP2 = 
                    ((pszProjection != NULL && pszProjection[0] != '\0' ) ||
                    nGCPCount != 0 || bGeoTransformValid);
            if( bGeoreferencingCompatOfGeoJP2 &&
                ((bHasGeoreferencingAtOpening && bMSIBox) ||
                 (!bHasGeoreferencingAtOpening) || nGCPCount > 0) )
                pszGeoJP2 = "GeoJP2=YES";
            else
                pszGeoJP2 = "GeoJP2=NO";

            /* Test that the length of the JP2C box is not 0 */
            int bJP2CBoxOKForRewriteInPlace = TRUE;
            if( nOffsetJP2C > 16 && !bUnsupportedConfiguration )
            {
                VSIFSeekL(fp, nOffsetJP2C - 8, SEEK_SET);
                GByte abyBuffer[8];
                VSIFReadL(abyBuffer, 1, 8, fp);
                if( EQUALN((const char*)abyBuffer + 4, "jp2c", 4) &&
                    abyBuffer[0] == 0 && abyBuffer[1] == 0 &&
                    abyBuffer[2] == 0 && abyBuffer[3] == 0 )
                {
                    if( (vsi_l_offset)(GUInt32)(nLengthJP2C + 8) == (nLengthJP2C + 8) )
                    {
                        CPLDebug("OPENJPEG", "Patching length of JP2C box with real length");
                        VSIFSeekL(fp, nOffsetJP2C - 8, SEEK_SET);
                        GUInt32 nLength = (GUInt32)nLengthJP2C + 8;
                        CPL_MSBPTR32(&nLength);
                        VSIFWriteL(&nLength, 1, 4, fp);
                    }
                    else
                        bJP2CBoxOKForRewriteInPlace = FALSE;
                }
            }

            if( nOffsetJP2C == 0 || bUnsupportedConfiguration )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot rewrite file due to unsupported JP2 box configuration");
                VSIFCloseL( fp );
            }
            else if( bJP2CBoxOKForRewriteInPlace &&
                     (nOffsetXML == 0 || nOffsetXML > nOffsetJP2C) &&
                     (nOffsetASOC == 0 || nOffsetASOC > nOffsetJP2C) &&
                     (nOffsetUUID == 0 || nOffsetUUID > nOffsetJP2C) )
            {
                CPLDebug("OPENJPEG", "Rewriting boxes after codestream");

                /* Update IPR flag */
                if( nLengthIHDR == 14 )
                {
                    VSIFSeekL( fp, nOffsetIHDR + nLengthIHDR - 1, SEEK_SET );
                    GByte bIPR = GetMetadata("xml:IPR") != NULL;
                    VSIFWriteL( &bIPR, 1, 1, fp );
                }

                VSIFSeekL( fp, nOffsetJP2C + nLengthJP2C, SEEK_SET );

                GDALJP2Metadata oJP2MD;
                if( GetGCPCount() > 0 )
                {
                    oJP2MD.SetGCPs( GetGCPCount(),
                                    GetGCPs() );
                    oJP2MD.SetProjection( GetGCPProjection() );
                }
                else
                {
                    const char* pszWKT = GetProjectionRef();
                    if( pszWKT != NULL && pszWKT[0] != '\0' )
                    {
                        oJP2MD.SetProjection( pszWKT );
                    }
                    if( bGeoTransformValid )
                    {
                        oJP2MD.SetGeoTransform( adfGeoTransform );
                    }
                }

                const char* pszAreaOrPoint = GetMetadataItem(GDALMD_AREA_OR_POINT);
                oJP2MD.bPixelIsPoint = pszAreaOrPoint != NULL && EQUAL(pszAreaOrPoint, GDALMD_AOP_POINT);

                WriteIPRBox(fp, this, NULL);

                if( bGeoreferencingCompatOfGMLJP2 && EQUAL(pszGMLJP2, "GMLJP2=YES") )
                {
                    GDALJP2Box* poBox = oJP2MD.CreateGMLJP2(nRasterXSize,nRasterYSize);
                    WriteBox(fp, poBox);
                    delete poBox;
                }

                WriteXMLBoxes(fp, this, NULL);
                WriteGDALMetadataBox(fp, this, NULL);

                if( bGeoreferencingCompatOfGeoJP2 && EQUAL(pszGeoJP2, "GeoJP2=YES") )
                {
                    GDALJP2Box* poBox = oJP2MD.CreateJP2GeoTIFF();
                    WriteBox(fp, poBox);
                    delete poBox;
                }

                WriteXMPBox(fp, this, NULL);
                
                VSIFTruncateL( fp, VSIFTellL(fp) );

                VSIFCloseL( fp );
            }
            else
            {
                VSIFCloseL( fp );
                
                CPLDebug("OPENJPEG", "Rewriting whole file");

                const char* apszOptions[] = {
                    "USE_SRC_CODESTREAM=YES", "CODEC=JP2", "WRITE_METADATA=YES",
                    NULL, NULL, NULL };
                apszOptions[3] = pszGMLJP2;
                apszOptions[4] = pszGeoJP2;
                CPLString osTmpFilename(CPLSPrintf("%s.tmp", GetDescription()));
                GDALDataset* poOutDS = CreateCopy( osTmpFilename, this, FALSE,
                                                (char**)apszOptions, GDALDummyProgress, NULL );
                if( poOutDS )
                {
                    GDALClose(poOutDS);
                    VSIRename(osTmpFilename, GetDescription());
                }
                else
                    VSIUnlink(osTmpFilename);
                VSIUnlink(CPLSPrintf("%s.tmp.aux.xml", GetDescription()));
            }
        }
        else
            VSIFCloseL( fp );
    }

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
/*                           SetProjection()                            */
/************************************************************************/

CPLErr JP2OpenJPEGDataset::SetProjection( const char * pszProjectionIn )
{
    if( eAccess == GA_Update )
    {
        bRewrite = TRUE;
        CPLFree(pszProjection);
        pszProjection = (pszProjectionIn) ? CPLStrdup(pszProjectionIn) : CPLStrdup("");
        return CE_None;
    }
    else
        return GDALJP2AbstractDataset::SetProjection(pszProjectionIn);
}

/************************************************************************/
/*                           SetGeoTransform()                          */
/************************************************************************/

CPLErr JP2OpenJPEGDataset::SetGeoTransform( double *padfGeoTransform )
{
    if( eAccess == GA_Update )
    {
        bRewrite = TRUE;
        memcpy(adfGeoTransform, padfGeoTransform, 6* sizeof(double));
        bGeoTransformValid = !(
            adfGeoTransform[0] == 0.0 && adfGeoTransform[1] == 1.0 &&
            adfGeoTransform[2] == 0.0 && adfGeoTransform[3] == 0.0 &&
            adfGeoTransform[4] == 0.0 && adfGeoTransform[5] == 1.0);
        return CE_None;
    }
    else
        return GDALJP2AbstractDataset::SetGeoTransform(padfGeoTransform);
}

/************************************************************************/
/*                           SetGCPs()                                  */
/************************************************************************/

CPLErr JP2OpenJPEGDataset::SetGCPs( int nGCPCountIn, const GDAL_GCP *pasGCPListIn,
                                    const char *pszGCPProjectionIn )
{
    if( eAccess == GA_Update )
    {
        bRewrite = TRUE;
        CPLFree( pszProjection );
        if( nGCPCount > 0 )
        {
            GDALDeinitGCPs( nGCPCount, pasGCPList );
            CPLFree( pasGCPList );
        }

        pszProjection = (pszGCPProjectionIn) ? CPLStrdup(pszGCPProjectionIn) : CPLStrdup("");
        nGCPCount = nGCPCountIn;
        pasGCPList = GDALDuplicateGCPs( nGCPCount, pasGCPListIn );

        return CE_None;
    }
    else
        return GDALJP2AbstractDataset::SetGCPs(nGCPCountIn, pasGCPListIn,
                                               pszGCPProjectionIn);
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr JP2OpenJPEGDataset::SetMetadata( char ** papszMetadata,
                                        const char * pszDomain )
{
    if( eAccess == GA_Update )
    {
        bRewrite = TRUE;
        return GDALDataset::SetMetadata(papszMetadata, pszDomain);
    }
    return GDALJP2AbstractDataset::SetMetadata(papszMetadata, pszDomain);
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr JP2OpenJPEGDataset::SetMetadataItem( const char * pszName,
                                            const char * pszValue,
                                            const char * pszDomain )
{
    if( eAccess == GA_Update )
    {
        bRewrite = TRUE;
        return GDALDataset::SetMetadataItem(pszName, pszValue, pszDomain);
    }
    return GDALJP2AbstractDataset::SetMetadataItem(pszName, pszValue, pszDomain);
}

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

static const unsigned char jpc_header[] = {0xff,0x4f};
static const unsigned char jp2_box_jp[] = {0x6a,0x50,0x20,0x20}; /* 'jP  ' */

int JP2OpenJPEGDataset::Identify( GDALOpenInfo * poOpenInfo )

{
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
/*                        JP2OpenJPEGFindCodeStream()                   */
/************************************************************************/

static vsi_l_offset JP2OpenJPEGFindCodeStream( VSILFILE* fp,
                                               vsi_l_offset* pnLength )
{
    vsi_l_offset nCodeStreamStart = 0;
    vsi_l_offset nCodeStreamLength = 0;

    VSIFSeekL(fp, 0, SEEK_SET);
    GByte abyHeader[16];
    VSIFReadL(abyHeader, 1, 16, fp);

    if (memcmp( abyHeader, jpc_header, sizeof(jpc_header) ) == 0)
    {
        VSIFSeekL(fp, 0, SEEK_END);
        nCodeStreamLength = VSIFTellL(fp);
    }
    else if (memcmp( abyHeader + 4, jp2_box_jp, sizeof(jp2_box_jp) ) == 0)
    {
        /* Find offset of first jp2c box */
        GDALJP2Box oBox( fp );
        if( oBox.ReadFirst() )
        {
            while( strlen(oBox.GetType()) > 0 )
            {
                if( EQUAL(oBox.GetType(),"jp2c") )
                {
                    nCodeStreamStart = VSIFTellL(fp);
                    nCodeStreamLength = oBox.GetDataLength();
                    break;
                }

                if (!oBox.ReadNext())
                    break;
            }
        }
    }
    *pnLength = nCodeStreamLength;
    return nCodeStreamStart;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *JP2OpenJPEGDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if (!Identify(poOpenInfo) || poOpenInfo->fpL == NULL)
        return NULL;

    /* Detect which codec to use : J2K or JP2 ? */
    vsi_l_offset nCodeStreamLength = 0;
    vsi_l_offset nCodeStreamStart = JP2OpenJPEGFindCodeStream(poOpenInfo->fpL,
                                                              &nCodeStreamLength);

    if( nCodeStreamStart == 0 && nCodeStreamLength == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No code-stream in JP2 file");
        return NULL;
    }

    OPJ_CODEC_FORMAT eCodecFormat = (nCodeStreamStart == 0) ? OPJ_CODEC_J2K : OPJ_CODEC_JP2;


    opj_codec_t* pCodec;

    pCodec = opj_create_decompress(OPJ_CODEC_J2K);

    opj_set_info_handler(pCodec, JP2OpenJPEGDataset_InfoCallback,NULL);
    opj_set_warning_handler(pCodec, JP2OpenJPEGDataset_WarningCallback, NULL);
    opj_set_error_handler(pCodec, JP2OpenJPEGDataset_ErrorCallback,NULL);

    opj_dparameters_t parameters;
    opj_set_default_decoder_parameters(&parameters);

    if (! opj_setup_decoder(pCodec,&parameters))
    {
        return NULL;
    }

    JP2OpenJPEGFile sJP2OpenJPEGFile;
    sJP2OpenJPEGFile.fp = poOpenInfo->fpL;
    sJP2OpenJPEGFile.nBaseOffset = nCodeStreamStart;
    opj_stream_t * pStream = JP2OpenJPEGCreateReadStream(&sJP2OpenJPEGFile,
                                                         nCodeStreamLength);

    opj_image_t * psImage = NULL;

    if(!opj_read_header(pStream,pCodec,&psImage))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "opj_read_header() failed");
        opj_destroy_codec(pCodec);
        opj_stream_destroy(pStream);
        opj_image_destroy(psImage);
        return NULL;
    }

    opj_codestream_info_v2_t* pCodeStreamInfo = opj_get_cstr_info(pCodec);
    OPJ_UINT32 nTileW,nTileH;
    nTileW = pCodeStreamInfo->tdx;
    nTileH = pCodeStreamInfo->tdy;
#ifdef DEBUG
    OPJ_UINT32  nX0,nY0;
    OPJ_UINT32 nTilesX,nTilesY;
    nX0 = pCodeStreamInfo->tx0;
    nY0 = pCodeStreamInfo->ty0;
    nTilesX = pCodeStreamInfo->tw;
    nTilesY = pCodeStreamInfo->th;
    int mct = pCodeStreamInfo->m_default_tile_info.mct;
#endif
    int numResolutions = pCodeStreamInfo->m_default_tile_info.tccp_info[0].numresolutions;
    opj_destroy_cstr_info(&pCodeStreamInfo);

    if (psImage == NULL)
    {
        opj_destroy_codec(pCodec);
        opj_stream_destroy(pStream);
        opj_image_destroy(psImage);
        return NULL;
    }

#ifdef DEBUG
    int i;
    CPLDebug("OPENJPEG", "nX0 = %u", nX0);
    CPLDebug("OPENJPEG", "nY0 = %u", nY0);
    CPLDebug("OPENJPEG", "nTileW = %u", nTileW);
    CPLDebug("OPENJPEG", "nTileH = %u", nTileH);
    CPLDebug("OPENJPEG", "nTilesX = %u", nTilesX);
    CPLDebug("OPENJPEG", "nTilesY = %u", nTilesY);
    CPLDebug("OPENJPEG", "mct = %d", mct);
    CPLDebug("OPENJPEG", "psImage->x0 = %u", psImage->x0);
    CPLDebug("OPENJPEG", "psImage->y0 = %u", psImage->y0);
    CPLDebug("OPENJPEG", "psImage->x1 = %u", psImage->x1);
    CPLDebug("OPENJPEG", "psImage->y1 = %u", psImage->y1);
    CPLDebug("OPENJPEG", "psImage->numcomps = %d", psImage->numcomps);
    //CPLDebug("OPENJPEG", "psImage->color_space = %d", psImage->color_space);
    CPLDebug("OPENJPEG", "numResolutions = %d", numResolutions);
    for(i=0;i<(int)psImage->numcomps;i++)
    {
        CPLDebug("OPENJPEG", "psImage->comps[%d].dx = %u", i, psImage->comps[i].dx);
        CPLDebug("OPENJPEG", "psImage->comps[%d].dy = %u", i, psImage->comps[i].dy);
        CPLDebug("OPENJPEG", "psImage->comps[%d].x0 = %u", i, psImage->comps[i].x0);
        CPLDebug("OPENJPEG", "psImage->comps[%d].y0 = %u", i, psImage->comps[i].y0);
        CPLDebug("OPENJPEG", "psImage->comps[%d].w = %u", i, psImage->comps[i].w);
        CPLDebug("OPENJPEG", "psImage->comps[%d].h = %u", i, psImage->comps[i].h);
        CPLDebug("OPENJPEG", "psImage->comps[%d].resno_decoded = %d", i, psImage->comps[i].resno_decoded);
        CPLDebug("OPENJPEG", "psImage->comps[%d].factor = %d", i, psImage->comps[i].factor);
        CPLDebug("OPENJPEG", "psImage->comps[%d].prec = %d", i, psImage->comps[i].prec);
        CPLDebug("OPENJPEG", "psImage->comps[%d].sgnd = %d", i, psImage->comps[i].sgnd);
    }
#endif

    if (psImage->x1 <= psImage->x0 ||
        psImage->y1 <= psImage->y0 ||
        psImage->numcomps == 0 ||
        (psImage->comps[0].w >> 31) != 0 ||
        (psImage->comps[0].h >> 31) != 0 ||
        (nTileW >> 31) != 0 ||
        (nTileH >> 31) != 0 ||
        psImage->comps[0].w != psImage->x1 - psImage->x0 ||
        psImage->comps[0].h != psImage->y1 - psImage->y0)
    {
        CPLDebug("OPENJPEG", "Unable to handle that image (1)");
        opj_destroy_codec(pCodec);
        opj_stream_destroy(pStream);
        opj_image_destroy(psImage);
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
                    (psImage->numcomps == 3 || psImage->numcomps == 4) &&
                    psImage->comps[1].w == psImage->comps[0].w / 2 &&
                    psImage->comps[1].h == psImage->comps[0].h / 2 &&
                    psImage->comps[2].w == psImage->comps[0].w / 2 &&
                    psImage->comps[2].h == psImage->comps[0].h / 2) &&
                    (psImage->numcomps == 3 || 
                     (psImage->numcomps == 4 &&
                      psImage->comps[3].w == psImage->comps[0].w &&
                      psImage->comps[3].h == psImage->comps[0].h));

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
    if( eCodecFormat == OPJ_CODEC_JP2 )
        poDS->eAccess = poOpenInfo->eAccess;
    poDS->eColorSpace = psImage->color_space;
    poDS->nRasterXSize = psImage->x1 - psImage->x0;
    poDS->nRasterYSize = psImage->y1 - psImage->y0;
    poDS->nBands = psImage->numcomps;
    poDS->fp = poOpenInfo->fpL;
    poOpenInfo->fpL = NULL;
    poDS->nCodeStreamStart = nCodeStreamStart;
    poDS->nCodeStreamLength = nCodeStreamLength;
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

    GDALColorTable* poCT = NULL;

/* -------------------------------------------------------------------- */
/*      Look for color table or cdef box                                */
/* -------------------------------------------------------------------- */
    if( eCodecFormat == OPJ_CODEC_JP2 )
    {
        GDALJP2Box oBox( poDS->fp );
        if( oBox.ReadFirst() )
        {
            while( strlen(oBox.GetType()) > 0 )
            {
                if( EQUAL(oBox.GetType(),"jp2h") )
                {
                    GDALJP2Box oSubBox( poDS->fp );

                    for( oSubBox.ReadFirstChild( &oBox );
                         strlen(oSubBox.GetType()) > 0;
                         oSubBox.ReadNextChild( &oBox ) )
                    {
                        GIntBig nDataLength = oSubBox.GetDataLength();
                        if( poCT == NULL &&
                            EQUAL(oSubBox.GetType(),"pclr") &&
                            nDataLength >= 3 &&
                            nDataLength <= 2 + 1 + 4 + 4 * 256 )
                        {
                            GByte* pabyCT = oSubBox.ReadBoxData();
                            if( pabyCT != NULL )
                            {
                                int nEntries = (pabyCT[0] << 8) | pabyCT[1];
                                int nComponents = pabyCT[2];
                                /* CPLDebug("OPENJPEG", "Color table found"); */
                                if( nEntries <= 256 && nComponents == 3 )
                                {
                                    /*CPLDebug("OPENJPEG", "resol[0] = %d", pabyCT[3]);
                                    CPLDebug("OPENJPEG", "resol[1] = %d", pabyCT[4]);
                                    CPLDebug("OPENJPEG", "resol[2] = %d", pabyCT[5]);*/
                                    if( pabyCT[3] == 7 && pabyCT[4] == 7 && pabyCT[5] == 7 &&
                                        nDataLength == 2 + 1 + 3 + 3 * nEntries )
                                    {
                                        poCT = new GDALColorTable();
                                        for(int i=0;i<nEntries;i++)
                                        {
                                            GDALColorEntry sEntry;
                                            sEntry.c1 = pabyCT[6 + 3 * i];
                                            sEntry.c2 = pabyCT[6 + 3 * i + 1];
                                            sEntry.c3 = pabyCT[6 + 3 * i + 2];
                                            sEntry.c4 = 255;
                                            poCT->SetColorEntry(i, &sEntry);
                                        }
                                    }
                                }
                                else if ( nEntries <= 256 && nComponents == 4 )
                                {
                                    if( pabyCT[3] == 7 && pabyCT[4] == 7 &&
                                        pabyCT[5] == 7 && pabyCT[6] == 7 &&
                                        nDataLength == 2 + 1 + 4 + 4 * nEntries )
                                    {
                                        poCT = new GDALColorTable();
                                        for(int i=0;i<nEntries;i++)
                                        {
                                            GDALColorEntry sEntry;
                                            sEntry.c1 = pabyCT[7 + 4 * i];
                                            sEntry.c2 = pabyCT[7 + 4 * i + 1];
                                            sEntry.c3 = pabyCT[7 + 4 * i + 2];
                                            sEntry.c4 = pabyCT[7 + 4 * i + 3];
                                            poCT->SetColorEntry(i, &sEntry);
                                        }
                                    }
                                }
                                CPLFree(pabyCT);
                            }
                        }
                        /* There's a bug/misfeature in openjpeg: the color_space
                           only gets set at read tile time */
                        else if( EQUAL(oSubBox.GetType(),"colr") &&
                                 nDataLength == 7 )
                        {
                            GByte* pabyContent = oSubBox.ReadBoxData();
                            if( pabyContent != NULL )
                            {
                                if( pabyContent[0] == 1 /* enumerated colourspace */ )
                                {
                                    GUInt32 enumcs = (pabyContent[3] << 24) |
                                                     (pabyContent[4] << 16) |
                                                     (pabyContent[5] << 8) |
                                                     (pabyContent[6]);
                                    if( enumcs == 16 )
                                    {
                                        poDS->eColorSpace = OPJ_CLRSPC_SRGB;
                                        CPLDebug("OPENJPEG", "SRGB color space");
                                    }
                                    else if( enumcs == 17 )
                                    {
                                        poDS->eColorSpace = OPJ_CLRSPC_GRAY;
                                        CPLDebug("OPENJPEG", "Grayscale color space");
                                    }
                                    else if( enumcs == 18 )
                                    {
                                        poDS->eColorSpace = OPJ_CLRSPC_SYCC;
                                        CPLDebug("OPENJPEG", "SYCC color space");
                                    }
                                    else if( enumcs == 20 )
                                    {
                                        /* Used by J2KP4files/testfiles_jp2/file7.jp2 */
                                        poDS->eColorSpace = OPJ_CLRSPC_SRGB;
                                        CPLDebug("OPENJPEG", "e-sRGB color space");
                                    }
                                    else if( enumcs == 21 )
                                    {
                                        /* Used by J2KP4files/testfiles_jp2/file5.jp2 */
                                        poDS->eColorSpace = OPJ_CLRSPC_SRGB;
                                        CPLDebug("OPENJPEG", "ROMM-RGB color space");
                                    }
                                    else
                                    {
                                        poDS->eColorSpace = OPJ_CLRSPC_UNKNOWN;
                                        CPLDebug("OPENJPEG", "Unknown color space");
                                    }
                                }
                                CPLFree(pabyContent);
                            }
                        }
                        /* Check if there's an alpha channel or odd channel attribution */
                        else if( EQUAL(oSubBox.GetType(),"cdef") &&
                                 nDataLength == 2 + poDS->nBands * 6 )
                        {
                            GByte* pabyContent = oSubBox.ReadBoxData();
                            if( pabyContent != NULL )
                            {
                                int nEntries = (pabyContent[0] << 8) | pabyContent[1];
                                if( nEntries == poDS->nBands )
                                {
                                    poDS->nRedIndex = -1;
                                    poDS->nGreenIndex = -1;
                                    poDS->nBlueIndex = -1;
                                    for(int i=0;i<poDS->nBands;i++)
                                    {
                                        int CNi = (pabyContent[2+6*i] << 8) | pabyContent[2+6*i+1];
                                        int Typi = (pabyContent[2+6*i+2] << 8) | pabyContent[2+6*i+3];
                                        int Asoci = (pabyContent[2+6*i+4] << 8) | pabyContent[2+6*i+5];
                                        if( CNi < 0 || CNi >= poDS->nBands )
                                        {
                                            CPLError(CE_Failure, CPLE_AppDefined,
                                                     "Wrong value of CN%d=%d", i, CNi);
                                            break;
                                        }
                                        if( Typi == 0 )
                                        {
                                            if( Asoci == 1 )
                                                poDS->nRedIndex = CNi;
                                            else if( Asoci == 2 )
                                                poDS->nGreenIndex = CNi;
                                            else if( Asoci == 3 )
                                                poDS->nBlueIndex = CNi;
                                            else if( Asoci < 0 || (Asoci > poDS->nBands && Asoci != 65535) )
                                            {
                                                CPLError(CE_Failure, CPLE_AppDefined,
                                                     "Wrong value of Asoc%d=%d", i, Asoci);
                                                break;
                                            }
                                        }
                                        else if( Typi == 1 )
                                        {
                                            poDS->nAlphaIndex = CNi;
                                        }
                                    }
                                }
                                else
                                {
                                    CPLDebug("OPENJPEG", "Unsupported cdef content");
                                }
                                CPLFree(pabyContent);
                            }
                        }
                    }
                }

                if (!oBox.ReadNext())
                    break;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( iBand = 1; iBand <= poDS->nBands; iBand++ )
    {
        int bPromoteTo8Bit = (
            iBand == poDS->nAlphaIndex + 1 &&
            psImage->comps[(poDS->nAlphaIndex==0 && poDS->nBands > 1) ? 1 : 0].prec == 8 &&
            psImage->comps[poDS->nAlphaIndex ].prec == 1 && 
            CSLFetchBoolean(poOpenInfo->papszOpenOptions, "1BIT_ALPHA_PROMOTION",
                    CSLTestBoolean(CPLGetConfigOption("JP2OPENJPEG_PROMOTE_1BIT_ALPHA_AS_8BIT", "YES"))) );
        if( bPromoteTo8Bit )
            CPLDebug("JP2OpenJPEG", "Alpha band is promoted from 1 bit to 8 bit");

        JP2OpenJPEGRasterBand* poBand =
            new JP2OpenJPEGRasterBand( poDS, iBand, eDataType,
                                        bPromoteTo8Bit ? 8: psImage->comps[iBand-1].prec,
                                        bPromoteTo8Bit,
                                        nTileW, nTileH);
        if( iBand == 1 && poCT != NULL )
            poBand->poCT = poCT;
        poDS->SetBand( iBand, poBand );
    }

/* -------------------------------------------------------------------- */
/*      Create overview datasets.                                       */
/* -------------------------------------------------------------------- */
    int nW = poDS->nRasterXSize;
    int nH = poDS->nRasterYSize;

    /* Lower resolutions are not compatible with a color-table */
    if( poCT != NULL )
        numResolutions = 0;

    while (poDS->nOverviewCount+1 < numResolutions &&
           (nW > 128 || nH > 128) &&
           (poDS->bUseSetDecodeArea || ((nTileW % 2) == 0 && (nTileH % 2) == 0)))
    {
        nW /= 2;
        nH /= 2;

        poDS->papoOverviewDS = (JP2OpenJPEGDataset**) CPLRealloc(
                    poDS->papoOverviewDS,
                    (poDS->nOverviewCount + 1) * sizeof(JP2OpenJPEGDataset*));
        JP2OpenJPEGDataset* poODS = new JP2OpenJPEGDataset();
        poODS->SetDescription( poOpenInfo->pszFilename );
        poODS->iLevel = poDS->nOverviewCount + 1;
        poODS->bUseSetDecodeArea = poDS->bUseSetDecodeArea;
        poODS->nRedIndex = poDS->nRedIndex;
        poODS->nGreenIndex = poDS->nGreenIndex;
        poODS->nBlueIndex = poDS->nBlueIndex;
        poODS->nAlphaIndex = poDS->nAlphaIndex;
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

        poODS->eColorSpace = poDS->eColorSpace;
        poODS->nRasterXSize = nW;
        poODS->nRasterYSize = nH;
        poODS->nBands = poDS->nBands;
        poODS->fp = poDS->fp;
        poODS->nCodeStreamStart = nCodeStreamStart;
        poODS->nCodeStreamLength = nCodeStreamLength;
        poODS->bIs420 = bIs420;
        for( iBand = 1; iBand <= poDS->nBands; iBand++ )
        {
            int bPromoteTo8Bit = (
                iBand == poDS->nAlphaIndex + 1 &&
                psImage->comps[(poDS->nAlphaIndex==0 && poDS->nBands > 1) ? 1 : 0].prec == 8 &&
                psImage->comps[poDS->nAlphaIndex].prec == 1 && 
                CSLFetchBoolean(poOpenInfo->papszOpenOptions, "1BIT_ALPHA_PROMOTION",
                        CSLTestBoolean(CPLGetConfigOption("JP2OPENJPEG_PROMOTE_1BIT_ALPHA_AS_8BIT", "YES"))) );

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
        poDS->GDALDataset::SetMetadataItem( "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE" );
    }

    poOpenInfo->fpL = poDS->fp;
    poDS->LoadJP2Metadata(poOpenInfo);
    poOpenInfo->fpL = NULL;

    poDS->bHasGeoreferencingAtOpening = 
        ((poDS->pszProjection != NULL && poDS->pszProjection[0] != '\0' )||
         poDS->nGCPCount != 0 || poDS->bGeoTransformValid);

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

    if( poBox == NULL )
        return;

    nLBox = (int) poBox->GetDataLength() + 8;
    nLBox = CPL_MSBWORD32( nLBox );

    memcpy(&nTBox, poBox->GetType(), 4);

    VSIFWriteL( &nLBox, 4, 1, fp );
    VSIFWriteL( &nTBox, 4, 1, fp );
    VSIFWriteL(poBox->GetWritableData(), 1, (int) poBox->GetDataLength(), fp);
}

/************************************************************************/
/*                         WriteGDALMetadataBox()                       */
/************************************************************************/

void JP2OpenJPEGDataset::WriteGDALMetadataBox( VSILFILE* fp,
                                               GDALDataset* poSrcDS,
                                               char** papszOptions )
{
    GDALJP2Box* poBox = GDALJP2Metadata::CreateGDALMultiDomainMetadataXMLBox(
        poSrcDS, CSLFetchBoolean(papszOptions, "MAIN_MD_DOMAIN_ONLY", FALSE));
    if( poBox )
        WriteBox(fp, poBox);
    delete poBox;
}

/************************************************************************/
/*                         WriteXMLBoxes()                              */
/************************************************************************/

void JP2OpenJPEGDataset::WriteXMLBoxes( VSILFILE* fp, GDALDataset* poSrcDS,
                                         CPL_UNUSED char** papszOptions )
{
    int nBoxes = 0;
    GDALJP2Box** papoBoxes = GDALJP2Metadata::CreateXMLBoxes(poSrcDS, &nBoxes);
    for(int i=0;i<nBoxes;i++)
    {
        WriteBox(fp, papoBoxes[i]);
        delete papoBoxes[i];
    }
    CPLFree(papoBoxes);
}

/************************************************************************/
/*                           WriteXMPBox()                              */
/************************************************************************/

void JP2OpenJPEGDataset::WriteXMPBox ( VSILFILE* fp, GDALDataset* poSrcDS,
                                       CPL_UNUSED char** papszOptions )
{
    GDALJP2Box* poBox = GDALJP2Metadata::CreateXMPBox(poSrcDS);
    if( poBox )
        WriteBox(fp, poBox);
    delete poBox;
}

/************************************************************************/
/*                           WriteIPRBox()                              */
/************************************************************************/

void JP2OpenJPEGDataset::WriteIPRBox ( VSILFILE* fp, GDALDataset* poSrcDS,
                                       CPL_UNUSED char** papszOptions )
{
    GDALJP2Box* poBox = GDALJP2Metadata::CreateIPRBox(poSrcDS);
    if( poBox )
        WriteBox(fp, poBox);
    delete poBox;
}
/************************************************************************/
/*                         FloorPowerOfTwo()                            */
/************************************************************************/

static int FloorPowerOfTwo(int nVal)
{
    int nBits = 0;
    while( nVal > 1 )
    {
        nBits ++;
        nVal >>= 1;
    }
    return 1 << nBits;
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

    if( nBands == 0 || nBands > 16384 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Unable to export files with %d bands. Must be >= 1 and <= 16384", nBands );
        return NULL;
    }

    GDALColorTable* poCT = poSrcDS->GetRasterBand(1)->GetColorTable();
    if (poCT != NULL && nBands != 1)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "JP2OpenJPEG driver only supports a color table for a single-band dataset");
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

    int bInspireTG = CSLFetchBoolean(papszOptions, "INSPIRE_TG", FALSE);

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
    if( eCodecFormat != OPJ_CODEC_JP2 && bInspireTG )
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                  "INSPIRE_TG=YES mandates CODEC=JP2 (TG requirement 21)");
        return NULL;
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
            ! (CSLFetchBoolean(papszOptions, "REVERSIBLE", poCT != NULL));

    std::vector<double> adfRates;
    const char* pszQuality = CSLFetchNameValueDef(papszOptions, "QUALITY", NULL);
    double dfDefaultQuality = ( poCT != NULL ) ? 100.0 : 25.0;
    if (pszQuality)
    {
        char **papszTokens = CSLTokenizeStringComplex( pszQuality, ",", FALSE, FALSE );
        for(int i=0; papszTokens[i] != NULL; i++ )
        {
            double dfQuality = CPLAtof(papszTokens[i]);
            if (dfQuality > 0 && dfQuality <= 100)
            {
                double dfRate = 100 / dfQuality;
                adfRates.push_back(dfRate);
            }
            else
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                         "Unsupported value for QUALITY: %s. Defaulting to single-layer, with quality=%.0f",
                         papszTokens[i], dfDefaultQuality);
                adfRates.resize(0);
                break;
            }
        }
        if( papszTokens[0] == NULL )
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Unsupported value for QUALITY: %s. Defaulting to single-layer, with quality=%.0f",
                     pszQuality, dfDefaultQuality);
        }
        CSLDestroy(papszTokens);
    }
    if( adfRates.size() == 0 )
    {
        adfRates.push_back(100. / dfDefaultQuality);
    }

    if( poCT != NULL && (bIsIrreversible || adfRates[adfRates.size()-1] != 100.0 / 100.0) )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Encoding a dataset with a color table with REVERSIBLE != YES "
                 "or QUALITY != 100 will likely lead to bad visual results");
    }

    int nMaxTileDim = MAX(nBlockXSize, nBlockYSize);
    int nNumResolutions = 1;
    /* Pickup a reasonable value compatible with PROFILE_1 requirements */
    while( (nMaxTileDim >> (nNumResolutions-1)) > 128 )
        nNumResolutions ++;
    int nMinProfile1Resolutions = nNumResolutions;
    const char* pszResolutions = CSLFetchNameValueDef(papszOptions, "RESOLUTIONS", NULL);
    if (pszResolutions)
    {
        nNumResolutions = atoi(pszResolutions);
        if (nNumResolutions <= 0 || nNumResolutions >= 32 ||
            (nMaxTileDim >> nNumResolutions) == 0 )
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                 "Unsupported value for RESOLUTIONS : %s. Defaulting to %d",
                 pszResolutions, nMinProfile1Resolutions);
            nNumResolutions = nMinProfile1Resolutions;
        }
    }

    int bSOP = CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "SOP", "FALSE"));
    int bEPH = CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "EPH", "FALSE"));

    int nRedBandIndex = -1, nGreenBandIndex = -1, nBlueBandIndex = -1;
    int nAlphaBandIndex = -1;
    for(int i=0;i<nBands;i++)
    {
        GDALColorInterp eInterp = poSrcDS->GetRasterBand(i+1)->GetColorInterpretation();
        if( eInterp == GCI_RedBand )
            nRedBandIndex = i;
        else if( eInterp == GCI_GreenBand )
            nGreenBandIndex = i;
        else if( eInterp == GCI_BlueBand )
            nBlueBandIndex = i;
        else if( eInterp == GCI_AlphaBand )
            nAlphaBandIndex = i;
    }
    const char* pszAlpha = CSLFetchNameValue(papszOptions, "ALPHA");
    if( nAlphaBandIndex < 0 && nBands > 1 && pszAlpha != NULL && CSLTestBoolean(pszAlpha) )
    {
        nAlphaBandIndex = nBands - 1;
    }

    const char* pszYCBCR420 = CSLFetchNameValue(papszOptions, "YCBCR420");
    int bYCBCR420 = FALSE;
    if( pszYCBCR420 && CSLTestBoolean(pszYCBCR420) )
    {
        if ((nBands == 3 || nBands == 4) && eDataType == GDT_Byte &&
            nRedBandIndex == 0 && nGreenBandIndex == 1 && nBlueBandIndex == 2)
        {
            if( ((nXSize % 2) == 0 && (nYSize % 2) == 0 && (nBlockXSize % 2) == 0 && (nBlockYSize % 2) == 0) )
            {
                bYCBCR420 = TRUE;
            }
            else
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                    "YCBCR420 unsupported when image size and/or tile size are not multiple of 2");
            }
        }
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                    "YCBCR420 unsupported with this image band count and/or data byte");
        }
    }

    const char* pszYCC = CSLFetchNameValue(papszOptions, "YCC");
    int bYCC = ((nBands == 3 || nBands == 4) && eDataType == GDT_Byte &&
            CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "YCC", "TRUE")));
    if( bYCBCR420 && bYCC )
    {
        if( pszYCC != NULL )
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                    "YCC unsupported when YCbCr requesting");
        }
        bYCC = FALSE;
    }
    
/* -------------------------------------------------------------------- */
/*      Deal with codeblocks size                                       */
/* -------------------------------------------------------------------- */

    int nCblockW = atoi(CSLFetchNameValueDef( papszOptions, "CODEBLOCK_WIDTH", "64" ));
    int nCblockH = atoi(CSLFetchNameValueDef( papszOptions, "CODEBLOCK_HEIGHT", "64" ));
    if( nCblockW < 4 || nCblockW > 1024 || nCblockH < 4 || nCblockH > 1024 )
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Invalid values for codeblock size. Defaulting to 64x64");
        nCblockW = 64;
        nCblockH = 64;
    }
    else if( nCblockW * nCblockH > 4096 )
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Invalid values for codeblock size. "
                 "CODEBLOCK_WIDTH * CODEBLOCK_HEIGHT should be <= 4096. "
                 "Defaulting to 64x64");
        nCblockW = 64;
        nCblockH = 64;
    }
    int nCblockW_po2 = FloorPowerOfTwo(nCblockW);
    int nCblockH_po2 = FloorPowerOfTwo(nCblockH);
    if( nCblockW_po2 != nCblockW || nCblockH_po2 != nCblockH )
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Non power of two values used for codeblock size. "
                 "Using to %dx%d",
                 nCblockW_po2, nCblockH_po2);
    }
    nCblockW = nCblockW_po2;
    nCblockH = nCblockH_po2;

/* -------------------------------------------------------------------- */
/*      Deal with codestream PROFILE                                    */
/* -------------------------------------------------------------------- */
    const char* pszProfile = CSLFetchNameValueDef( papszOptions, "PROFILE", "AUTO" );
    int bProfile1 = FALSE;
    if( EQUAL(pszProfile, "UNRESTRICTED") )
    {
        bProfile1 = FALSE;
        if( bInspireTG )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                    "INSPIRE_TG=YES mandates PROFILE=PROFILE_1 (TG requirement 21)");
            return NULL;
        }
    }
    else if( EQUAL(pszProfile, "UNRESTRICTED_FORCED") )
    {
        bProfile1 = FALSE;
    }
    else if( EQUAL(pszProfile, "PROFILE_1_FORCED") ) /* For debug only: can produce inconsistent codestream */
    {
        bProfile1 = TRUE;
    }
    else
    {
        if( !(EQUAL(pszProfile, "PROFILE_1") || EQUAL(pszProfile, "AUTO")) )
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Unsupported value for PROFILE : %s. Defaulting to AUTO",
                     pszProfile);
            pszProfile = "AUTO";
        }

        bProfile1 = TRUE;
        const char* pszReq21OrEmpty = (bInspireTG) ? " (TG requirement 21)" : "";
        if( (nBlockXSize != nXSize || nBlockYSize != nYSize) &&
            (nBlockXSize != nBlockYSize || nBlockXSize > 1024 || nBlockYSize > 1024 ) )
        {
            bProfile1 = FALSE;
            if( bInspireTG || EQUAL(pszProfile, "PROFILE_1") )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Tile dimensions incompatible with PROFILE_1%s. "
                         "Should be whole image or square with dimension <= 1024.",
                         pszReq21OrEmpty);
                return NULL;
            }
        }
        if( (nMaxTileDim >> (nNumResolutions-1)) > 128 )
        {
            bProfile1 = FALSE;
            if( bInspireTG || EQUAL(pszProfile, "PROFILE_1") )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Number of resolutions incompatible with PROFILE_1%s. "
                         "Should be at least %d.",
                         pszReq21OrEmpty,
                         nMinProfile1Resolutions);
                return NULL;
            }
        }
        if( nCblockW > 64 || nCblockH > 64 )
        {
            bProfile1 = FALSE;
            if( bInspireTG || EQUAL(pszProfile, "PROFILE_1") )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Codeblock width incompatible with PROFILE_1%s. "
                         "Codeblock width or height should be <= 64.",
                         pszReq21OrEmpty);
                return NULL;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Work out the precision.                                         */
/* -------------------------------------------------------------------- */
    int nBits;
    if( CSLFetchNameValue( papszOptions, "NBITS" ) != NULL )
    {
        nBits = atoi(CSLFetchNameValue(papszOptions,"NBITS"));
        if( bInspireTG &&
            !(nBits == 1 || nBits == 8 || nBits == 16 || nBits == 32) )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                    "INSPIRE_TG=YES mandates NBITS=1,8,16 or 32 (TG requirement 24)");
            return NULL;
        }
    }
    else if( poSrcDS->GetRasterBand(1)->GetMetadataItem( "NBITS", "IMAGE_STRUCTURE" ) 
             != NULL )
    {
        nBits = atoi(poSrcDS->GetRasterBand(1)->GetMetadataItem( "NBITS", 
                                                       "IMAGE_STRUCTURE" ));
        if( bInspireTG && 
            !(nBits == 1 || nBits == 8 || nBits == 16 || nBits == 32) )
        {
            /* Implements "NOTE If the original data do not satisfy this "
               "requirement, they will be converted in a representation using "
               "the next higher power of 2" */
            nBits = GDALGetDataTypeSize(eDataType);
        }
    }
    else
    {
        nBits = GDALGetDataTypeSize(eDataType);
    }

    if( (GDALGetDataTypeSize(eDataType) == 8 && nBits > 8) ||
        (GDALGetDataTypeSize(eDataType) == 16 && (nBits <= 8 || nBits > 16)) ||
        (GDALGetDataTypeSize(eDataType) == 32 && (nBits <= 16 || nBits > 32)) )
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Inconsistant NBITS value with data type. Using %d",
                 GDALGetDataTypeSize(eDataType));
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
    parameters.tcp_numlayers = (int)adfRates.size();
    for(int i=0;i<(int)adfRates.size();i++)
        parameters.tcp_rates[i] = (float) adfRates[i];
    parameters.cp_tx0 = 0;
    parameters.cp_ty0 = 0;
    parameters.tile_size_on = TRUE;
    parameters.cp_tdx = nBlockXSize;
    parameters.cp_tdy = nBlockYSize;
    parameters.irreversible = bIsIrreversible;
    parameters.numresolution = nNumResolutions;
    parameters.prog_order = eProgOrder;
    parameters.tcp_mct = bYCC;
    parameters.cblockw_init = nCblockW;
    parameters.cblockh_init = nCblockH;

    /* Add precincts */
    const char* pszPrecincts = CSLFetchNameValueDef(papszOptions, "PRECINCTS",
        "{512,512},{256,512},{128,512},{64,512},{32,512},{16,512},{8,512},{4,512},{2,512}");
    char **papszTokens = CSLTokenizeStringComplex( pszPrecincts, "{},", FALSE, FALSE );
    int nPrecincts = CSLCount(papszTokens) / 2;
    for(int i=0;i<nPrecincts && i < OPJ_J2K_MAXRLVLS;i++)
    {
        int nPCRW = atoi(papszTokens[2*i]);
        int nPCRH = atoi(papszTokens[2*i+1]);
        if( nPCRW < 1 || nPCRH < 1 )
            break;
        parameters.csty |= 0x01;
        parameters.res_spec ++;
        parameters.prcw_init[i] = nPCRW;
        parameters.prch_init[i] = nPCRH;
    }
    CSLDestroy(papszTokens);

    /* Add tileparts setting */
    const char* pszTileParts = CSLFetchNameValueDef(papszOptions, "TILEPARTS", "DISABLED");
    if( EQUAL(pszTileParts, "RESOLUTIONS") )
    {
        parameters.tp_on = 1;
        parameters.tp_flag = 'R';
    }
    else if( EQUAL(pszTileParts, "LAYERS") )
    {
        if( parameters.tcp_numlayers == 1 )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "TILEPARTS=LAYERS has no real interest with single-layer codestream");
        }
        parameters.tp_on = 1;
        parameters.tp_flag = 'L';
    }
    else if( EQUAL(pszTileParts, "COMPONENTS") )
    {
        parameters.tp_on = 1;
        parameters.tp_flag = 'C';
    }
    else if( !EQUAL(pszTileParts, "DISABLED") )
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Invalid value for TILEPARTS");
    }

    if( bProfile1 )
    {
#if defined(OPENJPEG_VERSION) && OPENJPEG_VERSION >= 20100
        parameters.rsiz = OPJ_PROFILE_1;
#else
        /* This is a hack but this works */
        parameters.cp_rsiz = (OPJ_RSIZ_CAPABILITIES) 2; /* Profile 1 */
#endif
    }

    opj_image_cmptparm_t* pasBandParams =
            (opj_image_cmptparm_t*)CPLMalloc(nBands * sizeof(opj_image_cmptparm_t));
    int iBand;
    int bSamePrecision = TRUE;
    int b1BitAlpha = FALSE;
    for(iBand=0;iBand<nBands;iBand++)
    {
        pasBandParams[iBand].x0 = 0;
        pasBandParams[iBand].y0 = 0;
        if (bYCBCR420 && (iBand == 1 || iBand == 2))
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
        pasBandParams[iBand].prec = nBits;

        const char* pszNBits = poSrcDS->GetRasterBand(iBand+1)->GetMetadataItem(
            "NBITS", "IMAGE_STRUCTURE");
        /* Recommendation 38 In the case of an opacity channel, the bit depth should be 1-bit. */
        if( iBand == nAlphaBandIndex &&
            ((pszNBits != NULL && EQUAL(pszNBits, "1")) ||
              CSLFetchBoolean(papszOptions, "1BIT_ALPHA", bInspireTG)) )
        {
            if( iBand != nBands - 1 && nBits != 1 )
            {
                /* Might be a bug in openjpeg, but it seems that if the alpha */
                /* band is the first one, it would select 1-bit for all channels... */
                CPLError(CE_Warning, CPLE_NotSupported,
                         "Cannot output 1-bit alpha channel if it is not the last one");
            }
            else
            {
                CPLDebug("OPENJPEG", "Using 1-bit alpha channel");
                pasBandParams[iBand].sgnd = 0;
                pasBandParams[iBand].prec = 1;
                bSamePrecision = FALSE;
                b1BitAlpha = TRUE;
            }
        }
    }

    if( bInspireTG && nAlphaBandIndex >= 0 && !b1BitAlpha )
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                  "INSPIRE_TG=YES recommends 1BIT_ALPHA=YES (Recommendation 38)");
    }

    /* Always ask OpenJPEG to do codestream only. We will take care */
    /* of JP2 boxes */
    opj_codec_t* pCodec = opj_create_compress(OPJ_CODEC_J2K);
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

    OPJ_COLOR_SPACE eColorSpace = OPJ_CLRSPC_GRAY;

    if( bYCBCR420 )
    {
        eColorSpace = OPJ_CLRSPC_SYCC;
    }
    else if( (nBands == 3 || nBands == 4) &&
             nRedBandIndex >= 0 && nGreenBandIndex >= 0 && nBlueBandIndex >= 0 )
    {
        eColorSpace = OPJ_CLRSPC_SRGB;
    }
    else if (poCT != NULL)
    {
        eColorSpace = OPJ_CLRSPC_SRGB;
    }

    opj_image_t* psImage = opj_image_tile_create(nBands,pasBandParams,
                                                 eColorSpace);

    if (psImage == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "opj_image_tile_create() failed");
        opj_destroy_codec(pCodec);
        CPLFree(pasBandParams);
        pasBandParams = NULL;
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
        CPLFree(pasBandParams);
        pasBandParams = NULL;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Setup GML and GeoTIFF information.                              */
/* -------------------------------------------------------------------- */
    GDALJP2Metadata oJP2MD;

    int bGeoreferencingCompatOfGeoJP2 = FALSE;
    int bGeoreferencingCompatOfGMLJP2 = FALSE;
    int bGMLJP2Option = CSLFetchBoolean( papszOptions, "GMLJP2", TRUE );
    int bGeoJP2Option = CSLFetchBoolean( papszOptions, "GeoJP2", TRUE );
    if( eCodecFormat == OPJ_CODEC_JP2 && (bGMLJP2Option || bGeoJP2Option) )
    {
        if( poSrcDS->GetGCPCount() > 0 )
        {
            bGeoreferencingCompatOfGeoJP2 = TRUE;
            oJP2MD.SetGCPs( poSrcDS->GetGCPCount(),
                            poSrcDS->GetGCPs() );
            oJP2MD.SetProjection( poSrcDS->GetGCPProjection() );
        }
        else
        {
            const char* pszWKT = poSrcDS->GetProjectionRef();
            if( pszWKT != NULL && pszWKT[0] != '\0' )
            {
                bGeoreferencingCompatOfGeoJP2 = TRUE;
                oJP2MD.SetProjection( pszWKT );
            }
            double adfGeoTransform[6];
            if( poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None )
            {
                bGeoreferencingCompatOfGeoJP2 = TRUE;
                oJP2MD.SetGeoTransform( adfGeoTransform );
            }
            bGeoreferencingCompatOfGMLJP2 =
                        ( pszWKT != NULL && pszWKT[0] != '\0' ) && 
                          poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None;
        }

        const char* pszAreaOrPoint = poSrcDS->GetMetadataItem(GDALMD_AREA_OR_POINT);
        oJP2MD.bPixelIsPoint = pszAreaOrPoint != NULL && EQUAL(pszAreaOrPoint, GDALMD_AOP_POINT);

        if( bGMLJP2Option && CPLGetConfigOption("GMLJP2OVERRIDE", NULL) != NULL )
            bGeoreferencingCompatOfGMLJP2 = TRUE;
    }

    if( CSLFetchNameValue( papszOptions, "GMLJP2" ) != NULL && bGMLJP2Option &&
        !bGeoreferencingCompatOfGMLJP2 )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "GMLJP2 box was explicitely required but cannot be written due "
                 "to lack of georeferencing and/or unsupported georeferencing for GMLJP2");
    }

    if( CSLFetchNameValue( papszOptions, "GeoJP2" ) != NULL && bGeoJP2Option &&
        !bGeoreferencingCompatOfGeoJP2 )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "GeoJP2 box was explicitely required but cannot be written due "
                 "to lack of georeferencing");
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

/* -------------------------------------------------------------------- */
/*      Add JP2 boxes.                                                  */
/* -------------------------------------------------------------------- */
    vsi_l_offset nStartJP2C;
    int bUseXLBoxes = FALSE;
    int bGeoBoxesAfter = CSLFetchBoolean(papszOptions, "GEOBOXES_AFTER_JP2C",
                                         bInspireTG);

    if( eCodecFormat == OPJ_CODEC_JP2  )
    {
        GDALJP2Box jPBox(fp);
        jPBox.SetType("jP  ");
        jPBox.AppendWritableData(4, "\x0D\x0A\x87\x0A");
        WriteBox(fp, &jPBox);

        GDALJP2Box ftypBox(fp);
        ftypBox.SetType("ftyp");
        ftypBox.AppendWritableData(4, "jp2 "); /* Branding */
        ftypBox.AppendUInt32(0); /* minimum version */
        ftypBox.AppendWritableData(4, "jp2 "); /* Compatibility list: first value */

        int bJPXOption = CSLFetchBoolean( papszOptions, "JPX", TRUE );
        if( bInspireTG && bGeoreferencingCompatOfGMLJP2 && bGMLJP2Option && !bJPXOption )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "INSPIRE_TG=YES implies following GMLJP2 specification which "
                     "recommends advertize reader requirement 67 feature, and thus JPX capability");
        }
        else if( bGeoreferencingCompatOfGMLJP2 && bGMLJP2Option && bJPXOption )
        {
            /* GMLJP2 uses lbl and asoc boxes, which are JPEG2000 Part II spec */
            /* advertizing jpx is required per 8.1 of 05-047r3 GMLJP2 */
            ftypBox.AppendWritableData(4, "jpx "); /* Compatibility list: second value */
        }
        WriteBox(fp, &ftypBox);

        int bIPR = poSrcDS->GetMetadata("xml:IPR") != NULL &&
                   CSLFetchBoolean(papszOptions, "WRITE_METADATA", FALSE);

        /* Reader requirement box */
        if( bGeoreferencingCompatOfGMLJP2 && bGMLJP2Option && bJPXOption )
        {
            GDALJP2Box rreqBox(fp);
            rreqBox.SetType("rreq");
            rreqBox.AppendUInt8(1); /* ML = 1 byte for mask length */

            rreqBox.AppendUInt8(0x80 | 0x40 | (bIPR ? 0x20 : 0)); /* FUAM */
            rreqBox.AppendUInt8(0x80); /* DCM */

            rreqBox.AppendUInt16(2 + bIPR); /* NSF: Number of standard features */

            rreqBox.AppendUInt16((bProfile1) ? 4 : 5); /* SF0 : PROFILE 1 or PROFILE 2 */
            rreqBox.AppendUInt8(0x80); /* SM0 */

            rreqBox.AppendUInt16(67); /* SF1 : GMLJP2 box */
            rreqBox.AppendUInt8(0x40); /* SM1 */

            if( bIPR )
            {
                rreqBox.AppendUInt16(35); /* SF2 : IPR metadata */
                rreqBox.AppendUInt8(0x20); /* SM2 */
            }
            rreqBox.AppendUInt16(0); /* NVF */
            WriteBox(fp, &rreqBox);
        }

        GDALJP2Box ihdrBox(fp);
        ihdrBox.SetType("ihdr");
        ihdrBox.AppendUInt32(nYSize);
        ihdrBox.AppendUInt32(nXSize);
        ihdrBox.AppendUInt16(nBands);
        GByte BPC;
        if( bSamePrecision )
            BPC = (pasBandParams[0].prec-1) | (pasBandParams[0].sgnd << 7);
        else
            BPC = 255;
        ihdrBox.AppendUInt8(BPC);
        ihdrBox.AppendUInt8(7); /* C=Compression type: fixed value */
        ihdrBox.AppendUInt8(0); /* UnkC: 0= colourspace of the image is known */
                                /*and correctly specified in the Colourspace Specification boxes within the file */
        ihdrBox.AppendUInt8(bIPR); /* IPR: 0=no intellectual property, 1=IPR box */

        GDALJP2Box bpccBox(fp);
        if( !bSamePrecision )
        {
            bpccBox.SetType("bpcc");
            for(int i=0;i<nBands;i++)
                bpccBox.AppendUInt8((pasBandParams[i].prec-1) | (pasBandParams[i].sgnd << 7));
        }

        GDALJP2Box colrBox(fp);
        colrBox.SetType("colr");
        colrBox.AppendUInt8(1); /* METHOD: 1=Enumerated Colourspace */
        colrBox.AppendUInt8(0); /* PREC: Precedence. 0=(field reserved for ISO use) */
        colrBox.AppendUInt8(0); /* APPROX: Colourspace approximation. */
        GUInt32 enumcs = 16;
        if( eColorSpace == OPJ_CLRSPC_SRGB )
            enumcs = 16;
        else if(  eColorSpace == OPJ_CLRSPC_GRAY )
            enumcs = 17;
        else if(  eColorSpace == OPJ_CLRSPC_SYCC )
            enumcs = 18;
        colrBox.AppendUInt32(enumcs); /* EnumCS: Enumerated colourspace */

        GDALJP2Box pclrBox(fp);
        GDALJP2Box cmapBox(fp);
        int nCTComponentCount = 0;
        if (poCT != NULL)
        {
            pclrBox.SetType("pclr");
            int nEntries = MIN(256, poCT->GetColorEntryCount());
            nCTComponentCount = atoi(CSLFetchNameValueDef(papszOptions, "CT_COMPONENTS", "0"));
            if( nCTComponentCount != 3 && nCTComponentCount != 4 )
            {
                nCTComponentCount = 3;
                for(int i=0;i<nEntries;i++)
                {
                    const GDALColorEntry* psEntry = poCT->GetColorEntry(i);
                    if( psEntry->c4 != 255 )
                    {
                        CPLDebug("OPENJPEG", "Color table has at least one non-opaque value. "
                                "This may cause compatibility problems with some readers. "
                                "In which case use CT_COMPONENTS=3 creation option");
                        nCTComponentCount = 4;
                        break;
                    }
                }
            }
            nRedBandIndex = 0;
            nGreenBandIndex = 1;
            nBlueBandIndex = 2;
            nAlphaBandIndex = (nCTComponentCount == 4) ? 3 : -1;

            pclrBox.AppendUInt16(nEntries);
            pclrBox.AppendUInt8(nCTComponentCount); /* NPC: Number of components */
            for(int i=0;i<nCTComponentCount;i++)
            {
                pclrBox.AppendUInt8(7); /* Bi: unsigned 8 bits */
            }
            for(int i=0;i<nEntries;i++)
            {
                const GDALColorEntry* psEntry = poCT->GetColorEntry(i);
                pclrBox.AppendUInt8((GByte)psEntry->c1);
                pclrBox.AppendUInt8((GByte)psEntry->c2);
                pclrBox.AppendUInt8((GByte)psEntry->c3);
                if( nCTComponentCount == 4 )
                    pclrBox.AppendUInt8((GByte)psEntry->c4);
            }
            
            cmapBox.SetType("cmap");
            for(int i=0;i<nCTComponentCount;i++)
            {
                cmapBox.AppendUInt16(0); /* CMPi: code stream component index */
                cmapBox.AppendUInt8(1); /* MYTPi: 1=palette mapping */
                cmapBox.AppendUInt8(i); /* PCOLi: index component from the map */
            }
        }

        GDALJP2Box cdefBox(fp);
        if( ((nBands == 3 || nBands == 4) &&
             (eColorSpace == OPJ_CLRSPC_SRGB || eColorSpace == OPJ_CLRSPC_SYCC) &&
             (nRedBandIndex != 0 || nGreenBandIndex != 1 || nBlueBandIndex != 2)) ||
            nAlphaBandIndex >= 0)
        {
            cdefBox.SetType("cdef");
            int nComponents = (nCTComponentCount == 4) ? 4 : nBands;
            cdefBox.AppendUInt16(nComponents);
            for(int i=0;i<nComponents;i++)
            {
                cdefBox.AppendUInt16(i);   /* Component number */
                if( i != nAlphaBandIndex )
                {
                    cdefBox.AppendUInt16(0);   /* Signification: This channel is the colour image data for the associated colour */
                    if( eColorSpace == OPJ_CLRSPC_GRAY && nComponents == 2)
                        cdefBox.AppendUInt16(1); /* Colour of the component: associated with a particular colour */
                    else if ((eColorSpace == OPJ_CLRSPC_SRGB ||
                            eColorSpace == OPJ_CLRSPC_SYCC) &&
                            (nComponents == 3 || nComponents == 4) )
                    {
                        if( i == nRedBandIndex )
                            cdefBox.AppendUInt16(1);
                        else if( i == nGreenBandIndex )
                            cdefBox.AppendUInt16(2);
                        else if( i == nBlueBandIndex )
                            cdefBox.AppendUInt16(3);
                        else
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                    "Could not associate band %d with a red/green/blue channel",
                                    i+1);
                            cdefBox.AppendUInt16(65535);
                        }
                    }
                    else
                        cdefBox.AppendUInt16(65535); /* Colour of the component: not associated with any particular colour */
                }
                else
                {
                    cdefBox.AppendUInt16(1);        /* Signification: Non pre-multiplied alpha */
                    cdefBox.AppendUInt16(0);        /* Colour of the component: This channel is associated as the image as a whole */
                }
            }
        }

        // Add res box if needed
        double dfXRes = 0, dfYRes = 0;
        int nResUnit = 0;
        GDALJP2Box* poRes = NULL;
        if( poSrcDS->GetMetadataItem("TIFFTAG_XRESOLUTION") != NULL
            && poSrcDS->GetMetadataItem("TIFFTAG_YRESOLUTION") != NULL
            && poSrcDS->GetMetadataItem("TIFFTAG_RESOLUTIONUNIT") != NULL )
        {
            dfXRes =
                CPLAtof(poSrcDS->GetMetadataItem("TIFFTAG_XRESOLUTION"));
            dfYRes =
                CPLAtof(poSrcDS->GetMetadataItem("TIFFTAG_YRESOLUTION"));
            nResUnit = atoi(poSrcDS->GetMetadataItem("TIFFTAG_RESOLUTIONUNIT"));
#define PIXELS_PER_INCH 2
#define PIXELS_PER_CM   3

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
                /* Format a resd box and embed it inside a res box */
                GDALJP2Box oResd;
                oResd.SetType("resd");

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

                oResd.AppendUInt16((GUInt16)dfYRes);
                oResd.AppendUInt16((GUInt16)nYDenom);
                oResd.AppendUInt16((GUInt16)dfXRes);
                oResd.AppendUInt16((GUInt16)nXDenom);
                oResd.AppendUInt8(2); /* vertical exponent */
                oResd.AppendUInt8(2); /* horizontal exponent */

                GDALJP2Box* poResd = &oResd;
                poRes = GDALJP2Box::CreateAsocBox( 1, &poResd );
                poRes->SetType("res ");
            }
        }

        /* Build and write jp2h super box now */
        GDALJP2Box* apoBoxes[7];
        int nBoxes = 1;
        apoBoxes[0] = &ihdrBox;
        if( bpccBox.GetDataLength() )
            apoBoxes[nBoxes++] = &bpccBox;
        apoBoxes[nBoxes++] = &colrBox;
        if( pclrBox.GetDataLength() )
            apoBoxes[nBoxes++] = &pclrBox;
        if( cmapBox.GetDataLength() )
            apoBoxes[nBoxes++] = &cmapBox;
        if( cdefBox.GetDataLength() )
            apoBoxes[nBoxes++] = &cdefBox;
        if( poRes )
            apoBoxes[nBoxes++] = poRes;
        GDALJP2Box* psJP2HBox = GDALJP2Box::CreateSuperBox( "jp2h",
                                                            nBoxes,
                                                            apoBoxes );
        WriteBox(fp, psJP2HBox);
        delete psJP2HBox;
        delete poRes;

        if( !bGeoBoxesAfter )
        {
            if( bGeoJP2Option && bGeoreferencingCompatOfGeoJP2 )
            {
                GDALJP2Box* poBox = oJP2MD.CreateJP2GeoTIFF();
                WriteBox(fp, poBox);
                delete poBox;
            }

            if( CSLFetchBoolean(papszOptions, "WRITE_METADATA", FALSE) &&
                !CSLFetchBoolean(papszOptions, "MAIN_MD_DOMAIN_ONLY", FALSE) )
            {
                WriteXMPBox(fp, poSrcDS, papszOptions);
            }

            if( CSLFetchBoolean(papszOptions, "WRITE_METADATA", FALSE) )
            {
                if( !CSLFetchBoolean(papszOptions, "MAIN_MD_DOMAIN_ONLY", FALSE) )
                    WriteXMLBoxes(fp, poSrcDS, papszOptions);
                WriteGDALMetadataBox(fp, poSrcDS, papszOptions);
            }

            if( bGMLJP2Option && bGeoreferencingCompatOfGMLJP2 )
            {
                GDALJP2Box* poBox = oJP2MD.CreateGMLJP2(nXSize,nYSize);
                WriteBox(fp, poBox);
                delete poBox;
            }
        }
    }
    CPLFree(pasBandParams);
    pasBandParams = NULL;

/* -------------------------------------------------------------------- */
/*      Try lossless reuse of an existing JPEG2000 codestream           */
/* -------------------------------------------------------------------- */
    vsi_l_offset nCodeStreamLength = 0;
    vsi_l_offset nCodeStreamStart = 0;
    VSILFILE* fpSrc = NULL;
    if( CSLFetchBoolean(papszOptions, "USE_SRC_CODESTREAM", FALSE) )
    {
        CPLString osSrcFilename( poSrcDS->GetDescription() );
        if( poSrcDS->GetDriver() != NULL &&
            poSrcDS->GetDriver() == GDALGetDriverByName("VRT") )
        {
            VRTDataset* poVRTDS = (VRTDataset* )poSrcDS;
            GDALDataset* poSimpleSourceDS = poVRTDS->GetSingleSimpleSource();
            if( poSimpleSourceDS )
                osSrcFilename = poSimpleSourceDS->GetDescription();
        }

        fpSrc = VSIFOpenL( osSrcFilename, "rb" );
        if( fpSrc )
        {
            nCodeStreamStart = JP2OpenJPEGFindCodeStream(fpSrc,
                                                         &nCodeStreamLength);
        }
        if( nCodeStreamLength == 0 )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "USE_SRC_CODESTREAM=YES specified, but no codestream found");
        }
    }

    if( eCodecFormat == OPJ_CODEC_JP2  )
    {
        // Start codestream box
        nStartJP2C = VSIFTellL(fp);
        if( nCodeStreamLength )
            bUseXLBoxes = ((vsi_l_offset)(GUInt32)nCodeStreamLength != nCodeStreamLength);
        else
            bUseXLBoxes = CSLFetchBoolean(papszOptions, "JP2C_XLBOX", FALSE) || /* For debugging */
                (GIntBig)nXSize * nYSize * nBands * nDataTypeSize / adfRates[adfRates.size()-1] > 4e9;
        GUInt32 nLBox = (bUseXLBoxes) ? 1 : 0;
        CPL_MSBPTR32(&nLBox);
        VSIFWriteL(&nLBox, 1, 4, fp);
        VSIFWriteL("jp2c", 1, 4, fp);
        if( bUseXLBoxes )
        {
            GUIntBig nXLBox = 0;
            VSIFWriteL(&nXLBox, 1, 8, fp);
        }
    }

/* -------------------------------------------------------------------- */
/*      Do lossless reuse of an existing JPEG2000 codestream            */
/* -------------------------------------------------------------------- */
    if( fpSrc )
    {
        const char* apszIgnoredOptions[] = {
            "BLOCKXSIZE", "BLOCKYSIZE", "QUALITY", "REVERSIBLE",
            "RESOLUTIONS", "PROGRESSION", "SOP", "EPH",
            "YCBCR420", "YCC", "NBITS", "1BIT_ALPHA", "PRECINCTS",
            "TILEPARTS", "CODEBLOCK_WIDTH", "CODEBLOCK_HEIGHT", NULL };
        for( int i = 0; apszIgnoredOptions[i]; i ++)
        {
            if( CSLFetchNameValue(papszOptions, apszIgnoredOptions[i]) )
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                            "Option %s ignored when USE_SRC_CODESTREAM=YES",
                            apszIgnoredOptions[i]);
            }
        }
        GByte abyBuffer[4096];
        VSIFSeekL( fpSrc, nCodeStreamStart, SEEK_SET );
        vsi_l_offset nRead = 0;
        while( nRead < nCodeStreamLength )
        {
            int nToRead = ( nCodeStreamLength-nRead > 4096 ) ? 4049 :
                                        (int)(nCodeStreamLength-nRead);
            if( (int)VSIFReadL(abyBuffer, 1, nToRead, fpSrc) != nToRead )
            {
                VSIFCloseL(fp);
                VSIFCloseL(fpSrc);
                opj_image_destroy(psImage);
                opj_destroy_codec(pCodec);
                return NULL;
            }
            if( nRead == 0 && (pszProfile || bInspireTG) &&
                abyBuffer[2] == 0xFF && abyBuffer[3] == 0x51 )
            {
                if( EQUAL(pszProfile, "UNRESTRICTED") )
                {
                    abyBuffer[6] = 0;
                    abyBuffer[7] = 0;
                }
                else if( EQUAL(pszProfile, "PROFILE_1") || bInspireTG )
                {
                    // TODO: ultimately we should check that we can really set Profile 1
                    abyBuffer[6] = 0;
                    abyBuffer[7] = 2;
                }
            }
            if( (int)VSIFWriteL(abyBuffer, 1, nToRead, fp) != nToRead ||
                !pfnProgress( (nRead + nToRead) * 1.0 / nCodeStreamLength,
                                NULL, pProgressData ) )
            {
                VSIFCloseL(fp);
                VSIFCloseL(fpSrc);
                opj_image_destroy(psImage);
                opj_destroy_codec(pCodec);
                return NULL;
            }
            nRead += nToRead;
        }

        VSIFCloseL(fpSrc);
    }
    else
    {
        opj_stream_t * pStream;
        JP2OpenJPEGFile sJP2OpenJPEGFile;
        sJP2OpenJPEGFile.fp = fp;
        sJP2OpenJPEGFile.nBaseOffset = VSIFTellL(fp);
        pStream = opj_stream_create(1024*1024, FALSE);
        opj_stream_set_write_function(pStream, JP2OpenJPEGDataset_Write);
        opj_stream_set_seek_function(pStream, JP2OpenJPEGDataset_Seek);
        opj_stream_set_skip_function(pStream, JP2OpenJPEGDataset_Skip);
#if defined(OPENJPEG_VERSION) && OPENJPEG_VERSION >= 20100
        opj_stream_set_user_data(pStream, &sJP2OpenJPEGFile, NULL);
#else
        opj_stream_set_user_data(pStream, &sJP2OpenJPEGFile);
#endif

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

        GUIntBig nTileSize = (GUIntBig)nBlockXSize * nBlockYSize * nBands * nDataTypeSize;
        GByte* pTempBuffer;
        if( nTileSize != (GUIntBig)(GUInt32)nTileSize )
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Tile size exceeds 4GB");
            pTempBuffer = NULL;
        }
        else
        {
            pTempBuffer = (GByte*)VSIMalloc((size_t)nTileSize);
        }
        if (pTempBuffer == NULL)
        {
            opj_stream_destroy(pStream);
            opj_image_destroy(psImage);
            opj_destroy_codec(pCodec);
            VSIFCloseL(fp);
            return NULL;
        }

        GByte* pYUV420Buffer = NULL;
        if (bYCBCR420)
        {
            pYUV420Buffer =(GByte*)VSIMalloc(3 * nBlockXSize * nBlockYSize / 2 +
                                            ((nBands == 4) ? nBlockXSize * nBlockYSize : 0));
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
                                        0,0,0,NULL);
                if( b1BitAlpha )
                {
                    for(int i=0;i<nWidthToRead*nHeightToRead;i++)
                    {
                        if( pTempBuffer[nAlphaBandIndex*nWidthToRead*nHeightToRead+i] )
                            pTempBuffer[nAlphaBandIndex*nWidthToRead*nHeightToRead+i] = 1;
                        else
                            pTempBuffer[nAlphaBandIndex*nWidthToRead*nHeightToRead+i] = 0;
                    }
                }
                if (eErr == CE_None)
                {
                    if (bYCBCR420)
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
                                if( nBands == 4 )
                                {
                                    pYUV420Buffer[3 * nHeightToRead * nWidthToRead / 2 + j*nWidthToRead+i ] =
                                        (GByte) pTempBuffer[3*nHeightToRead*nWidthToRead + j*nWidthToRead+i];
                                }
                            }
                        }

                        int nBytesToWrite = 3 * nWidthToRead * nHeightToRead / 2;
                        if (nBands == 4)
                            nBytesToWrite += nBlockXSize * nBlockYSize;

                        if (!opj_write_tile(pCodec,
                                            iTile,
                                            pYUV420Buffer,
                                            nBytesToWrite,
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
    }

    opj_image_destroy(psImage);
    opj_destroy_codec(pCodec);

/* -------------------------------------------------------------------- */
/*      Patch JP2C box length and add trailing JP2 boxes                */
/* -------------------------------------------------------------------- */
    if( eCodecFormat == OPJ_CODEC_JP2 &&
        !CSLFetchBoolean(papszOptions, "JP2C_LENGTH_ZERO", FALSE) /* debug option */ )
    {
        vsi_l_offset nEndJP2C = VSIFTellL(fp);
        GUIntBig nBoxSize = nEndJP2C -nStartJP2C;
        if( bUseXLBoxes )
        {
            VSIFSeekL(fp, nStartJP2C + 8, SEEK_SET);
            CPL_MSBPTR64(&nBoxSize);
            VSIFWriteL(&nBoxSize, 8, 1, fp);
        }
        else
        {
            GUInt32 nBoxSize32 = (GUInt32)nBoxSize;
            if( (vsi_l_offset)nBoxSize32 != nBoxSize )
            {
                /*  Shouldn't happen hopefully */
                if( (bGeoreferencingCompatOfGeoJP2 || bGeoreferencingCompatOfGMLJP2) && bGeoBoxesAfter )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Cannot write GMLJP2/GeoJP2 boxes as codestream is unexpectedly > 4GB");
                    bGeoreferencingCompatOfGeoJP2 = FALSE;
                    bGeoreferencingCompatOfGMLJP2 = FALSE;
                }
            }
            else
            {
                VSIFSeekL(fp, nStartJP2C, SEEK_SET);
                CPL_MSBPTR32(&nBoxSize32);
                VSIFWriteL(&nBoxSize32, 4, 1, fp);
            }
        }
        VSIFSeekL(fp, 0, SEEK_END);

        if( CSLFetchBoolean(papszOptions, "WRITE_METADATA", FALSE) )
        {
            WriteIPRBox(fp, poSrcDS, papszOptions);
        }

        if( bGeoBoxesAfter )
        {
            if( bGMLJP2Option && bGeoreferencingCompatOfGMLJP2 )
            {
                GDALJP2Box* poBox = oJP2MD.CreateGMLJP2(nXSize,nYSize);
                WriteBox(fp, poBox);
                delete poBox;
            }

            if( CSLFetchBoolean(papszOptions, "WRITE_METADATA", FALSE) )
            {
                if( !CSLFetchBoolean(papszOptions, "MAIN_MD_DOMAIN_ONLY", FALSE) )
                    WriteXMLBoxes(fp, poSrcDS, papszOptions);
                WriteGDALMetadataBox(fp, poSrcDS, papszOptions);
            }

            if( bGeoJP2Option && bGeoreferencingCompatOfGeoJP2 )
            {
                GDALJP2Box* poBox = oJP2MD.CreateJP2GeoTIFF();
                WriteBox(fp, poBox);
                delete poBox;
            }

            if( CSLFetchBoolean(papszOptions, "WRITE_METADATA", FALSE) &&
                !CSLFetchBoolean(papszOptions, "MAIN_MD_DOMAIN_ONLY", FALSE) )
            {
                WriteXMPBox(fp, poSrcDS, papszOptions);
            }
        }
    }

    VSIFCloseL(fp);

/* -------------------------------------------------------------------- */
/*      Re-open dataset, and copy any auxilary pam information.         */
/* -------------------------------------------------------------------- */

    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);
    JP2OpenJPEGDataset *poDS = (JP2OpenJPEGDataset*) JP2OpenJPEGDataset::Open(&oOpenInfo);

    if( poDS )
    {
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT & (~GCIF_METADATA) );

        /* Only write relevant metadata to PAM, and if needed */
        if( !CSLFetchBoolean(papszOptions, "WRITE_METADATA", FALSE) )
        {
            char** papszSrcMD = CSLDuplicate(poSrcDS->GetMetadata());
            papszSrcMD = CSLSetNameValue(papszSrcMD, GDALMD_AREA_OR_POINT, NULL);
            papszSrcMD = CSLSetNameValue(papszSrcMD, "Corder", NULL);
            for(char** papszSrcMDIter = papszSrcMD;
                    papszSrcMDIter && *papszSrcMDIter; )
            {
                /* Remove entries like KEY= (without value) */
                if( (*papszSrcMDIter)[0] &&
                    (*papszSrcMDIter)[strlen((*papszSrcMDIter))-1] == '=' )
                {
                    CPLFree(*papszSrcMDIter);
                    memmove(papszSrcMDIter, papszSrcMDIter + 1,
                            sizeof(char*) * (CSLCount(papszSrcMDIter + 1) + 1));
                }
                else
                    ++papszSrcMDIter;
            }
            char** papszMD = CSLDuplicate(poDS->GetMetadata());
            papszMD = CSLSetNameValue(papszMD, GDALMD_AREA_OR_POINT, NULL);
            if( papszSrcMD && papszSrcMD[0] != NULL &&
                CSLCount(papszSrcMD) != CSLCount(papszMD) )
            {
                poDS->SetMetadata(papszSrcMD);
            }
            CSLDestroy(papszSrcMD);
            CSLDestroy(papszMD);
        }
    }

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
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "JPEG-2000 driver based on OpenJPEG library" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_jp2openjpeg.html" );
        poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/jp2" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "jp2" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte Int16 UInt16 Int32 UInt32" );

        poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST, 
"<OpenOptionList>"
"   <Option name='1BIT_ALPHA_PROMOTION' type='boolean' description='Whether a 1-bit alpha channel should be promoted to 8-bit' default='YES'/>"
"</OpenOptionList>" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='CODEC' type='string-select' default='according to file extension. If unknown, default to J2K'>"
"       <Value>JP2</Value>"
"       <Value>J2K</Value>"
"   </Option>"
"   <Option name='GeoJP2' type='boolean' description='Whether to emit a GeoJP2 box' default='YES'/>"
"   <Option name='GMLJP2' type='boolean' description='Whether to emit a GMLJP2 box' default='YES'/>"
"   <Option name='QUALITY' type='string' description='Single quality value or comma separated list of increasing quality values for several layers, each in the 0-100 range' default='25'/>"
"   <Option name='REVERSIBLE' type='boolean' description='True if the compression is reversible' default='false'/>"
"   <Option name='RESOLUTIONS' type='int' description='Number of resolutions.' min='1' max='30'/>"
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
"   <Option name='YCC' type='boolean' description='if RGB must be transformed to YCC color space (lossless MCT transform)' default='YES'/>"
"   <Option name='NBITS' type='int' description='Bits (precision) for sub-byte files (1-7), sub-uint16 (9-15), sub-uint32 (17-31)'/>"
"   <Option name='1BIT_ALPHA' type='boolean' description='Whether to encode the alpha channel as a 1-bit channel' default='NO'/>"
"   <Option name='ALPHA' type='boolean' description='Whether to force encoding last channel as alpha channel' default='NO'/>"
"   <Option name='PROFILE' type='string-select' description='Which codestream profile to use' default='AUTO'>"
"       <Value>AUTO</Value>"
"       <Value>UNRESTRICTED</Value>"
"       <Value>PROFILE_1</Value>"
"   </Option>"
"   <Option name='INSPIRE_TG' type='boolean' description='Whether to use features that comply with Inspire Orthoimagery Technical Guidelines' default='NO'/>"
"   <Option name='JPX' type='boolean' description='Whether to advertize JPX features when a GMLJP2 box is written' default='YES'/>"
"   <Option name='GEOBOXES_AFTER_JP2C' type='boolean' description='Whether to place GeoJP2/GMLJP2 boxes after the code-stream' default='NO'/>"
"   <Option name='PRECINCTS' type='string' description='Precincts size as a string of the form {w,h},{w,h},... with power-of-two values'/>"
"   <Option name='TILEPARTS' type='string-select' description='Whether to generate tile-parts and according to which criterion' default='DISABLED'>"
"       <Value>DISABLED</Value>"
"       <Value>RESOLUTIONS</Value>"
"       <Value>LAYERS</Value>"
"       <Value>COMPONENTS</Value>"
"   </Option>"
"   <Option name='CODEBLOCK_WIDTH' type='int' description='Codeblock width' default='64' min='4' max='1024'/>"
"   <Option name='CODEBLOCK_HEIGHT' type='int' description='Codeblock height' default='64' min='4' max='1024'/>"
"   <Option name='CT_COMPONENTS' type='int' min='3' max='4' description='If there is one color table, number of color table components to write. Autodetected if not specified.'/>"
"   <Option name='WRITE_METADATA' type='boolean' description='Whether metadata should be written, in a dedicated JP2 XML box' default='NO'/>"
"   <Option name='MAIN_MD_DOMAIN_ONLY' type='boolean' description='(Only if WRITE_METADATA=YES) Whether only metadata from the main domain should be written' default='NO'/>"
"   <Option name='USE_SRC_CODESTREAM' type='boolean' description='When source dataset is JPEG2000, whether to reuse the codestream of the source dataset unmodified' default='NO'/>"
"</CreationOptionList>"  );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnIdentify = JP2OpenJPEGDataset::Identify;
        poDriver->pfnOpen = JP2OpenJPEGDataset::Open;
        poDriver->pfnCreateCopy = JP2OpenJPEGDataset::CreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
