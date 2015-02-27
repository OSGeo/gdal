/******************************************************************************
* $Id$
*
* Project:  OpenGIS Simple Features Reference Implementation
* Purpose:  Different utility functions used in FileGDB OGR driver.
* Author:   Ragi Yaser Burhum, ragi@burhum.com
*           Paul Ramsey, pramsey at cleverelephant.ca
*
******************************************************************************
* Copyright (c) 2010, Ragi Yaser Burhum
* Copyright (c) 2011, Paul Ramsey <pramsey at cleverelephant.ca>
 * Copyright (c) 2011-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "FGdbUtils.h"
#include <algorithm>

#include "ogr_api.h"
#include "ogrpgeogeometry.h"

CPL_CVSID("$Id$");

using std::string;

/*************************************************************************/
/*                          StringToWString()                            */
/*************************************************************************/

std::wstring StringToWString(const std::string& utf8string)
{
    wchar_t* pszUTF16 = CPLRecodeToWChar( utf8string.c_str(), CPL_ENC_UTF8, CPL_ENC_UCS2);
    std::wstring utf16string = pszUTF16;
    CPLFree(pszUTF16);
    return utf16string;
}

/*************************************************************************/
/*                          WStringToString()                            */
/*************************************************************************/

std::string WStringToString(const std::wstring& utf16string)
{
    char* pszUTF8 = CPLRecodeFromWChar( utf16string.c_str(), CPL_ENC_UCS2, CPL_ENC_UTF8 );
    std::string utf8string = pszUTF8;
    CPLFree(pszUTF8);
    return utf8string; 
}

/*************************************************************************/
/*                                GDBErr()                               */
/*************************************************************************/

bool GDBErr(long int hr, std::string desc, CPLErr errType, const char* pszAddMsg)
{
    std::wstring fgdb_error_desc_w;
    fgdbError er;
    er = FileGDBAPI::ErrorInfo::GetErrorDescription(hr, fgdb_error_desc_w);
    if ( er == S_OK )
    {
        std::string fgdb_error_desc = WStringToString(fgdb_error_desc_w);
        CPLError( errType, CPLE_AppDefined,
                  "%s (%s)%s", desc.c_str(), fgdb_error_desc.c_str(), pszAddMsg);
    }
    else
    {
        CPLError( errType, CPLE_AppDefined,
                  "Error (%ld): %s%s", hr, desc.c_str(), pszAddMsg);
    }
    // FIXME? EvenR: not sure if ClearErrors() is really necessary, but as it, it causes crashes in case of
    // repeated errors
    //FileGDBAPI::ErrorInfo::ClearErrors();

    return false;
}

/*************************************************************************/
/*                            GDBDebug()                                 */
/*************************************************************************/

bool GDBDebug(long int hr, std::string desc)
{
    std::wstring fgdb_error_desc_w;
    fgdbError er;
    er = FileGDBAPI::ErrorInfo::GetErrorDescription(hr, fgdb_error_desc_w);
    if ( er == S_OK )
    {
        std::string fgdb_error_desc = WStringToString(fgdb_error_desc_w);
        CPLDebug("FGDB", "%s (%s)", desc.c_str(), fgdb_error_desc.c_str());
    }
    else
    {
        CPLDebug("FGDB", "%s", desc.c_str());
    }
    // FIXME? EvenR: not sure if ClearErrors() is really necessary, but as it, it causes crashes in case of
    // repeated errors
    //FileGDBAPI::ErrorInfo::ClearErrors();

    return false;
}

/*************************************************************************/
/*                            GDBToOGRGeometry()                         */
/*************************************************************************/

bool GDBToOGRGeometry(string geoType, bool hasZ, OGRwkbGeometryType* pOut)
{
    if (geoType == "esriGeometryPoint")
    {
        *pOut = hasZ? wkbPoint25D : wkbPoint;
    }
    else if (geoType == "esriGeometryMultipoint")
    {
        *pOut = hasZ? wkbMultiPoint25D : wkbMultiPoint;
    }
    else if (geoType == "esriGeometryLine")
    {
        *pOut = hasZ? wkbLineString25D : wkbLineString;
    }
    else if (geoType == "esriGeometryPolyline")
    {
        *pOut = hasZ? wkbMultiLineString25D : wkbMultiLineString;
    }
    else if (geoType == "esriGeometryPolygon" ||
            geoType == "esriGeometryMultiPatch")
    {
        *pOut = hasZ? wkbMultiPolygon25D : wkbMultiPolygon; // no mapping to single polygon
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "Cannot map esriGeometryType(%s) to OGRwkbGeometryType", geoType.c_str());
        return false;
    }

    return true;
}

/*************************************************************************/
/*                            OGRGeometryToGDB()                         */
/*************************************************************************/

bool OGRGeometryToGDB(OGRwkbGeometryType ogrType, std::string *gdbType, bool *hasZ)
{
    switch (ogrType)
    {
        /* 3D forms */
        case wkbPoint25D:
        {
            *gdbType = "esriGeometryPoint";
            *hasZ = true;
            break;
        }

        case wkbMultiPoint25D:
        {
            *gdbType = "esriGeometryMultipoint";
            *hasZ = true;
            break;
        }

        case wkbLineString25D:
        case wkbMultiLineString25D:
        {
            *gdbType = "esriGeometryPolyline";
            *hasZ = true;
            break;
        }

        case wkbPolygon25D:
        case wkbMultiPolygon25D:
        {
            *gdbType = "esriGeometryPolygon";
            *hasZ = true;
            break;
        }

        /* 2D forms */
        case wkbPoint:
        {
            *gdbType = "esriGeometryPoint";
            *hasZ = false;
            break;
        }

        case wkbMultiPoint:
        {
            *gdbType = "esriGeometryMultipoint";
            *hasZ = false;
            break;
        }

        case wkbLineString:
        case wkbMultiLineString:
        {
            *gdbType = "esriGeometryPolyline";
            *hasZ = false;
            break;
        }

        case wkbPolygon:
        case wkbMultiPolygon:
        {
            *gdbType = "esriGeometryPolygon";
            *hasZ = false;
            break;
        }
        
        default:
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Cannot map OGRwkbGeometryType (%s) to ESRI type",
                      OGRGeometryTypeToName(ogrType));
            return false;
        }
    }
    return true;
}

/*************************************************************************/
/*                            GDBToOGRFieldType()                        */
/*************************************************************************/

// We could make this function far more robust by doing automatic coertion of types,
// and/or skipping fields we do not know. But our purposes this works fine
bool GDBToOGRFieldType(std::string gdbType, OGRFieldType* pOut, OGRFieldSubType* pSubType)
{
    /*
    ESRI types
    esriFieldTypeSmallInteger = 0,
    esriFieldTypeInteger = 1,
    esriFieldTypeSingle = 2,
    esriFieldTypeDouble = 3,
    esriFieldTypeString = 4,
    esriFieldTypeDate = 5,
    esriFieldTypeOID = 6,
    esriFieldTypeGeometry = 7,
    esriFieldTypeBlob = 8,
    esriFieldTypeRaster = 9,
    esriFieldTypeGUID = 10,
    esriFieldTypeGlobalID = 11,
    esriFieldTypeXML = 12
    */

    //OGR Types

    //            Desc                                 Name                GDB->OGR Mapped By Us?
    /** Simple 32bit integer *///                   OFTInteger = 0,             YES
    /** List of 32bit integers *///                 OFTIntegerList = 1,         NO
    /** Double Precision floating point *///        OFTReal = 2,                YES
    /** List of doubles *///                        OFTRealList = 3,            NO
    /** String of ASCII chars *///                  OFTString = 4,              YES
    /** Array of strings *///                       OFTStringList = 5,          NO
    /** deprecated *///                             OFTWideString = 6,          NO
    /** deprecated *///                             OFTWideStringList = 7,      NO
    /** Raw Binary data *///                        OFTBinary = 8,              YES
    /** Date *///                                   OFTDate = 9,                NO
    /** Time *///                                   OFTTime = 10,               NO
    /** Date and Time *///                          OFTDateTime = 11            YES

    *pSubType = OFSTNone;
    if (gdbType == "esriFieldTypeSmallInteger" )
    {
        *pSubType = OFSTInt16;
        *pOut = OFTInteger;
        return true;
    }
    else if (gdbType == "esriFieldTypeInteger")
    {
        *pOut = OFTInteger;
        return true;
    }
    else if (gdbType == "esriFieldTypeSingle" )
    {
        *pSubType = OFSTFloat32;
        *pOut = OFTReal;
        return true;
    }
    else if (gdbType == "esriFieldTypeDouble")
    {
        *pOut = OFTReal;
        return true;
    }
    else if (gdbType == "esriFieldTypeGUID" ||
        gdbType == "esriFieldTypeGlobalID" ||
        gdbType == "esriFieldTypeXML" ||
        gdbType == "esriFieldTypeString")
    {
        *pOut = OFTString;
        return true;
    }
    else if (gdbType == "esriFieldTypeDate")
    {
        *pOut = OFTDateTime;
        return true;
    }
    else if (gdbType == "esriFieldTypeBlob")
    {
        *pOut = OFTBinary;
        return true;
    }
    else
    {
        /* Intentionally fail at these
        esriFieldTypeOID
        esriFieldTypeGeometry
        esriFieldTypeRaster
        */
        CPLError( CE_Warning, CPLE_AppDefined, "%s", ("Cannot map field " + gdbType).c_str());

        return false;
    }
}

/*************************************************************************/
/*                            OGRToGDBFieldType()                        */
/*************************************************************************/

bool OGRToGDBFieldType(OGRFieldType ogrType, OGRFieldSubType eSubType, std::string* gdbType)
{
    switch(ogrType)
    {
        case OFTInteger:
        {
            if( eSubType == OFSTInt16 )
                *gdbType = "esriFieldTypeSmallInteger";
            else
                *gdbType = "esriFieldTypeInteger";
            break;
        }
        case OFTReal:
        case OFTInteger64:
        {
             if( eSubType == OFSTFloat32 )
                *gdbType = "esriFieldTypeSingle";
            else
                *gdbType = "esriFieldTypeDouble";
            break;
        }
        case OFTString:
        {
            *gdbType = "esriFieldTypeString";
            break;
        }
        case OFTBinary:
        {
            *gdbType = "esriFieldTypeBlob";
            break;
        }
        case OFTDate:
        case OFTDateTime:
        {
            *gdbType = "esriFieldTypeDate";
            break;
        }
        default:
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Cannot map OGR field type (%s)",
                      OGR_GetFieldTypeName(ogrType) );
            return false;
        }
    }

    return true;
}

/*************************************************************************/
/*                       GDBFieldTypeToWidthPrecision()                  */
/*************************************************************************/

bool GDBFieldTypeToWidthPrecision(std::string &gdbType, int *width, int *precision)
{
    *precision = 0;

    /* Width (Length in FileGDB terms) based on FileGDB_API/samples/XMLsamples/OneOfEachFieldType.xml */
    /* Length is in bytes per doc of FileGDB_API/xmlResources/FileGDBAPI.xsd */
    if(gdbType == "esriFieldTypeSmallInteger" )
    {
        *width = 2;
    }
    else if(gdbType == "esriFieldTypeInteger" )
    {
        *width = 4;
    }
    else if(gdbType == "esriFieldTypeSingle" )
    {
        *width = 4;
        *precision = 5; // FIXME ?
    }
    else if(gdbType == "esriFieldTypeDouble" )
    {
        *width = 8;
        *precision = 15; // FIXME ?
    }
    else if(gdbType == "esriFieldTypeString" ||
            gdbType == "esriFieldTypeXML")
    {
        *width = atoi(CPLGetConfigOption("FGDB_STRING_WIDTH", "65536"));
    }
    else if(gdbType == "esriFieldTypeDate" )
    {
        *width = 8;
    }
    else if(gdbType == "esriFieldTypeOID" )
    {
        *width = 4;
    }
    else if(gdbType == "esriFieldTypeGUID" )
    {
        *width = 16;
    }
    else if(gdbType == "esriFieldTypeBlob" )
    {
        *width = 0;
    }
    else if(gdbType == "esriFieldTypeGlobalID" )
    {
        *width = 38;
    }
    else
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Cannot map ESRI field type (%s)", gdbType.c_str());
        return false;
    }

    return true;
}

/*************************************************************************/
/*                       GDBFieldTypeToWidthPrecision()                  */
/*************************************************************************/

bool GDBGeometryToOGRGeometry(bool forceMulti, FileGDBAPI::ShapeBuffer* pGdbGeometry,
                              OGRSpatialReference* pOGRSR, OGRGeometry** ppOutGeometry)
{

    OGRGeometry* pOGRGeometry = NULL;

    OGRErr eErr = OGRCreateFromShapeBin( pGdbGeometry->shapeBuffer,
                                &pOGRGeometry,
                                pGdbGeometry->inUseLength);

    //OGRErr eErr = OGRGeometryFactory::createFromWkb(pGdbGeometry->shapeBuffer, pOGRSR, &pOGRGeometry, pGdbGeometry->inUseLength );

    if (eErr != OGRERR_NONE)
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Failed attempting to import GDB WKB Geometry. OGRGeometryFactory err:%d", eErr);
        return false;
    }

    if( pOGRGeometry != NULL )
    {
        // force geometries to multi if requested

        // If it is a polygon, force to MultiPolygon since we always produce multipolygons
        if (wkbFlatten(pOGRGeometry->getGeometryType()) == wkbPolygon)
        {
            pOGRGeometry = OGRGeometryFactory::forceToMultiPolygon(pOGRGeometry);
        }
        else if (forceMulti)
        {
            if (wkbFlatten(pOGRGeometry->getGeometryType()) == wkbLineString)
            {
                pOGRGeometry = OGRGeometryFactory::forceToMultiLineString(pOGRGeometry);
            }
            else if (wkbFlatten(pOGRGeometry->getGeometryType()) == wkbPoint)
            {
                pOGRGeometry = OGRGeometryFactory::forceToMultiPoint(pOGRGeometry);
            }
        }

        if (pOGRGeometry)
            pOGRGeometry->assignSpatialReference( pOGRSR );
    }


    *ppOutGeometry = pOGRGeometry;

    return true;
}

/*************************************************************************/
/*                         GDBToOGRSpatialReference()                    */
/*************************************************************************/

bool GDBToOGRSpatialReference(const string & wkt, OGRSpatialReference** ppSR)
{
    if (wkt.size() <= 0)
    {
        CPLError( CE_Warning, CPLE_AppDefined, "ESRI Spatial Reference is NULL");
        return false;
    }

    *ppSR = new OGRSpatialReference(wkt.c_str());

    OGRErr result = (*ppSR)->morphFromESRI();

    if (result == OGRERR_NONE)
    {
        return true;
    }
    else
    {
        delete *ppSR;
        *ppSR = NULL;

        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed morhping from ESRI Geometry: %s", wkt.c_str());

        return false;
    }
}

/*************************************************************************/
/*                           FGDB_CPLAddXMLAttribute()                   */
/*************************************************************************/

/* Utility method for attributing nodes */
void FGDB_CPLAddXMLAttribute(CPLXMLNode* node, const char* attrname, const char* attrvalue)
{
    if ( !node ) return;
    CPLCreateXMLNode( CPLCreateXMLNode( node, CXT_Attribute, attrname ), CXT_Text, attrvalue );
}

/*************************************************************************/
/*                          FGDBLaunderName()                            */
/*************************************************************************/

std::string FGDBLaunderName(const std::string name)
{
    std::string newName = name;

    if ( newName[0]>='0' && newName[0]<='9' )
    {
        newName = "_" + newName;
    }

    for(size_t i=0; i < newName.size(); i++)
    {
        if ( !( newName[i] == '_' ||
              ( newName[i]>='0' && newName[i]<='9') ||
              ( newName[i]>='a' && newName[i]<='z') || 
              ( newName[i]>='A' && newName[i]<='Z') ))
        {
            newName[i] = '_';
        }
    }

    return newName;
}

/*************************************************************************/
/*                     FGDBEscapeUnsupportedPrefixes()                   */
/*************************************************************************/

std::string FGDBEscapeUnsupportedPrefixes(const std::string className)
{
    std::string newName = className;
    // From ESRI docs
    // Feature classes starting with these strings are unsupported.
    static const char* UNSUPPORTED_PREFIXES[] = {"sde_", "gdb_", "delta_", NULL};

    for (int i = 0; UNSUPPORTED_PREFIXES[i] != NULL; i++)
    {
        if (newName.find(UNSUPPORTED_PREFIXES[i]) == 0)
        {
            newName = "_" + newName;
            break;
        }
    }

    return newName;
}

/*************************************************************************/
/*                        FGDBEscapeReservedKeywords()                   */
/*************************************************************************/

std::string FGDBEscapeReservedKeywords(const std::string name)
{
    std::string newName = name;
    std::string upperName = name;
    std::transform(upperName.begin(), upperName.end(), upperName.begin(), ::toupper);

    // From ESRI docs
    static const char* RESERVED_WORDS[] = {FGDB_OID_NAME, "ADD", "ALTER", "AND", "AS", "ASC", "BETWEEN",
                                    "BY", "COLUMN", "CREATE", "DATE", "DELETE", "DESC",
                                    "DROP", "EXISTS", "FOR", "FROM", "IN", "INSERT", "INTO",
                                    "IS", "LIKE", "NOT", "NULL", "OR", "ORDER", "SELECT",
                                    "SET", "TABLE", "UPDATE", "VALUES", "WHERE", NULL};

    // Append an underscore to any FGDB reserved words used as field names
    // This is the same behaviour ArcCatalog follows.
    for (int i = 0; RESERVED_WORDS[i] != NULL; i++)
    {
        const char* w = RESERVED_WORDS[i];
        if (upperName == w)
        {
            newName += '_';
            break;
        }
    }

    return newName;
}
