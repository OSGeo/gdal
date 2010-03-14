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

class GDALWMSDataset;
class GDALWMSRasterBand;

CPLString MD5String(const char *s);
CPLString ProjToWKT(const CPLString &proj);
void URLAppend(CPLString *url, const char *s);
void URLAppendF(CPLString *url, const char *s, ...);
void URLAppend(CPLString *url, const CPLString &s);
CPLString BufferToVSIFile(GByte *buffer, size_t size);
CPLErr MakeDirs(const char *path);
int StrToBool(const char *p);
int URLSearchAndReplace (CPLString *base, const char *search, const char *fmt, ...);

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
};

class GDALWMSTiledImageRequestInfo {
public:
    int m_x, m_y;
    int m_level;
};

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

class GDALWMSDataset : public GDALPamDataset {
    friend class GDALWMSRasterBand;
    friend GDALDataset *GDALWMSDatasetOpen(GDALOpenInfo *poOpenInfo);

public:
    GDALWMSDataset();
    virtual ~GDALWMSDataset();

public:
    virtual const char *GetProjectionRef();
    virtual CPLErr SetProjection(const char *proj);
    virtual CPLErr GetGeoTransform(double *gt);
    virtual CPLErr SetGeoTransform(double *gt);
    virtual CPLErr AdviseRead(int x0, int y0, int sx, int sy, int bsx, int bsy, GDALDataType bdt, int band_count, int *band_map, char **options);

protected:
    virtual CPLErr IRasterIO(GDALRWFlag rw, int x0, int y0, int sx, int sy, void *buffer, int bsx, int bsy, GDALDataType bdt, int band_count, int *band_map, int pixel_space, int line_space, int band_space);

protected:
    CPLErr Initialize(CPLXMLNode *config);

public:
    const GDALWMSDataWindow *WMSGetDataWindow() const;
    int WMSGetBlockSizeX() const;
    int WMSGetBlockSizeY() const;

protected:
    GDALWMSDataWindow m_data_window;
    GDALWMSMiniDriver *m_mini_driver;
    GDALWMSMiniDriverCapabilities m_mini_driver_caps;
    GDALWMSCache *m_cache;
    CPLString m_projection;
    int m_overview_count;
    GDALDataType m_data_type;
    int m_block_size_x, m_block_size_y;
    GDALWMSRasterIOHint m_hint;
    int m_use_advise_read;
    int m_verify_advise_read;
    int m_offline_mode;
    int m_http_max_conn;
    int m_http_timeout;
    int m_clamp_requests;
    CPLString m_osUserAgent;
};

class GDALWMSRasterBand : public GDALPamRasterBand {
    friend class GDALWMSDataset;

public:
    GDALWMSRasterBand(GDALWMSDataset *parent_dataset, int band, double scale);
    virtual ~GDALWMSRasterBand();

public:
    virtual CPLErr AdviseRead(int x0, int y0, int sx, int sy, int bsx, int bsy, GDALDataType bdt, char **options);

protected:
    CPLErr ReadBlocks(int x, int y, void *buffer, int bx0, int by0, int bx1, int by1, int advise_read);
    virtual CPLErr IReadBlock(int x, int y, void *buffer);
    virtual CPLErr IRasterIO(GDALRWFlag rw, int x0, int y0, int sx, int sy, void *buffer, int bsx, int bsy, GDALDataType bdt, int pixel_space, int line_space);
    virtual int HasArbitraryOverviews();
    virtual int GetOverviewCount();
    virtual GDALRasterBand *GetOverview(int n);
    void AddOverview(double scale);
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
};

GDALDataset *GDALWMSDatasetOpen(GDALOpenInfo *poOpenInfo);
GDALWMSMiniDriverManager *GetGDALWMSMiniDriverManager();
void DestroyWMSMiniDriverManager(void);
