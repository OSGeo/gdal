/******************************************************************************
 *
 * Project:  WMS Client Driver
 * Purpose:  Implementation of Dataset and RasterBand classes for WMS
 *           and other similar services.
 * Author:   Adam Nowacki, nowak@xpam.de
 *
 ******************************************************************************
 * Copyright (c) 2007, Adam Nowacki
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

#include "stdinc.h"

CPLErr GDALWMSDataWindow::Initialize(CPLXMLNode *config) {
    CPLErr ret = CE_None;

    const char *ulx = CPLGetXMLValue(config, "UpperLeftX", "");
    const char *uly = CPLGetXMLValue(config, "UpperLeftY", "");
    const char *lrx = CPLGetXMLValue(config, "LowerRightX", "");
    const char *lry = CPLGetXMLValue(config, "LowerRightY", "");
    const char *sx = CPLGetXMLValue(config, "SizeX", "");
    const char *sy = CPLGetXMLValue(config, "SizeY", "");
    const char *tx = CPLGetXMLValue(config, "TileX", "0");
    const char *ty = CPLGetXMLValue(config, "TileY", "0");
    const char *tlevel = CPLGetXMLValue(config, "TileLevel", "0");

    if (ret == CE_None) {
        if ((ulx[0] != '\0') && (uly[0] != '\0') && (lrx[0] != '\0') && (lry[0] != '\0') && (sx[0] != '\0') && (sy[0] != '\0')) {
            m_x0 = atof(ulx);
            m_y0 = atof(uly);
            m_x1 = atof(lrx);
            m_y1 = atof(lry);
            m_sx = atoi(sx);
            m_sy = atoi(sy);
        } else {
            ret = CE_Failure;
        }
    }
    if (ret == CE_None) {
        if ((tx[0] != '\0') && (ty[0] != '\0') && (tlevel[0] != '\0')) {
            m_tx = atoi(tx);
            m_ty = atoi(ty);
            m_tlevel = atoi(tlevel);
        }
    }

    return ret;
}
