/**********************************************************************
 * $Id: mitab_utils.h,v 1.10 2004-06-30 20:29:04 dmorissette Exp $
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
 **********************************************************************
 *
 * $Log: mitab_utils.h,v $
 * Revision 1.10  2004-06-30 20:29:04  dmorissette
 * Fixed refs to old address danmo@videotron.ca
 *
 * Revision 1.9  2001/01/23 21:23:42  daniel
 * Added projection bounds lookup table, called from TABFile::SetProjInfo()
 *
 * Revision 1.8  2000/02/18 20:46:58  daniel
 * Added TABCleanFieldName()
 *
 * Revision 1.7  2000/01/15 22:30:45  daniel
 * Switch to MIT/X-Consortium OpenSource license
 *
 * Revision 1.6  2000/01/14 23:47:00  daniel
 * Added TABEscapeString()/TABUnEscapeString()
 *
 * Revision 1.5  1999/12/14 02:08:16  daniel
 * Added TABGetBasename() + TAB_CSLLoad()
 *
 * Revision 1.4  1999/09/28 13:33:32  daniel
 * Moved definition for PI to mitab.h
 *
 * Revision 1.3  1999/09/26 14:59:38  daniel
 * Implemented write support
 *
 * Revision 1.2  1999/09/16 02:39:17  daniel
 * Completed read support for most feature types
 *
 * Revision 1.1  1999/07/12 04:18:25  daniel
 * Initial checkin
 *
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

#endif /* MITAB_UTILS_H_INCLUDED_ */
