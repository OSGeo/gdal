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

CPL_CVSID("$Id$")

/************************************************************************/
/*                          GMLASXPathMatcher()                         */
/************************************************************************/

GMLASXPathMatcher::GMLASXPathMatcher()
{
}

/************************************************************************/
/*                         ~GMLASXPathMatcher()                         */
/************************************************************************/

GMLASXPathMatcher::~GMLASXPathMatcher()
{
}

/************************************************************************/
/*                           SetRefXPaths()                             */
/************************************************************************/

void    GMLASXPathMatcher::SetRefXPaths(const std::map<CPLString, CPLString>&
                                                oMapPrefixToURIReferenceXPaths,
                                         const std::vector<CPLString>&
                                                aosReferenceXPaths)
{
    m_oMapPrefixToURIReferenceXPaths = oMapPrefixToURIReferenceXPaths;
    m_aosReferenceXPathsUncompiled = aosReferenceXPaths;
}

/************************************************************************/
/*                     SetDocumentMapURIToPrefix()                      */
/************************************************************************/

void    GMLASXPathMatcher::SetDocumentMapURIToPrefix(
                        const std::map<CPLString,CPLString>& oMapURIToPrefix )
{
    m_aosReferenceXPaths.clear();

    // Split each reference XPath into its components
    for(size_t i = 0; i < m_aosReferenceXPathsUncompiled.size(); ++i )
    {
        const CPLString& osXPath( m_aosReferenceXPathsUncompiled[i] );

        std::vector<XPathComponent> oVector;

        size_t iPos = 0;
        bool bDirectChild = false;
        if( osXPath.size() >= 2 &&
            osXPath[0] == '/' && osXPath[1] == '/' )
        {
            iPos += 2;
        }
        else if( osXPath.size() >= 1 && osXPath[0] == '/' )
        {
            iPos += 1;
            bDirectChild = true;
        }

        while( iPos < osXPath.size() )
        {
            size_t iPosNextSlash = osXPath.find('/', iPos);

            if( iPos == iPosNextSlash )
            {
                bDirectChild = false;
                iPos ++;
                continue;
            }

            CPLString osCurNode;
            if( iPosNextSlash == std::string::npos )
                osCurNode.assign(osXPath, iPos, std::string::npos);
            else
                osCurNode.assign(osXPath, iPos, iPosNextSlash - iPos);

            // Translate the configuration prefix to the equivalent in
            // this current schema
            size_t iPosColumn = osCurNode.find(':');
            if( iPosColumn != std::string::npos )
            {
                bool bIsAttr = ( osCurNode[0] == '@' );
                CPLString osPrefix;
                CPLString osLocalname;
                osPrefix.assign(osCurNode,
                                bIsAttr ? 1 : 0,
                                iPosColumn - (bIsAttr ? 1 : 0));
                osLocalname.assign(osCurNode, iPosColumn+1,
                                std::string::npos);

                const auto oIter =
                    m_oMapPrefixToURIReferenceXPaths.find(osPrefix);
                if( oIter != m_oMapPrefixToURIReferenceXPaths.end() )
                {
                    const CPLString& osURI( oIter->second );
                    const auto oIter2 = oMapURIToPrefix.find( osURI );
                    if( oIter2 == oMapURIToPrefix.end() )
                        break;
                    osPrefix.assign(oIter2->second);
                }

                osCurNode.clear();
                if( bIsAttr )
                    osCurNode.append(1, '@');
                osCurNode.append(osPrefix);
                osCurNode.append(1, ':');
                osCurNode.append(osLocalname);
            }

            XPathComponent comp;
            comp.m_osValue = osCurNode;
            comp.m_bDirectChild = bDirectChild;
            oVector.push_back(comp);

            if( iPosNextSlash == std::string::npos )
                iPos = osXPath.size();
            else
                iPos = iPosNextSlash + 1;

            bDirectChild = true;
        }

        if ( iPos < osXPath.size() )
            oVector.clear();
        m_aosReferenceXPaths.push_back(oVector);
    }
}

/************************************************************************/
/*                         MatchesRefXPath()                            */
/************************************************************************/

// This is a performance critical function, especially on geosciml schemas,
// and we make careful to not do any string copy or other memory allocation
// in it.
bool GMLASXPathMatcher::MatchesRefXPath(
                        const CPLString& osXPath,
                        const std::vector<XPathComponent>& oRefXPath)
{
    size_t iPos = 0;
    size_t iIdxInRef = 0;

    bool bDirectChild = oRefXPath[0].m_bDirectChild;
    while( iPos < osXPath.size() && iIdxInRef < oRefXPath.size() )
    {
        bDirectChild = oRefXPath[iIdxInRef].m_bDirectChild;
        size_t iPosNextSlash = osXPath.find('/', iPos);

        bool bNodeMatch;
        if( iPosNextSlash == std::string::npos )
        {
            bNodeMatch = osXPath.compare(iPos, std::string::npos,
                                         oRefXPath[iIdxInRef].m_osValue) == 0;
        }
        else
        {
            bNodeMatch = osXPath.compare(iPos, iPosNextSlash - iPos,
                                         oRefXPath[iIdxInRef].m_osValue) == 0;
        }

        if( !bNodeMatch )
        {
            if( bDirectChild )
                return false;

            if( iPosNextSlash == std::string::npos )
                return false;
            iPos = iPosNextSlash + 1;
            continue;
        }

        if( iPosNextSlash == std::string::npos )
            iPos = osXPath.size();
        else
            iPos = iPosNextSlash + 1;
        iIdxInRef ++;
        bDirectChild = true;
    }

    return (!bDirectChild || iPos == osXPath.size()) &&
            iIdxInRef == oRefXPath.size();
}

/************************************************************************/
/*                         MatchesRefXPath()                            */
/************************************************************************/

bool GMLASXPathMatcher::MatchesRefXPath(
                            const CPLString& osXPath,
                            CPLString& osOutMatchedXPath) const
{
    for(size_t i = 0; i < m_aosReferenceXPaths.size(); ++i )
    {
        if( !m_aosReferenceXPaths[i].empty() &&
            MatchesRefXPath(osXPath, m_aosReferenceXPaths[i]) )
        {
            osOutMatchedXPath = m_aosReferenceXPathsUncompiled[i];
            return true;
        }
    }
    return false;
}
