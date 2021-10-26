/******************************************************************************
 * $Id$
 *
 * Project:  PCRaster Integration
 * Purpose:  PCRaster raster band declaration.
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
#define INCLUDED_PCRASTERRASTERBAND

#include "gdal_pam.h"

// namespace {
  // PCRasterRasterBand declarations.
// }
class PCRasterDataset;

// namespace {

//! This class specialises the GDALRasterBand class for PCRaster rasters.
/*!
*/
class PCRasterRasterBand final: public GDALPamRasterBand
{

private:

  //! Dataset this band is part of. For use only.
  PCRasterDataset const* d_dataset;

  double           d_noDataValue;
  bool             d_defaultNoDataValueOverridden;
  GDALDataType     d_create_in;

  virtual CPLErr   IRasterIO           (GDALRWFlag, int, int, int, int,
                                        void *, int, int, GDALDataType,
                                        GSpacing nPixelSpace,
                                        GSpacing nLineSpace,
                                        GDALRasterIOExtraArg* psExtraArg) override;

  //! Assignment operator. NOT IMPLEMENTED.
  PCRasterRasterBand& operator=        (const PCRasterRasterBand&);

  //! Copy constructor. NOT IMPLEMENTED.
                   PCRasterRasterBand  (const PCRasterRasterBand&);

protected:
  // cppcheck-suppress functionConst
  double           GetNoDataValue      (int* success=nullptr) override;
  double           GetMinimum          (int* success) override;
  double           GetMaximum          (int* success) override;

public:
  explicit          PCRasterRasterBand  (PCRasterDataset* dataset);
  /* virtual */    ~PCRasterRasterBand ();

  //----------------------------------------------------------------------------
  // MANIPULATORS
  //----------------------------------------------------------------------------

  CPLErr           IWriteBlock         (CPL_UNUSED int nBlockXoff,
                                        int nBlockYoff,
                                        void* buffer) override;

  CPLErr           SetNoDataValue      (double no_data) override;

  //----------------------------------------------------------------------------
  // ACCESSORS
  //----------------------------------------------------------------------------

  CPLErr           IReadBlock          (int nBlockXoff,
                                        int nBlockYoff,
                                        void* buffer) override;
};
// } // namespace

#endif
