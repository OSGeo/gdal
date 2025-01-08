/******************************************************************************
 *
 * Project:  PCRaster Integration
 * Purpose:  PCRaster raster band declaration.
 * Author:   Kor de Jong, Oliver Schmitz
 *
 ******************************************************************************
 * Copyright (c) PCRaster owners
 *
 * SPDX-License-Identifier: MIT
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
class PCRasterRasterBand final : public GDALPamRasterBand
{

  private:
    //! Dataset this band is part of. For use only.
    PCRasterDataset const *d_dataset;

    double d_noDataValue;
    bool d_defaultNoDataValueOverridden;
    GDALDataType d_create_in;

    virtual CPLErr IRasterIO(GDALRWFlag, int, int, int, int, void *, int, int,
                             GDALDataType, GSpacing nPixelSpace,
                             GSpacing nLineSpace,
                             GDALRasterIOExtraArg *psExtraArg) override;

    //! Assignment operator. NOT IMPLEMENTED.
    PCRasterRasterBand &operator=(const PCRasterRasterBand &);

    //! Copy constructor. NOT IMPLEMENTED.
    PCRasterRasterBand(const PCRasterRasterBand &);

  protected:
    // cppcheck-suppress functionConst
    double GetNoDataValue(int *success = nullptr) override;
    double GetMinimum(int *success) override;
    double GetMaximum(int *success) override;

  public:
    explicit PCRasterRasterBand(PCRasterDataset *dataset);
    /* virtual */ ~PCRasterRasterBand();

    //----------------------------------------------------------------------------
    // MANIPULATORS
    //----------------------------------------------------------------------------

    CPLErr IWriteBlock(CPL_UNUSED int nBlockXoff, int nBlockYoff,
                       void *buffer) override;

    CPLErr SetNoDataValue(double no_data) override;

    //----------------------------------------------------------------------------
    // ACCESSORS
    //----------------------------------------------------------------------------

    CPLErr IReadBlock(int nBlockXoff, int nBlockYoff, void *buffer) override;
};

// } // namespace

#endif
