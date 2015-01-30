/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for OGR SDE driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 * Copyright (c) 2008, Shawn Gervais <project10@project10.net> 
 * Copyright (c) 2008, Howard Butler <hobu.inc@gmail.com>
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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

#ifndef _OGR_SDE_H_INCLUDED
#define _OGR_SDE_H_INCLUDED

#include "ogrsf_frmts.h"

#include <sdetype.h> /* ESRI SDE Client Includes */
#include <sdeerno.h>
#include <vector>
#include "cpl_string.h"

#define OGR_SDE_LAYER_CO_GRID1 1000
#define OGR_SDE_LAYER_CO_GRID2 0
#define OGR_SDE_LAYER_CO_GRID3 0
#define OGR_SDE_LAYER_CO_INIT_FEATS 50
#define OGR_SDE_LAYER_CO_AVG_PTS 5

/************************************************************************/
/*                            OGRSDELayer                                */
/************************************************************************/


class OGRSDEDataSource;

class OGRSDELayer : public OGRLayer
{
  protected:
    OGRFeatureDefn     *poFeatureDefn;

    // Layer spatial reference system, and srid.
    OGRSpatialReference *poSRS;

    char               *pszOwnerName;
    char               *pszDbTableName;
    
    int                 bUpdateAccess;
    int                 bVersioned;
    int                 bPreservePrecision;
    
    CPLString           osAttributeFilter;

    int                 bQueryInstalled;
    int                 bQueryActive;

    SE_STREAM           hStream;
    
    int                 bHaveLayerInfo;
    SE_LAYERINFO        hLayerInfo;
    SE_COORDREF         hCoordRef;

    OGRSDEDataSource    *poDS;

    int                 iFIDColumn;
    LONG                nFIDColumnType;

    int                 nNextFID;
    int                 iNextFIDToWrite;

    int                 iShapeColumn;

    int                 bUseNSTRING; 


    char              **papszAllColumns;
    std::vector<int>    anFieldMap;     // SDE index of OGR field.
    std::vector<int>    anFieldTypeMap; // SDE type

    int                 InstallQuery( int );
    OGRFeature         *TranslateSDERecord();
    OGRGeometry        *TranslateSDEGeometry( SE_SHAPE );
    OGRErr              TranslateOGRRecord( OGRFeature *, int );
    OGRErr              TranslateOGRGeometry( OGRGeometry *, SE_SHAPE *,
                                              SE_COORDREF );
    int                 NeedLayerInfo();

    // This process can be fairly expensive depending on the configuration
    // of the layer in SDE. Enable this feature with OGR_SDE_GETLAYERTYPE.
    OGRwkbGeometryType  DiscoverLayerType();

  public:

                        OGRSDELayer( OGRSDEDataSource *,
                                     int );
    virtual             ~OGRSDELayer();

    CPLString           osFIDColumnName;
    CPLString           osShapeColumnName;
    
    int                 Initialize( const char *, const char *, const char * );

    virtual void        ResetReading();
    OGRErr              ResetStream();

    virtual OGRFeature *GetNextFeature();
    virtual OGRFeature *GetFeature( GIntBig nFeatureId );
    virtual OGRErr      GetExtent( OGREnvelope *psExtent, int bForce );
    virtual GIntBig     GetFeatureCount( int bForce );

    virtual OGRErr      SetAttributeFilter( const char *pszQuery );
    
    virtual OGRErr      CreateField( OGRFieldDefn *poFieldIn,
                                     int bApproxOK );

    virtual OGRErr      ISetFeature( OGRFeature *poFeature );
    virtual OGRErr      ICreateFeature( OGRFeature *poFeature );
    virtual OGRErr      DeleteFeature( GIntBig nFID );
    
    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }
    
    virtual OGRSpatialReference *GetSpatialRef();

    virtual int         TestCapability( const char * );
    
    // The following methods are not base class overrides
    //void                SetOptions( char ** );
    
    void                SetFIDColType( LONG nType )
                                { nFIDColumnType = nType; }
    void                SetPrecisionFlag( int bFlag )
                                { bPreservePrecision = bFlag; }
    void                SetUseNSTRING( int bFlag )
                                { bUseNSTRING = bFlag; }
};

/************************************************************************/
/*                           OGRSDEDataSource                            */
/************************************************************************/
class OGRSDEDataSource : public OGRDataSource
{
    OGRSDELayer        **papoLayers;
    int                 nLayers;

    char               *pszName;
    
    int                 bDSUpdate;
    int                 bDSUseVersionEdits;
    int                 bDSVersionLocked;
    
    SE_CONNECTION       hConnection;
    LONG                nState;
    LONG                nNextState;
    SE_VERSIONINFO      hVersion;

  public:
                        OGRSDEDataSource();
                        ~OGRSDEDataSource();

    int                 Open( const char *, int );
    int                 OpenTable( const char *pszTableName, 
                                   const char *pszFIDColumn, 
                                   const char *pszShapeColumn,
                                   LONG nFIDColumnType );

    const char          *GetName() { return pszName; }
    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );
    
    virtual OGRLayer    *ICreateLayer( const char *,
                                      OGRSpatialReference * = NULL,
                                      OGRwkbGeometryType = wkbUnknown,
                                      char ** = NULL );
    
    virtual OGRErr      DeleteLayer( int );
    
    int                 TestCapability( const char * );

    SE_CONNECTION       GetConnection() { return hConnection; }
    LONG                GetState() {return nState; }
    LONG                GetNextState() {return nNextState; }

    void                IssueSDEError( int, const char * );

    OGRErr              ConvertOSRtoSDESpatRef( OGRSpatialReference *,
                                                SE_COORDREF * );
    int                 IsOpenForUpdate() { return bDSUpdate; } 
    int                 UseVersionEdits() { return bDSUseVersionEdits; }
    
  protected:
    void                EnumerateSpatialTables();
    void                OpenSpatialTable( const char* pszTableName );
    void                CreateLayerFromRegInfo(SE_REGINFO& reginfo);
    void                CleanupLayerCreation(const char* pszLayerName);
    int                 SetVersionState( const char* pszVersionName );
    int                 CreateVersion( const char* pszParentVersion, const char* pszChildVersion);
};

/************************************************************************/
/*                             OGRSDEDriver                              */
/************************************************************************/

class OGRSDEDriver : public OGRSFDriver
{
  public:
                ~OGRSDEDriver();

    const char *GetName();
    OGRDataSource *Open( const char *, int );

    int                 TestCapability( const char * );
    virtual OGRDataSource *CreateDataSource( const char *pszName,
                                             char ** = NULL);
};


#endif /* ndef _OGR_PG_H_INCLUDED */


