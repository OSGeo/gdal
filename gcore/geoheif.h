/******************************************************************************
 * Project:  GeoHEIF support class
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GEOHEIF_H_INCLUDED_
#define GEOHEIF_H_INCLUDED_

#include "gdal_pam.h"
#include "ogr_spatialref.h"

#include <vector>

//! @cond Doxygen_Suppress

namespace gdal
{

/*
 * GeoHEIF support implementation.
 * 
 * This class provides shared implementation for OGC GeoHEIF georeferencing,
 * which is currently in draft (see OGC 24-038).
 * 
 * GeoHEIF provides parsing and caching for spatial references, pixel
 * to model affine transformation, and tie-points.
 * 
 * This class is only shared here to provide common usage within
 * AVIF and HEIF drivers. It is not intended to be a user-level API.
*/
class CPL_DLL GeoHEIF final
{
    mutable OGRSpatialReference m_oSRS{};
    double modelTransform[6] = {0.0};
    bool haveGCPs = false;
    std::vector<gdal::GCP> gcps;

  public:
    GeoHEIF();
    ~GeoHEIF();

    bool has_SRS() const;
    bool has_GCPs() const;
    const OGRSpatialReference *GetSpatialRef() const;
    void setModelTransformation(const uint8_t *payload, size_t length);
    CPLErr GetGeoTransform(double *) const;
    void addGCPs(const uint8_t *payload, size_t length);
    int GetGCPCount() const;
    const GDAL_GCP *GetGCPs();
    void extractSRS(const uint8_t *payload, size_t length) const;
    const OGRSpatialReference *GetGCPSpatialRef() const;
};

//! @endcond

}  // namespace gdal

#endif /* GEOHEIF_H_INCLUDED_ */
