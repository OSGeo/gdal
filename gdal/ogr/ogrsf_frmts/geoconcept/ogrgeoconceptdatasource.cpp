/******************************************************************************
 * $Id: ogrgeoconceptdatasource.cpp
 *
 * Name:     ogrgeoconceptdatasource.h
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGeoconceptDataSource class.
 * Language: C++
 *
 ******************************************************************************
 * Copyright (c) 2007,  Geoconcept and IGN
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

#include "ogrgeoconceptlayer.h"
#include "ogrgeoconceptdatasource.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id: ogrgeoconceptdatasource.cpp 00000 2007-11-03 11:49:22Z drichard $");

/************************************************************************/
/*                         OGRGeoconceptDataSource()                    */
/************************************************************************/

OGRGeoconceptDataSource::OGRGeoconceptDataSource()

{
    _pszGCT = NULL;
    _pszName = NULL;
    _pszDirectory = NULL;
    _pszExt = NULL;
    _papoLayers = NULL;
    _nLayers = 0;
    _bSingleNewFile = FALSE;
    _bUpdate = FALSE;
    _papszOptions = NULL;
}

/************************************************************************/
/*                        ~OGRGeoconceptDataSource()                    */
/************************************************************************/

OGRGeoconceptDataSource::~OGRGeoconceptDataSource()

{
    if ( _pszGCT )
    {
      CPLFree( _pszGCT );
    }
    if ( _pszName )
    {
      CPLFree( _pszName );
    }
    if ( _pszDirectory )
    {
      CPLFree( _pszDirectory );
    }
    if ( _pszExt )
    {
      CPLFree( _pszExt );
    }

    if ( _papoLayers )
    {
      for( int i = 0; i < _nLayers; i++ )
        delete _papoLayers[i];

      CPLFree( _papoLayers );
    }
    if ( _papszOptions )
    {
      CSLDestroy( _papszOptions );
    }
}

/************************************************************************/
/*                                Open()                                */
/*                                                                      */
/*      Open an existing file, or directory of files.                   */
/************************************************************************/

int OGRGeoconceptDataSource::Open( const char* pszName, int bUpdate, int bTestOpen )

{
    VSIStatBuf  stat;

    if( !bUpdate )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Reading Geoconcept %s is not currently supported.\n",
                  pszName);
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Is the given path a directory or a regular file?                */
/* -------------------------------------------------------------------- */
    if( CPLStat( pszName, &stat ) != 0
        || (!VSI_ISDIR(stat.st_mode) && !VSI_ISREG(stat.st_mode)) )
    {
        if( !bTestOpen )
            CPLError( CE_Failure, CPLE_AppDefined,
                   "%s is neither a file or directory, Geoconcept access failed.\n",
                      pszName );

        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Build a list of filenames we figure are Geoconcept files.       */
/* -------------------------------------------------------------------- */
    if( VSI_ISREG(stat.st_mode) )
    {
        _bSingleNewFile= TRUE;
        if( !LoadFile( pszName, bUpdate, bTestOpen ) )
        {
            if( !bTestOpen )
                CPLError( CE_Failure, CPLE_OpenFailed,
                          "Failed to open Geoconcept %s.\n"
                          "It may be corrupt.\n",
                          pszName );

            return FALSE;
        }

        return TRUE;
    }
    else
    {
        char      **papszCandidates = CPLReadDir( pszName );
        int       iCan, nCandidateCount = CSLCount( papszCandidates );

        for( iCan = 0; iCan < nCandidateCount; iCan++ )
        {
            char        *pszFilename;
            const char  *pszCandidate = papszCandidates[iCan];

            if( strlen(pszCandidate) < 5
                || !( EQUAL(pszCandidate+strlen(pszCandidate)-4,".gxt") ||
                      EQUAL(pszCandidate+strlen(pszCandidate)-4,".txt") ) )
                continue;

            pszFilename =
                CPLStrdup(CPLFormFilename(pszName, pszCandidate, NULL));

            if( !LoadFile( pszFilename, bUpdate, bTestOpen )
                && !bTestOpen )
            {
                CPLError( CE_Failure, CPLE_OpenFailed,
                          "Failed to open Geoconcept %s.\n"
                          "It may be corrupt.\n",
                          pszFilename );
                CPLFree( pszFilename );
                return FALSE;
            }

            CPLFree( pszFilename );
        }

        CSLDestroy( papszCandidates );

        if( !bTestOpen && _nLayers == 0 && !bUpdate )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "No Geoconcept found in directory %s\n",
                      pszName );
        }
        else
        {
            _pszDirectory = CPLStrdup(pszName);
        }
    }

    return _nLayers > 0 || bUpdate;
}

/************************************************************************/
/*                              LoadFile()                              */
/************************************************************************/

int OGRGeoconceptDataSource::LoadFile( const char *pszName, int bUpdate, int bTestOpen )

{
    OGRGeoconceptLayer *poFile;

    _bUpdate= bUpdate;
    (void) bTestOpen;

    /* Let CreateLayer do the job ... */
    if( bUpdate )
    {
      _pszName = CPLStrdup( pszName );
      return TRUE;
    }

    _pszExt = (char *)CPLGetExtension(pszName);
    if( !EQUAL(_pszExt,"gxt") && !EQUAL(_pszExt,"txt") )
        return FALSE;
    if( EQUAL(_pszExt,"txt") )
        _pszExt = CPLStrdup("txt");
    else if( EQUAL(_pszExt,"gxt") )
        _pszExt = NULL;
    else
        _pszExt = NULL;
    CPLStrlwr( _pszExt );

    if( !_pszDirectory )
        _pszDirectory = CPLStrdup( CPLGetPath(pszName) );

    poFile = new OGRGeoconceptLayer;

    if( poFile->Open( pszName,
                      _pszExt,
                      _bUpdate? "a+t":"wt",
                      _pszGCT,
                      NULL
                    ) != OGRERR_NONE )
    {
        delete poFile;
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    _papoLayers = (OGRGeoconceptLayer **)CPLRealloc( _papoLayers,  sizeof(OGRGeoconceptLayer *) * (_nLayers+1) );
    _papoLayers[_nLayers++] = poFile;

    CPLDebug("GEOCONCEPT",
             "nLayers=%d - last=[%s]\n",
             _nLayers, poFile->GetLayerDefn()->GetName());

    return TRUE;
}

/************************************************************************/
/*                               Create()                               */
/*                                                                      */
/*      Create a new dataset (directory or file).                       */
/*                                                                      */
/* Options (-dsco) :                                                    */
/*   EXTENSION : gxt|txt                                                */
/*   CONFIG : path to GCT file                                          */
/************************************************************************/

int OGRGeoconceptDataSource::Create( const char *pszName, char** papszOptions )

{
    char *conf;
    if( _pszName ) CPLFree(_pszName);
    _pszName = CPLStrdup( pszName );
    _papszOptions = CSLDuplicate( papszOptions );

    conf = (char *)CSLFetchNameValue(papszOptions,"CONFIG");
    if( conf == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to create dataset name %s,\n"
                  "but without a GCT file (See option CONFIG).\n",
                  pszName);
        return FALSE;
    }
    _pszGCT = CPLStrdup(conf);

    _pszExt = (char *)CSLFetchNameValue(papszOptions,"EXTENSION");
    if( _pszExt == NULL )
    {
        _pszExt = (char *)CPLGetExtension(pszName);
    }
    if( EQUAL(_pszExt,"txt") )
        _pszExt = CPLStrdup("txt");
    else if( EQUAL(_pszExt,"gxt") )
        _pszExt = NULL;
    else
        _pszExt = NULL;
    CPLStrlwr( _pszExt );

/* -------------------------------------------------------------------- */
/*      Create a new empty directory.                                   */
/* -------------------------------------------------------------------- */
    if( strlen(CPLGetExtension(pszName)) == 0 )
    {
        VSIStatBuf  sStat;

        if( VSIStat( pszName, &sStat ) == 0 )
        {
            if( !VSI_ISDIR(sStat.st_mode) )
            {
                CPLError( CE_Failure, CPLE_OpenFailed,
                          "Attempt to create dataset named %s,\n"
                          "but that is an existing file.\n",
                          pszName );
                return FALSE;
            }
        }
        else
        {
            if( VSIMkdir( pszName, 0755 ) != 0 )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Unable to create directory %s.\n",
                          pszName );
                return FALSE;
            }
        }

        _pszDirectory = CPLStrdup(pszName);
    }

/* -------------------------------------------------------------------- */
/*      Create a new single file.                                       */
/*      OGRGeoconceptDriver::CreateLayer() will do the job.             */
/* -------------------------------------------------------------------- */
    else
    {
        _pszDirectory = CPLStrdup( CPLGetPath(pszName) );
        _bSingleNewFile = TRUE;
    }

    return TRUE;
}

/************************************************************************/
/*                            CreateLayer()                             */
/*                                                                      */
/* Options (-lco) :                                                     */
/*   FEATURETYPE : TYPE.SUBTYPE                                         */
/************************************************************************/

OGRLayer *OGRGeoconceptDataSource::CreateLayer( const char * pszLayerName,
                                                OGRSpatialReference *poSRS /* = NULL */,
                                                OGRwkbGeometryType eType /* = wkbUnknown */,
                                                char ** papszOptions /* = NULL */ )

{
    GCTypeKind gcioFldType;
    OGRGeoconceptLayer *poFile;
    char *pszFullFilename, *pszFeatureType;

    if( poSRS == NULL && !_bUpdate) {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "SRS is mandatory of creating a Geoconcept Layer.\n"
                );
        return NULL;
    }

    if( !(pszFeatureType = (char *)CSLFetchNameValue(papszOptions,"FEATURETYPE")) )
    {
      CPLError( CE_Failure, CPLE_NotSupported,
                "option FEATURETYPE=Class.SubClass is mandatory for Geoconcept datasource.\n"
              );
      return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Figure out what type of layer we need.                          */
/* -------------------------------------------------------------------- */
    if( eType == wkbUnknown || eType == wkbLineString )
        gcioFldType = vLine_GCIO;
    else if( eType == wkbPoint )
        gcioFldType = vPoint_GCIO;
    else if( eType == wkbPolygon )
        gcioFldType = vPoly_GCIO;
    else if( eType == wkbMultiPoint )
        gcioFldType = vPoint_GCIO;
    else if( eType == wkbPoint25D )
        gcioFldType = vPoint_GCIO;
    else if( eType == wkbLineString25D )
        gcioFldType = vLine_GCIO;
    else if( eType == wkbMultiLineString )
        gcioFldType = vLine_GCIO;
    else if( eType == wkbMultiLineString25D )
        gcioFldType = vLine_GCIO;
    else if( eType == wkbPolygon25D )
        gcioFldType = vPoly_GCIO;
    else if( eType == wkbMultiPolygon )
        gcioFldType = vPoly_GCIO;
    else if( eType == wkbMultiPolygon25D )
        gcioFldType = vPoly_GCIO;
    else if( eType == wkbMultiPoint25D )
        gcioFldType = vPoint_GCIO;
    else if( eType == wkbNone )
        gcioFldType = vUnknownItemType_GCIO;
    else
        gcioFldType = vUnknownItemType_GCIO;

    if( gcioFldType == vUnknownItemType_GCIO )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Geometry type of `%s' not supported in Geoconcept files.\n"
                  "Type can be overridden with a layer creation option\n"
                  "of GEOMETRY=POINT/LINE/POLY/POINT3D/LINE3D/POLY3D.\n",
                  OGRGeometryTypeToName(eType) );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      If it's a single file mode file, then we may have already       */
/*      instantiated the low level layer.                               */
/* -------------------------------------------------------------------- */
    if( _bSingleNewFile )
    {
        pszFullFilename = CPLStrdup(_pszName);
    }
/* -------------------------------------------------------------------- */
/*      We need to initially create the file, and add it as a layer.    */
/* -------------------------------------------------------------------- */
    else
    {
        pszFullFilename = CPLStrdup( CPLFormFilename( _pszDirectory,
                                                      pszLayerName,
                                                      _pszExt? _pszExt:"gxt" ) );
    }

    poFile = new OGRGeoconceptLayer;

    if( poFile->Open( pszFullFilename,
                      _pszExt,
                      _bUpdate? "a+t":"wt",
                      _pszGCT,
                      pszFeatureType
                    ) != OGRERR_NONE )
    {
        delete poFile;
        return FALSE;
    }

    _nLayers++;
    _papoLayers = (OGRGeoconceptLayer **) CPLRealloc(_papoLayers,sizeof(void*)*_nLayers);
    _papoLayers[_nLayers-1] = poFile;

    CPLDebug("GEOCONCEPT",
             "nLayers=%d - last=[%s]\n",
             _nLayers, poFile->GetLayerDefn()->GetName());

    CPLFree( pszFullFilename );

/* -------------------------------------------------------------------- */
/*      Assign the coordinate system (if provided)                      */
/* -------------------------------------------------------------------- */
    if( poSRS != NULL )
        poFile->SetSpatialRef( poSRS );

    return poFile;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGeoconceptDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRGeoconceptDataSource::GetLayer( int iLayer )

{
    OGRLayer *poFile;
    if( iLayer < 0 || iLayer >= GetLayerCount() )
        poFile= NULL;
    else
        poFile= _papoLayers[iLayer];
    return poFile;
}
