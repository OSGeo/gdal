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

#ifdef HAVE_SQLITE_VFS

class OGR2SQLITEModule;

OGR2SQLITEModule* OGR2SQLITE_Setup(OGRDataSource* poDS,
                                   OGRSQLiteDataSource* poSQLiteDS);

int OGR2SQLITE_AddExtraDS(OGR2SQLITEModule* poModule, OGRDataSource* poDS);

void OGR2SQLITE_Register();

CPLString OGR2SQLITE_GetNameForGeometryColumn(OGRLayer* poLayer);

#endif // HAVE_SQLITE_VFS

#endif // _OGR_SQLITE_VIRTUAL_OGR_H_INCLUDED
