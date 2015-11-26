/******************************************************************************
 * $Id$
 *
 * Project:  Multi-resolution Seamless Image Database (MrSID)
 * Purpose:  Input/output stream wrapper for usage with LizardTech's
 *           MrSID SDK, wrapper class declaration.
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2008, Andrey Kiselev <dron@ak4719.spb.edu>
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

#ifndef MRSIDSTREAM_H_INCLUDED
#define MRSIDSTREAM_H_INCLUDED

#include "lt_base.h"
#include "lt_ioStreamInf.h"
#include "cpl_vsi_virtual.h"

LT_USE_NAMESPACE(LizardTech)

class LTIVSIStream : public LTIOStreamInf
{
  public:
    LTIVSIStream();
    LT_STATUS initialize( const char *, const char * );
    LT_STATUS initialize( LTIVSIStream* ltiVSIStream );
    ~LTIVSIStream();

    bool isEOF();
    bool isOpen();
    
    LT_STATUS open();
    LT_STATUS close();
    
    lt_uint32 read( lt_uint8 *, lt_uint32 );
    lt_uint32 write( const lt_uint8 *, lt_uint32 );
    
    LT_STATUS seek( lt_int64, LTIOSeekDir );
    lt_int64 tell();
    
    LTIOStreamInf* duplicate();
    
    LT_STATUS getLastError() const;
    
    const char* getID() const;

  private:
    VSIVirtualHandle    *poFileHandle;
    int                 nError;
    int                 *pnRefCount;
    int                 bIsOpen;
};

#endif /* MRSIDSTREAM_H_INCLUDED */

