/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for OGR/PostgreSQL dump driver.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault
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

#ifndef _OGR_PGDUMP_H_INCLUDED
#define _OGR_PGDUMP_H_INCLUDED

#include "ogrsf_frmts.h"
#include "cpl_string.h"

CPLString OGRPGDumpEscapeColumnName(const char* pszColumnName);
CPLString OGRPGDumpEscapeString(   const char* pszStrValue, int nMaxLength = -1,
                                   const char* pszFieldName = "");


/************************************************************************/
/*                        OGRPGDumpGeomFieldDefn                        */
/************************************************************************/

class OGRPGDumpGeomFieldDefn : public OGRGeomFieldDefn
{
    public:
        OGRPGDumpGeomFieldDefn( OGRGeomFieldDefn *poGeomField ) :
            OGRGeomFieldDefn(poGeomField), nSRSId(-1), nCoordDimension(2)
            {
            }
            
        int nSRSId;
        int nCoordDimension;
};

/************************************************************************/
/*                          OGRPGDumpLayer                              */
/************************************************************************/


class OGRPGDumpDataSource;

class OGRPGDumpLayer : public OGRLayer
{
    char                *pszSchemaName;
    char                *pszSqlTableName;
    char                *pszFIDColumn;
    OGRFeatureDefn      *poFeatureDefn;
    OGRPGDumpDataSource *poDS;
    int                 nFeatures;
    int                 bLaunderColumnNames;
    int                 bPreservePrecision;
    int                 bUseCopy;
    int                 bWriteAsHex;
    int                 bCopyActive;
    int                 bFIDColumnInCopyFields;
    int                 bCreateTable;
    int                 nUnknownSRSId;
    int                 nForcedSRSId;
    int                 bCreateSpatialIndexFlag;

    char              **papszOverrideColumnTypes;

    void                AppendFieldValue(CPLString& osCommand,
                                       OGRFeature* poFeature, int i);
    char*               GByteArrayToBYTEA( const GByte* pabyData, int nLen);
    char*               GeometryToHex( OGRGeometry * poGeometry, int nSRSId );
    
    OGRErr              StartCopy(int bSetFID);
    CPLString           BuildCopyFields(int bSetFID);

  public:
                        OGRPGDumpLayer(OGRPGDumpDataSource* poDS,
                                       const char* pszSchemaName,
                                       const char* pszLayerName,
                                       const char *pszFIDColumn,
                                       int         bWriteAsHexIn,
                                       int         bCreateTable);
    virtual             ~OGRPGDumpLayer();

    virtual OGRFeatureDefn *GetLayerDefn() {return poFeatureDefn;}
    
    virtual void        ResetReading()  { }
    virtual int         TestCapability( const char * );
    
    virtual OGRErr      CreateFeature( OGRFeature *poFeature );
    virtual OGRErr      CreateFeatureViaInsert( OGRFeature *poFeature );
    virtual OGRErr      CreateFeatureViaCopy( OGRFeature *poFeature );

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );
    virtual OGRErr      CreateGeomField( OGRGeomFieldDefn *poGeomField,
                                         int bApproxOK = TRUE );

    virtual OGRFeature *GetNextFeature();

    // follow methods are not base class overrides
    void                SetLaunderFlag( int bFlag )
                                { bLaunderColumnNames = bFlag; }
    void                SetPrecisionFlag( int bFlag )
                                { bPreservePrecision = bFlag; }

    void                SetOverrideColumnTypes( const char* pszOverrideColumnTypes );
    void                SetUnknownSRSId( int nUnknownSRSIdIn )
                                { nUnknownSRSId = nUnknownSRSIdIn; }
    void                SetForcedSRSId( int nForcedSRSIdIn )
                                { nForcedSRSId = nForcedSRSIdIn; }
    void                SetCreateSpatialIndexFlag( int bFlag )
                                { bCreateSpatialIndexFlag = bFlag; }
    OGRErr              EndCopy();
};

/************************************************************************/
/*                       OGRPGDumpDataSource                            */
/************************************************************************/
class OGRPGDumpDataSource : public OGRDataSource
{
    int                 nLayers;
    OGRPGDumpLayer**    papoLayers;
    char*               pszName;
    int                 bTriedOpen;
    VSILFILE*           fp;
    int                 bInTransaction;
    OGRPGDumpLayer*     poLayerInCopyMode;
    const char*         pszEOL;

  public:
                        OGRPGDumpDataSource(const char* pszName,
                                            char** papszOptions);
                        ~OGRPGDumpDataSource();
                        
    char               *LaunderName( const char *pszSrcName );
    void                Log(const char* pszStr, int bAddSemiColumn = TRUE);

    virtual const char  *GetName() { return pszName; }
    virtual int         GetLayerCount() { return nLayers; }
    virtual OGRLayer   *GetLayer( int );

    virtual OGRLayer    *CreateLayer( const char *,
                                      OGRSpatialReference * = NULL,
                                      OGRwkbGeometryType = wkbUnknown,
                                      char ** = NULL );

    virtual int         TestCapability( const char * );

    void                StartTransaction();
    void                Commit();

    void                StartCopy( OGRPGDumpLayer *poPGLayer );
    OGRErr              EndCopy( );
};

/************************************************************************/
/*                             OGRPGDriver                              */
/************************************************************************/

class OGRPGDumpDriver : public OGRSFDriver
{
  public:
                ~OGRPGDumpDriver();

    virtual const char    *GetName();
    virtual OGRDataSource *Open( const char *, int );

    virtual OGRDataSource *CreateDataSource( const char *pszName,
                                             char ** = NULL );

    virtual int            TestCapability( const char * );
};

#endif /* ndef _OGR_PGDUMP_H_INCLUDED */

