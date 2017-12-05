/******************************************************************************
 * File :    PostGISRasterDriver.cpp
 * Project:  PostGIS Raster driver
 * Purpose:  Implements PostGIS Raster driver class methods
 * Author:   Jorge Arevalo, jorge.arevalo@deimos-space.com
 *
 * Last changes: $Id$
 *
 ******************************************************************************
 * Copyright (c) 2010, Jorge Arevalo, jorge.arevalo@deimos-space.com
 * Copyright (c) 2013, Even Rouault
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************/
#include "postgisraster.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$")

/************************
 * \brief Constructor
 ************************/
PostGISRasterDriver::PostGISRasterDriver() :
    hMutex(NULL)
{}

/************************
 * \brief Destructor
 ************************/
PostGISRasterDriver::~PostGISRasterDriver() {

    if( hMutex != NULL )
        CPLDestroyMutex(hMutex);
    std::map<CPLString, PGconn*>::iterator oIter = oMapConnection.begin();
    for(; oIter != oMapConnection.end(); ++oIter )
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
PGconn* PostGISRasterDriver::GetConnection(const char* pszConnectionString,
        const char * pszDbnameIn, const char * pszHostIn, const char * pszPortIn, const char * pszUserIn)
{
    PGconn * poConn = NULL;

    if( pszHostIn == NULL ) pszHostIn = "(null)";
    if( pszPortIn == NULL ) pszPortIn = "(null)";
    if( pszUserIn == NULL ) pszUserIn = "(null)";
    CPLString osKey = pszDbnameIn;
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
    std::map<CPLString, PGconn*>::iterator oIter = oMapConnection.find(osKey);
    if( oIter != oMapConnection.end() )
        return oIter->second;

    /**
     * There's no existing connection. Create a new one.
     **/
    poConn = PQconnectdb(pszConnectionString);
    if (poConn == NULL ||
            PQstatus(poConn) == CONNECTION_BAD) {
        CPLError(CE_Failure, CPLE_AppDefined, "PQconnectdb failed: %s\n",
                PQerrorMessage(poConn));
        PQfinish(poConn);
        return NULL;
    }

    /**
     * Save connection in the connection map.
     **/
    oMapConnection[osKey] = poConn;
    return poConn;
}
