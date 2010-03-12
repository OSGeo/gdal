/******************************************************************************
 *
 * Purpose:  Declaration of the SysBlockMap class.
 *
 * This class is used to manage access to the SYS virtual block map segment
 * (named SysBMDir).  This segment is used to keep track of one or more 
 * virtual files stored in SysBData segments.  These virtual files are normally
 * used to hold tiled images for primary bands or overviews.  
 *
 * This class is closely partnered with the SysVirtualFile class, and the
 * primary client is the CTiledChannel class. 
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
#ifndef __INCLUDE_SEGMENT_SYSBLOCKMAP_H
#define __INCLUDE_SEGMENT_SYSBLOCKMAP_H

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "pcidsk_segment.h"
#include "segment/cpcidsksegment.h"

namespace PCIDSK
{
    class SysVirtualFile;
    class PCIDSKFile;
    
    /************************************************************************/
    /*                             SysBlockMap                              */
    /************************************************************************/

    class SysBlockMap : public CPCIDSKSegment
    {
    public:
        SysBlockMap( PCIDSKFile *file, int segment,const char *segment_pointer );

        virtual        ~SysBlockMap();

        virtual void    Synchronize();
        virtual void    Initialize();

        SysVirtualFile *GetVirtualFile( int image );
        int             CreateVirtualFile();
        int             CreateVirtualImageFile( int width, int height, 
                                                int block_width, int block_height,
                                                eChanType chan_type,
                                                std::string compression );
        int             GrowVirtualFile( int image, int &last_block,
                                         int &block_segment_ret );
        void            SetVirtualFileSize( int image, uint64 file_length );
    
    private:
        bool         loaded;
        bool         dirty;

        void         Load();
        void         AllocateBlocks();

        PCIDSKBuffer seg_data;

        int          block_count;
        int          first_free_block;

        int          block_map_offset;
        int          layer_list_offset;

        int          growing_segment;

        std::vector<SysVirtualFile*> virtual_files;
    };
} // end namespace PCIDSK

#endif // __INCLUDE_SEGMENT_SYSBLOCKMAP_H
