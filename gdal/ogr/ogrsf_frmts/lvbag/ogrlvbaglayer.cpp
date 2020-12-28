/******************************************************************************
 *
 * Project:  LV BAG Translator
 * Purpose:  Implements OGRLVBAGLayer.
 * Author:   Laixer B.V., info at laixer dot com
 *
 ******************************************************************************
 * Copyright (c) 2020, Laixer B.V. <info at laixer dot com>
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
#include "ogr_geos.h"
#include "ogr_lvbag.h"
#include "ogr_p.h"

constexpr const char *pszSpecificationUrn = "urn:ogc:def:crs:EPSG::28992";
constexpr const size_t nDefaultIdentifierSize = 16;

/************************************************************************/
/*                           OGRLVBAGLayer()                            */
/*                                                                      */
/*      Note that the OGRLVBAGLayer assumes ownership of the passed     */
/*      file pointer.                                                   */
/************************************************************************/

OGRLVBAGLayer::OGRLVBAGLayer( const char *pszFilename, OGRLayerPool* poPoolIn, char **papszOpenOptions ) :
    OGRAbstractProxiedLayer{ poPoolIn },
    poFeatureDefn{ new OGRFeatureDefn{} },
    fp{ nullptr },
    nNextFID{ 0 },
    osFilename{ pszFilename },
    eFileDescriptorsState{ FD_CLOSED },
    oParser{ nullptr },
    bSchemaOnly{ false },
    bHasReadSchema{ false },
    bFixInvalidData{ CPLFetchBool(papszOpenOptions, "AUTOCORRECT_INVALID_DATA", false) },
    bLegacyId{ CPLFetchBool(papszOpenOptions, "LEGACY_ID", false) },
    nCurrentDepth{ 0 },
    nGeometryElementDepth{ 0 },
    nFeatureCollectionDepth{ 0 },
    nFeatureElementDepth{ 0 },
    nAttributeElementDepth{ 0 },
    bCollectData{ false }
{
    SetDescription(CPLGetBasename(pszFilename));

    poFeatureDefn->Reference();

    memset(aBuf, '\0', sizeof(aBuf));
}

/************************************************************************/
/*                           ~OGRLVBAGLayer()                           */
/************************************************************************/

OGRLVBAGLayer::~OGRLVBAGLayer()
{
    delete m_poFeature;
    poFeatureDefn->Release();
    CloseUnderlyingLayer();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRLVBAGLayer::ResetReading()
{
    if( !TouchLayer() )
        return;

    VSIRewindL(fp);

    nNextFID = 0;
    nCurrentDepth = 0;
    nGeometryElementDepth = 0;
    nFeatureCollectionDepth = 0;
    nFeatureElementDepth = 0;
    nAttributeElementDepth = 0;
    bCollectData = false;
}

/************************************************************************/
/*                            GetLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn* OGRLVBAGLayer::GetLayerDefn()
{
    if( !TouchLayer() )
        return nullptr;

    if ( !bHasReadSchema )
    {
        bSchemaOnly = true;

        ConfigureParser();
        ParseDocument();
    }

    return poFeatureDefn;
}

/************************************************************************/
/*                            XMLTagSplit()                             */
/************************************************************************/

static inline const char* XMLTagSplit( const char *pszName )
{
    const char *pszTag = pszName;
    const char *pszSep = strchr(pszTag, ':');
    if( pszSep )
        pszTag = pszSep + 1;

    return pszTag;
}

/************************************************************************/
/*                           AddSpatialRef()                            */
/************************************************************************/

void OGRLVBAGLayer::AddSpatialRef( OGRwkbGeometryType eTypeIn )
{
    OGRGeomFieldDefn *poGeomField = poFeatureDefn->GetGeomFieldDefn(0);
    OGRSpatialReference* poSRS = new OGRSpatialReference();
    poSRS->importFromURN(pszSpecificationUrn);
    poGeomField->SetSpatialRef(poSRS);
    poGeomField->SetType(eTypeIn);
    poSRS->Release();
}

/************************************************************************/
/*                      AddIdentifierFieldDefn()                        */
/************************************************************************/

void OGRLVBAGLayer::AddIdentifierFieldDefn()
{
    OGRFieldDefn oField0("identificatie", OFTString);

    poFeatureDefn->AddFieldDefn(&oField0);
}

/************************************************************************/
/*                       AddDocumentFieldDefn()                         */
/************************************************************************/

void OGRLVBAGLayer::AddDocumentFieldDefn()
{
    OGRFieldDefn oField0("status", OFTString);
    OGRFieldDefn oField1("geconstateerd", OFTInteger);
    oField1.SetSubType(OFSTBoolean);
    OGRFieldDefn oField2("documentDatum", OFTDate);
    OGRFieldDefn oField3("documentNummer", OFTString);

    poFeatureDefn->AddFieldDefn(&oField0);
    poFeatureDefn->AddFieldDefn(&oField1);
    poFeatureDefn->AddFieldDefn(&oField2);
    poFeatureDefn->AddFieldDefn(&oField3);
}

/************************************************************************/
/*                      AddOccurrenceFieldDefn()                        */
/************************************************************************/

void OGRLVBAGLayer::AddOccurrenceFieldDefn()
{
    OGRFieldDefn oField0("voorkomenIdentificatie", OFTInteger);
    OGRFieldDefn oField1("beginGeldigheid", OFTDate);
    OGRFieldDefn oField2("eindGeldigheid", OFTDate);
    OGRFieldDefn oField3("tijdstipRegistratie", OFTDateTime);
    OGRFieldDefn oField4("eindRegistratie", OFTDateTime);
    OGRFieldDefn oField5("tijdstipInactief", OFTDateTime);
    OGRFieldDefn oField6("tijdstipRegistratieLV", OFTDateTime);
    OGRFieldDefn oField7("tijdstipEindRegistratieLV", OFTDateTime);
    OGRFieldDefn oField8("tijdstipInactiefLV", OFTDateTime);
    OGRFieldDefn oField9("tijdstipNietBagLV", OFTDateTime);

    poFeatureDefn->AddFieldDefn(&oField0);
    poFeatureDefn->AddFieldDefn(&oField1);
    poFeatureDefn->AddFieldDefn(&oField2);
    poFeatureDefn->AddFieldDefn(&oField3);
    poFeatureDefn->AddFieldDefn(&oField4);
    poFeatureDefn->AddFieldDefn(&oField5);
    poFeatureDefn->AddFieldDefn(&oField6);
    poFeatureDefn->AddFieldDefn(&oField7);
    poFeatureDefn->AddFieldDefn(&oField8);
    poFeatureDefn->AddFieldDefn(&oField9);
}

/************************************************************************/
/*                         CreateFeatureDefn()                          */
/************************************************************************/

void OGRLVBAGLayer::CreateFeatureDefn( const char *pszDataset )
{
    if( EQUAL("pnd", pszDataset) )
    {
        OGRFieldDefn oField0("oorspronkelijkBouwjaar", OFTInteger);

        poFeatureDefn->AddFieldDefn(&oField0);
        
        AddIdentifierFieldDefn();
        AddDocumentFieldDefn();
        AddOccurrenceFieldDefn();

        poFeatureDefn->SetName("Pand");
        SetDescription(poFeatureDefn->GetName());

        AddSpatialRef(wkbMultiPolygon);
    }
    else if( EQUAL("num", pszDataset) )
    {
        OGRFieldDefn oField0("huisnummer", OFTInteger);
        OGRFieldDefn oField1("huisletter", OFTString);
        OGRFieldDefn oField2("huisnummerToevoeging", OFTString);
        OGRFieldDefn oField3("postcode", OFTString);
        OGRFieldDefn oField4("typeAdresseerbaarObject", OFTString);
        OGRFieldDefn oField5("openbareruimteRef", OFTString);
  
        poFeatureDefn->AddFieldDefn(&oField0);
        poFeatureDefn->AddFieldDefn(&oField1);
        poFeatureDefn->AddFieldDefn(&oField2);
        poFeatureDefn->AddFieldDefn(&oField3);
        poFeatureDefn->AddFieldDefn(&oField4);
        poFeatureDefn->AddFieldDefn(&oField5);
 
        AddIdentifierFieldDefn();
        AddDocumentFieldDefn();
        AddOccurrenceFieldDefn();

        poFeatureDefn->SetName("Nummeraanduiding");
        SetDescription(poFeatureDefn->GetName());
    }
    else if( EQUAL("lig", pszDataset) )
    {
        OGRFieldDefn oField0("nummeraanduidingRef", OFTString);

        poFeatureDefn->AddFieldDefn(&oField0);

        AddIdentifierFieldDefn();
        AddDocumentFieldDefn();
        AddOccurrenceFieldDefn();

        poFeatureDefn->SetName("Ligplaats");
        SetDescription(poFeatureDefn->GetName());

        AddSpatialRef(wkbPolygon);
    }
    else if( EQUAL("sta", pszDataset) )
    {
        OGRFieldDefn oField0("nummeraanduidingRef", OFTString);

        poFeatureDefn->AddFieldDefn(&oField0);

        AddIdentifierFieldDefn();
        AddDocumentFieldDefn();
        AddOccurrenceFieldDefn();

        poFeatureDefn->SetName("Standplaats");
        SetDescription(poFeatureDefn->GetName());

        AddSpatialRef(wkbPolygon);
    }
    else if( EQUAL("opr", pszDataset) )
    {
        OGRFieldDefn oField0("naam", OFTString);
        OGRFieldDefn oField1("type", OFTString);
        OGRFieldDefn oField2("woonplaatsRef", OFTString);

        poFeatureDefn->AddFieldDefn(&oField0);
        poFeatureDefn->AddFieldDefn(&oField1);
        poFeatureDefn->AddFieldDefn(&oField2);
 
        AddIdentifierFieldDefn();
        AddDocumentFieldDefn();
        AddOccurrenceFieldDefn();

        poFeatureDefn->SetName("Openbareruimte");
        SetDescription(poFeatureDefn->GetName());
    }
    else if( EQUAL("vbo", pszDataset) )
    {
        OGRFieldDefn oField0("gebruiksdoel", OFTString);
        OGRFieldDefn oField1("oppervlakte", OFTInteger);
        OGRFieldDefn oField2("nummeraanduidingRef", OFTString);
        OGRFieldDefn oField3("pandRef", OFTString);

        poFeatureDefn->AddFieldDefn(&oField0);
        poFeatureDefn->AddFieldDefn(&oField1);
        poFeatureDefn->AddFieldDefn(&oField2);
        poFeatureDefn->AddFieldDefn(&oField3);
 
        AddIdentifierFieldDefn();
        AddDocumentFieldDefn();
        AddOccurrenceFieldDefn();

        poFeatureDefn->SetName("Verblijfsobject");
        SetDescription(poFeatureDefn->GetName());

        AddSpatialRef(wkbPoint);
    }
    else if( EQUAL("wpl", pszDataset) )
    {
        OGRFieldDefn oField0("naam", OFTString);
  
        poFeatureDefn->AddFieldDefn(&oField0);
 
        AddIdentifierFieldDefn();
        AddDocumentFieldDefn();
        AddOccurrenceFieldDefn();

        poFeatureDefn->SetName("Woonplaats");
        SetDescription(poFeatureDefn->GetName());

        AddSpatialRef(wkbMultiPolygon);
    }
    else
        CPLError(CE_Failure, CPLE_AppDefined,
            "Parsing LV BAG extract failed : invalid layer definition");
}

/************************************************************************/
/*                         StartDataCollect()                           */
/************************************************************************/

void OGRLVBAGLayer::StartDataCollect()
{
    osElementString.Clear();
    osAttributeString.Clear();
    bCollectData = true;
}

/************************************************************************/
/*                         StopDataCollect()                            */
/************************************************************************/

void OGRLVBAGLayer::StopDataCollect()
{
    bCollectData = false;
    osElementString.Trim();
    osAttributeString.Trim();
}

/************************************************************************/
/*                           DataHandlerCbk()                           */
/************************************************************************/

void OGRLVBAGLayer::DataHandlerCbk( const char *data, int nLen )
{
    if( nLen && bCollectData )
        osElementString.append(data, nLen);
}

/************************************************************************/
/*                              TouchLayer()                            */
/************************************************************************/

bool OGRLVBAGLayer::TouchLayer()
{
    poPool->SetLastUsedLayer(this);

    switch( eFileDescriptorsState )
    {
        case FD_OPENED:
            return true;
        case FD_CANNOT_REOPEN:
            return false;
        case FD_CLOSED:
            break;
    }

    fp = VSIFOpenExL(osFilename, "rb", true);
    if( !fp )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
            "Opening LV BAG extract failed : %s", osFilename.c_str());
        eFileDescriptorsState = FD_CANNOT_REOPEN;
        return false;
    }
    
    eFileDescriptorsState = FD_OPENED;

    return true;
}

/************************************************************************/
/*                        CloseUnderlyingLayer()                        */
/************************************************************************/

void OGRLVBAGLayer::CloseUnderlyingLayer()
{
    if ( fp )
        VSIFCloseL(fp);
    fp = nullptr;

    eFileDescriptorsState = FD_CLOSED;
}

/************************************************************************/
/*                        startElementCbk()                            */
/************************************************************************/

void OGRLVBAGLayer::StartElementCbk( const char *pszName, const char **ppszAttr )
{
    if( nFeatureElementDepth > 0 && nAttributeElementDepth > 0 &&
        nGeometryElementDepth == 0 && EQUAL("objecten:geometrie", pszName) )
    {
        nGeometryElementDepth = nCurrentDepth;
        StartDataCollect();
    }
    else if ( nFeatureElementDepth > 0 && nAttributeElementDepth > 0
        && nGeometryElementDepth + 1 == nCurrentDepth && !STARTS_WITH_CI(pszName, "gml") )
    {
        nGeometryElementDepth = nCurrentDepth;
        StartDataCollect();
    }
    else if( nFeatureElementDepth > 0 && nAttributeElementDepth == 0 &&
        nGeometryElementDepth == 0 && STARTS_WITH_CI(pszName, "objecten") )
        nAttributeElementDepth = nCurrentDepth;
    else if( nFeatureElementDepth > 0 && nAttributeElementDepth > 0 &&
             nGeometryElementDepth == 0 &&
             ( EQUAL("objecten:identificatie", pszName) || STARTS_WITH_CI(pszName, "objecten-ref") ) )
    {
        StartDataCollect();
        const char** papszIter = ppszAttr;
        while( papszIter && *papszIter != nullptr )
        {
            if( EQUAL("domein", papszIter[0]) )
            {
                osAttributeString = papszIter[1];
                break;
            }
            papszIter += 2;
        }
    }
    else if( nFeatureElementDepth > 0 && nAttributeElementDepth > 0 &&
             nGeometryElementDepth == 0 )
        StartDataCollect();
    else if( nGeometryElementDepth > 0 && STARTS_WITH_CI(pszName, "gml") )
    {
        osElementString += "<";
        osElementString += pszName;

        const char** papszIter = ppszAttr;
        while( papszIter && *papszIter != nullptr )
        {
            OGRGeomFieldDefn *poGeomField = poFeatureDefn->GetGeomFieldDefn(0);
            if( EQUAL("srsname", papszIter[0]) && poGeomField->GetSpatialRef() == nullptr )
            {
                OGRSpatialReference* poSRS = new OGRSpatialReference();
                poSRS->importFromURN(papszIter[1]);
                poGeomField->SetSpatialRef(poSRS);
                poSRS->Release();
            }

            osElementString += " ";
            osElementString += papszIter[0];
            osElementString += "=\"";
            osElementString += papszIter[1];
            osElementString += "\"";
            papszIter += 2;
        }

        osElementString += ">";
    }
    else if( nFeatureCollectionDepth > 0 && nFeatureElementDepth == 0 &&
             EQUAL("sl-bag-extract:bagObject", pszName) &&
             bHasReadSchema )
    {
        nFeatureElementDepth = nCurrentDepth;
        m_poFeature = new OGRFeature(poFeatureDefn);
        m_poFeature->SetFID(nNextFID++);
    }
    else if( nFeatureCollectionDepth == 0 && EQUAL("sl:standBestand", pszName) )
        nFeatureCollectionDepth = nCurrentDepth;
    else if( nFeatureCollectionDepth > 0 && EQUAL("sl:objectType", pszName) )
        StartDataCollect();

    nCurrentDepth++;
}

/************************************************************************/
/*                           endElementCbk()                            */
/************************************************************************/

void OGRLVBAGLayer::EndElementCbk( const char *pszName )
{
    nCurrentDepth--;

    if( nCurrentDepth > nAttributeElementDepth
        && nAttributeElementDepth > 0
        && nGeometryElementDepth == 0 )
    {
        const char *pszTag = XMLTagSplit(pszName);
        
        StopDataCollect();
        if ( !osElementString.empty() )
        {
            const int iFieldIndex = poFeatureDefn->GetFieldIndex(pszTag);
            if( iFieldIndex > -1 )
            {
                const char *pszValue = osElementString.c_str();
                const size_t nIdLength = osElementString.size();
                const OGRFieldDefn *poFieldDefn = poFeatureDefn->GetFieldDefn(iFieldIndex);
                if( EQUAL("identificatie", pszTag) || STARTS_WITH_CI(pszName, "objecten-ref") )
                {
                    bool bIsIdInvalid = false;
                    if( nIdLength == nDefaultIdentifierSize-1 )
                    {
                        osElementString = '0' + osElementString;
                    }
                    else if( nIdLength > nDefaultIdentifierSize )
                    {
                        bIsIdInvalid = true;
                        m_poFeature->SetFieldNull(iFieldIndex);
                        CPLError(CE_Warning, CPLE_AppDefined, 
                            "Invalid identificatie : %s, value set to null", pszValue);
                    }
                    if ( !bIsIdInvalid )
                    {
                        if ( !bLegacyId && !osAttributeString.empty() )
                        {
                            osElementString = osAttributeString + '.' + osElementString;
                        }
                        m_poFeature->SetField(iFieldIndex, osElementString.c_str());
                    }
                }
                else if( poFieldDefn->GetSubType() == OGRFieldSubType::OFSTBoolean )
                {
                    if( EQUAL("n", pszValue) )
                        m_poFeature->SetField(iFieldIndex, 0);
                    else if( EQUAL("j", pszValue) )
                        m_poFeature->SetField(iFieldIndex, 1);
                    else
                    {
                        CPLError(CE_Failure, CPLE_AppDefined, "Parsing boolean failed");
                        XML_StopParser(oParser.get(), XML_FALSE);
                    }
                }
                else
                    m_poFeature->SetField(iFieldIndex, pszValue);
                
                if( bFixInvalidData
                    && (poFieldDefn->GetType() == OFTDate || poFieldDefn->GetType() == OFTDateTime) )
                {
                    int nYear;
                    m_poFeature->GetFieldAsDateTime(iFieldIndex, &nYear, nullptr, nullptr,
                                                nullptr, nullptr,
                                                static_cast<float*>(nullptr), nullptr);
                    if( nYear > 2100 )
                    {
                        m_poFeature->SetFieldNull(iFieldIndex);
                        CPLError(CE_Warning, CPLE_AppDefined, 
                            "Invalid date : %s, value set to null", pszValue);
                    }
                }
            }
            osElementString.Clear();
        }
    }
    else if( nAttributeElementDepth == nCurrentDepth )
        nAttributeElementDepth = 0;
    else if( nGeometryElementDepth > 0 && nCurrentDepth > nGeometryElementDepth )
    {
        osElementString += "</";
        osElementString += pszName;
        osElementString += ">";
    }
    else if( nGeometryElementDepth == nCurrentDepth )
    {
        StopDataCollect();
        if( !osElementString.empty() )
        {
            std::unique_ptr<OGRGeometry> poGeom = std::unique_ptr<OGRGeometry>{
                reinterpret_cast<OGRGeometry *>(OGR_G_CreateFromGML(osElementString.c_str())) };
            if( poGeom && !poGeom->IsEmpty() )
            {
                // The specification only accounts for 2-dimensional datasets
                if( poGeom->Is3D() )
                    poGeom->flattenTo2D();

// GEOS >= 3.8.0 for MakeValid.
#ifdef HAVE_GEOS
#if GEOS_VERSION_MAJOR > 3 || (GEOS_VERSION_MAJOR == 3 && GEOS_VERSION_MINOR >= 8)
                if( !poGeom->IsValid() && bFixInvalidData )
                {
                    std::unique_ptr<OGRGeometry> poSubGeom = std::unique_ptr<OGRGeometry>{
                        poGeom->MakeValid() };
                    if( poSubGeom && poSubGeom->IsValid() )
                        poGeom.reset(poSubGeom.release());
                }
#endif
#endif

                OGRGeomFieldDefn *poGeomField = poFeatureDefn->GetGeomFieldDefn(0);
                if( !poGeomField->GetSpatialRef() )
                    poGeomField->SetSpatialRef(poGeom->getSpatialReference());
                if( poGeomField->GetType() == wkbUnknown )
                    poGeomField->SetType(poGeom->getGeometryType());

                if( poGeomField->GetType() == wkbPoint )
                {
                    switch( poGeom->getGeometryType() )
                    {
                        case wkbPolygon:
                        case wkbMultiPolygon:
                        {
                            std::unique_ptr<OGRPoint> poPoint = std::unique_ptr<OGRPoint>{ new OGRPoint };
#ifdef HAVE_GEOS
                            if( poGeom->Centroid(poPoint.get()) == OGRERR_NONE )
                                poGeom.reset(poPoint.release());
#else
                            CPLError( CE_Warning, CPLE_AppDefined,
                                "Cannot shape geometry, GEOS support not enabled." );
                            poGeom.reset(poPoint.release());
#endif
                            break;
                        }

                        default:
                            break;
                    }
                }
                else if( poGeomField->GetType() == wkbMultiPolygon
                    && poGeom->getGeometryType() == wkbPolygon )
                {
                    std::unique_ptr<OGRMultiPolygon> poMultiPolygon = std::unique_ptr<OGRMultiPolygon>{ new OGRMultiPolygon };
                    poMultiPolygon->addGeometry(poGeom.get());
                    poGeom.reset(poMultiPolygon.release());
                }
                else if( poGeomField->GetType() == wkbMultiPolygon
                    && poGeom->getGeometryType() == wkbGeometryCollection
                    && poGeom->toGeometryCollection()->getNumGeometries() > 0
                    && poGeom->toGeometryCollection()->getGeometryRef(0)->getGeometryType() == wkbPolygon )
                {
                    std::unique_ptr<OGRMultiPolygon> poMultiPolygon = std::unique_ptr<OGRMultiPolygon>{ new OGRMultiPolygon };
                    for (auto &poChildGeom : poGeom->toGeometryCollection())
                        poMultiPolygon->addGeometry(poChildGeom);
                    poGeom.reset(poMultiPolygon.release());
                }

                if( poGeomField->GetSpatialRef() )
                    poGeom->assignSpatialReference(poGeomField->GetSpatialRef());
                m_poFeature->SetGeometryDirectly(poGeom.release());
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Parsing geometry as GML failed");
                XML_StopParser(oParser.get(), XML_FALSE);
            }
        }

        osElementString.Clear();
        osAttributeString.Clear();
        nGeometryElementDepth = 0;
    }
    else if( nFeatureElementDepth == nCurrentDepth )
    {
        nFeatureElementDepth = 0;
        XML_StopParser(oParser.get(), XML_TRUE);
    }
    else if( nFeatureCollectionDepth == nCurrentDepth )
        nFeatureCollectionDepth = 0;
    else if( EQUAL("sl:objecttype", pszName) && !poFeatureDefn->GetFieldCount() )
    {
        StopDataCollect();
        if ( osElementString.empty() )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Parsing LV BAG extract failed");
            XML_StopParser(oParser.get(), XML_FALSE);
        }
        
        if( !bHasReadSchema )
            CreateFeatureDefn(osElementString.c_str());
        bHasReadSchema = true;

        // The parser is suspended but never resumed. Stop
        // without resume indicated an error.
        if( bSchemaOnly )
            XML_StopParser(oParser.get(), XML_TRUE);
    }
}

/************************************************************************/
/*                          ConfigureParser()                           */
/************************************************************************/

void OGRLVBAGLayer::ConfigureParser()
{
    ResetReading();

    const auto startElementWrapper = [](void *pUserData, const char *pszName, const char **ppszAttr)
    {
        static_cast<OGRLVBAGLayer *>(pUserData)->StartElementCbk(pszName, ppszAttr);
    };

    const auto endElementWrapper = [](void *pUserData, const char *pszName)
    {
        static_cast<OGRLVBAGLayer *>(pUserData)->EndElementCbk(pszName);
    };

    const auto dataHandlerWrapper = [](void *pUserData, const XML_Char *data, int nLen)
    {
        static_cast<OGRLVBAGLayer *>(pUserData)->DataHandlerCbk(data, nLen);
    };

    oParser = OGRExpatUniquePtr{ OGRCreateExpatXMLParser() };
    XML_SetElementHandler(oParser.get(), startElementWrapper, endElementWrapper);
    XML_SetCharacterDataHandler(oParser.get(), dataHandlerWrapper);
    XML_SetUserData(oParser.get(), this);
}

/************************************************************************/
/*                         IsParserFinished()                           */
/************************************************************************/

bool OGRLVBAGLayer::IsParserFinished( XML_Status status )
{
    switch (status)
    {
        case XML_STATUS_OK:
            return false;
        
        case XML_STATUS_ERROR:
            CPLError( CE_Failure, CPLE_AppDefined,
                    "Parsing of LV BAG file failed : %s at line %d, "
                    "column %d",
                    XML_ErrorString(XML_GetErrorCode(oParser.get())),
                    static_cast<int>(XML_GetCurrentLineNumber(oParser.get())),
                    static_cast<int>(XML_GetCurrentColumnNumber(oParser.get())) );

            delete m_poFeature;
            m_poFeature = nullptr;
            return true;

        case XML_STATUS_SUSPENDED:
            return true;
    }

    return true;
}

/************************************************************************/
/*                           ParseDocument()                            */
/************************************************************************/

void OGRLVBAGLayer::ParseDocument()
{
    while( true )
    {
        XML_ParsingStatus status;
        XML_GetParsingStatus(oParser.get(), &status);
        switch (status.parsing)
        {
            case XML_INITIALIZED:
            case XML_PARSING:
            {
                memset(aBuf, '\0', sizeof(aBuf));
                const unsigned int nLen = static_cast<unsigned int>(VSIFReadL(aBuf, 1, sizeof(aBuf), fp));

                if( IsParserFinished(XML_Parse(oParser.get(), aBuf, nLen, VSIFEofL(fp))) )
                    return;

                break;
            }
            
            case XML_SUSPENDED:
            {
                if( IsParserFinished(XML_ResumeParser(oParser.get())) )
                    return;

                break;
            }
            
            case XML_FINISHED:
            default:
                return;
        }
    }
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature* OGRLVBAGLayer::GetNextFeature()
{
    if( !TouchLayer() )
        return nullptr;

    if ( !bHasReadSchema )
    {
        GetLayerDefn();
        if ( !bHasReadSchema )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                "Parsing LV BAG extract failed : invalid layer definition");
            return nullptr;
        }
    }

    return OGRGetNextFeatureThroughRaw<OGRLVBAGLayer>::GetNextFeature();
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature* OGRLVBAGLayer::GetNextRawFeature()
{
    bSchemaOnly = false;

    if (nNextFID == 0)
        ConfigureParser();

    if ( m_poFeature )
    {
        delete m_poFeature;
        m_poFeature = nullptr;
    }

    ParseDocument();
    OGRFeature* poFeatureRet = m_poFeature;
    m_poFeature = nullptr;
    return poFeatureRet;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRLVBAGLayer::TestCapability( const char *pszCap )
{
    if( !TouchLayer() )
        return FALSE;

    if( EQUAL(pszCap, OLCStringsAsUTF8) )
        return TRUE;
    
    return FALSE;
}
