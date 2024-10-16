/******************************************************************************
 *
 * Purpose:  PCIDSK LUT segment interface class.
 *
 ******************************************************************************
 * Copyright (c) 2015
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef INCLUDE_PCIDSK_LUT_H
#define INCLUDE_PCIDSK_LUT_H

#include <string>
#include <vector>

namespace PCIDSK
{
/************************************************************************/
/*                              PCIDSK_LUT                              */
/************************************************************************/

//! Interface to PCIDSK lookup table segment.

    class PCIDSK_DLL PCIDSK_LUT
    {
    public:
        virtual ~PCIDSK_LUT() {}

/**
\brief Read a LUT Segment (SEG_LUT).

@param lut      Lookup Table buffer (256 entries) into which the
lookup table is read. It consists of grey output values (lut[0-255].

*/
        virtual void ReadLUT(std::vector<unsigned char>& lut) = 0;

/**
\brief Write a LUT Segment.

@param lut      Lookup Table buffer (256 entries) from which the
lookup table is written. It consists of grey output values (lut[0-255].

*/
        virtual void WriteLUT(const std::vector<unsigned char>& lut) = 0;
    };
} // end namespace PCIDSK

#endif // INCLUDE_PCIDSK_LUT_H
