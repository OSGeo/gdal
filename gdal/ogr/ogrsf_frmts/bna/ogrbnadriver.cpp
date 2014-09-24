/******************************************************************************
 * $Id: ogrbnadriver.cpp
 *
 * Project:  BNA Translator
 * Purpose:  Implements OGRBNADriver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2007, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_bna.h"
#include "cpl_conv.h"

/************************************************************************/
/*                           ~OGRBNADriver()                            */
/************************************************************************/

OGRBNADriver::~OGRBNADriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRBNADriver::GetName()

{
    return "BNA";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRBNADriver::Open( const char * pszFilename, int bUpdate )

{
    OGRBNADataSource   *poDS = new OGRBNADataSource();

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

OGRDataSource *OGRBNADriver::CreateDataSource( const char * pszName,
                                                 char **papszOptions )

{
    OGRBNADataSource   *poDS = new OGRBNADataSource();

    if( !poDS->Create( pszName, papszOptions ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                          DeleteDataSource()                          */
/************************************************************************/

OGRErr OGRBNADriver::DeleteDataSource( const char *pszFilename )

{
    if( VSIUnlink( pszFilename ) == 0 )
        return OGRERR_NONE;
    else
        return OGRERR_FAILURE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRBNADriver::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODrCCreateDataSource) )
        return TRUE;
    else if( EQUAL(pszCap,ODrCDeleteDataSource) )
        return TRUE;
    else
        return FALSE;
}


/************************************************************************/
/*                           RegisterOGRBNA()                           */
/************************************************************************/

void RegisterOGRBNA()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRBNADriver );
}

