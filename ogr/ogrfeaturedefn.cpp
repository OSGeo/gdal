/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRFeatureDefn class implementation.
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
 * Revision 1.7  2002/09/26 18:12:38  warmerda
 * added C support
 *
 * Revision 1.6  2002/08/07 21:37:47  warmerda
 * added indirect OGRFeaturedefn constructor/destructor
 *
 * Revision 1.5  2001/07/18 05:03:05  warmerda
 * added CPL_CVSID
 *
 * Revision 1.4  1999/10/01 14:46:38  warmerda
 * don't blow assertion trying to get non-existant fields
 *
 * Revision 1.3  1999/08/28 03:12:06  warmerda
 * Improve debug message for left over reference count message.
 *
 * Revision 1.2  1999/07/05 17:19:52  warmerda
 * added docs
 *
 * Revision 1.1  1999/06/11 19:21:02  warmerda
 * New
 */

#include "ogr_feature.h"
#include "ogr_api.h"
#include "ogr_p.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGRFeatureDefn()                           */
/************************************************************************/

/**
 * Constructor
 *
 * The OGRFeatureDefn maintains a reference count, but this starts at
 * zero.  It is mainly intended to represent a count of OGRFeature's
 * based on this definition.
 *
 * This method is the same as the C function OGR_FD_Create().
 *
 * @param pszName the name to be assigned to this layer/class.  It does not
 * need to be unique. 
 */

OGRFeatureDefn::OGRFeatureDefn( const char * pszName )

{
    pszFeatureClassName = CPLStrdup( pszName );
    nRefCount = 0;
    nFieldCount = 0;
    papoFieldDefn = NULL;
    eGeomType = wkbUnknown;
}

/************************************************************************/
/*                           OGR_FD_Create()                            */
/************************************************************************/

OGRFeatureDefnH OGR_FD_Create( const char *pszName )

{
    return (OGRFeatureDefnH) new OGRFeatureDefn( pszName );
}


/************************************************************************/
/*                          ~OGRFeatureDefn()                           */
/************************************************************************/

OGRFeatureDefn::~OGRFeatureDefn()

{
    if( nRefCount != 0 )
    {
        CPLDebug( "OGRFeatureDefn",
                  "OGRFeatureDefn %s with a ref count of %d deleted!\n",
                  pszFeatureClassName, nRefCount );
    }
    
    CPLFree( pszFeatureClassName );

    for( int i = 0; i < nFieldCount; i++ )
    {
        delete papoFieldDefn[i];
    }

    CPLFree( papoFieldDefn );
}

/************************************************************************/
/*                           OGR_FD_Destroy()                           */
/************************************************************************/

void OGR_FD_Destroy( OGRFeatureDefnH hDefn )

{
    delete (OGRFeatureDefn *) hDefn;
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

/**
 * \fn const char *OGRFeatureDefn::GetName();
 *
 * Get name of this OGRFeatureDefn.
 *
 * This method is the same as the C function OGR_FD_GetName().
 *
 * @return the name.  This name is internal and should not be modified, or
 * freed.
 */

/************************************************************************/
/*                           OGR_FD_GetName()                           */
/************************************************************************/

const char *OGR_FD_GetName( OGRFeatureDefnH hDefn )

{
    return ((OGRFeatureDefn *) hDefn)->GetName();
}

/************************************************************************/
/*                           GetFieldCount()                            */
/************************************************************************/

/**
 * \fn int OGRFeatureDefn::GetFieldCount();
 *
 * Fetch number of fields on this feature.
 *
 * This method is the same as the C function OGR_FD_GetFieldCount().
 * @return count of fields.
 */

/************************************************************************/
/*                        OGR_FD_GetFieldCount()                        */
/************************************************************************/

int OGR_FD_GetFieldCount( OGRFeatureDefnH hDefn )

{
    return ((OGRFeatureDefn *) hDefn)->GetFieldCount();
}

/************************************************************************/
/*                            GetFieldDefn()                            */
/************************************************************************/

/**
 * Fetch field definition.
 *
 * This method is the same as the C function OGR_FD_GetFieldDefn().
 *
 * @param iField the field to fetch, between 0 and GetFieldCount()-1.
 *
 * @return a pointer to an internal field definition object.  This object
 * should not be modified or freed by the application.
 */

OGRFieldDefn *OGRFeatureDefn::GetFieldDefn( int iField )

{
    if( iField < 0 || iField >= nFieldCount )
    {
        return NULL;
    }

    return papoFieldDefn[iField];
}

/************************************************************************/
/*                        OGR_FD_GetFieldDefn()                         */
/************************************************************************/

OGRFieldDefnH OGR_FD_GetFieldDefn( OGRFeatureDefnH hDefn, int iField )

{
    return ((OGRFeatureDefn *) hDefn)->GetFieldDefn( iField );
}

/************************************************************************/
/*                            AddFieldDefn()                            */
/************************************************************************/

/**
 * Add a new field definition.
 *
 * This method should only be called while there are no OGRFeature
 * objects in existance based on this OGRFeatureDefn.  The OGRFieldDefn
 * passed in is copied, and remains the responsibility of the caller.
 *
 * This method is the same as the C function OGR_FD_AddFieldDefn().
 *
 * @param poNewDefn the definition of the new field.
 */

void OGRFeatureDefn::AddFieldDefn( OGRFieldDefn * poNewDefn )

{
    papoFieldDefn = (OGRFieldDefn **)
        CPLRealloc( papoFieldDefn, sizeof(void*)*(nFieldCount+1) );

    papoFieldDefn[nFieldCount] = new OGRFieldDefn( poNewDefn );
    nFieldCount++;
}

/************************************************************************/
/*                        OGR_FD_AddFieldDefn()                         */
/************************************************************************/

void OGR_FD_AddFieldDefn( OGRFeatureDefnH hDefn, OGRFieldDefnH hNewField )

{
    ((OGRFeatureDefn *) hDefn)->AddFieldDefn( (OGRFieldDefn *) hNewField );
}

/************************************************************************/
/*                            GetGeomType()                             */
/************************************************************************/

/**
 * Fetch the geometry base type.
 *
 * This method is the same as the C functin OGR_FD_GetGeomType().
 *
 * @return the base type for all geometry related to this definition.
 */

/************************************************************************/
/*                         OGR_FD_GetGeomType()                         */
/************************************************************************/

OGRwkbGeometryType OGR_FD_GetGeomType( OGRFieldDefnH hDefn )

{
    return ((OGRFeatureDefn *) hDefn)->GetGeomType();
}

/************************************************************************/
/*                            SetGeomType()                             */
/************************************************************************/

/**
 * Assign the base geometry type for this layer.
 *
 * All geometry objects using this type must be of the defined type or
 * a derived type.  The default upon creation is wkbUnknown which allows for
 * any geometry type.  The geometry type should generally not be changed
 * after any OGRFeatures have been created against this definition. 
 *
 * This method is the same as the C function OGR_FD_SetGeomType().
 *
 * @param eNewType the new type to assign.
 */

void OGRFeatureDefn::SetGeomType( OGRwkbGeometryType eNewType )

{
    eGeomType = eNewType;
}

/************************************************************************/
/*                         OGR_FD_SetGeomType()                         */
/************************************************************************/

void OGR_FD_SetGeomType( OGRFeatureDefnH hDefn, OGRwkbGeometryType eType )

{
    ((OGRFeatureDefn *) hDefn)->SetGeomType( eType );
}


/************************************************************************/
/*                             Reference()                              */
/************************************************************************/

/**
 * \fn int OGRFeatureDefn::Reference();
 * 
 * Increments the reference count by one.
 *
 * The reference count is used keep track of the number of OGRFeature
 * objects referencing this definition. 
 *
 * This method is the same as the C function OGR_FD_Reference().
 *
 * @return the updated reference count.
 */

/************************************************************************/
/*                          OGR_FD_Reference()                          */
/************************************************************************/

int OGR_FD_Reference( OGRFeatureDefnH hDefn )

{
    return ((OGRFeatureDefn *) hDefn)->Reference();
}

/************************************************************************/
/*                            Dereference()                             */
/************************************************************************/

/**
 * \fn int OGRFeatureDefn::Dereference();
 *
 * Decrements the reference count by one.
 *
 * This method is the same as the C function OGR_FD_Dereference().
 *
 * @return the updated reference count.
 */

/************************************************************************/
/*                         OGR_FD_Dereference()                         */
/************************************************************************/

int OGR_FD_Dereference( OGRFeatureDefnH hDefn )

{
    return ((OGRFeatureDefn *) hDefn)->Dereference();
}

/************************************************************************/
/*                         GetReferenceCount()                          */
/************************************************************************/

/**
 * \fn int OGRFeatureDefn::GetReferenceCount();
 *
 * Fetch current reference count.
 *
 * This method is the same as the C function OGR_FD_GetReferenceCount().
 *
 * @return the current reference count.
 */

/************************************************************************/
/*                      OGR_FD_GetReferenceCount()                      */
/************************************************************************/

int OGR_FD_GetReferenceCount( OGRFeatureDefnH hDefn )

{
    return ((OGRFeatureDefn *) hDefn)->GetReferenceCount();
}

/************************************************************************/
/*                           GetFieldIndex()                            */
/************************************************************************/

/**
 * Find field by name.
 *
 * The field index of the first field matching the passed field name (case
 * insensitively) is returned.
 *
 * This method is the same as the C function OGR_FD_GetFieldIndex().
 *
 * @param pszFieldName the field name to search for.
 *
 * @return the field index, or -1 if no match found.
 */
 

int OGRFeatureDefn::GetFieldIndex( const char * pszFieldName )

{
    for( int i = 0; i < nFieldCount; i++ )
    {
        if( EQUAL(pszFieldName, papoFieldDefn[i]->GetNameRef() ) )
            return i;
    }

    return -1;
}

/************************************************************************/
/*                        OGR_FD_GetFieldIndex()                        */
/************************************************************************/

int OGR_FD_GetFieldIndex( OGRFeatureDefnH hDefn, const char *pszFieldName )

{
    return ((OGRFeatureDefn *)hDefn)->GetFieldIndex( pszFieldName );
}

/************************************************************************/
/*                         CreateFeatureDefn()                          */
/************************************************************************/

OGRFeatureDefn *OGRFeatureDefn::CreateFeatureDefn( const char *pszName )

{
    return new OGRFeatureDefn( pszName );
}

/************************************************************************/
/*                         DestroyFeatureDefn()                         */
/************************************************************************/

void OGRFeatureDefn::DestroyFeatureDefn( OGRFeatureDefn *poDefn )

{
    delete poDefn;
}
