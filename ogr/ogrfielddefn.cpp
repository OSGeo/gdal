/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRFieldDefn class implementation.
 * Author:   Frank Warmerdam, warmerda@home.com
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
 * Revision 1.1  1999/06/11 19:21:02  warmerda
 * New
 *
 */

#include "ogr_feature.h"
#include "ogr_p.h"

/************************************************************************/
/*                            OGRFieldDefn()                            */
/************************************************************************/

OGRFieldDefn::OGRFieldDefn( const char * pszNameIn, OGRFieldType eTypeIn )

{
    Initialize( pszNameIn, eTypeIn );
}

/************************************************************************/
/*                            OGRFieldDefn()                            */
/************************************************************************/

OGRFieldDefn::OGRFieldDefn( OGRFieldDefn *poPrototype )

{
    Initialize( poPrototype->GetNameRef(), poPrototype->GetType() );

    SetJustify( poPrototype->GetJustify() );
    SetWidth( poPrototype->GetWidth() );
    SetPrecision( poPrototype->GetPrecision() );
//    SetDefault( poPrototype->GetDefaultRef() );
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

void OGRFieldDefn::Initialize( const char * pszNameIn, OGRFieldType eTypeIn )

{
    pszName = CPLStrdup( pszNameIn );
    eType = eTypeIn;
    eJustify = OJUndefined;

    nWidth = 0;		// should these be defined in some particular way
    nPrecision = 0;	// for numbers?

    memset( &uDefault, 0, sizeof(OGRField) );
}

/************************************************************************/
/*                           ~OGRFieldDefn()                            */
/************************************************************************/

OGRFieldDefn::~OGRFieldDefn()

{
    CPLFree( pszName );
}

/************************************************************************/
/*                              SetName()                               */
/************************************************************************/

void OGRFieldDefn::SetName( const char * pszNameIn )

{
    CPLFree( pszName );
    pszName = CPLStrdup( pszNameIn );
}

/************************************************************************/
/*                             SetDefault()                             */
/************************************************************************/

void OGRFieldDefn::SetDefault( const OGRField * puDefaultIn )

{
    switch( eType )
    {
      case OFTInteger:
      case OFTReal:
        uDefault = *puDefaultIn;
        break;

      case OFTString:
//        CPLFree( uDefault.String );
//        uDefault.String = CPLStrdup( puDefaultIn->String );
        break;

      default:
        // add handling for other complex types.
        CPLAssert( FALSE );
        break;
    }
}

/************************************************************************/
/*                          GetFieldTypeName()                          */
/************************************************************************/

const char * OGRFieldDefn::GetFieldTypeName( OGRFieldType eType )

{
    switch( eType )
    {
      case OFTInteger:
        return "Integer";

      case OFTReal:
        return "Real";

      case OFTString:
        return "String";

      case OFTWideString:
        return "WideString";

      case OFTIntegerList:
        return "IntegerList";

      case OFTRealList:
        return "RealList";

      case OFTStringList:
        return "StringList";

      case OFTWideStringList:
        return "WideStringList";

      case OFTBinary:
        return "Binary";

      default:
        CPLAssert( FALSE );
        return "(unknown)";
    }
}
