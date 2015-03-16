/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Epsilon driver
 * Purpose:  Implement GDAL Epsilon support using Epsilon library
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 **********************************************************************
 * Copyright (c) 2009-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "epsilon.h"
#include "gdal_pam.h"

CPL_CVSID("$Id$");

#define RASTERLITE_WAVELET_HEADER "StartWaveletsImage$$"
#define RASTERLITE_WAVELET_FOOTER "$$EndWaveletsImage"

#define BLOCK_DATA_MAX_SIZE MAX(EPS_MAX_GRAYSCALE_BUF, EPS_MAX_TRUECOLOR_BUF)

class EpsilonRasterBand;

typedef struct
{
    int x;
    int y;
    int w;
    int h;
    vsi_l_offset offset;
} BlockDesc;

#ifdef I_WANT_COMPATIBILITY_WITH_EPSILON_0_8_1
#define GET_FIELD(hdr, field) \
    (hdr.block_type == EPS_GRAYSCALE_BLOCK) ? hdr.gs.field : hdr.tc.field
#else
#define GET_FIELD(hdr, field) \
    (hdr.block_type == EPS_GRAYSCALE_BLOCK) ? hdr.hdr_data.gs.field : hdr.hdr_data.tc.field
#endif

/************************************************************************/
/* ==================================================================== */
/*                              EpsilonDataset                          */
/* ==================================================================== */
/************************************************************************/

class EpsilonDataset : public GDALPamDataset
{
    friend class EpsilonRasterBand;

    VSILFILE*    fp;
    vsi_l_offset nFileOff;
    
    GByte*   pabyFileBuf;
    int      nFileBufMaxSize;
    int      nFileBufCurSize;
    int      nFileBufOffset;
    int      bEOF;
    int      bError;
    
    GByte*   pabyBlockData;
    int      nBlockDataSize;
    vsi_l_offset nStartBlockFileOff;
    
    int      bRegularTiling;
    
    int        nBlocks;
    BlockDesc* pasBlocks;
    
    int      nBufferedBlock;
    GByte*   pabyRGBData;
    
    void     Seek(vsi_l_offset nPos);
    int      GetNextByte();
    int      GetNextBlockData();
    int      ScanBlocks(int *pnBands);

  public:
                 EpsilonDataset();
    virtual     ~EpsilonDataset();
    
    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            EpsilonRasterBand                         */
/* ==================================================================== */
/************************************************************************/

class EpsilonRasterBand : public GDALPamRasterBand
{
  public:
                            EpsilonRasterBand(EpsilonDataset* poDS, int nBand);
                 
    virtual CPLErr          IReadBlock( int, int, void * );
    virtual GDALColorInterp GetColorInterpretation();
};

/************************************************************************/
/*                         EpsilonDataset()                             */
/************************************************************************/

EpsilonDataset::EpsilonDataset()
{
    fp = NULL;
    nFileOff = 0;
    
    pabyFileBuf = NULL;
    nFileBufMaxSize = 0;
    nFileBufCurSize = 0;
    nFileBufOffset = 0;
    bEOF = FALSE;
    bError = FALSE;
    
    pabyBlockData = NULL;
    nBlockDataSize = 0;
    nStartBlockFileOff = 0;
    
    bRegularTiling = FALSE;
    
    nBlocks = 0;
    pasBlocks = NULL;
    
    nBufferedBlock = -1;
    pabyRGBData = NULL;
}

/************************************************************************/
/*                         ~EpsilonDataset()                            */
/************************************************************************/

EpsilonDataset::~EpsilonDataset()
{
    if (fp)
        VSIFCloseL(fp);
    VSIFree(pabyFileBuf);
    VSIFree(pasBlocks);
    CPLFree(pabyRGBData);
}

/************************************************************************/
/*                       EpsilonRasterBand()                            */
/************************************************************************/

EpsilonRasterBand::EpsilonRasterBand(EpsilonDataset* poDS, int nBand)
{
    this->poDS = poDS;
    this->nBand = nBand;
    this->eDataType = GDT_Byte;
    this->nBlockXSize = poDS->pasBlocks[0].w;
    this->nBlockYSize = poDS->pasBlocks[0].h;
}

/************************************************************************/
/*                   GetColorInterpretation()                           */
/************************************************************************/

GDALColorInterp EpsilonRasterBand::GetColorInterpretation()
{
    EpsilonDataset* poGDS = (EpsilonDataset*) poDS;
    if (poGDS->nBands == 1)
    {
        return GCI_GrayIndex;
    }
    else
    {
        if (nBand == 1)
            return GCI_RedBand;
        else if (nBand == 2)
            return GCI_GreenBand;
        else
            return GCI_BlueBand;
    }
}

/************************************************************************/
/*                           IReadBlock()                               */
/************************************************************************/

CPLErr EpsilonRasterBand::IReadBlock( int nBlockXOff,
                                      int nBlockYOff, void * pImage)
{
    EpsilonDataset* poGDS = (EpsilonDataset*) poDS;
    
    //CPLDebug("EPSILON", "IReadBlock(nBand=%d,nBlockXOff=%d,nBlockYOff=%d)",
    //         nBand, nBlockXOff, nBlockYOff);

    int nBlocksPerRow = (poGDS->nRasterXSize + nBlockXSize - 1) / nBlockXSize;
    int nBlock = nBlockXOff + nBlockYOff * nBlocksPerRow;
    
    BlockDesc* psDesc = &poGDS->pasBlocks[nBlock];
#ifdef DEBUG
    int nBlocksPerColumn = (poGDS->nRasterYSize + nBlockYSize - 1) / nBlockYSize;
    CPLAssert(psDesc->x == nBlockXOff * nBlockXSize);
    CPLAssert(psDesc->y == nBlockYOff * nBlockYSize);
    CPLAssert(psDesc->w == (nBlockXOff < nBlocksPerRow - 1) ?
                                nBlockXSize : poGDS->nRasterXSize - psDesc->x);
    CPLAssert(psDesc->h == (nBlockYOff < nBlocksPerColumn - 1) ?
                                nBlockYSize : poGDS->nRasterYSize - psDesc->y);
#endif

    poGDS->Seek(psDesc->offset);
        
    if (!poGDS->GetNextBlockData())
    {
        memset(pImage, 0, nBlockXSize * nBlockYSize);
        return CE_Failure;
    }
    
    eps_block_header hdr;
    if (eps_read_block_header (poGDS->pabyBlockData,
                               poGDS->nBlockDataSize, &hdr) != EPS_OK)
    {
        CPLError(CE_Warning, CPLE_AppDefined, "cannot read block header");
        memset(pImage, 0, nBlockXSize * nBlockYSize);
        return CE_Failure;
    }

    if (hdr.chk_flag == EPS_BAD_CRC ||
        hdr.crc_flag == EPS_BAD_CRC)
    {
        CPLError(CE_Warning, CPLE_AppDefined, "bad CRC");
        memset(pImage, 0, nBlockXSize * nBlockYSize);
        return CE_Failure;
    }
    
    int w = GET_FIELD(hdr, w);
    int h = GET_FIELD(hdr, h);
    int i;

    if (poGDS->nBands == 1)
    {
        unsigned char ** pTempData =
            (unsigned char **) CPLMalloc(h * sizeof(unsigned char*));        
        for(i=0;i<h;i++)
            pTempData[i] = ((GByte*)pImage) + i * nBlockXSize;

        if (w != nBlockXSize || h != nBlockYSize)
            memset(pImage, 0, nBlockXSize * nBlockYSize);

        if (eps_decode_grayscale_block (pTempData,
                                        poGDS->pabyBlockData, &hdr) != EPS_OK)
        {
            CPLFree(pTempData);
            memset(pImage, 0, nBlockXSize * nBlockYSize);
            return CE_Failure;
        }
        CPLFree(pTempData);
    }
    else
    {
        if (poGDS->pabyRGBData == NULL)
        {
            poGDS->pabyRGBData =
                            (GByte*) VSIMalloc3(nBlockXSize, nBlockYSize, 3);
            if (poGDS->pabyRGBData == NULL)
            {
                memset(pImage, 0, nBlockXSize * nBlockYSize);
                return CE_Failure;
            }
        }
            
        if (poGDS->nBufferedBlock == nBlock)
        {
            memcpy(pImage,
                   poGDS->pabyRGBData + (nBand - 1) * nBlockXSize * nBlockYSize,
                   nBlockXSize * nBlockYSize);
            return CE_None;
        }
    
        unsigned char ** pTempData[3];
        int iBand;
        for(iBand=0;iBand<3;iBand++)
        {
            pTempData[iBand] =
                (unsigned char **) CPLMalloc(h * sizeof(unsigned char*));
            for(i=0;i<h;i++)
                pTempData[iBand][i] = poGDS->pabyRGBData +
                    iBand * nBlockXSize * nBlockYSize + i * nBlockXSize;
        }
    
        if (w != nBlockXSize || h != nBlockYSize)
            memset(poGDS->pabyRGBData, 0, 3 * nBlockXSize * nBlockYSize);
            
        if (eps_decode_truecolor_block (pTempData[0], pTempData[1], pTempData[2],
                                        poGDS->pabyBlockData, &hdr) != EPS_OK)
        {
            for(iBand=0;iBand<poGDS->nBands;iBand++)
                CPLFree(pTempData[iBand]);
            memset(pImage, 0, nBlockXSize * nBlockYSize);
            return CE_Failure;
        }
        
        for(iBand=0;iBand<poGDS->nBands;iBand++)
            CPLFree(pTempData[iBand]);
            
        poGDS->nBufferedBlock = nBlock;
        memcpy(pImage,
               poGDS->pabyRGBData + (nBand - 1) * nBlockXSize * nBlockYSize,
               nBlockXSize * nBlockYSize);
                   
        if (nBand == 1)
        {
            int iOtherBand;
            for(iOtherBand=2;iOtherBand<=poGDS->nBands;iOtherBand++)
            {
                GDALRasterBlock *poBlock;

                poBlock = poGDS->GetRasterBand(iOtherBand)->
                    GetLockedBlockRef(nBlockXOff,nBlockYOff, TRUE);
                if (poBlock == NULL)
                    break;
                    
                GByte* pabySrcBlock = (GByte *) poBlock->GetDataRef();
                if( pabySrcBlock == NULL )
                {
                    poBlock->DropLock();
                    break;
                }

                memcpy(pabySrcBlock,
                       poGDS->pabyRGBData + (iOtherBand - 1) * nBlockXSize * nBlockYSize,
                       nBlockXSize * nBlockYSize);

                poBlock->DropLock();
            }
        }
    }
    
    return CE_None;
}

/************************************************************************/
/*                              Seek()                                  */
/************************************************************************/

void EpsilonDataset::Seek(vsi_l_offset nPos)
{
    bEOF = FALSE;
    VSIFSeekL(fp, nPos, SEEK_SET);
    nFileBufOffset = 0;
    nFileBufCurSize = 0;
    nFileOff = nPos;
}

/************************************************************************/
/*                          GetNextByte()                               */
/************************************************************************/

#define BUFFER_CHUNK    16384

int EpsilonDataset::GetNextByte()
{
    if (nFileBufOffset < nFileBufCurSize)
    {
        nFileOff ++;
        return pabyFileBuf[nFileBufOffset ++];
    }
        
    if (bError || bEOF)
        return -1;

    if (nFileBufCurSize + BUFFER_CHUNK > nFileBufMaxSize)
    {
        GByte* pabyFileBufNew =
            (GByte*)VSIRealloc(pabyFileBuf, nFileBufCurSize + BUFFER_CHUNK);
        if (pabyFileBufNew == NULL)
        {
            bError = TRUE;
            return -1;
        }
        pabyFileBuf = pabyFileBufNew;
        nFileBufMaxSize = nFileBufCurSize + BUFFER_CHUNK;
    }
    int nBytesRead =
        (int)VSIFReadL(pabyFileBuf + nFileBufCurSize, 1, BUFFER_CHUNK, fp);
    nFileBufCurSize += nBytesRead;
    if (nBytesRead < BUFFER_CHUNK)
        bEOF = TRUE;
    if (nBytesRead == 0)
        return -1;
    
    nFileOff ++;
    return pabyFileBuf[nFileBufOffset ++];
}

/************************************************************************/
/*                     GetNextBlockData()                               */
/************************************************************************/

#define MAX_SIZE_BEFORE_BLOCK_MARKER        100

int EpsilonDataset::GetNextBlockData()
{
    int nStartBlockBufOffset = 0;
    pabyBlockData = NULL;
    nBlockDataSize = 0;
    
    while (nFileBufOffset < MAX_SIZE_BEFORE_BLOCK_MARKER)
    {
        int chNextByte = GetNextByte();
        if (chNextByte < 0)
            return FALSE;
        
        if (chNextByte != EPS_MARKER)
        {
            nStartBlockFileOff = nFileOff - 1;
            nStartBlockBufOffset = nFileBufOffset - 1;
            nBlockDataSize = 1;
            break;
        }
    }
    if (nFileBufOffset == MAX_SIZE_BEFORE_BLOCK_MARKER)
        return FALSE;
        
    while (nFileBufOffset < BLOCK_DATA_MAX_SIZE)
    {
        int chNextByte = GetNextByte();
        if (chNextByte < 0)
            break;

        if (chNextByte == EPS_MARKER)
        {
            pabyBlockData = pabyFileBuf + nStartBlockBufOffset;
            return TRUE;
        }
            
        nBlockDataSize ++;
    }
    
    pabyBlockData = pabyFileBuf + nStartBlockBufOffset;
    return TRUE;
}

/************************************************************************/
/*                           ScanBlocks()                               */
/************************************************************************/

int EpsilonDataset::ScanBlocks(int* pnBands)
{
    int bRet = FALSE;

    int nExpectedX = 0;
    int nExpectedY = 0;

    int nTileW = -1;
    int nTileH = -1;
    
    *pnBands = 0;

    bRegularTiling = TRUE;
    
    eps_block_header hdr;
    while(TRUE)
    {
        Seek(nStartBlockFileOff + nBlockDataSize);
        
        if (!GetNextBlockData())
        {
            break;
        }
        
        /* Ignore rasterlite wavelet header */
        int nRasterliteWaveletHeaderLen = strlen(RASTERLITE_WAVELET_HEADER);
        if (nBlockDataSize >= nRasterliteWaveletHeaderLen &&
            memcmp(pabyBlockData, RASTERLITE_WAVELET_HEADER,
                   nRasterliteWaveletHeaderLen) == 0)
        {
            continue;
        }
        
        /* Stop at rasterlite wavelet footer */
        int nRasterlineWaveletFooterLen = strlen(RASTERLITE_WAVELET_FOOTER);
        if (nBlockDataSize >= nRasterlineWaveletFooterLen &&
            memcmp(pabyBlockData, RASTERLITE_WAVELET_FOOTER,
                   nRasterlineWaveletFooterLen) == 0)
        {
            break;
        }
        
        if (eps_read_block_header (pabyBlockData,
                                   nBlockDataSize, &hdr) != EPS_OK)
        {
            CPLError(CE_Warning, CPLE_AppDefined, "cannot read block header");
            continue;
        }

        if (hdr.chk_flag == EPS_BAD_CRC ||
            hdr.crc_flag == EPS_BAD_CRC)
        {
            CPLError(CE_Warning, CPLE_AppDefined, "bad CRC");
            continue;
        }
        
        int W = GET_FIELD(hdr, W);
        int H = GET_FIELD(hdr, H);
        int x = GET_FIELD(hdr, x);
        int y = GET_FIELD(hdr, y);
        int w = GET_FIELD(hdr, w);
        int h = GET_FIELD(hdr, h);

        //CPLDebug("EPSILON", "W=%d,H=%d,x=%d,y=%d,w=%d,h=%d,offset=" CPL_FRMT_GUIB,
        //                    W, H, x, y, w, h, nStartBlockFileOff);
        
        int nNewBands = (hdr.block_type == EPS_GRAYSCALE_BLOCK) ? 1 : 3;
        if (nRasterXSize == 0)
        {
            if (W <= 0 || H <= 0)
            {
                break;
            }
            
            bRet = TRUE;
            nRasterXSize = W;
            nRasterYSize = H;
            *pnBands = nNewBands;
        }
        
        if (nRasterXSize != W || nRasterYSize != H || *pnBands != nNewBands ||
            x < 0 || y < 0 || x + w > W || y + h > H)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Bad block characteristics");
            bRet = FALSE;
            break;
        }
        
        nBlocks++;
        pasBlocks = (BlockDesc*)VSIRealloc(pasBlocks, sizeof(BlockDesc) * nBlocks);
        pasBlocks[nBlocks-1].x = x;
        pasBlocks[nBlocks-1].y = y;
        pasBlocks[nBlocks-1].w = w;
        pasBlocks[nBlocks-1].h = h;
        pasBlocks[nBlocks-1].offset = nStartBlockFileOff;
        
        if (bRegularTiling)
        {
            if (nTileW < 0)
            {
                nTileW = w;
                nTileH = h;
            }
            
            if (w > nTileW || h > nTileH)
                bRegularTiling = FALSE;
            
            if (x != nExpectedX)
                bRegularTiling = FALSE;

            if (y != nExpectedY || nTileH != h)
            {
                if (y + h != H)
                    bRegularTiling = FALSE;
            }
            
            if (nTileW != w)
            {
                if (x + w != W)
                    bRegularTiling = FALSE;
                else
                {
                    nExpectedX = 0;
                    nExpectedY += nTileW;
                }
            }
            else
                nExpectedX += nTileW;
            
            //if (!bRegularTiling)
            //    CPLDebug("EPSILON", "not regular tiling!");
        }
    } 

    return bRet;
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int EpsilonDataset::Identify(GDALOpenInfo* poOpenInfo)
{
    int nRasterliteWaveletHeaderLen = strlen(RASTERLITE_WAVELET_HEADER);
    if (poOpenInfo->nHeaderBytes > nRasterliteWaveletHeaderLen + 1 &&
          EQUALN((const char*)poOpenInfo->pabyHeader,
                 RASTERLITE_WAVELET_HEADER, nRasterliteWaveletHeaderLen))
    {
        return TRUE;
    }
    
    if (poOpenInfo->nHeaderBytes > EPS_MIN_GRAYSCALE_BUF &&
        (EQUALN((const char*)poOpenInfo->pabyHeader, "type=gs", 7) ||
         EQUALN((const char*)poOpenInfo->pabyHeader, "type=tc", 7)))
    {
        return TRUE;
    }
    
    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset* EpsilonDataset::Open(GDALOpenInfo* poOpenInfo)
{
    if (!Identify(poOpenInfo))
        return NULL;

    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The EPSILON driver does not support update access to existing"
                  " files.\n" );
        return NULL;
    }

    VSILFILE* fp = VSIFOpenL(poOpenInfo->pszFilename, "rb");
    if (fp == NULL)
        return NULL;
    
    EpsilonDataset* poDS = new EpsilonDataset();
    poDS->fp = fp;
    
    poDS->nRasterXSize = 0;
    poDS->nRasterYSize = 0;
    
    int nBandsToAdd = 0;
    if (!poDS->ScanBlocks(&nBandsToAdd))
    {
        delete poDS;
        return NULL;
    }
    
    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize) ||
        !GDALCheckBandCount(nBandsToAdd, FALSE))
    {
        delete poDS;
        return NULL;
    }
    if (!poDS->bRegularTiling)
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The EPSILON driver does not support reading "
                  "not regularly blocked files.\n" );
        delete poDS;
        return NULL;
    }
    
    int i;
    for(i=1;i<=nBandsToAdd;i++)
        poDS->SetBand(i, new EpsilonRasterBand(poDS, i));
        
    if (nBandsToAdd > 1)
        poDS->SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
    
    return poDS;
}


/************************************************************************/
/*                  EpsilonDatasetCreateCopy ()                         */
/************************************************************************/

GDALDataset *
EpsilonDatasetCreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                       CPL_UNUSED int bStrict, char ** papszOptions, 
                       GDALProgressFunc pfnProgress, void * pProgressData )
{
    int nBands = poSrcDS->GetRasterCount();
    if ((nBands != 1 && nBands != 3) ||
        (nBands > 0 && poSrcDS->GetRasterBand(1)->GetColorTable() != NULL))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The EPSILON driver only supports 1 band (grayscale) "
                 "or 3 band (RGB) data");
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Fetch and check creation options                                */
/* -------------------------------------------------------------------- */

    int nBlockXSize =
        atoi(CSLFetchNameValueDef(papszOptions, "BLOCKXSIZE", "256"));
    int nBlockYSize =
        atoi(CSLFetchNameValueDef(papszOptions, "BLOCKYSIZE", "256"));
    if ((nBlockXSize != 32 && nBlockXSize != 64 && nBlockXSize != 128 &&
         nBlockXSize != 256 && nBlockXSize != 512 && nBlockXSize != 1024) ||
        (nBlockYSize != 32 && nBlockYSize != 64 && nBlockYSize != 128 &&
         nBlockYSize != 256 && nBlockYSize != 512 && nBlockYSize != 1024))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                "Block size must be a power of 2 between 32 et 1024");
        return NULL;
    }         
    
    const char* pszFilter =
        CSLFetchNameValueDef(papszOptions, "FILTER", "daub97lift");
    char** papszFBID = eps_get_fb_info(EPS_FB_ID);
    char** papszFBIDIter = papszFBID;
    int bFound = FALSE;
    int nIndexFB = 0;
    while(papszFBIDIter && *papszFBIDIter && !bFound)
    {
        if (strcmp(*papszFBIDIter, pszFilter) == 0)
            bFound = TRUE;
        else
            nIndexFB ++;
        papszFBIDIter ++;
    }
    eps_free_fb_info(papszFBID);
    if (!bFound)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "FILTER='%s' not supported",
                 pszFilter);
        return NULL;
    }
    
    int eMode = EPS_MODE_OTLPF;
    const char* pszMode = CSLFetchNameValueDef(papszOptions, "MODE", "OTLPF");
    if (EQUAL(pszMode, "NORMAL"))
        eMode = EPS_MODE_NORMAL;
    else if (EQUAL(pszMode, "OTLPF"))
        eMode = EPS_MODE_OTLPF;
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported, "MODE='%s' not supported",
                 pszMode);
        return NULL;
    }
    
    char** papszFBType = eps_get_fb_info(EPS_FB_TYPE);
    int bIsBiOrthogonal = EQUAL(papszFBType[nIndexFB], "biorthogonal");
    eps_free_fb_info(papszFBType);
    
    if (eMode == EPS_MODE_OTLPF && !bIsBiOrthogonal)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "MODE=OTLPF can only be used with biorthogonal filters. "
                 "Use MODE=NORMAL instead");
        return NULL;
    }    
    
    int bRasterliteOutput =
        CSLTestBoolean(CSLFetchNameValueDef(papszOptions,
                                            "RASTERLITE_OUTPUT", "NO"));
             
    int nYRatio = EPS_Y_RT;
    int nCbRatio = EPS_Cb_RT;
    int nCrRatio = EPS_Cr_RT;
    
    int eResample;
    if (CSLTestBoolean(CSLFetchNameValueDef(papszOptions,
                                            "RGB_RESAMPLE", "YES")))
        eResample = EPS_RESAMPLE_420;
    else
        eResample = EPS_RESAMPLE_444;
    
    const char* pszTarget = CSLFetchNameValueDef(papszOptions, "TARGET", "96");
    double dfReductionFactor = 1 - CPLAtof(pszTarget) / 100;
    if (dfReductionFactor > 1)
        dfReductionFactor = 1;
    else if (dfReductionFactor < 0)
        dfReductionFactor = 0;
    
/* -------------------------------------------------------------------- */
/*      Open file                                                       */
/* -------------------------------------------------------------------- */

    VSILFILE* fp = VSIFOpenL(pszFilename, "wb");
    if (fp == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create %s", pszFilename);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Compute number of blocks, block size, etc...                    */
/* -------------------------------------------------------------------- */

    int nXSize = poSrcDS->GetRasterXSize();
    int nYSize = poSrcDS->GetRasterYSize();
    if (eMode == EPS_MODE_OTLPF)
    {
        nBlockXSize ++;
        nBlockYSize ++;
    }
    int nXBlocks = (nXSize + nBlockXSize - 1) / nBlockXSize;
    int nYBlocks = (nYSize + nBlockYSize - 1) / nBlockYSize;
    int nBlocks = nXBlocks * nYBlocks;
    int nUncompressedFileSize = nXSize * nYSize * nBands;
    int nUncompressedBlockSize = nUncompressedFileSize / nBlocks;
    int nTargetBlockSize = (int) (dfReductionFactor * nUncompressedBlockSize);
    if (nBands == 1)
        nTargetBlockSize = MAX (nTargetBlockSize, EPS_MIN_GRAYSCALE_BUF + 1);
    else
        nTargetBlockSize = MAX (nTargetBlockSize, EPS_MIN_TRUECOLOR_BUF + 1);

/* -------------------------------------------------------------------- */
/*      Allocate work buffers                                           */
/* -------------------------------------------------------------------- */

    GByte* pabyBuffer = (GByte*)VSIMalloc3(nBlockXSize, nBlockYSize, nBands);
    if (pabyBuffer == NULL)
    {
        VSIFCloseL(fp);
        return NULL;
    }
    
    GByte* pabyOutBuf = (GByte*)VSIMalloc(nTargetBlockSize);
    if (pabyOutBuf == NULL)
    {
        VSIFree(pabyBuffer);
        VSIFCloseL(fp);
        return NULL;
    }
    
    GByte** apapbyRawBuffer[3];
    int i, j;
    for(i=0;i<nBands;i++)
    {
        apapbyRawBuffer[i] = (GByte**) VSIMalloc(sizeof(GByte*) * nBlockYSize);
        for(j=0;j<nBlockYSize;j++)
        {
            apapbyRawBuffer[i][j] =
                            pabyBuffer + (i * nBlockXSize + j) * nBlockYSize;
        }
    }
    
    if (bRasterliteOutput)
    {
        const char* pszHeader = RASTERLITE_WAVELET_HEADER;
        VSIFWriteL(pszHeader, 1, strlen(pszHeader) + 1, fp);
    }

/* -------------------------------------------------------------------- */
/*      Iterate over blocks                                             */
/* -------------------------------------------------------------------- */

    int nBlockXOff, nBlockYOff;
    CPLErr eErr = CE_None;
    for(nBlockYOff = 0;
        eErr == CE_None && nBlockYOff < nYBlocks; nBlockYOff ++)
    {
        for(nBlockXOff = 0;
            eErr == CE_None && nBlockXOff < nXBlocks; nBlockXOff ++)
        {
            int bMustMemset = FALSE;
            int nReqXSize = nBlockXSize, nReqYSize = nBlockYSize;
            if ((nBlockXOff+1) * nBlockXSize > nXSize)
            {
                bMustMemset = TRUE;
                nReqXSize = nXSize - nBlockXOff * nBlockXSize;
            }
            if ((nBlockYOff+1) * nBlockYSize > nYSize)
            {
                bMustMemset = TRUE;
                nReqYSize = nYSize - nBlockYOff * nBlockYSize;
            }
            if (bMustMemset)
                memset(pabyBuffer, 0, nBands * nBlockXSize * nBlockYSize);
            
            eErr = poSrcDS->RasterIO(GF_Read,
                              nBlockXOff * nBlockXSize,
                              nBlockYOff * nBlockYSize,
                              nReqXSize, nReqYSize,
                              pabyBuffer,
                              nReqXSize, nReqYSize,
                              GDT_Byte, nBands, NULL,
                              1,
                              nBlockXSize,
                              nBlockXSize * nBlockYSize, NULL);
            
            int nOutBufSize = nTargetBlockSize;
            if (eErr == CE_None && nBands == 1)
            {
                if (EPS_OK != eps_encode_grayscale_block(apapbyRawBuffer[0],
                                           nXSize, nYSize,
                                           nReqXSize, nReqYSize,
                                           nBlockXOff * nBlockXSize,
                                           nBlockYOff * nBlockYSize,
                                           pabyOutBuf, &nOutBufSize,
                                           (char*) pszFilter, eMode))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Error occured when encoding block (%d, %d)",
                             nBlockXOff, nBlockYOff);
                    eErr = CE_Failure;
                }
            }
            else if (eErr == CE_None)
            {
                if (EPS_OK != eps_encode_truecolor_block(
                                           apapbyRawBuffer[0],
                                           apapbyRawBuffer[1],
                                           apapbyRawBuffer[2],
                                           nXSize, nYSize,
                                           nReqXSize, nReqYSize,
                                           nBlockXOff * nBlockXSize,
                                           nBlockYOff * nBlockYSize,
                                           eResample,
                                           pabyOutBuf, &nOutBufSize,
                                           nYRatio, nCbRatio, nCrRatio,
                                           (char*) pszFilter, eMode))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Error occured when encoding block (%d, %d)",
                             nBlockXOff, nBlockYOff);
                    eErr = CE_Failure;
                }
            }
            
            if (eErr == CE_None)
            {
                if ((int)VSIFWriteL(pabyOutBuf, 1, nOutBufSize, fp) !=
                                                                nOutBufSize)
                    eErr = CE_Failure;

                char chEPSMarker = EPS_MARKER;
                VSIFWriteL(&chEPSMarker, 1, 1, fp);
                
                if (pfnProgress && !pfnProgress(
                      1.0 * (nBlockYOff * nXBlocks + nBlockXOff + 1) / nBlocks,
                      NULL, pProgressData))
                {
                    eErr = CE_Failure;
                }
            }
        }
    }
    
    if (bRasterliteOutput)
    {
        const char* pszFooter = RASTERLITE_WAVELET_FOOTER;
        VSIFWriteL(pszFooter, 1, strlen(pszFooter) + 1, fp);
    }

/* -------------------------------------------------------------------- */
/*      Cleanup work buffers                                            */
/* -------------------------------------------------------------------- */
    
    for(i=0;i<nBands;i++)
    {
        VSIFree(apapbyRawBuffer[i]);
    }
    
    VSIFree(pabyOutBuf);
    VSIFree(pabyBuffer);
        
    VSIFCloseL(fp);
    
    if (eErr != CE_None)
        return NULL;

/* -------------------------------------------------------------------- */
/*      Reopen the dataset, unless asked for not (Rasterlite optim)     */
/* -------------------------------------------------------------------- */
    return (GDALDataset*) GDALOpen(pszFilename, GA_ReadOnly);
}

/************************************************************************/
/*                     GDALRegister_EPSILON()                           */
/************************************************************************/

void GDALRegister_EPSILON()

{
    GDALDriver  *poDriver;
    
    if (! GDAL_CHECK_VERSION("EPSILON driver"))
        return;

    if( GDALGetDriverByName( "EPSILON" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "EPSILON" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Epsilon wavelets" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "frmt_epsilon.html" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                                   "Byte" );

        CPLString osMethods;
        char** papszFBID = eps_get_fb_info(EPS_FB_ID);
        char** papszFBIDIter = papszFBID;
        while(papszFBIDIter && *papszFBIDIter)
        {
            osMethods += "       <Value>";
            osMethods += *papszFBIDIter;
            osMethods += "</Value>\n";
            papszFBIDIter ++;
        }
        eps_free_fb_info(papszFBID);
        
        CPLString osOptionList;
        osOptionList.Printf(
"<CreationOptionList>"
"   <Option name='TARGET' type='int' description='target size reduction as a percentage of the original (0-100)' default='75'/>"
"   <Option name='FILTER' type='string-select' description='Filter ID' default='daub97lift'>"
"%s"
"   </Option>"
"   <Option name='BLOCKXSIZE' type='int' description='Tile Width. Between 32 and 1024' default='256'/>"
"   <Option name='BLOCKYSIZE' type='int' description='Tile Height. Between 32 and 1024' default='256'/>"
"   <Option name='MODE' type='string-select' default='OTLPF'>"
"       <Value>NORMAL</Value>"
"       <Value>OTLPF</Value>"
"   </Option>"
"   <Option name='RGB_RESAMPLE' type='boolean' description='if RGB must be resampled to 4:2:0' default='YES'/>"
"   <Option name='RASTERLITE_OUTPUT' type='boolean' description='if Rasterlite header and footers must be inserted' default='FALSE'/>"
"</CreationOptionList>", osMethods.c_str()  );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
                                   osOptionList.c_str() );
                            
        poDriver->pfnOpen = EpsilonDataset::Open;
        poDriver->pfnIdentify = EpsilonDataset::Identify;
        poDriver->pfnCreateCopy = EpsilonDatasetCreateCopy;
        
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
