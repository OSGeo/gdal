/**********************************************************************
 * $Id$
 *
 * Project:  GML Reader
 * Purpose:  Implementation of GMLReadState class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2002, Frank Warmerdam
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************
 *
 * $Log$
 * Revision 1.3  2003/05/21 03:48:35  warmerda
 * Expand tabs
 *
 * Revision 1.2  2002/01/24 17:38:11  warmerda
 * added MatchPath
 *
 * Revision 1.1  2002/01/04 19:46:30  warmerda
 * New
 *
 *
 **********************************************************************/

#include "gmlreaderp.h"
#include "cpl_conv.h"
#include "cpl_string.h"

/************************************************************************/
/*                            GMLReadState()                            */
/************************************************************************/

GMLReadState::GMLReadState()

{
    m_poFeature = NULL;
    m_poParentState = NULL;

    m_pszPath = CPLStrdup("");
    m_nPathLength = 0;
    m_papszPathComponents = NULL;
}

/************************************************************************/
/*                           ~GMLReadState()                            */
/************************************************************************/

GMLReadState::~GMLReadState()

{
    CPLFree( m_pszPath );
    for( int i = 0; i < m_nPathLength; i++ )
        CPLFree( m_papszPathComponents[i] );
    CPLFree( m_papszPathComponents );
}

/************************************************************************/
/*                              PushPath()                              */
/************************************************************************/

void GMLReadState::PushPath( const char *pszElement )

{
    m_nPathLength++;
    m_papszPathComponents = CSLAddString( m_papszPathComponents, pszElement );

    RebuildPath();
}

/************************************************************************/
/*                              PopPath()                               */
/************************************************************************/

void GMLReadState::PopPath()

{
    CPLAssert( m_nPathLength > 0 );
    if( m_nPathLength <= 0 )
        return;

    CPLFree( m_papszPathComponents[m_nPathLength-1] );
    m_papszPathComponents[--m_nPathLength] = NULL;

    RebuildPath();
}

/************************************************************************/
/*                            RebuildPath()                             */
/************************************************************************/

void GMLReadState::RebuildPath()

{
    int   nLength=0, i;

    for( i = 0; i < m_nPathLength; i++ )
        nLength += strlen(m_papszPathComponents[i]) + 1;
    
    m_pszPath = (char *) CPLRealloc(m_pszPath, nLength );

    nLength = 0;
    for( i = 0; i < m_nPathLength; i++ )
    {
        if( i > 0 )
            m_pszPath[nLength++] = '|';

        strcpy( m_pszPath + nLength, m_papszPathComponents[i] );
        nLength += strlen(m_papszPathComponents[i]);
    }
}

/************************************************************************/
/*                          GetLastComponent()                          */
/************************************************************************/

const char *GMLReadState::GetLastComponent() const

{
    if( m_nPathLength == 0 )
        return "";
    else
        return m_papszPathComponents[m_nPathLength-1];
}

/************************************************************************/
/*                             MatchPath()                              */
/*                                                                      */
/*      Compare the passed in path to the current one and see if        */
/*      they match.  It is assumed that the passed in path may          */
/*      contain one or more elements and must match the tail of the     */
/*      current path.  However, the passed in path does not need all    */
/*      the components of the current read state path.                  */
/*                                                                      */
/*      Returns TRUE if the paths match.                                */
/************************************************************************/

int GMLReadState::MatchPath( const char *pszPathIn )

{
    int iOffset;
    int nInputLength = strlen(pszPathIn);
    int nInternalLength = strlen(m_pszPath);

    if( nInputLength > nInternalLength )
        return FALSE;

    iOffset = nInternalLength - nInputLength;
    if( iOffset > 0 && m_pszPath[iOffset-1] != '|' )
        return FALSE;

    return strcmp(pszPathIn,m_pszPath + iOffset) == 0;
}
