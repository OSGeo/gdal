/**********************************************************************
 * $Id: ogrgeoconceptdatasource.h$
 *
 * Name:     ogrgeoconceptdatasource.h
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGeoconceptDataSource class.
 * Language: C++
 *
 **********************************************************************
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#include "ogrsf_frmts.h"
#include "ogrgeoconceptlayer.h"

#ifndef GEOCONCEPT_OGR_DATASOURCE_H_INCLUDED_
#define GEOCONCEPT_OGR_DATASOURCE_H_INCLUDED_

/**********************************************************************/
/*            OGCGeoconceptDataSource Class                           */
/**********************************************************************/
class OGRGeoconceptDataSource : public OGRDataSource
{
  private:
    OGRGeoconceptLayer **_papoLayers;
    int                  _nLayers;

    char                *_pszGCT;
    char                *_pszName;
    char                *_pszDirectory;
    char                *_pszExt;
    char               **_papszOptions;
    bool                 _bSingleNewFile;
    bool                 _bUpdate;
    GCExportFileH       *_hGXT;

  public:
                   OGRGeoconceptDataSource();
                  ~OGRGeoconceptDataSource();

    int            Open( const char* pszName, bool bTestOpen, bool bUpdate );
    int            Create( const char* pszName, char** papszOptions );

    const char*    GetName() override { return _pszName; }
    int            GetLayerCount() override { return _nLayers; }
    OGRLayer*      GetLayer( int iLayer ) override;
//    OGRErr         DeleteLayer( int iLayer );
    int            TestCapability( const char* pszCap ) override;

    OGRLayer*      ICreateLayer( const char* pszName,
                                OGRSpatialReference* poSpatialRef = nullptr,
                                OGRwkbGeometryType eGType = wkbUnknown,
                                char** papszOptions = nullptr ) override;
  private:
    int            LoadFile( const char * );
};

#endif /* GEOCONCEPT_OGR_DATASOURCE_H_INCLUDED_ */
