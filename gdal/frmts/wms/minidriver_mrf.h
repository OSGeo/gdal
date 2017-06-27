/******************************************************************************
* $Id: minidriver_mrf.h 35774 2016-10-17 00:35:33Z lplesea $
*
* Project:  WMS driver
* Purpose:  Access a remote MRF tile by tile, using range requests
* Author:   Lucian Plesea
*
******************************************************************************
* Copyright (c) 2016, Lucian Plesea
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

#include <vector>

namespace WMSMiniDriver_MRF_ns {

    //
// Almost like pread, but no thread safety
// Unlike pread, the first argument is a pointer to an opaque structure
// Return of zero means an error occurred (could be end of file)
//
typedef size_t(*pread_t)(void *user_data, void *buff, size_t count, off_t offset);

//
// A sector cache, for up to N sectors of a fixed size M
// N has to be at least two, the user specifies extras
// Used for session caching the remote index
//
class SectorCache {
public:
    SectorCache(void *user_data,
                pread_t fn = NULL,
                unsigned int size = 1024,
                unsigned int count = 2);

    // Fetches a pointer within the sector to the byte at the given address
    // No alignment is guaranteed, and only enough bytes to reach the end of the sector are available
    void *data(size_t address);

private:
    SectorCache();
    struct Sector {
        std::vector<char> range;
        size_t uid;
    };

    // N sectors of M bytes each
    unsigned int n, m;

    // Pointer to an pread like function
    pread_t reader;
    void *reader_data;
    // To avoid thrashing
    Sector *last_used;
    std::vector<Sector> store;
};

// Size of an image, also used as a tile or pixel location
struct ILSize {
    ILSize():  x(0), y(0), z(0), c(0), l(0) {}
    ILSize(GInt32 _x, GInt32 _y, GInt32 _z = 1, GInt32 _c = 1, GInt32 _l = -1) :
        x(_x), y(_y), z(_z), c(_c), l(_l) {}
    GInt32 x, y, z, c;
    GIntBig l; // Dual use, sometimes it holds the number of pages
};

} // namespace WMSMiniDriver_MRF

class WMSMiniDriver_MRF : public WMSMiniDriver {
public:
    WMSMiniDriver_MRF();
    virtual ~WMSMiniDriver_MRF();

    virtual CPLErr Initialize(CPLXMLNode *config, char **papszOpenOptions) override;
    virtual CPLErr EndInit() override;

    virtual CPLErr TiledImageRequest(WMSHTTPRequest &url,
                                    const GDALWMSImageRequestInfo &iri,
                                    const GDALWMSTiledImageRequestInfo &tiri) override;

    enum { tMRF, tBundle, tEND };

private:
    size_t GetIndexAddress(const GDALWMSTiledImageRequestInfo &tiri);

    // The path or URL for index
    CPLString m_idxname;

    // Which type of remote file this is, one of the types above
    int m_type;

    VSILFILE *fp; // If index is a file
    WMSHTTPRequest *m_request; // If index is an URL
    WMSMiniDriver_MRF_ns::SectorCache *index_cache;

    // Per level index offsets, level 0 being the full resolution
    std::vector<GUIntBig> offsets;
    // Matching pagecounts
    std::vector<WMSMiniDriver_MRF_ns::ILSize> pages;
};
