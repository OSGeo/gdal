/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The generic portions of the OGRSFLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogrsf_frmts.h"
#include "ogr_api.h"
#include "ogr_p.h"
#include "ogr_attrind.h"
#include "ogr_swq.h"
#include "ograpispy.h"
#include "ogr_wkb.h"
#include "ogrlayer_private.h"

#include "cpl_time.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <memory>
#include <set>

/************************************************************************/
/*                              OGRLayer()                              */
/************************************************************************/

OGRLayer::OGRLayer()
    : m_poPrivate(new Private()), m_bFilterIsEnvelope(FALSE),
      m_poFilterGeom(nullptr),
      m_pPreparedFilterGeom(nullptr), m_sFilterEnvelope{},
      m_iGeomFieldFilter(0), m_poStyleTable(nullptr), m_poAttrQuery(nullptr),
      m_pszAttrQueryString(nullptr), m_poAttrIndex(nullptr), m_nRefCount(0),
      m_nFeaturesRead(0)
{
}

/************************************************************************/
/*                             ~OGRLayer()                              */
/************************************************************************/

OGRLayer::~OGRLayer()

{
    if (m_poStyleTable)
    {
        delete m_poStyleTable;
        m_poStyleTable = nullptr;
    }

    if (m_poAttrIndex != nullptr)
    {
        delete m_poAttrIndex;
        m_poAttrIndex = nullptr;
    }

    if (m_poAttrQuery != nullptr)
    {
        delete m_poAttrQuery;
        m_poAttrQuery = nullptr;
    }

    CPLFree(m_pszAttrQueryString);

    if (m_poFilterGeom)
    {
        delete m_poFilterGeom;
        m_poFilterGeom = nullptr;
    }

    if (m_pPreparedFilterGeom != nullptr)
    {
        OGRDestroyPreparedGeometry(m_pPreparedFilterGeom);
        m_pPreparedFilterGeom = nullptr;
    }

    if (m_poSharedArrowArrayStreamPrivateData != nullptr)
    {
        m_poSharedArrowArrayStreamPrivateData->m_poLayer = nullptr;
    }
}

/************************************************************************/
/*                             Reference()                              */
/************************************************************************/

/**
\brief Increment layer reference count.

This method is the same as the C function OGR_L_Reference().

@return the reference count after incrementing.
*/
int OGRLayer::Reference()

{
    return ++m_nRefCount;
}

/************************************************************************/
/*                          OGR_L_Reference()                           */
/************************************************************************/

int OGR_L_Reference(OGRLayerH hLayer)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_Reference", 0);

    return OGRLayer::FromHandle(hLayer)->Reference();
}

/************************************************************************/
/*                            Dereference()                             */
/************************************************************************/

/**
\brief Decrement layer reference count.

This method is the same as the C function OGR_L_Dereference().

@return the reference count after decrementing.
*/

int OGRLayer::Dereference()

{
    return --m_nRefCount;
}

/************************************************************************/
/*                         OGR_L_Dereference()                          */
/************************************************************************/

int OGR_L_Dereference(OGRLayerH hLayer)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_Dereference", 0);

    return OGRLayer::FromHandle(hLayer)->Dereference();
}

/************************************************************************/
/*                            GetRefCount()                             */
/************************************************************************/

/**
\brief Fetch reference count.

This method is the same as the C function OGR_L_GetRefCount().

@return the current reference count for the layer object itself.
*/

int OGRLayer::GetRefCount() const

{
    return m_nRefCount;
}

/************************************************************************/
/*                         OGR_L_GetRefCount()                          */
/************************************************************************/

int OGR_L_GetRefCount(OGRLayerH hLayer)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_GetRefCount", 0);

    return OGRLayer::FromHandle(hLayer)->GetRefCount();
}

/************************************************************************/
/*                         GetFeatureCount()                            */
/************************************************************************/

/**
 \brief Fetch the feature count in this layer.

 Returns the number of features in the layer.  For dynamic databases the
 count may not be exact.  If bForce is FALSE, and it would be expensive
 to establish the feature count a value of -1 may be returned indicating
 that the count isn't know.  If bForce is TRUE some implementations will
 actually scan the entire layer once to count objects.

 The returned count takes the spatial filter into account.

 Note that some implementations of this method may alter the read cursor
 of the layer.

 This method is the same as the C function OGR_L_GetFeatureCount().


 @param bForce Flag indicating whether the count should be computed even
 if it is expensive.

 @return feature count, -1 if count not known.
*/

GIntBig OGRLayer::GetFeatureCount(int bForce)

{
    if (!bForce)
        return -1;

    GIntBig nFeatureCount = 0;
    for (auto &&poFeature : *this)
    {
        CPL_IGNORE_RET_VAL(poFeature.get());
        nFeatureCount++;
    }
    ResetReading();

    return nFeatureCount;
}

/************************************************************************/
/*                      OGR_L_GetFeatureCount()                         */
/************************************************************************/

/**
 \brief Fetch the feature count in this layer.

 Returns the number of features in the layer.  For dynamic databases the
 count may not be exact.  If bForce is FALSE, and it would be expensive
 to establish the feature count a value of -1 may be returned indicating
 that the count isn't know.  If bForce is TRUE some implementations will
 actually scan the entire layer once to count objects.

 The returned count takes the spatial filter into account.

 Note that some implementations of this method may alter the read cursor
 of the layer.

 This function is the same as the CPP OGRLayer::GetFeatureCount().


 @param hLayer handle to the layer that owned the features.
 @param bForce Flag indicating whether the count should be computed even
 if it is expensive.

 @return feature count, -1 if count not known.
*/

GIntBig OGR_L_GetFeatureCount(OGRLayerH hLayer, int bForce)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_GetFeatureCount", 0);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_GetFeatureCount(hLayer, bForce);
#endif

    return OGRLayer::FromHandle(hLayer)->GetFeatureCount(bForce);
}

/************************************************************************/
/*                            GetExtent()                               */
/************************************************************************/

/**
 \brief Fetch the extent of this layer.

 Returns the extent (MBR) of the data in the layer.  If bForce is FALSE,
 and it would be expensive to establish the extent then OGRERR_FAILURE
 will be returned indicating that the extent isn't know.  If bForce is
 TRUE then some implementations will actually scan the entire layer once
 to compute the MBR of all the features in the layer.

 Depending on the drivers, the returned extent may or may not take the
 spatial filter into account.  So it is safer to call GetExtent() without
 setting a spatial filter.

 Layers without any geometry may return OGRERR_FAILURE just indicating that
 no meaningful extents could be collected.

 Note that some implementations of this method may alter the read cursor
 of the layer.

 This method is the same as the C function OGR_L_GetExtent().

 @param psExtent the structure in which the extent value will be returned.
 @param bForce Flag indicating whether the extent should be computed even
 if it is expensive.

 @return OGRERR_NONE on success, OGRERR_FAILURE if extent not known.
*/

OGRErr OGRLayer::GetExtent(OGREnvelope *psExtent, bool bForce)
{
    return GetExtent(0, psExtent, bForce);
}

/**
 \brief Fetch the extent of this layer, on the specified geometry field.

 Returns the extent (MBR) of the data in the layer.  If bForce is FALSE,
 and it would be expensive to establish the extent then OGRERR_FAILURE
 will be returned indicating that the extent isn't know.  If bForce is
 TRUE then some implementations will actually scan the entire layer once
 to compute the MBR of all the features in the layer.

 Depending on the drivers, the returned extent may or may not take the
 spatial filter into account.  So it is safer to call GetExtent() without
 setting a spatial filter.

 Layers without any geometry may return OGRERR_FAILURE just indicating that
 no meaningful extents could be collected.

 Note that some implementations of this method may alter the read cursor
 of the layer.

 This method is the same as the C function OGR_L_GetExtentEx().

 @param iGeomField the index of the geometry field on which to compute the extent.
 @param psExtent the structure in which the extent value will be returned.
 @param bForce Flag indicating whether the extent should be computed even
 if it is expensive.

 @return OGRERR_NONE on success, OGRERR_FAILURE if extent not known.

*/

OGRErr OGRLayer::GetExtent(int iGeomField, OGREnvelope *psExtent, bool bForce)
{
    psExtent->MinX = 0.0;
    psExtent->MaxX = 0.0;
    psExtent->MinY = 0.0;
    psExtent->MaxY = 0.0;

    /* -------------------------------------------------------------------- */
    /*      If this layer has a none geometry type, then we can             */
    /*      reasonably assume there are not extents available.              */
    /* -------------------------------------------------------------------- */
    if (iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount() ||
        GetLayerDefn()->GetGeomFieldDefn(iGeomField)->GetType() == wkbNone)
    {
        if (iGeomField != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid geometry field index : %d", iGeomField);
        }
        return OGRERR_FAILURE;
    }

    return IGetExtent(iGeomField, psExtent, bForce);
}

/************************************************************************/
/*                            IGetExtent()                              */
/************************************************************************/

/**
 \brief Fetch the extent of this layer, on the specified geometry field.

 Virtual method implemented by drivers since 3.11. In previous versions,
 GetExtent() itself was the virtual method.

 Driver implementations, when wanting to call the base method, must take
 care of calling OGRLayer::IGetExtent() (and note the public method without
 the leading I).

 @param iGeomField 0-based index of the geometry field to consider.
 @param psExtent the computed extent of the layer.
 @param bForce if TRUE, the extent will be computed even if all the
        layer features have to be fetched.
 @return OGRERR_NONE on success or an error code in case of failure.
 @since GDAL 3.11
*/

OGRErr OGRLayer::IGetExtent(int iGeomField, OGREnvelope *psExtent, bool bForce)

{
    /* -------------------------------------------------------------------- */
    /*      If not forced, we should avoid having to scan all the           */
    /*      features and just return a failure.                             */
    /* -------------------------------------------------------------------- */
    if (!bForce)
        return OGRERR_FAILURE;

    /* -------------------------------------------------------------------- */
    /*      OK, we hate to do this, but go ahead and read through all       */
    /*      the features to collect geometries and build extents.           */
    /* -------------------------------------------------------------------- */
    OGREnvelope oEnv;
    bool bExtentSet = false;

    for (auto &&poFeature : *this)
    {
        OGRGeometry *poGeom = poFeature->GetGeomFieldRef(iGeomField);
        if (poGeom == nullptr || poGeom->IsEmpty())
        {
            /* Do nothing */
        }
        else if (!bExtentSet)
        {
            poGeom->getEnvelope(psExtent);
            if (!(std::isnan(psExtent->MinX) || std::isnan(psExtent->MinY) ||
                  std::isnan(psExtent->MaxX) || std::isnan(psExtent->MaxY)))
            {
                bExtentSet = true;
            }
        }
        else
        {
            poGeom->getEnvelope(&oEnv);
            if (oEnv.MinX < psExtent->MinX)
                psExtent->MinX = oEnv.MinX;
            if (oEnv.MinY < psExtent->MinY)
                psExtent->MinY = oEnv.MinY;
            if (oEnv.MaxX > psExtent->MaxX)
                psExtent->MaxX = oEnv.MaxX;
            if (oEnv.MaxY > psExtent->MaxY)
                psExtent->MaxY = oEnv.MaxY;
        }
    }
    ResetReading();

    return bExtentSet ? OGRERR_NONE : OGRERR_FAILURE;
}

/************************************************************************/
/*                          OGR_L_GetExtent()                           */
/************************************************************************/

/**
 \brief Fetch the extent of this layer.

 Returns the extent (MBR) of the data in the layer.  If bForce is FALSE,
 and it would be expensive to establish the extent then OGRERR_FAILURE
 will be returned indicating that the extent isn't know.  If bForce is
 TRUE then some implementations will actually scan the entire layer once
 to compute the MBR of all the features in the layer.

 Depending on the drivers, the returned extent may or may not take the
 spatial filter into account.  So it is safer to call OGR_L_GetExtent() without
 setting a spatial filter.

 Layers without any geometry may return OGRERR_FAILURE just indicating that
 no meaningful extents could be collected.

 Note that some implementations of this method may alter the read cursor
 of the layer.

 This function is the same as the C++ method OGRLayer::GetExtent().

 @param hLayer handle to the layer from which to get extent.
 @param psExtent the structure in which the extent value will be returned.
 @param bForce Flag indicating whether the extent should be computed even
 if it is expensive.

 @return OGRERR_NONE on success, OGRERR_FAILURE if extent not known.

*/

OGRErr OGR_L_GetExtent(OGRLayerH hLayer, OGREnvelope *psExtent, int bForce)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_GetExtent", OGRERR_INVALID_HANDLE);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_GetExtent(hLayer, bForce);
#endif

    return OGRLayer::FromHandle(hLayer)->GetExtent(0, psExtent,
                                                   bForce != FALSE);
}

/************************************************************************/
/*                         OGR_L_GetExtentEx()                          */
/************************************************************************/

/**
 \brief Fetch the extent of this layer, on the specified geometry field.

 Returns the extent (MBR) of the data in the layer.  If bForce is FALSE,
 and it would be expensive to establish the extent then OGRERR_FAILURE
 will be returned indicating that the extent isn't know.  If bForce is
 TRUE then some implementations will actually scan the entire layer once
 to compute the MBR of all the features in the layer.

 Depending on the drivers, the returned extent may or may not take the
 spatial filter into account.  So it is safer to call OGR_L_GetExtent() without
 setting a spatial filter.

 Layers without any geometry may return OGRERR_FAILURE just indicating that
 no meaningful extents could be collected.

 Note that some implementations of this method may alter the read cursor
 of the layer.

 This function is the same as the C++ method OGRLayer::GetExtent().

 @param hLayer handle to the layer from which to get extent.
 @param iGeomField the index of the geometry field on which to compute the extent.
 @param psExtent the structure in which the extent value will be returned.
 @param bForce Flag indicating whether the extent should be computed even
 if it is expensive.

 @return OGRERR_NONE on success, OGRERR_FAILURE if extent not known.

*/
OGRErr OGR_L_GetExtentEx(OGRLayerH hLayer, int iGeomField,
                         OGREnvelope *psExtent, int bForce)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_GetExtentEx", OGRERR_INVALID_HANDLE);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_GetExtentEx(hLayer, iGeomField, bForce);
#endif

    return OGRLayer::FromHandle(hLayer)->GetExtent(iGeomField, psExtent,
                                                   bForce != FALSE);
}

/************************************************************************/
/*                            GetExtent3D()                             */
/************************************************************************/

/**
 \brief Fetch the 3D extent of this layer, on the specified geometry field.

 Returns the 3D extent (MBR) of the data in the layer.  If bForce is FALSE,
 and it would be expensive to establish the extent then OGRERR_FAILURE
 will be returned indicating that the extent isn't know.  If bForce is
 TRUE then some implementations will actually scan the entire layer once
 to compute the MBR of all the features in the layer.

 (Contrary to GetExtent() 2D), the returned extent will always take into
 account the attribute and spatial filters that may be installed.

 Layers without any geometry may return OGRERR_FAILURE just indicating that
 no meaningful extents could be collected.

 For layers that have no 3D geometries, the psExtent3D->MinZ and psExtent3D->MaxZ
 fields will be respectively set to +Infinity and -Infinity.

 Note that some implementations of this method may alter the read cursor
 of the layer.

 This function is the same as the C function OGR_L_GetExtent3D().

 @param iGeomField 0-based index of the geometry field to consider.
 @param psExtent3D the computed 3D extent of the layer.
 @param bForce if TRUE, the extent will be computed even if all the
        layer features have to be fetched.
 @return OGRERR_NONE on success or an error code in case of failure.
 @since GDAL 3.9
*/

OGRErr OGRLayer::GetExtent3D(int iGeomField, OGREnvelope3D *psExtent3D,
                             bool bForce)

{
    psExtent3D->MinX = 0.0;
    psExtent3D->MaxX = 0.0;
    psExtent3D->MinY = 0.0;
    psExtent3D->MaxY = 0.0;
    psExtent3D->MinZ = std::numeric_limits<double>::infinity();
    psExtent3D->MaxZ = -std::numeric_limits<double>::infinity();

    /* -------------------------------------------------------------------- */
    /*      If this layer has a none geometry type, then we can             */
    /*      reasonably assume there are not extents available.              */
    /* -------------------------------------------------------------------- */
    if (iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount() ||
        GetLayerDefn()->GetGeomFieldDefn(iGeomField)->GetType() == wkbNone)
    {
        if (iGeomField != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid geometry field index : %d", iGeomField);
        }
        return OGRERR_FAILURE;
    }

    return IGetExtent3D(iGeomField, psExtent3D, bForce);
}

/************************************************************************/
/*                           IGetExtent3D()                             */
/************************************************************************/

/**
 \brief Fetch the 3D extent of this layer, on the specified geometry field.

 See GetExtent3D() documentation.

 Virtual method implemented by drivers since 3.11. In previous versions,
 GetExtent3D() itself was the virtual method.

 Driver implementations, when wanting to call the base method, must take
 care of calling OGRLayer::IGetExtent3D() (and note the public method without
 the leading I).

 @param iGeomField 0-based index of the geometry field to consider.
 @param psExtent3D the computed 3D extent of the layer.
 @param bForce if TRUE, the extent will be computed even if all the
        layer features have to be fetched.
 @return OGRERR_NONE on success or an error code in case of failure.
 @since GDAL 3.11
*/

OGRErr OGRLayer::IGetExtent3D(int iGeomField, OGREnvelope3D *psExtent3D,
                              bool bForce)

{
    /* -------------------------------------------------------------------- */
    /*      If not forced, we should avoid having to scan all the           */
    /*      features and just return a failure.                             */
    /* -------------------------------------------------------------------- */
    if (!bForce)
        return OGRERR_FAILURE;

    /* -------------------------------------------------------------------- */
    /*      OK, we hate to do this, but go ahead and read through all       */
    /*      the features to collect geometries and build extents.           */
    /* -------------------------------------------------------------------- */
    OGREnvelope3D oEnv;
    bool bExtentSet = false;

    for (auto &&poFeature : *this)
    {
        OGRGeometry *poGeom = poFeature->GetGeomFieldRef(iGeomField);
        if (poGeom == nullptr || poGeom->IsEmpty())
        {
            /* Do nothing */
        }
        else if (!bExtentSet)
        {
            poGeom->getEnvelope(psExtent3D);
            // This is required because getEnvelope initializes Z to 0 for 2D geometries
            if (!poGeom->Is3D())
            {
                psExtent3D->MinZ = std::numeric_limits<double>::infinity();
                psExtent3D->MaxZ = -std::numeric_limits<double>::infinity();
            }
            bExtentSet = true;
        }
        else
        {
            poGeom->getEnvelope(&oEnv);
            // This is required because getEnvelope initializes Z to 0 for 2D geometries
            if (!poGeom->Is3D())
            {
                oEnv.MinZ = std::numeric_limits<double>::infinity();
                oEnv.MaxZ = -std::numeric_limits<double>::infinity();
            }
            // Merge handles infinity correctly
            psExtent3D->Merge(oEnv);
        }
    }
    ResetReading();

    return bExtentSet ? OGRERR_NONE : OGRERR_FAILURE;
}

/************************************************************************/
/*                          OGR_L_GetExtent3D()                         */
/************************************************************************/

/**
 \brief Fetch the 3D extent of this layer, on the specified geometry field.

 Returns the 3D extent (MBR) of the data in the layer.  If bForce is FALSE,
 and it would be expensive to establish the extent then OGRERR_FAILURE
 will be returned indicating that the extent isn't know.  If bForce is
 TRUE then some implementations will actually scan the entire layer once
 to compute the MBR of all the features in the layer.

 (Contrary to GetExtent() 2D), the returned extent will always take into
 account the attribute and spatial filters that may be installed.

 Layers without any geometry may return OGRERR_FAILURE just indicating that
 no meaningful extents could be collected.

 For layers that have no 3D geometries, the psExtent3D->MinZ and psExtent3D->MaxZ
 fields will be respectively set to +Infinity and -Infinity.

 Note that some implementations of this method may alter the read cursor
 of the layer.

 This function is the same as the C++ method OGRLayer::GetExtent3D().

 @param hLayer the layer to consider.
 @param iGeomField 0-based index of the geometry field to consider.
 @param psExtent3D the computed 3D extent of the layer.
 @param bForce if TRUE, the extent will be computed even if all the
        layer features have to be fetched.
 @return OGRERR_NONE on success or an error code in case of failure.
 @since GDAL 3.9
*/

OGRErr OGR_L_GetExtent3D(OGRLayerH hLayer, int iGeomField,
                         OGREnvelope3D *psExtent3D, int bForce)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_GetExtent3D", OGRERR_INVALID_HANDLE);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_GetExtent3D(hLayer, iGeomField, bForce);
#endif

    return OGRLayer::FromHandle(hLayer)->GetExtent3D(iGeomField, psExtent3D,
                                                     bForce != FALSE);
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

/**
 \brief Set a new attribute query.

 This method sets the attribute query string to be used when
 fetching features via the GetNextFeature() method.  Only features for which
 the query evaluates as true will be returned.

 The query string should be in the format of an SQL WHERE clause.  For
 instance "population > 1000000 and population < 5000000" where population
 is an attribute in the layer. The query format is normally a SQL WHERE clause
 as described in the
 <a href="https://gdal.org/user/ogr_sql_dialect.html#where">"WHERE"</a> section
 of the OGR SQL dialect documentation.
 In some cases (RDBMS backed drivers, SQLite, GeoPackage) the native
 capabilities of the database may be used to to interpret the WHERE clause, in
 which case the capabilities will be broader than those of OGR SQL.

 Note that installing a query string will generally result in resetting
 the current reading position (ala ResetReading()).

 This method is the same as the C function OGR_L_SetAttributeFilter().

 @param pszQuery query in restricted SQL WHERE format, or NULL to clear the
 current query.

 @return OGRERR_NONE if successfully installed, or an error code if the
 query expression is in error, or some other failure occurs.
 */

OGRErr OGRLayer::SetAttributeFilter(const char *pszQuery)

{
    CPLFree(m_pszAttrQueryString);
    m_pszAttrQueryString = (pszQuery) ? CPLStrdup(pszQuery) : nullptr;

    /* -------------------------------------------------------------------- */
    /*      Are we just clearing any existing query?                        */
    /* -------------------------------------------------------------------- */
    if (pszQuery == nullptr || strlen(pszQuery) == 0)
    {
        if (m_poAttrQuery)
        {
            delete m_poAttrQuery;
            m_poAttrQuery = nullptr;
            ResetReading();
        }
        return OGRERR_NONE;
    }

    /* -------------------------------------------------------------------- */
    /*      Or are we installing a new query?                               */
    /* -------------------------------------------------------------------- */
    OGRErr eErr;

    if (!m_poAttrQuery)
        m_poAttrQuery = new OGRFeatureQuery();

    eErr = m_poAttrQuery->Compile(this, pszQuery);
    if (eErr != OGRERR_NONE)
    {
        delete m_poAttrQuery;
        m_poAttrQuery = nullptr;
    }

    ResetReading();

    return eErr;
}

/************************************************************************/
/*                        ContainGeomSpecialField()                     */
/************************************************************************/

static int ContainGeomSpecialField(swq_expr_node *expr, int nLayerFieldCount)
{
    if (expr->eNodeType == SNT_COLUMN)
    {
        if (expr->table_index == 0 && expr->field_index != -1)
        {
            int nSpecialFieldIdx = expr->field_index - nLayerFieldCount;
            return nSpecialFieldIdx == SPF_OGR_GEOMETRY ||
                   nSpecialFieldIdx == SPF_OGR_GEOM_WKT ||
                   nSpecialFieldIdx == SPF_OGR_GEOM_AREA;
        }
    }
    else if (expr->eNodeType == SNT_OPERATION)
    {
        for (int i = 0; i < expr->nSubExprCount; i++)
        {
            if (ContainGeomSpecialField(expr->papoSubExpr[i], nLayerFieldCount))
                return TRUE;
        }
    }
    return FALSE;
}

/************************************************************************/
/*                AttributeFilterEvaluationNeedsGeometry()              */
/************************************************************************/

//! @cond Doxygen_Suppress
int OGRLayer::AttributeFilterEvaluationNeedsGeometry()
{
    if (!m_poAttrQuery)
        return FALSE;

    swq_expr_node *expr =
        static_cast<swq_expr_node *>(m_poAttrQuery->GetSWQExpr());
    int nLayerFieldCount = GetLayerDefn()->GetFieldCount();

    return ContainGeomSpecialField(expr, nLayerFieldCount);
}

//! @endcond

/************************************************************************/
/*                      OGR_L_SetAttributeFilter()                      */
/************************************************************************/

/**
 \brief Set a new attribute query.

 This function sets the attribute query string to be used when
 fetching features via the OGR_L_GetNextFeature() function.
 Only features for which the query evaluates as true will be returned.

 The query string should be in the format of an SQL WHERE clause.  For
 instance "population > 1000000 and population < 5000000" where population
 is an attribute in the layer. The query format is normally a SQL WHERE clause
 as described in the
 <a href="https://gdal.org/user/ogr_sql_dialect.html#where">"WHERE"</a> section
 of the OGR SQL dialect documentation.
 In some cases (RDBMS backed drivers, SQLite, GeoPackage) the native
 capabilities of the database may be used to to interpret the WHERE clause, in
 which case the capabilities will be broader than those of OGR SQL.

 Note that installing a query string will generally result in resetting
 the current reading position (ala OGR_L_ResetReading()).

 This function is the same as the C++ method OGRLayer::SetAttributeFilter().

 @param hLayer handle to the layer on which attribute query will be executed.
 @param pszQuery query in restricted SQL WHERE format, or NULL to clear the
 current query.

 @return OGRERR_NONE if successfully installed, or an error code if the
 query expression is in error, or some other failure occurs.
 */

OGRErr OGR_L_SetAttributeFilter(OGRLayerH hLayer, const char *pszQuery)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_SetAttributeFilter",
                      OGRERR_INVALID_HANDLE);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_SetAttributeFilter(hLayer, pszQuery);
#endif

    return OGRLayer::FromHandle(hLayer)->SetAttributeFilter(pszQuery);
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

/**
 \brief Fetch a feature by its identifier.

 This function will attempt to read the identified feature.  The nFID
 value cannot be OGRNullFID.  Success or failure of this operation is
 unaffected by the spatial or attribute filters (and specialized implementations
 in drivers should make sure that they do not take into account spatial or
 attribute filters).

 If this method returns a non-NULL feature, it is guaranteed that its
 feature id (OGRFeature::GetFID()) will be the same as nFID.

 Use OGRLayer::TestCapability(OLCRandomRead) to establish if this layer
 supports efficient random access reading via GetFeature(); however, the
 call should always work if the feature exists as a fallback implementation
 just scans all the features in the layer looking for the desired feature.

 Sequential reads (with GetNextFeature()) are generally considered interrupted
 by a GetFeature() call.

 The returned feature should be free with OGRFeature::DestroyFeature().

 This method is the same as the C function OGR_L_GetFeature().

 @param nFID the feature id of the feature to read.

 @return a feature now owned by the caller, or NULL on failure.
*/

OGRFeature *OGRLayer::GetFeature(GIntBig nFID)

{
    /* Save old attribute and spatial filters */
    char *pszOldFilter =
        m_pszAttrQueryString ? CPLStrdup(m_pszAttrQueryString) : nullptr;
    OGRGeometry *poOldFilterGeom =
        (m_poFilterGeom != nullptr) ? m_poFilterGeom->clone() : nullptr;
    int iOldGeomFieldFilter = m_iGeomFieldFilter;
    /* Unset filters */
    SetAttributeFilter(nullptr);
    SetSpatialFilter(0, nullptr);

    OGRFeatureUniquePtr poFeature;
    for (auto &&poFeatureIter : *this)
    {
        if (poFeatureIter->GetFID() == nFID)
        {
            poFeature.swap(poFeatureIter);
            break;
        }
    }

    /* Restore filters */
    SetAttributeFilter(pszOldFilter);
    CPLFree(pszOldFilter);
    SetSpatialFilter(iOldGeomFieldFilter, poOldFilterGeom);
    delete poOldFilterGeom;

    return poFeature.release();
}

/************************************************************************/
/*                          OGR_L_GetFeature()                          */
/************************************************************************/

/**
 \brief Fetch a feature by its identifier.

 This function will attempt to read the identified feature.  The nFID
 value cannot be OGRNullFID.  Success or failure of this operation is
 unaffected by the spatial or attribute filters (and specialized implementations
 in drivers should make sure that they do not take into account spatial or
 attribute filters).

 If this function returns a non-NULL feature, it is guaranteed that its
 feature id (OGR_F_GetFID()) will be the same as nFID.

 Use OGR_L_TestCapability(OLCRandomRead) to establish if this layer
 supports efficient random access reading via OGR_L_GetFeature(); however,
 the call should always work if the feature exists as a fallback
 implementation just scans all the features in the layer looking for the
 desired feature.

 Sequential reads (with OGR_L_GetNextFeature()) are generally considered interrupted by a
 OGR_L_GetFeature() call.

 The returned feature should be free with OGR_F_Destroy().

 This function is the same as the C++ method OGRLayer::GetFeature( ).

 @param hLayer handle to the layer that owned the feature.
 @param nFeatureId the feature id of the feature to read.

 @return a handle to a feature now owned by the caller, or NULL on failure.
*/

OGRFeatureH OGR_L_GetFeature(OGRLayerH hLayer, GIntBig nFeatureId)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_GetFeature", nullptr);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_GetFeature(hLayer, nFeatureId);
#endif

    return OGRFeature::ToHandle(
        OGRLayer::FromHandle(hLayer)->GetFeature(nFeatureId));
}

/************************************************************************/
/*                           SetNextByIndex()                           */
/************************************************************************/

/**
 \brief Move read cursor to the nIndex'th feature in the current resultset.

 This method allows positioning of a layer such that the GetNextFeature()
 call will read the requested feature, where nIndex is an absolute index
 into the current result set.   So, setting it to 3 would mean the next
 feature read with GetNextFeature() would have been the 4th feature to have
 been read if sequential reading took place from the beginning of the layer,
 including accounting for spatial and attribute filters.

 Only in rare circumstances is SetNextByIndex() efficiently implemented.
 In all other cases the default implementation which calls ResetReading()
 and then calls GetNextFeature() nIndex times is used.  To determine if
 fast seeking is available on the current layer use the TestCapability()
 method with a value of OLCFastSetNextByIndex.

 Starting with GDAL 3.12, when implementations can detect that nIndex is
 invalid (at the minimum all should detect negative indices), they should
 return OGRERR_NON_EXISTING_FEATURE, and following calls to GetNextFeature()
 should return nullptr, until ResetReading() or a valid call to
 SetNextByIndex() is done.

 This method is the same as the C function OGR_L_SetNextByIndex().

 @param nIndex the index indicating how many steps into the result set
 to seek.

 @return OGRERR_NONE on success or an error code.
*/

OGRErr OGRLayer::SetNextByIndex(GIntBig nIndex)

{
    if (nIndex < 0)
        nIndex = GINTBIG_MAX;

    ResetReading();

    while (nIndex-- > 0)
    {
        auto poFeature = std::unique_ptr<OGRFeature>(GetNextFeature());
        if (poFeature == nullptr)
            return OGRERR_NON_EXISTING_FEATURE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                        OGR_L_SetNextByIndex()                        */
/************************************************************************/

/**
 \brief Move read cursor to the nIndex'th feature in the current resultset.

 This method allows positioning of a layer such that the GetNextFeature()
 call will read the requested feature, where nIndex is an absolute index
 into the current result set.   So, setting it to 3 would mean the next
 feature read with GetNextFeature() would have been the 4th feature to have
 been read if sequential reading took place from the beginning of the layer,
 including accounting for spatial and attribute filters.

 Only in rare circumstances is SetNextByIndex() efficiently implemented.
 In all other cases the default implementation which calls ResetReading()
 and then calls GetNextFeature() nIndex times is used.  To determine if
 fast seeking is available on the current layer use the TestCapability()
 method with a value of OLCFastSetNextByIndex.

 Starting with GDAL 3.12, when implementations can detect that nIndex is
 invalid (at the minimum all should detect negative indices), they should
 return OGRERR_NON_EXISTING_FEATURE, and following calls to GetNextFeature()
 should return nullptr, until ResetReading() or a valid call to
 SetNextByIndex() is done.

 This method is the same as the C++ method OGRLayer::SetNextByIndex()

 @param hLayer handle to the layer
 @param nIndex the index indicating how many steps into the result set
 to seek.

 @return OGRERR_NONE on success or an error code.
*/

OGRErr OGR_L_SetNextByIndex(OGRLayerH hLayer, GIntBig nIndex)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_SetNextByIndex", OGRERR_INVALID_HANDLE);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_SetNextByIndex(hLayer, nIndex);
#endif

    return OGRLayer::FromHandle(hLayer)->SetNextByIndex(nIndex);
}

/************************************************************************/
/*                       OGRLayer::GetNextFeature()                     */
/************************************************************************/

/**
 \fn OGRFeature *OGRLayer::GetNextFeature();

 \brief Fetch the next available feature from this layer.

 The returned feature becomes the responsibility of the caller to
 delete with OGRFeature::DestroyFeature(). It is critical that all
 features associated with an OGRLayer (more specifically an
 OGRFeatureDefn) be deleted before that layer/datasource is deleted.

 Only features matching the current spatial filter (set with
 SetSpatialFilter()) will be returned.

 This method implements sequential access to the features of a layer.  The
 ResetReading() method can be used to start at the beginning again.

 Starting with GDAL 3.6, it is possible to retrieve them by batches, with a
 column-oriented memory layout, using the GetArrowStream() method.

 Features returned by GetNextFeature() may or may not be affected by
 concurrent modifications depending on drivers. A guaranteed way of seeing
 modifications in effect is to call ResetReading() on layers where
 GetNextFeature() has been called, before reading again.  Structural changes
 in layers (field addition, deletion, ...) when a read is in progress may or
 may not be possible depending on drivers.  If a transaction is
 committed/aborted, the current sequential reading may or may not be valid
 after that operation and a call to ResetReading() might be needed.

 This method is the same as the C function OGR_L_GetNextFeature().

 @return a feature, or NULL if no more features are available.

*/

/************************************************************************/
/*                        OGR_L_GetNextFeature()                        */
/************************************************************************/

/**
 \brief Fetch the next available feature from this layer.

 The returned feature becomes the responsibility of the caller to
 delete with OGR_F_Destroy().  It is critical that all features
 associated with an OGRLayer (more specifically an OGRFeatureDefn) be
 deleted before that layer/datasource is deleted.

 Only features matching the current spatial filter (set with
 SetSpatialFilter()) will be returned.

 This function implements sequential access to the features of a layer.
 The OGR_L_ResetReading() function can be used to start at the beginning
 again.

 Starting with GDAL 3.6, it is possible to retrieve them by batches, with a
 column-oriented memory layout, using the OGR_L_GetArrowStream() function.

 Features returned by OGR_GetNextFeature() may or may not be affected by
 concurrent modifications depending on drivers. A guaranteed way of seeing
 modifications in effect is to call OGR_L_ResetReading() on layers where
 OGR_GetNextFeature() has been called, before reading again.  Structural
 changes in layers (field addition, deletion, ...) when a read is in progress
 may or may not be possible depending on drivers.  If a transaction is
 committed/aborted, the current sequential reading may or may not be valid
 after that operation and a call to OGR_L_ResetReading() might be needed.

 This function is the same as the C++ method OGRLayer::GetNextFeature().

 @param hLayer handle to the layer from which feature are read.
 @return a handle to a feature, or NULL if no more features are available.

*/

OGRFeatureH OGR_L_GetNextFeature(OGRLayerH hLayer)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_GetNextFeature", nullptr);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_GetNextFeature(hLayer);
#endif

    return OGRFeature::ToHandle(OGRLayer::FromHandle(hLayer)->GetNextFeature());
}

/************************************************************************/
/*                       ConvertGeomsIfNecessary()                      */
/************************************************************************/

void OGRLayer::ConvertGeomsIfNecessary(OGRFeature *poFeature)
{
    if (!m_poPrivate->m_bConvertGeomsIfNecessaryAlreadyCalled)
    {
        // One time initialization
        m_poPrivate->m_bConvertGeomsIfNecessaryAlreadyCalled = true;
        m_poPrivate->m_bSupportsCurve =
            CPL_TO_BOOL(TestCapability(OLCCurveGeometries));
        m_poPrivate->m_bSupportsM =
            CPL_TO_BOOL(TestCapability(OLCMeasuredGeometries));
        if (CPLTestBool(
                CPLGetConfigOption("OGR_APPLY_GEOM_SET_PRECISION", "FALSE")))
        {
            const auto poFeatureDefn = GetLayerDefn();
            const int nGeomFieldCount = poFeatureDefn->GetGeomFieldCount();
            for (int i = 0; i < nGeomFieldCount; i++)
            {
                const double dfXYResolution = poFeatureDefn->GetGeomFieldDefn(i)
                                                  ->GetCoordinatePrecision()
                                                  .dfXYResolution;
                if (dfXYResolution != OGRGeomCoordinatePrecision::UNKNOWN &&
                    OGRGeometryFactory::haveGEOS())
                {
                    m_poPrivate->m_bApplyGeomSetPrecision = true;
                    break;
                }
            }
        }
    }

    if (!m_poPrivate->m_bSupportsCurve || !m_poPrivate->m_bSupportsM ||
        m_poPrivate->m_bApplyGeomSetPrecision)
    {
        const auto poFeatureDefn = GetLayerDefn();
        const int nGeomFieldCount = poFeatureDefn->GetGeomFieldCount();
        for (int i = 0; i < nGeomFieldCount; i++)
        {
            OGRGeometry *poGeom = poFeature->GetGeomFieldRef(i);
            if (poGeom)
            {
                if (!m_poPrivate->m_bSupportsM &&
                    OGR_GT_HasM(poGeom->getGeometryType()))
                {
                    poGeom->setMeasured(FALSE);
                }

                if (!m_poPrivate->m_bSupportsCurve &&
                    OGR_GT_IsNonLinear(poGeom->getGeometryType()))
                {
                    OGRwkbGeometryType eTargetType =
                        OGR_GT_GetLinear(poGeom->getGeometryType());
                    poGeom = OGRGeometryFactory::forceTo(
                        poFeature->StealGeometry(i), eTargetType);
                    poFeature->SetGeomFieldDirectly(i, poGeom);
                    poGeom = poFeature->GetGeomFieldRef(i);
                }

                if (poGeom && m_poPrivate->m_bApplyGeomSetPrecision)
                {
                    const double dfXYResolution =
                        poFeatureDefn->GetGeomFieldDefn(i)
                            ->GetCoordinatePrecision()
                            .dfXYResolution;
                    if (dfXYResolution != OGRGeomCoordinatePrecision::UNKNOWN &&
                        !poGeom->hasCurveGeometry())
                    {
                        auto poNewGeom = poGeom->SetPrecision(dfXYResolution,
                                                              /* nFlags = */ 0);
                        if (poNewGeom)
                        {
                            poFeature->SetGeomFieldDirectly(i, poNewGeom);
                            // If there was potential further processing...
                            // poGeom = poFeature->GetGeomFieldRef(i);
                        }
                    }
                }
            }
        }
    }
}

/************************************************************************/
/*                             SetFeature()                             */
/************************************************************************/

/**
 \brief Rewrite/replace an existing feature.

 This method will write a feature to the layer, based on the feature id
 within the OGRFeature.

 Use OGRLayer::TestCapability(OLCRandomWrite) to establish if this layer
 supports random access writing via SetFeature().

 The way unset fields in the provided poFeature are processed is driver dependent:
 <ul>
 <li>
 SQL based drivers which implement SetFeature() through SQL UPDATE will skip
 unset fields, and thus the content of the existing feature will be preserved.
 </li>
 <li>
 The shapefile driver will write a NULL value in the DBF file.
 </li>
 <li>
 The GeoJSON driver will take into account unset fields to remove the corresponding
 JSON member.
 </li>
 </ul>

 Drivers should specialize the ISetFeature() method.

 This method is the same as the C function OGR_L_SetFeature().

 To set a feature, but create it if it doesn't exist see OGRLayer::UpsertFeature().

 @param poFeature the feature to write.

 @return OGRERR_NONE if the operation works, otherwise an appropriate error
 code (e.g OGRERR_NON_EXISTING_FEATURE if the feature does not exist).

 @see UpdateFeature(), CreateFeature(), UpsertFeature()
*/

OGRErr OGRLayer::SetFeature(OGRFeature *poFeature)

{
    ConvertGeomsIfNecessary(poFeature);
    return ISetFeature(poFeature);
}

/************************************************************************/
/*                             ISetFeature()                            */
/************************************************************************/

/**
 \brief Rewrite/replace an existing feature.

 This method is implemented by drivers and not called directly. User code should
 use SetFeature() instead.

 This method will write a feature to the layer, based on the feature id
 within the OGRFeature.

 @param poFeature the feature to write.

 @return OGRERR_NONE if the operation works, otherwise an appropriate error
 code (e.g OGRERR_NON_EXISTING_FEATURE if the feature does not exist).

 @see SetFeature()
*/

OGRErr OGRLayer::ISetFeature(OGRFeature *poFeature)

{
    (void)poFeature;
    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                          OGR_L_SetFeature()                          */
/************************************************************************/

/**
 \brief Rewrite/replace an existing feature.

 This function will write a feature to the layer, based on the feature id
 within the OGRFeature.

 Use OGR_L_TestCapability(OLCRandomWrite) to establish if this layer
 supports random access writing via OGR_L_SetFeature().

 The way unset fields in the provided poFeature are processed is driver dependent:
 <ul>
 <li>
 SQL based drivers which implement SetFeature() through SQL UPDATE will skip
 unset fields, and thus the content of the existing feature will be preserved.
 </li>
 <li>
 The shapefile driver will write a NULL value in the DBF file.
 </li>
 <li>
 The GeoJSON driver will take into account unset fields to remove the corresponding
 JSON member.
 </li>
 </ul>

 This function is the same as the C++ method OGRLayer::SetFeature().

 To set a feature, but create it if it doesn't exist see OGR_L_UpsertFeature().

 @param hLayer handle to the layer to write the feature.
 @param hFeat the feature to write.

 @return OGRERR_NONE if the operation works, otherwise an appropriate error
 code (e.g OGRERR_NON_EXISTING_FEATURE if the feature does not exist).

 @see OGR_L_UpdateFeature(), OGR_L_CreateFeature(), OGR_L_UpsertFeature()
*/

OGRErr OGR_L_SetFeature(OGRLayerH hLayer, OGRFeatureH hFeat)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_SetFeature", OGRERR_INVALID_HANDLE);
    VALIDATE_POINTER1(hFeat, "OGR_L_SetFeature", OGRERR_INVALID_HANDLE);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_SetFeature(hLayer, hFeat);
#endif

    return OGRLayer::FromHandle(hLayer)->SetFeature(
        OGRFeature::FromHandle(hFeat));
}

/************************************************************************/
/*                             SetFeature()                              */
/************************************************************************/

/**
 \brief Rewrite/replace an existing feature, transferring ownership
        of the feature to the layer

 This method will write a feature to the layer, based on the feature id
 within the OGRFeature.

 Use OGRLayer::TestCapability(OLCRandomWrite) to establish if this layer
 supports random access writing via SetFeature().

 The way unset fields in the provided poFeature are processed is driver dependent:
 <ul>
 <li>
 SQL based drivers which implement SetFeature() through SQL UPDATE will skip
 unset fields, and thus the content of the existing feature will be preserved.
 </li>
 <li>
 The shapefile driver will write a NULL value in the DBF file.
 </li>
 <li>
 The GeoJSON driver will take into account unset fields to remove the corresponding
 JSON member.
 </li>
 </ul>

 Drivers should specialize the ISetFeatureUniqPtr() method.

 To set a feature, but create it if it doesn't exist see OGRLayer::UpsertFeature().

 @param poFeature the feature to write.

 @return OGRERR_NONE if the operation works, otherwise an appropriate error
 code (e.g OGRERR_NON_EXISTING_FEATURE if the feature does not exist).

 @see UpdateFeature(), CreateFeature(), UpsertFeature()
 @since 3.13
*/

OGRErr OGRLayer::SetFeature(std::unique_ptr<OGRFeature> poFeature)

{
    ConvertGeomsIfNecessary(poFeature.get());
    return ISetFeatureUniqPtr(std::move(poFeature));
}

/************************************************************************/
/*                           ISetFeatureUniqPtr()                       */
/************************************************************************/

/**
 \brief Rewrite/replace an existing feature, transferring ownership
        of the feature to the layer

 WARNING: if drivers implement this method, they *MUST* also implement
 ISetFeature()

 This method is implemented by drivers and not called directly. User code should
 use SetFeature() instead.

 This method will write a feature to the layer, based on the feature id
 within the OGRFeature.

 @param poFeature the feature to write.

 @return OGRERR_NONE if the operation works, otherwise an appropriate error
 code (e.g OGRERR_NON_EXISTING_FEATURE if the feature does not exist).

 @see SetFeature()
 @since 3.13
*/

OGRErr OGRLayer::ISetFeatureUniqPtr(std::unique_ptr<OGRFeature> poFeature)

{
    return ISetFeature(poFeature.get());
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

/**
 \brief Create and write a new feature within a layer.

 The passed feature is written to the layer as a new feature, rather than
 overwriting an existing one.  If the feature has a feature id other than
 OGRNullFID, then the native implementation may use that as the feature id
 of the new feature, but not necessarily.  Upon successful return the
 passed feature will have been updated with the new feature id.

 Drivers should specialize the ICreateFeature() method.

 This method is the same as the C function OGR_L_CreateFeature().

 To create a feature, but set it if it exists see OGRLayer::UpsertFeature().

 @param poFeature the feature to write to disk.

 @return OGRERR_NONE on success.

 @see SetFeature(), UpdateFeature(), UpsertFeature()
*/

OGRErr OGRLayer::CreateFeature(OGRFeature *poFeature)

{
    ConvertGeomsIfNecessary(poFeature);
    return ICreateFeature(poFeature);
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

/**
 \brief Create and write a new feature within a layer.

 This method is implemented by drivers and not called directly. User code should
 use CreateFeature() instead.

 The passed feature is written to the layer as a new feature, rather than
 overwriting an existing one.  If the feature has a feature id other than
 OGRNullFID, then the native implementation may use that as the feature id
 of the new feature, but not necessarily.  Upon successful return the
 passed feature will have been updated with the new feature id.

 @param poFeature the feature to write to disk.

 @return OGRERR_NONE on success.

 @see CreateFeature()
*/

OGRErr OGRLayer::ICreateFeature(OGRFeature *poFeature)

{
    (void)poFeature;
    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                        OGR_L_CreateFeature()                         */
/************************************************************************/

/**
 \brief Create and write a new feature within a layer.

 The passed feature is written to the layer as a new feature, rather than
 overwriting an existing one.  If the feature has a feature id other than
 OGRNullFID, then the native implementation may use that as the feature id
 of the new feature, but not necessarily.  Upon successful return the
 passed feature will have been updated with the new feature id.

 This function is the same as the C++ method OGRLayer::CreateFeature().

 To create a feature, but set it if it exists see OGR_L_UpsertFeature().

 @param hLayer handle to the layer to write the feature to.
 @param hFeat the handle of the feature to write to disk.

 @return OGRERR_NONE on success.

 @see OGR_L_SetFeature(), OGR_L_UpdateFeature(), OGR_L_UpsertFeature()
*/

OGRErr OGR_L_CreateFeature(OGRLayerH hLayer, OGRFeatureH hFeat)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_CreateFeature", OGRERR_INVALID_HANDLE);
    VALIDATE_POINTER1(hFeat, "OGR_L_CreateFeature", OGRERR_INVALID_HANDLE);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_CreateFeature(hLayer, hFeat);
#endif

    return OGRLayer::FromHandle(hLayer)->CreateFeature(
        OGRFeature::FromHandle(hFeat));
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

/**
 \brief Create and write a new feature within a layer, transferring ownership
        of the feature to the layer

 The passed feature is written to the layer as a new feature, rather than
 overwriting an existing one.  If the feature has a feature id other than
 OGRNullFID, then the native implementation may use that as the feature id
 of the new feature, but not necessarily.  Upon successful return the
 passed feature will have been updated with the new feature id.

 Drivers should specialize the ICreateFeatureUniqPtr() method.

 To create a feature, but set it if it exists see OGRLayer::UpsertFeature().

 @param poFeature the feature to write to disk.
 @param[out] pnFID Pointer to an integer that will receive the potentially
             updated FID

 @return OGRERR_NONE on success.

 @see SetFeature(), UpdateFeature(), UpsertFeature()
 @since 3.13
*/

OGRErr OGRLayer::CreateFeature(std::unique_ptr<OGRFeature> poFeature,
                               GIntBig *pnFID)

{
    ConvertGeomsIfNecessary(poFeature.get());
    return ICreateFeatureUniqPtr(std::move(poFeature), pnFID);
}

/************************************************************************/
/*                         ICreateFeatureUniqPtr()                      */
/************************************************************************/

/**
 \brief Create and write a new feature within a layer, transferring ownership
        of the feature to the layer

 WARNING: if drivers implement this method, they *MUST* also implement
 ICreateFeature()

 The passed feature is written to the layer as a new feature, rather than
 overwriting an existing one.  If the feature has a feature id other than
 OGRNullFID, then the native implementation may use that as the feature id
 of the new feature, but not necessarily.  Upon successful return the
 passed feature will have been updated with the new feature id.

 @param poFeature the feature to write to disk.
 @param[out] pnFID Pointer to an integer that will receive the potentially
             updated FID

 @return OGRERR_NONE on success.

 @see ICreateFeature()
 @see CreateFeature(std::unique_ptr<OGRFeature> , GIntBig*)
 @since 3.13
*/

OGRErr OGRLayer::ICreateFeatureUniqPtr(std::unique_ptr<OGRFeature> poFeature,
                                       GIntBig *pnFID)

{
    const OGRErr eErr = ICreateFeature(poFeature.get());
    if (pnFID)
        *pnFID = poFeature->GetFID();
    return eErr;
}

/************************************************************************/
/*                           UpsertFeature()                           */
/************************************************************************/

/**
 \brief Rewrite/replace an existing feature or create a new feature within a layer.

 This function will write a feature to the layer, based on the feature id
 within the OGRFeature.  If the feature id doesn't exist a new feature will be
 written.  Otherwise, the existing feature will be rewritten.

 Use OGRLayer::TestCapability(OLCUpsertFeature) to establish if this layer
 supports upsert writing.

 This method is the same as the C function OGR_L_UpsertFeature().

 @param poFeature the feature to write to disk.

 @return OGRERR_NONE on success.
 @since GDAL 3.6.0

 @see SetFeature(), CreateFeature(), UpdateFeature()
*/

OGRErr OGRLayer::UpsertFeature(OGRFeature *poFeature)

{
    ConvertGeomsIfNecessary(poFeature);
    return IUpsertFeature(poFeature);
}

/************************************************************************/
/*                           IUpsertFeature()                           */
/************************************************************************/

/**
 \brief Rewrite/replace an existing feature or create a new feature within a layer.

 This method is implemented by drivers and not called directly. User code should
 use UpsertFeature() instead.

 This function will write a feature to the layer, based on the feature id
 within the OGRFeature.  If the feature id doesn't exist a new feature will be
 written.  Otherwise, the existing feature will be rewritten.

 @param poFeature the feature to write to disk.

 @return OGRERR_NONE on success.
 @since GDAL 3.6.0

 @see UpsertFeature()
*/

OGRErr OGRLayer::IUpsertFeature(OGRFeature *poFeature)
{
    (void)poFeature;
    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                        OGR_L_UpsertFeature()                         */
/************************************************************************/

/**
 \brief Rewrite/replace an existing feature or create a new feature within a layer.

 This function will write a feature to the layer, based on the feature id
 within the OGRFeature.  If the feature id doesn't exist a new feature will be
 written.  Otherwise, the existing feature will be rewritten.

 Use OGR_L_TestCapability(OLCUpsertFeature) to establish if this layer
 supports upsert writing.

 This function is the same as the C++ method OGRLayer::UpsertFeature().

 @param hLayer handle to the layer to write the feature to.
 @param hFeat the handle of the feature to write to disk.

 @return OGRERR_NONE on success.
 @since GDAL 3.6.0

 @see OGR_L_SetFeature(), OGR_L_CreateFeature(), OGR_L_UpdateFeature()
*/

OGRErr OGR_L_UpsertFeature(OGRLayerH hLayer, OGRFeatureH hFeat)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_UpsertFeature", OGRERR_INVALID_HANDLE);
    VALIDATE_POINTER1(hFeat, "OGR_L_UpsertFeature", OGRERR_INVALID_HANDLE);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_UpsertFeature(hLayer, hFeat);
#endif

    return OGRLayer::FromHandle(hLayer)->UpsertFeature(
        OGRFeature::FromHandle(hFeat));
}

/************************************************************************/
/*                           UpdateFeature()                            */
/************************************************************************/

/**
 \brief Update (part of) an existing feature.

 This method will update the specified attribute and geometry fields of a
 feature to the layer, based on the feature id within the OGRFeature.

 Use OGRLayer::TestCapability(OLCRandomWrite) to establish if this layer
 supports random access writing via UpdateFeature(). And to know if the
 driver supports a dedicated/efficient UpdateFeature() method, test for the
 OLCUpdateFeature capability.

 The way unset fields in the provided poFeature are processed is driver dependent:
 <ul>
 <li>
 SQL based drivers which implement SetFeature() through SQL UPDATE will skip
 unset fields, and thus the content of the existing feature will be preserved.
 </li>
 <li>
 The shapefile driver will write a NULL value in the DBF file.
 </li>
 <li>
 The GeoJSON driver will take into account unset fields to remove the corresponding
 JSON member.
 </li>
 </ul>

 This method is the same as the C function OGR_L_UpdateFeature().

 To fully replace a feature, see OGRLayer::SetFeature().

 Note that after this call the content of hFeat might have changed, and will
 *not* reflect the content you would get with GetFeature().
 In particular for performance reasons, passed geometries might have been "stolen",
 in particular for the default implementation of UpdateFeature() which relies
 on GetFeature() + SetFeature().

 @param poFeature the feature to update.

 @param nUpdatedFieldsCount number of attribute fields to update. May be 0

 @param panUpdatedFieldsIdx array of nUpdatedFieldsCount values, each between
                            0 and GetLayerDefn()->GetFieldCount() - 1, indicating
                            which fields of poFeature must be updated in the
                            layer.

 @param nUpdatedGeomFieldsCount number of geometry fields to update. May be 0

 @param panUpdatedGeomFieldsIdx array of nUpdatedGeomFieldsCount values, each between
                                0 and GetLayerDefn()->GetGeomFieldCount() - 1, indicating
                                which geometry fields of poFeature must be updated in the
                                layer.

 @param bUpdateStyleString whether the feature style string in the layer should
                           be updated with the one of poFeature.

 @return OGRERR_NONE if the operation works, otherwise an appropriate error
 code (e.g OGRERR_NON_EXISTING_FEATURE if the feature does not exist).

 @since GDAL 3.7

 @see UpdateFeature(), CreateFeature(), UpsertFeature()
*/

OGRErr OGRLayer::UpdateFeature(OGRFeature *poFeature, int nUpdatedFieldsCount,
                               const int *panUpdatedFieldsIdx,
                               int nUpdatedGeomFieldsCount,
                               const int *panUpdatedGeomFieldsIdx,
                               bool bUpdateStyleString)

{
    ConvertGeomsIfNecessary(poFeature);
    const int nFieldCount = GetLayerDefn()->GetFieldCount();
    for (int i = 0; i < nUpdatedFieldsCount; ++i)
    {
        if (panUpdatedFieldsIdx[i] < 0 || panUpdatedFieldsIdx[i] >= nFieldCount)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid panUpdatedFieldsIdx[%d] = %d", i,
                     panUpdatedFieldsIdx[i]);
            return OGRERR_FAILURE;
        }
    }
    const int nGeomFieldCount = GetLayerDefn()->GetGeomFieldCount();
    for (int i = 0; i < nUpdatedGeomFieldsCount; ++i)
    {
        if (panUpdatedGeomFieldsIdx[i] < 0 ||
            panUpdatedGeomFieldsIdx[i] >= nGeomFieldCount)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid panUpdatedGeomFieldsIdx[%d] = %d", i,
                     panUpdatedGeomFieldsIdx[i]);
            return OGRERR_FAILURE;
        }
    }
    return IUpdateFeature(poFeature, nUpdatedFieldsCount, panUpdatedFieldsIdx,
                          nUpdatedGeomFieldsCount, panUpdatedGeomFieldsIdx,
                          bUpdateStyleString);
}

/************************************************************************/
/*                           IUpdateFeature()                           */
/************************************************************************/

/**
 \brief Update (part of) an existing feature.

 This method is implemented by drivers and not called directly. User code should
 use UpdateFeature() instead.

 @param poFeature the feature to update.

 @param nUpdatedFieldsCount number of attribute fields to update. May be 0

 @param panUpdatedFieldsIdx array of nUpdatedFieldsCount values, each between
                            0 and GetLayerDefn()->GetFieldCount() - 1, indicating
                            which fields of poFeature must be updated in the
                            layer.

 @param nUpdatedGeomFieldsCount number of geometry fields to update. May be 0

 @param panUpdatedGeomFieldsIdx array of nUpdatedGeomFieldsCount values, each between
                                0 and GetLayerDefn()->GetGeomFieldCount() - 1, indicating
                                which geometry fields of poFeature must be updated in the
                                layer.

 @param bUpdateStyleString whether the feature style string in the layer should
                           be updated with the one of poFeature.

 @return OGRERR_NONE if the operation works, otherwise an appropriate error
 code (e.g OGRERR_NON_EXISTING_FEATURE if the feature does not exist).

 @since GDAL 3.7

 @see UpdateFeature()
*/

OGRErr OGRLayer::IUpdateFeature(OGRFeature *poFeature, int nUpdatedFieldsCount,
                                const int *panUpdatedFieldsIdx,
                                int nUpdatedGeomFieldsCount,
                                const int *panUpdatedGeomFieldsIdx,
                                bool bUpdateStyleString)
{
    if (!TestCapability(OLCRandomWrite))
        return OGRERR_UNSUPPORTED_OPERATION;

    auto poFeatureExisting =
        std::unique_ptr<OGRFeature>(GetFeature(poFeature->GetFID()));
    if (!poFeatureExisting)
        return OGRERR_NON_EXISTING_FEATURE;

    for (int i = 0; i < nUpdatedFieldsCount; ++i)
    {
        poFeatureExisting->SetField(
            panUpdatedFieldsIdx[i],
            poFeature->GetRawFieldRef(panUpdatedFieldsIdx[i]));
    }
    for (int i = 0; i < nUpdatedGeomFieldsCount; ++i)
    {
        poFeatureExisting->SetGeomFieldDirectly(
            panUpdatedGeomFieldsIdx[i],
            poFeature->StealGeometry(panUpdatedGeomFieldsIdx[i]));
    }
    if (bUpdateStyleString)
    {
        poFeatureExisting->SetStyleString(poFeature->GetStyleString());
    }
    return ISetFeature(poFeatureExisting.get());
}

/************************************************************************/
/*                        OGR_L_UpdateFeature()                         */
/************************************************************************/

/**
 \brief Update (part of) an existing feature.

 This function will update the specified attribute and geometry fields of a
 feature to the layer, based on the feature id within the OGRFeature.

 Use OGR_L_TestCapability(OLCRandomWrite) to establish if this layer
 supports random access writing via UpdateFeature(). And to know if the
 driver supports a dedicated/efficient UpdateFeature() method, test for the
 OLCUpdateFeature capability.

 The way unset fields in the provided poFeature are processed is driver dependent:
 <ul>
 <li>
 SQL based drivers which implement SetFeature() through SQL UPDATE will skip
 unset fields, and thus the content of the existing feature will be preserved.
 </li>
 <li>
 The shapefile driver will write a NULL value in the DBF file.
 </li>
 <li>
 The GeoJSON driver will take into account unset fields to remove the corresponding
 JSON member.
 </li>
 </ul>

 This method is the same as the C++ method OGRLayer::UpdateFeature().

 To fully replace a feature, see OGR_L_SetFeature()

 Note that after this call the content of hFeat might have changed, and will
 *not* reflect the content you would get with OGR_L_GetFeature().
 In particular for performance reasons, passed geometries might have been "stolen",
 in particular for the default implementation of UpdateFeature() which relies
 on GetFeature() + SetFeature().

 @param hLayer handle to the layer to write the feature.

 @param hFeat the feature to update.

 @param nUpdatedFieldsCount number of attribute fields to update. May be 0

 @param panUpdatedFieldsIdx array of nUpdatedFieldsCount values, each between
                            0 and GetLayerDefn()->GetFieldCount() - 1, indicating
                            which fields of hFeat must be updated in the
                            layer.

 @param nUpdatedGeomFieldsCount number of geometry fields to update. May be 0

 @param panUpdatedGeomFieldsIdx array of nUpdatedGeomFieldsCount values, each between
                                0 and GetLayerDefn()->GetGeomFieldCount() - 1, indicating
                                which geometry fields of hFeat must be updated in the
                                layer.

 @param bUpdateStyleString whether the feature style string in the layer should
                           be updated with the one of hFeat.

 @return OGRERR_NONE if the operation works, otherwise an appropriate error
 code (e.g OGRERR_NON_EXISTING_FEATURE if the feature does not exist).

 @since GDAL 3.7

 @see OGR_L_UpdateFeature(), OGR_L_CreateFeature(), OGR_L_UpsertFeature()
*/

OGRErr OGR_L_UpdateFeature(OGRLayerH hLayer, OGRFeatureH hFeat,
                           int nUpdatedFieldsCount,
                           const int *panUpdatedFieldsIdx,
                           int nUpdatedGeomFieldsCount,
                           const int *panUpdatedGeomFieldsIdx,
                           bool bUpdateStyleString)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_UpdateFeature", OGRERR_INVALID_HANDLE);
    VALIDATE_POINTER1(hFeat, "OGR_L_UpdateFeature", OGRERR_INVALID_HANDLE);

    return OGRLayer::FromHandle(hLayer)->UpdateFeature(
        OGRFeature::FromHandle(hFeat), nUpdatedFieldsCount, panUpdatedFieldsIdx,
        nUpdatedGeomFieldsCount, panUpdatedGeomFieldsIdx, bUpdateStyleString);
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

/**
\brief Create a new field on a layer.

You must use this to create new fields
on a real layer. Internally the OGRFeatureDefn for the layer will be updated
to reflect the new field.  Applications should never modify the OGRFeatureDefn
used by a layer directly.

This method should not be called while there are feature objects in existence that
were obtained or created with the previous layer definition.

Not all drivers support this method. You can query a layer to check if it supports it
with the OLCCreateField capability. Some drivers may only support this method while
there are still no features in the layer. When it is supported, the existing features of the
backing file/database should be updated accordingly.

Drivers may or may not support not-null constraints. If they support creating
fields with not-null constraints, this is generally before creating any feature to the layer.

This function is the same as the C function OGR_L_CreateField().

@param poField field definition to write to disk.
@param bApproxOK If TRUE, the field may be created in a slightly different
form depending on the limitations of the format driver.

@return OGRERR_NONE on success.
*/

OGRErr OGRLayer::CreateField(const OGRFieldDefn *poField, int bApproxOK)

{
    (void)poField;
    (void)bApproxOK;

    CPLError(CE_Failure, CPLE_NotSupported,
             "CreateField() not supported by this layer.\n");

    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                         OGR_L_CreateField()                          */
/************************************************************************/

/**
\brief Create a new field on a layer.

You must use this to create new fields
on a real layer. Internally the OGRFeatureDefn for the layer will be updated
to reflect the new field.  Applications should never modify the OGRFeatureDefn
used by a layer directly.

This function should not be called while there are feature objects in existence that
were obtained or created with the previous layer definition.

Not all drivers support this function. You can query a layer to check if it supports it
with the OLCCreateField capability. Some drivers may only support this method while
there are still no features in the layer. When it is supported, the existing features of the
backing file/database should be updated accordingly.

Drivers may or may not support not-null constraints. If they support creating
fields with not-null constraints, this is generally before creating any feature to the layer.

 This function is the same as the C++ method OGRLayer::CreateField().

 @param hLayer handle to the layer to write the field definition.
 @param hField handle of the field definition to write to disk.
 @param bApproxOK If TRUE, the field may be created in a slightly different
form depending on the limitations of the format driver.

 @return OGRERR_NONE on success.
*/

OGRErr OGR_L_CreateField(OGRLayerH hLayer, OGRFieldDefnH hField, int bApproxOK)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_CreateField", OGRERR_INVALID_HANDLE);
    VALIDATE_POINTER1(hField, "OGR_L_CreateField", OGRERR_INVALID_HANDLE);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_CreateField(hLayer, hField, bApproxOK);
#endif

    return OGRLayer::FromHandle(hLayer)->CreateField(
        OGRFieldDefn::FromHandle(hField), bApproxOK);
}

/************************************************************************/
/*                            DeleteField()                             */
/************************************************************************/

/**
\brief Delete an existing field on a layer.

You must use this to delete existing fields
on a real layer. Internally the OGRFeatureDefn for the layer will be updated
to reflect the deleted field.  Applications should never modify the OGRFeatureDefn
used by a layer directly.

This method should not be called while there are feature objects in existence that
were obtained or created with the previous layer definition.

If a OGRFieldDefn* object corresponding to the deleted field has been retrieved
from the layer definition before the call to DeleteField(), it must no longer be
used after the call to DeleteField(), which will have destroyed it.

Not all drivers support this method. You can query a layer to check if it supports it
with the OLCDeleteField capability. Some drivers may only support this method while
there are still no features in the layer. When it is supported, the existing features of the
backing file/database should be updated accordingly.

This function is the same as the C function OGR_L_DeleteField().

@param iField index of the field to delete.

@return OGRERR_NONE on success.
*/

OGRErr OGRLayer::DeleteField(int iField)

{
    (void)iField;

    CPLError(CE_Failure, CPLE_NotSupported,
             "DeleteField() not supported by this layer.\n");

    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                         OGR_L_DeleteField()                          */
/************************************************************************/

/**
\brief Delete an existing field on a layer.

You must use this to delete existing fields
on a real layer. Internally the OGRFeatureDefn for the layer will be updated
to reflect the deleted field.  Applications should never modify the OGRFeatureDefn
used by a layer directly.

This function should not be called while there are feature objects in existence that
were obtained or created with the previous layer definition.

If a OGRFieldDefnH object corresponding to the deleted field has been retrieved
from the layer definition before the call to DeleteField(), it must no longer be
used after the call to DeleteField(), which will have destroyed it.

Not all drivers support this function. You can query a layer to check if it supports it
with the OLCDeleteField capability. Some drivers may only support this method while
there are still no features in the layer. When it is supported, the existing features of the
backing file/database should be updated accordingly.

This function is the same as the C++ method OGRLayer::DeleteField().

@param hLayer handle to the layer.
@param iField index of the field to delete.

@return OGRERR_NONE on success.
*/

OGRErr OGR_L_DeleteField(OGRLayerH hLayer, int iField)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_DeleteField", OGRERR_INVALID_HANDLE);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_DeleteField(hLayer, iField);
#endif

    return OGRLayer::FromHandle(hLayer)->DeleteField(iField);
}

/************************************************************************/
/*                           ReorderFields()                            */
/************************************************************************/

/**
\brief Reorder all the fields of a layer.

You must use this to reorder existing fields
on a real layer. Internally the OGRFeatureDefn for the layer will be updated
to reflect the reordering of the fields.  Applications should never modify the OGRFeatureDefn
used by a layer directly.

This method should not be called while there are feature objects in existence that
were obtained or created with the previous layer definition.

panMap is such that,for each field definition at position i after reordering,
its position before reordering was panMap[i].

For example, let suppose the fields were "0","1","2","3","4" initially.
ReorderFields([0,2,3,1,4]) will reorder them as "0","2","3","1","4".

Not all drivers support this method. You can query a layer to check if it supports it
with the OLCReorderFields capability. Some drivers may only support this method while
there are still no features in the layer. When it is supported, the existing features of the
backing file/database should be updated accordingly.

This function is the same as the C function OGR_L_ReorderFields().

@param panMap an array of GetLayerDefn()->OGRFeatureDefn::GetFieldCount() elements which
is a permutation of [0, GetLayerDefn()->OGRFeatureDefn::GetFieldCount()-1].

@return OGRERR_NONE on success.
*/

OGRErr OGRLayer::ReorderFields(int *panMap)

{
    (void)panMap;

    CPLError(CE_Failure, CPLE_NotSupported,
             "ReorderFields() not supported by this layer.\n");

    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                       OGR_L_ReorderFields()                          */
/************************************************************************/

/**
\brief Reorder all the fields of a layer.

You must use this to reorder existing fields
on a real layer. Internally the OGRFeatureDefn for the layer will be updated
to reflect the reordering of the fields.  Applications should never modify the OGRFeatureDefn
used by a layer directly.

This function should not be called while there are feature objects in existence that
were obtained or created with the previous layer definition.

panMap is such that,for each field definition at position i after reordering,
its position before reordering was panMap[i].

For example, let suppose the fields were "0","1","2","3","4" initially.
ReorderFields([0,2,3,1,4]) will reorder them as "0","2","3","1","4".

Not all drivers support this function. You can query a layer to check if it supports it
with the OLCReorderFields capability. Some drivers may only support this method while
there are still no features in the layer. When it is supported, the existing features of the
backing file/database should be updated accordingly.

This function is the same as the C++ method OGRLayer::ReorderFields().

@param hLayer handle to the layer.
@param panMap an array of GetLayerDefn()->OGRFeatureDefn::GetFieldCount() elements which
is a permutation of [0, GetLayerDefn()->OGRFeatureDefn::GetFieldCount()-1].

@return OGRERR_NONE on success.
*/

OGRErr OGR_L_ReorderFields(OGRLayerH hLayer, int *panMap)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_ReorderFields", OGRERR_INVALID_HANDLE);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_ReorderFields(hLayer, panMap);
#endif

    return OGRLayer::FromHandle(hLayer)->ReorderFields(panMap);
}

/************************************************************************/
/*                            ReorderField()                            */
/************************************************************************/

/**
\brief Reorder an existing field on a layer.

This method is a convenience wrapper of ReorderFields() dedicated to move a single field.
It is a non-virtual method, so drivers should implement ReorderFields() instead.

You must use this to reorder existing fields
on a real layer. Internally the OGRFeatureDefn for the layer will be updated
to reflect the reordering of the fields.  Applications should never modify the OGRFeatureDefn
used by a layer directly.

This method should not be called while there are feature objects in existence that
were obtained or created with the previous layer definition.

The field definition that was at initial position iOldFieldPos will be moved at
position iNewFieldPos, and elements between will be shuffled accordingly.

For example, let suppose the fields were "0","1","2","3","4" initially.
ReorderField(1, 3) will reorder them as "0","2","3","1","4".

Not all drivers support this method. You can query a layer to check if it supports it
with the OLCReorderFields capability. Some drivers may only support this method while
there are still no features in the layer. When it is supported, the existing features of the
backing file/database should be updated accordingly.

This function is the same as the C function OGR_L_ReorderField().

@param iOldFieldPos previous position of the field to move. Must be in the range [0,GetFieldCount()-1].
@param iNewFieldPos new position of the field to move. Must be in the range [0,GetFieldCount()-1].

@return OGRERR_NONE on success.
*/

OGRErr OGRLayer::ReorderField(int iOldFieldPos, int iNewFieldPos)

{
    OGRErr eErr;

    int nFieldCount = GetLayerDefn()->GetFieldCount();

    if (iOldFieldPos < 0 || iOldFieldPos >= nFieldCount)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid field index");
        return OGRERR_FAILURE;
    }
    if (iNewFieldPos < 0 || iNewFieldPos >= nFieldCount)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid field index");
        return OGRERR_FAILURE;
    }
    if (iNewFieldPos == iOldFieldPos)
        return OGRERR_NONE;

    int *panMap = static_cast<int *>(CPLMalloc(sizeof(int) * nFieldCount));
    if (iOldFieldPos < iNewFieldPos)
    {
        /* "0","1","2","3","4" (1,3) -> "0","2","3","1","4" */
        int i = 0;  // Used after for.
        for (; i < iOldFieldPos; i++)
            panMap[i] = i;
        for (; i < iNewFieldPos; i++)
            panMap[i] = i + 1;
        panMap[iNewFieldPos] = iOldFieldPos;
        for (i = iNewFieldPos + 1; i < nFieldCount; i++)
            panMap[i] = i;
    }
    else
    {
        /* "0","1","2","3","4" (3,1) -> "0","3","1","2","4" */
        for (int i = 0; i < iNewFieldPos; i++)
            panMap[i] = i;
        panMap[iNewFieldPos] = iOldFieldPos;
        int i = iNewFieldPos + 1;  // Used after for.
        for (; i <= iOldFieldPos; i++)
            panMap[i] = i - 1;
        for (; i < nFieldCount; i++)
            panMap[i] = i;
    }

    eErr = ReorderFields(panMap);

    CPLFree(panMap);

    return eErr;
}

/************************************************************************/
/*                        OGR_L_ReorderField()                          */
/************************************************************************/

/**
\brief Reorder an existing field on a layer.

This function is a convenience wrapper of OGR_L_ReorderFields() dedicated to move a single field.

You must use this to reorder existing fields
on a real layer. Internally the OGRFeatureDefn for the layer will be updated
to reflect the reordering of the fields.  Applications should never modify the OGRFeatureDefn
used by a layer directly.

This function should not be called while there are feature objects in existence that
were obtained or created with the previous layer definition.

The field definition that was at initial position iOldFieldPos will be moved at
position iNewFieldPos, and elements between will be shuffled accordingly.

For example, let suppose the fields were "0","1","2","3","4" initially.
ReorderField(1, 3) will reorder them as "0","2","3","1","4".

Not all drivers support this function. You can query a layer to check if it supports it
with the OLCReorderFields capability. Some drivers may only support this method while
there are still no features in the layer. When it is supported, the existing features of the
backing file/database should be updated accordingly.

This function is the same as the C++ method OGRLayer::ReorderField().

@param hLayer handle to the layer.
@param iOldFieldPos previous position of the field to move. Must be in the range [0,GetFieldCount()-1].
@param iNewFieldPos new position of the field to move. Must be in the range [0,GetFieldCount()-1].

@return OGRERR_NONE on success.
*/

OGRErr OGR_L_ReorderField(OGRLayerH hLayer, int iOldFieldPos, int iNewFieldPos)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_ReorderField", OGRERR_INVALID_HANDLE);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_ReorderField(hLayer, iOldFieldPos, iNewFieldPos);
#endif

    return OGRLayer::FromHandle(hLayer)->ReorderField(iOldFieldPos,
                                                      iNewFieldPos);
}

/************************************************************************/
/*                           AlterFieldDefn()                           */
/************************************************************************/

/**
\brief Alter the definition of an existing field on a layer.

You must use this to alter the definition of an existing field of a real layer.
Internally the OGRFeatureDefn for the layer will be updated
to reflect the altered field.  Applications should never modify the OGRFeatureDefn
used by a layer directly.

This method should not be called while there are feature objects in existence that
were obtained or created with the previous layer definition.

Not all drivers support this method. You can query a layer to check if it supports it
with the OLCAlterFieldDefn capability. Some drivers may only support this method while
there are still no features in the layer. When it is supported, the existing features of the
backing file/database should be updated accordingly. Some drivers might also not support
all update flags.

This function is the same as the C function OGR_L_AlterFieldDefn().

@param iField index of the field whose definition must be altered.
@param poNewFieldDefn new field definition
@param nFlagsIn combination of ALTER_NAME_FLAG, ALTER_TYPE_FLAG, ALTER_WIDTH_PRECISION_FLAG,
ALTER_NULLABLE_FLAG and ALTER_DEFAULT_FLAG
to indicate which of the name and/or type and/or width and precision fields and/or nullability from the new field
definition must be taken into account.

@return OGRERR_NONE on success.
*/

OGRErr OGRLayer::AlterFieldDefn(int iField, OGRFieldDefn *poNewFieldDefn,
                                int nFlagsIn)

{
    (void)iField;
    (void)poNewFieldDefn;
    (void)nFlagsIn;
    CPLError(CE_Failure, CPLE_NotSupported,
             "AlterFieldDefn() not supported by this layer.\n");

    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                        OGR_L_AlterFieldDefn()                        */
/************************************************************************/

/**
\brief Alter the definition of an existing field on a layer.

You must use this to alter the definition of an existing field of a real layer.
Internally the OGRFeatureDefn for the layer will be updated
to reflect the altered field.  Applications should never modify the OGRFeatureDefn
used by a layer directly.

This function should not be called while there are feature objects in existence that
were obtained or created with the previous layer definition.

Not all drivers support this function. You can query a layer to check if it supports it
with the OLCAlterFieldDefn capability. Some drivers may only support this method while
there are still no features in the layer. When it is supported, the existing features of the
backing file/database should be updated accordingly. Some drivers might also not support
all update flags.

This function is the same as the C++ method OGRLayer::AlterFieldDefn().

@param hLayer handle to the layer.
@param iField index of the field whose definition must be altered.
@param hNewFieldDefn new field definition
@param nFlags combination of ALTER_NAME_FLAG, ALTER_TYPE_FLAG, ALTER_WIDTH_PRECISION_FLAG,
ALTER_NULLABLE_FLAG and ALTER_DEFAULT_FLAG
to indicate which of the name and/or type and/or width and precision fields and/or nullability from the new field
definition must be taken into account.

@return OGRERR_NONE on success.
*/

OGRErr OGR_L_AlterFieldDefn(OGRLayerH hLayer, int iField,
                            OGRFieldDefnH hNewFieldDefn, int nFlags)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_AlterFieldDefn", OGRERR_INVALID_HANDLE);
    VALIDATE_POINTER1(hNewFieldDefn, "OGR_L_AlterFieldDefn",
                      OGRERR_INVALID_HANDLE);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_AlterFieldDefn(hLayer, iField, hNewFieldDefn, nFlags);
#endif

    return OGRLayer::FromHandle(hLayer)->AlterFieldDefn(
        iField, OGRFieldDefn::FromHandle(hNewFieldDefn), nFlags);
}

/************************************************************************/
/*                        AlterGeomFieldDefn()                          */
/************************************************************************/

/**
\brief Alter the definition of an existing geometry field on a layer.

You must use this to alter the definition of an existing geometry field of a real layer.
Internally the OGRFeatureDefn for the layer will be updated
to reflect the altered field.  Applications should never modify the OGRFeatureDefn
used by a layer directly.

Note that altering the SRS does *not* cause coordinate reprojection to occur:
this is simply a modification of the layer metadata (correcting a wrong SRS
definition). No modification to existing geometries will ever be performed,
so this method cannot be used to e.g. promote single part geometries to their
multipart equivalents.

This method should not be called while there are feature objects in existence that
were obtained or created with the previous layer definition.

Not all drivers support this method. You can query a layer to check if it supports it
with the OLCAlterGeomFieldDefn capability. Some drivers might not support
all update flags. The GDAL_DMD_ALTER_GEOM_FIELD_DEFN_FLAGS driver metadata item
can be queried to examine which flags may be supported by a driver.

This function is the same as the C function OGR_L_AlterGeomFieldDefn().

@param iGeomField index of the field whose definition must be altered.
@param poNewGeomFieldDefn new field definition
@param nFlagsIn combination of ALTER_GEOM_FIELD_DEFN_NAME_FLAG, ALTER_GEOM_FIELD_DEFN_TYPE_FLAG, ALTER_GEOM_FIELD_DEFN_NULLABLE_FLAG, ALTER_GEOM_FIELD_DEFN_SRS_FLAG, ALTER_GEOM_FIELD_DEFN_SRS_COORD_EPOCH_FLAG
to indicate which of the name and/or type and/or nullability and/or SRS and/or coordinate epoch from the new field
definition must be taken into account. Or ALTER_GEOM_FIELD_DEFN_ALL_FLAG to update all members.

@return OGRERR_NONE on success.

@since OGR 3.6.0
*/

OGRErr OGRLayer::AlterGeomFieldDefn(int iGeomField,
                                    const OGRGeomFieldDefn *poNewGeomFieldDefn,
                                    int nFlagsIn)

{
    (void)iGeomField;
    (void)poNewGeomFieldDefn;
    (void)nFlagsIn;

    CPLError(CE_Failure, CPLE_NotSupported,
             "AlterGeomFieldDefn() not supported by this layer.\n");

    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                      OGR_L_AlterGeomFieldDefn()                      */
/************************************************************************/

/**
\brief Alter the definition of an existing geometry field on a layer.

You must use this to alter the definition of an existing geometry field of a real layer.
Internally the OGRFeatureDefn for the layer will be updated
to reflect the altered field.  Applications should never modify the OGRFeatureDefn
used by a layer directly.

Note that altering the SRS does *not* cause coordinate reprojection to occur:
this is simply a modification of the layer metadata (correcting a wrong SRS
definition). No modification to existing geometries will ever be performed,
so this method cannot be used to e.g. promote single part geometries to their
multipart equivalents.

This function should not be called while there are feature objects in existence that
were obtained or created with the previous layer definition.

Not all drivers support this function. You can query a layer to check if it supports it
with the OLCAlterGeomFieldDefn capability. Some drivers might not support
all update flags. The GDAL_DMD_ALTER_GEOM_FIELD_DEFN_FLAGS driver metadata item
can be queried to examine which flags may be supported by a driver.

This function is the same as the C++ method OGRLayer::AlterFieldDefn().

@param hLayer handle to the layer.
@param iGeomField index of the field whose definition must be altered.
@param hNewGeomFieldDefn new field definition
@param nFlags combination of ALTER_GEOM_FIELD_DEFN_NAME_FLAG, ALTER_GEOM_FIELD_DEFN_TYPE_FLAG, ALTER_GEOM_FIELD_DEFN_NULLABLE_FLAG, ALTER_GEOM_FIELD_DEFN_SRS_FLAG, ALTER_GEOM_FIELD_DEFN_SRS_COORD_EPOCH_FLAG
to indicate which of the name and/or type and/or nullability and/or SRS and/or coordinate epoch from the new field
definition must be taken into account. Or ALTER_GEOM_FIELD_DEFN_ALL_FLAG to update all members.

@return OGRERR_NONE on success.

@since OGR 3.6.0
*/

OGRErr OGR_L_AlterGeomFieldDefn(OGRLayerH hLayer, int iGeomField,
                                OGRGeomFieldDefnH hNewGeomFieldDefn, int nFlags)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_AlterGeomFieldDefn",
                      OGRERR_INVALID_HANDLE);
    VALIDATE_POINTER1(hNewGeomFieldDefn, "OGR_L_AlterGeomFieldDefn",
                      OGRERR_INVALID_HANDLE);

    return OGRLayer::FromHandle(hLayer)->AlterGeomFieldDefn(
        iGeomField,
        const_cast<const OGRGeomFieldDefn *>(
            OGRGeomFieldDefn::FromHandle(hNewGeomFieldDefn)),
        nFlags);
}

/************************************************************************/
/*                         CreateGeomField()                            */
/************************************************************************/

/**
\brief Create a new geometry field on a layer.

You must use this to create new geometry fields
on a real layer. Internally the OGRFeatureDefn for the layer will be updated
to reflect the new field.  Applications should never modify the OGRFeatureDefn
used by a layer directly.

This method should not be called while there are feature objects in existence that
were obtained or created with the previous layer definition.

Not all drivers support this method. You can query a layer to check if it supports it
with the OLCCreateGeomField capability. Some drivers may only support this method while
there are still no features in the layer. When it is supported, the existing features of the
backing file/database should be updated accordingly.

Drivers may or may not support not-null constraints. If they support creating
fields with not-null constraints, this is generally before creating any feature to the layer.

This function is the same as the C function OGR_L_CreateGeomField().

@param poField geometry field definition to write to disk.
@param bApproxOK If TRUE, the field may be created in a slightly different
form depending on the limitations of the format driver.

@return OGRERR_NONE on success.
*/

OGRErr OGRLayer::CreateGeomField(const OGRGeomFieldDefn *poField, int bApproxOK)

{
    (void)poField;
    (void)bApproxOK;

    CPLError(CE_Failure, CPLE_NotSupported,
             "CreateGeomField() not supported by this layer.\n");

    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                        OGR_L_CreateGeomField()                       */
/************************************************************************/

/**
\brief Create a new geometry field on a layer.

You must use this to create new geometry fields
on a real layer. Internally the OGRFeatureDefn for the layer will be updated
to reflect the new field.  Applications should never modify the OGRFeatureDefn
used by a layer directly.

This function should not be called while there are feature objects in existence that
were obtained or created with the previous layer definition.

Not all drivers support this function. You can query a layer to check if it supports it
with the OLCCreateField capability. Some drivers may only support this method while
there are still no features in the layer. When it is supported, the existing features of the
backing file/database should be updated accordingly.

Drivers may or may not support not-null constraints. If they support creating
fields with not-null constraints, this is generally before creating any feature to the layer.

 This function is the same as the C++ method OGRLayer::CreateField().

 @param hLayer handle to the layer to write the field definition.
 @param hField handle of the geometry field definition to write to disk.
 @param bApproxOK If TRUE, the field may be created in a slightly different
form depending on the limitations of the format driver.

 @return OGRERR_NONE on success.
*/

OGRErr OGR_L_CreateGeomField(OGRLayerH hLayer, OGRGeomFieldDefnH hField,
                             int bApproxOK)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_CreateGeomField", OGRERR_INVALID_HANDLE);
    VALIDATE_POINTER1(hField, "OGR_L_CreateGeomField", OGRERR_INVALID_HANDLE);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_CreateGeomField(hLayer, hField, bApproxOK);
#endif

    return OGRLayer::FromHandle(hLayer)->CreateGeomField(
        OGRGeomFieldDefn::FromHandle(hField), bApproxOK);
}

/************************************************************************/
/*                          StartTransaction()                          */
/************************************************************************/

/**
 \brief For datasources which support transactions, StartTransaction creates a transaction.

 If starting the transaction fails, will return
 OGRERR_FAILURE. Datasources which do not support transactions will
 always return OGRERR_NONE.

 Use of this API is discouraged when the dataset offers
 dataset level transaction with GDALDataset::StartTransaction(). The reason is
 that most drivers can only offer transactions at dataset level, and not layer level.
 Very few drivers really support transactions at layer scope.

 This function is the same as the C function OGR_L_StartTransaction().

 @return OGRERR_NONE on success.
*/

OGRErr OGRLayer::StartTransaction()

{
    return OGRERR_NONE;
}

/************************************************************************/
/*                       OGR_L_StartTransaction()                       */
/************************************************************************/

/**
 \brief For datasources which support transactions, StartTransaction creates a transaction.

 If starting the transaction fails, will return
 OGRERR_FAILURE. Datasources which do not support transactions will
 always return OGRERR_NONE.

 Use of this API is discouraged when the dataset offers
 dataset level transaction with GDALDataset::StartTransaction(). The reason is
 that most drivers can only offer transactions at dataset level, and not layer level.
 Very few drivers really support transactions at layer scope.

 This function is the same as the C++ method OGRLayer::StartTransaction().

 @param hLayer handle to the layer

 @return OGRERR_NONE on success.

*/

OGRErr OGR_L_StartTransaction(OGRLayerH hLayer)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_StartTransaction", OGRERR_INVALID_HANDLE);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_StartTransaction(hLayer);
#endif

    return OGRLayer::FromHandle(hLayer)->StartTransaction();
}

/************************************************************************/
/*                         CommitTransaction()                          */
/************************************************************************/

/**
 \brief For datasources which support transactions, CommitTransaction commits a transaction.

 If no transaction is active, or the commit fails, will return
 OGRERR_FAILURE. Datasources which do not support transactions will
 always return OGRERR_NONE.

 This function is the same as the C function OGR_L_CommitTransaction().

 @return OGRERR_NONE on success.
*/

OGRErr OGRLayer::CommitTransaction()

{
    return OGRERR_NONE;
}

/************************************************************************/
/*                       OGR_L_CommitTransaction()                      */
/************************************************************************/

/**
 \brief For datasources which support transactions, CommitTransaction commits a transaction.

 If no transaction is active, or the commit fails, will return
 OGRERR_FAILURE. Datasources which do not support transactions will
 always return OGRERR_NONE.

 This function is the same as the C function OGR_L_CommitTransaction().

 @return OGRERR_NONE on success.
*/

OGRErr OGR_L_CommitTransaction(OGRLayerH hLayer)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_CommitTransaction", OGRERR_INVALID_HANDLE);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_CommitTransaction(hLayer);
#endif

    return OGRLayer::FromHandle(hLayer)->CommitTransaction();
}

/************************************************************************/
/*                        RollbackTransaction()                         */
/************************************************************************/

/**
 \brief For datasources which support transactions, RollbackTransaction will roll back a datasource to its state before the start of the current transaction.
 If no transaction is active, or the rollback fails, will return
 OGRERR_FAILURE. Datasources which do not support transactions will
 always return OGRERR_NONE.

 This function is the same as the C function OGR_L_RollbackTransaction().


 OGRFeature* instances acquired or created between the StartTransaction() and RollbackTransaction() should
 be destroyed before RollbackTransaction() if the field structure has been modified during the transaction.

 In particular, the following is invalid:

 \code
 lyr->StartTransaction();
 lyr->DeleteField(...);
 f = new OGRFeature(lyr->GetLayerDefn());
 lyr->RollbackTransaction();
 // f is in a inconsistent state at this point, given its array of fields doesn't match
 // the updated layer definition, and thus it cannot even be safely deleted !
 \endcode

 Instead, the feature should be destroyed before the rollback:

 \code
 lyr->StartTransaction();
 lyr->DeleteField(...);
 f = new OGRFeature(lyr->GetLayerDefn());
 ...
 delete f;
 \endcode

 @return OGRERR_NONE on success.
*/

OGRErr OGRLayer::RollbackTransaction()

{
    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                     OGR_L_RollbackTransaction()                      */
/************************************************************************/

/**
 \brief For datasources which support transactions, RollbackTransaction will roll back a datasource to its state before the start of the current transaction.
 If no transaction is active, or the rollback fails, will return
 OGRERR_FAILURE. Datasources which do not support transactions will
 always return OGRERR_NONE.

 This function is the same as the C++ method OGRLayer::RollbackTransaction().

 @param hLayer handle to the layer

 @return OGRERR_NONE on success.
*/

OGRErr OGR_L_RollbackTransaction(OGRLayerH hLayer)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_RollbackTransaction",
                      OGRERR_INVALID_HANDLE);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_RollbackTransaction(hLayer);
#endif

    return OGRLayer::FromHandle(hLayer)->RollbackTransaction();
}

/************************************************************************/
/*                        OGRLayer::GetLayerDefn()                      */
/************************************************************************/

/**
 \fn OGRFeatureDefn *OGRLayer::GetLayerDefn();

 \brief Fetch the schema information for this layer.

 The returned OGRFeatureDefn is owned by the OGRLayer, and should not be
 modified or freed by the application.  It encapsulates the attribute schema
 of the features of the layer.

 This method is the same as the C function OGR_L_GetLayerDefn().

 @return feature definition.
*/

/**
 \fn const OGRFeatureDefn *OGRLayer::GetLayerDefn() const;

 \brief Fetch the schema information for this layer.

 The returned OGRFeatureDefn is owned by the OGRLayer, and should not be
 modified or freed by the application.  It encapsulates the attribute schema
 of the features of the layer.

 Note that even if this method is const, there is no guarantee it can be
 safely called by concurrent threads on the same GDALDataset object.

 This method is the same as the C function OGR_L_GetLayerDefn().

 @return feature definition.

 @since 3.12
*/

/************************************************************************/
/*                         OGR_L_GetLayerDefn()                         */
/************************************************************************/

/**
 \brief Fetch the schema information for this layer.

 The returned handle to the OGRFeatureDefn is owned by the OGRLayer,
 and should not be modified or freed by the application.  It encapsulates
 the attribute schema of the features of the layer.

 This function is the same as the C++ method OGRLayer::GetLayerDefn().

 @param hLayer handle to the layer to get the schema information.
 @return a handle to the feature definition.

*/
OGRFeatureDefnH OGR_L_GetLayerDefn(OGRLayerH hLayer)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_GetLayerDefn", nullptr);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_GetLayerDefn(hLayer);
#endif

    return OGRFeatureDefn::ToHandle(
        OGRLayer::FromHandle(hLayer)->GetLayerDefn());
}

/************************************************************************/
/*                         OGR_L_FindFieldIndex()                       */
/************************************************************************/

/**
 \brief Find the index of field in a layer.

 The returned number is the index of the field in the layers, or -1 if the
 field doesn't exist.

 If bExactMatch is set to FALSE and the field doesn't exist in the given form
 the driver might apply some changes to make it match, like those it might do
 if the layer was created (eg. like LAUNDER in the OCI driver).

 This method is the same as the C++ method OGRLayer::FindFieldIndex().

 @return field index, or -1 if the field doesn't exist
*/

int OGR_L_FindFieldIndex(OGRLayerH hLayer, const char *pszFieldName,
                         int bExactMatch)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_FindFieldIndex", -1);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_FindFieldIndex(hLayer, pszFieldName, bExactMatch);
#endif

    return OGRLayer::FromHandle(hLayer)->FindFieldIndex(pszFieldName,
                                                        bExactMatch);
}

/************************************************************************/
/*                           FindFieldIndex()                           */
/************************************************************************/

/**
 \brief Find the index of field in the layer.

 The returned number is the index of the field in the layers, or -1 if the
 field doesn't exist.

 If bExactMatch is set to FALSE and the field doesn't exist in the given form
 the driver might apply some changes to make it match, like those it might do
 if the layer was created (eg. like LAUNDER in the OCI driver).

 This method is the same as the C function OGR_L_FindFieldIndex().

 @return field index, or -1 if the field doesn't exist
*/

int OGRLayer::FindFieldIndex(const char *pszFieldName,
                             CPL_UNUSED int bExactMatch)
{
    return GetLayerDefn()->GetFieldIndex(pszFieldName);
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

/**
 \brief Fetch the spatial reference system for this layer.

 The returned object is owned by the OGRLayer and should not be modified
 or freed by the application.

 Note that even if this method is const (since GDAL 3.12), there is no guarantee
 it can be safely called by concurrent threads on the same GDALDataset object.

 Several geometry fields can be associated to a
 feature definition. Each geometry field can have its own spatial reference
 system, which is returned by OGRGeomFieldDefn::GetSpatialRef().
 OGRLayer::GetSpatialRef() is equivalent to
 GetLayerDefn()->OGRFeatureDefn::GetGeomFieldDefn(0)->GetSpatialRef()

 This method is the same as the C function OGR_L_GetSpatialRef().

 @return spatial reference, or NULL if there isn't one.
*/

const OGRSpatialReference *OGRLayer::GetSpatialRef() const
{
    const auto poLayerDefn = GetLayerDefn();
    if (poLayerDefn->GetGeomFieldCount() > 0)
        return const_cast<OGRSpatialReference *>(
            poLayerDefn->GetGeomFieldDefn(0)->GetSpatialRef());
    else
        return nullptr;
}

/************************************************************************/
/*                        OGR_L_GetSpatialRef()                         */
/************************************************************************/

/**
 \brief Fetch the spatial reference system for this layer.

 The returned object is owned by the OGRLayer and should not be modified
 or freed by the application.

 This function is the same as the C++ method OGRLayer::GetSpatialRef().

 @param hLayer handle to the layer to get the spatial reference from.
 @return spatial reference, or NULL if there isn't one.
*/

OGRSpatialReferenceH OGR_L_GetSpatialRef(OGRLayerH hLayer)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_GetSpatialRef", nullptr);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_GetSpatialRef(hLayer);
#endif

    return OGRSpatialReference::ToHandle(const_cast<OGRSpatialReference *>(
        OGRLayer::FromHandle(hLayer)->GetSpatialRef()));
}

/************************************************************************/
/*                     OGRLayer::TestCapability()                       */
/************************************************************************/

/**
 \fn int OGRLayer::TestCapability( const char * pszCap ) const;

 \brief Test if this layer supported the named capability.

 The capability codes that can be tested are represented as strings, but
 \#defined constants exists to ensure correct spelling.  Specific layer
 types may implement class specific capabilities, but this can't generally
 be discovered by the caller. <p>

<ul>

 <li> <b>OLCRandomRead</b> / "RandomRead": TRUE if the GetFeature() method
is implemented in an optimized way for this layer, as opposed to the default
implementation using ResetReading() and GetNextFeature() to find the requested
feature id.<p>

 <li> <b>OLCSequentialWrite</b> / "SequentialWrite": TRUE if the
CreateFeature() method works for this layer.  Note this means that this
particular layer is writable.  The same OGRLayer class may return FALSE
for other layer instances that are effectively read-only.<p>

 <li> <b>OLCRandomWrite</b> / "RandomWrite": TRUE if the SetFeature() method
is operational on this layer.  Note this means that this
particular layer is writable.  The same OGRLayer class may return FALSE
for other layer instances that are effectively read-only.<p>

 <li> <b>OLCUpsertFeature</b> / "UpsertFeature": TRUE if the UpsertFeature()
method is operational on this layer.  Note this means that this
particular layer is writable.  The same OGRLayer class may return FALSE
for other layer instances that are effectively read-only.<p>

 <li> <b>OLCFastSpatialFilter</b> / "FastSpatialFilter": TRUE if this layer
implements spatial filtering efficiently.  Layers that effectively read all
features, and test them with the OGRFeature intersection methods should
return FALSE.  This can be used as a clue by the application whether it
should build and maintain its own spatial index for features in this layer.<p>

 <li> <b>OLCFastFeatureCount</b> / "FastFeatureCount":
TRUE if this layer can return a feature
count (via GetFeatureCount()) efficiently. i.e. without counting
the features.  In some cases this will return TRUE until a spatial filter is
installed after which it will return FALSE.<p>

 <li> <b>OLCFastGetExtent</b> / "FastGetExtent":
TRUE if this layer can return its data extent (via GetExtent())
efficiently, i.e. without scanning all the features.  In some cases this
will return TRUE until a spatial filter is installed after which it will
return FALSE.<p>

 <li> <b>OLCFastSetNextByIndex</b> / "FastSetNextByIndex":
TRUE if this layer can perform the SetNextByIndex() call efficiently, otherwise
FALSE.<p>

 <li> <b>OLCCreateField</b> / "CreateField": TRUE if this layer can create
new fields on the current layer using CreateField(), otherwise FALSE.<p>

 <li> <b>OLCCreateGeomField</b> / "CreateGeomField": (GDAL >= 1.11) TRUE if this layer can create
new geometry fields on the current layer using CreateGeomField(), otherwise FALSE.<p>

 <li> <b>OLCDeleteField</b> / "DeleteField": TRUE if this layer can delete
existing fields on the current layer using DeleteField(), otherwise FALSE.<p>

 <li> <b>OLCReorderFields</b> / "ReorderFields": TRUE if this layer can reorder
existing fields on the current layer using ReorderField() or ReorderFields(), otherwise FALSE.<p>

 <li> <b>OLCAlterFieldDefn</b> / "AlterFieldDefn": TRUE if this layer can alter
the definition of an existing field on the current layer using AlterFieldDefn(), otherwise FALSE.<p>

 <li> <b>OLCAlterGeomFieldDefn</b> / "AlterGeomFieldDefn": TRUE if this layer can alter
the definition of an existing geometry field on the current layer using AlterGeomFieldDefn(), otherwise FALSE.<p>

 <li> <b>OLCDeleteFeature</b> / "DeleteFeature": TRUE if the DeleteFeature()
method is supported on this layer, otherwise FALSE.<p>

 <li> <b>OLCStringsAsUTF8</b> / "StringsAsUTF8": TRUE if values of OFTString
fields are assured to be in UTF-8 format.  If FALSE the encoding of fields
is uncertain, though it might still be UTF-8.<p>

<li> <b>OLCTransactions</b> / "Transactions": TRUE if the StartTransaction(),
CommitTransaction() and RollbackTransaction() methods work in a meaningful way,
otherwise FALSE.<p>

<li> <b>OLCIgnoreFields</b> / "IgnoreFields": TRUE if fields, geometry and style
will be omitted when fetching features as set by SetIgnoredFields() method.

<li> <b>OLCCurveGeometries</b> / "CurveGeometries": TRUE if this layer supports
writing curve geometries or may return such geometries.

<p>

</ul>

 This method is the same as the C function OGR_L_TestCapability().

 @param pszCap the name of the capability to test.

 @return TRUE if the layer has the requested capability, or FALSE otherwise.
OGRLayers will return FALSE for any unrecognized capabilities.<p>

*/

/************************************************************************/
/*                        OGR_L_TestCapability()                        */
/************************************************************************/

/**
 \brief Test if this layer supported the named capability.

 The capability codes that can be tested are represented as strings, but
 \#defined constants exists to ensure correct spelling.  Specific layer
 types may implement class specific capabilities, but this can't generally
 be discovered by the caller. <p>

<ul>

 <li> <b>OLCRandomRead</b> / "RandomRead": TRUE if the GetFeature() method
is implemented in an optimized way for this layer, as opposed to the default
implementation using ResetReading() and GetNextFeature() to find the requested
feature id.<p>

 <li> <b>OLCSequentialWrite</b> / "SequentialWrite": TRUE if the
CreateFeature() method works for this layer.  Note this means that this
particular layer is writable.  The same OGRLayer class may return FALSE
for other layer instances that are effectively read-only.<p>

 <li> <b>OLCRandomWrite</b> / "RandomWrite": TRUE if the SetFeature() method
is operational on this layer.  Note this means that this
particular layer is writable.  The same OGRLayer class may return FALSE
for other layer instances that are effectively read-only.<p>

 <li> <b>OLCUpsertFeature</b> / "UpsertFeature": TRUE if the UpsertFeature()
method is operational on this layer.  Note this means that this
particular layer is writable.  The same OGRLayer class may return FALSE
for other layer instances that are effectively read-only.<p>

 <li> <b>OLCFastSpatialFilter</b> / "FastSpatialFilter": TRUE if this layer
implements spatial filtering efficiently.  Layers that effectively read all
features, and test them with the OGRFeature intersection methods should
return FALSE.  This can be used as a clue by the application whether it
should build and maintain its own spatial index for features in this
layer.<p>

 <li> <b>OLCFastFeatureCount</b> / "FastFeatureCount":
TRUE if this layer can return a feature
count (via OGR_L_GetFeatureCount()) efficiently, i.e. without counting
the features.  In some cases this will return TRUE until a spatial filter is
installed after which it will return FALSE.<p>

 <li> <b>OLCFastGetExtent</b> / "FastGetExtent":
TRUE if this layer can return its data extent (via OGR_L_GetExtent())
efficiently, i.e. without scanning all the features.  In some cases this
will return TRUE until a spatial filter is installed after which it will
return FALSE.<p>

 <li> <b>OLCFastSetNextByIndex</b> / "FastSetNextByIndex":
TRUE if this layer can perform the SetNextByIndex() call efficiently, otherwise
FALSE.<p>

 <li> <b>OLCCreateField</b> / "CreateField": TRUE if this layer can create
new fields on the current layer using CreateField(), otherwise FALSE.<p>

 <li> <b>OLCCreateGeomField</b> / "CreateGeomField": (GDAL >= 1.11) TRUE if this layer can create
new geometry fields on the current layer using CreateGeomField(), otherwise FALSE.<p>

 <li> <b>OLCDeleteField</b> / "DeleteField": TRUE if this layer can delete
existing fields on the current layer using DeleteField(), otherwise FALSE.<p>

 <li> <b>OLCReorderFields</b> / "ReorderFields": TRUE if this layer can reorder
existing fields on the current layer using ReorderField() or ReorderFields(), otherwise FALSE.<p>

 <li> <b>OLCAlterFieldDefn</b> / "AlterFieldDefn": TRUE if this layer can alter
the definition of an existing field on the current layer using AlterFieldDefn(), otherwise FALSE.<p>

 <li> <b>OLCAlterGeomFieldDefn</b> / "AlterGeomFieldDefn": TRUE if this layer can alter
the definition of an existing geometry field on the current layer using AlterGeomFieldDefn(), otherwise FALSE.<p>

 <li> <b>OLCDeleteFeature</b> / "DeleteFeature": TRUE if the DeleteFeature()
method is supported on this layer, otherwise FALSE.<p>

 <li> <b>OLCStringsAsUTF8</b> / "StringsAsUTF8": TRUE if values of OFTString
fields are assured to be in UTF-8 format.  If FALSE the encoding of fields
is uncertain, though it might still be UTF-8.<p>

<li> <b>OLCTransactions</b> / "Transactions": TRUE if the StartTransaction(),
CommitTransaction() and RollbackTransaction() methods work in a meaningful way,
otherwise FALSE.<p>

<li> <b>OLCCurveGeometries</b> / "CurveGeometries": TRUE if this layer supports
writing curve geometries or may return such geometries.

<p>

</ul>

 This function is the same as the C++ method OGRLayer::TestCapability().

 @param hLayer handle to the layer to get the capability from.
 @param pszCap the name of the capability to test.

 @return TRUE if the layer has the requested capability, or FALSE otherwise.
OGRLayers will return FALSE for any unrecognized capabilities.<p>

*/

int OGR_L_TestCapability(OGRLayerH hLayer, const char *pszCap)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_TestCapability", 0);
    VALIDATE_POINTER1(pszCap, "OGR_L_TestCapability", 0);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_TestCapability(hLayer, pszCap);
#endif

    return OGRLayer::FromHandle(hLayer)->TestCapability(pszCap);
}

/************************************************************************/
/*                          GetSpatialFilter()                          */
/************************************************************************/

/**
 \brief This method returns the current spatial filter for this layer.

 The returned pointer is to an internally owned object, and should not
 be altered or deleted by the caller.

 This method is the same as the C function OGR_L_GetSpatialFilter().

 @return spatial filter geometry.
 */

OGRGeometry *OGRLayer::GetSpatialFilter()

{
    return m_poFilterGeom;
}

/************************************************************************/
/*                       OGR_L_GetSpatialFilter()                       */
/************************************************************************/

/**
 \brief This function returns the current spatial filter for this layer.

 The returned pointer is to an internally owned object, and should not
 be altered or deleted by the caller.

 This function is the same as the C++ method OGRLayer::GetSpatialFilter().

 @param hLayer handle to the layer to get the spatial filter from.
 @return a handle to the spatial filter geometry.
 */

OGRGeometryH OGR_L_GetSpatialFilter(OGRLayerH hLayer)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_GetSpatialFilter", nullptr);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_GetSpatialFilter(hLayer);
#endif

    return OGRGeometry::ToHandle(
        OGRLayer::FromHandle(hLayer)->GetSpatialFilter());
}

/************************************************************************/
/*             ValidateGeometryFieldIndexForSetSpatialFilter()          */
/************************************************************************/

//! @cond Doxygen_Suppress
bool OGRLayer::ValidateGeometryFieldIndexForSetSpatialFilter(
    int iGeomField, const OGRGeometry *poGeomIn, bool bIsSelectLayer)
{
    if (iGeomField == 0 && poGeomIn == nullptr &&
        GetLayerDefn()->GetGeomFieldCount() == 0)
    {
        // Setting a null spatial filter on geometry field idx 0
        // when there are no geometry field can't harm, and is accepted silently
        // for backward compatibility with existing practice.
    }
    else if (iGeomField < 0 ||
             iGeomField >= GetLayerDefn()->GetGeomFieldCount())
    {
        if (iGeomField == 0)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                bIsSelectLayer
                    ? "Cannot set spatial filter: no geometry field selected."
                    : "Cannot set spatial filter: no geometry field present in "
                      "layer.");
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot set spatial filter on non-existing geometry field "
                     "of index %d.",
                     iGeomField);
        }
        return false;
    }
    return true;
}

//! @endcond

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

/**
 \brief Set a new spatial filter.

 This method set the geometry to be used as a spatial filter when
 fetching features via the GetNextFeature() method.  Only features that
 geometrically intersect the filter geometry will be returned.

 Currently this test is may be inaccurately implemented, but it is
 guaranteed that all features whose envelope (as returned by
 OGRGeometry::getEnvelope()) overlaps the envelope of the spatial filter
 will be returned.  This can result in more shapes being returned that
 should strictly be the case.

 Features with null or empty geometries will never
 be considered as matching a spatial filter.

 This method makes an internal copy of the passed geometry.  The
 passed geometry remains the responsibility of the caller, and may
 be safely destroyed.

 For the time being the passed filter geometry should be in the same
 SRS as the layer (as returned by OGRLayer::GetSpatialRef()).  In the
 future this may be generalized.

 This method is the same as the C function OGR_L_SetSpatialFilter().

 @param poFilter the geometry to use as a filtering region.  NULL may
 be passed indicating that the current spatial filter should be cleared,
 but no new one instituted.
 */

OGRErr OGRLayer::SetSpatialFilter(const OGRGeometry *poFilter)

{
    return SetSpatialFilter(0, poFilter);
}

/**
 \brief Set a new spatial filter.

 This method set the geometry to be used as a spatial filter when
 fetching features via the GetNextFeature() method.  Only features that
 geometrically intersect the filter geometry will be returned.

 Currently this test is may be inaccurately implemented, but it is
 guaranteed that all features who's envelope (as returned by
 OGRGeometry::getEnvelope()) overlaps the envelope of the spatial filter
 will be returned.  This can result in more shapes being returned that
 should strictly be the case.

 This method makes an internal copy of the passed geometry.  The
 passed geometry remains the responsibility of the caller, and may
 be safely destroyed.

 For the time being the passed filter geometry should be in the same
 SRS as the geometry field definition it corresponds to (as returned by
 GetLayerDefn()->OGRFeatureDefn::GetGeomFieldDefn(iGeomField)->GetSpatialRef()).  In the
 future this may be generalized.

 Note that only the last spatial filter set is applied, even if several
 successive calls are done with different iGeomField values.

 This method is the same as the C function OGR_L_SetSpatialFilterEx().

 @param iGeomField index of the geometry field on which the spatial filter
 operates.
 @param poFilter the geometry to use as a filtering region.  NULL may
 be passed indicating that the current spatial filter should be cleared,
 but no new one instituted.
 */

OGRErr OGRLayer::SetSpatialFilter(int iGeomField, const OGRGeometry *poFilter)

{
    if (iGeomField == 0)
    {
        if (poFilter &&
            !ValidateGeometryFieldIndexForSetSpatialFilter(0, poFilter))
        {
            return OGRERR_FAILURE;
        }
    }
    else
    {
        if (!ValidateGeometryFieldIndexForSetSpatialFilter(iGeomField,
                                                           poFilter))
        {
            return OGRERR_FAILURE;
        }
    }

    return ISetSpatialFilter(iGeomField, poFilter);
}

/************************************************************************/
/*                         ISetSpatialFilter()                          */
/************************************************************************/

/**
 \brief Set a new spatial filter.

 Virtual method implemented by drivers since 3.11. In previous versions,
 SetSpatialFilter() / SetSpatialFilterRect() itself was the virtual method.

 Driver implementations, when wanting to call the base method, must take
 care of calling OGRLayer::ISetSpatialFilter() (and note the public method without
 the leading I).

 @param iGeomField index of the geometry field on which the spatial filter
 operates.
 @param poFilter the geometry to use as a filtering region.  NULL may
 be passed indicating that the current spatial filter should be cleared,
 but no new one instituted.

 @since GDAL 3.11
 */

OGRErr OGRLayer::ISetSpatialFilter(int iGeomField, const OGRGeometry *poFilter)

{
    m_iGeomFieldFilter = iGeomField;
    if (InstallFilter(poFilter))
        ResetReading();
    return OGRERR_NONE;
}

/************************************************************************/
/*                       OGR_L_SetSpatialFilter()                       */
/************************************************************************/

/**
 \brief Set a new spatial filter.

 This function set the geometry to be used as a spatial filter when
 fetching features via the OGR_L_GetNextFeature() function.  Only
 features that geometrically intersect the filter geometry will be
 returned.

 Currently this test is may be inaccurately implemented, but it is
 guaranteed that all features whose envelope (as returned by
 OGR_G_GetEnvelope()) overlaps the envelope of the spatial filter
 will be returned.  This can result in more shapes being returned that
 should strictly be the case.

 Features with null or empty geometries will never
 be considered as matching a spatial filter.

 This function makes an internal copy of the passed geometry.  The
 passed geometry remains the responsibility of the caller, and may
 be safely destroyed.

 For the time being the passed filter geometry should be in the same
 SRS as the layer (as returned by OGR_L_GetSpatialRef()).  In the
 future this may be generalized.

 This function is the same as the C++ method OGRLayer::SetSpatialFilter.

 @param hLayer handle to the layer on which to set the spatial filter.
 @param hGeom handle to the geometry to use as a filtering region.  NULL may
 be passed indicating that the current spatial filter should be cleared,
 but no new one instituted.

 */

void OGR_L_SetSpatialFilter(OGRLayerH hLayer, OGRGeometryH hGeom)

{
    VALIDATE_POINTER0(hLayer, "OGR_L_SetSpatialFilter");

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_SetSpatialFilter(hLayer, hGeom);
#endif

    OGRLayer::FromHandle(hLayer)->SetSpatialFilter(
        OGRGeometry::FromHandle(hGeom));
}

/************************************************************************/
/*                      OGR_L_SetSpatialFilterEx()                      */
/************************************************************************/

/**
 \brief Set a new spatial filter.

 This function set the geometry to be used as a spatial filter when
 fetching features via the OGR_L_GetNextFeature() function.  Only
 features that geometrically intersect the filter geometry will be
 returned.

 Currently this test is may be inaccurately implemented, but it is
 guaranteed that all features who's envelope (as returned by
 OGR_G_GetEnvelope()) overlaps the envelope of the spatial filter
 will be returned.  This can result in more shapes being returned that
 should strictly be the case.

 This function makes an internal copy of the passed geometry.  The
 passed geometry remains the responsibility of the caller, and may
 be safely destroyed.

 For the time being the passed filter geometry should be in the same
 SRS as the geometry field definition it corresponds to (as returned by
 GetLayerDefn()->OGRFeatureDefn::GetGeomFieldDefn(iGeomField)->GetSpatialRef()).  In the
 future this may be generalized.

 Note that only the last spatial filter set is applied, even if several
 successive calls are done with different iGeomField values.

 This function is the same as the C++ method OGRLayer::SetSpatialFilter.

 @param hLayer handle to the layer on which to set the spatial filter.
 @param iGeomField index of the geometry field on which the spatial filter
 operates.
 @param hGeom handle to the geometry to use as a filtering region.  NULL may
 be passed indicating that the current spatial filter should be cleared,
 but no new one instituted.

 */

void OGR_L_SetSpatialFilterEx(OGRLayerH hLayer, int iGeomField,
                              OGRGeometryH hGeom)

{
    VALIDATE_POINTER0(hLayer, "OGR_L_SetSpatialFilterEx");

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_SetSpatialFilterEx(hLayer, iGeomField, hGeom);
#endif

    OGRLayer::FromHandle(hLayer)->SetSpatialFilter(
        iGeomField, OGRGeometry::FromHandle(hGeom));
}

/************************************************************************/
/*                        SetSpatialFilterRect()                        */
/************************************************************************/

/**
 \brief Set a new rectangular spatial filter.

 This method set rectangle to be used as a spatial filter when
 fetching features via the GetNextFeature() method.  Only features that
 geometrically intersect the given rectangle will be returned.

 The x/y values should be in the same coordinate system as the layer as
 a whole (as returned by OGRLayer::GetSpatialRef()).   Internally this
 method is normally implemented as creating a 5 vertex closed rectangular
 polygon and passing it to OGRLayer::SetSpatialFilter().  It exists as
 a convenience.

 The only way to clear a spatial filter set with this method is to
 call OGRLayer::SetSpatialFilter(NULL).

 This method is the same as the C function OGR_L_SetSpatialFilterRect().

 @param dfMinX the minimum X coordinate for the rectangular region.
 @param dfMinY the minimum Y coordinate for the rectangular region.
 @param dfMaxX the maximum X coordinate for the rectangular region.
 @param dfMaxY the maximum Y coordinate for the rectangular region.

 */

OGRErr OGRLayer::SetSpatialFilterRect(double dfMinX, double dfMinY,
                                      double dfMaxX, double dfMaxY)

{
    return SetSpatialFilterRect(0, dfMinX, dfMinY, dfMaxX, dfMaxY);
}

/**
 \brief Set a new rectangular spatial filter.

 This method set rectangle to be used as a spatial filter when
 fetching features via the GetNextFeature() method.  Only features that
 geometrically intersect the given rectangle will be returned.

 The x/y values should be in the same coordinate system as as the geometry
 field definition it corresponds to (as returned by
 GetLayerDefn()->OGRFeatureDefn::GetGeomFieldDefn(iGeomField)->GetSpatialRef()). Internally this
 method is normally implemented as creating a 5 vertex closed rectangular
 polygon and passing it to OGRLayer::SetSpatialFilter().  It exists as
 a convenience.

 The only way to clear a spatial filter set with this method is to
 call OGRLayer::SetSpatialFilter(NULL).

 This method is the same as the C function OGR_L_SetSpatialFilterRectEx().

 @param iGeomField index of the geometry field on which the spatial filter
 operates.
 @param dfMinX the minimum X coordinate for the rectangular region.
 @param dfMinY the minimum Y coordinate for the rectangular region.
 @param dfMaxX the maximum X coordinate for the rectangular region.
 @param dfMaxY the maximum Y coordinate for the rectangular region.
 */

OGRErr OGRLayer::SetSpatialFilterRect(int iGeomField, double dfMinX,
                                      double dfMinY, double dfMaxX,
                                      double dfMaxY)

{
    auto poRing = std::make_unique<OGRLinearRing>();
    OGRPolygon oPoly;

    poRing->addPoint(dfMinX, dfMinY);
    poRing->addPoint(dfMinX, dfMaxY);
    poRing->addPoint(dfMaxX, dfMaxY);
    poRing->addPoint(dfMaxX, dfMinY);
    poRing->addPoint(dfMinX, dfMinY);

    oPoly.addRing(std::move(poRing));

    return SetSpatialFilter(iGeomField, &oPoly);
}

/************************************************************************/
/*                     OGR_L_SetSpatialFilterRect()                     */
/************************************************************************/

/**
 \brief Set a new rectangular spatial filter.

 This method set rectangle to be used as a spatial filter when
 fetching features via the OGR_L_GetNextFeature() method.  Only features that
 geometrically intersect the given rectangle will be returned.

 The x/y values should be in the same coordinate system as the layer as
 a whole (as returned by OGRLayer::GetSpatialRef()).   Internally this
 method is normally implemented as creating a 5 vertex closed rectangular
 polygon and passing it to OGRLayer::SetSpatialFilter().  It exists as
 a convenience.

 The only way to clear a spatial filter set with this method is to
 call OGRLayer::SetSpatialFilter(NULL).

 This method is the same as the C++ method OGRLayer::SetSpatialFilterRect().

 @param hLayer handle to the layer on which to set the spatial filter.
 @param dfMinX the minimum X coordinate for the rectangular region.
 @param dfMinY the minimum Y coordinate for the rectangular region.
 @param dfMaxX the maximum X coordinate for the rectangular region.
 @param dfMaxY the maximum Y coordinate for the rectangular region.

 */

void OGR_L_SetSpatialFilterRect(OGRLayerH hLayer, double dfMinX, double dfMinY,
                                double dfMaxX, double dfMaxY)

{
    VALIDATE_POINTER0(hLayer, "OGR_L_SetSpatialFilterRect");

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_SetSpatialFilterRect(hLayer, dfMinX, dfMinY, dfMaxX,
                                         dfMaxY);
#endif

    OGRLayer::FromHandle(hLayer)->SetSpatialFilterRect(dfMinX, dfMinY, dfMaxX,
                                                       dfMaxY);
}

/************************************************************************/
/*                    OGR_L_SetSpatialFilterRectEx()                    */
/************************************************************************/

/**
 \brief Set a new rectangular spatial filter.

 This method set rectangle to be used as a spatial filter when
 fetching features via the OGR_L_GetNextFeature() method.  Only features that
 geometrically intersect the given rectangle will be returned.

 The x/y values should be in the same coordinate system as as the geometry
 field definition it corresponds to (as returned by
 GetLayerDefn()->OGRFeatureDefn::GetGeomFieldDefn(iGeomField)->GetSpatialRef()). Internally this
 method is normally implemented as creating a 5 vertex closed rectangular
 polygon and passing it to OGRLayer::SetSpatialFilter().  It exists as
 a convenience.

 The only way to clear a spatial filter set with this method is to
 call OGRLayer::SetSpatialFilter(NULL).

 This method is the same as the C++ method OGRLayer::SetSpatialFilterRect().

 @param hLayer handle to the layer on which to set the spatial filter.
 @param iGeomField index of the geometry field on which the spatial filter
 operates.
 @param dfMinX the minimum X coordinate for the rectangular region.
 @param dfMinY the minimum Y coordinate for the rectangular region.
 @param dfMaxX the maximum X coordinate for the rectangular region.
 @param dfMaxY the maximum Y coordinate for the rectangular region.
*/

void OGR_L_SetSpatialFilterRectEx(OGRLayerH hLayer, int iGeomField,
                                  double dfMinX, double dfMinY, double dfMaxX,
                                  double dfMaxY)

{
    VALIDATE_POINTER0(hLayer, "OGR_L_SetSpatialFilterRectEx");

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_SetSpatialFilterRectEx(hLayer, iGeomField, dfMinX, dfMinY,
                                           dfMaxX, dfMaxY);
#endif

    OGRLayer::FromHandle(hLayer)->SetSpatialFilterRect(iGeomField, dfMinX,
                                                       dfMinY, dfMaxX, dfMaxY);
}

/************************************************************************/
/*                           InstallFilter()                            */
/*                                                                      */
/*      This method is only intended to be used from within             */
/*      drivers, normally from the SetSpatialFilter() method.           */
/*      It installs a filter, and also tests it to see if it is         */
/*      rectangular.  If so, it this is kept track of alongside the     */
/*      filter geometry itself so we can do cheaper comparisons in      */
/*      the FilterGeometry() call.                                      */
/*                                                                      */
/*      Returns TRUE if the newly installed filter differs in some      */
/*      way from the current one.                                       */
/************************************************************************/

//! @cond Doxygen_Suppress
int OGRLayer::InstallFilter(const OGRGeometry *poFilter)

{
    if (m_poFilterGeom == poFilter)
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      Replace the existing filter.                                    */
    /* -------------------------------------------------------------------- */
    if (m_poFilterGeom != nullptr)
    {
        delete m_poFilterGeom;
        m_poFilterGeom = nullptr;
    }

    if (m_pPreparedFilterGeom != nullptr)
    {
        OGRDestroyPreparedGeometry(m_pPreparedFilterGeom);
        m_pPreparedFilterGeom = nullptr;
    }

    if (poFilter != nullptr)
        m_poFilterGeom = poFilter->clone();

    m_bFilterIsEnvelope = FALSE;

    if (m_poFilterGeom == nullptr)
        return TRUE;

    m_poFilterGeom->getEnvelope(&m_sFilterEnvelope);

    /* Compile geometry filter as a prepared geometry */
    m_pPreparedFilterGeom =
        OGRCreatePreparedGeometry(OGRGeometry::ToHandle(m_poFilterGeom));

    m_bFilterIsEnvelope = m_poFilterGeom->IsRectangle();

    return TRUE;
}

//! @endcond

/************************************************************************/
/*                   DoesGeometryHavePointInEnvelope()                  */
/************************************************************************/

static bool DoesGeometryHavePointInEnvelope(const OGRGeometry *poGeometry,
                                            const OGREnvelope &sEnvelope)
{
    const OGRLineString *poLS = nullptr;

    switch (wkbFlatten(poGeometry->getGeometryType()))
    {
        case wkbPoint:
        {
            const auto poPoint = poGeometry->toPoint();
            const double x = poPoint->getX();
            const double y = poPoint->getY();
            return (x >= sEnvelope.MinX && y >= sEnvelope.MinY &&
                    x <= sEnvelope.MaxX && y <= sEnvelope.MaxY);
        }

        case wkbLineString:
            poLS = poGeometry->toLineString();
            break;

        case wkbPolygon:
        {
            const OGRPolygon *poPoly = poGeometry->toPolygon();
            poLS = poPoly->getExteriorRing();
            break;
        }

        case wkbMultiPoint:
        case wkbMultiLineString:
        case wkbMultiPolygon:
        case wkbGeometryCollection:
        {
            for (const auto &poSubGeom : *(poGeometry->toGeometryCollection()))
            {
                if (DoesGeometryHavePointInEnvelope(poSubGeom, sEnvelope))
                    return true;
            }
            return false;
        }

        default:
            return false;
    }

    if (poLS != nullptr)
    {
        const int nNumPoints = poLS->getNumPoints();
        for (int i = 0; i < nNumPoints; i++)
        {
            const double x = poLS->getX(i);
            const double y = poLS->getY(i);
            if (x >= sEnvelope.MinX && y >= sEnvelope.MinY &&
                x <= sEnvelope.MaxX && y <= sEnvelope.MaxY)
            {
                return true;
            }
        }
    }

    return false;
}

/************************************************************************/
/*                           FilterGeometry()                           */
/*                                                                      */
/*      Compare the passed in geometry to the currently installed       */
/*      filter.  Optimize for case where filter is just an              */
/*      envelope.                                                       */
/************************************************************************/

//! @cond Doxygen_Suppress
int OGRLayer::FilterGeometry(const OGRGeometry *poGeometry)

{
    /* -------------------------------------------------------------------- */
    /*      In trivial cases of new filter or target geometry, we accept    */
    /*      an intersection.  No geometry is taken to mean "the whole       */
    /*      world".                                                         */
    /* -------------------------------------------------------------------- */
    if (m_poFilterGeom == nullptr)
        return TRUE;

    if (poGeometry == nullptr || poGeometry->IsEmpty())
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      Compute the target geometry envelope, and if there is no        */
    /*      intersection between the envelopes we are sure not to have      */
    /*      any intersection.                                               */
    /* -------------------------------------------------------------------- */
    OGREnvelope sGeomEnv;

    poGeometry->getEnvelope(&sGeomEnv);

    if (sGeomEnv.MaxX < m_sFilterEnvelope.MinX ||
        sGeomEnv.MaxY < m_sFilterEnvelope.MinY ||
        m_sFilterEnvelope.MaxX < sGeomEnv.MinX ||
        m_sFilterEnvelope.MaxY < sGeomEnv.MinY)
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      If the filter geometry is its own envelope and if the           */
    /*      envelope of the geometry is inside the filter geometry,         */
    /*      the geometry itself is inside the filter geometry               */
    /* -------------------------------------------------------------------- */
    if (m_bFilterIsEnvelope && sGeomEnv.MinX >= m_sFilterEnvelope.MinX &&
        sGeomEnv.MinY >= m_sFilterEnvelope.MinY &&
        sGeomEnv.MaxX <= m_sFilterEnvelope.MaxX &&
        sGeomEnv.MaxY <= m_sFilterEnvelope.MaxY)
    {
        return TRUE;
    }
    else
    {
        // If the filter geometry is its own envelope and if the geometry has
        // at least one point inside the filter geometry, the geometry itself
        // intersects the filter geometry.
        if (m_bFilterIsEnvelope)
        {
            if (DoesGeometryHavePointInEnvelope(poGeometry, m_sFilterEnvelope))
                return true;
        }

        /* --------------------------------------------------------------------
         */
        /*      Fallback to full intersect test (using GEOS) if we still */
        /*      don't know for sure. */
        /* --------------------------------------------------------------------
         */
        if (OGRGeometryFactory::haveGEOS())
        {
            // CPLDebug("OGRLayer", "GEOS intersection");
            if (m_pPreparedFilterGeom != nullptr)
                return OGRPreparedGeometryIntersects(
                    m_pPreparedFilterGeom,
                    OGRGeometry::ToHandle(
                        const_cast<OGRGeometry *>(poGeometry)));
            else
                return m_poFilterGeom->Intersects(poGeometry);
        }
        else
            return TRUE;
    }
}

/************************************************************************/
/*                         FilterWKBGeometry()                          */
/************************************************************************/

bool OGRLayer::FilterWKBGeometry(const GByte *pabyWKB, size_t nWKBSize,
                                 bool bEnvelopeAlreadySet,
                                 OGREnvelope &sEnvelope) const
{
    OGRPreparedGeometry *pPreparedFilterGeom = m_pPreparedFilterGeom;
    bool bRet = FilterWKBGeometry(
        pabyWKB, nWKBSize, bEnvelopeAlreadySet, sEnvelope, m_poFilterGeom,
        m_bFilterIsEnvelope, m_sFilterEnvelope, pPreparedFilterGeom);
    const_cast<OGRLayer *>(this)->m_pPreparedFilterGeom = pPreparedFilterGeom;
    return bRet;
}

/* static */
bool OGRLayer::FilterWKBGeometry(const GByte *pabyWKB, size_t nWKBSize,
                                 bool bEnvelopeAlreadySet,
                                 OGREnvelope &sEnvelope,
                                 const OGRGeometry *poFilterGeom,
                                 bool bFilterIsEnvelope,
                                 const OGREnvelope &sFilterEnvelope,
                                 OGRPreparedGeometry *&pPreparedFilterGeom)
{
    if (!poFilterGeom)
        return true;

    if ((bEnvelopeAlreadySet ||
         OGRWKBGetBoundingBox(pabyWKB, nWKBSize, sEnvelope)) &&
        sFilterEnvelope.Intersects(sEnvelope))
    {
        if (bFilterIsEnvelope && sFilterEnvelope.Contains(sEnvelope))
        {
            return true;
        }
        else
        {
            if (bFilterIsEnvelope &&
                OGRWKBIntersectsPessimistic(pabyWKB, nWKBSize, sFilterEnvelope))
            {
                return true;
            }
            else if (OGRGeometryFactory::haveGEOS())
            {
                OGRGeometry *poGeom = nullptr;
                int ret = FALSE;
                if (OGRGeometryFactory::createFromWkb(pabyWKB, nullptr, &poGeom,
                                                      nWKBSize) == OGRERR_NONE)
                {
                    if (!pPreparedFilterGeom)
                    {
                        pPreparedFilterGeom =
                            OGRCreatePreparedGeometry(OGRGeometry::ToHandle(
                                const_cast<OGRGeometry *>(poFilterGeom)));
                    }
                    if (pPreparedFilterGeom)
                        ret = OGRPreparedGeometryIntersects(
                            pPreparedFilterGeom,
                            OGRGeometry::ToHandle(
                                const_cast<OGRGeometry *>(poGeom)));
                    else
                        ret = poFilterGeom->Intersects(poGeom);
                }
                delete poGeom;
                return CPL_TO_BOOL(ret);
            }
            else
            {
                // Assume intersection
                return true;
            }
        }
    }

    return false;
}

/************************************************************************/
/*                          PrepareStartTransaction()                   */
/************************************************************************/

void OGRLayer::PrepareStartTransaction()
{
    m_apoFieldDefnChanges.clear();
    m_apoGeomFieldDefnChanges.clear();
}

/************************************************************************/
/*                          FinishRollbackTransaction()                 */
/************************************************************************/

void OGRLayer::FinishRollbackTransaction(const std::string &osSavepointName)
{

    // Deleted fields can be safely removed from the storage after being restored.
    std::vector<int> toBeRemoved;

    bool bSavepointFound = false;

    // Loop through all changed fields and reset them to their previous state.
    for (int i = static_cast<int>(m_apoFieldDefnChanges.size()) - 1; i >= 0;
         i--)
    {
        auto &oFieldChange = m_apoFieldDefnChanges[i];

        if (!osSavepointName.empty())
        {
            if (oFieldChange.osSavepointName == osSavepointName)
            {
                bSavepointFound = true;
            }
            else if (bSavepointFound)
            {
                continue;
            }
        }

        CPLAssert(oFieldChange.poFieldDefn);
        const char *pszName = oFieldChange.poFieldDefn->GetNameRef();
        const int iField = oFieldChange.iField;
        if (iField >= 0)
        {
            switch (oFieldChange.eChangeType)
            {
                case FieldChangeType::DELETE_FIELD:
                {
                    // Transfer ownership of the field to the layer
                    whileUnsealing(GetLayerDefn())
                        ->AddFieldDefn(std::move(oFieldChange.poFieldDefn));

                    // Now move the field to the right place
                    // from the last position to its original position
                    const int iFieldCount = GetLayerDefn()->GetFieldCount();
                    CPLAssert(iFieldCount > 0);
                    CPLAssert(iFieldCount > iField);
                    std::vector<int> anOrder(iFieldCount);
                    for (int j = 0; j < iField; j++)
                    {
                        anOrder[j] = j;
                    }
                    for (int j = iField + 1; j < iFieldCount; j++)
                    {
                        anOrder[j] = j - 1;
                    }
                    anOrder[iField] = iFieldCount - 1;
                    if (OGRERR_NONE == whileUnsealing(GetLayerDefn())
                                           ->ReorderFieldDefns(anOrder.data()))
                    {
                        toBeRemoved.push_back(i);
                    }
                    else
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Failed to restore deleted field %s", pszName);
                    }
                    break;
                }
                case FieldChangeType::ALTER_FIELD:
                {
                    OGRFieldDefn *poFieldDefn =
                        GetLayerDefn()->GetFieldDefn(iField);
                    if (poFieldDefn)
                    {
                        *poFieldDefn = *oFieldChange.poFieldDefn;
                        toBeRemoved.push_back(i);
                    }
                    else
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Failed to restore altered field %s", pszName);
                    }
                    break;
                }
                case FieldChangeType::ADD_FIELD:
                {
                    std::unique_ptr<OGRFieldDefn> poFieldDef =
                        GetLayerDefn()->StealFieldDefn(iField);
                    if (poFieldDef)
                    {
                        oFieldChange.poFieldDefn = std::move(poFieldDef);
                    }
                    else
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Failed to delete added field %s", pszName);
                    }
                    break;
                }
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to restore field %s (field not found at index %d)",
                     pszName, iField);
        }
    }

    // Remove from the storage the deleted fields that have been restored
    for (const auto &i : toBeRemoved)
    {
        m_apoFieldDefnChanges.erase(m_apoFieldDefnChanges.begin() + i);
    }

    /**********************************************************************/
    /* Reset geometry fields to their previous state.                    */
    /**********************************************************************/

    bSavepointFound = false;

    // Loop through all changed geometry fields and reset them to their previous state.
    for (int i = static_cast<int>(m_apoGeomFieldDefnChanges.size()) - 1; i >= 0;
         i--)
    {
        auto &oGeomFieldChange = m_apoGeomFieldDefnChanges[i];

        if (!osSavepointName.empty())
        {
            if (oGeomFieldChange.osSavepointName == osSavepointName)
            {
                bSavepointFound = true;
            }
            else if (bSavepointFound)
            {
                continue;
            }
        }
        const char *pszName = oGeomFieldChange.poFieldDefn->GetNameRef();
        const int iGeomField = oGeomFieldChange.iField;
        if (iGeomField >= 0)
        {
            switch (oGeomFieldChange.eChangeType)
            {
                case FieldChangeType::DELETE_FIELD:
                case FieldChangeType::ALTER_FIELD:
                {
                    // Currently not handled by OGR for geometry fields
                    break;
                }
                case FieldChangeType::ADD_FIELD:
                {
                    std::unique_ptr<OGRGeomFieldDefn> poGeomFieldDef =
                        GetLayerDefn()->StealGeomFieldDefn(
                            oGeomFieldChange.iField);
                    if (poGeomFieldDef)
                    {
                        oGeomFieldChange.poFieldDefn =
                            std::move(poGeomFieldDef);
                    }
                    else
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Failed to delete added geometry field %s",
                                 pszName);
                    }
                    break;
                }
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to restore geometry field %s (field not found at "
                     "index %d)",
                     pszName, oGeomFieldChange.iField);
        }
    }
}

//! @endcond

/************************************************************************/
/*                    OGRLayer::ResetReading()                          */
/************************************************************************/

/**
 \fn void OGRLayer::ResetReading();

 \brief Reset feature reading to start on the first feature.

 This affects GetNextFeature() and GetArrowStream().

 This method is the same as the C function OGR_L_ResetReading().
*/

/************************************************************************/
/*                         OGR_L_ResetReading()                         */
/************************************************************************/

/**
 \brief Reset feature reading to start on the first feature.

 This affects GetNextFeature() and GetArrowStream().

 This function is the same as the C++ method OGRLayer::ResetReading().

 @param hLayer handle to the layer on which features are read.
*/

void OGR_L_ResetReading(OGRLayerH hLayer)

{
    VALIDATE_POINTER0(hLayer, "OGR_L_ResetReading");

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_ResetReading(hLayer);
#endif

    OGRLayer::FromHandle(hLayer)->ResetReading();
}

/************************************************************************/
/*                       InitializeIndexSupport()                       */
/*                                                                      */
/*      This is only intended to be called by driver layer              */
/*      implementations but we don't make it protected so that the      */
/*      datasources can do it too if that is more appropriate.          */
/************************************************************************/

//! @cond Doxygen_Suppress
OGRErr
OGRLayer::InitializeIndexSupport([[maybe_unused]] const char *pszFilename)

{
#ifdef HAVE_MITAB
    OGRErr eErr;

    if (m_poAttrIndex != nullptr)
        return OGRERR_NONE;

    m_poAttrIndex = OGRCreateDefaultLayerIndex();

    eErr = m_poAttrIndex->Initialize(pszFilename, this);
    if (eErr != OGRERR_NONE)
    {
        delete m_poAttrIndex;
        m_poAttrIndex = nullptr;
    }

    return eErr;
#else
    return OGRERR_FAILURE;
#endif
}

//! @endcond

/************************************************************************/
/*                             SyncToDisk()                             */
/************************************************************************/

/**
\brief Flush pending changes to disk.

This call is intended to force the layer to flush any pending writes to
disk, and leave the disk file in a consistent state.  It would not normally
have any effect on read-only datasources.

Some layers do not implement this method, and will still return
OGRERR_NONE.  The default implementation just returns OGRERR_NONE.  An error
is only returned if an error occurs while attempting to flush to disk.

In any event, you should always close any opened datasource with
OGRDataSource::DestroyDataSource() that will ensure all data is correctly flushed.

This method is the same as the C function OGR_L_SyncToDisk().

@return OGRERR_NONE if no error occurs (even if nothing is done) or an
error code.
*/

OGRErr OGRLayer::SyncToDisk()

{
    return OGRERR_NONE;
}

/************************************************************************/
/*                          OGR_L_SyncToDisk()                          */
/************************************************************************/

/**
\brief Flush pending changes to disk.

This call is intended to force the layer to flush any pending writes to
disk, and leave the disk file in a consistent state.  It would not normally
have any effect on read-only datasources.

Some layers do not implement this method, and will still return
OGRERR_NONE.  The default implementation just returns OGRERR_NONE.  An error
is only returned if an error occurs while attempting to flush to disk.

In any event, you should always close any opened datasource with
OGR_DS_Destroy() that will ensure all data is correctly flushed.

This method is the same as the C++ method OGRLayer::SyncToDisk()

@param hLayer handle to the layer

@return OGRERR_NONE if no error occurs (even if nothing is done) or an
error code.
*/

OGRErr OGR_L_SyncToDisk(OGRLayerH hLayer)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_SyncToDisk", OGRERR_INVALID_HANDLE);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_SyncToDisk(hLayer);
#endif

    return OGRLayer::FromHandle(hLayer)->SyncToDisk();
}

/************************************************************************/
/*                           DeleteFeature()                            */
/************************************************************************/

/**
 \brief Delete feature from layer.

 The feature with the indicated feature id is deleted from the layer if
 supported by the driver.  Most drivers do not support feature deletion,
 and will return OGRERR_UNSUPPORTED_OPERATION.  The TestCapability()
 layer method may be called with OLCDeleteFeature to check if the driver
 supports feature deletion.

 This method is the same as the C function OGR_L_DeleteFeature().

 @param nFID the feature id to be deleted from the layer

 @return OGRERR_NONE if the operation works, otherwise an appropriate error
 code (e.g OGRERR_NON_EXISTING_FEATURE if the feature does not exist).

*/

OGRErr OGRLayer::DeleteFeature(CPL_UNUSED GIntBig nFID)
{
    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                        OGR_L_DeleteFeature()                         */
/************************************************************************/

/**
 \brief Delete feature from layer.

 The feature with the indicated feature id is deleted from the layer if
 supported by the driver.  Most drivers do not support feature deletion,
 and will return OGRERR_UNSUPPORTED_OPERATION.  The OGR_L_TestCapability()
 function may be called with OLCDeleteFeature to check if the driver
 supports feature deletion.

 This method is the same as the C++ method OGRLayer::DeleteFeature().

 @param hLayer handle to the layer
 @param nFID the feature id to be deleted from the layer

 @return OGRERR_NONE if the operation works, otherwise an appropriate error
 code (e.g OGRERR_NON_EXISTING_FEATURE if the feature does not exist).
*/

OGRErr OGR_L_DeleteFeature(OGRLayerH hLayer, GIntBig nFID)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_DeleteFeature", OGRERR_INVALID_HANDLE);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_DeleteFeature(hLayer, nFID);
#endif

    return OGRLayer::FromHandle(hLayer)->DeleteFeature(nFID);
}

/************************************************************************/
/*                          GetFeaturesRead()                           */
/************************************************************************/

//! @cond Doxygen_Suppress
GIntBig OGRLayer::GetFeaturesRead()

{
    return m_nFeaturesRead;
}

//! @endcond

/************************************************************************/
/*                       OGR_L_GetFeaturesRead()                        */
/************************************************************************/

GIntBig OGR_L_GetFeaturesRead(OGRLayerH hLayer)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_GetFeaturesRead", 0);

    return OGRLayer::FromHandle(hLayer)->GetFeaturesRead();
}

/************************************************************************/
/*                             GetFIDColumn                             */
/************************************************************************/

/**
 \brief This method returns the name of the underlying database column being used as the FID column, or "" if not supported.

 This method is the same as the C function OGR_L_GetFIDColumn().

 @return fid column name.
*/

const char *OGRLayer::GetFIDColumn() const

{
    return "";
}

/************************************************************************/
/*                         OGR_L_GetFIDColumn()                         */
/************************************************************************/

/**
 \brief This method returns the name of the underlying database column being used as the FID column, or "" if not supported.

 This method is the same as the C++ method OGRLayer::GetFIDColumn()

 @param hLayer handle to the layer
 @return fid column name.
*/

const char *OGR_L_GetFIDColumn(OGRLayerH hLayer)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_GetFIDColumn", nullptr);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_GetFIDColumn(hLayer);
#endif

    return OGRLayer::FromHandle(hLayer)->GetFIDColumn();
}

/************************************************************************/
/*                         GetGeometryColumn()                          */
/************************************************************************/

/**
 \brief This method returns the name of the underlying database column being used as the geometry column, or "" if not supported.

 For layers with multiple geometry fields, this method only returns the name
 of the first geometry column. For other columns, use
 GetLayerDefn()->OGRFeatureDefn::GetGeomFieldDefn(i)->GetNameRef().

 This method is the same as the C function OGR_L_GetGeometryColumn().

 @return geometry column name.
*/

const char *OGRLayer::GetGeometryColumn() const

{
    const auto poLayerDefn = GetLayerDefn();
    if (poLayerDefn->GetGeomFieldCount() > 0)
        return poLayerDefn->GetGeomFieldDefn(0)->GetNameRef();
    else
        return "";
}

/************************************************************************/
/*                      OGR_L_GetGeometryColumn()                       */
/************************************************************************/

/**
 \brief This method returns the name of the underlying database column being used as the geometry column, or "" if not supported.

 For layers with multiple geometry fields, this method only returns the geometry
 type of the first geometry column. For other columns, use
 OGR_GFld_GetNameRef(OGR_FD_GetGeomFieldDefn(OGR_L_GetLayerDefn(hLayer), i)).

 This method is the same as the C++ method OGRLayer::GetGeometryColumn()

 @param hLayer handle to the layer
 @return geometry column name.
*/

const char *OGR_L_GetGeometryColumn(OGRLayerH hLayer)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_GetGeometryColumn", nullptr);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_GetGeometryColumn(hLayer);
#endif

    return OGRLayer::FromHandle(hLayer)->GetGeometryColumn();
}

/************************************************************************/
/*                            GetStyleTable()                           */
/************************************************************************/

/**
 \brief Returns layer style table.

 This method is the same as the C function OGR_L_GetStyleTable().

 @return pointer to a style table which should not be modified or freed by the
 caller.
*/

OGRStyleTable *OGRLayer::GetStyleTable()
{
    return m_poStyleTable;
}

/************************************************************************/
/*                         SetStyleTableDirectly()                      */
/************************************************************************/

/**
 \brief Set layer style table.

 This method operate exactly as OGRLayer::SetStyleTable() except that it
 assumes ownership of the passed table.

 This method is the same as the C function OGR_L_SetStyleTableDirectly().

 @param poStyleTable pointer to style table to set
*/

void OGRLayer::SetStyleTableDirectly(OGRStyleTable *poStyleTable)
{
    if (m_poStyleTable)
        delete m_poStyleTable;
    m_poStyleTable = poStyleTable;
}

/************************************************************************/
/*                            SetStyleTable()                           */
/************************************************************************/

/**
 \brief Set layer style table.

 This method operate exactly as OGRLayer::SetStyleTableDirectly() except
 that it does not assume ownership of the passed table.

 This method is the same as the C function OGR_L_SetStyleTable().

 @param poStyleTable pointer to style table to set
*/

void OGRLayer::SetStyleTable(OGRStyleTable *poStyleTable)
{
    if (m_poStyleTable)
        delete m_poStyleTable;
    if (poStyleTable)
        m_poStyleTable = poStyleTable->Clone();
}

/************************************************************************/
/*                         OGR_L_GetStyleTable()                        */
/************************************************************************/

OGRStyleTableH OGR_L_GetStyleTable(OGRLayerH hLayer)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_GetStyleTable", nullptr);

    return reinterpret_cast<OGRStyleTableH>(
        OGRLayer::FromHandle(hLayer)->GetStyleTable());
}

/************************************************************************/
/*                         OGR_L_SetStyleTableDirectly()                */
/************************************************************************/

void OGR_L_SetStyleTableDirectly(OGRLayerH hLayer, OGRStyleTableH hStyleTable)

{
    VALIDATE_POINTER0(hLayer, "OGR_L_SetStyleTableDirectly");

    OGRLayer::FromHandle(hLayer)->SetStyleTableDirectly(
        reinterpret_cast<OGRStyleTable *>(hStyleTable));
}

/************************************************************************/
/*                         OGR_L_SetStyleTable()                        */
/************************************************************************/

void OGR_L_SetStyleTable(OGRLayerH hLayer, OGRStyleTableH hStyleTable)

{
    VALIDATE_POINTER0(hLayer, "OGR_L_SetStyleTable");
    VALIDATE_POINTER0(hStyleTable, "OGR_L_SetStyleTable");

    OGRLayer::FromHandle(hLayer)->SetStyleTable(
        reinterpret_cast<OGRStyleTable *>(hStyleTable));
}

/************************************************************************/
/*                               GetName()                              */
/************************************************************************/

/**
 \brief Return the layer name.

 This returns the same content as GetLayerDefn()->OGRFeatureDefn::GetName(), but for a
 few drivers, calling GetName() directly can avoid lengthy layer
 definition initialization.

 This method is the same as the C function OGR_L_GetName().

 If this method is derived in a driver, it must be done such that it
 returns the same content as GetLayerDefn()->OGRFeatureDefn::GetName().

 @return the layer name (must not been freed)
*/

const char *OGRLayer::GetName() const

{
    return GetLayerDefn()->GetName();
}

/************************************************************************/
/*                           OGR_L_GetName()                            */
/************************************************************************/

/**
 \brief Return the layer name.

 This returns the same content as OGR_FD_GetName(OGR_L_GetLayerDefn(hLayer)),
 but for a few drivers, calling OGR_L_GetName() directly can avoid lengthy
 layer definition initialization.

 This function is the same as the C++ method OGRLayer::GetName().

 @param hLayer handle to the layer.
 @return the layer name (must not been freed)
*/

const char *OGR_L_GetName(OGRLayerH hLayer)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_GetName", "");

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_GetName(hLayer);
#endif

    return OGRLayer::FromHandle(hLayer)->GetName();
}

/************************************************************************/
/*                            GetGeomType()                             */
/************************************************************************/

/**
 \brief Return the layer geometry type.

 This returns the same result as GetLayerDefn()->OGRFeatureDefn::GetGeomType(), but for a
 few drivers, calling GetGeomType() directly can avoid lengthy layer
 definition initialization.

 Note that even if this method is const (since GDAL 3.12), there is no guarantee
 it can be safely called by concurrent threads on the same GDALDataset object.

 For layers with multiple geometry fields, this method only returns the geometry
 type of the first geometry column. For other columns, use
 GetLayerDefn()->OGRFeatureDefn::GetGeomFieldDefn(i)->GetType().
 For layers without any geometry field, this method returns wkbNone.

 This method is the same as the C function OGR_L_GetGeomType().

 If this method is derived in a driver, it must be done such that it
 returns the same content as GetLayerDefn()->OGRFeatureDefn::GetGeomType().

 @return the geometry type
*/

OGRwkbGeometryType OGRLayer::GetGeomType() const
{
    const OGRFeatureDefn *poLayerDefn = GetLayerDefn();
    if (poLayerDefn == nullptr)
    {
        CPLDebug("OGR", "GetLayerType() returns NULL !");
        return wkbUnknown;
    }
    return poLayerDefn->GetGeomType();
}

/************************************************************************/
/*                         OGR_L_GetGeomType()                          */
/************************************************************************/

/**
 \brief Return the layer geometry type.

 This returns the same result as OGR_FD_GetGeomType(OGR_L_GetLayerDefn(hLayer)),
 but for a few drivers, calling OGR_L_GetGeomType() directly can avoid lengthy
 layer definition initialization.

 For layers with multiple geometry fields, this method only returns the geometry
 type of the first geometry column. For other columns, use
 OGR_GFld_GetType(OGR_FD_GetGeomFieldDefn(OGR_L_GetLayerDefn(hLayer), i)).
 For layers without any geometry field, this method returns wkbNone.

 This function is the same as the C++ method OGRLayer::GetGeomType().

 @param hLayer handle to the layer.
 @return the geometry type
*/

OGRwkbGeometryType OGR_L_GetGeomType(OGRLayerH hLayer)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_GetGeomType", wkbUnknown);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_GetGeomType(hLayer);
#endif

    OGRwkbGeometryType eType = OGRLayer::FromHandle(hLayer)->GetGeomType();
    if (OGR_GT_IsNonLinear(eType) && !OGRGetNonLinearGeometriesEnabledFlag())
    {
        eType = OGR_GT_GetLinear(eType);
    }
    return eType;
}

/************************************************************************/
/*                          SetIgnoredFields()                          */
/************************************************************************/

/**
 \brief Set which fields can be omitted when retrieving features from the layer.

 If the driver supports this functionality (testable using OLCIgnoreFields capability), it will not fetch the specified fields
 in subsequent calls to GetFeature() / GetNextFeature() and thus save some processing time and/or bandwidth.

 Besides field names of the layers, the following special fields can be passed: "OGR_GEOMETRY" to ignore geometry and
 "OGR_STYLE" to ignore layer style.

 By default, no fields are ignored.

 Note that fields that are used in an attribute filter should generally not be set as
 ignored fields, as most drivers (such as those relying on the OGR SQL engine)
 will be unable to correctly evaluate the attribute filter.

 This method is the same as the C function OGR_L_SetIgnoredFields()

 @param papszFields an array of field names terminated by NULL item. If NULL is passed, the ignored list is cleared.
 @return OGRERR_NONE if all field names have been resolved (even if the driver does not support this method)
*/

OGRErr OGRLayer::SetIgnoredFields(CSLConstList papszFields)
{
    OGRFeatureDefn *poDefn = GetLayerDefn();

    // first set everything as *not* ignored
    for (int iField = 0; iField < poDefn->GetFieldCount(); iField++)
    {
        poDefn->GetFieldDefn(iField)->SetIgnored(FALSE);
    }
    for (int iField = 0; iField < poDefn->GetGeomFieldCount(); iField++)
    {
        poDefn->GetGeomFieldDefn(iField)->SetIgnored(FALSE);
    }
    poDefn->SetStyleIgnored(FALSE);

    // ignore some fields
    for (const char *pszFieldName : cpl::Iterate(papszFields))
    {
        // check special fields
        if (EQUAL(pszFieldName, "OGR_GEOMETRY"))
            poDefn->SetGeometryIgnored(TRUE);
        else if (EQUAL(pszFieldName, "OGR_STYLE"))
            poDefn->SetStyleIgnored(TRUE);
        else
        {
            // check ordinary fields
            int iField = poDefn->GetFieldIndex(pszFieldName);
            if (iField == -1)
            {
                // check geometry field
                iField = poDefn->GetGeomFieldIndex(pszFieldName);
                if (iField == -1)
                {
                    return OGRERR_FAILURE;
                }
                else
                    poDefn->GetGeomFieldDefn(iField)->SetIgnored(TRUE);
            }
            else
                poDefn->GetFieldDefn(iField)->SetIgnored(TRUE);
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                       OGR_L_SetIgnoredFields()                       */
/************************************************************************/

/**
 \brief Set which fields can be omitted when retrieving features from the layer.

 If the driver supports this functionality (testable using OLCIgnoreFields capability), it will not fetch the specified fields
 in subsequent calls to GetFeature() / GetNextFeature() and thus save some processing time and/or bandwidth.

 Besides field names of the layers, the following special fields can be passed: "OGR_GEOMETRY" to ignore geometry and
 "OGR_STYLE" to ignore layer style.

 By default, no fields are ignored.

 Note that fields that are used in an attribute filter should generally not be set as
 ignored fields, as most drivers (such as those relying on the OGR SQL engine)
 will be unable to correctly evaluate the attribute filter.

 This method is the same as the C++ method OGRLayer::SetIgnoredFields()

 @param hLayer handle to the layer
 @param papszFields an array of field names terminated by NULL item. If NULL is passed, the ignored list is cleared.
 @return OGRERR_NONE if all field names have been resolved (even if the driver does not support this method)
*/

OGRErr OGR_L_SetIgnoredFields(OGRLayerH hLayer, const char **papszFields)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_SetIgnoredFields", OGRERR_INVALID_HANDLE);

#ifdef OGRAPISPY_ENABLED
    if (bOGRAPISpyEnabled)
        OGRAPISpy_L_SetIgnoredFields(hLayer, papszFields);
#endif

    return OGRLayer::FromHandle(hLayer)->SetIgnoredFields(papszFields);
}

/************************************************************************/
/*                             Rename()                                 */
/************************************************************************/

/** Rename layer.
 *
 * This operation is implemented only by layers that expose the OLCRename
 * capability, and drivers that expose the GDAL_DCAP_RENAME_LAYERS capability
 *
 * This operation will fail if a layer with the new name already exists.
 *
 * On success, GetDescription() and GetLayerDefn()->GetName() will return
 * pszNewName.
 *
 * Renaming the layer may interrupt current feature iteration.
 *
 * @param pszNewName New layer name. Must not be NULL.
 * @return OGRERR_NONE in case of success
 *
 * @since GDAL 3.5
 */
OGRErr OGRLayer::Rename(CPL_UNUSED const char *pszNewName)
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "Rename() not supported by this layer.");

    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                           OGR_L_Rename()                             */
/************************************************************************/

/** Rename layer.
 *
 * This operation is implemented only by layers that expose the OLCRename
 * capability, and drivers that expose the GDAL_DCAP_RENAME_LAYERS capability
 *
 * This operation will fail if a layer with the new name already exists.
 *
 * On success, GetDescription() and GetLayerDefn()->GetName() will return
 * pszNewName.
 *
 * Renaming the layer may interrupt current feature iteration.
 *
 * @param hLayer     Layer to rename.
 * @param pszNewName New layer name. Must not be NULL.
 * @return OGRERR_NONE in case of success
 *
 * @since GDAL 3.5
 */
OGRErr OGR_L_Rename(OGRLayerH hLayer, const char *pszNewName)

{
    VALIDATE_POINTER1(hLayer, "OGR_L_Rename", OGRERR_INVALID_HANDLE);
    VALIDATE_POINTER1(pszNewName, "OGR_L_Rename", OGRERR_FAILURE);

    return OGRLayer::FromHandle(hLayer)->Rename(pszNewName);
}

/************************************************************************/
/*         helper functions for layer overlay methods                   */
/************************************************************************/

static OGRErr clone_spatial_filter(OGRLayer *pLayer, OGRGeometry **ppGeometry)
{
    OGRErr ret = OGRERR_NONE;
    OGRGeometry *g = pLayer->GetSpatialFilter();
    *ppGeometry = g ? g->clone() : nullptr;
    return ret;
}

static OGRErr create_field_map(OGRFeatureDefn *poDefn, int **map)
{
    OGRErr ret = OGRERR_NONE;
    int n = poDefn->GetFieldCount();
    if (n > 0)
    {
        *map = static_cast<int *>(VSI_MALLOC_VERBOSE(sizeof(int) * n));
        if (!(*map))
            return OGRERR_NOT_ENOUGH_MEMORY;
        for (int i = 0; i < n; i++)
            (*map)[i] = -1;
    }
    return ret;
}

static OGRErr set_result_schema(OGRLayer *pLayerResult,
                                OGRFeatureDefn *poDefnInput,
                                OGRFeatureDefn *poDefnMethod, int *mapInput,
                                int *mapMethod, bool combined,
                                const char *const *papszOptions)
{
    if (!CPLTestBool(CSLFetchNameValueDef(papszOptions, "ADD_FIELDS", "YES")))
        return OGRERR_NONE;

    OGRErr ret = OGRERR_NONE;
    OGRFeatureDefn *poDefnResult = pLayerResult->GetLayerDefn();
    const char *pszInputPrefix =
        CSLFetchNameValue(papszOptions, "INPUT_PREFIX");
    const char *pszMethodPrefix =
        CSLFetchNameValue(papszOptions, "METHOD_PREFIX");
    const bool bSkipFailures =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "SKIP_FAILURES", "NO"));
    if (poDefnResult->GetFieldCount() > 0)
    {
        // the user has defined the schema of the output layer
        if (mapInput)
        {
            for (int iField = 0; iField < poDefnInput->GetFieldCount();
                 iField++)
            {
                CPLString osName(
                    poDefnInput->GetFieldDefn(iField)->GetNameRef());
                if (pszInputPrefix != nullptr)
                    osName = pszInputPrefix + osName;
                mapInput[iField] = poDefnResult->GetFieldIndex(osName);
            }
        }
        if (!mapMethod)
            return ret;
        // cppcheck-suppress nullPointer
        for (int iField = 0; iField < poDefnMethod->GetFieldCount(); iField++)
        {
            // cppcheck-suppress nullPointer
            CPLString osName(poDefnMethod->GetFieldDefn(iField)->GetNameRef());
            if (pszMethodPrefix != nullptr)
                osName = pszMethodPrefix + osName;
            mapMethod[iField] = poDefnResult->GetFieldIndex(osName);
        }
    }
    else
    {
        // use schema from the input layer or from input and method layers
        const int nFieldsInput = poDefnInput->GetFieldCount();

        // If no prefix is specified and we have input+method layers, make
        // sure we will generate unique field names
        std::set<std::string> oSetInputFieldNames;
        std::set<std::string> oSetMethodFieldNames;
        if (poDefnMethod != nullptr && pszInputPrefix == nullptr &&
            pszMethodPrefix == nullptr)
        {
            for (int iField = 0; iField < nFieldsInput; iField++)
            {
                oSetInputFieldNames.insert(
                    poDefnInput->GetFieldDefn(iField)->GetNameRef());
            }
            const int nFieldsMethod = poDefnMethod->GetFieldCount();
            for (int iField = 0; iField < nFieldsMethod; iField++)
            {
                oSetMethodFieldNames.insert(
                    poDefnMethod->GetFieldDefn(iField)->GetNameRef());
            }
        }

        const bool bAddInputFields = CPLTestBool(
            CSLFetchNameValueDef(papszOptions, "ADD_INPUT_FIELDS", "YES"));
        if (bAddInputFields)
        {
            for (int iField = 0; iField < nFieldsInput; iField++)
            {
                OGRFieldDefn oFieldDefn(poDefnInput->GetFieldDefn(iField));
                if (pszInputPrefix != nullptr)
                    oFieldDefn.SetName(CPLSPrintf("%s%s", pszInputPrefix,
                                                  oFieldDefn.GetNameRef()));
                else if (!oSetMethodFieldNames.empty() &&
                         oSetMethodFieldNames.find(oFieldDefn.GetNameRef()) !=
                             oSetMethodFieldNames.end())
                {
                    // Field of same name present in method layer
                    oFieldDefn.SetName(
                        CPLSPrintf("input_%s", oFieldDefn.GetNameRef()));
                }
                ret = pLayerResult->CreateField(&oFieldDefn);
                if (ret != OGRERR_NONE)
                {
                    if (!bSkipFailures)
                        return ret;
                    else
                    {
                        CPLErrorReset();
                        ret = OGRERR_NONE;
                    }
                }
                if (mapInput)
                    mapInput[iField] =
                        pLayerResult->GetLayerDefn()->GetFieldCount() - 1;
            }
        }

        if (!combined)
            return ret;
        if (!mapMethod)
            return ret;
        if (!poDefnMethod)
            return ret;

        const bool bAddMethodFields = CPLTestBool(
            CSLFetchNameValueDef(papszOptions, "ADD_METHOD_FIELDS", "YES"));
        if (bAddMethodFields)
        {
            const int nFieldsMethod = poDefnMethod->GetFieldCount();
            for (int iField = 0; iField < nFieldsMethod; iField++)
            {
                OGRFieldDefn oFieldDefn(poDefnMethod->GetFieldDefn(iField));
                if (pszMethodPrefix != nullptr)
                    oFieldDefn.SetName(CPLSPrintf("%s%s", pszMethodPrefix,
                                                  oFieldDefn.GetNameRef()));
                else if (!oSetInputFieldNames.empty() &&
                         oSetInputFieldNames.find(oFieldDefn.GetNameRef()) !=
                             oSetInputFieldNames.end())
                {
                    // Field of same name present in method layer
                    oFieldDefn.SetName(
                        CPLSPrintf("method_%s", oFieldDefn.GetNameRef()));
                }
                ret = pLayerResult->CreateField(&oFieldDefn);
                if (ret != OGRERR_NONE)
                {
                    if (!bSkipFailures)
                        return ret;
                    else
                    {
                        CPLErrorReset();
                        ret = OGRERR_NONE;
                    }
                }
                mapMethod[iField] =
                    pLayerResult->GetLayerDefn()->GetFieldCount() - 1;
            }
        }
    }
    return ret;
}

static OGRGeometry *set_filter_from(OGRLayer *pLayer,
                                    OGRGeometry *pGeometryExistingFilter,
                                    OGRFeature *pFeature)
{
    OGRGeometry *geom = pFeature->GetGeometryRef();
    if (!geom)
        return nullptr;
    if (pGeometryExistingFilter)
    {
        if (!geom->Intersects(pGeometryExistingFilter))
            return nullptr;
        OGRGeometry *intersection = geom->Intersection(pGeometryExistingFilter);
        if (intersection)
        {
            pLayer->SetSpatialFilter(intersection);
            delete intersection;
        }
        else
            return nullptr;
    }
    else
    {
        pLayer->SetSpatialFilter(geom);
    }
    return geom;
}

static OGRGeometry *promote_to_multi(OGRGeometry *poGeom)
{
    OGRwkbGeometryType eType = wkbFlatten(poGeom->getGeometryType());
    if (eType == wkbPoint)
        return OGRGeometryFactory::forceToMultiPoint(poGeom);
    else if (eType == wkbPolygon)
        return OGRGeometryFactory::forceToMultiPolygon(poGeom);
    else if (eType == wkbLineString)
        return OGRGeometryFactory::forceToMultiLineString(poGeom);
    else
        return poGeom;
}

/************************************************************************/
/*                          Intersection()                              */
/************************************************************************/
/**
 * \brief Intersection of two layers.
 *
 * The result layer contains features whose geometries represent areas
 * that are common between features in the input layer and in the
 * method layer. The features in the result layer have attributes from
 * both input and method layers. The schema of the result layer can be
 * set by the user or, if it is empty, is initialized to contain all
 * fields in the input and method layers.
 *
 * \note If the schema of the result is set by user and contains
 * fields that have the same name as a field in input and in method
 * layer, then the attribute in the result feature will get the value
 * from the feature of the method layer.
 *
 * \note For best performance use the minimum amount of features in
 * the method layer and copy it into a memory layer.
 *
 * \note This method relies on GEOS support. Do not use unless the
 * GEOS support is compiled in.
 *
 * The recognized list of options is:
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set to YES to go on, even when a
 *     feature could not be inserted or a GEOS call failed.
 * </li>
 * <li>PROMOTE_TO_MULTI=YES/NO. Set to YES to convert Polygons
 *     into MultiPolygons, LineStrings to MultiLineStrings or
 *     Points to MultiPoints (only since GDAL 3.9.2 for the later)
 * </li>
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * </li>
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * </li>
 * <li>USE_PREPARED_GEOMETRIES=YES/NO. Set to NO to not use prepared
 *     geometries to pretest intersection of features of method layer
 *     with features of this layer.
 * </li>
 * <li>PRETEST_CONTAINMENT=YES/NO. Set to YES to pretest the
 *     containment of features of method layer within the features of
 *     this layer. This will speed up the method significantly in some
 *     cases. Requires that the prepared geometries are in effect.
 * </li>
 * <li>KEEP_LOWER_DIMENSION_GEOMETRIES=YES/NO. Set to NO to skip
 *     result features with lower dimension geometry that would
 *     otherwise be added to the result layer. The default is YES, to add
 *     features with lower dimension geometry, but only if the result layer
 *     has an unknown geometry type.
 * </li>
 * </ul>
 *
 * This method is the same as the C function OGR_L_Intersection().
 *
 * @param pLayerMethod the method layer. Should not be NULL.
 *
 * @param pLayerResult the layer where the features resulting from the
 * operation are inserted. Should not be NULL. See above the note
 * about the schema.
 *
 * @param papszOptions NULL terminated list of options (may be NULL).
 *
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 *
 * @param pProgressArg argument to be passed to pfnProgress. May be NULL.
 *
 * @return an error code if there was an error or the execution was
 * interrupted, OGRERR_NONE otherwise.
 *
 * @note The first geometry field is always used.
 *
 * @since OGR 1.10
 */

OGRErr OGRLayer::Intersection(OGRLayer *pLayerMethod, OGRLayer *pLayerResult,
                              char **papszOptions, GDALProgressFunc pfnProgress,
                              void *pProgressArg)
{
    OGRErr ret = OGRERR_NONE;
    OGRFeatureDefn *poDefnInput = GetLayerDefn();
    OGRFeatureDefn *poDefnMethod = pLayerMethod->GetLayerDefn();
    OGRFeatureDefn *poDefnResult = nullptr;
    OGRGeometry *pGeometryMethodFilter = nullptr;
    int *mapInput = nullptr;
    int *mapMethod = nullptr;
    OGREnvelope sEnvelopeMethod;
    GBool bEnvelopeSet;
    double progress_max = static_cast<double>(GetFeatureCount(FALSE));
    double progress_counter = 0;
    double progress_ticker = 0;
    const bool bSkipFailures =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "SKIP_FAILURES", "NO"));
    const bool bPromoteToMulti = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "PROMOTE_TO_MULTI", "NO"));
    const bool bUsePreparedGeometries = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "USE_PREPARED_GEOMETRIES", "YES"));
    const bool bPretestContainment = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "PRETEST_CONTAINMENT", "NO"));
    bool bKeepLowerDimGeom = CPLTestBool(CSLFetchNameValueDef(
        papszOptions, "KEEP_LOWER_DIMENSION_GEOMETRIES", "YES"));

    // check for GEOS
    if (!OGRGeometryFactory::haveGEOS())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "OGRLayer::Intersection() requires GEOS support");
        return OGRERR_UNSUPPORTED_OPERATION;
    }

    // get resources
    ret = clone_spatial_filter(pLayerMethod, &pGeometryMethodFilter);
    if (ret != OGRERR_NONE)
        goto done;
    ret = create_field_map(poDefnInput, &mapInput);
    if (ret != OGRERR_NONE)
        goto done;
    ret = create_field_map(poDefnMethod, &mapMethod);
    if (ret != OGRERR_NONE)
        goto done;
    ret = set_result_schema(pLayerResult, poDefnInput, poDefnMethod, mapInput,
                            mapMethod, true, papszOptions);
    if (ret != OGRERR_NONE)
        goto done;
    poDefnResult = pLayerResult->GetLayerDefn();
    bEnvelopeSet = pLayerMethod->GetExtent(&sEnvelopeMethod, 1) == OGRERR_NONE;
    if (bKeepLowerDimGeom)
    {
        // require that the result layer is of geom type unknown
        if (pLayerResult->GetGeomType() != wkbUnknown)
        {
            CPLDebug("OGR", "Resetting KEEP_LOWER_DIMENSION_GEOMETRIES to NO "
                            "since the result layer does not allow it.");
            bKeepLowerDimGeom = false;
        }
    }

    for (auto &&x : this)
    {

        if (pfnProgress)
        {
            double p = progress_counter / progress_max;
            if (p > progress_ticker)
            {
                if (!pfnProgress(p, "", pProgressArg))
                {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        // is it worth to proceed?
        if (bEnvelopeSet)
        {
            OGRGeometry *x_geom = x->GetGeometryRef();
            if (x_geom)
            {
                OGREnvelope x_env;
                x_geom->getEnvelope(&x_env);
                if (x_env.MaxX < sEnvelopeMethod.MinX ||
                    x_env.MaxY < sEnvelopeMethod.MinY ||
                    sEnvelopeMethod.MaxX < x_env.MinX ||
                    sEnvelopeMethod.MaxY < x_env.MinY)
                {
                    continue;
                }
            }
            else
            {
                continue;
            }
        }

        // set up the filter for method layer
        CPLErrorReset();
        OGRGeometry *x_geom =
            set_filter_from(pLayerMethod, pGeometryMethodFilter, x.get());
        if (CPLGetLastErrorType() != CE_None)
        {
            if (!bSkipFailures)
            {
                ret = OGRERR_FAILURE;
                goto done;
            }
            else
            {
                CPLErrorReset();
                ret = OGRERR_NONE;
            }
        }
        if (!x_geom)
        {
            continue;
        }

        OGRPreparedGeometryUniquePtr x_prepared_geom;
        if (bUsePreparedGeometries)
        {
            x_prepared_geom.reset(
                OGRCreatePreparedGeometry(OGRGeometry::ToHandle(x_geom)));
            if (!x_prepared_geom)
            {
                goto done;
            }
        }

        for (auto &&y : pLayerMethod)
        {
            OGRGeometry *y_geom = y->GetGeometryRef();
            if (!y_geom)
                continue;
            OGRGeometryUniquePtr z_geom;

            if (x_prepared_geom)
            {
                CPLErrorReset();
                ret = OGRERR_NONE;
                if (bPretestContainment &&
                    OGRPreparedGeometryContains(x_prepared_geom.get(),
                                                OGRGeometry::ToHandle(y_geom)))
                {
                    if (CPLGetLastErrorType() == CE_None)
                        z_geom.reset(y_geom->clone());
                }
                else if (!(OGRPreparedGeometryIntersects(
                             x_prepared_geom.get(),
                             OGRGeometry::ToHandle(y_geom))))
                {
                    if (CPLGetLastErrorType() == CE_None)
                    {
                        continue;
                    }
                }
                if (CPLGetLastErrorType() != CE_None)
                {
                    if (!bSkipFailures)
                    {
                        ret = OGRERR_FAILURE;
                        goto done;
                    }
                    else
                    {
                        CPLErrorReset();
                        ret = OGRERR_NONE;
                        continue;
                    }
                }
            }
            if (!z_geom)
            {
                CPLErrorReset();
                z_geom.reset(x_geom->Intersection(y_geom));
                if (CPLGetLastErrorType() != CE_None || z_geom == nullptr)
                {
                    if (!bSkipFailures)
                    {
                        ret = OGRERR_FAILURE;
                        goto done;
                    }
                    else
                    {
                        CPLErrorReset();
                        ret = OGRERR_NONE;
                        continue;
                    }
                }
                if (z_geom->IsEmpty() ||
                    (!bKeepLowerDimGeom &&
                     (x_geom->getDimension() == y_geom->getDimension() &&
                      z_geom->getDimension() < x_geom->getDimension())))
                {
                    continue;
                }
            }
            OGRFeatureUniquePtr z(new OGRFeature(poDefnResult));
            z->SetFieldsFrom(x.get(), mapInput);
            z->SetFieldsFrom(y.get(), mapMethod);
            if (bPromoteToMulti)
                z_geom.reset(promote_to_multi(z_geom.release()));
            z->SetGeometryDirectly(z_geom.release());
            ret = pLayerResult->CreateFeature(z.get());

            if (ret != OGRERR_NONE)
            {
                if (!bSkipFailures)
                {
                    goto done;
                }
                else
                {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                }
            }
        }
    }
    if (pfnProgress && !pfnProgress(1.0, "", pProgressArg))
    {
        CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
        ret = OGRERR_FAILURE;
        goto done;
    }
done:
    // release resources
    pLayerMethod->SetSpatialFilter(pGeometryMethodFilter);
    if (pGeometryMethodFilter)
        delete pGeometryMethodFilter;
    if (mapInput)
        VSIFree(mapInput);
    if (mapMethod)
        VSIFree(mapMethod);
    return ret;
}

/************************************************************************/
/*                       OGR_L_Intersection()                           */
/************************************************************************/
/**
 * \brief Intersection of two layers.
 *
 * The result layer contains features whose geometries represent areas
 * that are common between features in the input layer and in the
 * method layer. The features in the result layer have attributes from
 * both input and method layers. The schema of the result layer can be
 * set by the user or, if it is empty, is initialized to contain all
 * fields in the input and method layers.
 *
 * \note If the schema of the result is set by user and contains
 * fields that have the same name as a field in input and in method
 * layer, then the attribute in the result feature will get the value
 * from the feature of the method layer.
 *
 * \note For best performance use the minimum amount of features in
 * the method layer and copy it into a memory layer.
 *
 * \note This method relies on GEOS support. Do not use unless the
 * GEOS support is compiled in.
 *
 * The recognized list of options is :
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a
 *     feature could not be inserted or a GEOS call failed.
 * </li>
 * <li>PROMOTE_TO_MULTI=YES/NO. Set to YES to convert Polygons
 *     into MultiPolygons, LineStrings to MultiLineStrings or
 *     Points to MultiPoints (only since GDAL 3.9.2 for the later)
 * </li>
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * </li>
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * </li>
 * <li>USE_PREPARED_GEOMETRIES=YES/NO. Set to NO to not use prepared
 *     geometries to pretest intersection of features of method layer
 *     with features of this layer.
 * </li>
 * <li>PRETEST_CONTAINMENT=YES/NO. Set to YES to pretest the
 *     containment of features of method layer within the features of
 *     this layer. This will speed up the method significantly in some
 *     cases. Requires that the prepared geometries are in effect.
 * </li>
 * <li>KEEP_LOWER_DIMENSION_GEOMETRIES=YES/NO. Set to NO to skip
 *     result features with lower dimension geometry that would
 *     otherwise be added to the result layer. The default is YES, to add
 *     features with lower dimension geometry, but only if the result layer
 *     has an unknown geometry type.
 * </li>
 * </ul>
 *
 * This function is the same as the C++ method OGRLayer::Intersection().
 *
 * @param pLayerInput the input layer. Should not be NULL.
 *
 * @param pLayerMethod the method layer. Should not be NULL.
 *
 * @param pLayerResult the layer where the features resulting from the
 * operation are inserted. Should not be NULL. See above the note
 * about the schema.
 *
 * @param papszOptions NULL terminated list of options (may be NULL).
 *
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 *
 * @param pProgressArg argument to be passed to pfnProgress. May be NULL.
 *
 * @return an error code if there was an error or the execution was
 * interrupted, OGRERR_NONE otherwise.
 *
 * @note The first geometry field is always used.
 *
 * @since OGR 1.10
 */

OGRErr OGR_L_Intersection(OGRLayerH pLayerInput, OGRLayerH pLayerMethod,
                          OGRLayerH pLayerResult, char **papszOptions,
                          GDALProgressFunc pfnProgress, void *pProgressArg)

{
    VALIDATE_POINTER1(pLayerInput, "OGR_L_Intersection", OGRERR_INVALID_HANDLE);
    VALIDATE_POINTER1(pLayerMethod, "OGR_L_Intersection",
                      OGRERR_INVALID_HANDLE);
    VALIDATE_POINTER1(pLayerResult, "OGR_L_Intersection",
                      OGRERR_INVALID_HANDLE);

    return OGRLayer::FromHandle(pLayerInput)
        ->Intersection(OGRLayer::FromHandle(pLayerMethod),
                       OGRLayer::FromHandle(pLayerResult), papszOptions,
                       pfnProgress, pProgressArg);
}

/************************************************************************/
/*                              Union()                                 */
/************************************************************************/

/**
 * \brief Union of two layers.
 *
 * The result layer contains features whose geometries represent areas
 * that are either in the input layer, in the method layer, or in
 * both. The features in the result layer have attributes from both
 * input and method layers. For features which represent areas that
 * are only in the input or in the method layer the respective
 * attributes have undefined values. The schema of the result layer
 * can be set by the user or, if it is empty, is initialized to
 * contain all fields in the input and method layers.
 *
 * \note If the schema of the result is set by user and contains
 * fields that have the same name as a field in input and in method
 * layer, then the attribute in the result feature will get the value
 * from the feature of the method layer (even if it is undefined).
 *
 * \note For best performance use the minimum amount of features in
 * the method layer and copy it into a memory layer.
 *
 * \note This method relies on GEOS support. Do not use unless the
 * GEOS support is compiled in.
 *
 * The recognized list of options is :
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a
 *     feature could not be inserted or a GEOS call failed.
 * </li>
 * <li>PROMOTE_TO_MULTI=YES/NO. Set to YES to convert Polygons
 *     into MultiPolygons, LineStrings to MultiLineStrings or
 *     Points to MultiPoints (only since GDAL 3.9.2 for the later)
 * </li>
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * </li>
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * </li>
 * <li>USE_PREPARED_GEOMETRIES=YES/NO. Set to NO to not use prepared
 *     geometries to pretest intersection of features of method layer
 *     with features of this layer.
 * </li>
 * <li>KEEP_LOWER_DIMENSION_GEOMETRIES=YES/NO. Set to NO to skip
 *     result features with lower dimension geometry that would
 *     otherwise be added to the result layer. The default is YES, to add
 *     features with lower dimension geometry, but only if the result layer
 *     has an unknown geometry type.
 * </li>
 * </ul>
 *
 * This method is the same as the C function OGR_L_Union().
 *
 * @param pLayerMethod the method layer. Should not be NULL.
 *
 * @param pLayerResult the layer where the features resulting from the
 * operation are inserted. Should not be NULL. See above the note
 * about the schema.
 *
 * @param papszOptions NULL terminated list of options (may be NULL).
 *
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 *
 * @param pProgressArg argument to be passed to pfnProgress. May be NULL.
 *
 * @return an error code if there was an error or the execution was
 * interrupted, OGRERR_NONE otherwise.
 *
 * @note The first geometry field is always used.
 *
 * @since OGR 1.10
 */

OGRErr OGRLayer::Union(OGRLayer *pLayerMethod, OGRLayer *pLayerResult,
                       char **papszOptions, GDALProgressFunc pfnProgress,
                       void *pProgressArg)
{
    OGRErr ret = OGRERR_NONE;
    OGRFeatureDefn *poDefnInput = GetLayerDefn();
    OGRFeatureDefn *poDefnMethod = pLayerMethod->GetLayerDefn();
    OGRFeatureDefn *poDefnResult = nullptr;
    OGRGeometry *pGeometryMethodFilter = nullptr;
    OGRGeometry *pGeometryInputFilter = nullptr;
    int *mapInput = nullptr;
    int *mapMethod = nullptr;
    double progress_max =
        static_cast<double>(GetFeatureCount(FALSE)) +
        static_cast<double>(pLayerMethod->GetFeatureCount(FALSE));
    double progress_counter = 0;
    double progress_ticker = 0;
    const bool bSkipFailures =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "SKIP_FAILURES", "NO"));
    const bool bPromoteToMulti = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "PROMOTE_TO_MULTI", "NO"));
    const bool bUsePreparedGeometries = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "USE_PREPARED_GEOMETRIES", "YES"));
    bool bKeepLowerDimGeom = CPLTestBool(CSLFetchNameValueDef(
        papszOptions, "KEEP_LOWER_DIMENSION_GEOMETRIES", "YES"));

    // check for GEOS
    if (!OGRGeometryFactory::haveGEOS())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "OGRLayer::Union() requires GEOS support");
        return OGRERR_UNSUPPORTED_OPERATION;
    }

    // get resources
    ret = clone_spatial_filter(this, &pGeometryInputFilter);
    if (ret != OGRERR_NONE)
        goto done;
    ret = clone_spatial_filter(pLayerMethod, &pGeometryMethodFilter);
    if (ret != OGRERR_NONE)
        goto done;
    ret = create_field_map(poDefnInput, &mapInput);
    if (ret != OGRERR_NONE)
        goto done;
    ret = create_field_map(poDefnMethod, &mapMethod);
    if (ret != OGRERR_NONE)
        goto done;
    ret = set_result_schema(pLayerResult, poDefnInput, poDefnMethod, mapInput,
                            mapMethod, true, papszOptions);
    if (ret != OGRERR_NONE)
        goto done;
    poDefnResult = pLayerResult->GetLayerDefn();
    if (bKeepLowerDimGeom)
    {
        // require that the result layer is of geom type unknown
        if (pLayerResult->GetGeomType() != wkbUnknown)
        {
            CPLDebug("OGR", "Resetting KEEP_LOWER_DIMENSION_GEOMETRIES to NO "
                            "since the result layer does not allow it.");
            bKeepLowerDimGeom = FALSE;
        }
    }

    // add features based on input layer
    for (auto &&x : this)
    {

        if (pfnProgress)
        {
            double p = progress_counter / progress_max;
            if (p > progress_ticker)
            {
                if (!pfnProgress(p, "", pProgressArg))
                {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        // set up the filter on method layer
        CPLErrorReset();
        OGRGeometry *x_geom =
            set_filter_from(pLayerMethod, pGeometryMethodFilter, x.get());
        if (CPLGetLastErrorType() != CE_None)
        {
            if (!bSkipFailures)
            {
                ret = OGRERR_FAILURE;
                goto done;
            }
            else
            {
                CPLErrorReset();
                ret = OGRERR_NONE;
            }
        }
        if (!x_geom)
        {
            continue;
        }

        OGRPreparedGeometryUniquePtr x_prepared_geom;
        if (bUsePreparedGeometries)
        {
            x_prepared_geom.reset(
                OGRCreatePreparedGeometry(OGRGeometry::ToHandle(x_geom)));
            if (!x_prepared_geom)
            {
                goto done;
            }
        }

        OGRGeometryUniquePtr x_geom_diff(
            x_geom
                ->clone());  // this will be the geometry of the result feature
        for (auto &&y : pLayerMethod)
        {
            OGRGeometry *y_geom = y->GetGeometryRef();
            if (!y_geom)
            {
                continue;
            }

            CPLErrorReset();
            if (x_prepared_geom &&
                !(OGRPreparedGeometryIntersects(x_prepared_geom.get(),
                                                OGRGeometry::ToHandle(y_geom))))
            {
                if (CPLGetLastErrorType() == CE_None)
                {
                    continue;
                }
            }
            if (CPLGetLastErrorType() != CE_None)
            {
                if (!bSkipFailures)
                {
                    ret = OGRERR_FAILURE;
                    goto done;
                }
                else
                {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                }
            }

            CPLErrorReset();
            OGRGeometryUniquePtr poIntersection(x_geom->Intersection(y_geom));
            if (CPLGetLastErrorType() != CE_None || poIntersection == nullptr)
            {
                if (!bSkipFailures)
                {
                    ret = OGRERR_FAILURE;
                    goto done;
                }
                else
                {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                    continue;
                }
            }
            if (poIntersection->IsEmpty() ||
                (!bKeepLowerDimGeom &&
                 (x_geom->getDimension() == y_geom->getDimension() &&
                  poIntersection->getDimension() < x_geom->getDimension())))
            {
                // ok
            }
            else
            {
                OGRFeatureUniquePtr z(new OGRFeature(poDefnResult));
                z->SetFieldsFrom(x.get(), mapInput);
                z->SetFieldsFrom(y.get(), mapMethod);
                if (bPromoteToMulti)
                    poIntersection.reset(
                        promote_to_multi(poIntersection.release()));
                z->SetGeometryDirectly(poIntersection.release());

                if (x_geom_diff)
                {
                    CPLErrorReset();
                    OGRGeometryUniquePtr x_geom_diff_new(
                        x_geom_diff->Difference(y_geom));
                    if (CPLGetLastErrorType() != CE_None ||
                        x_geom_diff_new == nullptr)
                    {
                        if (!bSkipFailures)
                        {
                            ret = OGRERR_FAILURE;
                            goto done;
                        }
                        else
                        {
                            CPLErrorReset();
                        }
                    }
                    else
                    {
                        x_geom_diff.swap(x_geom_diff_new);
                    }
                }

                ret = pLayerResult->CreateFeature(z.get());
                if (ret != OGRERR_NONE)
                {
                    if (!bSkipFailures)
                    {
                        goto done;
                    }
                    else
                    {
                        CPLErrorReset();
                        ret = OGRERR_NONE;
                    }
                }
            }
        }
        x_prepared_geom.reset();

        if (x_geom_diff == nullptr || x_geom_diff->IsEmpty())
        {
            // ok
        }
        else
        {
            OGRFeatureUniquePtr z(new OGRFeature(poDefnResult));
            z->SetFieldsFrom(x.get(), mapInput);
            if (bPromoteToMulti)
                x_geom_diff.reset(promote_to_multi(x_geom_diff.release()));
            z->SetGeometryDirectly(x_geom_diff.release());
            ret = pLayerResult->CreateFeature(z.get());
            if (ret != OGRERR_NONE)
            {
                if (!bSkipFailures)
                {
                    goto done;
                }
                else
                {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                }
            }
        }
    }

    // restore filter on method layer and add features based on it
    pLayerMethod->SetSpatialFilter(pGeometryMethodFilter);
    for (auto &&x : pLayerMethod)
    {

        if (pfnProgress)
        {
            double p = progress_counter / progress_max;
            if (p > progress_ticker)
            {
                if (!pfnProgress(p, "", pProgressArg))
                {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        // set up the filter on input layer
        CPLErrorReset();
        OGRGeometry *x_geom =
            set_filter_from(this, pGeometryInputFilter, x.get());
        if (CPLGetLastErrorType() != CE_None)
        {
            if (!bSkipFailures)
            {
                ret = OGRERR_FAILURE;
                goto done;
            }
            else
            {
                CPLErrorReset();
                ret = OGRERR_NONE;
            }
        }
        if (!x_geom)
        {
            continue;
        }

        OGRGeometryUniquePtr x_geom_diff(
            x_geom
                ->clone());  // this will be the geometry of the result feature
        for (auto &&y : this)
        {
            OGRGeometry *y_geom = y->GetGeometryRef();
            if (!y_geom)
            {
                continue;
            }

            if (x_geom_diff)
            {
                CPLErrorReset();
                OGRGeometryUniquePtr x_geom_diff_new(
                    x_geom_diff->Difference(y_geom));
                if (CPLGetLastErrorType() != CE_None ||
                    x_geom_diff_new == nullptr)
                {
                    if (!bSkipFailures)
                    {
                        ret = OGRERR_FAILURE;
                        goto done;
                    }
                    else
                    {
                        CPLErrorReset();
                        ret = OGRERR_NONE;
                    }
                }
                else
                {
                    x_geom_diff.swap(x_geom_diff_new);
                }
            }
        }

        if (x_geom_diff == nullptr || x_geom_diff->IsEmpty())
        {
            // ok
        }
        else
        {
            OGRFeatureUniquePtr z(new OGRFeature(poDefnResult));
            z->SetFieldsFrom(x.get(), mapMethod);
            if (bPromoteToMulti)
                x_geom_diff.reset(promote_to_multi(x_geom_diff.release()));
            z->SetGeometryDirectly(x_geom_diff.release());
            ret = pLayerResult->CreateFeature(z.get());
            if (ret != OGRERR_NONE)
            {
                if (!bSkipFailures)
                {
                    goto done;
                }
                else
                {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                }
            }
        }
    }
    if (pfnProgress && !pfnProgress(1.0, "", pProgressArg))
    {
        CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
        ret = OGRERR_FAILURE;
        goto done;
    }
done:
    // release resources
    SetSpatialFilter(pGeometryInputFilter);
    pLayerMethod->SetSpatialFilter(pGeometryMethodFilter);
    if (pGeometryMethodFilter)
        delete pGeometryMethodFilter;
    if (pGeometryInputFilter)
        delete pGeometryInputFilter;
    if (mapInput)
        VSIFree(mapInput);
    if (mapMethod)
        VSIFree(mapMethod);
    return ret;
}

/************************************************************************/
/*                           OGR_L_Union()                              */
/************************************************************************/

/**
 * \brief Union of two layers.
 *
 * The result layer contains features whose geometries represent areas
 * that are in either in the input layer, in the method layer, or in
 * both. The features in the result layer have attributes from both
 * input and method layers. For features which represent areas that
 * are only in the input or in the method layer the respective
 * attributes have undefined values. The schema of the result layer
 * can be set by the user or, if it is empty, is initialized to
 * contain all fields in the input and method layers.
 *
 * \note If the schema of the result is set by user and contains
 * fields that have the same name as a field in input and in method
 * layer, then the attribute in the result feature will get the value
 * from the feature of the method layer (even if it is undefined).
 *
 * \note For best performance use the minimum amount of features in
 * the method layer and copy it into a memory layer.
 *
 * \note This method relies on GEOS support. Do not use unless the
 * GEOS support is compiled in.
 *
 * The recognized list of options is :
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a
 *     feature could not be inserted or a GEOS call failed.
 * </li>
 * <li>PROMOTE_TO_MULTI=YES/NO. Set to YES to convert Polygons
 *     into MultiPolygons, LineStrings to MultiLineStrings or
 *     Points to MultiPoints (only since GDAL 3.9.2 for the later)
 * </li>
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * </li>
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * </li>
 * <li>USE_PREPARED_GEOMETRIES=YES/NO. Set to NO to not use prepared
 *     geometries to pretest intersection of features of method layer
 *     with features of this layer.
 * </li>
 * <li>KEEP_LOWER_DIMENSION_GEOMETRIES=YES/NO. Set to NO to skip
 *     result features with lower dimension geometry that would
 *     otherwise be added to the result layer. The default is YES, to add
 *     features with lower dimension geometry, but only if the result layer
 *     has an unknown geometry type.
 * </li>
 * </ul>
 *
 * This function is the same as the C++ method OGRLayer::Union().
 *
 * @param pLayerInput the input layer. Should not be NULL.
 *
 * @param pLayerMethod the method layer. Should not be NULL.
 *
 * @param pLayerResult the layer where the features resulting from the
 * operation are inserted. Should not be NULL. See above the note
 * about the schema.
 *
 * @param papszOptions NULL terminated list of options (may be NULL).
 *
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 *
 * @param pProgressArg argument to be passed to pfnProgress. May be NULL.
 *
 * @return an error code if there was an error or the execution was
 * interrupted, OGRERR_NONE otherwise.
 *
 * @note The first geometry field is always used.
 *
 * @since OGR 1.10
 */

OGRErr OGR_L_Union(OGRLayerH pLayerInput, OGRLayerH pLayerMethod,
                   OGRLayerH pLayerResult, char **papszOptions,
                   GDALProgressFunc pfnProgress, void *pProgressArg)

{
    VALIDATE_POINTER1(pLayerInput, "OGR_L_Union", OGRERR_INVALID_HANDLE);
    VALIDATE_POINTER1(pLayerMethod, "OGR_L_Union", OGRERR_INVALID_HANDLE);
    VALIDATE_POINTER1(pLayerResult, "OGR_L_Union", OGRERR_INVALID_HANDLE);

    return OGRLayer::FromHandle(pLayerInput)
        ->Union(OGRLayer::FromHandle(pLayerMethod),
                OGRLayer::FromHandle(pLayerResult), papszOptions, pfnProgress,
                pProgressArg);
}

/************************************************************************/
/*                          SymDifference()                             */
/************************************************************************/

/**
 * \brief Symmetrical difference of two layers.
 *
 * The result layer contains features whose geometries represent areas
 * that are in either in the input layer or in the method layer but
 * not in both. The features in the result layer have attributes from
 * both input and method layers. For features which represent areas
 * that are only in the input or in the method layer the respective
 * attributes have undefined values. The schema of the result layer
 * can be set by the user or, if it is empty, is initialized to
 * contain all fields in the input and method layers.
 *
 * \note If the schema of the result is set by user and contains
 * fields that have the same name as a field in input and in method
 * layer, then the attribute in the result feature will get the value
 * from the feature of the method layer (even if it is undefined).
 *
 * \note For best performance use the minimum amount of features in
 * the method layer and copy it into a memory layer.
 *
 * \note This method relies on GEOS support. Do not use unless the
 * GEOS support is compiled in.
 *
 * The recognized list of options is :
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a
 *     feature could not be inserted or a GEOS call failed.
 * </li>
 * <li>PROMOTE_TO_MULTI=YES/NO. Set it to YES to convert Polygons
 *     into MultiPolygons, or LineStrings to MultiLineStrings.
 * </li>
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * </li>
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * </li>
 * </ul>
 *
 * This method is the same as the C function OGR_L_SymDifference().
 *
 * @param pLayerMethod the method layer. Should not be NULL.
 *
 * @param pLayerResult the layer where the features resulting from the
 * operation are inserted. Should not be NULL. See above the note
 * about the schema.
 *
 * @param papszOptions NULL terminated list of options (may be NULL).
 *
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 *
 * @param pProgressArg argument to be passed to pfnProgress. May be NULL.
 *
 * @return an error code if there was an error or the execution was
 * interrupted, OGRERR_NONE otherwise.
 *
 * @note The first geometry field is always used.
 *
 * @since OGR 1.10
 */

OGRErr OGRLayer::SymDifference(OGRLayer *pLayerMethod, OGRLayer *pLayerResult,
                               char **papszOptions,
                               GDALProgressFunc pfnProgress, void *pProgressArg)
{
    OGRErr ret = OGRERR_NONE;
    OGRFeatureDefn *poDefnInput = GetLayerDefn();
    OGRFeatureDefn *poDefnMethod = pLayerMethod->GetLayerDefn();
    OGRFeatureDefn *poDefnResult = nullptr;
    OGRGeometry *pGeometryMethodFilter = nullptr;
    OGRGeometry *pGeometryInputFilter = nullptr;
    int *mapInput = nullptr;
    int *mapMethod = nullptr;
    double progress_max =
        static_cast<double>(GetFeatureCount(FALSE)) +
        static_cast<double>(pLayerMethod->GetFeatureCount(FALSE));
    double progress_counter = 0;
    double progress_ticker = 0;
    const bool bSkipFailures =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "SKIP_FAILURES", "NO"));
    const bool bPromoteToMulti = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "PROMOTE_TO_MULTI", "NO"));

    // check for GEOS
    if (!OGRGeometryFactory::haveGEOS())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "OGRLayer::SymDifference() requires GEOS support");
        return OGRERR_UNSUPPORTED_OPERATION;
    }

    // get resources
    ret = clone_spatial_filter(this, &pGeometryInputFilter);
    if (ret != OGRERR_NONE)
        goto done;
    ret = clone_spatial_filter(pLayerMethod, &pGeometryMethodFilter);
    if (ret != OGRERR_NONE)
        goto done;
    ret = create_field_map(poDefnInput, &mapInput);
    if (ret != OGRERR_NONE)
        goto done;
    ret = create_field_map(poDefnMethod, &mapMethod);
    if (ret != OGRERR_NONE)
        goto done;
    ret = set_result_schema(pLayerResult, poDefnInput, poDefnMethod, mapInput,
                            mapMethod, true, papszOptions);
    if (ret != OGRERR_NONE)
        goto done;
    poDefnResult = pLayerResult->GetLayerDefn();

    // add features based on input layer
    for (auto &&x : this)
    {

        if (pfnProgress)
        {
            double p = progress_counter / progress_max;
            if (p > progress_ticker)
            {
                if (!pfnProgress(p, "", pProgressArg))
                {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        // set up the filter on method layer
        CPLErrorReset();
        OGRGeometry *x_geom =
            set_filter_from(pLayerMethod, pGeometryMethodFilter, x.get());
        if (CPLGetLastErrorType() != CE_None)
        {
            if (!bSkipFailures)
            {
                ret = OGRERR_FAILURE;
                goto done;
            }
            else
            {
                CPLErrorReset();
                ret = OGRERR_NONE;
            }
        }
        if (!x_geom)
        {
            continue;
        }

        OGRGeometryUniquePtr geom(
            x_geom
                ->clone());  // this will be the geometry of the result feature
        for (auto &&y : pLayerMethod)
        {
            OGRGeometry *y_geom = y->GetGeometryRef();
            if (!y_geom)
            {
                continue;
            }
            if (geom)
            {
                CPLErrorReset();
                OGRGeometryUniquePtr geom_new(geom->Difference(y_geom));
                if (CPLGetLastErrorType() != CE_None || geom_new == nullptr)
                {
                    if (!bSkipFailures)
                    {
                        ret = OGRERR_FAILURE;
                        goto done;
                    }
                    else
                    {
                        CPLErrorReset();
                        ret = OGRERR_NONE;
                    }
                }
                else
                {
                    geom.swap(geom_new);
                }
            }
            if (geom && geom->IsEmpty())
                break;
        }

        if (geom && !geom->IsEmpty())
        {
            OGRFeatureUniquePtr z(new OGRFeature(poDefnResult));
            z->SetFieldsFrom(x.get(), mapInput);
            if (bPromoteToMulti)
                geom.reset(promote_to_multi(geom.release()));
            z->SetGeometryDirectly(geom.release());
            ret = pLayerResult->CreateFeature(z.get());
            if (ret != OGRERR_NONE)
            {
                if (!bSkipFailures)
                {
                    goto done;
                }
                else
                {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                }
            }
        }
    }

    // restore filter on method layer and add features based on it
    pLayerMethod->SetSpatialFilter(pGeometryMethodFilter);
    for (auto &&x : pLayerMethod)
    {

        if (pfnProgress)
        {
            double p = progress_counter / progress_max;
            if (p > progress_ticker)
            {
                if (!pfnProgress(p, "", pProgressArg))
                {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        // set up the filter on input layer
        CPLErrorReset();
        OGRGeometry *x_geom =
            set_filter_from(this, pGeometryInputFilter, x.get());
        if (CPLGetLastErrorType() != CE_None)
        {
            if (!bSkipFailures)
            {
                ret = OGRERR_FAILURE;
                goto done;
            }
            else
            {
                CPLErrorReset();
                ret = OGRERR_NONE;
            }
        }
        if (!x_geom)
        {
            continue;
        }

        OGRGeometryUniquePtr geom(
            x_geom
                ->clone());  // this will be the geometry of the result feature
        for (auto &&y : this)
        {
            OGRGeometry *y_geom = y->GetGeometryRef();
            if (!y_geom)
                continue;
            if (geom)
            {
                CPLErrorReset();
                OGRGeometryUniquePtr geom_new(geom->Difference(y_geom));
                if (CPLGetLastErrorType() != CE_None || geom_new == nullptr)
                {
                    if (!bSkipFailures)
                    {
                        ret = OGRERR_FAILURE;
                        goto done;
                    }
                    else
                    {
                        CPLErrorReset();
                        ret = OGRERR_NONE;
                    }
                }
                else
                {
                    geom.swap(geom_new);
                }
            }
            if (geom == nullptr || geom->IsEmpty())
                break;
        }

        if (geom && !geom->IsEmpty())
        {
            OGRFeatureUniquePtr z(new OGRFeature(poDefnResult));
            z->SetFieldsFrom(x.get(), mapMethod);
            if (bPromoteToMulti)
                geom.reset(promote_to_multi(geom.release()));
            z->SetGeometryDirectly(geom.release());
            ret = pLayerResult->CreateFeature(z.get());
            if (ret != OGRERR_NONE)
            {
                if (!bSkipFailures)
                {
                    goto done;
                }
                else
                {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                }
            }
        }
    }
    if (pfnProgress && !pfnProgress(1.0, "", pProgressArg))
    {
        CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
        ret = OGRERR_FAILURE;
        goto done;
    }
done:
    // release resources
    SetSpatialFilter(pGeometryInputFilter);
    pLayerMethod->SetSpatialFilter(pGeometryMethodFilter);
    if (pGeometryMethodFilter)
        delete pGeometryMethodFilter;
    if (pGeometryInputFilter)
        delete pGeometryInputFilter;
    if (mapInput)
        VSIFree(mapInput);
    if (mapMethod)
        VSIFree(mapMethod);
    return ret;
}

/************************************************************************/
/*                        OGR_L_SymDifference()                         */
/************************************************************************/

/**
 * \brief Symmetrical difference of two layers.
 *
 * The result layer contains features whose geometries represent areas
 * that are in either in the input layer or in the method layer but
 * not in both. The features in the result layer have attributes from
 * both input and method layers. For features which represent areas
 * that are only in the input or in the method layer the respective
 * attributes have undefined values. The schema of the result layer
 * can be set by the user or, if it is empty, is initialized to
 * contain all fields in the input and method layers.
 *
 * \note If the schema of the result is set by user and contains
 * fields that have the same name as a field in input and in method
 * layer, then the attribute in the result feature will get the value
 * from the feature of the method layer (even if it is undefined).
 *
 * \note For best performance use the minimum amount of features in
 * the method layer and copy it into a memory layer.
 *
 * \note This method relies on GEOS support. Do not use unless the
 * GEOS support is compiled in.
 *
 * The recognized list of options is :
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a
 *     feature could not be inserted or a GEOS call failed.
 * </li>
 * <li>PROMOTE_TO_MULTI=YES/NO. Set to YES to convert Polygons
 *     into MultiPolygons, LineStrings to MultiLineStrings or
 *     Points to MultiPoints (only since GDAL 3.9.2 for the later)
 * </li>
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * </li>
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * </li>
 * </ul>
 *
 * This function is the same as the C++ method OGRLayer::SymDifference().
 *
 * @param pLayerInput the input layer. Should not be NULL.
 *
 * @param pLayerMethod the method layer. Should not be NULL.
 *
 * @param pLayerResult the layer where the features resulting from the
 * operation are inserted. Should not be NULL. See above the note
 * about the schema.
 *
 * @param papszOptions NULL terminated list of options (may be NULL).
 *
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 *
 * @param pProgressArg argument to be passed to pfnProgress. May be NULL.
 *
 * @return an error code if there was an error or the execution was
 * interrupted, OGRERR_NONE otherwise.
 *
 * @note The first geometry field is always used.
 *
 * @since OGR 1.10
 */

OGRErr OGR_L_SymDifference(OGRLayerH pLayerInput, OGRLayerH pLayerMethod,
                           OGRLayerH pLayerResult, char **papszOptions,
                           GDALProgressFunc pfnProgress, void *pProgressArg)

{
    VALIDATE_POINTER1(pLayerInput, "OGR_L_SymDifference",
                      OGRERR_INVALID_HANDLE);
    VALIDATE_POINTER1(pLayerMethod, "OGR_L_SymDifference",
                      OGRERR_INVALID_HANDLE);
    VALIDATE_POINTER1(pLayerResult, "OGR_L_SymDifference",
                      OGRERR_INVALID_HANDLE);

    return OGRLayer::FromHandle(pLayerInput)
        ->SymDifference(OGRLayer::FromHandle(pLayerMethod),
                        OGRLayer::FromHandle(pLayerResult), papszOptions,
                        pfnProgress, pProgressArg);
}

/************************************************************************/
/*                            Identity()                                */
/************************************************************************/

/**
 * \brief Identify the features of this layer with the ones from the
 * identity layer.
 *
 * The result layer contains features whose geometries represent areas
 * that are in the input layer. The features in the result layer have
 * attributes from both input and method layers. The schema of the
 * result layer can be set by the user or, if it is empty, is
 * initialized to contain all fields in input and method layers.
 *
 * \note If the schema of the result is set by user and contains
 * fields that have the same name as a field in input and in method
 * layer, then the attribute in the result feature will get the value
 * from the feature of the method layer (even if it is undefined).
 *
 * \note For best performance use the minimum amount of features in
 * the method layer and copy it into a memory layer.
 *
 * \note This method relies on GEOS support. Do not use unless the
 * GEOS support is compiled in.
 *
 * The recognized list of options is :
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a
 *     feature could not be inserted or a GEOS call failed.
 * </li>
 * <li>PROMOTE_TO_MULTI=YES/NO. Set to YES to convert Polygons
 *     into MultiPolygons, LineStrings to MultiLineStrings or
 *     Points to MultiPoints (only since GDAL 3.9.2 for the later)
 * </li>
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * </li>
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * </li>
 * <li>USE_PREPARED_GEOMETRIES=YES/NO. Set to NO to not use prepared
 *     geometries to pretest intersection of features of method layer
 *     with features of this layer.
 * </li>
 * <li>KEEP_LOWER_DIMENSION_GEOMETRIES=YES/NO. Set to NO to skip
 *     result features with lower dimension geometry that would
 *     otherwise be added to the result layer. The default is YES, to add
 *     features with lower dimension geometry, but only if the result layer
 *     has an unknown geometry type.
 * </li>
 * </ul>
 *
 * This method is the same as the C function OGR_L_Identity().
 *
 * @param pLayerMethod the method layer. Should not be NULL.
 *
 * @param pLayerResult the layer where the features resulting from the
 * operation are inserted. Should not be NULL. See above the note
 * about the schema.
 *
 * @param papszOptions NULL terminated list of options (may be NULL).
 *
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 *
 * @param pProgressArg argument to be passed to pfnProgress. May be NULL.
 *
 * @return an error code if there was an error or the execution was
 * interrupted, OGRERR_NONE otherwise.
 *
 * @note The first geometry field is always used.
 *
 * @since OGR 1.10
 */

OGRErr OGRLayer::Identity(OGRLayer *pLayerMethod, OGRLayer *pLayerResult,
                          char **papszOptions, GDALProgressFunc pfnProgress,
                          void *pProgressArg)
{
    OGRErr ret = OGRERR_NONE;
    OGRFeatureDefn *poDefnInput = GetLayerDefn();
    OGRFeatureDefn *poDefnMethod = pLayerMethod->GetLayerDefn();
    OGRFeatureDefn *poDefnResult = nullptr;
    OGRGeometry *pGeometryMethodFilter = nullptr;
    int *mapInput = nullptr;
    int *mapMethod = nullptr;
    double progress_max = static_cast<double>(GetFeatureCount(FALSE));
    double progress_counter = 0;
    double progress_ticker = 0;
    const bool bSkipFailures =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "SKIP_FAILURES", "NO"));
    const bool bPromoteToMulti = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "PROMOTE_TO_MULTI", "NO"));
    const bool bUsePreparedGeometries = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "USE_PREPARED_GEOMETRIES", "YES"));
    bool bKeepLowerDimGeom = CPLTestBool(CSLFetchNameValueDef(
        papszOptions, "KEEP_LOWER_DIMENSION_GEOMETRIES", "YES"));

    // check for GEOS
    if (!OGRGeometryFactory::haveGEOS())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "OGRLayer::Identity() requires GEOS support");
        return OGRERR_UNSUPPORTED_OPERATION;
    }
    if (bKeepLowerDimGeom)
    {
        // require that the result layer is of geom type unknown
        if (pLayerResult->GetGeomType() != wkbUnknown)
        {
            CPLDebug("OGR", "Resetting KEEP_LOWER_DIMENSION_GEOMETRIES to NO "
                            "since the result layer does not allow it.");
            bKeepLowerDimGeom = FALSE;
        }
    }

    // get resources
    ret = clone_spatial_filter(pLayerMethod, &pGeometryMethodFilter);
    if (ret != OGRERR_NONE)
        goto done;
    ret = create_field_map(poDefnInput, &mapInput);
    if (ret != OGRERR_NONE)
        goto done;
    ret = create_field_map(poDefnMethod, &mapMethod);
    if (ret != OGRERR_NONE)
        goto done;
    ret = set_result_schema(pLayerResult, poDefnInput, poDefnMethod, mapInput,
                            mapMethod, true, papszOptions);
    if (ret != OGRERR_NONE)
        goto done;
    poDefnResult = pLayerResult->GetLayerDefn();

    // split the features in input layer to the result layer
    for (auto &&x : this)
    {

        if (pfnProgress)
        {
            double p = progress_counter / progress_max;
            if (p > progress_ticker)
            {
                if (!pfnProgress(p, "", pProgressArg))
                {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        // set up the filter on method layer
        CPLErrorReset();
        OGRGeometry *x_geom =
            set_filter_from(pLayerMethod, pGeometryMethodFilter, x.get());
        if (CPLGetLastErrorType() != CE_None)
        {
            if (!bSkipFailures)
            {
                ret = OGRERR_FAILURE;
                goto done;
            }
            else
            {
                CPLErrorReset();
                ret = OGRERR_NONE;
            }
        }
        if (!x_geom)
        {
            continue;
        }

        OGRPreparedGeometryUniquePtr x_prepared_geom;
        if (bUsePreparedGeometries)
        {
            x_prepared_geom.reset(
                OGRCreatePreparedGeometry(OGRGeometry::ToHandle(x_geom)));
            if (!x_prepared_geom)
            {
                goto done;
            }
        }

        OGRGeometryUniquePtr x_geom_diff(
            x_geom
                ->clone());  // this will be the geometry of the result feature
        for (auto &&y : pLayerMethod)
        {
            OGRGeometry *y_geom = y->GetGeometryRef();
            if (!y_geom)
                continue;

            CPLErrorReset();
            if (x_prepared_geom &&
                !(OGRPreparedGeometryIntersects(x_prepared_geom.get(),
                                                OGRGeometry::ToHandle(y_geom))))
            {
                if (CPLGetLastErrorType() == CE_None)
                {
                    continue;
                }
            }
            if (CPLGetLastErrorType() != CE_None)
            {
                if (!bSkipFailures)
                {
                    ret = OGRERR_FAILURE;
                    goto done;
                }
                else
                {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                }
            }

            CPLErrorReset();
            OGRGeometryUniquePtr poIntersection(x_geom->Intersection(y_geom));
            if (CPLGetLastErrorType() != CE_None || poIntersection == nullptr)
            {
                if (!bSkipFailures)
                {
                    ret = OGRERR_FAILURE;
                    goto done;
                }
                else
                {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                }
            }
            else if (poIntersection->IsEmpty() ||
                     (!bKeepLowerDimGeom &&
                      (x_geom->getDimension() == y_geom->getDimension() &&
                       poIntersection->getDimension() <
                           x_geom->getDimension())))
            {
                /* ok*/
            }
            else
            {
                OGRFeatureUniquePtr z(new OGRFeature(poDefnResult));
                z->SetFieldsFrom(x.get(), mapInput);
                z->SetFieldsFrom(y.get(), mapMethod);
                if (bPromoteToMulti)
                    poIntersection.reset(
                        promote_to_multi(poIntersection.release()));
                z->SetGeometryDirectly(poIntersection.release());
                if (x_geom_diff)
                {
                    CPLErrorReset();
                    OGRGeometryUniquePtr x_geom_diff_new(
                        x_geom_diff->Difference(y_geom));
                    if (CPLGetLastErrorType() != CE_None ||
                        x_geom_diff_new == nullptr)
                    {
                        if (!bSkipFailures)
                        {
                            ret = OGRERR_FAILURE;
                            goto done;
                        }
                        else
                        {
                            CPLErrorReset();
                        }
                    }
                    else
                    {
                        x_geom_diff.swap(x_geom_diff_new);
                    }
                }
                ret = pLayerResult->CreateFeature(z.get());
                if (ret != OGRERR_NONE)
                {
                    if (!bSkipFailures)
                    {
                        goto done;
                    }
                    else
                    {
                        CPLErrorReset();
                        ret = OGRERR_NONE;
                    }
                }
            }
        }

        x_prepared_geom.reset();

        if (x_geom_diff == nullptr || x_geom_diff->IsEmpty())
        {
            /* ok */
        }
        else
        {
            OGRFeatureUniquePtr z(new OGRFeature(poDefnResult));
            z->SetFieldsFrom(x.get(), mapInput);
            if (bPromoteToMulti)
                x_geom_diff.reset(promote_to_multi(x_geom_diff.release()));
            z->SetGeometryDirectly(x_geom_diff.release());
            ret = pLayerResult->CreateFeature(z.get());
            if (ret != OGRERR_NONE)
            {
                if (!bSkipFailures)
                {
                    goto done;
                }
                else
                {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                }
            }
        }
    }
    if (pfnProgress && !pfnProgress(1.0, "", pProgressArg))
    {
        CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
        ret = OGRERR_FAILURE;
        goto done;
    }
done:
    // release resources
    pLayerMethod->SetSpatialFilter(pGeometryMethodFilter);
    if (pGeometryMethodFilter)
        delete pGeometryMethodFilter;
    if (mapInput)
        VSIFree(mapInput);
    if (mapMethod)
        VSIFree(mapMethod);
    return ret;
}

/************************************************************************/
/*                         OGR_L_Identity()                             */
/************************************************************************/

/**
 * \brief Identify the features of this layer with the ones from the
 * identity layer.
 *
 * The result layer contains features whose geometries represent areas
 * that are in the input layer. The features in the result layer have
 * attributes from both input and method layers. The schema of the
 * result layer can be set by the user or, if it is empty, is
 * initialized to contain all fields in input and method layers.
 *
 * \note If the schema of the result is set by user and contains
 * fields that have the same name as a field in input and in method
 * layer, then the attribute in the result feature will get the value
 * from the feature of the method layer (even if it is undefined).
 *
 * \note For best performance use the minimum amount of features in
 * the method layer and copy it into a memory layer.
 *
 * \note This method relies on GEOS support. Do not use unless the
 * GEOS support is compiled in.
 *
 * The recognized list of options is :
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a
 *     feature could not be inserted or a GEOS call failed.
 * </li>
 * <li>PROMOTE_TO_MULTI=YES/NO. Set to YES to convert Polygons
 *     into MultiPolygons, LineStrings to MultiLineStrings or
 *     Points to MultiPoints (only since GDAL 3.9.2 for the later)
 * </li>
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * </li>
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * </li>
 * <li>USE_PREPARED_GEOMETRIES=YES/NO. Set to NO to not use prepared
 *     geometries to pretest intersection of features of method layer
 *     with features of this layer.
 * </li>
 * <li>KEEP_LOWER_DIMENSION_GEOMETRIES=YES/NO. Set to NO to skip
 *     result features with lower dimension geometry that would
 *     otherwise be added to the result layer. The default is YES, to add
 *     features with lower dimension geometry, but only if the result layer
 *     has an unknown geometry type.
 * </li>
 * </ul>
 *
 * This function is the same as the C++ method OGRLayer::Identity().
 *
 * @param pLayerInput the input layer. Should not be NULL.
 *
 * @param pLayerMethod the method layer. Should not be NULL.
 *
 * @param pLayerResult the layer where the features resulting from the
 * operation are inserted. Should not be NULL. See above the note
 * about the schema.
 *
 * @param papszOptions NULL terminated list of options (may be NULL).
 *
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 *
 * @param pProgressArg argument to be passed to pfnProgress. May be NULL.
 *
 * @return an error code if there was an error or the execution was
 * interrupted, OGRERR_NONE otherwise.
 *
 * @note The first geometry field is always used.
 *
 * @since OGR 1.10
 */

OGRErr OGR_L_Identity(OGRLayerH pLayerInput, OGRLayerH pLayerMethod,
                      OGRLayerH pLayerResult, char **papszOptions,
                      GDALProgressFunc pfnProgress, void *pProgressArg)

{
    VALIDATE_POINTER1(pLayerInput, "OGR_L_Identity", OGRERR_INVALID_HANDLE);
    VALIDATE_POINTER1(pLayerMethod, "OGR_L_Identity", OGRERR_INVALID_HANDLE);
    VALIDATE_POINTER1(pLayerResult, "OGR_L_Identity", OGRERR_INVALID_HANDLE);

    return OGRLayer::FromHandle(pLayerInput)
        ->Identity(OGRLayer::FromHandle(pLayerMethod),
                   OGRLayer::FromHandle(pLayerResult), papszOptions,
                   pfnProgress, pProgressArg);
}

/************************************************************************/
/*                             Update()                                 */
/************************************************************************/

/**
 * \brief Update this layer with features from the update layer.
 *
 * The result layer contains features whose geometries represent areas
 * that are either in the input layer or in the method layer. The
 * features in the result layer have areas of the features of the
 * method layer or those ares of the features of the input layer that
 * are not covered by the method layer. The features of the result
 * layer get their attributes from the input layer. The schema of the
 * result layer can be set by the user or, if it is empty, is
 * initialized to contain all fields in the input layer.
 *
 * \note If the schema of the result is set by user and contains
 * fields that have the same name as a field in the method layer, then
 * the attribute in the result feature the originates from the method
 * layer will get the value from the feature of the method layer.
 *
 * \note For best performance use the minimum amount of features in
 * the method layer and copy it into a memory layer.
 *
 * \note This method relies on GEOS support. Do not use unless the
 * GEOS support is compiled in.
 *
 * The recognized list of options is :
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a
 *     feature could not be inserted or a GEOS call failed.
 * </li>
 * <li>PROMOTE_TO_MULTI=YES/NO. Set to YES to convert Polygons
 *     into MultiPolygons, LineStrings to MultiLineStrings or
 *     Points to MultiPoints (only since GDAL 3.9.2 for the later)
 * </li>
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * </li>
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * </li>
 * </ul>
 *
 * This method is the same as the C function OGR_L_Update().
 *
 * @param pLayerMethod the method layer. Should not be NULL.
 *
 * @param pLayerResult the layer where the features resulting from the
 * operation are inserted. Should not be NULL. See above the note
 * about the schema.
 *
 * @param papszOptions NULL terminated list of options (may be NULL).
 *
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 *
 * @param pProgressArg argument to be passed to pfnProgress. May be NULL.
 *
 * @return an error code if there was an error or the execution was
 * interrupted, OGRERR_NONE otherwise.
 *
 * @note The first geometry field is always used.
 *
 * @since OGR 1.10
 */

OGRErr OGRLayer::Update(OGRLayer *pLayerMethod, OGRLayer *pLayerResult,
                        char **papszOptions, GDALProgressFunc pfnProgress,
                        void *pProgressArg)
{
    OGRErr ret = OGRERR_NONE;
    OGRFeatureDefn *poDefnInput = GetLayerDefn();
    OGRFeatureDefn *poDefnMethod = pLayerMethod->GetLayerDefn();
    OGRFeatureDefn *poDefnResult = nullptr;
    OGRGeometry *pGeometryMethodFilter = nullptr;
    int *mapInput = nullptr;
    int *mapMethod = nullptr;
    double progress_max =
        static_cast<double>(GetFeatureCount(FALSE)) +
        static_cast<double>(pLayerMethod->GetFeatureCount(FALSE));
    double progress_counter = 0;
    double progress_ticker = 0;
    const bool bSkipFailures =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "SKIP_FAILURES", "NO"));
    const bool bPromoteToMulti = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "PROMOTE_TO_MULTI", "NO"));

    // check for GEOS
    if (!OGRGeometryFactory::haveGEOS())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "OGRLayer::Update() requires GEOS support");
        return OGRERR_UNSUPPORTED_OPERATION;
    }

    // get resources
    ret = clone_spatial_filter(pLayerMethod, &pGeometryMethodFilter);
    if (ret != OGRERR_NONE)
        goto done;
    ret = create_field_map(poDefnInput, &mapInput);
    if (ret != OGRERR_NONE)
        goto done;
    ret = create_field_map(poDefnMethod, &mapMethod);
    if (ret != OGRERR_NONE)
        goto done;
    ret = set_result_schema(pLayerResult, poDefnInput, poDefnMethod, mapInput,
                            mapMethod, false, papszOptions);
    if (ret != OGRERR_NONE)
        goto done;
    poDefnResult = pLayerResult->GetLayerDefn();

    // add clipped features from the input layer
    for (auto &&x : this)
    {

        if (pfnProgress)
        {
            double p = progress_counter / progress_max;
            if (p > progress_ticker)
            {
                if (!pfnProgress(p, "", pProgressArg))
                {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        // set up the filter on method layer
        CPLErrorReset();
        OGRGeometry *x_geom =
            set_filter_from(pLayerMethod, pGeometryMethodFilter, x.get());
        if (CPLGetLastErrorType() != CE_None)
        {
            if (!bSkipFailures)
            {
                ret = OGRERR_FAILURE;
                goto done;
            }
            else
            {
                CPLErrorReset();
                ret = OGRERR_NONE;
            }
        }
        if (!x_geom)
        {
            continue;
        }

        OGRGeometryUniquePtr x_geom_diff(
            x_geom->clone());  // this will be the geometry of a result feature
        for (auto &&y : pLayerMethod)
        {
            OGRGeometry *y_geom = y->GetGeometryRef();
            if (!y_geom)
                continue;
            if (x_geom_diff)
            {
                CPLErrorReset();
                OGRGeometryUniquePtr x_geom_diff_new(
                    x_geom_diff->Difference(y_geom));
                if (CPLGetLastErrorType() != CE_None ||
                    x_geom_diff_new == nullptr)
                {
                    if (!bSkipFailures)
                    {
                        ret = OGRERR_FAILURE;
                        goto done;
                    }
                    else
                    {
                        CPLErrorReset();
                        ret = OGRERR_NONE;
                    }
                }
                else
                {
                    x_geom_diff.swap(x_geom_diff_new);
                }
            }
        }

        if (x_geom_diff == nullptr || x_geom_diff->IsEmpty())
        {
            /* ok */
        }
        else
        {
            OGRFeatureUniquePtr z(new OGRFeature(poDefnResult));
            z->SetFieldsFrom(x.get(), mapInput);
            if (bPromoteToMulti)
                x_geom_diff.reset(promote_to_multi(x_geom_diff.release()));
            z->SetGeometryDirectly(x_geom_diff.release());
            ret = pLayerResult->CreateFeature(z.get());
            if (ret != OGRERR_NONE)
            {
                if (!bSkipFailures)
                {
                    goto done;
                }
                else
                {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                }
            }
        }
    }

    // restore the original filter and add features from the update layer
    pLayerMethod->SetSpatialFilter(pGeometryMethodFilter);
    for (auto &&y : pLayerMethod)
    {

        if (pfnProgress)
        {
            double p = progress_counter / progress_max;
            if (p > progress_ticker)
            {
                if (!pfnProgress(p, "", pProgressArg))
                {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        OGRGeometry *y_geom = y->StealGeometry();
        if (!y_geom)
            continue;
        OGRFeatureUniquePtr z(new OGRFeature(poDefnResult));
        if (mapMethod)
            z->SetFieldsFrom(y.get(), mapMethod);
        z->SetGeometryDirectly(y_geom);
        ret = pLayerResult->CreateFeature(z.get());
        if (ret != OGRERR_NONE)
        {
            if (!bSkipFailures)
            {
                goto done;
            }
            else
            {
                CPLErrorReset();
                ret = OGRERR_NONE;
            }
        }
    }
    if (pfnProgress && !pfnProgress(1.0, "", pProgressArg))
    {
        CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
        ret = OGRERR_FAILURE;
        goto done;
    }
done:
    // release resources
    pLayerMethod->SetSpatialFilter(pGeometryMethodFilter);
    if (pGeometryMethodFilter)
        delete pGeometryMethodFilter;
    if (mapInput)
        VSIFree(mapInput);
    if (mapMethod)
        VSIFree(mapMethod);
    return ret;
}

/************************************************************************/
/*                          OGR_L_Update()                              */
/************************************************************************/

/**
 * \brief Update this layer with features from the update layer.
 *
 * The result layer contains features whose geometries represent areas
 * that are either in the input layer or in the method layer. The
 * features in the result layer have areas of the features of the
 * method layer or those ares of the features of the input layer that
 * are not covered by the method layer. The features of the result
 * layer get their attributes from the input layer. The schema of the
 * result layer can be set by the user or, if it is empty, is
 * initialized to contain all fields in the input layer.
 *
 * \note If the schema of the result is set by user and contains
 * fields that have the same name as a field in the method layer, then
 * the attribute in the result feature the originates from the method
 * layer will get the value from the feature of the method layer.
 *
 * \note For best performance use the minimum amount of features in
 * the method layer and copy it into a memory layer.
 *
 * \note This method relies on GEOS support. Do not use unless the
 * GEOS support is compiled in.
 *
 * The recognized list of options is :
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a
 *     feature could not be inserted or a GEOS call failed.
 * </li>
 * <li>PROMOTE_TO_MULTI=YES/NO. Set to YES to convert Polygons
 *     into MultiPolygons, LineStrings to MultiLineStrings or
 *     Points to MultiPoints (only since GDAL 3.9.2 for the later)
 * </li>
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * </li>
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * </li>
 * </ul>
 *
 * This function is the same as the C++ method OGRLayer::Update().
 *
 * @param pLayerInput the input layer. Should not be NULL.
 *
 * @param pLayerMethod the method layer. Should not be NULL.
 *
 * @param pLayerResult the layer where the features resulting from the
 * operation are inserted. Should not be NULL. See above the note
 * about the schema.
 *
 * @param papszOptions NULL terminated list of options (may be NULL).
 *
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 *
 * @param pProgressArg argument to be passed to pfnProgress. May be NULL.
 *
 * @return an error code if there was an error or the execution was
 * interrupted, OGRERR_NONE otherwise.
 *
 * @note The first geometry field is always used.
 *
 * @since OGR 1.10
 */

OGRErr OGR_L_Update(OGRLayerH pLayerInput, OGRLayerH pLayerMethod,
                    OGRLayerH pLayerResult, char **papszOptions,
                    GDALProgressFunc pfnProgress, void *pProgressArg)

{
    VALIDATE_POINTER1(pLayerInput, "OGR_L_Update", OGRERR_INVALID_HANDLE);
    VALIDATE_POINTER1(pLayerMethod, "OGR_L_Update", OGRERR_INVALID_HANDLE);
    VALIDATE_POINTER1(pLayerResult, "OGR_L_Update", OGRERR_INVALID_HANDLE);

    return OGRLayer::FromHandle(pLayerInput)
        ->Update(OGRLayer::FromHandle(pLayerMethod),
                 OGRLayer::FromHandle(pLayerResult), papszOptions, pfnProgress,
                 pProgressArg);
}

/************************************************************************/
/*                              Clip()                                  */
/************************************************************************/

/**
 * \brief Clip off areas that are not covered by the method layer.
 *
 * The result layer contains features whose geometries represent areas
 * that are in the input layer and in the method layer. The features
 * in the result layer have the (possibly clipped) areas of features
 * in the input layer and the attributes from the same features. The
 * schema of the result layer can be set by the user or, if it is
 * empty, is initialized to contain all fields in the input layer.
 *
 * \note For best performance use the minimum amount of features in
 * the method layer and copy it into a memory layer.
 *
 * \note This method relies on GEOS support. Do not use unless the
 * GEOS support is compiled in.
 *
 * The recognized list of options is :
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a
 *     feature could not be inserted or a GEOS call failed.
 * </li>
 * <li>PROMOTE_TO_MULTI=YES/NO. Set to YES to convert Polygons
 *     into MultiPolygons, LineStrings to MultiLineStrings or
 *     Points to MultiPoints (only since GDAL 3.9.2 for the later)
 * </li>
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * </li>
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * </li>
 * </ul>
 *
 * This method is the same as the C function OGR_L_Clip().
 *
 * @param pLayerMethod the method layer. Should not be NULL.
 *
 * @param pLayerResult the layer where the features resulting from the
 * operation are inserted. Should not be NULL. See above the note
 * about the schema.
 *
 * @param papszOptions NULL terminated list of options (may be NULL).
 *
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 *
 * @param pProgressArg argument to be passed to pfnProgress. May be NULL.
 *
 * @return an error code if there was an error or the execution was
 * interrupted, OGRERR_NONE otherwise.
 *
 * @note The first geometry field is always used.
 *
 * @since OGR 1.10
 */

OGRErr OGRLayer::Clip(OGRLayer *pLayerMethod, OGRLayer *pLayerResult,
                      char **papszOptions, GDALProgressFunc pfnProgress,
                      void *pProgressArg)
{
    OGRErr ret = OGRERR_NONE;
    OGRFeatureDefn *poDefnInput = GetLayerDefn();
    OGRFeatureDefn *poDefnResult = nullptr;
    OGRGeometry *pGeometryMethodFilter = nullptr;
    int *mapInput = nullptr;
    double progress_max = static_cast<double>(GetFeatureCount(FALSE));
    double progress_counter = 0;
    double progress_ticker = 0;
    const bool bSkipFailures =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "SKIP_FAILURES", "NO"));
    const bool bPromoteToMulti = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "PROMOTE_TO_MULTI", "NO"));

    // check for GEOS
    if (!OGRGeometryFactory::haveGEOS())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "OGRLayer::Clip() requires GEOS support");
        return OGRERR_UNSUPPORTED_OPERATION;
    }

    ret = clone_spatial_filter(pLayerMethod, &pGeometryMethodFilter);
    if (ret != OGRERR_NONE)
        goto done;
    ret = create_field_map(poDefnInput, &mapInput);
    if (ret != OGRERR_NONE)
        goto done;
    ret = set_result_schema(pLayerResult, poDefnInput, nullptr, mapInput,
                            nullptr, false, papszOptions);
    if (ret != OGRERR_NONE)
        goto done;

    poDefnResult = pLayerResult->GetLayerDefn();
    for (auto &&x : this)
    {

        if (pfnProgress)
        {
            double p = progress_counter / progress_max;
            if (p > progress_ticker)
            {
                if (!pfnProgress(p, "", pProgressArg))
                {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        // set up the filter on method layer
        CPLErrorReset();
        OGRGeometry *x_geom =
            set_filter_from(pLayerMethod, pGeometryMethodFilter, x.get());
        if (CPLGetLastErrorType() != CE_None)
        {
            if (!bSkipFailures)
            {
                ret = OGRERR_FAILURE;
                goto done;
            }
            else
            {
                CPLErrorReset();
                ret = OGRERR_NONE;
            }
        }
        if (!x_geom)
        {
            continue;
        }

        OGRGeometryUniquePtr
            geom;  // this will be the geometry of the result feature
        // incrementally add area from y to geom
        for (auto &&y : pLayerMethod)
        {
            OGRGeometry *y_geom = y->GetGeometryRef();
            if (!y_geom)
                continue;
            if (!geom)
            {
                geom.reset(y_geom->clone());
            }
            else
            {
                CPLErrorReset();
                OGRGeometryUniquePtr geom_new(geom->Union(y_geom));
                if (CPLGetLastErrorType() != CE_None || geom_new == nullptr)
                {
                    if (!bSkipFailures)
                    {
                        ret = OGRERR_FAILURE;
                        goto done;
                    }
                    else
                    {
                        CPLErrorReset();
                        ret = OGRERR_NONE;
                    }
                }
                else
                {
                    geom.swap(geom_new);
                }
            }
        }

        // possibly add a new feature with area x intersection sum of y
        if (geom)
        {
            CPLErrorReset();
            OGRGeometryUniquePtr poIntersection(
                x_geom->Intersection(geom.get()));
            if (CPLGetLastErrorType() != CE_None || poIntersection == nullptr)
            {
                if (!bSkipFailures)
                {
                    ret = OGRERR_FAILURE;
                    goto done;
                }
                else
                {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                }
            }
            else if (!poIntersection->IsEmpty())
            {
                OGRFeatureUniquePtr z(new OGRFeature(poDefnResult));
                z->SetFieldsFrom(x.get(), mapInput);
                if (bPromoteToMulti)
                    poIntersection.reset(
                        promote_to_multi(poIntersection.release()));
                z->SetGeometryDirectly(poIntersection.release());
                ret = pLayerResult->CreateFeature(z.get());
                if (ret != OGRERR_NONE)
                {
                    if (!bSkipFailures)
                    {
                        goto done;
                    }
                    else
                    {
                        CPLErrorReset();
                        ret = OGRERR_NONE;
                    }
                }
            }
        }
    }
    if (pfnProgress && !pfnProgress(1.0, "", pProgressArg))
    {
        CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
        ret = OGRERR_FAILURE;
        goto done;
    }
done:
    // release resources
    pLayerMethod->SetSpatialFilter(pGeometryMethodFilter);
    if (pGeometryMethodFilter)
        delete pGeometryMethodFilter;
    if (mapInput)
        VSIFree(mapInput);
    return ret;
}

/************************************************************************/
/*                           OGR_L_Clip()                               */
/************************************************************************/

/**
 * \brief Clip off areas that are not covered by the method layer.
 *
 * The result layer contains features whose geometries represent areas
 * that are in the input layer and in the method layer. The features
 * in the result layer have the (possibly clipped) areas of features
 * in the input layer and the attributes from the same features. The
 * schema of the result layer can be set by the user or, if it is
 * empty, is initialized to contain all fields in the input layer.
 *
 * \note For best performance use the minimum amount of features in
 * the method layer and copy it into a memory layer.
 *
 * \note This method relies on GEOS support. Do not use unless the
 * GEOS support is compiled in.
 *
 * The recognized list of options is :
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a
 *     feature could not be inserted or a GEOS call failed.
 * </li>
 * <li>PROMOTE_TO_MULTI=YES/NO. Set to YES to convert Polygons
 *     into MultiPolygons, LineStrings to MultiLineStrings or
 *     Points to MultiPoints (only since GDAL 3.9.2 for the later)
 * </li>
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * </li>
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * </li>
 * </ul>
 *
 * This function is the same as the C++ method OGRLayer::Clip().
 *
 * @param pLayerInput the input layer. Should not be NULL.
 *
 * @param pLayerMethod the method layer. Should not be NULL.
 *
 * @param pLayerResult the layer where the features resulting from the
 * operation are inserted. Should not be NULL. See above the note
 * about the schema.
 *
 * @param papszOptions NULL terminated list of options (may be NULL).
 *
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 *
 * @param pProgressArg argument to be passed to pfnProgress. May be NULL.
 *
 * @return an error code if there was an error or the execution was
 * interrupted, OGRERR_NONE otherwise.
 *
 * @note The first geometry field is always used.
 *
 * @since OGR 1.10
 */

OGRErr OGR_L_Clip(OGRLayerH pLayerInput, OGRLayerH pLayerMethod,
                  OGRLayerH pLayerResult, char **papszOptions,
                  GDALProgressFunc pfnProgress, void *pProgressArg)

{
    VALIDATE_POINTER1(pLayerInput, "OGR_L_Clip", OGRERR_INVALID_HANDLE);
    VALIDATE_POINTER1(pLayerMethod, "OGR_L_Clip", OGRERR_INVALID_HANDLE);
    VALIDATE_POINTER1(pLayerResult, "OGR_L_Clip", OGRERR_INVALID_HANDLE);

    return OGRLayer::FromHandle(pLayerInput)
        ->Clip(OGRLayer::FromHandle(pLayerMethod),
               OGRLayer::FromHandle(pLayerResult), papszOptions, pfnProgress,
               pProgressArg);
}

/************************************************************************/
/*                              Erase()                                 */
/************************************************************************/

/**
 * \brief Remove areas that are covered by the method layer.
 *
 * The result layer contains features whose geometries represent areas
 * that are in the input layer but not in the method layer. The
 * features in the result layer have attributes from the input
 * layer. The schema of the result layer can be set by the user or, if
 * it is empty, is initialized to contain all fields in the input
 * layer.
 *
 * \note For best performance use the minimum amount of features in
 * the method layer and copy it into a memory layer.
 *
 * \note This method relies on GEOS support. Do not use unless the
 * GEOS support is compiled in.
 *
 * The recognized list of options is :
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a
 *     feature could not be inserted or a GEOS call failed.
 * </li>
 * <li>PROMOTE_TO_MULTI=YES/NO. Set to YES to convert Polygons
 *     into MultiPolygons, LineStrings to MultiLineStrings or
 *     Points to MultiPoints (only since GDAL 3.9.2 for the later)
 * </li>
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * </li>
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * </li>
 * </ul>
 *
 * This method is the same as the C function OGR_L_Erase().
 *
 * @param pLayerMethod the method layer. Should not be NULL.
 *
 * @param pLayerResult the layer where the features resulting from the
 * operation are inserted. Should not be NULL. See above the note
 * about the schema.
 *
 * @param papszOptions NULL terminated list of options (may be NULL).
 *
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 *
 * @param pProgressArg argument to be passed to pfnProgress. May be NULL.
 *
 * @return an error code if there was an error or the execution was
 * interrupted, OGRERR_NONE otherwise.
 *
 * @note The first geometry field is always used.
 *
 * @since OGR 1.10
 */

OGRErr OGRLayer::Erase(OGRLayer *pLayerMethod, OGRLayer *pLayerResult,
                       char **papszOptions, GDALProgressFunc pfnProgress,
                       void *pProgressArg)
{
    OGRErr ret = OGRERR_NONE;
    OGRFeatureDefn *poDefnInput = GetLayerDefn();
    OGRFeatureDefn *poDefnResult = nullptr;
    OGRGeometry *pGeometryMethodFilter = nullptr;
    int *mapInput = nullptr;
    double progress_max = static_cast<double>(GetFeatureCount(FALSE));
    double progress_counter = 0;
    double progress_ticker = 0;
    const bool bSkipFailures =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "SKIP_FAILURES", "NO"));
    const bool bPromoteToMulti = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "PROMOTE_TO_MULTI", "NO"));

    // check for GEOS
    if (!OGRGeometryFactory::haveGEOS())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "OGRLayer::Erase() requires GEOS support");
        return OGRERR_UNSUPPORTED_OPERATION;
    }

    // get resources
    ret = clone_spatial_filter(pLayerMethod, &pGeometryMethodFilter);
    if (ret != OGRERR_NONE)
        goto done;
    ret = create_field_map(poDefnInput, &mapInput);
    if (ret != OGRERR_NONE)
        goto done;
    ret = set_result_schema(pLayerResult, poDefnInput, nullptr, mapInput,
                            nullptr, false, papszOptions);
    if (ret != OGRERR_NONE)
        goto done;
    poDefnResult = pLayerResult->GetLayerDefn();

    for (auto &&x : this)
    {

        if (pfnProgress)
        {
            double p = progress_counter / progress_max;
            if (p > progress_ticker)
            {
                if (!pfnProgress(p, "", pProgressArg))
                {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    ret = OGRERR_FAILURE;
                    goto done;
                }
            }
            progress_counter += 1.0;
        }

        // set up the filter on the method layer
        CPLErrorReset();
        OGRGeometry *x_geom =
            set_filter_from(pLayerMethod, pGeometryMethodFilter, x.get());
        if (CPLGetLastErrorType() != CE_None)
        {
            if (!bSkipFailures)
            {
                ret = OGRERR_FAILURE;
                goto done;
            }
            else
            {
                CPLErrorReset();
                ret = OGRERR_NONE;
            }
        }
        if (!x_geom)
        {
            continue;
        }

        OGRGeometryUniquePtr geom(
            x_geom
                ->clone());  // this will be the geometry of the result feature
        // incrementally erase y from geom
        for (auto &&y : pLayerMethod)
        {
            OGRGeometry *y_geom = y->GetGeometryRef();
            if (!y_geom)
                continue;
            CPLErrorReset();
            OGRGeometryUniquePtr geom_new(geom->Difference(y_geom));
            if (CPLGetLastErrorType() != CE_None || geom_new == nullptr)
            {
                if (!bSkipFailures)
                {
                    ret = OGRERR_FAILURE;
                    goto done;
                }
                else
                {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                }
            }
            else
            {
                geom.swap(geom_new);
                if (geom->IsEmpty())
                {
                    break;
                }
            }
        }

        // add a new feature if there is remaining area
        if (!geom->IsEmpty())
        {
            OGRFeatureUniquePtr z(new OGRFeature(poDefnResult));
            z->SetFieldsFrom(x.get(), mapInput);
            if (bPromoteToMulti)
                geom.reset(promote_to_multi(geom.release()));
            z->SetGeometryDirectly(geom.release());
            ret = pLayerResult->CreateFeature(z.get());
            if (ret != OGRERR_NONE)
            {
                if (!bSkipFailures)
                {
                    goto done;
                }
                else
                {
                    CPLErrorReset();
                    ret = OGRERR_NONE;
                }
            }
        }
    }
    if (pfnProgress && !pfnProgress(1.0, "", pProgressArg))
    {
        CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
        ret = OGRERR_FAILURE;
        goto done;
    }
done:
    // release resources
    pLayerMethod->SetSpatialFilter(pGeometryMethodFilter);
    if (pGeometryMethodFilter)
        delete pGeometryMethodFilter;
    if (mapInput)
        VSIFree(mapInput);
    return ret;
}

/************************************************************************/
/*                           OGR_L_Erase()                              */
/************************************************************************/

/**
 * \brief Remove areas that are covered by the method layer.
 *
 * The result layer contains features whose geometries represent areas
 * that are in the input layer but not in the method layer. The
 * features in the result layer have attributes from the input
 * layer. The schema of the result layer can be set by the user or, if
 * it is empty, is initialized to contain all fields in the input
 * layer.
 *
 * \note For best performance use the minimum amount of features in
 * the method layer and copy it into a memory layer.
 *
 * \note This method relies on GEOS support. Do not use unless the
 * GEOS support is compiled in.
 *
 * The recognized list of options is :
 * <ul>
 * <li>SKIP_FAILURES=YES/NO. Set it to YES to go on, even when a
 *     feature could not be inserted or a GEOS call failed.
 * </li>
 * <li>PROMOTE_TO_MULTI=YES/NO. Set to YES to convert Polygons
 *     into MultiPolygons, LineStrings to MultiLineStrings or
 *     Points to MultiPoints (only since GDAL 3.9.2 for the later)
 * </li>
 * <li>INPUT_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the input layer.
 * </li>
 * <li>METHOD_PREFIX=string. Set a prefix for the field names that
 *     will be created from the fields of the method layer.
 * </li>
 * </ul>
 *
 * This function is the same as the C++ method OGRLayer::Erase().
 *
 * @param pLayerInput the input layer. Should not be NULL.
 *
 * @param pLayerMethod the method layer. Should not be NULL.
 *
 * @param pLayerResult the layer where the features resulting from the
 * operation are inserted. Should not be NULL. See above the note
 * about the schema.
 *
 * @param papszOptions NULL terminated list of options (may be NULL).
 *
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL.
 *
 * @param pProgressArg argument to be passed to pfnProgress. May be NULL.
 *
 * @return an error code if there was an error or the execution was
 * interrupted, OGRERR_NONE otherwise.
 *
 * @note The first geometry field is always used.
 *
 * @since OGR 1.10
 */

OGRErr OGR_L_Erase(OGRLayerH pLayerInput, OGRLayerH pLayerMethod,
                   OGRLayerH pLayerResult, char **papszOptions,
                   GDALProgressFunc pfnProgress, void *pProgressArg)

{
    VALIDATE_POINTER1(pLayerInput, "OGR_L_Erase", OGRERR_INVALID_HANDLE);
    VALIDATE_POINTER1(pLayerMethod, "OGR_L_Erase", OGRERR_INVALID_HANDLE);
    VALIDATE_POINTER1(pLayerResult, "OGR_L_Erase", OGRERR_INVALID_HANDLE);

    return OGRLayer::FromHandle(pLayerInput)
        ->Erase(OGRLayer::FromHandle(pLayerMethod),
                OGRLayer::FromHandle(pLayerResult), papszOptions, pfnProgress,
                pProgressArg);
}

/************************************************************************/
/*                  OGRLayer::FeatureIterator::Private                  */
/************************************************************************/

struct OGRLayer::FeatureIterator::Private
{
    CPL_DISALLOW_COPY_ASSIGN(Private)
    Private() = default;

    OGRFeatureUniquePtr m_poFeature{};
    OGRLayer *m_poLayer = nullptr;
    bool m_bError = false;
    bool m_bEOF = true;
};

/************************************************************************/
/*                OGRLayer::FeatureIterator::FeatureIterator()          */
/************************************************************************/

OGRLayer::FeatureIterator::FeatureIterator(OGRLayer *poLayer, bool bStart)
    : m_poPrivate(new OGRLayer::FeatureIterator::Private())
{
    m_poPrivate->m_poLayer = poLayer;
    if (bStart)
    {
        if (m_poPrivate->m_poLayer->m_poPrivate->m_bInFeatureIterator)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Only one feature iterator can be "
                     "active at a time");
            m_poPrivate->m_bError = true;
        }
        else
        {
            m_poPrivate->m_poLayer->ResetReading();
            m_poPrivate->m_poFeature.reset(
                m_poPrivate->m_poLayer->GetNextFeature());
            m_poPrivate->m_bEOF = m_poPrivate->m_poFeature == nullptr;
            m_poPrivate->m_poLayer->m_poPrivate->m_bInFeatureIterator = true;
        }
    }
}

/************************************************************************/
/*               ~OGRLayer::FeatureIterator::FeatureIterator()          */
/************************************************************************/

OGRLayer::FeatureIterator::~FeatureIterator()
{
    if (!m_poPrivate->m_bError && m_poPrivate->m_poLayer)
        m_poPrivate->m_poLayer->m_poPrivate->m_bInFeatureIterator = false;
}

/************************************************************************/
/*                              operator*()                             */
/************************************************************************/

OGRFeatureUniquePtr &OGRLayer::FeatureIterator::operator*()
{
    return m_poPrivate->m_poFeature;
}

/************************************************************************/
/*                              operator++()                            */
/************************************************************************/

OGRLayer::FeatureIterator &OGRLayer::FeatureIterator::operator++()
{
    m_poPrivate->m_poFeature.reset(m_poPrivate->m_poLayer->GetNextFeature());
    m_poPrivate->m_bEOF = m_poPrivate->m_poFeature == nullptr;
    return *this;
}

/************************************************************************/
/*                             operator!=()                             */
/************************************************************************/

bool OGRLayer::FeatureIterator::operator!=(
    const OGRLayer::FeatureIterator &it) const
{
    return m_poPrivate->m_bEOF != it.m_poPrivate->m_bEOF;
}

/************************************************************************/
/*                                 begin()                              */
/************************************************************************/

OGRLayer::FeatureIterator OGRLayer::begin()
{
    return {this, true};
}

/************************************************************************/
/*                                  end()                               */
/************************************************************************/

OGRLayer::FeatureIterator OGRLayer::end()
{
    return {this, false};
}

/************************************************************************/
/*                     OGRLayer::GetGeometryTypes()                     */
/************************************************************************/

/** \brief Get actual geometry types found in features.
 *
 * This method iterates over features to retrieve their geometry types. This
 * is mostly useful for layers that report a wkbUnknown geometry type with
 * GetGeomType() or GetGeomFieldDefn(iGeomField)->GetType().
 *
 * By default this method returns an array of nEntryCount entries with each
 * geometry type (in OGRGeometryTypeCounter::eGeomType) and the corresponding
 * number of features (in OGRGeometryTypeCounter::nCount).
 * Features without geometries are reported as eGeomType == wkbNone.
 *
 * The nFlagsGGT parameter can be a combination (with binary or operator) of the
 * following hints:
 * <ul>
 * <li>OGR_GGT_COUNT_NOT_NEEDED: to indicate that only the set of geometry types
 * matter, not the number of features per geometry type. Consequently the value
 * of OGRGeometryTypeCounter::nCount should be ignored.</li>
 * <li>OGR_GGT_STOP_IF_MIXED: to indicate that the implementation may stop
 * iterating over features as soon as 2 different geometry types (not counting
 * null geometries) are found. The value of OGRGeometryTypeCounter::nCount
 * should be ignored (zero might be systematically reported by some
 * implementations).</li> <li>OGR_GGT_GEOMCOLLECTIONZ_TINZ: to indicate that if
 * a geometry is of type wkbGeometryCollection25D and its first sub-geometry is
 * of type wkbTINZ, wkbTINZ should be reported as geometry type. This is mostly
 * useful for the ESRI Shapefile and (Open)FileGDB drivers regarding MultiPatch
 * geometries.</li>
 * </ul>
 *
 * If the layer has no features, a non-NULL returned array with nEntryCount == 0
 * will be returned.
 *
 * Spatial and/or attribute filters will be taken into account.
 *
 * This method will error out on a layer without geometry fields
 * (GetGeomType() == wkbNone).
 *
 * A cancellation callback may be provided. The progress percentage it is called
 * with is not relevant. The callback should return TRUE if processing should go
 * on, or FALSE if it should be interrupted.
 *
 * @param iGeomField Geometry field index.
 * @param nFlagsGGT Hint flags. 0, or combination of OGR_GGT_COUNT_NOT_NEEDED,
 *                  OGR_GGT_STOP_IF_MIXED, OGR_GGT_GEOMCOLLECTIONZ_TINZ
 * @param[out] nEntryCountOut Number of entries in the returned array.
 * @param pfnProgress Cancellation callback. May be NULL.
 * @param pProgressData User data for the cancellation callback. May be NULL.
 * @return an array of nEntryCount that must be freed with CPLFree(),
 *         or NULL in case of error
 * @since GDAL 3.6
 */
OGRGeometryTypeCounter *
OGRLayer::GetGeometryTypes(int iGeomField, int nFlagsGGT, int &nEntryCountOut,
                           GDALProgressFunc pfnProgress, void *pProgressData)
{
    OGRFeatureDefn *poDefn = GetLayerDefn();
    const int nGeomFieldCount = poDefn->GetGeomFieldCount();
    if (iGeomField < 0 || iGeomField >= nGeomFieldCount)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid value for iGeomField");
        nEntryCountOut = 0;
        return nullptr;
    }

    // Ignore all fields but the geometry one of interest
    CPLStringList aosIgnoredFieldsRestore;
    CPLStringList aosIgnoredFields;
    const int nFieldCount = poDefn->GetFieldCount();
    for (int iField = 0; iField < nFieldCount; iField++)
    {
        const auto poFieldDefn = poDefn->GetFieldDefn(iField);
        const char *pszName = poFieldDefn->GetNameRef();
        if (poFieldDefn->IsIgnored())
            aosIgnoredFieldsRestore.AddString(pszName);
        if (iField != iGeomField)
            aosIgnoredFields.AddString(pszName);
    }
    for (int iField = 0; iField < nGeomFieldCount; iField++)
    {
        const auto poFieldDefn = poDefn->GetGeomFieldDefn(iField);
        const char *pszName = poFieldDefn->GetNameRef();
        if (poFieldDefn->IsIgnored())
            aosIgnoredFieldsRestore.AddString(pszName);
        if (iField != iGeomField)
            aosIgnoredFields.AddString(pszName);
    }
    if (poDefn->IsStyleIgnored())
        aosIgnoredFieldsRestore.AddString("OGR_STYLE");
    aosIgnoredFields.AddString("OGR_STYLE");
    SetIgnoredFields(aosIgnoredFields.List());

    // Iterate over features
    std::map<OGRwkbGeometryType, int64_t> oMapCount;
    std::set<OGRwkbGeometryType> oSetNotNull;
    const bool bGeomCollectionZTInZ =
        (nFlagsGGT & OGR_GGT_GEOMCOLLECTIONZ_TINZ) != 0;
    const bool bStopIfMixed = (nFlagsGGT & OGR_GGT_STOP_IF_MIXED) != 0;
    if (pfnProgress == GDALDummyProgress)
        pfnProgress = nullptr;
    bool bInterrupted = false;
    for (auto &&poFeature : *this)
    {
        const auto poGeom = poFeature->GetGeomFieldRef(iGeomField);
        if (poGeom == nullptr)
        {
            ++oMapCount[wkbNone];
        }
        else
        {
            auto eGeomType = poGeom->getGeometryType();
            if (bGeomCollectionZTInZ && eGeomType == wkbGeometryCollection25D)
            {
                const auto poGC = poGeom->toGeometryCollection();
                if (poGC->getNumGeometries() > 0)
                {
                    auto eSubGeomType =
                        poGC->getGeometryRef(0)->getGeometryType();
                    if (eSubGeomType == wkbTINZ)
                        eGeomType = wkbTINZ;
                }
            }
            ++oMapCount[eGeomType];
            if (bStopIfMixed)
            {
                oSetNotNull.insert(eGeomType);
                if (oSetNotNull.size() == 2)
                    break;
            }
        }
        if (pfnProgress && !pfnProgress(0.0, "", pProgressData))
        {
            bInterrupted = true;
            break;
        }
    }

    // Restore ignore fields state
    SetIgnoredFields(aosIgnoredFieldsRestore.List());

    if (bInterrupted)
    {
        nEntryCountOut = 0;
        return nullptr;
    }

    // Format result
    nEntryCountOut = static_cast<int>(oMapCount.size());
    OGRGeometryTypeCounter *pasRet = static_cast<OGRGeometryTypeCounter *>(
        CPLCalloc(1 + nEntryCountOut, sizeof(OGRGeometryTypeCounter)));
    int i = 0;
    for (const auto &oIter : oMapCount)
    {
        pasRet[i].eGeomType = oIter.first;
        pasRet[i].nCount = oIter.second;
        ++i;
    }
    return pasRet;
}

/************************************************************************/
/*                      OGR_L_GetGeometryTypes()                        */
/************************************************************************/

/** \brief Get actual geometry types found in features.
 *
 * See OGRLayer::GetGeometryTypes() for details.
 *
 * @param hLayer Layer.
 * @param iGeomField Geometry field index.
 * @param nFlags Hint flags. 0, or combination of OGR_GGT_COUNT_NOT_NEEDED,
 *               OGR_GGT_STOP_IF_MIXED, OGR_GGT_GEOMCOLLECTIONZ_TINZ
 * @param[out] pnEntryCount Pointer to the number of entries in the returned
 *                          array. Must not be NULL.
 * @param pfnProgress Cancellation callback. May be NULL.
 * @param pProgressData User data for the cancellation callback. May be NULL.
 * @return an array of *pnEntryCount that must be freed with CPLFree(),
 *         or NULL in case of error
 * @since GDAL 3.6
 */
OGRGeometryTypeCounter *OGR_L_GetGeometryTypes(OGRLayerH hLayer, int iGeomField,
                                               int nFlags, int *pnEntryCount,
                                               GDALProgressFunc pfnProgress,
                                               void *pProgressData)
{
    VALIDATE_POINTER1(hLayer, "OGR_L_GetGeometryTypes", nullptr);
    VALIDATE_POINTER1(pnEntryCount, "OGR_L_GetGeometryTypes", nullptr);

    return OGRLayer::FromHandle(hLayer)->GetGeometryTypes(
        iGeomField, nFlags, *pnEntryCount, pfnProgress, pProgressData);
}

/************************************************************************/
/*                    OGRLayer::GetSupportedSRSList()                   */
/************************************************************************/

/** \brief Get the list of SRS supported.
 *
 * The base implementation of this method will return an empty list. Some
 * drivers (OAPIF, WFS) may return a non-empty list.
 *
 * One of the SRS returned may be passed to SetActiveSRS() to change the
 * active SRS.
 *
 * @param iGeomField Geometry field index.
 * @return list of supported SRS.
 * @since GDAL 3.7
 */
const OGRLayer::GetSupportedSRSListRetType &
OGRLayer::GetSupportedSRSList(CPL_UNUSED int iGeomField)
{
    static OGRLayer::GetSupportedSRSListRetType empty;
    return empty;
}

/************************************************************************/
/*                    OGR_L_GetSupportedSRSList()                       */
/************************************************************************/

/** \brief Get the list of SRS supported.
 *
 * The base implementation of this method will return an empty list. Some
 * drivers (OAPIF, WFS) may return a non-empty list.
 *
 * One of the SRS returned may be passed to SetActiveSRS() to change the
 * active SRS.
 *
 * @param hLayer Layer.
 * @param iGeomField Geometry field index.
 * @param[out] pnCount Number of values in returned array. Must not be null.
 * @return list of supported SRS, to be freed with OSRFreeSRSArray(), or
 * nullptr
 * @since GDAL 3.7
 */
OGRSpatialReferenceH *OGR_L_GetSupportedSRSList(OGRLayerH hLayer,
                                                int iGeomField, int *pnCount)
{
    VALIDATE_POINTER1(hLayer, "OGR_L_GetSupportedSRSList", nullptr);
    VALIDATE_POINTER1(pnCount, "OGR_L_GetSupportedSRSList", nullptr);

    const auto &srsList =
        OGRLayer::FromHandle(hLayer)->GetSupportedSRSList(iGeomField);
    *pnCount = static_cast<int>(srsList.size());
    if (srsList.empty())
    {
        return nullptr;
    }
    OGRSpatialReferenceH *pahRet = static_cast<OGRSpatialReferenceH *>(
        CPLMalloc((1 + srsList.size()) * sizeof(OGRSpatialReferenceH)));
    size_t i = 0;
    for (const auto &poSRS : srsList)
    {
        poSRS->Reference();
        pahRet[i] = OGRSpatialReference::ToHandle(poSRS.get());
        ++i;
    }
    pahRet[i] = nullptr;
    return pahRet;
}

/************************************************************************/
/*                       OGRLayer::SetActiveSRS()                       */
/************************************************************************/

/** \brief Change the active SRS.
 *
 * The passed SRS must be in the list returned by GetSupportedSRSList()
 * (the actual pointer may be different, but should be tested as identical
 * with OGRSpatialReference::IsSame()).
 *
 * Changing the active SRS affects:
 * <ul>
 * <li>the SRS in which geometries of returned features are expressed,</li>
 * <li>the SRS in which geometries of passed features (CreateFeature(),
 * SetFeature()) are expressed,</li>
 * <li>the SRS returned by GetSpatialRef() and
 * GetGeomFieldDefn()->GetSpatialRef(),</li>
 * <li>the SRS used to interpret SetSpatialFilter() values.</li>
 * </ul>
 * This also resets feature reading and the spatial filter.
 * Note however that this does not modify the storage SRS of the features of
 * geometries. Said otherwise, this setting is volatile and has no persistent
 * effects after dataset reopening.
 *
 * @param iGeomField Geometry field index.
 * @param poSRS SRS to use
 * @return OGRERR_NONE in case of success, or OGRERR_FAILURE if
 *         the passed SRS is not in GetSupportedSRSList()
 * @since GDAL 3.7
 */
OGRErr OGRLayer::SetActiveSRS(CPL_UNUSED int iGeomField,
                              CPL_UNUSED const OGRSpatialReference *poSRS)
{
    return OGRERR_FAILURE;
}

/************************************************************************/
/*                         OGR_L_SetActiveSRS()                         */
/************************************************************************/

/** \brief Change the active SRS.
 *
 * The passed SRS must be in the list returned by GetSupportedSRSList()
 * (the actual pointer may be different, but should be tested as identical
 * with OGRSpatialReference::IsSame()).
 *
 * Changing the active SRS affects:
 * <ul>
 * <li>the SRS in which geometries of returned features are expressed,</li>
 * <li>the SRS in which geometries of passed features (CreateFeature(),
 * SetFeature()) are expressed,</li>
 * <li>the SRS returned by GetSpatialRef() and
 * GetGeomFieldDefn()->GetSpatialRef(),</li>
 * <li>the SRS used to interpret SetSpatialFilter() values.</li>
 * </ul>
 * This also resets feature reading and the spatial filter.
 * Note however that this does not modify the storage SRS of the features of
 * geometries. Said otherwise, this setting is volatile and has no persistent
 * effects after dataset reopening.
 *
 * @param hLayer Layer.
 * @param iGeomField Geometry field index.
 * @param hSRS SRS to use
 * @return OGRERR_NONE in case of success, OGRERR_FAILURE if
 *         the passed SRS is not in GetSupportedSRSList().
 * @since GDAL 3.7
 */
OGRErr OGR_L_SetActiveSRS(OGRLayerH hLayer, int iGeomField,
                          OGRSpatialReferenceH hSRS)
{
    VALIDATE_POINTER1(hLayer, "OGR_L_SetActiveSRS", OGRERR_FAILURE);
    return OGRLayer::FromHandle(hLayer)->SetActiveSRS(
        iGeomField, OGRSpatialReference::FromHandle(hSRS));
}

/************************************************************************/
/*                             GetDataset()                             */
/************************************************************************/

/** Return the dataset associated with this layer.
 *
 * As of GDAL 3.9, GetDataset() is implemented on all in-tree drivers that
 * have CreateLayer() capability. It may not be implemented in read-only
 * drivers or out-of-tree drivers.
 *
 * It is currently only used by the GetRecordBatchSchema()
 * method to retrieve the field domain associated with a field, to fill the
 * dictionary field of a struct ArrowSchema.
 * It is also used by CreateFieldFromArrowSchema() to determine which field
 * types and subtypes are supported by the layer, by inspecting the driver
 * metadata, and potentially use fallback types when needed.
 *
 * This method is the same as the C function OGR_L_GetDataset().
 *
 * @return dataset, or nullptr when unknown.
 * @since GDAL 3.6
 */
GDALDataset *OGRLayer::GetDataset()
{
    return nullptr;
}

/************************************************************************/
/*                          OGR_L_GetDataset()                          */
/************************************************************************/

/** Return the dataset associated with this layer.
 *
 * As of GDAL 3.9, GetDataset() is implemented on all in-tree drivers that
 * have CreateLayer() capability. It may not be implemented in read-only
 * drivers or out-of-tree drivers.
 *
 * It is currently only used by the GetRecordBatchSchema()
 * method to retrieve the field domain associated with a field, to fill the
 * dictionary field of a struct ArrowSchema.
 * It is also used by CreateFieldFromArrowSchema() to determine which field
 * types and subtypes are supported by the layer, by inspecting the driver
 * metadata, and potentially use fallback types when needed.
 *
 * This function is the same as the C++ method OGRLayer::GetDataset().
 *
 * @return dataset, or nullptr when unknown.
 * @since GDAL 3.9
 */
GDALDatasetH OGR_L_GetDataset(OGRLayerH hLayer)
{
    VALIDATE_POINTER1(hLayer, "OGR_L_GetDataset", nullptr);
    return GDALDataset::ToHandle(OGRLayer::FromHandle(hLayer)->GetDataset());
}
