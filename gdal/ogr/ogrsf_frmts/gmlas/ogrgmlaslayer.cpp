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
                              bool bAlwaysGenerateOGRPKId ) :
    m_poDS( poDS ),
    m_oFC( oFC ),
    m_bLayerDefnFinalized( false ),
    m_poFeatureDefn( new OGRFeatureDefn( oFC.GetName() ) ),
    m_bEOF( false ),
    m_poReader( NULL ),
    m_fpGML( NULL ),
    m_nIDFieldIdx( -1 ),
    m_bIDFieldIsGenerated( false ),
    m_poParentLayer( poParentLayer ),
    m_nParentIDFieldIdx( -1 )

{
    m_poFeatureDefn->SetGeomType(wkbNone);
    m_poFeatureDefn->Reference();

    SetDescription( m_poFeatureDefn->GetName() );

    // Are we a regular table ?
    if( m_oFC.GetParentXPath().empty() )
    {
        if( bAlwaysGenerateOGRPKId )
        {
            OGRFieldDefn oFieldDefn( szOGR_PKID, OFTString );
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
            OGRFieldDefn oFieldDefn( szOGR_PKID, OFTString );
            oFieldDefn.SetNullable( false );
            m_nIDFieldIdx = m_poFeatureDefn->GetFieldCount();
            m_bIDFieldIsGenerated = true;
            m_poFeatureDefn->AddFieldDefn( &oFieldDefn );
        }
    }

    OGRLayer* poLayersMetadataLayer = m_poDS->GetLayersMetadataLayer();
    OGRFeature* poLayerDescFeature =
                        new OGRFeature(poLayersMetadataLayer->GetLayerDefn());
    poLayerDescFeature->SetField( szLAYER_NAME, GetName() );
    if( !m_oFC.GetParentXPath().empty() )
    {
        poLayerDescFeature->SetField( szLAYER_CATEGORY, szJUNCTION_TABLE );
    }
    else
    {
        poLayerDescFeature->SetField( szLAYER_XPATH, m_oFC.GetXPath() );

        poLayerDescFeature->SetField( szLAYER_CATEGORY,
                                m_oFC.IsTopLevelElt() ? szTOP_LEVEL_ELEMENT :
                                                        szNESTED_ELEMENT );

        if( m_nIDFieldIdx >= 0 )
        {
            poLayerDescFeature->SetField( szLAYER_PKID_NAME,
                m_poFeatureDefn->GetFieldDefn(m_nIDFieldIdx)->GetNameRef() );
        }

        // If we are a child class, then add a field to reference the parent.
        if( m_poParentLayer != NULL )
        {
            CPLString osFieldName(szPARENT_PREFIX);
            osFieldName += m_poParentLayer->GetLayerDefn()->GetFieldDefn(
                                    m_poParentLayer->GetIDFieldIdx())->GetNameRef();
            poLayerDescFeature->SetField( szLAYER_PARENT_PKID_NAME,
                                          osFieldName.c_str() );
        }

        if( !m_oFC.GetDocumentation().empty() )
        {
            poLayerDescFeature->SetField( szLAYER_DOCUMENTATION,
                                          m_oFC.GetDocumentation() );
        }
    }
    CPL_IGNORE_RET_VAL(
            poLayersMetadataLayer->CreateFeature(poLayerDescFeature));
    delete poLayerDescFeature;

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
            OGRFieldDefn oFieldDefn( szOCCURRENCE, OFTInteger );
            oFieldDefn.SetNullable( false );
            m_poFeatureDefn->AddFieldDefn( &oFieldDefn );

            OGRFeature* poFieldDescFeature =
                        new OGRFeature(poFieldsMetadataLayer->GetLayerDefn());
            poFieldDescFeature->SetField( szLAYER_NAME, GetName() );
            poFieldDescFeature->SetField( szFIELD_NAME, oFieldDefn.GetNameRef() );
            CPL_IGNORE_RET_VAL(
                    poFieldsMetadataLayer->CreateFeature(poFieldDescFeature));
            delete poFieldDescFeature;
        }

        {
            OGRFieldDefn oFieldDefn( szPARENT_PKID, OFTString );
            oFieldDefn.SetNullable( false );
            m_poFeatureDefn->AddFieldDefn( &oFieldDefn );

            OGRFeature* poFieldDescFeature =
                                new OGRFeature(poFieldsMetadataLayer->GetLayerDefn());
            poFieldDescFeature->SetField( szLAYER_NAME, GetName() );
            poFieldDescFeature->SetField( szFIELD_NAME, oFieldDefn.GetNameRef() );
            CPL_IGNORE_RET_VAL(
                    poFieldsMetadataLayer->CreateFeature(poFieldDescFeature));
            delete poFieldDescFeature;
        }
        {
            OGRFieldDefn oFieldDefn( szCHILD_PKID, OFTString );
            oFieldDefn.SetNullable( false );
            m_poFeatureDefn->AddFieldDefn( &oFieldDefn );

            OGRFeature* poFieldDescFeature =
                                new OGRFeature(poFieldsMetadataLayer->GetLayerDefn());
            poFieldDescFeature->SetField( szLAYER_NAME, GetName() );
            poFieldDescFeature->SetField( szFIELD_NAME, oFieldDefn.GetNameRef() );
            CPL_IGNORE_RET_VAL(
                    poFieldsMetadataLayer->CreateFeature(poFieldDescFeature));
            delete poFieldDescFeature;
        }

        return;
    }

    // If we are a child class, then add a field to reference the parent.
    if( m_poParentLayer != NULL )
    {
        CPLString osFieldName(szPARENT_PREFIX);
        osFieldName += m_poParentLayer->GetLayerDefn()->GetFieldDefn(
                                m_poParentLayer->GetIDFieldIdx())->GetNameRef();
        OGRFieldDefn oFieldDefn( osFieldName, OFTString );
        oFieldDefn.SetNullable( false );
        m_nParentIDFieldIdx = m_poFeatureDefn->GetFieldCount();
        m_poFeatureDefn->AddFieldDefn( &oFieldDefn );
    }

    int nFieldIndex = 0;
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
                poRelationshipsFeature->SetField( szPARENT_LAYER, GetName() );
                poRelationshipsFeature->SetField( szPARENT_PKID,
                        GetLayerDefn()->GetFieldDefn(
                                GetIDFieldIdx())->GetNameRef() );
                if( !oField.GetName().empty() )
                {
                    poRelationshipsFeature->SetField( szPARENT_ELEMENT_NAME,
                                                      oField.GetName() );
                }
                poRelationshipsFeature->SetField(szCHILD_LAYER,
                                                 poRelatedLayer->GetName() );
                if( eCategory ==
                            GMLASField::PATH_TO_CHILD_ELEMENT_WITH_JUNCTION_TABLE ||
                    eCategory == GMLASField::PATH_TO_CHILD_ELEMENT_WITH_LINK )
                {
                    poRelationshipsFeature->SetField( szCHILD_PKID,
                        poRelatedLayer->GetLayerDefn()->GetFieldDefn(
                            poRelatedLayer->GetIDFieldIdx())->GetNameRef() );
                }
                else
                {
                    CPLAssert( eCategory ==
                                    GMLASField::PATH_TO_CHILD_ELEMENT_NO_LINK ||
                               eCategory == GMLASField::GROUP);

                    poRelationshipsFeature->SetField( szCHILD_PKID,
                        (CPLString(szPARENT_PREFIX) +
                            GetLayerDefn()->GetFieldDefn(
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
        poFieldDescFeature->SetField( szLAYER_NAME, GetName() );

        ++nFieldIndex;
        poFieldDescFeature->SetField( szFIELD_INDEX, nFieldIndex);

        if( oField.GetName().empty() )
        {
            CPLAssert( eCategory == GMLASField::PATH_TO_CHILD_ELEMENT_NO_LINK ||
                       eCategory == GMLASField::GROUP );
        }
        else
        {
            poFieldDescFeature->SetField( szFIELD_NAME,
                                          oField.GetName().c_str() );
        }
        if( !oField.GetXPath().empty() )
        {
            poFieldDescFeature->SetField( szFIELD_XPATH,
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
            poFieldDescFeature->SetField( szFIELD_XPATH, osXPath.c_str() );
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
            poFieldDescFeature->SetField( szFIELD_TYPE,
                                        oField.GetTypeName().c_str() );
        }
        poFieldDescFeature->SetField( szFIELD_IS_LIST,
                                      static_cast<int>(oField.IsList()) );
        if( oField.GetMinOccurs() != -1 )
        {
            poFieldDescFeature->SetField( szFIELD_MIN_OCCURS,
                                        oField.GetMinOccurs() );
        }
        if( oField.GetMaxOccurs() == MAXOCCURS_UNLIMITED )
        {
            poFieldDescFeature->SetField( szFIELD_MAX_OCCURS, INT_MAX );
        }
        else if( oField.GetMaxOccurs() != -1 )
        {
            poFieldDescFeature->SetField( szFIELD_MAX_OCCURS,
                                        oField.GetMaxOccurs() );
        }
        if( oField.GetMaxOccurs() == MAXOCCURS_UNLIMITED ||
            oField.GetMaxOccurs() > 1 )
        {
            poFieldDescFeature->SetField( szFIELD_REPETITION_ON_SEQUENCE,
                                oField.GetRepetitionOnSequence() ? 1 : 0);
        }
        if( !oField.GetFixedValue().empty() )
        {
            poFieldDescFeature->SetField( szFIELD_FIXED_VALUE,
                                          oField.GetFixedValue() );
        }
        if( !oField.GetDefaultValue().empty() )
        {
            poFieldDescFeature->SetField( szFIELD_DEFAULT_VALUE,
                                          oField.GetDefaultValue() );
        }
        switch( eCategory )
        {
            case GMLASField::REGULAR:
                poFieldDescFeature->SetField(szFIELD_CATEGORY,
                                             szREGULAR);
                break;
            case GMLASField::PATH_TO_CHILD_ELEMENT_NO_LINK:
                poFieldDescFeature->SetField(szFIELD_CATEGORY,
                                             szPATH_TO_CHILD_ELEMENT_NO_LINK);
                break;
            case GMLASField::PATH_TO_CHILD_ELEMENT_WITH_LINK:
                poFieldDescFeature->SetField(szFIELD_CATEGORY,
                                             szPATH_TO_CHILD_ELEMENT_WITH_LINK);
                break;
            case GMLASField::PATH_TO_CHILD_ELEMENT_WITH_JUNCTION_TABLE:
                poFieldDescFeature->SetField(szFIELD_CATEGORY,
                                szPATH_TO_CHILD_ELEMENT_WITH_JUNCTION_TABLE);
                break;
            case GMLASField::GROUP:
                poFieldDescFeature->SetField(szFIELD_CATEGORY,
                                             szGROUP);
                break;
            default:
                CPLAssert(FALSE);
                break;
        }
        if( poRelatedLayer != NULL )
        {
            poFieldDescFeature->SetField( szFIELD_RELATED_LAYER,
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
                poFieldDescFeature->SetField( szFIELD_JUNCTION_LAYER,
                                              poJunctionLayer->GetName() );
            }
        }

        if( !oField.GetDocumentation().empty() )
        {
            poFieldDescFeature->SetField( szFIELD_DOCUMENTATION,
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
                osOGRFieldName += szXML_SUFFIX;
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
        if( oField.GetXPath().find(szAT_XLINK_HREF) != std::string::npos &&
            m_poDS->GetConf().m_oXLinkResolution.m_bDefaultResolutionEnabled &&
            m_poDS->GetConf().m_oXLinkResolution.m_eDefaultResolutionMode
                                        == GMLASXLinkResolutionConf::RawContent )
        {
            CPLString osRawContentFieldname(osOGRFieldName);
            size_t nPos = osRawContentFieldname.find(szHREF_SUFFIX);
            if( nPos != std::string::npos )
                osRawContentFieldname.resize(nPos);
            osRawContentFieldname += szRAW_CONTENT_SUFFIX;
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
        size_t iPosExtra = oFCXPath.find(szEXTRA_SUFFIX);
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
