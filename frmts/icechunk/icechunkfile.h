/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Icechunk driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef ICECHUNKFILE_H
#define ICECHUNKFILE_H

#include <string>

namespace gdal::icechunk
{

class IcechunkFile /* non final */
{
  public:
    virtual ~IcechunkFile();

    const std::string &GetFilename() const
    {
        return m_osFilename;
    }

  protected:
    IcechunkFile();

    std::string m_osFilename{};
};

}  // namespace gdal::icechunk

#endif
