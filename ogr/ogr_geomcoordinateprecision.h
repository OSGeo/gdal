/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Definition of OGRGeomCoordinatePrecision.
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef OGR_GEOMCOORDINATEPRECISION_H
#define OGR_GEOMCOORDINATEPRECISION_H

#if !defined(DOXYGEN_SKIP)
#include <map>
#include "cpl_string.h"
#endif

class OGRSpatialReference;

/** Geometry coordinate precision.
 *
 * This may affect how many decimal digits (for text-based output) or bits
 * (for binary encodings) are used to encode geometries.
 *
 * It is important to note that the coordinate precision has no direct
 * relationship with the "physical" accuracy. It is generally advised that
 * the resolution (precision) be at least 10 times smaller than the accuracy.
 * @since GDAL 3.9
 */
struct CPL_DLL OGRGeomCoordinatePrecision
{
    /** Constant for a UNKNOWN resolution. */
    static constexpr double UNKNOWN = 0;

    /** Resolution for the coordinate precision of the X and Y coordinates.
     * Expressed in the units of the X and Y axis of the SRS.
     * For example for a projected SRS with X,Y axis unit in meter, a value
     * of 1e-3 corresponds to a 1 mm precision.
     * For a geographic SRS (on Earth) with axis unit in degree, a value
     * of 8.9e-9 (degree) also corresponds to a 1 mm precision.
     * Set to UNKNOWN if unknown.
     */
    double dfXYResolution = UNKNOWN;

    /** Resolution for the coordinate precision of the Z coordinate.
     * Expressed in the units of the Z axis of the SRS.
     * Set to UNKNOWN if unknown.
     */
    double dfZResolution = UNKNOWN;

    /** Resolution for the coordinate precision of the M coordinate.
     * Set to UNKNOWN if unknown.
     */
    double dfMResolution = UNKNOWN;

    /** Map from a format name to a list of format specific options.
     *
     * This can be for example used to store FileGeodatabase
     * xytolerance, xorigin, yorigin, etc. coordinate precision grids
     * options, which can be help to maximize preservation of coordinates in
     * FileGDB -> FileGDB conversion processes.
     */
    std::map<std::string, CPLStringList> oFormatSpecificOptions{};

    void SetFromMeter(const OGRSpatialReference *poSRS,
                      double dfXYMeterResolution, double dfZMeterResolution,
                      double dfMResolution);

    OGRGeomCoordinatePrecision
    ConvertToOtherSRS(const OGRSpatialReference *poSRSSrc,
                      const OGRSpatialReference *poSRSDst) const;

    static int ResolutionToPrecision(double dfResolution);
};

#endif /* OGR_GEOMCOORDINATEPRECISION_H */
