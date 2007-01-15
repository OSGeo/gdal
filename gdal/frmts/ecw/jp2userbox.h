/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  ECW Driver: user defined data box.  Simple one to read/write
 *           user defined data.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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
 ****************************************************************************/

#ifndef JP2USERBOX_H_INCLUDED
#define JP2USERBOX_H_INCLUDED

#include "vsiiostream.h"
//#include "NCSJP2Box.h"

class NCSJPC_EXPORT_ALL JP2UserBox : public CNCSJP2Box {

private:
    int           nDataLength;
    unsigned char *pabyData;

public:
    JP2UserBox();

    virtual ~JP2UserBox();

    virtual CNCSError Parse(class CNCSJP2File &JP2File, 
                            CNCSJPCIOStream &Stream);
    virtual CNCSError UnParse(class CNCSJP2File &JP2File, 
                              CNCSJPCIOStream &Stream);
    virtual void UpdateXLBox(void);

    void    SetData( int nDataLength, const unsigned char *pabyDataIn );
    
    int     GetDataLength() { return nDataLength; }
    unsigned char *GetData() { return pabyData; }
};

#endif /* ndef JP2USERBOX_H_INCLUDED */

