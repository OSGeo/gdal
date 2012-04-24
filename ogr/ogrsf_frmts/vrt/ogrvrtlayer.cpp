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
 ****************************************************************************/

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

static const OGRGeomTypeName asGeomTypeNames[] = { /* 25D versions are implicit */
    { wkbUnknown, "wkbUnknown" },
    { wkbPoint, "wkbPoint" },
    { wkbLineString, "wkbLineString" },
    { wkbPolygon, "wkbPolygon" },
    { wkbMultiPoint, "wkbMultiPoint" },
    { wkbMultiLineString, "wkbMultiLineString" },
    { wkbMultiPolygon, "wkbMultiPolygon" },
    { wkbGeometryCollection, "wkbGeometryCollection" },
    { wkbNone, "wkbNone" },
    { wkbLinearRing, "wkbLinearRing" },
    { wkbNone, NULL }
};

#define UNSUPPORTED_OP_READ_ONLY "%s : unsupported operation on a read-only datasource."

/************************************************************************/
/*                            OGRVRTLayer()                             */
/************************************************************************/

OGRVRTLayer::OGRVRTLayer(OGRVRTDataSource* poDSIn)

{
    poDS = poDSIn;

    bHasFullInitialized = FALSE;
    eGeomType = wkbUnknown;
    psLTree = NULL;

    poFeatureDefn = NULL;
    poSrcLayer = NULL;
    poSRS = NULL;
    poSrcDS = NULL;
    poSrcFeatureDefn = NULL;

    bUseSpatialSubquery = FALSE;
    iFIDField = -1;
    iStyleField = -1;

    eGeometryStyle = VGS_Direct;
    iGeomField = iGeomXField = iGeomYField = iGeomZField = -1;

    pszAttrFilter = NULL;

    bNeedReset = TRUE;
    bSrcLayerFromSQL = FALSE;

    bSrcClip = FALSE;
    poSrcRegion = NULL;
    bUpdate = FALSE;
    bAttrFilterPassThrough = FALSE;
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

        if( bSrcDSShared )
            OGRSFDriverRegistrar::GetRegistrar()->ReleaseDataSource( poSrcDS );
        else
            delete poSrcDS;
    }

    if( poFeatureDefn )
        poFeatureDefn->Release();

    CPLFree( pszAttrFilter );

    if( poSrcRegion != NULL )
        delete poSrcRegion;
}

/************************************************************************/
/*                         GetSrcLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn* OGRVRTLayer::GetSrcLayerDefn()
{
    if (poSrcFeatureDefn)
        return poSrcFeatureDefn;

    if (poSrcLayer)
        poSrcFeatureDefn = poSrcLayer->GetLayerDefn();

    return poSrcFeatureDefn;
}

/************************************************************************/
/*                         FastInitialize()                             */
/************************************************************************/

int OGRVRTLayer::FastInitialize( CPLXMLNode *psLTree, const char *pszVRTDirectory,
                             int bUpdate)

{
    this->psLTree = psLTree;
    this->bUpdate = bUpdate;
    osVRTDirectory = pszVRTDirectory;

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

    osName = pszLayerName;

/* -------------------------------------------------------------------- */
/*      Do we have a fixed geometry type?  If so use it                 */
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
                 eGeomType = asGeomTypeNames[iType].eType;

                 if( strstr(pszGType,"25D") != NULL )
                     eGeomType = (OGRwkbGeometryType) (eGeomType | wkb25DBit);
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

/* -------------------------------------------------------------------- */
/*      Apply a spatial reference system if provided                    */
/* -------------------------------------------------------------------- */
     const char* pszLayerSRS = CPLGetXMLValue( psLTree, "LayerSRS", NULL );
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

     return TRUE;
}

/************************************************************************/
/*                         FullInitialize()                             */
/************************************************************************/

int OGRVRTLayer::FullInitialize()

{
    const char *pszSharedSetting = NULL;
    const char *pszSQL = NULL;
    const char *pszSrcRegion = NULL;
    const char *pszGType = NULL;
    const char *pszLayerSRS = NULL;
    const char *pszEncoding = NULL;
    const char *pszFIDFieldName = NULL;
    const char *pszStyleFieldName = NULL;
    CPLXMLNode *psChild = NULL;

    if (bHasFullInitialized)
        return TRUE;

    bHasFullInitialized = TRUE;

    poFeatureDefn = new OGRFeatureDefn( osName );
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
                  "Missing SrcDataSource for layer %s.", osName.c_str() );
        goto error;
    }

    if( CSLTestBoolean(CPLGetXMLValue( psLTree, "SrcDataSource.relativetoVRT", 
                                       "0")) )
    {
        pszSrcDSName = CPLStrdup(
            CPLProjectRelativeFilename( osVRTDirectory, pszSrcDSName ) );
    }
    else
    {
        pszSrcDSName = CPLStrdup(pszSrcDSName);
    }

/* -------------------------------------------------------------------- */
/*      Are we accessing this datasource in shared mode?  We default    */
/*      to shared for SrcSQL requests, but we also allow the XML to     */
/*      control our shared setting with an attribute on the             */
/*      datasource element.                                             */
/* -------------------------------------------------------------------- */
    pszSharedSetting = CPLGetXMLValue( psLTree,
                                                   "SrcDataSource.shared",
                                                   NULL );
    if( pszSharedSetting == NULL )
    {
        if( CPLGetXMLValue( psLTree, "SrcSQL", NULL ) == NULL )
            pszSharedSetting = "OFF";
        else
            pszSharedSetting = "ON";
    }

    bSrcDSShared = CSLTestBoolean( pszSharedSetting );

    // update mode doesn't make sense if we have a SrcSQL element
    if (CPLGetXMLValue( psLTree, "SrcSQL", NULL ) != NULL)
        bUpdate = FALSE;

/* -------------------------------------------------------------------- */
/*      Try to access the datasource.                                   */
/* -------------------------------------------------------------------- */
try_again:
    CPLErrorReset();
    if( EQUAL(pszSrcDSName,"@dummy@") )
    {
        OGRSFDriver *poMemDriver = OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName("Memory");
        if (poMemDriver != NULL)
        {
            poSrcDS = poMemDriver->CreateDataSource( "@dummy@" );
            poSrcDS->CreateLayer( "@dummy@" );
        }
    }
    else if( bSrcDSShared )
    {
        if (poDS->IsInForbiddenNames(pszSrcDSName))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cyclic VRT opening detected !");
        }
        else
        {
            poSrcDS = poReg->OpenShared( pszSrcDSName, bUpdate, NULL );
            /* Is it a VRT datasource ? */
            if (poSrcDS != NULL && poSrcDS->GetDriver() == poDS->GetDriver())
            {
                OGRVRTDataSource* poVRTSrcDS = (OGRVRTDataSource*)poSrcDS;
                poVRTSrcDS->AddForbiddenNames(poDS->GetName());
            }
        }
    }
    else
    {
        if (poDS->GetCallLevel() < 32)
        {
            poSrcDS = poReg->Open( pszSrcDSName, bUpdate, NULL );
            /* Is it a VRT datasource ? */
            if (poSrcDS != NULL && poSrcDS->GetDriver() == poDS->GetDriver())
            {
                OGRVRTDataSource* poVRTSrcDS = (OGRVRTDataSource*)poSrcDS;
                poVRTSrcDS->SetCallLevel(poDS->GetCallLevel() + 1);
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Trying to open a VRT from a VRT from a VRT from ... [32 times] a VRT !");
        }
    }

    if( poSrcDS == NULL ) 
    {
        if (bUpdate)
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Cannot open datasource `%s' in update mode. Trying again in read-only mode",
                       pszSrcDSName );
            bUpdate = FALSE;
            goto try_again;
        }
        if( strlen(CPLGetLastErrorMsg()) == 0 )
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Failed to open datasource `%s'.", 
                      pszSrcDSName );
        CPLFree( pszSrcDSName );
        goto error;
    }

    this->bUpdate = bUpdate;

/* -------------------------------------------------------------------- */
/*      Is this layer derived from an SQL query result?                 */
/* -------------------------------------------------------------------- */
    pszSQL = CPLGetXMLValue( psLTree, "SrcSQL", NULL );

    if( pszSQL != NULL )
    {
        poSrcLayer = poSrcDS->ExecuteSQL( pszSQL, NULL, NULL );
        if( poSrcLayer == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "SQL statement failed, or returned no layer result:\n%s",
                      pszSQL );
            goto error;
        }
        bSrcLayerFromSQL = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Fetch the layer if it is a regular layer.                       */
/* -------------------------------------------------------------------- */
    if( poSrcLayer == NULL )
    {
        const char *pszSrcLayerName = CPLGetXMLValue( psLTree, "SrcLayer", 
                                                      osName );
        
        poSrcLayer = poSrcDS->GetLayerByName( pszSrcLayerName );
        if( poSrcLayer == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to find layer '%s' on datasource '%s'.", 
                      pszSrcLayerName, pszSrcDSName );
            CPLFree( pszSrcDSName );
            goto error;
        }
    }
        
    CPLFree( pszSrcDSName );

/* -------------------------------------------------------------------- */
/*      Do we have a fixed geometry type?  If not derive from the       */
/*      source layer.                                                   */
/* -------------------------------------------------------------------- */
     pszGType = CPLGetXMLValue( psLTree, "GeometryType", NULL );
     if( pszGType == NULL )
     {
         eGeomType = poSrcLayer->GetGeomType();
     }
     poFeatureDefn->SetGeomType(eGeomType);

/* -------------------------------------------------------------------- */
/*      Copy spatial reference system from source if not provided       */
/* -------------------------------------------------------------------- */
     pszLayerSRS = CPLGetXMLValue( psLTree, "LayerSRS", NULL );
     if( pszLayerSRS == NULL )
     {
         CPLAssert(poSRS == NULL);
         if( poSrcLayer->GetSpatialRef() != NULL )
             poSRS = poSrcLayer->GetSpatialRef()->Clone();
         else
             poSRS = NULL;
     }

/* -------------------------------------------------------------------- */
/*      Handle GeometryField.                                           */
/* -------------------------------------------------------------------- */

     pszEncoding = CPLGetXMLValue( psLTree,"GeometryField.encoding", "direct");

     if( EQUAL(pszEncoding,"Direct") )
         eGeometryStyle = VGS_Direct;
     else if( EQUAL(pszEncoding,"None") )
         eGeometryStyle = VGS_None;
     else if( EQUAL(pszEncoding,"WKT") )
         eGeometryStyle = VGS_WKT;
     else if( EQUAL(pszEncoding,"WKB") )
         eGeometryStyle = VGS_WKB;
     else if( EQUAL(pszEncoding,"Shape") )
         eGeometryStyle = VGS_Shape;
     else if( EQUAL(pszEncoding,"PointFromColumns") )
     {
         eGeometryStyle = VGS_PointFromColumns;
         bUseSpatialSubquery = 
             CSLTestBoolean(
                 CPLGetXMLValue(psLTree, 
                                "GeometryField.useSpatialSubquery",
                                "TRUE"));

         iGeomXField = GetSrcLayerDefn()->GetFieldIndex(
             CPLGetXMLValue( psLTree, "GeometryField.x", "missing" ) );
         iGeomYField = GetSrcLayerDefn()->GetFieldIndex(
             CPLGetXMLValue( psLTree, "GeometryField.y", "missing" ) );
         iGeomZField = GetSrcLayerDefn()->GetFieldIndex(
             CPLGetXMLValue( psLTree, "GeometryField.z", "missing" ) );

         if( iGeomXField == -1 || iGeomYField == -1 )
         {
             CPLError( CE_Failure, CPLE_AppDefined, 
                       "Unable to identify source X or Y field for PointFromColumns encoding." );
             goto error;
         }
     }
     else
     {
         CPLError( CE_Failure, CPLE_AppDefined, 
                   "encoding=\"%s\" not recognised.", pszEncoding );
         goto error;
     }

     if( eGeometryStyle == VGS_WKT
         || eGeometryStyle == VGS_WKB
         || eGeometryStyle == VGS_Shape )
     {
         const char *pszFieldName = 
             CPLGetXMLValue( psLTree, "GeometryField.field", "missing" );

         iGeomField = GetSrcLayerDefn()->GetFieldIndex(pszFieldName);

         if( iGeomField == -1 )
         {
             CPLError( CE_Failure, CPLE_AppDefined, 
                       "Unable to identify source field '%s' for geometry.",
                       pszFieldName );
             goto error;
         }
     }

/* -------------------------------------------------------------------- */
/*      Figure out what should be used as an FID.                       */
/* -------------------------------------------------------------------- */
     pszFIDFieldName = CPLGetXMLValue( psLTree, "FID", NULL );

     if( pszFIDFieldName != NULL )
     {
         iFIDField =
             GetSrcLayerDefn()->GetFieldIndex( pszFIDFieldName );
         if( iFIDField == -1 )
         {
             CPLError( CE_Failure, CPLE_AppDefined, 
                       "Unable to identify FID field '%s'.",
                       pszFIDFieldName );
             goto error;
         }
     }

/* -------------------------------------------------------------------- */
/*      Figure out what should be used as a Style                       */
/* -------------------------------------------------------------------- */
     pszStyleFieldName = CPLGetXMLValue( psLTree, "Style", NULL );

     if( pszStyleFieldName != NULL )
     {
         iStyleField =
             GetSrcLayerDefn()->GetFieldIndex( pszStyleFieldName );
         if( iStyleField == -1 )
         {
             CPLError( CE_Failure, CPLE_AppDefined, 
                       "Unable to identify Style field '%s'.",
                       pszStyleFieldName );
             goto error;
         }
     }

/* ==================================================================== */
/*      Search for schema definitions in the VRT.                       */
/* ==================================================================== */
     bAttrFilterPassThrough = TRUE;
     for( psChild = psLTree->psChild; psChild != NULL; psChild=psChild->psNext )
     {
         if( psChild->eType == CXT_Element && EQUAL(psChild->pszValue,"Field") )
         {
/* -------------------------------------------------------------------- */
/*      Field name.                                                     */
/* -------------------------------------------------------------------- */
             const char *pszName = CPLGetXMLValue( psChild, "name", NULL );
             if( pszName == NULL )
             {
                 CPLError( CE_Failure, CPLE_AppDefined, 
                           "Unable to identify Field name." );
                 goto error;
             }

             OGRFieldDefn oFieldDefn( pszName, OFTString );
             
/* -------------------------------------------------------------------- */
/*      Type                                                            */
/* -------------------------------------------------------------------- */
             const char *pszArg = CPLGetXMLValue( psChild, "type", NULL );

             if( pszArg != NULL )
             {
                 int iType;

                 for( iType = 0; iType <= (int) OFTMaxType; iType++ )
                 {
                     if( EQUAL(pszArg,OGRFieldDefn::GetFieldTypeName(
                                   (OGRFieldType)iType)) )
                     {
                         oFieldDefn.SetType( (OGRFieldType) iType );
                         break;
                     }
                 }

                 if( iType > (int) OFTMaxType )
                 {
                     CPLError( CE_Failure, CPLE_AppDefined, 
                               "Unable to identify Field type '%s'.",
                               pszArg );
                     goto error;
                 }
             }

/* -------------------------------------------------------------------- */
/*      Width and precision.                                            */
/* -------------------------------------------------------------------- */
             oFieldDefn.SetWidth( 
                 atoi(CPLGetXMLValue( psChild, "width", "0" )) );
             oFieldDefn.SetPrecision( 
                 atoi(CPLGetXMLValue( psChild, "precision", "0" )) );

/* -------------------------------------------------------------------- */
/*      Create the field.                                               */
/* -------------------------------------------------------------------- */
             poFeatureDefn->AddFieldDefn( &oFieldDefn );

             abDirectCopy.push_back( FALSE );

/* -------------------------------------------------------------------- */
/*      Source field.                                                   */
/* -------------------------------------------------------------------- */
             int iSrcField =
                 GetSrcLayerDefn()->GetFieldIndex( pszName );

             pszArg = CPLGetXMLValue( psChild, "src", NULL );

             if( pszArg != NULL )
             {
                 iSrcField = 
                     GetSrcLayerDefn()->GetFieldIndex( pszArg );
                 if( iSrcField == -1 )
                 {
                     CPLError( CE_Failure, CPLE_AppDefined,
                               "Unable to find source field '%s'.",
                               pszArg );
                     goto error;
                 }
             }

             if (iSrcField != poFeatureDefn->GetFieldCount() - 1)
                 bAttrFilterPassThrough = FALSE;
             else
             {
                 OGRFieldDefn* poSrcFieldDefn = GetSrcLayerDefn()->GetFieldDefn(iSrcField);
                 if (poSrcFieldDefn->GetType() != oFieldDefn.GetType())
                     bAttrFilterPassThrough = FALSE;
             }

             anSrcField.push_back( iSrcField );
         }
     }

     bAttrFilterPassThrough &= ( poFeatureDefn->GetFieldCount() == GetSrcLayerDefn()->GetFieldCount() );

     CPLAssert( poFeatureDefn->GetFieldCount() == (int) anSrcField.size() );

/* -------------------------------------------------------------------- */
/*      Create the schema, if it was not explicitly in the VRT.         */
/* -------------------------------------------------------------------- */
     if( poFeatureDefn->GetFieldCount() == 0 )
     {
         int bReportSrcColumn =
             CSLTestBoolean(CPLGetXMLValue( psLTree, "GeometryField.reportSrcColumn", "YES" ));

         int iSrcField;
         int iDstField;
         int nSrcFieldCount = GetSrcLayerDefn()->GetFieldCount();
         int nDstFieldCount = nSrcFieldCount;
         if (bReportSrcColumn == FALSE)
         {
             if (iGeomXField != -1) nDstFieldCount --;
             if (iGeomYField != -1) nDstFieldCount --;
             if (iGeomZField != -1) nDstFieldCount --;
             if (iGeomField != -1) nDstFieldCount --;
         }
         
         for( iSrcField = 0, iDstField = 0; iSrcField < nSrcFieldCount; iSrcField++ )
         {
             if (bReportSrcColumn == FALSE &&
                 (iSrcField == iGeomXField || iSrcField == iGeomYField ||
                  iSrcField == iGeomZField || iSrcField == iGeomField))
                 continue;
             
             poFeatureDefn->AddFieldDefn( GetSrcLayerDefn()->GetFieldDefn( iSrcField ) );
             anSrcField.push_back( iSrcField );
             abDirectCopy.push_back( TRUE );
             iDstField++;
         }
         bAttrFilterPassThrough = TRUE;
     }

/* -------------------------------------------------------------------- */
/*      Do we have a SrcRegion?                                         */
/* -------------------------------------------------------------------- */
     pszSrcRegion = CPLGetXMLValue( psLTree, "SrcRegion", NULL );
     if( pszSrcRegion != NULL )
     {
        OGRGeometryFactory::createFromWkt( (char**) &pszSrcRegion, NULL, &poSrcRegion );
        if( poSrcRegion == NULL || wkbFlatten(poSrcRegion->getGeometryType()) != wkbPolygon)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                        "Ignoring SrcRegion. It must be a valid WKT polygon");
            delete poSrcRegion;
            poSrcRegion = NULL;
        }

        bSrcClip = CSLTestBoolean(CPLGetXMLValue( psLTree, "SrcRegion.clip", "FALSE" ));
     }

/* -------------------------------------------------------------------- */
/*      Is VRT layer definition identical to the source layer defn ?    */
/*      If so, use it directly, and save the translation of features.   */
/* -------------------------------------------------------------------- */
     if (poSrcFeatureDefn != NULL && iFIDField == -1 && iStyleField == -1 &&
         eGeometryStyle == VGS_Direct &&
         poSrcFeatureDefn->IsSame(poFeatureDefn))
     {
        CPLDebug("VRT", "Source feature definition is identical to VRT feature definition. Use optimized path");
        poFeatureDefn->Release();
        poFeatureDefn = poSrcFeatureDefn;
        poFeatureDefn->Reference();
     }

/* -------------------------------------------------------------------- */
/*      Allow vrt to override whether attribute filters should be       */
/*      passed through.                                                 */
/* -------------------------------------------------------------------- */
     if( CPLGetXMLValue( psLTree, "attrFilterPassThrough", NULL ) != NULL )
         bAttrFilterPassThrough =
             CSLTestBoolean(
                 CPLGetXMLValue(psLTree, "attrFilterPassThrough",
                                "TRUE") );

     return TRUE;

error:
    if( poFeatureDefn )
        poFeatureDefn->Release();
    poFeatureDefn = new OGRFeatureDefn( osName );
    poFeatureDefn->Reference();
    return FALSE;
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

int OGRVRTLayer::ResetSourceReading()

{
    int bSuccess = TRUE;

/* -------------------------------------------------------------------- */
/*      Do we want to let source layer do spatial restriction?          */
/* -------------------------------------------------------------------- */
    char *pszFilter = NULL;
    if( (m_poFilterGeom || poSrcRegion) && bUseSpatialSubquery &&
         eGeometryStyle == VGS_PointFromColumns )
    {
        const char *pszXField, *pszYField;

        pszXField = poSrcLayer->GetLayerDefn()->GetFieldDefn(iGeomXField)->GetNameRef();
        pszYField = poSrcLayer->GetLayerDefn()->GetFieldDefn(iGeomYField)->GetNameRef();
        if (bUseSpatialSubquery)
        {
            OGRFieldType xType = poSrcLayer->GetLayerDefn()->GetFieldDefn(iGeomXField)->GetType();
            OGRFieldType yType = poSrcLayer->GetLayerDefn()->GetFieldDefn(iGeomYField)->GetType();
            if (!((xType == OFTReal || xType == OFTInteger) && (yType == OFTReal || yType == OFTInteger)))
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                        "The '%s' and/or '%s' fields of the source layer are not declared as numeric fields,\n"
                        "so the spatial filter cannot be turned into an attribute filter on them",
                         pszXField, pszYField);
                bUseSpatialSubquery = FALSE;
            }
        }
        if (bUseSpatialSubquery)
        {
            OGREnvelope sEnvelope;

            pszFilter = (char *) 
                CPLMalloc(2*strlen(pszXField)+2*strlen(pszYField) + 100);

            if (poSrcRegion != NULL)
            {
                if (m_poFilterGeom == NULL)
                    poSrcRegion->getEnvelope( &sEnvelope );
                else
                {
                    OGRGeometry* poIntersection = poSrcRegion->Intersection(m_poFilterGeom);
                    if (poIntersection)
                    {
                        poIntersection->getEnvelope( &sEnvelope );
                        delete poIntersection;
                    }
                    else
                    {
                        sEnvelope.MinX = 0;
                        sEnvelope.MaxX = 0;
                        sEnvelope.MinY = 0;
                        sEnvelope.MaxY = 0;
                    }
                }
            }
            else
                m_poFilterGeom->getEnvelope( &sEnvelope );

            sprintf( pszFilter, 
                    "%s > %.15g AND %s < %.15g AND %s > %.15g AND %s < %.15g", 
                    pszXField, sEnvelope.MinX,
                    pszXField, sEnvelope.MaxX,
                    pszYField, sEnvelope.MinY,
                    pszYField, sEnvelope.MaxY );
            char* pszComma;
            while((pszComma = strchr(pszFilter, ',')) != NULL)
                *pszComma = '.';
        }
    }

/* -------------------------------------------------------------------- */
/*      Install spatial + attr filter query on source layer.            */
/* -------------------------------------------------------------------- */
    if( pszFilter == NULL && pszAttrFilter == NULL )
        bSuccess = (poSrcLayer->SetAttributeFilter( NULL ) == CE_None);

    else if( pszFilter != NULL && pszAttrFilter == NULL )
        bSuccess = (poSrcLayer->SetAttributeFilter( pszFilter ) == CE_None);

    else if( pszFilter == NULL && pszAttrFilter != NULL )
        bSuccess = (poSrcLayer->SetAttributeFilter( pszAttrFilter ) == CE_None);

    else
    {
        CPLString osMerged = pszFilter;

        osMerged += " AND ";
        osMerged += pszAttrFilter;

        bSuccess = (poSrcLayer->SetAttributeFilter(osMerged) == CE_None);
    }

    CPLFree( pszFilter );

/* -------------------------------------------------------------------- */
/*      Clear spatial filter (to be safe) for non direct geometries     */
/*      and reset reading.                                              */
/* -------------------------------------------------------------------- */
    if (eGeometryStyle == VGS_Direct)
    {
        if (poSrcRegion == NULL)
            poSrcLayer->SetSpatialFilter( m_poFilterGeom );
        else if (m_poFilterGeom == NULL)
            poSrcLayer->SetSpatialFilter( poSrcRegion );
        else
        {
            if( wkbFlatten(m_poFilterGeom->getGeometryType()) != wkbPolygon )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Spatial filter should be polygon when a SrcRegion is defined. Ignoring it");
                poSrcLayer->SetSpatialFilter( poSrcRegion );
            }
            else
            {
                OGRGeometry* poIntersection = m_poFilterGeom->Intersection(poSrcRegion);
                poSrcLayer->SetSpatialFilter( poIntersection );
                delete poIntersection;
            }
        }
    }
    else
        poSrcLayer->SetSpatialFilter( NULL );
    poSrcLayer->ResetReading();
    bNeedReset = FALSE;

    return bSuccess;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRVRTLayer::GetNextFeature()

{
    if (!bHasFullInitialized) FullInitialize();
    if (!poSrcLayer) return NULL;

    if( bNeedReset )
    {
        if( !ResetSourceReading() )
            return NULL;
    }

    for( ; TRUE; )
    {
        OGRFeature      *poSrcFeature, *poFeature;

        poSrcFeature = poSrcLayer->GetNextFeature();
        if( poSrcFeature == NULL )
            return NULL;

        if (poFeatureDefn == poSrcFeatureDefn)
        {
            poFeature = poSrcFeature;
            ClipAndAssignSRS(poFeature);
        }
        else
        {
            poFeature = TranslateFeature( poSrcFeature, TRUE );
            delete poSrcFeature;
        }

        if( poFeature == NULL )
            return NULL;

        if( (eGeometryStyle == VGS_Direct || m_poFilterGeom == NULL
            || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature )) )
            return poFeature;

        delete poFeature;
    }
}

/************************************************************************/
/*                          ClipAndAssignSRS()                          */
/************************************************************************/

void OGRVRTLayer::ClipAndAssignSRS(OGRFeature* poFeature)
{
    /* Clip the geometry to the SrcRegion if asked */
    if (poSrcRegion != NULL && bSrcClip && poFeature->GetGeometryRef() != NULL)
    {
        OGRGeometry* poClippedGeom = poFeature->GetGeometryRef()->Intersection(poSrcRegion);
        poFeature->SetGeometryDirectly(poClippedGeom);
    }

    if (poFeature->GetGeometryRef() != NULL && poSRS != NULL)
        poFeature->GetGeometryRef()->assignSpatialReference(poSRS);
}

/************************************************************************/
/*                          TranslateFeature()                          */
/*                                                                      */
/*      Translate a source feature into a feature for this layer.       */
/************************************************************************/

OGRFeature *OGRVRTLayer::TranslateFeature( OGRFeature*& poSrcFeat, int bUseSrcRegion )

{
retry:
    OGRFeature *poDstFeat = new OGRFeature( poFeatureDefn );

    m_nFeaturesRead++;

/* -------------------------------------------------------------------- */
/*      Handle FID.                                                     */
/* -------------------------------------------------------------------- */
    if( iFIDField == -1 )
        poDstFeat->SetFID( poSrcFeat->GetFID() );
    else
        poDstFeat->SetFID( poSrcFeat->GetFieldAsInteger( iFIDField ) );
    
/* -------------------------------------------------------------------- */
/*      Handle style string.                                            */
/* -------------------------------------------------------------------- */
    if( iStyleField != -1 )
    {
        if( poSrcFeat->IsFieldSet(iStyleField) )
            poDstFeat->SetStyleString( 
                poSrcFeat->GetFieldAsString(iStyleField) );
    }
    else
    {
        if( poSrcFeat->GetStyleString() != NULL )
            poDstFeat->SetStyleString(poSrcFeat->GetStyleString());
    }
    
/* -------------------------------------------------------------------- */
/*      Handle the geometry.  Eventually there will be several more     */
/*      supported options.                                              */
/* -------------------------------------------------------------------- */
    if( eGeometryStyle == VGS_None )
    {
        /* do nothing */
    }
    else if( eGeometryStyle == VGS_WKT )
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
    else if( eGeometryStyle == VGS_WKB )
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
    else if( eGeometryStyle == VGS_Shape )
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
    else if( eGeometryStyle == VGS_Direct )
    {
        poDstFeat->SetGeometry( poSrcFeat->GetGeometryRef() );
    }
    else if( eGeometryStyle == VGS_PointFromColumns )
    {
        if( iGeomZField != -1 )
            poDstFeat->SetGeometryDirectly( 
                new OGRPoint( poSrcFeat->GetFieldAsDouble( iGeomXField ),
                              poSrcFeat->GetFieldAsDouble( iGeomYField ),
                              poSrcFeat->GetFieldAsDouble( iGeomZField ) ) );
        else
            poDstFeat->SetGeometryDirectly( 
                new OGRPoint( poSrcFeat->GetFieldAsDouble( iGeomXField ),
                              poSrcFeat->GetFieldAsDouble( iGeomYField ) ) );
    }
    else
        /* add other options here. */;

    /* In the non direct case, we need to check that the geometry intersects the source */
    /* region before an optionnal clipping */
    if( bUseSrcRegion && eGeometryStyle != VGS_Direct && poSrcRegion != NULL )
    {
        OGRGeometry* poGeom = poDstFeat->GetGeometryRef();
        if (poGeom != NULL && !poGeom->Intersects(poSrcRegion))
        {
            delete poSrcFeat;
            delete poDstFeat;

            /* Fetch next source feature and retry translating it */
            poSrcFeat = poSrcLayer->GetNextFeature();
            if (poSrcFeat == NULL)
                return NULL;

            goto retry;
        }
    }

    ClipAndAssignSRS(poDstFeat);

/* -------------------------------------------------------------------- */
/*      Copy fields.                                                    */
/* -------------------------------------------------------------------- */
    int iVRTField;

    for( iVRTField = 0; iVRTField < poFeatureDefn->GetFieldCount(); iVRTField++ )
    {
        if( anSrcField[iVRTField] == -1 )
            continue;

        OGRFieldDefn *poDstDefn = poFeatureDefn->GetFieldDefn( iVRTField );
        OGRFieldDefn *poSrcDefn = poSrcLayer->GetLayerDefn()->GetFieldDefn( anSrcField[iVRTField] );

        if( !poSrcFeat->IsFieldSet( anSrcField[iVRTField] ) )
            continue;

        if( abDirectCopy[iVRTField] 
            && poDstDefn->GetType() == poSrcDefn->GetType() )
        {
            poDstFeat->SetField( iVRTField,
                                 poSrcFeat->GetRawFieldRef( anSrcField[iVRTField] ) );
        }
        else
        {
            /* Eventually we need to offer some more sophisticated translation
               options here for more esoteric types. */
            if (poDstDefn->GetType() == OFTReal)
                poDstFeat->SetField( iVRTField, 
                                 poSrcFeat->GetFieldAsDouble(anSrcField[iVRTField]));
            else
                poDstFeat->SetField( iVRTField, 
                                 poSrcFeat->GetFieldAsString(anSrcField[iVRTField]));
        }
    }

    return poDstFeat;
}


/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRVRTLayer::GetFeature( long nFeatureId )

{
    if (!bHasFullInitialized) FullInitialize();
    if (!poSrcLayer) return NULL;

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
        const char* pszFID = poSrcLayer->GetLayerDefn()->GetFieldDefn(iFIDField)->GetNameRef();
        char* pszFIDQuery = (char*)CPLMalloc(strlen(pszFID) + 64);

        poSrcLayer->ResetReading();
        sprintf( pszFIDQuery, "%s = %ld", pszFID, nFeatureId );
        poSrcLayer->SetSpatialFilter( NULL );
        poSrcLayer->SetAttributeFilter( pszFIDQuery );
        CPLFree(pszFIDQuery);
        
        poSrcFeature = poSrcLayer->GetNextFeature();
    }

    if( poSrcFeature == NULL )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Translate feature and return it.                                */
/* -------------------------------------------------------------------- */
    if (poFeatureDefn == poSrcFeatureDefn)
    {
        poFeature = poSrcFeature;
        ClipAndAssignSRS(poFeature);
    }
    else
    {
        poFeature = TranslateFeature( poSrcFeature, FALSE );
        delete poSrcFeature;
    }

    return poFeature;
}

/************************************************************************/
/*                          SetNextByIndex()                            */
/************************************************************************/

OGRErr OGRVRTLayer::SetNextByIndex( long nIndex )
{
    if (!bHasFullInitialized) FullInitialize();
    if (!poSrcLayer) return OGRERR_FAILURE;

    if (TestCapability(OLCFastSetNextByIndex))
        return poSrcLayer->SetNextByIndex(nIndex);

    return OGRLayer::SetNextByIndex(nIndex);
}

/************************************************************************/
/*               TranslateVRTFeatureToSrcFeature()                      */
/*                                                                      */
/*      Translate a VRT feature into a feature for the source layer     */
/************************************************************************/

OGRFeature* OGRVRTLayer::TranslateVRTFeatureToSrcFeature( OGRFeature* poVRTFeature)
{
    OGRFeature *poSrcFeat = new OGRFeature( poSrcLayer->GetLayerDefn() );

    poSrcFeat->SetFID( poVRTFeature->GetFID() );

/* -------------------------------------------------------------------- */
/*      Handle style string.                                            */
/* -------------------------------------------------------------------- */
    if( iStyleField != -1 )
    {
        if( poVRTFeature->GetStyleString() != NULL )
            poSrcFeat->SetField( iStyleField, poVRTFeature->GetStyleString() );
    }
    else
    {
        if( poVRTFeature->GetStyleString() != NULL )
            poSrcFeat->SetStyleString(poVRTFeature->GetStyleString());
    }
    
/* -------------------------------------------------------------------- */
/*      Handle the geometry.  Eventually there will be several more     */
/*      supported options.                                              */
/* -------------------------------------------------------------------- */
    if( eGeometryStyle == VGS_None )
    {
        /* do nothing */
    }
    else if( eGeometryStyle == VGS_WKT )
    {
        OGRGeometry* poGeom = poVRTFeature->GetGeometryRef();
        if (poGeom != NULL)
        {
            char* pszWKT = NULL;
            if (poGeom->exportToWkt(&pszWKT) == OGRERR_NONE)
            {
                poSrcFeat->SetField(iGeomField, pszWKT);
            }
            CPLFree(pszWKT);
        }
    }
    else if( eGeometryStyle == VGS_WKB )
    {
        OGRGeometry* poGeom = poVRTFeature->GetGeometryRef();
        if (poGeom != NULL)
        {
            int nSize = poGeom->WkbSize();
            GByte* pabyData = (GByte*)CPLMalloc(nSize);
            if (poGeom->exportToWkb(wkbNDR, pabyData) == OGRERR_NONE)
            {
                if ( poSrcFeat->GetFieldDefnRef(iGeomField)->GetType() == OFTBinary )
                {
                    poSrcFeat->SetField(iGeomField, nSize, pabyData);
                }
                else
                {
                    char* pszHexWKB = CPLBinaryToHex(nSize, pabyData);
                    poSrcFeat->SetField(iGeomField, pszHexWKB);
                    CPLFree(pszHexWKB);
                }
            }
            CPLFree(pabyData);
        }
    }
    else if( eGeometryStyle == VGS_Shape )
    {
        CPLDebug("OGR_VRT", "Update of VGS_Shape geometries not supported");
    }
    else if( eGeometryStyle == VGS_Direct )
    {
        poSrcFeat->SetGeometry( poVRTFeature->GetGeometryRef() );
    }
    else if( eGeometryStyle == VGS_PointFromColumns )
    {
        OGRGeometry* poGeom = poVRTFeature->GetGeometryRef();
        if (poGeom != NULL)
        {
            if (wkbFlatten(poGeom->getGeometryType()) != wkbPoint)
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                         "Cannot set a non ponctual geometry for PointFromColumns geometry");
            }
            else
            {
                poSrcFeat->SetField( iGeomXField, ((OGRPoint*)poGeom)->getX() );
                poSrcFeat->SetField( iGeomYField, ((OGRPoint*)poGeom)->getY() );
                if( iGeomZField != -1 )
                {
                    poSrcFeat->SetField( iGeomZField, ((OGRPoint*)poGeom)->getZ() );
                }
            }
        }
    }
    else
        /* add other options here. */;

    if (poSrcFeat->GetGeometryRef() != NULL && poSRS != NULL)
        poSrcFeat->GetGeometryRef()->assignSpatialReference(poSRS);

/* -------------------------------------------------------------------- */
/*      Copy fields.                                                    */
/* -------------------------------------------------------------------- */

    int iVRTField;

    for( iVRTField = 0; iVRTField < poFeatureDefn->GetFieldCount(); iVRTField++ )
    {
        /* Do not set source geometry columns. Have been set just above */
        if (anSrcField[iVRTField] == iGeomField || anSrcField[iVRTField] == iGeomXField ||
            anSrcField[iVRTField] == iGeomYField || anSrcField[iVRTField] == iGeomZField)
            continue;

        OGRFieldDefn *poVRTDefn = poFeatureDefn->GetFieldDefn( iVRTField );
        OGRFieldDefn *poSrcDefn = poSrcLayer->GetLayerDefn()->GetFieldDefn( anSrcField[iVRTField] );

        if( abDirectCopy[iVRTField] 
            && poVRTDefn->GetType() == poSrcDefn->GetType() )
        {
            poSrcFeat->SetField( anSrcField[iVRTField],
                                 poVRTFeature->GetRawFieldRef( iVRTField ) );
        }
        else
        {
            /* Eventually we need to offer some more sophisticated translation
               options here for more esoteric types. */
            poSrcFeat->SetField( anSrcField[iVRTField], 
                                 poVRTFeature->GetFieldAsString(iVRTField));
        }
    }

    return poSrcFeat;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRVRTLayer::CreateFeature( OGRFeature* poVRTFeature )
{
    if (!bHasFullInitialized) FullInitialize();
    if (!poSrcLayer) return OGRERR_FAILURE;

    if(!bUpdate)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "CreateFeature");
        return OGRERR_FAILURE;
    }

    if( iFIDField != -1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
            "The CreateFeature() operation is not supported if the FID option is specified." );
        return OGRERR_FAILURE;
    }

    if( poSrcFeatureDefn == poFeatureDefn )
        return poSrcLayer->CreateFeature(poVRTFeature);

    OGRFeature* poSrcFeature = TranslateVRTFeatureToSrcFeature(poVRTFeature);
    poSrcFeature->SetFID(OGRNullFID);
    OGRErr eErr = poSrcLayer->CreateFeature(poSrcFeature);
    if (eErr == OGRERR_NONE)
    {
        poVRTFeature->SetFID(poSrcFeature->GetFID());
    }
    delete poSrcFeature;
    return eErr;
}

/************************************************************************/
/*                             SetFeature()                             */
/************************************************************************/

OGRErr OGRVRTLayer::SetFeature( OGRFeature* poVRTFeature )
{
    if (!bHasFullInitialized) FullInitialize();
    if (!poSrcLayer) return OGRERR_FAILURE;

    if(!bUpdate)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "SetFeature");
        return OGRERR_FAILURE;
    }

    if( iFIDField != -1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
            "The SetFeature() operation is not supported if the FID option is specified." );
        return OGRERR_FAILURE;
    }

    if( poSrcFeatureDefn == poFeatureDefn )
        return poSrcLayer->SetFeature(poVRTFeature);

    OGRFeature* poSrcFeature = TranslateVRTFeatureToSrcFeature(poVRTFeature);
    OGRErr eErr = poSrcLayer->SetFeature(poSrcFeature);
    delete poSrcFeature;
    return eErr;
}

/************************************************************************/
/*                           DeleteFeature()                            */
/************************************************************************/

OGRErr OGRVRTLayer::DeleteFeature( long nFID )

{
    if (!bHasFullInitialized) FullInitialize();
    if (!poSrcLayer) return OGRERR_FAILURE;

    if(!bUpdate )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "DeleteFeature");
        return OGRERR_FAILURE;
    }

    if( iFIDField != -1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
            "The DeleteFeature() operation is not supported if the FID option is specified." );
        return OGRERR_FAILURE;
    }

    return poSrcLayer->DeleteFeature(nFID);
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRVRTLayer::SetAttributeFilter( const char *pszNewQuery )

{
    if (!bHasFullInitialized) FullInitialize();
    if (!poSrcLayer) return OGRERR_FAILURE;

    if( bAttrFilterPassThrough )
    {
        CPLFree( pszAttrFilter );
        if( pszNewQuery == NULL || strlen(pszNewQuery) == 0 )
            pszAttrFilter = NULL;
        else
            pszAttrFilter = CPLStrdup( pszNewQuery );

        ResetReading();
        return OGRERR_NONE;
    }
    else
    {
        /* setup m_poAttrQuery */
        return OGRLayer::SetAttributeFilter( pszNewQuery );
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRVRTLayer::TestCapability( const char * pszCap )

{
    if (!bHasFullInitialized) FullInitialize();
    if (!poSrcLayer) return FALSE;

    if ( EQUAL(pszCap,OLCFastFeatureCount) ||
         EQUAL(pszCap,OLCFastSetNextByIndex) )
        return (eGeometryStyle == VGS_Direct ||
               (poSrcRegion == NULL && m_poFilterGeom == NULL)) &&
               m_poAttrQuery == NULL &&
               poSrcLayer->TestCapability(pszCap);

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return eGeometryStyle == VGS_Direct && m_poAttrQuery == NULL &&
               poSrcLayer->TestCapability(pszCap);

    else if ( EQUAL(pszCap,OLCFastGetExtent) )
        return eGeometryStyle == VGS_Direct &&
               m_poAttrQuery == NULL &&
               poSrcRegion == NULL &&
               poSrcLayer->TestCapability(pszCap);

    else if( EQUAL(pszCap,OLCRandomRead) )
        return iFIDField == -1 && poSrcLayer->TestCapability(pszCap);

    else if( EQUAL(pszCap,OLCSequentialWrite) 
             || EQUAL(pszCap,OLCRandomWrite)
             || EQUAL(pszCap,OLCDeleteFeature) )
        return bUpdate && iFIDField == -1 && poSrcLayer->TestCapability(pszCap);

    else if( EQUAL(pszCap,OLCStringsAsUTF8) )
        return poSrcLayer->TestCapability(pszCap);

    else if( EQUAL(pszCap,OLCTransactions) )
        return bUpdate && poSrcLayer->TestCapability(pszCap);

    return FALSE;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRVRTLayer::GetSpatialRef()

{
    if (CPLGetXMLValue( psLTree, "LayerSRS", NULL ) != NULL)
        return poSRS;

    if (!bHasFullInitialized) FullInitialize();
    if (!poSrcLayer) return NULL;

    return poSRS;
}

/************************************************************************/
/*                              GetExtent()                             */
/************************************************************************/

OGRErr OGRVRTLayer::GetExtent( OGREnvelope *psExtent, int bForce )
{
    if (!bHasFullInitialized) FullInitialize();
    if (!poSrcLayer) return OGRERR_FAILURE;

    if ( eGeometryStyle == VGS_Direct &&
         m_poAttrQuery == NULL &&
         poSrcRegion == NULL )
    {
        if( bNeedReset )
            ResetSourceReading();

        return poSrcLayer->GetExtent(psExtent, bForce);
    }

    return OGRLayer::GetExtent(psExtent, bForce);
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRVRTLayer::GetFeatureCount( int bForce )

{
    if (!bHasFullInitialized) FullInitialize();
    if (!poSrcLayer) return 0;

    if ((eGeometryStyle == VGS_Direct ||
         (poSrcRegion == NULL && m_poFilterGeom == NULL)) &&
         m_poAttrQuery == NULL )
    {
        if( bNeedReset )
            ResetSourceReading();

        return poSrcLayer->GetFeatureCount( bForce );
    }
    else
        return OGRLayer::GetFeatureCount( bForce );
}


/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRVRTLayer::SetSpatialFilter( OGRGeometry * poGeomIn )
{
    if (!bHasFullInitialized) FullInitialize();
    if (!poSrcLayer) return;

    if (eGeometryStyle == VGS_Direct)
        bNeedReset = TRUE;
    OGRLayer::SetSpatialFilter(poGeomIn);
}

/************************************************************************/
/*                             SyncToDisk()                             */
/************************************************************************/

OGRErr OGRVRTLayer::SyncToDisk()
{
    if (!bHasFullInitialized) FullInitialize();
    if (!poSrcLayer) return OGRERR_FAILURE;

    return poSrcLayer->SyncToDisk();
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

/************************************************************************/
/*                            GetLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn *OGRVRTLayer::GetLayerDefn()
{
    if (!bHasFullInitialized) FullInitialize();

    return poFeatureDefn;
}

/************************************************************************/
/*                             GetGeomType()                            */
/************************************************************************/

OGRwkbGeometryType OGRVRTLayer::GetGeomType()
{
    if (CPLGetXMLValue( psLTree, "GeometryType", NULL ) != NULL)
        return eGeomType;

    return GetLayerDefn()->GetGeomType();
}

/************************************************************************/
/*                             GetFIDColumn()                           */
/************************************************************************/

const char * OGRVRTLayer::GetFIDColumn()
{
    if (!bHasFullInitialized) FullInitialize();
    if (!poSrcLayer) return "";

    const char* pszFIDColumn;
    if (iFIDField == -1)
    {
        /* If pass-through, then query the source layer FID column */
        pszFIDColumn = poSrcLayer->GetFIDColumn();
        if (pszFIDColumn == NULL || EQUAL(pszFIDColumn, ""))
            return "";
    }
    else
    {
        /* Otherwise get the name from the index in the source layer definition */
        OGRFieldDefn* poFDefn = GetSrcLayerDefn()->GetFieldDefn(iFIDField);
        pszFIDColumn = poFDefn->GetNameRef();
    }

    /* Check that the FIDColumn is actually reported in the VRT layer definition */
    if (GetLayerDefn()->GetFieldIndex(pszFIDColumn) != -1)
        return pszFIDColumn;
    else
        return "";
}

/************************************************************************/
/*                           StartTransaction()                         */
/************************************************************************/

OGRErr OGRVRTLayer::StartTransaction()
{
    if (!bHasFullInitialized) FullInitialize();
    if (!poSrcLayer || !bUpdate) return OGRERR_FAILURE;

    return poSrcLayer->StartTransaction();
}

/************************************************************************/
/*                           CommitTransaction()                        */
/************************************************************************/

OGRErr OGRVRTLayer::CommitTransaction()
{
    if (!bHasFullInitialized) FullInitialize();
    if (!poSrcLayer || !bUpdate) return OGRERR_FAILURE;

    return poSrcLayer->CommitTransaction();
}

/************************************************************************/
/*                          RollbackTransaction()                       */
/************************************************************************/

OGRErr OGRVRTLayer::RollbackTransaction()
{
    if (!bHasFullInitialized) FullInitialize();
    if (!poSrcLayer || !bUpdate) return OGRERR_FAILURE;

    return poSrcLayer->RollbackTransaction();
}
