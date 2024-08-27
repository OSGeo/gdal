/******************************************************************************
 *
 * Project:  Viewshed Generation
 * Purpose:  Core algorithm implementation for viewshed generation.
 *
 ******************************************************************************
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

#include "combiner.h"

namespace gdal
{
namespace viewshed
{

void Combiner::run()
{
    DatasetPtr pTempDataset;

    while (m_inputQueue.pop(pTempDataset))
    {
        if (!m_dataset)
            m_dataset = std::move(pTempDataset);
        else
            sum(std::move(pTempDataset));
    }
    // Queue remaining summed rasters.
    queueOutputBuffer();
}

void Combiner::sum(DatasetPtr src)
{
    if (!m_dataset)
    {
        m_dataset = std::move(src);
        return;
    }
    GDALRasterBand *dstBand = m_dataset->GetRasterBand(1);

    size_t size = dstBand->GetXSize() * dstBand->GetYSize();

    uint8_t *dstP =
        static_cast<uint8_t *>(m_dataset->GetInternalHandle("MEMORY1"));
    uint8_t *srcP = static_cast<uint8_t *>(src->GetInternalHandle("MEMORY1"));
    for (size_t i = 0; i <= size; ++i)
        *dstP++ += *srcP++;
    if (++m_count == 255)
        queueOutputBuffer();
}

void Combiner::queueOutputBuffer()
{
    if (!m_dataset)
        return;

    uint8_t *srcP =
        static_cast<uint8_t *>(m_dataset->GetInternalHandle("MEMORY1"));

    GDALRasterBand *srcBand = m_dataset->GetRasterBand(1);
    size_t size = srcBand->GetXSize() * srcBand->GetYSize();

    Cumulative::Buf8 output(srcP, srcP + size);
    m_dataset.reset();
    m_count = 0;
    m_outputQueue.push(std::move(output));
}

}  // namespace viewshed
}  // namespace gdal
