/******************************************************************************
 * $Id$
 *
 * Project:  Interlis 2 Reader
 * Purpose:  Public Declarations for Reader code.
 * Author:   Markus Schnider, Sourcepole AG
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

#ifndef CPL_ILI2READER_H_INCLUDED
#define CPL_ILI2READER_H_INCLUDED

#include "imdreader.h"
#include <list>

class IILI2Reader
{
public:
    virtual     ~IILI2Reader();

    virtual void SetSourceFile( const char *pszFilename ) = 0;

    virtual int  ReadModel( ImdReader *poImdReader, const char *modelFilename ) = 0;
    virtual int  SaveClasses( const char *pszFilename ) = 0;

    virtual std::list<OGRLayer *> GetLayers() = 0;
    virtual int GetLayerCount() = 0;
};

IILI2Reader *CreateILI2Reader();
void DestroyILI2Reader(IILI2Reader* reader);

#endif
