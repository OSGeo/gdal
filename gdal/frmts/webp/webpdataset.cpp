/******************************************************************************
 * $Id$
 *
 * Project:  GDAL WEBP Driver
 * Purpose:  Implement GDAL WEBP Support based on libwebp
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault
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

#include "webp/decode.h"
#include "webp/encode.h"

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_WEBP(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*                               WEBPDataset                            */
/* ==================================================================== */
/************************************************************************/

class WEBPRasterBand;

class WEBPDataset : public GDALPamDataset
{
    friend class WEBPRasterBand;

    VSILFILE*   fpImage;
    GByte* pabyUncompressed;
    int    bHasBeenUncompressed;
    CPLErr eUncompressErrRet;
    CPLErr Uncompress();

    int    bHasReadXMPMetadata;

  public:
                 WEBPDataset();
                 ~WEBPDataset();

    virtual CPLErr      IRasterIO( GDALRWFlag, int, int, int, int,
                                   void *, int, int, GDALDataType,
                                   int, int *, int, int, int );

    virtual char  **GetMetadata( const char * pszDomain = "" );

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
    static GDALDataset* CreateCopy( const char * pszFilename,
                                    GDALDataset *poSrcDS,
                                    int bStrict, char ** papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void * pProgressData );
};

/************************************************************************/
/* ==================================================================== */
/*                            WEBPRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class WEBPRasterBand : public GDALPamRasterBand
{
    friend class WEBPDataset;

  public:

                   WEBPRasterBand( WEBPDataset *, int );

    virtual CPLErr IReadBlock( int, int, void * );
    virtual GDALColorInterp GetColorInterpretation();
};

/************************************************************************/
/*                          WEBPRasterBand()                            */
/************************************************************************/

WEBPRasterBand::WEBPRasterBand( WEBPDataset *poDS, int nBand )

{
    this->poDS = poDS;

    eDataType = GDT_Byte;

    nBlockXSize = poDS->nRasterXSize;
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr WEBPRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    WEBPDataset* poGDS = (WEBPDataset*) poDS;

    if( poGDS->Uncompress() != CE_None )
        return CE_Failure;

    int i;
    GByte* pabyUncompressed =
        &poGDS->pabyUncompressed[nBlockYOff * nRasterXSize  * poGDS->nBands + nBand - 1];
    for(i=0;i<nRasterXSize;i++)
        ((GByte*)pImage)[i] = pabyUncompressed[poGDS->nBands * i];

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp WEBPRasterBand::GetColorInterpretation()

{
    if ( nBand == 1 )
        return GCI_RedBand;

    else if( nBand == 2 )
        return GCI_GreenBand;

    else if ( nBand == 3 )
        return GCI_BlueBand;

    else
        return GCI_AlphaBand;
}

/************************************************************************/
/* ==================================================================== */
/*                             WEBPDataset                               */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                            WEBPDataset()                              */
/************************************************************************/

WEBPDataset::WEBPDataset()

{
    fpImage = NULL;
    pabyUncompressed = NULL;
    bHasBeenUncompressed = FALSE;
    eUncompressErrRet = CE_None;
    bHasReadXMPMetadata = FALSE;
}

/************************************************************************/
/*                           ~WEBPDataset()                             */
/************************************************************************/

WEBPDataset::~WEBPDataset()

{
    FlushCache();
    if (fpImage)
        VSIFCloseL(fpImage);
    VSIFree(pabyUncompressed);
}

/************************************************************************/
/*                           GetMetadata()                              */
/************************************************************************/

char  **WEBPDataset::GetMetadata( const char * pszDomain )
{
    if ((pszDomain != NULL && EQUAL(pszDomain, "xml:XMP")) && !bHasReadXMPMetadata)
    {
        bHasReadXMPMetadata = TRUE;

        VSIFSeekL(fpImage, 12, SEEK_SET);

        int bFirst = TRUE;
        while(TRUE)
        {
            char szHeader[5];
            GUInt32 nChunkSize;

            if (VSIFReadL(szHeader, 1, 4, fpImage) != 4 ||
                VSIFReadL(&nChunkSize, 1, 4, fpImage) != 4)
                break;

            szHeader[4] = '\0';
            CPL_LSBPTR32(&nChunkSize);

            if (bFirst)
            {
                if (strcmp(szHeader, "VP8X") != 0 || nChunkSize < 10)
                    break;

                int nFlags;
                if (VSIFReadL(&nFlags, 1, 4, fpImage) != 4)
                    break;
                CPL_LSBPTR32(&nFlags);
                if ((nFlags & 8) == 0)
                    break;

                VSIFSeekL(fpImage, nChunkSize - 4, SEEK_CUR);

                bFirst = FALSE;
            }
            else if (strcmp(szHeader, "META") == 0)
            {
                if (nChunkSize > 1024 * 1024)
                    break;

                char* pszXMP = (char*) VSIMalloc(nChunkSize + 1);
                if (pszXMP == NULL)
                    break;

                if ((GUInt32)VSIFReadL(pszXMP, 1, nChunkSize, fpImage) != nChunkSize)
                {
                    VSIFree(pszXMP);
                    break;
                }
                pszXMP[nChunkSize] = '\0';

                /* Avoid setting the PAM dirty bit just for that */
                int nOldPamFlags = nPamFlags;

                char *apszMDList[2];
                apszMDList[0] = pszXMP;
                apszMDList[1] = NULL;
                SetMetadata(apszMDList, "xml:XMP");

                nPamFlags = nOldPamFlags;

                VSIFree(pszXMP);
                break;
            }
            else
                VSIFSeekL(fpImage, nChunkSize, SEEK_CUR);
        }
    }

    return GDALPamDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                            Uncompress()                              */
/************************************************************************/

CPLErr WEBPDataset::Uncompress()
{
    if (bHasBeenUncompressed)
        return eUncompressErrRet;
    bHasBeenUncompressed = TRUE;
    eUncompressErrRet = CE_Failure;

    VSIFSeekL(fpImage, 0, SEEK_END);
    vsi_l_offset nSize = VSIFTellL(fpImage);
    if (nSize != (vsi_l_offset)(uint32_t)nSize)
        return CE_Failure;
    VSIFSeekL(fpImage, 0, SEEK_SET);
    uint8_t* pabyCompressed = (uint8_t*)VSIMalloc(nSize);
    if (pabyCompressed == NULL)
        return CE_Failure;
    VSIFReadL(pabyCompressed, 1, nSize, fpImage);
    uint8_t* pRet;

    if (nBands == 4)
        pRet = WebPDecodeRGBAInto(pabyCompressed, (uint32_t)nSize,
                        (uint8_t*)pabyUncompressed, nRasterXSize * nRasterYSize * nBands,
                        nRasterXSize * nBands);
    else
        pRet = WebPDecodeRGBInto(pabyCompressed, (uint32_t)nSize,
                        (uint8_t*)pabyUncompressed, nRasterXSize * nRasterYSize * nBands,
                        nRasterXSize * nBands);
    VSIFree(pabyCompressed);
    if (pRet == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "WebPDecodeRGBInto() failed");
        return CE_Failure;
    }
    eUncompressErrRet = CE_None;

    return CE_None;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr WEBPDataset::IRasterIO( GDALRWFlag eRWFlag,
                              int nXOff, int nYOff, int nXSize, int nYSize,
                              void *pData, int nBufXSize, int nBufYSize,
                              GDALDataType eBufType,
                              int nBandCount, int *panBandMap,
                              int nPixelSpace, int nLineSpace, int nBandSpace )

{
    if((eRWFlag == GF_Read) &&
       (nBandCount == nBands) &&
       (nXOff == 0) && (nXOff == 0) &&
       (nXSize == nBufXSize) && (nXSize == nRasterXSize) &&
       (nYSize == nBufYSize) && (nYSize == nRasterYSize) &&
       (eBufType == GDT_Byte) && 
       (nPixelSpace == nBands) &&
       (nLineSpace == (nPixelSpace*nXSize)) &&
       (nBandSpace == 1) &&
       (pData != NULL) &&
       (panBandMap != NULL) &&
       (panBandMap[0] == 1) && (panBandMap[1] == 2) && (panBandMap[2] == 3) && (nBands == 3 || panBandMap[3] == 4))
    {
        Uncompress();
        memcpy(pData, pabyUncompressed, nBands * nXSize * nYSize);
        return CE_None;
    }

    return GDALPamDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize, eBufType,
                                     nBandCount, panBandMap,
                                     nPixelSpace, nLineSpace, nBandSpace);
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int WEBPDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    GByte  *pabyHeader = NULL;
    int    nHeaderBytes = poOpenInfo->nHeaderBytes;

    pabyHeader = poOpenInfo->pabyHeader;

    if( nHeaderBytes < 20 )
        return FALSE;

    return memcmp(pabyHeader, "RIFF", 4) == 0 &&
           memcmp(pabyHeader + 8, "WEBP", 4) == 0 &&
           (memcmp(pabyHeader + 12, "VP8 ", 4) == 0 ||
            memcmp(pabyHeader + 12, "VP8L", 4) == 0 ||
            memcmp(pabyHeader + 12, "VP8X", 4) == 0);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *WEBPDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify( poOpenInfo ) )
        return NULL;

    int nWidth, nHeight;
    if (!WebPGetInfo((const uint8_t*)poOpenInfo->pabyHeader, (uint32_t)poOpenInfo->nHeaderBytes,
                     &nWidth, &nHeight))
        return NULL;

    int nBands = 3;

#if WEBP_DECODER_ABI_VERSION >= 0x0002
     WebPDecoderConfig config;
     if (!WebPInitDecoderConfig(&config))
         return NULL;

     int bOK = WebPGetFeatures(poOpenInfo->pabyHeader, poOpenInfo->nHeaderBytes, &config.input) == VP8_STATUS_OK;

    if (config.input.has_alpha)
        nBands = 4;

     WebPFreeDecBuffer(&config.output);

    if (!bOK)
        return NULL;

#endif

    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The WEBP driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Open the file using the large file api.                         */
/* -------------------------------------------------------------------- */
    VSILFILE* fpImage = VSIFOpenL( poOpenInfo->pszFilename, "rb" );

    if( fpImage == NULL )
        return NULL;

    GByte* pabyUncompressed = (GByte*)VSIMalloc3(nWidth, nHeight, nBands);
    if (pabyUncompressed == NULL)
    {
        VSIFCloseL(fpImage);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    WEBPDataset  *poDS;

    poDS = new WEBPDataset();
    poDS->nRasterXSize = nWidth;
    poDS->nRasterYSize = nHeight;
    poDS->fpImage = fpImage;
    poDS->pabyUncompressed = pabyUncompressed;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < nBands; iBand++ )
        poDS->SetBand( iBand+1, new WEBPRasterBand( poDS, iBand+1 ) );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );

    poDS->TryLoadXML( poOpenInfo->papszSiblingFiles );

/* -------------------------------------------------------------------- */
/*      Open overviews.                                                 */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename, poOpenInfo->papszSiblingFiles );

    return poDS;
}

/************************************************************************/
/*                              WebPUserData                            */
/************************************************************************/

typedef struct
{
    VSILFILE         *fp;
    GDALProgressFunc  pfnProgress;
    void             *pProgressData;
} WebPUserData;

/************************************************************************/
/*                         WEBPDatasetWriter()                          */
/************************************************************************/

static
int WEBPDatasetWriter(const uint8_t* data, size_t data_size,
                      const WebPPicture* const picture)
{
    WebPUserData* pUserData = (WebPUserData*)picture->custom_ptr;
    return VSIFWriteL(data, 1, data_size, pUserData->fp) == data_size;
}

/************************************************************************/
/*                        WEBPDatasetProgressHook()                     */
/************************************************************************/

static
int WEBPDatasetProgressHook(int percent, const WebPPicture* const picture)
{
    WebPUserData* pUserData = (WebPUserData*)picture->custom_ptr;
    return pUserData->pfnProgress( percent / 100.0, NULL, pUserData->pProgressData );
}

/************************************************************************/
/*                              CreateCopy()                            */
/************************************************************************/

GDALDataset *
WEBPDataset::CreateCopy( const char * pszFilename, GDALDataset *poSrcDS,
                        int bStrict, char ** papszOptions,
                        GDALProgressFunc pfnProgress, void * pProgressData )

{
    int  nBands = poSrcDS->GetRasterCount();
    int  nXSize = poSrcDS->GetRasterXSize();
    int  nYSize = poSrcDS->GetRasterYSize();

/* -------------------------------------------------------------------- */
/*      WEBP library initialization                                     */
/* -------------------------------------------------------------------- */

    WebPPicture sPicture;
    if (!WebPPictureInit(&sPicture))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "WebPPictureInit() failed");
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Some some rudimentary checks                                    */
/* -------------------------------------------------------------------- */

    if( nXSize > 16383 || nYSize > 16383 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "WEBP maximum image dimensions are 16383 x 16383.");

        return NULL;
    }

    if( nBands != 3
#if WEBP_ENCODER_ABI_VERSION >= 0x0100
        && nBands != 4
#endif
        )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "WEBP driver doesn't support %d bands. Must be 3 (RGB) "
#if WEBP_ENCODER_ABI_VERSION >= 0x0100
                  "or 4 (RGBA) "
#endif
                  "bands.",
                  nBands );

        return NULL;
    }

    GDALDataType eDT = poSrcDS->GetRasterBand(1)->GetRasterDataType();

    if( eDT != GDT_Byte )
    {
        CPLError( (bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported,
                  "WEBP driver doesn't support data type %s. "
                  "Only eight bit byte bands supported.",
                  GDALGetDataTypeName(
                      poSrcDS->GetRasterBand(1)->GetRasterDataType()) );

        if (bStrict)
            return NULL;
    }

/* -------------------------------------------------------------------- */
/*      What options has the user selected?                             */
/* -------------------------------------------------------------------- */
    float fQuality = 75.0f;
    const char* pszQUALITY = CSLFetchNameValue(papszOptions, "QUALITY"); 
    if( pszQUALITY != NULL )
    {
        fQuality = (float) atof(pszQUALITY);
        if( fQuality < 0.0f || fQuality > 100.0f )
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                      "%s=%s is not a legal value.", "QUALITY", pszQUALITY);
            return NULL;
        }
    }

    WebPPreset nPreset = WEBP_PRESET_DEFAULT;
    const char* pszPRESET = CSLFetchNameValueDef(papszOptions, "PRESET", "DEFAULT");
    if (EQUAL(pszPRESET, "DEFAULT"))
        nPreset = WEBP_PRESET_DEFAULT;
    else if (EQUAL(pszPRESET, "PICTURE"))
        nPreset = WEBP_PRESET_PICTURE;
    else if (EQUAL(pszPRESET, "PHOTO"))
        nPreset = WEBP_PRESET_PHOTO;
    else if (EQUAL(pszPRESET, "PICTURE"))
        nPreset = WEBP_PRESET_PICTURE;
    else if (EQUAL(pszPRESET, "DRAWING"))
        nPreset = WEBP_PRESET_DRAWING;
    else if (EQUAL(pszPRESET, "ICON"))
        nPreset = WEBP_PRESET_ICON;
    else if (EQUAL(pszPRESET, "TEXT"))
        nPreset = WEBP_PRESET_TEXT;
    else
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "%s=%s is not a legal value.", "PRESET", pszPRESET );
        return NULL;
    }

    WebPConfig sConfig;
    if (!WebPConfigInitInternal(&sConfig, nPreset, fQuality, WEBP_ENCODER_ABI_VERSION))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "WebPConfigInit() failed");
        return NULL;
    }


#define FETCH_AND_SET_OPTION_INT(name, fieldname, minval, maxval) \
{ \
    const char* pszVal = CSLFetchNameValue(papszOptions, name); \
    if (pszVal != NULL) \
    { \
        sConfig.fieldname = atoi(pszVal); \
        if (sConfig.fieldname < minval || sConfig.fieldname > maxval) \
        { \
            CPLError( CE_Failure, CPLE_IllegalArg, \
                      "%s=%s is not a legal value.", name, pszVal ); \
            return NULL; \
        } \
    } \
}

    FETCH_AND_SET_OPTION_INT("TARGETSIZE", target_size, 0, INT_MAX);

    const char* pszPSNR = CSLFetchNameValue(papszOptions, "PSNR");
    if (pszPSNR)
    {
        sConfig.target_PSNR = atof(pszPSNR);
        if (sConfig.target_PSNR < 0)
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                      "PSNR=%s is not a legal value.", pszPSNR );
            return NULL;
        }
    }

    FETCH_AND_SET_OPTION_INT("METHOD", method, 0, 6);
    FETCH_AND_SET_OPTION_INT("SEGMENTS", segments, 1, 4);
    FETCH_AND_SET_OPTION_INT("SNS_STRENGTH", sns_strength, 0, 100);
    FETCH_AND_SET_OPTION_INT("FILTER_STRENGTH", filter_strength, 0, 100);
    FETCH_AND_SET_OPTION_INT("FILTER_SHARPNESS", filter_sharpness, 0, 7);
    FETCH_AND_SET_OPTION_INT("FILTER_TYPE", filter_type, 0, 1);
    FETCH_AND_SET_OPTION_INT("AUTOFILTER", autofilter, 0, 1);
    FETCH_AND_SET_OPTION_INT("PASS", pass, 1, 10);
    FETCH_AND_SET_OPTION_INT("PREPROCESSING", preprocessing, 0, 1);
    FETCH_AND_SET_OPTION_INT("PARTITIONS", partitions, 0, 3);
#if WEBP_ENCODER_ABI_VERSION >= 0x0002
    FETCH_AND_SET_OPTION_INT("PARTITION_LIMIT", partition_limit, 0, 100);
#endif
#if WEBP_ENCODER_ABI_VERSION >= 0x0100
    sConfig.lossless = CSLFetchBoolean(papszOptions, "LOSSLESS", FALSE);
    if (sConfig.lossless)
        sPicture.use_argb = 1;
#endif

    if (!WebPValidateConfig(&sConfig))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "WebPValidateConfig() failed");
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Allocate memory                                                 */
/* -------------------------------------------------------------------- */
    GByte   *pabyBuffer;

    pabyBuffer = (GByte *) VSIMalloc( nBands * nXSize * nYSize );
    if (pabyBuffer == NULL)
    {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
    VSILFILE    *fpImage;

    fpImage = VSIFOpenL( pszFilename, "wb" );
    if( fpImage == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to create WEBP file %s.\n",
                  pszFilename );
        VSIFree(pabyBuffer);
        return NULL;
    }

    WebPUserData sUserData;
    sUserData.fp = fpImage;
    sUserData.pfnProgress = pfnProgress ? pfnProgress : GDALDummyProgress;
    sUserData.pProgressData = pProgressData;

/* -------------------------------------------------------------------- */
/*      WEBP library settings                                           */
/* -------------------------------------------------------------------- */

    sPicture.width = nXSize;
    sPicture.height = nYSize;
    sPicture.writer = WEBPDatasetWriter;
    sPicture.custom_ptr = &sUserData;
#if WEBP_ENCODER_ABI_VERSION >= 0x0100
    sPicture.progress_hook = WEBPDatasetProgressHook;
#endif
    if (!WebPPictureAlloc(&sPicture))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "WebPPictureAlloc() failed");
        VSIFree(pabyBuffer);
        VSIFCloseL( fpImage );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Acquire source imagery.                                         */
/* -------------------------------------------------------------------- */
    CPLErr      eErr = CE_None;

    eErr = poSrcDS->RasterIO( GF_Read, 0, 0, nXSize, nYSize,
                              pabyBuffer, nXSize, nYSize, GDT_Byte,
                              nBands, NULL,
                              nBands, nBands * nXSize, 1 );

/* -------------------------------------------------------------------- */
/*      Import and write to file                                        */
/* -------------------------------------------------------------------- */
#if WEBP_ENCODER_ABI_VERSION >= 0x0100
    if (eErr == CE_None && nBands == 4)
    {
        if (!WebPPictureImportRGBA(&sPicture, pabyBuffer, nBands * nXSize))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "WebPPictureImportRGBA() failed");
            eErr = CE_Failure;
        }
    }
    else
#endif
    if (eErr == CE_None &&
        !WebPPictureImportRGB(&sPicture, pabyBuffer, nBands * nXSize))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "WebPPictureImportRGB() failed");
        eErr = CE_Failure;
    }

    if (eErr == CE_None && !WebPEncode(&sConfig, &sPicture))
    {
        const char* pszErrorMsg = NULL;
#if WEBP_ENCODER_ABI_VERSION >= 0x0100
        switch(sPicture.error_code)
        {
            case VP8_ENC_ERROR_OUT_OF_MEMORY: pszErrorMsg = "Out of memory"; break;
            case VP8_ENC_ERROR_BITSTREAM_OUT_OF_MEMORY: pszErrorMsg = "Out of memory while flushing bits"; break;
            case VP8_ENC_ERROR_NULL_PARAMETER: pszErrorMsg = "A pointer parameter is NULL"; break;
            case VP8_ENC_ERROR_INVALID_CONFIGURATION: pszErrorMsg = "Configuration is invalid"; break;
            case VP8_ENC_ERROR_BAD_DIMENSION: pszErrorMsg = "Picture has invalid width/height"; break;
            case VP8_ENC_ERROR_PARTITION0_OVERFLOW: pszErrorMsg = "Partition is bigger than 512k. Try using less SEGMENTS, or increase PARTITION_LIMIT value"; break;
            case VP8_ENC_ERROR_PARTITION_OVERFLOW: pszErrorMsg = "Partition is bigger than 16M"; break;
            case VP8_ENC_ERROR_BAD_WRITE: pszErrorMsg = "Error while flusing bytes"; break;
            case VP8_ENC_ERROR_FILE_TOO_BIG: pszErrorMsg = "File is bigger than 4G"; break;
            case VP8_ENC_ERROR_USER_ABORT: pszErrorMsg = "User interrupted"; break;
            default: break;
        }
#endif
        if (pszErrorMsg)
            CPLError(CE_Failure, CPLE_AppDefined, "WebPEncode() failed : %s", pszErrorMsg);
        else
            CPLError(CE_Failure, CPLE_AppDefined, "WebPEncode() failed");
        eErr = CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup and close.                                              */
/* -------------------------------------------------------------------- */
    CPLFree( pabyBuffer );

    WebPPictureFree(&sPicture);

    VSIFCloseL( fpImage );

    if( eErr != CE_None )
    {
        VSIUnlink( pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Re-open dataset, and copy any auxilary pam information.         */
/* -------------------------------------------------------------------- */
    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);

    /* If outputing to stdout, we can't reopen it, so we'll return */
    /* a fake dataset to make the caller happy */
    CPLPushErrorHandler(CPLQuietErrorHandler);
    WEBPDataset *poDS = (WEBPDataset*) WEBPDataset::Open( &oOpenInfo );
    CPLPopErrorHandler();
    if( poDS )
    {
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );
        return poDS;
    }

    return NULL;
}

/************************************************************************/
/*                         GDALRegister_WEBP()                          */
/************************************************************************/

void GDALRegister_WEBP()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "WEBP" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "WEBP" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "WEBP" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "frmt_webp.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "webp" );
        poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/webp" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                                   "Byte" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>\n"
"   <Option name='QUALITY' type='float' description='good=100, bad=0' default='75'/>\n"
#if WEBP_ENCODER_ABI_VERSION >= 0x0100
"   <Option name='LOSSLESS' type='boolean' description='Whether lossless compression should be used' default='FALSE'/>\n"
#endif
"   <Option name='PRESET' type='string-select' description='kind of image' default='DEFAULT'>\n"
"       <Value>DEFAULT</Value>\n"
"       <Value>PICTURE</Value>\n"
"       <Value>PHOTO</Value>\n"
"       <Value>DRAWING</Value>\n"
"       <Value>ICON</Value>\n"
"       <Value>TEXT</Value>\n"
"   </Option>\n"
"   <Option name='TARGETSIZE' type='int' description='if non-zero, desired target size in bytes. Has precedence over QUALITY'/>\n"
"   <Option name='PSNR' type='float' description='if non-zero, minimal distortion to to achieve. Has precedence over TARGETSIZE'/>\n"
"   <Option name='METHOD' type='int' description='quality/speed trade-off. fast=0, slower-better=6' default='4'/>\n"
"   <Option name='SEGMENTS' type='int' description='maximum number of segments [1-4]' default='4'/>\n"
"   <Option name='SNS_STRENGTH' type='int' description='Spatial Noise Shaping. off=0, maximum=100' default='50'/>\n"
"   <Option name='FILTER_STRENGTH' type='int' description='Filter strength. off=0, strongest=100' default='20'/>\n"
"   <Option name='FILTER_SHARPNESS' type='int' description='Filter sharpness. off=0, least sharp=7' default='0'/>\n"
"   <Option name='FILTER_TYPE' type='int' description='Filtering type. simple=0, strong=1' default='0'/>\n"
"   <Option name='AUTOFILTER' type='int' description=\"Auto adjust filter's strength. off=0, on=1\" default='0'/>\n"
"   <Option name='PASS' type='int' description='Number of entropy analysis passes [1-10]' default='1'/>\n"
"   <Option name='PREPROCESSING' type='int' description='Preprocessing filter. none=0, segment-smooth=1' default='0'/>\n"
"   <Option name='PARTITIONS' type='int' description='log2(number of token partitions) in [0..3]' default='0'/>\n"
#if WEBP_ENCODER_ABI_VERSION >= 0x0002
"   <Option name='PARTITION_LIMIT' type='int' description='quality degradation allowed to fit the 512k limit on prediction modes coding (0=no degradation, 100=full)' default='0'/>\n"
#endif
"</CreationOptionList>\n" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnIdentify = WEBPDataset::Identify;
        poDriver->pfnOpen = WEBPDataset::Open;
        poDriver->pfnCreateCopy = WEBPDataset::CreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
