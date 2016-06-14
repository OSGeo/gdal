/******************************************************************************
 *
 * Purpose: Support for reading and manipulating PCIDSK Polynomial Segments
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
        
        std::vector<double> GetXForwardCoefficients() const;
        std::vector<double> GetYForwardCoefficients() const;
        std::vector<double> GetXBackwardCoefficients() const;
        std::vector<double> GetYBackwardCoefficients() const;

        void SetCoefficients(const std::vector<double>& oXForward,
                             const std::vector<double>& oYForward,
                             const std::vector<double>& oXBackward,
                             const std::vector<double>& oYBackward) ;

        unsigned int GetLines() const;
        unsigned int GetPixels() const;
        void SetRasterSize(unsigned int nLines,unsigned int nPixels) ;

        std::string GetGeosysString() const;
        void SetGeosysString(const std::string& oGeosys) ;

        std::vector<double> GetProjParmInfo() const;
        void SetProjParmInfo(const std::vector<double>& oInfo) ;

        //synchronize the segment on disk.
        void Synchronize();
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
