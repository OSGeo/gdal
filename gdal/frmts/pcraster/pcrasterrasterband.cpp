/******************************************************************************
 * $Id$
 *
 * Project:  PCRaster Integration
 * Purpose:  PCRaster raster band implementation.
 * Author:   Kor de Jong, k.dejong at geog.uu.nl
 *
 ******************************************************************************
 * Copyright (c) 2004, Kor de Jong
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

  : GDALPamRasterBand(), d_dataset(dataset)

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



//------------------------------------------------------------------------------
// DEFINITION OF FREE OPERATORS
//------------------------------------------------------------------------------



//------------------------------------------------------------------------------
// DEFINITION OF FREE FUNCTIONS
//------------------------------------------------------------------------------
