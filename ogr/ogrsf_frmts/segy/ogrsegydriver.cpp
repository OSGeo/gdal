/******************************************************************************
 * $Id$
 *
 * Project:  SEG-Y Translator
 * Purpose:  Implements OGRSEGYDriver class.
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
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMSEGYS OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "ogr_segy.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           ~OGRSEGYDriver()                           */
/************************************************************************/

OGRSEGYDriver::~OGRSEGYDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRSEGYDriver::GetName()

{
    return "SEGY";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRSEGYDriver::Open( const char * pszFilename, int bUpdate )

{
    OGRSEGYDataSource   *poDS = new OGRSEGYDataSource();

    if( !poDS->Open( pszFilename, bUpdate ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSEGYDriver::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                           RegisterOGRSEGY()                           */
/************************************************************************/

void RegisterOGRSEGY()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRSEGYDriver );
}

