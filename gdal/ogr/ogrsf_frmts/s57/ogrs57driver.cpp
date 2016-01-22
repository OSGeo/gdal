/******************************************************************************
 * $Id$
 *
 * Project:  S-57 Translator
 * Purpose:  Implements OGRS57Driver
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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
 ****************************************************************************/

#include "ogr_s57.h"
#include "cpl_conv.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$");

S57ClassRegistrar *OGRS57Driver::poRegistrar = NULL;
static void* hS57RegistrarMutex = NULL;

/************************************************************************/
/*                            OGRS57Driver()                            */
/************************************************************************/

OGRS57Driver::OGRS57Driver()

{
}

/************************************************************************/
/*                           ~OGRS57Driver()                            */
/************************************************************************/

OGRS57Driver::~OGRS57Driver()

{
    if( poRegistrar != NULL )
    {
        delete poRegistrar;
        poRegistrar = NULL;
    }
    
    if( hS57RegistrarMutex != NULL )
    {
        CPLDestroyMutex(hS57RegistrarMutex);
        hS57RegistrarMutex = NULL;
    }
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRS57Driver::GetName()

{
    return "S57";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRS57Driver::Open( const char * pszFilename, int bUpdate )

{
    OGRS57DataSource    *poDS;

    poDS = new OGRS57DataSource;
    if( !poDS->Open( pszFilename, TRUE ) )
    {
        delete poDS;
        poDS = NULL;
    }

    if( poDS && bUpdate )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "S57 Driver doesn't support update." );
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

OGRDataSource *OGRS57Driver::CreateDataSource( const char *pszName, 
                                               char **papszOptions )

{
    OGRS57DataSource *poDS = new OGRS57DataSource();

    if( poDS->Create( pszName, papszOptions ) )
        return poDS;
    else
    {
        delete poDS;
        return NULL;
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRS57Driver::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODrCCreateDataSource) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                          GetS57Registrar()                           */
/************************************************************************/

S57ClassRegistrar *OGRS57Driver::GetS57Registrar()

{
/* -------------------------------------------------------------------- */
/*      Instantiate the class registrar if possible.                    */
/* -------------------------------------------------------------------- */
    CPLMutexHolderD(&hS57RegistrarMutex);

    if( poRegistrar == NULL )
    {
        poRegistrar = new S57ClassRegistrar();

        if( !poRegistrar->LoadInfo( NULL, NULL, FALSE ) )
        {
            delete poRegistrar;
            poRegistrar = NULL;
        }
    }

    return poRegistrar;
}

/************************************************************************/
/*                           RegisterOGRS57()                           */
/************************************************************************/

void RegisterOGRS57()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRS57Driver );
}


