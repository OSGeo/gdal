/******************************************************************************
 * $Id$
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  Header file for HDF5 datasets reader.
 * Author:   Denis Nadeau (denis.nadeau@gmail.com)
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef HDF5DATASET_H_INCLUDED_
#define HDF5DATASET_H_INCLUDED_

#include "hdf5_api.h"

#include "cpl_list.h"
#include "gdal_pam.h"

typedef struct HDF5GroupObjects
{
    char *pszName;
    char *pszPath;
    char *pszUnderscorePath;
    char *pszTemp;
    int nType;
    int nIndex;
    hsize_t nbObjs;
    int nbAttrs;
    int nRank;
    hsize_t *paDims;
    hid_t native;
    hid_t HDatatype;
    unsigned long objno[2];
    struct HDF5GroupObjects *poHparent;
    struct HDF5GroupObjects *poHchild;
} HDF5GroupObjects;

herr_t HDF5CreateGroupObjs(hid_t, const char *, void *);

hid_t HDF5GetFileDriver();
void HDF5UnloadFileDriver();

hid_t GDAL_HDF5Open(const std::string& osFilename );

#if defined(H5_VERSION_GE) // added in 1.8.7
# if !H5_VERSION_GE(1,8,13)
#ifndef _WIN32
#  define H5free_memory(x) free(x)
#else
#  define H5free_memory(x) CPL_IGNORE_RET_VAL(x)
#endif
# endif
#else
#ifndef _WIN32
#  define H5free_memory(x) free(x)
#else
#  define H5free_memory(x) CPL_IGNORE_RET_VAL(x)
#endif
#endif

// Release 1.6.3 or 1.6.4 changed the type of count in some API functions.

#if H5_VERS_MAJOR == 1 && H5_VERS_MINOR <= 6 \
       && (H5_VERS_MINOR < 6 || H5_VERS_RELEASE < 3)
#  define H5OFFSET_TYPE hssize_t
#else
#  define H5OFFSET_TYPE  hsize_t
#endif

class HDF5Dataset;
class BAGDataset;

namespace GDAL
{

/************************************************************************/
/*                         HDF5SharedResources                          */
/************************************************************************/

class HDF5SharedResources
{
    friend class ::HDF5Dataset;
    friend class ::BAGDataset;

    bool m_bReadOnly = true;
    hid_t            m_hHDF5 = 0;
    CPLString        m_osFilename{};
public:
    HDF5SharedResources() = default;
    ~HDF5SharedResources();

    const std::string& GetFilename() const { return m_osFilename; }

    inline hid_t GetHDF5() const { return m_hHDF5; }
    inline bool IsReadOnly() const { return m_bReadOnly; }
};

} // namespace GDAL

/************************************************************************/
/* ==================================================================== */
/*                              HDF5Dataset                             */
/* ==================================================================== */
/************************************************************************/
class HDF5Dataset CPL_NON_FINAL: public GDALPamDataset
{
protected:
    hid_t            hHDF5;
    hid_t            hGroupID; // H handler interface.
    char             **papszSubDatasets;
    int              bIsHDFEOS;
    int              nDatasetType;
    int              nSubDataCount;

    HDF5GroupObjects *poH5RootGroup; /* Contain hdf5 Groups information */
    std::shared_ptr<GDALGroup> m_poRootGroup{};

    CPLErr ReadGlobalAttributes(int);
    CPLErr HDF5ListGroupObjects(HDF5GroupObjects *, int );
    CPLErr CreateMetadata( HDF5GroupObjects *, int );

    HDF5GroupObjects *HDF5FindDatasetObjects( HDF5GroupObjects *, const char * );
    HDF5GroupObjects *HDF5FindDatasetObjectsbyPath( HDF5GroupObjects *, const char * );
    char *CreatePath(HDF5GroupObjects *);
    void DestroyH5Objects(HDF5GroupObjects *);

    static const char *GetDataTypeName(hid_t);

  /**
   * Reads an array of double attributes from the HDF5 metadata.
   * It reads the attributes directly on its binary form directly,
   * thus avoiding string conversions.
   *
   * Important: It allocates the memory for the attributes internally,
   * so the caller must free the returned array after using it.
   * @param pszAttrName Name of the attribute to be read.
   *                    the attribute name must be the form:
   *                                    root attribute name
   *                                    SUBDATASET/subdataset attribute name
   * @param pdfValues pointer which will store the array of doubles read.
   * @param nLen it stores the length of the array read. If NULL it doesn't inform
   *        the length of the array.
   * @return CPLErr CE_None in case of success, CE_Failure in case of failure
   */
    CPLErr HDF5ReadDoubleAttr(const char *pszAttrName, double **pdfValues,
                              int *nLen = nullptr);

  public:

    char             **papszMetadata;
    HDF5GroupObjects *poH5CurrentObject;

    HDF5Dataset();
    ~HDF5Dataset();

    std::shared_ptr<GDALGroup> GetRootGroup() const override { return m_poRootGroup; }

    static GDALDataset *Open(GDALOpenInfo *);
    static GDALDataset *OpenMultiDim(GDALOpenInfo *);
    static std::shared_ptr<GDALGroup> OpenGroup(std::shared_ptr<GDAL::HDF5SharedResources> poSharedResources);
    static int Identify(GDALOpenInfo *);

    static GDALDataType GetDataType(hid_t);
};

#endif /* HDF5DATASET_H_INCLUDED_ */
