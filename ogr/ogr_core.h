/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Define some core portability services for cross-platform OGR code.
 * Author:   Frank Warmerdam, warmerda@home.com
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
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.4  1999/07/05 18:56:52  warmerda
 * now includes cpl_port.h
 *
 * Revision 1.3  1999/07/05 17:19:03  warmerda
 * added OGRERR_UNSUPPORTED_SRS
 *
 * Revision 1.2  1999/05/31 15:00:37  warmerda
 * added generic OGRERR_FAILURE error code.
 *
 * Revision 1.1  1999/05/20 14:35:00  warmerda
 * New
 *
 */

#ifndef _OGR_CORE_H_INCLUDED
#define _OGR_CORE_H_INLLUDED

#include "cpl_port.h"

void  *OGRMalloc( size_t );
void  *OGRCalloc( size_t, size_t );
void  *OGRRealloc( void *, size_t );
char  *OGRStrdup( const char * );
void   OGRFree( void * );

typedef int OGRErr;

#define OGRERR_NONE		   0
#define OGRERR_NOT_ENOUGH_DATA	   1	/* not enough data to deserialize */
#define OGRERR_NOT_ENOUGH_MEMORY   2
#define OGRERR_UNSUPPORTED_GEOMETRY_TYPE 3
#define OGRERR_UNSUPPORTED_OPERATION 4
#define OGRERR_CORRUPT_DATA	   5
#define OGRERR_FAILURE		   6
#define OGRERR_UNSUPPORTED_SRS	   7

typedef int	OGRBoolean;


#endif /* ndef _OGR_CORE_H_INCLUDED */

