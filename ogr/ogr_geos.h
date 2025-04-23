/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Definitions related to support for use of GEOS in OGR.
 *           This file is only intended to be pulled in by OGR implementation
 *           code directly accessing GEOS.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_GEOS_H_INCLUDED
#define OGR_GEOS_H_INCLUDED

#ifdef HAVE_GEOS
// To avoid accidental use of non reentrant GEOS API.
// (check only effective in GEOS >= 3.5)
#define GEOS_USE_ONLY_R_API

#include <geos_c.h>
#else

namespace geos
{
class Geometry;
}

#endif

#endif /* ndef OGR_GEOS_H_INCLUDED */
