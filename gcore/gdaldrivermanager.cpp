/******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
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
 ******************************************************************************
 *
 * gdaldrivermanager.cpp
 *
 * The GDALDriverManager class from gdal_priv.h.
 * 
 * $Log$
 * Revision 1.3  1999/10/21 13:24:08  warmerda
 * Added documentation and C callable functions.
 *
 * Revision 1.2  1998/12/31 18:53:33  warmerda
 * Add GDALGetDriverByName
 *
 * Revision 1.1  1998/12/03 18:32:01  warmerda
 * New
 *
 */

#include "gdal_priv.h"

/************************************************************************/
/* ==================================================================== */
/*                           GDALDriverManager                          */
/* ==================================================================== */
/************************************************************************/

static GDALDriverManager	*poDM = NULL;

/************************************************************************/
/*                        GetGDALDriverManager()                        */
/*                                                                      */
/*      A freestanding function to get the only instance of the         */
/*      GDALDriverManager.                                              */
/************************************************************************/

/**
 * Fetch the global GDAL driver manager.
 *
 * This function fetches the pointer to the singleton global driver manager.
 * If the driver manager doesn't exist it is automatically created.
 *
 * @return pointer to the global driver manager.  This should not be able
 * to fail.
 */

GDALDriverManager *GetGDALDriverManager()

{
    if( poDM == NULL )
        new GDALDriverManager();

    return( poDM );
}

/************************************************************************/
/*                         GDALDriverManager()                          */
/************************************************************************/

GDALDriverManager::GDALDriverManager()

{
    nDrivers = 0;
    papoDrivers = NULL;

    CPLAssert( poDM == NULL );
    poDM = this;
}

/************************************************************************/
/*                         ~GDALDriverManager()                         */
/*                                                                      */
/*      Eventually this should also likely clean up all open            */
/*      datasets.  Or perhaps the drivers that own them should do       */
/*      that in their destructor?                                       */
/************************************************************************/

GDALDriverManager::~GDALDriverManager()

{
/* -------------------------------------------------------------------- */
/*      Destroy the existing drivers.                                   */
/* -------------------------------------------------------------------- */
    while( GetDriverCount() > 0 )
    {
        GDALDriver	*poDriver = GetDriver(0);

        DeregisterDriver(poDriver);
        delete poDriver;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup local memory.                                           */
/* -------------------------------------------------------------------- */
    VSIFree( papoDrivers );
}

/************************************************************************/
/*                           GetDriverCount()                           */
/************************************************************************/

/**
 * Fetch the number of registered drivers.
 *
 * This C analog to this is GDALGetDriverCount().
 *
 * @return the number of registered drivers.
 */

int GDALDriverManager::GetDriverCount()

{
    return( nDrivers );
}

/************************************************************************/
/*                         GDALGetDriverCount()                         */
/************************************************************************/

int GDALGetDriverCount()

{
    return GetGDALDriverManager()->GetDriverCount();
}

/************************************************************************/
/*                             GetDriver()                              */
/************************************************************************/

/**
 * Fetch driver by index.
 *
 * This C analog to this is GDALGetDriver().
 *
 * @param iDriver the driver index from 0 to GetDriverCount()-1.
 *
 * @return the number of registered drivers.
 */

GDALDriver * GDALDriverManager::GetDriver( int iDriver )

{
    if( iDriver < 0 || iDriver >= nDrivers )
        return NULL;
    else
        return papoDrivers[iDriver];
}

/************************************************************************/
/*                           GDALGetDriver()                            */
/************************************************************************/

GDALDriverH GDALGetDriver( int iDriver )

{
    return (GDALDriverH) GetGDALDriverManager()->GetDriver(iDriver);
}

/************************************************************************/
/*                           RegisterDriver()                           */
/************************************************************************/

/**
 * Register a driver for use.
 *
 * The C analog is GDALRegisterDriver().
 *
 * Normally this method is used by format specific C callable registration
 * entry points such as GDALRegister_GTiff() rather than being called
 * directly by application level code.
 *
 * If this driver (based on the object pointer, not short name) is already
 * registered, then no change is made, and the index of the existing driver
 * is returned.  Otherwise the driver list is extended, and the new driver
 * is added at the end.
 *
 * @param poDriver the driver to register.
 *
 * @return the index of the new installed driver.
 */

int GDALDriverManager::RegisterDriver( GDALDriver * poDriver )

{
/* -------------------------------------------------------------------- */
/*      If it is already registered, just return the existing           */
/*      index.                                                          */
/* -------------------------------------------------------------------- */
    if( GetDriverByName( poDriver->pszShortName ) != NULL )
    {
        int		i;

        for( i = 0; i < nDrivers; i++ )
        {
            if( papoDrivers[i] == poDriver )
                return i;
        }

        CPLAssert( FALSE );
    }
    
/* -------------------------------------------------------------------- */
/*      Otherwise grow the list to hold the new entry.                  */
/* -------------------------------------------------------------------- */
    papoDrivers = (GDALDriver **)
        VSIRealloc(papoDrivers, sizeof(GDALDriver *) * (nDrivers+1));

    papoDrivers[nDrivers] = poDriver;
    nDrivers++;

    return( nDrivers - 1 );
}

/************************************************************************/
/*                          DeregisterDriver()                          */
/************************************************************************/

/**
 * Deregister the passed driver.
 *
 * If the driver isn't found no change is made.
 *
 * The C analog is GDALDeregisterDriver().
 *
 * @param poDriver the driver to deregister.
 */

void GDALDriverManager::DeregisterDriver( GDALDriver * poDriver )

{
    int		i;

    for( i = 0; i < nDrivers; i++ )
    {
        if( papoDrivers[i] == poDriver )
            break;
    }

    if( i == nDrivers )
        return;

    while( i < nDrivers-1 )
    {
        papoDrivers[i] = papoDrivers[i+1];
        i++;
    }
    nDrivers--;
}

/************************************************************************/
/*                        GDALDeregisterDriver()                        */
/************************************************************************/

void GDALDeregisterDriver( GDALDriverH hDriver )

{
    GetGDALDriverManager()->DeregisterDriver( (GDALDriver *) hDriver );
}


/************************************************************************/
/*                          GetDriverByName()                           */
/************************************************************************/

/**
 * Fetch a driver based on the short name.
 *
 * The C analog is the GDALGetDriverByName() function.
 *
 * @param pszName the short name, such as GTiff, being searched for.
 *
 * @return the identified driver, or NULL if no match is found.
 */

GDALDriver * GDALDriverManager::GetDriverByName( const char * pszName )

{
    int		i;

    for( i = 0; i < nDrivers; i++ )
    {
        if( EQUAL(papoDrivers[i]->pszShortName, pszName) )
            return papoDrivers[i];
    }

    return NULL;
}

/************************************************************************/
/*                        GDALGetDriverByName()                         */
/************************************************************************/

GDALDriverH GDALGetDriverByName( const char * pszName )

{
    return( GetGDALDriverManager()->GetDriverByName( pszName ) );
}

