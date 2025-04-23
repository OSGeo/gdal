/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Various FileGDB OGR Datasource utility functions
 * Author:   Ragi Yaser Burhum, ragi@burhum.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Ragi Yaser Burhum
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef FGDB_UTILS_H_INCLUDED
#define FGDB_UTILS_H_INCLUDED

#include "ogr_fgdb.h"
#include <iostream>
#include <string>
#include "cpl_minixml.h"

std::wstring StringToWString(const std::string &s);
std::string WStringToString(const std::wstring &s);

//
// GDB API to OGR Geometry Mapping
//

// Type mapping
bool GDBToOGRGeometry(const std::string &geoType, bool hasZ, bool hasM,
                      OGRwkbGeometryType *pOut);
bool OGRGeometryToGDB(OGRwkbGeometryType ogrType, std::string *gdbType,
                      bool *hasZ, bool *hasM);

bool GDBToOGRSpatialReference(const std::string &wkt,
                              OGRSpatialReference **ppSR);

// Feature mapping
bool GDBGeometryToOGRGeometry(bool forceMulti,
                              FileGDBAPI::ShapeBuffer *pGdbGeometry,
                              OGRSpatialReference *pOGRSR,
                              OGRGeometry **ppOutGeometry);

// temporary version - until we can parse the full binary format
bool GhettoGDBGeometryToOGRGeometry(bool forceMulti,
                                    FileGDBAPI::ShapeBuffer *pGdbGeometry,
                                    OGRSpatialReference *pOGRSR,
                                    OGRGeometry **ppOutGeometry);
//
// GDB API to OGR Field Mapping
//
bool OGRToGDBFieldType(OGRFieldType ogrType, OGRFieldSubType eSubType,
                       std::string *gdbType);

//
// GDB Field Length
//
bool GDBFieldTypeToLengthInBytes(const std::string &gdbType, int &lengthOut);

//
// GDBAPI error to OGR
//
bool GDBErr(long hr, const std::string &desc, CPLErr errType = CE_Failure,
            const char *pszAddMsg = "");
bool GDBDebug(long hr, const std::string &desc);

//
// Utility for adding attributes to CPL nodes
//
void FGDB_CPLAddXMLAttribute(CPLXMLNode *node, const char *attrname,
                             const char *attrvalue);

//
// Utility for escaping reserved words and cleaning field names
//
std::wstring FGDBLaunderName(const std::wstring &name);
std::wstring FGDBEscapeUnsupportedPrefixes(const std::wstring &className);
std::wstring FGDBEscapeReservedKeywords(const std::wstring &name);

#endif
