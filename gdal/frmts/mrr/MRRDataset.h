/*****************************************************************************
* Copyright 2016, 2020 Precisely.
*
* Licensed under the MIT License (the “License”); you may not use this file
* except in the compliance with the License.
* You may obtain a copy of the License at https://opensource.org/licenses/MIT

* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an “AS IS” WITHOUT
* WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*****************************************************************************/

#include "gdal_pam.h"
#include "APIDef.h"

class MRRDataset : public GDALPamDataset
{
    friend class MRRRasterBand;
    uint32_t                nDatasetHandle;
    uint32_t                nInfoHandle;
    uint32_t                nXBlocksCount;                //block count in X direction
    uint32_t                nYBlocksCount;                //block count in Y direction
    int64_t                    nCellAtGridOriginX;            //Cell Offset in X direction
    int64_t                    nCellAtGridOriginY;            //Cell Offset in Y direction
    double                    dCellSizeX, dCellSizeY, dOriginX, dOriginY;
    char*                    pszProjection;
    bool                    bCategoriesInitialized;
    char**                    pszCategories;
    GDALColorTable*            pColorTable;


    MRRDataset() {}
    const uint32_t&            GetDSHandle() const { return nDatasetHandle; }
    const uint32_t&            GetInfoHandle() const { return nInfoHandle; }
    const unsigned int &    GetXBlocks() { return nXBlocksCount; }
    const unsigned int &    GetYBlocks() { return nYBlocksCount; }
    void                    PopulateColorTable(const uint32_t& nFieldIndex);
    void                    PopulateCategories(const uint32_t& nFieldIndex);
    GDALColorTable*            GetColorTable() const { return pColorTable; }
    char**                    GetCategoryNames(const uint32_t& nField);

public:
    MRRDataset(const uint32_t& nDatasetHandle, const uint32_t& nInfoHandle);
    ~MRRDataset();

    CPLErr                    GetGeoTransform(double * padfTransform) override;
    const char *            GetProjectionRef();

    //Static methods
    static GDALDataset*        OpenMRR(GDALOpenInfo *);
    static int                IdentifyMRR(GDALOpenInfo *);
};


