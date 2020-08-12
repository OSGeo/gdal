/******************************************************************************
 *
 * Project:  S-57 Translator
 * Purpose:  Implements OGRS57DataSource class
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_s57.h"

#include <algorithm>
#include <set>

CPL_CVSID("$Id$")

/************************************************************************/
/*                          OGRS57DataSource()                          */
/************************************************************************/

OGRS57DataSource::OGRS57DataSource(char** papszOpenOptionsIn) :
    pszName(nullptr),
    nLayers(0),
    papoLayers(nullptr),
    poSpatialRef(new OGRSpatialReference()),
    papszOptions(nullptr),
    nModules(0),
    papoModules(nullptr),
    poWriter(nullptr),
    poClassContentExplorer(nullptr),
    bExtentsSet(false)
{
    poSpatialRef->SetWellKnownGeogCS( "WGS84" );
    poSpatialRef->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

/* -------------------------------------------------------------------- */
/*      Allow initialization of options from the environment.           */
/* -------------------------------------------------------------------- */
    const char *pszOptString = CPLGetConfigOption( "OGR_S57_OPTIONS", nullptr );

    if ( pszOptString != nullptr )
    {
        papszOptions =
            CSLTokenizeStringComplex( pszOptString, ",", FALSE, FALSE );

        if ( papszOptions && *papszOptions )
        {
            CPLDebug( "S57", "The following S57 options are being set:" );
            char **papszCurOption = papszOptions;
            while( *papszCurOption )
                CPLDebug( "S57", "    %s", *papszCurOption++ );
        }
    }

/* -------------------------------------------------------------------- */
/*      And from open options.                                          */
/* -------------------------------------------------------------------- */
    for(char** papszIter = papszOpenOptionsIn; papszIter && *papszIter; ++papszIter )
    {
        char* pszKey = nullptr;
        const char* pszValue = CPLParseNameValue(*papszIter, &pszKey);
        if( pszKey && pszValue )
        {
            papszOptions = CSLSetNameValue(papszOptions, pszKey, pszValue);
        }
        CPLFree(pszKey);
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

    if( poWriter != nullptr )
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
    pszName = CPLStrdup( pszFilename );

/* -------------------------------------------------------------------- */
/*      Setup reader options.                                           */
/* -------------------------------------------------------------------- */
    char **papszReaderOptions = nullptr;

    if( GetOption(S57O_LNAM_REFS) == nullptr )
        papszReaderOptions = CSLSetNameValue( papszReaderOptions,
                                              S57O_LNAM_REFS, "ON" );
    else
        papszReaderOptions =
            CSLSetNameValue( papszReaderOptions, S57O_LNAM_REFS,
                             GetOption(S57O_LNAM_REFS) );

    if( GetOption(S57O_UPDATES) != nullptr )
        papszReaderOptions =
            CSLSetNameValue( papszReaderOptions, S57O_UPDATES,
                             GetOption(S57O_UPDATES));

    if( GetOption(S57O_SPLIT_MULTIPOINT) != nullptr )
        papszReaderOptions =
            CSLSetNameValue( papszReaderOptions, S57O_SPLIT_MULTIPOINT,
                             GetOption(S57O_SPLIT_MULTIPOINT) );

    if( GetOption(S57O_ADD_SOUNDG_DEPTH) != nullptr )
        papszReaderOptions =
            CSLSetNameValue( papszReaderOptions, S57O_ADD_SOUNDG_DEPTH,
                             GetOption(S57O_ADD_SOUNDG_DEPTH));

    if( GetOption(S57O_PRESERVE_EMPTY_NUMBERS) != nullptr )
        papszReaderOptions =
            CSLSetNameValue( papszReaderOptions, S57O_PRESERVE_EMPTY_NUMBERS,
                             GetOption(S57O_PRESERVE_EMPTY_NUMBERS) );

    if( GetOption(S57O_RETURN_PRIMITIVES) != nullptr )
        papszReaderOptions =
            CSLSetNameValue( papszReaderOptions, S57O_RETURN_PRIMITIVES,
                             GetOption(S57O_RETURN_PRIMITIVES) );

    if( GetOption(S57O_RETURN_LINKAGES) != nullptr )
        papszReaderOptions =
            CSLSetNameValue( papszReaderOptions, S57O_RETURN_LINKAGES,
                             GetOption(S57O_RETURN_LINKAGES) );

    if( GetOption(S57O_RETURN_DSID) != nullptr )
        papszReaderOptions =
            CSLSetNameValue( papszReaderOptions, S57O_RETURN_DSID,
                             GetOption(S57O_RETURN_DSID) );

    if( GetOption(S57O_RECODE_BY_DSSI) != nullptr )
        papszReaderOptions =
            CSLSetNameValue( papszReaderOptions, S57O_RECODE_BY_DSSI,
                             GetOption(S57O_RECODE_BY_DSSI) );

    if( GetOption(S57O_LIST_AS_STRING) != nullptr )
        papszReaderOptions =
            CSLSetNameValue( papszReaderOptions, S57O_LIST_AS_STRING,
                             GetOption(S57O_LIST_AS_STRING) );

    S57Reader *poModule = new S57Reader( pszFilename );
    bool bRet = poModule->SetOptions( papszReaderOptions );
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

    bool bSuccess = true;

    nModules = 1;
    papoModules = static_cast<S57Reader **>( CPLMalloc(sizeof(void*)) );
    papoModules[0] = poModule;

/* -------------------------------------------------------------------- */
/*      Add the header layers if they are called for.                   */
/* -------------------------------------------------------------------- */
    if( GetOption( S57O_RETURN_DSID ) == nullptr
        || CPLTestBool(GetOption( S57O_RETURN_DSID )) )
    {
        OGRFeatureDefn  *poDefn = S57GenerateDSIDFeatureDefn();
        AddLayer( new OGRS57Layer( this, poDefn ) );
    }

/* -------------------------------------------------------------------- */
/*      Add the primitive layers if they are called for.                */
/* -------------------------------------------------------------------- */
    if( GetOption( S57O_RETURN_PRIMITIVES ) != nullptr )
    {
        OGRFeatureDefn  *poDefn
            = S57GenerateVectorPrimitiveFeatureDefn(
                RCNM_VI, poModule->GetOptionFlags());
        AddLayer( new OGRS57Layer( this, poDefn ) );

        poDefn = S57GenerateVectorPrimitiveFeatureDefn(
            RCNM_VC, poModule->GetOptionFlags());
        AddLayer( new OGRS57Layer( this, poDefn ) );

        poDefn = S57GenerateVectorPrimitiveFeatureDefn(
            RCNM_VE, poModule->GetOptionFlags());
        AddLayer( new OGRS57Layer( this, poDefn ) );

        poDefn = S57GenerateVectorPrimitiveFeatureDefn(
            RCNM_VF, poModule->GetOptionFlags());
        AddLayer( new OGRS57Layer( this, poDefn ) );
    }

/* -------------------------------------------------------------------- */
/*      Initialize a layer for each type of geometry.  Eventually       */
/*      we will do this by object class.                                */
/* -------------------------------------------------------------------- */
    if( OGRS57Driver::GetS57Registrar() == nullptr )
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
        poClassContentExplorer =
            new S57ClassContentExplorer( OGRS57Driver::GetS57Registrar() );

        for( int iModule = 0; iModule < nModules; iModule++ )
            papoModules[iModule]->SetClassBased(
                OGRS57Driver::GetS57Registrar(), poClassContentExplorer );

        std::vector<int> anClassCount;

        for( int iModule = 0; iModule < nModules; iModule++ )
        {
            bSuccess &= CPL_TO_BOOL(
                papoModules[iModule]->CollectClassList(anClassCount) );
        }

        bool bGeneric = false;

        for( unsigned int iClass = 0; iClass < anClassCount.size(); iClass++ )
        {
            if( anClassCount[iClass] > 0 )
            {
                OGRFeatureDefn  *poDefn =
                    S57GenerateObjectClassDefn( OGRS57Driver::GetS57Registrar(),
                                                poClassContentExplorer,
                                                iClass,
                                                poModule->GetOptionFlags() );

                if( poDefn != nullptr )
                    AddLayer( new OGRS57Layer( this, poDefn,
                                               anClassCount[iClass] ) );
                else
                {
                    bGeneric = true;
                    CPLDebug( "S57",
                              "Unable to find definition for OBJL=%d\n",
                              iClass );
                }
            }
        }

        if( bGeneric )
        {
            OGRFeatureDefn  *poDefn
                = S57GenerateGeomFeatureDefn( wkbUnknown,
                                              poModule->GetOptionFlags() );
            AddLayer( new OGRS57Layer( this, poDefn ) );
        }
    }

/* -------------------------------------------------------------------- */
/*      Attach the layer definitions to each of the readers.            */
/* -------------------------------------------------------------------- */
    for( int iModule = 0; iModule < nModules; iModule++ )
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
        return nullptr;

    return papoLayers[iLayer];
}

/************************************************************************/
/*                              AddLayer()                              */
/************************************************************************/

void OGRS57DataSource::AddLayer( OGRS57Layer * poNewLayer )

{
    papoLayers = static_cast<OGRS57Layer **> (
        CPLRealloc( papoLayers, sizeof(void*) * ++nLayers ) );

    papoLayers[nLayers-1] = poNewLayer;
}

/************************************************************************/
/*                             GetModule()                              */
/************************************************************************/

S57Reader * OGRS57DataSource::GetModule( int i )

{
    if( i < 0 || i >= nModules )
        return nullptr;

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
            oExtents.MinX = std::min(oExtents.MinX, oModuleEnvelope.MinX);
            oExtents.MaxX = std::max(oExtents.MaxX, oModuleEnvelope.MaxX);
            oExtents.MinY = std::min(oExtents.MinY, oModuleEnvelope.MinY);
            oExtents.MaxX = std::max(oExtents.MaxY, oModuleEnvelope.MaxY);
        }
    }

    *psExtent = oExtents;
    bExtentsSet = true;

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
    if( OGRS57Driver::GetS57Registrar() == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to load s57objectclasses.csv.  Unable to continue." );
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
    int nOptionFlags = S57M_RETURN_LINKAGES | S57M_LNAM_REFS;

    OGRFeatureDefn *poDefn
        = S57GenerateVectorPrimitiveFeatureDefn( RCNM_VI, nOptionFlags );
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
    std::set<int> aoSetOBJL;
    while( poClassContentExplorer->NextClass() )
    {
        const int nOBJL = poClassContentExplorer->GetOBJL();
        // Detect potential duplicates in the classes
        if( aoSetOBJL.find(nOBJL) != aoSetOBJL.end() )
        {
            CPLDebug("S57", "OBJL %d already registered!", nOBJL);
            continue;
        }
        aoSetOBJL.insert(nOBJL);
        poDefn =
            S57GenerateObjectClassDefn( OGRS57Driver::GetS57Registrar(),
                                        poClassContentExplorer,
                                        nOBJL,
                                        nOptionFlags );

        AddLayer( new OGRS57Layer( this, poDefn, 0, nOBJL ) );
    }

/* -------------------------------------------------------------------- */
/*      Write out "header" records.                                     */
/* -------------------------------------------------------------------- */
    int nEXPP = atoi(CSLFetchNameValueDef( papszOptionsIn, "S57_EXPP", CPLSPrintf("%d", S57Writer::nDEFAULT_EXPP) ));
    int nINTU = atoi(CSLFetchNameValueDef( papszOptionsIn, "S57_INTU", CPLSPrintf("%d", S57Writer::nDEFAULT_INTU) ));
    const char *pszEDTN = CSLFetchNameValue( papszOptionsIn, "S57_EDTN" );
    const char *pszUPDN = CSLFetchNameValue( papszOptionsIn, "S57_UPDN" );
    const char *pszUADT = CSLFetchNameValue( papszOptionsIn, "S57_UADT" );
    const char *pszISDT = CSLFetchNameValue( papszOptionsIn, "S57_ISDT" );
    const char *pszSTED = CSLFetchNameValue( papszOptionsIn, "S57_STED" );
    int nAGEN = atoi(CSLFetchNameValueDef( papszOptionsIn, "S57_AGEN", CPLSPrintf("%d", S57Writer::nDEFAULT_AGEN) ));
    const char *pszCOMT = CSLFetchNameValue( papszOptionsIn, "S57_COMT" );
    int nAALL = atoi(CSLFetchNameValueDef( papszOptionsIn, "S57_AALL", "0" ));
    int nNALL = atoi(CSLFetchNameValueDef( papszOptionsIn, "S57_NALL", "0" ));
    int nNOMR = atoi(CSLFetchNameValueDef( papszOptionsIn, "S57_NOMR", "0" ));
    int nNOGR = atoi(CSLFetchNameValueDef( papszOptionsIn, "S57_NOGR", "0" ));
    int nNOLR = atoi(CSLFetchNameValueDef( papszOptionsIn, "S57_NOLR", "0" ));
    int nNOIN = atoi(CSLFetchNameValueDef( papszOptionsIn, "S57_NOIN", "0" ));
    int nNOCN = atoi(CSLFetchNameValueDef( papszOptionsIn, "S57_NOCN", "0" ));
    int nNOED = atoi(CSLFetchNameValueDef( papszOptionsIn, "S57_NOED", "0" ));
    poWriter->WriteDSID( nEXPP, nINTU, CPLGetFilename( pszFilename ),
                         pszEDTN, pszUPDN, pszUADT, pszISDT, pszSTED, nAGEN,
                         pszCOMT,
                         nAALL,
                         nNALL,
                         nNOMR, nNOGR, nNOLR, nNOIN, nNOCN, nNOED );

    int nHDAT = atoi(CSLFetchNameValueDef( papszOptionsIn, "S57_HDAT", CPLSPrintf("%d", S57Writer::nDEFAULT_HDAT) ));
    int nVDAT = atoi(CSLFetchNameValueDef( papszOptionsIn, "S57_VDAT", CPLSPrintf("%d", S57Writer::nDEFAULT_VDAT) ));
    int nSDAT = atoi(CSLFetchNameValueDef( papszOptionsIn, "S57_SDAT", CPLSPrintf("%d", S57Writer::nDEFAULT_SDAT) ));
    int nCSCL = atoi(CSLFetchNameValueDef( papszOptionsIn, "S57_CSCL", CPLSPrintf("%d", S57Writer::nDEFAULT_CSCL) ));
    int nCOMF = atoi(CSLFetchNameValueDef( papszOptionsIn, "S57_COMF", CPLSPrintf("%d", S57Writer::nDEFAULT_COMF) ));
    int nSOMF = atoi(CSLFetchNameValueDef( papszOptionsIn, "S57_SOMF", CPLSPrintf("%d", S57Writer::nDEFAULT_SOMF) ));
    poWriter->WriteDSPM(nHDAT, nVDAT, nSDAT, nCSCL, nCOMF, nSOMF);

    return TRUE;
}
