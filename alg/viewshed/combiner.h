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

#pragma once

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
