/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMemLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_conv.h"
#include "ogr_mem.h"
#include "ogr_p.h"

#include <algorithm>

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
    m_papoFeatures(NULL),
    m_bHasHoles(false),
    m_iNextCreateFID(0),
    m_bUpdatable(true),
    m_bAdvertizeUTF8(false),
    m_bUpdated(false)
{
    m_poFeatureDefn->Reference();

    SetDescription(m_poFeatureDefn->GetName());
    m_poFeatureDefn->SetGeomType(eReqType);

    if( eReqType != wkbNone && poSRSIn != NULL )
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
    if( m_nFeaturesRead > 0 && m_poFeatureDefn != NULL )
    {
        CPLDebug("Mem", CPL_FRMT_GIB " features read on layer '%s'.",
                 m_nFeaturesRead, m_poFeatureDefn->GetName());
    }

    if( m_papoFeatures != NULL )
    {
        for( GIntBig i = 0; i < m_nMaxFeatureCount; i++ )
        {
            if( m_papoFeatures[i] != NULL )
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
        OGRFeature *poFeature = NULL;
        if( m_papoFeatures )
        {
            if( m_iNextReadFID >= m_nMaxFeatureCount )
                return NULL;
            poFeature = m_papoFeatures[m_iNextReadFID++];
            if( poFeature == NULL )
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

        if( (m_poFilterGeom == NULL ||
             FilterGeometry(poFeature->GetGeomFieldRef(m_iGeomFieldFilter)) )
            && (m_poAttrQuery == NULL ||
                m_poAttrQuery->Evaluate(poFeature)) )
        {
            m_nFeaturesRead++;
            return poFeature->Clone();
        }
    }

    return NULL;
}

/************************************************************************/
/*                           SetNextByIndex()                           */
/************************************************************************/

OGRErr OGRMemLayer::SetNextByIndex( GIntBig nIndex )

{
    if( m_poFilterGeom != NULL || m_poAttrQuery != NULL ||
        m_papoFeatures == NULL || m_bHasHoles )
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
        return NULL;

    OGRFeature *poFeature = NULL;
    if( m_papoFeatures != NULL )
    {
        if( nFeatureId >= m_nMaxFeatureCount )
            return NULL;
        poFeature = m_papoFeatures[nFeatureId];
    }
    else
    {
        FeatureIterator oIter = m_oMapFeatures.find(nFeatureId);
        if( oIter != m_oMapFeatures.end() )
            poFeature = oIter->second;
    }
    if( poFeature == NULL )
        return NULL;

    return poFeature->Clone();
}

/************************************************************************/
/*                             ISetFeature()                             */
/************************************************************************/

OGRErr OGRMemLayer::ISetFeature( OGRFeature *poFeature )

{
    if( !m_bUpdatable )
        return OGRERR_FAILURE;

    if( poFeature == NULL )
        return OGRERR_FAILURE;

    // If we don't have a FID, find one available
    if( poFeature->GetFID() == OGRNullFID )
    {
        if( m_papoFeatures != NULL )
        {
            while( m_iNextCreateFID < m_nMaxFeatureCount &&
                   m_papoFeatures[m_iNextCreateFID] != NULL )
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
        poFeature->SetFID(m_iNextCreateFID++);
    }
    else if( poFeature->GetFID() < OGRNullFID )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "negative FID are not supported");
        return OGRERR_FAILURE;
    }

    OGRFeature *poFeatureCloned = poFeature->Clone();
    if( poFeatureCloned == NULL )
        return OGRERR_FAILURE;
    const GIntBig nFID = poFeature->GetFID();

    if( m_papoFeatures != NULL && nFID > 100000 &&
        nFID > m_nMaxFeatureCount + 1000 )
    {
        // Convert to map if gap from current max size is too big.
        IOGRMemLayerFeatureIterator *poIter = GetIterator();
        try
        {
            OGRFeature *poFeatureIter = NULL;
            while( (poFeatureIter = poIter->Next()) != NULL )
            {
                m_oMapFeatures[poFeatureIter->GetFID()] = poFeatureIter;
            }
            delete poIter;
            CPLFree(m_papoFeatures);
            m_papoFeatures = NULL;
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

    if( m_papoFeatures != NULL ||
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
            if( papoNewFeatures == NULL )
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
        if( m_papoFeatures == NULL )
        {
            delete poFeatureCloned;
            return OGRERR_FAILURE;
        }
#endif

        if( m_papoFeatures[nFID] != NULL )
        {
            delete m_papoFeatures[nFID];
            m_papoFeatures[nFID] = NULL;
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
        if( poGeom != NULL && poGeom->getSpatialReference() == NULL )
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
        if( m_papoFeatures != NULL )
        {
            if( poFeature->GetFID() < m_nMaxFeatureCount &&
                m_papoFeatures[poFeature->GetFID()] != NULL )
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

    if( m_papoFeatures != NULL )
    {
        if( nFID >= m_nMaxFeatureCount || m_papoFeatures[nFID] == NULL )
        {
            return OGRERR_FAILURE;
        }
        delete m_papoFeatures[nFID];
        m_papoFeatures[nFID] = NULL;
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
    if( m_poFilterGeom != NULL || m_poAttrQuery != NULL )
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
        return m_poFilterGeom == NULL && m_poAttrQuery == NULL;

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
        return m_poFilterGeom == NULL && m_poAttrQuery == NULL &&
               ((m_papoFeatures != NULL && !m_bHasHoles) ||
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

    int *panRemap = static_cast<int *>(
        CPLMalloc(sizeof(int) * m_poFeatureDefn->GetFieldCount()));
    for( int i = 0; i < m_poFeatureDefn->GetFieldCount(); ++i )
    {
        if( i < m_poFeatureDefn->GetFieldCount() - 1 )
            panRemap[i] = i;
        else
            panRemap[i] = -1;
    }

    // Remap all the internal features.  Hopefully there aren't any
    // external features referring to our OGRFeatureDefn!
    IOGRMemLayerFeatureIterator *poIter = GetIterator();
    OGRFeature *poFeature = NULL;
    while( (poFeature = poIter->Next()) != NULL )
    {
        poFeature->RemapFields(NULL, panRemap);
    }
    delete poIter;

    CPLFree(panRemap);

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
    OGRFeature *poFeature = NULL;
    while( (poFeature = poIter->Next()) != NULL )
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
    OGRFeature *poFeature = NULL;
    while( (poFeature = poIter->Next()) != NULL )
    {
        poFeature->RemapFields(NULL, panMap);
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
            OGRFeature *poFeature = NULL;
            while( (poFeature = poIter->Next()) != NULL )
            {
                OGRField *poFieldRaw = poFeature->GetRawFieldRef(iField);
                if( poFeature->IsFieldSetAndNotNull(iField) &&
                    !poFeature->IsFieldNull(iField) )
                {
                    poFieldRaw->Integer64 = poFieldRaw->Integer;
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
            OGRFeature *poFeature = NULL;
            while( (poFeature = poIter->Next()) != NULL )
            {
                OGRField *poFieldRaw = poFeature->GetRawFieldRef(iField);
                if( poFeature->IsFieldSetAndNotNull(iField) &&
                    !poFeature->IsFieldNull(iField) )
                {
                    poFieldRaw->Real = poFieldRaw->Integer;
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
            OGRFeature *poFeature = NULL;
            while( (poFeature = poIter->Next()) != NULL )
            {
                OGRField *poFieldRaw = poFeature->GetRawFieldRef(iField);
                if( poFeature->IsFieldSetAndNotNull(iField) &&
                    !poFeature->IsFieldNull(iField) )
                {
                    poFieldRaw->Real =
                        static_cast<double>(poFieldRaw->Integer64);
                }
            }
            delete poIter;
        }
        else
        {
            if( poNewFieldDefn->GetType() != OFTString )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Can only convert from OFTInteger to OFTReal, "
                         "or from anything to OFTString");
                return OGRERR_FAILURE;
            }

            // Update all the internal features.  Hopefully there aren't any
            // external features referring to our OGRFeatureDefn!
            IOGRMemLayerFeatureIterator *poIter = GetIterator();
            OGRFeature *poFeature = NULL;
            while( (poFeature = poIter->Next()) != NULL )
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
    OGRFeature *poFeature = NULL;
    while( (poFeature = poIter->Next()) != NULL )
    {
        poFeature->RemapGeomFields(NULL, panRemap);
    }
    delete poIter;

    CPLFree(panRemap);

    m_bUpdated = true;

    return OGRERR_NONE;
}

/************************************************************************/
/*                        OGRMemLayerIteratorArray                      */
/************************************************************************/

class OGRMemLayerIteratorArray: public IOGRMemLayerFeatureIterator
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
            if( poFeature != NULL )
                return poFeature;
        }
        return NULL;
    }
};

/************************************************************************/
/*                         OGRMemLayerIteratorMap                       */
/************************************************************************/

class OGRMemLayerIteratorMap: public IOGRMemLayerFeatureIterator
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
        return NULL;
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
