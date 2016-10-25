/******************************************************************************
 * Project:  OGR
 * Purpose:  OGRGMLASDriver implementation
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 * Initial development funded by the European Earth observation programme
 * Copernicus
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault, <even dot rouault at spatialys dot com>
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

// Must be first for DEBUG_BOOL case
#include "ogr_gmlas.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            OGRGMLASLayer()                           */
/************************************************************************/

OGRGMLASLayer::OGRGMLASLayer( OGRGMLASDataSource* poDS,
                              const GMLASFeatureClass& oFC,
                              OGRGMLASLayer* poParentLayer,
                              bool bAlwaysGenerateOGRPKId )
{
    m_poDS = poDS;
    m_oFC = oFC;
    m_bLayerDefnFinalized = false;
    m_poFeatureDefn = new OGRFeatureDefn( oFC.GetName() );
    m_poFeatureDefn->SetGeomType(wkbNone);
    m_poFeatureDefn->Reference();
    m_nIDFieldIdx = -1;
    m_bIDFieldIsGenerated = false;
    m_poParentLayer = poParentLayer;
    m_nParentIDFieldIdx = -1;
    m_poReader = NULL;
    m_bEOF = false;
    m_fpGML = NULL;

    SetDescription( m_poFeatureDefn->GetName() );

    OGRLayer* poLayersMetadataLayer = m_poDS->GetLayersMetadataLayer();
    OGRFeature* poLayerDescFeature =
                        new OGRFeature(poLayersMetadataLayer->GetLayerDefn());
    poLayerDescFeature->SetField( "layer_name", GetName() );
    if( !m_oFC.GetParentXPath().empty() )
    {
        poLayerDescFeature->SetField( "layer_category", "JUNCTION_TABLE" );
    }
    else
    {
        poLayerDescFeature->SetField( "layer_xpath", m_oFC.GetXPath() );

        poLayerDescFeature->SetField( "layer_category",
                                m_oFC.IsTopLevelElt() ? "TOP_LEVEL_ELEMENT" :
                                                        "NESTED_ELEMENT" );

        if( !m_oFC.GetDocumentation().empty() )
        {
            poLayerDescFeature->SetField( "layer_documentation",
                                          m_oFC.GetDocumentation() );
        }
    }
    CPL_IGNORE_RET_VAL(
            poLayersMetadataLayer->CreateFeature(poLayerDescFeature));
    delete poLayerDescFeature;

    // Are we a regular table ?
    if( m_oFC.GetParentXPath().empty() )
    {
        if( bAlwaysGenerateOGRPKId )
        {
            OGRFieldDefn oFieldDefn( "ogr_pkid", OFTString );
            oFieldDefn.SetNullable( false );
            m_nIDFieldIdx = m_poFeatureDefn->GetFieldCount();
            m_bIDFieldIsGenerated = true;
            m_poFeatureDefn->AddFieldDefn( &oFieldDefn );
        }

        // Determine if we have an xs:ID attribute/elt, and if it is compulsory,
        // If so, place it as first field (not strictly required, but more readable)
        // or second field (if we also add a ogr_pkid)
        // Furthermore restrict that to attributes, because otherwise it is
        // impractical in the reader when joining related features.
        const std::vector<GMLASField>& oFields = m_oFC.GetFields();
        for(int i=0; i< static_cast<int>(oFields.size()); i++ )
        {
            if( oFields[i].GetType() == GMLAS_FT_ID &&
                oFields[i].IsNotNullable() &&
                oFields[i].GetXPath().find('@') != std::string::npos)
            {
                OGRFieldDefn oFieldDefn( oFields[i].GetName(), OFTString );
                oFieldDefn.SetNullable( false );
                const int nOGRIdx = m_poFeatureDefn->GetFieldCount();
                if( m_nIDFieldIdx < 0 )
                    m_nIDFieldIdx = nOGRIdx;
                m_oMapFieldXPathToOGRFieldIdx[ oFields[i].GetXPath() ] =
                                            nOGRIdx;
                m_oMapOGRFieldIdxtoFCFieldIdx[ nOGRIdx ] = i;
                m_oMapFieldXPathToFCFieldIdx[ oFields[i].GetXPath() ] = i;
                m_poFeatureDefn->AddFieldDefn( &oFieldDefn );
                break;
            }
        }

        // If we don't have an explicit ID, then we need
        // to generate one, so that potentially related classes can reference it
        // (We could perhaps try to be clever to determine if we really need it)
        if( m_nIDFieldIdx < 0 )
        {
            OGRFieldDefn oFieldDefn( "ogr_pkid", OFTString );
            oFieldDefn.SetNullable( false );
            m_nIDFieldIdx = m_poFeatureDefn->GetFieldCount();
            m_bIDFieldIsGenerated = true;
            m_poFeatureDefn->AddFieldDefn( &oFieldDefn );
        }
    }
}

/************************************************************************/
/*                             PostInit()                               */
/************************************************************************/

void OGRGMLASLayer::PostInit( bool bIncludeGeometryXML )
{
    const std::vector<GMLASField>& oFields = m_oFC.GetFields();

    OGRLayer* poFieldsMetadataLayer = m_poDS->GetFieldsMetadataLayer();
    OGRLayer* poRelationshipsLayer = m_poDS->GetRelationshipsLayer();

    // Is it a junction table ?
    if( !m_oFC.GetParentXPath().empty() )
    {
        {
            OGRFieldDefn oFieldDefn( "occurrence", OFTInteger );
            oFieldDefn.SetNullable( false );
            m_poFeatureDefn->AddFieldDefn( &oFieldDefn );

            OGRFeature* poFieldDescFeature =
                        new OGRFeature(poFieldsMetadataLayer->GetLayerDefn());
            poFieldDescFeature->SetField( "layer_name", GetName() );
            poFieldDescFeature->SetField( "field_name", oFieldDefn.GetNameRef() );
            CPL_IGNORE_RET_VAL(
                    poFieldsMetadataLayer->CreateFeature(poFieldDescFeature));
            delete poFieldDescFeature;
        }

        {
            OGRFieldDefn oFieldDefn( "parent_pkid", OFTString );
            oFieldDefn.SetNullable( false );
            m_poFeatureDefn->AddFieldDefn( &oFieldDefn );

            OGRFeature* poFieldDescFeature =
                                new OGRFeature(poFieldsMetadataLayer->GetLayerDefn());
            poFieldDescFeature->SetField( "layer_name", GetName() );
            poFieldDescFeature->SetField( "field_name", oFieldDefn.GetNameRef() );
            CPL_IGNORE_RET_VAL(
                    poFieldsMetadataLayer->CreateFeature(poFieldDescFeature));
            delete poFieldDescFeature;
        }
        {
            OGRFieldDefn oFieldDefn( "child_pkid", OFTString );
            oFieldDefn.SetNullable( false );
            m_poFeatureDefn->AddFieldDefn( &oFieldDefn );

            OGRFeature* poFieldDescFeature =
                                new OGRFeature(poFieldsMetadataLayer->GetLayerDefn());
            poFieldDescFeature->SetField( "layer_name", GetName() );
            poFieldDescFeature->SetField( "field_name", oFieldDefn.GetNameRef() );
            CPL_IGNORE_RET_VAL(
                    poFieldsMetadataLayer->CreateFeature(poFieldDescFeature));
            delete poFieldDescFeature;
        }

        return;
    }

    // If we are a child class, then add a field to reference the parent.
    if( m_poParentLayer != NULL )
    {
        CPLString osFieldName("parent_");
        osFieldName += m_poParentLayer->GetLayerDefn()->GetFieldDefn(
                                m_poParentLayer->GetIDFieldIdx())->GetNameRef();
        OGRFieldDefn oFieldDefn( osFieldName, OFTString );
        oFieldDefn.SetNullable( false );
        m_nParentIDFieldIdx = m_poFeatureDefn->GetFieldCount();
        m_poFeatureDefn->AddFieldDefn( &oFieldDefn );
    }

    for(int i=0; i< static_cast<int>(oFields.size()); i++ )
    {
        OGRGMLASLayer* poRelatedLayer = NULL;
        const GMLASField& oField(oFields[i]);

        m_oMapFieldXPathToFCFieldIdx[ oField.GetXPath() ] = i;
        if( oField.IsIgnored() )
            continue;

        const GMLASField::Category eCategory(oField.GetCategory());
        if( !oField.GetRelatedClassXPath().empty() )
        {
            poRelatedLayer =
                    m_poDS->GetLayerByXPath(oField.GetRelatedClassXPath());
            if( poRelatedLayer != NULL )
            {
                OGRFeature* poRelationshipsFeature =
                    new OGRFeature(poRelationshipsLayer->GetLayerDefn());
                poRelationshipsFeature->SetField( "parent_layer",
                                                    GetName() );
                poRelationshipsFeature->SetField( "parent_pkid",
                        GetLayerDefn()->GetFieldDefn(
                                GetIDFieldIdx())->GetNameRef() );
                if( !oField.GetName().empty() )
                {
                    poRelationshipsFeature->SetField( "parent_element_name",
                                                    oField.GetName() );
                }
                poRelationshipsFeature->SetField( "child_layer",
                                                poRelatedLayer->GetName() );
                if( eCategory ==
                            GMLASField::PATH_TO_CHILD_ELEMENT_WITH_JUNCTION_TABLE ||
                    eCategory == GMLASField::PATH_TO_CHILD_ELEMENT_WITH_LINK )
                {
                    poRelationshipsFeature->SetField( "child_pkid",
                        poRelatedLayer->GetLayerDefn()->GetFieldDefn(
                            poRelatedLayer->GetIDFieldIdx())->GetNameRef() );
                }
                else
                {
                    CPLAssert( eCategory ==
                                    GMLASField::PATH_TO_CHILD_ELEMENT_NO_LINK ||
                               eCategory == GMLASField::GROUP);

                    poRelationshipsFeature->SetField( "child_pkid",
                        (CPLString("parent_") + GetLayerDefn()->GetFieldDefn(
                                    GetIDFieldIdx())->GetNameRef()).c_str() );
                }
                CPL_IGNORE_RET_VAL(poRelationshipsLayer->CreateFeature(
                                                poRelationshipsFeature));
                delete poRelationshipsFeature;
            }
            else
            {
                CPLDebug("GMLAS", "Cannot find class matching %s",
                        oField.GetRelatedClassXPath().c_str());
            }
        }

        OGRFeature* poFieldDescFeature =
                            new OGRFeature(poFieldsMetadataLayer->GetLayerDefn());
        poFieldDescFeature->SetField( "layer_name", GetName() );
        if( oField.GetName().empty() )
        {
            CPLAssert( eCategory == GMLASField::PATH_TO_CHILD_ELEMENT_NO_LINK ||
                       eCategory == GMLASField::GROUP );
        }
        else
        {
            poFieldDescFeature->SetField( "field_name",
                                          oField.GetName().c_str() );
        }
        if( !oField.GetXPath().empty() )
        {
            poFieldDescFeature->SetField( "field_xpath",
                                        oField.GetXPath().c_str() );
        }
        else if( !oField.GetAlternateXPaths().empty() )
        {
            CPLString osXPath;
            const std::vector<CPLString>& aoXPaths =
                                            oField.GetAlternateXPaths();
            for( size_t j=0; j<aoXPaths.size(); j++ )
            {
                if( j != 0 ) osXPath += ",";
                osXPath += aoXPaths[j];
            }
            poFieldDescFeature->SetField( "field_xpath", osXPath.c_str() );
        }
        if( oField.GetTypeName().empty() )
        {
            CPLAssert( eCategory ==
                                GMLASField::PATH_TO_CHILD_ELEMENT_NO_LINK ||
                       eCategory == GMLASField::
                                PATH_TO_CHILD_ELEMENT_WITH_JUNCTION_TABLE ||
                       eCategory == GMLASField::GROUP );
        }
        else
        {
            poFieldDescFeature->SetField( "field_type",
                                        oField.GetTypeName().c_str() );
        }
        poFieldDescFeature->SetField( "field_is_list",
                                      static_cast<int>(oField.IsList()) );
        if( oField.GetMinOccurs() != -1 )
        {
            poFieldDescFeature->SetField( "field_min_occurs",
                                        oField.GetMinOccurs() );
        }
        if( oField.GetMaxOccurs() == MAXOCCURS_UNLIMITED )
        {
            poFieldDescFeature->SetField( "field_max_occurs", INT_MAX );
        }
        else if( oField.GetMaxOccurs() != -1 )
        {
            poFieldDescFeature->SetField( "field_max_occurs",
                                        oField.GetMaxOccurs() );
        }
        if( !oField.GetFixedValue().empty() )
        {
            poFieldDescFeature->SetField( "field_fixed_value",
                                         oField.GetFixedValue() );
        }
        if( !oField.GetDefaultValue().empty() )
        {
            poFieldDescFeature->SetField( "field_default_value",
                                         oField.GetDefaultValue() );
        }
        switch( eCategory )
        {
            case GMLASField::REGULAR:
                poFieldDescFeature->SetField( "field_category",
                                             "REGULAR");
                break;
            case GMLASField::PATH_TO_CHILD_ELEMENT_NO_LINK:
                poFieldDescFeature->SetField( "field_category",
                                             "PATH_TO_CHILD_ELEMENT_NO_LINK");
                break;
            case GMLASField::PATH_TO_CHILD_ELEMENT_WITH_LINK:
                poFieldDescFeature->SetField( "field_category",
                                             "PATH_TO_CHILD_ELEMENT_WITH_LINK");
                break;
            case GMLASField::PATH_TO_CHILD_ELEMENT_WITH_JUNCTION_TABLE:
                poFieldDescFeature->SetField( "field_category",
                                "PATH_TO_CHILD_ELEMENT_WITH_JUNCTION_TABLE");
                break;
            case GMLASField::GROUP:
                poFieldDescFeature->SetField( "field_category",
                                             "GROUP");
                break;
            default:
                CPLAssert(FALSE);
                break;
        }
        if( poRelatedLayer != NULL )
        {
            poFieldDescFeature->SetField( "field_related_layer",
                                         poRelatedLayer->GetName() );
        }

        if( eCategory == GMLASField::PATH_TO_CHILD_ELEMENT_WITH_JUNCTION_TABLE)
        {
            const CPLString& osAbstractElementXPath(
                                oField.GetAbstractElementXPath());
            const CPLString& osNestedXPath(
                                oField.GetRelatedClassXPath());
            CPLAssert( !osAbstractElementXPath.empty() );
            CPLAssert( !osNestedXPath.empty() );

            OGRGMLASLayer* poJunctionLayer = m_poDS->GetLayerByXPath(
                        osAbstractElementXPath + "|" + osNestedXPath);
            if( poJunctionLayer != NULL )
            {
                poFieldDescFeature->SetField( "field_junction_layer",
                                            poJunctionLayer->GetName() );
            }
        }

        if( !oField.GetDocumentation().empty() )
        {
            poFieldDescFeature->SetField( "field_documentation",
                                          oField.GetDocumentation() );
        }

        CPL_IGNORE_RET_VAL(poFieldsMetadataLayer->CreateFeature(poFieldDescFeature));
        delete poFieldDescFeature;

        // Check whether the field is OGR instanciable
        if( eCategory == GMLASField::PATH_TO_CHILD_ELEMENT_NO_LINK ||
            eCategory == GMLASField::PATH_TO_CHILD_ELEMENT_WITH_JUNCTION_TABLE ||
            eCategory == GMLASField::GROUP )
        {
            continue;
        }

        OGRFieldType eType = OFTString;
        OGRFieldSubType eSubType = OFSTNone;
        CPLString osOGRFieldName( oField.GetName() );
        switch( oField.GetType() )
        {
            case GMLAS_FT_STRING:
                eType = OFTString;
                break;
            case GMLAS_FT_ID:
            {
                eType = OFTString;
                if( oField.IsNotNullable() )
                {
                    continue;
                }
                break;
            }
            case GMLAS_FT_BOOLEAN:
                eType = OFTInteger;
                eSubType = OFSTBoolean;
                break;
            case GMLAS_FT_SHORT:
                eType = OFTInteger;
                eSubType = OFSTInt16;
                break;
            case GMLAS_FT_INT32:
                eType = OFTInteger;
                break;
            case GMLAS_FT_INT64:
                eType = OFTInteger64;
                break;
            case GMLAS_FT_FLOAT:
                eType = OFTReal;
                eSubType = OFSTFloat32;
                break;
            case GMLAS_FT_DOUBLE:
                eType = OFTReal;
                break;
            case GMLAS_FT_DECIMAL:
                eType = OFTReal;
                break;
            case GMLAS_FT_DATE:
                eType = OFTDate;
                break;
            case GMLAS_FT_TIME:
                eType = OFTTime;
                break;
            case GMLAS_FT_DATETIME:
                eType = OFTDateTime;
                break;
            case GMLAS_FT_BASE64BINARY:
            case GMLAS_FT_HEXBINARY:
                eType = OFTBinary;
                break;
            case GMLAS_FT_ANYURI:
                eType = OFTString;
                break;
            case GMLAS_FT_ANYTYPE:
                eType = OFTString;
                break;
            case GMLAS_FT_ANYSIMPLETYPE:
                eType = OFTString;
                break;
            case GMLAS_FT_GEOMETRY:
            {
                // Create a geometry field
                OGRGeomFieldDefn oGeomFieldDefn( osOGRFieldName,
                                                 oField.GetGeomType() );
                m_poFeatureDefn->AddGeomFieldDefn( &oGeomFieldDefn );

                const int iOGRGeomIdx =
                                m_poFeatureDefn->GetGeomFieldCount() - 1;
                if( !oField.GetXPath().empty() )
                {
                    m_oMapFieldXPathToOGRGeomFieldIdx[ oField.GetXPath() ]
                            = iOGRGeomIdx ;
                }
                else
                {
                    const std::vector<CPLString>& aoXPaths =
                                        oField.GetAlternateXPaths();
                    for( size_t j=0; j<aoXPaths.size(); j++ )
                    {
                        m_oMapFieldXPathToOGRGeomFieldIdx[ aoXPaths[j] ]
                                = iOGRGeomIdx ;
                    }
                }

                m_oMapOGRGeomFieldIdxtoFCFieldIdx[ iOGRGeomIdx ] = i;

                // Suffix the regular non-geometry field
                osOGRFieldName += "_xml";
                eType = OFTString;
                break;
            }
            default:
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Unhandled type in enum: %d",
                         oField.GetType() );
                break;
        }

        if( oField.GetType() == GMLAS_FT_GEOMETRY &&
            !bIncludeGeometryXML )
        {
            continue;
        }

        if( oField.IsArray() )
        {
            switch( eType )
            {
                case OFTString: eType = OFTStringList; break;
                case OFTInteger: eType = OFTIntegerList; break;
                case OFTInteger64: eType = OFTInteger64List; break;
                case OFTReal: eType = OFTRealList; break;
                default:
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Unhandled type in enum: %d", eType );
                    break;
            }
        }
        OGRFieldDefn oFieldDefn( osOGRFieldName, eType );
        oFieldDefn.SetSubType(eSubType);
        if( oField.IsNotNullable() )
            oFieldDefn.SetNullable( false );
        CPLString osDefaultOrFixed = oField.GetDefaultValue();
        if( osDefaultOrFixed.empty() )
            osDefaultOrFixed = oField.GetFixedValue();
        if( !osDefaultOrFixed.empty() )
        {
            char* pszEscaped = CPLEscapeString(
                                        osDefaultOrFixed, -1, CPLES_SQL );
            oFieldDefn.SetDefault( (CPLString("'") +
                                        pszEscaped + CPLString("'")).c_str() );
            CPLFree(pszEscaped);
        }
        oFieldDefn.SetWidth( oField.GetWidth() );
        m_poFeatureDefn->AddFieldDefn( &oFieldDefn );

        const int iOGRIdx = m_poFeatureDefn->GetFieldCount() - 1;
        if( !oField.GetXPath().empty() )
        {
            m_oMapFieldXPathToOGRFieldIdx[ oField.GetXPath() ] = iOGRIdx ;
        }
        else
        {
            const std::vector<CPLString>& aoXPaths = oField.GetAlternateXPaths();
            for( size_t j=0; j<aoXPaths.size(); j++ )
            {
                m_oMapFieldXPathToOGRFieldIdx[ aoXPaths[j] ] = iOGRIdx ;
            }
        }

        m_oMapOGRFieldIdxtoFCFieldIdx[iOGRIdx] = i;

        // Create field to receive resolved xlink:href content, if needed
        if( oField.GetXPath().find("@xlink:href") != std::string::npos &&
            m_poDS->GetConf().m_oXLinkResolution.m_bDefaultResolutionEnabled &&
            m_poDS->GetConf().m_oXLinkResolution.m_eDefaultResolutionMode
                                        == GMLASXLinkResolutionConf::RawContent )
        {
            CPLString osRawContentFieldname(osOGRFieldName);
            size_t nPos = osRawContentFieldname.find("_href");
            if( nPos != std::string::npos )
                osRawContentFieldname.resize(nPos);
            osRawContentFieldname += "_rawcontent";
            OGRFieldDefn oFieldDefnRaw( osRawContentFieldname, OFTString );
            m_poFeatureDefn->AddFieldDefn( &oFieldDefnRaw );

            m_oMapFieldXPathToOGRFieldIdx[
                GMLASField::MakeXLinkRawContentFieldXPathFromXLinkHrefXPath(
                    oField.GetXPath()) ] = m_poFeatureDefn->GetFieldCount() - 1;
        }
    }

    CreateCompoundFoldedMappings();
}

/************************************************************************/
/*                   CreateCompoundFoldedMappings()                     */
/************************************************************************/

// In the case we have nested elements but we managed to fold into top
// level class, then register intermediate paths so they are not reported
// as unexpected in debug traces
void OGRGMLASLayer::CreateCompoundFoldedMappings()
{
    CPLString oFCXPath(m_oFC.GetXPath());
    if( m_oFC.IsRepeatedSequence() )
    {
        size_t iPosExtra = oFCXPath.find(";extra=");
        if (iPosExtra != std::string::npos)
        {
            oFCXPath.resize(iPosExtra);
        }
    }

    const std::vector<GMLASField>& oFields = m_oFC.GetFields();
    for(size_t i=0; i<oFields.size(); i++ )
    {
        std::vector<CPLString> aoXPaths = oFields[i].GetAlternateXPaths();
        if( aoXPaths.empty() )
            aoXPaths.push_back(oFields[i].GetXPath());
        for( size_t j=0; j<aoXPaths.size(); j++ )
        {
            if( aoXPaths[j].size() > oFCXPath.size() )
            {
                // Split on both '/' and '@'
                char** papszTokens = CSLTokenizeString2(
                    aoXPaths[j].c_str() + oFCXPath.size() + 1,
                    "/@", 0 );
                CPLString osSubXPath = oFCXPath;
                for(int k=0; papszTokens[k] != NULL &&
                            papszTokens[k+1] != NULL; k++)
                {
                    osSubXPath += "/";
                    osSubXPath += papszTokens[k];
                    if( m_oMapFieldXPathToOGRFieldIdx.find( osSubXPath ) ==
                                                m_oMapFieldXPathToOGRFieldIdx.end() )
                    {
                        m_oMapFieldXPathToOGRFieldIdx[ osSubXPath ] =
                                                            IDX_COMPOUND_FOLDED;
                    }
                }
                CSLDestroy(papszTokens);
            }
        }
    }
}

/************************************************************************/
/*                           ~OGRGMLASLayer()                           */
/************************************************************************/

OGRGMLASLayer::~OGRGMLASLayer()
{
    m_poFeatureDefn->Release();
    delete m_poReader;
    if( m_fpGML != NULL )
        VSIFCloseL(m_fpGML);
}

/************************************************************************/
/*                            RemoveField()                             */
/************************************************************************/

bool OGRGMLASLayer::RemoveField( int nIdx )
{
    if( nIdx == m_nIDFieldIdx || nIdx == m_nParentIDFieldIdx )
        return false;

    m_poFeatureDefn->DeleteFieldDefn( nIdx );

    // Refresh maps
    {
        std::map<CPLString, int>       oMapFieldXPathToOGRFieldIdx;
        std::map<CPLString, int>::const_iterator oIter =
                            m_oMapFieldXPathToOGRFieldIdx.begin();
        for( ; oIter != m_oMapFieldXPathToOGRFieldIdx.end(); ++oIter )
        {
            if( oIter->second < nIdx )
                oMapFieldXPathToOGRFieldIdx[oIter->first] = oIter->second;
            else if( oIter->second > nIdx )
                oMapFieldXPathToOGRFieldIdx[oIter->first] = oIter->second - 1;
        }
        m_oMapFieldXPathToOGRFieldIdx = oMapFieldXPathToOGRFieldIdx;
    }

    {
        std::map<int, int>             oMapOGRFieldIdxtoFCFieldIdx;
        std::map<int, int>::const_iterator oIter =
                            m_oMapOGRFieldIdxtoFCFieldIdx.begin();
        for( ; oIter != m_oMapOGRFieldIdxtoFCFieldIdx.end(); ++oIter )
        {
            if( oIter->first < nIdx )
                oMapOGRFieldIdxtoFCFieldIdx[oIter->first] = oIter->second;
            else if( oIter->first > nIdx )
                oMapOGRFieldIdxtoFCFieldIdx[oIter->first - 1] = oIter->second;
        }
        m_oMapOGRFieldIdxtoFCFieldIdx = oMapOGRFieldIdxtoFCFieldIdx;
    }

    return true;
}

/************************************************************************/
/*                            InsertNewField()                          */
/************************************************************************/

void OGRGMLASLayer::InsertNewField( int nInsertPos,
                                    OGRFieldDefn& oFieldDefn,
                                    const CPLString& osXPath )
{
    CPLAssert( nInsertPos >= 0 && nInsertPos <= m_poFeatureDefn->GetFieldCount() );
    m_poFeatureDefn->AddFieldDefn( &oFieldDefn );
    int* panMap = new int[ m_poFeatureDefn->GetFieldCount() ];
    for( int i = 0; i < nInsertPos; ++i )
    {
        panMap[i] = i;
    }
    panMap[nInsertPos] = m_poFeatureDefn->GetFieldCount() - 1;
    for( int i = nInsertPos + 1; i <  m_poFeatureDefn->GetFieldCount(); ++i )
    {
        panMap[i] = i - 1;
    }
    m_poFeatureDefn->ReorderFieldDefns( panMap );
    delete[] panMap;

    // Refresh maps
    {
        std::map<CPLString, int>       oMapFieldXPathToOGRFieldIdx;
        std::map<CPLString, int>::const_iterator oIter =
                            m_oMapFieldXPathToOGRFieldIdx.begin();
        for( ; oIter != m_oMapFieldXPathToOGRFieldIdx.end(); ++oIter )
        {
            if( oIter->second < nInsertPos )
                oMapFieldXPathToOGRFieldIdx[oIter->first] = oIter->second;
            else
                oMapFieldXPathToOGRFieldIdx[oIter->first] = oIter->second + 1;
        }
        m_oMapFieldXPathToOGRFieldIdx = oMapFieldXPathToOGRFieldIdx;
        m_oMapFieldXPathToOGRFieldIdx[ osXPath ] = nInsertPos;
    }

    {
        std::map<int, int>             oMapOGRFieldIdxtoFCFieldIdx;
        std::map<int, int>::const_iterator oIter =
                            m_oMapOGRFieldIdxtoFCFieldIdx.begin();
        for( ; oIter != m_oMapOGRFieldIdxtoFCFieldIdx.end(); ++oIter )
        {
            if( oIter->first < nInsertPos )
                oMapOGRFieldIdxtoFCFieldIdx[oIter->first] = oIter->second;
            else
                oMapOGRFieldIdxtoFCFieldIdx[oIter->first + 1] = oIter->second;
        }
        m_oMapOGRFieldIdxtoFCFieldIdx = oMapOGRFieldIdxtoFCFieldIdx;
    }
}

/************************************************************************/
/*                       GetOGRFieldIndexFromXPath()                    */
/************************************************************************/

int OGRGMLASLayer::GetOGRFieldIndexFromXPath(const CPLString& osXPath) const
{
    std::map<CPLString, int>::const_iterator oIter =
        m_oMapFieldXPathToOGRFieldIdx.find(osXPath);
    if( oIter == m_oMapFieldXPathToOGRFieldIdx.end() )
        return -1;
    return oIter->second;
}

/************************************************************************/
/*                      GetOGRGeomFieldIndexFromXPath()                 */
/************************************************************************/

int OGRGMLASLayer::GetOGRGeomFieldIndexFromXPath(const CPLString& osXPath) const
{
    std::map<CPLString, int>::const_iterator oIter =
        m_oMapFieldXPathToOGRGeomFieldIdx.find(osXPath);
    if( oIter == m_oMapFieldXPathToOGRGeomFieldIdx.end() )
        return -1;
    return oIter->second;
}

/************************************************************************/
/*                     GetFCFieldIndexFromOGRFieldIdx()                 */
/************************************************************************/

int OGRGMLASLayer::GetFCFieldIndexFromOGRFieldIdx(int iOGRFieldIdx) const
{
    std::map<int, int>::const_iterator oIter =
        m_oMapOGRFieldIdxtoFCFieldIdx.find(iOGRFieldIdx);
    if( oIter == m_oMapOGRFieldIdxtoFCFieldIdx.end() )
        return -1;
    return oIter->second;
}

/************************************************************************/
/*                     GetFCFieldIndexFromXPath()                       */
/************************************************************************/

int OGRGMLASLayer::GetFCFieldIndexFromXPath(const CPLString& osXPath) const
{
    std::map<CPLString, int>::const_iterator oIter =
        m_oMapFieldXPathToFCFieldIdx.find(osXPath);
    if( oIter == m_oMapFieldXPathToFCFieldIdx.end() )
        return -1;
    return oIter->second;
}

/************************************************************************/
/*                  GetFCFieldIndexFromOGRGeomFieldIdx()                */
/************************************************************************/

int OGRGMLASLayer::GetFCFieldIndexFromOGRGeomFieldIdx(int iOGRGeomFieldIdx) const
{
    std::map<int, int>::const_iterator oIter =
        m_oMapOGRGeomFieldIdxtoFCFieldIdx.find(iOGRGeomFieldIdx);
    if( oIter == m_oMapOGRGeomFieldIdxtoFCFieldIdx.end() )
        return -1;
    return oIter->second;
}

/************************************************************************/
/*                              GetLayerDefn()                          */
/************************************************************************/

OGRFeatureDefn* OGRGMLASLayer::GetLayerDefn()
{
    if( !m_bLayerDefnFinalized && m_poDS->IsLayerInitFinished() )
    {
        // If we haven't yet determined the SRS of geometry columns, do it now
        m_bLayerDefnFinalized = true;
        if( m_poFeatureDefn->GetGeomFieldCount() > 0 ||
            !m_poDS->GetConf().m_oXLinkResolution.m_aoURLSpecificRules.empty() )
        {
            if( m_poReader == NULL )
                InitReader();
        }
    }
    return m_poFeatureDefn;
}

/************************************************************************/
/*                              ResetReading()                          */
/************************************************************************/

void OGRGMLASLayer::ResetReading()
{
    delete m_poReader;
    m_poReader = NULL;
    m_bEOF = false;
}

/************************************************************************/
/*                              InitReader()                            */
/************************************************************************/

bool OGRGMLASLayer::InitReader()
{
    CPLAssert( m_poReader == NULL );

    m_poReader = m_poDS->CreateReader( m_fpGML );
    m_bLayerDefnFinalized = true;
    if( m_poReader != NULL )
    {
        m_poReader->SetLayerOfInterest( this );
        return true;
    }
    return false;
}

/************************************************************************/
/*                          GetNextRawFeature()                         */
/************************************************************************/

OGRFeature* OGRGMLASLayer::GetNextRawFeature()
{
    if( m_poReader == NULL && !InitReader() )
        return NULL;

    return m_poReader->GetNextFeature();
}

/************************************************************************/
/*                            EvaluateFilter()                          */
/************************************************************************/

bool OGRGMLASLayer::EvaluateFilter( OGRFeature* poFeature )
{
    return (m_poFilterGeom == NULL
             || FilterGeometry( poFeature->GetGeomFieldRef(m_iGeomFieldFilter) ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature ));
}

/************************************************************************/
/*                            GetNextFeature()                          */
/************************************************************************/

OGRFeature* OGRGMLASLayer::GetNextFeature()
{
    if( m_bEOF )
        return NULL;

    while( true )
    {
        OGRFeature *poFeature = GetNextRawFeature();
        if( poFeature == NULL )
        {
            // Avoid keeping too many file descriptor opened
            if( m_fpGML != NULL )
                m_poDS->PushUnusedGMLFilePointer(m_fpGML);
            m_fpGML = NULL;
            delete m_poReader;
            m_poReader = NULL;
            m_bEOF = true;
            return NULL;
        }

        if( EvaluateFilter(poFeature) )
        {
            return poFeature;
        }

        delete poFeature;
    }
}
