/******************************************************************************
 *
 * Project:  HEIF read-only Driver
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2020, Even Rouault <even.rouault at spatialys.com>
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

#ifndef HEIFDATASET_H_INCLUDED_
#define HEIFDATASET_H_INCLUDED_

#include "gdal_pam.h"
#include "ogr_spatialref.h"

#include "include_libheif.h"

#include "heifdrivercore.h"

#include <vector>

/************************************************************************/
/*                        GDALHEIFDataset                               */
/************************************************************************/

class GDALHEIFDataset final : public GDALPamDataset
{
    friend class GDALHEIFRasterBand;

    heif_context *m_hCtxt = nullptr;
    heif_image_handle *m_hImageHandle = nullptr;
    heif_image *m_hImage = nullptr;
    bool m_bFailureDecoding = false;
    std::vector<std::unique_ptr<GDALHEIFDataset>> m_apoOvrDS{};
    bool m_bIsThumbnail = false;

#ifdef HAS_CUSTOM_FILE_READER
    heif_reader m_oReader{};
    VSILFILE *m_fpL = nullptr;
    vsi_l_offset m_nSize = 0;

    static int64_t GetPositionCbk(void *userdata);
    static int ReadCbk(void *data, size_t size, void *userdata);
    static int SeekCbk(int64_t position, void *userdata);
    static enum heif_reader_grow_status WaitForFileSizeCbk(int64_t target_size,
                                                           void *userdata);
#endif

    bool Init(GDALOpenInfo *poOpenInfo);
    void ReadMetadata();
    void OpenThumbnails();

#ifdef HAS_CUSTOM_FILE_WRITER
    static heif_error VFS_WriterCallback(struct heif_context *ctx,
                                         const void *data, size_t size,
                                         void *userdata);
#endif

  public:
    GDALHEIFDataset();
    ~GDALHEIFDataset();

    static GDALDataset *Open(GDALOpenInfo *poOpenInfo);

#ifdef HAS_CUSTOM_FILE_WRITER
    static GDALDataset *CreateCopy(const char *pszFilename,
                                   GDALDataset *poSrcDS, int bStrict,
                                   char **papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);
#endif
};
#endif /* HEIFDATASET_H_INCLUDED_ */