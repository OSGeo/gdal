 /******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  MEM driver multidimensional classes
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2021, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef MEMMULTIDIM_H
#define MEMMULTIDIM_H

#include "gdal_priv.h"

// If modifying the below declaration, modify it in gdal_array.i too
std::shared_ptr<GDALMDArray> CPL_DLL MEMGroupCreateMDArray(GDALGroup* poGroup,
                                                   const std::string& osName,
                                                   const std::vector<std::shared_ptr<GDALDimension>>& aoDimensions,
                                                   const GDALExtendedDataType& oDataType,
                                                   void* pData,
                                                   CSLConstList papszOptions);

/************************************************************************/
/*                               MEMGroup                               */
/************************************************************************/

class CPL_DLL MEMGroup CPL_NON_FINAL: public GDALGroup
{
    std::map<CPLString, std::shared_ptr<GDALGroup>> m_oMapGroups{};
    std::map<CPLString, std::shared_ptr<GDALMDArray>> m_oMapMDArrays{};
    std::map<CPLString, std::shared_ptr<GDALAttribute>> m_oMapAttributes{};
    std::map<CPLString, std::shared_ptr<GDALDimension>> m_oMapDimensions{};

public:
    MEMGroup(const std::string& osParentName, const char* pszName): GDALGroup(osParentName, pszName ? pszName : "") {}

    std::vector<std::string> GetMDArrayNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALMDArray> OpenMDArray(const std::string& osName,
                                             CSLConstList papszOptions) const override;

    std::vector<std::string> GetGroupNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALGroup> OpenGroup(const std::string& osName,
                                         CSLConstList papszOptions) const override;

    std::shared_ptr<GDALGroup> CreateGroup(const std::string& osName,
                                           CSLConstList papszOptions) override;

    std::shared_ptr<GDALDimension> CreateDimension(const std::string&,
                                                   const std::string&,
                                                   const std::string&,
                                                   GUInt64,
                                                   CSLConstList papszOptions) override;

    std::shared_ptr<GDALMDArray> CreateMDArray(const std::string& osName,
                                                       const std::vector<std::shared_ptr<GDALDimension>>& aoDimensions,
                                                       const GDALExtendedDataType& oDataType,
                                                       CSLConstList papszOptions) override;

    std::shared_ptr<GDALMDArray> CreateMDArray(const std::string& osName,
                                                       const std::vector<std::shared_ptr<GDALDimension>>& aoDimensions,
                                                       const GDALExtendedDataType& oDataType,
                                                       void* pData,
                                                       CSLConstList papszOptions);

    std::shared_ptr<GDALAttribute> GetAttribute(const std::string& osName) const override;

    std::vector<std::shared_ptr<GDALAttribute>> GetAttributes(CSLConstList papszOptions) const override;

    std::vector<std::shared_ptr<GDALDimension>> GetDimensions(CSLConstList papszOptions) const override;

    std::shared_ptr<GDALAttribute> CreateAttribute(
        const std::string& osName,
        const std::vector<GUInt64>& anDimensions,
        const GDALExtendedDataType& oDataType,
        CSLConstList papszOptions) override;
};

/************************************************************************/
/*                            MEMAbstractMDArray                        */
/************************************************************************/

class CPL_DLL MEMAbstractMDArray: virtual public GDALAbstractMDArray
{
    std::vector<std::shared_ptr<GDALDimension>> m_aoDims;
    size_t m_nTotalSize = 0;
    GByte* m_pabyArray{};
    bool m_bOwnArray = false;
    std::vector<GPtrDiff_t> m_anStrides{};

    struct StackReadWrite
    {
        size_t       nIters = 0;
        const GByte* src_ptr = nullptr;
        GByte*       dst_ptr = nullptr;
        GPtrDiff_t   src_inc_offset = 0;
        GPtrDiff_t   dst_inc_offset = 0;
    };

    void ReadWrite(bool bIsWrite,
                   const size_t* count,
                    std::vector<StackReadWrite>& stack,
                    const GDALExtendedDataType& srcType,
                    const GDALExtendedDataType& dstType) const;

    MEMAbstractMDArray(const MEMAbstractMDArray&) = delete;
    MEMAbstractMDArray& operator= (const MEMAbstractMDArray&) = delete;

protected:
    GDALExtendedDataType m_oType;

    bool IRead(const GUInt64* arrayStartIdx,     // array of size GetDimensionCount()
                      const size_t* count,                 // array of size GetDimensionCount()
                      const GInt64* arrayStep,        // step in elements
                      const GPtrDiff_t* bufferStride, // stride in elements
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer) const override;

    bool IWrite(const GUInt64* arrayStartIdx,     // array of size GetDimensionCount()
                      const size_t* count,                 // array of size GetDimensionCount()
                      const GInt64* arrayStep,        // step in elements
                      const GPtrDiff_t* bufferStride, // stride in elements
                      const GDALExtendedDataType& bufferDataType,
                      const void* pSrcBuffer) override;

    bool m_bWritable = true;
    bool m_bModified = false;

public:
    MEMAbstractMDArray(const std::string& osParentName,
                       const std::string& osName,
                       const std::vector<std::shared_ptr<GDALDimension>>& aoDimensions,
                       const GDALExtendedDataType& oType);
    ~MEMAbstractMDArray();

    const std::vector<std::shared_ptr<GDALDimension>>& GetDimensions() const override { return m_aoDims; }

    const GDALExtendedDataType& GetDataType() const override { return m_oType; }

    bool Init(GByte* pData = nullptr,
              const std::vector<GPtrDiff_t>& anStrides = std::vector<GPtrDiff_t>());

    void SetWritable(bool bWritable) { m_bWritable = bWritable; }

    bool IsModified() const { return m_bModified; }

    void SetModified(bool bModified) { m_bModified = bModified; }
};

/************************************************************************/
/*                                MEMMDArray                            */
/************************************************************************/

#ifdef _MSC_VER
#pragma warning (push)
#pragma warning (disable:4250) // warning C4250: 'MEMMDArray': inherits 'MEMAbstractMDArray::MEMAbstractMDArray::IRead' via dominance
#endif //_MSC_VER

class MEMMDArray CPL_NON_FINAL: public MEMAbstractMDArray, public GDALMDArray
{
    std::map<CPLString, std::shared_ptr<GDALAttribute>> m_oMapAttributes{};
    std::string m_osUnit{};
    std::shared_ptr<OGRSpatialReference> m_poSRS{};
    GByte* m_pabyNoData = nullptr;
    double m_dfScale = 1.0;
    double m_dfOffset = 0.0;
    bool m_bHasScale = false;
    bool m_bHasOffset = false;
    GDALDataType m_eOffsetStorageType = GDT_Unknown;
    GDALDataType m_eScaleStorageType = GDT_Unknown;
    std::string m_osFilename{};

    MEMMDArray(const MEMMDArray&) = delete;
    MEMMDArray& operator= (const MEMMDArray&) = delete;

protected:
    MEMMDArray(const std::string& osParentName,
               const std::string& osName,
               const std::vector<std::shared_ptr<GDALDimension>>& aoDimensions,
               const GDALExtendedDataType& oType);

public:
    // MEMAbstractMDArray::Init() should be called afterwards
    static std::shared_ptr<MEMMDArray> Create(const std::string& osParentName,
               const std::string& osName,
               const std::vector<std::shared_ptr<GDALDimension>>& aoDimensions,
               const GDALExtendedDataType& oType)
    {
        auto array(std::shared_ptr<MEMMDArray>(
            new MEMMDArray(osParentName, osName, aoDimensions, oType)));
        array->SetSelf(array);
        return array;
    }
    ~MEMMDArray();

    bool IsWritable() const override { return m_bWritable; }

    const std::string& GetFilename() const override { return m_osFilename; }

    std::shared_ptr<GDALAttribute> GetAttribute(const std::string& osName) const override;

    std::vector<std::shared_ptr<GDALAttribute>> GetAttributes(CSLConstList papszOptions) const override;

    std::shared_ptr<GDALAttribute> CreateAttribute(
        const std::string& osName,
        const std::vector<GUInt64>& anDimensions,
        const GDALExtendedDataType& oDataType,
        CSLConstList papszOptions) override;

    const std::string& GetUnit() const override { return m_osUnit; }

    bool SetUnit(const std::string& osUnit) override {
        m_osUnit = osUnit; return true; }

    bool SetSpatialRef(const OGRSpatialReference* poSRS) override {
        m_poSRS.reset(poSRS ? poSRS->Clone() : nullptr); return true; }

    std::shared_ptr<OGRSpatialReference> GetSpatialRef() const override { return m_poSRS; }

    const void* GetRawNoDataValue() const override;

    bool SetRawNoDataValue(const void*) override;

    double GetOffset(bool* pbHasOffset, GDALDataType* peStorageType) const override
    {
        if( pbHasOffset) *pbHasOffset = m_bHasOffset;
        if( peStorageType ) *peStorageType = m_eOffsetStorageType;
        return m_dfOffset;
    }

    double GetScale(bool* pbHasScale, GDALDataType* peStorageType) const override
    {
        if( pbHasScale) *pbHasScale = m_bHasScale;
        if( peStorageType ) *peStorageType = m_eScaleStorageType;
        return m_dfScale;
    }

    bool SetOffset(double dfOffset, GDALDataType eStorageType) override
    { m_bHasOffset = true; m_dfOffset = dfOffset; m_eOffsetStorageType = eStorageType; return true; }

    bool SetScale(double dfScale, GDALDataType eStorageType) override
    { m_bHasScale = true; m_dfScale = dfScale; m_eScaleStorageType = eStorageType; return true; }
};

/************************************************************************/
/*                               MEMAttribute                           */
/************************************************************************/

class MEMAttribute CPL_NON_FINAL: public MEMAbstractMDArray, public GDALAttribute
{
protected:
    MEMAttribute(const std::string& osParentName,
                 const std::string& osName,
                 const std::vector<GUInt64>& anDimensions,
                 const GDALExtendedDataType& oType);
public:
    // May return nullptr as it calls MEMAbstractMDArray::Init() which can
    // fail
    static std::shared_ptr<MEMAttribute> Create(const std::string& osParentName,
                                                const std::string& osName,
                                                const std::vector<GUInt64>& anDimensions,
                                                const GDALExtendedDataType& oType);
};

#ifdef _MSC_VER
#pragma warning (pop)
#endif //_MSC_VER

/************************************************************************/
/*                               MEMDimension                           */
/************************************************************************/

class MEMDimension CPL_NON_FINAL: public GDALDimension
{
    std::weak_ptr<GDALMDArray> m_poIndexingVariable{};

public:
    MEMDimension(const std::string& osParentName,
                 const std::string& osName,
                 const std::string& osType,
                 const std::string& osDirection,
                 GUInt64 nSize);

    std::shared_ptr<GDALMDArray> GetIndexingVariable() const override { return m_poIndexingVariable.lock(); }

    bool SetIndexingVariable(std::shared_ptr<GDALMDArray> poIndexingVariable) override;
};

#endif //  MEMMULTIDIM_H
