/*
 * OpenOptions.h
 *
 *  Created on: Oct 9, 2018
 *      Author: nielson
 */

#ifndef FRMTS_HDF5R_OPENOPTIONS_H_
#define FRMTS_HDF5R_OPENOPTIONS_H_

#include "Hdf5rAttributeBase.h"

/**
 * @brief GDAL Open Options
 * The GDAL Open() options are set as NAME=VALUE pairs on gdal command lines
 * using the -oo switch.  These are not really HDF5-R attributes but the
 * Hdf5rAttributeBase class provides convenient methods for setting and
 * accessing the values.
 *
 * The following options control how Open() uses GCP data built from the
 * input HDF5-R GeoLocationData LOS grid.
 * Specified on the -oo option of gdal commands.
 *
 *   GCP_MAX=[N]  (default GCP_MAX=225)
 *     This option controls the number of GCPs generated from the
 *     (rather dense for the max GDAL order polynomial of 3) input
 *     GeoLocationData grid.  A value of 0 (or negative) specifies no
 *     maximum so there is one GCP per on-Earth GeoLocationData grid point.
 *     Otherwise the grid is reduced to fit within the GCP_MAX limit.
 *     Note that the GCP_MAX is the product of the two sides of the grid,
 *     so a value of 225 amounts to 15 grid points on a side for a square
 *     grid. Setting this option will suppress the warning regarding the
 *     reduced grid size.
 *
 *   ATTR_WARN=[0,1] (default ATTR_WARN=0)
 *     Issue a warning for missing attributes in the HDF5-R source file
 *     on open.  Setting this to 0 will suppress these warnings.
 *
 *   NO_GCP=[0,1]  (default NO_GCP=0)
 *     Use NO_GCP=1 to set the affine (first order) transform for the GDAL
 *     data set instead of using GCPs.
 *
 *   ATTR_RW=[0,1]  (default ATTR_RW=1)
 *     Use ATTR_RW=0 to keep the HDF5-R fileMetaData and summaryMetaData values
 *     (such as min-max intensity) as read from the source HDF5-R file
 *     which are set for all frames in the source.  The default of ATTR_RW=1
 *     will set attributes to reflect the single frame loaded into the GDAL
 *     data set.
 *
 *   BLANK_OFF_EARTH=[0,1]  (default BLANK_OFF_EARTH=1)
 *     When BLANK_OFF_EARTH=1, image pixels that are off-Earth are changed to
 *     the GDAL NODATA value.  If BLANK_OFF_EARTH=0, then the image pixel values
 *     are not modified.
 *
 *   SAT_LON=[Lon_degrees]   (default not set)
 *     If this attribute is set, it is used as the observing satellite Longitude
 *     and assumes 0 Latitude with a Earth geo-synchronous altitude.  This
 *     satellite location will override the H5R.satPosEcf and recompute the
 *     LOS grid points Line-of-Sight vectors.  The grid latitude and longitude
 *     values are unchanged, unless the new satellite location is no
 *     longer observable, in which case they will be set to the
 *     H5R.GEO.OFF_EARTH_value setting.  This option is primarily intended for
 *     generating test material when sources of images with off-Earth portions
 *     are unavailable.
 */
class OpenOptions: public Hdf5rAttributeBase
{
public:
    OpenOptions()
    {
        // negative GCP_MAX to indicate default -- changed to positive when used
        h5AttrMap_["GCP_MAX"]         = H5Attr_t( "", H5T_NATIVE_INT32,  h5ScalarSpaceId_, int32_t( -225 ) );
        h5AttrMap_["ATTR_WARN"]       = H5Attr_t( "", H5T_NATIVE_INT32,  h5ScalarSpaceId_, int32_t( 0 ) );
        h5AttrMap_["NO_GCP"]          = H5Attr_t( "", H5T_NATIVE_INT32,  h5ScalarSpaceId_, int32_t( 0 ) );
        h5AttrMap_["ATTR_RW"]         = H5Attr_t( "", H5T_NATIVE_INT32,  h5ScalarSpaceId_, int32_t( 1 ) );
        h5AttrMap_["BLANK_OFF_EARTH"] = H5Attr_t( "", H5T_NATIVE_INT32,  h5ScalarSpaceId_, int32_t( 1 ) );
        h5AttrMap_["SAT_LON"]         = H5Attr_t( "", H5T_NATIVE_DOUBLE, h5ScalarSpaceId_, double( std::nan( "" ) ) );

        char* dftStr = static_cast<char*>( calloc( 6, 1 ) );
        strcpy( dftStr, "wgs84" );
        h5AttrMap_["PROJ"]            = H5Attr_t( "", H5T_C_S1,          h5ScalarSpaceId_, dftStr, 5 );
    }

    virtual ~OpenOptions() {}
};

#endif /* FRMTS_HDF5R_OPENOPTIONS_H_ */
