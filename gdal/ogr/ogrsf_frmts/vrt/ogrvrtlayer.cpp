/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRVRTLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#define UNSUPPORTED_OP_READ_ONLY \
    "%s : unsupported operation on a read-only datasource."

/************************************************************************/
/*                       OGRVRTGeomFieldProps()                         */
/************************************************************************/

OGRVRTGeomFieldProps::OGRVRTGeomFieldProps() :
    eGeomType(wkbUnknown),
    poSRS(NULL),
    bSrcClip(false),
    poSrcRegion(NULL),
    eGeometryStyle(VGS_Direct),
    iGeomField(-1),
    iGeomXField(-1),
    iGeomYField(-1),
    iGeomZField(-1),
    iGeomMField(-1),
    bReportSrcColumn(true),
    bUseSpatialSubquery(false),
    bNullable(true)
{}

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

OGRVRTLayer::OGRVRTLayer( OGRVRTDataSource *poDSIn ) :
    poDS(poDSIn),
    bHasFullInitialized(false),
    psLTree(NULL),
    poFeatureDefn(NULL),
    poSrcDS(NULL),
    poSrcLayer(NULL),
    poSrcFeatureDefn(NULL),
    bNeedReset(true),
    bSrcLayerFromSQL(false),
    bSrcDSShared(false),
    bAttrFilterPassThrough(false),
    pszAttrFilter(NULL),
    iFIDField(-1),  // -1 means pass through.
    iStyleField(-1),  // -1 means pass through.
    bUpdate(false),
    nFeatureCount(-1),
    bError(false)
{}

/************************************************************************/
/*                            ~OGRVRTLayer()                            */
/************************************************************************/

OGRVRTLayer::~OGRVRTLayer()

{
    if( m_nFeaturesRead > 0 && poFeatureDefn != NULL )
    {
        CPLDebug("VRT", "%d features read on layer '%s'.",
                 static_cast<int>(m_nFeaturesRead),
                 poFeatureDefn->GetName());
    }

    for( size_t i = 0; i < apoGeomFieldProps.size(); i++ )
        delete apoGeomFieldProps[i];

    if( poSrcDS != NULL )
    {
        if( poSrcLayer )
        {
            poSrcLayer->SetIgnoredFields(NULL);
            poSrcLayer->SetAttributeFilter(NULL);
            poSrcLayer->SetSpatialFilter(NULL);
        }

        if( bSrcLayerFromSQL && poSrcLayer )
            poSrcDS->ReleaseResultSet(poSrcLayer);

        GDALClose((GDALDatasetH)poSrcDS);
    }

    if( poFeatureDefn )
        poFeatureDefn->Release();

    CPLFree(pszAttrFilter);
}

/************************************************************************/
/*                         GetSrcLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn* OGRVRTLayer::GetSrcLayerDefn()
{
    if( poSrcFeatureDefn )
        return poSrcFeatureDefn;

    if( poSrcLayer )
        poSrcFeatureDefn = poSrcLayer->GetLayerDefn();

    return poSrcFeatureDefn;
}

/************************************************************************/
/*                         FastInitialize()                             */
/************************************************************************/

bool OGRVRTLayer::FastInitialize( CPLXMLNode *psLTreeIn,
                                  const char *pszVRTDirectory,
                                  int bUpdateIn )

{
    psLTree = psLTreeIn;
    bUpdate = CPL_TO_BOOL(bUpdateIn);
    osVRTDirectory = pszVRTDirectory;

    if( !EQUAL(psLTree->pszValue, "OGRVRTLayer") )
        return FALSE;

    // Get layer name.
    const char *pszLayerName = CPLGetXMLValue(psLTree, "name", NULL);

    if( pszLayerName == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing name attribute on OGRVRTLayer");
        return FALSE;
    }

    osName = pszLayerName;
    SetDescription(pszLayerName);

    // Do we have a fixed geometry type?  If so, use it.
    CPLXMLNode *psGeometryFieldNode = CPLGetXMLNode(psLTree, "GeometryField");
    const char *pszGType = CPLGetXMLValue(psLTree, "GeometryType", NULL);
    if( pszGType == NULL && psGeometryFieldNode != NULL )
        pszGType = CPLGetXMLValue(psGeometryFieldNode, "GeometryType", NULL);
    if( pszGType != NULL )
    {
        int l_bError = FALSE;
        const OGRwkbGeometryType eGeomType =
            OGRVRTGetGeometryType(pszGType, &l_bError);
        if( l_bError )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "GeometryType %s not recognised.", pszGType);
            return FALSE;
        }
        if( eGeomType != wkbNone )
        {
            apoGeomFieldProps.push_back(new OGRVRTGeomFieldProps());
            apoGeomFieldProps[0]->eGeomType = eGeomType;
        }
    }

    // Apply a spatial reference system if provided.
    const char *pszLayerSRS = CPLGetXMLValue(psLTree, "LayerSRS", NULL);
    if( pszLayerSRS == NULL && psGeometryFieldNode != NULL )
        pszLayerSRS = CPLGetXMLValue(psGeometryFieldNode, "SRS", NULL);
    if( pszLayerSRS != NULL )
    {
        if( apoGeomFieldProps.empty() )
        {
            apoGeomFieldProps.push_back(new OGRVRTGeomFieldProps());
        }
        if( !(EQUAL(pszLayerSRS, "NULL")) )
        {
            OGRSpatialReference oSRS;

            if( oSRS.SetFromUserInput(pszLayerSRS) != OGRERR_NONE )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to import LayerSRS `%s'.", pszLayerSRS);
                return FALSE;
            }
            apoGeomFieldProps[0]->poSRS = oSRS.Clone();
        }
    }

    // Set FeatureCount if provided.
    const char *pszFeatureCount = CPLGetXMLValue(psLTree, "FeatureCount", NULL);
    if( pszFeatureCount != NULL )
    {
        nFeatureCount = CPLAtoGIntBig(pszFeatureCount);
    }

    // Set Extent if provided.
    const char *pszExtentXMin = CPLGetXMLValue(psLTree, "ExtentXMin", NULL);
    const char *pszExtentYMin = CPLGetXMLValue(psLTree, "ExtentYMin", NULL);
    const char *pszExtentXMax = CPLGetXMLValue(psLTree, "ExtentXMax", NULL);
    const char *pszExtentYMax = CPLGetXMLValue(psLTree, "ExtentYMax", NULL);
    if( pszExtentXMin == NULL && psGeometryFieldNode != NULL )
    {
        pszExtentXMin = CPLGetXMLValue(psGeometryFieldNode, "ExtentXMin", NULL);
        pszExtentYMin = CPLGetXMLValue(psGeometryFieldNode, "ExtentYMin", NULL);
        pszExtentXMax = CPLGetXMLValue(psGeometryFieldNode, "ExtentXMax", NULL);
        pszExtentYMax = CPLGetXMLValue(psGeometryFieldNode, "ExtentYMax", NULL);
    }
    if( pszExtentXMin != NULL && pszExtentYMin != NULL &&
        pszExtentXMax != NULL && pszExtentYMax != NULL )
    {
        if( apoGeomFieldProps.empty() )
        {
            apoGeomFieldProps.push_back(new OGRVRTGeomFieldProps());
        }
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

bool OGRVRTLayer::ParseGeometryField(CPLXMLNode *psNode,
                                     CPLXMLNode *psNodeParent,
                                     OGRVRTGeomFieldProps *poProps)
{
    const char *pszName = CPLGetXMLValue(psNode, "name", NULL);
    poProps->osName = pszName ? pszName : "";
    if( pszName == NULL && apoGeomFieldProps.size() > 1 &&
        poProps != apoGeomFieldProps[0] )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "A 'name' attribute should be defined when there are "
                 "several geometry fields");
    }

    // Do we have a fixed geometry type?
    const char *pszGType = CPLGetXMLValue(psNode, "GeometryType", NULL);
    if( pszGType == NULL && poProps == apoGeomFieldProps[0] )
        pszGType = CPLGetXMLValue(psNodeParent, "GeometryType", NULL);
    if( pszGType != NULL )
    {
        int l_bError = FALSE;
        poProps->eGeomType = OGRVRTGetGeometryType(pszGType, &l_bError);
        if( l_bError )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "GeometryType %s not recognised.", pszGType);
            return false;
        }
    }

    // Determine which field(s) to get the geometry from.
    const char *pszEncoding = CPLGetXMLValue(psNode, "encoding", "direct");

    if( EQUAL(pszEncoding, "Direct") )
        poProps->eGeometryStyle = VGS_Direct;
    else if( EQUAL(pszEncoding, "None") )
        poProps->eGeometryStyle = VGS_None;
    else if( EQUAL(pszEncoding, "WKT") )
        poProps->eGeometryStyle = VGS_WKT;
    else if( EQUAL(pszEncoding, "WKB") )
        poProps->eGeometryStyle = VGS_WKB;
    else if( EQUAL(pszEncoding, "Shape") )
        poProps->eGeometryStyle = VGS_Shape;
    else if( EQUAL(pszEncoding, "PointFromColumns") )
    {
        poProps->eGeometryStyle = VGS_PointFromColumns;
        poProps->bUseSpatialSubquery = CPLTestBool(
            CPLGetXMLValue(psNode, "GeometryField.useSpatialSubquery", "TRUE"));

        poProps->iGeomXField = GetSrcLayerDefn()->GetFieldIndex(
            CPLGetXMLValue(psNode, "x", "missing"));
        poProps->iGeomYField = GetSrcLayerDefn()->GetFieldIndex(
            CPLGetXMLValue(psNode, "y", "missing"));
        poProps->iGeomZField = GetSrcLayerDefn()->GetFieldIndex(
            CPLGetXMLValue(psNode, "z", "missing"));
        poProps->iGeomMField = GetSrcLayerDefn()->GetFieldIndex(
            CPLGetXMLValue(psNode, "m", "missing"));

        if( poProps->iGeomXField == -1 || poProps->iGeomYField == -1 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to identify source X or Y field for "
                     "PointFromColumns encoding.");
            return false;
        }

        if( pszGType == NULL )
        {
            poProps->eGeomType = wkbPoint;
            if( poProps->iGeomZField != -1 )
                poProps->eGeomType = OGR_GT_SetZ(poProps->eGeomType);
            if( poProps->iGeomMField != -1 )
                poProps->eGeomType = OGR_GT_SetM(poProps->eGeomType);
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "encoding=\"%s\" not recognised.",
                 pszEncoding);
        return false;
    }

    if( poProps->eGeometryStyle == VGS_WKT ||
        poProps->eGeometryStyle == VGS_WKB ||
        poProps->eGeometryStyle == VGS_Shape )
    {
        const char *pszFieldName = CPLGetXMLValue(psNode, "field", "missing");

        poProps->iGeomField = GetSrcLayerDefn()->GetFieldIndex(pszFieldName);

        if( poProps->iGeomField == -1 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to identify source field '%s' for geometry.",
                     pszFieldName);
            return false;
        }
    }
    else if( poProps->eGeometryStyle == VGS_Direct )
    {
        const char *pszFieldName = CPLGetXMLValue(psNode, "field", NULL);

        if( pszFieldName != NULL || GetSrcLayerDefn()->GetGeomFieldCount() > 1 )
        {
            if( pszFieldName == NULL )
                pszFieldName = poProps->osName;
            poProps->iGeomField =
                GetSrcLayerDefn()->GetGeomFieldIndex(pszFieldName);

            if( poProps->iGeomField == -1 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unable to identify source geometry field '%s' "
                         "for geometry.",
                         pszFieldName);
                return false;
            }
        }
        else if( GetSrcLayerDefn()->GetGeomFieldCount() == 1 )
        {
            poProps->iGeomField = 0;
        }
        else if( psNode != NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to identify source geometry field.");
            return false;
        }
    }

    poProps->bReportSrcColumn =
        CPLTestBool(CPLGetXMLValue(psNode, "reportSrcColumn", "YES"));

    // Guess geometry type if not explicitly provided (or computed).
    if( pszGType == NULL && poProps->eGeomType == wkbUnknown )
    {
        if( GetSrcLayerDefn()->GetGeomFieldCount() == 1 )
            poProps->eGeomType = poSrcLayer->GetGeomType();
        else if( poProps->eGeometryStyle == VGS_Direct &&
                 poProps->iGeomField >= 0 )
        {
            poProps->eGeomType = GetSrcLayerDefn()
                                     ->GetGeomFieldDefn(poProps->iGeomField)
                                     ->GetType();
        }
    }

    // Copy spatial reference system from source if not provided.
    const char *pszSRS = CPLGetXMLValue(psNode, "SRS", NULL);
    if( pszSRS == NULL && poProps == apoGeomFieldProps[0] )
        pszSRS = CPLGetXMLValue(psNodeParent, "LayerSRS", NULL);
    if( pszSRS == NULL )
    {
        OGRSpatialReference *poSRS = NULL;
        if( GetSrcLayerDefn()->GetGeomFieldCount() == 1 )
        {
            poSRS = poSrcLayer->GetSpatialRef();
        }
        else if( poProps->eGeometryStyle == VGS_Direct &&
                 poProps->iGeomField >= 0 )
        {
            poSRS = GetSrcLayerDefn()
                        ->GetGeomFieldDefn(poProps->iGeomField)
                        ->GetSpatialRef();
        }
        if( poSRS != NULL )
            poProps->poSRS = poSRS->Clone();
    }
    else if( poProps->poSRS == NULL )
    {
        if( !(EQUAL(pszSRS,"NULL")) )
        {
            OGRSpatialReference oSRS;

            if( oSRS.SetFromUserInput(pszSRS) != OGRERR_NONE )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to import SRS `%s'.", pszSRS);
                return false;
            }
            poProps->poSRS = oSRS.Clone();
        }
    }

    // Do we have a SrcRegion?
    const char *pszSrcRegion = CPLGetXMLValue(psNode, "SrcRegion", NULL);
    if( pszSrcRegion == NULL && poProps == apoGeomFieldProps[0] )
        pszSrcRegion = CPLGetXMLValue(psNodeParent, "SrcRegion", NULL);
    if( pszSrcRegion != NULL )
    {
        OGRGeometryFactory::createFromWkt((char **)&pszSrcRegion, NULL,
                                          &poProps->poSrcRegion);
        if( poProps->poSrcRegion == NULL ||
            wkbFlatten(poProps->poSrcRegion->getGeometryType()) != wkbPolygon )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Ignoring SrcRegion. It must be a valid WKT polygon");
            delete poProps->poSrcRegion;
            poProps->poSrcRegion = NULL;
        }

        poProps->bSrcClip =
            CPLTestBool(CPLGetXMLValue(psNode, "SrcRegion.clip", "FALSE"));
    }

    // Set Extent if provided.
    const char *pszExtentXMin = CPLGetXMLValue(psNode, "ExtentXMin", NULL);
    const char *pszExtentYMin = CPLGetXMLValue(psNode, "ExtentYMin", NULL);
    const char *pszExtentXMax = CPLGetXMLValue(psNode, "ExtentXMax", NULL);
    const char *pszExtentYMax = CPLGetXMLValue(psNode, "ExtentYMax", NULL);
    if( pszExtentXMin != NULL && pszExtentYMin != NULL &&
        pszExtentXMax != NULL && pszExtentYMax != NULL )
    {
        poProps->sStaticEnvelope.MinX = CPLAtof(pszExtentXMin);
        poProps->sStaticEnvelope.MinY = CPLAtof(pszExtentYMin);
        poProps->sStaticEnvelope.MaxX = CPLAtof(pszExtentXMax);
        poProps->sStaticEnvelope.MaxY = CPLAtof(pszExtentYMax);
    }

    poProps->bNullable =
        CPLTestBool(CPLGetXMLValue(psNode, "nullable", "TRUE"));

    return true;
}

/************************************************************************/
/*                         FullInitialize()                             */
/************************************************************************/

// TODO(schwehr): Remove gotos.
bool OGRVRTLayer::FullInitialize()
{
    if( bHasFullInitialized )
        return true;

    const char *pszSharedSetting = NULL;
    const char *pszSQL = NULL;
    const char *pszSrcFIDFieldName = NULL;
    const char *pszStyleFieldName = NULL;
    CPLXMLNode *psChild = NULL;
    bool bFoundGeometryField = false;

    bHasFullInitialized = true;

    poFeatureDefn = new OGRFeatureDefn(osName);
    poFeatureDefn->Reference();

    if( poDS->GetRecursionDetected() )
        return false;

    // Figure out the data source name.  It may be treated relative
    // to vrt filename, but normally it is used directly.
    char *pszSrcDSName =
        const_cast<char *>(CPLGetXMLValue(psLTree, "SrcDataSource", NULL));

    if( pszSrcDSName == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing SrcDataSource for layer %s.", osName.c_str());
        goto error;
    }

    if( CPLTestBool(CPLGetXMLValue(psLTree, "SrcDataSource.relativetoVRT",
                                   "0")) )
    {
        static const char *const apszPrefixes[] = {"CSV:", "GPSBABEL:"};
        bool bDone = false;
        for( size_t i = 0; i < sizeof(apszPrefixes) / sizeof(apszPrefixes[0]);
             i++ )
        {
            const char *pszPrefix = apszPrefixes[i];
            if( EQUALN(pszSrcDSName, pszPrefix, strlen(pszPrefix)) )
            {
                const char *pszLastPart = strrchr(pszSrcDSName, ':') + 1;
                // CSV:z:/foo.xyz
                if( (pszLastPart[0] == '/' || pszLastPart[0] == '\\') &&
                    pszLastPart - pszSrcDSName >= 3 && pszLastPart[-3] == ':' )
                    pszLastPart -= 2;
                CPLString osPrefix(pszSrcDSName);
                osPrefix.resize(pszLastPart - pszSrcDSName);
                pszSrcDSName = CPLStrdup((osPrefix +
                    CPLProjectRelativeFilename(osVRTDirectory, pszLastPart)).c_str());
                bDone = true;
                break;
            }
        }
        if( !bDone )
        {
            pszSrcDSName = CPLStrdup(
                CPLProjectRelativeFilename(osVRTDirectory, pszSrcDSName));
        }
    }
    else
    {
        pszSrcDSName = CPLStrdup(pszSrcDSName);
    }

    // Are we accessing this datasource in shared mode?  We default
    // to shared for SrcSQL requests, but we also allow the XML to
    // control our shared setting with an attribute on the
    // datasource element.
    pszSharedSetting = CPLGetXMLValue(psLTree, "SrcDataSource.shared", NULL);
    if( pszSharedSetting == NULL )
    {
        if( CPLGetXMLValue(psLTree, "SrcSQL", NULL) == NULL )
            pszSharedSetting = "OFF";
        else
            pszSharedSetting = "ON";
    }

    bSrcDSShared = CPLTestBool(pszSharedSetting);

    // Update mode doesn't make sense if we have a SrcSQL element.
    if( CPLGetXMLValue(psLTree, "SrcSQL", NULL) != NULL )
        bUpdate = false;

    // Try to access the datasource.
try_again:
    CPLErrorReset();
    if( EQUAL(pszSrcDSName, "@dummy@") )
    {
        GDALDriver *poMemDriver =
            OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName("Memory");
        if( poMemDriver != NULL )
        {
            poSrcDS =
                poMemDriver->Create("@dummy@", 0, 0, 0, GDT_Unknown, NULL);
            poSrcDS->CreateLayer("@dummy@");
        }
    }
    else if( bSrcDSShared )
    {
        if( poDS->IsInForbiddenNames(pszSrcDSName) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cyclic VRT opening detected!");
            poDS->SetRecursionDetected();
        }
        else
        {
            char **papszOpenOptions =
                GDALDeserializeOpenOptionsFromXML(psLTree);
            int l_nFlags = GDAL_OF_VECTOR | GDAL_OF_SHARED;
            if( bUpdate )
                l_nFlags |= GDAL_OF_UPDATE;
            poSrcDS = (GDALDataset *)GDALOpenEx(
                pszSrcDSName, l_nFlags, NULL,
                (const char *const *)papszOpenOptions, NULL);
            CSLDestroy(papszOpenOptions);
            // Is it a VRT datasource?
            if( poSrcDS != NULL && poSrcDS->GetDriver() == poDS->GetDriver() )
            {
                OGRVRTDataSource *poVRTSrcDS = (OGRVRTDataSource *)poSrcDS;
                poVRTSrcDS->AddForbiddenNames(poDS->GetName());
            }
        }
    }
    else
    {
        if( poDS->GetCallLevel() < 32 )
        {
            char **papszOpenOptions =
                GDALDeserializeOpenOptionsFromXML(psLTree);
            int l_nFlags = GDAL_OF_VECTOR;
            if( bUpdate )
                l_nFlags |= GDAL_OF_UPDATE;
            poSrcDS = (GDALDataset *)GDALOpenEx(
                pszSrcDSName, l_nFlags, NULL,
                (const char *const *)papszOpenOptions, NULL);
            CSLDestroy(papszOpenOptions);
            // Is it a VRT datasource?
            if( poSrcDS != NULL && poSrcDS->GetDriver() == poDS->GetDriver() )
            {
                OGRVRTDataSource *poVRTSrcDS = (OGRVRTDataSource *)poSrcDS;
                poVRTSrcDS->SetCallLevel(poDS->GetCallLevel() + 1);
                poVRTSrcDS->SetParentDS(poDS);
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Trying to open a VRT from a VRT from a VRT from ... "
                     "[32 times] a VRT!");

            poDS->SetRecursionDetected();

            OGRVRTDataSource *poParent = poDS->GetParentDS();
            while(poParent != NULL)
            {
                poParent->SetRecursionDetected();
                poParent = poParent->GetParentDS();
            }
        }
    }

    if( poSrcDS == NULL )
    {
        if( bUpdate )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Cannot open datasource `%s' in update mode. "
                     "Trying again in read-only mode",
                     pszSrcDSName);
            bUpdate = false;
            goto try_again;
        }
        if( strlen(CPLGetLastErrorMsg()) == 0 )
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to open datasource `%s'.", pszSrcDSName);
        goto error;
    }

    // Apply any metadata.
    oMDMD.XMLInit(psLTree, TRUE);

    // Is this layer derived from an SQL query result?
    pszSQL = CPLGetXMLValue(psLTree, "SrcSQL", NULL);

    if( pszSQL != NULL )
    {
        const char *pszDialect =
            CPLGetXMLValue(psLTree, "SrcSQL.dialect", NULL);
        if( pszDialect != NULL && pszDialect[0] == '\0' )
            pszDialect = NULL;
        poSrcLayer = poSrcDS->ExecuteSQL(pszSQL, NULL, pszDialect);
        if( poSrcLayer == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "SQL statement failed, or returned no layer result:\n%s",
                     pszSQL);
            goto error;
        }
        bSrcLayerFromSQL = true;
    }

    // Fetch the layer if it is a regular layer.
    if( poSrcLayer == NULL )
    {
        const char *pszSrcLayerName =
            CPLGetXMLValue(psLTree, "SrcLayer", osName);

        poSrcLayer = poSrcDS->GetLayerByName(pszSrcLayerName);
        if( poSrcLayer == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to find layer '%s' on datasource '%s'.",
                     pszSrcLayerName, pszSrcDSName);
            goto error;
        }
    }

    CPLFree(pszSrcDSName);
    pszSrcDSName = NULL;

    // Search for GeometryField definitions.

    // Create as many OGRVRTGeomFieldProps as there are
    // GeometryField elements.
    for( psChild = psLTree->psChild; psChild != NULL; psChild = psChild->psNext )
    {
        if( psChild->eType == CXT_Element &&
            EQUAL(psChild->pszValue, "GeometryField") )
        {
            if( !bFoundGeometryField )
            {
                bFoundGeometryField = true;

                // Recreate the first one if already taken into account in
                // FastInitialize().
                if( apoGeomFieldProps.size() == 1 )
                {
                    delete apoGeomFieldProps[0];
                    apoGeomFieldProps.resize(0);
                }
            }

            apoGeomFieldProps.push_back(new OGRVRTGeomFieldProps());
            if( !ParseGeometryField(psChild, psLTree,
                                    apoGeomFieldProps.back()) )
            {
                goto error;
            }
        }
    }

    if( !bFoundGeometryField &&
        CPLGetXMLValue(psLTree, "SrcRegion", NULL) != NULL )
    {
        apoGeomFieldProps.push_back(new OGRVRTGeomFieldProps());
    }

    if( !bFoundGeometryField && apoGeomFieldProps.size() == 1 )
    {
        // Otherwise use the top-level elements such as SrcRegion.
        if( !ParseGeometryField(NULL, psLTree, apoGeomFieldProps[0]) )
            goto error;
    }

    if( apoGeomFieldProps.empty() &&
        CPLGetXMLValue(psLTree, "GeometryType", NULL) == NULL )
    {
        // If no GeometryField is found but source geometry fields
        // exist, use them.
        for( int iGeomField = 0;
                iGeomField < GetSrcLayerDefn()->GetGeomFieldCount();
                iGeomField++ )
        {
            OGRVRTGeomFieldProps *poProps = new OGRVRTGeomFieldProps();
            apoGeomFieldProps.push_back(poProps);
            OGRGeomFieldDefn *poFDefn =
                GetSrcLayerDefn()->GetGeomFieldDefn(iGeomField);
            poProps->osName = poFDefn->GetNameRef();
            poProps->eGeomType = poFDefn->GetType();
            if( poFDefn->GetSpatialRef() != NULL )
                poProps->poSRS = poFDefn->GetSpatialRef()->Clone();
            poProps->iGeomField = iGeomField;
            poProps->bNullable = CPL_TO_BOOL(poFDefn->IsNullable());
        }
    }

    // Instantiate real geometry fields from VRT properties.
    poFeatureDefn->SetGeomType(wkbNone);
    for( size_t i = 0; i < apoGeomFieldProps.size(); i++ )
    {
        OGRGeomFieldDefn oFieldDefn(apoGeomFieldProps[i]->osName,
                                    apoGeomFieldProps[i]->eGeomType);
        oFieldDefn.SetSpatialRef(apoGeomFieldProps[i]->poSRS);
        oFieldDefn.SetNullable(apoGeomFieldProps[i]->bNullable);
        poFeatureDefn->AddGeomFieldDefn(&oFieldDefn);
    }

    // Figure out what should be used as an FID.
    bAttrFilterPassThrough = true;
    pszSrcFIDFieldName = CPLGetXMLValue(psLTree, "FID", NULL);

    if( pszSrcFIDFieldName != NULL )
    {
        iFIDField = GetSrcLayerDefn()->GetFieldIndex(pszSrcFIDFieldName);
        if( iFIDField == -1 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to identify FID field '%s'.", pszSrcFIDFieldName);
            goto error;
        }

        // User facing FID column name. If not defined we will report the
        // source FID column name only if it is exposed as a field too (#4637).
        osFIDFieldName = CPLGetXMLValue(psLTree, "FID.name", "");

        if( !EQUAL(pszSrcFIDFieldName, poSrcLayer->GetFIDColumn()) )
        {
            bAttrFilterPassThrough = false;
        }
    }

    // Figure out what should be used as a Style.
    pszStyleFieldName = CPLGetXMLValue(psLTree, "Style", NULL);

    if( pszStyleFieldName != NULL )
    {
        iStyleField = GetSrcLayerDefn()->GetFieldIndex(pszStyleFieldName);
        if( iStyleField == -1 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to identify Style field '%s'.", pszStyleFieldName);
            goto error;
        }

        if( !EQUAL(pszStyleFieldName, "OGR_STYLE") )
        {
            bAttrFilterPassThrough = false;
        }
    }

    // Search for schema definitions in the VRT.
    for( psChild = psLTree->psChild; psChild != NULL; psChild = psChild->psNext )
    {
        if( psChild->eType == CXT_Element && EQUAL(psChild->pszValue, "Field") )
        {
            // Field name.
            const char *pszName = CPLGetXMLValue(psChild, "name", NULL);
            if( pszName == NULL )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Unable to identify Field name.");
                goto error;
            }

            OGRFieldDefn oFieldDefn(pszName, OFTString);

            // Type.
            const char *pszArg = CPLGetXMLValue(psChild, "type", NULL);

            if( pszArg != NULL )
            {
                int iType = 0;  // Used after for.

                for( ; iType <= static_cast<int>(OFTMaxType); iType++ )
                {
                    if( EQUAL(pszArg, OGRFieldDefn::GetFieldTypeName(
                                          static_cast<OGRFieldType>(iType))) )
                    {
                        oFieldDefn.SetType(static_cast<OGRFieldType>(iType));
                        break;
                    }
                }

                if( iType > static_cast<int>(OFTMaxType) )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Unable to identify Field type '%s'.", pszArg);
                    goto error;
                }
            }

            // Subtype.
            pszArg = CPLGetXMLValue(psChild, "subtype", NULL);
            if( pszArg != NULL )
            {
                OGRFieldSubType eSubType = OFSTNone;

                int iType = 0;  // Used after for.
                for( ; iType <= static_cast<int>(OFSTMaxSubType); iType++ )
                {
                    if( EQUAL(pszArg,
                              OGRFieldDefn::GetFieldSubTypeName(
                                  static_cast<OGRFieldSubType>(iType))) )
                    {
                        eSubType = static_cast<OGRFieldSubType>(iType);
                        break;
                    }
                }

                if( iType > static_cast<int>(OFSTMaxSubType) )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Unable to identify Field subtype '%s'.", pszArg);
                    goto error;
                }

                if( !OGR_AreTypeSubTypeCompatible(oFieldDefn.GetType(),
                                                  eSubType) )
                {
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "Invalid subtype '%s' for type '%s'.", pszArg,
                        OGRFieldDefn::GetFieldTypeName(oFieldDefn.GetType()));
                    goto error;
                }

                oFieldDefn.SetSubType(eSubType);
            }

            // Width and precision.
            int nWidth = atoi(CPLGetXMLValue(psChild, "width", "0"));
            if( nWidth < 0 )
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Invalid width for field %s.", pszName);
                goto error;
            }
            oFieldDefn.SetWidth(nWidth);

            int nPrecision = atoi(CPLGetXMLValue(psChild, "precision", "0"));
            if( nPrecision < 0 || nPrecision > 1024 )
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Invalid precision for field %s.", pszName);
                goto error;
            }
            oFieldDefn.SetPrecision(nPrecision);

            // Nullable attribute.
            const bool bNullable =
                CPLTestBool(CPLGetXMLValue(psChild, "nullable", "true"));
            oFieldDefn.SetNullable(bNullable);

            // Default attribute.
            oFieldDefn.SetDefault(CPLGetXMLValue(psChild, "default", NULL));

            // Create the field.
            poFeatureDefn->AddFieldDefn(&oFieldDefn);

            abDirectCopy.push_back(FALSE);

            // Source field.
            int iSrcField = GetSrcLayerDefn()->GetFieldIndex(pszName);

            pszArg = CPLGetXMLValue(psChild, "src", NULL);

            if( pszArg != NULL )
            {
                iSrcField = GetSrcLayerDefn()->GetFieldIndex(pszArg);
                if( iSrcField == -1 )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Unable to find source field '%s'.", pszArg);
                    goto error;
                }
            }

            if( iSrcField < 0 || (pszArg != NULL && strcmp(pszArg, pszName) != 0) )
            {
                bAttrFilterPassThrough = false;
            }
            else
            {
                OGRFieldDefn *poSrcFieldDefn =
                    GetSrcLayerDefn()->GetFieldDefn(iSrcField);
                if( poSrcFieldDefn->GetType() != oFieldDefn.GetType() )
                    bAttrFilterPassThrough = false;
            }

            anSrcField.push_back(iSrcField);
        }
    }

    CPLAssert(poFeatureDefn->GetFieldCount() ==
              static_cast<int>(anSrcField.size()));

    // Create the schema, if it was not explicitly in the VRT.
    if( poFeatureDefn->GetFieldCount() == 0 )
    {
        const int nSrcFieldCount = GetSrcLayerDefn()->GetFieldCount();

        for( int iSrcField = 0; iSrcField < nSrcFieldCount; iSrcField++ )
        {
            bool bSkip = false;
            for( size_t iGF = 0; iGF < apoGeomFieldProps.size(); iGF++ )
            {
                if( !apoGeomFieldProps[iGF]->bReportSrcColumn &&
                    (iSrcField == apoGeomFieldProps[iGF]->iGeomXField ||
                     iSrcField == apoGeomFieldProps[iGF]->iGeomYField ||
                     iSrcField == apoGeomFieldProps[iGF]->iGeomZField ||
                     iSrcField == apoGeomFieldProps[iGF]->iGeomMField ||
                     (apoGeomFieldProps[iGF]->eGeometryStyle != VGS_Direct &&
                      iSrcField == apoGeomFieldProps[iGF]->iGeomField)) )
                {
                    bSkip = true;
                    break;
                }
            }
            if( bSkip )
                continue;

            poFeatureDefn->AddFieldDefn(
                GetSrcLayerDefn()->GetFieldDefn(iSrcField));
            anSrcField.push_back(iSrcField);
            abDirectCopy.push_back(TRUE);
        }

        bAttrFilterPassThrough = true;
    }

    // Is VRT layer definition identical to the source layer defn?
    // If so, use it directly, and save the translation of features.
    if( poSrcFeatureDefn != NULL && iFIDField == -1 && iStyleField == -1 &&
        poSrcFeatureDefn->IsSame(poFeatureDefn) )
    {
        bool bSame = true;
        for( size_t i = 0; i < apoGeomFieldProps.size(); i++ )
        {
            if( apoGeomFieldProps[i]->eGeometryStyle != VGS_Direct ||
                apoGeomFieldProps[i]->iGeomField != static_cast<int>(i) )
            {
                bSame = false;
                break;
            }
        }
        if( bSame )
        {
            CPLDebug(
                "VRT", "Source feature definition is identical to VRT "
                "feature definition. Use optimized path");
            poFeatureDefn->Release();
            poFeatureDefn = poSrcFeatureDefn;
            poFeatureDefn->Reference();
            for( int i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
            {
                if( apoGeomFieldProps[i]->poSRS != NULL )
                    apoGeomFieldProps[i]->poSRS->Release();
                apoGeomFieldProps[i]->poSRS =
                    poFeatureDefn->GetGeomFieldDefn(i)->GetSpatialRef();
                if( apoGeomFieldProps[i]->poSRS != NULL )
                    apoGeomFieldProps[i]->poSRS->Reference();
            }
        }
    }

    CPLAssert(poFeatureDefn->GetGeomFieldCount() ==
              static_cast<int>(apoGeomFieldProps.size()));

    // Allow vrt to override whether attribute filters should be
    // passed through.
    if( CPLGetXMLValue(psLTree, "attrFilterPassThrough", NULL) != NULL )
        bAttrFilterPassThrough = CPLTestBool(
            CPLGetXMLValue(psLTree, "attrFilterPassThrough", "TRUE"));

    SetIgnoredFields(NULL);

    return true;

error:
    bError = true;
    CPLFree(pszSrcDSName);
    poFeatureDefn->Release();
    poFeatureDefn = new OGRFeatureDefn(osName);
    poFeatureDefn->Reference();
    return false;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRVRTLayer::ResetReading() { bNeedReset = true; }

/************************************************************************/
/*                         ResetSourceReading()                         */
/************************************************************************/

bool OGRVRTLayer::ResetSourceReading()

{
    bool bSuccess = true;

    // Do we want to let source layer do spatial restriction?
    char *pszFilter = NULL;
    for( size_t i = 0; i < apoGeomFieldProps.size(); i++ )
    {
        if( (m_poFilterGeom || apoGeomFieldProps[i]->poSrcRegion) &&
            apoGeomFieldProps[i]->bUseSpatialSubquery &&
            apoGeomFieldProps[i]->eGeometryStyle == VGS_PointFromColumns )
        {
            OGRFieldDefn *poXField = poSrcLayer->GetLayerDefn()->GetFieldDefn(
                apoGeomFieldProps[i]->iGeomXField);
            OGRFieldDefn *poYField = poSrcLayer->GetLayerDefn()->GetFieldDefn(
                apoGeomFieldProps[i]->iGeomYField);

            const char *pszXField = poXField->GetNameRef();
            const char *pszYField = poYField->GetNameRef();
            if( apoGeomFieldProps[i]->bUseSpatialSubquery )
            {
                OGRFieldType xType = poXField->GetType();
                OGRFieldType yType = poYField->GetType();
                if( !((xType == OFTReal || xType == OFTInteger ||
                       xType == OFTInteger64) &&
                      (yType == OFTReal || yType == OFTInteger ||
                       yType == OFTInteger64)) )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "The '%s' and/or '%s' fields of the source layer "
                             "are not declared as numeric fields, "
                             "so the spatial filter cannot be turned into an "
                             "attribute filter on them",
                             pszXField, pszYField);
                    apoGeomFieldProps[i]->bUseSpatialSubquery = false;
                }
            }
            if( apoGeomFieldProps[i]->bUseSpatialSubquery )
            {
                OGREnvelope sEnvelope;
                CPLString osFilter;

                if( apoGeomFieldProps[i]->poSrcRegion != NULL )
                {
                    if( m_poFilterGeom == NULL )
                    {
                        apoGeomFieldProps[i]->poSrcRegion->getEnvelope(
                            &sEnvelope);
                    }
                    else
                    {
                        OGRGeometry *poIntersection =
                            apoGeomFieldProps[i]->poSrcRegion->Intersection(
                                m_poFilterGeom);
                        if( poIntersection && !poIntersection->IsEmpty() )
                        {
                            poIntersection->getEnvelope(&sEnvelope);
                        }
                        else
                        {
                            sEnvelope.MinX = 0;
                            sEnvelope.MaxX = 0;
                            sEnvelope.MinY = 0;
                            sEnvelope.MaxY = 0;
                        }
                        delete poIntersection;
                    }
                }
                else
                {
                    m_poFilterGeom->getEnvelope(&sEnvelope);
                }

                if( !CPLIsInf(sEnvelope.MinX) )
                    osFilter +=
                        CPLSPrintf("%s > %.15g", pszXField, sEnvelope.MinX);
                else if( sEnvelope.MinX > 0 )
                    osFilter += "0 = 1";

                if( !CPLIsInf(sEnvelope.MaxX) )
                {
                    if( !osFilter.empty() )
                        osFilter += " AND ";
                    osFilter +=
                        CPLSPrintf("%s < %.15g", pszXField, sEnvelope.MaxX);
                }
                else if( sEnvelope.MaxX < 0 )
                {
                    if( !osFilter.empty() )
                        osFilter += " AND ";
                    osFilter += "0 = 1";
                }

                if( !CPLIsInf(sEnvelope.MinY) )
                {
                    if( !osFilter.empty() )
                        osFilter += " AND ";
                    osFilter +=
                        CPLSPrintf("%s > %.15g", pszYField, sEnvelope.MinY);
                }
                else if( sEnvelope.MinY > 0 )
                {
                    if( !osFilter.empty() )
                        osFilter += " AND ";
                    osFilter += "0 = 1";
                }

                if( !CPLIsInf(sEnvelope.MaxY) )
                {
                    if( !osFilter.empty() )
                        osFilter += " AND ";
                    osFilter +=
                        CPLSPrintf("%s < %.15g", pszYField, sEnvelope.MaxY);
                }
                else if( sEnvelope.MaxY < 0 )
                {
                    if( !osFilter.empty() )
                        osFilter += " AND ";
                    osFilter += "0 = 1";
                }

                if( !osFilter.empty() )
                {
                    pszFilter = CPLStrdup(osFilter);
                }
            }

            // Just do it on one geometry field. To complicated otherwise!
            break;
        }
    }

    // Install spatial + attr filter query on source layer.
    if( pszFilter == NULL && pszAttrFilter == NULL )
    {
        bSuccess = poSrcLayer->SetAttributeFilter(NULL) == OGRERR_NONE;
    }
    else if( pszFilter != NULL && pszAttrFilter == NULL )
    {
        bSuccess = poSrcLayer->SetAttributeFilter(pszFilter) == OGRERR_NONE;
    }
    else if( pszFilter == NULL && pszAttrFilter != NULL )
    {
        bSuccess = poSrcLayer->SetAttributeFilter(pszAttrFilter) == OGRERR_NONE;
    }
    else
    {
        CPLString osMerged = pszFilter;

        osMerged += " AND (";
        osMerged += pszAttrFilter;
        osMerged += ")";

        bSuccess = poSrcLayer->SetAttributeFilter(osMerged) == OGRERR_NONE;
    }

    CPLFree(pszFilter);

    // Clear spatial filter (to be safe) for non direct geometries
    // and reset reading.
    if( m_iGeomFieldFilter < static_cast<int>(apoGeomFieldProps.size()) &&
        apoGeomFieldProps[m_iGeomFieldFilter]->eGeometryStyle == VGS_Direct &&
        apoGeomFieldProps[m_iGeomFieldFilter]->iGeomField >= 0 )
    {
        OGRGeometry *poSpatialGeom = NULL;
        OGRGeometry *poSrcRegion =
            apoGeomFieldProps[m_iGeomFieldFilter]->poSrcRegion;
        bool bToDelete = false;

        if( poSrcRegion == NULL )
        {
            poSpatialGeom = m_poFilterGeom;
        }
        else if( m_poFilterGeom == NULL )
        {
            poSpatialGeom = poSrcRegion;
        }
        else
        {
            if( wkbFlatten(m_poFilterGeom->getGeometryType()) != wkbPolygon )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Spatial filter should be polygon when a SrcRegion is "
                         "defined. Ignoring it");
                poSpatialGeom = poSrcRegion;
            }
            else
            {
                bool bDoIntersection = true;
                if( m_bFilterIsEnvelope )
                {
                    OGREnvelope sEnvelope;
                    m_poFilterGeom->getEnvelope(&sEnvelope);
                    if( CPLIsInf(sEnvelope.MinX) && CPLIsInf(sEnvelope.MinY) &&
                        CPLIsInf(sEnvelope.MaxX) && CPLIsInf(sEnvelope.MaxY) &&
                        sEnvelope.MinX < 0 && sEnvelope.MinY < 0 &&
                        sEnvelope.MaxX > 0 && sEnvelope.MaxY > 0 )
                    {
                        poSpatialGeom = poSrcRegion;
                        bDoIntersection = false;
                    }
                }
                if( bDoIntersection )
                {
                    poSpatialGeom = m_poFilterGeom->Intersection(poSrcRegion);
                    bToDelete = true;
                }
            }
        }
        poSrcLayer->SetSpatialFilter(
            apoGeomFieldProps[m_iGeomFieldFilter]->iGeomField, poSpatialGeom);
        if( bToDelete )
            delete poSpatialGeom;
    }
    else
    {
        poSrcLayer->SetSpatialFilter(NULL);
    }
    poSrcLayer->ResetReading();
    bNeedReset = false;

    return bSuccess;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRVRTLayer::GetNextFeature()

{
    if( !bHasFullInitialized )
        FullInitialize();
    if( !poSrcLayer || poDS->GetRecursionDetected() )
        return NULL;
    if( bError )
        return NULL;

    if( bNeedReset )
    {
        if( !ResetSourceReading() )
            return NULL;
    }

    for( ; true; )
    {
        OGRFeature *poSrcFeature = poSrcLayer->GetNextFeature();
        if( poSrcFeature == NULL )
            return NULL;

        OGRFeature *poFeature = NULL;
        if( poFeatureDefn == poSrcFeatureDefn )
        {
            poFeature = poSrcFeature;
            ClipAndAssignSRS(poFeature);
        }
        else
        {
            poFeature = TranslateFeature(poSrcFeature, TRUE);
            delete poSrcFeature;
        }

        if( poFeature == NULL )
            return NULL;

        if( ((m_iGeomFieldFilter < static_cast<int>(apoGeomFieldProps.size()) &&
              apoGeomFieldProps[m_iGeomFieldFilter]->eGeometryStyle ==
                  VGS_Direct) ||
             m_poFilterGeom == NULL ||
             FilterGeometry(poFeature->GetGeomFieldRef(m_iGeomFieldFilter))) &&
            (m_poAttrQuery == NULL || m_poAttrQuery->Evaluate(poFeature)) )
            return poFeature;

        delete poFeature;
    }
}

/************************************************************************/
/*                          ClipAndAssignSRS()                          */
/************************************************************************/

void OGRVRTLayer::ClipAndAssignSRS(OGRFeature *poFeature)
{
    for(int i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
    {
        // Clip the geometry to the SrcRegion if asked.
        OGRGeometry *poGeom = poFeature->GetGeomFieldRef(i);
        if( apoGeomFieldProps[i]->poSrcRegion != NULL &&
            apoGeomFieldProps[i]->bSrcClip &&
            poGeom != NULL )
        {
            poGeom = poGeom->Intersection(apoGeomFieldProps[i]->poSrcRegion);
            if( poGeom != NULL && apoGeomFieldProps[i]->poSRS != NULL )
                poGeom->assignSpatialReference(apoGeomFieldProps[i]->poSRS);

            poFeature->SetGeomFieldDirectly(i, poGeom);
        }
        else if( poGeom != NULL && apoGeomFieldProps[i]->poSRS != NULL )
            poGeom->assignSpatialReference(apoGeomFieldProps[i]->poSRS);
    }
}

/************************************************************************/
/*                          TranslateFeature()                          */
/*                                                                      */
/*      Translate a source feature into a feature for this layer.       */
/************************************************************************/

OGRFeature *OGRVRTLayer::TranslateFeature( OGRFeature *&poSrcFeat,
                                           int bUseSrcRegion )

{
retry:
    OGRFeature *poDstFeat = new OGRFeature(poFeatureDefn);

    m_nFeaturesRead++;

    // Handle FID.
    if( iFIDField == -1 )
        poDstFeat->SetFID(poSrcFeat->GetFID());
    else
        poDstFeat->SetFID(poSrcFeat->GetFieldAsInteger64(iFIDField));

    // Handle style string.
    if( iStyleField != -1 )
    {
        if( poSrcFeat->IsFieldSetAndNotNull(iStyleField) )
            poDstFeat->SetStyleString(poSrcFeat->GetFieldAsString(iStyleField));
    }
    else
    {
        if( poSrcFeat->GetStyleString() != NULL )
            poDstFeat->SetStyleString(poSrcFeat->GetStyleString());
    }

    for(int i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
    {
        OGRVRTGeometryStyle eGeometryStyle =
            apoGeomFieldProps[i]->eGeometryStyle;
        int iGeomField = apoGeomFieldProps[i]->iGeomField;

        // Handle the geometry.  Eventually there will be several more
        // supported options.
        if( eGeometryStyle == VGS_None ||
            GetLayerDefn()->GetGeomFieldDefn(i)->IsIgnored() )
        {
            // Do nothing.
        }
        else if( eGeometryStyle == VGS_WKT && iGeomField != -1 )
        {
            char *pszWKT =
                const_cast<char *>(poSrcFeat->GetFieldAsString(iGeomField));

            if( pszWKT != NULL )
            {
                OGRGeometry *poGeom = NULL;

                OGRGeometryFactory::createFromWkt(&pszWKT, NULL, &poGeom);
                if( poGeom == NULL )
                    CPLDebug("OGR_VRT", "Did not get geometry from %s", pszWKT);

                poDstFeat->SetGeomFieldDirectly(i, poGeom);
            }
        }
        else if( eGeometryStyle == VGS_WKB && iGeomField != -1 )
        {
            int nBytes = 0;
            GByte *pabyWKB = NULL;
            bool bNeedFree = false;

            if( poSrcFeat->GetFieldDefnRef(iGeomField)->GetType() == OFTBinary )
            {
                pabyWKB = poSrcFeat->GetFieldAsBinary(iGeomField, &nBytes);
            }
            else
            {
                const char *pszWKT = poSrcFeat->GetFieldAsString(iGeomField);

                pabyWKB = CPLHexToBinary(pszWKT, &nBytes);
                bNeedFree = true;
            }

            if( pabyWKB != NULL )
            {
                OGRGeometry *poGeom = NULL;

                if( OGRGeometryFactory::createFromWkb(pabyWKB, NULL, &poGeom,
                                                      nBytes) == OGRERR_NONE )
                    poDstFeat->SetGeomFieldDirectly(i, poGeom);
            }

            if( bNeedFree )
                CPLFree(pabyWKB);
        }
        else if( eGeometryStyle == VGS_Shape && iGeomField != -1 )
        {
            int nBytes = 0;
            GByte *pabyWKB = NULL;
            bool bNeedFree = false;

            if( poSrcFeat->GetFieldDefnRef(iGeomField)->GetType() == OFTBinary )
            {
                pabyWKB = poSrcFeat->GetFieldAsBinary(iGeomField, &nBytes);
            }
            else
            {
                const char *pszWKT = poSrcFeat->GetFieldAsString(iGeomField);

                pabyWKB = CPLHexToBinary(pszWKT, &nBytes);
                bNeedFree = true;
            }

            if( pabyWKB != NULL )
            {
                OGRGeometry *poGeom = NULL;

                if( OGRCreateFromShapeBin(pabyWKB, &poGeom, nBytes) ==
                    OGRERR_NONE )
                    poDstFeat->SetGeomFieldDirectly(i, poGeom);
            }

            if( bNeedFree )
                CPLFree(pabyWKB);
        }
        else if( eGeometryStyle == VGS_Direct && iGeomField != -1 )
        {
            poDstFeat->SetGeomField(i, poSrcFeat->GetGeomFieldRef(iGeomField));
        }
        else if( eGeometryStyle == VGS_PointFromColumns )
        {
            OGRPoint *poPoint = NULL;
            if( apoGeomFieldProps[i]->iGeomZField != -1 )
            {
                poPoint = new OGRPoint(poSrcFeat->GetFieldAsDouble(
                                           apoGeomFieldProps[i]->iGeomXField),
                                       poSrcFeat->GetFieldAsDouble(
                                           apoGeomFieldProps[i]->iGeomYField),
                                       poSrcFeat->GetFieldAsDouble(
                                           apoGeomFieldProps[i]->iGeomZField));
            }
            else
            {
                poPoint = new OGRPoint(poSrcFeat->GetFieldAsDouble(
                                           apoGeomFieldProps[i]->iGeomXField),
                                       poSrcFeat->GetFieldAsDouble(
                                           apoGeomFieldProps[i]->iGeomYField));
            }
            if( apoGeomFieldProps[i]->iGeomMField >= 0 )
            {
                poPoint->setM(poSrcFeat->GetFieldAsDouble(
                    apoGeomFieldProps[i]->iGeomMField));
            }
            poDstFeat->SetGeomFieldDirectly(i, poPoint);
        }
        else
        {
            // Add other options here.
        }

        // In the non-direct case, we need to check that the geometry
        // intersects the source region before an optional clipping.
        if( bUseSrcRegion &&
            apoGeomFieldProps[i]->eGeometryStyle != VGS_Direct &&
            apoGeomFieldProps[i]->poSrcRegion != NULL )
        {
            OGRGeometry *poGeom = poDstFeat->GetGeomFieldRef(i);
            if( poGeom != NULL &&
                !poGeom->Intersects(apoGeomFieldProps[i]->poSrcRegion) )
            {
                delete poSrcFeat;
                delete poDstFeat;

                // Fetch next source feature and retry translating it.
                poSrcFeat = poSrcLayer->GetNextFeature();
                if( poSrcFeat == NULL )
                    return NULL;

                goto retry;
            }
        }
    }

    ClipAndAssignSRS(poDstFeat);

    // Copy fields.
    for( int iVRTField = 0; iVRTField < poFeatureDefn->GetFieldCount();
         iVRTField++ )
    {
        if( anSrcField[iVRTField] == -1 )
            continue;

        OGRFieldDefn *poDstDefn = poFeatureDefn->GetFieldDefn(iVRTField);
        OGRFieldDefn *poSrcDefn =
            poSrcLayer->GetLayerDefn()->GetFieldDefn(anSrcField[iVRTField]);

        if( !poSrcFeat->IsFieldSetAndNotNull(anSrcField[iVRTField]) ||
            poDstDefn->IsIgnored() )
            continue;

        if( abDirectCopy[iVRTField] &&
            poDstDefn->GetType() == poSrcDefn->GetType() )
        {
            poDstFeat->SetField(
                iVRTField, poSrcFeat->GetRawFieldRef(anSrcField[iVRTField]));
        }
        else
        {
            // Eventually we need to offer some more sophisticated translation
            // options here for more esoteric types.
            if( poDstDefn->GetType() == OFTReal )
                poDstFeat->SetField(iVRTField, poSrcFeat->GetFieldAsDouble(
                                                   anSrcField[iVRTField]));
            else
                poDstFeat->SetField(iVRTField, poSrcFeat->GetFieldAsString(
                                                   anSrcField[iVRTField]));
        }
    }

    return poDstFeat;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRVRTLayer::GetFeature( GIntBig nFeatureId )

{
    if( !bHasFullInitialized )
        FullInitialize();
    if( !poSrcLayer || poDS->GetRecursionDetected() )
        return NULL;

    bNeedReset = true;

    // If the FID is directly mapped, we can do a simple
    // GetFeature() to get our target feature.  Otherwise we need
    // to setup an appropriate query to get it.
    OGRFeature *poSrcFeature = NULL;
    OGRFeature *poFeature = NULL;

    if( iFIDField == -1 )
    {
        poSrcFeature = poSrcLayer->GetFeature(nFeatureId);
    }
    else
    {
        const char *pszFID =
            poSrcLayer->GetLayerDefn()->GetFieldDefn(iFIDField)->GetNameRef();
        char *pszFIDQuery = static_cast<char *>(CPLMalloc(strlen(pszFID) + 64));

        poSrcLayer->ResetReading();
        snprintf(pszFIDQuery, strlen(pszFID) + 64, "%s = " CPL_FRMT_GIB, pszFID,
                 nFeatureId);
        poSrcLayer->SetSpatialFilter(NULL);
        poSrcLayer->SetAttributeFilter(pszFIDQuery);
        CPLFree(pszFIDQuery);

        poSrcFeature = poSrcLayer->GetNextFeature();
    }

    if( poSrcFeature == NULL )
        return NULL;

    // Translate feature and return it.
    if( poFeatureDefn == poSrcFeatureDefn )
    {
        poFeature = poSrcFeature;
        ClipAndAssignSRS(poFeature);
    }
    else
    {
        poFeature = TranslateFeature(poSrcFeature, FALSE);
        delete poSrcFeature;
    }

    return poFeature;
}

/************************************************************************/
/*                          SetNextByIndex()                            */
/************************************************************************/

OGRErr OGRVRTLayer::SetNextByIndex( GIntBig nIndex )
{
    if( !bHasFullInitialized )
        FullInitialize();
    if( !poSrcLayer || poDS->GetRecursionDetected() )
        return OGRERR_FAILURE;

    if( bNeedReset )
    {
        if( !ResetSourceReading() )
            return OGRERR_FAILURE;
    }

    if( TestCapability(OLCFastSetNextByIndex) )
        return poSrcLayer->SetNextByIndex(nIndex);

    return OGRLayer::SetNextByIndex(nIndex);
}

/************************************************************************/
/*               TranslateVRTFeatureToSrcFeature()                      */
/*                                                                      */
/*      Translate a VRT feature into a feature for the source layer     */
/************************************************************************/

OGRFeature *
OGRVRTLayer::TranslateVRTFeatureToSrcFeature(OGRFeature *poVRTFeature)
{
    OGRFeature *poSrcFeat = new OGRFeature(poSrcLayer->GetLayerDefn());

    poSrcFeat->SetFID(poVRTFeature->GetFID());

    // Handle style string.
    if( iStyleField != -1 )
    {
        if( poVRTFeature->GetStyleString() != NULL )
            poSrcFeat->SetField(iStyleField, poVRTFeature->GetStyleString());
    }
    else
    {
        if( poVRTFeature->GetStyleString() != NULL )
            poSrcFeat->SetStyleString(poVRTFeature->GetStyleString());
    }

    // Handle the geometry.  Eventually there will be several more
    // supported options.
    for(int i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
    {
        OGRVRTGeometryStyle eGeometryStyle =
            apoGeomFieldProps[i]->eGeometryStyle;
        int iGeomField = apoGeomFieldProps[i]->iGeomField;

        if( eGeometryStyle == VGS_None )
        {
            // Do nothing.
        }
        else if( eGeometryStyle == VGS_WKT && iGeomField >= 0 )
        {
            OGRGeometry *poGeom = poVRTFeature->GetGeomFieldRef(i);
            if( poGeom != NULL )
            {
                char *pszWKT = NULL;
                if( poGeom->exportToWkt(&pszWKT) == OGRERR_NONE )
                {
                    poSrcFeat->SetField(iGeomField, pszWKT);
                }
                CPLFree(pszWKT);
            }
        }
        else if( eGeometryStyle == VGS_WKB && iGeomField >= 0)
        {
            OGRGeometry *poGeom = poVRTFeature->GetGeomFieldRef(i);
            if( poGeom != NULL )
            {
                const int nSize = poGeom->WkbSize();
                GByte *pabyData = static_cast<GByte *>(CPLMalloc(nSize));
                if( poGeom->exportToWkb(wkbNDR, pabyData) == OGRERR_NONE )
                {
                    if( poSrcFeat->GetFieldDefnRef(iGeomField)->GetType() ==
                        OFTBinary )
                    {
                        poSrcFeat->SetField(iGeomField, nSize, pabyData);
                    }
                    else
                    {
                        char *pszHexWKB = CPLBinaryToHex(nSize, pabyData);
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
        else if( eGeometryStyle == VGS_Direct && iGeomField >= 0 )
        {
            poSrcFeat->SetGeomField(iGeomField,
                                    poVRTFeature->GetGeomFieldRef(i));
        }
        else if( eGeometryStyle == VGS_PointFromColumns )
        {
            OGRGeometry *poGeom = poVRTFeature->GetGeomFieldRef(i);
            if( poGeom != NULL )
            {
                if( wkbFlatten(poGeom->getGeometryType()) != wkbPoint )
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                             "Cannot set a non ponctual geometry for "
                             "PointFromColumns geometry");
                }
                else
                {
                    poSrcFeat->SetField(apoGeomFieldProps[i]->iGeomXField,
                                        ((OGRPoint *)poGeom)->getX());
                    poSrcFeat->SetField(apoGeomFieldProps[i]->iGeomYField,
                                        ((OGRPoint *)poGeom)->getY());
                    if( apoGeomFieldProps[i]->iGeomZField != -1 )
                    {
                        poSrcFeat->SetField(apoGeomFieldProps[i]->iGeomZField,
                                            ((OGRPoint *)poGeom)->getZ());
                    }
                    if( apoGeomFieldProps[i]->iGeomMField != -1 )
                    {
                        poSrcFeat->SetField(apoGeomFieldProps[i]->iGeomMField,
                                            ((OGRPoint *)poGeom)->getM());
                    }
                }
            }
        }
        else
        {
            // Add other options here.
        }

        OGRGeometry *poGeom = poSrcFeat->GetGeomFieldRef(i);
        if( poGeom != NULL && apoGeomFieldProps[i]->poSRS != NULL )
            poGeom->assignSpatialReference(apoGeomFieldProps[i]->poSRS);
    }

    // Copy fields.
    for( int iVRTField = 0; iVRTField < poFeatureDefn->GetFieldCount();
         iVRTField++ )
    {
        bool bSkip = false;
        for( int i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
        {
            // Do not set source geometry columns. Have been set just above.
            if( (apoGeomFieldProps[i]->eGeometryStyle != VGS_Direct &&
                 anSrcField[iVRTField] == apoGeomFieldProps[i]->iGeomField) ||
                anSrcField[iVRTField] == apoGeomFieldProps[i]->iGeomXField ||
                anSrcField[iVRTField] == apoGeomFieldProps[i]->iGeomYField ||
                anSrcField[iVRTField] == apoGeomFieldProps[i]->iGeomZField ||
                anSrcField[iVRTField] == apoGeomFieldProps[i]->iGeomMField )
            {
                bSkip = true;
                break;
            }
        }
        if( bSkip )
            continue;

        OGRFieldDefn *poVRTDefn = poFeatureDefn->GetFieldDefn(iVRTField);
        OGRFieldDefn *poSrcDefn =
            poSrcLayer->GetLayerDefn()->GetFieldDefn(anSrcField[iVRTField]);

        if( abDirectCopy[iVRTField] &&
            poVRTDefn->GetType() == poSrcDefn->GetType() )
        {
            poSrcFeat->SetField(anSrcField[iVRTField],
                                poVRTFeature->GetRawFieldRef(iVRTField));
        }
        else
        {
            // Eventually we need to offer some more sophisticated translation
            // options here for more esoteric types.
            poSrcFeat->SetField(anSrcField[iVRTField],
                                poVRTFeature->GetFieldAsString(iVRTField));
        }
    }

    return poSrcFeat;
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRVRTLayer::ICreateFeature(OGRFeature *poVRTFeature)
{
    if( !bHasFullInitialized )
        FullInitialize();
    if( !poSrcLayer || poDS->GetRecursionDetected() )
        return OGRERR_FAILURE;

    if( !bUpdate )
    {
        CPLError(CE_Failure, CPLE_NotSupported, UNSUPPORTED_OP_READ_ONLY,
                 "CreateFeature");
        return OGRERR_FAILURE;
    }

    if( iFIDField != -1 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The CreateFeature() operation is not supported "
                 "if the FID option is specified.");
        return OGRERR_FAILURE;
    }

    if( poSrcFeatureDefn == poFeatureDefn )
        return poSrcLayer->CreateFeature(poVRTFeature);

    OGRFeature *poSrcFeature = TranslateVRTFeatureToSrcFeature(poVRTFeature);
    poSrcFeature->SetFID(OGRNullFID);
    OGRErr eErr = poSrcLayer->CreateFeature(poSrcFeature);
    if( eErr == OGRERR_NONE )
    {
        poVRTFeature->SetFID(poSrcFeature->GetFID());
    }
    delete poSrcFeature;
    return eErr;
}

/************************************************************************/
/*                             ISetFeature()                             */
/************************************************************************/

OGRErr OGRVRTLayer::ISetFeature(OGRFeature *poVRTFeature)
{
    if( !bHasFullInitialized )
        FullInitialize();
    if( !poSrcLayer || poDS->GetRecursionDetected() )
        return OGRERR_FAILURE;

    if( !bUpdate )
    {
        CPLError(CE_Failure, CPLE_NotSupported, UNSUPPORTED_OP_READ_ONLY,
                 "SetFeature");
        return OGRERR_FAILURE;
    }

    if( iFIDField != -1 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The SetFeature() operation is not supported "
                 "if the FID option is specified.");
        return OGRERR_FAILURE;
    }

    if( poSrcFeatureDefn == poFeatureDefn )
        return poSrcLayer->SetFeature(poVRTFeature);

    OGRFeature *poSrcFeature = TranslateVRTFeatureToSrcFeature(poVRTFeature);
    OGRErr eErr = poSrcLayer->SetFeature(poSrcFeature);
    delete poSrcFeature;
    return eErr;
}

/************************************************************************/
/*                           DeleteFeature()                            */
/************************************************************************/

OGRErr OGRVRTLayer::DeleteFeature( GIntBig nFID )

{
    if( !bHasFullInitialized )
        FullInitialize();
    if( !poSrcLayer || poDS->GetRecursionDetected() )
        return OGRERR_FAILURE;

    if( !bUpdate )
    {
        CPLError(CE_Failure, CPLE_NotSupported, UNSUPPORTED_OP_READ_ONLY,
                 "DeleteFeature");
        return OGRERR_FAILURE;
    }

    if( iFIDField != -1 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The DeleteFeature() operation is not supported "
                 "if the FID option is specified.");
        return OGRERR_FAILURE;
    }

    return poSrcLayer->DeleteFeature(nFID);
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRVRTLayer::SetAttributeFilter( const char *pszNewQuery )

{
    if( !bHasFullInitialized )
        FullInitialize();
    if( !poSrcLayer || poDS->GetRecursionDetected() )
        return OGRERR_FAILURE;

    if( bAttrFilterPassThrough )
    {
        CPLFree(pszAttrFilter);
        if( pszNewQuery == NULL || strlen(pszNewQuery) == 0 )
            pszAttrFilter = NULL;
        else
            pszAttrFilter = CPLStrdup(pszNewQuery);

        ResetReading();
        return OGRERR_NONE;
    }
    else
    {
        // Setup m_poAttrQuery.
        return OGRLayer::SetAttributeFilter(pszNewQuery);
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRVRTLayer::TestCapability( const char *pszCap )

{
    if( EQUAL(pszCap, OLCFastFeatureCount) && nFeatureCount >= 0 &&
        m_poFilterGeom == NULL && m_poAttrQuery == NULL )
        return TRUE;

    if( EQUAL(pszCap, OLCFastGetExtent) && apoGeomFieldProps.size() == 1 &&
        apoGeomFieldProps[0]->sStaticEnvelope.IsInit() )
        return TRUE;

    if( !bHasFullInitialized )
        FullInitialize();
    if( !poSrcLayer || poDS->GetRecursionDetected() )
        return FALSE;

    if( EQUAL(pszCap, OLCFastFeatureCount) ||
        EQUAL(pszCap, OLCFastSetNextByIndex) )
    {
        if( m_poAttrQuery == NULL )
        {
            bool bForward = true;
            for( size_t i = 0; i < apoGeomFieldProps.size(); i++ )
            {
                if( !(apoGeomFieldProps[i]->eGeometryStyle == VGS_Direct ||
                      (apoGeomFieldProps[i]->poSrcRegion == NULL &&
                       m_poFilterGeom == NULL)) )
                {
                    bForward = false;
                    break;
                }
            }
            if( bForward )
            {
                return poSrcLayer->TestCapability(pszCap);
            }
        }
        return FALSE;
    }

    else if( EQUAL(pszCap, OLCFastSpatialFilter) )
        return apoGeomFieldProps.size() == 1 &&
               apoGeomFieldProps[0]->eGeometryStyle == VGS_Direct &&
               m_poAttrQuery == NULL && poSrcLayer->TestCapability(pszCap);

    else if( EQUAL(pszCap, OLCFastGetExtent) )
        return apoGeomFieldProps.size() == 1 &&
               apoGeomFieldProps[0]->eGeometryStyle == VGS_Direct &&
               m_poAttrQuery == NULL &&
               (apoGeomFieldProps[0]->poSrcRegion == NULL ||
                apoGeomFieldProps[0]->bSrcClip) &&
               poSrcLayer->TestCapability(pszCap);

    else if( EQUAL(pszCap, OLCRandomRead) )
        return iFIDField == -1 && poSrcLayer->TestCapability(pszCap);

    else if( EQUAL(pszCap, OLCSequentialWrite)
             || EQUAL(pszCap, OLCRandomWrite)
             || EQUAL(pszCap, OLCDeleteFeature) )
        return bUpdate && iFIDField == -1 && poSrcLayer->TestCapability(pszCap);

    else if( EQUAL(pszCap, OLCStringsAsUTF8) )
        return poSrcLayer->TestCapability(pszCap);

    else if( EQUAL(pszCap, OLCTransactions) )
        return bUpdate && poSrcLayer->TestCapability(pszCap);

    else if( EQUAL(pszCap, OLCIgnoreFields) ||
             EQUAL(pszCap, OLCCurveGeometries) ||
             EQUAL(pszCap, OLCMeasuredGeometries) )
        return poSrcLayer->TestCapability(pszCap);

    return FALSE;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRVRTLayer::GetSpatialRef()

{
    if( (CPLGetXMLValue(psLTree, "LayerSRS", NULL) != NULL ||
         CPLGetXMLValue(psLTree, "GeometryField.SRS", NULL) != NULL) &&
        !apoGeomFieldProps.empty() )
        return apoGeomFieldProps[0]->poSRS;

    if( !bHasFullInitialized )
        FullInitialize();
    if( !poSrcLayer || poDS->GetRecursionDetected() )
        return NULL;

    if( apoGeomFieldProps.size() >= 1 )
        return apoGeomFieldProps[0]->poSRS;
    else
        return NULL;
}

/************************************************************************/
/*                              GetExtent()                             */
/************************************************************************/

OGRErr OGRVRTLayer::GetExtent( OGREnvelope *psExtent, int bForce )
{
    return GetExtent(0, psExtent, bForce);
}

OGRErr OGRVRTLayer::GetExtent( int iGeomField, OGREnvelope *psExtent, int bForce )
{
    if( iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount() )
        return OGRERR_FAILURE;

    if( apoGeomFieldProps[iGeomField]->sStaticEnvelope.IsInit() )
    {
        memcpy(psExtent, &apoGeomFieldProps[iGeomField]->sStaticEnvelope,
               sizeof(OGREnvelope));
        return OGRERR_NONE;
    }

    if( !bHasFullInitialized )
        FullInitialize();
    if( !poSrcLayer || poDS->GetRecursionDetected() )
        return OGRERR_FAILURE;

    if( apoGeomFieldProps[iGeomField]->eGeometryStyle == VGS_Direct &&
        m_poAttrQuery == NULL &&
        (apoGeomFieldProps[iGeomField]->poSrcRegion == NULL ||
         apoGeomFieldProps[iGeomField]->bSrcClip) )
    {
        if( bNeedReset )
            ResetSourceReading();

        OGRErr eErr = poSrcLayer->GetExtent(
            apoGeomFieldProps[iGeomField]->iGeomField, psExtent, bForce);
        if( eErr != OGRERR_NONE ||
            apoGeomFieldProps[iGeomField]->poSrcRegion == NULL )
            return eErr;

        OGREnvelope sSrcRegionEnvelope;
        apoGeomFieldProps[iGeomField]->poSrcRegion->getEnvelope(
            &sSrcRegionEnvelope);

        psExtent->Intersect(sSrcRegionEnvelope);
        return eErr;
    }

    return OGRLayer::GetExtentInternal(iGeomField, psExtent, bForce);
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRVRTLayer::GetFeatureCount( int bForce )

{
    if( nFeatureCount >= 0 && m_poFilterGeom == NULL && m_poAttrQuery == NULL )
    {
        return nFeatureCount;
    }

    if( !bHasFullInitialized )
        FullInitialize();
    if( !poSrcLayer || poDS->GetRecursionDetected() )
        return 0;

    if( TestCapability(OLCFastFeatureCount) )
    {
        if( bNeedReset )
            ResetSourceReading();

        return poSrcLayer->GetFeatureCount(bForce);
    }

    return OGRLayer::GetFeatureCount(bForce);
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRVRTLayer::SetSpatialFilter( OGRGeometry * poGeomIn )
{
    SetSpatialFilter(0, poGeomIn);
}

void OGRVRTLayer::SetSpatialFilter( int iGeomField, OGRGeometry *poGeomIn )
{
    if( iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount() )
    {
        if( poGeomIn != NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid geometry field index : %d", iGeomField);
        }
        return;
    }

    if( !bHasFullInitialized )
        FullInitialize();
    if( !poSrcLayer || poDS->GetRecursionDetected() )
        return;

    if( apoGeomFieldProps[iGeomField]->eGeometryStyle == VGS_Direct)
        bNeedReset = true;

    m_iGeomFieldFilter = iGeomField;
    if( InstallFilter(poGeomIn) )
        ResetReading();
}

/************************************************************************/
/*                             SyncToDisk()                             */
/************************************************************************/

OGRErr OGRVRTLayer::SyncToDisk()
{
    if( !bHasFullInitialized )
        FullInitialize();
    if( !poSrcLayer || poDS->GetRecursionDetected() )
        return OGRERR_FAILURE;

    return poSrcLayer->SyncToDisk();
}

/************************************************************************/
/*                            GetLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn *OGRVRTLayer::GetLayerDefn()
{
    if( !bHasFullInitialized ) FullInitialize();

    return poFeatureDefn;
}

/************************************************************************/
/*                             GetGeomType()                            */
/************************************************************************/

OGRwkbGeometryType OGRVRTLayer::GetGeomType()
{
    if( CPLGetXMLValue(psLTree, "GeometryType", NULL) != NULL ||
        CPLGetXMLValue(psLTree, "GeometryField.GeometryType", NULL) != NULL )
    {
        if( apoGeomFieldProps.size() >= 1)
            return apoGeomFieldProps[0]->eGeomType;
        return wkbNone;
    }

    return GetLayerDefn()->GetGeomType();
}

/************************************************************************/
/*                             GetFIDColumn()                           */
/************************************************************************/

const char *OGRVRTLayer::GetFIDColumn()
{
    if( !bHasFullInitialized )
        FullInitialize();
    if( !poSrcLayer || poDS->GetRecursionDetected() )
        return "";

    if( !osFIDFieldName.empty() )
        return osFIDFieldName;

    const char *pszFIDColumn = NULL;
    if( iFIDField == -1 )
    {
        // If pass-through, then query the source layer FID column.
        pszFIDColumn = poSrcLayer->GetFIDColumn();
        if( pszFIDColumn == NULL || EQUAL(pszFIDColumn, "") )
            return "";
    }
    else
    {
        // Otherwise get the name from the index in the source layer definition.
        OGRFieldDefn *poFDefn = GetSrcLayerDefn()->GetFieldDefn(iFIDField);
        pszFIDColumn = poFDefn->GetNameRef();
    }

    // Check that the FIDColumn is actually reported in the VRT layer
    // definition.
    if( GetLayerDefn()->GetFieldIndex(pszFIDColumn) != -1 )
        return pszFIDColumn;
    else
        return "";
}

/************************************************************************/
/*                           StartTransaction()                         */
/************************************************************************/

OGRErr OGRVRTLayer::StartTransaction()
{
    if( !bHasFullInitialized )
        FullInitialize();
    if( !poSrcLayer || !bUpdate || poDS->GetRecursionDetected() )
        return OGRERR_FAILURE;

    return poSrcLayer->StartTransaction();
}

/************************************************************************/
/*                           CommitTransaction()                        */
/************************************************************************/

OGRErr OGRVRTLayer::CommitTransaction()
{
    if( !bHasFullInitialized )
        FullInitialize();
    if( !poSrcLayer || !bUpdate || poDS->GetRecursionDetected() )
        return OGRERR_FAILURE;

    return poSrcLayer->CommitTransaction();
}

/************************************************************************/
/*                          RollbackTransaction()                       */
/************************************************************************/

OGRErr OGRVRTLayer::RollbackTransaction()
{
    if( !bHasFullInitialized )
        FullInitialize();
    if( !poSrcLayer || !bUpdate || poDS->GetRecursionDetected() )
        return OGRERR_FAILURE;

    return poSrcLayer->RollbackTransaction();
}

/************************************************************************/
/*                          SetIgnoredFields()                          */
/************************************************************************/

OGRErr OGRVRTLayer::SetIgnoredFields( const char **papszFields )
{
    if( !bHasFullInitialized )
        FullInitialize();
    if( !poSrcLayer || poDS->GetRecursionDetected() )
        return OGRERR_FAILURE;

    if( !poSrcLayer->TestCapability(OLCIgnoreFields) )
        return OGRERR_FAILURE;

    OGRErr eErr = OGRLayer::SetIgnoredFields(papszFields);
    if( eErr != OGRERR_NONE )
        return eErr;

    const char **papszIter = papszFields;
    char **papszFieldsSrc = NULL;
    poSrcLayer->GetLayerDefn();

    // Translate explicitly ignored fields of VRT layers to their equivalent
    // source fields.
    while( papszIter != NULL && *papszIter != NULL )
    {
        const char *pszFieldName = *papszIter;
        if( EQUAL(pszFieldName, "OGR_GEOMETRY") ||
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
                if( iSrcField >= 0 )
                {
                    // If we are asked to ignore x or y for a
                    // VGS_PointFromColumns geometry field, we must NOT pass
                    // that order to the underlying layer.
                    bool bOKToIgnore = true;
                    for( int iGeomVRTField = 0;
                         iGeomVRTField < GetLayerDefn()->GetGeomFieldCount();
                         iGeomVRTField++ )
                    {
                        if( iSrcField ==
                                apoGeomFieldProps[iGeomVRTField]->iGeomXField ||
                            iSrcField ==
                                apoGeomFieldProps[iGeomVRTField]->iGeomYField ||
                            iSrcField ==
                                apoGeomFieldProps[iGeomVRTField]->iGeomZField ||
                            iSrcField ==
                                apoGeomFieldProps[iGeomVRTField]->iGeomMField )
                        {
                            bOKToIgnore = false;
                            break;
                        }
                    }
                    if( bOKToIgnore )
                    {
                        OGRFieldDefn *poSrcDefn =
                            poSrcFeatureDefn->GetFieldDefn(iSrcField);
                        papszFieldsSrc = CSLAddString(papszFieldsSrc,
                                                      poSrcDefn->GetNameRef());
                    }
                }
            }
            else
            {
                iVRTField = GetLayerDefn()->GetGeomFieldIndex(pszFieldName);
                if( iVRTField >= 0 &&
                    apoGeomFieldProps[iVRTField]->eGeometryStyle == VGS_Direct )
                {
                    int iSrcField = apoGeomFieldProps[iVRTField]->iGeomField;
                    if( iSrcField >= 0 )
                    {
                        OGRGeomFieldDefn *poSrcDefn =
                            poSrcFeatureDefn->GetGeomFieldDefn(iSrcField);
                        papszFieldsSrc = CSLAddString(papszFieldsSrc,
                                                      poSrcDefn->GetNameRef());
                    }
                }
            }
        }
        papszIter++;
    }

    // Add source fields that are not referenced by VRT layer.
    int *panSrcFieldsUsed = static_cast<int *>(
        CPLCalloc(sizeof(int), poSrcFeatureDefn->GetFieldCount()));
    for( int iVRTField = 0; iVRTField < GetLayerDefn()->GetFieldCount();
         iVRTField++ )
    {
        const int iSrcField = anSrcField[iVRTField];
        if( iSrcField >= 0 )
            panSrcFieldsUsed[iSrcField] = TRUE;
    }
    for(int iVRTField = 0;
            iVRTField < GetLayerDefn()->GetGeomFieldCount(); iVRTField++)
    {
        OGRVRTGeometryStyle eGeometryStyle =
            apoGeomFieldProps[iVRTField]->eGeometryStyle;
        // For a VGS_PointFromColumns geometry field, we must not ignore
        // the fields that help building it.
        if( eGeometryStyle == VGS_PointFromColumns )
        {
            int iSrcField = apoGeomFieldProps[iVRTField]->iGeomXField;
            if( iSrcField >= 0 )
                panSrcFieldsUsed[iSrcField] = TRUE;
            iSrcField = apoGeomFieldProps[iVRTField]->iGeomYField;
            if( iSrcField >= 0 )
                panSrcFieldsUsed[iSrcField] = TRUE;
            iSrcField = apoGeomFieldProps[iVRTField]->iGeomZField;
            if( iSrcField >= 0 )
                panSrcFieldsUsed[iSrcField] = TRUE;
            iSrcField = apoGeomFieldProps[iVRTField]->iGeomMField;
            if( iSrcField >= 0 )
                panSrcFieldsUsed[iSrcField] = TRUE;
        }
        // Similarly for other kinds of geometry fields.
        else if( eGeometryStyle == VGS_WKT || eGeometryStyle == VGS_WKB ||
                 eGeometryStyle == VGS_Shape )
        {
            int iSrcField = apoGeomFieldProps[iVRTField]->iGeomField;
            if( iSrcField >= 0 )
                panSrcFieldsUsed[iSrcField] = TRUE;
        }
    }
    if( iStyleField >= 0 )
        panSrcFieldsUsed[iStyleField] = TRUE;
    if( iFIDField >= 0 )
        panSrcFieldsUsed[iFIDField] = TRUE;
    for( int iSrcField = 0; iSrcField < poSrcFeatureDefn->GetFieldCount();
         iSrcField++ )
    {
        if( !panSrcFieldsUsed[iSrcField] )
        {
            OGRFieldDefn *poSrcDefn = poSrcFeatureDefn->GetFieldDefn(iSrcField);
            papszFieldsSrc =
                CSLAddString(papszFieldsSrc, poSrcDefn->GetNameRef());
        }
    }
    CPLFree(panSrcFieldsUsed);

    // Add source geometry fields that are not referenced by VRT layer.
    panSrcFieldsUsed = static_cast<int *>(
        CPLCalloc(sizeof(int), poSrcFeatureDefn->GetGeomFieldCount()));
    for( int iVRTField = 0; iVRTField < GetLayerDefn()->GetGeomFieldCount();
         iVRTField++ )
    {
        if( apoGeomFieldProps[iVRTField]->eGeometryStyle == VGS_Direct )
        {
            const int iSrcField = apoGeomFieldProps[iVRTField]->iGeomField;
            if( iSrcField >= 0 )
                panSrcFieldsUsed[iSrcField] = TRUE;
        }
    }
    for( int iSrcField = 0; iSrcField < poSrcFeatureDefn->GetGeomFieldCount();
         iSrcField++ )
    {
        if( !panSrcFieldsUsed[iSrcField] )
        {
            OGRGeomFieldDefn *poSrcDefn =
                poSrcFeatureDefn->GetGeomFieldDefn(iSrcField);
            papszFieldsSrc =
                CSLAddString(papszFieldsSrc, poSrcDefn->GetNameRef());
        }
    }
    CPLFree(panSrcFieldsUsed);

    eErr = poSrcLayer->SetIgnoredFields((const char **)papszFieldsSrc);

    CSLDestroy(papszFieldsSrc);

    return eErr;
}

/************************************************************************/
/*                          GetSrcDataset()                             */
/************************************************************************/

GDALDataset *OGRVRTLayer::GetSrcDataset()
{
    if( !bHasFullInitialized )
        FullInitialize();
    if( !poSrcLayer || poDS->GetRecursionDetected() )
        return NULL;
    return poSrcDS;
}
