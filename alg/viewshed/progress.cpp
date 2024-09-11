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

#include <algorithm>

#include "progress.h"

#include "cpl_error.h"

namespace gdal
{
namespace viewshed
{

/// Constructor
/// @param pfnProgress  Pointer to progress function.
/// @param pProgressArg  Pointer to progress function data.
/// @param expectedLines  Number of lines expected to be processed.
Progress::Progress(GDALProgressFunc pfnProgress, void *pProgressArg,
                   size_t expectedLines)
    : m_expectedLines(std::max(expectedLines, static_cast<size_t>(1)))
{
    using namespace std::placeholders;

    // cppcheck-suppress useInitializationList
    m_cb = std::bind(pfnProgress, _1, _2, pProgressArg);
}

/// Emit progress information saying that a line has been written to output.
///
/// @return  True on success, false otherwise.
bool Progress::lineComplete()
{
    double fraction;
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_lines < m_expectedLines)
            m_lines++;
        fraction = m_lines / static_cast<double>(m_expectedLines);
    }
    return emit(fraction);
}

/// Emit progress information saying that a fraction of work has been completed.
///
/// @return  True on success, false otherwise.
bool Progress::emit(double fraction)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Call the progress function.
    if (!m_cb(fraction, ""))
    {
        CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
        return false;
    }
    return true;
}

}  // namespace viewshed
}  // namespace gdal
