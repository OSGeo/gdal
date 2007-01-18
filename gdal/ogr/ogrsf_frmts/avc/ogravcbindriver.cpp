/******************************************************************************
 * $Id$
 *
 * Project:  OGR
 * Purpose:  OGRAVCBinDriver implementation (Arc/Info Binary Coverages)
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_avc.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          ~OGRAVCBinDriver()                          */
/************************************************************************/

OGRAVCBinDriver::~OGRAVCBinDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRAVCBinDriver::GetName()

{
    return "AVCBin";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRAVCBinDriver::Open( const char * pszFilename,
                                      int bUpdate )

{
    OGRAVCBinDataSource	*poDS;
    OGRAVCE00DataSource *poDSE00;

    if( bUpdate )
        return NULL;

    poDS = new OGRAVCBinDataSource();

    if( poDS->Open( pszFilename, TRUE )
        && poDS->GetLayerCount() > 0 )
    {
        return poDS;
    }
    delete poDS;

    poDSE00 = new OGRAVCE00DataSource();

    if( poDSE00->Open( pszFilename, TRUE )
        && poDSE00->GetLayerCount() > 0 )
    {
        return poDSE00;
    }
    delete poDSE00;

    return NULL;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRAVCBinDriver::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                           RegisterOGRAVC()                           */
/************************************************************************/

void RegisterOGRAVCBin()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( 
        new OGRAVCBinDriver );
}
