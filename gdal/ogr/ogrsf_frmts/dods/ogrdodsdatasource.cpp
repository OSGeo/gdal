/******************************************************************************
 * $Id$
 *
 * Project:  OGR/DODS Interface
 * Purpose:  Implements OGRDODSDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam
 * Copyright (c) 2010, Even Rouault <even dot rouault at mines-paris dot org>
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
    poConnection = NULL;

    poBTF = new BaseTypeFactory();
    poDDS = new DDS( poBTF );
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

    if( poConnection != NULL )
        delete poConnection;

    delete poDDS;
    delete poBTF;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRDODSDataSource::Open( const char * pszNewName )

{
    CPLAssert( nLayers == 0 );					     

    pszName = CPLStrdup( pszNewName );

/* -------------------------------------------------------------------- */
/*      Parse the URL into a base url, projection and constraint        */
/*      expression.                                                     */
/* -------------------------------------------------------------------- */
    char *pszWrkURL = CPLStrdup( pszNewName + 5 );
    char *pszFound;

    pszFound = strstr(pszWrkURL,"&");
    if( pszFound )
    {
        oConstraints = pszFound;
        *pszFound = '\0';
    }
        
    pszFound = strstr(pszWrkURL,"?");
    if( pszFound )
    {
        oProjection = pszFound+1;
        *pszFound = '\0';
    }

    // Trim common requests.
    int nLen = strlen(pszWrkURL);
    if( strcmp(pszWrkURL+nLen-4,".das") == 0 )
        pszWrkURL[nLen-4] = '\0';
    else if( strcmp(pszWrkURL+nLen-4,".dds") == 0 )
        pszWrkURL[nLen-4] = '\0';
    else if( strcmp(pszWrkURL+nLen-4,".asc") == 0 )
        pszWrkURL[nLen-4] = '\0';
    else if( strcmp(pszWrkURL+nLen-5,".dods") == 0 )
        pszWrkURL[nLen-5] = '\0';
    else if( strcmp(pszWrkURL+nLen-5,".html") == 0 )
        pszWrkURL[nLen-5] = '\0';
        
    oBaseURL = pszWrkURL;
    CPLFree( pszWrkURL );

/* -------------------------------------------------------------------- */
/*      Do we want to override the .dodsrc file setting?  Only do       */
/*      the putenv() if there isn't already a DODS_CONF in the          */
/*      environment.                                                    */
/* -------------------------------------------------------------------- */
    if( CPLGetConfigOption( "DODS_CONF", NULL ) != NULL 
        && getenv("DODS_CONF") == NULL )
    {
        static char szDODS_CONF[1000];

        sprintf( szDODS_CONF, "DODS_CONF=%.980s", 
                 CPLGetConfigOption( "DODS_CONF", "" ) );
        putenv( szDODS_CONF );
    }

/* -------------------------------------------------------------------- */
/*      If we have a overridding AIS file location, apply it now.       */
/* -------------------------------------------------------------------- */
    if( CPLGetConfigOption( "DODS_AIS_FILE", NULL ) != NULL )
    {
        string oAISFile = CPLGetConfigOption( "DODS_AIS_FILE", "" );
        RCReader::instance()->set_ais_database( oAISFile );
    }

/* -------------------------------------------------------------------- */
/*      Connect to the server.                                          */
/* -------------------------------------------------------------------- */
    string version;

    try 
    {
        poConnection = new AISConnect( oBaseURL );
        version = poConnection->request_version();
    } 
    catch (Error &e) 
    {
        CPLError(CE_Failure, CPLE_OpenFailed, 
                 "%s", e.get_error_message().c_str() );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      We presume we only work with version 3 servers.                 */
/* -------------------------------------------------------------------- */

    if (version.empty() || version.find("/3.") == string::npos)
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "I connected to the URL but could not get a DAP 3.x version string\n"
                  "from the server.  I will continue to connect but access may fail.");
    }

/* -------------------------------------------------------------------- */
/*      Fetch the DAS and DDS info about the server.                    */
/* -------------------------------------------------------------------- */
    try
    {
        poConnection->request_das( oDAS );
        poConnection->request_dds( *poDDS, oProjection + oConstraints );
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

#ifdef LIBDAP_39
    AttrTable* poTable = oDAS.container();
    if (poTable == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot get container");
        return FALSE;
    }
#else
    AttrTable* poTable = &oDAS;
#endif

    for( dv_i = poTable->attr_begin(); dv_i != poTable->attr_end(); dv_i++ )
    {
        if( EQUALN(poTable->get_name(dv_i).c_str(),"ogr_layer_info",14) 
            && poTable->is_container( dv_i ) )
        {
            AttrTable *poAttr = poTable->get_attr_table( dv_i );
            string target_container = poAttr->get_attr( "target_container" );
            BaseType *poVar = poDDS->var( target_container.c_str() );
            
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
            else if( poVar->type() == dods_grid_c 
                     || poVar->type() == dods_array_c )
                AddLayer( new OGRDODSGridLayer(this,target_container.c_str(),
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

        for( v_i = poDDS->var_begin(); v_i != poDDS->var_end(); v_i++ )
        {
            BaseType *poVar = *v_i;
            
            if( poVar->type() == dods_sequence_c )
                AddLayer( new OGRDODSSequenceLayer(this,poVar->name().c_str(),
                                                   NULL) );
            else if( poVar->type() == dods_grid_c 
                     || poVar->type() == dods_array_c )
                AddLayer( new OGRDODSGridLayer(this,poVar->name().c_str(),
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
