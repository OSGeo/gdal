/******************************************************************************
 * $Id$
 *
 * Project:  OGR/DODS Interface
 * Purpose:  Implements OGRDODSFieldDefn class.  This is a small class used
 *           to encapsulate information about a referenced field. 
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.2  2004/01/29 21:01:03  warmerda
 * added sequences within sequences support
 *
 * Revision 1.1  2004/01/21 20:08:29  warmerda
 * New
 *
 */

#include "ogr_dods.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRDODSFieldDefn()                          */
/************************************************************************/

OGRDODSFieldDefn::OGRDODSFieldDefn()

{
    pszFieldName = NULL;
    pszFieldScope = NULL;
    iFieldIndex = -1;
    pszFieldValue = NULL;
    bValid = FALSE;
    pszPathToSequence = NULL;
}

/************************************************************************/
/*                         ~OGRDODSFieldDefn()                          */
/************************************************************************/

OGRDODSFieldDefn::~OGRDODSFieldDefn()

{
    CPLFree( pszFieldName );
    CPLFree( pszFieldScope );
    CPLFree( pszFieldValue );
    CPLFree( pszPathToSequence );
}

/************************************************************************/
/*                             Initialize()                             */
/*                                                                      */
/*      Build field reference from a DAS entry.  The AttrTable          */
/*      passed should be the container of the field defn.  For          */
/*      instance, the "x_field" node with a name and scope sub          */
/*      entry.                                                          */
/************************************************************************/

int OGRDODSFieldDefn::Initialize( AttrTable *poEntry )

{
    string o;

    pszFieldName = CPLStrdup(poEntry->get_attr("name").c_str());
    pszFieldScope = CPLStrdup(poEntry->get_attr("scope").c_str());

    if( EQUAL(pszFieldScope,"") )
    {
        CPLFree( pszFieldScope );
        pszFieldScope = CPLStrdup("dds");
    }

    bValid = EQUAL(pszFieldScope,"dds") || EQUAL(pszFieldScope,"das");

    return bValid;
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

int OGRDODSFieldDefn::Initialize( const char *pszFieldNameIn, 
                                  const char *pszFieldScopeIn )

{
    pszFieldName = CPLStrdup( pszFieldNameIn );
    pszFieldScope = CPLStrdup( pszFieldScopeIn );
    bValid = TRUE;

    return TRUE;
}
