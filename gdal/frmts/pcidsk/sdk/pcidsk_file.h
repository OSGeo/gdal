/******************************************************************************
 *
 * Purpose: Declaration of the PCIDSKFile Interface
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
#ifndef __INCLUDE_PCIDSK_FILE_H
#define __INCLUDE_PCIDSK_FILE_H

#include "pcidsk_segment.h"
#include <string>
#include <vector>

namespace PCIDSK
{
/************************************************************************/
/*                              PCIDSKFile                              */
/************************************************************************/

    class PCIDSKChannel;
    class PCIDSKSegment;
    class PCIDSKInterfaces;
    class Mutex;
    
//! Top interface to PCIDSK (.pix) files.
    class PCIDSK_DLL PCIDSKFile
    {
    public:
        virtual ~PCIDSKFile() {};

        virtual PCIDSKInterfaces *GetInterfaces() = 0;

        virtual PCIDSKChannel  *GetChannel( int band ) = 0;
        virtual PCIDSKSegment  *GetSegment( int segment ) = 0;
        virtual std::vector<PCIDSKSegment *> GetSegments() = 0;

        virtual PCIDSK::PCIDSKSegment *GetSegment( int type, 
            std::string name, int previous = 0 ) = 0;

        virtual int GetWidth() const = 0;
        virtual int GetHeight() const = 0;
        virtual int GetChannels() const = 0;
        virtual std::string GetInterleaving() const = 0;
        virtual bool GetUpdatable() const = 0;
        virtual uint64 GetFileSize() const = 0; 

        virtual int  CreateSegment( std::string name, std::string description,
            eSegType seg_type, int data_blocks ) = 0;
        virtual void DeleteSegment( int segment ) = 0;
        virtual void CreateOverviews( int chan_count, int *chan_list, 
            int factor, std::string resampling ) = 0;

    // the following are only for pixel interleaved IO
        virtual int    GetPixelGroupSize() const = 0;
        virtual void *ReadAndLockBlock( int block_index, int xoff=-1, int xsize=-1) = 0;
        virtual void  UnlockBlock( bool mark_dirty = false ) = 0;

    // low level io, primarily internal.
        virtual void WriteToFile( const void *buffer, uint64 offset, uint64 size)=0;
        virtual void ReadFromFile( void *buffer, uint64 offset, uint64 size ) = 0;

        virtual void GetIODetails( void ***io_handle_pp, Mutex ***io_mutex_pp,
            std::string filename = "" ) = 0;

        virtual std::string GetMetadataValue( const std::string& key ) = 0;
        virtual void SetMetadataValue( const std::string& key, const std::string& value ) = 0;
        virtual std::vector<std::string> GetMetadataKeys() = 0;

        virtual void Synchronize() = 0;
    };
} // end namespace PCIDSK

#endif // __INCLUDE_PCIDSK_FILE_H
