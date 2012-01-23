/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions within the OGR Memory driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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

#ifndef _OGRMEM_H_INCLUDED
#define _OGRMEM_H_INCLUDED

#include "ogrsf_frmts.h"

/************************************************************************/
/*                             OGRMemLayer                              */
/************************************************************************/

class OGRMemLayer : public OGRLayer
{
    OGRSpatialReference *poSRS;
    OGRFeatureDefn     *poFeatureDefn;
    
    int                 nFeatureCount;
    int                 nMaxFeatureCount;
    OGRFeature        **papoFeatures;

    int                 iNextReadFID;
    int                 iNextCreateFID;

    OGRwkbGeometryType  eWkbType;

    int                 bUpdatable;
    int                 bAdvertizeUTF8;

  public:
                        OGRMemLayer( const char * pszName,
                                     OGRSpatialReference *poSRS,
                                     OGRwkbGeometryType eGeomType );
                        ~OGRMemLayer();

    void                ResetReading();
    OGRFeature *        GetNextFeature();
    virtual OGRErr      SetNextByIndex( long nIndex );

    OGRFeature         *GetFeature( long nFeatureId );
    OGRErr              SetFeature( OGRFeature *poFeature );
    OGRErr              CreateFeature( OGRFeature *poFeature );
    virtual OGRErr      DeleteFeature( long nFID );
    
    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    int                 GetFeatureCount( int );

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );
    virtual OGRErr      DeleteField( int iField );
    virtual OGRErr      ReorderFields( int* panMap );
    virtual OGRErr      AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlags );

    virtual OGRSpatialReference *GetSpatialRef();
    
    int                 TestCapability( const char * );

    void                SetUpdatable(int bUpdatableIn) { bUpdatable = bUpdatableIn; }
    void                SetAdvertizeUTF8(int bAdvertizeUTF8In) { bAdvertizeUTF8 = bAdvertizeUTF8In; }
};

/************************************************************************/
/*                           OGRMemDataSource                           */
/************************************************************************/

class OGRMemDataSource : public OGRDataSource
{
    OGRMemLayer     **papoLayers;
    int                 nLayers;
    
    char                *pszName;

  public:
                        OGRMemDataSource( const char *, char ** );
                        ~OGRMemDataSource();

    const char          *GetName() { return pszName; }
    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );

    virtual OGRLayer    *CreateLayer( const char *, 
                                      OGRSpatialReference * = NULL,
                                      OGRwkbGeometryType = wkbUnknown,
                                      char ** = NULL );
    OGRErr              DeleteLayer( int iLayer );

    int                 TestCapability( const char * );
};

/************************************************************************/
/*                             OGRMemDriver                             */
/************************************************************************/

class OGRMemDriver : public OGRSFDriver
{
  public:
                ~OGRMemDriver();
                
    const char *GetName();
    OGRDataSource *Open( const char *, int );

    virtual OGRDataSource *CreateDataSource( const char *pszName,
                                             char ** = NULL );
    
    int                 TestCapability( const char * );
};


#endif /* ndef _OGRMEM_H_INCLUDED */
