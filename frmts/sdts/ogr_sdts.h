/******************************************************************************
 * $Id$
 *
 * Project:  STS Translator
 * Purpose:  Definition of classes finding SDTS support into OGRDriver
 *           framework.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Frank Warmerdam
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
 * Revision 1.1  1999/09/22 03:05:40  warmerda
 * New
 *
 */

#ifndef _OGR_SDTS_H_INCLUDED
#define _OGR_SDTS_H_INLLUDED

#include "sdts_al.h"
#include "ogrsf_frmts.h"

/************************************************************************/
/*                             OGRSDTSLayer                             */
/************************************************************************/

class OGRSDTSLayer : public OGRLayer
{
    OGRFeatureDefn     *poFeatureDefn;
    OGRGeometry	       *poFilterGeom;

    SDTSTransfer       *poTransfer;
    int			iLayer;
    SDTSIndexedReader  *poReader;

    OGRFeature	       *GetNextUnfilteredFeature();
    
  public:
    			OGRSDTSLayer( SDTSTransfer *, int );
    			~OGRSDTSLayer();

    OGRGeometry *	GetSpatialFilter() { return poFilterGeom; }
    void		SetSpatialFilter( OGRGeometry * );

    void		ResetReading();
    OGRFeature *	GetNextFeature();

//    OGRFeature         *GetFeature( long nFeatureId );
    
    OGRFeatureDefn *	GetLayerDefn() { return poFeatureDefn; }

//    int                 GetFeatureCount( int );

    int                 TestCapability( const char * );
};

/************************************************************************/
/*                          OGRSDTSDataSource                           */
/************************************************************************/

class OGRSDTSDataSource : public OGRDataSource
{
    SDTSTransfer	*poTransfer;
    char		*pszName;

    int			nLayers;
    OGRSDTSLayer	**papoLayers;
    
  public:
    			OGRSDTSDataSource();
    			~OGRSDTSDataSource();

    int			Open( const char * pszFilename, int bTestOpen );
    
    const char	        *GetName() { return pszName; }
    int			GetLayerCount() { return nLayers; }
    OGRLayer		*GetLayer( int );
};

/************************************************************************/
/*                            OGRSDTSDriver                             */
/************************************************************************/

class OGRSDTSDriver : public OGRSFDriver
{
  public:
    		~OGRSDTSDriver();
                
    const char *GetName();
    OGRDataSource *Open( const char *, int );
};


#endif /* ndef _OGR_SDTS_H_INCLUDED */
