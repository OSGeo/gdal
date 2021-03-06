/******************************************************************************
 * $Id$
 *
 * Project:  PCRaster Integration
 * Purpose:  PCRaster CSF 2.0 raster file driver declarations.
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

#ifndef INCLUDED_PCRASTERDATASET
#define INCLUDED_PCRASTERDATASET

#include "gdal_pam.h"
#include "csf.h"

// namespace {
  // PCRasterDataset declarations.
// }
namespace gdal {
  class PCRasterDatasetTest;
}

// namespace {

//! This class specialises the GDALDataset class for PCRaster datasets.
/*!
  PCRaster raster datasets are currently formatted by the CSF 2.0 data format.
  A PCRasterDataset consists of one band.

  More info about PCRaster can be found at http://www.pcraster.nl and
  http://pcraster.geog.uu.nl

  Additional documentation about this driver can be found in
  frmts/frmts_various.html of the GDAL source code distribution.
*/
class PCRasterDataset final: public GDALPamDataset
{

  friend class gdal::PCRasterDatasetTest;

public:

  static GDALDataset* open             (GDALOpenInfo* info);

  static GDALDataset* create           (const char* filename,
                                        int nr_cols,
                                        int nr_rows,
                                        int nrBands,
                                        GDALDataType gdalType,
                                        char** papszParamList);

  static GDALDataset* createCopy       (char const* filename,
                                        GDALDataset* source,
                                        int strict,
                                        char** options,
                                        GDALProgressFunc progress,
                                        void* progressData);

private:

  //! CSF map structure.
  MAP*             d_map;

  //! Left coordinate of raster.
  double           d_west;

  //! Top coordinate of raster.
  double           d_north;

  //! Cell size.
  double           d_cellSize;

  //! Cell representation.
  CSF_CR           d_cellRepresentation;

  //! Value scale.
  CSF_VS           d_valueScale;

  //! No data value.
  double           d_defaultNoDataValue;

  bool             d_location_changed;

  //! Assignment operator. NOT IMPLEMENTED.
  PCRasterDataset& operator=           (const PCRasterDataset&);

  //! Copy constructor. NOT IMPLEMENTED.
                   PCRasterDataset     (const PCRasterDataset&);

public:

  //----------------------------------------------------------------------------
  // CREATORS
  //----------------------------------------------------------------------------

  explicit          PCRasterDataset     (MAP* map, GDALAccess eAccess);

  /* virtual */    ~PCRasterDataset    ();

  //----------------------------------------------------------------------------
  // MANIPULATORS
  //----------------------------------------------------------------------------

  CPLErr           SetGeoTransform     (double* transform) override;

  //----------------------------------------------------------------------------
  // ACCESSORS
  //----------------------------------------------------------------------------

  MAP*             map                 () const;
  CPLErr           GetGeoTransform     (double* transform) override;
  CSF_CR           cellRepresentation  () const;
  CSF_VS           valueScale          () const;
  double           defaultNoDataValue  () const;
  bool             location_changed    () const;
};
// } // namespace

#endif
