/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for OGR/PostgreSQL driver.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.7  2001/11/15 21:19:58  warmerda
 * added soft transaction semantics
 *
 * Revision 1.6  2001/09/28 04:03:52  warmerda
 * partially upraded to PostGIS 0.6
 *
 * Revision 1.5  2001/06/26 20:59:13  warmerda
 * implement efficient spatial and attribute query support
 *
 * Revision 1.4  2001/06/19 22:29:12  warmerda
 * upgraded to include PostGIS support
 *
 * Revision 1.3  2001/06/19 15:50:23  warmerda
 * added feature attribute query support
 *
 * Revision 1.2  2000/11/23 06:03:35  warmerda
 * added Oid support
 *
 * Revision 1.1  2000/10/17 17:46:51  warmerda
 * New
 *
 */

#ifndef _OGR_PG_H_INCLUDED
#define _OGR_PG_H_INLLUDED

#include "ogrsf_frmts.h"
#include "libpq-fe.h"

/************************************************************************/
/*                            OGRPGLayer                                */
/************************************************************************/

class OGRPGDataSource;
    
class OGRPGLayer : public OGRLayer
{
    OGRFeatureDefn     *poFeatureDefn;
    OGRGeometry		*poFilterGeom;
    int			iNextShapeId;

    int			bUpdateAccess;

    char               *GeometryToBYTEA( OGRGeometry * );
    OGRGeometry        *BYTEAToGeometry( const char * );
    Oid                 GeometryToOID( OGRGeometry * );
    OGRGeometry        *OIDToGeometry( Oid );

    OGRFeatureDefn     *ReadTableDefinition(const char *);
    OGRPGDataSource    *poDS;

    char	       *pszCursorName;
    PGresult	       *hCursorResult;
    int                 nResultOffset;

    int			bHasWkb;
    int                 bWkbAsOid;
    int                 bHasFid;
    int			bHasPostGISGeometry;
    char		*pszGeomColumn;

    void		BuildWhere(void);
    char 	       *BuildFields(void);

    char	        *pszQuery;
    char		*pszWHERE;

  public:
    			OGRPGLayer( OGRPGDataSource *,
                                    const char * pszName,
                                    int bUpdate );
    			~OGRPGLayer();

    OGRGeometry *	GetSpatialFilter() { return poFilterGeom; }
    void		SetSpatialFilter( OGRGeometry * );

    virtual OGRErr      SetAttributeFilter( const char * );

    void		ResetReading();
    OGRFeature *	GetNextRawFeature();
    OGRFeature *	GetNextFeature();

    OGRFeature         *GetFeature( long nFeatureId );
    OGRErr              SetFeature( OGRFeature *poFeature );
    OGRErr              CreateFeature( OGRFeature *poFeature );
    
    OGRFeatureDefn *	GetLayerDefn() { return poFeatureDefn; }

    int                 GetFeatureCount( int );

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );
    
    virtual OGRErr       StartTransaction();
    virtual OGRErr       CommitTransaction();
    virtual OGRErr       RollbackTransaction();

    int                 TestCapability( const char * );
};

/************************************************************************/
/*                           OGRPGDataSource                            */
/************************************************************************/

class OGRPGDataSource : public OGRDataSource
{
    OGRPGLayer        **papoLayers;
    int			nLayers;
    
    char		*pszName;

    int			bDSUpdate;
    int			bHavePostGIS;
    int			nSoftTransactionLevel;

    PGconn		*hPGConn;

    void		DeleteLayer( const char *pszLayerName );
    
  public:
    			OGRPGDataSource();
    			~OGRPGDataSource();

    PGconn             *GetPGConn() { return hPGConn; }

    int                 FetchSRSId( OGRSpatialReference * poSRS );
    OGRSpatialReference *FetchSRS( int nSRSId );
    OGRErr              InitializeMetadataTables();

    int			Open( const char *, int bUpdate, int bTestOpen );
    int                 OpenTable( const char *, int bUpdate, int bTestOpen );

    const char	        *GetName() { return pszName; }
    int			GetLayerCount() { return nLayers; }
    OGRLayer		*GetLayer( int );

    virtual OGRLayer    *CreateLayer( const char *, 
                                      OGRSpatialReference * = NULL,
                                      OGRwkbGeometryType = wkbUnknown,
                                      char ** = NULL );

    int                 TestCapability( const char * );

    OGRErr              SoftStartTransaction();
    OGRErr              SoftCommit();
    OGRErr              SoftRollback();
    
    OGRErr              FlushSoftTransaction();
};

/************************************************************************/
/*                             OGRPGDriver                              */
/************************************************************************/

class OGRPGDriver : public OGRSFDriver
{
  public:
    		~OGRPGDriver();
                
    const char *GetName();
    OGRDataSource *Open( const char *, int );

    virtual OGRDataSource *CreateDataSource( const char *pszName,
                                             char ** = NULL );
    
    int                 TestCapability( const char * );
};


#endif /* ndef _OGR_PG_H_INCLUDED */


