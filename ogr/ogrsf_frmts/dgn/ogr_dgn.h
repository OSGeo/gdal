/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGR Driver for DGN Reader.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam (warmerdam@pobox.com)
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
 * Revision 1.6  2002/10/29 19:45:29  warmerda
 * OGR driver now always builds an index if any features are to be read.  This
 * is primarily done to ensure that color tables appearing late in the file
 * will still affect feature elements occuring before them ... such as with
 * the m_epoche.dgn file.  Also implement fast feature counting and spatial
 * extents support based on the index.
 *
 * Revision 1.5  2002/05/31 19:03:46  warmerda
 * removed old CollectLines code
 *
 * Revision 1.4  2002/03/27 21:36:50  warmerda
 * added implementation of GetFeature()
 *
 * Revision 1.3  2002/02/22 22:18:44  warmerda
 * added support for commplex shapes
 *
 * Revision 1.2  2000/12/28 21:27:11  warmerda
 * updated email address
 *
 * Revision 1.1  2000/11/28 19:03:53  warmerda
 * New
 *
 */

#ifndef _OGR_DGN_H_INCLUDED
#define _OGR_DGN_H_INLLUDED

#include "dgnlib.h"
#include "ogrsf_frmts.h"

/************************************************************************/
/*                            OGRDGNLayer                               */
/************************************************************************/

class OGRDGNLayer : public OGRLayer
{
    OGRFeatureDefn     *poFeatureDefn;

    OGRGeometry		*poFilterGeom;
    int			iNextShapeId;
    int			nTotalShapeCount;

    DGNHandle           hDGN;

    OGRFeature	       *ElementToFeature( DGNElemCore * );
    void                ConsiderBrush( DGNElemCore *, const char *pszPen,
                                       OGRFeature *poFeature );

  public:
                        OGRDGNLayer( const char * pszName, DGNHandle hDGN );
    			~OGRDGNLayer();

    OGRGeometry *	GetSpatialFilter() { return poFilterGeom; }
    void		SetSpatialFilter( OGRGeometry * );

    void		ResetReading();
    OGRFeature *	GetNextFeature();
    OGRFeature *        GetFeature( long nFeatureId );

    virtual int         GetFeatureCount( int bForce = TRUE );
    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE);

    OGRFeatureDefn *	GetLayerDefn() { return poFeatureDefn; }

    int                 TestCapability( const char * );
};

/************************************************************************/
/*                          OGRDGNDataSource                            */
/************************************************************************/

class OGRDGNDataSource : public OGRDataSource
{
    OGRDGNLayer     **papoLayers;
    int			nLayers;
    
    char		*pszName;
    DGNHandle           hDGN;
    
  public:
    			OGRDGNDataSource();
    			~OGRDGNDataSource();

    int			Open( const char *, int bTestOpen );

    const char	        *GetName() { return pszName; }
    int			GetLayerCount() { return nLayers; }
    OGRLayer		*GetLayer( int );

    int                 TestCapability( const char * );
};

/************************************************************************/
/*                            OGRDGNDriver                              */
/************************************************************************/

class OGRDGNDriver : public OGRSFDriver
{
  public:
    		~OGRDGNDriver();
                
    const char *GetName();
    OGRDataSource *Open( const char *, int );

    int                 TestCapability( const char * );
};


#endif /* ndef _OGR_DGN_H_INCLUDED */
