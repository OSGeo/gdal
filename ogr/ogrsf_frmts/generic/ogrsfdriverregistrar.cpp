/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRSFDriverRegistrar class implementation.
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
 * Revision 1.6  2001/07/18 04:55:16  warmerda
 * added CPL_CSVID
 *
 * Revision 1.5  2001/01/22 22:36:05  warmerda
 * expanded tabs
 *
 * Revision 1.4  2000/12/05 23:07:43  warmerda
 * Check for CE_Failure, not just an error being set.
 *
 * Revision 1.3  2000/08/30 09:13:34  warmerda
 * Set INST_DATA as FinderLocation
 *
 * Revision 1.2  1999/07/27 00:51:08  warmerda
 * added arg to get driver out of Open()
 *
 * Revision 1.1  1999/07/05 18:58:32  warmerda
 * New
 *
 */

#include "ogrsf_frmts.h"
#include "ogr_p.h"

CPL_CVSID("$Id$");

static OGRSFDriverRegistrar *poRegistrar = NULL;

/************************************************************************/
/*                         OGRSFDriverRegistrar                         */
/************************************************************************/

OGRSFDriverRegistrar::OGRSFDriverRegistrar()

{
    CPLAssert( poRegistrar == NULL );
    nDrivers = 0;
    papoDrivers = NULL;

#ifdef INST_DATA
    CPLPushFinderLocation( INST_DATA );
#endif
}

/************************************************************************/
/*                       ~OGRSFDriverRegistrar()                        */
/************************************************************************/

OGRSFDriverRegistrar::~OGRSFDriverRegistrar()

{
    for( int i = 0; i < nDrivers; i++ )
    {
        delete papoDrivers[i];
    }

    poRegistrar = NULL;
}

/************************************************************************/
/*                            GetRegistrar()                            */
/************************************************************************/

OGRSFDriverRegistrar *OGRSFDriverRegistrar::GetRegistrar()

{
    if( poRegistrar == NULL )
        poRegistrar = new OGRSFDriverRegistrar();

    return poRegistrar;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRSFDriverRegistrar::Open( const char * pszName,
                                           int bUpdate,
                                           OGRSFDriver ** ppoDriver )

{
    OGRDataSource       *poDS;

    if( ppoDriver != NULL )
        *ppoDriver = NULL;

    GetRegistrar();
    
    CPLErrorReset();

    for( int iDriver = 0; iDriver < poRegistrar->nDrivers; iDriver++ )
    {
        poDS = poRegistrar->papoDrivers[iDriver]->Open( pszName, bUpdate );
        if( poDS != NULL )
        {
            if( ppoDriver != NULL )
                *ppoDriver = poRegistrar->papoDrivers[iDriver];
            
            return poDS;
        }

        if( CPLGetLastErrorType() == CE_Failure )
            return NULL;
    }

    return NULL;
}

/************************************************************************/
/*                           RegisterDriver()                           */
/************************************************************************/

void OGRSFDriverRegistrar::RegisterDriver( OGRSFDriver * poNewDriver )

{
    int         iDriver;

/* -------------------------------------------------------------------- */
/*      It has no effect to register a driver more than once.           */
/* -------------------------------------------------------------------- */
    for( iDriver = 0; iDriver < nDrivers; iDriver++ )
    {
        if( poNewDriver == papoDrivers[iDriver] )
            return;
    }                                                   

/* -------------------------------------------------------------------- */
/*      Add to the end of the driver list.                              */
/* -------------------------------------------------------------------- */
    papoDrivers = (OGRSFDriver **)
        CPLRealloc( papoDrivers, (nDrivers+1) * sizeof(void*) );

    papoDrivers[nDrivers++] = poNewDriver;
}

/************************************************************************/
/*                           GetDriverCount()                           */
/************************************************************************/

int OGRSFDriverRegistrar::GetDriverCount()

{
    return nDrivers;
}

/************************************************************************/
/*                             GetDriver()                              */
/************************************************************************/

OGRSFDriver *OGRSFDriverRegistrar::GetDriver( int i )

{
    if( i < 0 || i >= nDrivers )
        return NULL;
    else
        return papoDrivers[i];
}
