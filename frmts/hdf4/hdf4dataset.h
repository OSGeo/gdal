/******************************************************************************
 * $Id$
 *
 * Project:  Hierarchical Data Format Release 4 (HDF4)
 * Purpose:  Header file for HDF4 datasets reader.
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2002, Andrey Kiselev <dron@ak4719.spb.edu>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef HDF4DATASET_H_INCLUDED_
#define HDF4DATASET_H_INCLUDED_

#include "hdf.h"
#include "mfhdf.h"

#include "cpl_list.h"
#include "gdal_pam.h"

typedef enum  // Types of dataset:
{
    HDF4_SDS,  // Scientific Dataset
    HDF4_GR,   // General Raster Image
    HDF4_EOS,  // HDF EOS
    HDF4_UNKNOWN
} HDF4DatasetType;

typedef enum  // Types of data products:
{
    H4ST_GDAL,            // HDF written by GDAL
    H4ST_EOS_GRID,        // HDF-EOS Grid
    H4ST_EOS_SWATH,       // HDF-EOS Swath
    H4ST_EOS_SWATH_GEOL,  // HDF-EOS Swath Geolocation Array
    H4ST_SEAWIFS_L1A,     // SeaWiFS Level-1A Data
    H4ST_SEAWIFS_L2,      // SeaWiFS Level-2 Data
    H4ST_SEAWIFS_L3,      // SeaWiFS Level-3 Standard Mapped Image
    H4ST_HYPERION_L1,     // Hyperion L1 Data Product
    H4ST_UNKNOWN
} HDF4SubdatasetType;

/************************************************************************/
/* ==================================================================== */
/*                              HDF4Dataset                             */
/* ==================================================================== */
/************************************************************************/

class HDF4Dataset CPL_NON_FINAL : public GDALPamDataset
{

  private:
    bool bIsHDFEOS;
    std::shared_ptr<GDALGroup> m_poRootGroup{};

    static char **HDF4EOSTokenizeAttrs(const char *pszString);
    static char **HDF4EOSGetObject(char **papszAttrList, char **ppszAttrName,
                                   char **ppszAttrClass, char **ppszAttrValue);

    void OpenMultiDim(const char *pszFilename, CSLConstList papszOpenOptionsIn);

  protected:
    int32 hGR;
    int32 hSD;
    int32 nImages;
    HDF4SubdatasetType iSubdatasetType;
    const char *pszSubdatasetType;

    char **papszGlobalMetadata;
    char **papszSubDatasets;

    CPLErr ReadGlobalAttributes(int32);

  public:
    static GDALDataType GetDataType(int32);
    static const char *GetDataTypeName(int32);
    static int GetDataTypeSize(int32);
    static double AnyTypeToDouble(int32, void *);
    static char **TranslateHDF4Attributes(int32, int32, char *, int32, int32,
                                          char **);
    static char **TranslateHDF4EOSAttributes(int32, int32, int32, char **);

  public:
    HDF4Dataset();
    virtual ~HDF4Dataset();

    std::shared_ptr<GDALGroup> GetRootGroup() const override
    {
        return m_poRootGroup;
    }

    virtual char **GetMetadataDomainList() override;
    virtual char **GetMetadata(const char *pszDomain = "") override;
    static GDALDataset *Open(GDALOpenInfo *);
};

char *SPrintArray(GDALDataType eDataType, const void *paDataArray, int nValues,
                  const char *pszDelimiter);

#endif /* HDF4DATASET_H_INCLUDED_ */
