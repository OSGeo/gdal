/******************************************************************************
 *
 * Project:  KML Translator
 * Purpose:  Implements OGRLIBKMLDriver
 * Author:   Brian Case, rush at winkey dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Brian Case
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at spatialys.com>
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
 *****************************************************************************/

#include "libkml_headers.h"

#include <string>
#include "ogr_libkml.h"
#include "ogrlibkmlstyle.h"
#include "ogr_p.h"
#include "cpl_vsi_error.h"

CPL_CVSID("$Id$")

using kmlbase::Attributes;
using kmldom::ContainerPtr;
using kmldom::DocumentPtr;
using kmldom::ElementPtr;
using kmldom::FeaturePtr;
using kmldom::FolderPtr;
using kmldom::KmlFactory;
using kmldom::KmlPtr;
using kmldom::LinkPtr;
using kmldom::LinkSnippetPtr;
using kmldom::NetworkLinkControlPtr;
using kmldom::NetworkLinkPtr;
using kmldom::SchemaPtr;
using kmldom::SnippetPtr;
using kmldom::StyleSelectorPtr;
using kmlengine::KmzFile;

/************************************************************************/
/*                           OGRLIBKMLParse()                           */
/************************************************************************/

static ElementPtr OGRLIBKMLParse(const std::string& oKml, std::string *posError)
{
    try
    {
        // To allow reading files using an explicit namespace prefix like <kml:kml>
        // we need to use ParseNS (see #6981). But if we use ParseNS, we have
        // issues reading gx: elements. So use ParseNS only when we have errors
        // with Parse. This is not completely satisfactory...
        ElementPtr element = kmldom::Parse(oKml, posError);
        if( !element )
            element = kmldom::ParseNS(oKml, posError);
        return element;
    }
    catch(const std::exception& ex)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "LIBKML: libstdc++ exception during Parse() : %s", ex.what());
        return nullptr;
    }
}

/******************************************************************************
 OGRLIBKMLDataSource Constructor

 Args:          none

 Returns:       nothing

******************************************************************************/

OGRLIBKMLDataSource::OGRLIBKMLDataSource( KmlFactory * poKmlFactory ) :
    m_pszName(nullptr),
    papoLayers(nullptr),
    nLayers(0),
    nAllocated(0),
    bUpdate(false),
    bUpdated(false),
    m_papszOptions(nullptr),
    m_isKml(false),
    m_poKmlDSKml(nullptr),
    m_poKmlDSContainer(nullptr),
    m_poKmlUpdate(nullptr),
    m_isKmz(false),
    m_poKmlDocKml(nullptr),
    m_poKmlDocKmlRoot(nullptr),
    m_poKmlStyleKml(nullptr),
    pszStylePath(const_cast<char *>("")),
    m_isDir(false),
    m_poKmlFactory(poKmlFactory)
{}

/************************************************************************/
/*                       OGRLIBKMLPreProcessInput()                     */
/************************************************************************/

// Substitute <snippet> by deprecated <Snippet> since libkml currently
// only supports Snippet but ogckml22.xsd has deprecated it in favor of snippet.
static void OGRLIBKMLPreProcessInput( std::string& oKml )
{
    size_t nPos = 0;
    while( true )
    {
        nPos = oKml.find("<snippet>", nPos);
        if( nPos == std::string::npos )
        {
            break;
        }
        oKml[nPos+1] = 'S';
        nPos = oKml.find("</snippet>", nPos);
        if( nPos == std::string::npos )
        {
            break;
        }
        oKml[nPos+2] = 'S';
    }

    // Workaround Windows specific issue with libkml (at least the version
    // used by OSGeo4W at time of writing), where tabulations as
    // coordinate separators aren't properly handled
    //(see https://trac.osgeo.org/gdal/ticket/7231)
    // Another Windows specific issue is that if the content of
    // <coordinates> is non-empty and does not contain any digit,
    // libkml hangs (see https://trac.osgeo.org/gdal/ticket/7232)
    nPos = 0;
    while( true )
    {
        nPos = oKml.find("<coordinates>", nPos);
        if( nPos == std::string::npos )
        {
            break;
        }
        size_t nPosEnd = oKml.find("</coordinates>", nPos);
        if( nPosEnd == std::string::npos )
        {
            break;
        }
        nPos += strlen("<coordinates>");
        size_t nPosAfterCoordinates = nPos;
        bool bDigitFound = false;
        for( ; nPos < nPosEnd; nPos++ )
        {
            char ch = oKml[nPos];
            if( ch >= '0' && ch <= '9' )
                bDigitFound = true;
            else if( ch == '\t' || ch == '\n' )
                oKml[nPos] = ' ';
        }
        if( !bDigitFound )
        {
            oKml.replace(nPosAfterCoordinates,
                         nPosEnd + strlen("</coordinates>") -
                                nPosAfterCoordinates,
                         "</coordinates>");
            nPos = nPosAfterCoordinates + strlen("</coordinates>");
        }
    }

    // Some non conformant file may contain MultiPolygon/MultiLineString/MultiPoint
    // See https://github.com/OSGeo/gdal/issues/4031
    // Replace them by MultiGeometry.
    nPos = 0;
    while( true )
    {
        const char* pszStartTag = "<MultiPolygon>";
        const char* pszEndTag = "";
        auto nNewPos = oKml.find(pszStartTag, nPos);
        if( nNewPos != std::string::npos )
        {
            pszEndTag = "</MultiPolygon>";
        }
        else
        {
            pszStartTag = "<MultiLineString>";
            nNewPos = oKml.find(pszStartTag, nPos);
            if( nNewPos != std::string::npos )
            {
                pszEndTag = "</MultiLineString>";
            }
            else
            {
                pszStartTag = "<MultiPoint>";
                nNewPos = oKml.find(pszStartTag, nPos);
                if( nNewPos != std::string::npos )
                    pszEndTag = "</MultiPoint>";
                else
                    break;
            }
        }
        nPos = nNewPos;
        oKml.replace(nPos, strlen(pszStartTag), "<MultiGeometry>");
        nPos = oKml.find(pszEndTag, nPos);
        if( nPos == std::string::npos )
            break;
        oKml.replace(nPos, strlen(pszEndTag), "</MultiGeometry>");
    }
}

/************************************************************************/
/*                       OGRLIBKMLRemoveSpaces()                        */
/************************************************************************/

static void OGRLIBKMLRemoveSpaces(
    std::string& oKml, const std::string& osNeedle )
{
    size_t nPos = 0;
    while( true )
    {
        nPos = oKml.find("<" + osNeedle, nPos);
        if( nPos == std::string::npos )
        {
            break;
        }
        const size_t nPosOri = nPos;
        nPos = oKml.find(">", nPos);
        if( nPos == std::string::npos || oKml[nPos+1] != '\n' )
        {
            break;
        }
        oKml = oKml.substr(0, nPos) + ">" + oKml.substr(nPos + strlen(">\n"));
        CPLString osSpaces;
        for( size_t nPosTmp = nPosOri - 1; oKml[nPosTmp] == ' '; nPosTmp-- )
        {
            osSpaces += ' ';
        }
        nPos = oKml.find(osSpaces + "</" + osNeedle +">", nPos);
        if( nPos != std::string::npos )
            oKml =
                oKml.substr(0, nPos) + "</" + osNeedle +">" +
                oKml.substr(nPos + osSpaces.size() + strlen("</>") +
                            osNeedle.size());
        else
            break;
    }
}

/************************************************************************/
/*                      OGRLIBKMLPostProcessOutput()                    */
/************************************************************************/

// Substitute deprecated <Snippet> by <snippet> since libkml currently
// only supports Snippet but ogckml22.xsd has deprecated it in favor of snippet.
static void OGRLIBKMLPostProcessOutput( std::string& oKml )
{
    // Manually add <?xml> node since libkml does not produce it currently
    // and this is useful in some circumstances (#5407).
    if( !(oKml[0] == '<' && oKml[1] == '?') )
        oKml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" + oKml;

    size_t nPos = 0;
    while( true )
    {
        nPos = oKml.find("<Snippet>", nPos);
        if( nPos == std::string::npos )
        {
            break;
        }
        oKml[nPos+1] = 's';
        nPos = oKml.find("</Snippet>", nPos);
        if( nPos == std::string::npos )
        {
            break;
        }
        oKml[nPos+2] = 's';
    }

    // Fix indentation problems.
    OGRLIBKMLRemoveSpaces(oKml, "snippet");
    OGRLIBKMLRemoveSpaces(oKml, "linkSnippet");
    OGRLIBKMLRemoveSpaces(oKml, "SimpleData");
}

/******************************************************************************
 Method to write a single file ds .kml at ds destroy.

 Args:          none

 Returns:       nothing

******************************************************************************/

void OGRLIBKMLDataSource::WriteKml()
{
    std::string oKmlFilename = m_pszName;

    if( m_poKmlDSContainer
        && m_poKmlDSContainer->IsA( kmldom::Type_Document ) )
    {
        DocumentPtr poKmlDocument = AsDocument( m_poKmlDSContainer );

        ParseDocumentOptions(m_poKmlDSKml, poKmlDocument);

        for( int iLayer = 0; iLayer < nLayers; iLayer++ )
        {
            SchemaPtr poKmlSchema = nullptr;

            if( ( poKmlSchema = papoLayers[iLayer]->GetKmlSchema() ) )
            {
                const size_t nKmlSchemas =
                    poKmlDocument->get_schema_array_size();
                SchemaPtr poKmlSchema2 = nullptr;

                for( size_t iKmlSchema = 0;
                     iKmlSchema < nKmlSchemas;
                     iKmlSchema++ )
                {
                    poKmlSchema2 =
                        poKmlDocument->get_schema_array_at( iKmlSchema );
                    if( poKmlSchema2 == poKmlSchema )
                        break;
                }

                if( poKmlSchema2 != poKmlSchema )
                    poKmlDocument->add_schema( poKmlSchema );
            }

            papoLayers[iLayer]->Finalize(poKmlDocument);
        }
    }
    else
    {
        ParseDocumentOptions(m_poKmlDSKml, nullptr);
    }

    std::string oKmlOut;
    oKmlOut = kmldom::SerializePretty( m_poKmlDSKml );
    OGRLIBKMLPostProcessOutput(oKmlOut);

    if( !oKmlOut.empty() )
    {
        VSILFILE* fp = VSIFOpenExL( oKmlFilename.c_str(), "wb", true );
        if( fp == nullptr )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Error writing %s: %s", oKmlFilename.c_str(),
                      VSIGetLastErrorMsg() );
            return;
        }

        VSIFWriteL(oKmlOut.data(), 1, oKmlOut.size(), fp);
        VSIFCloseL(fp);
    }
}

/******************************************************************************/
/*                      OGRLIBKMLCreateOGCKml22()                             */
/******************************************************************************/

static KmlPtr OGRLIBKMLCreateOGCKml22(
    KmlFactory* poFactory, char** papszOptions = nullptr )
{
    const char* pszAuthorName = CSLFetchNameValue(papszOptions, "AUTHOR_NAME");
    const char* pszAuthorURI = CSLFetchNameValue(papszOptions, "AUTHOR_URI");
    const char* pszAuthorEmail =
        CSLFetchNameValue(papszOptions, "AUTHOR_EMAIL");
    const char* pszLink = CSLFetchNameValue(papszOptions, "LINK");
    const bool bWithAtom =
        pszAuthorName != nullptr ||
        pszAuthorURI != nullptr ||
        pszAuthorEmail != nullptr ||
        pszLink != nullptr;

    KmlPtr kml = poFactory->CreateKml();
    if( bWithAtom )
    {
        const char* kAttrs[] = {
            "xmlns", "http://www.opengis.net/kml/2.2",
            "xmlns:atom", "http://www.w3.org/2005/Atom", nullptr };
        kml->AddUnknownAttributes(Attributes::Create(kAttrs));
    }
    else
    {
        const char* kAttrs[] =
            { "xmlns", "http://www.opengis.net/kml/2.2", nullptr };
        kml->AddUnknownAttributes(Attributes::Create(kAttrs));
    }
    return kml;
}

/******************************************************************************
 Method to write a ds .kmz at ds destroy.

 Args:          none

 Returns:       nothing

******************************************************************************/

void OGRLIBKMLDataSource::WriteKmz()
{
    void* hZIP = CPLCreateZip( m_pszName, nullptr );

    if( !hZIP )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Error creating %s: %s",
                  m_pszName, VSIGetLastErrorMsg() );
        return;
    }

    /***** write out the doc.kml ****/
    const char *pszUseDocKml =
        CPLGetConfigOption( "LIBKML_USE_DOC.KML", "yes" );

    if( CPLTestBool( pszUseDocKml ) && (m_poKmlDocKml || m_poKmlUpdate) )
    {
        // If we do not have the doc.kmlroot
        // make it and add the container.
        if( !m_poKmlDocKmlRoot )
        {
            m_poKmlDocKmlRoot =
                OGRLIBKMLCreateOGCKml22(m_poKmlFactory, m_papszOptions);

            if( m_poKmlDocKml )
            {
                AsKml( m_poKmlDocKmlRoot )->set_feature( m_poKmlDocKml );
            }

            ParseDocumentOptions(
                AsKml( m_poKmlDocKmlRoot ), AsDocument(m_poKmlDocKml));
        }

        std::string oKmlOut = kmldom::SerializePretty( m_poKmlDocKmlRoot );
        OGRLIBKMLPostProcessOutput(oKmlOut);

        if( CPLCreateFileInZip( hZIP, "doc.kml", nullptr ) != CE_None ||
            CPLWriteFileInZip( hZIP, oKmlOut.data(),
                               static_cast<int>(oKmlOut.size()) ) != CE_None )
            CPLError( CE_Failure, CPLE_FileIO,
                      "ERROR adding %s to %s", "doc.kml", m_pszName );
        CPLCloseFileInZip(hZIP);
    }

    /***** loop though the layers and write them *****/
    for( int iLayer = 0; iLayer < nLayers && !m_poKmlUpdate; iLayer++ )
    {
        ContainerPtr poKmlContainer = papoLayers[iLayer]->GetKmlLayer();

        if( poKmlContainer->IsA( kmldom::Type_Document ) )
        {
            DocumentPtr poKmlDocument = AsDocument( poKmlContainer );
            SchemaPtr poKmlSchema = papoLayers[iLayer]->GetKmlSchema();

            if( !poKmlDocument->get_schema_array_size() &&
                poKmlSchema &&
                poKmlSchema->get_simplefield_array_size() )
            {
                poKmlDocument->add_schema( poKmlSchema );
            }

            papoLayers[iLayer]->Finalize(poKmlDocument);
        }

        // If we do not have the layers root
        // make it and add the container.
        KmlPtr poKmlKml = nullptr;

        if( !( poKmlKml = AsKml( papoLayers[iLayer]->GetKmlLayerRoot() ) ) )
        {
            poKmlKml = OGRLIBKMLCreateOGCKml22(m_poKmlFactory);

            poKmlKml->set_feature( poKmlContainer );
        }

        std::string oKmlOut = kmldom::SerializePretty( poKmlKml );
        OGRLIBKMLPostProcessOutput(oKmlOut);

        if( iLayer == 0 && CPLTestBool( pszUseDocKml ) )
            CPLCreateFileInZip( hZIP, "layers/", nullptr );

        const char* pszLayerFileName = nullptr;
        if( CPLTestBool( pszUseDocKml ) )
            pszLayerFileName =
                CPLSPrintf("layers/%s", papoLayers[iLayer]->GetFileName());
        else
            pszLayerFileName = papoLayers[iLayer]->GetFileName();

        if( CPLCreateFileInZip( hZIP, pszLayerFileName , nullptr ) != CE_None ||
             CPLWriteFileInZip( hZIP, oKmlOut.data(),
                                static_cast<int>(oKmlOut.size()) ) != CE_None )
            CPLError(
                CE_Failure, CPLE_FileIO,
                "ERROR adding %s to %s",
                papoLayers[iLayer]->GetFileName(), m_pszName );
        CPLCloseFileInZip(hZIP);
    }

    /***** write the style table *****/
    if( m_poKmlStyleKml )
    {
        KmlPtr poKmlKml = OGRLIBKMLCreateOGCKml22(m_poKmlFactory);

        poKmlKml->set_feature( m_poKmlStyleKml );
        std::string oKmlOut = kmldom::SerializePretty( poKmlKml );
        OGRLIBKMLPostProcessOutput(oKmlOut);

        if( CPLCreateFileInZip( hZIP, "style/", nullptr ) != CE_None ||
            CPLCreateFileInZip( hZIP, "style/style.kml", nullptr ) != CE_None ||
            CPLWriteFileInZip( hZIP, oKmlOut.data(),
                               static_cast<int>(oKmlOut.size()) ) != CE_None )
            CPLError( CE_Failure, CPLE_FileIO,
                      "ERROR adding %s to %s", "style/style.kml", m_pszName );
        CPLCloseFileInZip(hZIP);
    }

    CPLCloseZip(hZIP);
}

/******************************************************************************
 Method to write a dir ds at ds destroy.

 Args:          none

 Returns:       nothing

******************************************************************************/

void OGRLIBKMLDataSource::WriteDir()
{
    /***** write out the doc.kml ****/
    const char *pszUseDocKml =
        CPLGetConfigOption( "LIBKML_USE_DOC.KML", "yes" );

    if( CPLTestBool( pszUseDocKml ) && (m_poKmlDocKml || m_poKmlUpdate) )
    {
        // If we don't have the doc.kml root
        // make it and add the container.
        if( !m_poKmlDocKmlRoot )
        {
            m_poKmlDocKmlRoot =
                OGRLIBKMLCreateOGCKml22(m_poKmlFactory, m_papszOptions);
            if( m_poKmlDocKml )
                AsKml( m_poKmlDocKmlRoot )->set_feature( m_poKmlDocKml );

            ParseDocumentOptions(
                AsKml( m_poKmlDocKmlRoot ), AsDocument(m_poKmlDocKml));
        }

        std::string oKmlOut = kmldom::SerializePretty( m_poKmlDocKmlRoot );
        OGRLIBKMLPostProcessOutput(oKmlOut);

        const char *pszOutfile = CPLFormFilename( m_pszName, "doc.kml", nullptr );

        VSILFILE* fp = VSIFOpenExL( pszOutfile, "wb", true );
        if( fp == nullptr )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Error writing %s to %s: %s", "doc.kml", m_pszName,
                      VSIGetLastErrorMsg() );
            return;
        }

        VSIFWriteL(oKmlOut.data(), 1, oKmlOut.size(), fp);
        VSIFCloseL(fp);
    }

    /***** loop though the layers and write them *****/
    for( int iLayer = 0; iLayer < nLayers && !m_poKmlUpdate; iLayer++ )
    {
        ContainerPtr poKmlContainer = papoLayers[iLayer]->GetKmlLayer();

        if( poKmlContainer->IsA( kmldom::Type_Document ) )
        {
            DocumentPtr poKmlDocument = AsDocument( poKmlContainer );
            SchemaPtr poKmlSchema = papoLayers[iLayer]->GetKmlSchema();

            if( !poKmlDocument->get_schema_array_size() &&
                poKmlSchema &&
                poKmlSchema->get_simplefield_array_size() )
            {
                poKmlDocument->add_schema( poKmlSchema );
            }

            papoLayers[iLayer]->Finalize(poKmlDocument);
        }

        // If we do not have the layers root
        // make it and add the container.
        KmlPtr poKmlKml = nullptr;

        if( !( poKmlKml = AsKml( papoLayers[iLayer]->GetKmlLayerRoot() ) ) )
        {
            poKmlKml = OGRLIBKMLCreateOGCKml22(m_poKmlFactory);

            poKmlKml->set_feature( poKmlContainer );
        }

        std::string oKmlOut = kmldom::SerializePretty( poKmlKml );
        OGRLIBKMLPostProcessOutput(oKmlOut);

        const char *pszOutfile = CPLFormFilename(
            m_pszName, papoLayers[iLayer]->GetFileName(), nullptr );

        VSILFILE* fp = VSIFOpenL( pszOutfile, "wb" );
        if( fp == nullptr )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "ERROR Writing %s to %s",
                      papoLayers[iLayer]->GetFileName(), m_pszName );
            return;
        }

        VSIFWriteL(oKmlOut.data(), 1, oKmlOut.size(), fp);
        VSIFCloseL(fp);
    }

    /***** write the style table *****/
    if( m_poKmlStyleKml )
    {
        KmlPtr poKmlKml = OGRLIBKMLCreateOGCKml22(m_poKmlFactory);

        poKmlKml->set_feature( m_poKmlStyleKml );
        std::string oKmlOut = kmldom::SerializePretty( poKmlKml );
        OGRLIBKMLPostProcessOutput(oKmlOut);

        const char *pszOutfile =
            CPLFormFilename( m_pszName, "style.kml",  nullptr );

        VSILFILE* fp = VSIFOpenL( pszOutfile, "wb" );
        if( fp == nullptr )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "ERROR Writing %s to %s", "style.kml", m_pszName );
            return;
        }

        VSIFWriteL(oKmlOut.data(), 1, oKmlOut.size(), fp);
        VSIFCloseL(fp);
    }
}

/******************************************************************************
 Method to write the datasource to disk.

 Args:      none

 Returns    nothing

******************************************************************************/

void OGRLIBKMLDataSource::FlushCache()
{
    if( !bUpdated )
        return;

    if( bUpdate && IsKml() )
    {
        WriteKml();
    }
    else if( bUpdate && IsKmz() )
    {
        WriteKmz();
    }
    else if( bUpdate && IsDir() )
    {
        WriteDir();
    }

    bUpdated = false;
}

/******************************************************************************
 OGRLIBKMLDataSource Destructor

 Args:          none

 Returns:       nothing

******************************************************************************/

OGRLIBKMLDataSource::~OGRLIBKMLDataSource()
{
    /***** sync the DS to disk *****/
    OGRLIBKMLDataSource::FlushCache();

    CPLFree( m_pszName );

    if( !EQUAL(pszStylePath, "") )
        CPLFree( pszStylePath );

    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];

    CPLFree( papoLayers );

    CSLDestroy( m_papszOptions );
}

/******************************************************************************
 Method to parse a schemas out of a document.

 Args:          poKmlDocument   pointer to the document to parse

 Returns:       nothing

******************************************************************************/

SchemaPtr OGRLIBKMLDataSource::FindSchema( const char *pszSchemaUrl )
{
    if( !pszSchemaUrl || !*pszSchemaUrl )
        return nullptr;

    char *pszID = nullptr;
    char *pszFile = nullptr;
    char *pszSchemaName = nullptr;
    char *pszPound = nullptr;
    DocumentPtr poKmlDocument = nullptr;
    SchemaPtr poKmlSchemaResult = nullptr;

    if( *pszSchemaUrl == '#' )
    {
        pszID = CPLStrdup( pszSchemaUrl + 1 );

        /***** kml *****/
        if( IsKml() && m_poKmlDSContainer->IsA( kmldom::Type_Document ) )
            poKmlDocument = AsDocument( m_poKmlDSContainer );

        /***** kmz *****/
        else if( ( IsKmz() || IsDir() ) && m_poKmlDocKml
                  && m_poKmlDocKml->IsA( kmldom::Type_Document ) )
            poKmlDocument = AsDocument( m_poKmlDocKml );
    }
    else if( ( pszPound = strchr( const_cast<char *>(pszSchemaUrl), '#' ) )
             != nullptr )
    {
        pszFile = CPLStrdup( pszSchemaUrl );
        pszID = CPLStrdup( pszPound + 1 );
        pszPound = strchr( pszFile, '#' );
        *pszPound = '\0';
    }
    else
    {
        pszSchemaName = CPLStrdup( pszSchemaUrl );

        /***** kml *****/
        if( IsKml() && m_poKmlDSContainer->IsA( kmldom::Type_Document ) )
            poKmlDocument = AsDocument( m_poKmlDSContainer );

        /***** kmz *****/

        else if( ( IsKmz() || IsDir() ) && m_poKmlDocKml
                  && m_poKmlDocKml->IsA( kmldom::Type_Document ) )
            poKmlDocument = AsDocument( m_poKmlDocKml );
    }

    if( poKmlDocument )
    {
        size_t nKmlSchemas = poKmlDocument->get_schema_array_size();

        for( size_t iKmlSchema = 0; iKmlSchema < nKmlSchemas; iKmlSchema++ )
        {
            SchemaPtr poKmlSchema =
                poKmlDocument->get_schema_array_at( iKmlSchema );
            if( poKmlSchema->has_id() && pszID)
            {
                if( EQUAL( pszID, poKmlSchema->get_id().c_str() ) )
                {
                    poKmlSchemaResult = poKmlSchema;
                    break;
                }
            }

            else if( poKmlSchema->has_name() && pszSchemaName)
            {
                if( EQUAL( pszSchemaName, poKmlSchema->get_name().c_str() ) )
                {
                    poKmlSchemaResult = poKmlSchema;
                    break;
                }
            }
        }
    }

    CPLFree( pszFile );
    CPLFree( pszID );
    CPLFree( pszSchemaName );

    return poKmlSchemaResult;
}

/******************************************************************************
 Method to allocate memory for the layer array, create the layer,
 and add it to the layer array.

 Args:          pszLayerName    the name of the layer
                eGType          the layers geometry type
                poOgrDS         pointer to the datasource the layer is in
                poKmlRoot       pointer to the root kml element of the layer
                pszFileName     the filename of the layer
                bNew            true if its a new layer
                bUpdate         true if the layer is writable
                nGuess          a guess at the number of additional layers
                                we are going to need

 Returns:       Pointer to the new layer
******************************************************************************/

OGRLIBKMLLayer *OGRLIBKMLDataSource::AddLayer(
    const char *pszLayerName,
    OGRwkbGeometryType eGType,
    OGRLIBKMLDataSource * poOgrDS,
    ElementPtr poKmlRoot,
    ContainerPtr poKmlContainer,
    const char *pszFileName,
    int bNew,
    int bUpdateIn,
    int nGuess )
{
    // Build unique layer name
    CPLString osUniqueLayername(pszLayerName);
    int nIter = 2;
    while( true )
    {
        if( GetLayerByName(osUniqueLayername) == nullptr )
            break;
        osUniqueLayername = CPLSPrintf("%s (#%d)", pszLayerName, nIter);
        nIter ++;
    }

    /***** check to see if we have enough space to store the layer *****/
    if( nLayers == nAllocated )
    {
        nAllocated += nGuess;
        papoLayers = static_cast<OGRLIBKMLLayer **>(
            CPLRealloc( papoLayers, sizeof(OGRLIBKMLLayer *) * nAllocated ) );
    }

    /***** create the layer *****/
    OGRLIBKMLLayer *poOgrLayer = new OGRLIBKMLLayer( osUniqueLayername,
                                                      eGType,
                                                      poOgrDS,
                                                      poKmlRoot,
                                                      poKmlContainer,
                                                      m_poKmlUpdate,
                                                      pszFileName,
                                                      bNew,
                                                      bUpdateIn );

    /***** add the layer to the array *****/
    const int iLayer = nLayers++;
    papoLayers[iLayer] = poOgrLayer;
    m_oMapLayers[CPLString(osUniqueLayername).toupper()] = poOgrLayer;

    return poOgrLayer;
}

/******************************************************************************
 Method to parse multiple layers out of a container.

 Args:          poKmlContainer  pointer to the container to parse
                poOgrSRS        SRS to use when creating the layer

 Returns:       number of features in the container that are not another
                container

******************************************************************************/

int OGRLIBKMLDataSource::ParseLayers(
    ContainerPtr poKmlContainer,
    OGRSpatialReference * poOgrSRS,
    bool bRecurse)
{
    /***** if container is null just bail now *****/
    if( !poKmlContainer )
        return 0;

    const size_t nKmlFeatures = poKmlContainer->get_feature_array_size();

    /***** loop over the container to separate the style, layers, etc *****/

    int nResult = 0;
    for( size_t iKmlFeature = 0; iKmlFeature < nKmlFeatures; iKmlFeature++ )
    {
        FeaturePtr poKmlFeat =
            poKmlContainer->get_feature_array_at( iKmlFeature );

        /***** container *****/

        if( poKmlFeat->IsA( kmldom::Type_Container ) )
        {
          if( bRecurse )
          {
            /***** see if the container has a name *****/

            std::string oKmlFeatName;
            if( poKmlFeat->has_name() )
            {
                /* Strip leading and trailing spaces */
                const char* l_pszName = poKmlFeat->get_name().c_str();
                while( *l_pszName == ' ' || *l_pszName == '\n' ||
                       *l_pszName == '\r' || *l_pszName == '\t' )
                    l_pszName ++;
                oKmlFeatName = l_pszName;
                int nSize = (int)oKmlFeatName.size();
                while( nSize > 0 &&
                       (oKmlFeatName[nSize-1] == ' ' ||
                        oKmlFeatName[nSize-1] == '\n' ||
                        oKmlFeatName[nSize-1] == '\r' ||
                        oKmlFeatName[nSize-1] == '\t') )
                {
                    nSize--;
                    oKmlFeatName.resize(nSize);
                }
            }
            /***** use the feature index number as the name *****/
            /***** not sure i like this c++ ich *****/
            else
            {
                std::stringstream oOut;
                oOut << iKmlFeature;
                oKmlFeatName = "Layer";
                oKmlFeatName.append(oOut.str());
            }

            /***** create the layer *****/

            AddLayer( oKmlFeatName.c_str(),
                      wkbUnknown, this,
                      nullptr, AsContainer( poKmlFeat ), "", FALSE, bUpdate,
                      static_cast<int>(nKmlFeatures) );

            /***** check if any features are another layer *****/
            ParseLayers( AsContainer(poKmlFeat), poOgrSRS, true );
          }
        }
        else
        {
            nResult++;
        }
    }

    return nResult;
}

/******************************************************************************
 Function to get the container from the kmlroot.

 Args:          poKmlRoot   the root element

 Returns:       root if its a container, if its a kml the container it
                contains, or NULL

******************************************************************************/

static ContainerPtr GetContainerFromRoot(
    KmlFactory *m_poKmlFactory, ElementPtr poKmlRoot )
{
    ContainerPtr poKmlContainer = nullptr;

    const bool bReadGroundOverlay =
        CPLTestBool(CPLGetConfigOption("LIBKML_READ_GROUND_OVERLAY", "YES"));

    if( poKmlRoot )
    {
        /***** skip over the <kml> we want the container *****/
        if( poKmlRoot->IsA( kmldom::Type_kml ) )
        {
            KmlPtr poKmlKml = AsKml( poKmlRoot );

            if( poKmlKml->has_feature() )
            {
                FeaturePtr poKmlFeat = poKmlKml->get_feature();

                if( poKmlFeat->IsA( kmldom::Type_Container ) )
                    poKmlContainer = AsContainer( poKmlFeat );
                else if( poKmlFeat->IsA( kmldom::Type_Placemark ) ||
                         (bReadGroundOverlay &&
                          poKmlFeat->IsA( kmldom::Type_GroundOverlay )) )
                {
                    poKmlContainer = m_poKmlFactory->CreateDocument();
                    poKmlContainer->add_feature(
                        kmldom::AsFeature(kmlengine::Clone(poKmlFeat)) );
                }
            }
        }
        else if( poKmlRoot->IsA( kmldom::Type_Container ) )
        {
            poKmlContainer = AsContainer( poKmlRoot );
        }
    }

    return poKmlContainer;
}

/******************************************************************************
 Method to parse a kml string into the style table.
******************************************************************************/

int OGRLIBKMLDataSource::ParseIntoStyleTable(
    std::string *poKmlStyleKml,
    const char *pszMyStylePath )
{
    /***** parse the kml into the dom *****/
    std::string oKmlErrors;
    ElementPtr poKmlRoot = OGRLIBKMLParse( *poKmlStyleKml, &oKmlErrors );

    if( !poKmlRoot )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "ERROR parsing style kml %s :%s",
                  pszStylePath, oKmlErrors.c_str() );
        return false;
    }

    ContainerPtr poKmlContainer = nullptr;

    if( !( poKmlContainer = GetContainerFromRoot( m_poKmlFactory, poKmlRoot ) ) )
    {
        return false;
    }

    ParseStyles( AsDocument( poKmlContainer ), &m_poStyleTable );
    pszStylePath = CPLStrdup(pszMyStylePath);

    return true;
}

/******************************************************************************
 Method to open a kml file.

 Args:          pszFilename file to open
                bUpdate     update mode

 Returns:       True on success, false on failure

******************************************************************************/

int OGRLIBKMLDataSource::OpenKml( const char *pszFilename, int bUpdateIn )
{
    std::string oKmlKml;
    std::string osBuffer;
    osBuffer.resize(4096);

    VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
    if( fp == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Cannot open %s", pszFilename );
        return FALSE;
    }
    int nRead = 0;
    while( (nRead = static_cast<int>(VSIFReadL(&osBuffer[0], 1,
                                               osBuffer.size(), fp))) != 0 )
    {
        try
        {
            oKmlKml.append(osBuffer.c_str(), nRead);
        }
        catch(const std::exception& ex)
        {
            CPLDebug("LIBKML",
                 "libstdc++ exception during ingestion: %s", ex.what());
            VSIFCloseL(fp);
            return FALSE;
        }
    }
    OGRLIBKMLPreProcessInput(oKmlKml);
    VSIFCloseL(fp);

    CPLLocaleC  oLocaleForcer;

    /***** create a SRS *****/
    OGRSpatialReference *poOgrSRS =
        new OGRSpatialReference( SRS_WKT_WGS84_LAT_LONG );
    poOgrSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    /***** parse the kml into the DOM *****/
    std::string oKmlErrors;

    m_poKmlDSKml = AsKml(OGRLIBKMLParse( oKmlKml, &oKmlErrors ));

    if( !m_poKmlDSKml )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "ERROR parsing kml %s :%s",
                  pszFilename, oKmlErrors.c_str() );
        delete poOgrSRS;

        return FALSE;
    }

    /***** get the container from root  *****/
    if( !( m_poKmlDSContainer = GetContainerFromRoot( m_poKmlFactory,
                                                      m_poKmlDSKml ) ) )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "ERROR parsing kml %s :%s %s",
                  pszFilename, "This file does not fit the OGR model,",
                  "there is no container element at the root." );
        delete poOgrSRS;

        return FALSE;
    }

    m_isKml = true;

    /***** get the styles *****/
    ParseStyles( AsDocument( m_poKmlDSContainer ), &m_poStyleTable );

    /***** parse for layers *****/
    int nPlacemarks = ParseLayers( m_poKmlDSContainer, poOgrSRS, false );

    /***** if there is placemarks in the root its a layer *****/
    if( nPlacemarks )
    {
      std::string layername_default( CPLGetBasename( pszFilename ) );

      if( m_poKmlDSContainer->has_name() )
      {
          layername_default = m_poKmlDSContainer->get_name();
      }

      AddLayer( layername_default.c_str(),
                wkbUnknown,
                this, m_poKmlDSKml, m_poKmlDSContainer, pszFilename, FALSE,
                bUpdateIn, 1 );
    }

    ParseLayers( m_poKmlDSContainer, poOgrSRS, true );

    delete poOgrSRS;

    return TRUE;
}

/******************************************************************************
 Method to open a kmz file.

 Args:          pszFilename file to open
                bUpdate     update mode

 Returns:       True on success, false on failure

******************************************************************************/

int OGRLIBKMLDataSource::OpenKmz( const char *pszFilename, int bUpdateIn )
{
    std::string oKmlKmz;
    char szBuffer[1024+1] = {};

    VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
    if( fp == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Cannot open %s", pszFilename );
        return FALSE;
    }
    int nRead = 0;
    while( (nRead = static_cast<int>(VSIFReadL(szBuffer, 1, 1024, fp))) != 0 )
    {
        try
        {
            oKmlKmz.append(szBuffer, nRead);
        }
        catch( const std::bad_alloc& )
        {
            VSIFCloseL(fp);
            return FALSE;
        }
    }
    VSIFCloseL(fp);

    KmzFile *poKmlKmzfile = KmzFile::OpenFromString( oKmlKmz );

    if( !poKmlKmzfile )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "%s is not a valid kmz file", pszFilename );
        return FALSE;
    }

    CPLLocaleC  oLocaleForcer;

    /***** read the doc.kml *****/
    std::string oKmlKml;
    std::string oKmlKmlPath;
    if( !poKmlKmzfile->ReadKmlAndGetPath( &oKmlKml, &oKmlKmlPath ) )
    {
        return FALSE;
    }

    /***** create a SRS *****/
    OGRSpatialReference *poOgrSRS =
        new OGRSpatialReference( SRS_WKT_WGS84_LAT_LONG );
    poOgrSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    /***** parse the kml into the DOM *****/
    std::string oKmlErrors;
    ElementPtr poKmlDocKmlRoot = OGRLIBKMLParse( oKmlKml, &oKmlErrors );

    if( !poKmlDocKmlRoot )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "ERROR parsing kml layer %s from %s :%s",
                  oKmlKmlPath.c_str(),
                  pszFilename, oKmlErrors.c_str() );
        delete poOgrSRS;

        return FALSE;
    }

    /***** Get the child container from root. *****/
    ContainerPtr poKmlContainer = nullptr;

    if( !(poKmlContainer = GetContainerFromRoot( m_poKmlFactory,
                                                 poKmlDocKmlRoot )) )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "ERROR parsing %s from %s :%s",
                  oKmlKmlPath.c_str(),
                  pszFilename, "kml contains no Containers" );
        delete poOgrSRS;

        return FALSE;
    }

    /***** loop over the container looking for network links *****/

    size_t nKmlFeatures = poKmlContainer->get_feature_array_size();
    int nLinks = 0;

    for( size_t iKmlFeature = 0; iKmlFeature < nKmlFeatures; iKmlFeature++ )
    {
        FeaturePtr poKmlFeat =
            poKmlContainer->get_feature_array_at( iKmlFeature );

        /***** is it a network link? *****/
        if( !poKmlFeat->IsA( kmldom::Type_NetworkLink ) )
            continue;

        NetworkLinkPtr poKmlNetworkLink = AsNetworkLink( poKmlFeat );

        /***** does it have a link? *****/
        if( !poKmlNetworkLink->has_link() )
            continue;

        LinkPtr poKmlLink = poKmlNetworkLink->get_link();

        /***** does the link have a href? *****/
        if( !poKmlLink->has_href() )
            continue;

        kmlengine::Href * poKmlHref =
            new kmlengine::Href( poKmlLink->get_href() );

        /***** is the link relative? *****/
        if( poKmlHref->IsRelativePath() )
        {
            nLinks++;

            std::string oKml;
            if( poKmlKmzfile->
                 ReadFile( poKmlHref->get_path().c_str(), &oKml ) )
            {
                /***** parse the kml into the DOM *****/
                oKmlErrors.clear();
                ElementPtr poKmlLyrRoot = OGRLIBKMLParse( oKml, &oKmlErrors );

                if( !poKmlLyrRoot )
                {
                    CPLError( CE_Failure, CPLE_OpenFailed,
                              "ERROR parsing kml layer %s from %s :%s",
                              poKmlHref->get_path().c_str(),
                              pszFilename, oKmlErrors.c_str() );
                    delete poKmlHref;

                    continue;
                }

                /***** get the container from root  *****/
                ContainerPtr poKmlLyrContainer =
                    GetContainerFromRoot( m_poKmlFactory, poKmlLyrRoot );

                if( !poKmlLyrContainer )
                {
                    CPLError( CE_Failure, CPLE_OpenFailed,
                               "ERROR parsing kml layer %s from %s :%s",
                               poKmlHref->get_path().c_str(),
                               pszFilename, oKmlErrors.c_str() );
                    delete poKmlHref;

                    continue;
                }

                /***** create the layer *****/
                AddLayer( CPLGetBasename
                          ( poKmlHref->get_path().c_str() ),
                           wkbUnknown, this, poKmlLyrRoot, poKmlLyrContainer,
                           poKmlHref->get_path().c_str(), FALSE, bUpdateIn,
                           static_cast<int>(nKmlFeatures) );

                /***** check if any features are another layer *****/
                ParseLayers( poKmlLyrContainer, poOgrSRS, true );
           }
        }

        /***** cleanup *****/
        delete poKmlHref;
    }

    /***** if the doc.kml has links store it so if were in update mode we can write it *****/
    if( nLinks )
    {
        m_poKmlDocKml = poKmlContainer;
        m_poKmlDocKmlRoot = poKmlDocKmlRoot;
    }
    /***** if the doc.kml has no links treat it as a normal kml file *****/
    else
    {
        /* TODO: There could still be a separate styles file in the KMZ
           if there is this would be a layer style table IF its only a single
           layer.
         */

        /***** get the styles *****/
        ParseStyles( AsDocument( poKmlContainer ), &m_poStyleTable );

        /***** parse for layers *****/
       const int nPlacemarks = ParseLayers( poKmlContainer, poOgrSRS, false );

        /***** if there is placemarks in the root its a layer *****/
        if( nPlacemarks )
        {
            std::string layername_default( CPLGetBasename( pszFilename ) );

            if( poKmlContainer->has_name() )
            {
                layername_default = poKmlContainer->get_name();
            }

            AddLayer( layername_default.c_str(),
                      wkbUnknown,
                      this, poKmlDocKmlRoot, poKmlContainer,
                      pszFilename, FALSE, bUpdateIn, 1 );
        }

        ParseLayers( poKmlContainer, poOgrSRS, true );
    }

    /***** read the style table if it has one *****/
    std::string oKmlStyleKml;
    if( poKmlKmzfile->ReadFile( "style/style.kml", &oKmlStyleKml ) )
        ParseIntoStyleTable( &oKmlStyleKml, "style/style.kml");

    /***** cleanup *****/
    delete poOgrSRS;

    delete poKmlKmzfile;
    m_isKmz = true;

    return TRUE;
}

/******************************************************************************
 Method to open a dir.

 Args:          pszFilename Dir to open
                bUpdate     update mode

 Returns:       True on success, false on failure

******************************************************************************/

int OGRLIBKMLDataSource::OpenDir( const char *pszFilename, int bUpdateIn )
{
    char **papszDirList = VSIReadDir( pszFilename );

    if( papszDirList == nullptr )
        return FALSE;

    /***** create a SRS *****/
    OGRSpatialReference *poOgrSRS =
        new OGRSpatialReference( SRS_WKT_WGS84_LAT_LONG );
    poOgrSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    const int nFiles = CSLCount( papszDirList );

    for( int iFile = 0; iFile < nFiles; iFile++ )
    {
        /***** make sure its a .kml file *****/
        if( !EQUAL( CPLGetExtension( papszDirList[iFile] ), "kml" ) )
            continue;

        /***** read the file *****/
        std::string oKmlKml;
        char szBuffer[1024+1] = {};

        CPLString osFilePath =
            CPLFormFilename( pszFilename, papszDirList[iFile], nullptr );

        VSILFILE* fp = VSIFOpenL(osFilePath, "rb");
        if( fp == nullptr )
        {
             CPLError( CE_Failure, CPLE_OpenFailed,
                       "Cannot open %s", osFilePath.c_str() );
             continue;
        }

        int nRead = 0;
        while( (nRead = static_cast<int>(VSIFReadL(szBuffer, 1,
                                                   1024, fp))) != 0 )
        {
            try
            {
                oKmlKml.append(szBuffer, nRead);
            }
            catch( const std::bad_alloc& )
            {
                VSIFCloseL(fp);
                CSLDestroy( papszDirList );
                return FALSE;
            }
        }
        VSIFCloseL(fp);

        CPLLocaleC oLocaleForcer;

        /***** parse the kml into the DOM *****/
        std::string oKmlErrors;
        ElementPtr poKmlRoot = OGRLIBKMLParse( oKmlKml, &oKmlErrors );

        if( !poKmlRoot )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "ERROR parsing kml layer %s from %s :%s",
                      osFilePath.c_str(), pszFilename, oKmlErrors.c_str() );

            continue;
        }

        /***** Get the container from the root *****/
        ContainerPtr poKmlContainer = nullptr;

        if( !( poKmlContainer = GetContainerFromRoot( m_poKmlFactory,
                                                      poKmlRoot ) ) )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "ERROR parsing kml %s :%s %s",
                      pszFilename,
                      "This file does not fit the OGR model,",
                      "there is no container element at the root." );
            continue;
        }

        /***** is it a style table? *****/
        if( EQUAL( papszDirList[iFile], "style.kml" ) )
        {
            ParseStyles( AsDocument( poKmlContainer ), &m_poStyleTable );
            pszStylePath = CPLStrdup(const_cast<char *>("style.kml"));
            continue;
        }

        /***** create the layer *****/
        AddLayer( CPLGetBasename( osFilePath.c_str() ),
                  wkbUnknown,
                  this, poKmlRoot, poKmlContainer, osFilePath.c_str(), FALSE,
                  bUpdateIn, nFiles );

        /***** check if any features are another layer *****/
        ParseLayers( poKmlContainer, poOgrSRS, true );
    }

    delete poOgrSRS;

    CSLDestroy( papszDirList );

    if( nLayers > 0 )
    {
        m_isDir = true;
        return TRUE;
    }

    return FALSE;
}

/******************************************************************************
 Method to open a datasource.

 Args:          pszFilename Darasource to open
                bUpdate     update mode

 Returns:       True on success, false on failure

******************************************************************************/

static bool CheckIsKMZ( const char *pszFilename )
{
    char** papszFiles = VSIReadDir(pszFilename);
    char** papszIter = papszFiles;
    bool bFoundKML = false;
    while( papszIter && *papszIter )
    {
        if( EQUAL(CPLGetExtension(*papszIter), "kml") )
        {
            bFoundKML = true;
            break;
        }
        else
        {
            CPLString osFilename(pszFilename);
            osFilename += "/";
            osFilename += *papszIter;
            if( CheckIsKMZ(osFilename) )
            {
                bFoundKML = true;
                break;
            }
        }
        papszIter++;
    }
    CSLDestroy(papszFiles);
    return bFoundKML;
}

int OGRLIBKMLDataSource::Open( const char *pszFilename, int bUpdateIn )
{
    bUpdate = CPL_TO_BOOL(bUpdateIn);
    m_pszName = CPLStrdup( pszFilename );

    /***** dir *****/
    VSIStatBufL sStatBuf;
    if( !VSIStatExL( pszFilename, &sStatBuf, VSI_STAT_NATURE_FLAG ) &&
         VSI_ISDIR( sStatBuf.st_mode ) )
    {
        return OpenDir( pszFilename, bUpdate );
    }

    /***** kml *****/
    if( EQUAL( CPLGetExtension( pszFilename ), "kml" ) )
    {
        return OpenKml( pszFilename, bUpdate );
    }

    /***** kmz *****/
    if( EQUAL( CPLGetExtension( pszFilename ), "kmz" ) )
    {
        return OpenKmz( pszFilename, bUpdate );
    }

    VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
    if( fp == nullptr )
        return FALSE;

    char szBuffer[1024+1] = {};
    const int nRead = static_cast<int>(VSIFReadL(szBuffer, 1, 1024, fp));
    szBuffer[nRead] = 0;

    VSIFCloseL(fp);

    // Does it look like a zip file?
    if( nRead == 1024 &&
        szBuffer[0] == 0x50 && szBuffer[1] == 0x4B &&
        szBuffer[2] == 0x03 && szBuffer[3] == 0x04 )
    {
        CPLString osFilename("/vsizip/");
        osFilename += pszFilename;
        if( !CheckIsKMZ(osFilename) )
            return FALSE;

        return OpenKmz( pszFilename, bUpdate );
    }

    if( strstr(szBuffer, "<kml>") || strstr(szBuffer, "<kml xmlns=") )
        return OpenKml( pszFilename, bUpdate );

    return FALSE;
}

/************************************************************************/
/*                         IsValidPhoneNumber()                         */
/************************************************************************/

// Very approximative validation of http://tools.ietf.org/html/rfc3966#page-6
static bool IsValidPhoneNumber( const char* pszPhoneNumber )
{
    if( STARTS_WITH(pszPhoneNumber, "tel:") )
        pszPhoneNumber += strlen("tel:");
    char ch = '\0';
    bool bDigitFound = false;
    if( *pszPhoneNumber == '+' )
        pszPhoneNumber ++;
    while( (ch = *pszPhoneNumber) != '\0' )
    {
        if( ch >= '0' && ch <= '9' )
            bDigitFound = true;
        else if( ch == ';' )
            break;
        else if( !(ch == '-' || ch == '.' || ch == '(' || ch == ')') )
            return false;
        pszPhoneNumber ++;
    }
    return bDigitFound;
}

/************************************************************************/
/*                           SetCommonOptions()                         */
/************************************************************************/

void OGRLIBKMLDataSource::SetCommonOptions( ContainerPtr poKmlContainer,
                                            char** papszOptions )
{
    const char* l_pszName = CSLFetchNameValue(papszOptions, "NAME");
    if( l_pszName != nullptr )
        poKmlContainer->set_name(l_pszName);

    const char* pszVisibilility = CSLFetchNameValue(papszOptions, "VISIBILITY");
    if( pszVisibilility != nullptr )
        poKmlContainer->set_visibility(CPLTestBool(pszVisibilility));

    const char* pszOpen = CSLFetchNameValue(papszOptions, "OPEN");
    if( pszOpen != nullptr )
        poKmlContainer->set_open(CPLTestBool(pszOpen));

    const char* pszSnippet = CSLFetchNameValue(papszOptions, "SNIPPET");
    if( pszSnippet != nullptr )
    {
        SnippetPtr poKmlSnippet = m_poKmlFactory->CreateSnippet();
        poKmlSnippet->set_text(pszSnippet);
        poKmlContainer->set_snippet(poKmlSnippet);
    }

    const char* pszDescription = CSLFetchNameValue(papszOptions, "DESCRIPTION");
    if( pszDescription != nullptr )
        poKmlContainer->set_description(pszDescription);
}

/************************************************************************/
/*                        ParseDocumentOptions()                        */
/************************************************************************/

void OGRLIBKMLDataSource::ParseDocumentOptions( KmlPtr poKml,
                                                DocumentPtr poKmlDocument )
{
    if( poKmlDocument )
    {
        const char* pszDocumentId =
            CSLFetchNameValueDef(m_papszOptions, "DOCUMENT_ID", "root_doc");
        poKmlDocument->set_id(pszDocumentId);

        const char* pszAuthorName =
            CSLFetchNameValue(m_papszOptions, "AUTHOR_NAME");
        const char* pszAuthorURI =
            CSLFetchNameValue(m_papszOptions, "AUTHOR_URI");
        const char* pszAuthorEmail =
            CSLFetchNameValue(m_papszOptions, "AUTHOR_EMAIL");
        const char* pszLink =
            CSLFetchNameValue(m_papszOptions, "LINK");

        if( pszAuthorName != nullptr || pszAuthorURI != nullptr ||
            pszAuthorEmail != nullptr )
        {
            kmldom::AtomAuthorPtr author = m_poKmlFactory->CreateAtomAuthor();
            if( pszAuthorName != nullptr )
                author->set_name(pszAuthorName);
            if( pszAuthorURI != nullptr )
            {
                // Ad-hoc validation. The ABNF is horribly complicated:
                // http://tools.ietf.org/search/rfc3987#page-7
                if( STARTS_WITH(pszAuthorURI, "http://") ||
                    STARTS_WITH(pszAuthorURI, "https://") )
                {
                    author->set_uri(pszAuthorURI);
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Invalid IRI for AUTHOR_URI");
                }
            }
            if( pszAuthorEmail != nullptr )
            {
                const char* pszArobase = strchr(pszAuthorEmail, '@');
                if( pszArobase != nullptr && strchr(pszArobase + 1, '.') != nullptr )
                {
                    author->set_email(pszAuthorEmail);
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Invalid email for AUTHOR_EMAIL");
                }
            }
            poKmlDocument->set_atomauthor(author);
        }

        if( pszLink != nullptr )
        {
            kmldom::AtomLinkPtr link = m_poKmlFactory->CreateAtomLink();
            link->set_href(pszLink);
            link->set_rel("related");
            poKmlDocument->set_atomlink(link);
        }

        const char* pszPhoneNumber =
            CSLFetchNameValue(m_papszOptions, "PHONENUMBER");
        if( pszPhoneNumber != nullptr )
        {
            if( IsValidPhoneNumber(pszPhoneNumber) )
            {
                if( !STARTS_WITH(pszPhoneNumber, "tel:") )
                    poKmlDocument->set_phonenumber(
                        CPLSPrintf("tel:%s", pszPhoneNumber));
                else
                    poKmlDocument->set_phonenumber(pszPhoneNumber);
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined, "Invalid phone number");
            }
        }

        SetCommonOptions(poKmlDocument, m_papszOptions);

        CPLString osListStyleType =
            CSLFetchNameValueDef(m_papszOptions, "LISTSTYLE_TYPE", "");
        CPLString osListStyleIconHref =
            CSLFetchNameValueDef(m_papszOptions, "LISTSTYLE_ICON_HREF", "");
        createkmlliststyle( m_poKmlFactory,
                            pszDocumentId,
                            poKmlDocument,
                            poKmlDocument,
                            osListStyleType,
                            osListStyleIconHref );
    }

    if( poKml )
    {
        if( m_poKmlUpdate )
        {
            NetworkLinkControlPtr nlc = m_poKmlFactory->CreateNetworkLinkControl();
            poKml->set_networklinkcontrol( nlc );
            if( m_poKmlUpdate->get_updateoperation_array_size() != 0 )
            {
                nlc->set_update(m_poKmlUpdate);
            }
        }

        const char* pszNLCMinRefreshPeriod =
            CSLFetchNameValue(m_papszOptions, "NLC_MINREFRESHPERIOD");
        const char* pszNLCMaxSessionLength =
            CSLFetchNameValue(m_papszOptions, "NLC_MAXSESSIONLENGTH");
        const char* pszNLCCookie =
            CSLFetchNameValue(m_papszOptions, "NLC_COOKIE");
        const char* pszNLCMessage =
            CSLFetchNameValue(m_papszOptions, "NLC_MESSAGE");
        const char* pszNLCLinkName =
            CSLFetchNameValue(m_papszOptions, "NLC_LINKNAME");
        const char* pszNLCLinkDescription =
            CSLFetchNameValue(m_papszOptions, "NLC_LINKDESCRIPTION");
        const char* pszNLCLinkSnippet =
            CSLFetchNameValue(m_papszOptions, "NLC_LINKSNIPPET");
        const char* pszNLCExpires =
            CSLFetchNameValue(m_papszOptions, "NLC_EXPIRES");

        if( pszNLCMinRefreshPeriod != nullptr ||
            pszNLCMaxSessionLength != nullptr ||
            pszNLCCookie != nullptr ||
            pszNLCMessage != nullptr ||
            pszNLCLinkName != nullptr ||
            pszNLCLinkDescription != nullptr ||
            pszNLCLinkSnippet != nullptr ||
            pszNLCExpires != nullptr )
        {
            NetworkLinkControlPtr nlc = nullptr;
            if( poKml->has_networklinkcontrol() )
            {
                nlc = poKml->get_networklinkcontrol();
            }
            else
            {
                nlc = m_poKmlFactory->CreateNetworkLinkControl();
                poKml->set_networklinkcontrol( nlc );
            }
            if( pszNLCMinRefreshPeriod != nullptr )
            {
                const double dfVal = CPLAtof(pszNLCMinRefreshPeriod);
                if( dfVal >= 0 )
                    nlc->set_minrefreshperiod(dfVal);
            }
            if( pszNLCMaxSessionLength != nullptr )
            {
                const double dfVal = CPLAtof(pszNLCMaxSessionLength);
                nlc->set_maxsessionlength(dfVal);
            }
            if( pszNLCCookie != nullptr )
            {
                nlc->set_cookie(pszNLCCookie);
            }
            if( pszNLCMessage != nullptr )
            {
                nlc->set_message(pszNLCMessage);
            }
            if( pszNLCLinkName != nullptr )
            {
                nlc->set_linkname(pszNLCLinkName);
            }
            if( pszNLCLinkDescription != nullptr )
            {
                nlc->set_linkdescription(pszNLCLinkDescription);
            }
            if( pszNLCLinkSnippet != nullptr )
            {
                LinkSnippetPtr linksnippet =
                    m_poKmlFactory->CreateLinkSnippet();
                linksnippet->set_text(pszNLCLinkSnippet);
                nlc->set_linksnippet(linksnippet);
            }
            if( pszNLCExpires != nullptr )
            {
                OGRField sField;
                if( OGRParseXMLDateTime( pszNLCExpires, &sField) )
                {
                    char* pszXMLDate = OGRGetXMLDateTime(&sField);
                    nlc->set_expires(pszXMLDate);
                    CPLFree(pszXMLDate);
                }
            }
        }
    }
}

/******************************************************************************
 Method to create a single file .kml ds.

 Args:          pszFilename     the datasource to create
                papszOptions    datasource creation options

 Returns:       True on success, false on failure

******************************************************************************/

int OGRLIBKMLDataSource::CreateKml(
    const char * /* pszFilename */,
    char **papszOptions )
{
    m_poKmlDSKml = OGRLIBKMLCreateOGCKml22(m_poKmlFactory, papszOptions);
    if( osUpdateTargetHref.empty() )
    {
        DocumentPtr poKmlDocument = m_poKmlFactory->CreateDocument();
        m_poKmlDSKml->set_feature( poKmlDocument );
        m_poKmlDSContainer = poKmlDocument;
    }

    m_isKml = true;
    bUpdated = true;

    return true;
}

/******************************************************************************
 Method to create a .kmz ds.

 Args:          pszFilename     the datasource to create
                papszOptions    datasource creation options

 Returns:       True on success, false on failure

******************************************************************************/

int OGRLIBKMLDataSource::CreateKmz(
    const char * /* pszFilename */,
    char ** /* papszOptions */ )
{
    /***** create the doc.kml  *****/
    if( osUpdateTargetHref.empty() )
    {
        const char *pszUseDocKml =
            CPLGetConfigOption( "LIBKML_USE_DOC.KML", "yes" );

        if( CPLTestBool( pszUseDocKml ) )
        {
            m_poKmlDocKml = m_poKmlFactory->CreateDocument();
        }
    }

    pszStylePath = CPLStrdup(const_cast<char *>("style/style.kml"));

    m_isKmz = true;
    bUpdated = true;

    return TRUE;
}

/******************************************************************************
 Method to create a dir datasource.

 Args:          pszFilename     the datasource to create
                papszOptions    datasource creation options

 Returns:       True on success, false on failure

******************************************************************************/

int OGRLIBKMLDataSource::CreateDir(
    const char *pszFilename,
    char ** /* papszOptions */ )
{
    if( VSIMkdir( pszFilename, 0755 ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "ERROR Creating dir: %s for KML datasource", pszFilename );
        return FALSE;
    }

    m_isDir = true;
    bUpdated = true;

    if( osUpdateTargetHref.empty() )
    {
        const char *pszUseDocKml =
            CPLGetConfigOption( "LIBKML_USE_DOC.KML", "yes" );

        if( CPLTestBool( pszUseDocKml ) )
        {
            m_poKmlDocKml = m_poKmlFactory->CreateDocument();
        }
    }

    pszStylePath = CPLStrdup(const_cast<char *>("style.kml"));

    return TRUE;
}

/******************************************************************************
 Method to create a datasource.

 Args:          pszFilename     the datasource to create
                papszOptions    datasource creation options

 Returns:       True on success, false on failure

 env vars:
  LIBKML_USE_DOC.KML         default: yes

******************************************************************************/

int OGRLIBKMLDataSource::Create(
    const char *pszFilename,
    char **papszOptions )
{
    if( strcmp(pszFilename, "/dev/stdout") == 0 )
        pszFilename = "/vsistdout/";

    m_pszName = CPLStrdup( pszFilename );
    bUpdate = true;

    osUpdateTargetHref =
        CSLFetchNameValueDef(papszOptions, "UPDATE_TARGETHREF", "");
    if( !osUpdateTargetHref.empty() )
    {
        m_poKmlUpdate = m_poKmlFactory->CreateUpdate();
        m_poKmlUpdate->set_targethref(osUpdateTargetHref.c_str());
    }

    m_papszOptions = CSLDuplicate(papszOptions);

    /***** kml *****/
    if( strcmp(pszFilename, "/vsistdout/") == 0 ||
        STARTS_WITH(pszFilename, "/vsigzip/") ||
        EQUAL( CPLGetExtension( pszFilename ), "kml" ) )
        return CreateKml( pszFilename, papszOptions );

    /***** kmz *****/
    if( EQUAL( CPLGetExtension( pszFilename ), "kmz" ) )
        return CreateKmz( pszFilename, papszOptions );

    /***** dir *****/
    return CreateDir( pszFilename, papszOptions );
}

/******************************************************************************
 Method to get a layer by index.

 Args:          iLayer      the index of the layer to get

 Returns:       pointer to the layer, or NULL if the layer does not exist

******************************************************************************/

OGRLayer *OGRLIBKMLDataSource::GetLayer( int iLayer )
{
    if( iLayer < 0 || iLayer >= nLayers )
        return nullptr;

    return papoLayers[iLayer];
}

/******************************************************************************
 Method to get a layer by name.

 Args:          pszname     name of the layer to get

 Returns:       pointer to the layer, or NULL if the layer does not exist

******************************************************************************/

OGRLayer *OGRLIBKMLDataSource::GetLayerByName( const char *pszName )
{
    auto oIter = m_oMapLayers.find( CPLString(pszName).toupper() );
    if( oIter != m_oMapLayers.end() )
        return oIter->second;

    return nullptr;
}

/******************************************************************************
 Method to DeleteLayers in a .kml datasource.

 Args:          iLayer  index of the layer to delete

 Returns:       OGRERR_NONE on success, OGRERR_FAILURE on failure

******************************************************************************/

OGRErr OGRLIBKMLDataSource::DeleteLayerKml( int iLayer )
{
    OGRLIBKMLLayer *poOgrLayer = ( OGRLIBKMLLayer * ) papoLayers[iLayer];

    /***** loop over the features *****/

    const size_t nKmlFeatures = m_poKmlDSContainer->get_feature_array_size();

    for( size_t iKmlFeature = 0; iKmlFeature < nKmlFeatures; iKmlFeature++ )
    {
        FeaturePtr poKmlFeat =
            m_poKmlDSContainer->get_feature_array_at( iKmlFeature );

        if( poKmlFeat == poOgrLayer->GetKmlLayer() )
        {
            m_poKmlDSContainer->DeleteFeatureAt( iKmlFeature );
            break;
        }
    }

    return OGRERR_NONE;
}

/******************************************************************************
 Method to DeleteLayers in a .kmz datasource.

 Args:          iLayer  index of the layer to delete

 Returns:       OGRERR_NONE on success, OGRERR_FAILURE on failure

******************************************************************************/

OGRErr OGRLIBKMLDataSource::DeleteLayerKmz( int iLayer )
{
    OGRLIBKMLLayer *poOgrLayer = papoLayers[iLayer];

    const char *pszUseDocKml =
        CPLGetConfigOption( "LIBKML_USE_DOC.KML", "yes" );

    if( CPLTestBool( pszUseDocKml ) && m_poKmlDocKml )
    {
        /***** loop over the features *****/
        const size_t nKmlFeatures = m_poKmlDocKml->get_feature_array_size();

        for( size_t iKmlFeature = 0; iKmlFeature < nKmlFeatures; iKmlFeature++ )
        {
            FeaturePtr poKmlFeat =
                m_poKmlDocKml->get_feature_array_at( iKmlFeature );

            if( poKmlFeat->IsA( kmldom::Type_NetworkLink ) )
            {
                NetworkLinkPtr poKmlNetworkLink = AsNetworkLink( poKmlFeat );

                /***** does it have a link? *****/
                if( poKmlNetworkLink->has_link() )
                {
                    LinkPtr poKmlLink = poKmlNetworkLink->get_link();

                    /***** does the link have a href? *****/
                    if( poKmlLink->has_href() )
                    {
                        kmlengine::Href * poKmlHref =
                            new kmlengine::Href( poKmlLink->get_href() );

                        /***** is the link relative? *****/
                        if( poKmlHref->IsRelativePath() )
                        {
                            const char *pszLink = poKmlHref->get_path().c_str();

                            if( EQUAL( pszLink, poOgrLayer->GetFileName() ) )
                            {
                                m_poKmlDocKml->DeleteFeatureAt( iKmlFeature );
                                delete poKmlHref;
                                break;
                            }
                        }

                        delete poKmlHref;
                    }
                }
            }
        }
    }

    return OGRERR_NONE;
}

/******************************************************************************
 Method to delete a layer in a datasource.

 Args:          iLayer  index of the layer to delete

 Returns:       OGRERR_NONE on success, OGRERR_FAILURE on failure

******************************************************************************/

OGRErr OGRLIBKMLDataSource::DeleteLayer( int iLayer )
{
    if( !bUpdate )
        return OGRERR_UNSUPPORTED_OPERATION;

    if( iLayer >= nLayers )
        return OGRERR_FAILURE;

    if( IsKml() )
    {
        DeleteLayerKml( iLayer );
    }
    else if( IsKmz() )
    {
        DeleteLayerKmz( iLayer );
    }
    else if( IsDir() )
    {
        DeleteLayerKmz( iLayer );

        /***** delete the file the layer corresponds to *****/
        const char *pszFilePath =
            CPLFormFilename( m_pszName, papoLayers[iLayer]->GetFileName(),
                              nullptr );
        VSIStatBufL oStatBufL;
        if( !VSIStatL( pszFilePath, &oStatBufL ) )
        {
            if( VSIUnlink( pszFilePath ) )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "ERROR Deleting Layer %s from filesystem as %s",
                          papoLayers[iLayer]->GetName(), pszFilePath );
            }
        }
    }

    m_oMapLayers.erase( CPLString(papoLayers[iLayer]->GetName()).toupper() );
    delete papoLayers[iLayer];
    memmove( papoLayers + iLayer, papoLayers + iLayer + 1,
             sizeof( void * ) * ( nLayers - iLayer - 1 ) );
    nLayers--;
    bUpdated = true;

    return OGRERR_NONE;
}

/******************************************************************************
 Method to create a layer in a single file .kml.

 Args:          pszLayerName    name of the layer to create
                poOgrSRS        the SRS of the layer
                eGType          the layers geometry type
                papszOptions    layer creation options

 Returns:       return a pointer to the new layer or NULL on failure

******************************************************************************/

OGRLIBKMLLayer *OGRLIBKMLDataSource::CreateLayerKml(
    const char *pszLayerName,
    OGRSpatialReference *,
    OGRwkbGeometryType eGType,
    char **papszOptions )
{
    ContainerPtr poKmlLayerContainer = nullptr;

    if( m_poKmlDSContainer )
    {
        if( CPLFetchBool( papszOptions, "FOLDER", false ) )
            poKmlLayerContainer = m_poKmlFactory->CreateFolder();
        else
            poKmlLayerContainer = m_poKmlFactory->CreateDocument();
        poKmlLayerContainer->set_id(OGRLIBKMLGetSanitizedNCName(pszLayerName).c_str());

        m_poKmlDSContainer->add_feature( poKmlLayerContainer );
    }

    /***** create the layer *****/
    OGRLIBKMLLayer *poOgrLayer =
        AddLayer( pszLayerName, eGType, this,
                  nullptr, poKmlLayerContainer, "", TRUE, bUpdate, 1 );

    /***** add the layer name as a <Name> *****/
    if( poKmlLayerContainer )
        poKmlLayerContainer->set_name( pszLayerName );
    else if(  CPLFetchBool( papszOptions, "FOLDER", false ) )
    {
        poOgrLayer->SetUpdateIsFolder(TRUE);
    }

    return poOgrLayer;
}

/******************************************************************************
 Method to create a layer in a .kmz or dir.

 Args:          pszLayerName    name of the layer to create
                poOgrSRS        the SRS of the layer
                eGType          the layers geometry type
                papszOptions    layer creation options

 Returns:       return a pointer to the new layer or NULL on failure

******************************************************************************/

OGRLIBKMLLayer *OGRLIBKMLDataSource::CreateLayerKmz(
    const char *pszLayerName,
    OGRSpatialReference *,
    OGRwkbGeometryType eGType,
    char ** /* papszOptions */ )
{
    DocumentPtr poKmlDocument = nullptr;

    if( !m_poKmlUpdate )
    {
        /***** add a network link to doc.kml *****/
        const char *pszUseDocKml =
            CPLGetConfigOption( "LIBKML_USE_DOC.KML", "yes" );

        if( CPLTestBool( pszUseDocKml ) && m_poKmlDocKml )
        {
            poKmlDocument = AsDocument( m_poKmlDocKml );

            NetworkLinkPtr poKmlNetLink = m_poKmlFactory->CreateNetworkLink();
            LinkPtr poKmlLink = m_poKmlFactory->CreateLink();

            std::string oHref;
            if( IsKmz() )
                oHref.append( "layers/" );
            oHref.append( pszLayerName );
            oHref.append( ".kml" );
            poKmlLink->set_href( oHref );

            poKmlNetLink->set_link( poKmlLink );
            poKmlDocument->add_feature( poKmlNetLink );
        }

        /***** create the layer *****/

        poKmlDocument = m_poKmlFactory->CreateDocument();
        poKmlDocument->set_id(OGRLIBKMLGetSanitizedNCName(pszLayerName).c_str());
    }

    OGRLIBKMLLayer *poOgrLayer =
        AddLayer( pszLayerName, eGType, this,
                  nullptr, poKmlDocument,
                  CPLFormFilename( nullptr, pszLayerName, ".kml" ),
                  TRUE, bUpdate, 1 );

    /***** add the layer name as a <Name> *****/
    if( !m_poKmlUpdate )
    {
        poKmlDocument->set_name( pszLayerName );
    }

    return poOgrLayer;
}

/******************************************************************************
 ICreateLayer()

 Args:          pszLayerName    name of the layer to create
                poOgrSRS        the SRS of the layer
                eGType          the layers geometry type
                papszOptions    layer creation options

 Returns:       return a pointer to the new layer or NULL on failure

******************************************************************************/

OGRLayer *OGRLIBKMLDataSource::ICreateLayer(
    const char *pszLayerName,
    OGRSpatialReference * poOgrSRS,
    OGRwkbGeometryType eGType,
    char **papszOptions )
{
    if( !bUpdate )
        return nullptr;

    if( (IsKmz() || IsDir()) && EQUAL(pszLayerName, "doc") )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "'doc' is an invalid layer name in a KMZ file");
        return nullptr;
    }

    OGRLIBKMLLayer *poOgrLayer = nullptr;

    /***** kml DS *****/
    if( IsKml() )
    {
        poOgrLayer = CreateLayerKml( pszLayerName, poOgrSRS,
                                     eGType, papszOptions );
    }
    else if( IsKmz() || IsDir() )
    {
        poOgrLayer = CreateLayerKmz( pszLayerName, poOgrSRS,
                                     eGType, papszOptions );
    }

    const char* pszLookatLongitude =
        CSLFetchNameValue(papszOptions, "LOOKAT_LONGITUDE");
    const char* pszLookatLatitude =
        CSLFetchNameValue(papszOptions, "LOOKAT_LATITUDE");
    const char* pszLookatAltitude =
        CSLFetchNameValue(papszOptions, "LOOKAT_ALTITUDE");
    const char* pszLookatHeading =
        CSLFetchNameValue(papszOptions, "LOOKAT_HEADING");
    const char* pszLookatTilt = CSLFetchNameValue(papszOptions, "LOOKAT_TILT");
    const char* pszLookatRange =
        CSLFetchNameValue(papszOptions, "LOOKAT_RANGE");
    const char* pszLookatAltitudeMode =
        CSLFetchNameValue(papszOptions, "LOOKAT_ALTITUDEMODE");
    if( poOgrLayer != nullptr &&
        pszLookatLongitude != nullptr &&
        pszLookatLatitude != nullptr &&
        pszLookatRange != nullptr )
    {
        poOgrLayer->SetLookAt(pszLookatLongitude,
                              pszLookatLatitude,
                              pszLookatAltitude,
                              pszLookatHeading,
                              pszLookatTilt,
                              pszLookatRange,
                              pszLookatAltitudeMode);
    }
    else
    {
        const char* pszCameraLongitude =
            CSLFetchNameValue(papszOptions, "CAMERA_LONGITUDE");
        const char* pszCameraLatitude =
            CSLFetchNameValue(papszOptions, "CAMERA_LATITUDE");
        const char* pszCameraAltitude =
            CSLFetchNameValue(papszOptions, "CAMERA_ALTITUDE");
        const char* pszCameraHeading =
            CSLFetchNameValue(papszOptions, "CAMERA_HEADING");
        const char* pszCameraTilt =
            CSLFetchNameValue(papszOptions, "CAMERA_TILT");
        const char* pszCameraRoll =
            CSLFetchNameValue(papszOptions, "CAMERA_ROLL");
        const char* pszCameraAltitudeMode =
            CSLFetchNameValue(papszOptions, "CAMERA_ALTITUDEMODE");
        if( poOgrLayer != nullptr &&
            pszCameraLongitude != nullptr &&
            pszCameraLatitude != nullptr &&
            pszCameraAltitude != nullptr &&
            pszCameraAltitudeMode != nullptr )
        {
            poOgrLayer->SetCamera(pszCameraLongitude,
                                pszCameraLatitude,
                                pszCameraAltitude,
                                pszCameraHeading,
                                pszCameraTilt,
                                pszCameraRoll,
                                pszCameraAltitudeMode);
        }
    }

    const char* pszRegionAdd =
        CSLFetchNameValueDef(papszOptions, "ADD_REGION", "FALSE");
    const char* pszRegionXMin = CSLFetchNameValue(papszOptions, "REGION_XMIN");
    const char* pszRegionYMin = CSLFetchNameValue(papszOptions, "REGION_YMIN");
    const char* pszRegionXMax = CSLFetchNameValue(papszOptions, "REGION_XMAX");
    const char* pszRegionYMax = CSLFetchNameValue(papszOptions, "REGION_YMAX");
    const char* pszRegionMinLodPixels =
        CSLFetchNameValueDef(papszOptions, "REGION_MIN_LOD_PIXELS", "256");
    const char* pszRegionMaxLodPixels =
        CSLFetchNameValueDef(papszOptions, "REGION_MAX_LOD_PIXELS", "-1");
    const char* pszRegionMinFadeExtent =
        CSLFetchNameValueDef(papszOptions, "REGION_MIN_FADE_EXTENT", "0");
    const char* pszRegionMaxFadeExtent =
        CSLFetchNameValueDef(papszOptions, "REGION_MAX_FADE_EXTENT", "0");
    if( poOgrLayer != nullptr && CPLTestBool(pszRegionAdd) )
    {
        poOgrLayer->SetWriteRegion(CPLAtof(pszRegionMinLodPixels),
                                   CPLAtof(pszRegionMaxLodPixels),
                                   CPLAtof(pszRegionMinFadeExtent),
                                   CPLAtof(pszRegionMaxFadeExtent));
        if( pszRegionXMin != nullptr && pszRegionYMin != nullptr &&
            pszRegionXMax != nullptr && pszRegionYMax != nullptr )
        {
            const double xmin = CPLAtof(pszRegionXMin);
            const double ymin = CPLAtof(pszRegionYMin);
            const double xmax = CPLAtof(pszRegionXMax);
            const double ymax = CPLAtof(pszRegionYMax);
            if( xmin < xmax && ymin < ymax )
                poOgrLayer->SetRegionBounds(xmin, ymin, xmax, ymax);
        }
    }

    const char* pszSOHref = CSLFetchNameValue(papszOptions, "SO_HREF");
    const char* pszSOName = CSLFetchNameValue(papszOptions, "SO_NAME");
    const char* pszSODescription = CSLFetchNameValue(papszOptions, "SO_DESCRIPTION");
    const char* pszSOOverlayX = CSLFetchNameValue(papszOptions, "SO_OVERLAY_X");
    const char* pszSOOverlayY = CSLFetchNameValue(papszOptions, "SO_OVERLAY_Y");
    const char* pszSOOverlayXUnits = CSLFetchNameValue(papszOptions, "SO_OVERLAY_XUNITS");
    const char* pszSOOverlayYUnits = CSLFetchNameValue(papszOptions, "SO_OVERLAY_YUNITS");
    const char* pszSOScreenX = CSLFetchNameValue(papszOptions, "SO_SCREEN_X");
    const char* pszSOScreenY = CSLFetchNameValue(papszOptions, "SO_SCREEN_Y");
    const char* pszSOScreenXUnits = CSLFetchNameValue(papszOptions, "SO_SCREEN_XUNITS");
    const char* pszSOScreenYUnits = CSLFetchNameValue(papszOptions, "SO_SCREEN_YUNITS");
    const char* pszSOSizeX = CSLFetchNameValue(papszOptions, "SO_SIZE_X");
    const char* pszSOSizeY = CSLFetchNameValue(papszOptions, "SO_SIZE_Y");
    const char* pszSOSizeXUnits = CSLFetchNameValue(papszOptions, "SO_SIZE_XUNITS");
    const char* pszSOSizeYUnits = CSLFetchNameValue(papszOptions, "SO_SIZE_YUNITS");
    if( poOgrLayer != nullptr && pszSOHref != nullptr )
    {
        poOgrLayer->SetScreenOverlay(pszSOHref,
                                     pszSOName,
                                     pszSODescription,
                                     pszSOOverlayX,
                                     pszSOOverlayY,
                                     pszSOOverlayXUnits,
                                     pszSOOverlayYUnits,
                                     pszSOScreenX,
                                     pszSOScreenY,
                                     pszSOScreenXUnits,
                                     pszSOScreenYUnits,
                                     pszSOSizeX,
                                     pszSOSizeY,
                                     pszSOSizeXUnits,
                                     pszSOSizeYUnits);
    }

    const char* pszListStyleType = CSLFetchNameValue(papszOptions, "LISTSTYLE_TYPE");
    const char* pszListStyleIconHref = CSLFetchNameValue(papszOptions, "LISTSTYLE_ICON_HREF");
    if( poOgrLayer != nullptr )
    {
        poOgrLayer->SetListStyle(pszListStyleType, pszListStyleIconHref);
    }

    if( poOgrLayer != nullptr && poOgrLayer->GetKmlLayer() )
    {
        SetCommonOptions(poOgrLayer->GetKmlLayer(), papszOptions);
    }

    /***** mark the dataset as updated *****/
    if( poOgrLayer )
        bUpdated = true;

    return poOgrLayer;
}

/******************************************************************************
 Method to get a datasources style table.

 Args:          none

 Returns:       pointer to the datasources style table, or NULL if it does
                not have one

******************************************************************************/

OGRStyleTable *OGRLIBKMLDataSource::GetStyleTable()
{
    return m_poStyleTable;
}

/******************************************************************************
  Method to write a style table to a single file .kml ds.

 Args:          poStyleTable    pointer to the style table to add

 Returns:       nothing

******************************************************************************/

void OGRLIBKMLDataSource::SetStyleTable2Kml( OGRStyleTable * poStyleTable )
{
    if( !m_poKmlDSContainer )
        return;

    /***** delete all the styles *****/

    DocumentPtr poKmlDocument = AsDocument( m_poKmlDSContainer );
    int nKmlStyles = static_cast<int>(poKmlDocument->get_styleselector_array_size());

    for( int iKmlStyle = nKmlStyles - 1; iKmlStyle >= 0; iKmlStyle-- )
    {
        poKmlDocument->DeleteStyleSelectorAt( iKmlStyle );
    }

    /***** add the new style table to the document *****/

    styletable2kml( poStyleTable, m_poKmlFactory,
                     AsContainer( poKmlDocument ), m_papszOptions );
}

/******************************************************************************
 Method to write a style table to a kmz ds.

 Args:          poStyleTable    pointer to the style table to add

 Returns:       nothing

******************************************************************************/

void OGRLIBKMLDataSource::SetStyleTable2Kmz( OGRStyleTable * poStyleTable )
{
    if( m_poKmlStyleKml || poStyleTable != nullptr )
    {
        /***** replace the style document with a new one *****/

        m_poKmlStyleKml = m_poKmlFactory->CreateDocument();
        m_poKmlStyleKml->set_id( "styleId" );

        styletable2kml( poStyleTable, m_poKmlFactory, m_poKmlStyleKml );
    }
}

/******************************************************************************
 Method to write a style table to a datasource.

 Args:          poStyleTable    pointer to the style table to add

 Returns:       nothing

 Note: This method assumes ownership of the style table.

******************************************************************************/

void OGRLIBKMLDataSource::SetStyleTableDirectly( OGRStyleTable * poStyleTable )
{
    if( !bUpdate )
        return;

    if( m_poStyleTable )
        delete m_poStyleTable;

    m_poStyleTable = poStyleTable;

    /***** a kml datasource? *****/
    if( IsKml() )
        SetStyleTable2Kml( m_poStyleTable );

    else if( IsKmz() || IsDir() )
        SetStyleTable2Kmz( m_poStyleTable );

    bUpdated = true;
}

/******************************************************************************
 Method to write a style table to a datasource.

 Args:          poStyleTable    pointer to the style table to add

 Returns:       nothing

 Note:  This method copies the style table, and the user will still be
        responsible for its destruction.

******************************************************************************/

void OGRLIBKMLDataSource::SetStyleTable( OGRStyleTable * poStyleTable )
{
    if( !bUpdate )
        return;

    if( poStyleTable )
        SetStyleTableDirectly( poStyleTable->Clone() );
    else
        SetStyleTableDirectly( nullptr );
}

/******************************************************************************
 Test if capability is available.

 Args:          pszCap  datasource capability name to test

 Returns:       TRUE or FALSE

 ODsCCreateLayer: True if this datasource can create new layers.
 ODsCDeleteLayer: True if this datasource can delete existing layers.

******************************************************************************/

int OGRLIBKMLDataSource::TestCapability( const char *pszCap )
{
    if( EQUAL( pszCap, ODsCCreateLayer ) )
        return bUpdate;
    if( EQUAL( pszCap, ODsCDeleteLayer ) )
        return bUpdate;
    if( EQUAL(pszCap,ODsCRandomLayerWrite) )
        return bUpdate;

    return FALSE;
}
