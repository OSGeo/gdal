/******************************************************************************
 *
 * Purpose:  PCIDSK ARRAY segment interface class.
 * 
 ******************************************************************************
 * Copyright (c) 2010
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
#ifndef __INCLUDE_PCIDSK_ARRAY_H
#define __INCLUDE_PCIDSK_ARRAY_H

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
        virtual	~PCIDSK_ARRAY() {}

        //ARRAY functions
        virtual	unsigned char GetDimensionCount() const =0;
        virtual	void SetDimensionCount(unsigned char nDim) =0;
        virtual	const std::vector<unsigned int>& GetSizes() const =0;
        virtual	void SetSizes(const std::vector<unsigned int>& oSizes) =0;
        virtual	const std::vector<double>& GetArray() const =0;
        virtual	void SetArray(const std::vector<double>& oArray) =0;
    };
} // end namespace PCIDSK

#endif // __INCLUDE_PCIDSK_ARRAY_H
