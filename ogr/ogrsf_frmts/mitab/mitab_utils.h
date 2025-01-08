/**********************************************************************
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
 * SPDX-License-Identifier: MIT
 **********************************************************************/

#ifndef MITAB_UTILS_H_INCLUDED_
#define MITAB_UTILS_H_INCLUDED_

#include "ogr_geometry.h"

#define COLOR_R(color) ((color & 0xff0000) / 0x10000)
#define COLOR_G(color) ((color & 0xff00) / 0x100)
#define COLOR_B(color) (color & 0xff)

/*=====================================================================
                        Function prototypes
 =====================================================================*/

int TABGenerateArc(OGRLineString *poLine, int numPoints, double dCenterX,
                   double dCenterY, double dXRadius, double dYRadius,
                   double dStartAngle, double dEndAngle);
int TABCloseRing(OGRLineString *poRing);

GBool TABAdjustFilenameExtension(char *pszFname);
char *TABGetBasename(const char *pszFname);
char **TAB_CSLLoad(const char *pszFname);

char *TABEscapeString(char *pszString);
char *TABUnEscapeString(char *pszString, GBool bSrcIsConst);

char *TABCleanFieldName(const char *pszSrcName, const char *pszCharset,
                        bool bStrictLaundering);

void TABSaturatedAdd(GInt32 &nVal, GInt32 nAdd);
GInt16 TABInt16Diff(int a, int b);

#endif /* MITAB_UTILS_H_INCLUDED_ */
