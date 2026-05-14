/******************************************************************************
 *
 * Project:  Interlis 2 Translator
 * Purpose:  Implements OGRILI2DataSource class.
 * Author:   Markus Schnider, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG
 * Copyright (c) 2007-2008, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_conv.h"
#include "cpl_string.h"

#include "ili2reader.h"

#include "ogr_ili2.h"

using namespace std;

/************************************************************************/
/*                         OGRILI2DataSource()                          */
/************************************************************************/

OGRILI2DataSource::OGRILI2DataSource()
    : pszName(nullptr), poImdReader(new ImdReader(2)), poReader(nullptr),
      nLayers(0), papoLayers(nullptr)
{
}

/************************************************************************/
/*                         ~OGRILI2DataSource()                         */
/************************************************************************/

OGRILI2DataSource::~OGRILI2DataSource()

{
    for (int i = 0; i < nLayers; i++)
    {
        delete papoLayers[i];
    }
    CPLFree(papoLayers);

    DestroyILI2Reader(poReader);
    delete poImdReader;
    CPLFree(pszName);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRILI2DataSource::Open(const char *pszNewName, char **papszOpenOptionsIn,
                            int bTestOpen)

{
    CPLString osBasename;
    CPLString osModelFilename;

    if (CSLFetchNameValue(papszOpenOptionsIn, "MODEL") != nullptr)
    {
        osBasename = pszNewName;
        osModelFilename = CSLFetchNameValue(papszOpenOptionsIn, "MODEL");
    }
    else
    {
        char **filenames = CSLTokenizeString2(pszNewName, ",", 0);
        int nCount = CSLCount(filenames);
        if (nCount == 0)
        {
            CSLDestroy(filenames);
            return FALSE;
        }
        osBasename = filenames[0];

        if (nCount > 1)
            osModelFilename = filenames[1];

        CSLDestroy(filenames);
    }

    pszName = CPLStrdup(osBasename);

    /* -------------------------------------------------------------------- */
    /*      Open the source file.                                           */
    /* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL(pszName, "r");
    if (fp == nullptr)
    {
        if (!bTestOpen)
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Failed to open ILI2 file `%s'.", pszNewName);

        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      If we aren't sure it is ILI2, load a header chunk and check     */
    /*      for signs it is ILI2                                            */
    /* -------------------------------------------------------------------- */
    char szHeader[1000];
    if (bTestOpen)
    {
        int nLen =
            static_cast<int>(VSIFReadL(szHeader, 1, sizeof(szHeader), fp));
        if (nLen == sizeof(szHeader))
            szHeader[sizeof(szHeader) - 1] = '\0';
        else
            szHeader[nLen] = '\0';

        if (szHeader[0] != '<' ||
            (strstr(szHeader, "interlis.ch/INTERLIS2") == nullptr &&
             strstr(szHeader, "//www.interlis.ch/xtf/") == nullptr))
        {
            // "www.interlis.ch/INTERLIS2.3"
            VSIFCloseL(fp);
            return FALSE;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      We assume now that it is ILI2.  Close and instantiate a         */
    /*      ILI2Reader on it.                                               */
    /* -------------------------------------------------------------------- */
    VSIFCloseL(fp);

    poReader = CreateILI2Reader();
    if (poReader == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "File %s appears to be ILI2 but the ILI2 reader cannot\n"
                 "be instantiated, likely because Xerces support was not\n"
                 "configured in.",
                 pszNewName);
        return FALSE;
    }

    if (!osModelFilename.empty())
        poReader->ReadModel(this, poImdReader, osModelFilename);

    poReader->SetSourceFile(pszName);

    poReader->SaveClasses(pszName);

    for (auto &poLayer : poReader->GetLayers())
        poLayer->ResetReading();

    return TRUE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRILI2DataSource::TestCapability(const char *pszCap) const

{
    if (EQUAL(pszCap, ODsCCurveGeometries))
        return TRUE;
    else if (EQUAL(pszCap, ODsCZGeometries))
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

const OGRLayer *OGRILI2DataSource::GetLayer(int iLayer) const

{
    int i = 0;
    for (auto &poLayer : poReader->GetLayers())
    {
        if (i == iLayer)
        {
            return poLayer.get();
        }
        ++i;
    }
    return nullptr;
}
