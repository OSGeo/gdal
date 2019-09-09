/*
 * Hdf5rFileAttributes.h
 *
 *  Created on: Sep 10, 2018
 *      Author: nielson
 */

#ifndef FRMTS_HDF5R_HDF5RFILEATTRIBUTES_H_
#define FRMTS_HDF5R_HDF5RFILEATTRIBUTES_H_

#include "Hdf5rAttributeBase.h"

class Hdf5rFileAttributes : public Hdf5rAttributeBase
{
public:

    Hdf5rFileAttributes()
    {
        h5AttrMap_["H5R.SCID"]                 = H5Attr_t( "SCID",                 H5T_NATIVE_INT32,  h5ScalarSpaceId_, int32_t( 0 ) );
        h5AttrMap_["H5R.SCA"]                  = H5Attr_t( "SCA",                  H5T_NATIVE_INT32,  h5ScalarSpaceId_, int32_t( 0 ) );
        char* dftStr = static_cast<char*>( calloc( 24, 1 ) );
        strcpy( dftStr, "2018_100_36000 " );
        h5AttrMap_["H5R.minTimeStamp"]         = H5Attr_t( "minTimeStamp",         H5T_C_S1,          h5ScalarSpaceId_, dftStr, 16 );
        h5AttrMap_["H5R.minYear"]              = H5Attr_t( "minYear",              H5T_NATIVE_INT32,  h5ScalarSpaceId_, int32_t( 0 ) );
        h5AttrMap_["H5R.minDay"]               = H5Attr_t( "minDay",               H5T_NATIVE_INT32,  h5ScalarSpaceId_, int32_t( 0 ) );
        h5AttrMap_["H5R.minSeconds"]           = H5Attr_t( "minSeconds",           H5T_NATIVE_DOUBLE, h5ScalarSpaceId_, 0.0 );
        dftStr = static_cast<char*>( calloc( 24, 1 ) );
        strcpy( dftStr, "2018_100_36000 " );
        h5AttrMap_["H5R.maxTimeStamp"]         = H5Attr_t( "maxTimeStamp",         H5T_C_S1,          h5ScalarSpaceId_, dftStr, 16 );
        h5AttrMap_["H5R.maxYear"]              = H5Attr_t( "maxYear",              H5T_NATIVE_INT32,  h5ScalarSpaceId_, int32_t( 0 ) );
        h5AttrMap_["H5R.maxDay"]               = H5Attr_t( "maxDay",               H5T_NATIVE_INT32,  h5ScalarSpaceId_, int32_t( 0 ) );
        h5AttrMap_["H5R.maxSeconds"]           = H5Attr_t( "maxSeconds",           H5T_NATIVE_DOUBLE, h5ScalarSpaceId_, 0.0 );
        h5AttrMap_["H5R.numberOfFrames"]       = H5Attr_t( "numberOfFrames",       H5T_NATIVE_INT32,  h5ScalarSpaceId_, int32_t( 1 ) );
        h5AttrMap_["H5R.minLatitude"]          = H5Attr_t( "minLatitude",          H5T_NATIVE_DOUBLE, h5ScalarSpaceId_, 0.0 );
        h5AttrMap_["H5R.maxLatitude"]          = H5Attr_t( "maxLatitude",          H5T_NATIVE_DOUBLE, h5ScalarSpaceId_, 0.0 );
        h5AttrMap_["H5R.minLongitude"]         = H5Attr_t( "minLongitude",         H5T_NATIVE_DOUBLE, h5ScalarSpaceId_, 0.0 );
        h5AttrMap_["H5R.maxLongitude"]         = H5Attr_t( "maxLongitude",         H5T_NATIVE_DOUBLE, h5ScalarSpaceId_, 0.0 );
        h5AttrMap_["H5R.minCalIntensity"]      = H5Attr_t( "minCalIntensity",      H5T_NATIVE_INT32,  h5ScalarSpaceId_, int32_t( 0 ) );
        h5AttrMap_["H5R.maxCalIntensity"]      = H5Attr_t( "maxCalIntensity",      H5T_NATIVE_INT32,  h5ScalarSpaceId_, int32_t( 0 ) );
        h5AttrMap_["H5R.linesReversed"]        = H5Attr_t( "linesReversed",        H5T_NATIVE_INT32,  h5ScalarSpaceId_, int32_t( 0 ) );
        h5AttrMap_["H5R.chansReversed"]        = H5Attr_t( "chansReversed",        H5T_NATIVE_INT32,  h5ScalarSpaceId_, int32_t( 0 ) );
        h5AttrMap_["H5R.repositoryVerNum"]     = H5Attr_t( "repositoryVerNum",     H5T_NATIVE_FLOAT,  h5ScalarSpaceId_, 2.1f );
        h5AttrMap_["H5R.LOS_degraded"]         = H5Attr_t( "LOS_degraded",         H5T_NATIVE_INT32,  h5ScalarSpaceId_, int32_t( 0 ) );
        h5AttrMap_["H5R.LOS_failed"]           = H5Attr_t( "LOS_failed",           H5T_NATIVE_INT32,  h5ScalarSpaceId_, int32_t( 0 ) );
        h5AttrMap_["H5R.errorsDetectedCt"]     = H5Attr_t( "errorsDetectedCt",     H5T_NATIVE_INT32,  h5ScalarSpaceId_, int32_t( 0 ) );
        h5AttrMap_["H5R.offEarthDiscardCt"]    = H5Attr_t( "offEarthDiscardCt",    H5T_NATIVE_INT32,  h5ScalarSpaceId_, int32_t( 0 ) );
        h5AttrMap_["H5R.flowControlFrameCt"]   = H5Attr_t( "flowControlFrameCt",   H5T_NATIVE_INT32,  h5ScalarSpaceId_, int32_t( 0 ) );
        dftStr = static_cast<char*>( calloc( 24, 1 ) );
        strcpy( dftStr, "NO ERRORS DETECTED" );
        h5AttrMap_["H5R.errorsDetectedList"]   = H5Attr_t( "errorsDetectedList",   H5T_C_S1,          h5ScalarSpaceId_, dftStr );
        h5AttrMap_["H5R.imageStatus"]          = H5Attr_t( "imageStatus",          H5T_NATIVE_UINT64, h5ScalarSpaceId_, uint64_t( 0 ) );
        h5AttrMap_["H5R.fullRangeCalibration"] = H5Attr_t( "fullRangeCalibration", H5T_NATIVE_INT32,  h5ScalarSpaceId_, int32_t( 1 ) );
    }

    virtual ~Hdf5rFileAttributes() {}
};

#endif /* FRMTS_HDF5R_HDF5RFILEATTRIBUTES_H_ */
