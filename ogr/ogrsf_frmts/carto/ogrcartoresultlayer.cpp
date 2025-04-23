/******************************************************************************
 *
 * Project:  Carto Translator
 * Purpose:  Implements OGRCARTOResultLayer class.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_carto.h"

/************************************************************************/
/*                          OGRCARTOResultLayer()                     */
/************************************************************************/

OGRCARTOResultLayer::OGRCARTOResultLayer(OGRCARTODataSource *poDSIn,
                                         const char *pszRawQueryIn)
    : OGRCARTOLayer(poDSIn), poFirstFeature(nullptr)
{
    osBaseSQL = pszRawQueryIn;
    SetDescription("result");
}

/************************************************************************/
/*                       ~OGRCARTOResultLayer()                       */
/************************************************************************/

OGRCARTOResultLayer::~OGRCARTOResultLayer()

{
    delete poFirstFeature;
}

/************************************************************************/
/*                          GetLayerDefnInternal()                      */
/************************************************************************/

OGRFeatureDefn *OGRCARTOResultLayer::GetLayerDefnInternal(json_object *poObjIn)
{
    if (poFeatureDefn != nullptr)
        return poFeatureDefn;

    EstablishLayerDefn("result", poObjIn);

    return poFeatureDefn;
}

/************************************************************************/
/*                           GetNextRawFeature()                        */
/************************************************************************/

OGRFeature *OGRCARTOResultLayer::GetNextRawFeature()
{
    if (poFirstFeature)
    {
        OGRFeature *poRet = poFirstFeature;
        poFirstFeature = nullptr;
        return poRet;
    }
    else
        return OGRCARTOLayer::GetNextRawFeature();
}

/************************************************************************/
/*                                IsOK()                                */
/************************************************************************/

bool OGRCARTOResultLayer::IsOK()
{
    CPLErrorReset();
    poFirstFeature = GetNextFeature();
    return CPLGetLastErrorType() == 0;
}

/************************************************************************/
/*                             GetSRS_SQL()                             */
/************************************************************************/

CPLString OGRCARTOResultLayer::GetSRS_SQL(const char *pszGeomCol)
{
    CPLString osSQL;
    CPLString osLimitedSQL;

    size_t nPos = osBaseSQL.ifind(" LIMIT ");
    if (nPos != std::string::npos)
    {
        osLimitedSQL = osBaseSQL;
        size_t nSize = osLimitedSQL.size();
        for (size_t i = nPos + strlen(" LIMIT "); i < nSize; i++)
        {
            if (osLimitedSQL[i] == ' ' && osLimitedSQL[i - 1] == '0')
            {
                osLimitedSQL[i - 1] = '1';
                break;
            }
            osLimitedSQL[i] = '0';
        }
    }
    else
        osLimitedSQL.Printf("%s LIMIT 1", osBaseSQL.c_str());

    /* Assuming that the SRID of the first non-NULL geometry applies */
    /* to geometries of all rows. */
    osSQL.Printf("SELECT srid, srtext FROM spatial_ref_sys WHERE srid IN "
                 "(SELECT ST_SRID(%s) FROM (%s) ogr_subselect)",
                 OGRCARTOEscapeIdentifier(pszGeomCol).c_str(),
                 osLimitedSQL.c_str());

    return osSQL;
}
