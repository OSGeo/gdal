/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions within the Shapefile driver to implement
 *           integration with OGR.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
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
 * Revision 1.3  1999/07/26 13:59:25  warmerda
 * added feature writing api
 *
 * Revision 1.2  1999/07/08 20:05:45  warmerda
 * added GetFeatureCount()
 *
 * Revision 1.1  1999/07/05 18:58:07  warmerda
 * New
 *
 */

#ifndef _OGRSHAPE_H_INCLUDED
#define _OGRSHAPE_H_INLLUDED

#include "shapefil.h"
#include "ogrsf_frmts.h"

/* ==================================================================== */
/*      Functions from Shape2ogr.cpp.                                   */
/* ==================================================================== */
OGRFeature *SHPReadOGRFeature( SHPHandle hSHP, DBFHandle hDBF,
                               OGRFeatureDefn * poDefn, int iShape );
OGRGeometry *SHPReadOGRObject( SHPHandle hSHP, int iShape );
OGRFeatureDefn *SHPReadOGRFeatureDefn( SHPHandle hSHP, DBFHandle hDBF );
OGRErr SHPWriteOGRFeature( SHPHandle hSHP, DBFHandle hDBF,
                           OGRFeatureDefn *poFeatureDefn,
                           OGRFeature *poFeature, long * pnFeatureId );

/************************************************************************/
/*                            OGRShapeLayer                             */
/************************************************************************/

class OGRShapeLayer : public OGRLayer
{
    OGRFeatureDefn     *poFeatureDefn;
    OGRGeometry		*poFilterGeom;
    int			iNextShapeId;
    int			nTotalShapeCount;

    SHPHandle		hSHP;
    DBFHandle		hDBF;
    
  public:
    			OGRShapeLayer( SHPHandle hSHP, DBFHandle hDBF );
    			~OGRShapeLayer();

    OGRGeometry *	GetSpatialFilter() { return poFilterGeom; }
    void		SetSpatialFilter( OGRGeometry * );

    void		ResetReading();
    OGRFeature *	GetNextFeature( long * );

    OGRFeature         *GetFeature( long nFeatureId );
    OGRErr              SetFeature( OGRFeature *poFeature, long nFeatureId );
    OGRErr              CreateFeature( OGRFeature *poFeature,
                                       long * pnFeatureId );
    
    OGRFeatureDefn *	GetLayerDefn() { return poFeatureDefn; }

    int                 GetFeatureCount( int );

    int                 TestCapability( const char * );
};

/************************************************************************/
/*                          OGRShapeDataSource                          */
/************************************************************************/

class OGRShapeDataSource : public OGRDataSource
{
    OGRShapeLayer	*poLayer;
    char		*pszName;
    
  public:
    			OGRShapeDataSource( const char * pszName,
                                            OGRShapeLayer * poLayerIn );
    			~OGRShapeDataSource();

    const char	        *GetName() { return pszName; }
    int			GetLayerCount() { return 1; }
    OGRLayer		*GetLayer( int ) { return poLayer; }
};

/************************************************************************/
/*                            OGRShapeDriver                            */
/************************************************************************/

class OGRShapeDriver : public OGRSFDriver
{
  public:
    		~OGRShapeDriver();
                
    const char *GetName();
    OGRDataSource *Open( const char *, int );
};


#endif /* ndef _OGRSHAPE_H_INCLUDED */
