/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  SQLite Virtual Table module using OGR layers
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines dash paris dot org>
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

#ifndef _OGR_SQLITE_VIRTUAL_OGR_H_INCLUDED
#define _OGR_SQLITE_VIRTUAL_OGR_H_INCLUDED

#include "ogr_sqlite.h"
#include <map>

#ifdef HAVE_SQLITE_VFS

/************************************************************************/
/*                           OGR2SQLITEModule                           */
/************************************************************************/

class OGR2SQLITEModule
{
    OGRDataSource* poDS;

    std::map< std::pair<int,int>, OGRCoordinateTransformation*> oCachedTransformsMap;

  public:
                                OGR2SQLITEModule(OGRDataSource* poDS);
                                ~OGR2SQLITEModule();

    int                          SetToDB(sqlite3* hDB);
    OGRDataSource*               GetDS() { return poDS; }
    OGRCoordinateTransformation* GetTransform(int nSrcSRSId, int nDstSRSId);
};

void OGR2SQLITE_Register();

CPLString OGR2SQLITE_GetNameForGeometryColumn(OGRLayer* poLayer);

#endif // HAVE_SQLITE_VFS

#endif // _OGR_SQLITE_VIRTUAL_OGR_H_INCLUDED
