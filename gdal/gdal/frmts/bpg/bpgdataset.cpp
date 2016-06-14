/******************************************************************************
 * $Id$
 *
 * Project:  GDAL BPG Driver
 * Purpose:  Implement GDAL BPG Support based on libbpg
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys dot com>
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

#include <stdint.h>

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"

// NOTE: build instructions
// bpg Makefile needs to be modified to have -fPIC on CFLAGS:= at line 49 and LDFLAGS at line 54 before building bpg
// g++ -fPIC -g -Wall -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -I/home/even/libbpg-0.9.4 frmts/bpg/bpgdataset.cpp -shared -o gdal_BPG.so -L. -lgdal -L/home/even/libbpg-0.9.4/ -lbpg

CPL_C_START
#include "libbpg.h"
CPL_C_END

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*                               BPGDataset                             */
/* ==================================================================== */
/************************************************************************/

class BPGRasterBand;

class BPGDataset : public GDALPamDataset
{
    friend class BPGRasterBand;

    VSILFILE*   fpImage;
    GByte* pabyUncompressed;
    int    bHasBeenUncompressed;
    CPLErr eUncompressErrRet;
    CPLErr Uncompress();

  public:
                 BPGDataset();
                 ~BPGDataset();

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            BPGRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class BPGRasterBand : public GDALPamRasterBand
{
    friend class BPGDataset;

  public:

                   BPGRasterBand( BPGDataset *, int nbits );

    virtual CPLErr IReadBlock( int, int, void * );
    virtual GDALColorInterp GetColorInterpretation();
};

/************************************************************************/
/*                          BPGRasterBand()                            */
/************************************************************************/

BPGRasterBand::BPGRasterBand( BPGDataset *poDS, int nbits )
{
    this->poDS = poDS;

    eDataType = (nbits > 8) ? GDT_UInt16 : GDT_Byte;

    nBlockXSize = poDS->nRasterXSize;
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr BPGRasterBand::IReadBlock( CPL_UNUSED int nBlockXOff, int nBlockYOff,
                                   void * pImage )
{
    BPGDataset* poGDS = (BPGDataset*) poDS;

    if( poGDS->Uncompress() != CE_None )
        return CE_Failure;

    int i;
    int iBand = nBand;
    if( poGDS->nBands == 2 )
        iBand = 4;
    if( eDataType == GDT_Byte )
    {
        GByte* pabyUncompressed =
            &poGDS->pabyUncompressed[nBlockYOff * nRasterXSize  * poGDS->nBands + iBand - 1];
        for(i=0;i<nRasterXSize;i++)
            ((GByte*)pImage)[i] = pabyUncompressed[poGDS->nBands * i];
    }
    else
    {
        GUInt16* pasUncompressed = (GUInt16*)
            &poGDS->pabyUncompressed[(nBlockYOff * nRasterXSize  * poGDS->nBands + iBand - 1) * 2];
        for(i=0;i<nRasterXSize;i++)
            ((GUInt16*)pImage)[i] = pasUncompressed[poGDS->nBands * i];
    }

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp BPGRasterBand::GetColorInterpretation()

{
    BPGDataset* poGDS = (BPGDataset*) poDS;
    if( poGDS->nBands >= 3 )
        return (GDALColorInterp) (GCI_RedBand + (nBand - 1));
    else if( nBand == 1 )
        return GCI_GrayIndex;
    else
        return GCI_AlphaBand;
}

/************************************************************************/
/* ==================================================================== */
/*                             BPGDataset                               */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                            BPGDataset()                              */
/************************************************************************/

BPGDataset::BPGDataset()

{
    fpImage = NULL;
    pabyUncompressed = NULL;
    bHasBeenUncompressed = FALSE;
    eUncompressErrRet = CE_None;
}

/************************************************************************/
/*                           ~BPGDataset()                              */
/************************************************************************/

BPGDataset::~BPGDataset()

{
    FlushCache();
    if (fpImage)
        VSIFCloseL(fpImage);
    VSIFree(pabyUncompressed);
}

/************************************************************************/
/*                            Uncompress()                              */
/************************************************************************/

CPLErr BPGDataset::Uncompress()
{
    if (bHasBeenUncompressed)
        return eUncompressErrRet;
    bHasBeenUncompressed = TRUE;
    eUncompressErrRet = CE_Failure;

    int nOutBands = (nBands < 3) ? nBands + 2 : nBands;
    int nDTSize = ( GetRasterBand(1)->GetRasterDataType() == GDT_Byte ) ? 1 : 2;
    pabyUncompressed = (GByte*)VSIMalloc3(nRasterXSize, nRasterYSize,
                                          nOutBands * nDTSize);
    if (pabyUncompressed == NULL)
        return CE_Failure;

    VSIFSeekL(fpImage, 0, SEEK_END);
    vsi_l_offset nSize = VSIFTellL(fpImage);
    if (nSize != (vsi_l_offset)(int)nSize)
        return CE_Failure;
    VSIFSeekL(fpImage, 0, SEEK_SET);
    uint8_t* pabyCompressed = (uint8_t*)VSIMalloc(nSize);
    if (pabyCompressed == NULL)
        return CE_Failure;
    VSIFReadL(pabyCompressed, 1, nSize, fpImage);

    BPGDecoderContext* ctxt = bpg_decoder_open();
    if( ctxt == NULL )
    {
        VSIFree(pabyCompressed);
        return CE_Failure;
    }
    BPGDecoderOutputFormat eOutputFormat;

    if( GetRasterBand(1)->GetRasterDataType() == GDT_Byte )
        eOutputFormat = (nBands == 1 || nBands == 3) ? BPG_OUTPUT_FORMAT_RGB24 :
                                                       BPG_OUTPUT_FORMAT_RGBA32;
    else
        eOutputFormat = (nBands == 1 || nBands == 3) ? BPG_OUTPUT_FORMAT_RGB48 :
                                                       BPG_OUTPUT_FORMAT_RGBA64;
    if( bpg_decoder_decode(ctxt, pabyCompressed, (int)nSize) < 0 ||
        bpg_decoder_start(ctxt, eOutputFormat) < 0 )
    {
        bpg_decoder_close(ctxt);
        VSIFree(pabyCompressed);
        return CE_Failure;
    }

    for(int i=0;i<nRasterYSize;i++)
    {
        void* pRow = pabyUncompressed + i * nBands * nRasterXSize * nDTSize;
        if( bpg_decoder_get_line(ctxt, pRow) < 0 )
        {
            bpg_decoder_close(ctxt);
            VSIFree(pabyCompressed);
            return CE_Failure;
        }
    }

    bpg_decoder_close(ctxt);
    VSIFree(pabyCompressed);

    eUncompressErrRet = CE_None;

    return CE_None;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int BPGDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes < BPG_DECODER_INFO_BUF_SIZE )
        return FALSE;

    return memcmp(poOpenInfo->pabyHeader, "BPG\xfb", 4) == 0;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *BPGDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify( poOpenInfo ) || poOpenInfo->fpL == NULL )
        return NULL;

    BPGImageInfo imageInfo;
    if( bpg_decoder_get_info_from_buf(&imageInfo, NULL,
                                      poOpenInfo->pabyHeader,
                                      poOpenInfo->nHeaderBytes) < 0 )
        return NULL;

    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The BPG driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    BPGDataset  *poDS;

    poDS = new BPGDataset();
    poDS->nRasterXSize = imageInfo.width;
    poDS->nRasterYSize = imageInfo.height;
    poDS->fpImage = poOpenInfo->fpL;
    poOpenInfo->fpL = NULL;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int nBands = ( imageInfo.format == BPG_FORMAT_GRAY ) ? 1 : 3;
    if( imageInfo.has_alpha )
        nBands ++;

    for( int iBand = 0; iBand < nBands; iBand++ )
        poDS->SetBand( iBand+1, new BPGRasterBand( poDS, imageInfo.bit_depth ) );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );

    poDS->TryLoadXML( poOpenInfo->GetSiblingFiles() );

/* -------------------------------------------------------------------- */
/*      Open overviews.                                                 */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename,
                                 poOpenInfo->GetSiblingFiles() );

    return poDS;
}

/************************************************************************/
/*                         GDALRegister_BPG()                           */
/************************************************************************/

void GDALRegister_BPG()

{
    if( GDALGetDriverByName( "BPG" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "BPG" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "Better Portable Graphics" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                               "frmt_bpg.html" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "bpg" );
    // poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/bpg" );

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnIdentify = BPGDataset::Identify;
    poDriver->pfnOpen = BPGDataset::Open;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
