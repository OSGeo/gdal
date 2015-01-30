/******************************************************************************
 * $Id$
 *
 * Project:  XLS Translator
 * Purpose:  Definition of classes for OGR .xls driver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2011-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef _OGR_XLS_H_INCLUDED
#define _OGR_XLS_H_INCLUDED

#include "ogrsf_frmts.h"

/************************************************************************/
/*                             OGRXLSLayer                              */
/************************************************************************/

class OGRXLSDataSource;

class OGRXLSLayer : public OGRLayer
{
    OGRXLSDataSource*  poDS;
    OGRFeatureDefn*    poFeatureDefn;

    char              *pszName;
    int                iSheet;
    int                bFirstLineIsHeaders;
    int                nRows;
    unsigned short     nCols;

    int                nNextFID;

    OGRFeature *       GetNextRawFeature();

    void               DetectHeaderLine(const void* xlshandle);
    void               DetectColumnTypes(const void* xlshandle,
                                         int* paeFieldTypes);

  public:
                        OGRXLSLayer(OGRXLSDataSource* poDSIn,
                                    const char* pszSheetname,
                                    int iSheetIn,
                                    int nRowsIn,
                                    unsigned short nColsIn);
                        ~OGRXLSLayer();


    virtual void                ResetReading();
    virtual OGRFeature *        GetNextFeature();

    virtual OGRFeatureDefn *    GetLayerDefn();
    virtual GIntBig             GetFeatureCount( int bForce = TRUE );

    virtual const char         *GetName() { return pszName; }
    virtual OGRwkbGeometryType  GetGeomType() { return wkbNone; }

    virtual int                 TestCapability( const char * );

    virtual OGRSpatialReference *GetSpatialRef() { return NULL; }

};

/************************************************************************/
/*                           OGRXLSDataSource                           */
/************************************************************************/

class OGRXLSDataSource : public OGRDataSource
{
    char*               pszName;

    OGRLayer**          papoLayers;
    int                 nLayers;

    const void*         xlshandle;

  public:
                        OGRXLSDataSource();
                        ~OGRXLSDataSource();

    int                 Open( const char * pszFilename,
                              int bUpdate );

    virtual const char*         GetName() { return pszName; }

    virtual int                 GetLayerCount() { return nLayers; }
    virtual OGRLayer*           GetLayer( int );

    virtual int                 TestCapability( const char * );

    const void                 *GetXLSHandle();
};

/************************************************************************/
/*                             OGRXLSDriver                             */
/************************************************************************/

class OGRXLSDriver : public OGRSFDriver
{
  public:
                ~OGRXLSDriver();

    virtual const char*         GetName();
    virtual OGRDataSource*      Open( const char *, int );
    virtual int                 TestCapability( const char * );
};


#endif /* ndef _OGR_XLS_H_INCLUDED */
