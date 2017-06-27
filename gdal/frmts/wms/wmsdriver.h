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

#include <cmath>
#include <vector>
#include <set>
#include <algorithm>
#include <map>
#include <utility>
#include <curl/curl.h>

#include "cpl_conv.h"
#include "cpl_http.h"
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

// Decode s from encoding "base64" or "XMLencoded".
// If encoding is "file", s is the file name on input and file content on output
// If encoding is not recognized, does nothing
const char *WMSUtilDecode(CPLString &s, const char *encoding);

// Ensure that the url ends in ? or &
void URLPrepare(CPLString &url);
// void URLAppend(CPLString *url, const char *s);
// void URLAppendF(CPLString *url, const char *s, ...) CPL_PRINT_FUNC_FORMAT (2, 3);
// void URLAppend(CPLString *url, const CPLString &s);
CPLString BufferToVSIFile(GByte *buffer, size_t size);

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
  GDALWMSRasterIOHint() :
      m_x0(0),
      m_y0(0),
      m_sx(0),
      m_sy(0),
      m_overview(0),
      m_valid(false)
  {}
    int m_x0;
    int m_y0;
    int m_sx;
    int m_sy;
    int m_overview;
    bool m_valid;
};

typedef enum
{
    OVERVIEW_ROUNDED,
    OVERVIEW_FLOOR
} GDALWMSOverviewDimComputationMethod;

class WMSMiniDriverCapabilities {
public:
    // Default capabilities, suitable in most cases
    WMSMiniDriverCapabilities() :
        m_has_getinfo(0),
        m_has_geotransform(1),
        m_overview_dim_computation_method(OVERVIEW_ROUNDED)
    {}

    int m_has_getinfo; // Does it have meaningful implementation
    int m_has_geotransform;
    GDALWMSOverviewDimComputationMethod m_overview_dim_computation_method;
};

/* All data returned by mini-driver as pointer should remain valid for mini-driver lifetime
   and should be freed by mini-driver destructor unless otherwise specified.
 */

// Base class for minidrivers
// A minidriver has to implement at least the Initialize and the TiledImageRequest
//
class WMSMiniDriver {
friend class GDALWMSDataset;
public:
    WMSMiniDriver() : m_parent_dataset(NULL) {}
    virtual ~WMSMiniDriver() {}

public:
    // MiniDriver specific initialization from XML, required
    // Called once at the beginning of the dataset initialization
    virtual CPLErr Initialize(CPLXMLNode *config, char **papszOpenOptions) = 0;

    // Called once at the end of the dataset initialization
    virtual CPLErr EndInit() { return CE_None; }

    // Error message returned in url, required
    // Set error message in request.Error
    // If tile doesn't exist serverside, set request.range to "none"
    virtual CPLErr TiledImageRequest(CPL_UNUSED WMSHTTPRequest &,
        CPL_UNUSED const GDALWMSImageRequestInfo &iri,
        CPL_UNUSED const GDALWMSTiledImageRequestInfo &tiri) = 0;

    // change capabilities to be used by the parent
    virtual void GetCapabilities(CPL_UNUSED WMSMiniDriverCapabilities *caps) {}

    // signal by setting the m_has_getinfo in the GetCapabilities call
    virtual void GetTiledImageInfo(CPL_UNUSED CPLString &url,
        CPL_UNUSED const GDALWMSImageRequestInfo &iri,
        CPL_UNUSED const GDALWMSTiledImageRequestInfo &tiri,
        CPL_UNUSED int nXInBlock,
        CPL_UNUSED int nYInBlock) {}

    virtual const char *GetProjectionInWKT() {
        if (!m_projection_wkt.empty())
            return m_projection_wkt.c_str();
        return NULL;
    }

    virtual char **GetMetadataDomainList() {
        return NULL;
    }

protected:
    CPLString m_base_url;
    CPLString m_projection_wkt;
    GDALWMSDataset *m_parent_dataset;
};

class WMSMiniDriverFactory {
public:
    WMSMiniDriverFactory() {}
    virtual ~WMSMiniDriverFactory() {}

public:
    virtual WMSMiniDriver* New() const = 0;
    CPLString m_name;
};

// Interface with the global mini driver manager
WMSMiniDriver *NewWMSMiniDriver(const CPLString &name);
void WMSRegisterMiniDriverFactory(WMSMiniDriverFactory *mdf);
void WMSDeregisterMiniDrivers(GDALDriver *);

// WARNING: Called by GDALDestructor, unsafe to use any static objects
void WMSDeregister(GDALDriver *);

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
    // Bad name for this function, it only tests that the file is in cache and returns the real name
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

    virtual const char *GetProjectionRef() override;
    virtual CPLErr SetProjection(const char *proj) override;
    virtual CPLErr GetGeoTransform(double *gt) override;
    virtual CPLErr SetGeoTransform(double *gt) override;
    virtual CPLErr AdviseRead(int x0, int y0, int sx, int sy, int bsx, int bsy, GDALDataType bdt, int band_count, int *band_map, char **options) override;

    virtual char      **GetMetadataDomainList() override;
    virtual const char *GetMetadataItem( const char * pszName,
                                         const char * pszDomain = "" ) override;

    void SetColorTable(GDALColorTable *pct) { m_poColorTable=pct; }

    void mSetBand(int i, GDALRasterBand *band) { SetBand(i,band); }
    GDALWMSRasterBand *mGetBand(int i) { return reinterpret_cast<GDALWMSRasterBand *>(GetRasterBand(i)); }

    const GDALWMSDataWindow *WMSGetDataWindow() const {
        return &m_data_window;
    }

    void WMSSetBlockSize(int x, int y) {
        m_block_size_x = x;
        m_block_size_y = y;
    }

    void WMSSetRasterSize(int x, int y) {
        nRasterXSize = x;
        nRasterYSize = y;
    }

    void WMSSetBandsCount(int count) {
        nBands = count;
    }

    void WMSSetClamp(bool flag = true) {
        m_clamp_requests = flag;
    }

    void WMSSetDataType(GDALDataType type) {
        m_data_type = type;
    }

    void WMSSetDataWindow(GDALWMSDataWindow &window) {
        m_data_window = window;
    }

    void WMSSetDefaultBlockSize(int x, int y) {
        m_default_block_size_x = x;
        m_default_block_size_y = y;
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

    void WMSSetNeedsDataWindow(bool flag) {
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

    void SetXML(const char *psz) {
        m_osXML.clear();
        if (psz)
            m_osXML = psz;
    }

    static GDALDataset* Open(GDALOpenInfo *poOpenInfo);
    static int Identify(GDALOpenInfo *poOpenInfo);
    static GDALDataset *CreateCopy( const char * pszFilename,
                                    GDALDataset *poSrcDS,
                                    int bStrict, char ** papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void * pProgressData );

    const char * const * GetHTTPRequestOpts();

    static const char *GetServerConfig(const char *URI, char **papszHTTPOptions);
    static void DestroyCfgMutex();
    static void ClearConfigCache();

protected:
    virtual CPLErr IRasterIO(GDALRWFlag rw, int x0, int y0, int sx, int sy, void *buffer,
                             int bsx, int bsy, GDALDataType bdt,
                             int band_count, int *band_map,
                             GSpacing nPixelSpace, GSpacing nLineSpace,
                             GSpacing nBandSpace,
                             GDALRasterIOExtraArg* psExtraArg) override;
    CPLErr Initialize(CPLXMLNode *config, char **papszOpenOptions);

    GDALWMSDataWindow m_data_window;
    WMSMiniDriver *m_mini_driver;
    WMSMiniDriverCapabilities m_mini_driver_caps;
    GDALWMSCache *m_cache;
    CPLString m_projection;
    GDALColorTable *m_poColorTable;
    std::vector<double> vNoData;
    std::vector<double> vMin;
    std::vector<double> vMax;
    GDALDataType m_data_type;
    int m_block_size_x;
    int m_block_size_y;
    GDALWMSRasterIOHint m_hint;
    int m_use_advise_read;
    int m_verify_advise_read;
    int m_offline_mode;
    int m_http_max_conn;
    int m_http_timeout;
    char **m_http_options;
    // Open Option list for tiles
    char **m_tileOO;
    int m_clamp_requests;
    int m_unsafeSsl;
    std::set<int> m_http_zeroblock_codes;
    int m_zeroblock_on_serverexceptions;
    CPLString m_osUserAgent;
    CPLString m_osReferer;
    CPLString m_osUserPwd;

    GDALWMSDataWindow m_default_data_window;
    int m_default_block_size_x;
    int m_default_block_size_y;
    int m_default_tile_count_x;
    int m_default_tile_count_y;
    int m_default_overview_count;

    bool m_bNeedsDataWindow;

    CPLString m_osXML;

    // Per session cache of server configurations
    typedef std::map<CPLString, CPLString> StringMap_t;
    static CPLMutex *cfgmtx;
    static StringMap_t cfg;
};

/************************************************************************/
/*                            GDALWMSRasterBand                         */
/************************************************************************/

class GDALWMSRasterBand : public GDALPamRasterBand {
    friend class GDALWMSDataset;
    void    ComputeRequestInfo( GDALWMSImageRequestInfo &iri,
                                GDALWMSTiledImageRequestInfo &tiri,
                                int x, int y);

    CPLString osMetadataItem;
    CPLString osMetadataItemURL;

public:
    GDALWMSRasterBand( GDALWMSDataset *parent_dataset, int band, double scale );
    virtual ~GDALWMSRasterBand();
    bool AddOverview(double scale);
    virtual double GetNoDataValue( int * ) override;
    virtual double GetMinimum( int * ) override;
    virtual double GetMaximum( int * ) override;
    virtual GDALColorTable *GetColorTable() override;
    virtual CPLErr AdviseRead(int x0, int y0, int sx, int sy, int bsx, int bsy, GDALDataType bdt, char **options) override;

    virtual GDALColorInterp GetColorInterpretation() override;
    virtual CPLErr SetColorInterpretation( GDALColorInterp ) override;
    virtual CPLErr IReadBlock(int x, int y, void *buffer) override;
    virtual CPLErr IRasterIO(GDALRWFlag rw, int x0, int y0, int sx, int sy, void *buffer, int bsx, int bsy, GDALDataType bdt,
                             GSpacing nPixelSpace, GSpacing nLineSpace,
                             GDALRasterIOExtraArg* psExtraArg) override;
    virtual int HasArbitraryOverviews() override;
    virtual int GetOverviewCount() override;
    virtual GDALRasterBand *GetOverview(int n) override;

    virtual char      **GetMetadataDomainList() override;
    virtual const char *GetMetadataItem( const char * pszName,
                                         const char * pszDomain = "" ) override;

protected:
    CPLErr ReadBlocks(int x, int y, void *buffer, int bx0, int by0, int bx1, int by1, int advise_read);
    bool IsBlockInCache(int x, int y);
    CPLErr AskMiniDriverForBlock(WMSHTTPRequest &request, int x, int y);
    CPLErr ReadBlockFromFile(int x, int y, const char *file_name, int to_buffer_band, void *buffer, int advise_read);
    CPLErr ZeroBlock(int x, int y, int to_buffer_band, void *buffer);
    static CPLErr ReportWMSException(const char *file_name);

protected:
    GDALWMSDataset *m_parent_dataset;
    double m_scale;
    std::vector<GDALWMSRasterBand *> m_overviews;
    int m_overview;
    GDALColorInterp m_color_interp;
};

#endif /* notdef WMSDRIVER_H_INCLUDED */
