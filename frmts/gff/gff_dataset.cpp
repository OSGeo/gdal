/******************************************************************************
 * $Id$
 *
 * Project:  Ground-based SAR Applitcations Testbed File Format driver
 * Purpose:  Support in GDAL for Sandia National Laboratory's GFF format
 * 	     blame Tisham for putting me up to this
 * Author:   Philippe Vachon <philippe@cowpig.ca>
 *
 ******************************************************************************
 * Copyright (c) 2007, Philippe Vachon
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

#include "gdal_priv.h"
#include "gdal_pam.h"
#include "cpl_port.h"
#include "cpl_conv.h"
#include "cpl_vsi.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/*******************************************************************
 * Declaration of the GFFDataset class                             *
 *******************************************************************/

class GFFRasterBand;

class GFFDataset : public GDALPamDataset 
{
    friend class GFFRasterBand;
    VSILFILE *fp;
    GDALDataType eDataType;
    unsigned int nEndianess;
    /* Some relevant headers */
    unsigned short nVersionMajor;
    unsigned short nVersionMinor;
    unsigned int nLength;
    char *pszCreator;
    /* I am taking this at face value (are they freakin' insane?) */
    float fBPP;
    unsigned int nBPP;

    /* Good information to know */
    unsigned int nFrameCnt;
    unsigned int nImageType;
    unsigned int nRowMajor;
    unsigned int nRgCnt;
    unsigned int nAzCnt;
    long nScaleExponent;
    long nScaleMantissa;
    long nOffsetExponent;
    long nOffsetMantissa;
public:
    GFFDataset();
    ~GFFDataset();

    static GDALDataset *Open( GDALOpenInfo * );
    static int Identify( GDALOpenInfo * poOpenInfo );
};

GFFDataset::GFFDataset()
{
    fp = NULL;
}

GFFDataset::~GFFDataset()
{
    if (fp != NULL)
        VSIFCloseL(fp);
}

/*********************************************************************
 * Declaration and implementation of the GFFRasterBand Class         *
 *********************************************************************/

class GFFRasterBand : public GDALPamRasterBand {
    long nRasterBandMemory;
    int nSampleSize;
public:
    GFFRasterBand( GFFDataset *, int, GDALDataType );
    virtual CPLErr IReadBlock( int, int, void * );
};

/************************************************************************/
/*                           GFFRasterBand()                            */
/************************************************************************/
GFFRasterBand::GFFRasterBand( GFFDataset *poDS, int nBand,
	GDALDataType eDataType ) 
{
    unsigned long nBytes;
    this->poDS = poDS;
    this->nBand = nBand;

    this->eDataType = eDataType;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

    /* Determine the number of bytes per sample */
    switch (eDataType) {
      case GDT_CInt16:
        nBytes = 4;
        break;
      case GDT_CInt32:
      case GDT_CFloat32:
        nBytes = 8;
        break;
      default:
        nBytes = 1;
    }

    nRasterBandMemory = nBytes * poDS->GetRasterXSize();
    nSampleSize = nBytes;

}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GFFRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
				void *pImage ) 
{
    GFFDataset *poGDS = (GFFDataset *)poDS;
    long nOffset = poGDS->nLength;

    VSIFSeekL(poGDS->fp, nOffset + (poGDS->GetRasterXSize() * nBlockYOff * (nSampleSize)),SEEK_SET);

    /* Ingest entire range line */
    if (VSIFReadL(pImage,nRasterBandMemory,1,poGDS->fp) != 1)
        return CE_Failure;

#if defined(CPL_MSB)
    if( GDALDataTypeIsComplex( eDataType ) )
    {
        int nWordSize;

        nWordSize = GDALGetDataTypeSize(eDataType)/16;
        GDALSwapWords( pImage, nWordSize, nBlockXSize, 2*nWordSize );
        GDALSwapWords( ((GByte *) pImage)+nWordSize, 
                        nWordSize, nBlockXSize, 2*nWordSize );
    }
#endif

    return CE_None;
	
}

/********************************************************************
 * ================================================================ *
 * Implementation of the GFFDataset Class                           *
 * ================================================================ *
 ********************************************************************/

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/
int GFFDataset::Identify( GDALOpenInfo *poOpenInfo )
{
    if(poOpenInfo->nHeaderBytes < 7) 
        return 0;

    if (EQUALN((char *)poOpenInfo->pabyHeader,"GSATIMG",7)) 
        return 1;

    return 0;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GFFDataset::Open( GDALOpenInfo *poOpenInfo ) 
{
    unsigned short nCreatorLength = 0;

    /* Check that the dataset is indeed a GSAT File Format (GFF) file */
    if (!GFFDataset::Identify(poOpenInfo)) 
        return NULL;

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The GFF driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }
    
    GFFDataset *poDS;
    poDS = new GFFDataset();

    poDS->fp = VSIFOpenL( poOpenInfo->pszFilename, "r" );
    if( poDS->fp == NULL )
    {
        delete poDS;
        return NULL;
    }

    /* Check the endianess of the file */
    VSIFSeekL(poDS->fp,54,SEEK_SET);
    VSIFReadL(&(poDS->nEndianess),2,1,poDS->fp);

#if defined(CPL_LSB)
    int bSwap = 0;
#else
    int bSwap = 1;
#endif

    VSIFSeekL(poDS->fp,8,SEEK_SET);
    VSIFReadL(&poDS->nVersionMinor,2,1,poDS->fp);
    if (bSwap) CPL_SWAP16PTR(&poDS->nVersionMinor);
    VSIFReadL(&poDS->nVersionMajor,2,1,poDS->fp);
    if (bSwap) CPL_SWAP16PTR(&poDS->nVersionMajor);
    VSIFReadL(&poDS->nLength,4,1,poDS->fp);
    if (bSwap) CPL_SWAP32PTR(&poDS->nLength);
    VSIFReadL(&nCreatorLength,2,1,poDS->fp);
    if (bSwap) CPL_SWAP16PTR(&nCreatorLength);
    /* Hack for now... I should properly load the date metadata, for
     * example
     */
    VSIFSeekL(poDS->fp,56,SEEK_SET);

    /* By looking at the Matlab code, one should write something like the following test */
    /* but the results don't seem to be the ones really expected */
    /*if ((poDS->nVersionMajor == 1 && poDS->nVersionMinor > 7) || (poDS->nVersionMajor > 1))
    {
        float fBPP;
        VSIFRead(&fBPP,4,1,poDS->fp);
        poDS->nBPP = fBPP;
    }
    else*/
    {
        VSIFReadL(&poDS->nBPP,4,1,poDS->fp);
        if (bSwap) CPL_SWAP32PTR(&poDS->nBPP);
    }
    VSIFReadL(&poDS->nFrameCnt,4,1,poDS->fp);
    if (bSwap) CPL_SWAP32PTR(&poDS->nFrameCnt);
    VSIFReadL(&poDS->nImageType,4,1,poDS->fp);
    if (bSwap) CPL_SWAP32PTR(&poDS->nImageType);
    VSIFReadL(&poDS->nRowMajor,4,1,poDS->fp);
    if (bSwap) CPL_SWAP32PTR(&poDS->nRowMajor);
    VSIFReadL(&poDS->nRgCnt,4,1,poDS->fp);
    if (bSwap) CPL_SWAP32PTR(&poDS->nRgCnt);
    VSIFReadL(&poDS->nAzCnt,4,1,poDS->fp);
    if (bSwap) CPL_SWAP32PTR(&poDS->nAzCnt);

    /* We now have enough information to determine the number format */
    switch (poDS->nImageType) {
      case 0:
        poDS->eDataType = GDT_Byte;
        break;

      case 1:
        if (poDS->nBPP == 4)
            poDS->eDataType = GDT_CInt16;
        else
            poDS->eDataType = GDT_CInt32;
        break;

      case 2:
        poDS->eDataType = GDT_CFloat32;
        break;

      default:
        CPLError(CE_Failure, CPLE_AppDefined, "Unknown image type found!");
        delete poDS;
        return NULL;
    }

    /* Set raster width/height 
     * Note that the images that are complex are listed as having twice the
     * number of X-direction values than there are actual pixels. This is 
     * because whoever came up with the format was crazy (actually, my
     * hunch is that they designed it very much for Matlab)
     * */
    if (poDS->nRowMajor) {
        poDS->nRasterXSize = poDS->nRgCnt/(poDS->nImageType == 0 ? 1 : 2);
        poDS->nRasterYSize = poDS->nAzCnt;
    }
    else {
        poDS->nRasterXSize = poDS->nAzCnt/(poDS->nImageType == 0 ? 1 : 2);
        poDS->nRasterYSize = poDS->nRgCnt;
    }

    if (poDS->nRasterXSize <= 0 || poDS->nRasterYSize <= 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid raster dimensions : %d x %d",
                 poDS->nRasterXSize, poDS->nRasterYSize);
        delete poDS;
        return NULL;
    }

    poDS->SetBand(1, new GFFRasterBand(poDS, 1, poDS->eDataType));

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Support overviews.                                              */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return poDS;
}

/************************************************************************/
/*                          GDALRegister_GFF()                          */
/************************************************************************/

void GDALRegister_GFF(void) 
{
    GDALDriver *poDriver;
    if ( GDALGetDriverByName("GFF") == NULL ) {
        poDriver = new GDALDriver();
        poDriver->SetDescription("GFF");
        poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, 
                                  "Ground-based SAR Applications Testbed File Format (.gff)");
        poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "frmt_various.html#GFF");
        poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "gff");
        poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
        poDriver->pfnOpen = GFFDataset::Open;
        GetGDALDriverManager()->RegisterDriver(poDriver);
    }
}
