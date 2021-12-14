/******************************************************************************
 *
 * Purpose:  Implementation of the CPCIDSKRPCModelSegment class.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
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
    std::string proj_parms; // Projection parameters encoded as text

    // TODO: Projection Info

    // The raw segment data
    PCIDSKBuffer seg_data;
};

CPCIDSKRPCModelSegment::CPCIDSKRPCModelSegment(PCIDSKFile *fileIn, int segmentIn,const char *segment_pointer) :
    CPCIDSKSegment(fileIn, segmentIn, segment_pointer), pimpl_(new CPCIDSKRPCModelSegment::PCIDSKRPCInfo),
    loaded_(false),mbModified(false),mbEmpty(false)
{
    try
    {
        Load();
    }
    catch( const PCIDSKException& )
    {
        delete pimpl_;
        pimpl_ = nullptr;
        throw;
    }
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

    if(data_size == 1024)
    {
        mbEmpty = true;
        return;
    }

    mbEmpty = false;

    if( data_size != 1024 + 7 * 512 )
    {
        return ThrowPCIDSKException("Wrong data_size in CPCIDSKRPCModelSegment");
    }

    pimpl_->seg_data.SetSize((int) (data_size - 1024)); // should be 7 * 512

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

    if (!STARTS_WITH(pimpl_->seg_data.buffer, "RFMODEL "))
    {
        pimpl_->seg_data.Put("RFMODEL",0,8);
        pimpl_->userrpc = false;
        pimpl_->adjusted = false;
        pimpl_->seg_data.Put("DS",22,2);
        pimpl_->downsample = 1;
        pimpl_->seg_data.Put("SENSOR",30,6);
        pimpl_->num_coeffs = 20;
        loaded_ = true;
        return;
        // Something has gone terribly wrong!
        /*throw PCIDSKException("A segment that was previously identified as an RFMODEL "
            "segment does not contain the appropriate data. Found: [%s]",
            std::string(pimpl_->seg_data.buffer, 8).c_str());*/
    }

    // Determine if this is user-provided
    pimpl_->userrpc = pimpl_->seg_data.buffer[8] == '1' ? true : false;

    // Check for the DS characters
    pimpl_->downsample = 1;
    if (STARTS_WITH(&pimpl_->seg_data.buffer[22], "DS"))
    {
        // Read the downsample factor
        pimpl_->downsample = pimpl_->seg_data.GetInt(24, 3);
    }

    //This is required if writing with PCIDSKIO
    //and reading with GDBIO (probably because of legacy issue)
    // see Bugzilla 255 and 254.
    bool bSecond = false;
    if (STARTS_WITH(&pimpl_->seg_data.buffer[27], "2ND"))
    {
        bSecond = true;
    }

    // Sensor name:
    if (STARTS_WITH(&pimpl_->seg_data.buffer[30], "SENSOR")) {
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
    // if bSecond is false, then the coefficient are stored
    // at others positions
    // every value takes 22 bytes.

    if(bSecond)
    {
        pimpl_->num_coeffs = pimpl_->seg_data.GetInt(512, 4);

        if (pimpl_->num_coeffs * 22 > 512) {
            // this segment is malformed. Throw an exception.
            return ThrowPCIDSKException("RFMODEL segment coefficient count requires more "
                "than one block to store. There is an error in this segment. The "
                "number of coefficients according to the segment is %d.", pimpl_->num_coeffs);
        }

        pimpl_->pixels = pimpl_->seg_data.GetInt(512 + 4, 10);
        pimpl_->lines = pimpl_->seg_data.GetInt(512 + 14, 10);
        pimpl_->x_off = pimpl_->seg_data.GetDouble(512 + 24, 22);
        pimpl_->x_scale = pimpl_->seg_data.GetDouble(512 + 46, 22);
        pimpl_->y_off = pimpl_->seg_data.GetDouble(512 + 68, 22);
        pimpl_->y_scale = pimpl_->seg_data.GetDouble(512 + 90, 22);
        pimpl_->z_off = pimpl_->seg_data.GetDouble(512 + 112, 22);
        pimpl_->z_scale = pimpl_->seg_data.GetDouble(512 + 134, 22);
        pimpl_->pix_off = pimpl_->seg_data.GetDouble(512 + 156, 22);
        pimpl_->pix_scale = pimpl_->seg_data.GetDouble(512 + 178, 22);
        pimpl_->line_off = pimpl_->seg_data.GetDouble(512 + 200, 22);
        pimpl_->line_scale = pimpl_->seg_data.GetDouble(512 + 222, 22);

        pimpl_->adjusted = false;
        // Read in adjusted X coefficients
        for (unsigned int i = 0; i <= 5; i++)
        {
            double tmp = pimpl_->seg_data.GetDouble(512 + 244 + (i * 22), 22);
            pimpl_->x_adj.push_back(tmp);
            if (0.0 != tmp)
            {
                pimpl_->adjusted = true;
            }
        }

        // Read in adjusted Y coefficients
        for (unsigned int i = 0; i <= 5; i++)
        {
            double tmp = pimpl_->seg_data.GetDouble(512 + 376 + (i * 22), 22);
            pimpl_->y_adj.push_back(tmp);
            if (0.0 != tmp)
            {
                pimpl_->adjusted = true;
            }
        }
    }
    else
    {
        pimpl_->num_coeffs = pimpl_->seg_data.GetInt(512, 22);

        if (pimpl_->num_coeffs * 22 > 512) {
            // this segment is malformed. Throw an exception.
            return ThrowPCIDSKException("RFMODEL segment coefficient count requires more "
                "than one block to store. There is an error in this segment. The "
                "number of coefficients according to the segment is %d.", pimpl_->num_coeffs);
        }

        pimpl_->lines = pimpl_->seg_data.GetInt(512 + 22, 22);
        pimpl_->pixels = pimpl_->seg_data.GetInt(512 + 2*22,22);
        pimpl_->x_off = pimpl_->seg_data.GetDouble(512 + 3*22, 22);
        pimpl_->x_scale = pimpl_->seg_data.GetDouble(512 + 4*22, 22);
        pimpl_->y_off = pimpl_->seg_data.GetDouble(512 + 5*22, 22);
        pimpl_->y_scale = pimpl_->seg_data.GetDouble(512 + 6*22, 22);
        pimpl_->z_off = pimpl_->seg_data.GetDouble(512 + 7*22, 22);
        pimpl_->z_scale = pimpl_->seg_data.GetDouble(512 + 8*22, 22);
        pimpl_->pix_off = pimpl_->seg_data.GetDouble(512 + 9*22, 22);
        pimpl_->pix_scale = pimpl_->seg_data.GetDouble(512 + 10*22, 22);
        pimpl_->line_off = pimpl_->seg_data.GetDouble(512 + 11*22, 22);
        pimpl_->line_scale = pimpl_->seg_data.GetDouble(512 + 12*22, 22);

        pimpl_->adjusted = false;
        // Read in adjusted X coefficients
        for (unsigned int i = 0; i <= 3; i++)
        {
            double tmp = pimpl_->seg_data.GetDouble(512 + 12*22 + (i * 22), 22);
            pimpl_->x_adj.push_back(tmp);
            if (0.0 != tmp)
            {
                pimpl_->adjusted = true;
            }
        }
        pimpl_->x_adj.push_back(0.0);
        pimpl_->x_adj.push_back(0.0);
        pimpl_->x_adj.push_back(0.0);

        // Read in adjusted Y coefficients
        for (unsigned int i = 0; i <= 3; i++)
        {
            double tmp = pimpl_->seg_data.GetDouble(512 + 16*22 + (i * 22), 22);
            pimpl_->y_adj.push_back(tmp);
            if (0.0 != tmp)
            {
                pimpl_->adjusted = true;
            }
        }
        pimpl_->y_adj.push_back(0.0);
        pimpl_->y_adj.push_back(0.0);
        pimpl_->y_adj.push_back(0.0);
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

    // Pad coefficients up to 20
    for (unsigned int i = pimpl_->num_coeffs; i < 20; ++i)
    {
        pimpl_->pixel_num.push_back(0.0);
        pimpl_->pixel_denom.push_back(0.0);
        pimpl_->line_num.push_back(0.0);
        pimpl_->line_denom.push_back(0.0);
    }

    // Block 7:
    // Bytes    0-15: MapUnits string
    // Bytes 256-511: ProjInfo_t, serialized
    pimpl_->map_units = std::string(&pimpl_->seg_data.buffer[6 * 512], 16);
    pimpl_->proj_parms = std::string(&pimpl_->seg_data.buffer[6 * 512 + 256], 256);

    // We've now loaded the structure up with data. Mark it as being loaded
    // properly.
    loaded_ = true;
}

void CPCIDSKRPCModelSegment::Write(void)
{
    //We are not writing if nothing was loaded.
    if (!loaded_) {
        return;
    }

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
    pimpl_->seg_data.Put("RFMODEL",0,8);

    // Determine if this is user-provided
    pimpl_->seg_data.buffer[8] = pimpl_->userrpc ? '1' : '0';

    // Check for the DS characters
    pimpl_->seg_data.Put("DS",22,2);
    pimpl_->seg_data.Put(pimpl_->downsample,24,3);

    //This is required if writing with PCIDSKIO
    //and reading with GDBIO (probably because of legacy issue)
    // see Bugzilla 255 and 254.
    pimpl_->seg_data.Put("2ND",27,3);

    // Sensor name:
    pimpl_->seg_data.Put("SENSOR",30,6);
    pimpl_->seg_data.Put(pimpl_->sensor_name.c_str(), 36, static_cast<int>(pimpl_->sensor_name.size()), true);

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

    if (pimpl_->num_coeffs * 22 > 512) {
        // this segment is malformed. Throw an exception.
        return ThrowPCIDSKException("RFMODEL segment coefficient count requires more "
            "than one block to store. There is an error in this segment. The "
            "number of coefficients according to the segment is %d.", pimpl_->num_coeffs);
    }

    pimpl_->seg_data.Put(pimpl_->num_coeffs,512, 4);

    pimpl_->seg_data.Put(pimpl_->pixels,512 + 4, 10);
    pimpl_->seg_data.Put(pimpl_->lines,512 + 14, 10);
    pimpl_->seg_data.Put(pimpl_->x_off,512 + 24, 22,"%22.14f");
    pimpl_->seg_data.Put(pimpl_->x_scale,512 + 46, 22,"%22.14f");
    pimpl_->seg_data.Put(pimpl_->y_off,512 + 68, 22,"%22.14f");
    pimpl_->seg_data.Put(pimpl_->y_scale,512 + 90, 22,"%22.14f");
    pimpl_->seg_data.Put(pimpl_->z_off,512 + 112, 22,"%22.14f");
    pimpl_->seg_data.Put(pimpl_->z_scale,512 + 134, 22,"%22.14f");
    pimpl_->seg_data.Put(pimpl_->pix_off,512 + 156, 22,"%22.14f");
    pimpl_->seg_data.Put(pimpl_->pix_scale,512 + 178, 22,"%22.14f");
    pimpl_->seg_data.Put(pimpl_->line_off,512 + 200, 22,"%22.14f");
    pimpl_->seg_data.Put(pimpl_->line_scale,512 + 222, 22,"%22.14f");

    // Read in adjusted X coefficients
    for (unsigned int i = 0; i <= 5; i++)
    {
        pimpl_->seg_data.Put(pimpl_->x_adj[i],512 + 244 + (i * 22), 22,"%22.14f");
        if(pimpl_->x_adj[i] != 0.0)
        {
            pimpl_->adjusted = true;
        }
    }

    // Read in adjusted Y coefficients
    for (unsigned int i = 0; i <= 5; i++)
    {
        pimpl_->seg_data.Put(pimpl_->y_adj[i],512 + 376 + (i * 22), 22,"%22.14f");
        if(pimpl_->y_adj[i] != 0.0)
        {
            pimpl_->adjusted = true;
        }
    }

    // Block 3:
    // Block 3 contains the numerator coefficients for the pixel rational polynomial
    // Number of Coefficients * 22 bytes
    for (unsigned int i = 0; i < pimpl_->num_coeffs; i++)
    {
        pimpl_->seg_data.Put(pimpl_->pixel_num[i],2 * 512 + (i * 22), 22,"%22.14f");
    }

    // Block 4:
    // Block 4 contains the denominator coefficients for the pixel rational polynomial
    // Number of Coefficients * 22 bytes
    for (unsigned int i = 0; i < pimpl_->num_coeffs; i++)
    {
        pimpl_->seg_data.Put(pimpl_->pixel_denom[i],3 * 512 + (i * 22), 22,"%22.14f");
    }

    // Block 5:
    // Block 5 contains the numerator coefficients for the line rational polynomial
    // Number of Coefficients * 22 bytes
    for (unsigned int i = 0; i < pimpl_->num_coeffs; i++)
    {
        pimpl_->seg_data.Put(pimpl_->line_num[i],4 * 512 + (i * 22), 22,"%22.14f");
    }

    // Block 6:
    // Block 6 contains the denominator coefficients for the line rational polynomial
    // Number of Coefficients * 22 bytes
    for (unsigned int i = 0; i < pimpl_->num_coeffs; i++)
    {
        pimpl_->seg_data.Put(pimpl_->line_denom[i],5 * 512 + (i * 22), 22,"%22.14f");
    }

    // Block 7:
    // Bytes    0-15: MapUnits string
    // Bytes 256-511: ProjInfo_t, serialized
    pimpl_->seg_data.Put(pimpl_->map_units.c_str(),6 * 512, 16);
    pimpl_->seg_data.Put(pimpl_->proj_parms.c_str(), 6 * 512 + 256, 256);

    WriteToFile(pimpl_->seg_data.buffer,0,data_size-1024);
    mbModified = false;
    mbEmpty = false;
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
        return ThrowPCIDSKException("All RPC coefficient vectors must be the "
            "same size.");
    }

    pimpl_->pixel_num = xnum;
    pimpl_->pixel_denom = xdenom;
    pimpl_->line_num = ynum;
    pimpl_->line_denom = ydenom;
    mbModified = true;
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

    mbModified = true;
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
        return ThrowPCIDSKException("X and Y adjusted coordinates must have "
            "length 6.");
    }

    pimpl_->x_adj = xcoord;
    pimpl_->y_adj = ycoord;

    mbModified = true;
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
    mbModified = true;
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
    mbModified = true;
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
    mbModified = true;
}

/******************************************************************************/
/*      GetMapUnits()                                                         */
/******************************************************************************/
/**
 * Get output projection information of the RPC math model.
 *
 * @param[out] map_units PCI mapunits string
 * @param[out] proj_parms Additional projection parameters, encoded as a
 *      string.
 *
 * @remarks If false == IsUserGenerated(), then this projection represents
 *      the projection that is utilized by the RPC's ground-to-image
 *      coefficients, i.e., the projection that must be used when performing
 *      ground-to-image or image-to-ground projections with the model.
 * @remarks If true == IsUserGenerated(), then the RPC math model's projection
 *      is Geographic WGS84 and the values returned here are just nominal
 *      values that may be used to generate output products with this model.
 */
void
CPCIDSKRPCModelSegment::GetMapUnits(std::string& map_units,
                                    std::string& proj_parms) const
{
    map_units = pimpl_->map_units;
    proj_parms = pimpl_->proj_parms;
    return;
}// GetMapUnits


/******************************************************************************/
/*      SetMapUnits()                                                         */
/******************************************************************************/
/**
 * Set output projection information of the RPC math model.
 *
 * @param[in] map_units PCI mapunits string
 * @param[in] proj_parms Additional projection parameters, encoded as a
 *      string.
 *
 * @remarks If false == IsUserGenerated(), then this projection represents
 *      the projection that is utilized by the RPC's ground-to-image
 *      coefficients, i.e., the projection that must be used when performing
 *      ground-to-image or image-to-ground projections with the model.
 * @remarks If true == IsUserGenerated(), then the RPC math model's projection
 *      is Geographic WGS84 and the values returned here are just nominal
 *      values that may be used to generate output products with this model.
 */
void
CPCIDSKRPCModelSegment::SetMapUnits(std::string const& map_units,
                                    std::string const& proj_parms)
{
    if (map_units.size() > 16)
    {
        return ThrowPCIDSKException("GeoSys/MapUnits string must be no more than "
            "16 characters to be valid.");
    }
    if (proj_parms.size() > 256)
    {
        return ThrowPCIDSKException("GeoSys/Projection parameters string must be no more than "
            "256 characters to be valid.");
    }
    pimpl_->map_units = map_units;
    pimpl_->proj_parms = proj_parms;
    mbModified = true;
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
        return ThrowPCIDSKException("Nonsensical raster dimensions provided: %ux%u",
                              lines, pixels);
    }

    pimpl_->lines = lines;
    pimpl_->pixels = pixels;
    mbModified = true;
}

void CPCIDSKRPCModelSegment::SetDownsample(const unsigned int downsample)
{
    if (downsample == 0) {
        return ThrowPCIDSKException("Invalid downsample factor provided: %u", downsample);
    }

    pimpl_->downsample = downsample;
    mbModified = true;
}

unsigned int CPCIDSKRPCModelSegment::GetDownsample(void) const
{
    return pimpl_->downsample;
}

void CPCIDSKRPCModelSegment::Synchronize()
{
    if(mbModified)
    {
        this->Write();
    }
}
