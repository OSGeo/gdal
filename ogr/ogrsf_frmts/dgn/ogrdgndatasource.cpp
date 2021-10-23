/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam (warmerdam@pobox.com)
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

#include "ogr_dgn.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                         OGRDGNDataSource()                           */
/************************************************************************/

OGRDGNDataSource::OGRDGNDataSource() :
    papoLayers(nullptr),
    nLayers(0),
    pszName(nullptr),
    hDGN(nullptr),
    papszOptions(nullptr)
{}

/************************************************************************/
/*                        ~OGRDGNDataSource()                           */
/************************************************************************/

OGRDGNDataSource::~OGRDGNDataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];

    CPLFree( papoLayers );
    CPLFree( pszName );
    CSLDestroy( papszOptions );

    if( hDGN != nullptr )
        DGNClose( hDGN );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRDGNDataSource::Open( const char * pszNewName,
                            int bTestOpen,
                            int bUpdate )

{
    CPLAssert( nLayers == 0 );

/* -------------------------------------------------------------------- */
/*      For now we require files to have the .dgn or .DGN               */
/*      extension.  Eventually we will implement a more                 */
/*      sophisticated test to see if it is a dgn file.                  */
/* -------------------------------------------------------------------- */
    if( bTestOpen )
    {

        VSILFILE *fp = VSIFOpenL( pszNewName, "rb" );
        if( fp == nullptr )
            return FALSE;

        GByte abyHeader[512];
        const int nHeaderBytes = static_cast<int>(
            VSIFReadL( abyHeader, 1, sizeof(abyHeader), fp ) );

        VSIFCloseL( fp );

        if( nHeaderBytes < 512 )
            return FALSE;

        if( !DGNTestOpen( abyHeader, nHeaderBytes ) )
            return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Try to open the file as a DGN file.                             */
/* -------------------------------------------------------------------- */
    hDGN = DGNOpen( pszNewName, bUpdate );
    if( hDGN == nullptr )
    {
        if( !bTestOpen )
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unable to open %s as a Microstation .dgn file.",
                      pszNewName );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRDGNLayer *poLayer = new OGRDGNLayer( "elements", hDGN, bUpdate );
    pszName = CPLStrdup( pszNewName );

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = static_cast<OGRDGNLayer **>(
        CPLRealloc( papoLayers,  sizeof(OGRDGNLayer *) * (nLayers+1) ) );
    papoLayers[nLayers++] = poLayer;

    return TRUE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDGNDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRDGNDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return nullptr;

    return papoLayers[iLayer];
}

/************************************************************************/
/*                             PreCreate()                              */
/*                                                                      */
/*      Called by OGRDGNDriver::Create() method to setup a stub         */
/*      OGRDataSource object without the associated file created        */
/*      yet.  It will be created by theICreateLayer() call.             */
/************************************************************************/

bool OGRDGNDataSource::PreCreate( const char *pszFilename,
                                  char **papszOptionsIn )

{
    papszOptions = CSLDuplicate( papszOptionsIn );
    pszName = CPLStrdup( pszFilename );

    return true;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *OGRDGNDataSource::ICreateLayer( const char *pszLayerName,
                                         OGRSpatialReference *poSRS,
                                         OGRwkbGeometryType eGeomType,
                                         char **papszExtraOptions )

{
/* -------------------------------------------------------------------- */
/*      Ensure only one layer gets created.                             */
/* -------------------------------------------------------------------- */
    if( nLayers > 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "DGN driver only supports one layer with all the elements "
                  "in it." );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      If the coordinate system is geographic, we should use a         */
/*      localized default origin and resolution.                        */
/* -------------------------------------------------------------------- */
    const char *pszMasterUnit = "m";
    const char *pszSubUnit = "cm";

    int nUORPerSU = 1;
    int nSUPerMU = 100;

    double dfOriginX = -21474836.0;  // Default origin centered on zero
    double dfOriginY = -21474836.0;  // with two decimals of precision.
    double dfOriginZ = -21474836.0;

    if( poSRS != nullptr && poSRS->IsGeographic() )
    {
        dfOriginX = -200.0;
        dfOriginY = -200.0;

        pszMasterUnit = "d";
        pszSubUnit = "s";
        nSUPerMU = 3600;
        nUORPerSU = 1000;
    }

/* -------------------------------------------------------------------- */
/*      Parse out various creation options.                             */
/* -------------------------------------------------------------------- */
    papszOptions = CSLInsertStrings( papszOptions, 0, papszExtraOptions );

    const bool b3DRequested
        = CPLFetchBool( papszOptions, "3D", wkbHasZ(eGeomType) );

    const char *pszSeed = CSLFetchNameValue( papszOptions, "SEED" );
    int nCreationFlags = 0;
    if( pszSeed )
        nCreationFlags |= DGNCF_USE_SEED_ORIGIN | DGNCF_USE_SEED_UNITS;
    else if( b3DRequested )
        pszSeed = CPLFindFile( "gdal", "seed_3d.dgn" );
    else
        pszSeed = CPLFindFile( "gdal", "seed_2d.dgn" );

    if( pszSeed == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "No seed file provided, and unable to find seed_2d.dgn." );
        return nullptr;
    }

    if( CPLFetchBool( papszOptions, "COPY_WHOLE_SEED_FILE", true ) )
        nCreationFlags |= DGNCF_COPY_WHOLE_SEED_FILE;
    if( CPLFetchBool( papszOptions, "COPY_SEED_FILE_COLOR_TABLE", true ) )
        nCreationFlags |= DGNCF_COPY_SEED_FILE_COLOR_TABLE;

    const char *pszValue
        = CSLFetchNameValue( papszOptions, "MASTER_UNIT_NAME" );
    if( pszValue != nullptr )
    {
        nCreationFlags &= ~DGNCF_USE_SEED_UNITS;
        pszMasterUnit = pszValue;
    }

    pszValue = CSLFetchNameValue( papszOptions, "SUB_UNIT_NAME" );
    if( pszValue != nullptr )
    {
        nCreationFlags &= ~DGNCF_USE_SEED_UNITS;
        pszSubUnit = pszValue;
    }

    pszValue = CSLFetchNameValue( papszOptions, "SUB_UNITS_PER_MASTER_UNIT" );
    if( pszValue != nullptr )
    {
        nCreationFlags &= ~DGNCF_USE_SEED_UNITS;
        nSUPerMU = atoi(pszValue);
    }

    pszValue = CSLFetchNameValue( papszOptions, "UOR_PER_SUB_UNIT" );
    if( pszValue != nullptr )
    {
        nCreationFlags &= ~DGNCF_USE_SEED_UNITS;
        nUORPerSU = atoi(pszValue);
    }

    pszValue = CSLFetchNameValue( papszOptions, "ORIGIN" );
    if( pszValue != nullptr )
    {
        char **papszTuple = CSLTokenizeStringComplex( pszValue, " ,",
                                                      FALSE, FALSE );

        nCreationFlags &= ~DGNCF_USE_SEED_ORIGIN;
        if( CSLCount(papszTuple) == 3 )
        {
            dfOriginX = CPLAtof(papszTuple[0]);
            dfOriginY = CPLAtof(papszTuple[1]);
            dfOriginZ = CPLAtof(papszTuple[2]);
        }
        else if( CSLCount(papszTuple) == 2 )
        {
            dfOriginX = CPLAtof(papszTuple[0]);
            dfOriginY = CPLAtof(papszTuple[1]);
            dfOriginZ = 0.0;
        }
        else
        {
            CSLDestroy(papszTuple);
            CPLError( CE_Failure, CPLE_AppDefined,
                      "ORIGIN is not a valid 2d or 3d tuple.\n"
                      "Separate tuple values with comma." );
            return nullptr;
        }
        CSLDestroy(papszTuple);
    }

/* -------------------------------------------------------------------- */
/*      Try creating the base file.                                     */
/* -------------------------------------------------------------------- */
    hDGN = DGNCreate( pszName, pszSeed, nCreationFlags,
                      dfOriginX, dfOriginY, dfOriginZ,
                      nSUPerMU, nUORPerSU, pszMasterUnit, pszSubUnit );
    if( hDGN == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRDGNLayer *poLayer = new OGRDGNLayer( pszLayerName, hDGN, TRUE );

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = static_cast<OGRDGNLayer **>(
        CPLRealloc( papoLayers,  sizeof(OGRDGNLayer *) * (nLayers+1) ) );
    papoLayers[nLayers++] = poLayer;

    return poLayer;
}
