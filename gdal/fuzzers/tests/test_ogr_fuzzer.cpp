/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Fuzzer
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
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

#include "../ogr_fuzzer.cpp"

int main()
{
    const char szGEOJSON[] = "{ \"type\": \"FeatureCollection\", \"features\":[ { \"type\": \"Feature\", \"properties\": { \"AREA\": 5268.813, \"EAS_ID\": 170, \"PRFEDEA\": \"35043413\" }, \"geometry\": { \"type\": \"Polygon\", \"coordinates\": [ [ [ 479750.6875, 4764702.0 ], [ 479658.59375, 4764670.0 ], [ 479640.09375, 4764721.0 ], [ 479735.90625, 4764752.0 ], [ 479750.6875, 4764702.0 ] ] ] } } ] }";
    LLVMFuzzerTestOneInput(reinterpret_cast<const uint8_t*>(szGEOJSON),
                           strlen(szGEOJSON));
    return 0;
}
