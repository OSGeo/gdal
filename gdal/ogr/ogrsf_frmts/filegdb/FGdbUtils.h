/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Various FileGDB OGR Datasource utility functions
 * Author:   Ragi Yaser Burhum, ragi@burhum.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Ragi Yaser Burhum
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


#ifndef _FGDB_UTILS_H_INCLUDED
#define _FGDB_UTILS_H_INCLUDED

#include "ogr_fgdb.h"
#include <iostream>
#include <string>
#include "cpl_minixml.h"

std::wstring StringToWString(const std::string& s);
std::string WStringToString(const std::wstring& s);

//
// GDB API to OGR Geometry Mapping
//

// Type mapping
bool GDBToOGRGeometry(std::string geoType, bool hasZ, OGRwkbGeometryType* pOut);
bool OGRGeometryToGDB(OGRwkbGeometryType ogrType, std::string *gdbType, bool *hasZ);


bool GDBToOGRSpatialReference(const std::string & wkt, OGRSpatialReference** ppSR);

// Feature mapping 
bool GDBGeometryToOGRGeometry(bool forceMulti, FileGDBAPI::ShapeBuffer* pGdbGeometry, 
                              OGRSpatialReference* pOGRSR, OGRGeometry** ppOutGeometry);

//temporary version - until we can parse the full binary format
bool GhettoGDBGeometryToOGRGeometry(bool forceMulti, FileGDBAPI::ShapeBuffer* pGdbGeometry, 
                                    OGRSpatialReference* pOGRSR, OGRGeometry** ppOutGeometry);
//
// GDB API to OGR Field Mapping
//
bool GDBToOGRFieldType(std::string gdbType, OGRFieldType* ogrType, OGRFieldSubType* pSubType);
bool OGRToGDBFieldType(OGRFieldType ogrType, OGRFieldSubType eSubType, std::string* gdbType);

//
// GDB Field Width defaults
//
bool GDBFieldTypeToWidthPrecision(std::string &gdbType, int *width, int *precision);

//
// GDBAPI error to OGR
//
bool GDBErr(long hr, std::string desc);
bool GDBDebug(long hr, std::string desc);

//
// Utility for adding attributes to CPL nodes
//
void FGDB_CPLAddXMLAttribute(CPLXMLNode* node, const char* attrname, const char* attrvalue);

//
// Utility for escaping reserved words and cleaning field names
//
std::string FGDBLaunderName(const std::string name);
std::string FGDBEscapeUnsupportedPrefixes(const std::string className);
std::string FGDBEscapeReservedKeywords(const std::string name);

#endif
