/******************************************************************************
 * $Id: iso19115_srs.cpp 35951 2016-10-26 16:53:54Z goatbar $
 *
 * Project:  BAG Driver
 * Purpose:  Implements code to parse ISO 19115 metadata to extract a
 *           spatial reference system.  Eventually intended to be made
 *           a method on OGRSpatialReference.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2012, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef ISO19155_SRS_H_INCLUDED_
#define ISO19155_SRS_H_INCLUDED_

#include "cpl_port.h"
#include "ogr_core.h"
#include "ogr_spatialref.h"

OGRErr OGR_SRS_ImportFromISO19115( OGRSpatialReference *poThis,
                                   const char *pszISOXML );

#endif  // ISO19155_SRS_H_INCLUDED_
