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

int GDALDriverManager::GetDriverCount()

{
    return( nDrivers );
}

/************************************************************************/
/*                             GetDriver()                              */
/************************************************************************/

GDALDriver * GDALDriverManager::GetDriver( int i )

{
    if( i < 0 || i >= nDrivers )
        return NULL;
    else
        return papoDrivers[i];
}


/************************************************************************/
/*                           RegisterDriver()                           */
/************************************************************************/

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
/*                          GetDriverByName()                           */
/************************************************************************/

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

