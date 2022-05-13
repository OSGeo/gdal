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

#include "ogr_gmlas.h"
#include "ogr_p.h"
#include "ogrgeojsonreader.h"
#include "cpl_time.h"

#include <algorithm>

CPL_CVSID("$Id$")

namespace GMLAS
{

/************************************************************************/
/*                          GMLASWriter                                 */
/************************************************************************/

typedef std::pair<CPLString, CPLString> PairNSElement;

typedef std::vector<PairNSElement> XPathComponents;

typedef std::pair<CPLString, CPLString> PairLayerNameColName;

class LayerDescription
{
    public:
        CPLString osName;
        CPLString osXPath;
        CPLString osPKIDName;
        CPLString osParentPKIDName;
        bool      bIsSelected;
        bool      bIsTopLevel;
        bool      bIsJunction;
        // map a field sequential number to a field
        std::map< int, GMLASField > oMapIdxToField;
        // map a field xpath to its sequential number
        std::map< CPLString, int > oMapFieldXPathToIdx;
        std::map< CPLString, int > oMapFieldNameToOGRIdx;
        std::vector<PairLayerNameColName> aoReferencingLayers;

        // NOTE: this doesn't scale to arbitrarily large datasets
        std::set<GIntBig> aoSetReferencedFIDs;

        LayerDescription(): bIsSelected(false),
                            bIsTopLevel(false),
                            bIsJunction(false) {}

        int GetOGRIdxFromFieldName( const CPLString& osFieldName ) const
        {
            const auto oIter = oMapFieldNameToOGRIdx.find(osFieldName);
            if( oIter == oMapFieldNameToOGRIdx.end() )
                return -1;
            return oIter->second;
        }
};

class GMLASWriter
{
        GMLASConfiguration m_oConf;
        CPLString       m_osFilename;
        CPLString       m_osGMLVersion;
        CPLString       m_osSRSNameFormat;
        CPLString       m_osEOL;
        GDALDataset*    m_poSrcDS;
        char**          m_papszOptions;
        VSILFILE*       m_fpXML;
        OGRGMLASDataSource *m_poTmpDS;
        OGRLayer           *m_poLayersMDLayer;
        OGRLayer           *m_poFieldsMDLayer;
        OGRLayer           *m_poLayerRelationshipsLayer;
        std::vector<LayerDescription> m_aoLayerDesc;
        std::map<CPLString, int> m_oMapLayerNameToIdx;
        std::map<CPLString, int> m_oMapXPathToIdx;
        std::map<CPLString, OGRLayer*> m_oMapLayerNameToLayer;
        std::map<CPLString, XPathComponents> m_oMapXPathToComponents;
        std::map<const OGRSpatialReference*, bool> m_oMapSRSToCoordSwap;

        CPLString           m_osTargetNameSpace;
        CPLString           m_osTargetNameSpacePrefix;

        CPLString           m_osIndentation;
        int                 m_nIndentLevel;

        void                IncIndent() { ++m_nIndentLevel; }
        void                DecIndent() { --m_nIndentLevel; }
        void                PrintIndent(VSILFILE* fp);

        void                PrintLine(VSILFILE* fp, const char *fmt, ...)
                                                CPL_PRINT_FUNC_FORMAT (3, 4);

        bool                WriteXSD(
                                const CPLString& osXSDFilenameIn,
                                const std::vector<PairURIFilename>& aoXSDs );
        bool                WriteXMLHeader(
                    bool bWFS2FeatureCollection,
                    GIntBig nTotalFeatures,
                    bool bGenerateXSD,
                    const CPLString& osXSDFilenameIn,
                    const std::vector<PairURIFilename>& aoXSDs,
                    const std::map<CPLString, CPLString>& oMapURIToPrefix );
        bool                CollectLayers();
        bool                CollectFields();
        bool                CollectRelationships();
        void                ComputeTopLevelFIDs();
        bool                WriteLayer( bool bWFS2FeatureCollection,
                                        const LayerDescription& oDesc,
                                        GIntBig& nFeaturesWritten,
                                        GIntBig nTotalTopLevelFeatures,
                                        GDALProgressFunc pfnProgress,
                                        void* pProgressData );

        bool                WriteFeature(
                        OGRFeature* poFeature,
                        const LayerDescription& oLayerDesc,
                        const std::set<CPLString>& oSetLayersInIteration,
                        const XPathComponents& aoInitialComponents,
                        const XPathComponents& aoPrefixComponents,
                        int nRecLevel);

        void                WriteClosingTags(
                                    size_t nCommonLength,
                                    const XPathComponents& aoCurComponents,
                                    const XPathComponents& aoNewComponents,
                                    bool bCurIsRegularField,
                                    bool bNewIsRegularField );

        void                WriteClosingAndStartingTags(
                        const XPathComponents& aoCurComponents,
                        const XPathComponents& aoNewComponents,
                        bool bCurIsRegularField );

        void     PrintMultipleValuesSeparator(
                                const GMLASField& oField,
                                const XPathComponents& aoFieldComponents);

        OGRLayer* GetFilteredLayer(OGRLayer* poSrcLayer,
                            const CPLString& osFilter,
                            const std::set<CPLString>& oSetLayersInIteration);
        void      ReleaseFilteredLayer(OGRLayer* poSrcLayer,
                                       OGRLayer* poIterLayer);

        bool     WriteFieldRegular(
                        OGRFeature* poFeature,
                        const GMLASField& oField,
                        const LayerDescription& oLayerDesc,
                        /*XPathComponents& aoLayerComponents,*/
                        XPathComponents& aoCurComponents,
                        const XPathComponents& aoPrefixComponents,
                        /*const std::set<CPLString>& oSetLayersInIteration,*/
                        bool& bAtLeastOneFieldWritten,
                        bool& bCurIsRegularField);

        bool     WriteFieldNoLink(
                        OGRFeature* poFeature,
                        const GMLASField& oField,
                        const LayerDescription& oLayerDesc,
                        XPathComponents& aoLayerComponents,
                        XPathComponents& aoCurComponents,
                        const XPathComponents& aoPrefixComponents,
                        const std::set<CPLString>& oSetLayersInIteration,
                        int nRecLevel,
                        bool& bAtLeastOneFieldWritten,
                        bool& bCurIsRegularField);

        bool     WriteFieldWithLink(
                        OGRFeature* poFeature,
                        const GMLASField& oField,
                        const LayerDescription& oLayerDesc,
                        XPathComponents& aoLayerComponents,
                        XPathComponents& aoCurComponents,
                        const XPathComponents& aoPrefixComponents,
                        const std::set<CPLString>& oSetLayersInIteration,
                        int nRecLevel,
                        bool& bAtLeastOneFieldWritten,
                        bool& bCurIsRegularField);

        bool     WriteFieldJunctionTable(
                        OGRFeature* poFeature,
                        const GMLASField& oField,
                        const LayerDescription& oLayerDesc,
                        XPathComponents& aoLayerComponents,
                        XPathComponents& aoCurComponents,
                        const XPathComponents& aoPrefixComponents,
                        const std::set<CPLString>& oSetLayersInIteration,
                        int nRecLevel,
                        bool& bAtLeastOneFieldWritten,
                        bool& bCurIsRegularField);

        void Close();

        OGRLayer*              GetLayerByName(const CPLString& osName);

        const XPathComponents& SplitXPath( const CPLString& osXPath );

        bool                   GetCoordSwap( const OGRSpatialReference* poSRS );

    public:
        GMLASWriter(const char * pszFilename,
                    GDALDataset *poSrcDS,
                    char** papszOptions);
        ~GMLASWriter();

        bool Write(GDALProgressFunc pfnProgress,
                   void * pProgressData);
};

/************************************************************************/
/*                            GMLASWriter()                             */
/************************************************************************/

GMLASWriter::GMLASWriter(const char * pszFilename,
                         GDALDataset *poSrcDS,
                         char** papszOptions)
    : m_osFilename(pszFilename)
#ifdef WIN32
    , m_osEOL("\r\n")
#else
    , m_osEOL("\n")
#endif
    , m_poSrcDS(poSrcDS)
    , m_papszOptions(CSLDuplicate(papszOptions))
    , m_fpXML(nullptr)
    , m_poTmpDS(nullptr)
    , m_poLayersMDLayer(nullptr)
    , m_poFieldsMDLayer(nullptr)
    , m_poLayerRelationshipsLayer(nullptr)
    , m_osTargetNameSpace(szOGRGMLAS_URI)
    , m_osTargetNameSpacePrefix(szOGRGMLAS_PREFIX)
    , m_osIndentation(std::string(INDENT_SIZE_DEFAULT, ' '))
    , m_nIndentLevel(0)
{

}

/************************************************************************/
/*                           ~GMLASWriter()                             */
/************************************************************************/

GMLASWriter::~GMLASWriter()
{
    CSLDestroy(m_papszOptions);
    Close();
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

void GMLASWriter::Close()
{
    if( m_fpXML != nullptr )
        VSIFCloseL( m_fpXML );
    m_fpXML = nullptr;
    delete m_poTmpDS;
    m_poTmpDS = nullptr;
}

/************************************************************************/
/*                              Write()                                 */
/************************************************************************/

bool GMLASWriter::Write(GDALProgressFunc pfnProgress,
                        void * pProgressData)
{
    if( m_poSrcDS->GetLayerCount() == 0 &&
        m_poSrcDS->GetLayerByName(szOGR_OTHER_METADATA) == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Source dataset has no layers");
        return false;
    }

    // Load configuration file
    CPLString osConfigFile = CSLFetchNameValueDef(m_papszOptions,
                                                  szCONFIG_FILE_OPTION, "");
    if( osConfigFile.empty() )
    {
        const char* pszConfigFile = CPLFindFile("gdal",
                                                szDEFAULT_CONF_FILENAME);
        if( pszConfigFile )
            osConfigFile = pszConfigFile;
    }
    if( osConfigFile.empty() )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "No configuration file found. Using hard-coded defaults");
        m_oConf.Finalize();
    }
    else
    {
        if( !m_oConf.Load(osConfigFile) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Loading of configuration failed");
            return false;
        }
    }

    CPLString osXSDFilenames = CSLFetchNameValueDef(m_papszOptions,
                                                    szINPUT_XSD_OPTION, "");
    std::vector<PairURIFilename> aoXSDs;
    std::map<CPLString, CPLString> oMapURIToPrefix;
    CPLString osGMLVersion;

    if( !osXSDFilenames.empty() )
    {
        // Create a fake GMLAS dataset from the XSD= value
        m_poTmpDS = new OGRGMLASDataSource();
        GDALOpenInfo oOpenInfo(szGMLAS_PREFIX, GA_ReadOnly );
        oOpenInfo.papszOpenOptions = CSLSetNameValue(oOpenInfo.papszOpenOptions,
                                                     szXSD_OPTION,
                                                     osXSDFilenames);
        bool bRet = m_poTmpDS->Open(&oOpenInfo);
        CSLDestroy(oOpenInfo.papszOpenOptions);
        oOpenInfo.papszOpenOptions = nullptr;
        if( !bRet )
        {
            return false;
        }
    }

    GDALDataset* poQueryDS = m_poTmpDS ? m_poTmpDS : m_poSrcDS;

    // No explicit XSD creation option, then we assume that the source
    // dataset contains all the metadata layers we need
    OGRLayer* poOtherMetadataLayer =
                        poQueryDS->GetLayerByName(szOGR_OTHER_METADATA);
    if( poOtherMetadataLayer == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot establish schema since no %s creation option "
                    "specified and no %s found in source "
                    "dataset. One of them must be defined.",
                    szINPUT_XSD_OPTION,
                    szOGR_OTHER_METADATA);
        return false;
    }

    m_poLayersMDLayer =
        poQueryDS->GetLayerByName(szOGR_LAYERS_METADATA);
    m_poFieldsMDLayer =
        poQueryDS->GetLayerByName(szOGR_FIELDS_METADATA);
    m_poLayerRelationshipsLayer =
        poQueryDS->GetLayerByName(szOGR_LAYER_RELATIONSHIPS);
    if( m_poLayersMDLayer == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s not found",
                 szOGR_LAYERS_METADATA);
        return false;
    }
    if( m_poFieldsMDLayer == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s not found",
                 szOGR_FIELDS_METADATA);
        return false;
    }
    if( m_poLayerRelationshipsLayer == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s not found",
                 szOGR_LAYER_RELATIONSHIPS);
        return false;
    }

    std::map<int, CPLString> oMapToUri;
    std::map<int, CPLString> oMapToLocation;
    std::map<int, CPLString> oMapToPrefix;
    while( true )
    {
        OGRFeature* poFeature = poOtherMetadataLayer->GetNextFeature();
        if( poFeature == nullptr )
            break;
        const char* pszKey = poFeature->GetFieldAsString(szKEY);
        int i = 0;
        if( sscanf( pszKey, szNAMESPACE_URI_FMT, &i ) == 1 && i > 0 )
        {
            oMapToUri[i] = poFeature->GetFieldAsString(szVALUE);
        }
        else if( sscanf( pszKey, szNAMESPACE_LOCATION_FMT, &i ) == 1 &&
                    i > 0 )
        {
            oMapToLocation[i] = poFeature->GetFieldAsString(szVALUE);
        }
        else if( sscanf( pszKey, szNAMESPACE_PREFIX_FMT, &i ) == 1 &&
                    i > 0 )
        {
            oMapToPrefix[i] = poFeature->GetFieldAsString(szVALUE);
        }
        else if( EQUAL(pszKey, szGML_VERSION) )
        {
            osGMLVersion = poFeature->GetFieldAsString(szVALUE);
        }
        delete poFeature;
    }
    poOtherMetadataLayer->ResetReading();

    for( int i = 1; i <= static_cast<int>(oMapToUri.size()); ++i )
    {
        if( oMapToUri.find(i) != oMapToUri.end() )
        {
            const CPLString& osURI( oMapToUri[i] );
            aoXSDs.push_back( PairURIFilename( osURI,
                                                oMapToLocation[i] ) );
            if( oMapToPrefix.find(i) != oMapToPrefix.end() )
            {
                oMapURIToPrefix[ osURI ] = oMapToPrefix[i];
            }
        }
    }

    if( !CollectLayers() )
        return false;

    if( !CollectFields() )
        return false;

    if( !CollectRelationships() )
        return false;

    const char* pszLayers = CSLFetchNameValue(m_papszOptions, szLAYERS_OPTION);
    if( pszLayers )
    {
        for( const auto& oLayerIter: m_oMapLayerNameToIdx )
        {
            LayerDescription& oDesc = m_aoLayerDesc[oLayerIter.second];
            oDesc.bIsSelected = false;
        }

        char** papszLayers = CSLTokenizeString2(pszLayers, ",", 0);
        for( char** papszIter = papszLayers; *papszIter != nullptr; ++papszIter )
        {
            if( EQUAL(*papszIter, "{SPATIAL_LAYERS}") )
            {
                for( const auto& oLayerIter: m_oMapLayerNameToIdx )
                {
                    LayerDescription& oDesc = m_aoLayerDesc[oLayerIter.second];
                    if( oDesc.bIsTopLevel )
                    {
                        bool bIsGeometric = false;
                        for( const auto& oFieldIter: oDesc.oMapIdxToField )
                        {
                            if( oFieldIter.second.GetType() ==
                                                            GMLAS_FT_GEOMETRY )
                            {
                                bIsGeometric = true;
                                break;
                            }
                        }
                        oDesc.bIsSelected = bIsGeometric;
                    }
                }
            }
            else
            {
                const auto oLayerIter = m_oMapLayerNameToIdx.find(*papszIter);
                if( oLayerIter == m_oMapLayerNameToIdx.end() )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Layer %s specified in LAYERS option "
                             "does not exist",
                             *papszIter);
                    CSLDestroy(papszLayers);
                    return false;
                }
                else
                {
                    LayerDescription& oDesc = m_aoLayerDesc[oLayerIter->second];
                    oDesc.bIsSelected = true;
                }
            }
        }
        CSLDestroy(papszLayers);
    }
    else
    {
        ComputeTopLevelFIDs();
    }

    const bool bWFS2FeatureCollection = EQUAL(
        CSLFetchNameValueDef( m_papszOptions, szWRAPPING_OPTION,
                              m_oConf.m_osWrapping),
        szWFS2_FEATURECOLLECTION);

    if( pfnProgress == GDALDummyProgress )
        pfnProgress = nullptr;
    // Compute total number of top level features
    GIntBig nTotalTopLevelFeatures = -1;
    if( pfnProgress != nullptr || bWFS2FeatureCollection )
    {
        nTotalTopLevelFeatures = 0;
        for( const auto& oLayerIter: m_oMapLayerNameToIdx )
        {
            const LayerDescription& oDesc = m_aoLayerDesc[oLayerIter.second];
            OGRLayer* poSrcLayer = m_poSrcDS->GetLayerByName(oDesc.osName);
            if( oDesc.bIsSelected && poSrcLayer != nullptr )
            {
                nTotalTopLevelFeatures += poSrcLayer->GetFeatureCount(true);
                nTotalTopLevelFeatures -=
                        static_cast<GIntBig>(oDesc.aoSetReferencedFIDs.size());
            }
        }
        CPLDebug("GMLAS", CPL_FRMT_GIB " top level features to be written",
                 nTotalTopLevelFeatures);
    }

    // Now read options related to writing
    int nIndentSize = std::min(INDENT_SIZE_MAX,
        std::max(INDENT_SIZE_MIN,
            atoi(CSLFetchNameValueDef(m_papszOptions, szINDENT_SIZE_OPTION,
                                  CPLSPrintf("%d", m_oConf.m_nIndentSize)))));
    m_osIndentation.assign( nIndentSize, ' ' );

    if( oMapURIToPrefix.find( szGML32_URI ) != oMapURIToPrefix.end()
        ||
        // Used by tests
        oMapURIToPrefix.find( "http://fake_gml32" ) != oMapURIToPrefix.end() )
    {
        m_osGMLVersion = "3.2.1";
    }
    else
    {
        m_osGMLVersion = osGMLVersion;
    }

    m_osSRSNameFormat = CSLFetchNameValueDef(m_papszOptions,
                                             szSRSNAME_FORMAT_OPTION,
                                             m_oConf.m_osSRSNameFormat);

    CPLString osLineFormat = CSLFetchNameValueDef(m_papszOptions,
                                                  szLINEFORMAT_OPTION,
                                                  m_oConf.m_osLineFormat);
    if( !osLineFormat.empty() )
    {
        if( EQUAL(osLineFormat, szCRLF) )
            m_osEOL = "\r\n";
        else if( EQUAL(osLineFormat, szLF) )
            m_osEOL = "\n";
    }

    CPLString osOutXSDFilename =
        CSLFetchNameValueDef( m_papszOptions,
                              szOUTPUT_XSD_FILENAME_OPTION, "" );
    const bool bGenerateXSD = !bWFS2FeatureCollection &&
        (m_osFilename != "/vsistdout/" || !osOutXSDFilename.empty()) &&
        CPLFetchBool( m_papszOptions, szGENERATE_XSD_OPTION, true );

    // Write .xsd
    if( bWFS2FeatureCollection )
        VSIUnlink( CPLResetExtension( m_osFilename, "xsd" ) );
    else if( bGenerateXSD && !WriteXSD( osOutXSDFilename, aoXSDs ) )
        return false;

    // Write .xml header
    if( !WriteXMLHeader( bWFS2FeatureCollection,
                         nTotalTopLevelFeatures,
                         bGenerateXSD,
                         osOutXSDFilename,
                         aoXSDs, oMapURIToPrefix ) )
        return false;

    // Iterate over layers
    GIntBig nFeaturesWritten = 0;
    bool bRet = true;
    for(const auto& oLayerIter : m_oMapLayerNameToIdx )
    {
        if( m_aoLayerDesc[oLayerIter.second].bIsSelected )
        {
            bRet = WriteLayer( bWFS2FeatureCollection,
                               m_aoLayerDesc[oLayerIter.second],
                               nFeaturesWritten,
                               nTotalTopLevelFeatures,
                               pfnProgress, pProgressData );
            if( !bRet )
                break;
        }
    }
    CPLDebug("GMLAS", CPL_FRMT_GIB " top level features written",
             nFeaturesWritten);

    // Epilogue of .xml file
    if( bWFS2FeatureCollection )
    {
        PrintLine( m_fpXML, "</%s:%s>",
                   szWFS_PREFIX,
                   szFEATURE_COLLECTION );
    }
    else
    {
        PrintLine( m_fpXML, "</%s:%s>",
                   m_osTargetNameSpacePrefix.c_str(),
                   szFEATURE_COLLECTION );
    }

    Close();
    return bRet;
}

/************************************************************************/
/*                           GetLayerByName()                           */
/************************************************************************/

// Mostly equivalent to m_poSrcDS->GetLayerByName(), except that we use
// a map to cache instead of linear search.
OGRLayer* GMLASWriter::GetLayerByName(const CPLString& osName)
{
    const auto oIter = m_oMapLayerNameToLayer.find(osName);
    if( oIter == m_oMapLayerNameToLayer.end() )
    {
        OGRLayer* poLayer = m_poSrcDS->GetLayerByName(osName);
        m_oMapLayerNameToLayer[osName] = poLayer;
        return poLayer;
    }
    return oIter->second;
}

/************************************************************************/
/*                            XMLEscape()                               */
/************************************************************************/

static CPLString XMLEscape(const CPLString& osStr)
{
    char* pszEscaped = CPLEscapeString( osStr, -1, CPLES_XML );
    CPLString osRet(pszEscaped);
    CPLFree(pszEscaped);
    return osRet;
}

/************************************************************************/
/*                            WriteXSD()                                */
/************************************************************************/

bool GMLASWriter::WriteXSD( const CPLString& osXSDFilenameIn,
                            const std::vector<PairURIFilename>& aoXSDs )
{
    const CPLString osXSDFilename(
        !osXSDFilenameIn.empty() ? osXSDFilenameIn :
                CPLString(CPLResetExtension( m_osFilename, "xsd" )) );
    VSILFILE* fpXSD = VSIFOpenL( osXSDFilename, "wb" );
    if( fpXSD == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot create %s", osXSDFilename.c_str());
        return false;
    }

    PrintLine( fpXSD, "<?xml version=\"1.0\" encoding=\"utf-8\" ?>" );
    PrintLine( fpXSD,
               "<xs:schema ");
    PrintLine( fpXSD,
               "    targetNamespace=\"%s\"",
               XMLEscape(m_osTargetNameSpace).c_str() );
    PrintLine( fpXSD,
               "    xmlns:%s=\"%s\"",
               m_osTargetNameSpacePrefix.c_str(),
               XMLEscape(m_osTargetNameSpace).c_str() );
    PrintLine( fpXSD,
               "    xmlns:xs=\"%s\"",
               szXS_URI);
    PrintLine( fpXSD,
               "    elementFormDefault=\"qualified\" version=\"1.0\" >");

    // Those imports are not really needed, since the schemaLocation are
    // already specified in the .xml file, but that helps validating the
    // document with libxml2/xmllint since it can only accept one single main
    // schema.
    for( size_t i = 0; i < aoXSDs.size(); ++i )
    {
        if( !aoXSDs[i].second.empty() )
        {
            if( !aoXSDs[i].first.empty() )
            {
                PrintLine( fpXSD,
                        "<xs:import namespace=\"%s\" schemaLocation=\"%s\"/>",
                        XMLEscape(aoXSDs[i].first).c_str(),
                        XMLEscape(aoXSDs[i].second).c_str() );
            }
            else
            {
                PrintLine( fpXSD,
                        "<xs:import schemaLocation=\"%s\"/>",
                        XMLEscape(aoXSDs[i].second).c_str() );
            }
        }
    }

    PrintLine( fpXSD,
               "<xs:element name=\"%s\" "
                           "type=\"%s:%sType\"/>",
               szFEATURE_COLLECTION,
               m_osTargetNameSpacePrefix.c_str(),
               szFEATURE_COLLECTION);

    PrintLine( fpXSD, "<xs:complexType name=\"%sType\">",
               szFEATURE_COLLECTION );
    PrintLine( fpXSD, "  <xs:sequence>" );
    PrintLine( fpXSD, "    <xs:element name=\"%s\" "
                                "minOccurs=\"0\" maxOccurs=\"unbounded\">",
               szFEATURE_MEMBER );
    PrintLine( fpXSD, "      <xs:complexType>" );
    PrintLine( fpXSD, "        <xs:sequence>" );
    PrintLine( fpXSD, "           <xs:any/>" );
    PrintLine( fpXSD, "        </xs:sequence>" );
    PrintLine( fpXSD, "      </xs:complexType>" );
    PrintLine( fpXSD, "    </xs:element>" );
    PrintLine( fpXSD, "  </xs:sequence>" );
    PrintLine( fpXSD, "</xs:complexType>" );
    PrintLine( fpXSD,
            "</xs:schema>");

    VSIFCloseL( fpXSD );

    return true;
}

/************************************************************************/
/*                         WriteXMLHeader()                             */
/************************************************************************/

bool GMLASWriter::WriteXMLHeader(
                        bool bWFS2FeatureCollection,
                        GIntBig nTotalFeatures,
                        bool bGenerateXSD,
                        const CPLString& osXSDFilenameIn,
                        const std::vector<PairURIFilename>& aoXSDs,
                        const std::map<CPLString, CPLString>& oMapURIToPrefix )
{
    m_fpXML = VSIFOpenL( m_osFilename, "wb" );
    if( m_fpXML == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot create %s", m_osFilename.c_str());
        return false;
    }

    // Delete potentially existing .gfs file
    VSIUnlink( CPLResetExtension( m_osFilename, "gfs" ) );

    std::map<CPLString, CPLString> aoWrittenPrefixes;
    aoWrittenPrefixes[szXSI_PREFIX] = szXSI_URI;

    PrintLine( m_fpXML, "<?xml version=\"1.0\" encoding=\"utf-8\" ?>" );
    if( bWFS2FeatureCollection )
    {
        PrintLine( m_fpXML, "<%s:%s",
                    szWFS_PREFIX,
                    szFEATURE_COLLECTION );

        const CPLString osTimestamp( CSLFetchNameValueDef(m_papszOptions,
                                                    szTIMESTAMP_OPTION,
                                                    m_oConf.m_osTimestamp) );
        if( osTimestamp.empty() )
        {
            struct tm sTime;
            CPLUnixTimeToYMDHMS(time(nullptr), &sTime);
            PrintLine( m_fpXML,
                       "    timeStamp=\"%04d-%02d-%02dT%02d:%02d:%02dZ\"",
                       sTime.tm_year + 1900,
                       sTime.tm_mon + 1,
                       sTime.tm_mday,
                       sTime.tm_hour,
                       sTime.tm_min,
                       sTime.tm_sec );
        }
        else
        {
            PrintLine( m_fpXML,
                       "    timeStamp=\"%s\"", osTimestamp.c_str() );
        }
        PrintLine( m_fpXML,
                   "    numberMatched=\"unknown\"" );
        PrintLine( m_fpXML,
                   "    numberReturned=\"" CPL_FRMT_GIB "\"",
                   nTotalFeatures );
        PrintLine( m_fpXML,
                   "    xmlns:%s=\"%s\"",
                   szWFS_PREFIX,
                   szWFS20_URI );
        aoWrittenPrefixes[szWFS_PREFIX] = szWFS20_URI;
    }
    else
    {
        PrintLine( m_fpXML, "<%s:%s",
                    m_osTargetNameSpacePrefix.c_str(),
                    szFEATURE_COLLECTION );
        PrintLine( m_fpXML,
                   "    xmlns:%s=\"%s\"",
                   m_osTargetNameSpacePrefix.c_str(),
                   XMLEscape(m_osTargetNameSpace).c_str() );
    }
    PrintLine( m_fpXML,
               "    xmlns:%s=\"%s\"",
               szXSI_PREFIX, szXSI_URI);

    CPLString osSchemaURI;
    if( bWFS2FeatureCollection )
    {
        const CPLString osWFS20SchemaLocation(
          CSLFetchNameValueDef(m_papszOptions, szWFS20_SCHEMALOCATION_OPTION,
                               m_oConf.m_osWFS20SchemaLocation) );
        osSchemaURI += szWFS20_URI;
        osSchemaURI += " ";
        osSchemaURI += osWFS20SchemaLocation;
    }
    else if( bGenerateXSD || !osXSDFilenameIn.empty() )
    {
        const CPLString osXSDFilename(
            !osXSDFilenameIn.empty() ? osXSDFilenameIn :
            CPLString( CPLGetFilename(
                                CPLResetExtension( m_osFilename, "xsd" )) ) );
        osSchemaURI += m_osTargetNameSpace;
        osSchemaURI += " ";
        osSchemaURI += osXSDFilename;
    }

    for( size_t i = 0; i < aoXSDs.size(); ++i )
    {
        const CPLString& osURI( aoXSDs[i].first );
        const CPLString& osLocation( aoXSDs[i].second );

        CPLString osPrefix;
        if( !osURI.empty() )
        {
            const auto oIter = oMapURIToPrefix.find(osURI);
            if( oIter != oMapURIToPrefix.end() )
            {
                osPrefix = oIter->second;
            }
        }
        if( !osPrefix.empty() )
        {
            const auto& oIter = aoWrittenPrefixes.find( osPrefix );
            if( oIter != aoWrittenPrefixes.end() )
            {
                if( oIter->second != osURI )
                {
                    CPLDebug("GMLAS",
                             "Namespace prefix %s already defined as URI %s "
                             "but now redefefined as %s. Skipped",
                             osPrefix.c_str(),
                             oIter->second.c_str(),
                             osURI.c_str());
                }
                continue;
            }
            aoWrittenPrefixes[osPrefix] = osURI;
        }

        if( osURI.empty() )
        {
            if( !osLocation.empty() )
            {
                PrintLine( m_fpXML,
                           "    xsi:%s=\"%s\"",
                           szNO_NAMESPACE_SCHEMA_LOCATION,
                           XMLEscape(osLocation).c_str() );
            }
        }
        else
        {
            if( osPrefix.empty() )
            {
                osPrefix = CPLSPrintf("ns%d", static_cast<int>(i));
            }

            PrintLine( m_fpXML,
                       "    xmlns:%s=\"%s\"",
                       osPrefix.c_str(),
                       XMLEscape(osURI).c_str() );

            if( !osLocation.empty() )
            {
                if( !osSchemaURI.empty() )
                    osSchemaURI += " ";
                osSchemaURI += osURI;
                osSchemaURI += " ";
                osSchemaURI += osLocation;
            }
        }
    }

    if( !osSchemaURI.empty() )
    {
        PrintLine( m_fpXML,
                "    xsi:%s=\"%s\" >",
                szSCHEMA_LOCATION,
                XMLEscape(osSchemaURI).c_str() );
    }

    // Write optional user comment
    CPLString osComment(CSLFetchNameValueDef(
                                    m_papszOptions, szCOMMENT_OPTION,
                                    m_oConf.m_osComment));
    if( !osComment.empty() )
    {
        while( true )
        {
            const size_t nSizeBefore = osComment.size();
            osComment.replaceAll("--", "- -");
            if( nSizeBefore == osComment.size() )
                break;
        }
        PrintLine( m_fpXML, "<!-- %s -->", osComment.c_str() );
    }

    return true;
}

/************************************************************************/
/*                          CollectLayers()                             */
/************************************************************************/

bool GMLASWriter::CollectLayers()
{
    OGRFeatureDefn* poFDefn = m_poLayersMDLayer->GetLayerDefn();
    const char* const apszFields[] = { szLAYER_NAME,
                                       szLAYER_XPATH,
                                       szLAYER_CATEGORY,
                                       szLAYER_PKID_NAME,
                                       szLAYER_PARENT_PKID_NAME };
    for( size_t i = 0; i < CPL_ARRAYSIZE(apszFields); ++i )
    {
        if( poFDefn->GetFieldIndex(apszFields[i]) < 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find field %s in %s layer",
                     apszFields[i],
                     m_poLayersMDLayer->GetName());
            return false;
        }
    }

    m_poLayersMDLayer->SetAttributeFilter(nullptr);
    m_poLayersMDLayer->ResetReading();
    while( true )
    {
        OGRFeature* poFeature = m_poLayersMDLayer->GetNextFeature();
        if( poFeature == nullptr )
            break;
        LayerDescription desc;
        desc.osName = poFeature->GetFieldAsString(szLAYER_NAME);
        desc.osXPath = poFeature->GetFieldAsString(szLAYER_XPATH);
        desc.osPKIDName = poFeature->GetFieldAsString(szLAYER_PKID_NAME);
        desc.osParentPKIDName = poFeature->GetFieldAsString(
                                                    szLAYER_PARENT_PKID_NAME);
        desc.bIsTopLevel = EQUAL( poFeature->GetFieldAsString(szLAYER_CATEGORY),
                                  szTOP_LEVEL_ELEMENT );
        desc.bIsSelected = desc.bIsTopLevel;
        desc.bIsJunction = EQUAL( poFeature->GetFieldAsString(szLAYER_CATEGORY),
                                  szJUNCTION_TABLE );
        delete poFeature;

        OGRLayer* poLyr = GetLayerByName(desc.osName);
        if( poLyr )
        {
            if( !desc.osPKIDName.empty() )
                desc.oMapFieldNameToOGRIdx[desc.osPKIDName] =
                        poLyr->GetLayerDefn()->GetFieldIndex(desc.osPKIDName);
            if( !desc.osParentPKIDName.empty() )
                desc.oMapFieldNameToOGRIdx[desc.osParentPKIDName] =
                    poLyr->GetLayerDefn()->GetFieldIndex(desc.osParentPKIDName);
        }

        m_aoLayerDesc.push_back(desc);
        if( m_oMapLayerNameToIdx.find(desc.osName) !=
                                                    m_oMapLayerNameToIdx.end() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Several layers with same %s = %s",
                     szLAYER_NAME,
                     desc.osName.c_str() );
            return false;
        }
        if( !desc.bIsJunction &&
            m_oMapXPathToIdx.find(desc.osXPath) != m_oMapXPathToIdx.end() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Several layers with same %s = %s",
                     szLAYER_XPATH,
                     desc.osXPath.c_str() );
            return false;
        }
        const int nIdx = static_cast<int>(m_aoLayerDesc.size() - 1);
        m_oMapLayerNameToIdx[ desc.osName ] = nIdx;
        if( !desc.bIsJunction )
            m_oMapXPathToIdx[ desc.osXPath ] = nIdx;
    }
    m_poLayersMDLayer->ResetReading();

    return true;
}

/************************************************************************/
/*                          CollectFields()                             */
/************************************************************************/

bool GMLASWriter::CollectFields()
{
    OGRFeatureDefn* poFDefn = m_poFieldsMDLayer->GetLayerDefn();
    const char* const apszFields[] = { szLAYER_NAME,
                                       szFIELD_INDEX,
                                       szFIELD_NAME,
                                       szFIELD_TYPE,
                                       szFIELD_XPATH,
                                       szFIELD_CATEGORY,
                                       szFIELD_RELATED_LAYER,
                                       szFIELD_JUNCTION_LAYER,
                                       szFIELD_IS_LIST,
                                       szFIELD_MIN_OCCURS,
                                       szFIELD_MAX_OCCURS,
                                       szFIELD_REPETITION_ON_SEQUENCE,
                                       szFIELD_DEFAULT_VALUE };
    for( size_t i = 0; i < CPL_ARRAYSIZE(apszFields); ++i )
    {
        if( poFDefn->GetFieldIndex(apszFields[i]) < 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find field %s in %s layer",
                     apszFields[i],
                     m_poFieldsMDLayer->GetName());
            return false;
        }
    }

    m_poFieldsMDLayer->SetAttributeFilter(
        (CPLString(szFIELD_CATEGORY) + " != '" + szSWE_FIELD + "'").c_str() );
    m_poFieldsMDLayer->ResetReading();
    while( true )
    {
        OGRFeature* poFeature = m_poFieldsMDLayer->GetNextFeature();
        if( poFeature == nullptr )
            break;

        GMLASField oField;

        oField.SetName( poFeature->GetFieldAsString( szFIELD_NAME ) );

        CPLString osLayerName( poFeature->GetFieldAsString( szLAYER_NAME ) );
        const auto& oIterToIdx = m_oMapLayerNameToIdx.find(osLayerName);
        if( oIterToIdx == m_oMapLayerNameToIdx.end() )
        {
            // Shouldn't happen for well behaved metadata
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Cannot find in %s layer %s, "
                     "referenced in %s by field %s",
                     szOGR_LAYERS_METADATA,
                     osLayerName.c_str(),
                     szOGR_FIELDS_METADATA,
                     oField.GetName().c_str());
            delete poFeature;
            continue;
        }
        if( m_aoLayerDesc[oIterToIdx->second].bIsJunction )
        {
            delete poFeature;
            continue;
        }

        CPLString osXPath( poFeature->GetFieldAsString( szFIELD_XPATH ) );
        oField.SetXPath( osXPath );

        CPLString osType( poFeature->GetFieldAsString( szFIELD_TYPE ) );
        if( !osType.empty() )
        {
            if( osType == szFAKEXS_JSON_DICT )
                oField.SetType( GMLAS_FT_STRING, osType );
            else if( osType == szFAKEXS_GEOMETRY )
            {
                oField.SetType( GMLAS_FT_GEOMETRY, osType );
                // Hack for geometry field that have a xpath like
                // foo/bar/gml:Point,foo/bar/gml:LineString,...
                size_t nPos = osXPath.find("/gml:Point,");
                if( nPos != std::string::npos )
                    osXPath.resize(nPos);
                oField.SetXPath( osXPath );
            }
            else
                oField.SetType( GMLASField::GetTypeFromString(osType), osType );
        }

        CPLString osCategory( poFeature->GetFieldAsString( szFIELD_CATEGORY ) );
        if( osCategory == szREGULAR )
        {
            oField.SetCategory( GMLASField::REGULAR );
        }
        else if( osCategory == szPATH_TO_CHILD_ELEMENT_NO_LINK )
        {
            oField.SetCategory( GMLASField::PATH_TO_CHILD_ELEMENT_NO_LINK );
        }
        else if( osCategory == szPATH_TO_CHILD_ELEMENT_WITH_LINK )
        {
            oField.SetCategory( GMLASField::PATH_TO_CHILD_ELEMENT_WITH_LINK );
        }
        else if( osCategory == szPATH_TO_CHILD_ELEMENT_WITH_JUNCTION_TABLE )
        {
            oField.SetCategory( GMLASField::
                                    PATH_TO_CHILD_ELEMENT_WITH_JUNCTION_TABLE );

            CPLString osJunctionLayer( poFeature->GetFieldAsString(
                                                    szFIELD_JUNCTION_LAYER ) );
            if( osJunctionLayer.empty() )
            {
                // Shouldn't happen for well behaved metadata
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Missing value for %s for field (%s,%s)",
                         szFIELD_JUNCTION_LAYER,
                         osLayerName.c_str(),
                         oField.GetName().c_str());
                delete poFeature;
                continue;
            }
            oField.SetJunctionLayer(osJunctionLayer);
        }
        else if( osCategory == szGROUP )
        {
            oField.SetCategory( GMLASField::GROUP );
        }
        else
        {
            // Shouldn't happen for well behaved metadata
            CPLError(CE_Warning, CPLE_AppDefined,
                        "Unknown category = %s for field (%s,%s)",
                        osCategory.c_str(),
                        osLayerName.c_str(),
                        oField.GetName().c_str());
            delete poFeature;
            continue;
        }

        CPLString osRelatedLayer( poFeature->GetFieldAsString(
                                                    szFIELD_RELATED_LAYER ) );
        if( !osRelatedLayer.empty() &&
            m_oMapLayerNameToIdx.find( osRelatedLayer ) !=
                                                m_oMapLayerNameToIdx.end() )
        {
            oField.SetRelatedClassXPath(
                m_aoLayerDesc[m_oMapLayerNameToIdx[ osRelatedLayer ]].osXPath );
        }

        oField.SetList( CPL_TO_BOOL(
                            poFeature->GetFieldAsInteger(szFIELD_IS_LIST) ) );

        oField.SetMinOccurs( poFeature->GetFieldAsInteger(szFIELD_MIN_OCCURS) );
        oField.SetMaxOccurs( poFeature->GetFieldAsInteger(szFIELD_MAX_OCCURS) );
        oField.SetRepetitionOnSequence( CPL_TO_BOOL(
            poFeature->GetFieldAsInteger(szFIELD_REPETITION_ON_SEQUENCE) ) );
        oField.SetDefaultValue( poFeature->GetFieldAsString(
                                                szFIELD_DEFAULT_VALUE) );

        const int nIdx = poFeature->GetFieldAsInteger( szFIELD_INDEX );
        delete poFeature;

        const int nLayerIdx = m_oMapLayerNameToIdx[osLayerName];
        LayerDescription& oLayerDesc = m_aoLayerDesc[nLayerIdx];
        if( oLayerDesc.oMapIdxToField.find(nIdx) !=
                                            oLayerDesc.oMapIdxToField.end() )
        {
            // Shouldn't happen for well behaved metadata
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Field %s of %s has the same index as field %s",
                      oField.GetName().c_str(),
                      osLayerName.c_str(),
                      oLayerDesc.oMapIdxToField[nIdx].GetName().c_str() );
            return false;
        }
        oLayerDesc.oMapIdxToField[ nIdx ] = oField;

        if( !oField.GetXPath().empty() )
        {
            if( oLayerDesc.oMapFieldXPathToIdx.find(oField.GetXPath()) !=
                        oLayerDesc.oMapFieldXPathToIdx.end() )
            {
                // Shouldn't happen for well behaved metadata
                CPLError( CE_Failure, CPLE_AppDefined,
                        "Field %s of %s has the same XPath as field %s",
                        oField.GetName().c_str(),
                        osLayerName.c_str(),
                        oLayerDesc.oMapIdxToField[
                            oLayerDesc.oMapFieldXPathToIdx[oField.GetXPath()]].
                                GetName().c_str() );
                return false;
            }
            oLayerDesc.oMapFieldXPathToIdx[oField.GetXPath()] = nIdx;
        }

        OGRLayer* poLyr = GetLayerByName(osLayerName);
        if( poLyr )
        {
            oLayerDesc.oMapFieldNameToOGRIdx[oField.GetName()] =
                poLyr->GetLayerDefn()->GetFieldIndex(oField.GetName());
            if( oField.GetType() == GMLAS_FT_GEOMETRY )
            {
                oLayerDesc.oMapFieldNameToOGRIdx[oField.GetName() + "_xml"] =
                    poLyr->GetLayerDefn()->GetFieldIndex(
                                        (oField.GetName() + "_xml").c_str());
            }
        }
    }
    m_poFieldsMDLayer->ResetReading();

    return true;
}

/************************************************************************/
/*                      CollectRelationships()                          */
/************************************************************************/

bool GMLASWriter::CollectRelationships()
{
    OGRFeatureDefn* poFDefn = m_poLayerRelationshipsLayer->GetLayerDefn();
    const char* const apszFields[] = { szPARENT_LAYER,
                                       szCHILD_LAYER,
                                       szPARENT_ELEMENT_NAME };
    for( size_t i = 0; i < CPL_ARRAYSIZE(apszFields); ++i )
    {
        if( poFDefn->GetFieldIndex(apszFields[i]) < 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find field %s in %s layer",
                     apszFields[i],
                     m_poLayerRelationshipsLayer->GetName());
            return false;
        }
    }

    m_poLayerRelationshipsLayer->SetAttributeFilter( nullptr );
    m_poLayerRelationshipsLayer->ResetReading();

    while( true )
    {
        OGRFeature* poFeature = m_poLayerRelationshipsLayer->GetNextFeature();
        if( poFeature == nullptr )
            break;

        const CPLString osParentLayer(
                            poFeature->GetFieldAsString( szPARENT_LAYER ) );
        if( m_oMapLayerNameToIdx.find( osParentLayer ) ==
                                            m_oMapLayerNameToIdx.end() )
        {
            // Shouldn't happen for well behaved metadata
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Cannot find in %s layer %s, referenced in %s",
                     szOGR_LAYERS_METADATA,
                     osParentLayer.c_str(),
                     szOGR_LAYER_RELATIONSHIPS);
            delete poFeature;
            continue;
        }

        const CPLString osChildLayer(
                                poFeature->GetFieldAsString( szCHILD_LAYER ) );
        if( m_oMapLayerNameToIdx.find( osChildLayer ) ==
                                            m_oMapLayerNameToIdx.end() )
        {
            // Shouldn't happen for well behaved metadata
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Cannot find in %s layer %s, referenced in %s",
                     szOGR_LAYERS_METADATA,
                     osChildLayer.c_str(),
                     szOGR_LAYER_RELATIONSHIPS);
            delete poFeature;
            continue;
        }

        const int nChildLayerIdx = m_oMapLayerNameToIdx[osChildLayer];
        if( m_aoLayerDesc[nChildLayerIdx].bIsTopLevel )
        {
            const CPLString osReferencingField(
                        poFeature->GetFieldAsString( szPARENT_ELEMENT_NAME ) );

            m_aoLayerDesc[nChildLayerIdx].aoReferencingLayers.push_back(
                    PairLayerNameColName( osParentLayer, osReferencingField ) );
        }

        delete poFeature;
    }
    m_poLayerRelationshipsLayer->ResetReading();

    return true;
}

/************************************************************************/
/*                      ComputeTopLevelFIDs()                           */
/*                                                                      */
/* Find which features of top-level layers are referenced by other      */
/* features, in which case we don't need to emit them in their layer    */
/************************************************************************/

void GMLASWriter::ComputeTopLevelFIDs()
{
    for( size_t i = 0; i < m_aoLayerDesc.size(); ++i )
    {
        LayerDescription& oDesc = m_aoLayerDesc[i];
        OGRLayer* poLayer = GetLayerByName(oDesc.osName);
        if( oDesc.bIsTopLevel && poLayer != nullptr &&
            !oDesc.aoReferencingLayers.empty() )
        {
            for( size_t j = 0; j < oDesc.aoReferencingLayers.size(); ++j )
            {
                CPLString osSQL;
                CPLString osFID("FID");
                if( poLayer->GetFIDColumn() &&
                    !EQUAL(poLayer->GetFIDColumn(), "") )
                {
                    osFID = poLayer->GetFIDColumn();
                }

                // Determine if the referencing field points to a junction
                // table
                const auto oIter = m_oMapLayerNameToIdx.find(
                                        oDesc.aoReferencingLayers[j].first);
                if( oIter != m_oMapLayerNameToIdx.end() )
                {
                    const LayerDescription& oReferencingLayerDesc =
                                                m_aoLayerDesc[oIter->second];
                    for( const auto& oIterField:
                                            oReferencingLayerDesc.oMapIdxToField )
                    {
                        const GMLASField& oField = oIterField.second;
                        if( oField.GetName() ==
                                        oDesc.aoReferencingLayers[j].second )
                        {
                            if( oField.GetCategory() == GMLASField::
                                    PATH_TO_CHILD_ELEMENT_WITH_JUNCTION_TABLE )
                            {
                                osSQL.Printf(
                                    "SELECT s.\"%s\" AS ogr_main_fid  "
                                    "FROM \"%s\" s "
                                    "JOIN \"%s\" j ON j.%s = s.\"%s\"",
                                    osFID.c_str(),
                                    oDesc.osName.c_str(),
                                    oField.GetJunctionLayer().c_str(),
                                    szCHILD_PKID,
                                    oDesc.osPKIDName.c_str());
                            }
                            break;
                        }
                    }
                }

                // Otherwise we can use the referencing (layer_name,
                // field_name) tuple directly.
                if( osSQL.empty() )
                {
                    osSQL.Printf("SELECT s.\"%s\" AS ogr_main_fid "
                                 "FROM \"%s\" s "
                                 "JOIN \"%s\" m ON m.\"%s\" = s.\"%s\"",
                                 osFID.c_str(),
                                 oDesc.osName.c_str(),
                                 oDesc.aoReferencingLayers[j].first.c_str(),
                                 oDesc.aoReferencingLayers[j].second.c_str(),
                                 oDesc.osPKIDName.c_str());
                }

                CPLDebug("GMLAS", "Executing %s", osSQL.c_str());
                OGRLayer* poSQLLyr = m_poSrcDS->ExecuteSQL( osSQL, nullptr, nullptr );
                if( poSQLLyr )
                {
                    while( true )
                    {
                        OGRFeature* poFeature = poSQLLyr->GetNextFeature();
                        if( poFeature == nullptr )
                            break;

                        const GIntBig nFID = poFeature->GetFieldAsInteger64(0);
                        oDesc.aoSetReferencedFIDs.insert( nFID );

                        delete poFeature;
                    }
                    m_poSrcDS->ReleaseResultSet( poSQLLyr );
                }
            }

        }
    }
}

/************************************************************************/
/*                           SplitXPath()                               */
/************************************************************************/

// Decompose a XPath ns1:foo1/@ns2:foo2/... in its components
// [ (ns1,foo1), (ns2,@foo2), ... ]
static XPathComponents SplitXPathInternal( const CPLString& osXPath )
{
    char** papszTokens = CSLTokenizeString2(osXPath, "/", 0);
    XPathComponents aoComponents;
    for( int i = 0; papszTokens[i] != nullptr; ++i )
    {
        const bool bAttr = (papszTokens[i][0] == '@');
        char** papszNSElt = CSLTokenizeString2(papszTokens[i] +
                                                    (bAttr ? 1 : 0), ":", 0);
        if( papszNSElt[0] != nullptr && papszNSElt[1] != nullptr &&
            papszNSElt[2] == nullptr )
        {
            CPLString osVal(papszNSElt[1]);
            size_t nPos = osVal.find(szEXTRA_SUFFIX);
            if( nPos != std::string::npos )
                osVal.resize(nPos);
            aoComponents.push_back( PairNSElement( papszNSElt[0],
                    (bAttr ? CPLString("@") : CPLString()) + osVal ) );
        }
        else if( papszNSElt[0] != nullptr && papszNSElt[1] == nullptr )
        {
            CPLString osVal(papszNSElt[0]);
            size_t nPos = osVal.find(szEXTRA_SUFFIX);
            if( nPos != std::string::npos )
                osVal.resize(nPos);
            aoComponents.push_back( PairNSElement( "",
                (bAttr ? CPLString("@") : CPLString()) + osVal ) );
        }
        CSLDestroy(papszNSElt);
    }
    CSLDestroy(papszTokens);
    return aoComponents;
}

const XPathComponents& GMLASWriter::SplitXPath( const CPLString& osXPath )
{
    const auto oIter = m_oMapXPathToComponents.find(osXPath);
    if( oIter != m_oMapXPathToComponents.end() )
        return oIter->second;

    m_oMapXPathToComponents[ osXPath ] = SplitXPathInternal(osXPath);
    return m_oMapXPathToComponents[ osXPath ];
}

/************************************************************************/
/*                            IsAttr()                                  */
/************************************************************************/

static bool IsAttr( const PairNSElement& pair )
{
    return !pair.second.empty() && pair.second[0] == '@' ;
}

/************************************************************************/
/*                           MakeXPath()                                */
/************************************************************************/

static CPLString MakeXPath( const PairNSElement& pair )
{
    if( pair.first.empty() )
    {
        if( IsAttr(pair) )
            return pair.second.substr(1);
        else
            return pair.second;
    }
    else if( IsAttr(pair) )
        return pair.first + ":" + pair.second.substr(1);
    else
        return pair.first + ":" + pair.second;
}

/************************************************************************/
/*                           WriteLayer()                               */
/************************************************************************/

bool GMLASWriter::WriteLayer( bool bWFS2FeatureCollection,
                              const LayerDescription& oDesc,
                              GIntBig& nFeaturesWritten,
                              GIntBig nTotalTopLevelFeatures,
                              GDALProgressFunc pfnProgress,
                              void* pProgressData )
{
    OGRLayer* poSrcLayer = GetLayerByName(oDesc.osName);
    if( poSrcLayer == nullptr )
        return true;

    poSrcLayer->ResetReading();
    IncIndent();
    std::set<CPLString> oSetLayersInIteration;
    oSetLayersInIteration.insert(oDesc.osName);
    bool bRet = true;
    while( bRet )
    {
        OGRFeature* poFeature = poSrcLayer->GetNextFeature();
        if( poFeature == nullptr )
            break;

        if( oDesc.aoSetReferencedFIDs.find( poFeature->GetFID() )
                                    == oDesc.aoSetReferencedFIDs.end() )
        {
            PrintIndent(m_fpXML);
            if( bWFS2FeatureCollection )
            {
                PrintLine(m_fpXML, "<%s:%s>", szWFS_PREFIX, szMEMBER);
            }
            else
            {
                PrintLine(m_fpXML, "<%s:%s>",
                          m_osTargetNameSpacePrefix.c_str(), szFEATURE_MEMBER);
            }

            bRet = WriteFeature( poFeature, oDesc,
                                 oSetLayersInIteration,
                                 XPathComponents(),
                                 XPathComponents(),
                                 0 );

            PrintIndent(m_fpXML);
            if( bWFS2FeatureCollection )
            {
                PrintLine(m_fpXML, "</%s:%s>", szWFS_PREFIX, szMEMBER);
            }
            else
            {
                PrintLine(m_fpXML, "</%s:%s>",
                          m_osTargetNameSpacePrefix.c_str(), szFEATURE_MEMBER);
            }

            if( bRet )
            {
                nFeaturesWritten ++;
                const double dfPct = static_cast<double>(nFeaturesWritten) /
                                                        nTotalTopLevelFeatures;
                if( pfnProgress && !pfnProgress(dfPct, "", pProgressData) )
                {
                    bRet = false;
                }
            }

        }
        delete poFeature;
    }
    poSrcLayer->ResetReading();
    DecIndent();

    return bRet;
}

/************************************************************************/
/*                        FindCommonPrefixLength()                      */
/************************************************************************/

static
size_t FindCommonPrefixLength( const XPathComponents& a,
                               const XPathComponents& b )
{
    size_t i = 0;
    for(; i < a.size() && i < b.size(); ++i )
    {
        if( a[i].first != b[i].first || a[i].second != b[i].second )
            break;
    }
    return i;
}

/************************************************************************/
/*                        WriteClosingTags()                            */
/************************************************************************/

void GMLASWriter::WriteClosingTags( size_t nCommonLength,
                                    const XPathComponents& aoCurComponents,
                                    const XPathComponents& aoNewComponents,
                                    bool bCurIsRegularField,
                                    bool bNewIsRegularField )
{
    if( nCommonLength < aoCurComponents.size() )
    {
        bool bFieldIsAnotherAttrOfCurElt = false;
        size_t i = aoCurComponents.size() - 1;

        bool bMustIndent = !bCurIsRegularField;

        if( IsAttr(aoCurComponents.back()) )
        {
            if( nCommonLength + 1 == aoCurComponents.size() &&
                nCommonLength + 1 == aoNewComponents.size() &&
                IsAttr(aoNewComponents.back()) )
            {
                bFieldIsAnotherAttrOfCurElt = true;
            }
            else
            {
                /*
                a/@b  cur
                a     new
                ==> <a b="">foo</a>

                a/@b  cur
                a/c   new
                ==> <a b="">
                        <c/>
                     </a>

                a/@b  cur
                c     new
                ==> <a b=""/>
                    <c/>

                */
                if( (nCommonLength == 0 ||
                     nCommonLength + 2 <= aoCurComponents.size()) &&
                    i >= 2 )
                {
                    PrintLine(m_fpXML, " />");
                    i -= 2;
                    DecIndent();
                    bMustIndent = true;
                }
                else
                {
                    VSIFPrintfL(m_fpXML, ">");
                    CPLAssert( i > 0 );
                    i --;
                    // Print a new line except in the <elt attr="foo">bar</elt>
                    // situation
                    if( !(nCommonLength + 1 == aoCurComponents.size() &&
                          nCommonLength == aoNewComponents.size() &&
                          bNewIsRegularField) )
                    {
                        PrintLine(m_fpXML, "%s", "");
                    }
                }
            }
        }

        if( !bFieldIsAnotherAttrOfCurElt )
        {
            for( ; i >= nCommonLength; --i )
            {
                if( bMustIndent)
                {
                    PrintIndent(m_fpXML);
                }
                bMustIndent = true;
                PrintLine(m_fpXML, "</%s>",
                        MakeXPath(aoCurComponents[i]).c_str());
                DecIndent();
                if( i == 0 )
                    break;
            }
        }
    }
}

/************************************************************************/
/*                      WriteClosingAndStartingTags()                   */
/************************************************************************/

void GMLASWriter::WriteClosingAndStartingTags(
                        const XPathComponents& aoCurComponents,
                        const XPathComponents& aoNewComponents,
                        bool bCurIsRegularField )
{

    const size_t nCommonLength =
        FindCommonPrefixLength( aoCurComponents, aoNewComponents );

    WriteClosingTags( nCommonLength, aoCurComponents,
                      aoNewComponents, bCurIsRegularField, false );
    for(size_t i = nCommonLength; i < aoNewComponents.size(); ++i )
    {
        IncIndent();
        PrintIndent(m_fpXML);
        PrintLine(m_fpXML, "<%s>",
                    MakeXPath(aoNewComponents[i]).c_str());
    }
}

/************************************************************************/
/*                          WriteFeature()                              */
/************************************************************************/

bool GMLASWriter::WriteFeature(
                        OGRFeature* poFeature,
                        const LayerDescription& oLayerDesc,
                        const std::set<CPLString>& oSetLayersInIteration,
                        const XPathComponents& aoInitialComponents,
                        const XPathComponents& aoPrefixComponents,
                        int nRecLevel)
{
    if( nRecLevel == 100 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "WriteFeature() called with 100 levels of recursion");
        return false;
    }

    XPathComponents aoCurComponents(aoInitialComponents);
    XPathComponents aoLayerComponents;
    bool bAtLeastOneFieldWritten = false;
    bool bCurIsRegularField = false;
    for( const auto& oIter: oLayerDesc.oMapIdxToField )
    {
        const GMLASField& oField = oIter.second;
        const GMLASField::Category eCategory( oField.GetCategory() );
        if( eCategory == GMLASField::REGULAR )
        {
            WriteFieldRegular(           poFeature, oField,
                                         oLayerDesc,
                                         /*aoLayerComponents, */
                                         aoCurComponents,
                                         aoPrefixComponents,
                                         /*oSetLayersInIteration,*/
                                         bAtLeastOneFieldWritten,
                                         bCurIsRegularField);
        }
        else if( eCategory == GMLASField::PATH_TO_CHILD_ELEMENT_NO_LINK ||
                 eCategory == GMLASField::GROUP )
        {
            if( !WriteFieldNoLink(       poFeature, oField,
                                         oLayerDesc,
                                         aoLayerComponents,
                                         aoCurComponents,
                                         aoPrefixComponents,
                                         oSetLayersInIteration,
                                         nRecLevel,
                                         bAtLeastOneFieldWritten,
                                         bCurIsRegularField ) )
            {
                return false;
            }
        }
        else if( eCategory == GMLASField::PATH_TO_CHILD_ELEMENT_WITH_LINK )
        {
            if( !WriteFieldWithLink(     poFeature, oField,
                                         oLayerDesc,
                                         aoLayerComponents,
                                         aoCurComponents,
                                         aoPrefixComponents,
                                         oSetLayersInIteration,
                                         nRecLevel,
                                         bAtLeastOneFieldWritten,
                                         bCurIsRegularField ) )
            {
                return false;
            }
        }
        else if( eCategory ==
                        GMLASField::PATH_TO_CHILD_ELEMENT_WITH_JUNCTION_TABLE )
        {
            if( !WriteFieldJunctionTable(poFeature, oField,
                                         oLayerDesc,
                                         aoLayerComponents,
                                         aoCurComponents,
                                         aoPrefixComponents,
                                         oSetLayersInIteration,
                                         nRecLevel,
                                         bAtLeastOneFieldWritten,
                                         bCurIsRegularField ) )
            {
                return false;
            }
        }

    }

    if( !bAtLeastOneFieldWritten && aoInitialComponents.empty() &&
        !oLayerDesc.osXPath.empty() )
    {
        aoLayerComponents = SplitXPath(oLayerDesc.osXPath);
        const CPLString osLayerElt(MakeXPath(aoLayerComponents.back()) );
        PrintIndent(m_fpXML);
        VSIFPrintfL(m_fpXML, "%s", m_osIndentation.c_str());
        PrintLine(m_fpXML, "<%s />", osLayerElt.c_str());
    }
    else
    {
        const size_t nCommonLength =
                FindCommonPrefixLength( aoCurComponents, aoInitialComponents );
        WriteClosingTags( nCommonLength, aoCurComponents,
                          aoInitialComponents, bCurIsRegularField, false );
    }

    return true;
}

/************************************************************************/
/*                     PrintMultipleValuesSeparator()                   */
/************************************************************************/

void GMLASWriter::PrintMultipleValuesSeparator(
                                const GMLASField& oField,
                                const XPathComponents& aoFieldComponents)
{
    if( oField.IsList() )
    {
        VSIFPrintfL(m_fpXML, " ");
    }
    else
    {
        PrintLine(m_fpXML, "</%s>",
                MakeXPath(aoFieldComponents.back()).c_str());
        PrintIndent(m_fpXML);
        VSIFPrintfL(m_fpXML, "<%s>",
                MakeXPath(aoFieldComponents.back()).c_str());
    }
}

/************************************************************************/
/*                         PrintXMLDouble()                             */
/************************************************************************/

static void PrintXMLDouble(VSILFILE* fp, double dfVal )
{
    if( CPLIsInf(dfVal) )
    {
        if( dfVal > 0 )
            VSIFPrintfL(fp, "INF");
        else
            VSIFPrintfL(fp, "-INF");
    }
    else if( CPLIsNan(dfVal) )
        VSIFPrintfL(fp, "NaN");
    else
        VSIFPrintfL(fp, "%.16g", dfVal);
}

/************************************************************************/
/*                 AreGeomsEqualAxisOrderInsensitive()                  */
/************************************************************************/

static bool AreGeomsEqualAxisOrderInsensitive(OGRGeometry* poGeomRef,
                                              OGRGeometry* poGeomModifiable)
{
    if( poGeomRef->Equals(poGeomModifiable) )
        return true;
    poGeomModifiable->swapXY();
    return CPL_TO_BOOL(poGeomRef->Equals(poGeomModifiable));
}


/************************************************************************/
/*                             GetCoordSwap()                           */
/************************************************************************/

bool GMLASWriter::GetCoordSwap( const OGRSpatialReference* poSRS )
{
    const auto oIter = m_oMapSRSToCoordSwap.find(poSRS);
    if( oIter != m_oMapSRSToCoordSwap.end() )
        return oIter->second;

    bool bCoordSwap = false;
    if( m_osSRSNameFormat != "SHORT" )
    {
        const auto& map = poSRS->GetDataAxisToSRSAxisMapping();
        if( map.size() >= 2 && map[0] == 2 && map[1] == 1 )
        {
            bCoordSwap = true;
        }
    }
    m_oMapSRSToCoordSwap[poSRS] = bCoordSwap;
    return bCoordSwap;
}

/************************************************************************/
/*                     WriteFieldRegular()                              */
/************************************************************************/

bool GMLASWriter::WriteFieldRegular(
                        OGRFeature* poFeature,
                        const GMLASField& oField,
                        const LayerDescription& oLayerDesc,
                        /*XPathComponents& aoLayerComponents,*/
                        XPathComponents& aoCurComponents,
                        const XPathComponents& aoPrefixComponents,
                        /*const std::set<CPLString>& oSetLayersInIteration,*/
                        bool& bAtLeastOneFieldWritten,
                        bool& bCurIsRegularField)
{
    const bool bIsGeometryField = oField.GetTypeName() == szFAKEXS_GEOMETRY;
    const int nFieldIdx = bIsGeometryField ?
        // Some drivers may not store the geometry field name, so for a
        // feature with a single geometry, use it
        ( poFeature->GetGeomFieldCount() == 1 ? 0 :
          poFeature->GetGeomFieldIndex( oField.GetName() ) ) :
        oLayerDesc.GetOGRIdxFromFieldName( oField.GetName() );
    XPathComponents aoFieldComponents = SplitXPath(oField.GetXPath());
    aoFieldComponents.insert( aoFieldComponents.begin(),
                              aoPrefixComponents.begin(),
                              aoPrefixComponents.end() );

    // For extension/* case
    if( !aoFieldComponents.empty() && aoFieldComponents.back().second == "*" )
    {
        aoFieldComponents.resize( aoFieldComponents.size() - 1 );
    }

    const size_t nCommonLength =
        FindCommonPrefixLength( aoCurComponents, aoFieldComponents );

    const bool bEmptyContent =
        nFieldIdx < 0 ||
          ((bIsGeometryField && !poFeature->GetGeomFieldRef(nFieldIdx)) ||
           (!bIsGeometryField && !poFeature->IsFieldSetAndNotNull(nFieldIdx)));
    const bool bIsNull = m_oConf.m_bUseNullState  &&
                         (!bIsGeometryField && nFieldIdx >= 0 &&
                           poFeature->IsFieldNull(nFieldIdx));
    bool bMustBeEmittedEvenIfEmpty = oField.GetMinOccurs() > 0 || bIsNull;
    if( !m_oConf.m_bUseNullState &&
        oField.GetMinOccurs() == 0 && bEmptyContent &&
        nCommonLength + 1 == aoCurComponents.size() &&
        IsAttr(aoCurComponents.back()) &&
        nCommonLength == aoFieldComponents.size() &&
        oLayerDesc.oMapFieldXPathToIdx.find(
            oField.GetXPath() + "/" + szAT_XSI_NIL) ==
                                    oLayerDesc.oMapFieldXPathToIdx.end() )
    {
        // This is quite tricky to determine if a <foo bar="baz"/> node is
        // valid or if we must add a xsi:nil="true" to make it valid
        // For now assume that a string can be empty
        if( oField.GetType() != GMLAS_FT_STRING )
            bMustBeEmittedEvenIfEmpty = true;
    }

    if( bEmptyContent && !bMustBeEmittedEvenIfEmpty )
        return true;

    // Do not emit optional attributes at default/fixed value
    if( !aoFieldComponents.empty() &&
        oField.GetMinOccurs() == 0 &&
        IsAttr( aoFieldComponents.back() ) )
    {
        const CPLString& osDefaultVal( !oField.GetDefaultValue().empty() ?
                                            oField.GetDefaultValue() :
                                            oField.GetFixedValue() );
        if( !osDefaultVal.empty() )
        {
            if( oField.GetType() == GMLAS_FT_BOOLEAN )
            {
                const int nVal = poFeature->GetFieldAsInteger(nFieldIdx);
                if( osDefaultVal == "false" && nVal == 0 )
                    return true;
                if( osDefaultVal == "true" && nVal == 1 )
                    return true;
            }
            else if ( osDefaultVal == poFeature->GetFieldAsString(nFieldIdx) )
            {
                return true;
            }
        }
    }

    bAtLeastOneFieldWritten = true;

    if( bEmptyContent &&
        nCommonLength + 1 == aoCurComponents.size() &&
        IsAttr(aoCurComponents.back()) &&
        nCommonLength == aoFieldComponents.size() )
    {
        // Particular case for <a foo="bar" xsi:nil="true"/>
        VSIFPrintfL(m_fpXML, " xsi:nil=\"true\">");
        aoCurComponents = aoFieldComponents;
        bCurIsRegularField = true;
        return true;
    }
    else
    {
        // Emit closing tags
        WriteClosingTags( nCommonLength, aoCurComponents,
                            aoFieldComponents, bCurIsRegularField, true );
    }

    // Emit opening tags and attribute names
    // We may do a 0 iteration in case of returning from an attribute
    // to its element
    bool bWriteEltContent = true;
    for(size_t i = nCommonLength; i < aoFieldComponents.size(); ++i )
    {
        if( i + 1 == aoFieldComponents.size() &&
            IsAttr(aoFieldComponents[i]) )
        {
            if( aoFieldComponents[i].second != szAT_ANY_ATTR )
            {
                VSIFPrintfL(m_fpXML, " %s=",
                            MakeXPath(aoFieldComponents[i]).c_str());
                bWriteEltContent = false;
            }
        }
        else
        {
            if( i > nCommonLength ) PrintLine(m_fpXML, "%s", "");
            IncIndent();
            PrintIndent(m_fpXML);

            if( i + 2 == aoFieldComponents.size() &&
                    IsAttr(aoFieldComponents[i + 1]) )
            {
                // Are we an element that is going to have an
                // attribute ?
                VSIFPrintfL(m_fpXML, "<%s",
                        MakeXPath(aoFieldComponents[i]).c_str());
            }
            else
            {
                // Are we a regular element ?
                if( bEmptyContent )
                {
                    VSIFPrintfL(m_fpXML, "<%s xsi:nil=\"true\">",
                        MakeXPath(aoFieldComponents[i]).c_str());
                }
                else
                {
                    VSIFPrintfL(m_fpXML, "<%s>",
                        MakeXPath(aoFieldComponents[i]).c_str());
                }
            }
        }
    }

    // Write content
    if( !bWriteEltContent )
        VSIFPrintfL(m_fpXML, "\"" );

    if( !bEmptyContent && oField.GetTypeName() == szFAKEXS_JSON_DICT )
    {
        json_object* poObj = nullptr;
        if( OGRJSonParse( poFeature->GetFieldAsString(nFieldIdx),
                            &poObj ) )
        {
            if( json_type_object == json_object_get_type( poObj ) )
            {
                json_object_iter it;
                it.key = nullptr;
                it.val = nullptr;
                it.entry = nullptr;
                json_object_object_foreachC( poObj, it )
                {
                    if( it.val != nullptr &&
                        json_object_get_type(it.val) ==
                                                json_type_string )
                    {
                        VSIFPrintfL(m_fpXML, " %s=\"%s\"",
                                    it.key,
                            XMLEscape(
                                json_object_get_string(it.val)).c_str());
                    }
                }
            }
            json_object_put(poObj);
        }
    }
    else if( !bEmptyContent && bIsGeometryField )
    {
        bool bWriteOGRGeom = true;
        OGRGeometry* poGeom = poFeature->GetGeomFieldRef( nFieldIdx );

        // In case the original GML string was saved, fetch it and compare it
        // to the current OGR geometry. If they match (in a axis order
        // insensitive way), then use the original GML string
        const int nFieldXMLIdx = oLayerDesc.GetOGRIdxFromFieldName
                                                (oField.GetName() + "_xml");
        if( nFieldXMLIdx >= 0 &&
            poFeature->IsFieldSetAndNotNull(nFieldXMLIdx) )
        {
            if( poFeature->GetFieldDefnRef(nFieldXMLIdx)->GetType() ==
                                                                OFTStringList )
            {
                if( wkbFlatten(poGeom->getGeometryType()) ==
                                                        wkbGeometryCollection )
                {
                    OGRGeometryCollection* poGC = new OGRGeometryCollection();
                    char** papszValues = poFeature->
                                        GetFieldAsStringList(nFieldXMLIdx);
                    for( int j = 0; papszValues != nullptr &&
                                    papszValues[j] != nullptr; ++j )
                    {
                        OGRGeometry* poPart = reinterpret_cast<OGRGeometry*>(
                                        OGR_G_CreateFromGML(papszValues[j]));
                        if( poPart )
                            poGC->addGeometryDirectly(poPart);
                    }
                    if( AreGeomsEqualAxisOrderInsensitive(poGeom, poGC) )
                    {
                        for( int j = 0; papszValues != nullptr &&
                                    papszValues[j] != nullptr; ++j )
                        {
                            if( j > 0 )
                                PrintMultipleValuesSeparator(oField,
                                                            aoFieldComponents);
                            VSIFPrintfL(m_fpXML, "%s", papszValues[j]);
                        }
                        bWriteOGRGeom = false;
                    }
                    delete poGC;
                }
            }
            else
            {
                const char* pszXML = poFeature->GetFieldAsString(nFieldXMLIdx);
                OGRGeometry* poOrigGeom = reinterpret_cast<OGRGeometry*>(
                                                OGR_G_CreateFromGML(pszXML));

                if( poOrigGeom != nullptr )
                {
                    if( AreGeomsEqualAxisOrderInsensitive(poGeom, poOrigGeom) )
                    {
                        VSIFPrintfL(m_fpXML, "%s", pszXML);
                        bWriteOGRGeom = false;
                    }
                    delete poOrigGeom;
                }
            }
        }

        if( bWriteOGRGeom )
        {
            CPLString osExtraElt;
            bool bGMLSurface311 = false;
            bool bGMLCurve311 = false;
            bool bGMLPoint311 = false;
            if( m_osGMLVersion == "3.1.1" &&
                MakeXPath(aoFieldComponents.back()) == "gml:Surface"  )
            {
                bGMLSurface311 = true;

            }
            else if( m_osGMLVersion == "3.1.1" &&
                     MakeXPath(aoFieldComponents.back()) == "gml:Curve" )
            {
                bGMLCurve311 = true;
            }
            else if( m_osGMLVersion == "3.1.1" &&
                     MakeXPath(aoFieldComponents.back()) == "gml:Point" )
            {
                bGMLPoint311 = true;
            }

            const double dfGMLVersion =
                m_osGMLVersion.empty() ? 3.2 : CPLAtof(m_osGMLVersion);
            char** papszOptions = CSLSetNameValue(nullptr, "FORMAT",
                    (dfGMLVersion >= 2.0 && dfGMLVersion < 3.0) ?  "GML2" :
                    (dfGMLVersion >= 3.0 && dfGMLVersion < 3.2) ?  "GML3" :
                                                                   "GML32" );
            papszOptions = CSLSetNameValue(papszOptions, "SRSNAME_FORMAT",
                                           m_osSRSNameFormat);

            if( dfGMLVersion < 3.0 )
            {
                bool bSwap = false;
                const OGRSpatialReference* poSRS =
                                                poGeom->getSpatialReference();
                if( poSRS != nullptr && GetCoordSwap(poSRS) )
                    bSwap = true;
                papszOptions = CSLSetNameValue(papszOptions, "COORD_SWAP",
                                                    bSwap ? "TRUE" : "FALSE");
            }

            if( oField.GetMaxOccurs() > 1 &&
                wkbFlatten(poGeom->getGeometryType()) ==
                                                        wkbGeometryCollection )
            {
                OGRGeometryCollection* poGC = poGeom->toGeometryCollection();
                for(int j=0; j<poGC->getNumGeometries(); ++j)
                {
                    if( dfGMLVersion >= 3.2 )
                    {
                        CPLString osGMLID =
                            poFeature->GetFieldAsString(oLayerDesc.osPKIDName);
                        osGMLID += CPLSPrintf(".geom%d.%d", nFieldIdx, j);
                        papszOptions = CSLSetNameValue(papszOptions, "GMLID",
                                                        osGMLID);
                    }
                    if( j > 0 )
                        PrintMultipleValuesSeparator(oField,
                                                     aoFieldComponents);
                    char* pszGML = OGR_G_ExportToGMLEx(
                        reinterpret_cast<OGRGeometryH>(poGC->getGeometryRef(j)),
                        papszOptions);
                    if( pszGML )
                        VSIFPrintfL(m_fpXML, "%s", pszGML);
                    CPLFree(pszGML);
                }
            }
            else
            {
                if( dfGMLVersion >= 3.2 )
                {
                    CPLString osGMLID =
                            poFeature->GetFieldAsString(oLayerDesc.osPKIDName);
                    osGMLID += CPLSPrintf(".geom%d", nFieldIdx);
                    papszOptions = CSLSetNameValue(papszOptions, "GMLID",
                                                    osGMLID);
                }
                char* pszGML = OGR_G_ExportToGMLEx(
                    reinterpret_cast<OGRGeometryH>(poGeom), papszOptions);
                if( pszGML )
                {
                    if( bGMLSurface311 && STARTS_WITH(pszGML, "<gml:Polygon>") )
                    {
                        char* pszEnd = strstr(pszGML, "</gml:Polygon>");
                        if( pszEnd )
                        {
                            *pszEnd = '\0';
                            VSIFPrintfL(m_fpXML,
                                        "<gml:patches><gml:PolygonPatch>%s"
                                        "</gml:PolygonPatch></gml:patches>",
                                        pszGML + strlen("<gml:Polygon>"));
                        }
                    }
                    else if( bGMLCurve311 && STARTS_WITH(pszGML, "<gml:LineString>") )
                    {
                        char* pszEnd = strstr(pszGML, "</gml:LineString>");
                        if( pszEnd )
                        {
                            *pszEnd = '\0';
                            VSIFPrintfL(m_fpXML,
                                        "<gml:segments><gml:LineStringSegment>%s"
                                        "</gml:LineStringSegment></gml:segments>",
                                        pszGML + strlen("<gml:LineString>"));
                        }
                    }
                    else if( bGMLPoint311 && STARTS_WITH(pszGML, "<gml:Point>") )
                    {
                        char* pszEnd = strstr(pszGML, "</gml:Point>");
                        if( pszEnd )
                        {
                            *pszEnd = '\0';
                            VSIFPrintfL(m_fpXML, "%s",
                                        pszGML + strlen("<gml:Point>"));
                        }
                    }
                    else
                    {
                        VSIFPrintfL(m_fpXML, "%s", pszGML);
                    }
                }
                CPLFree(pszGML);
            }
            CSLDestroy(papszOptions);
        }
    }
    else if( !bEmptyContent && oField.GetTypeName() == szXS_ANY_TYPE )
    {
        CPLString osXML(poFeature->GetFieldAsString(nFieldIdx));
        // Check that the content is valid XML
        CPLString osValidatingXML( "<X>" + osXML + "</X>" );
        CPLXMLNode* psNode = CPLParseXMLString(osValidatingXML);
        if( psNode != nullptr )
        {
            VSIFPrintfL(m_fpXML, "%s", osXML.c_str());
            CPLDestroyXMLNode(psNode);
        }
        else
        {
            // Otherwise consider it as text and escape
            VSIFPrintfL(m_fpXML, "%s", XMLEscape(osXML).c_str());
        }
    }
    else if( !bEmptyContent )
    {
        const OGRFieldType eOGRType(
            poFeature->GetFieldDefnRef(nFieldIdx)->GetType() );
        switch( oField.GetType() )
        {
            case GMLAS_FT_BOOLEAN:
            {
                if( (oField.GetMaxOccurs() > 1 || oField.IsList()) &&
                    eOGRType == OFTIntegerList )
                {
                    int nCount = 0;
                    const int* panValues = poFeature->
                                GetFieldAsIntegerList(nFieldIdx, &nCount);
                    for( int j = 0; j < nCount; ++j )
                    {
                        if( j > 0 )
                            PrintMultipleValuesSeparator(oField,
                                                            aoFieldComponents);
                        VSIFPrintfL(m_fpXML,
                                    panValues[j] ? "true" : "false");
                    }
                }
                else
                {
                    VSIFPrintfL(m_fpXML,
                            poFeature->GetFieldAsInteger(nFieldIdx) ?
                                                    "true" : "false" );
                }
                break;
            }

            case GMLAS_FT_DATETIME:
            case GMLAS_FT_DATE:
            case GMLAS_FT_TIME:
            {
                if( eOGRType == OFTDateTime ||
                    eOGRType == OFTDate ||
                    eOGRType == OFTTime)
                {
                    char* pszFormatted = OGRGetXMLDateTime(
                                poFeature->GetRawFieldRef(nFieldIdx));
                    char* pszT = strchr(pszFormatted, 'T');
                    if( oField.GetType() == GMLAS_FT_TIME &&
                        pszT != nullptr )
                    {
                        VSIFPrintfL(m_fpXML, "%s", pszT+1);
                    }
                    else
                    {
                        if( oField.GetType() == GMLAS_FT_DATE )
                        {
                            if( pszT )
                                *pszT = '\0';
                        }
                        VSIFPrintfL(m_fpXML, "%s", pszFormatted);
                    }
                    VSIFree(pszFormatted);
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                            "Invalid content for field %s of type %s: %s",
                            oField.GetName().c_str(),
                            oField.GetTypeName().c_str(),
                            poFeature->GetFieldAsString(nFieldIdx));
                }
                break;
            }

            case GMLAS_FT_BASE64BINARY:
            {
                if( eOGRType == OFTBinary )
                {
                    int nCount = 0;
                    GByte* pabyContent = poFeature->
                                    GetFieldAsBinary(nFieldIdx, &nCount );
                    char* pszBase64 = CPLBase64Encode(nCount, pabyContent);
                    VSIFPrintfL(m_fpXML, "%s", pszBase64);
                    CPLFree(pszBase64);
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                            "Invalid content for field %s of type %s: %s",
                            oField.GetName().c_str(),
                            oField.GetTypeName().c_str(),
                            poFeature->GetFieldAsString(nFieldIdx));
                }
                break;
            }

            case GMLAS_FT_HEXBINARY:
            {
                if( eOGRType == OFTBinary )
                {
                    int nCount = 0;
                    GByte* pabyContent = poFeature->
                                    GetFieldAsBinary(nFieldIdx, &nCount );
                    for( int i = 0; i < nCount; ++i )
                        VSIFPrintfL(m_fpXML, "%02X", pabyContent[i]);
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                            "Invalid content for field %s of type %s: %s",
                            oField.GetName().c_str(),
                            oField.GetTypeName().c_str(),
                            poFeature->GetFieldAsString(nFieldIdx));
                }
                break;
            }

            default:
            {
                if( (oField.GetMaxOccurs() > 1 || oField.IsList()) &&
                    (eOGRType == OFTStringList ||
                    eOGRType == OFTRealList ||
                    eOGRType == OFTIntegerList ||
                    eOGRType == OFTInteger64List ) )
                {
                    if( eOGRType == OFTStringList )
                    {
                        char** papszValues = poFeature->
                                        GetFieldAsStringList(nFieldIdx);
                        for( int j = 0; papszValues != nullptr &&
                                        papszValues[j] != nullptr; ++j )
                        {
                            if( j > 0 )
                                PrintMultipleValuesSeparator(oField,
                                                            aoFieldComponents);
                            VSIFPrintfL(m_fpXML, "%s", XMLEscape(
                                            papszValues[j]).c_str());
                        }
                    }
                    else if( eOGRType == OFTRealList )
                    {
                        int nCount = 0;
                        const double* padfValues = poFeature->
                                    GetFieldAsDoubleList(nFieldIdx, &nCount);
                        for( int j = 0; j < nCount; ++j )
                        {
                            if( j > 0 )
                                PrintMultipleValuesSeparator(oField,
                                                            aoFieldComponents);
                            PrintXMLDouble(m_fpXML, padfValues[j]);
                        }
                    }
                    else if( eOGRType == OFTIntegerList )
                    {
                        int nCount = 0;
                        const int* panValues = poFeature->
                                GetFieldAsIntegerList(nFieldIdx, &nCount);
                        for( int j = 0; j < nCount; ++j )
                        {
                            if( j > 0 )
                                PrintMultipleValuesSeparator(oField,
                                                            aoFieldComponents);
                            VSIFPrintfL(m_fpXML, "%d", panValues[j]);
                        }
                    }
                    else if( eOGRType == OFTInteger64List )
                    {
                        int nCount = 0;
                        const GIntBig* panValues = poFeature->
                                GetFieldAsInteger64List(nFieldIdx, &nCount);
                        for( int j = 0; j < nCount; ++j )
                        {
                            if( j > 0 )
                                PrintMultipleValuesSeparator(oField,
                                                            aoFieldComponents);
                            VSIFPrintfL(m_fpXML, CPL_FRMT_GIB, panValues[j]);
                        }
                    }
                }
                else if( eOGRType == OFTReal )
                {
                    PrintXMLDouble(m_fpXML,
                                poFeature->GetFieldAsDouble(nFieldIdx));
                }
                else
                {
                    VSIFPrintfL(m_fpXML, "%s", XMLEscape(
                        poFeature->GetFieldAsString(nFieldIdx)).c_str());
                }
                break;
            }
        }
    }

    if( !bWriteEltContent )
        VSIFPrintfL(m_fpXML, "\"" );


    aoCurComponents = aoFieldComponents;
    bCurIsRegularField = true;

    return true;
}

/************************************************************************/
/*                     WriteFieldNoLink()                               */
/************************************************************************/

bool GMLASWriter::WriteFieldNoLink(
                        OGRFeature* poFeature,
                        const GMLASField& oField,
                        const LayerDescription& oLayerDesc,
                        XPathComponents& aoLayerComponents,
                        XPathComponents& aoCurComponents,
                        const XPathComponents& aoPrefixComponents,
                        const std::set<CPLString>& oSetLayersInIteration,
                        int nRecLevel,
                        bool& bAtLeastOneFieldWritten,
                        bool& bCurIsRegularField)
{
    const auto oIter = m_oMapXPathToIdx.find( oField.GetRelatedClassXPath() );
    if( oIter == m_oMapXPathToIdx.end() )
    {
        // Not necessary to be more verbose in case of truncated
        // source dataset
        CPLDebug("GMLAS", "No child layer of %s matching xpath = %s",
                    oLayerDesc.osName.c_str(),
                    oField.GetRelatedClassXPath().c_str());
        return true;
    }

    const LayerDescription& oChildLayerDesc =
                                    m_aoLayerDesc[oIter->second];
    OGRLayer* poRelLayer = GetLayerByName( oChildLayerDesc.osName );
    if( poRelLayer == nullptr )
    {
        // Not necessary to be more verbose in case of truncated
        // source dataset
        CPLDebug("GMLAS", "Child layer %s of %s not found",
                    oChildLayerDesc.osName.c_str(),
                    oLayerDesc.osName.c_str());
        return true;
    }

    if( oLayerDesc.osPKIDName.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Missing %s for layer %s",
                    szLAYER_PKID_NAME,
                    oLayerDesc.osName.c_str());
        return true;
    }
    int nParentPKIDIdx;
    if( (nParentPKIDIdx =
            oLayerDesc.GetOGRIdxFromFieldName( oLayerDesc.osPKIDName )) < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot find field %s in layer %s",
                    oLayerDesc.osPKIDName.c_str(),
                    oLayerDesc.osName.c_str());
        return true;
    }
    if( !poFeature->IsFieldSetAndNotNull( nParentPKIDIdx ) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Missing value of %s field for feature "
                    CPL_FRMT_GIB " of layer %s",
                    oLayerDesc.osPKIDName.c_str(),
                    poFeature->GetFID(),
                    oLayerDesc.osName.c_str());
        return true;
    }
    if( oChildLayerDesc.osParentPKIDName.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Missing %s for layer %s",
                    szLAYER_PARENT_PKID_NAME,
                    oChildLayerDesc.osName.c_str());
    }
    if( oSetLayersInIteration.find( oChildLayerDesc.osName ) !=
                oSetLayersInIteration.end() )
    {
        CPLDebug("GMLAS", "Unexpected at line %d", __LINE__);
        return true;
    }

    std::set<CPLString> oSetLayersInIterationSub(oSetLayersInIteration);
    oSetLayersInIterationSub.insert( oChildLayerDesc.osName );

    if( aoLayerComponents.empty() )
    {
        aoLayerComponents = SplitXPath(oLayerDesc.osXPath);
        aoLayerComponents.insert( aoLayerComponents.begin(),
                                  aoPrefixComponents.begin(),
                                  aoPrefixComponents.end() );
    }

    XPathComponents aoFieldComponents = SplitXPath(oField.GetXPath());
    aoFieldComponents.insert( aoFieldComponents.begin(),
                              aoPrefixComponents.begin(),
                              aoPrefixComponents.end() );

    CPLString osParentPKID (
        poFeature->GetFieldAsString( nParentPKIDIdx ) );
    poRelLayer->SetAttributeFilter(
        CPLSPrintf( "%s = '%s'",
                    oChildLayerDesc.osParentPKIDName.c_str(),
                    osParentPKID.c_str() ) );
    poRelLayer->ResetReading();

    OGRFeature* poChildFeature = poRelLayer->GetNextFeature();
    XPathComponents aoNewInitialContext;
    if( poChildFeature != nullptr )
    {
        if( aoFieldComponents.size() == aoLayerComponents.size() + 1 &&
            oField.GetRepetitionOnSequence() )
        {
            /* Case of
            <xs:element name="sequence_unbounded_dt_1">
                <xs:complexType>
                    <xs:sequence maxOccurs="unbounded">
                        <xs:element name="subelement"
                                    type="xs:dateTime"/>
                    </xs:sequence>
                </xs:complexType>
            </xs:element>
            */
            aoNewInitialContext = aoFieldComponents;
        }
        else if( aoFieldComponents.size() ==
                                        aoLayerComponents.size() + 2 )
        {
            /* Case of
            <xs:element name="sequence_1_dt_unbounded">
                <xs:complexType>
                    <xs:sequence>
                        <xs:element name="subelement"
                                    type="xs:dateTime"
                                    maxOccurs="unbounded"/>
                    </xs:sequence>
                </xs:complexType>
            </xs:element>
            */
            aoNewInitialContext = aoFieldComponents;
            aoNewInitialContext.resize(
                                    aoNewInitialContext.size() - 1 );
        }
        else
        {
            /* Case of
            <xs:element name="unbounded_sequence_1_dt"
                        maxOccurs="unbounded">
                <xs:complexType>
                    <xs:sequence>
                        <xs:element name="subelement"
                                    type="xs:dateTime"/>
                    </xs:sequence>
                </xs:complexType>
            </xs:element>
            */
            aoNewInitialContext = aoLayerComponents;
        }


        WriteClosingAndStartingTags( aoCurComponents,
                                        aoNewInitialContext,
                                        bCurIsRegularField );

        bAtLeastOneFieldWritten = true;
        aoCurComponents = aoNewInitialContext;
        bCurIsRegularField = false;
    }

    while( poChildFeature )
    {
        bool bRet = WriteFeature(poChildFeature,
                                 oChildLayerDesc,
                                 oSetLayersInIterationSub,
                                 aoNewInitialContext,
                                 aoPrefixComponents,
                                 nRecLevel + 1);

        delete poChildFeature;
        if( !bRet )
            return false;

        poChildFeature = poRelLayer->GetNextFeature();
    }
    poRelLayer->ResetReading();

    return true;
}

/************************************************************************/
/*                       GetFilteredLayer()                             */
/************************************************************************/

OGRLayer* GMLASWriter::GetFilteredLayer(
                            OGRLayer* poSrcLayer,
                            const CPLString& osFilter,
                            const std::set<CPLString>& oSetLayersInIteration)
{
    if( oSetLayersInIteration.find(poSrcLayer->GetName()) ==
                                    oSetLayersInIteration.end() )
    {
        poSrcLayer->SetAttributeFilter(osFilter);
        poSrcLayer->ResetReading();
        return poSrcLayer;
    }

    // RDBMS drivers will really create a new iterator independent of the
    // underlying layer when using a SELECT statement
    GDALDriver* poDriver = m_poSrcDS->GetDriver();
    if( poDriver != nullptr &&
        ( EQUAL( poDriver->GetDescription(), "SQLite" ) ||
          EQUAL( poDriver->GetDescription(), "PostgreSQL" ) ) )
    {
        CPLString osSQL;
        osSQL.Printf("SELECT * FROM \"%s\" WHERE %s",
                     poSrcLayer->GetName(), osFilter.c_str());
        return m_poSrcDS->ExecuteSQL(osSQL, nullptr, nullptr);
    }

    // TODO ?
    CPLDebug("GMLAS", "Cannot recursively iterate on %s on this driver",
             poSrcLayer->GetName());
    return nullptr;
}

/************************************************************************/
/*                      ReleaseFilteredLayer()                          */
/************************************************************************/

void GMLASWriter::ReleaseFilteredLayer(OGRLayer* poSrcLayer,
                                       OGRLayer* poIterLayer)
{
    if( poIterLayer != poSrcLayer )
        m_poSrcDS->ReleaseResultSet(poIterLayer);
    else
        poSrcLayer->ResetReading();
}

/************************************************************************/
/*                     WriteFieldWithLink()                             */
/************************************************************************/

bool GMLASWriter::WriteFieldWithLink(
                        OGRFeature* poFeature,
                        const GMLASField& oField,
                        const LayerDescription& oLayerDesc,
                        XPathComponents& aoLayerComponents,
                        XPathComponents& aoCurComponents,
                        const XPathComponents& aoPrefixComponents,
                        const std::set<CPLString>& oSetLayersInIteration,
                        int nRecLevel,
                        bool& bAtLeastOneFieldWritten,
                        bool& bCurIsRegularField)
{
    const auto oIter = m_oMapXPathToIdx.find( oField.GetRelatedClassXPath() );
    if( oIter == m_oMapXPathToIdx.end() )
    {
        // Not necessary to be more verbose in case of truncated
        // source dataset
        CPLDebug("GMLAS", "No child layer of %s matching xpath = %s",
                    oLayerDesc.osName.c_str(),
                    oField.GetRelatedClassXPath().c_str());
        return true;
    }

    const LayerDescription& oChildLayerDesc =
                                    m_aoLayerDesc[oIter->second];
    OGRLayer* poRelLayer = GetLayerByName( oChildLayerDesc.osName );
    if( poRelLayer == nullptr )
    {
        // Not necessary to be more verbose in case of truncated
        // source dataset
        CPLDebug("GMLAS", "Referenced layer %s of %s not found",
                    oChildLayerDesc.osName.c_str(),
                    oLayerDesc.osName.c_str());
        return true;
    }

    const int nFieldIdx = oLayerDesc.GetOGRIdxFromFieldName(oField.GetName());
    XPathComponents aoFieldComponents = SplitXPath(oField.GetXPath());
    aoFieldComponents.insert( aoFieldComponents.begin(),
                              aoPrefixComponents.begin(),
                              aoPrefixComponents.end() );

    if( nFieldIdx < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Missing field %s for layer %s",
                    oField.GetName().c_str(),
                    oLayerDesc.osName.c_str());
        return true;
    }
    if( !poFeature->IsFieldSetAndNotNull(nFieldIdx) )
    {
        // Not an error (unless the field is required)
        return true;
    }
    if( oLayerDesc.osPKIDName.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Missing %s for layer %s",
                    szLAYER_PKID_NAME,
                    oLayerDesc.osName.c_str());
        return true;
    }
    if( oChildLayerDesc.osPKIDName.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Missing %s for layer %s",
                    szLAYER_PKID_NAME,
                    oChildLayerDesc.osName.c_str());
        return true;
    }
    if( aoFieldComponents.size() < 2 )
    {
        // Shouldn't happen for well behaved metadata
        CPLDebug("GMLAS", "Unexpected at line %d", __LINE__);
        return true;
    }
    if( oChildLayerDesc.osXPath.empty() ||
        aoFieldComponents.back() != SplitXPath(oChildLayerDesc.osXPath).front())
    {
        // Shouldn't happen for well behaved metadata
        CPLDebug("GMLAS", "Unexpected at line %d", __LINE__);
        return true;
    }

    CPLString osChildPKID (
        poFeature->GetFieldAsString( nFieldIdx ) );
    const CPLString osFilter( CPLSPrintf( "%s = '%s'",
                                          oChildLayerDesc.osPKIDName.c_str(),
                                          osChildPKID.c_str() ) );
    OGRLayer* poIterLayer = GetFilteredLayer(poRelLayer, osFilter,
                                             oSetLayersInIteration);
    if( poIterLayer == nullptr )
    {
        return true;
    }

    std::set<CPLString> oSetLayersInIterationSub(oSetLayersInIteration);
    oSetLayersInIterationSub.insert( oChildLayerDesc.osName );

    XPathComponents aoPrefixComponentsNew(aoFieldComponents);
    aoPrefixComponentsNew.resize( aoPrefixComponentsNew.size() - 1 );

    if( aoLayerComponents.empty() )
    {
        aoLayerComponents = SplitXPath(oLayerDesc.osXPath);
        aoLayerComponents.insert( aoLayerComponents.begin(),
                                  aoPrefixComponents.begin(),
                                  aoPrefixComponents.end() );
    }

    OGRFeature* poChildFeature = poIterLayer->GetNextFeature();
    XPathComponents aoInitialComponents;
    const bool bHasChild = poChildFeature != nullptr;
    if( bHasChild )
    {
        aoInitialComponents = aoFieldComponents;
        if( !aoInitialComponents.empty() )
            aoInitialComponents.resize( aoInitialComponents.size()-1 );
        WriteClosingAndStartingTags( aoCurComponents,
                                     aoInitialComponents,
                                     bCurIsRegularField );
    }

    bool bRet = true;
    while( poChildFeature )
    {
        bRet = WriteFeature(poChildFeature,
                            oChildLayerDesc,
                            oSetLayersInIterationSub,
                            aoInitialComponents,
                            aoPrefixComponentsNew,
                            nRecLevel + 1);

        delete poChildFeature;
        if( !bRet )
            break;
        poChildFeature = poIterLayer->GetNextFeature();
    }
    ReleaseFilteredLayer(poRelLayer, poIterLayer);

    if( bHasChild )
    {
        bAtLeastOneFieldWritten = true;
        aoCurComponents = aoInitialComponents;
        bCurIsRegularField = false;
    }

    return bRet;
}

/************************************************************************/
/*                   WriteFieldJunctionTable()                          */
/************************************************************************/

bool GMLASWriter::WriteFieldJunctionTable(
                        OGRFeature* poFeature,
                        const GMLASField& oField,
                        const LayerDescription& oLayerDesc,
                        XPathComponents& /*aoLayerComponents */,
                        XPathComponents& aoCurComponents,
                        const XPathComponents& aoPrefixComponents,
                        const std::set<CPLString>& oSetLayersInIteration,
                        int nRecLevel,
                        bool& bAtLeastOneFieldWritten,
                        bool& bCurIsRegularField)
{
    const auto oIter = m_oMapXPathToIdx.find( oField.GetRelatedClassXPath() );
    if( oIter == m_oMapXPathToIdx.end() )
    {
        // Not necessary to be more verbose in case of truncated
        // source dataset
        CPLDebug("GMLAS", "No related layer of %s matching xpath = %s",
                    oLayerDesc.osName.c_str(),
                    oField.GetRelatedClassXPath().c_str());
        return true;
    }

    const LayerDescription& oRelLayerDesc = m_aoLayerDesc[oIter->second];
    OGRLayer* poRelLayer = GetLayerByName(oRelLayerDesc.osName);
    OGRLayer* poJunctionLayer = GetLayerByName( oField.GetJunctionLayer() );
    if( poRelLayer == nullptr )
    {
        // Not necessary to be more verbose in case of truncated
        // source dataset
        CPLDebug("GMLAS", "Referenced layer %s of %s not found",
                    oRelLayerDesc.osName.c_str(),
                    oLayerDesc.osName.c_str());
        return true;
    }
    if( poJunctionLayer == nullptr )
    {
        // Not necessary to be more verbose in case of truncated
        // source dataset
        CPLDebug("GMLAS", "Junction layer %s not found",
                    oField.GetJunctionLayer().c_str());
        return true;
    }

    int nIndexPKID = -1;
    if( oLayerDesc.osPKIDName.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Missing %s for layer %s",
                    szLAYER_PKID_NAME,
                    oLayerDesc.osName.c_str());
        return true;
    }
    if( (nIndexPKID =
            oLayerDesc.GetOGRIdxFromFieldName(oLayerDesc.osPKIDName)) < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot find %s='%s' in layer %s",
                    szLAYER_PKID_NAME,
                    oLayerDesc.osPKIDName.c_str(),
                    oLayerDesc.osName.c_str());
        return true;
    }
    if( !poFeature->IsFieldSetAndNotNull(nIndexPKID) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Field '%s' in layer %s is not set for "
                    "feature " CPL_FRMT_GIB,
                    oLayerDesc.osPKIDName.c_str(),
                    oLayerDesc.osName.c_str(),
                    poFeature->GetFID());
        return true;
    }
    if( oRelLayerDesc.osPKIDName.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Missing %s for layer %s",
                    szLAYER_PKID_NAME,
                    oRelLayerDesc.osName.c_str());
        return true;
    }
    if( oSetLayersInIteration.find( oRelLayerDesc.osName ) !=
                oSetLayersInIteration.end() )
    {
        // TODO... cycle situation. We will need to open a new
        // source dataset or something
        return true;
    }

    std::set<CPLString> oSetLayersInIterationSub(oSetLayersInIteration);
    oSetLayersInIterationSub.insert( oRelLayerDesc.osName );

    poJunctionLayer->SetAttributeFilter(
        CPLSPrintf( "%s = '%s'",
                    szPARENT_PKID,
                    poFeature->GetFieldAsString(nIndexPKID) ) );
    poJunctionLayer->ResetReading();
    std::vector<CPLString> aoChildPKIDs;
    while( true )
    {
        OGRFeature* poJunctionFeature = poJunctionLayer->GetNextFeature();
        if( poJunctionFeature == nullptr )
            break;

        aoChildPKIDs.push_back(
            poJunctionFeature->GetFieldAsString(szCHILD_PKID) );

        delete poJunctionFeature;
    }
    poJunctionLayer->ResetReading();

    bool bRet = true;
    bool bHasChild = false;
    XPathComponents aoInitialComponents;
    for(size_t j=0; bRet && j<aoChildPKIDs.size(); j++ )
    {
        CPLString osFilter;
        osFilter.Printf( "%s = '%s'",
                        oRelLayerDesc.osPKIDName.c_str(),
                        aoChildPKIDs[j].c_str() );
        OGRLayer* poIterLayer = GetFilteredLayer(poRelLayer, osFilter,
                                                 oSetLayersInIteration);
        if( poIterLayer == nullptr )
        {
            return true;
        }

        OGRFeature* poChildFeature = poIterLayer->GetNextFeature();
        if( poChildFeature != nullptr )
        {
            if( !bHasChild )
            {
                bHasChild = true;

                aoInitialComponents = SplitXPath(oField.GetXPath());
                aoInitialComponents.insert( aoInitialComponents.begin(),
                                            aoPrefixComponents.begin(),
                                            aoPrefixComponents.end() );

                if( !aoInitialComponents.empty() )
                    aoInitialComponents.resize( aoInitialComponents.size()-1 );
                WriteClosingAndStartingTags( aoCurComponents,
                                            aoInitialComponents,
                                            bCurIsRegularField );
            }

            bRet = WriteFeature( poChildFeature,
                                 oRelLayerDesc,
                                 oSetLayersInIterationSub,
                                 XPathComponents(),
                                 XPathComponents(),
                                 nRecLevel + 1 );

            delete poChildFeature;
            ReleaseFilteredLayer(poRelLayer, poIterLayer);
        }
        else
        {
            ReleaseFilteredLayer(poRelLayer, poIterLayer);
        }
    }

    if( bHasChild )
    {
        bAtLeastOneFieldWritten = true;
        aoCurComponents = aoInitialComponents;
        bCurIsRegularField = false;
    }

    return bRet;
}

/************************************************************************/
/*                           PrintIndent()                              */
/************************************************************************/

void GMLASWriter::PrintIndent(VSILFILE* fp)
{
    for( int i = 0; i < m_nIndentLevel; i++ )
    {
        VSIFWriteL(m_osIndentation.c_str(), 1, m_osIndentation.size(), fp);
    }
}

/************************************************************************/
/*                            PrintLine()                               */
/************************************************************************/

void GMLASWriter::PrintLine(VSILFILE* fp, const char *fmt, ...)
{
    CPLString osWork;
    va_list args;

    va_start( args, fmt );
    osWork.vPrintf( fmt, args );
    va_end( args );

    VSIFWriteL(osWork.c_str(), 1, osWork.size(), fp);
    VSIFWriteL(m_osEOL.c_str(), 1, m_osEOL.size(), fp);
}

} /* namespace GMLAS */


/************************************************************************/
/*                           GMLASFakeDataset                           */
/************************************************************************/

class GMLASFakeDataset final: public GDALDataset
{
    public:
        GMLASFakeDataset() {}
};

/************************************************************************/
/*                        OGRGMLASDriverCreateCopy()                    */
/************************************************************************/

GDALDataset *OGRGMLASDriverCreateCopy(
                          const char * pszFilename,
                          GDALDataset *poSrcDS,
                          int /*bStrict*/,
                          char ** papszOptions,
                          GDALProgressFunc pfnProgress,
                          void * pProgressData )
{
    if( strcmp(CPLGetExtension(pszFilename), "xsd") == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, ".xsd extension is not valid");
        return nullptr;
    }

    // Strip GMLAS: prefix if specified
    if( STARTS_WITH_CI(pszFilename, szGMLAS_PREFIX) )
        pszFilename += strlen(szGMLAS_PREFIX);

    GMLAS::GMLASWriter oWriter(pszFilename, poSrcDS, papszOptions);
    if( !oWriter.Write(pfnProgress, pProgressData) )
        return nullptr;

    if( CPLString(pszFilename) == "/vsistdout/" ||
        // This option is mostly useful for tests where we don't want
        // WFS 2.0 schemas to be pulled from the network
        !CPLFetchBool(papszOptions, "REOPEN_DATASET_WITH_GMLAS", true) )
    {
        return new GMLASFakeDataset();
    }
    else
    {
        GDALOpenInfo oOpenInfo(
            (CPLString(szGMLAS_PREFIX) + pszFilename).c_str(), GA_ReadOnly );
        OGRGMLASDataSource* poOutDS = new OGRGMLASDataSource();
        if( !poOutDS->Open(  &oOpenInfo ) )
        {
            delete poOutDS;
            poOutDS = nullptr;
        }
        return poOutDS;
    }
}
