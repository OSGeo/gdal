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

#ifndef JP2LURARASTERBAND_H_INCLUDED
#define JP2LURARASTERBAND_H_INCLUDED

#include "gdal_pam.h"

class JP2LuraDataset;

class JP2LuraRasterBand : public GDALPamRasterBand
{
        friend class JP2LuraDataset;


public:

        JP2LuraRasterBand(JP2LuraDataset * poDS, int nBand,
                          GDALDataType eDataType,
                          int nBits,
                          int nBlockXSize, int nBlockYSize);
        ~JP2LuraRasterBand();

        virtual CPLErr          IReadBlock(int, int, void *) override;
        virtual CPLErr          IRasterIO(GDALRWFlag eRWFlag,
                int nXOff, int nYOff, int nXSize, int nYSize,
                void * pData, int nBufXSize, int nBufYSize, GDALDataType eBufType,
                GSpacing nPixelSpace, GSpacing nLineSpace,
                GDALRasterIOExtraArg* psExtraArg) override;

        virtual int             GetOverviewCount() override;
        virtual GDALRasterBand* GetOverview(int iOvrLevel) override;

        virtual GDALColorInterp GetColorInterpretation() override;
        virtual GDALColorTable* GetColorTable() override;
};

#endif
