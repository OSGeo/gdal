/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata from Spot imagery.
 * Author:   Alexander Lisovenko
 * Author:   Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) 2014-2015 NextGIS <info@nextgis.ru>
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

#ifndef READER_SPOT_H_INCLUDED
#define READER_SPOT_H_INCLUDED

#include "reader_pleiades.h"

/**
@brief Metadata reader for Spot

TIFF filename:      aaaaaaaaaa.tif
Metadata filename:  METADATA.DIM
RPC filename:

Common metadata (from metadata filename):
    SatelliteId:         MISSION, MISSION_INDEX
    AcquisitionDateTime: IMAGING_DATE, IMAGING_TIME
*/

class GDALMDReaderSpot: public GDALMDReaderPleiades
{
public:
    GDALMDReaderSpot(const char *pszPath, char **papszSiblingFiles);
    virtual ~GDALMDReaderSpot();
protected:
    virtual void LoadMetadata();
    virtual char** ReadXMLToList(CPLXMLNode* psNode, char** papszList,
                                 const char* pszName = "");
};

#endif // READER_SPOT_H_INCLUDED

