/******************************************************************************
 *
 * Project:  GeoTIFF Driver
 * Purpose:  GDAL GeoTIFF support.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, 2002, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2015, Even Rouault <even dot rouault at spatialys dot com>
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

#ifndef GTIFFODDBITSBAND_H_INCLUDED
#define GTIFFODDBITSBAND_H_INCLUDED

#include "gtiffrasterband.h"

/************************************************************************/
/* ==================================================================== */
/*                             GTiffOddBitsBand                         */
/* ==================================================================== */
/************************************************************************/

class GTiffOddBitsBand CPL_NON_FINAL : public GTiffRasterBand
{
    friend class GTiffDataset;

  public:
    GTiffOddBitsBand(GTiffDataset *, int);

    virtual ~GTiffOddBitsBand()
    {
    }

    bool IsBaseGTiffClass() const override
    {
        return false;
    }

    virtual CPLErr IReadBlock(int, int, void *) override;
    virtual CPLErr IWriteBlock(int, int, void *) override;
};

#endif  // GTIFFODDBITSBAND_H_INCLUDED
