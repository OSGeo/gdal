/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata from Resurs-DK1 imagery.
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

#ifndef READER_RDK1_H_INCLUDED
#define READER_RDK1_H_INCLUDED

#include "../gdal_mdreader.h"

/**
@brief Metadata reader for RDK1

TIFF filename:		aaaaaaaaaa.tif
Metadata filename:	aaaaaaaaaa.xml
RPC filename:

Common metadata (from metadata filename):
        MDName_SatelliteId:			cCodeKA
        MDName_AcquisitionDateTime:	dSceneDate, tSceneTime
*/

class GDALMDReaderResursDK1: public GDALMDReaderBase
{
public:
    GDALMDReaderResursDK1(const char *pszPath, char **papszSiblingFiles);
    virtual ~GDALMDReaderResursDK1();
    virtual const bool HasRequiredFiles() const;
    virtual char** GetMetadataFiles() const;
protected:
    virtual void LoadMetadata();
    virtual const time_t GetAcquisitionTimeFromString(const char* pszDateTime);
    virtual char** AddXMLNameValueToList(char** papszList, const char *pszName,
                                         const char *pszValue);
protected:
    CPLString m_osXMLSourceFilename;
};

#endif // READER_RDK1_H_INCLUDED

