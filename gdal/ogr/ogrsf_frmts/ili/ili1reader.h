/******************************************************************************
 * $Id$
 *
 * Project:  Interlis 1 Reader
 * Purpose:  Private Declarations for Reader code.
 * Author:   Pirmin Kalberer, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef _CPL_ILI1READER_H_INCLUDED
#define _CPL_ILI1READER_H_INCLUDED

#include "imdreader.h"

class OGRILI1DataSource;

class CPL_DLL IILI1Reader
{
public:
    virtual     ~IILI1Reader();

    virtual int  OpenFile( const char *pszFilename ) = 0;
    
    virtual int  ReadModel( ImdReader *poImdReader, const char *pszModelFilename, OGRILI1DataSource *poDS ) = 0;
    virtual int  ReadFeatures() = 0;       
    
    virtual OGRLayer *GetLayer( int ) = 0;
    virtual OGRLayer *GetLayerByName( const char* ) = 0;
    virtual int  GetLayerCount() = 0;
};

IILI1Reader *CreateILI1Reader();
void DestroyILI1Reader(IILI1Reader* reader);

#endif
