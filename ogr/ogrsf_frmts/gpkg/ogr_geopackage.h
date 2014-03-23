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
#include "sqlite3.h"

/* 1.1.1: A GeoPackage SHALL contain 0x47503130 ("GP10" in ASCII) in the application id */
/* http://opengis.github.io/geopackage/#_file_format */
/* 0x47503130 = 1196437808 */
#define GPKG_APPLICATION_ID 1196437808

#define UNDEFINED_SRID 0



  

/************************************************************************/
/*                           OGRGeoPackageDriver                        */
/************************************************************************/

class OGRGeoPackageDriver : public OGRSFDriver
{
    public:
                            ~OGRGeoPackageDriver();
        const char*         GetName();
        OGRDataSource*      Open( const char *, int );
        OGRDataSource*      CreateDataSource( const char * pszFilename, char **papszOptions );
        OGRErr              DeleteDataSource( const char * pszFilename );
        int                 TestCapability( const char * );
};


/************************************************************************/
/*                           OGRGeoPackageDataSource                    */
/************************************************************************/

class OGRGeoPackageDataSource : public OGRDataSource
{
    char*               m_pszFileName;
    OGRLayer**          m_papoLayers;
    int                 m_nLayers;
    int                 m_bUpdate;
    int                 m_bUtf8;
    sqlite3*            m_poDb;
    
    
    public:
                            OGRGeoPackageDataSource();
                            ~OGRGeoPackageDataSource();

        virtual const char* GetName() { return m_pszFileName; }
        virtual int         GetLayerCount() { return m_nLayers; }
        int                 Open( const char * pszFilename, int bUpdate );
        int                 Create( const char * pszFilename, char **papszOptions );
        OGRLayer*           GetLayer( int iLayer );
        int                 DeleteLayer( int iLayer );
        OGRLayer*           CreateLayer( const char * pszLayerName,
                                         OGRSpatialReference * poSpatialRef,
                                         OGRwkbGeometryType eGType,
                                         char **papszOptions );
        int                 TestCapability( const char * );
        
        int                 IsUpdatable() { return m_bUpdate; }
        int                 GetSrsId( const OGRSpatialReference * poSRS );
        const char*         GetSrsName( const OGRSpatialReference * poSRS );
        OGRSpatialReference* GetSpatialRef( int iSrsId );
        sqlite3*            GetDatabaseHandle();
        virtual int         GetUTF8() { return m_bUtf8; }
        OGRErr              AddColumn( const char * pszTableName, 
                                       const char * pszColumnName, 
                                       const char * pszColumnType );

    private:
    
        OGRErr              PragmaCheck(const char * pszPragma, const char * pszExpected, int nRowsExpected);
        bool                CheckApplicationId(const char * pszFileName);
        OGRErr              SetApplicationId();
    
};


/************************************************************************/
/*                           OGRGeoPackageLayer                         */
/************************************************************************/

class OGRGeoPackageLayer : public OGRLayer
{
    char*                       m_pszTableName;
    char*                       m_pszFidColumn;
    int                         m_iSrs;
    OGRGeoPackageDataSource*    m_poDS;
    OGREnvelope*                m_poExtent;
    CPLString                   m_soColumns;
    CPLString                   m_soFilter;
    OGRBoolean                  m_bExtentChanged;
    OGRFeatureDefn*             m_poFeatureDefn;
    sqlite3_stmt*               m_poQueryStatement;
    sqlite3_stmt*               m_poUpdateStatement;
    sqlite3_stmt*               m_poInsertStatement;
    sqlite3_stmt*               m_poFidStatement;    
    
    public:
    
                        OGRGeoPackageLayer( OGRGeoPackageDataSource *poDS,
                                            const char * pszTableName );
                        ~OGRGeoPackageLayer();

    /************************************************************************/
    /* OGR API methods */
                        
    OGRFeatureDefn*     GetLayerDefn() { return m_poFeatureDefn; }
    int                 TestCapability( const char * );
    OGRErr              CreateField( OGRFieldDefn *poField, int bApproxOK = TRUE );
    void                ResetReading();
	OGRErr				CreateFeature( OGRFeature *poFeater );
    OGRErr              SetFeature( OGRFeature *poFeature );
    OGRErr              DeleteFeature(long nFID);
    OGRErr              SetAttributeFilter( const char *pszQuery );
    OGRErr              SyncToDisk();
    OGRFeature*         GetNextFeature();
    OGRFeature*         GetFeature(long nFID);
    const char*         GetFIDColumn();	
    OGRErr              StartTransaction();
    OGRErr              CommitTransaction();
    OGRErr              RollbackTransaction();
    int                 GetFeatureCount( int );
    OGRErr              GetExtent(OGREnvelope *psExtent, int bForce = TRUE);
    
    // void                SetSpatialFilter( int iGeomField, OGRGeometry * poGeomIn );

    OGRErr              ReadTableDefinition();

    /************************************************************************/
    /* GPKG methods */
    
    private:
    
    OGRErr              ReadFeature( sqlite3_stmt *poQuery, OGRFeature **ppoFeature );
    OGRErr              UpdateExtent( const OGREnvelope *poExtent );
    OGRErr              SaveExtent();
    OGRErr              BuildColumns();
    OGRBoolean          IsGeomFieldSet( OGRFeature *poFeature );
    CPLString           FeatureGenerateUpdateSQL( OGRFeature *poFeature );
    CPLString           FeatureGenerateInsertSQL( OGRFeature *poFeature );
    OGRErr              FeatureBindUpdateParameters( OGRFeature *poFeature, sqlite3_stmt *poStmt );
    OGRErr              FeatureBindInsertParameters( OGRFeature *poFeature, sqlite3_stmt *poStmt );
    OGRErr              FeatureBindParameters( OGRFeature *poFeature, sqlite3_stmt *poStmt, int *pnColCount );

};



#endif /* _OGR_GEOPACKAGE_H_INCLUDED */