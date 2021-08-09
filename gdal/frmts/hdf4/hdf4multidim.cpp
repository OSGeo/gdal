/******************************************************************************
 *
 * Project:  HDF4 read Driver
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
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

#include "hdf4dataset.h"

#include "hdf.h"
#include "mfhdf.h"

#include "HdfEosDef.h"

#include "cpl_string.h"

#include <algorithm>
#include <map>
#include <set>

extern CPLMutex *hHDF4Mutex;
extern const char * const pszGDALSignature;

/************************************************************************/
/*                         HDF4SharedResources                          */
/************************************************************************/

class HDF4SharedResources
{
    friend class ::HDF4Dataset;
    int32       m_hSD = -1;
    std::string m_osFilename;
    CPLStringList m_aosOpenOptions;
    std::shared_ptr<GDALPamMultiDim> m_poPAM{};

public:
    explicit HDF4SharedResources(const std::string& osFilename);
    ~HDF4SharedResources();

    int32       GetSDHandle() const { return m_hSD; }
    const std::string& GetFilename() const { return m_osFilename; }
    const char*        FetchOpenOption(const char* pszName, const char* pszDefault) const {
        return m_aosOpenOptions.FetchNameValueDef(pszName, pszDefault);
    }

    const std::shared_ptr<GDALPamMultiDim>& GetPAM() { return m_poPAM; }
};

/************************************************************************/
/*                               HDF4Group                              */
/************************************************************************/

class HDF4SDSGroup;

class HDF4Group final: public GDALGroup
{
    std::shared_ptr<HDF4SharedResources> m_poShared;
    std::shared_ptr<HDF4SDSGroup> m_poGDALGroup{};

public:
    HDF4Group(const std::string& osParentName,
              const std::string& osName,
              const std::shared_ptr<HDF4SharedResources>& poShared);

    std::vector<std::shared_ptr<GDALAttribute>> GetAttributes(CSLConstList papszOptions = nullptr) const override;

    std::vector<std::string> GetGroupNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALGroup> OpenGroup(const std::string& osName, CSLConstList) const override;

    std::vector<std::shared_ptr<GDALDimension>> GetDimensions(CSLConstList papszOptions = nullptr) const override;

    std::vector<std::string> GetMDArrayNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALMDArray> OpenMDArray(const std::string& osName,
                                             CSLConstList papszOptions) const override;
};

/************************************************************************/
/*                         HDF4AbstractAttribute                        */
/************************************************************************/

class HDF4AbstractAttribute: public GDALAttribute
{
    std::shared_ptr<HDF4SharedResources> m_poShared;
    std::vector<std::shared_ptr<GDALDimension>> m_dims{};
    GDALExtendedDataType m_dt = GDALExtendedDataType::Create(GDT_Unknown);
    int32 m_nValues = 0;

protected:

    bool IRead(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer) const override;

public:
    HDF4AbstractAttribute(const std::string& osParentName,
                  const std::string& osName,
                  const std::shared_ptr<HDF4SharedResources>& poShared,
                  int32 iNumType,
                  int32 nValues);

    const std::vector<std::shared_ptr<GDALDimension>>& GetDimensions() const override { return m_dims; }

    const GDALExtendedDataType &GetDataType() const override { return m_dt; }

    virtual void ReadData(void* pDstBuffer) const = 0;
};

/************************************************************************/
/*                            HDF4SwathsHandle                          */
/************************************************************************/

struct HDF4SwathsHandle
{
    int32 m_handle;

    explicit HDF4SwathsHandle(int32 handle): m_handle(handle) {}
    ~HDF4SwathsHandle() { CPLMutexHolderD(&hHDF4Mutex); SWclose(m_handle); }
};

/************************************************************************/
/*                            HDF4SwathHandle                           */
/************************************************************************/

struct HDF4SwathHandle
{
    std::shared_ptr<HDF4SwathsHandle> m_poSwathsHandle;
    int32 m_handle;

    explicit HDF4SwathHandle(
        const std::shared_ptr<HDF4SwathsHandle>& poSwathsHandle, int32 handle):
        m_poSwathsHandle(poSwathsHandle), m_handle(handle) {}
    ~HDF4SwathHandle() { CPLMutexHolderD(&hHDF4Mutex); SWdetach(m_handle); }
};

/************************************************************************/
/*                            HDF4SwathsGroup                           */
/************************************************************************/

class HDF4SwathsGroup final: public GDALGroup
{
    std::shared_ptr<HDF4SharedResources> m_poShared;
    std::shared_ptr<HDF4SwathsHandle> m_poSwathsHandle;

public:
    HDF4SwathsGroup(const std::string& osParentName,
              const std::string& osName,
              const std::shared_ptr<HDF4SharedResources>& poShared,
              const std::shared_ptr<HDF4SwathsHandle>& poSwathsHandle):
        GDALGroup(osParentName, osName),
        m_poShared(poShared),
        m_poSwathsHandle(poSwathsHandle)
    {
    }

    std::vector<std::string> GetGroupNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALGroup> OpenGroup(const std::string& osName, CSLConstList) const override;
};

/************************************************************************/
/*                            HDF4SwathGroup                            */
/************************************************************************/

class HDF4SwathGroup final: public GDALGroup
{
    std::shared_ptr<HDF4SharedResources> m_poShared;
    std::shared_ptr<HDF4SwathHandle> m_poSwathHandle;
    mutable std::vector<std::shared_ptr<GDALDimension>> m_dims{};

public:
    HDF4SwathGroup(const std::string& osParentName,
              const std::string& osName,
              const std::shared_ptr<HDF4SharedResources>& poShared,
              const std::shared_ptr<HDF4SwathHandle>& poSwathHandle):
        GDALGroup(osParentName, osName),
        m_poShared(poShared),
        m_poSwathHandle(poSwathHandle)
    {
    }

    std::vector<std::shared_ptr<GDALDimension>> GetDimensions(CSLConstList papszOptions = nullptr) const override;

    std::vector<std::shared_ptr<GDALAttribute>> GetAttributes(CSLConstList papszOptions = nullptr) const override;

    std::vector<std::string> GetGroupNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALGroup> OpenGroup(const std::string& osName, CSLConstList) const override;
};

/************************************************************************/
/*                         HDF4SwathSubGroup                            */
/************************************************************************/

class HDF4SwathSubGroup final: public GDALGroup
{
    std::shared_ptr<HDF4SharedResources> m_poShared;
    std::shared_ptr<HDF4SwathHandle> m_poSwathHandle;
    int32 m_entryType;
    std::vector<std::shared_ptr<GDALDimension>> m_groupDims{};

public:
    HDF4SwathSubGroup(const std::string& osParentName,
              const std::string& osName,
              const std::shared_ptr<HDF4SharedResources>& poShared,
              const std::shared_ptr<HDF4SwathHandle>& poSwathHandle,
              int32 entryType,
              const std::vector<std::shared_ptr<GDALDimension>>& groupDims):
        GDALGroup(osParentName, osName),
        m_poShared(poShared),
        m_poSwathHandle(poSwathHandle),
        m_entryType(entryType),
        m_groupDims(groupDims)
    {
    }

    std::vector<std::string> GetMDArrayNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALMDArray> OpenMDArray(const std::string& osName,
                                             CSLConstList papszOptions) const override;
};

/************************************************************************/
/*                            HDF4SwathArray                            */
/************************************************************************/

class HDF4SwathArray final: public GDALPamMDArray
{
    std::shared_ptr<HDF4SharedResources> m_poShared;
    std::shared_ptr<HDF4SwathHandle> m_poSwathHandle;
    std::vector<std::shared_ptr<GDALDimension>> m_dims{};
    GDALExtendedDataType m_dt = GDALExtendedDataType::Create(GDT_Unknown);
    mutable std::vector<GByte> m_abyNoData{};

protected:
    HDF4SwathArray(const std::string& osParentName,
                   const std::string& osName,
                   const std::shared_ptr<HDF4SharedResources>& poShared,
                   const std::shared_ptr<HDF4SwathHandle>& poSwathHandle,
                   const std::vector<int32>& aiDimSizes,
                   const std::string& dimNames,
                   int32 iNumType,
                   const std::vector<std::shared_ptr<GDALDimension>>& groupDims);

    bool IRead(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer) const override;

public:
    static std::shared_ptr<HDF4SwathArray> Create(const std::string& osParentName,
                   const std::string& osName,
                   const std::shared_ptr<HDF4SharedResources>& poShared,
                   const std::shared_ptr<HDF4SwathHandle>& poSwathHandle,
                   const std::vector<int32>& aiDimSizes,
                   const std::string& dimNames,
                   int32 iNumType,
                   const std::vector<std::shared_ptr<GDALDimension>>& groupDims)
    {
        auto ar(std::shared_ptr<HDF4SwathArray>(new HDF4SwathArray(
            osParentName, osName, poShared,
            poSwathHandle, aiDimSizes, dimNames,
            iNumType, groupDims)));
        ar->SetSelf(ar);
        return ar;
    }

    bool IsWritable() const override { return false; }

    const std::string& GetFilename() const override { return m_poShared->GetFilename(); }

    const std::vector<std::shared_ptr<GDALDimension>>& GetDimensions() const override { return m_dims; }

    const GDALExtendedDataType &GetDataType() const override { return m_dt; }

    std::vector<std::shared_ptr<GDALAttribute>> GetAttributes(CSLConstList papszOptions = nullptr) const override;

    const void* GetRawNoDataValue() const override;
};

/************************************************************************/
/*                            HDF4SDAttribute                             */
/************************************************************************/

class HDF4SwathAttribute final: public HDF4AbstractAttribute
{
    std::shared_ptr<HDF4SwathHandle> m_poSwathHandle;

public:
    HDF4SwathAttribute(const std::string& osParentName,
                  const std::string& osName,
                  const std::shared_ptr<HDF4SharedResources>& poShared,
                  const std::shared_ptr<HDF4SwathHandle>& poSwathHandle,
                  int32 iNumType,
                  int32 nValues):
        GDALAbstractMDArray(osParentName, osName),
        HDF4AbstractAttribute(osParentName, osName, poShared, iNumType, nValues),
        m_poSwathHandle(poSwathHandle)
    {}

    void ReadData(void* pDstBuffer) const override {
        SWreadattr( m_poSwathHandle->m_handle, GetName().c_str(), pDstBuffer);
    }
};

/************************************************************************/
/*                             HDF4GDsHandle                             */
/************************************************************************/

struct HDF4GDsHandle
{
    int32 m_handle;

    explicit HDF4GDsHandle(int32 handle): m_handle(handle) {}
    ~HDF4GDsHandle() { CPLMutexHolderD(&hHDF4Mutex); GDclose(m_handle); }
};

/************************************************************************/
/*                             HDF4GDHandle                             */
/************************************************************************/

struct HDF4GDHandle
{
    std::shared_ptr<HDF4GDsHandle> m_poGDsHandle;
    int32 m_handle;

    explicit HDF4GDHandle(
        const std::shared_ptr<HDF4GDsHandle>& poGDsHandle, int32 handle):
        m_poGDsHandle(poGDsHandle), m_handle(handle) {}
    ~HDF4GDHandle() { CPLMutexHolderD(&hHDF4Mutex); GDdetach(m_handle); }
};

/************************************************************************/
/*                          HDF4EOSGridsGroup                           */
/************************************************************************/

class HDF4EOSGridsGroup final: public GDALGroup
{
    std::shared_ptr<HDF4SharedResources> m_poShared;
    std::shared_ptr<HDF4GDsHandle> m_poGDsHandle;

public:
    HDF4EOSGridsGroup(const std::string& osParentName,
              const std::string& osName,
              const std::shared_ptr<HDF4SharedResources>& poShared,
              const std::shared_ptr<HDF4GDsHandle>& poGDsHandle):
        GDALGroup(osParentName, osName),
        m_poShared(poShared),
        m_poGDsHandle(poGDsHandle)
    {
    }

    std::vector<std::string> GetGroupNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALGroup> OpenGroup(const std::string& osName, CSLConstList) const override;
};

/************************************************************************/
/*                          HDF4EOSGridGroup                            */
/************************************************************************/

class HDF4EOSGridGroup final: public GDALGroup
{
    std::shared_ptr<HDF4SharedResources> m_poShared;
    std::shared_ptr<HDF4GDHandle> m_poGDHandle;
    mutable std::vector<std::shared_ptr<GDALDimension>> m_dims{};
    mutable std::shared_ptr<GDALMDArray> m_varX{};
    mutable std::shared_ptr<GDALMDArray> m_varY{};

public:
    HDF4EOSGridGroup(const std::string& osParentName,
              const std::string& osName,
              const std::shared_ptr<HDF4SharedResources>& poShared,
              const std::shared_ptr<HDF4GDHandle>& poGDHandle):
        GDALGroup(osParentName, osName),
        m_poShared(poShared),
        m_poGDHandle(poGDHandle)
    {
    }
    std::vector<std::shared_ptr<GDALDimension>> GetDimensions(CSLConstList papszOptions = nullptr) const override;

    std::vector<std::string> GetMDArrayNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALMDArray> OpenMDArray(const std::string& osName,
                                             CSLConstList papszOptions) const override;

    std::vector<std::string> GetGroupNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALGroup> OpenGroup(const std::string& osName, CSLConstList) const override;

    std::vector<std::shared_ptr<GDALAttribute>> GetAttributes(CSLConstList papszOptions = nullptr) const override;
};

/************************************************************************/
/*                         HDF4EOSGridSubGroup                          */
/************************************************************************/

class HDF4EOSGridSubGroup final: public GDALGroup
{
    std::shared_ptr<HDF4SharedResources> m_poShared;
    std::shared_ptr<HDF4GDHandle> m_poGDHandle;
    int32 m_entryType;
    std::vector<std::shared_ptr<GDALDimension>> m_groupDims{};

public:
    HDF4EOSGridSubGroup(const std::string& osParentName,
              const std::string& osName,
              const std::shared_ptr<HDF4SharedResources>& poShared,
              const std::shared_ptr<HDF4GDHandle>& poGDHandle,
              int32 entryType,
              const std::vector<std::shared_ptr<GDALDimension>>& groupDims):
        GDALGroup(osParentName, osName),
        m_poShared(poShared),
        m_poGDHandle(poGDHandle),
        m_entryType(entryType),
        m_groupDims(groupDims)
    {
    }

    std::vector<std::string> GetMDArrayNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALMDArray> OpenMDArray(const std::string& osName,
                                             CSLConstList papszOptions) const override;
};

/************************************************************************/
/*                          HDF4EOSGridArray                            */
/************************************************************************/

class HDF4EOSGridArray final: public GDALPamMDArray
{
    std::shared_ptr<HDF4SharedResources> m_poShared;
    std::shared_ptr<HDF4GDHandle> m_poGDHandle;
    std::vector<std::shared_ptr<GDALDimension>> m_dims{};
    GDALExtendedDataType m_dt = GDALExtendedDataType::Create(GDT_Unknown);
    mutable std::vector<GByte> m_abyNoData{};
    mutable std::string m_osUnit{};

protected:
    HDF4EOSGridArray(const std::string& osParentName,
                   const std::string& osName,
                   const std::shared_ptr<HDF4SharedResources>& poShared,
                   const std::shared_ptr<HDF4GDHandle>& poGDHandle,
                   const std::vector<int32>& aiDimSizes,
                   const std::string& dimNames,
                   int32 iNumType,
                   const std::vector<std::shared_ptr<GDALDimension>>& groupDims);
    bool IRead(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer) const override;

public:
    static std::shared_ptr<HDF4EOSGridArray> Create(const std::string& osParentName,
                   const std::string& osName,
                   const std::shared_ptr<HDF4SharedResources>& poShared,
                   const std::shared_ptr<HDF4GDHandle>& poGDHandle,
                   const std::vector<int32>& aiDimSizes,
                   const std::string& dimNames,
                   int32 iNumType,
                   const std::vector<std::shared_ptr<GDALDimension>>& groupDims)
    {
        auto ar(std::shared_ptr<HDF4EOSGridArray>(new HDF4EOSGridArray(
            osParentName, osName, poShared,
            poGDHandle, aiDimSizes, dimNames,
            iNumType, groupDims)));
        ar->SetSelf(ar);
        return ar;
    }

    bool IsWritable() const override { return false; }

    const std::string& GetFilename() const override { return m_poShared->GetFilename(); }

    const std::vector<std::shared_ptr<GDALDimension>>& GetDimensions() const override { return m_dims; }

    const GDALExtendedDataType &GetDataType() const override { return m_dt; }

    std::vector<std::shared_ptr<GDALAttribute>> GetAttributes(CSLConstList papszOptions = nullptr) const override;

    const void* GetRawNoDataValue() const override;

    double GetOffset(bool* pbHasOffset = nullptr, GDALDataType* peStorageType = nullptr) const override;

    double GetScale(bool* pbHasScale = nullptr, GDALDataType* peStorageType = nullptr) const override;

    const std::string& GetUnit() const override;

    std::shared_ptr<OGRSpatialReference> GetSpatialRef() const override;
};

/************************************************************************/
/*                      HDF4EOSGridAttribute                            */
/************************************************************************/

class HDF4EOSGridAttribute final: public HDF4AbstractAttribute
{
    std::shared_ptr<HDF4GDHandle> m_poGDHandle;

public:
    HDF4EOSGridAttribute(const std::string& osParentName,
                  const std::string& osName,
                  const std::shared_ptr<HDF4SharedResources>& poShared,
                  const std::shared_ptr<HDF4GDHandle>& poGDHandle,
                  int32 iNumType,
                  int32 nValues):
        GDALAbstractMDArray(osParentName, osName),
        HDF4AbstractAttribute(osParentName, osName, poShared, iNumType, nValues),
        m_poGDHandle(poGDHandle)
    {}

    void ReadData(void* pDstBuffer) const override {
        GDreadattr( m_poGDHandle->m_handle, GetName().c_str(), pDstBuffer);
    }
};

/************************************************************************/
/*                             HDF4SDSGroup                             */
/************************************************************************/

class HDF4SDSGroup final: public GDALGroup
{
    std::shared_ptr<HDF4SharedResources> m_poShared;
    mutable std::map<std::string, int> m_oMapNameToSDSIdx{};
    mutable std::vector<std::shared_ptr<GDALDimension>> m_dims{};
    mutable std::vector<std::shared_ptr<GDALMDArray>> m_oSetIndexingVariables{};
    mutable bool m_bInGetDimensions = false;
    bool m_bIsGDALDataset = false;
    std::vector<std::shared_ptr<GDALAttribute>> m_oGlobalAttributes{};
    mutable std::shared_ptr<GDALMDArray> m_varX{};
    mutable std::shared_ptr<GDALMDArray> m_varY{};

public:
    HDF4SDSGroup(const std::string& osParentName,
              const std::string& osName,
              const std::shared_ptr<HDF4SharedResources>& poShared):
        GDALGroup(osParentName, osName),
        m_poShared(poShared)
    {
    }

    void SetIsGDALDataset() { m_bIsGDALDataset = true; }
    void SetGlobalAttributes(const std::vector<std::shared_ptr<GDALAttribute>>& attrs) { m_oGlobalAttributes = attrs; }

    std::vector<std::shared_ptr<GDALDimension>> GetDimensions(CSLConstList papszOptions = nullptr) const override;

    std::vector<std::string> GetMDArrayNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALMDArray> OpenMDArray(const std::string& osName,
                                             CSLConstList papszOptions) const override;
};

/************************************************************************/
/*                            HDF4SDSArray                              */
/************************************************************************/

class HDF4SDSArray final: public GDALPamMDArray
{
    std::shared_ptr<HDF4SharedResources> m_poShared;
    int32 m_iSDS;
    std::vector<std::shared_ptr<GDALDimension>> m_dims{};
    GDALExtendedDataType m_dt = GDALExtendedDataType::Create(GDT_Unknown);
    int32 m_nAttributes;
    mutable std::string m_osUnit{};
    std::vector<std::shared_ptr<GDALAttribute>> m_oGlobalAttributes{};
    bool m_bIsGDALDataset;
    mutable std::vector<GByte> m_abyNoData{};

protected:
    HDF4SDSArray(const std::string& osParentName,
                   const std::string& osName,
                   const std::shared_ptr<HDF4SharedResources>& poShared,
                   int32 iSDS,
                   const std::vector<int32>& aiDimSizes,
                   const std::vector<std::shared_ptr<GDALDimension>>& groupDims,
                   int32 iNumType,
                   int32 nAttrs,
                   bool bIsGDALDS);

    bool IRead(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer) const override;

public:
    static std::shared_ptr<HDF4SDSArray> Create(const std::string& osParentName,
                   const std::string& osName,
                   const std::shared_ptr<HDF4SharedResources>& poShared,
                   int32 iSDS,
                   const std::vector<int32>& aiDimSizes,
                   const std::vector<std::shared_ptr<GDALDimension>>& groupDims,
                   int32 iNumType,
                   int32 nAttrs,
                   bool bIsGDALDS)
    {
        auto ar(std::shared_ptr<HDF4SDSArray>(new HDF4SDSArray(
            osParentName, osName, poShared,
            iSDS, aiDimSizes, groupDims,
            iNumType, nAttrs, bIsGDALDS)));
        ar->SetSelf(ar);
        return ar;
    }

    ~HDF4SDSArray();

    void SetGlobalAttributes(const std::vector<std::shared_ptr<GDALAttribute>>& attrs) { m_oGlobalAttributes = attrs; }

    bool IsWritable() const override { return false; }

    const std::string& GetFilename() const override { return m_poShared->GetFilename(); }

    const std::vector<std::shared_ptr<GDALDimension>>& GetDimensions() const override { return m_dims; }

    const GDALExtendedDataType &GetDataType() const override { return m_dt; }

    std::vector<std::shared_ptr<GDALAttribute>> GetAttributes(CSLConstList papszOptions = nullptr) const override;

    const void* GetRawNoDataValue() const override;

    double GetOffset(bool* pbHasOffset = nullptr, GDALDataType* peStorageType = nullptr) const override;

    double GetScale(bool* pbHasScale = nullptr, GDALDataType* peStorageType = nullptr) const override;

    const std::string& GetUnit() const override;

    std::shared_ptr<OGRSpatialReference> GetSpatialRef() const override;
};

/************************************************************************/
/*                            HDF4GRsHandle                              */
/************************************************************************/

struct HDF4GRsHandle
{
    int32 m_hHandle;
    int32 m_grHandle;

    explicit HDF4GRsHandle(int32 hHandle, int32 grHandle): m_hHandle(hHandle), m_grHandle(grHandle) {}
    ~HDF4GRsHandle() { CPLMutexHolderD(&hHDF4Mutex); GRend(m_grHandle); Hclose(m_hHandle); }
};

/************************************************************************/
/*                             HDF4GRHandle                             */
/************************************************************************/

struct HDF4GRHandle
{
    std::shared_ptr<HDF4GRsHandle> m_poGRsHandle;
    int32 m_iGR;

    explicit HDF4GRHandle(
        const std::shared_ptr<HDF4GRsHandle>& poGRsHandle, int32 iGR):
        m_poGRsHandle(poGRsHandle), m_iGR(iGR) {}
    ~HDF4GRHandle() { CPLMutexHolderD(&hHDF4Mutex); GRendaccess( m_iGR ); }
};

/************************************************************************/
/*                            HDF4GRsGroup                              */
/************************************************************************/

class HDF4GRsGroup final: public GDALGroup
{
    std::shared_ptr<HDF4SharedResources> m_poShared;
    std::shared_ptr<HDF4GRsHandle> m_poGRsHandle;
    mutable std::map<std::string, int> m_oMapNameToGRIdx{};

public:
    HDF4GRsGroup(const std::string& osParentName,
              const std::string& osName,
              const std::shared_ptr<HDF4SharedResources>& poShared,
              const std::shared_ptr<HDF4GRsHandle>& poGRsHandle):
        GDALGroup(osParentName, osName),
        m_poShared(poShared),
        m_poGRsHandle(poGRsHandle)
    {
    }

    std::vector<std::string> GetMDArrayNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALMDArray> OpenMDArray(const std::string& osName,
                                             CSLConstList papszOptions) const override;

    std::vector<std::shared_ptr<GDALAttribute>> GetAttributes(CSLConstList papszOptions = nullptr) const override;

};

/************************************************************************/
/*                            HDF4GRArray                               */
/************************************************************************/

class HDF4GRArray final: public GDALPamMDArray
{
    std::shared_ptr<HDF4SharedResources> m_poShared;
    std::shared_ptr<HDF4GRHandle> m_poGRHandle;
    std::vector<std::shared_ptr<GDALDimension>> m_dims{};
    GDALExtendedDataType m_dt = GDALExtendedDataType::Create(GDT_Unknown);
    int32 m_nAttributes;

protected:
    HDF4GRArray(const std::string& osParentName,
                   const std::string& osName,
                   const std::shared_ptr<HDF4SharedResources>& poShared,
                   const std::shared_ptr<HDF4GRHandle>& poGRHandle,
                   int32 nBands,
                   const std::vector<int32>& aiDimSizes,
                   int32 iNumType,
                   int32 nAttrs);

    bool IRead(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer) const override;

public:
    static std::shared_ptr<HDF4GRArray> Create(const std::string& osParentName,
                   const std::string& osName,
                   const std::shared_ptr<HDF4SharedResources>& poShared,
                   const std::shared_ptr<HDF4GRHandle>& poGRHandle,
                   int32 nBands,
                   const std::vector<int32>& aiDimSizes,
                   int32 iNumType,
                   int32 nAttrs)
    {
        auto ar(std::shared_ptr<HDF4GRArray>(new HDF4GRArray(
            osParentName, osName, poShared,
            poGRHandle, nBands, aiDimSizes,
            iNumType, nAttrs)));
        ar->SetSelf(ar);
        return ar;
    }

    bool IsWritable() const override { return false; }

    const std::string& GetFilename() const override { return m_poShared->GetFilename(); }

    const std::vector<std::shared_ptr<GDALDimension>>& GetDimensions() const override { return m_dims; }

    const GDALExtendedDataType &GetDataType() const override { return m_dt; }

    std::vector<std::shared_ptr<GDALAttribute>> GetAttributes(CSLConstList papszOptions = nullptr) const override;
};

/************************************************************************/
/*                            HDF4SDAttribute                           */
/************************************************************************/

class HDF4SDAttribute final: public HDF4AbstractAttribute
{
    std::shared_ptr<HDF4SwathHandle> m_poSwathHandle;
    std::shared_ptr<HDF4GDHandle> m_poGDHandle;
    int32 m_sdHandle = 0;
    int32 m_iAttribute = 0;

public:
    HDF4SDAttribute(const std::string& osParentName,
                  const std::string& osName,
                  const std::shared_ptr<HDF4SharedResources>& poShared,
                  const std::shared_ptr<HDF4SwathHandle>& poSwathHandle,
                  const std::shared_ptr<HDF4GDHandle>& poGDHandle,
                  int32 sdHandle,
                  int32 iAttribute,
                  int32 iNumType,
                  int32 nValues):
        GDALAbstractMDArray(osParentName, osName),
        HDF4AbstractAttribute(osParentName, osName, poShared, iNumType, nValues),
        m_poSwathHandle(poSwathHandle),
        m_poGDHandle(poGDHandle),
        m_sdHandle(sdHandle),
        m_iAttribute(iAttribute)
    {}

    void ReadData(void* pDstBuffer) const override {
        SDreadattr( m_sdHandle, m_iAttribute, pDstBuffer);
    }
};

/************************************************************************/
/*                           HDF4GRAttribute                            */
/************************************************************************/

class HDF4GRAttribute final: public HDF4AbstractAttribute
{
    std::shared_ptr<HDF4GRsHandle> m_poGRsHandle;
    std::shared_ptr<HDF4GRHandle> m_poGRHandle;
    int32 m_grHandle = 0;
    int32 m_iAttribute = 0;

public:
    HDF4GRAttribute(const std::string& osParentName,
                  const std::string& osName,
                  const std::shared_ptr<HDF4SharedResources>& poShared,
                  const std::shared_ptr<HDF4GRsHandle>& poGRsHandle,
                  const std::shared_ptr<HDF4GRHandle>& poGRHandle,
                  int32 iGRHandle,
                  int32 iAttribute,
                  int32 iNumType,
                  int32 nValues):
        GDALAbstractMDArray(osParentName, osName),
        HDF4AbstractAttribute(osParentName, osName, poShared, iNumType, nValues),
        m_poGRsHandle(poGRsHandle),
        m_poGRHandle(poGRHandle),
        m_grHandle(iGRHandle),
        m_iAttribute(iAttribute)
    {}

    void ReadData(void* pDstBuffer) const override {
        GRgetattr( m_grHandle, m_iAttribute, pDstBuffer);
    }
};

/************************************************************************/
/*                         HDF4GRPalette                                */
/************************************************************************/

class HDF4GRPalette final: public GDALAttribute
{
    std::shared_ptr<HDF4SharedResources> m_poShared;
    std::shared_ptr<HDF4GRHandle> m_poGRHandle;
    std::vector<std::shared_ptr<GDALDimension>> m_dims{};
    GDALExtendedDataType m_dt = GDALExtendedDataType::Create(GDT_Byte);
    int32 m_iPal = 0;
    int32 m_nValues = 0;

protected:

    bool IRead(const GUInt64* arrayStartIdx,
                      const size_t* count,
                      const GInt64* arrayStep,
                      const GPtrDiff_t* bufferStride,
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer) const override;

public:
    HDF4GRPalette(const std::string& osParentName,
                  const std::string& osName,
                  const std::shared_ptr<HDF4SharedResources>& poShared,
                  const std::shared_ptr<HDF4GRHandle>& poGRHandle,
                  int32 iPal,
                  int32 nValues);

    const std::vector<std::shared_ptr<GDALDimension>>& GetDimensions() const override { return m_dims; }

    const GDALExtendedDataType &GetDataType() const override { return m_dt; }
};

/************************************************************************/
/*                        HDF4SharedResources()                         */
/************************************************************************/

HDF4SharedResources::HDF4SharedResources(const std::string& osFilename):
    m_osFilename(osFilename),
    m_poPAM(std::make_shared<GDALPamMultiDim>(osFilename))
{
}

/************************************************************************/
/*                        ~HDF4SharedResources()                        */
/************************************************************************/

HDF4SharedResources::~HDF4SharedResources()
{
    CPLMutexHolderD(&hHDF4Mutex);

    if ( m_hSD )
        SDend( m_hSD );
}

/************************************************************************/
/*                               HDF4Group()                            */
/************************************************************************/

HDF4Group::HDF4Group(const std::string& osParentName,
              const std::string& osName,
              const std::shared_ptr<HDF4SharedResources>& poShared):
        GDALGroup(osParentName, osName),
        m_poShared(poShared)
{
    bool bIsGDALDS = false;
    auto poAttr = GetAttribute("Signature");
    if( poAttr && poAttr->GetDataType().GetClass() == GEDTC_STRING )
    {
        const char* pszVal = poAttr->ReadAsString();
        if( pszVal && EQUAL(pszVal, pszGDALSignature) )
        {
            bIsGDALDS = true;
        }
    }
    if( bIsGDALDS )
    {
        m_poGDALGroup = std::make_shared<HDF4SDSGroup>(std::string(), "/", m_poShared);
        m_poGDALGroup->SetIsGDALDataset();
        m_poGDALGroup->SetGlobalAttributes(GetAttributes(nullptr));
    }
}

/************************************************************************/
/*                           GetAttributes()                            */
/************************************************************************/

std::vector<std::shared_ptr<GDALAttribute>> HDF4Group::GetAttributes(
                                            CSLConstList) const
{
    CPLMutexHolderD(&hHDF4Mutex);
    std::vector<std::shared_ptr<GDALAttribute>> ret;
    int32 nDatasets = 0;
    int32 nAttributes = 0;
    if ( SDfileinfo( m_poShared->GetSDHandle(), &nDatasets, &nAttributes ) != 0 )
        return ret;

    std::map<CPLString, std::shared_ptr<GDALAttribute>> oMapAttrs;
    const auto AddAttribute = [&ret, &oMapAttrs](const std::shared_ptr<GDALAttribute>& poNewAttr)
    {
        auto oIter = oMapAttrs.find(poNewAttr->GetName());
        if( oIter != oMapAttrs.end() )
        {
            const char* pszOldVal = oIter->second->ReadAsString();
            const char* pszNewVal = poNewAttr->ReadAsString();
            // As found in MOD35_L2.A2017161.1525.061.2017315035809.hdf
            // product of https://github.com/OSGeo/gdal/issues/2848,
            // the identifier_product_doi attribute is found in a
            // HDF4EOS attribute bundle, as well as a standalone attribute
            if( pszOldVal && pszNewVal && strcmp(pszOldVal, pszNewVal) == 0 )
                return;
            // TODO
            CPLDebug("HDF4",
                     "Attribute with same name (%s) found, but different value",
                     poNewAttr->GetName().c_str());
        }
        // cppcheck-suppress unreadVariable
        oMapAttrs[poNewAttr->GetName()] = poNewAttr;
        ret.emplace_back(poNewAttr);
    };

    for( int32 iAttribute = 0; iAttribute < nAttributes; iAttribute++ )
    {
        int32 iNumType = 0;
        int32 nValues = 0;

        std::string osAttrName;
        osAttrName.resize(H4_MAX_NC_NAME);
        SDattrinfo( m_poShared->GetSDHandle(), iAttribute, &osAttrName[0], &iNumType, &nValues );
        osAttrName.resize(strlen(osAttrName.c_str()));

        if ( STARTS_WITH_CI(osAttrName.c_str(), "coremetadata")    ||
             STARTS_WITH_CI(osAttrName.c_str(), "archivemetadata.") ||
             STARTS_WITH_CI(osAttrName.c_str(), "productmetadata.") ||
             STARTS_WITH_CI(osAttrName.c_str(), "badpixelinformation") ||
             STARTS_WITH_CI(osAttrName.c_str(), "product_summary") ||
             STARTS_WITH_CI(osAttrName.c_str(), "dem_specific") ||
             STARTS_WITH_CI(osAttrName.c_str(), "bts_specific") ||
             STARTS_WITH_CI(osAttrName.c_str(), "etse_specific") ||
             STARTS_WITH_CI(osAttrName.c_str(), "dst_specific") ||
             STARTS_WITH_CI(osAttrName.c_str(), "acv_specific") ||
             STARTS_WITH_CI(osAttrName.c_str(), "act_specific") ||
             STARTS_WITH_CI(osAttrName.c_str(), "etst_specific") ||
             STARTS_WITH_CI(osAttrName.c_str(), "level_1_carryover") )
        {
            char** papszMD = HDF4Dataset::TranslateHDF4EOSAttributes(
                    m_poShared->GetSDHandle(), iAttribute, nValues, nullptr );
            for( char** iter = papszMD; iter && *iter; ++iter )
            {
                char* pszKey = nullptr;
                const char* pszValue = CPLParseNameValue(*iter, &pszKey);
                if( pszKey && pszValue )
                {
                    AddAttribute(std::make_shared<GDALAttributeString>(
                                                            GetFullName(),
                                                            pszKey,
                                                            pszValue));
                }
                CPLFree(pszKey);
            }
            CSLDestroy(papszMD);
        }

        // Skip "StructMetadata.N" records. We will fetch information
        // from them using HDF-EOS API
        else if ( STARTS_WITH_CI(osAttrName.c_str(), "structmetadata.") )
        {
            continue;
        }
        else
        {
            AddAttribute(std::make_shared<HDF4SDAttribute>(GetFullName(),
                                                             osAttrName,
                                                             m_poShared,
                                                             nullptr,
                                                             nullptr,
                                                             m_poShared->GetSDHandle(),
                                                             iAttribute,
                                                             iNumType,
                                                             nValues));
        }
    }
    return ret;
}

/************************************************************************/
/*                            GetGroupNames()                           */
/************************************************************************/

std::vector<std::string> HDF4Group::GetGroupNames(CSLConstList) const
{
    if( m_poGDALGroup )
        return {};

    CPLMutexHolderD(&hHDF4Mutex);
    std::vector<std::string> res;
    auto sw_handle = SWopen( m_poShared->GetFilename().c_str(), DFACC_READ );
    if( sw_handle >= 0 )
    {
        int32 nStrBufSize = 0;
        int32 nSubDatasets =
            SWinqswath(m_poShared->GetFilename().c_str(), nullptr, &nStrBufSize);
        if( nSubDatasets > 0 )
        {
            res.emplace_back("swaths");
        }
        SWclose( sw_handle );
    }

    auto gd_handle = GDopen( m_poShared->GetFilename().c_str(), DFACC_READ );
    if( gd_handle >= 0 )
    {
        int32 nStrBufSize = 0;
        int32 nSubDatasets =
            GDinqgrid(m_poShared->GetFilename().c_str(), nullptr, &nStrBufSize);
        if( nSubDatasets > 0 )
        {
            res.emplace_back("eos_grids");
        }
        GDclose( gd_handle );
    }

    const char* pszListSDS =
        m_poShared->FetchOpenOption("LIST_SDS", "AUTO");
    if( (res.empty() && EQUAL(pszListSDS, "AUTO")) ||
        (!EQUAL(pszListSDS, "AUTO") && CPLTestBool(pszListSDS)) )
    {
        int32 nDatasets = 0;
        int32 nAttrs = 0;
        if ( SDfileinfo( m_poShared->GetSDHandle(), &nDatasets, &nAttrs ) == 0 &&
             nDatasets > 0 )
        {
            res.emplace_back("scientific_datasets");
        }
    }

    auto hHandle = Hopen( m_poShared->GetFilename().c_str(), DFACC_READ, 0 );
    if( hHandle >= 0 )
    {
        auto grHandle = GRstart( hHandle );
        if( grHandle >= 0 )
        {
            int32 nImages = 0;
            int32 nAttrs = 0;
            if( GRfileinfo( grHandle, &nImages, &nAttrs ) == 0 &&
                nImages > 0 )
            {
                res.emplace_back("general_rasters");
            }
            GRend( grHandle );
        }
        Hclose( hHandle );
    }

    return res;
}

/************************************************************************/
/*                             OpenGroup()                              */
/************************************************************************/

std::shared_ptr<GDALGroup> HDF4Group::OpenGroup(const std::string& osName, CSLConstList) const
{
    if( m_poGDALGroup )
        return nullptr;

    CPLMutexHolderD(&hHDF4Mutex);
    if( osName == "swaths" )
    {
        auto handle = SWopen( m_poShared->GetFilename().c_str(), DFACC_READ );
        if( handle >= 0 )
            return std::make_shared<HDF4SwathsGroup>(
                GetFullName(), osName, m_poShared,
                std::make_shared<HDF4SwathsHandle>(handle));
    }
    if( osName == "eos_grids" )
    {
        auto handle = GDopen( m_poShared->GetFilename().c_str(), DFACC_READ );
        if( handle >= 0 )
            return std::make_shared<HDF4EOSGridsGroup>(
                GetFullName(), osName, m_poShared,
                std::make_shared<HDF4GDsHandle>(handle));
    }
    if( osName == "scientific_datasets" )
    {
        return std::make_shared<HDF4SDSGroup>(GetFullName(), osName, m_poShared);
    }
    if( osName == "general_rasters" )
    {
        auto hHandle = Hopen( m_poShared->GetFilename().c_str(), DFACC_READ, 0 );
        if( hHandle >= 0 )
        {
            auto grHandle = GRstart( hHandle );
            if( grHandle >= 0 )
            {
                return std::make_shared<HDF4GRsGroup>(
                    GetFullName(), osName, m_poShared,
                    std::make_shared<HDF4GRsHandle>(hHandle, grHandle));
            }
            else
            {
                Hclose(hHandle);
            }
        }
    }
    return nullptr;
}

/************************************************************************/
/*                         GetMDArrayNames()                            */
/************************************************************************/

std::vector<std::string> HDF4Group::GetMDArrayNames(CSLConstList) const
{
    if( m_poGDALGroup )
        return m_poGDALGroup->GetMDArrayNames(nullptr);
    return {};
}

/************************************************************************/
/*                           OpenMDArray()                              */
/************************************************************************/

std::shared_ptr<GDALMDArray> HDF4Group::OpenMDArray(
                                const std::string& osName, CSLConstList) const
{
    if( m_poGDALGroup )
        return m_poGDALGroup->OpenMDArray(osName, nullptr);
    return nullptr;
}

/************************************************************************/
/*                            GetDimensions()                           */
/************************************************************************/

std::vector<std::shared_ptr<GDALDimension>> HDF4Group::GetDimensions(CSLConstList) const
{
    if( m_poGDALGroup )
        return m_poGDALGroup->GetDimensions(nullptr);
    return {};
}

/************************************************************************/
/*                            GetGroupNames()                           */
/************************************************************************/

std::vector<std::string> HDF4SwathsGroup::GetGroupNames(CSLConstList) const
{
    CPLMutexHolderD(&hHDF4Mutex);
    std::vector<std::string> res;

    int32 nStrBufSize = 0;
    SWinqswath( m_poShared->GetFilename().c_str(), nullptr, &nStrBufSize );

    std::string osSwathList;
    osSwathList.resize(nStrBufSize);
    SWinqswath( m_poShared->GetFilename().c_str(), &osSwathList[0], &nStrBufSize );

    CPLStringList aosSwaths(
            CSLTokenizeString2( osSwathList.c_str(), ",", CSLT_HONOURSTRINGS ));
    for( int i = 0; i < aosSwaths.size(); i++ )
        res.push_back(aosSwaths[i]);

    return res;
}

/************************************************************************/
/*                             OpenGroup()                              */
/************************************************************************/

std::shared_ptr<GDALGroup> HDF4SwathsGroup::OpenGroup(
                                const std::string& osName, CSLConstList) const
{
    CPLMutexHolderD(&hHDF4Mutex);

    int32 swathHandle = SWattach( m_poSwathsHandle->m_handle, osName.c_str() );
    if( swathHandle < 0 )
    {
        return nullptr;
    }

    return std::make_shared<HDF4SwathGroup>(GetFullName(), osName, m_poShared,
        std::make_shared<HDF4SwathHandle>(m_poSwathsHandle, swathHandle));
}

/************************************************************************/
/*                         GetMDArrayNames()                            */
/************************************************************************/

std::vector<std::string> HDF4SwathSubGroup::GetMDArrayNames(CSLConstList) const
{
    CPLMutexHolderD(&hHDF4Mutex);
    std::vector<std::string> ret;

    int32 nStrBufSize = 0;
    const int32 nFields
        = SWnentries( m_poSwathHandle->m_handle, m_entryType, &nStrBufSize );
    std::string osFieldList;
    osFieldList.resize(nStrBufSize);
    std::vector<int32> ranks(nFields);
    std::vector<int32> numberTypes(nFields);

    if( m_entryType == HDFE_NENTDFLD )
        SWinqdatafields( m_poSwathHandle->m_handle, &osFieldList[0], &ranks[0], &numberTypes[0] );
    else
        SWinqgeofields( m_poSwathHandle->m_handle, &osFieldList[0], &ranks[0], &numberTypes[0] );

    CPLStringList aosFields( CSLTokenizeString2( osFieldList.c_str(), ",",
                                                         CSLT_HONOURSTRINGS ) );
    for( int i = 0; i < aosFields.size(); i++ )
        ret.push_back(aosFields[i]);

    return ret;
}

/************************************************************************/
/*                           OpenMDArray()                              */
/************************************************************************/

std::shared_ptr<GDALMDArray> HDF4SwathSubGroup::OpenMDArray(
                                const std::string& osName, CSLConstList) const
{
    CPLMutexHolderD(&hHDF4Mutex);

    int32 iRank;
    int32 iNumType;
    std::vector<int32> aiDimSizes(H4_MAX_VAR_DIMS);
    std::string dimNames;

    int32 nStrBufSize = 0;
    if( SWnentries( m_poSwathHandle->m_handle, HDFE_NENTDIM, &nStrBufSize ) < 0
        || nStrBufSize <= 0 )
    {
        return nullptr;
    }
    dimNames.resize(nStrBufSize);
    if( SWfieldinfo( m_poSwathHandle->m_handle, osName.c_str(), &iRank, &aiDimSizes[0],
                     &iNumType, &dimNames[0] ) < 0 )
    {
        return nullptr;
    }
    aiDimSizes.resize(iRank);

    return HDF4SwathArray::Create(
        GetFullName(), osName, m_poShared, m_poSwathHandle,
        aiDimSizes, dimNames, iNumType, m_groupDims);
}

/************************************************************************/
/*                            GetGroupNames()                           */
/************************************************************************/

std::vector<std::string> HDF4SwathGroup::GetGroupNames(CSLConstList) const
{
    std::vector<std::string> res;
    res.push_back("Data Fields");
    res.push_back("Geolocation Fields");
    return res;
}

/************************************************************************/
/*                             OpenGroup()                              */
/************************************************************************/

std::shared_ptr<GDALGroup> HDF4SwathGroup::OpenGroup(
                                const std::string& osName, CSLConstList) const
{
    if( osName == "Data Fields" )
    {
        return std::make_shared<HDF4SwathSubGroup>(
            GetFullName(), osName, m_poShared, m_poSwathHandle, HDFE_NENTDFLD, GetDimensions());
    }
    if( osName == "Geolocation Fields" )
    {
        return std::make_shared<HDF4SwathSubGroup>(
            GetFullName(), osName, m_poShared, m_poSwathHandle, HDFE_NENTGFLD, GetDimensions());
    }
    return nullptr;
}

/************************************************************************/
/*                            GetDimensions()                           */
/************************************************************************/

std::vector<std::shared_ptr<GDALDimension>> HDF4SwathGroup::GetDimensions(CSLConstList) const
{
    if( !m_dims.empty() )
        return m_dims;
    std::string dimNames;
    int32 nStrBufSize = 0;
    if( SWnentries( m_poSwathHandle->m_handle, HDFE_NENTDIM, &nStrBufSize ) < 0
        || nStrBufSize <= 0 )
    {
        return m_dims;
    }
    dimNames.resize(nStrBufSize);
    int32 nDims = SWinqdims(m_poSwathHandle->m_handle, &dimNames[0], nullptr);
    std::vector<int32> aiDimSizes(nDims);
    SWinqdims(m_poSwathHandle->m_handle, &dimNames[0], &aiDimSizes[0]);
    CPLStringList aosDimNames(CSLTokenizeString2(
        dimNames.c_str(), ",", CSLT_HONOURSTRINGS ));
    if( static_cast<size_t>(aosDimNames.size()) == aiDimSizes.size() )
    {
        for( int i = 0; i < aosDimNames.size(); i++ )
        {
            m_dims.push_back(std::make_shared<GDALDimension>(GetFullName(),
                                                             aosDimNames[i],
                                                             std::string(),
                                                             std::string(),
                                                             aiDimSizes[i]));
        }
    }
    return m_dims;
}

/************************************************************************/
/*                           GetAttributes()                            */
/************************************************************************/

std::vector<std::shared_ptr<GDALAttribute>> HDF4SwathGroup::GetAttributes(
                                            CSLConstList) const
{
    CPLMutexHolderD(&hHDF4Mutex);
    std::vector<std::shared_ptr<GDALAttribute>> ret;
    int32 nStrBufSize = 0;
    if( SWinqattrs( m_poSwathHandle->m_handle, nullptr, &nStrBufSize ) <= 0 ||
        nStrBufSize <= 0 )
    {
        return ret;
    }
    std::string osAttrs;
    osAttrs.resize(nStrBufSize);
    SWinqattrs( m_poSwathHandle->m_handle, &osAttrs[0], &nStrBufSize );

    CPLStringList aosAttrs(CSLTokenizeString2( osAttrs.c_str(), ",",
                                                     CSLT_HONOURSTRINGS ));
    for( int i = 0; i < aosAttrs.size(); i++ )
    {
        int32 iNumType = 0;
        int32 nSize = 0;

        const auto& osAttrName = aosAttrs[i];
        if( SWattrinfo( m_poSwathHandle->m_handle, osAttrName,
                        &iNumType, &nSize ) < 0 )
            continue;
        const int nDataTypeSize = HDF4Dataset::GetDataTypeSize(iNumType);
        if( nDataTypeSize == 0 )
            continue;

        ret.emplace_back(std::make_shared<HDF4SwathAttribute>(GetFullName(),
                                                        osAttrName,
                                                        m_poShared,
                                                        m_poSwathHandle,
                                                        iNumType,
                                                        nSize / nDataTypeSize));
    }
    return ret;
}

/************************************************************************/
/*                          HDF4SwathArray()                            */
/************************************************************************/

HDF4SwathArray::HDF4SwathArray(const std::string& osParentName,
                   const std::string& osName,
                   const std::shared_ptr<HDF4SharedResources>& poShared,
                   const std::shared_ptr<HDF4SwathHandle>& poSwathHandle,
                   const std::vector<int32>& aiDimSizes,
                   const std::string& dimNames,
                   int32 iNumType,
                   const std::vector<std::shared_ptr<GDALDimension>>& groupDims):
    GDALAbstractMDArray(osParentName, osName),
    GDALPamMDArray(osParentName, osName, poShared->GetPAM()),
    m_poShared(poShared),
    m_poSwathHandle(poSwathHandle),
    m_dt( iNumType == DFNT_CHAR8 ?
            GDALExtendedDataType::CreateString() :
            GDALExtendedDataType::Create(HDF4Dataset::GetDataType(iNumType)) )
{
    CPLStringList aosDimNames(CSLTokenizeString2(
        dimNames.c_str(), ",", CSLT_HONOURSTRINGS ));
    if( static_cast<size_t>(aosDimNames.size()) == aiDimSizes.size() )
    {
        for( int i = 0; i < aosDimNames.size(); i++ )
        {
            bool bFound = false;
            for( const auto& poDim: groupDims )
            {
                if( poDim->GetName() == aosDimNames[i] &&
                    poDim->GetSize() == static_cast<GUInt64>(aiDimSizes[i]) )
                {
                    bFound = true;
                    m_dims.push_back(poDim);
                    break;
                }
            }
            if( !bFound )
            {
                m_dims.push_back(std::make_shared<GDALDimension>(std::string(),
                                                                aosDimNames[i],
                                                                std::string(),
                                                                std::string(),
                                                                aiDimSizes[i]));
            }
        }
    }
}

/************************************************************************/
/*                           GetAttributes()                            */
/************************************************************************/

std::vector<std::shared_ptr<GDALAttribute>> HDF4SwathArray::GetAttributes(
                                            CSLConstList) const
{
    CPLMutexHolderD(&hHDF4Mutex);
    std::vector<std::shared_ptr<GDALAttribute>> ret;
    int32 iSDS = 0;
    if( SWsdid(m_poSwathHandle->m_handle, GetName().c_str(), &iSDS) != -1 )
    {
        int32 iRank = 0;
        int32 iNumType = 0;
        int32 nAttrs = 0;
        std::vector<int32> aiDimSizes(H4_MAX_VAR_DIMS);

        if( SDgetinfo( iSDS, nullptr, &iRank, &aiDimSizes[0], &iNumType,
                       &nAttrs) == 0 )
        {
            for( int32 iAttribute = 0; iAttribute < nAttrs; iAttribute++ )
            {
                std::string osAttrName;
                osAttrName.resize(H4_MAX_NC_NAME);
                iNumType = 0;
                int32 nValues = 0;
                SDattrinfo( iSDS, iAttribute, &osAttrName[0],
                            &iNumType, &nValues );
                osAttrName.resize(strlen(osAttrName.c_str()));
                ret.emplace_back(std::make_shared<HDF4SDAttribute>(GetFullName(),
                                                            osAttrName,
                                                            m_poShared,
                                                            m_poSwathHandle,
                                                            nullptr,
                                                            iSDS,
                                                            iAttribute,
                                                            iNumType,
                                                            nValues));
            }
        }
    }
    return ret;
}

/************************************************************************/
/*                           ReadPixels()                               */
/************************************************************************/

union ReadFunc
{
    intn (*pReadField)(int32, const char *, int32 [], int32 [], int32 [], VOIDP);
    intn (*pReadData)(int32, int32 [], int32 [], int32 [], VOIDP);
};

// The overflow is a bit technical here and not a real one. This comes from
// the fact that GPtrDiff_t is signed, but size_t is not. So from C/C++
// standards, GPtrDiff_t is converted to a unsigned integer, and
// complement-to-2 arithmetic does things right.
CPL_NOSANITIZE_UNSIGNED_INT_OVERFLOW
static inline void IncrPointer(GByte*& ptr, GPtrDiff_t nInc, size_t nIncSize)
{
    ptr += nInc * nIncSize;
}

static bool ReadPixels(const GUInt64* arrayStartIdx,
                    const size_t* count,
                    const GInt64* arrayStep,
                    const GPtrDiff_t* bufferStride,
                    const GDALExtendedDataType& bufferDataType,
                    void* pDstBuffer,
                    const std::shared_ptr<HDF4SharedResources>& poShared,
                    const GDALExtendedDataType& dt,
                    const std::vector<std::shared_ptr<GDALDimension>>& dims,
                    int32 handle,
                    const char* pszFieldName,
                    ReadFunc readFunc)
{
    CPLMutexHolderD(&hHDF4Mutex);
/* -------------------------------------------------------------------- */
/*      HDF files with external data files, such as some landsat        */
/*      products (eg. data/hdf/L1G) need to be told what directory      */
/*      to look in to find the external files.  Normally this is the    */
/*      directory holding the hdf file.                                 */
/* -------------------------------------------------------------------- */
    HXsetdir(CPLGetPath(poShared->GetFilename().c_str()));

    const size_t nDims(dims.size());
    std::vector<int32> sw_start(nDims);
    std::vector<int32> sw_stride(nDims);
    std::vector<int32> sw_edge(nDims);
    std::vector<GPtrDiff_t> newBufferStride(nDims);
    GByte* pabyDstBuffer = static_cast<GByte*>(pDstBuffer);
    const size_t nBufferDataTypeSize = bufferDataType.GetSize();
    for( size_t i = 0; i < nDims; i++ )
    {
        sw_start[i] = static_cast<int>(arrayStartIdx[i]);
        sw_stride[i] = static_cast<int>(arrayStep[i]);
        sw_edge[i] = static_cast<int>(count[i]);
        newBufferStride[i] = bufferStride[i];
        if( sw_stride[i] < 0 )
        {
            // SWreadfield() doesn't like negative step / array stride, so
            // transform the request to a classic "left-to-right" one
            sw_start[i] += sw_stride[i] * (sw_edge[i] - 1);
            sw_stride[i] = -sw_stride[i];
            pabyDstBuffer += (sw_edge[i]-1) * newBufferStride[i] * nBufferDataTypeSize;
            newBufferStride[i] = -newBufferStride[i];
        }
    }
    size_t nExpectedStride = 1;
    bool bContiguousStride = true;
    for( size_t i = nDims; i > 0; )
    {
        --i;
        if( newBufferStride[i] != static_cast<GPtrDiff_t>(nExpectedStride) )
        {
            bContiguousStride = false;
        }
        nExpectedStride *= count[i];
    }
    if( bufferDataType == dt && bContiguousStride )
    {
        auto status = pszFieldName ?
            readFunc.pReadField(handle, pszFieldName,
                       &sw_start[0], &sw_stride[0], &sw_edge[0],
                       pabyDstBuffer) :
            readFunc.pReadData(handle, &sw_start[0], &sw_stride[0], &sw_edge[0],
                      pabyDstBuffer);
        return status == 0;
    }
    auto pabyTemp = static_cast<GByte*>(
                VSI_MALLOC2_VERBOSE(dt.GetSize(), nExpectedStride));
    if( pabyTemp == nullptr )
        return false;
    auto status = pszFieldName ?
        readFunc.pReadField(handle, pszFieldName,
                   &sw_start[0], &sw_stride[0], &sw_edge[0],
                   pabyTemp) :
        readFunc.pReadData(handle, &sw_start[0], &sw_stride[0], &sw_edge[0],
                  pabyTemp);
    if( status != 0 )
    {
        VSIFree(pabyTemp);
        return false;
    }

    const size_t nSrcDataTypeSize = dt.GetSize();
    std::vector<size_t> anStackCount(nDims);
    GByte* pabySrc = pabyTemp;
    std::vector<GByte*> pabyDstBufferStack(nDims + 1);
    pabyDstBufferStack[0] = pabyDstBuffer;
    size_t iDim = 0;
lbl_next_depth:
    if( iDim == nDims )
    {
        GDALExtendedDataType::CopyValue(
            pabySrc, dt,
            pabyDstBufferStack[nDims], bufferDataType);
        pabySrc += nSrcDataTypeSize;
    }
    else
    {
        anStackCount[iDim] = count[iDim];
        while( true )
        {
            ++ iDim;
            pabyDstBufferStack[iDim] = pabyDstBufferStack[iDim-1];
            goto lbl_next_depth;
lbl_return_to_caller_in_loop:
            -- iDim;
            -- anStackCount[iDim];
            if( anStackCount[iDim] == 0 )
                break;
            IncrPointer(pabyDstBufferStack[iDim],
                        newBufferStride[iDim], nBufferDataTypeSize);
        }
    }
    if( iDim > 0 )
        goto lbl_return_to_caller_in_loop;

    VSIFree(pabyTemp);
    return true;
}

/************************************************************************/
/*                               IRead()                                */
/************************************************************************/

bool HDF4SwathArray::IRead(const GUInt64* arrayStartIdx,
                               const size_t* count,
                               const GInt64* arrayStep,
                               const GPtrDiff_t* bufferStride,
                               const GDALExtendedDataType& bufferDataType,
                               void* pDstBuffer) const
{
    ReadFunc readFunc;
    readFunc.pReadField = SWreadfield;
    return ReadPixels(arrayStartIdx, count, arrayStep, bufferStride,
                   bufferDataType, pDstBuffer,
                   m_poShared,
                   m_dt,
                   m_dims,
                   m_poSwathHandle->m_handle,
                   GetName().c_str(),
                   readFunc);
}

/************************************************************************/
/*                          GetRawNoDataValue()                         */
/************************************************************************/

const void* HDF4SwathArray::GetRawNoDataValue() const
{
    if( !m_abyNoData.empty() )
        return m_abyNoData.data();
    m_abyNoData.resize(GetDataType().GetSize());

    auto poAttr = GetAttribute("_FillValue");
    if( poAttr )
    {
        const double dfVal = poAttr->ReadAsDouble();
        GDALExtendedDataType::CopyValue(
            &dfVal, GDALExtendedDataType::Create(GDT_Float64),
            &m_abyNoData[0], GetDataType());
        return m_abyNoData.data();
    }

    CPLMutexHolderD(&hHDF4Mutex);
    if( SWgetfillvalue( m_poSwathHandle->m_handle, GetName().c_str(),
                                    &m_abyNoData[0] ) != -1 )
    {
        return m_abyNoData.data();
    }

    m_abyNoData.clear();
    return nullptr;
}

/************************************************************************/
/*                      HDF4AbstractAttribute()                         */
/************************************************************************/

HDF4AbstractAttribute::HDF4AbstractAttribute(const std::string& osParentName,
                             const std::string& osName,
                             const std::shared_ptr<HDF4SharedResources>& poShared,
                             int32 iNumType,
                             int32 nValues):
    GDALAbstractMDArray(osParentName, osName),
    GDALAttribute(osParentName, osName),
    m_poShared(poShared),
    m_dt( iNumType == DFNT_CHAR8 ?
            GDALExtendedDataType::CreateString() :
            GDALExtendedDataType::Create(HDF4Dataset::GetDataType(iNumType)) ),
    m_nValues(nValues)
{
    if( m_dt.GetClass() != GEDTC_STRING && m_nValues > 1 )
    {
        m_dims.emplace_back(std::make_shared<GDALDimension>(
            std::string(), "dim", std::string(), std::string(), nValues) );
    }
}

/************************************************************************/
/*                               IRead()                                */
/************************************************************************/

bool HDF4AbstractAttribute::IRead(const GUInt64* arrayStartIdx,
                               const size_t* count,
                               const GInt64* arrayStep,
                               const GPtrDiff_t* bufferStride,
                               const GDALExtendedDataType& bufferDataType,
                               void* pDstBuffer) const
{
    CPLMutexHolderD(&hHDF4Mutex);
    if( m_dt.GetClass() == GEDTC_STRING )
    {
        if( bufferDataType.GetClass() != GEDTC_STRING )
            return false;
        char* pszStr = static_cast<char*>(VSIMalloc( m_nValues + 1 ));
        if( pszStr == nullptr )
            return false;
        ReadData( pszStr );
        pszStr[m_nValues] = 0;
        *static_cast<char**>(pDstBuffer) = pszStr;
        return true;
    }

    std::vector<GByte> abyTemp(m_nValues * m_dt.GetSize());
    ReadData( &abyTemp[0] );
    GByte* pabyDstBuffer = static_cast<GByte*>(pDstBuffer);
    for( size_t i = 0; i < (m_dims.empty() ? 1 : count[0]); ++i )
    {
        const size_t idx = m_dims.empty() ? 0 :
            static_cast<size_t>(arrayStartIdx[0] + i * arrayStep[0]);
        GDALExtendedDataType::CopyValue(
            &abyTemp[0] + idx * m_dt.GetSize(), m_dt,
            pabyDstBuffer, bufferDataType);
        if( !m_dims.empty() )
            pabyDstBuffer += bufferStride[0] * bufferDataType.GetSize();
    }

    return true;
}

/************************************************************************/
/*                            GetGroupNames()                           */
/************************************************************************/

std::vector<std::string> HDF4EOSGridsGroup::GetGroupNames(CSLConstList) const
{
    CPLMutexHolderD(&hHDF4Mutex);
    std::vector<std::string> res;

    int32 nStrBufSize = 0;
    GDinqgrid( m_poShared->GetFilename().c_str(), nullptr, &nStrBufSize );

    std::string osGridList;
    osGridList.resize(nStrBufSize);
    GDinqgrid( m_poShared->GetFilename().c_str(), &osGridList[0], &nStrBufSize );

    CPLStringList aosGrids(
            CSLTokenizeString2( osGridList.c_str(), ",", CSLT_HONOURSTRINGS ));
    for( int i = 0; i < aosGrids.size(); i++ )
        res.push_back(aosGrids[i]);

    return res;
}

/************************************************************************/
/*                             OpenGroup()                              */
/************************************************************************/

std::shared_ptr<GDALGroup> HDF4EOSGridsGroup::OpenGroup(
                                const std::string& osName, CSLConstList) const
{
    CPLMutexHolderD(&hHDF4Mutex);

    int32 gdHandle = GDattach( m_poGDsHandle->m_handle, osName.c_str() );
    if( gdHandle < 0 )
    {
        return nullptr;
    }

    return std::make_shared<HDF4EOSGridGroup>(GetFullName(), osName, m_poShared,
        std::make_shared<HDF4GDHandle>(m_poGDsHandle, gdHandle));
}


/************************************************************************/
/*                            GetDimensions()                           */
/************************************************************************/

std::vector<std::shared_ptr<GDALDimension>> HDF4EOSGridGroup::GetDimensions(CSLConstList) const
{
    if( !m_dims.empty() )
        return m_dims;

    int32 iProjCode = 0;
    int32 iZoneCode = 0;
    int32 iSphereCode = 0;
    double adfProjParams[15];

    GDprojinfo( m_poGDHandle->m_handle, &iProjCode, &iZoneCode,
                &iSphereCode, adfProjParams);

    int32 nXSize = 0;
    int32 nYSize = 0;
    double adfUpLeft[2];
    double adfLowRight[2];
    const bool bGotGridInfo = GDgridinfo( m_poGDHandle->m_handle,
                                          &nXSize, &nYSize,
                                          adfUpLeft, adfLowRight ) >= 0;
    if( bGotGridInfo  )
    {
        m_dims = {
            std::make_shared<GDALDimensionWeakIndexingVar>(
                GetFullName(), "YDim", GDAL_DIM_TYPE_HORIZONTAL_Y, "NORTH",
                nYSize),
            std::make_shared<GDALDimensionWeakIndexingVar>(
                GetFullName(), "XDim", GDAL_DIM_TYPE_HORIZONTAL_X, "EAST",
                nXSize)
        };

        if( iProjCode == 0 )
        {
            adfLowRight[0] = CPLPackedDMSToDec(adfLowRight[0]);
            adfLowRight[1] = CPLPackedDMSToDec(adfLowRight[1]);
            adfUpLeft[0] = CPLPackedDMSToDec(adfUpLeft[0]);
            adfUpLeft[1] = CPLPackedDMSToDec(adfUpLeft[1]);
        }

        m_varX = std::make_shared<GDALMDArrayRegularlySpaced>(
            GetFullName(), m_dims[1]->GetName(), m_dims[1],
            adfUpLeft[0],
            (adfLowRight[0] - adfUpLeft[0]) / nXSize, 0.5);
        m_dims[1]->SetIndexingVariable(m_varX);

        m_varY = std::make_shared<GDALMDArrayRegularlySpaced>(
            GetFullName(), m_dims[0]->GetName(), m_dims[0],
            adfUpLeft[1],
            (adfLowRight[1] - adfUpLeft[1]) / nYSize, 0.5);
        m_dims[0]->SetIndexingVariable(m_varY);
    }

#if 0
    // Dimensions seem to be never defined properly on eos_grids datasets.

    std::string dimNames;
    int32 nStrBufSize = 0;
    if( GDnentries( m_poGDHandle->m_handle, HDFE_NENTDIM, &nStrBufSize ) < 0
        || nStrBufSize <= 0 )
    {
        return m_dims;
    }
    dimNames.resize(nStrBufSize);
    int32 nDims = GDinqdims(m_poGDHandle->m_handle, &dimNames[0], nullptr);
    std::vector<int32> aiDimSizes(nDims);
    GDinqdims(m_poGDHandle->m_handle, &dimNames[0], &aiDimSizes[0]);
    CPLStringList aosDimNames(CSLTokenizeString2(
        dimNames.c_str(), ",", CSLT_HONOURSTRINGS ));
    if( static_cast<size_t>(aosDimNames.size()) == aiDimSizes.size() )
    {
        for( int i = 0; i < aosDimNames.size(); i++ )
        {
            m_dims.push_back(std::make_shared<GDALDimension>(GetFullName(),
                                                             aosDimNames[i],
                                                             std::string(),
                                                             std::string(),
                                                             aiDimSizes[i]));
        }
    }
#endif
    return m_dims;
}


/************************************************************************/
/*                         GetMDArrayNames()                            */
/************************************************************************/

std::vector<std::string> HDF4EOSGridGroup::GetMDArrayNames(CSLConstList) const
{
    GetDimensions();
    std::vector<std::string> ret;
    if( m_varX && m_varY )
    {
        ret.push_back(m_varY->GetName());
        ret.push_back(m_varX->GetName());
    }
    return ret;
}

/************************************************************************/
/*                           OpenMDArray()                              */
/************************************************************************/

std::shared_ptr<GDALMDArray> HDF4EOSGridGroup::OpenMDArray(
                                const std::string& osName, CSLConstList) const
{
    if( m_varX && osName == m_varX->GetName() )
        return m_varX;
    if( m_varY && osName == m_varY->GetName() )
        return m_varY;
    return nullptr;
}

/************************************************************************/
/*                            GetGroupNames()                           */
/************************************************************************/

std::vector<std::string> HDF4EOSGridGroup::GetGroupNames(CSLConstList) const
{
    std::vector<std::string> res;
    res.push_back("Data Fields");
    return res;
}

/************************************************************************/
/*                             OpenGroup()                              */
/************************************************************************/

std::shared_ptr<GDALGroup> HDF4EOSGridGroup::OpenGroup(
                                const std::string& osName, CSLConstList) const
{
    if( osName == "Data Fields" )
    {
        return std::make_shared<HDF4EOSGridSubGroup>(
            GetFullName(), osName, m_poShared, m_poGDHandle, HDFE_NENTDFLD, GetDimensions());
    }
    return nullptr;
}

/************************************************************************/
/*                           GetAttributes()                            */
/************************************************************************/

std::vector<std::shared_ptr<GDALAttribute>> HDF4EOSGridGroup::GetAttributes(
                                            CSLConstList) const
{
    CPLMutexHolderD(&hHDF4Mutex);
    std::vector<std::shared_ptr<GDALAttribute>> ret;
    int32 nStrBufSize = 0;
    if( GDinqattrs( m_poGDHandle->m_handle, nullptr, &nStrBufSize ) <= 0 ||
        nStrBufSize <= 0 )
    {
        return ret;
    }
    std::string osAttrs;
    osAttrs.resize(nStrBufSize);
    GDinqattrs( m_poGDHandle->m_handle, &osAttrs[0], &nStrBufSize );

    CPLStringList aosAttrs(CSLTokenizeString2( osAttrs.c_str(), ",",
                                                     CSLT_HONOURSTRINGS ));
    for( int i = 0; i < aosAttrs.size(); i++ )
    {
        int32 iNumType = 0;
        int32 nSize = 0;

        if( GDattrinfo( m_poGDHandle->m_handle, aosAttrs[i],
                        &iNumType, &nSize ) < 0 )
            continue;
        const int nDataTypeSize = HDF4Dataset::GetDataTypeSize(iNumType);
        if( nDataTypeSize == 0 )
            continue;

        ret.emplace_back(std::make_shared<HDF4EOSGridAttribute>(GetFullName(),
                                                        aosAttrs[i],
                                                        m_poShared,
                                                        m_poGDHandle,
                                                        iNumType,
                                                        nSize / nDataTypeSize));
    }
    return ret;
}

/************************************************************************/
/*                         GetMDArrayNames()                            */
/************************************************************************/

std::vector<std::string> HDF4EOSGridSubGroup::GetMDArrayNames(CSLConstList) const
{
    std::vector<std::string> ret;

    int32 nStrBufSize = 0;
    const int32 nFields
        = GDnentries( m_poGDHandle->m_handle, m_entryType, &nStrBufSize );
    std::string osFieldList;
    osFieldList.resize(nStrBufSize);
    std::vector<int32> ranks(nFields);
    std::vector<int32> numberTypes(nFields);

    CPLAssert( m_entryType == HDFE_NENTDFLD );
    GDinqfields( m_poGDHandle->m_handle, &osFieldList[0], &ranks[0], &numberTypes[0] );

    CPLStringList aosFields( CSLTokenizeString2( osFieldList.c_str(), ",",
                                                         CSLT_HONOURSTRINGS ) );
    for( int i = 0; i < aosFields.size(); i++ )
        ret.push_back(aosFields[i]);

    return ret;
}

/************************************************************************/
/*                           OpenMDArray()                              */
/************************************************************************/

std::shared_ptr<GDALMDArray> HDF4EOSGridSubGroup::OpenMDArray(
                                const std::string& osName, CSLConstList) const
{
    CPLMutexHolderD(&hHDF4Mutex);

    int32 iRank;
    int32 iNumType;
    std::vector<int32> aiDimSizes(H4_MAX_VAR_DIMS);
    std::string dimNames;

    int32 nStrBufSize = 0;
    GDnentries( m_poGDHandle->m_handle, HDFE_NENTDIM, &nStrBufSize );
    if( nStrBufSize <= 0 )
        dimNames.resize(HDFE_DIMBUFSIZE);
    else
        dimNames.resize(nStrBufSize);
    if( GDfieldinfo( m_poGDHandle->m_handle, osName.c_str(), &iRank, &aiDimSizes[0],
                     &iNumType, &dimNames[0] ) < 0 )
    {
        return nullptr;
    }
    aiDimSizes.resize(iRank);
    dimNames.resize(strlen(dimNames.c_str()));

    return HDF4EOSGridArray::Create(
        GetFullName(), osName, m_poShared, m_poGDHandle,
        aiDimSizes, dimNames, iNumType, m_groupDims);
}

/************************************************************************/
/*                         HDF4EOSGridArray()                           */
/************************************************************************/

HDF4EOSGridArray::HDF4EOSGridArray(const std::string& osParentName,
                   const std::string& osName,
                   const std::shared_ptr<HDF4SharedResources>& poShared,
                   const std::shared_ptr<HDF4GDHandle>& poGDHandle,
                   const std::vector<int32>& aiDimSizes,
                   const std::string& dimNames,
                   int32 iNumType,
                   const std::vector<std::shared_ptr<GDALDimension>>& groupDims):
    GDALAbstractMDArray(osParentName, osName),
    GDALPamMDArray(osParentName, osName, poShared->GetPAM()),
    m_poShared(poShared),
    m_poGDHandle(poGDHandle),
    m_dt( iNumType == DFNT_CHAR8 ?
            GDALExtendedDataType::CreateString() :
            GDALExtendedDataType::Create(HDF4Dataset::GetDataType(iNumType)) )
{
    CPLStringList aosDimNames(CSLTokenizeString2(
        dimNames.c_str(), ",", CSLT_HONOURSTRINGS ));
    if( static_cast<size_t>(aosDimNames.size()) == aiDimSizes.size() )
    {
        for( int i = 0; i < aosDimNames.size(); i++ )
        {
            bool bFound = false;
            for( const auto& poDim: groupDims )
            {
                if( poDim->GetName() == aosDimNames[i] &&
                    poDim->GetSize() == static_cast<GUInt64>(aiDimSizes[i]) )
                {
                    bFound = true;
                    m_dims.push_back(poDim);
                    break;
                }
            }
            if( !bFound )
            {
                m_dims.push_back(std::make_shared<GDALDimension>(std::string(),
                                                                aosDimNames[i],
                                                                std::string(),
                                                                std::string(),
                                                                aiDimSizes[i]));
            }
        }
    }
}

/************************************************************************/
/*                           GetAttributes()                            */
/************************************************************************/

std::vector<std::shared_ptr<GDALAttribute>> HDF4EOSGridArray::GetAttributes(
                                            CSLConstList) const
{
    CPLMutexHolderD(&hHDF4Mutex);
    std::vector<std::shared_ptr<GDALAttribute>> ret;
    int32 iSDS = 0;
    if( GDsdid(m_poGDHandle->m_handle, GetName().c_str(), &iSDS) != -1 )
    {
        int32 iRank = 0;
        int32 iNumType = 0;
        int32 nAttrs = 0;
        std::vector<int32> aiDimSizes(H4_MAX_VAR_DIMS);

        if( SDgetinfo( iSDS, nullptr, &iRank, &aiDimSizes[0], &iNumType,
                       &nAttrs) == 0 )
        {
            for( int32 iAttribute = 0; iAttribute < nAttrs; iAttribute++ )
            {
                std::string osAttrName;
                osAttrName.resize(H4_MAX_NC_NAME);
                iNumType = 0;
                int32 nValues = 0;
                SDattrinfo( iSDS, iAttribute, &osAttrName[0],
                            &iNumType, &nValues );
                osAttrName.resize(strlen(osAttrName.c_str()));
                ret.emplace_back(std::make_shared<HDF4SDAttribute>(GetFullName(),
                                                            osAttrName,
                                                            m_poShared,
                                                            nullptr,
                                                            m_poGDHandle,
                                                            iSDS,
                                                            iAttribute,
                                                            iNumType,
                                                            nValues));
            }
        }
    }
    return ret;
}

/************************************************************************/
/*                          GetRawNoDataValue()                         */
/************************************************************************/

const void* HDF4EOSGridArray::GetRawNoDataValue() const
{
    if( !m_abyNoData.empty() )
        return m_abyNoData.data();
    m_abyNoData.resize(GetDataType().GetSize());

    auto poAttr = GetAttribute("_FillValue");
    if( poAttr )
    {
        const double dfVal = poAttr->ReadAsDouble();
        GDALExtendedDataType::CopyValue(
            &dfVal, GDALExtendedDataType::Create(GDT_Float64),
            &m_abyNoData[0], GetDataType());
        return m_abyNoData.data();
    }

    CPLMutexHolderD(&hHDF4Mutex);
    if( GDgetfillvalue( m_poGDHandle->m_handle, GetName().c_str(),
                                    &m_abyNoData[0] ) != -1 )
    {
        return m_abyNoData.data();
    }
    m_abyNoData.clear();
    return nullptr;
}

/************************************************************************/
/*                           GetOffsetOrScale()                         */
/************************************************************************/

static double GetOffsetOrScale(const GDALMDArray* poArray,
                               const char* pszAttrName,
                               double dfDefaultValue,
                               bool* pbHasVal,
                               GDALDataType* peStorageType)
{
    auto poAttr = poArray->GetAttribute(pszAttrName);
    if( poAttr &&
        (poAttr->GetDataType().GetNumericDataType() == GDT_Float32 ||
         poAttr->GetDataType().GetNumericDataType() == GDT_Float64) )
    {
        if( pbHasVal )
            *pbHasVal = true;
        if( peStorageType )
            *peStorageType = poAttr->GetDataType().GetNumericDataType();
        return poAttr->ReadAsDouble();
    }
    if( pbHasVal )
        *pbHasVal = false;
    return dfDefaultValue;
}

/************************************************************************/
/*                              GetOffset()                             */
/************************************************************************/

static double GetOffset(const GDALMDArray* poArray, bool* pbHasOffset,
                        GDALDataType* peStorageType)
{
    return GetOffsetOrScale(poArray, "add_offset", 0, pbHasOffset, peStorageType);
}

/************************************************************************/
/*                              GetOffset()                             */
/************************************************************************/

double HDF4EOSGridArray::GetOffset(bool* pbHasOffset, GDALDataType* peStorageType) const
{
    return ::GetOffset(this, pbHasOffset, peStorageType);
}

/************************************************************************/
/*                               GetScale()                             */
/************************************************************************/

static double GetScale(const GDALMDArray* poArray, bool* pbHasScale,
                       GDALDataType* peStorageType)
{
    return GetOffsetOrScale(poArray, "scale_factor", 1, pbHasScale, peStorageType);
}

/************************************************************************/
/*                               GetScale()                             */
/************************************************************************/

double HDF4EOSGridArray::GetScale(bool* pbHasScale, GDALDataType* peStorageType) const
{
    return ::GetScale(this, pbHasScale, peStorageType);
}

/************************************************************************/
/*                             GetUnit()                                */
/************************************************************************/

const std::string& HDF4EOSGridArray::GetUnit() const
{
    auto poAttr = GetAttribute("units");
    if( poAttr && poAttr->GetDataType().GetClass() == GEDTC_STRING )
    {
        const char* pszVal = poAttr->ReadAsString();
        if( pszVal )
            m_osUnit = pszVal;
    }
    return m_osUnit;
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

std::shared_ptr<OGRSpatialReference> HDF4EOSGridArray::GetSpatialRef() const
{
    CPLMutexHolderD(&hHDF4Mutex);
    int32 iProjCode = 0;
    int32 iZoneCode = 0;
    int32 iSphereCode = 0;
    double adfProjParams[15];

    if( GDprojinfo( m_poGDHandle->m_handle, &iProjCode, &iZoneCode,
                    &iSphereCode, adfProjParams) >= 0 )
    {
        auto poSRS(std::make_shared<OGRSpatialReference>());
        poSRS->importFromUSGS( iProjCode, iZoneCode,
                                    adfProjParams, iSphereCode,
                                    USGS_ANGLE_RADIANS );
        int iDimY = -1;
        int iDimX = -1;
        if( m_dims.size() >= 2 )
        {
            iDimY = 1 + static_cast<int>(m_dims.size() - 2);
            iDimX = 1 + static_cast<int>(m_dims.size() - 1);
        }
        if( iDimX > 0 && iDimY > 0 )
        {
            poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            if( poSRS->GetDataAxisToSRSAxisMapping() == std::vector<int>{ 2, 1 } )
                poSRS->SetDataAxisToSRSAxisMapping({ iDimY, iDimX });
            else
                poSRS->SetDataAxisToSRSAxisMapping({ iDimX, iDimY });
        }
        return poSRS;
    }
    return nullptr;
}

/************************************************************************/
/*                               IRead()                                */
/************************************************************************/

bool HDF4EOSGridArray::IRead(const GUInt64* arrayStartIdx,
                               const size_t* count,
                               const GInt64* arrayStep,
                               const GPtrDiff_t* bufferStride,
                               const GDALExtendedDataType& bufferDataType,
                               void* pDstBuffer) const
{
    ReadFunc readFunc;
    readFunc.pReadField = GDreadfield;
    return ReadPixels(arrayStartIdx, count, arrayStep, bufferStride,
                   bufferDataType, pDstBuffer,
                   m_poShared,
                   m_dt,
                   m_dims,
                   m_poGDHandle->m_handle,
                   GetName().c_str(),
                   readFunc);
}

/************************************************************************/
/*                         GetMDArrayNames()                            */
/************************************************************************/

std::vector<std::string> HDF4SDSGroup::GetMDArrayNames(CSLConstList) const
{
    CPLMutexHolderD(&hHDF4Mutex);
    std::vector<std::string> ret;

    int32 nDatasets = 0;
    int32 nAttrs = 0;
    if ( SDfileinfo( m_poShared->GetSDHandle(), &nDatasets, &nAttrs ) != 0 )
        return ret;

    std::set<std::string> oSetNames;
    for( int32 i = 0; i < nDatasets; i++ )
    {
        const int32 iSDS = SDselect( m_poShared->GetSDHandle(), i );
        std::string osName;
        osName.resize(VSNAMELENMAX);
        int32 iRank = 0;
        int32 iNumType = 0;
        std::vector<int32> aiDimSizes(H4_MAX_VAR_DIMS);
        if ( SDgetinfo( iSDS, &osName[0], &iRank, &aiDimSizes[0], &iNumType,
                        &nAttrs) == 0 )
        {
            osName.resize(strlen(osName.c_str()));
            int counter = 2;
            std::string osRadix(osName);
            while( oSetNames.find(osName) != oSetNames.end() )
            {
                osName = osRadix + CPLSPrintf("_%d", counter);
                counter++;
            }
            ret.push_back(osName);
            m_oMapNameToSDSIdx[osName] = i;
        }
        SDendaccess( iSDS );
    }

    if( m_bIsGDALDataset )
    {
        GetDimensions();
        if( m_varX && m_varY )
        {
            ret.push_back(m_varX->GetName());
            ret.push_back(m_varY->GetName());
        }
    }

    return ret;
}

/************************************************************************/
/*                           OpenMDArray()                              */
/************************************************************************/

std::shared_ptr<GDALMDArray> HDF4SDSGroup::OpenMDArray(
                                const std::string& osName, CSLConstList) const
{
    CPLMutexHolderD(&hHDF4Mutex);
    if( m_oMapNameToSDSIdx.empty() )
    {
        GetMDArrayNames(nullptr);
    }
    auto oIter = m_oMapNameToSDSIdx.find(osName);
    if( oIter == m_oMapNameToSDSIdx.end() )
    {
        if( m_bIsGDALDataset )
        {
            GetDimensions();
            if( m_varX && m_varX->GetName() == osName )
            {
                return m_varX;
            }
            if( m_varY && m_varY->GetName() == osName )
            {
                return m_varY;
            }
        }
        return nullptr;
    }
    const int32 iSDS = SDselect( m_poShared->GetSDHandle(),
                                 oIter->second );

    int32 iRank = 0;
    int32 iNumType = 0;
    int32 nAttrs = 0;
    std::vector<int32> aiDimSizes(H4_MAX_VAR_DIMS);
    SDgetinfo( iSDS, nullptr, &iRank, &aiDimSizes[0], &iNumType,
               &nAttrs);
    aiDimSizes.resize(iRank);

    auto ar = HDF4SDSArray::Create(GetFullName(), osName, m_poShared,
                                          iSDS,
                                          aiDimSizes,
                                          GetDimensions(),
                                          iNumType, nAttrs,
                                          m_bIsGDALDataset);
    if( m_bIsGDALDataset )
        ar->SetGlobalAttributes(m_oGlobalAttributes);
    return ar;
}

/************************************************************************/
/*                            GetDimensions()                           */
/************************************************************************/

std::vector<std::shared_ptr<GDALDimension>> HDF4SDSGroup::GetDimensions(CSLConstList) const
{
    CPLMutexHolderD(&hHDF4Mutex);
    if( m_bInGetDimensions )
        return {};
    if( !m_dims.empty() )
        return m_dims;
    if( m_oMapNameToSDSIdx.empty() )
    {
        m_bInGetDimensions = true;
        GetMDArrayNames(nullptr);
        m_bInGetDimensions = false;
    }

    std::string osProjection;
    std::string osTransformationMatrix;
    if( m_bIsGDALDataset )
    {
        for( auto& poAttr: m_oGlobalAttributes )
        {
            if( poAttr->GetName() == "Projection" &&
                poAttr->GetDataType().GetClass() == GEDTC_STRING )
            {
                const char* pszVal = poAttr->ReadAsString();
                if( pszVal )
                    osProjection = pszVal;
            }
            else if( poAttr->GetName() == "TransformationMatrix" &&
                poAttr->GetDataType().GetClass() == GEDTC_STRING )
            {
                const char* pszVal = poAttr->ReadAsString();
                if( pszVal )
                    osTransformationMatrix = pszVal;
            }
        }
    }

    // First collect all dimension ids referenced by all datasets
    std::map<int32, int32> oMapDimIdToDimSize;
    std::set<std::string> oSetArrayNames;
    for(const auto& oIter: m_oMapNameToSDSIdx)
    {
        const int32 iSDS = SDselect( m_poShared->GetSDHandle(),
                                     oIter.second );
        int32 iRank = 0;
        int32 iNumType = 0;
        int32 nAttrs = 0;
        std::vector<int32> aiDimSizes(H4_MAX_VAR_DIMS);
        SDgetinfo( iSDS, nullptr, &iRank, &aiDimSizes[0], &iNumType,
                &nAttrs);
        for( int i = 0; i < iRank; i++ )
        {
            const auto dimId = SDgetdimid(iSDS, i);
            oMapDimIdToDimSize[dimId] = std::max(oMapDimIdToDimSize[dimId], aiDimSizes[i]);
        }
        oSetArrayNames.insert(oIter.first);
        SDendaccess(iSDS);
    }

    // Instantiate dimensions
    std::set<std::shared_ptr<GDALDimensionWeakIndexingVar>> oSetDimsWithVariable;
    for(const auto& iter: oMapDimIdToDimSize )
    {
        std::string osName;
        osName.resize(VSNAMELENMAX);
        int32 iSize = 0; // can be 0 for unlimited dimension
        int32 iNumType = 0;
        int32 nAttrs = 0;
        SDdiminfo(iter.first, &osName[0], &iSize, &iNumType, &nAttrs);
        osName.resize(strlen(osName.c_str()));

        std::string osType;
        std::string osDirection;
        bool bIsIndexedDim = false;
        if( iNumType > 0 &&
            oSetArrayNames.find(osName) != oSetArrayNames.end() )
        {
            bIsIndexedDim = true;
            m_bInGetDimensions = true;
            auto poArray(OpenMDArray(osName, nullptr));
            m_bInGetDimensions = false;
            if( poArray )
            {
                auto poAxis = poArray->GetAttribute("axis");
                if( poAxis && poAxis->GetDataType().GetClass() == GEDTC_STRING )
                {
                    const char* pszVal = poAxis->ReadAsString();
                    if( pszVal && EQUAL(pszVal, "X") )
                        osType = GDAL_DIM_TYPE_HORIZONTAL_X;
                    else if( pszVal && EQUAL(pszVal, "Y") )
                        osType = GDAL_DIM_TYPE_HORIZONTAL_Y;
                }
            }
        }

        // Do not trust iSize which can be 0 for a unlimited dimension, but
        // the size actually taken by the array(s)
        auto poDim(std::make_shared<GDALDimensionWeakIndexingVar>(GetFullName(),
                                                      osName,
                                                      osType,
                                                      osDirection,
                                                      iter.second));
        // cppcheck-suppress knownConditionTrueFalse
        if( bIsIndexedDim )
        {
            oSetDimsWithVariable.insert(poDim);
        }
        m_dims.push_back(poDim);
    }

    if( m_bIsGDALDataset && (m_dims.size() == 2 || m_dims.size() == 3) &&
        !osProjection.empty() && !osTransformationMatrix.empty() )
    {
        CPLStringList aosCoeffs(
            CSLTokenizeString2( osTransformationMatrix.c_str(), ",", 0 ));
        if( aosCoeffs.size() == 6 && CPLAtof(aosCoeffs[2]) == 0 &&
            CPLAtof(aosCoeffs[4]) == 0 )
        {
            auto newDims = std::vector<std::shared_ptr<GDALDimension>>{
                std::make_shared<GDALDimensionWeakIndexingVar>(
                    GetFullName(), "Y", GDAL_DIM_TYPE_HORIZONTAL_Y, std::string(),
                    m_dims[0]->GetSize()),
                std::make_shared<GDALDimensionWeakIndexingVar>(
                    GetFullName(), "X", GDAL_DIM_TYPE_HORIZONTAL_X, std::string(),
                    m_dims[1]->GetSize())
            };
            if( m_dims.size() == 3 )
            {
                newDims.push_back(
                    std::make_shared<GDALDimensionWeakIndexingVar>(
                    GetFullName(), "Band", std::string(), std::string(),
                    m_dims[2]->GetSize()));
            }
            m_dims = newDims;

            m_varX = std::make_shared<GDALMDArrayRegularlySpaced>(
                GetFullName(), m_dims[1]->GetName(), m_dims[1],
                CPLAtof(aosCoeffs[0]),
                CPLAtof(aosCoeffs[1]), 0.5);
            m_dims[1]->SetIndexingVariable(m_varX);

            m_varY = std::make_shared<GDALMDArrayRegularlySpaced>(
                GetFullName(), m_dims[0]->GetName(), m_dims[0],
                CPLAtof(aosCoeffs[3]),
                CPLAtof(aosCoeffs[5]), 0.5);
            m_dims[0]->SetIndexingVariable(m_varY);
        }
    }

    // Now that we have eatablished all dimensions, we can link them to
    // variables
    for(auto& poDim: oSetDimsWithVariable )
    {
        auto poArray(OpenMDArray(poDim->GetName(), nullptr));
        if( poArray )
        {
            m_oSetIndexingVariables.push_back(poArray);
            poDim->SetIndexingVariable(poArray);
        }
    }

    return m_dims;
}

/************************************************************************/
/*                           HDF4SDSArray()                             */
/************************************************************************/

HDF4SDSArray::HDF4SDSArray(const std::string& osParentName,
                   const std::string& osName,
                   const std::shared_ptr<HDF4SharedResources>& poShared,
                   int32 iSDS,
                   const std::vector<int32>& aiDimSizes,
                   const std::vector<std::shared_ptr<GDALDimension>>& groupDims,
                   int32 iNumType,
                   int32 nAttrs,
                   bool bIsGDALDS):
    GDALAbstractMDArray(osParentName, osName),
    GDALPamMDArray(osParentName, osName, poShared->GetPAM()),
    m_poShared(poShared),
    m_iSDS(iSDS),
    m_dt( iNumType == DFNT_CHAR8 ?
            GDALExtendedDataType::CreateString() :
            GDALExtendedDataType::Create(HDF4Dataset::GetDataType(iNumType)) ),
    m_nAttributes(nAttrs),
    m_bIsGDALDataset(bIsGDALDS)
{
    for( int i = 0; i < static_cast<int>(aiDimSizes.size()); i++ )
    {
        std::string osDimName;
        osDimName.resize(VSNAMELENMAX);
        int32 iSize = 0;
        int32 iDimNumType = 0;
        int32 nDimAttrs = 0;
        int32 dimId = SDgetdimid(iSDS, i);
        SDdiminfo(dimId, &osDimName[0], &iSize, &iDimNumType, &nDimAttrs);
        osDimName.resize(strlen(osDimName.c_str()));
        bool bFound = false;
        for( const auto& poDim: groupDims )
        {
            if( poDim->GetName() == osDimName ||
                (bIsGDALDS && i == 0 && poDim->GetName() == "Y") ||
                (bIsGDALDS && i == 1 && poDim->GetName() == "X") ||
                (bIsGDALDS && i == 2 && poDim->GetName() == "Band") )
            {
                bFound = true;
                m_dims.push_back(poDim);
                break;
            }
        }
        if( !bFound )
        {
            m_dims.push_back(std::make_shared<GDALDimension>(
                std::string(),
                CPLSPrintf("dim%d", i),
                std::string(), std::string(), aiDimSizes[i]));
        }
    }
}

/************************************************************************/
/*                          ~HDF4SDSArray()                             */
/************************************************************************/

HDF4SDSArray::~HDF4SDSArray()
{
    CPLMutexHolderD(&hHDF4Mutex);
    SDendaccess( m_iSDS );
}

/************************************************************************/
/*                          GetRawNoDataValue()                         */
/************************************************************************/

const void* HDF4SDSArray::GetRawNoDataValue() const
{
    if( !m_abyNoData.empty() )
        return m_abyNoData.data();
    m_abyNoData.resize(GetDataType().GetSize());

    auto poAttr = GetAttribute("_FillValue");
    if( poAttr )
    {
        const double dfVal = poAttr->ReadAsDouble();
        GDALExtendedDataType::CopyValue(
            &dfVal, GDALExtendedDataType::Create(GDT_Float64),
            &m_abyNoData[0], GetDataType());
        return m_abyNoData.data();
    }

    CPLMutexHolderD(&hHDF4Mutex);
    if( SDgetfillvalue( m_iSDS, &m_abyNoData[0] ) != -1 )
    {
        return m_abyNoData.data();
    }

    m_abyNoData.clear();
    return nullptr;
}

/************************************************************************/
/*                           GetAttributes()                            */
/************************************************************************/

std::vector<std::shared_ptr<GDALAttribute>> HDF4SDSArray::GetAttributes(
                                            CSLConstList) const
{
    CPLMutexHolderD(&hHDF4Mutex);
    std::vector<std::shared_ptr<GDALAttribute>> ret;

    for( int32 iAttribute = 0; iAttribute < m_nAttributes; iAttribute++ )
    {
        std::string osAttrName;
        osAttrName.resize(H4_MAX_NC_NAME);
        int32 iNumType = 0;
        int32 nValues = 0;
        SDattrinfo( m_iSDS, iAttribute, &osAttrName[0],
                    &iNumType, &nValues );
        osAttrName.resize(strlen(osAttrName.c_str()));
        ret.emplace_back(std::make_shared<HDF4SDAttribute>(GetFullName(),
                                                    osAttrName,
                                                    m_poShared,
                                                    nullptr,
                                                    nullptr,
                                                    m_iSDS,
                                                    iAttribute,
                                                    iNumType,
                                                    nValues));
    }

    return ret;
}

/************************************************************************/
/*                              GetOffset()                             */
/************************************************************************/

double HDF4SDSArray::GetOffset(bool* pbHasOffset, GDALDataType* peStorageType) const
{
    return ::GetOffset(this, pbHasOffset, peStorageType);
}

/************************************************************************/
/*                               GetScale()                             */
/************************************************************************/

double HDF4SDSArray::GetScale(bool* pbHasScale, GDALDataType* peStorageType) const
{
    return ::GetScale(this, pbHasScale, peStorageType);
}

/************************************************************************/
/*                             GetUnit()                                */
/************************************************************************/

const std::string& HDF4SDSArray::GetUnit() const
{
    auto poAttr = GetAttribute("units");
    if( poAttr && poAttr->GetDataType().GetClass() == GEDTC_STRING )
    {
        const char* pszVal = poAttr->ReadAsString();
        if( pszVal )
            m_osUnit = pszVal;
    }
    return m_osUnit;
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

std::shared_ptr<OGRSpatialReference> HDF4SDSArray::GetSpatialRef() const
{
    if( m_bIsGDALDataset )
    {
        std::string osProjection;
        for( auto& poAttr: m_oGlobalAttributes )
        {
            if( poAttr->GetName() == "Projection" &&
                poAttr->GetDataType().GetClass() == GEDTC_STRING )
            {
                const char* pszVal = poAttr->ReadAsString();
                if( pszVal )
                    osProjection = pszVal;
                break;
            }
        }
        if( !osProjection.empty() )
        {
            auto poSRS(std::make_shared<OGRSpatialReference>());
            poSRS->SetFromUserInput(osProjection.c_str());
            poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            if( poSRS->GetDataAxisToSRSAxisMapping() == std::vector<int>{ 2, 1 } )
                poSRS->SetDataAxisToSRSAxisMapping({ 1, 2 });
            else
                poSRS->SetDataAxisToSRSAxisMapping({ 2, 1 });
            return poSRS;
        }
    }
    return nullptr;
}

/************************************************************************/
/*                               IRead()                                */
/************************************************************************/

bool HDF4SDSArray::IRead(const GUInt64* arrayStartIdx,
                               const size_t* count,
                               const GInt64* arrayStep,
                               const GPtrDiff_t* bufferStride,
                               const GDALExtendedDataType& bufferDataType,
                               void* pDstBuffer) const
{
    ReadFunc readFunc;
    readFunc.pReadData = SDreaddata;
    return ReadPixels(arrayStartIdx, count, arrayStep, bufferStride,
                   bufferDataType, pDstBuffer,
                   m_poShared,
                   m_dt,
                   m_dims,
                   m_iSDS,
                   nullptr,
                   readFunc);
}

/************************************************************************/
/*                          GetMDArrayNames()                           */
/************************************************************************/

std::vector<std::string> HDF4GRsGroup::GetMDArrayNames(CSLConstList) const
{
    CPLMutexHolderD(&hHDF4Mutex);
    std::vector<std::string> res;

    int32 nImages = 0;
    int32 nAttrs = 0;
    GRfileinfo( m_poGRsHandle->m_grHandle, &nImages, &nAttrs );
    for( int32 i = 0; i < nImages; i++ )
    {
        const int32 iGR = GRselect( m_poGRsHandle->m_grHandle, i );

        std::string osName;
        osName.resize(VSNAMELENMAX);
        int32 nBands = 0;
        int32 iNumType = 0;
        int32 iInterlaceMode = 0;
        std::vector<int32> aiDimSizes(2);
        if ( GRgetiminfo( iGR, &osName[0], &nBands, &iNumType, &iInterlaceMode,
                          &aiDimSizes[0], &nAttrs ) == 0 )
        {
            osName.resize(strlen(osName.c_str()));
            m_oMapNameToGRIdx[osName] = i;
            res.push_back(osName);
        }

        GRendaccess( iGR );
    }
    return res;
}

/************************************************************************/
/*                           GetAttributes()                            */
/************************************************************************/

std::vector<std::shared_ptr<GDALAttribute>> HDF4GRsGroup::GetAttributes(
                                            CSLConstList) const
{
    CPLMutexHolderD(&hHDF4Mutex);
    std::vector<std::shared_ptr<GDALAttribute>> ret;
    int32 nDatasets = 0;
    int32 nAttributes = 0;
    if ( GRfileinfo( m_poGRsHandle->m_grHandle, &nDatasets, &nAttributes ) != 0 )
        return ret;
    for( int32 iAttribute = 0; iAttribute < nAttributes; iAttribute++ )
    {
        int32 iNumType = 0;
        int32 nValues = 0;

        std::string osAttrName;
        osAttrName.resize(H4_MAX_NC_NAME);
        GRattrinfo( m_poGRsHandle->m_grHandle, iAttribute, &osAttrName[0], &iNumType, &nValues );
        osAttrName.resize(strlen(osAttrName.c_str()));

        ret.emplace_back(std::make_shared<HDF4GRAttribute>(GetFullName(),
                                                            osAttrName,
                                                            m_poShared,
                                                            m_poGRsHandle,
                                                            nullptr,
                                                            m_poGRsHandle->m_grHandle,
                                                            iAttribute,
                                                            iNumType,
                                                            nValues));
    }
    return ret;
}

/************************************************************************/
/*                           OpenMDArray()                              */
/************************************************************************/

std::shared_ptr<GDALMDArray> HDF4GRsGroup::OpenMDArray(
                                const std::string& osName, CSLConstList) const
{
    CPLMutexHolderD(&hHDF4Mutex);
    if( m_oMapNameToGRIdx.empty() )
    {
        GetMDArrayNames(nullptr);
    }
    auto oIter = m_oMapNameToGRIdx.find(osName);
    if( oIter == m_oMapNameToGRIdx.end() )
    {
        return nullptr;
    }
    const int32 iGR = GRselect( m_poGRsHandle->m_grHandle, oIter->second );

    int32 nBands = 0;
    int32 iNumType = 0;
    int32 iInterlaceMode = 0;
    std::vector<int32> aiDimSizes(2);
    int32 nAttrs;
    GRgetiminfo( iGR, nullptr, &nBands, &iNumType, &iInterlaceMode,
                 &aiDimSizes[0], &nAttrs );

    return HDF4GRArray::Create(GetFullName(), osName, m_poShared,
                               std::make_shared<HDF4GRHandle>(m_poGRsHandle, iGR),
                               nBands, aiDimSizes, iNumType, nAttrs);
}

/************************************************************************/
/*                           HDF4GRArray()                              */
/************************************************************************/

HDF4GRArray::HDF4GRArray(const std::string& osParentName,
                   const std::string& osName,
                   const std::shared_ptr<HDF4SharedResources>& poShared,
                   const std::shared_ptr<HDF4GRHandle>& poGRHandle,
                   int32 nBands,
                   const std::vector<int32>& aiDimSizes,
                   int32 iNumType,
                   int32 nAttrs):
    GDALAbstractMDArray(osParentName, osName),
    GDALPamMDArray(osParentName, osName, poShared->GetPAM()),
    m_poShared(poShared),
    m_poGRHandle(poGRHandle),
    m_dt( iNumType == DFNT_CHAR8 ?
            GDALExtendedDataType::CreateString() :
            GDALExtendedDataType::Create(HDF4Dataset::GetDataType(iNumType)) ),
    m_nAttributes(nAttrs)
{
    for( int i = 0; i < static_cast<int>(aiDimSizes.size()); i++ )
    {
        m_dims.push_back(std::make_shared<GDALDimension>(
            std::string(),
            i == 0 ? "y" : "x",
            std::string(), std::string(), aiDimSizes[i]));
    }
    m_dims.push_back(std::make_shared<GDALDimension>(
            std::string(),
            "bands",
            std::string(), std::string(), nBands));
}

/************************************************************************/
/*                           GetAttributes()                            */
/************************************************************************/

std::vector<std::shared_ptr<GDALAttribute>> HDF4GRArray::GetAttributes(
                                            CSLConstList) const
{
    CPLMutexHolderD(&hHDF4Mutex);
    std::vector<std::shared_ptr<GDALAttribute>> ret;
    for( int32 iAttribute = 0; iAttribute < m_nAttributes; iAttribute++ )
    {
        int32 iNumType = 0;
        int32 nValues = 0;

        std::string osAttrName;
        osAttrName.resize(H4_MAX_NC_NAME);
        GRattrinfo( m_poGRHandle->m_iGR, iAttribute, &osAttrName[0], &iNumType, &nValues );
        osAttrName.resize(strlen(osAttrName.c_str()));

        ret.emplace_back(std::make_shared<HDF4GRAttribute>(GetFullName(),
                                                            osAttrName,
                                                            m_poShared,
                                                            nullptr,
                                                            m_poGRHandle,
                                                            m_poGRHandle->m_iGR,
                                                            iAttribute,
                                                            iNumType,
                                                            nValues));
    }

    auto iPal = GRgetlutid( m_poGRHandle->m_iGR, 0 );
    if( iPal != -1 )
    {
        int32 nComps = 0;
        int32 iPalDataType = 0;
        int32 iPalInterlaceMode = 0;
        int32 nPalEntries = 0;
        GRgetlutinfo( iPal, &nComps, &iPalDataType,
                      &iPalInterlaceMode, &nPalEntries );
        if( nPalEntries && nComps == 3 &&
            GDALGetDataTypeSizeBytes(HDF4Dataset::GetDataType(iPalDataType)) == 1 &&
            nPalEntries <= 256 )
        {
            ret.emplace_back(std::make_shared<HDF4GRPalette>(GetFullName(),
                                                             "lut",
                                                             m_poShared,
                                                             m_poGRHandle,
                                                             iPal,
                                                             nPalEntries));
        }
    }

    return ret;
}

/************************************************************************/
/*                               IRead()                                */
/************************************************************************/

bool HDF4GRArray::IRead(const GUInt64* arrayStartIdx,
                               const size_t* count,
                               const GInt64* arrayStep,
                               const GPtrDiff_t* bufferStride,
                               const GDALExtendedDataType& bufferDataType,
                               void* pDstBuffer) const
{
    CPLMutexHolderD(&hHDF4Mutex);
/* -------------------------------------------------------------------- */
/*      HDF files with external data files, such as some landsat        */
/*      products (eg. data/hdf/L1G) need to be told what directory      */
/*      to look in to find the external files.  Normally this is the    */
/*      directory holding the hdf file.                                 */
/* -------------------------------------------------------------------- */
    HXsetdir(CPLGetPath(m_poShared->GetFilename().c_str()));

    const size_t nDims(m_dims.size());
    std::vector<int32> sw_start(nDims);
    std::vector<int32> sw_stride(nDims);
    std::vector<int32> sw_edge(nDims);
    std::vector<GPtrDiff_t> newBufferStride(nDims);
    GByte* pabyDstBuffer = static_cast<GByte*>(pDstBuffer);
    const size_t nBufferDataTypeSize = bufferDataType.GetSize();
    for( size_t i = 0; i < nDims; i++ )
    {
        sw_start[i] = static_cast<int>(arrayStartIdx[i]);
        sw_stride[i] = static_cast<int>(arrayStep[i]);
        sw_edge[i] = static_cast<int>(count[i]);
        newBufferStride[i] = bufferStride[i];
        if( sw_stride[i] < 0 )
        {
            // GRreadimage() doesn't like negative step / array stride, so
            // transform the request to a classic "left-to-right" one
            sw_start[i] += sw_stride[i] * (sw_edge[i] - 1);
            sw_stride[i] = -sw_stride[i];
            pabyDstBuffer += (sw_edge[i]-1) * newBufferStride[i] * nBufferDataTypeSize;
            newBufferStride[i] = -newBufferStride[i];
        }
    }
    size_t nExpectedStride = 1;
    bool bContiguousStride = true;
    for( size_t i = nDims; i > 0; )
    {
        --i;
        if( newBufferStride[i] != static_cast<GPtrDiff_t>(nExpectedStride) )
        {
            bContiguousStride = false;
            break;
        }
        nExpectedStride *= count[i];
    }
    if( bufferDataType == m_dt && bContiguousStride &&
        arrayStartIdx[2] == 0 && count[2] == m_dims[2]->GetSize() &&
        arrayStep[2] == 1 )
    {
        auto status =
            GRreadimage(m_poGRHandle->m_iGR,
                        &sw_start[0], &sw_stride[0], &sw_edge[0],
                        pabyDstBuffer);
        return status >= 0;
    }
    auto pabyTemp = static_cast<GByte*>(
        VSI_MALLOC2_VERBOSE(m_dt.GetSize(),
            count[0] * count[1] * static_cast<size_t>(m_dims[2]->GetSize())));
    if( pabyTemp == nullptr )
        return false;
    auto status = GRreadimage(m_poGRHandle->m_iGR,
                        &sw_start[0], &sw_stride[0], &sw_edge[0],
                        pabyTemp);
    if( status < 0 )
    {
        VSIFree(pabyTemp);
        return false;
    }

    const size_t nSrcDataTypeSize = m_dt.GetSize();
    std::vector<size_t> anStackCount(nDims);
    GByte* pabySrc = pabyTemp + nSrcDataTypeSize * sw_start[2];
    std::vector<GByte*> pabyDstBufferStack(nDims + 1);
    pabyDstBufferStack[0] = pabyDstBuffer;
    size_t iDim = 0;
lbl_next_depth:
    if( iDim == nDims )
    {
        GDALExtendedDataType::CopyValue(
            pabySrc, m_dt,
            pabyDstBufferStack[nDims], bufferDataType);
        pabySrc += nSrcDataTypeSize * sw_stride[2];
    }
    else
    {
        anStackCount[iDim] = count[iDim];
        while( true )
        {
            ++ iDim;
            pabyDstBufferStack[iDim] = pabyDstBufferStack[iDim-1];
            goto lbl_next_depth;
lbl_return_to_caller_in_loop:
            -- iDim;
            -- anStackCount[iDim];
            if( anStackCount[iDim] == 0 )
                break;
            IncrPointer(pabyDstBufferStack[iDim],
                        newBufferStride[iDim], nBufferDataTypeSize);
        }
        if( iDim == 2 )
            pabySrc += nSrcDataTypeSize * static_cast<size_t>(
                m_dims[2]->GetSize() - count[2] * sw_stride[2]);
    }
    if( iDim > 0 )
        goto lbl_return_to_caller_in_loop;

    VSIFree(pabyTemp);
    return true;
}

/************************************************************************/
/*                           HDF4GRPalette()                            */
/************************************************************************/

HDF4GRPalette::HDF4GRPalette(const std::string& osParentName,
                  const std::string& osName,
                  const std::shared_ptr<HDF4SharedResources>& poShared,
                  const std::shared_ptr<HDF4GRHandle>& poGRHandle,
                  int32 iPal,
                  int32 nValues):
    GDALAbstractMDArray(osParentName, osName),
    GDALAttribute(osParentName, osName),
    m_poShared(poShared),
    m_poGRHandle(poGRHandle),
    m_iPal(iPal),
    m_nValues(nValues)
{
    m_dims.push_back(std::make_shared<GDALDimension>(
            std::string(),
            "index",
            std::string(), std::string(), nValues));
    m_dims.push_back(std::make_shared<GDALDimension>(
            std::string(),
            "component",
            std::string(), std::string(), 3));
}

/************************************************************************/
/*                               IRead()                                */
/************************************************************************/

bool HDF4GRPalette::IRead(const GUInt64* arrayStartIdx,
                               const size_t* count,
                               const GInt64* arrayStep,
                               const GPtrDiff_t* bufferStride,
                               const GDALExtendedDataType& bufferDataType,
                               void* pDstBuffer) const
{
    CPLMutexHolderD(&hHDF4Mutex);

    std::vector<GByte> abyValues(3 * m_nValues);
    GRreadlut( m_iPal, &abyValues[0] );

    GByte* pabyDstBuffer = static_cast<GByte*>(pDstBuffer);
    const size_t nBufferDataTypeSize = bufferDataType.GetSize();
    const auto srcDt(GDALExtendedDataType::Create(GDT_Byte));
    for(size_t i = 0; i < count[0]; ++i)
    {
        size_t idx = static_cast<size_t>(arrayStartIdx[0] + i * arrayStep[0]);
        for(size_t j = 0; j < count[1]; ++j)
        {
            size_t comp = static_cast<size_t>(arrayStartIdx[1] + j * arrayStep[1]);
            GByte* pDst = pabyDstBuffer +
                (i * bufferStride[0] + j * bufferStride[1]) * nBufferDataTypeSize;
            GDALExtendedDataType::CopyValue(&abyValues[3 * idx + comp], srcDt,
                                            pDst, bufferDataType);
        }
    }

    return true;
}

/************************************************************************/
/*                           OpenMultiDim()                             */
/************************************************************************/

void HDF4Dataset::OpenMultiDim(const char* pszFilename,
                               CSLConstList papszOpenOptionsIn)
{
    // under hHDF4Mutex

    auto poShared = std::make_shared<HDF4SharedResources>(pszFilename);
    poShared->m_hSD = hSD;
    poShared->m_aosOpenOptions = papszOpenOptionsIn;

    hSD = -1;

    m_poRootGroup = std::make_shared<HDF4Group>(std::string(), "/", poShared);

    SetDescription(pszFilename);

    // Setup/check for pam .aux.xml.
    TryLoadXML();
}
