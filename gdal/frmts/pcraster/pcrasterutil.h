/******************************************************************************
 * $Id$
 *
 * Project:  PCRaster Integration
 * Purpose:  PCRaster driver support declarations.
 * Author:   Kor de Jong, Oliver Schmitz
 *
 ******************************************************************************
 * Copyright (c) PCRaster owners
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

// Library headers.
#ifndef INCLUDED_STRING
#include <string>
#define INCLUDED_STRING
#endif

// PCRaster library headers.
#ifndef INCLUDED_CSF
#include "csf.h"
#define INCLUDED_CSF
#endif

#ifndef INCLUDED_PCRTYPES
#include "pcrtypes.h"
#define INCLUDED_PCRTYPES
#endif

// Module headers.
#ifndef INCLUDED_GDAL_PRIV
#include "gdal_priv.h"
#define INCLUDED_GDAL_PRIV
#endif


GDALDataType       cellRepresentation2GDALType(CSF_CR cellRepresentation);

CSF_VS             string2ValueScale   (const std::string& string);

std::string        valueScale2String   (CSF_VS valueScale);

std::string        cellRepresentation2String(CSF_CR cellRepresentation);

CSF_VS             GDALType2ValueScale (GDALDataType type);

/*
CSF_CR             string2PCRasterCellRepresentation(
                                        const std::string& string);
                                        */

CSF_CR             GDALType2CellRepresentation(
                                        GDALDataType type,
                                        bool exact);

void*              createBuffer        (size_t size,
                                        CSF_CR type);

void               deleteBuffer        (void* buffer,
                                        CSF_CR type);

bool               isContinuous        (CSF_VS valueScale);

double             missingValue        (CSF_CR type);

void               alterFromStdMV      (void* buffer,
                                        size_t size,
                                        CSF_CR cellRepresentation,
                                        double missingValue);

void               alterToStdMV        (void* buffer,
                                        size_t size,
                                        CSF_CR cellRepresentation,
                                        double missingValue);

MAP*               mapOpen             (std::string const& filename,
                                        MOPEN_PERM mode);

CSF_VS             fitValueScale       (CSF_VS valueScale,
                                        CSF_CR cellRepresentation);

void               castValuesToBooleanRange(
                                        void* buffer,
                                        size_t size,
                                        CSF_CR cellRepresentation);

void               castValuesToDirectionRange(
                                        void* buffer,
                                        size_t size);

void               castValuesToLddRange(void* buffer,
                                        size_t size);

template<typename T>
struct CastToBooleanRange
{
  void operator()(T& value) {
    if(!pcr::isMV(value)) {
      if(value != 0) {
        value = T(value > T(0));
      }
      else {
        pcr::setMV(value);
      }
    }
  }
};



template<>
struct CastToBooleanRange<UINT1>
{
  void operator()(UINT1& value) {
    if(!pcr::isMV(value)) {
      value = UINT1(value > UINT1(0));
    }
  }
};



template<>
struct CastToBooleanRange<UINT2>
{
  void operator()(UINT2& value) {
    if(!pcr::isMV(value)) {
      value = UINT2(value > UINT2(0));
    }
  }
};



template<>
struct CastToBooleanRange<UINT4>
{
  void operator()(UINT4& value) {
    if(!pcr::isMV(value)) {
      value = UINT4(value > UINT4(0));
    }
  }
};


struct CastToDirection
{
  void operator()(REAL4& value) {
    REAL4 factor = M_PI / 180.0;
    if(!pcr::isMV(value)) {
      value = REAL4(value * factor);
    }
  }
};



struct CastToLdd
{
  void operator()(UINT1& value) {
    if(!pcr::isMV(value)) {
      if((value < 1) || (value > 9)) {
        CPLError(CE_Warning, CPLE_IllegalArg,
         "PCRaster driver: incorrect LDD value used, assigned MV instead");
        pcr::setMV(value);
      }
      else {
        value = UINT1(value);
      }
    }
  }
};