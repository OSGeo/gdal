/******************************************************************************
 *
 * Purpose: Support for reading and manipulating PCIDSK Polynomial Segments
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

        struct PCIDSKPolyInfo;
        PCIDSKPolyInfo *pimpl_;
        bool loaded_;
        bool mbModified;
    };
}

#endif // INCLUDE_PCIDSK_SEGMENT_PCIDSKPOLYMODEL_H
