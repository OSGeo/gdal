/******************************************************************************
 * $Id$
 *
 * Project:  Interlis 1/2 Translator
 * Purpose:  IlisMeta model reader.
 * Author:   Pirmin Kalberer, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2014, Pirmin Kalberer, Sourcepole AG
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

#ifndef _IMDREADER_H_INCLUDED
#define _IMDREADER_H_INCLUDED

#include "cpl_vsi.h"
#include "cpl_error.h"
#include "ogr_feature.h"
#include <list>
#include <map>


class GeomFieldInfo
{
public:
    OGRFeatureDefn* geomTable; /* separate geometry table for Ili 1 */
    CPLString       iliGeomType;
    GeomFieldInfo() : geomTable(0) {};
};

typedef std::map<CPLString,GeomFieldInfo> GeomFieldInfos; /* key: geom field name, value: ILI geom field info */
typedef std::map<CPLString,CPLString> StructFieldInfos; /* key: struct field name, value: struct table */

class FeatureDefnInfo
{
public:
    OGRFeatureDefn* poTableDefn;
    GeomFieldInfos  poGeomFieldInfos;
    StructFieldInfos poStructFieldInfos;
    FeatureDefnInfo() : poTableDefn(0) {};
};
typedef std::list<FeatureDefnInfo> FeatureDefnInfos;

class IliModelInfo
{
public:
    CPLString name;
    CPLString version;
    CPLString uri;
};
typedef std::list<IliModelInfo> IliModelInfos;

class ImdReader
{
public:
    int                  iliVersion; /* 1 or 2 */
    IliModelInfos        modelInfos;
    CPLString            mainModelName;
    CPLString            mainBasketName;
    CPLString            mainTopicName;
    FeatureDefnInfos     featureDefnInfos;
    char                 codeBlank;
    char                 codeUndefined;
    char                 codeContinue;
public:
                         ImdReader(int iliVersion);
                        ~ImdReader();
    void                 ReadModel(const char *pszFilename);
    FeatureDefnInfo      GetFeatureDefnInfo(const char *pszLayerName);
};

#endif /* _IMDREADER_H_INCLUDED */
