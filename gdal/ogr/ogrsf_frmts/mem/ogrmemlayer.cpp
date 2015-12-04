/******************************************************************************
 * $Id$
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

#include "ogr_mem.h"
#include "cpl_conv.h"
#include "ogr_p.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                      IOGRMemLayerFeatureIterator                     */
/************************************************************************/

class IOGRMemLayerFeatureIterator
{
    public:
        virtual ~IOGRMemLayerFeatureIterator() {}

        virtual OGRFeature* Next() = 0;
};

/************************************************************************/
/*                            OGRMemLayer()                             */
/************************************************************************/

OGRMemLayer::OGRMemLayer( const char * pszName, OGRSpatialReference *poSRSIn,
                          OGRwkbGeometryType eReqType ) :
    nFeatureCount(0),
    iNextReadFID(0),
    nMaxFeatureCount(0),
    papoFeatures(NULL),
    bHasHoles(FALSE),
    iNextCreateFID(0),
    bUpdatable(TRUE),
    bAdvertizeUTF8(FALSE)
{
    poFeatureDefn = new OGRFeatureDefn( pszName );
    poFeatureDefn->Reference();

    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->SetGeomType( eReqType );

    if( eReqType != wkbNone && poSRSIn != NULL )
    {
        OGRSpatialReference* poSRS = poSRSIn->Clone();
        poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
        poSRS->Release();
    }

    oMapFeaturesIter = oMapFeatures.begin();
}

/************************************************************************/
/*                           ~OGRMemLayer()                           */
/************************************************************************/

OGRMemLayer::~OGRMemLayer()

{
    if( m_nFeaturesRead > 0 && poFeatureDefn != NULL )
    {
        CPLDebug( "Mem", CPL_FRMT_GIB " features read on layer '%s'.",
                  m_nFeaturesRead,
                  poFeatureDefn->GetName() );
    }

    if( papoFeatures != NULL )
    {
        for( GIntBig i = 0; i < nMaxFeatureCount; i++ )
        {
            if( papoFeatures[i] != NULL )
                delete papoFeatures[i];
        }
        CPLFree( papoFeatures );
    }
    else
    {
        for( oMapFeaturesIter = oMapFeatures.begin();
             oMapFeaturesIter != oMapFeatures.end();
             ++oMapFeaturesIter )
        {
            delete oMapFeaturesIter->second;
        }
    }

    if( poFeatureDefn )
        poFeatureDefn->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRMemLayer::ResetReading()

{
    iNextReadFID = 0;
    oMapFeaturesIter = oMapFeatures.begin();
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRMemLayer::GetNextFeature()

{
    while( TRUE )
    {
        OGRFeature *poFeature;
        if( papoFeatures )
        {
            if( iNextReadFID >= nMaxFeatureCount )
                return NULL;
            poFeature = papoFeatures[iNextReadFID++];
            if( poFeature == NULL )
                continue;
        }
        else if( oMapFeaturesIter != oMapFeatures.end() )
        {
            poFeature = oMapFeaturesIter->second;
            ++ oMapFeaturesIter;
        }
        else
            break;

        if( (m_poFilterGeom == NULL
             || FilterGeometry( poFeature->GetGeomFieldRef(m_iGeomFieldFilter) ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature ) ) )
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
        papoFeatures == NULL || bHasHoles )
        return OGRLayer::SetNextByIndex( nIndex );

    if (nIndex < 0 || nIndex >= nMaxFeatureCount)
        return OGRERR_FAILURE;

    iNextReadFID = nIndex;

    return OGRERR_NONE;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRMemLayer::GetFeature( GIntBig nFeatureId )

{
    if( nFeatureId < 0 )
        return NULL;

    OGRFeature* poFeature;
    if( papoFeatures != NULL )
    {
        if( nFeatureId >= nMaxFeatureCount )
            return NULL;
        poFeature = papoFeatures[nFeatureId];
    }
    else
    {
        FeatureIterator oIter = oMapFeatures.find(nFeatureId);
        if( oIter != oMapFeatures.end() )
            poFeature = oIter->second;
        else
            poFeature = NULL;
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
    if (!bUpdatable)
        return OGRERR_FAILURE;

    if( poFeature == NULL )
        return OGRERR_FAILURE;

    // If we don't have a FID, find one available
    if( poFeature->GetFID() == OGRNullFID )
    {
        if( papoFeatures != NULL )
        {
            while( iNextCreateFID < nMaxFeatureCount
                && papoFeatures[iNextCreateFID] != NULL )
            {
                iNextCreateFID++;
            }
        }
        else
        {
            FeatureIterator oIter;
            while( (oIter = oMapFeatures.find(iNextCreateFID)) != oMapFeatures.end() )
                iNextCreateFID++;
        }
        poFeature->SetFID( iNextCreateFID++ );
    }
    else if ( poFeature->GetFID() < OGRNullFID )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "negative FID are not supported");
        return OGRERR_FAILURE;
    }

    OGRFeature* poFeatureCloned = poFeature->Clone();
    if( poFeatureCloned == NULL )
        return OGRERR_FAILURE;
    const GIntBig nFID = poFeature->GetFID();

    if( papoFeatures != NULL && nFID > 100000 && nFID > nMaxFeatureCount + 1000 )
    {
        // Convert to map if gap from current max size is too big
        IOGRMemLayerFeatureIterator* poIter = GetIterator();
        try
        {
            OGRFeature* poFeatureIter;
            while( (poFeatureIter = poIter->Next()) != NULL )
            {
                oMapFeatures[poFeatureIter->GetFID()] = poFeatureIter;
            }
            delete poIter;
            CPLFree(papoFeatures);
            papoFeatures = NULL;
            nMaxFeatureCount = 0;
        }
        catch( const std::bad_alloc& )
        {
            oMapFeatures.clear();
            CPLError(CE_Failure, CPLE_OutOfMemory,
                    "Cannot allocate memory");
            delete poFeatureCloned;
            delete poIter;
            return OGRERR_FAILURE;
        }
    }

    if( papoFeatures != NULL ||
        (oMapFeatures.size() == 0 && nFID <= 100000) )
    {
        if( nFID >= nMaxFeatureCount )
        {
            GIntBig nNewCount = MAX(nMaxFeatureCount+nMaxFeatureCount/3+10, nFID + 1 );
            if( (GIntBig)(size_t)(sizeof(OGRFeature *) * nNewCount) !=
                                    (GIntBig)sizeof(OGRFeature *) * nNewCount )
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                        "Cannot allocate array of " CPL_FRMT_GIB " elements", nNewCount);
                delete poFeatureCloned;
                return OGRERR_FAILURE;
            }

            OGRFeature** papoNewFeatures = (OGRFeature **) 
                VSI_REALLOC_VERBOSE( papoFeatures, (size_t)(sizeof(OGRFeature *) * nNewCount) );
            if (papoNewFeatures == NULL)
            {
                delete poFeatureCloned;
                return OGRERR_FAILURE;
            }
            papoFeatures = papoNewFeatures;
            memset( papoFeatures + nMaxFeatureCount, 0, 
                    sizeof(OGRFeature *) * (size_t)(nNewCount - nMaxFeatureCount) );
            nMaxFeatureCount = nNewCount;
        }

        if( papoFeatures[nFID] != NULL )
        {
            delete papoFeatures[nFID];
            papoFeatures[nFID] = NULL;
        }
        else
            nFeatureCount++;

        papoFeatures[nFID] = poFeatureCloned;

    }
    else
    {
        FeatureIterator oIter = oMapFeatures.find(nFID);
        if( oIter != oMapFeatures.end() )
        {
            delete oIter->second;
            oIter->second = poFeatureCloned;
        }
        else
        {
            try
            {
                oMapFeatures[nFID] = poFeatureCloned;
                nFeatureCount++;
            }
            catch( const std::bad_alloc& )
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                        "Cannot allocate memory");
                delete poFeatureCloned;
                return OGRERR_FAILURE;
            }
        }
    }

    for(int i = 0; i < poFeatureDefn->GetGeomFieldCount(); i ++)
    {
        OGRGeometry* poGeom = poFeatureCloned->GetGeomFieldRef(i);
        if( poGeom != NULL && poGeom->getSpatialReference() == NULL )
        {
            poGeom->assignSpatialReference(
                poFeatureDefn->GetGeomFieldDefn(i)->GetSpatialRef());
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRMemLayer::ICreateFeature( OGRFeature *poFeature )

{
    if (!bUpdatable)
        return OGRERR_FAILURE;

    if( poFeature->GetFID() != OGRNullFID &&
        poFeature->GetFID() != iNextCreateFID )
        bHasHoles = TRUE;

    // If the feature has already a FID and that a feature with the same
    // FID is already registered in the layer, then unset our FID
    if( poFeature->GetFID() >= 0 )
    {
        if( papoFeatures != NULL )
        {
            if( poFeature->GetFID() < nMaxFeatureCount &&
                papoFeatures[poFeature->GetFID()] != NULL )
            {
                poFeature->SetFID( OGRNullFID );
            }
        }
        else
        {
            FeatureIterator oIter = oMapFeatures.find(poFeature->GetFID());
            if( oIter != oMapFeatures.end() )
                poFeature->SetFID( OGRNullFID );
        }
    }

    return SetFeature( poFeature );
}

/************************************************************************/
/*                           DeleteFeature()                            */
/************************************************************************/

OGRErr OGRMemLayer::DeleteFeature( GIntBig nFID )

{
    if (!bUpdatable)
        return OGRERR_FAILURE;

    if( nFID < 0 )
    {
        return OGRERR_FAILURE;
    }

    if( papoFeatures != NULL )
    {
        if( nFID >= nMaxFeatureCount || papoFeatures[nFID] == NULL )
        {
            return OGRERR_FAILURE;
        }
        delete papoFeatures[nFID];
        papoFeatures[nFID] = NULL;
    }
    else
    {
        FeatureIterator oIter = oMapFeatures.find(nFID);
        if( oIter == oMapFeatures.end() )
        {
            return OGRERR_FAILURE;
        }
        delete oIter->second;
        oMapFeatures.erase(oIter);
    }

    bHasHoles = TRUE;
    nFeatureCount--;
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
        return OGRLayer::GetFeatureCount( bForce );

    return nFeatureCount;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRMemLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    else if( EQUAL(pszCap,OLCSequentialWrite)
             || EQUAL(pszCap,OLCRandomWrite) )
        return bUpdatable;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return m_poFilterGeom == NULL && m_poAttrQuery == NULL;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return FALSE;

    else if( EQUAL(pszCap,OLCDeleteFeature) )
        return bUpdatable;

    else if( EQUAL(pszCap,OLCCreateField) ||
             EQUAL(pszCap,OLCCreateGeomField) ||
             EQUAL(pszCap,OLCDeleteField) ||
             EQUAL(pszCap,OLCReorderFields) ||
             EQUAL(pszCap,OLCAlterFieldDefn) )
        return bUpdatable;

    else if( EQUAL(pszCap,OLCFastSetNextByIndex) )
        return m_poFilterGeom == NULL && m_poAttrQuery == NULL &&
               ((papoFeatures != NULL && !bHasHoles) || oMapFeatures.size() == 0);

    else if( EQUAL(pszCap,OLCStringsAsUTF8) )
        return bAdvertizeUTF8;

    else if( EQUAL(pszCap,OLCCurveGeometries) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRMemLayer::CreateField( OGRFieldDefn *poField,
                                 CPL_UNUSED int bApproxOK )
{
    if (!bUpdatable)
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      simple case, no features exist yet.                             */
/* -------------------------------------------------------------------- */
    if( nFeatureCount == 0 )
    {
        poFeatureDefn->AddFieldDefn( poField );
        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Add field definition and setup remap definition.                */
/* -------------------------------------------------------------------- */
    poFeatureDefn->AddFieldDefn( poField );

    int *panRemap = (int *) CPLMalloc(sizeof(int) * poFeatureDefn->GetFieldCount());
    for( GIntBig i = 0; i < poFeatureDefn->GetFieldCount(); ++i )
    {
        if( i < poFeatureDefn->GetFieldCount() - 1 )
            panRemap[i] = (int)i;
        else
            panRemap[i] = -1;
    }

/* -------------------------------------------------------------------- */
/*      Remap all the internal features.  Hopefully there aren't any    */
/*      external features referring to our OGRFeatureDefn!              */
/* -------------------------------------------------------------------- */
    IOGRMemLayerFeatureIterator* poIter = GetIterator();
    OGRFeature* poFeature;
    while( (poFeature = poIter->Next()) != NULL )
    {
        poFeature->RemapFields( NULL, panRemap );
    }
    delete poIter;

    CPLFree( panRemap );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            DeleteField()                             */
/************************************************************************/

OGRErr OGRMemLayer::DeleteField( int iField )
{
    if (!bUpdatable)
        return OGRERR_FAILURE;

    if (iField < 0 || iField >= poFeatureDefn->GetFieldCount())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid field index");
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Update all the internal features.  Hopefully there aren't any   */
/*      external features referring to our OGRFeatureDefn!              */
/* -------------------------------------------------------------------- */
    IOGRMemLayerFeatureIterator* poIter = GetIterator();
    OGRFeature* poFeature;
    while( (poFeature = poIter->Next()) != NULL )
    {
        OGRField* poFieldRaw = poFeature->GetRawFieldRef(iField);
        if( poFeature->IsFieldSet(iField) )
        {
            /* Little trick to unallocate the field */
            OGRField sField;
            sField.Set.nMarker1 = OGRUnsetMarker;
            sField.Set.nMarker2 = OGRUnsetMarker;
            poFeature->SetField(iField, &sField);
        }

        if (iField < poFeatureDefn->GetFieldCount() - 1)
        {
            memmove( poFieldRaw, poFieldRaw + 1,
                     sizeof(OGRField) * (poFeatureDefn->GetFieldCount() - 1 - iField) );
        }
    }
    delete poIter;

    return poFeatureDefn->DeleteFieldDefn( iField );
}

/************************************************************************/
/*                           ReorderFields()                            */
/************************************************************************/

OGRErr OGRMemLayer::ReorderFields( int* panMap )
{
    if (!bUpdatable)
        return OGRERR_FAILURE;

    if (poFeatureDefn->GetFieldCount() == 0)
        return OGRERR_NONE;

    OGRErr eErr = OGRCheckPermutation(panMap, poFeatureDefn->GetFieldCount());
    if (eErr != OGRERR_NONE)
        return eErr;

/* -------------------------------------------------------------------- */
/*      Remap all the internal features.  Hopefully there aren't any    */
/*      external features referring to our OGRFeatureDefn!              */
/* -------------------------------------------------------------------- */
    IOGRMemLayerFeatureIterator* poIter = GetIterator();
    OGRFeature* poFeature;
    while( (poFeature = poIter->Next()) != NULL )
    {
        poFeature->RemapFields( NULL, panMap );
    }
    delete poIter;

    return poFeatureDefn->ReorderFieldDefns( panMap );
}

/************************************************************************/
/*                           AlterFieldDefn()                           */
/************************************************************************/

OGRErr OGRMemLayer::AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlags )
{
    if (!bUpdatable)
        return OGRERR_FAILURE;

    if (iField < 0 || iField >= poFeatureDefn->GetFieldCount())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid field index");
        return OGRERR_FAILURE;
    }

    OGRFieldDefn* poFieldDefn = poFeatureDefn->GetFieldDefn(iField);

    if ((nFlags & ALTER_TYPE_FLAG) &&
        poFieldDefn->GetType() != poNewFieldDefn->GetType())
    {
        if ((poNewFieldDefn->GetType() == OFTDate ||
             poNewFieldDefn->GetType() == OFTTime ||
             poNewFieldDefn->GetType() == OFTDateTime) &&
            (poFieldDefn->GetType() == OFTDate ||
             poFieldDefn->GetType() == OFTTime ||
             poFieldDefn->GetType() == OFTDateTime))
        {
            /* do nothing on features */
        }
        else if (poNewFieldDefn->GetType() == OFTInteger64 &&
                 poFieldDefn->GetType() == OFTInteger)
        {
    /* -------------------------------------------------------------------- */
    /*      Update all the internal features.  Hopefully there aren't any   */
    /*      external features referring to our OGRFeatureDefn!              */
    /* -------------------------------------------------------------------- */
            IOGRMemLayerFeatureIterator* poIter = GetIterator();
            OGRFeature* poFeature;
            while( (poFeature = poIter->Next()) != NULL )
            {
                OGRField* poFieldRaw = poFeature->GetRawFieldRef(iField);
                if( poFeature->IsFieldSet(iField) )
                {
                    poFieldRaw->Integer64 = poFieldRaw->Integer;
                }
            }
            delete poIter;
        }
        else if (poNewFieldDefn->GetType() == OFTReal &&
                 poFieldDefn->GetType() == OFTInteger)
        {
    /* -------------------------------------------------------------------- */
    /*      Update all the internal features.  Hopefully there aren't any   */
    /*      external features referring to our OGRFeatureDefn!              */
    /* -------------------------------------------------------------------- */
            IOGRMemLayerFeatureIterator* poIter = GetIterator();
            OGRFeature* poFeature;
            while( (poFeature = poIter->Next()) != NULL )
            {
                OGRField* poFieldRaw = poFeature->GetRawFieldRef(iField);
                if( poFeature->IsFieldSet(iField) )
                {
                    poFieldRaw->Real = poFieldRaw->Integer;
                }
            }
            delete poIter;
        }
        else if (poNewFieldDefn->GetType() == OFTReal &&
                 poFieldDefn->GetType() == OFTInteger64)
        {
    /* -------------------------------------------------------------------- */
    /*      Update all the internal features.  Hopefully there aren't any   */
    /*      external features referring to our OGRFeatureDefn!              */
    /* -------------------------------------------------------------------- */
            IOGRMemLayerFeatureIterator* poIter = GetIterator();
            OGRFeature* poFeature;
            while( (poFeature = poIter->Next()) != NULL )
            {
                OGRField* poFieldRaw = poFeature->GetRawFieldRef(iField);
                if( poFeature->IsFieldSet(iField) )
                {
                    poFieldRaw->Real = (double) poFieldRaw->Integer64;
                }
            }
            delete poIter;
        }
        else
        {
            if (poNewFieldDefn->GetType() != OFTString)
            {
                CPLError( CE_Failure, CPLE_NotSupported,
                        "Can only convert from OFTInteger to OFTReal, or from anything to OFTString");
                return OGRERR_FAILURE;
            }

    /* -------------------------------------------------------------------- */
    /*      Update all the internal features.  Hopefully there aren't any   */
    /*      external features referring to our OGRFeatureDefn!              */
    /* -------------------------------------------------------------------- */
            IOGRMemLayerFeatureIterator* poIter = GetIterator();
            OGRFeature* poFeature;
            while( (poFeature = poIter->Next()) != NULL )
            {
                OGRField* poFieldRaw = poFeature->GetRawFieldRef(iField);
                if( poFeature->IsFieldSet(iField) )
                {
                    char* pszVal = CPLStrdup(poFeature->GetFieldAsString(iField));

                    /* Little trick to unallocate the field */
                    OGRField sField;
                    sField.Set.nMarker1 = OGRUnsetMarker;
                    sField.Set.nMarker2 = OGRUnsetMarker;
                    poFeature->SetField(iField, &sField);

                    poFieldRaw->String = pszVal;
                }
            }
            delete poIter;
        }

        poFieldDefn->SetType(poNewFieldDefn->GetType());
    }

    if (nFlags & ALTER_NAME_FLAG)
        poFieldDefn->SetName(poNewFieldDefn->GetNameRef());
    if (nFlags & ALTER_WIDTH_PRECISION_FLAG)
    {
        poFieldDefn->SetWidth(poNewFieldDefn->GetWidth());
        poFieldDefn->SetPrecision(poNewFieldDefn->GetPrecision());
    }

    return OGRERR_NONE;
}


/************************************************************************/
/*                          CreateGeomField()                           */
/************************************************************************/

OGRErr OGRMemLayer::CreateGeomField( OGRGeomFieldDefn *poGeomField,
                                     CPL_UNUSED int bApproxOK )
{
    if (!bUpdatable)
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      simple case, no features exist yet.                             */
/* -------------------------------------------------------------------- */
    if( nFeatureCount == 0 )
    {
        poFeatureDefn->AddGeomFieldDefn( poGeomField );
        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Add field definition and setup remap definition.                */
/* -------------------------------------------------------------------- */
    poFeatureDefn->AddGeomFieldDefn( poGeomField );

    int *panRemap = (int *) CPLMalloc(sizeof(int) * poFeatureDefn->GetGeomFieldCount());
    for( GIntBig i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
    {
        if( i < poFeatureDefn->GetGeomFieldCount() - 1 )
            panRemap[i] = (int) i;
        else
            panRemap[i] = -1;
    }

/* -------------------------------------------------------------------- */
/*      Remap all the internal features.  Hopefully there aren't any    */
/*      external features referring to our OGRFeatureDefn!              */
/* -------------------------------------------------------------------- */
    IOGRMemLayerFeatureIterator* poIter = GetIterator();
    OGRFeature* poFeature;
    while( (poFeature = poIter->Next()) != NULL )
    {
        poFeature->RemapGeomFields( NULL, panRemap );
    }
    delete poIter;

    CPLFree( panRemap );

    return OGRERR_NONE;
}

/************************************************************************/
/*                        OGRMemLayerIteratorArray                      */
/************************************************************************/

class OGRMemLayerIteratorArray: public IOGRMemLayerFeatureIterator
{
            GIntBig      m_iCurIdx;
            GIntBig      m_nMaxFeatureCount;
            OGRFeature **m_papoFeatures;

    public:
        OGRMemLayerIteratorArray(GIntBig nMaxFeatureCount,
                                 OGRFeature **papoFeatures):
            m_iCurIdx(0), m_nMaxFeatureCount(nMaxFeatureCount),
            m_papoFeatures(papoFeatures)
        {
        }

       ~OGRMemLayerIteratorArray()
       {
       }

       virtual OGRFeature* Next()
       {
           while( m_iCurIdx < m_nMaxFeatureCount )
           {
               OGRFeature* poFeature = m_papoFeatures[m_iCurIdx];
               m_iCurIdx ++;
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
    typedef std::map<GIntBig, OGRFeature*>           FeatureMap;
    typedef std::map<GIntBig, OGRFeature*>::iterator FeatureIterator;

            FeatureMap&          m_oMapFeatures;
            FeatureIterator      m_oIter;

    public:
        OGRMemLayerIteratorMap(FeatureMap& oMapFeatures):
            m_oMapFeatures(oMapFeatures),
            m_oIter(oMapFeatures.begin())
        {
        }

       ~OGRMemLayerIteratorMap()
       {
       }

       virtual OGRFeature* Next()
       {
           if( m_oIter != m_oMapFeatures.end() )
           {
               OGRFeature* poFeature = m_oIter->second;
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

IOGRMemLayerFeatureIterator* OGRMemLayer::GetIterator()
{
    if( oMapFeatures.size() == 0 )
        return new OGRMemLayerIteratorArray(nMaxFeatureCount, papoFeatures);
    else
        return new OGRMemLayerIteratorMap(oMapFeatures);
}
