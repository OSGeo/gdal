/******************************************************************************
 * $Id$
 *
 * Project:  S-57 Translator
 * Purpose:  Implements OGRS57DataSource class
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
 * Revision 1.4  1999/11/18 19:01:25  warmerda
 * expanded tabs
 *
 * Revision 1.3  1999/11/16 21:47:31  warmerda
 * updated class occurance collection
 *
 * Revision 1.2  1999/11/08 22:23:00  warmerda
 * added object class support
 *
 * Revision 1.1  1999/11/03 22:12:43  warmerda
 * New
 *
 */

#include "ogr_s57.h"
#include "cpl_conv.h"
#include "cpl_string.h"

S57ClassRegistrar *OGRS57DataSource::poRegistrar = NULL;

/************************************************************************/
/*                          OGRS57DataSource()                          */
/************************************************************************/

OGRS57DataSource::OGRS57DataSource()

{
    nLayers = 0;
    papoLayers = NULL;

    pszName = NULL;

    papszOptions = NULL;

    poSpatialRef = new OGRSpatialReference( "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563]],PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]]");
}

/************************************************************************/
/*                         ~OGRS57DataSource()                          */
/************************************************************************/

OGRS57DataSource::~OGRS57DataSource()

{
    int         i;

    for( i = 0; i < nLayers; i++ )
        delete papoLayers[i];

    CPLFree( papoLayers );

    CPLFree( pszName );

    CSLDestroy( papszOptions );

    delete poSpatialRef;
}

/************************************************************************/
/*                             SetOptions()                             */
/************************************************************************/

void OGRS57DataSource::SetOptionList( char ** papszNewOptions )

{
    CSLDestroy( papszOptions );
    papszOptions = CSLDuplicate( papszNewOptions );
}

/************************************************************************/
/*                             GetOption()                              */
/************************************************************************/

const char *OGRS57DataSource::GetOption( const char * pszOption )

{
    return CSLFetchNameValue( papszOptions, pszOption );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRS57DataSource::TestCapability( const char * )

{
    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRS57DataSource::Open( const char * pszFilename, int bTestOpen )

{
    int         iModule;
    
    pszName = CPLStrdup( pszFilename );
    
/* -------------------------------------------------------------------- */
/*      Check a few bits of the header to see if it looks like an       */
/*      S57 file (really, if it looks like an ISO8211 file).            */
/* -------------------------------------------------------------------- */
    if( bTestOpen )
    {
        FILE    *fp;
        char    pachLeader[10];

        fp = VSIFOpen( pszFilename, "rb" );
        if( fp == NULL )
            return FALSE;
        
        if( VSIFRead( pachLeader, 1, 10, fp ) != 10
            || (pachLeader[5] != '1' && pachLeader[5] != '2'
                && pachLeader[5] != '3' )
            || pachLeader[6] != 'L'
            || (pachLeader[8] != '1' && pachLeader[8] != ' ') )
        {
            VSIFClose( fp );
            return FALSE;
        }

        VSIFClose( fp );
    }

/* -------------------------------------------------------------------- */
/*      Instantiate an S57Reader and try opening.                       */
/*                                                                      */
/*      Eventually this should check for catalogs, and if found         */
/*      instantiate a whole series of modules.                          */
/* -------------------------------------------------------------------- */
    S57Reader   *poModule;

    poModule = new S57Reader( pszFilename );
    if( !poModule->Open( bTestOpen ) )
    {
        delete poModule;

        return FALSE;
    }

    nModules = 1;
    papoModules = (S57Reader **) CPLMalloc(sizeof(void*));
    papoModules[0] = poModule;
    
/* -------------------------------------------------------------------- */
/*      Instantiate the class registrar if possible.                    */
/* -------------------------------------------------------------------- */
    if( poRegistrar == NULL )
    {
        poRegistrar = new S57ClassRegistrar();

        if( !poRegistrar->LoadInfo( "/home/warmerda/data/s57", FALSE ) )
        {
            delete poRegistrar;
            poRegistrar = NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Initialize a layer for each type of geometry.  Eventually       */
/*      we will do this by object class.                                */
/* -------------------------------------------------------------------- */
    if( poRegistrar == NULL )
    {
        OGRFeatureDefn  *poDefn;

        poDefn = S57Reader::GenerateGeomFeatureDefn( wkbPoint );
        AddLayer( new OGRS57Layer( this, poDefn ) );
    
        poDefn = S57Reader::GenerateGeomFeatureDefn( wkbLineString );
        AddLayer( new OGRS57Layer( this, poDefn ) );
    
        poDefn = S57Reader::GenerateGeomFeatureDefn( wkbPolygon );
        AddLayer( new OGRS57Layer( this, poDefn ) );
    
        poDefn = S57Reader::GenerateGeomFeatureDefn( wkbNone );
        AddLayer( new OGRS57Layer( this, poDefn ) );
    }

/* -------------------------------------------------------------------- */
/*      Initialize a feature definition for each class that actually    */
/*      occurs in the dataset.                                          */
/* -------------------------------------------------------------------- */
    else
    {
        OGRFeatureDefn  *poDefn;
        int             *panClassCount;
        int             iClass;

        panClassCount = (int *) CPLCalloc(sizeof(int),MAX_CLASSES);

        for( iModule = 0; iModule < nModules; iModule++ )
            papoModules[iModule]->CollectClassList(panClassCount,MAX_CLASSES);

        for( iClass = 0; iClass < MAX_CLASSES; iClass++ )
        {
            if( panClassCount[iClass] > 0 )
            {
                poDefn = S57Reader::GenerateObjectClassDefn( poRegistrar,
                                                             iClass );
                if( poDefn != NULL )
                    AddLayer( new OGRS57Layer( this, poDefn ) );
            }
        }

        CPLFree( panClassCount );
    }

/* -------------------------------------------------------------------- */
/*      Attach the layer definitions to each of the readers.            */
/* -------------------------------------------------------------------- */
    for( iModule = 0; iModule < nModules; iModule++ )
    {
        if( poRegistrar != NULL )
            papoModules[iModule]->SetClassBased( poRegistrar );
        
        for( int iLayer = 0; iLayer < nLayers; iLayer++ )
        {
            papoModules[iModule]->AddFeatureDefn(
                papoLayers[iLayer]->GetLayerDefn() );
        }
    }
    
    return TRUE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRS57DataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                              AddLayer()                              */
/************************************************************************/

void OGRS57DataSource::AddLayer( OGRS57Layer * poNewLayer )

{
    papoLayers = (OGRS57Layer **)
        CPLRealloc( papoLayers, sizeof(void*) * ++nLayers );
    
    papoLayers[nLayers-1] = poNewLayer;
}

/************************************************************************/
/*                             GetModule()                              */
/************************************************************************/

S57Reader * OGRS57DataSource::GetModule( int i )

{
    if( i < 0 || i >= nModules )
        return NULL;
    else
        return papoModules[i];
}
