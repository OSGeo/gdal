/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRFieldDefn class implementation.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
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
#include "ogr_feature.h"

#include <cstring>

#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_p.h"
#include "ograpispy.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"


CPL_CVSID("$Id$")

/************************************************************************/
/*                            OGRFieldDefn()                            */
/************************************************************************/

/**
 * \brief Constructor.
 *
 * By default, fields have no width, precision, are nullable and not ignored.
 *
 * @param pszNameIn the name of the new field.
 * @param eTypeIn the type of the new field.
 */

OGRFieldDefn::OGRFieldDefn( const char * pszNameIn, OGRFieldType eTypeIn ) :
    pszName(CPLStrdup(pszNameIn)),
    pszAlternativeName(CPLStrdup("")),
    eType(eTypeIn),
    eJustify(OJUndefined),
    // Should nWidth & nPrecision be defined in some particular way for numbers?
    nWidth(0),
    nPrecision(0),
    pszDefault(nullptr),
    bIgnore(FALSE),
    eSubType(OFSTNone),
    bNullable(TRUE),
    bUnique(FALSE)
{}

/************************************************************************/
/*                            OGRFieldDefn()                            */
/************************************************************************/

/**
 * \brief Constructor.
 *
 * Create by cloning an existing field definition.
 *
 * @param poPrototype the field definition to clone.
 */

OGRFieldDefn::OGRFieldDefn( const OGRFieldDefn *poPrototype ) :
    pszName(CPLStrdup(poPrototype->GetNameRef())),
    pszAlternativeName(CPLStrdup(poPrototype->GetAlternativeNameRef())),
    eType(poPrototype->GetType()),
    eJustify(poPrototype->GetJustify()),
    nWidth(poPrototype->GetWidth()),
    nPrecision(poPrototype->GetPrecision()),
    pszDefault(nullptr),
    bIgnore(FALSE),  // TODO(schwehr): Can we use IsIgnored()?
    eSubType(poPrototype->GetSubType()),
    bNullable(poPrototype->IsNullable()),
    bUnique(poPrototype->IsUnique()),
    m_osDomainName(poPrototype->m_osDomainName)
{
    SetDefault(poPrototype->GetDefault());
}

/************************************************************************/
/*                           OGR_Fld_Create()                           */
/************************************************************************/
/**
 * \brief Create a new field definition.
 *
 * By default, fields have no width, precision, are nullable and not ignored.
 *
 * This function is the same as the CPP method OGRFieldDefn::OGRFieldDefn().
 *
 * @param pszName the name of the new field definition.
 * @param eType the type of the new field definition.
 * @return handle to the new field definition.
 */

OGRFieldDefnH OGR_Fld_Create( const char *pszName, OGRFieldType eType )

{
    return OGRFieldDefn::ToHandle(new OGRFieldDefn(pszName, eType));
}

/************************************************************************/
/*                           ~OGRFieldDefn()                            */
/************************************************************************/

OGRFieldDefn::~OGRFieldDefn()

{
    CPLFree(pszName);
    CPLFree(pszAlternativeName);
    CPLFree(pszDefault);
}

/************************************************************************/
/*                          OGR_Fld_Destroy()                           */
/************************************************************************/
/**
 * \brief Destroy a field definition.
 *
 * @param hDefn handle to the field definition to destroy.
 */

void OGR_Fld_Destroy( OGRFieldDefnH hDefn )

{
    delete OGRFieldDefn::FromHandle(hDefn);
}

/************************************************************************/
/*                              SetName()                               */
/************************************************************************/

/**
 * \brief Reset the name of this field.
 *
 * This method is the same as the C function OGR_Fld_SetName().
 *
 * @param pszNameIn the new name to apply.
 */

void OGRFieldDefn::SetName( const char * pszNameIn )

{
    if( pszName != pszNameIn )
    {
        CPLFree(pszName);
        pszName = CPLStrdup(pszNameIn);
    }
}

/************************************************************************/
/*                          OGR_Fld_SetName()                           */
/************************************************************************/
/**
 * \brief Reset the name of this field.
 *
 * This function is the same as the CPP method OGRFieldDefn::SetName().
 *
 * @param hDefn handle to the field definition to apply the new name to.
 * @param pszName the new name to apply.
 */

void OGR_Fld_SetName( OGRFieldDefnH hDefn, const char *pszName )

{
    OGRFieldDefn::FromHandle(hDefn)->SetName(pszName);
}

/************************************************************************/
/*                             GetNameRef()                             */
/************************************************************************/

/**
 * \fn const char *OGRFieldDefn::GetNameRef();
 *
 * \brief Fetch name of this field.
 *
 * This method is the same as the C function OGR_Fld_GetNameRef().
 *
 * @return pointer to an internal name string that should not be freed or
 * modified.
 */

/************************************************************************/
/*                         OGR_Fld_GetNameRef()                         */
/************************************************************************/
/**
 * \brief Fetch name of this field.
 *
 * This function is the same as the CPP method OGRFieldDefn::GetNameRef().
 *
 * @param hDefn handle to the field definition.
 * @return the name of the field definition.
 *
 */

const char *OGR_Fld_GetNameRef( OGRFieldDefnH hDefn )

{

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_Fld_GetXXXX(hDefn, "GetNameRef");
#endif

    return OGRFieldDefn::FromHandle(hDefn)->GetNameRef();
}


/************************************************************************/
/*                              SetAlternativeName()                    */
/************************************************************************/

/**
 * \brief Reset the alternative name (or "alias") for this field.
 *
 * The alternative name is an optional attribute for a field which can provide
 * a more user-friendly, descriptive name of a field which is not subject to
 * the usual naming constraints defined by the data provider.
 *
 * This is a metadata style attribute only: the alternative name cannot
 * be used in place of the actual field name during SQL queries or other
 * field name dependent API calls.
 *
 * This method is the same as the C function OGR_Fld_SetAlternativeName().
 *
 * @param pszAlternativeNameIn the new alternative name to apply.
 *
 * @since GDAL 3.2
 */

void OGRFieldDefn::SetAlternativeName( const char * pszAlternativeNameIn )

{
    if( pszAlternativeName != pszAlternativeNameIn )
    {
        CPLFree(pszAlternativeName);
        pszAlternativeName = CPLStrdup(pszAlternativeNameIn);
    }
}

/************************************************************************/
/*                          OGR_Fld_SetAlternativeName()                */
/************************************************************************/
/**
 * \brief Reset the alternative name (or "alias") for this field.
 *
 * The alternative name is an optional attribute for a field which can provide
 * a more user-friendly, descriptive name of a field which is not subject to
 * the usual naming constraints defined by the data provider.
 *
 * This is a metadata style attribute only: the alternative name cannot
 * be used in place of the actual field name during SQL queries or other
 * field name dependent API calls.
 *
 * This function is the same as the CPP method OGRFieldDefn::SetAlternativeName().
 *
 * @param hDefn handle to the field definition to apply the new alternative name to.
 * @param pszAlternativeName the new alternative name to apply.
 *
 * @since GDAL 3.2
 */

void OGR_Fld_SetAlternativeName( OGRFieldDefnH hDefn, const char *pszAlternativeName )

{
    OGRFieldDefn::FromHandle(hDefn)->SetAlternativeName(pszAlternativeName);
}

/************************************************************************/
/*                             GetAlternativeNameRef()                  */
/************************************************************************/

/**
 * \fn const char *OGRFieldDefn::GetAlternativeNameRef();
 *
 * \brief Fetch the alternative name (or "alias") for this field.
 *
 * The alternative name is an optional attribute for a field which can provide
 * a more user-friendly, descriptive name of a field which is not subject to
 * the usual naming constraints defined by the data provider.
 *
 * This is a metadata style attribute only: the alternative name cannot
 * be used in place of the actual field name during SQL queries or other
 * field name dependent API calls.
 *
 * This method is the same as the C function OGR_Fld_GetAlternativeNameRef().
 *
 * @return pointer to an internal alternative name string that should not be freed or
 * modified.
 *
 * @since GDAL 3.2
 */

/************************************************************************/
/*                         OGR_Fld_GetAlternativeNameRef()              */
/************************************************************************/
/**
 * \brief Fetch the alternative name (or "alias") for this field.
 *
 * The alternative name is an optional attribute for a field which can provide
 * a more user-friendly, descriptive name of a field which is not subject to
 * the usual naming constraints defined by the data provider.
 *
 * This is a metadata style attribute only: the alternative name cannot
 * be used in place of the actual field name during SQL queries or other
 * field name dependent API calls.
 *
 * This function is the same as the CPP method OGRFieldDefn::GetAlternativeNameRef().
 *
 * @param hDefn handle to the field definition.
 * @return the alternative name of the field definition.
 *
 * @since GDAL 3.2
 */

const char *OGR_Fld_GetAlternativeNameRef( OGRFieldDefnH hDefn )

{

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_Fld_GetXXXX(hDefn, "GetAlternativeNameRef");
#endif

    return OGRFieldDefn::FromHandle(hDefn)->GetAlternativeNameRef();
}


/************************************************************************/
/*                              GetType()                               */
/************************************************************************/

/**
 * \fn OGRFieldType OGRFieldDefn::GetType() const;
 *
 * \brief Fetch type of this field.
 *
 * This method is the same as the C function OGR_Fld_GetType().
 *
 * @return field type.
 */

/************************************************************************/
/*                          OGR_Fld_GetType()                           */
/************************************************************************/
/**
 * \brief Fetch type of this field.
 *
 * This function is the same as the CPP method OGRFieldDefn::GetType().
 *
 * @param hDefn handle to the field definition to get type from.
 * @return field type.
 */

OGRFieldType OGR_Fld_GetType( OGRFieldDefnH hDefn )

{

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_Fld_GetXXXX(hDefn, "GetType");
#endif

    return OGRFieldDefn::FromHandle(hDefn)->GetType();
}

/************************************************************************/
/*                              SetType()                               */
/************************************************************************/

/**
 * \brief Set the type of this field.
 * This should never be done to an OGRFieldDefn
 * that is already part of an OGRFeatureDefn.
 *
 * This method is the same as the C function OGR_Fld_SetType().
 *
 * @param eTypeIn the new field type.
 */

void OGRFieldDefn::SetType( OGRFieldType eTypeIn )
{
    if( !OGR_AreTypeSubTypeCompatible(eTypeIn, eSubType) )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Type and subtype of field definition are not compatible. "
                 "Resetting to OFSTNone");
        eSubType = OFSTNone;
    }
    eType = eTypeIn;
}

/************************************************************************/
/*                          OGR_Fld_SetType()                           */
/************************************************************************/
/**
 * \brief Set the type of this field.
 * This should never be done to an OGRFieldDefn
 * that is already part of an OGRFeatureDefn.
 *
 * This function is the same as the CPP method OGRFieldDefn::SetType().
 *
 * @param hDefn handle to the field definition to set type to.
 * @param eType the new field type.
 */

void OGR_Fld_SetType( OGRFieldDefnH hDefn, OGRFieldType eType )

{
    OGRFieldDefn::FromHandle(hDefn)->SetType(eType);
}

/************************************************************************/
/*                             GetSubType()                             */
/************************************************************************/

/**
 * \fn OGRFieldSubType OGRFieldDefn::GetSubType() const;
 *
 * \brief Fetch subtype of this field.
 *
 * This method is the same as the C function OGR_Fld_GetSubType().
 *
 * @return field subtype.
 * @since GDAL 2.0
 */

/************************************************************************/
/*                         OGR_Fld_GetSubType()                         */
/************************************************************************/
/**
 * \brief Fetch subtype of this field.
 *
 * This function is the same as the CPP method OGRFieldDefn::GetSubType().
 *
 * @param hDefn handle to the field definition to get subtype from.
 * @return field subtype.
 * @since GDAL 2.0
 */

OGRFieldSubType OGR_Fld_GetSubType( OGRFieldDefnH hDefn )

{

#ifdef OGRAPISPY_ENABLED
    if( bOGRAPISpyEnabled )
        OGRAPISpy_Fld_GetXXXX(hDefn, "GetSubType");
#endif

    return OGRFieldDefn::FromHandle(hDefn)->GetSubType();
}

/************************************************************************/
/*                             SetSubType()                             */
/************************************************************************/

/**
 * \brief Set the subtype of this field.
 * This should never be done to an OGRFieldDefn
 * that is already part of an OGRFeatureDefn.
 *
 * This method is the same as the C function OGR_Fld_SetSubType().
 *
 * @param eSubTypeIn the new field subtype.
 * @since GDAL 2.0
 */
void OGRFieldDefn::SetSubType( OGRFieldSubType eSubTypeIn )
{
    if( !OGR_AreTypeSubTypeCompatible(eType, eSubTypeIn) )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Type and subtype of field definition are not compatible. "
                 "Resetting to OFSTNone");
        eSubType = OFSTNone;
    }
    else
    {
        eSubType = eSubTypeIn;
    }
}

/************************************************************************/
/*                         OGR_Fld_SetSubType()                         */
/************************************************************************/
/**
 * \brief Set the subtype of this field.
 * This should never be done to an OGRFieldDefn
 * that is already part of an OGRFeatureDefn.
 *
 * This function is the same as the CPP method OGRFieldDefn::SetSubType().
 *
 * @param hDefn handle to the field definition to set type to.
 * @param eSubType the new field subtype.
 * @since GDAL 2.0
 */

void OGR_Fld_SetSubType( OGRFieldDefnH hDefn, OGRFieldSubType eSubType )

{
    OGRFieldDefn::FromHandle(hDefn)->SetSubType(eSubType);
}

/************************************************************************/
/*                             SetDefault()                             */
/************************************************************************/

/**
 * \brief Set default field value.
 *
 * The default field value is taken into account by drivers (generally those
 * with a SQL interface) that support it at field creation time. OGR will
 * generally not automatically set the default field value to null fields by
 * itself when calling OGRFeature::CreateFeature() / OGRFeature::SetFeature(),
 * but will let the low-level layers to do the job. So retrieving the feature
 * from the layer is recommended.
 *
 * The accepted values are NULL, a numeric value, a literal value enclosed
 * between single quote characters (and inner single quote characters escaped by
 * repetition of the single quote character),
 * CURRENT_TIMESTAMP, CURRENT_TIME, CURRENT_DATE or
 * a driver specific expression (that might be ignored by other drivers).
 * For a datetime literal value, format should be 'YYYY/MM/DD HH:MM:SS[.sss]'
 * (considered as UTC time).
 *
 * Drivers that support writing DEFAULT clauses will advertise the
 * GDAL_DCAP_DEFAULT_FIELDS driver metadata item.
 *
 * This function is the same as the C function OGR_Fld_SetDefault().
 *
 * @param pszDefaultIn new default field value or NULL pointer.
 *
 * @since GDAL 2.0
 */

void OGRFieldDefn::SetDefault( const char* pszDefaultIn )

{
    CPLFree(pszDefault);
    pszDefault = nullptr;

    if( pszDefaultIn && pszDefaultIn[0] == '\''  &&
        pszDefaultIn[strlen(pszDefaultIn)-1] == '\'' )
    {
        const char* pszPtr = pszDefaultIn + 1;  // Used after for.
        for( ; *pszPtr != '\0'; pszPtr++ )
        {
            if( *pszPtr == '\'' )
            {
                if( pszPtr[1] == '\0' )
                    break;
                if( pszPtr[1] != '\'' )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Incorrectly quoted string literal");
                    return;
                }
                pszPtr++;
            }
        }
        if( *pszPtr == '\0' )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Incorrectly quoted string literal");
            return;
        }
    }

    pszDefault = pszDefaultIn ? CPLStrdup(pszDefaultIn) : nullptr;
}

/************************************************************************/
/*                         OGR_Fld_SetDefault()                         */
/************************************************************************/

/**
 * \brief Set default field value.
 *
 * The default field value is taken into account by drivers (generally those
 * with a SQL interface) that support it at field creation time. OGR will
 * generally not automatically set the default field value to null fields by
 * itself when calling OGRFeature::CreateFeature() / OGRFeature::SetFeature(),
 * but will let the low-level layers to do the job. So retrieving the feature
 * from the layer is recommended.
 *
 * The accepted values are NULL, a numeric value, a literal value enclosed
 * between single quote characters (and inner single quote characters escaped by
 * repetition of the single quote character),
 * CURRENT_TIMESTAMP, CURRENT_TIME, CURRENT_DATE or
 * a driver specific expression (that might be ignored by other drivers).
 * For a datetime literal value, format should be 'YYYY/MM/DD HH:MM:SS[.sss]'
 * (considered as UTC time).
 *
 * Drivers that support writing DEFAULT clauses will advertise the
 * GDAL_DCAP_DEFAULT_FIELDS driver metadata item.
 *
 * This function is the same as the C++ method OGRFieldDefn::SetDefault().
 *
 * @param hDefn handle to the field definition.
 * @param pszDefault new default field value or NULL pointer.
 *
 * @since GDAL 2.0
 */

void CPL_DLL OGR_Fld_SetDefault( OGRFieldDefnH hDefn, const char* pszDefault )
{
    OGRFieldDefn::FromHandle(hDefn)->SetDefault(pszDefault);
}

/************************************************************************/
/*                             GetDefault()                             */
/************************************************************************/

/**
 * \brief Get default field value.
 *
 * This function is the same as the C function OGR_Fld_GetDefault().
 *
 * @return default field value or NULL.
 * @since GDAL 2.0
 */

const char* OGRFieldDefn::GetDefault() const

{
    return pszDefault;
}

/************************************************************************/
/*                         OGR_Fld_GetDefault()                         */
/************************************************************************/

/**
 * \brief Get default field value.
 *
 * This function is the same as the C++ method OGRFieldDefn::GetDefault().
 *
 * @param hDefn handle to the field definition.
 * @return default field value or NULL.
 * @since GDAL 2.0
 */

const char *OGR_Fld_GetDefault( OGRFieldDefnH hDefn )
{
    return OGRFieldDefn::FromHandle(hDefn)->GetDefault();
}

/************************************************************************/
/*                        IsDefaultDriverSpecific()                     */
/************************************************************************/

/**
 * \brief Returns whether the default value is driver specific.
 *
 * Driver specific default values are those that are *not* NULL, a
 * numeric value, a literal value enclosed between single quote
 * characters, CURRENT_TIMESTAMP, CURRENT_TIME, CURRENT_DATE or
 * datetime literal value.
 *
 * This method is the same as the C function
 * OGR_Fld_IsDefaultDriverSpecific().
 *
 * @return TRUE if the default value is driver specific.
 * @since GDAL 2.0
 */

int OGRFieldDefn::IsDefaultDriverSpecific() const
{
    if( pszDefault == nullptr )
        return FALSE;

    if( EQUAL(pszDefault, "NULL") ||
        EQUAL(pszDefault, "CURRENT_TIMESTAMP") ||
        EQUAL(pszDefault, "CURRENT_TIME") ||
        EQUAL(pszDefault, "CURRENT_DATE") )
        return FALSE;

    if( pszDefault[0] == '\'' && pszDefault[strlen(pszDefault)-1] == '\'' )
        return FALSE;

    char* pszEnd = nullptr;
    CPLStrtod(pszDefault, &pszEnd);
    if( *pszEnd == '\0' )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                     OGR_Fld_IsDefaultDriverSpecific()                */
/************************************************************************/

/**
 * \brief Returns whether the default value is driver specific.
 *
 * Driver specific default values are those that are *not* NULL, a
 * numeric value, a literal value enclosed between single quote
 * characters, CURRENT_TIMESTAMP, CURRENT_TIME, CURRENT_DATE or
 * datetime literal value.
 *
 * This function is the same as the C++ method
 * OGRFieldDefn::IsDefaultDriverSpecific().
 *
 * @param hDefn handle to the field definition
 * @return TRUE if the default value is driver specific.
 * @since GDAL 2.0
 */

int OGR_Fld_IsDefaultDriverSpecific( OGRFieldDefnH hDefn )
{
    return OGRFieldDefn::FromHandle(hDefn)->IsDefaultDriverSpecific();
}

/************************************************************************/
/*                          GetFieldTypeName()                          */
/************************************************************************/

/**
 * \brief Fetch human readable name for a field type.
 *
 * This static method is the same as the C function OGR_GetFieldTypeName().
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

      case OFTInteger64:
        return "Integer64";

      case OFTReal:
        return "Real";

      case OFTString:
        return "String";

      case OFTIntegerList:
        return "IntegerList";

      case OFTInteger64List:
        return "Integer64List";

      case OFTRealList:
        return "RealList";

      case OFTStringList:
        return "StringList";

      case OFTBinary:
        return "Binary";

      case OFTDate:
        return "Date";

      case OFTTime:
        return "Time";

      case OFTDateTime:
        return "DateTime";

      case OFTWideString:
      case OFTWideStringList:
        break;
    }

    return "(unknown)";
}

/************************************************************************/
/*                        OGR_GetFieldTypeName()                        */
/************************************************************************/
/**
 * \brief Fetch human readable name for a field type.
 *
 * This function is the same as the CPP method
 * OGRFieldDefn::GetFieldTypeName().
 *
 * @param eType the field type to get name for.
 * @return the name.
 */

const char *OGR_GetFieldTypeName( OGRFieldType eType )

{
    return OGRFieldDefn::GetFieldTypeName(eType);
}

/************************************************************************/
/*                        GetFieldSubTypeName()                         */
/************************************************************************/

/**
 * \brief Fetch human readable name for a field subtype.
 *
 * This static method is the same as the C function OGR_GetFieldSubTypeName().
 *
 * @param eSubType the field subtype to get name for.
 *
 * @return pointer to an internal static name string. It should not be
 * modified or freed.
 *
 * @since GDAL 2.0
 */

const char * OGRFieldDefn::GetFieldSubTypeName( OGRFieldSubType eSubType )

{
    switch( eSubType )
    {
      case OFSTNone:
        break;

      case OFSTBoolean:
        return "Boolean";

      case OFSTInt16:
        return "Int16";

      case OFSTFloat32:
        return "Float32";

      case OFSTJSON:
        return "JSON";

      case OFSTUUID:
        return "UUID";
    }
    return "None";
}

/************************************************************************/
/*                       OGR_GetFieldSubTypeName()                      */
/************************************************************************/
/**
 * \brief Fetch human readable name for a field subtype.
 *
 * This function is the same as the CPP method
 * OGRFieldDefn::GetFieldSubTypeName().
 *
 * @param eSubType the field subtype to get name for.
 * @return the name.
 *
 * @since GDAL 2.0
 */

const char *OGR_GetFieldSubTypeName( OGRFieldSubType eSubType )

{
    return OGRFieldDefn::GetFieldSubTypeName(eSubType);
}

/************************************************************************/
/*                       OGR_IsValidTypeAndSubType()                    */
/************************************************************************/
/**
 * \brief Return if type and subtype are compatible
 *
 * @param eType the field type.
 * @param eSubType the field subtype.
 * @return TRUE if type and subtype are compatible
 *
 * @since GDAL 2.0
 */

int OGR_AreTypeSubTypeCompatible( OGRFieldType eType, OGRFieldSubType eSubType )
{
    if( eSubType == OFSTNone )
        return TRUE;
    if( eSubType == OFSTBoolean || eSubType == OFSTInt16 )
        return eType == OFTInteger || eType == OFTIntegerList;
    if( eSubType == OFSTFloat32 )
        return eType == OFTReal || eType == OFTRealList;
    if( eSubType == OFSTJSON )
        return eType == OFTString;
    if( eSubType == OFSTUUID )
        return eType == OFTString;
    return FALSE;
}

/************************************************************************/
/*                             GetJustify()                             */
/************************************************************************/

/**
 * \fn OGRJustification OGRFieldDefn::GetJustify() const;
 *
 * \brief Get the justification for this field.
 *
 * Note: no driver is know to use the concept of field justification.
 *
 * This method is the same as the C function OGR_Fld_GetJustify().
 *
 * @return the justification.
 */

/************************************************************************/
/*                         OGR_Fld_GetJustify()                         */
/************************************************************************/
/**
 * \brief Get the justification for this field.
 *
 * This function is the same as the CPP method OGRFieldDefn::GetJustify().
 *
 * Note: no driver is know to use the concept of field justification.
 *
 * @param hDefn handle to the field definition to get justification from.
 * @return the justification.
 */

OGRJustification OGR_Fld_GetJustify( OGRFieldDefnH hDefn )

{
    return OGRFieldDefn::FromHandle(hDefn)->GetJustify();
}

/************************************************************************/
/*                             SetJustify()                             */
/************************************************************************/

/**
 * \fn void OGRFieldDefn::SetJustify( OGRJustification eJustify );
 *
 * \brief Set the justification for this field.
 *
 * Note: no driver is know to use the concept of field justification.
 *
 * This method is the same as the C function OGR_Fld_SetJustify().
 *
 * @param eJustify the new justification.
 */

/************************************************************************/
/*                         OGR_Fld_SetJustify()                         */
/************************************************************************/
/**
 * \brief Set the justification for this field.
 *
 * Note: no driver is know to use the concept of field justification.
 *
 * This function is the same as the CPP method OGRFieldDefn::SetJustify().
 *
 * @param hDefn handle to the field definition to set justification to.
 * @param eJustify the new justification.
 */

void OGR_Fld_SetJustify( OGRFieldDefnH hDefn, OGRJustification eJustify )

{
    OGRFieldDefn::FromHandle(hDefn)->SetJustify(eJustify);
}

/************************************************************************/
/*                              GetWidth()                              */
/************************************************************************/

/**
 * \fn int OGRFieldDefn::GetWidth() const;
 *
 * \brief Get the formatting width for this field.
 *
 * This method is the same as the C function OGR_Fld_GetWidth().
 *
 * @return the width, zero means no specified width.
 */

/************************************************************************/
/*                          OGR_Fld_GetWidth()                          */
/************************************************************************/
/**
 * \brief Get the formatting width for this field.
 *
 * This function is the same as the CPP method OGRFieldDefn::GetWidth().
 *
 * @param hDefn handle to the field definition to get width from.
 * @return the width, zero means no specified width.
 */

int OGR_Fld_GetWidth( OGRFieldDefnH hDefn )

{
    return OGRFieldDefn::FromHandle(hDefn)->GetWidth();
}

/************************************************************************/
/*                              SetWidth()                              */
/************************************************************************/

/**
 * \fn void OGRFieldDefn::SetWidth( int nWidth );
 *
 * \brief Set the formatting width for this field in characters.
 *
 * This method is the same as the C function OGR_Fld_SetWidth().
 *
 * @param nWidth the new width.
 */

/************************************************************************/
/*                          OGR_Fld_SetWidth()                          */
/************************************************************************/
/**
 * \brief Set the formatting width for this field in characters.
 *
 * This function is the same as the CPP method OGRFieldDefn::SetWidth().
 *
 * @param hDefn handle to the field definition to set width to.
 * @param nNewWidth the new width.
 */

void OGR_Fld_SetWidth( OGRFieldDefnH hDefn, int nNewWidth )

{
    OGRFieldDefn::FromHandle(hDefn)->SetWidth(nNewWidth);
}

/************************************************************************/
/*                            GetPrecision()                            */
/************************************************************************/

/**
 * \fn int OGRFieldDefn::GetPrecision() const;
 *
 * \brief Get the formatting precision for this field.
 * This should normally be
 * zero for fields of types other than OFTReal.
 *
 * This method is the same as the C function OGR_Fld_GetPrecision().
 *
 * @return the precision.
 */

/************************************************************************/
/*                        OGR_Fld_GetPrecision()                        */
/************************************************************************/
/**
 * \brief Get the formatting precision for this field.
 * This should normally be
 * zero for fields of types other than OFTReal.
 *
 * This function is the same as the CPP method OGRFieldDefn::GetPrecision().
 *
 * @param hDefn handle to the field definition to get precision from.
 * @return the precision.
 */

int OGR_Fld_GetPrecision( OGRFieldDefnH hDefn )

{
    return OGRFieldDefn::FromHandle(hDefn)->GetPrecision();
}

/************************************************************************/
/*                            SetPrecision()                            */
/************************************************************************/

/**
 * \fn void OGRFieldDefn::SetPrecision( int nPrecision );
 *
 * \brief Set the formatting precision for this field in characters.
 *
 * This should normally be zero for fields of types other than OFTReal.
 *
 * This method is the same as the C function OGR_Fld_SetPrecision().
 *
 * @param nPrecision the new precision.
 */

/************************************************************************/
/*                        OGR_Fld_SetPrecision()                        */
/************************************************************************/
/**
 * \brief Set the formatting precision for this field in characters.
 *
 * This should normally be zero for fields of types other than OFTReal.
 *
 * This function is the same as the CPP method OGRFieldDefn::SetPrecision().
 *
 * @param hDefn handle to the field definition to set precision to.
 * @param nPrecision the new precision.
 */

void OGR_Fld_SetPrecision( OGRFieldDefnH hDefn, int nPrecision )

{
    OGRFieldDefn::FromHandle(hDefn)->SetPrecision( nPrecision );
}

/************************************************************************/
/*                                Set()                                 */
/************************************************************************/

/**
 * \brief Set defining parameters for a field in one call.
 *
 * This method is the same as the C function OGR_Fld_Set().
 *
 * @param pszNameIn the new name to assign.
 * @param eTypeIn the new type (one of the OFT values like OFTInteger).
 * @param nWidthIn the preferred formatting width.  Defaults to zero indicating
 * undefined.
 * @param nPrecisionIn number of decimals places for formatting, defaults to
 * zero indicating undefined.
 * @param eJustifyIn the formatting justification (OJLeft or OJRight), defaults
 * to OJUndefined.
 */

void OGRFieldDefn::Set( const char *pszNameIn,
                        OGRFieldType eTypeIn,
                        int nWidthIn, int nPrecisionIn,
                        OGRJustification eJustifyIn )
{
    SetName(pszNameIn);
    SetType(eTypeIn);
    SetWidth(nWidthIn);
    SetPrecision(nPrecisionIn);
    SetJustify(eJustifyIn);
}

/************************************************************************/
/*                            OGR_Fld_Set()                             */
/************************************************************************/
/**
 * \brief Set defining parameters for a field in one call.
 *
 * This function is the same as the CPP method OGRFieldDefn::Set().
 *
 * @param hDefn handle to the field definition to set to.
 * @param pszNameIn the new name to assign.
 * @param eTypeIn the new type (one of the OFT values like OFTInteger).
 * @param nWidthIn the preferred formatting width.  Defaults to zero indicating
 * undefined.
 * @param nPrecisionIn number of decimals places for formatting, defaults to
 * zero indicating undefined.
 * @param eJustifyIn the formatting justification (OJLeft or OJRight), defaults
 * to OJUndefined.
 */

void OGR_Fld_Set( OGRFieldDefnH hDefn, const char *pszNameIn,
                  OGRFieldType eTypeIn,
                  int nWidthIn, int nPrecisionIn,
                  OGRJustification eJustifyIn )

{
    OGRFieldDefn::FromHandle(hDefn)->
        Set(pszNameIn, eTypeIn, nWidthIn, nPrecisionIn, eJustifyIn);
}

/************************************************************************/
/*                             IsIgnored()                              */
/************************************************************************/

/**
 * \fn int OGRFieldDefn::IsIgnored() const;
 *
 * \brief Return whether this field should be omitted when fetching features
 *
 * This method is the same as the C function OGR_Fld_IsIgnored().
 *
 * @return ignore state
 */

/************************************************************************/
/*                         OGR_Fld_IsIgnored()                          */
/************************************************************************/

/**
 * \brief Return whether this field should be omitted when fetching features
 *
 * This method is the same as the C++ method OGRFieldDefn::IsIgnored().
 *
 * @param hDefn handle to the field definition
 * @return ignore state
 */

int OGR_Fld_IsIgnored( OGRFieldDefnH hDefn )
{
    return OGRFieldDefn::FromHandle(hDefn)->IsIgnored();
}

/************************************************************************/
/*                            SetIgnored()                              */
/************************************************************************/

/**
 * \fn void OGRFieldDefn::SetIgnored( int ignore );
 *
 * \brief Set whether this field should be omitted when fetching features
 *
 * This method is the same as the C function OGR_Fld_SetIgnored().
 *
 * @param ignore ignore state
 */

/************************************************************************/
/*                        OGR_Fld_SetIgnored()                          */
/************************************************************************/

/**
 * \brief Set whether this field should be omitted when fetching features
 *
 * This method is the same as the C++ method OGRFieldDefn::SetIgnored().
 *
 * @param hDefn handle to the field definition
 * @param ignore ignore state
 */

void OGR_Fld_SetIgnored( OGRFieldDefnH hDefn, int ignore )
{
    OGRFieldDefn::FromHandle(hDefn)->SetIgnored(ignore);
}

/************************************************************************/
/*                             IsSame()                                 */
/************************************************************************/

/**
 * \brief Test if the field definition is identical to the other one.
 *
 * @param poOtherFieldDefn the other field definition to compare to.
 * @return TRUE if the field definition is identical to the other one.
 */

int OGRFieldDefn::IsSame( const OGRFieldDefn * poOtherFieldDefn ) const
{
    return
        strcmp(pszName, poOtherFieldDefn->pszName) == 0 &&
        strcmp(pszAlternativeName, poOtherFieldDefn->pszAlternativeName) == 0 &&
        eType == poOtherFieldDefn->eType &&
        eSubType == poOtherFieldDefn->eSubType &&
        nWidth == poOtherFieldDefn->nWidth &&
        nPrecision == poOtherFieldDefn->nPrecision &&
        bNullable == poOtherFieldDefn->bNullable;
}

/************************************************************************/
/*                             IsNullable()                             */
/************************************************************************/

/**
 * \fn int OGRFieldDefn::IsNullable() const
 *
 * \brief Return whether this field can receive null values.
 *
 * By default, fields are nullable.
 *
 * Even if this method returns FALSE (i.e not-nullable field), it doesn't mean
 * that OGRFeature::IsFieldSet() will necessary return TRUE, as fields can be
 * temporary unset and null/not-null validation is usually done when
 * OGRLayer::CreateFeature()/SetFeature() is called.
 *
 * This method is the same as the C function OGR_Fld_IsNullable().
 *
 * @return TRUE if the field is authorized to be null.
 * @since GDAL 2.0
 */

/************************************************************************/
/*                         OGR_Fld_IsNullable()                         */
/************************************************************************/

/**
 * \brief Return whether this field can receive null values.
 *
 * By default, fields are nullable.
 *
 * Even if this method returns FALSE (i.e not-nullable field), it doesn't mean
 * that OGRFeature::IsFieldSet() will necessary return TRUE, as fields can be
 * temporary unset and null/not-null validation is usually done when
 * OGRLayer::CreateFeature()/SetFeature() is called.
 *
 * This method is the same as the C++ method OGRFieldDefn::IsNullable().
 *
 * @param hDefn handle to the field definition
 * @return TRUE if the field is authorized to be null.
 * @since GDAL 2.0
 */

int OGR_Fld_IsNullable( OGRFieldDefnH hDefn )
{
    return OGRFieldDefn::FromHandle(hDefn)->IsNullable();
}

/************************************************************************/
/*                            SetNullable()                             */
/************************************************************************/

/**
 * \fn void OGRFieldDefn::SetNullable( int bNullableIn );
 *
 * \brief Set whether this field can receive null values.
 *
 * By default, fields are nullable, so this method is generally called with
 * FALSE to set a not-null constraint.
 *
 * Drivers that support writing not-null constraint will advertise the
 * GDAL_DCAP_NOTNULL_FIELDS driver metadata item.
 *
 * This method is the same as the C function OGR_Fld_SetNullable().
 *
 * @param bNullableIn FALSE if the field must have a not-null constraint.
 * @since GDAL 2.0
 */

/************************************************************************/
/*                        OGR_Fld_SetNullable()                          */
/************************************************************************/

/**
 * \brief Set whether this field can receive null values.
 *
 * By default, fields are nullable, so this method is generally called with
 * FALSE to set a not-null constraint.
 *
 * Drivers that support writing not-null constraint will advertise the
 * GDAL_DCAP_NOTNULL_FIELDS driver metadata item.
 *
 * This method is the same as the C++ method OGRFieldDefn::SetNullable().
 *
 * @param hDefn handle to the field definition
 * @param bNullableIn FALSE if the field must have a not-null constraint.
 * @since GDAL 2.0
 */

void OGR_Fld_SetNullable( OGRFieldDefnH hDefn, int bNullableIn )
{
    OGRFieldDefn::FromHandle(hDefn)->SetNullable(bNullableIn);
}

/************************************************************************/
/*                             IsUnique()                             */
/************************************************************************/

/**
 * \fn int OGRFieldDefn::IsUnique() const
 *
 * \brief Return whether this field has a unique constraint.
 *
 * By default, fields have no unique constraint.
 *
 * This method is the same as the C function OGR_Fld_IsUnique().
 *
 * @return TRUE if the field has a unique constraint.
 * @since GDAL 3.2
 */

/************************************************************************/
/*                         OGR_Fld_IsUnique()                         */
/************************************************************************/

/**
 * \brief Return whether this field has a unique constraint.
 *
 * By default, fields have no unique constraint.
 *
 * This method is the same as the C++ method OGRFieldDefn::IsUnique().
 *
 * @param hDefn handle to the field definition
 * @return TRUE if the field has a unique constraint.
 * @since GDAL 3.2
 */

int OGR_Fld_IsUnique( OGRFieldDefnH hDefn )
{
    return OGRFieldDefn::FromHandle(hDefn)->IsUnique();
}

/************************************************************************/
/*                            SetUnique()                             */
/************************************************************************/

/**
 * \fn void OGRFieldDefn::SetUnique( int bUniqueIn );
 *
 * \brief Set whether this field has a unique constraint.
 *
 * By default, fields have no unique constraint, so this method is generally called with
 * TRUE to set a unique constraint.
 *
 * Drivers that support writing unique constraint will advertise the
 * GDAL_DCAP_UNIQUE_FIELDS driver metadata item.
 *
 * This method is the same as the C function OGR_Fld_SetUnique().
 *
 * @param bUniqueIn TRUE if the field must have a unique constraint.
 * @since GDAL 3.2
 */

/************************************************************************/
/*                        OGR_Fld_SetUnique()                          */
/************************************************************************/

/**
 * \brief Set whether this field has a unique constraint.
 *
 * By default, fields have no unique constraint, so this method is generally called with
 * TRUE to set a unique constraint.
 *
 * Drivers that support writing unique constraint will advertise the
 * GDAL_DCAP_UNIQUE_FIELDS driver metadata item.
 *field can receive null values.
 *
 * This method is the same as the C++ method OGRFieldDefn::SetUnique().
 *
 * @param hDefn handle to the field definition
 * @param bUniqueIn TRUE if the field must have a unique constraint.
 * @since GDAL 3.2
 */

void OGR_Fld_SetUnique( OGRFieldDefnH hDefn, int bUniqueIn )
{
    OGRFieldDefn::FromHandle(hDefn)->SetUnique(bUniqueIn);
}

/************************************************************************/
/*                           GetDomainName()                            */
/************************************************************************/

/**
 * \fn const std::string& OGRFieldDefn::GetDomainName() const
 *
 * \brief Return the name of the field domain for this field.
 *
 * By default, none (empty string) is returned.
 *
 * Field domains (OGRFieldDomain class) are attached at the GDALDataset level
 * and should be retrieved with GDALDataset::GetFieldDomain().
 *
 * This method is the same as the C function OGR_Fld_GetDomainName().
 *
 * @return the field domain name, or an empty string if there is none.
 * @since GDAL 3.3
 */

/************************************************************************/
/*                      OGR_Fld_GetDomainName()                         */
/************************************************************************/

/**
 * \brief Return the name of the field domain for this field.
 *
 * By default, none (empty string) is returned.
 *
 * Field domains (OGRFieldDomain class) are attached at the GDALDataset level
 * and should be retrieved with GDALDatasetGetFieldDomain().
 *
 * This method is the same as the C++ method OGRFieldDefn::GetDomainName().
 *
 * @param hDefn handle to the field definition
 * @return the field domain name, or an empty string if there is none.
 * @since GDAL 3.3
 */

const char* OGR_Fld_GetDomainName( OGRFieldDefnH hDefn )
{
    return OGRFieldDefn::FromHandle(hDefn)->GetDomainName().c_str();
}

/************************************************************************/
/*                           SetDomainName()                            */
/************************************************************************/

/**
 * \fn void OGRFieldDefn::SetDomainName( const std:string& osDomainName );
 *
 * \brief Set the name of the field domain for this field.
 *
 * Field domains (OGRFieldDomain) are attached at the GDALDataset level.
 *
 * This method is the same as the C function OGR_Fld_SetDomainName().
 *
 * @param osDomainName Field domain name.
 * @since GDAL 3.3
 */

/************************************************************************/
/*                      OGR_Fld_SetDomainName()                         */
/************************************************************************/

/**
 * \brief Set the name of the field domain for this field.
 *
 * Field domains (OGRFieldDomain) are attached at the GDALDataset level.
 *
 * This method is the same as the C++ method OGRFieldDefn::SetDomainName().
 *
 * @param hDefn handle to the field definition
 * @param pszFieldName Field domain name.
 * @since GDAL 3.3
 */

void OGR_Fld_SetDomainName( OGRFieldDefnH hDefn, const char* pszFieldName )
{
    OGRFieldDefn::FromHandle(hDefn)->SetDomainName(
        pszFieldName ? pszFieldName : "");
}

/************************************************************************/
/*                        OGRUpdateFieldType()                          */
/************************************************************************/

/**
 * \brief Update the type of a field definition by "merging" its existing type
 * with a new type.
 *
 * The update is done such as broadening the type. For example a OFTInteger
 * updated with OFTInteger64 will be promoted to OFTInteger64.
 *
 * @param poFDefn the field definition whose type must be updated.
 * @param eNewType the new field type to merge into the existing type.
 * @param eNewSubType the new field subtype to merge into the existing subtype.
 * @since GDAL 2.1
 */

void OGRUpdateFieldType( OGRFieldDefn* poFDefn,
                         OGRFieldType eNewType,
                         OGRFieldSubType eNewSubType )
{
    OGRFieldType eType = poFDefn->GetType();
    if( eType == OFTInteger )
    {
        if( eNewType == OFTInteger &&
            poFDefn->GetSubType() == OFSTBoolean && eNewSubType != OFSTBoolean )
        {
            poFDefn->SetSubType(OFSTNone);
        }
        else if( eNewType == OFTInteger64 || eNewType == OFTReal )
        {
            poFDefn->SetSubType(OFSTNone);
            poFDefn->SetType(eNewType);
        }
        else if( eNewType == OFTIntegerList || eNewType == OFTInteger64List ||
                 eNewType == OFTRealList || eNewType == OFTStringList )
        {
            if( eNewType != OFTIntegerList || eNewSubType != OFSTBoolean )
                poFDefn->SetSubType(OFSTNone);
            poFDefn->SetType(eNewType);
        }
        else if( eNewType != OFTInteger )
        {
            poFDefn->SetSubType(OFSTNone);
            poFDefn->SetType(OFTString);
        }
    }
    else if( eType == OFTInteger64 )
    {
        if( eNewType == OFTReal )
        {
            poFDefn->SetSubType(OFSTNone);
            poFDefn->SetType(eNewType);
        }
        else if( eNewType == OFTIntegerList )
        {
            poFDefn->SetSubType(OFSTNone);
            poFDefn->SetType(OFTInteger64List);
        }
        else if( eNewType == OFTInteger64List ||
                 eNewType == OFTRealList || eNewType == OFTStringList )
        {
            poFDefn->SetSubType(OFSTNone);
            poFDefn->SetType(eNewType);
        }
        else if( eNewType != OFTInteger && eNewType != OFTInteger64 )
        {
            poFDefn->SetSubType(OFSTNone);
            poFDefn->SetType(OFTString);
        }
    }
    else if( eType == OFTReal )
    {
        if( eNewType == OFTIntegerList || eNewType == OFTInteger64List ||
            eNewType == OFTRealList )
        {
            poFDefn->SetType(OFTRealList);
        }
        else if( eNewType == OFTStringList )
        {
            poFDefn->SetType(OFTStringList);
        }
        else if( eNewType != OFTInteger && eNewType != OFTInteger64 &&
                 eNewType != OFTReal )
        {
            poFDefn->SetSubType(OFSTNone);
            poFDefn->SetType(OFTString);
        }
    }
    else if( eType == OFTIntegerList )
    {
        if( eNewType == OFTIntegerList &&
            poFDefn->GetSubType() == OFSTBoolean && eNewSubType != OFSTBoolean )
        {
            poFDefn->SetSubType(OFSTNone);
        }
        else if( eNewType == OFTInteger64 || eNewType == OFTInteger64List )
        {
            poFDefn->SetSubType(OFSTNone);
            poFDefn->SetType(OFTInteger64List);
        }
        else if( eNewType == OFTReal || eNewType == OFTRealList )
        {
            poFDefn->SetSubType(OFSTNone);
            poFDefn->SetType(OFTRealList);
        }
        else if( eNewType != OFTInteger && eNewType != OFTIntegerList )
        {
            poFDefn->SetSubType(OFSTNone);
            poFDefn->SetType(OFTStringList);
        }
    }
    else if( eType == OFTInteger64List )
    {
        if( eNewType == OFTReal || eNewType == OFTRealList )
            poFDefn->SetType(OFTRealList);
        else if( eNewType != OFTInteger && eNewType != OFTInteger64 &&
                 eNewType != OFTIntegerList && eNewType != OFTInteger64List )
        {
            poFDefn->SetSubType(OFSTNone);
            poFDefn->SetType(OFTStringList);
        }
    }
    else if( eType == OFTRealList )
    {
        if( eNewType != OFTInteger && eNewType != OFTInteger64 &&
            eNewType != OFTReal &&
            eNewType != OFTIntegerList && eNewType != OFTInteger64List &&
            eNewType != OFTRealList )
        {
            poFDefn->SetSubType(OFSTNone);
            poFDefn->SetType(OFTStringList);
        }
    }
    else if( eType == OFTDateTime )
    {
        if( eNewType != OFTDateTime && eNewType != OFTDate )
        {
            poFDefn->SetType(OFTString);
        }
    }
    else if( eType == OFTDate || eType == OFTTime )
    {
        if( eNewType == OFTDateTime )
            poFDefn->SetType(OFTDateTime);
        else if( eNewType != eType )
            poFDefn->SetType(OFTString);
    }
    else if( eType == OFTString && eNewType == OFTStringList )
    {
        poFDefn->SetType(OFTStringList);
    }
}

/************************************************************************/
/*                          OGRFieldDomain()                            */
/************************************************************************/

/*! @cond Doxygen_Suppress */
OGRFieldDomain::OGRFieldDomain(const std::string& osName,
                               const std::string& osDescription,
                               OGRFieldDomainType eDomainType,
                               OGRFieldType eFieldType,
                               OGRFieldSubType eFieldSubType):
    m_osName(osName),
    m_osDescription(osDescription),
    m_eDomainType(eDomainType),
    m_eFieldType(eFieldType),
    m_eFieldSubType(eFieldSubType)
{
}
/*! @endcond */

/************************************************************************/
/*                         ~OGRFieldDomain()                            */
/************************************************************************/

OGRFieldDomain::~OGRFieldDomain() = default;

/************************************************************************/
/*                         ~OGRFieldDomain()                            */
/************************************************************************/

/** Destroy a field domain.
 *
 * This is the same as the C++ method OGRFieldDomain::~OGRFieldDomain()
 *
 * @param hFieldDomain the field domain.
 * @since GDAL 3.3
 */
void OGR_FldDomain_Destroy(OGRFieldDomainH hFieldDomain)
{
    delete OGRFieldDomain::FromHandle(hFieldDomain);
}

/************************************************************************/
/*                        OGRCodedFieldDomain()                         */
/************************************************************************/

OGRCodedFieldDomain::OGRCodedFieldDomain(const std::string& osName,
                                         const std::string& osDescription,
                                         OGRFieldType eFieldType,
                                         OGRFieldSubType eFieldSubType,
                                         std::vector<OGRCodedValue>&& asValues):
    OGRFieldDomain(osName, osDescription, OFDT_CODED, eFieldType, eFieldSubType),
    m_asValues(std::move(asValues))
{
    // Guard
    if( m_asValues.empty() || m_asValues.back().pszCode != nullptr )
    {
        OGRCodedValue cv;
        cv.pszCode = nullptr;
        cv.pszValue = nullptr;
        m_asValues.emplace_back(cv);
    }
}

/************************************************************************/
/*                     OGR_CodedFldDomain_Create()                      */
/************************************************************************/

/** Creates a new coded field domain.
 *
 * This is the same as the C++ method OGRCodedFieldDomain::OGRCodedFieldDomain()
 * (except that the C function copies the enumeration, whereas the C++
 * method moves it)
 *
 * @param pszName        Domain name. Should not be NULL.
 * @param pszDescription Domain description (can be NULL)
 * @param eFieldType     Field type. Generally numeric. Potentially OFTDateTime
 * @param eFieldSubType  Field subtype.
 * @param enumeration    Enumeration as (code, value) pairs. Should not be
 *                       NULL. The end of the enumeration is marked by a code
 *                       set to NULL. The enumeration will be copied.
 *                       Each code should appear only once, but it is the
 *                       responsibility of the user to check it.
 * @return a new handle that should be freed with OGR_FldDomain_Destroy(),
 *         or NULL in case of error.
 * @since GDAL 3.3
 */
OGRFieldDomainH OGR_CodedFldDomain_Create(const char* pszName,
                                          const char* pszDescription,
                                          OGRFieldType eFieldType,
                                          OGRFieldSubType eFieldSubType,
                                          const OGRCodedValue* enumeration)
{
    VALIDATE_POINTER1(pszName, __func__, nullptr);
    VALIDATE_POINTER1(enumeration, __func__, nullptr);
    size_t count = 0;
    for( int i = 0; enumeration[i].pszCode != nullptr; ++i )
    {
        ++count;
    }
    std::vector<OGRCodedValue> asValues;
    try
    {
        asValues.reserve(count + 1);
    }
    catch( const std::exception& e )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
        return nullptr;
    }
    bool error = false;
    for( int i = 0; enumeration[i].pszCode != nullptr; ++i )
    {
        OGRCodedValue cv;
        cv.pszCode = VSI_STRDUP_VERBOSE(enumeration[i].pszCode);
        if( cv.pszCode == nullptr )
        {
            error = true;
            break;
        }
        if( enumeration[i].pszValue )
        {
            cv.pszValue = VSI_STRDUP_VERBOSE(enumeration[i].pszValue);
            if( cv.pszValue == nullptr )
            {
                VSIFree(cv.pszCode);
                error = true;
                break;
            }
        }
        else
        {
            cv.pszValue = nullptr;
        }
        asValues.emplace_back(cv);
    }
    if( error )
    {
        for( auto& cv: asValues )
        {
            VSIFree(cv.pszCode);
            VSIFree(cv.pszValue);
        }
        return nullptr;
    }
    // Add guard
    {
        OGRCodedValue cv;
        cv.pszCode = nullptr;
        cv.pszValue = nullptr;
        asValues.emplace_back(cv);
    }
    return OGRFieldDomain::ToHandle(new OGRCodedFieldDomain(
                                       pszName,
                                       pszDescription ? pszDescription : "",
                                       eFieldType,
                                       eFieldSubType,
                                       std::move(asValues)));
}

/************************************************************************/
/*                   OGRCodedFieldDomain::Clone()                       */
/************************************************************************/

OGRCodedFieldDomain* OGRCodedFieldDomain::Clone() const
{
    return cpl::down_cast<OGRCodedFieldDomain*>(
            OGRFieldDomain::FromHandle(OGR_CodedFldDomain_Create(
                m_osName.c_str(), m_osDescription.c_str(),
                m_eFieldType, m_eFieldSubType, m_asValues.data())));
}

/************************************************************************/
/*                      ~OGRCodedFieldDomain()                          */
/************************************************************************/

OGRCodedFieldDomain::~OGRCodedFieldDomain()
{
    for( auto& cv: m_asValues )
    {
        CPLFree(cv.pszCode);
        CPLFree(cv.pszValue);
    }
}

/************************************************************************/
/*                       OGRRangeFieldDomain()                          */
/************************************************************************/

OGRRangeFieldDomain::OGRRangeFieldDomain(const std::string& osName,
                                         const std::string& osDescription,
                                         OGRFieldType eFieldType,
                                         OGRFieldSubType eFieldSubType,
                                         const OGRField& sMin,
                                         bool        bMinIsInclusive,
                                         const OGRField& sMax,
                                         bool        bMaxIsInclusive):
    OGRFieldDomain(osName, osDescription, OFDT_RANGE, eFieldType, eFieldSubType),
    m_sMin(sMin),
    m_sMax(sMax),
    m_bMinIsInclusive(bMinIsInclusive),
    m_bMaxIsInclusive(bMaxIsInclusive)
{
    CPLAssert( eFieldType == OFTInteger ||
               eFieldType == OFTInteger64 ||
               eFieldType == OFTReal ||
               eFieldType == OFTDateTime );
}

/************************************************************************/
/*                     OGR_RangeFldDomain_Create()                      */
/************************************************************************/

static OGRField GetUnsetField()
{
    OGRField sUnset;
    OGR_RawField_SetUnset(&sUnset);
    return sUnset;
}

/** Creates a new range field domain.
 *
 * This is the same as the C++ method OGRRangeFieldDomain::OGRRangeFieldDomain().
 *
 * @param pszName        Domain name. Should not be NULL.
 * @param pszDescription Domain description (can be NULL)
 * @param eFieldType     Field type. Among OFTInteger, OFTInteger64, OFTReal
 *                       and OFTDateTime.
 * @param eFieldSubType  Field subtype.
 * @param psMin          Minimum value (can be NULL). The member in the union
 *                       that is read is consistent with eFieldType
 * @param bMinIsInclusive Whether the minimum value is included in the range.
 * @param psMax          Maximum value (can be NULL). The member in the union
 *                       that is read is consistent with eFieldType
 * @param bMaxIsInclusive Whether the maximum value is included in the range.
 * @return a new handle that should be freed with OGR_FldDomain_Destroy()
 * @since GDAL 3.3
 */
OGRFieldDomainH OGR_RangeFldDomain_Create(const char* pszName,
                                          const char* pszDescription,
                                          OGRFieldType eFieldType,
                                          OGRFieldSubType eFieldSubType,
                                          const OGRField* psMin,
                                          bool bMinIsInclusive,
                                          const OGRField* psMax,
                                          bool bMaxIsInclusive)
{
    VALIDATE_POINTER1(pszName, __func__, nullptr);
    if( eFieldType != OFTInteger &&
        eFieldType != OFTInteger64 &&
        eFieldType != OFTReal &&
        eFieldType != OFTDateTime )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported field type");
        return nullptr;
    }
    const OGRField unsetField = GetUnsetField();
    return OGRFieldDomain::ToHandle(new OGRRangeFieldDomain(
                                       pszName,
                                       pszDescription ? pszDescription : "",
                                       eFieldType,
                                       eFieldSubType,
                                       psMin ? *psMin : unsetField,
                                       bMinIsInclusive,
                                       psMax ? *psMax : unsetField,
                                       bMaxIsInclusive));
}

/************************************************************************/
/*                        OGRGlobFieldDomain()                          */
/************************************************************************/

OGRGlobFieldDomain::OGRGlobFieldDomain(const std::string& osName,
                                        const std::string& osDescription,
                                        OGRFieldType eFieldType,
                                        OGRFieldSubType eFieldSubType,
                                        const std::string& osGlob):
    OGRFieldDomain(osName, osDescription, OFDT_GLOB, eFieldType, eFieldSubType),
    m_osGlob(osGlob)
{
}

/************************************************************************/
/*                       OGR_GlobFldDomain_Create()                     */
/************************************************************************/

/** Creates a new blob field domain.
 *
 * This is the same as the C++ method OGRGlobFieldDomain::OGRGlobFieldDomain()
 *
 * @param pszName        Domain name. Should not be NULL.
 * @param pszDescription Domain description (can be NULL)
 * @param eFieldType     Field type.
 * @param eFieldSubType  Field subtype.
 * @param pszGlob        Glob expression. Should not be NULL.
 * @return a new handle that should be freed with OGR_FldDomain_Destroy()
 * @since GDAL 3.3
 */
OGRFieldDomainH OGR_GlobFldDomain_Create(const char* pszName,
                                         const char* pszDescription,
                                         OGRFieldType eFieldType,
                                         OGRFieldSubType eFieldSubType,
                                         const char* pszGlob)
{
    VALIDATE_POINTER1(pszName, __func__, nullptr);
    VALIDATE_POINTER1(pszGlob, __func__, nullptr);
    return OGRFieldDomain::ToHandle(new OGRGlobFieldDomain(
                                       pszName,
                                       pszDescription ? pszDescription : "",
                                       eFieldType,
                                       eFieldSubType,
                                       pszGlob));
}

/************************************************************************/
/*                       OGR_FldDomain_GetName()                        */
/************************************************************************/

/** Get the name of the field domain.
 *
 * This is the same as the C++ method OGRFieldDomain::GetName()
 *
 * @param hFieldDomain Field domain handle.
 * @return the field domain name.
 * @since GDAL 3.3
 */
const char* OGR_FldDomain_GetName(OGRFieldDomainH hFieldDomain)
{
    return OGRFieldDomain::FromHandle(hFieldDomain)->GetName().c_str();
}

/************************************************************************/
/*                     OGR_FldDomain_GetDescription()                   */
/************************************************************************/

/** Get the description of the field domain.
 *
 * This is the same as the C++ method OGRFieldDomain::GetDescription()
 *
 * @param hFieldDomain Field domain handle.
 * @return the field domain description (might be empty string).
 * @since GDAL 3.3
 */
const char* OGR_FldDomain_GetDescription(OGRFieldDomainH hFieldDomain)
{
    return OGRFieldDomain::FromHandle(hFieldDomain)->GetDescription().c_str();
}

/************************************************************************/
/*                     OGR_FldDomain_GetDomainType()                    */
/************************************************************************/

/** Get the type of the field domain.
 *
 * This is the same as the C++ method OGRFieldDomain::GetDomainType()
 *
 * @param hFieldDomain Field domain handle.
 * @return the type of the field domain.
 * @since GDAL 3.3
 */
OGRFieldDomainType OGR_FldDomain_GetDomainType(OGRFieldDomainH hFieldDomain)
{
    return OGRFieldDomain::FromHandle(hFieldDomain)->GetDomainType();
}

/************************************************************************/
/*                     OGR_FldDomain_GetFieldType()                     */
/************************************************************************/

/** Get the field type of the field domain.
 *
 * This is the same as the C++ method OGRFieldDomain::GetFieldType()
 *
 * @param hFieldDomain Field domain handle.
 * @return the field type of the field domain.
 * @since GDAL 3.3
 */
OGRFieldType OGR_FldDomain_GetFieldType(OGRFieldDomainH hFieldDomain)
{
    return OGRFieldDomain::FromHandle(hFieldDomain)->GetFieldType();
}

/************************************************************************/
/*                   OGR_FldDomain_GetFieldSubType()                    */
/************************************************************************/

/** Get the field subtype of the field domain.
 *
 * This is the same as OGRFieldDomain::GetFieldSubType()
 *
 * @param hFieldDomain Field domain handle.
 * @return the field subtype of the field domain.
 * @since GDAL 3.3
 */
OGRFieldSubType OGR_FldDomain_GetFieldSubType(OGRFieldDomainH hFieldDomain)
{
    return OGRFieldDomain::FromHandle(hFieldDomain)->GetFieldSubType();
}

/************************************************************************/
/*                    OGR_FldDomain_GetSplitPolicy()                    */
/************************************************************************/

/** Get the split policy of the field domain.
 *
 * This is the same as the C++ method OGRFieldDomain::GetSplitPolicy()
 *
 * @param hFieldDomain Field domain handle.
 * @return the split policy of the field domain.
 * @since GDAL 3.3
 */

OGRFieldDomainSplitPolicy OGR_FldDomain_GetSplitPolicy(OGRFieldDomainH hFieldDomain)
{
    return OGRFieldDomain::FromHandle(hFieldDomain)->GetSplitPolicy();
}

/************************************************************************/
/*                    OGR_FldDomain_SetSplitPolicy()                    */
/************************************************************************/

/** Set the split policy of the field domain.
 *
 * This is the same as the C++ method OGRFieldDomain::SetSplitPolicy()
 *
 * @param hFieldDomain Field domain handle.
 * @param policy the split policy of the field domain.
 * @since GDAL 3.3
 */

void OGR_FldDomain_SetSplitPolicy(OGRFieldDomainH hFieldDomain,
                                  OGRFieldDomainSplitPolicy policy)
{
    OGRFieldDomain::FromHandle(hFieldDomain)->SetSplitPolicy(policy);
}

/************************************************************************/
/*                    OGR_FldDomain_GetMergePolicy()                    */
/************************************************************************/

/** Get the split policy of the field domain.
 *
 * This is the same as the C++ method OGRFieldDomain::GetMergePolicy()
 *
 * @param hFieldDomain Field domain handle.
 * @return the split policy of the field domain.
 * @since GDAL 3.3
 */

OGRFieldDomainMergePolicy OGR_FldDomain_GetMergePolicy(OGRFieldDomainH hFieldDomain)
{
    return OGRFieldDomain::FromHandle(hFieldDomain)->GetMergePolicy();
}

/************************************************************************/
/*                    OGR_FldDomain_SetMergePolicy()                    */
/************************************************************************/

/** Set the split policy of the field domain.
 *
 * This is the same as the C++ method OGRFieldDomain::SetMergePolicy()
 *
 * @param hFieldDomain Field domain handle.
 * @param policy the split policy of the field domain.
 * @since GDAL 3.3
 */

void OGR_FldDomain_SetMergePolicy(OGRFieldDomainH hFieldDomain,
                                  OGRFieldDomainMergePolicy policy)
{
    OGRFieldDomain::FromHandle(hFieldDomain)->SetMergePolicy(policy);
}

/************************************************************************/
/*                  OGR_CodedFldDomain_GetEnumeration()                 */
/************************************************************************/

/** Get the enumeration as (code, value) pairs.
 *
 * The end of the enumeration is signaled by code == NULL
 *
 * This is the same as the C++ method OGRCodedFieldDomain::GetEnumeration()
 *
 * @param hFieldDomain Field domain handle.
 * @return the (code, value) pairs, or nullptr in case of error.
 * @since GDAL 3.3
 */
const OGRCodedValue* OGR_CodedFldDomain_GetEnumeration(OGRFieldDomainH hFieldDomain)
{
    // The user should normally only call us with the right object type, but
    // it doesn't hurt to check.
    auto poFieldDomain = dynamic_cast<const OGRCodedFieldDomain*>(
                                OGRFieldDomain::FromHandle(hFieldDomain));
    if( !poFieldDomain )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "This function should be called with a coded field domain object");
        return nullptr;
    }
    return poFieldDomain->GetEnumeration();
}

/************************************************************************/
/*                     OGR_RangeFldDomain_GetMin()                      */
/************************************************************************/

/** Get the minimum value.
 *
 * Which member in the returned OGRField enum must be read depends on the field type.
 *
 * If no minimum value is set, the OGR_RawField_IsUnset() will return true when
 * called on the result.
 *
 * This is the same as the C++ method OGRRangeFieldDomain::GetMin()
 *
 * @param hFieldDomain Field domain handle.
 * @param pbIsInclusiveOut set to true if the minimum is included in the range.
 * @return the minimum value.
 * @since GDAL 3.3
 */
const OGRField *OGR_RangeFldDomain_GetMin(OGRFieldDomainH hFieldDomain,
                                          bool* pbIsInclusiveOut)
{
    // The user should normally only call us with the right object type, but
    // it doesn't hurt to check.
    auto poFieldDomain = dynamic_cast<const OGRRangeFieldDomain*>(
                                OGRFieldDomain::FromHandle(hFieldDomain));
    if( !poFieldDomain )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "This function should be called with a range field domain object");
        static const OGRField dummyField = GetUnsetField();
        return &dummyField;
    }
    bool bIsInclusive = false;
    const auto& ret = poFieldDomain->GetMin(bIsInclusive);
    if( pbIsInclusiveOut )
        *pbIsInclusiveOut = bIsInclusive;
    return &ret;
}

/************************************************************************/
/*                     OGR_RangeFldDomain_GetMax()                      */
/************************************************************************/

/** Get the maximum value.
 *
 * Which member in the returned OGRField enum must be read depends on the field type.
 *
 * If no maximum value is set, the OGR_RawField_IsUnset() will return true when
 * called on the result.
 *
 * This is the same as the C++ method OGRRangeFieldDomain::GetMax()
 *
 * @param hFieldDomain Field domain handle.
 * @param pbIsInclusiveOut set to true if the maximum is included in the range.
 * @return the maximum value.
 * @since GDAL 3.3
 */
const OGRField *OGR_RangeFldDomain_GetMax(OGRFieldDomainH hFieldDomain,
                                          bool* pbIsInclusiveOut)
{
    // The user should normally only call us with the right object type, but
    // it doesn't hurt to check.
    auto poFieldDomain = dynamic_cast<const OGRRangeFieldDomain*>(
                                OGRFieldDomain::FromHandle(hFieldDomain));
    if( !poFieldDomain )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "This function should be called with a range field domain object");
        static const OGRField dummyField = GetUnsetField();
        return &dummyField;
    }
    bool bIsInclusive = false;
    const auto& ret = poFieldDomain->GetMax(bIsInclusive);
    if( pbIsInclusiveOut )
        *pbIsInclusiveOut = bIsInclusive;
    return &ret;
}

/************************************************************************/
/*                     OGR_GlobFldDomain_GetGlob()                      */
/************************************************************************/

/** Get the glob expression.
 *
 * This is the same as the C++ method OGRGlobFieldDomain::GetGlob()
 *
 * @param hFieldDomain Field domain handle.
 * @return the glob expression, or nullptr in case of error
 * @since GDAL 3.3
 */
const char *OGR_GlobFldDomain_GetGlob(OGRFieldDomainH hFieldDomain)
{
    // The user should normally only call us with the right object type, but
    // it doesn't hurt to check.
    auto poFieldDomain = dynamic_cast<const OGRGlobFieldDomain*>(
                                OGRFieldDomain::FromHandle(hFieldDomain));
    if( !poFieldDomain )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "This function should be called with a glob field domain object");
        return nullptr;
    }
    return poFieldDomain->GetGlob().c_str();
}

