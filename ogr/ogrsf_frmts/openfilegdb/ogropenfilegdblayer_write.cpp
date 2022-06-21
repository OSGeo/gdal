/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Open FileGDB OGR driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"
#include "ogr_openfilegdb.h"
#include "filegdb_gdbtoogrfieldtype.h"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <algorithm>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"
#include "ogr_spatialref.h"
#include "ogr_srs_api.h"
#include "ogrsf_frmts.h"
#include "filegdbtable.h"

/*************************************************************************/
/*                            StringToWString()                          */
/*************************************************************************/

static
std::wstring StringToWString(const std::string& utf8string)
{
    wchar_t* pszUTF16 = CPLRecodeToWChar( utf8string.c_str(), CPL_ENC_UTF8, CPL_ENC_UCS2);
    std::wstring utf16string = pszUTF16;
    CPLFree(pszUTF16);
    return utf16string;
}

/*************************************************************************/
/*                            WStringToString()                          */
/*************************************************************************/

static
std::string WStringToString(const std::wstring& utf16string)
{
    char* pszUTF8 = CPLRecodeFromWChar( utf16string.c_str(), CPL_ENC_UCS2, CPL_ENC_UTF8 );
    std::string utf8string = pszUTF8;
    CPLFree(pszUTF8);
    return utf8string;
}

/*************************************************************************/
/*                              LaunderName()                            */
/*************************************************************************/

static
std::wstring LaunderName(const std::wstring& name)
{
    std::wstring newName = name;

    // https://support.esri.com/en/technical-article/000005588

    // "Do not start field or table names with an underscore or a number."
    // But we can see in the wild table names starting with underscore...
    // (cf https://github.com/OSGeo/gdal/issues/4112)
    if( !newName.empty() && newName[0]>='0' && newName[0]<='9' )
    {
        newName = StringToWString("_") + newName;
    }

    // "Essentially, eliminate anything that is not alphanumeric or an underscore."
    // Note: alphanumeric unicode is supported
    for( size_t i=0; i < newName.size(); i++)
    {
        if ( !( newName[i] == '_' ||
              ( newName[i]>='0' && newName[i]<='9') ||
              ( newName[i]>='a' && newName[i]<='z') ||
              ( newName[i]>='A' && newName[i]<='Z') ||
               newName[i] >= 128 ) )
        {
            newName[i] = '_';
        }
    }

    return newName;
}

/*************************************************************************/
/*                      EscapeUnsupportedPrefixes()                      */
/*************************************************************************/

static
std::wstring EscapeUnsupportedPrefixes(const std::wstring& className)
{
    std::wstring newName = className;
    // From ESRI docs
    // Feature classes starting with these strings are unsupported.
    static const char* const UNSUPPORTED_PREFIXES[] = {"sde_", "gdb_", "delta_", nullptr};

    for (int i = 0; UNSUPPORTED_PREFIXES[i] != nullptr; i++)
    {
        // cppcheck-suppress stlIfStrFind
        if (newName.find(StringToWString(UNSUPPORTED_PREFIXES[i])) == 0)
        {
            // Normally table names shouldn't start with underscore, but
            // there are such in the wild (cf https://github.com/OSGeo/gdal/issues/4112)
            newName = StringToWString("_") + newName;
            break;
        }
    }

    return newName;
}

/*************************************************************************/
/*                         EscapeReservedKeywords()                      */
/*************************************************************************/

static
std::wstring EscapeReservedKeywords(const std::wstring& name)
{
    std::string newName = WStringToString(name);
    std::string upperName = CPLString(newName).toupper();

    // From ESRI docs
    static const char* const RESERVED_WORDS[] = {"OBJECTID", "ADD", "ALTER", "AND", "AS", "ASC", "BETWEEN",
                                    "BY", "COLUMN", "CREATE", "DATE", "DELETE", "DESC",
                                    "DROP", "EXISTS", "FOR", "FROM", "IN", "INSERT", "INTO",
                                    "IS", "LIKE", "NOT", "NULL", "OR", "ORDER", "SELECT",
                                    "SET", "TABLE", "UPDATE", "VALUES", "WHERE", nullptr};

    // Append an underscore to any FGDB reserved words used as field names
    // This is the same behavior ArcCatalog follows.
    for (int i = 0; RESERVED_WORDS[i] != nullptr; i++)
    {
        const char* w = RESERVED_WORDS[i];
        if (upperName == w)
        {
            newName += '_';
            break;
        }
    }

    return StringToWString(newName);
}

/***********************************************************************/
/*                     XMLSerializeGeomFieldBase()                     */
/***********************************************************************/

static void XMLSerializeGeomFieldBase(CPLXMLNode* psRoot,
                                  const FileGDBGeomField* poGeomFieldDefn,
                                  const OGRSpatialReference* poSRS)
{
    auto psExtent = CPLCreateXMLElementAndValue(psRoot, "Extent", "");
    CPLAddXMLAttributeAndValue(psExtent, "xsi:nil", "true");

    auto psSpatialReference = CPLCreateXMLNode(psRoot, CXT_Element, "SpatialReference");

    if( poSRS == nullptr )
    {
        CPLAddXMLAttributeAndValue(psSpatialReference, "xsi:type",
                                   "typens:UnknownCoordinateSystem");
    }
    else
    {
        if( poSRS->IsGeographic() )
            CPLAddXMLAttributeAndValue(psSpatialReference, "xsi:type",
                                       "typens:GeographicCoordinateSystem");
        else
            CPLAddXMLAttributeAndValue(psSpatialReference, "xsi:type",
                                       "typens:ProjectedCoordinateSystem");
        CPLCreateXMLElementAndValue(psSpatialReference, "WKT",
                                    poGeomFieldDefn->GetWKT().c_str());
    }
    CPLCreateXMLElementAndValue(psSpatialReference, "XOrigin",
                                CPLSPrintf("%.18g", poGeomFieldDefn->GetXOrigin()));
    CPLCreateXMLElementAndValue(psSpatialReference, "YOrigin",
                                CPLSPrintf("%.18g", poGeomFieldDefn->GetYOrigin()));
    CPLCreateXMLElementAndValue(psSpatialReference, "XYScale",
                                CPLSPrintf("%.18g", poGeomFieldDefn->GetXYScale()));
    CPLCreateXMLElementAndValue(psSpatialReference, "ZOrigin",
                                CPLSPrintf("%.18g", poGeomFieldDefn->GetZOrigin()));
    CPLCreateXMLElementAndValue(psSpatialReference, "ZScale",
                                CPLSPrintf("%.18g", poGeomFieldDefn->GetZScale()));
    CPLCreateXMLElementAndValue(psSpatialReference, "MOrigin",
                                CPLSPrintf("%.18g", poGeomFieldDefn->GetMOrigin()));
    CPLCreateXMLElementAndValue(psSpatialReference, "MScale",
                                CPLSPrintf("%.18g", poGeomFieldDefn->GetMScale()));
    CPLCreateXMLElementAndValue(psSpatialReference, "XYTolerance",
                                CPLSPrintf("%.18g", poGeomFieldDefn->GetXYTolerance()));
    CPLCreateXMLElementAndValue(psSpatialReference, "ZTolerance",
                                CPLSPrintf("%.18g", poGeomFieldDefn->GetZTolerance()));
    CPLCreateXMLElementAndValue(psSpatialReference, "MTolerance",
                                CPLSPrintf("%.18g", poGeomFieldDefn->GetMTolerance()));
    CPLCreateXMLElementAndValue(psSpatialReference, "HighPrecision", "true");
    if( poSRS )
    {
        const char* pszAuthorityName = poSRS->GetAuthorityName(nullptr);
        const char* pszAuthorityCode = poSRS->GetAuthorityCode(nullptr);
        if( pszAuthorityName && pszAuthorityCode &&
            (EQUAL(pszAuthorityName, "EPSG") || EQUAL(pszAuthorityName, "ESRI")) )
        {
            CPLCreateXMLElementAndValue(psSpatialReference, "WKID", pszAuthorityCode);
        }
    }
}

/***********************************************************************/
/*                    CreateFeatureDataset()                           */
/***********************************************************************/

bool OGROpenFileGDBLayer::CreateFeatureDataset(const char* pszFeatureDataset)
{
    std::string osPath("\\");
    osPath += pszFeatureDataset;

    CPLXMLTreeCloser oTree(CPLCreateXMLNode(nullptr, CXT_Element, "?xml"));
    CPLAddXMLAttributeAndValue(oTree.get(), "version", "1.0");
    CPLAddXMLAttributeAndValue(oTree.get(), "encoding", "UTF-8");

    CPLXMLNode *psRoot = CPLCreateXMLNode(nullptr, CXT_Element, "typens:DEFeatureDataset");
    CPLAddXMLSibling(oTree.get(), psRoot);

    CPLAddXMLAttributeAndValue(psRoot, "xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
    CPLAddXMLAttributeAndValue(psRoot, "xmlns:xs", "http://www.w3.org/2001/XMLSchema");
    CPLAddXMLAttributeAndValue(psRoot, "xmlns:typens", "http://www.esri.com/schemas/ArcGIS/10.1");
    CPLAddXMLAttributeAndValue(psRoot, "xsi:type", "typens:DEFeatureDataset");

    CPLCreateXMLElementAndValue(psRoot, "CatalogPath", osPath.c_str());
    CPLCreateXMLElementAndValue(psRoot, "Name", pszFeatureDataset);
    CPLCreateXMLElementAndValue(psRoot, "ChildrenExpanded", "false");
    CPLCreateXMLElementAndValue(psRoot, "DatasetType", "esriDTFeatureDataset");

    {
        FileGDBTable oTable;
        if( !oTable.Open(m_poDS->m_osGDBItemsFilename.c_str(), false) )
            return false;
        CPLCreateXMLElementAndValue(psRoot, "DSID",
            CPLSPrintf("%d", 1 + oTable.GetTotalRecordCount()));
    }

    CPLCreateXMLElementAndValue(psRoot, "Versioned", "false");
    CPLCreateXMLElementAndValue(psRoot, "CanVersion", "false");

    if( m_eGeomType != wkbNone )
    {
        XMLSerializeGeomFieldBase(psRoot,
                              m_poLyrTable->GetGeomField(),
                              GetSpatialRef());
    }

    char* pszDefinition = CPLSerializeXMLTree(oTree.get());
    const std::string osDefinition = pszDefinition;
    CPLFree(pszDefinition);

    m_osFeatureDatasetGUID = OFGDBGenerateUUID();

    if( !m_poDS->RegisterInItemRelationships(m_poDS->m_osRootGUID,
                                             m_osFeatureDatasetGUID,
                                             "{dc78f1ab-34e4-43ac-ba47-1c4eabd0e7c7}") )
    {
        return false;
    }

    if( !m_poDS->RegisterFeatureDatasetInItems(m_osFeatureDatasetGUID,
                                               pszFeatureDataset,
                                               osDefinition.c_str()) )
    {
        return false;
    }

    return true;
}

/***********************************************************************/
/*                      GetLaunderedLayerName()                        */
/***********************************************************************/

std::string OGROpenFileGDBLayer::GetLaunderedLayerName(const std::string& osNameOri) const
{
    std::wstring wlayerName = StringToWString(osNameOri);

    wlayerName = LaunderName(wlayerName);
    wlayerName = EscapeReservedKeywords(wlayerName);
    wlayerName = EscapeUnsupportedPrefixes(wlayerName);

    // https://desktop.arcgis.com/en/arcmap/latest/manage-data/administer-file-gdbs/file-geodatabase-size-and-name-limits.htm document 160 character limit
    // but https://desktop.arcgis.com/en/arcmap/latest/manage-data/tables/fundamentals-of-adding-and-deleting-fields.htm#GUID-8E190093-8F8F-4132-AF4F-B0C9220F76B3 mentions 64.
    // let be optimistic and aim for 160
    constexpr size_t TABLE_NAME_MAX_SIZE = 160;
    if (wlayerName.size() > TABLE_NAME_MAX_SIZE)
        wlayerName.resize(TABLE_NAME_MAX_SIZE);

    /* Ensures uniqueness of layer name */
    int numRenames = 1;
    while ((m_poDS->GetLayerByName(WStringToString(wlayerName).c_str()) != nullptr) && (numRenames < 10))
    {
        wlayerName = StringToWString(
            CPLSPrintf("%s_%d", WStringToString(wlayerName.substr(0, TABLE_NAME_MAX_SIZE-2)).c_str(), numRenames));
        numRenames ++;
    }
    while ((m_poDS->GetLayerByName(WStringToString(wlayerName).c_str()) != nullptr) && (numRenames < 100))
    {
        wlayerName = StringToWString(
            CPLSPrintf("%s_%d", WStringToString(wlayerName.substr(0, TABLE_NAME_MAX_SIZE-3)).c_str(), numRenames));
        numRenames ++;
    }

    return WStringToString(wlayerName);
}

/***********************************************************************/
/*                            Create()                                 */
/***********************************************************************/

bool OGROpenFileGDBLayer::Create(const OGRSpatialReference* poSRS)
{
    FileGDBTableGeometryType eTableGeomType = FGTGT_NONE;
    const auto eFlattenType = wkbFlatten(m_eGeomType);
    if( eFlattenType == wkbNone )
        eTableGeomType = FGTGT_NONE;
    else if( eFlattenType == wkbPoint )
        eTableGeomType = FGTGT_POINT;
    else if( eFlattenType == wkbMultiPoint )
        eTableGeomType = FGTGT_MULTIPOINT;
    else if( eFlattenType == wkbLineString ||
             eFlattenType == wkbMultiLineString ||
             eFlattenType == wkbCircularString ||
             eFlattenType == wkbCompoundCurve ||
             eFlattenType == wkbMultiCurve )
    {
        eTableGeomType = FGTGT_LINE;
    }
    else if( eFlattenType == wkbPolygon || eFlattenType == wkbMultiPolygon ||
             eFlattenType == wkbCompoundCurve || eFlattenType == wkbMultiSurface )
    {
        eTableGeomType = FGTGT_POLYGON;
    }
    else if( eFlattenType == wkbTIN || eFlattenType == wkbPolyhedralSurface ||
             m_eGeomType == wkbGeometryCollection25D ||
             m_eGeomType == wkbSetZ(wkbUnknown) )
    {
        eTableGeomType = FGTGT_MULTIPATCH;
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported geometry type");
        return false;
    }

    const std::string osNameOri(m_osName);
    /* Launder the Layer name */
    m_osName = GetLaunderedLayerName(osNameOri);
    if (osNameOri != m_osName)
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                "Normalized/laundered layer name: '%s' to '%s'",
                osNameOri.c_str(), m_osName.c_str());
    }

    const char* pszFeatureDataset = m_aosCreationOptions.FetchNameValue("FEATURE_DATASET");
    std::string osFeatureDatasetDef;
    std::unique_ptr<OGRSpatialReference> poFeatureDatasetSRS;
    if( pszFeatureDataset )
    {
        {
            FileGDBTable oTable;
            if( !oTable.Open(m_poDS->m_osGDBItemsFilename.c_str(), false) )
                return false;

            FETCH_FIELD_IDX(iUUID, "UUID", FGFT_GLOBALID);
            FETCH_FIELD_IDX(iName, "Name", FGFT_STRING);
            FETCH_FIELD_IDX(iDefinition, "Definition", FGFT_XML);

            for( int iCurFeat = 0; iCurFeat < oTable.GetTotalRecordCount(); ++iCurFeat )
            {
                iCurFeat = oTable.GetAndSelectNextNonEmptyRow(iCurFeat);
                if( iCurFeat < 0 )
                    break;
                const auto psName = oTable.GetFieldValue(iName);
                if( psName && strcmp(psName->String, pszFeatureDataset) == 0 )
                {
                    const auto psDefinition = oTable.GetFieldValue(iDefinition);
                    if( psDefinition )
                    {
                        osFeatureDatasetDef = psDefinition->String;
                    }
                    else
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Feature dataset found, but no defininition");
                        return false;
                    }

                    const auto psUUID = oTable.GetFieldValue(iUUID);
                    if( psUUID == nullptr )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Feature dataset found, but no UUID");
                        return false;
                    }

                    m_osFeatureDatasetGUID = psUUID->String;
                    break;
                }
            }
        }
        CPLXMLNode* psParentTree = CPLParseXMLString(osFeatureDatasetDef.c_str());
        if( psParentTree != nullptr )
        {
            CPLStripXMLNamespace( psParentTree, nullptr, TRUE );
            CPLXMLNode* psParentInfo = CPLSearchXMLNode( psParentTree, "=DEFeatureDataset" );
            if( psParentInfo != nullptr )
            {
                poFeatureDatasetSRS.reset(BuildSRS(psParentInfo));
            }
            CPLDestroyXMLNode(psParentTree);
        }
    }

    m_poFeatureDefn = new OGROpenFileGDBFeatureDefn(this, m_osName.c_str(), true);
    SetDescription( m_poFeatureDefn->GetName() );
    m_poFeatureDefn->SetGeomType(wkbNone);
    m_poFeatureDefn->Reference();
    if( m_eGeomType != wkbNone )
    {
        auto poGeomFieldDefn =
                cpl::make_unique<OGROpenFileGDBGeomFieldDefn>(this,
                    m_aosCreationOptions.FetchNameValueDef("GEOMETRY_NAME", "SHAPE"),
                    m_eGeomType);
        poGeomFieldDefn->SetNullable(
            CPLTestBool(m_aosCreationOptions.FetchNameValueDef("GEOMETRY_NULLABLE", "YES")));

        if( poSRS )
        {
            const char* const apszOptions[] = {
                "IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES",
                nullptr
            };
            if( poFeatureDatasetSRS &&
                !poSRS->IsSame(poFeatureDatasetSRS.get(), apszOptions) )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Layer CRS does not match feature dataset CRS");
                return false;
            }

            auto poSRSClone = poSRS->Clone();
            poGeomFieldDefn->SetSpatialRef(poSRSClone);
            poSRSClone->Release();
        }
        else if( poFeatureDatasetSRS )
        {
            auto poSRSClone = poFeatureDatasetSRS->Clone();
            poGeomFieldDefn->SetSpatialRef(poSRSClone);
            poSRSClone->Release();
        }

        m_poFeatureDefn->AddGeomFieldDefn(std::move(poGeomFieldDefn));
    }

    m_osThisGUID = OFGDBGenerateUUID();

    m_bValidLayerDefn = true;
    m_bEditable = true;
    m_bRegisteredTable = false;
    m_bTimeInUTC = CPLTestBool(m_aosCreationOptions.FetchNameValueDef("TIME_IN_UTC", "YES"));

    int nTablxOffsetSize = 5;
    bool bTextUTF16 = false;
    const char* pszConfigurationKeyword = m_aosCreationOptions.FetchNameValue("CONFIGURATION_KEYWORD");
    if( pszConfigurationKeyword)
    {
        if( EQUAL(pszConfigurationKeyword, "MAX_FILE_SIZE_4GB") )
        {
            m_osConfigurationKeyword = "MAX_FILE_SIZE_4GB";
            nTablxOffsetSize = 4;
        }
        else if( EQUAL(pszConfigurationKeyword, "MAX_FILE_SIZE_256TB") )
        {
            m_osConfigurationKeyword = "MAX_FILE_SIZE_256TB";
            nTablxOffsetSize = 6;
        }
        else if( EQUAL(pszConfigurationKeyword, "TEXT_UTF16") )
        {
            m_osConfigurationKeyword = "TEXT_UTF16";
            bTextUTF16 = true;
        }
        else if( !EQUAL(pszConfigurationKeyword, "DEFAULTS") )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported value for CONFIGURATION_KEYWORD: %s",
                     pszConfigurationKeyword);
            return false;
        }
    }

    m_osPath = '\\';
    if( pszFeatureDataset )
    {
        m_osPath += pszFeatureDataset;
        m_osPath += '\\';
    }
    m_osPath += m_osName;

    const char* pszDocumentation = m_aosCreationOptions.FetchNameValue("DOCUMENTATION");
    if( pszDocumentation)
        m_osDocumentation = pszDocumentation;

    const bool bGeomTypeHasZ = CPL_TO_BOOL(OGR_GT_HasZ(m_eGeomType));
    const bool bGeomTypeHasM = CPL_TO_BOOL(OGR_GT_HasM(m_eGeomType));

    m_poLyrTable = new FileGDBTable();
    if( !m_poLyrTable->Create(m_osGDBFilename.c_str(), nTablxOffsetSize,
                              eTableGeomType, bGeomTypeHasZ, bGeomTypeHasM) )
    {
        Close();
        return false;
    }
    if( bTextUTF16 )
        m_poLyrTable->SetTextUTF16();

    // To be able to test this unusual situation of having an attribute field
    // before the geometry field
    if( CPLTestBool(CPLGetConfigOption(
            "OPENFILEGDB_CREATE_FIELD_BEFORE_GEOMETRY", "NO")) )
    {
        OGRFieldDefn oField("field_before_geom", OFTString);
        m_poLyrTable->CreateField(cpl::make_unique<FileGDBField>(
                oField.GetNameRef(),
                std::string(),
                FGFT_STRING,
                true, 0, FileGDBField::UNSET_FIELD));
        m_poFeatureDefn->AddFieldDefn(&oField);
    }

    if( m_eGeomType != wkbNone )
    {
        std::string osWKT;
        if( poSRS )
        {
            const char* const apszOptions[] = { "FORMAT=WKT1_ESRI", nullptr };
            char* pszWKT;
            poSRS->exportToWkt(&pszWKT, apszOptions);
            osWKT = pszWKT;
            CPLFree(pszWKT);
        }
        else
        {
            osWKT = "{B286C06B-0879-11D2-AACA-00C04FA33C20}";
        }

        double dfXOrigin;
        double dfYOrigin;
        double dfXYScale;
        double dfZOrigin = -100000;
        double dfMOrigin = -100000;
        double dfMScale = 10000;
        double dfXYTolerance;
        // default tolerance is 1mm in the units of the coordinate system
        double dfZTolerance = 0.001 * (poSRS ? poSRS->GetTargetLinearUnits("VERT_CS") : 1.0);
        double dfZScale = 1 / dfZTolerance * 10;
        double dfMTolerance = 0.001;

        if( poSRS == nullptr || poSRS->IsProjected() )
        {
            // default tolerance is 1mm in the units of the coordinate system
            dfXYTolerance = 0.001 * (poSRS ? poSRS->GetTargetLinearUnits("PROJCS") : 1.0);
            // default scale is 10x the tolerance
            dfXYScale = 1 / dfXYTolerance * 10;

            // Ideally we would use the same X/Y origins as ArcGIS, but we need the algorithm they use.
            dfXOrigin = -2147483647;
            dfYOrigin = -2147483647;
        }
        else
        {
            dfXOrigin = -400;
            dfYOrigin = -400;
            dfXYScale = 1000000000;
            dfXYTolerance = 0.000000008983153;
        }

        const char* const paramNames[] = {
          "XOrigin", "YOrigin", "XYScale",
          "ZOrigin", "ZScale",
          "MOrigin", "MScale",
          "XYTolerance", "ZTolerance", "MTolerance" };
        double* pGridValues[] = {
            &dfXOrigin, &dfYOrigin, &dfXYScale,
            &dfZOrigin, &dfZScale,
            &dfMOrigin, &dfMScale,
            &dfXYTolerance, &dfZTolerance, &dfMTolerance
        };
        static_assert(CPL_ARRAYSIZE(paramNames) == CPL_ARRAYSIZE(pGridValues),
                      "CPL_ARRAYSIZE(paramNames) == CPL_ARRAYSIZE(pGridValues)");

        /* Convert any layer creation options available, use defaults otherwise */
        for( size_t i = 0; i < CPL_ARRAYSIZE(paramNames); i++ )
        {
            const char* pszVal = m_aosCreationOptions.FetchNameValue(paramNames[i]);
            if( pszVal )
                *(pGridValues[i]) = CPLAtof(pszVal);
        }

        if( !m_poDS->GetExistingSpatialRef(osWKT,
                                           dfXOrigin,
                                           dfYOrigin,
                                           dfXYScale,
                                           dfZOrigin,
                                           dfZScale,
                                           dfMOrigin,
                                           dfMScale,
                                           dfXYTolerance,
                                           dfZTolerance,
                                           dfMTolerance) )
        {
            m_poDS->AddNewSpatialRef(osWKT,
                                     dfXOrigin,
                                     dfYOrigin,
                                     dfXYScale,
                                     dfZOrigin,
                                     dfZScale,
                                     dfMOrigin,
                                     dfMScale,
                                     dfXYTolerance,
                                     dfZTolerance,
                                     dfMTolerance);
        }
        // Will be patched later
        constexpr double dfSpatialGridResolution = 0;
        auto poGeomField = std::unique_ptr<FileGDBGeomField>(
            new FileGDBGeomField(
              m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef(),
              std::string(), // alias
              CPL_TO_BOOL(m_poFeatureDefn->GetGeomFieldDefn(0)->IsNullable()),
              osWKT,
              dfXOrigin, dfYOrigin,
              dfXYScale,
              dfXYTolerance,
              {dfSpatialGridResolution}));
        poGeomField->SetZOriginScaleTolerance(dfZOrigin, dfZScale, dfZTolerance);
        poGeomField->SetMOriginScaleTolerance(dfMOrigin, dfMScale, dfMTolerance);

        if( !m_poLyrTable->CreateField(std::move(poGeomField)) )
        {
            Close();
            return false;
        }

        m_iGeomFieldIdx = m_poLyrTable->GetGeomFieldIdx();
        m_poGeomConverter.reset(FileGDBOGRGeometryConverter::BuildConverter(
            m_poLyrTable->GetGeomField()));
    }

    const std::string osFIDName =
        m_aosCreationOptions.FetchNameValueDef("FID", "OBJECTID");
    if( !m_poLyrTable->CreateField(std::unique_ptr<FileGDBField>(
            new FileGDBField(osFIDName,
                             std::string(),
                             FGFT_OBJECTID,
                             false, 0, FileGDBField::UNSET_FIELD))) )
    {
        Close();
        return false;
    }

    const bool bCreateShapeLength =
        (eTableGeomType == FGTGT_LINE || eTableGeomType == FGTGT_POLYGON) &&
        CPLTestBool(m_aosCreationOptions.FetchNameValueDef(
            "CREATE_SHAPE_AREA_AND_LENGTH_FIELDS", "NO"));
    // Setting a non-default value doesn't work
    const char* pszLengthFieldName = m_aosCreationOptions.FetchNameValueDef(
        "LENGTH_FIELD_NAME", "Shape_Length");

    const bool bCreateShapeArea =
        eTableGeomType == FGTGT_POLYGON &&
        CPLTestBool(m_aosCreationOptions.FetchNameValueDef(
            "CREATE_SHAPE_AREA_AND_LENGTH_FIELDS", "NO"));
    // Setting a non-default value doesn't work
    const char* pszAreaFieldName = m_aosCreationOptions.FetchNameValueDef(
        "AREA_FIELD_NAME", "Shape_Area");

    if( bCreateShapeArea )
    {
        OGRFieldDefn oField(pszAreaFieldName, OFTReal);
        oField.SetDefault("FILEGEODATABASE_SHAPE_AREA");
        if( CreateField(&oField, false) != OGRERR_NONE )
        {
            Close();
            return false;
        }
    }
    if( bCreateShapeLength )
    {
        OGRFieldDefn oField(pszLengthFieldName, OFTReal);
        oField.SetDefault("FILEGEODATABASE_SHAPE_LENGTH");
        if( CreateField(&oField, false) != OGRERR_NONE )
        {
            Close();
            return false;
        }
    }

    m_poLyrTable->CreateIndex("FDO_OBJECTID", osFIDName);

    // Just to immitate the FileGDB SDK which register the index on the
    // geometry column after the OBJECTID one, but the OBJECTID column is the
    // first one in .gdbtable
    if( m_iGeomFieldIdx >= 0 )
        m_poLyrTable->CreateIndex("FDO_SHAPE", m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef());

    if( !m_poDS->RegisterLayerInSystemCatalog(m_osName) )
    {
        Close();
        return false;
    }

    if( pszFeatureDataset != nullptr &&
        m_osFeatureDatasetGUID.empty() &&
        !CreateFeatureDataset(pszFeatureDataset) )
    {
        Close();
        return false;
    }

    RefreshXMLDefinitionInMemory();

    return true;
}

/************************************************************************/
/*                       CreateXMLFieldDefinition()                     */
/************************************************************************/

static CPLXMLNode* CreateXMLFieldDefinition(const OGRFieldDefn* poFieldDefn,
                                            const FileGDBField* poGDBFieldDefn)
{
    auto GPFieldInfoEx = CPLCreateXMLNode(nullptr, CXT_Element, "GPFieldInfoEx");
    CPLAddXMLAttributeAndValue(GPFieldInfoEx, "xsi:type", "typens:GPFieldInfoEx");
    CPLCreateXMLElementAndValue(GPFieldInfoEx, "Name", poGDBFieldDefn->GetName().c_str());
    if( !poGDBFieldDefn->GetAlias().empty() )
    {
        CPLCreateXMLElementAndValue(GPFieldInfoEx, "AliasName", poGDBFieldDefn->GetAlias().c_str());
    }
    const auto* psDefault = poGDBFieldDefn->GetDefault();
    if( !OGR_RawField_IsNull(psDefault) && !OGR_RawField_IsUnset(psDefault) )
    {
        if( poGDBFieldDefn->GetType() == FGFT_STRING )
        {
            auto psDefaultValue = CPLCreateXMLElementAndValue(
                GPFieldInfoEx, "DefaultValueString", psDefault->String);
            CPLAddXMLAttributeAndValue(psDefaultValue, "xmlns:typens",
                                       "http://www.esri.com/schemas/ArcGIS/10.3");
        }
        else if( poGDBFieldDefn->GetType() == FGFT_INT32 )
        {
            auto psDefaultValue = CPLCreateXMLElementAndValue(
                GPFieldInfoEx, "DefaultValue", CPLSPrintf("%d", psDefault->Integer));
            CPLAddXMLAttributeAndValue(psDefaultValue, "xsi:type", "xs:int");
        }
        else if( poGDBFieldDefn->GetType() == FGFT_FLOAT64 )
        {
            auto psDefaultValue = CPLCreateXMLElementAndValue(
                GPFieldInfoEx, "DefaultValueNumeric", CPLSPrintf("%.18g", psDefault->Real));
            CPLAddXMLAttributeAndValue(psDefaultValue, "xmlns:typens",
                                       "http://www.esri.com/schemas/ArcGIS/10.3");
        }
    }
    const char* pszFieldType = "";
    int nLength = 0;
    switch( poGDBFieldDefn->GetType() )
    {
        case FGFT_UNDEFINED: CPLAssert(false); break;
        case FGFT_INT16:     nLength = 2; pszFieldType = "esriFieldTypeSmallInteger"; break;
        case FGFT_INT32:     nLength = 4; pszFieldType = "esriFieldTypeInteger"; break;
        case FGFT_FLOAT32:   nLength = 4; pszFieldType = "esriFieldTypeSingle"; break;
        case FGFT_FLOAT64:   nLength = 8; pszFieldType = "esriFieldTypeDouble"; break;
        case FGFT_STRING:    nLength = poGDBFieldDefn->GetMaxWidth(); pszFieldType = "esriFieldTypeString"; break;
        case FGFT_DATETIME:  nLength = 8; pszFieldType = "esriFieldTypeDate"; break;
        case FGFT_OBJECTID:  pszFieldType = "esriFieldTypeOID"; break; // shouldn't happen
        case FGFT_GEOMETRY:  pszFieldType = "esriFieldTypeGeometry"; break; // shouldn't happen
        case FGFT_BINARY:    pszFieldType = "esriFieldTypeBlob"; break;
        case FGFT_RASTER:    pszFieldType = "esriFieldTypeRaster"; break;
        case FGFT_GUID:      pszFieldType = "esriFieldTypeGUID"; break;
        case FGFT_GLOBALID:  pszFieldType = "esriFieldTypeGlobalID"; break;
        case FGFT_XML:       pszFieldType = "esriFieldTypeXML"; break;
    }
    auto psFieldType = CPLCreateXMLElementAndValue(GPFieldInfoEx, "FieldType", pszFieldType);
    CPLAddXMLAttributeAndValue(psFieldType, "xmlns:typens", "http://www.esri.com/schemas/ArcGIS/10.3");
    CPLCreateXMLElementAndValue(GPFieldInfoEx, "IsNullable", poGDBFieldDefn->IsNullable() ? "true": "false");
    CPLCreateXMLElementAndValue(GPFieldInfoEx, "Length", CPLSPrintf("%d", nLength));
    CPLCreateXMLElementAndValue(GPFieldInfoEx, "Precision", "0");
    CPLCreateXMLElementAndValue(GPFieldInfoEx, "Scale", "0");
    if( !poFieldDefn->GetDomainName().empty() )
    {
        CPLCreateXMLElementAndValue(GPFieldInfoEx, "DomainName",
                                    poFieldDefn->GetDomainName().c_str());
    }
    return GPFieldInfoEx;
}

/************************************************************************/
/*                            GetDefault()                              */
/************************************************************************/

static
bool GetDefault(const OGRFieldDefn* poField,
                FileGDBFieldType eType,
                OGRField& sDefault,
                std::string& osDefaultVal)
{
    sDefault = FileGDBField::UNSET_FIELD;
    const char* pszDefault = poField->GetDefault();
    if( pszDefault != nullptr && !poField->IsDefaultDriverSpecific() )
    {
        if( eType == FGFT_STRING )
        {
            osDefaultVal = pszDefault;
            if( osDefaultVal[0] == '\'' && osDefaultVal.back() == '\'' )
            {
                osDefaultVal = osDefaultVal.substr(1);
                osDefaultVal.resize(osDefaultVal.size()-1);
                char* pszTmp = CPLUnescapeString(osDefaultVal.c_str(), nullptr, CPLES_SQL);
                osDefaultVal = pszTmp;
                CPLFree(pszTmp);
            }
            sDefault.String = &osDefaultVal[0];
        }
        else if( eType == FGFT_INT16 || eType == FGFT_INT32 )
            sDefault.Integer = atoi(pszDefault);
        else if( eType == FGFT_FLOAT32 || eType == FGFT_FLOAT64 )
            sDefault.Real = CPLAtof(pszDefault);
        else if( eType == FGFT_DATETIME )
        {
            osDefaultVal = pszDefault;
            if( osDefaultVal[0] == '\'' && osDefaultVal.back() == '\'' )
            {
                osDefaultVal = osDefaultVal.substr(1);
                osDefaultVal.resize(osDefaultVal.size()-1);
                char* pszTmp = CPLUnescapeString(osDefaultVal.c_str(), nullptr, CPLES_SQL);
                osDefaultVal = pszTmp;
                CPLFree(pszTmp);
            }
            if( !OGRParseDate(osDefaultVal.c_str(), &sDefault, 0) )
                return false;
        }
    }
    return true;
}

/************************************************************************/
/*                         GetGDBFieldType()                            */
/************************************************************************/

static FileGDBFieldType GetGDBFieldType(const OGRFieldDefn* poField)
{
    FileGDBFieldType eType = FGFT_UNDEFINED;
    switch( poField->GetType() )
    {
        case OFTInteger:
            eType = poField->GetSubType() == OFSTInt16 ? FGFT_INT16 : FGFT_INT32;
            break;
        case OFTReal:
            eType = poField->GetSubType() == OFSTFloat32 ? FGFT_FLOAT32 : FGFT_FLOAT64;
            break;
        case OFTInteger64:
            eType = FGFT_FLOAT64;
            break;
        case OFTString:
        case OFTWideString:
        case OFTStringList:
        case OFTWideStringList:
        case OFTIntegerList:
        case OFTInteger64List:
        case OFTRealList:
            eType = FGFT_STRING;
            break;
        case OFTBinary:
            eType = FGFT_BINARY;
            break;
        case OFTDate:
        case OFTTime:
        case OFTDateTime:
            eType = FGFT_DATETIME;
            break;
    }
    return eType;
}

/************************************************************************/
/*                        GetGPFieldInfoExsNode()                       */
/************************************************************************/

static CPLXMLNode* GetGPFieldInfoExsNode(CPLXMLNode* psParent)
{
    CPLXMLNode* psInfo =
        CPLSearchXMLNode( psParent, "=DEFeatureClassInfo" );
    if( psInfo == nullptr )
        psInfo = CPLSearchXMLNode( psParent, "=typens:DEFeatureClassInfo" );
    if( psInfo == nullptr )
        psInfo = CPLSearchXMLNode( psParent, "=DETableInfo" );
    if( psInfo == nullptr )
        psInfo = CPLSearchXMLNode( psParent, "=typens:DETableInfo" );
    if( psInfo != nullptr )
    {
        return CPLGetXMLNode(psInfo, "GPFieldInfoExs");
    }
    return nullptr;
}

/************************************************************************/
/*                      GetLaunderedFieldName()                         */
/************************************************************************/

std::string OGROpenFileGDBLayer::GetLaunderedFieldName(const std::string& osNameOri) const
{
    std::wstring osName = LaunderName(StringToWString(osNameOri));
    osName = EscapeReservedKeywords(osName);

    /* Truncate to 64 characters */
    constexpr size_t FIELD_NAME_MAX_SIZE = 64;
    if (osName.size() > FIELD_NAME_MAX_SIZE)
        osName.resize(FIELD_NAME_MAX_SIZE);

    /* Ensures uniqueness of field name */
    int numRenames = 1;
    while ((m_poFeatureDefn->GetFieldIndex(WStringToString(osName).c_str()) >= 0) && (numRenames < 10))
    {
        osName = StringToWString(
            CPLSPrintf("%s_%d", WStringToString(osName.substr(0, FIELD_NAME_MAX_SIZE-2)).c_str(), numRenames));
        numRenames ++;
    }
    while ((m_poFeatureDefn->GetFieldIndex(WStringToString(osName).c_str()) >= 0) && (numRenames < 100))
    {
        osName = StringToWString(
            CPLSPrintf("%s_%d", WStringToString(osName.substr(0, FIELD_NAME_MAX_SIZE-3)).c_str(), numRenames));
        numRenames ++;
    }

    return WStringToString(osName);
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGROpenFileGDBLayer::CreateField(OGRFieldDefn* poField, int bApproxOK)
{
    if( !m_bEditable )
        return OGRERR_FAILURE;

    if( !BuildLayerDefinition() )
        return OGRERR_FAILURE;

    if( m_poDS->IsInTransaction() &&
        ((!m_bHasCreatedBackupForTransaction && !BeginEmulatedTransaction()) ||
         !m_poDS->BackupSystemTablesForTransaction()) )
    {
        return OGRERR_FAILURE;
    }

    /* Clean field names */
    OGRFieldDefn oField(poField);
    poField = &oField;

    const std::string osFieldNameOri(poField->GetNameRef());
    const std::string osFieldName = GetLaunderedFieldName(osFieldNameOri);
    if (osFieldName != osFieldNameOri)
    {
        if( !bApproxOK || (m_poFeatureDefn->GetFieldIndex(osFieldName.c_str()) >= 0) )
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                "Failed to add field named '%s'",
                osFieldNameOri.c_str() );
            return OGRERR_FAILURE;
        }
        CPLError(CE_Warning, CPLE_NotSupported,
            "Normalized/laundered field name: '%s' to '%s'",
            osFieldNameOri.c_str(), osFieldName.c_str());

        poField->SetName(osFieldName.c_str());
    }

    const char* pszColumnTypes = m_aosCreationOptions.FetchNameValue("COLUMN_TYPES");
    std::string gdbFieldType;
    if( pszColumnTypes != nullptr )
    {
        char** papszTokens = CSLTokenizeString2(pszColumnTypes, ",", 0);
        const char* pszFieldType = CSLFetchNameValue(papszTokens, poField->GetNameRef());
        if( pszFieldType != nullptr )
        {
            OGRFieldType fldtypeCheck;
            OGRFieldSubType eSubType;
            if( GDBToOGRFieldType(pszFieldType, &fldtypeCheck, &eSubType) )
            {
                if( fldtypeCheck != poField->GetType() )
                {
                    CPLError(CE_Warning, CPLE_AppDefined, "Ignoring COLUMN_TYPES=%s=%s : %s not consistent with OGR data type",
                         poField->GetNameRef(), pszFieldType, pszFieldType);
                }
                else
                    gdbFieldType = pszFieldType;
            }
            else
                CPLError(CE_Warning, CPLE_AppDefined, "Ignoring COLUMN_TYPES=%s=%s : %s not recognized",
                         poField->GetNameRef(), pszFieldType, pszFieldType);
        }
        CSLDestroy(papszTokens);
    }

    FileGDBFieldType eType = FGFT_UNDEFINED;
    if( !gdbFieldType.empty() )
    {
        if( gdbFieldType == "esriFieldTypeSmallInteger" )
            eType = FGFT_INT16;
        else if( gdbFieldType == "esriFieldTypeInteger" )
            eType = FGFT_INT32;
        else if( gdbFieldType == "esriFieldTypeSingle" )
            eType = FGFT_FLOAT32;
        else if( gdbFieldType == "esriFieldTypeDouble" )
            eType = FGFT_FLOAT64;
        else if( gdbFieldType == "esriFieldTypeString" )
            eType = FGFT_STRING;
        else if( gdbFieldType == "esriFieldTypeDate" )
            eType = FGFT_DATETIME;
        else if( gdbFieldType == "esriFieldTypeBlob" )
            eType = FGFT_BINARY;
        else if (gdbFieldType == "esriFieldTypeGUID" )
            eType = FGFT_GUID;
        else if (gdbFieldType == "esriFieldTypeGlobalID" )
            eType = FGFT_GLOBALID;
        else if (gdbFieldType == "esriFieldTypeXML" )
            eType = FGFT_XML;
        else
        {
            CPLAssert(false);
        }
    }
    else
    {
        eType = GetGDBFieldType(poField);
    }

    int nWidth = 0;
    if( eType == FGFT_GLOBALID || eType == FGFT_GUID )
    {
        nWidth = 38;
    }
    else if( poField->GetType() == OFTString )
    {
        nWidth = poField->GetWidth();
        if( nWidth == 0 )
        {
            // We can't use a 0 width value since that prevents ArcMap
            // from editing (#5952)
            nWidth = atoi(CPLGetConfigOption("OPENFILEGDB_DEFAULT_STRING_WIDTH", "65536"));
            if( nWidth < 65536 )
                poField->SetWidth(nWidth);
        }
    }

    OGRField sDefault = FileGDBField::UNSET_FIELD;
    std::string osDefaultVal;
    if( !GetDefault(poField, eType, sDefault, osDefaultVal) )
        return OGRERR_FAILURE;

    if( !poField->GetDomainName().empty() &&
        (!m_osThisGUID.empty() ||
          m_poDS->FindUUIDFromName(GetName(), m_osThisGUID)) )
    {
        if( !m_poDS->LinkDomainToTable(poField->GetDomainName(), m_osThisGUID) )
        {
            poField->SetDomainName(std::string());
        }
    }

    const char* pszAlias = poField->GetAlternativeNameRef();
    if( !m_poLyrTable->CreateField(cpl::make_unique<FileGDBField>(
            poField->GetNameRef(),
            pszAlias ? std::string(pszAlias) : std::string(),
            eType,
            CPL_TO_BOOL(poField->IsNullable()), nWidth, sDefault)) )
    {
        return OGRERR_FAILURE;
    }

    if( poField->GetType() == OFTReal )
    {
        const char* pszDefault = poField->GetDefault();
        if( pszDefault && EQUAL(pszDefault, "FILEGEODATABASE_SHAPE_AREA") )
            m_iAreaField = m_poFeatureDefn->GetFieldCount();
        else if( pszDefault && EQUAL(pszDefault, "FILEGEODATABASE_SHAPE_LENGTH") )
            m_iLengthField = m_poFeatureDefn->GetFieldCount();
    }

    m_poFeatureDefn->AddFieldDefn(poField);

    if( m_bRegisteredTable )
    {
        // If the table is already registered (that is updating an existing
        // layer), patch the XML definition to add the new field
        CPLXMLTreeCloser oTree(CPLParseXMLString(m_osDefinition.c_str()));
        if( oTree )
        {
            CPLXMLNode* psGPFieldInfoExs = GetGPFieldInfoExsNode(oTree.get());
            if( psGPFieldInfoExs )
            {
                CPLAddXMLChild(psGPFieldInfoExs,
                    CreateXMLFieldDefinition(
                        poField,
                        m_poLyrTable->GetField(m_poLyrTable->GetFieldCount()-1)));

                char* pszDefinition = CPLSerializeXMLTree(oTree.get());
                m_osDefinition = pszDefinition;
                CPLFree(pszDefinition);

                m_poDS->UpdateXMLDefinition(m_osName.c_str(),
                                            m_osDefinition.c_str());
            }
        }
    }
    else
    {
        RefreshXMLDefinitionInMemory();
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           AlterFieldDefn()                           */
/************************************************************************/

OGRErr OGROpenFileGDBLayer::AlterFieldDefn( int iFieldToAlter, OGRFieldDefn* poNewFieldDefn, int nFlagsIn )
{
    if( !m_bEditable )
        return OGRERR_FAILURE;

    if( !BuildLayerDefinition() )
        return OGRERR_FAILURE;

    if( m_poDS->IsInTransaction() &&
        ((!m_bHasCreatedBackupForTransaction && !BeginEmulatedTransaction()) ||
         !m_poDS->BackupSystemTablesForTransaction()) )
    {
        return OGRERR_FAILURE;
    }

    if (iFieldToAlter < 0 || iFieldToAlter >= m_poFeatureDefn->GetFieldCount())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid field index");
        return OGRERR_FAILURE;
    }

    const int nGDBIdx = m_poLyrTable->GetFieldIdx(
            m_poFeatureDefn->GetFieldDefn(iFieldToAlter)->GetNameRef());
    if( nGDBIdx < 0 )
        return OGRERR_FAILURE;

    OGRFieldDefn* poFieldDefn = m_poFeatureDefn->GetFieldDefn(iFieldToAlter);
    OGRFieldDefn oField(poFieldDefn);
    const std::string osOldFieldName(poFieldDefn->GetNameRef());
    const std::string osOldDomainName(std::string(poFieldDefn->GetDomainName()));
    const bool bRenamedField =
        (nFlagsIn & ALTER_NAME_FLAG) != 0 && poNewFieldDefn->GetNameRef() != osOldFieldName;

    if (nFlagsIn & ALTER_TYPE_FLAG)
    {
        if( poFieldDefn->GetType() != poNewFieldDefn->GetType() ||
            poFieldDefn->GetSubType() != poNewFieldDefn->GetSubType() )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Altering the field type is not supported");
            return OGRERR_FAILURE;
        }
    }
    if (nFlagsIn & ALTER_NAME_FLAG)
    {
        if( bRenamedField )
        {
            const std::string osFieldNameOri(poNewFieldDefn->GetNameRef());
            const std::string osFieldNameLaundered = GetLaunderedFieldName(osFieldNameOri);
            if (osFieldNameLaundered != osFieldNameOri)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid field name: %s. "
                         "A potential valid name would be: %s",
                         osFieldNameOri.c_str(),
                         osFieldNameLaundered.c_str());
                return OGRERR_FAILURE;
            }

            oField.SetName(poNewFieldDefn->GetNameRef());
        }
        oField.SetAlternativeName(poNewFieldDefn->GetAlternativeNameRef());
    }
    if (nFlagsIn & ALTER_WIDTH_PRECISION_FLAG)
    {
        oField.SetWidth(poNewFieldDefn->GetWidth());
        oField.SetPrecision(poNewFieldDefn->GetPrecision());
    }
    if (nFlagsIn & ALTER_DEFAULT_FLAG)
    {
        oField.SetDefault(poNewFieldDefn->GetDefault());
    }
    if (nFlagsIn & ALTER_NULLABLE_FLAG)
    {
        // could be potentially done, but involves .gdbtable rewriting
        if( poFieldDefn->IsNullable() != poNewFieldDefn->IsNullable() )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Altering the nullable state of a field "
                     "is not currently supported for OpenFileGDB");
            return OGRERR_FAILURE;
        }
    }
    if (nFlagsIn & ALTER_DOMAIN_FLAG)
    {
        oField.SetDomainName(poNewFieldDefn->GetDomainName());
    }

    const auto eType = GetGDBFieldType(&oField);

    int nWidth = 0;
    if( eType == FGFT_GLOBALID || eType == FGFT_GUID )
    {
        nWidth = 38;
    }
    else if( oField.GetType() == OFTString )
    {
        nWidth = oField.GetWidth();
        if( nWidth == 0 )
        {
            // Can be useful to try to replicate FileGDB driver, but do
            // not use its 65536 default value.
            nWidth = atoi(CPLGetConfigOption("OPENFILEGDB_STRING_WIDTH", "0"));
        }
    }

    OGRField sDefault = FileGDBField::UNSET_FIELD;
    std::string osDefaultVal;
    if( !GetDefault(&oField, eType, sDefault, osDefaultVal) )
        return OGRERR_FAILURE;

    const char* pszAlias = oField.GetAlternativeNameRef();
    if( !m_poLyrTable->AlterField(nGDBIdx,
            oField.GetNameRef(),
            pszAlias ? std::string(pszAlias) : std::string(),
            eType,
            CPL_TO_BOOL(oField.IsNullable()), nWidth, sDefault) )
    {
        return OGRERR_FAILURE;
    }

    poFieldDefn->SetSubType(OFSTNone);
    poFieldDefn->SetName(oField.GetNameRef());
    poFieldDefn->SetAlternativeName(oField.GetAlternativeNameRef());
    poFieldDefn->SetType(oField.GetType());
    poFieldDefn->SetSubType(oField.GetSubType());
    poFieldDefn->SetWidth(oField.GetWidth());
    poFieldDefn->SetPrecision(oField.GetPrecision());
    poFieldDefn->SetDefault(oField.GetDefault());
    poFieldDefn->SetNullable(oField.IsNullable());
    poFieldDefn->SetDomainName(oField.GetDomainName());

    if( m_bRegisteredTable )
    {
        // If the table is already registered (that is updating an existing
        // layer), patch the XML definition
        CPLXMLTreeCloser oTree(CPLParseXMLString(m_osDefinition.c_str()));
        if( oTree )
        {
            CPLXMLNode* psGPFieldInfoExs = GetGPFieldInfoExsNode(oTree.get());
            if( psGPFieldInfoExs )
            {
                CPLXMLNode* psLastChild = nullptr;
                for( CPLXMLNode* psIter = psGPFieldInfoExs->psChild; psIter; psIter = psIter->psNext )
                {
                    if( psIter->eType == CXT_Element &&
                        strcmp(psIter->pszValue, "GPFieldInfoEx") == 0 &&
                        CPLGetXMLValue(psIter, "Name", "") == osOldFieldName )
                    {
                        CPLXMLNode* psNext = psIter->psNext;
                        psIter->psNext = nullptr;
                        CPLDestroyXMLNode(psIter);
                        psIter = CreateXMLFieldDefinition(
                             poFieldDefn, m_poLyrTable->GetField(nGDBIdx));
                        psIter->psNext = psNext;
                        if( psLastChild == nullptr )
                            psGPFieldInfoExs->psChild = psIter;
                        else
                            psLastChild->psNext = psIter;
                        break;
                    }
                    psLastChild = psIter;
                }

                if( bRenamedField && m_iAreaField == iFieldToAlter )
                {
                    CPLXMLNode* psNode = CPLSearchXMLNode(oTree.get(), "=AreaFieldName");
                    if( psNode )
                    {
                        CPLSetXMLValue(psNode, "", poFieldDefn->GetNameRef());
                    }
                }
                else if( bRenamedField && m_iLengthField == iFieldToAlter )
                {
                    CPLXMLNode* psNode = CPLSearchXMLNode(oTree.get(), "=LengthFieldName");
                    if( psNode )
                    {
                        CPLSetXMLValue(psNode, "", poFieldDefn->GetNameRef());
                    }
                }

                char* pszDefinition = CPLSerializeXMLTree(oTree.get());
                m_osDefinition = pszDefinition;
                CPLFree(pszDefinition);

                m_poDS->UpdateXMLDefinition(m_osName.c_str(),
                                            m_osDefinition.c_str());
            }
        }
    }
    else
    {
        RefreshXMLDefinitionInMemory();
    }

    if( osOldDomainName != oField.GetDomainName() &&
        (!m_osThisGUID.empty() ||
         m_poDS->FindUUIDFromName(GetName(), m_osThisGUID)) )
    {
        if( osOldDomainName.empty() )
        {
            if( !m_poDS->LinkDomainToTable(oField.GetDomainName(), m_osThisGUID) )
            {
                poFieldDefn->SetDomainName(std::string());
            }
        }
        else
        {
            bool bDomainStillUsed = false;
            for( int i = 0; i < m_poFeatureDefn->GetFieldCount(); ++i )
            {
                if( m_poFeatureDefn->GetFieldDefn(i)->GetDomainName() == osOldDomainName )
                {
                    bDomainStillUsed = true;
                    break;
                }
            }
            if( !bDomainStillUsed )
            {
                m_poDS->UnlinkDomainToTable(osOldDomainName, m_osThisGUID);
            }
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                         AlterGeomFieldDefn()                         */
/************************************************************************/

OGRErr OGROpenFileGDBLayer::AlterGeomFieldDefn( int iGeomFieldToAlter,
                                                const OGRGeomFieldDefn* poNewGeomFieldDefn,
                                                int nFlagsIn )
{
    if( !m_bEditable )
        return OGRERR_FAILURE;

    if( !BuildLayerDefinition() )
        return OGRERR_FAILURE;

    if( m_poDS->IsInTransaction() &&
        ((!m_bHasCreatedBackupForTransaction && !BeginEmulatedTransaction()) ||
         !m_poDS->BackupSystemTablesForTransaction()) )
    {
        return OGRERR_FAILURE;
    }

    if (iGeomFieldToAlter < 0 || iGeomFieldToAlter >= m_poFeatureDefn->GetGeomFieldCount())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid field index");
        return OGRERR_FAILURE;
    }

    const int nGDBIdx = m_poLyrTable->GetFieldIdx(
            m_poFeatureDefn->GetGeomFieldDefn(iGeomFieldToAlter)->GetNameRef());
    if( nGDBIdx < 0 )
        return OGRERR_FAILURE;

    const auto poGeomFieldDefn = m_poFeatureDefn->GetGeomFieldDefn(iGeomFieldToAlter);
    OGRGeomFieldDefn oField(poGeomFieldDefn);

    if( (nFlagsIn & ALTER_GEOM_FIELD_DEFN_TYPE_FLAG) != 0 )
    {
        if( poGeomFieldDefn->GetType() != poNewGeomFieldDefn->GetType() )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Altering the geometry field type is not supported for "
                     "the FileGeodatabase format");
            return OGRERR_FAILURE;
        }
    }

    const std::string osOldFieldName = poGeomFieldDefn->GetNameRef();
    const bool bRenamedField =
        (nFlagsIn & ALTER_GEOM_FIELD_DEFN_NAME_FLAG) != 0 && poNewGeomFieldDefn->GetNameRef() != osOldFieldName;
    if( (nFlagsIn & ALTER_GEOM_FIELD_DEFN_NAME_FLAG) != 0 )
    {
        if( bRenamedField )
        {
            const std::string osFieldNameOri(poNewGeomFieldDefn->GetNameRef());
            const std::string osFieldNameLaundered = GetLaunderedFieldName(osFieldNameOri);
            if (osFieldNameLaundered != osFieldNameOri)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid field name: %s. "
                         "A potential valid name would be: %s",
                         osFieldNameOri.c_str(),
                         osFieldNameLaundered.c_str());
                return OGRERR_FAILURE;
            }

            oField.SetName(poNewGeomFieldDefn->GetNameRef());
        }
        // oField.SetAlternativeName(poNewGeomFieldDefn->GetAlternativeNameRef());
    }

    if( (nFlagsIn & ALTER_GEOM_FIELD_DEFN_NULLABLE_FLAG) != 0 )
    {
        // could be potentially done, but involves .gdbtable rewriting
        if( poGeomFieldDefn->IsNullable() != poNewGeomFieldDefn->IsNullable() )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Altering the nullable state of the geometry field "
                     "is not currently supported for OpenFileGDB");
            return OGRERR_FAILURE;
        }
    }

    if( (nFlagsIn & ALTER_GEOM_FIELD_DEFN_SRS_FLAG) != 0 )
    {
        const auto poOldSRS = poGeomFieldDefn->GetSpatialRef();
        const auto poNewSRS = poNewGeomFieldDefn->GetSpatialRef();

        const char* const apszOptions[] =
        {
            "IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES",
            nullptr
        };
        if( (poOldSRS == nullptr && poNewSRS != nullptr) ||
            (poOldSRS != nullptr && poNewSRS == nullptr) ||
            (poOldSRS != nullptr && poNewSRS != nullptr &&
             !poOldSRS->IsSame(poNewSRS, apszOptions)) )
        {
            if( !m_osFeatureDatasetGUID.empty() )
            {
                // Could potentially be done (would require changing the SRS
                // in all layers of the feature dataset)
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Altering the SRS of the geometry field of a layer "
                         "in a feature daaset is not currently supported "
                         "for OpenFileGDB");
                return OGRERR_FAILURE;
            }

            if( poNewSRS )
            {
                auto poNewSRSClone = poNewSRS->Clone();
                oField.SetSpatialRef(poNewSRSClone);
                poNewSRSClone->Release();
            }
            else
            {
                oField.SetSpatialRef(nullptr);
            }
        }
    }

    std::string osWKT = "{B286C06B-0879-11D2-AACA-00C04FA33C20}"; // No SRS
    if( oField.GetSpatialRef() )
    {
        const char* const apszOptions[] = { "FORMAT=WKT1_ESRI", nullptr };
        char* pszWKT;
        oField.GetSpatialRef()->exportToWkt(&pszWKT, apszOptions);
        osWKT = pszWKT;
        CPLFree(pszWKT);
    }

    if( !m_poLyrTable->AlterGeomField(
            oField.GetNameRef(),
            std::string(), // Alias
            CPL_TO_BOOL(oField.IsNullable()),
            osWKT) )
    {
        return OGRERR_FAILURE;
    }

    poGeomFieldDefn->SetName(oField.GetNameRef());
    poGeomFieldDefn->SetSpatialRef(oField.GetSpatialRef());

    if( m_bRegisteredTable )
    {
        // If the table is already registered (that is updating an existing
        // layer), patch the XML definition
        CPLXMLTreeCloser oTree(CPLParseXMLString(m_osDefinition.c_str()));
        if( oTree )
        {
            CPLXMLNode* psGPFieldInfoExs = GetGPFieldInfoExsNode(oTree.get());
            if( psGPFieldInfoExs )
            {
                for( CPLXMLNode* psIter = psGPFieldInfoExs->psChild; psIter; psIter = psIter->psNext )
                {
                    if( psIter->eType == CXT_Element &&
                        strcmp(psIter->pszValue, "GPFieldInfoEx") == 0 &&
                        CPLGetXMLValue(psIter, "Name", "") == osOldFieldName )
                    {
                        CPLXMLNode* psNode = CPLGetXMLNode(psIter, "Name");
                        if( psNode && psNode->psChild && psNode->psChild->eType == CXT_Text )
                        {
                            CPLFree(psNode->psChild->pszValue);
                            psNode->psChild->pszValue = CPLStrdup(poGeomFieldDefn->GetNameRef());
                        }
                        break;
                    }
                }

                CPLXMLNode* psNode = CPLSearchXMLNode(oTree.get(), "=ShapeFieldName");
                if( psNode )
                {
                    CPLSetXMLValue(psNode, "", poGeomFieldDefn->GetNameRef());
                }

                CPLXMLNode* psFeatureClassInfo =
                    CPLSearchXMLNode( oTree.get(), "=DEFeatureClassInfo" );
                if( psFeatureClassInfo == nullptr )
                    psFeatureClassInfo = CPLSearchXMLNode( oTree.get(), "=typens:DEFeatureClassInfo" );
                if( psFeatureClassInfo )
                {
                    psNode = CPLGetXMLNode(psFeatureClassInfo, "Extent");
                    if( psNode )
                    {
                        if( CPLRemoveXMLChild(psFeatureClassInfo, psNode) )
                            CPLDestroyXMLNode(psNode);
                    }

                    psNode = CPLGetXMLNode(psFeatureClassInfo, "SpatialReference");
                    if( psNode )
                    {
                        if( CPLRemoveXMLChild(psFeatureClassInfo, psNode) )
                            CPLDestroyXMLNode(psNode);
                    }

                    XMLSerializeGeomFieldBase(psFeatureClassInfo,
                                              m_poLyrTable->GetGeomField(), GetSpatialRef());
                }

                char* pszDefinition = CPLSerializeXMLTree(oTree.get());
                m_osDefinition = pszDefinition;
                CPLFree(pszDefinition);

                m_poDS->UpdateXMLDefinition(m_osName.c_str(),
                                            m_osDefinition.c_str());
            }
        }
    }
    else
    {
        RefreshXMLDefinitionInMemory();
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                             DeleteField()                            */
/************************************************************************/

OGRErr OGROpenFileGDBLayer::DeleteField( int iFieldToDelete )
{
    if( !m_bEditable )
        return OGRERR_FAILURE;

    if( !BuildLayerDefinition() )
        return OGRERR_FAILURE;

    if( m_poDS->IsInTransaction() &&
        ((!m_bHasCreatedBackupForTransaction && !BeginEmulatedTransaction()) ||
         !m_poDS->BackupSystemTablesForTransaction()) )
    {
        return OGRERR_FAILURE;
    }

    if (iFieldToDelete < 0 || iFieldToDelete >= m_poFeatureDefn->GetFieldCount())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid field index");
        return OGRERR_FAILURE;
    }

    const auto poFieldDefn = m_poFeatureDefn->GetFieldDefn(iFieldToDelete);
    const int nGDBIdx = m_poLyrTable->GetFieldIdx(poFieldDefn->GetNameRef());
    if( nGDBIdx < 0 )
        return OGRERR_FAILURE;
    const bool bRet = m_poLyrTable->DeleteField(nGDBIdx);
    m_iGeomFieldIdx = m_poLyrTable->GetGeomFieldIdx();

    if( !bRet )
        return OGRERR_FAILURE;

    const std::string osDeletedFieldName = poFieldDefn->GetNameRef();
    const std::string osOldDomainName = std::string(poFieldDefn->GetDomainName());

    m_poFeatureDefn->DeleteFieldDefn( iFieldToDelete );

    if( iFieldToDelete < m_iAreaField )
        m_iAreaField --;
    if( iFieldToDelete < m_iLengthField )
        m_iLengthField --;

    bool bEmptyAreaFieldName = false;
    bool bEmptyLengthFieldName = false;
    if( m_iAreaField == iFieldToDelete )
    {
        bEmptyAreaFieldName = true;
        m_iAreaField = -1;
    }
    else if( m_iLengthField == iFieldToDelete )
    {
        bEmptyLengthFieldName = true;
        m_iLengthField = -1;
    }

    if( m_bRegisteredTable )
    {
        // If the table is already registered (that is updating an existing
        // layer), patch the XML definition to add the new field
        CPLXMLTreeCloser oTree(CPLParseXMLString(m_osDefinition.c_str()));
        if( oTree )
        {
            CPLXMLNode* psGPFieldInfoExs = GetGPFieldInfoExsNode(oTree.get());
            if( psGPFieldInfoExs )
            {
                CPLXMLNode* psLastChild = nullptr;
                for( CPLXMLNode* psIter = psGPFieldInfoExs->psChild; psIter; psIter = psIter->psNext )
                {
                    if( psIter->eType == CXT_Element &&
                        strcmp(psIter->pszValue, "GPFieldInfoEx") == 0 &&
                        CPLGetXMLValue(psIter, "Name", "") == osDeletedFieldName )
                    {
                        if( psLastChild == nullptr )
                            psGPFieldInfoExs->psChild = psIter->psNext;
                        else
                            psLastChild->psNext = psIter->psNext;
                        psIter->psNext = nullptr;
                        CPLDestroyXMLNode(psIter);
                        break;
                    }
                    psLastChild = psIter;
                }

                if( bEmptyAreaFieldName )
                {
                    CPLXMLNode* psNode = CPLSearchXMLNode(oTree.get(), "=AreaFieldName");
                    if( psNode && psNode->psChild )
                    {
                        CPLDestroyXMLNode(psNode->psChild);
                        psNode->psChild = nullptr;
                    }
                }
                else if( bEmptyLengthFieldName )
                {
                    CPLXMLNode* psNode = CPLSearchXMLNode(oTree.get(), "=LengthFieldName");
                    if( psNode && psNode->psChild )
                    {
                        CPLDestroyXMLNode(psNode->psChild);
                        psNode->psChild = nullptr;
                    }
                }

                char* pszDefinition = CPLSerializeXMLTree(oTree.get());
                m_osDefinition = pszDefinition;
                CPLFree(pszDefinition);

                m_poDS->UpdateXMLDefinition(m_osName.c_str(),
                                            m_osDefinition.c_str());
            }
        }
    }
    else
    {
        RefreshXMLDefinitionInMemory();
    }

    if( !osOldDomainName.empty() )
    {
        bool bDomainStillUsed = false;
        for( int i = 0; i < m_poFeatureDefn->GetFieldCount(); ++i )
        {
            if( m_poFeatureDefn->GetFieldDefn(i)->GetDomainName() == osOldDomainName )
            {
                bDomainStillUsed = true;
                break;
            }
        }
        if( !bDomainStillUsed )
        {
            if( !m_osThisGUID.empty() ||
                m_poDS->FindUUIDFromName(GetName(), m_osThisGUID) )
            {
                m_poDS->UnlinkDomainToTable(osOldDomainName, m_osThisGUID);
            }
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                            GetLength()                               */
/************************************************************************/

static double GetLength( const OGRCurvePolygon* poPoly )
{
    double dfLength = 0;
    for( const auto* poRing: *poPoly )
        dfLength += poRing->get_Length();
    return dfLength;
}

static double GetLength( const OGRMultiSurface* poMS )
{
    double dfLength = 0;
    for( const auto* poPoly: *poMS )
    {
        auto poCurvePolygon = dynamic_cast<const OGRCurvePolygon*>(poPoly);
        if( poCurvePolygon )
            dfLength += GetLength(poCurvePolygon);
    }
    return dfLength;
}

/************************************************************************/
/*                      PrepareFileGDBFeature()                         */
/************************************************************************/

bool OGROpenFileGDBLayer::PrepareFileGDBFeature( OGRFeature *poFeature,
                                                 std::vector<OGRField>& fields,
                                                 const OGRGeometry*& poGeom )
{
    // Check geometry type
    poGeom = poFeature->GetGeometryRef();
    const auto eFlattenType = poGeom ? wkbFlatten(poGeom->getGeometryType()) : wkbNone;
    if( poGeom )
    {
        switch( m_poLyrTable->GetGeometryType() )
        {
            case FGTGT_NONE: break;

            case FGTGT_POINT:
            {
                if( eFlattenType != wkbPoint )
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Can only insert a Point in a esriGeometryPoint layer");
                    return false;
                }
                break;
            }

            case FGTGT_MULTIPOINT:
            {
                if( eFlattenType != wkbMultiPoint )
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Can only insert a MultiPoint in a esriGeometryMultiPoint layer");
                    return false;
                }
                break;
            }

            case FGTGT_LINE:
            {
                if( eFlattenType != wkbLineString && eFlattenType != wkbMultiLineString &&
                    eFlattenType != wkbCircularString && eFlattenType != wkbCompoundCurve &&
                    eFlattenType != wkbMultiCurve )
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Can only insert a LineString/MultiLineString/CircularString/CompoundCurve/MultiCurve in a esriGeometryLine layer");
                    return false;
                }
                break;
            }

            case FGTGT_POLYGON:
            {
                if( eFlattenType != wkbPolygon && eFlattenType != wkbMultiPolygon &&
                    eFlattenType != wkbCurvePolygon && eFlattenType != wkbMultiSurface )
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Can only insert a Polygon/MultiPolygon/CurvePolygon/MultiSurface in a esriGeometryPolygon layer");
                    return false;
                }
                break;
            }

            case FGTGT_MULTIPATCH:
            {
                if( eFlattenType != wkbTIN && eFlattenType != wkbPolyhedralSurface &&
                    eFlattenType != wkbGeometryCollection )
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Can only insert a TIN/PolyhedralSurface/GeometryCollection in a esriGeometryMultiPatch layer");
                    return false;
                }
                break;
            }
        }

        // Treat empty geometries as NULL, like the FileGDB driver
        if( poGeom->IsEmpty() )
            poGeom = nullptr;
    }

    if( m_iAreaField >= 0 )
    {
        const int i = m_iAreaField;
        if( poGeom != nullptr )
        {
            if( eFlattenType == wkbPolygon || eFlattenType == wkbCurvePolygon )
                poFeature->SetField(i, poGeom->toCurvePolygon()->get_Area());
            else if( eFlattenType == wkbMultiPolygon || eFlattenType == wkbMultiSurface )
                poFeature->SetField(i, poGeom->toMultiSurface()->get_Area());
            else
                poFeature->SetFieldNull(i); // shouldn't happen in nominal situation
        }
        else
        {
            poFeature->SetFieldNull(i);
        }
    }

    if( m_iLengthField >= 0 )
    {
        const int i = m_iLengthField;
        if( poGeom != nullptr )
        {
            if( OGR_GT_IsCurve(eFlattenType) )
                poFeature->SetField(i, poGeom->toCurve()->get_Length());
            else if( OGR_GT_IsSubClassOf(eFlattenType, wkbMultiCurve) )
                poFeature->SetField(i, poGeom->toMultiCurve()->get_Length());
            else if( eFlattenType == wkbPolygon || eFlattenType == wkbCurvePolygon )
                poFeature->SetField(i, GetLength(poGeom->toCurvePolygon()));
            else if( eFlattenType == wkbMultiPolygon ||  eFlattenType == wkbMultiSurface )
                poFeature->SetField(i, GetLength(poGeom->toMultiSurface()));
            else
                poFeature->SetFieldNull(i); // shouldn't happen in nominal situation
        }
        else
        {
            poFeature->SetFieldNull(i);
        }
    }

    fields.resize(m_poLyrTable->GetFieldCount(), FileGDBField::UNSET_FIELD);
    m_aosTempStrings.clear();
    for( int i = 0; i < m_poFeatureDefn->GetFieldCount(); ++i )
    {
        const auto poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
        const int idxFileGDB = m_poLyrTable->GetFieldIdx(poFieldDefn->GetNameRef());
        if( idxFileGDB < 0 )
            continue;
        if( !poFeature->IsFieldSetAndNotNull(i) )
        {
            if( m_poLyrTable->GetField(idxFileGDB)->GetType() == FGFT_GLOBALID )
            {
                m_aosTempStrings.emplace_back(OFGDBGenerateUUID());
                fields[idxFileGDB].String = &m_aosTempStrings.back()[0];
            }
            continue;
        }
        switch( m_poLyrTable->GetField(idxFileGDB)->GetType() )
        {
            case FGFT_UNDEFINED: CPLAssert(false); break;
            case FGFT_INT16: fields[idxFileGDB].Integer = poFeature->GetRawFieldRef(i)->Integer; break;
            case FGFT_INT32: fields[idxFileGDB].Integer = poFeature->GetRawFieldRef(i)->Integer; break;
            case FGFT_FLOAT32: fields[idxFileGDB].Real = poFeature->GetRawFieldRef(i)->Real; break;
            case FGFT_FLOAT64:
            {
                if( poFieldDefn->GetType() == OFTReal )
                {
                    fields[idxFileGDB].Real = poFeature->GetRawFieldRef(i)->Real;
                }
                else
                {
                    fields[idxFileGDB].Real = poFeature->GetFieldAsDouble(i);
                }
                break;
            }
            case FGFT_STRING:
            case FGFT_GUID:
            case FGFT_XML:
            {
                if( poFieldDefn->GetType() == OFTString )
                {
                    fields[idxFileGDB].String = poFeature->GetRawFieldRef(i)->String;
                }
                else
                {
                    m_aosTempStrings.emplace_back( poFeature->GetFieldAsString(i) );
                    fields[idxFileGDB].String = &m_aosTempStrings.back()[0];
                }
                break;
            }
            case FGFT_DATETIME:
            {
                fields[idxFileGDB].Date = poFeature->GetRawFieldRef(i)->Date;
                if( m_bTimeInUTC && fields[idxFileGDB].Date.TZFlag <= 1 )
                {
                    if( !m_bRegisteredTable && m_poLyrTable->GetTotalRecordCount() == 0 &&
                        m_aosCreationOptions.FetchNameValue("TIME_IN_UTC") == nullptr )
                    {
                        // If the user didn't explicitly set TIME_IN_UTC, and
                        // this is the first feature written, automatically adjust
                        // m_bTimeInUTC from the first value
                        m_bTimeInUTC = false;
                    }
                    else if( !m_bWarnedDateNotConvertibleUTC )
                    {
                        m_bWarnedDateNotConvertibleUTC = true;
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Attempt at writing a datetime with a unknown time zone "
                                 "or local time in a layer that expects dates "
                                 "to be convertible to UTC. It will be written as "
                                 "if it was expressed in UTC.");
                    }
                }
                break;
            }
            case FGFT_OBJECTID: CPLAssert(false); break; // shouldn't happen
            case FGFT_GEOMETRY: CPLAssert(false); break; // shouldn't happen
            case FGFT_RASTER: CPLAssert(false); break; // shouldn't happen
            case FGFT_BINARY: fields[idxFileGDB].Binary = poFeature->GetRawFieldRef(i)->Binary; break;
            case FGFT_GLOBALID:
            {
                if( poFeature->GetRawFieldRef(i)->String[0] != '\0' &&
                    CPLTestBool(CPLGetConfigOption("OPENFILEGDB_REGENERATE_GLOBALID", "YES")) )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Value found in a GlobalID field. It will be replaced by a "
                             "newly generated UUID.");
                }
                m_aosTempStrings.emplace_back(OFGDBGenerateUUID());
                fields[idxFileGDB].String = &m_aosTempStrings.back()[0];
                break;
            }
        }
    }

    return true;
}

/************************************************************************/
/*                           ICreateFeature()                           */
/************************************************************************/

OGRErr OGROpenFileGDBLayer::ICreateFeature( OGRFeature *poFeature )
{
    if( !m_bEditable )
        return OGRERR_FAILURE;

    if( !BuildLayerDefinition() )
        return OGRERR_FAILURE;

    if( m_poDS->IsInTransaction() && !m_bHasCreatedBackupForTransaction &&
        !BeginEmulatedTransaction() )
    {
        return OGRERR_FAILURE;
    }

    const auto nFID64Bit = poFeature->GetFID();
    if( nFID64Bit < -1 || nFID64Bit == 0 || nFID64Bit > INT_MAX )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only 32 bit positive integers FID supported by FileGDB");
        return OGRERR_FAILURE;
    }

    int nFID32Bit = (nFID64Bit > 0) ? static_cast<int>(nFID64Bit) : 0;

    poFeature->FillUnsetWithDefault(FALSE, nullptr);

    const OGRGeometry* poGeom = nullptr;
    std::vector<OGRField> fields;
    if( !PrepareFileGDBFeature( poFeature, fields, poGeom ) )
        return OGRERR_FAILURE;

    m_eSpatialIndexState = SPI_INVALID;
    m_nFilteredFeatureCount = -1;

    if( !m_poLyrTable->CreateFeature(fields, poGeom, &nFID32Bit) )
        return OGRERR_FAILURE;

    poFeature->SetFID(nFID32Bit);
    return OGRERR_NONE;
}

/************************************************************************/
/*                           ISetFeature()                              */
/************************************************************************/

OGRErr OGROpenFileGDBLayer::ISetFeature( OGRFeature *poFeature )
{
    if( !m_bEditable )
        return OGRERR_FAILURE;

    if( !BuildLayerDefinition() )
        return OGRERR_FAILURE;

    if( m_poDS->IsInTransaction() && !m_bHasCreatedBackupForTransaction &&
        !BeginEmulatedTransaction() )
    {
        return OGRERR_FAILURE;
    }

    const GIntBig nFID = poFeature->GetFID();
    if( nFID <= 0 || !CPL_INT64_FITS_ON_INT32(nFID) )
        return OGRERR_NON_EXISTING_FEATURE;

    const int nFID32Bit = static_cast<int>(nFID);
    if( nFID32Bit > m_poLyrTable->GetTotalRecordCount() )
        return OGRERR_NON_EXISTING_FEATURE;
    if( !m_poLyrTable->SelectRow(nFID32Bit - 1) )
        return OGRERR_NON_EXISTING_FEATURE;

    const OGRGeometry* poGeom = nullptr;
    std::vector<OGRField> fields;
    if( !PrepareFileGDBFeature( poFeature, fields, poGeom ) )
        return OGRERR_FAILURE;

    m_eSpatialIndexState = SPI_INVALID;
    m_nFilteredFeatureCount = -1;

    if( !m_poLyrTable->UpdateFeature(nFID32Bit, fields, poGeom) )
        return OGRERR_FAILURE;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           DeleteFeature()                            */
/************************************************************************/

OGRErr OGROpenFileGDBLayer::DeleteFeature( GIntBig nFID )

{
    if( !m_bEditable )
        return OGRERR_FAILURE;

    if( !BuildLayerDefinition() )
        return OGRERR_FAILURE;

    if( m_poDS->IsInTransaction() && !m_bHasCreatedBackupForTransaction &&
        !BeginEmulatedTransaction() )
    {
        return OGRERR_FAILURE;
    }

    if( nFID <= 0 || !CPL_INT64_FITS_ON_INT32(nFID) )
        return OGRERR_NON_EXISTING_FEATURE;

    const int nFID32Bit = static_cast<int>(nFID);
    if( nFID32Bit > m_poLyrTable->GetTotalRecordCount() )
        return OGRERR_NON_EXISTING_FEATURE;
    if( !m_poLyrTable->SelectRow(nFID32Bit - 1) )
        return OGRERR_NON_EXISTING_FEATURE;

    m_eSpatialIndexState = SPI_INVALID;
    m_nFilteredFeatureCount = -1;

    return m_poLyrTable->DeleteFeature(nFID32Bit) ? OGRERR_NONE : OGRERR_FAILURE;
}

/************************************************************************/
/*                     RefreshXMLDefinitionInMemory()                   */
/************************************************************************/

void OGROpenFileGDBLayer::RefreshXMLDefinitionInMemory()
{
    CPLXMLTreeCloser oTree(CPLCreateXMLNode(nullptr, CXT_Element, "?xml"));
    CPLAddXMLAttributeAndValue(oTree.get(), "version", "1.0");
    CPLAddXMLAttributeAndValue(oTree.get(), "encoding", "UTF-8");

    CPLXMLNode *psRoot = CPLCreateXMLNode(nullptr, CXT_Element,
      m_eGeomType == wkbNone ? "typens:DETableInfo" : "typens:DEFeatureClassInfo");
    CPLAddXMLSibling(oTree.get(), psRoot);

    CPLAddXMLAttributeAndValue(psRoot, "xmlns:typens", "http://www.esri.com/schemas/ArcGIS/10.3");
    CPLAddXMLAttributeAndValue(psRoot, "xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
    CPLAddXMLAttributeAndValue(psRoot, "xsi:type",
       m_eGeomType == wkbNone ? "typens:DETableInfo" : "typens:DEFeatureClassInfo");
    CPLCreateXMLElementAndValue(psRoot, "CatalogPath", m_osPath.c_str());
    CPLCreateXMLElementAndValue(psRoot, "Name", m_osName.c_str());
    CPLCreateXMLElementAndValue(psRoot, "ChildrenExpanded", "false");
    CPLCreateXMLElementAndValue(psRoot, "DatasetType",
        m_eGeomType == wkbNone ? "esriDTTable" : "esriDTFeatureClass");

    {
        FileGDBTable oTable;
        if( !oTable.Open(m_poDS->m_osGDBItemsFilename.c_str(), false) )
            return;
        CPLCreateXMLElementAndValue(psRoot, "DSID",
            CPLSPrintf("%d", 1 + oTable.GetTotalRecordCount()));
    }

    CPLCreateXMLElementAndValue(psRoot, "Versioned", "false");
    CPLCreateXMLElementAndValue(psRoot, "CanVersion", "false");
    if( !m_osConfigurationKeyword.empty() )
    {
        CPLCreateXMLElementAndValue(psRoot, "ConfigurationKeyword",
                                    m_osConfigurationKeyword.c_str());
    }
    CPLCreateXMLElementAndValue(psRoot, "HasOID", "true");
    CPLCreateXMLElementAndValue(psRoot, "OIDFieldName", GetFIDColumn());
    auto GPFieldInfoExs = CPLCreateXMLNode(psRoot, CXT_Element, "GPFieldInfoExs");
    CPLAddXMLAttributeAndValue(GPFieldInfoExs, "xsi:type", "typens:ArrayOfGPFieldInfoEx");

    for( int i = 0; i < m_poLyrTable->GetFieldCount(); ++i )
    {
        const auto* poGDBFieldDefn = m_poLyrTable->GetField(i);
        if( poGDBFieldDefn->GetType() == FGFT_OBJECTID )
        {
            auto GPFieldInfoEx = CPLCreateXMLNode(GPFieldInfoExs, CXT_Element, "GPFieldInfoEx");
            CPLAddXMLAttributeAndValue(GPFieldInfoEx, "xsi:type", "typens:GPFieldInfoEx");
            CPLCreateXMLElementAndValue(GPFieldInfoEx, "Name", poGDBFieldDefn->GetName().c_str());
            CPLCreateXMLElementAndValue(GPFieldInfoEx, "FieldType", "esriFieldTypeOID");
            CPLCreateXMLElementAndValue(GPFieldInfoEx, "IsNullable", "false");
            CPLCreateXMLElementAndValue(GPFieldInfoEx, "Length", "12");
            CPLCreateXMLElementAndValue(GPFieldInfoEx, "Precision", "0");
            CPLCreateXMLElementAndValue(GPFieldInfoEx, "Scale", "0");
            CPLCreateXMLElementAndValue(GPFieldInfoEx, "Required", "true");
        }
        else if( poGDBFieldDefn->GetType() == FGFT_GEOMETRY )
        {
            auto GPFieldInfoEx = CPLCreateXMLNode(GPFieldInfoExs, CXT_Element, "GPFieldInfoEx");
            CPLAddXMLAttributeAndValue(GPFieldInfoEx, "xsi:type", "typens:GPFieldInfoEx");
            CPLCreateXMLElementAndValue(GPFieldInfoEx, "Name", poGDBFieldDefn->GetName().c_str());
            CPLCreateXMLElementAndValue(GPFieldInfoEx, "FieldType", "esriFieldTypeGeometry");
            CPLCreateXMLElementAndValue(GPFieldInfoEx, "IsNullable",
                                        poGDBFieldDefn->IsNullable() ? "true" : "false");
            CPLCreateXMLElementAndValue(GPFieldInfoEx, "Length", "0");
            CPLCreateXMLElementAndValue(GPFieldInfoEx, "Precision", "0");
            CPLCreateXMLElementAndValue(GPFieldInfoEx, "Scale", "0");
            CPLCreateXMLElementAndValue(GPFieldInfoEx, "Required", "true");
        }
        else
        {
            const int nOGRIdx = m_poFeatureDefn->GetFieldIndex(
                                    poGDBFieldDefn->GetName().c_str());
            if( nOGRIdx >= 0 )
            {
                const auto poFieldDefn = m_poFeatureDefn->GetFieldDefn(nOGRIdx);
                CPLAddXMLChild(GPFieldInfoExs,
                               CreateXMLFieldDefinition(poFieldDefn, poGDBFieldDefn));
            }
        }
    }

    CPLCreateXMLElementAndValue(psRoot, "CLSID",
        m_eGeomType == wkbNone ? "{7A566981-C114-11D2-8A28-006097AFF44E}" :
                                 "{52353152-891A-11D0-BEC6-00805F7C4268}");
    CPLCreateXMLElementAndValue(psRoot, "EXTCLSID", "");

    const char* pszLayerAlias = m_aosCreationOptions.FetchNameValue("LAYER_ALIAS");
    if ( pszLayerAlias != nullptr )
    {
        CPLCreateXMLElementAndValue(psRoot, "AliasName", pszLayerAlias);
    }

    CPLCreateXMLElementAndValue(psRoot, "IsTimeInUTC",
                                m_bTimeInUTC ? "true" : " false");

    if( m_eGeomType != wkbNone )
    {
        const auto poGeomFieldDefn = m_poLyrTable->GetGeomField();
        CPLCreateXMLElementAndValue(psRoot, "FeatureType", "esriFTSimple");

        const char* pszShapeType = "";
        switch( m_poLyrTable->GetGeometryType() )
        {
            case FGTGT_NONE: break;
            case FGTGT_POINT: pszShapeType = "esriGeometryPoint"; break;
            case FGTGT_MULTIPOINT: pszShapeType = "esriGeometryMultipoint"; break;
            case FGTGT_LINE: pszShapeType = "esriGeometryLine"; break;
            case FGTGT_POLYGON: pszShapeType = "esriGeometryPolygon"; break;
            case FGTGT_MULTIPATCH: pszShapeType = "esriGeometryMultiPatch"; break;
        }
        CPLCreateXMLElementAndValue(psRoot, "ShapeType", pszShapeType);
        CPLCreateXMLElementAndValue(psRoot, "ShapeFieldName", poGeomFieldDefn->GetName().c_str());

        const bool bGeomTypeHasZ = CPL_TO_BOOL(OGR_GT_HasZ(m_eGeomType));
        const bool bGeomTypeHasM = CPL_TO_BOOL(OGR_GT_HasM(m_eGeomType));
        CPLCreateXMLElementAndValue(psRoot, "HasM", bGeomTypeHasM ? "true" : "false");
        CPLCreateXMLElementAndValue(psRoot, "HasZ", bGeomTypeHasZ ? "true" : "false");
        CPLCreateXMLElementAndValue(psRoot, "HasSpatialIndex", "false");
        const char* pszAreaFieldName = m_iAreaField >= 0 ?
             m_poFeatureDefn->GetFieldDefn(m_iAreaField)->GetNameRef() : "";
        CPLCreateXMLElementAndValue(psRoot, "AreaFieldName", pszAreaFieldName);
        const char* pszLengthFieldName = m_iLengthField >= 0 ?
             m_poFeatureDefn->GetFieldDefn(m_iLengthField)->GetNameRef() : "";
        CPLCreateXMLElementAndValue(psRoot, "LengthFieldName",pszLengthFieldName);

        XMLSerializeGeomFieldBase(psRoot, poGeomFieldDefn, GetSpatialRef());
    }

    char* pszDefinition = CPLSerializeXMLTree(oTree.get());
    m_osDefinition = pszDefinition;
    CPLFree(pszDefinition);
}

/************************************************************************/
/*                            RegisterTable()                           */
/************************************************************************/

bool OGROpenFileGDBLayer::RegisterTable()
{
    m_bRegisteredTable = true;

    CPLAssert(!m_osThisGUID.empty());

    const char* pszFeatureDataset = m_aosCreationOptions.FetchNameValue("FEATURE_DATASET");
    if( pszFeatureDataset )
    {
        if( !m_poDS->RegisterInItemRelationships(m_osFeatureDatasetGUID,
                                                 m_osThisGUID,
                                                 pszDatasetInFeatureDatasetUUID) )
        {
            return false;
        }
    }
    else
    {
        if( !m_poDS->RegisterInItemRelationships(m_poDS->m_osRootGUID,
                                                 m_osThisGUID,
                                                 // DatasetInFolder
                                                 pszDatasetInFolderUUID) )
        {
            return false;
        }
    }

    if( m_eGeomType != wkbNone )
    {
        return m_poDS->RegisterFeatureClassInItems(m_osThisGUID, m_osName,
                                                   m_osPath,
                                                   m_poLyrTable,
                                                   m_osDefinition.c_str(),
                                                   m_osDocumentation.c_str());
    }
    else
    {
        return m_poDS->RegisterASpatialTableInItems(m_osThisGUID, m_osName,
                                                    m_osPath,
                                                    m_osDefinition.c_str(),
                                                    m_osDocumentation.c_str());
    }
}

/************************************************************************/
/*                             SyncToDisk()                             */
/************************************************************************/

OGRErr OGROpenFileGDBLayer::SyncToDisk()
{
    if( !m_bEditable || m_poLyrTable == nullptr )
        return OGRERR_NONE;

    if( !m_bRegisteredTable && !RegisterTable() )
        return OGRERR_FAILURE;

    return m_poLyrTable->Sync() ? OGRERR_NONE : OGRERR_FAILURE;
}

/************************************************************************/
/*                        CreateSpatialIndex()                          */
/************************************************************************/

void OGROpenFileGDBLayer::CreateSpatialIndex()
{
    if( !m_bEditable )
        return;

    if( !BuildLayerDefinition() )
        return;

    m_poLyrTable->CreateSpatialIndex();
}

/************************************************************************/
/*                           CreateIndex()                              */
/************************************************************************/

void OGROpenFileGDBLayer::CreateIndex(const std::string& osIdxName,
                                      const std::string& osExpression)
{
    if( !m_bEditable )
        return;

    if( !BuildLayerDefinition() )
        return;

    const auto wIdxName = StringToWString(osIdxName);
    if( EscapeReservedKeywords(wIdxName) != wIdxName )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid index name: must not be a reserved keyword");
        return;
    }

    m_poLyrTable->CreateIndex(osIdxName, osExpression);
}

/************************************************************************/
/*                                Repack()                              */
/************************************************************************/

bool OGROpenFileGDBLayer::Repack()
{
    if( !m_bEditable )
        return false;

    if( !BuildLayerDefinition() )
        return false;

    return m_poLyrTable->Repack();
}

/************************************************************************/
/*                        RecomputeExtent()                             */
/************************************************************************/

void OGROpenFileGDBLayer::RecomputeExtent()
{
    if( !m_bEditable )
        return;

    if( !BuildLayerDefinition() )
        return;

    m_poLyrTable->RecomputeExtent();
}

/************************************************************************/
/*                        CheckFreeListConsistency()                    */
/************************************************************************/

bool OGROpenFileGDBLayer::CheckFreeListConsistency()
{
    if( !BuildLayerDefinition() )
        return false;

    return m_poLyrTable->CheckFreeListConsistency();
}

/************************************************************************/
/*                        BeginEmulatedTransaction()                    */
/************************************************************************/

bool OGROpenFileGDBLayer::BeginEmulatedTransaction()
{
    if( !BuildLayerDefinition() )
        return false;

    if( SyncToDisk() != OGRERR_NONE )
        return false;

    bool bRet = true;

    const std::string osThisDirname = CPLGetPath(m_osGDBFilename.c_str());
    const std::string osThisBasename = CPLGetBasename(m_osGDBFilename.c_str());
    char** papszFiles = VSIReadDir(osThisDirname.c_str());
    for( char** papszIter = papszFiles; papszIter != nullptr && *papszIter != nullptr; ++papszIter )
    {
        const std::string osBasename = CPLGetBasename(*papszIter);
        if( osBasename == osThisBasename )
        {
            std::string osDestFilename = CPLFormFilename(m_poDS->GetBackupDirName().c_str(), *papszIter, nullptr);
            std::string osSourceFilename = CPLFormFilename(osThisDirname.c_str(), *papszIter, nullptr);
            if( CPLCopyFile(osDestFilename.c_str(), osSourceFilename.c_str()) != 0 )
            {
                bRet = false;
            }
        }
    }
    CSLDestroy(papszFiles);

    m_bHasCreatedBackupForTransaction = true;

    m_poFeatureDefnBackup.reset(m_poFeatureDefn->Clone());

    return bRet;
}

/************************************************************************/
/*                        CommitEmulatedTransaction()                   */
/************************************************************************/

bool OGROpenFileGDBLayer::CommitEmulatedTransaction()
{
    m_poFeatureDefnBackup.reset();

    m_bHasCreatedBackupForTransaction = false;
    return true;
}

/************************************************************************/
/*                        RollbackEmulatedTransaction()                 */
/************************************************************************/

bool OGROpenFileGDBLayer::RollbackEmulatedTransaction()
{
    if( !m_bHasCreatedBackupForTransaction )
        return true;

    SyncToDisk();

    // Restore feature definition
    if( m_poFeatureDefnBackup != nullptr &&
        !m_poFeatureDefn->IsSame(m_poFeatureDefnBackup.get()) )
    {
        {
            const int nFieldCount = m_poFeatureDefn->GetFieldCount();
            for( int i = nFieldCount - 1; i >= 0; i-- )
                m_poFeatureDefn->DeleteFieldDefn(i);
        }
        {
            const int nFieldCount = m_poFeatureDefnBackup->GetFieldCount();
            for( int i = 0; i < nFieldCount; i++ )
                m_poFeatureDefn->AddFieldDefn( m_poFeatureDefnBackup->GetFieldDefn( i ) );
        }
    }
    m_poFeatureDefnBackup.reset();

    Close();

    bool bRet = true;

    const std::string osThisDirname = CPLGetPath(m_osGDBFilename.c_str());
    const std::string osThisBasename = CPLGetBasename(m_osGDBFilename.c_str());

    // Delete files in working directory that match our basename
    {
        char** papszFiles = VSIReadDir(osThisDirname.c_str());
        for( char** papszIter = papszFiles; papszIter != nullptr && *papszIter != nullptr; ++papszIter )
        {
            const std::string osBasename = CPLGetBasename(*papszIter);
            if( osBasename == osThisBasename )
            {
                std::string osDestFilename = CPLFormFilename(osThisDirname.c_str(), *papszIter, nullptr);
                VSIUnlink(osDestFilename.c_str());
            }
        }
        CSLDestroy(papszFiles);
    }

    // Restore backup files
    bool bBackupFound = false;
    {
        char** papszFiles = VSIReadDir(m_poDS->GetBackupDirName().c_str());
        for( char** papszIter = papszFiles; papszIter != nullptr && *papszIter != nullptr; ++papszIter )
        {
            const std::string osBasename = CPLGetBasename(*papszIter);
            if( osBasename == osThisBasename )
            {
                bBackupFound = true;
                std::string osDestFilename = CPLFormFilename(osThisDirname.c_str(), *papszIter, nullptr);
                std::string osSourceFilename = CPLFormFilename(m_poDS->GetBackupDirName().c_str(), *papszIter, nullptr);
                if( CPLCopyFile(osDestFilename.c_str(), osSourceFilename.c_str()) != 0 )
                {
                    bRet = false;
                }
            }
        }
        CSLDestroy(papszFiles);
    }

    if( bBackupFound )
    {
        m_poLyrTable = new FileGDBTable();
        if( m_poLyrTable->Open(m_osGDBFilename, m_bEditable, GetDescription()) )
        {
            if( m_iGeomFieldIdx >= 0 )
            {
                m_iGeomFieldIdx = m_poLyrTable->GetGeomFieldIdx();
                if( m_iGeomFieldIdx < 0 )
                {
                    Close();
                    bRet = false;
                }
                else
                {
                    m_bValidLayerDefn = TRUE;
                }
            }
            else
            {
                m_bValidLayerDefn = TRUE;
            }
        }
        else
        {
            Close();
            bRet = false;
        }
    }

    m_bHasCreatedBackupForTransaction = false;

    delete m_poAttributeIterator;
    m_poAttributeIterator = nullptr;

    delete m_poIterMinMax;
    m_poIterMinMax = nullptr;

    delete m_poSpatialIndexIterator;
    m_poSpatialIndexIterator = nullptr;

    delete m_poCombinedIterator;
    m_poCombinedIterator = nullptr;

    if( m_pQuadTree != nullptr )
        CPLQuadTreeDestroy(m_pQuadTree);
    m_pQuadTree = nullptr;

    CPLFree(m_pahFilteredFeatures);
    m_pahFilteredFeatures = nullptr;

    m_nFilteredFeatureCount = -1;

    m_eSpatialIndexState = SPI_INVALID;

    if( m_poLyrTable && m_iGeomFieldIdx >= 0 )
    {
        m_poGeomConverter.reset(FileGDBOGRGeometryConverter::BuildConverter(
            m_poLyrTable->GetGeomField()));
    }

    return bRet;
}

/************************************************************************/
/*                           Rename()                                   */
/************************************************************************/

OGRErr OGROpenFileGDBLayer::Rename(const char* pszDstTableName)
{
    if( !m_bEditable )
        return OGRERR_FAILURE;

    if( !BuildLayerDefinition() )
        return OGRERR_FAILURE;

    if( SyncToDisk() != OGRERR_NONE )
        return OGRERR_FAILURE;

    if( m_poDS->IsInTransaction() &&
        ((!m_bHasCreatedBackupForTransaction && !BeginEmulatedTransaction()) ||
         !m_poDS->BackupSystemTablesForTransaction()) )
    {
        return OGRERR_FAILURE;
    }

    const std::string osLaunderedName(GetLaunderedLayerName(pszDstTableName));
    if( pszDstTableName != osLaunderedName )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s is not a valid layer name. %s would be a valid one.",
                 pszDstTableName, osLaunderedName.c_str());
        return OGRERR_FAILURE;
    }

    if( m_poDS->GetLayerByName(pszDstTableName) != nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Layer %s already exists",
                 pszDstTableName);
        return OGRERR_FAILURE;
    }

    const std::string osOldName(m_osName);

    m_osName = pszDstTableName;
    SetDescription(pszDstTableName);
    m_poFeatureDefn->SetName(pszDstTableName);

    auto nLastSlashPos = m_osPath.rfind('\\');
    if( nLastSlashPos != std::string::npos )
    {
        m_osPath.resize(nLastSlashPos + 1);
    }
    else
    {
        m_osPath = '\\';
    }
    m_osPath += m_osName;

    RefreshXMLDefinitionInMemory();

    // Update GDB_SystemCatalog
    {
        FileGDBTable oTable;
        if( !oTable.Open(m_poDS->m_osGDBSystemCatalogFilename.c_str(), true) )
            return OGRERR_FAILURE;

        FETCH_FIELD_IDX_WITH_RET(iName, "Name", FGFT_STRING, OGRERR_FAILURE);

        for( int iCurFeat = 0; iCurFeat < oTable.GetTotalRecordCount(); ++iCurFeat )
        {
            iCurFeat = oTable.GetAndSelectNextNonEmptyRow(iCurFeat);
            if( iCurFeat < 0 )
                break;
            const auto psName = oTable.GetFieldValue(iName);
            if( psName && psName->String == osOldName )
            {
                auto asFields = oTable.GetAllFieldValues();

                CPLFree(asFields[iName].String);
                asFields[iName].String = CPLStrdup(m_osName.c_str());

                bool bRet = oTable.UpdateFeature(iCurFeat + 1,
                                                 asFields,
                                                 nullptr) && oTable.Sync();
                oTable.FreeAllFieldValues(asFields);
                if( !bRet )
                    return OGRERR_FAILURE;
                break;
            }
        }
    }

    // Update GDB_Items
    {
        FileGDBTable oTable;
        if( !oTable.Open(m_poDS->m_osGDBItemsFilename.c_str(), true) )
            return OGRERR_FAILURE;

        FETCH_FIELD_IDX_WITH_RET(iName, "Name", FGFT_STRING, OGRERR_FAILURE);
        FETCH_FIELD_IDX_WITH_RET(iPath, "Path", FGFT_STRING, OGRERR_FAILURE);
        FETCH_FIELD_IDX_WITH_RET(iPhysicalName, "PhysicalName", FGFT_STRING, OGRERR_FAILURE);
        FETCH_FIELD_IDX_WITH_RET(iDefinition, "Definition", FGFT_XML, OGRERR_FAILURE);

        for( int iCurFeat = 0; iCurFeat < oTable.GetTotalRecordCount(); ++iCurFeat )
        {
            iCurFeat = oTable.GetAndSelectNextNonEmptyRow(iCurFeat);
            if( iCurFeat < 0 )
                break;
            const auto psName = oTable.GetFieldValue(iName);
            if( psName && psName->String == osOldName )
            {
                auto asFields = oTable.GetAllFieldValues();

                CPLFree(asFields[iName].String);
                asFields[iName].String = CPLStrdup(m_osName.c_str());

                if( !OGR_RawField_IsNull(&asFields[iPath]) &&
                    !OGR_RawField_IsUnset(&asFields[iPath]) )
                {
                    CPLFree(asFields[iPath].String);
                }
                asFields[iPath].String = CPLStrdup(m_osPath.c_str());

                if( !OGR_RawField_IsNull(&asFields[iPhysicalName]) &&
                    !OGR_RawField_IsUnset(&asFields[iPhysicalName]) )
                {
                    CPLFree(asFields[iPhysicalName].String);
                }
                CPLString osUCName(m_osName);
                osUCName.toupper();
                asFields[iPhysicalName].String = CPLStrdup(osUCName.c_str());

                if( !OGR_RawField_IsNull(&asFields[iDefinition]) &&
                    !OGR_RawField_IsUnset(&asFields[iDefinition]) )
                {
                    CPLFree(asFields[iDefinition].String);
                }
                asFields[iDefinition].String = CPLStrdup(m_osDefinition.c_str());

                bool bRet = oTable.UpdateFeature(iCurFeat + 1,
                                                 asFields,
                                                 nullptr) && oTable.Sync();
                oTable.FreeAllFieldValues(asFields);
                if( !bRet )
                    return OGRERR_FAILURE;
                break;
            }
        }
    }

    return OGRERR_NONE;
}
