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
  \todo      What about unknown data types?
  \todo      Convert assertions to exceptions.
*/
PCRasterRasterBand::PCRasterRasterBand(PCRasterDataset* dataset)

  : GDALRasterBand(), d_dataset(dataset)

{
  this->poDS = dataset;
  this->nBand = 1;

  this->eDataType = PCRasterType2GDALType(dataset->cellRepresentation());
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



//!
/*!
  \param     .
  \return    .
  \exception .
  \warning   .
  \sa        .
  \todo      Convert assertions to exceptions.
*/
CPLErr PCRasterRasterBand::IReadBlock(int nBlockXoff, int nBlockYoff,
         void* buffer)
{
  PCRasterDataset* dataset = dynamic_cast<PCRasterDataset*>(this->poDS);

  // We should be reading rows of data (see constructor).
  assert(nBlockXoff == 0);

  // Row number is in nBlockYoff.
  size_t nrCellsRead = RgetRow(dataset->map(), nBlockYoff, buffer);
  assert(static_cast<int>(nrCellsRead) == this->nBlockXSize);

  if(dataset->cellRepresentation() == CR_REAL4) {
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



