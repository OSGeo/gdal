/******************************************************************************
 *
 * Project:  HEIF read-only Driver
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

#include "heifdataset.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "boxes/cmpd.h"
#include "boxes/ftyp.h"
#include "boxes/hdlr.h"
#include "boxes/iinf.h"
#include "boxes/iloc.h"
#include "boxes/iprp.h"
#include "boxes/ispe.h"
#include "boxes/mdat.h"
#include "boxes/meta.h"
#include "boxes/pitm.h"
#include "boxes/uncc.h"
#include "cpl_error.h"

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *GDALHEIFDataset::CreateCopy(const char *pszFilename,
                                         GDALDataset *poSrcDS, int,
                                         CPL_UNUSED char **papszOptions,
                                         GDALProgressFunc pfnProgress,
                                         void *pProgressData)
{
    if (pfnProgress == nullptr)
        pfnProgress = GDALDummyProgress;

    int nBands = poSrcDS->GetRasterCount();
    if (nBands == 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Driver does not support source dataset with zero band.\n");
        return nullptr;
    }

    // TODO: sanity checks

    VSILFILE *fp = VSIFOpenL(pszFilename, "w+b");

    if (fp == nullptr)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Attempt to create file '%s' failed.\n", pszFilename);
        return nullptr;
    }

    GInt32 nXSize = poSrcDS->GetRasterXSize();
    GInt32 nYSize = poSrcDS->GetRasterYSize();

    // TODO: maybe per-band extents
    MediaDataBox mdat;
    // TODO: don't hard code the per-pixel component data size.
    size_t bandSize = nXSize * nYSize;
    size_t size = bandSize * nBands;
    std::shared_ptr<std::vector<uint8_t>> data =
        std::make_shared<std::vector<uint8_t>>();

    uint8_t *bandData =
        (uint8_t *)VSI_MALLOC2_VERBOSE(bandSize, sizeof(uint8_t));
    if (bandData == nullptr)
    {
        VSIFCloseL(fp);
        return nullptr;
    }

    // TODO: iterator form
    for (int bandIndex = 0; bandIndex < nBands; bandIndex++)
    {
        GDALRasterBand *band = poSrcDS->GetRasterBand(bandIndex + 1);
        // TODO: we can probably handle line and pixel padding here.
        auto eErr = band->RasterIO(GF_Read, 0, 0, nXSize, nYSize, bandData,
                                   nXSize, nYSize, GDT_Byte, 0, 0, nullptr);

        if (eErr != CE_None)
        {
            VSIFCloseL(fp);
            VSIFree(bandData);
            return nullptr;
        }

        data->insert(data->begin() + bandIndex * bandSize, bandData,
                     bandData + bandSize);

        if (!pfnProgress(static_cast<double>(bandIndex / nBands), nullptr,
                         pProgressData))
        {
            VSIFCloseL(fp);
            VSIFree(bandData);
            CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
            return nullptr;
        }
    }

    VSIFree(bandData);
    mdat.setData(data);

    std::shared_ptr<ItemLocationBox::Item::Extent> extent =
        std::make_shared<ItemLocationBox::Item::Extent>();
    extent->index = 0;
    extent->offset = 0;
    extent->length = size;

    FileTypeBox ftyp;
    ftyp.setMajorBrand(fourcc("mif1"));
    ftyp.addCompatibleBrand(fourcc("mif1"));
    ftyp.addCompatibleBrand(fourcc("heic"));
    ftyp.addCompatibleBrand(fourcc("unif"));
    ftyp.writeTo(fp);

    MetaBox meta;
    uint32_t primaryItemId = 1;

    HandlerBox hdlr;
    hdlr.setHandlerType(fourcc("pict"));
    hdlr.setName("Picture handler");
    meta.addBox(&hdlr);

    PrimaryItemBox pitm;
    pitm.setItemID(primaryItemId);
    meta.addBox(&pitm);

    ItemInfoBox iinf;
    std::shared_ptr<ItemInfoEntry> infe =
        std::make_shared<ItemInfoEntry>(primaryItemId, "unci", "Primary Image");
    iinf.addEntry(infe);
    meta.addBox(&iinf);

    ItemLocationBox iloc;
    std::shared_ptr<ItemLocationBox::Item> locationItem =
        std::make_shared<ItemLocationBox::Item>(primaryItemId);
    locationItem->addExtent(extent);
    iloc.addItem(locationItem);
    meta.addBox(&iloc);

    ItemPropertiesBox iprp;
    std::shared_ptr<ItemPropertyContainerBox> ipco =
        std::make_shared<ItemPropertyContainerBox>();
    iprp.addChildBox(ipco);
    std::shared_ptr<ItemPropertyAssociationBox> ipma =
        std::make_shared<ItemPropertyAssociationBox>();
    iprp.addChildBox(ipma);

    std::shared_ptr<ImageSpatialExtentsProperty> ispe =
        std::make_shared<ImageSpatialExtentsProperty>(nXSize, nYSize);
    // iprp.addEssentialAssociation(ispe, primaryItemId);
    int ispeIndex = ipco->addChildBox(ispe);
    ItemPropertyAssociationBox::Entry entry(primaryItemId);
    ItemPropertyAssociationBox::Entry::Association ispeAssociation;
    ispeAssociation.essential = true;
    ispeAssociation.property_index = ispeIndex;
    entry.addAssociation(ispeAssociation);

    std::shared_ptr<ComponentDefinitionBox> cmpd =
        std::make_shared<ComponentDefinitionBox>();
    // TODO: add components based on GDAL band info.
    cmpd->addComponent(4);
    cmpd->addComponent(5);
    cmpd->addComponent(6);
    int cmpdIndex = ipco->addChildBox(cmpd);
    ItemPropertyAssociationBox::Entry::Association cmpdAssociation;
    cmpdAssociation.essential = true;
    cmpdAssociation.property_index = cmpdIndex;
    entry.addAssociation(cmpdAssociation);

    std::shared_ptr<UncompressedFrameConfigBox> uncC =
        std::make_shared<UncompressedFrameConfigBox>();
    // TODO: build from GDAL band info
    UncompressedFrameConfigBox::Component redComponent;
    redComponent.component_index = 0;
    redComponent.component_bit_depth_minus_one = 7;
    redComponent.component_format = 0;
    redComponent.component_align_size = 0;
    uncC->addComponent(redComponent);
    UncompressedFrameConfigBox::Component greenComponent;
    greenComponent.component_index = 1;
    greenComponent.component_bit_depth_minus_one = 7;
    greenComponent.component_format = 0;
    greenComponent.component_align_size = 0;
    uncC->addComponent(greenComponent);
    UncompressedFrameConfigBox::Component blueComponent;
    blueComponent.component_index = 2;
    blueComponent.component_bit_depth_minus_one = 7;
    blueComponent.component_format = 0;
    blueComponent.component_align_size = 0;
    uncC->addComponent(blueComponent);

    int uncCIndex = ipco->addChildBox(uncC);
    ItemPropertyAssociationBox::Entry::Association uncCAssociation;
    uncCAssociation.essential = true;
    uncCAssociation.property_index = uncCIndex;
    entry.addAssociation(uncCAssociation);

    ipma->addEntry(entry);
    meta.addBox(&iprp);

    uint64_t mdatOffset = ftyp.getFullSize() + meta.getFullSize();
    extent->offset = mdatOffset + mdat.getHeaderSize();

    meta.writeTo(fp);

    mdat.writeTo(fp);

    VSIFCloseL(fp);

    GDALPamDataset *poDS = (GDALPamDataset *)GDALOpen(pszFilename, GA_ReadOnly);
    return poDS;
}
