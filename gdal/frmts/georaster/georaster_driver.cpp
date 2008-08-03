/******************************************************************************
 * $Id: $
 *
 * Name:     georaster_driver.cpp
 * Project:  Oracle Spatial GeoRaster Driver
 * Purpose:  Implement GeoRasterDriver Methods
 * Author:   Ivan Lucena [ivan.lucena@pmldnet.com]
 *
 ******************************************************************************
 * Copyright (c) 2008, Ivan Lucena
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files ( the "Software" ),
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
 *****************************************************************************/

#include "georaster_priv.h"

//  ---------------------------------------------------------------------------
//                                                            GeoRasterDriver()
//  ---------------------------------------------------------------------------

GeoRasterDriver::GeoRasterDriver()
{
    papoConnection = NULL;
    nRefCount = 0;
}

//  ---------------------------------------------------------------------------
//                                                              GetConnection()
//  ---------------------------------------------------------------------------

/**
 * \brief Create a OWConnection object and store it in a list
 *
 * The georaster driver keeps the connection with the Oracle database
 * server for as long it leaves. Consequent GeoRasterDataset instance 
 * can re-use the existing conncetion as long it used the same 
 * database, same user name and password. 
 *
 * If a requested connection does not exist on the list yet than a new
 * OWConnection will be instanciated, added to the list and returned to
 * the caller.
 *
 * All connection will be disconnect when the GeoRasterDriver is destroyed.
 *
 * Licenses issues must be concern of the application developer. it might
 * be necessary to destroy the driver when a new database is selected and the
 * user doesn't have license for more than one connection at the same time.
 ****/

OWConnection* GeoRasterDriver::GetConnection( const char* pszUserIn,
                                              const char* pszPasswordIn,
                                              const char* pszServerIn )
{
    //  --------------------------------------------------------------------
    //  Look for an existing connection on the list
    //  --------------------------------------------------------------------

    int i = 0;

    for( i = 0; i < nRefCount; i++ )
    {
        if( EQUAL( pszUserIn,     papoConnection[i]->GetUser() ) &&
            EQUAL( pszPasswordIn, papoConnection[i]->GetPassword() ) &&
            EQUAL( pszServerIn,   papoConnection[i]->GetServer() ) )
        {
            return papoConnection[i];
        }
    }

    //  --------------------------------------------------------------------
    //  Create a new connection
    //  --------------------------------------------------------------------

    OWConnection* poConnection = new OWConnection( 
        pszUserIn, 
        pszPasswordIn, 
        pszServerIn );

    //  --------------------------------------------------------------------
    //  Save into connection list
    //  --------------------------------------------------------------------

    if( poConnection )
    {
        nRefCount++;
        papoConnection = (OWConnection**) CPLRealloc( papoConnection, 
                           sizeof(OWConnection*) * nRefCount);
        papoConnection[nRefCount - 1] = poConnection;
    }

    return poConnection;
}

//  ---------------------------------------------------------------------------
//                                                           ~GeoRasterDriver()
//  ---------------------------------------------------------------------------

GeoRasterDriver::~GeoRasterDriver()
{
    //  --------------------------------------------------------------------
    //  Destroy existents connections
    //  --------------------------------------------------------------------

    int i;

    for( i = 0; i < nRefCount; i++ )
    {
        delete (OWConnection*) papoConnection[i];
    }

    //  --------------------------------------------------------------------
    //  Destroy the connection list
    //  --------------------------------------------------------------------

    CPLFree( papoConnection );
}
