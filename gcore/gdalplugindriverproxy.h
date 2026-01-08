/******************************************************************************
 *
 * Name:     gdalplugindriverproxy.h
 * Project:  GDAL Core
 * Purpose:  GDAL Core C++/Private declarations.
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALPLUGINDRIVERPROXY_H_INCLUDED
#define GDALPLUGINDRIVERPROXY_H_INCLUDED

#include "cpl_port.h"
#include "gdal_driver.h"

#include <set>

/************************************************************************/
/*                       GDALPluginDriverProxy                          */
/************************************************************************/

// clang-format off
/** Proxy for a plugin driver.
 *
 * Such proxy must be registered with
 * GDALDriverManager::DeclareDeferredPluginDriver().
 *
 * If the real driver defines any of the following metadata items, the
 * proxy driver should also define them with the same value:
 * <ul>
 * <li>GDAL_DMD_LONGNAME</li>
 * <li>GDAL_DMD_EXTENSIONS</li>
 * <li>GDAL_DMD_EXTENSION</li>
 * <li>GDAL_DMD_OPENOPTIONLIST</li>
 * <li>GDAL_DMD_SUBDATASETS</li>
 * <li>GDAL_DMD_CONNECTION_PREFIX</li>
 * <li>GDAL_DCAP_RASTER</li>
 * <li>GDAL_DCAP_MULTIDIM_RASTER</li>
 * <li>GDAL_DCAP_VECTOR</li>
 * <li>GDAL_DCAP_GNM</li>
 * <li>GDAL_DCAP_MULTIPLE_VECTOR_LAYERS</li>
 * <li>GDAL_DCAP_NONSPATIAL</li>
 * <li>GDAL_DCAP_VECTOR_TRANSLATE_FROM</li>
 * </ul>
 *
 * The pfnIdentify and pfnGetSubdatasetInfoFunc callbacks, if they are
 * defined in the real driver, should also be set on the proxy driver.
 *
 * Furthermore, the following metadata items must be defined if the real
 * driver sets the corresponding callback:
 * <ul>
 * <li>GDAL_DCAP_OPEN: must be set to YES if the real driver defines pfnOpen</li>
 * <li>GDAL_DCAP_CREATE: must be set to YES if the real driver defines pfnCreate</li>
 * <li>GDAL_DCAP_CREATE_MULTIDIMENSIONAL: must be set to YES if the real driver defines pfnCreateMultiDimensional</li>
 * <li>GDAL_DCAP_CREATECOPY: must be set to YES if the real driver defines pfnCreateCopy</li>
 * </ul>
 *
 * @since 3.9
 */
// clang-format on

class GDALPluginDriverProxy final : public GDALDriver
{
    const std::string m_osPluginFileName;
    std::string m_osPluginFullPath{};
    std::unique_ptr<GDALDriver> m_poRealDriver{};
    std::set<std::string> m_oSetMetadataItems{};

    GDALDriver *GetRealDriver();

    CPL_DISALLOW_COPY_ASSIGN(GDALPluginDriverProxy)

  protected:
    friend class GDALDriverManager;

    //! @cond Doxygen_Suppress
    void SetPluginFullPath(const std::string &osFullPath)
    {
        m_osPluginFullPath = osFullPath;
    }

    //! @endcond

  public:
    explicit GDALPluginDriverProxy(const std::string &osPluginFileName);

    /** Return the plugin file name (not a full path) */
    const std::string &GetPluginFileName() const
    {
        return m_osPluginFileName;
    }

    //! @cond Doxygen_Suppress
    OpenCallback GetOpenCallback() override;

    CreateCallback GetCreateCallback() override;

    CreateMultiDimensionalCallback GetCreateMultiDimensionalCallback() override;

    CreateCopyCallback GetCreateCopyCallback() override;

    DeleteCallback GetDeleteCallback() override;

    RenameCallback GetRenameCallback() override;

    CopyFilesCallback GetCopyFilesCallback() override;

    InstantiateAlgorithmCallback GetInstantiateAlgorithmCallback() override;
    //! @endcond

    CPLErr SetMetadataItem(const char *pszName, const char *pszValue,
                           const char *pszDomain = "") override;

    CSLConstList GetMetadata(const char *pszDomain) override;

    const char *GetMetadataItem(const char *pszName,
                                const char *pszDomain = "") override;
};

#endif
