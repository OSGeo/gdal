/******************************************************************************
 * $Id$
 *
 * Project:  GML Reader
 * Purpose:  Declarations for OGR wrapper classes for GML, and GML<->OGR
 *           translation of geometry.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.10  2005/02/22 12:56:28  fwarmerdam
 * use OGRLayer base spatial filter support
 *
 * Revision 1.9  2003/05/21 03:48:35  warmerda
 * Expand tabs
 *
 * Revision 1.8  2003/04/03 16:23:49  warmerda
 * Added support for XSISCHEMA creation option which may be INTERNAL, EXTERNAL
 * or OFF.  EXTERNAL (write an associated .xsd file) is the default.
 *
 * Revision 1.7  2003/03/07 14:53:21  warmerda
 * implement preliminary BXFS write support
 *
 * Revision 1.6  2003/03/06 20:30:28  warmerda
 * use GML/OGR geometry translations from ogr_geometry.h now
 *
 * Revision 1.5  2003/01/17 20:39:29  warmerda
 * added bounding rectangle support
 *
 * Revision 1.4  2002/03/07 22:39:01  warmerda
 * include ogr_gml_geom.h
 *
 * Revision 1.3  2002/03/06 20:08:24  warmerda
 * add accelerated GetFeatureCount and GetExtent
 *
 * Revision 1.2  2002/01/25 20:37:23  warmerda
 * added driver and related classes
 *
 * Revision 1.1  2002/01/24 17:39:22  warmerda
 * New
 *
 *
 */

#ifndef _OGR_GML_H_INCLUDED
#define _OGR_GML_H_INCLUDED

#include "ogrsf_frmts.h"
#include "gmlreader.h"

class OGRGMLDataSource;

/************************************************************************/
/*                            OGRGMLLayer                               */
/************************************************************************/

class OGRGMLLayer : public OGRLayer
{
    OGRSpatialReference *poSRS;
    OGRFeatureDefn     *poFeatureDefn;

    int                 iNextGMLId;
    int                 nTotalGMLCount;

    int                 bWriter;

    OGRGMLDataSource    *poDS;

    GMLFeatureClass     *poFClass;

  public:
                        OGRGMLLayer( const char * pszName, 
                                     OGRSpatialReference *poSRS, 
                                     int bWriter,
                                     OGRwkbGeometryType eType,
                                     OGRGMLDataSource *poDS );

                        ~OGRGMLLayer();

    void                ResetReading();
    OGRFeature *        GetNextFeature();

    int                 GetFeatureCount( int bForce = TRUE );
    OGRErr              GetExtent(OGREnvelope *psExtent, int bForce = TRUE);

    OGRErr              CreateFeature( OGRFeature *poFeature );
    
    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );

    virtual OGRSpatialReference *GetSpatialRef();
    
    int                 TestCapability( const char * );
};

/************************************************************************/
/*                           OGRGMLDataSource                           */
/************************************************************************/

class OGRGMLDataSource : public OGRDataSource
{
    OGRGMLLayer     **papoLayers;
    int                 nLayers;
    
    char                *pszName;
    
    OGRGMLLayer         *TranslateGMLSchema( GMLFeatureClass * );

    char               **papszCreateOptions;

    // output related parameters 
    FILE                *fpOutput;
    OGREnvelope         sBoundingRect;
    int                 nBoundedByLocation;
    
    int                 nSchemaInsertLocation;

    // input related parameters.
    IGMLReader          *poReader;

    void                InsertHeader();

  public:
                        OGRGMLDataSource();
                        ~OGRGMLDataSource();

    int                 Open( const char *, int bTestOpen );
    int                 Create( const char *pszFile, char **papszOptions );

    const char          *GetName() { return pszName; }
    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );

    virtual OGRLayer    *CreateLayer( const char *, 
                                      OGRSpatialReference * = NULL,
                                      OGRwkbGeometryType = wkbUnknown,
                                      char ** = NULL );

    int                 TestCapability( const char * );

    FILE                *GetOutputFP() { return fpOutput; }
    IGMLReader          *GetReader() { return poReader; }

    void                GrowExtents( OGREnvelope *psGeomBounds );
};

/************************************************************************/
/*                             OGRGMLDriver                             */
/************************************************************************/

class OGRGMLDriver : public OGRSFDriver
{
  public:
                ~OGRGMLDriver();
                
    const char *GetName();
    OGRDataSource *Open( const char *, int );

    virtual OGRDataSource *CreateDataSource( const char *pszName,
                                             char ** = NULL );
    
    int                 TestCapability( const char * );
};

#endif /* _OGR_GML_H_INCLUDED */
