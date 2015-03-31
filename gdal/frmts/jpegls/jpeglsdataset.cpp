/******************************************************************************
 * $Id$
 *
 * Project:  JPEGLS driver based on CharLS library
 * Purpose:  JPEGLS driver based on CharLS library
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "gdal_pam.h"
#include "cpl_string.h"

/* CharLS header */
#include <interface.h>

extern "C" void GDALRegister_JPEGLS();

/* g++ -Wall -g fmrts/jpegls/jpeglsdataset.cpp -shared -fPIC -o gdal_JPEGLS.so -Iport -Igcore -L. -lgdal -I/home/even/charls-1.0 -L/home/even/charls-1.0/build -lCharLS */

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*                           JPEGLSDataset                              */
/* ==================================================================== */
/************************************************************************/

class JPEGLSDataset : public GDALPamDataset
{
    friend class JPEGLSRasterBand;

    CPLString osFilename;
    GByte* pabyUncompressedData;
    int    bHasUncompressed;
    int    nBitsPerSample;
    int    nOffset;

    CPLErr Uncompress();

    static int Identify( GDALOpenInfo * poOpenInfo, int& bIsDCOM );

  public:
                JPEGLSDataset();
                ~JPEGLSDataset();

    static int Identify( GDALOpenInfo * poOpenInfo );
    static GDALDataset  *Open( GDALOpenInfo * );
    static GDALDataset  *CreateCopy( const char * pszFilename, GDALDataset *poSrcDS,
                int bStrict, char ** papszOptions,
                GDALProgressFunc pfnProgress, void * pProgressData );
};

/************************************************************************/
/* ==================================================================== */
/*                         JPEGLSRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class JPEGLSRasterBand : public GDALPamRasterBand
{
    friend class JPEGLSDataset;

  public:

                JPEGLSRasterBand( JPEGLSDataset * poDS, int nBand);
                ~JPEGLSRasterBand();

    virtual CPLErr          IReadBlock( int, int, void * );
    virtual GDALColorInterp GetColorInterpretation();
};


/************************************************************************/
/*                        JPEGLSRasterBand()                            */
/************************************************************************/

JPEGLSRasterBand::JPEGLSRasterBand( JPEGLSDataset *poDS, int nBand)

{
    this->poDS = poDS;
    this->nBand = nBand;
    this->eDataType = (poDS->nBitsPerSample <= 8) ? GDT_Byte : GDT_Int16;
    this->nBlockXSize = poDS->nRasterXSize;
    this->nBlockYSize = poDS->nRasterYSize;
}

/************************************************************************/
/*                      ~JPEGLSRasterBand()                             */
/************************************************************************/

JPEGLSRasterBand::~JPEGLSRasterBand()
{
}


/************************************************************************/
/*                    JPEGLSGetErrorAsString()                          */
/************************************************************************/

static const char* JPEGLSGetErrorAsString(JLS_ERROR eCode)
{
    switch(eCode)
    {
        case OK: return "OK";
        case InvalidJlsParameters: return "InvalidJlsParameters";
        case ParameterValueNotSupported: return "ParameterValueNotSupported";
        case UncompressedBufferTooSmall: return "UncompressedBufferTooSmall";
        case CompressedBufferTooSmall: return "CompressedBufferTooSmall";
        case InvalidCompressedData: return "InvalidCompressedData";
        case ImageTypeNotSupported: return "ImageTypeNotSupported";
        case UnsupportedBitDepthForTransform: return "UnsupportedBitDepthForTransform";
        case UnsupportedColorTransform: return "UnsupportedColorTransform";
        default: return "unknown";
    };
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr JPEGLSRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                      void * pImage )
{
    JPEGLSDataset *poGDS = (JPEGLSDataset *) poDS;

    if (!poGDS->bHasUncompressed)
    {
        CPLErr eErr = poGDS->Uncompress();
        if (eErr != CE_None)
            return eErr;
    }

    if (poGDS->pabyUncompressedData == NULL)
        return CE_Failure;

    int i, j;
    if (eDataType == GDT_Byte)
    {
        for(j=0;j<nBlockYSize;j++)
        {
            for(i=0;i<nBlockXSize;i++)
            {
                ((GByte*)pImage)[j * nBlockXSize + i] =
                    poGDS->pabyUncompressedData[
                            poGDS->nBands * (j * nBlockXSize + i) + nBand - 1];
            }
        }
    }
    else
    {
        for(j=0;j<nBlockYSize;j++)
        {
            for(i=0;i<nBlockXSize;i++)
            {
                ((GInt16*)pImage)[j * nBlockXSize + i] =
                    ((GInt16*)poGDS->pabyUncompressedData)[
                            poGDS->nBands * (j * nBlockXSize + i) + nBand - 1];
            }
        }
    }
    

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp JPEGLSRasterBand::GetColorInterpretation()
{
    JPEGLSDataset *poGDS = (JPEGLSDataset *) poDS;

    if (poGDS->nBands == 1)
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
/*                           JPEGLSDataset                         */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                        JPEGLSDataset()                          */
/************************************************************************/

JPEGLSDataset::JPEGLSDataset()
{
    pabyUncompressedData = NULL;
    bHasUncompressed = FALSE;
    nBitsPerSample = 0;
}

/************************************************************************/
/*                         ~JPEGLSDataset()                        */
/************************************************************************/

JPEGLSDataset::~JPEGLSDataset()

{
    VSIFree(pabyUncompressedData);
}

/************************************************************************/
/*                             Uncompress()                             */
/************************************************************************/

CPLErr JPEGLSDataset::Uncompress()
{
    if (bHasUncompressed)
        return CE_None;

    bHasUncompressed = TRUE;

    VSILFILE* fp = VSIFOpenL(osFilename, "rb");
    if (!fp)
        return CE_Failure;

    VSIFSeekL(fp, 0, SEEK_END);
    int nFileSize = (int)VSIFTellL(fp) - nOffset;
    VSIFSeekL(fp, 0, SEEK_SET);

    GByte* pabyCompressedData = (GByte*)VSIMalloc(nFileSize);
    if (pabyCompressedData == NULL)
    {
        VSIFCloseL(fp);
        return CE_Failure;
    }

    VSIFSeekL(fp, nOffset, SEEK_SET);
    VSIFReadL(pabyCompressedData, 1, nFileSize, fp);
    VSIFCloseL(fp);
    fp = NULL;

    int nUncompressedSize = nRasterXSize * nRasterYSize *
                            nBands * (GDALGetDataTypeSize(GetRasterBand(1)->GetRasterDataType()) / 8);
    pabyUncompressedData = (GByte*)VSIMalloc(nUncompressedSize);
    if (pabyUncompressedData == NULL)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory");
        VSIFree(pabyCompressedData);
        return CE_Failure;
    }


    JLS_ERROR eError = JpegLsDecode(pabyUncompressedData, nUncompressedSize, pabyCompressedData, nFileSize, NULL);
    if (eError != OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Uncompression of data failed : %s",
                    JPEGLSGetErrorAsString(eError));
        VSIFree(pabyCompressedData);
        VSIFree(pabyUncompressedData);
        pabyUncompressedData = NULL;
        return CE_Failure;
    }

    VSIFree(pabyCompressedData);

    return CE_None;
}

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

int JPEGLSDataset::Identify( GDALOpenInfo * poOpenInfo, int& bIsDCOM )

{
    GByte  *pabyHeader = poOpenInfo->pabyHeader;
    int    nHeaderBytes = poOpenInfo->nHeaderBytes;

    bIsDCOM = FALSE;

    if( nHeaderBytes < 10 )
        return FALSE;

    if( pabyHeader[0] != 0xff
        || pabyHeader[1] != 0xd8 )
    {
        /* Is it a DICOM JPEG-LS ? */
        if (nHeaderBytes < 1024)
            return FALSE;

        char abyEmpty[128];
        memset(abyEmpty, 0, sizeof(abyEmpty));
        if (memcmp(pabyHeader, abyEmpty, sizeof(abyEmpty)) != 0)
            return FALSE;

        if (memcmp(pabyHeader + 128, "DICM", 4) != 0)
            return FALSE;

        int i;
        for(i=0;i<1024 - 22;i++)
        {
            if (memcmp(pabyHeader + i, "1.2.840.10008.1.2.4.80", 22) == 0)
            {
                bIsDCOM = TRUE;
                return TRUE;
            }
            if (memcmp(pabyHeader + i, "1.2.840.10008.1.2.4.81", 22) == 0)
            {
                bIsDCOM = TRUE;
                return TRUE;
            }
        }

        return FALSE;
    }

    int nOffset = 2;
    for (;nOffset + 4 < nHeaderBytes;)
    {
        if (pabyHeader[nOffset] != 0xFF)
            return FALSE;

        int nMarker = pabyHeader[nOffset + 1];
        if (nMarker == 0xF7 /* JPEG Extension 7, JPEG-LS */)
            return TRUE;
        if (nMarker == 0xC3 /* Start of Frame 3 */)
            return TRUE;

        nOffset += 2 + pabyHeader[nOffset + 2] * 256 + pabyHeader[nOffset + 3];
    }

    return FALSE;
}

int JPEGLSDataset::Identify( GDALOpenInfo * poOpenInfo )
{
    int bIsDCOM;
    return Identify(poOpenInfo, bIsDCOM);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *JPEGLSDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int bIsDCOM;
    if (!Identify(poOpenInfo, bIsDCOM))
        return NULL;

    JlsParameters sParams;

    JLS_ERROR eError;
    int nOffset = 0;

    if (!bIsDCOM)
    {
        eError = JpegLsReadHeader(poOpenInfo->pabyHeader,
                                poOpenInfo->nHeaderBytes, &sParams);
    }
    else
    {
        VSILFILE* fp = VSIFOpenL(poOpenInfo->pszFilename, "rb");
        if (fp == NULL)
            return NULL;
        GByte abyBuffer[1028];
        GByte abySignature[] = { 0xFF, 0xD8, 0xFF, 0xF7 };
        while(TRUE)
        {
            if (VSIFReadL(abyBuffer, 1, 1028, fp) != 1028)
            {
                VSIFCloseL(fp);
                return NULL;
            }
            int i;
            for(i=0;i<1024;i++)
            {
                if (memcmp(abyBuffer + i, abySignature, 4) == 0)
                {
                    nOffset += i;
                    break;
                }
            }
            if (i != 1024)
                break;
            nOffset += 1024;
            VSIFSeekL(fp, nOffset, SEEK_SET);
        }

        VSIFSeekL(fp, nOffset, SEEK_SET);
        VSIFReadL(abyBuffer, 1, 1024, fp);
        eError = JpegLsReadHeader(abyBuffer, 1024, &sParams);
        VSIFCloseL(fp);
        if (eError == OK)
        {
            CPLDebug("JPEGLS", "JPEGLS image found at offset %d", nOffset);
        }
    }
    if (eError != OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot read header : %s",
                     JPEGLSGetErrorAsString(eError));
        return NULL;
    }

    if (sParams.bitspersample > 16)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupport bitspersample : %d",
                 sParams.bitspersample);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    JPEGLSDataset     *poDS;
    int                 iBand;

    poDS = new JPEGLSDataset();
    poDS->osFilename = poOpenInfo->pszFilename;
    poDS->nRasterXSize = sParams.width;
    poDS->nRasterYSize = sParams.height;
    poDS->nBands = sParams.components;
    poDS->nBitsPerSample = sParams.bitspersample;
    poDS->nOffset = nOffset;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( iBand = 1; iBand <= poDS->nBands; iBand++ )
    {
        poDS->SetBand( iBand, new JPEGLSRasterBand( poDS, iBand) );

        if (poDS->nBitsPerSample != 8 && poDS->nBitsPerSample != 16)
        {
            poDS->GetRasterBand(iBand)->SetMetadataItem( "NBITS",
                                                    CPLString().Printf( "%d", poDS->nBitsPerSample ),
                                                    "IMAGE_STRUCTURE" );
        }
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/*                           JPEGCreateCopy()                           */
/************************************************************************/

GDALDataset *
JPEGLSDataset::CreateCopy( const char * pszFilename, GDALDataset *poSrcDS,
                int bStrict, char ** papszOptions,
                GDALProgressFunc pfnProgress, void * pProgressData )

{
    int  nBands = poSrcDS->GetRasterCount();
    int  nXSize = poSrcDS->GetRasterXSize();
    int  nYSize = poSrcDS->GetRasterYSize();

/* -------------------------------------------------------------------- */
/*      Some some rudimentary checks                                    */
/* -------------------------------------------------------------------- */
    if( nBands != 1 && nBands != 3 && nBands != 4 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "JPGLS driver doesn't support %d bands.  Must be 1 (grey), "
                  "3 (RGB) or 4 bands.\n", nBands );

        return NULL;
    }

    if (nBands == 1 &&
        poSrcDS->GetRasterBand(1)->GetColorTable() != NULL)
    {
        CPLError( (bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported,
                  "JPGLS driver ignores color table. "
                  "The source raster band will be considered as grey level.\n"
                  "Consider using color table expansion (-expand option in gdal_translate)\n");
        if (bStrict)
            return NULL;
    }

    GDALDataType eDT = poSrcDS->GetRasterBand(1)->GetRasterDataType();

    if( eDT != GDT_Byte && eDT != GDT_Int16 )
    {
        CPLError( (bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported,
                  "JPGLS driver doesn't support data type %s",
                  GDALGetDataTypeName(
                      poSrcDS->GetRasterBand(1)->GetRasterDataType()) );

        if (bStrict)
            return NULL;
    }

    int nWordSize = GDALGetDataTypeSize(eDT) / 8;
    int nUncompressedSize = nXSize * nYSize * nBands * nWordSize;
    int nCompressedSize = nUncompressedSize + 256; /* FIXME? bug in charls-1.0beta ?. I needed a "+ something" to avoid erros on byte.tif */
    GByte* pabyDataCompressed = (GByte*)VSIMalloc(nCompressedSize);
    GByte* pabyDataUncompressed = (GByte*)VSIMalloc(nUncompressedSize);
    if (pabyDataCompressed == NULL || pabyDataUncompressed == NULL)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory");
        VSIFree(pabyDataCompressed);
        VSIFree(pabyDataUncompressed);
        return NULL;
    }

    CPLErr eErr;
    eErr = poSrcDS->s(GF_Read, 0, 0, nXSize, nYSize,
                      pabyDataUncompressed, nXSize, nYSize,
                      eDT, nBands, NULL,
                      nBands * nWordSize, nBands * nWordSize * nXSize, nWordSize, NULL);
    if (eErr != CE_None)
    {
        VSIFree(pabyDataCompressed);
        VSIFree(pabyDataUncompressed);
        return NULL;
    }

    size_t nWritten = 0;

    JlsParameters sParams;
    memset(&sParams, 0, sizeof(sParams));
    sParams.width = nXSize;
    sParams.height = nYSize;
    sParams.bitspersample = (eDT == GDT_Byte) ? 8 : 16;
    sParams.ilv = ILV_NONE;

    const char* pszINTERLEAVE = CSLFetchNameValue( papszOptions, "INTERLEAVE" );
    if (pszINTERLEAVE)
    {
        if (EQUAL(pszINTERLEAVE, "PIXEL"))
            sParams.ilv = ILV_SAMPLE;
        else if (EQUAL(pszINTERLEAVE, "LINE"))
            sParams.ilv = ILV_LINE;
        else if (EQUAL(pszINTERLEAVE, "BAND"))
            sParams.ilv = ILV_NONE;
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Unsupported value for INTERLEAVE : %s. Defaulting to BAND",
                     pszINTERLEAVE);
        }
    }

    const char* pszLOSSFACTOR = CSLFetchNameValue( papszOptions, "LOSS_FACTOR" );
    if (pszLOSSFACTOR)
    {
        int nLOSSFACTOR = atoi(pszLOSSFACTOR);
        if (nLOSSFACTOR >= 0)
            sParams.allowedlossyerror = nLOSSFACTOR;
    }

    const char* pszNBITS = poSrcDS->GetRasterBand(1)->GetMetadataItem( "NBITS", "IMAGE_STRUCTURE" );
    if (pszNBITS != NULL)
    {
        int nBits = atoi(pszNBITS);
        if (nBits != 8 && nBits != 16)
            sParams.bitspersample = nBits;
    }
    
    sParams.components = nBands;
    JLS_ERROR eError = JpegLsEncode(pabyDataCompressed, nCompressedSize,
                                    &nWritten,
                                    pabyDataUncompressed, nUncompressedSize,
                                    &sParams);

    VSIFree(pabyDataUncompressed);
    pabyDataUncompressed = NULL;

    if (eError != OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Compression of data failed : %s",
                 JPEGLSGetErrorAsString(eError));
        VSIFree(pabyDataCompressed);
        return NULL;
    }

    VSILFILE* fp = VSIFOpenL(pszFilename, "wb");
    if (fp == NULL)
    {
        VSIFree(pabyDataCompressed);
        return NULL;
    }
    VSIFWriteL(pabyDataCompressed, 1, nWritten, fp);

    VSIFree(pabyDataCompressed);

    VSIFCloseL(fp);

/* -------------------------------------------------------------------- */
/*      Re-open dataset, and copy any auxiliary pam information.         */
/* -------------------------------------------------------------------- */
    JPEGLSDataset *poDS = (JPEGLSDataset *) GDALOpen( pszFilename, GA_ReadOnly );

    if( poDS )
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );

    return poDS;
}

/************************************************************************/
/*                          GDALRegister_JPEGLS()                       */
/************************************************************************/

void GDALRegister_JPEGLS()

{
    GDALDriver  *poDriver;

    if (! GDAL_CHECK_VERSION("JPEGLS driver"))
        return;

    if( GDALGetDriverByName( "JPEGLS" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "JPEGLS" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "JPEGLS" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "frmt_jpegls.html" );
        //poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/jls" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "jls" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                                   "Byte Int16" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>\n"
"   <Option name='INTERLEAVE' type='string-select' default='BAND' description='File interleaving'>"
"       <Value>PIXEL</Value>"
"       <Value>LINE</Value>"
"       <Value>BAND</Value>"
"   </Option>"
"   <Option name='LOSS_FACTOR' type='int' default='0' description='0 = lossless, 1 = near lossless, > 1 lossless'/>"
"</CreationOptionList>\n" );
        
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnIdentify = JPEGLSDataset::Identify;
        poDriver->pfnOpen = JPEGLSDataset::Open;
        poDriver->pfnCreateCopy = JPEGLSDataset::CreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

