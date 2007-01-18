/******************************************************************************
 * $Id$
 *
 * Project:  Interlis 1/2 Translator
 * Purpose:   Definition of classes for OGR Interlis 1 driver.
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef _IOMHELPER_H_INCLUDED
#define _IOMHELPER_H_INCLUDED

#include "iom/iom.h"

IOM_OBJECT GetAttrObj(IOM_BASKET model, IOM_OBJECT obj, const char* attrname);
int GetAttrObjPos(IOM_OBJECT obj, const char* attrname);
const char* GetAttrObjName(IOM_BASKET model, IOM_OBJECT obj, const char* attrname);
IOM_OBJECT GetTypeObj(IOM_BASKET model, IOM_OBJECT obj);
const char* GetTypeName(IOM_BASKET model, IOM_OBJECT obj);
unsigned int GetCoordDim(IOM_BASKET model, IOM_OBJECT typeobj);
const char* GetAttrObjName(IOM_BASKET model, const char* tagname);

#endif /* _IOMHELPER_H_INCLUDED */
