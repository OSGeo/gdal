/******************************************************************************
 *
 * Purpose: Interface representing access to a PCIDSK Polynomial Segment
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
#ifndef INCLUDE_PCIDSK_PCIDSK_POLY_H
#define INCLUDE_PCIDSK_PCIDSK_POLY_H

#include <vector>
#include <string>

namespace PCIDSK {
//! Interface to PCIDSK Polynomial segment.
    class PCIDSKPolySegment
    {
    public:
        //Get the coefficients
        virtual std::vector<double> GetXForwardCoefficients() const=0;
        virtual std::vector<double> GetYForwardCoefficients() const=0;
        virtual std::vector<double> GetXBackwardCoefficients() const=0;
        virtual std::vector<double> GetYBackwardCoefficients() const=0;

        //Set the coefficients
        virtual void SetCoefficients(const std::vector<double>& oXForward,
                                     const std::vector<double>& oYForward,
                                     const std::vector<double>& oXBackward,
                                     const std::vector<double>& oYBackward) =0;

        // Get the number of lines
        virtual unsigned int GetLines() const=0;
        // Get the number of pixels
        virtual unsigned int GetPixels() const=0;
        // Set the number of lines/pixels
        virtual void SetRasterSize(unsigned int nLines,unsigned int nPixels) =0;

        // Get the Geosys String
        virtual std::string GetGeosysString() const=0;
        // Set the Geosys string
        virtual void SetGeosysString(const std::string& oGeosys) =0;

        //Get the projection information
        virtual std::vector<double> GetProjParamInfo() const=0;
        //Set the projection information
        virtual void SetProjParamInfo(const std::vector<double>& oInfo) =0;

        // Virtual destructor
        virtual ~PCIDSKPolySegment() {}
    };
}

#endif // INCLUDE_PCIDSK_PCIDSK_POLY_H
