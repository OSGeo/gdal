/******************************************************************************
 *
 * Project:  PDS Driver; Planetary Data System Format
 * Purpose:  Implementation of NASAKeywordHandler - a class to read
 *           keyword data from PDS, ISIS2 and ISIS3 data products.
 * Author:   Frank Warmerdam <warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2010, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2017 Hobu Inc
 * Copyright (c) 2017, Dmitry Baryshnikov <polimax@mail.ru>
 * Copyright (c) 2017, NextGIS <info@nextgis.com>
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
 ****************************************************************************
 * Object Description Language (ODL) is used to encode data labels for PDS
 * and other NASA data systems. Refer to Chapter 12 of "PDS Standards
 * Reference" at http://pds.jpl.nasa.gov/tools/standards-reference.shtml for
 * further details about ODL.
 *
 * This is also known as PVL (Parameter Value Language) which is written
 * about at http://www.orrery.us/node/44 where it notes:
 *
 * The PVL syntax that the PDS uses is specified by the Consultative Committee
 * for Space Data Systems in their Blue Book publication: "Parameter Value
 * Language Specification (CCSD0006 and CCSD0008)", June 2000
 * [CCSDS 641.0-B-2], and Green Book publication: "Parameter Value Language -
 * A Tutorial", June 2000 [CCSDS 641.0-G-2]. PVL has also been accepted by the
 * International Standards Organization (ISO), as a Final Draft International
 * Standard (ISO 14961:2002) keyword value type language for naming and
 * expressing data values.
 * --
 * also of interest, on PDS ODL:
 *  http://pds.jpl.nasa.gov/documents/sr/Chapter12.pdf
 *
 ****************************************************************************/

#include "nasakeywordhandler.h"
#include "ogrgeojsonreader.h"
#include <vector>

CPL_CVSID("$Id$")

//! @cond Doxygen_Suppress

/************************************************************************/
/* ==================================================================== */
/*                          NASAKeywordHandler                          */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                         NASAKeywordHandler()                         */
/************************************************************************/

NASAKeywordHandler::NASAKeywordHandler() :
    pszHeaderNext(nullptr),
    m_bStripSurroundingQuotes(false)
{
    oJSon.Deinit();
}

/************************************************************************/
/*                        ~NASAKeywordHandler()                         */
/************************************************************************/

NASAKeywordHandler::~NASAKeywordHandler()

{
}

/************************************************************************/
/*                               Ingest()                               */
/************************************************************************/

int NASAKeywordHandler::Ingest( VSILFILE *fp, int nOffset )

{
/* -------------------------------------------------------------------- */
/*      Read in buffer till we find END all on its own line.            */
/* -------------------------------------------------------------------- */
    if( VSIFSeekL( fp, nOffset, SEEK_SET ) != 0 )
        return FALSE;

    for( ; true; )
    {
        char szChunk[513];

        int nBytesRead = static_cast<int>(VSIFReadL( szChunk, 1, 512, fp ));

        szChunk[nBytesRead] = '\0';
        osHeaderText += szChunk;

        if( nBytesRead < 512 )
            break;

        const char *pszCheck = nullptr;
        if( osHeaderText.size() > 520 )
            pszCheck = osHeaderText.c_str() + (osHeaderText.size() - 520);
        else
            pszCheck = szChunk;

        if( strstr(pszCheck,"\r\nEND\r\n") != nullptr
            || strstr(pszCheck,"\nEND\n") != nullptr
            || strstr(pszCheck,"\r\nEnd\r\n") != nullptr
            || strstr(pszCheck,"\nEnd\n") != nullptr )
            break;
    }

    pszHeaderNext = osHeaderText.c_str();


/* -------------------------------------------------------------------- */
/*      Process name/value pairs, keeping track of a "path stack".      */
/* -------------------------------------------------------------------- */
    oJSon = CPLJSONObject();
    return ReadGroup( "", oJSon, 0 );
}

/************************************************************************/
/*                             ReadGroup()                              */
/************************************************************************/

int NASAKeywordHandler::ReadGroup( const std::string& osPathPrefix, CPLJSONObject &oCur,
                                   int nRecLevel )

{
    if( osPathPrefix.size() > 256 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Too big prefix for GROUP");
        return FALSE;
    }
    if( nRecLevel == 100 )
        return FALSE;
    for( ; true; )
    {
        CPLString osName, osValue;
        if( !ReadPair( osName, osValue, oCur ) )
            return FALSE;

        if( EQUAL(osName,"OBJECT") || EQUAL(osName,"GROUP") )
        {
            CPLJSONObject oNewGroup;
            oNewGroup.Add( "_type", EQUAL(osName,"OBJECT") ? "object" : "group" );
            if( !ReadGroup( (osPathPrefix + osValue + ".").c_str(),
                            oNewGroup, nRecLevel + 1 ) )
            {
                return FALSE;
            }
            CPLJSONObject oName = oNewGroup["Name"];
            if( (osValue == "Table" || osValue == "Field") &&
                (oName.GetType() == CPLJSONObject::Type::String) )
            {
                oCur.Add( osValue + "_" + oName.ToString(), oNewGroup );
                oNewGroup.Add( "_container_name", osValue );
            }
            else if( oCur[osValue].IsValid() )
            {
                int nIter = 2;
                while( oCur[osValue + CPLSPrintf("_%d", nIter)].IsValid() )
                {
                    nIter ++;
                }
                oCur.Add( osValue + CPLSPrintf("_%d", nIter), oNewGroup );
                oNewGroup.Add( "_container_name", osValue );
            }
            else
            {
                oCur.Add( osValue, oNewGroup );
            }
        }
        else if( EQUAL(osName,"END")
                 || EQUAL(osName,"END_GROUP" )
                 || EQUAL(osName,"END_OBJECT" ) )
        {
            return TRUE;
        }
        else
        {
            osName = osPathPrefix + osName;
            aosKeywordList.AddNameValue( osName, osValue );
        }
    }
}

/************************************************************************/
/*                        StripQuotesIfNeeded()                         */
/************************************************************************/

static CPLString StripQuotesIfNeeded(const CPLString& osWord,
                                     bool bQuotesAlreadyRemoved)
{
    if( bQuotesAlreadyRemoved || osWord.size() < 2 || osWord[0] != '"' )
        return osWord;
    return osWord.substr(1, osWord.size() - 2);
}

/************************************************************************/
/*                              ReadPair()                              */
/*                                                                      */
/*      Read a name/value pair from the input stream.  Strip off        */
/*      white space, ignore comments, split on '='.                     */
/*      Returns TRUE on success.                                        */
/************************************************************************/

int NASAKeywordHandler::ReadPair( CPLString &osName, CPLString &osValue,
                                  CPLJSONObject &oCur )

{
    osName = "";
    osValue = "";

    if( !ReadWord( osName ) )
        return FALSE;

    SkipWhite();

    if( EQUAL(osName,"END") )
        return TRUE;

    if( *pszHeaderNext != '=' )
    {
        // ISIS3 does not have anything after the end group/object keyword.
        if( EQUAL(osName,"End_Group") || EQUAL(osName,"End_Object") )
            return TRUE;

        return FALSE;
    }

    pszHeaderNext++;

    SkipWhite();

    osValue = "";
    bool bIsString = true;

    // Handle value lists like:
    // Name   = (Red, Red) or  {Red, Red} or even ({Red, Red}, {Red, Red})
    CPLJSONArray oArray;
    if( *pszHeaderNext == '(' || *pszHeaderNext == '{' )
    {
        std::vector<char> oStackArrayBeginChar;
        CPLString osWord;

        oStackArrayBeginChar.push_back(*pszHeaderNext);
        osValue += *pszHeaderNext;
        pszHeaderNext++;

        while( ReadWord( osWord, m_bStripSurroundingQuotes,
                         true, &bIsString ) )
        {
            if( *pszHeaderNext == '(' ||  *pszHeaderNext == '{' )
            {
                oStackArrayBeginChar.push_back(*pszHeaderNext);
                osValue += *pszHeaderNext;
                pszHeaderNext ++;
            }

            // TODO: we could probably do better with nested json arrays
            // instead of flattening when there are (( )) or ({ }) constructs
            if( bIsString )
            {
                if( !(osWord.empty() && (*pszHeaderNext == '(' ||
                      *pszHeaderNext == '{' || *pszHeaderNext == ')' ||
                      *pszHeaderNext == '}')) )
                {
                    oArray.Add(StripQuotesIfNeeded(osWord, m_bStripSurroundingQuotes));
                }
            }
            else  if( CPLGetValueType(osWord) == CPL_VALUE_INTEGER )
            {
                oArray.Add( atoi(osWord) );
            }
            else
            {
                oArray.Add( CPLAtof(osWord) );
            }

            osValue += osWord;
            while ( isspace( static_cast<unsigned char>( *pszHeaderNext ) ) )
            {
                pszHeaderNext++;
            }

            if( *pszHeaderNext == ')' )
            {
                osValue += *pszHeaderNext;
                if( oStackArrayBeginChar.empty() ||
                    oStackArrayBeginChar.back() != '(' )
                {
                    CPLDebug("PDS", "Unpaired ( ) for %s", osName.c_str());
                    return FALSE;
                }
                oStackArrayBeginChar.pop_back();
                pszHeaderNext ++;
                if( oStackArrayBeginChar.empty() )
                    break;
            }
            else if( *pszHeaderNext == '}' )
            {
                osValue += *pszHeaderNext;
                if( oStackArrayBeginChar.empty() ||
                    oStackArrayBeginChar.back() != '{' )
                {
                    CPLDebug("PDS", "Unpaired { } for %s", osName.c_str());
                    return FALSE;
                }
                oStackArrayBeginChar.pop_back();
                pszHeaderNext ++;
                if( oStackArrayBeginChar.empty() )
                    break;
            }
            else if( *pszHeaderNext == ',' )
            {
                osValue += *pszHeaderNext;
                pszHeaderNext ++;
                // Do not use SkipWhite() here to avoid being confuse by
                // constructs like
                // FOO = (#123456,
                //        #123456)
                // where we could confuse the second line with a comment.
                while ( isspace( static_cast<unsigned char>( *pszHeaderNext ) ) )
                {
                    pszHeaderNext++;
                }
            }
            SkipWhite();

        }
    }

    else // Handle more normal "single word" values.
    {
        if( !ReadWord( osValue, m_bStripSurroundingQuotes, false, &bIsString ) )
            return FALSE;
    }

    SkipWhite();

    // No units keyword?
    if( *pszHeaderNext != '<' )
    {
        if( !EQUAL(osName, "OBJECT") && !EQUAL(osName, "GROUP") )
        {
            if( oArray.Size() > 0 )
            {
                oCur.Add(osName, oArray);
            }
            else
            {
                if( bIsString )
                {
                    oCur.Add( osName, StripQuotesIfNeeded(osValue, m_bStripSurroundingQuotes) );
                }
                else if( CPLGetValueType(osValue) == CPL_VALUE_INTEGER )
                {
                    oCur.Add( osName, atoi(osValue) );
                }
                else
                {
                    oCur.Add( osName, CPLAtof(osValue) );
                }
            }
        }
        return TRUE;
    }

    CPLString osValueNoUnit(osValue);
    // Append units keyword.  For lines that like like this:
    //  MAP_RESOLUTION               = 4.0 <PIXEL/DEGREE>

    osValue += " ";

    CPLString osWord;
    CPLString osUnit;
    while( ReadWord( osWord ) )
    {
        SkipWhite();

        osValue += osWord;
        osUnit = osWord;
        if( osWord.back() == '>' )
            break;
    }

    if( osUnit[0] == '<' )
        osUnit = osUnit.substr(1);
    if( !osUnit.empty() && osUnit.back() == '>' )
        osUnit = osUnit.substr(0, osUnit.size() - 1);

    CPLJSONObject newObject;
    oCur.Add( osName, newObject );

    if( oArray.Size() > 0 )
    {
        newObject.Add( "value", oArray );
    }
    else
    {
        if( bIsString )
        {
            newObject.Add( "value", osValueNoUnit );
        }
        else if( CPLGetValueType(osValueNoUnit) == CPL_VALUE_INTEGER )
        {
            newObject.Add( "value", atoi(osValueNoUnit) );
        }
        else
        {
            newObject.Add( "value", CPLAtof(osValueNoUnit) );
        }
    }
    newObject.Add( "unit", osUnit );

    return TRUE;
}

/************************************************************************/
/*                              ReadWord()                              */
/*  Returns TRUE on success                                             */
/************************************************************************/

int NASAKeywordHandler::ReadWord( CPLString &osWord,
                                  bool bStripSurroundingQuotes,
                                  bool bParseList,
                                  bool* pbIsString )

{
    if( pbIsString )
        *pbIsString = false;
    osWord = "";

    SkipWhite();

    if( !(*pszHeaderNext != '\0'
          && *pszHeaderNext != '='
          && !isspace( static_cast<unsigned char>( *pszHeaderNext ) ) ) )
        return FALSE;

    /* Extract a text string delimited by '\"' */
    /* Convert newlines (CR or LF) within quotes. While text strings
       support them as per ODL, the keyword list doesn't want them */
    if( *pszHeaderNext == '"' )
    {
        if( pbIsString )
            *pbIsString = true;
        if( !bStripSurroundingQuotes )
            osWord += *(pszHeaderNext);
        pszHeaderNext ++;
        while( *pszHeaderNext != '"' )
        {
            if( *pszHeaderNext == '\0' )
                return FALSE;
            if( *pszHeaderNext == '\n' )
            {
                osWord += "\\n";
                pszHeaderNext++;
                continue;
            }
            if( *pszHeaderNext == '\r' )
            {
                osWord += "\\r";
                pszHeaderNext++;
                continue;
            }
            osWord += *(pszHeaderNext++);
        }
        if( !bStripSurroundingQuotes )
            osWord += *(pszHeaderNext);
        pszHeaderNext ++;

        return TRUE;
    }

    /* Extract a symbol string */
    /* These are expected to not have
       '\'' (delimiters),
       format effectors (should fit on a single line) or
       control characters.
    */
    if( *pszHeaderNext == '\'' )
    {
        if( pbIsString )
            *pbIsString = true;
        if( !bStripSurroundingQuotes )
            osWord += *(pszHeaderNext);
        pszHeaderNext ++;
        while( *pszHeaderNext != '\'' )
        {
            if( *pszHeaderNext == '\0' )
                return FALSE;

            osWord += *(pszHeaderNext++);
        }
        if( !bStripSurroundingQuotes )
            osWord += *(pszHeaderNext);
        pszHeaderNext ++;
        return TRUE;
    }

    /*
     * Extract normal text.  Terminated by '=' or whitespace.
     *
     * A special exception is that a line may terminate with a '-'
     * which is taken as a line extender, and we suck up white space to new
     * text.
     */
    while( *pszHeaderNext != '\0'
           && *pszHeaderNext != '='
           && ((bParseList && *pszHeaderNext != ',' && *pszHeaderNext != '(' &&
                *pszHeaderNext != ')'&& *pszHeaderNext != '{' &&
                *pszHeaderNext != '}' ) ||
               (!bParseList && !isspace(static_cast<unsigned char>( *pszHeaderNext ) ))) )
    {
        osWord += *pszHeaderNext;
        pszHeaderNext++;

        if( *pszHeaderNext == '-'
            && (pszHeaderNext[1] == 10 || pszHeaderNext[1] == 13) )
        {
            pszHeaderNext += 2;
            SkipWhite();
        }
    }

    if( pbIsString )
        *pbIsString = CPLGetValueType(osWord) == CPL_VALUE_STRING;

    return TRUE;
}

/************************************************************************/
/*                             SkipWhite()                              */
/*  Skip white spaces and C style comments                              */
/************************************************************************/

void NASAKeywordHandler::SkipWhite()

{
    for( ; true; )
    {
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
                return;

            pszHeaderNext += 2;

            // consume till end of line.
            // reduce sensibility to a label error
            while( *pszHeaderNext != '\0'
                   && *pszHeaderNext != 10
                   && *pszHeaderNext != 13 )
            {
                pszHeaderNext++;
            }
            continue;
        }

        // Skip # style comments
        if( (*pszHeaderNext == 10 || *pszHeaderNext == 13 ||
             *pszHeaderNext == ' ' || *pszHeaderNext == '\t' )
              && pszHeaderNext[1] == '#' )
        {
            pszHeaderNext += 2;

            // consume till end of line.
            while( *pszHeaderNext != '\0'
                   && *pszHeaderNext != 10
                   && *pszHeaderNext != 13 )
            {
                pszHeaderNext++;
            }
            continue;
        }

        // Skip white space (newline, space, tab, etc )
        if( isspace( static_cast<unsigned char>( *pszHeaderNext ) ) )
        {
            pszHeaderNext++;
            continue;
        }

        // not white space, return.
        return;
    }
}

/************************************************************************/
/*                             GetKeyword()                             */
/************************************************************************/

const char *NASAKeywordHandler::GetKeyword( const char *pszPath,
                                            const char *pszDefault )

{
    return aosKeywordList.FetchNameValueDef(pszPath, pszDefault);
}

/************************************************************************/
/*                             GetKeywordList()                         */
/************************************************************************/

char **NASAKeywordHandler::GetKeywordList()
{
    return aosKeywordList.List();
}

/************************************************************************/
/*                               StealJSon()                            */
/************************************************************************/

CPLJSONObject NASAKeywordHandler::GetJsonObject() const
{
    return oJSon;
}

//! @endcond
