#ifndef INCLUDED_PCRASTERRASTERBAND
#define INCLUDED_PCRASTERRASTERBAND



// Library headers.

// PCRaster library headers.

// Module headers.
#ifndef INCLUDED_GDAL_PAM
#include "gdal_pam.h"
#define INCLUDED_GDAL_PAM
#endif



// namespace {
  // PCRasterRasterBand declarations.
// }
class PCRasterDataset;



// namespace {



//! This class specialises the GDALRasterBand class for PCRaster rasters.
/*!
*/
class PCRasterRasterBand: public GDALPamRasterBand
{

private:

  //! Dataset this band is part of. For use only.
  PCRasterDataset const* d_dataset;

  //! Assignment operator. NOT IMPLEMENTED.
  PCRasterRasterBand& operator=        (const PCRasterRasterBand&);

  //! Copy constructor. NOT IMPLEMENTED.
                   PCRasterRasterBand  (const PCRasterRasterBand&);

protected:

  double           GetNoDataValue      (int* success);

  double           GetMinimum          (int* success);

  double           GetMaximum          (int* success);

public:

  //----------------------------------------------------------------------------
  // CREATORS
  //----------------------------------------------------------------------------

                   PCRasterRasterBand  (PCRasterDataset* dataset);

  /* virtual */    ~PCRasterRasterBand ();

  //----------------------------------------------------------------------------
  // MANIPULATORS
  //----------------------------------------------------------------------------

  //----------------------------------------------------------------------------
  // ACCESSORS
  //----------------------------------------------------------------------------

  CPLErr           IReadBlock          (int nBlockXoff,
                                        int nBlockYoff,
                                        void* buffer);

};



//------------------------------------------------------------------------------
// INLINE FUNCTIONS
//------------------------------------------------------------------------------



//------------------------------------------------------------------------------
// FREE OPERATORS
//------------------------------------------------------------------------------



//------------------------------------------------------------------------------
// FREE FUNCTIONS
//------------------------------------------------------------------------------



// } // namespace

#endif
