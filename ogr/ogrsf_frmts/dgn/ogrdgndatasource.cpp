/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam (warmerdam@pobox.com)
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_dgn.h"
#include "cpl_conv.h"
#include "cpl_string.h"

#ifdef EMBED_RESOURCE_FILES
#include "embedded_resources.h"
#endif

/************************************************************************/
/*                         OGRDGNDataSource()                           */
/************************************************************************/

OGRDGNDataSource::OGRDGNDataSource() = default;

/************************************************************************/
/*                        ~OGRDGNDataSource()                           */
/************************************************************************/

OGRDGNDataSource::~OGRDGNDataSource()

{
    for (int i = 0; i < nLayers; i++)
        delete papoLayers[i];

    CPLFree(papoLayers);
    CSLDestroy(papszOptions);

    if (hDGN != nullptr)
        DGNClose(hDGN);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

bool OGRDGNDataSource::Open(GDALOpenInfo *poOpenInfo)

{
    m_osEncoding =
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "ENCODING", "");

    CPLAssert(nLayers == 0);

    /* -------------------------------------------------------------------- */
    /*      Try to open the file as a DGN file.                             */
    /* -------------------------------------------------------------------- */
    const bool bUpdate = (poOpenInfo->eAccess == GA_Update);
    hDGN = DGNOpen(poOpenInfo->pszFilename, bUpdate);
    if (hDGN == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to open %s as a Microstation .dgn file.",
                 poOpenInfo->pszFilename);
        return false;
    }

    /* -------------------------------------------------------------------- */
    /*      Create the layer object.                                        */
    /* -------------------------------------------------------------------- */
    OGRDGNLayer *poLayer = new OGRDGNLayer(this, "elements", hDGN, bUpdate);

    /* -------------------------------------------------------------------- */
    /*      Add layer to data source layer list.                            */
    /* -------------------------------------------------------------------- */
    papoLayers = static_cast<OGRDGNLayer **>(
        CPLRealloc(papoLayers, sizeof(OGRDGNLayer *) * (nLayers + 1)));
    papoLayers[nLayers++] = poLayer;

    return true;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDGNDataSource::TestCapability(const char *pszCap)

{
    if (EQUAL(pszCap, ODsCCreateLayer))
        return TRUE;
    else if (EQUAL(pszCap, ODsCZGeometries))
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRDGNDataSource::GetLayer(int iLayer)

{
    if (iLayer < 0 || iLayer >= nLayers)
        return nullptr;

    return papoLayers[iLayer];
}

/************************************************************************/
/*                             PreCreate()                              */
/*                                                                      */
/*      Called by OGRDGNDriver::Create() method to setup a stub         */
/*      OGRDataSource object without the associated file created        */
/*      yet.  It will be created by the ICreateLayer() call.            */
/************************************************************************/

void OGRDGNDataSource::PreCreate(CSLConstList papszOptionsIn)

{
    papszOptions = CSLDuplicate(papszOptionsIn);

    m_osEncoding = CSLFetchNameValueDef(papszOptionsIn, "ENCODING", "");
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRDGNDataSource::ICreateLayer(const char *pszLayerName,
                               const OGRGeomFieldDefn *poGeomFieldDefn,
                               CSLConstList papszExtraOptions)

{
    /* -------------------------------------------------------------------- */
    /*      Ensure only one layer gets created.                             */
    /* -------------------------------------------------------------------- */
    if (nLayers > 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "DGN driver only supports one layer with all the elements "
                 "in it.");
        return nullptr;
    }

    const auto eGeomType =
        poGeomFieldDefn ? poGeomFieldDefn->GetType() : wkbNone;
    const auto poSRS =
        poGeomFieldDefn ? poGeomFieldDefn->GetSpatialRef() : nullptr;

    /* -------------------------------------------------------------------- */
    /*      If the coordinate system is geographic, we should use a         */
    /*      localized default origin and resolution.                        */
    /* -------------------------------------------------------------------- */
    const char *pszMasterUnit = "m";
    const char *pszSubUnit = "cm";

    int nUORPerSU = 1;
    int nSUPerMU = 100;

    double dfOriginX = -21474836.0;  // Default origin centered on zero
    double dfOriginY = -21474836.0;  // with two decimals of precision.
    double dfOriginZ = -21474836.0;

    if (poSRS != nullptr && poSRS->IsGeographic())
    {
        dfOriginX = -200.0;
        dfOriginY = -200.0;

        pszMasterUnit = "d";
        pszSubUnit = "s";
        nSUPerMU = 3600;
        nUORPerSU = 1000;
    }

    /* -------------------------------------------------------------------- */
    /*      Parse out various creation options.                             */
    /* -------------------------------------------------------------------- */
    papszOptions = CSLInsertStrings(papszOptions, 0, papszExtraOptions);

    const bool b3DRequested =
        CPLFetchBool(papszOptions, "3D", wkbHasZ(eGeomType));

    const char *pszRequestSeed = CSLFetchNameValue(papszOptions, "SEED");
    const char *pszSeed = pszRequestSeed;
    int nCreationFlags = 0;
#ifdef EMBED_RESOURCE_FILES
    std::string osTmpSeedFilename;
#endif
    if (pszSeed)
        nCreationFlags |= DGNCF_USE_SEED_ORIGIN | DGNCF_USE_SEED_UNITS;
    else
    {
        pszRequestSeed = b3DRequested ? "seed_3d.dgn" : "seed_2d.dgn";
#ifdef EMBED_RESOURCE_FILES
        CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);
#endif
        pszSeed = CPLFindFile("gdal", pszRequestSeed);
#ifdef EMBED_RESOURCE_FILES
        if (!pszSeed)
        {
            if (b3DRequested)
            {
                static const bool bOnce [[maybe_unused]] = []()
                {
                    CPLDebug("DGN", "Using embedded seed_3d");
                    return true;
                }();
            }
            else
            {
                static const bool bOnce [[maybe_unused]] = []()
                {
                    CPLDebug("DGN", "Using embedded seed_2d");
                    return true;
                }();
            }
            unsigned nSize = 0;
            const unsigned char *pabyData =
                b3DRequested ? DGNGetSeed3D(&nSize) : DGNGetSeed2D(&nSize);
            osTmpSeedFilename = VSIMemGenerateHiddenFilename(pszRequestSeed);
            pszSeed = osTmpSeedFilename.c_str();
            VSIFCloseL(VSIFileFromMemBuffer(osTmpSeedFilename.c_str(),
                                            const_cast<GByte *>(pabyData),
                                            static_cast<int>(nSize),
                                            /* bTakeOwnership = */ false));
        }
#endif
    }

    if (pszSeed == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "No seed file provided, and unable to find %s.",
                 pszRequestSeed);
        return nullptr;
    }

    if (CPLFetchBool(papszOptions, "COPY_WHOLE_SEED_FILE", true))
        nCreationFlags |= DGNCF_COPY_WHOLE_SEED_FILE;
    if (CPLFetchBool(papszOptions, "COPY_SEED_FILE_COLOR_TABLE", true))
        nCreationFlags |= DGNCF_COPY_SEED_FILE_COLOR_TABLE;

    const char *pszValue = CSLFetchNameValue(papszOptions, "MASTER_UNIT_NAME");
    if (pszValue != nullptr)
    {
        nCreationFlags &= ~DGNCF_USE_SEED_UNITS;
        pszMasterUnit = pszValue;
    }

    pszValue = CSLFetchNameValue(papszOptions, "SUB_UNIT_NAME");
    if (pszValue != nullptr)
    {
        nCreationFlags &= ~DGNCF_USE_SEED_UNITS;
        pszSubUnit = pszValue;
    }

    pszValue = CSLFetchNameValue(papszOptions, "SUB_UNITS_PER_MASTER_UNIT");
    if (pszValue != nullptr)
    {
        nCreationFlags &= ~DGNCF_USE_SEED_UNITS;
        nSUPerMU = atoi(pszValue);
    }

    pszValue = CSLFetchNameValue(papszOptions, "UOR_PER_SUB_UNIT");
    if (pszValue != nullptr)
    {
        nCreationFlags &= ~DGNCF_USE_SEED_UNITS;
        nUORPerSU = atoi(pszValue);
    }

    pszValue = CSLFetchNameValue(papszOptions, "ORIGIN");
    if (pszValue != nullptr)
    {
        char **papszTuple =
            CSLTokenizeStringComplex(pszValue, " ,", FALSE, FALSE);

        nCreationFlags &= ~DGNCF_USE_SEED_ORIGIN;
        if (CSLCount(papszTuple) == 3)
        {
            dfOriginX = CPLAtof(papszTuple[0]);
            dfOriginY = CPLAtof(papszTuple[1]);
            dfOriginZ = CPLAtof(papszTuple[2]);
        }
        else if (CSLCount(papszTuple) == 2)
        {
            dfOriginX = CPLAtof(papszTuple[0]);
            dfOriginY = CPLAtof(papszTuple[1]);
            dfOriginZ = 0.0;
        }
        else
        {
            CSLDestroy(papszTuple);
            CPLError(CE_Failure, CPLE_AppDefined,
                     "ORIGIN is not a valid 2d or 3d tuple.\n"
                     "Separate tuple values with comma.");
#ifdef EMBED_RESOURCE_FILES
            if (!osTmpSeedFilename.empty())
                VSIUnlink(osTmpSeedFilename.c_str());
#endif
            return nullptr;
        }
        CSLDestroy(papszTuple);
    }

    /* -------------------------------------------------------------------- */
    /*      Try creating the base file.                                     */
    /* -------------------------------------------------------------------- */
    hDGN = DGNCreate(GetDescription(), pszSeed, nCreationFlags, dfOriginX,
                     dfOriginY, dfOriginZ, nSUPerMU, nUORPerSU, pszMasterUnit,
                     pszSubUnit);
#ifdef EMBED_RESOURCE_FILES
    if (!osTmpSeedFilename.empty())
        VSIUnlink(osTmpSeedFilename.c_str());
#endif

    if (hDGN == nullptr)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Create the layer object.                                        */
    /* -------------------------------------------------------------------- */
    OGRDGNLayer *poLayer = new OGRDGNLayer(this, pszLayerName, hDGN, TRUE);

    /* -------------------------------------------------------------------- */
    /*      Add layer to data source layer list.                            */
    /* -------------------------------------------------------------------- */
    papoLayers = static_cast<OGRDGNLayer **>(
        CPLRealloc(papoLayers, sizeof(OGRDGNLayer *) * (nLayers + 1)));
    papoLayers[nLayers++] = poLayer;

    return poLayer;
}
