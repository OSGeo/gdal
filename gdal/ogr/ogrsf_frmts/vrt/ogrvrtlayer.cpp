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
#include "ogrpgeogeometry.h"
#include <string>

CPL_CVSID("$Id$");

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

    nFeatureCount = -1;
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
         int bError;
         eGeomType = OGRVRTGetGeometryType(pszGType, &bError);
         if( bError )
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

/* -------------------------------------------------------------------- */
/*      Set FeatureCount if provided                                    */
/* -------------------------------------------------------------------- */
     const char* pszFeatureCount = CPLGetXMLValue( psLTree, "FeatureCount", NULL );
     if( pszFeatureCount != NULL )
     {
         nFeatureCount = atoi(pszFeatureCount);
     }

/* -------------------------------------------------------------------- */
/*      Set Extent if provided                                          */
/* -------------------------------------------------------------------- */
     const char* pszExtentXMin = CPLGetXMLValue( psLTree, "ExtentXMin", NULL );
     const char* pszExtentYMin = CPLGetXMLValue( psLTree, "ExtentYMin", NULL );
     const char* pszExtentXMax = CPLGetXMLValue( psLTree, "ExtentXMax", NULL );
     const char* pszExtentYMax = CPLGetXMLValue( psLTree, "ExtentYMax", NULL );
     if( pszExtentXMin != NULL && pszExtentYMin != NULL &&
         pszExtentXMax != NULL && pszExtentYMax != NULL )
     {
         sStaticEnvelope.MinX = CPLAtof(pszExtentXMin);
         sStaticEnvelope.MinY = CPLAtof(pszExtentYMin);
         sStaticEnvelope.MaxX = CPLAtof(pszExtentXMax);
         sStaticEnvelope.MaxY = CPLAtof(pszExtentYMax);
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

    if (poDS->GetRecursionDetected())
        return FALSE;

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
            poDS->SetRecursionDetected();
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
                poVRTSrcDS->SetParentDS(poDS);
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Trying to open a VRT from a VRT from a VRT from ... [32 times] a VRT !");

            poDS->SetRecursionDetected();

            OGRVRTDataSource* poParent = poDS->GetParentDS();
            while(poParent != NULL)
            {
                poParent->SetRecursionDetected();
                poParent = poParent->GetParentDS();
            }
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
            goto error;
        }
    }
        
    CPLFree( pszSrcDSName );
    pszSrcDSName = NULL;

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
             int nWidth = atoi(CPLGetXMLValue( psChild, "width", "0" ));
             if (nWidth < 0 || nWidth > 1024)
             {
                CPLError( CE_Failure, CPLE_IllegalArg,
                          "Invalid width for field %s.",
                          pszName );
                goto error;
             }
             oFieldDefn.SetWidth(nWidth);

             int nPrecision = atoi(CPLGetXMLValue( psChild, "precision", "0" ));
             if (nPrecision < 0 || nPrecision > 1024)
             {
                CPLError( CE_Failure, CPLE_IllegalArg,
                          "Invalid precision for field %s.",
                          pszName );
                goto error;
             }
             oFieldDefn.SetPrecision(nPrecision);

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

             if (iSrcField < 0 || (pszArg != NULL && strcmp(pszArg, pszName) != 0))
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

     SetIgnoredFields(NULL);

     return TRUE;

error:
    CPLFree( pszSrcDSName );
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

        osMerged += " AND (";
        osMerged += pszAttrFilter;
        osMerged += ")";

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
    if (!poSrcLayer || poDS->GetRecursionDetected()) return NULL;

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
    if( eGeometryStyle == VGS_None || GetLayerDefn()->IsGeometryIgnored() )
    {
        /* do nothing */
    }
    else if( eGeometryStyle == VGS_WKT && iGeomField != -1 )
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
    else if( eGeometryStyle == VGS_WKB && iGeomField != -1 )
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
    else if( eGeometryStyle == VGS_Shape && iGeomField != -1 )
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

            if( OGRCreateFromShapeBin( pabyWKB, &poGeom, nBytes ) == OGRERR_NONE )
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

        if( !poSrcFeat->IsFieldSet( anSrcField[iVRTField] ) || poDstDefn->IsIgnored() )
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
    if (!poSrcLayer || poDS->GetRecursionDetected()) return NULL;

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
    if (!poSrcLayer || poDS->GetRecursionDetected()) return OGRERR_FAILURE;

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
    if (!poSrcLayer || poDS->GetRecursionDetected()) return OGRERR_FAILURE;

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
    if (!poSrcLayer || poDS->GetRecursionDetected()) return OGRERR_FAILURE;

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
    if (!poSrcLayer || poDS->GetRecursionDetected()) return OGRERR_FAILURE;

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
    if (!poSrcLayer || poDS->GetRecursionDetected()) return OGRERR_FAILURE;

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
    if ( EQUAL(pszCap,OLCFastFeatureCount) &&
         nFeatureCount >= 0 &&
         m_poFilterGeom == NULL && m_poAttrQuery == NULL )
        return TRUE;

    if ( EQUAL(pszCap,OLCFastGetExtent) &&
         sStaticEnvelope.IsInit() )
        return TRUE;

    if (!bHasFullInitialized) FullInitialize();
    if (!poSrcLayer || poDS->GetRecursionDetected()) return FALSE;

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
               (poSrcRegion == NULL || bSrcClip) &&
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

    else if( EQUAL(pszCap,OLCIgnoreFields) )
        return poSrcLayer->TestCapability(pszCap);

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
    if (!poSrcLayer || poDS->GetRecursionDetected()) return NULL;

    return poSRS;
}

/************************************************************************/
/*                              GetExtent()                             */
/************************************************************************/

OGRErr OGRVRTLayer::GetExtent( OGREnvelope *psExtent, int bForce )
{
    if( sStaticEnvelope.IsInit() )
    {
        memcpy(psExtent, &sStaticEnvelope, sizeof(OGREnvelope));
        return OGRERR_NONE;
    }

    if (!bHasFullInitialized) FullInitialize();
    if (!poSrcLayer || poDS->GetRecursionDetected()) return OGRERR_FAILURE;

    if ( eGeometryStyle == VGS_Direct &&
         m_poAttrQuery == NULL &&
         (poSrcRegion == NULL || bSrcClip) )
    {
        if( bNeedReset )
            ResetSourceReading();

        OGRErr eErr = poSrcLayer->GetExtent(psExtent, bForce);
        if( eErr != OGRERR_NONE || poSrcRegion == NULL )
            return eErr;

        OGREnvelope sSrcRegionEnvelope;
        poSrcRegion->getEnvelope(&sSrcRegionEnvelope);

        psExtent->Intersect(sSrcRegionEnvelope);
        return eErr;
    }

    return OGRLayer::GetExtent(psExtent, bForce);
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRVRTLayer::GetFeatureCount( int bForce )

{
    if (nFeatureCount >= 0 &&
        m_poFilterGeom == NULL && m_poAttrQuery == NULL)
    {
        return nFeatureCount;
    }

    if (!bHasFullInitialized) FullInitialize();
    if (!poSrcLayer || poDS->GetRecursionDetected()) return 0;

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
    if (!poSrcLayer || poDS->GetRecursionDetected()) return;

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
    if (!poSrcLayer || poDS->GetRecursionDetected()) return OGRERR_FAILURE;

    return poSrcLayer->SyncToDisk();
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
    if (!poSrcLayer || poDS->GetRecursionDetected()) return "";

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
    if (!poSrcLayer || !bUpdate || poDS->GetRecursionDetected()) return OGRERR_FAILURE;

    return poSrcLayer->StartTransaction();
}

/************************************************************************/
/*                           CommitTransaction()                        */
/************************************************************************/

OGRErr OGRVRTLayer::CommitTransaction()
{
    if (!bHasFullInitialized) FullInitialize();
    if (!poSrcLayer || !bUpdate || poDS->GetRecursionDetected()) return OGRERR_FAILURE;

    return poSrcLayer->CommitTransaction();
}

/************************************************************************/
/*                          RollbackTransaction()                       */
/************************************************************************/

OGRErr OGRVRTLayer::RollbackTransaction()
{
    if (!bHasFullInitialized) FullInitialize();
    if (!poSrcLayer || !bUpdate || poDS->GetRecursionDetected()) return OGRERR_FAILURE;

    return poSrcLayer->RollbackTransaction();
}

/************************************************************************/
/*                          SetIgnoredFields()                          */
/************************************************************************/

OGRErr OGRVRTLayer::SetIgnoredFields( const char **papszFields )
{
    if (!bHasFullInitialized) FullInitialize();
    if (!poSrcLayer || poDS->GetRecursionDetected()) return OGRERR_FAILURE;

    if( !poSrcLayer->TestCapability(OLCIgnoreFields) )
        return OGRERR_FAILURE;

    OGRErr eErr = OGRLayer::SetIgnoredFields(papszFields);
    if( eErr != OGRERR_NONE )
        return eErr;

    const char** papszIter = papszFields;
    char** papszFieldsSrc = NULL;
    OGRFeatureDefn* poSrcFeatureDefn = poSrcLayer->GetLayerDefn();
    while ( papszIter != NULL && *papszIter != NULL )
    {
        const char* pszFieldName = *papszIter;
        if ( EQUAL(pszFieldName, "OGR_GEOMETRY") ||
                EQUAL(pszFieldName, "OGR_STYLE") )
        {
            papszFieldsSrc = CSLAddString(papszFieldsSrc, pszFieldName);
        }
        else
        {
            int iVRTField = GetLayerDefn()->GetFieldIndex(pszFieldName);
            if( iVRTField >= 0 )
            {
                int iSrcField = anSrcField[iVRTField];
                if (iSrcField >= 0)
                {
                    OGRFieldDefn *poSrcDefn = poSrcFeatureDefn->GetFieldDefn( iSrcField );
                    papszFieldsSrc = CSLAddString(papszFieldsSrc, poSrcDefn->GetNameRef());
                }
            }
        }
        papszIter++;
    }

    int* panSrcFieldsUsed = (int*) CPLCalloc(sizeof(int), poSrcFeatureDefn->GetFieldCount());
    for(int iVRTField = 0; iVRTField < GetLayerDefn()->GetFieldCount(); iVRTField++)
    {
        int iSrcField = anSrcField[iVRTField];
        if (iSrcField >= 0)
            panSrcFieldsUsed[iSrcField] = TRUE;
    }
    for(int iSrcField = 0; iSrcField < poSrcFeatureDefn->GetFieldCount(); iSrcField ++)
    {
        if( !panSrcFieldsUsed[iSrcField] )
        {
            OGRFieldDefn *poSrcDefn = poSrcFeatureDefn->GetFieldDefn( iSrcField );
            papszFieldsSrc = CSLAddString(papszFieldsSrc, poSrcDefn->GetNameRef());
        }
    }
    CPLFree(panSrcFieldsUsed);

    eErr = poSrcLayer->SetIgnoredFields((const char**)papszFieldsSrc);

    CSLDestroy(papszFieldsSrc);

    return eErr;
}
