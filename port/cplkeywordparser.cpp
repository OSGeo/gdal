/******************************************************************************
 * $Id$
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

#include "cpl_string.h" 
#include "cplkeywordparser.h"

CPL_CVSID("$Id");

/************************************************************************/
/* ==================================================================== */
/*                          CPLKeywordParser                           */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                         CPLKeywordParser()                          */
/************************************************************************/

CPLKeywordParser::CPLKeywordParser()

{
    papszKeywordList = NULL;
}

/************************************************************************/
/*                        ~CPLKeywordParser()                          */
/************************************************************************/

CPLKeywordParser::~CPLKeywordParser()

{
    CSLDestroy( papszKeywordList );
    papszKeywordList = NULL;
}

/************************************************************************/
/*                               Ingest()                               */
/************************************************************************/

int CPLKeywordParser::Ingest( VSILFILE *fp )

{
/* -------------------------------------------------------------------- */
/*      Read in buffer till we find END all on it's own line.           */
/* -------------------------------------------------------------------- */
    for( ; TRUE; ) 
    {
        const char *pszCheck;
        char szChunk[513];

        int nBytesRead = VSIFReadL( szChunk, 1, 512, fp );

        szChunk[nBytesRead] = '\0';
        osHeaderText += szChunk;

        if( nBytesRead < 512 )
            break;

        if( osHeaderText.size() > 520 )
            pszCheck = osHeaderText.c_str() + (osHeaderText.size() - 520);
        else
            pszCheck = szChunk;

        if( strstr(pszCheck,"\r\nEND;\r\n") != NULL 
            || strstr(pszCheck,"\nEND;\n") != NULL )
            break;
    }

    pszHeaderNext = osHeaderText.c_str();

/* -------------------------------------------------------------------- */
/*      Process name/value pairs, keeping track of a "path stack".      */
/* -------------------------------------------------------------------- */
    return ReadGroup( "" );
}

/************************************************************************/
/*                             ReadGroup()                              */
/************************************************************************/

int CPLKeywordParser::ReadGroup( const char *pszPathPrefix )

{
    CPLString osName, osValue;

    for( ; TRUE; )
    {
        if( !ReadPair( osName, osValue ) )
            return FALSE;

        if( EQUAL(osName,"BEGIN_GROUP") )
        {
            if( !ReadGroup( (CPLString(pszPathPrefix) + osValue + ".").c_str() ) )
                return FALSE;
        }
        else if( EQUALN(osName,"END",3) )
        {
            return TRUE;
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

int CPLKeywordParser::ReadPair( CPLString &osName, CPLString &osValue )

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
        else
            return FALSE;
    }
    
    pszHeaderNext++;
    
    SkipWhite();
    
    osValue = "";

    // Handle value lists like:     Name   = (Red, Red)
    // or list of lists like : TLCList = ( (0,  0.000000), (8299,  4.811014) );
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
            int bInQuote = FALSE;
            while(*pszIter != '\0')
            {
                if (*pszIter == '"')
                    bInQuote = !bInQuote;
                else if (!bInQuote)
                {
                    if (*pszIter == '(')
                        nDepth ++;
                    else if (*pszIter == ')')
                    {
                        nDepth --;
                        if (nDepth == 0)
                            break;
                    }
                }
                pszIter ++;
            }
            if (*pszIter == ')' && nDepth == 0)
                break;
        }
    }

    else // Handle more normal "single word" values. 
    {
        if( !ReadWord( osValue ) )
            return FALSE;

    }
        
    SkipWhite();

    // No units keyword?   
    if( *pszHeaderNext != '<' )
        return TRUE;

    // Append units keyword.  For lines that like like this:
    //  MAP_RESOLUTION               = 4.0 <PIXEL/DEGREE>
    
    CPLString osWord;
    
    osValue += " ";
    
    while( ReadWord( osWord ) )
    {
        SkipWhite();
        
        osValue += osWord;
        if( osWord[strlen(osWord)-1] == '>' )
            break;
    }
    
    return TRUE;
}

/************************************************************************/
/*                              ReadWord()                              */
/************************************************************************/

int CPLKeywordParser::ReadWord( CPLString &osWord )

{
    osWord = "";

    SkipWhite();

    if( *pszHeaderNext == '\0' )
        return FALSE;

    while( *pszHeaderNext != '\0' 
           && *pszHeaderNext != '=' 
           && *pszHeaderNext != ';'
           && !isspace((unsigned char)*pszHeaderNext) )
    {
        if( *pszHeaderNext == '"' )
        {
            osWord += *(pszHeaderNext++);
            while( *pszHeaderNext != '"' )
            {
                if( *pszHeaderNext == '\0' )
                    return FALSE;

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
                    return FALSE;

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
    
    return TRUE;
}

/************************************************************************/
/*                             SkipWhite()                              */
/************************************************************************/

void CPLKeywordParser::SkipWhite()

{
    for( ; TRUE; )
    {
        // Skip white space (newline, space, tab, etc )
        if( isspace( (unsigned char)*pszHeaderNext ) )
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

            pszHeaderNext += 2;
            continue;
        }

        // Skip # style comments 
        if( *pszHeaderNext == '#'  )
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
    const char *pszResult;

    pszResult = CSLFetchNameValue( papszKeywordList, pszPath );
    if( pszResult == NULL )
        return pszDefault;
    else
        return pszResult;
}
