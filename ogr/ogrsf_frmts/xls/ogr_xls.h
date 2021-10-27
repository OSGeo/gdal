/******************************************************************************
 * $Id$
 *
 * Project:  XLS Translator
 * Purpose:  Definition of classes for OGR .xls driver.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2011-2012, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef OGR_XLS_H_INCLUDED
#define OGR_XLS_H_INCLUDED

#include "ogrsf_frmts.h"

/************************************************************************/
/*                             OGRXLSLayer                              */
/************************************************************************/

class OGRXLSDataSource;

class OGRXLSLayer final: public OGRLayer, public OGRGetNextFeatureThroughRaw<OGRXLSLayer>
{
    OGRXLSDataSource*  poDS;
    OGRFeatureDefn*    poFeatureDefn;

    char              *pszName;
    int                iSheet;
    bool               bFirstLineIsHeaders;
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
                        virtual ~OGRXLSLayer();

    virtual void                ResetReading() override;
    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRXLSLayer)

    virtual OGRFeatureDefn *    GetLayerDefn() override;
    virtual GIntBig             GetFeatureCount( int bForce = TRUE ) override;

    virtual const char         *GetName() override { return pszName; }
    virtual OGRwkbGeometryType  GetGeomType() override { return wkbNone; }

    virtual int                 TestCapability( const char * ) override;

    virtual OGRSpatialReference *GetSpatialRef() override { return nullptr; }
};

/************************************************************************/
/*                           OGRXLSDataSource                           */
/************************************************************************/

class OGRXLSDataSource final: public OGRDataSource
{
    char*               pszName;

    OGRLayer**          papoLayers;
    int                 nLayers;

    const void*         xlshandle;

    CPLString           m_osANSIFilename;
#ifdef WIN32
    CPLString           m_osTempFilename;
#endif
  public:
                        OGRXLSDataSource();
                        virtual ~OGRXLSDataSource();

    int                 Open( const char * pszFilename,
                              int bUpdate );

    virtual const char*         GetName() override { return pszName; }

    virtual int                 GetLayerCount() override { return nLayers; }
    virtual OGRLayer*           GetLayer( int ) override;

    virtual int                 TestCapability( const char * ) override;

    const void                 *GetXLSHandle();
};

/************************************************************************/
/*                             OGRXLSDriver                             */
/************************************************************************/

class OGRXLSDriver final: public OGRSFDriver
{
  public:
                virtual ~OGRXLSDriver();

    virtual const char*         GetName() override;
    virtual OGRDataSource*      Open( const char *, int ) override;
    virtual int                 TestCapability( const char * ) override;
};

#endif /* ndef OGR_XLS_H_INCLUDED */
