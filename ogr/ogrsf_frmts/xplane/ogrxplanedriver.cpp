/******************************************************************************
 * $Id: ogrxplanedriver.cpp
 *
 * Project:  X-Plane aeronautical data reader
 * Purpose:  Implements OGRXPlaneDriver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2008, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_xplane.h"
#include "cpl_conv.h"

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRXPlaneDriver::GetName()

{
    return "XPlane";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRXPlaneDriver::Open( const char * pszFilename, int bUpdate )

{
    if ( bUpdate )
    {
        return FALSE;
    }

    OGRXPlaneDataSource   *poDS = new OGRXPlaneDataSource();

    int bReadWholeFile = CSLTestBoolean(CPLGetConfigOption("OGR_XPLANE_READ_WHOLE_FILE", "TRUE"));

    if( !poDS->Open( pszFilename, bReadWholeFile ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRXPlaneDriver::TestCapability( const char * pszCap )
{
    return FALSE;
}

/************************************************************************/
/*                           RegisterOGRXPlane()                        */
/************************************************************************/

void RegisterOGRXPlane()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRXPlaneDriver );
}

