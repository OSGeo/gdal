/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for OGR/GPSBabel driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_GPSBABEL_H_INCLUDED
#define OGR_GPSBABEL_H_INCLUDED

#include "ogrsf_frmts.h"
#include "cpl_string.h"
#include <array>

/************************************************************************/
/*                        OGRGPSBabelDataSource                         */
/************************************************************************/

class OGRGPSBabelDataSource final : public GDALDataset
{
    int nLayers = 0;
    std::array<OGRLayer *, 5> apoLayers{
        {nullptr, nullptr, nullptr, nullptr, nullptr}};
    char *pszGPSBabelDriverName = nullptr;
    char *pszFilename = nullptr;
    CPLString osTmpFileName{};
    GDALDataset *poGPXDS = nullptr;

  public:
    OGRGPSBabelDataSource();
    virtual ~OGRGPSBabelDataSource();

    virtual int CloseDependentDatasets() override;

    virtual int GetLayerCount() override
    {
        return nLayers;
    }

    virtual OGRLayer *GetLayer(int) override;

    int Open(const char *pszFilename, const char *pszGPSBabelDriverNameIn,
             char **papszOpenOptions);

    static bool IsSpecialFile(const char *pszFilename);
    static bool IsValidDriverName(const char *pszGPSBabelDriverName);
};

/************************************************************************/
/*                   OGRGPSBabelWriteDataSource                         */
/************************************************************************/

class OGRGPSBabelWriteDataSource final : public GDALDataset
{
    char *pszGPSBabelDriverName;
    char *pszFilename;
    CPLString osTmpFileName;
    GDALDataset *poGPXDS;

    bool Convert();

  public:
    OGRGPSBabelWriteDataSource();
    virtual ~OGRGPSBabelWriteDataSource();

    virtual int GetLayerCount() override;
    virtual OGRLayer *GetLayer(int) override;

    virtual int TestCapability(const char *) override;

    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;

    int Create(const char *pszFilename, char **papszOptions);
};

#endif /* ndef OGR_GPSBABEL_H_INCLUDED */
