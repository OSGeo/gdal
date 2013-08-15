/******************************************************************************
 * $Id$
 *
 * Project:  S-57 Translator
 * Purpose:  Implements OGRS57DataSource class
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 ****************************************************************************/

#include "ogr_s57.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRS57DataSource()                          */
/************************************************************************/

OGRS57DataSource::OGRS57DataSource()

{
    nLayers = 0;
    papoLayers = NULL;

    nModules = 0;
    papoModules = NULL;
    poClassContentExplorer = NULL;
    poWriter = NULL;

    pszName = NULL;

    poSpatialRef = new OGRSpatialReference();
    poSpatialRef->SetWellKnownGeogCS( "WGS84" );
    
    bExtentsSet = FALSE;


/* -------------------------------------------------------------------- */
/*      Allow initialization of options from the environment.           */
/* -------------------------------------------------------------------- */
    const char *pszOptString = CPLGetConfigOption( "OGR_S57_OPTIONS", NULL );
    papszOptions = NULL;

    if ( pszOptString )
    {
        char    **papszCurOption;

        papszOptions = 
            CSLTokenizeStringComplex( pszOptString, ",", FALSE, FALSE );

        if ( papszOptions && *papszOptions )
        {
            CPLDebug( "S57", "The following S57 options are being set:" );
            papszCurOption = papszOptions;
            while( *papszCurOption )
                CPLDebug( "S57", "    %s", *papszCurOption++ );
        }
    }
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

    for( i = 0; i < nModules; i++ )
        delete papoModules[i];
    CPLFree( papoModules );

    CPLFree( pszName );

    CSLDestroy( papszOptions );

    poSpatialRef->Release();

    if( poWriter != NULL )
    {
        poWriter->Close();
        delete poWriter;
    }
    delete poClassContentExplorer;
}

/************************************************************************/
/*                           SetOptionList()                            */
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
        VSILFILE    *fp;
        char    pachLeader[10];

        VSIStatBufL sStatBuf;
        if (VSIStatExL( pszFilename, &sStatBuf, VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG ) != 0 ||
            VSI_ISDIR(sStatBuf.st_mode))
            return FALSE;

        fp = VSIFOpenL( pszFilename, "rb" );
        if( fp == NULL )
            return FALSE;
        
        if( VSIFReadL( pachLeader, 1, 10, fp ) != 10
            || (pachLeader[5] != '1' && pachLeader[5] != '2'
                && pachLeader[5] != '3' )
            || pachLeader[6] != 'L'
            || (pachLeader[8] != '1' && pachLeader[8] != ' ') )
        {
            VSIFCloseL( fp );
            return FALSE;
        }

        VSIFCloseL( fp );
    }

/* -------------------------------------------------------------------- */
/*      Setup reader options.                                           */
/* -------------------------------------------------------------------- */
    char **papszReaderOptions = NULL;
    S57Reader   *poModule;

    poModule = new S57Reader( pszFilename );

    papszReaderOptions = CSLSetNameValue(papszReaderOptions, 
                                         S57O_LNAM_REFS, "ON" );
    if( GetOption(S57O_UPDATES) != NULL )
        papszReaderOptions = 
            CSLSetNameValue( papszReaderOptions, S57O_UPDATES, 
                             GetOption(S57O_UPDATES));
                                              
    if( GetOption(S57O_SPLIT_MULTIPOINT) != NULL )
        papszReaderOptions = 
            CSLSetNameValue( papszReaderOptions, S57O_SPLIT_MULTIPOINT,
                             GetOption(S57O_SPLIT_MULTIPOINT) );
                                              
    if( GetOption(S57O_ADD_SOUNDG_DEPTH) != NULL )
        papszReaderOptions = 
            CSLSetNameValue( papszReaderOptions, S57O_ADD_SOUNDG_DEPTH,
                             GetOption(S57O_ADD_SOUNDG_DEPTH));
                                              
    if( GetOption(S57O_PRESERVE_EMPTY_NUMBERS) != NULL )
        papszReaderOptions = 
            CSLSetNameValue( papszReaderOptions, S57O_PRESERVE_EMPTY_NUMBERS,
                             GetOption(S57O_PRESERVE_EMPTY_NUMBERS) );
                                              
    if( GetOption(S57O_RETURN_PRIMITIVES) != NULL )
        papszReaderOptions = 
            CSLSetNameValue( papszReaderOptions, S57O_RETURN_PRIMITIVES,
                             GetOption(S57O_RETURN_PRIMITIVES) );
                                              
    if( GetOption(S57O_RETURN_LINKAGES) != NULL )
        papszReaderOptions = 
            CSLSetNameValue( papszReaderOptions, S57O_RETURN_LINKAGES,
                             GetOption(S57O_RETURN_LINKAGES) );
                                              
    if( GetOption(S57O_RETURN_DSID) != NULL )
        papszReaderOptions = 
            CSLSetNameValue( papszReaderOptions, S57O_RETURN_DSID,
                             GetOption(S57O_RETURN_DSID) );

    if( GetOption(S57O_RECODE_BY_DSSI) != NULL )
        papszReaderOptions = 
            CSLSetNameValue( papszReaderOptions, S57O_RECODE_BY_DSSI,
                             GetOption(S57O_RECODE_BY_DSSI) );

    poModule->SetOptions( papszReaderOptions );
    CSLDestroy( papszReaderOptions );

/* -------------------------------------------------------------------- */
/*      Try opening.                                                    */
/*                                                                      */
/*      Eventually this should check for catalogs, and if found         */
/*      instantiate a whole series of modules.                          */
/* -------------------------------------------------------------------- */
    if( !poModule->Open( bTestOpen ) )
    {
        delete poModule;

        return FALSE;
    }

    int bSuccess = TRUE;

    nModules = 1;
    papoModules = (S57Reader **) CPLMalloc(sizeof(void*));
    papoModules[0] = poModule;
    
/* -------------------------------------------------------------------- */
/*      Add the header layers if they are called for.                   */
/* -------------------------------------------------------------------- */
    if( GetOption( S57O_RETURN_DSID ) == NULL 
        || CSLTestBoolean(GetOption( S57O_RETURN_DSID )) )
    {
        OGRFeatureDefn  *poDefn;

        poDefn = S57GenerateDSIDFeatureDefn();
        AddLayer( new OGRS57Layer( this, poDefn ) );
    }

/* -------------------------------------------------------------------- */
/*      Add the primitive layers if they are called for.                */
/* -------------------------------------------------------------------- */
    if( GetOption( S57O_RETURN_PRIMITIVES ) != NULL )
    {
        OGRFeatureDefn  *poDefn;

        poDefn = S57GenerateVectorPrimitiveFeatureDefn( RCNM_VI, poModule->GetOptionFlags());
        AddLayer( new OGRS57Layer( this, poDefn ) );

        poDefn = S57GenerateVectorPrimitiveFeatureDefn( RCNM_VC, poModule->GetOptionFlags());
        AddLayer( new OGRS57Layer( this, poDefn ) );

        poDefn = S57GenerateVectorPrimitiveFeatureDefn( RCNM_VE, poModule->GetOptionFlags());
        AddLayer( new OGRS57Layer( this, poDefn ) );

        poDefn = S57GenerateVectorPrimitiveFeatureDefn( RCNM_VF, poModule->GetOptionFlags());
        AddLayer( new OGRS57Layer( this, poDefn ) );
    }

/* -------------------------------------------------------------------- */
/*      Initialize a layer for each type of geometry.  Eventually       */
/*      we will do this by object class.                                */
/* -------------------------------------------------------------------- */
    if( OGRS57Driver::GetS57Registrar() == NULL )
    {
        OGRFeatureDefn  *poDefn;

        poDefn = S57GenerateGeomFeatureDefn( wkbPoint, 
                                             poModule->GetOptionFlags() );
        AddLayer( new OGRS57Layer( this, poDefn ) );
    
        poDefn = S57GenerateGeomFeatureDefn( wkbLineString, 
                                             poModule->GetOptionFlags() );
        AddLayer( new OGRS57Layer( this, poDefn ) );
    
        poDefn = S57GenerateGeomFeatureDefn( wkbPolygon, 
                                             poModule->GetOptionFlags() );
        AddLayer( new OGRS57Layer( this, poDefn ) );
    
        poDefn = S57GenerateGeomFeatureDefn( wkbNone, 
                                             poModule->GetOptionFlags() );
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
        int             iClass, bGeneric = FALSE;
        
        poClassContentExplorer =
            new S57ClassContentExplorer( OGRS57Driver::GetS57Registrar() );

        for( iModule = 0; iModule < nModules; iModule++ )
            papoModules[iModule]->SetClassBased( OGRS57Driver::GetS57Registrar(),
                                                 poClassContentExplorer );
        
        panClassCount = (int *) CPLCalloc(sizeof(int),MAX_CLASSES);

        for( iModule = 0; iModule < nModules; iModule++ )
        {
            bSuccess &= 
                papoModules[iModule]->CollectClassList(panClassCount,
                                                       MAX_CLASSES);
        }

        for( iClass = 0; iClass < MAX_CLASSES; iClass++ )
        {
            if( panClassCount[iClass] > 0 )
            {
                poDefn = 
                    S57GenerateObjectClassDefn( OGRS57Driver::GetS57Registrar(),
                                                poClassContentExplorer,
                                                iClass, 
                                                poModule->GetOptionFlags() );

                if( poDefn != NULL )
                    AddLayer( new OGRS57Layer( this, poDefn, 
                                               panClassCount[iClass] ) );
                else
                {
                    bGeneric = TRUE;
                    CPLDebug( "S57", 
                              "Unable to find definition for OBJL=%d\n", 
                              iClass );
                }
            }
        }

        if( bGeneric )
        {
            poDefn = S57GenerateGeomFeatureDefn( wkbUnknown, 
                                                 poModule->GetOptionFlags() );
            AddLayer( new OGRS57Layer( this, poDefn ) );
        }
            
        CPLFree( panClassCount );
    }

/* -------------------------------------------------------------------- */
/*      Attach the layer definitions to each of the readers.            */
/* -------------------------------------------------------------------- */
    for( iModule = 0; iModule < nModules; iModule++ )
    {
        for( int iLayer = 0; iLayer < nLayers; iLayer++ )
        {
            papoModules[iModule]->AddFeatureDefn(
                papoLayers[iLayer]->GetLayerDefn() );
        }
    }
    
    return bSuccess;
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

/************************************************************************/
/*                            GetDSExtent()                             */
/************************************************************************/

OGRErr OGRS57DataSource::GetDSExtent( OGREnvelope *psExtent, int bForce )

{
/* -------------------------------------------------------------------- */
/*      If we have it, return it immediately.                           */
/* -------------------------------------------------------------------- */
    if( bExtentsSet )
    {
        *psExtent = oExtents;
        return OGRERR_NONE;
    }

    if( nModules == 0 )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Otherwise try asking each of the readers for it.                */
/* -------------------------------------------------------------------- */
    for( int iModule=0; iModule < nModules; iModule++ )
    {
        OGREnvelope     oModuleEnvelope;
        OGRErr          eErr;

        eErr = papoModules[iModule]->GetExtent( &oModuleEnvelope, bForce );
        if( eErr != OGRERR_NONE )
            return eErr;

        if( iModule == 0 )
            oExtents = oModuleEnvelope;
        else
        {
            oExtents.MinX = MIN(oExtents.MinX,oModuleEnvelope.MinX);
            oExtents.MaxX = MAX(oExtents.MaxX,oModuleEnvelope.MaxX);
            oExtents.MinY = MIN(oExtents.MinY,oModuleEnvelope.MinY);
            oExtents.MaxX = MAX(oExtents.MaxY,oModuleEnvelope.MaxY);
        }
    }

    *psExtent = oExtents;
    bExtentsSet = TRUE;

    return OGRERR_NONE;
}

/************************************************************************/
/*                               Create()                               */
/*                                                                      */
/*      Create a new S57 file, and represent it as a datasource.        */
/************************************************************************/

int OGRS57DataSource::Create( const char *pszFilename, char **papszOptions )

{
/* -------------------------------------------------------------------- */
/*      Instantiate the class registrar if possible.                    */
/* -------------------------------------------------------------------- */
    if( OGRS57Driver::GetS57Registrar() == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to load s57objectclasses.csv, unable to continue." );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Create the S-57 file with definition record.                    */
/* -------------------------------------------------------------------- */
    poWriter = new S57Writer();

    if( !poWriter->CreateS57File( pszFilename ) )
        return FALSE;

    poClassContentExplorer =
        new S57ClassContentExplorer( OGRS57Driver::GetS57Registrar() );

    poWriter->SetClassBased( OGRS57Driver::GetS57Registrar(),
                             poClassContentExplorer );
    pszName = CPLStrdup( pszFilename );

/* -------------------------------------------------------------------- */
/*      Add the primitive layers if they are called for.                */
/* -------------------------------------------------------------------- */
    OGRFeatureDefn  *poDefn;
    int nOptionFlags = S57M_RETURN_LINKAGES | S57M_LNAM_REFS;

    poDefn = S57GenerateVectorPrimitiveFeatureDefn( RCNM_VI, nOptionFlags );
    AddLayer( new OGRS57Layer( this, poDefn ) );
    
    poDefn = S57GenerateVectorPrimitiveFeatureDefn( RCNM_VC, nOptionFlags );
    AddLayer( new OGRS57Layer( this, poDefn ) );
    
    poDefn = S57GenerateVectorPrimitiveFeatureDefn( RCNM_VE, nOptionFlags );
    AddLayer( new OGRS57Layer( this, poDefn ) );
    
    poDefn = S57GenerateVectorPrimitiveFeatureDefn( RCNM_VF, nOptionFlags );
    AddLayer( new OGRS57Layer( this, poDefn ) );

/* -------------------------------------------------------------------- */
/*      Initialize a feature definition for each object class.          */
/* -------------------------------------------------------------------- */
    for( int iClass = 0; iClass < MAX_CLASSES; iClass++ )
    {
        poDefn = 
            S57GenerateObjectClassDefn( OGRS57Driver::GetS57Registrar(), 
                                        poClassContentExplorer,
                                        iClass, nOptionFlags );
        
        if( poDefn == NULL )
            continue;

        AddLayer( new OGRS57Layer( this, poDefn, 0, iClass ) );
    }

/* -------------------------------------------------------------------- */
/*      Write out "header" records.                                     */
/* -------------------------------------------------------------------- */
    poWriter->WriteDSID( pszFilename, "20010409", "03.1", 540, "" );

    poWriter->WriteDSPM();


    return TRUE;
}
