/**********************************************************************
 * $Id$
 *
 * Project:  GML Reader
 * Purpose:  Implementation of GMLPropertyDefn
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
 * Revision 1.3  2006/05/25 02:35:15  fwarmerdam
 * capture maximum string length in scan pass (Peter Rushforth)
 *
 * Revision 1.2  2004/01/19 16:54:44  warmerda
 * added logic to capture field types
 *
 * Revision 1.1  2002/01/04 19:46:30  warmerda
 * New
 *
 *
 **********************************************************************/

#include "gmlreader.h"
#include "cpl_conv.h"

/************************************************************************/
/*                           GMLPropertyDefn                            */
/************************************************************************/

GMLPropertyDefn::GMLPropertyDefn( const char *pszName, 
                                  const char *pszSrcElement )

{
    m_pszName = CPLStrdup( pszName );
    if( pszSrcElement != NULL )
        m_pszSrcElement = CPLStrdup( pszSrcElement );
    else
        m_pszSrcElement = NULL;
    m_eType = GMLPT_Untyped;
    m_nWidth = 0; 
}

/************************************************************************/
/*                          ~GMLPropertyDefn()                          */
/************************************************************************/

GMLPropertyDefn::~GMLPropertyDefn()

{
    CPLFree( m_pszName );
    CPLFree( m_pszSrcElement );
}

/************************************************************************/
/*                           SetSrcElement()                            */
/************************************************************************/

void GMLPropertyDefn::SetSrcElement( const char *pszSrcElement )

{
    CPLFree( m_pszSrcElement );
    if( pszSrcElement != NULL )
        m_pszSrcElement = CPLStrdup( pszSrcElement );
    else
        m_pszSrcElement = NULL;
}

/************************************************************************/
/*                        AnalysePropertyValue()                        */
/*                                                                      */
/*      Examine the passed property value, and see if we need to        */
/*      make the field type more specific, or more general.             */
/************************************************************************/

void GMLPropertyDefn::AnalysePropertyValue( const char *pszValue )

{
/* -------------------------------------------------------------------- */
/*      We can't get more general than string, so at this point just    */
/*      give up on changing.                                            */
/* -------------------------------------------------------------------- */
    if( m_eType == GMLPT_String )
    {
        /* grow the Width to the length of the string passed in */
        int nWidth;
        nWidth = strlen(pszValue);
        if ( m_nWidth < nWidth ) 
            SetWidth( nWidth );
        return;
    }

/* -------------------------------------------------------------------- */
/*      If it is a zero length string, just return.  We can't deduce    */
/*      much from this.                                                 */
/* -------------------------------------------------------------------- */
    if( *pszValue == '\0' )
        return;

/* -------------------------------------------------------------------- */
/*      Does the string consist entirely of numeric values?             */
/* -------------------------------------------------------------------- */
    int bIsReal = FALSE;
    

    for(; *pszValue != '\0'; pszValue++ )
    {
        if( isdigit( *pszValue) || *pszValue == '-' || *pszValue == '+' 
            || isspace( *pszValue ) )
            /* do nothing */;
        else if( *pszValue == '.' || *pszValue == 'D' || *pszValue == 'd'
                 || *pszValue == 'E' || *pszValue == 'e' )
            bIsReal = TRUE;
        else 
        {
            m_eType = GMLPT_String;
            return;
        }
    }

    if( m_eType == GMLPT_Untyped || m_eType == GMLPT_Integer )
    {
        if( bIsReal )
            m_eType = GMLPT_Real;
        else
            m_eType = GMLPT_Integer;
    }
}

