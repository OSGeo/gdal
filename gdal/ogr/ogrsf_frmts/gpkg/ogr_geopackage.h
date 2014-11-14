/******************************************************************************
 * $Id$
 *
 * Project:  GeoPackage Translator
 * Purpose:  Definition of classes for OGR GeoPackage driver.
 * Author:   Paul Ramsey, pramsey@boundlessgeo.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Paul Ramsey <pramsey@boundlessgeo.com>
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

#ifndef _OGR_GEOPACKAGE_H_INCLUDED
#define _OGR_GEOPACKAGE_H_INCLUDED

#include "ogrsf_frmts.h"
#include "ogr_sqlite.h"

#define UNDEFINED_SRID 0

/************************************************************************/
/*                           OGRGeoPackageDataSource                    */
/************************************************************************/

class OGRGeoPackageTableLayer;

class OGRGeoPackageDataSource : public OGRSQLiteBaseDataSource
{
    OGRGeoPackageTableLayer** m_papoLayers;
    int                 m_nLayers;
    int                 m_bUtf8;
    void                CheckUnknownExtensions();

    public:
                            OGRGeoPackageDataSource();
                            ~OGRGeoPackageDataSource();

        virtual int         GetLayerCount() { return m_nLayers; }
        int                 Open( const char * pszFilename, int bUpdate );
        int                 Create( const char * pszFilename, char **papszOptions );
        OGRLayer*           GetLayer( int iLayer );
        int                 DeleteLayer( int iLayer );
        OGRLayer*           ICreateLayer( const char * pszLayerName,
                                         OGRSpatialReference * poSpatialRef,
                                         OGRwkbGeometryType eGType,
                                         char **papszOptions );
        int                 TestCapability( const char * );
        
        virtual std::pair<OGRLayer*, IOGRSQLiteGetSpatialWhere*> GetLayerWithGetSpatialWhereByName( const char* pszName );

        virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                        OGRGeometry *poSpatialFilter,
                                        const char *pszDialect );
        virtual void        ReleaseResultSet( OGRLayer * poLayer );

        int                 GetSrsId( const OGRSpatialReference * poSRS );
        const char*         GetSrsName( const OGRSpatialReference * poSRS );
        OGRSpatialReference* GetSpatialRef( int iSrsId );
        virtual int         GetUTF8() { return m_bUtf8; }
        OGRErr              AddColumn( const char * pszTableName, 
                                       const char * pszColumnName, 
                                       const char * pszColumnType );
        OGRErr              CreateExtensionsTableIfNecessary();
        int                 HasExtensionsTable();

    private:
    
        OGRErr              PragmaCheck(const char * pszPragma, const char * pszExpected, int nRowsExpected);
        OGRErr              SetApplicationId();
        int                 OpenOrCreateDB(int flags);
        int                 HasGDALAspatialExtension();
        OGRErr              CreateGDALAspatialExtension();
};

/************************************************************************/
/*                           OGRGeoPackageLayer                         */
/************************************************************************/

class OGRGeoPackageLayer : public OGRLayer, public IOGRSQLiteGetSpatialWhere
{
  protected:
    OGRGeoPackageDataSource *m_poDS;

    OGRFeatureDefn*      m_poFeatureDefn;
    int                  iNextShapeId;

    sqlite3_stmt        *m_poQueryStatement;
    int                  bDoStep;

    char                *m_pszFidColumn;

    int                 iFIDCol;
    int                 iGeomCol;
    int                *panFieldOrdinals;

    void                ClearStatement();
    virtual OGRErr      ResetStatement() = 0;
    
    void                BuildFeatureDefn( const char *pszLayerName,
                                           sqlite3_stmt *hStmt );

    OGRFeature*         TranslateFeature(sqlite3_stmt* hStmt);

  public:

                        OGRGeoPackageLayer(OGRGeoPackageDataSource* poDS);
                        ~OGRGeoPackageLayer();
    /************************************************************************/
    /* OGR API methods */

    OGRFeature*         GetNextFeature();
    const char*         GetFIDColumn();
    void                ResetReading();
    int                 TestCapability( const char * );
    OGRFeatureDefn*     GetLayerDefn() { return m_poFeatureDefn; }

    virtual int          HasFastSpatialFilter(CPL_UNUSED int iGeomCol) { return FALSE; }
    virtual CPLString    GetSpatialWhere(CPL_UNUSED int iGeomCol,
                                         CPL_UNUSED OGRGeometry* poFilterGeom) { return ""; }

};

/************************************************************************/
/*                        OGRGeoPackageTableLayer                       */
/************************************************************************/

class OGRGeoPackageTableLayer : public OGRGeoPackageLayer
{
    char*                       m_pszTableName;
    int                         m_iSrs;
    OGREnvelope*                m_poExtent;
    CPLString                   m_soColumns;
    CPLString                   m_soFilter;
    CPLString                   osQuery;
    OGRBoolean                  m_bExtentChanged;
    sqlite3_stmt*               m_poUpdateStatement;
    sqlite3_stmt*               m_poInsertStatement;
    int                         bDeferedSpatialIndexCreation;
    int                         m_bHasSpatialIndex;
    int                         bDropRTreeTable;
    int                         m_anHasGeometryExtension[wkbMultiSurface+1];

    virtual OGRErr      ResetStatement();
    
    void                BuildWhere(void);
    
    public:
    
                        OGRGeoPackageTableLayer( OGRGeoPackageDataSource *poDS,
                                            const char * pszTableName );
                        ~OGRGeoPackageTableLayer();

    /************************************************************************/
    /* OGR API methods */
                        
    int                 TestCapability( const char * );
    OGRErr              CreateField( OGRFieldDefn *poField, int bApproxOK = TRUE );
    void                ResetReading();
	OGRErr              ICreateFeature( OGRFeature *poFeater );
    OGRErr              ISetFeature( OGRFeature *poFeature );
    OGRErr              DeleteFeature(long nFID);
    virtual void        SetSpatialFilter( OGRGeometry * );
    OGRErr              SetAttributeFilter( const char *pszQuery );
    OGRErr              SyncToDisk();
    OGRFeature*         GetNextFeature();
    OGRFeature*         GetFeature(long nFID);
    OGRErr              StartTransaction();
    OGRErr              CommitTransaction();
    OGRErr              RollbackTransaction();
    int                 GetFeatureCount( int );
    OGRErr              GetExtent(OGREnvelope *psExtent, int bForce = TRUE);
    
    // void                SetSpatialFilter( int iGeomField, OGRGeometry * poGeomIn );

    OGRErr              ReadTableDefinition(int bIsSpatial);
    void                SetDeferedSpatialIndexCreation( int bFlag )
                                { bDeferedSpatialIndexCreation = bFlag; }

    void                CreateSpatialIndexIfNecessary();
    int                 CreateSpatialIndex();
    int                 DropSpatialIndex(int bCalledFromSQLFunction = FALSE);

    void                RenameTo(const char* pszDstTableName);

    virtual int          HasFastSpatialFilter(int iGeomCol);
    virtual CPLString    GetSpatialWhere(int iGeomCol,
                                         OGRGeometry* poFilterGeom);

    int                 HasSpatialIndex();
    int                 CreateGeometryExtensionIfNecessary(OGRwkbGeometryType eGType);

    /************************************************************************/
    /* GPKG methods */
    
    private:
    
    OGRErr              UpdateExtent( const OGREnvelope *poExtent );
    OGRErr              SaveExtent();
    OGRErr              BuildColumns();
    OGRBoolean          IsGeomFieldSet( OGRFeature *poFeature );
    CPLString           FeatureGenerateUpdateSQL( OGRFeature *poFeature );
    CPLString           FeatureGenerateInsertSQL( OGRFeature *poFeature, int bAddFID );
    OGRErr              FeatureBindUpdateParameters( OGRFeature *poFeature, sqlite3_stmt *poStmt );
    OGRErr              FeatureBindInsertParameters( OGRFeature *poFeature, sqlite3_stmt *poStmt, int bAddFID );
    OGRErr              FeatureBindParameters( OGRFeature *poFeature, sqlite3_stmt *poStmt, int *pnColCount, int bAddFID );

    void                CheckUnknownExtensions();
};

/************************************************************************/
/*                         OGRGeoPackageSelectLayer                     */
/************************************************************************/

class OGRGeoPackageSelectLayer : public OGRGeoPackageLayer, public IOGRSQLiteSelectLayer
{
    OGRSQLiteSelectLayerCommonBehaviour* poBehaviour;

    virtual OGRErr      ResetStatement();

  public:
                        OGRGeoPackageSelectLayer( OGRGeoPackageDataSource *, 
                                              CPLString osSQL,
                                              sqlite3_stmt *,
                                              int bUseStatementForGetNextFeature,
                                              int bEmptyLayer );
                       ~OGRGeoPackageSelectLayer();

    virtual void        ResetReading();

    virtual OGRFeature *GetNextFeature();
    virtual int         GetFeatureCount( int );

    virtual void        SetSpatialFilter( OGRGeometry * poGeom ) { SetSpatialFilter(0, poGeom); }
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry * );
    virtual OGRErr      SetAttributeFilter( const char * );

    virtual int         TestCapability( const char * );

    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE) { return GetExtent(0, psExtent, bForce); }
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce = TRUE);

    virtual OGRFeatureDefn *     GetLayerDefn() { return OGRGeoPackageLayer::GetLayerDefn(); }
    virtual char*&               GetAttrQueryString() { return m_pszAttrQueryString; }
    virtual OGRFeatureQuery*&    GetFeatureQuery() { return m_poAttrQuery; }
    virtual OGRGeometry*&        GetFilterGeom() { return m_poFilterGeom; }
    virtual int&                 GetIGeomFieldFilter() { return m_iGeomFieldFilter; }
    virtual OGRSpatialReference* GetSpatialRef() { return OGRGeoPackageLayer::GetSpatialRef(); }
    virtual int                  InstallFilter( OGRGeometry * poGeomIn ) { return OGRGeoPackageLayer::InstallFilter(poGeomIn); }
    virtual int                  HasReadFeature() { return iNextShapeId > 0; }
    virtual void                 BaseResetReading() { OGRGeoPackageLayer::ResetReading(); }
    virtual OGRFeature          *BaseGetNextFeature() { return OGRGeoPackageLayer::GetNextFeature(); }
    virtual OGRErr               BaseSetAttributeFilter(const char* pszQuery) { return OGRGeoPackageLayer::SetAttributeFilter(pszQuery); }
    virtual int                  BaseGetFeatureCount(int bForce) { return OGRGeoPackageLayer::GetFeatureCount(bForce); }
    virtual int                  BaseTestCapability( const char *pszCap ) { return OGRGeoPackageLayer::TestCapability(pszCap); }
    virtual OGRErr               BaseGetExtent(OGREnvelope *psExtent, int bForce) { return OGRGeoPackageLayer::GetExtent(psExtent, bForce); }
    virtual OGRErr               BaseGetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) { return OGRGeoPackageLayer::GetExtent(iGeomField, psExtent, bForce); }
};


#endif /* _OGR_GEOPACKAGE_H_INCLUDED */
