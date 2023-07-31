/***************************************************************************
  gdal_subdatasetinfo.h - GDALSubdatasetInfo

 ---------------------
 begin                : 21.7.2023
 copyright            : (C) 2023 by Alessndro Pasotti
 email                : elpaso@itopen.it
 ***************************************************************************
 *                                                                         *
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
 *                                                                         *
 ***************************************************************************/
#ifndef GDALSUBDATASETINFO_H
#define GDALSUBDATASETINFO_H

#include "cpl_port.h"
#include <string>
/**
 * The GDALSubdatasetInfo abstract class provides methods to extract and
 * manipulate subdataset information from a file name that contains subdataset
 * information.
 *
 * Drivers offering this functionality must override the parseFileName() method.
 */
struct GDALSubdatasetInfo
{

  public:
    /**
     * @brief Construct a GDALSubdatasetInfo object from a subdataset file name descriptor.
     * @param fileName          The subdataset file name descriptor.
     */
    GDALSubdatasetInfo(const std::string &fileName);

    virtual ~GDALSubdatasetInfo() = default;

    /**
 * @brief Returns the path to the file, stripping any subdataset information from the file name
 * @return                      The path to the file
 * @since                       GDAL 3.8
 */
    std::string GetFileName() const;

    /**
 * @brief Replaces the base component of a
 *        file name by keeping the subdataset information unaltered.
 *        The returned string must be freed with CPLFree()
 * @param newFileName           New file name with no subdataset information
 * @note                        This method does not check if the subdataset actually exists.
 * @return                      The original string with the old file name replaced by newFileName and the subdataset information unaltered.
 * @since                       GDAL 3.8
 */
    std::string ModifyFileName(const std::string &newFileName) const;

    /**
 * @brief Returns the subdataset component of the file name.
 *
 * @return                      The subdataset name
 * @since                       GDAL 3.8
 */
    std::string GetSubdatasetName() const;

    //! @cond Doxygen_Suppress
  protected:
    /**
     * This method is called once to parse the fileName and populate the member variables.
     * It must be reimplemented by concrete derived classes.
     */
    virtual void parseFileName() = 0;
    mutable bool m_initialized = false;
    std::string m_fileName;
    std::string m_baseComponent;
    std::string m_subdatasetComponent;
    std::string m_driverPrefixComponent;
    //! @endcond
};

#endif  // GDALSUBDATASETINFO_H
