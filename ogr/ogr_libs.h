/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Definitions related to support for use of SFCGAL and GEOS in OGR.
 *           This file is only intended to be pulled in by OGR implementation
 *           code directly accessing SFCGAL and/or GEOS.
 * Author:   Avyav Kumar Singh <avyavkumar at gmail dot com>
 *
 ******************************************************************************
 * Copyright (c) 2016, Avyav Kumar Singh <avyavkumar at gmail dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef HAVE_GEOS
#define UNUSED_IF_NO_GEOS CPL_UNUSED
#else
#define UNUSED_IF_NO_GEOS
#endif

#ifndef HAVE_SFCGAL
#define UNUSED_IF_NO_SFCGAL CPL_UNUSED
#else
#define UNUSED_IF_NO_SFCGAL
#endif

#ifndef UNUSED_PARAMETER

#ifdef HAVE_GEOS
#ifndef HAVE_SFCGAL
#define UNUSED_PARAMETER                                                       \
    UNUSED_IF_NO_SFCGAL  // SFCGAL no and GEOS yes - GEOS methods always work
#else
#define UNUSED_PARAMETER  // Both libraries are present
#endif
#endif

#ifndef HAVE_GEOS
#ifdef HAVE_SFCGAL
#define UNUSED_PARAMETER                                                       \
    UNUSED_IF_NO_GEOS  // SFCGAL yes and GEOS no - SFCGAL methods always work
#else
#define UNUSED_PARAMETER                                                       \
    CPL_UNUSED  // Neither of the libraries have support enabled
#endif
#endif

#endif
