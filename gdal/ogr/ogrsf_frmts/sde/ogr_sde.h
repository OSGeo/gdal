/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for OGR SDE driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
#define _OGR_SDE_H_INLLUDED

#include "ogrsf_frmts.h"

#include <sdetype.h> /* ESRI SDE Client Includes */
#include <sdeerno.h>
#include <vector>
#include "cpl_string.h"

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

    CPLString           osAttributeFilter;

    int                 bQueryInstalled;
    int                 bQueryActive;

    SE_STREAM           hStream;
    
    int                 bHaveLayerInfo;
    SE_LAYERINFO        hLayerInfo;
    SE_COORDREF         hCoordRef;

    OGRSDEDataSource    *poDS;

    int                 iFIDColumn;
    int                 nNextFID;

    int                 iShapeColumn;
    CPLString           osShapeColumnName;

    char              **papszAllColumns;
    std::vector<int>    anFieldMap;     // SDE index of OGR field.
    std::vector<int>    anFieldTypeMap; // SDE type

    int                 InstallQuery( int );
    OGRFeature         *TranslateSDERecord();
    OGRGeometry        *TranslateSDEGeometry( SE_SHAPE );
    int                 NeedLayerInfo();

    // This process can be fairly expensive depending on the configuration
    // of the layer in SDE. Enable this feature with OGR_SDE_GETLAYERTYPE.
    OGRwkbGeometryType  DiscoverLayerType();

  public:
                        OGRSDELayer( OGRSDEDataSource * );
    virtual             ~OGRSDELayer();

    int                 Initialize( const char *, const char *, const char * );

    virtual void        ResetReading();

    virtual OGRFeature *GetNextFeature();
    virtual OGRFeature *GetFeature( long nFeatureId );
    virtual OGRErr      GetExtent( OGREnvelope *psExtent, int bForce );
    virtual int         GetFeatureCount( int bForce );

    virtual OGRErr      SetAttributeFilter( const char *pszQuery );

    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    virtual OGRSpatialReference *GetSpatialRef();

    virtual int         TestCapability( const char * );
};

/************************************************************************/
/*                           OGRSDEDataSource                            */
/************************************************************************/
class OGRSDEDataSource : public OGRDataSource
{
    OGRSDELayer        **papoLayers;
    int                 nLayers;

    char               *pszName;

    SE_CONNECTION       hConnection;

  public:
                        OGRSDEDataSource();
                        ~OGRSDEDataSource();

    int                 Open( const char * );
    int                 OpenTable( const char *pszTableName, 
                                   const char *pszFIDColumn, 
                                   const char *pszShapeColumn );

    const char          *GetName() { return pszName; }
    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );

    int                 TestCapability( const char * );

    SE_CONNECTION       GetConnection() { return hConnection; }

    void                IssueSDEError( int, const char * );

  protected:
    void                EnumerateSpatialTables();
    void                OpenSpatialTable( const char* pszTableName );
    void                CreateLayerFromRegInfo(SE_REGINFO& reginfo);
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
};


#endif /* ndef _OGR_PG_H_INCLUDED */


