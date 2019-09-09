#include <cmath>
#include <iostream>

#include "Earth.h"

// singleton instance pointer
Earth* Earth::appInstance_ = nullptr;

//******************************************************************************
// Earth::where determines where a line-of-sight unit vector with an origin
// at vectorBaseEcf (usually from a satellite ephemeris) contacts the Earth
// ellipsoid, of if over the horizon, a point in space above the tangent point.
//******************************************************************************
bool Earth::where( const m3d::Vector& vectorBaseEcf,
                   const m3d::Vector& vectorDirectionEcf,
                   m3d::Vector* ecfPoint ) const
{
  // 3D line equation is (x-x1)/A = (y-y1)/B = (z-z1)/C
  // The x1,y1,z1 terms are the point on the line, in this case
  // the origin of the ECF line-of-sight vector.  Must be in
  // the same units as the Earth model
  double x1 = vectorBaseEcf.i();
  double y1 = vectorBaseEcf.j();
  double z1 = vectorBaseEcf.k();

  // the A,B,C terms are the corresponding elements of the
  // unit ECF line-of-sight vector
  double A = vectorDirectionEcf.i();
  double B = vectorDirectionEcf.j();
  double C = vectorDirectionEcf.k();

  // common terms
  double k1 = A*A + B*B;
  double k2 = C * (A*x1 + B*y1);
  double Csq = C*C;

  //                          2
  // quadratic coefficients az   + bz  +  c = 0 for the ellipsoid
  // after substitution from the line equations for x and y  in terms of z:
  //
  double a = k1 + Csq/one_minus_f_sq_;
  double b = 2.0 * (k2 - k1 * z1);
  double c = (k1*z1 - 2.0*k2)*z1 + Csq*(x1*x1 + y1*y1 - re_sq_);

  // calculated the quadratic discriminant
  double discriminant = b*b - 4.0*a*c;

  // if negative then the solution is over the horizon
  bool isOth = (discriminant < 0.0);

  // for the over-the-horizon case, project to a sphere with a radius equal
  // to the length of the LOS vector at the horizon to the spherical Earth
  // this allows for a very fast (no trig functions) computation
  if (isOth)
  {
      // length of LOS vector for spherical Earth, tangent point is 90 deg, so
      // can use Pythagorean theorem to compute length
      double losMag = std::sqrt( vectorBaseEcf.sumsq() - re_sq_ );

      // Earth intersection vector is sum of LOS vector and satellite vector
      *ecfPoint = (losMag * vectorDirectionEcf) + vectorBaseEcf;
  }

  else // intersects the Earth ellipsoid
  {
    // intermediate terms
    double k3 = 1.0 / (2.0 * a);
    double AoverC = A/C;
    double BoverC = B/C;

    // square root of the discriminant
    double sqrt_discrim = std::sqrt( discriminant );

    // solution 1 and the square of the vector distance to the reference point
    double z_plus  = (-b + sqrt_discrim) * k3;
    m3d::Vector solution1( AoverC * (z_plus - z1) + x1,
                           BoverC * (z_plus - z1) + y1,
                           z_plus );
    double distsq1 = m3d::sumsq( solution1 - vectorBaseEcf );

    // solution 2 and the square of the vector distance to the reference point
    double z_minus = (-b - sqrt_discrim) * k3;
    m3d::Vector solution2( AoverC * (z_minus - z1) + x1,
                           BoverC * (z_minus - z1) + y1,
                           z_minus );
    double distsq2 = m3d::sumsq( solution2 - vectorBaseEcf );

    // pick the solution that is closest
    *ecfPoint = (distsq1 < distsq2) ? solution1 : solution2;
  }

  return isOth;
}

//******************************************************************************
// Earth::toEcef converts a geodetic latitude, longitude and altitude to
//     an ECF vector, in the units of the Earth model. Angles are in radians.
//******************************************************************************
m3d::Vector Earth::toEcef( double latRadians, double lonRadians, double altitude ) const
{
    double cosLat = std::cos( latRadians );
    double sinLat = std::sin( latRadians );
    double cosLon = std::cos( lonRadians );
    double sinLon = std::sin( lonRadians );

    double N = re_ / std::sqrt( 1.0 - e_sq_ * sinLat * sinLat );

    return m3d::Vector( (N + altitude) * cosLat * cosLon,
                        (N + altitude) * cosLat * sinLon,
                        (one_minus_f_sq_ * N + altitude) * sinLat );
}

//******************************************************************************
// Earth::toLatLonAlt converts an ECEF vector to geodetic latitude and
//     longitude in radians and altitude in the Earth model units
//
// Uses Olson's series approximation
//
// For a description and Python code, see:
// https://possiblywrong.wordpress.com/2014/02/14/when-approximate-is-better-than-exact/
//
// You need IEEE access for the original technical paper:
//
// Olson, D. K., Converting Earth-Centered, Earth-Fixed Coordinates to
// Geodetic Coordinates, IEEE Transactions on Aerospace and Electronic
// Systems, 32 (1996) 473-476.
//******************************************************************************
std::vector<double> Earth::toLatLonAlt( const m3d::Vector& ecf ) const
{
    // Derived parameters
    static const double a1 = re_ * e_sq_;
    static const double a2 = a1 * a1;
    static const double a3 = a1 * e_sq_ / 2.0;
    static const double a4 = 2.5 * a2;
    static const double a5 = a1 + a3;
    static const double a6 = 1.0 - e_sq_;

    // The algorithm (converted 'when-approximate-is-better-than-exact'
    // Python code to C++)
    //
    double w = std::sqrt( ecf.i() * ecf.i() + ecf.j() * ecf.j() );
    double z = ecf.k();
    double zp = std::abs( z );
    double w2 = w * w;
    double r2 = z * z + w2;
    double r  = std::sqrt( r2 );
    double s2 = z * z / r2;
    double c2 = w2 / r2;
    double u = a2 / r;
    double v = a3 - a4 / r;
    double s, lat, ss, c;
    if (c2 > 0.3)
    {
        s = (zp / r) * (1 + c2 * (a1 + u + s2 * v) / r);
        lat = std::asin(s);
        ss = s * s;
        c = std::sqrt(1.0 - ss);
    }
    else
    {
        c = (w / r) * (1 - s2 * (a5 - u - c2 * v) / r);
        lat = std::acos(c);
        ss = 1 - c * c;
        s = std::sqrt(ss);
    }

    double g = 1 - e_sq_ * ss;
    double rg = re_ / std::sqrt(g);
    double rf = a6 * rg;
    u = w - rg * c;
    v = zp - rf * s;
    double f = c * u + s * v;
    double m = c * v - s * u;
    double p = m / (rf / g + f);
    lat = lat + p;
    if (z < 0.0)
        lat = -lat;

    // instantiate and return the 3 element result std::vector [lat, lon, alt]
    // in radians and Earth::units
    std::vector<double> latLonAlt( 3 );
    latLonAlt[0] = lat;
    latLonAlt[1] = std::atan2( ecf.j(), ecf.i() );
    latLonAlt[2] = f + m * p / 2.0;

    return latLonAlt;
}

//******************************************************************************
// Static method that returns the ellipsoidal orthographic projection direction
// cosine matrix given an ECEF vector.
//******************************************************************************
m3d::Matrix Earth::getEllipsoidalOrthographicXform( const m3d::Vector ecf ) const
{
    // get the Geodetic lat, lon
    std::vector<double> latLonAlt = ecefToLatLonAlt( ecf );
    const double& lat = latLonAlt[0];
    const double& lon = latLonAlt[1];

    double sin_lat = std::sin( lat );
    double cos_lat = std::cos( lat );
    double sin_lon = std::sin( lon );
    double cos_lon = std::cos( lon );
    m3d::Matrix orthoXform( -sin_lon,          cos_lon,         0.0,
                            -sin_lat*cos_lon, -sin_lat*sin_lon, cos_lat,
                             cos_lat*cos_lon,  cos_lat*sin_lon, sin_lat );
    return orthoXform;
}

//******************************************************************************
// Set an the reference location for standard orthographic projection
// from a geodetic latitude, longitude.
//******************************************************************************
void Earth::setOrthoGraphicReference( double lat0, double lon0 )
{
    // validate input ranges
    if (std::abs( lat0 ) > M_PI )
        throw std::range_error( "Earth::setOrthoGraphicReference"
                "latitude out of [-PI, +PI] range: " + std::to_string( lat0 ) );

    if (std::abs( lon0 ) > 2.0*M_PI)
        throw std::range_error( "Earth::setOrthoGraphicReference"
                "longitude out of [-2PI, +2PI] range: " + std::to_string( lon0 ) );

    orthoLat0_ = lat0;
    orthoLon0_ = lon0;

    sinOrthoLat0_ = std::sin( lat0 );
    cosOrthoLat0_ = std::cos( lat0 );
}

//******************************************************************************
// Set an the reference location for standard orthographic projection
// from an ECEF location (usually a satellite ephemeris)
//******************************************************************************
void Earth::setOrthoGraphicReference( const m3d::Vector& ecef0 )
{
    std::vector<double> latLonAlt = toLatLonAlt( ecef0 );
    setOrthoGraphicReference( latLonAlt[0], latLonAlt[1] );
}

//******************************************************************************
// Compute the Orthographic projection map X,Y location for a
// geodetic lat, lon location in radians
//******************************************************************************
Earth::MapXY_t Earth::orthoGrapicMapXY( double lat, double lon ) const
{
    // verify reference location has been set
    if (std::abs( orthoLat0_ ) > M_PI )
        throw std::range_error( "Earth::setOrthoGraphicReference"
                " reference not set. need to call setOrthoGraphicReference() first" );

    // validate input ranges
    if (std::abs( lat ) > M_PI )
        throw std::range_error( "Earth::setOrthoGraphicReference"
                "latitude out of [-PI, +PI] range: " + std::to_string( lat ) );

    if (std::abs( lon ) > 2.0*M_PI)
        throw std::range_error( "Earth::setOrthoGraphicReference"
                "longitude out of [-2PI, +2PI] range: " + std::to_string( lon ) );

    double deltaLon = (lon - orthoLon0_);
    double cosLat = std::cos( lat );
    return MapXY_t( re_  * cosLat * std::sin( deltaLon ),
                    re_  * (cosOrthoLat0_ * std::sin( lat )
                              - sinOrthoLat0_ * cosLat * std::cos( deltaLon )));
}

//******************************************************************************
// Compute the Orthographic projection map X,Y location for an ECEF vector
//******************************************************************************
Earth::MapXY_t Earth::orthoGrapicMapXY( const m3d::Vector& ecef ) const
{
    std::vector<double> latLonAlt = toLatLonAlt( ecef );
    return orthoGrapicMapXY( latLonAlt[0], latLonAlt[1] );
}
