/******************************************************************************
 * Project:  GDAL
 * Author:   Raul Alonso Reyes <raul dot alonsoreyes at satcen dot europa dot eu>
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 * Purpose:  JPEG-2000 driver based on Lurawave library, driver developed by SatCen
 *
 ******************************************************************************
 * Copyright (c) 2016, SatCen - European Union Satellite Centre
 * Copyright (c) 2016, Even Rouault
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

#include "cpl_port.h"

#include "lwf_jp2.h"

#include "jp2luracallbacks.h"

#ifdef ENABLE_MEMORY_REGISTRAR
JP2LuraMemoryRegistrar::JP2LuraMemoryRegistrar()
{
}

JP2LuraMemoryRegistrar::~JP2LuraMemoryRegistrar()
{
    CPLDebug("JP2Lura",
             "JP2LuraMemoryRegistrar: %d block allocated leaked", 
             static_cast<int>(oMap.size()));
    std::map<void*, size_t>::const_iterator oIter = oMap.begin();
    for( ; oIter != oMap.end(); ++oIter )
    {
        CPLDebug("JP2Lura", "force freeing %d bytes", 
                 static_cast<int>(oIter->second));
        VSIFree(oIter->first);
    }
}

void JP2LuraMemoryRegistrar::Register(size_t nSize, void* ptr)
{
    CPLAssert( oMap.find(ptr) == oMap.end() );
    oMap[ptr] = nSize;
}

void JP2LuraMemoryRegistrar::Unregister(void* ptr)
{
    CPLAssert( oMap.find(ptr) != oMap.end() );
    oMap.erase(ptr);
}
#endif // ENABLE_MEMORY_REGISTRAR

/************************************************************************/
/*                    GDALJP2Lura_Callback_Malloc()                     */
/************************************************************************/

void *  JP2_Callback_Conv  GDALJP2Lura_Callback_Malloc(size_t size,
                                            JP2_Callback_Param 
#ifdef ENABLE_MEMORY_REGISTRAR
                                                                lParam
#endif
                                                       )
{
    void* ptr = VSIMalloc(size);
#ifdef ENABLE_MEMORY_REGISTRAR
    if( lParam && ptr )
    {
        ((JP2LuraMemoryRegistrar*)lParam)->Register(size, ptr);
    }
#endif
    return ptr;
}


/************************************************************************/
/*                    GDALJP2Lura_Callback_Free()                       */
/************************************************************************/

JP2_Error  JP2_Callback_Conv  GDALJP2Lura_Callback_Free(void *ptr,
                                            JP2_Callback_Param 
#ifdef ENABLE_MEMORY_REGISTRAR
                                                                lParam
#endif
                                                       )
{
#ifdef ENABLE_MEMORY_REGISTRAR
    if( lParam && ptr )
    {
        ((JP2LuraMemoryRegistrar*)lParam)->Unregister(ptr);
    }
#endif
    VSIFree(ptr);
    return cJP2_Error_OK;
}


/************************************************************************/
/*                  GDALJP2Lura_Callback_Decompress_Read()              */
/************************************************************************/

unsigned long  JP2_Callback_Conv  GDALJP2Lura_Callback_Decompress_Read(
                                                    unsigned char  *pucData,
                                                    unsigned long   ulPos,
                                                    unsigned long   ulSize,
                                                    JP2_Callback_Param lParam)
{
    VSILFILE* fp = reinterpret_cast<VSILFILE*>(lParam);

    if (VSIFSeekL(fp, ulPos, SEEK_SET) != 0)
    {
        return 0;
    }

    return static_cast<unsigned long>(
            VSIFReadL(pucData, 1, static_cast<size_t>(ulSize), fp));
}

/************************************************************************/
/*                          splitIEEE754Float()                         */
/************************************************************************/

typedef union {
    float f;
    unsigned int ui;
} float_uint_union;

static void splitIEEE754Float(float f, unsigned int *mantissa, int *exponent,
                              int *sign)
{
    float_uint_union x;
    x.f = f;

    if (x.ui & 0x80000000)
        *sign = 1;
    else
        *sign = 0;

    *mantissa = x.ui & 0x07FFFFF;

    *exponent = (x.ui >> 23) & 0xFF;
}

/************************************************************************/
/*                           setIIIE754Sign()                           */
/************************************************************************/

static void setIIIE754Sign(float_uint_union* f, unsigned char sign)
{
    if (!sign)
        f->ui = (f->ui & 0x7FFFFFFFU);
    else
        f->ui = (f->ui & 0x7FFFFFFFU) | 0x80000000U;
}

/************************************************************************/
/*                         setIIIE754Exponent()                         */
/************************************************************************/

static void setIIIE754Exponent(float_uint_union* f, unsigned char exponent)
{
    f->ui = (f->ui & 0x807fffffU) | (exponent << 23);
}

/************************************************************************/
/*                         setIIIE754Mantissa()                         */
/************************************************************************/

static void setIIIE754Mantissa(float_uint_union* f, unsigned int mantissa)
{
    f->ui = (f->ui & (0xFF800000U)) | mantissa;
}

/************************************************************************/
/*                 GDALJP2Lura_Callback_Decompress_Write()              */
/************************************************************************/

JP2_Error  JP2_Callback_Conv  GDALJP2Lura_Callback_Decompress_Write(
                                    unsigned char*  pucData,
                                    short           sComponent,
                                    unsigned long   ulRow,
                                    unsigned long   ulStart, // starting pixel
                                    unsigned long   ulNum, // number of pixels
                                    JP2_Callback_Param   lParam)
{
#ifdef DEBUG_VERBOSE
    CPLDebug("JP2Lura", "Decompress(%d, %lu, %lu, %lu)",
             sComponent, ulRow, ulStart, ulNum);
#endif

    GDALJP2Lura_Output_Data* pOutputData =
                    reinterpret_cast<GDALJP2Lura_Output_Data*>(lParam);

    CPLAssert(ulRow < static_cast<unsigned long>(pOutputData->nBufYSize));
    CPLAssert(ulStart + ulNum <=
                        static_cast<unsigned long>(pOutputData->nBufXSize));

    long lBps = 0;
    /****************************************************/
    /*  convert from component index to channel index   */
    /*  i.e. index after expanding any palette samples  */
    /****************************************************/
    if (pOutputData->lBps==0) //float
    {
        switch (sComponent)
        {
            case 0:
            case 1:
            case 2:
            {
                lBps = 32;
                break;
            }

            default:
                return cJP2_Error_Write_Callback_Undefined;
        }
    }
    else
    {
        lBps = pOutputData->lBps;
    }

    unsigned char* pucImageData;  // buffer for decompressed image stripe

    if (pOutputData->lBps == 0)
        pucImageData = pOutputData->pimage;
    else
    {
        if( sComponent >= pOutputData->nBands )
        {
            // Ignored component
            return cJP2_Error_OK;
        }
        if (sComponent != pOutputData->nBand - 1)
        {
            pucImageData = pOutputData->pDatacache[sComponent];

        }
        else
        {
            pucImageData = pOutputData->pimage;
        }
    }


    /***********************************/
    /* number of bytes for each sample */
    /***********************************/

    unsigned long ulBytesFromLura = ((lBps + 7) >> 3);
    unsigned long ulBytesrequest =
                            GDALGetDataTypeSizeBytes(pOutputData->eBufType);

    /* distance between samples of the same channel */

    unsigned long ulSkip = ulBytesrequest;

    unsigned long ulOffset = (pOutputData->nBufXSize * ulSkip) * ulRow +
                ulStart * ulSkip;

    unsigned char* pucStart = pucImageData + ulOffset;
    if (pOutputData->lBps == 0)
    {
#ifdef DEBUG_VERBOSE
        CPLString osLineValues;
#endif
        const unsigned int nSpaceMantissa = 4;
        for (unsigned long i = 0; i < ulNum; i++)
        {
            float_uint_union* f = (float_uint_union*)(&pucStart[i*4]);
            if (sComponent == 0)
            {
#ifdef DEBUG_VERBOSE
                osLineValues += CPLSPrintf("%02X ", *pucData);
#endif
                setIIIE754Sign(f, (*pucData == 0) ? 0 : 1); 
                pucData++;
            }
            else if (sComponent == 1)
            {
#ifdef DEBUG_VERBOSE
                osLineValues += CPLSPrintf("%02X ", *pucData);
#endif
                setIIIE754Exponent(f, *pucData);
                pucData++;
            }
            else if (sComponent == 2)
            {
                unsigned int mantissa = *(unsigned int*)pucData;
#ifdef DEBUG_VERBOSE
                osLineValues += CPLSPrintf("%02X ",mantissa);
#endif
                setIIIE754Mantissa(f, mantissa);
                pucData += nSpaceMantissa;
            }
        }
#ifdef DEBUG_VERBOSE
        CPLDebug("JP2Lura", "Component %d: %s", sComponent,
                 osLineValues.c_str());
#endif
    }
    else
    {
        memcpy(pucStart, pucData, ulBytesFromLura * ulNum);
    }


    return cJP2_Error_OK;
}


/************************************************************************/
/*                 GDALJP2Lura_Callback_Compress_Write()                */
/************************************************************************/

JP2_Error  JP2_Callback_Conv  GDALJP2Lura_Callback_Compress_Write(
                                        unsigned char           *pucData,
                                        unsigned long           ulPos,
                                        unsigned long           ulSize,
                                        JP2_Callback_Param      lParam)
{
    JP2_Gdal_Stream_Data* data =
                            reinterpret_cast<JP2_Gdal_Stream_Data*>(lParam);

    if (VSIFSeekL(data->fp, (vsi_l_offset)(ulPos + data->Position),
                  SEEK_SET) != 0)
    {
        return cJP2_Error_Failure_Write;
    }
    if (VSIFWriteL(pucData, 1, (vsi_l_offset)ulSize, data->fp) != ulSize)
    {
        return cJP2_Error_Failure_Write;
    }

    return cJP2_Error_OK;
}

/************************************************************************/
/*                 GDALJP2Lura_Callback_Compress_Read()                 */
/************************************************************************/

JP2_Error  JP2_Callback_Conv  GDALJP2Lura_Callback_Compress_Read(
                                            unsigned char*      pucData,
                                            short               sComponent,
                                            unsigned long       ulRow,
                                            unsigned long       ulStart,
                                            unsigned long       ulNum,
                                            JP2_Callback_Param  lParam)
{
    GDALJP2Lura_Input_Data* idata =
                            reinterpret_cast<GDALJP2Lura_Input_Data*>(lParam);
    GDALDataset* poSrcDS = idata->poSrcDS;
    const int  nBands = poSrcDS->GetRasterCount();
    const int  nYSize = poSrcDS->GetRasterYSize();

    GDALProgressFunc pfnProgress = idata->pfnProgress;
    void * pProgressData = idata->pProgressData;

    if( ulStart == 0 && pfnProgress &&
        !pfnProgress(static_cast<double>(ulRow+1)/nYSize, "", pProgressData) )
    {
        return cJP2_Error_Read_Callback_Undefined;
    }

    GDALRasterBand  *poBand;
    if (nBands == 1 &&
        poSrcDS->GetRasterBand(1)->GetRasterDataType() == GDT_Float32)
    {
        poBand = poSrcDS->GetRasterBand(1);
    }
    else
    {
        poBand = poSrcDS->GetRasterBand(sComponent + 1);
    }
    GDALDataType eDataType = poBand->GetRasterDataType();

    unsigned long ulBpsRead = 0;
    switch (eDataType)
    {
        case GDT_Byte:
        {
            ulBpsRead = 8;
            //Signed = 0;
            break;
        }
        case GDT_UInt16:
        {
            ulBpsRead = 16;
            //Signed = 0;
            break;
        }
        case GDT_Int16:
        {
            ulBpsRead = 16;
            //Signed = 1;
            break;
        }
        case GDT_UInt32:
        {
            ulBpsRead = 32;
            //Signed = 0;
            break;
        }
        case GDT_Int32:
        {
            ulBpsRead = 32;
            //Signed = 1;
            break;
        }
        case GDT_Float32:
        {
            ulBpsRead = 32;
            //Signed = 1;
            break;
        }

        default:
            break;
    }

    unsigned long ulBytes = (ulBpsRead <= 8) ? 1 : ((ulBpsRead > 16) ? 4 : 2);
    unsigned long ulRowBytes = ulBytes * ulNum;

    /* malloc of the row*/
    unsigned char *pucPos = reinterpret_cast<unsigned char*>(
                                                        VSIMalloc(ulRowBytes));
    if (pucPos == nullptr)
    {
        return cJP2_Error_Failure_Malloc;
    }

    /* check scanlines already read */
    CPLErr err = poBand->RasterIO(GF_Read,
                                  static_cast<int>(ulStart),
                                  static_cast<int>(ulRow),
                                  static_cast<int>(ulNum), 1,
                                  pucPos,
                                  static_cast<int>(ulNum), 1,
                                  eDataType, 0, 0, nullptr);
    if (err != CE_None)
    {
        VSIFree(pucPos);
        return cJP2_Error_Read_Callback_Undefined;
    }

    /* deliver the requested pixels to the library */
    if (nBands == 1 && eDataType == GDT_Float32 )
    {
        unsigned int mantissa;
        int exponent;
        int sign;

        const unsigned long nSpaceMantissa = 4;
        for (int i = 0; i < (int) ulNum; i++)
        {
            float *ptr = (float*)(&pucPos[i * 4]);
            splitIEEE754Float(*ptr, &mantissa, &exponent, &sign);
            switch (sComponent)
            {
                case 0:
                {
                    pucData[i] = (sign) ? 255 : 0;
                    break;
                }
                case 1:
                {
                    pucData[i] = static_cast<unsigned char>(exponent);
                    break;
                }
                case 2:
                {
                    *reinterpret_cast<unsigned int*>(
                                pucData + i * nSpaceMantissa) = mantissa;
                    break;
                }
            }
        }
    }
    else
    {
        memcpy(pucData, pucPos, ulBytes * ulNum);
    }

    VSIFree(pucPos);

    return cJP2_Error_OK;
}
