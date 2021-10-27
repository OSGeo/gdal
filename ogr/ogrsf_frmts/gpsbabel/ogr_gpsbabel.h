/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for OGR/GPSBabel driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef OGR_GPSBABEL_H_INCLUDED
#define OGR_GPSBABEL_H_INCLUDED

#include "ogrsf_frmts.h"
#include "cpl_string.h"
#include <array>

/************************************************************************/
/*                        OGRGPSBabelDataSource                         */
/************************************************************************/

class OGRGPSBabelDataSource final: public OGRDataSource
{
    int                 nLayers = 0;
    std::array<OGRLayer*, 5>  apoLayers{{nullptr,nullptr,nullptr,nullptr,nullptr}};
    char               *pszName = nullptr;
    char               *pszGPSBabelDriverName = nullptr;
    char               *pszFilename = nullptr;
    CPLString           osTmpFileName{};
    GDALDataset        *poGPXDS = nullptr;

  public:
                        OGRGPSBabelDataSource();
                        virtual ~OGRGPSBabelDataSource();

    virtual int         CloseDependentDatasets() override;

    virtual const char  *GetName() override { return pszName; }
    virtual int         GetLayerCount() override { return nLayers; }
    virtual OGRLayer   *GetLayer( int ) override;

    virtual int         TestCapability( const char * ) override;

    int                 Open ( const char* pszFilename,
                               const char* pszGPSBabelDriverNameIn,
                               char** papszOpenOptions );

    static bool         IsSpecialFile( const char* pszFilename );
    static bool         IsValidDriverName( const char* pszGPSBabelDriverName );
};

/************************************************************************/
/*                   OGRGPSBabelWriteDataSource                         */
/************************************************************************/

class OGRGPSBabelWriteDataSource final: public OGRDataSource
{
    char               *pszName;
    char               *pszGPSBabelDriverName;
    char               *pszFilename;
    CPLString           osTmpFileName;
    GDALDataset        *poGPXDS;

    bool                Convert();

  public:
                        OGRGPSBabelWriteDataSource();
                        virtual ~OGRGPSBabelWriteDataSource();

    virtual const char  *GetName() override { return pszName; }
    virtual int         GetLayerCount() override;
    virtual OGRLayer   *GetLayer( int ) override;

    virtual int         TestCapability( const char * ) override;

    virtual OGRLayer   *ICreateLayer( const char * pszLayerName,
                                     OGRSpatialReference *poSRS,
                                     OGRwkbGeometryType eType,
                                     char ** papszOptions ) override;

    int                 Create ( const char* pszFilename, char **papszOptions );
};

#endif /* ndef OGR_GPSBABEL_H_INCLUDED */
