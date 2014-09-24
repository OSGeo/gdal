/******************************************************************************
 *
 * Purpose:  PCIDSK Georeferencing information storage class. Declaration.
 * 
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 50 West Wilmot Street, Richmond Hill, Ont, Canada
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/
#ifndef __INCLUDE_PCIDSK_GEOREF_H
#define __INCLUDE_PCIDSK_GEOREF_H

#include <string>
#include <vector>

namespace PCIDSK
{
    typedef enum {
        UNIT_US_FOOT = 1,
        UNIT_METER = 2,
        UNIT_DEGREE = 4,
        UNIT_INTL_FOOT = 5
    } UnitCode;

/************************************************************************/
/*                             PCIDSKGeoref                             */
/************************************************************************/

//! Interface to PCIDSK georeferencing segment.

    class PCIDSK_DLL PCIDSKGeoref
    {
    public:
        virtual	~PCIDSKGeoref() {}

/**
\brief Get georeferencing transformation.

Returns the affine georeferencing transform coefficients for this image.
Used to map from pixel/line coordinates to georeferenced coordinates using
the transformation:

 Xgeo = a1 +   a2 * Xpix + xrot * Ypix

 Ygeo = b1 + yrot * Xpix +   b2 * Ypix

where Xpix and Ypix are pixel line locations with (0,0) being the top left
corner of the top left pixel, and (0.5,0.5) being the center of the top left
pixel.  For an ungeoreferenced image the values will be 
(0.0,1.0,0.0,0.0,0.0,1.0).

@param a1 returns easting of top left corner.
@param a2 returns easting pixel size.
@param xrot returns rotational coefficient, normally zero.
@param b1 returns northing of the top left corner.
@param yrot returns rotational coefficient, normally zero.
@param b3 returns northing pixel size, normally negative indicating north-up.

*/
        virtual void GetTransform( double &a1, double &a2, double &xrot, 
            double &b1, double &yrot, double &b3 ) = 0;

/**
\brief Fetch georeferencing string.

Returns the short, 16 character, georeferncing string.  This string is
sufficient to document the coordinate system of simple coordinate
systems (like "UTM    17 S D000"), while other coordinate systems are
only fully defined with additional projection parameters.

@return the georeferencing string. 

*/        
        virtual std::string GetGeosys() = 0;

/**
\brief Fetch projection parameters.

Fetches the list of detailed projection parameters used for projection
methods not fully described by the Geosys string.  The projection 
parameters are as shown below, though in the future more items might
be added to the array.  The first 15 are the classic USGS GCTP parameters.

<ul>
<li> Parm[0]: diameter of earth - major axis (meters). 
<li> Parm[1]: diameter of earth - minor axis (meters).
<li> Parm[2]: Reference Longitude (degrees)
<li> Parm[3]: Reference Latitude (degrees)
<li> Parm[4]: Standard Parallel 1 (degrees)
<li> Parm[5]: Standard Parallel 2 (degrees)
<li> Parm[6]: False Easting (meters?)
<li> Parm[7]: False Northing (meters?)
<li> Parm[8]: Scale (unitless)
<li> Parm[9]: Height (meters?)
<li> Parm[10]: Longitude 1 (degrees)
<li> Parm[11]: Latitude 1 (degrees)
<li> Parm[12]: Longitude 2 (degrees)
<li> Parm[13]: Latitude 2 (degrees)
<li> Parm[14]: Azimuth (degrees)
<li> Parm[15]: Landsat Number 
<li> Parm[16]: Landsat Path
<li> Parm[17]: Unit Code (1=US Foot, 2=Meter, 4=Degree, 5=Intl Foot).
</ul>

Review the PCIDSK Database Reference Manual to understand which parameters
apply to which projection methods.

@return an array of values, at least 18.
*/

        virtual std::vector<double> GetParameters() = 0;

/**
\brief Write simple georeferencing information

Writes out a georeferencing string and geotransform to the segment. 

@param geosys 16 character coordinate system, like "UTM    17 S D000".
@param a1 easting of top left corner.
@param a2 easting pixel size.
@param xrot rotational coefficient, normally zero.
@param b1 northing of the top left corner.
@param yrot rotational coefficient, normally zero.
@param b3 northing pixel size, normally negative indicating north-up.

*/
        virtual void WriteSimple( std::string const& geosys, 
            double a1, double a2, double xrot, 
            double b1, double yrot, double b3 ) = 0;

/**
\brief Write complex projection parameters.

See GetParameters() for the description of the parameters list. 

@param parameters A list of at least 17 projection parameters. 

*/

        virtual void WriteParameters( std::vector<double> const& parameters ) = 0;
    };
} // end namespace PCIDSK

#endif // __INCLUDE_PCIDSK_GEOREF_H
