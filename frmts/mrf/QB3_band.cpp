/*
Copyright 2021 Esri
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

QB3 band implementation
QB3 page compression and decompression functions

Authors:  Lucian Plesea
*/
#include "marfa.h"
#include <QB3.h>

NAMESPACE_MRF_START
CPLErr QB3_Band::Compress(buf_mgr& dst, buf_mgr& src)
{
    auto bands = static_cast<size_t>(img.pagesize.c);
    encsp pQB3 = nullptr;
#define CREATE_QB3(T) qb3_create_encoder(img.pagesize.x, img.pagesize.y, bands, qb3_dtype::T)

    switch (img.dt) {
    case (GDT_Byte):
        pQB3 = CREATE_QB3(QB3_U8);
        break;
    case (GDT_Int16):
        pQB3 = CREATE_QB3(QB3_I16);
        break;
    case (GDT_UInt16):
        pQB3 = CREATE_QB3(QB3_U16);
        break;
    case (GDT_Int32):
        pQB3 = CREATE_QB3(QB3_I32);
        break;
    case (GDT_UInt32):
        pQB3 = CREATE_QB3(QB3_U32);
        break;
    default:
        CPLError(CE_Failure, CPLE_AssertionFailed, "MRF:QB3 Data type not supported");
        return CE_Failure;
    }
#undef CREATE_QB3

    if (nullptr == pQB3) {
        CPLError(CE_Failure, CPLE_AssertionFailed, "MRF:QB3 Cannot create encoder");
        return CE_Failure;
    }

    CPLErr status = CE_None;
    try {
        if (dst.size < qb3_max_encoded_size(pQB3)) {
            CPLError(CE_Failure, CPLE_AssertionFailed, "MRF:QB3 encoded buffer size too small");
            throw CE_Failure;
        }

        // Use independent band compression when by default band 1 is core band
        if ((3 == bands || 4 == bands) &&
            EQUAL(poMRFDS->GetPhotometricInterpretation(), "MULTISPECTRAL")) {
            size_t corebands[] = { 0, 1, 2, 3 }; // Identity, no core bands
            qb3_set_encoder_coreband(pQB3, bands, corebands);
        }

        // Quality of 90 and above trigger the better encoding
        qb3_set_encoder_mode(pQB3, (img.quality > 90) ? QB3M_BEST : QB3M_BASE);

        dst.size = qb3_encode(pQB3, src.buffer, dst.buffer);
        if (0 == dst.size) {
            CPLError(CE_Failure, CPLE_AssertionFailed, "MRF:QB3 encoding failed");
            throw CE_Failure;
        }

        // Never happens if qb3_max_encoded doesn't lie
        if (dst.size > qb3_max_encoded_size(pQB3)) {
            CPLError(CE_Failure, CPLE_AssertionFailed, "MRF:QB3 encoded size exceeds limit, check QB3 library");
            throw CE_Failure;
        }
    }
    catch (CPLErr error) {
        status = error;
    }
    qb3_destroy_encoder(pQB3);
    return status;
}

CPLErr QB3_Band::Decompress(buf_mgr& dst, buf_mgr& src)
{
    size_t img_size[3];
    auto pdQB3 = qb3_read_start(src.buffer, src.size, img_size);
    if (nullptr == pdQB3) {
        CPLError(CE_Failure, CPLE_AppDefined, "MRF: QB3 can't create decoder, is it a valid QB3 stream?");
        return CE_Failure;
    }

    CPLErr status = CE_None;
    try {
        if (img_size[0] != static_cast<size_t>(img.pagesize.x)
            || img_size[1] != static_cast<size_t>(img.pagesize.y)
            || img_size[2] != static_cast<size_t>(img.pagesize.c)) {
            CPLError(CE_Failure, CPLE_AppDefined, "MRF: QB Page has invalid size");
            throw CE_Failure;
        }

        if (!qb3_read_info(pdQB3)) {
            CPLError(CE_Failure, CPLE_AppDefined, "MRF: QB3 metadata read failure");
            throw CE_Failure;
        }

        if (static_cast<size_t>(img.pageSizeBytes) != qb3_decoded_size(pdQB3)) {
            CPLError(CE_Failure, CPLE_AppDefined, "MRF: QB3 incorect decoded tile size");
            throw CE_Failure;
        }

        dst.size = qb3_read_data(pdQB3, dst.buffer);
        if (static_cast<size_t>(img.pageSizeBytes) != dst.size) {
            CPLError(CE_Failure, CPLE_AppDefined, "MRF: QB3 decoding error");
            throw CE_Failure;
        }
    }
    catch (CPLErr error) {
        status = error;
    }
    qb3_destroy_decoder(pdQB3);
    return status;
}

QB3_Band::QB3_Band(MRFDataset* pDS, const ILImage& image, int b, int level)
    : MRFRasterBand(pDS, image, b, level)
{
    static_assert(CPL_IS_LSB, "QB3 is only implemented for little endian architectures");
    if (image.pageSizeBytes > INT_MAX / 4)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "QB3 page too large");
        return;
    }

    if (0 != nBlockXSize % 4 || 0 != nBlockYSize % 4) {
        CPLError(CE_Failure, CPLE_NotSupported, "QB3 page size has to be a multiple of 4");
        return;
    }

    if (image.dt != GDT_Byte && image.dt != GDT_Int16 && image.dt != GDT_UInt16
        && image.dt != GDT_Int32 && image.dt != GDT_UInt32)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
            "Data type not supported by QB3 compression");
        return;
    }

    // Should use qb3_max_encoded_size();

    // Enlarge the page buffer, QB3 may expand data.
    pDS->SetPBufferSize(2 * image.pageSizeBytes);
}

NAMESPACE_MRF_END
