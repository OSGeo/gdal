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
 * Revision 1.2  1999/07/05 17:19:52  warmerda
 * added docs
 *
 * Revision 1.1  1999/06/11 19:21:02  warmerda
 * New
 */

#include "ogr_feature.h"
#include "ogr_p.h"

/************************************************************************/
/*                            OGRFieldDefn()                            */
/************************************************************************/

/**
 * Constructor.
 *
 * @param pszNameIn the name of the new field.
 * @param eTypeIn the type of the new field.
 */

OGRFieldDefn::OGRFieldDefn( const char * pszNameIn, OGRFieldType eTypeIn )

{
    Initialize( pszNameIn, eTypeIn );
}

/************************************************************************/
/*                            OGRFieldDefn()                            */
/************************************************************************/

/**
 * Constructor.
 *
 * Create by cloning an existing field definition.
 *
 * @param poPrototype the field definition to clone.
 */

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

/**
 * Reset the name of this field.
 *
 * @param pszNameIn the new name to apply.
 */

void OGRFieldDefn::SetName( const char * pszNameIn )

{
    CPLFree( pszName );
    pszName = CPLStrdup( pszNameIn );
}

/************************************************************************/
/*                             GetNameRef()                             */
/************************************************************************/

/**
 * \fn const char *OGRFieldDefn::GetNameRef();
 *
 * Fetch name of this field.
 *
 * @return pointer to an internal name string that should not be freed or
 * modified.
 */

/************************************************************************/
/*                              GetType()                               */
/************************************************************************/

/**
 * \fn OGRFieldType OGRFieldDefn::GetType();
 *
 * Fetch type of this field.
 *
 * @return field type.
 */

/************************************************************************/
/*                              SetType()                               */
/************************************************************************/

/**
 * \fn void OGRFieldDefn::SetType( OGRFieldType eType );
 *
 * Set the type of this field.  This should never be done to an OGRFieldDefn
 * that is already part of an OGRFeatureDefn.
 *
 * @param eType the new field type.
 */

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

/**
 * Fetch human readable name for a field type.
 *
 * @param eType the field type to get name for.
 *
 * @return pointer to an internal static name string. It should not be
 * modified or freed.
 */

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

/************************************************************************/
/*                             GetJustify()                             */
/************************************************************************/

/**
 * \fn OGRJustification OGRFieldDefn::GetJustify();
 *
 * Get the justification for this field.
 *
 * @return the justification.
 */

/************************************************************************/
/*                             SetJustify()                             */
/************************************************************************/

/**
 * \fn void OGRFieldDefn::SetJustify( OGRJustification eJustify );
 *
 * Set the justification for this field.
 *
 * @param eJustify the new justification.
 */

/************************************************************************/
/*                              GetWidth()                              */
/************************************************************************/

/**
 * \fn int OGRFieldDefn::GetWidth();
 *
 * Get the formatting width for this field.
 *
 * @return the width, zero means no specified width. 
 */

/************************************************************************/
/*                              SetWidth()                              */
/************************************************************************/

/**
 * \fn void OGRFieldDefn::SetWidth( int nWidth );
 *
 * Set the formatting width for this field in characters.
 *
 * @param nWidth the new width.
 */

/************************************************************************/
/*                            GetPrecision()                            */
/************************************************************************/

/**
 * \fn int OGRFieldDefn::GetPrecision();
 *
 * Get the formatting precision for this field.  This should normally be
 * zero for fields of types other than OFTReal.
 *
 * @return the precision.
 */

/************************************************************************/
/*                            SetPrecision()                            */
/************************************************************************/

/**
 * \fn void OGRFieldDefn::SetPrecision( int nPrecision );
 *
 * Set the formatting precision for this field in characters.
 * 
 * This should normally be zero for fields of types other than OFTReal. 
 *
 * @param nPrecision the new precision. 
 */

