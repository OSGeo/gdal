/******************************************************************************
 *
 * Purpose: Interface. Builder class for constructing a related PCIDSK segment
 *          class.
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
 
#ifndef INCLUDE_SEGMENT_PCIDSKSEGMENTBUILDER_H
#define INCLUDE_SEGMENT_PCIDSKSEGMENTBUILDER_H

namespace PCIDSK
{
    class PCIDSKSegment;
    class PCIDSKFile;

    /**
     * PCIDSK Abstract Builder class. Given a segment pointer, constructs
     * an instance of a given PCIDSKSegment implementer. Typically an instance
     * of this will be registered with the PCIDSK Segment Factory.
     */
    struct IPCIDSKSegmentBuilder
    {
        // TODO: Determine required arguments for a segment to be constructed
        virtual PCIDSKSegment *BuildSegment(PCIDSKFile *poFile, 
            unsigned int nSegmentID, const char *psSegmentPointer) = 0;
        
        // Get a list of segments that this builder can handle
        virtual std::vector<std::string> GetSupportedSegments(void) const = 0;
        
        // Get a copyright string
        virtual std::string GetVendorString(void) const = 0;
        
        // Get a name string
        virtual std::string GetSegmentBuilderName(void) const = 0;
        
        // Virtual destructor
        virtual ~IPCIDSKSegmentBuilder() {}
    };

}; // end namespace PCIDSK

#endif // INCLUDE_SEGMENT_PCIDSKSEGMENTBUILDER_H
