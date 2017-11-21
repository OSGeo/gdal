/******************************************************************************
 * $Id$
 *
 * Project:  DXF and DWG Translators
 * Purpose:  Declarations for shared AutoCAD format services.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2011,  Frank Warmerdam
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

#ifndef OGR_AUTOCAD_SERVICES_H_INCLUDED
#define OGR_AUTOCAD_SERVICES_H_INCLUDED

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_geometry.h"
#include "ogr_feature.h"

/* -------------------------------------------------------------------- */
/*      Various Functions.                                              */
/* -------------------------------------------------------------------- */
CPLString ACTextUnescape( const char *pszInput, const char *pszEncoding,
    bool bIsMText );

const unsigned char *ACGetColorTable( void );

const int* ACGetKnownDimStyleCodes( void );

const char *ACGetDimStylePropertyName( const int iDimStyleCode );

const char *ACGetDimStylePropertyDefault( const int iDimStyleCode );

void ACAdjustText( const double dfAngle, const double dfScaleX,
    const double dfScaleY, OGRFeature* const poFeature );

#endif /* ndef OGR_AUTOCAD_SERVICES_H_INCLUDED */
