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

#ifndef OGR_PGDUMP_H_INCLUDED
#define OGR_PGDUMP_H_INCLUDED

#include "ogrsf_frmts.h"
#include "cpl_string.h"

CPLString OGRPGDumpEscapeColumnName(const char* pszColumnName);
CPLString OGRPGDumpEscapeString( const char* pszStrValue, int nMaxLength = -1,
                                 const char* pszFieldName = "");
CPLString CPL_DLL OGRPGCommonLayerGetType( OGRFieldDefn& oField,
                                           bool bPreservePrecision,
                                           bool bApproxOK );
bool CPL_DLL OGRPGCommonLayerSetType( OGRFieldDefn& oField,
                                      const char* pszType,
                                      const char* pszFormatType,
                                      int nWidth );
void CPL_DLL OGRPGCommonLayerNormalizeDefault( OGRFieldDefn* poFieldDefn,
                                               const char* pszDefault );
CPLString CPL_DLL OGRPGCommonLayerGetPGDefault( OGRFieldDefn* poFieldDefn );

typedef CPLString (*OGRPGCommonEscapeStringCbk)( void* userdata,
                                                 const char* pszValue,
                                                 int nWidth,
                                                 const char* pszLayerName,
                                                 const char* pszFieldRef );
void CPL_DLL
OGRPGCommonAppendCopyFieldsExceptGeom(
    CPLString& osCommand,
    OGRFeature* poFeature,
    const char* pszFIDColumn,
    bool bFIDColumnInCopyFields,
    OGRPGCommonEscapeStringCbk pfnEscapeString,
    void* userdata );

void CPL_DLL OGRPGCommonAppendFieldValue(
    CPLString& osCommand,
    OGRFeature* poFeature, int i,
    OGRPGCommonEscapeStringCbk pfnEscapeString,
    void* userdata);

char CPL_DLL *OGRPGCommonLaunderName( const char *pszSrcName,
                                      const char* pszDebugPrefix = "OGR" );

/************************************************************************/
/*                        OGRPGDumpGeomFieldDefn                        */
/************************************************************************/

class OGRPGDumpGeomFieldDefn : public OGRGeomFieldDefn
{
    public:
        explicit OGRPGDumpGeomFieldDefn( OGRGeomFieldDefn *poGeomField ) :
            OGRGeomFieldDefn(poGeomField),
            nSRSId(-1),
            GeometryTypeFlags(0)
            {}

        int nSRSId;
        int GeometryTypeFlags;
};

/************************************************************************/
/*                          OGRPGDumpLayer                              */
/************************************************************************/

class OGRPGDumpDataSource;

class OGRPGDumpLayer : public OGRLayer
{
    char                *pszSchemaName;
    char                *pszSqlTableName;
    CPLString           osForcedDescription;
    char                *pszFIDColumn;
    OGRFeatureDefn      *poFeatureDefn;
    OGRPGDumpDataSource *poDS;
    bool                bLaunderColumnNames;
    bool                bPreservePrecision;
    int                 bUseCopy;
    bool                bWriteAsHex;
    bool                bCopyActive;
    bool                bFIDColumnInCopyFields;
    int                 bCreateTable;
    int                 nUnknownSRSId;
    int                 nForcedSRSId;
    int                 nForcedGeometryTypeFlags;
    bool                bCreateSpatialIndexFlag;
    int                 nPostGISMajor;
    int                 nPostGISMinor;

    int                 iNextShapeId;
    int                 iFIDAsRegularColumnIndex;
    bool                bAutoFIDOnCreateViaCopy;
    bool                bCopyStatementWithFID;

    char              **papszOverrideColumnTypes;

    CPLString           m_osFirstGeometryFieldName;

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

    virtual OGRFeatureDefn *GetLayerDefn() override { return poFeatureDefn; }
    virtual const char* GetFIDColumn() override { return pszFIDColumn; }

    virtual void        ResetReading() override {}
    virtual int         TestCapability( const char * ) override;

    virtual OGRErr      ICreateFeature( OGRFeature *poFeature ) override;
    virtual OGRErr      CreateFeatureViaInsert( OGRFeature *poFeature );
    virtual OGRErr      CreateFeatureViaCopy( OGRFeature *poFeature );

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE ) override;
    virtual OGRErr      CreateGeomField( OGRGeomFieldDefn *poGeomField,
                                         int bApproxOK = TRUE ) override;

    virtual OGRFeature *GetNextFeature() override;

    virtual CPLErr      SetMetadata( char** papszMD,
                                     const char* pszDomain = "" ) override;
    virtual CPLErr      SetMetadataItem( const char* pszName,
                                         const char* pszValue,
                                         const char* pszDomain = "" ) override;

    // follow methods are not base class overrides
    void                SetLaunderFlag( bool bFlag )
                                { bLaunderColumnNames = bFlag; }
    void                SetPrecisionFlag( bool bFlag )
                                { bPreservePrecision = bFlag; }

    void                SetOverrideColumnTypes( const char* pszOverrideColumnTypes );
    void                SetUnknownSRSId( int nUnknownSRSIdIn )
                                { nUnknownSRSId = nUnknownSRSIdIn; }
    void                SetForcedSRSId( int nForcedSRSIdIn )
                                { nForcedSRSId = nForcedSRSIdIn; }
    void                SetForcedGeometryTypeFlags( int GeometryTypeFlagsIn )
                                { nForcedGeometryTypeFlags = GeometryTypeFlagsIn; }
    void                SetCreateSpatialIndexFlag( bool bFlag )
                                { bCreateSpatialIndexFlag = bFlag; }
    void                SetPostGISVersion(int nPostGISMajorIn, int nPostGISMinorIn)
                                { nPostGISMajor = nPostGISMajorIn; nPostGISMinor = nPostGISMinorIn; }
    void                SetGeometryFieldName( const char* pszGeomFieldName )
                                { m_osFirstGeometryFieldName = pszGeomFieldName; }
    void                SetForcedDescription( const char* pszDescriptionIn );
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
    bool                bTriedOpen;
    VSILFILE*           fp;
    bool                bInTransaction;
    OGRPGDumpLayer*     poLayerInCopyMode;
    const char*         pszEOL;

  public:
                        OGRPGDumpDataSource(const char* pszName,
                                            char** papszOptions);
                        virtual ~OGRPGDumpDataSource();

    bool                Log( const char* pszStr, bool bAddSemiColumn = true );

    virtual const char  *GetName() override { return pszName; }
    virtual int         GetLayerCount() override { return nLayers; }
    virtual OGRLayer   *GetLayer( int ) override;

    virtual OGRLayer    *ICreateLayer( const char *,
                                      OGRSpatialReference * = NULL,
                                      OGRwkbGeometryType = wkbUnknown,
                                      char ** = NULL ) override;

    virtual int         TestCapability( const char * ) override;

    void                LogStartTransaction();
    void                LogCommit();

    void                StartCopy( OGRPGDumpLayer *poPGLayer );
    OGRErr              EndCopy( );
};

#endif /* ndef OGR_PGDUMP_H_INCLUDED */
