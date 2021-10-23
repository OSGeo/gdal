/******************************************************************************
 * Project:  WMS Client Driver
 * Purpose:  OGC API maps
 * Author:   Even Rouault
 *
 ******************************************************************************
 * Copyright (c) 2020, Even Rouault
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

#include "wmsdriver.h"
#include "minidriver_ogcapimaps.h"

CPL_CVSID("$Id$")

CPLErr WMSMiniDriver_OGCAPIMaps::Initialize(CPLXMLNode *config, CPL_UNUSED char **papszOpenOptions) {
    CPLErr ret = CE_None;

    {
        const char *base_url = CPLGetXMLValue(config, "ServerURL", "");
        if (base_url[0] != '\0') {
            m_base_url = base_url;
        } else {
            CPLError(CE_Failure, CPLE_AppDefined, "GDALWMS, OGCAPIMaps mini-driver: ServerURL missing.");
            ret = CE_Failure;
        }
    }

    return ret;
}

CPLErr WMSMiniDriver_OGCAPIMaps::TiledImageRequest(WMSHTTPRequest &request,
                                            const GDALWMSImageRequestInfo &iri,
                                            const GDALWMSTiledImageRequestInfo &)
{
    CPLString &url = request.URL;

    url = m_base_url;

    URLPrepare(url);
    url += CPLOPrintf("width=%d&height=%d&bbox=%.18g,%.18g,%.18g,%.18g",
                        iri.m_sx,
                        iri.m_sy,
                        iri.m_x0,
                        iri.m_y1,
                        iri.m_x1,
                        iri.m_y0);

    return CE_None;
}
