/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata from Pleiades imagery.
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

#ifndef READER_PLEIADES_H_INCLUDED
#define READER_PLEIADES_H_INCLUDED

#include "../gdal_mdreader.h"

/**
@brief Metadata reader for Pleiades

TIFF filename:      IMG_xxxxxx.tif
Metadata filename:  DIM_xxxxxx.XML
RPC filename:       RPC_xxxxxx.XML

Common metadata (from metadata filename):
    SatelliteId:         MISSION, MISSION_INDEX
    AcquisitionDateTime: IMAGING_DATE, IMAGING_TIME

*/

class GDALMDReaderPleiades: public GDALMDReaderBase
{
public:
    GDALMDReaderPleiades(const char *pszPath, char **papszSiblingFiles);
    virtual ~GDALMDReaderPleiades();
    virtual bool HasRequiredFiles() const;
    virtual char** GetMetadataFiles() const;
protected:
    virtual void LoadMetadata();
    char** LoadRPCXmlFile();
protected:
    CPLString m_osIMDSourceFilename;
    CPLString m_osRPBSourceFilename;
};

#endif // READER_PLEIADES_H_INCLUDED
