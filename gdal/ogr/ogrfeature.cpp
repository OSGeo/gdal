/******************************************************************************
 * $Id$
 * 
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRFeature class implementation. 
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
 ****************************************************************************/

#include "ogr_feature.h"
#include "ogr_api.h"
#include "ogr_p.h"
#include <vector>

CPL_CVSID("$Id$");

/************************************************************************/
/*                             OGRFeature()                             */
/************************************************************************/

/**
 * \brief Constructor
 *
 * Note that the OGRFeature will increment the reference count of it's
 * defining OGRFeatureDefn.  Destruction of the OGRFeatureDefn before
 * destruction of all OGRFeatures that depend on it is likely to result in
 * a crash. 
 *
 * This method is the same as the C function OGR_F_Create().
 *
 * @param poDefnIn feature class (layer) definition to which the feature will
 * adhere.
 */

OGRFeature::OGRFeature( OGRFeatureDefn * poDefnIn )

{
    m_pszStyleString = NULL;
    m_poStyleTable = NULL;
    m_pszTmpFieldValue = NULL;
    poDefnIn->Reference();
    poDefn = poDefnIn;

    nFID = OGRNullFID;
    
    poGeometry = NULL;

    // we should likely be initializing from the defaults, but this will
    // usually be a waste. 
    pauFields = (OGRField *) CPLCalloc( poDefn->GetFieldCount(),
                                        sizeof(OGRField) );

    for( int i = 0; i < poDefn->GetFieldCount(); i++ )
    {
        pauFields[i].Set.nMarker1 = OGRUnsetMarker;
        pauFields[i].Set.nMarker2 = OGRUnsetMarker;
    }
}

/************************************************************************/
/*                            OGR_F_Create()                            */
/************************************************************************/
/**
 * \brief Feature factory.
 *
 * Note that the OGRFeature will increment the reference count of it's
 * defining OGRFeatureDefn.  Destruction of the OGRFeatureDefn before
 * destruction of all OGRFeatures that depend on it is likely to result in
 * a crash. 
 *
 * This function is the same as the C++ method OGRFeature::OGRFeature().
 * 
 * @param hDefn handle to the feature class (layer) definition to 
 * which the feature will adhere.
 * 
 * @return an handle to the new feature object with null fields and 
 * no geometry.
 */

OGRFeatureH OGR_F_Create( OGRFeatureDefnH hDefn )

{
    VALIDATE_POINTER1( hDefn, "OGR_F_Create", NULL );

    return (OGRFeatureH) new OGRFeature( (OGRFeatureDefn *) hDefn );
}

/************************************************************************/
/*                            ~OGRFeature()                             */
/************************************************************************/

OGRFeature::~OGRFeature()

{
    if( poGeometry != NULL )
        delete poGeometry;

    for( int i = 0; i < poDefn->GetFieldCount(); i++ )
    {
        OGRFieldDefn    *poFDefn = poDefn->GetFieldDefn(i);
        
        if( !IsFieldSet(i) )
            continue;
    
        switch( poFDefn->GetType() )
        {
          case OFTString:
            if( pauFields[i].String != NULL )
                VSIFree( pauFields[i].String );
            break;

          case OFTBinary:
            if( pauFields[i].Binary.paData != NULL )
                VSIFree( pauFields[i].Binary.paData );
            break;

          case OFTStringList:
            CSLDestroy( pauFields[i].StringList.paList );
            break;

          case OFTIntegerList:
          case OFTRealList:
            CPLFree( pauFields[i].IntegerList.paList );
            break;

          default:
            // should add support for wide strings.
            break;
        }
    }
    
    poDefn->Release();

    CPLFree( pauFields );
    CPLFree(m_pszStyleString);
    CPLFree(m_pszTmpFieldValue);
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
    delete (OGRFeature *) hFeat;
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
 * @return new feature object with null fields and no geometry.  May be
 * deleted with delete. 
 */

OGRFeature *OGRFeature::CreateFeature( OGRFeatureDefn *poDefn )

{
    return new OGRFeature( poDefn );
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
 * @return an handle to the feature definition object on which feature
 * depends.
 */

OGRFeatureDefnH OGR_F_GetDefnRef( OGRFeatureH hFeat )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetDefnRef", NULL );

    return (OGRFeatureDefnH) ((OGRFeature *) hFeat)->GetDefnRef();
}

/************************************************************************/
/*                        SetGeometryDirectly()                         */
/************************************************************************/

/**
 * \brief Set feature geometry.
 *
 * This method updates the features geometry, and operate exactly as
 * SetGeometry(), except that this method assumes ownership of the
 * passed geometry.
 *
 * This method is the same as the C function OGR_F_SetGeometryDirectly().
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
    delete poGeometry;
    poGeometry = poGeomIn;

    // I should be verifying that the geometry matches the defn's type.
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                     OGR_F_SetGeometryDirectly()                      */
/************************************************************************/

/**
 * \brief Set feature geometry.
 *
 * This function updates the features geometry, and operate exactly as
 * SetGeometry(), except that this function assumes ownership of the
 * passed geometry.
 *
 * This function is the same as the C++ method 
 * OGRFeature::SetGeometryDirectly.
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
    VALIDATE_POINTER1( hFeat, "OGR_F_SetGeometryDirectly", CE_Failure );

    return ((OGRFeature *) hFeat)->SetGeometryDirectly((OGRGeometry *) hGeom);
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
 * @param poGeomIn new geometry to apply to feature. Passing NULL value here
 * is correct and it will result in deallocation of currently assigned geometry
 * without assigning new one.
 *
 * @return OGRERR_NONE if successful, or OGR_UNSUPPORTED_GEOMETRY_TYPE if
 * the geometry type is illegal for the OGRFeatureDefn (checking not yet
 * implemented). 
 */ 

OGRErr OGRFeature::SetGeometry( OGRGeometry * poGeomIn )

{
    delete poGeometry;

    if( poGeomIn != NULL )
        poGeometry = poGeomIn->clone();
    else
        poGeometry = NULL;

    // I should be verifying that the geometry matches the defn's type.
    
    return OGRERR_NONE;
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
 * @param hFeat handle to the feature on which new geometry is applied to.
 * @param hGeom handle to the new geometry to apply to feature.
 *
 * @return OGRERR_NONE if successful, or OGR_UNSUPPORTED_GEOMETRY_TYPE if
 * the geometry type is illegal for the OGRFeatureDefn (checking not yet
 * implemented). 
 */ 

OGRErr OGR_F_SetGeometry( OGRFeatureH hFeat, OGRGeometryH hGeom )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_SetGeometry", CE_Failure );

    return ((OGRFeature *) hFeat)->SetGeometry((OGRGeometry *) hGeom);
}

/************************************************************************/
/*                           StealGeometry()                            */
/************************************************************************/

/**
 * \brief Take away ownership of geometry.
 *
 * Fetch the geometry from this feature, and clear the reference to the
 * geometry on the feature.  This is a mechanism for the application to
 * take over ownship of the geometry from the feature without copying. 
 * Sort of an inverse to SetGeometryDirectly().
 *
 * After this call the OGRFeature will have a NULL geometry.
 *
 * @return the pointer to the geometry.
 */

OGRGeometry *OGRFeature::StealGeometry()

{
    OGRGeometry *poReturn = poGeometry;
    poGeometry = NULL;
    return poReturn;
}

/************************************************************************/
/*                        OGR_F_StealGeometry()                         */
/************************************************************************/

/**
 * \brief Take away ownership of geometry.
 *
 * Fetch the geometry from this feature, and clear the reference to the
 * geometry on the feature.  This is a mechanism for the application to
 * take over ownship of the geometry from the feature without copying. 
 * Sort of an inverse to OGR_FSetGeometryDirectly().
 *
 * After this call the OGRFeature will have a NULL geometry.
 *
 * @return the pointer to the geometry.
 */

OGRGeometryH OGR_F_StealGeometry( OGRFeatureH hFeat )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_StealGeometry", NULL );

    return (OGRGeometryH) ((OGRFeature *) hFeat)->StealGeometry();
}

/************************************************************************/
/*                           GetGeometryRef()                           */
/************************************************************************/

/**
 * \fn OGRGeometry *OGRFeature::GetGeometryRef();
 *
 * \brief Fetch pointer to feature geometry.
 *
 * This method is the same as the C function OGR_F_GetGeometryRef().
 *
 * @return pointer to internal feature geometry.  This object should
 * not be modified.
 */

/************************************************************************/
/*                        OGR_F_GetGeometryRef()                        */
/************************************************************************/

/**
 * \brief Fetch an handle to feature geometry.
 *
 * This function is the same as the C++ method OGRFeature::GetGeometryRef().
 *
 * @param hFeat handle to the feature to get geometry from.
 * @return an handle to internal feature geometry.  This object should
 * not be modified.
 */

OGRGeometryH OGR_F_GetGeometryRef( OGRFeatureH hFeat )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetGeometryRef", NULL );

    return (OGRGeometryH) ((OGRFeature *) hFeat)->GetGeometryRef();
}

/************************************************************************/
/*                               Clone()                                */
/************************************************************************/

/**
 * \brief Duplicate feature.
 *
 * The newly created feature is owned by the caller, and will have it's own
 * reference to the OGRFeatureDefn.
 *
 * This method is the same as the C function OGR_F_Clone().
 *
 * @return new feature, exactly matching this feature.
 */

OGRFeature *OGRFeature::Clone()

{
    OGRFeature  *poNew = new OGRFeature( poDefn );

    poNew->SetGeometry( poGeometry );

    for( int i = 0; i < poDefn->GetFieldCount(); i++ )
    {
        poNew->SetField( i, pauFields + i );
    }

    if( GetStyleString() != NULL )
        poNew->SetStyleString(GetStyleString());

    poNew->SetFID( GetFID() );

    return poNew;
}

/************************************************************************/
/*                            OGR_F_Clone()                             */
/************************************************************************/

/**
 * \brief Duplicate feature.
 *
 * The newly created feature is owned by the caller, and will have it's own
 * reference to the OGRFeatureDefn.
 *
 * This function is the same as the C++ method OGRFeature::Clone().
 *
 * @param hFeat handle to the feature to clone.
 * @return an handle to the new feature, exactly matching this feature.
 */

OGRFeatureH OGR_F_Clone( OGRFeatureH hFeat )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_Clone", NULL );

    return (OGRFeatureH) ((OGRFeature *) hFeat)->Clone();
}

/************************************************************************/
/*                           GetFieldCount()                            */
/************************************************************************/

/**
 * \fn int OGRFeature::GetFieldCount();
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

    return ((OGRFeature *) hFeat)->GetFieldCount();
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
 * @return an handle to the field definition (from the OGRFeatureDefn).
 * This is an internal reference, and should not be deleted or modified.
 */

OGRFieldDefnH OGR_F_GetFieldDefnRef( OGRFeatureH hFeat, int i )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetFieldDefnRef", NULL );

    return (OGRFieldDefnH) ((OGRFeature *) hFeat)->GetFieldDefnRef(i);
}

/************************************************************************/
/*                           GetFieldIndex()                            */
/************************************************************************/

/**
 * \fn int OGRFeature::GetFieldIndex( const char * pszName );
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

    return ((OGRFeature *) hFeat)->GetFieldIndex( pszName );
}

/************************************************************************/
/*                             IsFieldSet()                             */
/************************************************************************/

/**
 * \fn int OGRFeature::IsFieldSet( int iField ) const;
 *
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
    int iSpecialField = iField - poDefn->GetFieldCount();
    if (iSpecialField >= 0)
    {
        // special field value accessors
        switch (iSpecialField)
        {
          case SPF_FID:
            return ((OGRFeature *)this)->GetFID() != OGRNullFID;

          case SPF_OGR_GEOM_WKT:
          case SPF_OGR_GEOMETRY:
            return poGeometry != NULL;

          case SPF_OGR_STYLE:
            return ((OGRFeature *)this)->GetStyleString() != NULL;

          case SPF_OGR_GEOM_AREA:
            if( poGeometry == NULL )
                return FALSE;

            return OGR_G_GetArea((OGRGeometryH)poGeometry) != 0.0;

          default:
            return FALSE;
        }
    }
    else
    { 
        return pauFields[iField].Set.nMarker1 != OGRUnsetMarker
            || pauFields[iField].Set.nMarker2 != OGRUnsetMarker;
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
    
    OGRFeature* poFeature = (OGRFeature* )hFeat;
    
    if (iField < 0 || iField >= poFeature->GetFieldCount())
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
    OGRFieldDefn        *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == NULL || !IsFieldSet(iField) )
        return;
    
    switch( poFDefn->GetType() )
    {
      case OFTRealList:
      case OFTIntegerList:
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

    pauFields[iField].Set.nMarker1 = OGRUnsetMarker;
    pauFields[iField].Set.nMarker2 = OGRUnsetMarker;
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

    ((OGRFeature *) hFeat)->UnsetField( iField );
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

/************************************************************************/
/*                        OGR_F_GetRawFieldRef()                        */
/************************************************************************/

/**
 * \brief Fetch an handle to the internal field value given the index.  
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
    VALIDATE_POINTER1( hFeat, "OGR_F_GetRawFieldRef", NULL );

    return ((OGRFeature *)hFeat)->GetRawFieldRef( iField );
}

/************************************************************************/
/*                         GetFieldAsInteger()                          */
/************************************************************************/

/**
 * \brief Fetch field value as integer.
 *
 * OFTString features will be translated using atoi().  OFTReal fields
 * will be cast to integer.   Other field types, or errors will result in
 * a return value of zero.
 *
 * This method is the same as the C function OGR_F_GetFieldAsInteger().
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 *
 * @return the field value.
 */

int OGRFeature::GetFieldAsInteger( int iField )

{
    int iSpecialField = iField - poDefn->GetFieldCount();
    if (iSpecialField >= 0)
    {
    // special field value accessors
        switch (iSpecialField)
        {
        case SPF_FID:
            return GetFID();

        case SPF_OGR_GEOM_AREA:
            if( poGeometry == NULL )
                return 0;
            return (int)OGR_G_GetArea((OGRGeometryH)poGeometry);

        default:
            return 0;
        }
    }
    
    OGRFieldDefn        *poFDefn = poDefn->GetFieldDefn( iField );
    
    if( poFDefn == NULL )
        return 0;
    
    if( !IsFieldSet(iField) )
        return 0;
    
    if( poFDefn->GetType() == OFTInteger )
        return pauFields[iField].Integer;
    else if( poFDefn->GetType() == OFTReal )
        return (int) pauFields[iField].Real;
    else if( poFDefn->GetType() == OFTString )
    {
        if( pauFields[iField].String == NULL )
            return 0;
        else
            return atoi(pauFields[iField].String);
    }
    else
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

    return ((OGRFeature *)hFeat)->GetFieldAsInteger(iField);
}

/************************************************************************/
/*                          GetFieldAsDouble()                          */
/************************************************************************/

/**
 * \brief Fetch field value as a double.
 *
 * OFTString features will be translated using atof().  OFTInteger fields
 * will be cast to double.   Other field types, or errors will result in
 * a return value of zero.
 *
 * This method is the same as the C function OGR_F_GetFieldAsDouble().
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 *
 * @return the field value.
 */

double OGRFeature::GetFieldAsDouble( int iField )

{
    int iSpecialField = iField - poDefn->GetFieldCount();
    if (iSpecialField >= 0)
    {
    // special field value accessors
        switch (iSpecialField)
        {
        case SPF_FID:
            return GetFID();

        case SPF_OGR_GEOM_AREA:
            if( poGeometry == NULL )
                return 0.0;
            return OGR_G_GetArea((OGRGeometryH)poGeometry);

        default:
            return 0.0;
        }
    }
    
    OGRFieldDefn        *poFDefn = poDefn->GetFieldDefn( iField );
    
    if( poFDefn == NULL )
        return 0.0;

    if( !IsFieldSet(iField) )
        return 0.0;
    
    if( poFDefn->GetType() == OFTReal )
        return pauFields[iField].Real;
    else if( poFDefn->GetType() == OFTInteger )
        return pauFields[iField].Integer;
    else if( poFDefn->GetType() == OFTString )
    {
        if( pauFields[iField].String == NULL )
            return 0;
        else
            return atof(pauFields[iField].String);
    }
    else
        return 0.0;
}

/************************************************************************/
/*                       OGR_F_GetFieldAsDouble()                       */
/************************************************************************/

/**
 * \brief Fetch field value as a double.
 *
 * OFTString features will be translated using atof().  OFTInteger fields
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

    return ((OGRFeature *)hFeat)->GetFieldAsDouble(iField);
}

/************************************************************************/
/*                          GetFieldAsString()                          */
/************************************************************************/

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

const char *OGRFeature::GetFieldAsString( int iField )

{
#define TEMP_BUFFER_SIZE 80
    char         szTempBuffer[TEMP_BUFFER_SIZE];

    CPLFree(m_pszTmpFieldValue);
    m_pszTmpFieldValue = NULL;            

    int iSpecialField = iField - poDefn->GetFieldCount();
    if (iSpecialField >= 0)
    {
        // special field value accessors
        switch (iSpecialField)
        {
          case SPF_FID:
            snprintf( szTempBuffer, TEMP_BUFFER_SIZE, "%ld", GetFID() );
            return m_pszTmpFieldValue = CPLStrdup( szTempBuffer );

          case SPF_OGR_GEOMETRY:
            if( poGeometry )
                return poGeometry->getGeometryName();
            else
                return "";

          case SPF_OGR_STYLE:
            if( GetStyleString() == NULL )
                return "";
            else
                return GetStyleString();

          case SPF_OGR_GEOM_WKT:
          {
              if( poGeometry == NULL )
                  return "";

              if (poGeometry->exportToWkt( &m_pszTmpFieldValue ) == OGRERR_NONE )
                  return m_pszTmpFieldValue;
              else
                  return "";
          }

          case SPF_OGR_GEOM_AREA:
            if( poGeometry == NULL )
                return "";

            snprintf( szTempBuffer, TEMP_BUFFER_SIZE, "%.16g", 
                      OGR_G_GetArea((OGRGeometryH)poGeometry) );
            return m_pszTmpFieldValue = CPLStrdup( szTempBuffer );

          default:
            return "";
        }
    }
    
    OGRFieldDefn        *poFDefn = poDefn->GetFieldDefn( iField );
    
    if( poFDefn == NULL )
        return "";
    
    if( !IsFieldSet(iField) )
        return "";
    
    if( poFDefn->GetType() == OFTString )
    {
        if( pauFields[iField].String == NULL )
            return "";
        else
            return pauFields[iField].String;
    }
    else if( poFDefn->GetType() == OFTInteger )
    {
        snprintf( szTempBuffer, TEMP_BUFFER_SIZE,
                  "%d", pauFields[iField].Integer );
        return m_pszTmpFieldValue = CPLStrdup( szTempBuffer );
    }
    else if( poFDefn->GetType() == OFTReal )
    {
        char    szFormat[64];

        if( poFDefn->GetWidth() != 0 )
        {
            snprintf( szFormat, sizeof(szFormat), "%%%d.%df",
                      poFDefn->GetWidth(), poFDefn->GetPrecision() );
        }
        else
            strcpy( szFormat, "%.15g" );
        
        snprintf( szTempBuffer, TEMP_BUFFER_SIZE,
                  szFormat, pauFields[iField].Real );
        
        return m_pszTmpFieldValue = CPLStrdup( szTempBuffer );
    }
    else if( poFDefn->GetType() == OFTDateTime )
    {
        snprintf( szTempBuffer, TEMP_BUFFER_SIZE,
                  "%04d/%02d/%02d %2d:%02d:%02d", 
                  pauFields[iField].Date.Year,
                  pauFields[iField].Date.Month,
                  pauFields[iField].Date.Day,
                  pauFields[iField].Date.Hour,
                  pauFields[iField].Date.Minute,
                  pauFields[iField].Date.Second );
        
        if( pauFields[iField].Date.TZFlag > 1 )
        {
            int nOffset = (pauFields[iField].Date.TZFlag - 100) * 15;
            int nHours = (int) (nOffset / 60);  // round towards zero
            int nMinutes = ABS(nOffset - nHours * 60);

            if( nOffset < 0 )
            {
                strcat( szTempBuffer, "-" );
                nHours = ABS(nHours);
            }
            else
                strcat( szTempBuffer, "+" );

            if( nMinutes == 0 )
                snprintf( szTempBuffer+strlen(szTempBuffer), 
                          TEMP_BUFFER_SIZE-strlen(szTempBuffer), "%02d", nHours );
            else
                snprintf( szTempBuffer+strlen(szTempBuffer), 
                          TEMP_BUFFER_SIZE-strlen(szTempBuffer), "%02d%02d", nHours, nMinutes );
        }

        return m_pszTmpFieldValue = CPLStrdup( szTempBuffer );
    }
    else if( poFDefn->GetType() == OFTDate )
    {
        snprintf( szTempBuffer, TEMP_BUFFER_SIZE, "%04d/%02d/%02d",
                  pauFields[iField].Date.Year,
                  pauFields[iField].Date.Month,
                  pauFields[iField].Date.Day );

        return m_pszTmpFieldValue = CPLStrdup( szTempBuffer );
    }
    else if( poFDefn->GetType() == OFTTime )
    {
        snprintf( szTempBuffer, TEMP_BUFFER_SIZE, "%2d:%02d:%02d", 
                  pauFields[iField].Date.Hour,
                  pauFields[iField].Date.Minute,
                  pauFields[iField].Date.Second );
        
        return m_pszTmpFieldValue = CPLStrdup( szTempBuffer );
    }
    else if( poFDefn->GetType() == OFTIntegerList )
    {
        char    szItem[32];
        int     i, nCount = pauFields[iField].IntegerList.nCount;

        snprintf( szTempBuffer, TEMP_BUFFER_SIZE, "(%d:", nCount );
        for( i = 0; i < nCount; i++ )
        {
            snprintf( szItem, sizeof(szItem), "%d",
                      pauFields[iField].IntegerList.paList[i] );
            if( strlen(szTempBuffer) + strlen(szItem) + 6
                >= sizeof(szTempBuffer) )
            {
                break;
            }
            
            if( i > 0 )
                strcat( szTempBuffer, "," );
            
            strcat( szTempBuffer, szItem );
        }

        if( i < nCount )
            strcat( szTempBuffer, ",...)" );
        else
            strcat( szTempBuffer, ")" );
        
        return m_pszTmpFieldValue = CPLStrdup( szTempBuffer );
    }
    else if( poFDefn->GetType() == OFTRealList )
    {
        char    szItem[40];
        char    szFormat[64];
        int     i, nCount = pauFields[iField].RealList.nCount;

        if( poFDefn->GetWidth() != 0 )
        {
            snprintf( szFormat, sizeof(szFormat), "%%%d.%df",
                      poFDefn->GetWidth(), poFDefn->GetPrecision() );
        }
        else
            strcpy( szFormat, "%.16g" );
        
        snprintf( szTempBuffer, TEMP_BUFFER_SIZE, "(%d:", nCount );
        for( i = 0; i < nCount; i++ )
        {
            snprintf( szItem, sizeof(szItem), szFormat,
                      pauFields[iField].RealList.paList[i] );
            if( strlen(szTempBuffer) + strlen(szItem) + 6
                >= sizeof(szTempBuffer) )
            {
                break;
            }
            
            if( i > 0 )
                strcat( szTempBuffer, "," );
            
            strcat( szTempBuffer, szItem );
        }

        if( i < nCount )
            strcat( szTempBuffer, ",...)" );
        else
            strcat( szTempBuffer, ")" );
        
        return m_pszTmpFieldValue = CPLStrdup( szTempBuffer );
    }
    else if( poFDefn->GetType() == OFTStringList )
    {
        int     i, nCount = pauFields[iField].StringList.nCount;

        snprintf( szTempBuffer, TEMP_BUFFER_SIZE, "(%d:", nCount );
        for( i = 0; i < nCount; i++ )
        {
            const char  *pszItem = pauFields[iField].StringList.paList[i];
            
            if( strlen(szTempBuffer) + strlen(pszItem)  + 6
                >= sizeof(szTempBuffer) )
            {
                break;
            }

            if( i > 0 )
                strcat( szTempBuffer, "," );
            
            strcat( szTempBuffer, pszItem );
        }

        if( i < nCount )
            strcat( szTempBuffer, ",...)" );
        else
            strcat( szTempBuffer, ")" );
        
        return m_pszTmpFieldValue = CPLStrdup( szTempBuffer );
    }
    else if( poFDefn->GetType() == OFTBinary )
    {
        int     nCount = pauFields[iField].Binary.nCount;
        char    *pszHex;
        
        if( nCount > (int) sizeof(szTempBuffer) / 2 - 4 )
            nCount = sizeof(szTempBuffer) / 2 - 4;

        pszHex = CPLBinaryToHex( nCount, pauFields[iField].Binary.paData );

        memcpy( szTempBuffer, pszHex, 2 * nCount );
        szTempBuffer[nCount*2] = '\0';
        if( nCount < pauFields[iField].Binary.nCount )
            strcat( szTempBuffer, "..." );

        CPLFree( pszHex );

        return m_pszTmpFieldValue = CPLStrdup( szTempBuffer );
    }
    else
        return "";
#undef TEMP_BUFFER_SIZE
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
    VALIDATE_POINTER1( hFeat, "OGR_F_GetFieldAsString", NULL );

    return ((OGRFeature *)hFeat)->GetFieldAsString(iField);
}

/************************************************************************/
/*                       GetFieldAsIntegerList()                        */
/************************************************************************/

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

const int *OGRFeature::GetFieldAsIntegerList( int iField, int *pnCount )

{
    OGRFieldDefn        *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn != NULL && IsFieldSet(iField) &&
        poFDefn->GetType() == OFTIntegerList )
    {
        if( pnCount != NULL )
            *pnCount = pauFields[iField].IntegerList.nCount;

        return pauFields[iField].IntegerList.paList;
    }
    else
    {
        if( pnCount != NULL )
            *pnCount = 0;
        
        return NULL;
    }
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
    VALIDATE_POINTER1( hFeat, "OGR_F_GetFieldAsIntegerList", NULL );

    return ((OGRFeature *)hFeat)->GetFieldAsIntegerList(iField, pnCount);
}

/************************************************************************/
/*                        GetFieldAsDoubleList()                        */
/************************************************************************/

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

const double *OGRFeature::GetFieldAsDoubleList( int iField, int *pnCount )

{
    OGRFieldDefn        *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn != NULL && IsFieldSet(iField) &&
        poFDefn->GetType() == OFTRealList )
    {
        if( pnCount != NULL )
            *pnCount = pauFields[iField].RealList.nCount;

        return pauFields[iField].RealList.paList;
    }
    else
    {
        if( pnCount != NULL )
            *pnCount = 0;
        
        return NULL;
    }
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
    VALIDATE_POINTER1( hFeat, "OGR_F_GetFieldAsDoubleList", NULL );

    return ((OGRFeature *)hFeat)->GetFieldAsDoubleList(iField, pnCount);
}

/************************************************************************/
/*                        GetFieldAsStringList()                        */
/************************************************************************/

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
    OGRFieldDefn        *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == NULL )
        return NULL;
    
    if( !IsFieldSet(iField) )
        return NULL;
    
    if( poFDefn->GetType() == OFTStringList )
    {
        return pauFields[iField].StringList.paList;
    }
    else
    {
        return NULL;
    }
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
    VALIDATE_POINTER1( hFeat, "OGR_F_GetFieldAsStringList", NULL );

    return ((OGRFeature *)hFeat)->GetFieldAsStringList(iField);
}

/************************************************************************/
/*                          GetFieldAsBinary()                          */
/************************************************************************/

/**
 * \brief Fetch field value as binary data.
 *
 * Currently this method only works for OFTBinary fields.
 *
 * This method is the same as the C function OGR_F_GetFieldAsBinary().
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param pnBytes location to put the number of bytes returned.
 *
 * @return the field value.  This data is internal, and should not be
 * modified, or freed.  Its lifetime may be very brief.
 */

GByte *OGRFeature::GetFieldAsBinary( int iField, int *pnBytes )

{
    OGRFieldDefn        *poFDefn = poDefn->GetFieldDefn( iField );

    *pnBytes = 0;

    if( poFDefn == NULL )
        return NULL;
    
    if( !IsFieldSet(iField) )
        return NULL;
    
    if( poFDefn->GetType() == OFTBinary )
    {
        *pnBytes = pauFields[iField].Binary.nCount;
        return pauFields[iField].Binary.paData;
    }
    else
    {
        return NULL;
    }
}

/************************************************************************/
/*                       OGR_F_GetFieldAsBinary()                       */
/************************************************************************/

/**
 * \brief Fetch field value as binary.
 *
 * Currently this method only works for OFTBinary fields.
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
    VALIDATE_POINTER1( hFeat, "OGR_F_GetFieldAsBinary", NULL );
    VALIDATE_POINTER1( pnBytes, "OGR_F_GetFieldAsBinary", NULL );

    return ((OGRFeature *)hFeat)->GetFieldAsBinary(iField,pnBytes);
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
 * @param pnSecond (0-59)
 * @param pnTZFlag (0=unknown, 1=localtime, 100=GMT, see data model for details)
 *
 * @return TRUE on success or FALSE on failure.
 */

int OGRFeature::GetFieldAsDateTime( int iField,
                                    int *pnYear, int *pnMonth, int *pnDay,
                                    int *pnHour, int *pnMinute, int *pnSecond,
                                    int *pnTZFlag )

{
    OGRFieldDefn        *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == NULL )
        return FALSE;
    
    if( !IsFieldSet(iField) )
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
        if( pnSecond )
            *pnSecond = pauFields[iField].Date.Second;
        if( pnTZFlag )
            *pnTZFlag = pauFields[iField].Date.TZFlag;
        
        return TRUE;
    }
    else
    {
        return FALSE;
    }
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
 */

int OGR_F_GetFieldAsDateTime( OGRFeatureH hFeat, int iField,
                              int *pnYear, int *pnMonth, int *pnDay,
                              int *pnHour, int *pnMinute, int *pnSecond,
                              int *pnTZFlag )
    
{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetFieldAsDateTime", 0 );

    return ((OGRFeature *)hFeat)->GetFieldAsDateTime( iField,
                                                      pnYear, pnMonth, pnDay,
                                                      pnHour, pnMinute,pnSecond,
                                                      pnTZFlag );
}

/************************************************************************/
/*                              SetField()                              */
/************************************************************************/

/**
 * \brief Set field to integer value. 
 *
 * OFTInteger and OFTReal fields will be set directly.  OFTString fields
 * will be assigned a string representation of the value, but not necessarily
 * taking into account formatting constraints on this field.  Other field
 * types may be unaffected.
 *
 * This method is the same as the C function OGR_F_SetFieldInteger().
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param nValue the value to assign.
 */

void OGRFeature::SetField( int iField, int nValue )

{
    OGRFieldDefn        *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == NULL )
        return;

    if( poFDefn->GetType() == OFTInteger )
    {
        pauFields[iField].Integer = nValue;
        pauFields[iField].Set.nMarker2 = 0;
    }
    else if( poFDefn->GetType() == OFTReal )
    {
        pauFields[iField].Real = nValue;
    }
    else if( poFDefn->GetType() == OFTIntegerList )
    {
        SetField( iField, 1, &nValue );
    }
    else if( poFDefn->GetType() == OFTRealList )
    {
        double dfValue = nValue;
        SetField( iField, 1, &dfValue );
    }
    else if( poFDefn->GetType() == OFTString )
    {
        char    szTempBuffer[64];

        sprintf( szTempBuffer, "%d", nValue );

        if( IsFieldSet( iField) )
            CPLFree( pauFields[iField].String );
        
        pauFields[iField].String = CPLStrdup( szTempBuffer );
    }
    else
        /* do nothing for other field types */;
}

/************************************************************************/
/*                       OGR_F_SetFieldInteger()                        */
/************************************************************************/

/**
 * \brief Set field to integer value. 
 *
 * OFTInteger and OFTReal fields will be set directly.  OFTString fields
 * will be assigned a string representation of the value, but not necessarily
 * taking into account formatting constraints on this field.  Other field
 * types may be unaffected.
 *
 * This function is the same as the C++ method OGRFeature::SetField().
 *
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param nValue the value to assign.
 */

void OGR_F_SetFieldInteger( OGRFeatureH hFeat, int iField, int nValue )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetFieldInteger" );

    ((OGRFeature *)hFeat)->SetField( iField, nValue );
}

/************************************************************************/
/*                              SetField()                              */
/************************************************************************/

/**
 * \brief Set field to double value. 
 *
 * OFTInteger and OFTReal fields will be set directly.  OFTString fields
 * will be assigned a string representation of the value, but not necessarily
 * taking into account formatting constraints on this field.  Other field
 * types may be unaffected.
 *
 * This method is the same as the C function OGR_F_SetFieldDouble().
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param dfValue the value to assign.
 */

void OGRFeature::SetField( int iField, double dfValue )

{
    OGRFieldDefn        *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == NULL )
        return;
    
    if( poFDefn->GetType() == OFTReal )
    {
        pauFields[iField].Real = dfValue;
    }
    else if( poFDefn->GetType() == OFTInteger )
    {
        pauFields[iField].Integer = (int) dfValue;
        pauFields[iField].Set.nMarker2 = 0;
    }
    else if( poFDefn->GetType() == OFTRealList )
    {
        SetField( iField, 1, &dfValue );
    }
    else if( poFDefn->GetType() == OFTIntegerList )
    {
        int nValue = (int) dfValue;
        SetField( iField, 1, &nValue );
    }
    else if( poFDefn->GetType() == OFTString )
    {
        char    szTempBuffer[128];

        sprintf( szTempBuffer, "%.16g", dfValue );

        if( IsFieldSet( iField) )
            CPLFree( pauFields[iField].String );

        pauFields[iField].String = CPLStrdup( szTempBuffer );
    }
    else
        /* do nothing for other field types */;
}

/************************************************************************/
/*                        OGR_F_SetFieldDouble()                        */
/************************************************************************/

/**
 * \brief Set field to double value. 
 *
 * OFTInteger and OFTReal fields will be set directly.  OFTString fields
 * will be assigned a string representation of the value, but not necessarily
 * taking into account formatting constraints on this field.  Other field
 * types may be unaffected.
 *
 * This function is the same as the C++ method OGRFeature::SetField().
 *
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param dfValue the value to assign.
 */

void OGR_F_SetFieldDouble( OGRFeatureH hFeat, int iField, double dfValue )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetFieldDouble" );

    ((OGRFeature *)hFeat)->SetField( iField, dfValue );
}

/************************************************************************/
/*                              SetField()                              */
/************************************************************************/

/**
 * \brief Set field to string value. 
 *
 * OFTInteger fields will be set based on an atoi() conversion of the string.
 * OFTReal fields will be set based on an atof() conversion of the string.
 * Other field types may be unaffected.
 *
 * This method is the same as the C function OGR_F_SetFieldString().
 *
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param pszValue the value to assign.
 */

void OGRFeature::SetField( int iField, const char * pszValue )

{
    OGRFieldDefn        *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == NULL )
        return;
    
    if( poFDefn->GetType() == OFTString )
    {
        if( IsFieldSet(iField) )
            CPLFree( pauFields[iField].String );
            
        pauFields[iField].String = CPLStrdup( pszValue );
    }
    else if( poFDefn->GetType() == OFTInteger )
    {
        pauFields[iField].Integer = atoi(pszValue);
        pauFields[iField].Set.nMarker2 = OGRUnsetMarker;
    }
    else if( poFDefn->GetType() == OFTReal )
    {
        pauFields[iField].Real = atof(pszValue);
    }
    else if( poFDefn->GetType() == OFTDate 
             || poFDefn->GetType() == OFTTime
             || poFDefn->GetType() == OFTDateTime )
    {
        OGRField sWrkField;

        if( OGRParseDate( pszValue, &sWrkField, 0 ) )
            memcpy( pauFields+iField, &sWrkField, sizeof(sWrkField));
    }
    else if( poFDefn->GetType() == OFTIntegerList 
             || poFDefn->GetType() == OFTRealList )
    {
        char **papszValueList = NULL;

        if( pszValue[0] == '(' && strchr(pszValue,':') != NULL )
        {
            papszValueList = CSLTokenizeString2( 
                pszValue, ",:()", 0 );
        }

        if( CSLCount(papszValueList) == 0
            || atoi(papszValueList[0]) != CSLCount(papszValueList)-1 )
        {
            /* do nothing - the count does not match entries */
        }
        else if( poFDefn->GetType() == OFTIntegerList )
        {
            int i, nCount = atoi(papszValueList[0]);
            std::vector<int> anValues;

            for( i=0; i < nCount; i++ )
                anValues.push_back( atoi(papszValueList[i+1]) );
            SetField( iField, nCount, &(anValues[0]) );
        }
        else if( poFDefn->GetType() == OFTRealList )
        {
            int i, nCount = atoi(papszValueList[0]);
            std::vector<double> adfValues;

            for( i=0; i < nCount; i++ )
                adfValues.push_back( atof(papszValueList[i+1]) );
            SetField( iField, nCount, &(adfValues[0]) );
        }

        CSLDestroy(papszValueList);
    }
    else
        /* do nothing for other field types */;
}

/************************************************************************/
/*                        OGR_F_SetFieldString()                        */
/************************************************************************/

/**
 * \brief Set field to string value. 
 *
 * OFTInteger fields will be set based on an atoi() conversion of the string.
 * OFTReal fields will be set based on an atof() conversion of the string.
 * Other field types may be unaffected.
 *
 * This function is the same as the C++ method OGRFeature::SetField().
 *
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param pszValue the value to assign.
 */

void OGR_F_SetFieldString( OGRFeatureH hFeat, int iField, const char *pszValue)

{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetFieldString" );

    ((OGRFeature *)hFeat)->SetField( iField, pszValue );
}

/************************************************************************/
/*                              SetField()                              */
/************************************************************************/

/**
 * \brief Set field to list of integers value. 
 *
 * This method currently on has an effect of OFTIntegerList fields.
 *
 * This method is the same as the C function OGR_F_SetFieldIntegerList().
 *
 * @param iField the field to set, from 0 to GetFieldCount()-1.
 * @param nCount the number of values in the list being assigned.
 * @param panValues the values to assign.
 */

void OGRFeature::SetField( int iField, int nCount, int *panValues )

{
    OGRFieldDefn        *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == NULL )
        return;
    
    if( poFDefn->GetType() == OFTIntegerList )
    {
        OGRField        uField;

        uField.IntegerList.nCount = nCount;
        uField.Set.nMarker2 = 0;
        uField.IntegerList.paList = panValues;

        SetField( iField, &uField );
    }
    else if( poFDefn->GetType() == OFTRealList )
    {
        std::vector<double> adfValues;

        for( int i=0; i < nCount; i++ )
            adfValues.push_back( (double) panValues[i] );

        SetField( iField, nCount, &adfValues[0] );
    }
    else if( (poFDefn->GetType() == OFTInteger || poFDefn->GetType() == OFTReal)
             && nCount == 1 )
    {
        SetField( iField, panValues[0] );
    }
}

/************************************************************************/
/*                     OGR_F_SetFieldIntegerList()                      */
/************************************************************************/

/**
 * \brief Set field to list of integers value. 
 *
 * This function currently on has an effect of OFTIntegerList fields.
 *
 * This function is the same as the C++ method OGRFeature::SetField().
 *
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to set, from 0 to GetFieldCount()-1.
 * @param nCount the number of values in the list being assigned.
 * @param panValues the values to assign.
 */

void OGR_F_SetFieldIntegerList( OGRFeatureH hFeat, int iField, 
                                int nCount, int *panValues )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetFieldIntegerList" );

    ((OGRFeature *)hFeat)->SetField( iField, nCount, panValues );
}

/************************************************************************/
/*                              SetField()                              */
/************************************************************************/

/**
 * \brief Set field to list of doubles value. 
 *
 * This method currently on has an effect of OFTRealList fields.
 *
 * This method is the same as the C function OGR_F_SetFieldDoubleList().
 *
 * @param iField the field to set, from 0 to GetFieldCount()-1.
 * @param nCount the number of values in the list being assigned.
 * @param padfValues the values to assign.
 */

void OGRFeature::SetField( int iField, int nCount, double * padfValues )

{
    OGRFieldDefn        *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == NULL )
        return;
    
    if( poFDefn->GetType() == OFTRealList )
    {
        OGRField        uField;
        
        uField.RealList.nCount = nCount;
        uField.Set.nMarker2 = 0;
        uField.RealList.paList = padfValues;
        
        SetField( iField, &uField );
    }
    else if( poFDefn->GetType() == OFTIntegerList )
    {
        std::vector<int> anValues;

        for( int i=0; i < nCount; i++ )
            anValues.push_back( (int) padfValues[i] );

        SetField( iField, nCount, &anValues[0] );
    }
    else if( (poFDefn->GetType() == OFTInteger || poFDefn->GetType() == OFTReal)
             && nCount == 1 )
    {
        SetField( iField, padfValues[0] );
    }
}

/************************************************************************/
/*                      OGR_F_SetFieldDoubleList()                      */
/************************************************************************/

/**
 * \brief Set field to list of doubles value. 
 *
 * This function currently on has an effect of OFTRealList fields.
 *
 * This function is the same as the C++ method OGRFeature::SetField().
 *
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to set, from 0 to GetFieldCount()-1.
 * @param nCount the number of values in the list being assigned.
 * @param padfValues the values to assign.
 */

void OGR_F_SetFieldDoubleList( OGRFeatureH hFeat, int iField, 
                               int nCount, double *padfValues )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetFieldDoubleList" );

    ((OGRFeature *)hFeat)->SetField( iField, nCount, padfValues );
}

/************************************************************************/
/*                              SetField()                              */
/************************************************************************/

/**
 * \brief Set field to list of strings value. 
 *
 * This method currently on has an effect of OFTStringList fields.
 *
 * This method is the same as the C function OGR_F_SetFieldStringList().
 *
 * @param iField the field to set, from 0 to GetFieldCount()-1.
 * @param papszValues the values to assign.
 */

void OGRFeature::SetField( int iField, char ** papszValues )

{
    OGRFieldDefn        *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == NULL )
        return;
    
    if( poFDefn->GetType() == OFTStringList )
    {
        OGRField        uField;
        
        uField.StringList.nCount = CSLCount(papszValues);
        uField.Set.nMarker2 = 0;
        uField.StringList.paList = papszValues;

        SetField( iField, &uField );
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
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to set, from 0 to GetFieldCount()-1.
 * @param papszValues the values to assign.
 */

void OGR_F_SetFieldStringList( OGRFeatureH hFeat, int iField, 
                               char ** papszValues )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetFieldStringList" );

    ((OGRFeature *)hFeat)->SetField( iField, papszValues );
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
 * @param iField the field to set, from 0 to GetFieldCount()-1.
 * @param nBytes bytes of data being set.
 * @param pabyData the raw data being applied. 
 */

void OGRFeature::SetField( int iField, int nBytes, GByte *pabyData )

{
    OGRFieldDefn        *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == NULL )
        return;
    
    if( poFDefn->GetType() == OFTBinary )
    {
        OGRField        uField;

        uField.Binary.nCount = nBytes;
        uField.Set.nMarker2 = 0;
        uField.Binary.paData = pabyData;

        SetField( iField, &uField );
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
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to set, from 0 to GetFieldCount()-1.
 * @param nBytes the number of bytes in pabyData array.
 * @param pabyData the data to apply.
 */

void OGR_F_SetFieldBinary( OGRFeatureH hFeat, int iField, 
                           int nBytes, GByte *pabyData )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetFieldBinary" );

    ((OGRFeature *)hFeat)->SetField( iField, nBytes, pabyData );
}

/************************************************************************/
/*                              SetField()                              */
/************************************************************************/

/**
 * \brief Set field to date.
 *
 * This method currently only has an effect for OFTDate, OFTTime and OFTDateTime
 * fields.
 *
 * This method is the same as the C function OGR_F_SetFieldDateTime().
 *
 * @param iField the field to set, from 0 to GetFieldCount()-1.
 * @param nYear (including century)
 * @param nMonth (1-12)
 * @param nDay (1-31)
 * @param nHour (0-23)
 * @param nMinute (0-59)
 * @param nSecond (0-59)
 * @param nTZFlag (0=unknown, 1=localtime, 100=GMT, see data model for details)
 */

void OGRFeature::SetField( int iField, int nYear, int nMonth, int nDay,
                           int nHour, int nMinute, int nSecond, 
                           int nTZFlag )

{
    OGRFieldDefn        *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == NULL )
        return;
    
    if( poFDefn->GetType() == OFTDate 
        || poFDefn->GetType() == OFTTime 
        || poFDefn->GetType() == OFTDateTime )
    {
        pauFields[iField].Date.Year = (GInt16)nYear;
        pauFields[iField].Date.Month = (GByte)nMonth;
        pauFields[iField].Date.Day = (GByte)nDay;
        pauFields[iField].Date.Hour = (GByte)nHour;
        pauFields[iField].Date.Minute = (GByte)nMinute;
        pauFields[iField].Date.Second = (GByte)nSecond;
        pauFields[iField].Date.TZFlag = (GByte)nTZFlag;
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
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to set, from 0 to GetFieldCount()-1.
 * @param nYear (including century)
 * @param nMonth (1-12)
 * @param nDay (1-31)
 * @param nHour (0-23)
 * @param nMinute (0-59)
 * @param nSecond (0-59)
 * @param nTZFlag (0=unknown, 1=localtime, 100=GMT, see data model for details)
 */

void OGR_F_SetFieldDateTime( OGRFeatureH hFeat, int iField, 
                             int nYear, int nMonth, int nDay,
                             int nHour, int nMinute, int nSecond, 
                             int nTZFlag )


{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetFieldDateTime" );

    ((OGRFeature *)hFeat)->SetField( iField, nYear, nMonth, nDay,
                                     nHour, nMinute, nSecond, nTZFlag );
}

/************************************************************************/
/*                              SetField()                              */
/************************************************************************/

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
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param puValue the value to assign.
 */

void OGRFeature::SetField( int iField, OGRField * puValue )

{
    OGRFieldDefn        *poFDefn = poDefn->GetFieldDefn( iField );

    if( poFDefn == NULL )
        return;
    
    if( poFDefn->GetType() == OFTInteger )
    {
        pauFields[iField] = *puValue;
    }
    else if( poFDefn->GetType() == OFTReal )
    {
        pauFields[iField] = *puValue;
    }
    else if( poFDefn->GetType() == OFTString )
    {
        if( IsFieldSet( iField ) )
            CPLFree( pauFields[iField].String );
        
        if( puValue->String == NULL )
            pauFields[iField].String = NULL;
        else if( puValue->Set.nMarker1 == OGRUnsetMarker
                 && puValue->Set.nMarker2 == OGRUnsetMarker )
            pauFields[iField] = *puValue;
        else
            pauFields[iField].String = CPLStrdup( puValue->String );
    }
    else if( poFDefn->GetType() == OFTDate
             || poFDefn->GetType() == OFTTime
             || poFDefn->GetType() == OFTDateTime )
    {
        memcpy( pauFields+iField, puValue, sizeof(OGRField) );
    }
    else if( poFDefn->GetType() == OFTIntegerList )
    {
        int     nCount = puValue->IntegerList.nCount;
        
        if( IsFieldSet( iField ) )
            CPLFree( pauFields[iField].IntegerList.paList );
        
        if( puValue->Set.nMarker1 == OGRUnsetMarker
            && puValue->Set.nMarker2 == OGRUnsetMarker )
        {
            pauFields[iField] = *puValue;
        }
        else
        {
            pauFields[iField].IntegerList.paList =
                (int *) CPLMalloc(sizeof(int) * nCount);
            memcpy( pauFields[iField].IntegerList.paList,
                    puValue->IntegerList.paList,
                    sizeof(int) * nCount );
            pauFields[iField].IntegerList.nCount = nCount;
        }
    }
    else if( poFDefn->GetType() == OFTRealList )
    {
        int     nCount = puValue->RealList.nCount;

        if( IsFieldSet( iField ) )
            CPLFree( pauFields[iField].RealList.paList );

        if( puValue->Set.nMarker1 == OGRUnsetMarker
            && puValue->Set.nMarker2 == OGRUnsetMarker )
        {
            pauFields[iField] = *puValue;
        }
        else
        {
            pauFields[iField].RealList.paList =
                (double *) CPLMalloc(sizeof(double) * nCount);
            memcpy( pauFields[iField].RealList.paList,
                    puValue->RealList.paList,
                    sizeof(double) * nCount );
            pauFields[iField].RealList.nCount = nCount;
        }
    }
    else if( poFDefn->GetType() == OFTStringList )
    {
        if( IsFieldSet( iField ) )
            CSLDestroy( pauFields[iField].StringList.paList );
        
        if( puValue->Set.nMarker1 == OGRUnsetMarker
            && puValue->Set.nMarker2 == OGRUnsetMarker )
        {
            pauFields[iField] = *puValue;
        }
        else
        {
            pauFields[iField].StringList.paList =
                CSLDuplicate( puValue->StringList.paList );
            
            pauFields[iField].StringList.nCount = puValue->StringList.nCount;
            CPLAssert( CSLCount(puValue->StringList.paList)
                       == puValue->StringList.nCount );
        }
    }
    else if( poFDefn->GetType() == OFTBinary )
    {
        if( IsFieldSet( iField ) )
            CPLFree( pauFields[iField].Binary.paData );
        
        if( puValue->Set.nMarker1 == OGRUnsetMarker
            && puValue->Set.nMarker2 == OGRUnsetMarker )
        {
            pauFields[iField] = *puValue;
        }
        else
        {
            pauFields[iField].Binary.nCount = puValue->Binary.nCount;
            pauFields[iField].Binary.paData = 
                (GByte *) CPLMalloc(puValue->Binary.nCount);
            memcpy( pauFields[iField].Binary.paData, 
                    puValue->Binary.paData, 
                    puValue->Binary.nCount );
        }
    }
    else
        /* do nothing for other field types */;
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
 * @param hFeat handle to the feature that owned the field.
 * @param iField the field to fetch, from 0 to GetFieldCount()-1.
 * @param psValue handle on the value to assign.
 */

void OGR_F_SetFieldRaw( OGRFeatureH hFeat, int iField, OGRField *psValue )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetFieldRaw" );

    ((OGRFeature *)hFeat)->SetField( iField, psValue );
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

void OGRFeature::DumpReadable( FILE * fpOut, char** papszOptions )

{
    if( fpOut == NULL )
        fpOut = stdout;

    fprintf( fpOut, "OGRFeature(%s):%ld\n", poDefn->GetName(), GetFID() );

    const char* pszDisplayFields =
            CSLFetchNameValue(papszOptions, "DISPLAY_FIELDS");
    if (pszDisplayFields == NULL || CSLTestBoolean(pszDisplayFields))
    {
        for( int iField = 0; iField < GetFieldCount(); iField++ )
        {
            OGRFieldDefn    *poFDefn = poDefn->GetFieldDefn(iField);

            fprintf( fpOut, "  %s (%s) = ",
                    poFDefn->GetNameRef(),
                    OGRFieldDefn::GetFieldTypeName(poFDefn->GetType()) );

            if( IsFieldSet( iField ) )
                fprintf( fpOut, "%s\n", GetFieldAsString( iField ) );
            else
                fprintf( fpOut, "(null)\n" );

        }
    }


    if( GetStyleString() != NULL )
    {
        const char* pszDisplayStyle =
            CSLFetchNameValue(papszOptions, "DISPLAY_STYLE");
        if (pszDisplayStyle == NULL || CSLTestBoolean(pszDisplayStyle))
        {
            fprintf( fpOut, "  Style = %s\n", GetStyleString() );
        }
    }

    if( poGeometry != NULL )
    {
        const char* pszDisplayGeometry =
                CSLFetchNameValue(papszOptions, "DISPLAY_GEOMETRY");
        if ( ! (pszDisplayGeometry != NULL && EQUAL(pszDisplayGeometry, "NO") ) )
            poGeometry->dumpReadable( fpOut, "  ", papszOptions );
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

    ((OGRFeature *) hFeat)->DumpReadable( fpOut );
}

/************************************************************************/
/*                               GetFID()                               */
/************************************************************************/

/**
 * \fn long OGRFeature::GetFID();
 *
 * \brief Get feature identifier.
 *
 * This method is the same as the C function OGR_F_GetFID().
 *
 * @return feature id or OGRNullFID if none has been assigned.
 */

/************************************************************************/
/*                            OGR_F_GetFID()                            */
/************************************************************************/

/**
 * \brief Get feature identifier.
 *
 * This function is the same as the C++ method OGRFeature::GetFID().
 *
 * @param hFeat handle to the feature from which to get the feature
 * identifier.
 * @return feature id or OGRNullFID if none has been assigned.
 */

long OGR_F_GetFID( OGRFeatureH hFeat )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetFID", 0 );

    return ((OGRFeature *) hFeat)->GetFID();
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
 * @param nFID the new feature identifier value to assign.
 *
 * @return On success OGRERR_NONE, or on failure some other value. 
 */

OGRErr OGRFeature::SetFID( long nFID )

{
    this->nFID = nFID;
    
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

OGRErr OGR_F_SetFID( OGRFeatureH hFeat, long nFID )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_SetFID", CE_Failure );

    return ((OGRFeature *) hFeat)->SetFID(nFID);
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

OGRBoolean OGRFeature::Equal( OGRFeature * poFeature )

{
    if( poFeature == this )
        return TRUE;

    if( GetFID() != poFeature->GetFID() )
        return FALSE;
    
    if( GetDefnRef() != poFeature->GetDefnRef() )
        return FALSE;

    int i;
    int nFields = GetDefnRef()->GetFieldCount();
    for(i=0; i<nFields; i++)
    {
        if( IsFieldSet(i) != poFeature->IsFieldSet(i) )
            return FALSE;

        if( !IsFieldSet(i) )
            continue;

        switch (GetDefnRef()->GetFieldDefn(i)->GetType() )
        {
            case OFTInteger:
                if( GetFieldAsInteger(i) !=
                       poFeature->GetFieldAsInteger(i) )
                    return FALSE;
                break;

            case OFTReal:
                if( GetFieldAsDouble(i) !=
                       poFeature->GetFieldAsDouble(i) )
                    return FALSE;
                break;

            case OFTString:
                if ( strcmp(GetFieldAsString(i), 
                            poFeature->GetFieldAsString(i)) != 0 )
                    return FALSE;
                break;

            case OFTIntegerList:
            {
                int nCount1, nCount2;
                const int* pnList1 = GetFieldAsIntegerList(i, &nCount1);
                const int* pnList2 =
                          poFeature->GetFieldAsIntegerList(i, &nCount2);
                if( nCount1 != nCount2 )
                    return FALSE;
                int j;
                for(j=0;j<nCount1;j++)
                {
                    if( pnList1[j] != pnList2[j] )
                        return FALSE;
                }
                break;
            }

            case OFTRealList:
            {
                int nCount1, nCount2;
                const double* padfList1 =
                                   GetFieldAsDoubleList(i, &nCount1);
                const double* padfList2 =
                        poFeature->GetFieldAsDoubleList(i, &nCount2);
                if( nCount1 != nCount2 )
                    return FALSE;
                int j;
                for(j=0;j<nCount1;j++)
                {
                    if( padfList1[j] != padfList2[j] )
                        return FALSE;
                }
                break;
            }

            case OFTStringList:
            {
                int nCount1, nCount2;
                char** papszList1 = GetFieldAsStringList(i);
                char** papszList2 = poFeature->GetFieldAsStringList(i);
                nCount1 = CSLCount(papszList1);
                nCount2 = CSLCount(papszList2);
                if( nCount1 != nCount2 )
                    return FALSE;
                int j;
                for(j=0;j<nCount1;j++)
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
                int nYear1, nMonth1, nDay1, nHour1,
                    nMinute1, nSecond1, nTZFlag1;
                int nYear2, nMonth2, nDay2, nHour2,
                    nMinute2, nSecond2, nTZFlag2;
                GetFieldAsDateTime(i, &nYear1, &nMonth1, &nDay1,
                              &nHour1, &nMinute1, &nSecond1, &nTZFlag1);
                poFeature->GetFieldAsDateTime(i, &nYear2, &nMonth2, &nDay2,
                              &nHour2, &nMinute2, &nSecond2, &nTZFlag2);

                if( !(nYear1 == nYear2 && nMonth1 == nMonth2 &&
                      nDay1 == nDay2 && nHour1 == nHour2 &&
                      nMinute1 == nMinute2 && nSecond1 == nSecond2 &&
                      nTZFlag1 == nTZFlag2) )
                    return FALSE;
                break;
            }

            case OFTBinary:
            {
                int nCount1, nCount2;
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

    if( GetGeometryRef() == NULL && poFeature->GetGeometryRef() != NULL )
        return FALSE;

    if( GetGeometryRef() != NULL && poFeature->GetGeometryRef() == NULL )
        return FALSE;

    if( GetGeometryRef() != NULL && poFeature->GetGeometryRef() != NULL 
        && (!GetGeometryRef()->Equals( poFeature->GetGeometryRef() ) ) )
        return FALSE;

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

    return ((OGRFeature *) hFeat)->Equal( (OGRFeature *) hOtherFeat );
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

OGRErr OGRFeature::SetFrom( OGRFeature * poSrcFeature, int bForgiving )

{
/* -------------------------------------------------------------------- */
/*      Retrieve the field ids by name.                                 */
/* -------------------------------------------------------------------- */
    int         iField, *panMap;
    OGRErr      eErr;

    panMap = (int *) VSIMalloc( sizeof(int) * poSrcFeature->GetFieldCount() );
    for( iField = 0; iField < poSrcFeature->GetFieldCount(); iField++ )
    {
        panMap[iField] = GetFieldIndex(
            poSrcFeature->GetFieldDefnRef(iField)->GetNameRef() );

        if( panMap[iField] == -1 )
        {
            if( bForgiving )
                continue;
            else
            {
                VSIFree(panMap);
                return OGRERR_FAILURE;
            }
        }
    }

    eErr = SetFrom( poSrcFeature, panMap, bForgiving );
    
    VSIFree(panMap);
    
    return eErr;
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
    VALIDATE_POINTER1( hFeat, "OGR_F_SetFrom", CE_Failure );
    VALIDATE_POINTER1( hOtherFeat, "OGR_F_SetFrom", CE_Failure );

    return ((OGRFeature *) hFeat)->SetFrom( (OGRFeature *) hOtherFeat, 
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

OGRErr OGRFeature::SetFrom( OGRFeature * poSrcFeature, int *panMap ,
                            int bForgiving )

{
    OGRErr      eErr;

    SetFID( OGRNullFID );

/* -------------------------------------------------------------------- */
/*      Set the geometry.                                               */
/* -------------------------------------------------------------------- */
    eErr = SetGeometry( poSrcFeature->GetGeometryRef() );
    if( eErr != OGRERR_NONE )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Copy feature style string.                                      */
/* -------------------------------------------------------------------- */
    SetStyleString( poSrcFeature->GetStyleString() );

/* -------------------------------------------------------------------- */
/*      Set the fields by name.                                         */
/* -------------------------------------------------------------------- */
    int         iField, iDstField;

    for( iField = 0; iField < poSrcFeature->GetFieldCount(); iField++ )
    {
        iDstField = panMap[iField];

        if( iDstField < 0 )
            continue;

        if( GetFieldCount() <= iDstField )
            return OGRERR_FAILURE;

        if( !poSrcFeature->IsFieldSet(iField) )
        {
            UnsetField( iDstField );
            continue;
        }

        switch( poSrcFeature->GetFieldDefnRef(iField)->GetType() )
        {
          case OFTInteger:
            SetField( iDstField, poSrcFeature->GetFieldAsInteger( iField ) );
            break;

          case OFTReal:
            SetField( iDstField, poSrcFeature->GetFieldAsDouble( iField ) );
            break;

          case OFTString:
            SetField( iDstField, poSrcFeature->GetFieldAsString( iField ) );
            break;

          case OFTIntegerList:
          {
              if (GetFieldDefnRef(iDstField)->GetType() == OFTString)
              {
                  SetField( iDstField, poSrcFeature->GetFieldAsString(iField) );
              }
              else
              {
                  int nCount;
                  const int *panValues = poSrcFeature->GetFieldAsIntegerList( iField, &nCount);
                  SetField( iDstField, nCount, (int*) panValues );
              }
          }
          break;

          case OFTRealList:
          {
              if (GetFieldDefnRef(iDstField)->GetType() == OFTString)
              {
                  SetField( iDstField, poSrcFeature->GetFieldAsString(iField) );
              }
              else
              {
                  int nCount;
                  const double *padfValues = poSrcFeature->GetFieldAsDoubleList( iField, &nCount);
                  SetField( iDstField, nCount, (double*) padfValues );
              }
          }
          break;

          case OFTDate:
          case OFTDateTime:
          case OFTTime:
            if (GetFieldDefnRef(iDstField)->GetType() == OFTDate ||
                GetFieldDefnRef(iDstField)->GetType() == OFTTime ||
                GetFieldDefnRef(iDstField)->GetType() == OFTDateTime)
            {
                SetField( iDstField, poSrcFeature->GetRawFieldRef( iField ) );
            }
            else if (GetFieldDefnRef(iDstField)->GetType() == OFTString)
            {
                SetField( iDstField, poSrcFeature->GetFieldAsString( iField ) );
            }
            else if( !bForgiving )
                return OGRERR_FAILURE;
            break;

          default:
            if( poSrcFeature->GetFieldDefnRef(iField)->GetType()
                == GetFieldDefnRef(iDstField)->GetType() )
            {
                SetField( iDstField, poSrcFeature->GetRawFieldRef(iField) );
            }
            else if (GetFieldDefnRef(iDstField)->GetType() == OFTString)
            {
                SetField( iDstField, poSrcFeature->GetFieldAsString( iField ) );
            }
            else if( !bForgiving )
                return OGRERR_FAILURE;
            break;
        }
    }

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
                      int bForgiving, int *panMap )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_SetFrom", CE_Failure );
    VALIDATE_POINTER1( hOtherFeat, "OGR_F_SetFrom", CE_Failure );
    VALIDATE_POINTER1( panMap, "OGR_F_SetFrom", CE_Failure);

    return ((OGRFeature *) hFeat)->SetFrom( (OGRFeature *) hOtherFeat, 
                                           panMap, bForgiving );
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

const char *OGRFeature::GetStyleString()
{
    int  iStyleFieldIndex;

    if (m_pszStyleString)
        return m_pszStyleString;

    iStyleFieldIndex = GetFieldIndex("OGR_STYLE");
    if (iStyleFieldIndex >= 0)
        return GetFieldAsString(iStyleFieldIndex);

    return NULL;
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
    VALIDATE_POINTER1( hFeat, "OGR_F_GetStyleString", NULL );

    return ((OGRFeature *)hFeat)->GetStyleString();
}

/************************************************************************/
/*                             SetStyleString()                         */
/************************************************************************/

/**
 * \brief Set feature style string.
 * This method operate exactly as
 * OGRFeature::SetStyleStringDirectly() except that it does not assume
 * ownership of the passed string, but instead makes a copy of it.
 *
 * This method is the same as the C function OGR_F_SetStyleString().
 *
 * @param pszString the style string to apply to this feature, cannot be NULL.
 */

void OGRFeature::SetStyleString(const char *pszString)
{
    if (m_pszStyleString)
    {
        CPLFree(m_pszStyleString);
        m_pszStyleString = NULL;
    }
    
    if( pszString )
        m_pszStyleString = CPLStrdup(pszString);
}

/************************************************************************/
/*                        OGR_F_SetStyleString()                        */
/************************************************************************/

/**
 * \brief Set feature style string.
 * This method operate exactly as
 * OGR_F_SetStyleStringDirectly() except that it does not assume ownership
 * of the passed string, but instead makes a copy of it.
 *
 * This function is the same as the C++ method OGRFeature::SetStyleString().
 *
 * @param hFeat handle to the feature to set style to.
 * @param pszStyle the style string to apply to this feature, cannot be NULL.
 */

void OGR_F_SetStyleString( OGRFeatureH hFeat, const char *pszStyle )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetStyleString" );

    ((OGRFeature *)hFeat)->SetStyleString( pszStyle );
}

/************************************************************************/
/*                       SetStyleStringDirectly()                       */
/************************************************************************/

/**
 * \brief Set feature style string.
 * This method operate exactly as
 * OGRFeature::SetStyleString() except that it assumes ownership of the passed
 * string.
 *
 * This method is the same as the C function OGR_F_SetStyleStringDirectly().
 *
 * @param pszString the style string to apply to this feature, cannot be NULL.
 */

void OGRFeature::SetStyleStringDirectly(char *pszString)
{
    if (m_pszStyleString)
        CPLFree(m_pszStyleString);
    m_pszStyleString = pszString;
}

/************************************************************************/
/*                     OGR_F_SetStyleStringDirectly()                   */
/************************************************************************/

/**
 * \brief Set feature style string.
 * This method operate exactly as
 * OGR_F_SetStyleString() except that it assumes ownership of the passed
 * string.
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

    ((OGRFeature *)hFeat)->SetStyleStringDirectly( pszStyle );
}

//************************************************************************/
/*                           SetStyleTable()                            */
/************************************************************************/
void OGRFeature::SetStyleTable(OGRStyleTable *poStyleTable)
{
    if ( m_poStyleTable )
        delete m_poStyleTable;
    m_poStyleTable = ( poStyleTable ) ? poStyleTable->Clone() : NULL;
}

/************************************************************************/
/*                            RemapFields()                             */
/*                                                                      */
/*      This is used to transform a feature "in place" from one         */
/*      feature defn to another with minimum work.                      */
/************************************************************************/

OGRErr OGRFeature::RemapFields( OGRFeatureDefn *poNewDefn, 
                                int *panRemapSource )

{
    int  iDstField;
    OGRField *pauNewFields;

    if( poNewDefn == NULL )
        poNewDefn = poDefn;

    pauNewFields = (OGRField *) CPLCalloc( poNewDefn->GetFieldCount(), 
                                           sizeof(OGRField) );

    for( iDstField = 0; iDstField < poDefn->GetFieldCount(); iDstField++ )
    {
        if( panRemapSource[iDstField] == -1 )
        {
            pauNewFields[iDstField].Set.nMarker1 = OGRUnsetMarker;
            pauNewFields[iDstField].Set.nMarker2 = OGRUnsetMarker;
        }
        else
        {
            memcpy( pauNewFields + iDstField, 
                    pauFields + panRemapSource[iDstField],
                    sizeof(OGRField) );
        }
    }

    /* 
    ** We really should be freeing memory for old columns that
    ** are no longer present.  We don't for now because it is a bit messy
    ** and would take too long to test.  
    */

/* -------------------------------------------------------------------- */
/*      Apply new definition and fields.                                */
/* -------------------------------------------------------------------- */
    CPLFree( pauFields );
    pauFields = pauNewFields;

    poDefn = poNewDefn;

    return OGRERR_NONE;
}

/************************************************************************/
/*                         OGR_F_GetStyleTable()                        */
/************************************************************************/

OGRStyleTableH OGR_F_GetStyleTable( OGRFeatureH hFeat )

{
    VALIDATE_POINTER1( hFeat, "OGR_F_GetStyleTable", NULL );
    
    return (OGRStyleTableH) ((OGRFeature *) hFeat)->GetStyleTable( );
}

/************************************************************************/
/*                         OGR_F_SetStyleTableDirectly()                */
/************************************************************************/

void OGR_F_SetStyleTableDirectly( OGRFeatureH hFeat,
                                  OGRStyleTableH hStyleTable )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetStyleTableDirectly" );
    
    ((OGRFeature *) hFeat)->SetStyleTableDirectly( (OGRStyleTable *) hStyleTable);
}

/************************************************************************/
/*                         OGR_F_SetStyleTable()                        */
/************************************************************************/

void OGR_F_SetStyleTable( OGRFeatureH hFeat,
                          OGRStyleTableH hStyleTable )

{
    VALIDATE_POINTER0( hFeat, "OGR_F_SetStyleTable" );
    VALIDATE_POINTER0( hStyleTable, "OGR_F_SetStyleTable" );
    
    ((OGRFeature *) hFeat)->SetStyleTable( (OGRStyleTable *) hStyleTable);
}
