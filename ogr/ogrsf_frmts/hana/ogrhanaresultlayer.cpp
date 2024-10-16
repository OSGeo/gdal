/******************************************************************************
 *
 * Project:  SAP HANA Spatial Driver
 * Purpose:  OGRHanaResultLayer class implementation
 * Author:   Maxim Rylov
 *
 ******************************************************************************
 * Copyright (c) 2020, SAP SE
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_core.h"
#include "ogr_hana.h"

#include <memory>

#include "odbc/PreparedStatement.h"
#include "odbc/ResultSetMetaData.h"

namespace OGRHANA
{

/************************************************************************/
/*                              OGRHanaResultLayer()                    */
/************************************************************************/

OGRHanaResultLayer::OGRHanaResultLayer(OGRHanaDataSource *datasource,
                                       const char *query)
    : OGRHanaLayer(datasource)
{
    rawQuery_ = (query == nullptr) ? "" : query;
    SetDescription("sql_statement");
}

/************************************************************************/
/*                                Initialize()                          */
/************************************************************************/

OGRErr OGRHanaResultLayer::Initialize()
{
    if (initialized_)
        return OGRERR_NONE;

    auto names = dataSource_->FindSchemaAndTableNames(rawQuery_.c_str());
    OGRErr err = InitFeatureDefinition(names.first, names.second, rawQuery_,
                                       "sql_statement");
    return err;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRHanaResultLayer::TestCapability(const char *capabilities)
{
    if (EQUAL(capabilities, OLCFastGetExtent))
    {
        EnsureInitialized();
        return IsFastExtentAvailable();
    }
    if (EQUAL(capabilities, OLCFastFeatureCount) ||
        EQUAL(capabilities, OLCFastSpatialFilter))
    {
        EnsureInitialized();
        return (geomColumns_.size() > 0);
    }
    if (EQUAL(capabilities, OLCStringsAsUTF8))
        return TRUE;

    return FALSE;
}

}  // namespace OGRHANA
