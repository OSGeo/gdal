/******************************************************************************
 *
 * Project:  JPEG-2000
 * Purpose:  Implements VSI*L based writer.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef VSIL_TARGET_H
#define VSIL_TARGET_H

#include "kdu_file_io.h"
#include "cpl_error.h"
#include "cpl_vsi.h"

/************************************************************************/
/*                             vsil_target                              */
/************************************************************************/

class vsil_target : public kdu_compressed_target
{

  public:
    vsil_target()
    {
        file = nullptr;
    }

    ~vsil_target()
    {
        vsil_target::close();
    }

    void open(const char *fname, const char *access)
    {
        close();
        file = VSIFOpenL(fname, access);
        if (file == nullptr)
        {
            kdu_error e;
            e << "Unable to open compressed data file, \"" << fname << "\"!";
            return;
        }
    }

    bool write(const kdu_byte *buf, int num_bytes) override
    {
        if (file == nullptr)
            return false;

        if ((int)VSIFWriteL(buf, 1, num_bytes, file) != num_bytes)
            return false;
        else
            return true;
    }

    bool start_rewrite(kdu_long backtrack) override
    {
        if (file == nullptr)
            return false;

        if (VSIFSeekL(file, VSIFTellL(file) - backtrack, SEEK_SET) != 0)
            return false;
        else
            return true;
    }

    bool end_rewrite() override
    {
        if (file == nullptr)
            return false;

        if (VSIFSeekL(file, 0, SEEK_END) != 0)
            return false;
        else
            return true;
    }

    bool close() override
    {
        if (file != nullptr)
            VSIFCloseL(file);
        file = nullptr;
        return true;
    }

  private:  // Data
    VSILFILE *file;
};

#endif  // VSIL_TARGET_H
