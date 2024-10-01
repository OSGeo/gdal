/******************************************************************************
 * (c) 2024 info@hobu.co
 *
 * SPDX-License-Identifier: MIT
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
