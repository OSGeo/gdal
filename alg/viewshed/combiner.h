/******************************************************************************
 * (c) 2024 info@hobu.co
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef VIEWSHED_COMBINER_H_INCLUDED
#define VIEWSHED_COMBINER_H_INCLUDED

#include "cumulative.h"
#include "viewshed_types.h"

namespace gdal
{
namespace viewshed
{

/// Reads completed viewshed rasters and sums them together. When the
/// summed values may exceed the 8-bit limit, push it on the output
/// queue.
class Combiner
{
  public:
    /// Constructor
    /// @param inputQueue  Reference to input queue of datasets
    /// @param outputQueue  Reference to output queue of datasets
    Combiner(Cumulative::DatasetQueue &inputQueue,
             Cumulative::DatasetQueue &outputQueue)
        : m_inputQueue(inputQueue), m_outputQueue(outputQueue)
    {
    }

    /// Copy ctor. Allows initialization in a vector of Combiners.
    /// @param src  Source Combiner.
    // cppcheck-suppress missingMemberCopy
    Combiner(const Combiner &src)
        : m_inputQueue(src.m_inputQueue), m_outputQueue(src.m_outputQueue)
    {
    }

    void queueOutputBuffer();
    void run();

  private:
    Cumulative::DatasetQueue &m_inputQueue;
    Cumulative::DatasetQueue &m_outputQueue;
    DatasetPtr m_dataset{};
    size_t m_count{0};

    void sum(DatasetPtr srcDs);
};

}  // namespace viewshed
}  // namespace gdal

#endif
