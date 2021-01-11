/******************************************************************************
 *
 * Project:  WFS Translator
 * Purpose:  Implements OGRWFSLayer class.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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
#include "ogr_wfs.h"
#include "ogr_api.h"
#include "cpl_minixml.h"
#include "cpl_http.h"
#include "parsexsd.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                      OGRWFSRecursiveUnlink()                         */
/************************************************************************/

void OGRWFSRecursiveUnlink( const char *pszName )

{
    char **papszFileList = VSIReadDir( pszName );

    for( int i = 0; papszFileList != nullptr && papszFileList[i] != nullptr; i++ )
    {
        VSIStatBufL  sStatBuf;

        if( EQUAL(papszFileList[i],".") || EQUAL(papszFileList[i],"..") )
            continue;

        CPLString osFullFilename =
                 CPLFormFilename( pszName, papszFileList[i], nullptr );

        if( VSIStatL( osFullFilename, &sStatBuf ) == 0 )
        {
            if( VSI_ISREG( sStatBuf.st_mode ) )
            {
                VSIUnlink( osFullFilename );
            }
            else if( VSI_ISDIR( sStatBuf.st_mode ) )
            {
                OGRWFSRecursiveUnlink( osFullFilename );
            }
        }
    }

    CSLDestroy( papszFileList );

    VSIRmdir( pszName );
}

/************************************************************************/
/*                            OGRWFSLayer()                             */
/************************************************************************/

OGRWFSLayer::OGRWFSLayer( OGRWFSDataSource* poDSIn,
                          OGRSpatialReference* poSRSIn,
                          int bAxisOrderAlreadyInvertedIn,
                          const char* pszBaseURLIn,
                          const char* pszNameIn,
                          const char* pszNSIn,
                          const char* pszNSValIn ) :
    poDS(poDSIn),
    poFeatureDefn(nullptr),
    bGotApproximateLayerDefn(false),
    poGMLFeatureClass(nullptr),
    bAxisOrderAlreadyInverted(bAxisOrderAlreadyInvertedIn),
    poSRS(poSRSIn),
    pszBaseURL(CPLStrdup(pszBaseURLIn)),
    pszName(CPLStrdup(pszNameIn)),
    pszNS(pszNSIn ? CPLStrdup(pszNSIn) : nullptr),
    pszNSVal(pszNSValIn ? CPLStrdup(pszNSValIn) : nullptr),
    bStreamingDS(false),
    poBaseDS(nullptr),
    poBaseLayer(nullptr),
    bHasFetched(false),
    bReloadNeeded(false),
    eGeomType(wkbUnknown),
    nFeatures(-1),
    bCountFeaturesInGetNextFeature(false),
    dfMinX(0.0),
    dfMinY(0.0),
    dfMaxX(0.0),
    dfMaxY(0.0),
    bHasExtents(false),
    poFetchedFilterGeom(nullptr),
    nExpectedInserts(0),
    bInTransaction(false),
    bUseFeatureIdAtLayerLevel(false),
    bPagingActive(false),
    nPagingStartIndex(0),
    nFeatureRead(0),
    nFeatureCountRequested(0),
    pszRequiredOutputFormat(nullptr)
{
    SetDescription( pszName );
}

/************************************************************************/
/*                             Clone()                                  */
/************************************************************************/

OGRWFSLayer* OGRWFSLayer::Clone()
{
    OGRWFSLayer* poDupLayer = new OGRWFSLayer(poDS, poSRS, bAxisOrderAlreadyInverted,
                                              pszBaseURL, pszName, pszNS, pszNSVal);
    if (poSRS)
        poSRS->Reference();
    poDupLayer->poFeatureDefn = GetLayerDefn()->Clone();
    poDupLayer->poFeatureDefn->Reference();
    poDupLayer->bGotApproximateLayerDefn = bGotApproximateLayerDefn;
    poDupLayer->eGeomType = poDupLayer->poFeatureDefn->GetGeomType();
    poDupLayer->pszRequiredOutputFormat = pszRequiredOutputFormat ? CPLStrdup(pszRequiredOutputFormat) : nullptr;

    /* Copy existing schema file if already found */
    CPLString osSrcFileName = CPLSPrintf("/vsimem/tempwfs_%p/file.xsd", this);
    CPLString osTargetFileName = CPLSPrintf("/vsimem/tempwfs_%p/file.xsd", poDupLayer);
    CPLCopyFile(osTargetFileName, osSrcFileName);

    return poDupLayer;
}

/************************************************************************/
/*                            ~OGRWFSLayer()                            */
/************************************************************************/

OGRWFSLayer::~OGRWFSLayer()

{
    if( bInTransaction )
        OGRWFSLayer::CommitTransaction();

    if( poSRS != nullptr )
        poSRS->Release();

    if( poFeatureDefn != nullptr )
        poFeatureDefn->Release();
    delete poGMLFeatureClass;

    CPLFree(pszBaseURL);
    CPLFree(pszName);
    CPLFree(pszNS);
    CPLFree(pszNSVal);

    GDALClose(poBaseDS);

    delete poFetchedFilterGeom;

    CPLString osTmpDirName = CPLSPrintf("/vsimem/tempwfs_%p", this);
    OGRWFSRecursiveUnlink(osTmpDirName);

    CPLFree(pszRequiredOutputFormat);
}

/************************************************************************/
/*                    GetDescribeFeatureTypeURL()                       */
/************************************************************************/

CPLString OGRWFSLayer::GetDescribeFeatureTypeURL(CPL_UNUSED int bWithNS)
{
    CPLString osURL(pszBaseURL);
    osURL = CPLURLAddKVP(osURL, "SERVICE", "WFS");
    osURL = CPLURLAddKVP(osURL, "VERSION", poDS->GetVersion());
    osURL = CPLURLAddKVP(osURL, "REQUEST", "DescribeFeatureType");
    osURL = CPLURLAddKVP(osURL, "TYPENAME", WFS_EscapeURL(pszName));
    osURL = CPLURLAddKVP(osURL, "PROPERTYNAME", nullptr);
    osURL = CPLURLAddKVP(osURL, "MAXFEATURES", nullptr);
    osURL = CPLURLAddKVP(osURL, "COUNT", nullptr);
    osURL = CPLURLAddKVP(osURL, "FILTER", nullptr);
    osURL = CPLURLAddKVP(osURL, "OUTPUTFORMAT", pszRequiredOutputFormat ? WFS_EscapeURL(pszRequiredOutputFormat).c_str() : nullptr);

    if (pszNS && poDS->GetNeedNAMESPACE())
    {
        /* Older Deegree version require NAMESPACE (e.g. http://www.nokis.org/deegree2/ogcwebservice) */
        /* This has been now corrected */
        CPLString osValue("xmlns(");
        osValue += pszNS;
        osValue += "=";
        osValue += pszNSVal;
        osValue += ")";
        osURL = CPLURLAddKVP(osURL, "NAMESPACE", WFS_EscapeURL(osValue));
    }

    return osURL;
}

/************************************************************************/
/*                      DescribeFeatureType()                           */
/************************************************************************/

OGRFeatureDefn* OGRWFSLayer::DescribeFeatureType()
{
    CPLString osURL = GetDescribeFeatureTypeURL(TRUE);

    CPLDebug("WFS", "%s", osURL.c_str());

    CPLHTTPResult* psResult = poDS->HTTPFetch( osURL, nullptr);
    if (psResult == nullptr)
    {
        return nullptr;
    }

    if (strstr((const char*)psResult->pabyData, "<ServiceExceptionReport") != nullptr)
    {
        if( poDS->IsOldDeegree((const char*)psResult->pabyData) )
        {
            CPLHTTPDestroyResult(psResult);
            return DescribeFeatureType();
        }
        CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s",
                 psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }

    CPLXMLNode* psXML = CPLParseXMLString( (const char*) psResult->pabyData );
    if (psXML == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid XML content : %s",
                psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }
    CPLHTTPDestroyResult(psResult);

    CPLXMLNode* psSchema = WFSFindNode(psXML, "schema");
    if (psSchema == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find <Schema>");
        CPLDestroyXMLNode( psXML );

        return nullptr;
    }

    OGRFeatureDefn* poFDefn = ParseSchema(psSchema);
    if (poFDefn)
        poDS->SaveLayerSchema(pszName, psSchema);

    CPLDestroyXMLNode( psXML );
    return poFDefn;
}

/************************************************************************/
/*                            ParseSchema()                             */
/************************************************************************/

OGRFeatureDefn* OGRWFSLayer::ParseSchema(CPLXMLNode* psSchema)
{
    osTargetNamespace = CPLGetXMLValue(psSchema, "targetNamespace", "");

    CPLString osTmpFileName;

    osTmpFileName = CPLSPrintf("/vsimem/tempwfs_%p/file.xsd", this);
    CPLSerializeXMLTreeToFile(psSchema, osTmpFileName);

    std::vector<GMLFeatureClass*> aosClasses;
    bool bFullyUnderstood = false;
    bool bHaveSchema = GMLParseXSD( osTmpFileName, aosClasses, bFullyUnderstood );

    if (bHaveSchema && aosClasses.size() == 1)
    {
        //CPLDebug("WFS", "Creating %s for %s", osTmpFileName.c_str(), GetName());
        return BuildLayerDefnFromFeatureClass(aosClasses[0]);
    }
    else if (bHaveSchema)
    {
        std::vector<GMLFeatureClass*>::const_iterator oIter = aosClasses.begin();
        std::vector<GMLFeatureClass*>::const_iterator oEndIter = aosClasses.end();
        while (oIter != oEndIter)
        {
            GMLFeatureClass* poClass = *oIter;
            ++oIter;
            delete poClass;
        }
    }

    VSIUnlink(osTmpFileName);

    return nullptr;
}
/************************************************************************/
/*                   BuildLayerDefnFromFeatureClass()                   */
/************************************************************************/

OGRFeatureDefn* OGRWFSLayer::BuildLayerDefnFromFeatureClass(GMLFeatureClass* poClass)
{
    poGMLFeatureClass = poClass;

    OGRFeatureDefn* poFDefn = new OGRFeatureDefn( pszName );
    poFDefn->SetGeomType(wkbNone);
    if( poGMLFeatureClass->GetGeometryPropertyCount() > 0 )
    {
        poFDefn->SetGeomType( (OGRwkbGeometryType)poGMLFeatureClass->GetGeometryProperty(0)->GetType() );
        poFDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
    }

/* -------------------------------------------------------------------- */
/*      Added attributes (properties).                                  */
/* -------------------------------------------------------------------- */
    if( poDS->ExposeGMLId() )
    {
        OGRFieldDefn oField( "gml_id", OFTString );
        oField.SetNullable(FALSE);
        poFDefn->AddFieldDefn( &oField );
    }

    for( int iField = 0; iField < poGMLFeatureClass->GetPropertyCount(); iField++ )
    {
        GMLPropertyDefn *poProperty = poGMLFeatureClass->GetProperty( iField );
        OGRFieldSubType eSubType = OFSTNone;
        const OGRFieldType eFType = GML_GetOGRFieldType(poProperty->GetType(), eSubType);

        OGRFieldDefn oField( poProperty->GetName(), eFType );
        oField.SetSubType(eSubType);
        if ( STARTS_WITH_CI(oField.GetNameRef(), "ogr:") )
            oField.SetName(poProperty->GetName()+4);
        if( poProperty->GetWidth() > 0 )
            oField.SetWidth( poProperty->GetWidth() );
        if( poProperty->GetPrecision() > 0 )
            oField.SetPrecision( poProperty->GetPrecision() );
        if( !poDS->IsEmptyAsNull() )
            oField.SetNullable(poProperty->IsNullable());

        poFDefn->AddFieldDefn( &oField );
    }

    if( poGMLFeatureClass->GetGeometryPropertyCount() > 0 )
    {
        const char* pszGeometryColumnName = poGMLFeatureClass->GetGeometryProperty(0)->GetSrcElement();
        if (pszGeometryColumnName[0] != '\0')
        {
            osGeometryColumnName = pszGeometryColumnName;
            if( poFDefn->GetGeomFieldCount() > 0 )
            {
                poFDefn->GetGeomFieldDefn(0)->SetNullable(poGMLFeatureClass->GetGeometryProperty(0)->IsNullable());
                poFDefn->GetGeomFieldDefn(0)->SetName(pszGeometryColumnName);
            }
        }
    }

    return poFDefn;
}

/************************************************************************/
/*                       MakeGetFeatureURL()                            */
/************************************************************************/

CPLString OGRWFSLayer::MakeGetFeatureURL(int nRequestMaxFeatures, int bRequestHits)
{
    CPLString osURL(pszBaseURL);
    osURL = CPLURLAddKVP(osURL, "SERVICE", "WFS");
    osURL = CPLURLAddKVP(osURL, "VERSION", poDS->GetVersion());
    osURL = CPLURLAddKVP(osURL, "REQUEST", "GetFeature");
    if( atoi(poDS->GetVersion()) >= 2 )
        osURL = CPLURLAddKVP(osURL, "TYPENAMES", WFS_EscapeURL(pszName));
    else
        osURL = CPLURLAddKVP(osURL, "TYPENAME", WFS_EscapeURL(pszName));
    if (pszRequiredOutputFormat)
        osURL = CPLURLAddKVP(osURL, "OUTPUTFORMAT", WFS_EscapeURL(pszRequiredOutputFormat));

    if (poDS->IsPagingAllowed() && !bRequestHits)
    {
        osURL = CPLURLAddKVP(osURL, "STARTINDEX",
            CPLSPrintf("%d", nPagingStartIndex +
                                poDS->GetBaseStartIndex()));
        nRequestMaxFeatures = poDS->GetPageSize();
        nFeatureCountRequested = nRequestMaxFeatures;
        bPagingActive = true;
    }

    if (nRequestMaxFeatures)
    {
        osURL = CPLURLAddKVP(osURL,
                             atoi(poDS->GetVersion()) >= 2 ? "COUNT" : "MAXFEATURES",
                             CPLSPrintf("%d", nRequestMaxFeatures));
    }
    if (pszNS && poDS->GetNeedNAMESPACE())
    {
        /* Older Deegree version require NAMESPACE (e.g. http://www.nokis.org/deegree2/ogcwebservice) */
        /* This has been now corrected */
        CPLString osValue("xmlns(");
        osValue += pszNS;
        osValue += "=";
        osValue += pszNSVal;
        osValue += ")";
        osURL = CPLURLAddKVP(osURL, "NAMESPACE", WFS_EscapeURL(osValue));
    }

    delete poFetchedFilterGeom;
    poFetchedFilterGeom = nullptr;

    CPLString osGeomFilter;

    if (m_poFilterGeom != nullptr && !osGeometryColumnName.empty())
    {
        OGREnvelope oEnvelope;
        m_poFilterGeom->getEnvelope(&oEnvelope);

        poFetchedFilterGeom = m_poFilterGeom->clone();

        osGeomFilter = "<BBOX>";
        if (atoi(poDS->GetVersion()) >= 2)
            osGeomFilter += "<ValueReference>";
        else
            osGeomFilter += "<PropertyName>";
        if (pszNS)
        {
            osGeomFilter += pszNS;
            osGeomFilter += ":";
        }
        osGeomFilter += osGeometryColumnName;
        if (atoi(poDS->GetVersion()) >= 2)
            osGeomFilter += "</ValueReference>";
        else
            osGeomFilter += "</PropertyName>";

        if ( atoi(poDS->GetVersion()) >= 2 )
        {
            osGeomFilter += "<gml:Envelope";

            CPLString osSRSName = CPLURLGetValue(pszBaseURL, "SRSNAME");
            if( !osSRSName.empty() )
            {
                osGeomFilter += " srsName=\"";
                osGeomFilter += osSRSName;
                osGeomFilter += "\"";
            }

            osGeomFilter += ">";
            if (bAxisOrderAlreadyInverted)
            {
                osGeomFilter += CPLSPrintf("<gml:lowerCorner>%.16f %.16f</gml:lowerCorner><gml:upperCorner>%.16f %.16f</gml:upperCorner>",
                                        oEnvelope.MinY, oEnvelope.MinX, oEnvelope.MaxY, oEnvelope.MaxX);
            }
            else
                osGeomFilter += CPLSPrintf("<gml:lowerCorner>%.16f %.16f</gml:lowerCorner><gml:upperCorner>%.16f %.16f</gml:upperCorner>",
                                        oEnvelope.MinX, oEnvelope.MinY, oEnvelope.MaxX, oEnvelope.MaxY);
            osGeomFilter += "</gml:Envelope>";
        }
        else if ( poDS->RequiresEnvelopeSpatialFilter() )
        {
            osGeomFilter += "<Envelope xmlns=\"http://www.opengis.net/gml\">";
            if (bAxisOrderAlreadyInverted)
            {
                /* We can go here in WFS 1.1 with geographic coordinate systems */
                /* that are natively return in lat,long order, but as we have */
                /* presented long,lat order to the user, we must switch back */
                /* for the server... */
                osGeomFilter += CPLSPrintf("<coord><X>%.16f</X><Y>%.16f</Y></coord><coord><X>%.16f</X><Y>%.16f</Y></coord>",
                                        oEnvelope.MinY, oEnvelope.MinX, oEnvelope.MaxY, oEnvelope.MaxX);
            }
            else
                osGeomFilter += CPLSPrintf("<coord><X>%.16f</X><Y>%.16f</Y></coord><coord><X>%.16f</X><Y>%.16f</Y></coord>",
                                        oEnvelope.MinX, oEnvelope.MinY, oEnvelope.MaxX, oEnvelope.MaxY);
            osGeomFilter += "</Envelope>";
        }
        else
        {
            osGeomFilter += "<gml:Box>";
            osGeomFilter += "<gml:coordinates>";
            if (bAxisOrderAlreadyInverted)
            {
                /* We can go here in WFS 1.1 with geographic coordinate systems */
                /* that are natively return in lat,long order, but as we have */
                /* presented long,lat order to the user, we must switch back */
                /* for the server... */
                osGeomFilter += CPLSPrintf("%.16f,%.16f %.16f,%.16f", oEnvelope.MinY, oEnvelope.MinX, oEnvelope.MaxY, oEnvelope.MaxX);
            }
            else
                osGeomFilter += CPLSPrintf("%.16f,%.16f %.16f,%.16f", oEnvelope.MinX, oEnvelope.MinY, oEnvelope.MaxX, oEnvelope.MaxY);
            osGeomFilter += "</gml:coordinates>";
            osGeomFilter += "</gml:Box>";
        }
        osGeomFilter += "</BBOX>";
    }

    if (!osGeomFilter.empty() || !osWFSWhere.empty())
    {
        CPLString osFilter;
        if (atoi(poDS->GetVersion()) >= 2)
            osFilter = "<Filter xmlns=\"http://www.opengis.net/fes/2.0\"";
        else
            osFilter = "<Filter xmlns=\"http://www.opengis.net/ogc\"";
        if (pszNS)
        {
            osFilter += " xmlns:";
            osFilter += pszNS;
            osFilter += "=\"";
            osFilter += pszNSVal;
            osFilter += "\"";
        }
        if (atoi(poDS->GetVersion()) >= 2)
            osFilter += " xmlns:gml=\"http://www.opengis.net/gml/3.2\">";
        else
            osFilter += " xmlns:gml=\"http://www.opengis.net/gml\">";
        if (!osGeomFilter.empty() && !osWFSWhere.empty())
            osFilter += "<And>";
        osFilter += osWFSWhere;
        osFilter += osGeomFilter;
        if (!osGeomFilter.empty() && !osWFSWhere.empty())
            osFilter += "</And>";
        osFilter += "</Filter>";

        osURL = CPLURLAddKVP(osURL, "FILTER", WFS_EscapeURL(osFilter));
    }

    if (bRequestHits)
    {
        osURL = CPLURLAddKVP(osURL, "RESULTTYPE", "hits");
    }
    else if (!aoSortColumns.empty())
    {
        CPLString osSortBy;
        for( int i=0; i < (int)aoSortColumns.size(); i++)
        {
            if( i > 0 )
                osSortBy += ",";
            osSortBy += aoSortColumns[i].osColumn;
            if( !aoSortColumns[i].bAsc )
            {
                if (atoi(poDS->GetVersion()) >= 2)
                    osSortBy += " DESC";
                else
                    osSortBy += " D";
            }
        }
        osURL = CPLURLAddKVP(osURL, "SORTBY", WFS_EscapeURL(osSortBy));
    }

    /* If no PROPERTYNAME is specified, build one if there are ignored fields */
    CPLString osPropertyName = CPLURLGetValue(osURL, "PROPERTYNAME");
    const char* pszPropertyName = osPropertyName.c_str();
    if (pszPropertyName[0] == 0 && poFeatureDefn != nullptr)
    {
        bool bHasIgnoredField = false;
        osPropertyName.clear();
        for( int iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
        {
            if (EQUAL(poFeatureDefn->GetFieldDefn(iField)->GetNameRef(), "gml_id"))
            {
                /* fake field : skip it */
            }
            else if (poFeatureDefn->GetFieldDefn(iField)->IsIgnored())
            {
                bHasIgnoredField = true;
            }
            else
            {
                if (!osPropertyName.empty())
                    osPropertyName += ",";
                osPropertyName += poFeatureDefn->GetFieldDefn(iField)->GetNameRef();
            }
        }
        if (!osGeometryColumnName.empty())
        {
            if (poFeatureDefn->IsGeometryIgnored())
            {
                bHasIgnoredField = true;
            }
            else
            {
                if (!osPropertyName.empty())
                    osPropertyName += ",";
                osPropertyName += osGeometryColumnName;
            }
        }

        if( bHasIgnoredField && !osPropertyName.empty() )
        {
            osPropertyName = "(" + osPropertyName + ")";
            osURL = CPLURLAddKVP(osURL, "PROPERTYNAME", WFS_EscapeURL(osPropertyName));
        }
    }

    return osURL;
}

/************************************************************************/
/*               OGRWFSFetchContentDispositionFilename()                */
/************************************************************************/

static const char* OGRWFSFetchContentDispositionFilename(char** papszHeaders)
{
    const char* pszContentDisposition =
        CSLFetchNameValue(papszHeaders, "Content-Disposition");
    if( pszContentDisposition &&
        STARTS_WITH(pszContentDisposition, "attachment; filename=") )
    {
        return pszContentDisposition + strlen("attachment; filename=");
    }
    return nullptr;
}

/************************************************************************/
/*                  MustRetryIfNonCompliantServer()                     */
/************************************************************************/

bool OGRWFSLayer::MustRetryIfNonCompliantServer( const char* pszServerAnswer )
{
    bool bRetry = false;

    /* Deegree server does not support PropertyIsNotEqualTo */
    /* We have to turn it into <Not><PropertyIsEqualTo> */
    if (!osWFSWhere.empty() && poDS->PropertyIsNotEqualToSupported() &&
        strstr(pszServerAnswer, "Unknown comparison operation: 'PropertyIsNotEqualTo'") != nullptr)
    {
        poDS->SetPropertyIsNotEqualToUnSupported();
        bRetry = true;
    }

    /* Deegree server requires the gml: prefix in GmlObjectId element, but ESRI */
    /* doesn't like it at all ! Other servers don't care... */
    if (!osWFSWhere.empty() && !poDS->DoesGmlObjectIdNeedGMLPrefix() &&
        strstr(pszServerAnswer, "&lt;GmlObjectId&gt; requires 'gml:id'-attribute!") != nullptr)
    {
        poDS->SetGmlObjectIdNeedsGMLPrefix();
        bRetry = true;
    }

    /* GeoServer can return the error 'Only FeatureIds are supported when encoding id filters to SDE' */
    if( !osWFSWhere.empty() && !bUseFeatureIdAtLayerLevel &&
        strstr(pszServerAnswer, "Only FeatureIds are supported") != nullptr )
    {
        bUseFeatureIdAtLayerLevel = true;
        bRetry = true;
    }

    if( bRetry )
    {
        SetAttributeFilter(osSQLWhere);
        bHasFetched = true;
        bReloadNeeded = false;
    }

    return bRetry;
}

/************************************************************************/
/*                         FetchGetFeature()                            */
/************************************************************************/

GDALDataset* OGRWFSLayer::FetchGetFeature(int nRequestMaxFeatures)
{

    CPLString osURL = MakeGetFeatureURL(nRequestMaxFeatures, FALSE);
    CPLDebug("WFS", "%s", osURL.c_str());

    CPLHTTPResult* psResult = nullptr;

    CPLString osOutputFormat = CPLURLGetValue(osURL, "OUTPUTFORMAT");

    if (CPLTestBool(CPLGetConfigOption("OGR_WFS_USE_STREAMING", "YES"))) {
        CPLString osStreamingName;
        if( STARTS_WITH(osURL, "/vsimem/") &&
                CPLTestBool(CPLGetConfigOption("CPL_CURL_ENABLE_VSIMEM", "FALSE")) )
        {
            osStreamingName = osURL;
        }
        else
        {
            osStreamingName += "/vsicurl_streaming/";
            osStreamingName += osURL;
        }

        GDALDataset* poOutputDS = nullptr;

        /* Try streaming when the output format is GML and that we have a .xsd */
        /* that we are able to understand */
        CPLString osXSDFileName = CPLSPrintf("/vsimem/tempwfs_%p/file.xsd", this);
        VSIStatBufL sBuf;
        if ((osOutputFormat.empty() || osOutputFormat.ifind("GML") != std::string::npos) &&
            VSIStatL(osXSDFileName, &sBuf) == 0 && GDALGetDriverByName("GML") != nullptr)
        {
            bStreamingDS = true;
            const char* const apszAllowedDrivers[] = { "GML", nullptr };
            const char* apszOpenOptions[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
            apszOpenOptions[0] = CPLSPrintf("XSD=%s", osXSDFileName.c_str());
            apszOpenOptions[1] = CPLSPrintf("EMPTY_AS_NULL=%s", poDS->IsEmptyAsNull() ? "YES" : "NO");
            int iGMLOOIdex = 2;
            if( CPLGetConfigOption("GML_INVERT_AXIS_ORDER_IF_LAT_LONG", nullptr) == nullptr )
            {
                apszOpenOptions[iGMLOOIdex] = CPLSPrintf("INVERT_AXIS_ORDER_IF_LAT_LONG=%s",
                                        poDS->InvertAxisOrderIfLatLong() ? "YES" : "NO");
                iGMLOOIdex ++;
            }
            if( CPLGetConfigOption("GML_CONSIDER_EPSG_AS_URN", nullptr) == nullptr )
            {
                apszOpenOptions[iGMLOOIdex] = CPLSPrintf("CONSIDER_EPSG_AS_URN=%s",
                                                poDS->GetConsiderEPSGAsURN().c_str());
                iGMLOOIdex ++;
            }
            if( CPLGetConfigOption("GML_EXPOSE_GML_ID", nullptr) == nullptr )
            {
                apszOpenOptions[iGMLOOIdex] = CPLSPrintf("EXPOSE_GML_ID=%s",
                                                poDS->ExposeGMLId() ? "YES" : "NO");
                // iGMLOOIdex ++;
            }

            poOutputDS = (GDALDataset*)
                    GDALOpenEx(osStreamingName, GDAL_OF_VECTOR, apszAllowedDrivers,
                            apszOpenOptions, nullptr);

        }
        /* Try streaming when the output format is FlatGeobuf */
        else if ((osOutputFormat.empty() || osOutputFormat.ifind("flatgeobuf") != std::string::npos) &&
            VSIStatL(osXSDFileName, &sBuf) == 0 && GDALGetDriverByName("FlatGeobuf") != nullptr)
        {
            bStreamingDS = true;
            const char* const apszAllowedDrivers[] = { "FlatGeobuf", nullptr };

            GDALDataset* poFlatGeobuf_DS = (GDALDataset*)
                    GDALOpenEx(osStreamingName, GDAL_OF_VECTOR, apszAllowedDrivers,
                            nullptr, nullptr);
            if( poFlatGeobuf_DS )
            {
                bStreamingDS = true;
                return poFlatGeobuf_DS;
            }
        }
        else
        {
            bStreamingDS = false;
        }

        if( poOutputDS )
        {
            return poOutputDS;
        }

        if  (bStreamingDS) {
            /* In case of failure, read directly the content to examine */
            /* it, if it is XML error content */
            char szBuffer[2048];
            int nRead = 0;
            VSILFILE* fp = VSIFOpenL(osStreamingName, "rb");
            if (fp)
            {
                nRead = (int)VSIFReadL(szBuffer, 1, sizeof(szBuffer) - 1, fp);
                szBuffer[nRead] = '\0';
                VSIFCloseL(fp);
            }

            if (nRead != 0)
            {
                if( MustRetryIfNonCompliantServer(szBuffer) )
                    return FetchGetFeature(nRequestMaxFeatures);

                if (strstr(szBuffer, "<ServiceExceptionReport") != nullptr ||
                    strstr(szBuffer, "<ows:ExceptionReport") != nullptr)
                {
                    if( poDS->IsOldDeegree(szBuffer) )
                    {
                        return FetchGetFeature(nRequestMaxFeatures);
                    }

                    CPLError(CE_Failure, CPLE_AppDefined,
                            "Error returned by server : %s",
                            szBuffer);
                    return nullptr;
                }
            }
        }
    }

    bStreamingDS = false;
    psResult = poDS->HTTPFetch( osURL, nullptr);
    if (psResult == nullptr)
    {
        return nullptr;
    }

    const char* pszContentType = "";
    if (psResult->pszContentType)
        pszContentType = psResult->pszContentType;

    CPLString osTmpDirName = CPLSPrintf("/vsimem/tempwfs_%p", this);
    VSIMkdir(osTmpDirName, 0);

    GByte *pabyData = psResult->pabyData;
    int    nDataLen = psResult->nDataLen;
    bool bIsMultiPart = false;
    const char* pszAttachmentFilename = nullptr;

    if(strstr(pszContentType,"multipart")
        && CPLHTTPParseMultipartMime(psResult) )
    {
        bIsMultiPart = true;
        OGRWFSRecursiveUnlink(osTmpDirName);
        VSIMkdir(osTmpDirName, 0);
        for( int i = 0; i < psResult->nMimePartCount; i++ )
        {
            CPLString osTmpFileName = osTmpDirName + "/";
            pszAttachmentFilename =
                OGRWFSFetchContentDispositionFilename(
                    psResult->pasMimePart[i].papszHeaders);

            if (pszAttachmentFilename)
                osTmpFileName += pszAttachmentFilename;
            else
                osTmpFileName += CPLSPrintf("file_%d", i);

            GByte* pData = (GByte*)VSI_MALLOC_VERBOSE(psResult->pasMimePart[i].nDataLen);
            if (pData)
            {
                memcpy(pData, psResult->pasMimePart[i].pabyData, psResult->pasMimePart[i].nDataLen);
                VSILFILE *fp = VSIFileFromMemBuffer( osTmpFileName,
                                                pData,
                                                psResult->pasMimePart[i].nDataLen, TRUE);
                VSIFCloseL(fp);
            }
        }
    }
    else
        pszAttachmentFilename =
                OGRWFSFetchContentDispositionFilename(
                    psResult->papszHeaders);

    bool bJSON = false;
    bool bCSV = false;
    bool bKML = false;
    bool bKMZ = false;
    bool bFlatGeobuf = false;
    bool bZIP = false;
    bool bGZIP = false;

    const char* pszOutputFormat = osOutputFormat.c_str();

    if (FindSubStringInsensitive(pszContentType, "json") ||
        FindSubStringInsensitive(pszOutputFormat, "json"))
    {
        bJSON = true;
    }
    else if (FindSubStringInsensitive(pszContentType, "csv") ||
             FindSubStringInsensitive(pszOutputFormat, "csv"))
    {
        bCSV = true;
    }
    else if (FindSubStringInsensitive(pszContentType, "kml") ||
             FindSubStringInsensitive(pszOutputFormat, "kml"))
    {
        bKML = true;
    }
    else if (FindSubStringInsensitive(pszContentType, "kmz") ||
             FindSubStringInsensitive(pszOutputFormat, "kmz"))
    {
        bKMZ = true;
    }
    else if (FindSubStringInsensitive(pszContentType, "flatgeobuf") ||
             FindSubStringInsensitive(pszOutputFormat, "flatgeobuf"))
    {
        bFlatGeobuf = true;
    }
    else if (strstr(pszContentType, "application/zip") != nullptr)
    {
        bZIP = true;
    }
    else if (strstr(pszContentType, "application/gzip") != nullptr)
    {
        bGZIP = true;
    }

    if( MustRetryIfNonCompliantServer((const char*)pabyData) )
    {
        CPLHTTPDestroyResult(psResult);
        return FetchGetFeature(nRequestMaxFeatures);
    }

    if (strstr((const char*)pabyData, "<ServiceExceptionReport") != nullptr ||
        strstr((const char*)pabyData, "<ows:ExceptionReport") != nullptr)
    {
        if( poDS->IsOldDeegree((const char*)pabyData) )
        {
            CPLHTTPDestroyResult(psResult);
            return FetchGetFeature(nRequestMaxFeatures);
        }

        CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s",
                 pabyData);
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }

    CPLString osTmpFileName;

    if( !bIsMultiPart )
    {
        if( bJSON )
            osTmpFileName = osTmpDirName + "/file.geojson";
        else if( bZIP )
            osTmpFileName = osTmpDirName + "/file.zip";
        else if( bCSV )
            osTmpFileName = osTmpDirName + "/file.csv";
        else if( bKML )
            osTmpFileName = osTmpDirName + "/file.kml";
        else if( bKMZ )
            osTmpFileName = osTmpDirName + "/file.kmz";
        else if( bFlatGeobuf )
            osTmpFileName = osTmpDirName + "/file.fgb";
        /* GML is a special case. It needs the .xsd file that has been saved */
        /* as file.xsd, so we cannot used the attachment filename */
        else if (pszAttachmentFilename &&
                 !EQUAL(CPLGetExtension(pszAttachmentFilename), "GML"))
        {
            osTmpFileName = osTmpDirName + "/";
            osTmpFileName += pszAttachmentFilename;
        }
        else
        {
            osTmpFileName = osTmpDirName + "/file.gfs";
            VSIUnlink(osTmpFileName);

            osTmpFileName = osTmpDirName + "/file.gml";
        }

        VSILFILE *fp = VSIFileFromMemBuffer( osTmpFileName, pabyData,
                                        nDataLen, TRUE);
        VSIFCloseL(fp);
        psResult->pabyData = nullptr;

        if( bZIP )
        {
            osTmpFileName = "/vsizip/" + osTmpFileName;
        }
        else if( bGZIP )
        {
            osTmpFileName = "/vsigzip/" + osTmpFileName;
        }
    }
    else
    {
        pabyData = nullptr;
        nDataLen = 0;
        osTmpFileName = osTmpDirName;
    }

    CPLHTTPDestroyResult(psResult);

    const char* const * papszOpenOptions = nullptr;
    const char* apszGMLOpenOptions[4] = { nullptr, nullptr, nullptr, nullptr };
    int iGMLOOIdex = 0;
    if( CPLGetConfigOption("GML_INVERT_AXIS_ORDER_IF_LAT_LONG", nullptr) == nullptr )
    {
        apszGMLOpenOptions[iGMLOOIdex] = CPLSPrintf("INVERT_AXIS_ORDER_IF_LAT_LONG=%s",
                                poDS->InvertAxisOrderIfLatLong() ? "YES" : "NO");
        iGMLOOIdex ++;
    }
    if( CPLGetConfigOption("GML_CONSIDER_EPSG_AS_URN", nullptr) == nullptr )
    {
        apszGMLOpenOptions[iGMLOOIdex] = CPLSPrintf("CONSIDER_EPSG_AS_URN=%s",
                                        poDS->GetConsiderEPSGAsURN().c_str());
        iGMLOOIdex ++;
    }
    if( CPLGetConfigOption("GML_EXPOSE_GML_ID", nullptr) == nullptr )
    {
        apszGMLOpenOptions[iGMLOOIdex] = CPLSPrintf("EXPOSE_GML_ID=%s",
                                        poDS->ExposeGMLId() ? "YES" : "NO");
        // iGMLOOIdex ++;
    }

    GDALDriverH hDrv = GDALIdentifyDriver(osTmpFileName, nullptr);
    if( hDrv != nullptr && hDrv == GDALGetDriverByName("GML") )
        papszOpenOptions = apszGMLOpenOptions;

    GDALDataset* poPageDS = (GDALDataset*) GDALOpenEx(osTmpFileName,
                                GDAL_OF_VECTOR, nullptr, papszOpenOptions, nullptr);
    if( poPageDS == nullptr && (bZIP || bIsMultiPart) )
    {
        char** papszFileList = VSIReadDir(osTmpFileName);
        for( int i = 0; papszFileList != nullptr && papszFileList[i] != nullptr; i++ )
        {
            CPLString osFullFilename =
                CPLFormFilename( osTmpFileName, papszFileList[i], nullptr );
            hDrv = GDALIdentifyDriver(osFullFilename, nullptr);
            if( hDrv != nullptr && hDrv == GDALGetDriverByName("GML") )
                papszOpenOptions = apszGMLOpenOptions;
            poPageDS = (GDALDataset*) GDALOpenEx(osFullFilename,
                                GDAL_OF_VECTOR, nullptr, papszOpenOptions, nullptr);
            if (poPageDS != nullptr)
                break;
        }

        CSLDestroy( papszFileList );
    }

    if( poPageDS == nullptr )
    {
        if( pabyData != nullptr && !bJSON && !bZIP &&
            strstr((const char*)pabyData, "<wfs:FeatureCollection") == nullptr &&
            strstr((const char*)pabyData, "<gml:FeatureCollection") == nullptr )
        {
            if (nDataLen > 1000)
                pabyData[1000] = 0;
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Error: cannot parse %s", pabyData);
        }
        return nullptr;
    }

    OGRLayer* poLayer = poPageDS->GetLayer(0);
    if (poLayer == nullptr)
    {
        GDALClose(poPageDS);
        return nullptr;
    }

    return poPageDS;
}

/************************************************************************/
/*                            GetLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn * OGRWFSLayer::GetLayerDefn()
{
    if (poFeatureDefn)
        return poFeatureDefn;

    poDS->LoadMultipleLayerDefn(GetName(), pszNS, pszNSVal);

    if (poFeatureDefn)
        return poFeatureDefn;

    return BuildLayerDefn();
}

/************************************************************************/
/*                          BuildLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn * OGRWFSLayer::BuildLayerDefn(OGRFeatureDefn* poSrcFDefn)
{
    bool bUnsetWidthPrecision = false;

    poFeatureDefn = new OGRFeatureDefn( pszName );
    poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
    poFeatureDefn->Reference();

    GDALDataset* l_poDS = nullptr;

    if (poSrcFDefn == nullptr)
        poSrcFDefn = DescribeFeatureType();
    if (poSrcFDefn == nullptr)
    {
        l_poDS = FetchGetFeature(1);
        if (l_poDS == nullptr)
        {
            return poFeatureDefn;
        }
        OGRLayer* l_poLayer = l_poDS->GetLayer(0);
        if( l_poLayer == nullptr )
        {
            return poFeatureDefn;
        }
        poSrcFDefn = l_poLayer->GetLayerDefn();
        bGotApproximateLayerDefn = true;
        /* We cannot trust width and precision based on a single feature */
        bUnsetWidthPrecision = true;
    }

    CPLString osPropertyName = CPLURLGetValue(pszBaseURL, "PROPERTYNAME");
    const char* pszPropertyName = osPropertyName.c_str();

    poFeatureDefn->SetGeomType(poSrcFDefn->GetGeomType());
    if( poSrcFDefn->GetGeomFieldCount() > 0 )
        poFeatureDefn->GetGeomFieldDefn(0)->SetName(poSrcFDefn->GetGeomFieldDefn(0)->GetNameRef());
    for( int i = 0; i < poSrcFDefn->GetFieldCount(); i++ )
    {
        if (pszPropertyName[0] != 0)
        {
            if (strstr(pszPropertyName,
                       poSrcFDefn->GetFieldDefn(i)->GetNameRef()) != nullptr)
                poFeatureDefn->AddFieldDefn(poSrcFDefn->GetFieldDefn(i));
            else
                bGotApproximateLayerDefn = true;
        }
        else
        {
            OGRFieldDefn oFieldDefn(poSrcFDefn->GetFieldDefn(i));
            if( bUnsetWidthPrecision )
            {
                oFieldDefn.SetWidth(0);
                oFieldDefn.SetPrecision(0);
            }
            poFeatureDefn->AddFieldDefn(&oFieldDefn);
        }
    }

    if (l_poDS)
        GDALClose(l_poDS);
    else
        delete poSrcFDefn;

    return poFeatureDefn;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRWFSLayer::ResetReading()

{
    GetLayerDefn();
    if( bPagingActive )
        bReloadNeeded = true;
    nPagingStartIndex = 0;
    nFeatureRead = 0;
    nFeatureCountRequested = 0;
    if( bReloadNeeded )
    {
        GDALClose(poBaseDS);
        poBaseDS = nullptr;
        poBaseLayer = nullptr;
        bHasFetched = false;
        bReloadNeeded = false;
    }
    if (poBaseLayer)
        poBaseLayer->ResetReading();
}

/************************************************************************/
/*                         SetIgnoredFields()                           */
/************************************************************************/

OGRErr OGRWFSLayer::SetIgnoredFields( const char **papszFields )
{
    bReloadNeeded = true;
    ResetReading();
    return OGRLayer::SetIgnoredFields(papszFields);
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRWFSLayer::GetNextFeature()
{
    GetLayerDefn();

    while( true )
    {
        if( bPagingActive &&
            nFeatureRead == nPagingStartIndex + nFeatureCountRequested )
        {
            bReloadNeeded = true;
            nPagingStartIndex = nFeatureRead;
        }
        if( bReloadNeeded )
        {
            GDALClose(poBaseDS);
            poBaseDS = nullptr;
            poBaseLayer = nullptr;
            bHasFetched = false;
            bReloadNeeded = false;
        }
        if( poBaseDS == nullptr && !bHasFetched )
        {
            bHasFetched = true;
            poBaseDS = FetchGetFeature(0);
            poBaseLayer = nullptr;
            if (poBaseDS)
            {
                poBaseLayer = poBaseDS->GetLayer(0);
                if( poBaseLayer == nullptr )
                    return nullptr;
                poBaseLayer->ResetReading();

                /* Check that the layer field definition is consistent with the one */
                /* we got in BuildLayerDefn() */
                if (poFeatureDefn->GetFieldCount() != poBaseLayer->GetLayerDefn()->GetFieldCount())
                    bGotApproximateLayerDefn = true;
                else
                {
                    for( int iField = 0;
                         iField < poFeatureDefn->GetFieldCount();
                         iField++)
                    {
                        OGRFieldDefn* poFDefn1 = poFeatureDefn->GetFieldDefn(iField);
                        OGRFieldDefn* poFDefn2 = poBaseLayer->GetLayerDefn()->GetFieldDefn(iField);
                        if (strcmp(poFDefn1->GetNameRef(), poFDefn2->GetNameRef()) != 0 ||
                            poFDefn1->GetType() != poFDefn2->GetType())
                        {
                            bGotApproximateLayerDefn = true;
                            break;
                        }
                    }
                }
            }
        }
        if (poBaseDS == nullptr || poBaseLayer == nullptr)
            return nullptr;

        OGRFeature* poSrcFeature = poBaseLayer->GetNextFeature();
        if (poSrcFeature == nullptr)
            return nullptr;
        nFeatureRead ++;
        if( bCountFeaturesInGetNextFeature )
            nFeatures ++;

        OGRGeometry* poGeom = poSrcFeature->GetGeometryRef();
        if( m_poFilterGeom != nullptr && poGeom != nullptr &&
            !FilterGeometry( poGeom ) )
        {
            delete poSrcFeature;
            continue;
        }

        /* Client-side attribute filtering with underlying layer defn */
        /* identical to exposed layer defn. */
        if( !bGotApproximateLayerDefn &&
            osWFSWhere.empty() &&
            m_poAttrQuery != nullptr &&
            !m_poAttrQuery->Evaluate( poSrcFeature ) )
        {
            delete poSrcFeature;
            continue;
        }

        OGRFeature* poNewFeature = new OGRFeature(poFeatureDefn);
        if( bGotApproximateLayerDefn )
        {
            poNewFeature->SetFrom(poSrcFeature);

            /* Client-side attribute filtering. */
            if( m_poAttrQuery != nullptr &&
                osWFSWhere.empty() &&
                !m_poAttrQuery->Evaluate( poNewFeature ) )
            {
                delete poSrcFeature;
                delete poNewFeature;
                continue;
            }
        }
        else
        {
            for( int iField = 0;
                 iField < poFeatureDefn->GetFieldCount();
                 iField++ )
            {
                poNewFeature->SetField(
                    iField, poSrcFeature->GetRawFieldRef(iField) );
            }
            poNewFeature->SetStyleString(poSrcFeature->GetStyleString());
            poNewFeature->SetGeometryDirectly(poSrcFeature->StealGeometry());
        }
        poNewFeature->SetFID(poSrcFeature->GetFID());
        poGeom = poNewFeature->GetGeometryRef();

        /* FIXME? I don't really know what we should do with WFS 1.1.0 */
        /* and non-GML format !!! I guess 50% WFS servers must do it wrong anyway */
        /* GeoServer does currently axis inversion for non GML output, but */
        /* apparently this is not correct : http://jira.codehaus.org/browse/GEOS-3657 */
        if (poGeom != nullptr &&
            bAxisOrderAlreadyInverted &&
            strcmp(poBaseDS->GetDriverName(), "GML") != 0)
        {
            poGeom->swapXY();
        }

        if (poGeom && poSRS)
            poGeom->assignSpatialReference(poSRS);
        delete poSrcFeature;
        return poNewFeature;
    }
}

/************************************************************************/
/*                         SetSpatialFilter()                           */
/************************************************************************/

void OGRWFSLayer::SetSpatialFilter( OGRGeometry * poGeom )
{
    if( bStreamingDS )
    {
        bReloadNeeded = true;
    }
    else if( poFetchedFilterGeom == nullptr && poBaseDS != nullptr )
    {
        /* If there was no filter set, and that we set one */
        /* the new result set can only be a subset of the whole */
        /* so no need to reload from source */
        bReloadNeeded = false;
    }
    else if (poFetchedFilterGeom != nullptr && poGeom != nullptr && poBaseDS != nullptr)
    {
        OGREnvelope oOldEnvelope, oNewEnvelope;
        poFetchedFilterGeom->getEnvelope(&oOldEnvelope);
        poGeom->getEnvelope(&oNewEnvelope);
        /* Optimization : we don't need to request the server */
        /* if the new BBOX is inside the old BBOX as we have */
        /* already all the features */
        bReloadNeeded = !oOldEnvelope.Contains(oNewEnvelope);
    }
    else
    {
        bReloadNeeded = true;
    }
    nFeatures = -1;
    OGRLayer::SetSpatialFilter(poGeom);
    ResetReading();
}

/************************************************************************/
/*                        SetAttributeFilter()                          */
/************************************************************************/

OGRErr OGRWFSLayer::SetAttributeFilter( const char * pszFilter )
{
    if (pszFilter != nullptr && pszFilter[0] == 0)
        pszFilter = nullptr;

    CPLString osOldWFSWhere(osWFSWhere);

    CPLFree(m_pszAttrQueryString);
    m_pszAttrQueryString = (pszFilter) ? CPLStrdup(pszFilter) : nullptr;

    delete m_poAttrQuery;
    m_poAttrQuery = nullptr;

    if( pszFilter != nullptr )
    {
        m_poAttrQuery = new OGRFeatureQuery();

        OGRErr eErr = m_poAttrQuery->Compile( GetLayerDefn(), pszFilter, TRUE,
                                              WFSGetCustomFuncRegistrar() );
        if( eErr != OGRERR_NONE )
        {
            delete m_poAttrQuery;
            m_poAttrQuery = nullptr;
            return eErr;
        }
    }

    if (poDS->HasMinOperators() && m_poAttrQuery != nullptr )
    {
        swq_expr_node* poNode = (swq_expr_node*) m_poAttrQuery->GetSWQExpr();
        poNode->ReplaceBetweenByGEAndLERecurse();

        int bNeedsNullCheck = FALSE;
        int nVersion = (strcmp(poDS->GetVersion(),"1.0.0") == 0) ? 100 :
                       (atoi(poDS->GetVersion()) >= 2) ? 200 : 110;
        if( poNode->field_type != SWQ_BOOLEAN )
            osWFSWhere = "";
        else
            osWFSWhere = WFS_TurnSQLFilterToOGCFilter(
                poNode,
                nullptr,
                GetLayerDefn(),
                nVersion,
                poDS->PropertyIsNotEqualToSupported(),
                poDS->UseFeatureId() || bUseFeatureIdAtLayerLevel,
                poDS->DoesGmlObjectIdNeedGMLPrefix(),
                "",
                &bNeedsNullCheck);
        if (bNeedsNullCheck && !poDS->HasNullCheck())
            osWFSWhere = "";
    }
    else
        osWFSWhere = "";

    if (m_poAttrQuery != nullptr && osWFSWhere.empty())
    {
        CPLDebug("WFS", "Using client-side only mode for filter \"%s\"", pszFilter);
        OGRErr eErr = OGRLayer::SetAttributeFilter(pszFilter);
        if (eErr != OGRERR_NONE)
            return eErr;
    }
    ResetReading();

    osSQLWhere = (pszFilter) ? pszFilter : "";

    if (osWFSWhere != osOldWFSWhere)
        bReloadNeeded = true;
    else
        bReloadNeeded = false;
    nFeatures = -1;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRWFSLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCFastFeatureCount) )
    {
        if (nFeatures >= 0)
            return TRUE;

        return poBaseLayer != nullptr && m_poFilterGeom == nullptr &&
               m_poAttrQuery == nullptr && poBaseLayer->TestCapability(pszCap) &&
               (!poDS->IsPagingAllowed() && poBaseLayer->GetFeatureCount() < poDS->GetPageSize());
    }

    else if( EQUAL(pszCap,OLCFastGetExtent) )
    {
        if( bHasExtents )
            return TRUE;

        return poBaseLayer != nullptr &&
               poBaseLayer->TestCapability(pszCap);
    }

    else if( EQUAL(pszCap,OLCStringsAsUTF8) )
        return poBaseLayer != nullptr && poBaseLayer->TestCapability(pszCap);

    else if( EQUAL(pszCap, OLCSequentialWrite) ||
             EQUAL(pszCap, OLCDeleteFeature) ||
             EQUAL(pszCap, OLCRandomWrite) )
    {
        GetLayerDefn();
        return poDS->SupportTransactions() && poDS->UpdateMode() &&
               poFeatureDefn->GetFieldIndex("gml_id") == 0;
    }
    else if ( EQUAL(pszCap, OLCTransactions) )
    {
        return poDS->SupportTransactions() && poDS->UpdateMode();
    }
    else if( EQUAL(pszCap,OLCIgnoreFields) )
    {
        return TRUE;
    }

    return FALSE;
}

/************************************************************************/
/*                  ExecuteGetFeatureResultTypeHits()                   */
/************************************************************************/

GIntBig OGRWFSLayer::ExecuteGetFeatureResultTypeHits()
{
    char* pabyData = nullptr;
    CPLString osURL = MakeGetFeatureURL(0, TRUE);
    if (pszRequiredOutputFormat)
        osURL = CPLURLAddKVP(osURL, "OUTPUTFORMAT", WFS_EscapeURL(pszRequiredOutputFormat));
    CPLDebug("WFS", "%s", osURL.c_str());

    CPLHTTPResult* psResult = poDS->HTTPFetch( osURL, nullptr);
    if (psResult == nullptr)
    {
        return -1;
    }

    /* http://demo.snowflakesoftware.com:8080/Obstacle_AIXM_ZIP/GOPublisherWFS returns */
    /* zip content, including for RESULTTYPE=hits */
    if (psResult->pszContentType != nullptr &&
        strstr(psResult->pszContentType, "application/zip") != nullptr)
    {
        CPLString osTmpFileName;
        osTmpFileName.Printf("/vsimem/wfstemphits_%p.zip", this);
        VSILFILE *fp = VSIFileFromMemBuffer( osTmpFileName, psResult->pabyData,
                                             psResult->nDataLen, FALSE);
        VSIFCloseL(fp);

        CPLString osZipTmpFileName("/vsizip/" + osTmpFileName);

        char** papszDirContent = VSIReadDir(osZipTmpFileName);
        if (CSLCount(papszDirContent) != 1)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot parse result of RESULTTYPE=hits request : more than one file in zip");
            CSLDestroy(papszDirContent);
            CPLHTTPDestroyResult(psResult);
            VSIUnlink(osTmpFileName);
            return -1;
        }

        CPLString osFileInZipTmpFileName = osZipTmpFileName + "/";
        osFileInZipTmpFileName += papszDirContent[0];

        fp = VSIFOpenL(osFileInZipTmpFileName.c_str(), "rb");
        VSIStatBufL sBuf;
        if (fp == nullptr || VSIStatL(osFileInZipTmpFileName.c_str(), &sBuf) != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot parse result of RESULTTYPE=hits request : cannot open one file in zip");
            CSLDestroy(papszDirContent);
            CPLHTTPDestroyResult(psResult);
            VSIUnlink(osTmpFileName);
            if( fp )
                VSIFCloseL(fp);
            return -1;
        }
        pabyData = (char*) CPLMalloc((size_t)(sBuf.st_size + 1));
        pabyData[sBuf.st_size] = 0;
        VSIFReadL(pabyData, 1, (size_t)sBuf.st_size, fp);
        VSIFCloseL(fp);

        CSLDestroy(papszDirContent);
        VSIUnlink(osTmpFileName);
    }
    else
    {
        pabyData = (char*) psResult->pabyData;
        psResult->pabyData = nullptr;
    }

    if (strstr(pabyData, "<ServiceExceptionReport") != nullptr ||
        strstr(pabyData, "<ows:ExceptionReport") != nullptr)
    {
        if( poDS->IsOldDeegree(pabyData) )
        {
            CPLHTTPDestroyResult(psResult);
            return ExecuteGetFeatureResultTypeHits();
        }
        CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s",
                 pabyData);
        CPLHTTPDestroyResult(psResult);
        CPLFree(pabyData);
        return -1;
    }

    CPLXMLNode* psXML = CPLParseXMLString( pabyData );
    if (psXML == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid XML content : %s",
                pabyData);
        CPLHTTPDestroyResult(psResult);
        CPLFree(pabyData);
        return -1;
    }

    CPLStripXMLNamespace( psXML, nullptr, TRUE );
    CPLXMLNode* psRoot = CPLGetXMLNode( psXML, "=FeatureCollection" );
    if (psRoot == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find <FeatureCollection>");
        CPLDestroyXMLNode( psXML );
        CPLHTTPDestroyResult(psResult);
        CPLFree(pabyData);
        return -1;
    }

    const char* pszValue = CPLGetXMLValue(psRoot, "numberOfFeatures", nullptr);
    if (pszValue == nullptr)
        pszValue = CPLGetXMLValue(psRoot, "numberMatched", nullptr); /* WFS 2.0.0 */
    if (pszValue == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find numberOfFeatures");
        CPLDestroyXMLNode( psXML );
        CPLHTTPDestroyResult(psResult);
        CPLFree(pabyData);

        poDS->DisableSupportHits();
        return -1;
    }

    GIntBig l_nFeatures = CPLAtoGIntBig(pszValue);
    /* Hum, http://deegree3-testing.deegree.org:80/deegree-inspire-node/services?MAXFEATURES=10&SERVICE=WFS&VERSION=1.1.0&REQUEST=GetFeature&TYPENAME=ad:Address&OUTPUTFORMAT=text/xml;%20subtype=gml/3.2.1&RESULTTYPE=hits */
    /* returns more than MAXFEATURES features... So truncate to MAXFEATURES */
    CPLString osMaxFeatures = CPLURLGetValue(osURL, atoi(poDS->GetVersion()) >= 2 ? "COUNT" : "MAXFEATURES");
    if (!osMaxFeatures.empty())
    {
        GIntBig nMaxFeatures = CPLAtoGIntBig(osMaxFeatures);
        if (l_nFeatures > nMaxFeatures)
        {
            CPLDebug("WFS", "Truncating result from " CPL_FRMT_GIB " to " CPL_FRMT_GIB,
                     l_nFeatures, nMaxFeatures);
            l_nFeatures = nMaxFeatures;
        }
    }

    CPLDestroyXMLNode( psXML );
    CPLHTTPDestroyResult(psResult);
    CPLFree(pabyData);

    return l_nFeatures;
}
/************************************************************************/
/*              CanRunGetFeatureCountAndGetExtentTogether()             */
/************************************************************************/

int OGRWFSLayer::CanRunGetFeatureCountAndGetExtentTogether()
{
    /* In some cases, we can evaluate the result of GetFeatureCount() */
    /* and GetExtent() with the same data */
    CPLString osRequestURL = MakeGetFeatureURL(0, FALSE);
    return( !bHasExtents && nFeatures < 0 &&
            osRequestURL.ifind("FILTER") == std::string::npos &&
            osRequestURL.ifind("MAXFEATURES") == std::string::npos &&
            osRequestURL.ifind("COUNT") == std::string::npos &&
            !(GetLayerDefn()->IsGeometryIgnored()) );
}

/************************************************************************/
/*                           GetFeatureCount()                          */
/************************************************************************/

GIntBig OGRWFSLayer::GetFeatureCount( int bForce )
{
    if (nFeatures >= 0)
        return nFeatures;

    if (TestCapability(OLCFastFeatureCount))
        return poBaseLayer->GetFeatureCount(bForce);

    if ((m_poAttrQuery == nullptr || !osWFSWhere.empty()) &&
         poDS->GetFeatureSupportHits())
    {
        nFeatures = ExecuteGetFeatureResultTypeHits();
        if (nFeatures >= 0)
            return nFeatures;
    }

    /* If we have not yet the base layer, try to read one */
    /* feature, and then query again OLCFastFeatureCount on the */
    /* base layer. In case the WFS response would contain the */
    /* number of features */
    if (poBaseLayer == nullptr)
    {
        ResetReading();
        OGRFeature* poFeature = GetNextFeature();
        delete poFeature;
        ResetReading();

        if (TestCapability(OLCFastFeatureCount))
            return poBaseLayer->GetFeatureCount(bForce);
    }

    /* In some cases, we can evaluate the result of GetFeatureCount() */
    /* and GetExtent() with the same data */
    if( CanRunGetFeatureCountAndGetExtentTogether() )
    {
        OGREnvelope sDummy;
        GetExtent(&sDummy);
    }

    if( nFeatures < 0 )
        nFeatures = OGRLayer::GetFeatureCount(bForce);

    return nFeatures;
}

/************************************************************************/
/*                              SetExtent()                             */
/************************************************************************/

void OGRWFSLayer::SetExtents( double dfMinXIn, double dfMinYIn,
                              double dfMaxXIn, double dfMaxYIn )
{
    dfMinX = dfMinXIn;
    dfMinY = dfMinYIn;
    dfMaxX = dfMaxXIn;
    dfMaxY = dfMaxYIn;
    bHasExtents = true;
}

/************************************************************************/
/*                              GetExtent()                             */
/************************************************************************/

OGRErr OGRWFSLayer::GetExtent( OGREnvelope *psExtent, int bForce )
{
    if( bHasExtents )
    {
        psExtent->MinX = dfMinX;
        psExtent->MinY = dfMinY;
        psExtent->MaxX = dfMaxX;
        psExtent->MaxY = dfMaxY;
        return OGRERR_NONE;
    }

    /* If we have not yet the base layer, try to read one */
    /* feature, and then query again OLCFastGetExtent on the */
    /* base layer. In case the WFS response would contain the */
    /* global extent */
    if (poBaseLayer == nullptr)
    {
        ResetReading();
        OGRFeature* poFeature = GetNextFeature();
        delete poFeature;
        ResetReading();
    }

    if (TestCapability(OLCFastGetExtent))
        return poBaseLayer->GetExtent(psExtent, bForce);

    /* In some cases, we can evaluate the result of GetFeatureCount() */
    /* and GetExtent() with the same data */
    if( CanRunGetFeatureCountAndGetExtentTogether() )
    {
        bCountFeaturesInGetNextFeature = true;
        nFeatures = 0;
    }

    OGRErr eErr = OGRLayer::GetExtent(psExtent, bForce);

    if( bCountFeaturesInGetNextFeature )
    {
        if( eErr == OGRERR_NONE )
        {
            dfMinX = psExtent->MinX;
            dfMinY = psExtent->MinY;
            dfMaxX = psExtent->MaxX;
            dfMaxY = psExtent->MaxY;
            bHasExtents = true;
        }
        else
        {
            nFeatures = -1;
        }
        bCountFeaturesInGetNextFeature = false;
    }

    return eErr;
}

/************************************************************************/
/*                          GetShortName()                              */
/************************************************************************/

const char* OGRWFSLayer::GetShortName()
{
    const char* pszShortName = strchr(pszName, ':');
    if (pszShortName == nullptr)
        pszShortName = pszName;
    else
        pszShortName ++;
    return pszShortName;
}

/************************************************************************/
/*                          GetPostHeader()                             */
/************************************************************************/

CPLString OGRWFSLayer::GetPostHeader()
{
    CPLString osPost;
    osPost += "<?xml version=\"1.0\"?>\n";
    osPost += "<wfs:Transaction xmlns:wfs=\"http://www.opengis.net/wfs\"\n";
    osPost += "                 xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n";
    osPost += "                 service=\"WFS\" version=\""; osPost += poDS->GetVersion(); osPost += "\"\n";
    osPost += "                 xmlns:gml=\"http://www.opengis.net/gml\"\n";
    osPost += "                 xmlns:ogc=\"http://www.opengis.net/ogc\"\n";
    osPost += "                 xsi:schemaLocation=\"http://www.opengis.net/wfs http://schemas.opengis.net/wfs/";
    osPost += poDS->GetVersion();
    osPost += "/wfs.xsd ";
    osPost += osTargetNamespace;
    osPost += " ";

    char* pszXMLEncoded = CPLEscapeString(
                    GetDescribeFeatureTypeURL(FALSE), -1, CPLES_XML);
    osPost += pszXMLEncoded;
    CPLFree(pszXMLEncoded);

    osPost += "\">\n";

    return osPost;
}

/************************************************************************/
/*                          ICreateFeature()                             */
/************************************************************************/

OGRErr OGRWFSLayer::ICreateFeature( OGRFeature *poFeature )
{
    if (!TestCapability(OLCSequentialWrite))
    {
        if (!poDS->SupportTransactions())
            CPLError(CE_Failure, CPLE_AppDefined,
                     "CreateFeature() not supported: no WMS-T features advertized by server");
        else if (!poDS->UpdateMode())
            CPLError(CE_Failure, CPLE_AppDefined,
                     "CreateFeature() not supported: datasource opened as read-only");
        return OGRERR_FAILURE;
    }

    if (poGMLFeatureClass == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot insert feature because we didn't manage to parse the .XSD schema");
        return OGRERR_FAILURE;
    }

    if (poFeatureDefn->GetFieldIndex("gml_id") != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find gml_id field");
        return OGRERR_FAILURE;
    }

    if (poFeature->IsFieldSetAndNotNull(0))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot insert a feature when gml_id field is already set");
        return OGRERR_FAILURE;
    }

    CPLString osPost;

    const char* pszShortName = GetShortName();

    if( !bInTransaction )
    {
        osPost += GetPostHeader();
        osPost += "  <wfs:Insert>\n";
    }
    osPost += "    <feature:"; osPost += pszShortName; osPost += " xmlns:feature=\"";
    osPost += osTargetNamespace; osPost += "\">\n";

    for( int i = 1; i <= poFeature->GetFieldCount(); i++ )
    {
        if (poGMLFeatureClass->GetGeometryPropertyCount() == 1 &&
            poGMLFeatureClass->GetGeometryProperty(0)->GetAttributeIndex() == i - 1)
        {
            OGRGeometry* poGeom = poFeature->GetGeometryRef();
            if (poGeom != nullptr && !osGeometryColumnName.empty())
            {
                if (poGeom->getSpatialReference() == nullptr)
                    poGeom->assignSpatialReference(poSRS);
                char* pszGML = nullptr;
                if (strcmp(poDS->GetVersion(), "1.1.0") == 0 || atoi(poDS->GetVersion()) >= 2)
                {
                    char** papszOptions = CSLAddString(nullptr, "FORMAT=GML3");
                    pszGML = OGR_G_ExportToGMLEx((OGRGeometryH)poGeom, papszOptions);
                    CSLDestroy(papszOptions);
                }
                else
                    pszGML = OGR_G_ExportToGML((OGRGeometryH)poGeom);
                osPost += "      <feature:"; osPost += osGeometryColumnName; osPost += ">";
                osPost += pszGML;
                osPost += "</feature:"; osPost += osGeometryColumnName; osPost += ">\n";
                CPLFree(pszGML);
            }
        }
        if (i == poFeature->GetFieldCount())
            break;

#ifdef notdef
        if( poFeature->IsFieldNull(i) )
        {
            OGRFieldDefn* poFDefn = poFeature->GetFieldDefnRef(i);
            osPost += "      <feature:";
            osPost += poFDefn->GetNameRef();
            osPost += " xsi:nil=\"true\" />\n";
        }
        else
#endif
        if (poFeature->IsFieldSet(i) && !poFeature->IsFieldNull(i) )
        {
            OGRFieldDefn* poFDefn = poFeature->GetFieldDefnRef(i);
            osPost += "      <feature:";
            osPost += poFDefn->GetNameRef();
            osPost += ">";
            if (poFDefn->GetType() == OFTInteger)
                osPost += CPLSPrintf("%d", poFeature->GetFieldAsInteger(i));
            else if (poFDefn->GetType() == OFTInteger64)
                osPost += CPLSPrintf(CPL_FRMT_GIB, poFeature->GetFieldAsInteger64(i));
            else if (poFDefn->GetType() == OFTReal)
                osPost += CPLSPrintf("%.16g", poFeature->GetFieldAsDouble(i));
            else
            {
                char* pszXMLEncoded = CPLEscapeString(poFeature->GetFieldAsString(i),
                                                -1, CPLES_XML);
                osPost += pszXMLEncoded;
                CPLFree(pszXMLEncoded);
            }
            osPost += "</feature:";
            osPost += poFDefn->GetNameRef();
            osPost += ">\n";
        }
    }

    osPost += "    </feature:"; osPost += pszShortName; osPost += ">\n";

    if( !bInTransaction )
    {
        osPost += "  </wfs:Insert>\n";
        osPost += "</wfs:Transaction>\n";
    }
    else
    {
        osGlobalInsert += osPost;
        nExpectedInserts ++;
        return OGRERR_NONE;
    }

    CPLDebug("WFS", "Post : %s", osPost.c_str());

    char** papszOptions = nullptr;
    papszOptions = CSLAddNameValue(papszOptions, "POSTFIELDS", osPost.c_str());
    papszOptions = CSLAddNameValue(papszOptions, "HEADERS",
                                   "Content-Type: application/xml; charset=UTF-8");

    CPLHTTPResult* psResult = poDS->HTTPFetch(poDS->GetPostTransactionURL(), papszOptions);
    CSLDestroy(papszOptions);

    if (psResult == nullptr)
    {
        return OGRERR_FAILURE;
    }

    if (strstr((const char*)psResult->pabyData,
                                    "<ServiceExceptionReport") != nullptr ||
        strstr((const char*)psResult->pabyData,
                                    "<ows:ExceptionReport") != nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s",
                 psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    CPLDebug("WFS", "Response: %s", psResult->pabyData);

    CPLXMLNode* psXML = CPLParseXMLString( (const char*) psResult->pabyData );
    if (psXML == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid XML content : %s",
                psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    CPLStripXMLNamespace( psXML, nullptr, TRUE );
    bool bUse100Schema = false;
    CPLXMLNode* psRoot = CPLGetXMLNode( psXML, "=TransactionResponse" );
    if (psRoot == nullptr)
    {
        psRoot = CPLGetXMLNode( psXML, "=WFS_TransactionResponse" );
        if (psRoot)
            bUse100Schema = true;
    }

    if (psRoot == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find <TransactionResponse>");
        CPLDestroyXMLNode( psXML );
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    CPLXMLNode* psFeatureID = nullptr;

    if( bUse100Schema )
    {
        if (CPLGetXMLNode( psRoot, "TransactionResult.Status.FAILED" ))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Insert failed : %s",
                     psResult->pabyData);
            CPLDestroyXMLNode( psXML );
            CPLHTTPDestroyResult(psResult);
            return OGRERR_FAILURE;
        }

        psFeatureID =
            CPLGetXMLNode( psRoot, "InsertResult.FeatureId");
        if (psFeatureID == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot find InsertResult.FeatureId");
            CPLDestroyXMLNode( psXML );
            CPLHTTPDestroyResult(psResult);
            return OGRERR_FAILURE;
        }
    }
    else
    {
        const char *pszFeatureIdElt = atoi(poDS->GetVersion()) >= 2 ?
            "InsertResults.Feature.ResourceId" : "InsertResults.Feature.FeatureId";
        psFeatureID =
            CPLGetXMLNode( psRoot, pszFeatureIdElt);
        if (psFeatureID == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot find %s", pszFeatureIdElt);
            CPLDestroyXMLNode( psXML );
            CPLHTTPDestroyResult(psResult);
            return OGRERR_FAILURE;
        }
    }

    const char *pszFIDAttr = atoi(poDS->GetVersion()) >= 2 ? "rid" : "fid";
    const char* pszFID = CPLGetXMLValue(psFeatureID, pszFIDAttr, nullptr);
    if (pszFID == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s", pszFIDAttr);
        CPLDestroyXMLNode( psXML );
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    poFeature->SetField("gml_id", pszFID);

    /* If the returned fid is of the form layer_name.num, then use */
    /* num as the OGR FID */
    if (strncmp(pszFID, pszShortName, strlen(pszShortName)) == 0 &&
        pszFID[strlen(pszShortName)] == '.')
    {
        GIntBig nFID = CPLAtoGIntBig(pszFID + strlen(pszShortName) + 1);
        poFeature->SetFID(nFID);
    }

    CPLDebug("WFS", "Got FID = " CPL_FRMT_GIB, poFeature->GetFID());

    CPLDestroyXMLNode( psXML );
    CPLHTTPDestroyResult(psResult);

    /* Invalidate layer */
    bReloadNeeded = true;
    nFeatures = -1;
    bHasExtents = false;

    return OGRERR_NONE;
}

/************************************************************************/
/*                             ISetFeature()                             */
/************************************************************************/

OGRErr OGRWFSLayer::ISetFeature( OGRFeature *poFeature )
{
    if (!TestCapability(OLCRandomWrite))
    {
        if (!poDS->SupportTransactions())
            CPLError(CE_Failure, CPLE_AppDefined,
                     "SetFeature() not supported: no WMS-T features advertized by server");
        else if (!poDS->UpdateMode())
            CPLError(CE_Failure, CPLE_AppDefined,
                     "SetFeature() not supported: datasource opened as read-only");
        return OGRERR_FAILURE;
    }

    if (poFeatureDefn->GetFieldIndex("gml_id") != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find gml_id field");
        return OGRERR_FAILURE;
    }

    if (poFeature->IsFieldSetAndNotNull(0) == FALSE)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot update a feature when gml_id field is not set");
        return OGRERR_FAILURE;
    }

    if( bInTransaction )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "SetFeature() not yet dealt in transaction. Issued immediately");
    }

    const char* pszShortName = GetShortName();

    CPLString osPost;
    osPost += GetPostHeader();

    osPost += "  <wfs:Update typeName=\"feature:"; osPost += pszShortName; osPost +=  "\" xmlns:feature=\"";
    osPost += osTargetNamespace; osPost += "\">\n";

    OGRGeometry* poGeom = poFeature->GetGeometryRef();
    if ( !osGeometryColumnName.empty() )
    {
        osPost += "    <wfs:Property>\n";
        osPost += "      <wfs:Name>"; osPost += osGeometryColumnName; osPost += "</wfs:Name>\n";
        if (poGeom != nullptr)
        {
            if (poGeom->getSpatialReference() == nullptr)
                poGeom->assignSpatialReference(poSRS);
            char* pszGML = nullptr;
            if (strcmp(poDS->GetVersion(), "1.1.0") == 0 || atoi(poDS->GetVersion()) >= 2)
            {
                char** papszOptions = CSLAddString(nullptr, "FORMAT=GML3");
                pszGML = OGR_G_ExportToGMLEx((OGRGeometryH)poGeom, papszOptions);
                CSLDestroy(papszOptions);
            }
            else
                pszGML = OGR_G_ExportToGML((OGRGeometryH)poGeom);
            osPost += "      <wfs:Value>";
            osPost += pszGML;
            osPost += "</wfs:Value>\n";
            CPLFree(pszGML);
        }
        osPost += "    </wfs:Property>\n";
    }

    for( int i = 1; i < poFeature->GetFieldCount(); i++ )
    {
        OGRFieldDefn* poFDefn = poFeature->GetFieldDefnRef(i);

        osPost += "    <wfs:Property>\n";
        osPost += "      <wfs:Name>"; osPost += poFDefn->GetNameRef(); osPost += "</wfs:Name>\n";
        if (poFeature->IsFieldSetAndNotNull(i))
        {
            osPost += "      <wfs:Value>";
            if (poFDefn->GetType() == OFTInteger)
                osPost += CPLSPrintf("%d", poFeature->GetFieldAsInteger(i));
            else if (poFDefn->GetType() == OFTInteger64)
                osPost += CPLSPrintf(CPL_FRMT_GIB, poFeature->GetFieldAsInteger64(i));
            else if (poFDefn->GetType() == OFTReal)
                osPost += CPLSPrintf("%.16g", poFeature->GetFieldAsDouble(i));
            else
            {
                char* pszXMLEncoded = CPLEscapeString(poFeature->GetFieldAsString(i),
                                                -1, CPLES_XML);
                osPost += pszXMLEncoded;
                CPLFree(pszXMLEncoded);
            }
            osPost += "</wfs:Value>\n";
        }
        osPost += "    </wfs:Property>\n";
    }
    osPost += "    <ogc:Filter>\n";
    if( poDS->UseFeatureId() || bUseFeatureIdAtLayerLevel )
        osPost += "      <ogc:FeatureId fid=\"";
    else if (atoi(poDS->GetVersion()) >= 2)
        osPost += "      <ogc:ResourceId rid=\"";
    else
        osPost += "      <ogc:GmlObjectId gml:id=\"";
    osPost += poFeature->GetFieldAsString(0); osPost += "\"/>\n";
    osPost += "    </ogc:Filter>\n";
    osPost += "  </wfs:Update>\n";
    osPost += "</wfs:Transaction>\n";

    CPLDebug("WFS", "Post : %s", osPost.c_str());

    char** papszOptions = nullptr;
    papszOptions = CSLAddNameValue(papszOptions, "POSTFIELDS", osPost.c_str());
    papszOptions = CSLAddNameValue(papszOptions, "HEADERS",
                                   "Content-Type: application/xml; charset=UTF-8");

    CPLHTTPResult* psResult = poDS->HTTPFetch(poDS->GetPostTransactionURL(), papszOptions);
    CSLDestroy(papszOptions);

    if (psResult == nullptr)
    {
        return OGRERR_FAILURE;
    }

    if (strstr((const char*)psResult->pabyData,
                                    "<ServiceExceptionReport") != nullptr ||
        strstr((const char*)psResult->pabyData,
                                    "<ows:ExceptionReport") != nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s",
                 psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    CPLDebug("WFS", "Response: %s", psResult->pabyData);

    CPLXMLNode* psXML = CPLParseXMLString( (const char*) psResult->pabyData );
    if (psXML == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid XML content : %s",
                psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    CPLStripXMLNamespace( psXML, nullptr, TRUE );
    int bUse100Schema = false;
    CPLXMLNode* psRoot = CPLGetXMLNode( psXML, "=TransactionResponse" );
    if (psRoot == nullptr)
    {
        psRoot = CPLGetXMLNode( psXML, "=WFS_TransactionResponse" );
        if (psRoot)
            bUse100Schema = true;
    }
    if (psRoot == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find <TransactionResponse>");
        CPLDestroyXMLNode( psXML );
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    if( bUse100Schema )
    {
        if (CPLGetXMLNode( psRoot, "TransactionResult.Status.FAILED" ))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Update failed : %s",
                     psResult->pabyData);
            CPLDestroyXMLNode( psXML );
            CPLHTTPDestroyResult(psResult);
            return OGRERR_FAILURE;
        }
    }

    CPLDestroyXMLNode( psXML );
    CPLHTTPDestroyResult(psResult);

    /* Invalidate layer */
    bReloadNeeded = true;
    nFeatures = -1;
    bHasExtents = false;

    return OGRERR_NONE;
}
/************************************************************************/
/*                               GetFeature()                           */
/************************************************************************/

OGRFeature* OGRWFSLayer::GetFeature(GIntBig nFID)
{
    GetLayerDefn();
    if (poBaseLayer == nullptr && poFeatureDefn->GetFieldIndex("gml_id") == 0)
    {
        /* This is lovely hackish. We assume that then gml_id will be */
        /* layer_name.number. This is actually what we can observe with */
        /* GeoServer and TinyOWS */
        CPLString osVal = CPLSPrintf("gml_id = '%s." CPL_FRMT_GIB "'", GetShortName(), nFID);
        CPLString osOldSQLWhere(osSQLWhere);
        SetAttributeFilter(osVal);
        OGRFeature* poFeature = GetNextFeature();
        const char* pszOldFilter = osOldSQLWhere.size() ? osOldSQLWhere.c_str() : nullptr;
        SetAttributeFilter(pszOldFilter);
        if (poFeature)
            return poFeature;
    }

    return OGRLayer::GetFeature(nFID);
}

/************************************************************************/
/*                         DeleteFromFilter()                           */
/************************************************************************/

OGRErr OGRWFSLayer::DeleteFromFilter( CPLString osOGCFilter )
{
    if (!TestCapability(OLCDeleteFeature))
    {
        if (!poDS->SupportTransactions())
            CPLError(CE_Failure, CPLE_AppDefined,
                     "DeleteFromFilter() not supported: no WMS-T features advertized by server");
        else if (!poDS->UpdateMode())
            CPLError(CE_Failure, CPLE_AppDefined,
                     "DeleteFromFilter() not supported: datasource opened as read-only");
        return OGRERR_FAILURE;
    }

    if (poFeatureDefn->GetFieldIndex("gml_id") != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find gml_id field");
        return OGRERR_FAILURE;
    }
    const char* pszShortName = GetShortName();

    CPLString osPost;
    osPost += GetPostHeader();

    osPost += "  <wfs:Delete xmlns:feature=\""; osPost += osTargetNamespace;
    osPost += "\" typeName=\"feature:"; osPost += pszShortName; osPost += "\">\n";
    osPost += "    <ogc:Filter>\n";
    osPost += osOGCFilter;
    osPost += "    </ogc:Filter>\n";
    osPost += "  </wfs:Delete>\n";
    osPost += "</wfs:Transaction>\n";

    CPLDebug("WFS", "Post : %s", osPost.c_str());

    char** papszOptions = nullptr;
    papszOptions = CSLAddNameValue(papszOptions, "POSTFIELDS", osPost.c_str());
    papszOptions = CSLAddNameValue(papszOptions, "HEADERS",
                                   "Content-Type: application/xml; charset=UTF-8");

    CPLHTTPResult* psResult = poDS->HTTPFetch(poDS->GetPostTransactionURL(), papszOptions);
    CSLDestroy(papszOptions);

    if (psResult == nullptr)
    {
        return OGRERR_FAILURE;
    }

    if (strstr((const char*)psResult->pabyData, "<ServiceExceptionReport") != nullptr ||
        strstr((const char*)psResult->pabyData, "<ows:ExceptionReport") != nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s",
                 psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    CPLDebug("WFS", "Response: %s", psResult->pabyData);

    CPLXMLNode* psXML = CPLParseXMLString( (const char*) psResult->pabyData );
    if (psXML == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid XML content : %s",
                psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    CPLStripXMLNamespace( psXML, nullptr, TRUE );
    bool bUse100Schema = false;
    CPLXMLNode* psRoot = CPLGetXMLNode( psXML, "=TransactionResponse" );
    if (psRoot == nullptr)
    {
        psRoot = CPLGetXMLNode( psXML, "=WFS_TransactionResponse" );
        if (psRoot)
            bUse100Schema = true;
    }
    if (psRoot == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find <TransactionResponse>");
        CPLDestroyXMLNode( psXML );
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    if( bUse100Schema )
    {
        if (CPLGetXMLNode( psRoot, "TransactionResult.Status.FAILED" ))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Delete failed : %s",
                     psResult->pabyData);
            CPLDestroyXMLNode( psXML );
            CPLHTTPDestroyResult(psResult);
            return OGRERR_FAILURE;
        }
    }

    CPLDestroyXMLNode( psXML );
    CPLHTTPDestroyResult(psResult);

    /* Invalidate layer */
    bReloadNeeded = true;
    nFeatures = -1;
    bHasExtents = false;

    return OGRERR_NONE;
}

/************************************************************************/
/*                            DeleteFeature()                           */
/************************************************************************/

OGRErr OGRWFSLayer::DeleteFeature( GIntBig nFID )
{
    if (!TestCapability(OLCDeleteFeature))
    {
        if (!poDS->SupportTransactions())
            CPLError(CE_Failure, CPLE_AppDefined,
                     "DeleteFeature() not supported: no WMS-T features advertized by server");
        else if (!poDS->UpdateMode())
            CPLError(CE_Failure, CPLE_AppDefined,
                     "DeleteFeature() not supported: datasource opened as read-only");
        return OGRERR_FAILURE;
    }

    if (poFeatureDefn->GetFieldIndex("gml_id") != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find gml_id field");
        return OGRERR_FAILURE;
    }

    OGRFeature* poFeature = GetFeature(nFID);
    if (poFeature == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find feature " CPL_FRMT_GIB, nFID);
        return OGRERR_FAILURE;
    }

    const char* pszGMLID = poFeature->GetFieldAsString("gml_id");
    if (pszGMLID == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot delete a feature with gml_id unset");
        delete poFeature;
        return OGRERR_FAILURE;
    }

    if( bInTransaction )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "DeleteFeature() not yet dealt in transaction. Issued immediately");
    }

    CPLString osGMLID = pszGMLID;
    pszGMLID = nullptr;
    delete poFeature;
    poFeature = nullptr;

    CPLString osFilter;
    osFilter = "<ogc:FeatureId fid=\""; osFilter += osGMLID; osFilter += "\"/>\n";
    return DeleteFromFilter(osFilter);
}

/************************************************************************/
/*                         StartTransaction()                           */
/************************************************************************/

OGRErr OGRWFSLayer::StartTransaction()
{
    if (!TestCapability(OLCTransactions))
    {
        if (!poDS->SupportTransactions())
            CPLError(CE_Failure, CPLE_AppDefined,
                     "StartTransaction() not supported: no WMS-T features advertized by server");
        else if (!poDS->UpdateMode())
            CPLError(CE_Failure, CPLE_AppDefined,
                     "StartTransaction() not supported: datasource opened as read-only");
        return OGRERR_FAILURE;
    }

    if( bInTransaction )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                     "StartTransaction() has already been called");
        return OGRERR_FAILURE;
    }

    bInTransaction = true;
    osGlobalInsert = "";
    nExpectedInserts = 0;
    aosFIDList.resize(0);

    return OGRERR_NONE;
}

/************************************************************************/
/*                        CommitTransaction()                           */
/************************************************************************/

OGRErr OGRWFSLayer::CommitTransaction()
{
    if (!TestCapability(OLCTransactions))
    {
        if (!poDS->SupportTransactions())
            CPLError(CE_Failure, CPLE_AppDefined,
                     "CommitTransaction() not supported: no WMS-T features advertized by server");
        else if (!poDS->UpdateMode())
            CPLError(CE_Failure, CPLE_AppDefined,
                     "CommitTransaction() not supported: datasource opened as read-only");
        return OGRERR_FAILURE;
    }

    if( !bInTransaction )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                     "StartTransaction() has not yet been called");
        return OGRERR_FAILURE;
    }

    if (!osGlobalInsert.empty())
    {
        CPLString osPost = GetPostHeader();
        osPost += "  <wfs:Insert>\n";
        osPost += osGlobalInsert;
        osPost += "  </wfs:Insert>\n";
        osPost += "</wfs:Transaction>\n";

        bInTransaction = false;
        osGlobalInsert = "";
        int l_nExpectedInserts = nExpectedInserts;
        nExpectedInserts = 0;

        CPLDebug("WFS", "Post : %s", osPost.c_str());

        char** papszOptions = nullptr;
        papszOptions = CSLAddNameValue(papszOptions, "POSTFIELDS", osPost.c_str());
        papszOptions = CSLAddNameValue(papszOptions, "HEADERS",
                                    "Content-Type: application/xml; charset=UTF-8");

        CPLHTTPResult* psResult = poDS->HTTPFetch(poDS->GetPostTransactionURL(), papszOptions);
        CSLDestroy(papszOptions);

        if (psResult == nullptr)
        {
            return OGRERR_FAILURE;
        }

        if (strstr((const char*)psResult->pabyData,
                                        "<ServiceExceptionReport") != nullptr ||
            strstr((const char*)psResult->pabyData,
                                        "<ows:ExceptionReport") != nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s",
                    psResult->pabyData);
            CPLHTTPDestroyResult(psResult);
            return OGRERR_FAILURE;
        }

        CPLDebug("WFS", "Response: %s", psResult->pabyData);

        CPLXMLNode* psXML = CPLParseXMLString( (const char*) psResult->pabyData );
        if (psXML == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid XML content : %s",
                    psResult->pabyData);
            CPLHTTPDestroyResult(psResult);
            return OGRERR_FAILURE;
        }

        CPLStripXMLNamespace( psXML, nullptr, TRUE );
        bool bUse100Schema = false;
        CPLXMLNode* psRoot = CPLGetXMLNode( psXML, "=TransactionResponse" );
        if (psRoot == nullptr)
        {
            psRoot = CPLGetXMLNode( psXML, "=WFS_TransactionResponse" );
            if (psRoot)
                bUse100Schema = true;
        }

        if (psRoot == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot find <TransactionResponse>");
            CPLDestroyXMLNode( psXML );
            CPLHTTPDestroyResult(psResult);
            return OGRERR_FAILURE;
        }

        if( bUse100Schema )
        {
            if (CPLGetXMLNode( psRoot, "TransactionResult.Status.FAILED" ))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Insert failed : %s",
                        psResult->pabyData);
                CPLDestroyXMLNode( psXML );
                CPLHTTPDestroyResult(psResult);
                return OGRERR_FAILURE;
            }

            /* TODO */
        }
        else
        {
            int nGotInserted = atoi(CPLGetXMLValue(psRoot, "TransactionSummary.totalInserted", ""));
            if (nGotInserted != l_nExpectedInserts)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Only %d features were inserted whereas %d where expected",
                         nGotInserted, l_nExpectedInserts);
                CPLDestroyXMLNode( psXML );
                CPLHTTPDestroyResult(psResult);
                return OGRERR_FAILURE;
            }

            CPLXMLNode* psInsertResults =
                CPLGetXMLNode( psRoot, "InsertResults");
            if (psInsertResults == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Cannot find node InsertResults");
                CPLDestroyXMLNode( psXML );
                CPLHTTPDestroyResult(psResult);
                return OGRERR_FAILURE;
            }

            aosFIDList.resize(0);

            CPLXMLNode* psChild = psInsertResults->psChild;
            while(psChild)
            {
                const char* pszFID = CPLGetXMLValue(psChild, "FeatureId.fid", nullptr);
                if (pszFID == nullptr)
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Cannot find fid");
                    CPLDestroyXMLNode( psXML );
                    CPLHTTPDestroyResult(psResult);
                    return OGRERR_FAILURE;
                }
                aosFIDList.push_back(pszFID);

                psChild = psChild->psNext;
            }

            if ((int)aosFIDList.size() != nGotInserted)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Inconsistent InsertResults: did not get expected FID count");
                CPLDestroyXMLNode( psXML );
                CPLHTTPDestroyResult(psResult);
                return OGRERR_FAILURE;
            }
        }

        CPLDestroyXMLNode( psXML );
        CPLHTTPDestroyResult(psResult);
    }

    bInTransaction = false;
    osGlobalInsert = "";
    nExpectedInserts = 0;

    return OGRERR_NONE;
}

/************************************************************************/
/*                      RollbackTransaction()                           */
/************************************************************************/

OGRErr OGRWFSLayer::RollbackTransaction()
{
    if (!TestCapability(OLCTransactions))
    {
        if (!poDS->SupportTransactions())
            CPLError(CE_Failure, CPLE_AppDefined,
                     "RollbackTransaction() not supported: no WMS-T features advertized by server");
        else if (!poDS->UpdateMode())
            CPLError(CE_Failure, CPLE_AppDefined,
                     "RollbackTransaction() not supported: datasource opened as read-only");
        return OGRERR_FAILURE;
    }

    if( !bInTransaction )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                     "StartTransaction() has not yet been called");
        return OGRERR_FAILURE;
    }

    bInTransaction = false;
    osGlobalInsert = "";
    nExpectedInserts = 0;

    return OGRERR_NONE;
}

/************************************************************************/
/*                    SetRequiredOutputFormat()                         */
/************************************************************************/

void  OGRWFSLayer::SetRequiredOutputFormat(const char* pszRequiredOutputFormatIn)
{
    CPLFree(pszRequiredOutputFormat);
    if (pszRequiredOutputFormatIn)
    {
        pszRequiredOutputFormat = CPLStrdup(pszRequiredOutputFormatIn);
    }
    else
    {
        pszRequiredOutputFormat = nullptr;
    }
}

/************************************************************************/
/*                            SetOrderBy()                              */
/************************************************************************/

void OGRWFSLayer::SetOrderBy(const std::vector<OGRWFSSortDesc>& aoSortColumnsIn)
{
    aoSortColumns = aoSortColumnsIn;
}
