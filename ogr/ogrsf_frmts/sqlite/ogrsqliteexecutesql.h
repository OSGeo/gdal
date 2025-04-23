/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Run SQL requests with SQLite SQL engine
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_SQLITE_EXECUTE_SQL_H_INCLUDED
#define OGR_SQLITE_EXECUTE_SQL_H_INCLUDED

#include "ogrsf_frmts.h"
#include <set>

OGRLayer *OGRSQLiteExecuteSQL(GDALDataset *poDS, const char *pszStatement,
                              OGRGeometry *poSpatialFilter,
                              const char *pszDialect);

/************************************************************************/
/*                               LayerDesc                              */
/************************************************************************/

class LayerDesc
{
  public:
    bool operator<(const LayerDesc &other) const
    {
        return osOriginalStr < other.osOriginalStr;
    }

    CPLString osOriginalStr{};
    CPLString osSubstitutedName{};
    CPLString osDSName{};
    CPLString osLayerName{};
};

std::set<LayerDesc> OGRSQLiteGetReferencedLayers(const char *pszStatement);

#endif /* ndef OGR_SQLITE_EXECUTE_SQL_H_INCLUDED */
