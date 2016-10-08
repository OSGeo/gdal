/******************************************************************************
 * Project:  OGR
 * Purpose:  OGRGMLASDriver implementation
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 * Initial development funded by the European Earth observation programme
 * Copernicus
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault, <even dot rouault at spatialys dot com>
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

#include "ogr_gmlas.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                              GMLASField()                            */
/************************************************************************/

GMLASField::GMLASField()
    : m_eType(GMLAS_FT_STRING)
    , m_eGeomType(wkbNone)
    , m_nWidth(0)
    , m_bNotNullable(false)
    , m_bArray(false)
    , m_bList(false)
    , m_eCategory(REGULAR)
    , m_nMinOccurs(-1)
    , m_nMaxOccurs(-1)
    , m_bIncludeThisEltInBlob(false)
    , m_bIgnored(false)
{
}

/************************************************************************/
/*                             GetTypeFromString()                      */
/************************************************************************/

GMLASFieldType GMLASField::GetTypeFromString( const CPLString& osType )
{
    if( osType == "string" ||
        osType == "token" ||
        osType == "NMTOKEN" ||
        osType == "NCName" )
    {
        // token has special processing by XML processor: all leading/trailing
        // white space is removed
        return GMLAS_FT_STRING;
    }
    else if( osType == "ID" )
        return GMLAS_FT_ID;
    else if( osType == "boolean" )
        return GMLAS_FT_BOOLEAN;
    else if( osType == "short" )
        return GMLAS_FT_SHORT;
    else if( osType == "int" )
        return GMLAS_FT_INT32;
    else if( osType == "byte" ||
             osType == "integer" ||
             osType == "negativeInteger" ||
             osType == "nonNegativeInteger" ||
             osType == "nonPositiveInteger" ||
             osType == "positiveInteger" ||
             osType == "unsignedByte" ||
             osType == "unsignedShort" ||
             osType == "unsignedInt") // FIXME ?
        return GMLAS_FT_INT32;
    else if( osType == "long" ||
             osType == "unsignedLong" )
        return GMLAS_FT_INT64;
    else if( osType == "float" )
        return GMLAS_FT_FLOAT;
    else if( osType == "double" )
        return GMLAS_FT_DOUBLE;
    else if( osType == "decimal" )
        return GMLAS_FT_DECIMAL;
    else if( osType == "date" )
        return GMLAS_FT_DATE;
    else if( osType == "time" )
        return GMLAS_FT_TIME;
    else if( osType == "dateTime" )
        return GMLAS_FT_DATETIME;
    else if( osType == "anyURI" )
        return GMLAS_FT_ANYURI;
    else if( osType == "anyType" )
        return GMLAS_FT_ANYTYPE;
    else if( osType == "anySimpleType" )
        return GMLAS_FT_ANYSIMPLETYPE;
    else if( osType == "duration" )
        return GMLAS_FT_STRING;
    else if( osType == "base64Binary" )
        return GMLAS_FT_BASE64BINARY;
    else if( osType == "hexBinary" )
        return GMLAS_FT_HEXBINARY;
    else
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                    "Unhandled type: %s", osType.c_str());
        return GMLAS_FT_STRING;
    }
}

/************************************************************************/
/*                               SetType()                              */
/************************************************************************/

void GMLASField::SetType(GMLASFieldType eType, const char* pszTypeName)
{
    m_eType = eType;
    m_osTypeName = pszTypeName;
}

/************************************************************************/
/*                          GMLASFeatureClass()                         */
/************************************************************************/

GMLASFeatureClass::GMLASFeatureClass()
    : m_bIsRepeatedSequence(false)
    , m_bIsGroup(false)
    , m_bIsTopLevelElt(false)
{
}

/************************************************************************/
/*                                 SetName()                            */
/************************************************************************/

void GMLASFeatureClass::SetName(const CPLString& osName)
{
    m_osName = osName;
}

/************************************************************************/
/*                                SetXPath()                            */
/************************************************************************/

void GMLASFeatureClass::SetXPath(const CPLString& osXPath)
{
    m_osXPath = osXPath;
}

/************************************************************************/
/*                                AddField()                            */
/************************************************************************/

void GMLASFeatureClass::AddField( const GMLASField& oField )
{
    m_aoFields.push_back(oField);
}

/************************************************************************/
/*                            PrependFields()                           */
/************************************************************************/

void GMLASFeatureClass::PrependFields( const std::vector<GMLASField>& aoFields )
{
    m_aoFields.insert( m_aoFields.begin(), aoFields.begin(), aoFields.end() );
}

/************************************************************************/
/*                             AppendFields()                           */
/************************************************************************/

void GMLASFeatureClass::AppendFields( const std::vector<GMLASField>& aoFields )
{
    m_aoFields.insert( m_aoFields.end(), aoFields.begin(), aoFields.end() );
}

/************************************************************************/
/*                             AddNestedClass()                         */
/************************************************************************/

void GMLASFeatureClass::AddNestedClass( const GMLASFeatureClass& oNestedClass )
{
    m_aoNestedClasses.push_back(oNestedClass);
}
