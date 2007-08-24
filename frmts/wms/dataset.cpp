/******************************************************************************
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

#include "stdinc.h"

GDALWMSDataset::GDALWMSDataset() {
    m_mini_driver = 0;
    m_cache = 0;
    m_hint.m_valid = false;
    m_data_type = GDT_Byte;
}

GDALWMSDataset::~GDALWMSDataset() {
    if (m_mini_driver) delete m_mini_driver;
    if (m_cache) delete m_cache;
}
CPLErr GDALWMSDataset::Initialize(CPLXMLNode *config) {
    CPLErr ret = CE_None;

    if (ret == CE_None) {
        CPLXMLNode *n = CPLGetXMLNode(config, "DataWindow");
        if (n == NULL) {
            CPLError(CE_Failure, CPLE_AppDefined, "GDALWMS: DataWindow missing.\n");
            ret = CE_Failure;
        } else {
            m_data_window.Initialize(n);
        }
    }
    if (ret == CE_None) {
        const char *proj = CPLGetXMLValue(config, "Projection", "");
        if (proj[0] != '\0') {
            m_projection = ProjToWKT(proj);
            if (m_projection.size() == 0) {
                CPLError(CE_Failure, CPLE_AppDefined, "GDALWMS: Bad projection specified.\n");
                ret = CE_Failure;
            }
        }
    }
    if (ret == CE_None) {
        const char *overview_count = CPLGetXMLValue(config, "OverviewCount", "");
        if (overview_count[0] != '\0') {
            m_overview_count = atoi(overview_count);
        } else {
            double a = log(static_cast<double>(MIN(m_data_window.m_sx, m_data_window.m_sy)))/log(2.0) - 5.0;
            m_overview_count = MAX(0, MIN(static_cast<int>(a), 32));
        }
    }
    if (ret == CE_None) {
        const char *block_size_x = CPLGetXMLValue(config, "BlockSizeX", "256");
        const char *block_size_y = CPLGetXMLValue(config, "BlockSizeY", "256");
        m_block_size_x = atoi(block_size_x);
        m_block_size_y = atoi(block_size_y);
    }
    if (ret == CE_None) {
        const char *bands_count = CPLGetXMLValue(config, "BandsCount", "3");
        m_bands_count = atoi(bands_count);
    }

    if (ret == CE_None) {
        CPLXMLNode *n = CPLGetXMLNode(config, "Cache");
        if (n != NULL) {
            m_cache = new GDALWMSCache();
            if (m_cache->Initialize(n) != CE_None) {
                delete m_cache;
                m_cache = NULL;
                ret = CE_Failure;
            }
        }
    }

    if (ret == CE_None) {
        CPLXMLNode *service = CPLGetXMLNode(config, "Service");
        if (service != NULL) {
            const char *service_name = CPLGetXMLValue(service, "name", "");
            if (service_name[0] != '\0') {
                GDALWMSMiniDriverManager *const mdm = GetGDALWMSMiniDriverManager();
                GDALWMSMiniDriverFactory *const mdf = mdm->Find(CPLString(service_name));
                if (mdf != NULL) {
                    m_mini_driver = mdf->New();
                    if (m_mini_driver->Initialize(service) == CE_None) {
                        m_mini_driver->GetCapabilities(&m_mini_driver_caps);
                    } else {
                        delete m_mini_driver;
                        m_mini_driver = NULL;
                    }
                } else {
                    CPLError(CE_Failure, CPLE_AppDefined, "GDALWMS: No mini-driver registered for '%s'.\n", service_name);
                    ret = CE_Failure;
                }
            } else {
                CPLError(CE_Failure, CPLE_AppDefined, "GDALWMS: No Service specified.\n");
                ret = CE_Failure;
            }
        } else {
            CPLError(CE_Failure, CPLE_AppDefined, "GDALWMS: No Service specified.\n");
            ret = CE_Failure;
        }
    }

    if (ret == CE_None) {
        nRasterXSize = m_data_window.m_sx;
        nRasterYSize = m_data_window.m_sy;
        for (int i = 0; i < m_bands_count; ++i) {
            GDALWMSRasterBand *band = new GDALWMSRasterBand(this, i, 1.0);
            SetBand(i + 1, band);
            double scale = 0.5;
            for (int j = 0; j < m_overview_count; ++j) {
                band->AddOverview(scale);
                scale *= 0.5;
            }
        }
    }

    return ret;
}

CPLErr GDALWMSDataset::IRasterIO(GDALRWFlag rw, int x0, int y0, int sx, int sy, void *buffer, int bsx, int bsy, GDALDataType bdt, int band_count, int *band_map, int pixel_space, int line_space, int band_space) {
    CPLErr ret;

    if (rw != GF_Read) return CE_Failure;
    if (buffer == NULL) return CE_Failure;
    if ((sx == 0) || (sy == 0) || (bsx == 0) || (bsy == 0) || (band_count == 0)) return CE_None;

    m_hint.m_x0 = x0;
    m_hint.m_y0 = y0;
    m_hint.m_sx = sx;
    m_hint.m_sy = sy;
    m_hint.m_overview = -1;
    m_hint.m_valid = true;
    //	printf("[%p] GDALWMSDataset::IRasterIO(x0: %d, y0: %d, sx: %d, sy: %d, bsx: %d, bsy: %d, band_count: %d, band_map: %p)\n", this, x0, y0, sx, sy, bsx, bsy, band_count, band_map);
    ret = GDALDataset::IRasterIO(rw, x0, y0, sx, sy, buffer, bsx, bsx, bdt, band_count, band_map, pixel_space, line_space, band_space);
    m_hint.m_valid = false;

    return ret;
}

const char *GDALWMSDataset::GetProjectionRef() {
    return m_projection.c_str();
}

CPLErr GDALWMSDataset::SetProjection(const char *proj) {
    return CE_Failure;
}

CPLErr GDALWMSDataset::GetGeoTransform(double *gt) {
    gt[0] = m_data_window.m_x0;
    gt[1] = (m_data_window.m_x1 - m_data_window.m_x0) / static_cast<double>(m_data_window.m_sx);
    gt[2] = 0.0;
    gt[3] = m_data_window.m_y0;
    gt[4] = 0.0;
    gt[5] = (m_data_window.m_y1 - m_data_window.m_y0) / static_cast<double>(m_data_window.m_sy);
    return CE_None;
}

CPLErr GDALWMSDataset::SetGeoTransform(double *gt) {
    return CE_Failure;
}
