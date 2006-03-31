/******************************************************************************
 * $Id$
 *
 * Project: OpenGIS Simple Features Reference Implementation
 * Purpose: Simple Features Provider Enumerator (SFCEnumerator) implementation.
 * Author:  Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.3  2006/03/31 17:44:20  fwarmerdam
 * header updates
 *
 * Revision 1.2  1999/07/07 19:39:20  warmerda
 * added OpenAny()
 *
 * Revision 1.1  1999/06/08 03:51:00  warmerda
 * New
 *
 */

#include "sfcenumerator.h"
#include "sfcdatasource.h"
#include <stdio.h>

/************************************************************************/
/*                           IsOGISProvider()                           */
/************************************************************************/

/**
 * Establish if the current provider (record) is OGIS complaint. 
 *
 * The classid of the current provider record is looked up in the 
 * registry to see if OGISDataProvider is an implemented category for
 * the provider.
 */

int SFCEnumerator::IsOGISProvider()

{
    long      lResult;
    char      szRegPath[512];
    HKEY      hKey;

    sprintf( szRegPath, 
             "CLSID\\%S\\Implemented Categories\\"
             "{A0690A28-FAF5-11D1-BAF5-080036DB0B03}", //CATID_OGISDataProvider
             m_szParseName );

    lResult = RegOpenKeyEx( HKEY_CLASSES_ROOT, szRegPath, 0L, KEY_READ,
                            &hKey );

    if( lResult == ERROR_SUCCESS )
    {
        RegCloseKey( hKey );
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/************************************************************************/
/*                              OpenAny()                               */
/************************************************************************/

/**
 * Try to open a datasource with any of the available providers till it
 * works. 
 *
 * @param pszDataSource the name of the data source to try and open.
 *
 * @return the data source opened, or NULL on failure.  
 */

SFCDataSource * SFCEnumerator::OpenAny( const char * pszDataSource )

{
/* -------------------------------------------------------------------- */
/*      Reset to the beginning, and make a pass trying OpenGIS          */
/*      providers.                                                      */
/* -------------------------------------------------------------------- */
    Close();
    Open();

    while( MoveNext() == S_OK )
    {
        SFCDataSource      *poDS;

        if( !IsOGISProvider() )
            continue;

        poDS = new SFCDataSource();
        if( !FAILED(poDS->Open( *this, pszDataSource )) )
            return poDS;

        delete poDS;
    }
    
/* -------------------------------------------------------------------- */
/*      Reset to the beginning, and make a pass trying non OpenGIS      */
/*      providers.                                                      */
/* -------------------------------------------------------------------- */
    Close();
    Open();

    while( MoveNext() == S_OK )
    {
        SFCDataSource      *poDS;

        if( IsOGISProvider() )
            continue;

        poDS = new SFCDataSource();
        if( !FAILED(poDS->Open( *this, pszDataSource )) )
            return poDS;

        delete poDS;
    }
    
    return NULL;
}
