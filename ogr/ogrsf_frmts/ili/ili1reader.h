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

#ifndef CPL_ILI1READER_H_INCLUDED
#define CPL_ILI1READER_H_INCLUDED

#include "imdreader.h"

class OGRILI1DataSource;

class IILI1Reader
{
  public:
    virtual ~IILI1Reader();

    virtual int OpenFile(const char *pszFilename) = 0;

    virtual int ReadModel(ImdReader *poImdReader, const char *pszModelFilename,
                          OGRILI1DataSource *poDS) = 0;
    virtual int ReadFeatures() = 0;

    virtual OGRLayer *GetLayer(int) = 0;
    virtual OGRLayer *GetLayerByName(const char *) = 0;
    virtual int GetLayerCount() = 0;
};

IILI1Reader *CreateILI1Reader();
void DestroyILI1Reader(IILI1Reader *reader);

#endif
