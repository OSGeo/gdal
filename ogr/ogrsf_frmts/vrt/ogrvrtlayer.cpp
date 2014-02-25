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
/*                       OGRVRTGeomFieldProps()                         */
/************************************************************************/

OGRVRTGeomFieldProps::OGRVRTGeomFieldProps()
{
    eGeomType = wkbUnknown;
    bUseSpatialSubquery = FALSE;
    eGeometryStyle = VGS_Direct;
    poSRS = NULL;
    iGeomField = iGeomXField = iGeomYField = iGeomZField = -1;
    bSrcClip = FALSE;
    poSrcRegion = NULL;
    bReportSrcColumn = TRUE;
}

/************************************************************************/
/*                      ~OGRVRTGeomFieldProps()                         */
/************************************************************************/

OGRVRTGeomFieldProps::~OGRVRTGeomFieldProps()
{
    if( poSRS != NULL )
        poSRS->Release();
    if( poSrcRegion != NULL )
        delete poSrcRegion;
}

/************************************************************************/
/*                            OGRVRTLayer()                             */
/************************************************************************/

OGRVRTLayer::OGRVRTLayer(OGRVRTDataSource* poDSIn)

{
    poDS = poDSIn;

    bHasFullInitialized = FALSE;
    psLTree = NULL;

    poFeatureDefn = NULL;
    poSrcLayer = NULL;
    poSrcDS = NULL;
    poSrcFeatureDefn = NULL;

    iFIDField = -1;
    iStyleField = -1;

    pszAttrFilter = NULL;

    bNeedReset = TRUE;
    bSrcLayerFromSQL = FALSE;

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
    
    for(size_t i=0;i<apoGeomFieldProps.size();i++)
        delete apoGeomFieldProps[i];

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
     OGRwkbGeometryType eGeomType = wkbUnknown;
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

     if( eGeomType != wkbNone )
     {
         apoGeomFieldProps.push_back(new OGRVRTGeomFieldProps());
         apoGeomFieldProps[0]->eGeomType = eGeomType;
     }

/* -------------------------------------------------------------------- */
/*      Apply a spatial reference system if provided                    */
/* -------------------------------------------------------------------- */
     const char* pszLayerSRS = CPLGetXMLValue( psLTree, "LayerSRS", NULL );
     if( apoGeomFieldProps.size() != 0 && pszLayerSRS != NULL )
     {
         if( !(EQUAL(pszLayerSRS,"NULL")) )
         {
             OGRSpatialReference oSRS;

             if( oSRS.SetFromUserInput( pszLayerSRS ) != OGRERR_NONE )
             {
                 CPLError( CE_Failure, CPLE_AppDefined,
                           "Failed to import LayerSRS `%s'.", pszLayerSRS );
                 return FALSE;
             }
             apoGeomFieldProps[0]->poSRS = oSRS.Clone();
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
     if( apoGeomFieldProps.size() != 0 &&
         pszExtentXMin != NULL && pszExtentYMin != NULL &&
         pszExtentXMax != NULL && pszExtentYMax != NULL )
     {
         apoGeomFieldProps[0]->sStaticEnvelope.MinX = CPLAtof(pszExtentXMin);
         apoGeomFieldProps[0]->sStaticEnvelope.MinY = CPLAtof(pszExtentYMin);
         apoGeomFieldProps[0]->sStaticEnvelope.MaxX = CPLAtof(pszExtentXMax);
         apoGeomFieldProps[0]->sStaticEnvelope.MaxY = CPLAtof(pszExtentYMax);
     }

     return TRUE;
}

/************************************************************************/
/*                       ParseGeometryField()                           */
/************************************************************************/

int OGRVRTLayer::ParseGeometryField(CPLXMLNode* psNode,
                                    OGRVRTGeomFieldProps* poProps,
                                    const char* pszGType)
{
    const char *pszEncoding = NULL;

    pszEncoding = CPLGetXMLValue( psNode,"GeometryField.encoding", "direct");

    if( EQUAL(pszEncoding,"Direct") )
        poProps->eGeometryStyle = VGS_Direct;
    else if( EQUAL(pszEncoding,"None") )
        poProps->eGeometryStyle = VGS_None;
    else if( EQUAL(pszEncoding,"WKT") )
    {
        if( pszGType == NULL && poProps->eGeomType == wkbNone )
            poProps->eGeomType = wkbUnknown;
        poProps->eGeometryStyle = VGS_WKT;
    }
    else if( EQUAL(pszEncoding,"WKB") )
    {
        if( pszGType == NULL && poProps->eGeomType == wkbNone )
            poProps->eGeomType = wkbUnknown;
        poProps->eGeometryStyle = VGS_WKB;
    }
    else if( EQUAL(pszEncoding,"Shape") )
    {
        if( pszGType == NULL && poProps->eGeomType == wkbNone )
            poProps->eGeomType = wkbUnknown;
        poProps->eGeometryStyle = VGS_Shape;
    }
    else if( EQUAL(pszEncoding,"PointFromColumns") )
    {
        poProps->eGeometryStyle = VGS_PointFromColumns;
        poProps->bUseSpatialSubquery = 
            CSLTestBoolean(
                CPLGetXMLValue(psNode, 
                            "GeometryField.useSpatialSubquery",
                            "TRUE"));

        poProps->iGeomXField = GetSrcLayerDefn()->GetFieldIndex(
            CPLGetXMLValue( psNode, "GeometryField.x", "missing" ) );
        poProps->iGeomYField = GetSrcLayerDefn()->GetFieldIndex(
            CPLGetXMLValue( psNode, "GeometryField.y", "missing" ) );
        poProps->iGeomZField = GetSrcLayerDefn()->GetFieldIndex(
            CPLGetXMLValue( psNode, "GeometryField.z", "missing" ) );

        if( poProps->iGeomXField == -1 || poProps->iGeomYField == -1 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                    "Unable to identify source X or Y field for PointFromColumns encoding." );
            return FALSE;
        }

        if( pszGType == NULL )
        {
            if( poProps->iGeomZField != -1 )
                poProps->eGeomType = wkbPoint25D;
            else
                poProps->eGeomType = wkbPoint;
        }
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                "encoding=\"%s\" not recognised.", pszEncoding );
        return FALSE;
    }

    if( poProps->eGeometryStyle == VGS_WKT
        || poProps->eGeometryStyle == VGS_WKB
        || poProps->eGeometryStyle == VGS_Shape )
    {
        const char *pszFieldName = 
            CPLGetXMLValue( psNode, "GeometryField.field", "missing" );

        poProps->iGeomField = GetSrcLayerDefn()->GetFieldIndex(pszFieldName);

        if( poProps->iGeomField == -1 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                    "Unable to identify source field '%s' for geometry.",
                    pszFieldName );
            return FALSE;
        }
    }

    poProps->bReportSrcColumn =
             CSLTestBoolean(CPLGetXMLValue( psNode, "GeometryField.reportSrcColumn", "YES" ));

/* -------------------------------------------------------------------- */
/*      Do we have a SrcRegion?                                         */
/* -------------------------------------------------------------------- */
    const char *pszSrcRegion = CPLGetXMLValue( psNode, "SrcRegion", NULL );
    if( pszSrcRegion != NULL )
    {
        OGRGeometryFactory::createFromWkt( (char**) &pszSrcRegion, NULL, &poProps->poSrcRegion );
        if( poProps->poSrcRegion == NULL || wkbFlatten(poProps->poSrcRegion->getGeometryType()) != wkbPolygon)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                        "Ignoring SrcRegion. It must be a valid WKT polygon");
            delete poProps->poSrcRegion;
            poProps->poSrcRegion = NULL;
        }

        poProps->bSrcClip = CSLTestBoolean(CPLGetXMLValue( psNode, "SrcRegion.clip", "FALSE" ));
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
    const char *pszGType = NULL;
    const char *pszLayerSRS = NULL;
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

/* -------------------------------------------------------------------- */
/*      Is this layer derived from an SQL query result?                 */
/* -------------------------------------------------------------------- */
    pszSQL = CPLGetXMLValue( psLTree, "SrcSQL", NULL );

    if( pszSQL != NULL )
    {
        const char* pszDialect = CPLGetXMLValue( psLTree, "SrcSQL.dialect", NULL );
        if( pszDialect != NULL && pszDialect[0] == '\0' )
            pszDialect = NULL;
        poSrcLayer = poSrcDS->ExecuteSQL( pszSQL, NULL, pszDialect );
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
         apoGeomFieldProps[0]->eGeomType = poSrcLayer->GetGeomType();
     }

/* -------------------------------------------------------------------- */
/*      Copy spatial reference system from source if not provided       */
/* -------------------------------------------------------------------- */
     pszLayerSRS = CPLGetXMLValue( psLTree, "LayerSRS", NULL );
     if( pszLayerSRS == NULL && apoGeomFieldProps.size() != 0 )
     {
         CPLAssert(apoGeomFieldProps[0]->poSRS == NULL);
         if( poSrcLayer->GetSpatialRef() != NULL )
             apoGeomFieldProps[0]->poSRS = poSrcLayer->GetSpatialRef()->Clone();
         else
             apoGeomFieldProps[0]->poSRS = NULL;
     }

/* -------------------------------------------------------------------- */
/*      Handle GeometryField.                                           */
/* -------------------------------------------------------------------- */

     if( apoGeomFieldProps.size() != 0 )
     {
         if( !ParseGeometryField( psLTree, apoGeomFieldProps[0], pszGType ) )
             goto error;

         poFeatureDefn->SetGeomType( apoGeomFieldProps[0]->eGeomType );
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
             if (nWidth < 0)
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
         int iSrcField;
         int nSrcFieldCount = GetSrcLayerDefn()->GetFieldCount();

         for( iSrcField = 0; iSrcField < nSrcFieldCount; iSrcField++ )
         {
             int bSkip = FALSE;
             for( size_t iGF = 0; iGF < apoGeomFieldProps.size(); iGF++ )
             {
                if( apoGeomFieldProps[iGF]->bReportSrcColumn == FALSE &&
                    (iSrcField == apoGeomFieldProps[iGF]->iGeomXField || iSrcField == apoGeomFieldProps[iGF]->iGeomYField ||
                     iSrcField == apoGeomFieldProps[iGF]->iGeomZField || iSrcField == apoGeomFieldProps[iGF]->iGeomField) )
                {
                    bSkip = TRUE;
                    break;
                }
             }
             if( bSkip )
                 continue;

             poFeatureDefn->AddFieldDefn( GetSrcLayerDefn()->GetFieldDefn( iSrcField ) );
             anSrcField.push_back( iSrcField );
             abDirectCopy.push_back( TRUE );
         }

         bAttrFilterPassThrough = TRUE;
     }

/* -------------------------------------------------------------------- */
/*      Is VRT layer definition identical to the source layer defn ?    */
/*      If so, use it directly, and save the translation of features.   */
/* -------------------------------------------------------------------- */
     if( poFeatureDefn->GetGeomFieldCount() != 0 && apoGeomFieldProps.size() != 0 )
         poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(apoGeomFieldProps[0]->poSRS);

     if (poSrcFeatureDefn != NULL && iFIDField == -1 && iStyleField == -1 &&
         apoGeomFieldProps.size() == 1 &&
         apoGeomFieldProps[0]->eGeometryStyle == VGS_Direct &&
         poSrcFeatureDefn->IsSame(poFeatureDefn))
     {
        CPLDebug("VRT", "Source feature definition is identical to VRT feature definition. Use optimized path");
        poFeatureDefn->Release();
        poFeatureDefn = poSrcFeatureDefn;
        poFeatureDefn->Reference();
        if ( poFeatureDefn->GetGeomFieldCount() != 0 )
        {
            if( apoGeomFieldProps[0]->poSRS != NULL )
                apoGeomFieldProps[0]->poSRS->Release();
            apoGeomFieldProps[0]->poSRS = poFeatureDefn->GetGeomFieldDefn(0)->GetSpatialRef();
            if( apoGeomFieldProps[0]->poSRS != NULL )
                apoGeomFieldProps[0]->poSRS->Reference();
        }
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
    if( apoGeomFieldProps.size() == 1 &&
        (m_poFilterGeom || apoGeomFieldProps[0]->poSrcRegion) && apoGeomFieldProps[0]->bUseSpatialSubquery &&
         apoGeomFieldProps[0]->eGeometryStyle == VGS_PointFromColumns )
    {
        const char *pszXField, *pszYField;

        pszXField = poSrcLayer->GetLayerDefn()->GetFieldDefn(apoGeomFieldProps[0]->iGeomXField)->GetNameRef();
        pszYField = poSrcLayer->GetLayerDefn()->GetFieldDefn(apoGeomFieldProps[0]->iGeomYField)->GetNameRef();
        if (apoGeomFieldProps[0]->bUseSpatialSubquery)
        {
            OGRFieldType xType = poSrcLayer->GetLayerDefn()->GetFieldDefn(apoGeomFieldProps[0]->iGeomXField)->GetType();
            OGRFieldType yType = poSrcLayer->GetLayerDefn()->GetFieldDefn(apoGeomFieldProps[0]->iGeomYField)->GetType();
            if (!((xType == OFTReal || xType == OFTInteger) && (yType == OFTReal || yType == OFTInteger)))
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                        "The '%s' and/or '%s' fields of the source layer are not declared as numeric fields,\n"
                        "so the spatial filter cannot be turned into an attribute filter on them",
                         pszXField, pszYField);
                apoGeomFieldProps[0]->bUseSpatialSubquery = FALSE;
            }
        }
        if (apoGeomFieldProps[0]->bUseSpatialSubquery)
        {
            OGREnvelope sEnvelope;

            pszFilter = (char *) 
                CPLMalloc(2*strlen(pszXField)+2*strlen(pszYField) + 100);

            if (apoGeomFieldProps[0]->poSrcRegion != NULL)
            {
                if (m_poFilterGeom == NULL)
                    apoGeomFieldProps[0]->poSrcRegion->getEnvelope( &sEnvelope );
                else
                {
                    OGRGeometry* poIntersection = apoGeomFieldProps[0]->poSrcRegion->Intersection(m_poFilterGeom);
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
    if (apoGeomFieldProps.size() == 1 &&
        apoGeomFieldProps[0]->eGeometryStyle == VGS_Direct)
    {
        if (apoGeomFieldProps[0]->poSrcRegion == NULL)
            poSrcLayer->SetSpatialFilter( m_poFilterGeom );
        else if (m_poFilterGeom == NULL)
            poSrcLayer->SetSpatialFilter( apoGeomFieldProps[0]->poSrcRegion );
        else
        {
            if( wkbFlatten(m_poFilterGeom->getGeometryType()) != wkbPolygon )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Spatial filter should be polygon when a SrcRegion is defined. Ignoring it");
                poSrcLayer->SetSpatialFilter( apoGeomFieldProps[0]->poSrcRegion );
            }
            else
            {
                OGRGeometry* poIntersection = m_poFilterGeom->Intersection(apoGeomFieldProps[0]->poSrcRegion);
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

        if( ((apoGeomFieldProps.size() == 1 && apoGeomFieldProps[0]->eGeometryStyle == VGS_Direct)
            || m_poFilterGeom == NULL
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
    for(int i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
    {
        /* Clip the geometry to the SrcRegion if asked */
        if (apoGeomFieldProps[i]->poSrcRegion != NULL &&
            apoGeomFieldProps[i]->bSrcClip &&
            poFeature->GetGeomFieldRef(i) != NULL)
        {
            OGRGeometry* poClippedGeom = poFeature->GetGeomFieldRef(i)->Intersection(apoGeomFieldProps[i]->poSrcRegion);
            poFeature->SetGeomFieldDirectly(i, poClippedGeom);
        }

        if (poFeature->GetGeomFieldRef(i) != NULL && apoGeomFieldProps[i]->poSRS != NULL)
            poFeature->GetGeomFieldRef(i)->assignSpatialReference(apoGeomFieldProps[i]->poSRS);
    }
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
    
    for(int i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
    {
    /* -------------------------------------------------------------------- */
    /*      Handle the geometry.  Eventually there will be several more     */
    /*      supported options.                                              */
    /* -------------------------------------------------------------------- */
        if( apoGeomFieldProps[i]->eGeometryStyle == VGS_None ||
            GetLayerDefn()->GetGeomFieldDefn(i)->IsIgnored() )
        {
            /* do nothing */
        }
        else if( apoGeomFieldProps[i]->eGeometryStyle == VGS_WKT &&
                 apoGeomFieldProps[i]->iGeomField != -1 )
        {
            char *pszWKT = (char *) poSrcFeat->GetFieldAsString(
                apoGeomFieldProps[i]->iGeomField );
            
            if( pszWKT != NULL )
            {
                OGRGeometry *poGeom = NULL;

                OGRGeometryFactory::createFromWkt( &pszWKT, NULL, &poGeom );
                if( poGeom == NULL )
                    CPLDebug( "OGR_VRT", "Did not get geometry from %s",
                            pszWKT );

                poDstFeat->SetGeomFieldDirectly( i, poGeom );
            }
        }
        else if( apoGeomFieldProps[i]->eGeometryStyle == VGS_WKB &&
                 apoGeomFieldProps[i]->iGeomField != -1 )
        {
            int nBytes;
            GByte *pabyWKB;
            int bNeedFree = FALSE;

            if( poSrcFeat->GetFieldDefnRef(apoGeomFieldProps[i]->iGeomField)->GetType() == OFTBinary )
            {
                pabyWKB = poSrcFeat->GetFieldAsBinary( apoGeomFieldProps[i]->iGeomField, &nBytes );
            }
            else
            {
                const char *pszWKT = poSrcFeat->GetFieldAsString( apoGeomFieldProps[i]->iGeomField );

                pabyWKB = CPLHexToBinary( pszWKT, &nBytes );
                bNeedFree = TRUE;
            }
            
            if( pabyWKB != NULL )
            {
                OGRGeometry *poGeom = NULL;

                if( OGRGeometryFactory::createFromWkb( pabyWKB, NULL, &poGeom,
                                                    nBytes ) == OGRERR_NONE )
                    poDstFeat->SetGeomFieldDirectly( i, poGeom );
            }

            if( bNeedFree )
                CPLFree( pabyWKB );
        }
        else if( apoGeomFieldProps[i]->eGeometryStyle == VGS_Shape &&
                 apoGeomFieldProps[i]->iGeomField != -1 )
        {
            int nBytes;
            GByte *pabyWKB;
            int bNeedFree = FALSE;

            if( poSrcFeat->GetFieldDefnRef(apoGeomFieldProps[i]->iGeomField)->GetType() == OFTBinary )
            {
                pabyWKB = poSrcFeat->GetFieldAsBinary( apoGeomFieldProps[i]->iGeomField, &nBytes );
            }
            else
            {
                const char *pszWKT = poSrcFeat->GetFieldAsString( apoGeomFieldProps[i]->iGeomField );

                pabyWKB = CPLHexToBinary( pszWKT, &nBytes );
                bNeedFree = TRUE;
            }
            
            if( pabyWKB != NULL )
            {
                OGRGeometry *poGeom = NULL;

                if( OGRCreateFromShapeBin( pabyWKB, &poGeom, nBytes ) == OGRERR_NONE )
                    poDstFeat->SetGeomFieldDirectly( i, poGeom );
            }

            if( bNeedFree )
                CPLFree( pabyWKB );
        }
        else if( apoGeomFieldProps[i]->eGeometryStyle == VGS_Direct )
        {
            poDstFeat->SetGeomField( i, poSrcFeat->GetGeomFieldRef(i) );
        }
        else if( apoGeomFieldProps[i]->eGeometryStyle == VGS_PointFromColumns )
        {
            if( apoGeomFieldProps[i]->iGeomZField != -1 )
                poDstFeat->SetGeomFieldDirectly( i,
                    new OGRPoint( poSrcFeat->GetFieldAsDouble( apoGeomFieldProps[i]->iGeomXField ),
                                poSrcFeat->GetFieldAsDouble( apoGeomFieldProps[i]->iGeomYField ),
                                poSrcFeat->GetFieldAsDouble( apoGeomFieldProps[i]->iGeomZField ) ) );
            else
                poDstFeat->SetGeomFieldDirectly( i,
                    new OGRPoint( poSrcFeat->GetFieldAsDouble( apoGeomFieldProps[i]->iGeomXField ),
                                poSrcFeat->GetFieldAsDouble( apoGeomFieldProps[i]->iGeomYField ) ) );
        }
        else
            /* add other options here. */;

        /* In the non direct case, we need to check that the geometry intersects the source */
        /* region before an optionnal clipping */
        if( bUseSrcRegion &&
            apoGeomFieldProps[i]->eGeometryStyle != VGS_Direct &&
            apoGeomFieldProps[i]->poSrcRegion != NULL )
        {
            OGRGeometry* poGeom = poDstFeat->GetGeomFieldRef(i);
            if (poGeom != NULL && !poGeom->Intersects(apoGeomFieldProps[i]->poSrcRegion))
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

    if( bNeedReset )
    {
        if( !ResetSourceReading() )
            return OGRERR_FAILURE;
    }

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
    for(int i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
    {
        if( apoGeomFieldProps[i]->eGeometryStyle == VGS_None )
        {
            /* do nothing */
        }
        else if( apoGeomFieldProps[i]->eGeometryStyle == VGS_WKT )
        {
            OGRGeometry* poGeom = poVRTFeature->GetGeomFieldRef(i);
            if (poGeom != NULL)
            {
                char* pszWKT = NULL;
                if (poGeom->exportToWkt(&pszWKT) == OGRERR_NONE)
                {
                    poSrcFeat->SetField(apoGeomFieldProps[i]->iGeomField, pszWKT);
                }
                CPLFree(pszWKT);
            }
        }
        else if( apoGeomFieldProps[i]->eGeometryStyle == VGS_WKB )
        {
            OGRGeometry* poGeom = poVRTFeature->GetGeomFieldRef(i);
            if (poGeom != NULL)
            {
                int nSize = poGeom->WkbSize();
                GByte* pabyData = (GByte*)CPLMalloc(nSize);
                if (poGeom->exportToWkb(wkbNDR, pabyData) == OGRERR_NONE)
                {
                    if ( poSrcFeat->GetFieldDefnRef(apoGeomFieldProps[i]->iGeomField)->GetType() == OFTBinary )
                    {
                        poSrcFeat->SetField(apoGeomFieldProps[i]->iGeomField, nSize, pabyData);
                    }
                    else
                    {
                        char* pszHexWKB = CPLBinaryToHex(nSize, pabyData);
                        poSrcFeat->SetField(apoGeomFieldProps[i]->iGeomField, pszHexWKB);
                        CPLFree(pszHexWKB);
                    }
                }
                CPLFree(pabyData);
            }
        }
        else if( apoGeomFieldProps[i]->eGeometryStyle == VGS_Shape )
        {
            CPLDebug("OGR_VRT", "Update of VGS_Shape geometries not supported");
        }
        else if( apoGeomFieldProps[i]->eGeometryStyle == VGS_Direct )
        {
            poSrcFeat->SetGeometry( poVRTFeature->GetGeomFieldRef(i) );
        }
        else if( apoGeomFieldProps[i]->eGeometryStyle == VGS_PointFromColumns )
        {
            OGRGeometry* poGeom = poVRTFeature->GetGeomFieldRef(i);
            if (poGeom != NULL)
            {
                if (wkbFlatten(poGeom->getGeometryType()) != wkbPoint)
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                            "Cannot set a non ponctual geometry for PointFromColumns geometry");
                }
                else
                {
                    poSrcFeat->SetField( apoGeomFieldProps[i]->iGeomXField, ((OGRPoint*)poGeom)->getX() );
                    poSrcFeat->SetField( apoGeomFieldProps[i]->iGeomYField, ((OGRPoint*)poGeom)->getY() );
                    if( apoGeomFieldProps[i]->iGeomZField != -1 )
                    {
                        poSrcFeat->SetField( apoGeomFieldProps[i]->iGeomZField, ((OGRPoint*)poGeom)->getZ() );
                    }
                }
            }
        }
        else
            /* add other options here. */;

        if (poSrcFeat->GetGeomFieldRef(i) != NULL && apoGeomFieldProps[i]->poSRS != NULL)
            poSrcFeat->GetGeomFieldRef(i)->assignSpatialReference(apoGeomFieldProps[i]->poSRS);
    }

/* -------------------------------------------------------------------- */
/*      Copy fields.                                                    */
/* -------------------------------------------------------------------- */

    int iVRTField;

    for( iVRTField = 0; iVRTField < poFeatureDefn->GetFieldCount(); iVRTField++ )
    {
        int bSkip = FALSE;
        for(int i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
        {
            /* Do not set source geometry columns. Have been set just above */
            if (anSrcField[iVRTField] == apoGeomFieldProps[i]->iGeomField || anSrcField[iVRTField] == apoGeomFieldProps[i]->iGeomXField ||
                anSrcField[iVRTField] == apoGeomFieldProps[i]->iGeomYField || anSrcField[iVRTField] == apoGeomFieldProps[i]->iGeomZField)
            {
                bSkip = TRUE;
                break;
            }
        }
        if( bSkip )
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
         apoGeomFieldProps.size() == 1 &&
         apoGeomFieldProps[0]->sStaticEnvelope.IsInit() )
        return TRUE;

    if (!bHasFullInitialized) FullInitialize();
    if (!poSrcLayer || poDS->GetRecursionDetected()) return FALSE;

    if ( EQUAL(pszCap,OLCFastFeatureCount) ||
         EQUAL(pszCap,OLCFastSetNextByIndex) )
        return (apoGeomFieldProps.size() == 1 &&
                (apoGeomFieldProps[0]->eGeometryStyle == VGS_Direct ||
               (apoGeomFieldProps[0]->poSrcRegion == NULL && m_poFilterGeom == NULL)) &&
               m_poAttrQuery == NULL &&
               poSrcLayer->TestCapability(pszCap));

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return apoGeomFieldProps.size() == 1 && apoGeomFieldProps[0]->eGeometryStyle == VGS_Direct && m_poAttrQuery == NULL &&
               poSrcLayer->TestCapability(pszCap);

    else if ( EQUAL(pszCap,OLCFastGetExtent) )
        return apoGeomFieldProps.size() == 1 && apoGeomFieldProps[0]->eGeometryStyle == VGS_Direct &&
               m_poAttrQuery == NULL &&
               (apoGeomFieldProps[0]->poSrcRegion == NULL || apoGeomFieldProps[0]->bSrcClip) &&
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
    if (CPLGetXMLValue( psLTree, "LayerSRS", NULL ) != NULL &&
        apoGeomFieldProps.size() == 1)
        return apoGeomFieldProps[0]->poSRS;

    if (!bHasFullInitialized) FullInitialize();
    if (!poSrcLayer || poDS->GetRecursionDetected()) return NULL;

    if( apoGeomFieldProps.size() == 1 )
        return apoGeomFieldProps[0]->poSRS;
    else
        return NULL;
}

/************************************************************************/
/*                              GetExtent()                             */
/************************************************************************/

OGRErr OGRVRTLayer::GetExtent( int iGeomField, OGREnvelope *psExtent, int bForce )
{
    if( iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount() )
        return OGRERR_FAILURE;

    if( apoGeomFieldProps[iGeomField]->sStaticEnvelope.IsInit() )
    {
        memcpy(psExtent, &apoGeomFieldProps[iGeomField]->sStaticEnvelope, sizeof(OGREnvelope));
        return OGRERR_NONE;
    }

    if (!bHasFullInitialized) FullInitialize();
    if (!poSrcLayer || poDS->GetRecursionDetected()) return OGRERR_FAILURE;

    if ( apoGeomFieldProps[iGeomField]->eGeometryStyle == VGS_Direct &&
         m_poAttrQuery == NULL &&
         (apoGeomFieldProps[iGeomField]->poSrcRegion == NULL || apoGeomFieldProps[iGeomField]->bSrcClip) )
    {
        if( bNeedReset )
            ResetSourceReading();

        // FIXME : should be iSrcGeomField
        OGRErr eErr = poSrcLayer->GetExtent(iGeomField, psExtent, bForce);
        if( eErr != OGRERR_NONE || apoGeomFieldProps[iGeomField]->poSrcRegion == NULL )
            return eErr;

        OGREnvelope sSrcRegionEnvelope;
        apoGeomFieldProps[iGeomField]->poSrcRegion->getEnvelope(&sSrcRegionEnvelope);

        psExtent->Intersect(sSrcRegionEnvelope);
        return eErr;
    }

    return OGRLayer::GetExtent(iGeomField, psExtent, bForce);
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

    if (apoGeomFieldProps.size() == 1 &&
        (apoGeomFieldProps[0]->eGeometryStyle == VGS_Direct ||
         (apoGeomFieldProps[0]->poSrcRegion == NULL && m_poFilterGeom == NULL)) &&
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

    if (apoGeomFieldProps.size() == 1 && apoGeomFieldProps[0]->eGeometryStyle == VGS_Direct)
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
    if (CPLGetXMLValue( psLTree, "GeometryType", NULL ) != NULL &&
        apoGeomFieldProps.size() == 1)
        return apoGeomFieldProps[0]->eGeomType;

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
