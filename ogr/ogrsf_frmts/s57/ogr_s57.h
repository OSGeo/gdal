/******************************************************************************
 *
 * Project:  S-57 Translator
 * Purpose:  Declarations for classes binding S57 support onto OGRLayer,
 *           OGRDataSource and OGRDriver.  See also s57.h.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_S57_H_INCLUDED
#define OGR_S57_H_INCLUDED

#include "ogrsf_frmts.h"
#include "s57.h"

class OGRS57DataSource;

/************************************************************************/
/*                             OGRS57Layer                              */
/*                                                                      */
/*      Represents all features of a particular S57 object class.       */
/************************************************************************/

class OGRS57Layer final : public OGRLayer
{
    OGRS57DataSource *poDS;

    OGRFeatureDefn *poFeatureDefn;

    int nCurrentModule;
    int nRCNM;
    int nOBJL;
    int nNextFEIndex;
    int nFeatureCount;

  public:
    OGRS57Layer(OGRS57DataSource *poDS, OGRFeatureDefn *,
                int nFeatureCount = -1, int nOBJL = -1);
    virtual ~OGRS57Layer();

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;
    OGRFeature *GetNextUnfilteredFeature();
    virtual OGRFeature *GetFeature(GIntBig nFeatureId) override;

    virtual GIntBig GetFeatureCount(int bForce = TRUE) override;
    virtual OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                              bool bForce) override;

    OGRFeatureDefn *GetLayerDefn() override
    {
        return poFeatureDefn;
    }

    virtual OGRErr ICreateFeature(OGRFeature *poFeature) override;
    int TestCapability(const char *) override;
};

/************************************************************************/
/*                          OGRS57DataSource                            */
/************************************************************************/

class OGRS57DataSource final : public GDALDataset
{
    int nLayers;
    OGRS57Layer **papoLayers;

    OGRSpatialReference *poSpatialRef;

    char **papszOptions;

    int nModules;
    S57Reader **papoModules;

    S57Writer *poWriter;

    S57ClassContentExplorer *poClassContentExplorer;

    bool bExtentsSet;
    OGREnvelope oExtents;

    CPL_DISALLOW_COPY_ASSIGN(OGRS57DataSource)

  public:
    explicit OGRS57DataSource(char **papszOpenOptions = nullptr);
    ~OGRS57DataSource();

    void SetOptionList(char **);
    const char *GetOption(const char *);

    int Open(const char *pszName);
    int Create(const char *pszName, char **papszOptions);

    int GetLayerCount() override
    {
        return nLayers;
    }

    OGRLayer *GetLayer(int) override;
    void AddLayer(OGRS57Layer *);
    int TestCapability(const char *) override;

    OGRSpatialReference *DSGetSpatialRef()
    {
        return poSpatialRef;
    }

    int GetModuleCount()
    {
        return nModules;
    }

    S57Reader *GetModule(int);

    S57Writer *GetWriter()
    {
        return poWriter;
    }

    OGRErr GetDSExtent(OGREnvelope *psExtent, int bForce = TRUE);
};

/************************************************************************/
/*                            OGRS57Driver                              */
/************************************************************************/

class OGRS57Driver final : public GDALDriver
{
    static S57ClassRegistrar *poRegistrar;

  public:
    OGRS57Driver();
    ~OGRS57Driver();

    static GDALDataset *Open(GDALOpenInfo *poOpenInfo);
    static GDALDataset *Create(const char *pszName, int nBands, int nXSize,
                               int nYSize, GDALDataType eDT,
                               char **papszOptions);

    static S57ClassRegistrar *GetS57Registrar();
};

#endif /* ndef OGR_S57_H_INCLUDED */
