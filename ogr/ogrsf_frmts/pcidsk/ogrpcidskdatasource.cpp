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
    poFile = NULL;
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
    
    if( poFile != NULL )
    {
        delete poFile;
        poFile = NULL;
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPCIDSKDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return bUpdate;
    else
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
    if( !EQUAL(CPLGetExtension(pszFilename),"pix") )
        return FALSE;

    osName = pszFilename;
    if( bUpdateIn )
        bUpdate = true;
    else
        bUpdate = false;

/* -------------------------------------------------------------------- */
/*      Open the file, and create layer for each vector segment.        */
/* -------------------------------------------------------------------- */
    try 
    {
        PCIDSK::PCIDSKSegment *segobj;
        const char *pszAccess = "r";

        if( bUpdateIn )
            pszAccess = "r+";

        poFile = PCIDSK::Open( pszFilename, pszAccess, NULL );

        for( segobj = poFile->GetSegment( PCIDSK::SEG_VEC, "" );
             segobj != NULL;
             segobj = poFile->GetSegment( PCIDSK::SEG_VEC, "",
                                          segobj->GetSegmentNumber() ) )
        {
            apoLayers.push_back( new OGRPCIDSKLayer( segobj, bUpdate ) );
        }

        /* Check if this is a raster-only PCIDSK file */
        if ( !bUpdate && apoLayers.size() == 0 && poFile->GetChannels() != 0 )
            return FALSE;
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

    return TRUE;
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRPCIDSKDataSource::CreateLayer( const char * pszLayerName,
                                  OGRSpatialReference *poSRS,
                                  OGRwkbGeometryType eType,
                                  char ** papszOptions )
    
{
/* -------------------------------------------------------------------- */
/*      Verify we are in update mode.                                   */
/* -------------------------------------------------------------------- */
    if( !bUpdate )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Data source %s opened read-only.\n"
                  "New layer %s cannot be created.\n",
                  osName.c_str(), pszLayerName );
        
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Figure out what type of layer we need.                          */
/* -------------------------------------------------------------------- */
    std::string osLayerType;

    switch( wkbFlatten(eType) )
    {
      case wkbPoint:
        osLayerType = "POINTS";
        break;
    
      case wkbLineString:
        osLayerType = "ARCS";
        break;

      case wkbPolygon:
        osLayerType = "WHOLE_POLYGONS";
        break;
        
      case wkbNone:
        osLayerType = "TABLE";
        break;

      default:
        break;
    }

/* -------------------------------------------------------------------- */
/*      Create the segment.                                             */
/* -------------------------------------------------------------------- */
    int     nSegNum = poFile->CreateSegment( pszLayerName, "", 
                                             PCIDSK::SEG_VEC, 0L );
    PCIDSK::PCIDSKSegment *poSeg = poFile->GetSegment( nSegNum );
    PCIDSK::PCIDSKVectorSegment *poVecSeg = 
        dynamic_cast<PCIDSK::PCIDSKVectorSegment*>( poSeg );

    if( osLayerType != "" )
        poSeg->SetMetadataValue( "LAYER_TYPE", osLayerType );

/* -------------------------------------------------------------------- */
/*      Do we need to apply a coordinate system?                        */
/* -------------------------------------------------------------------- */
    char *pszGeosys = NULL;
    char *pszUnits = NULL;
    double *padfPrjParams = NULL;

    if( poSRS != NULL 
        && poSRS->exportToPCI( &pszGeosys, &pszUnits, 
                               &padfPrjParams ) == OGRERR_NONE )
    {
        try
        {
            std::vector<double> adfPCIParameters;
            int i;

            for( i = 0; i < 17; i++ )
                adfPCIParameters.push_back( padfPrjParams[i] );
            
            if( EQUALN(pszUnits,"FOOT",4) )
                adfPCIParameters.push_back( 
                    (double)(int) PCIDSK::UNIT_US_FOOT );
            else if( EQUALN(pszUnits,"INTL FOOT",9) )
                adfPCIParameters.push_back( 
                    (double)(int) PCIDSK::UNIT_INTL_FOOT );
            else if( EQUALN(pszUnits,"DEGREE",6) )
                adfPCIParameters.push_back( 
                    (double)(int) PCIDSK::UNIT_DEGREE );
            else 
                adfPCIParameters.push_back( 
                    (double)(int) PCIDSK::UNIT_METER );
            
            poVecSeg->SetProjection( pszGeosys, adfPCIParameters );
        }
        catch( PCIDSK::PCIDSKException ex )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s", ex.what() );
        }
        
        CPLFree( pszGeosys );
        CPLFree( pszUnits );
        CPLFree( padfPrjParams );
    }

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    apoLayers.push_back( new OGRPCIDSKLayer( poSeg, TRUE ) );

    return apoLayers[apoLayers.size()-1];
}

