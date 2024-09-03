/******************************************************************************
 * (c) 2024 info@hobu.co
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
#include "util.h"

namespace gdal
{
namespace viewshed
{

/// Read viewshed executor output and sum it up in our owned memory raster.
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

/// Add the values of the source dataset to those of the owned dataset.
/// @param src  Source dataset.
void Combiner::sum(DatasetPtr src)
{
    if (!m_dataset)
    {
        m_dataset = std::move(src);
        return;
    }
    size_t size = bandSize(*m_dataset->GetRasterBand(1));

    uint8_t *dstP =
        static_cast<uint8_t *>(m_dataset->GetInternalHandle("MEMORY1"));
    uint8_t *srcP = static_cast<uint8_t *>(src->GetInternalHandle("MEMORY1"));
    for (size_t i = 0; i < size; ++i)
        *dstP++ += *srcP++;
    // If we've seen 255 inputs, queue our raster for output and rollup since we might overflow
    // otherwise.
    if (++m_count == 255)
        queueOutputBuffer();
}

/// Queue the owned buffer as for output.
void Combiner::queueOutputBuffer()
{
    if (m_dataset)
        m_outputQueue.push(std::move(m_dataset));
    m_count = 0;
}

}  // namespace viewshed
}  // namespace gdal
