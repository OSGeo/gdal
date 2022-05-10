/******************************************************************************
 *
 * Project:  SAP HANA Spatial Driver
 * Purpose:  OGRHanaResultLayer class implementation
 * Author:   Maxim Rylov
 *
 ******************************************************************************
 * Copyright (c) 2020, SAP SE
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

#include "ogr_hana.h"

#include <memory>

#include "odbc/PreparedStatement.h"
#include "odbc/ResultSetMetaData.h"

CPL_CVSID("$Id$")

namespace OGRHANA {

/************************************************************************/
/*                              OGRHanaResultLayer()                    */
/************************************************************************/

OGRHanaResultLayer::OGRHanaResultLayer(
    OGRHanaDataSource* datasource, const char* query)
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
    OGRErr err = InitFeatureDefinition(
        names.first, names.second, rawQuery_, "sql_statement");
    return err;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRHanaResultLayer::TestCapability(const char* capabilities)
{
    if (EQUAL(capabilities, OLCFastFeatureCount)
        || EQUAL(capabilities, OLCFastSpatialFilter)
        || EQUAL(capabilities, OLCFastGetExtent))
    {
        EnsureInitialized();
        return (geomColumns_.size() > 0);
    }
    if (EQUAL(capabilities, OLCStringsAsUTF8))
        return TRUE;

    return FALSE;
}

} /* end of OGRHANA namespace */
