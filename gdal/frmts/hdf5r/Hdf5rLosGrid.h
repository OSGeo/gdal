/*
 * Hdf5rLosGrid.h
 *
 *  Created on: Sep 4, 2018
 *      Author: nielson
 */

#ifndef FRMTS_HDF5R_HDF5RLOSGRID_H_
#define FRMTS_HDF5R_HDF5RLOSGRID_H_

#include <limits>
#include <iostream>

#include <cstring>

#include "hdf5r.h"
#include "m3d.h"
#include "Earth.h"

class Hdf5rLosGrid_t
{
public:
    enum Status_t {UNINITIALIZED, ALL_ON_EARTH, ALL_OFF_EARTH, PARTIAL_ON_EARTH};

    // LOS grid point definition
    struct Hdf5rLosData_t
    {
        // values loaded from or to the HDF5-R file
        float ecf_X;
        float ecf_Y;
        float ecf_Z;

        // Projected X, Y (depends on map projection, can be LON-LAT or X,Y)
        // must be LON-LAT when loaded to the HDF5-R file
        float map_X;
        float map_Y;

        // auxiliary data not mapped to the HDF5-R file
        // computed Earth intersection in ECF (xyz)
        m3d::Vector geoLoc;
        bool oth_;

        /** Default Constructor **/
        Hdf5rLosData_t()
        : ecf_X( 0.0 ),
          ecf_Y( 0.0 ),
          ecf_Z( 0.0 ),
          map_X( 0.0 ),
          map_Y( 0.0 ),
          geoLoc(),
          oth_( false )
        {}
    };

    // std::vector of LOS data as a linear array
    typedef std::vector<Hdf5rLosData_t> Hdf5rLosDataArray_t;

    // initialization constants for the max/min map XY limits
    static constexpr double DMAX = std::numeric_limits<double>::max();

    /**
     * Default constructor.
     */
    Hdf5rLosGrid_t()
    : nRows_(0), nCols_(0), losDataSz_(0),
      nOnEarthPts_( 0 ),
      rowStepSize_( 0 ), colStepSize_( 0 ),
      losData_(),
      status_( UNINITIALIZED ),
      iXmin_( 0 ), iXmax_( 0 ), iYmin_( 0 ), iYmax_( 0 ),
      Xmin_( DMAX ), Xmax_( -DMAX ), Ymin_( DMAX ), Ymax_( -DMAX ),
      // avgXpixelSz_( 0.0 ), avgYpixelSz_( 0.0 ),
      satEcfMeters_(),
      invalidLatLonValue_( -9999.0 ),
      earth_( *Earth::getInstance() )
    {}

    /**
     * Construct a grid with the given dimensions.  The LOS grid initializes
     * with all 0 elements.  Usually populated with the fillGeoLocation() after
     * reading from an HDF5-R file or with buildGridfromGdalArrays() before
     * writing to an HDF5-R file.
     * @param nRows Number of rows == Y == GDAL lines
     * @param nCols Number of columns == X == GDAL rows
     * @param rowStepSz HDF5-R LOS grid step size in row dimension
     * @param colStepSz HDF5-R LOS grid step size in column dimension
     */
    Hdf5rLosGrid_t( size_t nRows,
                    size_t nCols,
                    int rowStepSz,
                    int colStepSz,
                    m3d::Vector& satEcfMeters,
                    const Earth& earth ) :
        nRows_(nRows), nCols_(nCols), losDataSz_(nRows*nCols),
        nOnEarthPts_( 0 ),
        rowStepSize_( rowStepSz ), colStepSize_( colStepSz ),
        losData_( losDataSz_ ),
        status_( UNINITIALIZED ),
        iXmin_( 0 ), iXmax_( 0 ), iYmin_( 0 ), iYmax_( 0 ),
        Xmin_( DMAX ), Xmax_( -DMAX ), Ymin_( DMAX ), Ymax_( -DMAX ),
        // avgXpixelSz_( 0.0 ), avgYpixelSz_( 0.0 ),
        satEcfMeters_( satEcfMeters ),
        invalidLatLonValue_( -9999.0 ),
        earth_( earth )
    {}

    /**
     * Prevent copies
     */
    Hdf5rLosGrid_t( const Hdf5rLosGrid_t& ) = delete;

    /**
     * No assignment
     */
    Hdf5rLosGrid_t& operator=( const Hdf5rLosGrid_t& ) = delete;

    /** Destructor */
    virtual ~Hdf5rLosGrid_t() {}

    /**
     * This is a simple validity test that verifies that the size of the
     * los array matches the product of rows and columns used to init this
     * class.
     * @return
     */
    bool isValid() const {return losData_.size() == (nRows_ * nCols_);}

    size_t getNrows() const {return nRows_;}
    size_t getNcols() const {return nCols_;}

    size_t size() const {return losDataSz_;}

    int getRowStepSize() const {return rowStepSize_;}
    int getColStepSize() const {return colStepSize_;}

    bool hasGeoLocationFilled() const {return status_ != UNINITIALIZED;}
    bool hasAllOnEarth() const {return status_ == ALL_ON_EARTH;}
    bool hasAllOffEarth() const {return status_ == ALL_OFF_EARTH;}
    bool hasSomeOnEarth() const {return (status_ == ALL_ON_EARTH || status_ == PARTIAL_ON_EARTH);}

    size_t getNumOnEarth() const {return nOnEarthPts_;}

    double getXmin() const {return Xmin_;}
    double getXmax() const {return Xmax_;}
    double getYmin() const {return Ymin_;}
    double getYmax() const {return Ymax_;}

    typedef std::pair<double, double> GeoMapXY_t;

    /**
     * Get a reference to the LOS data for a given row and column.  Bounds are
     * checked.
     * @param row same as GDAL 'line' or Y coordinate
     * @param col same as GDAL 'pixel' or X coordinate
     * @return Hdf5rLosData_t reference for that coordinate
     */
    const Hdf5rLosData_t& operator() ( size_t row, size_t col ) const
    {
        // verify arrays sizes match the LOS grid size
        if ((row >= nRows_) || (col >= nCols_))
            throw std::out_of_range( "Hdf5rLosGrid_t::operator(): requested row or column exceeds dimensions" );
        return losData_[(row * nCols_) + col];
    }

    /**
     * Get a reference to the LOS data for a given row and column.  Bounds are
     * checked.
     * @param row same as GDAL 'line' or Y coordinate
     * @param col same as GDAL 'pixel' or X coordinate
     * @return Hdf5rLosData_t reference for that coordinate
     */
    Hdf5rLosData_t& at( size_t row, size_t col )
    {
        // verify arrays sizes match the LOS grid size
        if ((row >= nRows_) || (col >= nCols_))
            throw std::out_of_range( "Hdf5rLosGrid_t::operator(): requested row or column exceeds dimensions" );
        return losData_[(row * nCols_) + col];
    }

    /**
     * Return the row number given the linear index into the LOS Grid array.
     * @param i Linear index from 0.
     * @return Row number from 0.
     */
    int rowFromIdx( int i ) const {return i / nCols_;}

    /**
     * Return the column number given the linear index into the LOS Grid array.
     * @param i Linear index from 0.
     * @return Column number from 0.
     */
    int colFromIdx( int i ) const {return i % nCols_;}

    /**
     * Given a row and column, return the linear array index into the grid.
     * @param row Row, line, y index from 0
     * @param col Column, channel, x index from 0
     * @return Linear array index
     */
    size_t getIdx( size_t row, size_t col )
    {
        return (row * nCols_) + col;
    }

    /**
     * If the LOS grid does not enclose the entire image, use this method to
     * extrapolate the missing column.  Requires at least 2 columns and
     * a valid LOS grid.
     * @return true if extrapolation was successful
     */
    bool extrapLastColumn();

    /**
     * If the LOS grid does not enclose the entire image, use this method to
     * extrapolate the missing row.  Requires at least 2 rows and
     * a valid LOS grid.
     * @return true if extrapolation was successful
     */
    bool extrapLastRow();

    /**
     * Using the observer ECF location, ecfLocMeters, and an Earth model,
     * compute the lat-lon and OTH status for each grid point LOS vector.
     */
//    bool fillGeoLocation( const m3d::Vector& ecfLocMeters,
//                          const Earth& earth );

    /**
     * Get a pointer the underlying LOS grid as a C-array.  Not a constant
     * pointer since H5Read will fill this array (for example).
     * @return Pointer to C-style LOS grid array.
     */
    Hdf5rLosData_t* getLosDataArray() {return losData_.data();}

    const Hdf5rLosData_t* getConstLosDataArray() const {return losData_.data();}

    /**
     * Build the LOS grid from arrays of lat-lon points -- as are generated
     * by GDAL transform calls.  The satellite (or observer) ECF location
     * vector and Earth lat, lon points are used to calculate the line-of-sight
     * vector or fil with -999 if GDAL status is 0.
     * @param nGridRows Number of rows in the grid.
     * @param nGridCols Number of columns in the grid.
     * @param lat Earth latitude in degrees (matching Earth model), ignored
     *            for indexes where gdalXformStatus == 0.
     * @param lon Earth longitude in degrees (matching Earth model), ignored
     *            for indexes where gdalXformStatus == 0.
     * @param gdalXformStatus 0 if lat-lon is invalid and ignored, 1 if valid
     * @return Count of valid LOS vectors built.
     */
    int buildGridfromGdalArrays( unsigned nGridRows,
                                 unsigned nGridCols,
                                 const double* lat,
                                 const double* lon,
                                 const int* gdalXformStatus );

    /**
     * Call this after loading the LOS grid to update the min/max XY
     * (lon, lat for WGS-84 projection). Initializes the min/max values to the
     * stops, even if the grid is empty.
     * @return Number of on-Earth grid points.
     */
    int summarize();

    /**
     * Recompute the LOS grid using an observer at 'satEcfMeters'
     * @param satEcfMeters Observing satellite vector.
     * @param offEarthValue The value used to flag off-Earth latitude and
     *                      longitude.
     * @return number of on-Earth points in grid
     */
    int changeObserverLocation( const m3d::Vector& satEcfMeters,
                                double offEarthValue );

    /**
     * Interpolate the latitude-longitude for a pixel coordinate.  This
     * method implements a bilinear interpolation, as recommended in the
     * HDF5R ICD, of the line-of-sight vectors within a grid area.  This
     * LOS vector is then drilled to the Earth.
     * @param xPixel Pixel x-coordinate. Also corresponds to channel, column, or
     *               detector number starting from 0.
     * @param yPixel Pixel y-coordinate. Also corresponds to line or
     *               row number starting from 0.
     * @param drillLonLat Longitude (X) and Latitude (Y) of the drill point or
     *                    -9999 for over-the-horizon.
     *                    Not set for error conditions.
     * @return 0 if drilled to the Earth, 1 if over-the-horizon, -1 if error.
     */
    int interpolate( int xPixel, int yPixel, Earth::MapXY_t* drillLonLat ) const;

    /**
     * Enclosed class for a grid tile consisting of the four corner points.
     * Principle method is testPixelOnEarth() to test if a particular
     * interior or edge point is on/off Earth.
     */
    class GridTile_t
    {
    public:
        /**
         * Grid Tile constructor
         * @param losGrid A pointer to the Hdf5rLosGrid_t containing the
         *                LOS to extract the tile from given the upper left
         *                corner in grid space (which could be any orientation
         *                on a map projection).
         * @param gridRow0 Upper-left corner row/y/line index from 0.
         * @param gridCol0 Upper-left corner column/x/detector index from 0.
         */
        GridTile_t( const Hdf5rLosGrid_t* losGrid,
                    size_t gridRow0, size_t gridCol0 );

        /**
         * Grid tile destructor
         */
        virtual ~GridTile_t() {}

        /**
         * Test if a given tile pixel is on-Earth
         * @param tileRow row/y/line index from 0 in the tile or on the edge.
         * @param tileCol column/x/detector index from 0 in the tile or on the edge.
         * @return True if on-Earth, false if off.
         */
        bool testPixelOnEarth( int tileRow, int tileCol ) const;

        /**
         * Get the summary corner point status for this tile.
         * @return Status enumeration value of ALL_ON_EARTH, ALL_OFF_EARTH,
         *                or PARTIAL_ON_EARTH
         */
        Status_t getStatus() const {return status_;}

        /**
         * Get the number of on-Earth corner points.
         * @return A value in the interval [0, 4]
         */
        int getNumOnEarth() const {return numOnEarth_;}

    private:
        Status_t status_;
        int numOnEarth_;
        const Hdf5rLosGrid_t* losGrid_;

        const Hdf5rLosGrid_t::Hdf5rLosData_t* gridpt_ul_;
        const Hdf5rLosGrid_t::Hdf5rLosData_t* gridpt_ur_;
        const Hdf5rLosGrid_t::Hdf5rLosData_t* gridpt_ll_;
        const Hdf5rLosGrid_t::Hdf5rLosData_t* gridpt_lr_;

        m3d::Vector y0_x0_;
        m3d::Vector y0_x1_;
        m3d::Vector y1_x0_;
        m3d::Vector y1_x1_;
    };

private:
    size_t nRows_;
    size_t nCols_;
    size_t losDataSz_;   // nRows * nCols
    size_t nOnEarthPts_;

    // step sizes grid vs image
    int rowStepSize_;
    int colStepSize_;

    // the LOS grid as a std::vector
    Hdf5rLosDataArray_t losData_;

    // set by fillGeoLocation()
    Status_t status_;
    int iXmin_;
    int iXmax_;
    int iYmin_;
    int iYmax_;
    double Xmin_;
    double Xmax_;
    double Ymin_;
    double Ymax_;
    // double avgXpixelSz_;
    // double avgYpixelSz_;

    m3d::Vector satEcfMeters_;

    double invalidLatLonValue_;

    const Earth& earth_;

    /**
     * Private function to update min/max for a single grid point
     * @param i point
     * @param mapXY point coordinates
     */
    void updateMinMax( int i, const GeoMapXY_t mapXY );

    /**
     * Private function that sets the on/off Earth status given the number
     * of on-Earth points.
     */
    void setStatus();

    m3d::Vector offEarthEstimate( const m3d::Vector& los ) const;
};

#endif /* FRMTS_HDF5R_HDF5RLOSGRID_H_ */
