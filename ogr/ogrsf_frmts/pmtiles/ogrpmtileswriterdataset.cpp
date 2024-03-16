/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of PMTiles
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Planet Labs
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

#include "ogr_pmtiles.h"

#ifdef HAVE_MVT_WRITE_SUPPORT

#include "mvtutils.h"
#include "ogrpmtilesfrommbtiles.h"

/************************************************************************/
/*                     ~OGRPMTilesWriterDataset()                       */
/************************************************************************/

OGRPMTilesWriterDataset::~OGRPMTilesWriterDataset()
{
    OGRPMTilesWriterDataset::Close();
}

/************************************************************************/
/*                             Close()                                  */
/************************************************************************/

CPLErr OGRPMTilesWriterDataset::Close()
{
    CPLErr eErr = CE_None;
    if (nOpenFlags != OPEN_FLAGS_CLOSED)
    {
        if (m_poMBTilesWriterDataset)
        {
            if (m_poMBTilesWriterDataset->Close() != CE_None)
            {
                eErr = CE_Failure;
            }
            else
            {
                if (!OGRPMTilesConvertFromMBTiles(
                        GetDescription(),
                        m_poMBTilesWriterDataset->GetDescription()))
                {
                    eErr = CE_Failure;
                }
            }

            VSIUnlink(m_poMBTilesWriterDataset->GetDescription());
            m_poMBTilesWriterDataset.reset();
        }

        if (GDALDataset::Close() != CE_None)
            eErr = CE_Failure;
    }
    return eErr;
}

/************************************************************************/
/*                             Create()                                 */
/************************************************************************/

bool OGRPMTilesWriterDataset::Create(const char *pszFilename,
                                     CSLConstList papszOptions)
{
    SetDescription(pszFilename);
    CPLStringList aosOptions(papszOptions);
    aosOptions.SetNameValue("FORMAT", "MBTILES");

    // Let's build a temporary file that contains the tile data in
    // a way that corresponds to the "clustered" mode, that is
    // "offsets are either contiguous with the previous offset+length, or
    // refer to a lesser offset, when writing with deduplication."
    std::string osTmpFile(pszFilename);
    if (!VSIIsLocal(pszFilename))
    {
        osTmpFile = CPLGenerateTempFilename(CPLGetFilename(pszFilename));
    }
    osTmpFile += ".tmp.mbtiles";

    if (!aosOptions.FetchNameValue("NAME"))
        aosOptions.SetNameValue("NAME", CPLGetBasename(pszFilename));

    m_poMBTilesWriterDataset.reset(OGRMVTWriterDatasetCreate(
        osTmpFile.c_str(), 0, 0, 0, GDT_Unknown, aosOptions.List()));

    return m_poMBTilesWriterDataset != nullptr;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRPMTilesWriterDataset::ICreateLayer(const char *pszLayerName,
                                      const OGRGeomFieldDefn *poGeomFieldDefn,
                                      CSLConstList papszOptions)
{
    return m_poMBTilesWriterDataset->CreateLayer(pszLayerName, poGeomFieldDefn,
                                                 papszOptions);
}

/************************************************************************/
/*                            TestCapability()                          */
/************************************************************************/

int OGRPMTilesWriterDataset::TestCapability(const char *pszCap)
{
    return m_poMBTilesWriterDataset->TestCapability(pszCap);
}

#endif  // HAVE_MVT_WRITE_SUPPORT
