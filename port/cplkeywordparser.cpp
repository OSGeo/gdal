/******************************************************************************
 *
 * Project:  Common Portability Library
 * Purpose:  Implementation of CPLKeywordParser - a class for parsing
 *           the keyword format used for files like QuickBird .RPB files.
 *           This is a slight variation on the NASAKeywordParser used for
 *           the PDS/ISIS2/ISIS3 formats.
 * Author:   Frank Warmerdam <warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2010, Even Rouault <even dot rouault at spatialys.com>
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

//! @cond Doxygen_Suppress

#include "cpl_port.h"
#include "cplkeywordparser.h"

#include <cctype>
#include <cstring>
#include <string>

#include "cpl_string.h"
#include "cpl_vsi.h"

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                          CPLKeywordParser                           */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                         CPLKeywordParser()                          */
/************************************************************************/

CPLKeywordParser::CPLKeywordParser() = default;

/************************************************************************/
/*                        ~CPLKeywordParser()                          */
/************************************************************************/

CPLKeywordParser::~CPLKeywordParser()

{
    CSLDestroy( papszKeywordList );
    papszKeywordList = nullptr;
}

/************************************************************************/
/*                               Ingest()                               */
/************************************************************************/

int CPLKeywordParser::Ingest( VSILFILE *fp )

{
/* -------------------------------------------------------------------- */
/*      Read in buffer till we find END all on its own line.            */
/* -------------------------------------------------------------------- */
    for( ; true; )
    {
        char szChunk[513] = {};
        const size_t nBytesRead = VSIFReadL( szChunk, 1, 512, fp );

        szChunk[nBytesRead] = '\0';
        osHeaderText += szChunk;

        if( nBytesRead < 512 )
            break;

        const char *pszCheck = nullptr;
        if( osHeaderText.size() > 520 )
            pszCheck = osHeaderText.c_str() + (osHeaderText.size() - 520);
        else
            pszCheck = szChunk;

        if( strstr(pszCheck, "\r\nEND;\r\n") != nullptr
            || strstr(pszCheck, "\nEND;\n") != nullptr )
            break;
    }

    pszHeaderNext = osHeaderText.c_str();

/* -------------------------------------------------------------------- */
/*      Process name/value pairs, keeping track of a "path stack".      */
/* -------------------------------------------------------------------- */
    return ReadGroup( "", 0 );
}

/************************************************************************/
/*                             ReadGroup()                              */
/************************************************************************/

bool CPLKeywordParser::ReadGroup( const char *pszPathPrefix, int nRecLevel )

{
    CPLString osName;
    CPLString osValue;

    // Arbitrary threshold to avoid stack overflow
    if( nRecLevel == 100 )
        return false;

    for( ; true; )
    {
        if( !ReadPair( osName, osValue ) )
            return false;

        if( EQUAL(osName, "BEGIN_GROUP") || EQUAL(osName, "GROUP") )
        {
            if( !ReadGroup((CPLString(pszPathPrefix) + osValue + ".").c_str(),
                           nRecLevel + 1) )
                return false;
        }
        else if( STARTS_WITH_CI(osName, "END") )
        {
            return true;
        }
        else
        {
            osName = pszPathPrefix + osName;
            papszKeywordList = CSLSetNameValue( papszKeywordList,
                                                osName, osValue );
        }
    }
}

/************************************************************************/
/*                              ReadPair()                              */
/*                                                                      */
/*      Read a name/value pair from the input stream.  Strip off        */
/*      white space, ignore comments, split on '='.                     */
/************************************************************************/

bool CPLKeywordParser::ReadPair( CPLString &osName, CPLString &osValue )

{
    osName = "";
    osValue = "";

    if( !ReadWord( osName ) )
        return false;

    SkipWhite();

    if( EQUAL(osName, "END") )
        return TRUE;

    if( *pszHeaderNext != '=' )
    {
        // ISIS3 does not have anything after the end group/object keyword.
        return EQUAL(osName, "End_Group") || EQUAL(osName, "End_Object");
    }

    pszHeaderNext++;

    SkipWhite();

    osValue = "";

    // Handle value lists like:     Name   = (Red, Red)
    // or list of lists like: TLCList = ( (0, 0.000000), (8299, 4.811014) );
    if( *pszHeaderNext == '(' )
    {
        CPLString osWord;
        int nDepth = 0;
        const char* pszLastPos = pszHeaderNext;

        while( ReadWord( osWord ) && pszLastPos != pszHeaderNext)
        {
            SkipWhite();
            pszLastPos = pszHeaderNext;

            osValue += osWord;
            const char* pszIter = osWord.c_str();
            bool bInQuote = false;
            while( *pszIter != '\0' )
            {
                if( *pszIter == '"' )
                    bInQuote = !bInQuote;
                else if( !bInQuote )
                {
                    if( *pszIter == '(' )
                        nDepth++;
                    else if( *pszIter == ')' )
                    {
                        nDepth--;
                        if( nDepth == 0 )
                            break;
                    }
                }
                pszIter++;
            }
            if( *pszIter == ')' && nDepth == 0 )
                break;
        }
    }

    else // Handle more normal "single word" values.
    {
        // Special case to handle non-conformant IMD files generated by
        // previous GDAL version where we omit to surround values that have
        // spaces with double quotes.
        // So we use a heuristics to handle things like:
        //       key = value with spaces without single or double quotes at beginning of value;[\r]\n
        const char* pszNextLF = strchr(pszHeaderNext, '\n');
        if( pszNextLF )
        {
            std::string osTxt(pszHeaderNext, pszNextLF - pszHeaderNext);
            const auto nCRPos = osTxt.find('\r');
            const auto nSemiColonPos = osTxt.find(';');
            const auto nQuotePos = osTxt.find('\'');
            const auto nDoubleQuotePos = osTxt.find('"');
            const auto nLTPos = osTxt.find('<');
            if( nSemiColonPos != std::string::npos &&
                (nCRPos == std::string::npos || (nCRPos + 1 == osTxt.size())) &&
                ((nCRPos != std::string::npos && (nSemiColonPos + 1 == nCRPos)) ||
                 (nCRPos == std::string::npos && (nSemiColonPos + 1 == osTxt.size()))) &&
                (nQuotePos == std::string::npos || nQuotePos != 0) &&
                (nDoubleQuotePos == std::string::npos || nDoubleQuotePos != 0) &&
                (nLTPos == std::string::npos || osTxt.find('>') == std::string::npos) )
            {
                pszHeaderNext = pszNextLF;
                osTxt.resize(nSemiColonPos);
                osValue = osTxt;
                while( !osValue.empty() && osValue.back() == ' ' )
                    osValue.resize(osValue.size() - 1);
                return true;
            }
        }

        if( !ReadWord( osValue ) )
            return false;
    }

    SkipWhite();

    // No units keyword?
    if( *pszHeaderNext != '<' )
        return true;

    // Append units keyword.  For lines that like like this:
    //  MAP_RESOLUTION               = 4.0 <PIXEL/DEGREE>

    CPLString osWord;

    osValue += " ";

    while( ReadWord( osWord ) )
    {
        SkipWhite();

        osValue += osWord;
        if( osWord.back() == '>' )
            break;
    }

    return true;
}

/************************************************************************/
/*                              ReadWord()                              */
/************************************************************************/

bool CPLKeywordParser::ReadWord( CPLString &osWord )

{
    osWord = "";

    SkipWhite();

    if( *pszHeaderNext == '\0' || *pszHeaderNext == '=' )
        return false;

    while( *pszHeaderNext != '\0'
           && *pszHeaderNext != '='
           && *pszHeaderNext != ';'
           && !isspace(static_cast<unsigned char>(*pszHeaderNext)) )
    {
        if( *pszHeaderNext == '"' )
        {
            osWord += *(pszHeaderNext++);
            while( *pszHeaderNext != '"' )
            {
                if( *pszHeaderNext == '\0' )
                    return false;

                osWord += *(pszHeaderNext++);
            }
            osWord += *(pszHeaderNext++);
        }
        else if( *pszHeaderNext == '\'' )
        {
            osWord += *(pszHeaderNext++);
            while( *pszHeaderNext != '\'' )
            {
                if( *pszHeaderNext == '\0' )
                    return false;

                osWord += *(pszHeaderNext++);
            }
            osWord += *(pszHeaderNext++);
        }
        else
        {
            osWord += *pszHeaderNext;
            pszHeaderNext++;
        }
    }

    if( *pszHeaderNext == ';' )
        pszHeaderNext++;

    return true;
}

/************************************************************************/
/*                             SkipWhite()                              */
/************************************************************************/

void CPLKeywordParser::SkipWhite()

{
    for( ; true; )
    {
        // Skip white space (newline, space, tab, etc )
        if( isspace( static_cast<unsigned char>(*pszHeaderNext) ) )
        {
            pszHeaderNext++;
            continue;
        }

        // Skip C style comments
        if( *pszHeaderNext == '/' && pszHeaderNext[1] == '*' )
        {
            pszHeaderNext += 2;

            while( *pszHeaderNext != '\0'
                   && (*pszHeaderNext != '*'
                       || pszHeaderNext[1] != '/' ) )
            {
                pszHeaderNext++;
            }
            if( *pszHeaderNext == '\0' )
                break;

            pszHeaderNext += 2;
            continue;
        }

        // Skip # style comments
        if( *pszHeaderNext == '#' )
        {
            pszHeaderNext += 1;

            // consume till end of line.
            while( *pszHeaderNext != '\0'
                   && *pszHeaderNext != 10
                   && *pszHeaderNext != 13 )
            {
                pszHeaderNext++;
            }
            continue;
        }

        // not white space, return.
        return;
    }
}

/************************************************************************/
/*                             GetKeyword()                             */
/************************************************************************/

const char *CPLKeywordParser::GetKeyword( const char *pszPath,
                                            const char *pszDefault )

{
    const char *pszResult = CSLFetchNameValue( papszKeywordList, pszPath );
    if( pszResult == nullptr )
        return pszDefault;

    return pszResult;
}

//! @endcond
