/******************************************************************************
 * $Id$
 *
 * Project:  JPEG-2000
 * Purpose:  Implements VSI*L based writer.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
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

#ifndef VSIL_TARGET_H
#define VSIL_TARGET_H

#include "kdu_file_io.h"
#include "cpl_error.h"
#include "cpl_vsi.h"

/************************************************************************/
/*                             vsil_target                              */
/************************************************************************/

class vsil_target : public kdu_compressed_target {

public:
    vsil_target() { file = nullptr; }
    ~vsil_target() { vsil_target::close(); }

    void open(const char *fname, const char *access )
        {
            close();
            file = VSIFOpenL( fname, access );
            if( file == nullptr )
            {
                kdu_error e;
                e << "Unable to open compressed data file, \"" <<
                    fname << "\"!";
                return;
            }
        }

    bool write(const kdu_byte *buf, int num_bytes) override
        {
            if( file == nullptr )
                return false;

            if( (int) VSIFWriteL( buf, 1, num_bytes, file ) != num_bytes )
                return false;
            else
                return true;
        }

    bool start_rewrite(kdu_long backtrack) override
        {
            if( file == nullptr )
                return false;

            if( VSIFSeekL( file, VSIFTellL(file)-backtrack, SEEK_SET ) != 0 )
                return false;
            else
                return true;
        }

    bool end_rewrite() override
        {
            if( file == nullptr )
                return false;

            if( VSIFSeekL( file, 0, SEEK_END ) != 0 )
                return false;
            else
                return true;
        }

    bool close() override
        {
            if (file != nullptr)
                VSIFCloseL( file );
            file = nullptr;
            return true;
        }

private: // Data
    VSILFILE *file;
};

#endif // VSIL_TARGET_H
