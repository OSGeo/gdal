/******************************************************************************
 *
 * Purpose:  PCIDSK External Database Interface declaration.  This provides
 *           mechanisms for access to external linked image file formats.
 *
 ******************************************************************************
 * Copyright (c) 2010
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
#ifndef INCLUDE_PCIDSK_EDB_H
#define INCLUDE_PCIDSK_EDB_H

#include "pcidsk_config.h"

#include <string>

namespace PCIDSK
{
/************************************************************************/
/*                               EDBFile                                */
/************************************************************************/

//! External Database Interface class.

    class EDBFile
    {
    public:
        virtual ~EDBFile() {}
        virtual int Close() const = 0;

        virtual int GetWidth() const = 0;
        virtual int GetHeight() const = 0;
        virtual int GetChannels() const = 0;
        virtual int GetBlockWidth(int channel ) const = 0;
        virtual int GetBlockHeight(int channel ) const = 0;
        virtual eChanType GetType(int channel ) const = 0;
        virtual int ReadBlock(int channel,
            int block_index, void *buffer,
            int win_xoff=-1, int win_yoff=-1,
            int win_xsize=-1, int win_ysize=-1 ) = 0;
        virtual int WriteBlock( int channel, int block_index, void *buffer) = 0;
    };

    EDBFile PCIDSK_DLL *DefaultOpenEDB(const std::string& filename,
                                       const std::string& access);
} // end namespace PCIDSK

#endif // INCLUDE_PCIDSK_EDB_H
