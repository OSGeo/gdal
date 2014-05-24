/******************************************************************************
 * $Id$
 *
 * Project:  Arc/Info Generate Translator
 * Purpose:  Definition of classes for OGR .arcgen driver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMARCGENS OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef _OGR_ARCGEN_H_INCLUDED
#define _OGR_ARCGEN_H_INCLUDED

#include "ogrsf_frmts.h"

/************************************************************************/
/*                             OGRARCGENLayer                              */
/************************************************************************/

class OGRARCGENLayer : public OGRLayer
{
    OGRFeatureDefn*    poFeatureDefn;

    VSILFILE*          fp;
    int                bEOF;

    int                nNextFID;

    OGRFeature *       GetNextRawFeature();

  public:
                        OGRARCGENLayer(const char* pszFilename,
                                    VSILFILE* fp, OGRwkbGeometryType eType);
                        ~OGRARCGENLayer();


    virtual void                ResetReading();
    virtual OGRFeature *        GetNextFeature();

    virtual OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    virtual int                 TestCapability( const char * );
};

/************************************************************************/
/*                           OGRARCGENDataSource                           */
/************************************************************************/

class OGRARCGENDataSource : public OGRDataSource
{
    char*               pszName;

    OGRLayer**          papoLayers;
    int                 nLayers;

  public:
                        OGRARCGENDataSource();
                        ~OGRARCGENDataSource();

    int                 Open( const char * pszFilename );

    virtual const char*         GetName() { return pszName; }

    virtual int                 GetLayerCount() { return nLayers; }
    virtual OGRLayer*           GetLayer( int );

    virtual int                 TestCapability( const char * );
};

#endif /* ndef _OGR_ARCGEN_H_INCLUDED */
