/******************************************************************************
 * $Id$
 *
 * Project:  SOSI Translator
 * Purpose:  Implements OGRSOSIDriver.
 * Author:   Thomas Hirsch, <thomas.hirsch statkart no>
 *
 ******************************************************************************
 * Copyright (c) 2010, Thomas Hirsch
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

#include "ogr_sosi.h"

void RegisterOGRSOSI() {
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRSOSIDriver );
}

static int nFYBAInitCounter = 0;

/************************************************************************/
/*                           OGRSOSIDriver()                           */
/************************************************************************/
OGRSOSIDriver::OGRSOSIDriver() {
    if ( nFYBAInitCounter == 0 )
    {
        LC_Init();  /* Init FYBA */
    }
    nFYBAInitCounter++;
}

/************************************************************************/
/*                           ~OGRSOSIDriver()                           */
/************************************************************************/

OGRSOSIDriver::~OGRSOSIDriver() {
    nFYBAInitCounter--;
    if ( nFYBAInitCounter == 0 )
    {
        LC_Close(); /* Close FYBA */
    }
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRSOSIDriver::GetName() {
    return "SOSI";
}

/************************************************************************/
/*                              Open()                                  */
/************************************************************************/

OGRDataSource *OGRSOSIDriver::Open( const char * pszFilename, int bUpdate ) {
    OGRSOSIDataSource   *poDS = new OGRSOSIDataSource();
    if ( !poDS->Open( pszFilename, 0 ) ) {
        delete poDS;
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                              CreateDataSource()                      */
/************************************************************************/
OGRDataSource *OGRSOSIDriver::CreateDataSource( const char *pszName, char **papszOptions) {
    OGRSOSIDataSource   *poDS = new OGRSOSIDataSource();
    if ( !poDS->Create( pszName ) ) {
        delete poDS;
        return NULL;
    }
    return poDS;
}


/************************************************************************/
/*                              TestCapability()                        */
/************************************************************************/

int OGRSOSIDriver::TestCapability( const char * pszCap ) {
    if (strcmp("CreateDataSource",pszCap) == 0) {
        return TRUE; 
    } else {
        CPLDebug( "[TestCapability]","Capability %s not supported by SOSI driver", pszCap);
    }
    return FALSE;
}
