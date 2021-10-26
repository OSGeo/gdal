/**********************************************************************
 *
 * Project:  GML Reader
 * Purpose:  Implementation of GMLFeatureClass.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_port.h"
#include "gmlreader.h"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "ogr_core.h"
#include "ogr_p.h"
#include "ogr_geometry.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                          GMLFeatureClass()                           */
/************************************************************************/

GMLFeatureClass::GMLFeatureClass( const char *pszName ) :
    m_pszName(CPLStrdup(pszName)),
    m_pszElementName(nullptr),
    n_nNameLen(static_cast<int>(strlen(pszName))),
    n_nElementNameLen(0),
    m_nPropertyCount(0),
    m_papoProperty(nullptr),
    m_nGeometryPropertyCount(0),
    m_papoGeometryProperty(nullptr),
    m_bSchemaLocked(false),
    m_nFeatureCount(-1),  // Unknown.
    m_pszExtraInfo(nullptr),
    m_bHaveExtents(false),
    m_dfXMin(0.0),
    m_dfXMax(0.0),
    m_dfYMin(0.0),
    m_dfYMax(0.0),
    m_pszSRSName(nullptr),
    m_bSRSNameConsistent(true)
{}

/************************************************************************/
/*                          ~GMLFeatureClass()                          */
/************************************************************************/

GMLFeatureClass::~GMLFeatureClass()

{
    CPLFree(m_pszName);
    CPLFree(m_pszElementName);

    for( int i = 0; i < m_nPropertyCount; i++ )
        delete m_papoProperty[i];
    CPLFree(m_papoProperty);

    ClearGeometryProperties();

    CPLFree(m_pszSRSName);
}

/************************************************************************/
/*                         StealProperties()                            */
/************************************************************************/

void GMLFeatureClass::StealProperties()
{
    m_nPropertyCount = 0;
    CPLFree(m_papoProperty);
    m_papoProperty = nullptr;
    m_oMapPropertyNameToIndex.clear();
    m_oMapPropertySrcElementToIndex.clear();
}

/************************************************************************/
/*                       StealGeometryProperties()                      */
/************************************************************************/

void GMLFeatureClass::StealGeometryProperties()
{
    m_nGeometryPropertyCount = 0;
    CPLFree(m_papoGeometryProperty);
    m_papoGeometryProperty = nullptr;
}

/************************************************************************/
/*                            SetName()                                 */
/************************************************************************/

void GMLFeatureClass::SetName(const char *pszNewName)
{
    CPLFree(m_pszName);
    m_pszName = CPLStrdup(pszNewName);
}

/************************************************************************/
/*                           GetProperty(int)                           */
/************************************************************************/

GMLPropertyDefn *GMLFeatureClass::GetProperty( int iIndex ) const

{
    if( iIndex < 0 || iIndex >= m_nPropertyCount )
        return nullptr;

    return m_papoProperty[iIndex];
}

/************************************************************************/
/*                          GetPropertyIndex()                          */
/************************************************************************/

int GMLFeatureClass::GetPropertyIndex( const char *pszName ) const

{
    auto oIter = m_oMapPropertyNameToIndex.find(CPLString(pszName).toupper());
    if( oIter != m_oMapPropertyNameToIndex.end() )
        return oIter->second;

    return -1;
}

/************************************************************************/
/*                        GetPropertyIndexBySrcElement()                */
/************************************************************************/

int GMLFeatureClass::GetPropertyIndexBySrcElement( const char *pszElement,
                                                   int nLen ) const

{
    auto oIter = m_oMapPropertySrcElementToIndex.find(CPLString(pszElement, nLen));
    if( oIter != m_oMapPropertySrcElementToIndex.end() )
        return oIter->second;

    return -1;
}

/************************************************************************/
/*                            AddProperty()                             */
/************************************************************************/

int GMLFeatureClass::AddProperty( GMLPropertyDefn *poDefn )

{
    if( GetProperty(poDefn->GetName()) != nullptr )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Field with same name (%s) already exists in (%s). "
                 "Skipping newer ones",
                 poDefn->GetName(), m_pszName);
        return -1;
    }

    m_nPropertyCount++;
    m_papoProperty = static_cast<GMLPropertyDefn **>(
        CPLRealloc(m_papoProperty, sizeof(void *) * m_nPropertyCount));

    m_papoProperty[m_nPropertyCount - 1] = poDefn;
    m_oMapPropertyNameToIndex[ CPLString(poDefn->GetName()).toupper() ] =
        m_nPropertyCount - 1;
    if( m_oMapPropertySrcElementToIndex.find(poDefn->GetSrcElement()) ==
            m_oMapPropertySrcElementToIndex.end() )
    {
        m_oMapPropertySrcElementToIndex[ poDefn->GetSrcElement() ] =
            m_nPropertyCount - 1;
    }

    return m_nPropertyCount - 1;
}

/************************************************************************/
/*                         GetGeometryProperty(int)                      */
/************************************************************************/

GMLGeometryPropertyDefn *
GMLFeatureClass::GetGeometryProperty( int iIndex ) const
{
    if( iIndex < 0 || iIndex >= m_nGeometryPropertyCount )
        return nullptr;

    return m_papoGeometryProperty[iIndex];
}

/************************************************************************/
/*                   GetGeometryPropertyIndexBySrcElement()             */
/************************************************************************/

int GMLFeatureClass::GetGeometryPropertyIndexBySrcElement(
    const char *pszElement) const

{
    for( int i = 0; i < m_nGeometryPropertyCount; i++ )
        if( strcmp(pszElement, m_papoGeometryProperty[i]->GetSrcElement()) == 0 )
            return i;

    return -1;
}

/************************************************************************/
/*                         AddGeometryProperty()                        */
/************************************************************************/

int GMLFeatureClass::AddGeometryProperty( GMLGeometryPropertyDefn *poDefn )

{
    if( GetGeometryPropertyIndexBySrcElement(poDefn->GetSrcElement()) >= 0 )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Geometry field with same name (%s) already exists in (%s). "
                 "Skipping newer ones",
                 poDefn->GetSrcElement(), m_pszName);
        return -1;
    }

    m_nGeometryPropertyCount++;
    m_papoGeometryProperty = static_cast<GMLGeometryPropertyDefn **>(CPLRealloc(
        m_papoGeometryProperty, sizeof(void *) * m_nGeometryPropertyCount));

    m_papoGeometryProperty[m_nGeometryPropertyCount - 1] = poDefn;

    return m_nGeometryPropertyCount - 1;
}

/************************************************************************/
/*                       ClearGeometryProperties()                      */
/************************************************************************/

void GMLFeatureClass::ClearGeometryProperties()
{
    for( int i = 0; i < m_nGeometryPropertyCount; i++ )
        delete m_papoGeometryProperty[i];
    CPLFree(m_papoGeometryProperty);
    m_nGeometryPropertyCount = 0;
    m_papoGeometryProperty = nullptr;
}

/************************************************************************/
/*                         HasFeatureProperties()                       */
/************************************************************************/

bool GMLFeatureClass::HasFeatureProperties()
{
    for( int i = 0; i < m_nPropertyCount; i++ )
    {
        if( m_papoProperty[i]->GetType() == GMLPT_FeatureProperty ||
            m_papoProperty[i]->GetType() == GMLPT_FeaturePropertyList )
            return true;
    }
    return false;
}

/************************************************************************/
/*                           SetElementName()                           */
/************************************************************************/

void GMLFeatureClass::SetElementName( const char *pszElementName )

{
    CPLFree(m_pszElementName);
    m_pszElementName = CPLStrdup(pszElementName);
    n_nElementNameLen = static_cast<int>(strlen(pszElementName));
}

/************************************************************************/
/*                           GetElementName()                           */
/************************************************************************/

const char *GMLFeatureClass::GetElementName() const

{
    if( m_pszElementName == nullptr )
        return m_pszName;

    return m_pszElementName;
}

/************************************************************************/
/*                           GetElementName()                           */
/************************************************************************/

size_t GMLFeatureClass::GetElementNameLen() const

{
    if( m_pszElementName == nullptr )
        return n_nNameLen;

    return n_nElementNameLen;
}

/************************************************************************/
/*                         GetFeatureCount()                          */
/************************************************************************/

GIntBig GMLFeatureClass::GetFeatureCount() { return m_nFeatureCount; }

/************************************************************************/
/*                          SetFeatureCount()                           */
/************************************************************************/

void GMLFeatureClass::SetFeatureCount( GIntBig nNewCount )

{
    m_nFeatureCount = nNewCount;
}

/************************************************************************/
/*                            GetExtraInfo()                            */
/************************************************************************/

const char *GMLFeatureClass::GetExtraInfo() { return m_pszExtraInfo; }

/************************************************************************/
/*                            SetExtraInfo()                            */
/************************************************************************/

void GMLFeatureClass::SetExtraInfo( const char *pszExtraInfo )

{
    CPLFree(m_pszExtraInfo);
    m_pszExtraInfo = nullptr;

    if( pszExtraInfo != nullptr )
        m_pszExtraInfo = CPLStrdup(pszExtraInfo);
}

/************************************************************************/
/*                             SetExtents()                             */
/************************************************************************/

void GMLFeatureClass::SetExtents( double dfXMin, double dfXMax,
                                  double dfYMin, double dfYMax )

{
    m_dfXMin = dfXMin;
    m_dfXMax = dfXMax;
    m_dfYMin = dfYMin;
    m_dfYMax = dfYMax;

    m_bHaveExtents = true;
}

/************************************************************************/
/*                             GetExtents()                             */
/************************************************************************/

bool GMLFeatureClass::GetExtents( double *pdfXMin, double *pdfXMax,
                                  double *pdfYMin, double *pdfYMax )

{
    if( m_bHaveExtents )
    {
        *pdfXMin = m_dfXMin;
        *pdfXMax = m_dfXMax;
        *pdfYMin = m_dfYMin;
        *pdfYMax = m_dfYMax;
    }

    return m_bHaveExtents;
}

/************************************************************************/
/*                            SetSRSName()                              */
/************************************************************************/

void GMLFeatureClass::SetSRSName( const char* pszSRSName )

{
    m_bSRSNameConsistent = true;
    CPLFree(m_pszSRSName);
    m_pszSRSName = pszSRSName ? CPLStrdup(pszSRSName) : nullptr;
}

/************************************************************************/
/*                           MergeSRSName()                             */
/************************************************************************/

void GMLFeatureClass::MergeSRSName( const char *pszSRSName )

{
    if(!m_bSRSNameConsistent)
        return;

    if( m_pszSRSName == nullptr )
    {
        if (pszSRSName)
            m_pszSRSName = CPLStrdup(pszSRSName);
    }
    else
    {
        m_bSRSNameConsistent =
            pszSRSName != nullptr && strcmp(m_pszSRSName, pszSRSName) == 0;
        if (!m_bSRSNameConsistent)
        {
            CPLFree(m_pszSRSName);
            m_pszSRSName = nullptr;
        }
    }
}

/************************************************************************/
/*                         InitializeFromXML()                          */
/************************************************************************/

bool GMLFeatureClass::InitializeFromXML( CPLXMLNode *psRoot )

{
    // Do some rudimentary checking that this is a well formed node.
    if( psRoot == nullptr || psRoot->eType != CXT_Element ||
        !EQUAL(psRoot->pszValue, "GMLFeatureClass") )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GMLFeatureClass::InitializeFromXML() called on %s node!",
                 psRoot ? psRoot->pszValue : "(null)");
        return false;
    }

    if( CPLGetXMLValue( psRoot, "Name", nullptr ) == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GMLFeatureClass has no <Name> element.");
        return false;
    }

    // Collect base info.
    CPLFree(m_pszName);
    m_pszName = CPLStrdup(CPLGetXMLValue(psRoot, "Name", nullptr));
    n_nNameLen = static_cast<int>(strlen(m_pszName));

    SetElementName(CPLGetXMLValue(psRoot, "ElementPath", m_pszName));

    // Collect geometry properties.
    bool bHasValidGeometryName = false;
    bool bHasValidGeometryElementPath = false;
    bool bHasFoundGeomType = false;
    bool bHasFoundGeomElements = false;
    const char *pszGName = "";
    const char *pszGPath = "";
    int nGeomType = wkbUnknown;

    const auto FlattenGeomTypeFromInt = [] (int eType)
    {
        eType = eType & (~wkb25DBitInternalUse);
        if( eType >= 1000 && eType < 2000 )  // ISO Z.
            return eType - 1000;
        if( eType >= 2000 && eType < 3000 )  // ISO M.
            return eType - 2000;
        if( eType >= 3000 && eType < 4000 )  // ISO ZM.
            return eType - 3000;
        return eType;
    };

    CPLXMLNode *psThis = nullptr;
    for( psThis = psRoot->psChild; psThis != nullptr; psThis = psThis->psNext )
    {
        if( psThis->eType == CXT_Element &&
            EQUAL(psThis->pszValue, "GeomPropertyDefn") )
        {
            const char *pszName = CPLGetXMLValue(psThis, "Name", "");
            const char *pszElementPath =
                CPLGetXMLValue(psThis, "ElementPath", "");
            const char *pszType = CPLGetXMLValue(psThis, "Type", nullptr);
            const bool bNullable =
                CPLTestBool(CPLGetXMLValue(psThis, "Nullable", "true"));
            nGeomType = wkbUnknown;
            if( pszType != nullptr && !EQUAL(pszType, "0") )
            {
                nGeomType = atoi(pszType);
                const int nFlattenGeomType = FlattenGeomTypeFromInt(nGeomType);
                if( nGeomType != 0 &&
                    !(nFlattenGeomType >= static_cast<int>(wkbPoint) &&
                      nFlattenGeomType <= static_cast<int>(wkbTIN)) )
                {
                    nGeomType = wkbUnknown;
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Unrecognized geometry type : %s",
                             pszType);
                }
                else if( nGeomType == 0 )
                {
                    nGeomType = OGRFromOGCGeomType(pszType);
                }
            }
            bHasFoundGeomElements = true;
            auto poDefn = new GMLGeometryPropertyDefn(
                pszName, pszElementPath, nGeomType, -1, bNullable);
            if( AddGeometryProperty(poDefn) < 0 )
                delete poDefn;
            bHasValidGeometryName = false;
            bHasValidGeometryElementPath = false;
            bHasFoundGeomType = false;
        }
        else if( psThis->eType == CXT_Element &&
                 strcmp(psThis->pszValue, "GeometryName") == 0 )
        {
            bHasFoundGeomElements = true;

            if( bHasValidGeometryName )
            {
                auto poDefn = new GMLGeometryPropertyDefn(
                    pszGName, pszGPath, nGeomType, -1, true);
                if( AddGeometryProperty(poDefn) < 0 )
                    delete poDefn;
                // bHasValidGeometryName = false;
                bHasValidGeometryElementPath = false;
                bHasFoundGeomType = false;
                pszGPath = "";
                nGeomType = wkbUnknown;
            }
            pszGName = CPLGetXMLValue(psThis, nullptr, "");
            bHasValidGeometryName = true;
        }
        else if( psThis->eType == CXT_Element &&
                 strcmp(psThis->pszValue, "GeometryElementPath") == 0 )
        {
            bHasFoundGeomElements = true;

            if( bHasValidGeometryElementPath )
            {
                auto poDefn = new GMLGeometryPropertyDefn(
                    pszGName, pszGPath, nGeomType, -1, true);
                if( AddGeometryProperty(poDefn) < 0 )
                    delete poDefn;
                bHasValidGeometryName = false;
                // bHasValidGeometryElementPath = false;
                bHasFoundGeomType = false;
                pszGName = "";
                nGeomType = wkbUnknown;
            }
            pszGPath = CPLGetXMLValue(psThis, nullptr, "");
            bHasValidGeometryElementPath = true;
        }
        else if( psThis->eType == CXT_Element &&
                 strcmp(psThis->pszValue, "GeometryType") == 0 )
        {
            bHasFoundGeomElements = true;

            if( bHasFoundGeomType )
            {
                auto poDefn = new GMLGeometryPropertyDefn(
                    pszGName, pszGPath, nGeomType, -1, true);
                if( AddGeometryProperty(poDefn) < 0 )
                    delete poDefn;
                bHasValidGeometryName = false;
                bHasValidGeometryElementPath = false;
                // bHasFoundGeomType = false;
                pszGName = "";
                pszGPath = "";
            }
            const char *pszGeometryType = CPLGetXMLValue(psThis, nullptr, nullptr);
            nGeomType = wkbUnknown;
            if( pszGeometryType != nullptr && !EQUAL(pszGeometryType, "0") )
            {
                nGeomType = atoi(pszGeometryType);
                const int nFlattenGeomType = FlattenGeomTypeFromInt(nGeomType);
                if( nGeomType == 100 || EQUAL(pszGeometryType, "NONE") )
                {
                    bHasValidGeometryElementPath = false;
                    bHasFoundGeomType = false;
                    break;
                }
                else if( nGeomType != 0 &&
                         !(nFlattenGeomType >= static_cast<int>(wkbPoint) &&
                           nFlattenGeomType <= static_cast<int>(wkbTIN)) )
                {
                    nGeomType = wkbUnknown;
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Unrecognized geometry type : %s",
                             pszGeometryType);
                }
                else if( nGeomType == 0 )
                {
                    nGeomType = OGRFromOGCGeomType(pszGeometryType);
                }
            }
            bHasFoundGeomType = true;
        }
    }

    // If there was a dangling <GeometryElementPath> or <GeometryType> or
    // that no explicit geometry information has been found, then add
    // a geometry field.
    if( bHasValidGeometryElementPath || bHasFoundGeomType ||
        !bHasFoundGeomElements )
    {
        auto poDefn = new GMLGeometryPropertyDefn(pszGName, pszGPath,
                                                  nGeomType, -1, true);
        if( AddGeometryProperty(poDefn) < 0 )
            delete poDefn;
    }

    SetSRSName(CPLGetXMLValue(psRoot, "SRSName", nullptr));

    // Collect dataset specific info.
    CPLXMLNode *psDSI = CPLGetXMLNode(psRoot, "DatasetSpecificInfo");
    if( psDSI != nullptr )
    {
        const char *pszValue = CPLGetXMLValue(psDSI, "FeatureCount", nullptr);
        if( pszValue != nullptr )
            SetFeatureCount(CPLAtoGIntBig(pszValue));

        // Eventually we should support XML subtrees.
        pszValue = CPLGetXMLValue(psDSI, "ExtraInfo", nullptr);
        if( pszValue != nullptr )
            SetExtraInfo(pszValue);

        if( CPLGetXMLValue(psDSI, "ExtentXMin", nullptr) != nullptr &&
            CPLGetXMLValue(psDSI, "ExtentXMax", nullptr) != nullptr &&
            CPLGetXMLValue(psDSI, "ExtentYMin", nullptr) != nullptr &&
            CPLGetXMLValue(psDSI, "ExtentYMax", nullptr) != nullptr )
        {
            SetExtents(CPLAtof(CPLGetXMLValue(psDSI, "ExtentXMin", "0.0")),
                       CPLAtof(CPLGetXMLValue(psDSI, "ExtentXMax", "0.0")),
                       CPLAtof(CPLGetXMLValue(psDSI, "ExtentYMin", "0.0")),
                       CPLAtof(CPLGetXMLValue(psDSI, "ExtentYMax", "0.0")));
        }
    }

    // Collect property definitions.
    for( psThis = psRoot->psChild; psThis != nullptr; psThis = psThis->psNext )
    {
        if( psThis->eType == CXT_Element &&
            EQUAL(psThis->pszValue, "PropertyDefn") )
        {
            const char *pszName = CPLGetXMLValue(psThis, "Name", nullptr);
            const char *pszType = CPLGetXMLValue(psThis, "Type", "Untyped");
            const char *pszSubType = CPLGetXMLValue(psThis, "Subtype", "");
            const char *pszCondition =
                CPLGetXMLValue(psThis, "Condition", nullptr);
            const bool bNullable =
                CPLTestBool(CPLGetXMLValue(psThis, "Nullable", "true"));
            const bool bUnique =
                CPLTestBool(CPLGetXMLValue(psThis, "Unique", "false"));

            if( pszName == nullptr )
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "GMLFeatureClass %s has a PropertyDefn without a <Name>.",
                    m_pszName);
                return false;
            }

            GMLPropertyDefn *poPDefn = new GMLPropertyDefn(
                pszName, CPLGetXMLValue(psThis, "ElementPath", nullptr));

            poPDefn->SetNullable(bNullable);
            poPDefn->SetUnique(bUnique);
            if( EQUAL(pszType, "Untyped") )
            {
                poPDefn->SetType(GMLPT_Untyped);
            }
            else if( EQUAL(pszType, "String") )
            {
                if( EQUAL(pszSubType, "Boolean") )
                {
                    poPDefn->SetType(GMLPT_Boolean);
                    poPDefn->SetWidth(1);
                }
                else if( EQUAL(pszSubType, "Date") )
                {
                    poPDefn->SetType(GMLPT_Date);
                }
                else if( EQUAL(pszSubType, "Time") )
                {
                    poPDefn->SetType(GMLPT_Time);
                }
                else if( EQUAL(pszSubType, "Datetime") )
                {
                    poPDefn->SetType(GMLPT_DateTime);
                }
                else
                {
                    poPDefn->SetType(GMLPT_String);
                    poPDefn->SetWidth(
                        atoi(CPLGetXMLValue(psThis, "Width", "0")));
                }
            }
            else if( EQUAL(pszType, "Integer") )
            {
                if( EQUAL(pszSubType, "Short") )
                {
                    poPDefn->SetType(GMLPT_Short);
                }
                else if( EQUAL(pszSubType, "Integer64") )
                {
                    poPDefn->SetType(GMLPT_Integer64);
                }
                else
                {
                    poPDefn->SetType(GMLPT_Integer);
                }
                poPDefn->SetWidth(atoi(CPLGetXMLValue(psThis, "Width", "0")));
            }
            else if( EQUAL(pszType, "Real") )
            {
                if( EQUAL(pszSubType, "Float") )
                {
                    poPDefn->SetType(GMLPT_Float);
                }
                else
                {
                    poPDefn->SetType(GMLPT_Real);
                }
                poPDefn->SetWidth(atoi(CPLGetXMLValue(psThis, "Width", "0")));
                poPDefn->SetPrecision(
                    atoi(CPLGetXMLValue(psThis, "Precision", "0")));
            }
            else if( EQUAL(pszType, "StringList") )
            {
                if( EQUAL(pszSubType, "Boolean") )
                    poPDefn->SetType(GMLPT_BooleanList);
                else
                    poPDefn->SetType(GMLPT_StringList);
            }
            else if( EQUAL(pszType, "IntegerList") )
            {
                if( EQUAL(pszSubType, "Integer64") )
                    poPDefn->SetType(GMLPT_Integer64List);
                else
                    poPDefn->SetType(GMLPT_IntegerList);
            }
            else if( EQUAL(pszType, "RealList") )
            {
                poPDefn->SetType(GMLPT_RealList);
            }
            else if( EQUAL(pszType, "Complex") ) {
                poPDefn->SetType(GMLPT_Complex);
            }
            else if( EQUAL(pszType, "FeatureProperty") )
            {
                poPDefn->SetType(GMLPT_FeatureProperty);
            }
            else if( EQUAL(pszType, "FeaturePropertyList") )
            {
                poPDefn->SetType(GMLPT_FeaturePropertyList );
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unrecognized property type (%s) in (%s).",
                         pszType, pszName);
                delete poPDefn;
                return false;
            }
            if( pszCondition != nullptr )
                poPDefn->SetCondition(pszCondition);

            if( AddProperty(poPDefn) < 0 )
                delete poPDefn;
        }
    }

    return true;
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *GMLFeatureClass::SerializeToXML()

{
    // Set feature class and core information.
    CPLXMLNode *psRoot = CPLCreateXMLNode(nullptr, CXT_Element, "GMLFeatureClass");

    CPLCreateXMLElementAndValue(psRoot, "Name", GetName());
    CPLCreateXMLElementAndValue(psRoot, "ElementPath", GetElementName());

    if( m_nGeometryPropertyCount > 1 )
    {
        for(int i = 0; i < m_nGeometryPropertyCount; i++)
        {
            GMLGeometryPropertyDefn *poGeomFDefn = m_papoGeometryProperty[i];

            CPLXMLNode *psPDefnNode =
                CPLCreateXMLNode(psRoot, CXT_Element, "GeomPropertyDefn");
            if( strlen(poGeomFDefn->GetName()) > 0 )
                CPLCreateXMLElementAndValue(psPDefnNode, "Name",
                                            poGeomFDefn->GetName());
            if( poGeomFDefn->GetSrcElement() != nullptr &&
                strlen(poGeomFDefn->GetSrcElement()) > 0 )
                CPLCreateXMLElementAndValue(psPDefnNode, "ElementPath",
                                            poGeomFDefn->GetSrcElement());

            if( poGeomFDefn->GetType() != 0 /* wkbUnknown */ )
            {
                char szValue[128] = {};

                const OGRwkbGeometryType eType =
                    static_cast<OGRwkbGeometryType>(poGeomFDefn->GetType());

                CPLString osStr(OGRToOGCGeomType(eType));
                if( wkbHasZ(eType) )
                    osStr += "Z";
                CPLCreateXMLNode(psPDefnNode, CXT_Comment, osStr.c_str());

                snprintf(szValue, sizeof(szValue), "%d", eType);
                CPLCreateXMLElementAndValue(psPDefnNode, "Type", szValue);
            }
        }
    }
    else if( m_nGeometryPropertyCount == 1 )
    {
        GMLGeometryPropertyDefn *poGeomFDefn = m_papoGeometryProperty[0];

        if( strlen(poGeomFDefn->GetName()) > 0 )
            CPLCreateXMLElementAndValue(psRoot, "GeometryName",
                                        poGeomFDefn->GetName());

        if( poGeomFDefn->GetSrcElement() != nullptr &&
            strlen(poGeomFDefn->GetSrcElement()) > 0 )
            CPLCreateXMLElementAndValue(psRoot, "GeometryElementPath",
                                        poGeomFDefn->GetSrcElement());

        if( poGeomFDefn->GetType() != 0 /* wkbUnknown */ )
        {
            char szValue[128] = {};

            OGRwkbGeometryType eType =
                static_cast<OGRwkbGeometryType>(poGeomFDefn->GetType());

            CPLString osStr(OGRToOGCGeomType(eType));
            if( wkbHasZ(eType) )
                osStr += "Z";
            CPLCreateXMLNode(psRoot, CXT_Comment, osStr.c_str());

            snprintf(szValue, sizeof(szValue), "%d", eType);
            CPLCreateXMLElementAndValue(psRoot, "GeometryType", szValue);
        }
    }
    else
    {
        CPLCreateXMLElementAndValue(psRoot, "GeometryType", "100");
    }

    const char *pszSRSName = GetSRSName();
    if( pszSRSName )
    {
        CPLCreateXMLElementAndValue(psRoot, "SRSName", pszSRSName);
    }

    // Write out dataset specific information.
    if( m_bHaveExtents || m_nFeatureCount != -1 || m_pszExtraInfo != nullptr )
    {
        CPLXMLNode *psDSI =
            CPLCreateXMLNode(psRoot, CXT_Element, "DatasetSpecificInfo");

        if( m_nFeatureCount != -1 )
        {
            char szValue[128] = {};

            snprintf(szValue, sizeof(szValue), CPL_FRMT_GIB, m_nFeatureCount);
            CPLCreateXMLElementAndValue(psDSI, "FeatureCount", szValue);
        }

        if( m_bHaveExtents &&
            fabs(m_dfXMin) < 1e100 &&
            fabs(m_dfXMax) < 1e100 &&
            fabs(m_dfYMin) < 1e100 &&
            fabs(m_dfYMax) < 1e100 )
        {
            char szValue[128] = {};

            CPLsnprintf(szValue, sizeof(szValue), "%.5f", m_dfXMin);
            CPLCreateXMLElementAndValue(psDSI, "ExtentXMin", szValue);

            CPLsnprintf(szValue, sizeof(szValue), "%.5f", m_dfXMax);
            CPLCreateXMLElementAndValue(psDSI, "ExtentXMax", szValue);

            CPLsnprintf(szValue, sizeof(szValue), "%.5f", m_dfYMin);
            CPLCreateXMLElementAndValue(psDSI, "ExtentYMin", szValue);

            CPLsnprintf(szValue, sizeof(szValue), "%.5f", m_dfYMax);
            CPLCreateXMLElementAndValue(psDSI, "ExtentYMax", szValue);
        }

        if( m_pszExtraInfo )
            CPLCreateXMLElementAndValue(psDSI, "ExtraInfo", m_pszExtraInfo);
    }

    CPLXMLNode* psLastChild = psRoot->psChild;
    while( psLastChild->psNext )
    {
        psLastChild = psLastChild->psNext;
    }

    // Emit property information.
    for( int iProperty = 0; iProperty < GetPropertyCount(); iProperty++ )
    {
        GMLPropertyDefn *poPDefn = GetProperty(iProperty);
        const char *pszTypeName = "Unknown";

        CPLXMLNode *psPDefnNode =
            CPLCreateXMLNode(nullptr, CXT_Element, "PropertyDefn");
        psLastChild->psNext = psPDefnNode;
        psLastChild = psPDefnNode;
        CPLCreateXMLElementAndValue(psPDefnNode, "Name", poPDefn->GetName());
        CPLCreateXMLElementAndValue(psPDefnNode, "ElementPath",
                                    poPDefn->GetSrcElement());
        const auto gmlType = poPDefn->GetType();
        const char* pszSubTypeName = nullptr;
        switch( gmlType )
        {
          case GMLPT_Untyped:
            pszTypeName = "Untyped";
            break;

          case GMLPT_String:
            pszTypeName = "String";
            break;

          case GMLPT_Boolean:
            pszTypeName = "String";
            pszSubTypeName = "Boolean";
            break;

          case GMLPT_Date:
            pszTypeName = "String";
            pszSubTypeName = "Date";
            break;

          case GMLPT_Time:
            pszTypeName = "String";
            pszSubTypeName = "Time";
            break;

          case GMLPT_DateTime:
            pszTypeName = "String";
            pszSubTypeName = "DateTime";
            break;

          case GMLPT_Integer:
            pszTypeName = "Integer";
            break;

          case GMLPT_Short:
            pszTypeName = "Integer";
            pszSubTypeName = "Short";
            break;

          case GMLPT_Integer64:
            pszTypeName = "Integer";
            pszSubTypeName = "Integer64";
            break;

          case GMLPT_Real:
            pszTypeName = "Real";
            break;

          case GMLPT_Float:
            pszTypeName = "Real";
            pszSubTypeName = "Float";
            break;

          case GMLPT_Complex:
            pszTypeName = "Complex";
            break;

          case GMLPT_IntegerList:
            pszTypeName = "IntegerList";
            break;

          case GMLPT_Integer64List:
            pszTypeName = "IntegerList";
            pszSubTypeName = "Integer64";
            break;

          case GMLPT_RealList:
            pszTypeName = "RealList";
            break;

          case GMLPT_StringList:
            pszTypeName = "StringList";
            break;

          case GMLPT_BooleanList:
            pszTypeName = "StringList";
            pszSubTypeName = "Boolean";
            break;

          // Should not happen in practice for now because this is not
          // autodetected.
          case GMLPT_FeatureProperty:
            pszTypeName = "FeatureProperty";
            break;

          // Should not happen in practice for now because this is not
          // autodetected.
          case GMLPT_FeaturePropertyList:
            pszTypeName = "FeaturePropertyList";
            break;
        }
        CPLCreateXMLElementAndValue(psPDefnNode, "Type", pszTypeName);
        if( pszSubTypeName )
            CPLCreateXMLElementAndValue(psPDefnNode, "Subtype", pszSubTypeName);

        if( EQUAL(pszTypeName, "String") )
        {
            char szMaxLength[48] = {};
            snprintf(szMaxLength, sizeof(szMaxLength), "%d",
                     poPDefn->GetWidth());
            CPLCreateXMLElementAndValue(psPDefnNode, "Width", szMaxLength);
        }
        if( poPDefn->GetWidth() > 0 && EQUAL(pszTypeName, "Integer") )
        {
            char szLength[48] = {};
            snprintf(szLength, sizeof(szLength), "%d", poPDefn->GetWidth());
            CPLCreateXMLElementAndValue(psPDefnNode, "Width", szLength);
        }
        if( poPDefn->GetWidth() > 0 && EQUAL(pszTypeName, "Real") )
        {
            char szLength[48] = {};
            snprintf(szLength, sizeof(szLength), "%d", poPDefn->GetWidth());
            CPLCreateXMLElementAndValue(psPDefnNode, "Width", szLength);
            char szPrecision[48] = {};
            snprintf(szPrecision, sizeof(szPrecision), "%d",
                     poPDefn->GetPrecision());
            CPLCreateXMLElementAndValue(psPDefnNode, "Precision", szPrecision);
        }
    }

    return psRoot;
}

/************************************************************************/
/*                       GML_GetOGRFieldType()                          */
/************************************************************************/

OGRFieldType GML_GetOGRFieldType(GMLPropertyType eType, OGRFieldSubType& eSubType)
{
    OGRFieldType eFType = OFTString;
    eSubType = OFSTNone;
    if( eType == GMLPT_Untyped )
        eFType = OFTString;
    else if( eType == GMLPT_String )
        eFType = OFTString;
    else if( eType == GMLPT_Integer )
        eFType = OFTInteger;
    else if( eType == GMLPT_Boolean )
    {
        eFType = OFTInteger;
        eSubType = OFSTBoolean;
    }
    else if( eType == GMLPT_Short )
    {
        eFType = OFTInteger;
        eSubType = OFSTInt16;
    }
    else if( eType == GMLPT_Integer64 )
        eFType = OFTInteger64;
    else if( eType == GMLPT_Real )
        eFType = OFTReal;
    else if( eType == GMLPT_Float )
    {
        eFType = OFTReal;
        eSubType = OFSTFloat32;
    }
    else if( eType == GMLPT_StringList )
        eFType = OFTStringList;
    else if( eType == GMLPT_IntegerList )
        eFType = OFTIntegerList;
    else if( eType == GMLPT_BooleanList )
    {
        eFType = OFTIntegerList;
        eSubType = OFSTBoolean;
    }
    else if( eType == GMLPT_Integer64List )
        eFType = OFTInteger64List;
    else if( eType == GMLPT_RealList )
        eFType = OFTRealList;
    else if( eType == GMLPT_Date)
        eFType = OFTDate;
    else if( eType == GMLPT_Time )
        eFType = OFTTime;
    else if( eType == GMLPT_DateTime )
        eFType = OFTDateTime;
    else if( eType == GMLPT_FeaturePropertyList )
        eFType = OFTStringList;
    return eFType;
}
