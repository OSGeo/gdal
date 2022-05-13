/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRFeature class implementation.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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
#include "ogr_feature.h"

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>

#include <algorithm>
#include <limits>
#include <map>
#include <new>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_time.h"
#include "cpl_vsi.h"
#include "ogr_core.h"
#include "ogr_featurestyle.h"
#include "ogr_geometry.h"
#include "ogr_p.h"
#include "ogrgeojsonreader.h"

#include "cpl_json_header.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                             OGRFeature()                             */
/************************************************************************/

/**
 * \brief Constructor
 *
 * Note that the OGRFeature will increment the reference count of its
 * defining OGRFeatureDefn.  Destruction of the OGRFeatureDefn before
 * destruction of all OGRFeatures that depend on it is likely to result in
 * a crash.
 *
 * This method is the same as the C function OGR_F_Create().
 *
 * @param poDefnIn feature class (layer) definition to which the feature will
 * adhere.
 */

OGRFeature::OGRFeature( OGRFeatureDefn * poDefnIn ) :
    nFID(OGRNullFID),
    poDefn(poDefnIn),
    papoGeometries(nullptr),
    pauFields(nullptr),
    m_pszNativeData(nullptr),
    m_pszNativeMediaType(nullptr),
    m_pszStyleString(nullptr),
    m_poStyleTable(nullptr),
    m_pszTmpFieldValue(nullptr)
{
    poDefnIn->Reference();

    const int nFieldCount = poDefn->GetFieldCount();
    pauFields = static_cast<OGRField *>(
        VSI_MALLOC_VERBOSE(nFieldCount * sizeof(OGRField)));

    papoGeometries = static_cast<OGRGeometry **>(
        VSI_CALLOC_VERBOSE(poDefn->GetGeomFieldCount(),
                           sizeof(OGRGeometry*)));

    // Initialize array to the unset special value.
    if( pauFields != nullptr )
    {
        for( int i = 0; i < nFieldCount; i++ )
        {
            pauFields[i].Set.nMarker1 = OGRUnsetMarker;
            pauFields[i].Set.nMarker2 = OGRUnsetMarker;
            pauFields[i].Set.nMarker3 = OGRUnsetMarker;
        }
    }
}

/************************************************************************/
/*                            OGR_F_Create()                            */
/************************************************************************/
/**
 * \brief Feature factory.
 *
 * Note that the OGRFeature will increment the reference count of its
 * defining OGRFeatureDefn.  Destruction of the OGRFeatureDefn before
 * destruction of all OGRFeatures that depend on it is likely to result in
 * a crash.
 *
 * This function is the same as the C++ method OGRFeature::OGRFeature().
 *
 * @param hDefn handle to the feature class (layer) definition to
 * which the feature will adhere.
 *
 * @return a handle to the new feature object with null fields and no geometry,
 * or, starting with GDAL 2.1, NULL in case out of memory situation.
 */

OGRFeatureH OGR_F_Create( OGRFeatureDefnH hDefn )

{
    VALIDATE_POINTER1( hDefn, "OGR_F_Create", nullptr );
    return OGRFeature::ToHandle(
        OGRFeature::CreateFeature(OGRFeatureDefn::FromHandle(hDefn)));
}

/************************************************************************/
/*                            ~OGRFeature()                             */
/************************************************************************/

OGRFeature::~OGRFeature()

{
    if( pauFields != nullptr )
    {
        const int nFieldcount = poDefn->GetFieldCount();
        for( int i = 0; i < nFieldcount; i++ )
        {
            OGRFieldDefn *poFDefn = poDefn->GetFieldDefn(i);

            if( !IsFieldSetAndNotNullUnsafe(i) )
                continue;

            switch( poFDefn->GetType() )
            {
              case OFTString:
                if( pauFields[i].String != nullptr )
                    VSIFree( pauFields[i].String );
                break;

              case OFTBinary:
                if( pauFields[i].Binary.paData != nullptr )
                    VSIFree( pauFields[i].Binary.paData );
                break;

              case OFTStringList:
                CSLDestroy( pauFields[i].StringList.paList );
                break;

              case OFTIntegerList:
              case OFTInteger64List:
              case OFTRealList:
                CPLFree( pauFields[i].IntegerList.paList );
                break;

              default:
                // TODO(schwehr): Add support for wide strings.
                break;
            }
        }
    }

    if( papoGeometries != nullptr )
    {
        const int nGeomFieldCount = poDefn->GetGeomFieldCount();

        for( int i = 0; i < nGeomFieldCount; i++ )
        {
            delete papoGeometries[i];
        }
    }

    if( poDefn )
        poDefn->Release();

    CPLFree(pauFields);
    CPLFree(papoGeometries);
    CPLFree(m_pszStyleString);
    CPLFree(m_pszTmpFieldValue);
    CPLFree(m_pszNativeData);
    CPLFree(m_pszNativeMediaType);
}

/************************************************************************/
/*                           OGR_F_Destroy()                            */
/************************************************************************/
/**
 * \brief Destroy feature
 *
 * The feature is deleted, but within the context of the GDAL/OGR heap.
 * This is necessary when higher level applications use GDAL/OGR from a
 * DLL and they want to delete a feature created within the DLL.  If the
 * delete is done in the calling application the memory will be freed onto
 * the application heap which is inappropriate.
 *
 * This function is the same as the C++ method OGRFeature::DestroyFeature().
 *
 * @param hFeat handle to the feature to destroy.
 */

void OGR_F_Destroy( OGRFeatureH hFeat )

{
    delete OGRFeature::FromHandle(hFeat);
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

/**
 * \brief Feature factory.
 *
 * This is essentially a feature factory, useful for
 * applications creating features but wanting to ensure they
 * are created out of the OGR/GDAL heap.
 *
 * This method is the same as the C function OGR_F_Create().
 *
 * @param poDefn Feature definition defining schema.
 *
 * @return new feature object with null fields and no geometry, or, starting
 * with GDAL 2.1, NULL in case of out of memory situation.  May be deleted with
 * DestroyFeature().
 */

OGRFeature *OGRFeature::CreateFeature( OGRFeatureDefn *poDefn )

{
    OGRFeature* poFeature = new (std::nothrow) OGRFeature( poDefn );
    if( poFeature == nullptr )
        return nullptr;

    if( (poFeature->pauFields == nullptr && poDefn->GetFieldCount() != 0) ||
        (poFeature->papoGeometries == nullptr &&
         poDefn->GetGeomFieldCount() != 0) )
    {
        delete poFeature;
        return nullptr;
    }

    return poFeature;
}

/************************************************************************/
/*                           DestroyFeature()                           */
/************************************************************************/

/**
 * \brief Destroy feature
 *
 * The feature is deleted, but within the context of the GDAL/OGR heap.
 * This is necessary when higher level applications use GDAL/OGR from a
 * DLL and they want to delete a feature created within the DLL.  If the
 * delete is done in the calling application the memory will be freed onto
 * the application heap which is inappropriate.
 *
 * This method is the same as the C function OGR_F_Destroy().
 *
 * @param poFeature the feature to delete.
 */

void OGRFeature::DestroyFeature( OGRFeature *poFeature )

{
    delete poFeature;
}

/************************************************************************/
/*                                Reset()                               */
/************************************************************************/

/** Reset the state of a OGRFeature to its state after construction.
 *
 * This enables recycling existing OGRFeature instances.
 *
 * @since GDAL 3.5
 */
void OGRFeature::Reset()
{
    nFID = OGRNullFID;

    if( pauFields != nullptr )
    {
        const int nFieldcount = poDefn->GetFieldCountUnsafe();
        for( int i = 0; i < nFieldcount; i++ )
        {
            if( !IsFieldSetAndNotNullUnsafe(i) )
                continue;

            OGRFieldDefn *poFDefn = poDefn->GetFieldDefn(i);
            switch( poFDefn->GetType() )
            {
              case OFTString:
                if( pauFields[i].String != nullptr )
                    VSIFree( pauFields[i].String );
                break;

              case OFTBinary:
                if( pauFields[i].Binary.paData != nullptr )
                    VSIFree( pauFields[i].Binary.paData );
                break;

              case OFTStringList:
                CSLDestroy( pauFields[i].StringList.paList );
                break;

              case OFTIntegerList:
              case OFTInteger64List:
              case OFTRealList:
                CPLFree( pauFields[i].IntegerList.paList );
                break;

              default:
                // TODO(schwehr): Add support for wide strings.
                break;
            }

            pauFields[i].Set.nMarker1 = OGRUnsetMarker;
            pauFields[i].Set.nMarker2 = OGRUnsetMarker;
            pauFields[i].Set.nMarker3 = OGRUnsetMarker;
        }
    }

    if( papoGeometries != nullptr )
    {
        const int nGeomFieldCount = poDefn->GetGeomFieldCount();

        for( int i = 0; i < nGeomFieldCount; i++ )
        {
            delete papoGeometries[i];
            papoGeometries[i] = nullptr;
        }
    }

    if( m_pszStyleString )
    {
        CPLFree(m_pszStyleString);
        m_pszStyleString = nullptr;
    }

    if( m_pszNativeData )
    {
        CPLFree(m_pszNativeData);
        m_pszNativeData = nullptr;
    }

    if( m_pszNativeMediaType )
    {
        CPLFree(m_pszNativeMediaType);
        m_pszNativeMediaType = nullptr;
    }
}

/************************************************************************/
/*                        SetFDefnUnsafe()                              */
/************************************************************************/

//! @cond Doxygen_Suppress
void OGRFeature::SetFDefnUnsafe( OGRFeatureDefn* poNewFDefn )
{
    poNewFDefn->Reference();
    poDefn->Release();
    poDefn = poNewFDefn;
}
//! @endcond

/************************************************************************/
/*                             GetDefnRef()                             */
/************************************************************************/

/**
 * \fn OGRFeatureDefn *OGRFeature::GetDefnRef();
 *
 * \brief Fetch feature definition.
 *
 * This method is the same as the C function OGR_F_GetDefnRef().
 *
 * @return a reference to the feature definition object.
 */

/**
 * \fn const OGRFeatureDefn *OGRFeature::GetDefnRef() const;
 *
 * \brief Fetch feature definition.
 *
 * This method is the same as the C function OGR_F_GetDefnRef().
 *
 * @return a reference to the feature definition object.
 * @since GDAL 2.3
 */

/************************************************************************/
/*                          OGR_F_GetDefnRef()                          */
/************************************************************************/

/**
 * \brief Fetch feature definition.
 *
 * This function is the same as the C++ method OGRFeature::GetDefnRef().
 *
 * @param hFeat handle to the feature to get the feature definition from.
 *
 * @return a handle to the feature definition object on which feature
 * depends.
 */

OGRFeatureDefnH OGR_F_GetDefnRef( OGRFeatureH hFeat )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetDefnRef", nullptr );

    return OGRFeatureDefn::ToHandle(
        OGRFeature::FromHandle(hFeat)->GetDefnRef());
}

/************************************************************************/
/*                        SetGeometryDirectly()                         */
/************************************************************************/

/**
 * \brief Set feature geometry.
 *
 * This method updates the features geometry, and operate exactly as
 * SetGeometry(), except that this method assumes ownership of the
 * passed geometry (even in case of failure of that function).
 *
 * This method is the same as the C function OGR_F_SetGeometryDirectly().
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param poGeomIn new geometry to apply to feature. Passing NULL value here
 * is correct and it will result in deallocation of currently assigned geometry
 * without assigning new one.
 *
 * @return OGRERR_NONE if successful, or OGR_UNSUPPORTED_GEOMETRY_TYPE if
 * the geometry type is illegal for the OGRFeatureDefn (checking not yet
 * implemented).
 */

OGRErr OGRFeature::SetGeometryDirectly( OGRGeometry * poGeomIn )

{
    if( GetGeomFieldCount() > 0 )
        return SetGeomFieldDirectly(0, poGeomIn);

    delete poGeomIn;
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                     OGR_F_SetGeometryDirectly()                      */
/************************************************************************/

/**
 * \brief Set feature geometry.
 *
 * This function updates the features geometry, and operate exactly as
 * SetGeometry(), except that this function assumes ownership of the
 * passed geometry (even in case of failure of that function).
 *
 * This function is the same as the C++ method
 * OGRFeature::SetGeometryDirectly.
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param hFeat handle to the feature on which to apply the geometry.
 * @param hGeom handle to the new geometry to apply to feature.
 *
 * @return OGRERR_NONE if successful, or OGR_UNSUPPORTED_GEOMETRY_TYPE if
 * the geometry type is illegal for the OGRFeatureDefn (checking not yet
 * implemented).
 */

OGRErr OGR_F_SetGeometryDirectly( OGRFeatureH hFeat, OGRGeometryH hGeom )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_SetGeometryDirectly", OGRERR_FAILURE );

    return OGRFeature::FromHandle(hFeat)->
        SetGeometryDirectly(OGRGeometry::FromHandle(hGeom));
}

/************************************************************************/
/*                            SetGeometry()                             */
/************************************************************************/

/**
 * \brief Set feature geometry.
 *
 * This method updates the features geometry, and operate exactly as
 * SetGeometryDirectly(), except that this method does not assume ownership
 * of the passed geometry, but instead makes a copy of it.
 *
 * This method is the same as the C function OGR_F_SetGeometry().
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param poGeomIn new geometry to apply to feature. Passing NULL value here
 * is correct and it will result in deallocation of currently assigned geometry
 * without assigning new one.
 *
 * @return OGRERR_NONE if successful, or OGR_UNSUPPORTED_GEOMETRY_TYPE if
 * the geometry type is illegal for the OGRFeatureDefn (checking not yet
 * implemented).
 */

OGRErr OGRFeature::SetGeometry( const OGRGeometry * poGeomIn )

{
    if( GetGeomFieldCount() < 1 )
        return OGRERR_FAILURE;

    return SetGeomField(0, poGeomIn);
}

/************************************************************************/
/*                         OGR_F_SetGeometry()                          */
/************************************************************************/

/**
 * \brief Set feature geometry.
 *
 * This function updates the features geometry, and operate exactly as
 * SetGeometryDirectly(), except that this function does not assume ownership
 * of the passed geometry, but instead makes a copy of it.
 *
 * This function is the same as the C++ OGRFeature::SetGeometry().
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param hFeat handle to the feature on which new geometry is applied to.
 * @param hGeom handle to the new geometry to apply to feature.
 *
 * @return OGRERR_NONE if successful, or OGR_UNSUPPORTED_GEOMETRY_TYPE if
 * the geometry type is illegal for the OGRFeatureDefn (checking not yet
 * implemented).
 */

OGRErr OGR_F_SetGeometry( OGRFeatureH hFeat, OGRGeometryH hGeom )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_SetGeometry", OGRERR_FAILURE );

    return OGRFeature::FromHandle(hFeat)->
        SetGeometry(OGRGeometry::FromHandle(hGeom));
}

/************************************************************************/
/*                           StealGeometry()                            */
/************************************************************************/

/**
 * \brief Take away ownership of geometry.
 *
 * Fetch the geometry from this feature, and clear the reference to the
 * geometry on the feature.  This is a mechanism for the application to
 * take over ownership of the geometry from the feature without copying.
 * Sort of an inverse to SetGeometryDirectly().
 *
 * After this call the OGRFeature will have a NULL geometry.
 *
 * @return the pointer to the geometry.
 */

OGRGeometry *OGRFeature::StealGeometry()

{
    if( GetGeomFieldCount() > 0 )
    {
        OGRGeometry *poReturn = papoGeometries[0];
        papoGeometries[0] = nullptr;
        return poReturn;
    }

    return nullptr;
}

/**
 * \brief Take away ownership of geometry.
 *
 * Fetch the geometry from this feature, and clear the reference to the
 * geometry on the feature.  This is a mechanism for the application to
 * take over ownership of the geometry from the feature without copying.
 * Sort of an inverse to SetGeometryDirectly().
 *
 * After this call the OGRFeature will have a NULL geometry.
 *
 * @param iGeomField index of the geometry field.
 *
 * @return the pointer to the geometry.
 */

OGRGeometry *OGRFeature::StealGeometry( int iGeomField )

{
    if( iGeomField >= 0 && iGeomField < GetGeomFieldCount() )
    {
        OGRGeometry *poReturn = papoGeometries[iGeomField];
        papoGeometries[iGeomField] = nullptr;
        return poReturn;
    }

    return nullptr;
}

/************************************************************************/
/*                        OGR_F_StealGeometry()                         */
/************************************************************************/

/**
 * \brief Take away ownership of geometry.
 *
 * Fetch the geometry from this feature, and clear the reference to the
 * geometry on the feature.  This is a mechanism for the application to
 * take over ownership of the geometry from the feature without copying.
 * Sort of an inverse to OGR_FSetGeometryDirectly().
 *
 * After this call the OGRFeature will have a NULL geometry.
 *
 * @param hFeat feature from which to steal the first geometry.
 * @return the pointer to the stolen geometry.
 */

OGRGeometryH OGR_F_StealGeometry( OGRFeatureH hFeat )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_StealGeometry", nullptr );

    return OGRGeometry::ToHandle(
        OGRFeature::FromHandle(hFeat)->StealGeometry());
}

/************************************************************************/
/*                       OGR_F_StealGeometryEx()                        */
/************************************************************************/

/**
 * \brief Take away ownership of geometry.
 *
 * Fetch the geometry from this feature, and clear the reference to the
 * geometry on the feature.  This is a mechanism for the application to
 * take over ownership of the geometry from the feature without copying.
 * This is the functional opposite of OGR_F_SetGeomFieldDirectly.
 *
 * After this call the OGRFeature will have a NULL geometry for the
 * geometry field of index iGeomField.
 *
 * @param hFeat feature from which to steal a geometry.
 * @param iGeomField index of the geometry field to steal.
 * @return the pointer to the stolen geometry.
 * @since GDAL 3.5
 */

OGRGeometryH OGR_F_StealGeometryEx( OGRFeatureH hFeat, int iGeomField )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_StealGeometryEx", nullptr );

    return OGRGeometry::ToHandle(
        OGRFeature::FromHandle(hFeat)->StealGeometry( iGeomField ));
}

/************************************************************************/
/*                           GetGeometryRef()                           */
/************************************************************************/

/**
 * \fn OGRGeometry *OGRFeature::GetGeometryRef();
 *
 * \brief Fetch pointer to feature geometry.
 *
 * This method is essentially the same as the C function OGR_F_GetGeometryRef().
 * (the only difference is that the C function honours OGRGetNonLinearGeometriesEnabledFlag())
 *
 * Starting with GDAL 1.11, this is equivalent to calling
 * OGRFeature::GetGeomFieldRef(0).
 *
 * @return pointer to internal feature geometry.  This object should
 * not be modified.
 */
OGRGeometry *OGRFeature::GetGeometryRef()

{
    if( GetGeomFieldCount() > 0 )
        return GetGeomFieldRef(0);

    return nullptr;
}

/**
 * \fn const OGRGeometry *OGRFeature::GetGeometryRef() const;
 *
 * \brief Fetch pointer to feature geometry.
 *
 * This method is essentially the same as the C function OGR_F_GetGeometryRef().
 * (the only difference is that the C function honours OGRGetNonLinearGeometriesEnabledFlag())
 *
 * @return pointer to internal feature geometry.  This object should
 * not be modified.
 * @since GDAL 2.3
 */
const OGRGeometry *OGRFeature::GetGeometryRef() const

{
    if( GetGeomFieldCount() > 0 )
        return GetGeomFieldRef(0);

    return nullptr;
}

/************************************************************************/
/*                        OGR_F_GetGeometryRef()                        */
/************************************************************************/

/**
 * \brief Fetch a handle to feature geometry.
 *
 * This function is essentially the same as the C++ method OGRFeature::GetGeometryRef()
 * (the only difference is that this C function honours OGRGetNonLinearGeometriesEnabledFlag())
 *
 * @param hFeat handle to the feature to get geometry from.
 * @return a handle to internal feature geometry.  This object should
 * not be modified.
 */

OGRGeometryH OGR_F_GetGeometryRef( OGRFeatureH hFeat )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetGeometryRef", nullptr );

    OGRFeature* poFeature = OGRFeature::FromHandle(hFeat);
    OGRGeometry* poGeom = poFeature->GetGeometryRef();

    if( !OGRGetNonLinearGeometriesEnabledFlag() && poGeom != nullptr &&
        OGR_GT_IsNonLinear(poGeom->getGeometryType()) )
    {
        const OGRwkbGeometryType eTargetType =
            OGR_GT_GetLinear(poGeom->getGeometryType());
        poGeom = OGRGeometryFactory::forceTo(poFeature->StealGeometry(),
                                             eTargetType);
        poFeature->SetGeomFieldDirectly(0, poGeom);
        poGeom = poFeature->GetGeometryRef();
    }

    return OGRGeometry::ToHandle(poGeom);
}

/************************************************************************/
/*                           GetGeomFieldRef()                          */
/************************************************************************/

/**
 * \brief Fetch pointer to feature geometry.
 *
 * This method is the same as the C function OGR_F_GetGeomFieldRef().
 *
 * @param iField geometry field to get.
 *
 * @return pointer to internal feature geometry.  This object should
 * not be modified.
 *
 * @since GDAL 1.11
 */
OGRGeometry *OGRFeature::GetGeomFieldRef( int iField )

{
    if( iField < 0 || iField >= GetGeomFieldCount() )
        return nullptr;
    else
        return papoGeometries[iField];
}

/**
 * \brief Fetch pointer to feature geometry.
 *
 * This method is the same as the C function OGR_F_GetGeomFieldRef().
 *
 * @param iField geometry field to get.
 *
 * @return pointer to internal feature geometry.  This object should
 * not be modified.
 * @since GDAL 2.3
 */
const OGRGeometry *OGRFeature::GetGeomFieldRef( int iField ) const

{
    if( iField < 0 || iField >= GetGeomFieldCount() )
        return nullptr;
    else
        return papoGeometries[iField];
}

/************************************************************************/
/*                           GetGeomFieldRef()                          */
/************************************************************************/

/**
 * \brief Fetch pointer to feature geometry.
 *
 * @param pszFName name of geometry field to get.
 *
 * @return pointer to internal feature geometry.  This object should
 * not be modified.
 *
 * @since GDAL 1.11
 */
OGRGeometry *OGRFeature::GetGeomFieldRef( const char* pszFName )

{
    const int iField = GetGeomFieldIndex(pszFName);
    if( iField < 0 )
        return nullptr;

    return papoGeometries[iField];
}

/**
 * \brief Fetch pointer to feature geometry.
 *
 * @param pszFName name of geometry field to get.
 *
 * @return pointer to internal feature geometry.  This object should
 * not be modified.
 * @since GDAL 2.3
 */
const OGRGeometry *OGRFeature::GetGeomFieldRef( const char* pszFName ) const

{
    const int iField = GetGeomFieldIndex(pszFName);
    if( iField < 0 )
        return nullptr;

    return papoGeometries[iField];
}

/************************************************************************/
/*                       OGR_F_GetGeomFieldRef()                        */
/************************************************************************/

/**
 * \brief Fetch a handle to feature geometry.
 *
 * This function is the same as the C++ method OGRFeature::GetGeomFieldRef().
 *
 * @param hFeat handle to the feature to get geometry from.
 * @param iField geometry field to get.
 * @return a handle to internal feature geometry.  This object should
 * not be modified.
 *
 * @since GDAL 1.11
 */

OGRGeometryH OGR_F_GetGeomFieldRef( OGRFeatureH hFeat, int iField )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetGeomFieldRef", nullptr );

    OGRFeature* poFeature = OGRFeature::FromHandle(hFeat);
    OGRGeometry* poGeom = poFeature->GetGeomFieldRef(iField);

    if( !OGRGetNonLinearGeometriesEnabledFlag() && poGeom != nullptr &&
        OGR_GT_IsNonLinear(poGeom->getGeometryType()) )
    {
        const OGRwkbGeometryType eTargetType =
            OGR_GT_GetLinear(poGeom->getGeometryType());
        poGeom = OGRGeometryFactory::forceTo(poFeature->StealGeometry(iField),
                                             eTargetType);
        poFeature->SetGeomFieldDirectly(iField, poGeom);
        poGeom = poFeature->GetGeomFieldRef(iField);
    }

    return OGRGeometry::ToHandle(poGeom);
}

/************************************************************************/
/*                       SetGeomFieldDirectly()                         */
/************************************************************************/

/**
 * \brief Set feature geometry of a specified geometry field.
 *
 * This method updates the features geometry, and operate exactly as
 * SetGeomField(), except that this method assumes ownership of the
 * passed geometry (even in case of failure of that function).
 *
 * This method is the same as the C function OGR_F_SetGeomFieldDirectly().
 *
 * @param iField geometry field to set.
 * @param poGeomIn new geometry to apply to feature. Passing NULL value here
 * is correct and it will result in deallocation of currently assigned geometry
 * without assigning new one.
 *
 * @return OGRERR_NONE if successful, or OGRERR_FAILURE if the index is invalid,
 * or OGRERR_UNSUPPORTED_GEOMETRY_TYPE if the geometry type is illegal for the
 * OGRFeatureDefn (checking not yet implemented).
 *
 * @since GDAL 1.11
 */

OGRErr OGRFeature::SetGeomFieldDirectly( int iField, OGRGeometry * poGeomIn )

{
    if( iField < 0 || iField >= GetGeomFieldCount() )
    {
        delete poGeomIn;
        return OGRERR_FAILURE;
    }

    if( papoGeometries[iField] != poGeomIn )
    {
        delete papoGeometries[iField];
        papoGeometries[iField] = poGeomIn;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                     OGR_F_SetGeomFieldDirectly()                     */
/************************************************************************/

/**
 * \brief Set feature geometry of a specified geometry field.
 *
 * This function updates the features geometry, and operate exactly as
 * SetGeomField(), except that this function assumes ownership of the
 * passed geometry (even in case of failure of that function).
 *
 * This function is the same as the C++ method
 * OGRFeature::SetGeomFieldDirectly.
 *
 * @param hFeat handle to the feature on which to apply the geometry.
 * @param iField geometry field to set.
 * @param hGeom handle to the new geometry to apply to feature.
 *
 * @return OGRERR_NONE if successful, or OGRERR_FAILURE if the index is invalid,
 * or OGR_UNSUPPORTED_GEOMETRY_TYPE if the geometry type is illegal for the
 * OGRFeatureDefn (checking not yet implemented).
 *
 * @since GDAL 1.11
 */

OGRErr OGR_F_SetGeomFieldDirectly( OGRFeatureH hFeat, int iField,
                                   OGRGeometryH hGeom )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_SetGeomFieldDirectly", OGRERR_FAILURE );

    return OGRFeature::FromHandle(hFeat)->
        SetGeomFieldDirectly(iField, OGRGeometry::FromHandle(hGeom));
}

/************************************************************************/
/*                            SetGeomField()                            */
/************************************************************************/

/**
 * \brief Set feature geometry of a specified geometry field.
 *
 * This method updates the features geometry, and operate exactly as
 * SetGeomFieldDirectly(), except that this method does not assume ownership
 * of the passed geometry, but instead makes a copy of it.
 *
 * This method is the same as the C function OGR_F_SetGeomField().
 *
 * @param iField geometry field to set.
 * @param poGeomIn new geometry to apply to feature. Passing NULL value here
 * is correct and it will result in deallocation of currently assigned geometry
 * without assigning new one.
 *
 * @return OGRERR_NONE if successful, or OGRERR_FAILURE if the index is invalid,
 * or OGR_UNSUPPORTED_GEOMETRY_TYPE if the geometry type is illegal for the
 * OGRFeatureDefn (checking not yet implemented).
 *
 * @since GDAL 1.11
 */

OGRErr OGRFeature::SetGeomField( int iField, const OGRGeometry * poGeomIn )

{
    if( iField < 0 || iField >= GetGeomFieldCount() )
        return OGRERR_FAILURE;

    if( papoGeometries[iField] != poGeomIn )
    {
        delete papoGeometries[iField];

        if( poGeomIn != nullptr )
            papoGeometries[iField] = poGeomIn->clone();
        else
            papoGeometries[iField] = nullptr;
    }

    // TODO(schwehr): Verify that the geometry matches the defn's type.

    return OGRERR_NONE;
}

/************************************************************************/
/*                        OGR_F_SetGeomField()                          */
/************************************************************************/

/**
 * \brief Set feature geometry of a specified geometry field.
 *
 * This function updates the features geometry, and operate exactly as
 * SetGeometryDirectly(), except that this function does not assume ownership
 * of the passed geometry, but instead makes a copy of it.
 *
 * This function is the same as the C++ OGRFeature::SetGeomField().
 *
 * @param hFeat handle to the feature on which new geometry is applied to.
 * @param iField geometry field to set.
 * @param hGeom handle to the new geometry to apply to feature.
 *
 * @return OGRERR_NONE if successful, or OGR_UNSUPPORTED_GEOMETRY_TYPE if
 * the geometry type is illegal for the OGRFeatureDefn (checking not yet
 * implemented).
 */

OGRErr OGR_F_SetGeomField( OGRFeatureH hFeat, int iField, OGRGeometryH hGeom )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_SetGeomField", OGRERR_FAILURE );

    return OGRFeature::FromHandle(hFeat)->
        SetGeomField(iField, OGRGeometry::FromHandle(hGeom));
}

/************************************************************************/
/*                               Clone()                                */
/************************************************************************/

/**
 * \brief Duplicate feature.
 *
 * The newly created feature is owned by the caller, and will have its own
 * reference to the OGRFeatureDefn.
 *
 * This method is the same as the C function OGR_F_Clone().
 *
 * @return new feature, exactly matching this feature. Or, starting with GDAL
 * 2.1, NULL in case of out of memory situation.
 */

OGRFeature *OGRFeature::Clone() const

{
    OGRFeature *poNew = CreateFeature( poDefn );
    if( poNew == nullptr )
        return nullptr;

    if( !CopySelfTo( poNew ) )
    {
        delete poNew;
        return nullptr;
    }

    return poNew;
}

/************************************************************************/
/*                            OGR_F_Clone()                             */
/************************************************************************/

/**
 * \brief Duplicate feature.
 *
 * The newly created feature is owned by the caller, and will have its own
 * reference to the OGRFeatureDefn.
 *
 * This function is the same as the C++ method OGRFeature::Clone().
 *
 * @param hFeat handle to the feature to clone.
 * @return a handle to the new feature, exactly matching this feature.
 */

OGRFeatureH OGR_F_Clone( OGRFeatureH hFeat )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_Clone", nullptr );

    return OGRFeature::ToHandle(
        OGRFeature::FromHandle(hFeat)->Clone());
}

/************************************************************************/
/*                             CopySelfTo()                             */
/************************************************************************/

/**
* \brief Copies the innards of this OGRFeature into the supplied object.
*
* This is mainly intended to allow derived classes to implement their own
* Clone functions.
*
* @param poNew The object into which to copy the data of this object.
* @return True if successful, false if the copy failed.
*/

bool OGRFeature::CopySelfTo( OGRFeature* poNew ) const
{
    for( int i = 0; i < poDefn->GetFieldCount(); i++ )
    {
        if( !poNew->SetFieldInternal( i, pauFields + i ) )
        {
            return false;
        }
    }
    if( poNew->papoGeometries )
    {
        for( int i = 0; i < poDefn->GetGeomFieldCount(); i++ )
        {
            if( papoGeometries[i] != nullptr )
            {
                poNew->papoGeometries[i] = papoGeometries[i]->clone();
                if( poNew->papoGeometries[i] == nullptr )
                {
                    return false;
                }
            }
        }
    }

    if( m_pszStyleString != nullptr )
    {
        poNew->m_pszStyleString = VSI_STRDUP_VERBOSE(m_pszStyleString);
        if( poNew->m_pszStyleString == nullptr )
        {
            return false;
        }
    }

    poNew->SetFID( GetFID() );

    if( m_pszNativeData != nullptr )
    {
        poNew->m_pszNativeData = VSI_STRDUP_VERBOSE(m_pszNativeData);
        if( poNew->m_pszNativeData == nullptr )
        {
            return false;
        }
    }

    if( m_pszNativeMediaType != nullptr )
    {
        poNew->m_pszNativeMediaType = VSI_STRDUP_VERBOSE(m_pszNativeMediaType);
        if( poNew->m_pszNativeMediaType == nullptr )
        {
            return false;
        }
    }

    return true;
}

/************************************************************************/
/*                           GetFieldCount()                            */
/************************************************************************/

/**
 * \fn int OGRFeature::GetFieldCount() const;
 *
 * \brief Fetch number of fields on this feature.
 * This will always be the same
 * as the field count for the OGRFeatureDefn.
 *
 * This method is the same as the C function OGR_F_GetFieldCount().
 *
 * @return count of fields.
 */

/************************************************************************/
/*                        OGR_F_GetFieldCount()                         */
/************************************************************************/

/**
 * \brief Fetch number of fields on this feature
 * This will always be the same
 * as the field count for the OGRFeatureDefn.
 *
 * This function is the same as the C++ method OGRFeature::GetFieldCount().
 *
 * @param hFeat handle to the feature to get the fields count from.
 * @return count of fields.
 */

int OGR_F_GetFieldCount( OGRFeatureH hFeat )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetFieldCount", 0 );

    return OGRFeature::FromHandle(hFeat)->GetFieldCount();
}

/************************************************************************/
/*                          GetFieldDefnRef()                           */
/************************************************************************/

/**
 * \fn OGRFieldDefn *OGRFeature::GetFieldDefnRef( int iField );
 *
 * \brief Fetch definition for this field.
 *
 * This method is the same as the C function OGR_F_GetFieldDefnRef().
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 *
 * @return the field definition (from the OGRFeatureDefn).  This is an
 * internal reference, and should not be deleted or modified.
 */

/**
 * \fn const OGRFieldDefn *OGRFeature::GetFieldDefnRef( int iField ) const;
 *
 * \brief Fetch definition for this field.
 *
 * This method is the same as the C function OGR_F_GetFieldDefnRef().
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 *
 * @return the field definition (from the OGRFeatureDefn).  This is an
 * internal reference, and should not be deleted or modified.
 * @since GDAL 2.3
 */

/************************************************************************/
/*                       OGR_F_GetFieldDefnRef()                        */
/************************************************************************/

/**
 * \brief Fetch definition for this field.
 *
 * This function is the same as the C++ method OGRFeature::GetFieldDefnRef().
 *
 * @param hFeat handle to the feature on which the field is found.
 * @param i the field to fetch, from 0 to GetFieldCount()-1.
 *
 * @return a handle to the field definition (from the OGRFeatureDefn).
 * This is an internal reference, and should not be deleted or modified.
 */

OGRFieldDefnH OGR_F_GetFieldDefnRef( OGRFeatureH hFeat, int i )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetFieldDefnRef", nullptr );

    OGRFeature *poFeat = OGRFeature::FromHandle(hFeat);

    return OGRFieldDefn::ToHandle(poFeat->GetFieldDefnRef(i));
}

/************************************************************************/
/*                           GetFieldIndex()                            */
/************************************************************************/

/**
 * \fn int OGRFeature::GetFieldIndex( const char * pszName ) const;
 *
 * \brief Fetch the field index given field name.
 *
 * This is a cover for the OGRFeatureDefn::GetFieldIndex() method.
 *
 * This method is the same as the C function OGR_F_GetFieldIndex().
 *
 * @param pszName the name of the field to search for.
 *
 * @return the field index, or -1 if no matching field is found.
 */

/************************************************************************/
/*                        OGR_F_GetFieldIndex()                         */
/************************************************************************/

/**
 * \brief Fetch the field index given field name.
 *
 * This is a cover for the OGRFeatureDefn::GetFieldIndex() method.
 *
 * This function is the same as the C++ method OGRFeature::GetFieldIndex().
 *
 * @param hFeat handle to the feature on which the field is found.
 * @param pszName the name of the field to search for.
 *
 * @return the field index, or -1 if no matching field is found.
 */

int OGR_F_GetFieldIndex( OGRFeatureH hFeat, const char *pszName )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetFieldIndex", 0 );

    return OGRFeature::FromHandle(hFeat)->GetFieldIndex( pszName );
}

/************************************************************************/
/*                         GetGeomFieldCount()                          */
/************************************************************************/

/**
 * \fn int OGRFeature::GetGeomFieldCount() const;
 *
 * \brief Fetch number of geometry fields on this feature.
 * This will always be the same
 * as the geometry field count for the OGRFeatureDefn.
 *
 * This method is the same as the C function OGR_F_GetGeomFieldCount().
 *
 * @return count of geometry fields.
 *
 * @since GDAL 1.11
 */

/************************************************************************/
/*                      OGR_F_GetGeomFieldCount()                       */
/************************************************************************/

/**
 * \brief Fetch number of geometry fields on this feature
 * This will always be the same
 * as the geometry field count for the OGRFeatureDefn.
 *
 * This function is the same as the C++ method OGRFeature::GetGeomFieldCount().
 *
 * @param hFeat handle to the feature to get the geometry fields count from.
 * @return count of geometry fields.
 *
 * @since GDAL 1.11
 */

int OGR_F_GetGeomFieldCount( OGRFeatureH hFeat )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetGeomFieldCount", 0 );

    return OGRFeature::FromHandle(hFeat)->GetGeomFieldCount();
}

/************************************************************************/
/*                        GetGeomFieldDefnRef()                         */
/************************************************************************/

/**
 * \fn OGRGeomFieldDefn *OGRFeature::GetGeomFieldDefnRef( int iGeomField );
 *
 * \brief Fetch definition for this geometry field.
 *
 * This method is the same as the C function OGR_F_GetGeomFieldDefnRef().
 *
 * @param iGeomField the field to fetch, from 0 to GetGeomFieldCount()-1.
 *
 * @return the field definition (from the OGRFeatureDefn).  This is an
 * internal reference, and should not be deleted or modified.
 *
 * @since GDAL 1.11
 */

/**
 * \fn const OGRGeomFieldDefn *OGRFeature::GetGeomFieldDefnRef( int iGeomField ) const;
 *
 * \brief Fetch definition for this geometry field.
 *
 * This method is the same as the C function OGR_F_GetGeomFieldDefnRef().
 *
 * @param iGeomField the field to fetch, from 0 to GetGeomFieldCount()-1.
 *
 * @return the field definition (from the OGRFeatureDefn).  This is an
 * internal reference, and should not be deleted or modified.
 *
* @since GDAL 2.3
 */

/************************************************************************/
/*                       OGR_F_GetGeomFieldDefnRef()                    */
/************************************************************************/

/**
 * \brief Fetch definition for this geometry field.
 *
 * This function is the same as the C++ method
 * OGRFeature::GetGeomFieldDefnRef().
 *
 * @param hFeat handle to the feature on which the field is found.
 * @param i the field to fetch, from 0 to GetGeomFieldCount()-1.
 *
 * @return a handle to the field definition (from the OGRFeatureDefn).
 * This is an internal reference, and should not be deleted or modified.
 *
 * @since GDAL 1.11
 */

OGRGeomFieldDefnH OGR_F_GetGeomFieldDefnRef( OGRFeatureH hFeat, int i )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetGeomFieldDefnRef", nullptr );

    return OGRGeomFieldDefn::ToHandle(
        OGRFeature::FromHandle(hFeat)->GetGeomFieldDefnRef(i));
}

/************************************************************************/
/*                           GetGeomFieldIndex()                            */
/************************************************************************/

/**
 * \fn int OGRFeature::GetGeomFieldIndex( const char * pszName );
 *
 * \brief Fetch the geometry field index given geometry field name.
 *
 * This is a cover for the OGRFeatureDefn::GetGeomFieldIndex() method.
 *
 * This method is the same as the C function OGR_F_GetGeomFieldIndex().
 *
 * @param pszName the name of the geometry field to search for.
 *
 * @return the geometry field index, or -1 if no matching geometry field is
 * found.
 *
 * @since GDAL 1.11
 */

/************************************************************************/
/*                       OGR_F_GetGeomFieldIndex()                      */
/************************************************************************/

/**
 * \brief Fetch the geometry field index given geometry field name.
 *
 * This is a cover for the OGRFeatureDefn::GetGeomFieldIndex() method.
 *
 * This function is the same as the C++ method OGRFeature::GetGeomFieldIndex().
 *
 * @param hFeat handle to the feature on which the geometry field is found.
 * @param pszName the name of the geometry field to search for.
 *
 * @return the geometry field index, or -1 if no matching geometry field is
 * found.
 *
 * @since GDAL 1.11
 */

int OGR_F_GetGeomFieldIndex( OGRFeatureH hFeat, const char *pszName )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetGeomFieldIndex", 0 );

    return OGRFeature::FromHandle(hFeat)->GetGeomFieldIndex( pszName );
}

/************************************************************************/
/*                             IsFieldSet()                             */
/************************************************************************/

/**
 * \brief Test if a field has ever been assigned a value or not.
 *
 * This method is the same as the C function OGR_F_IsFieldSet().
 *
 * @param iField the field to test.
 *
 * @return TRUE if the field has been set, otherwise false.
 */

int OGRFeature::IsFieldSet( int iField ) const

{
    const int iSpecialField = iField - poDefn->GetFieldCount();
    if( iSpecialField >= 0 )
    {
        // Special field value accessors.
        switch( iSpecialField )
        {
          case SPF_FID:
            return GetFID() != OGRNullFID;

          case SPF_OGR_GEOM_WKT:
          case SPF_OGR_GEOMETRY:
            return GetGeomFieldCount() > 0 && papoGeometries[0] != nullptr;

          case SPF_OGR_STYLE:
            return GetStyleString() != nullptr;

          case SPF_OGR_GEOM_AREA:
            if( GetGeomFieldCount() == 0 || papoGeometries[0] == nullptr )
                return FALSE;

            return OGR_G_Area(
                OGRGeometry::ToHandle(papoGeometries[0])) != 0.0;

          default:
            return FALSE;
        }
    }
    else
    {
        return !OGR_RawField_IsUnset(&pauFields[iField]);
    }
}

/************************************************************************/
/*                          OGR_F_IsFieldSet()                          */
/************************************************************************/

/**
 * \brief Test if a field has ever been assigned a value or not.
 *
 * This function is the same as the C++ method OGRFeature::IsFieldSet().
 *
 * @param hFeat handle to the feature on which the field is.
 * @param iField the field to test.
 *
 * @return TRUE if the field has been set, otherwise false.
 */

int OGR_F_IsFieldSet( OGRFeatureH hFeat, int iField )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_IsFieldSet", 0 );

    const OGRFeature* poFeature = OGRFeature::FromHandle(hFeat);
    if( iField < 0 || iField >= poFeature->GetFieldCount() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid index : %d", iField);
        return FALSE;
    }

    return poFeature->IsFieldSet( iField );
}

/************************************************************************/
/*                             UnsetField()                             */
/************************************************************************/

/**
 * \brief Clear a field, marking it as unset.
 *
 * This method is the same as the C function OGR_F_UnsetField().
 *
 * @param iField the field to unset.
 */

void OGRFeature::UnsetField( int iField )

{
    OGRFieldDefn *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == nullptr || !IsFieldSet(iField) )
        return;

    if( !IsFieldNull(iField) )
    {
        switch( poFDefn->GetType() )
        {
            case OFTRealList:
            case OFTIntegerList:
            case OFTInteger64List:
                CPLFree( pauFields[iField].IntegerList.paList );
                break;

            case OFTStringList:
                CSLDestroy( pauFields[iField].StringList.paList );
                break;

            case OFTString:
                CPLFree( pauFields[iField].String );
                break;

            case OFTBinary:
                CPLFree( pauFields[iField].Binary.paData );
                break;

            default:
                break;
        }
    }

    OGR_RawField_SetUnset(&pauFields[iField]);
}

/************************************************************************/
/*                          OGR_F_UnsetField()                          */
/************************************************************************/

/**
 * \brief Clear a field, marking it as unset.
 *
 * This function is the same as the C++ method OGRFeature::UnsetField().
 *
 * @param hFeat handle to the feature on which the field is.
 * @param iField the field to unset.
 */

void OGR_F_UnsetField( OGRFeatureH hFeat, int iField )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_UnsetField" );

    OGRFeature::FromHandle(hFeat)->UnsetField( iField );
}


/************************************************************************/
/*                            IsFieldNull()                             */
/************************************************************************/

/**
 * \brief Test if a field is null.
 *
 * This method is the same as the C function OGR_F_IsFieldNull().
 *
 * @param iField the field to test.
 *
 * @return TRUE if the field is null, otherwise false.
 *
 * @since GDAL 2.2
 */

bool OGRFeature::IsFieldNull( int iField ) const

{
    const int iSpecialField = iField - poDefn->GetFieldCount();
    if( iSpecialField >= 0 )
    {
        // FIXME?
        return false;
    }
    else
    {
        return CPL_TO_BOOL(OGR_RawField_IsNull(&pauFields[iField]));
    }
}

/************************************************************************/
/*                          OGR_F_IsFieldNull()                         */
/************************************************************************/

/**
 * \brief Test if a field is null.
 *
 * This function is the same as the C++ method OGRFeature::IsFieldNull().
 *
 * @param hFeat handle to the feature on which the field is.
 * @param iField the field to test.
 *
 * @return TRUE if the field is null, otherwise false.
*
 * @since GDAL 2.2
 */

int OGR_F_IsFieldNull( OGRFeatureH hFeat, int iField )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_IsFieldNull", 0 );

    const OGRFeature* poFeature = OGRFeature::FromHandle(hFeat);
    if( iField < 0 || iField >= poFeature->GetFieldCount() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid index : %d", iField);
        return FALSE;
    }

    return poFeature->IsFieldNull( iField );
}

/************************************************************************/
/*                       IsFieldSetAndNull()                            */
/************************************************************************/

/**
 * \brief Test if a field is set and not null.
 *
 * This method is the same as the C function OGR_F_IsFieldSetAndNotNull().
 *
 * @param iField the field to test.
 *
 * @return TRUE if the field is set and not null, otherwise false.
 *
 * @since GDAL 2.2
 */

bool OGRFeature::IsFieldSetAndNotNull( int iField ) const

{
    const int iSpecialField = iField - poDefn->GetFieldCount();
    if( iSpecialField >= 0 )
    {
        return CPL_TO_BOOL(IsFieldSet(iField));
    }
    else
    {
        return IsFieldSetAndNotNullUnsafe(iField);
    }
}

/************************************************************************/
/*                      OGR_F_IsFieldSetAndNotNull()                    */
/************************************************************************/

/**
 * \brief Test if a field is set and not null.
 *
 * This function is the same as the C++ method OGRFeature::IsFieldSetAndNotNull().
 *
 * @param hFeat handle to the feature on which the field is.
 * @param iField the field to test.
 *
 * @return TRUE if the field is set and not null, otherwise false.
*
 * @since GDAL 2.2
 */

int OGR_F_IsFieldSetAndNotNull( OGRFeatureH hFeat, int iField )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_IsFieldSetAndNotNull", 0 );

    OGRFeature* poFeature = OGRFeature::FromHandle(hFeat);

    if( iField < 0 || iField >= poFeature->GetFieldCount() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid index : %d", iField);
        return FALSE;
    }

    return poFeature->IsFieldSetAndNotNull( iField );
}

/************************************************************************/
/*                             SetFieldNull()                           */
/************************************************************************/

/**
 * \brief Clear a field, marking it as null.
 *
 * This method is the same as the C function OGR_F_SetFieldNull().
 *
 * @param iField the field to set to null.
 *
 * @since GDAL 2.2
 */

void OGRFeature::SetFieldNull( int iField )

{
    OGRFieldDefn *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == nullptr || IsFieldNull(iField) )
        return;

    if( IsFieldSet(iField) )
    {
        switch( poFDefn->GetType() )
        {
            case OFTRealList:
            case OFTIntegerList:
            case OFTInteger64List:
                CPLFree( pauFields[iField].IntegerList.paList );
                break;

            case OFTStringList:
                CSLDestroy( pauFields[iField].StringList.paList );
                break;

            case OFTString:
                CPLFree( pauFields[iField].String );
                break;

            case OFTBinary:
                CPLFree( pauFields[iField].Binary.paData );
                break;

            default:
                break;
        }
    }

    OGR_RawField_SetNull(&pauFields[iField]);
}

/************************************************************************/
/*                          OGR_F_SetFieldNull()                        */
/************************************************************************/

/**
 * \brief Clear a field, marking it as null.
 *
 * This function is the same as the C++ method OGRFeature::SetFieldNull().
 *
 * @param hFeat handle to the feature on which the field is.
 * @param iField the field to set to null.
 *
 * @since GDAL 2.2
 */

void OGR_F_SetFieldNull( OGRFeatureH hFeat, int iField )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetFieldNull" );

    OGRFeature::FromHandle(hFeat)->SetFieldNull( iField );
}

/************************************************************************/
/*                           operator[]                                */
/************************************************************************/

/**
* \brief Return a field value.
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1. This is not
 *               checked by the method !
 *
 * @return the field value.
 * @since GDAL 2.3
 */
const OGRFeature::FieldValue OGRFeature::operator[](int iField) const
{
    return {this, iField};
}

/**
* \brief Return a field value.
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1. This is not
 *               checked by the method !
 *
 * @return the field value.
 * @since GDAL 2.3
 */
OGRFeature::FieldValue OGRFeature::operator[](int iField)
{
    return {this, iField};
}

/**
* \brief Return a field value.
 *
 * @param pszFieldName field name
 *
 * @return the field value, or throw a FieldNotFoundException if not found.
 * @since GDAL 2.3
 */
const OGRFeature::FieldValue OGRFeature::operator[](const char* pszFieldName) const
{
    int iField = GetFieldIndex(pszFieldName);
    if( iField < 0 )
        throw FieldNotFoundException();
    return {this, iField};
}

/**
* \brief Return a field value.
 *
 * @param pszFieldName field name
 *
 * @return the field value, or throw a FieldNotFoundException if not found.
 * @since GDAL 2.3
 */
OGRFeature::FieldValue OGRFeature::operator[](const char* pszFieldName)
{
    int iField = GetFieldIndex(pszFieldName);
    if( iField < 0 )
        throw FieldNotFoundException();
    return {this, iField};
}


/************************************************************************/
/*                           GetRawFieldRef()                           */
/************************************************************************/

/**
 * \fn OGRField *OGRFeature::GetRawFieldRef( int iField );
 *
 * \brief Fetch a pointer to the internal field value given the index.
 *
 * This method is the same as the C function OGR_F_GetRawFieldRef().
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 *
 * @return the returned pointer is to an internal data structure, and should
 * not be freed, or modified.
 */

/**
 * \fn const OGRField *OGRFeature::GetRawFieldRef( int iField ) const;
 *
 * \brief Fetch a pointer to the internal field value given the index.
 *
 * This method is the same as the C function OGR_F_GetRawFieldRef().
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 *
 * @return the returned pointer is to an internal data structure, and should
 * not be freed, or modified.
 * @since GDAL 2.3
 */

/************************************************************************/
/*                        OGR_F_GetRawFieldRef()                        */
/************************************************************************/

/**
 * \brief Fetch a handle to the internal field value given the index.
 *
 * This function is the same as the C++ method OGRFeature::GetRawFieldRef().
 *
 * @param hFeat handle to the feature on which field is found.
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 *
 * @return the returned handle is to an internal data structure, and should
 * not be freed, or modified.
 */

OGRField *OGR_F_GetRawFieldRef( OGRFeatureH hFeat, int iField )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetRawFieldRef", nullptr );

    return OGRFeature::FromHandle(hFeat)->GetRawFieldRef( iField );
}

/************************************************************************/
/*                         GetFieldAsInteger()                          */
/************************************************************************/

/**
 * \fn OGRFeature::GetFieldAsInteger( const char* pszFName ) const
 * \brief Fetch field value as integer.
 *
 * OFTString features will be translated using atoi().  OFTReal fields
 * will be cast to integer. OFTInteger64 are demoted to 32 bit, with
 * clamping if out-of-range. Other field types, or errors will result in
 * a return value of zero.
 *
 * @param pszFName the name of the field to fetch.
 *
 * @return the field value.
 */

/**
 * \brief Fetch field value as integer.
 *
 * OFTString features will be translated using atoi().  OFTReal fields
 * will be cast to integer. OFTInteger64 are demoted to 32 bit, with
 * clamping if out-of-range. Other field types, or errors will result in
 * a return value of zero.
 *
 * This method is the same as the C function OGR_F_GetFieldAsInteger().
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 *
 * @return the field value.
 */

int OGRFeature::GetFieldAsInteger( int iField ) const

{
    int iSpecialField = iField - poDefn->GetFieldCount();
    if( iSpecialField >= 0 )
    {
        // Special field value accessors.
        switch( iSpecialField )
        {
        case SPF_FID:
        {
            const int nVal =
                nFID > INT_MAX ? INT_MAX :
                nFID < INT_MIN ? INT_MIN : static_cast<int>(nFID);

            if( static_cast<GIntBig>(nVal) != nFID )
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Integer overflow occurred when trying to return "
                          "64bit integer. Use GetFieldAsInteger64() instead" );
            }
            return nVal;
        }

        case SPF_OGR_GEOM_AREA:
            if( GetGeomFieldCount() == 0 || papoGeometries[0] == nullptr )
                return 0;
            return static_cast<int>(
                OGR_G_Area(OGRGeometry::ToHandle(papoGeometries[0])));

        default:
            return 0;
        }
    }

    OGRFieldDefn *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == nullptr )
        return 0;

    if( !IsFieldSetAndNotNullUnsafe(iField) )
        return 0;

    const OGRFieldType eType = poFDefn->GetType();
    if( eType == OFTInteger )
    {
        return pauFields[iField].Integer;
    }
    else if( eType == OFTInteger64 )
    {
        const GIntBig nVal64 = pauFields[iField].Integer64;
        const int nVal =
            nVal64 > INT_MAX ? INT_MAX :
            nVal64 < INT_MIN ? INT_MIN : static_cast<int>(nVal64);

        if( static_cast<GIntBig>(nVal) != nVal64 )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Integer overflow occurred when trying to return 64bit "
                      "integer. Use GetFieldAsInteger64() instead");
        }
        return nVal;
    }
    else if( eType == OFTReal )
    {
        return static_cast<int>(pauFields[iField].Real);
    }
    else if( eType == OFTString )
    {
        if( pauFields[iField].String == nullptr )
            return 0;
        else
            return atoi(pauFields[iField].String);
    }

    return 0;
}

/************************************************************************/
/*                      OGR_F_GetFieldAsInteger()                       */
/************************************************************************/

/**
 * \brief Fetch field value as integer.
 *
 * OFTString features will be translated using atoi().  OFTReal fields
 * will be cast to integer.   Other field types, or errors will result in
 * a return value of zero.
 *
 * This function is the same as the C++ method OGRFeature::GetFieldAsInteger().
 *
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 *
 * @return the field value.
 */

int OGR_F_GetFieldAsInteger( OGRFeatureH hFeat, int iField )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetFieldAsInteger", 0 );

    return OGRFeature::FromHandle(hFeat)->GetFieldAsInteger(iField);
}

/************************************************************************/
/*                        GetFieldAsInteger64()                         */
/************************************************************************/

/**
 * \fn OGRFeature::GetFieldAsInteger64( const char* pszFName ) const
 * \brief Fetch field value as integer 64 bit.
 *
 * OFTInteger are promoted to 64 bit.
 * OFTString features will be translated using CPLAtoGIntBig().  OFTReal fields
 * will be cast to integer.   Other field types, or errors will result in
 * a return value of zero.
 *
 * @param pszFName the name of the field to fetch.
 *
 * @return the field value.
 */

/**
 * \brief Fetch field value as integer 64 bit.
 *
 * OFTInteger are promoted to 64 bit.
 * OFTString features will be translated using CPLAtoGIntBig().  OFTReal fields
 * will be cast to integer.   Other field types, or errors will result in
 * a return value of zero.
 *
 * This method is the same as the C function OGR_F_GetFieldAsInteger64().
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 *
 * @return the field value.
 * @since GDAL 2.0
 */

GIntBig OGRFeature::GetFieldAsInteger64( int iField ) const

{
    const int iSpecialField = iField - poDefn->GetFieldCount();
    if( iSpecialField >= 0 )
    {
        // Special field value accessors.
        switch( iSpecialField )
        {
        case SPF_FID:
            return nFID;

        case SPF_OGR_GEOM_AREA:
            if( GetGeomFieldCount() == 0 || papoGeometries[0] == nullptr )
                return 0;
            return static_cast<int>(
                OGR_G_Area(OGRGeometry::ToHandle(papoGeometries[0])));

        default:
            return 0;
        }
    }

    OGRFieldDefn *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == nullptr )
        return 0;

    if( !IsFieldSetAndNotNullUnsafe(iField) )
        return 0;

    OGRFieldType eType = poFDefn->GetType();
    if( eType == OFTInteger )
    {
        return static_cast<GIntBig>(pauFields[iField].Integer);
    }
    else if( eType == OFTInteger64 )
    {
        return pauFields[iField].Integer64;
    }
    else if( eType == OFTReal )
    {
        return static_cast<GIntBig>(pauFields[iField].Real);
    }
    else if( eType == OFTString )
    {
        if( pauFields[iField].String == nullptr )
            return 0;
        else
        {
            return CPLAtoGIntBigEx(pauFields[iField].String, TRUE, nullptr);
        }
    }

    return 0;
}

/************************************************************************/
/*                      OGR_F_GetFieldAsInteger64()                     */
/************************************************************************/

/**
 * \brief Fetch field value as integer 64 bit.
 *
 * OFTInteger are promoted to 64 bit.
 * OFTString features will be translated using CPLAtoGIntBig().  OFTReal fields
 * will be cast to integer.   Other field types, or errors will result in
 * a return value of zero.
 *
 * This function is the same as the C++ method
 * OGRFeature::GetFieldAsInteger64().
 *
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 *
 * @return the field value.
 * @since GDAL 2.0
 */

GIntBig OGR_F_GetFieldAsInteger64( OGRFeatureH hFeat, int iField )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetFieldAsInteger64", 0 );

    return OGRFeature::FromHandle(hFeat)->GetFieldAsInteger64(iField);
}

/************************************************************************/
/*                          GetFieldAsDouble()                          */
/************************************************************************/

/**
 * \fn OGRFeature::GetFieldAsDouble( const char* pszFName ) const
 * \brief Fetch field value as a double.
 *
 * OFTString features will be translated using CPLAtof().  OFTInteger and
 * OFTInteger64 fields will be cast to double.  Other field types, or errors
 * will result in a return value of zero.
 *
 * @param pszFName the name of the field to fetch.
 *
 * @return the field value.
 */

/**
 * \brief Fetch field value as a double.
 *
 * OFTString features will be translated using CPLAtof().  OFTInteger and
 * OFTInteger64 fields will be cast to double.  Other field types, or errors
 * will result in a return value of zero.
 *
 * This method is the same as the C function OGR_F_GetFieldAsDouble().
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 *
 * @return the field value.
 */

double OGRFeature::GetFieldAsDouble( int iField ) const

{
    const int iSpecialField = iField - poDefn->GetFieldCount();
    if( iSpecialField >= 0 )
    {
        // Special field value accessors.
        switch( iSpecialField )
        {
        case SPF_FID:
            return static_cast<double>(GetFID());

        case SPF_OGR_GEOM_AREA:
            if( GetGeomFieldCount() == 0 || papoGeometries[0] == nullptr )
                return 0.0;
            return
                OGR_G_Area(OGRGeometry::ToHandle(papoGeometries[0]));

        default:
            return 0.0;
        }
    }

    OGRFieldDefn *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == nullptr )
        return 0.0;

    if( !IsFieldSetAndNotNullUnsafe(iField) )
        return 0.0;

    const OGRFieldType eType = poFDefn->GetType();
    if( eType == OFTReal )
    {
        return pauFields[iField].Real;
    }
    else if( eType == OFTInteger )
    {
        return pauFields[iField].Integer;
    }
    else if( eType == OFTInteger64 )
    {
        return static_cast<double>(pauFields[iField].Integer64);
    }
    else if( eType == OFTString )
    {
        if( pauFields[iField].String == nullptr )
            return 0;
        else
            return CPLAtof(pauFields[iField].String);
    }

    return 0.0;
}

/************************************************************************/
/*                       OGR_F_GetFieldAsDouble()                       */
/************************************************************************/

/**
 * \brief Fetch field value as a double.
 *
 * OFTString features will be translated using CPLAtof().  OFTInteger fields
 * will be cast to double.   Other field types, or errors will result in
 * a return value of zero.
 *
 * This function is the same as the C++ method OGRFeature::GetFieldAsDouble().
 *
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 *
 * @return the field value.
 */

double OGR_F_GetFieldAsDouble( OGRFeatureH hFeat, int iField )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetFieldAsDouble", 0 );

    return OGRFeature::FromHandle(hFeat)->GetFieldAsDouble(iField);
}

/************************************************************************/
/*                      OGRFeatureFormatDateTimeBuffer()                */
/************************************************************************/

static void OGRFeatureFormatDateTimeBuffer( char* szTempBuffer,
                                            size_t nMaxSize,
                                            int nYear, int nMonth, int nDay,
                                            int nHour, int nMinute,
                                            float fSecond,
                                            int nTZFlag )
{
    const int ms = OGR_GET_MS(fSecond);
    if( ms != 0 )
        CPLsnprintf( szTempBuffer, nMaxSize,
                  "%04d/%02d/%02d %02d:%02d:%06.3f",
                  nYear,
                  nMonth,
                  nDay,
                  nHour,
                  nMinute,
                  fSecond );
    else  // Default format.
    {
        if( CPLIsNan(fSecond) || fSecond < 0.0  || fSecond > 62.0 )
        {
            fSecond = 0.0;
            CPLError(CE_Failure, CPLE_NotSupported,
                     "OGRFeatureFormatDateTimeBuffer: fSecond is invalid.  "
                     "Forcing '%f' to 0.0.", fSecond);
        }
        snprintf( szTempBuffer, nMaxSize,
                  "%04d/%02d/%02d %02d:%02d:%02d",
                  nYear,
                  nMonth,
                  nDay,
                  nHour,
                  nMinute,
                  static_cast<int>(fSecond) );
    }

    if( nTZFlag > 1 )
    {
        char chSign;
        const int nOffset = (nTZFlag - 100) * 15;
        int nHours = static_cast<int>(nOffset / 60);  // Round towards zero.
        const int nMinutes = std::abs(nOffset - nHours * 60);

        if( nOffset < 0 )
        {
            chSign = '-';
            nHours = std::abs(nHours);
        }
        else
        {
            chSign = '+';
        }

        if( nMinutes == 0 )
            snprintf( szTempBuffer+strlen(szTempBuffer),
                      nMaxSize-strlen(szTempBuffer), "%c%02d", chSign, nHours );
        else
            snprintf( szTempBuffer+strlen(szTempBuffer),
                      nMaxSize-strlen(szTempBuffer),
                      "%c%02d%02d", chSign, nHours, nMinutes );
    }
}

/************************************************************************/
/*                          GetFieldAsString()                          */
/************************************************************************/

/**
 * \fn OGRFeature::GetFieldAsString( const char* pszFName ) const
 * \brief Fetch field value as a string.
 *
 * OFTReal and OFTInteger fields will be translated to string using
 * sprintf(), but not necessarily using the established formatting rules.
 * Other field types, or errors will result in a return value of zero.
 *
 * @param pszFName the name of the field to fetch.
 *
 * @return the field value.  This string is internal, and should not be
 * modified, or freed.  Its lifetime may be very brief.
 */

/**
 * \brief Fetch field value as a string.
 *
 * OFTReal and OFTInteger fields will be translated to string using
 * sprintf(), but not necessarily using the established formatting rules.
 * Other field types, or errors will result in a return value of zero.
 *
 * This method is the same as the C function OGR_F_GetFieldAsString().
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 *
 * @return the field value.  This string is internal, and should not be
 * modified, or freed.  Its lifetime may be very brief.
 */

const char *OGRFeature::GetFieldAsString( int iField ) const

{
    CPLFree(m_pszTmpFieldValue);
    m_pszTmpFieldValue = nullptr;

    const int iSpecialField = iField - poDefn->GetFieldCount();
    if( iSpecialField >= 0 )
    {
        // Special field value accessors.
        switch( iSpecialField )
        {
          case SPF_FID:
          {
            constexpr size_t MAX_SIZE = 20 + 1;
            m_pszTmpFieldValue = static_cast<char*>(CPLMalloc( MAX_SIZE ));
            CPLsnprintf( m_pszTmpFieldValue, MAX_SIZE, CPL_FRMT_GIB, GetFID() );
            return m_pszTmpFieldValue;
          }

          case SPF_OGR_GEOMETRY:
            if( GetGeomFieldCount() > 0 && papoGeometries[0] != nullptr )
                return papoGeometries[0]->getGeometryName();
            else
                return "";

          case SPF_OGR_STYLE:
            if( GetStyleString() == nullptr )
                return "";
            else
                return GetStyleString();

          case SPF_OGR_GEOM_WKT:
          {
              if( GetGeomFieldCount() == 0 || papoGeometries[0] == nullptr )
                  return "";

              if( papoGeometries[0]->exportToWkt( &m_pszTmpFieldValue ) ==
                  OGRERR_NONE )
                  return m_pszTmpFieldValue;
              else
                  return "";
          }

          case SPF_OGR_GEOM_AREA:
          {
            if( GetGeomFieldCount() == 0 || papoGeometries[0] == nullptr )
                return "";

            constexpr size_t MAX_SIZE = 20 + 1;
            m_pszTmpFieldValue = static_cast<char*>(CPLMalloc( MAX_SIZE ));
            CPLsnprintf(
                m_pszTmpFieldValue, MAX_SIZE, "%.16g",
                OGR_G_Area(OGRGeometry::ToHandle(papoGeometries[0])));
            return m_pszTmpFieldValue;
          }

          default:
            return "";
        }
    }

    OGRFieldDefn *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == nullptr )
        return "";

    if( !IsFieldSetAndNotNullUnsafe(iField) )
        return "";

    OGRFieldType eType = poFDefn->GetType();
    if( eType == OFTString )
    {
        if( pauFields[iField].String == nullptr )
            return "";
        else
            return pauFields[iField].String;
    }
    else if( eType == OFTInteger )
    {
        constexpr size_t MAX_SIZE = 11 + 1;
        m_pszTmpFieldValue = static_cast<char*>(CPLMalloc( MAX_SIZE ));
        snprintf( m_pszTmpFieldValue, MAX_SIZE,
                  "%d", pauFields[iField].Integer );
        return m_pszTmpFieldValue;
    }
    else if( eType == OFTInteger64 )
    {
        constexpr size_t MAX_SIZE = 20 + 1;
        m_pszTmpFieldValue = static_cast<char*>(CPLMalloc( MAX_SIZE ));
        CPLsnprintf( m_pszTmpFieldValue, MAX_SIZE,
                  CPL_FRMT_GIB, pauFields[iField].Integer64 );
        return m_pszTmpFieldValue;
    }
    else if( eType == OFTReal )
    {
        char szFormat[32] = {};
        constexpr int TEMP_BUFFER_SIZE = 80;
        char szTempBuffer[TEMP_BUFFER_SIZE] = {};

        if( poFDefn->GetWidth() != 0 )
        {
            snprintf( szFormat, sizeof(szFormat), "%%.%df",
                poFDefn->GetPrecision() );

            CPLsnprintf( szTempBuffer, TEMP_BUFFER_SIZE,
                         szFormat, pauFields[iField].Real );
        }
        else
        {
            if( poFDefn->GetSubType() == OFSTFloat32 )
            {
                OGRFormatFloat(szTempBuffer, TEMP_BUFFER_SIZE,
                               static_cast<float>(pauFields[iField].Real),
                               -1, 'g');
            }
            else
            {
                strcpy( szFormat, "%.15g" );

                CPLsnprintf( szTempBuffer, TEMP_BUFFER_SIZE,
                        szFormat, pauFields[iField].Real );
            }
        }

        m_pszTmpFieldValue = VSI_STRDUP_VERBOSE( szTempBuffer );
        if( m_pszTmpFieldValue == nullptr )
            return "";
        return m_pszTmpFieldValue;
    }
    else if( eType == OFTDateTime )
    {
        // "YYYY/MM/DD HH:MM:SS.sss+ZZ"
        constexpr size_t EXTRA_SPACE_FOR_NEGATIVE_OR_LARGE_YEARS = 5;
        constexpr size_t MAX_SIZE = 26 + EXTRA_SPACE_FOR_NEGATIVE_OR_LARGE_YEARS + 1;
        m_pszTmpFieldValue = static_cast<char*>(CPLMalloc( MAX_SIZE ));
        OGRFeatureFormatDateTimeBuffer(m_pszTmpFieldValue,
                                       MAX_SIZE,
                                       pauFields[iField].Date.Year,
                                       pauFields[iField].Date.Month,
                                       pauFields[iField].Date.Day,
                                       pauFields[iField].Date.Hour,
                                       pauFields[iField].Date.Minute,
                                       pauFields[iField].Date.Second,
                                       pauFields[iField].Date.TZFlag );

        return m_pszTmpFieldValue;
    }
    else if( eType == OFTDate )
    {
        constexpr size_t EXTRA_SPACE_FOR_NEGATIVE_OR_LARGE_YEARS = 5;
        constexpr size_t MAX_SIZE = 10 + EXTRA_SPACE_FOR_NEGATIVE_OR_LARGE_YEARS + 1;
        m_pszTmpFieldValue = static_cast<char*>(CPLMalloc( MAX_SIZE ));
        snprintf( m_pszTmpFieldValue, MAX_SIZE, "%04d/%02d/%02d",
                  pauFields[iField].Date.Year,
                  pauFields[iField].Date.Month,
                  pauFields[iField].Date.Day );
        return m_pszTmpFieldValue;
    }
    else if( eType == OFTTime )
    {
        constexpr size_t EXTRA_SPACE_TO_MAKE_GCC_HAPPY = 2;
        constexpr size_t MAX_SIZE = 12 + EXTRA_SPACE_TO_MAKE_GCC_HAPPY + 1;
        m_pszTmpFieldValue = static_cast<char*>(CPLMalloc( MAX_SIZE ));
        const int ms = OGR_GET_MS(pauFields[iField].Date.Second);
        if( ms != 0 || CPLIsNan(pauFields[iField].Date.Second) )
            snprintf(
                m_pszTmpFieldValue, MAX_SIZE, "%02d:%02d:%06.3f",
                pauFields[iField].Date.Hour,
                pauFields[iField].Date.Minute,
                pauFields[iField].Date.Second );
        else
            snprintf(
                m_pszTmpFieldValue, MAX_SIZE, "%02d:%02d:%02d",
                pauFields[iField].Date.Hour,
                pauFields[iField].Date.Minute,
                static_cast<int>(pauFields[iField].Date.Second) );

        return m_pszTmpFieldValue;
    }
    else if( eType == OFTIntegerList )
    {
        char szItem[32] = {};
        const int nCount = pauFields[iField].IntegerList.nCount;
        CPLString osBuffer;

        osBuffer.Printf("(%d:", nCount );
        for(int i = 0; i < nCount; i++ )
        {
            snprintf( szItem, sizeof(szItem), "%d",
                      pauFields[iField].IntegerList.paList[i] );
            if( i > 0 )
                osBuffer += ',';
            osBuffer += szItem;
        }
        osBuffer += ')';

        m_pszTmpFieldValue = VSI_STRDUP_VERBOSE( osBuffer.c_str() );
        if( m_pszTmpFieldValue == nullptr )
            return "";
        return m_pszTmpFieldValue;
    }
    else if( eType == OFTInteger64List )
    {
        char szItem[32] = {};
        const int nCount = pauFields[iField].Integer64List.nCount;
        CPLString osBuffer;

        osBuffer.Printf("(%d:", nCount );
        for(int i = 0; i < nCount; i++ )
        {
            CPLsnprintf( szItem, sizeof(szItem), CPL_FRMT_GIB,
                      pauFields[iField].Integer64List.paList[i] );
            if( i > 0 )
                osBuffer += ',';
            osBuffer += szItem;
        }
        osBuffer += ')';

        m_pszTmpFieldValue = VSI_STRDUP_VERBOSE( osBuffer.c_str() );
        if( m_pszTmpFieldValue == nullptr )
            return "";
        return m_pszTmpFieldValue;
    }
    else if( eType == OFTRealList )
    {
        char szItem[40] = {};
        char szFormat[64] = {};
        const int nCount = pauFields[iField].RealList.nCount;
        const bool bIsFloat32 = poFDefn->GetSubType() == OFSTFloat32;
        const bool bIsZeroWidth = poFDefn->GetWidth() == 0;

        if( !bIsZeroWidth )
        {
            snprintf( szFormat, sizeof(szFormat), "%%%d.%df",
                      poFDefn->GetWidth(), poFDefn->GetPrecision() );
        }
        else
            strcpy( szFormat, "%.16g" );

        CPLString osBuffer;

        osBuffer.Printf("(%d:", nCount );

        for(int i = 0; i < nCount; i++ )
        {
            if( bIsFloat32 && bIsZeroWidth )
            {
                OGRFormatFloat(szItem, sizeof(szItem),
                               static_cast<float>(pauFields[iField].RealList.paList[i]),
                               -1, 'g');
            }
            else
            {
                CPLsnprintf( szItem, sizeof(szItem), szFormat,
                        pauFields[iField].RealList.paList[i] );
            }
            if( i > 0 )
                osBuffer += ',';
            osBuffer += szItem;
        }
        osBuffer += ')';

        m_pszTmpFieldValue = VSI_STRDUP_VERBOSE( osBuffer.c_str() );
        if( m_pszTmpFieldValue == nullptr )
            return "";
        return m_pszTmpFieldValue;
    }
    else if( eType == OFTStringList )
    {
        const int nCount = pauFields[iField].StringList.nCount;

        CPLString osBuffer;

        osBuffer.Printf("(%d:", nCount );
        for(int i = 0; i < nCount; i++ )
        {
            const char *pszItem = pauFields[iField].StringList.paList[i];
            if( i > 0 )
                osBuffer += ',';
            osBuffer += pszItem;
        }
        osBuffer += ')';

        m_pszTmpFieldValue = VSI_STRDUP_VERBOSE( osBuffer.c_str() );
        if( m_pszTmpFieldValue == nullptr )
            return "";
        return m_pszTmpFieldValue;
    }
    else if( eType == OFTBinary )
    {
        const int nCount = pauFields[iField].Binary.nCount;
        m_pszTmpFieldValue =
            CPLBinaryToHex( nCount, pauFields[iField].Binary.paData );
        if( m_pszTmpFieldValue == nullptr )
            return "";
        return m_pszTmpFieldValue;
    }

    return "";
}

/************************************************************************/
/*                       OGR_F_GetFieldAsString()                       */
/************************************************************************/

/**
 * \brief Fetch field value as a string.
 *
 * OFTReal and OFTInteger fields will be translated to string using
 * sprintf(), but not necessarily using the established formatting rules.
 * Other field types, or errors will result in a return value of zero.
 *
 * This function is the same as the C++ method OGRFeature::GetFieldAsString().
 *
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 *
 * @return the field value.  This string is internal, and should not be
 * modified, or freed.  Its lifetime may be very brief.
 */

const char *OGR_F_GetFieldAsString( OGRFeatureH hFeat, int iField )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetFieldAsString", nullptr );

    return OGRFeature::FromHandle(hFeat)->GetFieldAsString(iField);
}

/************************************************************************/
/*                       GetFieldAsIntegerList()                        */
/************************************************************************/

/**
 * \fn OGRFeature::GetFieldAsIntegerList( const char* pszFName, int *pnCount ) const
 * \brief Fetch field value as a list of integers.
 *
 * Currently this method only works for OFTIntegerList fields.

 * @param pszFName the name of the field to fetch.
 * @param pnCount an integer to put the list count (number of integers) into.
 *
 * @return the field value.  This list is internal, and should not be
 * modified, or freed.  Its lifetime may be very brief.  If *pnCount is zero
 * on return the returned pointer may be NULL or non-NULL.
 * OFTReal and OFTInteger fields will be translated to string using
 * sprintf(), but not necessarily using the established formatting rules.
 * Other field types, or errors will result in a return value of zero.
 */

/**
 * \brief Fetch field value as a list of integers.
 *
 * Currently this method only works for OFTIntegerList fields.
 *
 * This method is the same as the C function OGR_F_GetFieldAsIntegerList().
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param pnCount an integer to put the list count (number of integers) into.
 *
 * @return the field value.  This list is internal, and should not be
 * modified, or freed.  Its lifetime may be very brief.  If *pnCount is zero
 * on return the returned pointer may be NULL or non-NULL.
 */

const int *OGRFeature::GetFieldAsIntegerList( int iField, int *pnCount ) const

{
    OGRFieldDefn *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn != nullptr && IsFieldSetAndNotNullUnsafe(iField) &&
        poFDefn->GetType() == OFTIntegerList )
    {
        if( pnCount != nullptr )
            *pnCount = pauFields[iField].IntegerList.nCount;

        return pauFields[iField].IntegerList.paList;
    }

    if( pnCount != nullptr )
        *pnCount = 0;

    return nullptr;
}

/************************************************************************/
/*                    OGR_F_GetFieldAsIntegerList()                     */
/************************************************************************/

/**
 * \brief Fetch field value as a list of integers.
 *
 * Currently this function only works for OFTIntegerList fields.
 *
 * This function is the same as the C++ method
 * OGRFeature::GetFieldAsIntegerList().
 *
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param pnCount an integer to put the list count (number of integers) into.
 *
 * @return the field value.  This list is internal, and should not be
 * modified, or freed.  Its lifetime may be very brief.  If *pnCount is zero
 * on return the returned pointer may be NULL or non-NULL.
 */

const int *OGR_F_GetFieldAsIntegerList( OGRFeatureH hFeat, int iField,
                                        int *pnCount )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetFieldAsIntegerList", nullptr );

    return OGRFeature::FromHandle(hFeat)->
        GetFieldAsIntegerList(iField, pnCount);
}

/************************************************************************/
/*                      GetFieldAsInteger64List()                       */
/************************************************************************/
/**
 * \fn OGRFeature::GetFieldAsInteger64List( const char* pszFName, int *pnCount ) const
 * \brief Fetch field value as a list of 64 bit integers.
 *
 * Currently this method only works for OFTInteger64List fields.

 * @param pszFName the name of the field to fetch.
 * @param pnCount an integer to put the list count (number of integers) into.
 *
 * @return the field value.  This list is internal, and should not be
 * modified, or freed.  Its lifetime may be very brief.  If *pnCount is zero
 * on return the returned pointer may be NULL or non-NULL.
 * @since GDAL 2.0
 */

/**
 * \brief Fetch field value as a list of 64 bit integers.
 *
 * Currently this method only works for OFTInteger64List fields.
 *
 * This method is the same as the C function OGR_F_GetFieldAsInteger64List().
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param pnCount an integer to put the list count (number of integers) into.
 *
 * @return the field value.  This list is internal, and should not be
 * modified, or freed.  Its lifetime may be very brief.  If *pnCount is zero
 * on return the returned pointer may be NULL or non-NULL.
 * @since GDAL 2.0
 */

const GIntBig *OGRFeature::GetFieldAsInteger64List( int iField, int *pnCount ) const

{
    OGRFieldDefn *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn != nullptr && IsFieldSetAndNotNullUnsafe(iField) &&
        poFDefn->GetType() == OFTInteger64List )
    {
        if( pnCount != nullptr )
            *pnCount = pauFields[iField].Integer64List.nCount;

        return pauFields[iField].Integer64List.paList;
    }

    if( pnCount != nullptr )
        *pnCount = 0;

    return nullptr;
}

/************************************************************************/
/*                   OGR_F_GetFieldAsInteger64List()                    */
/************************************************************************/

/**
 * \brief Fetch field value as a list of 64 bit integers.
 *
 * Currently this function only works for OFTInteger64List fields.
 *
 * This function is the same as the C++ method
 * OGRFeature::GetFieldAsInteger64List().
 *
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param pnCount an integer to put the list count (number of integers) into.
 *
 * @return the field value.  This list is internal, and should not be
 * modified, or freed.  Its lifetime may be very brief.  If *pnCount is zero
 * on return the returned pointer may be NULL or non-NULL.
 * @since GDAL 2.0
 */

const GIntBig *OGR_F_GetFieldAsInteger64List( OGRFeatureH hFeat, int iField,
                                              int *pnCount )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetFieldAsInteger64List", nullptr );

    return OGRFeature::FromHandle(hFeat)->
        GetFieldAsInteger64List(iField, pnCount);
}

/************************************************************************/
/*                        GetFieldAsDoubleList()                        */
/************************************************************************/
/**
 * \fn OGRFeature::GetFieldAsDoubleList( const char* pszFName, int *pnCount ) const
 * \brief Fetch field value as a list of doubles.
 *
 * Currently this method only works for OFTRealList fields.

 * @param pszFName the name of the field to fetch.
 * @param pnCount an integer to put the list count (number of doubles) into.
 *
 * @return the field value.  This list is internal, and should not be
 * modified, or freed.  Its lifetime may be very brief.  If *pnCount is zero
 * on return the returned pointer may be NULL or non-NULL.
 */

/**
 * \brief Fetch field value as a list of doubles.
 *
 * Currently this method only works for OFTRealList fields.
 *
 * This method is the same as the C function OGR_F_GetFieldAsDoubleList().
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param pnCount an integer to put the list count (number of doubles) into.
 *
 * @return the field value.  This list is internal, and should not be
 * modified, or freed.  Its lifetime may be very brief.  If *pnCount is zero
 * on return the returned pointer may be NULL or non-NULL.
 */

const double *OGRFeature::GetFieldAsDoubleList( int iField, int *pnCount ) const

{
    OGRFieldDefn *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn != nullptr && IsFieldSetAndNotNullUnsafe(iField) &&
        poFDefn->GetType() == OFTRealList )
    {
        if( pnCount != nullptr )
            *pnCount = pauFields[iField].RealList.nCount;

        return pauFields[iField].RealList.paList;
    }

    if( pnCount != nullptr )
        *pnCount = 0;

    return nullptr;
}

/************************************************************************/
/*                     OGR_F_GetFieldAsDoubleList()                     */
/************************************************************************/

/**
 * \brief Fetch field value as a list of doubles.
 *
 * Currently this function only works for OFTRealList fields.
 *
 * This function is the same as the C++ method
 * OGRFeature::GetFieldAsDoubleList().
 *
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param pnCount an integer to put the list count (number of doubles) into.
 *
 * @return the field value.  This list is internal, and should not be
 * modified, or freed.  Its lifetime may be very brief.  If *pnCount is zero
 * on return the returned pointer may be NULL or non-NULL.
 */

const double *OGR_F_GetFieldAsDoubleList( OGRFeatureH hFeat, int iField,
                                          int *pnCount )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetFieldAsDoubleList", nullptr );

    return OGRFeature::FromHandle(hFeat)->
        GetFieldAsDoubleList(iField, pnCount);
}

/************************************************************************/
/*                        GetFieldAsStringList()                        */
/************************************************************************/
/**
 * \fn OGRFeature::GetFieldAsStringList( const char* pszFName ) const
 * \brief Fetch field value as a list of strings.
 *
 * Currently this method only works for OFTStringList fields.
 *
 * The returned list is terminated by a NULL pointer. The number of
 * elements can also be calculated using CSLCount().
 *
 * @param pszFName the name of the field to fetch.
 *
 * @return the field value.  This list is internal, and should not be
 * modified, or freed.  Its lifetime may be very brief.
 */

/**
 * \brief Fetch field value as a list of strings.
 *
 * Currently this method only works for OFTStringList fields.
 *
 * The returned list is terminated by a NULL pointer. The number of
 * elements can also be calculated using CSLCount().
 *
 * This method is the same as the C function OGR_F_GetFieldAsStringList().
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 *
 * @return the field value.  This list is internal, and should not be
 * modified, or freed.  Its lifetime may be very brief.
 */

char **OGRFeature::GetFieldAsStringList( int iField ) const

{
    OGRFieldDefn *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == nullptr )
        return nullptr;

    if( !IsFieldSetAndNotNullUnsafe(iField) )
        return nullptr;

    if( poFDefn->GetType() == OFTStringList )
    {
        return pauFields[iField].StringList.paList;
    }

    return nullptr;
}

/************************************************************************/
/*                     OGR_F_GetFieldAsStringList()                     */
/************************************************************************/

/**
 * \brief Fetch field value as a list of strings.
 *
 * Currently this method only works for OFTStringList fields.
 *
 * The returned list is terminated by a NULL pointer. The number of
 * elements can also be calculated using CSLCount().
 *
 * This function is the same as the C++ method
 * OGRFeature::GetFieldAsStringList().
 *
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 *
 * @return the field value.  This list is internal, and should not be
 * modified, or freed.  Its lifetime may be very brief.
 */

char **OGR_F_GetFieldAsStringList( OGRFeatureH hFeat, int iField )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetFieldAsStringList", nullptr );

    return OGRFeature::FromHandle(hFeat)->GetFieldAsStringList(iField);
}

/************************************************************************/
/*                          GetFieldAsBinary()                          */
/************************************************************************/

/**
 * \brief Fetch field value as binary data.
 *
 * This method only works for OFTBinary and OFTString fields.
 *
 * This method is the same as the C function OGR_F_GetFieldAsBinary().
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param pnBytes location to put the number of bytes returned.
 *
 * @return the field value.  This data is internal, and should not be
 * modified, or freed.  Its lifetime may be very brief.
 */

GByte *OGRFeature::GetFieldAsBinary( int iField, int *pnBytes ) const

{
    OGRFieldDefn *poFDefn = poDefn->GetFieldDefn( iField );

    *pnBytes = 0;

    if( poFDefn == nullptr )
        return nullptr;

    if( !IsFieldSetAndNotNullUnsafe(iField) )
        return nullptr;

    if( poFDefn->GetType() == OFTBinary )
    {
        *pnBytes = pauFields[iField].Binary.nCount;
        return pauFields[iField].Binary.paData;
    }
    else if( poFDefn->GetType() == OFTString )
    {
        *pnBytes = static_cast<int>(strlen(pauFields[iField].String));
        return reinterpret_cast<GByte *>(pauFields[iField].String);
    }

    return nullptr;
}

/************************************************************************/
/*                       OGR_F_GetFieldAsBinary()                       */
/************************************************************************/

/**
 * \brief Fetch field value as binary.
 *
 * This method only works for OFTBinary and OFTString fields.
 *
 * This function is the same as the C++ method
 * OGRFeature::GetFieldAsBinary().
 *
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param pnBytes location to place count of bytes returned.
 *
 * @return the field value.  This list is internal, and should not be
 * modified, or freed.  Its lifetime may be very brief.
 */

GByte *OGR_F_GetFieldAsBinary( OGRFeatureH hFeat, int iField, int *pnBytes )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetFieldAsBinary", nullptr );
    VALIDATE_POINTER1( pnBytes, "OGR_F_GetFieldAsBinary", nullptr );

    return OGRFeature::FromHandle(hFeat)->
        GetFieldAsBinary(iField, pnBytes);
}

/************************************************************************/
/*                         GetFieldAsDateTime()                         */
/************************************************************************/

/**
 * \brief Fetch field value as date and time.
 *
 * Currently this method only works for OFTDate, OFTTime and OFTDateTime fields.
 *
 * This method is the same as the C function OGR_F_GetFieldAsDateTime().
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param pnYear (including century)
 * @param pnMonth (1-12)
 * @param pnDay (1-31)
 * @param pnHour (0-23)
 * @param pnMinute (0-59)
 * @param pfSecond (0-59 with millisecond accuracy)
 * @param pnTZFlag (0=unknown, 1=localtime, 100=GMT, see data model for details)
 *
 * @return TRUE on success or FALSE on failure.
 */

int OGRFeature::GetFieldAsDateTime( int iField,
                                    int *pnYear, int *pnMonth, int *pnDay,
                                    int *pnHour, int *pnMinute, float *pfSecond,
                                    int *pnTZFlag ) const

{
    OGRFieldDefn *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == nullptr )
        return FALSE;

    if( !IsFieldSetAndNotNullUnsafe(iField) )
        return FALSE;

    if( poFDefn->GetType() == OFTDate
        || poFDefn->GetType() == OFTTime
        || poFDefn->GetType() == OFTDateTime )
    {
        if( pnYear )
            *pnYear = pauFields[iField].Date.Year;
        if( pnMonth )
            *pnMonth = pauFields[iField].Date.Month;
        if( pnDay )
            *pnDay = pauFields[iField].Date.Day;
        if( pnHour )
            *pnHour = pauFields[iField].Date.Hour;
        if( pnMinute )
            *pnMinute = pauFields[iField].Date.Minute;
        if( pfSecond )
            *pfSecond = pauFields[iField].Date.Second;
        if( pnTZFlag )
            *pnTZFlag = pauFields[iField].Date.TZFlag;

        return TRUE;
    }

    return FALSE;
}

/**
 * \brief Fetch field value as date and time.
 *
 * Currently this method only works for OFTDate, OFTTime and OFTDateTime fields.
 *
 * This method is the same as the C function OGR_F_GetFieldAsDateTime().
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param pnYear (including century)
 * @param pnMonth (1-12)
 * @param pnDay (1-31)
 * @param pnHour (0-23)
 * @param pnMinute (0-59)
 * @param pnSecond (0-59)
 * @param pnTZFlag (0=unknown, 1=localtime, 100=GMT, see data model for details)
 *
 * @return TRUE on success or FALSE on failure.
 */

int OGRFeature::GetFieldAsDateTime( int iField,
                                    int *pnYear, int *pnMonth, int *pnDay,
                                    int *pnHour, int *pnMinute, int *pnSecond,
                                    int *pnTZFlag ) const
{
    float fSecond = 0.0f;
    const bool bRet = CPL_TO_BOOL(
        GetFieldAsDateTime( iField, pnYear, pnMonth, pnDay, pnHour, pnMinute,
                            &fSecond, pnTZFlag));
    if( bRet && pnSecond ) *pnSecond = static_cast<int>(fSecond);
    return bRet;
}

/************************************************************************/
/*                      OGR_F_GetFieldAsDateTime()                      */
/************************************************************************/

/**
 * \brief Fetch field value as date and time.
 *
 * Currently this method only works for OFTDate, OFTTime and OFTDateTime fields.
 *
 * This function is the same as the C++ method
 * OGRFeature::GetFieldAsDateTime().
 *
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param pnYear (including century)
 * @param pnMonth (1-12)
 * @param pnDay (1-31)
 * @param pnHour (0-23)
 * @param pnMinute (0-59)
 * @param pnSecond (0-59)
 * @param pnTZFlag (0=unknown, 1=localtime, 100=GMT, see data model for details)
 *
 * @return TRUE on success or FALSE on failure.
 *
 * @see Use OGR_F_GetFieldAsDateTimeEx() for second with millisecond accuracy.
 */

int OGR_F_GetFieldAsDateTime( OGRFeatureH hFeat, int iField,
                              int *pnYear, int *pnMonth, int *pnDay,
                              int *pnHour, int *pnMinute, int *pnSecond,
                              int *pnTZFlag )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetFieldAsDateTime", 0 );

    float fSecond = 0.0f;
    const bool bRet = CPL_TO_BOOL(OGRFeature::FromHandle(hFeat)->
        GetFieldAsDateTime( iField,
                            pnYear, pnMonth, pnDay,
                            pnHour, pnMinute, &fSecond,
                            pnTZFlag ));
    if( bRet && pnSecond ) *pnSecond = static_cast<int>(fSecond);
    return bRet;
}

/************************************************************************/
/*                     OGR_F_GetFieldAsDateTimeEx()                     */
/************************************************************************/

/**
 * \brief Fetch field value as date and time.
 *
 * Currently this method only works for OFTDate, OFTTime and OFTDateTime fields.
 *
 * This function is the same as the C++ method
 * OGRFeature::GetFieldAsDateTime().
 *
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param pnYear (including century)
 * @param pnMonth (1-12)
 * @param pnDay (1-31)
 * @param pnHour (0-23)
 * @param pnMinute (0-59)
 * @param pfSecond (0-59 with millisecond accuracy)
 * @param pnTZFlag (0=unknown, 1=localtime, 100=GMT, see data model for details)
 *
 * @return TRUE on success or FALSE on failure.
 * @since GDAL 2.0
 */

int OGR_F_GetFieldAsDateTimeEx( OGRFeatureH hFeat, int iField,
                                int *pnYear, int *pnMonth, int *pnDay,
                                int *pnHour, int *pnMinute, float *pfSecond,
                                int *pnTZFlag )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetFieldAsDateTimeEx", 0 );

    return
        OGRFeature::FromHandle(hFeat)->
            GetFieldAsDateTime(iField,
                               pnYear, pnMonth, pnDay,
                               pnHour, pnMinute, pfSecond,
                               pnTZFlag);
}

/************************************************************************/
/*                        OGRFeatureGetIntegerValue()                   */
/************************************************************************/

static int OGRFeatureGetIntegerValue( OGRFieldDefn *poFDefn, int nValue )
{
    if( poFDefn->GetSubType() == OFSTBoolean && nValue != 0 && nValue != 1 )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Only 0 or 1 should be passed for a OFSTBoolean subtype. "
                 "Considering this non-zero value as 1.");
        nValue = 1;
    }
    else if( poFDefn->GetSubType() == OFSTInt16 )
    {
        if( nValue < -32768 )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Out-of-range value for a OFSTInt16 subtype. "
                     "Considering this value as -32768.");
            nValue = -32768;
        }
        else if( nValue > 32767 )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Out-of-range value for a OFSTInt16 subtype. "
                     "Considering this value as 32767.");
            nValue = 32767;
        }
    }
    return nValue;
}

/************************************************************************/
/*                        GetFieldAsSerializedJSon()                   */
/************************************************************************/

/**
 * \brief Fetch field value as a serialized JSon object.
 *
 * Currently this method only works for OFTStringList, OFTIntegerList,
 * OFTInteger64List and OFTRealList
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 *
 * @return a string that must be de-allocate with CPLFree()
 * @since GDAL 2.2
 */
char* OGRFeature::GetFieldAsSerializedJSon( int iField ) const

{
    const int iSpecialField = iField - poDefn->GetFieldCount();
    if( iSpecialField >= 0 )
    {
        return nullptr;
    }

    OGRFieldDefn *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == nullptr )
        return nullptr;

    if( !IsFieldSetAndNotNullUnsafe(iField) )
        return nullptr;

    char* pszRet = nullptr;
    OGRFieldType eType = poFDefn->GetType();
    if( eType == OFTStringList )
    {
        char** papszValues = GetFieldAsStringList(iField);
        if( papszValues == nullptr )
        {
            pszRet = CPLStrdup("[]");
        }
        else
        {
            json_object* poObj = json_object_new_array();
            for( int i=0; papszValues[i] != nullptr; i++)
            {
                json_object_array_add( poObj,
                                       json_object_new_string(papszValues[i]) );
            }
            pszRet = CPLStrdup( json_object_to_json_string(poObj) );
            json_object_put(poObj);
        }
    }
    else if( eType == OFTIntegerList )
    {
        json_object* poObj = json_object_new_array();
        int nCount = 0;
        const int* panValues = GetFieldAsIntegerList(iField, &nCount);
        for( int i = 0; i < nCount; i++ )
        {
            json_object_array_add( poObj,
                                   json_object_new_int(panValues[i]) );
        }
        pszRet = CPLStrdup( json_object_to_json_string(poObj) );
        json_object_put(poObj);
    }
    else if( eType == OFTInteger64List )
    {
        json_object* poObj = json_object_new_array();
        int nCount = 0;
        const GIntBig* panValues = GetFieldAsInteger64List(iField, &nCount);
        for( int i = 0; i < nCount; i++ )
        {
            json_object_array_add( poObj,
                                   json_object_new_int64(panValues[i]) );
        }
        pszRet = CPLStrdup( json_object_to_json_string(poObj) );
        json_object_put(poObj);
    }
    else if( eType == OFTRealList )
    {
        json_object* poObj = json_object_new_array();
        int nCount = 0;
        const double* padfValues = GetFieldAsDoubleList(iField, &nCount);
        for( int i = 0; i < nCount; i++ )
        {
            json_object_array_add( poObj,
                            json_object_new_double(padfValues[i]) );
        }
        pszRet = CPLStrdup( json_object_to_json_string(poObj) );
        json_object_put(poObj);
    }

    return pszRet;
}

/************************************************************************/
/*                              SetField()                              */
/************************************************************************/

/**
 * \fn OGRFeature::SetField( const char* pszFName, int nValue )
 * \brief Set field to integer value.
 * OFTInteger, OFTInteger64 and OFTReal fields will be set directly.  OFTString
 * fields will be assigned a string representation of the value, but not
 * necessarily taking into account formatting constraints on this field.  Other
 * field types may be unaffected.
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param pszFName the name of the field to set.
 * @param nValue the value to assign.
 */

/**
 * \brief Set field to integer value.
 *
 * OFTInteger, OFTInteger64 and OFTReal fields will be set directly.  OFTString
 * fields will be assigned a string representation of the value, but not
 * necessarily taking into account formatting constraints on this field.  Other
 * field types may be unaffected.
 *
 * This method is the same as the C function OGR_F_SetFieldInteger().
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param nValue the value to assign.
 */

void OGRFeature::SetField( int iField, int nValue )

{
    OGRFieldDefn *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == nullptr )
        return;

    OGRFieldType eType = poFDefn->GetType();
    if( eType == OFTInteger )
    {
        pauFields[iField].Integer = OGRFeatureGetIntegerValue(poFDefn, nValue);
        pauFields[iField].Set.nMarker2 = 0;
        pauFields[iField].Set.nMarker3 = 0;
    }
    else if( eType == OFTInteger64 )
    {
        pauFields[iField].Integer64 =
            OGRFeatureGetIntegerValue(poFDefn, nValue);
    }
    else if( eType == OFTReal )
    {
        pauFields[iField].Real = nValue;
    }
    else if( eType == OFTIntegerList )
    {
        SetField( iField, 1, &nValue );
    }
    else if( eType == OFTInteger64List )
    {
        GIntBig nVal64 = nValue;
        SetField( iField, 1, &nVal64 );
    }
    else if( eType == OFTRealList )
    {
        double dfValue = nValue;
        SetField( iField, 1, &dfValue );
    }
    else if( eType == OFTString )
    {
        char szTempBuffer[64] = {};

        snprintf( szTempBuffer, sizeof(szTempBuffer), "%d", nValue );

        if( IsFieldSetAndNotNullUnsafe(iField) )
            CPLFree( pauFields[iField].String );

        pauFields[iField].String = VSI_STRDUP_VERBOSE( szTempBuffer );
        if( pauFields[iField].String == nullptr )
        {
            OGR_RawField_SetUnset(&pauFields[iField]);
        }
    }
    else if( eType == OFTStringList )
    {
        char szTempBuffer[64] = {};

        snprintf( szTempBuffer, sizeof(szTempBuffer), "%d", nValue );
        char *apszValues[2] = { szTempBuffer, nullptr };
        SetField( iField, apszValues);
    }
    else
    {
        // Do nothing for other field types.
    }
}

/************************************************************************/
/*                       OGR_F_SetFieldInteger()                        */
/************************************************************************/

/**
 * \brief Set field to integer value.
 *
 * OFTInteger, OFTInteger64 and OFTReal fields will be set directly.  OFTString
 * fields will be assigned a string representation of the value, but not
 * necessarily taking into account formatting constraints on this field.  Other
 * field types may be unaffected.
 *
 * This function is the same as the C++ method OGRFeature::SetField().
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param nValue the value to assign.
 */

void OGR_F_SetFieldInteger( OGRFeatureH hFeat, int iField, int nValue )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetFieldInteger" );

    OGRFeature::FromHandle(hFeat)->SetField( iField, nValue );
}

/************************************************************************/
/*                              SetField()                              */
/************************************************************************/

/**
 * \fn OGRFeature::SetField( const char* pszFName, GIntBig nValue )
 * \brief Set field to 64 bit integer value.
 * OFTInteger, OFTInteger64 and OFTReal fields will be set directly.  OFTString
 * fields will be assigned a string representation of the value, but not
 * necessarily taking into account formatting constraints on this field.  Other
 * field types may be unaffected.
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param pszFName the name of the field to set.
 * @param nValue the value to assign.
 */

/**
 * \brief Set field to 64 bit integer value.
 *
 * OFTInteger, OFTInteger64 and OFTReal fields will be set directly.  OFTString
 * fields will be assigned a string representation of the value, but not
 * necessarily taking into account formatting constraints on this field.  Other
 * field types may be unaffected.
 *
 * This method is the same as the C function OGR_F_SetFieldInteger64().
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param nValue the value to assign.
 * @since GDAL 2.0
 */

void OGRFeature::SetField( int iField, GIntBig nValue )

{
    OGRFieldDefn *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == nullptr )
        return;

    OGRFieldType eType = poFDefn->GetType();
    if( eType == OFTInteger )
    {
        const int nVal32 =
            nValue < INT_MIN ? INT_MIN :
            nValue > INT_MAX ? INT_MAX : static_cast<int>(nValue);

        if( static_cast<GIntBig>(nVal32) != nValue )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Integer overflow occurred when trying to set "
                      "32bit field." );
        }
        SetField(iField, nVal32);
    }
    else if( eType == OFTInteger64 )
    {
        pauFields[iField].Integer64 = nValue;
    }
    else if( eType == OFTReal )
    {
        pauFields[iField].Real = static_cast<double>(nValue);
    }
    else if( eType == OFTIntegerList )
    {
        int nVal32 =
            nValue < INT_MIN ? INT_MIN :
            nValue > INT_MAX ? INT_MAX : static_cast<int>(nValue);

        if( static_cast<GIntBig>(nVal32) != nValue )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Integer overflow occurred when trying to set "
                      "32bit field." );
        }
        SetField( iField, 1, &nVal32 );
    }
    else if( eType == OFTInteger64List )
    {
        SetField( iField, 1, &nValue );
    }
    else if( eType == OFTRealList )
    {
        double dfValue = static_cast<double>(nValue);
        SetField( iField, 1, &dfValue );
    }
    else if( eType == OFTString )
    {
        char szTempBuffer[64] = {};

        CPLsnprintf( szTempBuffer, sizeof(szTempBuffer), CPL_FRMT_GIB, nValue );

        if( IsFieldSetAndNotNullUnsafe(iField) )
            CPLFree( pauFields[iField].String );

        pauFields[iField].String = VSI_STRDUP_VERBOSE( szTempBuffer );
        if( pauFields[iField].String == nullptr )
        {
            OGR_RawField_SetUnset(&pauFields[iField]);
        }
    }
    else if( eType == OFTStringList )
    {
        char szTempBuffer[64] = {};

        CPLsnprintf( szTempBuffer, sizeof(szTempBuffer), CPL_FRMT_GIB, nValue );
        char *apszValues[2] = { szTempBuffer, nullptr };
        SetField( iField, apszValues);
    }
    else
    {
        // Do nothing for other field types.
    }
}

/************************************************************************/
/*                      OGR_F_SetFieldInteger64()                       */
/************************************************************************/

/**
 * \brief Set field to 64 bit integer value.
 *
 * OFTInteger, OFTInteger64 and OFTReal fields will be set directly.  OFTString
 * fields will be assigned a string representation of the value, but not
 * necessarily taking into account formatting constraints on this field.  Other
 * field types may be unaffected.
 *
 * This function is the same as the C++ method OGRFeature::SetField().
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param nValue the value to assign.
 * @since GDAL 2.0
 */

void OGR_F_SetFieldInteger64( OGRFeatureH hFeat, int iField, GIntBig nValue )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetFieldInteger64" );

    OGRFeature::FromHandle(hFeat)->SetField( iField, nValue );
}

/************************************************************************/
/*                              SetField()                              */
/************************************************************************/

/**
 * \fn OGRFeature::SetField( const char* pszFName, double dfValue )
 * \brief Set field to double value.
 *
 * OFTInteger, OFTInteger64 and OFTReal fields will be set directly.  OFTString
 * fields will be assigned a string representation of the value, but not
 * necessarily taking into account formatting constraints on this field.  Other
 * field types may be unaffected.
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param pszFName the name of the field to set.
 * @param dfValue the value to assign.
 */

/**
 * \brief Set field to double value.
 *
 * OFTInteger, OFTInteger64 and OFTReal fields will be set directly.  OFTString
 * fields will be assigned a string representation of the value, but not
 * necessarily taking into account formatting constraints on this field.  Other
 * field types may be unaffected.
 *
 * This method is the same as the C function OGR_F_SetFieldDouble().
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param dfValue the value to assign.
 */

void OGRFeature::SetField( int iField, double dfValue )

{
    OGRFieldDefn *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == nullptr )
        return;

    const OGRFieldType eType = poFDefn->GetType();
    if( eType == OFTReal )
    {
        // if( poFDefn->GetSubType() == OFSTFloat32 &&
        //     dfValue != (double)(float)dfValue )
        // {
        //     CPLError(CE_Warning, CPLE_AppDefined,
        //              "Passed value cannot be exactly representing as "
        //              "a single-precision floating point value.");
        //     dfValue = (double)(float)dfValue;
        // }
        pauFields[iField].Real = dfValue;
    }
    else if( eType == OFTInteger )
    {
        const int nMin = std::numeric_limits<int>::min();
        const int nMax = std::numeric_limits<int>::max();
        const int nVal =
            dfValue < nMin ? nMin :
            dfValue > nMax ? nMax : static_cast<int>(dfValue);
        pauFields[iField].Integer = OGRFeatureGetIntegerValue(poFDefn, nVal);
        pauFields[iField].Set.nMarker2 = 0;
        pauFields[iField].Set.nMarker3 = 0;
    }
    else if( eType == OFTInteger64 )
    {
      pauFields[iField].Integer64 = static_cast<GIntBig>(dfValue);
      pauFields[iField].Set.nMarker3 = 0;
    }
    else if( eType == OFTRealList )
    {
        SetField( iField, 1, &dfValue );
    }
    else if( eType == OFTIntegerList )
    {
        int nValue = static_cast<int>(dfValue);
        SetField( iField, 1, &nValue );
    }
    else if( eType == OFTInteger64List )
    {
        GIntBig nValue = static_cast<GIntBig>(dfValue);
        SetField( iField, 1, &nValue );
    }
    else if( eType == OFTString )
    {
        char szTempBuffer[128] = {};

        CPLsnprintf( szTempBuffer, sizeof(szTempBuffer), "%.16g", dfValue );

        if( IsFieldSetAndNotNullUnsafe(iField) )
            CPLFree( pauFields[iField].String );

        pauFields[iField].String = VSI_STRDUP_VERBOSE( szTempBuffer );
        if( pauFields[iField].String == nullptr )
        {
            OGR_RawField_SetUnset(&pauFields[iField]);
        }
    }
    else if( eType == OFTStringList )
    {
        char szTempBuffer[64] = {};

        CPLsnprintf( szTempBuffer, sizeof(szTempBuffer), "%.16g", dfValue );
        char *apszValues[2] = { szTempBuffer, nullptr };
        SetField( iField, apszValues);
    }
    else
    {
        // Do nothing for other field types.
    }
}

/************************************************************************/
/*                        OGR_F_SetFieldDouble()                        */
/************************************************************************/

/**
 * \brief Set field to double value.
 *
 * OFTInteger, OFTInteger64 and OFTReal fields will be set directly.  OFTString
 * fields will be assigned a string representation of the value, but not
 * necessarily taking into account formatting constraints on this field.  Other
 * field types may be unaffected.
 *
 * This function is the same as the C++ method OGRFeature::SetField().
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param dfValue the value to assign.
 */

void OGR_F_SetFieldDouble( OGRFeatureH hFeat, int iField, double dfValue )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetFieldDouble" );

    OGRFeature::FromHandle(hFeat)->SetField( iField, dfValue );
}

/************************************************************************/
/*                              SetField()                              */
/************************************************************************/

/**
 * \fn OGRFeature::SetField( const char* pszFName, const char * pszValue )
 * \brief Set field to string value.
 *
 * OFTInteger fields will be set based on an atoi() conversion of the string.
 * OFTInteger64 fields will be set based on an CPLAtoGIntBig() conversion of the
 * string.  OFTReal fields will be set based on an CPLAtof() conversion of the
 * string.  Other field types may be unaffected.
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param pszFName the name of the field to set.
 * @param pszValue the value to assign.
 */

/**
 * \brief Set field to string value.
 *
 * OFTInteger fields will be set based on an atoi() conversion of the string.
 * OFTInteger64 fields will be set based on an CPLAtoGIntBig() conversion of the
 * string.  OFTReal fields will be set based on an CPLAtof() conversion of the
 * string.  Other field types may be unaffected.
 *
 * This method is the same as the C function OGR_F_SetFieldString().
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param pszValue the value to assign.
 */

void OGRFeature::SetField( int iField, const char * pszValue )

{
    static int bWarn = -1;
    if( bWarn < 0 )
        bWarn = CPLTestBool( CPLGetConfigOption( "OGR_SETFIELD_NUMERIC_WARNING",
                                                 "YES" ) );

    OGRFieldDefn *poFDefn = poDefn->GetFieldDefn( iField );
    if( poFDefn == nullptr )
        return;

    char *pszLast = nullptr;

    OGRFieldType eType = poFDefn->GetType();
    if( eType == OFTString )
    {
        if( IsFieldSetAndNotNullUnsafe(iField) )
            CPLFree( pauFields[iField].String );

        pauFields[iField].String = VSI_STRDUP_VERBOSE(pszValue ? pszValue : "");
        if( pauFields[iField].String == nullptr )
        {
            OGR_RawField_SetUnset(&pauFields[iField]);
        }
    }
    else if( eType == OFTInteger )
    {
        // As allowed by C standard, some systems like MSVC do not reset errno.
        errno = 0;

        long nVal = strtol(pszValue, &pszLast, 10);
        nVal = OGRFeatureGetIntegerValue(poFDefn, static_cast<int>(nVal));
        pauFields[iField].Integer =
            nVal > INT_MAX ? INT_MAX :
            nVal < INT_MIN ? INT_MIN : static_cast<int>(nVal);
        if( bWarn && (errno == ERANGE ||
                      nVal != static_cast<long>(pauFields[iField].Integer) ||
                      !pszLast || *pszLast ) )
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "Value '%s' of field %s.%s parsed incompletely to integer %d.",
                pszValue, poDefn->GetName(), poFDefn->GetNameRef(),
                pauFields[iField].Integer );
        pauFields[iField].Set.nMarker2 = 0;
        pauFields[iField].Set.nMarker3 = 0;
    }
    else if( eType == OFTInteger64 )
    {
        pauFields[iField].Integer64 = CPLAtoGIntBigEx(pszValue, bWarn, nullptr);
        pauFields[iField].Set.nMarker3 = 0;
    }
    else if( eType == OFTReal )
    {
        pauFields[iField].Real = CPLStrtod(pszValue, &pszLast);
        if( bWarn && ( !pszLast || *pszLast ) )
             CPLError(
                 CE_Warning, CPLE_AppDefined,
                 "Value '%s' of field %s.%s parsed incompletely to real %.16g.",
                 pszValue, poDefn->GetName(), poFDefn->GetNameRef(),
                 pauFields[iField].Real );
    }
    else if( eType == OFTDate
             || eType == OFTTime
             || eType == OFTDateTime )
    {
        OGRField sWrkField;

        if( OGRParseDate( pszValue, &sWrkField, 0 ) )
            memcpy( pauFields+iField, &sWrkField, sizeof(sWrkField));
    }
    else if( eType == OFTIntegerList
             || eType == OFTInteger64List
             || eType == OFTRealList )
    {
        json_object* poJSonObj = nullptr;
        if( pszValue[0] == '[' && pszValue[strlen(pszValue)-1] == ']' &&
            OGRJSonParse(pszValue, &poJSonObj, false) )
        {
            const auto nLength = json_object_array_length(poJSonObj);
            if( eType == OFTIntegerList && nLength > 0 )
            {
                std::vector<int> anValues;
                for( auto i = decltype(nLength){0}; i < nLength; i++ )
                {
                    json_object* poItem =
                        json_object_array_get_idx(poJSonObj, i);
                    anValues.push_back( json_object_get_int( poItem ) );
                }
                SetField( iField, static_cast<int>(nLength), &(anValues[0]) );
            }
            else if( eType == OFTInteger64List && nLength > 0 )
            {
                std::vector<GIntBig> anValues;
                for( auto i = decltype(nLength){0}; i < nLength; i++ )
                {
                    json_object* poItem =
                        json_object_array_get_idx(poJSonObj, i);
                    anValues.push_back( json_object_get_int64( poItem ) );
                }
                SetField( iField, static_cast<int>(nLength), &(anValues[0]) );
            }
            else if( eType == OFTRealList && nLength > 0 )
            {
                std::vector<double> adfValues;
                for( auto i = decltype(nLength){0}; i < nLength; i++ )
                {
                    json_object* poItem =
                        json_object_array_get_idx(poJSonObj, i);
                    adfValues.push_back( json_object_get_double( poItem ) );
                }
                SetField( iField, static_cast<int>(nLength), &(adfValues[0]) );
            }

            json_object_put(poJSonObj);
        }
        else
        {
            char **papszValueList = nullptr;

            if( pszValue[0] == '(' && strchr(pszValue, ':') != nullptr )
            {
                papszValueList = CSLTokenizeString2(
                    pszValue, ",:()", 0 );
            }

            if( papszValueList == nullptr || *papszValueList == nullptr
                || atoi(papszValueList[0]) != CSLCount(papszValueList)-1 )
            {
                // Do nothing - the count does not match entries.
            }
            else if( eType == OFTIntegerList )
            {
                const int nCount = atoi(papszValueList[0]);
                std::vector<int> anValues;
                if( nCount == CSLCount(papszValueList)-1 )
                {
                    for( int i = 0; i < nCount; i++ )
                    {
                        // As allowed by C standard, some systems like
                        // MSVC do not reset errno.
                        errno = 0;
                        int nVal = atoi(papszValueList[i+1]);
                        if( errno == ERANGE )
                        {
                            CPLError(
                                CE_Warning, CPLE_AppDefined,
                                "32 bit integer overflow when converting %s",
                                pszValue);
                        }
                        anValues.push_back( nVal );
                    }
                    if( nCount > 0 )
                        SetField( iField, nCount, &(anValues[0]) );
                }
            }
            else if( eType == OFTInteger64List )
            {
                const int nCount = atoi(papszValueList[0]);
                std::vector<GIntBig> anValues;
                if( nCount == CSLCount(papszValueList)-1 )
                {
                    for( int i = 0; i < nCount; i++ )
                    {
                        const GIntBig nVal =
                            CPLAtoGIntBigEx(papszValueList[i+1], TRUE, nullptr);
                        anValues.push_back( nVal );
                    }
                    if( nCount > 0 )
                        SetField( iField, nCount, &(anValues[0]) );
                }
            }
            else if( eType == OFTRealList )
            {
                int nCount = atoi(papszValueList[0]);
                std::vector<double> adfValues;
                if( nCount == CSLCount(papszValueList)-1 )
                {
                    for( int i = 0; i < nCount; i++ )
                        adfValues.push_back( CPLAtof(papszValueList[i+1]) );
                    if( nCount > 0 )
                        SetField( iField, nCount, &(adfValues[0]) );
                }
            }

            CSLDestroy(papszValueList);
        }
    }
    else if( eType == OFTStringList )
    {
        if( pszValue && *pszValue )
        {
            json_object* poJSonObj = nullptr;
            if( pszValue[0] == '(' && strchr(pszValue, ':') != nullptr &&
                pszValue[strlen(pszValue)-1] == ')' )
            {
                char** papszValueList =
                    CSLTokenizeString2(pszValue, ",:()", 0);
                const int nCount =
                    papszValueList[0] == nullptr ? 0 : atoi(papszValueList[0]);
                std::vector<char*> aosValues;
                if( nCount == CSLCount(papszValueList)-1 )
                {
                    for( int i = 0; i < nCount; i++ )
                        aosValues.push_back( papszValueList[i+1] );
                    aosValues.push_back( nullptr );
                    SetField( iField, &(aosValues[0]) );
                }
                CSLDestroy(papszValueList);
            }
            // Is this a JSon array?
            else if( pszValue[0] == '[' &&
                     pszValue[strlen(pszValue)-1] == ']' &&
                     OGRJSonParse(pszValue, &poJSonObj, false) )
            {
                CPLStringList aoList;
                const auto nLength = json_object_array_length(poJSonObj);
                for( auto i = decltype(nLength){0}; i < nLength; i++ )
                {
                    json_object* poItem =
                        json_object_array_get_idx(poJSonObj, i);
                    if( !poItem )
                        aoList.AddString("");
                    else
                        aoList.AddString( json_object_get_string(poItem) );
                }
                SetField( iField, aoList.List() );
                json_object_put(poJSonObj);
            }
            else
            {
                const char *papszValues[2] = { pszValue, nullptr };
                SetField( iField, const_cast<char **>(papszValues) );
            }
        }
    }
    else
    {
        // Do nothing for other field types.
    }
}

/************************************************************************/
/*                        OGR_F_SetFieldString()                        */
/************************************************************************/

/**
 * \brief Set field to string value.
 *
 * OFTInteger fields will be set based on an atoi() conversion of the string.
 * OFTInteger64 fields will be set based on an CPLAtoGIntBig() conversion of the
 * string.  OFTReal fields will be set based on an CPLAtof() conversion of the
 * string.  Other field types may be unaffected.
 *
 * This function is the same as the C++ method OGRFeature::SetField().
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param pszValue the value to assign.
 */

void OGR_F_SetFieldString( OGRFeatureH hFeat, int iField, const char *pszValue)

{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetFieldString" );

    OGRFeature::FromHandle(hFeat)->SetField( iField, pszValue );
}

/************************************************************************/
/*                              SetField()                              */
/************************************************************************/

/**
 * \fn OGRFeature::SetField( const char* pszFName, int nCount, const int *panValues )
 * \brief Set field to list of integers value.
 *
 * This method currently on has an effect of OFTIntegerList, OFTInteger64List
 * and OFTRealList fields.
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param pszFName the name of the field to set.
 * @param nCount the number of values in the list being assigned.
 * @param panValues the values to assign.
 */

/**
 * \brief Set field to list of integers value.
 *
 * This method currently on has an effect of OFTIntegerList, OFTInteger64List
 * and OFTRealList fields.
 *
 * This method is the same as the C function OGR_F_SetFieldIntegerList().
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param iField the field to set, from 0 to GetFieldCount()-1.
 * @param nCount the number of values in the list being assigned.
 * @param panValues the values to assign.
 */

void OGRFeature::SetField( int iField, int nCount, const int *panValues )

{
    OGRFieldDefn *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == nullptr )
        return;

    OGRFieldType eType = poFDefn->GetType();
    if( eType == OFTIntegerList )
    {
        OGRField uField;
        int *panValuesMod = nullptr;

        if( poFDefn->GetSubType() == OFSTBoolean ||
            poFDefn->GetSubType() == OFSTInt16 )
        {
            for( int i = 0; i < nCount; i++ )
            {
                int nVal = OGRFeatureGetIntegerValue(poFDefn, panValues[i]);
                if( panValues[i] != nVal )
                {
                    if( panValuesMod == nullptr )
                    {
                        panValuesMod = static_cast<int *>(
                            VSI_MALLOC_VERBOSE(nCount * sizeof(int)) );
                        if( panValuesMod == nullptr )
                            return;
                        memcpy(panValuesMod, panValues, nCount * sizeof(int));
                    }
                    panValuesMod[i] = nVal;
                }
            }
        }

        uField.IntegerList.nCount = nCount;
        uField.Set.nMarker2 = 0;
        uField.Set.nMarker3 = 0;
        uField.IntegerList.paList = panValuesMod ?
                panValuesMod : const_cast<int*>(panValues);

        SetField( iField, &uField );
        CPLFree(panValuesMod);
    }
    else if( eType == OFTInteger64List )
    {
        std::vector<GIntBig> anValues;
        anValues.reserve(nCount);
        for( int i = 0; i < nCount; i++ )
            anValues.push_back( panValues[i] );
        if( nCount > 0 )
            SetField( iField, nCount, &anValues[0] );
    }
    else if( eType == OFTRealList )
    {
        std::vector<double> adfValues;
        adfValues.reserve(nCount);
        for( int i = 0; i < nCount; i++ )
            adfValues.push_back( static_cast<double>(panValues[i]) );
        if( nCount > 0 )
            SetField( iField, nCount, &adfValues[0] );
    }
    else if( (eType == OFTInteger ||
              eType == OFTInteger64 ||
              eType == OFTReal)
             && nCount == 1 )
    {
        SetField( iField, panValues[0] );
    }
    else if( eType == OFTStringList )
    {
        char** papszValues = static_cast<char **>(
            VSI_MALLOC_VERBOSE((nCount+1) * sizeof(char*)) );
        if( papszValues == nullptr )
            return;
        for( int i = 0; i < nCount; i++ )
            papszValues[i] = VSI_STRDUP_VERBOSE(CPLSPrintf("%d", panValues[i]));
        papszValues[nCount] = nullptr;
        SetField( iField, papszValues);
        CSLDestroy(papszValues);
    }
}

/************************************************************************/
/*                     OGR_F_SetFieldIntegerList()                      */
/************************************************************************/

/**
 * \brief Set field to list of integers value.
 *
 * This function currently on has an effect of OFTIntegerList, OFTInteger64List
 * and OFTRealList fields.
 *
 * This function is the same as the C++ method OGRFeature::SetField().
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to set, from 0 to GetFieldCount()-1.
 * @param nCount the number of values in the list being assigned.
 * @param panValues the values to assign.
 */

void OGR_F_SetFieldIntegerList( OGRFeatureH hFeat, int iField,
                                int nCount, const int *panValues )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetFieldIntegerList" );

    OGRFeature::FromHandle(hFeat)->
        SetField( iField, nCount, panValues );
}

/************************************************************************/
/*                              SetField()                              */
/************************************************************************/

/**
 * \fn OGRFeature::SetField( const char* pszFName, int nCount, const GIntBig
 * *panValues )
 * \brief Set field to list of 64 bit integers value.
 *
 * This method currently on has an effect of OFTIntegerList, OFTInteger64List
 * and OFTRealList fields.
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param pszFName the name of the field to set.
 * @param nCount the number of values in the list being assigned.
 * @param panValues the values to assign.
 */

/**
 * \brief Set field to list of 64 bit integers value.
 *
 * This method currently on has an effect of OFTIntegerList, OFTInteger64List
 * and OFTRealList fields.
 *
 * This method is the same as the C function OGR_F_SetFieldInteger64List().
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param iField the field to set, from 0 to GetFieldCount()-1.
 * @param nCount the number of values in the list being assigned.
 * @param panValues the values to assign.
 * @since GDAL 2.0
 */

void OGRFeature::SetField( int iField, int nCount, const GIntBig *panValues )

{
    OGRFieldDefn *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == nullptr )
        return;

    OGRFieldType eType = poFDefn->GetType();
    if( eType == OFTIntegerList )
    {
        std::vector<int> anValues;

        for( int i = 0; i < nCount; i++ )
        {
            const GIntBig nValue = panValues[i];
            const int nVal32 =
                nValue < INT_MIN ? INT_MIN :
                nValue > INT_MAX ? INT_MAX : static_cast<int>(nValue);

            if( static_cast<GIntBig>(nVal32) != nValue )
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Integer overflow occurred when trying to set "
                          "32bit field." );
            }
            anValues.push_back( nVal32 );
        }
        if( nCount > 0 )
            SetField( iField, nCount, &anValues[0] );
    }
    else if( eType == OFTInteger64List )
    {
        OGRField uField;
        uField.Integer64List.nCount = nCount;
        uField.Set.nMarker2 = 0;
        uField.Set.nMarker3 = 0;
        uField.Integer64List.paList = const_cast<GIntBig *>(panValues);

        SetField( iField, &uField );
    }
    else if( eType == OFTRealList )
    {
        std::vector<double> adfValues;
        adfValues.reserve(nCount);
        for( int i = 0; i < nCount; i++ )
            adfValues.push_back( static_cast<double>(panValues[i]) );
        if( nCount > 0 )
            SetField( iField, nCount, &adfValues[0] );
    }
    else if( (eType == OFTInteger ||
              eType == OFTInteger64 ||
              eType == OFTReal)
             && nCount == 1 )
    {
        SetField( iField, panValues[0] );
    }
    else if( eType == OFTStringList )
    {
        char** papszValues = static_cast<char **>(
            VSI_MALLOC_VERBOSE((nCount+1) * sizeof(char*)) );
        if( papszValues == nullptr )
            return;
        for( int i = 0; i < nCount; i++ )
            papszValues[i] =
                VSI_STRDUP_VERBOSE(CPLSPrintf(CPL_FRMT_GIB, panValues[i]));
        papszValues[nCount] = nullptr;
        SetField( iField, papszValues);
        CSLDestroy(papszValues);
    }
}

/************************************************************************/
/*                    OGR_F_SetFieldInteger64List()                     */
/************************************************************************/

/**
 * \brief Set field to list of 64 bit integers value.
 *
 * This function currently on has an effect of OFTIntegerList, OFTInteger64List
 * and OFTRealList fields.
 *
 * This function is the same as the C++ method OGRFeature::SetField().
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to set, from 0 to GetFieldCount()-1.
 * @param nCount the number of values in the list being assigned.
 * @param panValues the values to assign.
 * @since GDAL 2.0
 */

void OGR_F_SetFieldInteger64List( OGRFeatureH hFeat, int iField,
                                  int nCount, const GIntBig *panValues )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetFieldInteger64List" );

    OGRFeature::FromHandle(hFeat)->SetField(iField, nCount, panValues);
}

/************************************************************************/
/*                              SetField()                              */
/************************************************************************/

/**
 * \fn OGRFeature::SetField( const char* pszFName, int nCount, const double * padfValues )
 * \brief Set field to list of doubles value.
 *
 * This method currently on has an effect of OFTIntegerList, OFTInteger64List,
 * OFTRealList fields.
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param pszFName the name of the field to set.
 * @param nCount the number of values in the list being assigned.
 * @param padfValues the values to assign.
 */

/**
 * \brief Set field to list of doubles value.
 *
 * This method currently on has an effect of OFTIntegerList, OFTInteger64List,
 * OFTRealList fields.
 *
 * This method is the same as the C function OGR_F_SetFieldDoubleList().
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param iField the field to set, from 0 to GetFieldCount()-1.
 * @param nCount the number of values in the list being assigned.
 * @param padfValues the values to assign.
 */

void OGRFeature::SetField( int iField, int nCount, const double * padfValues )

{
    OGRFieldDefn *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == nullptr )
        return;

    OGRFieldType eType = poFDefn->GetType();
    if( eType == OFTRealList )
    {
        OGRField uField;

        uField.RealList.nCount = nCount;
        uField.Set.nMarker2 = 0;
        uField.Set.nMarker3 = 0;
        uField.RealList.paList = const_cast<double*>(padfValues);

        SetField( iField, &uField );
    }
    else if( eType == OFTIntegerList )
    {
        std::vector<int> anValues;
        anValues.reserve(nCount);
        for( int i = 0; i < nCount; i++ )
            anValues.push_back( static_cast<int>(padfValues[i]) );

        if( nCount > 0 )
            SetField( iField, nCount, &anValues[0] );
    }
    else if( eType == OFTInteger64List )
    {
        std::vector<GIntBig> anValues;
        anValues.reserve(nCount);
        for( int i = 0; i < nCount; i++ )
            anValues.push_back( static_cast<GIntBig>(padfValues[i]) );
        if( nCount > 0 )
            SetField( iField, nCount, &anValues[0] );
    }
    else if( (eType == OFTInteger ||
              eType == OFTInteger64 ||
              eType == OFTReal)
             && nCount == 1 )
    {
        SetField( iField, padfValues[0] );
    }
    else if( eType == OFTStringList )
    {
        char** papszValues = static_cast<char **>(
            VSI_MALLOC_VERBOSE((nCount+1) * sizeof(char*)) );
        if( papszValues == nullptr )
            return;
        for( int i = 0; i < nCount; i++ )
            papszValues[i] =
                VSI_STRDUP_VERBOSE(CPLSPrintf("%.16g", padfValues[i]));
        papszValues[nCount] = nullptr;
        SetField( iField, papszValues);
        CSLDestroy(papszValues);
    }
}

/************************************************************************/
/*                      OGR_F_SetFieldDoubleList()                      */
/************************************************************************/

/**
 * \brief Set field to list of doubles value.
 *
 * This function currently on has an effect of OFTIntegerList, OFTInteger64List,
 * OFTRealList fields.
 *
 * This function is the same as the C++ method OGRFeature::SetField().
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to set, from 0 to GetFieldCount()-1.
 * @param nCount the number of values in the list being assigned.
 * @param padfValues the values to assign.
 */

void OGR_F_SetFieldDoubleList( OGRFeatureH hFeat, int iField,
                               int nCount, const double *padfValues )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetFieldDoubleList" );

    OGRFeature::FromHandle(hFeat)->SetField(iField, nCount, padfValues);
}

/************************************************************************/
/*                              SetField()                              */
/************************************************************************/

/**
 * \fn OGRFeature::SetField( const char* pszFName,  const char * const * papszValues )
 * \brief Set field to list of strings value.
 *
 * This method currently on has an effect of OFTStringList fields.
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param pszFName the name of the field to set.
 * @param papszValues the values to assign. List of NUL-terminated string, ending
 * with a NULL pointer.
 */

/**
 * \brief Set field to list of strings value.
 *
 * This method currently on has an effect of OFTStringList fields.
 *
 * This method is the same as the C function OGR_F_SetFieldStringList().
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param iField the field to set, from 0 to GetFieldCount()-1.
 * @param papszValues the values to assign. List of NUL-terminated string, ending
 * with a NULL pointer.
 */

void OGRFeature::SetField( int iField, const char * const * papszValues )

{
    OGRFieldDefn *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == nullptr )
        return;

    OGRFieldType eType = poFDefn->GetType();
    if( eType == OFTStringList )
    {
        if( !IsFieldSetAndNotNullUnsafe(iField) ||
            papszValues != pauFields[iField].StringList.paList )
        {
            OGRField uField;

            uField.StringList.nCount = CSLCount(papszValues);
            uField.Set.nMarker2 = 0;
            uField.Set.nMarker3 = 0;
            uField.StringList.paList = const_cast<char**>(papszValues);

            SetField( iField, &uField );
        }
    }
    else if( eType == OFTIntegerList )
    {
        const int nValues = CSLCount(papszValues);
        int* panValues = static_cast<int *>(
            VSI_MALLOC_VERBOSE(nValues * sizeof(int)) );
        if( panValues == nullptr )
            return;
        for( int i = 0; i < nValues; i++ )
        {
            // As allowed by C standard, some systems like MSVC do not
            // reset errno.
            errno = 0;

            int nVal = atoi(papszValues[i]);
            if( errno == ERANGE )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "32 bit integer overflow when converting %s",
                         papszValues[i]);
                if( papszValues[i][0] == '-' )
                    nVal = INT_MIN;
                else
                    nVal = INT_MAX;
            }
            panValues[i] = nVal;
        }
        SetField( iField, nValues, panValues );
        CPLFree(panValues);
    }
    else if( eType == OFTInteger64List )
    {
        const int nValues = CSLCount(papszValues);
        GIntBig* panValues = static_cast<GIntBig *>(
            VSI_MALLOC_VERBOSE(nValues * sizeof(GIntBig)) );
        if( panValues == nullptr )
            return;
        for( int i = 0; i < nValues; i++ )
        {
            panValues[i] = CPLAtoGIntBigEx(papszValues[i], TRUE, nullptr);
        }
        SetField( iField, nValues, panValues );
        CPLFree(panValues);
    }
    else if( eType == OFTRealList )
    {
        const int nValues = CSLCount(papszValues);
        double* padfValues = static_cast<double *>(
            VSI_MALLOC_VERBOSE(nValues * sizeof(double)) );
        if( padfValues == nullptr )
            return;
        for( int i = 0; i < nValues; i++ )
        {
            padfValues[i] = CPLAtof(papszValues[i]);
        }
        SetField( iField, nValues, padfValues );
        CPLFree(padfValues);
    }
}

/************************************************************************/
/*                      OGR_F_SetFieldStringList()                      */
/************************************************************************/

/**
 * \brief Set field to list of strings value.
 *
 * This function currently on has an effect of OFTStringList fields.
 *
 * This function is the same as the C++ method OGRFeature::SetField().
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to set, from 0 to GetFieldCount()-1.
 * @param papszValues the values to assign. List of NUL-terminated string, ending
 * with a NULL pointer.
 */

void OGR_F_SetFieldStringList( OGRFeatureH hFeat, int iField,
                               CSLConstList papszValues )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetFieldStringList" );

    OGRFeature::FromHandle(hFeat)->SetField( iField,
                                const_cast<const char* const*>(papszValues) );
}

/************************************************************************/
/*                              SetField()                              */
/************************************************************************/

/**
 * \brief Set field to binary data.
 *
 * This method currently on has an effect of OFTBinary fields.
 *
 * This method is the same as the C function OGR_F_SetFieldBinary().
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param iField the field to set, from 0 to GetFieldCount()-1.
 * @param nBytes bytes of data being set.
 * @param pabyData the raw data being applied.
 */

void OGRFeature::SetField( int iField, int nBytes, const void *pabyData )

{
    OGRFieldDefn *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == nullptr )
        return;

    OGRFieldType eType = poFDefn->GetType();
    if( eType == OFTBinary )
    {
        OGRField uField;

        uField.Binary.nCount = nBytes;
        uField.Set.nMarker2 = 0;
        uField.Set.nMarker3 = 0;
        uField.Binary.paData = const_cast<GByte*>(static_cast<const GByte*>(pabyData));

        SetField( iField, &uField );
    }
    else if( eType == OFTString || eType == OFTStringList )
    {
        char* pszStr = static_cast<char *>( VSI_MALLOC_VERBOSE(nBytes + 1) );
        if( pszStr == nullptr )
            return;
        memcpy(pszStr, pabyData, nBytes);
        pszStr[nBytes] = 0;
        SetField( iField, pszStr );
        CPLFree( pszStr);
    }
}

/************************************************************************/
/*                        OGR_F_SetFieldBinary()                        */
/************************************************************************/

/**
 * \brief Set field to binary data.
 *
 * This function currently on has an effect of OFTBinary fields.
 *
 * This function is the same as the C++ method OGRFeature::SetField().
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to set, from 0 to GetFieldCount()-1.
 * @param nBytes the number of bytes in pabyData array.
 * @param pabyData the data to apply.
 */

void OGR_F_SetFieldBinary( OGRFeatureH hFeat, int iField,
                           int nBytes, const void *pabyData )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetFieldBinary" );

    OGRFeature::FromHandle(hFeat)->SetField( iField, nBytes, pabyData );
}

/************************************************************************/
/*                              SetField()                              */
/************************************************************************/

/**
 * \fn OGRFeature::SetField( const char* pszFName, int nYear, int nMonth,
 *                           int nDay, int nHour, int nMinute, float fSecond,
 *                           int nTZFlag )
 * \brief Set field to date.
 *
 * This method currently only has an effect for OFTDate, OFTTime and OFTDateTime
 * fields.
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param pszFName the name of the field to set.
 * @param nYear (including century)
 * @param nMonth (1-12)
 * @param nDay (1-31)
 * @param nHour (0-23)
 * @param nMinute (0-59)
 * @param fSecond (0-59, with millisecond accuracy)
 * @param nTZFlag (0=unknown, 1=localtime, 100=GMT, see data model for details)
 */

/**
 * \brief Set field to date.
 *
 * This method currently only has an effect for OFTDate, OFTTime and OFTDateTime
 * fields.
 *
 * This method is the same as the C function OGR_F_SetFieldDateTime().
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param iField the field to set, from 0 to GetFieldCount()-1.
 * @param nYear (including century)
 * @param nMonth (1-12)
 * @param nDay (1-31)
 * @param nHour (0-23)
 * @param nMinute (0-59)
 * @param fSecond (0-59, with millisecond accuracy)
 * @param nTZFlag (0=unknown, 1=localtime, 100=GMT, see data model for details)
 */

void OGRFeature::SetField( int iField, int nYear, int nMonth, int nDay,
                           int nHour, int nMinute, float fSecond,
                           int nTZFlag )

{
    OGRFieldDefn *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == nullptr )
        return;

    OGRFieldType eType = poFDefn->GetType();
    if( eType == OFTDate
        || eType == OFTTime
        || eType == OFTDateTime )
    {
        if( static_cast<GInt16>(nYear) != nYear )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Years < -32768 or > 32767 are not supported");
            return;
        }

        pauFields[iField].Date.Year = static_cast<GInt16>(nYear);
        pauFields[iField].Date.Month = static_cast<GByte>(nMonth);
        pauFields[iField].Date.Day = static_cast<GByte>(nDay);
        pauFields[iField].Date.Hour = static_cast<GByte>(nHour);
        pauFields[iField].Date.Minute = static_cast<GByte>(nMinute);
        pauFields[iField].Date.Second = fSecond;
        pauFields[iField].Date.TZFlag = static_cast<GByte>(nTZFlag);
    }
    else if( eType == OFTString || eType == OFTStringList )
    {
        // "YYYY/MM/DD HH:MM:SS.sss+ZZ"
        constexpr size_t MAX_SIZE = 26 + 1;
        char szTempBuffer[MAX_SIZE] = {};
        OGRFeatureFormatDateTimeBuffer(szTempBuffer,
                                       MAX_SIZE,
                                       nYear,
                                       nMonth,
                                       nDay,
                                       nHour,
                                       nMinute,
                                       fSecond,
                                       nTZFlag);
        SetField( iField, szTempBuffer );
    }
}

/************************************************************************/
/*                       OGR_F_SetFieldDateTime()                       */
/************************************************************************/

/**
 * \brief Set field to datetime.
 *
 * This method currently only has an effect for OFTDate, OFTTime and OFTDateTime
 * fields.
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to set, from 0 to GetFieldCount()-1.
 * @param nYear (including century)
 * @param nMonth (1-12)
 * @param nDay (1-31)
 * @param nHour (0-23)
 * @param nMinute (0-59)
 * @param nSecond (0-59)
 * @param nTZFlag (0=unknown, 1=localtime, 100=GMT, see data model for details)
 *
 * @see Use OGR_F_SetFieldDateTimeEx() for second with millisecond accuracy.
 */

void OGR_F_SetFieldDateTime( OGRFeatureH hFeat, int iField,
                             int nYear, int nMonth, int nDay,
                             int nHour, int nMinute, int nSecond,
                             int nTZFlag )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetFieldDateTime" );

    OGRFeature::FromHandle(hFeat)->
        SetField( iField, nYear, nMonth, nDay,
                  nHour, nMinute, static_cast<float>(nSecond), nTZFlag );
}

/************************************************************************/
/*                      OGR_F_SetFieldDateTimeEx()                      */
/************************************************************************/

/**
 * \brief Set field to datetime.
 *
 * This method currently only has an effect for OFTDate, OFTTime and OFTDateTime
 * fields.
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to set, from 0 to GetFieldCount()-1.
 * @param nYear (including century)
 * @param nMonth (1-12)
 * @param nDay (1-31)
 * @param nHour (0-23)
 * @param nMinute (0-59)
 * @param fSecond (0-59, with millisecond accuracy)
 * @param nTZFlag (0=unknown, 1=localtime, 100=GMT, see data model for details)
 *
 * @since GDAL 2.0
 */

void OGR_F_SetFieldDateTimeEx( OGRFeatureH hFeat, int iField,
                               int nYear, int nMonth, int nDay,
                               int nHour, int nMinute, float fSecond,
                               int nTZFlag )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetFieldDateTimeEx" );

    OGRFeature::FromHandle(hFeat)->
        SetField( iField, nYear, nMonth, nDay,
                  nHour, nMinute, fSecond, nTZFlag );
}

/************************************************************************/
/*                              SetField()                              */
/************************************************************************/

/**
 * \fn OGRFeature::SetField( const char* pszFName, const OGRField * puValue )
 * \brief Set field.
 *
 * The passed value OGRField must be of exactly the same type as the
 * target field, or an application crash may occur.  The passed value
 * is copied, and will not be affected.  It remains the responsibility of
 * the caller.
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param pszFName the name of the field to set.
 * @param puValue the value to assign.
 */

/**
 * \brief Set field.
 *
 * The passed value OGRField must be of exactly the same type as the
 * target field, or an application crash may occur.  The passed value
 * is copied, and will not be affected.  It remains the responsibility of
 * the caller.
 *
 * This method is the same as the C function OGR_F_SetFieldRaw().
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param puValue the value to assign.
 */

void OGRFeature::SetField( int iField, const OGRField * puValue )

{
    SetFieldInternal( iField, puValue );
}

bool OGRFeature::SetFieldInternal( int iField, const OGRField * puValue )

{
    OGRFieldDefn *poFDefn = poDefn->GetFieldDefn( iField );
    if( poFDefn == nullptr )
        return false;

    if( poFDefn->GetType() == OFTInteger )
    {
        pauFields[iField] = *puValue;
    }
    else if( poFDefn->GetType() == OFTInteger64 )
    {
        pauFields[iField] = *puValue;
    }
    else if( poFDefn->GetType() == OFTReal )
    {
        pauFields[iField] = *puValue;
    }
    else if( poFDefn->GetType() == OFTString )
    {
        if( IsFieldSetAndNotNullUnsafe(iField) )
            CPLFree( pauFields[iField].String );

        if( puValue->String == nullptr )
            pauFields[iField].String = nullptr;
        else if( OGR_RawField_IsUnset(puValue) ||
                 OGR_RawField_IsNull(puValue) )
            pauFields[iField] = *puValue;
        else
        {
            pauFields[iField].String = VSI_STRDUP_VERBOSE( puValue->String );
            if( pauFields[iField].String == nullptr )
            {
                OGR_RawField_SetUnset(&pauFields[iField]);
                return false;
            }
        }
    }
    else if( poFDefn->GetType() == OFTDate
             || poFDefn->GetType() == OFTTime
             || poFDefn->GetType() == OFTDateTime )
    {
        memcpy( pauFields+iField, puValue, sizeof(OGRField) );
    }
    else if( poFDefn->GetType() == OFTIntegerList )
    {
        const int nCount = puValue->IntegerList.nCount;

        if( IsFieldSetAndNotNullUnsafe(iField) )
            CPLFree( pauFields[iField].IntegerList.paList );

        if( OGR_RawField_IsUnset(puValue) ||
            OGR_RawField_IsNull(puValue) )
        {
            pauFields[iField] = *puValue;
        }
        else
        {
            pauFields[iField].IntegerList.paList =
                static_cast<int *>( VSI_MALLOC_VERBOSE(sizeof(int) * nCount) );
            if( pauFields[iField].IntegerList.paList == nullptr )
            {
                OGR_RawField_SetUnset(&pauFields[iField]);
                return false;
            }
            memcpy( pauFields[iField].IntegerList.paList,
                    puValue->IntegerList.paList,
                    sizeof(int) * nCount );
            pauFields[iField].IntegerList.nCount = nCount;
        }
    }
    else if( poFDefn->GetType() == OFTInteger64List )
    {
        const int nCount = puValue->Integer64List.nCount;

        if( IsFieldSetAndNotNullUnsafe(iField) )
            CPLFree( pauFields[iField].Integer64List.paList );

        if( OGR_RawField_IsUnset(puValue) ||
            OGR_RawField_IsNull(puValue) )
        {
            pauFields[iField] = *puValue;
        }
        else
        {
            pauFields[iField].Integer64List.paList = static_cast<GIntBig *>(
                VSI_MALLOC_VERBOSE(sizeof(GIntBig) * nCount) );
            if( pauFields[iField].Integer64List.paList == nullptr )
            {
                OGR_RawField_SetUnset(&pauFields[iField]);
                return false;
            }
            memcpy( pauFields[iField].Integer64List.paList,
                    puValue->Integer64List.paList,
                    sizeof(GIntBig) * nCount );
            pauFields[iField].Integer64List.nCount = nCount;
        }
    }
    else if( poFDefn->GetType() == OFTRealList )
    {
        const int nCount = puValue->RealList.nCount;

        if( IsFieldSetAndNotNullUnsafe(iField) )
            CPLFree( pauFields[iField].RealList.paList );

        if( OGR_RawField_IsUnset(puValue) ||
            OGR_RawField_IsNull(puValue) )
        {
            pauFields[iField] = *puValue;
        }
        else
        {
            pauFields[iField].RealList.paList = static_cast<double *>(
                VSI_MALLOC_VERBOSE(sizeof(double) * nCount) );
            if( pauFields[iField].RealList.paList == nullptr )
            {
                OGR_RawField_SetUnset(&pauFields[iField]);
                return false;
            }
            memcpy( pauFields[iField].RealList.paList,
                    puValue->RealList.paList,
                    sizeof(double) * nCount );
            pauFields[iField].RealList.nCount = nCount;
        }
    }
    else if( poFDefn->GetType() == OFTStringList )
    {
        if( IsFieldSetAndNotNullUnsafe(iField) )
            CSLDestroy( pauFields[iField].StringList.paList );

        if( OGR_RawField_IsUnset(puValue) ||
            OGR_RawField_IsNull(puValue) )
        {
            pauFields[iField] = *puValue;
        }
        else
        {
            char** papszNewList = nullptr;
            for( char** papszIter = puValue->StringList.paList;
                 papszIter != nullptr && *papszIter != nullptr;
                 ++papszIter )
            {
                char** papszNewList2 =
                    CSLAddStringMayFail(papszNewList, *papszIter);
                if( papszNewList2 == nullptr )
                {
                    CSLDestroy(papszNewList);
                    OGR_RawField_SetUnset(&pauFields[iField]);
                    return false;
                }
                papszNewList = papszNewList2;
            }
            pauFields[iField].StringList.paList = papszNewList;
            pauFields[iField].StringList.nCount = puValue->StringList.nCount;
            CPLAssert( CSLCount(puValue->StringList.paList)
                       == puValue->StringList.nCount );
        }
    }
    else if( poFDefn->GetType() == OFTBinary )
    {
        if( IsFieldSetAndNotNullUnsafe(iField) )
            CPLFree( pauFields[iField].Binary.paData );

        if( OGR_RawField_IsUnset(puValue) ||
            OGR_RawField_IsNull(puValue) )
        {
            pauFields[iField] = *puValue;
        }
        else
        {
            pauFields[iField].Binary.paData = static_cast<GByte *>(
                VSI_MALLOC_VERBOSE(puValue->Binary.nCount) );
            if( pauFields[iField].Binary.paData == nullptr )
            {
                OGR_RawField_SetUnset(&pauFields[iField]);
                return false;
            }
            memcpy( pauFields[iField].Binary.paData,
                    puValue->Binary.paData,
                    puValue->Binary.nCount );
            pauFields[iField].Binary.nCount = puValue->Binary.nCount;
        }
    }
    else
    {
        // Do nothing for other field types.
    }
    return true;
}

/************************************************************************/
/*                      OGR_F_SetFieldRaw()                             */
/************************************************************************/

/**
 * \brief Set field.
 *
 * The passed value OGRField must be of exactly the same type as the
 * target field, or an application crash may occur.  The passed value
 * is copied, and will not be affected.  It remains the responsibility of
 * the caller.
 *
 * This function is the same as the C++ method OGRFeature::SetField().
 *
 * @note This method has only an effect on the in-memory feature object. If
 * this object comes from a layer and the modifications must be serialized back
 * to the datasource, OGR_L_SetFeature() must be used afterwards. Or if this is
 * a new feature, OGR_L_CreateFeature() must be used afterwards.
 *
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param psValue handle on the value to assign.
 */

void OGR_F_SetFieldRaw( OGRFeatureH hFeat, int iField, const OGRField *psValue )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetFieldRaw" );

    OGRFeature::FromHandle(hFeat)->SetField( iField, psValue );
}

/************************************************************************/
/*                            DumpReadable()                            */
/************************************************************************/

/**
 * \brief Dump this feature in a human readable form.
 *
 * This dumps the attributes, and geometry; however, it doesn't definition
 * information (other than field types and names), nor does it report the
 * geometry spatial reference system.
 *
 * A few options can be defined to change the default dump :
 * <ul>
 * <li>DISPLAY_FIELDS=NO : to hide the dump of the attributes</li>
 * <li>DISPLAY_STYLE=NO : to hide the dump of the style string</li>
 * <li>DISPLAY_GEOMETRY=NO : to hide the dump of the geometry</li>
 * <li>DISPLAY_GEOMETRY=SUMMARY : to get only a summary of the geometry</li>
 * </ul>
 *
 * This method is the same as the C function OGR_F_DumpReadable().
 *
 * @param fpOut the stream to write to, such as stdout.  If NULL stdout will
 * be used.
 * @param papszOptions NULL terminated list of options (may be NULL)
 */

void OGRFeature::DumpReadable( FILE * fpOut, char** papszOptions ) const

{
    if( fpOut == nullptr )
        fpOut = stdout;

    char szFID[32];
    CPLsnprintf(szFID, sizeof(szFID), CPL_FRMT_GIB, GetFID());
    fprintf( fpOut,
             "OGRFeature(%s):%s\n", poDefn->GetName(), szFID );

    const char* pszDisplayFields =
        CSLFetchNameValue(papszOptions, "DISPLAY_FIELDS");
    if( pszDisplayFields == nullptr || CPLTestBool(pszDisplayFields) )
    {
        for( int iField = 0; iField < GetFieldCount(); iField++ )
        {
            if( !IsFieldSet( iField) )
                continue;
            OGRFieldDefn *poFDefn = poDefn->GetFieldDefn(iField);

            const char* pszType = (poFDefn->GetSubType() != OFSTNone) ?
                CPLSPrintf(
                    "%s(%s)",
                    poFDefn->GetFieldTypeName( poFDefn->GetType() ),
                    poFDefn->GetFieldSubTypeName(poFDefn->GetSubType())) :
                    poFDefn->GetFieldTypeName( poFDefn->GetType() );

            fprintf( fpOut, "  %s (%s) = ",
                    poFDefn->GetNameRef(),
                    pszType );

            if( IsFieldNull( iField) )
                fprintf( fpOut, "(null)\n" );
            else
                fprintf( fpOut, "%s\n", GetFieldAsString( iField ) );
        }
    }

    if( GetStyleString() != nullptr )
    {
        const char* pszDisplayStyle =
            CSLFetchNameValue(papszOptions, "DISPLAY_STYLE");
        if( pszDisplayStyle == nullptr || CPLTestBool(pszDisplayStyle) )
        {
            fprintf( fpOut, "  Style = %s\n", GetStyleString() );
        }
    }

    const int nGeomFieldCount = GetGeomFieldCount();
    if( nGeomFieldCount > 0 )
    {
        const char* pszDisplayGeometry =
            CSLFetchNameValue(papszOptions, "DISPLAY_GEOMETRY");
        if( !(pszDisplayGeometry != nullptr && EQUAL(pszDisplayGeometry, "NO")) )
        {
            for( int iField = 0; iField < nGeomFieldCount; iField++ )
            {
                OGRGeomFieldDefn *poFDefn = poDefn->GetGeomFieldDefn(iField);

                if( papoGeometries[iField] != nullptr )
                {
                    fprintf( fpOut, "  " );
                    if( strlen(poFDefn->GetNameRef()) > 0 &&
                        GetGeomFieldCount() > 1 )
                        fprintf( fpOut, "%s = ", poFDefn->GetNameRef() );
                    papoGeometries[iField]->dumpReadable( fpOut, "",
                                                          papszOptions );
                }
            }
        }
    }

    fprintf( fpOut, "\n" );
}

/************************************************************************/
/*                         OGR_F_DumpReadable()                         */
/************************************************************************/

/**
 * \brief Dump this feature in a human readable form.
 *
 * This dumps the attributes, and geometry; however, it doesn't definition
 * information (other than field types and names), nor does it report the
 * geometry spatial reference system.
 *
 * This function is the same as the C++ method OGRFeature::DumpReadable().
 *
 * @param hFeat handle to the feature to dump.
 * @param fpOut the stream to write to, such as strout.
 */

void OGR_F_DumpReadable( OGRFeatureH hFeat, FILE *fpOut )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_DumpReadable" );

    OGRFeature::FromHandle(hFeat)->DumpReadable( fpOut );
}

/************************************************************************/
/*                               GetFID()                               */
/************************************************************************/

/**
 * \fn GIntBig OGRFeature::GetFID() const;
 *
 * \brief Get feature identifier.
 *
 * This method is the same as the C function OGR_F_GetFID().
 * Note: since GDAL 2.0, this method returns a GIntBig (previously a long)
 *
 * @return feature id or OGRNullFID if none has been assigned.
 */

/************************************************************************/
/*                            OGR_F_GetFID()                          */
/************************************************************************/

/**
 * \brief Get feature identifier.
 *
 * This function is the same as the C++ method OGRFeature::GetFID().
 * Note: since GDAL 2.0, this method returns a GIntBig (previously a long)
 *
 * @param hFeat handle to the feature from which to get the feature
 * identifier.
 * @return feature id or OGRNullFID if none has been assigned.
 */

GIntBig OGR_F_GetFID( OGRFeatureH hFeat )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetFID", 0 );

    return OGRFeature::FromHandle(hFeat)->GetFID();
}

/************************************************************************/
/*                               SetFID()                               */
/************************************************************************/

/**
 * \brief Set the feature identifier.
 *
 * For specific types of features this operation may fail on illegal
 * features ids.  Generally it always succeeds.  Feature ids should be
 * greater than or equal to zero, with the exception of OGRNullFID (-1)
 * indicating that the feature id is unknown.
 *
 * This method is the same as the C function OGR_F_SetFID().
 *
 * @param nFIDIn the new feature identifier value to assign.
 *
 * @return On success OGRERR_NONE, or on failure some other value.
 */

OGRErr OGRFeature::SetFID( GIntBig nFIDIn )

{
    nFID = nFIDIn;

    return OGRERR_NONE;
}

/************************************************************************/
/*                            OGR_F_SetFID()                            */
/************************************************************************/

/**
 * \brief Set the feature identifier.
 *
 * For specific types of features this operation may fail on illegal
 * features ids.  Generally it always succeeds.  Feature ids should be
 * greater than or equal to zero, with the exception of OGRNullFID (-1)
 * indicating that the feature id is unknown.
 *
 * This function is the same as the C++ method OGRFeature::SetFID().
 *
 * @param hFeat handle to the feature to set the feature id to.
 * @param nFID the new feature identifier value to assign.
 *
 * @return On success OGRERR_NONE, or on failure some other value.
 */

OGRErr OGR_F_SetFID( OGRFeatureH hFeat, GIntBig nFID )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_SetFID", OGRERR_FAILURE );

    return OGRFeature::FromHandle(hFeat)->SetFID(nFID);
}

/************************************************************************/
/*                               Equal()                                */
/************************************************************************/

/**
 * \brief Test if two features are the same.
 *
 * Two features are considered equal if the share them (pointer equality)
 * same OGRFeatureDefn, have the same field values, and the same geometry
 * (as tested by OGRGeometry::Equal()) as well as the same feature id.
 *
 * This method is the same as the C function OGR_F_Equal().
 *
 * @param poFeature the other feature to test this one against.
 *
 * @return TRUE if they are equal, otherwise FALSE.
 */

OGRBoolean OGRFeature::Equal( const OGRFeature * poFeature ) const

{
    if( poFeature == this )
        return TRUE;

    if( GetFID() != poFeature->GetFID() )
        return FALSE;

    if( GetDefnRef() != poFeature->GetDefnRef() )
        return FALSE;

    const int nFields = GetDefnRef()->GetFieldCount();
    for( int i = 0; i < nFields; i++ )
    {
        if( IsFieldSet(i) != poFeature->IsFieldSet(i) )
            return FALSE;
        if( IsFieldNull(i) != poFeature->IsFieldNull(i) )
            return FALSE;
        if( !IsFieldSetAndNotNullUnsafe(i) )
            continue;

        switch( GetDefnRef()->GetFieldDefn(i)->GetType() )
        {
            case OFTInteger:
                if( GetFieldAsInteger(i) !=
                       poFeature->GetFieldAsInteger(i) )
                    return FALSE;
                break;

            case OFTInteger64:
                if( GetFieldAsInteger64(i) !=
                       poFeature->GetFieldAsInteger64(i) )
                    return FALSE;
                break;

            case OFTReal:
            {
                const double dfVal1 = GetFieldAsDouble(i);
                const double dfVal2 = poFeature->GetFieldAsDouble(i);
                if( std::isnan(dfVal1) )
                {
                    if( !std::isnan(dfVal2) )
                        return FALSE;
                }
                else if( std::isnan(dfVal2) )
                {
                    if( !std::isnan(dfVal1) )
                        return FALSE;
                }
                else if ( dfVal1 != dfVal2 )
                {
                    return FALSE;
                }
                break;
            }

            case OFTString:
                if( strcmp(GetFieldAsString(i),
                           poFeature->GetFieldAsString(i)) != 0 )
                    return FALSE;
                break;

            case OFTIntegerList:
            {
                int nCount1 = 0;
                int nCount2 = 0;
                const int* pnList1 = GetFieldAsIntegerList(i, &nCount1);
                const int* pnList2 =
                    poFeature->GetFieldAsIntegerList(i, &nCount2);
                if( nCount1 != nCount2 )
                    return FALSE;
                for( int j = 0; j < nCount1; j++ )
                {
                    if( pnList1[j] != pnList2[j] )
                        return FALSE;
                }
                break;
            }

            case OFTInteger64List:
            {
                int nCount1 = 0;
                int nCount2 = 0;
                const GIntBig* pnList1 = GetFieldAsInteger64List(i, &nCount1);
                const GIntBig* pnList2 =
                    poFeature->GetFieldAsInteger64List(i, &nCount2);
                if( nCount1 != nCount2 )
                    return FALSE;
                for( int j = 0; j < nCount1; j++ )
                {
                    if( pnList1[j] != pnList2[j] )
                        return FALSE;
                }
                break;
            }

            case OFTRealList:
            {
                int nCount1 = 0;
                int nCount2 = 0;
                const double* padfList1 = GetFieldAsDoubleList(i, &nCount1);
                const double* padfList2 =
                    poFeature->GetFieldAsDoubleList(i, &nCount2);
                if( nCount1 != nCount2 )
                    return FALSE;
                for( int j = 0; j < nCount1; j++ )
                {
                    const double dfVal1 = padfList1[j];
                    const double dfVal2 = padfList2[j];
                    if( std::isnan(dfVal1) )
                    {
                        if( !std::isnan(dfVal2) )
                            return FALSE;
                    }
                    else if( std::isnan(dfVal2) )
                    {
                        if( !std::isnan(dfVal1) )
                            return FALSE;
                    }
                    else if ( dfVal1 != dfVal2 )
                    {
                        return FALSE;
                    }
                }
                break;
            }

            case OFTStringList:
            {
                int nCount1 = 0;
                int nCount2 = 0;
                char** papszList1 = GetFieldAsStringList(i);
                char** papszList2 = poFeature->GetFieldAsStringList(i);
                nCount1 = CSLCount(papszList1);
                nCount2 = CSLCount(papszList2);
                if( nCount1 != nCount2 )
                    return FALSE;
                for( int j = 0; j < nCount1; j++ )
                {
                    if( strcmp(papszList1[j], papszList2[j]) != 0 )
                        return FALSE;
                }
                break;
            }

            case OFTTime:
            case OFTDate:
            case OFTDateTime:
            {
                int nYear1 = 0;
                int nMonth1 = 0;
                int nDay1 = 0;
                int nHour1 = 0;
                int nMinute1 = 0;
                int nTZFlag1 = 0;
                int nYear2 = 0;
                int nMonth2 = 0;
                int nDay2 = 0;
                int nHour2 = 0;
                int nMinute2 = 0;
                int nTZFlag2 = 0;
                float fSecond1 = 0.0f;
                float fSecond2 = 0.0f;
                GetFieldAsDateTime(i, &nYear1, &nMonth1, &nDay1,
                              &nHour1, &nMinute1, &fSecond1, &nTZFlag1);
                poFeature->GetFieldAsDateTime(i, &nYear2, &nMonth2, &nDay2,
                              &nHour2, &nMinute2, &fSecond2, &nTZFlag2);

                if( !(nYear1 == nYear2 && nMonth1 == nMonth2 &&
                      nDay1 == nDay2 && nHour1 == nHour2 &&
                      nMinute1 == nMinute2 && fSecond1 == fSecond2 &&
                      nTZFlag1 == nTZFlag2) )
                    return FALSE;
                break;
            }

            case OFTBinary:
            {
                int nCount1 = 0;
                int nCount2 = 0;
                GByte* pabyData1 = GetFieldAsBinary(i, &nCount1);
                GByte* pabyData2 = poFeature->GetFieldAsBinary(i, &nCount2);
                if( nCount1 != nCount2 )
                    return FALSE;
                if( memcmp(pabyData1, pabyData2, nCount1) != 0 )
                    return FALSE;
                break;
            }

            default:
                if( strcmp(GetFieldAsString(i),
                           poFeature->GetFieldAsString(i)) != 0 )
                    return FALSE;
                break;
        }
    }

    const int nGeomFieldCount = GetGeomFieldCount();
    for( int i = 0; i < nGeomFieldCount; i++ )
    {
        const OGRGeometry* poThisGeom = GetGeomFieldRef(i);
        const OGRGeometry* poOtherGeom = poFeature->GetGeomFieldRef(i);

        if( poThisGeom == nullptr && poOtherGeom != nullptr )
            return FALSE;

        if( poThisGeom != nullptr && poOtherGeom == nullptr )
            return FALSE;

        if( poThisGeom != nullptr && poOtherGeom != nullptr
            && (!poThisGeom->Equals( poOtherGeom ) ) )
            return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                            OGR_F_Equal()                             */
/************************************************************************/

/**
 * \brief Test if two features are the same.
 *
 * Two features are considered equal if the share them (handle equality)
 * same OGRFeatureDefn, have the same field values, and the same geometry
 * (as tested by OGR_G_Equal()) as well as the same feature id.
 *
 * This function is the same as the C++ method OGRFeature::Equal().
 *
 * @param hFeat handle to one of the feature.
 * @param hOtherFeat handle to the other feature to test this one against.
 *
 * @return TRUE if they are equal, otherwise FALSE.
 */

int OGR_F_Equal( OGRFeatureH hFeat, OGRFeatureH hOtherFeat )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_Equal", 0 );
    VALIDATE_POINTER1( hOtherFeat, "OGR_F_Equal", 0 );

    return OGRFeature::FromHandle(hFeat)->
        Equal( OGRFeature::FromHandle(hOtherFeat) );
}

/************************************************************************/
/*                              SetFrom()                               */
/************************************************************************/

/**
 * \brief Set one feature from another.
 *
 * Overwrite the contents of this feature from the geometry and attributes
 * of another.  The poSrcFeature does not need to have the same
 * OGRFeatureDefn.  Field values are copied by corresponding field names.
 * Field types do not have to exactly match.  SetField() method conversion
 * rules will be applied as needed.
 *
 * This method is the same as the C function OGR_F_SetFrom().
 *
 * @param poSrcFeature the feature from which geometry, and field values will
 * be copied.
 *
 * @param bForgiving TRUE if the operation should continue despite lacking
 * output fields matching some of the source fields.
 *
 * @return OGRERR_NONE if the operation succeeds, even if some values are
 * not transferred, otherwise an error code.
 */

OGRErr OGRFeature::SetFrom( const OGRFeature * poSrcFeature, int bForgiving )

{
    const auto& oMap = poDefn->ComputeMapForSetFrom(
        poSrcFeature->GetDefnRef(), CPL_TO_BOOL(bForgiving) );
    if( oMap.empty() )
    {
        if( poSrcFeature->GetFieldCount() )
            return OGRERR_FAILURE;
        int dummy = 0;
        return SetFrom( poSrcFeature, &dummy, bForgiving );
    }
    return SetFrom( poSrcFeature, oMap.data(), bForgiving );
}

/************************************************************************/
/*                           OGR_F_SetFrom()                            */
/************************************************************************/

/**
 * \brief Set one feature from another.
 *
 * Overwrite the contents of this feature from the geometry and attributes
 * of another.  The hOtherFeature does not need to have the same
 * OGRFeatureDefn.  Field values are copied by corresponding field names.
 * Field types do not have to exactly match.  OGR_F_SetField*() function
 * conversion rules will be applied as needed.
 *
 * This function is the same as the C++ method OGRFeature::SetFrom().
 *
 * @param hFeat handle to the feature to set to.
 * @param hOtherFeat handle to the feature from which geometry,
 * and field values will be copied.
 *
 * @param bForgiving TRUE if the operation should continue despite lacking
 * output fields matching some of the source fields.
 *
 * @return OGRERR_NONE if the operation succeeds, even if some values are
 * not transferred, otherwise an error code.
 */

OGRErr OGR_F_SetFrom( OGRFeatureH hFeat, OGRFeatureH hOtherFeat,
                      int bForgiving )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_SetFrom", OGRERR_FAILURE );
    VALIDATE_POINTER1( hOtherFeat, "OGR_F_SetFrom", OGRERR_FAILURE );

    return
        OGRFeature::FromHandle(hFeat)->
            SetFrom( OGRFeature::FromHandle(hOtherFeat),
                     bForgiving );
}

/************************************************************************/
/*                              SetFrom()                               */
/************************************************************************/

/**
 * \brief Set one feature from another.
 *
 * Overwrite the contents of this feature from the geometry and attributes
 * of another.  The poSrcFeature does not need to have the same
 * OGRFeatureDefn.  Field values are copied according to the provided indices
 * map. Field types do not have to exactly match.  SetField() method
 * conversion rules will be applied as needed. This is more efficient than
 * OGR_F_SetFrom() in that this doesn't lookup the fields by their names.
 * Particularly useful when the field names don't match.
 *
 * This method is the same as the C function OGR_F_SetFromWithMap().
 *
 * @param poSrcFeature the feature from which geometry, and field values will
 * be copied.
 *
 * @param panMap Array of the indices of the feature's fields
 * stored at the corresponding index of the source feature's fields. A value of
 * -1 should be used to ignore the source's field. The array should not be NULL
 * and be as long as the number of fields in the source feature.
 *
 * @param bForgiving TRUE if the operation should continue despite lacking
 * output fields matching some of the source fields.
 *
 * @return OGRERR_NONE if the operation succeeds, even if some values are
 * not transferred, otherwise an error code.
 */

OGRErr OGRFeature::SetFrom( const OGRFeature * poSrcFeature,
                            const int *panMap ,
                            int bForgiving )

{
    if( poSrcFeature == this )
        return OGRERR_FAILURE;

    SetFID( OGRNullFID );

/* -------------------------------------------------------------------- */
/*      Set the geometry.                                               */
/* -------------------------------------------------------------------- */
    if( GetGeomFieldCount() == 1 )
    {
        const OGRGeomFieldDefn* poGFieldDefn = GetGeomFieldDefnRef(0);

        int iSrc = poSrcFeature->GetGeomFieldIndex(
                                    poGFieldDefn->GetNameRef());
        if( iSrc >= 0 )
            SetGeomField( 0, poSrcFeature->GetGeomFieldRef(iSrc) );
        else
            // Whatever the geometry field names are.  For backward
            // compatibility.
            SetGeomField( 0, poSrcFeature->GetGeomFieldRef(0) );
    }
    else
    {
        for( int i = 0; i < GetGeomFieldCount(); i++ )
        {
            const OGRGeomFieldDefn* poGFieldDefn = GetGeomFieldDefnRef(i);

            const int iSrc =
                poSrcFeature->GetGeomFieldIndex(poGFieldDefn->GetNameRef());
            if( iSrc >= 0 )
                SetGeomField( i, poSrcFeature->GetGeomFieldRef(iSrc) );
            else
                SetGeomField( i, nullptr );
        }
    }

/* -------------------------------------------------------------------- */
/*      Copy feature style string.                                      */
/* -------------------------------------------------------------------- */
    SetStyleString( poSrcFeature->GetStyleString() );

/* -------------------------------------------------------------------- */
/*      Copy native data.                                               */
/* -------------------------------------------------------------------- */
    SetNativeData( poSrcFeature->GetNativeData() );
    SetNativeMediaType( poSrcFeature->GetNativeMediaType() );

/* -------------------------------------------------------------------- */
/*      Set the fields by name.                                         */
/* -------------------------------------------------------------------- */

    const OGRErr eErr = SetFieldsFrom( poSrcFeature, panMap, bForgiving );
    if( eErr != OGRERR_NONE )
        return eErr;

    return OGRERR_NONE;
}

/************************************************************************/
/*                      OGR_F_SetFromWithMap()                          */
/************************************************************************/

/**
 * \brief Set one feature from another.
 *
 * Overwrite the contents of this feature from the geometry and attributes
 * of another.  The hOtherFeature does not need to have the same
 * OGRFeatureDefn.  Field values are copied according to the provided indices
 * map. Field types do not have to exactly match.  OGR_F_SetField*() function
 * conversion rules will be applied as needed. This is more efficient than
 * OGR_F_SetFrom() in that this doesn't lookup the fields by their names.
 * Particularly useful when the field names don't match.
 *
 * This function is the same as the C++ method OGRFeature::SetFrom().
 *
 * @param hFeat handle to the feature to set to.
 * @param hOtherFeat handle to the feature from which geometry,
 * and field values will be copied.
 *
 * @param panMap Array of the indices of the destination feature's fields
 * stored at the corresponding index of the source feature's fields. A value of
 * -1 should be used to ignore the source's field. The array should not be NULL
 * and be as long as the number of fields in the source feature.
 *
 * @param bForgiving TRUE if the operation should continue despite lacking
 * output fields matching some of the source fields.
 *
 * @return OGRERR_NONE if the operation succeeds, even if some values are
 * not transferred, otherwise an error code.
 */

OGRErr OGR_F_SetFromWithMap( OGRFeatureH hFeat, OGRFeatureH hOtherFeat,
                             int bForgiving, const int *panMap )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_SetFrom", OGRERR_FAILURE );
    VALIDATE_POINTER1( hOtherFeat, "OGR_F_SetFrom", OGRERR_FAILURE );
    VALIDATE_POINTER1( panMap, "OGR_F_SetFrom", OGRERR_FAILURE);

    return OGRFeature::FromHandle(hFeat)->
      SetFrom( OGRFeature::FromHandle(hOtherFeat),
                 panMap, bForgiving );
}

/************************************************************************/
/*                           SetFieldsFrom()                            */
/************************************************************************/

/**
 * \brief Set fields from another feature.
 *
 * Overwrite the fields of this feature from the attributes of
 * another. The FID and the style string are not set. The poSrcFeature
 * does not need to have the same OGRFeatureDefn.  Field values are
 * copied according to the provided indices map. Field types do not
 * have to exactly match.  SetField() method conversion rules will be
 * applied as needed. This is more efficient than OGR_F_SetFrom() in
 * that this doesn't lookup the fields by their names.  Particularly
 * useful when the field names don't match.
 *
 * @param poSrcFeature the feature from which geometry, and field values will
 * be copied.
 *
 * @param panMap Array of the indices of the feature's fields
 * stored at the corresponding index of the source feature's fields. A value of
 * -1 should be used to ignore the source's field. The array should not be NULL
 * and be as long as the number of fields in the source feature.
 *
 * @param bForgiving TRUE if the operation should continue despite lacking
 * output fields matching some of the source fields.
 *
 * @return OGRERR_NONE if the operation succeeds, even if some values are
 * not transferred, otherwise an error code.
 */

OGRErr OGRFeature::SetFieldsFrom( const OGRFeature * poSrcFeature,
                                  const int *panMap,
                                  int bForgiving )

{
    const int nSrcFieldCount = poSrcFeature->poDefn->GetFieldCountUnsafe();
    const int nFieldCount = poDefn->GetFieldCountUnsafe();
    for( int iField = 0; iField < nSrcFieldCount; iField++ )
    {
        const int iDstField = panMap[iField];

        if( iDstField < 0 )
            continue;

        if( nFieldCount <= iDstField )
            return OGRERR_FAILURE;

        if( !poSrcFeature->IsFieldSetUnsafe(iField) )
        {
            UnsetField( iDstField );
            continue;
        }

        if( poSrcFeature->IsFieldNullUnsafe(iField) )
        {
            SetFieldNull( iDstField );
            continue;
        }

        const auto eSrcType = poSrcFeature->poDefn->GetFieldDefnUnsafe(iField)->GetType();
        const auto eDstType = poDefn->GetFieldDefnUnsafe(iDstField)->GetType();
        if( eSrcType == eDstType )
        {
            if( eSrcType == OFTInteger )
            {
                SetFieldSameTypeUnsafe( iDstField, poSrcFeature->GetFieldAsIntegerUnsafe( iField ) );
                continue;
            }
            if( eSrcType == OFTInteger64 )
            {
                SetFieldSameTypeUnsafe( iDstField, poSrcFeature->GetFieldAsInteger64Unsafe( iField ) );
                continue;
            }
            if( eSrcType == OFTReal )
            {
                SetFieldSameTypeUnsafe( iDstField, poSrcFeature->GetFieldAsDoubleUnsafe( iField ) );
                continue;
            }
            if( eSrcType == OFTString )
            {
                if( IsFieldSetAndNotNullUnsafe(iDstField) )
                    CPLFree( pauFields[iDstField].String );

                SetFieldSameTypeUnsafe( iDstField, VSI_STRDUP_VERBOSE(poSrcFeature->GetFieldAsStringUnsafe( iField )) );
                continue;
            }
        }

        switch( eSrcType )
        {
          case OFTInteger:
            SetField( iDstField, poSrcFeature->GetFieldAsIntegerUnsafe( iField ) );
            break;

          case OFTInteger64:
            SetField( iDstField, poSrcFeature->GetFieldAsInteger64Unsafe( iField ) );
            break;

          case OFTReal:
            SetField( iDstField, poSrcFeature->GetFieldAsDoubleUnsafe( iField ) );
            break;

          case OFTString:
            SetField( iDstField, poSrcFeature->GetFieldAsStringUnsafe( iField ) );
            break;

          case OFTIntegerList:
          {
              if( eDstType == OFTString )
              {
                  SetField( iDstField, poSrcFeature->GetFieldAsString(iField) );
              }
              else
              {
                  int nCount = 0;
                  const int *panValues =
                      poSrcFeature->GetFieldAsIntegerList( iField, &nCount);
                  SetField(iDstField, nCount, panValues);
              }
          }
          break;

          case OFTInteger64List:
          {
              if( eDstType == OFTString )
              {
                  SetField( iDstField, poSrcFeature->GetFieldAsString(iField) );
              }
              else
              {
                  int nCount = 0;
                  const GIntBig *panValues =
                      poSrcFeature->GetFieldAsInteger64List( iField, &nCount);
                  SetField( iDstField, nCount, panValues );
              }
          }
          break;

          case OFTRealList:
          {
              if( eDstType == OFTString )
              {
                  SetField( iDstField, poSrcFeature->GetFieldAsString(iField) );
              }
              else
              {
                  int nCount = 0;
                  const double *padfValues =
                      poSrcFeature->GetFieldAsDoubleList( iField, &nCount);
                  SetField(iDstField, nCount, padfValues);
              }
          }
          break;

          case OFTDate:
          case OFTDateTime:
          case OFTTime:
          {
            if( eDstType == OFTDate ||
                eDstType == OFTTime ||
                eDstType == OFTDateTime )
            {
                SetField( iDstField, const_cast<OGRField*>(
                    poSrcFeature->GetRawFieldRef( iField )) );
            }
            else if( eDstType == OFTString ||
                     eDstType == OFTStringList )
            {
                SetField( iDstField, poSrcFeature->GetFieldAsString( iField ) );
            }
            else if( !bForgiving )
                return OGRERR_FAILURE;
            break;
          }

          default:
          {
            if( eSrcType == eDstType )
            {
                SetField( iDstField, const_cast<OGRField*>(
                    poSrcFeature->GetRawFieldRef( iField )) );
            }
            else if( eDstType == OFTString ||
                     eDstType == OFTStringList )
            {
                SetField( iDstField, poSrcFeature->GetFieldAsString( iField ) );
            }
            else if( !bForgiving )
                return OGRERR_FAILURE;
            break;
          }
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                             GetStyleString()                         */
/************************************************************************/

/**
 * \brief Fetch style string for this feature.
 *
 * Set the OGR Feature Style Specification for details on the format of
 * this string, and ogr_featurestyle.h for services available to parse it.
 *
 * This method is the same as the C function OGR_F_GetStyleString().
 *
 * @return a reference to a representation in string format, or NULL if
 * there isn't one.
 */

const char *OGRFeature::GetStyleString() const
{
    if( m_pszStyleString )
        return m_pszStyleString;

    const int iStyleFieldIndex = GetFieldIndex("OGR_STYLE");
    if( iStyleFieldIndex >= 0 )
        return GetFieldAsString(iStyleFieldIndex);

    return nullptr;
}

/************************************************************************/
/*                        OGR_F_GetStyleString()                        */
/************************************************************************/

/**
 * \brief Fetch style string for this feature.
 *
 * Set the OGR Feature Style Specification for details on the format of
 * this string, and ogr_featurestyle.h for services available to parse it.
 *
 * This function is the same as the C++ method OGRFeature::GetStyleString().
 *
 * @param hFeat handle to the feature to get the style from.
 * @return a reference to a representation in string format, or NULL if
 * there isn't one.
 */

const char *OGR_F_GetStyleString( OGRFeatureH hFeat )
{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetStyleString", nullptr );

    return OGRFeature::FromHandle(hFeat)->GetStyleString();
}

/************************************************************************/
/*                             SetStyleString()                         */
/************************************************************************/

/**
 * \brief Set feature style string.
 *
 * This method operate exactly as OGRFeature::SetStyleStringDirectly() except
 * that it does not assume ownership of the passed string, but instead makes a
 * copy of it.
 *
 * This method is the same as the C function OGR_F_SetStyleString().
 *
 * @param pszString the style string to apply to this feature, cannot be NULL.
 */

void OGRFeature::SetStyleString(const char *pszString)
{
    if( m_pszStyleString )
    {
        CPLFree(m_pszStyleString);
        m_pszStyleString = nullptr;
    }

    if( pszString )
        m_pszStyleString = VSI_STRDUP_VERBOSE(pszString);
}

/************************************************************************/
/*                        OGR_F_SetStyleString()                        */
/************************************************************************/

/**
 * \brief Set feature style string.
 *
 * This method operate exactly as OGR_F_SetStyleStringDirectly() except that it
 * does not assume ownership of the passed string, but instead makes a copy of
 * it.
 *
 * This function is the same as the C++ method OGRFeature::SetStyleString().
 *
 * @param hFeat handle to the feature to set style to.
 * @param pszStyle the style string to apply to this feature, cannot be NULL.
 */

void OGR_F_SetStyleString( OGRFeatureH hFeat, const char *pszStyle )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetStyleString" );

    OGRFeature::FromHandle(hFeat)->SetStyleString(pszStyle);
}

/************************************************************************/
/*                       SetStyleStringDirectly()                       */
/************************************************************************/

/**
 * \brief Set feature style string.
 *
 * This method operate exactly as OGRFeature::SetStyleString() except that it
 * assumes ownership of the passed string.
 *
 * This method is the same as the C function OGR_F_SetStyleStringDirectly().
 *
 * @param pszString the style string to apply to this feature, cannot be NULL.
 */

void OGRFeature::SetStyleStringDirectly( char *pszString )
{
    CPLFree(m_pszStyleString);
    m_pszStyleString = pszString;
}

/************************************************************************/
/*                     OGR_F_SetStyleStringDirectly()                   */
/************************************************************************/

/**
 * \brief Set feature style string.
 *
 * This method operate exactly as OGR_F_SetStyleString() except that it assumes
 * ownership of the passed string.
 *
 * This function is the same as the C++ method
 * OGRFeature::SetStyleStringDirectly().
 *
 * @param hFeat handle to the feature to set style to.
 * @param pszStyle the style string to apply to this feature, cannot be NULL.
 */

void OGR_F_SetStyleStringDirectly( OGRFeatureH hFeat, char *pszStyle )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetStyleStringDirectly" );

    OGRFeature::FromHandle(hFeat)->SetStyleStringDirectly(pszStyle);
}

//************************************************************************/
/*                           SetStyleTable()                            */
/************************************************************************/

/** Set style table.
 * @param poStyleTable new style table (will be cloned)
 */
void OGRFeature::SetStyleTable( OGRStyleTable *poStyleTable )
{
    if( m_poStyleTable )
        delete m_poStyleTable;
    m_poStyleTable = poStyleTable ? poStyleTable->Clone() : nullptr;
}

//************************************************************************/
/*                          SetStyleTableDirectly()                      */
/************************************************************************/

/** Set style table.
 * @param poStyleTable new style table (ownership transferred to the object)
 */
void OGRFeature::SetStyleTableDirectly( OGRStyleTable *poStyleTable )
{
    if( m_poStyleTable )
        delete m_poStyleTable;
    m_poStyleTable = poStyleTable;
}

//! @cond Doxygen_Suppress

/************************************************************************/
/*                            RemapFields()                             */
/*                                                                      */
/*      This is used to transform a feature "in place" from one         */
/*      feature defn to another with minimum work.                      */
/************************************************************************/

OGRErr OGRFeature::RemapFields( OGRFeatureDefn *poNewDefn,
                                const int *panRemapSource )

{
    if( poNewDefn == nullptr )
        poNewDefn = poDefn;

    OGRField *pauNewFields = static_cast<OGRField *>(
        CPLCalloc( poNewDefn->GetFieldCount(), sizeof(OGRField) ) );

    for( int iDstField = 0; iDstField < poDefn->GetFieldCount(); iDstField++ )
    {
        if( panRemapSource[iDstField] == -1 )
        {
            OGR_RawField_SetUnset(&pauNewFields[iDstField]);
        }
        else
        {
            memcpy( pauNewFields + iDstField,
                    pauFields + panRemapSource[iDstField],
                    sizeof(OGRField) );
        }
    }

    // We really should be freeing memory for old columns that
    // are no longer present.  We don't for now because it is a bit messy
    // and would take too long to test.

/* -------------------------------------------------------------------- */
/*      Apply new definition and fields.                                */
/* -------------------------------------------------------------------- */
    CPLFree( pauFields );
    pauFields = pauNewFields;

    poDefn = poNewDefn;

    return OGRERR_NONE;
}

/************************************************************************/
/*                            AppendField()                             */
/*                                                                      */
/*      This is used to transform a feature "in place" by appending     */
/*      an unset field.                                                 */
/************************************************************************/

void OGRFeature::AppendField()
{
    int nFieldCount = poDefn->GetFieldCount();
    pauFields = static_cast<OGRField *>(CPLRealloc( pauFields,
                            nFieldCount * sizeof(OGRField) ) );
    OGR_RawField_SetUnset(&pauFields[nFieldCount-1]);
}

/************************************************************************/
/*                        RemapGeomFields()                             */
/*                                                                      */
/*      This is used to transform a feature "in place" from one         */
/*      feature defn to another with minimum work.                      */
/************************************************************************/

OGRErr OGRFeature::RemapGeomFields( OGRFeatureDefn *poNewDefn,
                                    const int *panRemapSource )

{
    if( poNewDefn == nullptr )
        poNewDefn = poDefn;

    OGRGeometry** papoNewGeomFields = static_cast<OGRGeometry **>(
        CPLCalloc( poNewDefn->GetGeomFieldCount(), sizeof(OGRGeometry*) ) );

    for( int iDstField = 0;
         iDstField < poDefn->GetGeomFieldCount();
         iDstField++ )
    {
        if( panRemapSource[iDstField] == -1 )
        {
            papoNewGeomFields[iDstField] = nullptr;
        }
        else
        {
            papoNewGeomFields[iDstField] =
                papoGeometries[panRemapSource[iDstField]];
        }
    }

    // We really should be freeing memory for old columns that
    // are no longer present.  We don't for now because it is a bit messy
    // and would take too long to test.

/* -------------------------------------------------------------------- */
/*      Apply new definition and fields.                                */
/* -------------------------------------------------------------------- */
    CPLFree( papoGeometries );
    papoGeometries = papoNewGeomFields;

    poDefn = poNewDefn;

    return OGRERR_NONE;
}
//! @endcond

/************************************************************************/
/*                         OGR_F_GetStyleTable()                        */
/************************************************************************/

OGRStyleTableH OGR_F_GetStyleTable( OGRFeatureH hFeat )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetStyleTable", nullptr );

    return reinterpret_cast<OGRStyleTableH>(
        OGRFeature::FromHandle(hFeat)->GetStyleTable());
}

/************************************************************************/
/*                         OGR_F_SetStyleTableDirectly()                */
/************************************************************************/

void OGR_F_SetStyleTableDirectly( OGRFeatureH hFeat,
                                  OGRStyleTableH hStyleTable )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetStyleTableDirectly" );

    OGRFeature::FromHandle(hFeat)->
        SetStyleTableDirectly(reinterpret_cast<OGRStyleTable *>(hStyleTable));
}

/************************************************************************/
/*                         OGR_F_SetStyleTable()                        */
/************************************************************************/

void OGR_F_SetStyleTable( OGRFeatureH hFeat,
                          OGRStyleTableH hStyleTable )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetStyleTable" );
    VALIDATE_POINTER0( hStyleTable, "OGR_F_SetStyleTable" );

    OGRFeature::FromHandle(hFeat)->
        SetStyleTable(reinterpret_cast<OGRStyleTable *>(hStyleTable));
}

/************************************************************************/
/*                          FillUnsetWithDefault()                      */
/************************************************************************/

/**
 * \brief Fill unset fields with default values that might be defined.
 *
 * This method is the same as the C function OGR_F_FillUnsetWithDefault().
 *
 * @param bNotNullableOnly if we should fill only unset fields with a not-null
 *                     constraint.
 * @param papszOptions unused currently. Must be set to NULL.
 * @since GDAL 2.0
 */

void OGRFeature::FillUnsetWithDefault( int bNotNullableOnly,
                                       CPL_UNUSED char** papszOptions)
{
    const int nFieldCount = poDefn->GetFieldCount();
    for( int i = 0; i < nFieldCount; i++ )
    {
        if( IsFieldSet(i) )
            continue;
        if( bNotNullableOnly && poDefn->GetFieldDefn(i)->IsNullable() )
            continue;
        const char* pszDefault = poDefn->GetFieldDefn(i)->GetDefault();
        OGRFieldType eType = poDefn->GetFieldDefn(i)->GetType();
        if( pszDefault != nullptr )
        {
            if( eType == OFTDate || eType == OFTTime || eType == OFTDateTime )
            {
                if( STARTS_WITH_CI(pszDefault, "CURRENT") )
                {
                    time_t t = time(nullptr);
                    struct tm brokendown;
                    CPLUnixTimeToYMDHMS(t, &brokendown);
                    SetField(i, brokendown.tm_year + 1900,
                                brokendown.tm_mon + 1,
                                brokendown.tm_mday,
                                brokendown.tm_hour,
                                brokendown.tm_min,
                                static_cast<float>(brokendown.tm_sec),
                                100 );
                }
                else
                {
                    int nYear = 0;
                    int nMonth = 0;
                    int nDay = 0;
                    int nHour = 0;
                    int nMinute = 0;
                    float fSecond = 0.0f;
                    if( sscanf(pszDefault, "'%d/%d/%d %d:%d:%f'",
                               &nYear, &nMonth, &nDay,
                               &nHour, &nMinute, &fSecond) == 6 )
                    {
                        SetField(i, nYear, nMonth, nDay, nHour, nMinute,
                                 fSecond, 100 );
                    }
                }
            }
            else if( eType == OFTString &&
                     pszDefault[0] == '\'' &&
                     pszDefault[strlen(pszDefault)-1] == '\'' )
            {
                CPLString osDefault(pszDefault + 1);
                osDefault.resize(osDefault.size()-1);
                char* pszTmp = CPLUnescapeString(osDefault, nullptr, CPLES_SQL);
                SetField(i, pszTmp);
                CPLFree(pszTmp);
            }
            else
                SetField(i, pszDefault);
        }
    }
}

/************************************************************************/
/*                     OGR_F_FillUnsetWithDefault()                     */
/************************************************************************/

/**
 * \brief Fill unset fields with default values that might be defined.
 *
 * This function is the same as the C++ method
 * OGRFeature::FillUnsetWithDefault().
 *
 * @param hFeat handle to the feature.
 * @param bNotNullableOnly if we should fill only unset fields with a not-null
 *                     constraint.
 * @param papszOptions unused currently. Must be set to NULL.
 * @since GDAL 2.0
 */

void OGR_F_FillUnsetWithDefault( OGRFeatureH hFeat,
                                 int bNotNullableOnly,
                                 char** papszOptions )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_FillUnsetWithDefault" );

    OGRFeature::FromHandle(hFeat)->
        FillUnsetWithDefault( bNotNullableOnly, papszOptions );
}

/************************************************************************/
/*                              Validate()                              */
/************************************************************************/

/**
 * \brief Validate that a feature meets constraints of its schema.
 *
 * The scope of test is specified with the nValidateFlags parameter.
 *
 * Regarding OGR_F_VAL_WIDTH, the test is done assuming the string width must be
 * interpreted as the number of UTF-8 characters. Some drivers might interpret
 * the width as the number of bytes instead. So this test is rather conservative
 * (if it fails, then it will fail for all interpretations).
 *
 * This method is the same as the C function OGR_F_Validate().
 *
 * @param nValidateFlags OGR_F_VAL_ALL or combination of OGR_F_VAL_NULL,
 *                       OGR_F_VAL_GEOM_TYPE, OGR_F_VAL_WIDTH and
 *                       OGR_F_VAL_ALLOW_NULL_WHEN_DEFAULT,
 *                       OGR_F_VAL_ALLOW_DIFFERENT_GEOM_DIM with '|' operator
 * @param bEmitError TRUE if a CPLError() must be emitted when a check fails
 * @return TRUE if all enabled validation tests pass.
 * @since GDAL 2.0
 */

int OGRFeature::Validate( int nValidateFlags, int bEmitError ) const

{
    bool bRet = true;

    const int nGeomFieldCount = poDefn->GetGeomFieldCount();
    for( int i = 0; i < nGeomFieldCount; i++ )
    {
        if( (nValidateFlags & OGR_F_VAL_NULL) &&
            !poDefn->GetGeomFieldDefn(i)->IsNullable() &&
            GetGeomFieldRef(i) == nullptr )
        {
            bRet = false;
            if( bEmitError )
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Geometry field %s has a NULL content which is not allowed",
                    poDefn->GetGeomFieldDefn(i)->GetNameRef());
            }
        }
        if( (nValidateFlags & OGR_F_VAL_GEOM_TYPE) &&
            poDefn->GetGeomFieldDefn(i)->GetType() != wkbUnknown )
        {
            const OGRGeometry* poGeom = GetGeomFieldRef(i);
            if( poGeom != nullptr )
            {
                const OGRwkbGeometryType eType =
                    poDefn->GetGeomFieldDefn(i)->GetType();
                const OGRwkbGeometryType eFType = poGeom->getGeometryType();
                if( (nValidateFlags & OGR_F_VAL_ALLOW_DIFFERENT_GEOM_DIM) &&
                    (wkbFlatten(eFType) == wkbFlatten(eType) ||
                     wkbFlatten(eType) == wkbUnknown) )
                {
                    // Ok.
                }
                else if( (eType == wkbSetZ(wkbUnknown) && !wkbHasZ(eFType)) ||
                         (eType != wkbSetZ(wkbUnknown) && eFType != eType) )
                {
                    bRet = false;
                    if( bEmitError )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Geometry field %s has a %s geometry whereas "
                                 "%s is expected",
                                 poDefn->GetGeomFieldDefn(i)->GetNameRef(),
                                 OGRGeometryTypeToName(eFType),
                                 OGRGeometryTypeToName(eType));
                    }
                }
            }
        }
    }
    const int nFieldCount = poDefn->GetFieldCount();
    for( int i = 0; i < nFieldCount; i++ )
    {
        if( (nValidateFlags & OGR_F_VAL_NULL) &&
            !poDefn->GetFieldDefn(i)->IsNullable() &&
            !IsFieldSet(i) &&
            (!(nValidateFlags & OGR_F_VAL_ALLOW_NULL_WHEN_DEFAULT) ||
               poDefn->GetFieldDefn(i)->GetDefault() == nullptr))
        {
            bRet = false;
            if( bEmitError )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Field %s has a NULL content which is not allowed",
                         poDefn->GetFieldDefn(i)->GetNameRef());
            }
        }
        if( (nValidateFlags & OGR_F_VAL_WIDTH) &&
            poDefn->GetFieldDefn(i)->GetWidth() > 0 &&
            poDefn->GetFieldDefn(i)->GetType() == OFTString &&
            IsFieldSet(i) &&
            CPLIsUTF8(GetFieldAsString(i), -1) &&
            CPLStrlenUTF8(GetFieldAsString(i)) >
            poDefn->GetFieldDefn(i)->GetWidth())
        {
            bRet = false;
            if( bEmitError )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Field %s has a %d UTF-8 characters whereas "
                         "a maximum of %d is allowed",
                         poDefn->GetFieldDefn(i)->GetNameRef(),
                         CPLStrlenUTF8(GetFieldAsString(i)),
                         poDefn->GetFieldDefn(i)->GetWidth());
            }
        }
    }

    return bRet;
}

/************************************************************************/
/*                           OGR_F_Validate()                           */
/************************************************************************/

/**
 * \brief Validate that a feature meets constraints of its schema.
 *
 * The scope of test is specified with the nValidateFlags parameter.
 *
 * Regarding OGR_F_VAL_WIDTH, the test is done assuming the string width must be
 * interpreted as the number of UTF-8 characters. Some drivers might interpret
 * the width as the number of bytes instead. So this test is rather conservative
 * (if it fails, then it will fail for all interpretations).
 *
 * This function is the same as the C++ method
 * OGRFeature::Validate().
 *
 * @param hFeat handle to the feature to validate.
 * @param nValidateFlags OGR_F_VAL_ALL or combination of OGR_F_VAL_NULL,
 *                       OGR_F_VAL_GEOM_TYPE, OGR_F_VAL_WIDTH and
 *                       OGR_F_VAL_ALLOW_NULL_WHEN_DEFAULT with '|' operator
 * @param bEmitError TRUE if a CPLError() must be emitted when a check fails
 * @return TRUE if all enabled validation tests pass.
 * @since GDAL 2.0
 */

int OGR_F_Validate( OGRFeatureH hFeat, int nValidateFlags, int bEmitError )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_Validate", FALSE );

    return OGRFeature::FromHandle(hFeat)->
        Validate( nValidateFlags, bEmitError );
}

/************************************************************************/
/*                          GetNativeData()                             */
/************************************************************************/

/**
 * \fn const char* OGRFeature::GetNativeData() const;
 *
 * \brief Returns the native data for the feature.
 *
 * The native data is the representation in a "natural" form that comes from
 * the driver that created this feature, or that is aimed at an output driver.
 * The native data may be in different format, which is indicated by
 * GetNativeMediaType().
 *
 * Note that most drivers do not support storing the native data in the feature
 * object, and if they do, generally the NATIVE_DATA open option must be passed
 * at dataset opening.
 *
 * The "native data" does not imply it is something more performant or powerful
 * than what can be obtained with the rest of the API, but it may be useful in
 * round-tripping scenarios where some characteristics of the underlying format
 * are not captured otherwise by the OGR abstraction.
 *
 * This function is the same as the C function
 * OGR_F_GetNativeData().
 *
 * @return a string with the native data, or NULL if there is none.
 * @since GDAL 2.1
 *
 * @see https://trac.osgeo.org/gdal/wiki/rfc60_improved_roundtripping_in_ogr
 */

/************************************************************************/
/*                         OGR_F_GetNativeData()                        */
/************************************************************************/

/**
 * \brief Returns the native data for the feature.
 *
 * The native data is the representation in a "natural" form that comes from
 * the driver that created this feature, or that is aimed at an output driver.
 * The native data may be in different format, which is indicated by
 * OGR_F_GetNativeMediaType().
 *
 * Note that most drivers do not support storing the native data in the feature
 * object, and if they do, generally the NATIVE_DATA open option must be passed
 * at dataset opening.
 *
 * The "native data" does not imply it is something more performant or powerful
 * than what can be obtained with the rest of the API, but it may be useful in
 * round-tripping scenarios where some characteristics of the underlying format
 * are not captured otherwise by the OGR abstraction.
 *
 * This function is the same as the C++ method
 * OGRFeature::GetNativeData().
 *
 * @param hFeat handle to the feature.
 * @return a string with the native data, or NULL if there is none.
 * @since GDAL 2.1
 *
 * @see https://trac.osgeo.org/gdal/wiki/rfc60_improved_roundtripping_in_ogr
 */

const char *OGR_F_GetNativeData( OGRFeatureH hFeat )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetNativeData", nullptr );

    return OGRFeature::FromHandle(hFeat)->GetNativeData();
}

/************************************************************************/
/*                        GetNativeMediaType()                          */
/************************************************************************/

/**
 * \fn const char* OGRFeature::GetNativeMediaType() const;
 *
 * \brief Returns the native media type for the feature.
 *
 * The native media type is the identifier for the format of the native data.
 * It follows the IANA RFC 2045 (see https://en.wikipedia.org/wiki/Media_type),
 * e.g. "application/vnd.geo+json" for JSon.
 *
 * This function is the same as the C function
 * OGR_F_GetNativeMediaType().
 *
 * @return a string with the native media type, or NULL if there is none.
 * @since GDAL 2.1
 *
 * @see https://trac.osgeo.org/gdal/wiki/rfc60_improved_roundtripping_in_ogr
 */

/************************************************************************/
/*                       OGR_F_GetNativeMediaType()                     */
/************************************************************************/

/**
 * \brief Returns the native media type for the feature.
 *
 * The native media type is the identifier for the format of the native data.
 * It follows the IANA RFC 2045 (see https://en.wikipedia.org/wiki/Media_type),
 * e.g. "application/vnd.geo+json" for JSon.
 *
 * This function is the same as the C function
 * OGR_F_GetNativeMediaType().
 *
 * @param hFeat handle to the feature.
 * @return a string with the native media type, or NULL if there is none.
 * @since GDAL 2.1
 *
 * @see https://trac.osgeo.org/gdal/wiki/rfc60_improved_roundtripping_in_ogr
 */

const char *OGR_F_GetNativeMediaType( OGRFeatureH hFeat )
{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetNativeMediaType", nullptr );

    return OGRFeature::FromHandle(hFeat)->GetNativeMediaType();
}

/************************************************************************/
/*                           SetNativeData()                            */
/************************************************************************/

/**
 * \brief Sets the native data for the feature.
 *
 * The native data is the representation in a "natural" form that comes from
 * the driver that created this feature, or that is aimed at an output driver.
 * The native data may be in different format, which is indicated by
 * GetNativeMediaType().
 *
 * This function is the same as the C function
 * OGR_F_SetNativeData().
 *
 * @param pszNativeData a string with the native data, or NULL if there is none.
 * @since GDAL 2.1
 *
 * @see https://trac.osgeo.org/gdal/wiki/rfc60_improved_roundtripping_in_ogr
 */

void OGRFeature::SetNativeData( const char* pszNativeData )
{
    CPLFree( m_pszNativeData );
    m_pszNativeData = pszNativeData ? VSI_STRDUP_VERBOSE(pszNativeData) : nullptr;
}

/************************************************************************/
/*                          OGR_F_SetNativeData()                       */
/************************************************************************/

/**
 * \brief Sets the native data for the feature.
 *
 * The native data is the representation in a "natural" form that comes from
 * the driver that created this feature, or that is aimed at an output driver.
 * The native data may be in different format, which is indicated by
 * OGR_F_GetNativeMediaType().
 *
 * This function is the same as the C++ method
 * OGRFeature::SetNativeData().
 *
 * @param hFeat handle to the feature.
 * @param pszNativeData a string with the native data, or NULL if there is none.
 * @since GDAL 2.1
 *
 * @see https://trac.osgeo.org/gdal/wiki/rfc60_improved_roundtripping_in_ogr
 */

void OGR_F_SetNativeData( OGRFeatureH hFeat, const char* pszNativeData )
{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetNativeData" );

    OGRFeature::FromHandle(hFeat)->SetNativeData(pszNativeData);
}

/************************************************************************/
/*                         SetNativeMediaType()                         */
/************************************************************************/

/**
 * \brief Sets the native media type for the feature.
 *
 * The native media type is the identifier for the format of the native data.
 * It follows the IANA RFC 2045 (see https://en.wikipedia.org/wiki/Media_type),
 * e.g. "application/vnd.geo+json" for JSon.
 *
 * This function is the same as the C function
 * OGR_F_SetNativeMediaType().
 *
 * @param pszNativeMediaType a string with the native media type, or NULL if
 * there is none.
 * @since GDAL 2.1
 *
 * @see https://trac.osgeo.org/gdal/wiki/rfc60_improved_roundtripping_in_ogr
 */

void OGRFeature::SetNativeMediaType( const char* pszNativeMediaType )
{
    CPLFree( m_pszNativeMediaType );
    m_pszNativeMediaType =
        pszNativeMediaType ? VSI_STRDUP_VERBOSE(pszNativeMediaType) : nullptr;
}

/************************************************************************/
/*                          OGR_F_SetNativeMediaType()                  */
/************************************************************************/

/**
 * \brief Sets the native media type for the feature.
 *
 * The native media type is the identifier for the format of the native data.
 * It follows the IANA RFC 2045 (see https://en.wikipedia.org/wiki/Media_type),
 * e.g. "application/vnd.geo+json" for JSon.
 *
 * This function is the same as the C++ method
 * OGRFeature::SetNativeMediaType().
 *
 * @param hFeat handle to the feature.
 * @param pszNativeMediaType a string with the native media type, or NULL if
 * there is none.
 * @since GDAL 2.1
 *
 * @see https://trac.osgeo.org/gdal/wiki/rfc60_improved_roundtripping_in_ogr
 */

void OGR_F_SetNativeMediaType( OGRFeatureH hFeat,
                               const char* pszNativeMediaType )
{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetNativeMediaType" );

    OGRFeature::FromHandle(hFeat)->
        SetNativeMediaType(pszNativeMediaType);
}

/************************************************************************/
/*                           OGR_RawField_IsUnset()                     */
/************************************************************************/

/**
 * \brief Returns whether a raw field is unset.
 *
 * Note: this function is rather low-level and should be rarely used in client
 * code. Use instead OGR_F_IsFieldSet().
 *
 * @param puField pointer to raw field.
 * @since GDAL 2.2
 */

int OGR_RawField_IsUnset( const OGRField* puField )
{
    return puField->Set.nMarker1 == OGRUnsetMarker &&
           puField->Set.nMarker2 == OGRUnsetMarker &&
           puField->Set.nMarker3 == OGRUnsetMarker;
}

/************************************************************************/
/*                           OGR_RawField_IsNull()                      */
/************************************************************************/

/**
 * \brief Returns whether a raw field is null.
 *
 * Note: this function is rather low-level and should be rarely used in client
 * code. Use instead OGR_F_IsFieldNull().
 *
 * @param puField pointer to raw field.
 * @since GDAL 2.2
 */

int OGR_RawField_IsNull( const OGRField* puField )
{
    return puField->Set.nMarker1 == OGRNullMarker &&
           puField->Set.nMarker2 == OGRNullMarker &&
           puField->Set.nMarker3 == OGRNullMarker;
}

/************************************************************************/
/*                          OGR_RawField_SetUnset()                     */
/************************************************************************/

/**
 * \brief Mark a raw field as unset.
 *
 * This should be called on a un-initialized field. In particular this will not
 * free any memory dynamically allocated.
 *
 * Note: this function is rather low-level and should be rarely used in client
 * code. Use instead OGR_F_UnsetField().
 *
 * @param puField pointer to raw field.
 * @since GDAL 2.2
 */

void OGR_RawField_SetUnset( OGRField* puField )
{
    puField->Set.nMarker1 = OGRUnsetMarker;
    puField->Set.nMarker2 = OGRUnsetMarker;
    puField->Set.nMarker3 = OGRUnsetMarker;
}

/************************************************************************/
/*                          OGR_RawField_SetNull()                      */
/************************************************************************/

/**
 * \brief Mark a raw field as null.
 *
 * This should be called on a un-initialized field. In particular this will not
 * free any memory dynamically allocated.
 *
 * Note: this function is rather low-level and should be rarely used in client
 * code. Use instead OGR_F_SetFieldNull().
 *
 * @param puField pointer to raw field.
 * @since GDAL 2.2
 */

void OGR_RawField_SetNull( OGRField* puField )
{
    puField->Set.nMarker1 = OGRNullMarker;
    puField->Set.nMarker2 = OGRNullMarker;
    puField->Set.nMarker3 = OGRNullMarker;
}

/************************************************************************/
/*                     OGRFeatureUniquePtrDeleter                       */
/************************************************************************/

//! @cond Doxygen_Suppress
void OGRFeatureUniquePtrDeleter::operator()(OGRFeature* poFeature) const
{
    delete poFeature;
}
//! @endcond

/************************************************************************/
/*                    OGRFeature::ConstFieldIterator                    */
/************************************************************************/

struct OGRFeature::FieldValue::Private
{
    CPL_DISALLOW_COPY_ASSIGN(Private)

    OGRFeature* m_poSelf = nullptr;
    int m_nPos = 0;
    mutable std::vector<int> m_anList{};
    mutable std::vector<GIntBig> m_anList64{};
    mutable std::vector<double> m_adfList{};
    mutable std::vector<std::string> m_aosList{};

    Private(const OGRFeature* poSelf, int iFieldIndex):
        m_poSelf(const_cast<OGRFeature*>(poSelf)), m_nPos(iFieldIndex)
    {}
    Private(OGRFeature* poSelf, int iFieldIndex):
        m_poSelf(poSelf), m_nPos(iFieldIndex)
    {}
};

struct OGRFeature::ConstFieldIterator::Private
{
    CPL_DISALLOW_COPY_ASSIGN(Private)

    OGRFeature::FieldValue m_oValue;
    int m_nPos = 0;

    Private(const OGRFeature* poSelf, int iFieldIndex):
        m_oValue(poSelf, iFieldIndex)
    {}
};

//! @cond Doxygen_Suppress
OGRFeature::ConstFieldIterator::ConstFieldIterator(const OGRFeature* poSelf, int nPos):
    m_poPrivate(new Private(poSelf, nPos))
{
    m_poPrivate->m_nPos = nPos;
}

OGRFeature::ConstFieldIterator::~ConstFieldIterator() = default;

const OGRFeature::FieldValue& OGRFeature::ConstFieldIterator::operator*() const
{
    return m_poPrivate->m_oValue;
}

OGRFeature::ConstFieldIterator& OGRFeature::ConstFieldIterator::operator++()
{
    ++m_poPrivate->m_nPos;
    m_poPrivate->m_oValue.m_poPrivate->m_nPos = m_poPrivate->m_nPos;
    return *this;
}

bool OGRFeature::ConstFieldIterator::operator!=(const ConstFieldIterator& it) const
{
    return m_poPrivate->m_nPos != it.m_poPrivate->m_nPos;
}
//! @endcond

OGRFeature::ConstFieldIterator OGRFeature::begin() const
{
    return {this, 0};
}

OGRFeature::ConstFieldIterator OGRFeature::end() const
{
    return {this, GetFieldCount()};
}

/************************************************************************/
/*                      OGRFeature::FieldValue                          */
/************************************************************************/

OGRFeature::FieldValue::FieldValue(const OGRFeature* poFeature,
                                   int iFieldIndex) :
    m_poPrivate(new Private(poFeature, iFieldIndex))
{
}

OGRFeature::FieldValue::FieldValue(OGRFeature* poFeature,
                                   int iFieldIndex) :
    m_poPrivate(new Private(poFeature, iFieldIndex))
{
}

OGRFeature::FieldValue& OGRFeature::FieldValue::operator=
                                                    (const FieldValue& oOther)
{
    if( &oOther != this &&
        !( m_poPrivate->m_poSelf == oOther.m_poPrivate->m_poSelf &&
           m_poPrivate->m_nPos == oOther.m_poPrivate->m_nPos) )
    {
        OGRFieldType eOtherType(oOther.GetType());
        if( oOther.IsNull() )
            SetNull();
        else if( oOther.IsUnset() )
            Unset();
        else if( eOtherType == OFTInteger )
            m_poPrivate->m_poSelf->SetField(m_poPrivate->m_nPos,
                                            oOther.GetInteger());
        else if( eOtherType == OFTInteger64 )
            m_poPrivate->m_poSelf->SetField(m_poPrivate->m_nPos,
                                            oOther.GetInteger64());
        else if( eOtherType == OFTReal )
            m_poPrivate->m_poSelf->SetField(m_poPrivate->m_nPos,
                                            oOther.GetDouble());
        else if( eOtherType == OFTString )
            m_poPrivate->m_poSelf->SetField(m_poPrivate->m_nPos,
                                            oOther.GetString());
        else if( eOtherType == OFTDate ||
                 eOtherType == OFTDateTime ||
                 eOtherType == OFTTime )
        {
            int nYear = 0;
            int nMonth = 0;
            int nDay = 0;
            int nHour = 0;
            int nMinute = 0;
            float fSecond = 0.0f;
            int nTZFlag = 0;
            oOther.GetDateTime(&nYear, &nMonth, &nDay, &nHour, &nMinute,
                               &fSecond, &nTZFlag);
            m_poPrivate->m_poSelf->SetField(m_poPrivate->m_nPos,
                                            nYear, nMonth, nDay,
                                            nHour, nMinute, fSecond, nTZFlag);
        }
        else if( eOtherType == OFTStringList )
        {
            m_poPrivate->m_poSelf->SetField(m_poPrivate->m_nPos,
                oOther.m_poPrivate->m_poSelf->GetFieldAsStringList(
                    oOther.m_poPrivate->m_nPos));
        }
        else if( eOtherType == OFTIntegerList )
        {
            return operator= (oOther.GetAsIntegerList());
        }
        else if( eOtherType == OFTInteger64List )
        {
            return operator= (oOther.GetAsInteger64List());
        }
        else if( eOtherType == OFTRealList )
        {
            return operator= (oOther.GetAsDoubleList());
        }
    }
    return *this;
}

OGRFeature::FieldValue& OGRFeature::FieldValue::operator= (int nVal)
{
    m_poPrivate->m_poSelf->SetField(m_poPrivate->m_nPos, nVal);
    return *this;
}

OGRFeature::FieldValue& OGRFeature::FieldValue::operator= (GIntBig nVal)
{
    m_poPrivate->m_poSelf->SetField(m_poPrivate->m_nPos, nVal);
    return *this;
}

OGRFeature::FieldValue& OGRFeature::FieldValue::operator= (double dfVal)
{
    m_poPrivate->m_poSelf->SetField(m_poPrivate->m_nPos, dfVal);
    return *this;
}

OGRFeature::FieldValue& OGRFeature::FieldValue::operator= (const char* pszVal)
{
    m_poPrivate->m_poSelf->SetField(m_poPrivate->m_nPos, pszVal);
    return *this;
}

OGRFeature::FieldValue& OGRFeature::FieldValue::operator= (const std::string& osVal)
{
    m_poPrivate->m_poSelf->SetField(m_poPrivate->m_nPos, osVal.c_str());
    return *this;
}

OGRFeature::FieldValue& OGRFeature::FieldValue::operator= (
                                            const std::vector<int>& oArray)
{
    m_poPrivate->m_poSelf->SetField(m_poPrivate->m_nPos,
                                    static_cast<int>(oArray.size()),
                                    oArray.empty() ? nullptr : oArray.data());
    return *this;
}

OGRFeature::FieldValue& OGRFeature::FieldValue::operator= (
                                        const std::vector<GIntBig>& oArray)
{
    m_poPrivate->m_poSelf->SetField(m_poPrivate->m_nPos,
                                    static_cast<int>(oArray.size()),
                                    oArray.empty() ? nullptr : oArray.data());
    return *this;
}

OGRFeature::FieldValue& OGRFeature::FieldValue::operator= (
                                    const std::vector<double>& oArray)
{
    m_poPrivate->m_poSelf->SetField(m_poPrivate->m_nPos,
                                    static_cast<int>(oArray.size()),
                                    oArray.empty() ? nullptr : oArray.data());
    return *this;
}

OGRFeature::FieldValue& OGRFeature::FieldValue::operator= (
                                        const std::vector<std::string>& oArray)
{
    CPLStringList aosList;
    for( auto&& oStr: oArray )
        aosList.AddString(oStr.c_str());
    m_poPrivate->m_poSelf->SetField(m_poPrivate->m_nPos,
                                    aosList.List());
    return *this;
}

OGRFeature::FieldValue& OGRFeature::FieldValue::operator= (
                                        CSLConstList papszValues)
{
    m_poPrivate->m_poSelf->SetField(m_poPrivate->m_nPos, papszValues);
    return *this;
}

void OGRFeature::FieldValue::SetDateTime(int nYear, int nMonth, int nDay,
                                         int nHour, int nMinute, float fSecond,
                                         int nTZFlag)
{
    m_poPrivate->m_poSelf->SetField(m_poPrivate->m_nPos,
                                    nYear, nMonth, nDay,
                                    nHour, nMinute, fSecond, nTZFlag);
}


void OGRFeature::FieldValue::SetNull()
{
    m_poPrivate->m_poSelf->SetFieldNull(m_poPrivate->m_nPos);
}

void OGRFeature::FieldValue::clear()
{
    m_poPrivate->m_poSelf->UnsetField(m_poPrivate->m_nPos);
}

//! @cond Doxygen_Suppress
OGRFeature::FieldValue::~FieldValue() = default;

//! @endcond

int OGRFeature::FieldValue::GetIndex() const
{
    return m_poPrivate->m_nPos ;
}

const OGRFieldDefn* OGRFeature::FieldValue::GetDefn() const
{
    return m_poPrivate->m_poSelf->GetFieldDefnRef(GetIndex());
}

const OGRField* OGRFeature::FieldValue::GetRawValue() const
{
    return &(m_poPrivate->m_poSelf->pauFields[GetIndex()]);
}

bool OGRFeature::FieldValue::IsUnset() const
{
    return CPL_TO_BOOL(OGR_RawField_IsUnset(GetRawValue()));
}

bool OGRFeature::FieldValue::IsNull() const
{
    return CPL_TO_BOOL(OGR_RawField_IsNull(GetRawValue()));
}

int OGRFeature::FieldValue::GetAsInteger() const
{
    return const_cast<OGRFeature*>(m_poPrivate->m_poSelf)->
                GetFieldAsInteger(GetIndex());
}

GIntBig OGRFeature::FieldValue::GetAsInteger64() const
{
    return const_cast<OGRFeature*>(m_poPrivate->m_poSelf)->
                GetFieldAsInteger64(GetIndex());
}

double OGRFeature::FieldValue::GetAsDouble() const
{
    return const_cast<OGRFeature*>(m_poPrivate->m_poSelf)->
                GetFieldAsDouble(GetIndex());
}

const char* OGRFeature::FieldValue::GetAsString() const
{
    return const_cast<OGRFeature*>(m_poPrivate->m_poSelf)->
                GetFieldAsString(GetIndex());
}

bool OGRFeature::FieldValue::GetDateTime( int *pnYear, int *pnMonth,
                                int *pnDay,
                                int *pnHour, int *pnMinute,
                                float *pfSecond,
                                int *pnTZFlag ) const
{
    return CPL_TO_BOOL(const_cast<OGRFeature*>(m_poPrivate->m_poSelf)->
        GetFieldAsDateTime(GetIndex(), pnYear, pnMonth, pnDay,
                           pnHour, pnMinute, pfSecond, pnTZFlag));
}

const std::vector<int>& OGRFeature::FieldValue::GetAsIntegerList() const
{
    int nCount = 0;
    auto&& panList = const_cast<OGRFeature*>(m_poPrivate->m_poSelf)->
                                GetFieldAsIntegerList( GetIndex(), &nCount );
    m_poPrivate->m_anList.assign( panList, panList + nCount );
    return m_poPrivate->m_anList;
}

const std::vector<GIntBig>& OGRFeature::FieldValue::GetAsInteger64List() const
{
    int nCount = 0;
    auto&& panList = const_cast<OGRFeature*>(m_poPrivate->m_poSelf)->
                                GetFieldAsInteger64List( GetIndex(), &nCount );
    m_poPrivate->m_anList64.assign( panList, panList + nCount );
    return m_poPrivate->m_anList64;
}

const std::vector<double>& OGRFeature::FieldValue::GetAsDoubleList() const
{
    int nCount = 0;
    auto&& panList = const_cast<OGRFeature*>(m_poPrivate->m_poSelf)->
                                GetFieldAsDoubleList( GetIndex(), &nCount );
    m_poPrivate->m_adfList.assign( panList, panList + nCount );
    return m_poPrivate->m_adfList;
}

const std::vector<std::string>& OGRFeature::FieldValue::GetAsStringList() const
{
    auto&& papszList = const_cast<OGRFeature*>(m_poPrivate->m_poSelf)->
                                GetFieldAsStringList( GetIndex() );
    m_poPrivate->m_aosList.clear();
    if( papszList )
    {
        for( char** papszIter = papszList; *papszIter; ++papszIter )
        {
            m_poPrivate->m_aosList.emplace_back(*papszIter);
        }
    }
    return m_poPrivate->m_aosList;
}

OGRFeature::FieldValue::operator CSLConstList() const
{
    return const_cast<CSLConstList>(
        const_cast<OGRFeature*>(m_poPrivate->m_poSelf)->
                                GetFieldAsStringList( GetIndex() ));
}
