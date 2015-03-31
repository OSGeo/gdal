/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for OGR/PostgreSQL dump driver.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
CPLString CPL_DLL OGRPGCommonLayerGetType(OGRFieldDefn& oField,
                                          int bPreservePrecision,
                                          int bApproxOK);
int CPL_DLL OGRPGCommonLayerSetType(OGRFieldDefn& oField,
                                    const char* pszType,
                                    const char* pszFormatType,
                                    int nWidth);
void CPL_DLL OGRPGCommonLayerNormalizeDefault(OGRFieldDefn* poFieldDefn,
                                              const char* pszDefault);
CPLString CPL_DLL OGRPGCommonLayerGetPGDefault(OGRFieldDefn* poFieldDefn);

typedef CPLString (*OGRPGCommonEscapeStringCbk)(void* userdata,
                                                const char* pszValue, 
                                                int nWidth,
                                                const char* pszLayerName,
                                                const char* pszFieldRef);
void CPL_DLL OGRPGCommonAppendCopyFieldsExceptGeom(CPLString& osCommand,
                                           OGRFeature* poFeature,
                                           const char* pszFIDColumn,
                                           int bFIDColumnInCopyFields,
                                           OGRPGCommonEscapeStringCbk pfnEscapeString,
                                           void* userdata);
void CPL_DLL OGRPGCommonAppendFieldValue(CPLString& osCommand,
                                 OGRFeature* poFeature, int i,
                                 OGRPGCommonEscapeStringCbk pfnEscapeString,
                                 void* userdata);

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
    int                 bPostGIS2;

    int                 iNextShapeId;
    int                 iFIDAsRegularColumnIndex;
    int                 bAutoFIDOnCreateViaCopy;
    int                 bCopyStatementWithFID;

    char              **papszOverrideColumnTypes;

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
    virtual const char* GetFIDColumn() { return pszFIDColumn; }
    
    virtual void        ResetReading()  { }
    virtual int         TestCapability( const char * );
    
    virtual OGRErr      ICreateFeature( OGRFeature *poFeature );
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
    void                SetPostGIS2( int bFlag )
                                { bPostGIS2 = bFlag; }
    OGRErr              EndCopy();

    static char*        GByteArrayToBYTEA( const GByte* pabyData, int nLen);
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
    int                 Log(const char* pszStr, int bAddSemiColumn = TRUE);

    virtual const char  *GetName() { return pszName; }
    virtual int         GetLayerCount() { return nLayers; }
    virtual OGRLayer   *GetLayer( int );

    virtual OGRLayer    *ICreateLayer( const char *,
                                      OGRSpatialReference * = NULL,
                                      OGRwkbGeometryType = wkbUnknown,
                                      char ** = NULL );

    virtual int         TestCapability( const char * );

    void                LogStartTransaction();
    void                LogCommit();

    void                StartCopy( OGRPGDumpLayer *poPGLayer );
    OGRErr              EndCopy( );
};

#endif /* ndef _OGR_PGDUMP_H_INCLUDED */

