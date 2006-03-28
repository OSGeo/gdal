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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.4  2006/03/28 16:07:14  pka
 * Optional model file for Interlis 2 reader
 *
 * Revision 1.3  2005/08/06 22:21:53  pka
 * Area polygonizer added
 *
 * Revision 1.2  2005/07/11 19:42:42  pka
 * Syntax fix
 *
 * Revision 1.1  2005/07/08 22:10:57  pka
 * Initial import of OGR Interlis driver
 *
 */

#ifndef _CPL_ILI2READER_H_INCLUDED
#define _CPL_ILI2READER_H_INCLUDED

// This works around problems with math.h on some platforms #defining INFINITY
#ifdef INFINITY
#undef  INFINITY
#define INFINITY INFINITY_XERCES
#endif

#include <list>


class CPL_DLL IILI2Reader
{
public:
    virtual     ~IILI2Reader();

    virtual void SetSourceFile( const char *pszFilename ) = 0;
    virtual int  ReadModel( const char *pszModelFilename ) = 0;
    virtual int  SaveClasses( const char *pszFilename ) = 0;
    
    virtual std::list<OGRLayer *> GetLayers() = 0;
    virtual int GetLayerCount() = 0;
    virtual void SetArcDegrees(double newArcDegrees) = 0;
};

IILI2Reader *CreateILI2Reader();

#endif
