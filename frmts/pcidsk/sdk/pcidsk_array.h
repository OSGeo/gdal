/******************************************************************************
 *
 * Purpose:  PCIDSK ARRAY segment interface class.
 *
 ******************************************************************************
 * Copyright (c) 2010
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef INCLUDE_PCIDSK_ARRAY_H
#define INCLUDE_PCIDSK_ARRAY_H

#include <string>
#include <vector>

namespace PCIDSK
{
/************************************************************************/
/*                              PCIDSK_ARRAY                            */
/************************************************************************/

//! Interface to PCIDSK text segment.

    class PCIDSK_DLL PCIDSK_ARRAY
    {
    public:
        virtual ~PCIDSK_ARRAY() {}

        //ARRAY functions
        virtual unsigned char GetDimensionCount() const =0;
        virtual void SetDimensionCount(unsigned char nDim) =0;
        virtual const std::vector<unsigned int>& GetSizes() const =0;
        virtual void SetSizes(const std::vector<unsigned int>& oSizes) =0;
        virtual const std::vector<double>& GetArray() const =0;
        virtual void SetArray(const std::vector<double>& oArray) =0;
    };
} // end namespace PCIDSK

#endif // INCLUDE_PCIDSK_ARRAY_H
