/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Epsilon driver
 * Purpose:  Implement GDAL Epsilon support using Epsilon library
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 **********************************************************************
 * Copyright (c) 2009, Even Rouault, <even dot rouault at mines dash paris dot org>
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

/************************************************************************/
/* ==================================================================== */
/*                              EpsilonDataset                          */
/* ==================================================================== */
/************************************************************************/

class EpsilonDataset : public GDALPamDataset
{
    friend class EpsilonRasterBand;
    GByte*   pabyData;
    int      nOff;
    int      nDataSize;
    
    GByte*   pabyBlockData;
    int      nBlockDataSize;
    
    GByte*   pImageData;
    
    int      GetNextBlockData();
    int      DecodeBlock(eps_block_header* phdr, int x, int y, int w, int h);
    void     FillImageBuffer(int x, int y, int w, int h,
                             int nBand, unsigned char** pImage2D);
    
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
};

/************************************************************************/
/*                         EpsilonDataset()                             */
/************************************************************************/

EpsilonDataset::EpsilonDataset()
{
    pabyData = NULL;
    nDataSize = 0;
    nOff = 0;
    
    pabyBlockData = NULL;
    nBlockDataSize = 0;
    
    pImageData = NULL;
}

/************************************************************************/
/*                         ~EpsilonDataset()                            */
/************************************************************************/

EpsilonDataset::~EpsilonDataset()
{
    VSIFree(pabyData);
    VSIFree(pImageData);
}

/************************************************************************/
/*                       EpsilonRasterBand()                            */
/************************************************************************/

EpsilonRasterBand::EpsilonRasterBand(EpsilonDataset* poDS, int nBand)
{
    this->poDS = poDS;
    this->nBand = nBand;
    this->eDataType = GDT_Byte;
    this->nBlockXSize = poDS->nRasterXSize;
    this->nBlockYSize = poDS->nRasterYSize;
}

/************************************************************************/
/*                           IReadBlock()                               */
/************************************************************************/

CPLErr EpsilonRasterBand::IReadBlock( int nBlockXOff,
                                      int nBlockYOff, void * pImage)
{
    EpsilonDataset* poGDS = (EpsilonDataset*) poDS;
    memcpy(pImage, poGDS->pImageData + (nBand - 1) * nBlockXSize * nBlockYSize,
           nBlockXSize * nBlockYSize);
    return CE_None;
}

/************************************************************************/
/*                     GetNextBlockData()                               */
/************************************************************************/

int EpsilonDataset::GetNextBlockData()
{
    nBlockDataSize = 0;
    pabyBlockData = NULL;
    
    while (nOff < nDataSize)
    {
        if (pabyData[nOff] != EPS_MARKER)
        {
            pabyBlockData = pabyData + nOff;
            nOff ++;
            nBlockDataSize ++;
            break;
        }
        nOff++;
    }
    if (nOff == nDataSize)
        return FALSE;
        
    while (nOff < nDataSize)
    {
        if (pabyData[nOff] == EPS_MARKER)
            return TRUE;
            
        nOff ++;
        nBlockDataSize ++;
    }
    
    return TRUE;
}

/************************************************************************/
/*                      FillImageBuffer()                               */
/************************************************************************/

void EpsilonDataset::FillImageBuffer(int x, int y, int w, int h,
                                     int nBand, unsigned char** pImage2D)
{
    int j;
    GByte* pData = pImageData + (nBand - 1) * nRasterXSize * nRasterYSize;
    
    if (x < 0 || x + w > nRasterXSize || y < 0 || y + h > nRasterYSize)
        return;
    
    for(j=0;j<h;j++)
    {
        memcpy(pData + (j + y) * nRasterXSize + x, pImage2D[j], w);
    }
}

/************************************************************************/
/*                          DecodeBlock()                               */
/************************************************************************/

int EpsilonDataset::DecodeBlock(eps_block_header* phdr,
                                int x, int y, int w, int h)
{
    if (nBands == 1)
    {
        unsigned char ** pTempData1 =
            (unsigned char **) eps_malloc_2D (w, h, 1);
        if (eps_decode_grayscale_block (pTempData1,
                                        pabyBlockData, phdr) != EPS_OK)
        {
            eps_free_2D ((void **) pTempData1, w, h);
            
            return FALSE;
        }
        FillImageBuffer(x, y, w, h, 1, pTempData1);
    }
    else
    {
        unsigned char ** pTempData1 =
            (unsigned char **) eps_malloc_2D (w, h, 1);
        unsigned char ** pTempData2 =
            (unsigned char **) eps_malloc_2D (w, h, 1);
        unsigned char ** pTempData3 = 
            (unsigned char **) eps_malloc_2D (w, h, 1);
        if (eps_decode_truecolor_block (pTempData1, pTempData2, pTempData3,
                                        pabyBlockData, phdr) != EPS_OK)
        {
            eps_free_2D ((void **) pTempData1, w, h);
            eps_free_2D ((void **) pTempData2, w, h);
            eps_free_2D ((void **) pTempData3, w, h);
            
            return FALSE;
        }
        FillImageBuffer(x, y, w, h, 1, pTempData1);
        FillImageBuffer(x, y, w, h, 2, pTempData2);
        FillImageBuffer(x, y, w, h, 3, pTempData3);
        
        eps_free_2D ((void **) pTempData1, w, h);
        eps_free_2D ((void **) pTempData2, w, h);
        eps_free_2D ((void **) pTempData3, w, h);
    }
    
    return TRUE;
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

    FILE* fp = VSIFOpenL(poOpenInfo->pszFilename, "rb");
    if (fp == NULL)
        return NULL;
    
    VSIFSeekL(fp, 0, SEEK_END);
    int nSize = (int)VSIFTellL(fp);
    if (nSize > 10 * 1000 * 1000)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "EPSILON driver cannot support reading too big files");
        VSIFCloseL(fp);
        return NULL;
    }
    
    GByte* pabyData = (GByte*)VSIMalloc(nSize);
    if (pabyData == NULL)
    {
        VSIFCloseL(fp);
        return NULL;
    }

    VSIFSeekL(fp, 0, SEEK_SET);
    if ((int)VSIFReadL(pabyData, 1, nSize, fp) != nSize)
    {
        CPLFree(pabyData);
        VSIFCloseL(fp);
        return NULL;
    }
    
    VSIFCloseL(fp);
    fp = NULL;
        
    EpsilonDataset* poDS = new EpsilonDataset();
    poDS->pabyData = pabyData;
    poDS->nDataSize = nSize;
    
    poDS->nRasterXSize = 0;
    poDS->nRasterYSize = 0;
    
    eps_block_header hdr;
    while(TRUE)
    {
        if (!poDS->GetNextBlockData())
        {
            if (poDS->nRasterXSize == 0)
            {
                delete poDS;
                return NULL;
            }
            break;
        }
        
        /* Ignore rasterlite wavelet header */
        int nRasterliteWaveletHeaderLen = strlen(RASTERLITE_WAVELET_HEADER);
        if (poDS->nBlockDataSize >= nRasterliteWaveletHeaderLen &&
            memcmp(poDS->pabyBlockData, RASTERLITE_WAVELET_HEADER,
                   nRasterliteWaveletHeaderLen) == 0)
        {
            continue;
        }
        
        /* Stop at rasterlite wavelet footer */
        int nRasterlineWaveletFooterLen = strlen(RASTERLITE_WAVELET_FOOTER);
        if (poDS->nBlockDataSize >= nRasterlineWaveletFooterLen &&
            memcmp(poDS->pabyBlockData, RASTERLITE_WAVELET_FOOTER,
                   nRasterlineWaveletFooterLen) == 0)
        {
            break;
        }
        
        if (eps_read_block_header (poDS->pabyBlockData,
                                   poDS->nBlockDataSize, &hdr) != EPS_OK)
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
        
        int W = (hdr.block_type == EPS_GRAYSCALE_BLOCK) ? hdr.gs.W : hdr.tc.W;
        int H = (hdr.block_type == EPS_GRAYSCALE_BLOCK) ? hdr.gs.H : hdr.tc.H;
        int x = (hdr.block_type == EPS_GRAYSCALE_BLOCK) ? hdr.gs.x : hdr.tc.x;
        int y = (hdr.block_type == EPS_GRAYSCALE_BLOCK) ? hdr.gs.y : hdr.tc.y;
        int w = (hdr.block_type == EPS_GRAYSCALE_BLOCK) ? hdr.gs.w : hdr.tc.w;
        int h = (hdr.block_type == EPS_GRAYSCALE_BLOCK) ? hdr.gs.h : hdr.tc.h;
        
        //printf("W=%d,H=%d,x=%d,y=%d,w=%d,h=%d\n", W, H, x, y, w, h);
        
        int nNewBands = (hdr.block_type == EPS_GRAYSCALE_BLOCK) ? 1 : 3;
        if (poDS->nRasterXSize == 0)
        {
            int i;
            if (W <= 0 || H <= 0)
            {
                delete poDS;
                return NULL;
            }
            
            poDS->nRasterXSize = W;
            poDS->nRasterYSize = H;

            for(i=1;i<=nNewBands;i++)
                poDS->SetBand(i, new EpsilonRasterBand(poDS, i));
                
            poDS->pImageData = (GByte*)VSICalloc(poDS->nBands, W * H);
            if (poDS->pImageData == NULL)
            {
                delete poDS;
                return NULL;
            }
        }
        else if (poDS->nRasterXSize != W ||
                 poDS->nRasterYSize != H ||
                 poDS->nBands != nNewBands)
        {
            delete poDS;
            return NULL;
        }
        
        if (!poDS->DecodeBlock(&hdr, x, y, w, h))
        {
            delete poDS;
            return NULL;
        }
        
    } 
    
    VSIFree(poDS->pabyData);
    poDS->pabyData = NULL;
    poDS->pabyBlockData = NULL;
    
    return poDS;
}


/************************************************************************/
/*                       RasterliteCreateCopy ()                        */
/************************************************************************/

GDALDataset *
EpsilonDatasetCreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                       int bStrict, char ** papszOptions, 
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
    double dfReductionFactor = 1 - atof(pszTarget) / 100;
    if (dfReductionFactor > 1)
        dfReductionFactor = 1;
    else if (dfReductionFactor < 0)
        dfReductionFactor = 0;
    
/* -------------------------------------------------------------------- */
/*      Open file                                                       */
/* -------------------------------------------------------------------- */

    FILE* fp = VSIFOpenL(pszFilename, "wb");
    if (fp == NULL)
        return NULL;

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
                              nBlockXSize * nBlockYSize);
            
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
    if (CSLTestBoolean(CPLGetConfigOption("GDAL_RELOAD_AFTER_CREATE_COPY", "YES")))
        return (GDALDataset*) GDALOpen(pszFilename, GA_ReadOnly);
    else
        return NULL;
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
"   <Option name='BLOCKXSIZE' type='int' description='Tile Width. Between 32 and 1024' default=256/>"
"   <Option name='BLOCKYSIZE' type='int' description='Tile Height. Between 32 and 1024' default=256/>"
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
