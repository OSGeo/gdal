/******************************************************************************
 * $Id$
 *
 * Project:  PCRaster Integration
 * Purpose:  PCRaster raster band implementation.
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

#ifndef INCLUDED_PCRASTERRASTERBAND
#include "pcrasterrasterband.h"
#define INCLUDED_PCRASTERRASTERBAND
#endif

// PCRaster library headers.
#ifndef INCLUDED_CSF
#include "csf.h"
#define INCLUDED_CSF
#endif

#ifndef INCLUDED_CSFIMPL
#include "csfimpl.h"
#define INCLUDED_CSFIMPL
#endif

// Module headers.
#ifndef INCLUDED_PCRASTERDATASET
#include "pcrasterdataset.h"
#define INCLUDED_PCRASTERDATASET
#endif

#ifndef INCLUDED_PCRASTERUTIL
#include "pcrasterutil.h"
#define INCLUDED_PCRASTERUTIL
#endif



/*!
  \file
  This file contains the implementation of the PCRasterRasterBand class.
*/



//------------------------------------------------------------------------------
// DEFINITION OF STATIC PCRRASTERBAND MEMBERS
//------------------------------------------------------------------------------



//------------------------------------------------------------------------------
// DEFINITION OF PCRRASTERBAND MEMBERS
//------------------------------------------------------------------------------

//! Constructor.
/*!
  \param     dataset The dataset we are a part of.
*/
PCRasterRasterBand::PCRasterRasterBand(
         PCRasterDataset* dataset)

  : GDALPamRasterBand(),
    d_dataset(dataset),
    d_missing_value(-FLT_MAX),
    d_create_in(GDT_Unknown)

{
  this->poDS = dataset;
  this->nBand = 1;
  this->eDataType = cellRepresentation2GDALType(dataset->cellRepresentation());
  this->nBlockXSize = dataset->GetRasterXSize();
  this->nBlockYSize = 1;
}



//! Destructor.
/*!
*/
PCRasterRasterBand::~PCRasterRasterBand()
{
}



double PCRasterRasterBand::GetNoDataValue(
         int* success)
{
  if(success) {
    *success = 1;
  }

  return d_dataset->missingValue();
}



double PCRasterRasterBand::GetMinimum(
         int* success)
{
  double result;
  int isValid;

  switch(d_dataset->cellRepresentation()) {
    // CSF version 2. ----------------------------------------------------------
    case CR_UINT1: {
      UINT1 min;
      isValid = RgetMinVal(d_dataset->map(), &min);
      result = static_cast<double>(min);
      break;
    }
    case CR_INT4: {
      INT4 min;
      isValid = RgetMinVal(d_dataset->map(), &min);
      result = static_cast<double>(min);
      break;
    }
    case CR_REAL4: {
      REAL4 min;
      isValid = RgetMinVal(d_dataset->map(), &min);
      result = static_cast<double>(min);
      break;
    }
    case CR_REAL8: {
      REAL8 min;
      isValid = RgetMinVal(d_dataset->map(), &min);
      result = static_cast<double>(min);
      break;
    }
    // CSF version 1. ----------------------------------------------------------
    case CR_INT1: {
      INT1 min;
      isValid = RgetMinVal(d_dataset->map(), &min);
      result = static_cast<double>(min);
      break;
    }
    case CR_INT2: {
      INT2 min;
      isValid = RgetMinVal(d_dataset->map(), &min);
      result = static_cast<double>(min);
      break;
    }
    case CR_UINT2: {
      UINT2 min;
      isValid = RgetMinVal(d_dataset->map(), &min);
      result = static_cast<double>(min);
      break;
    }
    case CR_UINT4: {
      UINT4 min;
      isValid = RgetMinVal(d_dataset->map(), &min);
      result = static_cast<double>(min);
      break;
    }
    default: {
      result = 0.0;
      isValid = 0;
      break;
    }
  }

  if(success) {
    *success = isValid ? 1 : 0;
  }

  return result;
}



double PCRasterRasterBand::GetMaximum(
         int* success)
{
  double result;
  int isValid;

  switch(d_dataset->cellRepresentation()) {
    case CR_UINT1: {
      UINT1 max;
      isValid = RgetMaxVal(d_dataset->map(), &max);
      result = static_cast<double>(max);
      break;
    }
    case CR_INT4: {
      INT4 max;
      isValid = RgetMaxVal(d_dataset->map(), &max);
      result = static_cast<double>(max);
      break;
    }
    case CR_REAL4: {
      REAL4 max;
      isValid = RgetMaxVal(d_dataset->map(), &max);
      result = static_cast<double>(max);
      break;
    }
    // CSF version 1. ----------------------------------------------------------
    case CR_INT1: {
      INT1 max;
      isValid = RgetMaxVal(d_dataset->map(), &max);
      result = static_cast<double>(max);
      break;
    }
    case CR_INT2: {
      INT2 max;
      isValid = RgetMaxVal(d_dataset->map(), &max);
      result = static_cast<double>(max);
      break;
    }
    case CR_UINT2: {
      UINT2 max;
      isValid = RgetMaxVal(d_dataset->map(), &max);
      result = static_cast<double>(max);
      break;
    }
    case CR_UINT4: {
      UINT4 max;
      isValid = RgetMaxVal(d_dataset->map(), &max);
      result = static_cast<double>(max);
      break;
    }
    default: {
      result = 0.0;
      isValid = 0;
      break;
    }
  }

  if(success) {
    *success = isValid ? 1 : 0;
  }

  return result;
}



CPLErr PCRasterRasterBand::IReadBlock(
    CPL_UNUSED int nBlockXoff,
    int nBlockYoff,
    void* buffer)
{
  size_t nrCellsRead = RgetRow(d_dataset->map(), nBlockYoff, buffer);

  // Now we have raw values, missing values are set according to the CSF
  // conventions. This means that floating points should not be evaluated.
  // Since this is done by the GDal library we replace these with valid
  // values. Non-MV values are not touched.

  // Replace in-file MV with in-app MV which may be different.
  alterFromStdMV(buffer, nrCellsRead, d_dataset->cellRepresentation(),
         d_dataset->missingValue());

  return CE_None;
}


CPLErr PCRasterRasterBand::IRasterIO(GDALRWFlag eRWFlag,
                                     int nXOff, int nYOff, int nXSize,
                                     int nYSize, void * pData,
                                     int nBufXSize, int nBufYSize,
                                     GDALDataType eBufType,
                                     GSpacing nPixelSpace,
                                     GSpacing nLineSpace,
                                     GDALRasterIOExtraArg* psExtraArg)
{
  if (eRWFlag == GF_Read){
    // read should just be the default
    return GDALRasterBand::IRasterIO(GF_Read, nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize, eBufType,
                                     nPixelSpace, nLineSpace, psExtraArg);
  }
  else{
    // the datatype of the incoming data can be of different type than the
    // cell representation used in the raster
    // 'remember' the GDAL type to distinguish it later on in iWriteBlock
    d_create_in = eBufType;
    return GDALRasterBand::IRasterIO(GF_Write, nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize, eBufType,
                                     nPixelSpace, nLineSpace, psExtraArg);

  }
}



CPLErr PCRasterRasterBand::IWriteBlock(
    CPL_UNUSED int nBlockXoff,
    int nBlockYoff,
    void* source)
{
  CSF_VS valuescale = d_dataset->valueScale();

  if(valuescale == VS_LDD) {
    if((d_create_in == GDT_Byte) || (d_create_in == GDT_Float32) || (d_create_in == GDT_Float64)) {
      CPLError(CE_Failure, CPLE_NotSupported,
               "PCRaster driver: "
               "conversion from %s to LDD not supported",
               GDALGetDataTypeName(d_create_in));
      return CE_Failure;
    }
  }

  // set new location attributes to the header
  if (d_dataset->location_changed()){
    REAL8 west = 0.0;
    REAL8 north = 0.0;
    REAL8 cellSize = 1.0;
    double transform[6];
    if(this->poDS->GetGeoTransform(transform) == CE_None) {
      if(transform[2] == 0.0 && transform[4] == 0.0) {
        west = static_cast<REAL8>(transform[0]);
        north = static_cast<REAL8>(transform[3]);
        cellSize = static_cast<REAL8>(transform[1]);
      }
    }
    (void)RputXUL(d_dataset->map(), west);
    (void)RputYUL(d_dataset->map(), north);
    (void)RputCellSize(d_dataset->map(), cellSize);
  }

  int nr_cols = this->poDS->GetRasterXSize();

  // new maps from create() set min/max to MV
  // in case of reopening that map the min/max
  // value tracking is disabled (MM_WRONGVALUE)
  // reactivate it again to ensure that the output will
  // get the correct values when values are written to map
  d_dataset->map()->minMaxStatus = MM_KEEPTRACK;

  // allocate memory for row
  void* buffer = Rmalloc(d_dataset->map(), nr_cols);
  memcpy(buffer, source, nr_cols * 4);

  // convert source no_data values to MV in dest
  if((valuescale == VS_BOOLEAN) || (valuescale == VS_LDD)) {
    alterToStdMV(buffer, nr_cols, CR_UINT1, d_missing_value);
  }
  if((valuescale == VS_SCALAR) || (valuescale == VS_DIRECTION)) {
    alterToStdMV(buffer, nr_cols, CR_REAL4, d_missing_value);
  }
  if((valuescale == VS_NOMINAL)|| (valuescale == VS_ORDINAL)) {
    alterToStdMV(buffer, nr_cols, CR_INT4, d_missing_value);
  }

  // conversion of values according to value scale
  if(valuescale == VS_BOOLEAN) {
    castValuesToBooleanRange(buffer, nr_cols, CR_UINT1);
  }
  if(valuescale == VS_LDD) {
    castValuesToLddRange(buffer, nr_cols);
  }
  if(valuescale == VS_DIRECTION) {
    castValuesToDirectionRange(buffer, nr_cols);
  }

  RputRow(d_dataset->map(), nBlockYoff, buffer);
  free(buffer);

  return CE_None;
}


CPLErr PCRasterRasterBand::SetNoDataValue(double nodata){
  d_missing_value = nodata;

  return CE_None;
}



//------------------------------------------------------------------------------
// DEFINITION OF FREE OPERATORS
//------------------------------------------------------------------------------



//------------------------------------------------------------------------------
// DEFINITION OF FREE FUNCTIONS
//------------------------------------------------------------------------------
