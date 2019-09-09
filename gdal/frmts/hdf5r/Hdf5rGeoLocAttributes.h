/*
 * Hdf5rGeoLocAttributes.h
 *
 *  Created on: Sep 27, 2018
 *      Author: nielson
 */

#ifndef FRMTS_HDF5R_HDF5RGEOLOCATTRIBUTES_H_
#define FRMTS_HDF5R_HDF5RGEOLOCATTRIBUTES_H_

#include "Hdf5rAttributeBase.h"

class Hdf5rGeoLocAttributes: public Hdf5rAttributeBase
{
public:
    Hdf5rGeoLocAttributes()
   : Hdf5rAttributeBase()
   {
        h5AttrMap_["H5R.GEO.OFF_EARTH_value"]      = H5Attr_t( "OFF_EARTH_value",      H5T_NATIVE_DOUBLE, h5ScalarSpaceId_, -9999.0 );
        h5AttrMap_["H5R.GEO.equatorial_radius_km"] = H5Attr_t( "equatorial_radius_km", H5T_NATIVE_DOUBLE, h5ScalarSpaceId_, -6378.137 );
        h5AttrMap_["H5R.GEO.flattening"]           = H5Attr_t( "flattening",           H5T_NATIVE_DOUBLE, h5ScalarSpaceId_, 1.0/298.257223563 );
        h5AttrMap_["H5R.GEO.X_Stepsize_Pixels"]    = H5Attr_t( "X_Stepsize_Pixels",    H5T_NATIVE_INT32,  h5ScalarSpaceId_, int32_t( 12 ) );
        h5AttrMap_["H5R.GEO.Y_Stepsize_Pixels"]    = H5Attr_t( "Y_Stepsize_Pixels",    H5T_NATIVE_INT32,  h5ScalarSpaceId_, int32_t( 12 ) );

        char* dftStr = static_cast<char*>( calloc( 24, 1 ) );
        strcpy( dftStr, "LOS ECF unit vector" );
        h5AttrMap_["H5R.GEO.XY_coord_system"]      = H5Attr_t( "XY_coord_system",      H5T_C_S1,          h5ScalarSpaceId_, dftStr, 16 );

        dftStr = static_cast<char*>( calloc( 24, 1 ) );
        strcpy( dftStr, "WGS_84" );
        h5AttrMap_["H5R.GEO.geodetic_ellipsoid"]   = H5Attr_t( "geodetic_ellipsoid",   H5T_C_S1,          h5ScalarSpaceId_, dftStr, 16 );
    }

    virtual ~Hdf5rGeoLocAttributes() {}
};

#endif /* FRMTS_HDF5R_HDF5RGEOLOCATTRIBUTES_H_ */
