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
 * Revision 1.20  2005/12/08 23:11:01  fwarmerdam
 * added debug message if geometry not parsed successfully ... should use err
 *
 * Revision 1.19  2005/09/21 00:54:43  fwarmerdam
 * fixup OGRFeatureDefn and OGRSpatialReference refcount handling
 *
 * Revision 1.18  2005/09/05 20:29:19  fwarmerdam
 * removed unused variable
 *
 * Revision 1.17  2005/09/05 19:35:10  fwarmerdam
 * Added shape support
 *
 * Revision 1.16  2005/08/30 23:53:00  fwarmerdam
 * implement WKB translation
 *
 * Revision 1.15  2005/08/15 20:12:06  fwarmerdam
 * Ensure that SrcSQL resultsets are released.
 *
 * Revision 1.14  2005/08/02 20:17:26  fwarmerdam
 * pass attribute filter to sublayer
 *
 * Revision 1.13  2005/07/23 13:15:45  fwarmerdam
 * Fixed problem with size of filter string for spatial query.
 *
 * Revision 1.12  2005/06/27 20:13:11  fwarmerdam
 * Delete poSRS if no longer referenced.
 *
 * Revision 1.11  2005/05/16 20:09:46  fwarmerdam
 * added spatial query on x/y columns
 *
 * Revision 1.10  2005/02/22 12:50:10  fwarmerdam
 * use OGRLayer base spatial filter support
 *
 * Revision 1.9  2005/02/02 20:54:27  fwarmerdam
 * track m_nFeaturesRead
 *
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
#include <string>

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
    poSrcLayer = NULL;
    poSRS = NULL;

    iFIDField = -1;

    eGeometryType = VGS_Direct;
    iGeomField = iGeomXField = iGeomYField = iGeomZField = -1;

    pszAttrFilter = NULL;
    panSrcField = NULL;
    pabDirectCopy = NULL;

    bNeedReset = TRUE;
    bSrcLayerFromSQL = FALSE;
}

/************************************************************************/
/*                            ~OGRVRTLayer()                            */
/************************************************************************/

OGRVRTLayer::~OGRVRTLayer()

{
    if( m_nFeaturesRead > 0 && poFeatureDefn != NULL )
    {
        CPLDebug( "VRT", "%d features read on layer '%s'.",
                  (int) m_nFeaturesRead, 
                  poFeatureDefn->GetName() );
    }

    if( poSRS != NULL )
        poSRS->Release();

    if( poSrcDS != NULL )
    {
        if( bSrcLayerFromSQL && poSrcLayer )
            poSrcDS->ReleaseResultSet( poSrcLayer );

        OGRSFDriverRegistrar::GetRegistrar()->ReleaseDataSource( poSrcDS );
    }

    if( poFeatureDefn )
        poFeatureDefn->Release();

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
    poFeatureDefn->Reference();

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
        bSrcLayerFromSQL = TRUE;
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
         eGeometryType = VGS_WKT;
     else if( EQUAL(pszEncoding,"WKB") )
         eGeometryType = VGS_WKB;
     else if( EQUAL(pszEncoding,"Shape") )
         eGeometryType = VGS_Shape;
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

     if( eGeometryType == VGS_WKT 
         || eGeometryType == VGS_WKB 
         || eGeometryType == VGS_Shape )
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
/*                         ResetSourceReading()                         */
/************************************************************************/

void OGRVRTLayer::ResetSourceReading()

{
/* -------------------------------------------------------------------- */
/*      Do we want to let source layer do spatial restriction?          */
/* -------------------------------------------------------------------- */
    char *pszFilter = NULL;
    if( m_poFilterGeom && m_bFilterIsEnvelope 
        && eGeometryType == VGS_PointFromColumns )
    {
        const char *pszXField, *pszYField;

        pszXField = poSrcLayer->GetLayerDefn()->GetFieldDefn(iGeomXField)->GetNameRef();
        pszYField = poSrcLayer->GetLayerDefn()->GetFieldDefn(iGeomYField)->GetNameRef();
        pszFilter = (char *) 
            CPLMalloc(2*strlen(pszXField)+2*strlen(pszYField) + 100);

        sprintf( pszFilter, 
                 "%s > %.15g AND %s < %.15g AND %s > %.15g AND %s < %.15g", 
                 pszXField, m_sFilterEnvelope.MinX,
                 pszXField, m_sFilterEnvelope.MaxX,
                 pszYField, m_sFilterEnvelope.MinY,
                 pszYField, m_sFilterEnvelope.MaxY );

    }
    else
        poSrcLayer->SetAttributeFilter( NULL );

/* -------------------------------------------------------------------- */
/*      Install spatial + attr filter query on source layer.            */
/* -------------------------------------------------------------------- */
    if( pszFilter == NULL && pszAttrFilter == NULL )
        poSrcLayer->SetAttributeFilter( NULL );

    else if( pszFilter != NULL && pszAttrFilter == NULL )
        poSrcLayer->SetAttributeFilter( pszFilter );

    else if( pszFilter == NULL && pszAttrFilter != NULL )
        poSrcLayer->SetAttributeFilter( pszAttrFilter );

    else
    {
        std::string osMerged = pszFilter;

        osMerged += " AND ";
        osMerged += pszAttrFilter;

        poSrcLayer->SetAttributeFilter( osMerged.c_str() );
    }

    CPLFree( pszFilter );

/* -------------------------------------------------------------------- */
/*      Clear spatial filter (to be safe) and reset reading.            */
/* -------------------------------------------------------------------- */
    poSrcLayer->SetSpatialFilter( NULL );
    poSrcLayer->ResetReading();
    bNeedReset = FALSE;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRVRTLayer::GetNextFeature()

{
    if( poSrcLayer == NULL )
        return NULL;

    if( bNeedReset )
        ResetSourceReading();

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

        if( (m_poFilterGeom == NULL
            || FilterGeometry( poFeature->GetGeometryRef() ) )
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

    m_nFeaturesRead++;

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
            if( poGeom == NULL )
                CPLDebug( "OGR_VRT", "Did not get geometry from %s",
                          pszWKT );

            poDstFeat->SetGeometryDirectly( poGeom );
        }
    }
    else if( eGeometryType == VGS_WKB )
    {
        int nBytes;
        GByte *pabyWKB;
        int bNeedFree = FALSE;

        if( poSrcFeat->GetFieldDefnRef(iGeomField)->GetType() == OFTBinary )
        {
            pabyWKB = poSrcFeat->GetFieldAsBinary( iGeomField, &nBytes );
        }
        else
        {
            const char *pszWKT = poSrcFeat->GetFieldAsString( iGeomField );

            pabyWKB = CPLHexToBinary( pszWKT, &nBytes );
            bNeedFree = TRUE;
        }
        
        if( pabyWKB != NULL )
        {
            OGRGeometry *poGeom = NULL;

            if( OGRGeometryFactory::createFromWkb( pabyWKB, NULL, &poGeom,
                                                   nBytes ) == OGRERR_NONE )
                poDstFeat->SetGeometryDirectly( poGeom );
        }

        if( bNeedFree )
            CPLFree( pabyWKB );
    }
    else if( eGeometryType == VGS_Shape )
    {
        int nBytes;
        GByte *pabyWKB;
        int bNeedFree = FALSE;

        if( poSrcFeat->GetFieldDefnRef(iGeomField)->GetType() == OFTBinary )
        {
            pabyWKB = poSrcFeat->GetFieldAsBinary( iGeomField, &nBytes );
        }
        else
        {
            const char *pszWKT = poSrcFeat->GetFieldAsString( iGeomField );

            pabyWKB = CPLHexToBinary( pszWKT, &nBytes );
            bNeedFree = TRUE;
        }
        
        if( pabyWKB != NULL )
        {
            OGRGeometry *poGeom = NULL;

            if( createFromShapeBin( pabyWKB, &poGeom, nBytes ) == OGRERR_NONE )
                poDstFeat->SetGeometryDirectly( poGeom );
        }

        if( bNeedFree )
            CPLFree( pabyWKB );
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
        if( panSrcField[iField] < 0 )
            continue;

        OGRFieldDefn *poDstDefn = poFeatureDefn->GetFieldDefn( iField );
        OGRFieldDefn *poSrcDefn = poFeatureDefn->GetFieldDefn( panSrcField[iField] );

        if( pabDirectCopy[iField] 
            && poDstDefn->GetType() == poSrcDefn->GetType() )
        {
            poDstFeat->SetField( iField,
                                 poSrcFeat->GetRawFieldRef( panSrcField[iField] ) );
        }
        else
        {
            /* Eventually we need to offer some more sophisticated translation
               options here for more esoteric types. */
            
            poDstFeat->SetField( iField, 
                                 poSrcFeat->GetFieldAsString(panSrcField[iField]));
        }
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
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRVRTLayer::SetAttributeFilter( const char *pszNewQuery )

{
    CPLFree( pszAttrFilter );
    if( pszNewQuery == NULL || strlen(pszNewQuery) == 0 )
    {
        pszAttrFilter = NULL;
    }
    else
    {
        CPLFree( pszAttrFilter );
        pszAttrFilter = CPLStrdup( pszNewQuery );
    }

    ResetReading();
    return OGRERR_NONE;
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
    if( m_poFilterGeom == NULL && m_poAttrQuery == NULL )
        return poSrcLayer->GetFeatureCount( bForce );
    else
        return OGRLayer::GetFeatureCount( bForce );
}

/************************************************************************/
/*                         createFromShapeBin()                         */
/*                                                                      */
/*      Translate shapefile binary representation to an OGR             */
/*      geometry.                                                       */
/************************************************************************/

#define SHPT_NULL	0
#define SHPT_POINT	1
#define SHPT_ARC	3
#define SHPT_POLYGON	5
#define SHPT_MULTIPOINT	8
#define SHPT_POINTZ	11
#define SHPT_ARCZ	13
#define SHPT_POLYGONZ	15
#define SHPT_MULTIPOINTZ 18
#define SHPT_POINTM	21
#define SHPT_ARCM	23
#define SHPT_POLYGONM	25
#define SHPT_MULTIPOINTM 28
#define SHPT_MULTIPATCH 31

OGRErr OGRVRTLayer::createFromShapeBin( GByte *pabyShape, OGRGeometry **ppoGeom,
                                        int nBytes )

{
    *ppoGeom = NULL;

    if( nBytes < 1 )
        return OGRERR_FAILURE;

//    printf( "%s\n", CPLBinaryToHex( nBytes, pabyShape ) );

    int nSHPType = pabyShape[0];

/* ==================================================================== */
/*  Extract vertices for a Polygon or Arc.				*/
/* ==================================================================== */
    if( nSHPType == SHPT_POLYGON 
        || nSHPType == SHPT_ARC
        || nSHPType == SHPT_POLYGONZ
        || nSHPType == SHPT_POLYGONM
        || nSHPType == SHPT_ARCZ
        || nSHPType == SHPT_ARCM
        || nSHPType == SHPT_MULTIPATCH )
    {
	GInt32		nPoints, nParts;
	int    		i, nOffset;
        GInt32         *panPartStart;

/* -------------------------------------------------------------------- */
/*      Extract part/point count, and build vertex and part arrays      */
/*      to proper size.                                                 */
/* -------------------------------------------------------------------- */
	memcpy( &nPoints, pabyShape + 40, 4 );
	memcpy( &nParts, pabyShape + 36, 4 );

	CPL_LSBPTR32( &nPoints );
	CPL_LSBPTR32( &nParts );

        panPartStart = (GInt32 *) CPLCalloc(nParts,sizeof(GInt32));

/* -------------------------------------------------------------------- */
/*      Copy out the part array from the record.                        */
/* -------------------------------------------------------------------- */
	memcpy( panPartStart, pabyShape + 44, 4 * nParts );
	for( i = 0; i < nParts; i++ )
	{
            CPL_LSBPTR32( panPartStart + i );
	}

	nOffset = 44 + 4*nParts;

/* -------------------------------------------------------------------- */
/*      If this is a multipatch, we will also have parts types.  For    */
/*      now we ignore and skip past them.                               */
/* -------------------------------------------------------------------- */
        if( nSHPType == SHPT_MULTIPATCH )
            nOffset += 4*nParts;
        
/* -------------------------------------------------------------------- */
/*      Copy out the vertices from the record.                          */
/* -------------------------------------------------------------------- */
        double *padfX = (double *) CPLMalloc(sizeof(double)*nPoints);
        double *padfY = (double *) CPLMalloc(sizeof(double)*nPoints);
        double *padfZ = (double *) CPLCalloc(sizeof(double),nPoints);

	for( i = 0; i < nPoints; i++ )
	{
	    memcpy(padfX + i, pabyShape + nOffset + i * 16, 8 );
	    memcpy(padfY + i, pabyShape + nOffset + i * 16 + 8, 8 );
            CPL_LSBPTR64( padfX + i );
            CPL_LSBPTR64( padfY + i );
	}

        nOffset += 16*nPoints;
        
/* -------------------------------------------------------------------- */
/*      If we have a Z coordinate, collect that now.                    */
/* -------------------------------------------------------------------- */
        if( nSHPType == SHPT_POLYGONZ
            || nSHPType == SHPT_ARCZ
            || nSHPType == SHPT_MULTIPATCH )
        {
            for( i = 0; i < nPoints; i++ )
            {
                memcpy( padfZ + i, pabyShape + nOffset + 16 + i*8, 8 );
                CPL_LSBPTR64( padfZ + i );
            }

            nOffset += 16 + 8*nPoints;
        }

/* -------------------------------------------------------------------- */
/*      Build corresponding OGR objects.                                */
/* -------------------------------------------------------------------- */
        if( nSHPType == SHPT_ARC 
            || nSHPType == SHPT_ARCZ
            || nSHPType == SHPT_ARCM )
        {
/* -------------------------------------------------------------------- */
/*      Arc - As LineString                                             */
/* -------------------------------------------------------------------- */
            if( nParts == 1 )
            {
                OGRLineString *poLine = new OGRLineString();
                *ppoGeom = poLine;

                poLine->setPoints( nPoints, padfX, padfY, padfX );
            }

/* -------------------------------------------------------------------- */
/*      Arc - As MultiLineString                                        */
/* -------------------------------------------------------------------- */
            else
            {
                OGRMultiLineString *poMulti = new OGRMultiLineString;
                *ppoGeom = poMulti;

                for( i = 0; i < nParts; i++ )
                {
                    OGRLineString *poLine = new OGRLineString;
                    int nVerticesInThisPart;

                    if( i == nParts-1 )
                        nVerticesInThisPart = nPoints - panPartStart[i];
                    else
                        nVerticesInThisPart = 
                            panPartStart[i+1] - panPartStart[i];

                    poLine->setPoints( nVerticesInThisPart, 
                                       padfX + panPartStart[i], 
                                       padfY + panPartStart[i], 
                                       padfZ + panPartStart[i] );

                    poMulti->addGeometryDirectly( poLine );
                }
            }
        } /* ARC */

/* -------------------------------------------------------------------- */
/*      Polygon                                                         */
/* -------------------------------------------------------------------- */
        else if( nSHPType == SHPT_POLYGON
                 || nSHPType == SHPT_POLYGONZ
                 || nSHPType == SHPT_POLYGONM )
        {
            OGRPolygon *poMulti = new OGRPolygon;
            *ppoGeom = poMulti;

            for( i = 0; i < nParts; i++ )
            {
                OGRLinearRing *poRing = new OGRLinearRing;
                int nVerticesInThisPart;

                if( i == nParts-1 )
                    nVerticesInThisPart = nPoints - panPartStart[i];
                else
                    nVerticesInThisPart = 
                        panPartStart[i+1] - panPartStart[i];

                poRing->setPoints( nVerticesInThisPart, 
                                   padfX + panPartStart[i], 
                                   padfY + panPartStart[i], 
                                   padfZ + panPartStart[i] );

                poMulti->addRingDirectly( poRing );
            }
        } /* polygon */

/* -------------------------------------------------------------------- */
/*      Multipatch                                                      */
/* -------------------------------------------------------------------- */
        else if( nSHPType == SHPT_MULTIPATCH )
        {
            /* return to this later */
        } 

        CPLFree( panPartStart );
        CPLFree( padfX );
        CPLFree( padfY );
        CPLFree( padfZ );

        if( nSHPType == SHPT_ARC
            || nSHPType == SHPT_POLYGON )
            (*ppoGeom)->setCoordinateDimension( 2 );

        return OGRERR_NONE;
    }

/* ==================================================================== */
/*  Extract vertices for a MultiPoint.					*/
/* ==================================================================== */
    else if( nSHPType == SHPT_MULTIPOINT
             || nSHPType == SHPT_MULTIPOINTM
             || nSHPType == SHPT_MULTIPOINTZ )
    {
#ifdef notdef
	int32		nPoints;
	int    		i, nOffset;

	memcpy( &nPoints, psSHP->pabyRec + 44, 4 );
	if( bBigEndian ) SwapWord( 4, &nPoints );

	psShape->nVertices = nPoints;
        psShape->padfX = (double *) calloc(nPoints,sizeof(double));
        psShape->padfY = (double *) calloc(nPoints,sizeof(double));
        psShape->padfZ = (double *) calloc(nPoints,sizeof(double));
        psShape->padfM = (double *) calloc(nPoints,sizeof(double));

	for( i = 0; i < nPoints; i++ )
	{
	    memcpy(psShape->padfX+i, psSHP->pabyRec + 48 + 16 * i, 8 );
	    memcpy(psShape->padfY+i, psSHP->pabyRec + 48 + 16 * i + 8, 8 );

	    if( bBigEndian ) SwapWord( 8, psShape->padfX + i );
	    if( bBigEndian ) SwapWord( 8, psShape->padfY + i );
	}

        nOffset = 48 + 16*nPoints;
        
/* -------------------------------------------------------------------- */
/*	Get the X/Y bounds.						*/
/* -------------------------------------------------------------------- */
        memcpy( &(psShape->dfXMin), psSHP->pabyRec + 8 +  4, 8 );
        memcpy( &(psShape->dfYMin), psSHP->pabyRec + 8 + 12, 8 );
        memcpy( &(psShape->dfXMax), psSHP->pabyRec + 8 + 20, 8 );
        memcpy( &(psShape->dfYMax), psSHP->pabyRec + 8 + 28, 8 );

	if( bBigEndian ) SwapWord( 8, &(psShape->dfXMin) );
	if( bBigEndian ) SwapWord( 8, &(psShape->dfYMin) );
	if( bBigEndian ) SwapWord( 8, &(psShape->dfXMax) );
	if( bBigEndian ) SwapWord( 8, &(psShape->dfYMax) );

/* -------------------------------------------------------------------- */
/*      If we have a Z coordinate, collect that now.                    */
/* -------------------------------------------------------------------- */
        if( psShape->nSHPType == SHPT_MULTIPOINTZ )
        {
            memcpy( &(psShape->dfZMin), psSHP->pabyRec + nOffset, 8 );
            memcpy( &(psShape->dfZMax), psSHP->pabyRec + nOffset + 8, 8 );
            
            if( bBigEndian ) SwapWord( 8, &(psShape->dfZMin) );
            if( bBigEndian ) SwapWord( 8, &(psShape->dfZMax) );
            
            for( i = 0; i < nPoints; i++ )
            {
                memcpy( psShape->padfZ + i,
                        psSHP->pabyRec + nOffset + 16 + i*8, 8 );
                if( bBigEndian ) SwapWord( 8, psShape->padfZ + i );
            }

            nOffset += 16 + 8*nPoints;
        }

/* -------------------------------------------------------------------- */
/*      If we have a M measure value, then read it now.  We assume      */
/*      that the measure can be present for any shape if the size is    */
/*      big enough, but really it will only occur for the Z shapes      */
/*      (options), and the M shapes.                                    */
/* -------------------------------------------------------------------- */
        if( psSHP->panRecSize[hEntity]+8 >= nOffset + 16 + 8*nPoints )
        {
            memcpy( &(psShape->dfMMin), psSHP->pabyRec + nOffset, 8 );
            memcpy( &(psShape->dfMMax), psSHP->pabyRec + nOffset + 8, 8 );
            
            if( bBigEndian ) SwapWord( 8, &(psShape->dfMMin) );
            if( bBigEndian ) SwapWord( 8, &(psShape->dfMMax) );
            
            for( i = 0; i < nPoints; i++ )
            {
                memcpy( psShape->padfM + i,
                        psSHP->pabyRec + nOffset + 16 + i*8, 8 );
                if( bBigEndian ) SwapWord( 8, psShape->padfM + i );
            }
        }
#endif
    }

/* ==================================================================== */
/*      Extract vertices for a point.                                   */
/* ==================================================================== */
    else if( nSHPType == SHPT_POINT
             || nSHPType == SHPT_POINTM
             || nSHPType == SHPT_POINTZ )
    {
        int	nOffset;
        double  dfX, dfY, dfZ = 0;
        
	memcpy( &dfX, pabyShape + 4, 8 );
	memcpy( &dfY, pabyShape + 4 + 8, 8 );

        CPL_LSBPTR64( &dfX );
        CPL_LSBPTR64( &dfY );
        nOffset = 20 + 8;
        
        if( nSHPType == SHPT_POINTZ )
        {
            memcpy( &dfZ, pabyShape + 4 + 16, 8 );
            CPL_LSBPTR64( &dfY );
        }

        *ppoGeom = new OGRPoint( dfX, dfY, dfZ );

        if( nSHPType != SHPT_POINTZ )
            (*ppoGeom)->setCoordinateDimension( 2 );

        return OGRERR_NONE;
    }

    return OGRERR_FAILURE;
}
