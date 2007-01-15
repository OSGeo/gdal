/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Various classid objects for ``well known'' SF COM classes.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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

#ifndef _SFCLSID_H_INCLUDED
#define _SFCLSID_H_INCLUDED

DEFINE_GUID(CLSID_SampProv, 0xE8CCCB79L,0x7C36,0x101B,0xAC,0x3A,0x00,0xAA,0x00,0x44,0x77,0x3D);

DEFINE_GUID(CLSID_CadcorpSFProvider,
       0x88c1b679L,0xc001,0x11d2,0x85,0x0b,0x00,0xc0,0x4f,0x72,0xee,0xf7);

DEFINE_GUID(CLSID_SFProvider,
       0xC9BD5075L,0x0D6D,0x11D3,0x94,0xFF,0x00,0x10,0x4B,0x23,0x89,0x35);

DEFINE_GUID(CLSID_CadcorpSFGeometryFactory,
            0xa71279eb,0xac51,0x11d2,0x84,0xfa,0x00,0xc0,0x4f,0x72,0xee,0xf7);

DEFINE_GUID(CLSID_CadcorpSFSpatialReferenceFactory,
       0x096f5734,0xb4d2,0x11d1,0xa6,0x23,0x00,0x40,0x95,0x24,0x50,0x6c);

DEFINE_GUID(CLSID_OGRComClassFactory,
            0xbc518ff0,0x08dd,0x11d3,0xb3,0xf9,0x00,0x80,0xc8,0xe6,0x25,0x64);

DEFINE_GUID(CLSID_JETOLEDB_3_51,
            0xdee35060,0x506b,0x11cf,0xb1,0xaa,0x00,0xaa,0x00,0xb8,0xde,0x95);

#endif
