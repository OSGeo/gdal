/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRVRTLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2003/11/07 17:50:36  warmerda
 * New
 *
 */

#include "cpl_conv.h"
#include "ogr_vrt.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

typedef struct 
{
    OGRwkbGeometryType  eType;
    const char          *pszName;
} OGRGeomTypeName;

static OGRGeomTypeName asGeomTypeNames[] = { /* 25D versions are implicit */
    { wkbUnknown, "wkbUnknown" },
    { wkbPoint, "wkbPoint" },
    { wkbLineString, "wkbLineString" },
    { wkbPolygon, "wkbPolygon" },
    { wkbMultiPoint, "wkbMultiPoint" },
    { wkbMultiLineString, "wkbLineString" },
    { wkbMultiPolygon, "wkbPolygon" },
    { wkbGeometryCollection, "wkbGeometryCollection" },
    { wkbNone, "wkbNone" },
    { wkbLinearRing, "wkbLinearRing" },
    { wkbNone, NULL }
};

/************************************************************************/
/*                            OGRVRTLayer()                             */
/************************************************************************/

OGRVRTLayer::OGRVRTLayer()

{
    poFeatureDefn = NULL;
    poFilterGeom = NULL;
    pszQuery = NULL;
    poSrcLayer = NULL;
    poSRS = NULL;

    iFIDField = -1;

    eGeometryType = VGS_Direct;
    iGeomField = iGeomXField = iGeomYField = iGeomZField = -1;
    
    panSrcField = NULL;
    pabDirectCopy = NULL;
}

/************************************************************************/
/*                            ~OGRVRTLayer()                            */
/************************************************************************/

OGRVRTLayer::~OGRVRTLayer()

{
    if( poFilterGeom != NULL )
        delete poFilterGeom;

    if( poSRS != NULL )
        poSRS->Dereference();
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

int OGRVRTLayer::Initialize( CPLXMLNode *psLTree )

{
    
    if( !EQUAL(psLTree->pszValue,"OGRVRTLayer") )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Get layer name.                                                 */
/* -------------------------------------------------------------------- */
    const char *pszLayerName = CPLGetXMLValue( psLTree, "name", NULL );

    if( pszLayerName == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Missing name attribute on OGRVRTLayer" );
        return FALSE;
    }

    poFeatureDefn = new OGRFeatureDefn( pszLayerName );

/* -------------------------------------------------------------------- */
/*      Try to access the datasource.                                   */
/* -------------------------------------------------------------------- */
    OGRSFDriverRegistrar *poReg = OGRSFDriverRegistrar::GetRegistrar();
    const char *pszSrcDSName = CPLGetXMLValue( psLTree, "SrcDataSource", NULL);

    if( pszSrcDSName == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Missing SrcDataSource for layer %s.", pszLayerName );
        return FALSE;
    }

    CPLErrorReset();
    poSrcDS = poReg->OpenShared( pszSrcDSName, FALSE, NULL );

    if( poSrcDS == NULL ) 
    {
        if( strlen(CPLGetLastErrorMsg()) == 0 )
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Failed to open datasource `%s'.", 
                      pszSrcDSName );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Find the layer.                                                 */
/* -------------------------------------------------------------------- */
    const char *pszSrcLayerName = CPLGetXMLValue( psLTree, "SrcLayer", 
                                                  pszLayerName );

    poSrcLayer = poSrcDS->GetLayerByName( pszSrcLayerName );
    if( poSrcLayer == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to file layer '%s' on datasource '%s'.", 
                  pszLayerName, pszSrcDSName );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Do we have a fixed geometry type?  If so use it, otherwise      */
/*      derive from the source layer.                                   */
/* -------------------------------------------------------------------- */
     const char *pszGType = CPLGetXMLValue( psLTree, "GeometryType", NULL );
     
     if( pszGType != NULL )
     {
         int iType;

         for( iType = 0; asGeomTypeNames[iType].pszName != NULL; iType++ )
         {
             if( EQUALN(pszGType, asGeomTypeNames[iType].pszName, 
                        strlen(asGeomTypeNames[iType].pszName)) )
             {
                 poFeatureDefn->SetGeomType( asGeomTypeNames[iType].eType );

                 if( strstr(pszGType,"25D") != NULL )
                     poFeatureDefn->SetGeomType( 
                         (OGRwkbGeometryType)
                         (poFeatureDefn->GetGeomType() | wkb25DBit) );
                 break;
             }
         }

         if( asGeomTypeNames[iType].pszName == NULL )
         {
             CPLError( CE_Failure, CPLE_AppDefined, 
                       "GeometryType %s not recognised.", 
                       pszGType );
             return FALSE;
         }
     }

     else
     {
         poFeatureDefn->SetGeomType(poSrcLayer->GetLayerDefn()->GetGeomType());
     }
     
/* -------------------------------------------------------------------- */
/*      For now we copy the schema directly from the source layer.      */
/* -------------------------------------------------------------------- */
     int iField;
     OGRFeatureDefn *poSrcDefn = poSrcLayer->GetLayerDefn();
     panSrcField = (int *) CPLMalloc(sizeof(int) * poSrcDefn->GetFieldCount());
     pabDirectCopy = (int *) CPLMalloc(sizeof(int)*poSrcDefn->GetFieldCount());

     for( iField = 0; iField < poSrcDefn->GetFieldCount(); iField++ )
     {
         poFeatureDefn->AddFieldDefn( poSrcDefn->GetFieldDefn( iField ) );
         panSrcField[iField] = iField;
         pabDirectCopy[iField] = TRUE;
     }
     
/* -------------------------------------------------------------------- */
/*      Apply a spatial reference system if provided, otherwise copy    */
/*      from source.                                                    */
/* -------------------------------------------------------------------- */
     char *pszLayerSRS = (char *) CPLGetXMLValue( psLTree, "LayerSRS", NULL );

     if( pszLayerSRS != NULL )
     {
         if( EQUAL(pszLayerSRS,"NULL") )
             poSRS = NULL;
         else
         {
             OGRSpatialReference oSRS;

             if( oSRS.importFromWkt( &pszLayerSRS ) != OGRERR_NONE )
             {
                 CPLError( CE_Failure, CPLE_AppDefined, 
                           "Failed to import LayerSRS `%s'.", pszLayerSRS );
                 return FALSE;
             }
             poSRS = oSRS.Clone();
         }
     }

     else
     {
         if( poSrcLayer->GetSpatialRef() != NULL )
             poSRS = poSrcLayer->GetSpatialRef()->Clone();
         else
             poSRS = NULL;
     }
                                               
     return TRUE;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRVRTLayer::ResetReading()

{
    if( poSrcLayer != NULL )
        poSrcLayer->ResetReading();
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRVRTLayer::GetNextFeature()

{
    if( poSrcLayer == NULL )
        return NULL;

    for( ; TRUE; )
    {
        OGRFeature      *poSrcFeature, *poFeature;

        poSrcFeature = poSrcLayer->GetNextFeature();
        if( poSrcFeature == NULL )
            return NULL;

        poFeature = TranslateFeature( poSrcFeature );
        delete poSrcFeature;

        if( poFeature == NULL )
            return NULL;

        if( (poFilterGeom == NULL
            || poFilterGeom->Intersect( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature )) )
            return poFeature;

        delete poFeature;
    }
}

/************************************************************************/
/*                          TranslateFeature()                          */
/*                                                                      */
/*      Translate a source feature into a feature for this layer.       */
/************************************************************************/

OGRFeature *OGRVRTLayer::TranslateFeature( OGRFeature *poSrcFeat )

{
    OGRFeature *poDstFeat = new OGRFeature( poFeatureDefn );

/* -------------------------------------------------------------------- */
/*      Handle FID.  We should offer an option to derive it from a      */
/*      field.  (TODO)                                                  */
/* -------------------------------------------------------------------- */
    poDstFeat->SetFID( poSrcFeat->GetFID() );
    
/* -------------------------------------------------------------------- */
/*      Handle the geometry.  Eventually there will be several more     */
/*      supported options.                                              */
/* -------------------------------------------------------------------- */
    if( eGeometryType == VGS_None )
    {
        /* do nothing */
    }
    else if( eGeometryType == VGS_Direct )
    {
        poDstFeat->SetGeometry( poSrcFeat->GetGeometryRef() );
    }
    else
        /* add other options here. */;

/* -------------------------------------------------------------------- */
/*      Copy fields.                                                    */
/* -------------------------------------------------------------------- */
    int iField;

    for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        OGRFieldDefn *poDstDefn = poFeatureDefn->GetFieldDefn( iField );

        if( panSrcField[iField] < 0 )
            continue;

        if( pabDirectCopy[iField] 
            && (poDstDefn->GetType() == OFTInteger
                || poDstDefn->GetType() == OFTReal) )
        {
            memcpy( poDstFeat->GetRawFieldRef( iField ), 
                    poSrcFeat->GetRawFieldRef( panSrcField[iField] ), 
                    sizeof(OGRField) );
            continue;
        }

        /* Eventually we need to offer some more sophisticated translation
           options here for more esoteric types. */

        poDstFeat->SetField( iField, 
                             poSrcFeat->GetFieldAsString(panSrcField[iField]));
    }

    return poDstFeat;
}


/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRVRTLayer::GetFeature( long nFeatureId )

{
    /* This should be implemented directly! */

    return OGRLayer::GetFeature( nFeatureId );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRVRTLayer::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRVRTLayer::GetSpatialRef()

{
    return poSRS;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRVRTLayer::GetFeatureCount( int bForce )

{
    return OGRLayer::GetFeatureCount( bForce );
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRVRTLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    if( poFilterGeom != NULL )
    {
        delete poFilterGeom;
        poFilterGeom = NULL;
    }

    if( poGeomIn != NULL )
        poFilterGeom = poGeomIn->clone();

    ResetReading();
}
