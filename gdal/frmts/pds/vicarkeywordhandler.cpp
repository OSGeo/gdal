/******************************************************************************
* 
* Project:  VICAR Driver; JPL/MIPL VICAR Format
* Purpose:  Implementation of VICARKeywordHandler - a class to read 
*           keyword data from VICAR data products.
* Author:   Sebastian Walter <sebastian dot walter at fu-berlin dot de>
*
* NOTE: This driver code is loosely based on the ISIS and PDS drivers.
* It is not intended to diminish the contribution of the authors 
******************************************************************************
* Copyright (c) 2014, Sebastian Walter <sebastian dot walter at fu-berlin dot de>
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
#include "vicarkeywordhandler.h"

/************************************************************************/
/* ==================================================================== */
/*                          VICARKeywordHandler                          */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                         VICARKeywordHandler()                         */
/************************************************************************/

VICARKeywordHandler::VICARKeywordHandler()

{
    papszKeywordList = NULL;
}

/************************************************************************/
/*                        ~VICARKeywordHandler()                         */
/************************************************************************/

VICARKeywordHandler::~VICARKeywordHandler()

{
    CSLDestroy( papszKeywordList );
    papszKeywordList = NULL;
}

/************************************************************************/
/*                               Ingest()                               */
/************************************************************************/

int VICARKeywordHandler::Ingest( VSILFILE *fp, GByte *pabyHeader )

{
/* -------------------------------------------------------------------- */
/*      Read in buffer till we find END all on it's own line.           */
/* -------------------------------------------------------------------- */
    if( VSIFSeekL( fp, 0, SEEK_SET ) != 0 )
        return FALSE;

    char *pch1, *pch2;
    char keyval[100];

    // Find LBLSIZE Entry
    char* pszLBLSIZE=strstr((char*)pabyHeader,"LBLSIZE");
    int nOffset = 0;

    if (pszLBLSIZE)
        nOffset = pszLBLSIZE - (const char *)pabyHeader;
    pch1=strstr((char*)pabyHeader+nOffset,"=");
    if( pch1 == NULL ) return FALSE;
    pch1 ++;
    pch2=strstr((char*)pabyHeader+nOffset," ");
    if( pch2 == NULL ) return FALSE;
    strncpy(keyval,pch1,MAX(pch2-pch1, 99));
    keyval[MAX(pch2-pch1, 99)] = 0;
    LabelSize=atoi(keyval);    
    if( LabelSize > 10 * 1024 * 124 )
        return FALSE;

    char* pszChunk = (char*) VSIMalloc(LabelSize+1);
    if( pszChunk == NULL )
        return FALSE;
    int nBytesRead = VSIFReadL( pszChunk, 1, LabelSize, fp );
    pszChunk[LabelSize] = 0;

    osHeaderText += pszChunk ;
    VSIFree(pszChunk);
    pszHeaderNext = osHeaderText.c_str();
    
/* -------------------------------------------------------------------- */
/*      Process name/value pairs, keeping track of a "path stack".      */
/* -------------------------------------------------------------------- */
    if( !ReadGroup("") )
        return FALSE;
/* -------------------------------------------------------------------- */
/*      Now check for the Vicar End-of-Dataset Label...                 */
/* -------------------------------------------------------------------- */
    const char *pszResult;

    pszResult = CSLFetchNameValue( papszKeywordList, "EOL" );
    
    if( pszResult == NULL )
        return FALSE;

    if( !EQUAL(pszResult,"1") ) 
        return TRUE;

/* -------------------------------------------------------------------- */
/*      There is a EOL!   e.G.  h4231_0000.nd4.06                       */
/* -------------------------------------------------------------------- */

    long int	    nLineOffset=0, nPixelOffset=0, nBandOffset=0;
    if (EQUAL( CSLFetchNameValue(papszKeywordList,"FORMAT" ), "BYTE" )) {
        nPixelOffset = 1;
    }
    else if (EQUAL( CSLFetchNameValue(papszKeywordList,"FORMAT" ), "HALF" )) {
        nPixelOffset = 2;
    }
    else if (EQUAL( CSLFetchNameValue(papszKeywordList,"FORMAT" ), "FULL" )) {
        nPixelOffset = 4;
    } 
    else if (EQUAL( CSLFetchNameValue(papszKeywordList,"FORMAT" ), "REAL" )) {
        nPixelOffset = 4;
    }
    long int nCols = atoi(CSLFetchNameValue(papszKeywordList,"NS")) ;
    long int nRows = atoi(CSLFetchNameValue(papszKeywordList,"NL")) ;
    int nBands = atoi(CSLFetchNameValue(papszKeywordList,"NB")) ;
    int nBB = atoi(CSLFetchNameValue(papszKeywordList,"NBB")) ;
    nLineOffset = nPixelOffset * nCols + nBB ;
    nBandOffset = nLineOffset * nRows;
    long int starteol = LabelSize + nBandOffset * nBands;
    if( VSIFSeekL( fp, starteol, SEEK_SET ) != 0 ) {
        printf("Error seeking to EOL!\n");
        return FALSE;
        }
    char szChunk[100];
    nBytesRead = VSIFReadL( szChunk, 1, 30, fp );
    szChunk[nBytesRead] = '\0';
    pszLBLSIZE=strstr(szChunk,"LBLSIZE");
    nOffset=0;
    
    if (pszLBLSIZE)
        nOffset = pszLBLSIZE - (const char *)szChunk;
    pch1=strstr((char*)szChunk+nOffset,"=")+1;
    pch2=strstr((char*)szChunk+nOffset," ");
    strncpy(keyval,pch1,pch2-pch1);
    int EOLabelSize=atoi(keyval);
    if( EOLabelSize > 99 )
        EOLabelSize = 99;
    if( VSIFSeekL( fp, starteol, SEEK_SET ) != 0 ) {
        printf("Error seeking again to EOL!\n");
        return FALSE;
        }
    nBytesRead = VSIFReadL( szChunk, 1, EOLabelSize, fp );
    szChunk[nBytesRead] = '\0';
    osHeaderText += szChunk ;
    osHeaderText.append("END");
    pszHeaderNext = osHeaderText.c_str();
    return ReadGroup( "" );
    
}

/************************************************************************/
/*                             ReadGroup()                              */
/************************************************************************/

int VICARKeywordHandler::ReadGroup( CPL_UNUSED const char *pszPathPrefix ) {
    CPLString osName, osValue, osProperty;

    for( ; TRUE; ) {
        if( !ReadPair( osName, osValue ) ) {
        //printf("Could not... \n");
                return FALSE;
                }
            if( EQUAL(osName,"END") )
            return TRUE;
            else if( EQUAL(osName,"PROPERTY") || EQUAL(osName,"HISTORY") || EQUAL(osName,"TASK"))
                osProperty = osValue;
        else {
            if ( !EQUAL(osProperty,"") )
                osName = osProperty + "." + osName;
            papszKeywordList = CSLSetNameValue( papszKeywordList, osName, osValue );
        }
    }
}


/************************************************************************/
/*                              ReadPair()                              */
/*                                                                      */
/*      Read a name/value pair from the input stream.  Strip off        */
/*      white space, ignore comments, split on '='.                     */
/*      Returns TRUE on success.                                        */
/************************************************************************/

int VICARKeywordHandler::ReadPair( CPLString &osName, CPLString &osValue ) {

    osName = "";
    osValue = "";

    if( !ReadWord( osName ) ) {
    return FALSE;}

    SkipWhite();
    
    // VICAR has no NULL string termination
    if( *pszHeaderNext == '\0') {
        osName="END";
        return TRUE;
    }

    pszHeaderNext++;
    
    SkipWhite();
    
    osValue = "";

    if( *pszHeaderNext == '(' && pszHeaderNext[1] == '\'' )
    {
        CPLString osWord;
        while( ReadWord( osWord ) )
        {
            osValue += osWord;
            if ( strlen(osWord) < 2 ) continue;
            if( osWord[strlen(osWord)-1] == ')' && osWord[strlen(osWord)-2] == '\'' ) break;
        }
    }

    else if( *pszHeaderNext == '(' && *pszHeaderNext-1 != '\'' )
    {
        CPLString osWord;

        while( ReadWord( osWord ) )
        {
            SkipWhite();

            osValue += osWord;
            if( osWord[strlen(osWord)-1] == ')'  ) break;
        }
    }

    else
    {
        if( !ReadWord( osValue ) ) return FALSE;

    }
        
    SkipWhite();
    
    return TRUE;
}

/************************************************************************/
/*                              ReadWord()                              */
/*  Returns TRUE on success                                             */
/************************************************************************/

int VICARKeywordHandler::ReadWord( CPLString &osWord )

{
    osWord = "";

    SkipWhite();

    if( *pszHeaderNext == '\0') return TRUE;
    
    if( !( *pszHeaderNext != '='  && !isspace((unsigned char)*pszHeaderNext)) ) {
    return FALSE;
    }

    if( *pszHeaderNext == '\'' )
    {
        pszHeaderNext++;
        while( *pszHeaderNext != '\'' )
        {
            //Skip Double Quotes
                        if( *pszHeaderNext+1 == '\'' ) continue;
            osWord += *(pszHeaderNext++);
        }
        pszHeaderNext++;
        return TRUE;
    }

    while( *pszHeaderNext != '=' && !isspace((unsigned char)*pszHeaderNext) )
    {
        osWord += *pszHeaderNext;
        pszHeaderNext++;

    }
    
    return TRUE;
}

/************************************************************************/
/*                             SkipWhite()                              */
/*  Skip white spaces and C style comments                              */
/************************************************************************/

void VICARKeywordHandler::SkipWhite()

{
    for( ; TRUE; )
    {
        if( isspace( (unsigned char)*pszHeaderNext ) )
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

const char *VICARKeywordHandler::GetKeyword( const char *pszPath, const char *pszDefault )

{
    const char *pszResult;

    pszResult = CSLFetchNameValue( papszKeywordList, pszPath );
    if( pszResult == NULL ){
        return pszDefault;}
    else
        return pszResult;
}

/************************************************************************/
/*                             GetKeywordList()                         */
/************************************************************************/

char **VICARKeywordHandler::GetKeywordList()
{
    return papszKeywordList;
}

