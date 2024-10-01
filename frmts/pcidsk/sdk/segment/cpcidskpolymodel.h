/******************************************************************************
 *
 * Purpose: Support for reading and manipulating PCIDSK Polynomial Segments
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef INCLUDE_PCIDSK_SEGMENT_PCIDSKPOLYMODEL_H
#define INCLUDE_PCIDSK_SEGMENT_PCIDSKPOLYMODEL_H

#include "pcidsk_poly.h"
#include "segment/cpcidsksegment.h"

namespace PCIDSK {
    class PCIDSKFile;

    class CPCIDSKPolyModelSegment : public PCIDSKPolySegment,
                                     public CPCIDSKSegment
    {
    public:
        CPCIDSKPolyModelSegment(PCIDSKFile *file, int segment,const char *segment_pointer);
        ~CPCIDSKPolyModelSegment();

        std::vector<double> GetXForwardCoefficients() const override;
        std::vector<double> GetYForwardCoefficients() const override;
        std::vector<double> GetXBackwardCoefficients() const override;
        std::vector<double> GetYBackwardCoefficients() const override;

        void SetCoefficients(const std::vector<double>& oXForward,
                             const std::vector<double>& oYForward,
                             const std::vector<double>& oXBackward,
                             const std::vector<double>& oYBackward) override ;

        unsigned int GetLines() const override;
        unsigned int GetPixels() const override;
        void SetRasterSize(unsigned int nLines,unsigned int nPixels) override ;

        std::string GetGeosysString() const override;
        void SetGeosysString(const std::string& oGeosys) override ;

        std::vector<double> GetProjParamInfo() const override;
        void SetProjParamInfo(const std::vector<double>& oInfo) override ;

        //synchronize the segment on disk.
        void Synchronize() override;
    private:
        // Helper housekeeping functions
        void Load();
        void Write();

        // Struct to store details of the RPC model
        struct PCIDSKPolyInfo
        {
            // number of coefficients
            unsigned int nNumCoeffs;

            // pixels in the image
            unsigned int nPixels;
            // lines in the image
            unsigned int nLines;

            // Forward Coefficients (Geo2Img)
            std::vector<double> vdfX1;
            // Forward Coefficients (Geo2Img)
            std::vector<double> vdfY1;
            // Backward Coefficients Img2Geo
            std::vector<double> vdfX2;
            // Backward Coefficients Img2Geo
            std::vector<double> vdfY2;

            //map units of required projection
            std::string oMapUnit;
            //proj param info of required projection
            std::vector<double> oProjectionInfo;

            // The raw segment data
            PCIDSKBuffer seg_data;
        };

        PCIDSKPolyInfo *pimpl_;
        bool loaded_;
        bool mbModified;
    };
}

#endif // INCLUDE_PCIDSK_SEGMENT_PCIDSKPOLYMODEL_H
