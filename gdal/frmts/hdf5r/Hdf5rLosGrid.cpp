/*
 * Hdf5rLosGrid.cpp
 *
 *  Created on: Sep 4, 2018
 *      Author: nielson
 */
#include "cpl_error.h"
#include "Hdf5rLosGrid.h"

//******************************************************************************
// Extrapolate the last column of the LosGrid_t from the two previous columns
//******************************************************************************
bool Hdf5rLosGrid_t::extrapLastColumn()
{
    bool rc = false;

    // basic checks
    if (isValid() && (nCols_ >= 3))
    {
        // indexes of ultimate, and penultimate, and antepenultimate columns
        size_t idx2 = nCols_ - 1;
        size_t idx1 = idx2 - 1;
        size_t idx0 = idx1 - 1;

        // reference to 'this' this for using the index operator cleanly
        Hdf5rLosGrid_t& losGrid = *this;

        for (unsigned i=0; i<nRows_; ++i)
        {
            m3d::Vector v0( losGrid( i, idx0 ).ecf_X,
                            losGrid( i, idx0 ).ecf_Y,
                            losGrid( i, idx0 ).ecf_Z );
            m3d::Vector v1( losGrid( i, idx1 ).ecf_X,
                            losGrid( i, idx1 ).ecf_Y,
                            losGrid( i, idx1 ).ecf_Z );

            // linear extrapolation: v2 = v1 + (v1 - v0) == 2*v1 - v0
            m3d::Vector v2 = 2.0 * v1 - v0;
            v2.normalize();

            Hdf5rLosData_t& gridPt = losGrid.at( i, idx2 );

            gridPt.ecf_X = float( v2.i() );
            gridPt.ecf_Y = float( v2.j() );
            gridPt.ecf_Z = float( v2.k() );

            gridPt.oth_ = earth_.where( satEcfMeters_, v2, &gridPt.geoLoc );
            if (gridPt.oth_)
            {
                gridPt.map_X = float( invalidLatLonValue_ );
                gridPt.map_Y = float( invalidLatLonValue_ );
            }
            else
            {
                // function call returns (latitude, longitude) in radians
                std::pair<double, double> drillLatLon = earth_.toLatLon0( gridPt.geoLoc );

                // convert to degrees and order to (longitude, latitude) to
                // match the Map X,Y convention
                gridPt.map_X = float( drillLatLon.second * Earth::radToDeg );
                gridPt.map_Y = float( drillLatLon.first * Earth::radToDeg );
            }
        }

        rc = true;
    }
    return rc;
}

//******************************************************************************
// Extrapolate the last row of the LosGrid_t from the two previous rows
//******************************************************************************
bool Hdf5rLosGrid_t::extrapLastRow()
{
    bool rc = false;

    // basic checks
    if (isValid() && (nRows_ >= 3))
    {
        // indexes of ultimate, and penultimate, and antepenultimate rows
        size_t idx2 = nRows_ - 1;
        size_t idx1 = idx2 - 1;
        size_t idx0 = idx1 - 1;

        // reference to 'this' target for using the index operator cleanly
        Hdf5rLosGrid_t& losGrid = *this;

        for (unsigned j=0; j<nCols_; ++j)
        {
            m3d::Vector v0( losGrid( idx0, j ).ecf_X,
                            losGrid( idx0, j ).ecf_Y,
                            losGrid( idx0, j ).ecf_Z );
            m3d::Vector v1( losGrid( idx1, j ).ecf_X,
                            losGrid( idx1, j ).ecf_Y,
                            losGrid( idx1, j ).ecf_Z );

            // linear extrapolation: v2 = v1 + (v1 - v0) == 2*v1 - v0
            m3d::Vector v2 = 2.0 * v1 - v0;
            v2.normalize();

            Hdf5rLosData_t& gridPt = losGrid.at( idx2, j );

            gridPt.ecf_X = float( v2.i() );
            gridPt.ecf_Y = float( v2.j() );
            gridPt.ecf_Z = float( v2.k() );

            gridPt.oth_ = earth_.where( satEcfMeters_, v2, &gridPt.geoLoc );
            if (gridPt.oth_)
            {
                gridPt.map_X = float( invalidLatLonValue_ );
                gridPt.map_Y = float( invalidLatLonValue_ );
            }
            else
            {
                // function call returns (latitude, longitude) in radians
                std::pair<double, double> drillLatLon = earth_.toLatLon0( gridPt.geoLoc );

                // convert to degrees and order to (longitude, latitude) to
                // match the Map X,Y convention
                gridPt.map_X = float( drillLatLon.second * Earth::radToDeg );
                gridPt.map_Y = float( drillLatLon.first * Earth::radToDeg );
            }
        }

        rc = true;
    }
    return rc;
}

//******************************************************************************
// Compute the geo-location for each LOS vector
//    - intersection of the LOS vector with the ellipsoidal Earth model
//    - sets oth (Over-the-Horizon) if off-earth, then location is the
//      intersection with the normal to the tangent plane
//******************************************************************************
//bool Hdf5rLosGrid_t::fillGeoLocation( const m3d::Vector& ecfRef,
//                                      const Earth& earth )
//{
//    bool rc = false;
//
//    // Must have LOS grid data to convert
//    if (isValid() && (losDataSz_ > 0))
//    {
//        rc = true;
//
//        // process every LOS grid point
//        size_t nOnEarth = 0;
//        size_t nOffEarth = 0;
//        for (unsigned i=0; i<losDataSz_; ++i)
//        {
//            // drill LOS vector to the ellipsoidal Earth model
//            Hdf5rLosData_t& losData = losData_[i];
//            losData.oth_ = earth.where( ecfRef,
//                                        m3d::Vector( losData.ecf_X,
//                                                     losData.ecf_Y,
//                                                     losData.ecf_Z ),
//                                        &losData.geoLoc );
//
////            compare drilled lat-lon to values passed in
////            Earth::MapXY_t chkLonLat = earth.toLatLon0( losData.geoLoc );
////
////            CPLDebug( HDF5R_DEBUG_STR, "drilled lat, lon: %f, %f   in %f, %f",
////                      chkLonLat.second * Earth::radToDeg, chkLonLat.first * Earth::radToDeg,
////                      losData.map_X, losData.map_Y );
//
//            // init map XY to 0,0
//            Earth::MapXY_t mapXY( 0.0, 0.0 );
//
//            // if off-Earth, stick with 0,0 otherwise do the Orthographic
//            // transform
//            if (losData.oth_)
//                ++nOffEarth;
//            else
//            {
//                ++nOnEarth;
//
//                // Do the orthoGraphic transform from the drilled location
// //               mapXY = earth.orthoGrapicMapXY( losData.geoLoc );
//
//                // Do the orthographic transform from the input lat-lon
//                mapXY = earth.orthoGrapicMapXY( Earth::degToRad * losData.map_Y,
//                                                Earth::degToRad * losData.map_X );
//
//                // update X and Y max, min and their indexes
//                updateMinMax( i, mapXY );
//            }
//            // Set the values in the LOS data struct
//            losData.map_X = float( mapXY.first );
//            losData.map_Y = float( mapXY.second );
//        }
//
//        // set grid status based on counts
//        if (nOffEarth == losDataSz_)
//            status_ = ALL_OFF_EARTH;
//        else
//        {
//            if (nOnEarth == losDataSz_)
//                status_ = ALL_ON_EARTH;
//            else
//                status_ = PARTIAL_ON_EARTH;
//
//            // calculate average pixel sizes (in Earth::units)
//            avgXpixelSz_ = std::fabs( (Xmax_ - Xmin_) /
//                    double((colFromIdx( iXmax_ ) - colFromIdx( iXmin_ )) * colStepSize_) );
//            avgYpixelSz_ = std::fabs( (Ymax_ - Ymin_) /
//                    double((rowFromIdx( iYmax_ ) - rowFromIdx( iYmin_ )) * rowStepSize_) );
//
//            CPLDebug( HDF5R_DEBUG_STR, "LosGrid_t::fillGeoLocation() Xnum=%f Xden=%f",
//                    (Xmax_ - Xmin_), double((colFromIdx( iXmax_ ) - colFromIdx( iXmin_ )) * colStepSize_) );
//
//            // uncomment for unit test
//            CPLDebug( HDF5R_DEBUG_STR, "LosGrid_t::fillGeoLocation() onEarth: %ld off %ld total %ld",
//                      nOnEarth, nOffEarth, losDataSz_ );
//            CPLDebug( HDF5R_DEBUG_STR, "LosGrid_t::fillGeoLocation() XstepSz=%d YstepSz=%d",
//                      colStepSize_, rowStepSize_ );
//            CPLDebug( HDF5R_DEBUG_STR, "LosGrid_t::fillGeoLocation() Xmax=%f iXmax=%d col=%d",
//                      Xmax_, iXmax_, colFromIdx( iXmax_ ) );
//            CPLDebug( HDF5R_DEBUG_STR, "                                           Xmin=%f iXmin=%d col=%d",
//                      Xmin_, iXmin_, colFromIdx( iXmin_ ) );
//            CPLDebug( HDF5R_DEBUG_STR, "                                           Ymax=%f iYmax=%d row=%d",
//                      Ymax_, iYmax_, rowFromIdx( iYmax_ ) );
//            CPLDebug( HDF5R_DEBUG_STR, "                                           Ymin=%f iYmin=%d row=%d",
//                      Ymin_, iYmin_, rowFromIdx( iYmin_ ) );
//        }
//    }
//
//    return rc;
//}

//******************************************************************************
// Scan the LOS grid points and update min-max values
//******************************************************************************
int Hdf5rLosGrid_t::summarize()
{
    iXmin_ = iXmax_ = iYmin_ = iYmax_ = 0;
    Xmin_ = DMAX;
    Xmax_ = -DMAX;
    Ymin_ = DMAX;
    Ymax_ = -DMAX;

    nOnEarthPts_ = 0;

    // Must have LOS grid data to convert
    if (isValid() && (losDataSz_ > 0))
    {
        // iterate over each grid point
        for (unsigned i=0; i<losDataSz_; ++i)
        {
            // reference to current grid point to populate
            Hdf5rLosData_t& losData = losData_[i];

            // set OTH state based on latitude within range
            losData.oth_ = (losData.map_Y > 90.0) || (losData.map_Y < -90.0);

            // if below the horizon, then update
            if (!losData.oth_)
            {
                updateMinMax( i, GeoMapXY_t( losData.map_X, losData.map_Y ) );
                ++nOnEarthPts_;
            }
        }

        // set on/off Earth status
        setStatus();
    }

    return (int)nOnEarthPts_;
}

//******************************************************************************
// Update the Min and Max bounds for a single coordinate
//******************************************************************************
void Hdf5rLosGrid_t::updateMinMax( int i, const GeoMapXY_t mapXY )
{
    if (mapXY.first > Xmax_)
    {
        Xmax_ = mapXY.first;
        iXmax_ = i;
    }
    else if (mapXY.first < Xmin_)
    {
        Xmin_ = mapXY.first;
        iXmin_ = i;
    }
    if (mapXY.second > Ymax_)
    {
        Ymax_ = mapXY.second;
        iYmax_ = i;
    }
    else if (mapXY.second < Ymin_)
    {
        Ymin_ = mapXY.second;
        iYmin_ = i;
    }
}

//******************************************************************************
// Set the on/off Earth status
//******************************************************************************
void Hdf5rLosGrid_t::setStatus()
{
    if (nOnEarthPts_ == losDataSz_)
        status_ = ALL_ON_EARTH;
    else if (ALL_ON_EARTH == 0)
        status_ = ALL_OFF_EARTH;
    else
        status_ = PARTIAL_ON_EARTH;
}

//******************************************************************************
// Populate the LOS grid given a matching set of Earth location
// arrays in a format compatible with GDAL transform outputs
//******************************************************************************
int Hdf5rLosGrid_t::buildGridfromGdalArrays( unsigned nGridRows,
                                             unsigned nGridCols,
                                             const double* lat,
                                             const double* lon,
                                             const int* gdalXformStatus )
{
    nOnEarthPts_ = 0;

    // verify arrays sizes match the LOS grid size
    if (losDataSz_ != losData_.size())
        throw std::length_error( "buildGridfromGdalArrays: losDataSz_ != vector size()." );

    // iterate over each grid point
    for (unsigned iRow=0; iRow<nGridRows; ++iRow)
    {
        for (unsigned iCol=0; iCol<nGridCols; ++iCol)
        {
            // reference to current grid point to populate
            Hdf5rLosData_t& losData = at( iRow, iCol );

            // if status good
            if (*gdalXformStatus == 1)
            {
                losData.map_X = float( *lon );
                losData.map_Y = float( *lat );

                // ECEF Earth and OTH flag
                losData.geoLoc = earth_.toEcef( Earth::degToRad * *lat,
                                                Earth::degToRad * *lon );

                // LOS unit vector
                m3d::Vector los = losData.geoLoc - satEcfMeters_;
                los.normalize();

                // test if LOS is to near side of Earth -- dot product will be
                // negative (if GCP is from polynomial extrapolation it can
                // map to a non-visible location.
                losData.oth_ = (( los * losData.geoLoc ) > 0.0);
                if (losData.oth_)
                {
                    // Estimate an off-Earth LOS vector from the
                    // behind-the-Earth vector
                    los = offEarthEstimate( los );
                    losData.ecf_X = float( los.i() );
                    losData.ecf_Y = float( los.j() );
                    losData.ecf_Z = float( los.k() );

                    // Set the off-Earth lat-lon values.
                    losData.map_X = losData.map_Y = float( invalidLatLonValue_ );
                }
                else // on-earth
                {
                    losData.ecf_X = float( los.i() );
                    losData.ecf_Y = float( los.j() );
                    losData.ecf_Z = float( los.k() );

                    // update X and Y max, min and their indexes
                    updateMinMax( getIdx( iRow, iCol ), GeoMapXY_t( *lon, *lat ) );

                    ++nOnEarthPts_;
                }

                // debug first point
                if ((iRow == 0) && (iCol == 0))
                {
                    CPLDebug( HDF5R_DEBUG_STR, "Hdf5rLosGrid_t::buildGridfromGdalArrays"
                              "Input first pt lat-lon: %f %f",
                              losData.map_Y, losData.map_X );
                    m3d::Vector drillEcf;
                    earth_.where( satEcfMeters_,
                                  m3d::Vector( losData.ecf_X, losData.ecf_Y, losData.ecf_Z ),
                                  &drillEcf );
                    Earth::MapXY_t drillLatLon = earth_.toLatLon0( drillEcf );
                    CPLDebug( HDF5R_DEBUG_STR, "Hdf5rLosGrid_t::buildGridfromGdalArrays"
                              "       drilled lat-lon: %f %f",
                              Earth::radToDeg * drillLatLon.second,
                              Earth::radToDeg * drillLatLon.first );
                }
            }
            else
            {
                //  losData inits to all zeroes so only need to set lat, lon here
                losData.map_X = float( invalidLatLonValue_ );
                losData.map_Y = float( invalidLatLonValue_ );
            }
            ++lat;
            ++lon;
            ++gdalXformStatus;
        }
    }

    // set on/off Earth status
    setStatus();

    return (int)nOnEarthPts_;
}

//******************************************************************************
// Recompute the grid give a new observer vector.  Recomputes the LOS vector for
//  each grid point.  Latitude and longitude are unchanged unless the point
//  is behind the Earth.  In that case latitude and longitude are changed to
//  the off-Earth value.
//******************************************************************************
int Hdf5rLosGrid_t::changeObserverLocation( const m3d::Vector& satEcfMeters,
                                            double offEarthValue )
{
    satEcfMeters_ = satEcfMeters;
    nOnEarthPts_ = 0;

    // Must have LOS grid data to convert
    if (isValid() && (losDataSz_ > 0))
    {
        // for each grid point
        for (unsigned i=0; i<losDataSz_; ++i)
        {
            // reference to current grid point to process
            Hdf5rLosData_t& losData = losData_[i];

            // can only process points that start on-earth
            if (!losData.oth_)
            {
                // ECEF location of point
                m3d::Vector ptEcf = earth_.toEcef( Earth::degToRad * losData.map_Y,
                                                   Earth::degToRad * losData.map_X,
                                                   0.0 );

                // new LOS vector
                m3d::Vector los = ptEcf - satEcfMeters;
                los.normalize();

                // test if LOS is to near side of Earth -- dot product will be
                // negative
                if ((los * ptEcf ) < 0.0)
                {
                    // set the new LOS vector, Lat-Lon is unchanged
                    losData.ecf_X = float( los.i() );
                    losData.ecf_Y = float( los.j() );
                    losData.ecf_Z = float( los.k() );
                    ++nOnEarthPts_;
                }
                else
                {
                    // set oth flag and invalid lat-long values
                    // Estimate an off-Earth LOS vector from the
                    // behind-the-Earth vector
                    los = offEarthEstimate( los );
                    losData.ecf_X = float( los.i() );
                    losData.ecf_Y = float( los.j() );
                    losData.ecf_Z = float( los.k() );

                    // Set the OTH flag and the off-Earth lat-lon values.
                    losData.map_X = losData.map_Y = float( offEarthValue );
                    losData.oth_ = true;
                }
            }

        }

        // set on/off Earth status
        setStatus();
    }

    return (int)nOnEarthPts_;
}

//******************************************************************************
// Determine a representative off-Earth Line-of-Sight vector from an input
// LOS vector that points to a location behind the Earth
//******************************************************************************
m3d::Vector Hdf5rLosGrid_t::offEarthEstimate( const m3d::Vector& los ) const
{
    // build direction cosine matrix for nadir pointing attitude reference
    // frame
    m3d::Vector ecfZcrossSat = crossprod( m3d::Vector( 0.0, 0.0, 1.0 ), satEcfMeters_ );

    m3d::Matrix ecfToArfXform( ecfZcrossSat.getUnitVector(),
                               crossprod( ecfZcrossSat, satEcfMeters_).getUnitVector(),
                               -satEcfMeters_.getUnitVector() );

    // transform ECF LOS to ARF
    m3d::Vector arfLos = ecfToArfXform * los;

    // calculate az-el for the original LOS
    double az = std::atan2( arfLos.j(), arfLos.i() );
    double el = std::acos( arfLos.k() );

    // horizon elevation for spherical Earth
    double horizonEl = std::asin( earth_.getEquatorialRadius() / satEcfMeters_.magnitude() );

    // delta elevation below the horizon
    double deltaEl = horizonEl - el;

    // we only want to calculate an estimate if already below the horizon
    if ( deltaEl > 0)
    {
        el = horizonEl + deltaEl;

        double sinEl = std::sin( el );

        m3d::Vector newLos =  ecfToArfXform.transpose() * m3d::Vector( std::cos( az ) * sinEl,
                                                                       std::sin( az ) * sinEl,
                                                                       std::cos( el ) );

        // sanity check that this point is OTH
        m3d::Vector drillEcf;
        bool oth = earth_.where( satEcfMeters_, newLos, &drillEcf );
        if (!oth)
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Hdf5rLosGrid_t::offEarthEstimate() Error estimating off-Earth point" );

        return newLos;
    }
    else
    {
        // sanity check that this point is OTH
        m3d::Vector drillEcf;
        bool oth = earth_.where( satEcfMeters_, los, &drillEcf );
        if (!oth)
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Hdf5rLosGrid_t::offEarthEstimate() Error estimating off-Earth point" );

        // already above, just return the input
        return los;
    }
}

//******************************************************************************
// LOS grid bilinear interpolation
//******************************************************************************
int Hdf5rLosGrid_t::interpolate( int iRowPixel, int iColPixel,
                                 Earth::MapXY_t* drillLonLat ) const
{
    static bool onetime = true;

    // (x,y) grid location of pixel is factor of step-size
    double xGrid = double(iColPixel) / double(colStepSize_);
    double yGrid = double(iRowPixel) / double(rowStepSize_);

    // lower bound grid coordinate
    int xi = int( std::floor( xGrid ) );
    int yi = int( std::floor( yGrid ) );

    // if either bound is at the upper limit (rare case) then reduce by one
    if (xi == (int(nCols_)-1))
        --xi;
    if (yi == (int(nRows_)-1))
        --yi;

    // Reference to each grid corner
    const Hdf5rLosGrid_t& losGrid = *this;

    try
    {
        const Hdf5rLosData_t& gridpt0 = losGrid( yi, xi );
        const Hdf5rLosData_t& gridpt1 = losGrid( yi, xi+1 );
        const Hdf5rLosData_t& gridpt2 = losGrid( yi+1, xi );
        const Hdf5rLosData_t& gridpt3 = losGrid( yi+1, xi+1 );

        if (onetime)
        {
            CPLDebug( HDF5R_DEBUG_STR, "Hdf5rLosGrid_t::interpolate: one-time (first call) grid corners:\n"
                      "%9.4f %9.4f    %9.4f %9.4f\n%9.4f %9.4f    %9.4f %9.4f",
                      gridpt0.map_Y, gridpt0.map_X, gridpt1.map_Y, gridpt1.map_X,
                      gridpt2.map_Y, gridpt2.map_X, gridpt3.map_Y, gridpt3.map_X );
            onetime = false;
        }

        // short circuit if all corners are OTH
        if (gridpt0.oth_ && gridpt1.oth_ && gridpt2.oth_ && gridpt3.oth_)
        {
            drillLonLat->first = drillLonLat->second = float( invalidLatLonValue_ );
            return 1;
        }

        // LOS at each grid corner
        m3d::Vector y0_x0( gridpt0.ecf_X, gridpt0.ecf_Y, gridpt0.ecf_Z );
        m3d::Vector y0_x1( gridpt1.ecf_X, gridpt1.ecf_Y, gridpt1.ecf_Z );
        m3d::Vector y1_x0( gridpt2.ecf_X, gridpt2.ecf_Y, gridpt2.ecf_Z );
        m3d::Vector y1_x1( gridpt3.ecf_X, gridpt3.ecf_Y, gridpt3.ecf_Z );

        // interpolation factors
        double f = xGrid - double( xi );
        double g = yGrid - double( yi );

        // bilinear interpolation of the LOS vector with re-normalization to ensure
        // a unit vector result
        m3d::Vector za = (1.0 - f) * y0_x0 + f * y0_x1;
        m3d::Vector zb = (1.0 - f) * y1_x0 + f * y1_x1;
        m3d::Vector z = (1.0 - g) * za + g * zb;
        z.normalize();

        // drill to the Earth
        m3d::Vector drillEcf;
        bool oth = earth_.where( satEcfMeters_, z, &drillEcf );
        if (oth)
        {
            drillLonLat->first = drillLonLat->second = float( invalidLatLonValue_ );
        }
        else
        {
            // function call returns (latitude, longitude) in radians
            std::pair<double, double> latlon = earth_.toLatLon0( drillEcf );

            // convert to degrees and order to (longitude, latitude) to
            // match the Map X,Y convention
            drillLonLat->first = latlon.second * Earth::radToDeg;
            drillLonLat->second = latlon.first * Earth::radToDeg;
        }

        return oth;
    }
    catch (std::exception& ex)
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Hdf5rLosGrid_t::interpolate() exception: %s", ex.what() );

        return -1;
    }
}

//******************************************************************************
// Grid Tile constructor
//   Give the upper-left corner of the grid, collect the four corner
//   points their LOS vectors and the corner point on/off Earth summary status
//******************************************************************************
Hdf5rLosGrid_t::GridTile_t::GridTile_t( const Hdf5rLosGrid_t* losGrid,
                                        size_t gridRow0, size_t gridCol0 )
: status_( Hdf5rLosGrid_t::UNINITIALIZED ),
  numOnEarth_( 0 ),
  losGrid_( losGrid ),
  gridpt_ul_( &(*losGrid_)( gridRow0,   gridCol0   ) ),
  gridpt_ur_( &(*losGrid_)( gridRow0,   gridCol0+1 ) ),
  gridpt_ll_( &(*losGrid_)( gridRow0+1, gridCol0   ) ),
  gridpt_lr_( &(*losGrid_)( gridRow0+1, gridCol0+1 ) ),
  y0_x0_( gridpt_ul_->ecf_X, gridpt_ul_->ecf_Y, gridpt_ul_->ecf_Z ) ,
  y0_x1_( gridpt_ur_->ecf_X, gridpt_ur_->ecf_Y, gridpt_ur_->ecf_Z ) ,
  y1_x0_( gridpt_ll_->ecf_X, gridpt_ll_->ecf_Y, gridpt_ll_->ecf_Z ) ,
  y1_x1_( gridpt_lr_->ecf_X, gridpt_lr_->ecf_Y, gridpt_lr_->ecf_Z )
{
    numOnEarth_ = int(!gridpt_ul_->oth_) + int(!gridpt_ur_->oth_)
                    + int(!gridpt_ll_->oth_) + int(!gridpt_lr_->oth_);

    // Determine summary status
    if (numOnEarth_ == 0)
    {
        status_ = Hdf5rLosGrid_t::ALL_OFF_EARTH;
    }

    else if (numOnEarth_ == 4)
    {
        status_ = Hdf5rLosGrid_t::ALL_ON_EARTH;
    }
    else
        status_ = Hdf5rLosGrid_t::PARTIAL_ON_EARTH;
}

//******************************************************************************
// Grid Tile method to test if a given tile (row,col) is ON or OFF Earth
//******************************************************************************
bool Hdf5rLosGrid_t::GridTile_t::testPixelOnEarth( int tileRow, int tileCol ) const
{
    // interpolation factors
    double f = double( tileCol ) / double( losGrid_->getColStepSize() );
    double g = double( tileRow ) / double( losGrid_->getRowStepSize() );

    // bilinear interpolation of the LOS vector with re-normalization to ensure
    // a unit vector result
    m3d::Vector za = (1.0 - f) * y0_x0_ + f * y0_x1_;
    m3d::Vector zb = (1.0 - f) * y1_x0_ + f * y1_x1_;
    m3d::Vector z = (1.0 - g) * za + g * zb;
    z.normalize();

    // drill to the Earth, where() returns true if off-earth, so return
    // the complement of that
    m3d::Vector drillEcf;
    bool oth = losGrid_->earth_.where( losGrid_->satEcfMeters_, z, &drillEcf );

//    std::pair<double, double> ll = losGrid_->earth_.toLatLon0( drillEcf );
//    double lat = ll.first * Earth::radToDeg;
//    double lon = ll.second * Earth::radToDeg;
//    std::cout << "row,col: " << tileRow << ", " << tileCol << " lat,lon: "
//            << lat << ", " << lon << std::endl;

    return !oth;
}
