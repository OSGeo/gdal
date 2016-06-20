/******************************************************************************
 * $Id$
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
#define UNUSED_PARAMETER UNUSED_IF_NO_SFCGAL    // SFCGAL no and GEOS yes - GEOS methods always work
#else
#define UNUSED_PARAMETER                        // Both libraries are present
#endif
#endif

#ifndef HAVE_GEOS
#ifdef HAVE_SFCGAL
#define UNUSED_PARAMETER UNUSED_IF_NO_GEOS      // SFCGAL yes and GEOS no - SFCGAL methods always work
#else
#define UNUSED_PARAMETER CPL_UNUSED             // Neither of the libraries have support enabled
#endif
#endif

#endif
