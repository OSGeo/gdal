/******************************************************************************
 * $Id$
 *
 * Project:  WMS Client Driver
 * Purpose:  Implementation of Dataset and RasterBand classes for WMS
 *           and other similar services.
 * Author:   Adam Nowacki, nowak@xpam.de
 *
 ******************************************************************************
 * Copyright (c) 2007, Adam Nowacki
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef WMSDRIVER_H_INCLUDED
#define WMSDRIVER_H_INCLUDED

#include <math.h>
#include <vector>
#include <list>
#include <algorithm>
#include <curl/curl.h>

#include "cpl_conv.h"
#include "cpl_http.h"
#include "cpl_multiproc.h"
#include "gdal_pam.h"
#include "ogr_spatialref.h"
#include "gdalwarper.h"
#include "gdal_alg.h"

#include "md5.h"
#include "gdalhttp.h"

class GDALWMSDataset;
class GDALWMSRasterBand;

/* -------------------------------------------------------------------- */
/*      Helper functions.                                               */
/* -------------------------------------------------------------------- */
CPLString MD5String(const char *s);
CPLString ProjToWKT(const CPLString &proj);
void URLAppend(CPLString *url, const char *s);
void URLAppendF(CPLString *url, const char *s, ...) CPL_PRINT_FUNC_FORMAT (2, 3);
void URLAppend(CPLString *url, const CPLString &s);
CPLString BufferToVSIFile(GByte *buffer, size_t size);
CPLErr MakeDirs(const char *path);


int StrToBool(const char *p);
int URLSearchAndReplace (CPLString *base, const char *search, const char *fmt, ...) CPL_PRINT_FUNC_FORMAT (3, 4);
/* Convert a.b.c.d to a * 0x1000000 + b * 0x10000 + c * 0x100 + d */
int VersionStringToInt(const char *version);


class GDALWMSImageRequestInfo {
public:
    double m_x0, m_y0;
    double m_x1, m_y1;
    int m_sx, m_sy;
};

class GDALWMSDataWindow {
public:
    double m_x0, m_y0;
    double m_x1, m_y1;
    int m_sx, m_sy;
    int m_tx, m_ty, m_tlevel;
    enum { BOTTOM = -1, DEFAULT = 0, TOP = 1 } m_y_origin;

    GDALWMSDataWindow() : m_x0(-180), m_y0(90), m_x1(180), m_y1(-90),
                          m_sx(-1), m_sy(-1), m_tx(0), m_ty(0),
                          m_tlevel(-1), m_y_origin(DEFAULT) {}
};

class GDALWMSTiledImageRequestInfo {
public:
    int m_x, m_y;
    int m_level;
};

/************************************************************************/
/*                         Mini Driver Related                          */
/************************************************************************/

class GDALWMSRasterIOHint {
public:
    int m_x0, m_y0;
    int m_sx, m_sy;
    int m_overview;
    bool m_valid;
};

class GDALWMSMiniDriverCapabilities {
public:
/* Version N capabilities require all version N and earlier variables to be set to correct values */
    int m_capabilities_version;

/* Version 1 capabilities */
    int m_has_image_request;            // 1 if ImageRequest method is implemented
    int m_has_tiled_image_requeset;     // 1 if TiledImageRequest method is implemented
    int m_has_arb_overviews;            // 1 if ImageRequest method supports arbitrary overviews / resolutions
    int m_max_overview_count;               // Maximum number of overviews supported if known, -1 otherwise
};

/* All data returned by mini-driver as pointer should remain valid for mini-driver lifetime
   and should be freed by mini-driver destructor unless otherwise specified. */
class GDALWMSMiniDriver {
friend class GDALWMSDataset;
public:
    GDALWMSMiniDriver();
    virtual ~GDALWMSMiniDriver();

public:
/* Read mini-driver specific configuration. */
    virtual CPLErr Initialize(CPLXMLNode *config);

public:
    virtual void GetCapabilities(GDALWMSMiniDriverCapabilities *caps);
    virtual void ImageRequest(CPLString *url, const GDALWMSImageRequestInfo &iri);
    virtual void TiledImageRequest(CPLString *url, const GDALWMSImageRequestInfo &iri, const GDALWMSTiledImageRequestInfo &tiri);
    virtual void GetTiledImageInfo(CPLString *url,
                                              const GDALWMSImageRequestInfo &iri,
                                              const GDALWMSTiledImageRequestInfo &tiri,
                                              int nXInBlock,
                                              int nYInBlock);

/* Return data projection in WKT format, NULL or empty string if unknown */
    virtual const char *GetProjectionInWKT();

protected:
    GDALWMSDataset *m_parent_dataset;
};

class GDALWMSMiniDriverFactory {
public:
    GDALWMSMiniDriverFactory();
    virtual ~GDALWMSMiniDriverFactory();

public:
    virtual GDALWMSMiniDriver* New() = 0;
    virtual void Delete(GDALWMSMiniDriver *instance) = 0;

public:
    const CPLString &GetName() {
        return m_name;
    }

protected:
    CPLString m_name;
};

class GDALWMSMiniDriverManager {
public:
    GDALWMSMiniDriverManager();
    ~GDALWMSMiniDriverManager();

public:
    void Register(GDALWMSMiniDriverFactory *mdf);
    GDALWMSMiniDriverFactory *Find(const CPLString &name);

protected:
    std::list<GDALWMSMiniDriverFactory *> m_mdfs;
};

#define H_GDALWMSMiniDriverFactory(name) \
class GDALWMSMiniDriverFactory_##name : public GDALWMSMiniDriverFactory { \
public: \
    GDALWMSMiniDriverFactory_##name(); \
    virtual ~GDALWMSMiniDriverFactory_##name(); \
    \
public: \
    virtual GDALWMSMiniDriver* New(); \
    virtual void Delete(GDALWMSMiniDriver *instance); \
};

#define CPP_GDALWMSMiniDriverFactory(name) \
    GDALWMSMiniDriverFactory_##name::GDALWMSMiniDriverFactory_##name() { \
    m_name = #name;\
} \
    \
    GDALWMSMiniDriverFactory_##name::~GDALWMSMiniDriverFactory_##name() { \
} \
    \
    GDALWMSMiniDriver* GDALWMSMiniDriverFactory_##name::New() { \
    return new GDALWMSMiniDriver_##name(); \
} \
    \
    void GDALWMSMiniDriverFactory_##name::Delete(GDALWMSMiniDriver *instance) { \
    delete instance; \
}

/************************************************************************/
/*                            GDALWMSCache                              */
/************************************************************************/

class GDALWMSCache {
public:
    GDALWMSCache();
    ~GDALWMSCache();

public:
    CPLErr Initialize(CPLXMLNode *config);
    CPLErr Write(const char *key, const CPLString &file_name);
    CPLErr Read(const char *key, CPLString *file_name);

protected:
    CPLString KeyToCacheFile(const char *key);

protected:
    CPLString m_cache_path;
    CPLString m_postfix;
    int m_cache_depth;
};

/************************************************************************/
/*                            GDALWMSDataset                            */
/************************************************************************/

class GDALWMSDataset : public GDALPamDataset {
    friend class GDALWMSRasterBand;

public:
    GDALWMSDataset();
    virtual ~GDALWMSDataset();

    virtual const char *GetProjectionRef();
    virtual CPLErr SetProjection(const char *proj);
    virtual CPLErr GetGeoTransform(double *gt);
    virtual CPLErr SetGeoTransform(double *gt);
    virtual CPLErr AdviseRead(int x0, int y0, int sx, int sy, int bsx, int bsy, GDALDataType bdt, int band_count, int *band_map, char **options);
    
    virtual char      **GetMetadataDomainList();
    virtual const char *GetMetadataItem( const char * pszName,
                                         const char * pszDomain = "" );

    void SetColorTable(GDALColorTable *pct) { m_poColorTable=pct; }

    void mSetBand(int i, GDALRasterBand *band) { SetBand(i,band); };
    GDALWMSRasterBand *mGetBand(int i) { return reinterpret_cast<GDALWMSRasterBand *>(GetRasterBand(i)); };

    const GDALWMSDataWindow *WMSGetDataWindow() const {
        return &m_data_window;
    }

    void WMSSetBlockSize(int x, int y) {
        m_block_size_x=x;
        m_block_size_y=y;
    }

    void WMSSetRasterSize(int x, int y) {
        nRasterXSize=x;
        nRasterYSize=y;
    }

    void WMSSetBandsCount(int count) {
        nBands=count;
    }

    void WMSSetClamp(bool flag=true) {
        m_clamp_requests=flag;
    }

    void WMSSetDataType(GDALDataType type) {
        m_data_type=type;
    }

    void WMSSetDataWindow(GDALWMSDataWindow &window) {
        m_data_window=window;
    }

    void WMSSetDefaultBlockSize(int x, int y) {
        m_default_block_size_x=x;
        m_default_block_size_y=y;
    }

    void WMSSetDefaultDataWindowCoordinates(double x0, double y0, double x1, double y1) {
        m_default_data_window.m_x0 = x0;
        m_default_data_window.m_y0 = y0;
        m_default_data_window.m_x1 = x1;
        m_default_data_window.m_y1 = y1;
    }

    void WMSSetDefaultTileCount(int tilecountx, int tilecounty) {
        m_default_tile_count_x = tilecountx;
        m_default_tile_count_y = tilecounty;
    }

    void WMSSetDefaultTileLevel(int tlevel) {
        m_default_data_window.m_tlevel = tlevel;
    }

    void WMSSetDefaultOverviewCount(int overview_count) {
        m_default_overview_count = overview_count;
    }

    void WMSSetNeedsDataWindow(int flag) {
        m_bNeedsDataWindow = flag;
    }
    
    static void list2vec(std::vector<double> &v,const char *pszList) {
        if ((pszList==NULL)||(pszList[0]==0)) return;
        char **papszTokens=CSLTokenizeString2(pszList," \t\n\r",
                                              CSLT_STRIPLEADSPACES|CSLT_STRIPENDSPACES);
        v.clear();
        for (int i=0;i<CSLCount(papszTokens);i++)
            v.push_back(CPLStrtod(papszTokens[i],NULL));
        CSLDestroy(papszTokens);
    }

    void WMSSetNoDataValue(const char * pszNoData) {
        list2vec(vNoData,pszNoData);
    }

    void WMSSetMinValue(const char * pszMin) {
        list2vec(vMin,pszMin);
    }

    void WMSSetMaxValue(const char * pszMax) {
        list2vec(vMax,pszMax);
    }

    static GDALDataset* Open(GDALOpenInfo *poOpenInfo);
    static int Identify(GDALOpenInfo *poOpenInfo);
    static GDALDataset *CreateCopy( const char * pszFilename,
                                    GDALDataset *poSrcDS,
                                    int bStrict, char ** papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void * pProgressData );

protected:
    virtual CPLErr IRasterIO(GDALRWFlag rw, int x0, int y0, int sx, int sy, void *buffer,
                             int bsx, int bsy, GDALDataType bdt,
                             int band_count, int *band_map,
                             GSpacing nPixelSpace, GSpacing nLineSpace,
                             GSpacing nBandSpace,
                             GDALRasterIOExtraArg* psExtraArg);
    CPLErr Initialize(CPLXMLNode *config);

    GDALWMSDataWindow m_data_window;
    GDALWMSMiniDriver *m_mini_driver;
    GDALWMSMiniDriverCapabilities m_mini_driver_caps;
    GDALWMSCache *m_cache;
    CPLString m_projection;
    GDALColorTable *m_poColorTable;
    std::vector<double> vNoData, vMin, vMax;
    GDALDataType m_data_type;
    int m_block_size_x, m_block_size_y;
    GDALWMSRasterIOHint m_hint;
    int m_use_advise_read;
    int m_verify_advise_read;
    int m_offline_mode;
    int m_http_max_conn;
    int m_http_timeout;
    int m_clamp_requests;
    int m_unsafeSsl;
    std::vector<int> m_http_zeroblock_codes;
    int m_zeroblock_on_serverexceptions;
    CPLString m_osUserAgent;
    CPLString m_osReferer;
    CPLString m_osUserPwd;

    GDALWMSDataWindow m_default_data_window;
    int m_default_block_size_x, m_default_block_size_y;
    int m_default_tile_count_x, m_default_tile_count_y;
    int m_default_overview_count;

    int m_bNeedsDataWindow;

    CPLString m_osXML;
};

/************************************************************************/
/*                            GDALWMSRasterBand                         */
/************************************************************************/

class GDALWMSRasterBand : public GDALPamRasterBand {
    friend class GDALWMSDataset;

    char**  BuildHTTPRequestOpts();
    void    ComputeRequestInfo( GDALWMSImageRequestInfo &iri,
                                GDALWMSTiledImageRequestInfo &tiri,
                                int x, int y);

    CPLString osMetadataItem;
    CPLString osMetadataItemURL;

public:
    GDALWMSRasterBand(GDALWMSDataset *parent_dataset, int band, double scale);
    virtual ~GDALWMSRasterBand();
    void AddOverview(double scale);
    virtual double GetNoDataValue( int * );
    virtual double GetMinimum( int * );
    virtual double GetMaximum( int * );
    virtual GDALColorTable *GetColorTable();
    virtual CPLErr AdviseRead(int x0, int y0, int sx, int sy, int bsx, int bsy, GDALDataType bdt, char **options);

    virtual GDALColorInterp GetColorInterpretation();
    virtual CPLErr SetColorInterpretation( GDALColorInterp );
    virtual CPLErr IReadBlock(int x, int y, void *buffer);
    virtual CPLErr IRasterIO(GDALRWFlag rw, int x0, int y0, int sx, int sy, void *buffer, int bsx, int bsy, GDALDataType bdt,
                             GSpacing nPixelSpace, GSpacing nLineSpace,
                             GDALRasterIOExtraArg* psExtraArg);
    virtual int HasArbitraryOverviews();
    virtual int GetOverviewCount();
    virtual GDALRasterBand *GetOverview(int n);

    virtual char      **GetMetadataDomainList();
    virtual const char *GetMetadataItem( const char * pszName,
                                         const char * pszDomain = "" );

protected:
    CPLErr ReadBlocks(int x, int y, void *buffer, int bx0, int by0, int bx1, int by1, int advise_read);
    bool IsBlockInCache(int x, int y);
    void AskMiniDriverForBlock(CPLString *url, int x, int y);
    CPLErr ReadBlockFromFile(int x, int y, const char *file_name, int to_buffer_band, void *buffer, int advise_read);
    CPLErr ZeroBlock(int x, int y, int to_buffer_band, void *buffer);
    CPLErr ReportWMSException(const char *file_name);

protected:
    GDALWMSDataset *m_parent_dataset;
    double m_scale;
    std::vector<GDALWMSRasterBand *> m_overviews;
    int m_overview;
    GDALColorInterp m_color_interp;
};

GDALWMSMiniDriverManager *GetGDALWMSMiniDriverManager();
void DestroyWMSMiniDriverManager(void);

#endif /* notdef WMSDRIVER_H_INCLUDED */
