/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Simple client for translating between formats.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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

#include "ogrsf_frmts.h"
#include "ogr_p.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_api.h"
#include "gdal.h"
#include "gdal_alg.h"
#include "commonutils.h"
#include <map>

CPL_CVSID("$Id$");

static int bSkipFailures = FALSE;
static int nGroupTransactions = 200;
static int bPreserveFID = FALSE;
static int nFIDToFetch = OGRNullFID;

static void Usage(int bShort = TRUE);
static void Usage(const char* pszAdditionalMsg, int bShort = TRUE);

typedef enum
{
    NONE,
    SEGMENTIZE,
    SIMPLIFY_PRESERVE_TOPOLOGY,
} GeomOperation;

typedef struct
{
    GIntBig      nFeaturesRead;
    int          bPerFeatureCT;
    OGRLayer    *poDstLayer;
    OGRCoordinateTransformation *poCT;
    char       **papszTransformOptions;
    int         *panMap;
    int          iSrcZField;
} TargetLayerInfo;

typedef struct
{
    OGRLayer         *poSrcLayer;
    TargetLayerInfo  *psInfo;
} AssociatedLayers;

static TargetLayerInfo* SetupTargetLayer( OGRDataSource *poSrcDS,
                                                OGRLayer * poSrcLayer,
                                                OGRDataSource *poDstDS,
                                                char **papszLCO,
                                                const char *pszNewLayerName,
                                                OGRSpatialReference *poOutputSRS,
                                                int bNullifyOutputSRS,
                                                char **papszSelFields,
                                                int bAppend, int eGType,
                                                int bPromoteToMulti,
                                                int nCoordDim, int bOverwrite,
                                                char** papszFieldTypesToString,
                                                int bExplodeCollections,
                                                const char* pszZField,
                                                char **papszFieldMap,
                                                const char* pszWHERE );

static void FreeTargetLayerInfo(TargetLayerInfo* psInfo);

static int TranslateLayer( TargetLayerInfo* psInfo,
                           OGRDataSource *poSrcDS,
                           OGRLayer * poSrcLayer,
                           OGRDataSource *poDstDS,
                           int bTransform,
                           int bWrapDateline,
                           const char* pszDateLineOffset,
                           OGRSpatialReference *poOutputSRS,
                           int bNullifyOutputSRS,
                           OGRSpatialReference *poUserSourceSRS,
                           OGRCoordinateTransformation *poGCPCoordTrans,
                           int eGType,
                           int bPromoteToMulti,
                           int nCoordDim,
                           GeomOperation eGeomOp,
                           double dfGeomOpParam,
                           long nCountLayerFeatures,
                           OGRGeometry* poClipSrc,
                           OGRGeometry *poClipDst,
                           int bExplodeCollections,
                           vsi_l_offset nSrcFileSize,
                           GIntBig* pnReadFeatureCount,
                           GDALProgressFunc pfnProgress,
                           void *pProgressArg);

/* -------------------------------------------------------------------- */
/*                  CheckDestDataSourceNameConsistency()                */
/* -------------------------------------------------------------------- */

static
void CheckDestDataSourceNameConsistency(const char* pszDestFilename,
                                        const char* pszDriverName)
{
    int i;
    char* pszDestExtension = CPLStrdup(CPLGetExtension(pszDestFilename));

    /* TODO: Would be good to have driver metadata like for GDAL drivers ! */
    static const char* apszExtensions[][2] = { { "shp"    , "ESRI Shapefile" },
                                               { "dbf"    , "ESRI Shapefile" },
                                               { "sqlite" , "SQLite" },
                                               { "db"     , "SQLite" },
                                               { "mif"    , "MapInfo File" },
                                               { "tab"    , "MapInfo File" },
                                               { "s57"    , "S57" },
                                               { "bna"    , "BNA" },
                                               { "csv"    , "CSV" },
                                               { "gml"    , "GML" },
                                               { "kml"    , "KML/LIBKML" },
                                               { "kmz"    , "LIBKML" },
                                               { "json"   , "GeoJSON" },
                                               { "geojson", "GeoJSON" },
                                               { "dxf"    , "DXF" },
                                               { "gdb"    , "FileGDB" },
                                               { "pix"    , "PCIDSK" },
                                               { "sql"    , "PGDump" },
                                               { "gtm"    , "GPSTrackMaker" },
                                               { "gmt"    , "GMT" },
                                               { "pdf"    , "PDF" },
                                               { NULL, NULL }
                                              };
    static const char* apszBeginName[][2] =  { { "PG:"      , "PG" },
                                               { "MySQL:"   , "MySQL" },
                                               { "CouchDB:" , "CouchDB" },
                                               { "GFT:"     , "GFT" },
                                               { "MSSQL:"   , "MSSQLSpatial" },
                                               { "ODBC:"    , "ODBC" },
                                               { "OCI:"     , "OCI" },
                                               { "SDE:"     , "SDE" },
                                               { "WFS:"     , "WFS" },
                                               { NULL, NULL }
                                             };

    for(i=0; apszExtensions[i][0] != NULL; i++)
    {
        if (EQUAL(pszDestExtension, apszExtensions[i][0]) && !EQUAL(pszDriverName, apszExtensions[i][1]))
        {
            fprintf(stderr,
                    "Warning: The target file has a '%s' extension, which is normally used by the %s driver,\n"
                    "but the requested output driver is %s. Is it really what you want ?\n",
                    pszDestExtension,
                    apszExtensions[i][1],
                    pszDriverName);
            break;
        }
    }

    for(i=0; apszBeginName[i][0] != NULL; i++)
    {
        if (EQUALN(pszDestFilename, apszBeginName[i][0], strlen(apszBeginName[i][0])) &&
            !EQUAL(pszDriverName, apszBeginName[i][1]))
        {
            fprintf(stderr,
                    "Warning: The target file has a name which is normally recognized by the %s driver,\n"
                    "but the requested output driver is %s. Is it really what you want ?\n",
                    apszBeginName[i][1],
                    pszDriverName);
            break;
        }
    }

    CPLFree(pszDestExtension);
}

/************************************************************************/
/*                            IsNumber()                               */
/************************************************************************/

static int IsNumber(const char* pszStr)
{
    if (*pszStr == '-' || *pszStr == '+')
        pszStr ++;
    if (*pszStr == '.')
        pszStr ++;
    return (*pszStr >= '0' && *pszStr <= '9');
}

/************************************************************************/
/*                           LoadGeometry()                             */
/************************************************************************/

static OGRGeometry* LoadGeometry( const char* pszDS,
                                  const char* pszSQL,
                                  const char* pszLyr,
                                  const char* pszWhere)
{
    OGRDataSource       *poDS;
    OGRLayer            *poLyr;
    OGRFeature          *poFeat;
    OGRGeometry         *poGeom = NULL;
        
    poDS = OGRSFDriverRegistrar::Open( pszDS, FALSE );
    if (poDS == NULL)
        return NULL;

    if (pszSQL != NULL)
        poLyr = poDS->ExecuteSQL( pszSQL, NULL, NULL ); 
    else if (pszLyr != NULL)
        poLyr = poDS->GetLayerByName(pszLyr);
    else
        poLyr = poDS->GetLayer(0);
        
    if (poLyr == NULL)
    {
        fprintf( stderr, "Failed to identify source layer from datasource.\n" );
        OGRDataSource::DestroyDataSource(poDS);
        return NULL;
    }
    
    if (pszWhere)
        poLyr->SetAttributeFilter(pszWhere);
        
    while ((poFeat = poLyr->GetNextFeature()) != NULL)
    {
        OGRGeometry* poSrcGeom = poFeat->GetGeometryRef();
        if (poSrcGeom)
        {
            OGRwkbGeometryType eType = wkbFlatten( poSrcGeom->getGeometryType() );
            
            if (poGeom == NULL)
                poGeom = OGRGeometryFactory::createGeometry( wkbMultiPolygon );

            if( eType == wkbPolygon )
                ((OGRGeometryCollection*)poGeom)->addGeometry( poSrcGeom );
            else if( eType == wkbMultiPolygon )
            {
                int iGeom;
                int nGeomCount = OGR_G_GetGeometryCount( (OGRGeometryH)poSrcGeom );

                for( iGeom = 0; iGeom < nGeomCount; iGeom++ )
                {
                    ((OGRGeometryCollection*)poGeom)->addGeometry(
                                ((OGRGeometryCollection*)poSrcGeom)->getGeometryRef(iGeom) );
                }
            }
            else
            {
                fprintf( stderr, "ERROR: Geometry not of polygon type.\n" );
                OGRGeometryFactory::destroyGeometry(poGeom);
                OGRFeature::DestroyFeature(poFeat);
                if( pszSQL != NULL )
                    poDS->ReleaseResultSet( poLyr );
                OGRDataSource::DestroyDataSource(poDS);
                return NULL;
            }
        }
    
        OGRFeature::DestroyFeature(poFeat);
    }
    
    if( pszSQL != NULL )
        poDS->ReleaseResultSet( poLyr );
    OGRDataSource::DestroyDataSource(poDS);
    
    return poGeom;
}


/************************************************************************/
/*                     OGRSplitListFieldLayer                           */
/************************************************************************/

typedef struct
{
    int          iSrcIndex;
    OGRFieldType eType;
    int          nMaxOccurences;
    int          nWidth;
} ListFieldDesc;

class OGRSplitListFieldLayer : public OGRLayer
{
    OGRLayer                    *poSrcLayer;
    OGRFeatureDefn              *poFeatureDefn;
    ListFieldDesc               *pasListFields;
    int                          nListFieldCount;
    int                          nMaxSplitListSubFields;

    OGRFeature                  *TranslateFeature(OGRFeature* poSrcFeature);

  public:
                                 OGRSplitListFieldLayer(OGRLayer* poSrcLayer,
                                                        int nMaxSplitListSubFields);
                                ~OGRSplitListFieldLayer();

    int                          BuildLayerDefn(GDALProgressFunc pfnProgress,
                                                void *pProgressArg);

    virtual OGRFeature          *GetNextFeature();
    virtual OGRFeature          *GetFeature(long nFID);
    virtual OGRFeatureDefn      *GetLayerDefn();

    virtual void                 ResetReading() { poSrcLayer->ResetReading(); }
    virtual int                  TestCapability(const char*) { return FALSE; }

    virtual int                  GetFeatureCount( int bForce = TRUE )
    {
        return poSrcLayer->GetFeatureCount(bForce);
    }

    virtual OGRSpatialReference *GetSpatialRef()
    {
        return poSrcLayer->GetSpatialRef();
    }

    virtual OGRGeometry         *GetSpatialFilter()
    {
        return poSrcLayer->GetSpatialFilter();
    }

    virtual OGRStyleTable       *GetStyleTable()
    {
        return poSrcLayer->GetStyleTable();
    }

    virtual void                 SetSpatialFilter( OGRGeometry *poGeom )
    {
        poSrcLayer->SetSpatialFilter(poGeom);
    }

    virtual void                 SetSpatialFilterRect( double dfMinX, double dfMinY,
                                                       double dfMaxX, double dfMaxY )
    {
        poSrcLayer->SetSpatialFilterRect(dfMinX, dfMinY, dfMaxX, dfMaxY);
    }

    virtual OGRErr               SetAttributeFilter( const char *pszFilter )
    {
        return poSrcLayer->SetAttributeFilter(pszFilter);
    }
};

/************************************************************************/
/*                    OGRSplitListFieldLayer()                          */
/************************************************************************/

OGRSplitListFieldLayer::OGRSplitListFieldLayer(OGRLayer* poSrcLayer,
                                               int nMaxSplitListSubFields)
{
    this->poSrcLayer = poSrcLayer;
    if (nMaxSplitListSubFields < 0)
        nMaxSplitListSubFields = INT_MAX;
    this->nMaxSplitListSubFields = nMaxSplitListSubFields;
    poFeatureDefn = NULL;
    pasListFields = NULL;
    nListFieldCount = 0;
}

/************************************************************************/
/*                   ~OGRSplitListFieldLayer()                          */
/************************************************************************/

OGRSplitListFieldLayer::~OGRSplitListFieldLayer()
{
    if( poFeatureDefn )
        poFeatureDefn->Release();

    CPLFree(pasListFields);
}

/************************************************************************/
/*                       BuildLayerDefn()                               */
/************************************************************************/

int  OGRSplitListFieldLayer::BuildLayerDefn(GDALProgressFunc pfnProgress,
                                            void *pProgressArg)
{
    CPLAssert(poFeatureDefn == NULL);
    
    OGRFeatureDefn* poSrcFieldDefn = poSrcLayer->GetLayerDefn();
    
    int nSrcFields = poSrcFieldDefn->GetFieldCount();
    pasListFields =
            (ListFieldDesc*)CPLCalloc(sizeof(ListFieldDesc), nSrcFields);
    nListFieldCount = 0;
    int i;
    
    /* Establish the list of fields of list type */
    for(i=0;i<nSrcFields;i++)
    {
        OGRFieldType eType = poSrcFieldDefn->GetFieldDefn(i)->GetType();
        if (eType == OFTIntegerList ||
            eType == OFTRealList ||
            eType == OFTStringList)
        {
            pasListFields[nListFieldCount].iSrcIndex = i;
            pasListFields[nListFieldCount].eType = eType;
            if (nMaxSplitListSubFields == 1)
                pasListFields[nListFieldCount].nMaxOccurences = 1;
            nListFieldCount++;
        }
    }

    if (nListFieldCount == 0)
        return FALSE;

    /* No need for full scan if the limit is 1. We just to have to create */
    /* one and a single one field */
    if (nMaxSplitListSubFields != 1)
    {
        poSrcLayer->ResetReading();
        OGRFeature* poSrcFeature;

        int nFeatureCount = 0;
        if (poSrcLayer->TestCapability(OLCFastFeatureCount))
            nFeatureCount = poSrcLayer->GetFeatureCount();
        int nFeatureIndex = 0;

        /* Scan the whole layer to compute the maximum number of */
        /* items for each field of list type */
        while( (poSrcFeature = poSrcLayer->GetNextFeature()) != NULL )
        {
            for(i=0;i<nListFieldCount;i++)
            {
                int nCount = 0;
                OGRField* psField =
                        poSrcFeature->GetRawFieldRef(pasListFields[i].iSrcIndex);
                switch(pasListFields[i].eType)
                {
                    case OFTIntegerList:
                        nCount = psField->IntegerList.nCount;
                        break;
                    case OFTRealList:
                        nCount = psField->RealList.nCount;
                        break;
                    case OFTStringList:
                    {
                        nCount = psField->StringList.nCount;
                        char** paList = psField->StringList.paList;
                        int j;
                        for(j=0;j<nCount;j++)
                        {
                            int nWidth = strlen(paList[j]);
                            if (nWidth > pasListFields[i].nWidth)
                                pasListFields[i].nWidth = nWidth;
                        }
                        break;
                    }
                    default:
                        CPLAssert(0);
                        break;
                }
                if (nCount > pasListFields[i].nMaxOccurences)
                {
                    if (nCount > nMaxSplitListSubFields)
                        nCount = nMaxSplitListSubFields;
                    pasListFields[i].nMaxOccurences = nCount;
                }
            }
            OGRFeature::DestroyFeature(poSrcFeature);

            nFeatureIndex ++;
            if (pfnProgress != NULL && nFeatureCount != 0)
                pfnProgress(nFeatureIndex * 1.0 / nFeatureCount, "", pProgressArg);
        }
    }

    /* Now let's build the target feature definition */

    poFeatureDefn =
            OGRFeatureDefn::CreateFeatureDefn( poSrcFieldDefn->GetName() );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( poSrcFieldDefn->GetGeomType() );

    int iListField = 0;
    for(i=0;i<nSrcFields;i++)
    {
        OGRFieldType eType = poSrcFieldDefn->GetFieldDefn(i)->GetType();
        if (eType == OFTIntegerList ||
            eType == OFTRealList ||
            eType == OFTStringList)
        {
            int nMaxOccurences = pasListFields[iListField].nMaxOccurences;
            int nWidth = pasListFields[iListField].nWidth;
            iListField ++;
            int j;
            if (nMaxOccurences == 1)
            {
                OGRFieldDefn oFieldDefn(poSrcFieldDefn->GetFieldDefn(i)->GetNameRef(),
                                            (eType == OFTIntegerList) ? OFTInteger :
                                            (eType == OFTRealList) ?    OFTReal :
                                                                        OFTString);
                poFeatureDefn->AddFieldDefn(&oFieldDefn);
            }
            else
            {
                for(j=0;j<nMaxOccurences;j++)
                {
                    CPLString osFieldName;
                    osFieldName.Printf("%s%d",
                        poSrcFieldDefn->GetFieldDefn(i)->GetNameRef(), j+1);
                    OGRFieldDefn oFieldDefn(osFieldName.c_str(),
                                            (eType == OFTIntegerList) ? OFTInteger :
                                            (eType == OFTRealList) ?    OFTReal :
                                                                        OFTString);
                    oFieldDefn.SetWidth(nWidth);
                    poFeatureDefn->AddFieldDefn(&oFieldDefn);
                }
            }
        }
        else
        {
            poFeatureDefn->AddFieldDefn(poSrcFieldDefn->GetFieldDefn(i));
        }
    }

    return TRUE;
}


/************************************************************************/
/*                       TranslateFeature()                             */
/************************************************************************/

OGRFeature *OGRSplitListFieldLayer::TranslateFeature(OGRFeature* poSrcFeature)
{
    if (poSrcFeature == NULL)
        return NULL;
    if (poFeatureDefn == NULL)
        return poSrcFeature;

    OGRFeature* poFeature = OGRFeature::CreateFeature(poFeatureDefn);
    poFeature->SetFID(poSrcFeature->GetFID());
    poFeature->SetGeometryDirectly(poSrcFeature->StealGeometry());
    poFeature->SetStyleString(poFeature->GetStyleString());

    OGRFeatureDefn* poSrcFieldDefn = poSrcLayer->GetLayerDefn();
    int nSrcFields = poSrcFeature->GetFieldCount();
    int iSrcField;
    int iDstField = 0;
    int iListField = 0;
    int j;
    for(iSrcField=0;iSrcField<nSrcFields;iSrcField++)
    {
        OGRFieldType eType = poSrcFieldDefn->GetFieldDefn(iSrcField)->GetType();
        OGRField* psField = poSrcFeature->GetRawFieldRef(iSrcField);
        switch(eType)
        {
            case OFTIntegerList:
            {
                int nCount = psField->IntegerList.nCount;
                if (nCount > nMaxSplitListSubFields)
                    nCount = nMaxSplitListSubFields;
                int* paList = psField->IntegerList.paList;
                for(j=0;j<nCount;j++)
                    poFeature->SetField(iDstField + j, paList[j]);
                iDstField += pasListFields[iListField].nMaxOccurences;
                iListField++;
                break;
            }
            case OFTRealList:
            {
                int nCount = psField->RealList.nCount;
                if (nCount > nMaxSplitListSubFields)
                    nCount = nMaxSplitListSubFields;
                double* paList = psField->RealList.paList;
                for(j=0;j<nCount;j++)
                    poFeature->SetField(iDstField + j, paList[j]);
                iDstField += pasListFields[iListField].nMaxOccurences;
                iListField++;
                break;
            }
            case OFTStringList:
            {
                int nCount = psField->StringList.nCount;
                if (nCount > nMaxSplitListSubFields)
                    nCount = nMaxSplitListSubFields;
                char** paList = psField->StringList.paList;
                for(j=0;j<nCount;j++)
                    poFeature->SetField(iDstField + j, paList[j]);
                iDstField += pasListFields[iListField].nMaxOccurences;
                iListField++;
                break;
            }
            default:
                poFeature->SetField(iDstField, psField);
                iDstField ++;
                break;
        }
    }

    OGRFeature::DestroyFeature(poSrcFeature);

    return poFeature;
}

/************************************************************************/
/*                       GetNextFeature()                               */
/************************************************************************/

OGRFeature *OGRSplitListFieldLayer::GetNextFeature()
{
    return TranslateFeature(poSrcLayer->GetNextFeature());
}

/************************************************************************/
/*                           GetFeature()                               */
/************************************************************************/

OGRFeature *OGRSplitListFieldLayer::GetFeature(long nFID)
{
    return TranslateFeature(poSrcLayer->GetFeature(nFID));
}

/************************************************************************/
/*                        GetLayerDefn()                                */
/************************************************************************/

OGRFeatureDefn* OGRSplitListFieldLayer::GetLayerDefn()
{
    if (poFeatureDefn == NULL)
        return poSrcLayer->GetLayerDefn();
    return poFeatureDefn;
}

/************************************************************************/
/*                            GCPCoordTransformation()                  */
/*                                                                      */
/*      Apply GCP Transform to points                                   */
/************************************************************************/

class GCPCoordTransformation : public OGRCoordinateTransformation
{
public:

    void               *hTransformArg;
    int                 bUseTPS;
    OGRSpatialReference* poSRS;

    GCPCoordTransformation( int nGCPCount,
                            const GDAL_GCP *pasGCPList,
                            int  nReqOrder,
                            OGRSpatialReference* poSRS)
    {
        if( nReqOrder < 0 )
        {
            bUseTPS = TRUE;
            hTransformArg = 
                GDALCreateTPSTransformer( nGCPCount, pasGCPList, FALSE );
        }
        else
        {
            bUseTPS = FALSE;
            hTransformArg = 
                GDALCreateGCPTransformer( nGCPCount, pasGCPList, nReqOrder, FALSE );
        }
        this->poSRS = poSRS;
        if( poSRS) 
            poSRS->Reference();
    }

    int IsValid() const { return hTransformArg != NULL; }

    virtual ~GCPCoordTransformation()
    {
        if( hTransformArg != NULL )
        {
            if( bUseTPS )
                GDALDestroyTPSTransformer(hTransformArg);
            else
                GDALDestroyGCPTransformer(hTransformArg);
        }
        if( poSRS) 
            poSRS->Dereference();
    }

    virtual OGRSpatialReference *GetSourceCS() { return poSRS; }
    virtual OGRSpatialReference *GetTargetCS() { return poSRS; }

    virtual int Transform( int nCount, 
                           double *x, double *y, double *z = NULL )
    {
        int *pabSuccess = (int *) CPLMalloc(sizeof(int) * nCount );
        int bOverallSuccess, i;

        bOverallSuccess = TransformEx( nCount, x, y, z, pabSuccess );

        for( i = 0; i < nCount; i++ )
        {
            if( !pabSuccess[i] )
            {
                bOverallSuccess = FALSE;
                break;
            }
        }

        CPLFree( pabSuccess );

        return bOverallSuccess;
    }

    virtual int TransformEx( int nCount, 
                             double *x, double *y, double *z = NULL,
                             int *pabSuccess = NULL )
    {
        if( bUseTPS )
            return GDALTPSTransform( hTransformArg, FALSE, 
                                 nCount, x, y, z, pabSuccess );
        else
            return GDALGCPTransform( hTransformArg, FALSE, 
                                 nCount, x, y, z, pabSuccess );
    }
};

/************************************************************************/
/*                            CompositeCT                               */
/************************************************************************/

class CompositeCT : public OGRCoordinateTransformation
{
public:

    OGRCoordinateTransformation* poCT1;
    OGRCoordinateTransformation* poCT2;

    CompositeCT( OGRCoordinateTransformation* poCT1, /* will not be deleted */
                 OGRCoordinateTransformation* poCT2  /* deleted with OGRCoordinateTransformation::DestroyCT() */ )
    {
        this->poCT1 = poCT1;
        this->poCT2 = poCT2;
    }

    virtual ~CompositeCT()
    {
        OGRCoordinateTransformation::DestroyCT(poCT2);
    }

    virtual OGRSpatialReference *GetSourceCS()
    {
        return poCT1 ? poCT1->GetSourceCS() :
               poCT2 ? poCT2->GetSourceCS() : NULL;
    }

    virtual OGRSpatialReference *GetTargetCS()
    {
        return poCT2 ? poCT2->GetTargetCS() :
               poCT1 ? poCT1->GetTargetCS() : NULL;
    }

    virtual int Transform( int nCount, 
                           double *x, double *y, double *z = NULL )
    {
        int nResult = TRUE;
        if( poCT1 )
            nResult = poCT1->Transform(nCount, x, y, z);
        if( nResult && poCT2 )
            nResult = poCT2->Transform(nCount, x, y, z);
        return nResult;
    }

    virtual int TransformEx( int nCount, 
                             double *x, double *y, double *z = NULL,
                             int *pabSuccess = NULL )
    {
        int nResult = TRUE;
        if( poCT1 )
            nResult = poCT1->TransformEx(nCount, x, y, z, pabSuccess);
        if( nResult && poCT2 )
            nResult = poCT2->TransformEx(nCount, x, y, z, pabSuccess);
        return nResult;
    }
};

/************************************************************************/
/*                                main()                                */
/************************************************************************/

#define CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(nExtraArg) \
    do { if (iArg + nExtraArg >= nArgc) \
        Usage(CPLSPrintf("%s option requires %d argument(s)", papszArgv[iArg], nExtraArg)); } while(0)

int main( int nArgc, char ** papszArgv )

{
    int          nRetCode = 0;
    int          bQuiet = FALSE;
    int          bFormatExplicitelySet = FALSE;
    const char  *pszFormat = "ESRI Shapefile";
    const char  *pszDataSource = NULL;
    const char  *pszDestDataSource = NULL;
    char        **papszLayers = NULL;
    char        **papszDSCO = NULL, **papszLCO = NULL;
    int         bTransform = FALSE;
    int         bAppend = FALSE, bUpdate = FALSE, bOverwrite = FALSE;
    const char  *pszOutputSRSDef = NULL;
    const char  *pszSourceSRSDef = NULL;
    OGRSpatialReference *poOutputSRS = NULL;
    int         bNullifyOutputSRS = FALSE;
    OGRSpatialReference *poSourceSRS = NULL;
    char        *pszNewLayerName = NULL;
    const char  *pszWHERE = NULL;
    OGRGeometry *poSpatialFilter = NULL;
    const char  *pszSelect;
    char        **papszSelFields = NULL;
    const char  *pszSQLStatement = NULL;
    const char  *pszDialect = NULL;
    int         eGType = -2;
    int          bPromoteToMulti = FALSE;
    GeomOperation eGeomOp = NONE;
    double       dfGeomOpParam = 0;
    char        **papszFieldTypesToString = NULL;
    int          bDisplayProgress = FALSE;
    GDALProgressFunc pfnProgress = NULL;
    void        *pProgressArg = NULL;
    int          bWrapDateline = FALSE;
    const char  *pszDateLineOffset = "10";
    int          bClipSrc = FALSE;
    OGRGeometry* poClipSrc = NULL;
    const char  *pszClipSrcDS = NULL;
    const char  *pszClipSrcSQL = NULL;
    const char  *pszClipSrcLayer = NULL;
    const char  *pszClipSrcWhere = NULL;
    OGRGeometry *poClipDst = NULL;
    const char  *pszClipDstDS = NULL;
    const char  *pszClipDstSQL = NULL;
    const char  *pszClipDstLayer = NULL;
    const char  *pszClipDstWhere = NULL;
    int          bSplitListFields = FALSE;
    int          nMaxSplitListSubFields = -1;
    int          bExplodeCollections = FALSE;
    const char  *pszZField = NULL;
    const char  *pszFieldMap = NULL;
    char        **papszFieldMap = NULL;
    int          nCoordDim = -1;
 
    int          nGCPCount = 0;
    GDAL_GCP    *pasGCPs = NULL;
    int          nTransformOrder = 0;  /* Default to 0 for now... let the lib decide */

    /* Check strict compilation and runtime library version as we use C++ API */
    if (! GDAL_CHECK_VERSION(papszArgv[0]))
        exit(1);

    EarlySetConfigOptions(nArgc, papszArgv);

/* -------------------------------------------------------------------- */
/*      Register format(s).                                             */
/* -------------------------------------------------------------------- */
    OGRRegisterAll();

/* -------------------------------------------------------------------- */
/*      Processing command line arguments.                              */
/* -------------------------------------------------------------------- */
    nArgc = OGRGeneralCmdLineProcessor( nArgc, &papszArgv, 0 );
    
    if( nArgc < 1 )
        exit( -nArgc );

    for( int iArg = 1; iArg < nArgc; iArg++ )
    {
        if( EQUAL(papszArgv[iArg], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                   papszArgv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            return 0;
        }
        else if( EQUAL(papszArgv[iArg],"--help") )
            Usage();
        else if ( EQUAL(papszArgv[iArg], "--long-usage") )
        {
            Usage(FALSE);
        }

        else if( EQUAL(papszArgv[iArg],"-q") || EQUAL(papszArgv[iArg],"-quiet") )
        {
            bQuiet = TRUE;
        }
        else if( EQUAL(papszArgv[iArg],"-f") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            bFormatExplicitelySet = TRUE;
            pszFormat = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg],"-dsco") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            papszDSCO = CSLAddString(papszDSCO, papszArgv[++iArg] );
        }
        else if( EQUAL(papszArgv[iArg],"-lco") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            papszLCO = CSLAddString(papszLCO, papszArgv[++iArg] );
        }
        else if( EQUAL(papszArgv[iArg],"-preserve_fid") )
        {
            bPreserveFID = TRUE;
        }
        else if( EQUALN(papszArgv[iArg],"-skip",5) )
        {
            bSkipFailures = TRUE;
            nGroupTransactions = 1; /* #2409 */
        }
        else if( EQUAL(papszArgv[iArg],"-append") )
        {
            bAppend = TRUE;
            bUpdate = TRUE;
        }
        else if( EQUAL(papszArgv[iArg],"-overwrite") )
        {
            bOverwrite = TRUE;
            bUpdate = TRUE;
        }
        else if( EQUAL(papszArgv[iArg],"-update") )
        {
            bUpdate = TRUE;
        }
        else if( EQUAL(papszArgv[iArg],"-fid") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            nFIDToFetch = atoi(papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg],"-sql") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszSQLStatement = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg],"-dialect") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszDialect = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg],"-nln") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszNewLayerName = CPLStrdup(papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg],"-nlt") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            int bIs3D = FALSE;
            CPLString osGeomName = papszArgv[iArg+1];
            if (strlen(papszArgv[iArg+1]) > 3 &&
                EQUALN(papszArgv[iArg+1] + strlen(papszArgv[iArg+1]) - 3, "25D", 3))
            {
                bIs3D = TRUE;
                osGeomName.resize(osGeomName.size() - 3);
            }
            if( EQUAL(osGeomName,"NONE") )
                eGType = wkbNone;
            else if( EQUAL(osGeomName,"GEOMETRY") )
                eGType = wkbUnknown;
            else if( EQUAL(osGeomName,"PROMOTE_TO_MULTI") )
                bPromoteToMulti = TRUE;
            else
            {
                eGType = OGRFromOGCGeomType(osGeomName);
                if (eGType == wkbUnknown)
                {
                    fprintf( stderr, "-nlt %s: type not recognised.\n",
                            papszArgv[iArg+1] );
                    exit( 1 );
                }
            }
            if (eGType != -2 && eGType != wkbNone && bIs3D)
                eGType |= wkb25DBit;

            iArg++;
        }
        else if( EQUAL(papszArgv[iArg],"-dim")  )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            nCoordDim = atoi(papszArgv[iArg+1]);
            if( nCoordDim != 2 && nCoordDim != 3 )
            {
                fprintf( stderr, "-dim %s: value not handled.\n",
                        papszArgv[iArg+1] );
                exit( 1 );
            }
            iArg ++;
        }
        else if( EQUAL(papszArgv[iArg],"-tg") ||
                 EQUAL(papszArgv[iArg],"-gt") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            nGroupTransactions = atoi(papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg],"-s_srs") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszSourceSRSDef = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg],"-a_srs") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszOutputSRSDef = papszArgv[++iArg];
            if (EQUAL(pszOutputSRSDef, "NULL") ||
                EQUAL(pszOutputSRSDef, "NONE"))
            {
                pszOutputSRSDef = NULL;
                bNullifyOutputSRS = TRUE;
            }
        }
        else if( EQUAL(papszArgv[iArg],"-t_srs") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszOutputSRSDef = papszArgv[++iArg];
            bTransform = TRUE;
        }
        else if( EQUAL(papszArgv[iArg],"-spat") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(4);
            OGRLinearRing  oRing;

            oRing.addPoint( atof(papszArgv[iArg+1]), atof(papszArgv[iArg+2]) );
            oRing.addPoint( atof(papszArgv[iArg+1]), atof(papszArgv[iArg+4]) );
            oRing.addPoint( atof(papszArgv[iArg+3]), atof(papszArgv[iArg+4]) );
            oRing.addPoint( atof(papszArgv[iArg+3]), atof(papszArgv[iArg+2]) );
            oRing.addPoint( atof(papszArgv[iArg+1]), atof(papszArgv[iArg+2]) );

            poSpatialFilter = OGRGeometryFactory::createGeometry(wkbPolygon);
            ((OGRPolygon *) poSpatialFilter)->addRing( &oRing );
            iArg += 4;
        }
        else if( EQUAL(papszArgv[iArg],"-where") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszWHERE = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg],"-select") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszSelect = papszArgv[++iArg];
            papszSelFields = CSLTokenizeStringComplex(pszSelect, " ,", 
                                                      FALSE, FALSE );
        }
        else if( EQUAL(papszArgv[iArg],"-segmentize") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            eGeomOp = SEGMENTIZE;
            dfGeomOpParam = atof(papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg],"-simplify") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            eGeomOp = SIMPLIFY_PRESERVE_TOPOLOGY;
            dfGeomOpParam = atof(papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg],"-fieldTypeToString") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            papszFieldTypesToString =
                    CSLTokenizeStringComplex(papszArgv[++iArg], " ,", 
                                             FALSE, FALSE );
            char** iter = papszFieldTypesToString;
            while(*iter)
            {
                if (EQUAL(*iter, "Integer") ||
                    EQUAL(*iter, "Real") ||
                    EQUAL(*iter, "String") ||
                    EQUAL(*iter, "Date") ||
                    EQUAL(*iter, "Time") ||
                    EQUAL(*iter, "DateTime") ||
                    EQUAL(*iter, "Binary") ||
                    EQUAL(*iter, "IntegerList") ||
                    EQUAL(*iter, "RealList") ||
                    EQUAL(*iter, "StringList"))
                {
                    /* Do nothing */
                }
                else if (EQUAL(*iter, "All"))
                {
                    CSLDestroy(papszFieldTypesToString);
                    papszFieldTypesToString = NULL;
                    papszFieldTypesToString = CSLAddString(papszFieldTypesToString, "All");
                    break;
                }
                else
                {
                    Usage(CPLSPrintf("Unhandled type for fieldtypeasstring option : %s",
                            *iter));
                }
                iter ++;
            }
        }
        else if( EQUAL(papszArgv[iArg],"-progress") )
        {
            bDisplayProgress = TRUE;
        }
        else if( EQUAL(papszArgv[iArg],"-wrapdateline") )
        {
            bWrapDateline = TRUE;
        }
        else if( EQUAL(papszArgv[iArg],"-datelineoffset") && iArg < nArgc-1 )
        {
            pszDateLineOffset = papszArgv[++iArg];
        }        
        else if( EQUAL(papszArgv[iArg],"-clipsrc") )
        {
            if (iArg + 1 >= nArgc)
                Usage(CPLSPrintf("%s option requires 1 or 4 arguments", papszArgv[iArg]));

            VSIStatBufL  sStat;
            bClipSrc = TRUE;
            if ( IsNumber(papszArgv[iArg+1])
                 && papszArgv[iArg+2] != NULL 
                 && papszArgv[iArg+3] != NULL 
                 && papszArgv[iArg+4] != NULL)
            {
                OGRLinearRing  oRing;

                oRing.addPoint( atof(papszArgv[iArg+1]), atof(papszArgv[iArg+2]) );
                oRing.addPoint( atof(papszArgv[iArg+1]), atof(papszArgv[iArg+4]) );
                oRing.addPoint( atof(papszArgv[iArg+3]), atof(papszArgv[iArg+4]) );
                oRing.addPoint( atof(papszArgv[iArg+3]), atof(papszArgv[iArg+2]) );
                oRing.addPoint( atof(papszArgv[iArg+1]), atof(papszArgv[iArg+2]) );

                poClipSrc = OGRGeometryFactory::createGeometry(wkbPolygon);
                ((OGRPolygon *) poClipSrc)->addRing( &oRing );
                iArg += 4;
            }
            else if ((EQUALN(papszArgv[iArg+1], "POLYGON", 7) ||
                      EQUALN(papszArgv[iArg+1], "MULTIPOLYGON", 12)) &&
                      VSIStatL(papszArgv[iArg+1], &sStat) != 0)
            {
                char* pszTmp = (char*) papszArgv[iArg+1];
                OGRGeometryFactory::createFromWkt(&pszTmp, NULL, &poClipSrc);
                if (poClipSrc == NULL)
                {
                    Usage("Invalid geometry. Must be a valid POLYGON or MULTIPOLYGON WKT");
                }
                iArg ++;
            }
            else if (EQUAL(papszArgv[iArg+1], "spat_extent") )
            {
                iArg ++;
            }
            else
            {
                pszClipSrcDS = papszArgv[iArg+1];
                iArg ++;
            }
        }
        else if( EQUAL(papszArgv[iArg],"-clipsrcsql")  )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszClipSrcSQL = papszArgv[iArg+1];
            iArg ++;
        }
        else if( EQUAL(papszArgv[iArg],"-clipsrclayer") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszClipSrcLayer = papszArgv[iArg+1];
            iArg ++;
        }
        else if( EQUAL(papszArgv[iArg],"-clipsrcwhere") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszClipSrcWhere = papszArgv[iArg+1];
            iArg ++;
        }
        else if( EQUAL(papszArgv[iArg],"-clipdst") )
        {
            if (iArg + 1 >= nArgc)
                Usage(CPLSPrintf("%s option requires 1 or 4 arguments", papszArgv[iArg]));

            VSIStatBufL  sStat;
            if ( IsNumber(papszArgv[iArg+1])
                 && papszArgv[iArg+2] != NULL 
                 && papszArgv[iArg+3] != NULL 
                 && papszArgv[iArg+4] != NULL)
            {
                OGRLinearRing  oRing;

                oRing.addPoint( atof(papszArgv[iArg+1]), atof(papszArgv[iArg+2]) );
                oRing.addPoint( atof(papszArgv[iArg+1]), atof(papszArgv[iArg+4]) );
                oRing.addPoint( atof(papszArgv[iArg+3]), atof(papszArgv[iArg+4]) );
                oRing.addPoint( atof(papszArgv[iArg+3]), atof(papszArgv[iArg+2]) );
                oRing.addPoint( atof(papszArgv[iArg+1]), atof(papszArgv[iArg+2]) );

                poClipDst = OGRGeometryFactory::createGeometry(wkbPolygon);
                ((OGRPolygon *) poClipDst)->addRing( &oRing );
                iArg += 4;
            }
            else if ((EQUALN(papszArgv[iArg+1], "POLYGON", 7) ||
                      EQUALN(papszArgv[iArg+1], "MULTIPOLYGON", 12)) &&
                      VSIStatL(papszArgv[iArg+1], &sStat) != 0)
            {
                char* pszTmp = (char*) papszArgv[iArg+1];
                OGRGeometryFactory::createFromWkt(&pszTmp, NULL, &poClipDst);
                if (poClipDst == NULL)
                {
                    Usage("Invalid geometry. Must be a valid POLYGON or MULTIPOLYGON WKT");
                }
                iArg ++;
            }
            else
            {
                pszClipDstDS = papszArgv[iArg+1];
                iArg ++;
            }
        }
        else if( EQUAL(papszArgv[iArg],"-clipdstsql") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszClipDstSQL = papszArgv[iArg+1];
            iArg ++;
        }
        else if( EQUAL(papszArgv[iArg],"-clipdstlayer") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszClipDstLayer = papszArgv[iArg+1];
            iArg ++;
        }
        else if( EQUAL(papszArgv[iArg],"-clipdstwhere") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszClipDstWhere = papszArgv[iArg+1];
            iArg ++;
        }
        else if( EQUAL(papszArgv[iArg],"-splitlistfields") )
        {
            bSplitListFields = TRUE;
        }
        else if ( EQUAL(papszArgv[iArg],"-maxsubfields") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            if (IsNumber(papszArgv[iArg+1]))
            {
                int nTemp = atoi(papszArgv[iArg+1]);
                if (nTemp > 0)
                {
                    nMaxSplitListSubFields = nTemp;
                    iArg ++;
                }
            }
        }
        else if( EQUAL(papszArgv[iArg],"-explodecollections") )
        {
            bExplodeCollections = TRUE;
        }
        else if( EQUAL(papszArgv[iArg],"-zfield") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszZField = papszArgv[iArg+1];
            iArg ++;
        }
        else if( EQUAL(papszArgv[iArg],"-gcp") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(4);
            char* endptr = NULL;
            /* -gcp pixel line easting northing [elev] */

            nGCPCount++;
            pasGCPs = (GDAL_GCP *) 
                CPLRealloc( pasGCPs, sizeof(GDAL_GCP) * nGCPCount );
            GDALInitGCPs( 1, pasGCPs + nGCPCount - 1 );

            pasGCPs[nGCPCount-1].dfGCPPixel = atof(papszArgv[++iArg]);
            pasGCPs[nGCPCount-1].dfGCPLine = atof(papszArgv[++iArg]);
            pasGCPs[nGCPCount-1].dfGCPX = atof(papszArgv[++iArg]);
            pasGCPs[nGCPCount-1].dfGCPY = atof(papszArgv[++iArg]);
            if( papszArgv[iArg+1] != NULL 
                && (CPLStrtod(papszArgv[iArg+1], &endptr) != 0.0 || papszArgv[iArg+1][0] == '0') )
            {
                /* Check that last argument is really a number and not a filename */
                /* looking like a number (see ticket #863) */
                if (endptr && *endptr == 0)
                    pasGCPs[nGCPCount-1].dfGCPZ = atof(papszArgv[++iArg]);
            }

            /* should set id and info? */
        }
        else if( EQUAL(papszArgv[iArg],"-tps") )
        {
            nTransformOrder = -1;
        }
        else if( EQUAL(papszArgv[iArg],"-order") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            nTransformOrder = atoi( papszArgv[++iArg] );
        }
        else if( EQUAL(papszArgv[iArg],"-fieldmap") && papszArgv[iArg+1] != NULL)
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszFieldMap = papszArgv[++iArg];
            papszFieldMap = CSLTokenizeStringComplex(pszFieldMap, ",", 
                                                      FALSE, FALSE );
        }
        else if( papszArgv[iArg][0] == '-' )
        {
            Usage(CPLSPrintf("Unkown option name '%s'", papszArgv[iArg]));
        }
        else if( pszDestDataSource == NULL )
            pszDestDataSource = papszArgv[iArg];
        else if( pszDataSource == NULL )
            pszDataSource = papszArgv[iArg];
        else
            papszLayers = CSLAddString( papszLayers, papszArgv[iArg] );
    }

    if( pszDataSource == NULL )
    {
        if( pszDestDataSource == NULL )
            Usage("no target datasource provided");
        else
            Usage("no source datasource provided");
    }

    if( bPreserveFID && bExplodeCollections )
    {
        Usage("cannot use -preserve_fid and -explodecollections at the same time.");
    }

    if (pszFieldMap && !bAppend)
    {
        Usage("if -fieldmap is specified, -append must also be specified");
    }

    if( bClipSrc && pszClipSrcDS != NULL)
    {
        poClipSrc = LoadGeometry(pszClipSrcDS, pszClipSrcSQL, pszClipSrcLayer, pszClipSrcWhere);
        if (poClipSrc == NULL)
        {
            Usage("cannot load source clip geometry");
        }
    }
    else if( bClipSrc && poClipSrc == NULL )
    {
        if (poSpatialFilter)
            poClipSrc = poSpatialFilter->clone();
        if (poClipSrc == NULL)
        {
            Usage("-clipsrc must be used with -spat option or a\n"
                             "bounding box, WKT string or datasource must be specified");
        }
    }
    
    if( pszClipDstDS != NULL)
    {
        poClipDst = LoadGeometry(pszClipDstDS, pszClipDstSQL, pszClipDstLayer, pszClipDstWhere);
        if (poClipDst == NULL)
        {
            Usage("cannot load dest clip geometry");
        }
    }

/* -------------------------------------------------------------------- */
/*      Open data source.                                               */
/* -------------------------------------------------------------------- */
    OGRDataSource       *poDS;
    OGRDataSource       *poODS = NULL;
    OGRSFDriver         *poDriver = NULL;
    int                  bCloseODS = TRUE;

    /* Avoid opening twice the same datasource if it is both the input and output */
    /* Known to cause problems with at least FGdb and SQlite drivers. See #4270 */
    if (bUpdate && strcmp(pszDestDataSource, pszDataSource) == 0)
    {
        poODS = poDS = OGRSFDriverRegistrar::Open( pszDataSource, TRUE, &poDriver );
        /* Restrict to those 2 drivers. For example it is known to break with */
        /* the PG driver due to the way it manages transactions... */
        if (poDS && !(EQUAL(poDriver->GetName(), "FileGDB") ||
                      EQUAL(poDriver->GetName(), "SQLite")))
        {
            poDS = OGRSFDriverRegistrar::Open( pszDataSource, FALSE );
        }
        else
            bCloseODS = FALSE;
        if (poDS)
        {
            if (bOverwrite || bAppend)
            {
                /* Various tests to avoid overwriting the source layer(s) */
                /* or to avoid appending a layer to itself */
                int bError = FALSE;
                if (pszNewLayerName == NULL)
                    bError = TRUE;
                else if (CSLCount(papszLayers) == 1)
                    bError = strcmp(pszNewLayerName, papszLayers[0]) == 0;
                else if (pszSQLStatement == NULL)
                    bError = TRUE;
                if (bError)
                {
                    fprintf( stderr,
                             "ERROR: -nln name must be specified combined with "
                             "a single source layer name,\nor a -sql statement, and "
                             "name must be different from an existing layer.\n");
                    exit(1);
                }
            }
        }
    }
    else
        poDS = OGRSFDriverRegistrar::Open( pszDataSource, FALSE );

/* -------------------------------------------------------------------- */
/*      Report failure                                                  */
/* -------------------------------------------------------------------- */
    if( poDS == NULL )
    {
        OGRSFDriverRegistrar    *poR = OGRSFDriverRegistrar::GetRegistrar();
        
        fprintf( stderr, "FAILURE:\n"
                "Unable to open datasource `%s' with the following drivers.\n",
                pszDataSource );

        for( int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
        {
            fprintf( stderr, "  -> %s\n", poR->GetDriver(iDriver)->GetName() );
        }

        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Try opening the output datasource as an existing, writable      */
/* -------------------------------------------------------------------- */

    if( bUpdate && poODS == NULL )
    {
        poODS = OGRSFDriverRegistrar::Open( pszDestDataSource, TRUE, &poDriver );

        if( poODS == NULL )
        {
            if (bOverwrite || bAppend)
            {
                poODS = OGRSFDriverRegistrar::Open( pszDestDataSource, FALSE, &poDriver );
                if (poODS == NULL)
                {
                    /* ok the datasource doesn't exist at all */
                    bUpdate = FALSE;
                }
                else
                {
                    OGRDataSource::DestroyDataSource(poODS);
                    poODS = NULL;
                }
            }

            if (bUpdate)
            {
                fprintf( stderr, "FAILURE:\n"
                        "Unable to open existing output datasource `%s'.\n",
                        pszDestDataSource );
                exit( 1 );
            }
        }
        else if( CSLCount(papszDSCO) > 0 )
        {
            fprintf( stderr, "WARNING: Datasource creation options ignored since an existing datasource\n"
                    "         being updated.\n" );
        }
    }

/* -------------------------------------------------------------------- */
/*      Find the output driver.                                         */
/* -------------------------------------------------------------------- */
    if( !bUpdate )
    {
        if (!bQuiet && !bFormatExplicitelySet)
            CheckDestDataSourceNameConsistency(pszDestDataSource, pszFormat);

        OGRSFDriverRegistrar *poR = OGRSFDriverRegistrar::GetRegistrar();
        int                  iDriver;

        poDriver = poR->GetDriverByName(pszFormat);
        if( poDriver == NULL )
        {
            fprintf( stderr, "Unable to find driver `%s'.\n", pszFormat );
            fprintf( stderr,  "The following drivers are available:\n" );
        
            for( iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
            {
                fprintf( stderr,  "  -> `%s'\n", poR->GetDriver(iDriver)->GetName() );
            }
            exit( 1 );
        }

        if( !poDriver->TestCapability( ODrCCreateDataSource ) )
        {
            fprintf( stderr,  "%s driver does not support data source creation.\n",
                    pszFormat );
            exit( 1 );
        }

/* -------------------------------------------------------------------- */
/*      Special case to improve user experience when translating        */
/*      a datasource with multiple layers into a shapefile. If the      */
/*      user gives a target datasource with .shp and it does not exist, */
/*      the shapefile driver will try to create a file, but this is not */
/*      appropriate because here we have several layers, so create      */
/*      a directory instead.                                            */
/* -------------------------------------------------------------------- */
        VSIStatBufL  sStat;
        if (EQUAL(poDriver->GetName(), "ESRI Shapefile") &&
            pszSQLStatement == NULL &&
            (CSLCount(papszLayers) > 1 ||
             (CSLCount(papszLayers) == 0 && poDS->GetLayerCount() > 1)) &&
            pszNewLayerName == NULL &&
            EQUAL(CPLGetExtension(pszDestDataSource), "SHP") &&
            VSIStatL(pszDestDataSource, &sStat) != 0)
        {
            if (VSIMkdir(pszDestDataSource, 0755) != 0)
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                      "Failed to create directory %s\n"
                      "for shapefile datastore.\n",
                      pszDestDataSource );
                exit(1);
            }
        }

/* -------------------------------------------------------------------- */
/*      Create the output data source.                                  */
/* -------------------------------------------------------------------- */
        poODS = poDriver->CreateDataSource( pszDestDataSource, papszDSCO );
        if( poODS == NULL )
        {
            fprintf( stderr,  "%s driver failed to create %s\n", 
                    pszFormat, pszDestDataSource );
            exit( 1 );
        }
    }

/* -------------------------------------------------------------------- */
/*      Parse the output SRS definition if possible.                    */
/* -------------------------------------------------------------------- */
    if( pszOutputSRSDef != NULL )
    {
        poOutputSRS = (OGRSpatialReference*)OSRNewSpatialReference(NULL);
        if( poOutputSRS->SetFromUserInput( pszOutputSRSDef ) != OGRERR_NONE )
        {
            fprintf( stderr,  "Failed to process SRS definition: %s\n", 
                    pszOutputSRSDef );
            exit( 1 );
        }
    }

/* -------------------------------------------------------------------- */
/*      Parse the source SRS definition if possible.                    */
/* -------------------------------------------------------------------- */
    if( pszSourceSRSDef != NULL )
    {
        poSourceSRS = (OGRSpatialReference*)OSRNewSpatialReference(NULL);
        if( poSourceSRS->SetFromUserInput( pszSourceSRSDef ) != OGRERR_NONE )
        {
            fprintf( stderr,  "Failed to process SRS definition: %s\n", 
                    pszSourceSRSDef );
            exit( 1 );
        }
    }
 
/* -------------------------------------------------------------------- */
/*      Create a transformation object from the source to               */
/*      destination coordinate system.                                  */
/* -------------------------------------------------------------------- */
    GCPCoordTransformation *poGCPCoordTrans = NULL;
    if( nGCPCount > 0 )
    {
        poGCPCoordTrans = new GCPCoordTransformation( nGCPCount, pasGCPs, 
                                                      nTransformOrder, 
                                                      poSourceSRS ? poSourceSRS : poOutputSRS );
        if( !(poGCPCoordTrans->IsValid()) )
        {
            delete poGCPCoordTrans;
            poGCPCoordTrans = NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      For OSM file.                                                   */
/* -------------------------------------------------------------------- */
    int         bSrcIsOSM = (poDS->GetDriver() != NULL &&
                             strcmp(poDS->GetDriver()->GetName(), "OSM") == 0);
    vsi_l_offset nSrcFileSize = 0;
    if( bSrcIsOSM && strcmp(poDS->GetName(), "/vsistdin/") != 0)
    {
        VSIStatBufL sStat;
        if( VSIStatL(poDS->GetName(), &sStat) == 0 )
            nSrcFileSize = sStat.st_size;
    }

/* -------------------------------------------------------------------- */
/*      Special case for -sql clause.  No source layers required.       */
/* -------------------------------------------------------------------- */
    if( pszSQLStatement != NULL )
    {
        OGRLayer *poResultSet;

        if( pszWHERE != NULL )
            fprintf( stderr,  "-where clause ignored in combination with -sql.\n" );
        if( CSLCount(papszLayers) > 0 )
            fprintf( stderr,  "layer names ignored in combination with -sql.\n" );
        
        poResultSet = poDS->ExecuteSQL( pszSQLStatement, poSpatialFilter, 
                                        pszDialect );

        if( poResultSet != NULL )
        {
            long nCountLayerFeatures = 0;
            if (bDisplayProgress)
            {
                if (bSrcIsOSM)
                {
                    pfnProgress = GDALTermProgress;
                }
                else if (!poResultSet->TestCapability(OLCFastFeatureCount))
                {
                    fprintf( stderr, "Progress turned off as fast feature count is not available.\n");
                    bDisplayProgress = FALSE;
                }
                else
                {
                    nCountLayerFeatures = poResultSet->GetFeatureCount();
                    pfnProgress = GDALTermProgress;
                }
            }

            OGRLayer* poPassedLayer = poResultSet;
            if (bSplitListFields)
            {
                poPassedLayer = new OGRSplitListFieldLayer(poPassedLayer, nMaxSplitListSubFields);
                int nRet = ((OGRSplitListFieldLayer*)poPassedLayer)->BuildLayerDefn(NULL, NULL);
                if (!nRet)
                {
                    delete poPassedLayer;
                    poPassedLayer = poResultSet;
                }
            }

/* -------------------------------------------------------------------- */
/*      Special case to improve user experience when translating into   */
/*      single file shapefile and source has only one layer, and that   */
/*      the layer name isn't specified                                  */
/* -------------------------------------------------------------------- */
            VSIStatBufL  sStat;
            if (EQUAL(poDriver->GetName(), "ESRI Shapefile") &&
                pszNewLayerName == NULL &&
                VSIStatL(pszDestDataSource, &sStat) == 0 && VSI_ISREG(sStat.st_mode))
            {
                pszNewLayerName = CPLStrdup(CPLGetBasename(pszDestDataSource));
            }

            TargetLayerInfo* psInfo = SetupTargetLayer( poDS,
                                                poPassedLayer,
                                                poODS,
                                                papszLCO,
                                                pszNewLayerName,
                                                poOutputSRS,
                                                bNullifyOutputSRS,
                                                papszSelFields,
                                                bAppend, eGType,
                                                bPromoteToMulti,
                                                nCoordDim, bOverwrite,
                                                papszFieldTypesToString,
                                                bExplodeCollections,
                                                pszZField,
                                                papszFieldMap,
                                                pszWHERE );

            poPassedLayer->ResetReading();

            if( psInfo == NULL ||
                !TranslateLayer( psInfo, poDS, poPassedLayer, poODS,
                                 bTransform, bWrapDateline, pszDateLineOffset,
                                 poOutputSRS, bNullifyOutputSRS,
                                 poSourceSRS,
                                 poGCPCoordTrans,
                                 eGType, bPromoteToMulti, nCoordDim,
                                 eGeomOp, dfGeomOpParam,
                                 nCountLayerFeatures,
                                 poClipSrc, poClipDst,
                                 bExplodeCollections,
                                 nSrcFileSize, NULL,
                                 pfnProgress, pProgressArg ))
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Terminating translation prematurely after failed\n"
                          "translation from sql statement." );

                exit( 1 );
            }

            FreeTargetLayerInfo(psInfo);

            if (poPassedLayer != poResultSet)
                delete poPassedLayer;

            poDS->ReleaseResultSet( poResultSet );
        }
        else
        {
            if( CPLGetLastErrorNo() != 0 )
                nRetCode = 1;
        }
    }

/* -------------------------------------------------------------------- */
/*      Special case for layer interleaving mode.                       */
/* -------------------------------------------------------------------- */
    else if( bSrcIsOSM &&
                CSLTestBoolean(CPLGetConfigOption("OGR_INTERLEAVED_READING", "YES")) )
    {
        CPLSetConfigOption("OGR_INTERLEAVED_READING", "YES");

        if (bSplitListFields)
        {
            fprintf( stderr, "FAILURE: -splitlistfields not supported in this mode\n" );
            exit( 1 );
        }

        int nSrcLayerCount = poDS->GetLayerCount();
        AssociatedLayers* pasAssocLayers =
            (AssociatedLayers* ) CPLCalloc(nSrcLayerCount, sizeof(AssociatedLayers));

/* -------------------------------------------------------------------- */
/*      Special case to improve user experience when translating into   */
/*      single file shapefile and source has only one layer, and that   */
/*      the layer name isn't specified                                  */
/* -------------------------------------------------------------------- */
        VSIStatBufL  sStat;
        if (EQUAL(poDriver->GetName(), "ESRI Shapefile") &&
            (CSLCount(papszLayers) == 1 || nSrcLayerCount == 1) && pszNewLayerName == NULL &&
            VSIStatL(pszDestDataSource, &sStat) == 0 && VSI_ISREG(sStat.st_mode))
        {
            pszNewLayerName = CPLStrdup(CPLGetBasename(pszDestDataSource));
        }

        if ( bDisplayProgress && bSrcIsOSM )
            pfnProgress = GDALTermProgress;

/* -------------------------------------------------------------------- */
/*      If no target layer specified, use all source layers.            */
/* -------------------------------------------------------------------- */
        int iLayer;
        if ( CSLCount(papszLayers) == 0)
        {
            papszLayers = (char**) CPLCalloc(sizeof(char*), nSrcLayerCount + 1);
            for( iLayer = 0; iLayer < nSrcLayerCount; iLayer++ )
            {
                OGRLayer        *poLayer = poDS->GetLayer(iLayer);

                if( poLayer == NULL )
                {
                    fprintf( stderr, "FAILURE: Couldn't fetch advertised layer %d!\n",
                            iLayer );
                    exit( 1 );
                }

                papszLayers[iLayer] = CPLStrdup(poLayer->GetName());
            }
        }
        else
        {
            if ( bSrcIsOSM )
            {
                CPLString osInterestLayers = "SET interest_layers =";
                for( iLayer = 0; papszLayers[iLayer] != NULL; iLayer++ )
                {
                    if( iLayer != 0 ) osInterestLayers += ",";
                    osInterestLayers += papszLayers[iLayer];
                }

                poDS->ExecuteSQL(osInterestLayers.c_str(), NULL, NULL);
            }
        }

/* -------------------------------------------------------------------- */
/*      First pass to set filters and create target layers.             */
/* -------------------------------------------------------------------- */
        for( iLayer = 0; iLayer < nSrcLayerCount; iLayer++ )
        {
            OGRLayer        *poLayer = poDS->GetLayer(iLayer);
            if( poLayer == NULL )
            {
                fprintf( stderr, "FAILURE: Couldn't fetch advertised layer %d!\n",
                        iLayer );
                exit( 1 );
            }

            pasAssocLayers[iLayer].poSrcLayer = poLayer;

            if( CSLFindString(papszLayers, poLayer->GetName()) >= 0 )
            {
                if( pszWHERE != NULL )
                {
                    if( poLayer->SetAttributeFilter( pszWHERE ) != OGRERR_NONE )
                    {
                        fprintf( stderr, "FAILURE: SetAttributeFilter(%s) on layer '%s' failed.\n",
                                 pszWHERE, poLayer->GetName() );
                        if (!bSkipFailures)
                            exit( 1 );
                    }
                }

                if( poSpatialFilter != NULL )
                    poLayer->SetSpatialFilter( poSpatialFilter );

                TargetLayerInfo* psInfo = SetupTargetLayer( poDS,
                                                    poLayer,
                                                    poODS,
                                                    papszLCO,
                                                    pszNewLayerName,
                                                    poOutputSRS,
                                                    bNullifyOutputSRS,
                                                    papszSelFields,
                                                    bAppend, eGType,
                                                    bPromoteToMulti,
                                                    nCoordDim, bOverwrite,
                                                    papszFieldTypesToString,
                                                    bExplodeCollections,
                                                    pszZField,
                                                    papszFieldMap,
                                                    pszWHERE );

                if( psInfo == NULL && !bSkipFailures )
                    exit(1);

                pasAssocLayers[iLayer].psInfo = psInfo;
            }
            else
            {
                pasAssocLayers[iLayer].psInfo = NULL;
            }
        }

/* -------------------------------------------------------------------- */
/*      Second pass to process features in a interleaved layer mode.    */
/* -------------------------------------------------------------------- */
        int bHasLayersNonEmpty;
        do
        {
            bHasLayersNonEmpty = FALSE;

            for( iLayer = 0; iLayer < nSrcLayerCount;  iLayer++ )
            {
                OGRLayer        *poLayer = pasAssocLayers[iLayer].poSrcLayer;
                TargetLayerInfo *psInfo = pasAssocLayers[iLayer].psInfo;
                GIntBig nReadFeatureCount = 0;

                if( psInfo )
                {
                    if( !TranslateLayer(psInfo, poDS, poLayer, poODS,
                                        bTransform, bWrapDateline, pszDateLineOffset,
                                        poOutputSRS, bNullifyOutputSRS,
                                        poSourceSRS,
                                        poGCPCoordTrans,
                                        eGType, bPromoteToMulti, nCoordDim,
                                        eGeomOp, dfGeomOpParam,
                                        0,
                                        poClipSrc, poClipDst,
                                        bExplodeCollections,
                                        nSrcFileSize,
                                        &nReadFeatureCount,
                                        pfnProgress, pProgressArg )
                        && !bSkipFailures )
                    {
                        CPLError( CE_Failure, CPLE_AppDefined,
                                "Terminating translation prematurely after failed\n"
                                "translation of layer %s (use -skipfailures to skip errors)\n",
                                poLayer->GetName() );

                        exit( 1 );
                    }
                }
                else
                {
                    /* No matching target layer : just consumes the features */

                    OGRFeature* poFeature;
                    while( (poFeature = poLayer->GetNextFeature()) != NULL )
                    {
                        nReadFeatureCount ++;
                        OGRFeature::DestroyFeature(poFeature);
                    }
                }

                if( nReadFeatureCount != 0 )
                    bHasLayersNonEmpty = TRUE;
            }
        }
        while( bHasLayersNonEmpty );

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
        for( iLayer = 0; iLayer < nSrcLayerCount;  iLayer++ )
        {
            if( pasAssocLayers[iLayer].psInfo )
                FreeTargetLayerInfo(pasAssocLayers[iLayer].psInfo);
        }
        CPLFree(pasAssocLayers);
    }

    else
    {
        int nLayerCount = 0;
        OGRLayer** papoLayers = NULL;

/* -------------------------------------------------------------------- */
/*      Process each data source layer.                                 */
/* -------------------------------------------------------------------- */
        if ( CSLCount(papszLayers) == 0)
        {
            nLayerCount = poDS->GetLayerCount();
            papoLayers = (OGRLayer**)CPLMalloc(sizeof(OGRLayer*) * nLayerCount);

            for( int iLayer = 0; 
                 iLayer < nLayerCount; 
                 iLayer++ )
            {
                OGRLayer        *poLayer = poDS->GetLayer(iLayer);

                if( poLayer == NULL )
                {
                    fprintf( stderr, "FAILURE: Couldn't fetch advertised layer %d!\n",
                            iLayer );
                    exit( 1 );
                }

                papoLayers[iLayer] = poLayer;
            }
        }
/* -------------------------------------------------------------------- */
/*      Process specified data source layers.                           */
/* -------------------------------------------------------------------- */
        else
        {
            nLayerCount = CSLCount(papszLayers);
            papoLayers = (OGRLayer**)CPLMalloc(sizeof(OGRLayer*) * nLayerCount);

            for( int iLayer = 0; 
                papszLayers[iLayer] != NULL; 
                iLayer++ )
            {
                OGRLayer        *poLayer = poDS->GetLayerByName(papszLayers[iLayer]);

                if( poLayer == NULL )
                {
                    fprintf( stderr, "FAILURE: Couldn't fetch requested layer '%s'!\n",
                             papszLayers[iLayer] );
                    if (!bSkipFailures)
                        exit( 1 );
                }

                papoLayers[iLayer] = poLayer;
            }
        }

/* -------------------------------------------------------------------- */
/*      Special case to improve user experience when translating into   */
/*      single file shapefile and source has only one layer, and that   */
/*      the layer name isn't specified                                  */
/* -------------------------------------------------------------------- */
        VSIStatBufL  sStat;
        if (EQUAL(poDriver->GetName(), "ESRI Shapefile") &&
            nLayerCount == 1 && pszNewLayerName == NULL &&
            VSIStatL(pszDestDataSource, &sStat) == 0 && VSI_ISREG(sStat.st_mode))
        {
            pszNewLayerName = CPLStrdup(CPLGetBasename(pszDestDataSource));
        }

        long* panLayerCountFeatures = (long*) CPLCalloc(sizeof(long), nLayerCount);
        long nCountLayersFeatures = 0;
        long nAccCountFeatures = 0;
        int iLayer;

        /* First pass to apply filters and count all features if necessary */
        for( iLayer = 0; 
            iLayer < nLayerCount; 
            iLayer++ )
        {
            OGRLayer        *poLayer = papoLayers[iLayer];
            if (poLayer == NULL)
                continue;

            if( pszWHERE != NULL )
            {
                if( poLayer->SetAttributeFilter( pszWHERE ) != OGRERR_NONE )
                {
                    fprintf( stderr, "FAILURE: SetAttributeFilter(%s) on layer '%s' failed.\n",
                             pszWHERE, poLayer->GetName() );
                    if (!bSkipFailures)
                        exit( 1 );
                }
            }

            if( poSpatialFilter != NULL )
                poLayer->SetSpatialFilter( poSpatialFilter );

            if (bDisplayProgress && !bSrcIsOSM)
            {
                if (!poLayer->TestCapability(OLCFastFeatureCount))
                {
                    fprintf( stderr, "Progress turned off as fast feature count is not available.\n");
                    bDisplayProgress = FALSE;
                }
                else
                {
                    panLayerCountFeatures[iLayer] = poLayer->GetFeatureCount();
                    nCountLayersFeatures += panLayerCountFeatures[iLayer];
                }
            }
        }

        /* Second pass to do the real job */
        for( iLayer = 0; 
            iLayer < nLayerCount; 
            iLayer++ )
        {
            OGRLayer        *poLayer = papoLayers[iLayer];
            if (poLayer == NULL)
                continue;


            OGRLayer* poPassedLayer = poLayer;
            if (bSplitListFields)
            {
                poPassedLayer = new OGRSplitListFieldLayer(poPassedLayer, nMaxSplitListSubFields);

                if (bDisplayProgress && nMaxSplitListSubFields != 1)
                {
                    pfnProgress = GDALScaledProgress;
                    pProgressArg = 
                        GDALCreateScaledProgress(nAccCountFeatures * 1.0 / nCountLayersFeatures,
                                                (nAccCountFeatures + panLayerCountFeatures[iLayer] / 2) * 1.0 / nCountLayersFeatures,
                                                GDALTermProgress,
                                                NULL);
                }
                else
                {
                    pfnProgress = NULL;
                    pProgressArg = NULL;
                }

                int nRet = ((OGRSplitListFieldLayer*)poPassedLayer)->BuildLayerDefn(pfnProgress, pProgressArg);
                if (!nRet)
                {
                    delete poPassedLayer;
                    poPassedLayer = poLayer;
                }

                if (bDisplayProgress)
                    GDALDestroyScaledProgress(pProgressArg);
            }


            if (bDisplayProgress)
            {
                if ( bSrcIsOSM )
                    pfnProgress = GDALTermProgress;
                else
                {
                    pfnProgress = GDALScaledProgress;
                    int nStart = 0;
                    if (poPassedLayer != poLayer && nMaxSplitListSubFields != 1)
                        nStart = panLayerCountFeatures[iLayer] / 2;
                    pProgressArg =
                        GDALCreateScaledProgress((nAccCountFeatures + nStart) * 1.0 / nCountLayersFeatures,
                                                (nAccCountFeatures + panLayerCountFeatures[iLayer]) * 1.0 / nCountLayersFeatures,
                                                GDALTermProgress,
                                                NULL);
                }
            }

            nAccCountFeatures += panLayerCountFeatures[iLayer];

            TargetLayerInfo* psInfo = SetupTargetLayer( poDS,
                                                poPassedLayer,
                                                poODS,
                                                papszLCO,
                                                pszNewLayerName,
                                                poOutputSRS,
                                                bNullifyOutputSRS,
                                                papszSelFields,
                                                bAppend, eGType,
                                                bPromoteToMulti,
                                                nCoordDim, bOverwrite,
                                                papszFieldTypesToString,
                                                bExplodeCollections,
                                                pszZField,
                                                papszFieldMap,
                                                pszWHERE );

            poPassedLayer->ResetReading();

            if( (psInfo == NULL ||
                !TranslateLayer( psInfo, poDS, poPassedLayer, poODS,
                                  bTransform, bWrapDateline, pszDateLineOffset,
                                  poOutputSRS, bNullifyOutputSRS,
                                  poSourceSRS,
                                  poGCPCoordTrans,
                                  eGType, bPromoteToMulti, nCoordDim,
                                  eGeomOp, dfGeomOpParam,
                                  panLayerCountFeatures[iLayer],
                                  poClipSrc, poClipDst,
                                  bExplodeCollections,
                                  nSrcFileSize, NULL,
                                  pfnProgress, pProgressArg ))
                && !bSkipFailures )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                        "Terminating translation prematurely after failed\n"
                        "translation of layer %s (use -skipfailures to skip errors)\n", 
                        poLayer->GetName() );

                exit( 1 );
            }

            FreeTargetLayerInfo(psInfo);

            if (poPassedLayer != poLayer)
                delete poPassedLayer;

            if (bDisplayProgress && !bSrcIsOSM)
                GDALDestroyScaledProgress(pProgressArg);
        }

        CPLFree(panLayerCountFeatures);
        CPLFree(papoLayers);
    }
/* -------------------------------------------------------------------- */
/*      Process DS style table                                          */
/* -------------------------------------------------------------------- */

    poODS->SetStyleTable( poDS->GetStyleTable () );
    
/* -------------------------------------------------------------------- */
/*      Close down.                                                     */
/* -------------------------------------------------------------------- */
    if (bCloseODS)
        OGRDataSource::DestroyDataSource(poODS);
    OGRDataSource::DestroyDataSource(poDS);
    OGRGeometryFactory::destroyGeometry(poSpatialFilter);
    OGRGeometryFactory::destroyGeometry(poClipSrc);
    OGRGeometryFactory::destroyGeometry(poClipDst);

    delete poGCPCoordTrans;
    if( pasGCPs != NULL )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPs );
        CPLFree( pasGCPs );
    }

    /* Destroy them after the last potential user */
    OGRSpatialReference::DestroySpatialReference(poOutputSRS);
    OGRSpatialReference::DestroySpatialReference(poSourceSRS);

    CSLDestroy(papszSelFields);
    CSLDestroy( papszFieldMap );
    CSLDestroy( papszArgv );
    CSLDestroy( papszLayers );
    CSLDestroy( papszDSCO );
    CSLDestroy( papszLCO );
    CSLDestroy( papszFieldTypesToString );
    CPLFree( pszNewLayerName );

    OGRCleanupAll();

#ifdef DBMALLOC
    malloc_dump(1);
#endif
    
    return nRetCode;
}

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage(int bShort)
{
    Usage(NULL, bShort);
}

static void Usage(const char* pszAdditionalMsg, int bShort)

{
    OGRSFDriverRegistrar        *poR = OGRSFDriverRegistrar::GetRegistrar();


    printf( "Usage: ogr2ogr [--help-general] [-skipfailures] [-append] [-update]\n"
            "               [-select field_list] [-where restricted_where]\n"
            "               [-progress] [-sql <sql statement>] [-dialect dialect]\n"
            "               [-preserve_fid] [-fid FID]\n"
            "               [-spat xmin ymin xmax ymax]\n"
            "               [-a_srs srs_def] [-t_srs srs_def] [-s_srs srs_def]\n"
            "               [-f format_name] [-overwrite] [[-dsco NAME=VALUE] ...]\n"
            "               dst_datasource_name src_datasource_name\n"
            "               [-lco NAME=VALUE] [-nln name] [-nlt type] [-dim 2|3] [layer [layer ...]]\n"
            "\n"
            "Advanced options :\n"
            "               [-gt n]\n"
            "               [-clipsrc [xmin ymin xmax ymax]|WKT|datasource|spat_extent]\n"
            "               [-clipsrcsql sql_statement] [-clipsrclayer layer]\n"
            "               [-clipsrcwhere expression]\n"
            "               [-clipdst [xmin ymin xmax ymax]|WKT|datasource]\n"
            "               [-clipdstsql sql_statement] [-clipdstlayer layer]\n"
            "               [-clipdstwhere expression]\n"
            "               [-wrapdateline][-datelineoffset val]\n"
            "               [[-simplify tolerance] | [-segmentize max_dist]]\n"
            "               [-fieldTypeToString All|(type1[,type2]*)]\n"
            "               [-fieldmap identity | index1[,index2]*]\n"
            "               [-splitlistfields] [-maxsubfields val]\n"
            "               [-explodecollections] [-zfield field_name]\n"
            "               [-gcp pixel line easting northing [elevation]]* [-order n | -tps]\n");

    if (bShort)
    {
        printf( "\nNote: ogr2ogr --long-usage for full help.\n");
        if( pszAdditionalMsg )
            fprintf(stderr, "\nFAILURE: %s\n", pszAdditionalMsg);
        exit( 1 );
    }

    printf("\n -f format_name: output file format name, possible values are:\n");

    for( int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
    {
        OGRSFDriver *poDriver = poR->GetDriver(iDriver);

        if( poDriver->TestCapability( ODrCCreateDataSource ) )
            printf( "     -f \"%s\"\n", poDriver->GetName() );
    }

    printf( " -append: Append to existing layer instead of creating new if it exists\n"
            " -overwrite: delete the output layer and recreate it empty\n"
            " -update: Open existing output datasource in update mode\n"
            " -progress: Display progress on terminal. Only works if input layers have the \n"
            "                                          \"fast feature count\" capability\n"
            " -select field_list: Comma-delimited list of fields from input layer to\n"
            "                     copy to the new layer (defaults to all)\n" 
            " -where restricted_where: Attribute query (like SQL WHERE)\n" 
            " -wrapdateline: split geometries crossing the dateline meridian\n"
            "                (long. = +/- 180deg)\n" 
            " -datelineoffset: offset from dateline in degrees\n"
            "                (default long. = +/- 10deg,\n"
            "                geometries within 170deg to -170deg will be splited)\n" 
            " -sql statement: Execute given SQL statement and save result.\n"
            " -dialect value: select a dialect, usually OGRSQL to avoid native sql.\n"
            " -skipfailures: skip features or layers that fail to convert\n"
            " -gt n: group n features per transaction (default 200)\n"
            " -spat xmin ymin xmax ymax: spatial query extents\n"
            " -simplify tolerance: distance tolerance for simplification.\n"
            " -segmentize max_dist: maximum distance between 2 nodes.\n"
            "                       Used to create intermediate points\n"
            " -dsco NAME=VALUE: Dataset creation option (format specific)\n"
            " -lco  NAME=VALUE: Layer creation option (format specific)\n"
            " -nln name: Assign an alternate name to the new layer\n"
            " -nlt type: Force a geometry type for new layer.  One of NONE, GEOMETRY,\n"
            "      POINT, LINESTRING, POLYGON, GEOMETRYCOLLECTION, MULTIPOINT,\n"
            "      MULTIPOLYGON, or MULTILINESTRING, or PROMOTE_TO_MULTI.  Add \"25D\" for 3D layers.\n"
            "      Default is type of source layer.\n"
            " -dim dimension: Force the coordinate dimension to the specified value.\n"
            " -fieldTypeToString type1,...: Converts fields of specified types to\n"
            "      fields of type string in the new layer. Valid types are : Integer,\n"
            "      Real, String, Date, Time, DateTime, Binary, IntegerList, RealList,\n"
            "      StringList. Special value All will convert all fields to strings.\n"
            " -fieldmap index1,index2,...: Specifies the list of field indexes to be\n"
            "      copied from the source to the destination. The (n)th value specified\n"
            "      in the list is the index of the field in the target layer definition\n"
            "      in which the n(th) field of the source layer must be copied. Index count\n"
            "      starts at zero. There must be exactly as many values in the list as\n"
            "      the count of the fields in the source layer. We can use the 'identity'\n"
            "      setting to specify that the fields should be transferred by using the\n"
            "      same order. This setting should be used along with the append setting.");

    printf(" -a_srs srs_def: Assign an output SRS\n"
           " -t_srs srs_def: Reproject/transform to this SRS on output\n"
           " -s_srs srs_def: Override source SRS\n"
           "\n" 
           " Srs_def can be a full WKT definition (hard to escape properly),\n"
           " or a well known definition (ie. EPSG:4326) or a file with a WKT\n"
           " definition.\n" );

    if( pszAdditionalMsg )
        fprintf(stderr, "\nFAILURE: %s\n", pszAdditionalMsg);

    exit( 1 );
}

/************************************************************************/
/*                               SetZ()                                 */
/************************************************************************/
static void SetZ (OGRGeometry* poGeom, double dfZ )
{
    if (poGeom == NULL)
        return;
    switch (wkbFlatten(poGeom->getGeometryType()))
    {
        case wkbPoint:
            ((OGRPoint*)poGeom)->setZ(dfZ);
            break;

        case wkbLineString:
        case wkbLinearRing:
        {
            int i;
            OGRLineString* poLS = (OGRLineString*) poGeom;
            for(i=0;i<poLS->getNumPoints();i++)
                poLS->setPoint(i, poLS->getX(i), poLS->getY(i), dfZ);
            break;
        }

        case wkbPolygon:
        {
            int i;
            OGRPolygon* poPoly = (OGRPolygon*) poGeom;
            SetZ(poPoly->getExteriorRing(), dfZ);
            for(i=0;i<poPoly->getNumInteriorRings();i++)
                SetZ(poPoly->getInteriorRing(i), dfZ);
            break;
        }

        case wkbMultiPoint:
        case wkbMultiLineString:
        case wkbMultiPolygon:
        case wkbGeometryCollection:
        {
            int i;
            OGRGeometryCollection* poGeomColl = (OGRGeometryCollection*) poGeom;
            for(i=0;i<poGeomColl->getNumGeometries();i++)
                SetZ(poGeomColl->getGeometryRef(i), dfZ);
            break;
        }

        default:
            break;
    }
}

/************************************************************************/
/*                         SetupTargetLayer()                           */
/************************************************************************/

static TargetLayerInfo* SetupTargetLayer( OGRDataSource *poSrcDS,
                                                OGRLayer * poSrcLayer,
                                                OGRDataSource *poDstDS,
                                                char **papszLCO,
                                                const char *pszNewLayerName,
                                                OGRSpatialReference *poOutputSRS,
                                                int bNullifyOutputSRS,
                                                char **papszSelFields,
                                                int bAppend, int eGType,
                                                int bPromoteToMulti,
                                                int nCoordDim, int bOverwrite,
                                                char** papszFieldTypesToString,
                                                int bExplodeCollections,
                                                const char* pszZField,
                                                char **papszFieldMap,
                                                const char* pszWHERE )
{
    OGRLayer    *poDstLayer;
    OGRFeatureDefn *poSrcFDefn;
    OGRFeatureDefn *poDstFDefn = NULL;

    if( pszNewLayerName == NULL )
        pszNewLayerName = poSrcLayer->GetName();

/* -------------------------------------------------------------------- */
/*      Get other info.                                                 */
/* -------------------------------------------------------------------- */
    poSrcFDefn = poSrcLayer->GetLayerDefn();

    if( poOutputSRS == NULL && !bNullifyOutputSRS )
        poOutputSRS = poSrcLayer->GetSpatialRef();

/* -------------------------------------------------------------------- */
/*      Find the layer.                                                 */
/* -------------------------------------------------------------------- */

    /* GetLayerByName() can instanciate layers that would have been */
    /* 'hidden' otherwise, for example, non-spatial tables in a */
    /* Postgis-enabled database, so this apparently useless command is */
    /* not useless... (#4012) */
    CPLPushErrorHandler(CPLQuietErrorHandler);
    poDstLayer = poDstDS->GetLayerByName(pszNewLayerName);
    CPLPopErrorHandler();
    CPLErrorReset();

    int iLayer = -1;
    if (poDstLayer != NULL)
    {
        int nLayerCount = poDstDS->GetLayerCount();
        for( iLayer = 0; iLayer < nLayerCount; iLayer++ )
        {
            OGRLayer        *poLayer = poDstDS->GetLayer(iLayer);
            if (poLayer == poDstLayer)
                break;
        }

        if (iLayer == nLayerCount)
            /* shouldn't happen with an ideal driver */
            poDstLayer = NULL;
    }

/* -------------------------------------------------------------------- */
/*      If the user requested overwrite, and we have the layer in       */
/*      question we need to delete it now so it will get recreated      */
/*      (overwritten).                                                  */
/* -------------------------------------------------------------------- */
    if( poDstLayer != NULL && bOverwrite )
    {
        if( poDstDS->DeleteLayer( iLayer ) != OGRERR_NONE )
        {
            fprintf( stderr,
                     "DeleteLayer() failed when overwrite requested.\n" );
            return NULL;
        }
        poDstLayer = NULL;
    }

/* -------------------------------------------------------------------- */
/*      If the layer does not exist, then create it.                    */
/* -------------------------------------------------------------------- */
    if( poDstLayer == NULL )
    {
        if( eGType == -2 )
        {
            eGType = poSrcFDefn->GetGeomType();

            int n25DBit = eGType & wkb25DBit;
            if ( bPromoteToMulti )
            {
                if (wkbFlatten(eGType) == wkbLineString)
                    eGType = wkbMultiLineString | n25DBit;
                else if (wkbFlatten(eGType) == wkbPolygon)
                    eGType = wkbMultiPolygon | n25DBit;
            }

            if ( bExplodeCollections )
            {
                if (wkbFlatten(eGType) == wkbMultiPoint)
                {
                    eGType = wkbPoint | n25DBit;
                }
                else if (wkbFlatten(eGType) == wkbMultiLineString)
                {
                    eGType = wkbLineString | n25DBit;
                }
                else if (wkbFlatten(eGType) == wkbMultiPolygon)
                {
                    eGType = wkbPolygon | n25DBit;
                }
                else if (wkbFlatten(eGType) == wkbGeometryCollection)
                {
                    eGType = wkbUnknown | n25DBit;
                }
            }

            if ( pszZField )
                eGType |= wkb25DBit;
        }

        if( nCoordDim == 2 )
            eGType &= ~wkb25DBit;
        else if( nCoordDim == 3 )
            eGType |= wkb25DBit;

        if( !poDstDS->TestCapability( ODsCCreateLayer ) )
        {
            fprintf( stderr,
              "Layer %s not found, and CreateLayer not supported by driver.\n",
                     pszNewLayerName );
            return NULL;
        }

        CPLErrorReset();

        poDstLayer = poDstDS->CreateLayer( pszNewLayerName, poOutputSRS,
                                           (OGRwkbGeometryType) eGType,
                                           papszLCO );

        if( poDstLayer == NULL )
            return NULL;

        bAppend = FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we will append to it, if append was requested.        */
/* -------------------------------------------------------------------- */
    else if( !bAppend )
    {
        fprintf( stderr, "FAILED: Layer %s already exists, and -append not specified.\n"
                "        Consider using -append, or -overwrite.\n",
                pszNewLayerName );
        return NULL;
    }
    else
    {
        if( CSLCount(papszLCO) > 0 )
        {
            fprintf( stderr, "WARNING: Layer creation options ignored since an existing layer is\n"
                    "         being appended to.\n" );
        }
    }

/* -------------------------------------------------------------------- */
/*      Process Layer style table                                       */
/* -------------------------------------------------------------------- */

    poDstLayer->SetStyleTable( poSrcLayer->GetStyleTable () );
/* -------------------------------------------------------------------- */
/*      Add fields.  Default to copy all field.                         */
/*      If only a subset of all fields requested, then output only      */
/*      the selected fields, and in the order that they were            */
/*      selected.                                                       */
/* -------------------------------------------------------------------- */
    int         nSrcFieldCount = poSrcFDefn->GetFieldCount();
    int         iField, *panMap;

    // Initialize the index-to-index map to -1's
    panMap = (int *) VSIMalloc( sizeof(int) * nSrcFieldCount );
    for( iField=0; iField < nSrcFieldCount; iField++)
        panMap[iField] = -1;

    /* Caution : at the time of writing, the MapInfo driver */
    /* returns NULL until a field has been added */
    poDstFDefn = poDstLayer->GetLayerDefn();

    if (papszFieldMap && bAppend)
    {
        int bIdentity = FALSE;

        if (EQUAL(papszFieldMap[0], "identity"))
            bIdentity = TRUE;
        else if (CSLCount(papszFieldMap) != nSrcFieldCount)
        {
            fprintf( stderr, "Field map should contain the value 'identity' or the same number of integer values as the source field count.\n");
            VSIFree(panMap);
            return NULL;
        }

        for( iField=0; iField < nSrcFieldCount; iField++)
        {
            panMap[iField] = bIdentity? iField : atoi(papszFieldMap[iField]);
            if (panMap[iField] >= poDstFDefn->GetFieldCount())
            {
                fprintf( stderr, "Invalid destination field index %d.\n", panMap[iField]);
                VSIFree(panMap);
                return NULL;
            }
        }
    }
    else if (papszSelFields && !bAppend )
    {
        int  nDstFieldCount = 0;
        if (poDstFDefn)
            nDstFieldCount = poDstFDefn->GetFieldCount();
        for( iField=0; papszSelFields[iField] != NULL; iField++)
        {
            int iSrcField = poSrcFDefn->GetFieldIndex(papszSelFields[iField]);
            if (iSrcField >= 0)
            {
                OGRFieldDefn* poSrcFieldDefn = poSrcFDefn->GetFieldDefn(iSrcField);
                OGRFieldDefn oFieldDefn( poSrcFieldDefn );

                if (papszFieldTypesToString != NULL &&
                    (CSLFindString(papszFieldTypesToString, "All") != -1 ||
                     CSLFindString(papszFieldTypesToString,
                                   OGRFieldDefn::GetFieldTypeName(poSrcFieldDefn->GetType())) != -1))
                {
                    oFieldDefn.SetType(OFTString);
                }

                /* The field may have been already created at layer creation */
                int iDstField = -1;
                if (poDstFDefn)
                    iDstField = poDstFDefn->GetFieldIndex(oFieldDefn.GetNameRef());
                if (iDstField >= 0)
                {
                    panMap[iSrcField] = iDstField;
                }
                else if (poDstLayer->CreateField( &oFieldDefn ) == OGRERR_NONE)
                {
                    /* now that we've created a field, GetLayerDefn() won't return NULL */
                    if (poDstFDefn == NULL)
                        poDstFDefn = poDstLayer->GetLayerDefn();

                    /* Sanity check : if it fails, the driver is buggy */
                    if (poDstFDefn != NULL &&
                        poDstFDefn->GetFieldCount() != nDstFieldCount + 1)
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "The output driver has claimed to have added the %s field, but it did not!",
                                 oFieldDefn.GetNameRef() );
                    }
                    else
                    {
                        panMap[iSrcField] = nDstFieldCount;
                        nDstFieldCount ++;
                    }
                }
            }
            else
            {
                fprintf( stderr, "Field '%s' not found in source layer.\n",
                        papszSelFields[iField] );
                if( !bSkipFailures )
                {
                    VSIFree(panMap);
                    return NULL;
                }
            }
        }

        /* -------------------------------------------------------------------- */
        /* Use SetIgnoredFields() on source layer if available                  */
        /* -------------------------------------------------------------------- */
        if (poSrcLayer->TestCapability(OLCIgnoreFields))
        {
            int iSrcField;
            char** papszIgnoredFields = NULL;
            int bUseIgnoredFields = TRUE;
            char** papszWHEREUsedFields = NULL;

            if (pszWHERE)
            {
                /* We must not ignore fields used in the -where expression (#4015) */
                OGRFeatureQuery oFeatureQuery;
                if ( oFeatureQuery.Compile( poSrcLayer->GetLayerDefn(), pszWHERE ) == OGRERR_NONE )
                {
                    papszWHEREUsedFields = oFeatureQuery.GetUsedFields();
                }
                else
                {
                    bUseIgnoredFields = FALSE;
                }
            }

            for(iSrcField=0;iSrcField<poSrcFDefn->GetFieldCount();iSrcField++)
            {
                const char* pszFieldName =
                    poSrcFDefn->GetFieldDefn(iSrcField)->GetNameRef();
                int bFieldRequested = FALSE;
                for( iField=0; papszSelFields[iField] != NULL; iField++)
                {
                    if (EQUAL(pszFieldName, papszSelFields[iField]))
                    {
                        bFieldRequested = TRUE;
                        break;
                    }
                }
                bFieldRequested |= CSLFindString(papszWHEREUsedFields, pszFieldName) >= 0;
                bFieldRequested |= (pszZField != NULL && EQUAL(pszFieldName, pszZField));

                /* If source field not requested, add it to ignored files list */
                if (!bFieldRequested)
                    papszIgnoredFields = CSLAddString(papszIgnoredFields, pszFieldName);
            }
            if (bUseIgnoredFields)
                poSrcLayer->SetIgnoredFields((const char**)papszIgnoredFields);
            CSLDestroy(papszIgnoredFields);
            CSLDestroy(papszWHEREUsedFields);
        }
    }
    else if( !bAppend )
    {
        int nDstFieldCount = 0;
        if (poDstFDefn)
            nDstFieldCount = poDstFDefn->GetFieldCount();

        /* Save the map of existing fields, before creating new ones */
        /* This helps when converting a source layer that has duplicated field names */
        /* which is a bad idea */
        std::map<CPLString, int> oMapExistingFields;
        for( iField = 0; iField < nDstFieldCount; iField++ )
        {
            const char* pszFieldName = poDstFDefn->GetFieldDefn(iField)->GetNameRef();
            if( oMapExistingFields.find(pszFieldName) == oMapExistingFields.end() )
                oMapExistingFields[pszFieldName] = iField;
            /*else
                CPLError(CE_Warning, CPLE_AppDefined,
                         "The target layer has already a duplicated field name '%s' before "
                         "adding the fields of the source layer", pszFieldName); */
        }

        for( iField = 0; iField < nSrcFieldCount; iField++ )
        {
            OGRFieldDefn* poSrcFieldDefn = poSrcFDefn->GetFieldDefn(iField);
            OGRFieldDefn oFieldDefn( poSrcFieldDefn );

            if (papszFieldTypesToString != NULL &&
                (CSLFindString(papszFieldTypesToString, "All") != -1 ||
                 CSLFindString(papszFieldTypesToString,
                               OGRFieldDefn::GetFieldTypeName(poSrcFieldDefn->GetType())) != -1))
            {
                oFieldDefn.SetType(OFTString);
            }

            /* The field may have been already created at layer creation */
            std::map<CPLString, int>::iterator oIter =
                oMapExistingFields.find(oFieldDefn.GetNameRef());
            if( oIter != oMapExistingFields.end() )
            {
                panMap[iField] = oIter->second;
                continue;
            }

            int bHasRenamed = FALSE;
            /* In case the field name already exists in the target layer, */
            /* build a unique field name */
            if( poDstFDefn != NULL &&
                poDstFDefn->GetFieldIndex(oFieldDefn.GetNameRef()) >= 0 )
            {
                int nTry = 1;
                while(TRUE)
                {
                    ++nTry;
                    CPLString osTmpName;
                    osTmpName.Printf("%s%d", oFieldDefn.GetNameRef(), nTry);
                    /* Check that the proposed name doesn't exist either in the already */
                    /* created fields or in the source fields */
                    if( poDstFDefn->GetFieldIndex(osTmpName) < 0 &&
                        poSrcFDefn->GetFieldIndex(osTmpName) < 0 )
                    {
                        bHasRenamed = TRUE;
                        oFieldDefn.SetName(osTmpName);
                        break;
                    }
                }
            }

            if (poDstLayer->CreateField( &oFieldDefn ) == OGRERR_NONE)
            {
                /* now that we've created a field, GetLayerDefn() won't return NULL */
                if (poDstFDefn == NULL)
                    poDstFDefn = poDstLayer->GetLayerDefn();

                /* Sanity check : if it fails, the driver is buggy */
                if (poDstFDefn != NULL &&
                    poDstFDefn->GetFieldCount() != nDstFieldCount + 1)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "The output driver has claimed to have added the %s field, but it did not!",
                             oFieldDefn.GetNameRef() );
                }
                else
                {
                    if( bHasRenamed )
                    {
                        const char* pszNewFieldName =
                            poDstFDefn->GetFieldDefn(nDstFieldCount)->GetNameRef();
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Field '%s' already exists. Renaming it as '%s'",
                                 poSrcFieldDefn->GetNameRef(), pszNewFieldName);
                    }

                    panMap[iField] = nDstFieldCount;
                    nDstFieldCount ++;
                }
            }
        }
    }
    else
    {
        /* For an existing layer, build the map by fetching the index in the destination */
        /* layer for each source field */
        if (poDstFDefn == NULL)
        {
            fprintf( stderr, "poDstFDefn == NULL.\n" );
            VSIFree(panMap);
            return NULL;
        }

        for( iField = 0; iField < nSrcFieldCount; iField++ )
        {
            OGRFieldDefn* poSrcFieldDefn = poSrcFDefn->GetFieldDefn(iField);
            int iDstField = poDstFDefn->GetFieldIndex(poSrcFieldDefn->GetNameRef());
            if (iDstField >= 0)
                panMap[iField] = iDstField;
            else
                CPLDebug("OGR2OGR", "Skipping field '%s' not found in destination layer '%s'.",
                         poSrcFieldDefn->GetNameRef(), poDstLayer->GetName() );
        }
    }

    int iSrcZField = -1;
    if (pszZField != NULL)
    {
        iSrcZField = poSrcFDefn->GetFieldIndex(pszZField);
    }

    TargetLayerInfo* psInfo = (TargetLayerInfo*)
                                            CPLMalloc(sizeof(TargetLayerInfo));
    psInfo->nFeaturesRead = 0;
    psInfo->bPerFeatureCT = FALSE;
    psInfo->poDstLayer = poDstLayer;
    psInfo->poCT = NULL;
    psInfo->papszTransformOptions = NULL;
    psInfo->panMap = panMap;
    psInfo->iSrcZField = iSrcZField;

    return psInfo;
}

/************************************************************************/
/*                         FreeTargetLayerInfo()                        */
/************************************************************************/

static void FreeTargetLayerInfo(TargetLayerInfo* psInfo)
{
    if( psInfo == NULL )
        return;
    delete psInfo->poCT;
    CSLDestroy(psInfo->papszTransformOptions);
    CPLFree(psInfo->panMap);
    CPLFree(psInfo);
}

/************************************************************************/
/*                           TranslateLayer()                           */
/************************************************************************/

static int TranslateLayer( TargetLayerInfo* psInfo,
                           OGRDataSource *poSrcDS,
                           OGRLayer * poSrcLayer,
                           OGRDataSource *poDstDS,
                           int bTransform,
                           int bWrapDateline,
                           const char* pszDateLineOffset,
                           OGRSpatialReference *poOutputSRS,
                           int bNullifyOutputSRS,
                           OGRSpatialReference *poUserSourceSRS,
                           OGRCoordinateTransformation *poGCPCoordTrans,
                           int eGType,
                           int bPromoteToMulti,
                           int nCoordDim,
                           GeomOperation eGeomOp,
                           double dfGeomOpParam,
                           long nCountLayerFeatures,
                           OGRGeometry* poClipSrc,
                           OGRGeometry *poClipDst,
                           int bExplodeCollections,
                           vsi_l_offset nSrcFileSize,
                           GIntBig* pnReadFeatureCount,
                           GDALProgressFunc pfnProgress,
                           void *pProgressArg )

{
    OGRLayer    *poDstLayer;
    int         bForceToPolygon = FALSE;
    int         bForceToMultiPolygon = FALSE;
    int         bForceToMultiLineString = FALSE;
    char**      papszTransformOptions = NULL;
    OGRCoordinateTransformation *poCT = NULL;
    int         *panMap = NULL;
    int         iSrcZField;

    poDstLayer = psInfo->poDstLayer;
    panMap = psInfo->panMap;
    iSrcZField = psInfo->iSrcZField;

    if( poOutputSRS == NULL && !bNullifyOutputSRS )
        poOutputSRS = poSrcLayer->GetSpatialRef();
    
    if( wkbFlatten(eGType) == wkbPolygon )
        bForceToPolygon = TRUE;
    else if( wkbFlatten(eGType) == wkbMultiPolygon )
        bForceToMultiPolygon = TRUE;
    else if( wkbFlatten(eGType) == wkbMultiLineString )
        bForceToMultiLineString = TRUE;

/* -------------------------------------------------------------------- */
/*      Transfer features.                                              */
/* -------------------------------------------------------------------- */
    OGRFeature  *poFeature;
    int         nFeaturesInTransaction = 0;
    GIntBig      nCount = 0; /* written + failed */
    GIntBig      nFeaturesWritten = 0;

    if( nGroupTransactions )
        poDstLayer->StartTransaction();

    while( TRUE )
    {
        OGRFeature      *poDstFeature = NULL;

        if( nFIDToFetch != OGRNullFID )
        {
            // Only fetch feature on first pass.
            if( nFeaturesInTransaction == 0 )
                poFeature = poSrcLayer->GetFeature(nFIDToFetch);
            else
                poFeature = NULL;
        }
        else
            poFeature = poSrcLayer->GetNextFeature();

        if( poFeature == NULL )
            break;

        if( psInfo->nFeaturesRead == 0 || psInfo->bPerFeatureCT )
        {
        /* -------------------------------------------------------------------- */
        /*      Setup coordinate transformation if we need it.                  */
        /* -------------------------------------------------------------------- */
            OGRSpatialReference* poSourceSRS = NULL;

            poCT = NULL;
            papszTransformOptions = NULL;

            if( bTransform )
            {
                if( psInfo->nFeaturesRead == 0 )
                {
                    poSourceSRS = poUserSourceSRS;
                    if( poSourceSRS == NULL )
                        poSourceSRS = poSrcLayer->GetSpatialRef();
                }
                if( poSourceSRS == NULL )
                {
                    OGRGeometry* poSrcGeometry = poFeature->GetGeometryRef();
                    if( poSrcGeometry )
                        poSourceSRS = poSrcGeometry->getSpatialReference();
                    psInfo->bPerFeatureCT = TRUE;
                }

                if( poSourceSRS == NULL )
                {
                    fprintf( stderr, "Can't transform coordinates, source layer has no\n"
                            "coordinate system.  Use -s_srs to set one.\n" );
                    OGRFeature::DestroyFeature( poFeature );
                    return FALSE;
                }

                CPLAssert( NULL != poSourceSRS );
                CPLAssert( NULL != poOutputSRS );

                if( psInfo->poCT != NULL &&
                    psInfo->poCT->GetSourceCS() == poSourceSRS )
                {
                    poCT = psInfo->poCT;
                }
                else
                {
                    poCT = OGRCreateCoordinateTransformation( poSourceSRS, poOutputSRS );
                    if( poCT == NULL )
                    {
                        char        *pszWKT = NULL;

                        fprintf( stderr, "Failed to create coordinate transformation between the\n"
                            "following coordinate systems.  This may be because they\n"
                            "are not transformable, or because projection services\n"
                            "(PROJ.4 DLL/.so) could not be loaded.\n" );

                        poSourceSRS->exportToPrettyWkt( &pszWKT, FALSE );
                        fprintf( stderr,  "Source:\n%s\n", pszWKT );
                        CPLFree(pszWKT);

                        poOutputSRS->exportToPrettyWkt( &pszWKT, FALSE );
                        fprintf( stderr,  "Target:\n%s\n", pszWKT );
                        CPLFree(pszWKT);

                        OGRFeature::DestroyFeature( poFeature );
                        return FALSE;
                    }
                    if( poGCPCoordTrans != NULL )
                        poCT = new CompositeCT( poGCPCoordTrans, poCT );
                }

                if( poCT != psInfo->poCT )
                {
                    delete psInfo->poCT;
                    psInfo->poCT = poCT;
                }
            }
            else
            {
                poCT = poGCPCoordTrans;
            }

            if (bWrapDateline)
            {
                if( poSourceSRS == NULL )
                {
                    if( psInfo->nFeaturesRead == 0 )
                    {
                        poSourceSRS = poUserSourceSRS;
                        if( poSourceSRS == NULL )
                            poSourceSRS = poSrcLayer->GetSpatialRef();
                    }
                    if( poSourceSRS == NULL )
                    {
                        OGRGeometry* poSrcGeometry = poFeature->GetGeometryRef();
                        if( poSrcGeometry )
                            poSourceSRS = poSrcGeometry->getSpatialReference();
                        psInfo->bPerFeatureCT = TRUE;
                    }
                }

                if (bTransform && poCT != NULL && poOutputSRS != NULL && poOutputSRS->IsGeographic())
                {
                    papszTransformOptions =
                        CSLAddString(papszTransformOptions, "WRAPDATELINE=YES");
                    CPLString soOffset("DATELINEOFFSET=");
                    soOffset += pszDateLineOffset;
                    papszTransformOptions =
                        CSLAddString(papszTransformOptions, soOffset);
                }
                else if (poSourceSRS != NULL && poSourceSRS->IsGeographic())
                {
                    papszTransformOptions =
                        CSLAddString(papszTransformOptions, "WRAPDATELINE=YES");
                    CPLString soOffset("DATELINEOFFSET=");
                    soOffset += pszDateLineOffset;
                    papszTransformOptions =
                        CSLAddString(papszTransformOptions, soOffset);
                }
                else
                {
                    static int bHasWarned = FALSE;
                    if( !bHasWarned )
                        fprintf(stderr, "-wrapdateline option only works when reprojecting to a geographic SRS\n");
                    bHasWarned = TRUE;
                }

                CSLDestroy(psInfo->papszTransformOptions);
                psInfo->papszTransformOptions = papszTransformOptions;
            }

        }

        psInfo->nFeaturesRead ++;

        int nParts = 0;
        int nIters = 1;
        if (bExplodeCollections)
        {
            OGRGeometry* poSrcGeometry = poFeature->GetGeometryRef();
            if (poSrcGeometry)
            {
                switch (wkbFlatten(poSrcGeometry->getGeometryType()))
                {
                    case wkbMultiPoint:
                    case wkbMultiLineString:
                    case wkbMultiPolygon:
                    case wkbGeometryCollection:
                        nParts = ((OGRGeometryCollection*)poSrcGeometry)->getNumGeometries();
                        nIters = nParts;
                        if (nIters == 0)
                            nIters = 1;
                    default:
                        break;
                }
            }
        }

        for(int iPart = 0; iPart < nIters; iPart++)
        {
            if( ++nFeaturesInTransaction == nGroupTransactions )
            {
                poDstLayer->CommitTransaction();
                poDstLayer->StartTransaction();
                nFeaturesInTransaction = 0;
            }

            CPLErrorReset();
            poDstFeature = OGRFeature::CreateFeature( poDstLayer->GetLayerDefn() );

            /* Optimization to avoid duplicating the source geometry in the */
            /* target feature : we steal it from the source feature for now... */
            OGRGeometry* poStealedGeometry = NULL;
            if( !bExplodeCollections )
                poStealedGeometry = poFeature->StealGeometry();

            if( poDstFeature->SetFrom( poFeature, panMap, TRUE ) != OGRERR_NONE )
            {
                if( nGroupTransactions )
                    poDstLayer->CommitTransaction();

                CPLError( CE_Failure, CPLE_AppDefined,
                        "Unable to translate feature %ld from layer %s.\n",
                        poFeature->GetFID(), poSrcLayer->GetName() );

                OGRFeature::DestroyFeature( poFeature );
                OGRFeature::DestroyFeature( poDstFeature );
                OGRGeometryFactory::destroyGeometry( poStealedGeometry );
                return FALSE;
            }

            /* ... and now we can attach the stealed geometry */
            if( poStealedGeometry )
                poDstFeature->SetGeometryDirectly(poStealedGeometry);

            if( bPreserveFID )
                poDstFeature->SetFID( poFeature->GetFID() );

            OGRGeometry* poDstGeometry = poDstFeature->GetGeometryRef();
            if (poDstGeometry != NULL)
            {
                if (nParts > 0)
                {
                    /* For -explodecollections, extract the iPart(th) of the geometry */
                    OGRGeometry* poPart = ((OGRGeometryCollection*)poDstGeometry)->getGeometryRef(iPart);
                    ((OGRGeometryCollection*)poDstGeometry)->removeGeometry(iPart, FALSE);
                    poDstFeature->SetGeometryDirectly(poPart);
                    poDstGeometry = poPart;
                }

                if (iSrcZField != -1)
                {
                    SetZ(poDstGeometry, poFeature->GetFieldAsDouble(iSrcZField));
                    /* This will correct the coordinate dimension to 3 */
                    OGRGeometry* poDupGeometry = poDstGeometry->clone();
                    poDstFeature->SetGeometryDirectly(poDupGeometry);
                    poDstGeometry = poDupGeometry;
                }

                if (nCoordDim == 2 || nCoordDim == 3)
                    poDstGeometry->setCoordinateDimension( nCoordDim );

                if (eGeomOp == SEGMENTIZE)
                {
                    if (dfGeomOpParam > 0)
                        poDstGeometry->segmentize(dfGeomOpParam);
                }
                else if (eGeomOp == SIMPLIFY_PRESERVE_TOPOLOGY)
                {
                    if (dfGeomOpParam > 0)
                    {
                        OGRGeometry* poNewGeom = poDstGeometry->SimplifyPreserveTopology(dfGeomOpParam);
                        if (poNewGeom)
                        {
                            poDstFeature->SetGeometryDirectly(poNewGeom);
                            poDstGeometry = poNewGeom;
                        }
                    }
                }

                if (poClipSrc)
                {
                    OGRGeometry* poClipped = poDstGeometry->Intersection(poClipSrc);
                    if (poClipped == NULL || poClipped->IsEmpty())
                    {
                        OGRGeometryFactory::destroyGeometry(poClipped);
                        goto end_loop;
                    }
                    poDstFeature->SetGeometryDirectly(poClipped);
                    poDstGeometry = poClipped;
                }

                if( poCT != NULL || papszTransformOptions != NULL)
                {
                    OGRGeometry* poReprojectedGeom =
                        OGRGeometryFactory::transformWithOptions(poDstGeometry, poCT, papszTransformOptions);
                    if( poReprojectedGeom == NULL )
                    {
                        if( nGroupTransactions )
                            poDstLayer->CommitTransaction();

                        fprintf( stderr, "Failed to reproject feature %d (geometry probably out of source or destination SRS).\n",
                                (int) poFeature->GetFID() );
                        if( !bSkipFailures )
                        {
                            OGRFeature::DestroyFeature( poFeature );
                            OGRFeature::DestroyFeature( poDstFeature );
                            return FALSE;
                        }
                    }

                    poDstFeature->SetGeometryDirectly(poReprojectedGeom);
                    poDstGeometry = poReprojectedGeom;
                }
                else if (poOutputSRS != NULL)
                {
                    poDstGeometry->assignSpatialReference(poOutputSRS);
                }

                if (poClipDst)
                {
                    OGRGeometry* poClipped = poDstGeometry->Intersection(poClipDst);
                    if (poClipped == NULL || poClipped->IsEmpty())
                    {
                        OGRGeometryFactory::destroyGeometry(poClipped);
                        goto end_loop;
                    }

                    poDstFeature->SetGeometryDirectly(poClipped);
                    poDstGeometry = poClipped;
                }

                if( bForceToPolygon )
                {
                    poDstFeature->SetGeometryDirectly(
                        OGRGeometryFactory::forceToPolygon(
                            poDstFeature->StealGeometry() ) );
                }
                else if( bForceToMultiPolygon ||
                        (bPromoteToMulti && wkbFlatten(poDstGeometry->getGeometryType()) == wkbPolygon) )
                {
                    poDstFeature->SetGeometryDirectly(
                        OGRGeometryFactory::forceToMultiPolygon(
                            poDstFeature->StealGeometry() ) );
                }
                else if ( bForceToMultiLineString ||
                        (bPromoteToMulti && wkbFlatten(poDstGeometry->getGeometryType()) == wkbLineString) )
                {
                    poDstFeature->SetGeometryDirectly(
                        OGRGeometryFactory::forceToMultiLineString(
                            poDstFeature->StealGeometry() ) );
                }
            }

            CPLErrorReset();
            if( poDstLayer->CreateFeature( poDstFeature ) == OGRERR_NONE )
            {
                nFeaturesWritten ++;
            }
            else if( !bSkipFailures )
            {
                if( nGroupTransactions )
                    poDstLayer->RollbackTransaction();

                OGRFeature::DestroyFeature( poFeature );
                OGRFeature::DestroyFeature( poDstFeature );
                return FALSE;
            }

end_loop:
            OGRFeature::DestroyFeature( poDstFeature );
        }

        OGRFeature::DestroyFeature( poFeature );

        /* Report progress */
        nCount ++;
        if (pfnProgress)
        {
            if (nSrcFileSize != 0)
            {
                if ((nCount % 1000) == 0)
                {
                    OGRLayer* poFCLayer = poSrcDS->ExecuteSQL("GetBytesRead()", NULL, NULL);
                    if( poFCLayer != NULL )
                    {
                        OGRFeature* poFeat = poFCLayer->GetNextFeature();
                        if( poFeat )
                        {
                            const char* pszReadSize = poFeat->GetFieldAsString(0);
                            GUIntBig nReadSize = CPLScanUIntBig( pszReadSize, 32 );
                            pfnProgress(nReadSize * 1.0 / nSrcFileSize, "", pProgressArg);
                            OGRFeature::DestroyFeature( poFeat );
                        }
                    }
                    poSrcDS->ReleaseResultSet(poFCLayer);
                }
            }
            else
            {
                pfnProgress(nCount * 1.0 / nCountLayerFeatures, "", pProgressArg);
            }
        }

        if (pnReadFeatureCount)
            *pnReadFeatureCount = nCount;
    }

    if( nGroupTransactions )
        poDstLayer->CommitTransaction();

    CPLDebug("OGR2OGR", CPL_FRMT_GIB " features written in layer '%s'",
             nFeaturesWritten, poDstLayer->GetName());

    return TRUE;
}

