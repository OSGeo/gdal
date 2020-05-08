/*
 * HDF5RSubDataSet.h
 *
 *  Created on: May 10, 2018
 *      Author: nielson
 */

#ifndef FRMTS_HDF5R_HDF5RSUBDATASET_H_
#define FRMTS_HDF5R_HDF5RSUBDATASET_H_

#include "HDF5RDataSet.h"

class HDF5RSubDataSet: public HDF5RDataSet
{
public:
    HDF5RSubDataSet();
    virtual ~HDF5RSubDataSet();

    friend class HDF5RRasterBand;
    friend class HDF5RDataSet;

    /**
     * Required static Open method required for all GDAL drivers
     * @param gdalInfo File information class for the file to open.
     * @return On success an HDF5RDataSet super-class of GDALDataSet.
     */
    static GDALDataset* Open(GDALOpenInfo* gdalInfo);

    /**
     * Required static method required for all GDAL drivers. The identity
     * decision for HDF5-R certifies that the file is valid hdf5 using the
     * hdf5 library function H5Fis_hdf5 and contains two specific file level
     * attributes in HDF5-R: "SCID" and "SCA".  Note that this driver must
     * precede the other GDAL HDF5 drivers because their Identify() methods
     * yield false positives for HDF5-R.
     * @param gdalInfo File information class for the file to open.
     * @return 1 (true) on HDF5-R identity match, 0 otherwise.
     */
    static int Identify(GDALOpenInfo* gdalInfo);

private:

    /**
     * Read one complete frame of the source HDF5-R file into the GDAL datasubset
     * @param frameIndex Index into the HDF5-R frames from 0.
     * @param ooList Pointer to null terminated list of NAME=VALUE
     *               open options passed on the command line with -oo
     * @return true on success, false otherwise.
     */
    bool loadHdf5File( unsigned frameIndex,
                       char* const* ooList );

    /**
     * Load attributes from a local copy into a GDAL name=value list.
     * @param attributes Pointer to a superclass of Hdf5rAttributeBase which
     *                   contains a map of attributes names and values as
     *                   native types.
     * @param nvList Pointer to GDAL name=value strings. List is null terminated.
     * @return Boolean true on success, false otherwise.
     */
    bool loadGdalAttributes( const Hdf5rAttributeBase* attributes,
                             char**& nvList ) const;

    /**
     * Load frame data members into a GDAL name=value list from the
     * Hdf5rFrameData class for a specific frame number.
     * @param compound Pointer to CompoundBase.
     * @param attrIndex Attribute Index.
     * @param nvList Pointer to GDAL name=value strings. List is null terminated.
     * @return Boolean true on success, false otherwise.
     */
    bool loadGdalCompoundAttributes( const CompoundBase* compound,
                                     unsigned attrIndex,
                                     char**& nvList ) const;

    bool loadGdalTiffTimeTag( const Hdf5rFrameData::FrameData_t* frameData,
                              char**& nvList ) const;

    /**
     * Set the projection parameters for WGS-84 lat-lon
     * a.k. ESPG:4326.
     * @return OGRerr (from ogr_core.h) OGRERR_NONE is the desired result
     */
    OGRErr setWgs84OgrSpatialRef();

    /**
     * Set the projection parameters for the NSPER (Near side perspective frame)
     * @param lat  Geocentric latitude in degrees
     * @param lon  Geocentric longitude in degrees
     * @param altitude_m Altitude above the Earth in meters.
     * @return OGRerr (from ogr_core.h) OGRERR_NONE is the desired result
     */
    OGRErr setNsperOgrSpatialRef( double lat, double lon, double altitude_m );

    /**
     * Set the projection from user specification.
     * @param userStr User string in Proj format or WKT or GDAL short name
     * @return OGRerr (from ogr_core.h) OGRERR_NONE is the desired result
     */
    OGRErr setUserOgrSpatialRef( const CPLString& userStr );

    /**
     * Build the OGR spatial reference transform given a Well Known Text
     * reference system format.
     * @param ogrWkt Well Known Text reference system
     * @return The transform (use delete operator to clean up)
     *         or nullptr if unable to build it.
     */
    OGRCoordinateTransformation* buildGcpOgrXform( const std::string ogrWkt );

    /**
     * Build the Ground Control Points from the GEO grid for the projection.
     * Off-Earth points are skipped.  GCPs are saved to pasGCPList_ pointer
     * in this class.
     * @return Number of GCPs generated.
     */
    int buildGcpListFromLosGrid( const Hdf5rLosGrid_t& losGrid,
                                 int gcpMax );

private:

    // file level attributes loaded by Open()
    int32_t scid_;
    int32_t sca_;

    // pointer to GCP array and count of the number of elements
    GDAL_GCP    *pasGCPList_;
    int         nGCPCount_;

    // flag indicating if open option GCP_MAX set on command line
    // if so then suppress warning
    bool userSetGcpMax_;

    // Projection to use for GCP transformation
    //   PROJ_WGS84: Default WGS84 lat, lon coordinates
    //   PROJ_NSPER: Near-side perspective using the satPosEcf  attribute
    //   PROJ_USER:  User specified projection in 'proj' format
    enum projection_t { PROJ_WGS84, PROJ_NSPER, PROJ_USER } gcpProjection_ ;

    // User specified projection string -- in effect if
    //     gcpProjection_ == PROJ_USER
    std::string userProjectionStr_;

    // GCP Transform for specified projection -- if gcpProjection_ != PROJ_WGS84
    OGRCoordinateTransformation* gcpXform_;

    // returns map X, Y for 40 deg N, 120 W
    std::pair<double, double> earthTest( const m3d::Vector& satellite );
};

#endif /* FRMTS_HDF5R_HDF5RSUBDATASET_H_ */
