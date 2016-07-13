/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Implementation of derived subdatasets
 * Author:   Julien Michel <julien dot michel at cnes dot fr>
 *
 ******************************************************************************
 * Copyright (c) 2016 Julien Michel <julien dot michel at cnes dot fr>
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
 *****************************************************************************/
#include "derivedlist.h"
#include "gdal.h"

CPL_C_START

static const DerivedDatasetDescription asDDSDesc [] =
{
  { "AMPLITUDE", "Amplitude of input bands", "mod", "complex","Float64"},
  { "PHASE", "Phase of input bands", "phase", "complex","Float64"},
  { "REAL", "Real part of input bands", "real", "complex","Float64"},
  { "IMAG", "Imaginary part of input bands", "imag", "complex","Float64"},
  { "CONJ", "Conjugate of input bands", "conj", "complex","CFloat64"},
  { "INTENSITY", "Intensity (squared amplitude) of input bands", "intensity", "complex","Float64"},
  { "LOGAMPLITUDE", "log10 of amplitude of input bands", "log10", "all","Float64"}
};

#define NB_DERIVED_DATASETS (sizeof(asDDSDesc)/sizeof(asDDSDesc[0]))

const DerivedDatasetDescription* CPL_STDCALL GDALGetDerivedDatasetDescriptions(unsigned int * pnDescriptionCount)
{
  *pnDescriptionCount = (unsigned int)NB_DERIVED_DATASETS;
  return asDDSDesc;
}

CPL_C_END
