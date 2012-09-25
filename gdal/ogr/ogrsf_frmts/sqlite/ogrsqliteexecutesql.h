/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Run SQL requests with SQLite SQL engine
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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

#ifndef _OGR_SQLITE_EXECUTE_SQL_H_INCLUDED
#define _OGR_SQLITE_EXECUTE_SQL_H_INCLUDED

#include "ogrsf_frmts.h"
#include <set>

OGRLayer * OGRSQLiteExecuteSQL( OGRDataSource* poDS,
                                const char *pszStatement,
                                OGRGeometry *poSpatialFilter,
                                const char *pszDialect );


/************************************************************************/
/*                               LayerDesc                              */
/************************************************************************/

class LayerDesc
{
    public:
        LayerDesc() {};

        bool operator < ( const LayerDesc& other ) const
        {
            return osOriginalStr < other.osOriginalStr;
        }

        CPLString osOriginalStr;
        CPLString osSubstitutedName;
        CPLString osDSName;
        CPLString osLayerName;
};

std::set<LayerDesc> OGRSQLiteGetReferencedLayers(const char* pszStatement);

#endif /* ndef _OGR_SQLITE_EXECUTE_SQL_H_INCLUDED */


