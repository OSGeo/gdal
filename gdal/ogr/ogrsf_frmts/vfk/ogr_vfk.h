/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for OGR/VFK driver.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2009-2010, Martin Landa <landa.martin gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ****************************************************************************/

#ifndef GDAL_OGR_VFK_H_INCLUDED
#define GDAL_OGR_VFK_H_INCLUDED

#include <map>
#include <string>

#include "ogrsf_frmts.h"
#include "vfkreader.h"

class OGRVFKDataSource;

/************************************************************************/
/*                            OGRVFKLayer                               */
/************************************************************************/

class OGRVFKLayer:public OGRLayer
{
private:
    /* spatial reference */
    OGRSpatialReference *poSRS;

    /* feature definition */
    OGRFeatureDefn      *poFeatureDefn;

    /* VFK data block */
    IVFKDataBlock       *poDataBlock;

    /* get next feature */
    int                  m_iNextFeature;

    /* private methods */
    OGRGeometry         *CreateGeometry(IVFKFeature *);
    OGRFeature          *GetFeature(IVFKFeature *);

public:
    OGRVFKLayer(const char *, OGRSpatialReference *,
                OGRwkbGeometryType, OGRVFKDataSource *);
    ~OGRVFKLayer();

    OGRFeature          *GetNextFeature() override;
    OGRFeature          *GetFeature(GIntBig) override;

    OGRFeatureDefn      *GetLayerDefn() override { return poFeatureDefn; }

    void                 ResetReading() override;

    int                  TestCapability(const char *) override;

    GIntBig              GetFeatureCount(int = TRUE) override;
};

/************************************************************************/
/*                           OGRVFKDataSource                           */
/************************************************************************/
class OGRVFKDataSource:public OGRDataSource
{
private:
    /* list of available layers */
    OGRVFKLayer  **papoLayers;
    int            nLayers;

    char *         pszName;

    /* input related parameters */
    IVFKReader    *poReader;

    /* private methods */
    OGRVFKLayer   *CreateLayerFromBlock(const IVFKDataBlock *);

public:
    OGRVFKDataSource();
    ~OGRVFKDataSource();

    int            Open(GDALOpenInfo* poOpenInfo);

    const char    *GetName() override { return pszName; }

    int            GetLayerCount() override { return nLayers; }
    OGRLayer      *GetLayer(int) override;

    int            TestCapability(const char *) override;

    IVFKReader    *GetReader() const { return poReader; }
};

#endif // GDAL_OGR_VFK_H_INCLUDED
