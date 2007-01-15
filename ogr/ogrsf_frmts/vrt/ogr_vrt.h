/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for OGR/VRT driver.
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.9  2006/05/15 18:04:32  fwarmerdam
 * added 'secret' useSpatialSubquery attribute on GeometryField
 *
 * Revision 1.8  2006/04/13 16:41:03  fwarmerdam
 * improved success reporting, preliminary srcrect support
 *
 * Revision 1.7  2005/09/05 19:35:10  fwarmerdam
 * Added shape support
 *
 * Revision 1.6  2005/08/16 00:08:52  fwarmerdam
 * Added bSrcLayerFromSQL
 *
 * Revision 1.5  2005/08/02 20:17:26  fwarmerdam
 * pass attribute filter to sublayer
 *
 * Revision 1.4  2005/05/16 20:09:46  fwarmerdam
 * added spatial query on x/y columns
 *
 * Revision 1.3  2005/02/22 12:50:10  fwarmerdam
 * use OGRLayer base spatial filter support
 *
 * Revision 1.2  2003/11/07 21:55:12  warmerda
 * complete fid support, relative dsname, fixes
 *
 * Revision 1.1  2003/11/07 17:50:09  warmerda
 * New
 *
 */

#ifndef _OGR_VRT_H_INCLUDED
#define _OGR_VRT_H_INLLUDED

#include "ogrsf_frmts.h"
#include "cpl_error.h"
#include "cpl_minixml.h"

typedef enum { 
    VGS_None,
    VGS_Direct,
    VGS_PointFromColumns, 
    VGS_WKT, 
    VGS_WKB,
    VGS_Shape
} OGRVRTGeometryStyle;

/************************************************************************/
/*                            OGRVRTLayer                                */
/************************************************************************/

class OGRVRTLayer : public OGRLayer
{
  protected:
    OGRFeatureDefn      *poFeatureDefn;

    OGRDataSource       *poSrcDS;
    OGRLayer            *poSrcLayer;
    int                 bNeedReset;
    int                 bSrcLayerFromSQL;

    // Layer spatial reference system, and srid.
    OGRSpatialReference *poSRS;

    char                *pszAttrFilter;

    int                 bSrcClip;
    OGRGeometry         *poSrcRegion;

    int                 iFIDField; // -1 means pass through. 

    // Geometry interpretation related.
    OGRVRTGeometryStyle eGeometryType;
    
    int                 iGeomField; 

                        // VGS_PointFromColumn
    int                 iGeomXField, iGeomYField, iGeomZField;

    int                 bUseSpatialSubquery;

    // Attribute Mapping
    int                *panSrcField;
    int                *pabDirectCopy;

    OGRFeature         *TranslateFeature( OGRFeature * );
    OGRErr              createFromShapeBin( GByte *, OGRGeometry **, int );

    int                 ResetSourceReading();

  public:
                        OGRVRTLayer();
    virtual             ~OGRVRTLayer();

    virtual int         Initialize( CPLXMLNode *psLTree, 
                                    const char *pszVRTDirectory );

    virtual void        ResetReading();
    virtual OGRFeature *GetNextFeature();

    virtual OGRFeature *GetFeature( long nFeatureId );
    
    virtual OGRFeatureDefn *GetLayerDefn() { return poFeatureDefn; }

    virtual OGRSpatialReference *GetSpatialRef();

    virtual int         GetFeatureCount( int );

    virtual OGRErr      SetAttributeFilter( const char * );

    virtual int         TestCapability( const char * );
};

/************************************************************************/
/*                           OGRVRTDataSource                            */
/************************************************************************/

class OGRVRTDataSource : public OGRDataSource
{
    OGRVRTLayer        **papoLayers;
    int                 nLayers;
    
    char               *pszName;

  public:
                        OGRVRTDataSource();
                        ~OGRVRTDataSource();

    int                 Initialize( CPLXMLNode *psXML, const char *pszName );

    const char          *GetName() { return pszName; }
    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );

    int                 TestCapability( const char * );
};

/************************************************************************/
/*                             OGRVRTDriver                             */
/************************************************************************/

class OGRVRTDriver : public OGRSFDriver
{
  public:
                ~OGRVRTDriver();
                
    const char *GetName();
    OGRDataSource *Open( const char *, int );
    int         TestCapability( const char * );
};


#endif /* ndef _OGR_VRT_H_INCLUDED */


