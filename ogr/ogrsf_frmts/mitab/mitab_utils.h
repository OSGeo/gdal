/**********************************************************************
 * $Id$
 *
 * Name:     mitab_utils.h
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Header file containing definitions of misc. util functions.
 * Author:   Daniel Morissette, dmorissette@dmsolutions.ca
 *
 **********************************************************************
 * Copyright (c) 1999-2001, Daniel Morissette
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
 **********************************************************************/

#ifndef MITAB_UTILS_H_INCLUDED_
#define MITAB_UTILS_H_INCLUDED_

#include "ogr_geometry.h"

#define COLOR_R(color) ((color&0xff0000)/0x10000)
#define COLOR_G(color) ((color&0xff00)/0x100)
#define COLOR_B(color) (color&0xff)

/*=====================================================================
                        Function prototypes
 =====================================================================*/

int TABGenerateArc(OGRLineString *poLine, int numPoints,
                   double dCenterX, double dCenterY,
                   double dXRadius, double dYRadius,
                   double dStartAngle, double dEndAngle);
int TABCloseRing(OGRLineString *poRing);

GBool TABAdjustFilenameExtension(char *pszFname);
char *TABGetBasename(const char *pszFname);
char **TAB_CSLLoad(const char *pszFname);

char *TABEscapeString(char *pszString);
char *TABUnEscapeString(char *pszString, GBool bSrcIsConst);

char *TABCleanFieldName(const char *pszSrcName);

const char *TABUnitIdToString(int nId);
int   TABUnitIdFromString(const char *pszName);

void TABSaturatedAdd(GInt32& nVal, GInt32 nAdd);
GInt16 TABInt16Diff(int a, int b);

#endif /* MITAB_UTILS_H_INCLUDED_ */
