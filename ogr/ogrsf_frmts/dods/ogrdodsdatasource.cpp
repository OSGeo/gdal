/******************************************************************************
 * $Id$
 *
 * Project:  OGR/DODS Interface
 * Purpose:  Implements OGRDODSDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam
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
 * Revision 1.1  2004/01/21 20:09:01  warmerda
 * New
 *
 */

#include "ogr_dods.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");
/************************************************************************/
/*                         OGRDODSDataSource()                          */
/************************************************************************/

OGRDODSDataSource::OGRDODSDataSource()

{
    pszName = NULL;
    papoLayers = NULL;
    nLayers = 0;
}

/************************************************************************/
/*                         ~OGRDODSDataSource()                         */
/************************************************************************/

OGRDODSDataSource::~OGRDODSDataSource()

{
    int         i;

    CPLFree( pszName );

    for( i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    
    CPLFree( papoLayers );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRDODSDataSource::Open( const char * pszNewName )

{
    CPLAssert( nLayers == 0 );					     

    pszName = CPLStrdup( pszNewName );

/* -------------------------------------------------------------------- */
/*      Connect to the server.                                          */
/* -------------------------------------------------------------------- */
    string oURL = (pszNewName + 5);
    string version;

    try 
    {
        poConnection = new AISConnect( oURL );
        version = poConnection->request_version();
    } 
    catch (Error &e) 
    {
        CPLError(CE_Failure, CPLE_OpenFailed, 
                 e.get_error_message().c_str() );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      We presume we only work with version 3 servers.                 */
/* -------------------------------------------------------------------- */
    if (version.empty() || version.find("/3.") == string::npos)
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "I connected to the URL but could not get a DAP 3.x version string from the server");
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Fetch the DAS and DDS info about the server.                    */
/* -------------------------------------------------------------------- */
    try
    {
        poConnection->request_das( oDAS );
        poConnection->request_dds( oDDS );
    }
    catch (Error &e) 
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Error fetching DAS or DDS:\n%s", 
                 e.get_error_message().c_str() );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Do we have any ogr_layer_info attributes in the DAS?  If so,    */
/*      use them to define the layers.                                  */
/* -------------------------------------------------------------------- */
    AttrTable::Attr_iter dv_i;

    for( dv_i = oDAS.attr_begin(); dv_i != oDAS.attr_end(); dv_i++ )
    {
        if( EQUAL(oDAS.get_name(dv_i).c_str(),"ogr_layer_info") 
            && oDAS.is_container( dv_i ) )
        {
            AttrTable *poAttr = oDAS.get_attr_table( dv_i );
            string target_container = poAttr->get_attr( "target_container" );
            BaseType *poVar = oDDS.var( target_container.c_str() );
            
            if( poVar == NULL )
            {
                CPLError( CE_Warning, CPLE_AppDefined, 
                          "Unable to find variable '%s' named in\n"
                          "ogr_layer_info.target_container, skipping.", 
                          target_container.c_str() );
                continue;
            }

            if( poVar->type() == dods_sequence_c )
                AddLayer( 
                    new OGRDODSSequenceLayer(this,
                                             target_container.c_str(),
                                             poAttr) );
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Walk through the DODS variables looking for easily targetted    */
/*      ones.  Eventually this will need to be driven by the AIS info.  */
/* -------------------------------------------------------------------- */
    if( nLayers == 0 )
    {
        DDS::Vars_iter v_i;

        for( v_i = oDDS.var_begin(); v_i != oDDS.var_end(); v_i++ )
        {
            BaseType *poVar = *v_i;
            
            if( poVar->type() == dods_sequence_c )
                AddLayer( new OGRDODSSequenceLayer(this,poVar->name().c_str(),
                                                   NULL) );
        }
    }

    return TRUE;
}

/************************************************************************/
/*                              AddLayer()                              */
/************************************************************************/

void OGRDODSDataSource::AddLayer( OGRDODSLayer *poLayer )

{
    papoLayers = (OGRDODSLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRDODSLayer *) * (nLayers+1) );
    papoLayers[nLayers++] = poLayer;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDODSDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRDODSDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}
