/*
 * CreationOptions.h
 *
 *  Created on: Oct 9, 2018
 *      Author: nielson
 */

#ifndef FRMTS_HDF5R_CREATIONOPTIONS_H_
#define FRMTS_HDF5R_CREATIONOPTIONS_H_

#include "Hdf5rAttributeBase.h"

/**
 * @brief GDAL Create Options
 * The GDAL Create() options are set as NAME=VALUE pairs on gdal command lines
 * using the -co switch.  These are not really HDF5-R attributes but the
 * Hdf5rAttributeBase class provides convenient methods for setting and
 * accessing the values.
 *
 * The following options control how CreateCopy() uses GCPs, if available,
 * from the SOURCE data set.
 *
 *   NO_GCP=[0,1]  (default NO_GCP=0)
 *      Controls whether or not to use GCPs if they are available.
 *      NO_GCP=0 allows use of GCPs if available.  Setting NO_GCP=1
 *      tells the driver not to use source GCPs at all, in which case the
 *      source data set must have an affine transform (and projection)
 *      defined, otherwise the HDF5-R output data set will not have a
 *      GeoLocationData data set.
 *
 *   GCP_REGRID=[0,1]  (default GCP_REGRID=0)
 *      If the source data set contains GCPs, NO_GCP=0 (the default),
 *      and GCP_REGRID=0, then the GCPs are tested to see if they make a
 *      fully populated grid.  If they do, then they are used directly.
 *      If GCP_REGRID=1 or the GCPs do not make a complete grid,
 *      the GCPs are used to generate a polynomial estimate
 *      to convert from pixel coordinates to projection coordinates on
 *      the grid boundaries specified by the GeoLocationAttributes,
 *      H5R.GEO.X_STEPSIZE_PIXELS, and H5R.GEO.Y_STEPSIZE_PIXELS.
 *
 *   GCP_ORDER=[0,1,...N]  (default GCP_ORDER=0)
 *      If the source data set contains GCPs, NO_GCP=0 (the default),
 *      and, GCP_REGRID=1 (not the default) or the input GCPs are not a
 *      complete grid, the GCPs are used (through GDAL methods) to build a
 *      polynomial of the specified order that maps pixel coordinates to
 *      projected coordinates.  GDAL uses an order of 0 to specify using
 *      the highest reliable order polynomial. Order N is whatever GDAL
 *      currently supports as the maximum order.
 */
class CreationOptions: public Hdf5rAttributeBase
{
public:
    CreationOptions()
    {
        h5AttrMap_["GCP_ORDER"]  = H5Attr_t( "", H5T_NATIVE_INT32,  h5ScalarSpaceId_, int32_t( 0 ) );
        h5AttrMap_["GCP_REGRID"] = H5Attr_t( "", H5T_NATIVE_INT32,  h5ScalarSpaceId_, int32_t( 0 ) );
        h5AttrMap_["NO_GCP"]     = H5Attr_t( "", H5T_NATIVE_INT32,  h5ScalarSpaceId_, int32_t( 0 ) );
    }

    virtual ~CreationOptions() {}
};

#endif /* FRMTS_HDF5R_CREATIONOPTIONS_H_ */
