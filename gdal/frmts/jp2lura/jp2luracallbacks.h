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

#ifndef JP2LURACALLBACKS_H_INCLUDED
#define JP2LURACALLBACKS_H_INCLUDED

#include "gdal_priv.h"

#include "lwf_jp2.h"

#ifdef ENABLE_MEMORY_REGISTRAR
#include <map>
class JP2LuraMemoryRegistrar
{
    std::map<void*, size_t> oMap;
public:
    JP2LuraMemoryRegistrar();
    ~JP2LuraMemoryRegistrar();

    void Register(size_t nSize, void* ptr);
    void Unregister(void* ptr);
};
#endif // ENABLE_MEMORY_REGISTRAR

typedef struct
{
    JP2_Decomp_Handle   handle;     // JP2 decompression handle

    long                lBps;       // bits for each sample
    bool                bSigned;    // are the samples signed?

    JP2_Palette_Params      *pPalette;  // pointer to optional palette. Not to be freed
    JP2_Channel_Def_Params  *pChannelDefs; // pointer to channel def. Not to be freed
    unsigned long       ulChannelDefs;   // number of channel def entries

    // to preserve a cache
    int              nBand; // starting at 1
    int              nBands;
    GDALDataType     eBufType;
    int              nXOff;
    int              nYOff;
    int              nXSize;
    int              nYSize;
    unsigned char ** pDatacache;
    int              nBufXSize;
    int              nBufYSize;


    unsigned char* pimage;
} GDALJP2Lura_Output_Data;

typedef struct
{
    GDALDataset *poSrcDS;
    bool bLinux64Hack; // whether the Lura SDK use 8 bytes to space 32bit samples

    GDALProgressFunc pfnProgress; // to progress report
    void * pProgressData; // to progress report
} GDALJP2Lura_Input_Data;

typedef struct
{
    vsi_l_offset Position;
    VSILFILE* fp;
} JP2_Gdal_Stream_Data;

void *  JP2_Callback_Conv  GDALJP2Lura_Callback_Malloc(size_t size,
                                                    JP2_Callback_Param lParam);

JP2_Error  JP2_Callback_Conv  GDALJP2Lura_Callback_Free(void *ptr,
                                                    JP2_Callback_Param lParam);

unsigned long  JP2_Callback_Conv  GDALJP2Lura_Callback_Decompress_Read(
                                                    unsigned char  *pucData,
                                                    unsigned long   ulPos,
                                                    unsigned long   ulSize,
                                                    JP2_Callback_Param lParam);

extern JP2_Error JP2_Callback_Conv GDALJP2Lura_Callback_Decompress_Write(
                                    unsigned char*  pucData,
                                    short           sComponent,
                                    unsigned long   ulRow,
                                    unsigned long   ulStart, // starting pixel
                                    unsigned long   ulNum, // number of pixels
                                    JP2_Callback_Param   lParam);

extern JP2_Error  JP2_Callback_Conv  GDALJP2Lura_Callback_Compress_Write(
                                        unsigned char           *pucData,
                                        unsigned long           ulPos,
                                        unsigned long           ulSize,
                                        JP2_Callback_Param      lParam);

extern JP2_Error  JP2_Callback_Conv  GDALJP2Lura_Callback_Compress_Read(
                                            unsigned char*      pucData,
                                            short               sComponent,
                                            unsigned long       ulRow,
                                            unsigned long       ulStart,
                                            unsigned long       ulNum,
                                            JP2_Callback_Param  lParam);

#endif
