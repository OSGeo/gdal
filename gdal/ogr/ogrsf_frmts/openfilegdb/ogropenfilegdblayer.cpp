/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Open FileGDB OGR driver.
 * Author:   Even Rouault, <even dot rouault at mines-dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_openfilegdb.h"
#include "cpl_minixml.h"
#include <algorithm>

CPL_CVSID("$Id");

/************************************************************************/
/*                      OGROpenFileGDBGeomFieldDefn                     */
/************************************************************************/
class OGROpenFileGDBGeomFieldDefn: public OGRGeomFieldDefn
{
        OGROpenFileGDBLayer* m_poLayer;

    public:
        OGROpenFileGDBGeomFieldDefn(OGROpenFileGDBLayer* poLayer,
                                    const char *pszNameIn,
                                    OGRwkbGeometryType eGeomTypeIn) :
                OGRGeomFieldDefn(pszNameIn, eGeomTypeIn), m_poLayer(poLayer)
        {
        };

        ~OGROpenFileGDBGeomFieldDefn() {}

        void UnsetLayer() { m_poLayer = NULL; }

        virtual OGRSpatialReference* GetSpatialRef()
        {
            if( poSRS )
                return poSRS;
            if( m_poLayer != NULL )
                (void) m_poLayer->BuildLayerDefinition();
            return poSRS;
        }
};

/************************************************************************/
/*                      OGROpenFileGDBFeatureDefn                       */
/************************************************************************/
class OGROpenFileGDBFeatureDefn: public OGRFeatureDefn
{
        OGROpenFileGDBLayer* m_poLayer;
        int m_bHasBuildFieldDefn;

    public:
        OGROpenFileGDBFeatureDefn( OGROpenFileGDBLayer* poLayer,
                                   const char * pszName ) :
                        OGRFeatureDefn(pszName), m_poLayer(poLayer)
        {
            m_bHasBuildFieldDefn = FALSE;
        }

        ~OGROpenFileGDBFeatureDefn() {}

        void UnsetLayer()
        {
            if( nGeomFieldCount )
                ((OGROpenFileGDBGeomFieldDefn*)papoGeomFieldDefn[0])->UnsetLayer();
            m_poLayer = NULL;
        }

        virtual int GetFieldCount()
        {
            if( nFieldCount )
                return nFieldCount;
            if( !m_bHasBuildFieldDefn && m_poLayer != NULL )
            {
                m_bHasBuildFieldDefn = TRUE;
                (void) m_poLayer->BuildLayerDefinition();
            }
            return nFieldCount;
        }
};

/************************************************************************/
/*                      OGROpenFileGDBLayer()                           */
/************************************************************************/

OGROpenFileGDBLayer::OGROpenFileGDBLayer(const char* pszGDBFilename,
                                         const char* pszName,
                                         const std::string& osDefinition,
                                         const std::string& osDocumentation,
                                         const char* pszGeomName,
                                         OGRwkbGeometryType eGeomType) :
            m_osGDBFilename(pszGDBFilename),
            m_osName(pszName),
            m_poLyrTable(NULL),
            m_poFeatureDefn(NULL),
            m_iGeomFieldIdx(-1),
            m_iCurFeat(0),
            m_osDefinition(osDefinition),
            m_osDocumentation(osDocumentation),
            m_eGeomType(eGeomType),
            m_bValidLayerDefn(-1),
            m_bEOF(FALSE),
            m_poGeomConverter(NULL),
            m_iFieldToReadAsBinary(-1),
            m_poIterator(NULL),
            m_bIteratorSufficientToEvaluateFilter(FALSE),
            m_poIterMinMax(NULL),
            m_eSpatialIndexState(SPI_IN_BUILDING),
            m_pQuadTree(NULL),
            m_pahFilteredFeatures(NULL),
            m_nFilteredFeatureCount(-1)
{
    m_poFeatureDefn = new OGROpenFileGDBFeatureDefn(this, pszName);
    SetDescription( m_poFeatureDefn->GetName() );
    m_poFeatureDefn->SetGeomType(wkbNone);
    m_poFeatureDefn->Reference();

    if( m_osDefinition.size() && BuildGeometryColumnGDBv10() )
    {
        ;
    }
    else if( eGeomType != wkbNone )
    {
        m_poFeatureDefn->AddGeomFieldDefn(
            new OGROpenFileGDBGeomFieldDefn(this, pszGeomName, eGeomType), FALSE);
    }
}

/***********************************************************************/
/*                      ~OGROpenFileGDBLayer()                         */
/***********************************************************************/

OGROpenFileGDBLayer::~OGROpenFileGDBLayer()
{
    delete m_poLyrTable;
    if( m_poFeatureDefn )
    {
        m_poFeatureDefn->UnsetLayer();
        m_poFeatureDefn->Release();
    }
    delete m_poIterator;
    delete m_poIterMinMax;
    delete m_poGeomConverter;
    if( m_pQuadTree != NULL )
        CPLQuadTreeDestroy(m_pQuadTree);
    CPLFree(m_pahFilteredFeatures);
}

/************************************************************************/
/*                     BuildGeometryColumnGDBv10()                      */
/************************************************************************/

int OGROpenFileGDBLayer::BuildGeometryColumnGDBv10()
{
    CPLXMLNode* psTree = CPLParseXMLString(m_osDefinition.c_str());
    if( psTree == NULL )
    {
        return FALSE;
    }

    CPLStripXMLNamespace( psTree, NULL, TRUE );
    /* CPLSerializeXMLTreeToFile( psTree, "/dev/stderr" ); */
    CPLXMLNode* psInfo = CPLSearchXMLNode( psTree, "=DEFeatureClassInfo" );
    if( psInfo == NULL )
        psInfo = CPLSearchXMLNode( psTree, "=DETableInfo" );
    if( psInfo == NULL )
    {
        CPLDestroyXMLNode(psTree);
        return FALSE;
    }

    /* We cannot trust the XML definition to build the field definitions. */
    /* It sometimes misses a few fields ! */

    int bHasZ = CSLTestBoolean(CPLGetXMLValue( psInfo, "HasZ", "NO" ));
    const char* pszShapeType = CPLGetXMLValue(psInfo, "ShapeType", NULL);
    const char* pszShapeFieldName = CPLGetXMLValue(psInfo, "ShapeFieldName", NULL);
    if( pszShapeType != NULL && pszShapeFieldName != NULL )
    {
        m_eGeomType =
            FileGDBOGRGeometryConverter::GetGeometryTypeFromESRI(pszShapeType);
        if( bHasZ )
            m_eGeomType = wkbSetZ( m_eGeomType );

        const char* pszWKT = CPLGetXMLValue( psInfo, "SpatialReference.WKT", NULL );
        int nWKID = atoi(CPLGetXMLValue( psInfo, "SpatialReference.WKID", "0" ));
        /* The concept of LatestWKID is explained in http://resources.arcgis.com/en/help/arcgis-rest-api/index.html#//02r3000000n1000000 */
        int nLatestWKID = atoi(CPLGetXMLValue( psInfo, "SpatialReference.LatestWKID", "0" ));

        OGROpenFileGDBGeomFieldDefn* poGeomFieldDefn =
            new OGROpenFileGDBGeomFieldDefn(NULL, pszShapeFieldName, m_eGeomType);

        OGRSpatialReference* poSRS = NULL;
        if( nWKID > 0 || nLatestWKID > 0 )
        {
            int bSuccess = FALSE;
            poSRS = new OGRSpatialReference();
            CPLPushErrorHandler(CPLQuietErrorHandler);
            /* Try first with nLatestWKID as there's a higher chance it is a EPSG code and not an ESRI one */
            if( nLatestWKID > 0 )
            {
                if( poSRS->importFromEPSG(nLatestWKID) == OGRERR_NONE )
                {
                    bSuccess = TRUE;
                }
                else
                {
                    CPLDebug("OpenFileGDB", "Cannot import SRID %d", nLatestWKID);
                }
            }
            if( !bSuccess && nWKID > 0 )
            {
                if( poSRS->importFromEPSG(nWKID) == OGRERR_NONE )
                {
                    bSuccess = TRUE;
                }
                else
                {
                    CPLDebug("OpenFileGDB", "Cannot import SRID %d", nWKID);
                }
            }
            if( !bSuccess )
            {
                delete poSRS;
                poSRS = NULL;
            }
            CPLPopErrorHandler();
            CPLErrorReset();
        }
        if( poSRS == NULL && pszWKT != NULL && pszWKT[0] != '{' )
        {
            poSRS = new OGRSpatialReference( pszWKT );
            if( poSRS->morphFromESRI() != OGRERR_NONE )
            {
                delete poSRS;
                poSRS = NULL;
            }
        }
        if( poSRS != NULL )
        {
            poGeomFieldDefn->SetSpatialRef(poSRS);
            poSRS->Dereference();
        }
        m_poFeatureDefn->AddGeomFieldDefn(poGeomFieldDefn, FALSE);
    }
    else
    {
        m_eGeomType = wkbNone;
    }
    CPLDestroyXMLNode(psTree);
    return TRUE;
}

/************************************************************************/
/*                      BuildLayerDefinition()                          */
/************************************************************************/

int OGROpenFileGDBLayer::BuildLayerDefinition()
{
    if( m_bValidLayerDefn >= 0 )
        return m_bValidLayerDefn;

    m_poLyrTable = new FileGDBTable();
    if( !(m_poLyrTable->Open(m_osGDBFilename)) )
    {
        delete m_poLyrTable;
        m_poLyrTable = NULL;
        m_bValidLayerDefn = FALSE;
        return FALSE;
    }

    m_bValidLayerDefn = TRUE;

    m_iGeomFieldIdx = m_poLyrTable->GetGeomFieldIdx();
    if( m_iGeomFieldIdx >= 0 )
    {
        FileGDBGeomField* poGDBGeomField =
            (FileGDBGeomField* )m_poLyrTable->GetField(m_iGeomFieldIdx);
        m_poGeomConverter = FileGDBOGRGeometryConverter::BuildConverter(poGDBGeomField);

        if( CSLTestBoolean(CPLGetConfigOption("OPENFILEGDB_IN_MEMORY_SPI", "YES")) )
        {
            CPLRectObj sGlobalBounds;
            sGlobalBounds.minx = poGDBGeomField->GetXMin();
            sGlobalBounds.miny = poGDBGeomField->GetYMin();
            sGlobalBounds.maxx = poGDBGeomField->GetXMax();
            sGlobalBounds.maxy = poGDBGeomField->GetYMax();
            m_pQuadTree = CPLQuadTreeCreate(&sGlobalBounds,
                                            NULL);
            CPLQuadTreeSetMaxDepth(m_pQuadTree,
                CPLQuadTreeGetAdvisedMaxDepth(m_poLyrTable->GetValidRecordCount()));
        }
        else
        {
            m_eSpatialIndexState = SPI_INVALID;
        }
    }

    if( m_osDefinition.size() == 0 && m_iGeomFieldIdx >= 0 )
    {
        FileGDBGeomField* poGDBGeomField =
            (FileGDBGeomField* )m_poLyrTable->GetField(m_iGeomFieldIdx);
        const char* pszName = poGDBGeomField->GetName().c_str();
        FileGDBTableGeometryType eGDBGeomType = m_poLyrTable->GetGeometryType();

        OGRwkbGeometryType eGeomType = wkbUnknown;
        switch( eGDBGeomType )
        {
            case FGTGT_NONE: /* doesn't make sense ! */ break;
            case FGTGT_POINT: eGeomType = wkbPoint; break;
            case FGTGT_MULTIPOINT: eGeomType = wkbMultiPoint; break;
            case FGTGT_LINE: eGeomType = wkbMultiLineString; break;
            case FGTGT_POLYGON: eGeomType = wkbMultiPolygon; break;
            case FGTGT_MULTIPATCH: eGeomType = wkbMultiPolygon; break;
        }
        if( m_eGeomType == wkbUnknown )
            m_eGeomType = eGeomType;
        if( eGeomType != m_eGeomType )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Inconsistency for layer geometry type");
        }

        OGROpenFileGDBGeomFieldDefn* poGeomFieldDefn;
        if( m_poFeatureDefn->GetGeomFieldCount() == 0 )
        {
            poGeomFieldDefn =
                    new OGROpenFileGDBGeomFieldDefn(NULL, pszName, m_eGeomType);
            m_poFeatureDefn->AddGeomFieldDefn(poGeomFieldDefn, FALSE);
        }
        else
        {
            poGeomFieldDefn = (OGROpenFileGDBGeomFieldDefn*)
                                        m_poFeatureDefn->GetGeomFieldDefn(0);
            poGeomFieldDefn->SetType(m_eGeomType);
        }

        OGRSpatialReference* poSRS = NULL;
        if( poGDBGeomField->GetWKT().size() && 
            poGDBGeomField->GetWKT()[0] != '{' )
        {
            poSRS = new OGRSpatialReference( poGDBGeomField->GetWKT().c_str() );
            if( poSRS->morphFromESRI() != OGRERR_NONE )
            {
                delete poSRS;
                poSRS = NULL;
            }
        }
        if( poSRS != NULL )
        {
            poGeomFieldDefn->SetSpatialRef(poSRS);
            poSRS->Dereference();
        }
    }

    for(int i=0;i<m_poLyrTable->GetFieldCount();i++)
    {
        if( i == m_iGeomFieldIdx )
            continue;

        const FileGDBField* poGDBField = m_poLyrTable->GetField(i);
        OGRFieldType eType = OFTString;
        OGRFieldSubType eSubType = OFSTNone;
        /* int nWidth = 0; */
        switch( poGDBField->GetType() )
        {
            case FGFT_INT16:
                eType = OFTInteger;
                eSubType = OFSTInt16;
                break;
            case FGFT_INT32:
                eType = OFTInteger;
                break;
            case FGFT_FLOAT32:
                eType = OFTReal;
                eSubType = OFSTFloat32;
                break;
            case FGFT_FLOAT64:
                eType = OFTReal;
                break;
            case FGFT_STRING:
                /* nWidth = poGDBField->GetMaxWidth(); */
                eType = OFTString;
                break;
            case FGFT_UUID_1:
            case FGFT_UUID_2:
            case FGFT_XML:
                eType = OFTString;
                break;
            case FGFT_DATETIME:
                eType = OFTDateTime;
                break;
            case FGFT_UNDEFINED:
            case FGFT_OBJECTID:
            case FGFT_GEOMETRY:
                CPLAssert(FALSE);
                break;
            case FGFT_BINARY:
            case FGFT_RASTER:
            {
                /* Special case for v9 GDB_UserMetadata table */
                if( m_iFieldToReadAsBinary < 0 &&
                    poGDBField->GetName() == "Xml" &&
                    poGDBField->GetType() == FGFT_BINARY )
                {
                    m_iFieldToReadAsBinary = i;
                    eType = OFTString;
                }
                else
                    eType = OFTBinary;
                break;
            }
        }
        OGRFieldDefn oFieldDefn(poGDBField->GetName().c_str(), eType);
        oFieldDefn.SetSubType(eSubType);
        /* oFieldDefn.SetWidth(nWidth); */
        m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }

    return TRUE;
}

/************************************************************************/
/*                           GetGeomType()                              */
/************************************************************************/

OGRwkbGeometryType OGROpenFileGDBLayer::GetGeomType()
{
    if( m_eGeomType == wkbUnknown )
    {
        (void) BuildLayerDefinition();
    }

    return m_eGeomType;
}

/***********************************************************************/
/*                          GetLayerDefn()                             */
/***********************************************************************/

OGRFeatureDefn* OGROpenFileGDBLayer::GetLayerDefn()
{
    return m_poFeatureDefn;
}

/***********************************************************************/
/*                          GetFIDColumn()                             */
/***********************************************************************/

const char* OGROpenFileGDBLayer::GetFIDColumn()
{
    if( m_osFIDName.size() )
        return m_osFIDName.c_str();

    if( !BuildLayerDefinition() )
        return "";
    return m_poLyrTable->GetObjectIdColName().c_str();
}

/***********************************************************************/
/*                          ResetReading()                             */
/***********************************************************************/

void OGROpenFileGDBLayer::ResetReading()
{
    if( m_iCurFeat != 0 )
    {
        if( m_eSpatialIndexState == SPI_IN_BUILDING )
            m_eSpatialIndexState = SPI_INVALID;
    }
    m_bEOF = FALSE;
    m_iCurFeat = 0;
    if( m_poIterator )
        m_poIterator->Reset();
}

/***********************************************************************/
/*                         SetSpatialFilter()                          */
/***********************************************************************/

void OGROpenFileGDBLayer::SetSpatialFilter( OGRGeometry *poGeom )
{
    if( !BuildLayerDefinition() )
        return;

    OGRLayer::SetSpatialFilter(poGeom);

    if( m_bFilterIsEnvelope )
    {
        OGREnvelope sLayerEnvelope;
        if( GetExtent(&sLayerEnvelope, FALSE) == OGRERR_NONE )
        {
            if( m_sFilterEnvelope.MinX <= sLayerEnvelope.MinX &&
                m_sFilterEnvelope.MinY <= sLayerEnvelope.MinY &&
                m_sFilterEnvelope.MaxX >= sLayerEnvelope.MaxX &&
                m_sFilterEnvelope.MaxY >= sLayerEnvelope.MaxY )
            {
                CPLDebug("OpenFileGDB", "Disabling spatial filter since it "
                         "contains the layer spatial extent");
                poGeom = NULL;
                OGRLayer::SetSpatialFilter(poGeom);
            }
        }
    }

    if( poGeom != NULL )
    {
        if( m_eSpatialIndexState == SPI_COMPLETED )
        {
            CPLRectObj aoi;
            aoi.minx = m_sFilterEnvelope.MinX;
            aoi.miny = m_sFilterEnvelope.MinY;
            aoi.maxx = m_sFilterEnvelope.MaxX;
            aoi.maxy = m_sFilterEnvelope.MaxY;
            CPLFree(m_pahFilteredFeatures);
            m_nFilteredFeatureCount = -1;
            m_pahFilteredFeatures = CPLQuadTreeSearch(m_pQuadTree,
                                                      &aoi,
                                                      &m_nFilteredFeatureCount);
            if( m_nFilteredFeatureCount >= 0 )
            {
                size_t* panStart = (size_t*)m_pahFilteredFeatures;
                std::sort(panStart, panStart + m_nFilteredFeatureCount);
            }
        }
        m_poLyrTable->InstallFilterEnvelope(&m_sFilterEnvelope);
    }
    else
    {
        CPLFree(m_pahFilteredFeatures);
        m_pahFilteredFeatures = NULL;
        m_nFilteredFeatureCount = -1;
        m_poLyrTable->InstallFilterEnvelope(NULL);
    }
}

/***********************************************************************/
/*                            CompValues()                             */
/***********************************************************************/

static int CompValues(OGRFieldDefn* poFieldDefn,
                      const swq_expr_node* poValue1,
                      const swq_expr_node* poValue2)
{
    switch( poFieldDefn->GetType() )
    {
        case OFTInteger:
        {
            int n1, n2;
            if (poValue1->field_type == SWQ_FLOAT)
                n1 = (int) poValue1->float_value;
            else
                n1 = poValue1->int_value;
            if (poValue2->field_type == SWQ_FLOAT)
                n2 = (int) poValue2->float_value;
            else
                n2 = poValue2->int_value;
            if( n1 < n2 )
                return -1;
            if( n1 == n2 )
                return 0;
            else 
                return 1;
            break;
        }

        case OFTReal:
            if( poValue1->float_value < poValue2->float_value )
                return -1;
            if( poValue1->float_value == poValue2->float_value )
                return 0;
            else
                return 1;
            break;

        case OFTString:
            return strcmp(poValue1->string_value, poValue2->string_value);
            break;

        case OFTDate:
        case OFTTime:
        case OFTDateTime:
        {
            if ((poValue1->field_type == SWQ_TIMESTAMP ||
                 poValue1->field_type == SWQ_DATE ||
                 poValue1->field_type == SWQ_TIME) &&
                (poValue2->field_type == SWQ_TIMESTAMP ||
                 poValue2->field_type == SWQ_DATE ||
                 poValue2->field_type == SWQ_TIME))
            {
                return strcmp(poValue1->string_value, poValue2->string_value);
            }
            return 0;
            break;
        }

        default:
            return 0;
            break;
    }
}

/***********************************************************************/
/*                    OGROpenFileGDBIsComparisonOp()                   */
/***********************************************************************/

int OGROpenFileGDBIsComparisonOp(int op)
{
    return (op == SWQ_EQ || op == SWQ_NE || op == SWQ_LT ||
            op == SWQ_LE || op == SWQ_GT || op == SWQ_GE);
}

/***********************************************************************/
/*                        AreExprExclusive()                           */
/***********************************************************************/

static const struct 
{
    swq_op op1;
    swq_op op2;
    int    expected_comp_1;
    int    expected_comp_2;
}
asPairsOfComparisons[] = 
{
    { SWQ_EQ, SWQ_EQ, -1, 1 },
    { SWQ_LT, SWQ_GT, -1, 0 },
    { SWQ_GT, SWQ_LT, 0, 1 },
    { SWQ_LT, SWQ_GE, -1, 999 },
    { SWQ_LE, SWQ_GE, -1, 999 },
    { SWQ_LE, SWQ_GT, -1, 999 },
    { SWQ_GE, SWQ_LE, 1, 999 },
    { SWQ_GE, SWQ_LT, 1, 999 },
    { SWQ_GT, SWQ_LE, 1, 999 }
};

static int AreExprExclusive(OGRFeatureDefn* poFeatureDefn,
                            const swq_expr_node* poNode1,
                            const swq_expr_node* poNode2)
{
    if( poNode1->eNodeType != SNT_OPERATION )
        return FALSE;
    if( poNode2->eNodeType != SNT_OPERATION )
        return FALSE;

    const size_t nParis = sizeof(asPairsOfComparisons) / sizeof(asPairsOfComparisons[0]);
    for(size_t i = 0; i < nParis; i++ )
    {
        if( poNode1->nOperation == asPairsOfComparisons[i].op1 &&
            poNode2->nOperation == asPairsOfComparisons[i].op2 &&
            poNode1->nSubExprCount == 2 &&
            poNode2->nSubExprCount == 2 )
        {
            swq_expr_node *poColumn1 = poNode1->papoSubExpr[0];
            swq_expr_node *poValue1 = poNode1->papoSubExpr[1];
            swq_expr_node *poColumn2 = poNode2->papoSubExpr[0];
            swq_expr_node *poValue2 = poNode2->papoSubExpr[1];
            if( poColumn1->eNodeType == SNT_COLUMN &&
                poValue1->eNodeType == SNT_CONSTANT &&
                poColumn2->eNodeType == SNT_COLUMN &&
                poValue2->eNodeType == SNT_CONSTANT &&
                poColumn1->field_index == poColumn2->field_index &&
                poColumn1->field_index < poFeatureDefn->GetFieldCount() )
            {
                OGRFieldDefn *poFieldDefn;
                poFieldDefn = poFeatureDefn->GetFieldDefn(poColumn1->field_index);

                int nComp = CompValues(poFieldDefn, poValue1, poValue2);
                return nComp == asPairsOfComparisons[i].expected_comp_1 ||
                       nComp == asPairsOfComparisons[i].expected_comp_2;
            }
            return FALSE;
        }
    }

    if( (poNode2->nOperation == SWQ_ISNULL &&
         OGROpenFileGDBIsComparisonOp(poNode1->nOperation) &&
         poNode1->nSubExprCount == 2 &&
         poNode2->nSubExprCount == 1) ||
        (poNode1->nOperation == SWQ_ISNULL &&
         OGROpenFileGDBIsComparisonOp(poNode2->nOperation) &&
         poNode2->nSubExprCount == 2 &&
         poNode1->nSubExprCount == 1))
    {
        swq_expr_node *poColumn1 = poNode1->papoSubExpr[0];
        swq_expr_node *poColumn2 = poNode2->papoSubExpr[0];
        if( poColumn1->eNodeType == SNT_COLUMN &&
            poColumn2->eNodeType == SNT_COLUMN &&
            poColumn1->field_index == poColumn2->field_index &&
            poColumn1->field_index < poFeatureDefn->GetFieldCount() )
        {
            return TRUE;
        }
    }

    /* In doubt: return FALSE */
    return FALSE;
}


/***********************************************************************/
/*                     FillTargetValueFromSrcExpr()                    */
/***********************************************************************/

static
int FillTargetValueFromSrcExpr( OGRFieldDefn* poFieldDefn,
                                OGRField* poTargetValue,
                                const swq_expr_node* poSrcValue )
{
    switch( poFieldDefn->GetType() )
    {
        case OFTInteger:
            if (poSrcValue->field_type == SWQ_FLOAT)
                poTargetValue->Integer = (int) poSrcValue->float_value;
            else
                poTargetValue->Integer = poSrcValue->int_value;
            break;

        case OFTReal:
            poTargetValue->Real = poSrcValue->float_value;
            break;

        case OFTString:
            poTargetValue->String = poSrcValue->string_value;
            break;

        case OFTDate:
        case OFTTime:
        case OFTDateTime:
            if (poSrcValue->field_type == SWQ_TIMESTAMP ||
                poSrcValue->field_type == SWQ_DATE ||
                poSrcValue->field_type == SWQ_TIME)
            {
                int nYear = 0, nMonth = 0, nDay = 0, nHour = 0, nMin = 0, nSec = 0;
                if( sscanf(poSrcValue->string_value, "%04d/%02d/%02d %02d:%02d:%02d",
                           &nYear, &nMonth, &nDay, &nHour, &nMin, &nSec) == 6 ||
                    sscanf(poSrcValue->string_value, "%04d/%02d/%02d",
                           &nYear, &nMonth, &nDay) == 3 ||
                    sscanf(poSrcValue->string_value, "%02d:%02d:%02d",
                           &nHour, &nMin, &nSec) == 3 )
                {
                    poTargetValue->Date.Year = (GInt16)nYear;
                    poTargetValue->Date.Month = (GByte)nMonth;
                    poTargetValue->Date.Day = (GByte)nDay;
                    poTargetValue->Date.Hour = (GByte)nHour;
                    poTargetValue->Date.Minute = (GByte)nMin;
                    poTargetValue->Date.Second = (GByte)nSec;
                    poTargetValue->Date.TZFlag = 0;
                }
                else
                    return FALSE;
            }
            else
                return FALSE;
            break;

        default:
            return FALSE;
    }
    return TRUE;
}

/***********************************************************************/
/*                        GetColumnSubNode()                           */
/***********************************************************************/

static swq_expr_node* GetColumnSubNode(swq_expr_node* poNode)
{
    if( poNode->eNodeType == SNT_OPERATION &&
        poNode->nSubExprCount == 2 )
    {
        if( poNode->papoSubExpr[0]->eNodeType == SNT_COLUMN )
            return poNode->papoSubExpr[0];
        if( poNode->papoSubExpr[1]->eNodeType == SNT_COLUMN )
            return poNode->papoSubExpr[1];
    }
    return NULL;
}

/***********************************************************************/
/*                        GetConstantSubNode()                         */
/***********************************************************************/

static swq_expr_node* GetConstantSubNode(swq_expr_node* poNode)
{
    if( poNode->eNodeType == SNT_OPERATION &&
        poNode->nSubExprCount == 2 )
    {
        if( poNode->papoSubExpr[1]->eNodeType == SNT_CONSTANT )
            return poNode->papoSubExpr[1];
        if( poNode->papoSubExpr[0]->eNodeType == SNT_CONSTANT )
            return poNode->papoSubExpr[0];
    }
    return NULL;
}
/***********************************************************************/
/*                     BuildIteratorFromExprNode()                     */
/***********************************************************************/

FileGDBIterator* OGROpenFileGDBLayer::BuildIteratorFromExprNode(swq_expr_node* poNode)
{
    if( m_bIteratorSufficientToEvaluateFilter == FALSE )
        return NULL;

    if( poNode->eNodeType == SNT_OPERATION &&
        poNode->nOperation == SWQ_AND && poNode->nSubExprCount == 2 )
    {
        /* Even if there's only one branch of the 2 that results to an iterator, */
        /* it is useful. Of course, the iterator will not be sufficient to evaluate */
        /* the filter, but it will be a super-set of the features */
        FileGDBIterator* poIter1 = BuildIteratorFromExprNode(poNode->papoSubExpr[0]);

        /* In case the first branch didn't result to an iterator, temporarily */
        /* restore the flag */
        int bSaveIteratorSufficientToEvaluateFilter = m_bIteratorSufficientToEvaluateFilter;
        m_bIteratorSufficientToEvaluateFilter = -1;
        FileGDBIterator* poIter2 = BuildIteratorFromExprNode(poNode->papoSubExpr[1]);
        m_bIteratorSufficientToEvaluateFilter = bSaveIteratorSufficientToEvaluateFilter;

        if( poIter1 != NULL && poIter2 != NULL )
            return FileGDBIterator::BuildAnd(poIter1, poIter2);
        m_bIteratorSufficientToEvaluateFilter = FALSE;
        if( poIter1 != NULL )
            return poIter1;
        if( poIter2 != NULL )
            return poIter2;
    }

    else if( poNode->eNodeType == SNT_OPERATION &&
        poNode->nOperation == SWQ_OR && poNode->nSubExprCount == 2 )
    {
        /* For a OR, we need an iterator for the 2 branches */
        FileGDBIterator* poIter1 = BuildIteratorFromExprNode(poNode->papoSubExpr[0]);
        if( poIter1 != NULL )
        {
            FileGDBIterator* poIter2 = BuildIteratorFromExprNode(poNode->papoSubExpr[1]);
            if( poIter2 == NULL )
            {
                delete poIter1;
            }
            else
            {
                return FileGDBIterator::BuildOr(poIter1, poIter2,
                                        AreExprExclusive(GetLayerDefn(),
                                                        poNode->papoSubExpr[0],
                                                        poNode->papoSubExpr[1]));
            }
        }
    }

    else if( poNode->eNodeType == SNT_OPERATION &&
             OGROpenFileGDBIsComparisonOp(poNode->nOperation) &&
             poNode->nSubExprCount == 2 )
    {
        swq_expr_node *poColumn = GetColumnSubNode(poNode);
        swq_expr_node *poValue = GetConstantSubNode(poNode);
        if( poColumn != NULL && poValue != NULL &&
            poColumn->field_index < GetLayerDefn()->GetFieldCount())
        {
            OGRFieldDefn *poFieldDefn;
            poFieldDefn = GetLayerDefn()->GetFieldDefn(poColumn->field_index);

            int nTableColIdx = m_poLyrTable->GetFieldIdx(poFieldDefn->GetNameRef());
            if( nTableColIdx >= 0 && m_poLyrTable->GetField(nTableColIdx)->HasIndex() )
            {
                OGRField sValue;

                if( FillTargetValueFromSrcExpr(poFieldDefn, &sValue, poValue) )
                {
                    FileGDBSQLOp eOp;
                    if( poColumn == poNode->papoSubExpr[0] )
                    {
                        switch( poNode->nOperation )
                        {
                            case SWQ_LE: eOp = FGSO_LE; break;
                            case SWQ_LT: eOp = FGSO_LT; break;
                            case SWQ_NE: eOp = FGSO_EQ; /* yes : EQ */ break;
                            case SWQ_EQ: eOp = FGSO_EQ; break;
                            case SWQ_GE: eOp = FGSO_GE; break;
                            case SWQ_GT: eOp = FGSO_GT; break;
                            default: eOp = FGSO_EQ; CPLAssert(FALSE); break;
                        }
                    }
                    else
                    {
                        /* If "constant op column", then we must reverse */
                        /* the operator */
                        switch( poNode->nOperation )
                        {
                            case SWQ_LE: eOp = FGSO_GE; break;
                            case SWQ_LT: eOp = FGSO_GT; break;
                            case SWQ_NE: eOp = FGSO_EQ; /* yes : EQ */ break;
                            case SWQ_EQ: eOp = FGSO_EQ; break;
                            case SWQ_GE: eOp = FGSO_LE; break;
                            case SWQ_GT: eOp = FGSO_LT; break;
                            default: eOp = FGSO_EQ; CPLAssert(FALSE); break;
                        }
                    }

                    FileGDBIterator* poIter = FileGDBIterator::Build(
                        m_poLyrTable, nTableColIdx, TRUE,
                        eOp, poFieldDefn->GetType(), &sValue);
                    if( poIter != NULL )
                        m_bIteratorSufficientToEvaluateFilter = TRUE;
                    if( poIter && poNode->nOperation == SWQ_NE )
                        return FileGDBIterator::BuildNot(poIter);
                    else
                        return poIter;
                }
            }
        }
    }
    else if( poNode->eNodeType == SNT_OPERATION &&
             poNode->nOperation == SWQ_ISNULL && poNode->nSubExprCount == 1 )
    {
        swq_expr_node *poColumn = poNode->papoSubExpr[0];
        if( poColumn->eNodeType == SNT_COLUMN &&
            poColumn->field_index < GetLayerDefn()->GetFieldCount() )
        {
            OGRFieldDefn *poFieldDefn;
            poFieldDefn = GetLayerDefn()->GetFieldDefn(poColumn->field_index);

            int nTableColIdx = m_poLyrTable->GetFieldIdx(poFieldDefn->GetNameRef());
            if( nTableColIdx >= 0 && m_poLyrTable->GetField(nTableColIdx)->HasIndex() )
            {
                FileGDBIterator* poIter = FileGDBIterator::BuildIsNotNull(
                                                    m_poLyrTable, nTableColIdx, TRUE);
                if( poIter )
                {
                    m_bIteratorSufficientToEvaluateFilter = TRUE;
                    poIter = FileGDBIterator::BuildNot(poIter);
                }
                return poIter;
            }
        }
    }
    else if( poNode->eNodeType == SNT_OPERATION &&
                poNode->nOperation == SWQ_NOT && poNode->nSubExprCount == 1 &&
                poNode->papoSubExpr[0]->eNodeType == SNT_OPERATION &&
                poNode->papoSubExpr[0]->nOperation == SWQ_ISNULL &&
                poNode->papoSubExpr[0]->nSubExprCount == 1 )
    {
        swq_expr_node *poColumn = poNode->papoSubExpr[0]->papoSubExpr[0];
        if( poColumn->eNodeType == SNT_COLUMN &&
            poColumn->field_index < GetLayerDefn()->GetFieldCount() )
        {
            OGRFieldDefn *poFieldDefn;
            poFieldDefn = GetLayerDefn()->GetFieldDefn(poColumn->field_index);

            int nTableColIdx = m_poLyrTable->GetFieldIdx(poFieldDefn->GetNameRef());
            if( nTableColIdx >= 0 && m_poLyrTable->GetField(nTableColIdx)->HasIndex() )
            {
                FileGDBIterator* poIter = FileGDBIterator::BuildIsNotNull(
                                                m_poLyrTable, nTableColIdx, TRUE);
                if( poIter )
                    m_bIteratorSufficientToEvaluateFilter = TRUE;
                return poIter;
            }
        }
    }
    else if( poNode->eNodeType == SNT_OPERATION &&
        poNode->nOperation == SWQ_BETWEEN && poNode->nSubExprCount == 3 )
    {
        swq_expr_node *poColumn = poNode->papoSubExpr[0];
        swq_expr_node *poValue1 = poNode->papoSubExpr[1];
        swq_expr_node *poValue2 = poNode->papoSubExpr[2];
        if( poColumn->eNodeType == SNT_COLUMN &&
            poColumn->field_index < GetLayerDefn()->GetFieldCount() &&
            poValue1->eNodeType == SNT_CONSTANT &&
            poValue2->eNodeType == SNT_CONSTANT )
        {
            OGRFieldDefn *poFieldDefn;
            poFieldDefn = GetLayerDefn()->GetFieldDefn(poColumn->field_index);

            int nTableColIdx = m_poLyrTable->GetFieldIdx(poFieldDefn->GetNameRef());
            if( nTableColIdx >= 0 && m_poLyrTable->GetField(nTableColIdx)->HasIndex() )
            {
                OGRField sValue1, sValue2;

                if( FillTargetValueFromSrcExpr(poFieldDefn, &sValue1, poValue1) &&
                    FillTargetValueFromSrcExpr(poFieldDefn, &sValue2, poValue2) )
                {
                    FileGDBIterator* poIter1 = FileGDBIterator::Build(
                                            m_poLyrTable, nTableColIdx, TRUE,
                                            FGSO_GE,
                                            poFieldDefn->GetType(), &sValue1);
                    FileGDBIterator* poIter2 = FileGDBIterator::Build(
                                            m_poLyrTable, nTableColIdx, TRUE,
                                            FGSO_LE,
                                            poFieldDefn->GetType(), &sValue2);
                    if( poIter1 != NULL && poIter2 != NULL )
                    {
                        m_bIteratorSufficientToEvaluateFilter = TRUE;
                        return FileGDBIterator::BuildAnd(poIter1, poIter2);
                    }
                    delete poIter1;
                    delete poIter2;
                }
            }
        }
    }
    else if( poNode->eNodeType == SNT_OPERATION &&
             poNode->nOperation == SWQ_IN && poNode->nSubExprCount >= 2 )
    {
        swq_expr_node *poColumn = poNode->papoSubExpr[0];
        if( poColumn->eNodeType == SNT_COLUMN &&
            poColumn->field_index < GetLayerDefn()->GetFieldCount() )
        {
            int bAllConstants = TRUE;
            int i;
            for(i=1;i<poNode->nSubExprCount;i++)
            {
                if( poNode->papoSubExpr[i]->eNodeType != SNT_CONSTANT )
                    bAllConstants = FALSE;
            }
            OGRFieldDefn *poFieldDefn;
            poFieldDefn = GetLayerDefn()->GetFieldDefn(poColumn->field_index);

            int nTableColIdx = m_poLyrTable->GetFieldIdx(poFieldDefn->GetNameRef());
            if( bAllConstants && nTableColIdx >= 0 &&
                m_poLyrTable->GetField(nTableColIdx)->HasIndex() )
            {
                FileGDBIterator* poRet = NULL;
                for(i=1;i<poNode->nSubExprCount;i++)
                {
                    OGRField sValue;
                    if( !FillTargetValueFromSrcExpr(poFieldDefn, &sValue,
                                                    poNode->papoSubExpr[i]) )
                    {
                        delete poRet;
                        poRet = NULL;
                        break;
                    }
                    FileGDBIterator* poIter = FileGDBIterator::Build(
                                        m_poLyrTable, nTableColIdx, TRUE, FGSO_EQ,
                                        poFieldDefn->GetType(), &sValue);
                    if( poIter == NULL )
                    {
                        delete poRet;
                        poRet = NULL;
                        break;
                    }
                    if( poRet == NULL )
                        poRet = poIter;
                    else
                        poRet = FileGDBIterator::BuildOr(poRet, poIter);
                }
                if( poRet != NULL )
                {
                    m_bIteratorSufficientToEvaluateFilter = TRUE;
                    return poRet;
                }
            }
        }
    }
    else if( poNode->eNodeType == SNT_OPERATION &&
             poNode->nOperation == SWQ_NOT && poNode->nSubExprCount == 1 )
    {
        FileGDBIterator* poIter = BuildIteratorFromExprNode(poNode->papoSubExpr[0]);
        /* If we have an iterator that is only partial w.r.t the full clause */
        /* then we cannot do anything with it unfortunately */
        if( m_bIteratorSufficientToEvaluateFilter == FALSE )
        {
            if( poIter != NULL )
                CPLDebug("OpenFileGDB", "Disabling use of indexes");
            delete poIter;
        }
        else if( poIter != NULL )
        {
            return FileGDBIterator::BuildNot(poIter);
        }
    }


    if( m_bIteratorSufficientToEvaluateFilter == TRUE )
        CPLDebug("OpenFileGDB", "Disabling use of indexes");
    m_bIteratorSufficientToEvaluateFilter = FALSE;
    return NULL;
}

/***********************************************************************/
/*                         SetAttributeFilter()                        */
/***********************************************************************/

OGRErr OGROpenFileGDBLayer::SetAttributeFilter( const char* pszFilter )
{
    if( !BuildLayerDefinition() )
        return OGRERR_FAILURE;

    delete m_poIterator;
    m_poIterator = NULL;
    m_bIteratorSufficientToEvaluateFilter = FALSE;

    OGRErr eErr = OGRLayer::SetAttributeFilter(pszFilter);
    if( eErr != OGRERR_NONE ||
        !CSLTestBoolean(CPLGetConfigOption("OPENFILEGDB_USE_INDEX", "YES")) )
        return eErr;

    if( m_poAttrQuery != NULL && m_nFilteredFeatureCount < 0 )
    {
        swq_expr_node* poNode = (swq_expr_node*) m_poAttrQuery->GetSWGExpr();
        m_bIteratorSufficientToEvaluateFilter = -1;
        m_poIterator = BuildIteratorFromExprNode(poNode);
        if( m_poIterator != NULL && m_eSpatialIndexState == SPI_IN_BUILDING )
            m_eSpatialIndexState = SPI_INVALID;
        if( m_bIteratorSufficientToEvaluateFilter < 0 )
            m_bIteratorSufficientToEvaluateFilter = FALSE;
    }
    return eErr;
}

/***********************************************************************/
/*                         GetCurrentFeature()                         */
/***********************************************************************/

OGRFeature* OGROpenFileGDBLayer::GetCurrentFeature()
{
    OGRFeature *poFeature = NULL;
    int iOGRIdx = 0;
    int iRow = m_poLyrTable->GetCurRow();
    for(int iGDBIdx=0;iGDBIdx<m_poLyrTable->GetFieldCount();iGDBIdx++)
    {
        if( iGDBIdx == m_iGeomFieldIdx )
        {
            if( m_poFeatureDefn->GetGeomFieldDefn(0)->IsIgnored() )
            {
                if( m_eSpatialIndexState == SPI_IN_BUILDING )
                    m_eSpatialIndexState = SPI_INVALID;
                continue;
            }

            const OGRField* psField = m_poLyrTable->GetFieldValue(iGDBIdx);
            if( psField != NULL )
            {
                if( m_eSpatialIndexState == SPI_IN_BUILDING )
                {
                    OGREnvelope sFeatureEnvelope;
                    if( m_poLyrTable->GetFeatureExtent(psField,
                                                       &sFeatureEnvelope) )
                    {
                        CPLRectObj sBounds;
                        sBounds.minx = sFeatureEnvelope.MinX;
                        sBounds.miny = sFeatureEnvelope.MinY;
                        sBounds.maxx = sFeatureEnvelope.MaxX;
                        sBounds.maxy = sFeatureEnvelope.MaxY;
                        CPLQuadTreeInsertWithBounds(m_pQuadTree,
                                                    (void*)(size_t)iRow,
                                                    &sBounds);
                    }
                }

                if( m_poFilterGeom != NULL &&
                    m_eSpatialIndexState != SPI_COMPLETED &&
                    !m_poLyrTable->DoesGeometryIntersectsFilterEnvelope(psField) )
                {
                    delete poFeature;
                    return NULL;
                }

                OGRGeometry* poGeom = m_poGeomConverter->GetAsGeometry(psField);
                if( poGeom != NULL )
                {
                    OGRwkbGeometryType eFlattenType = wkbFlatten(poGeom->getGeometryType());
                    if( eFlattenType == wkbPolygon )
                        poGeom = OGRGeometryFactory::forceToMultiPolygon(poGeom);
                    else if( eFlattenType == wkbLineString )
                        poGeom = OGRGeometryFactory::forceToMultiLineString(poGeom);
                    poGeom->assignSpatialReference(
                        m_poFeatureDefn->GetGeomFieldDefn(0)->GetSpatialRef() );

                    if( poFeature == NULL )
                        poFeature = new OGRFeature(m_poFeatureDefn);
                    poFeature->SetGeometryDirectly( poGeom );
                }
            }
        }
        else
        {
            if( !m_poFeatureDefn->GetFieldDefn(iOGRIdx)->IsIgnored() )
            {
                const OGRField* psField = m_poLyrTable->GetFieldValue(iGDBIdx);
                if( psField != NULL )
                {
                    if( poFeature == NULL )
                        poFeature = new OGRFeature(m_poFeatureDefn);

                    if( iGDBIdx == m_iFieldToReadAsBinary )
                        poFeature->SetField(iOGRIdx, (const char*) psField->Binary.paData);
                    else
                        poFeature->SetField(iOGRIdx, (OGRField*) psField);
                }
            }
            iOGRIdx ++;
        }
    }

    if( poFeature == NULL )
        poFeature = new OGRFeature(m_poFeatureDefn);
    poFeature->SetFID(iRow + 1);
    return poFeature;
}

/***********************************************************************/
/*                         GetNextFeature()                            */
/***********************************************************************/

OGRFeature* OGROpenFileGDBLayer::GetNextFeature()
{
    if( !BuildLayerDefinition() || m_bEOF )
        return NULL;

    while( TRUE )
    {
        OGRFeature *poFeature = NULL;

        if( m_nFilteredFeatureCount >= 0 )
        {
            while( TRUE )
            {
                if( m_iCurFeat >= m_nFilteredFeatureCount )
                {
                    return NULL;
                }
                int iRow = (int)(size_t)m_pahFilteredFeatures[m_iCurFeat++];
                if( m_poLyrTable->SelectRow(iRow) )
                {
                    poFeature = GetCurrentFeature();
                    if( poFeature )
                        break;
                }
                else if( m_poLyrTable->HasGotError() )
                {
                    m_bEOF = TRUE;
                    return NULL;
                }
            }
        }
        else if( m_poIterator != NULL )
        {
            while( TRUE )
            {
                int iRow = m_poIterator->GetNextRowSortedByFID();
                if( iRow < 0 )
                    return NULL;
                if( m_poLyrTable->SelectRow(iRow) )
                {
                    poFeature = GetCurrentFeature();
                    if( poFeature )
                        break;
                }
                else if( m_poLyrTable->HasGotError() )
                {
                    m_bEOF = TRUE;
                    return NULL;
                }
            }
        }
        else
        {
            while( TRUE )
            {
                if( m_iCurFeat == m_poLyrTable->GetTotalRecordCount() )
                {
                    return NULL;
                }
                if( m_poLyrTable->SelectRow(m_iCurFeat++) )
                {
                    poFeature = GetCurrentFeature();
                    if( m_eSpatialIndexState == SPI_IN_BUILDING &&
                        m_iCurFeat == m_poLyrTable->GetTotalRecordCount() )
                    {
                        CPLDebug("OpenFileGDB", "SPI_COMPLETED");
                        m_eSpatialIndexState = SPI_COMPLETED;
                    }
                    if( poFeature )
                        break;
                }
                else if( m_poLyrTable->HasGotError() )
                {
                    m_bEOF = TRUE;
                    return NULL;
                }
            }
        }

        if( (m_poFilterGeom == NULL
             || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == NULL ||
                (m_poIterator != NULL && m_bIteratorSufficientToEvaluateFilter) ||
                m_poAttrQuery->Evaluate( poFeature ) ) )
        {
            return poFeature;
        }

        delete poFeature;
    }
}

/***********************************************************************/
/*                          GetFeature()                               */
/***********************************************************************/

OGRFeature* OGROpenFileGDBLayer::GetFeature( GIntBig nFeatureId )
{
    if( !BuildLayerDefinition() )
        return NULL;

    if( nFeatureId < 1 || nFeatureId > m_poLyrTable->GetTotalRecordCount() )
        return NULL;
    if( !m_poLyrTable->SelectRow((int)nFeatureId - 1) )
        return NULL;

    /* Temporarily disable spatial filter */
    OGRGeometry* poOldSpatialFilter = m_poFilterGeom;
    m_poFilterGeom = NULL;
    /* and also spatial index state to avoid features to be inserted */
    /* multiple times in spatial index */
    SPIState eOldState = m_eSpatialIndexState;
    m_eSpatialIndexState = SPI_INVALID;

    OGRFeature* poFeature = GetCurrentFeature();

    /* Set it back */
    m_poFilterGeom = poOldSpatialFilter;
    m_eSpatialIndexState = eOldState;

    return poFeature;
}

/***********************************************************************/
/*                         SetNextByIndex()                            */
/***********************************************************************/

OGRErr OGROpenFileGDBLayer::SetNextByIndex( GIntBig nIndex )
{
    if( m_poIterator != NULL )
        return OGRLayer::SetNextByIndex(nIndex);

    if( !BuildLayerDefinition() )
        return OGRERR_FAILURE;

    if( m_eSpatialIndexState == SPI_IN_BUILDING )
        m_eSpatialIndexState = SPI_INVALID;

    if( m_nFilteredFeatureCount >= 0 )
    {
        if( nIndex < 0 || nIndex >= m_nFilteredFeatureCount )
            return OGRERR_FAILURE;
        m_iCurFeat = nIndex;
        return OGRERR_NONE;
    }
    else if( m_poLyrTable->GetValidRecordCount() == m_poLyrTable->GetTotalRecordCount() )
    {
        if( nIndex < 0 || nIndex >= m_poLyrTable->GetValidRecordCount() )
            return OGRERR_FAILURE;
        m_iCurFeat = nIndex;
        return OGRERR_NONE;
    }
    else
        return OGRLayer::SetNextByIndex(nIndex);
}

/***********************************************************************/
/*                           GetExtent()                               */
/***********************************************************************/

OGRErr OGROpenFileGDBLayer::GetExtent( OGREnvelope *psExtent, int bForce )
{
    (void)bForce;

    if( !BuildLayerDefinition() )
        return OGRERR_FAILURE;

    if( m_iGeomFieldIdx >= 0 && m_poLyrTable->GetValidRecordCount() > 0 )
    {
        FileGDBGeomField* poGDBGeomField =
            (FileGDBGeomField* )m_poLyrTable->GetField(m_iGeomFieldIdx);
        psExtent->MinX = poGDBGeomField->GetXMin();
        psExtent->MinY = poGDBGeomField->GetYMin();
        psExtent->MaxX = poGDBGeomField->GetXMax();
        psExtent->MaxY = poGDBGeomField->GetYMax();
        return OGRERR_NONE;
    }
    else
        return OGRERR_FAILURE;
}

/***********************************************************************/
/*                         GetFeatureCount()                           */
/***********************************************************************/

GIntBig OGROpenFileGDBLayer::GetFeatureCount( int bForce )
{
    if( !BuildLayerDefinition() )
        return 0;

    /* No filter */
    if( (m_poFilterGeom == NULL || m_iGeomFieldIdx < 0 ) &&
        m_poAttrQuery == NULL )
    {
        return m_poLyrTable->GetValidRecordCount();
    }
    else if( m_nFilteredFeatureCount >= 0 && m_poAttrQuery == NULL )
    {
        return m_nFilteredFeatureCount;
    }

    /* Only geometry filter ? */
    if( m_poAttrQuery == NULL && m_bFilterIsEnvelope )
    {
        int nCount = 0;
        if( m_eSpatialIndexState == SPI_IN_BUILDING && m_iCurFeat != 0 )
            m_eSpatialIndexState = SPI_INVALID;
        
        int nFilteredFeatureCountAlloc = 0;
        if( m_eSpatialIndexState == SPI_IN_BUILDING )
        {
            CPLFree(m_pahFilteredFeatures);
            m_pahFilteredFeatures = NULL;
            m_nFilteredFeatureCount = 0;
        }

        for(int i=0;i<m_poLyrTable->GetTotalRecordCount();i++)
        {
            if( !m_poLyrTable->SelectRow(i) )
            {
                if( m_poLyrTable->HasGotError() )
                    break;
                else
                    continue;
            }

            const OGRField* psField = m_poLyrTable->GetFieldValue(m_iGeomFieldIdx);
            if( psField != NULL )
            {
                if( m_eSpatialIndexState == SPI_IN_BUILDING )
                {
                    OGREnvelope sFeatureEnvelope;
                    if( m_poLyrTable->GetFeatureExtent(psField,
                                                       &sFeatureEnvelope) )
                    {
                        CPLRectObj sBounds;
                        sBounds.minx = sFeatureEnvelope.MinX;
                        sBounds.miny = sFeatureEnvelope.MinY;
                        sBounds.maxx = sFeatureEnvelope.MaxX;
                        sBounds.maxy = sFeatureEnvelope.MaxY;
                        CPLQuadTreeInsertWithBounds(m_pQuadTree,
                                                    (void*)(size_t)i,
                                                    &sBounds);
                    }
                }

                if( m_poLyrTable->DoesGeometryIntersectsFilterEnvelope(psField) )
                {
                    OGRGeometry* poGeom = m_poGeomConverter->GetAsGeometry(psField);
                    if( poGeom != NULL && FilterGeometry( poGeom ))
                    {
                        if( m_eSpatialIndexState == SPI_IN_BUILDING )
                        {
                            if( nCount == nFilteredFeatureCountAlloc )
                            {
                                nFilteredFeatureCountAlloc = 4 * nFilteredFeatureCountAlloc / 3 + 1024;
                                m_pahFilteredFeatures = (void**)CPLRealloc(
                                    m_pahFilteredFeatures, sizeof(void*) * nFilteredFeatureCountAlloc);
                            }
                            m_pahFilteredFeatures[nCount] = (void*)(size_t)i;
                        }
                        nCount ++;
                    }
                    delete poGeom;
                }
            }
        }
        if( m_eSpatialIndexState == SPI_IN_BUILDING )
        {
            m_nFilteredFeatureCount = nCount;
            m_eSpatialIndexState = SPI_COMPLETED;
        }

        return nCount;
    }
    /* Only simple attribute filter ? */
    else if( m_poFilterGeom == NULL &&
             m_poIterator != NULL && m_bIteratorSufficientToEvaluateFilter )
    {
        return m_poIterator->GetRowCount();
    }

    return OGRLayer::GetFeatureCount(bForce);
}

/***********************************************************************/
/*                         TestCapability()                            */
/***********************************************************************/

int OGROpenFileGDBLayer::TestCapability( const char * pszCap )
{
    if( !BuildLayerDefinition() )
        return FALSE;

    if( EQUAL(pszCap,OLCFastFeatureCount) )
    {
        return( (m_poFilterGeom == NULL || m_iGeomFieldIdx < 0 ) &&
                m_poAttrQuery == NULL );
    }
    else if( EQUAL(pszCap,OLCFastSetNextByIndex) )
    {
        return ( m_poLyrTable->GetValidRecordCount() ==
                 m_poLyrTable->GetTotalRecordCount() &&
                 m_poIterator == NULL );
    }
    else if( EQUAL(pszCap,OLCRandomRead) )
    {
        return TRUE;
    }
    else if( EQUAL(pszCap,OLCFastGetExtent) )
    {
        return TRUE;
    }
    else if( EQUAL(pszCap,OLCIgnoreFields) )
    {
        return TRUE;
    }
    else if( EQUAL(pszCap,OLCStringsAsUTF8) )
    {
        return TRUE; /* ? */
    }

    return FALSE;
}

/***********************************************************************/
/*                         HasIndexForField()                          */
/***********************************************************************/

int OGROpenFileGDBLayer::HasIndexForField(const char* pszFieldName)
{
    if( !BuildLayerDefinition() )
        return FALSE;
    
    int nTableColIdx = m_poLyrTable->GetFieldIdx(pszFieldName);
    return ( nTableColIdx >= 0 &&
             m_poLyrTable->GetField(nTableColIdx)->HasIndex() );
}

/***********************************************************************/
/*                             BuildIndex()                            */
/***********************************************************************/

FileGDBIterator* OGROpenFileGDBLayer::BuildIndex(const char* pszFieldName,
                                                 int bAscending,
                                                 swq_op op,
                                                 swq_expr_node* poValue)
{
    if( !BuildLayerDefinition() )
        return NULL;

    int idx = GetLayerDefn()->GetFieldIndex(pszFieldName);
    if( idx < 0 )
        return NULL;
    OGRFieldDefn* poFieldDefn = GetLayerDefn()->GetFieldDefn(idx);

    int nTableColIdx = m_poLyrTable->GetFieldIdx(pszFieldName);
    if( nTableColIdx >= 0 && m_poLyrTable->GetField(nTableColIdx)->HasIndex() )
    {
        if( op == SWQ_UNKNOWN )
            return FileGDBIterator::BuildIsNotNull(m_poLyrTable, nTableColIdx, bAscending);
        else
        {
            OGRField sValue;
            if( FillTargetValueFromSrcExpr(poFieldDefn, &sValue, poValue) )
            {
                FileGDBSQLOp eOp;
                switch( op )
                {
                    case SWQ_LE: eOp = FGSO_LE; break;
                    case SWQ_LT: eOp = FGSO_LT; break;
                    case SWQ_EQ: eOp = FGSO_EQ; break;
                    case SWQ_GE: eOp = FGSO_GE; break;
                    case SWQ_GT: eOp = FGSO_GT; break;
                    default: return NULL;
                }

                return FileGDBIterator::Build(
                                m_poLyrTable, nTableColIdx, bAscending,
                                eOp, poFieldDefn->GetType(), &sValue);
            }
        }
    }
    return NULL;
}

/***********************************************************************/
/*                          GetMinMaxValue()                           */
/***********************************************************************/

const OGRField* OGROpenFileGDBLayer::GetMinMaxValue(OGRFieldDefn* poFieldDefn,
                                                    int bIsMin,
                                                    int& eOutType)
{
    eOutType = OFTMaxType;
    if( !BuildLayerDefinition() )
        return NULL;

    int nTableColIdx = m_poLyrTable->GetFieldIdx(poFieldDefn->GetNameRef());
    if( nTableColIdx >= 0 && m_poLyrTable->GetField(nTableColIdx)->HasIndex() )
    {
        delete m_poIterMinMax;
        m_poIterMinMax = FileGDBIterator::BuildIsNotNull(
                                                m_poLyrTable, nTableColIdx, TRUE);
        if( m_poIterMinMax != NULL )
        {
            const OGRField* poRet = (bIsMin ) ?
                m_poIterMinMax->GetMinValue(eOutType) :
                m_poIterMinMax->GetMaxValue(eOutType);
            if( poRet == NULL )
                eOutType = poFieldDefn->GetType();
            return poRet;
        }
    }
    return NULL;
}

/***********************************************************************/
/*                        GetMinMaxSumCount()                          */
/***********************************************************************/

int OGROpenFileGDBLayer::GetMinMaxSumCount(OGRFieldDefn* poFieldDefn,
                                           double& dfMin, double& dfMax,
                                           double& dfSum, int& nCount)
{
    dfMin = 0.0;
    dfMax = 0.0;
    dfSum = 0.0;
    nCount = 0;
    if( !BuildLayerDefinition() )
        return FALSE;

    int nTableColIdx = m_poLyrTable->GetFieldIdx(poFieldDefn->GetNameRef());
    if( nTableColIdx >= 0 && m_poLyrTable->GetField(nTableColIdx)->HasIndex() )
    {
        FileGDBIterator* poIter = FileGDBIterator::BuildIsNotNull(
                                                m_poLyrTable, nTableColIdx, TRUE);
        if( poIter != NULL )
        {
            int nRet = poIter->GetMinMaxSumCount(dfMin, dfMax, dfSum, nCount);
            delete poIter;
            return nRet;
        }
    }
    return FALSE;
}
