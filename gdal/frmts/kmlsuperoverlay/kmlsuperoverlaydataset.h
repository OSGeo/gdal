/******************************************************************************
 * $Id: kmlsuperoverlaydataset.h
 *
 * Project:  KmlSuperOverlay
 * Purpose:  Implements write support for KML superoverlay - KMZ.
 * Author:   Harsh Govind, harsh.govind@spadac.com
 *
 ******************************************************************************
 * Copyright (c) 2010, SPADAC Inc. <harsh.govind@spadac.com>
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "gdal_pam.h"
#include "gdal_priv.h"
#include "cpl_minixml.h"
#include <map>

CPL_C_START
void CPL_DLL GDALRegister_KMLSUPEROVERLAY();
CPL_C_END

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

class KmlSuperOverlayReadDataset : public GDALDataset
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
    virtual int         CloseDependentDatasets();

  public:
                  KmlSuperOverlayReadDataset();
    virtual      ~KmlSuperOverlayReadDataset();

    static int          Identify(GDALOpenInfo *);
    static GDALDataset *Open(const char* pszFilename, KmlSuperOverlayReadDataset* poParent = NULL, int nRec = 0);
    static GDALDataset *Open(GDALOpenInfo *);

    virtual CPLErr GetGeoTransform( double * );
    virtual const char *GetProjectionRef();

    virtual CPLErr IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               int nBandCount, int *panBandMap,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GSpacing nBandSpace,
                               GDALRasterIOExtraArg* psExtraArg);
};

/************************************************************************/
/*                     KmlSuperOverlayRasterBand                        */
/************************************************************************/

class KmlSuperOverlayRasterBand: public GDALRasterBand
{
    public:
                    KmlSuperOverlayRasterBand(KmlSuperOverlayReadDataset* poDS, int nBand);
  protected:

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg);
    virtual GDALColorInterp GetColorInterpretation();

    virtual int GetOverviewCount();
    virtual GDALRasterBand *GetOverview(int);
};

#endif /* ndef KMLSUPEROVERLAYDATASET_H_INCLUDED */
