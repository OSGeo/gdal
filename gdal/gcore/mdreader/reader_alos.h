/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata from Alos imagery.
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

#ifndef READER_ALOS_H_INCLUDED
#define READER_ALOS_H_INCLUDED

#include "reader_pleiades.h"

/**
Metadata reader for ALOS

TIFF filename:      IMG-sssssssssssssss-pppppppp.tif or
                    IMG-01-sssssssssssssss-pppppppp.tif
                    IMG-02-sssssssssssssss-pppppppp.tif
Metadata filename:  summary.txt
RPC filename:       RPC-sssssssssssssss-pppppppp.txt

Common metadata (from metadata filename):
    AcquisitionDateTime: Img_SceneCenterDateTime or Lbi_ObservationDate
    SatelliteId:         Lbi_Satellite
    CloudCover:          Img_CloudQuantityOfAllImage
*/

class GDALMDReaderALOS: public GDALMDReaderBase
{
public:
    GDALMDReaderALOS(const char *pszPath, char **papszSiblingFiles);
    virtual ~GDALMDReaderALOS();
    virtual bool HasRequiredFiles() const;
    virtual char** GetMetadataFiles() const;
protected:
    virtual void LoadMetadata();
    char** LoadRPCTxtFile();
    virtual time_t GetAcquisitionTimeFromString(const char* pszDateTime);
protected:
    CPLString m_osIMDSourceFilename;
    CPLString m_osHDRSourceFilename;
    CPLString m_osRPBSourceFilename;
};

#endif // READER_ALOS_H_INCLUDED

