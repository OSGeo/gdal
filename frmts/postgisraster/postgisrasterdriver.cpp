/******************************************************************************
 * File :    PostGISRasterDriver.cpp
 * Project:  PostGIS Raster driver
 * Purpose:  Implements PostGIS Raster driver class methods
 * Author:   Jorge Arevalo, jorge.arevalo@deimos-space.com
 *
 *
 ******************************************************************************
 * Copyright (c) 2010, Jorge Arevalo, jorge.arevalo@deimos-space.com
 * Copyright (c) 2013, Even Rouault
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include "postgisraster.h"
#include "cpl_multiproc.h"

PostGISRasterDriver *PostGISRasterDriver::gpoPostGISRasterDriver = nullptr;

/************************
 * \brief Constructor
 ************************/
PostGISRasterDriver::PostGISRasterDriver()
{
    gpoPostGISRasterDriver = this;
}

/************************
 * \brief Destructor
 ************************/
PostGISRasterDriver::~PostGISRasterDriver()
{
    gpoPostGISRasterDriver = nullptr;
    if (hMutex != nullptr)
        CPLDestroyMutex(hMutex);
    std::map<CPLString, PGconn *>::iterator oIter = oMapConnection.begin();
    for (; oIter != oMapConnection.end(); ++oIter)
        PQfinish(oIter->second);
}

/***************************************************************************
 * \brief Create a PQconn object and store it in a list
 *
 * The PostGIS Raster driver keeps the connection with the PostgreSQL database
 * server for as long it leaves. Following PostGISRasterDataset instance
 * can re-use the existing connection as long it used the same database,
 * same host, port and user name.
 *
 * The PostGIS Raster driver will keep a list of all the successful
 * connections so, when connection is requested and it does not exist
 * on the list a new one will be instantiated, added to the list and
 * returned to the caller.
 *
 * All connection will be destroyed when the PostGISRasterDriver is destroyed.
 *
 ***************************************************************************/
PGconn *PostGISRasterDriver::GetConnection(const char *pszConnectionString,
                                           const char *pszServiceIn,
                                           const char *pszDbnameIn,
                                           const char *pszHostIn,
                                           const char *pszPortIn,
                                           const char *pszUserIn)
{
    PGconn *poConn = nullptr;

    if (pszHostIn == nullptr)
        pszHostIn = "(null)";
    if (pszPortIn == nullptr)
        pszPortIn = "(null)";
    if (pszUserIn == nullptr)
        pszUserIn = "(null)";
    CPLString osKey = (pszServiceIn == nullptr) ? pszDbnameIn : pszServiceIn;
    osKey += "-";
    osKey += pszHostIn;
    osKey += "-";
    osKey += pszPortIn;
    osKey += "-";
    osKey += pszUserIn;
    osKey += "-";
    osKey += CPLSPrintf(CPL_FRMT_GIB, CPLGetPID());

    /**
     * Look for an existing connection in the map
     **/
    CPLMutexHolderD(&hMutex);
    std::map<CPLString, PGconn *>::iterator oIter = oMapConnection.find(osKey);
    if (oIter != oMapConnection.end())
        return oIter->second;

    /**
     * There's no existing connection. Create a new one.
     **/
    poConn = PQconnectdb(pszConnectionString);
    if (poConn == nullptr || PQstatus(poConn) == CONNECTION_BAD)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "PQconnectdb failed: %s\n",
                 PQerrorMessage(poConn));
        PQfinish(poConn);
        return nullptr;
    }

    /**
     * Save connection in the connection map.
     **/
    oMapConnection[osKey] = poConn;
    return poConn;
}
