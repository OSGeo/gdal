/******************************************************************************
 *
 * Purpose:  Implementation of the APMODEL segment and storage objects.
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
#include "pcidsk_airphoto.h"
#include "pcidsk_exception.h"
#include "segment/cpcidskapmodel.h"

#include <utility>
#include <vector>
#include <cassert>
#include <cstring>

using namespace PCIDSK;

/**
 * Construct a new PCIDSK Airphoto Model Interior Orientation
 * parameters store.
 */
PCIDSKAPModelIOParams::PCIDSKAPModelIOParams(std::vector<double> const& imgtofocalx,
                                            std::vector<double> const& imgtofocaly,
                                            std::vector<double> const& focaltocolumn,
                                            std::vector<double> const& focaltorow,
                                            double focal_len,
                                            std::pair<double, double> const& prin_pt,
                                            std::vector<double> const& radial_dist) :
    imgtofocalx_(imgtofocalx), imgtofocaly_(imgtofocaly), focaltocolumn_(focaltocolumn),
    focaltorow_(focaltorow), focal_len_(focal_len), prin_point_(prin_pt),
    rad_dist_coeff_(radial_dist)
{
}

std::vector<double> const& PCIDSKAPModelIOParams::GetImageToFocalPlaneXCoeffs(void) const
{
    return imgtofocalx_;
}

std::vector<double> const& PCIDSKAPModelIOParams::GetImageToFocalPlaneYCoeffs(void) const
{
    return imgtofocaly_;
}

std::vector<double> const& PCIDSKAPModelIOParams::GetFocalPlaneToColumnCoeffs(void) const
{
    return focaltocolumn_;
}

std::vector<double> const& PCIDSKAPModelIOParams::GetFocalPlaneToRowCoeffs(void) const
{
    return focaltorow_;
}

double PCIDSKAPModelIOParams::GetFocalLength(void) const
{
    return focal_len_;
}

std::pair<double, double> const& PCIDSKAPModelIOParams::GetPrincipalPoint(void) const
{
    return prin_point_;
}

std::vector<double> const& PCIDSKAPModelIOParams::GetRadialDistortionCoeffs(void) const
{
    return rad_dist_coeff_;
}

/**
 * Construct a new PCIDSK Airphoto Model Exterior Orientation parameters
 * storage object.
 */
PCIDSKAPModelEOParams::PCIDSKAPModelEOParams(std::string const& rotation_type,
                                             std::vector<double> const& earth_to_body,
                                             std::vector<double> const& perspect_cen,
                                             unsigned int epsg_code) :
     rot_type_(rotation_type), earth_to_body_(earth_to_body),
     perspective_centre_pos_(perspect_cen), epsg_code_(epsg_code)
{
}

std::string PCIDSKAPModelEOParams::GetEarthToBodyRotationType(void) const
{
    return rot_type_;
}

std::vector<double> const& PCIDSKAPModelEOParams::GetEarthToBodyRotation(void) const
{
    return earth_to_body_;
}

std::vector<double> const& PCIDSKAPModelEOParams::GetPerspectiveCentrePosition(void) const
{
    return perspective_centre_pos_;
}

unsigned int PCIDSKAPModelEOParams::GetEPSGCode(void) const
{
    return epsg_code_;
}

/**
 * Miscellaneous camera parameters for the AP Model
 */
PCIDSKAPModelMiscParams::PCIDSKAPModelMiscParams(std::vector<double> const& decentering_coeffs,
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
                                                 bool has_radius) :
    decentering_coeffs_(decentering_coeffs), x3dcoord_(x3dcoord), y3dcoord_(y3dcoord),
    z3dcoord_(z3dcoord), radius_(radius), rff_(rff), min_gcp_hgt_(min_gcp_hgt),
    max_gcp_hgt_(max_gcp_hgt), is_prin_pt_off_(is_prin_pt_off),
    has_dist_(has_dist), has_decent_(has_decent), has_radius_(has_radius)
{
}

std::vector<double> const& PCIDSKAPModelMiscParams::GetDecenteringDistortionCoeffs(void) const
{
    return decentering_coeffs_;
}

std::vector<double> const& PCIDSKAPModelMiscParams::GetX3DCoord(void) const
{
    return x3dcoord_;
}

std::vector<double> const& PCIDSKAPModelMiscParams::GetY3DCoord(void) const
{
    return y3dcoord_;
}

std::vector<double> const& PCIDSKAPModelMiscParams::GetZ3DCoord(void) const
{
    return z3dcoord_;
}

double PCIDSKAPModelMiscParams::GetRadius(void) const
{
    return radius_;
}

double PCIDSKAPModelMiscParams::GetRFF(void) const
{
    return rff_;
}

double PCIDSKAPModelMiscParams::GetGCPMinHeight(void) const
{
    return min_gcp_hgt_;
}

double PCIDSKAPModelMiscParams::GetGCPMaxHeight(void) const
{
    return max_gcp_hgt_;
}

bool PCIDSKAPModelMiscParams::IsPrincipalPointOffset(void) const
{
    return is_prin_pt_off_;
}

bool PCIDSKAPModelMiscParams::HasDistortion(void) const
{
    return has_dist_;
}

bool PCIDSKAPModelMiscParams::HasDecentering(void) const
{
    return has_decent_;
}

bool PCIDSKAPModelMiscParams::HasRadius(void) const
{
    return has_radius_;
}

/**
 * Create a new PCIDSK APMODEL segment
 */
CPCIDSKAPModelSegment::CPCIDSKAPModelSegment(PCIDSKFile *file, int segment, const char *segment_pointer) : 
    CPCIDSKSegment(file, segment, segment_pointer)
{
    filled_ = false;
    io_params_ = NULL;
    eo_params_ = NULL;
    misc_params_ = NULL;
    UpdateFromDisk();
}
 
CPCIDSKAPModelSegment::~CPCIDSKAPModelSegment()
{
    delete io_params_;
    delete eo_params_;
    delete misc_params_;
}

unsigned int CPCIDSKAPModelSegment::GetWidth(void) const
{
    if (!filled_) {
        ThrowPCIDSKException("Failed to determine width from APModel.");
    }
    return width_;
}

unsigned int CPCIDSKAPModelSegment::GetHeight(void) const
{
    if (!filled_) {
        ThrowPCIDSKException("Failed to determine height from APModel.");
    }
    return height_;
}

unsigned int CPCIDSKAPModelSegment::GetDownsampleFactor(void) const
{
    if (!filled_) {
        ThrowPCIDSKException("Failed to determine APModel downsample factor.");
    }
    return downsample_;
}

// Interior Orientation Parameters
PCIDSKAPModelIOParams const& CPCIDSKAPModelSegment::GetInteriorOrientationParams(void) const
{
    if (io_params_ == NULL) {
        ThrowPCIDSKException("There was a failure in reading the APModel IO params.");
    }
    return *io_params_;
}

// Exterior Orientation Parameters
PCIDSKAPModelEOParams const& CPCIDSKAPModelSegment::GetExteriorOrientationParams(void) const
{
    if (eo_params_ == NULL) {
        ThrowPCIDSKException("There was a failure in reading the APModel EO params.");
    }
    return *eo_params_;
}

PCIDSKAPModelMiscParams const& CPCIDSKAPModelSegment::GetAdditionalParams(void) const
{
    if (misc_params_ == NULL) {
        ThrowPCIDSKException("There was a failure in reading the APModel camera params.");
    }
    return *misc_params_;
}

std::string CPCIDSKAPModelSegment::GetMapUnitsString(void) const
{
    return map_units_;
}

std::string CPCIDSKAPModelSegment::GetUTMUnitsString(void) const
{
    return map_units_;
}

std::vector<double> const& CPCIDSKAPModelSegment::GetProjParams(void) const
{
    return proj_parms_;
}

/************************************************************************/
/*                        BinaryToAPInfo()                          	*/
/************************************************************************/
/**
  * Convert the contents of the PCIDSKBuffer buf to a set of APModel
  * params
  *
  * @param buf A reference pointer to a PCIDSKBuffer
  * @param eo_params A pointer to EO params to be populated
  * @param io_params A pointer to IO params to be populated
  * @param misc_params A pointer to camera params to be populated
  * @param pixels The number of pixels in the image
  * @param lines The number of lines in the image
  * @param downsample The downsampling factor applied
  * @param map_units the map units/geosys string
  * @param utm_units the UTM units string
  */
namespace {
    void BinaryToAPInfo(PCIDSKBuffer& buf,
                        PCIDSKAPModelEOParams*& eo_params,
                        PCIDSKAPModelIOParams*& io_params,
                        PCIDSKAPModelMiscParams*& misc_params,
                        unsigned int& pixels,
                        unsigned int& lines,
                        unsigned int& downsample,
                        std::string& map_units,
                        std::vector<double>& proj_parms,
                        std::string& utm_units)

    {
        proj_parms.clear();
        map_units.clear();
        utm_units.clear();
    /* -------------------------------------------------------------------- */
    /*	Read the header block						*/
    /* -------------------------------------------------------------------- */
    
        if(strncmp(buf.buffer,"APMODEL ",8))
        {
            std::string magic(buf.buffer, 8);
            ThrowPCIDSKException("Bad segment magic found. Found: [%s] expecting [APMODEL ]",
                magic.c_str());
        }

    /* -------------------------------------------------------------------- */
    /*      Allocate the APModel.                                         	*/
    /* -------------------------------------------------------------------- */

        if (!strncmp(&buf.buffer[22],"DS",2))
    	    downsample = buf.GetInt(22, 3);

    /* -------------------------------------------------------------------- */
    /*      Read the values							*/
    /* -------------------------------------------------------------------- */
        pixels = buf.GetInt(0 * 22 + 512, 22);
        lines = buf.GetInt(1 * 22 + 512, 22);
        double focal_length = buf.GetDouble(2 * 22 + 512, 22);
        std::vector<double> perspective_centre(3);
        perspective_centre[0] = buf.GetDouble(3 * 22 + 512, 22);
        perspective_centre[1] = buf.GetDouble(4 * 22 + 512, 22);
        perspective_centre[2] = buf.GetDouble(5 * 22 + 512, 22);
    
        std::vector<double> earth_to_body(3);
        earth_to_body[0] = buf.GetDouble(6 * 22 + 512, 22);
        earth_to_body[1] = buf.GetDouble(7 * 22 + 512, 22);
        earth_to_body[2] = buf.GetDouble(8 * 22 + 512, 22);
    
        // NOTE: PCIDSK itself doesn't support storing information
        //       about the rotation type, nor the EPSG code for the
        //       transformation. However, in the (not so distant)
        //       future, we will likely want to add this support to
        //       the APMODEL segment (or perhaps a future means of
        //       storing airphoto information).
        eo_params = new PCIDSKAPModelEOParams("",
                                              earth_to_body,
                                              perspective_centre,
                                              -1);
    
        std::vector<double> x3d(3);
        std::vector<double> y3d(3);
        std::vector<double> z3d(3);
    
        x3d[0] = buf.GetDouble(9 * 22 + 512, 22);
        x3d[1] = buf.GetDouble(10 * 22 + 512, 22);
        x3d[2] = buf.GetDouble(11 * 22 + 512, 22);
        y3d[0] = buf.GetDouble(12 * 22 + 512, 22);
        y3d[1] = buf.GetDouble(13 * 22 + 512, 22);
        y3d[2] = buf.GetDouble(14 * 22 + 512, 22);
        z3d[0] = buf.GetDouble(15 * 22 + 512, 22);
        z3d[1] = buf.GetDouble(16 * 22 + 512, 22);
        z3d[2] = buf.GetDouble(17 * 22 + 512, 22);
    
        std::vector<double> img_to_focal_plane_x(4);
        std::vector<double> img_to_focal_plane_y(4);
        img_to_focal_plane_x[0]  = buf.GetDouble(18 * 22 + 512, 22);
        img_to_focal_plane_x[1]  = buf.GetDouble(19 * 22 + 512, 22);
        img_to_focal_plane_x[2]  = buf.GetDouble(20 * 22 + 512, 22);
        img_to_focal_plane_x[3]  = buf.GetDouble(21 * 22 + 512, 22);

        img_to_focal_plane_y[0]  = buf.GetDouble(0 * 22 + 512 * 2, 22);
        img_to_focal_plane_y[1]  = buf.GetDouble(1 * 22 + 512 * 2, 22);
        img_to_focal_plane_y[2]  = buf.GetDouble(2 * 22 + 512 * 2, 22);
        img_to_focal_plane_y[3]  = buf.GetDouble(3 * 22 + 512 * 2, 22);
    
        std::vector<double> focal_to_cols(4);
        std::vector<double> focal_to_lines(4);
        focal_to_cols[0]  = buf.GetDouble(4 * 22 + 512 * 2, 22);
        focal_to_cols[1]  = buf.GetDouble(5 * 22 + 512 * 2, 22);
        focal_to_cols[2]  = buf.GetDouble(6 * 22 + 512 * 2, 22);
        focal_to_cols[3]  = buf.GetDouble(7 * 22 + 512 * 2, 22);
    
        focal_to_lines[0]  = buf.GetDouble(8 * 22 + 512 * 2, 22);
        focal_to_lines[1]  = buf.GetDouble(9 * 22 + 512 * 2, 22);
        focal_to_lines[2]  = buf.GetDouble(10 * 22 + 512 * 2, 22);
        focal_to_lines[3]  = buf.GetDouble(11 * 22 + 512 * 2, 22);

        std::pair<double, double> principal_point;
    
        principal_point.first = buf.GetDouble(12 * 22 + 512 * 2, 22);
        principal_point.second  = buf.GetDouble(13 * 22 + 512 * 2, 22);
    
        std::vector<double> radial_distortion(8);
        radial_distortion[0] = buf.GetDouble(14 * 22 + 512 * 2, 22);
        radial_distortion[1] = buf.GetDouble(15 * 22 + 512 * 2, 22);
        radial_distortion[2] = buf.GetDouble(16 * 22 + 512 * 2, 22);
        radial_distortion[3] = buf.GetDouble(17 * 22 + 512 * 2, 22);
        radial_distortion[4] = buf.GetDouble(18 * 22 + 512 * 2, 22);
        radial_distortion[5] = buf.GetDouble(19 * 22 + 512 * 2, 22);
        radial_distortion[6] = buf.GetDouble(20 * 22 + 512 * 2, 22);
        radial_distortion[7] = buf.GetDouble(21 * 22 + 512 * 2, 22);
    
        // We have enough information now to construct the interior
        // orientation parameters
        io_params = new PCIDSKAPModelIOParams(img_to_focal_plane_x,
                                              img_to_focal_plane_y,
                                              focal_to_cols,
                                              focal_to_lines,
                                              focal_length,
                                              principal_point,
                                              radial_distortion);

        std::vector<double> decentering(4);
        decentering[0]  = buf.GetDouble(0 * 22 + 512 * 3, 22);
        decentering[1]  = buf.GetDouble(1 * 22 + 512 * 3, 22);
        decentering[2]  = buf.GetDouble(2 * 22 + 512 * 3, 22);
        decentering[3]  = buf.GetDouble(3 * 22 + 512 * 3, 22);
    
        double radius = buf.GetDouble(4 * 22 + 512 * 3, 22);
        double rff = buf.GetDouble(5 * 22 + 512 * 3, 22);
        double gcp_min_height = buf.GetDouble(6 * 22 + 512 * 3, 22);
        double gcp_max_height = buf.GetDouble(7 * 22 + 512 * 3, 22);
        bool prin_off = buf.GetInt(8 * 22 + 512 * 3, 22) != 0;
        bool distort_true = buf.GetInt(9 * 22 + 512 * 3, 22) != 0;
        bool has_decentering = buf.GetInt(10 * 22 + 512 * 3, 22) != 0;
        bool has_radius = buf.GetInt(11 * 22 + 512 * 3, 22) != 0;
    
        // Fill in the camera parameters
        misc_params = new PCIDSKAPModelMiscParams(decentering,
                                                  x3d,
                                                  y3d,
                                                  z3d,
                                                  radius,
                                                  rff,
                                                  gcp_min_height,
                                                  gcp_max_height,
                                                  prin_off,
                                                  distort_true,
                                                  has_decentering,
                                                  has_radius);


    /* -------------------------------------------------------------------- */
    /*      Read the projection required					*/
    /* -------------------------------------------------------------------- */
        buf.Get(512 * 4, 16, map_units);
    
        if (!strncmp(buf.Get(512 * 4 + 16, 3), "UTM", 3))
        {
            buf.Get(512 * 4, 3, utm_units);
        }

        //ProjParms2Info(szTmp, APModel->sProjection);
        // Parse the Proj Params
    
    }
} // end anonymous namespace

void CPCIDSKAPModelSegment::UpdateFromDisk(void)
{
    if (filled_) {
        return;
    }
    
    // Start reading in the APModel segment. APModel segments should be
    // 7 blocks long.
    if (data_size < (1024 + 7 * 512)) {
        ThrowPCIDSKException("APMODEL segment is smaller than expected. A "
            "segment of size %d was found", data_size);
    }
    buf.SetSize(data_size - 1024);
    ReadFromFile(buf.buffer, 0, data_size - 1024);
    
    // Expand it using an analogue to a method pulled from GDB
    BinaryToAPInfo(buf,
                   eo_params_,
                   io_params_,
                   misc_params_,
                   width_,
                   height_,
                   downsample_,
                   map_units_,
                   proj_parms_,
                   utm_units_);
    
    // Done, mark ourselves as having been properly filled
    filled_ = true;
}
