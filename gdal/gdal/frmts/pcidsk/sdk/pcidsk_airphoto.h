/******************************************************************************
 *
 * Purpose: Declaration of the Airphoto segment interface and the helper
 *          storage objects.
 * 
 ******************************************************************************
 * Copyright (c) 2010
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
#ifndef INCLUDE_PCIDSK_SRC_PCIDSK_AIRPHOTO_H
#define INCLUDE_PCIDSK_SRC_PCIDSK_AIRPHOTO_H

#include "pcidsk_config.h"

#include <vector>
#include <string>
#include <utility>

namespace PCIDSK {
    /**
     * Structure for storing interior orientation parameters associated 
     * with the APModel
     */
    class PCIDSK_DLL PCIDSKAPModelIOParams
    {
    public:
        PCIDSKAPModelIOParams(std::vector<double> const& imgtofocalx,
            std::vector<double> const& imgtofocaly,
            std::vector<double> const& focaltocolumn,
            std::vector<double> const& focaltorow,
            double focal_len,
            std::pair<double, double> const& prin_pt,
            std::vector<double> const& radial_dist);
        std::vector<double> const& GetImageToFocalPlaneXCoeffs(void) const;
        std::vector<double> const& GetImageToFocalPlaneYCoeffs(void) const;
        std::vector<double> const& GetFocalPlaneToColumnCoeffs(void) const;
        std::vector<double> const& GetFocalPlaneToRowCoeffs(void) const;
        
        double GetFocalLength(void) const;
        std::pair<double, double> const& GetPrincipalPoint(void) const;
        std::vector<double> const& GetRadialDistortionCoeffs(void) const;
    private:
        std::vector<double> imgtofocalx_;
        std::vector<double> imgtofocaly_;
        std::vector<double> focaltocolumn_;
        std::vector<double> focaltorow_;
        double focal_len_;
        std::pair<double, double> prin_point_;
        std::vector<double> rad_dist_coeff_;
    };
    
    /**
     * Structure for storing exterior orientation parameters associated
     * with the APModel
     */
    class PCIDSK_DLL PCIDSKAPModelEOParams
    {
    public:
        PCIDSKAPModelEOParams(std::string const& rotation_type,
            std::vector<double> const& earth_to_body,
            std::vector<double> const& perspect_cen,
            unsigned int epsg_code = 0);
        std::string GetEarthToBodyRotationType(void) const;
        std::vector<double> const& GetEarthToBodyRotation(void) const;
        std::vector<double> const& GetPerspectiveCentrePosition(void) const;
        unsigned int GetEPSGCode(void) const;
    private:
        std::string rot_type_;
        std::vector<double> earth_to_body_;
        std::vector<double> perspective_centre_pos_;
        unsigned int epsg_code_;
    };
    
    class PCIDSK_DLL PCIDSKAPModelMiscParams
    {
    public:
        PCIDSKAPModelMiscParams(std::vector<double> const& decentering_coeffs,
                                std::vector<double> const& x3dcoord,
                                std::vector<double> const& y3dcoord,
                                std::vector<double> const& z3dcoord,
                                double radius,
                                double rff,
                                double min_gcp_hgt,
                                double max_gcp_hgt,
                                bool is_prin_pt_off,
                                bool has_dist,
                                bool has_decent,
                                bool has_radius);
        std::vector<double> const& GetDecenteringDistortionCoeffs(void) const;
        std::vector<double> const& GetX3DCoord(void) const;
        std::vector<double> const& GetY3DCoord(void) const;
        std::vector<double> const& GetZ3DCoord(void) const;
        double GetRadius(void) const;
        double GetRFF(void) const; // radius * focal * focal
        double GetGCPMinHeight(void) const;
        double GetGCPMaxHeight(void) const;
        bool IsPrincipalPointOffset(void) const;
        bool HasDistortion(void) const;
        bool HasDecentering(void) const;
        bool HasRadius(void) const;
    private:
        std::vector<double> decentering_coeffs_;
        std::vector<double> x3dcoord_;
        std::vector<double> y3dcoord_;
        std::vector<double> z3dcoord_;
        double radius_, rff_, min_gcp_hgt_, max_gcp_hgt_;
        bool is_prin_pt_off_;
        bool has_dist_;
        bool has_decent_;
        bool has_radius_;
    };
    
    /**
     * Interface for accessing the contents of the Airphoto Model
     * segment.
     */
    class PCIDSKAPModelSegment
    {
    public:
        virtual ~PCIDSKAPModelSegment() {}

        virtual unsigned int GetWidth(void) const = 0;
        virtual unsigned int GetHeight(void) const = 0;
        virtual unsigned int GetDownsampleFactor(void) const = 0;

        // Interior Orientation Parameters
        virtual PCIDSKAPModelIOParams const& GetInteriorOrientationParams(void) const = 0;
        
        // Exterior Orientation Parameters
        virtual PCIDSKAPModelEOParams const& GetExteriorOrientationParams(void) const = 0;

        // ProjInfo
        virtual PCIDSKAPModelMiscParams const& GetAdditionalParams(void) const = 0;
        
        virtual std::string GetMapUnitsString(void) const = 0;
        virtual std::string GetUTMUnitsString(void) const = 0;
        virtual std::vector<double> const& GetProjParams(void) const = 0;
        
    };
    
} // end namespace PCIDSK

#endif // INCLUDE_PCIDSK_SRC_PCIDSK_AIRPHOTO_H

