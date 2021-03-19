/******************************************************************************
 *
 * Project:  JPEGLS driver based on CharLS library
 * Purpose:  JPEGLS driver based on CharLS library
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"

#include "jpegls_header.h"

/* g++ -Wall -g fmrts/jpegls/jpeglsdataset.cpp -shared -fPIC -o gdal_JPEGLS.so -Iport -Igcore -L. -lgdal -I/home/even/charls-1.0 -L/home/even/charls-1.0/build -lCharLS */

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                           JPEGLSDataset                              */
/* ==================================================================== */
/************************************************************************/

class JPEGLSDataset final: public GDALPamDataset
{
    friend class JPEGLSRasterBand;

    CPLString osFilename;
    VSILFILE *fpL;
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

class JPEGLSRasterBand final: public GDALPamRasterBand
{
    friend class JPEGLSDataset;

  public:

                JPEGLSRasterBand( JPEGLSDataset * poDS, int nBand);
    virtual ~JPEGLSRasterBand();

    virtual CPLErr          IReadBlock( int, int, void * ) override;
    virtual GDALColorInterp GetColorInterpretation() override;
};

/************************************************************************/
/*                        JPEGLSRasterBand()                            */
/************************************************************************/

JPEGLSRasterBand::JPEGLSRasterBand( JPEGLSDataset *poDSIn, int nBandIn )

{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = (poDSIn->nBitsPerSample <= 8) ? GDT_Byte : GDT_Int16;
    nBlockXSize = poDSIn->nRasterXSize;
    nBlockYSize = poDSIn->nRasterYSize;
}

/************************************************************************/
/*                      ~JPEGLSRasterBand()                             */
/************************************************************************/

JPEGLSRasterBand::~JPEGLSRasterBand() {}

/************************************************************************/
/*                    JPEGLSGetErrorAsString()                          */
/************************************************************************/

#ifdef CHARLS_2
static const char* JPEGLSGetErrorAsString(CharlsApiResultType eCode)
{
    switch(eCode)
    {
        case CharlsApiResultType::OK: return "OK";
        case CharlsApiResultType::InvalidJlsParameters: return "InvalidJlsParameters";
        case CharlsApiResultType::ParameterValueNotSupported: return "ParameterValueNotSupported";
        case CharlsApiResultType::UncompressedBufferTooSmall: return "UncompressedBufferTooSmall";
        case CharlsApiResultType::CompressedBufferTooSmall: return "CompressedBufferTooSmall";
#ifndef CHARLS_2_1
        case CharlsApiResultType::InvalidCompressedData: return "InvalidCompressedData";
        case CharlsApiResultType::ImageTypeNotSupported: return "ImageTypeNotSupported";
        case CharlsApiResultType::UnsupportedBitDepthForTransform: return "UnsupportedBitDepthForTransform";
#endif
        case CharlsApiResultType::UnsupportedColorTransform: return "UnsupportedColorTransform";
        default: return "unknown";
    };
}
#else
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
#endif

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr JPEGLSRasterBand::IReadBlock( int /*nBlockXOff*/, int /*nBlockYOff*/,
                                      void * pImage )
{
    JPEGLSDataset *poGDS = (JPEGLSDataset *) poDS;

    if (!poGDS->bHasUncompressed)
    {
        CPLErr eErr = poGDS->Uncompress();
        if (eErr != CE_None)
            return eErr;
    }

    if (poGDS->pabyUncompressedData == nullptr)
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

JPEGLSDataset::JPEGLSDataset() :
    fpL(nullptr),
    pabyUncompressedData(nullptr),
    bHasUncompressed(FALSE),
    nBitsPerSample(0),
    nOffset(0)
{}

/************************************************************************/
/*                         ~JPEGLSDataset()                        */
/************************************************************************/

JPEGLSDataset::~JPEGLSDataset()

{
    if( fpL != nullptr )
        VSIFCloseL(fpL);
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

    CPLAssert( fpL != nullptr );
    VSIFSeekL(fpL, 0, SEEK_END);
    const vsi_l_offset nFileSizeBig = VSIFTellL(fpL) - nOffset;
    VSIFSeekL(fpL, 0, SEEK_SET);
    const size_t nFileSize = static_cast<size_t>(nFileSizeBig);
#if SIZEOF_VOIDP != 8
    if( nFileSizeBig != nFileSize )
    {
        return CE_Failure;
    }
#endif

    GByte* pabyCompressedData = (GByte*)VSIMalloc(nFileSize);
    if (pabyCompressedData == nullptr)
    {
        VSIFCloseL(fpL);
        fpL = nullptr;
        return CE_Failure;
    }

    VSIFSeekL(fpL, nOffset, SEEK_SET);
    if( VSIFReadL(pabyCompressedData, 1, nFileSize, fpL) != nFileSize )
    {
        VSIFCloseL(fpL);
        fpL = nullptr;
        return CE_Failure;
    }
    VSIFCloseL(fpL);
    fpL = nullptr;

    const GUIntBig nUncompressedSizeBig = static_cast<GUIntBig>(nRasterXSize) *
        nRasterYSize * nBands *
        GDALGetDataTypeSizeBytes(GetRasterBand(1)->GetRasterDataType());
    const size_t nUncompressedSize = static_cast<size_t>(nUncompressedSizeBig);
#if SIZEOF_VOIDP != 8
    if( nUncompressedSizeBig != nUncompressedSize )
    {
        VSIFree(pabyCompressedData);
        return CE_Failure;
    }
#endif
    pabyUncompressedData = (GByte*)VSI_MALLOC_VERBOSE(nUncompressedSize);
    if (pabyUncompressedData == nullptr)
    {
        VSIFree(pabyCompressedData);
        return CE_Failure;
    }

#ifdef CHARLS_2
    auto eError = JpegLsDecode( pabyUncompressedData, nUncompressedSize,
                            pabyCompressedData, nFileSize, nullptr, nullptr);
    if (eError != CharlsApiResultType::OK)
#else
    auto eError = JpegLsDecode( pabyUncompressedData, nUncompressedSize,
                                     pabyCompressedData, nFileSize, nullptr);
    if (eError != OK)
#endif
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Decompression of data failed : %s",
                  JPEGLSGetErrorAsString(eError) );
        VSIFree(pabyCompressedData);
        VSIFree(pabyUncompressedData);
        pabyUncompressedData = nullptr;
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
    const GByte  *pabyHeader = poOpenInfo->pabyHeader;
    int    nHeaderBytes = poOpenInfo->nHeaderBytes;

    bIsDCOM = FALSE;

    if( poOpenInfo->fpL == nullptr || nHeaderBytes < 10 )
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
        return nullptr;

    if( !GDALIsDriverDeprecatedForGDAL35StillEnabled("JPEGLS") )
        return nullptr;

    JlsParameters sParams;

#ifdef CHARLS_2
    CharlsApiResultType eError;
#else
    JLS_ERROR eError;
#endif
    int nOffset = 0;

    if (!bIsDCOM)
    {
#ifdef CHARLS_2
        eError = JpegLsReadHeader(poOpenInfo->pabyHeader,
                                poOpenInfo->nHeaderBytes, &sParams, nullptr);
#else
        eError = JpegLsReadHeader(poOpenInfo->pabyHeader,
                                poOpenInfo->nHeaderBytes, &sParams);
#endif
    }
    else
    {
        VSILFILE* fp = poOpenInfo->fpL;
        GByte abyBuffer[1028];
        GByte abySignature[] = { 0xFF, 0xD8, 0xFF, 0xF7 };
        VSIFSeekL(fp, 0, SEEK_SET);
        while( true )
        {
            if (VSIFReadL(abyBuffer, 1, 1028, fp) != 1028)
            {
                VSIFCloseL(fp);
                return nullptr;
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
        VSIFSeekL(fp, 0, SEEK_SET);
#ifdef CHARLS_2
        eError = JpegLsReadHeader(abyBuffer, 1024, &sParams, nullptr);
        if (eError == CharlsApiResultType::OK )
#else
        eError = JpegLsReadHeader(abyBuffer, 1024, &sParams);
        if (eError == OK)
#endif
        {
            CPLDebug("JPEGLS", "JPEGLS image found at offset %d", nOffset);
        }
    }
#ifdef CHARLS_2
    if (eError != CharlsApiResultType::OK)
#else
    if (eError != OK)
#endif
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot read header : %s",
                     JPEGLSGetErrorAsString(eError));
        return nullptr;
    }

#ifdef CHARLS_2
    int nBitsPerSample = sParams.bitsPerSample;
#else
    int nBitsPerSample = sParams.bitspersample;
#endif

    if (nBitsPerSample > 16)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported bitspersample : %d",
                 nBitsPerSample);
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    JPEGLSDataset *poDS = new JPEGLSDataset();
    poDS->osFilename = poOpenInfo->pszFilename;
    poDS->nRasterXSize = sParams.width;
    poDS->nRasterYSize = sParams.height;
    poDS->nBands = sParams.components;
    poDS->nBitsPerSample = nBitsPerSample;
    poDS->nOffset = nOffset;
    poDS->fpL = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iBand = 1; iBand <= poDS->nBands; iBand++ )
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

    return poDS;
}

/************************************************************************/
/*                           JPEGCreateCopy()                           */
/************************************************************************/

GDALDataset *
JPEGLSDataset::CreateCopy( const char * pszFilename, GDALDataset *poSrcDS,
                int bStrict, char ** papszOptions,
                GDALProgressFunc /*pfnProgress*/, void * /*pProgressData*/ )

{

    if( !GDALIsDriverDeprecatedForGDAL35StillEnabled("JPEGLS") )
        return nullptr;

    const int  nBands = poSrcDS->GetRasterCount();
    const int  nXSize = poSrcDS->GetRasterXSize();
    const int  nYSize = poSrcDS->GetRasterYSize();

/* -------------------------------------------------------------------- */
/*      Some some rudimentary checks                                    */
/* -------------------------------------------------------------------- */
    if( nBands != 1 && nBands != 3 && nBands != 4 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "JPGLS driver doesn't support %d bands.  Must be 1 (grey), "
                  "3 (RGB) or 4 bands.\n", nBands );

        return nullptr;
    }

    if (nBands == 1 &&
        poSrcDS->GetRasterBand(1)->GetColorTable() != nullptr)
    {
        CPLError( (bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported,
                  "JPGLS driver ignores color table. "
                  "The source raster band will be considered as grey level.\n"
                  "Consider using color table expansion (-expand option in gdal_translate)\n");
        if (bStrict)
            return nullptr;
    }

    GDALDataType eDT = poSrcDS->GetRasterBand(1)->GetRasterDataType();

    if( eDT != GDT_Byte && eDT != GDT_Int16 )
    {
        CPLError( (bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported,
                  "JPGLS driver doesn't support data type %s",
                  GDALGetDataTypeName(
                      poSrcDS->GetRasterBand(1)->GetRasterDataType()) );

        if (bStrict)
            return nullptr;
    }

    const int nWordSize = GDALGetDataTypeSizeBytes(eDT);
    const GUIntBig nUncompressedSizeBig = static_cast<GUIntBig>(nXSize) *
                                                nYSize * nBands * nWordSize;
#if SIZEOF_VOIDP != 8
    if( nUncompressedSizeBig + 256 !=
                    static_cast<size_t>(nUncompressedSizeBig + 256) )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Too big image");
        return NULL;
    }
#endif
    const size_t nUncompressedSize = static_cast<size_t>(nUncompressedSizeBig);
    // FIXME? bug in charls-1.0beta ?. I needed a "+ something" to
    // avoid errors on byte.tif.
    const size_t nCompressedSize = nUncompressedSize + 256;
    GByte* pabyDataCompressed = (GByte*)VSI_MALLOC_VERBOSE(nCompressedSize);
    GByte* pabyDataUncompressed = (GByte*)VSI_MALLOC_VERBOSE(nUncompressedSize);
    if (pabyDataCompressed == nullptr || pabyDataUncompressed == nullptr)
    {
        VSIFree(pabyDataCompressed);
        VSIFree(pabyDataUncompressed);
        return nullptr;
    }

    CPLErr eErr;
    eErr = poSrcDS->RasterIO(GF_Read, 0, 0, nXSize, nYSize,
                      pabyDataUncompressed, nXSize, nYSize,
                      eDT, nBands, nullptr,
                      nBands * nWordSize, nBands * nWordSize * nXSize, nWordSize, nullptr);
    if (eErr != CE_None)
    {
        VSIFree(pabyDataCompressed);
        VSIFree(pabyDataUncompressed);
        return nullptr;
    }

    size_t nWritten = 0;

    JlsParameters sParams;
    memset(&sParams, 0, sizeof(sParams));
    sParams.width = nXSize;
    sParams.height = nYSize;
#ifdef CHARLS_2
    sParams.bitsPerSample = (eDT == GDT_Byte) ? 8 : 16;
    sParams.interleaveMode = CharlsInterleaveModeType::None;
#else
    sParams.bitspersample = (eDT == GDT_Byte) ? 8 : 16;
    sParams.ilv = ILV_NONE;
#endif

    const char* pszINTERLEAVE = CSLFetchNameValue( papszOptions, "INTERLEAVE" );
    if (pszINTERLEAVE)
    {
#ifdef CHARLS_2
        if (EQUAL(pszINTERLEAVE, "PIXEL"))
            sParams.interleaveMode = CharlsInterleaveModeType::Sample;
        else if (EQUAL(pszINTERLEAVE, "LINE"))
            sParams.interleaveMode = CharlsInterleaveModeType::Line;
        else if (EQUAL(pszINTERLEAVE, "BAND"))
            sParams.interleaveMode = CharlsInterleaveModeType::None;
#else
        if (EQUAL(pszINTERLEAVE, "PIXEL"))
            sParams.ilv = ILV_SAMPLE;
        else if (EQUAL(pszINTERLEAVE, "LINE"))
            sParams.ilv = ILV_LINE;
        else if (EQUAL(pszINTERLEAVE, "BAND"))
            sParams.ilv = ILV_NONE;
#endif
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
        {
#ifdef CHARLS_2
            sParams.allowedLossyError = nLOSSFACTOR;
#else
            sParams.allowedlossyerror = nLOSSFACTOR;
#endif
        }
    }

    const char* pszNBITS = poSrcDS->GetRasterBand(1)->GetMetadataItem( "NBITS", "IMAGE_STRUCTURE" );
    if (pszNBITS != nullptr)
    {
        int nBits = atoi(pszNBITS);
        if (nBits != 8 && nBits != 16)
        {
#ifdef CHARLS_2
            sParams.bitsPerSample = nBits;
#else
            sParams.bitspersample = nBits;
#endif
        }
    }

    sParams.components = nBands;
    auto eError = JpegLsEncode(pabyDataCompressed, nCompressedSize,
                                    &nWritten,
                                    pabyDataUncompressed, nUncompressedSize,
                                    &sParams
#ifdef CHARLS_2
                                    , nullptr
#endif
                              );

    VSIFree(pabyDataUncompressed);
    pabyDataUncompressed = nullptr;

#ifdef CHARLS_2
    if (eError != CharlsApiResultType::OK)
#else
    if (eError != OK)
#endif
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Compression of data failed : %s",
                 JPEGLSGetErrorAsString(eError));
        VSIFree(pabyDataCompressed);
        return nullptr;
    }

    VSILFILE* fp = VSIFOpenL(pszFilename, "wb");
    if (fp == nullptr)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot create %s", pszFilename);
        VSIFree(pabyDataCompressed);
        return nullptr;
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
    if( !GDAL_CHECK_VERSION( "JPEGLS driver" ) )
        return;

    if( GDALGetDriverByName( "JPEGLS" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "JPEGLS" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "JPEGLS" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/jpegls.html" );
    // poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/jls" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "jls" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Byte Int16" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>\n"
"   <Option name='INTERLEAVE' type='string-select' default='BAND' description='File interleaving'>"
"       <Value>PIXEL</Value>"
"       <Value>LINE</Value>"
"       <Value>BAND</Value>"
"   </Option>"
"   <Option name='LOSS_FACTOR' type='int' default='0' description='0 = lossless, 1 = near lossless, >1 = lossy'/>"
"</CreationOptionList>\n" );

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnIdentify = JPEGLSDataset::Identify;
    poDriver->pfnOpen = JPEGLSDataset::Open;
    poDriver->pfnCreateCopy = JPEGLSDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
