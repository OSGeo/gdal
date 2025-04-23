/******************************************************************************
 *
 * Purpose: Interface. Builder class for constructing a related PCIDSK segment
 *          class.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
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
