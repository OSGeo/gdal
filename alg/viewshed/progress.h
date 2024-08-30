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

#include <functional>
#include <mutex>

#include "cpl_progress.h"

namespace gdal
{
namespace viewshed
{

/// Support for progress reporting in viewshed construction. Determines the faction of
/// progress made based on the number of raster lines completed.
class Progress
{
  public:
    Progress(GDALProgressFunc pfnProgress, void *pProgressArg,
             size_t expectedLines);

    bool lineComplete();
    bool emit(double fraction);

  private:
    using ProgressFunc = std::function<bool(double frac, const char *msg)>;

    size_t m_lines{0};       ///< Number of lines completed.
    size_t m_expectedLines;  ///< Number of lines expected.
    std::mutex m_mutex{};    ///< Progress function might not be thread-safe.
    ProgressFunc m_cb{};     ///< Progress callback function.
};

}  // namespace viewshed
}  // namespace gdal
