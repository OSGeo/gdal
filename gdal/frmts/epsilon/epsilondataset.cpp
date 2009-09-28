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
    if (!(poOpenInfo->nHeaderBytes > nRasterliteWaveletHeaderLen + 1 &&
          EQUALN((const char*)poOpenInfo->pabyHeader,
                 RASTERLITE_WAVELET_HEADER, nRasterliteWaveletHeaderLen)))
    {
        return FALSE;
    }
    
    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset* EpsilonDataset::Open(GDALOpenInfo* poOpenInfo)
{
    if (!Identify(poOpenInfo))
        return NULL;
    
    FILE* fp = VSIFOpenL(poOpenInfo->pszFilename, "rb");
    if (fp == NULL)
        return NULL;
    
    VSIFSeekL(fp, 0, SEEK_END);
    int nSize = (int)VSIFTellL(fp);
    if (nSize > 10 * 1000 * 1000)
    {
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
/*                     GDALRegister_EPSILON()                           */
/************************************************************************/

void GDALRegister_EPSILON()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "EPSILON" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "EPSILON" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Epsilon wavelets" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_epsilon.html" );

        poDriver->pfnOpen = EpsilonDataset::Open;
        poDriver->pfnIdentify = EpsilonDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
