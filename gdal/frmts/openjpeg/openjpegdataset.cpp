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

/* Necessary for opj_setup_decoder() */
#define USE_OPJ_DEPRECATED
#include <openjpeg.h>

#include "gdal_pam.h"
#include "cpl_string.h"
#include "gdaljp2metadata.h"

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
    CPLError(CE_Warning, CPLE_AppDefined, "%s", pszMsg);
}

/************************************************************************/
/*                 JP2OpenJPEGDataset_InfoCallback()                    */
/************************************************************************/

static void JP2OpenJPEGDataset_InfoCallback(const char *pszMsg, void *unused)
{
    CPLDebug("OPENJPEG", "info: %s", pszMsg);
}

/************************************************************************/
/*                      JP2OpenJPEGDataset_Read()                       */
/************************************************************************/

static OPJ_UINT32 JP2OpenJPEGDataset_Read(void* pBuffer, OPJ_UINT32 nBytes,
                                       void *pUserData)
{
    int nRet = VSIFReadL(pBuffer, 1, nBytes, (FILE*)pUserData);
#ifdef DEBUG
    CPLDebug("OPENJPEG", "JP2OpenJPEGDataset_Read(%d) = %d", nBytes, nRet);
#endif
    if (nRet == 0)
        nRet = -1;
    return nRet;
}

/************************************************************************/
/*                      JP2OpenJPEGDataset_Write()                      */
/************************************************************************/

static OPJ_UINT32 JP2OpenJPEGDataset_Write(void* pBuffer, OPJ_UINT32 nBytes,
                                       void *pUserData)
{
    int nRet = VSIFWriteL(pBuffer, 1, nBytes, (FILE*)pUserData);
#ifdef DEBUG
    CPLDebug("OPENJPEG", "JP2OpenJPEGDataset_Write(%d) = %d", nBytes, nRet);
#endif
    return nRet;
}

/************************************************************************/
/*                       JP2OpenJPEGDataset_Seek()                      */
/************************************************************************/

static bool JP2OpenJPEGDataset_Seek(OPJ_SIZE_T nBytes, void * pUserData)
{
#ifdef DEBUG
    CPLDebug("OPENJPEG", "JP2OpenJPEGDataset_Seek(%d)", nBytes);
#endif
    return VSIFSeekL((FILE*)pUserData, nBytes, SEEK_SET) == 0;
}

/************************************************************************/
/*                     JP2OpenJPEGDataset_Skip()                        */
/************************************************************************/

static OPJ_SIZE_T JP2OpenJPEGDataset_Skip(OPJ_SIZE_T nBytes, void * pUserData)
{
    int nOffset = VSIFTellL((FILE*)pUserData) + nBytes;
#ifdef DEBUG
    CPLDebug("OPENJPEG", "JP2OpenJPEGDataset_Skip(%d -> %d)", nBytes, nOffset);
#endif
    if (nOffset < 0)
        return -1;
    VSIFSeekL((FILE*)pUserData, nOffset, SEEK_SET);
    return nBytes;
}

/************************************************************************/
/* ==================================================================== */
/*                           JP2OpenJPEGDataset                         */
/* ==================================================================== */
/************************************************************************/

class JP2OpenJPEGDataset : public GDALPamDataset
{
    friend class JP2OpenJPEGRasterBand;

    FILE        *fp; /* Large FILE API */

    char        *pszProjection;
    int         bGeoTransformValid;
    double      adfGeoTransform[6];
    int         nGCPCount;
    GDAL_GCP    *pasGCPList;

    OPJ_CODEC_FORMAT eCodecFormat;
    OPJ_COLOR_SPACE eColorSpace;

    int         bLoadingOtherBands;
    int         bIs420;

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
    CPLErr              GetGeoTransform( double* );
    virtual const char  *GetProjectionRef(void);
    virtual int         GetGCPCount();
    virtual const char  *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();
};

/************************************************************************/
/* ==================================================================== */
/*                         JP2OpenJPEGRasterBand                        */
/* ==================================================================== */
/************************************************************************/

class JP2OpenJPEGRasterBand : public GDALPamRasterBand
{
    friend class JP2OpenJPEGDataset;

  public:

                JP2OpenJPEGRasterBand( JP2OpenJPEGDataset * poDS, int nBand,
                                    GDALDataType eDataType,
                                    int nBlockXSize, int nBlockYSize);
                ~JP2OpenJPEGRasterBand();
                
    virtual CPLErr          IReadBlock( int, int, void * );
    virtual GDALColorInterp GetColorInterpretation();
};


/************************************************************************/
/*                        JP2OpenJPEGRasterBand()                       */
/************************************************************************/

JP2OpenJPEGRasterBand::JP2OpenJPEGRasterBand( JP2OpenJPEGDataset *poDS, int nBand,
                                        GDALDataType eDataType,
                                        int nBlockXSize, int nBlockYSize)

{
    this->poDS = poDS;
    this->nBand = nBand;
    this->eDataType = eDataType;
    this->nBlockXSize = nBlockXSize;
    this->nBlockYSize = nBlockYSize;
}

/************************************************************************/
/*                      ~JP2OpenJPEGRasterBand()                        */
/************************************************************************/

JP2OpenJPEGRasterBand::~JP2OpenJPEGRasterBand()
{
}

/************************************************************************/
/*                            CopySrcToDst()                            */
/************************************************************************/

static CPL_INLINE GByte CLAMP_0_255(int val)
{
    if (val < 0)
        return 0;
    else if (val > 255)
        return 255;
    else
        return val;
}

static void CopySrcToDst(int nWidthToRead, int nHeightToRead,
                         GByte* pTempBuffer,
                         int nBlockXSize, int nBlockYSize, int nDataTypeSize,
                         void* pImage, int nBand, int bIs420)
{
    int i, j;
    if (bIs420)
    {
        GByte* pSrc = (GByte*)pTempBuffer;
        GByte* pDst = (GByte*)pImage;
        for(j=0;j<nHeightToRead;j++)
        {
            for(i=0;i<nWidthToRead;i++)
            {
                int Y = pSrc[j * nWidthToRead + i];
                int Cb = pSrc[nHeightToRead * nWidthToRead + ((j/2) * (nWidthToRead/2) + i/2) ];
                int Cr = pSrc[5 * nHeightToRead * nWidthToRead / 4 + ((j/2) * (nWidthToRead/2) + i/2) ];
                if (nBand == 1)
                    pDst[j * nBlockXSize + i] = CLAMP_0_255(Y + 1.402 * (Cr - 128));
                else if (nBand == 2)
                    pDst[j * nBlockXSize + i] = CLAMP_0_255(Y - 0.34414 * (Cb - 128) - 0.71414 * (Cr - 128));
                else
                    pDst[j * nBlockXSize + i] = CLAMP_0_255(Y + 1.772 * (Cb - 128));
            }
        }
    }
    else
    {
        for(j=0;j<nHeightToRead;j++)
        {
            memcpy(((GByte*)pImage) + j*nBlockXSize * nDataTypeSize,
                    pTempBuffer + (j*nWidthToRead + (nBand-1) * nHeightToRead * nWidthToRead) * nDataTypeSize,
                    nWidthToRead * nDataTypeSize);
        }
    }
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr JP2OpenJPEGRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                      void * pImage )
{
    JP2OpenJPEGDataset *poGDS = (JP2OpenJPEGDataset *) poDS;
    opj_codec_t* pCodec = NULL;
    int nDataTypeSize = (GDALGetDataTypeSize(eDataType) / 8);

    CPLDebug("OPENJPEG", "xoff=%d yoff=%d band=%d",
             nBlockXOff, nBlockYOff, nBand);

    int nWidthToRead = MIN(nBlockXSize, poGDS->nRasterXSize - nBlockXOff * nBlockXSize);
    int nHeightToRead = MIN(nBlockYSize, poGDS->nRasterYSize - nBlockYOff * nBlockYSize);
    if (nWidthToRead != nBlockXSize || nHeightToRead != nBlockYSize)
    {
        memset(pImage, 0, nBlockXSize * nBlockYSize * nDataTypeSize);
    }

    /* FIXME ? Well, this is pretty inefficient as for each block we recreate */
    /* a new decoding session. But currently there's no way to call opj_set_decode_area() */
    /* twice on the same codec instance... */

    pCodec = opj_create_decompress(poGDS->eCodecFormat);

    opj_set_info_handler(pCodec, JP2OpenJPEGDataset_InfoCallback,NULL);
    opj_set_warning_handler(pCodec, JP2OpenJPEGDataset_WarningCallback,NULL);
    opj_set_error_handler(pCodec, JP2OpenJPEGDataset_ErrorCallback,NULL);

    opj_dparameters_t parameters;
    opj_set_default_decoder_parameters(&parameters);

    if (! opj_setup_decoder(pCodec,&parameters))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "opj_setup_decoder() failed");
        opj_destroy_codec(pCodec);
        return CE_Failure;
    }

    /* Reseek to file beginning */
    VSIFSeekL(poGDS->fp, 0, SEEK_SET);

    opj_stream_t * pStream;
    pStream = opj_stream_create(1024, TRUE); // Default 1MB is way too big for some datasets
    opj_stream_set_read_function(pStream, JP2OpenJPEGDataset_Read);
    opj_stream_set_seek_function(pStream, JP2OpenJPEGDataset_Seek);
    opj_stream_set_skip_function(pStream, JP2OpenJPEGDataset_Skip);
    opj_stream_set_user_data(pStream, poGDS->fp);

    opj_image_t * psImage = NULL;
    OPJ_INT32  nX0,nY0;
    OPJ_UINT32 nTileW,nTileH,nTilesX,nTilesY;
    if(!opj_read_header(pCodec, &psImage, &nX0, &nY0, &nTileW, &nTileH,
                        &nTilesX, &nTilesY, pStream))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "opj_read_header() failed");
        opj_destroy_codec(pCodec);
        opj_stream_destroy(pStream);
        return CE_Failure;
    }

    if (!opj_set_decode_area(pCodec,
                            nBlockXOff * nBlockXSize,
                            nBlockYOff * nBlockYSize,
                            nBlockXOff * nBlockXSize + nWidthToRead,
                            nBlockYOff * nBlockYSize + nHeightToRead))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "opj_set_decode_area() failed");
        opj_destroy_codec(pCodec);
        opj_stream_destroy(pStream);
        opj_image_destroy(psImage);
        return CE_Failure;
    }

    bool bDataToUncompress;
    OPJ_UINT32 nTileIndex,nCompCount;
    OPJ_INT32 nTileX0,nTileY0,nTileX1,nTileY1;
    OPJ_UINT32 nRequiredSize;

    int nAllocatedSize;
    if (poGDS->bIs420)
        nAllocatedSize = 3 * nWidthToRead * nHeightToRead * nDataTypeSize / 2;
    else
        nAllocatedSize = poGDS->nBands * nWidthToRead * nHeightToRead * nDataTypeSize;
    OPJ_BYTE *pTempBuffer = (OPJ_BYTE *)VSIMalloc(nAllocatedSize);
    if (pTempBuffer == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot allocate temp buffer");
        opj_destroy_codec(pCodec);
        opj_stream_destroy(pStream);
        opj_image_destroy(psImage);
        return CE_Failure;
    }

    do
    {
        if (!opj_read_tile_header(pCodec, &nTileIndex, &nRequiredSize,
                                  &nTileX0, &nTileY0, &nTileX1, &nTileY1,
                                  &nCompCount, &bDataToUncompress, pStream))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "opj_read_tile_header() failed");
            CPLFree(pTempBuffer);
            opj_destroy_codec(pCodec);
            opj_stream_destroy(pStream);
            opj_image_destroy(psImage);
            return CE_Failure;
        }

        /* A few sanity checks */
        if (nTileX0 != nBlockXOff * nBlockXSize ||
            nTileY0 != nBlockYOff * nBlockYSize ||
            nTileX1 != nBlockXOff * nBlockXSize + nWidthToRead ||
            nTileY1 != nBlockYOff * nBlockYSize + nHeightToRead ||
            (int)nRequiredSize != nAllocatedSize ||
            (int)nCompCount != poGDS->nBands)
        {
            CPLDebug("OPENJPEG",
                     "bDataToUncompress=%d nTileIndex=%d nRequiredSize=%d nCompCount=%d",
                     bDataToUncompress, nTileIndex, nRequiredSize, nCompCount);
            CPLDebug("OPENJPEG",
                     "nTileX0=%d nTileY0=%d nTileX1=%d nTileY1=%d",
                     nTileX0, nTileY0, nTileX1, nTileY1);
            CPLError(CE_Failure, CPLE_AppDefined,
                     "opj_read_tile_header() returned unexpected parameters");
            CPLFree(pTempBuffer);
            opj_destroy_codec(pCodec);
            opj_stream_destroy(pStream);
            opj_image_destroy(psImage);
            return CE_Failure;
        }

        if (bDataToUncompress)
        {
            if (!opj_decode_tile_data(pCodec,nTileIndex,pTempBuffer,
                                      nRequiredSize,pStream))
            {
                CPLError(CE_Failure, CPLE_AppDefined, "opj_decode_tile_data() failed");
                CPLFree(pTempBuffer);
                opj_destroy_codec(pCodec);
                opj_stream_destroy(pStream);
                opj_image_destroy(psImage);
                return CE_Failure;
            }
        }
    } while(bDataToUncompress);

    CopySrcToDst(nWidthToRead, nHeightToRead, pTempBuffer,
                 nBlockXSize, nBlockYSize, nDataTypeSize, pImage,
                 nBand, poGDS->bIs420);

    /* Let's cache other bands */
    if( poGDS->nBands != 1 && !poGDS->bLoadingOtherBands)
    {
        int iOtherBand;

        poGDS->bLoadingOtherBands = TRUE;

        for( iOtherBand = 1; iOtherBand <= poGDS->nBands; iOtherBand++ )
        {
            if( iOtherBand == nBand )
                continue;

            GDALRasterBlock *poBlock;

            poBlock = poGDS->GetRasterBand(iOtherBand)->
                GetLockedBlockRef(nBlockXOff,nBlockYOff, TRUE);
            if (poBlock == NULL)
            {
                break;
            }

            void* pData = poBlock->GetDataRef();
            if (pData)
            {
                CopySrcToDst(nWidthToRead, nHeightToRead, pTempBuffer,
                             nBlockXSize, nBlockYSize, nDataTypeSize, pData,
                             iOtherBand, poGDS->bIs420);
            }

            poBlock->DropLock();
        }

        poGDS->bLoadingOtherBands = FALSE;
    }

    CPLFree(pTempBuffer);

    opj_end_decompress(pCodec,pStream);
    opj_stream_destroy(pStream);
    opj_destroy_codec(pCodec);
    opj_image_destroy(psImage);

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp JP2OpenJPEGRasterBand::GetColorInterpretation()
{
    JP2OpenJPEGDataset *poGDS = (JP2OpenJPEGDataset *) poDS;

    if (poGDS->eColorSpace == CLRSPC_GRAY)
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
    pszProjection = CPLStrdup("");
    nGCPCount = 0;
    pasGCPList = NULL;
    bGeoTransformValid = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
    bLoadingOtherBands = FALSE;
    eCodecFormat = CODEC_UNKNOWN;
    eColorSpace = CLRSPC_UNKNOWN;
    bIs420 = FALSE;
}

/************************************************************************/
/*                         ~JP2OpenJPEGDataset()                        */
/************************************************************************/

JP2OpenJPEGDataset::~JP2OpenJPEGDataset()

{
    FlushCache();

    if ( pszProjection )
        CPLFree( pszProjection );
    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }
    if( fp != NULL )
        VSIFCloseL( fp );
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *JP2OpenJPEGDataset::GetProjectionRef()

{
    return( pszProjection );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr JP2OpenJPEGDataset::GetGeoTransform( double * padfTransform )
{
    if( bGeoTransformValid )
    {
        memcpy( padfTransform, adfGeoTransform, sizeof(adfGeoTransform[0]) * 6 );
        return CE_None;
    }
    else
        return CE_Failure;
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int JP2OpenJPEGDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *JP2OpenJPEGDataset::GetGCPProjection()

{
    if( nGCPCount > 0 )
        return pszProjection;
    else
        return "";
}

/************************************************************************/
/*                               GetGCP()                               */
/************************************************************************/

const GDAL_GCP *JP2OpenJPEGDataset::GetGCPs()

{
    return pasGCPList;
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

    FILE* fp = VSIFOpenL(poOpenInfo->pszFilename, "rb");
    if (!fp)
        return NULL;

    OPJ_CODEC_FORMAT eCodecFormat;

    /* Detect which codec to use : J2K or JP2 ? */
    static const unsigned char jpc_header[] = {0xff,0x4f};
    if (memcmp( poOpenInfo->pabyHeader, jpc_header, 
                    sizeof(jpc_header) ) == 0)
        eCodecFormat = CODEC_J2K;
    else
        eCodecFormat = CODEC_JP2;

    opj_codec_t* pCodec = opj_create_decompress(eCodecFormat);

    opj_set_info_handler(pCodec, JP2OpenJPEGDataset_InfoCallback,NULL);
    opj_set_warning_handler(pCodec, JP2OpenJPEGDataset_WarningCallback,NULL);
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
    opj_stream_set_read_function(pStream, JP2OpenJPEGDataset_Read);
    opj_stream_set_seek_function(pStream, JP2OpenJPEGDataset_Seek);
    opj_stream_set_skip_function(pStream, JP2OpenJPEGDataset_Skip);
    opj_stream_set_user_data(pStream, fp);

    opj_image_t * psImage = NULL;
    OPJ_INT32  nX0,nY0;
    OPJ_UINT32 nTileW,nTileH,nTilesX,nTilesY;
    if(!opj_read_header(pCodec, &psImage, &nX0, &nY0, &nTileW, &nTileH,
                        &nTilesX, &nTilesY, pStream))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "opj_read_header() failed");
        opj_destroy_codec(pCodec);
        opj_stream_destroy(pStream);
        opj_image_destroy(psImage);
        VSIFCloseL(fp);
        return NULL;
    }
    
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
    for(i=0;i<(int)psImage->numcomps;i++)
    {
        CPLDebug("OPENJPEG", "psImage->comps[%d].dx = %d", i, psImage->comps[i].dx);
        CPLDebug("OPENJPEG", "psImage->comps[%d].dy = %d", i, psImage->comps[i].dy);
        CPLDebug("OPENJPEG", "psImage->comps[%d].x0 = %d", i, psImage->comps[i].x0);
        CPLDebug("OPENJPEG", "psImage->comps[%d].y0 = %d", i, psImage->comps[i].y0);
        CPLDebug("OPENJPEG", "psImage->comps[%d].w = %d", i, psImage->comps[i].w);
        CPLDebug("OPENJPEG", "psImage->comps[%d].h = %d", i, psImage->comps[i].h);
        CPLDebug("OPENJPEG", "psImage->comps[%d].factor = %d", i, psImage->comps[i].factor);
        CPLDebug("OPENJPEG", "psImage->comps[%d].prec = %d", i, psImage->comps[i].prec);
        CPLDebug("OPENJPEG", "psImage->comps[%d].sgnd = %d", i, psImage->comps[i].sgnd);
    }
#endif

    if (psImage->x1 - psImage->x0 <= 0 ||
        psImage->y1 - psImage->y0 <= 0 ||
        psImage->numcomps == 0 ||
        (int)psImage->comps[0].w != psImage->x1 - psImage->x0 ||
        (int)psImage->comps[0].h != psImage->y1 - psImage->y0)
    {
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

    int bIs420  =  (psImage->color_space != CLRSPC_SRGB &&
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
            if (psImage->comps[iBand-1].w != psImage->comps[0].w ||
                psImage->comps[iBand-1].h != psImage->comps[0].h ||
                psImage->comps[iBand-1].prec != psImage->comps[0].prec)
            {
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

    opj_end_decompress(pCodec,pStream);
    opj_stream_destroy(pStream);
    opj_destroy_codec(pCodec);
    opj_image_destroy(psImage);

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( iBand = 1; iBand <= poDS->nBands; iBand++ )
    {
        poDS->SetBand( iBand, new JP2OpenJPEGRasterBand( poDS, iBand, eDataType,
                                                      nTileW, nTileH) );
    }

/* -------------------------------------------------------------------- */
/*      More metadata.                                                  */
/* -------------------------------------------------------------------- */
    if( poDS->nBands > 1 )
    {
        poDS->SetMetadataItem( "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE" );
    }

/* -------------------------------------------------------------------- */
/*      Check for georeferencing information.                           */
/* -------------------------------------------------------------------- */
    GDALJP2Metadata oJP2Geo;

    if( oJP2Geo.ReadAndParse( poOpenInfo->pszFilename ) )
    {
        if ( poDS->pszProjection )
            CPLFree( poDS->pszProjection );
        poDS->pszProjection = CPLStrdup(oJP2Geo.pszProjection);
        poDS->bGeoTransformValid = oJP2Geo.bHaveGeoTransform;
        memcpy( poDS->adfGeoTransform, oJP2Geo.adfGeoTransform, 
                sizeof(double) * 6 );
        poDS->nGCPCount = oJP2Geo.nGCPCount;
        poDS->pasGCPList =
            GDALDuplicateGCPs( oJP2Geo.nGCPCount, oJP2Geo.pasGCPList );
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
    OPJ_CODEC_FORMAT eCodecFormat = CODEC_J2K;
    const char* pszCodec = CSLFetchNameValueDef(papszOptions, "CODEC", NULL);
    if (pszCodec)
    {
        if (EQUAL(pszCodec, "JP2"))
            eCodecFormat = CODEC_JP2;
        else if (EQUAL(pszCodec, "J2K"))
            eCodecFormat = CODEC_J2K;
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
            eCodecFormat = CODEC_JP2;
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

    OPJ_PROG_ORDER eProgOrder = LRCP;
    const char* pszPROGORDER =
            CSLFetchNameValueDef(papszOptions, "PROGRESSION", "LRCP");
    if (EQUAL(pszPROGORDER, "LRCP"))
        eProgOrder = LRCP;
    else if (EQUAL(pszPROGORDER, "RLCP"))
        eProgOrder = RLCP;
    else if (EQUAL(pszPROGORDER, "RPCL"))
        eProgOrder = RPCL;
    else if (EQUAL(pszPROGORDER, "PCRL"))
        eProgOrder = PCRL;
    else if (EQUAL(pszPROGORDER, "CPRL"))
        eProgOrder = CPRL;
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
    parameters.tcp_rates[0] = dfRate;
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

    OPJ_COLOR_SPACE eColorSpace = (bResample) ? CLRSPC_SYCC : (nBands == 3) ? CLRSPC_SRGB : CLRSPC_GRAY;
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
    FILE* fp = VSIFOpenL(pszFilename, pszAccess);
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
                            int Y = 0.299 * R + 0.587 * G + 0.114 * B;
                            int Cb = CLAMP_0_255(-0.1687 * R - 0.3313 * G + 0.5 * B  + 128);
                            int Cr = CLAMP_0_255(0.5 * R - 0.4187 * G - 0.0813 * B  + 128);
                            pYUV420Buffer[j*nWidthToRead+i] = Y;
                            pYUV420Buffer[nHeightToRead * nWidthToRead + ((j/2) * ((nWidthToRead)/2) + i/2) ] = Cb;
                            pYUV420Buffer[5 * nHeightToRead * nWidthToRead / 4 + ((j/2) * ((nWidthToRead)/2) + i/2) ] = Cr;
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
"   <Option name='QUALITY' type='float' description='Quality. 0-100' default=25/>"
"   <Option name='REVERSIBLE' type='boolean' description='True if the compression is reversible' default='false'/>"
"   <Option name='RESOLUTIONS' type='int' description='Number of resolutions. 1-7' default=6/>"
"   <Option name='BLOCKXSIZE' type='int' description='Tile Width' default=1024/>"
"   <Option name='BLOCKYSIZE' type='int' description='Tile Height' default=1024/>"
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

