#ifndef INCLUDED_PCRASTERRASTERBAND
#include "pcrasterrasterband.h"
#define INCLUDED_PCRASTERRASTERBAND
#endif

// Library headers.
#ifndef INCLUDED_CASSERT
#include <cassert>
#define INCLUDED_CASSERT
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
PCRasterRasterBand::PCRasterRasterBand(PCRasterDataset* dataset)

  : GDALRasterBand(), d_dataset(dataset)

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



double PCRasterRasterBand::GetNoDataValue(int* success)
{
  if(success) {
    *success = 1;
  }

  return d_dataset->missingValue();
}



double PCRasterRasterBand::GetMinimum(int* success)
{
  double result;
  int isValid;

  switch(d_dataset->cellRepresentation()) {
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



double PCRasterRasterBand::GetMaximum(int* success)
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



CPLErr PCRasterRasterBand::IReadBlock(int nBlockXoff, int nBlockYoff,
         void* buffer)
{
  PCRasterDataset* dataset = dynamic_cast<PCRasterDataset*>(this->poDS);
  size_t nrCellsRead = RgetRow(dataset->map(), nBlockYoff, buffer);

  if(dataset->cellRepresentation() == CR_REAL4 ||
         dataset->cellRepresentation() == CR_REAL8) {
    // Missing value in the buffer is a NAN. Replace by valid value.
    alterFromStdMV(buffer, nrCellsRead, dataset->cellRepresentation(),
           d_dataset->missingValue());
  }

  return CE_None;
}



//------------------------------------------------------------------------------
// DEFINITION OF FREE OPERATORS
//------------------------------------------------------------------------------



//------------------------------------------------------------------------------
// DEFINITION OF FREE FUNCTIONS
//------------------------------------------------------------------------------



