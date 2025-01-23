/******************************************************************************
 *
 * Project:  Interlis 1 Translator
 * Purpose:  Implements OGRILI1DataSource class.
 * Author:   Pirmin Kalberer, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG
 * Copyright (c) 2007-2008, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_conv.h"
#include "cpl_string.h"

#include "ili1reader.h"
#include "ogr_ili1.h"

#include <string>

/************************************************************************/
/*                         OGRILI1DataSource()                         */
/************************************************************************/

OGRILI1DataSource::OGRILI1DataSource()
    : poImdReader(new ImdReader(1)), poReader(nullptr), nLayers(0),
      papoLayers(nullptr)
{
}

/************************************************************************/
/*                        ~OGRILI1DataSource()                         */
/************************************************************************/

OGRILI1DataSource::~OGRILI1DataSource()

{
    for (int i = 0; i < nLayers; i++)
    {
        delete papoLayers[i];
    }
    CPLFree(papoLayers);

    DestroyILI1Reader(poReader);
    delete poImdReader;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRILI1DataSource::Open(const char *pszNewName, char **papszOpenOptionsIn,
                            int bTestOpen)

{
    if (strlen(pszNewName) == 0)
    {
        return FALSE;
    }

    std::string osBasename;
    std::string osModelFilename;
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

    /* -------------------------------------------------------------------- */
    /*      Open the source file.                                           */
    /* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL(osBasename.c_str(), "r");
    if (fp == nullptr)
    {
        if (!bTestOpen)
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Failed to open ILI1 file `%s'.", pszNewName);

        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      If we aren't sure it is ILI1, load a header chunk and check      */
    /*      for signs it is ILI1                                             */
    /* -------------------------------------------------------------------- */
    char szHeader[1000];

    if (bTestOpen)
    {
        int nLen = (int)VSIFReadL(szHeader, 1, sizeof(szHeader), fp);
        if (nLen == sizeof(szHeader))
            szHeader[sizeof(szHeader) - 1] = '\0';
        else
            szHeader[nLen] = '\0';

        if (strstr(szHeader, "SCNT") == nullptr)
        {
            VSIFCloseL(fp);
            return FALSE;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      We assume now that it is ILI1.  Close and instantiate a          */
    /*      ILI1Reader on it.                                                */
    /* -------------------------------------------------------------------- */
    VSIFCloseL(fp);

    poReader = CreateILI1Reader();
    if (poReader == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "File %s appears to be ILI1 but the ILI1 reader cannot\n"
                 "be instantiated, likely because Xerces support was not\n"
                 "configured in.",
                 pszNewName);
        return FALSE;
    }

    poReader->OpenFile(osBasename.c_str());

    if (osModelFilename.length() > 0)
        poReader->ReadModel(poImdReader, osModelFilename.c_str(), this);

    CPLConfigOptionSetter oSetter("OGR_ARC_STEPSIZE", "0.96",
                                  /* bSetOnlyIfUndefined = */ true);

    // Parse model and read data - without surface join and area polygonizing.
    poReader->ReadFeatures();

    return TRUE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRILI1DataSource::TestCapability(const char *pszCap)

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

OGRLayer *OGRILI1DataSource::GetLayer(int iLayer)
{
    if (!poReader)
    {
        if (iLayer < 0 || iLayer >= nLayers)
            return nullptr;
        return papoLayers[iLayer];
    }
    return poReader->GetLayer(iLayer);
}

/************************************************************************/
/*                              GetLayerByName()                              */
/************************************************************************/

OGRILI1Layer *OGRILI1DataSource::GetLayerByName(const char *pszLayerName)
{
    if (!poReader)
    {
        return cpl::down_cast<OGRILI1Layer *>(
            GDALDataset::GetLayerByName(pszLayerName));
    }

    return cpl::down_cast<OGRILI1Layer *>(
        poReader->GetLayerByName(pszLayerName));
}
