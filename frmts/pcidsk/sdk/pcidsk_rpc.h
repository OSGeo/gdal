/******************************************************************************
 *
 * Purpose: Interface representing access to a PCIDSK RPC Segment
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
#ifndef INCLUDE_PCIDSK_PCIDSK_RPC_H
#define INCLUDE_PCIDSK_PCIDSK_RPC_H

#include <vector>
#include <string>

namespace PCIDSK {
//! Interface to PCIDSK RPC segment.
    class PCIDSKRPCSegment
    {
    public:

        // Get the X and Y RPC coefficients
        virtual std::vector<double> GetXNumerator(void) const = 0;
        virtual std::vector<double> GetXDenominator(void) const = 0;
        virtual std::vector<double> GetYNumerator(void) const = 0;
        virtual std::vector<double> GetYDenominator(void) const = 0;

        // Set the X and Y RPC Coefficients
        virtual void SetCoefficients(const std::vector<double>& xnum,
            const std::vector<double>& xdenom, const std::vector<double>& ynum,
            const std::vector<double>& ydenom) = 0;

        // Get the RPC offset/scale Coefficients
        virtual void GetRPCTranslationCoeffs(double& xoffset, double& xscale,
            double& yoffset, double& yscale, double& zoffset, double& zscale,
            double& pixoffset, double& pixscale, double& lineoffset, double& linescale) const = 0;

        // Set the RPC offset/scale Coefficients
        virtual void SetRPCTranslationCoeffs(const double xoffset, const double xscale,
            const double yoffset, const double yscale,
            const double zoffset, const double zscale,
            const double pixoffset, const double pixscale,
            const double lineoffset, const double linescale) = 0;

        // Get the adjusted X values
        virtual std::vector<double> GetAdjXValues(void) const = 0;
        // Get the adjusted Y values
        virtual std::vector<double> GetAdjYValues(void) const = 0;

        // Set the adjusted X/Y values
        virtual void SetAdjCoordValues(const std::vector<double>& xcoord,
            const std::vector<double>& ycoord) = 0;

        // Get whether or not this is a user-generated RPC model
        virtual bool IsUserGenerated(void) const = 0;
        // Set whether or not this is a user-generated RPC model
        virtual void SetUserGenerated(bool usergen) = 0;

        // Get whether the model has been adjusted
        virtual bool IsNominalModel(void) const = 0;
        // Set whether the model has been adjusted
        virtual void SetIsNominalModel(bool nominal) = 0;

        // Get sensor name
        virtual std::string GetSensorName(void) const = 0;
        // Set sensor name
        virtual void SetSensorName(const std::string& name) = 0;

        // Output projection information of RPC Model
        // Get the Geosys String
        virtual void GetMapUnits(std::string& map_units, std::string& proj_parms) const = 0;
        // Set the Geosys string
        virtual void SetMapUnits(std::string const& map_units, std::string const& proj_parms) = 0;

        // Get the number of lines
        virtual unsigned int GetLines(void) const = 0;

        // Get the number of pixels
        virtual unsigned int GetPixels(void) const = 0;

        // Set the number of lines/pixels
        virtual void SetRasterSize(const unsigned int lines, const unsigned int pixels) = 0;

        // Set/get the downsample factor
        virtual void SetDownsample(const unsigned int downsample) = 0;
        virtual unsigned int GetDownsample(void) const = 0;

        // TODO: Setting/getting detailed projection params (just GCTP params?)

        // Virtual destructor
        virtual ~PCIDSKRPCSegment() {}
    };
}

#endif // INCLUDE_PCIDSK_PCIDSK_RPC_H
