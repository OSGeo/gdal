/******************************************************************************
 *
 * Project:  Interlis 2 Reader
 * Purpose:  Public Declarations for Reader code.
 * Author:   Markus Schnider, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_ILI2READER_H_INCLUDED
#define CPL_ILI2READER_H_INCLUDED

#include "imdreader.h"
#include <list>

class OGRILI2DataSource;

class IILI2Reader
{
  public:
    virtual ~IILI2Reader();

    virtual void SetSourceFile(const char *pszFilename) = 0;

    virtual int ReadModel(OGRILI2DataSource *poDS, ImdReader *poImdReader,
                          const char *modelFilename) = 0;
    virtual int SaveClasses(const char *pszFilename) = 0;

    virtual std::list<OGRLayer *> GetLayers() = 0;
    virtual int GetLayerCount() = 0;
};

IILI2Reader *CreateILI2Reader();
void DestroyILI2Reader(IILI2Reader *reader);

#endif
