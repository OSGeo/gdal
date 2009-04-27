/******************************************************************************
 * $Id: pdsdataset.cpp 12658 2007-11-07 23:14:33Z warmerdam $
 *
 * Project:  PDS Driver; Planetary Data System Format
 * Purpose:  Implementation of NASAKeywordHandler - a class to read 
 *           keyword data from PDS, ISIS2 and ISIS3 data products.
 * Author:   Frank Warmerdam <warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam <warmerdam@pobox.com>
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
#include "nasakeywordhandler.h"

/************************************************************************/
/* ==================================================================== */
/*                          NASAKeywordHandler                          */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                         NASAKeywordHandler()                         */
/************************************************************************/

NASAKeywordHandler::NASAKeywordHandler()

{
    papszKeywordList = NULL;
}

/************************************************************************/
/*                        ~NASAKeywordHandler()                         */
/************************************************************************/

NASAKeywordHandler::~NASAKeywordHandler()

{
    CSLDestroy( papszKeywordList );
    papszKeywordList = NULL;
}

/************************************************************************/
/*                               Ingest()                               */
/************************************************************************/

int NASAKeywordHandler::Ingest( FILE *fp, int nOffset )

{
/* -------------------------------------------------------------------- */
/*      Read in buffer till we find END all on it's own line.           */
/* -------------------------------------------------------------------- */
    if( VSIFSeekL( fp, nOffset, SEEK_SET ) != 0 )
        return FALSE;

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

        if( strstr(pszCheck,"\r\nEND\r\n") != NULL 
            || strstr(pszCheck,"\nEND\n") != NULL 
            || strstr(pszCheck,"\r\nEnd\r\n") != NULL 
            || strstr(pszCheck,"\nEnd\n") != NULL )
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

int NASAKeywordHandler::ReadGroup( const char *pszPathPrefix )

{
    CPLString osName, osValue;

    for( ; TRUE; )
    {
        if( !ReadPair( osName, osValue ) )
            return FALSE;

        if( EQUAL(osName,"OBJECT") || EQUAL(osName,"GROUP") )
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

int NASAKeywordHandler::ReadPair( CPLString &osName, CPLString &osValue )

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
    if( *pszHeaderNext == '(' )
    {
        CPLString osWord;

        while( ReadWord( osWord ) )
        {
            SkipWhite();

            osValue += osWord;
            if( osWord[strlen(osWord)-1] == ')' )
                break;
        }
    }

    // Handle value lists like:     Name   = {Red, Red}
    else if( *pszHeaderNext == '{' )
    {
        CPLString osWord;

        while( ReadWord( osWord ) )
        {
            SkipWhite();

            osValue += osWord;
            if( osWord[strlen(osWord)-1] == '}' )
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

int NASAKeywordHandler::ReadWord( CPLString &osWord )

{
    osWord = "";

    SkipWhite();

    if( !(*pszHeaderNext != '\0' 
           && *pszHeaderNext != '=' 
           && !isspace((unsigned char)*pszHeaderNext)) )
        return FALSE;

    while( *pszHeaderNext != '\0' 
           && *pszHeaderNext != '=' 
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
    
    return TRUE;
}

/************************************************************************/
/*                             SkipWhite()                              */
/************************************************************************/

void NASAKeywordHandler::SkipWhite()

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

const char *NASAKeywordHandler::GetKeyword( const char *pszPath,
                                            const char *pszDefault )

{
    const char *pszResult;

    pszResult = CSLFetchNameValue( papszKeywordList, pszPath );
    if( pszResult == NULL )
        return pszDefault;
    else
        return pszResult;
}

