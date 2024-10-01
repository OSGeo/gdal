/******************************************************************************
 * (c) 2024 info@hobu.co
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef VIEWSHED_PROGRESS_H_INCLUDED
#define VIEWSHED_PROGRESS_H_INCLUDED

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

#endif
