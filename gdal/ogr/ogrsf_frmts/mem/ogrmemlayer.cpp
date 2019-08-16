/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMemLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"
#include "ogr_mem.h"

#include <cstddef>
#include <cstring>
#include <algorithm>
#include <map>
#include <new>
#include <utility>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"
#include "ogrsf_frmts.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                      IOGRMemLayerFeatureIterator                     */
/************************************************************************/

class IOGRMemLayerFeatureIterator
{
  public:
    virtual ~IOGRMemLayerFeatureIterator() {}

    virtual OGRFeature *Next() = 0;
};

/************************************************************************/
/*                            OGRMemLayer()                             */
/************************************************************************/

OGRMemLayer::OGRMemLayer( const char *pszName, OGRSpatialReference *poSRSIn,
                          OGRwkbGeometryType eReqType ) :
    m_poFeatureDefn(new OGRFeatureDefn(pszName)),
    m_nFeatureCount(0),
    m_iNextReadFID(0),
    m_nMaxFeatureCount(0),
    m_papoFeatures(nullptr),
    m_bHasHoles(false),
    m_iNextCreateFID(0),
    m_bUpdatable(true),
    m_bAdvertizeUTF8(false),
    m_bUpdated(false)
{
    m_poFeatureDefn->Reference();

    SetDescription(m_poFeatureDefn->GetName());
    m_poFeatureDefn->SetGeomType(eReqType);

    if( eReqType != wkbNone && poSRSIn != nullptr )
    {
        OGRSpatialReference *poSRS = poSRSIn->Clone();
        m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
        poSRS->Release();
    }

    m_oMapFeaturesIter = m_oMapFeatures.begin();
}

/************************************************************************/
/*                           ~OGRMemLayer()                           */
/************************************************************************/

OGRMemLayer::~OGRMemLayer()

{
    if( m_nFeaturesRead > 0 && m_poFeatureDefn != nullptr )
    {
        CPLDebug("Mem", CPL_FRMT_GIB " features read on layer '%s'.",
                 m_nFeaturesRead, m_poFeatureDefn->GetName());
    }

    if( m_papoFeatures != nullptr )
    {
        for( GIntBig i = 0; i < m_nMaxFeatureCount; i++ )
        {
            if( m_papoFeatures[i] != nullptr )
                delete m_papoFeatures[i];
        }
        CPLFree(m_papoFeatures);
    }
    else
    {
        for( m_oMapFeaturesIter = m_oMapFeatures.begin();
             m_oMapFeaturesIter != m_oMapFeatures.end();
             ++m_oMapFeaturesIter )
        {
            delete m_oMapFeaturesIter->second;
        }
    }

    if( m_poFeatureDefn )
        m_poFeatureDefn->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRMemLayer::ResetReading()

{
    m_iNextReadFID = 0;
    m_oMapFeaturesIter = m_oMapFeatures.begin();
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRMemLayer::GetNextFeature()

{
    while( true )
    {
        OGRFeature *poFeature = nullptr;
        if( m_papoFeatures )
        {
            if( m_iNextReadFID >= m_nMaxFeatureCount )
                return nullptr;
            poFeature = m_papoFeatures[m_iNextReadFID++];
            if( poFeature == nullptr )
                continue;
        }
        else if( m_oMapFeaturesIter != m_oMapFeatures.end() )
        {
            poFeature = m_oMapFeaturesIter->second;
            ++m_oMapFeaturesIter;
        }
        else
        {
            break;
        }

        if( (m_poFilterGeom == nullptr ||
             FilterGeometry(poFeature->GetGeomFieldRef(m_iGeomFieldFilter)) )
            && (m_poAttrQuery == nullptr ||
                m_poAttrQuery->Evaluate(poFeature)) )
        {
            m_nFeaturesRead++;
            return poFeature->Clone();
        }
    }

    return nullptr;
}

/************************************************************************/
/*                           SetNextByIndex()                           */
/************************************************************************/

OGRErr OGRMemLayer::SetNextByIndex( GIntBig nIndex )

{
    if( m_poFilterGeom != nullptr || m_poAttrQuery != nullptr ||
        m_papoFeatures == nullptr || m_bHasHoles )
        return OGRLayer::SetNextByIndex(nIndex);

    if( nIndex < 0 || nIndex >= m_nMaxFeatureCount )
        return OGRERR_FAILURE;

    m_iNextReadFID = nIndex;

    return OGRERR_NONE;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRMemLayer::GetFeature( GIntBig nFeatureId )

{
    if( nFeatureId < 0 )
        return nullptr;

    OGRFeature *poFeature = nullptr;
    if( m_papoFeatures != nullptr )
    {
        if( nFeatureId >= m_nMaxFeatureCount )
            return nullptr;
        poFeature = m_papoFeatures[nFeatureId];
    }
    else
    {
        FeatureIterator oIter = m_oMapFeatures.find(nFeatureId);
        if( oIter != m_oMapFeatures.end() )
            poFeature = oIter->second;
    }
    if( poFeature == nullptr )
        return nullptr;

    return poFeature->Clone();
}

/************************************************************************/
/*                             ISetFeature()                             */
/************************************************************************/

OGRErr OGRMemLayer::ISetFeature( OGRFeature *poFeature )

{
    if( !m_bUpdatable )
        return OGRERR_FAILURE;

    if( poFeature == nullptr )
        return OGRERR_FAILURE;

    // If we don't have a FID, find one available
    GIntBig nFID = poFeature->GetFID();
    if( nFID == OGRNullFID )
    {
        if( m_papoFeatures != nullptr )
        {
            while( m_iNextCreateFID < m_nMaxFeatureCount &&
                   m_papoFeatures[m_iNextCreateFID] != nullptr )
            {
                m_iNextCreateFID++;
            }
        }
        else
        {
            FeatureIterator oIter;
            while( (oIter = m_oMapFeatures.find(m_iNextCreateFID)) !=
                   m_oMapFeatures.end() )
                ++m_iNextCreateFID;
        }
        nFID = m_iNextCreateFID++;
        poFeature->SetFID(nFID);
    }
    else if( nFID < OGRNullFID )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "negative FID are not supported");
        return OGRERR_FAILURE;
    }
    else
    {
        if( !m_bHasHoles )
        {
            // If the feature does not exist, set m_bHasHoles
            if( m_papoFeatures != nullptr )
            {
                if( nFID >= m_nMaxFeatureCount ||
                    m_papoFeatures[nFID] == nullptr )
                {
                    m_bHasHoles = true;
                }
            }
            else
            {
                FeatureIterator oIter = m_oMapFeatures.find(nFID);
                if( oIter == m_oMapFeatures.end() )
                    m_bHasHoles = true;
            }
        }
    }

    OGRFeature *poFeatureCloned = poFeature->Clone();
    if( poFeatureCloned == nullptr )
        return OGRERR_FAILURE;

    if( m_papoFeatures != nullptr && nFID > 100000 &&
        nFID > m_nMaxFeatureCount + 1000 )
    {
        // Convert to map if gap from current max size is too big.
        IOGRMemLayerFeatureIterator *poIter = GetIterator();
        try
        {
            OGRFeature *poFeatureIter = nullptr;
            while( (poFeatureIter = poIter->Next()) != nullptr )
            {
                m_oMapFeatures[poFeatureIter->GetFID()] = poFeatureIter;
            }
            delete poIter;
            CPLFree(m_papoFeatures);
            m_papoFeatures = nullptr;
            m_nMaxFeatureCount = 0;
        }
        catch( const std::bad_alloc & )
        {
            m_oMapFeatures.clear();
            CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate memory");
            delete poFeatureCloned;
            delete poIter;
            return OGRERR_FAILURE;
        }
    }

    if( m_papoFeatures != nullptr ||
        (m_oMapFeatures.empty() && nFID <= 100000) )
    {
        if( nFID >= m_nMaxFeatureCount )
        {
            const GIntBig nNewCount = std::max(
                m_nMaxFeatureCount + m_nMaxFeatureCount / 3 + 10, nFID + 1);
            if( static_cast<GIntBig>(static_cast<size_t>(sizeof(OGRFeature *)) *
                                     nNewCount) !=
                static_cast<GIntBig>(sizeof(OGRFeature *)) * nNewCount )
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                         "Cannot allocate array of " CPL_FRMT_GIB " elements",
                         nNewCount);
                delete poFeatureCloned;
                return OGRERR_FAILURE;
            }

            OGRFeature **papoNewFeatures =
                static_cast<OGRFeature **>(VSI_REALLOC_VERBOSE(
                    m_papoFeatures,
                    static_cast<size_t>(sizeof(OGRFeature *) * nNewCount)));
            if( papoNewFeatures == nullptr )
            {
                delete poFeatureCloned;
                return OGRERR_FAILURE;
            }
            m_papoFeatures = papoNewFeatures;
            memset(m_papoFeatures + m_nMaxFeatureCount, 0,
                   sizeof(OGRFeature *) *
                       static_cast<size_t>(nNewCount - m_nMaxFeatureCount));
            m_nMaxFeatureCount = nNewCount;
        }
#ifdef DEBUG
        // Just to please Coverity. Cannot happen.
        if( m_papoFeatures == nullptr )
        {
            delete poFeatureCloned;
            return OGRERR_FAILURE;
        }
#endif

        if( m_papoFeatures[nFID] != nullptr )
        {
            delete m_papoFeatures[nFID];
            m_papoFeatures[nFID] = nullptr;
        }
        else
        {
            ++m_nFeatureCount;
        }

        m_papoFeatures[nFID] = poFeatureCloned;
    }
    else
    {
        FeatureIterator oIter = m_oMapFeatures.find(nFID);
        if( oIter != m_oMapFeatures.end() )
        {
            delete oIter->second;
            oIter->second = poFeatureCloned;
        }
        else
        {
            try
            {
                m_oMapFeatures[nFID] = poFeatureCloned;
                m_nFeatureCount++;
            }
            catch( const std::bad_alloc & )
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                         "Cannot allocate memory");
                delete poFeatureCloned;
                return OGRERR_FAILURE;
            }
        }
    }

    for( int i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); ++i )
    {
        OGRGeometry *poGeom = poFeatureCloned->GetGeomFieldRef(i);
        if( poGeom != nullptr && poGeom->getSpatialReference() == nullptr )
        {
            poGeom->assignSpatialReference(
                m_poFeatureDefn->GetGeomFieldDefn(i)->GetSpatialRef());
        }
    }

    m_bUpdated = true;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRMemLayer::ICreateFeature( OGRFeature *poFeature )

{
    if( !m_bUpdatable )
        return OGRERR_FAILURE;

    if( poFeature->GetFID() != OGRNullFID &&
        poFeature->GetFID() != m_iNextCreateFID )
        m_bHasHoles = true;

    // If the feature has already a FID and that a feature with the same
    // FID is already registered in the layer, then unset our FID.
    if( poFeature->GetFID() >= 0 )
    {
        if( m_papoFeatures != nullptr )
        {
            if( poFeature->GetFID() < m_nMaxFeatureCount &&
                m_papoFeatures[poFeature->GetFID()] != nullptr )
            {
                poFeature->SetFID(OGRNullFID);
            }
        }
        else
        {
            FeatureIterator oIter = m_oMapFeatures.find(poFeature->GetFID());
            if( oIter != m_oMapFeatures.end() )
                poFeature->SetFID(OGRNullFID);
        }
    }

    return SetFeature(poFeature);
}

/************************************************************************/
/*                           DeleteFeature()                            */
/************************************************************************/

OGRErr OGRMemLayer::DeleteFeature( GIntBig nFID )

{
    if( !m_bUpdatable )
        return OGRERR_FAILURE;

    if( nFID < 0 )
    {
        return OGRERR_FAILURE;
    }

    if( m_papoFeatures != nullptr )
    {
        if( nFID >= m_nMaxFeatureCount || m_papoFeatures[nFID] == nullptr )
        {
            return OGRERR_FAILURE;
        }
        delete m_papoFeatures[nFID];
        m_papoFeatures[nFID] = nullptr;
    }
    else
    {
        FeatureIterator oIter = m_oMapFeatures.find(nFID);
        if( oIter == m_oMapFeatures.end() )
        {
            return OGRERR_FAILURE;
        }
        delete oIter->second;
        m_oMapFeatures.erase(oIter);
    }

    m_bHasHoles = true;
    --m_nFeatureCount;

    m_bUpdated = true;

    return OGRERR_NONE;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      If a spatial filter is in effect, we turn control over to       */
/*      the generic counter.  Otherwise we return the total count.      */
/*      Eventually we should consider implementing a more efficient     */
/*      way of counting features matching a spatial query.              */
/************************************************************************/

GIntBig OGRMemLayer::GetFeatureCount( int bForce )

{
    if( m_poFilterGeom != nullptr || m_poAttrQuery != nullptr )
        return OGRLayer::GetFeatureCount(bForce);

    return m_nFeatureCount;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRMemLayer::TestCapability( const char *pszCap )

{
    if( EQUAL(pszCap, OLCRandomRead) )
        return TRUE;

    else if( EQUAL(pszCap, OLCSequentialWrite) ||
             EQUAL(pszCap, OLCRandomWrite) )
        return m_bUpdatable;

    else if( EQUAL(pszCap, OLCFastFeatureCount) )
        return m_poFilterGeom == nullptr && m_poAttrQuery == nullptr;

    else if( EQUAL(pszCap, OLCFastSpatialFilter) )
        return FALSE;

    else if( EQUAL(pszCap, OLCDeleteFeature) )
        return m_bUpdatable;

    else if( EQUAL(pszCap, OLCCreateField) ||
             EQUAL(pszCap, OLCCreateGeomField) ||
             EQUAL(pszCap, OLCDeleteField) ||
             EQUAL(pszCap, OLCReorderFields) ||
             EQUAL(pszCap, OLCAlterFieldDefn) )
        return m_bUpdatable;

    else if( EQUAL(pszCap, OLCFastSetNextByIndex) )
        return m_poFilterGeom == nullptr && m_poAttrQuery == nullptr &&
               ((m_papoFeatures != nullptr && !m_bHasHoles) ||
                m_oMapFeatures.empty());

    else if( EQUAL(pszCap, OLCStringsAsUTF8) )
        return m_bAdvertizeUTF8;

    else if( EQUAL(pszCap, OLCCurveGeometries) )
        return TRUE;

    else if( EQUAL(pszCap, OLCMeasuredGeometries) )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRMemLayer::CreateField( OGRFieldDefn *poField, int /* bApproxOK */ )
{
    if( !m_bUpdatable )
        return OGRERR_FAILURE;

    // Simple case, no features exist yet.
    if( m_nFeatureCount == 0 )
    {
        m_poFeatureDefn->AddFieldDefn(poField);
        return OGRERR_NONE;
    }

    // Add field definition and setup remap definition.
    m_poFeatureDefn->AddFieldDefn(poField);

    // Remap all the internal features.  Hopefully there aren't any
    // external features referring to our OGRFeatureDefn!
    IOGRMemLayerFeatureIterator *poIter = GetIterator();
    OGRFeature *poFeature = nullptr;
    while( (poFeature = poIter->Next()) != nullptr )
    {
        poFeature->AppendField();
    }
    delete poIter;

    m_bUpdated = true;

    return OGRERR_NONE;
}

/************************************************************************/
/*                            DeleteField()                             */
/************************************************************************/

OGRErr OGRMemLayer::DeleteField( int iField )
{
    if( !m_bUpdatable )
        return OGRERR_FAILURE;

    if( iField < 0 || iField >= m_poFeatureDefn->GetFieldCount() )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid field index");
        return OGRERR_FAILURE;
    }

    // Update all the internal features.  Hopefully there aren't any
    // external features referring to our OGRFeatureDefn!
    IOGRMemLayerFeatureIterator *poIter = GetIterator();
    OGRFeature *poFeature = nullptr;
    while( (poFeature = poIter->Next()) != nullptr )
    {
        OGRField *poFieldRaw = poFeature->GetRawFieldRef(iField);
        if( poFeature->IsFieldSetAndNotNull(iField) &&
            !poFeature->IsFieldNull(iField) )
        {
            // Little trick to unallocate the field.
            OGRField sField;
            OGR_RawField_SetUnset(&sField);
            poFeature->SetField(iField, &sField);
        }

        if( iField < m_poFeatureDefn->GetFieldCount() - 1 )
        {
            memmove(
                poFieldRaw, poFieldRaw + 1,
                sizeof(OGRField) *
                (m_poFeatureDefn->GetFieldCount() - 1 - iField) );
        }
    }
    delete poIter;

    m_bUpdated = true;

    return m_poFeatureDefn->DeleteFieldDefn(iField);
}

/************************************************************************/
/*                           ReorderFields()                            */
/************************************************************************/

OGRErr OGRMemLayer::ReorderFields( int *panMap )
{
    if( !m_bUpdatable )
        return OGRERR_FAILURE;

    if( m_poFeatureDefn->GetFieldCount() == 0 )
        return OGRERR_NONE;

    const OGRErr eErr =
        OGRCheckPermutation(panMap, m_poFeatureDefn->GetFieldCount());
    if( eErr != OGRERR_NONE )
        return eErr;

    // Remap all the internal features.  Hopefully there aren't any
    // external features referring to our OGRFeatureDefn!
    IOGRMemLayerFeatureIterator *poIter = GetIterator();
    OGRFeature *poFeature = nullptr;
    while( (poFeature = poIter->Next()) != nullptr )
    {
        poFeature->RemapFields(nullptr, panMap);
    }
    delete poIter;

    m_bUpdated = true;

    return m_poFeatureDefn->ReorderFieldDefns(panMap);
}

/************************************************************************/
/*                           AlterFieldDefn()                           */
/************************************************************************/

OGRErr OGRMemLayer::AlterFieldDefn( int iField, OGRFieldDefn *poNewFieldDefn,
                                    int nFlagsIn )
{
    if( !m_bUpdatable )
        return OGRERR_FAILURE;

    if( iField < 0 || iField >= m_poFeatureDefn->GetFieldCount() )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid field index");
        return OGRERR_FAILURE;
    }

    OGRFieldDefn *poFieldDefn = m_poFeatureDefn->GetFieldDefn(iField);

    if( (nFlagsIn & ALTER_TYPE_FLAG) &&
        (poFieldDefn->GetType() != poNewFieldDefn->GetType() ||
         poFieldDefn->GetSubType() != poNewFieldDefn->GetSubType()))
    {
        if( (poNewFieldDefn->GetType() == OFTDate ||
             poNewFieldDefn->GetType() == OFTTime ||
             poNewFieldDefn->GetType() == OFTDateTime) &&
            (poFieldDefn->GetType() == OFTDate ||
             poFieldDefn->GetType() == OFTTime ||
             poFieldDefn->GetType() == OFTDateTime) )
        {
            // Do nothing on features.
        }
        else if( poNewFieldDefn->GetType() == OFTInteger64 &&
                 poFieldDefn->GetType() == OFTInteger )
        {
            // Update all the internal features.  Hopefully there aren't any
            // external features referring to our OGRFeatureDefn!
            IOGRMemLayerFeatureIterator *poIter = GetIterator();
            OGRFeature *poFeature = nullptr;
            while( (poFeature = poIter->Next()) != nullptr )
            {
                OGRField *poFieldRaw = poFeature->GetRawFieldRef(iField);
                if( poFeature->IsFieldSetAndNotNull(iField) &&
                    !poFeature->IsFieldNull(iField) )
                {
                    const GIntBig nVal = poFieldRaw->Integer;
                    poFieldRaw->Integer64 = nVal;
                }
            }
            delete poIter;
        }
        else if( poNewFieldDefn->GetType() == OFTReal &&
                 poFieldDefn->GetType() == OFTInteger )
        {
            // Update all the internal features.  Hopefully there aren't any
            // external features referring to our OGRFeatureDefn!
            IOGRMemLayerFeatureIterator *poIter = GetIterator();
            OGRFeature *poFeature = nullptr;
            while( (poFeature = poIter->Next()) != nullptr )
            {
                OGRField *poFieldRaw = poFeature->GetRawFieldRef(iField);
                if( poFeature->IsFieldSetAndNotNull(iField) &&
                    !poFeature->IsFieldNull(iField) )
                {
                    const double dfVal = poFieldRaw->Integer;
                    poFieldRaw->Real = dfVal;
                }
            }
            delete poIter;
        }
        else if( poNewFieldDefn->GetType() == OFTReal &&
                 poFieldDefn->GetType() == OFTInteger64 )
        {
            // Update all the internal features.  Hopefully there aren't any
            // external features referring to our OGRFeatureDefn!
            IOGRMemLayerFeatureIterator *poIter = GetIterator();
            OGRFeature *poFeature = nullptr;
            while( (poFeature = poIter->Next()) != nullptr )
            {
                OGRField *poFieldRaw = poFeature->GetRawFieldRef(iField);
                if( poFeature->IsFieldSetAndNotNull(iField) &&
                    !poFeature->IsFieldNull(iField) )
                {
                    const double dfVal =
                        static_cast<double>(poFieldRaw->Integer64);
                    poFieldRaw->Real = dfVal;
                }
            }
            delete poIter;
        }
        else
        {
            if( poFieldDefn->GetType() != OGRUnknownType )
            {
                if( poNewFieldDefn->GetType() != OFTString )
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Can only convert from OFTInteger to OFTReal, "
                             "or from anything to OFTString");
                    return OGRERR_FAILURE;
                }
            }

            // Update all the internal features.  Hopefully there aren't any
            // external features referring to our OGRFeatureDefn!
            IOGRMemLayerFeatureIterator *poIter = GetIterator();
            OGRFeature *poFeature = nullptr;
            while( (poFeature = poIter->Next()) != nullptr )
            {
                OGRField *poFieldRaw = poFeature->GetRawFieldRef(iField);
                if( poFeature->IsFieldSetAndNotNull(iField) &&
                    !poFeature->IsFieldNull(iField) )
                {
                    char *pszVal =
                        CPLStrdup(poFeature->GetFieldAsString(iField));

                    // Little trick to unallocate the field.
                    OGRField sField;
                    OGR_RawField_SetUnset(&sField);
                    poFeature->SetField(iField, &sField);

                    poFieldRaw->String = pszVal;
                }
            }
            delete poIter;
        }

        poFieldDefn->SetSubType(OFSTNone);
        poFieldDefn->SetType(poNewFieldDefn->GetType());
        poFieldDefn->SetSubType(poNewFieldDefn->GetSubType());
    }

    if( nFlagsIn & ALTER_NAME_FLAG )
        poFieldDefn->SetName(poNewFieldDefn->GetNameRef());
    if( nFlagsIn & ALTER_WIDTH_PRECISION_FLAG )
    {
        poFieldDefn->SetWidth(poNewFieldDefn->GetWidth());
        poFieldDefn->SetPrecision(poNewFieldDefn->GetPrecision());
    }

    m_bUpdated = true;

    return OGRERR_NONE;
}

/************************************************************************/
/*                          CreateGeomField()                           */
/************************************************************************/

OGRErr OGRMemLayer::CreateGeomField( OGRGeomFieldDefn *poGeomField,
                                     int /* bApproxOK */ )
{
    if( !m_bUpdatable )
        return OGRERR_FAILURE;

    // Simple case, no features exist yet.
    if( m_nFeatureCount == 0 )
    {
        m_poFeatureDefn->AddGeomFieldDefn(poGeomField);
        return OGRERR_NONE;
    }

    // Add field definition and setup remap definition.
    m_poFeatureDefn->AddGeomFieldDefn(poGeomField);

    int *panRemap = static_cast<int *>(
        CPLMalloc(sizeof(int) * m_poFeatureDefn->GetGeomFieldCount()));
    for( GIntBig i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); ++i )
    {
        if( i < m_poFeatureDefn->GetGeomFieldCount() - 1 )
            panRemap[i] = static_cast<int>(i);
        else
            panRemap[i] = -1;
    }

    // Remap all the internal features.  Hopefully there aren't any
    // external features referring to our OGRFeatureDefn!
    IOGRMemLayerFeatureIterator *poIter = GetIterator();
    OGRFeature *poFeature = nullptr;
    while( (poFeature = poIter->Next()) != nullptr )
    {
        poFeature->RemapGeomFields(nullptr, panRemap);
    }
    delete poIter;

    CPLFree(panRemap);

    m_bUpdated = true;

    return OGRERR_NONE;
}

/************************************************************************/
/*                        OGRMemLayerIteratorArray                      */
/************************************************************************/

class OGRMemLayerIteratorArray final: public IOGRMemLayerFeatureIterator
{
    GIntBig m_iCurIdx;
    GIntBig m_nMaxFeatureCount;
    OGRFeature **m_papoFeatures;

  public:
    OGRMemLayerIteratorArray( GIntBig nMaxFeatureCount,
                              OGRFeature **papoFeatures ):
        m_iCurIdx(0),
        m_nMaxFeatureCount(nMaxFeatureCount),
        m_papoFeatures(papoFeatures)
        {}

    virtual ~OGRMemLayerIteratorArray() {}

    virtual OGRFeature *Next() override
    {
        while( m_iCurIdx < m_nMaxFeatureCount )
        {
            OGRFeature *poFeature = m_papoFeatures[m_iCurIdx];
            ++m_iCurIdx;
            if( poFeature != nullptr )
                return poFeature;
        }
        return nullptr;
    }
};

/************************************************************************/
/*                         OGRMemLayerIteratorMap                       */
/************************************************************************/

class OGRMemLayerIteratorMap final: public IOGRMemLayerFeatureIterator
{
    typedef std::map<GIntBig, OGRFeature *>           FeatureMap;
    typedef std::map<GIntBig, OGRFeature *>::iterator FeatureIterator;

    FeatureMap     &m_oMapFeatures;
    FeatureIterator m_oIter;

  public:
    explicit OGRMemLayerIteratorMap(FeatureMap &oMapFeatures) :
        m_oMapFeatures(oMapFeatures),
        m_oIter(oMapFeatures.begin())
        {}

    virtual ~OGRMemLayerIteratorMap() {}

    virtual OGRFeature *Next() override
    {
        if( m_oIter != m_oMapFeatures.end() )
        {
            OGRFeature *poFeature = m_oIter->second;
            ++m_oIter;
            return poFeature;
        }
        return nullptr;
    }

  private:
    CPL_DISALLOW_COPY_ASSIGN(OGRMemLayerIteratorMap)
};

/************************************************************************/
/*                            GetIterator()                             */
/************************************************************************/

IOGRMemLayerFeatureIterator *OGRMemLayer::GetIterator()
{
    if( m_oMapFeatures.empty() )
        return new OGRMemLayerIteratorArray(m_nMaxFeatureCount, m_papoFeatures);

    return new OGRMemLayerIteratorMap(m_oMapFeatures);
}
