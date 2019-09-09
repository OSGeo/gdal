/*
 * earth.h
 *
 *  Created on: Jul 3, 2018
 *      Author: nielson
 */

#ifndef FRMTS_HDF5R_EARTH_H_
#define FRMTS_HDF5R_EARTH_H_

#include <string>
#include <vector>
#include <cmath> // for M_PI

#include "m3d.h"

/**
 * @brief Earth model container class.
 *
 * This class provide for both a singleton instance for application use with
 * static methods and for ad-hoc Earth models outside of the singleton instance.
 * For the singleton instance it is best to establish it early in the main
 * program to ensure that another constructor doesn't construct the default
 * first.  The default singleton and ad-hoc instances use the WGS-84 Earth model.
 */
class Earth
{
public:
    /**
     * Multiplicative conversion constants from degrees to/from radians.
     */
    static constexpr double radToDeg = 180.0 / M_PI;
    static constexpr double degToRad = M_PI / 180.0;

    /**
     * Constants in the default units (meters).
     */
    static constexpr double WGS84_RE_METERS = 6378137.0;
    static constexpr double WGS84_F = 1.0 / 298.257223563;
    static constexpr double GEO_SYNC_RADIUS_METERS = 42164e3;
    static constexpr double GEO_SYNC_ALTITUDE_METERS = GEO_SYNC_RADIUS_METERS - WGS84_RE_METERS;

    /**
     * Construct a WGS-84 Earth model with units of meters.
     */
    Earth() :
        re_( WGS84_RE_METERS ),
        f_( WGS84_F ),
        units_( "meters" ),
        one_minus_f_( 1.0 - f_ ),
        rp_( re_ * one_minus_f_ ),
        one_minus_f_sq_( one_minus_f_ * one_minus_f_ ),
        re_sq_( re_ * re_ ),
        e_sq_( (2.0 - f_) * f_),
        orthoLat0_( 999.0 ),
        orthoLon0_( 0.0 ),
        sinOrthoLat0_( 0.0 ),
        cosOrthoLat0_( 1.0 )
    {
    }

    /**
     * Construct an ellipsoidal Earth model with specific values for equatorial
     * radius and flattening factors.
     * @param re Earth radius
     * @param f Ellipsoid flattening factor
     * @param units String containing units used for the Earth radius.
     */
    Earth( double re, double f, std::string units ) :
        re_( re ),
        f_( f ),
        units_( units ),
        one_minus_f_( 1.0 - f_ ),
        rp_( re_ * one_minus_f_ ),
        one_minus_f_sq_( one_minus_f_ * one_minus_f_ ),
        re_sq_( re_ * re_ ),
        e_sq_( (2.0 - f_) * f_),
        orthoLat0_( 0.0 ),
        orthoLon0_( 0.0 ),
        sinOrthoLat0_( 0.0 ),
        cosOrthoLat0_( 1.0 )
    {
    }

    /**
     * Destructor
     */
    virtual ~Earth()
    {
    }

    /**
     * Get, or create the instance if it does not yet exist,
     * for the singleton instance of this class. This should be called early
     * in application initialization to ensure the intended model is created.
     * @return Pointer to the single instance.
     */
    static const Earth* getInstance()
    {
        if (!appInstance_)
            appInstance_ = new Earth();
        return appInstance_;
    }

    /**
     * Get, or create the singleton instance if it does not yet exist,
     * for this class. This should be called early
     * in application initialization to ensure the intended model is created.
     * @note If the model already exists when this method is called, it will
     *       throw an exception because it may not match the calling parameters.
     * @return Pointer to the single instance.
     */
    static const Earth* getInstance( double re, double f, std::string units )
    {
        if (appInstance_)
            throw std::runtime_error( "Earth::getInstance called with"
                    "model parameters when an instance already exists." );
        else
            appInstance_ = new Earth( re, f, units );
        return appInstance_;
    }

    /**
     * Destroy the singleton application instance if it exists. Applications
     * should call this at shutdown to ensure a clean memory report.
     */
    static void rmInstance()
    {
        delete appInstance_;
        appInstance_ = nullptr;
    }

    /**
     * This method returns 'where' a line-of-sight vector from the specified
     * starting point intersects the Earth ellipsoid.
     * @param vectorBaseEcf The ECF location of the line-of-sight vector
     *                      origin.  Must be in the same units as the Earth
     *                      model.
     * @param vectorDirectionEcf  Line-of-sight unit vector parallel in the
     *                            ECF frame, originating at the vectorBaseEcf.
     * @param ecfPoint Pointer to an m3d::Vector to load with the ECF
     *                 intersection with the ellipsoidal Earth model, if
     *                 off-earth, a point on the constant LOS sphere with
     *                 radius equal to the LOS tangent point.
     * @return True if point is over-the-horizon, false if on-Earth.
     */
    bool where( const m3d::Vector& vectorBaseEcf,
                const m3d::Vector& vectorDirectionEcf,
                m3d::Vector* ecfPoint ) const;

    /**
     * This static method uses the singleton instance to compute
     * where a line-of-sight vector from the specified
     * starting point intersects the Earth ellipsoid.
     * @param vectorBaseEcf The ECF location of the line-of-sight vector
     *                      origin.  Must be in the same units as the Earth
     *                      model.
     * @param vectorDirectionEcf  Line-of-sight unit vector parallel in the
     *                            ECF frame, originating at the vectorBaseEcf.
     * @param ecfPoint Pointer to an m3d::Vector to load with the ECF
     *                 intersection with the ellipsoidal Earth model, if
     *                 off-earth, a point on the constant LOS sphere with
     *                 radius equal to the LOS tangent point.
     * @return True if point is over-the-horizon, false if on-Earth.
     */
    static bool drill( const m3d::Vector& vectorBaseEcf,
                       const m3d::Vector& vectorDirectionEcf,
                       m3d::Vector* ecfPoint )
    {
        return getInstance()->where( vectorBaseEcf, vectorDirectionEcf, ecfPoint );
    }

    /**
     * This static method returns the geocentric latitude and longitude, in
     * radians, of an ECF vector (does not need to be a unit vector).
     * @param ecf Vector in the ECF frame.
     * @return A std::pair containing geocentric latitude and longitude in radians.
     */
    static std::pair<double, double> toGeocentricLatLon( m3d::Vector ecf )
    {
        ecf.normalize();
        return std::pair<double, double>( std::asin( ecf.k() ),
                                          std::atan2( ecf.j(), ecf.i() ) );
    }

    /**
     * This method returns the geodetic latitude and longitude, in
     * radians, of an ECF vector (does not need to be a unit vector).
     * @note This method requires knowledge that the specified input vector
     *       is known to have zero altitude and therefore is on the Earth
     *       ellipsoid (hence to 0 at the end of the method name). Otherwise
     *       use the more general method, toLatLonAlt().
     * @param ecf Vector in the ECF frame.
     * @return A std::pair containing latitude and longitude in radians.
     */
    std::pair<double, double> toLatLon0( m3d::Vector ecf ) const
    {
        ecf.normalize();
        double geoCentricLat = std::asin( ecf.k());
        return std::pair<double, double>( std::atan( (1.0/one_minus_f_sq_) * std::tan( geoCentricLat ) ),
                                          std::atan2( ecf.j(), ecf.i() ) );
    }

    /**
     * This method returns the geodetic latitude and longitude, in
     * radians, and altitude in units of the Earth model of an ECF vector
     * (does not need to be a unit vector).
     * @param ecf Vector in the ECF frame.
     * @return A std::vector containing, in order, latitude and longitude in
     *         radians and altitude in Earth::units.
     */
    std::vector<double> toLatLonAlt( const m3d::Vector& ecf ) const;

    /**
     * This static method uses the singleton Earth model and
     * returns the geodetic latitude and longitude, in
     * radians, of an ECF vector (does not need to be a unit vector).
     * @note This method requires knowledge that the specified input vector
     *       is known to have zero altitude and therefore is on the Earth
     *       ellipsoid (hence to 0 at the end of the method name). Otherwise
     *       use the more general method, ecefToLatLonAlt().
     * @param ecf Vector in the ECF frame.
     * @return A std::pair containing latitude and longitude in radians.
     */
    static std::pair<double, double> ecefToLatLon0( const m3d::Vector& ecf )
    {
        return getInstance()->toLatLon0( ecf );
    }

    /**
     * This static method uses the singleton Earth model and
     * returns the geodetic latitude and longitude, in
     * radians, and altitude in units of the Earth model
     * of an ECF vector (does not need to be a unit vector).
     * @param ecf Vector in the ECF frame.
     * @return A std::vector containing, in order, latitude and longitude in
     *         radians and altitude in Earth::units
     */
    static std::vector<double> ecefToLatLonAlt( const m3d::Vector& ecf )
    {
        return getInstance()->toLatLonAlt( ecf );
    }

    /**
     * This method computes the ECEF xyz vector from geodetic latitude,
     * longitude, and altitude.
     * @param latRadians Geodetic latitude in radians.
     * @param lonRadians Longitude in radians.
     * @param altitude Altitude above the ellipsoid in the same units as the
     *                 Earth model.
     * @return An ECEF vector for the geodtic input location.
     */
    m3d::Vector toEcef( double latRadians, double lonRadians, double altitude = 0.0 ) const;

    /**
     * This static method uses the singleton Earth model and computes the
     * ECEF xyz vector from geodetic latitude, longitude, and altitude.
     * @param latRadians Geodetic latitude in radians.
     * @param lonRadians Longitude in radians.
     * @param altitude Altitude above the ellipsoid in the same units as the
     *                 Earth model.
     * @return An ECEF vector for the geodtic input location.
     */
    static m3d::Vector llaToEcef( double latRadians, double lonRadians, double altitude )
    {
        return getInstance()->toEcef( latRadians, lonRadians, altitude );
    }

    /**
     * Compute the ellipsoidal Orthographic projection direction cosine
     * matrix given an ECEF vector.  This projection is not the same as the
     * standard Orthograpic projection but provides fast vector based
     * transformation for systems that support this projection.
     *
     * @see http://www.hydrometronics.com/downloads/Ellipsoidal%20Orthographic%20Projection.pdf
     *
     * @param ecf Reference vector in the ECEF frame (usually a satellite
     *            location)
     * @return Direction cosine matrix that can be applied against ECEF vectors
     *         for an ellipsoidal Orthographic projection.
     */
    m3d::Matrix getEllipsoidalOrthographicXform( const m3d::Vector ecf ) const;

    /**
     * This static method uses the singleton Earth model and computes the
     * the ellipsoidal Orthographic projection direction cosine
     * matrix given an ECEF vector.  This projection is not the same as the
     * standard Orthograpinc projection but provides fast vector based
     * transformation for systems that support this projection.
     * @param ecf Reference vector in the ECEF frame (usually a satellite
     *            location)
     * @return Direction cosine matrix that can be applied against ECEF vectors
     *         for an ellipsoidal Orthographic projection.
     */
    static m3d::Matrix getEllipsoidalOrthoXform( const m3d::Vector& ecf )
    {
        return getInstance()->getEllipsoidalOrthographicXform( ecf );
    }

    /**
     * Set the orthographic reference origin for projection.  Must be called
     * before using orthoGrapicMapXY().
     * @param lat0 Reference Geodetic latitude in radians.
     * @param lon0 Reference longitude in radians.
     */
    void setOrthoGraphicReference( double lat0, double lon0 );

    /**
     * Set the orthographic reference origin for projection.  Must be called
     * before using orthoGrapicMapXY().
     * @param ecef An ECEF reference vector which will be converted to a
     *             geodetic location for the projection with orthoGrapicMapXY().
     */
    void setOrthoGraphicReference( const m3d::Vector& ecef0 );

    typedef std::pair<double, double> MapXY_t;

    /**
     * Compute the map XY coordinates for a given geodetic latitude and
     * longitude.  The setOrthoGraphicReference() must be called before calling
     * this method -- it set the Orthographic projection reference lat, lon.
     *
     * @see https://en.wikipedia.org/wiki/Orthographic_projection_in_cartography
     *
     * @param lat Geodetic latitude in radians.
     * @param lon Longitude in radians.
     * @return An X,Y coordinate pair in Earth::units.
     */
    MapXY_t orthoGrapicMapXY( double lat, double lon ) const;

    /**
     * Compute the map XY coordinates for a given ECEF vector.
     * The setOrthoGraphicReference() must be called before calling
     * this method -- it set the Orthographic projection reference lat, lon.
     * @see https://en.wikipedia.org/wiki/Orthographic_projection_in_cartography
     * @param ecef ECEF vector to project.
     * @return An X,Y coordinate pair in Earth::units.
     */
    MapXY_t orthoGrapicMapXY( const m3d::Vector& ecef ) const;

    double getEquatorialRadius() const {return re_;}
    double getPolarRadius() const {return rp_;}
    double getFlattening() const {return f_;}
    const std::string& getUnits() const {return units_;}

private:
    static Earth* appInstance_;  /// singleton instance for application-wide use

    // constructor supplied
    double re_;          /// Equatorial radius
    double f_;           /// flattening factor
    std::string units_;

    // derived from constructor parameters
    double one_minus_f_;
    double rp_;          /// Polar radius
    double one_minus_f_sq_;
    double re_sq_;
    double e_sq_;

    // Orthographic projection variables set by setOrthographicReference()
    double orthoLat0_;  // radians
    double orthoLon0_;  // radians
    double sinOrthoLat0_;
    double cosOrthoLat0_;
};
#endif /* FRMTS_HDF5R_EARTH_H_ */
