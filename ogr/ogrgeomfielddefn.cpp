/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRGeomFieldDefn class implementation.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"
#include "ogr_api.h"

#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"
#include "ogr_srs_api.h"
#include "ograpispy.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                         OGRGeomFieldDefn()                           */
/************************************************************************/

/**
 * \brief Constructor.
 *
 * @param pszNameIn the name of the new field.
 * @param eGeomTypeIn the type of the new field.
 *
 * @since GDAL 1.11
 */

OGRGeomFieldDefn::OGRGeomFieldDefn( const char * pszNameIn,
                                    OGRwkbGeometryType eGeomTypeIn )

{
    Initialize( pszNameIn, eGeomTypeIn );
}

/************************************************************************/
/*                          OGRGeomFieldDefn()                          */
/************************************************************************/

/**
 * \brief Constructor.
 *
 * Create by cloning an existing geometry field definition.
 *
 * @param poPrototype the geometry field definition to clone.
 *
 * @since GDAL 1.11
 */

OGRGeomFieldDefn::OGRGeomFieldDefn( const OGRGeomFieldDefn *poPrototype )

{
    Initialize( poPrototype->GetNameRef(), poPrototype->GetType() );
    auto l_poSRS = poPrototype->GetSpatialRef();
    if( l_poSRS )
    {
        l_poSRS = l_poSRS->Clone();
        SetSpatialRef( l_poSRS );
        l_poSRS->Release();
    }
    SetNullable( poPrototype->IsNullable() );
}

/************************************************************************/
/*                           OGR_GFld_Create()                          */
/************************************************************************/
/**
 * \brief Create a new field geometry definition.
 *
 * This function is the same as the CPP method
 * OGRGeomFieldDefn::OGRGeomFieldDefn().
 *
 * @param pszName the name of the new field definition.
 * @param eType the type of the new field definition.
 * @return handle to the new field definition.
 *
 * @since GDAL 1.11
 */

OGRGeomFieldDefnH OGR_GFld_Create( const char *pszName,
                                   OGRwkbGeometryType eType )

{
  return
      OGRGeomFieldDefn::ToHandle(new OGRGeomFieldDefn(pszName, eType));
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

//! @cond Doxygen_Suppress
void OGRGeomFieldDefn::Initialize( const char * pszNameIn,
                                   OGRwkbGeometryType eTypeIn )

{
    pszName = CPLStrdup( pszNameIn );
    eGeomType = eTypeIn;
}
//! @endcond

/************************************************************************/
/*                         ~OGRGeomFieldDefn()                          */
/************************************************************************/

OGRGeomFieldDefn::~OGRGeomFieldDefn()

{
    CPLFree( pszName );

    if( nullptr != poSRS )
        poSRS->Release();
}

/************************************************************************/
/*                         OGR_GFld_Destroy()                           */
/************************************************************************/
/**
 * \brief Destroy a geometry field definition.
 *
 * @param hDefn handle to the geometry field definition to destroy.
 *
 * @since GDAL 1.11
 */

void OGR_GFld_Destroy( OGRGeomFieldDefnH hDefn )

{
    VALIDATE_POINTER0( hDefn, "OGR_GFld_Destroy" );

    delete OGRGeomFieldDefn::FromHandle(hDefn);
}

/************************************************************************/
/*                              SetName()                               */
/************************************************************************/

/**
 * \brief Reset the name of this field.
 *
 * This method is the same as the C function OGR_GFld_SetName().
 *
 * @param pszNameIn the new name to apply.
 *
 * @since GDAL 1.11
 */

void OGRGeomFieldDefn::SetName( const char * pszNameIn )

{
    if( pszName != pszNameIn )
    {
        CPLFree( pszName );
        pszName = CPLStrdup( pszNameIn );
    }
}

/************************************************************************/
/*                         OGR_GFld_SetName()                           */
/************************************************************************/
/**
 * \brief Reset the name of this field.
 *
 * This function is the same as the CPP method OGRGeomFieldDefn::SetName().
 *
 * @param hDefn handle to the geometry field definition to apply the
 * new name to.
 * @param pszName the new name to apply.
 *
 * @since GDAL 1.11
 */

void OGR_GFld_SetName( OGRGeomFieldDefnH hDefn, const char *pszName )

{
    VALIDATE_POINTER0( hDefn, "OGR_GFld_SetName" );

    OGRGeomFieldDefn::FromHandle(hDefn)->SetName(pszName);
}

/************************************************************************/
/*                             GetNameRef()                             */
/************************************************************************/

/**
 * \fn const char *OGRGeomFieldDefn::GetNameRef();
 *
 * \brief Fetch name of this field.
 *
 * This method is the same as the C function OGR_GFld_GetNameRef().
 *
 * @return pointer to an internal name string that should not be freed or
 * modified.
 *
 * @since GDAL 1.11
 */

/************************************************************************/
/*                        OGR_GFld_GetNameRef()                         */
/************************************************************************/
/**
 * \brief Fetch name of this field.
 *
 * This function is the same as the CPP method OGRGeomFieldDefn::GetNameRef().
 *
 * @param hDefn handle to the geometry field definition.
 * @return the name of the geometry field definition.
 *
 * @since GDAL 1.11
 */

const char *OGR_GFld_GetNameRef( OGRGeomFieldDefnH hDefn )

{
    VALIDATE_POINTER1( hDefn, "OGR_GFld_GetNameRef", "" );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_GFld_GetXXXX(hDefn, "GetNameRef");
#endif

    return OGRGeomFieldDefn::FromHandle(hDefn)->GetNameRef();
}

/************************************************************************/
/*                              GetType()                               */
/************************************************************************/

/**
 * \fn OGRwkbGeometryType OGRGeomFieldDefn::GetType() const;
 *
 * \brief Fetch geometry type of this field.
 *
 * This method is the same as the C function OGR_GFld_GetType().
 *
 * @return field geometry type.
 *
 * @since GDAL 1.11
 */

/************************************************************************/
/*                          OGR_GFld_GetType()                          */
/************************************************************************/
/**
 * \brief Fetch geometry type of this field.
 *
 * This function is the same as the CPP method OGRGeomFieldDefn::GetType().
 *
 * @param hDefn handle to the geometry field definition to get type from.
 * @return field geometry type.
 *
 * @since GDAL 1.11
 */

OGRwkbGeometryType OGR_GFld_GetType( OGRGeomFieldDefnH hDefn )

{
    VALIDATE_POINTER1( hDefn, "OGR_GFld_GetType", wkbUnknown );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_GFld_GetXXXX(hDefn, "GetType");
#endif

    OGRwkbGeometryType eType =
        OGRGeomFieldDefn::FromHandle(hDefn)->GetType();
    if( OGR_GT_IsNonLinear(eType) && !OGRGetNonLinearGeometriesEnabledFlag() )
    {
        eType = OGR_GT_GetLinear(eType);
    }
    return eType;
}

/************************************************************************/
/*                              SetType()                               */
/************************************************************************/

/**
 * \brief Set the geometry type of this field.
 * This should never be done to an OGRGeomFieldDefn
 * that is already part of an OGRFeatureDefn.
 *
 * This method is the same as the C function OGR_GFld_SetType().
 *
 * @param eTypeIn the new field geometry type.
 *
 * @since GDAL 1.11
 */

void OGRGeomFieldDefn::SetType( OGRwkbGeometryType eTypeIn )

{
    eGeomType = eTypeIn;
}

/************************************************************************/
/*                          OGR_GFld_SetType()                          */
/************************************************************************/
/**
 * \brief Set the geometry type of this field.
 * This should never be done to an OGRGeomFieldDefn
 * that is already part of an OGRFeatureDefn.
 *
 * This function is the same as the CPP method OGRGeomFieldDefn::SetType().
 *
 * @param hDefn handle to the geometry field definition to set type to.
 * @param eType the new field geometry type.
 *
 * @since GDAL 1.11
 */

void OGR_GFld_SetType( OGRGeomFieldDefnH hDefn, OGRwkbGeometryType eType )

{
    VALIDATE_POINTER0( hDefn, "OGR_GFld_SetType" );

    OGRGeomFieldDefn::FromHandle(hDefn)->SetType(eType);
}

/************************************************************************/
/*                             IsIgnored()                              */
/************************************************************************/

/**
 * \fn int OGRGeomFieldDefn::IsIgnored() const;
 *
 * \brief Return whether this field should be omitted when fetching features
 *
 * This method is the same as the C function OGR_GFld_IsIgnored().
 *
 * @return ignore state
 *
 * @since GDAL 1.11
 */

/************************************************************************/
/*                         OGR_GFld_IsIgnored()                         */
/************************************************************************/

/**
 * \brief Return whether this field should be omitted when fetching features
 *
 * This method is the same as the C++ method OGRGeomFieldDefn::IsIgnored().
 *
 * @param hDefn handle to the geometry field definition
 * @return ignore state
 *
 * @since GDAL 1.11
 */

int OGR_GFld_IsIgnored( OGRGeomFieldDefnH hDefn )
{
    VALIDATE_POINTER1( hDefn, "OGR_GFld_IsIgnored", FALSE );

    return OGRGeomFieldDefn::FromHandle(hDefn)->IsIgnored();
}

/************************************************************************/
/*                            SetIgnored()                              */
/************************************************************************/

/**
 * \fn void OGRGeomFieldDefn::SetIgnored( int ignore );
 *
 * \brief Set whether this field should be omitted when fetching features
 *
 * This method is the same as the C function OGR_GFld_SetIgnored().
 *
 * @param ignore ignore state
 *
 * @since GDAL 1.11
 */

/************************************************************************/
/*                        OGR_GFld_SetIgnored()                         */
/************************************************************************/

/**
 * \brief Set whether this field should be omitted when fetching features
 *
 * This method is the same as the C++ method OGRGeomFieldDefn::SetIgnored().
 *
 * @param hDefn handle to the geometry field definition
 * @param ignore ignore state
 *
 * @since GDAL 1.11
 */

void OGR_GFld_SetIgnored( OGRGeomFieldDefnH hDefn, int ignore )
{
    VALIDATE_POINTER0( hDefn, "OGR_GFld_SetIgnored" );

    OGRGeomFieldDefn::FromHandle(hDefn)->SetIgnored(ignore);
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/
/**
 * \brief Fetch spatial reference system of this field.
 *
 * This method is the same as the C function OGR_GFld_GetSpatialRef().
 *
 * @return field spatial reference system.
 *
 * @since GDAL 1.11
 */

OGRSpatialReference* OGRGeomFieldDefn::GetSpatialRef() const
{
    return poSRS;
}

/************************************************************************/
/*                       OGR_GFld_GetSpatialRef()                       */
/************************************************************************/

/**
 * \brief Fetch spatial reference system of this field.
 *
 * This function is the same as the C++ method
 * OGRGeomFieldDefn::GetSpatialRef().
 *
 * @param hDefn handle to the geometry field definition
 *
 * @return field spatial reference system.
 *
 * @since GDAL 1.11
 */

OGRSpatialReferenceH OGR_GFld_GetSpatialRef( OGRGeomFieldDefnH hDefn )
{
    VALIDATE_POINTER1( hDefn, "OGR_GFld_GetSpatialRef", nullptr );

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_GFld_GetXXXX(hDefn, "GetSpatialRef");
#endif

    return reinterpret_cast<OGRSpatialReferenceH>(
        OGRGeomFieldDefn::FromHandle(hDefn)->GetSpatialRef());
}

/************************************************************************/
/*                           SetSpatialRef()                            */
/************************************************************************/

/**
 * \brief Set the spatial reference of this field.
 *
 * This method is the same as the C function OGR_GFld_SetSpatialRef().
 *
 * This method drops the reference of the previously set SRS object and
 * acquires a new reference on the passed object (if non-NULL).
 *
 * @param poSRSIn the new SRS to apply.
 *
 * @since GDAL 1.11
 */
void OGRGeomFieldDefn::SetSpatialRef(OGRSpatialReference* poSRSIn)
{
    if( poSRS != nullptr )
        poSRS->Release();
    poSRS = poSRSIn;
    if( poSRS != nullptr )
        poSRS->Reference();
}

/************************************************************************/
/*                       OGR_GFld_SetSpatialRef()                       */
/************************************************************************/

/**
 * \brief Set the spatial reference of this field.
 *
 * This function is the same as the C++ method
 * OGRGeomFieldDefn::SetSpatialRef().
 *
 * This function drops the reference of the previously set SRS object and
 * acquires a new reference on the passed object (if non-NULL).
 *
 * @param hDefn handle to the geometry field definition
 * @param hSRS the new SRS to apply.
 *
 * @since GDAL 1.11
 */

void OGR_GFld_SetSpatialRef( OGRGeomFieldDefnH hDefn,
                             OGRSpatialReferenceH hSRS )
{
    VALIDATE_POINTER0( hDefn, "OGR_GFld_SetSpatialRef" );

    OGRGeomFieldDefn::FromHandle(hDefn)->
        SetSpatialRef(reinterpret_cast<OGRSpatialReference *>(hSRS));
}

/************************************************************************/
/*                             IsSame()                                 */
/************************************************************************/

/**
 * \brief Test if the geometry field definition is identical to the other one.
 *
 * @param poOtherFieldDefn the other field definition to compare to.
 * @return TRUE if the geometry field definition is identical to the other one.
 *
 * @since GDAL 1.11
 */

int OGRGeomFieldDefn::IsSame( const OGRGeomFieldDefn * poOtherFieldDefn ) const
{
    if( !(strcmp(GetNameRef(), poOtherFieldDefn->GetNameRef()) == 0 &&
                 GetType() == poOtherFieldDefn->GetType() &&
                 IsNullable() == poOtherFieldDefn->IsNullable()) )
        return FALSE;
    OGRSpatialReference* poMySRS = GetSpatialRef();
    OGRSpatialReference* poOtherSRS = poOtherFieldDefn->GetSpatialRef();
    return ((poMySRS == poOtherSRS) ||
            (poMySRS != nullptr && poOtherSRS != nullptr &&
             poMySRS->IsSame(poOtherSRS)));
}

/************************************************************************/
/*                             IsNullable()                             */
/************************************************************************/

/**
 * \fn int OGRGeomFieldDefn::IsNullable() const
 *
 * \brief Return whether this geometry field can receive null values.
 *
 * By default, fields are nullable.
 *
 * Even if this method returns FALSE (i.e not-nullable field), it doesn't mean
 * that OGRFeature::IsFieldSet() will necessary return TRUE, as fields can be
 * temporary unset and null/not-null validation is usually done when
 * OGRLayer::CreateFeature()/SetFeature() is called.
 *
 * Note that not-nullable geometry fields might also contain 'empty' geometries.
 *
 * This method is the same as the C function OGR_GFld_IsNullable().
 *
 * @return TRUE if the field is authorized to be null.
 * @since GDAL 2.0
 */

/************************************************************************/
/*                         OGR_GFld_IsNullable()                        */
/************************************************************************/

/**
 * \brief Return whether this geometry field can receive null values.
 *
 * By default, fields are nullable.
 *
 * Even if this method returns FALSE (i.e not-nullable field), it doesn't mean
 * that OGRFeature::IsFieldSet() will necessary return TRUE, as fields can be
 * temporary unset and null/not-null validation is usually done when
 * OGRLayer::CreateFeature()/SetFeature() is called.
 *
 * Note that not-nullable geometry fields might also contain 'empty' geometries.
 *
 * This method is the same as the C++ method OGRGeomFieldDefn::IsNullable().
 *
 * @param hDefn handle to the field definition
 * @return TRUE if the field is authorized to be null.
 * @since GDAL 2.0
 */

int OGR_GFld_IsNullable( OGRGeomFieldDefnH hDefn )
{
    return OGRGeomFieldDefn::FromHandle(hDefn)->IsNullable();
}

/************************************************************************/
/*                            SetNullable()                             */
/************************************************************************/

/**
 * \fn void OGRGeomFieldDefn::SetNullable( int bNullableIn );
 *
 * \brief Set whether this geometry field can receive null values.
 *
 * By default, fields are nullable, so this method is generally called with
 * FALSE to set a not-null constraint.
 *
 * Drivers that support writing not-null constraint will advertise the
 * GDAL_DCAP_NOTNULL_GEOMFIELDS driver metadata item.
 *
 * This method is the same as the C function OGR_GFld_SetNullable().
 *
 * @param bNullableIn FALSE if the field must have a not-null constraint.
 * @since GDAL 2.0
 */

/************************************************************************/
/*                        OGR_GFld_SetNullable()                        */
/************************************************************************/

/**
 * \brief Set whether this geometry field can receive null values.
 *
 * By default, fields are nullable, so this method is generally called with
 * FALSE to set a not-null constraint.
 *
 * Drivers that support writing not-null constraint will advertise the
 * GDAL_DCAP_NOTNULL_GEOMFIELDS driver metadata item.
 *
 * This method is the same as the C++ method OGRGeomFieldDefn::SetNullable().
 *
 * @param hDefn handle to the field definition
 * @param bNullableIn FALSE if the field must have a not-null constraint.
 * @since GDAL 2.0
 */

void OGR_GFld_SetNullable( OGRGeomFieldDefnH hDefn, int bNullableIn )
{
    OGRGeomFieldDefn::FromHandle(hDefn)->SetNullable(bNullableIn);
}
