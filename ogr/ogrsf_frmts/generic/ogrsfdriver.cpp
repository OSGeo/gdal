/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The generic portions of the OGRSFDriver class.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
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
 * $Log$
 * Revision 1.3  2002/09/26 18:16:19  warmerda
 * added C entry points
 *
 * Revision 1.2  2001/07/18 04:55:16  warmerda
 * added CPL_CSVID
 *
 * Revision 1.1  1999/11/04 21:10:51  warmerda
 * New
 *
 */

#include "ogrsf_frmts.h"
#include "ogr_api.h"
#include "ogr_p.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            ~OGRSFDriver()                            */
/************************************************************************/

OGRSFDriver::~OGRSFDriver()

{
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

OGRDataSource *OGRSFDriver::CreateDataSource( const char *, char ** )

{
    CPLError( CE_Failure, CPLE_NotSupported,
              "CreateDataSource() not supported by this driver.\n" );
              
    return NULL;
}

/************************************************************************/
/*                      OGR_Dr_CreateDataSource()                       */
/************************************************************************/

OGRDataSourceH OGR_Dr_CreateDataSource( OGRSFDriverH hDriver,
                                        const char *pszName, 
                                        char ** papszOptions )

{
    return ((OGRSFDriver *)hDriver)->CreateDataSource( pszName, 
                                                       papszOptions );
}

/************************************************************************/
/*                           OGR_Dr_GetName()                           */
/************************************************************************/

const char *OGR_Dr_GetName( OGRSFDriverH hDriver )

{
    return ((OGRSFDriver *) hDriver)->GetName();
}

/************************************************************************/
/*                            OGR_Dr_Open()                             */
/************************************************************************/

OGRDataSourceH OGR_Dr_Open( OGRSFDriverH hDriver, const char *pszName, 
                            int bUpdate )

{
    return ((OGRSFDriver *)hDriver)->Open( pszName, bUpdate );
}

/************************************************************************/
/*                       OGR_Dr_TestCapability()                        */
/************************************************************************/

int OGR_Dr_TestCapability( OGRSFDriverH hDriver, const char *pszCap )

{
    return ((OGRSFDriver *) hDriver)->TestCapability( pszCap );
}

