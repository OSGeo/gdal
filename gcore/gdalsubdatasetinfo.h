/***************************************************************************
  gdal_subdatasetinfo.h - GDALSubdatasetInfo

 ---------------------
 begin                : 21.7.2023
 copyright            : (C) 2023 by ale
 email                : [your-email-here]
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef GDALSUBDATASETINFO_H
#define GDALSUBDATASETINFO_H

#include "cpl_port.h"
#include <string>
/**
 * The GDALSubdatasetInfo struct provides methods to extract and
 * manipulate subdataset information from a file name.
 *
 * Drivers offering this functionality should override the methods.
 */
struct GDALSubdatasetInfo
{

    virtual ~GDALSubdatasetInfo() = default;

    /**
 * @brief Checks wether the file name syntax represents a subdataset
 * @param fileName           File name
 * @note                        This method does not check if the subdataset actually exists but only if the
 *                              file name syntax represents a subdataset
 * @return                      true if the file name represents a subdataset
 * @since                       GDAL 3.8
 */
    virtual bool IsSubdatasetSyntax(const std::string &fileName) const
    {
        (void)fileName;
        return false;
    }

    /**
 * @brief Returns the path to the file, stripping any subdataset information from the file name
 * @param fileName           File name
 * @note                        This method does not check if the subdataset or the file actually exist.
 *                              If the file name does not represent a subdataset it is returned unmodified.
 * @return                      The path to the file
 * @since                       GDAL 3.8
 */
    virtual std::string
    GetFilenameFromSubdatasetName(const std::string &fileName) const
    {
        (void)fileName;
        return "";
    }
};

#endif  // GDALSUBDATASETINFO_H
