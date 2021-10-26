/******************************************************************************
 *
 * Purpose: Support for reading and manipulating PCIDSK RPC Segments
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
#ifndef INCLUDE_PCIDSK_SEGMENT_PCIDSKRPCMODEL_H
#define INCLUDE_PCIDSK_SEGMENT_PCIDSKRPCMODEL_H

#include "pcidsk_rpc.h"
#include "segment/cpcidsksegment.h"

namespace PCIDSK {
    class PCIDSKFile;

    class CPCIDSKRPCModelSegment : virtual public PCIDSKRPCSegment,
                                   public CPCIDSKSegment
    {
    public:
        CPCIDSKRPCModelSegment(PCIDSKFile *file, int segment,const char *segment_pointer);
        ~CPCIDSKRPCModelSegment();

        // Implementation of PCIDSKRPCSegment
        // Get the X and Y RPC coefficients
        std::vector<double> GetXNumerator(void) const override;
        std::vector<double> GetXDenominator(void) const override;
        std::vector<double> GetYNumerator(void) const override;
        std::vector<double> GetYDenominator(void) const override;

        // Set the X and Y RPC Coefficients
        void SetCoefficients(const std::vector<double>& xnum,
            const std::vector<double>& xdenom, const std::vector<double>& ynum,
            const std::vector<double>& ydenom) override;

        // Get the RPC offset/scale Coefficients
        void GetRPCTranslationCoeffs(double& xoffset, double& xscale,
            double& yoffset, double& yscale, double& zoffset, double& zscale,
            double& pixoffset, double& pixscale, double& lineoffset, double& linescale) const override;

        // Set the RPC offset/scale Coefficients
        void SetRPCTranslationCoeffs(
            const double xoffset, const double xscale,
            const double yoffset, const double yscale,
            const double zoffset, const double zscale,
            const double pixoffset, const double pixscale,
            const double lineoffset, const double linescale) override;

        // Get the adjusted X values
        std::vector<double> GetAdjXValues(void) const override;
        // Get the adjusted Y values
        std::vector<double> GetAdjYValues(void) const override;

        // Set the adjusted X/Y values
        void SetAdjCoordValues(const std::vector<double>& xcoord,
            const std::vector<double>& ycoord) override;

        // Get whether or not this is a user-generated RPC model
        bool IsUserGenerated(void) const override;
        // Set whether or not this is a user-generated RPC model
        void SetUserGenerated(bool usergen) override;

        // Get whether the model has been adjusted
        bool IsNominalModel(void) const override;
        // Set whether the model has been adjusted
        void SetIsNominalModel(bool nominal) override;

        // Get sensor name
        std::string GetSensorName(void) const override;
        // Set sensor name
        void SetSensorName(const std::string& name) override;

        // Output projection information of RPC Model
        void GetMapUnits(std::string& map_units, std::string& proj_parms) const override;

        // Set the map units
        void SetMapUnits(std::string const& map_units, std::string const& proj_parms) override;

        // Get the number of lines
        unsigned int GetLines(void) const override;

        // Get the number of pixels
        unsigned int GetPixels(void) const override;

        // Set the number of lines/pixels
        void SetRasterSize(const unsigned int lines, const unsigned int pixels) override;

        // Set the downsample factor
        void SetDownsample(const unsigned int downsample) override;

        // Get the downsample factor
        unsigned int GetDownsample(void) const override;

        //synchronize the segment on disk.
        void Synchronize() override;
    private:
        // Helper housekeeping functions
        void Load();
        void Write();

        struct PCIDSKRPCInfo;
        PCIDSKRPCInfo *pimpl_;
        bool loaded_;
        bool mbModified;

        //this member is used when the segment was newly created
        //and nothing was yet set in it.
        bool mbEmpty;
    };
}

#endif // INCLUDE_PCIDSK_SEGMENT_PCIDSKRPCMODEL_H
