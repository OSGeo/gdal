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
 * Revision 1.8  2004/10/30 04:54:54  fwarmerdam
 * Improved geometry check error message.
 *
 * Revision 1.7  2004/10/30 04:44:00  fwarmerdam
 * Fixed error report when fetching layer.
 *
 * Revision 1.6  2004/10/16 21:56:36  fwarmerdam
 * Fixed initialization of "z".
 *
 * Revision 1.5  2004/03/25 13:23:41  warmerda
 * Fixed typo in error message.
 *
 * Revision 1.4  2003/12/30 18:34:57  warmerda
 * Added support for SrcSQL instead of SrcLayer.
 *
 * Revision 1.3  2003/11/10 20:11:55  warmerda
 * Allow any UserInput in LayerSYS
 *
 * Revision 1.2  2003/11/07 21:55:12  warmerda
 * complete fid support, relative dsname, fixes
 *
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

    bNeedReset = TRUE;
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

    if( poSrcDS != NULL )
    {
        OGRSFDriverRegistrar::GetRegistrar()->ReleaseDataSource( poSrcDS );
    }

    delete poFeatureDefn;

    CPLFree( panSrcField );
    CPLFree( pabDirectCopy );
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

int OGRVRTLayer::Initialize( CPLXMLNode *psLTree, const char *pszVRTDirectory )

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
/*      Figure out the data source name.  It may be treated relative    */
/*      to vrt filename, but normally it is used directly.              */
/* -------------------------------------------------------------------- */
    OGRSFDriverRegistrar *poReg = OGRSFDriverRegistrar::GetRegistrar();
    char *pszSrcDSName = (char *) CPLGetXMLValue(psLTree,"SrcDataSource",NULL);

    if( pszSrcDSName == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Missing SrcDataSource for layer %s.", pszLayerName );
        return FALSE;
    }

    if( atoi(CPLGetXMLValue( psLTree, "SrcDataSource.relativetoVRT", "0")) )
    {
        pszSrcDSName = CPLStrdup(
            CPLProjectRelativeFilename( pszVRTDirectory, pszSrcDSName ) );
    }
    else
    {
        pszSrcDSName = CPLStrdup(pszSrcDSName);
    }

/* -------------------------------------------------------------------- */
/*      Try to access the datasource.                                   */
/* -------------------------------------------------------------------- */
    CPLErrorReset();
    poSrcDS = poReg->OpenShared( pszSrcDSName, FALSE, NULL );

    if( poSrcDS == NULL ) 
    {
        if( strlen(CPLGetLastErrorMsg()) == 0 )
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Failed to open datasource `%s'.", 
                      pszSrcDSName );
        CPLFree( pszSrcDSName );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Is this layer derived from an SQL query result?                 */
/* -------------------------------------------------------------------- */
    const char *pszSQL = CPLGetXMLValue( psLTree, "SrcSQL", NULL );

    if( pszSQL != NULL )
    {
        poSrcLayer = poSrcDS->ExecuteSQL( pszSQL, NULL, NULL );
        if( poSrcLayer == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "SQL statement failed, or returned no layer result:\n%s",
                      pszSQL );					      
            return FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Fetch the layer if it is a regular layer.                       */
/* -------------------------------------------------------------------- */
    if( poSrcLayer == NULL )
    {
        const char *pszSrcLayerName = CPLGetXMLValue( psLTree, "SrcLayer", 
                                                      pszLayerName );
        
        poSrcLayer = poSrcDS->GetLayerByName( pszSrcLayerName );
        if( poSrcLayer == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to find layer '%s' on datasource '%s'.", 
                      pszSrcLayerName, pszSrcDSName );
            CPLFree( pszSrcDSName );
            return FALSE;
        }
    }
        
    CPLFree( pszSrcDSName );

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
     const char *pszLayerSRS = CPLGetXMLValue( psLTree, "LayerSRS", NULL );

     if( pszLayerSRS != NULL )
     {
         if( EQUAL(pszLayerSRS,"NULL") )
             poSRS = NULL;
         else
         {
             OGRSpatialReference oSRS;

             if( oSRS.SetFromUserInput( pszLayerSRS ) != OGRERR_NONE )
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

/* -------------------------------------------------------------------- */
/*      Handle GeometryField.                                           */
/* -------------------------------------------------------------------- */
     const char *pszEncoding;

     pszEncoding = CPLGetXMLValue( psLTree,"GeometryField.encoding", "direct");

     if( EQUAL(pszEncoding,"Direct") )
         eGeometryType = VGS_Direct;
     else if( EQUAL(pszEncoding,"None") )
         eGeometryType = VGS_None;
     else if( EQUAL(pszEncoding,"WKT") )
     {
         eGeometryType = VGS_WKT;
     }
     else if( EQUAL(pszEncoding,"WKB") )
     {
         eGeometryType = VGS_WKB;
     }
     else if( EQUAL(pszEncoding,"PointFromColumns") )
     {
         eGeometryType = VGS_PointFromColumns;

         iGeomXField = poSrcLayer->GetLayerDefn()->GetFieldIndex(
             CPLGetXMLValue( psLTree, "GeometryField.x", "missing" ) );
         iGeomYField = poSrcLayer->GetLayerDefn()->GetFieldIndex(
             CPLGetXMLValue( psLTree, "GeometryField.y", "missing" ) );
         iGeomZField = poSrcLayer->GetLayerDefn()->GetFieldIndex(
             CPLGetXMLValue( psLTree, "GeometryField.z", "missing" ) );

         if( iGeomXField == -1 || iGeomYField == -1 )
         {
             CPLError( CE_Failure, CPLE_AppDefined, 
                       "Unable to identify source X or Y field for PointFromColumns encoding." );
             return FALSE;
         }
     }
     else
     {
         CPLError( CE_Failure, CPLE_AppDefined, 
                   "encoding=\"%s\" not recognised.", pszEncoding );
         return FALSE;
     }

     if( eGeometryType == VGS_WKT || eGeometryType == VGS_WKB )
     {
         const char *pszFieldName = 
             CPLGetXMLValue( psLTree, "GeometryField.field", "missing" );

         iGeomField = poSrcLayer->GetLayerDefn()->GetFieldIndex(pszFieldName);

         if( iGeomField == -1 )
         {
             CPLError( CE_Failure, CPLE_AppDefined, 
                       "Unable to identify source field '%s' for geometry.",
                       pszFieldName );
             return FALSE;
         }
     }
                                               
/* -------------------------------------------------------------------- */
/*      Figure out what should be used as an FID.                       */
/* -------------------------------------------------------------------- */
     const char *pszFIDFieldName = CPLGetXMLValue( psLTree, "FID", NULL );

     if( pszFIDFieldName != NULL )
     {
         iFIDField = 
             poSrcLayer->GetLayerDefn()->GetFieldIndex( pszFIDFieldName );
         if( iFIDField == -1 )
         {
             CPLError( CE_Failure, CPLE_AppDefined, 
                       "Unable to identify FID field '%s'.",
                       pszFIDFieldName );
             return FALSE;
         }
     }
     
     return TRUE;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRVRTLayer::ResetReading()

{
    bNeedReset = TRUE;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRVRTLayer::GetNextFeature()

{
    if( poSrcLayer == NULL )
        return NULL;

    if( bNeedReset )
    {
        poSrcLayer->SetAttributeFilter( NULL );
        poSrcLayer->SetSpatialFilter( NULL );
        poSrcLayer->ResetReading();
        bNeedReset = FALSE;
    }

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
    if( iFIDField == -1 )
        poDstFeat->SetFID( poSrcFeat->GetFID() );
    else
        poDstFeat->SetFID( poSrcFeat->GetFieldAsInteger( iFIDField ) );
    
/* -------------------------------------------------------------------- */
/*      Handle the geometry.  Eventually there will be several more     */
/*      supported options.                                              */
/* -------------------------------------------------------------------- */
    if( eGeometryType == VGS_None )
    {
        /* do nothing */
    }
    else if( eGeometryType == VGS_WKT )
    {
        char *pszWKT = (char *) poSrcFeat->GetFieldAsString( iGeomField );
        
        if( pszWKT != NULL )
        {
            OGRGeometry *poGeom = NULL;

            OGRGeometryFactory::createFromWkt( &pszWKT, NULL, &poGeom );
            poDstFeat->SetGeometryDirectly( poGeom );
        }
    }
    else if( eGeometryType == VGS_Direct )
    {
        poDstFeat->SetGeometry( poSrcFeat->GetGeometryRef() );
    }
    else if( eGeometryType == VGS_PointFromColumns )
    {
        double dfZ = 0.0;

        if( iGeomZField != -1 )
            dfZ = poSrcFeat->GetFieldAsDouble( iGeomZField );
        
        poDstFeat->SetGeometryDirectly( 
            new OGRPoint( poSrcFeat->GetFieldAsDouble( iGeomXField ),
                          poSrcFeat->GetFieldAsDouble( iGeomYField ),
                          dfZ ) );
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
    if( poSrcLayer == NULL )
        return NULL;

    bNeedReset = TRUE;

/* -------------------------------------------------------------------- */
/*      If the FID is directly mapped, we can do a simple               */
/*      GetFeature() to get our target feature.  Otherwise we need      */
/*      to setup an appropriate query to get it.                        */
/* -------------------------------------------------------------------- */
    OGRFeature      *poSrcFeature, *poFeature;
    
    if( iFIDField == -1 )
    {
        poSrcFeature = poSrcLayer->GetFeature( nFeatureId );
    }
    else 
    {
        char szFIDQuery[200];

        poSrcLayer->ResetReading();
        sprintf( szFIDQuery, "%s = %ld", 
            poSrcLayer->GetLayerDefn()->GetFieldDefn(iFIDField)->GetNameRef(),
                 nFeatureId );
        poSrcLayer->SetSpatialFilter( NULL );
        poSrcLayer->SetAttributeFilter( szFIDQuery );
        
        poSrcFeature = poSrcLayer->GetNextFeature();
    }

    if( poSrcFeature == NULL )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Translate feature and return it.                                */
/* -------------------------------------------------------------------- */
    poFeature = TranslateFeature( poSrcFeature );
    delete poSrcFeature;

    return poFeature;
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
    if( poFilterGeom == NULL && m_poAttrQuery == NULL )
        return poSrcLayer->GetFeatureCount( bForce );
    else
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
