/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Classes related to format registration, and file opening.
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
 * Revision 1.1  1999/07/05 18:59:00  warmerda
 * new
 *
 */

#ifndef _OGRSF_FRMTS_H_INCLUDED
#define _OGRSF_FRMTS_H_INLLUDED

#include "ogr_feature.h"

/**
 * \file ogrsf_frmts.h
 *
 * Classes related to registration of format support, and opening datasets.
 */

/************************************************************************/
/*                               OGRLayer                               */
/************************************************************************/

class OGRLayer
{
  public:
    virtual 	~OGRLayer() {}

    virtual OGRGeometry *GetSpatialFilter() = 0;
    virtual void	SetSpatialFilter( OGRGeometry * ) = 0;

    virtual void	ResetReading() = 0;
    virtual OGRFeature *GetNextFeature( long * pnFeatureId = NULL ) = 0;

    virtual OGRFeatureDefn *GetLayerDefn() = 0;

    virtual OGRSpatialReference *GetSpatialRef() { return NULL; }
};

/************************************************************************/
/*                            OGRDataSource                             */
/************************************************************************/

class OGRDataSource
{
  public:
    
    virtual 	~OGRDataSource() {}

    virtual const char  *GetName() = 0;

    virtual int		GetLayerCount() = 0;
    virtual OGRLayer    *GetLayer(int) = 0;
};

/************************************************************************/
/*                             OGRSFDriver                              */
/************************************************************************/

class OGRSFDriver
{
  public:
    virtual 	~OGRSFDriver() {}

    virtual const char	*GetName() = 0;
    virtual OGRDataSource *Open( const char *pszName, int bUpdate ) = 0;
};


/************************************************************************/
/*                         OGRSFDriverRegistrar                         */
/************************************************************************/

class OGRSFDriverRegistrar
{
    int		nDrivers;
    OGRSFDriver **papoDrivers;

                OGRSFDriverRegistrar();

  public:

                ~OGRSFDriverRegistrar();

    static OGRSFDriverRegistrar *GetRegistrar();
    static OGRDataSource *Open( const char *pszName, int bUpdate );
    
    void	RegisterDriver( OGRSFDriver * );

    int		GetDriverCount( void );
    OGRSFDriver *GetDriver( int );

};

/* -------------------------------------------------------------------- */
/*      Various available registration methods.                         */
/* -------------------------------------------------------------------- */
CPL_C_START
void	RegisterOGRShape();
CPL_C_END


#endif /* ndef _OGRSF_FRMTS_H_INCLUDED */
