/******************************************************************************
 * $Id$
 *
 * Project:  NTF Translator
 * Purpose:  NTFCodeList class implementation.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
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

#include <stdarg.h>
#include "ntf.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                             NTFCodeList                              */
/************************************************************************/

NTFCodeList::NTFCodeList( NTFRecord * poRecord )

{
    int         iThisField;
    const char  *pszText;

    CPLAssert( EQUAL(poRecord->GetField(1,2),"42") );
    
    strcpy( szValType, poRecord->GetField(13,14) );
    strcpy( szFInter, poRecord->GetField(15,19) );

    nNumCode = atoi(poRecord->GetField(20,22));

    papszCodeVal = (char **) CPLMalloc(sizeof(char*) * nNumCode );
    papszCodeDes = (char **) CPLMalloc(sizeof(char*) * nNumCode );

    pszText = poRecord->GetData() + 22;
    for( iThisField=0; 
         *pszText != '\0' && iThisField < nNumCode; 
         iThisField++ )
    {
        char    szVal[128], szDes[128];
        int     iLen;

        iLen = 0;
        while( *pszText != '\\' && *pszText != '\0' )
            szVal[iLen++] = *(pszText++);
        szVal[iLen] = '\0';
        
        if( *pszText == '\\' )
            pszText++;
        
        iLen = 0;
        while( *pszText != '\\' && *pszText != '\0' )
            szDes[iLen++] = *(pszText++);
        szDes[iLen] = '\0';

        if( *pszText == '\\' )
            pszText++;

        papszCodeVal[iThisField] = CPLStrdup(szVal);
        papszCodeDes[iThisField] = CPLStrdup(szDes);
    }

    if( iThisField < nNumCode )
    {
        nNumCode = iThisField;
        CPLDebug( "NTF", 
                  "Didn't get all the expected fields from a CODELIST." );
    }
}

/************************************************************************/
/*                            ~NTFCodeList()                            */
/************************************************************************/

NTFCodeList::~NTFCodeList()

{
    for( int i = 0; i < nNumCode; i++ )
    {
        CPLFree( papszCodeVal[i] );
        CPLFree( papszCodeDes[i] );
    }

    CPLFree( papszCodeVal );
    CPLFree( papszCodeDes );
}

/************************************************************************/
/*                               Lookup()                               */
/************************************************************************/

const char *NTFCodeList::Lookup( const char * pszCode )

{
    for( int i = 0; i < nNumCode; i++ )
    {
        if( EQUAL(pszCode,papszCodeVal[i]) )
            return papszCodeDes[i];
    }

    return NULL;
}


