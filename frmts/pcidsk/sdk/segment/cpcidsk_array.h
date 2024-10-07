/******************************************************************************
 *
 * Purpose:  Declaration of the CPCIDSK_ARRAY class.
 *
 ******************************************************************************
 * Copyright (c) 2010
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef INCLUDE_SEGMENT_PCIDSK_ARRAY_H
#define INCLUDE_SEGMENT_PCIDSK_ARRAY_H

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "pcidsk_array.h"
#include "pcidsk_buffer.h"
#include "segment/cpcidsksegment.h"

#include <string>

namespace PCIDSK
{
    class PCIDSKFile;

    /************************************************************************/
    /*                            CPCIDSK_ARRAY                             */
    /************************************************************************/

    class CPCIDSK_ARRAY : public CPCIDSKSegment,
                          public PCIDSK_ARRAY
    {
    public:
        CPCIDSK_ARRAY( PCIDSKFile *file, int segment,const char *segment_pointer);

        virtual     ~CPCIDSK_ARRAY();

        // CPCIDSK_ARRAY
        unsigned char GetDimensionCount() const override ;
        void SetDimensionCount(unsigned char nDim) override ;
        const std::vector<unsigned int>& GetSizes() const override ;
        void SetSizes(const std::vector<unsigned int>& oSizes) override ;
        const std::vector<double>& GetArray() const override ;
        void SetArray(const std::vector<double>& oArray) override ;

        //synchronize the segment on disk.
        void Synchronize() override;
    private:

        //Headers are not supported by PCIDSK, we keep the function
        //private here in case we want to add the feature
        //in the future.
        const std::vector<std::string>&  GetHeaders() const ;
        void SetHeaders(const std::vector<std::string>& oHeaders) ;

        // Helper housekeeping functions
        void Load();
        void Write();

        //members
        PCIDSKBuffer seg_data;
        bool loaded_;
        bool mbModified;
        unsigned char MAX_DIMENSIONS;

        //Array information
        std::vector<std::string> moHeaders;
        unsigned char mnDimension;
        std::vector<unsigned int> moSizes;
        std::vector<double> moArray;
    };
} // end namespace PCIDSK

#endif // INCLUDE_SEGMENT_PCIDSK_ARRAY_H
