/******************************************************************************
 * $Id$
 *
 * Project:  OGDI Bridge
 * Purpose:  Private definitions within the OGDI driver to implement
 *           integration with OGR.
 * Author:   Daniel Morissette, danmo@videotron.ca
 *           (Based on some code contributed by Frank Warmerdam :)
 *
 ******************************************************************************
 * Copyright (c) 2000,  Daniel Morissette
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
 * Revision 1.2  2000/08/30 01:36:56  danmo
 * Added GetSpatialRef() support
 *
 * Revision 1.1  2000/08/24 04:16:19  danmo
 * Initial revision
 *
 */

#ifndef _OGDOGDI_H_INCLUDED
#define _OGDOGDI_H_INLLUDED

#include "ecs.h"
#include "ogrsf_frmts.h"


/************************************************************************/
/*                             OGROGDILayer                             */
/************************************************************************/

class OGROGDILayer : public OGRLayer
{
    int                 m_nClientID;
    char               *m_pszOGDILayerName;
    ecs_Family          m_eFamily;

    OGRFeatureDefn     *m_poFeatureDefn;
    OGRSpatialReference *m_poSpatialRef;
    ecs_Region          m_sFilterBounds;
    OGRGeometry        *m_poFilterGeom;

    int                 m_iNextShapeId;
    int                 m_nTotalShapeCount;

  public:
                        OGROGDILayer( int nClientID, const char * pszName,
                                      ecs_Family eFamily, ecs_Region *sBounds,
                                      OGRSpatialReference *poSpatialRef);
                        ~OGROGDILayer();

    OGRGeometry *       GetSpatialFilter() { return m_poFilterGeom; }
    void                SetSpatialFilter( OGRGeometry * );

    void                ResetReading();
    OGRFeature *        GetNextFeature();

    OGRFeature         *GetFeature( long nFeatureId );

    OGRFeatureDefn *    GetLayerDefn() { return m_poFeatureDefn; }

    int                 GetFeatureCount( int );

    int                 TestCapability( const char * );

    OGRSpatialReference *GetSpatialRef()  { return m_poSpatialRef; }

  private:
    void                BuildFeatureDefn();
};

/************************************************************************/
/*                          OGROGDIDataSource                           */
/************************************************************************/

class OGROGDIDataSource : public OGRDataSource
{
    OGROGDILayer      **m_papoLayers;
    int                 m_nLayers;
    
    char               *m_pszFullName;
    char               *m_pszURL;
    char               *m_pszOGDILayerName;
    int                 m_nClientID;
    ecs_Family          m_eFamily;

    ecs_Region          m_sGlobalBounds;
    OGRSpatialReference *m_poSpatialRef;

  public:
                        OGROGDIDataSource();
                        ~OGROGDIDataSource();

    int                 Open( const char *, int bTestOpen );

    const char          *GetName() { return m_pszFullName; }
    int                 GetLayerCount() { return m_nLayers; }
    OGRLayer            *GetLayer( int );

    int                 TestCapability( const char * );
};

/************************************************************************/
/*                            OGROGDIDriver                             */
/************************************************************************/

class OGROGDIDriver : public OGRSFDriver
{
  public:
                ~OGROGDIDriver();
                
    const char *GetName();
    OGRDataSource *Open( const char *, int );

    int         TestCapability( const char * );
};


#endif /* _OGDOGDI_H_INCLUDED */
