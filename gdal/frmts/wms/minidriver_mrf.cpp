/******************************************************************************
* $Id$
*
* Project:  WMS Client Mini Driver
* Purpose:  Implementation of Dataset and RasterBand classes for WMS
*           and other similar services.
* Author:   Lucian Plesea
*
******************************************************************************
* Copyright (c) 2016, Lucian Plesea
*
* Copyright 2016 Esri
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
****************************************************************************/

/*
 A WMS style minidriver that allows an MRF or an Esri bundle to be read from a URL, using one range request per tile
 All parameters have to be defined in the WMS file, especially for the MRF, so only simple MRF files work.
 For a bundle, the size is assumed to be 128 tiles of 256 pixels each, which is the standard size.
 */

#include "wmsdriver.h"
#include "minidriver_mrf.h"

CPL_CVSID("$Id$")

using namespace WMSMiniDriver_MRF_ns;

// Copied from frmts/mrf

// A tile index record, 16 bytes, big endian
typedef struct {
    GIntBig offset;
    GIntBig size;
} MRFIdx;

// Number of pages of size psz needed to hold n elements
static inline int pcount(const int n, const int sz) {
    return 1 + (n - 1) / sz;
}

// Returns a pagecount per dimension, .l will have the total number
static inline const ILSize pcount(const ILSize &size, const ILSize &psz) {
    ILSize count;
    count.x = pcount(size.x, psz.x);
    count.y = pcount(size.y, psz.y);
    count.z = pcount(size.z, psz.z);
    count.c = pcount(size.c, psz.c);
    count.l = static_cast<GIntBig>(count.x) * count.y * count.z * count.c;
    return count;
}

// End copied from frmts/mrf


// pread_t adapter for VSIL
static size_t pread_VSIL(void *user_data, void *buff, size_t count, off_t offset) {
    VSILFILE *fp = reinterpret_cast<VSILFILE *>(user_data);
    VSIFSeekL(fp, offset, SEEK_SET);
    return VSIFReadL(buff, 1, count, fp);
}

// pread_t adapter for curl.  We use the multi interface to get the same options
static size_t pread_curl(void *user_data, void *buff, size_t count, off_t offset) {
    // Use a copy of the provided request, which has the options and the URL preset
    WMSHTTPRequest request(*(reinterpret_cast<WMSHTTPRequest *>(user_data)));
    request.Range.Printf(CPL_FRMT_GUIB "-" CPL_FRMT_GUIB,
                            static_cast<GUIntBig>(offset),
                            static_cast<GUIntBig>(offset + count - 1));
    WMSHTTPInitializeRequest(&request);
    if (WMSHTTPFetchMulti(&request) != CE_None) {
        CPLError(CE_Failure, CPLE_AppDefined, "GDALWMS_MRF: failed to retrieve index data");
        return 0;
    }

    int success = (request.nStatus == 200) ||
        (!request.Range.empty() && request.nStatus == 206);
    if (!success || request.pabyData == NULL || request.nDataLen == 0) {
        CPLError(CE_Failure, CPLE_HttpResponse,
            "GDALWMS: Unable to download data from %s",
            request.URL.c_str());
        return 0; // Error flag
    }

    // Might get less data than requested
    if (request.nDataLen < count)
        memset(buff, 0, count);
    memcpy(buff, request.pabyData, request.nDataLen);
    return request.nDataLen;
}

SectorCache::SectorCache(void *user_data,
                         pread_t fn,
                         unsigned int size,
                         unsigned int count ) :
    n(count + 2), m(size), reader(fn ? fn : pread_VSIL),
    reader_data(user_data), last_used(NULL)
{
}


// Returns an in-memory offset to the byte at the given address, within a sector
// Returns NULL if the sector can't be read
void *SectorCache::data(size_t address) {
    for (size_t i = 0; i < store.size(); i++) {
        if (store[i].uid == address / m) {
            last_used = &store[i];
            return &(last_used->range[address % m]);
        }
    }

    // Not found, need a target sector to replace
    Sector *target;
    if (store.size() < m) { // Create a new sector if there are slots available
        store.resize(store.size() + 1);
        target = &store.back();
    }
    else { // Choose a random one to replace, but not the last used, to avoid thrashing
        do {
            // coverity[dont_call]
            target = &(store[rand() % n]);
        } while (target == last_used);
    }

    target->range.resize(m);
    if (reader(reader_data, &target->range[0], m, static_cast<off_t>((address / m ) * m))) { // Success
        target->uid = address / m;
        last_used = target;
        return &(last_used->range[address %m]);
    }

    // Failure
    // If this is the last sector, it could be a new sector with invalid data, so we remove it
    // Otherwise, the previous content is still good
    if (target == &store.back())
        store.resize(store.size() - 1);
    // Signal invalid request
    return NULL;
}

// Keep in sync with the type enum
static const int ir_size[WMSMiniDriver_MRF::tEND] = { 16, 8 };

WMSMiniDriver_MRF::WMSMiniDriver_MRF(): m_type(tMRF), fp(NULL), m_request(NULL),index_cache(NULL) {}

WMSMiniDriver_MRF::~WMSMiniDriver_MRF() {
    if (index_cache)
        delete index_cache;
    if (fp)
        VSIFCloseL(fp);
    delete m_request;
}

CPLErr WMSMiniDriver_MRF::Initialize(CPLXMLNode *config, CPL_UNUSED char **papszOpenOptions) {
    // This gets called before the rest of the WMS driver gets initialized
    // The MRF reader only works if all datawindow is defined within the WMS file

    m_base_url = CPLGetXMLValue(config, "ServerURL", "");
    if (m_base_url.empty()) {
        CPLError(CE_Failure, CPLE_AppDefined, "GDALWMS, MRF: ServerURL missing.");
        return CE_Failure;
    }

    // Index file location, in case it is different from the normal file name
    m_idxname = CPLGetXMLValue(config, "index", "");

    CPLString osType(CPLGetXMLValue(config, "type", ""));

    if (EQUAL(osType, "bundle"))
        m_type = tBundle;

    if (m_type == tBundle) {
        m_parent_dataset->WMSSetDefaultOverviewCount(0);
        m_parent_dataset->WMSSetDefaultTileCount(128, 128);
        m_parent_dataset->WMSSetDefaultBlockSize(256, 256);
        m_parent_dataset->WMSSetDefaultTileLevel(0);
        m_parent_dataset->WMSSetNeedsDataWindow(FALSE);
        offsets.push_back(64);
    }
    else { // MRF
        offsets.push_back(0);
    }

    return CE_None;
}

// Test for URL, things that curl can deal with while doing a range request
// http and https should work, not sure about ftp or file
int inline static is_url(const CPLString &value) {
    return (value.ifind("http://") == 0
        || value.ifind("https://") == 0
        || value.ifind("ftp://") == 0
        || value.ifind("file://") == 0
        );
}

// Called after the dataset is initialized by the main WMS driver
CPLErr WMSMiniDriver_MRF::EndInit() {
    int index_is_url = 1;
    if (!m_idxname.empty() ) { // Provided, could be path or URL
        if (!is_url(m_idxname)) {
            index_is_url = 0;
            fp = VSIFOpenL(m_idxname, "rb");
            if (fp == NULL) {
                CPLError(CE_Failure, CPLE_FileIO, "Can't open index file %s", m_idxname.c_str());
                return CE_Failure;
            }
            index_cache = new SectorCache(fp);
        }
    }
    else { // Not provided, change extension to .idx if we can, otherwise use the same file
        m_idxname = m_base_url;
    }

    if (index_is_url) { // prepare a WMS request, the pread_curl will execute it repeatedly
        m_request = new WMSHTTPRequest();
        m_request->URL = m_idxname;
        m_request->options = m_parent_dataset->GetHTTPRequestOpts();
        index_cache = new SectorCache(m_request, pread_curl);
    }

    // Set the level index offsets, assume MRF order since esri bundles don't have overviews
    ILSize size(m_parent_dataset->GetRasterXSize(),
                m_parent_dataset->GetRasterYSize(),
                1, // Single slice for now
                1, // Ignore the c, only single or interleved data supported by WMS
                m_parent_dataset->GetRasterBand(1)->GetOverviewCount());

    int psx, psy;
    m_parent_dataset->GetRasterBand(1)->GetBlockSize(&psx, &psy);
    ILSize pagesize(psx, psy, 1, 1, 1);

    if (m_type == tBundle) { // A bundle contains 128x128 pages, regadless of the raster size
        size.x = psx * 128;
        size.y = psy * 128;
    }

    for (GIntBig l = size.l; l >= 0; l--) {
        ILSize pagecount = pcount(size, pagesize);
        pages.push_back(pagecount);
        if (l > 0) // Only for existing levels
            offsets.push_back(offsets.back() + ir_size[m_type] * pagecount.l);

        // Sometimes this may be a 3
        size.x = pcount(size.x, 2);
        size.y = pcount(size.y, 2);
    }

    return CE_None;
}

// Return -1 if error occurs
size_t WMSMiniDriver_MRF::GetIndexAddress(const GDALWMSTiledImageRequestInfo &tiri) {
    // Bottom level is 0
    int l = - tiri.m_level;
    if (l < 0 || l >= static_cast<int>(offsets.size()))
        return ~static_cast<size_t>(0); // Indexing error
    if (tiri.m_x >= pages[l].x || tiri.m_y >= pages[l].y)
        return ~static_cast<size_t>(0);
    return static_cast<size_t>(offsets[l] + (pages[l].x * tiri.m_y + tiri.m_x) * ir_size[m_type]);
}

// Signal errors and return error message
CPLErr WMSMiniDriver_MRF::TiledImageRequest(WMSHTTPRequest &request,
    CPL_UNUSED const GDALWMSImageRequestInfo &iri,
    const GDALWMSTiledImageRequestInfo &tiri)
{
    CPLString &url = request.URL;
    url = m_base_url;

    size_t offset = GetIndexAddress(tiri);
    if (offset == static_cast<size_t>(-1)) {
        request.Error = "Invalid level requested";
        return CE_Failure;
    }

    void *raw_index = index_cache->data(offset);
    if (raw_index == NULL) {
        request.Error = "Invalid indexing";
        return CE_Failure;
    };

    // Store the tile size and offset in this structure
    MRFIdx idx;

    if (m_type == tMRF) {
        memcpy(&idx, raw_index, sizeof(idx));

#if defined(CPL_LSB) // raw index is MSB
        idx.offset = CPL_SWAP64(idx.offset);
        idx.size = CPL_SWAP64(idx.size);
#endif

    } else { // Bundle
        GIntBig bidx;
        memcpy(&bidx, raw_index, sizeof(bidx));

#if defined(CPL_MSB) // bundle index is LSB
        bidx = CPL_SWAP64(bidx);
#endif

        idx.offset = bidx & ((1ULL << 40) -1);
        idx.size = bidx >> 40;
    }

    // Set the range or flag it as missing
    if (idx.size == 0)
        request.Range = "none"; // Signal that this block doesn't exist server-side
    else
        request.Range.Printf(CPL_FRMT_GUIB "-" CPL_FRMT_GUIB, idx.offset, idx.offset + idx.size - 1);

    return CE_None;
}
