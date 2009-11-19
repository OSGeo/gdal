/******************************************************************************
 * $Id: ogrcsvdatasource.cpp 17806 2009-10-13 17:27:54Z rouault $
 *
 * Project:  PCIDSK Translator
 * Purpose:  Implements OGRPCIDSKDataSource class
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_pcidsk.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_csv.h"

CPL_CVSID("$Id: ogrcsvdatasource.cpp 17806 2009-10-13 17:27:54Z rouault $");

/************************************************************************/
/*                        OGRPCIDSKDataSource()                         */
/************************************************************************/

OGRPCIDSKDataSource::OGRPCIDSKDataSource()

{
    bUpdate = FALSE;
}

/************************************************************************/
/*                        ~OGRPCIDSKDataSource()                        */
/************************************************************************/

OGRPCIDSKDataSource::~OGRPCIDSKDataSource()

{
    while( apoLayers.size() > 0 )
    {
        delete apoLayers.back();
        apoLayers.pop_back();
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPCIDSKDataSource::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRPCIDSKDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= (int) apoLayers.size() )
        return NULL;
    else
        return apoLayers[iLayer];
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRPCIDSKDataSource::Open( const char * pszFilename, int bUpdateIn )

{
    osName = pszFilename;
    bUpdate = bUpdateIn;

/* -------------------------------------------------------------------- */
/*      Open the file, and create layer for each vector segment.        */
/* -------------------------------------------------------------------- */
    try 
    {
        PCIDSK::PCIDSKSegment *segobj;

        poFile = PCIDSK::Open( pszFilename, "r", NULL );

        for( segobj = poFile->GetSegment( PCIDSK::SEG_VEC, "" );
             segobj != NULL;
             segobj = poFile->GetSegment( PCIDSK::SEG_VEC, "",
                                          segobj->GetSegmentNumber() ) )
        {
            apoLayers.push_back( new OGRPCIDSKLayer( segobj ) );
        }
    }

/* -------------------------------------------------------------------- */
/*      Trap exceptions and report as CPL errors.                       */
/* -------------------------------------------------------------------- */
    catch( PCIDSK::PCIDSKException ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s", ex.what() );
        return FALSE;
    }
    catch(...)
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Non-PCIDSK exception trapped." );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      We presume that this is indeed intended to be a PCIDSK             */
/*      datasource if over half the files were .csv files.              */
/* -------------------------------------------------------------------- */
    return TRUE;
}

