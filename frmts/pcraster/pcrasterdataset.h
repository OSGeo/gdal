#ifndef INCLUDED_PCRASTERDATASET
#define INCLUDED_PCRASTERDATASET



// Library headers.

// PCRaster library headers.
#ifndef INCLUDED_CSF
#include "csf.h"
#define INCLUDED_CSF
#endif

// Module headers.
#ifndef INCLUDED_GDAL_PRIV
#include "gdal_priv.h"
#define INCLUDED_GDAL_PRIV
#endif



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

  More info about PCRaster can be found at http://www.pcraster.nl and
  http://pcraster.geog.uu.nl
*/
class PCRasterDataset: public GDALDataset
{

  friend class gdal::PCRasterDatasetTest;

public:

  static GDALDataset* Open             (GDALOpenInfo* info);

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

  //! No data value.
  double           d_missingValue;

  //! Assignment operator. NOT IMPLEMENTED.
  PCRasterDataset& operator=           (const PCRasterDataset&);

  //! Copy constructor. NOT IMPLEMENTED.
                   PCRasterDataset     (const PCRasterDataset&);

  // void             determineMissingValue();

public:

  //----------------------------------------------------------------------------
  // CREATORS
  //----------------------------------------------------------------------------

                   PCRasterDataset     (MAP* map);

  /* virtual */    ~PCRasterDataset    ();

  //----------------------------------------------------------------------------
  // MANIPULATORS
  //----------------------------------------------------------------------------

  //----------------------------------------------------------------------------
  // ACCESSORS
  //----------------------------------------------------------------------------

  MAP*             map                 () const;

  CPLErr           GetGeoTransform     (double* transform);

  CSF_CR           cellRepresentation  () const;

  double           missingValue        () const;

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
