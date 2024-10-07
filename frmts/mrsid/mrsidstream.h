/******************************************************************************
 *
 * Project:  Multi-resolution Seamless Image Database (MrSID)
 * Purpose:  Input/output stream wrapper for usage with LizardTech's
 *           MrSID SDK, implementation of the wrapper class methods.
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2008, Andrey Kiselev <dron@ak4719.spb.edu>
 * Copyright (c) 2008-2010, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef MRSIDSTREAM_H_INCLUDED
#define MRSIDSTREAM_H_INCLUDED

#include "mrsidstream_headers_include.h"

#include "cpl_vsi_virtual.h"

using namespace LizardTech;

class LTIVSIStream : public LTIOStreamInf
{
  public:
    LTIVSIStream();
    LT_STATUS initialize(const char *, const char *);
    LT_STATUS initialize(LTIVSIStream *ltiVSIStream);
    ~LTIVSIStream();

    bool isEOF() override;
    bool isOpen() override;

    LT_STATUS open() override;
    LT_STATUS close() override;

    lt_uint32 read(lt_uint8 *, lt_uint32) override;
    lt_uint32 write(const lt_uint8 *, lt_uint32) override;

    LT_STATUS seek(lt_int64, LTIOSeekDir) override;
    lt_int64 tell() override;

    LTIOStreamInf *duplicate() override;

    LT_STATUS getLastError() const override;

    const char *getID() const override;

  private:
    VSIVirtualHandle *poFileHandle;
    int nError;
    int *pnRefCount;
    int bIsOpen;
};

#endif /* MRSIDSTREAM_H_INCLUDED */
