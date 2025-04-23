/******************************************************************************
 *
 * Project:  Interlis 1/2 Translator
 * Purpose:  IlisMeta model reader.
 * Author:   Pirmin Kalberer, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2014, Pirmin Kalberer, Sourcepole AG
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef IMDREADER_H_INCLUDED
#define IMDREADER_H_INCLUDED

#include "cpl_vsi.h"
#include "cpl_error.h"
#include "ogr_feature.h"
#include <list>
#include <map>

class GeomFieldInfo
{
    OGRFeatureDefn *geomTable; /* separate geometry table for Ili 1 */
  public:
    CPLString iliGeomType;

    GeomFieldInfo() : geomTable(nullptr)
    {
    }

    ~GeomFieldInfo()
    {
        if (geomTable)
            geomTable->Release();
    }

    GeomFieldInfo(const GeomFieldInfo &other)
    {
        geomTable = other.geomTable;
        if (geomTable)
            geomTable->Reference();
        iliGeomType = other.iliGeomType;
    }

    GeomFieldInfo &operator=(const GeomFieldInfo &other)
    {
        if (this != &other)
        {
            if (geomTable)
                geomTable->Release();
            geomTable = other.geomTable;
            if (geomTable)
                geomTable->Reference();
            iliGeomType = other.iliGeomType;
        }
        return *this;
    }

    OGRFeatureDefn *GetGeomTableDefnRef() const
    {
        return geomTable;
    }

    void SetGeomTableDefn(OGRFeatureDefn *geomTableIn)
    {
        CPLAssert(geomTable == nullptr);
        geomTable = geomTableIn;
        if (geomTable)
            geomTable->Reference();
    }
};

typedef std::map<CPLString, GeomFieldInfo>
    GeomFieldInfos; /* key: geom field name, value: ILI geom field info */
typedef std::map<CPLString, CPLString>
    StructFieldInfos; /* key: struct field name, value: struct table */

class FeatureDefnInfo
{
    OGRFeatureDefn *poTableDefn;

  public:
    GeomFieldInfos poGeomFieldInfos;
    StructFieldInfos poStructFieldInfos;

    FeatureDefnInfo() : poTableDefn(nullptr)
    {
    }

    ~FeatureDefnInfo()
    {
        if (poTableDefn)
            poTableDefn->Release();
    }

    FeatureDefnInfo(const FeatureDefnInfo &other)
    {
        poTableDefn = other.poTableDefn;
        if (poTableDefn)
            poTableDefn->Reference();
        poGeomFieldInfos = other.poGeomFieldInfos;
        poStructFieldInfos = other.poStructFieldInfos;
    }

    FeatureDefnInfo &operator=(const FeatureDefnInfo &other)
    {
        if (this != &other)
        {
            if (poTableDefn)
                poTableDefn->Release();
            poTableDefn = other.poTableDefn;
            if (poTableDefn)
                poTableDefn->Reference();
            poGeomFieldInfos = other.poGeomFieldInfos;
            poStructFieldInfos = other.poStructFieldInfos;
        }
        return *this;
    }

    OGRFeatureDefn *GetTableDefnRef() const
    {
        return poTableDefn;
    }

    void SetTableDefn(OGRFeatureDefn *poTableDefnIn)
    {
        CPLAssert(poTableDefn == nullptr);
        poTableDefn = poTableDefnIn;
        if (poTableDefn)
            poTableDefn->Reference();
    }
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
  public:           // TODO(schwehr): Private?
    int iliVersion; /* 1 or 2 */
    IliModelInfos modelInfos;
    CPLString mainModelName;
    CPLString mainBasketName;
    CPLString mainTopicName;
    FeatureDefnInfos featureDefnInfos;
    char codeBlank;
    char codeUndefined;
    char codeContinue;

  public:
    explicit ImdReader(int iliVersion);
    ~ImdReader();
    void ReadModel(const char *pszFilename);
    FeatureDefnInfo GetFeatureDefnInfo(const char *pszLayerName);
};

#endif /* IMDREADER_H_INCLUDED */
