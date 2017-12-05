/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRUnionLayer class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2012-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef DOXYGEN_SKIP

#include "ogrunionlayer.h"
#include "ogrwarpedlayer.h"
#include "ogr_p.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                      OGRUnionLayerGeomFieldDefn()                    */
/************************************************************************/

OGRUnionLayerGeomFieldDefn::OGRUnionLayerGeomFieldDefn(
    const char* pszNameIn,
    OGRwkbGeometryType eType) :
    OGRGeomFieldDefn(pszNameIn, eType),
    bGeomTypeSet(FALSE),
    bSRSSet(FALSE)
{}

/************************************************************************/
/*                      OGRUnionLayerGeomFieldDefn()                    */
/************************************************************************/

OGRUnionLayerGeomFieldDefn::OGRUnionLayerGeomFieldDefn(
    OGRGeomFieldDefn* poSrc) :
    OGRGeomFieldDefn(poSrc->GetNameRef(), poSrc->GetType()),
    bGeomTypeSet(FALSE),
    bSRSSet(FALSE)
{
    SetSpatialRef(poSrc->GetSpatialRef());
}

/************************************************************************/
/*                      OGRUnionLayerGeomFieldDefn()                    */
/************************************************************************/

OGRUnionLayerGeomFieldDefn::OGRUnionLayerGeomFieldDefn(
    OGRUnionLayerGeomFieldDefn* poSrc) :
    OGRGeomFieldDefn(poSrc->GetNameRef(), poSrc->GetType()),
    bGeomTypeSet(poSrc->bGeomTypeSet),
    bSRSSet(poSrc->bSRSSet)
{
    SetSpatialRef(poSrc->GetSpatialRef());
    sStaticEnvelope = poSrc->sStaticEnvelope;
}

/************************************************************************/
/*                     ~OGRUnionLayerGeomFieldDefn()                    */
/************************************************************************/

OGRUnionLayerGeomFieldDefn::~OGRUnionLayerGeomFieldDefn() {}

/************************************************************************/
/*                          OGRUnionLayer()                             */
/************************************************************************/

OGRUnionLayer::OGRUnionLayer( const char* pszName,
                              int nSrcLayersIn,
                              OGRLayer** papoSrcLayersIn,
                              int bTakeLayerOwnership ) :
    osName(pszName),
    nSrcLayers(nSrcLayersIn),
    papoSrcLayers(papoSrcLayersIn),
    bHasLayerOwnership(bTakeLayerOwnership),
    poFeatureDefn(NULL),
    nFields(0),
    papoFields(NULL),
    nGeomFields(0),
    papoGeomFields(NULL),
    eFieldStrategy(FIELD_UNION_ALL_LAYERS),
    bPreserveSrcFID(FALSE),
    nFeatureCount(-1),
    iCurLayer(-1),
    pszAttributeFilter(NULL),
    nNextFID(0),
    panMap(NULL),
    papszIgnoredFields(NULL),
    bAttrFilterPassThroughValue(-1),
    pabModifiedLayers(static_cast<int*>(CPLCalloc(sizeof(int), nSrcLayers))),
    pabCheckIfAutoWrap(static_cast<int*>(CPLCalloc(sizeof(int), nSrcLayers))),
    poGlobalSRS(NULL)
{
    CPLAssert(nSrcLayersIn > 0);

    SetDescription( pszName );
}

/************************************************************************/
/*                         ~OGRUnionLayer()                             */
/************************************************************************/

OGRUnionLayer::~OGRUnionLayer()
{
    if( bHasLayerOwnership )
    {
        for(int i = 0; i < nSrcLayers; i++)
            delete papoSrcLayers[i];
    }
    CPLFree(papoSrcLayers);

    for(int i = 0; i < nFields; i++)
        delete papoFields[i];
    CPLFree(papoFields);
    for(int i = 0; i < nGeomFields; i++)
        delete papoGeomFields[i];
    CPLFree(papoGeomFields);

    CPLFree(pszAttributeFilter);
    CPLFree(panMap);
    CSLDestroy(papszIgnoredFields);
    CPLFree(pabModifiedLayers);
    CPLFree(pabCheckIfAutoWrap);

    if( poFeatureDefn )
        poFeatureDefn->Release();
    if( poGlobalSRS != NULL )
        poGlobalSRS->Release();
}

/************************************************************************/
/*                              SetFields()                             */
/************************************************************************/

void OGRUnionLayer::SetFields(FieldUnionStrategy eFieldStrategyIn,
                              int nFieldsIn,
                              OGRFieldDefn** papoFieldsIn,
                              int nGeomFieldsIn,
                              OGRUnionLayerGeomFieldDefn** papoGeomFieldsIn)
{
    CPLAssert(nFields == 0);
    CPLAssert(poFeatureDefn == NULL);

    eFieldStrategy = eFieldStrategyIn;
    if( nFieldsIn )
    {
        nFields = nFieldsIn;
        papoFields = (OGRFieldDefn** )CPLMalloc(nFields * sizeof(OGRFieldDefn*));
        for(int i=0;i<nFields;i++)
            papoFields[i] = new OGRFieldDefn(papoFieldsIn[i]);
    }
    nGeomFields = nGeomFieldsIn;
    if( nGeomFields > 0 )
    {
        papoGeomFields = (OGRUnionLayerGeomFieldDefn** )CPLMalloc(
                            nGeomFields * sizeof(OGRUnionLayerGeomFieldDefn*));
        for(int i=0;i<nGeomFields;i++)
            papoGeomFields[i] = new OGRUnionLayerGeomFieldDefn(papoGeomFieldsIn[i]);
    }
}

/************************************************************************/
/*                        SetSourceLayerFieldName()                     */
/************************************************************************/

void OGRUnionLayer::SetSourceLayerFieldName(const char* pszSourceLayerFieldName)
{
    CPLAssert(poFeatureDefn == NULL);

    CPLAssert(osSourceLayerFieldName.empty());
    if( pszSourceLayerFieldName != NULL )
        osSourceLayerFieldName = pszSourceLayerFieldName;
}

/************************************************************************/
/*                           SetPreserveSrcFID()                        */
/************************************************************************/

void OGRUnionLayer::SetPreserveSrcFID(int bPreserveSrcFIDIn)
{
    CPLAssert(poFeatureDefn == NULL);

    bPreserveSrcFID = bPreserveSrcFIDIn;
}

/************************************************************************/
/*                          SetFeatureCount()                           */
/************************************************************************/

void OGRUnionLayer::SetFeatureCount(int nFeatureCountIn)
{
    CPLAssert(poFeatureDefn == NULL);

    nFeatureCount = nFeatureCountIn;
}

/************************************************************************/
/*                         MergeFieldDefn()                             */
/************************************************************************/

static void MergeFieldDefn(OGRFieldDefn* poFieldDefn,
                           OGRFieldDefn* poSrcFieldDefn)
{
    if( poFieldDefn->GetType() != poSrcFieldDefn->GetType() )
    {
        if( poSrcFieldDefn->GetType() == OFTReal &&
            (poFieldDefn->GetType() == OFTInteger ||
             poFieldDefn->GetType() == OFTInteger64) )
            poFieldDefn->SetType(OFTReal);
        if( poFieldDefn->GetType() == OFTReal &&
            (poSrcFieldDefn->GetType() == OFTInteger ||
             poSrcFieldDefn->GetType() == OFTInteger64) )
            poFieldDefn->SetType(OFTReal);
        else if( poSrcFieldDefn->GetType() == OFTInteger64 &&
                 poFieldDefn->GetType() == OFTInteger)
            poFieldDefn->SetType(OFTInteger64);
        else if( poFieldDefn->GetType() == OFTInteger64 &&
                 poSrcFieldDefn->GetType() == OFTInteger)
            poFieldDefn->SetType(OFTInteger64);
        else
            poFieldDefn->SetType(OFTString);
    }

    if( poFieldDefn->GetWidth() != poSrcFieldDefn->GetWidth() ||
        poFieldDefn->GetPrecision() != poSrcFieldDefn->GetPrecision() )
    {
        poFieldDefn->SetWidth(0);
        poFieldDefn->SetPrecision(0);
    }
}

/************************************************************************/
/*                             GetLayerDefn()                           */
/************************************************************************/

OGRFeatureDefn *OGRUnionLayer::GetLayerDefn()
{
    if( poFeatureDefn != NULL )
        return poFeatureDefn;

    poFeatureDefn = new OGRFeatureDefn( osName );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbNone);

    int iCompareFirstIndex = 0;
    if( !osSourceLayerFieldName.empty() )
    {
        OGRFieldDefn oField(osSourceLayerFieldName, OFTString);
        poFeatureDefn->AddFieldDefn(&oField);
        iCompareFirstIndex = 1;
    }

    if( eFieldStrategy == FIELD_SPECIFIED )
    {
        for( int i = 0; i < nFields; i++ )
            poFeatureDefn->AddFieldDefn(papoFields[i]);
        for( int i = 0; i < nGeomFields; i++ )
        {
            poFeatureDefn->AddGeomFieldDefn(
                new OGRUnionLayerGeomFieldDefn(papoGeomFields[i]), FALSE);
            OGRUnionLayerGeomFieldDefn* poGeomFieldDefn =
                (OGRUnionLayerGeomFieldDefn* ) poFeatureDefn->GetGeomFieldDefn(i);

            if( poGeomFieldDefn->bGeomTypeSet == FALSE ||
                poGeomFieldDefn->bSRSSet == FALSE )
            {
                for( int iLayer = 0; iLayer < nSrcLayers; iLayer++ )
                {
                    OGRFeatureDefn* poSrcFeatureDefn =
                                papoSrcLayers[iLayer]->GetLayerDefn();
                    int nIndex =
                        poSrcFeatureDefn->GetGeomFieldIndex(poGeomFieldDefn->GetNameRef());
                    if( nIndex >= 0 )
                    {
                        OGRGeomFieldDefn* poSrcGeomFieldDefn =
                            poSrcFeatureDefn->GetGeomFieldDefn(nIndex);
                        if( poGeomFieldDefn->bGeomTypeSet == FALSE )
                        {
                            poGeomFieldDefn->bGeomTypeSet = TRUE;
                            poGeomFieldDefn->SetType(poSrcGeomFieldDefn->GetType());
                        }
                        if( poGeomFieldDefn->bSRSSet == FALSE )
                        {
                            poGeomFieldDefn->bSRSSet = TRUE;
                            poGeomFieldDefn->SetSpatialRef(poSrcGeomFieldDefn->GetSpatialRef());
                            if( i == 0 && poGlobalSRS == NULL )
                            {
                                poGlobalSRS = poSrcGeomFieldDefn->GetSpatialRef();
                                if( poGlobalSRS != NULL )
                                    poGlobalSRS->Reference();
                            }
                        }
                        break;
                    }
                }
            }
        }
    }
    else if( eFieldStrategy == FIELD_FROM_FIRST_LAYER )
    {
        OGRFeatureDefn* poSrcFeatureDefn = papoSrcLayers[0]->GetLayerDefn();
        for( int i = 0; i < poSrcFeatureDefn->GetFieldCount(); i++)
            poFeatureDefn->AddFieldDefn(poSrcFeatureDefn->GetFieldDefn(i));
        for( int i = 0;
             nGeomFields != - 1 && i < poSrcFeatureDefn->GetGeomFieldCount();
             i++ )
        {
            OGRGeomFieldDefn* poFldDefn = poSrcFeatureDefn->GetGeomFieldDefn(i);
            poFeatureDefn->AddGeomFieldDefn(
                new OGRUnionLayerGeomFieldDefn(poFldDefn), FALSE);
        }
    }
    else if (eFieldStrategy == FIELD_UNION_ALL_LAYERS )
    {
        if( nGeomFields == 1 )
        {
            poFeatureDefn->AddGeomFieldDefn(
                        new OGRUnionLayerGeomFieldDefn(papoGeomFields[0]), FALSE);
        }

        for(int iLayer = 0; iLayer < nSrcLayers; iLayer++)
        {
            OGRFeatureDefn* poSrcFeatureDefn =
                                papoSrcLayers[iLayer]->GetLayerDefn();

            /* Add any field that is found in the source layers */
            for( int i = 0; i < poSrcFeatureDefn->GetFieldCount(); i++ )
            {
                OGRFieldDefn* poSrcFieldDefn = poSrcFeatureDefn->GetFieldDefn(i);
                int nIndex =
                    poFeatureDefn->GetFieldIndex(poSrcFieldDefn->GetNameRef());
                if( nIndex < 0 )
                    poFeatureDefn->AddFieldDefn(poSrcFieldDefn);
                else
                {
                    OGRFieldDefn* poFieldDefn =
                                        poFeatureDefn->GetFieldDefn(nIndex);
                    MergeFieldDefn(poFieldDefn, poSrcFieldDefn);
                }
            }

            for( int i = 0;
                 nGeomFields != - 1 &&
                 i < poSrcFeatureDefn->GetGeomFieldCount();
                 i++)
            {
                OGRGeomFieldDefn* poSrcFieldDefn = poSrcFeatureDefn->GetGeomFieldDefn(i);
                int nIndex =
                    poFeatureDefn->GetGeomFieldIndex(poSrcFieldDefn->GetNameRef());
                if( nIndex < 0 )
                {
                    poFeatureDefn->AddGeomFieldDefn(
                        new OGRUnionLayerGeomFieldDefn(poSrcFieldDefn), FALSE);
                    if( poFeatureDefn->GetGeomFieldCount() == 1 && nGeomFields == 0 &&
                        GetSpatialRef() != NULL )
                    {
                        OGRUnionLayerGeomFieldDefn* poGeomFieldDefn =
                            (OGRUnionLayerGeomFieldDefn* ) poFeatureDefn->GetGeomFieldDefn(0);
                        poGeomFieldDefn->bSRSSet = TRUE;
                        poGeomFieldDefn->SetSpatialRef(GetSpatialRef());
                    }
                }
                else
                {
                    if( nIndex == 0 && nGeomFields == 1 )
                    {
                        OGRUnionLayerGeomFieldDefn* poGeomFieldDefn =
                            (OGRUnionLayerGeomFieldDefn* ) poFeatureDefn->GetGeomFieldDefn(0);
                        if( poGeomFieldDefn->bGeomTypeSet == FALSE )
                        {
                            poGeomFieldDefn->bGeomTypeSet = TRUE;
                            poGeomFieldDefn->SetType(poSrcFieldDefn->GetType());
                        }
                        if( poGeomFieldDefn->bSRSSet == FALSE )
                        {
                            poGeomFieldDefn->bSRSSet = TRUE;
                            poGeomFieldDefn->SetSpatialRef(poSrcFieldDefn->GetSpatialRef());
                        }
                    }
                    /* TODO: merge type, SRS, extent ? */
                }
            }
        }
    }
    else if (eFieldStrategy == FIELD_INTERSECTION_ALL_LAYERS )
    {
        OGRFeatureDefn* poSrcFeatureDefn = papoSrcLayers[0]->GetLayerDefn();
        for( int i = 0; i < poSrcFeatureDefn->GetFieldCount(); i++ )
            poFeatureDefn->AddFieldDefn(poSrcFeatureDefn->GetFieldDefn(i));
        for( int i = 0; i < poSrcFeatureDefn->GetGeomFieldCount(); i++ )
        {
            OGRGeomFieldDefn* poFldDefn = poSrcFeatureDefn->GetGeomFieldDefn(i);
            poFeatureDefn->AddGeomFieldDefn(
                new OGRUnionLayerGeomFieldDefn(poFldDefn), FALSE);
        }

        /* Remove any field that is not found in the source layers */
        for( int iLayer = 1; iLayer < nSrcLayers; iLayer++ )
        {
            OGRFeatureDefn* l_poSrcFeatureDefn =
                                        papoSrcLayers[iLayer]->GetLayerDefn();
            for( int i = iCompareFirstIndex;
                 i < poFeatureDefn->GetFieldCount();
                 // No increment.
                 )
            {
                OGRFieldDefn* poFieldDefn = poFeatureDefn->GetFieldDefn(i);
                int nSrcIndex = l_poSrcFeatureDefn->GetFieldIndex(
                                                    poFieldDefn->GetNameRef());
                if( nSrcIndex < 0 )
                {
                    poFeatureDefn->DeleteFieldDefn(i);
                }
                else
                {
                    OGRFieldDefn* poSrcFieldDefn =
                        l_poSrcFeatureDefn->GetFieldDefn(nSrcIndex);
                    MergeFieldDefn(poFieldDefn, poSrcFieldDefn);

                    i++;
                }
            }
            for( int i = 0;
                 i < poFeatureDefn->GetGeomFieldCount();
                 // No increment.
                 )
            {
                OGRGeomFieldDefn* poFieldDefn = poFeatureDefn->GetGeomFieldDefn(i);
                int nSrcIndex = l_poSrcFeatureDefn->GetGeomFieldIndex(
                                                    poFieldDefn->GetNameRef());
                if( nSrcIndex < 0 )
                {
                    poFeatureDefn->DeleteGeomFieldDefn(i);
                }
                else
                {
                    /* TODO: merge type, SRS, extent ? */

                    i++;
                }
            }
        }
    }

    return poFeatureDefn;
}

/************************************************************************/
/*                             GetGeomType()                            */
/************************************************************************/

OGRwkbGeometryType OGRUnionLayer::GetGeomType()
{
    if( nGeomFields < 0 )
        return wkbNone;
    if( nGeomFields >= 1 && papoGeomFields[0]->bGeomTypeSet )
    {
        return papoGeomFields[0]->GetType();
    }

    return OGRLayer::GetGeomType();
}

/************************************************************************/
/*                    SetSpatialFilterToSourceLayer()                   */
/************************************************************************/

void OGRUnionLayer::SetSpatialFilterToSourceLayer(OGRLayer* poSrcLayer)
{
    if( m_iGeomFieldFilter >= 0 &&
        m_iGeomFieldFilter < GetLayerDefn()->GetGeomFieldCount() )
    {
        int iSrcGeomField = poSrcLayer->GetLayerDefn()->GetGeomFieldIndex(
            GetLayerDefn()->GetGeomFieldDefn(m_iGeomFieldFilter)->GetNameRef());
        if( iSrcGeomField >= 0 )
        {
            poSrcLayer->SetSpatialFilter(iSrcGeomField, m_poFilterGeom);
        }
        else
        {
            poSrcLayer->SetSpatialFilter(NULL);
        }
    }
    else
    {
        poSrcLayer->SetSpatialFilter(NULL);
    }
}

/************************************************************************/
/*                        ConfigureActiveLayer()                        */
/************************************************************************/

void OGRUnionLayer::ConfigureActiveLayer()
{
    AutoWarpLayerIfNecessary(iCurLayer);
    ApplyAttributeFilterToSrcLayer(iCurLayer);
    SetSpatialFilterToSourceLayer(papoSrcLayers[iCurLayer]);
    papoSrcLayers[iCurLayer]->ResetReading();

    /* Establish map */
    GetLayerDefn();
    OGRFeatureDefn* poSrcFeatureDefn = papoSrcLayers[iCurLayer]->GetLayerDefn();
    CPLFree(panMap);
    panMap = (int*) CPLMalloc(poSrcFeatureDefn->GetFieldCount() * sizeof(int));
    for(int i=0; i < poSrcFeatureDefn->GetFieldCount(); i++)
    {
        OGRFieldDefn* poSrcFieldDefn = poSrcFeatureDefn->GetFieldDefn(i);
        if( CSLFindString(papszIgnoredFields,
                          poSrcFieldDefn->GetNameRef() ) == -1 )
        {
            panMap[i] =
                poFeatureDefn->GetFieldIndex(poSrcFieldDefn->GetNameRef());
        }
        else
        {
            panMap[i] = -1;
        }
    }

    if( papoSrcLayers[iCurLayer]->TestCapability(OLCIgnoreFields) )
    {
        char** papszIter = papszIgnoredFields;
        char** papszFieldsSrc = NULL;
        while ( papszIter != NULL && *papszIter != NULL )
        {
            const char* pszFieldName = *papszIter;
            if ( EQUAL(pszFieldName, "OGR_GEOMETRY") ||
                 EQUAL(pszFieldName, "OGR_STYLE") ||
                 poSrcFeatureDefn->GetFieldIndex(pszFieldName) >= 0 ||
                 poSrcFeatureDefn->GetGeomFieldIndex(pszFieldName) >= 0 )
            {
                papszFieldsSrc = CSLAddString(papszFieldsSrc, pszFieldName);
            }
            papszIter++;
        }

        /* Attribute fields */
        int* panSrcFieldsUsed = (int*) CPLCalloc(sizeof(int),
                                          poSrcFeatureDefn->GetFieldCount());
        for(int iField = 0;
                iField < poFeatureDefn->GetFieldCount(); iField++)
        {
            OGRFieldDefn* poFieldDefn = poFeatureDefn->GetFieldDefn(iField);
            int iSrcField =
                    poSrcFeatureDefn->GetFieldIndex(poFieldDefn->GetNameRef());
            if (iSrcField >= 0)
                panSrcFieldsUsed[iSrcField] = TRUE;
        }
        for(int iSrcField = 0;
                iSrcField < poSrcFeatureDefn->GetFieldCount(); iSrcField ++)
        {
            if( !panSrcFieldsUsed[iSrcField] )
            {
                OGRFieldDefn *poSrcDefn =
                        poSrcFeatureDefn->GetFieldDefn( iSrcField );
                papszFieldsSrc =
                        CSLAddString(papszFieldsSrc, poSrcDefn->GetNameRef());
            }
        }
        CPLFree(panSrcFieldsUsed);

        /* geometry fields now */
        panSrcFieldsUsed = (int*) CPLCalloc(sizeof(int),
                                          poSrcFeatureDefn->GetGeomFieldCount());
        for(int iField = 0;
                iField < poFeatureDefn->GetGeomFieldCount(); iField++)
        {
            OGRGeomFieldDefn* poFieldDefn = poFeatureDefn->GetGeomFieldDefn(iField);
            int iSrcField =
                    poSrcFeatureDefn->GetGeomFieldIndex(poFieldDefn->GetNameRef());
            if (iSrcField >= 0)
                panSrcFieldsUsed[iSrcField] = TRUE;
        }
        for(int iSrcField = 0;
                iSrcField < poSrcFeatureDefn->GetGeomFieldCount(); iSrcField ++)
        {
            if( !panSrcFieldsUsed[iSrcField] )
            {
                OGRGeomFieldDefn *poSrcDefn =
                        poSrcFeatureDefn->GetGeomFieldDefn( iSrcField );
                papszFieldsSrc =
                        CSLAddString(papszFieldsSrc, poSrcDefn->GetNameRef());
            }
        }
        CPLFree(panSrcFieldsUsed);

        papoSrcLayers[iCurLayer]->SetIgnoredFields((const char**)papszFieldsSrc);

        CSLDestroy(papszFieldsSrc);
    }
}

/************************************************************************/
/*                             ResetReading()                           */
/************************************************************************/

void OGRUnionLayer::ResetReading()
{
    iCurLayer = 0;
    ConfigureActiveLayer();
    nNextFID = 0;
}

/************************************************************************/
/*                         AutoWarpLayerIfNecessary()                   */
/************************************************************************/

void OGRUnionLayer::AutoWarpLayerIfNecessary(int iLayer)
{
    if( !pabCheckIfAutoWrap[iLayer] )
    {
        pabCheckIfAutoWrap[iLayer] = TRUE;

        for(int i=0; i<GetLayerDefn()->GetGeomFieldCount();i++)
        {
            OGRSpatialReference* poSRS = GetLayerDefn()->GetGeomFieldDefn(i)->GetSpatialRef();
            if( poSRS != NULL )
                poSRS->Reference();

            OGRFeatureDefn* poSrcFeatureDefn = papoSrcLayers[iLayer]->GetLayerDefn();
            int iSrcGeomField = poSrcFeatureDefn->GetGeomFieldIndex(
                    GetLayerDefn()->GetGeomFieldDefn(i)->GetNameRef());
            if( iSrcGeomField >= 0 )
            {
                OGRSpatialReference* poSRS2 =
                    poSrcFeatureDefn->GetGeomFieldDefn(iSrcGeomField)->GetSpatialRef();

                if( (poSRS == NULL && poSRS2 != NULL) ||
                    (poSRS != NULL && poSRS2 == NULL) )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                            "SRS of geometry field '%s' layer %s not consistent with UnionLayer SRS",
                            GetLayerDefn()->GetGeomFieldDefn(i)->GetNameRef(),
                            papoSrcLayers[iLayer]->GetName());
                }
                else if (poSRS != NULL && poSRS2 != NULL &&
                        poSRS != poSRS2 && !poSRS->IsSame(poSRS2))
                {
                    CPLDebug("VRT", "SRS of geometry field '%s' layer %s not consistent with UnionLayer SRS. "
                            "Trying auto warping",
                            GetLayerDefn()->GetGeomFieldDefn(i)->GetNameRef(),
                            papoSrcLayers[iLayer]->GetName());
                    OGRCoordinateTransformation* poCT =
                        OGRCreateCoordinateTransformation( poSRS2, poSRS );
                    OGRCoordinateTransformation* poReversedCT = (poCT != NULL) ?
                        OGRCreateCoordinateTransformation( poSRS, poSRS2 ) : NULL;
                    if( poReversedCT != NULL )
                        papoSrcLayers[iLayer] = new OGRWarpedLayer(
                                    papoSrcLayers[iLayer], iSrcGeomField, TRUE, poCT, poReversedCT);
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "AutoWarpLayerIfNecessary failed to create "
                                 "poCT or poReversedCT.");
                        if ( poCT != NULL )
                            delete poCT;
                    }
                }
            }

            if( poSRS != NULL )
                poSRS->Release();
        }
    }
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRUnionLayer::GetNextFeature()
{
    if( poFeatureDefn == NULL ) GetLayerDefn();
    if( iCurLayer < 0 )
        ResetReading();

    if( iCurLayer == nSrcLayers )
        return NULL;

    while( true )
    {
        OGRFeature* poSrcFeature = papoSrcLayers[iCurLayer]->GetNextFeature();
        if( poSrcFeature == NULL )
        {
            iCurLayer ++;
            if( iCurLayer < nSrcLayers )
            {
                ConfigureActiveLayer();
                continue;
            }
            else
                break;
        }

        OGRFeature* poFeature = TranslateFromSrcLayer(poSrcFeature);
        delete poSrcFeature;

        if( (m_poFilterGeom == NULL ||
             FilterGeometry( poFeature->GetGeomFieldRef(m_iGeomFieldFilter) ) ) &&
            (m_poAttrQuery == NULL ||
             m_poAttrQuery->Evaluate( poFeature )) )
        {
            return poFeature;
        }

        delete poFeature;
    }
    return NULL;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRUnionLayer::GetFeature( GIntBig nFeatureId )
{
    OGRFeature* poFeature = NULL;

    if( !bPreserveSrcFID )
    {
        poFeature = OGRLayer::GetFeature(nFeatureId);
    }
    else
    {
        int iGeomFieldFilterSave = m_iGeomFieldFilter;
        OGRGeometry* poGeomSave = m_poFilterGeom;
        m_poFilterGeom = NULL;
        SetSpatialFilter(NULL);

        for(int i=0;i<nSrcLayers;i++)
        {
            iCurLayer = i;
            ConfigureActiveLayer();

            OGRFeature* poSrcFeature = papoSrcLayers[i]->GetFeature(nFeatureId);
            if( poSrcFeature != NULL )
            {
                poFeature = TranslateFromSrcLayer(poSrcFeature);
                delete poSrcFeature;

                break;
            }
        }

        SetSpatialFilter(iGeomFieldFilterSave, poGeomSave);
        delete poGeomSave;

        ResetReading();
    }

    return poFeature;
}

/************************************************************************/
/*                          ICreateFeature()                             */
/************************************************************************/

OGRErr OGRUnionLayer::ICreateFeature( OGRFeature* poFeature )
{
    if( osSourceLayerFieldName.empty() )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "CreateFeature() not supported when SourceLayerFieldName is not set");
        return OGRERR_FAILURE;
    }

    if( poFeature->GetFID() != OGRNullFID )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "CreateFeature() not supported when FID is set");
        return OGRERR_FAILURE;
    }

    if( !poFeature->IsFieldSetAndNotNull(0) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "CreateFeature() not supported when '%s' field is not set",
                 osSourceLayerFieldName.c_str());
        return OGRERR_FAILURE;
    }

    const char* pszSrcLayerName = poFeature->GetFieldAsString(0);
    for(int i=0;i<nSrcLayers;i++)
    {
        if( strcmp(pszSrcLayerName, papoSrcLayers[i]->GetName()) == 0)
        {
            pabModifiedLayers[i] = TRUE;

            OGRFeature* poSrcFeature =
                        new OGRFeature(papoSrcLayers[i]->GetLayerDefn());
            poSrcFeature->SetFrom(poFeature, TRUE);
            OGRErr eErr = papoSrcLayers[i]->CreateFeature(poSrcFeature);
            if( eErr == OGRERR_NONE )
                poFeature->SetFID(poSrcFeature->GetFID());
            delete poSrcFeature;
            return eErr;
        }
    }

    CPLError(CE_Failure, CPLE_NotSupported,
             "CreateFeature() not supported : '%s' source layer does not exist",
             pszSrcLayerName);
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                             ISetFeature()                             */
/************************************************************************/

OGRErr OGRUnionLayer::ISetFeature( OGRFeature* poFeature )
{
    if( !bPreserveSrcFID )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetFeature() not supported when PreserveSrcFID is OFF");
        return OGRERR_FAILURE;
    }

    if( osSourceLayerFieldName.empty() )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetFeature() not supported when SourceLayerFieldName is not set");
        return OGRERR_FAILURE;
    }

    if( poFeature->GetFID() == OGRNullFID )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetFeature() not supported when FID is not set");
        return OGRERR_FAILURE;
    }

    if( !poFeature->IsFieldSetAndNotNull(0) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetFeature() not supported when '%s' field is not set",
                 osSourceLayerFieldName.c_str());
        return OGRERR_FAILURE;
    }

    const char* pszSrcLayerName = poFeature->GetFieldAsString(0);
    for(int i=0;i<nSrcLayers;i++)
    {
        if( strcmp(pszSrcLayerName, papoSrcLayers[i]->GetName()) == 0)
        {
            pabModifiedLayers[i] = TRUE;

            OGRFeature* poSrcFeature =
                        new OGRFeature(papoSrcLayers[i]->GetLayerDefn());
            poSrcFeature->SetFrom(poFeature, TRUE);
            poSrcFeature->SetFID(poFeature->GetFID());
            OGRErr eErr = papoSrcLayers[i]->SetFeature(poSrcFeature);
            delete poSrcFeature;
            return eErr;
        }
    }

    CPLError(CE_Failure, CPLE_NotSupported,
             "SetFeature() not supported : '%s' source layer does not exist",
             pszSrcLayerName);
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRUnionLayer::GetSpatialRef()
{
    if( nGeomFields < 0 )
        return NULL;
    if( nGeomFields >= 1 &&
        papoGeomFields[0]->bSRSSet )
        return papoGeomFields[0]->GetSpatialRef();

    if( poGlobalSRS == NULL )
    {
        poGlobalSRS = papoSrcLayers[0]->GetSpatialRef();
        if( poGlobalSRS != NULL )
            poGlobalSRS->Reference();
    }
    return poGlobalSRS;
}

/************************************************************************/
/*                      GetAttrFilterPassThroughValue()                 */
/************************************************************************/

int OGRUnionLayer::GetAttrFilterPassThroughValue()
{
    if( m_poAttrQuery == NULL )
        return TRUE;

    if( bAttrFilterPassThroughValue >= 0)
        return bAttrFilterPassThroughValue;

    char** papszUsedFields = m_poAttrQuery->GetUsedFields();
    int bRet = TRUE;

    for(int iLayer = 0; iLayer < nSrcLayers; iLayer++)
    {
        OGRFeatureDefn* poSrcFeatureDefn =
                                papoSrcLayers[iLayer]->GetLayerDefn();
        char** papszIter = papszUsedFields;
        while( papszIter != NULL && *papszIter != NULL )
        {
            int bIsSpecial = FALSE;
            for(int i = 0; i < SPECIAL_FIELD_COUNT; i++)
            {
                if( EQUAL(*papszIter, SpecialFieldNames[i]) )
                {
                    bIsSpecial = TRUE;
                    break;
                }
            }
            if( !bIsSpecial &&
                poSrcFeatureDefn->GetFieldIndex(*papszIter) < 0 )
            {
                bRet = FALSE;
                break;
            }
            papszIter ++;
        }
    }

    CSLDestroy(papszUsedFields);

    bAttrFilterPassThroughValue = bRet;

    return bRet;
}

/************************************************************************/
/*                  ApplyAttributeFilterToSrcLayer()                    */
/************************************************************************/

void OGRUnionLayer::ApplyAttributeFilterToSrcLayer(int iSubLayer)
{
    CPLAssert(iSubLayer >= 0 && iSubLayer < nSrcLayers);

    if( GetAttrFilterPassThroughValue() )
        papoSrcLayers[iSubLayer]->SetAttributeFilter(pszAttributeFilter);
    else
        papoSrcLayers[iSubLayer]->SetAttributeFilter(NULL);
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRUnionLayer::GetFeatureCount( int bForce )
{
    if (nFeatureCount >= 0 &&
        m_poFilterGeom == NULL && m_poAttrQuery == NULL)
    {
        return nFeatureCount;
    }

    if( !GetAttrFilterPassThroughValue() )
        return OGRLayer::GetFeatureCount(bForce);

    GIntBig nRet = 0;
    for(int i = 0; i < nSrcLayers; i++)
    {
        AutoWarpLayerIfNecessary(i);
        ApplyAttributeFilterToSrcLayer(i);
        SetSpatialFilterToSourceLayer(papoSrcLayers[i]);
        nRet += papoSrcLayers[i]->GetFeatureCount(bForce);
    }
    ResetReading();
    return nRet;
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRUnionLayer::SetAttributeFilter( const char * pszAttributeFilterIn )
{
    if( pszAttributeFilterIn == NULL && pszAttributeFilter == NULL)
        return OGRERR_NONE;
    if( pszAttributeFilterIn != NULL && pszAttributeFilter != NULL &&
        strcmp(pszAttributeFilterIn, pszAttributeFilter) == 0)
        return OGRERR_NONE;

    if( poFeatureDefn == NULL ) GetLayerDefn();

    bAttrFilterPassThroughValue = -1;

    OGRErr eErr = OGRLayer::SetAttributeFilter(pszAttributeFilterIn);
    if( eErr != OGRERR_NONE )
        return eErr;

    CPLFree(pszAttributeFilter);
    pszAttributeFilter = pszAttributeFilterIn ?
                                CPLStrdup(pszAttributeFilterIn) : NULL;

    if( iCurLayer >= 0 && iCurLayer < nSrcLayers)
        ApplyAttributeFilterToSrcLayer(iCurLayer);

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int  OGRUnionLayer::TestCapability( const char * pszCap )
{
    if( EQUAL(pszCap, OLCFastFeatureCount ) )
    {
        if( nFeatureCount >= 0 &&
            m_poFilterGeom == NULL && m_poAttrQuery == NULL )
            return TRUE;

        if( !GetAttrFilterPassThroughValue() )
            return FALSE;

        for(int i = 0; i < nSrcLayers; i++)
        {
            AutoWarpLayerIfNecessary(i);
            ApplyAttributeFilterToSrcLayer(i);
            SetSpatialFilterToSourceLayer(papoSrcLayers[i]);
            if( !papoSrcLayers[i]->TestCapability(pszCap) )
                return FALSE;
        }
        return TRUE;
    }

    if( EQUAL(pszCap, OLCFastGetExtent ) )
    {
        if( nGeomFields >= 1 &&
            papoGeomFields[0]->sStaticEnvelope.IsInit() )
            return TRUE;

        for(int i = 0; i < nSrcLayers; i++)
        {
            AutoWarpLayerIfNecessary(i);
            if( !papoSrcLayers[i]->TestCapability(pszCap) )
                return FALSE;
        }
        return TRUE;
    }

    if( EQUAL(pszCap, OLCFastSpatialFilter) )
    {
        for(int i = 0; i < nSrcLayers; i++)
        {
            AutoWarpLayerIfNecessary(i);
            ApplyAttributeFilterToSrcLayer(i);
            if( !papoSrcLayers[i]->TestCapability(pszCap) )
                return FALSE;
        }
        return TRUE;
    }

    if( EQUAL(pszCap, OLCStringsAsUTF8) )
    {
        for(int i = 0; i < nSrcLayers; i++)
        {
            if( !papoSrcLayers[i]->TestCapability(pszCap) )
                return FALSE;
        }
        return TRUE;
    }

    if( EQUAL(pszCap, OLCRandomRead ) )
    {
        if( !bPreserveSrcFID )
            return FALSE;

        for(int i = 0; i < nSrcLayers; i++)
        {
            if( !papoSrcLayers[i]->TestCapability(pszCap) )
                return FALSE;
        }
        return TRUE;
    }

    if( EQUAL(pszCap, OLCRandomWrite ) )
    {
        if( !bPreserveSrcFID || osSourceLayerFieldName.empty())
            return FALSE;

        for(int i = 0; i < nSrcLayers; i++)
        {
            if( !papoSrcLayers[i]->TestCapability(pszCap) )
                return FALSE;
        }
        return TRUE;
    }

    if( EQUAL(pszCap, OLCSequentialWrite ) )
    {
        if( osSourceLayerFieldName.empty())
            return FALSE;

        for(int i = 0; i < nSrcLayers; i++)
        {
            if( !papoSrcLayers[i]->TestCapability(pszCap) )
                return FALSE;
        }
        return TRUE;
    }

    if( EQUAL(pszCap, OLCIgnoreFields) )
        return TRUE;

    if( EQUAL(pszCap,OLCCurveGeometries) )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                              GetExtent()                             */
/************************************************************************/

OGRErr OGRUnionLayer::GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce)
{
    if( iGeomField >= 0 && iGeomField < nGeomFields &&
        papoGeomFields[iGeomField]->sStaticEnvelope.IsInit() )
    {
        *psExtent = papoGeomFields[iGeomField]->sStaticEnvelope;
        return OGRERR_NONE;
    }

    if( iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid geometry field index : %d", iGeomField);
        return OGRERR_FAILURE;
    }

    int bInit = FALSE;
    for(int i = 0; i < nSrcLayers; i++)
    {
        AutoWarpLayerIfNecessary(i);
        int iSrcGeomField = papoSrcLayers[i]->GetLayerDefn()->GetGeomFieldIndex(
            GetLayerDefn()->GetGeomFieldDefn(iGeomField)->GetNameRef());
        if( iSrcGeomField >= 0 )
        {
            if( !bInit )
            {
                if( papoSrcLayers[i]->GetExtent(iSrcGeomField, psExtent, bForce) == OGRERR_NONE )
                    bInit = TRUE;
            }
            else
            {
                OGREnvelope sExtent;
                if( papoSrcLayers[i]->GetExtent(iSrcGeomField, &sExtent, bForce) == OGRERR_NONE )
                {
                    psExtent->Merge(sExtent);
                }
            }
        }
    }
    return (bInit) ? OGRERR_NONE : OGRERR_FAILURE;
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRUnionLayer::GetExtent( OGREnvelope *psExtent, int bForce )
{
    return GetExtent(0, psExtent, bForce);
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRUnionLayer::SetSpatialFilter( OGRGeometry * poGeomIn )
{
    SetSpatialFilter(0, poGeomIn);
}

/************************************************************************/
/*                         SetSpatialFilter()                           */
/************************************************************************/

void OGRUnionLayer::SetSpatialFilter( int iGeomField, OGRGeometry *poGeom )
{
    if( iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount() )
    {
        if( poGeom != NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                        "Invalid geometry field index : %d", iGeomField);
            return;
        }
    }

    m_iGeomFieldFilter = iGeomField;
    if( InstallFilter( poGeom ) )
        ResetReading();

    if( iCurLayer >= 0 && iCurLayer < nSrcLayers)
    {
        SetSpatialFilterToSourceLayer(papoSrcLayers[iCurLayer]);
    }
}

/************************************************************************/
/*                        TranslateFromSrcLayer()                       */
/************************************************************************/

OGRFeature* OGRUnionLayer::TranslateFromSrcLayer(OGRFeature* poSrcFeature)
{
    CPLAssert(poSrcFeature->GetFieldCount() == 0 || panMap != NULL);
    CPLAssert(iCurLayer >= 0 && iCurLayer < nSrcLayers);

    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    poFeature->SetFrom(poSrcFeature, panMap, TRUE);

    if( !osSourceLayerFieldName.empty() &&
        !poFeatureDefn->GetFieldDefn(0)->IsIgnored() )
    {
        poFeature->SetField(0, papoSrcLayers[iCurLayer]->GetName());
    }

    for(int i=0;i<poFeatureDefn->GetGeomFieldCount();i++)
    {
        if( poFeatureDefn->GetGeomFieldDefn(i)->IsIgnored() )
            poFeature->SetGeomFieldDirectly(i, NULL);
        else
        {
            OGRGeometry* poGeom = poFeature->GetGeomFieldRef(i);
            if( poGeom != NULL )
            {
                poGeom->assignSpatialReference(
                    poFeatureDefn->GetGeomFieldDefn(i)->GetSpatialRef());
            }
        }
    }

    if( bPreserveSrcFID )
        poFeature->SetFID(poSrcFeature->GetFID());
    else
        poFeature->SetFID(nNextFID ++);
    return poFeature;
}

/************************************************************************/
/*                          SetIgnoredFields()                          */
/************************************************************************/

OGRErr OGRUnionLayer::SetIgnoredFields( const char **papszFields )
{
    OGRErr eErr = OGRLayer::SetIgnoredFields(papszFields);
    if( eErr != OGRERR_NONE )
        return eErr;

    CSLDestroy(papszIgnoredFields);
    papszIgnoredFields = papszFields ? CSLDuplicate((char**)papszFields) : NULL;

    return eErr;
}

/************************************************************************/
/*                             SyncToDisk()                             */
/************************************************************************/

OGRErr OGRUnionLayer::SyncToDisk()
{
    for(int i = 0; i < nSrcLayers; i++)
    {
        if (pabModifiedLayers[i])
        {
            papoSrcLayers[i]->SyncToDisk();
            pabModifiedLayers[i] = FALSE;
        }
    }

    return OGRERR_NONE;
}

#endif /* #ifndef DOXYGEN_SKIP */
