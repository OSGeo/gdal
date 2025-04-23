/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements FileGDB OGR layer.
 * Author:   Ragi Yaser Burhum, ragi@burhum.com
 *           Paul Ramsey, pramsey at cleverelephant.ca
 *
 ******************************************************************************
 * Copyright (c) 2010, Ragi Yaser Burhum
 * Copyright (c) 2011, Paul Ramsey <pramsey at cleverelephant.ca>
 * Copyright (c) 2011-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include <cassert>
#include <cmath>

#include "ogr_fgdb.h"
#include "ogrpgeogeometry.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "FGdbUtils.h"
#include "cpl_minixml.h"  // the only way right now to extract schema information
#include "filegdb_gdbtoogrfieldtype.h"
#include "filegdb_fielddomain.h"
#include "filegdb_coordprec_read.h"

// See https://github.com/Esri/file-geodatabase-api/issues/46
// On certain FileGDB datasets with binary fields, iterating over a result set
// where the binary field is requested crashes in EnumRows::Next() at the
// second iteration.
// The workaround consists in iterating only over OBJECTID in the main loop,
// and requesting each feature in a separate request.
#define WORKAROUND_CRASH_ON_CDF_WITH_BINARY_FIELD

using std::string;
using std::wstring;

/************************************************************************/
/*                           FGdbBaseLayer()                            */
/************************************************************************/
FGdbBaseLayer::FGdbBaseLayer()
    : m_pFeatureDefn(nullptr), m_pSRS(nullptr), m_pEnumRows(nullptr),
      m_suppressColumnMappingError(false), m_forceMulti(false)
{
}

/************************************************************************/
/*                          ~FGdbBaseLayer()                            */
/************************************************************************/
FGdbBaseLayer::~FGdbBaseLayer()
{
    if (m_pFeatureDefn)
    {
        m_pFeatureDefn->Release();
        m_pFeatureDefn = nullptr;
    }

    FGdbBaseLayer::CloseGDBObjects();

    if (m_pSRS)
    {
        m_pSRS->Release();
        m_pSRS = nullptr;
    }
}

/************************************************************************/
/*                          CloseGDBObjects()                           */
/************************************************************************/

void FGdbBaseLayer::CloseGDBObjects()
{
    if (m_pEnumRows)
    {
        delete m_pEnumRows;
        m_pEnumRows = nullptr;
    }
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *FGdbBaseLayer::GetNextFeature()
{
    while (true)  // want to skip errors
    {
        if (m_pEnumRows == nullptr)
            return nullptr;

        long hr;

        Row row;

        if (FAILED(hr = m_pEnumRows->Next(row)))
        {
            GDBErr(hr, "Failed fetching features");
            return nullptr;
        }

        if (hr != S_OK)
        {
            // It's OK, we are done fetching - failure is caught by FAILED macro
            return nullptr;
        }

        OGRFeature *pOGRFeature = nullptr;

        if (!OGRFeatureFromGdbRow(&row, &pOGRFeature) || !pOGRFeature)
        {
            int32 oid = -1;
            CPL_IGNORE_RET_VAL(row.GetOID(oid));

            GDBErr(hr,
                   CPLSPrintf("Failed translating FGDB row [%d] to OGR Feature",
                              oid));

            // return NULL;
            continue;  // skip feature
        }

        if ((m_poFilterGeom == nullptr ||
             FilterGeometry(pOGRFeature->GetGeometryRef())))
        {
            return pOGRFeature;
        }
        delete pOGRFeature;
    }
}

/************************************************************************/
/*                              FGdbLayer()                             */
/************************************************************************/
FGdbLayer::FGdbLayer()
    : m_pDS(nullptr), m_pTable(nullptr), m_wstrSubfields(L"*"),
      m_bFilterDirty(true), m_bLaunderReservedKeywords(true)
{
    m_pEnumRows = new EnumRows;

#ifdef EXTENT_WORKAROUND
    m_bLayerEnvelopeValid = false;
#endif
}

/************************************************************************/
/*                            ~FGdbLayer()                              */
/************************************************************************/

FGdbLayer::~FGdbLayer()
{
    FGdbLayer::CloseGDBObjects();

    for (size_t i = 0; i < m_apoByteArrays.size(); i++)
        delete m_apoByteArrays[i];
    m_apoByteArrays.resize(0);
}

/************************************************************************/
/*                        CloseGDBObjects()                             */
/************************************************************************/

void FGdbLayer::CloseGDBObjects()
{
#ifdef EXTENT_WORKAROUND
    WorkAroundExtentProblem();
#endif

    if (m_pTable)
    {
        delete m_pTable;
        m_pTable = nullptr;
    }

    FGdbBaseLayer::CloseGDBObjects();
}

#ifdef EXTENT_WORKAROUND

/************************************************************************/
/*                     UpdateRowWithGeometry()                          */
/************************************************************************/

bool FGdbLayer::UpdateRowWithGeometry(Row &row, OGRGeometry *poGeom)
{
    ShapeBuffer shape;
    long hr;

    /* Write geometry to a buffer */
    GByte *pabyShape = nullptr;
    int nShapeSize = 0;
    if (OGRWriteToShapeBin(poGeom, &pabyShape, &nShapeSize) != OGRERR_NONE)
    {
        CPLFree(pabyShape);
        return false;
    }

    /* Copy it into a ShapeBuffer */
    if (nShapeSize > 0)
    {
        shape.Allocate(nShapeSize);
        memcpy(shape.shapeBuffer, pabyShape, nShapeSize);
        shape.inUseLength = nShapeSize;
    }

    /* Free the shape buffer */
    CPLFree(pabyShape);

    /* Write ShapeBuffer into the Row */
    hr = row.SetGeometry(shape);
    if (FAILED(hr))
    {
        return false;
    }

    /* Update row */
    hr = m_pTable->Update(row);
    if (FAILED(hr))
    {
        return false;
    }

    return true;
}

/************************************************************************/
/*                    WorkAroundExtentProblem()                         */
/*                                                                      */
/* Work-around problem with FileGDB API 1.1 on Linux 64bit. See #4455   */
/************************************************************************/

void FGdbLayer::WorkAroundExtentProblem()
{
    if (!m_bLayerEnvelopeValid)
        return;

    OGREnvelope sEnvelope;
    if (FGdbLayer::GetExtent(&sEnvelope, TRUE) != OGRERR_NONE)
        return;

    /* The characteristic of the bug is that the reported extent */
    /* is the real extent truncated incorrectly to integer values */
    /* We work around that by temporary updating one feature with a geometry */
    /* whose coordinates are integer values but ceil'ed and floor'ed */
    /* such that they include the real layer extent. */
    if (((double)(int)sEnvelope.MinX == sEnvelope.MinX &&
         (double)(int)sEnvelope.MinY == sEnvelope.MinY &&
         (double)(int)sEnvelope.MaxX == sEnvelope.MaxX &&
         (double)(int)sEnvelope.MaxY == sEnvelope.MaxY) &&
        (fabs(sEnvelope.MinX - sLayerEnvelope.MinX) > 1e-5 ||
         fabs(sEnvelope.MinY - sLayerEnvelope.MinY) > 1e-5 ||
         fabs(sEnvelope.MaxX - sLayerEnvelope.MaxX) > 1e-5 ||
         fabs(sEnvelope.MaxY - sLayerEnvelope.MaxY) > 1e-5))
    {
        long hr;
        Row row;
        EnumRows enumRows;

        if (FAILED(hr = m_pTable->Search(StringToWString("*"),
                                         StringToWString(""), true, enumRows)))
            return;

        if (FAILED(hr = enumRows.Next(row)))
            return;

        if (hr != S_OK)
            return;

        /* Backup original shape buffer */
        ShapeBuffer originalGdbGeometry;
        if (FAILED(hr = row.GetGeometry(originalGdbGeometry)))
            return;

        OGRGeometry *pOGRGeo = nullptr;
        if ((!GDBGeometryToOGRGeometry(m_forceMulti, &originalGdbGeometry,
                                       m_pSRS, &pOGRGeo)) ||
            pOGRGeo == nullptr)
        {
            delete pOGRGeo;
            return;
        }

        OGRwkbGeometryType eType = wkbFlatten(pOGRGeo->getGeometryType());

        delete pOGRGeo;
        pOGRGeo = nullptr;

        OGRPoint oP1(floor(sLayerEnvelope.MinX), floor(sLayerEnvelope.MinY));
        OGRPoint oP2(ceil(sLayerEnvelope.MaxX), ceil(sLayerEnvelope.MaxY));

        OGRLinearRing oLR;
        oLR.addPoint(&oP1);
        oLR.addPoint(&oP2);
        oLR.addPoint(&oP1);

        if (eType == wkbPoint)
        {
            UpdateRowWithGeometry(row, &oP1);
            UpdateRowWithGeometry(row, &oP2);
        }
        else if (eType == wkbLineString)
        {
            UpdateRowWithGeometry(row, &oLR);
        }
        else if (eType == wkbPolygon)
        {
            OGRPolygon oPoly;
            oPoly.addRing(&oLR);

            UpdateRowWithGeometry(row, &oPoly);
        }
        else if (eType == wkbMultiPoint)
        {
            OGRMultiPoint oColl;
            oColl.addGeometry(&oP1);
            oColl.addGeometry(&oP2);

            UpdateRowWithGeometry(row, &oColl);
        }
        else if (eType == wkbMultiLineString)
        {
            OGRMultiLineString oColl;
            oColl.addGeometry(&oLR);

            UpdateRowWithGeometry(row, &oColl);
        }
        else if (eType == wkbMultiPolygon)
        {
            OGRMultiPolygon oColl;
            OGRPolygon oPoly;
            oPoly.addRing(&oLR);
            oColl.addGeometry(&oPoly);

            UpdateRowWithGeometry(row, &oColl);
        }
        else
            return;

        /* Restore original ShapeBuffer */
        hr = row.SetGeometry(originalGdbGeometry);
        if (FAILED(hr))
            return;

        /* Update Row */
        hr = m_pTable->Update(row);
        if (FAILED(hr))
            return;

        CPLDebug("FGDB",
                 "Workaround extent problem with Linux 64bit FGDB SDK 1.1");
    }
}
#endif  // EXTENT_WORKAROUND

/************************************************************************/
/*                             GetRow()                                 */
/************************************************************************/

OGRErr FGdbLayer::GetRow(EnumRows &enumRows, Row &row, GIntBig nFID)
{
    long hr;
    CPLString osQuery;

    /* Querying a 64bit FID causes a runtime exception in FileGDB... */
    if (!CPL_INT64_FITS_ON_INT32(nFID))
    {
        return OGRERR_FAILURE;
    }

    osQuery.Printf("%s = " CPL_FRMT_GIB, m_strOIDFieldName.c_str(), nFID);

    if (FAILED(hr = m_pTable->Search(m_wstrSubfields,
                                     StringToWString(osQuery.c_str()), true,
                                     enumRows)))
    {
        GDBErr(hr, "Failed fetching row ");
        return OGRERR_FAILURE;
    }

    if (FAILED(hr = enumRows.Next(row)))
    {
        GDBErr(hr, "Failed fetching row ");
        return OGRERR_FAILURE;
    }

    if (hr != S_OK)
        return OGRERR_NON_EXISTING_FEATURE;  // none found - but no failure

    return OGRERR_NONE;
}

/*************************************************************************/
/*                            Initialize()                               */
/* Has ownership of the table as soon as it is called.                   */
/************************************************************************/

bool FGdbLayer::Initialize(FGdbDataSource *pParentDataSource, Table *pTable,
                           const std::wstring &wstrTablePath,
                           const std::wstring &wstrType)
{
    long hr;

    m_pDS = pParentDataSource;  // we never assume ownership of the parent - so
                                // our destructor should not delete

    m_pTable = pTable;

    m_wstrTablePath = wstrTablePath;
    m_wstrType = wstrType;

    wstring wstrQueryName;
    if (FAILED(hr = pParentDataSource->GetGDB()->GetQueryName(wstrTablePath,
                                                              wstrQueryName)))
        return GDBErr(hr, "Failed at getting underlying table name for " +
                              WStringToString(wstrTablePath));

    m_strName = WStringToString(wstrQueryName);

    m_pFeatureDefn = new OGRFeatureDefn(
        m_strName.c_str());  // TODO: Should I "new" an OGR smart pointer -
                             // sample says so, but it doesn't seem right
    SetDescription(m_pFeatureDefn->GetName());
    // as long as we use the same compiler & settings in both the ogr build and
    // this driver, we should be OK
    m_pFeatureDefn->Reference();

    string tableDef;
    if (FAILED(hr = m_pTable->GetDefinition(tableDef)))
        return GDBErr(hr, "Failed at getting table definition for " +
                              WStringToString(wstrTablePath));

    // CPLDebug("FGDB", "tableDef = %s", tableDef.c_str());

    bool abort = false;

    // extract schema information from table
    CPLXMLNode *psRoot = CPLParseXMLString(tableDef.c_str());

    if (psRoot == nullptr)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined, "%s",
            ("Failed parsing GDB Table Schema XML for " + m_strName).c_str());
        return false;
    }

    CPLXMLNode *pDataElementNode =
        psRoot->psNext;  // Move to next field which should be DataElement

    if (pDataElementNode != nullptr && pDataElementNode->psChild != nullptr &&
        pDataElementNode->eType == CXT_Element &&
        EQUAL(pDataElementNode->pszValue, "esri:DataElement"))
    {
        CPLXMLNode *psNode;

        m_bTimeInUTC = CPLTestBool(
            CPLGetXMLValue(pDataElementNode, "IsTimeInUTC", "false"));

        std::string osAreaFieldName;
        std::string osLengthFieldName;
        for (psNode = pDataElementNode->psChild; psNode != nullptr;
             psNode = psNode->psNext)
        {
            if (psNode->eType == CXT_Element && psNode->psChild != nullptr)
            {
                if (EQUAL(psNode->pszValue, "OIDFieldName"))
                {
                    m_strOIDFieldName = CPLGetXMLValue(psNode, nullptr, "");
                }
                else if (EQUAL(psNode->pszValue, "ShapeFieldName"))
                {
                    m_strShapeFieldName = CPLGetXMLValue(psNode, nullptr, "");
                }
                else if (EQUAL(psNode->pszValue, "AreaFieldName"))
                {
                    osAreaFieldName = CPLGetXMLValue(psNode, nullptr, "");
                }
                else if (EQUAL(psNode->pszValue, "LengthFieldName"))
                {
                    osLengthFieldName = CPLGetXMLValue(psNode, nullptr, "");
                }
                else if (EQUAL(psNode->pszValue, "Fields"))
                {
                    if (!GDBToOGRFields(psNode))
                    {
                        abort = true;
                        break;
                    }
                }
            }
        }

        if (!osAreaFieldName.empty())
        {
            const int nIdx =
                m_pFeatureDefn->GetFieldIndex(osAreaFieldName.c_str());
            if (nIdx >= 0)
            {
                m_pFeatureDefn->GetFieldDefn(nIdx)->SetDefault(
                    "FILEGEODATABASE_SHAPE_AREA");
            }
        }

        if (!osLengthFieldName.empty())
        {
            const int nIdx =
                m_pFeatureDefn->GetFieldIndex(osLengthFieldName.c_str());
            if (nIdx >= 0)
            {
                m_pFeatureDefn->GetFieldDefn(nIdx)->SetDefault(
                    "FILEGEODATABASE_SHAPE_LENGTH");
            }
        }

        if (m_strShapeFieldName.empty())
            m_pFeatureDefn->SetGeomType(wkbNone);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                 ("Failed parsing GDB Table Schema XML (DataElement) for " +
                  m_strName)
                     .c_str());
        return false;
    }
    CPLDestroyXMLNode(psRoot);

    if (m_pFeatureDefn->GetGeomFieldCount() != 0)
    {
        m_pFeatureDefn->GetGeomFieldDefn(0)->SetName(
            m_strShapeFieldName.c_str());
        m_pFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(m_pSRS);
    }

    if (abort)
        return false;

    return true;  // AOToOGRFields(ipFields, m_pFeatureDefn,
                  // m_vOGRFieldToESRIField);
}

/************************************************************************/
/*                          ParseGeometryDef()                          */
/************************************************************************/

bool FGdbLayer::ParseGeometryDef(const CPLXMLNode *psRoot)
{
    string geometryType;
    bool hasZ = false, hasM = false;
    string wkt, wkid, latestwkid;

    OGRGeomCoordinatePrecision oCoordPrec;
    for (const CPLXMLNode *psGeometryDefItem = psRoot->psChild;
         psGeometryDefItem; psGeometryDefItem = psGeometryDefItem->psNext)
    {
        // loop through all "GeometryDef" elements
        //

        if (psGeometryDefItem->eType == CXT_Element &&
            psGeometryDefItem->psChild != nullptr)
        {
            if (EQUAL(psGeometryDefItem->pszValue, "GeometryType"))
            {
                geometryType = CPLGetXMLValue(psGeometryDefItem, nullptr, "");
            }
            else if (EQUAL(psGeometryDefItem->pszValue, "SpatialReference"))
            {
                ParseSpatialReference(
                    psGeometryDefItem, &wkt, &wkid,
                    &latestwkid);  // we don't check for success because it
                                   // may not be there
                oCoordPrec = GDBGridSettingsToOGR(psGeometryDefItem);
            }
            else if (EQUAL(psGeometryDefItem->pszValue, "HasM"))
            {
                if (!strcmp(CPLGetXMLValue(psGeometryDefItem, nullptr, ""),
                            "true"))
                    hasM = true;
            }
            else if (EQUAL(psGeometryDefItem->pszValue, "HasZ"))
            {
                if (!strcmp(CPLGetXMLValue(psGeometryDefItem, nullptr, ""),
                            "true"))
                    hasZ = true;
            }
        }
    }

    OGRwkbGeometryType ogrGeoType;
    if (!GDBToOGRGeometry(geometryType, hasZ, hasM, &ogrGeoType))
        return false;

    m_pFeatureDefn->SetGeomType(ogrGeoType);

    if (m_pFeatureDefn->GetGeomFieldCount() != 0)
        m_pFeatureDefn->GetGeomFieldDefn(0)->SetCoordinatePrecision(oCoordPrec);

    if (wkbFlatten(ogrGeoType) == wkbMultiLineString ||
        wkbFlatten(ogrGeoType) == wkbMultiPoint)
        m_forceMulti = true;

    if (latestwkid.length() > 0 || wkid.length() > 0)
    {
        int bSuccess = FALSE;
        m_pSRS = new OGRSpatialReference();
        m_pSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        CPLPushErrorHandler(CPLQuietErrorHandler);
        if (latestwkid.length() > 0)
        {
            if (m_pSRS->importFromEPSG(atoi(latestwkid.c_str())) == OGRERR_NONE)
            {
                bSuccess = TRUE;
            }
            else
            {
                CPLDebug("FGDB", "Cannot import SRID %s", latestwkid.c_str());
            }
        }
        if (!bSuccess && wkid.length() > 0)
        {
            if (m_pSRS->importFromEPSG(atoi(wkid.c_str())) == OGRERR_NONE)
            {
                bSuccess = TRUE;
            }
            else
            {
                CPLDebug("FGDB", "Cannot import SRID %s", wkid.c_str());
            }
        }
        CPLPopErrorHandler();
        CPLErrorReset();
        if (!bSuccess)
        {
            delete m_pSRS;
            m_pSRS = nullptr;
        }
        else
            return true;
    }

    if (wkt.length() > 0)
    {
        if (!GDBToOGRSpatialReference(wkt, &m_pSRS))
        {
            // report error, but be passive about it
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Failed Mapping ESRI Spatial Reference");
        }
    }

    return true;
}

/************************************************************************/
/*                        ParseSpatialReference()                       */
/************************************************************************/

bool FGdbLayer::ParseSpatialReference(const CPLXMLNode *psSpatialRefNode,
                                      string *pOutWkt, string *pOutWKID,
                                      string *pOutLatestWKID)
{
    *pOutWkt = "";
    *pOutWKID = "";
    *pOutLatestWKID = "";

    /* Loop through all the SRS elements we want to store */
    for (const CPLXMLNode *psSRItemNode = psSpatialRefNode->psChild;
         psSRItemNode; psSRItemNode = psSRItemNode->psNext)
    {
        /* The WKID maps (mostly) to an EPSG code */
        if (psSRItemNode->eType == CXT_Element &&
            psSRItemNode->psChild != nullptr &&
            EQUAL(psSRItemNode->pszValue, "WKID"))
        {
            *pOutWKID = CPLGetXMLValue(psSRItemNode, nullptr, "");

            // Needed with FileGDB v1.4 with layers with empty SRS
            if (*pOutWKID == "0")
                *pOutWKID = "";
        }
        /* The concept of LatestWKID is explained in
         * http://resources.arcgis.com/en/help/arcgis-rest-api/index.html#//02r3000000n1000000
         */
        else if (psSRItemNode->eType == CXT_Element &&
                 psSRItemNode->psChild != nullptr &&
                 EQUAL(psSRItemNode->pszValue, "LatestWKID"))
        {
            *pOutLatestWKID = CPLGetXMLValue(psSRItemNode, nullptr, "");
        }
        /* The WKT well-known text can be converted by OGR */
        else if (psSRItemNode->eType == CXT_Element &&
                 psSRItemNode->psChild != nullptr &&
                 EQUAL(psSRItemNode->pszValue, "WKT"))
        {
            *pOutWkt = CPLGetXMLValue(psSRItemNode, nullptr, "");
        }
    }
    return *pOutWkt != "" || *pOutWKID != "";
}

/************************************************************************/
/*                          GDBToOGRFields()                           */
/************************************************************************/

bool FGdbLayer::GDBToOGRFields(CPLXMLNode *psRoot)
{
    m_vOGRFieldToESRIField.clear();

    if (psRoot->psChild == nullptr || psRoot->psChild->psNext == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unrecognized GDB XML Schema");

        return false;
    }

    psRoot = psRoot->psChild->psNext;  // change root to "FieldArray"

    // CPLAssert(ogrToESRIFieldMapping.size() ==
    // pOGRFeatureDef->GetFieldCount());

    CPLXMLNode *psFieldNode;

    for (psFieldNode = psRoot->psChild; psFieldNode != nullptr;
         psFieldNode = psFieldNode->psNext)
    {
        // loop through all "Field" elements
        //

        if (psFieldNode->eType == CXT_Element &&
            psFieldNode->psChild != nullptr &&
            EQUAL(psFieldNode->pszValue, "Field"))
        {

            CPLXMLNode *psFieldItemNode;
            std::string fieldName;
            std::string fieldAlias;
            std::string fieldType;
            int nLength = 0;
            int bNullable = TRUE;
            std::string osDefault;
            std::string osDomainName;

            // loop through all items in Field element
            //

            for (psFieldItemNode = psFieldNode->psChild;
                 psFieldItemNode != nullptr;
                 psFieldItemNode = psFieldItemNode->psNext)
            {
                if (psFieldItemNode->eType == CXT_Element)
                {
                    const char *pszValue =
                        CPLGetXMLValue(psFieldItemNode, nullptr, "");
                    if (EQUAL(psFieldItemNode->pszValue, "Name"))
                    {
                        fieldName = pszValue;
                    }
                    else if (EQUAL(psFieldItemNode->pszValue, "AliasName"))
                    {
                        fieldAlias = pszValue;
                    }
                    else if (EQUAL(psFieldItemNode->pszValue, "Type"))
                    {
                        fieldType = pszValue;
                    }
                    else if (EQUAL(psFieldItemNode->pszValue, "GeometryDef"))
                    {
                        if (!ParseGeometryDef(psFieldItemNode))
                            return false;  // if we failed parsing the
                                           // GeometryDef, we are done!
                    }
                    else if (EQUAL(psFieldItemNode->pszValue, "Length"))
                    {
                        nLength = atoi(pszValue);
                    }
                    else if (EQUAL(psFieldItemNode->pszValue, "IsNullable"))
                    {
                        bNullable = EQUAL(pszValue, "true");
                    }
                    else if (EQUAL(psFieldItemNode->pszValue, "DefaultValue"))
                    {
                        osDefault = pszValue;
                    }
                    // NOTE: when using the GetDefinition() API, the domain name
                    // is set in <Domain><DomainName>, whereas the raw XML is
                    // just <DomainName>
                    else if (EQUAL(psFieldItemNode->pszValue, "Domain"))
                    {
                        osDomainName =
                            CPLGetXMLValue(psFieldItemNode, "DomainName", "");
                    }
                }
            }

            ///////////////////////////////////////////////////////////////////
            // At this point we have parsed everything about the current field

            if (fieldType == "esriFieldTypeGeometry")
            {
                m_strShapeFieldName = fieldName;
                m_pFeatureDefn->GetGeomFieldDefn(0)->SetNullable(bNullable);

                continue;  // finish here for special field - don't add as OGR
                           // fielddef
            }
            else if (fieldType == "esriFieldTypeOID")
            {
                // m_strOIDFieldName = fieldName; // already set by this point

                continue;  // finish here for special field - don't add as OGR
                           // fielddef
            }

            OGRFieldType ogrType;
            OGRFieldSubType eSubType;
            // CPLDebug("FGDB", "name = %s, type = %s", fieldName.c_str(),
            // fieldType.c_str() );
            if (!GDBToOGRFieldType(fieldType, &ogrType, &eSubType))
            {
                // field cannot be mapped, skipping further processing
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Skipping field: [%s] type: [%s] ", fieldName.c_str(),
                         fieldType.c_str());
                continue;
            }

            // TODO: Optimization - modify m_wstrSubFields so it only fetches
            // fields that are mapped

            OGRFieldDefn fieldTemplate(fieldName.c_str(), ogrType);
            if (fieldAlias != fieldName)
            {
                // The SDK generates an alias even with it is not explicitly
                // written
                fieldTemplate.SetAlternativeName(fieldAlias.c_str());
            }
            fieldTemplate.SetSubType(eSubType);
            /* On creation (GDBFieldTypeToLengthInBytes) if string width is 0,
             * we pick up */
            /* 65536 by default to mean unlimited string length, but we don't
             * want */
            /* to advertise such a big number */
            if (ogrType == OFTString && nLength < 65536)
                fieldTemplate.SetWidth(nLength);
            fieldTemplate.SetNullable(bNullable);
            if (!osDefault.empty())
            {
                if (ogrType == OFTString)
                {
                    char *pszTmp =
                        CPLEscapeString(osDefault.c_str(), -1, CPLES_SQL);
                    osDefault = "'";
                    osDefault += pszTmp;
                    CPLFree(pszTmp);
                    osDefault += "'";
                    fieldTemplate.SetDefault(osDefault.c_str());
                }
                else if (ogrType == OFTInteger || ogrType == OFTReal)
                {
#ifdef unreliable
                    /* Disabling this as GDBs and the FileGDB SDK aren't
                     * reliable for numeric values */
                    /* It often occurs that the XML definition in
                     * a00000004.gdbtable doesn't */
                    /* match the default values (in binary) found in the field
                     * definition */
                    /* section of the .gdbtable of the layers themselves */
                    /* The Table::GetDefinition() API of FileGDB doesn't seem to
                     * use the */
                    /* XML definition, but rather the values found in the field
                     * definition */
                    /* section of the .gdbtable of the layers themselves */
                    /* It seems that the XML definition in a00000004.gdbtable is
                     * authoritative */
                    /* in ArcGIS, so we're screwed... */

                    fieldTemplate.SetDefault(osDefault.c_str());
#endif
                }
                else if (ogrType == OFTDateTime)
                {
                    int nYear, nMonth, nDay, nHour, nMinute;
                    float fSecond;
                    if (sscanf(osDefault.c_str(), "%d-%d-%dT%d:%d:%fZ", &nYear,
                               &nMonth, &nDay, &nHour, &nMinute,
                               &fSecond) == 6 ||
                        sscanf(osDefault.c_str(), "'%d-%d-%d %d:%d:%fZ'",
                               &nYear, &nMonth, &nDay, &nHour, &nMinute,
                               &fSecond) == 6)
                    {
                        fieldTemplate.SetDefault(CPLSPrintf(
                            "'%04d/%02d/%02d %02d:%02d:%02d'", nYear, nMonth,
                            nDay, nHour, nMinute, (int)(fSecond + 0.5)));
                    }
                }
            }
            if (!osDomainName.empty())
            {
                fieldTemplate.SetDomainName(osDomainName);
            }

            m_pFeatureDefn->AddFieldDefn(&fieldTemplate);

            m_vOGRFieldToESRIField.push_back(StringToWString(fieldName));
            m_vOGRFieldToESRIFieldType.push_back(fieldType);
            if (ogrType == OFTBinary)
                m_apoByteArrays.push_back(new ByteArray());
        }
    }

    /* Using OpenFileGDB to get reliable default values for integer/real fields
     */
    /* and alias */
    if (m_pDS->UseOpenFileGDB())
    {
        const char *const apszDrivers[] = {"OpenFileGDB", nullptr};
        GDALDataset *poDS = GDALDataset::Open(
            m_pDS->GetFSName(), GDAL_OF_VECTOR, apszDrivers, nullptr, nullptr);
        if (poDS != nullptr)
        {
            OGRLayer *poLyr = poDS->GetLayerByName(GetName());
            if (poLyr)
            {
                const auto poOFGBLayerDefn = poLyr->GetLayerDefn();
                const int nOFGDBFieldCount = poOFGBLayerDefn->GetFieldCount();
                for (int i = 0; i < nOFGDBFieldCount; i++)
                {
                    const OGRFieldDefn *poSrcDefn =
                        poOFGBLayerDefn->GetFieldDefn(i);
                    if ((poSrcDefn->GetType() == OFTInteger ||
                         poSrcDefn->GetType() == OFTReal) &&
                        poSrcDefn->GetDefault() != nullptr)
                    {
                        int nIdxDst = m_pFeatureDefn->GetFieldIndex(
                            poSrcDefn->GetNameRef());
                        if (nIdxDst >= 0)
                            m_pFeatureDefn->GetFieldDefn(nIdxDst)->SetDefault(
                                poSrcDefn->GetDefault());
                    }

                    // XML parsing by the SDK fails when there are special
                    // characters, like &, so fallback to using OpenFileGDB.
                    const char *pszAlternativeName =
                        poSrcDefn->GetAlternativeNameRef();
                    if (pszAlternativeName != nullptr &&
                        pszAlternativeName[0] != '\0' &&
                        strcmp(pszAlternativeName, poSrcDefn->GetNameRef()) !=
                            0)
                    {
                        int nIdxDst = m_pFeatureDefn->GetFieldIndex(
                            poSrcDefn->GetNameRef());
                        if (nIdxDst >= 0)
                            m_pFeatureDefn->GetFieldDefn(nIdxDst)
                                ->SetAlternativeName(pszAlternativeName);
                    }
                }
            }
            GDALClose(poDS);
        }
    }

    return true;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void FGdbLayer::ResetReading()
{
    long hr;

    if (m_pTable == nullptr)
        return;

#ifdef WORKAROUND_CRASH_ON_CDF_WITH_BINARY_FIELD
    const std::wstring wstrSubFieldBackup(m_wstrSubfields);
    if (!m_apoByteArrays.empty())
    {
        m_bWorkaroundCrashOnCDFWithBinaryField = CPLTestBool(CPLGetConfigOption(
            "OGR_FGDB_WORKAROUND_CRASH_ON_BINARY_FIELD", "YES"));
        if (m_bWorkaroundCrashOnCDFWithBinaryField)
        {
            m_wstrSubfields = StringToWString(m_strOIDFieldName);
            if (!m_strShapeFieldName.empty() && m_poFilterGeom &&
                !m_poFilterGeom->IsEmpty())
            {
                m_wstrSubfields += StringToWString(", " + m_strShapeFieldName);
            }
        }
    }
#endif

    if (m_poFilterGeom && !m_poFilterGeom->IsEmpty())
    {
        // Search spatial
        // As of beta1, FileGDB only supports bbox searched, if we have GEOS
        // installed, we can do the rest ourselves.

        OGREnvelope ogrEnv;

        m_poFilterGeom->getEnvelope(&ogrEnv);

        // spatial query
        FileGDBAPI::Envelope env(ogrEnv.MinX, ogrEnv.MaxX, ogrEnv.MinY,
                                 ogrEnv.MaxY);

        if (FAILED(hr = m_pTable->Search(m_wstrSubfields, m_wstrWhereClause,
                                         env, true, *m_pEnumRows)))
            GDBErr(hr, "Failed Searching");
    }
    else
    {
        // Search non-spatial
        if (FAILED(hr = m_pTable->Search(m_wstrSubfields, m_wstrWhereClause,
                                         true, *m_pEnumRows)))
            GDBErr(hr, "Failed Searching");
    }

#ifdef WORKAROUND_CRASH_ON_CDF_WITH_BINARY_FIELD
    if (!m_apoByteArrays.empty() && m_bWorkaroundCrashOnCDFWithBinaryField)
        m_wstrSubfields = wstrSubFieldBackup;
#endif

    m_bFilterDirty = false;
}

/************************************************************************/
/*                         ISetSpatialFilter()                          */
/************************************************************************/

OGRErr FGdbLayer::ISetSpatialFilter(int iGeomField, const OGRGeometry *pOGRGeom)
{
    m_bFilterDirty = true;
    return OGRLayer::ISetSpatialFilter(iGeomField, pOGRGeom);
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr FGdbLayer::SetAttributeFilter(const char *pszQuery)
{
    m_wstrWhereClause = StringToWString((pszQuery != nullptr) ? pszQuery : "");

    m_bFilterDirty = true;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           OGRFeatureFromGdbRow()                      */
/************************************************************************/

bool FGdbBaseLayer::OGRFeatureFromGdbRow(Row *pRow, OGRFeature **ppFeature)
{
    long hr;

    OGRFeature *pOutFeature = new OGRFeature(m_pFeatureDefn);

    /////////////////////////////////////////////////////////
    // Translate OID
    //

    int32 oid = -1;
    if (FAILED(hr = pRow->GetOID(oid)))
    {
        // this should never happen unless not selecting the OBJECTID
    }
    else
    {
        pOutFeature->SetFID(oid);
    }

    /////////////////////////////////////////////////////////
    // Translate Geometry
    //

    ShapeBuffer gdbGeometry;
    // Row::GetGeometry() will fail with -2147467259 for NULL geometries
    // Row::GetGeometry() will fail with -2147219885 for tables without a
    // geometry field
    if (!m_pFeatureDefn->IsGeometryIgnored() &&
        !FAILED(hr = pRow->GetGeometry(gdbGeometry)))
    {
        OGRGeometry *pOGRGeo = nullptr;

        if ((!GDBGeometryToOGRGeometry(m_forceMulti, &gdbGeometry, m_pSRS,
                                       &pOGRGeo)))
        {
            delete pOutFeature;
            return GDBErr(hr, "Failed to translate FileGDB Geometry to OGR "
                              "Geometry for row " +
                                  string(CPLSPrintf("%d", (int)oid)));
        }

        pOutFeature->SetGeometryDirectly(pOGRGeo);
    }

    //////////////////////////////////////////////////////////
    // Map fields
    //

    int mappedFieldCount = static_cast<int>(m_vOGRFieldToESRIField.size());

    bool foundBadColumn = false;

    for (int i = 0; i < mappedFieldCount; ++i)
    {
        OGRFieldDefn *poFieldDefn = m_pFeatureDefn->GetFieldDefn(i);
        // The IsNull() and GetXXX() API are very slow when there are a
        // big number of fields, for example with Tiger database.
        if (poFieldDefn->IsIgnored())
            continue;

        const wstring &wstrFieldName = m_vOGRFieldToESRIField[i];
        const std::string &strFieldType = m_vOGRFieldToESRIFieldType[i];

        bool isNull = false;

        if (FAILED(hr = pRow->IsNull(wstrFieldName, isNull)))
        {
            GDBErr(hr, "Failed to determine NULL status from column " +
                           WStringToString(wstrFieldName));
            foundBadColumn = true;
            continue;
        }

        if (isNull)
        {
            pOutFeature->SetFieldNull(i);
            continue;
        }

        //
        // NOTE: This switch statement needs to be kept in sync with
        // GDBToOGRFieldType utility function
        //       since we are only checking for types we mapped in that utility
        //       function

        switch (poFieldDefn->GetType())
        {

            case OFTInteger:
            {
                int32 val;

                if (FAILED(hr = pRow->GetInteger(wstrFieldName, val)))
                {
                    int16 shortval;
                    if (FAILED(hr = pRow->GetShort(wstrFieldName, shortval)))
                    {
                        GDBErr(hr,
                               "Failed to determine integer value for column " +
                                   WStringToString(wstrFieldName));
                        foundBadColumn = true;
                        continue;
                    }
                    val = shortval;
                }

                pOutFeature->SetField(i, (int)val);
            }
            break;

            case OFTReal:
            {
                if (strFieldType == "esriFieldTypeSingle")
                {
                    float val;

                    if (FAILED(hr = pRow->GetFloat(wstrFieldName, val)))
                    {
                        GDBErr(hr,
                               "Failed to determine float value for column " +
                                   WStringToString(wstrFieldName));
                        foundBadColumn = true;
                        continue;
                    }

                    pOutFeature->SetField(i, val);
                }
                else
                {
                    double val;

                    if (FAILED(hr = pRow->GetDouble(wstrFieldName, val)))
                    {
                        GDBErr(hr,
                               "Failed to determine real value for column " +
                                   WStringToString(wstrFieldName));
                        foundBadColumn = true;
                        continue;
                    }

                    pOutFeature->SetField(i, val);
                }
            }
            break;
            case OFTString:
            {
                wstring val;
                std::string strValue;

                if (strFieldType == "esriFieldTypeGlobalID")
                {
                    Guid guid;
                    if (FAILED(hr = pRow->GetGlobalID(guid)) ||
                        FAILED(hr = guid.ToString(val)))
                    {
                        GDBErr(hr,
                               "Failed to determine string value for column " +
                                   WStringToString(wstrFieldName));
                        foundBadColumn = true;
                        continue;
                    }
                    strValue = WStringToString(val);
                }
                else if (strFieldType == "esriFieldTypeGUID")
                {
                    Guid guid;
                    if (FAILED(hr = pRow->GetGUID(wstrFieldName, guid)) ||
                        FAILED(hr = guid.ToString(val)))
                    {
                        GDBErr(hr,
                               "Failed to determine string value for column " +
                                   WStringToString(wstrFieldName));
                        foundBadColumn = true;
                        continue;
                    }
                    strValue = WStringToString(val);
                }
                else if (strFieldType == "esriFieldTypeXML")
                {
                    if (FAILED(hr = pRow->GetXML(wstrFieldName, strValue)))
                    {
                        GDBErr(hr, "Failed to determine XML value for column " +
                                       WStringToString(wstrFieldName));
                        foundBadColumn = true;
                        continue;
                    }
                }
                else
                {
                    if (FAILED(hr = pRow->GetString(wstrFieldName, val)))
                    {
                        GDBErr(hr,
                               "Failed to determine string value for column " +
                                   WStringToString(wstrFieldName));
                        foundBadColumn = true;
                        continue;
                    }
                    strValue = WStringToString(val);
                }

                pOutFeature->SetField(i, strValue.c_str());
            }
            break;

            case OFTBinary:
            {
                ByteArray binaryBuf;

                if (FAILED(hr = pRow->GetBinary(wstrFieldName, binaryBuf)))
                {
                    GDBErr(hr, "Failed to determine binary value for column " +
                                   WStringToString(wstrFieldName));
                    foundBadColumn = true;
                    continue;
                }

                pOutFeature->SetField(i, (int)binaryBuf.inUseLength,
                                      (GByte *)binaryBuf.byteArray);
            }
            break;

            case OFTDateTime:
            {
                struct tm val;

                if (FAILED(hr = pRow->GetDate(wstrFieldName, val)))
                {
                    GDBErr(hr, "Failed to determine date value for column " +
                                   WStringToString(wstrFieldName));
                    foundBadColumn = true;
                    continue;
                }

                pOutFeature->SetField(i, val.tm_year + 1900, val.tm_mon + 1,
                                      val.tm_mday, val.tm_hour, val.tm_min,
                                      (float)val.tm_sec,
                                      m_bTimeInUTC ? 100 : 0);
                // Examine test data to figure out how to extract that
            }
            break;

            default:
            {
                if (!m_suppressColumnMappingError)
                {
                    foundBadColumn = true;
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Row id: %d col:%d has unhandled col type (%d). "
                             "Setting to NULL.",
                             (int)oid, (int)i,
                             m_pFeatureDefn->GetFieldDefn(i)->GetType());
                }
            }
        }
    }

    if (foundBadColumn)
        m_suppressColumnMappingError = true;

    *ppFeature = pOutFeature;

    return true;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *FGdbLayer::GetNextFeature()
{
    if (m_bFilterDirty)
        ResetReading();

#ifdef WORKAROUND_CRASH_ON_CDF_WITH_BINARY_FIELD
    if (!m_apoByteArrays.empty() && m_bWorkaroundCrashOnCDFWithBinaryField)
    {
        while (true)
        {
            if (m_pEnumRows == nullptr)
                return nullptr;

            long hr;

            Row rowOnlyOid;

            if (FAILED(hr = m_pEnumRows->Next(rowOnlyOid)))
            {
                GDBErr(hr, "Failed fetching features");
                return nullptr;
            }

            if (hr != S_OK)
            {
                // It's OK, we are done fetching - failure is caught by FAILED
                // macro
                return nullptr;
            }

            int32 oid = -1;
            if (FAILED(hr = rowOnlyOid.GetOID(oid)))
            {
                GDBErr(hr, "Failed to get oid");
                continue;
            }

            EnumRows enumRows;
            OGRFeature *pOGRFeature = nullptr;
            Row rowFull;
            if (GetRow(enumRows, rowFull, oid) != OGRERR_NONE ||
                !OGRFeatureFromGdbRow(&rowFull, &pOGRFeature) || !pOGRFeature)
            {
                GDBErr(hr,
                       CPLSPrintf(
                           "Failed translating FGDB row [%d] to OGR Feature",
                           oid));

                // return NULL;
                continue;  // skip feature
            }

            if ((m_poFilterGeom == nullptr ||
                 FilterGeometry(pOGRFeature->GetGeometryRef())))
            {
                return pOGRFeature;
            }
            delete pOGRFeature;
        }
    }
#endif

    OGRFeature *poFeature = FGdbBaseLayer::GetNextFeature();
    return poFeature;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *FGdbLayer::GetFeature(GIntBig oid)
{
    // do query to fetch individual row
    EnumRows enumRows;
    Row row;
    if (!CPL_INT64_FITS_ON_INT32(oid) || m_pTable == nullptr)
        return nullptr;

    int nFID32 = (int)oid;

    if (GetRow(enumRows, row, nFID32) != OGRERR_NONE)
        return nullptr;

    OGRFeature *pOGRFeature = nullptr;

    if (!OGRFeatureFromGdbRow(&row, &pOGRFeature))
    {
        return nullptr;
    }
    if (pOGRFeature)
    {
        pOGRFeature->SetFID(oid);
    }

    return pOGRFeature;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig FGdbLayer::GetFeatureCount(CPL_UNUSED int bForce)
{
    int32 rowCount = 0;

    if (m_pTable == nullptr)
        return 0;

    if (m_poFilterGeom != nullptr || !m_wstrWhereClause.empty())
    {
        ResetReading();
        if (m_pEnumRows == nullptr)
            return 0;

        int nFeatures = 0;
        while (true)
        {
            long hr;

            Row row;

            if (FAILED(hr = m_pEnumRows->Next(row)))
            {
                GDBErr(hr, "Failed fetching features");
                return 0;
            }

            if (hr != S_OK)
            {
                break;
            }

            if (m_poFilterGeom == nullptr)
            {
                nFeatures++;
            }
            else
            {
                ShapeBuffer gdbGeometry;
                if (FAILED(hr = row.GetGeometry(gdbGeometry)))
                {
                    continue;
                }

                OGRGeometry *pOGRGeo = nullptr;
                if (!GDBGeometryToOGRGeometry(m_forceMulti, &gdbGeometry,
                                              m_pSRS, &pOGRGeo) ||
                    pOGRGeo == nullptr)
                {
                    delete pOGRGeo;
                    continue;
                }

                if (FilterGeometry(pOGRGeo))
                {
                    nFeatures++;
                }

                delete pOGRGeo;
            }
        }
        ResetReading();
        return nFeatures;
    }

    long hr;
    if (FAILED(hr = m_pTable->GetRowCount(rowCount)))
    {
        GDBErr(hr, "Failed counting rows");
        return 0;
    }

    return static_cast<int>(rowCount);
}

/************************************************************************/
/*                         GetMetadataItem()                            */
/************************************************************************/

const char *FGdbLayer::GetMetadataItem(const char *pszName,
                                       const char *pszDomain)
{
    return OGRLayer::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                            IGetExtent()                              */
/************************************************************************/

OGRErr FGdbLayer::IGetExtent(int iGeomField, OGREnvelope *psExtent, bool bForce)
{
    if (m_pTable == nullptr)
        return OGRERR_FAILURE;

    if (m_poFilterGeom != nullptr || !m_wstrWhereClause.empty() ||
        m_strShapeFieldName.empty())
    {
        const int nFieldCount = m_pFeatureDefn->GetFieldCount();
        int *pabSaveFieldIgnored = new int[nFieldCount];
        for (int i = 0; i < nFieldCount; i++)
        {
            // cppcheck-suppress uninitdata
            pabSaveFieldIgnored[i] =
                m_pFeatureDefn->GetFieldDefn(i)->IsIgnored();
            m_pFeatureDefn->GetFieldDefn(i)->SetIgnored(TRUE);
        }
        OGRErr eErr = OGRLayer::IGetExtent(iGeomField, psExtent, bForce);
        for (int i = 0; i < nFieldCount; i++)
        {
            m_pFeatureDefn->GetFieldDefn(i)->SetIgnored(pabSaveFieldIgnored[i]);
        }
        delete[] pabSaveFieldIgnored;
        return eErr;
    }

    long hr;
    Envelope envelope;
    if (FAILED(hr = m_pTable->GetExtent(envelope)))
    {
        GDBErr(hr, "Failed fetching extent");
        return OGRERR_FAILURE;
    }

    psExtent->MinX = envelope.xMin;
    psExtent->MinY = envelope.yMin;
    psExtent->MaxX = envelope.xMax;
    psExtent->MaxY = envelope.yMax;

    if (std::isnan(psExtent->MinX) || std::isnan(psExtent->MinY) ||
        std::isnan(psExtent->MaxX) || std::isnan(psExtent->MaxY))
        return OGRERR_FAILURE;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           GetLayerXML()                              */
/* Return XML definition of the Layer as provided by FGDB. Caller must  */
/* free result.                                                         */
/* Not currently used by the driver, but can be used by external code   */
/* for specific purposes.                                               */
/************************************************************************/

OGRErr FGdbLayer::GetLayerXML(char **ppXml)
{
    long hr;
    std::string xml;

    if (m_pTable == nullptr)
        return OGRERR_FAILURE;

    if (FAILED(hr = m_pTable->GetDefinition(xml)))
    {
        GDBErr(hr, "Failed fetching XML table definition");
        return OGRERR_FAILURE;
    }

    *ppXml = CPLStrdup(xml.c_str());
    return OGRERR_NONE;
}

/************************************************************************/
/*                           GetLayerMetadataXML()                      */
/* Return XML metadata for the Layer as provided by FGDB. Caller must  */
/* free result.                                                         */
/* Not currently used by the driver, but can be used by external code   */
/* for specific purposes.                                               */
/************************************************************************/

OGRErr FGdbLayer::GetLayerMetadataXML(char **ppXml)
{
    long hr;
    std::string xml;

    if (m_pTable == nullptr)
        return OGRERR_FAILURE;

    if (FAILED(hr = m_pTable->GetDocumentation(xml)))
    {
        GDBErr(hr, "Failed fetching XML table metadata");
        return OGRERR_FAILURE;
    }

    *ppXml = CPLStrdup(xml.c_str());
    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int FGdbLayer::TestCapability(const char *pszCap)
{

    if (EQUAL(pszCap, OLCRandomRead))
        return TRUE;

    else if (EQUAL(pszCap, OLCFastFeatureCount))
        return m_poFilterGeom == nullptr && m_wstrWhereClause.empty();

    else if (EQUAL(pszCap, OLCFastSpatialFilter))
        return TRUE;

    else if (EQUAL(pszCap, OLCFastGetExtent))
        return m_poFilterGeom == nullptr && m_wstrWhereClause.empty();

    else if (EQUAL(pszCap,
                   OLCStringsAsUTF8)) /* Native UTF16, converted to UTF8 */
        return TRUE;

    else if (EQUAL(pszCap,
                   OLCFastSetNextByIndex)) /* TBD FastSetNextByIndex() */
        return FALSE;

    else if (EQUAL(pszCap, OLCIgnoreFields))
        return TRUE;

    else if (EQUAL(pszCap, OLCMeasuredGeometries))
        return TRUE;

    else if (EQUAL(pszCap, OLCZGeometries))
        return TRUE;

    else
        return FALSE;
}

/************************************************************************/
/*                             GetDataset()                             */
/************************************************************************/

GDALDataset *FGdbLayer::GetDataset()
{
    return m_pDS;
}
