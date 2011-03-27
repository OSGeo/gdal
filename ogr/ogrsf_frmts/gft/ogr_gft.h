/******************************************************************************
 * $Id$
 *
 * Project:  GFT Translator
 * Purpose:  Definition of classes for OGR Google Fusion Tables driver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault <even dot rouault at mines dash paris dot org>
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

#ifndef _OGR_GFT_H_INCLUDED
#define _OGR_GFT_H_INCLUDED

#include "ogrsf_frmts.h"

#include <vector>

/************************************************************************/
/*                             OGRGFTLayer                              */
/************************************************************************/
class OGRGFTDataSource;

class OGRGFTLayer : public OGRLayer
{
    OGRGFTDataSource* poDS;
    CPLString         osTableName;
    CPLString         osTableId;

    OGRFeatureDefn*    poFeatureDefn;
    OGRSpatialReference *poSRS;

    int                nNextFID;
    int                nFeatureCount;

    int                iGeometryField;
    int                iLatitude;
    int                iLongitude;

    OGRFeature *       GetNextRawFeature();

    int                FetchDescribe();

    int                nOffset;
    int                bEOF;
    int                FetchNextRows();

    std::vector<CPLString> aosRows;

    int                bHasTriedCreateTable;
    void               CreateTableIfNecessary();

    CPLString          osTransaction;
    int                bInTransaction;

    CPLString           osWHERE;
    CPLString           osQuery;

    void                BuildWhere(void);

  public:
                        OGRGFTLayer(OGRGFTDataSource* poDS,
                                    const char* pszTableName,
                                    const char* pszTableId = "");
                        ~OGRGFTLayer();

    virtual const char *        GetName() { return osTableName.c_str(); }
    //virtual OGRwkbGeometryType GetGeomType() { return wkbUnknown; }

    virtual void                ResetReading();
    virtual OGRFeature *        GetNextFeature();
    virtual OGRFeature *        GetFeature( long nFID );

    virtual OGRFeatureDefn *    GetLayerDefn();

    virtual int                 TestCapability( const char * );

    virtual OGRSpatialReference *GetSpatialRef() { return poSRS; }

    virtual int         GetFeatureCount( int bForce = TRUE );

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );
    OGRErr              CreateFeature( OGRFeature *poFeature );

    virtual OGRErr      StartTransaction();
    virtual OGRErr      CommitTransaction();
    virtual OGRErr      RollbackTransaction();

    virtual void        SetSpatialFilter( OGRGeometry * );
    virtual OGRErr      SetAttributeFilter( const char * );

    const CPLString&            GetTableId() const { return osTableId; }
};

/************************************************************************/
/*                           OGRGFTDataSource                           */
/************************************************************************/

class OGRGFTDataSource : public OGRDataSource
{
    char*               pszName;

    OGRLayer**          papoLayers;
    int                 nLayers;

    int                 bReadWrite;

    int                 bUseHTTPS;
    CPLString           osAuth;
    int                 FetchAuth(const char* pszEmail, const char* pszPassword);

    void                DeleteLayer( const char *pszLayerName );

  public:
                        OGRGFTDataSource();
                        ~OGRGFTDataSource();

    int                 Open( const char * pszFilename,
                              int bUpdate );

    virtual const char*         GetName() { return pszName; }

    virtual int                 GetLayerCount() { return nLayers; }
    virtual OGRLayer*           GetLayer( int );

    virtual int                 TestCapability( const char * );

    virtual OGRLayer   *CreateLayer( const char *pszName,
                                     OGRSpatialReference *poSpatialRef = NULL,
                                     OGRwkbGeometryType eGType = wkbUnknown,
                                     char ** papszOptions = NULL );
    virtual OGRErr      DeleteLayer(int);

    const CPLString&            GetAuth() const { return osAuth; }
    const char*                 GetAPIURL() const;
    int                         IsReadWrite() const { return bReadWrite; }
    char**                      AddHTTPOptions(char** papszOptions = NULL);
};

/************************************************************************/
/*                             OGRGFTDriver                             */
/************************************************************************/

class OGRGFTDriver : public OGRSFDriver
{
  public:
                ~OGRGFTDriver();

    virtual const char*         GetName();
    virtual OGRDataSource*      Open( const char *, int );
    virtual int                 TestCapability( const char * );
};


#endif /* ndef _OGR_GFT_H_INCLUDED */
