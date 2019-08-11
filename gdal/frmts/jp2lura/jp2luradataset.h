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

#ifndef JP2LURADATASET_H_INCLUDED
#define JP2LURADATASET_H_INCLUDED

#include "gdaljp2abstractdataset.h"
#include "jp2luracallbacks.h"
#include "gdaljp2metadata.h"

class JP2LuraDataset final: public GDALJP2AbstractDataset
{
    friend class JP2LuraRasterBand;

    VSILFILE   *fp; // Large FILE API 

    int         iLevel;
    int         nOverviewCount;
    JP2LuraDataset** papoOverviewDS;
    GDALJP2Lura_Output_Data sOutputData;
    GDALColorTable* poCT;
    JP2_Colorspace eColorspace;
    int            nRedIndex;
    int            nGreenIndex;
    int            nBlueIndex;
    int            nAlphaIndex;

#ifdef ENABLE_MEMORY_REGISTRAR
    JP2LuraMemoryRegistrar oMemoryRegistrar;
#endif

public:
    JP2LuraDataset();
    ~JP2LuraDataset();

    static int Identify(GDALOpenInfo * poOpenInfo);
    static GDALDataset  *Open(GDALOpenInfo *);
    static GDALDataset  *CreateCopy(const char * pszFilename,
            GDALDataset *poSrcDS,
            int bStrict, char ** papszOptions,
            GDALProgressFunc pfnProgress,
            void * pProgressData);


    virtual CPLErr  IRasterIO(GDALRWFlag eRWFlag,
            int nXOff, int nYOff, int nXSize, int nYSize,
            void * pData, int nBufXSize, int nBufYSize,
            GDALDataType eBufType,
            int nBandCount, int *panBandMap,
            GSpacing nPixelSpace, GSpacing nLineSpace,
            GSpacing nBandSpace,
            GDALRasterIOExtraArg* psExtraArg) override;


    static void         WriteBox(VSILFILE* fp, GDALJP2Box* poBox);
    static void         WriteGDALMetadataBox(VSILFILE* fp, GDALDataset* poSrcDS,
                                             char** papszOptions);
    static void         WriteXMLBoxes(VSILFILE* fp, GDALDataset* poSrcDS,
                                      char** papszOptions);
    static void         WriteXMPBox(VSILFILE* fp, GDALDataset* poSrcDS,
                                    char** papszOptions);

    static const char*  GetErrorMessage( long nErrorCode );
};

#endif
