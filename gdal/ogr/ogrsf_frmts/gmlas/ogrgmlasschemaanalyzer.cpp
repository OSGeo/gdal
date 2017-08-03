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
#include "xercesc_headers.h"

// Hack to avoid bool, possibly redefined to pedantic bool class, being later used
static XSModel* getGrammarPool(XMLGrammarPool* pool)
{
    bool changed;
    return pool->getXSModel(changed);
}

#include "ogr_gmlas.h"
#include "ogr_pgdump.h"

CPL_CVSID("$Id$")

static OGRwkbGeometryType GetOGRGeometryType( XSTypeDefinition* poTypeDef );

/************************************************************************/
/*                        IsCompatibleOfArray()                         */
/************************************************************************/

static bool IsCompatibleOfArray( GMLASFieldType eType )
{
    return eType == GMLAS_FT_STRING ||
           eType == GMLAS_FT_BOOLEAN ||
           eType == GMLAS_FT_SHORT ||
           eType == GMLAS_FT_INT32 ||
           eType == GMLAS_FT_INT64 ||
           eType == GMLAS_FT_FLOAT ||
           eType == GMLAS_FT_DOUBLE ||
           eType == GMLAS_FT_DECIMAL ||
           eType == GMLAS_FT_ANYURI;
}

/************************************************************************/
/*                       GMLASPrefixMappingHander                       */
/************************************************************************/

class GMLASPrefixMappingHander: public DefaultHandler
{
        std::map<CPLString, CPLString>& m_oMapURIToPrefix;
        const std::map<CPLString, CPLString>& m_oMapDocNSURIToPrefix;
        CPLString& m_osGMLVersionFound;

  public:
        GMLASPrefixMappingHander(
                        std::map<CPLString, CPLString>& oMapURIToPrefix,
                        const std::map<CPLString, CPLString>& oMapDocNSURIToPrefix,
                        CPLString& osGMLVersionFound) :
            m_oMapURIToPrefix( oMapURIToPrefix ),
            m_oMapDocNSURIToPrefix( oMapDocNSURIToPrefix ),
            m_osGMLVersionFound( osGMLVersionFound )
        {}

        virtual void startElement(
            const   XMLCh* const    uri,
            const   XMLCh* const    localname,
            const   XMLCh* const    qname,
            const   Attributes& attrs) override;

        virtual void startPrefixMapping(const XMLCh* const prefix,
                                        const XMLCh* const uri) override;
};

/************************************************************************/
/*                           startElement()                             */
/************************************************************************/

void GMLASPrefixMappingHander::startElement(
                                    const   XMLCh* const    uri,
                                    const   XMLCh* const    localname,
                                    const   XMLCh* const    /*qname*/,
                                    const   Attributes& attrs )
{
    if( !m_osGMLVersionFound.empty() )
        return;

    const CPLString osURI( transcode(uri) );
    const CPLString osLocalname( transcode(localname) );
    if( osURI == szXS_URI && osLocalname == "schema" )
    {
        bool bIsGML = false;
        CPLString osVersion;
        for(unsigned int i=0; i < attrs.getLength(); i++)
        {
            CPLString osAttrLocalName(transcode(attrs.getLocalName(i)));
            if( osAttrLocalName == "targetNamespace" )
            {
                bIsGML = transcode(attrs.getValue(i)) == szGML_URI;
            }
            else if( osAttrLocalName == "version" )
            {
                osVersion = transcode(attrs.getValue(i));
            }
        }
        if( bIsGML && !osVersion.empty() )
        {
            m_osGMLVersionFound = osVersion;
        }
    }
}

/************************************************************************/
/*                         startPrefixMapping()                         */
/************************************************************************/

void GMLASPrefixMappingHander::startPrefixMapping(const XMLCh* const prefix,
                                                  const XMLCh* const uri)
{
    const CPLString osURI( transcode(uri) );
    CPLString osPrefix( transcode(prefix) );
    if( osPrefix.empty() )
    {
        std::map<CPLString, CPLString>::const_iterator oIter 
            = m_oMapDocNSURIToPrefix.find( osURI );
        if( oIter != m_oMapDocNSURIToPrefix.end() )
        {
            osPrefix = oIter->second;
        }
    }
    if( !osPrefix.empty() )
    {
        std::map<CPLString, CPLString>::iterator oIter =
                    m_oMapURIToPrefix.find( osURI );
        if( oIter == m_oMapURIToPrefix.end() )
        {
            m_oMapURIToPrefix[ osURI ] = osPrefix;
            CPLDebug("GMLAS", "Registering prefix=%s for uri=%s",
                     osPrefix.c_str(), osURI.c_str());
        }
        else if( oIter->second != osPrefix )
        {
            CPLDebug("GMLAS",
                     "Existing prefix=%s for uri=%s (new prefix %s not used)",
                    oIter->second.c_str(), osURI.c_str(), osPrefix.c_str());
        }
    }
}

/************************************************************************/
/*                        CollectNamespacePrefixes()                    */
/************************************************************************/

static
void CollectNamespacePrefixes(const char* pszXSDFilename,
                              VSILFILE* fpXSD,
                              std::map<CPLString, CPLString>& oMapURIToPrefix,
                              const std::map<CPLString, CPLString>& oMapDocNSURIToPrefix,
                              CPLString& osGMLVersionFound)
{
    GMLASInputSource oSource(pszXSDFilename, fpXSD, false);
    // This is a bit silly but the startPrefixMapping() callback only gets
    // called when using SAX2XMLReader::parse(), and not when using
    // loadGrammar(), so we have to parse the doc twice.
    SAX2XMLReader* poReader = XMLReaderFactory::createXMLReader ();

    GMLASPrefixMappingHander contentHandler(oMapURIToPrefix,
                                            oMapDocNSURIToPrefix,
                                            osGMLVersionFound);
    poReader->setContentHandler(&contentHandler);

    GMLASErrorHandler oErrorHandler;
    poReader->setErrorHandler(&oErrorHandler);

    poReader->parse(oSource);
    delete poReader;
}

/************************************************************************/
/*                       GMLASAnalyzerEntityResolver                    */
/************************************************************************/

class GMLASAnalyzerEntityResolver: public GMLASBaseEntityResolver
{
        std::map<CPLString, CPLString>& m_oMapURIToPrefix;
        const std::map<CPLString, CPLString>& m_oMapDocNSURIToPrefix;

  public:
        GMLASAnalyzerEntityResolver(const CPLString& osBasePath,
                            std::map<CPLString, CPLString>& oMapURIToPrefix,
                            const std::map<CPLString, CPLString>& oMapDocNSURIToPrefix,
                            GMLASXSDCache& oCache)
            : GMLASBaseEntityResolver(osBasePath, oCache)
            , m_oMapURIToPrefix(oMapURIToPrefix)
            , m_oMapDocNSURIToPrefix(oMapDocNSURIToPrefix)
        {
        }

        virtual void DoExtraSchemaProcessing(const CPLString& osFilename,
                                             VSILFILE* fp) override;
};

/************************************************************************/
/*                         DoExtraSchemaProcessing()                    */
/************************************************************************/

void GMLASAnalyzerEntityResolver::DoExtraSchemaProcessing(
                                             const CPLString& osFilename,
                                             VSILFILE* fp)
{
    CollectNamespacePrefixes(osFilename, fp, m_oMapURIToPrefix,
                             m_oMapDocNSURIToPrefix,
                             m_osGMLVersionFound);
    VSIFSeekL(fp, 0, SEEK_SET);
}

/************************************************************************/
/*                        GMLASSchemaAnalyzer()                         */
/************************************************************************/

GMLASSchemaAnalyzer::GMLASSchemaAnalyzer(
                            GMLASXPathMatcher& oIgnoredXPathMatcher,
                            GMLASXPathMatcher& oChildrenElementsConstraintsXPathMatcher,
                            const std::map<CPLString, std::vector<CPLString> >&
                                                        oMapChildrenElementsConstraints,
                            GMLASXPathMatcher& oForcedFlattenedXPathMatcher,
                            GMLASXPathMatcher& oDisabledFlattenedXPathMatcher )
    : m_oIgnoredXPathMatcher(oIgnoredXPathMatcher)
    , m_oChildrenElementsConstraintsXPathMatcher(oChildrenElementsConstraintsXPathMatcher)
    , m_oForcedFlattenedXPathMatcher(oForcedFlattenedXPathMatcher)
    , m_oDisabledFlattenedXPathMatcher(oDisabledFlattenedXPathMatcher)
    , m_oMapChildrenElementsConstraints(oMapChildrenElementsConstraints)
    , m_bUseArrays(true)
    , m_bUseNullState(false)
    , m_bInstantiateGMLFeaturesOnly(true)
    , m_nIdentifierMaxLength(0)
    , m_bCaseInsensitiveIdentifier(CASE_INSENSITIVE_IDENTIFIER_DEFAULT)
    , m_bPGIdentifierLaundering(PG_IDENTIFIER_LAUNDERING_DEFAULT)
    , m_nMaximumFieldsForFlattening(MAXIMUM_FIELDS_FLATTENING_DEFAULT)
{
    // A few hardcoded namespace uri->prefix mappings
    m_oMapURIToPrefix[ szXMLNS_URI ] = szXMLNS_PREFIX;
    m_oMapURIToPrefix[ szXSI_URI ] = szXSI_PREFIX;
}

/************************************************************************/
/*                               GetPrefix()                            */
/************************************************************************/

CPLString GMLASSchemaAnalyzer::GetPrefix( const CPLString& osNamespaceURI )
{
    if( osNamespaceURI.empty() )
        return "";
    std::map<CPLString,CPLString>::const_iterator oIter =
                                        m_oMapURIToPrefix.find(osNamespaceURI);
    if( oIter != m_oMapURIToPrefix.end() )
        return oIter->second;
    else if( !osNamespaceURI.empty() )
    {
        // If the schema doesn't define a xmlns:MYPREFIX=myuri, then forge a
        // fake prefix for conveniency
        CPLString osPrefix;
        if( osNamespaceURI.find(szOPENGIS_URL) == 0 )
            osPrefix = osNamespaceURI.substr( strlen(szOPENGIS_URL) );
        else if( osNamespaceURI.find("http://") == 0 )
            osPrefix = osNamespaceURI.substr( strlen("http://") );
        else
            osPrefix = osNamespaceURI;
        for(size_t i = 0; i < osPrefix.size(); i++ )
        {
            if( !isalnum(osPrefix[i]) )
                osPrefix[i] = '_';
        }
        m_oMapURIToPrefix[osNamespaceURI] = osPrefix;
        CPLDebug("GMLAS",
                 "Cannot find prefix for ns='%s'. Forging %s",
                 osNamespaceURI.c_str(),
                 osPrefix.c_str());
        return osPrefix;
    }
    else
    {
        CPLDebug("GMLAS",
                 "Cannot find prefix for ns='%s'.",
                 osNamespaceURI.c_str());
        return "";
    }
}

/************************************************************************/
/*                               MakeXPath()                            */
/************************************************************************/

CPLString GMLASSchemaAnalyzer::MakeXPath( const CPLString& osNamespaceURI,
                                          const CPLString& osName )
{
    const CPLString osPrefix(GetPrefix(osNamespaceURI));
    if( osPrefix.empty() )
        return osName;
    return osPrefix + ":" + osName;
}

/************************************************************************/
/*                         GetNSOfLastXPathComponent()                  */
/************************************************************************/

// Return the namespace (if any) of the last component of the XPath
static CPLString GetNSOfLastXPathComponent(const CPLString& osXPath )
{
    size_t nPos = osXPath.rfind('@');
    if( nPos != std::string::npos )
        nPos ++;
    else
    {
        nPos = osXPath.rfind('/');
        if( nPos != std::string::npos )
            nPos ++;
        else
            nPos = 0;
    }
    size_t nPosColumn = osXPath.find(':', nPos);
    if( nPosColumn == std::string::npos )
        return CPLString();
    return CPLString(osXPath.substr(nPos, nPosColumn - nPos));
}

/************************************************************************/
/*                         LaunderFieldNames()                          */
/************************************************************************/

// Make sure that field names are unique within the class
void GMLASSchemaAnalyzer::LaunderFieldNames( GMLASFeatureClass& oClass )
{
    std::vector<GMLASField>& aoFields = oClass.GetFields();

    // Duplicates can happen if a class has both an element and an attribute
    // with same name, and/or attributes/elements with same name in different
    // namespaces.
    bool bHasDoneSomeRenaming = false;
    do
    {
        bHasDoneSomeRenaming = false;

        // Detect duplicated field names
        std::map<CPLString, std::vector<int> > oSetNames;
        for(int i=0; i< static_cast<int>(aoFields.size());i++)
        {
            if( aoFields[i].GetCategory() == GMLASField::REGULAR )
            {
                oSetNames[ aoFields[i].GetName() ].push_back(i ) ;
            }
        }

        // Iterate over the unique names
        std::map<CPLString, std::vector<int> >::const_iterator
                oIter = oSetNames.begin();
        for(; oIter != oSetNames.end(); ++oIter)
        {
            // Has it duplicates ?
            const size_t nOccurrences = oIter->second.size();
            if( nOccurrences > 1 )
            {
                const CPLString oClassNS =
                        GetNSOfLastXPathComponent(oClass.GetXPath());
                bool bHasDoneRemnamingForThatCase = false;

                for(size_t i=0; i<nOccurrences;i++)
                {
                    GMLASField& oField = aoFields[oIter->second[i]];
                    // CPLDebug("GMLAS", "%s", oField.GetXPath().c_str() );
                    const CPLString oNS(
                                GetNSOfLastXPathComponent(oField.GetXPath()));
                    // If the field has a namespace that is not the one of its
                    // class, then prefix its name with its namespace
                    if( !oNS.empty() && oNS != oClassNS &&
                        !STARTS_WITH(oField.GetName(), (oNS + "_").c_str() ) )
                    {
                        bHasDoneSomeRenaming = true;
                        bHasDoneRemnamingForThatCase = true;
                        oField.SetName( oNS + "_" + oField.GetName() );
                        break;
                    }
                    // If it is an attribute without a particular namespace,
                    // then suffix with _attr
                    else if( oNS.empty() &&
                             oField.GetXPath().find('@') != std::string::npos &&
                             oField.GetName().find("_attr") == std::string::npos )
                    {
                        bHasDoneSomeRenaming = true;
                        bHasDoneRemnamingForThatCase = true;
                        oField.SetName( oField.GetName() + "_attr" );
                        break;
                    }
                }

                // If none of the above renaming strategies have worked, then
                // append a counter to the duplicates.
                if( !bHasDoneRemnamingForThatCase )
                {
                    for(size_t i=0; i<nOccurrences;i++)
                    {
                        GMLASField& oField = aoFields[oIter->second[i]];
                        if( i > 0 )
                        {
                            bHasDoneSomeRenaming = true;
                            oField.SetName( oField.GetName() +
                                CPLSPrintf("%d", static_cast<int>(i)+1) );
                        }
                    }
                }
            }
        }
    }
    // As renaming could have created new duplicates (hopefully not!), loop
    // until no renaming has been done.
    while( bHasDoneSomeRenaming );

    // Now check if we must truncate names
    if( m_nIdentifierMaxLength >= MIN_VALUE_OF_MAX_IDENTIFIER_LENGTH )
    {
        for(size_t i=0; i< aoFields.size();i++)
        {
            int nNameSize = static_cast<int>(aoFields[i].GetName().size());
            if( nNameSize > m_nIdentifierMaxLength )
            {
                aoFields[i].SetName(TruncateIdentifier(aoFields[i].GetName()));
            }
        }
    }

    if( m_bPGIdentifierLaundering )
    {
        for(size_t i=0; i< aoFields.size();i++)
        {
            char* pszLaundered = OGRPGCommonLaunderName( aoFields[i].GetName(),
                                                         "GMLAS" );
            aoFields[i].SetName( pszLaundered );
            CPLFree( pszLaundered );
        }
    }

    // Detect duplicated field names
    std::map<CPLString, std::vector<int> > oSetNames;
    for(int i=0; i< static_cast<int>(aoFields.size());i++)
    {
        if( aoFields[i].GetCategory() == GMLASField::REGULAR )
        {
            CPLString osName( aoFields[i].GetName());
            if( m_bCaseInsensitiveIdentifier )
                osName.toupper();
            oSetNames[ osName ].push_back(i ) ;
        }
    }

    // Iterate over the unique names
    std::map<CPLString, std::vector<int> >::const_iterator
            oIter = oSetNames.begin();
    for(; oIter != oSetNames.end(); ++oIter)
    {
        // Has it duplicates ?
        const size_t nOccurrences = oIter->second.size();
        if( nOccurrences > 1 )
        {
            for(size_t i=0; i<nOccurrences;i++)
            {
                GMLASField& oField = aoFields[oIter->second[i]];
                oField.SetName( AddSerialNumber( oField.GetName(),
                                                    static_cast<int>(i+1),
                                                    nOccurrences) );
            }
        }
    }

    // Recursively process nested classes
    std::vector<GMLASFeatureClass>& aoNestedClasses = oClass.GetNestedClasses();
    for(size_t i=0; i<aoNestedClasses.size();i++)
    {
        LaunderFieldNames( aoNestedClasses[i] );
    }
}

/************************************************************************/
/*                       CollectClassesReferences()                     */
/************************************************************************/

void GMLASSchemaAnalyzer::CollectClassesReferences(
                                GMLASFeatureClass& oClass,
                                std::vector<GMLASFeatureClass*>& aoClasses )
{
    aoClasses.push_back(&oClass);
    std::vector<GMLASFeatureClass>& aoNestedClasses = oClass.GetNestedClasses();
    for(size_t i=0; i<aoNestedClasses.size();i++)
    {
        CollectClassesReferences( aoNestedClasses[i], aoClasses );
    }
}

/************************************************************************/
/*                         LaunderClassNames()                          */
/************************************************************************/

void GMLASSchemaAnalyzer::LaunderClassNames()
{
    std::vector<GMLASFeatureClass*> aoClasses;
    for(size_t i=0; i< m_aoClasses.size();i++)
    {
        CollectClassesReferences( m_aoClasses[i], aoClasses );
    }

    if( m_nIdentifierMaxLength >= MIN_VALUE_OF_MAX_IDENTIFIER_LENGTH )
    {
        for(size_t i=0; i< aoClasses.size();i++)
        {
            int nNameSize = static_cast<int>(aoClasses[i]->GetName().size());
            if( nNameSize > m_nIdentifierMaxLength )
            {
                aoClasses[i]->SetName(TruncateIdentifier(aoClasses[i]->GetName()));
            }
        }
    }

    if( m_bPGIdentifierLaundering )
    {
        for(size_t i=0; i< aoClasses.size();i++)
        {
            char* pszLaundered = OGRPGCommonLaunderName( aoClasses[i]->GetName(),
                                                         "GMLAS" );
            aoClasses[i]->SetName( pszLaundered );
            CPLFree( pszLaundered );
        }
    }

    // Detect duplicated names. This should normally not happen in normal
    // conditions except if you have classes like
    // prefix_foo, prefix:foo, other_prefix:foo
    // or if names have been truncated in the previous step
    std::map<CPLString, std::vector<int> > oSetNames;
    for(int i=0; i< static_cast<int>(aoClasses.size());i++)
    {
        CPLString osName( aoClasses[i]->GetName() );
        if( m_bCaseInsensitiveIdentifier )
            osName.toupper();
        oSetNames[ osName ].push_back(i ) ;
    }

    // Iterate over the unique names
    std::map<CPLString, std::vector<int> >::const_iterator
            oIter = oSetNames.begin();
    for(; oIter != oSetNames.end(); ++oIter)
    {
        // Has it duplicates ?
        const size_t nOccurrences = oIter->second.size();
        if( nOccurrences > 1 )
        {
            for(size_t i=0; i<nOccurrences;i++)
            {
                GMLASFeatureClass* poClass = aoClasses[oIter->second[i]];
                poClass->SetName( AddSerialNumber(poClass->GetName(),
                                                  static_cast<int>(i+1),
                                                  nOccurrences) );
            }
        }
    }
}

/************************************************************************/
/*                        AddSerialNumber()                             */
/************************************************************************/

CPLString GMLASSchemaAnalyzer::AddSerialNumber(const CPLString& osNameIn,
                                               int iOccurrence,
                                               size_t nOccurrences)
{
    CPLString osName(osNameIn);
    const int nDigitsSize = (nOccurrences < 10) ? 1:
                            (nOccurrences < 100) ? 2 : 3;
    char szDigits[4];
    snprintf(szDigits, sizeof(szDigits), "%0*d",
                nDigitsSize, iOccurrence);
    if( m_nIdentifierMaxLength >= MIN_VALUE_OF_MAX_IDENTIFIER_LENGTH )
    {
        if( static_cast<int>(osName.size()) < m_nIdentifierMaxLength )
        {
            if( static_cast<int>(osName.size()) + nDigitsSize <
                                            m_nIdentifierMaxLength )
            {
                osName += szDigits;
            }
            else
            {
                osName.resize(m_nIdentifierMaxLength - nDigitsSize);
                osName += szDigits;
            }
        }
        else
        {
            osName.resize(osName.size() - nDigitsSize);
            osName += szDigits;
        }
    }
    else
    {
        osName += szDigits;
    }
    return osName;
}

/************************************************************************/
/*                      TruncateIdentifier()                            */
/************************************************************************/

CPLString GMLASSchemaAnalyzer::TruncateIdentifier(const CPLString& osName)
{
    int nExtra = static_cast<int>(osName.size()) - m_nIdentifierMaxLength;
    CPLAssert(nExtra > 0);

    // Decompose in tokens
    char** papszTokens = CSLTokenizeString2(osName, "_",
                                            CSLT_ALLOWEMPTYTOKENS );
    std::vector< char > achDelimiters;
    std::vector< CPLString > aosTokens;
    for( int j=0; papszTokens[j] != NULL; ++j )
    {
        const char* pszToken = papszTokens[j];
        bool bIsCamelCase = false;
        // Split parts like camelCase or CamelCase into several tokens
        if( pszToken[0] != '\0' && islower(pszToken[1]) )
        {
            bIsCamelCase = true;
            bool bLastIsLower = true;
            std::vector<CPLString> aoParts;
            CPLString osCurrentPart;
            osCurrentPart += pszToken[0];
            osCurrentPart += pszToken[1];
            for( int k=2; pszToken[k]; ++k)
            {
                if( isupper(pszToken[k]) )
                {
                    if( !bLastIsLower )
                    {
                        bIsCamelCase = false;
                        break;
                    }
                    aoParts.push_back(osCurrentPart);
                    osCurrentPart.clear();
                    bLastIsLower = false;
                }
                else
                {
                    bLastIsLower = true;
                }
                osCurrentPart += pszToken[k];
            }
            if( bIsCamelCase )
            {
                if( !osCurrentPart.empty() )
                    aoParts.push_back(osCurrentPart);
                for( size_t k=0; k<aoParts.size(); ++k )
                {
                    achDelimiters.push_back( (j > 0 && k == 0) ? '_' : '\0' );
                    aosTokens.push_back( aoParts[k] );
                }
            }
        }
        if( !bIsCamelCase )
        {
            achDelimiters.push_back( (j > 0) ? '_' : '\0' );
            aosTokens.push_back( pszToken );
        }
    }
    CSLDestroy(papszTokens);

    // Truncate identifier by removing last character of longest part
    bool bHasDoneSomething = true;
    while( nExtra > 0 && bHasDoneSomething )
    {
        bHasDoneSomething = false;
        int nMaxSize = 0;
        size_t nIdxMaxSize = 0;
        for( size_t j=0; j < aosTokens.size(); ++j )
        {
            int nTokenLen = static_cast<int>(aosTokens[j].size());
            if( nTokenLen > nMaxSize )
            {
                // Avoid truncating last token unless it is excessively longer
                // than previous ones.
                if( j < aosTokens.size() - 1 ||
                    nTokenLen > 2 * nMaxSize )
                {
                    nMaxSize = nTokenLen;
                    nIdxMaxSize = j;
                }
            }
        }

        if( nMaxSize > 1 )
        {
            aosTokens[nIdxMaxSize].resize( nMaxSize - 1 );
            bHasDoneSomething = true;
            nExtra --;
        }
    }

    // Reassemble truncated parts
    CPLString osNewName;
    for( size_t j=0; j < aosTokens.size(); ++j )
    {
        if( achDelimiters[j] )
            osNewName += achDelimiters[j];
        osNewName += aosTokens[j];
    }

    // If we are still longer than max allowed, truncate beginning of name
    if( nExtra > 0 )
    {
        osNewName = osNewName.substr(nExtra);
    }
    CPLAssert( static_cast<int>(osNewName.size()) == m_nIdentifierMaxLength );
    return osNewName;
}

/************************************************************************/
/*                       GMLASUniquePtr()                               */
/************************************************************************/

// Poor-man std::unique_ptr
template<class T> class GMLASUniquePtr
{
        T* m_p;

        GMLASUniquePtr(const GMLASUniquePtr&);
        GMLASUniquePtr& operator=(const GMLASUniquePtr&);

    public:
        explicit GMLASUniquePtr(T* p): m_p(p) {}
       ~GMLASUniquePtr() { delete m_p; }

       T* operator->() const { CPLAssert(m_p); return m_p; }

       T* get () const { return m_p; }
       T* release() { T* ret = m_p; m_p = NULL; return ret; }
};

/************************************************************************/
/*                   GetTopElementDeclarationFromXPath()                */
/************************************************************************/

XSElementDeclaration* GMLASSchemaAnalyzer::GetTopElementDeclarationFromXPath(
                                                    const CPLString& osXPath,
                                                    XSModel* poModel)
{
    const char* pszTypename = osXPath.c_str();
    const char* pszName = strrchr(pszTypename, ':');
    if( pszName )
        pszName ++;
    XSElementDeclaration* poEltDecl = NULL;
    if( pszName != NULL )
    {
        CPLString osNSPrefix = pszTypename;
        osNSPrefix.resize( pszName - 1 - pszTypename );
        CPLString osName = pszName;
        CPLString osNSURI;

        std::map<CPLString, CPLString>::const_iterator oIterNS =
                                            m_oMapURIToPrefix.begin();
        for( ; oIterNS != m_oMapURIToPrefix.end(); ++oIterNS)
        {
            const CPLString& osIterNSURI(oIterNS->first);
            const CPLString& osIterNSPrefix(oIterNS->second);
            if( osNSPrefix == osIterNSPrefix )
            {
                osNSURI = osIterNSURI;
                break;
            }
        }
        XMLCh* xmlNS = XMLString::transcode(osNSURI);
        XMLCh* xmlName = XMLString::transcode(osName);
        poEltDecl = poModel->getElementDeclaration(xmlName, xmlNS);
        XMLString::release( &xmlNS );
        XMLString::release( &xmlName );
    }
    else
    {
        XMLCh* xmlName = XMLString::transcode(pszTypename);
        poEltDecl = poModel->getElementDeclaration(xmlName, NULL);
        XMLString::release( &xmlName );
    }
    return poEltDecl;
}

/************************************************************************/
/*                        IsEltCompatibleOfFC()                         */
/************************************************************************/

static XSComplexTypeDefinition* IsEltCompatibleOfFC(
                                            XSElementDeclaration* poEltDecl)
{
    XSTypeDefinition* poTypeDef = poEltDecl->getTypeDefinition();
    if( poTypeDef->getTypeCategory() == XSTypeDefinition::COMPLEX_TYPE &&
        transcode(poEltDecl->getName()) != szFEATURE_COLLECTION )
    {
        XSComplexTypeDefinition* poCT =
                    reinterpret_cast<XSComplexTypeDefinition*>(poTypeDef);
        XSComplexTypeDefinition::CONTENT_TYPE eContentType(
                                                poCT->getContentType());
        if( eContentType == XSComplexTypeDefinition::CONTENTTYPE_ELEMENT ||
            eContentType == XSComplexTypeDefinition::CONTENTTYPE_MIXED )
        {
            return poCT;
        }
    }
    return NULL;
}

/************************************************************************/
/*                          DerivesFromGMLFeature()                     */
/************************************************************************/

bool GMLASSchemaAnalyzer::DerivesFromGMLFeature(XSElementDeclaration* poEltDecl)
{
    XSElementDeclaration* poIter = poEltDecl;
    while( true )
    {
        XSElementDeclaration* poSubstGroup =
            poIter->getSubstitutionGroupAffiliation();
        if( poSubstGroup == NULL )
            break;
        const CPLString osSubstNS(
                    transcode(poSubstGroup->getNamespace()) );
        const CPLString osSubstName(
                    transcode(poSubstGroup->getName()) );
        if( IsGMLNamespace(osSubstNS) &&
            osSubstName == "_FeatureCollection" )
        {
            return false;
        }
        if( IsGMLNamespace(osSubstNS) &&
            (osSubstName == "AbstractFeature" ||
                osSubstName == "_Feature") )
        {
            return true;
        }
        poIter = poSubstGroup;
    }
    return false;
}

/************************************************************************/
/*                               Analyze()                              */
/************************************************************************/

bool GMLASSchemaAnalyzer::Analyze(GMLASXSDCache& oCache,
                                  const CPLString& osBaseDirname,
                                  std::vector<PairURIFilename>& aoXSDs,
                                  bool bSchemaFullChecking,
                                  bool bHandleMultipleImports)
{
    GMLASUniquePtr<XMLGrammarPool> poGrammarPool(
         (new XMLGrammarPoolImpl(XMLPlatformUtils::fgMemoryManager)));

    std::vector<CPLString> aoNamespaces;
    GMLASAnalyzerEntityResolver oXSDEntityResolver( CPLString(),
                                                    m_oMapURIToPrefix,
                                                    m_oMapDocNSURIToPrefix,
                                                    oCache );

    aoNamespaces.push_back("");
    for( size_t i = 0; i < aoXSDs.size(); i++ )
    {
        const CPLString osURI(aoXSDs[i].first);
        const CPLString osXSDFilename(aoXSDs[i].second);

        GMLASUniquePtr<SAX2XMLReader> poParser(
                    XMLReaderFactory::createXMLReader(
                                    XMLPlatformUtils::fgMemoryManager,
                                    poGrammarPool.get()));

        // Commonly useful configuration.
        //
        poParser->setFeature (XMLUni::fgSAX2CoreNameSpaces, true);
        poParser->setFeature (XMLUni::fgSAX2CoreNameSpacePrefixes, true);
        poParser->setFeature (XMLUni::fgSAX2CoreValidation, true);

        // Enable validation.
        //
        poParser->setFeature (XMLUni::fgXercesSchema, true);

        poParser->setFeature (XMLUni::fgXercesValidationErrorAsFatal, false);

        // Use the loaded grammar during parsing.
        //
        poParser->setFeature (XMLUni::fgXercesUseCachedGrammarInParse, true);

        // Don't load schemas from any other source (e.g., from XML document's
        // xsi:schemaLocation attributes).
        //
        poParser->setFeature (XMLUni::fgXercesLoadSchema, false);

        Grammar* poGrammar = NULL;
        if( !GMLASReader::LoadXSDInParser( poParser.get(),
                                           oCache,
                                           oXSDEntityResolver,
                                           osBaseDirname,
                                           osXSDFilename,
                                           &poGrammar,
                                           bSchemaFullChecking,
                                           bHandleMultipleImports ) )
        {
            return false;
        }

        // Some .xsd like
        // http://www.opengis.net/gwml-main/2.1 -> https://wfspoc.brgm-rec.fr/constellation/WS/wfs/BRGM:GWML2?request=DescribeFeatureType&version=2.0.0&service=WFS&namespace=xmlns(ns1=http://www.opengis.net/gwml-main/2.1)&typenames=ns1:GW_Aquifer
        // do not have a declared targetNamespace, so use the one of the
        // schemaLocation if the grammar returns an empty namespace.
        CPLString osGrammarURI( transcode(poGrammar->getTargetNamespace()) );
        if( osGrammarURI.empty() )
        {
            if( !osURI.empty() )
                osGrammarURI = osURI;
        }
        if( !osGrammarURI.empty() )
        {
            // Patch back the aoXSDs element in case we didn't know the
            // namespace URI initially
            if( osURI.empty() )
                aoXSDs[i].first = osGrammarURI;
            aoNamespaces.push_back( osGrammarURI );
        }
    }

    m_osGMLVersionFound = oXSDEntityResolver.GetGMLVersionFound();
    m_oSetSchemaURLs = oXSDEntityResolver.GetSchemaURLS();

    m_oIgnoredXPathMatcher.SetDocumentMapURIToPrefix( m_oMapURIToPrefix );
    m_oChildrenElementsConstraintsXPathMatcher.SetDocumentMapURIToPrefix( m_oMapURIToPrefix );
    m_oForcedFlattenedXPathMatcher.SetDocumentMapURIToPrefix( m_oMapURIToPrefix );
    m_oDisabledFlattenedXPathMatcher.SetDocumentMapURIToPrefix( m_oMapURIToPrefix );

    XSModel* poModel = getGrammarPool(poGrammarPool.get());
    CPLAssert(poModel); // should not be null according to doc

#if 0
    XSNamespaceItem* nsItem = poModel->getNamespaceItem(
                                        loadedGrammar->getTargetNamespace());
    if( nsItem == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "getNamespaceItem(%s) failed",
                 transcode(loadedGrammar->getTargetNamespace()).c_str());
        return false;
    }
#endif

    bool bFoundGMLFeature = false;

    // Initial pass, in all namespaces, to figure out inheritance relationships
    // and group models that have names
    std::map<CPLString, CPLString> oMapURIToPrefixWithEmpty(m_oMapURIToPrefix);
    oMapURIToPrefixWithEmpty[""] = "";
    std::map<CPLString, CPLString>::const_iterator oIterNS =
                                                oMapURIToPrefixWithEmpty.begin();
    for( ; oIterNS != oMapURIToPrefixWithEmpty.end(); ++oIterNS)
    {
        const CPLString& osNSURI(oIterNS->first);
        if( osNSURI == szXS_URI ||
            osNSURI == szXSI_URI ||
            osNSURI == szXMLNS_URI ||
            osNSURI == szXLINK_URI )
        {
            continue;
        }

        XMLCh* xmlNamespace = XMLString::transcode(osNSURI.c_str());

        XSNamedMap<XSObject>* poMapModelGroupDefinition =
            poModel->getComponentsByNamespace(XSConstants::MODEL_GROUP_DEFINITION,
                                            xmlNamespace);

        // Remember group models that have names
        for(XMLSize_t i = 0; poMapModelGroupDefinition != NULL &&
                             i <  poMapModelGroupDefinition->getLength(); i++ )
        {
            XSModelGroupDefinition* modelGroupDefinition =
                reinterpret_cast<XSModelGroupDefinition*>(
                                            poMapModelGroupDefinition->item(i));
            m_oMapModelGroupToMGD[modelGroupDefinition->getModelGroup()]
                            = modelGroupDefinition;
        }

        CPLDebug("GMLAS", "Discovering substitutions of %s (%s)",
                 oIterNS->second.c_str(), osNSURI.c_str());

        XSNamedMap<XSObject>* poMapElements = poModel->getComponentsByNamespace(
                            XSConstants::ELEMENT_DECLARATION, xmlNamespace);

        for(XMLSize_t i = 0; poMapElements != NULL &&
                             i < poMapElements->getLength(); i++ )
        {
            XSElementDeclaration* poEltDecl =
                reinterpret_cast<XSElementDeclaration*>(poMapElements->item(i));
            XSElementDeclaration* poSubstGroup =
                            poEltDecl->getSubstitutionGroupAffiliation();
            const CPLString osEltXPath(
                            MakeXPath(transcode(poEltDecl->getNamespace()),
                                      transcode(poEltDecl->getName())));
            m_oMapXPathToEltDecl[ osEltXPath ] = poEltDecl;
            if( poSubstGroup )
            {
                m_oMapParentEltToChildElt[poSubstGroup].push_back(poEltDecl);
#ifdef DEBUG_VERBOSE
                CPLString osParentType(MakeXPath(
                            transcode(poSubstGroup->getNamespace()),
                            transcode(poSubstGroup->getName())));
                CPLDebug("GMLAS", "%s is a substitution for %s",
                        osEltXPath.c_str(),
                        osParentType.c_str());
#endif

                // Check if this element derives from gml:_Feature/AbstractFeature
                if( !bFoundGMLFeature &&
                    m_bInstantiateGMLFeaturesOnly &&
                    !IsGMLNamespace(osNSURI) &&
                    DerivesFromGMLFeature(poEltDecl) )
                {
                    CPLDebug("GMLAS",
                                "Restricting (in first pass) top level "
                                "elements to those deriving from "
                                "gml:_Feature/gml:AbstractFeature (due "
                                "to %s found)",
                                osEltXPath.c_str());
                    bFoundGMLFeature = true;
                }
            }
        }

        XMLString::release(&xmlNamespace);
    }

    // Find which elements must be top levels (because referenced several
    // times)
    std::set<XSElementDeclaration*> oSetVisitedEltDecl;
    std::set<XSModelGroup*> oSetVisitedModelGroups;
    std::vector<XSElementDeclaration*> oVectorEltsForTopClass;

    // For some reason, different XSElementDeclaration* can point to the
    // same element, but we only want to instantiate a single class.
    // This is the case for base:SpatialDataSet in
    // inspire/geologicalunit/geologicalunit.gml test dataset.
    std::set<CPLString> aoSetXPathEltsForTopClass;

    for( int iPass = 0; iPass < 2; ++iPass )
    {
        for( size_t iNS = 0; iNS < aoNamespaces.size(); iNS++ )
        {
            XMLCh* xmlNamespace = XMLString::transcode(aoNamespaces[iNS].c_str());

            XSNamedMap<XSObject>* poMapElements = poModel->getComponentsByNamespace(
                XSConstants::ELEMENT_DECLARATION, xmlNamespace);

            for(XMLSize_t i = 0; poMapElements != NULL &&
                                i < poMapElements->getLength(); i++ )
            {
                XSElementDeclaration* poEltDecl =
                    reinterpret_cast<XSElementDeclaration*>(poMapElements->item(i));
                XSComplexTypeDefinition* poCT = IsEltCompatibleOfFC(poEltDecl);
                if( !poEltDecl->getAbstract() && poCT != NULL  )
                {
                    const CPLString osXPath(MakeXPath(
                                    transcode(poEltDecl->getNamespace()),
                                    transcode(poEltDecl->getName())));
                    if( !IsIgnoredXPath(osXPath ) )
                    {
                        if( bFoundGMLFeature &&
                            m_bInstantiateGMLFeaturesOnly &&
                            !DerivesFromGMLFeature(poEltDecl) )
                        {
                            // Do nothing
                        }
                        else if( iPass == 0)
                        {
#ifdef DEBUG_VERBOSE
                        CPLDebug("GMLAS", "%s (%s) must be exposed as "
                                 "top-level (is top level in imported schemas)",
                                osXPath.c_str(),
                                transcode(poEltDecl->getTypeDefinition()->
                                                            getName()).c_str());
#endif
                            oSetVisitedEltDecl.insert( poEltDecl );
                            if( aoSetXPathEltsForTopClass.find( osXPath ) ==
                                aoSetXPathEltsForTopClass.end() )
                            {
                                m_oSetEltsForTopClass.insert( poEltDecl );
                                oVectorEltsForTopClass.push_back( poEltDecl );
                                aoSetXPathEltsForTopClass.insert( osXPath );
                            }
                        }
                        else
                        {
                            bool bSimpleEnoughOut = true;
                            int nSubCountSubEltOut = 0;
                            FindElementsWithMustBeToLevel(
                                    osXPath,
                                    poCT->getParticle()->getModelGroupTerm(),
                                    0,
                                    oSetVisitedEltDecl,
                                    oSetVisitedModelGroups,
                                    oVectorEltsForTopClass,
                                    aoSetXPathEltsForTopClass,
                                    poModel,
                                    bSimpleEnoughOut,
                                    nSubCountSubEltOut );
                        }
                    }
                }
            }

            XMLString::release(&xmlNamespace);
        }
    }

    // Find ambiguous class names
    {
        std::set<XSElementDeclaration*>::const_iterator oIter =
                                            m_oSetEltsForTopClass.begin();
        for(; oIter != m_oSetEltsForTopClass.end(); ++oIter )
        {
            CPLString osName(transcode((*oIter)->getName()));
            m_oMapEltNamesToInstanceCount[osName] ++;
        }
    }

    // Instantiate all needed typenames
    std::vector<XSElementDeclaration*>::iterator oIter =
                                        oVectorEltsForTopClass.begin();
    for(; oIter != oVectorEltsForTopClass.end(); ++oIter )
    {
        XSElementDeclaration* poEltDecl = *oIter;

        const CPLString osXPath(MakeXPath(
                            transcode(poEltDecl->getNamespace()),
                            transcode(poEltDecl->getName())));

        bool bError = false;
        bool bResolvedType = InstantiateClassFromEltDeclaration(poEltDecl,
                                                                poModel,
                                                                bError);
        if( bError )
        {
            return false;
        }
        if( !bResolvedType )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                        "Couldn't resolve %s (%s)",
                        osXPath.c_str(),
                        transcode(poEltDecl->getTypeDefinition()->getName()).c_str()
                        );
            return false;
        }
    }

    LaunderClassNames();

    return true;
}

/************************************************************************/
/*                            GetAnnotationDoc()                        */
/************************************************************************/

static CPLString GetAnnotationDoc( const XSAnnotation* annotation )
{
    if( !annotation )
        return CPLString();
    CPLString osAnnot(transcode(annotation->getAnnotationString()));
    CPLXMLNode* psRoot = CPLParseXMLString(osAnnot);
    CPLStripXMLNamespace(psRoot, NULL, TRUE);
    CPLString osDoc( CPLGetXMLValue(psRoot, "=annotation.documentation", "") );
    CPLDestroyXMLNode(psRoot);
    return osDoc.Trim();
}

/************************************************************************/
/*                            GetAnnotationDoc()                        */
/************************************************************************/

static CPLString GetAnnotationDoc( const XSAnnotationList* annotationList )
{
    if( !annotationList )
        return CPLString();
    CPLString osRet;
    for( size_t i = 0; i < annotationList->size(); ++i )
    {
        CPLString osDoc( GetAnnotationDoc( annotationList->elementAt(i) ) );
        if( !osDoc.empty() )
        {
            if( !osRet.empty() )
                osRet += "\n";
            osRet += osDoc;
        }
    }
    return osRet;
}

/************************************************************************/
/*                            GetAnnotationDoc()                        */
/************************************************************************/

static CPLString GetAnnotationDoc( const XSElementDeclaration* poEltDecl )
{
    XSTypeDefinition* poTypeDef = poEltDecl->getTypeDefinition();
    CPLString osDoc = GetAnnotationDoc( poEltDecl->getAnnotation() );
    XSAnnotationList* list = NULL;
    while( poTypeDef != NULL )
    {
        if( poTypeDef->getTypeCategory() == XSTypeDefinition::COMPLEX_TYPE )
        {
            XSComplexTypeDefinition* poCT =
                        reinterpret_cast<XSComplexTypeDefinition*>(poTypeDef);
            list = poCT->getAnnotations();
        }
        else if( poTypeDef->getTypeCategory() == XSTypeDefinition::SIMPLE_TYPE )
        {
            XSSimpleTypeDefinition* poST =
                        reinterpret_cast<XSSimpleTypeDefinition*>(poTypeDef);
            list = poST->getAnnotations();
        }
        if( list != NULL )
            break;
        XSTypeDefinition* poNewTypeDef = poTypeDef->getBaseType();
        if( poNewTypeDef == poTypeDef )
            break;
        poTypeDef = poNewTypeDef;
    }
    CPLString osDoc2 = GetAnnotationDoc( list );
    if( !osDoc.empty() && !osDoc2.empty() )
    {
        osDoc += "\n";
        osDoc += osDoc2;
    }
    else if( !osDoc2.empty() )
        osDoc = osDoc2;
    return osDoc;
}

/************************************************************************/
/*                  InstantiateClassFromEltDeclaration()                */
/************************************************************************/

bool GMLASSchemaAnalyzer::InstantiateClassFromEltDeclaration(
                                                XSElementDeclaration* poEltDecl,
                                                XSModel* poModel,
                                                bool& bError)
{
    bError = false;
    XSComplexTypeDefinition* poCT = IsEltCompatibleOfFC(poEltDecl);
    if( !poEltDecl->getAbstract() && poCT != NULL )
    {
        GMLASFeatureClass oClass;
        const CPLString osEltName( transcode(poEltDecl->getName()) );
        const CPLString osXPath( MakeXPath(
                                transcode(poEltDecl->getNamespace()),
                                osEltName ) );

        if( IsIgnoredXPath( osXPath ) )
        {
#ifdef DEBUG_VERBOSE
            CPLDebug("GMLAS", "%s is in ignored xpaths",
                        osXPath.c_str());
#endif
            return false;
        }

        if( m_oMapEltNamesToInstanceCount[osEltName] > 1 )
        {
            CPLString osLaunderedXPath(osXPath);
            osLaunderedXPath.replaceAll(':', '_');
            oClass.SetName( osLaunderedXPath );
        }
        else
            oClass.SetName( osEltName );

#ifdef DEBUG_VERBOSE
        CPLDebug("GMLAS", "Instantiating element %s", osXPath.c_str());
#endif
        oClass.SetXPath( osXPath );
        oClass.SetIsTopLevelElt(
            GetTopElementDeclarationFromXPath(osXPath, poModel) != NULL );

        std::set<XSModelGroup*> oSetVisitedModelGroups;

        oClass.SetDocumentation( GetAnnotationDoc(poEltDecl) );

        // might be NULL on swe:values for example
        if( poCT->getParticle() != NULL )
        {
            std::map< CPLString, int > oMapCountOccurrencesOfSameName;
            BuildMapCountOccurrencesOfSameName(
                poCT->getParticle()->getModelGroupTerm(),
                oMapCountOccurrencesOfSameName);

            OGRwkbGeometryType eGeomType = wkbUnknown;
            if( IsGMLNamespace(transcode(poCT->getNamespace())) &&
                (eGeomType = GetOGRGeometryType(poCT)) != wkbNone )
            {
                GMLASField oField;
                oField.SetName( "geometry" );
                oField.SetMinOccurs( 1 );
                oField.SetMaxOccurs( 1 );
                oField.SetType( GMLAS_FT_GEOMETRY, szFAKEXS_GEOMETRY );
                oField.SetGeomType( eGeomType );
                oField.SetXPath( osXPath + szMATCH_ALL );
                oField.SetIncludeThisEltInBlob( true );

                oClass.AddField( oField );
            }
            else if( !ExploreModelGroup(
                                poCT->getParticle()->getModelGroupTerm(),
                                poCT->getAttributeUses(),
                                oClass,
                                0,
                                oSetVisitedModelGroups,
                                poModel,
                                oMapCountOccurrencesOfSameName) )
            {
                bError = true;
                return false;
            }
        }
        else
        {
            // TODO ?
        }

        LaunderFieldNames( oClass );

        m_aoClasses.push_back(oClass);
        return true;
    }
    return false;
}

/************************************************************************/
/*                 SetFieldTypeAndWidthFromDefinition()                 */
/************************************************************************/

void GMLASSchemaAnalyzer::SetFieldTypeAndWidthFromDefinition(
                                                 XSSimpleTypeDefinition* poST,
                                                 GMLASField& oField )
{
    int nMaxLength = 0;
    while( poST->getBaseType() != poST &&
            poST->getBaseType()->getTypeCategory() ==
                                        XSTypeDefinition::SIMPLE_TYPE &&
            !XMLString::equals(poST->getNamespace(),
                               PSVIUni::fgNamespaceXmlSchema) )
    {
        const XMLCh* maxLength = poST->getLexicalFacetValue(
                                    XSSimpleTypeDefinition::FACET_LENGTH );
        if( maxLength == NULL )
        {
            maxLength = poST->getLexicalFacetValue(
                                XSSimpleTypeDefinition::FACET_MAXLENGTH );
        }
        if( maxLength != NULL )
            nMaxLength = MAX(nMaxLength, atoi( transcode(maxLength) ) );
        poST = reinterpret_cast<XSSimpleTypeDefinition*>(poST->getBaseType());
    }

    if( XMLString::equals(poST->getNamespace(), PSVIUni::fgNamespaceXmlSchema) )
    {
        CPLString osType( transcode(poST->getName()) );
        oField.SetType( GMLASField::GetTypeFromString(osType), osType );
    }
    else
    {
        CPLError(CE_Warning, CPLE_AppDefined, "Base type is not a xs: one ???");
    }

    oField.SetWidth( nMaxLength );
}

/************************************************************************/
/*                              IsSame()                                */
/*                                                                      */
/* The objects returned by different PSVI API are not always the same   */
/* so do content inspection to figure out if they are equivalent.       */
/************************************************************************/

bool GMLASSchemaAnalyzer::IsSame( const XSModelGroup* poModelGroup1,
                                  const XSModelGroup* poModelGroup2 )
{
    if( poModelGroup1->getCompositor() != poModelGroup2->getCompositor() )
        return false;

    const XSParticleList* poParticleList1 = poModelGroup1->getParticles();
    const XSParticleList* poParticleList2 = poModelGroup2->getParticles();
    if( poParticleList1->size() != poParticleList2->size() )
        return false;

    for(size_t i = 0; i < poParticleList1->size(); ++i )
    {
        const XSParticle* poParticle1 = poParticleList1->elementAt(i);
        const XSParticle* poParticle2 = poParticleList2->elementAt(i);
        if( poParticle1->getTermType() != poParticle2->getTermType() ||
            poParticle1->getMinOccurs() != poParticle2->getMinOccurs() ||
            poParticle1->getMaxOccurs() != poParticle2->getMaxOccurs() ||
            poParticle1->getMaxOccursUnbounded() !=
                                        poParticle2->getMaxOccursUnbounded() )
        {
            return false;
        }
        switch( poParticle1->getTermType() )
        {
            case XSParticle::TERM_EMPTY:
                break;

            case XSParticle::TERM_ELEMENT:
            {
                const XSElementDeclaration* poElt1 =
                    const_cast<XSParticle*>(poParticle1)->getElementTerm();
                const XSElementDeclaration* poElt2 =
                    const_cast<XSParticle*>(poParticle2)->getElementTerm();
                // Pointer comparison works here
                if( poElt1 != poElt2 )
                    return false;
                break;
            }

            case XSParticle::TERM_MODELGROUP:
            {
                const XSModelGroup* psSubGroup1 =
                    const_cast<XSParticle*>(poParticle1)->getModelGroupTerm();
                const XSModelGroup* psSubGroup2 =
                    const_cast<XSParticle*>(poParticle2)->getModelGroupTerm();
                if( !IsSame(psSubGroup1, psSubGroup2) )
                    return false;
                break;
            }

            case XSParticle::TERM_WILDCARD:
            {
                // TODO: check that pointer comparison works
                const XSWildcard* psWildcard1 =
                    const_cast<XSParticle*>(poParticle1)->getWildcardTerm();
                const XSWildcard* psWildcard2 =
                    const_cast<XSParticle*>(poParticle2)->getWildcardTerm();
                if( psWildcard1 != psWildcard2 )
                    return false;
                break;
            }

            default:
            {
                CPLAssert(FALSE);
                return false;
            }
        }
    }

    return true;
}

/************************************************************************/
/*                           GetGroupName()                             */
/*                                                                      */
/*  The model group object returned when exploring a high level model   */
/*  group isn't the same object as the one returned by model group      */
/*  definitions and has no name. So we have to investigate the content  */
/*  of model groups to figure out if they are the same.                 */
/************************************************************************/

XSModelGroupDefinition* GMLASSchemaAnalyzer::GetGroupDefinition( const XSModelGroup* poModelGroup )
{
    std::map< XSModelGroup*, XSModelGroupDefinition*>::const_iterator oIter =
        m_oMapModelGroupToMGD.begin();
    for(; oIter != m_oMapModelGroupToMGD.end(); ++oIter )
    {
        const XSModelGroup* psIterModelGroup = oIter->first;
        if( IsSame(poModelGroup, psIterModelGroup) )
        {
            return oIter->second;
        }
    }

    return NULL;
}

/************************************************************************/
/*                              IsAnyType()                             */
/************************************************************************/

static bool IsAnyType(XSComplexTypeDefinition* poType)
{
    if( XMLString::equals(poType->getBaseType()->getNamespace(),
                             PSVIUni::fgNamespaceXmlSchema) &&
        transcode( poType->getBaseType()->getName() ) == szXS_ANY_TYPE )
    {
        XSParticle* poParticle = poType->getParticle();
        if( poParticle != NULL )
        {
            XSModelGroup* poGroupTerm = poParticle->getModelGroupTerm();
            if( poGroupTerm != NULL )
            {
                XSParticleList* poParticles =  poGroupTerm->getParticles();
                if( poParticles != NULL )
                {
                    return poParticles->size() == 1 &&
                           poParticles->elementAt(0)->getTermType() ==
                                                    XSParticle::TERM_WILDCARD;
                }
            }
        }
        else if( poType->getDerivationMethod() ==
                                            XSConstants::DERIVATION_EXTENSION )
        {
            // swe:values case
            return true;
        }
    }
    return false;
}

/************************************************************************/
/*                       SetFieldFromAttribute()                        */
/************************************************************************/

void GMLASSchemaAnalyzer::SetFieldFromAttribute(
                                  GMLASField& oField,
                                  XSAttributeUse* poAttr,
                                  const CPLString& osXPathPrefix,
                                  const CPLString& osNamePrefix)
{
    XSAttributeDeclaration* poAttrDecl = poAttr->getAttrDeclaration();
    XSSimpleTypeDefinition* poAttrType = poAttrDecl->getTypeDefinition();

    SetFieldTypeAndWidthFromDefinition(poAttrType, oField);

    CPLString osNS(transcode(poAttrDecl->getNamespace()));
    CPLString osName(transcode(poAttrDecl->getName()));

    if( osNamePrefix.empty() )
        oField.SetName( osName );
    else
        oField.SetName( osNamePrefix + "_" + osName );

    oField.SetXPath( osXPathPrefix + "/@" +
                        MakeXPath( osNS, osName) );
    if( poAttr->getRequired() )
    {
        oField.SetNotNullable( true );
    }
    oField.SetMinOccurs( oField.IsNotNullable() ? 1 : 0 );
    oField.SetMaxOccurs( 1 );
    if( poAttr->getConstraintType() ==
                            XSConstants::VALUE_CONSTRAINT_FIXED )
    {
        oField.SetFixedValue(
                    transcode(poAttr->getConstraintValue()) );
    }
    else if( poAttr->getConstraintType() ==
                            XSConstants::VALUE_CONSTRAINT_DEFAULT )
    {
        oField.SetDefaultValue(
                    transcode(poAttr->getConstraintValue()) );
    }

    const bool bIsList =
        ( poAttrType->getVariety() == XSSimpleTypeDefinition::VARIETY_LIST );
    if( bIsList )
    {
        SetFieldTypeAndWidthFromDefinition(poAttrType->getItemType(), oField);
        if( m_bUseArrays && IsCompatibleOfArray(oField.GetType()) )
        {
            oField.SetList( true );
            oField.SetArray( true );
        }
        else
        {
            // We should probably create an auxiliary table here, but this
            // is too corner case for now...
            oField.SetType( GMLAS_FT_STRING, szXS_STRING );
        }
    }

    oField.SetDocumentation( GetAnnotationDoc( poAttrDecl->getAnnotation() ) );
}

/************************************************************************/
/*                      GetConcreteImplementationTypes()                */
/************************************************************************/

void GMLASSchemaAnalyzer::GetConcreteImplementationTypes(
                                XSElementDeclaration* poParentElt,
                                std::vector<XSElementDeclaration*>& apoImplEltList)
{
    tMapParentEltToChildElt::const_iterator oIter =
        m_oMapParentEltToChildElt.find( poParentElt );
    if( oIter != m_oMapParentEltToChildElt.end() )
    {
        for( size_t j = 0; j < oIter->second.size(); j++ )
        {
            XSElementDeclaration* poSubElt = oIter->second[j];
            if( IsEltCompatibleOfFC(poSubElt) )
            {
                if( !poSubElt->getAbstract() )
                {
                    apoImplEltList.push_back(poSubElt);
                }
            }
            GetConcreteImplementationTypes(poSubElt, apoImplEltList);
        }
    }
}

/************************************************************************/
/*                       GetConstraintChildrenElements()                */
/************************************************************************/

std::vector<XSElementDeclaration*>
    GMLASSchemaAnalyzer::GetConstraintChildrenElements(const CPLString& osFullXPath)
{

    std::vector<XSElementDeclaration*> oVectorRes;
    CPLString osMatched;
    if( m_oChildrenElementsConstraintsXPathMatcher.MatchesRefXPath(
                                                    osFullXPath, osMatched) )
    {
        const std::vector<CPLString>& oVector =
                                m_oMapChildrenElementsConstraints[osMatched];
        const std::map<CPLString, CPLString>& oMapPrefixToURI =
                m_oChildrenElementsConstraintsXPathMatcher.GetMapPrefixToURI();
        for( size_t j = 0; j < oVector.size(); ++j )
        {
            const CPLString& osSubElt(oVector[j]);
            CPLString osSubEltPrefix;
            CPLString osSubEltURI;
            CPLString osSubEltType(osSubElt);
            size_t nPos = osSubElt.find(":");
            if( nPos != std::string::npos )
            {
                osSubEltPrefix = osSubElt.substr(0, nPos);
                osSubEltType = osSubElt.substr(nPos+1);

                std::map<CPLString, CPLString>::const_iterator oIter2 =
                    oMapPrefixToURI.find(osSubEltPrefix);
                if( oIter2 != oMapPrefixToURI.end() )
                {
                    osSubEltURI = oIter2->second;
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                            "Cannot find prefix of type constraint %s",
                            osSubElt.c_str());
                }
            }

            const CPLString osSubEltXPath(MakeXPath(osSubEltURI, osSubEltType));
            std::map<CPLString, XSElementDeclaration*>::const_iterator oIter2 =
                m_oMapXPathToEltDecl.find(osSubEltXPath);
            if( oIter2 != m_oMapXPathToEltDecl.end() )
            {
                XSElementDeclaration* poSubElt = oIter2->second;
                if( IsEltCompatibleOfFC(poSubElt) )
                {
                    oVectorRes.push_back(poSubElt);
                }
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Cannot find element declaration of type constraint %s",
                         osSubElt.c_str());
            }
        }
    }
    return oVectorRes;
}

/************************************************************************/
/*                        GetOGRGeometryType()                          */
/************************************************************************/

static OGRwkbGeometryType GetOGRGeometryType( XSTypeDefinition* poTypeDef )
{
    const struct MyStruct
    {
        const char* pszName;
        OGRwkbGeometryType eType;
    } asArray[] = {
        { "GeometryPropertyType", wkbUnknown },
        { "PointPropertyType", wkbPoint },
        { "PolygonPropertyType", wkbPolygon },
        { "LineStringPropertyType", wkbLineString },
        { "MultiPointPropertyType", wkbMultiPoint },
        { "MultiPolygonPropertyType", wkbMultiPolygon },
        { "MultiLineStringPropertyType", wkbMultiLineString },
        { "MultiGeometryPropertyType", wkbGeometryCollection },
        { "MultiCurvePropertyType", wkbMultiCurve },
        { "MultiSurfacePropertyType", wkbMultiSurface },
        { "MultiSolidPropertyType", wkbUnknown },
        // GeometryArrayPropertyType ?
        // GeometricPrimitivePropertyType ?
        { "CurvePropertyType", wkbCurve },
        { "SurfacePropertyType", wkbSurface },
        // SurfaceArrayPropertyType ?
        // AbstractRingPropertyType ?
        // LinearRingPropertyType ?
        { "CompositeCurvePropertyType", wkbCurve },
        { "CompositeSurfacePropertyType", wkbSurface },
        { "CompositeSolidPropertyType", wkbUnknown },
        { "GeometricComplexPropertyType", wkbUnknown },
        { "SolidPropertyType", wkbPolyhedralSurface }
    };

    CPLString osName(transcode(poTypeDef->getName()));
    for( size_t i = 0; i < CPL_ARRAYSIZE(asArray); ++i )
    {
        if( osName == asArray[i].pszName )
            return asArray[i].eType;
    }
    return wkbNone;

#if 0
  <complexType name="CurveSegmentArrayPropertyType">
  <complexType name="KnotPropertyType">
  <complexType name="SurfacePatchArrayPropertyType">
  <complexType name="RingPropertyType">
  <complexType name="PolygonPatchArrayPropertyType">
  <complexType name="TrianglePatchArrayPropertyType">
  <complexType name="LineStringSegmentArrayPropertyType">
  <complexType name="SolidArrayPropertyType">
#endif
}


/************************************************************************/
/*                 GetOGRGeometryTypeFromGMLEltName()                   */
/************************************************************************/

static OGRwkbGeometryType GetOGRGeometryTypeFromGMLEltName(
                                                    const CPLString& osEltName )
{
    const struct MyStruct
    {
        const char* pszName;
        OGRwkbGeometryType eType;
    } asArray[] = {
        { "Point", wkbPoint },
        { "Polygon", wkbPolygon },
        { "LineString", wkbLineString },
        { "MultiPoint", wkbMultiPoint },
        { "MultiPolygon", wkbMultiPolygon },
        { "MultiLineString", wkbMultiLineString },
        { "MultiGeometry", wkbGeometryCollection },
        { "MultiCurve", wkbMultiCurve },
        { "MultiSurface", wkbMultiSurface },
        { "MultiSolid", wkbUnknown },
        { "Curve", wkbCurve },
        { "Surface", wkbSurface },
        { "CompositeCurve", wkbCurve },
        { "CompositeSurface", wkbSurface },
        { "CompositeSolid", wkbUnknown },
        { "GeometricComplex", wkbUnknown },
    };

    for( size_t i = 0; i < CPL_ARRAYSIZE(asArray); ++i )
    {
        if( osEltName == asArray[i].pszName )
            return asArray[i].eType;
    }
    return wkbNone;
}

/************************************************************************/
/*                      CreateNonNestedRelationship()                  */
/************************************************************************/

void GMLASSchemaAnalyzer::CreateNonNestedRelationship(
                        XSElementDeclaration* poElt,
                        std::vector<XSElementDeclaration*>& apoImplEltList,
                        GMLASFeatureClass& oClass,
                        int nMaxOccurs,
                        bool bEltNameWillNeedPrefix,
                        bool bForceJunctionTable,
                        bool bCaseOfConstraintChildren )
{
    const CPLString osEltPrefix(
                GetPrefix(transcode(poElt->getNamespace())));
    const CPLString osEltName(transcode(poElt->getName()));
    const CPLString osOnlyElementXPath(
                    MakeXPath(transcode(poElt->getNamespace()),
                              osEltName) );
    const CPLString osElementXPath( oClass.GetXPath() + "/" +
                                    osOnlyElementXPath );

    if( !poElt->getAbstract() && !bCaseOfConstraintChildren )
    {
        apoImplEltList.insert(apoImplEltList.begin(), poElt);
    }

    std::set<CPLString> aoSetSubEltXPath;
    if( nMaxOccurs == 1 && !bForceJunctionTable )
    {
        // If the field isn't repeated, then we can link to each
        // potential realization types with a field

        for( size_t j = 0; j < apoImplEltList.size(); j++ )
        {
            XSElementDeclaration* poSubElt = apoImplEltList[j];
            const CPLString osSubEltName(transcode(poSubElt->getName()));
            const CPLString osSubEltXPath(
                MakeXPath(transcode(poSubElt->getNamespace()),
                          osSubEltName) );

            // For AbstractFeature_SpatialDataSet_pkid in SpatialDataSet_member
            if( aoSetSubEltXPath.find(osSubEltXPath) !=
                                            aoSetSubEltXPath.end() )
            {
                continue;
            }
            aoSetSubEltXPath.insert(osSubEltXPath);

            const CPLString osRealFullXPath( oClass.GetXPath() + "/" +
                    ((bCaseOfConstraintChildren) ?
                            osOnlyElementXPath + "/" : CPLString("")) +
                                             osSubEltXPath );

            if( IsIgnoredXPath( osRealFullXPath ) )
            {
#ifdef DEBUG_VERBOSE
                CPLDebug("GMLAS", "%s is in ignored xpaths",
                         osRealFullXPath.c_str());
#endif
                continue;
            }

            GMLASField oField;
            if( apoImplEltList.size() > 1 || bCaseOfConstraintChildren )
            {
                if( m_oMapEltNamesToInstanceCount[osSubEltName] > 1  )
                {
                    CPLString osLaunderedXPath(osSubEltXPath);
                    osLaunderedXPath.replaceAll(':', '_');
                    oField.SetName(
                        ((bEltNameWillNeedPrefix) ? osEltPrefix + "_" :
                                                                CPLString()) +
                                    transcode(poElt->getName()) + "_" +
                                    osLaunderedXPath + "_pkid" );
                }
                else
                {
                    oField.SetName(
                        ((bEltNameWillNeedPrefix) ? osEltPrefix + "_" :
                                                                CPLString()) +
                            transcode(poElt->getName()) + "_" +
                                    osSubEltName + "_pkid" );
                }
            }
            else
            {
                oField.SetName( transcode(poElt->getName()) + "_pkid" );
            }
            oField.SetXPath( osRealFullXPath );
            oField.SetMinOccurs( 0 );
            oField.SetMaxOccurs( nMaxOccurs );
            oField.SetCategory( GMLASField::PATH_TO_CHILD_ELEMENT_WITH_LINK );
            oField.SetRelatedClassXPath(osSubEltXPath);
            oField.SetType( GMLAS_FT_STRING, szXS_STRING );
            oClass.AddField( oField );
        }
    }
    else
    {
        // If the field is repeated, we need to use junction
        // tables
        for( size_t j = 0; j < apoImplEltList.size(); j++ )
        {
            XSElementDeclaration* poSubElt = apoImplEltList[j];
            const CPLString osSubEltName( transcode(poSubElt->getName()) );
            const CPLString osSubEltXPath(
                MakeXPath(transcode(poSubElt->getNamespace()), osSubEltName) );

            // For AbstractFeature_SpatialDataSet_pkid in SpatialDataSet_member
            if( aoSetSubEltXPath.find(osSubEltXPath) !=
                                            aoSetSubEltXPath.end() )
            {
                continue;
            }
            aoSetSubEltXPath.insert(osSubEltXPath);

            // Instantiate a junction table
            GMLASFeatureClass oJunctionTable;

            if( m_oMapEltNamesToInstanceCount[osSubEltName] > 1 )
            {
                CPLString osLaunderedXPath(osSubEltXPath);
                osLaunderedXPath.replaceAll(':', '_');
                oJunctionTable.SetName( oClass.GetName() + "_" +
                                        transcode(poElt->getName()) + "_" +
                                        osLaunderedXPath );
            }
            else
            {
                oJunctionTable.SetName( oClass.GetName() + "_" +
                                        transcode(poElt->getName()) + "_" +
                                        osSubEltName );
            }
            // Create a fake XPath binding the parent xpath (to an abstract
            // element) to the child element
            oJunctionTable.SetXPath(
                BuildJunctionTableXPath(osElementXPath, osSubEltXPath) );
            oJunctionTable.SetParentXPath( oClass.GetXPath() );
            oJunctionTable.SetChildXPath( osSubEltXPath );
            m_aoClasses.push_back(oJunctionTable);

            // Add an abstract field
            GMLASField oField;
            oField.SetName(
                ((bEltNameWillNeedPrefix) ? osEltPrefix + "_" :
                                                                CPLString()) +
                osEltName + "_" + osSubEltName );
            oField.SetXPath( oClass.GetXPath() + "/" +
                    ((bCaseOfConstraintChildren) ?
                            osOnlyElementXPath + "/" : CPLString("")) +
                                                osSubEltXPath);
            oField.SetMinOccurs( 0 );
            oField.SetMaxOccurs( nMaxOccurs );
            oField.SetAbstractElementXPath(osElementXPath);
            oField.SetRelatedClassXPath(osSubEltXPath);
            oField.SetCategory(
                    GMLASField::PATH_TO_CHILD_ELEMENT_WITH_JUNCTION_TABLE );
            oClass.AddField( oField );
        }
    }

#if 0
    GMLASField oField;
    oField.SetName( transcode(poElt->getName()) );
    oField.SetXPath( osElementXPath );
    oField.SetMinOccurs( poParticle->getMinOccurs() );
    oField.SetMaxOccurs( poParticle->getMaxOccursUnbounded() ?
        MAXOCCURS_UNLIMITED : poParticle->getMaxOccurs() );

    for( size_t j = 0; j < apoImplEltList.size(); j++ )
    {
        XSElementDeclaration* poSubElt = apoImplEltList[j];
        XSTypeDefinition* poSubEltType =
                                    poSubElt->getTypeDefinition();
        XSComplexTypeDefinition* poCT =
            reinterpret_cast<XSComplexTypeDefinition*>(poSubEltType);

        GMLASFeatureClass oNestedClass;
        oNestedClass.SetName( oClass.GetName() + "_" +
                    transcode(poSubElt->getName()) );
        oNestedClass.SetXPath( oClass.GetXPath() + "/" +
            MakeXPath(transcode(poSubElt->getNamespace()),
                        transcode(poSubElt->getName())) );

        std::set<XSModelGroup*>
            oSetNewVisitedModelGroups(oSetVisitedModelGroups);
        if( !ExploreModelGroup(
                poCT->getParticle()->getModelGroupTerm(),
                NULL,
                oNestedClass,
                nRecursionCounter + 1,
                oSetNewVisitedModelGroups ) )
        {
            return false;
        }

        oClass.AddNestedClass( oNestedClass );
    }

    if( !apoImplEltList.empty() )
    {
        oField.SetAbstract(true);
    }
    else
    {
        oField.SetType( GMLAS_FT_ANYTYPE, "anyType" );
        oField.SetXPath( oClass.GetXPath() + "/" + "*" );
        oField.SetIncludeThisEltInBlob( true );
    }
    oClass.AddField( oField );
#endif
}

/************************************************************************/
/*                          IsIgnoredXPath()                            */
/************************************************************************/

bool GMLASSchemaAnalyzer::IsIgnoredXPath(const CPLString& osXPath)
{
    CPLString osIgnored;
    return m_oIgnoredXPathMatcher.MatchesRefXPath(osXPath, osIgnored);
}

/************************************************************************/
/*                     FindElementsWithMustBeToLevel()                  */
/************************************************************************/

bool GMLASSchemaAnalyzer::FindElementsWithMustBeToLevel(
                            const CPLString& osParentXPath,
                            XSModelGroup* poModelGroup,
                            int nRecursionCounter,
                            std::set<XSElementDeclaration*>& oSetVisitedEltDecl,
                            std::set<XSModelGroup*>& oSetVisitedModelGroups,
                            std::vector<XSElementDeclaration*>&
                                                        oVectorEltsForTopClass,
                            std::set<CPLString>& aoSetXPathEltsForTopClass,
                            XSModel* poModel,
                            bool& bSimpleEnoughOut,
                            int& nCountSubEltsOut )
{
    const bool bAlreadyVisitedMG =
            ( oSetVisitedModelGroups.find(poModelGroup) !=
                                                oSetVisitedModelGroups.end() );

    oSetVisitedModelGroups.insert(poModelGroup);

    if( nRecursionCounter == 100 )
    {
        // Presumably an hostile schema
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Schema analysis failed due to too deeply nested model");
        return false;
    }

    {
        CPLString osIgnored;
        if( m_oDisabledFlattenedXPathMatcher.MatchesRefXPath(
                                            osParentXPath, osIgnored))
        {
            bSimpleEnoughOut = false;
        }
    }

    XSParticleList* poParticles = poModelGroup->getParticles();
    for(size_t i = 0; i < poParticles->size(); ++i )
    {
        XSParticle* poParticle = poParticles->elementAt(i);

        const bool bRepeatedParticle = poParticle->getMaxOccursUnbounded() ||
                                        poParticle->getMaxOccurs() > 1;

        if( poParticle->getTermType() == XSParticle::TERM_ELEMENT )
        {
            XSElementDeclaration* poElt = poParticle->getElementTerm();
            XSTypeDefinition* poTypeDef = poElt->getTypeDefinition();
            const CPLString osEltName(transcode(poElt->getName()));
            const CPLString osEltNS(transcode(poElt->getNamespace()));
            const CPLString osXPath( MakeXPath(osEltNS, osEltName) );
            const CPLString osFullXPath( osParentXPath + "/" + osXPath );

#ifdef DEBUG_SUPER_VERBOSE
            CPLDebug("GMLAS", "FindElementsWithMustBeToLevel: %s",
                     osFullXPath.c_str());
#endif

            if( IsIgnoredXPath( osFullXPath ) )
            {
#ifdef DEBUG_VERBOSE
                CPLDebug("GMLAS", "%s is in ignored xpaths",
                         osFullXPath.c_str());
#endif
                continue;
            }

            // This could be refined to detect if the repeated element might not
            // be simplifiable as an array
            if( bSimpleEnoughOut && bRepeatedParticle )
            {
#ifdef DEBUG_VERBOSE
                CPLDebug("GMLAS",
                         "%s not inlinable because %s is repeated",
                         osParentXPath.c_str(),
                         osXPath.c_str()
                        );
#endif
                bSimpleEnoughOut = false;
            }

            // We don't want to inline
            // sub-classes with hundereds of attributes
            nCountSubEltsOut ++;

            std::vector<XSElementDeclaration*> apoImplEltList;
            GetConcreteImplementationTypes(poElt, apoImplEltList);

            std::vector<XSElementDeclaration*> apoChildrenElements =
                GetConstraintChildrenElements(osFullXPath);

            // Special case for a GML geometry property
            if( IsGMLNamespace(transcode(poTypeDef->getNamespace())) &&
                GetOGRGeometryType(poTypeDef) != wkbNone )
            {
                // Do nothing
            }
            else if( IsGMLNamespace(osEltNS) &&
                     GetOGRGeometryTypeFromGMLEltName(osEltName) != wkbNone )
            {
                // Do nothing
            }
            // Any GML abstract type
            else if( poElt->getAbstract() &&
                     IsGMLNamespace(osEltNS) &&
                     osEltName != "_Feature" &&
                     osEltName != "AbstractFeature" &&
                     osEltName != "AbstractTimeObject" )
            {
                // Do nothing
            }
            // Are there substitution groups for this element ?
            else if( !apoImplEltList.empty() || !apoChildrenElements.empty() )
            {
                if( !apoChildrenElements.empty() )
                {
                    apoImplEltList = apoChildrenElements;
                }
                else if( !poElt->getAbstract() )
                {
                    apoImplEltList.insert(apoImplEltList.begin(), poElt);
                }
                for( size_t j = 0; j < apoImplEltList.size(); j++ )
                {
                    XSElementDeclaration* poSubElt = apoImplEltList[j];
                    const CPLString osSubEltXPath(
                        MakeXPath(transcode(poSubElt->getNamespace()),
                                    transcode(poSubElt->getName())) );

                    if( IsIgnoredXPath( osParentXPath + "/" + osSubEltXPath ) )
                    {
#ifdef DEBUG_VERBOSE
                        CPLDebug("GMLAS", "%s is in ignored xpaths",
                                 (osParentXPath + "/" + osSubEltXPath).c_str());
#endif
                        continue;
                    }

                    // Make sure we will instantiate the referenced element
                    if( m_oSetEltsForTopClass.find( poSubElt ) ==
                                m_oSetEltsForTopClass.end() &&
                        aoSetXPathEltsForTopClass.find( osSubEltXPath )
                                == aoSetXPathEltsForTopClass.end() )
                    {
#ifdef DEBUG_VERBOSE
                        CPLDebug("GMLAS", "%s (%s) must be exposed as "
                                     "top-level (%s of %s)",
                                osSubEltXPath.c_str(),
                                transcode(poSubElt->getTypeDefinition()->
                                                            getName()).c_str(),
                                apoChildrenElements.empty() ?
                                                "derived class" : "child",
                                osParentXPath.c_str()
                                );
#endif

                        oSetVisitedEltDecl.insert(poSubElt);
                        m_oSetEltsForTopClass.insert(poSubElt);
                        oVectorEltsForTopClass.push_back(poSubElt);
                        aoSetXPathEltsForTopClass.insert( osSubEltXPath );

                        XSComplexTypeDefinition* poSubEltCT =
                                            IsEltCompatibleOfFC(poSubElt);
                        if( !bAlreadyVisitedMG &&
                            poSubEltCT != NULL &&
                            poSubEltCT->getParticle() != NULL )
                        {
                            bool bSubSimpleEnoughOut = true;
                            int nSubCountSubElt = 0;
                            if( !FindElementsWithMustBeToLevel(
                                            osSubEltXPath,
                                            poSubEltCT->getParticle()->
                                                        getModelGroupTerm(),
                                            nRecursionCounter + 1,
                                            oSetVisitedEltDecl,
                                            oSetVisitedModelGroups,
                                            oVectorEltsForTopClass,
                                            aoSetXPathEltsForTopClass,
                                            poModel,
                                            bSubSimpleEnoughOut,
                                            nSubCountSubElt ) )
                            {
                                return false;
                            }
                        }
                    }
                }
            }

            else if( !poElt->getAbstract() &&
                poTypeDef->getTypeCategory() == XSTypeDefinition::COMPLEX_TYPE )
            {
                nCountSubEltsOut --;

                XSComplexTypeDefinition* poEltCT = IsEltCompatibleOfFC(poElt);
                if( poEltCT )
                {
                    // Might be a bit extreme, but for now we don't inline
                    // classes that have subclasses.
                    if( bSimpleEnoughOut )
                    {
#ifdef DEBUG_VERBOSE
                        CPLDebug("GMLAS",
                                  "%s not inlinable because %s field is complex",
                                  osParentXPath.c_str(),
                                 osXPath.c_str());
#endif
                        bSimpleEnoughOut = false;
                    }

                    if( oSetVisitedEltDecl.find(poElt) !=
                                    oSetVisitedEltDecl.end() )
                    {
                        if( m_oSetEltsForTopClass.find(poElt) ==
                                                m_oSetEltsForTopClass.end() &&
                            m_oSetSimpleEnoughElts.find(poElt) ==
                                        m_oSetSimpleEnoughElts.end() &&
                            aoSetXPathEltsForTopClass.find( osXPath )
                                == aoSetXPathEltsForTopClass.end() )
                        {
                            CPLString osIgnored;
                            if( !m_oForcedFlattenedXPathMatcher.MatchesRefXPath(
                                                        osXPath, osIgnored))
                            {
#ifdef DEBUG_VERBOSE
                                CPLDebug("GMLAS", "%s (%s) must be exposed as "
                                        "top-level (multiple time referenced)",
                                        osXPath.c_str(),
                                        transcode(
                                            poTypeDef->getNamespace()).c_str());
#endif
                                m_oSetEltsForTopClass.insert(poElt);
                                oVectorEltsForTopClass.push_back(poElt);
                                aoSetXPathEltsForTopClass.insert( osXPath );
                            }
                        }
                    }
                    else
                    {
                        oSetVisitedEltDecl.insert(poElt);

                        if( !bAlreadyVisitedMG &&
                            poEltCT->getParticle() != NULL )
                        {
                            bool bSubSimpleEnoughOut = true;
                            int nSubCountSubElt = 0;

                            // Process attributes
                            XSAttributeUseList* poAttrList =
                                                    poEltCT->getAttributeUses();
                            const size_t nAttrListSize = (poAttrList != NULL) ?
                                                                poAttrList->size(): 0;
                            for(size_t j=0; bSubSimpleEnoughOut && j< nAttrListSize; ++j )
                            {
                                XSAttributeUse* poAttr = poAttrList->elementAt(j);
                                GMLASField oField;
                                SetFieldFromAttribute(oField, poAttr, osFullXPath);
                                if( !IsIgnoredXPath( oField.GetXPath() ) &&
                                    oField.GetFixedValue().empty() )
                                {
#ifdef DEBUG_SUPER_VERBOSE
                                    CPLDebug("GMLAS", "FindElementsWithMustBeToLevel: %s",
                                             oField.GetXPath().c_str());
#endif
                                    nSubCountSubElt ++;
                                }
                            }

                            if( !FindElementsWithMustBeToLevel(
                                            osFullXPath,
                                            poEltCT->getParticle()->
                                                            getModelGroupTerm(),
                                            nRecursionCounter + 1,
                                            oSetVisitedEltDecl,
                                            oSetVisitedModelGroups,
                                            oVectorEltsForTopClass,
                                            aoSetXPathEltsForTopClass,
                                            poModel,
                                            bSubSimpleEnoughOut,
                                            nSubCountSubElt ) )
                            {
                                return false;
                            }
                            if( bSubSimpleEnoughOut )
                            {
#ifdef DEBUG_VERBOSE
                                CPLDebug("GMLAS",
                                    "%s is inlinable: %d fields",
                                    osXPath.c_str(),
                                    nSubCountSubElt
                                    );
#endif
                                m_oSetSimpleEnoughElts.insert(poElt);

                                nCountSubEltsOut += nSubCountSubElt;
                            }
                            else if( bSimpleEnoughOut )
                            {
#ifdef DEBUG_VERBOSE
                                CPLDebug("GMLAS",
                                    "%s not inlinable because %s is not inlinable",
                                    osParentXPath.c_str(),
                                    osXPath.c_str()
                                    );
#endif
                                bSimpleEnoughOut = false;
                            }
                        }
                    }
                }
                else
                {
                    if( transcode(poElt->getName()) != szFEATURE_COLLECTION )
                    {
                        poEltCT =
                            reinterpret_cast<XSComplexTypeDefinition*>(poTypeDef);
// Process attributes
                        XSAttributeUseList* poAttrList =
                                                poEltCT->getAttributeUses();
                        const size_t nAttrListSize = (poAttrList != NULL) ?
                                                            poAttrList->size(): 0;
                        for(size_t j=0; bSimpleEnoughOut && j< nAttrListSize; ++j )
                        {
                            XSAttributeUse* poAttr = poAttrList->elementAt(j);
                            GMLASField oField;
                            SetFieldFromAttribute(oField, poAttr, osFullXPath);
                            if( !IsIgnoredXPath( oField.GetXPath() ) &&
                                oField.GetFixedValue().empty() )
                            {
#ifdef DEBUG_SUPER_VERBOSE
                                CPLDebug("GMLAS", "FindElementsWithMustBeToLevel: %s",
                                            oField.GetXPath().c_str());
#endif
                                nCountSubEltsOut ++;
                            }
                        }
                    }
                }

                CPLString osTargetElement;
                if( poElt->getAnnotation() != NULL )
                {
                    CPLString osAnnot(transcode(
                        poElt->getAnnotation()->getAnnotationString()));

#ifdef DEBUG_SUPER_VERBOSE
                    CPLDebug("GMLAS", "Annot: %s", osAnnot.c_str());
#endif
                    CPLXMLNode* psRoot = CPLParseXMLString(osAnnot);
                    CPLStripXMLNamespace(psRoot, NULL, TRUE);
                    osTargetElement =
                        CPLGetXMLValue(psRoot,
                                       "=annotation.appinfo.targetElement", "");
                    CPLDestroyXMLNode(psRoot);
#ifdef DEBUG_VERBOSE
                    if( !osTargetElement.empty() )
                        CPLDebug("GMLAS", "targetElement: %s",
                                osTargetElement.c_str());
#endif
                }

                // If we have a element of type gml:ReferenceType that has
                // a targetElement in its annotation.appinfo, then create
                // a dedicated field to have cross-layer relationships.
                if( IsGMLNamespace(transcode(poTypeDef->getNamespace())) &&
                    transcode(poTypeDef->getName()) == "ReferenceType" &&
                    !osTargetElement.empty() )
                {
                    XSElementDeclaration* poTargetElt =
                        GetTopElementDeclarationFromXPath(osTargetElement,
                                                            poModel);
                    // TODO: even for non abstract we should probably
                    // handle substitutions
                    if( poTargetElt != NULL && !poTargetElt->getAbstract() )
                    {
                        const CPLString osTargetEltXPath( MakeXPath(
                                    transcode(poTargetElt->getNamespace()),
                                    transcode(poTargetElt->getName())) );

                        if( IsIgnoredXPath( osTargetEltXPath ) )
                        {
#ifdef DEBUG_VERBOSE
                            CPLDebug("GMLAS", "%s is in ignored xpaths",
                                    osTargetEltXPath.c_str());
#endif
                            continue;
                        }

                        // Make sure we will instantiate the referenced
                        //element
                        if( m_oSetEltsForTopClass.find( poTargetElt ) ==
                                    m_oSetEltsForTopClass.end() &&
                            aoSetXPathEltsForTopClass.find( osTargetEltXPath )
                                == aoSetXPathEltsForTopClass.end() )
                        {
#ifdef DEBUG_VERBOSE
                            CPLDebug("GMLAS",
                                        "%d: Adding %s as (%s) needed type",
                                        __LINE__,
                                    osTargetElement.c_str(),
                                    transcode(poTargetElt->
                                                getTypeDefinition()->
                                                    getName()).c_str());
#endif
                            oSetVisitedEltDecl.insert(poTargetElt);
                            m_oSetEltsForTopClass.insert( poTargetElt );
                            oVectorEltsForTopClass.push_back(poTargetElt);
                            aoSetXPathEltsForTopClass.insert( osTargetEltXPath );
                        }

                        XSComplexTypeDefinition* poTargetEltCT =
                                    IsEltCompatibleOfFC(poTargetElt);
                        if( !bAlreadyVisitedMG &&
                            poTargetEltCT &&
                            poTargetEltCT->getParticle() != NULL )
                        {
                            bool bSubSimpleEnoughOut = true;
                            int nSubCountSubElt = 0;
                            if( !FindElementsWithMustBeToLevel(
                                            osTargetEltXPath,
                                            poTargetEltCT->getParticle()->
                                                            getModelGroupTerm(),
                                            nRecursionCounter + 1,
                                            oSetVisitedEltDecl,
                                            oSetVisitedModelGroups,
                                            oVectorEltsForTopClass,
                                            aoSetXPathEltsForTopClass,
                                            poModel,
                                            bSubSimpleEnoughOut,
                                            nSubCountSubElt) )
                            {
                                return false;
                            }
                        }
                    }
                }
            }
        }
        else if( !bAlreadyVisitedMG &&
                 poParticle->getTermType() == XSParticle::TERM_MODELGROUP )
        {
            // This could be refined to detect if the repeated element might not
            // be simplifiable as an array
            if( bSimpleEnoughOut && bRepeatedParticle )
            {
#ifdef DEBUG_VERBOSE
                CPLDebug("GMLAS",
                        "%s not inlinable because there is a repeated particle",
                        osParentXPath.c_str());
#endif
                bSimpleEnoughOut = false;
            }

            XSModelGroup* psSubModelGroup = poParticle->getModelGroupTerm();
            if( !FindElementsWithMustBeToLevel(
                                    osParentXPath,
                                    psSubModelGroup,
                                    nRecursionCounter + 1,
                                    oSetVisitedEltDecl,
                                    oSetVisitedModelGroups,
                                    oVectorEltsForTopClass,
                                    aoSetXPathEltsForTopClass,
                                    poModel,
                                    bSimpleEnoughOut,
                                    nCountSubEltsOut) )
            {
                return false;
            }
        }
        else
        {
            // This could be refined to detect if the repeated element might not
            // be simplifiable as an array
            if( bSimpleEnoughOut && bRepeatedParticle )
            {
#ifdef DEBUG_VERBOSE
                CPLDebug("GMLAS",
                        "%s not inlinable because there is a repeated particle",
                        osParentXPath.c_str());
#endif
                bSimpleEnoughOut = false;
            }
        }
    }

    if( bSimpleEnoughOut &&
        nCountSubEltsOut > m_nMaximumFieldsForFlattening )
    {
        CPLString osIgnored;
        if( !m_oForcedFlattenedXPathMatcher.MatchesRefXPath(
                                                osParentXPath, osIgnored))
        {
#ifdef DEBUG_VERBOSE
            CPLDebug("GMLAS",
                        "%s not inlinable because it has more than %d fields",
                        osParentXPath.c_str(),
                        m_nMaximumFieldsForFlattening);
#endif
            bSimpleEnoughOut = false;
        }
    }

    return true;
}

/************************************************************************/
/*                           IsGMLNamespace()                           */
/************************************************************************/

bool GMLASSchemaAnalyzer::IsGMLNamespace(const CPLString& osURI)
{
    if( osURI.find(szGML_URI) == 0 )
        return true;
    // Below is mostly for unit tests were we use xmlns:gml="http://fake_gml"
    std::map<CPLString,CPLString>::const_iterator oIter =
                                        m_oMapURIToPrefix.find(osURI);
    return oIter != m_oMapURIToPrefix.end() && oIter->second == szGML_PREFIX;
}

/************************************************************************/
/*                    BuildMapCountOccurrencesOfSameName()               */
/************************************************************************/

void GMLASSchemaAnalyzer::BuildMapCountOccurrencesOfSameName(
                    XSModelGroup* poModelGroup,
                    std::map< CPLString, int >& oMapCountOccurrencesOfSameName)
{
    XSParticleList* poParticles = poModelGroup->getParticles();
    for(size_t i = 0; i < poParticles->size(); ++i )
    {
        XSParticle* poParticle = poParticles->elementAt(i);
        if( poParticle->getTermType() == XSParticle::TERM_ELEMENT )
        {
            XSElementDeclaration* poElt = poParticle->getElementTerm();
            const CPLString osEltName(transcode(poElt->getName()));
            oMapCountOccurrencesOfSameName[ osEltName ] ++;
        }
        else if( poParticle->getTermType() == XSParticle::TERM_MODELGROUP )
        {
            XSModelGroup* psSubModelGroup = poParticle->getModelGroupTerm();
            BuildMapCountOccurrencesOfSameName(psSubModelGroup,
                                              oMapCountOccurrencesOfSameName);
        }
    }
}

/************************************************************************/
/*                         ComposeMinOccurs()                           */
/************************************************************************/

static int ComposeMinOccurs(int nVal1, int nVal2)
{
    return nVal1 * nVal2;
}

/************************************************************************/
/*                         ComposeMaxOccurs()                           */
/************************************************************************/

static int ComposeMaxOccurs(int nVal1, int nVal2)
{
    if( nVal1 == MAXOCCURS_UNLIMITED || nVal2 == MAXOCCURS_UNLIMITED )
        return MAXOCCURS_UNLIMITED;
    return nVal1 * nVal2;
}

/************************************************************************/
/*                         ExploreModelGroup()                          */
/************************************************************************/

bool GMLASSchemaAnalyzer::ExploreModelGroup(
                            XSModelGroup* poModelGroup,
                            XSAttributeUseList* poMainAttrList,
                            GMLASFeatureClass& oClass,
                            int nRecursionCounter,
                            std::set<XSModelGroup*>& oSetVisitedModelGroups,
                            XSModel* poModel,
                            const std::map< CPLString, int >& oMapCountOccurrencesOfSameName)
{
    if( oSetVisitedModelGroups.find(poModelGroup) !=
                                                oSetVisitedModelGroups.end() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s already visited",
                 oClass.GetXPath().c_str());
        return false;
    }
    oSetVisitedModelGroups.insert(poModelGroup);

    if( nRecursionCounter == 100 )
    {
        // Presumably an hostile schema
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Schema analysis failed due to too deeply nested model");
        return false;
    }

    if( poMainAttrList != NULL )
    {
        const size_t nMainAttrListSize = poMainAttrList->size();
        for(size_t j=0; j < nMainAttrListSize; ++j )
        {
            GMLASField oField;
            XSAttributeUse* poAttr = poMainAttrList->elementAt(j);
            SetFieldFromAttribute(oField, poAttr, oClass.GetXPath());

            if( IsIgnoredXPath( oField.GetXPath() ) )
            {
#ifdef DEBUG_VERBOSE
                CPLDebug("GMLAS", "%s is in ignored xpaths",
                        oField.GetXPath().c_str());
#endif
                if( !oField.GetFixedValue().empty() ||
                    !oField.GetDefaultValue().empty() )
                {
                    oField.SetIgnored();
                }
                else
                {
                    continue;
                }
            }

            oClass.AddField(oField);
        }
    }

    XSParticleList* poParticles = poModelGroup->getParticles();

    // Special case for GML 3.1.1 where gml:metaDataProperty should be
    // a sequence of gml:_Metadata but for some reason they have used
    // a sequence of any.
    if( oClass.GetXPath() == "gml:metaDataProperty" &&
        poModelGroup->getCompositor() ==
                                XSModelGroup::COMPOSITOR_SEQUENCE &&
        poParticles->size() == 1 &&
        poParticles->elementAt(0)->
                        getTermType() == XSParticle::TERM_WILDCARD )
    {
        XSElementDeclaration* poGMLMetadata =
            GetTopElementDeclarationFromXPath("gml:_MetaData", poModel);
        if( poGMLMetadata != NULL )
        {
            std::vector<XSElementDeclaration*> apoImplEltList;
            GetConcreteImplementationTypes(poGMLMetadata, apoImplEltList);
            CreateNonNestedRelationship(poGMLMetadata,
                                        apoImplEltList,
                                        oClass,
                                        1,
                                        false, // doesn't need prefix
                                        true, // force junction table
                                        false // regular case
                                       );

            return true;
        }
    }

    const bool bIsChoice = (poModelGroup->getCompositor() ==
                                            XSModelGroup::COMPOSITOR_CHOICE);
    int nGroup = 0;

    for(size_t i = 0; i < poParticles->size(); ++i )
    {
        XSParticle* poParticle = poParticles->elementAt(i);
        const bool bRepeatedParticle = poParticle->getMaxOccursUnbounded() ||
                                       poParticle->getMaxOccurs() > 1;
        const int nMinOccurs = static_cast<int>(poParticle->getMinOccurs());
        const int nMaxOccurs =
                        poParticle->getMaxOccursUnbounded() ?
                            MAXOCCURS_UNLIMITED :
                            static_cast<int>(poParticle->getMaxOccurs());

        if( poParticle->getTermType() == XSParticle::TERM_ELEMENT )
        {
            XSElementDeclaration* poElt = poParticle->getElementTerm();
            const CPLString osEltName(transcode(poElt->getName()));

            std::map< CPLString, int >::const_iterator oIter =
                oMapCountOccurrencesOfSameName.find(osEltName);
            const bool bEltNameWillNeedPrefix =
                oIter != oMapCountOccurrencesOfSameName.end() &&
                oIter->second > 1;
            const CPLString osEltNS(transcode(poElt->getNamespace()));
            const CPLString osPrefixedEltName(
                (bEltNameWillNeedPrefix ? GetPrefix(osEltNS) + "_" : CPLString()) +
                osEltName);
            const CPLString osOnlyElementXPath(MakeXPath(osEltNS, osEltName));
            const CPLString osElementXPath( oClass.GetXPath() + "/" +
                                            osOnlyElementXPath );
#ifdef DEBUG_VERBOSE
            CPLDebug("GMLAS", "Iterating through %s", osElementXPath.c_str());
#endif

            if( IsIgnoredXPath( osElementXPath ) )
            {
#ifdef DEBUG_VERBOSE
                CPLDebug("GMLAS", "%s is in ignored xpaths",
                         osElementXPath.c_str());
#endif
                continue;
            }

            CPLString osTargetElement;
            if( poElt->getAnnotation() != NULL )
            {
                CPLString osAnnot(transcode(
                    poElt->getAnnotation()->getAnnotationString()));

#ifdef DEBUG_SUPER_VERBOSE
                CPLDebug("GMLAS", "Annot: %s", osAnnot.c_str());
#endif
                CPLXMLNode* psRoot = CPLParseXMLString(osAnnot);
                CPLStripXMLNamespace(psRoot, NULL, TRUE);
                osTargetElement =
                    CPLGetXMLValue(psRoot, "=annotation.appinfo.targetElement", "");
                CPLDestroyXMLNode(psRoot);
#ifdef DEBUG_VERBOSE
                if( !osTargetElement.empty() )
                    CPLDebug("GMLAS", "targetElement: %s",
                             osTargetElement.c_str());
#endif
            }

            XSTypeDefinition* poTypeDef = poElt->getTypeDefinition();

            std::vector<XSElementDeclaration*> apoImplEltList;
            GetConcreteImplementationTypes(poElt, apoImplEltList);

            std::vector<XSElementDeclaration*> apoChildrenElements =
                GetConstraintChildrenElements(osElementXPath);

            // Special case for a GML geometry property
            OGRwkbGeometryType eGeomType = wkbNone;

            if( !apoChildrenElements.empty() )
            {
                CreateNonNestedRelationship(poElt,
                                            apoChildrenElements,
                                            oClass,
                                            nMaxOccurs,
                                            bEltNameWillNeedPrefix,
                                            false, // do not force junction table
                                            true // special case for children elements
                                           );
            }

            else if( IsGMLNamespace(transcode(poTypeDef->getNamespace())) &&
                (eGeomType = GetOGRGeometryType(poTypeDef)) != wkbNone )
            {
                GMLASField oField;
                oField.SetName( osPrefixedEltName );
                oField.SetMinOccurs( nMinOccurs );
                oField.SetMaxOccurs( nMaxOccurs );
                oField.SetType( GMLAS_FT_GEOMETRY, szFAKEXS_GEOMETRY );
                if( nMaxOccurs > 1 || nMaxOccurs == MAXOCCURS_UNLIMITED )
                {
                    // Repeated geometry property can happen in some schemas
                    // like inspire.ec.europa.eu/schemas/ge_gp/4.0/GeophysicsCore.xsd
                    // or http://ngwd-bdnes.cits.nrcan.gc.ca/service/gwml/schemas/2.1/gwml2-flow.xsd
                    oField.SetGeomType( wkbUnknown );
                    oField.SetArray( true );
                }
                else
                    oField.SetGeomType( eGeomType );
                oField.SetXPath( osElementXPath );
                oField.SetDocumentation( GetAnnotationDoc( poElt ) );

                oClass.AddField( oField );
            }

            else if( IsGMLNamespace(osEltNS) &&
                     (eGeomType = GetOGRGeometryTypeFromGMLEltName(osEltName))
                                                                    != wkbNone )
            {
                GMLASField oField;
                oField.SetName( osPrefixedEltName );
                oField.SetMinOccurs( nMinOccurs );
                oField.SetMaxOccurs( nMaxOccurs );

                oField.SetType( GMLAS_FT_GEOMETRY, szFAKEXS_GEOMETRY );
                oField.SetGeomType( eGeomType );
                oField.SetArray( nMaxOccurs > 1 ||
                                    nMaxOccurs == MAXOCCURS_UNLIMITED );

                oField.SetXPath( osElementXPath );
                oField.SetIncludeThisEltInBlob( true );
                oField.SetDocumentation( GetAnnotationDoc( poElt ) );

                oClass.AddField( oField );
            }

            // Any GML abstract type
            else if( poElt->getAbstract() &&
                     IsGMLNamespace(osEltNS) &&
                     osEltName != "_Feature" &&
                     osEltName != "AbstractFeature" &&
                     osEltName != "AbstractTimeObject" )
            {
                GMLASField oField;
                oField.SetName( osPrefixedEltName );
                oField.SetMinOccurs( nMinOccurs );
                oField.SetMaxOccurs( nMaxOccurs );
                if( osEltName == "AbstractGeometry" )
                {
                    oField.SetType( GMLAS_FT_GEOMETRY, szFAKEXS_GEOMETRY );
                    oField.SetGeomType( wkbUnknown );
                    oField.SetArray( nMaxOccurs > 1 ||
                                     nMaxOccurs == MAXOCCURS_UNLIMITED );
                }
                else
                {
                    oField.SetType( GMLAS_FT_ANYTYPE, szXS_ANY_TYPE );
                }
                oField.SetIncludeThisEltInBlob( true );
                oField.SetDocumentation( GetAnnotationDoc( poElt ) );

                for( size_t j = 0; j < apoImplEltList.size(); j++ )
                {
                    XSElementDeclaration* poSubElt = apoImplEltList[j];
                    oField.AddAlternateXPath( oClass.GetXPath() + "/" +
                         MakeXPath(transcode(poSubElt->getNamespace()),
                                   transcode(poSubElt->getName())) );
                }

                oClass.AddField( oField );
            }

            // Are there substitution groups for this element ?
            // or is this element already identified as being a top-level one ?
            else if( !apoImplEltList.empty() ||
                     (m_oSetEltsForTopClass.find(poElt) !=
                                                m_oSetEltsForTopClass.end() &&
                      m_oSetSimpleEnoughElts.find(poElt) ==
                                                m_oSetSimpleEnoughElts.end()) )
            {
                CreateNonNestedRelationship(poElt,
                                            apoImplEltList,
                                            oClass,
                                            nMaxOccurs,
                                            bEltNameWillNeedPrefix,
                                            false, // do not force junction table
                                            false // regular case
                                           );
            }

            // Abstract element without realizations !
            else if ( poElt->getAbstract() )
            {
                // Do nothing with it since it cannot be instantiated
                // in a valid way.
                CPLDebug("GMLAS",
                         "Ignoring %s that is abstract without realizations",
                         osElementXPath.c_str());
            }

            // Simple type like string, int, etc...
            else
            if( poTypeDef->getTypeCategory() == XSTypeDefinition::SIMPLE_TYPE )
            {
                XSSimpleTypeDefinition* poST =
                            reinterpret_cast<XSSimpleTypeDefinition*>(poTypeDef);
                GMLASField oField;
                SetFieldTypeAndWidthFromDefinition(poST, oField);
                oField.SetMinOccurs( (bIsChoice) ? 0 : nMinOccurs );
                oField.SetMaxOccurs( nMaxOccurs );
                oField.SetDocumentation( GetAnnotationDoc( poElt ) );

                bool bNeedAuxTable = false;
                const bool bIsList =
                    ( poST->getVariety() == XSSimpleTypeDefinition::VARIETY_LIST );
                if( bIsList )
                {
                    SetFieldTypeAndWidthFromDefinition(poST->getItemType(),
                                                       oField);
                    if( bRepeatedParticle || !m_bUseArrays ||
                        !IsCompatibleOfArray(oField.GetType()) )
                    {
                        // Really particular case. This is a workaround
                        oField.SetType( GMLAS_FT_STRING, szXS_STRING );
                    }
                    else
                    {
                        oField.SetList( true );
                        oField.SetArray( true );
                    }
                }

                if( m_bUseArrays && bRepeatedParticle &&
                    IsCompatibleOfArray(oField.GetType()) )
                {
                    oField.SetArray( true );
                }
                else if( bRepeatedParticle )
                {
                    bNeedAuxTable = true;
                }
                if( bNeedAuxTable )
                {
                    GMLASFeatureClass oNestedClass;
                    oNestedClass.SetName( oClass.GetName() + "_" +
                                          osPrefixedEltName );
                    oNestedClass.SetXPath( osElementXPath );
                    GMLASField oUniqueField;
                    oUniqueField.SetName("value");
                    oUniqueField.SetMinOccurs( 1 );
                    oUniqueField.SetMaxOccurs( 1 );
                    oUniqueField.SetXPath( osElementXPath );
                    oUniqueField.SetType( oField.GetType(),
                                          oField.GetTypeName() );
                    oNestedClass.AddField(oUniqueField);
                    oNestedClass.SetDocumentation( GetAnnotationDoc( poElt ) );

                    oClass.AddNestedClass( oNestedClass );

                    oField.SetType( GMLAS_FT_STRING, "" );
                    oField.SetName( osPrefixedEltName );
                    oField.SetXPath( osElementXPath );
                    oField.SetCategory(
                                    GMLASField::PATH_TO_CHILD_ELEMENT_NO_LINK);
                    oField.SetRelatedClassXPath( oField.GetXPath() );
                    oClass.AddField(oField);
                }
                else
                {
                    oField.SetName( osPrefixedEltName );
                    oField.SetXPath( osElementXPath );
                    if( !bIsChoice && nMinOccurs > 0 &&
                        !poElt->getNillable() )
                    {
                        oField.SetNotNullable( true );
                    }
                    oClass.AddField(oField);

                    // If the element has minOccurs=0 and is nillable, then we
                    // need an extra field to be able to distinguish between the
                    // case of the missing element or the element with
                    // xsi:nil="true"
                    if( nMinOccurs == 0 && poElt->getNillable() && !m_bUseNullState )
                    {
                        GMLASField oFieldNil;
                        oFieldNil.SetName( osPrefixedEltName + "_" + szNIL );
                        oFieldNil.SetXPath( osElementXPath + "/" +
                                            szAT_XSI_NIL );
                        oFieldNil.SetType( GMLAS_FT_BOOLEAN, "boolean" );
                        oFieldNil.SetMinOccurs( 0 );
                        oFieldNil.SetMaxOccurs( 1 );
                        oClass.AddField(oFieldNil);
                    }
                }
            }

            // Complex type (element with attributes, composed element, etc...)
            else if( poTypeDef->getTypeCategory() == XSTypeDefinition::COMPLEX_TYPE )
            {
                XSComplexTypeDefinition* poEltCT =
                        reinterpret_cast<XSComplexTypeDefinition*>(poTypeDef);
                std::vector< GMLASField > aoFields;
                bool bNothingMoreToDo = false;
                std::vector<GMLASFeatureClass> aoNestedClasses;

                const int nMinOccursEltParticle = poEltCT->getParticle() ?
                  static_cast<int>(poEltCT->getParticle()->getMinOccurs()) : -1;
                const int nMaxOccursEltParticle = poEltCT->getParticle() ?
                    (poEltCT->getParticle()->getMaxOccursUnbounded() ?
                        MAXOCCURS_UNLIMITED :
                        static_cast<int>(poEltCT->getParticle()->getMaxOccurs()))
                    : -1;

                const bool bEltRepeatedParticle = nMaxOccursEltParticle > 1 ||
                                nMaxOccursEltParticle == MAXOCCURS_UNLIMITED;
                const bool bMoveNestedClassToTop =
                        !bRepeatedParticle && !bEltRepeatedParticle;

                // Process attributes
                XSAttributeUseList* poAttrList =
                                        poEltCT->getAttributeUses();
                const size_t nAttrListSize = (poAttrList != NULL) ?
                                                    poAttrList->size(): 0;
                for(size_t j=0; j< nAttrListSize; ++j )
                {
                    XSAttributeUse* poAttr = poAttrList->elementAt(j);
                    GMLASField oField;
                    CPLString osNamePrefix( bMoveNestedClassToTop ?
                        osPrefixedEltName : CPLString() );
                    SetFieldFromAttribute(oField, poAttr,
                                          osElementXPath,
                                          osNamePrefix);
                    if( nMinOccurs == 0 || bIsChoice )
                    {
                        oField.SetMinOccurs(0);
                        oField.SetNotNullable(false);
                    }

                    if( IsIgnoredXPath( oField.GetXPath() ) )
                    {
#ifdef DEBUG_VERBOSE
                        CPLDebug("GMLAS", "%s is in ignored xpaths",
                                 oField.GetXPath().c_str());
#endif
                        if( !oField.GetFixedValue().empty() ||
                            !oField.GetDefaultValue().empty() )
                        {
                            oField.SetIgnored();
                        }
                        else
                        {
                            continue;
                        }
                    }

                    aoFields.push_back(oField);
                }

                // Deal with anyAttributes (or any element that also imply it)
                XSWildcard* poAttrWildcard = poEltCT->getAttributeWildcard();
                if( poAttrWildcard != NULL )
                {
                    GMLASField oField;
                    oField.SetType( GMLASField::GetTypeFromString(szXS_STRING),
                                    szFAKEXS_JSON_DICT );
                    if( !bMoveNestedClassToTop )
                    {
                        oField.SetName( "anyAttributes" );
                    }
                    else
                    {
                        oField.SetName( osPrefixedEltName + "_anyAttributes" );
                    }
                    oField.SetXPath(  osElementXPath + "/" + szAT_ANY_ATTR );
                    oField.SetDocumentation( GetAnnotationDoc(
                                            poAttrWildcard->getAnnotation() ) );

                    aoFields.push_back(oField);
                }

                XSSimpleTypeDefinition* poST = poEltCT->getSimpleType();
                if( poST != NULL )
                {
                    /* Case of an element, generally with attributes */

                    GMLASField oField;
                    SetFieldTypeAndWidthFromDefinition(poST, oField);
                    if( bRepeatedParticle && nAttrListSize == 0 &&
                        m_bUseArrays &&
                        IsCompatibleOfArray(oField.GetType()) &&
                        oField.GetCategory() !=
                                GMLASField::PATH_TO_CHILD_ELEMENT_WITH_LINK )
                    {
                        /* We have a complex type, but no attributes, and */
                        /* compatible of arrays, so move it to top level! */
                        oField.SetName( osPrefixedEltName );
                        oField.SetArray( true );
                        oField.SetMinOccurs( nMinOccurs );
                        oField.SetMaxOccurs( nMaxOccurs );
                    }
                    else if( bRepeatedParticle )
                    {
                        oField.SetName( "value" );
                        oField.SetMinOccurs( 1 );
                        oField.SetMaxOccurs( 1 );
                        oField.SetNotNullable( true );
                    }
                    else
                    {
                        if( nMinOccurs == 0 )
                        {
                            for(size_t j=0; j<aoFields.size();j++)
                            {
                                aoFields[j].SetMinOccurs( 0 );
                                aoFields[j].SetNotNullable( false );
                            }
                        }

                        oField.SetName( osPrefixedEltName );
                        oField.SetMinOccurs( (bIsChoice) ? 0 : nMinOccurs );
                        oField.SetMaxOccurs( nMaxOccurs );

                        // If the element has minOccurs=0 and is nillable, then we
                        // need an extra field to be able to distinguish between the
                        // case of the missing element or the element with
                        // xsi:nil="true"
                        if( nMinOccurs == 0 && poElt->getNillable() && !m_bUseNullState )
                        {
                            GMLASField oFieldNil;
                            oFieldNil.SetName( osPrefixedEltName + "_" + szNIL );
                            oFieldNil.SetXPath( osElementXPath + "/" +
                                                szAT_XSI_NIL );
                            oFieldNil.SetType( GMLAS_FT_BOOLEAN, "boolean" );
                            oFieldNil.SetMinOccurs( 0 );
                            oFieldNil.SetMaxOccurs( 1 );
                            aoFields.push_back(oFieldNil);
                        }

                    }
                    oField.SetXPath( osElementXPath );
                    oField.SetDocumentation( GetAnnotationDoc( poElt ) );

                    aoFields.push_back(oField);
                    if( oField.IsArray() )
                    {
                        oClass.AddField( oField );
                        bNothingMoreToDo = true;
                    }
                }
                else if( IsAnyType(poEltCT) )
                {
                    GMLASField oField;
                    oField.SetType( GMLAS_FT_ANYTYPE, szXS_ANY_TYPE );
                    if( bRepeatedParticle )
                    {
                        oField.SetName( "value" );
                        oField.SetMinOccurs( 1 );
                        oField.SetMaxOccurs( 1 );
                        oField.SetNotNullable( true );
                    }
                    else
                    {
                        if( nMinOccurs == 0 )
                        {
                            for(size_t j=0; j<aoFields.size();j++)
                            {
                                aoFields[j].SetMinOccurs( 0 );
                                aoFields[j].SetNotNullable( false );
                            }
                        }

                        oField.SetName( osPrefixedEltName );
                        oField.SetMinOccurs( nMinOccurs );
                        oField.SetMaxOccurs( nMaxOccurs );
                    }
                    oField.SetXPath( osElementXPath );
                    oField.SetDocumentation( GetAnnotationDoc( poElt ) );

                    aoFields.push_back(oField);
                }

                // Is it an element that we already visited ? (cycle)
                else if( poEltCT->getParticle() != NULL &&
                         oSetVisitedModelGroups.find(
                            poEltCT->getParticle()->getModelGroupTerm()) !=
                                                oSetVisitedModelGroups.end() )
                {
                    CreateNonNestedRelationship(poElt,
                                                apoImplEltList,
                                                oClass,
                                                bMoveNestedClassToTop ? 1 :
                                                        MAXOCCURS_UNLIMITED,
                                                bEltNameWillNeedPrefix,
                                                true, // force junction table
                                                false // regular case
                                               );

                    bNothingMoreToDo = true;
                }

                else
                {
                    GMLASFeatureClass oNestedClass;
                    oNestedClass.SetName( oClass.GetName() + "_" +
                                          osPrefixedEltName );
                    oNestedClass.SetXPath( osElementXPath );
                    oNestedClass.SetDocumentation( GetAnnotationDoc( poElt ) );

                    // NULL can happen, for example for gml:ReferenceType
                    // that is an empty sequence with just attributes
                    if( poEltCT->getParticle() != NULL )
                    {
#ifdef DEBUG_VERBOSE
                        CPLDebug("GMLAS", "Exploring %s",
                                 osElementXPath.c_str());
#endif
                        std::set<XSModelGroup*>
                            oSetNewVisitedModelGroups(oSetVisitedModelGroups);

                        std::map< CPLString, int > oMapCountOccurrencesOfSameNameSub;
                        BuildMapCountOccurrencesOfSameName(
                            poEltCT->getParticle()->getModelGroupTerm(),
                            oMapCountOccurrencesOfSameNameSub);

                        if( !ExploreModelGroup(
                                           poEltCT->getParticle()->
                                                            getModelGroupTerm(),
                                           NULL,
                                           oNestedClass,
                                           nRecursionCounter + 1,
                                           oSetNewVisitedModelGroups,
                                           poModel,
                                           oMapCountOccurrencesOfSameNameSub) )
                        {
                            return false;
                        }
                    }

                    // If we have a element of type gml:ReferenceType that has
                    // a targetElement in its annotation.appinfo, then create
                    // a dedicated field to have cross-layer relationships.
                    if( IsGMLNamespace(transcode(poTypeDef->getNamespace())) &&
                        transcode(poTypeDef->getName()) == "ReferenceType" &&
                        !osTargetElement.empty() )
                    {
                        XSElementDeclaration* poTargetElt =
                            GetTopElementDeclarationFromXPath(osTargetElement,
                                                              poModel);
                        // TODO: even for non abstract we should probably
                        // handle substitutions
                        if( poTargetElt != NULL && !poTargetElt->getAbstract() )
                        {
                            // If the element is nillable, then we
                            // need an extra field to be able to distinguish between the
                            // case of the missing element or the element with
                            // xsi:nil="true"
                            if( poElt->getNillable() && !m_bUseNullState )
                            {
                                GMLASField oFieldNil;
                                oFieldNil.SetName( osPrefixedEltName + "_" + szNIL );
                                oFieldNil.SetXPath( osElementXPath + "/" +
                                                    szAT_XSI_NIL );
                                oFieldNil.SetType( GMLAS_FT_BOOLEAN, "boolean" );
                                oFieldNil.SetMinOccurs( 0 );
                                oFieldNil.SetMaxOccurs( 1 );
                                aoFields.push_back(oFieldNil);
                            }

                            GMLASField oField;
                            // Fake xpath
                            oField.SetXPath(
                                GMLASField::MakePKIDFieldXPathFromXLinkHrefXPath(
                                            osElementXPath + "/" + szAT_XLINK_HREF));
                            oField.SetName( osPrefixedEltName + szPKID_SUFFIX );
                            oField.SetMinOccurs(0);
                            oField.SetMaxOccurs(1);
                            oField.SetType( GMLAS_FT_STRING, szXS_STRING );
                            oField.SetCategory(
                                GMLASField::PATH_TO_CHILD_ELEMENT_WITH_LINK );
                            oField.SetRelatedClassXPath(osTargetElement);
                            aoFields.push_back( oField );
                        }
                        else if( poTargetElt != NULL && poTargetElt->getAbstract() )
                        {
                            // If the element is nillable, then we
                            // need an extra field to be able to distinguish between the
                            // case of the missing element or the element with
                            // xsi:nil="true"
                            if( poElt->getNillable() && !m_bUseNullState )
                            {
                                GMLASField oFieldNil;
                                oFieldNil.SetName( osPrefixedEltName + "_" + szNIL );
                                oFieldNil.SetXPath( osElementXPath + "/" +
                                                    szAT_XSI_NIL );
                                oFieldNil.SetType( GMLAS_FT_BOOLEAN, "boolean" );
                                oFieldNil.SetMinOccurs( 0 );
                                oFieldNil.SetMaxOccurs( 1 );
                                aoFields.push_back(oFieldNil);
                            }

                            // e.g importing http://inspire.ec.europa.eu/schemas/ad/4.0
                            // references bu-base:AbstractConstruction, but sometimes
                            // there are no realization available for it, so no
                            // need to be verbose about that.
                            std::vector<XSElementDeclaration*>
                                                        apoImplTargetEltList;
                            GetConcreteImplementationTypes(poTargetElt,
                                                        apoImplTargetEltList);
                            if( !apoImplTargetEltList.empty() )
                            {
                                CPLDebug("GMLAS",
                                         "Not handled: targetElement %s of %s "
                                         "is abstract but has substitutions",
                                         osTargetElement.c_str(),
                                         osElementXPath.c_str());
                            }
                        }
                        else
                        {
                            // This shouldn't happen with consistent schemas
                            // but as targetElement is in <annotation>, no
                            // general-purpose XSD validator can ensure this
                            CPLDebug("GMLAS", "%s is a targetElement of %s, "
                                     "but cannot be found",
                                     osTargetElement.c_str(),
                                     osElementXPath.c_str());
                        }
                    }

                    // Can we move the nested class(es) one level up ?
                    if( bMoveNestedClassToTop )
                    {
                        // Case of an element like
                        //   <xs:element name="foo">
                        //      <xs:complexType>
                        //          <xs:sequence>

                        const std::vector<GMLASField>& osNestedClassFields =
                                                    oNestedClass.GetFields();
                        for(size_t j = 0; j < osNestedClassFields.size(); j++ )
                        {
                            GMLASField oField(osNestedClassFields[j]);
                            oField.SetName( 
                                osPrefixedEltName + "_" + oField.GetName() );
                            if( nMinOccurs == 0 ||
                                (poEltCT->getParticle() != NULL &&
                                 poEltCT->getParticle()->getMinOccurs() == 0) )
                            {
                                oField.SetMinOccurs(0);
                                oField.SetNotNullable(false);
                            }
                            aoFields.push_back( oField );
                        }

                        aoNestedClasses = oNestedClass.GetNestedClasses();
                    }
                    else
                    {
                        // Case of an element like
                        //   <xs:element name="foo">
                        //      <xs:complexType>
                        //          <xs:sequence maxOccurs="unbounded">
                        // or
                        //   <xs:element name="foo" maxOccurs="unbounded">
                        //      <xs:complexType>
                        //          <xs:sequence>
                        // or even
                        //   <xs:element name="foo" maxOccurs="unbounded">
                        //      <xs:complexType>
                        //          <xs:sequence maxOccurs="unbounded">
                        if( m_bUseArrays && nAttrListSize == 0 &&
                            oNestedClass.GetNestedClasses().empty() &&
                            oNestedClass.GetFields().size() == 1 &&
                            IsCompatibleOfArray(
                                    oNestedClass.GetFields()[0].GetType()) &&
                            oNestedClass.GetFields()[0].GetCategory() !=
                                GMLASField::PATH_TO_CHILD_ELEMENT_WITH_LINK )
                        {
                            // In the case the sequence has a single element,
                            // compatible of array type, and no attribute and
                            // no nested classes, then add an array attribute
                            // at the top-level
                            GMLASField oField (oNestedClass.GetFields()[0] );
                            oField.SetName( osPrefixedEltName + "_" +
                                            oField.GetName() );
                            if( oField.GetMaxOccurs() == 1 &&
                                bEltRepeatedParticle &&
                                poEltCT->getParticle() != NULL )
                            {
                                oField.SetMaxOccurs( nMaxOccursEltParticle );
                            }
                            oField.SetArray( true );
                            oClass.AddField( oField );
                        }
                        else
                        {
                            if( !aoFields.empty() && bEltRepeatedParticle)
                            {
                                // We have attributes and the sequence is
                                // repeated
                                //   <xs:element name="foo" maxOccurs="unbounded">
                                //      <xs:complexType>
                                //          <xs:sequence maxOccurs="unbounded">
                                //              ...
                                //          </xs:sequence>
                                //          <xs:attribute .../>
                                //      </xs:complexType>
                                //   </xs:element>
                                // So we need to create an
                                // intermediate class to store them
                                GMLASFeatureClass oIntermediateNestedClass;
                                oIntermediateNestedClass.SetName(
                                        oClass.GetName() + "_" +
                                        osPrefixedEltName );
                                oIntermediateNestedClass.SetXPath( osElementXPath );

                                oIntermediateNestedClass.PrependFields( aoFields );

                                oNestedClass.SetName( oClass.GetName() + "_" +
                                        osPrefixedEltName + "_sequence" );
                                oNestedClass.SetXPath( oNestedClass.GetXPath() +
                                        szEXTRA_SUFFIX + "sequence");
                                oNestedClass.SetIsRepeatedSequence( true );

                                GMLASField oField;
                                oField.SetXPath( osElementXPath );
                                oField.SetCategory(
                                    GMLASField::PATH_TO_CHILD_ELEMENT_NO_LINK);
                                if( nMaxOccursEltParticle != 1 )
                                    oField.SetRepetitionOnSequence( true );
                                oField.SetMinOccurs( nMinOccursEltParticle );
                                oField.SetMaxOccurs( nMaxOccursEltParticle );
                                oField.SetRelatedClassXPath( oNestedClass.GetXPath() );
                                oIntermediateNestedClass.AddField(oField);

                                oIntermediateNestedClass.AddNestedClass( oNestedClass );

                                oClass.AddNestedClass( oIntermediateNestedClass );
                            }
                            else
                            {
                                oNestedClass.SetIsRepeatedSequence(
                                                        bEltRepeatedParticle );
                                oNestedClass.PrependFields( aoFields );

                                oClass.AddNestedClass( oNestedClass );
                            }

                            GMLASField oField;
                            oField.SetName( osPrefixedEltName );
                            oField.SetXPath( osElementXPath );
                            if( bRepeatedParticle )
                            {
                                if( poEltCT->getParticle() != NULL )
                                {
                                    oField.SetMinOccurs( ComposeMinOccurs(
                                        nMinOccurs, nMinOccursEltParticle) );
                                    oField.SetMaxOccurs( ComposeMaxOccurs(
                                        nMaxOccurs, nMaxOccursEltParticle) );
                                }
                                else
                                {
                                    oField.SetMinOccurs( nMinOccurs );
                                    oField.SetMaxOccurs( nMaxOccurs );
                                }
                            }
                            else if( poEltCT->getParticle() != NULL )
                            {
                                if( nMaxOccursEltParticle != 1 )
                                    oField.SetRepetitionOnSequence( true );
                                oField.SetMinOccurs( nMinOccursEltParticle );
                                oField.SetMaxOccurs( nMaxOccursEltParticle );
                            }
                            oField.SetCategory(
                                    GMLASField::PATH_TO_CHILD_ELEMENT_NO_LINK);
                            oField.SetRelatedClassXPath( oField.GetXPath() );
                            oClass.AddField(oField);
                        }

                        bNothingMoreToDo = true;
                    }
                }

                if( bNothingMoreToDo )
                {
                    // Nothing to do
                }
                else if( bRepeatedParticle )
                {
                    GMLASFeatureClass oNestedClass;
                    oNestedClass.SetName( oClass.GetName() + "_" +
                                          osPrefixedEltName );
                    oNestedClass.SetXPath( osElementXPath );
                    oNestedClass.AppendFields( aoFields );
                    oNestedClass.SetDocumentation( GetAnnotationDoc( poElt ) );
                    oClass.AddNestedClass( oNestedClass );

                    GMLASField oField;
                    oField.SetName( osPrefixedEltName );
                    oField.SetXPath( osElementXPath );
                    oField.SetMinOccurs( (bIsChoice) ? 0 : nMinOccurs );
                    oField.SetMaxOccurs( nMaxOccurs );
                    oField.SetCategory(
                                    GMLASField::PATH_TO_CHILD_ELEMENT_NO_LINK);
                    oField.SetRelatedClassXPath( oField.GetXPath() );
                    oClass.AddField(oField);
                }
                else
                {
                    oClass.AppendFields( aoFields );
                    for(size_t j = 0; j < aoNestedClasses.size(); j++ )
                    {
                        oClass.AddNestedClass( aoNestedClasses[j] );
                    }
                }
            }
        }
        else if( poParticle->getTermType() == XSParticle::TERM_MODELGROUP )
        {
            XSModelGroup* psSubModelGroup = poParticle->getModelGroupTerm();
            if( bRepeatedParticle )
            {
                GMLASFeatureClass oNestedClass;
                CPLString osGroupName;
                XSModelGroupDefinition* psGroupDefinition =
                                            GetGroupDefinition(psSubModelGroup);
                if( psGroupDefinition != NULL )
                {
                    osGroupName = transcode( psGroupDefinition->getName() );
                    oNestedClass.SetDocumentation(
                        GetAnnotationDoc( psGroupDefinition->getAnnotation() ) );
                }
                else
                {
                    // Is it a <xs:choice maxOccurs=">1|unbounded"
                    if (psSubModelGroup->getCompositor() ==
                                            XSModelGroup::COMPOSITOR_CHOICE)
                    {
                        std::set<XSModelGroup*>
                            oSetNewVisitedModelGroups(oSetVisitedModelGroups);
                        GMLASFeatureClass oTmpClass;
                        oTmpClass.SetName( oClass.GetName() );
                        oTmpClass.SetXPath( oClass.GetXPath() );
                        if( !ExploreModelGroup( psSubModelGroup,
                                                NULL,
                                                oTmpClass,
                                                nRecursionCounter + 1,
                                                oSetNewVisitedModelGroups,
                                                poModel,
                                                oMapCountOccurrencesOfSameName ) )
                        {
                            return false;
                        }
                        bool bHasArray = false;
                        std::vector<GMLASField>& oTmpFields =
                                                        oTmpClass.GetFields();
                        for( size_t j = 0; j < oTmpFields.size(); ++j )
                        {
                            if( oTmpFields[j].IsArray() )
                            {
                                bHasArray = true;
                                break;
                            }
                        }
                        if( !bHasArray )
                        {
                            for( size_t j = 0; j < oTmpFields.size(); ++j )
                            {
                                oTmpFields[j].SetMayAppearOutOfOrder( true );
                                oClass.AddField( oTmpFields[j] );
                            }
                            return true;
                        }
                    }

                    nGroup ++;
                    osGroupName = CPLSPrintf("_group%d", nGroup);
                }
                oNestedClass.SetName( oClass.GetName() + "_" + osGroupName);
                oNestedClass.SetIsGroup(true);
                oNestedClass.SetIsRepeatedSequence(true);
                // Caution: we will change it afterwards !
                oNestedClass.SetXPath( oClass.GetXPath() );
                std::set<XSModelGroup*>
                    oSetNewVisitedModelGroups(oSetVisitedModelGroups);
                if( !ExploreModelGroup( psSubModelGroup,
                                        NULL,
                                        oNestedClass,
                                        nRecursionCounter + 1,
                                        oSetNewVisitedModelGroups,
                                        poModel,
                                        oMapCountOccurrencesOfSameName) )
                {
                    return false;
                }
                // This is a nasty hack. We set a unique fake xpath *AFTER*
                // processing the group, so that we can add a fake GROUP field
                // pointing to the nested class
                oNestedClass.SetXPath( oClass.GetXPath() + szEXTRA_SUFFIX + osGroupName );

                if( m_bUseArrays &&
                    oNestedClass.GetFields().size() == 1 &&
                    IsCompatibleOfArray(oNestedClass.GetFields()[0].GetType()) )
                {
                    GMLASField oField(oNestedClass.GetFields()[0]);
                    oField.SetMinOccurs( ComposeMinOccurs(oField.GetMinOccurs(), nMinOccurs) );
                    oField.SetMaxOccurs( ComposeMaxOccurs(oField.GetMaxOccurs(), nMaxOccurs) );
                    oField.SetArray( true );
                    oClass.AddField( oField );
                }
                else
                {
                    oClass.AddNestedClass( oNestedClass );

                    GMLASField oField;
                    oField.SetCategory( GMLASField::GROUP );
                    oField.SetMinOccurs( nMinOccurs );
                    oField.SetMaxOccurs( nMaxOccurs );
                    oField.SetRelatedClassXPath( oNestedClass.GetXPath() );
                    oClass.AddField(oField);
                }
            }
            else
            {
                std::set<XSModelGroup*>
                    oSetNewVisitedModelGroups(oSetVisitedModelGroups);
                if( !ExploreModelGroup( psSubModelGroup,
                                        NULL,
                                        oClass,
                                        nRecursionCounter + 1,
                                        oSetNewVisitedModelGroups,
                                        poModel,
                                        oMapCountOccurrencesOfSameName ) )
                {
                    return false;
                }
            }
        }
        else if( poParticle->getTermType() == XSParticle::TERM_WILDCARD )
        {
            /* Special case for a layer that matches everything, as found */
            /* in swe:extension */
            XSWildcard* poWildcard = poParticle->getWildcardTerm();
            GMLASField oField;
            oField.SetXPath( oClass.GetXPath() + szMATCH_ALL );
            oField.SetName( "value" );
            oField.SetType( GMLAS_FT_ANYTYPE, szXS_ANY_TYPE );
            oField.SetIncludeThisEltInBlob( true );
            oField.SetMinOccurs( nMinOccurs );
            oField.SetMaxOccurs( 1 );
            oField.SetDocumentation(
                        GetAnnotationDoc( poWildcard->getAnnotation() ) );
            oClass.AddField(oField);
        }
    }

    return true;
}
