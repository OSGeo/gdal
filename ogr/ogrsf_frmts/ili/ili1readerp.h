/******************************************************************************
 *
 * Project:  Interlis 1 Reader
 * Purpose:  Private Declarations for Reader code.
 * Author:   Pirmin Kalberer, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_ILI1READERP_H_INCLUDED
#define CPL_ILI1READERP_H_INCLUDED

#include "ili1reader.h"
#include "ogr_ili1.h"

class ILI1Reader;
class OGRILI1Layer;

/************************************************************************/
/*                              ILI1Reader                              */
/************************************************************************/

class ILI1Reader : public IILI1Reader
{
  private:
    VSILFILE *fpItf;
    int nLayers;
    OGRILI1Layer **papoLayers;
    OGRILI1Layer *curLayer;
    char codeBlank;
    char codeUndefined;
    char codeContinue;

    ILI1Reader(ILI1Reader &) = delete;
    ILI1Reader &operator=(const ILI1Reader &) = delete;
    ILI1Reader(ILI1Reader &&) = delete;
    ILI1Reader &operator=(ILI1Reader &&) = delete;

  public:
    ILI1Reader();
    ~ILI1Reader();

    int OpenFile(const char *pszFilename) override;
    int ReadModel(ImdReader *poImdReader, const char *pszModelFilename,
                  OGRILI1DataSource *poDS) override;
    int ReadFeatures() override;
    int ReadTable(const char *layername);
    void ReadGeom(char **stgeom, int geomIdx, OGRwkbGeometryType eType,
                  OGRFeature *feature);
    char **ReadParseLine();

    void AddLayer(OGRILI1Layer *poNewLayer);
    OGRILI1Layer *GetLayer(int) override;
    OGRILI1Layer *GetLayerByName(const char *) override;
    int GetLayerCount() override;

    static const char *GetLayerNameString(const char *topicname,
                                          const char *tablename);
};

#endif
