/******************************************************************************
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

#include "csf.h"
#include "csfimpl.h"
#include "pcrasterdataset.h"
#include "pcrasterrasterband.h"
#include "pcrasterutil.h"

CPL_CVSID("$Id$");

/*!
  \file
  This file contains the implementation of the PCRasterRasterBand class.
*/

//------------------------------------------------------------------------------
// DEFINITION OF PCRRASTERBAND MEMBERS
//------------------------------------------------------------------------------

//! Constructor.
/*!
  \param     dataset The dataset we are a part of.
*/
PCRasterRasterBand::PCRasterRasterBand( PCRasterDataset* dataset ) :
    GDALPamRasterBand(),
    d_dataset(dataset),
    d_noDataValue(),
    d_defaultNoDataValueOverridden(false),
    d_create_in(GDT_Unknown)
{
    poDS = dataset;
    nBand = 1;
    eDataType = cellRepresentation2GDALType(dataset->cellRepresentation());
    nBlockXSize = dataset->GetRasterXSize();
    nBlockYSize = 1;
}

//! Destructor.
/*!
*/
PCRasterRasterBand::~PCRasterRasterBand() {}

double PCRasterRasterBand::GetNoDataValue( int* success )
{
  if(success) {
    *success = 1;
  }

  return d_defaultNoDataValueOverridden
    ? d_noDataValue : d_dataset->defaultNoDataValue();
}

double PCRasterRasterBand::GetMinimum(
         int* success)
{
  double result;
  bool isValid;

  switch(d_dataset->cellRepresentation()) {
    // CSF version 2. ----------------------------------------------------------
    case CR_UINT1: {
      UINT1 min;
      isValid = CPL_TO_BOOL(RgetMinVal(d_dataset->map(), &min));
      result = static_cast<double>(min);
      break;
    }
    case CR_INT4: {
      INT4 min;
      isValid = CPL_TO_BOOL(RgetMinVal(d_dataset->map(), &min));
      result = static_cast<double>(min);
      break;
    }
    case CR_REAL4: {
      REAL4 min;
      isValid = CPL_TO_BOOL(RgetMinVal(d_dataset->map(), &min));
      result = static_cast<double>(min);
      break;
    }
    case CR_REAL8: {
      REAL8 min;
      isValid = CPL_TO_BOOL(RgetMinVal(d_dataset->map(), &min));
      result = static_cast<double>(min);
      break;
    }
    // CSF version 1. ----------------------------------------------------------
    case CR_INT1: {
      INT1 min;
      isValid = CPL_TO_BOOL(RgetMinVal(d_dataset->map(), &min));
      result = static_cast<double>(min);
      break;
    }
    case CR_INT2: {
      INT2 min;
      isValid = CPL_TO_BOOL(RgetMinVal(d_dataset->map(), &min));
      result = static_cast<double>(min);
      break;
    }
    case CR_UINT2: {
      UINT2 min;
      isValid = CPL_TO_BOOL(RgetMinVal(d_dataset->map(), &min));
      result = static_cast<double>(min);
      break;
    }
    case CR_UINT4: {
      UINT4 min;
      isValid = CPL_TO_BOOL(RgetMinVal(d_dataset->map(), &min));
      result = static_cast<double>(min);
      break;
    }
    default: {
      result = 0.0;
      isValid = false;
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
  bool isValid;

  switch(d_dataset->cellRepresentation()) {
    case CR_UINT1: {
      UINT1 max;
      isValid = CPL_TO_BOOL(RgetMaxVal(d_dataset->map(), &max));
      result = static_cast<double>(max);
      break;
    }
    case CR_INT4: {
      INT4 max;
      isValid = CPL_TO_BOOL(RgetMaxVal(d_dataset->map(), &max));
      result = static_cast<double>(max);
      break;
    }
    case CR_REAL4: {
      REAL4 max;
      isValid = CPL_TO_BOOL(RgetMaxVal(d_dataset->map(), &max));
      result = static_cast<double>(max);
      break;
    }
    // CSF version 1. ----------------------------------------------------------
    case CR_INT1: {
      INT1 max;
      isValid = CPL_TO_BOOL(RgetMaxVal(d_dataset->map(), &max));
      result = static_cast<double>(max);
      break;
    }
    case CR_INT2: {
      INT2 max;
      isValid = CPL_TO_BOOL(RgetMaxVal(d_dataset->map(), &max));
      result = static_cast<double>(max);
      break;
    }
    case CR_UINT2: {
      UINT2 max;
      isValid = CPL_TO_BOOL(RgetMaxVal(d_dataset->map(), &max));
      result = static_cast<double>(max);
      break;
    }
    case CR_UINT4: {
      UINT4 max;
      isValid = CPL_TO_BOOL(RgetMaxVal(d_dataset->map(), &max));
      result = static_cast<double>(max);
      break;
    }
    default: {
      result = 0.0;
      isValid = false;
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
         GetNoDataValue());

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
  else
  {
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

  const int nr_cols = this->poDS->GetRasterXSize();

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
  switch(valuescale) {
    case VS_BOOLEAN:
    case VS_LDD: {
      alterToStdMV(buffer, nr_cols, CR_UINT1, GetNoDataValue());
      break;
    }
    case VS_NOMINAL:
    case VS_ORDINAL: {
      alterToStdMV(buffer, nr_cols, CR_INT4, GetNoDataValue());
      break;
    }
    case VS_SCALAR:
    case VS_DIRECTION: {
      alterToStdMV(buffer, nr_cols, CR_REAL4, GetNoDataValue());
      break;
    }
    default: {
      break;
    }
  }

  // conversion of values according to value scale
  switch(valuescale) {
    case VS_BOOLEAN: {
      castValuesToBooleanRange(buffer, nr_cols, CR_UINT1);
      break;
    }
    case VS_LDD: {
      castValuesToLddRange(buffer, nr_cols);
      break;
    }
    case VS_DIRECTION: {
      castValuesToDirectionRange(buffer, nr_cols);
      break;
    }
    default: {
      break;
    }
  }

  RputRow(d_dataset->map(), nBlockYoff, buffer);
  free(buffer);

  return CE_None;
}

CPLErr PCRasterRasterBand::SetNoDataValue(double nodata)
{
  d_noDataValue = nodata;
  d_defaultNoDataValueOverridden = true;

  return CE_None;
}
