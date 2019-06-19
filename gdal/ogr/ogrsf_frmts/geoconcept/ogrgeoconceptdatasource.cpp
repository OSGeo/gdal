/******************************************************************************
 *
 * Name:     ogrgeoconceptdatasource.h
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGeoconceptDataSource class.
 * Language: C++
 *
 ******************************************************************************
 * Copyright (c) 2007, Geoconcept and IGN
 * Copyright (c) 2008, Even Rouault <even dot rouault at spatialys.com>
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
#include "ogrgeoconceptdatasource.h"
#include "ogrgeoconceptlayer.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                         OGRGeoconceptDataSource()                    */
/************************************************************************/

OGRGeoconceptDataSource::OGRGeoconceptDataSource() :
    _papoLayers(nullptr),
    _nLayers(0),
    _pszGCT(nullptr),
    _pszName(nullptr),
    _pszDirectory(nullptr),
    _pszExt(nullptr),
    _papszOptions(nullptr),
    _bSingleNewFile(false),
    _bUpdate(false),
    _hGXT(nullptr)
{}

/************************************************************************/
/*                        ~OGRGeoconceptDataSource()                    */
/************************************************************************/

OGRGeoconceptDataSource::~OGRGeoconceptDataSource()

{
    for( int i = 0; i < _nLayers; i++ )
    {
        delete _papoLayers[i];
    }
    CPLFree( _papoLayers );
    CPLFree( _pszGCT );
    CPLFree( _pszName );
    CPLFree( _pszDirectory );
    CPLFree( _pszExt );
    CSLDestroy( _papszOptions );

    if( _hGXT )
    {
      Close_GCIO(&_hGXT);
    }
}

/************************************************************************/
/*                                Open()                                */
/*                                                                      */
/*      Open an existing file.                                          */
/************************************************************************/

int OGRGeoconceptDataSource::Open( const char* pszName, bool bTestOpen,
                                   bool bUpdate )

{
/* -------------------------------------------------------------------- */
/*      Is the given path a directory or a regular file?                */
/* -------------------------------------------------------------------- */
    VSIStatBufL  sStat;

    if( VSIStatL( pszName, &sStat ) != 0
        || (!VSI_ISDIR(sStat.st_mode) && !VSI_ISREG(sStat.st_mode)) )
    {
        if( !bTestOpen )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s is neither a file or directory, "
                      "Geoconcept access failed.",
                      pszName );
        }

        return FALSE;
    }

    if( VSI_ISDIR(sStat.st_mode) )
    {
        CPLDebug( "GEOCONCEPT",
                  "%s is a directory, Geoconcept access is not yet supported.",
                  pszName );

        return FALSE;
    }

    if( VSI_ISREG(sStat.st_mode) )
    {
        _bSingleNewFile = false;
        _bUpdate = bUpdate;
        _pszName = CPLStrdup( pszName );
        if( !LoadFile( _bUpdate ? "a+t":"rt" ) )
        {
            CPLDebug( "GEOCONCEPT",
                      "Failed to open Geoconcept %s."
                      " It may be corrupt.",
                      pszName );

            return FALSE;
        }

        return TRUE;
    }

    return _nLayers > 0;
}

/************************************************************************/
/*                              LoadFile()                              */
/************************************************************************/

int OGRGeoconceptDataSource::LoadFile( const char *pszMode )

{
    if( _pszExt == nullptr)
    {
      const char* pszExtension = CPLGetExtension(_pszName);
      _pszExt = CPLStrdup(pszExtension);
    }
    CPLStrlwr( _pszExt );

    if( !_pszDirectory )
        _pszDirectory = CPLStrdup( CPLGetPath(_pszName) );

    if( (_hGXT= Open_GCIO(_pszName,_pszExt,pszMode,_pszGCT))==nullptr )
    {
      return FALSE;
    }

    /* Collect layers : */
    GCExportFileMetadata* Meta= GetGCMeta_GCIO(_hGXT);
    if( Meta )
    {
      const int nC = CountMetaTypes_GCIO(Meta);

      if( nC > 0 )
      {
        for( int iC= 0; iC<nC; iC++ )
        {
          GCType* aClass = GetMetaType_GCIO(Meta,iC);
          if( aClass )
          {
            const int nS = CountTypeSubtypes_GCIO(aClass);
            if( nS )
            {
              for( int iS = 0; iS<nS; iS++ )
              {
                GCSubType *aSubclass = GetTypeSubtype_GCIO(aClass,iS);
                if( aSubclass )
                {
                  OGRGeoconceptLayer *poFile = new OGRGeoconceptLayer;
                  if( poFile->Open(aSubclass) != OGRERR_NONE )
                  {
                    delete poFile;
                    return FALSE;
                  }

                  /* Add layer to data source layers list */
                  _papoLayers = static_cast<OGRGeoconceptLayer **>(
                      CPLRealloc( _papoLayers,
                                  sizeof(OGRGeoconceptLayer *)
                                  * (_nLayers+1) ) );
                  _papoLayers[_nLayers++] = poFile;

                  CPLDebug( "GEOCONCEPT",
                            "nLayers=%d - last=[%s]",
                            _nLayers, poFile->GetLayerDefn()->GetName());
                }
              }
            }
          }
        }
      }
    }

    return TRUE;
}

/************************************************************************/
/*                               Create()                               */
/*                                                                      */
/*      Create a new dataset.                                           */
/*                                                                      */
/* Options (-dsco) :                                                    */
/*   EXTENSION : gxt|txt                                                */
/*   CONFIG : path to GCT file                                          */
/************************************************************************/

int OGRGeoconceptDataSource::Create( const char *pszName, char** papszOptions )

{
    CPLFree( _pszName );
    _papszOptions = CSLDuplicate( papszOptions );

    const char *pszConf = CSLFetchNameValue(papszOptions,"CONFIG");
    if( pszConf != nullptr )
    {
      _pszGCT = CPLStrdup(pszConf);
    }

    _pszExt = (char *)CSLFetchNameValue(papszOptions,"EXTENSION");
    const char *pszExtension = CSLFetchNameValue( papszOptions, "EXTENSION" );
    if( pszExtension == nullptr )
    {
        _pszExt = CPLStrdup(CPLGetExtension(pszName));
    }
    else
    {
        _pszExt = CPLStrdup(pszExtension);
    }

    if( strlen(_pszExt) == 0 )
    {
        if( VSIMkdir( pszName, 0755 ) != 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Directory %s already exists"
                      " as geoconcept datastore or"
                      " is made up of a non existing list of directories.",
                      pszName );

            return FALSE;
        }
        _pszDirectory = CPLStrdup( pszName );
        CPLFree(_pszExt);
        _pszExt = CPLStrdup("gxt");
        char *pszbName = CPLStrdup(CPLGetBasename( pszName ));
        if (strlen(pszbName)==0) {/* pszName ends with '/' */
            CPLFree(pszbName);
            char *pszNameDup= CPLStrdup(pszName);
            pszNameDup[strlen(pszName)-2] = '\0';
            pszbName = CPLStrdup(CPLGetBasename( pszNameDup ));
            CPLFree(pszNameDup);
        }
        _pszName = CPLStrdup((char *)CPLFormFilename( _pszDirectory, pszbName, nullptr ));
        CPLFree(pszbName);
    }
    else
    {
        _pszDirectory = CPLStrdup( CPLGetPath(pszName) );
        _pszName = CPLStrdup( pszName );
    }

/* -------------------------------------------------------------------- */
/*      Create a new single file.                                       */
/*      OGRGeoconceptDriver::ICreateLayer() will do the job.             */
/* -------------------------------------------------------------------- */
    _bSingleNewFile = true;

    if( !LoadFile( "wt" ) )
    {
        CPLDebug( "GEOCONCEPT",
                  "Failed to create Geoconcept %s.",
                  pszName );

        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/*                                                                      */
/* Options (-lco) :                                                     */
/*   FEATURETYPE : TYPE.SUBTYPE                                         */
/************************************************************************/

OGRLayer *OGRGeoconceptDataSource::ICreateLayer( const char * pszLayerName,
                                                OGRSpatialReference *poSRS /* = NULL */,
                                                OGRwkbGeometryType eType /* = wkbUnknown */,
                                                char ** papszOptions /* = NULL */ )

{
    if( _hGXT == nullptr )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Internal Error : null datasource handler."
                );
        return nullptr;
    }

    if( poSRS == nullptr && !_bUpdate)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "SRS is mandatory of creating a Geoconcept Layer."
                );
        return nullptr;
    }

    /*
     * pszLayerName Class.Subclass if -nln option used, otherwise file name
     */
    const char *pszFeatureType = nullptr;
    char pszln[512];

    if( !(pszFeatureType = CSLFetchNameValue(papszOptions,"FEATURETYPE")) )
    {
      if( !pszLayerName || !strchr(pszLayerName,'.') )
      {
        snprintf(pszln,511,"%s.%s", pszLayerName? pszLayerName:"ANONCLASS",
                                    pszLayerName? pszLayerName:"ANONSUBCLASS");
        pszln[511]= '\0';
        pszFeatureType= pszln;
      }
      else
        pszFeatureType= pszLayerName;
    }

    char **ft = CSLTokenizeString2(pszFeatureType,".",0);
    if( !ft ||
        CSLCount(ft)!=2 )
    {
      CSLDestroy(ft);
      CPLError( CE_Failure, CPLE_AppDefined,
                "Feature type name '%s' is incorrect."
                "Correct syntax is : Class.Subclass.",
                pszFeatureType );
      return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Figure out what type of layer we need.                          */
/* -------------------------------------------------------------------- */
    GCTypeKind gcioFeaType;
    GCDim gcioDim = v2D_GCIO;

    if( eType == wkbUnknown )
        gcioFeaType = vUnknownItemType_GCIO;
    else if( eType == wkbPoint )
        gcioFeaType = vPoint_GCIO;
    else if( eType == wkbLineString )
        gcioFeaType = vLine_GCIO;
    else if( eType == wkbPolygon )
        gcioFeaType = vPoly_GCIO;
    else if( eType == wkbMultiPoint )
        gcioFeaType = vPoint_GCIO;
    else if( eType == wkbMultiLineString )
        gcioFeaType = vLine_GCIO;
    else if( eType == wkbMultiPolygon )
        gcioFeaType = vPoly_GCIO;
    else if( eType == wkbPoint25D )
    {
        gcioFeaType = vPoint_GCIO;
        gcioDim= v3DM_GCIO;
    }
    else if( eType == wkbLineString25D )
    {
        gcioFeaType = vLine_GCIO;
        gcioDim= v3DM_GCIO;
    }
    else if( eType == wkbPolygon25D )
    {
        gcioFeaType = vPoly_GCIO;
        gcioDim= v3DM_GCIO;
    }
    else if( eType == wkbMultiPoint25D )
    {
        gcioFeaType = vPoint_GCIO;
        gcioDim= v3DM_GCIO;
    }
    else if( eType == wkbMultiLineString25D )
    {
        gcioFeaType = vLine_GCIO;
        gcioDim= v3DM_GCIO;
    }
    else if( eType == wkbMultiPolygon25D )
    {
        gcioFeaType = vPoly_GCIO;
        gcioDim= v3DM_GCIO;
    }
    else
    {
        CSLDestroy(ft);
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Geometry type of '%s' not supported in Geoconcept files.",
                  OGRGeometryTypeToName(eType) );
        return nullptr;
    }

    /*
     * As long as we use the CONFIG, creating a layer implies the
     * layer name to exist in the CONFIG as "Class.Subclass".
     * Removing the CONFIG, implies on-the-fly-creation of layers...
     */
    OGRGeoconceptLayer *poFile= nullptr;

    if( _nLayers > 0 )
      for( int iLayer= 0; iLayer<_nLayers; iLayer++)
      {
        poFile= reinterpret_cast<OGRGeoconceptLayer *>( GetLayer(iLayer) );
        if( poFile != nullptr && EQUAL(poFile->GetLayerDefn()->GetName(),pszFeatureType) )
        {
          break;
        }
        poFile= nullptr;
      }
    if( !poFile )
    {
      GCSubType* aSubclass = nullptr;
      GCExportFileMetadata* m = GetGCMeta_GCIO(_hGXT);

      if( !m )
      {
        if( !(m= CreateHeader_GCIO()) )
        {
          CSLDestroy(ft);
          return nullptr;
        }
        SetMetaExtent_GCIO(m, CreateExtent_GCIO(HUGE_VAL,HUGE_VAL,-HUGE_VAL,-HUGE_VAL));
        SetGCMeta_GCIO(_hGXT, m);
      }
      if( FindFeature_GCIO(_hGXT, pszFeatureType) )
      {
        CSLDestroy(ft);
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Layer '%s' already exists.",
                  pszFeatureType );
        return nullptr;
      }
      if( !AddType_GCIO(_hGXT, ft[0], -1L) )
      {
        CSLDestroy(ft);
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to add layer '%s'.",
                  pszFeatureType );
        return nullptr;
      }
      if( !(aSubclass= AddSubType_GCIO(_hGXT, ft[0], ft[1], -1L, gcioFeaType, gcioDim)) )
      {
        CSLDestroy(ft);
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to add layer '%s'.",
                  pszFeatureType );
        return nullptr;
      }
      /* complete feature type with private fields : */
      AddSubTypeField_GCIO(_hGXT, ft[0], ft[1], -1L, kIdentifier_GCIO, -100, vIntFld_GCIO, nullptr, nullptr);
      AddSubTypeField_GCIO(_hGXT, ft[0], ft[1], -1L, kClass_GCIO, -101, vMemoFld_GCIO, nullptr, nullptr);
      AddSubTypeField_GCIO(_hGXT, ft[0], ft[1], -1L, kSubclass_GCIO, -102, vMemoFld_GCIO, nullptr, nullptr);
      AddSubTypeField_GCIO(_hGXT, ft[0], ft[1], -1L, kName_GCIO, -103, vMemoFld_GCIO, nullptr, nullptr);
      AddSubTypeField_GCIO(_hGXT, ft[0], ft[1], -1L, kNbFields_GCIO, -104, vIntFld_GCIO, nullptr, nullptr);
      AddSubTypeField_GCIO(_hGXT, ft[0], ft[1], -1L, kX_GCIO, -105, vRealFld_GCIO, nullptr, nullptr);
      AddSubTypeField_GCIO(_hGXT, ft[0], ft[1], -1L, kY_GCIO, -106, vRealFld_GCIO, nullptr, nullptr);
      /* user's fields will be added with Layer->CreateField() method ... */
      switch( gcioFeaType )
      {
        case vPoint_GCIO :
          break;
        case vLine_GCIO  :
          AddSubTypeField_GCIO(_hGXT, ft[0], ft[1], -1L, kXP_GCIO, -107, vRealFld_GCIO, nullptr, nullptr);
          AddSubTypeField_GCIO(_hGXT, ft[0], ft[1], -1L, kYP_GCIO, -108, vRealFld_GCIO, nullptr, nullptr);
          AddSubTypeField_GCIO(_hGXT, ft[0], ft[1], -1L, kGraphics_GCIO, -109, vUnknownItemType_GCIO, nullptr, nullptr);
          break;
        default          :
          AddSubTypeField_GCIO(_hGXT, ft[0], ft[1], -1L, kGraphics_GCIO, -109, vUnknownItemType_GCIO, nullptr, nullptr);
          break;
      }
      SetSubTypeGCHandle_GCIO(aSubclass,_hGXT);

      /* Add layer to data source layers list */
      poFile = new OGRGeoconceptLayer;
      if( poFile->Open(aSubclass) != OGRERR_NONE )
      {
        CSLDestroy(ft);
        delete poFile;
        return nullptr;
      }

      _papoLayers = static_cast<OGRGeoconceptLayer **>(
          CPLRealloc( _papoLayers,
                      sizeof(OGRGeoconceptLayer *) * (_nLayers+1) ) );
      _papoLayers[_nLayers++] = poFile;

      CPLDebug("GEOCONCEPT",
               "nLayers=%d - last=[%s]",
               _nLayers, poFile->GetLayerDefn()->GetName());
    }
    CSLDestroy(ft);

/* -------------------------------------------------------------------- */
/*      Assign the coordinate system (if provided)                      */
/* -------------------------------------------------------------------- */
    if( poSRS != nullptr )
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

    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRGeoconceptDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= GetLayerCount() )
      return nullptr;

    OGRLayer *poFile = _papoLayers[iLayer];
    return poFile;
}
