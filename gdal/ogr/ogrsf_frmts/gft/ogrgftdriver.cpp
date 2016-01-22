/******************************************************************************
 * $Id$
 *
 * Project:  GFT Translator
 * Purpose:  Implements OGRGFTDriver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_gft.h"

// g++ -g -Wall -fPIC -shared -o ogr_GFT.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/gft ogr/ogrsf_frmts/gft/*.c* -L. -lgdal

/* http://code.google.com/intl/fr/apis/fusiontables/docs/developers_reference.html */

CPL_CVSID("$Id$");

extern "C" void RegisterOGRGFT();

/************************************************************************/
/*                           ~OGRGFTDriver()                            */
/************************************************************************/

OGRGFTDriver::~OGRGFTDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRGFTDriver::GetName()

{
    return "GFT";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRGFTDriver::Open( const char * pszFilename, int bUpdate )

{
    OGRGFTDataSource   *poDS = new OGRGFTDataSource();

    if( !poDS->Open( pszFilename, bUpdate ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}


/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

OGRDataSource *OGRGFTDriver::CreateDataSource( const char * pszName,
                                               CPL_UNUSED char **papszOptions )
{
    OGRGFTDataSource   *poDS = new OGRGFTDataSource();

    if( !poDS->Open( pszName, TRUE ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGFTDriver::TestCapability( const char * pszCap )

{
    if (EQUAL(pszCap, ODrCCreateDataSource))
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                           RegisterOGRGFT()                           */
/************************************************************************/

void RegisterOGRGFT()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRGFTDriver );
}

