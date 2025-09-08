/******************************************************************************
 *
 * Name:     georaster_dataset.cpp
 * Project:  Oracle Spatial GeoRaster Driver
 * Purpose:  Implement GeoRasterDataset Methods
 * Author:   Fengting Chen [fengting.chen at oracle.com]
 *
 ******************************************************************************
 * Copyright (c) 2025, Fengting Chen <fengtign dot chen at oracle dot com>
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#include "georaster_priv.h"

GeoRasterDriver *GeoRasterDriver::gpoGeoRasterDriver = nullptr;

/************************
 * \brief Constructor
 ************************/
GeoRasterDriver::GeoRasterDriver()
{
    gpoGeoRasterDriver = this;
}

/************************
 * \brief Destructor
 ************************/
GeoRasterDriver::~GeoRasterDriver()
{
    gpoGeoRasterDriver = nullptr;
    std::map<CPLString, OWSessionPool *>::iterator oIter =
        oMapSessionPool.begin();
    for (; oIter != oMapSessionPool.end(); ++oIter)
        delete (oIter->second);
}

/***************************************************************************
 * \brief Create a OCI Session Pool and store it in a list
 *
 * All OCI Session Pool will be destroyed when the GeoRasterDriver
 * is destroyed.
 *
 ***************************************************************************/
OWConnection *
GeoRasterDriver::GetConnection(const char *pszUserIn, const char *pszPasswordIn,
                               const char *pszServerIn, int nPoolSessionMinIn,
                               int nPoolSessionMaxIn, int nPoolSessionIncrIn)
{
    OWSessionPool *poPool = nullptr;

    CPLString osKey = pszUserIn;
    osKey += "/";
    osKey += pszPasswordIn;
    osKey += "@";
    osKey += pszServerIn;

    bool bConfigPool = false;
    if (nPoolSessionMinIn >= 0 || nPoolSessionMaxIn > 0 ||
        nPoolSessionIncrIn > 0)
        bConfigPool = true;

    CPLDebug("GEOR", "Getting connection from the Session pool with key %s ",
             osKey.c_str());

    /**
     * Look for an existing session pool in the map
     **/
    std::lock_guard<std::mutex> oLock(oMutex);
    std::map<CPLString, OWSessionPool *>::iterator oIter =
        oMapSessionPool.find(osKey);

    if (oIter != oMapSessionPool.end())
    {
        poPool = oIter->second;
        if (bConfigPool)
        {
            ub4 nSessMin = (nPoolSessionMinIn >= 0) ? (ub4)nPoolSessionMinIn
                                                    : poPool->GetSessMin();
            ub4 nSessMax = (nPoolSessionMaxIn > 0) ? (ub4)nPoolSessionMaxIn
                                                   : poPool->GetSessMax();
            ub4 nSessIncr = (nPoolSessionIncrIn > 0) ? (ub4)nPoolSessionIncrIn
                                                     : poPool->GetSessIncr();

            poPool->ReInitialize(nSessMin, nSessMax, nSessIncr);
        }

        return poPool->GetConnection(pszUserIn, pszPasswordIn, pszServerIn);
    }

    /**
     * There's no existing connection. Create a new one.
     **/
    poPool = new OWSessionPool(pszUserIn, pszPasswordIn, pszServerIn);
    if (poPool == nullptr || !poPool->Succeeded())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Failed to create session pool.");
        return nullptr;
    }

    if (bConfigPool)
    {
        ub4 nSessMin = (nPoolSessionMinIn >= 0) ? (ub4)nPoolSessionMinIn
                                                : SDO_SPOOL_DEFAULT_SESSMIN;
        ub4 nSessMax = (nPoolSessionMaxIn > 0) ? (ub4)nPoolSessionMaxIn
                                               : SDO_SPOOL_DEFAULT_SESSMAX;
        ub4 nSessIncr = (nPoolSessionIncrIn > 0) ? (ub4)nPoolSessionIncrIn
                                                 : SDO_SPOOL_DEFAULT_SESSINCR;

        poPool->ReInitialize(nSessMin, nSessMax, nSessIncr);
    }

    /**
     * Save connection in the connection map.
     **/
    oMapSessionPool[osKey] = poPool;
    return poPool->GetConnection(pszUserIn, pszPasswordIn, pszServerIn);
}

/* end of file georasterdriver.cpp */
