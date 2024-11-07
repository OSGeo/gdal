/******************************************************************************
 *
 * Project:  AmigoCloud Translator
 * Purpose:  Implements OGRAmigoCloudResultLayer class.
 * Author:   Victor Chernetsky, <victor at amigocloud dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Victor Chernetsky, <victor at amigocloud dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_amigocloud.h"

/************************************************************************/
/*                          OGRAmigoCloudResultLayer()                     */
/************************************************************************/

OGRAmigoCloudResultLayer::OGRAmigoCloudResultLayer(
    OGRAmigoCloudDataSource *poDSIn, const char *pszRawQueryIn)
    : OGRAmigoCloudLayer(poDSIn)
{
    osBaseSQL = pszRawQueryIn;
    SetDescription("result");
    poFirstFeature = nullptr;
}

/************************************************************************/
/*                       ~OGRAmigoCloudResultLayer()                       */
/************************************************************************/

OGRAmigoCloudResultLayer::~OGRAmigoCloudResultLayer()

{
    delete poFirstFeature;
}

/************************************************************************/
/*                          GetLayerDefnInternal()                      */
/************************************************************************/

OGRFeatureDefn *
OGRAmigoCloudResultLayer::GetLayerDefnInternal(json_object *poObjIn)
{
    if (poFeatureDefn != nullptr)
        return poFeatureDefn;

    EstablishLayerDefn("result", poObjIn);

    return poFeatureDefn;
}

/************************************************************************/
/*                           GetNextRawFeature()                        */
/************************************************************************/

OGRFeature *OGRAmigoCloudResultLayer::GetNextRawFeature()
{
    if (poFirstFeature)
    {
        OGRFeature *poRet = poFirstFeature;
        poFirstFeature = nullptr;
        return poRet;
    }
    else
        return OGRAmigoCloudLayer::GetNextRawFeature();
}

/************************************************************************/
/*                                IsOK()                                */
/************************************************************************/

int OGRAmigoCloudResultLayer::IsOK()
{
    CPLErrorReset();
    poFirstFeature = GetNextFeature();
    return CPLGetLastErrorType() == 0;
}

/************************************************************************/
/*                             GetSRS_SQL()                             */
/************************************************************************/

CPLString OGRAmigoCloudResultLayer::GetSRS_SQL(const char *pszGeomCol)
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
                 OGRAMIGOCLOUDEscapeIdentifier(pszGeomCol).c_str(),
                 osLimitedSQL.c_str());

    return osSQL;
}
