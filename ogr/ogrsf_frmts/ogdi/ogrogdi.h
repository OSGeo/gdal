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
 * Revision 1.6  2005/02/22 13:08:54  fwarmerdam
 * use OGRLayer base spatial filter support
 *
 * Revision 1.5  2003/05/21 03:58:49  warmerda
 * expand tabs
 *
 * Revision 1.4  2001/06/01 14:40:35  warmerda
 * fixup windows builds
 *
 * Revision 1.3  2001/04/17 21:41:02  warmerda
 * Added use of cln_GetLayerCapabilities() to query list of available layers.
 * Restructured OGROGDIDataSource and OGROGDILayer classes somewhat to
 * avoid passing so much information in the layer creation call.  Added support
 * for preserving text on OGDI text features.
 *
 * Revision 1.2  2000/08/30 01:36:56  danmo
 * Added GetSpatialRef() support
 *
 * Revision 1.1  2000/08/24 04:16:19  danmo
 * Initial revision
 *
 */

#ifndef _OGDOGDI_H_INCLUDED
#define _OGDOGDI_H_INLLUDED

#include <math.h>
extern "C" {
#include "ecs.h"
}
#include "ogrsf_frmts.h"


/************************************************************************/
/*                             OGROGDILayer                             */
/************************************************************************/
class OGROGDIDataSource;

class OGROGDILayer : public OGRLayer
{
    OGROGDIDataSource  *m_poODS;
    int                 m_nClientID;
    char               *m_pszOGDILayerName;
    ecs_Family          m_eFamily;

    OGRFeatureDefn     *m_poFeatureDefn;
    OGRSpatialReference *m_poSpatialRef;
    ecs_Region          m_sFilterBounds;

    int                 m_iNextShapeId;
    int                 m_nTotalShapeCount;

  public:
                        OGROGDILayer(OGROGDIDataSource *, const char *, 
                                     ecs_Family);
                        ~OGROGDILayer();

    virtual void        SetSpatialFilter( OGRGeometry * );

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
    
    int                 m_nClientID;

    ecs_Region          m_sGlobalBounds;
    OGRSpatialReference *m_poSpatialRef;

    OGROGDILayer        *m_poCurrentLayer;

    char                *m_pszFullName;

    void                IAddLayer( const char *pszLayerName, 
                                   ecs_Family eFamily );

  public:
                        OGROGDIDataSource();
                        ~OGROGDIDataSource();

    int                 Open( const char *, int bTestOpen );

    const char          *GetName() { return m_pszFullName; }
    int                 GetLayerCount() { return m_nLayers; }
    OGRLayer            *GetLayer( int );

    int                 TestCapability( const char * );

    ecs_Region         *GetGlobalBounds() { return &m_sGlobalBounds; }
    OGRSpatialReference*GetSpatialRef() { return m_poSpatialRef; }
    int                 GetClientID() { return m_nClientID; }
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
