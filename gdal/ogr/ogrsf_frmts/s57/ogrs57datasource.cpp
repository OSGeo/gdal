/******************************************************************************
 * $Id$
 *
 * Project:  S-57 Translator
 * Purpose:  Implements OGRS57DataSource class
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

OGRS57DataSource::OGRS57DataSource(char** papszOpenOptionsIn)

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
    if( papszOpenOptionsIn != NULL )
    {
        papszOptions = CSLDuplicate(papszOpenOptionsIn);
    }
    else
    {
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
}

/************************************************************************/
/*                         ~OGRS57DataSource()                          */
/************************************************************************/

OGRS57DataSource::~OGRS57DataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];

    CPLFree( papoLayers );

    for( int i = 0; i < nModules; i++ )
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

int OGRS57DataSource::Open( const char * pszFilename )

{
    int         iModule;

    pszName = CPLStrdup( pszFilename );

/* -------------------------------------------------------------------- */
/*      Setup reader options.                                           */
/* -------------------------------------------------------------------- */
    char **papszReaderOptions = NULL;
    S57Reader   *poModule = new S57Reader( pszFilename );

    if( GetOption(S57O_LNAM_REFS) == NULL )
        papszReaderOptions = CSLSetNameValue(papszReaderOptions, 
                                         S57O_LNAM_REFS, "ON" );
    else
        papszReaderOptions = 
            CSLSetNameValue( papszReaderOptions, S57O_LNAM_REFS, 
                             GetOption(S57O_LNAM_REFS));

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

    int bRet = poModule->SetOptions( papszReaderOptions );
    CSLDestroy( papszReaderOptions );

    if( !bRet )
    {
        delete poModule;
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Try opening.                                                    */
/*                                                                      */
/*      Eventually this should check for catalogs, and if found         */
/*      instantiate a whole series of modules.                          */
/* -------------------------------------------------------------------- */
    if( !poModule->Open( TRUE ) )
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
        OGRFeatureDefn  *poDefn = S57GenerateDSIDFeatureDefn();
        AddLayer( new OGRS57Layer( this, poDefn ) );
    }

/* -------------------------------------------------------------------- */
/*      Add the primitive layers if they are called for.                */
/* -------------------------------------------------------------------- */
    if( GetOption( S57O_RETURN_PRIMITIVES ) != NULL )
    {
        OGRFeatureDefn  *poDefn
            = S57GenerateVectorPrimitiveFeatureDefn( RCNM_VI, poModule->GetOptionFlags());
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
        OGRFeatureDefn  *poDefn
            = S57GenerateGeomFeatureDefn( wkbPoint,
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
        std::vector<int> anClassCount;
        int              bGeneric = FALSE;
        unsigned int     iClass;

        poClassContentExplorer =
            new S57ClassContentExplorer( OGRS57Driver::GetS57Registrar() );

        for( iModule = 0; iModule < nModules; iModule++ )
            papoModules[iModule]->SetClassBased( OGRS57Driver::GetS57Registrar(),
                                                 poClassContentExplorer );

        for( iModule = 0; iModule < nModules; iModule++ )
        {
            bSuccess &= 
                papoModules[iModule]->CollectClassList(anClassCount);
        }

        for( iClass = 0; iClass < anClassCount.size(); iClass++ )
        {
            if( anClassCount[iClass] > 0 )
            {
                poDefn = 
                    S57GenerateObjectClassDefn( OGRS57Driver::GetS57Registrar(),
                                                poClassContentExplorer,
                                                iClass, 
                                                poModule->GetOptionFlags() );

                if( poDefn != NULL )
                    AddLayer( new OGRS57Layer( this, poDefn, 
                                               anClassCount[iClass] ) );
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

        OGRErr eErr
            = papoModules[iModule]->GetExtent( &oModuleEnvelope, bForce );
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

int OGRS57DataSource::Create( const char *pszFilename,
                              char **papszOptionsIn )
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
    poClassContentExplorer->Rewind();
    while( poClassContentExplorer->NextClass() )
    {
        poDefn = 
            S57GenerateObjectClassDefn( OGRS57Driver::GetS57Registrar(), 
                                        poClassContentExplorer,
                                        poClassContentExplorer->GetOBJL(),
                                        nOptionFlags );

        AddLayer( new OGRS57Layer( this, poDefn, 0, 
                                   poClassContentExplorer->GetOBJL() ) );
    }

/* -------------------------------------------------------------------- */
/*      Write out "header" records.                                     */
/* -------------------------------------------------------------------- */
    int nEXPP = 1, nINTU = 4, nAGEN = 540, nNOMR = 0, nNOGR = 0,
        nNOLR = 0, nNOIN = 0, nNOCN = 0, nNOED = 0;
    const char
        *pszEXPP = CSLFetchNameValue(papszOptionsIn, "S57_EXPP"),
        *pszINTU = CSLFetchNameValue(papszOptionsIn, "S57_INTU"),
        *pszEDTN = CSLFetchNameValue(papszOptionsIn, "S57_EDTN"),
        *pszUPDN = CSLFetchNameValue(papszOptionsIn, "S57_UPDN"),
        *pszUADT = CSLFetchNameValue(papszOptionsIn, "S57_UADT"),
        *pszISDT = CSLFetchNameValue(papszOptionsIn, "S57_ISDT"),
        *pszSTED = CSLFetchNameValue(papszOptionsIn, "S57_STED"),
        *pszAGEN = CSLFetchNameValue(papszOptionsIn, "S57_AGEN"),
        *pszCOMT = CSLFetchNameValue(papszOptionsIn, "S57_COMT"),
        *pszNOMR = CSLFetchNameValue(papszOptionsIn, "S57_NOMR"),
        *pszNOGR = CSLFetchNameValue(papszOptionsIn, "S57_NOGR"),
        *pszNOLR = CSLFetchNameValue(papszOptionsIn, "S57_NOLR"),
        *pszNOIN = CSLFetchNameValue(papszOptionsIn, "S57_NOIN"),
        *pszNOCN = CSLFetchNameValue(papszOptionsIn, "S57_NOCN"),
        *pszNOED = CSLFetchNameValue(papszOptionsIn, "S57_NOED");
    if (pszEXPP) nEXPP = atoi(pszEXPP);
    if (pszINTU) nINTU = atoi(pszINTU);
    if (pszAGEN) nAGEN = atoi(pszAGEN);
    if (pszNOMR) nNOMR = atoi(pszNOMR);
    if (pszNOGR) nNOGR = atoi(pszNOGR);
    if (pszNOLR) nNOLR = atoi(pszNOLR);
    if (pszNOIN) nNOIN = atoi(pszNOIN);
    if (pszNOCN) nNOCN = atoi(pszNOCN);
    if (pszNOED) nNOED = atoi(pszNOED);
    poWriter->WriteDSID( nEXPP, nINTU, CPLGetFilename( pszFilename ),
                         pszEDTN, pszUPDN, pszUADT, pszISDT, pszSTED, nAGEN,
                         pszCOMT, nNOMR, nNOGR, nNOLR, nNOIN, nNOCN, nNOED );

    int nHDAT = 2, nVDAT = 17, nSDAT = 23, nCSCL = 52000;
    const char
        *pszHDAT = CSLFetchNameValue(papszOptionsIn, "S57_HDAT"),
        *pszVDAT = CSLFetchNameValue(papszOptionsIn, "S57_VDAT"),
        *pszSDAT = CSLFetchNameValue(papszOptionsIn, "S57_SDAT"),
        *pszCSCL = CSLFetchNameValue(papszOptionsIn, "S57_CSCL");
    if (pszHDAT)
        nHDAT = atoi(pszHDAT);
    if (pszVDAT)
        nVDAT = atoi(pszVDAT);
    if (pszSDAT)
        nSDAT = atoi(pszSDAT);
    if (pszCSCL)
        nCSCL = atoi(pszCSCL);
    poWriter->WriteDSPM(nHDAT, nVDAT, nSDAT, nCSCL);

    return TRUE;
}
