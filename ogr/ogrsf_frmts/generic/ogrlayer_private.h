/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGRLayer::Private struct
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef OGRLAYER_PRIVATE_H_INCLUDED
#define OGRLAYER_PRIVATE_H_INCLUDED

#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress
struct OGRLayer::Private
{
    bool m_bInFeatureIterator = false;

    // Used by CreateFieldFromArrowSchema() and WriteArrowBatch()
    // to store the mapping between the input Arrow field name and the
    // output OGR field name, that can be different sometimes (for example
    // Shapefile truncating at 10 characters)
    // This is admittedly not super clean to store that mapping at that level.
    // We should probably have CreateFieldFromArrowSchema() and
    // WriteArrowBatch() explicitly returning and accepting that mapping.
    std::map<std::string, std::string> m_oMapArrowFieldNameToOGRFieldName{};

    //! Whether OGRLayer::ConvertGeomsIfNecessary() has already been called
    bool m_bConvertGeomsIfNecessaryAlreadyCalled = false;

    //! Value of TestCapability(OLCCurveGeometries). Only valid after ConvertGeomsIfNecessary() has been called.
    bool m_bSupportsCurve = false;

    //! Value of TestCapability(OLCMeasuredGeometries). Only valid after ConvertGeomsIfNecessary() has been called.
    bool m_bSupportsM = false;

    //! Whether OGRGeometry::SetPrecision() should be applied. Only valid after ConvertGeomsIfNecessary() has been called.
    bool m_bApplyGeomSetPrecision = false;
};

//! @endcond

#endif /* OGRLAYER_PRIVATE_H_INCLUDED */
