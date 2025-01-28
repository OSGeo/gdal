/******************************************************************************
 *
 * Project:  DXF and DWG Translators
 * Purpose:  Declarations for shared AutoCAD format services.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2011,  Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
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
CPLString ACTextUnescape(const char *pszInput, const char *pszEncoding,
                         bool bIsMText);

const unsigned char *ACGetColorTable(void);

const int *ACGetKnownDimStyleCodes(void);

const char *ACGetDimStylePropertyName(const int iDimStyleCode);

const char *ACGetDimStylePropertyDefault(const int iDimStyleCode);

void ACAdjustText(const double dfAngle, const double dfScaleX,
                  const double dfScaleY, OGRFeature *const poFeature);

#endif /* ndef OGR_AUTOCAD_SERVICES_H_INCLUDED */
