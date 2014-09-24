/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRFeatureDefn class implementation.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_feature.h"
#include "ogr_api.h"
#include "ogr_p.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGRFeatureDefn()                           */
/************************************************************************/

/**
 * \brief Constructor.
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
    nGeomFieldCount = 1;
    papoGeomFieldDefn = (OGRGeomFieldDefn**) CPLMalloc(sizeof(OGRGeomFieldDefn*));
    papoGeomFieldDefn[0] = new OGRGeomFieldDefn("", wkbUnknown);
    bIgnoreStyle = FALSE;
}

/************************************************************************/
/*                           OGR_FD_Create()                            */
/************************************************************************/
/**
 * \brief Create a new feature definition object to hold the field definitions.
 *
 * The OGRFeatureDefn maintains a reference count, but this starts at
 * zero, and should normally be incremented by the owner.
 *
 * This function is the same as the C++ method 
 * OGRFeatureDefn::OGRFeatureDefn().
 *
 * @param pszName the name to be assigned to this layer/class.  It does not
 * need to be unique. 
 * @return handle to the newly created feature definition.
 */

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

    for( int i = 0; i < nGeomFieldCount; i++ )
    {
        delete papoGeomFieldDefn[i];
    }

    CPLFree( papoGeomFieldDefn );
}

/************************************************************************/
/*                           OGR_FD_Destroy()                           */
/************************************************************************/
/**
 * \brief Destroy a feature definition object and release all memory associated with it. 
 *
 * This function is the same as the C++ method 
 * OGRFeatureDefn::~OGRFeatureDefn().
 *
 * @param hDefn handle to the feature definition to be destroyed.
 */

void OGR_FD_Destroy( OGRFeatureDefnH hDefn )

{
    delete (OGRFeatureDefn *) hDefn;
}

/************************************************************************/
/*                              Release()                               */
/************************************************************************/

/**
 * \fn void OGRFeatureDefn::Release();
 *
 * \brief Drop a reference to this object, and destroy if no longer referenced.
 */

void OGRFeatureDefn::Release()

{
    CPLAssert( NULL != this );

    if( Dereference() <= 0 )
        delete this;
}

/************************************************************************/
/*                           OGR_FD_Release()                           */
/************************************************************************/

/**
 * \brief Drop a reference, and destroy if unreferenced.
 *
 * This function is the same as the C++ method OGRFeatureDefn::Release().
 *
 * @param hDefn handle to the feature definition to be released.
 */

void OGR_FD_Release( OGRFeatureDefnH hDefn )

{
    ((OGRFeatureDefn *) hDefn)->Release();
}

/************************************************************************/
/*                               Clone()                                */
/************************************************************************/

/**
 * \fn OGRFeatureDefn *OGRFeatureDefn::Clone();
 *
 * \brief Create a copy of this feature definition.
 *
 * Creates a deep copy of the feature definition. 
 * 
 * @return the copy. 
 */

OGRFeatureDefn *OGRFeatureDefn::Clone()

{
    int i;
    OGRFeatureDefn *poCopy;

    poCopy = new OGRFeatureDefn( GetName() );

    GetFieldCount();
    for( i = 0; i < nFieldCount; i++ )
        poCopy->AddFieldDefn( GetFieldDefn( i ) );

    /* There is a default geometry field created at OGRFeatureDefn instanciation */
    poCopy->DeleteGeomFieldDefn(0);
    GetGeomFieldCount();
    for( i = 0; i < nGeomFieldCount; i++ )
        poCopy->AddGeomFieldDefn( GetGeomFieldDefn( i ) );

    return poCopy;
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

/**
 * \fn const char *OGRFeatureDefn::GetName();
 *
 * \brief Get name of this OGRFeatureDefn.
 *
 * This method is the same as the C function OGR_FD_GetName().
 *
 * @return the name.  This name is internal and should not be modified, or
 * freed.
 */
const char * OGRFeatureDefn::GetName()
{
    return pszFeatureClassName;
}

/************************************************************************/
/*                           OGR_FD_GetName()                           */
/************************************************************************/
/**
 * \brief Get name of the OGRFeatureDefn passed as an argument.
 *
 * This function is the same as the C++ method OGRFeatureDefn::GetName().
 *
 * @param hDefn handle to the feature definition to get the name from.
 * @return the name.  This name is internal and should not be modified, or
 * freed.
 */

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
 * \brief Fetch number of fields on this feature.
 *
 * This method is the same as the C function OGR_FD_GetFieldCount().
 * @return count of fields.
 */

int OGRFeatureDefn::GetFieldCount()
{
    return nFieldCount;
}

/************************************************************************/
/*                        OGR_FD_GetFieldCount()                        */
/************************************************************************/

/**
 * \brief Fetch number of fields on the passed feature definition.
 *
 * This function is the same as the C++ OGRFeatureDefn::GetFieldCount().
 *
 * @param hDefn handle to the feature definition to get the fields count from.
 * @return count of fields.
 */

int OGR_FD_GetFieldCount( OGRFeatureDefnH hDefn )

{
    return ((OGRFeatureDefn *) hDefn)->GetFieldCount();
}

/************************************************************************/
/*                            GetFieldDefn()                            */
/************************************************************************/

/**
 * \brief Fetch field definition.
 *
 * This method is the same as the C function OGR_FD_GetFieldDefn().
 *
 * Starting with GDAL 1.7.0, this method will also issue an error if the index
 * is not valid.
 *
 * @param iField the field to fetch, between 0 and GetFieldCount()-1.
 *
 * @return a pointer to an internal field definition object or NULL if invalid index.
 * This object should not be modified or freed by the application.
 */

OGRFieldDefn *OGRFeatureDefn::GetFieldDefn( int iField )

{
    if( iField < 0 || iField >= GetFieldCount() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid index : %d", iField);
        return NULL;
    }

    return papoFieldDefn[iField];
}

/************************************************************************/
/*                        OGR_FD_GetFieldDefn()                         */
/************************************************************************/

/**
 * \brief Fetch field definition of the passed feature definition.
 *
 * This function is the same as the C++ method 
 * OGRFeatureDefn::GetFieldDefn().
 *
 * Starting with GDAL 1.7.0, this method will also issue an error if the index
 * is not valid.
 *
 * @param hDefn handle to the feature definition to get the field definition
 * from.
 * @param iField the field to fetch, between 0 and GetFieldCount()-1.
 *
 * @return an handle to an internal field definition object or NULL if invalid index.
 * This object should not be modified or freed by the application.
 */

OGRFieldDefnH OGR_FD_GetFieldDefn( OGRFeatureDefnH hDefn, int iField )

{
    return (OGRFieldDefnH) ((OGRFeatureDefn *) hDefn)->GetFieldDefn( iField );
}

/************************************************************************/
/*                            AddFieldDefn()                            */
/************************************************************************/

/**
 * \brief Add a new field definition.
 *
 * To add a new field definition to a layer definition, do not use this
 * function directly, but use OGRLayer::CreateField() instead.
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
    GetFieldCount();
    papoFieldDefn = (OGRFieldDefn **)
        CPLRealloc( papoFieldDefn, sizeof(void*)*(nFieldCount+1) );

    papoFieldDefn[nFieldCount] = new OGRFieldDefn( poNewDefn );
    nFieldCount++;
}

/************************************************************************/
/*                        OGR_FD_AddFieldDefn()                         */
/************************************************************************/

/**
 * \brief Add a new field definition to the passed feature definition.
 *
 * To add a new field definition to a layer definition, do not use this
 * function directly, but use OGR_L_CreateField() instead.
 *
 * This function  should only be called while there are no OGRFeature
 * objects in existance based on this OGRFeatureDefn.  The OGRFieldDefn
 * passed in is copied, and remains the responsibility of the caller.
 *
 * This function is the same as the C++ method OGRFeatureDefn::AddFieldDefn().
 *
 * @param hDefn handle to the feature definition to add the field definition
 * to.
 * @param hNewField handle to the new field definition.
 */

void OGR_FD_AddFieldDefn( OGRFeatureDefnH hDefn, OGRFieldDefnH hNewField )

{
    ((OGRFeatureDefn *) hDefn)->AddFieldDefn( (OGRFieldDefn *) hNewField );
}

/************************************************************************/
/*                           DeleteFieldDefn()                          */
/************************************************************************/

/**
 * \brief Delete an existing field definition.
 *
 * To delete an existing field definition from a layer definition, do not use this
 * function directly, but use OGRLayer::DeleteField() instead.
 *
 * This method should only be called while there are no OGRFeature
 * objects in existance based on this OGRFeatureDefn.
 *
 * This method is the same as the C function OGR_FD_DeleteFieldDefn().
 *
 * @param iField the index of the field defintion.
 * @return OGRERR_NONE in case of success.
 * @since OGR 1.9.0
 */

OGRErr OGRFeatureDefn::DeleteFieldDefn( int iField )

{
    if (iField < 0 || iField >= GetFieldCount())
        return OGRERR_FAILURE;

    delete papoFieldDefn[iField];
    papoFieldDefn[iField] = NULL;

    if (iField < nFieldCount - 1)
    {
        memmove(papoFieldDefn + iField,
                papoFieldDefn + iField + 1,
                (nFieldCount - 1 - iField) * sizeof(void*));
    }

    nFieldCount--;

    return OGRERR_NONE;
}

/************************************************************************/
/*                       OGR_FD_DeleteFieldDefn()                       */
/************************************************************************/

/**
 * \brief Delete an existing field definition.
 *
 * To delete an existing field definition from a layer definition, do not use this
 * function directly, but use OGR_L_DeleteField() instead.
 *
 * This method should only be called while there are no OGRFeature
 * objects in existance based on this OGRFeatureDefn.
 *
 * This method is the same as the C++ method OGRFeatureDefn::DeleteFieldDefn().
 *
 * @param hDefn handle to the feature definition.
 * @param iField the index of the field defintion.
 * @return OGRERR_NONE in case of success.
 * @since OGR 1.9.0
 */

OGRErr OGR_FD_DeleteFieldDefn( OGRFeatureDefnH hDefn, int iField )

{
    return ((OGRFeatureDefn *) hDefn)->DeleteFieldDefn( iField );
}

/************************************************************************/
/*                         ReorderFieldDefns()                          */
/************************************************************************/

/**
 * \brief Reorder the field definitions in the array of the feature definition
 *
 * To reorder the field definitions in a layer definition, do not use this
 * function directly, but use OGR_L_ReorderFields() instead.
 *
 * This method should only be called while there are no OGRFeature
 * objects in existance based on this OGRFeatureDefn.
 *
 * This method is the same as the C function OGR_FD_ReorderFieldDefns().
 *
 * @param panMap an array of GetFieldCount() elements which
 * is a permutation of [0, GetFieldCount()-1]. panMap is such that,
 * for each field definition at position i after reordering,
 * its position before reordering was panMap[i].
 * @return OGRERR_NONE in case of success.
 * @since OGR 1.9.0
 */

OGRErr OGRFeatureDefn::ReorderFieldDefns( int* panMap )

{
    if (GetFieldCount() == 0)
        return OGRERR_NONE;

    OGRErr eErr = OGRCheckPermutation(panMap, nFieldCount);
    if (eErr != OGRERR_NONE)
        return eErr;

    OGRFieldDefn** papoFieldDefnNew = (OGRFieldDefn**)
        CPLMalloc(sizeof(OGRFieldDefn*) * nFieldCount);

    for(int i=0;i<nFieldCount;i++)
    {
        papoFieldDefnNew[i] = papoFieldDefn[panMap[i]];
    }

    CPLFree(papoFieldDefn);
    papoFieldDefn = papoFieldDefnNew;

    return OGRERR_NONE;
}

/************************************************************************/
/*                     OGR_FD_ReorderFieldDefns()                       */
/************************************************************************/

/**
 * \brief Reorder the field definitions in the array of the feature definition
 *
 * To reorder the field definitions in a layer definition, do not use this
 * function directly, but use OGR_L_ReorderFields() instead.
 *
 * This method should only be called while there are no OGRFeature
 * objects in existance based on this OGRFeatureDefn.
 *
 * This method is the same as the C++ method OGRFeatureDefn::ReorderFieldDefns().
 *
 * @param hDefn handle to the feature definition.
 * @param panMap an array of GetFieldCount() elements which
 * is a permutation of [0, GetFieldCount()-1]. panMap is such that,
 * for each field definition at position i after reordering,
 * its position before reordering was panMap[i].
 * @return OGRERR_NONE in case of success.
 * @since OGR 1.9.0
 */

OGRErr OGR_FD_ReorderFieldDefn( OGRFeatureDefnH hDefn, int* panMap )

{
    return ((OGRFeatureDefn *) hDefn)->ReorderFieldDefns( panMap );
}


/************************************************************************/
/*                         GetGeomFieldCount()                          */
/************************************************************************/

/**
 * \fn int OGRFeatureDefn::GetGeomFieldCount();
 *
 * \brief Fetch number of geometry fields on this feature.
 *
 * This method is the same as the C function OGR_FD_GetGeomFieldCount().
 * @return count of geometry fields.
 *
 * @since GDAL 1.11
 */
int OGRFeatureDefn::GetGeomFieldCount()
{
    return nGeomFieldCount;
}

/************************************************************************/
/*                      OGR_FD_GetGeomFieldCount()                      */
/************************************************************************/

/**
 * \brief Fetch number of geometry fields on the passed feature definition.
 *
 * This function is the same as the C++ OGRFeatureDefn::GetGeomFieldCount().
 *
 * @param hDefn handle to the feature definition to get the fields count from.
 * @return count of geometry fields.
 *
 * @since GDAL 1.11
 */

int OGR_FD_GetGeomFieldCount( OGRFeatureDefnH hDefn )

{
    return ((OGRFeatureDefn *) hDefn)->GetGeomFieldCount();
}

/************************************************************************/
/*                           GetGeomFieldDefn()                         */
/************************************************************************/

/**
 * \brief Fetch geometry field definition.
 *
 * This method is the same as the C function OGR_FD_GetGeomFieldDefn().
 *
 * @param iGeomField the geometry field to fetch, between 0 and GetGeomFieldCount()-1.
 *
 * @return a pointer to an internal field definition object or NULL if invalid index.
 * This object should not be modified or freed by the application.
 *
 * @since GDAL 1.11
 */

OGRGeomFieldDefn *OGRFeatureDefn::GetGeomFieldDefn( int iGeomField )

{
    if( iGeomField < 0 || iGeomField >= GetGeomFieldCount() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid index : %d", iGeomField);
        return NULL;
    }

    return papoGeomFieldDefn[iGeomField];
}

/************************************************************************/
/*                      OGR_FD_GetGeomFieldDefn()                       */
/************************************************************************/

/**
 * \brief Fetch geometry field definition of the passed feature definition.
 *
 * This function is the same as the C++ method 
 * OGRFeatureDefn::GetGeomFieldDefn().
 *
 * @param hDefn handle to the feature definition to get the field definition
 * from.
 * @param iGeomField the geometry field to fetch, between 0 and GetGeomFieldCount()-1.
 *
 * @return an handle to an internal field definition object or NULL if invalid index.
 * This object should not be modified or freed by the application.
 *
 * @since GDAL 1.11
 */

OGRGeomFieldDefnH OGR_FD_GetGeomFieldDefn( OGRFeatureDefnH hDefn, int iGeomField )

{
    return (OGRGeomFieldDefnH) ((OGRFeatureDefn *) hDefn)->GetGeomFieldDefn( iGeomField );
}

/************************************************************************/
/*                          AddGeomFieldDefn()                          */
/************************************************************************/

/**
 * \brief Add a new geometry field definition.
 *
 * To add a new geometry field definition to a layer definition, do not use this
 * function directly, but use OGRLayer::CreateGeomField() instead.
 * 
 * This method does an internal copy of the passed geometry field definition,
 * unless bCopy is set to FALSE (in which case it takes ownership of the
 * field definition.
 *
 * This method should only be called while there are no OGRFeature
 * objects in existance based on this OGRFeatureDefn.  The OGRGeomFieldDefn
 * passed in is copied, and remains the responsibility of the caller.
 *
 * This method is the same as the C function OGR_FD_AddGeomFieldDefn().
 *
 * @param poNewDefn the definition of the new geometry field.
 * @param bCopy whether poNewDefn should be copied.
 *
 * @since GDAL 1.11
 */

void OGRFeatureDefn::AddGeomFieldDefn( OGRGeomFieldDefn * poNewDefn,
                                       int bCopy )
{
    GetGeomFieldCount();
    papoGeomFieldDefn = (OGRGeomFieldDefn **)
        CPLRealloc( papoGeomFieldDefn, sizeof(void*)*(nGeomFieldCount+1) );

    papoGeomFieldDefn[nGeomFieldCount] = (bCopy) ?
        new OGRGeomFieldDefn( poNewDefn ) : poNewDefn;
    nGeomFieldCount++;
}

/************************************************************************/
/*                      OGR_FD_AddGeomFieldDefn()                       */
/************************************************************************/

/**
 * \brief Add a new field definition to the passed feature definition.
 *
 * To add a new field definition to a layer definition, do not use this
 * function directly, but use OGR_L_CreateGeomField() instead.
 *
 * This function  should only be called while there are no OGRFeature
 * objects in existance based on this OGRFeatureDefn.  The OGRGeomFieldDefn
 * passed in is copied, and remains the responsibility of the caller.
 *
 * This function is the same as the C++ method OGRFeatureDefn::AddGeomFieldDefn().
 *
 * @param hDefn handle to the feature definition to add the geometry field definition
 * to.
 * @param hNewGeomField handle to the new field definition.
 *
 * @since GDAL 1.11
 */

void OGR_FD_AddGeomFieldDefn( OGRFeatureDefnH hDefn, OGRGeomFieldDefnH hNewGeomField )

{
    ((OGRFeatureDefn *) hDefn)->AddGeomFieldDefn( (OGRGeomFieldDefn *) hNewGeomField );
}

/************************************************************************/
/*                         DeleteGeomFieldDefn()                        */
/************************************************************************/

/**
 * \brief Delete an existing geometry field definition.
 *
 * To delete an existing field definition from a layer definition, do not use this
 * function directly, but use OGRLayer::DeleteGeomField() instead.
 *
 * This method should only be called while there are no OGRFeature
 * objects in existance based on this OGRFeatureDefn.
 *
 * This method is the same as the C function OGR_FD_DeleteGeomFieldDefn().
 *
 * @param iGeomField the index of the geometry field defintion.
 * @return OGRERR_NONE in case of success.
 *
 * @since GDAL 1.11
 */

OGRErr OGRFeatureDefn::DeleteGeomFieldDefn( int iGeomField )

{
    if (iGeomField < 0 || iGeomField >= GetGeomFieldCount())
        return OGRERR_FAILURE;

    delete papoGeomFieldDefn[iGeomField];
    papoGeomFieldDefn[iGeomField] = NULL;

    if (iGeomField < nGeomFieldCount - 1)
    {
        memmove(papoGeomFieldDefn + iGeomField,
                papoGeomFieldDefn + iGeomField + 1,
                (nGeomFieldCount - 1 - iGeomField) * sizeof(void*));
    }

    nGeomFieldCount--;

    return OGRERR_NONE;
}

/************************************************************************/
/*                     OGR_FD_DeleteGeomFieldDefn()                     */
/************************************************************************/

/**
 * \brief Delete an existing geometry field definition.
 *
 * To delete an existing geometry field definition from a layer definition, do not use this
 * function directly, but use OGR_L_DeleteGeomField() instead (*not implemented yet*)
 *
 * This method should only be called while there are no OGRFeature
 * objects in existance based on this OGRFeatureDefn.
 *
 * This method is the same as the C++ method OGRFeatureDefn::DeleteGeomFieldDefn().
 *
 * @param hDefn handle to the feature definition.
 * @param iGeomField the index of the geometry field defintion.
 * @return OGRERR_NONE in case of success.
 *
 * @since GDAL 1.11
 */

OGRErr OGR_FD_DeleteGeomFieldDefn( OGRFeatureDefnH hDefn, int iGeomField )

{
    return ((OGRFeatureDefn *) hDefn)->DeleteGeomFieldDefn( iGeomField );
}


/************************************************************************/
/*                         GetGeomFieldIndex()                          */
/************************************************************************/

/**
 * \brief Find geometry field by name.
 *
 * The geometry field index of the first geometry field matching the passed
 * field name (case insensitively) is returned.
 *
 * This method is the same as the C function OGR_FD_GetGeomFieldIndex().
 *
 * @param pszGeomFieldName the geometry field name to search for.
 *
 * @return the geometry field index, or -1 if no match found.
 */
 

int OGRFeatureDefn::GetGeomFieldIndex( const char * pszGeomFieldName )

{
    GetGeomFieldCount();
    for( int i = 0; i < nGeomFieldCount; i++ )
    {
        if( EQUAL(pszGeomFieldName, GetGeomFieldDefn(i)->GetNameRef() ) )
            return i;
    }

    return -1;
}

/************************************************************************/
/*                      OGR_FD_GetGeomFieldIndex()                      */
/************************************************************************/
/**
 * \brief Find geometry field by name.
 *
 * The geometry field index of the first geometry field matching the passed
 * field name (case insensitively) is returned.
 *
 * This function is the same as the C++ method OGRFeatureDefn::GetGeomFieldIndex.
 *
 * @param hDefn handle to the feature definition to get field index from. 
 * @param pszGeomFieldName the geometry field name to search for.
 *
 * @return the geometry field index, or -1 if no match found.
 */

int OGR_FD_GetGeomFieldIndex( OGRFeatureDefnH hDefn,
                              const char *pszGeomFieldName )

{
    return ((OGRFeatureDefn *)hDefn)->GetGeomFieldIndex( pszGeomFieldName );
}


/************************************************************************/
/*                            GetGeomType()                             */
/************************************************************************/

/**
 * \fn OGRwkbGeometryType OGRFeatureDefn::GetGeomType();
 *
 * \brief Fetch the geometry base type.
 *
 * Note that some drivers are unable to determine a specific geometry
 * type for a layer, in which case wkbUnknown is returned.  A value of
 * wkbNone indicates no geometry is available for the layer at all.
 * Many drivers do not properly mark the geometry
 * type as 25D even if some or all geometries are in fact 25D.  A few (broken)
 * drivers return wkbPolygon for layers that also include wkbMultiPolygon.  
 *
 * Starting with GDAL 1.11, this method returns GetGeomFieldDefn(0)->GetType().
 *
 * This method is the same as the C function OGR_FD_GetGeomType().
 *
 * @return the base type for all geometry related to this definition.
 */
OGRwkbGeometryType OGRFeatureDefn::GetGeomType()
{
    if( GetGeomFieldCount() == 0 )
        return wkbNone;
    return GetGeomFieldDefn(0)->GetType();
}

/************************************************************************/
/*                         OGR_FD_GetGeomType()                         */
/************************************************************************/
/**
 * \brief Fetch the geometry base type of the passed feature definition.
 *
 * This function is the same as the C++ method OGRFeatureDefn::GetGeomType().
 *
 * Starting with GDAL 1.11, this method returns GetGeomFieldDefn(0)->GetType().
 *
 * @param hDefn handle to the feature definition to get the geometry type from.
 * @return the base type for all geometry related to this definition.
 */

OGRwkbGeometryType OGR_FD_GetGeomType( OGRFeatureDefnH hDefn )

{
    return ((OGRFeatureDefn *) hDefn)->GetGeomType();
}

/************************************************************************/
/*                            SetGeomType()                             */
/************************************************************************/

/**
 * \brief Assign the base geometry type for this layer.
 *
 * All geometry objects using this type must be of the defined type or
 * a derived type.  The default upon creation is wkbUnknown which allows for
 * any geometry type.  The geometry type should generally not be changed
 * after any OGRFeatures have been created against this definition. 
 *
 * This method is the same as the C function OGR_FD_SetGeomType().
 *
 * Starting with GDAL 1.11, this method calls GetGeomFieldDefn(0)->SetType().
 *
 * @param eNewType the new type to assign.
 */

void OGRFeatureDefn::SetGeomType( OGRwkbGeometryType eNewType )

{
    if( GetGeomFieldCount() > 0 )
    {
        if( GetGeomFieldCount() == 1 && eNewType == wkbNone )
            DeleteGeomFieldDefn(0);
        else
            GetGeomFieldDefn(0)->SetType(eNewType);
    }
    else if( eNewType != wkbNone )
    {
        OGRGeomFieldDefn oGeomFieldDefn( "", eNewType );
        AddGeomFieldDefn(&oGeomFieldDefn);
    }
}

/************************************************************************/
/*                         OGR_FD_SetGeomType()                         */
/************************************************************************/

/**
 * \brief Assign the base geometry type for the passed layer (the same as the feature definition).
 *
 * All geometry objects using this type must be of the defined type or
 * a derived type.  The default upon creation is wkbUnknown which allows for
 * any geometry type.  The geometry type should generally not be changed
 * after any OGRFeatures have been created against this definition. 
 *
 * This function is the same as the C++ method OGRFeatureDefn::SetGeomType().
 *
 * Starting with GDAL 1.11, this method calls GetGeomFieldDefn(0)->SetType().
 *
 * @param hDefn handle to the layer or feature definition to set the geometry
 * type to.
 * @param eType the new type to assign.
 */

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
 * \brief Increments the reference count by one.
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
/**
 * \brief Increments the reference count by one.
 *
 * The reference count is used keep track of the number of OGRFeature
 * objects referencing this definition. 
 *
 * This function is the same as the C++ method OGRFeatureDefn::Reference().
 *
 * @param hDefn handle to the feature definition on witch OGRFeature are
 * based on.
 * @return the updated reference count.
 */

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
 * \brief Decrements the reference count by one.
 *
 * This method is the same as the C function OGR_FD_Dereference().
 *
 * @return the updated reference count.
 */

/************************************************************************/
/*                         OGR_FD_Dereference()                         */
/************************************************************************/

/**
 * \brief Decrements the reference count by one.
 *
 * This function is the same as the C++ method OGRFeatureDefn::Dereference().
 *
 * @param hDefn handle to the feature definition on witch OGRFeature are
 * based on. 
 * @return the updated reference count.
 */

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
 * \brief Fetch current reference count.
 *
 * This method is the same as the C function OGR_FD_GetReferenceCount().
 *
 * @return the current reference count.
 */

/************************************************************************/
/*                      OGR_FD_GetReferenceCount()                      */
/************************************************************************/

/**
 * \brief Fetch current reference count.
 *
 * This function is the same as the C++ method 
 * OGRFeatureDefn::GetReferenceCount().
 *
 * @param hDefn handle to the feature definition on witch OGRFeature are
 * based on. 
 * @return the current reference count.
 */

int OGR_FD_GetReferenceCount( OGRFeatureDefnH hDefn )

{
    return ((OGRFeatureDefn *) hDefn)->GetReferenceCount();
}

/************************************************************************/
/*                           GetFieldIndex()                            */
/************************************************************************/

/**
 * \brief Find field by name.
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
    GetFieldCount();
    for( int i = 0; i < nFieldCount; i++ )
    {
        if( EQUAL(pszFieldName, GetFieldDefn(i)->GetNameRef() ) )
            return i;
    }

    return -1;
}

/************************************************************************/
/*                        OGR_FD_GetFieldIndex()                        */
/************************************************************************/
/**
 * \brief Find field by name.
 *
 * The field index of the first field matching the passed field name (case
 * insensitively) is returned.
 *
 * This function is the same as the C++ method OGRFeatureDefn::GetFieldIndex.
 *
 * @param hDefn handle to the feature definition to get field index from. 
 * @param pszFieldName the field name to search for.
 *
 * @return the field index, or -1 if no match found.
 */

int OGR_FD_GetFieldIndex( OGRFeatureDefnH hDefn, const char *pszFieldName )

{
    return ((OGRFeatureDefn *)hDefn)->GetFieldIndex( pszFieldName );
}

/************************************************************************/
/*                         IsGeometryIgnored()                          */
/************************************************************************/

/**
 * \fn int OGRFeatureDefn::IsGeometryIgnored();
 *
 * \brief Determine whether the geometry can be omitted when fetching features
 *
 * This method is the same as the C function OGR_FD_IsGeometryIgnored().
 *
 * Starting with GDAL 1.11, this method returns GetGeomFieldDefn(0)->IsIgnored().
 *
 * @return ignore state
 */

int OGRFeatureDefn::IsGeometryIgnored()
{
    if( GetGeomFieldCount() == 0 )
        return FALSE;
    return GetGeomFieldDefn(0)->IsIgnored();
}

/************************************************************************/
/*                      OGR_FD_IsGeometryIgnored()                      */
/************************************************************************/

/**
 * \brief Determine whether the geometry can be omitted when fetching features
 *
 * This function is the same as the C++ method 
 * OGRFeatureDefn::IsGeometryIgnored().
 *
 * Starting with GDAL 1.11, this method returns GetGeomFieldDefn(0)->IsIgnored().
 *
 * @param hDefn handle to the feature definition on witch OGRFeature are
 * based on. 
 * @return ignore state
 */

int OGR_FD_IsGeometryIgnored( OGRFeatureDefnH hDefn )
{
    return ((OGRFeatureDefn *) hDefn)->IsGeometryIgnored();
}

/************************************************************************/
/*                         SetGeometryIgnored()                         */
/************************************************************************/

/**
 * \fn void OGRFeatureDefn::SetGeometryIgnored( int bIgnore );
 *
 * \brief Set whether the geometry can be omitted when fetching features
 *
 * This method is the same as the C function OGR_FD_SetGeometryIgnored().
 *
 * Starting with GDAL 1.11, this method calls GetGeomFieldDefn(0)->SetIgnored().
 *
 * @param bIgnore ignore state
 */

void OGRFeatureDefn::SetGeometryIgnored( int bIgnore )
{
    if( GetGeomFieldCount() > 0 )
        GetGeomFieldDefn(0)->SetIgnored(bIgnore);
}

/************************************************************************/
/*                      OGR_FD_SetGeometryIgnored()                     */
/************************************************************************/

/**
 * \brief Set whether the geometry can be omitted when fetching features
 *
 * This function is the same as the C++ method 
 * OGRFeatureDefn::SetGeometryIgnored().
 *
 * Starting with GDAL 1.11, this method calls GetGeomFieldDefn(0)->SetIgnored().
 *
 * @param hDefn handle to the feature definition on witch OGRFeature are
 * based on. 
 * @param bIgnore ignore state
 */

void OGR_FD_SetGeometryIgnored( OGRFeatureDefnH hDefn, int bIgnore )
{
    ((OGRFeatureDefn *) hDefn)->SetGeometryIgnored( bIgnore );
}

/************************************************************************/
/*                           IsStyleIgnored()                           */
/************************************************************************/

/**
 * \fn int OGRFeatureDefn::IsStyleIgnored();
 *
 * \brief Determine whether the style can be omitted when fetching features
 *
 * This method is the same as the C function OGR_FD_IsStyleIgnored().
 *
 * @return ignore state
 */

/************************************************************************/
/*                       OGR_FD_IsStyleIgnored()                        */
/************************************************************************/

/**
 * \brief Determine whether the style can be omitted when fetching features
 *
 * This function is the same as the C++ method 
 * OGRFeatureDefn::IsStyleIgnored().
 *
 * @param hDefn handle to the feature definition on which OGRFeature are
 * based on. 
 * @return ignore state
 */

int OGR_FD_IsStyleIgnored( OGRFeatureDefnH hDefn )
{
    return ((OGRFeatureDefn *) hDefn)->IsStyleIgnored();
}

/************************************************************************/
/*                          SetStyleIgnored()                           */
/************************************************************************/

/**
 * \fn void OGRFeatureDefn::SetStyleIgnored( int bIgnore );
 *
 * \brief Set whether the style can be omitted when fetching features
 *
 * This method is the same as the C function OGR_FD_SetStyleIgnored().
 *
 * @param bIgnore ignore state
 */

/************************************************************************/
/*                       OGR_FD_SetStyleIgnored()                       */
/************************************************************************/

/**
 * \brief Set whether the style can be omitted when fetching features
 *
 * This function is the same as the C++ method 
 * OGRFeatureDefn::SetStyleIgnored().
 *
 * @param hDefn handle to the feature definition on witch OGRFeature are
 * based on. 
 * @param bIgnore ignore state
 */

void OGR_FD_SetStyleIgnored( OGRFeatureDefnH hDefn, int bIgnore )
{
    ((OGRFeatureDefn *) hDefn)->SetStyleIgnored( bIgnore );
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

/************************************************************************/
/*                             IsSame()                                 */
/************************************************************************/

/**
 * \brief Test if the feature definition is identical to the other one.
 *
 * @param poOtherFeatureDefn the other feature definition to compare to.
 * @return TRUE if the feature definition is identical to the other one.
 */

int OGRFeatureDefn::IsSame( OGRFeatureDefn * poOtherFeatureDefn )
{
    if (strcmp(GetName(), poOtherFeatureDefn->GetName()) == 0 &&
        GetFieldCount() == poOtherFeatureDefn->GetFieldCount() &&
        GetGeomFieldCount() == poOtherFeatureDefn->GetGeomFieldCount())
    {
        int i;
        for(i=0;i<nFieldCount;i++)
        {
            const OGRFieldDefn* poFldDefn = GetFieldDefn(i);
            const OGRFieldDefn* poOtherFldDefn = poOtherFeatureDefn->GetFieldDefn(i);
            if (!poFldDefn->IsSame(poOtherFldDefn))
            {
                return FALSE;
            }
        }
        for(i=0;i<nGeomFieldCount;i++)
        {
            OGRGeomFieldDefn* poGFldDefn = GetGeomFieldDefn(i);
            OGRGeomFieldDefn* poOtherGFldDefn =
                poOtherFeatureDefn->GetGeomFieldDefn(i);
            if (!poGFldDefn->IsSame(poOtherGFldDefn))
            {
                return FALSE;
            }
        }
        return TRUE;
    }
    return FALSE;
}

/************************************************************************/
/*                           OGR_FD_IsSame()                            */
/************************************************************************/

/**
 * \brief Test if the feature definition is identical to the other one.
 *
 * @param hFDefn handle to the feature definition on witch OGRFeature are
 * based on. 
 * @param hOtherFDefn handle to the other feature definition to compare to.
 * @return TRUE if the feature definition is identical to the other one.
 *
 * @since OGR 1.11
 */

int OGR_FD_IsSame( OGRFeatureDefnH hFDefn, OGRFeatureDefnH hOtherFDefn )
{
    VALIDATE_POINTER1( hFDefn, "OGR_FD_IsSame", FALSE );
    VALIDATE_POINTER1( hOtherFDefn, "OGR_FD_IsSame", FALSE );
    return ((OGRFeatureDefn*)hFDefn)->IsSame((OGRFeatureDefn*)hOtherFDefn);
}
