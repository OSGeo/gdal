/******************************************************************************
 * $Id$
 *
 * Project:  S-57 Translator
 * Purpose:  Implements OGRS57Layer class.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 * Revision 1.2  1999/11/18 19:01:25  warmerda
 * expanded tabs
 *
 * Revision 1.1  1999/11/03 22:12:43  warmerda
 * New
 *
 */

#include "ogr_s57.h"
#include "cpl_conv.h"
#include "cpl_string.h"

/************************************************************************/
/*                            OGRS57Layer()                             */
/*                                                                      */
/*      Note that the OGRS57Layer assumes ownership of the passed       */
/*      OGRFeatureDefn object.                                          */
/************************************************************************/

OGRS57Layer::OGRS57Layer( OGRS57DataSource *poDSIn,
                          OGRFeatureDefn * poDefnIn )

{
    poFilterGeom = NULL;

    poDS = poDSIn;

    poFeatureDefn = poDefnIn;

    nNextFEIndex = 0;
    nCurrentModule = -1;
}

/************************************************************************/
/*                           ~OGRS57Layer()                           */
/************************************************************************/

OGRS57Layer::~OGRS57Layer()

{
    delete poFeatureDefn;

    if( poFilterGeom != NULL )
        delete poFilterGeom;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRS57Layer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    if( poFilterGeom != NULL )
    {
        delete poFilterGeom;
        poFilterGeom = NULL;
    }

    if( poGeomIn != NULL )
        poFilterGeom = poGeomIn->clone();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRS57Layer::ResetReading()

{
    nNextFEIndex = 0;
    nCurrentModule = -1;
}

/************************************************************************/
/*                      GetNextUnfilteredFeature()                      */
/************************************************************************/

OGRFeature *OGRS57Layer::GetNextUnfilteredFeature()

{
    OGRFeature  *poFeature = NULL;
    
/* -------------------------------------------------------------------- */
/*      Are we out of modules to request features from?                 */
/* -------------------------------------------------------------------- */
    if( nCurrentModule >= poDS->GetModuleCount() )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Set the current position on the current module and fetch a      */
/*      feature.                                                        */
/* -------------------------------------------------------------------- */
    S57Reader   *poReader = poDS->GetModule(nCurrentModule);
    
    if( poReader != NULL )
    {
        poReader->SetNextFEIndex( nNextFEIndex );
        poFeature = poReader->ReadNextFeature( poFeatureDefn );
        nNextFEIndex = poReader->GetNextFEIndex();
    }

/* -------------------------------------------------------------------- */
/*      If we didn't get a feature we need to move onto the next file.  */
/* -------------------------------------------------------------------- */
    if( poFeature == NULL )
    {
        nCurrentModule++;
        poReader = poDS->GetModule(nCurrentModule);

        if( poReader != NULL && poReader->GetModule() == NULL )
        {
            if( !poReader->Open( FALSE ) )
                return NULL;
        }

        return GetNextUnfilteredFeature();
    }

    return poFeature;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRS57Layer::GetNextFeature()

{
    OGRFeature  *poFeature = NULL;
    
/* -------------------------------------------------------------------- */
/*      Read features till we find one that satisfies our current       */
/*      spatial criteria.                                               */
/* -------------------------------------------------------------------- */
    while( TRUE )
    {
        poFeature = GetNextUnfilteredFeature();
        if( poFeature == NULL
            || poFilterGeom == NULL
            || poFeature->GetGeometryRef() == NULL 
            || poFilterGeom->Intersect( poFeature->GetGeometryRef() ) )
            break;

        delete poFeature;
    }

    return poFeature;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRS57Layer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return FALSE;

    else if( EQUAL(pszCap,OLCSequentialWrite) 
             || EQUAL(pszCap,OLCRandomWrite) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return FALSE;

    else 
        return FALSE;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRS57Layer::GetSpatialRef()

{
    return poDS->GetSpatialRef();
}
