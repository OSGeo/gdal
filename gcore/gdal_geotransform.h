/******************************************************************************
 *
 * Name:     gdal_geotransform.h
 * Project:  GDAL Core
 * Purpose:  Declaration of GDALGeoTransform class
 * Author:   Even Rouault, <even.rouault@spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault, <even.rouault@spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALGEOTRANSFORM_H_INCLUDED
#define GDALGEOTRANSFORM_H_INCLUDED

#include "gdal.h"

#include <utility>

class GDALRasterWindow;

/* ******************************************************************** */
/*                             GDALGeoTransform                         */
/* ******************************************************************** */

/** Class that encapsulates a geotransform matrix.
 *
 * It contains 6 coefficients expressing an affine transformation from
 * (column, line) raster space to (X, Y) georeferenced space, such that
 *
 * \code{.c}
 *  X = xorig + column * xscale + line * xrot;
 *  Y = yorig + column * yrot   + line * yscale;
 * \endcode
 *
 * The default value is the identity transformation.
 *
 * @since 3.12
 */
class CPL_DLL GDALGeoTransform
{
  public:
    // NOTE to GDAL developers: do not reorder those coefficients!

    /** X value of the origin of the raster */
    double xorig = 0;

    /** X scale factor */
    double xscale = 1;

    /** X rotation factor */
    double xrot = 0;

    /** Y value of the origin of the raster */
    double yorig = 0;

    /** Y rotation factor */
    double yrot = 0;

    /** Y scale factor */
    double yscale = 1;

    /** Default constructor for an identity geotransformation matrix. */
    inline GDALGeoTransform() = default;

    /** Constructor from a array of 6 double */
    inline explicit GDALGeoTransform(const double coeffs[6])
    {
        static_assert(sizeof(GDALGeoTransform) == 6 * sizeof(double),
                      "Wrong size for GDALGeoTransform");
        xorig = coeffs[0];
        xscale = coeffs[1];
        xrot = coeffs[2];
        yorig = coeffs[3];
        yrot = coeffs[4];
        yscale = coeffs[5];
    }

    /** Constructor from 6 double values */
    inline GDALGeoTransform(double xorigIn, double xscaleIn, double xrotIn,
                            double yorigIn, double yrotIn, double yscaleIn)
    {
        xorig = xorigIn;
        xscale = xscaleIn;
        xrot = xrotIn;
        yorig = yorigIn;
        yrot = yrotIn;
        yscale = yscaleIn;
    }

    /** Element accessor. idx must be in [0,5] range */
    template <typename T> inline double operator[](T idx) const
    {
        return *(&xorig + idx);
    }

    /** Element accessor. idx must be in [0,5] range */
    template <typename T> inline double &operator[](T idx)
    {
        return *(&xorig + idx);
    }

    /** Equality test operator */
    inline bool operator==(const GDALGeoTransform &other) const
    {
        return xorig == other.xorig && xscale == other.xscale &&
               xrot == other.xrot && yorig == other.yorig &&
               yrot == other.yrot && yscale == other.yscale;
    }

    /** Inequality test operator */
    inline bool operator!=(const GDALGeoTransform &other) const
    {
        return !(operator==(other));
    }

    /** Cast to const double* */
    inline const double *data() const
    {
        return &xorig;
    }

    /** Cast to double* */
    inline double *data()
    {
        return &xorig;
    }

    /**
     * Apply GeoTransform to x/y coordinate.
     *
     * Applies the following computation, converting a (pixel, line) coordinate
     * into a georeferenced (geo_x, geo_y) location.
     * \code{.c}
     *  *pdfGeoX = padfGeoTransform[0] + dfPixel * padfGeoTransform[1]
     *                                 + dfLine  * padfGeoTransform[2];
     *  *pdfGeoY = padfGeoTransform[3] + dfPixel * padfGeoTransform[4]
     *                                 + dfLine  * padfGeoTransform[5];
     * \endcode
     *
     * @param dfPixel Input pixel position.
     * @param dfLine Input line position.
     * @param pdfGeoX output location where geo_x (easting/longitude)
     * location is placed.
     * @param pdfGeoY output location where geo_y (northing/latitude)
     * location is placed.
     */

    inline void Apply(double dfPixel, double dfLine, double *pdfGeoX,
                      double *pdfGeoY) const
    {
        GDALApplyGeoTransform(data(), dfPixel, dfLine, pdfGeoX, pdfGeoY);
    }

    /** Apply a (inverse) geotransform to an OGREnvelope in georeferenced coordinates.
     *
     * @param env An envelope in georeferenced coordinates
     * @param[out] window A window in pixel/line coordinates
     * @return true if the geotransform was successfully applied
     */
    bool Apply(const OGREnvelope &env, GDALRasterWindow &window) const;

    /** Apply a geotransform to a GDALRasterWindow in pixel/line coordinates.
     *
     * @param window A window in pixel/line coordinates
     * @param[out] env An envelope in georeferenced coordinates
     * @return true if the geotransform was successfully applied
     */
    bool Apply(const GDALRasterWindow &window, OGREnvelope &env) const;

    /**
     * Apply GeoTransform to x/y coordinate.
     *
     * Applies the following computation, converting a (pixel, line) coordinate
     * into a georeferenced (geo_x, geo_y) location.
     * \code{.c}
     *  out.first = padfGeoTransform[0] + dfPixel * padfGeoTransform[1]
     *                                   + dfLine  * padfGeoTransform[2];
     *  out.second = padfGeoTransform[3] + dfPixel * padfGeoTransform[4]
     *                                   + dfLine  * padfGeoTransform[5];
     * \endcode
     *
     * @param dfPixel Input pixel position.
     * @param dfLine Input line position.
     * @return output location as a (geo_x, geo_y) pair
     */

    inline std::pair<double, double> Apply(double dfPixel, double dfLine) const
    {
        double dfOutX, dfOutY;
        GDALApplyGeoTransform(data(), dfPixel, dfLine, &dfOutX, &dfOutY);
        return {dfOutX, dfOutY};
    }

    /**
     * Invert Geotransform.
     *
     * This function will invert a standard 3x2 set of GeoTransform coefficients.
     * This converts the equation from being pixel to geo to being geo to pixel.
     *
     * @param[out] inverse Output geotransform
     *
     * @return true on success or false if the equation is uninvertable.
     */
    inline bool GetInverse(GDALGeoTransform &inverse) const
    {
        return GDALInvGeoTransform(data(), inverse.data()) == TRUE;
    }

    /** Rescale a geotransform by multiplying its scale and rotation terms by
     * the provided ratios.
     *
     * This is typically used to compute the geotransform matrix of an overview
     * dataset from the full resolution dataset, where the ratios are the size
     * of the full resolution dataset divided by the size of the overview.
     */
    inline void Rescale(double dfXRatio, double dfYRatio)
    {
        xscale *= dfXRatio;
        xrot *= dfYRatio;
        yrot *= dfXRatio;
        yscale *= dfYRatio;
    }

    /** Check whether the geotransform has a rotation component.
     */
    inline bool IsAxisAligned() const
    {
        return xrot == 0 && yrot == 0;
    }
};

#endif
