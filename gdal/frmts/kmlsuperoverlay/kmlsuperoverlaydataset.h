/******************************************************************************
 * $Id$
 *
 * Project:  KmlSuperOverlay
 * Purpose:  Implements write support for KML superoverlay - KMZ.
 * Author:   Harsh Govind, harsh.govind@spadac.com
 *
 ******************************************************************************
 * Copyright (c) 2010, SPADAC Inc. <harsh.govind@spadac.com>
 * Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef KMLSUPEROVERLAYDATASET_H_INCLUDED
#define KMLSUPEROVERLAYDATASET_H_INCLUDED

#include <map>

#include "cpl_minixml.h"
#include "gdal_pam.h"
#include "gdal_priv.h"

/************************************************************************/
/*                    KmlSuperOverlayReadDataset                        */
/************************************************************************/
class KmlSuperOverlayRasterBand;
class KmlSuperOverlayReadDataset;

class LinkedDataset;
class LinkedDataset
{
    public:
        KmlSuperOverlayReadDataset* poDS;
        LinkedDataset* psPrev;
        LinkedDataset* psNext;
        CPLString      osSubFilename;
};

class KmlSuperOverlayReadDataset final: public GDALDataset
{
    friend class        KmlSuperOverlayRasterBand;

    int                 nFactor;
    CPLString           osFilename;
    CPLXMLNode         *psRoot;
    CPLXMLNode         *psDocument;
    GDALDataset        *poDSIcon;
    double              adfGeoTransform[6];

    int                 nOverviewCount;
    KmlSuperOverlayReadDataset** papoOverviewDS;
    int                 bIsOvr;

    KmlSuperOverlayReadDataset* poParent;

    std::map<CPLString, LinkedDataset*> oMapChildren;
    LinkedDataset      *psFirstLink;
    LinkedDataset      *psLastLink;

  protected:
    virtual int         CloseDependentDatasets() override;

  public:
                  KmlSuperOverlayReadDataset();
    virtual      ~KmlSuperOverlayReadDataset();

    static int          Identify(GDALOpenInfo *);
    static GDALDataset *Open(const char* pszFilename, KmlSuperOverlayReadDataset* poParent = nullptr, int nRec = 0);
    static GDALDataset *Open(GDALOpenInfo *);
 
    static const int KMLSO_ContainsOpaquePixels = 0x1;
    static const int KMLSO_ContainsTransparentPixels = 0x2;
    static const int KMLSO_ContainsPartiallyTransparentPixels = 0x4;

    static int DetectTransparency( int rxsize, int rysize, int rx, int ry, int dxsize, int dysize, GDALDataset* poSrcDs );

    virtual CPLErr GetGeoTransform( double * ) override;
    virtual const char *_GetProjectionRef() override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }

    virtual CPLErr IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               int nBandCount, int *panBandMap,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GSpacing nBandSpace,
                               GDALRasterIOExtraArg* psExtraArg) override;
};

/************************************************************************/
/*                     KmlSuperOverlayRasterBand                        */
/************************************************************************/

class KmlSuperOverlayRasterBand final: public GDALRasterBand
{
    public:
                    KmlSuperOverlayRasterBand( KmlSuperOverlayReadDataset* poDS,
                                               int nBand );
  protected:

    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg) override;
    virtual GDALColorInterp GetColorInterpretation() override;

    virtual int GetOverviewCount() override;
    virtual GDALRasterBand *GetOverview(int) override;
};

#endif /* ndef KMLSUPEROVERLAYDATASET_H_INCLUDED */
