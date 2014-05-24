/******************************************************************************
 * $Id$
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

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGRDGNDataSource()                           */
/************************************************************************/

OGRDGNDataSource::OGRDGNDataSource()

{
    papoLayers = NULL;
    nLayers = 0;
    hDGN = NULL;
    pszName = NULL;
    papszOptions = NULL;
}

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

    if( hDGN != NULL )
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
        FILE    *fp;
        GByte   abyHeader[512];
        int     nHeaderBytes = 0;

        fp = VSIFOpen( pszNewName, "rb" );
        if( fp == NULL )
            return FALSE;

        nHeaderBytes = (int) VSIFRead( abyHeader, 1, sizeof(abyHeader), fp );

        VSIFClose( fp );

        if( nHeaderBytes < 512 )
            return FALSE;

        if( !DGNTestOpen( abyHeader, nHeaderBytes ) )
            return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Try to open the file as a DGN file.                             */
/* -------------------------------------------------------------------- */
    hDGN = DGNOpen( pszNewName, bUpdate );
    if( hDGN == NULL )
    {
        if( !bTestOpen )
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Unable to open %s as a Microstation .dgn file.\n", 
                      pszNewName );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRDGNLayer *poLayer;

    poLayer = new OGRDGNLayer( "elements", hDGN, bUpdate );
    pszName = CPLStrdup( pszNewName );

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRDGNLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRDGNLayer *) * (nLayers+1) );
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
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRDGNDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}


/************************************************************************/
/*                             PreCreate()                              */
/*                                                                      */
/*      Called by OGRDGNDriver::Create() method to setup a stub         */
/*      OGRDataSource object without the associated file created        */
/*      yet.  It will be created by theICreateLayer() call.             */
/************************************************************************/

int OGRDGNDataSource::PreCreate( const char *pszFilename, 
                                 char **papszOptions )

{
    this->papszOptions = CSLDuplicate( papszOptions );
    pszName = CPLStrdup( pszFilename );

    return TRUE;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *OGRDGNDataSource::ICreateLayer( const char *pszLayerName, 
                                         OGRSpatialReference *poSRS, 
                                         OGRwkbGeometryType eGeomType, 
                                         char **papszExtraOptions )

{
    const char *pszSeed, *pszMasterUnit = "m", *pszSubUnit = "cm";
    const char *pszValue;
    int nUORPerSU=1, nSUPerMU=100;
    int nCreationFlags = 0, b3DRequested;
    double dfOriginX = -21474836.0,  /* default origin centered on zero */
           dfOriginY = -21474836.0,  /* with two decimals of precision */
           dfOriginZ = -21474836.0;

/* -------------------------------------------------------------------- */
/*      Ensure only one layer gets created.                             */
/* -------------------------------------------------------------------- */
    if( nLayers > 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "DGN driver only supports one layer will all the elements in it." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      If the coordinate system is geographic, we should use a         */
/*      localized default origin and resolution.                        */
/* -------------------------------------------------------------------- */
    if( poSRS != NULL && poSRS->IsGeographic() )
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

    b3DRequested = CSLFetchBoolean( papszOptions, "3D", 
                                    (((int) eGeomType) & wkb25DBit) );

    pszSeed = CSLFetchNameValue( papszOptions, "SEED" );
    if( pszSeed )
        nCreationFlags |= DGNCF_USE_SEED_ORIGIN | DGNCF_USE_SEED_UNITS;
    else if( b3DRequested )
        pszSeed = CPLFindFile( "gdal", "seed_3d.dgn" );
    else
        pszSeed = CPLFindFile( "gdal", "seed_2d.dgn" );

    if( pszSeed == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "No seed file provided, and unable to find seed_2d.dgn." );
        return NULL;
    }
    
    if( CSLFetchBoolean( papszOptions, "COPY_WHOLE_SEED_FILE", TRUE ) )
        nCreationFlags |= DGNCF_COPY_WHOLE_SEED_FILE;
    if( CSLFetchBoolean( papszOptions, "COPY_SEED_FILE_COLOR_TABLE", TRUE ) )
        nCreationFlags |= DGNCF_COPY_SEED_FILE_COLOR_TABLE;
    
    pszValue = CSLFetchNameValue( papszOptions, "MASTER_UNIT_NAME" );
    if( pszValue != NULL )
    {
        nCreationFlags &= ~DGNCF_USE_SEED_UNITS;
        pszMasterUnit = pszValue;
    }
    
    pszValue = CSLFetchNameValue( papszOptions, "SUB_UNIT_NAME" );
    if( pszValue != NULL )
    {
        nCreationFlags &= ~DGNCF_USE_SEED_UNITS;
        pszSubUnit = pszValue;
    }


    pszValue = CSLFetchNameValue( papszOptions, "SUB_UNITS_PER_MASTER_UNIT" );
    if( pszValue != NULL )
    {
        nCreationFlags &= ~DGNCF_USE_SEED_UNITS;
        nSUPerMU = atoi(pszValue);
    }

    pszValue = CSLFetchNameValue( papszOptions, "UOR_PER_SUB_UNIT" );
    if( pszValue != NULL )
    {
        nCreationFlags &= ~DGNCF_USE_SEED_UNITS;
        nUORPerSU = atoi(pszValue);
    }

    pszValue = CSLFetchNameValue( papszOptions, "ORIGIN" );
    if( pszValue != NULL )
    {
        char **papszTuple = CSLTokenizeStringComplex( pszValue, " ,", 
                                                      FALSE, FALSE );

        nCreationFlags &= ~DGNCF_USE_SEED_ORIGIN;
        if( CSLCount(papszTuple) == 3 )
        {
            dfOriginX = atof(papszTuple[0]);
            dfOriginY = atof(papszTuple[1]);
            dfOriginZ = atof(papszTuple[2]);
        }
        else if( CSLCount(papszTuple) == 2 )
        {
            dfOriginX = atof(papszTuple[0]);
            dfOriginY = atof(papszTuple[1]);
            dfOriginZ = 0.0;
        }
        else
        {
            CSLDestroy(papszTuple);
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "ORIGIN is not a valid 2d or 3d tuple.\n"
                      "Separate tuple values with comma." );
            return FALSE;
        }
        CSLDestroy(papszTuple);
    }

/* -------------------------------------------------------------------- */
/*      Try creating the base file.                                     */
/* -------------------------------------------------------------------- */
    hDGN = DGNCreate( pszName, pszSeed, nCreationFlags, 
                      dfOriginX, dfOriginY, dfOriginZ, 
                      nSUPerMU, nUORPerSU, pszMasterUnit, pszSubUnit );
    if( hDGN == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRDGNLayer *poLayer;

    poLayer = new OGRDGNLayer( pszLayerName, hDGN, TRUE );

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRDGNLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRDGNLayer *) * (nLayers+1) );
    papoLayers[nLayers++] = poLayer;
    
    return poLayer;
}
