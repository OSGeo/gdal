/******************************************************************************
 *
 * Purpose:  Implementation of the CPCIDSKRPCModelSegment class.
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

#include "pcidsk_rpc.h"
#include "segment/cpcidsksegment.h"
#include "core/pcidsk_utils.h"
#include "pcidsk_exception.h"
#include "segment/cpcidskrpcmodel.h"

#include <vector>
#include <string>
#include <cassert>
#include <cstring>

using namespace PCIDSK;

// Struct to store details of the RPC model
struct CPCIDSKRPCModelSegment::PCIDSKRPCInfo
{
    bool userrpc; // whether or not the RPC was generated from GCPs
    bool adjusted; // Whether or not the RPC has been adjusted
    int downsample; // Epipolar Downsample factor
    
    unsigned int pixels; // pixels in the image
    unsigned int lines; // lines in the image
    
    unsigned int num_coeffs; // number of coefficientsg
    
    std::vector<double> pixel_num; // numerator, pixel direction
    std::vector<double> pixel_denom; // denominator, pixel direction
    std::vector<double> line_num; // numerator, line direction
    std::vector<double> line_denom; // denominator, line direction
    
    // Scale/offset coefficients in the ground domain
    double x_off;
    double x_scale;
    
    double y_off;
    double y_scale;
    
    double z_off;
    double z_scale;
    
    // Scale/offset coefficients in the raster domain
    double pix_off;
    double pix_scale;
    
    double line_off;
    double line_scale;
    
    std::vector<double> x_adj; // adjusted X values
    std::vector<double> y_adj; // adjusted Y values
    
    std::string sensor_name; // the name of the sensor
    
    std::string map_units; // the map units string
    
    // TODO: Projection Info
    
    // The raw segment data
    PCIDSKBuffer seg_data;
};

CPCIDSKRPCModelSegment::CPCIDSKRPCModelSegment(PCIDSKFile *file, int segment,const char *segment_pointer) :
    CPCIDSKSegment(file, segment, segment_pointer), pimpl_(new CPCIDSKRPCModelSegment::PCIDSKRPCInfo), 
    loaded_(false)
{
    Load();
}


CPCIDSKRPCModelSegment::~CPCIDSKRPCModelSegment()
{
    delete pimpl_;
}

// Load the contents of the segment
void CPCIDSKRPCModelSegment::Load()
{
    // Check if we've already loaded the segment into memory
    if (loaded_) {
        return;
    }
    
    assert(data_size - 1024 == 7 * 512);
    
    pimpl_->seg_data.SetSize(data_size - 1024); // should be 7 * 512
    
    ReadFromFile(pimpl_->seg_data.buffer, 0, data_size - 1024);
    
    // The RPC Model Segment is defined as follows:
    // RFMODEL Segment: 7 512-byte blocks
    
    // Block 1:
    // Bytes   0-7: 'RFMODEL '
    // Byte      8: User Provided RPC (1: user-provided, 0: computed from GCPs)
    // Bytes 22-23: 'DS' 
    // Bytes 24-26: Downsample factor used during Epipolar Generation
    // Bytes 27-29: '2ND' -- no clue what this means
    // Bytes 30-35: 'SENSOR'
    // Bytes    36: Sensor Name (NULL terminated)
    
    if (std::strncmp(pimpl_->seg_data.buffer, "RFMODEL ", 8)) {
        // Something has gone terribly wrong!
        throw PCIDSKException("A segment that was previously identified as an RFMODEL "
            "segment does not contain the appropriate data. Found: [%s]", 
            std::string(pimpl_->seg_data.buffer, 8).c_str());
    }
    
    // Determine if this is user-provided
    pimpl_->userrpc = pimpl_->seg_data.buffer[8] == 0 ? true : false;
    
    // Check for the DS characters
    pimpl_->downsample = 1;
    if (!std::strncmp(&pimpl_->seg_data.buffer[22], "DS", 2)) {
        // Read the downsample factor
        pimpl_->downsample = pimpl_->seg_data.GetInt(24, 3);
    }
    
    // I don't know what 2ND means yet.
    
    // Sensor name:
    if (!std::strncmp(&pimpl_->seg_data.buffer[30], "SENSOR", 6)) {
        pimpl_->sensor_name = std::string(&pimpl_->seg_data.buffer[36]);
    } else {
        pimpl_->sensor_name = "";
    }
    
    // Block 2:
    // Bytes     0-3: Number of coefficients
    // Bytes    4-13: Number of pixels
    // Bytes   14-23: Number of lines
    // Bytes   24-45: Longitude offset
    // Bytes   46-67: Longitude scale
    // Bytes   68-89: Latitude Offset
    // Bytes  90-111: Latitude Scale
    // Bytes 112-133: Height offset
    // Bytes 134-155: Height scale
    // Bytes 156-177: Sample offset
    // Bytes 178-199: Sample scale
    // Bytes 200-221: Line offset
    // Bytes 222-243: line scale
    // Bytes 244-375: Adjusted X coefficients (5 * 22 bytes)
    // Bytes 376-507: Adjusted Y coefficients (5 * 22 bytes)
    
    pimpl_->num_coeffs = pimpl_->seg_data.GetInt(512, 4);
    
    if (pimpl_->num_coeffs * 22 > 512) {
        // this segment is malformed. Throw an exception.
        throw PCIDSKException("RFMODEL segment coefficient count requires more "
            "than one block to store. There is an error in this segment. The "
            "number of coefficients according to the segment is %d.", pimpl_->num_coeffs);
    }
    
    pimpl_->lines = pimpl_->seg_data.GetInt(512 + 4, 10);
    pimpl_->pixels = pimpl_->seg_data.GetInt(512 + 14, 10);
    pimpl_->y_off = pimpl_->seg_data.GetDouble(512 + 24, 22);
    pimpl_->y_scale = pimpl_->seg_data.GetDouble(512 + 46, 22);
    pimpl_->x_off = pimpl_->seg_data.GetDouble(512 + 68, 22);
    pimpl_->x_scale = pimpl_->seg_data.GetDouble(512 + 90, 22);
    pimpl_->z_off = pimpl_->seg_data.GetDouble(512 + 112, 22);
    pimpl_->z_scale = pimpl_->seg_data.GetDouble(512 + 134, 22);
    pimpl_->pix_off = pimpl_->seg_data.GetDouble(512 + 156, 22);
    pimpl_->pix_scale = pimpl_->seg_data.GetDouble(512 + 178, 22);
    pimpl_->line_off = pimpl_->seg_data.GetDouble(512 + 200, 22);
    pimpl_->line_scale = pimpl_->seg_data.GetDouble(512 + 222, 22);
    
    // Read in adjusted X coefficients
    for (unsigned int i = 0; i <= 5; i++) {
        double tmp = pimpl_->seg_data.GetDouble(512 + 244 + (i * 22), 22);
        pimpl_->x_adj.push_back(tmp);
    }
    
    // Read in adjusted Y coefficients
    for (unsigned int i = 0; i <= 5; i++) {
        double tmp = pimpl_->seg_data.GetDouble(512 + 376 + (i * 22), 22);
        pimpl_->y_adj.push_back(tmp);
    }
    
    // Block 3:
    // Block 3 contains the numerator coefficients for the pixel rational polynomial
    // Number of Coefficients * 22 bytes
    for (unsigned int i = 0; i < pimpl_->num_coeffs; i++) {
        pimpl_->pixel_num.push_back(pimpl_->seg_data.GetDouble(2 * 512 + (i * 22), 22));
    }
    
    // Block 4:
    // Block 4 contains the denominator coefficients for the pixel rational polynomial
    // Number of Coefficients * 22 bytes
    for (unsigned int i = 0; i < pimpl_->num_coeffs; i++) {
        pimpl_->pixel_denom.push_back(pimpl_->seg_data.GetDouble(3 * 512 + (i * 22), 22));
    }
    
    // Block 5:
    // Block 5 contains the numerator coefficients for the line rational polynomial
    // Number of Coefficients * 22 bytes
    for (unsigned int i = 0; i < pimpl_->num_coeffs; i++) {
        pimpl_->line_num.push_back(pimpl_->seg_data.GetDouble(4 * 512 + (i * 22), 22));
    }
    
    // Block 6:
    // Block 6 contains the denominator coefficients for the line rational polynomial
    // Number of Coefficients * 22 bytes
    for (unsigned int i = 0; i < pimpl_->num_coeffs; i++) {
        pimpl_->line_denom.push_back(pimpl_->seg_data.GetDouble(5 * 512 + (i * 22), 22));
    }
    
    // Block 7:
    // Bytes    0-15: MapUnits string
    // Bytes 256-511: ProjInfo_t, serialized
    pimpl_->map_units = std::string(&pimpl_->seg_data.buffer[6 * 512], 16);
    
    // We've now loaded the structure up with data. Mark it as being loaded 
    // properly.
    loaded_ = true;
    
}

void CPCIDSKRPCModelSegment::Write(void)
{
    
}

std::vector<double> CPCIDSKRPCModelSegment::GetXNumerator(void) const
{
    return pimpl_->pixel_num;
}

std::vector<double> CPCIDSKRPCModelSegment::GetXDenominator(void) const
{
    return pimpl_->pixel_denom;
}

std::vector<double> CPCIDSKRPCModelSegment::GetYNumerator(void) const
{
    return pimpl_->line_num;
}

std::vector<double> CPCIDSKRPCModelSegment::GetYDenominator(void) const
{
    return pimpl_->line_denom;
}

// Set the RPC Coefficients
void CPCIDSKRPCModelSegment::SetCoefficients(
    const std::vector<double>& xnum, const std::vector<double>& xdenom,
    const std::vector<double>& ynum, const std::vector<double>& ydenom)
{
    if (xnum.size() != xdenom.size() || ynum.size() != ydenom.size() ||
        xnum.size() != ynum.size() || xdenom.size() != ydenom.size()) {
        throw PCIDSKException("All RPC coefficient vectors must be the "
            "same size.");
    }
    
    pimpl_->pixel_num = xnum;
    pimpl_->pixel_denom = xdenom;   
    pimpl_->line_num = ynum;
    pimpl_->line_denom = ydenom;
}
    
// Get the RPC offset/scale Coefficients
void CPCIDSKRPCModelSegment::GetRPCTranslationCoeffs(double& xoffset, double& xscale,
    double& yoffset, double& yscale, double& zoffset, double& zscale,
    double& pixoffset, double& pixscale, double& lineoffset, double& linescale) const
{
    xoffset = pimpl_->x_off;
    xscale = pimpl_->x_scale;

    yoffset = pimpl_->y_off;
    yscale = pimpl_->y_scale;
    
    zoffset = pimpl_->z_off;
    zscale = pimpl_->z_scale;
    
    pixoffset = pimpl_->pix_off;
    pixscale = pimpl_->pix_scale;
    
    lineoffset = pimpl_->line_off;
    linescale = pimpl_->line_scale;
}
    
// Set the RPC offset/scale Coefficients
void CPCIDSKRPCModelSegment::SetRPCTranslationCoeffs(
    const double xoffset, const double xscale,
    const double yoffset, const double yscale, 
    const double zoffset, const double zscale,
    const double pixoffset, const double pixscale, 
    const double lineoffset, const double linescale)
{
    pimpl_->x_off = xoffset;
    pimpl_->x_scale = xscale;
    
    pimpl_->y_off = yoffset;
    pimpl_->y_scale = yscale;

    pimpl_->z_off = zoffset;
    pimpl_->z_scale = zscale;

    pimpl_->pix_off = pixoffset;
    pimpl_->pix_scale = pixscale;

    pimpl_->line_off = lineoffset;
    pimpl_->line_scale = linescale;

}

// Get the adjusted X values
std::vector<double> CPCIDSKRPCModelSegment::GetAdjXValues(void) const
{
    return pimpl_->x_adj;
}

// Get the adjusted Y values
std::vector<double> CPCIDSKRPCModelSegment::GetAdjYValues(void) const
{
    return pimpl_->y_adj;
}

// Set the adjusted X/Y values
void CPCIDSKRPCModelSegment::SetAdjCoordValues(const std::vector<double>& xcoord,
    const std::vector<double>& ycoord)
{
    if (xcoord.size() != 6 || ycoord.size() != 6) {
        throw PCIDSKException("X and Y adjusted coordinates must have "
            "length 5.");
    }
    
    pimpl_->x_adj = xcoord;
    pimpl_->y_adj = ycoord;
}

// Get whether or not this is a user-generated RPC model
bool CPCIDSKRPCModelSegment::IsUserGenerated(void) const
{
    return pimpl_->userrpc;
}

// Set whether or not this is a user-generated RPC model
void CPCIDSKRPCModelSegment::SetUserGenerated(bool usergen)
{
    pimpl_->userrpc = usergen;
}

// Get whether the model has been adjusted
bool CPCIDSKRPCModelSegment::IsNominalModel(void) const
{
    return !pimpl_->adjusted;
}

// Set whether the model has been adjusted
void CPCIDSKRPCModelSegment::SetIsNominalModel(bool nominal)
{
    pimpl_->adjusted = !nominal;
}

// Get sensor name
std::string CPCIDSKRPCModelSegment::GetSensorName(void) const
{
    return pimpl_->sensor_name;
}

// Set sensor name
void CPCIDSKRPCModelSegment::SetSensorName(const std::string& name)
{
    pimpl_->sensor_name = name;
}

// Output projection information of RPC Model
// Get the Geosys String
std::string CPCIDSKRPCModelSegment::GetGeosysString(void) const
{
    return pimpl_->map_units;
}

// Set the Geosys string
void CPCIDSKRPCModelSegment::SetGeosysString(const std::string& geosys)
{
    if (geosys.size() > 16) {
        throw PCIDSKException("GeoSys/MapUnits string must be no more than "
            "16 characters to be valid.");
    }
    pimpl_->map_units = geosys;
}

// Get number of lines
unsigned int CPCIDSKRPCModelSegment::GetLines(void) const 
{
    return pimpl_->lines;
}

unsigned int CPCIDSKRPCModelSegment::GetPixels(void) const
{
    return pimpl_->pixels;
}

void CPCIDSKRPCModelSegment::SetRasterSize(const unsigned int lines, const unsigned int pixels)
{
    if (lines == 0 || pixels == 0) {
        throw PCIDSKException("Non-sensical raster dimensions provided: %ux%u", lines, pixels);
    }
    
    pimpl_->lines = lines;
    pimpl_->pixels = pixels;
}
