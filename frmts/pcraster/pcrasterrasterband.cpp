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

#ifndef INCLUDED_PCRTYPES
#include "pcrtypes.h"
#define INCLUDED_PCRTYPES
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
  \todo      What about unknown data types?
*/
#ifndef INCLUDED_IOSTREAM
#include <iostream>
#define INCLUDED_IOSTREAM
#endif
PCRasterRasterBand::PCRasterRasterBand(PCRasterDataset* dataset)

  : GDALRasterBand(), d_dataset(dataset)

{
  this->poDS = dataset;
  this->nBand = 1;

  this->eDataType = PCRType2GDALType(dataset->cellRepresentation());
  assert(eDataType != GDT_Unknown);

  // This results in a buffer of one row.
  this->nBlockXSize = dataset->GetRasterXSize();
  this->nBlockYSize = 1;

  // CPLErr result = SetNoDataValue(dataset->missingValue());
  // assert(result == CE_None);
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



CPLErr PCRasterRasterBand::IReadBlock(int nBlockXoff, int nBlockYoff,
         void* buffer)
{
  PCRasterDataset* dataset = dynamic_cast<PCRasterDataset*>(this->poDS);

  // We should be reading rows of data (see constructor).
  assert(nBlockXoff == 0);

  // Row number is in nBlockYoff.
  size_t nrCellsRead = RgetRow(dataset->map(), nBlockYoff, buffer);
  assert(static_cast<int>(nrCellsRead) == this->nBlockXSize);

  switch(dataset->cellRepresentation()) {
    case(CR_REAL4): {
      // Missing value in the buffer is a NAN. Replace by valid value.
      std::for_each(static_cast<REAL4*>(buffer),
         static_cast<REAL4*>(buffer) + nrCellsRead,
         pcr::AlterFromStdMV<REAL4>(static_cast<REAL4>(
         d_dataset->missingValue())));
      break;
    }
    default: {
      break;
    }
  }

  return CE_None;
}



//------------------------------------------------------------------------------
// DEFINITION OF FREE OPERATORS
//------------------------------------------------------------------------------



//------------------------------------------------------------------------------
// DEFINITION OF FREE FUNCTIONS
//------------------------------------------------------------------------------



