/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGmtDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
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
#include "ogr_gmt.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                          OGRGmtDataSource()                          */
/************************************************************************/

OGRGmtDataSource::OGRGmtDataSource() :
    papoLayers(NULL),
    nLayers(0),
    pszName(NULL),
    bUpdate(false)
{}

/************************************************************************/
/*                         ~OGRGmtDataSource()                          */
/************************************************************************/

OGRGmtDataSource::~OGRGmtDataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );
    CPLFree( pszName );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRGmtDataSource::Open( const char *pszFilename, int bUpdateIn )

{
    bUpdate = CPL_TO_BOOL( bUpdateIn );

    OGRGmtLayer *poLayer = new OGRGmtLayer( pszFilename, bUpdate );
    if( !poLayer->bValidFile )
    {
        delete poLayer;
        return FALSE;
    }

    papoLayers = static_cast<OGRGmtLayer **>( CPLRealloc( papoLayers,
                                        (nLayers + 1) *sizeof(OGRGmtLayer*)) );
    papoLayers[nLayers] = poLayer;
    nLayers ++;

    CPLFree (pszName);
    pszName = CPLStrdup( pszFilename );

    return TRUE;
}

/************************************************************************/
/*                               Create()                               */
/*                                                                      */
/*      Create a new datasource.  This does not really do anything      */
/*      currently but save the name.                                    */
/************************************************************************/

int OGRGmtDataSource::Create( const char *pszDSName, char ** /* papszOptions */)

{
    pszName = CPLStrdup( pszDSName );

    return TRUE;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRGmtDataSource::ICreateLayer( const char * pszLayerName,
                                OGRSpatialReference *poSRS,
                                OGRwkbGeometryType eType,
                                CPL_UNUSED char ** papszOptions )
{
    if( nLayers != 0 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Establish the geometry type.  Note this logic                   */
/* -------------------------------------------------------------------- */
    const char *pszGeom = NULL;

    switch( wkbFlatten(eType) )
    {
      case wkbPoint:
        pszGeom = " @GPOINT";
        break;
      case wkbLineString:
        pszGeom = " @GLINESTRING";
        break;
      case wkbPolygon:
        pszGeom = " @GPOLYGON";
        break;
      case wkbMultiPoint:
        pszGeom = " @GMULTIPOINT";
        break;
      case wkbMultiLineString:
        pszGeom = " @GMULTILINESTRING";
        break;
      case wkbMultiPolygon:
        pszGeom = " @GMULTIPOLYGON";
        break;
      default:
        pszGeom = "";
        break;
    }

/* -------------------------------------------------------------------- */
/*      If this is the first layer for this datasource, and if the      */
/*      datasource name ends in .gmt we will override the provided      */
/*      layer name with the name from the gmt.                          */
/* -------------------------------------------------------------------- */
    CPLString osPath = CPLGetPath( pszName );
    CPLString osFilename;

    if( EQUAL(CPLGetExtension(pszName),"gmt") )
        osFilename = pszName;
    else
        osFilename = CPLFormFilename( osPath, pszLayerName, "gmt" );

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL( osFilename, "w" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "open(%s) failed: %s",
                  osFilename.c_str(), VSIStrerror(errno) );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Write out header.                                               */
/* -------------------------------------------------------------------- */
    VSIFPrintfL( fp, "# @VGMT1.0%s\n", pszGeom );
    VSIFPrintfL( fp, "# REGION_STUB                                      "
                 "                       \n" );

/* -------------------------------------------------------------------- */
/*      Write the projection, if possible.                              */
/* -------------------------------------------------------------------- */
    if( poSRS != NULL )
    {
        if( poSRS->IsProjected()
            && poSRS->GetAuthorityName("PROJCS")
            && EQUAL(poSRS->GetAuthorityName("PROJCS"),"EPSG") )
        {
            VSIFPrintfL( fp, "# @Je%s\n",
                         poSRS->GetAuthorityCode("PROJCS") );
        }
        else if( poSRS->IsGeographic()
                 && poSRS->GetAuthorityName("GEOGCS")
                 && EQUAL(poSRS->GetAuthorityName("GEOGCS"),"EPSG") )
        {
            VSIFPrintfL( fp, "# @Je%s\n",
                         poSRS->GetAuthorityCode("GEOGCS") );
        }

        char *pszValue = NULL;
        if( poSRS->exportToProj4( &pszValue ) == OGRERR_NONE )
        {
            VSIFPrintfL( fp, "# @Jp\"%s\"\n", pszValue );
            CPLFree( pszValue );
            pszValue = NULL;
        }

        if( poSRS->exportToWkt( &pszValue ) == OGRERR_NONE )
        {
            char *pszEscapedWkt = CPLEscapeString( pszValue, -1,
                                                   CPLES_BackslashQuotable );

            VSIFPrintfL( fp, "# @Jw\"%s\"\n", pszEscapedWkt );
            CPLFree( pszValue );
            CPLFree( pszEscapedWkt );
            pszValue = NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Finish header and close.                                        */
/* -------------------------------------------------------------------- */
    VSIFCloseL( fp );

/* -------------------------------------------------------------------- */
/*      Return open layer handle.                                       */
/* -------------------------------------------------------------------- */
    if( Open( osFilename, TRUE ) )
        return papoLayers[nLayers-1];

    return NULL;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGmtDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRGmtDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;

    return papoLayers[iLayer];
}
