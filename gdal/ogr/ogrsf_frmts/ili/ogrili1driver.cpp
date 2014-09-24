/******************************************************************************
 * $Id$
 *
 * Project:  Interlis 1 Translator
 * Purpose:  Implements OGRILI1Layer class.
 * Author:   Pirmin Kalberer, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG
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

#include "ogr_ili1.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          ~OGRILI1Driver()                           */
/************************************************************************/

OGRILI1Driver::~OGRILI1Driver() {
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRILI1Driver::GetName() {
    return "Interlis 1";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRILI1Driver::Open( const char * pszFilename,
                                   int bUpdate )

{
    OGRILI1DataSource    *poDS;

    if( bUpdate )
        return NULL;

    poDS = new OGRILI1DataSource();

    if( !poDS->Open( pszFilename, TRUE )
        || poDS->GetLayerCount() == 0 )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

OGRDataSource *OGRILI1Driver::CreateDataSource( const char * pszName,
                                               char **papszOptions )

{
    OGRILI1DataSource    *poDS = new OGRILI1DataSource();

    if( !poDS->Create( pszName, papszOptions ) )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRILI1Driver::TestCapability( const char * pszCap ) {
    if( EQUAL(pszCap,ODrCCreateDataSource) )
        return TRUE;
    else if( EQUAL(pszCap,ODrCDeleteDataSource) )
        return FALSE;
    else
        return FALSE;
}

/************************************************************************/
/*                           RegisterOGRILI1()                           */
/************************************************************************/

void RegisterOGRILI1() {
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRILI1Driver );
}

