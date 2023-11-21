/******************************************************************************
 * Name:     gdalmultidim_priv.h
 * Project:  GDAL Core
 * Purpose:  GDAL private header for multidimensional support
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even.rouault at spatialys.com>
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

#ifndef GDALMULTIDIM_PRIV_INCLUDED
#define GDALMULTIDIM_PRIV_INCLUDED

#include "gdal_priv.h"

//! @cond Doxygen_Suppress

// For C API

struct GDALExtendedDataTypeHS
{
    std::unique_ptr<GDALExtendedDataType> m_poImpl;

    explicit GDALExtendedDataTypeHS(GDALExtendedDataType *dt) : m_poImpl(dt)
    {
    }
};

struct GDALEDTComponentHS
{
    std::unique_ptr<GDALEDTComponent> m_poImpl;

    explicit GDALEDTComponentHS(const GDALEDTComponent &component)
        : m_poImpl(new GDALEDTComponent(component))
    {
    }
};

struct GDALGroupHS
{
    std::shared_ptr<GDALGroup> m_poImpl;

    explicit GDALGroupHS(const std::shared_ptr<GDALGroup> &poGroup)
        : m_poImpl(poGroup)
    {
    }
};

struct GDALMDArrayHS
{
    std::shared_ptr<GDALMDArray> m_poImpl;

    explicit GDALMDArrayHS(const std::shared_ptr<GDALMDArray> &poArray)
        : m_poImpl(poArray)
    {
    }
};

struct GDALAttributeHS
{
    std::shared_ptr<GDALAttribute> m_poImpl;

    explicit GDALAttributeHS(const std::shared_ptr<GDALAttribute> &poAttr)
        : m_poImpl(poAttr)
    {
    }
};

struct GDALDimensionHS
{
    std::shared_ptr<GDALDimension> m_poImpl;

    explicit GDALDimensionHS(const std::shared_ptr<GDALDimension> &poDim)
        : m_poImpl(poDim)
    {
    }
};

//! @endcond

#endif  // GDALMULTIDIM_PRIV_INCLUDED
