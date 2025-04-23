/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements reading of FileGDB indexes
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "filegdbtable_priv.h"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_mem_cache.h"
#include "cpl_noncopyablevector.h"
#include "cpl_string.h"
#include "cpl_time.h"
#include "cpl_vsi.h"
#include "ogr_core.h"
#include "filegdbtable.h"

namespace OpenFileGDB
{

/************************************************************************/
/*                    GetFieldNameFromExpression()                      */
/************************************************************************/

std::string
FileGDBIndex::GetFieldNameFromExpression(const std::string &osExpression)
{
    if (STARTS_WITH_CI(osExpression.c_str(), "LOWER(") &&
        osExpression.back() == ')')
        return osExpression.substr(strlen("LOWER("),
                                   osExpression.size() - strlen("LOWER()"));
    return osExpression;
}

/************************************************************************/
/*                           GetFieldName()                             */
/************************************************************************/

std::string FileGDBIndex::GetFieldName() const
{
    return GetFieldNameFromExpression(m_osExpression);
}

/************************************************************************/
/*                        FileGDBTrivialIterator                        */
/************************************************************************/

class FileGDBTrivialIterator final : public FileGDBIterator
{
    FileGDBIterator *poParentIter = nullptr;
    FileGDBTable *poTable = nullptr;
    int64_t iRow = 0;

    FileGDBTrivialIterator(const FileGDBTrivialIterator &) = delete;
    FileGDBTrivialIterator &operator=(const FileGDBTrivialIterator &) = delete;

  public:
    explicit FileGDBTrivialIterator(FileGDBIterator *poParentIter);

    virtual ~FileGDBTrivialIterator()
    {
        delete poParentIter;
    }

    virtual FileGDBTable *GetTable() override
    {
        return poTable;
    }

    virtual void Reset() override
    {
        iRow = 0;
        poParentIter->Reset();
    }

    virtual int64_t GetNextRowSortedByFID() override;

    virtual int64_t GetRowCount() override
    {
        return poTable->GetTotalRecordCount();
    }

    virtual int64_t GetNextRowSortedByValue() override
    {
        return poParentIter->GetNextRowSortedByValue();
    }

    virtual const OGRField *GetMinValue(int &eOutType) override
    {
        return poParentIter->GetMinValue(eOutType);
    }

    virtual const OGRField *GetMaxValue(int &eOutType) override
    {
        return poParentIter->GetMaxValue(eOutType);
    }

    virtual bool GetMinMaxSumCount(double &dfMin, double &dfMax, double &dfSum,
                                   int &nCount) override
    {
        return poParentIter->GetMinMaxSumCount(dfMin, dfMax, dfSum, nCount);
    }
};

/************************************************************************/
/*                        FileGDBNotIterator                            */
/************************************************************************/

class FileGDBNotIterator final : public FileGDBIterator
{
    FileGDBIterator *poIterBase = nullptr;
    FileGDBTable *poTable = nullptr;
    int64_t iRow = 0;
    int64_t iNextRowBase = -1;
    int bNoHoles = 0;

    FileGDBNotIterator(const FileGDBNotIterator &) = delete;
    FileGDBNotIterator &operator=(const FileGDBNotIterator &) = delete;

  public:
    explicit FileGDBNotIterator(FileGDBIterator *poIterBase);
    virtual ~FileGDBNotIterator();

    virtual FileGDBTable *GetTable() override
    {
        return poTable;
    }

    virtual void Reset() override;
    virtual int64_t GetNextRowSortedByFID() override;
    virtual int64_t GetRowCount() override;
};

/************************************************************************/
/*                        FileGDBAndIterator                            */
/************************************************************************/

class FileGDBAndIterator final : public FileGDBIterator
{
    FileGDBIterator *poIter1 = nullptr;
    FileGDBIterator *poIter2 = nullptr;
    int64_t iNextRow1 = -1;
    int64_t iNextRow2 = -1;
    bool m_bTakeOwnershipOfIterators = false;

    FileGDBAndIterator(const FileGDBAndIterator &) = delete;
    FileGDBAndIterator &operator=(const FileGDBAndIterator &) = delete;

  public:
    FileGDBAndIterator(FileGDBIterator *poIter1, FileGDBIterator *poIter2,
                       bool bTakeOwnershipOfIterators);
    virtual ~FileGDBAndIterator();

    virtual FileGDBTable *GetTable() override
    {
        return poIter1->GetTable();
    }

    virtual void Reset() override;
    virtual int64_t GetNextRowSortedByFID() override;
};

/************************************************************************/
/*                        FileGDBOrIterator                             */
/************************************************************************/

class FileGDBOrIterator final : public FileGDBIterator
{
    FileGDBIterator *poIter1 = nullptr;
    FileGDBIterator *poIter2 = nullptr;
    int bIteratorAreExclusive = false;
    int64_t iNextRow1 = -1;
    int64_t iNextRow2 = -1;
    bool bHasJustReset = true;

    FileGDBOrIterator(const FileGDBOrIterator &) = delete;
    FileGDBOrIterator &operator=(const FileGDBOrIterator &) = delete;

  public:
    FileGDBOrIterator(FileGDBIterator *poIter1, FileGDBIterator *poIter2,
                      int bIteratorAreExclusive = FALSE);
    virtual ~FileGDBOrIterator();

    virtual FileGDBTable *GetTable() override
    {
        return poIter1->GetTable();
    }

    virtual void Reset() override;
    virtual int64_t GetNextRowSortedByFID() override;
    virtual int64_t GetRowCount() override;
};

/************************************************************************/
/*                       FileGDBIndexIteratorBase                       */
/************************************************************************/

constexpr int MAX_DEPTH = 3;
constexpr int FGDB_PAGE_SIZE_V1 = 4096;
constexpr int FGDB_PAGE_SIZE_V2 = 65536;
constexpr int MAX_FGDB_PAGE_SIZE = FGDB_PAGE_SIZE_V2;

class FileGDBIndexIteratorBase : virtual public FileGDBIterator
{
  protected:
    FileGDBTable *poParent = nullptr;
    bool bAscending = false;
    VSILFILE *fpCurIdx = nullptr;

    //! Version of .atx/.spx: 1 or 2
    GUInt32 m_nVersion = 0;

    // Number of pages of size m_nPageSize
    GUInt32 m_nPageCount = 0;

    //! Page size in bytes: 4096 for v1 format, 65536 for v2
    int m_nPageSize = 0;

    //! Maximum number of features or sub-pages referenced by a page.
    GUInt32 nMaxPerPages = 0;

    //! Size of ObjectID referenced in pages, in bytes.
    // sizeof(uint32_t) for V1, sizeof(uint64_t) for V2
    GUInt32 m_nObjectIDSize = 0;

    //! Size of the indexed value, in bytes.
    GUInt32 m_nValueSize = 0;

    //! Non-leaf page header size in bytes. 8 for V1, 12 for V2
    GUInt32 m_nNonLeafPageHeaderSize = 0;

    //! Leaf page header size in bytes. 12 for V1, 20 for V2
    GUInt32 m_nLeafPageHeaderSize = 0;

    //! Offset within a page at which the first indexed value is found.
    GUInt32 m_nOffsetFirstValInPage = 0;

    //! Number of values referenced in the index.
    GUInt64 m_nValueCountInIdx = 0;

    GUInt32 nIndexDepth = 0;
#ifdef DEBUG
    uint64_t iLoadedPage[MAX_DEPTH];
#endif
    int iFirstPageIdx[MAX_DEPTH];
    int iLastPageIdx[MAX_DEPTH];
    int iCurPageIdx[MAX_DEPTH];
    GUInt32 nSubPagesCount[MAX_DEPTH];
    uint64_t nLastPageAccessed[MAX_DEPTH];

    int iCurFeatureInPage = -1;
    int nFeaturesInPage = 0;

    bool bEOF = false;

    GByte abyPage[MAX_DEPTH][MAX_FGDB_PAGE_SIZE];
    GByte abyPageFeature[MAX_FGDB_PAGE_SIZE];

    typedef lru11::Cache<uint64_t, cpl::NonCopyableVector<GByte>> CacheType;
    std::array<CacheType, MAX_DEPTH> m_oCachePage{
        {CacheType{2, 0}, CacheType{2, 0}, CacheType{2, 0}}};
    CacheType m_oCacheFeaturePage{2, 0};

    bool ReadTrailer(const std::string &osFilename);

    uint64_t ReadPageNumber(int iLevel);
    bool LoadNextPage(int iLevel);
    virtual bool FindPages(int iLevel, uint64_t nPage) = 0;
    bool LoadNextFeaturePage();

    FileGDBIndexIteratorBase(FileGDBTable *poParent, int bAscending);

    FileGDBIndexIteratorBase(const FileGDBIndexIteratorBase &) = delete;
    FileGDBIndexIteratorBase &
    operator=(const FileGDBIndexIteratorBase &) = delete;

  public:
    virtual ~FileGDBIndexIteratorBase();

    virtual FileGDBTable *GetTable() override
    {
        return poParent;
    }

    virtual void Reset() override;
};

/************************************************************************/
/*                        FileGDBIndexIterator                          */
/************************************************************************/

constexpr int UUID_LEN_AS_STRING = 38;
constexpr int MAX_UTF8_LEN_STR = 4 * MAX_CAR_COUNT_INDEXED_STR;

class FileGDBIndexIterator final : public FileGDBIndexIteratorBase
{
    FileGDBFieldType eFieldType = FGFT_UNDEFINED;
    FileGDBSQLOp eOp = FGSO_ISNOTNULL;
    OGRField sValue{};

    bool bEvaluateToFALSE = false;

    int iSorted = 0;
    int nSortedCount = -1;
    int64_t *panSortedRows = nullptr;
    int SortRows();

    GUInt16 asUTF16Str[MAX_CAR_COUNT_INDEXED_STR];
    int nStrLen = 0;
    char szUUID[UUID_LEN_AS_STRING + 1];

    OGRField sMin{};
    OGRField sMax{};
    char szMin[MAX_UTF8_LEN_STR + 1];
    char szMax[MAX_UTF8_LEN_STR + 1];
    const OGRField *GetMinMaxValue(OGRField *psField, int &eOutType,
                                   int bIsMin);

    virtual bool FindPages(int iLevel, uint64_t nPage) override;
    int64_t GetNextRow();

    FileGDBIndexIterator(FileGDBTable *poParent, int bAscending);
    int SetConstraint(int nFieldIdx, FileGDBSQLOp op,
                      OGRFieldType eOGRFieldType, const OGRField *psValue);

    template <class Getter>
    void GetMinMaxSumCount(double &dfMin, double &dfMax, double &dfSum,
                           int &nCount);

    FileGDBIndexIterator(const FileGDBIndexIterator &) = delete;
    FileGDBIndexIterator &operator=(const FileGDBIndexIterator &) = delete;

  public:
    virtual ~FileGDBIndexIterator();

    static FileGDBIterator *Build(FileGDBTable *poParentIn, int nFieldIdx,
                                  int bAscendingIn, FileGDBSQLOp op,
                                  OGRFieldType eOGRFieldType,
                                  const OGRField *psValue);

    virtual int64_t GetNextRowSortedByFID() override;
    virtual int64_t GetRowCount() override;
    virtual void Reset() override;

    virtual int64_t GetNextRowSortedByValue() override
    {
        return GetNextRow();
    }

    virtual const OGRField *GetMinValue(int &eOutType) override;
    virtual const OGRField *GetMaxValue(int &eOutType) override;
    virtual bool GetMinMaxSumCount(double &dfMin, double &dfMax, double &dfSum,
                                   int &nCount) override;
};

/************************************************************************/
/*                            GetMinValue()                             */
/************************************************************************/

const OGRField *FileGDBIterator::GetMinValue(int &eOutType)
{
    PrintError();
    eOutType = -1;
    return nullptr;
}

/************************************************************************/
/*                            GetMaxValue()                             */
/************************************************************************/

const OGRField *FileGDBIterator::GetMaxValue(int &eOutType)
{
    PrintError();
    eOutType = -1;
    return nullptr;
}

/************************************************************************/
/*                       GetNextRowSortedByValue()                      */
/************************************************************************/

int64_t FileGDBIterator::GetNextRowSortedByValue()
{
    PrintError();
    return -1;
}

/************************************************************************/
/*                        GetMinMaxSumCount()                           */
/************************************************************************/

bool FileGDBIterator::GetMinMaxSumCount(double &dfMin, double &dfMax,
                                        double &dfSum, int &nCount)
{
    PrintError();
    dfMin = 0.0;
    dfMax = 0.0;
    dfSum = 0.0;
    nCount = 0;
    return false;
}

/************************************************************************/
/*                             Build()                                  */
/************************************************************************/

FileGDBIterator *FileGDBIterator::Build(FileGDBTable *poParent, int nFieldIdx,
                                        int bAscending, FileGDBSQLOp op,
                                        OGRFieldType eOGRFieldType,
                                        const OGRField *psValue)
{
    return FileGDBIndexIterator::Build(poParent, nFieldIdx, bAscending, op,
                                       eOGRFieldType, psValue);
}

/************************************************************************/
/*                           BuildIsNotNull()                           */
/************************************************************************/

FileGDBIterator *FileGDBIterator::BuildIsNotNull(FileGDBTable *poParent,
                                                 int nFieldIdx, int bAscending)
{
    FileGDBIterator *poIter = Build(poParent, nFieldIdx, bAscending,
                                    FGSO_ISNOTNULL, OFTMaxType, nullptr);
    if (poIter != nullptr)
    {
        /* Optimization */
        if (poIter->GetRowCount() == poParent->GetTotalRecordCount())
        {
            CPLAssert(poParent->GetValidRecordCount() ==
                      poParent->GetTotalRecordCount());
            poIter = new FileGDBTrivialIterator(poIter);
        }
    }
    return poIter;
}

/************************************************************************/
/*                              BuildNot()                              */
/************************************************************************/

FileGDBIterator *FileGDBIterator::BuildNot(FileGDBIterator *poIterBase)
{
    return new FileGDBNotIterator(poIterBase);
}

/************************************************************************/
/*                               BuildAnd()                             */
/************************************************************************/

FileGDBIterator *FileGDBIterator::BuildAnd(FileGDBIterator *poIter1,
                                           FileGDBIterator *poIter2,
                                           bool bTakeOwnershipOfIterators)
{
    return new FileGDBAndIterator(poIter1, poIter2, bTakeOwnershipOfIterators);
}

/************************************************************************/
/*                               BuildOr()                              */
/************************************************************************/

FileGDBIterator *FileGDBIterator::BuildOr(FileGDBIterator *poIter1,
                                          FileGDBIterator *poIter2,
                                          int bIteratorAreExclusive)
{
    return new FileGDBOrIterator(poIter1, poIter2, bIteratorAreExclusive);
}

/************************************************************************/
/*                           GetRowCount()                              */
/************************************************************************/

int64_t FileGDBIterator::GetRowCount()
{
    Reset();
    int64_t nCount = 0;
    while (GetNextRowSortedByFID() >= 0)
        nCount++;
    Reset();
    return nCount;
}

/************************************************************************/
/*                         FileGDBTrivialIterator()                     */
/************************************************************************/

FileGDBTrivialIterator::FileGDBTrivialIterator(FileGDBIterator *poParentIterIn)
    : poParentIter(poParentIterIn), poTable(poParentIterIn->GetTable())
{
}

/************************************************************************/
/*                        GetNextRowSortedByFID()                       */
/************************************************************************/

int64_t FileGDBTrivialIterator::GetNextRowSortedByFID()
{
    if (iRow < poTable->GetTotalRecordCount())
        return iRow++;
    else
        return -1;
}

/************************************************************************/
/*                           FileGDBNotIterator()                       */
/************************************************************************/

FileGDBNotIterator::FileGDBNotIterator(FileGDBIterator *poIterBaseIn)
    : poIterBase(poIterBaseIn), poTable(poIterBaseIn->GetTable())
{
    bNoHoles =
        (poTable->GetValidRecordCount() == poTable->GetTotalRecordCount());
}

/************************************************************************/
/*                          ~FileGDBNotIterator()                       */
/************************************************************************/

FileGDBNotIterator::~FileGDBNotIterator()
{
    delete poIterBase;
}

/************************************************************************/
/*                             Reset()                                  */
/************************************************************************/

void FileGDBNotIterator::Reset()
{
    poIterBase->Reset();
    iRow = 0;
    iNextRowBase = -1;
}

/************************************************************************/
/*                        GetNextRowSortedByFID()                       */
/************************************************************************/

int64_t FileGDBNotIterator::GetNextRowSortedByFID()
{
    if (iNextRowBase < 0)
    {
        iNextRowBase = poIterBase->GetNextRowSortedByFID();
        if (iNextRowBase < 0)
            iNextRowBase = poTable->GetTotalRecordCount();
    }

    while (true)
    {
        if (iRow < iNextRowBase)
        {
            if (bNoHoles)
                return iRow++;
            else if (poTable->GetOffsetInTableForRow(iRow))
                return iRow++;
            else if (!poTable->HasGotError())
                iRow++;
            else
                return -1;
        }
        else if (iRow == poTable->GetTotalRecordCount())
            return -1;
        else
        {
            iRow = iNextRowBase + 1;
            iNextRowBase = poIterBase->GetNextRowSortedByFID();
            if (iNextRowBase < 0)
                iNextRowBase = poTable->GetTotalRecordCount();
        }
    }
}

/************************************************************************/
/*                           GetRowCount()                              */
/************************************************************************/

int64_t FileGDBNotIterator::GetRowCount()
{
    return poTable->GetValidRecordCount() - poIterBase->GetRowCount();
}

/************************************************************************/
/*                          FileGDBAndIterator()                        */
/************************************************************************/

FileGDBAndIterator::FileGDBAndIterator(FileGDBIterator *poIter1In,
                                       FileGDBIterator *poIter2In,
                                       bool bTakeOwnershipOfIterators)
    : poIter1(poIter1In), poIter2(poIter2In), iNextRow1(-1), iNextRow2(-1),
      m_bTakeOwnershipOfIterators(bTakeOwnershipOfIterators)
{
    CPLAssert(poIter1->GetTable() == poIter2->GetTable());
}

/************************************************************************/
/*                          ~FileGDBAndIterator()                       */
/************************************************************************/

FileGDBAndIterator::~FileGDBAndIterator()
{
    if (m_bTakeOwnershipOfIterators)
    {
        delete poIter1;
        delete poIter2;
    }
}

/************************************************************************/
/*                             Reset()                                  */
/************************************************************************/

void FileGDBAndIterator::Reset()
{
    poIter1->Reset();
    poIter2->Reset();
    iNextRow1 = -1;
    iNextRow2 = -1;
}

/************************************************************************/
/*                        GetNextRowSortedByFID()                       */
/************************************************************************/

int64_t FileGDBAndIterator::GetNextRowSortedByFID()
{
    if (iNextRow1 == iNextRow2)
    {
        iNextRow1 = poIter1->GetNextRowSortedByFID();
        iNextRow2 = poIter2->GetNextRowSortedByFID();
        if (iNextRow1 < 0 || iNextRow2 < 0)
        {
            return -1;
        }
    }

    while (true)
    {
        if (iNextRow1 < iNextRow2)
        {
            iNextRow1 = poIter1->GetNextRowSortedByFID();
            if (iNextRow1 < 0)
                return -1;
        }
        else if (iNextRow2 < iNextRow1)
        {
            iNextRow2 = poIter2->GetNextRowSortedByFID();
            if (iNextRow2 < 0)
                return -1;
        }
        else
            return iNextRow1;
    }
}

/************************************************************************/
/*                          FileGDBOrIterator()                         */
/************************************************************************/

FileGDBOrIterator::FileGDBOrIterator(FileGDBIterator *poIter1In,
                                     FileGDBIterator *poIter2In,
                                     int bIteratorAreExclusiveIn)
    : poIter1(poIter1In), poIter2(poIter2In),
      bIteratorAreExclusive(bIteratorAreExclusiveIn)
{
    CPLAssert(poIter1->GetTable() == poIter2->GetTable());
}

/************************************************************************/
/*                          ~FileGDBOrIterator()                        */
/************************************************************************/

FileGDBOrIterator::~FileGDBOrIterator()
{
    delete poIter1;
    delete poIter2;
}

/************************************************************************/
/*                             Reset()                                  */
/************************************************************************/

void FileGDBOrIterator::Reset()
{
    poIter1->Reset();
    poIter2->Reset();
    iNextRow1 = -1;
    iNextRow2 = -1;
    bHasJustReset = true;
}

/************************************************************************/
/*                        GetNextRowSortedByFID()                       */
/************************************************************************/

int64_t FileGDBOrIterator::GetNextRowSortedByFID()
{
    if (bHasJustReset)
    {
        bHasJustReset = false;
        iNextRow1 = poIter1->GetNextRowSortedByFID();
        iNextRow2 = poIter2->GetNextRowSortedByFID();
    }

    if (iNextRow1 < 0)
    {
        auto iVal = iNextRow2;
        iNextRow2 = poIter2->GetNextRowSortedByFID();
        return iVal;
    }
    if (iNextRow2 < 0 || iNextRow1 < iNextRow2)
    {
        auto iVal = iNextRow1;
        iNextRow1 = poIter1->GetNextRowSortedByFID();
        return iVal;
    }
    if (iNextRow2 < iNextRow1)
    {
        auto iVal = iNextRow2;
        iNextRow2 = poIter2->GetNextRowSortedByFID();
        return iVal;
    }

    if (bIteratorAreExclusive)
        PrintError();

    auto iVal = iNextRow1;
    iNextRow1 = poIter1->GetNextRowSortedByFID();
    iNextRow2 = poIter2->GetNextRowSortedByFID();
    return iVal;
}

/************************************************************************/
/*                           GetRowCount()                              */
/************************************************************************/

int64_t FileGDBOrIterator::GetRowCount()
{
    if (bIteratorAreExclusive)
        return poIter1->GetRowCount() + poIter2->GetRowCount();
    else
        return FileGDBIterator::GetRowCount();
}

/************************************************************************/
/*                     FileGDBIndexIteratorBase()                       */
/************************************************************************/

FileGDBIndexIteratorBase::FileGDBIndexIteratorBase(FileGDBTable *poParentIn,
                                                   int bAscendingIn)
    : poParent(poParentIn), bAscending(CPL_TO_BOOL(bAscendingIn))
{
#ifdef DEBUG
    memset(&iLoadedPage, 0, sizeof(iLoadedPage));
#endif
    memset(&iFirstPageIdx, 0xFF, sizeof(iFirstPageIdx));
    memset(&iLastPageIdx, 0xFF, sizeof(iFirstPageIdx));
    memset(&iCurPageIdx, 0xFF, sizeof(iCurPageIdx));
    memset(&nSubPagesCount, 0, sizeof(nSubPagesCount));
    memset(&nLastPageAccessed, 0, sizeof(nLastPageAccessed));
    memset(&abyPage, 0, sizeof(abyPage));
    memset(&abyPageFeature, 0, sizeof(abyPageFeature));
}

/************************************************************************/
/*                       ~FileGDBIndexIteratorBase()                    */
/************************************************************************/

FileGDBIndexIteratorBase::~FileGDBIndexIteratorBase()
{
    if (fpCurIdx)
        VSIFCloseL(fpCurIdx);
    fpCurIdx = nullptr;
}

/************************************************************************/
/*                           ReadTrailer()                              */
/************************************************************************/

bool FileGDBIndexIteratorBase::ReadTrailer(const std::string &osFilename)
{
    const bool errorRetValue = false;

    fpCurIdx = VSIFOpenL(osFilename.c_str(), "rb");
    returnErrorIf(fpCurIdx == nullptr);

    VSIFSeekL(fpCurIdx, 0, SEEK_END);
    vsi_l_offset nFileSize = VSIFTellL(fpCurIdx);
    constexpr int V1_TRAILER_SIZE = 22;
    constexpr int V2_TRAILER_SIZE = 30;
    returnErrorIf(nFileSize < V1_TRAILER_SIZE);

    GByte abyTrailer[V2_TRAILER_SIZE];
    VSIFSeekL(fpCurIdx, nFileSize - sizeof(uint32_t), SEEK_SET);
    returnErrorIf(VSIFReadL(abyTrailer, sizeof(uint32_t), 1, fpCurIdx) != 1);
    m_nVersion = GetUInt32(abyTrailer, 0);
    returnErrorIf(m_nVersion != 1 && m_nVersion != 2);

    if (m_nVersion == 1)
    {
        m_nPageSize = FGDB_PAGE_SIZE_V1;
        VSIFSeekL(fpCurIdx, nFileSize - V1_TRAILER_SIZE, SEEK_SET);
        returnErrorIf(VSIFReadL(abyTrailer, V1_TRAILER_SIZE, 1, fpCurIdx) != 1);

        m_nPageCount =
            static_cast<GUInt32>((nFileSize - V1_TRAILER_SIZE) / m_nPageSize);

        m_nValueSize = abyTrailer[0];
        m_nObjectIDSize = static_cast<uint32_t>(sizeof(uint32_t));
        m_nNonLeafPageHeaderSize = 8;
        m_nLeafPageHeaderSize = 12;

        nMaxPerPages = (m_nPageSize - m_nLeafPageHeaderSize) /
                       (m_nObjectIDSize + m_nValueSize);
        m_nOffsetFirstValInPage =
            m_nLeafPageHeaderSize + nMaxPerPages * m_nObjectIDSize;

        GUInt32 nMagic1 = GetUInt32(abyTrailer + 2, 0);
        returnErrorIf(nMagic1 != 1);

        nIndexDepth = GetUInt32(abyTrailer + 6, 0);
        /* CPLDebug("OpenFileGDB", "nIndexDepth = %u", nIndexDepth); */
        returnErrorIf(!(nIndexDepth >= 1 && nIndexDepth <= MAX_DEPTH + 1));

        m_nValueCountInIdx = GetUInt32(abyTrailer + 10, 0);
        /* CPLDebug("OpenFileGDB", "m_nValueCountInIdx = %u", m_nValueCountInIdx); */
        /* negative like in sample_clcV15_esri_v10.gdb/a00000005.FDO_UUID.atx */
        if ((m_nValueCountInIdx >> (8 * sizeof(m_nValueCountInIdx) - 1)) != 0)
        {
            CPLDebugOnly("OpenFileGDB", "m_nValueCountInIdx=%u",
                         static_cast<uint32_t>(m_nValueCountInIdx));
            return false;
        }

        /* QGIS_TEST_101.gdb/a00000006.FDO_UUID.atx */
        /* or .spx file from test dataset https://github.com/OSGeo/gdal/issues/5888
         */
        if (m_nValueCountInIdx == 0 && nIndexDepth == 1)
        {
            VSIFSeekL(fpCurIdx, 4, SEEK_SET);
            GByte abyBuffer[4];
            returnErrorIf(VSIFReadL(abyBuffer, 4, 1, fpCurIdx) != 1);
            m_nValueCountInIdx = GetUInt32(abyBuffer, 0);
        }
        /* PreNIS.gdb/a00000006.FDO_UUID.atx has depth 2 and the value of */
        /* m_nValueCountInIdx is 11 which is not the number of non-null values */
        else if (m_nValueCountInIdx < nMaxPerPages && nIndexDepth > 1)
        {
            if (m_nValueCountInIdx > 0 && poParent->IsFileGDBV9() &&
                strstr(osFilename.c_str(), "blk_key_index.atx"))
            {
                // m_nValueCountInIdx not reliable in FileGDB v9 .blk_key_index.atx
                // but index seems to be OK
                return true;
            }

            CPLDebugOnly(
                "OpenFileGDB",
                "m_nValueCountInIdx=%u < nMaxPerPages=%u, nIndexDepth=%u",
                static_cast<uint32_t>(m_nValueCountInIdx), nMaxPerPages,
                nIndexDepth);
            return false;
        }
    }
    else
    {
        m_nPageSize = FGDB_PAGE_SIZE_V2;
        VSIFSeekL(fpCurIdx, nFileSize - V2_TRAILER_SIZE, SEEK_SET);
        returnErrorIf(VSIFReadL(abyTrailer, V2_TRAILER_SIZE, 1, fpCurIdx) != 1);

        m_nPageCount =
            static_cast<GUInt32>((nFileSize - V2_TRAILER_SIZE) / m_nPageSize);

        m_nValueSize = abyTrailer[0];
        m_nObjectIDSize = static_cast<uint32_t>(sizeof(uint64_t));
        m_nNonLeafPageHeaderSize = 12;
        m_nLeafPageHeaderSize = 20;

        nMaxPerPages = (m_nPageSize - m_nLeafPageHeaderSize) /
                       (m_nObjectIDSize + m_nValueSize);
        m_nOffsetFirstValInPage =
            m_nLeafPageHeaderSize + nMaxPerPages * m_nObjectIDSize;

        GUInt32 nMagic1 = GetUInt32(abyTrailer + 2, 0);
        returnErrorIf(nMagic1 != 1);

        nIndexDepth = GetUInt32(abyTrailer + 6, 0);
        /* CPLDebug("OpenFileGDB", "nIndexDepth = %u", nIndexDepth); */
        returnErrorIf(!(nIndexDepth >= 1 && nIndexDepth <= MAX_DEPTH + 1));

        m_nValueCountInIdx = GetUInt64(abyTrailer + 10, 0);
    }

    return true;
}

/************************************************************************/
/*                         FileGDBIndexIterator()                       */
/************************************************************************/

FileGDBIndexIterator::FileGDBIndexIterator(FileGDBTable *poParentIn,
                                           int bAscendingIn)
    : FileGDBIndexIteratorBase(poParentIn, bAscendingIn), nStrLen(0)
{
    memset(&sValue, 0, sizeof(sValue));
    memset(&asUTF16Str, 0, sizeof(asUTF16Str));
    memset(&szUUID, 0, sizeof(szUUID));
    memset(&sMin, 0, sizeof(sMin));
    memset(&sMax, 0, sizeof(sMax));
    memset(&szMin, 0, sizeof(szMin));
    memset(&szMax, 0, sizeof(szMax));
}

/************************************************************************/
/*                         ~FileGDBIndexIterator()                      */
/************************************************************************/

FileGDBIndexIterator::~FileGDBIndexIterator()
{
    VSIFree(panSortedRows);
}

/************************************************************************/
/*                             Build()                                  */
/************************************************************************/

FileGDBIterator *FileGDBIndexIterator::Build(FileGDBTable *poParentIn,
                                             int nFieldIdx, int bAscendingIn,
                                             FileGDBSQLOp op,
                                             OGRFieldType eOGRFieldType,
                                             const OGRField *psValue)
{
    FileGDBIndexIterator *poIndexIterator =
        new FileGDBIndexIterator(poParentIn, bAscendingIn);
    if (poIndexIterator->SetConstraint(nFieldIdx, op, eOGRFieldType, psValue))
    {
        return poIndexIterator;
    }
    delete poIndexIterator;
    return nullptr;
}

/************************************************************************/
/*                           FileGDBSQLOpToStr()                        */
/************************************************************************/

static const char *FileGDBSQLOpToStr(FileGDBSQLOp op)
{
    switch (op)
    {
        case FGSO_ISNOTNULL:
            return "IS NOT NULL";
        case FGSO_LT:
            return "<";
        case FGSO_LE:
            return "<=";
        case FGSO_EQ:
            return "=";
        case FGSO_GE:
            return ">=";
        case FGSO_GT:
            return ">";
        case FGSO_ILIKE:
            return "ILIKE";
    }
    return "unknown_op";
}

/************************************************************************/
/*                           FileGDBValueToStr()                        */
/************************************************************************/

static const char *FileGDBValueToStr(OGRFieldType eOGRFieldType,
                                     const OGRField *psValue)
{
    if (psValue == nullptr)
        return "";

    switch (eOGRFieldType)
    {
        case OFTInteger:
            return CPLSPrintf("%d", psValue->Integer);
        case OFTReal:
            return CPLSPrintf("%.17g", psValue->Real);
        case OFTString:
            return psValue->String;
        case OFTDateTime:
            return CPLSPrintf(
                "%04d/%02d/%02d %02d:%02d:%02d", psValue->Date.Year,
                psValue->Date.Month, psValue->Date.Day, psValue->Date.Hour,
                psValue->Date.Minute, static_cast<int>(psValue->Date.Second));
        case OFTDate:
            return CPLSPrintf("%04d/%02d/%02d", psValue->Date.Year,
                              psValue->Date.Month, psValue->Date.Day);
        case OFTTime:
            return CPLSPrintf("%02d:%02d:%02d", psValue->Date.Hour,
                              psValue->Date.Minute,
                              static_cast<int>(psValue->Date.Second));
        default:
            break;
    }
    return "";
}

/************************************************************************/
/*                          GetMaxWidthInBytes()                        */
/************************************************************************/

int FileGDBIndex::GetMaxWidthInBytes(const FileGDBTable *poTable) const
{
    const std::string osAtxName = CPLResetExtensionSafe(
        poTable->GetFilename().c_str(), (GetIndexName() + ".atx").c_str());
    VSILFILE *fpCurIdx = VSIFOpenL(osAtxName.c_str(), "rb");
    if (fpCurIdx == nullptr)
        return 0;

    VSIFSeekL(fpCurIdx, 0, SEEK_END);
    vsi_l_offset nFileSize = VSIFTellL(fpCurIdx);

    constexpr int V1_TRAILER_SIZE = 22;
    constexpr int V2_TRAILER_SIZE = 30;

    if (nFileSize < FGDB_PAGE_SIZE_V1 + V1_TRAILER_SIZE)
    {
        VSIFCloseL(fpCurIdx);
        return 0;
    }

    GByte abyTrailer[V2_TRAILER_SIZE];
    VSIFSeekL(fpCurIdx, nFileSize - sizeof(uint32_t), SEEK_SET);
    if (VSIFReadL(abyTrailer, sizeof(uint32_t), 1, fpCurIdx) != 1)
    {
        VSIFCloseL(fpCurIdx);
        return 0;
    }
    const auto nVersion = GetUInt32(abyTrailer, 0);
    if (nVersion != 1 && nVersion != 2)
    {
        VSIFCloseL(fpCurIdx);
        return 0;
    }

    const int nTrailerSize = nVersion == 1 ? V1_TRAILER_SIZE : V2_TRAILER_SIZE;

    if (nVersion == 2 && nFileSize < FGDB_PAGE_SIZE_V2 + V2_TRAILER_SIZE)
    {
        VSIFCloseL(fpCurIdx);
        return 0;
    }

    VSIFSeekL(fpCurIdx, nFileSize - nTrailerSize, SEEK_SET);
    if (VSIFReadL(abyTrailer, nTrailerSize, 1, fpCurIdx) != 1)
    {
        VSIFCloseL(fpCurIdx);
        return 0;
    }

    const int nRet = abyTrailer[0];
    VSIFCloseL(fpCurIdx);
    return nRet;
}

/************************************************************************/
/*                           SetConstraint()                            */
/************************************************************************/

int FileGDBIndexIterator::SetConstraint(int nFieldIdx, FileGDBSQLOp op,
                                        OGRFieldType eOGRFieldType,
                                        const OGRField *psValue)
{
    const int errorRetValue = FALSE;
    CPLAssert(fpCurIdx == nullptr);

    returnErrorIf(nFieldIdx < 0 || nFieldIdx >= poParent->GetFieldCount());
    FileGDBField *poField = poParent->GetField(nFieldIdx);
    returnErrorIf(!(poField->HasIndex()));

    eFieldType = poField->GetType();
    eOp = op;

    returnErrorIf(eFieldType != FGFT_INT16 && eFieldType != FGFT_INT32 &&
                  eFieldType != FGFT_FLOAT32 && eFieldType != FGFT_FLOAT64 &&
                  eFieldType != FGFT_STRING && eFieldType != FGFT_DATETIME &&
                  eFieldType != FGFT_GUID && eFieldType != FGFT_GLOBALID &&
                  eFieldType != FGFT_INT64 && eFieldType != FGFT_DATE &&
                  eFieldType != FGFT_TIME &&
                  eFieldType != FGFT_DATETIME_WITH_OFFSET);

    const auto poIndex = poField->GetIndex();

    // Only supports ILIKE on a field string if the index expression starts
    // with LOWER() and the string to compare with is only ASCII without
    // wildcards
    if (eOGRFieldType == OFTString &&
        STARTS_WITH_CI(poIndex->GetExpression().c_str(), "LOWER("))
    {
        if (eOp == FGSO_ILIKE)
        {
            if (!CPLIsASCII(psValue->String, strlen(psValue->String)) ||
                strchr(psValue->String, '%') || strchr(psValue->String, '_'))
            {
                return FALSE;
            }
        }
        else if (eOp != FGSO_ISNOTNULL)
        {
            return FALSE;
        }
    }
    else if (eOp == FGSO_ILIKE)
    {
        return FALSE;
    }

    const std::string osAtxName =
        CPLFormFilenameSafe(
            CPLGetPathSafe(poParent->GetFilename().c_str()).c_str(),
            CPLGetBasenameSafe(poParent->GetFilename().c_str()).c_str(),
            poIndex->GetIndexName().c_str())
            .append(".atx");

    if (!ReadTrailer(osAtxName.c_str()))
        return FALSE;
    returnErrorIf(m_nValueCountInIdx >
                  static_cast<GUInt64>(poParent->GetValidRecordCount()));

    switch (eFieldType)
    {
        case FGFT_INT16:
            returnErrorIf(m_nValueSize != sizeof(GUInt16));
            if (eOp != FGSO_ISNOTNULL)
            {
                returnErrorIf(eOGRFieldType != OFTInteger);
                sValue.Integer = psValue->Integer;
            }
            break;
        case FGFT_INT32:
            returnErrorIf(m_nValueSize != sizeof(GUInt32));
            if (eOp != FGSO_ISNOTNULL)
            {
                returnErrorIf(eOGRFieldType != OFTInteger);
                sValue.Integer = psValue->Integer;
            }
            break;
        case FGFT_FLOAT32:
            returnErrorIf(m_nValueSize != sizeof(float));
            if (eOp != FGSO_ISNOTNULL)
            {
                returnErrorIf(eOGRFieldType != OFTReal);
                sValue.Real = psValue->Real;
            }
            break;
        case FGFT_FLOAT64:
            returnErrorIf(m_nValueSize != sizeof(double));
            if (eOp != FGSO_ISNOTNULL)
            {
                returnErrorIf(eOGRFieldType != OFTReal);
                sValue.Real = psValue->Real;
            }
            break;
        case FGFT_STRING:
        {
            returnErrorIf((m_nValueSize % 2) != 0);
            returnErrorIf(m_nValueSize == 0);
            returnErrorIf(m_nValueSize > 2 * MAX_CAR_COUNT_INDEXED_STR);
            nStrLen = m_nValueSize / 2;
            if (eOp != FGSO_ISNOTNULL)
            {
                returnErrorIf(eOGRFieldType != OFTString);
                wchar_t *pWide = CPLRecodeToWChar(psValue->String, CPL_ENC_UTF8,
                                                  CPL_ENC_UCS2);
                returnErrorIf(pWide == nullptr);
                int nCount = 0;
                while (pWide[nCount] != 0)
                {
                    returnErrorAndCleanupIf(nCount == nStrLen, CPLFree(pWide));
                    asUTF16Str[nCount] = pWide[nCount];
                    nCount++;
                }
                while (nCount < nStrLen)
                {
                    asUTF16Str[nCount] = 32; /* space character */
                    nCount++;
                }
                CPLFree(pWide);
            }
            break;
        }

        case FGFT_DATETIME:
        case FGFT_DATE:
        case FGFT_DATETIME_WITH_OFFSET:
        {
            returnErrorIf(m_nValueSize != sizeof(double));
            if (eOp != FGSO_ISNOTNULL)
            {
                returnErrorIf(
                    eOGRFieldType != OFTReal && eOGRFieldType != OFTDateTime &&
                    eOGRFieldType != OFTDate && eOGRFieldType != OFTTime);
                if (eOGRFieldType == OFTReal)
                    sValue.Real = psValue->Real;
                else
                    sValue.Real = FileGDBOGRDateToDoubleDate(
                        psValue, true,
                        /* bHighPrecision= */ eFieldType ==
                                FGFT_DATETIME_WITH_OFFSET ||
                            poField->IsHighPrecision());
            }
            break;
        }

        case FGFT_GUID:
        case FGFT_GLOBALID:
        {
            returnErrorIf(m_nValueSize != UUID_LEN_AS_STRING);
            if (eOp != FGSO_ISNOTNULL)
            {
                returnErrorIf(eOGRFieldType != OFTString);
                memset(szUUID, 0, UUID_LEN_AS_STRING + 1);
                // cppcheck-suppress redundantCopy
                strncpy(szUUID, psValue->String, UUID_LEN_AS_STRING);
                bEvaluateToFALSE = eOp == FGSO_EQ &&
                                   strlen(psValue->String) !=
                                       static_cast<size_t>(UUID_LEN_AS_STRING);
            }
            break;
        }

        case FGFT_INT64:
            returnErrorIf(m_nValueSize != sizeof(int64_t));
            if (eOp != FGSO_ISNOTNULL)
            {
                returnErrorIf(eOGRFieldType != OFTInteger64);
                sValue.Integer64 = psValue->Integer64;
            }
            break;

        case FGFT_TIME:
        {
            returnErrorIf(m_nValueSize != sizeof(double));
            if (eOp != FGSO_ISNOTNULL)
            {
                returnErrorIf(eOGRFieldType != OFTReal &&
                              eOGRFieldType != OFTTime);
                if (eOGRFieldType == OFTReal)
                    sValue.Real = psValue->Real;
                else
                    sValue.Real = FileGDBOGRTimeToDoubleTime(psValue);
            }
            break;
        }

        default:
            CPLAssert(false);
            break;
    }

    if (m_nValueCountInIdx > 0)
    {
        if (nIndexDepth == 1)
        {
            iFirstPageIdx[0] = iLastPageIdx[0] = 0;
        }
        else
        {
            returnErrorIf(!FindPages(0, 1));
        }
    }

    // To avoid 'spamming' on huge raster files
    if (poField->GetName() != "block_key")
    {
        CPLDebug("OpenFileGDB", "Using index on field %s (%s %s)",
                 poField->GetName().c_str(), FileGDBSQLOpToStr(eOp),
                 FileGDBValueToStr(eOGRFieldType, psValue));
    }

    Reset();

    return TRUE;
}

/************************************************************************/
/*                          FileGDBUTF16StrCompare()                    */
/************************************************************************/

static int FileGDBUTF16StrCompare(const GUInt16 *pasFirst,
                                  const GUInt16 *pasSecond, int nStrLen,
                                  bool bCaseInsensitive)
{
    for (int i = 0; i < nStrLen; i++)
    {
        GUInt16 chA = pasFirst[i];
        GUInt16 chB = pasSecond[i];
        if (bCaseInsensitive)
        {
            if (chA >= 'a' && chA <= 'z')
                chA -= 'a' - 'A';
            if (chB >= 'a' && chB <= 'z')
                chB -= 'a' - 'A';
        }
        if (chA < chB)
            return -1;
        if (chA > chB)
            return 1;
    }
    return 0;
}

/************************************************************************/
/*                              COMPARE()                               */
/************************************************************************/

#define COMPARE(a, b) (((a) < (b)) ? -1 : ((a) == (b)) ? 0 : 1)

/************************************************************************/
/*                             FindPages()                              */
/************************************************************************/

bool FileGDBIndexIterator::FindPages(int iLevel, uint64_t nPage)
{
    const bool errorRetValue = false;
    VSIFSeekL(fpCurIdx, static_cast<vsi_l_offset>(nPage - 1) * m_nPageSize,
              SEEK_SET);
#ifdef DEBUG
    iLoadedPage[iLevel] = nPage;
#endif
    returnErrorIf(VSIFReadL(abyPage[iLevel], m_nPageSize, 1, fpCurIdx) != 1);

    nSubPagesCount[iLevel] = GetUInt32(abyPage[iLevel] + m_nObjectIDSize, 0);
    returnErrorIf(nSubPagesCount[iLevel] == 0 ||
                  nSubPagesCount[iLevel] > nMaxPerPages);
    if (nIndexDepth == 2)
        returnErrorIf(m_nValueCountInIdx > static_cast<uint64_t>(nMaxPerPages) *
                                               (nSubPagesCount[0] + 1));

    if (eOp == FGSO_ISNOTNULL)
    {
        iFirstPageIdx[iLevel] = 0;
        iLastPageIdx[iLevel] = nSubPagesCount[iLevel];
        return true;
    }

    GUInt32 i;
#ifdef DEBUG_INDEX_CONSISTENCY
    double dfLastMax = 0.0;
    int nLastMax = 0;
    GUInt16 asLastMax[MAX_CAR_COUNT_INDEXED_STR] = {0};
    char szLastMaxUUID[UUID_LEN_AS_STRING + 1] = {0};
#endif
    iFirstPageIdx[iLevel] = iLastPageIdx[iLevel] = -1;

    for (i = 0; i < nSubPagesCount[iLevel]; i++)
    {
        int nComp;

        switch (eFieldType)
        {
            case FGFT_INT16:
            {
                GInt16 nVal =
                    GetInt16(abyPage[iLevel] + m_nOffsetFirstValInPage, i);
#ifdef DEBUG_INDEX_CONSISTENCY
                returnErrorIf(i > 0 && nVal < nLastMax);
                nLastMax = nVal;
#endif
                nComp = COMPARE(sValue.Integer, nVal);
                break;
            }

            case FGFT_INT32:
            {
                GInt32 nVal =
                    GetInt32(abyPage[iLevel] + m_nOffsetFirstValInPage, i);
#ifdef DEBUG_INDEX_CONSISTENCY
                returnErrorIf(i > 0 && nVal < nLastMax);
                nLastMax = nVal;
#endif
                nComp = COMPARE(sValue.Integer, nVal);
                break;
            }

            case FGFT_INT64:
            {
                int64_t nVal =
                    GetInt64(abyPage[iLevel] + m_nOffsetFirstValInPage, i);
#ifdef DEBUG_INDEX_CONSISTENCY
                returnErrorIf(i > 0 && nVal < nLastMax);
                nLastMax = nVal;
#endif
                nComp = COMPARE(sValue.Integer64, nVal);
                break;
            }

            case FGFT_FLOAT32:
            {
                float fVal =
                    GetFloat32(abyPage[iLevel] + m_nOffsetFirstValInPage, i);
#ifdef DEBUG_INDEX_CONSISTENCY
                returnErrorIf(i > 0 && fVal < dfLastMax);
                dfLastMax = fVal;
#endif
                nComp = COMPARE(sValue.Real, fVal);
                break;
            }

            case FGFT_FLOAT64:
            {
                const double dfVal =
                    GetFloat64(abyPage[iLevel] + m_nOffsetFirstValInPage, i);
#ifdef DEBUG_INDEX_CONSISTENCY
                returnErrorIf(i > 0 && dfVal < dfLastMax);
                dfLastMax = dfVal;
#endif
                nComp = COMPARE(sValue.Real, dfVal);
                break;
            }

            case FGFT_DATETIME:
            case FGFT_DATE:
            case FGFT_TIME:
            {
                const double dfVal =
                    GetFloat64(abyPage[iLevel] + m_nOffsetFirstValInPage, i);
#ifdef DEBUG_INDEX_CONSISTENCY
                returnErrorIf(i > 0 && dfVal < dfLastMax);
                dfLastMax = dfVal;
#endif
                if (sValue.Real + 1e-10 < dfVal)
                    nComp = -1;
                else if (sValue.Real - 1e-10 > dfVal)
                    nComp = 1;
                else
                    nComp = 0;
                break;
            }

            case FGFT_STRING:
            {
                GUInt16 *pasMax;
                GUInt16 asMax[MAX_CAR_COUNT_INDEXED_STR];
                pasMax = asMax;
                memcpy(asMax,
                       abyPage[iLevel] + m_nOffsetFirstValInPage +
                           nStrLen * sizeof(GUInt16) * i,
                       nStrLen * sizeof(GUInt16));
                for (int j = 0; j < nStrLen; j++)
                    CPL_LSBPTR16(&asMax[j]);
                    // Note: we have an inconsistency. OGR SQL equality operator
                    // is advertized to be case insensitive, but we have always
                    // implemented FGSO_EQ as case sensitive.
#ifdef DEBUG_INDEX_CONSISTENCY
                returnErrorIf(i > 0 &&
                              FileGDBUTF16StrCompare(pasMax, asLastMax, nStrLen,
                                                     eOp == FGSO_ILIKE) < 0);
                memcpy(asLastMax, pasMax, nStrLen * 2);
#endif
                nComp = FileGDBUTF16StrCompare(asUTF16Str, pasMax, nStrLen,
                                               eOp == FGSO_ILIKE);
                break;
            }

            case FGFT_GUID:
            case FGFT_GLOBALID:
            {
                const char *psNonzMaxUUID = reinterpret_cast<char *>(
                    abyPage[iLevel] + m_nOffsetFirstValInPage +
                    UUID_LEN_AS_STRING * i);
#ifdef DEBUG_INDEX_CONSISTENCY
                returnErrorIf(i > 0 && memcmp(psNonzMaxUUID, szLastMaxUUID,
                                              UUID_LEN_AS_STRING) < 0);
                memcpy(szLastMaxUUID, psNonzMaxUUID, UUID_LEN_AS_STRING);
#endif
                nComp = memcmp(szUUID, psNonzMaxUUID, UUID_LEN_AS_STRING);
                break;
            }

            default:
                CPLAssert(false);
                nComp = 0;
                break;
        }

        int bStop = FALSE;
        switch (eOp)
        {
            /* dfVal = 1 2 2 3 3 4 */
            /* sValue.Real = 3 */
            /* nComp = (sValue.Real < dfVal) ? -1 : (sValue.Real == dfVal) ? 0 :
             * 1; */
            case FGSO_LT:
            case FGSO_LE:
                if (iFirstPageIdx[iLevel] < 0)
                {
                    iFirstPageIdx[iLevel] = iLastPageIdx[iLevel] =
                        static_cast<int>(i);
                }
                else
                {
                    iLastPageIdx[iLevel] = static_cast<int>(i);
                    if (nComp < 0)
                    {
                        bStop = TRUE;
                    }
                }
                break;

            case FGSO_EQ:
            case FGSO_ILIKE:
                if (iFirstPageIdx[iLevel] < 0)
                {
                    if (nComp <= 0)
                        iFirstPageIdx[iLevel] = iLastPageIdx[iLevel] =
                            static_cast<int>(i);
                }
                else
                {
                    if (nComp == 0)
                        iLastPageIdx[iLevel] = static_cast<int>(i);
                    else
                        bStop = TRUE;
                }
                break;

            case FGSO_GE:
                if (iFirstPageIdx[iLevel] < 0)
                {
                    if (nComp <= 0)
                    {
                        iFirstPageIdx[iLevel] = static_cast<int>(i);
                        iLastPageIdx[iLevel] = nSubPagesCount[iLevel];
                        bStop = TRUE;
                    }
                }
                break;

            case FGSO_GT:
                if (iFirstPageIdx[iLevel] < 0)
                {
                    if (nComp < 0)
                    {
                        iFirstPageIdx[iLevel] = static_cast<int>(i);
                        iLastPageIdx[iLevel] = nSubPagesCount[iLevel];
                        bStop = TRUE;
                    }
                }
                break;

            case FGSO_ISNOTNULL:
                CPLAssert(false);
                break;
        }
        if (bStop)
            break;
    }

    if (iFirstPageIdx[iLevel] < 0)
    {
        iFirstPageIdx[iLevel] = iLastPageIdx[iLevel] = nSubPagesCount[iLevel];
    }
    else if (iLastPageIdx[iLevel] < static_cast<int>(nSubPagesCount[iLevel]))
    {
        iLastPageIdx[iLevel]++;
    }

    return true;
}

/************************************************************************/
/*                             Reset()                                  */
/************************************************************************/

void FileGDBIndexIteratorBase::Reset()
{
    iCurPageIdx[0] = (bAscending) ? iFirstPageIdx[0] - 1 : iLastPageIdx[0] + 1;
    memset(iFirstPageIdx + 1, 0xFF, (MAX_DEPTH - 1) * sizeof(iFirstPageIdx[0]));
    memset(iLastPageIdx + 1, 0xFF, (MAX_DEPTH - 1) * sizeof(iLastPageIdx[0]));
    memset(iCurPageIdx + 1, 0xFF, (MAX_DEPTH - 1) * sizeof(iCurPageIdx[0]));
    memset(nLastPageAccessed, 0, MAX_DEPTH * sizeof(nLastPageAccessed[0]));
    iCurFeatureInPage = 0;
    nFeaturesInPage = 0;

    bEOF = (m_nValueCountInIdx == 0);
}

/************************************************************************/
/*                             Reset()                                  */
/************************************************************************/

void FileGDBIndexIterator::Reset()
{
    FileGDBIndexIteratorBase::Reset();
    iSorted = 0;
    bEOF = bEOF || bEvaluateToFALSE;
}

/************************************************************************/
/*                           ReadPageNumber()                           */
/************************************************************************/

uint64_t FileGDBIndexIteratorBase::ReadPageNumber(int iLevel)
{
    const int errorRetValue = 0;
    uint64_t nPage;
    if (m_nVersion == 1)
    {
        nPage = GetUInt32(abyPage[iLevel] + m_nNonLeafPageHeaderSize,
                          iCurPageIdx[iLevel]);
        if (nPage == nLastPageAccessed[iLevel])
        {
            if (!LoadNextPage(iLevel))
                return 0;
            nPage = GetUInt32(abyPage[iLevel] + m_nNonLeafPageHeaderSize,
                              iCurPageIdx[iLevel]);
        }
    }
    else
    {
        nPage = GetUInt64(abyPage[iLevel] + m_nNonLeafPageHeaderSize,
                          iCurPageIdx[iLevel]);
        if (nPage == nLastPageAccessed[iLevel])
        {
            if (!LoadNextPage(iLevel))
                return 0;
            nPage = GetUInt64(abyPage[iLevel] + m_nNonLeafPageHeaderSize,
                              iCurPageIdx[iLevel]);
        }
    }
    nLastPageAccessed[iLevel] = nPage;
    returnErrorIf(nPage < 2);
    return nPage;
}

/************************************************************************/
/*                           LoadNextPage()                             */
/************************************************************************/

bool FileGDBIndexIteratorBase::LoadNextPage(int iLevel)
{
    const bool errorRetValue = false;
    if ((bAscending && iCurPageIdx[iLevel] == iLastPageIdx[iLevel]) ||
        (!bAscending && iCurPageIdx[iLevel] == iFirstPageIdx[iLevel]))
    {
        if (iLevel == 0 || !LoadNextPage(iLevel - 1))
            return false;

        const auto nPage = ReadPageNumber(iLevel - 1);
        returnErrorIf(!FindPages(iLevel, nPage));

        iCurPageIdx[iLevel] =
            (bAscending) ? iFirstPageIdx[iLevel] : iLastPageIdx[iLevel];
    }
    else
    {
        if (bAscending)
            iCurPageIdx[iLevel]++;
        else
            iCurPageIdx[iLevel]--;
    }

    return true;
}

/************************************************************************/
/*                        LoadNextFeaturePage()                         */
/************************************************************************/

bool FileGDBIndexIteratorBase::LoadNextFeaturePage()
{
    const bool errorRetValue = false;
    GUInt64 nPage;

    if (nIndexDepth == 1)
    {
        if (iCurPageIdx[0] == iLastPageIdx[0])
        {
            return false;
        }
        if (bAscending)
            iCurPageIdx[0]++;
        else
            iCurPageIdx[0]--;
        nPage = 1;
    }
    else
    {
        if (!LoadNextPage(nIndexDepth - 2))
        {
            return false;
        }
        nPage = ReadPageNumber(nIndexDepth - 2);
        returnErrorIf(nPage < 2);
    }

    const cpl::NonCopyableVector<GByte> *cachedPagePtr =
        m_oCacheFeaturePage.getPtr(nPage);
    if (cachedPagePtr)
    {
        memcpy(abyPageFeature, cachedPagePtr->data(), m_nPageSize);
    }
    else
    {
        cpl::NonCopyableVector<GByte> cachedPage;
        if (m_oCacheFeaturePage.size() == m_oCacheFeaturePage.getMaxSize())
        {
            m_oCacheFeaturePage.removeAndRecycleOldestEntry(cachedPage);
            cachedPage.clear();
        }

        VSIFSeekL(fpCurIdx, static_cast<vsi_l_offset>(nPage - 1) * m_nPageSize,
                  SEEK_SET);
#ifdef DEBUG
        iLoadedPage[nIndexDepth - 1] = nPage;
#endif
        returnErrorIf(VSIFReadL(abyPageFeature, m_nPageSize, 1, fpCurIdx) != 1);
        cachedPage.insert(cachedPage.end(), abyPageFeature,
                          abyPageFeature + m_nPageSize);
        m_oCacheFeaturePage.insert(nPage, std::move(cachedPage));
    }

    const GUInt32 nFeatures = GetUInt32(abyPageFeature + m_nObjectIDSize, 0);
    returnErrorIf(nFeatures > nMaxPerPages);

    nFeaturesInPage = static_cast<int>(nFeatures);
    iCurFeatureInPage = (bAscending) ? 0 : nFeaturesInPage - 1;
    return nFeatures != 0;
}

/************************************************************************/
/*                              GetNextRow()                            */
/************************************************************************/

int64_t FileGDBIndexIterator::GetNextRow()
{
    const int64_t errorRetValue = -1;
    if (bEOF)
        return -1;

    while (true)
    {
        if (iCurFeatureInPage >= nFeaturesInPage || iCurFeatureInPage < 0)
        {
            if (!LoadNextFeaturePage())
            {
                bEOF = true;
                return -1;
            }
        }

        bool bMatch = false;
        if (eOp == FGSO_ISNOTNULL)
        {
            bMatch = true;
        }
        else
        {
            int nComp = 0;
            switch (eFieldType)
            {
                case FGFT_INT16:
                {
                    const GInt16 nVal =
                        GetInt16(abyPageFeature + m_nOffsetFirstValInPage,
                                 iCurFeatureInPage);
                    nComp = COMPARE(sValue.Integer, nVal);
                    break;
                }

                case FGFT_INT32:
                {
                    const GInt32 nVal =
                        GetInt32(abyPageFeature + m_nOffsetFirstValInPage,
                                 iCurFeatureInPage);
                    nComp = COMPARE(sValue.Integer, nVal);
                    break;
                }

                case FGFT_FLOAT32:
                {
                    const float fVal =
                        GetFloat32(abyPageFeature + m_nOffsetFirstValInPage,
                                   iCurFeatureInPage);
                    nComp = COMPARE(sValue.Real, fVal);
                    break;
                }

                case FGFT_FLOAT64:
                {
                    const double dfVal =
                        GetFloat64(abyPageFeature + m_nOffsetFirstValInPage,
                                   iCurFeatureInPage);
                    nComp = COMPARE(sValue.Real, dfVal);
                    break;
                }

                case FGFT_DATETIME:
                case FGFT_DATE:
                case FGFT_TIME:
                case FGFT_DATETIME_WITH_OFFSET:
                {
                    const double dfVal =
                        GetFloat64(abyPageFeature + m_nOffsetFirstValInPage,
                                   iCurFeatureInPage);
                    if (sValue.Real + 1e-10 < dfVal)
                        nComp = -1;
                    else if (sValue.Real - 1e-10 > dfVal)
                        nComp = 1;
                    else
                        nComp = 0;
                    break;
                }

                case FGFT_STRING:
                {
                    GUInt16 asVal[MAX_CAR_COUNT_INDEXED_STR];
                    memcpy(asVal,
                           abyPageFeature + m_nOffsetFirstValInPage +
                               nStrLen * 2 * iCurFeatureInPage,
                           nStrLen * 2);
                    for (int j = 0; j < nStrLen; j++)
                        CPL_LSBPTR16(&asVal[j]);
                    // Note: we have an inconsistency. OGR SQL equality operator
                    // is advertized to be case insensitive, but we have always
                    // implemented FGSO_EQ as case sensitive.
                    nComp = FileGDBUTF16StrCompare(asUTF16Str, asVal, nStrLen,
                                                   eOp == FGSO_ILIKE);
                    break;
                }

                case FGFT_GUID:
                case FGFT_GLOBALID:
                {
                    nComp = memcmp(szUUID,
                                   abyPageFeature + m_nOffsetFirstValInPage +
                                       UUID_LEN_AS_STRING * iCurFeatureInPage,
                                   UUID_LEN_AS_STRING);
                    break;
                }

                case FGFT_INT64:
                {
                    const int64_t nVal =
                        GetInt64(abyPageFeature + m_nOffsetFirstValInPage,
                                 iCurFeatureInPage);
                    nComp = COMPARE(sValue.Integer64, nVal);
                    break;
                }

                default:
                    CPLAssert(false);
                    nComp = 0;
                    break;
            }

            bMatch = false;
            CPL_IGNORE_RET_VAL(bMatch);
            switch (eOp)
            {
                case FGSO_LT:
                    if (nComp <= 0 && bAscending)
                    {
                        bEOF = true;
                        return -1;
                    }
                    bMatch = true;
                    break;

                case FGSO_LE:
                    if (nComp < 0 && bAscending)
                    {
                        bEOF = true;
                        return -1;
                    }
                    bMatch = true;
                    break;

                case FGSO_EQ:
                case FGSO_ILIKE:
                    if (nComp < 0 && bAscending)
                    {
                        bEOF = true;
                        return -1;
                    }
                    bMatch = nComp == 0;
                    break;

                case FGSO_GE:
                    bMatch = nComp <= 0;
                    break;

                case FGSO_GT:
                    bMatch = nComp < 0;
                    break;

                case FGSO_ISNOTNULL:
                    CPLAssert(false);
                    break;
            }
        }

        if (bMatch)
        {
            const GUInt64 nFID =
                m_nVersion == 1
                    ? GetUInt32(abyPageFeature + m_nLeafPageHeaderSize,
                                iCurFeatureInPage)
                    : GetUInt64(abyPageFeature + m_nLeafPageHeaderSize,
                                iCurFeatureInPage);
            if (bAscending)
                iCurFeatureInPage++;
            else
                iCurFeatureInPage--;
            returnErrorAndCleanupIf(
                nFID < 1 || nFID > static_cast<GUInt64>(
                                       poParent->GetTotalRecordCount()),
                bEOF = true);
            return static_cast<int64_t>(nFID - 1);
        }
        else
        {
            if (bAscending)
                iCurFeatureInPage++;
            else
                iCurFeatureInPage--;
        }
    }
}

/************************************************************************/
/*                             SortRows()                               */
/************************************************************************/

int FileGDBIndexIterator::SortRows()
{
    nSortedCount = 0;
    iSorted = 0;
    int nSortedAlloc = 0;
    Reset();
    while (true)
    {
        int64_t nRow = GetNextRow();
        if (nRow < 0)
            break;
        if (nSortedCount == nSortedAlloc)
        {
            int nNewSortedAlloc = 4 * nSortedAlloc / 3 + 16;
            int64_t *panNewSortedRows =
                static_cast<int64_t *>(VSI_REALLOC_VERBOSE(
                    panSortedRows, sizeof(int64_t) * nNewSortedAlloc));
            if (panNewSortedRows == nullptr)
            {
                nSortedCount = 0;
                return FALSE;
            }
            nSortedAlloc = nNewSortedAlloc;
            panSortedRows = panNewSortedRows;
        }
        panSortedRows[nSortedCount++] = nRow;
    }
    if (nSortedCount == 0)
        return FALSE;
    std::sort(panSortedRows, panSortedRows + nSortedCount);
#ifdef m_nValueCountInIdx_reliable
    if (eOp == FGSO_ISNOTNULL && (int64_t)m_nValueCountInIdx != nSortedCount)
        PrintError();
#endif
    return TRUE;
}

/************************************************************************/
/*                        GetNextRowSortedByFID()                       */
/************************************************************************/

int64_t FileGDBIndexIterator::GetNextRowSortedByFID()
{
    if (eOp == FGSO_EQ)
        return GetNextRow();

    if (iSorted < nSortedCount)
        return panSortedRows[iSorted++];

    if (nSortedCount < 0)
    {
        if (!SortRows())
            return -1;
        return panSortedRows[iSorted++];
    }
    else
    {
        return -1;
    }
}

/************************************************************************/
/*                           GetRowCount()                              */
/************************************************************************/

int64_t FileGDBIndexIterator::GetRowCount()
{
    // The m_nValueCountInIdx value has been found to be unreliable when the index
    // is built as features are inserted (and when they are not in increasing
    // order) (with FileGDB SDK 1.3) So disable this optimization as there's no
    // fast way to know if the value is reliable or not.
#ifdef m_nValueCountInIdx_reliable
    if (eOp == FGSO_ISNOTNULL)
        return (int64_t)m_nValueCountInIdx;
#endif

    if (nSortedCount >= 0)
        return nSortedCount;

    int64_t nRowCount = 0;
    bool bSaveAscending = bAscending;
    bAscending = true; /* for a tiny bit of more efficiency */
    Reset();
    while (GetNextRow() >= 0)
        nRowCount++;
    bAscending = bSaveAscending;
    Reset();
    return nRowCount;
}

/************************************************************************/
/*                            GetMinMaxValue()                          */
/************************************************************************/

const OGRField *FileGDBIndexIterator::GetMinMaxValue(OGRField *psField,
                                                     int &eOutType, int bIsMin)
{
    const OGRField *errorRetValue = nullptr;
    eOutType = -1;
    if (m_nValueCountInIdx == 0)
        return nullptr;

    std::vector<GByte> l_abyPageV;
    try
    {
        l_abyPageV.resize(m_nPageSize);
    }
    catch (const std::exception &)
    {
        return nullptr;
    }
    GByte *l_abyPage = l_abyPageV.data();
    uint64_t nPage = 1;
    for (GUInt32 iLevel = 0; iLevel < nIndexDepth - 1; iLevel++)
    {
        VSIFSeekL(fpCurIdx, static_cast<vsi_l_offset>(nPage - 1) * m_nPageSize,
                  SEEK_SET);
#ifdef DEBUG
        iLoadedPage[iLevel] = nPage;
#endif
        returnErrorIf(VSIFReadL(l_abyPage, m_nPageSize, 1, fpCurIdx) != 1);
        GUInt32 l_nSubPagesCount = GetUInt32(l_abyPage + m_nObjectIDSize, 0);
        returnErrorIf(l_nSubPagesCount == 0 || l_nSubPagesCount > nMaxPerPages);

        if (m_nVersion == 1)
        {
            nPage = GetUInt32(l_abyPage + m_nNonLeafPageHeaderSize,
                              bIsMin ? 0 : l_nSubPagesCount);
        }
        else
        {
            nPage = GetUInt64(l_abyPage + m_nNonLeafPageHeaderSize,
                              bIsMin ? 0 : l_nSubPagesCount);
        }
        returnErrorIf(nPage < 2);
    }

    VSIFSeekL(fpCurIdx, static_cast<vsi_l_offset>(nPage - 1) * m_nPageSize,
              SEEK_SET);
#ifdef DEBUG
    iLoadedPage[nIndexDepth - 1] = nPage;
#endif
    returnErrorIf(VSIFReadL(l_abyPage, m_nPageSize, 1, fpCurIdx) != 1);

    GUInt32 nFeatures = GetUInt32(l_abyPage + m_nObjectIDSize, 0);
    returnErrorIf(nFeatures < 1 || nFeatures > nMaxPerPages);

    int iFeature = (bIsMin) ? 0 : nFeatures - 1;

    switch (eFieldType)
    {
        case FGFT_INT16:
        {
            const GInt16 nVal =
                GetInt16(l_abyPage + m_nOffsetFirstValInPage, iFeature);
            psField->Integer = nVal;
            eOutType = OFTInteger;
            return psField;
        }

        case FGFT_INT32:
        {
            const GInt32 nVal =
                GetInt32(l_abyPage + m_nOffsetFirstValInPage, iFeature);
            psField->Integer = nVal;
            eOutType = OFTInteger;
            return psField;
        }

        case FGFT_FLOAT32:
        {
            const float fVal =
                GetFloat32(l_abyPage + m_nOffsetFirstValInPage, iFeature);
            psField->Real = fVal;
            eOutType = OFTReal;
            return psField;
        }

        case FGFT_FLOAT64:
        {
            const double dfVal =
                GetFloat64(l_abyPage + m_nOffsetFirstValInPage, iFeature);
            psField->Real = dfVal;
            eOutType = OFTReal;
            return psField;
        }

        case FGFT_DATETIME:
        case FGFT_DATETIME_WITH_OFFSET:
        {
            const double dfVal =
                GetFloat64(l_abyPage + m_nOffsetFirstValInPage, iFeature);
            FileGDBDoubleDateToOGRDate(dfVal, false, psField);
            eOutType = OFTDateTime;
            return psField;
        }

        case FGFT_DATE:
        {
            const double dfVal =
                GetFloat64(l_abyPage + m_nOffsetFirstValInPage, iFeature);
            FileGDBDoubleDateToOGRDate(dfVal, false, psField);
            eOutType = OFTDate;
            return psField;
        }

        case FGFT_TIME:
        {
            const double dfVal =
                GetFloat64(l_abyPage + m_nOffsetFirstValInPage, iFeature);
            FileGDBDoubleTimeToOGRTime(dfVal, psField);
            eOutType = OFTTime;
            return psField;
        }

        case FGFT_STRING:
        {
            wchar_t awsVal[MAX_CAR_COUNT_INDEXED_STR + 1] = {0};
            for (int j = 0; j < nStrLen; j++)
            {
                GUInt16 nCh =
                    GetUInt16(l_abyPage + m_nOffsetFirstValInPage +
                                  nStrLen * sizeof(GUInt16) * iFeature,
                              j);
                awsVal[j] = nCh;
            }
            awsVal[nStrLen] = 0;
            char *pszOut =
                CPLRecodeFromWChar(awsVal, CPL_ENC_UCS2, CPL_ENC_UTF8);
            returnErrorIf(pszOut == nullptr);
            returnErrorAndCleanupIf(strlen(pszOut) >
                                        static_cast<size_t>(MAX_UTF8_LEN_STR),
                                    VSIFree(pszOut));
            strcpy(psField->String, pszOut);
            CPLFree(pszOut);
            eOutType = OFTString;
            return psField;
        }

        case FGFT_GUID:
        case FGFT_GLOBALID:
        {
            memcpy(psField->String,
                   l_abyPage + m_nOffsetFirstValInPage +
                       UUID_LEN_AS_STRING * iFeature,
                   UUID_LEN_AS_STRING);
            psField->String[UUID_LEN_AS_STRING] = 0;
            eOutType = OFTString;
            return psField;
        }

        case FGFT_INT64:
        {
            const int64_t nVal =
                GetInt64(l_abyPage + m_nOffsetFirstValInPage, iFeature);
            psField->Integer64 = nVal;
            eOutType = OFTInteger64;
            return psField;
        }

        default:
            CPLAssert(false);
            break;
    }
    return nullptr;
}

/************************************************************************/
/*                            GetMinValue()                             */
/************************************************************************/

const OGRField *FileGDBIndexIterator::GetMinValue(int &eOutType)
{
    if (eOp != FGSO_ISNOTNULL)
        return FileGDBIterator::GetMinValue(eOutType);
    if (eFieldType == FGFT_STRING || eFieldType == FGFT_GUID ||
        eFieldType == FGFT_GLOBALID)
        sMin.String = szMin;
    return GetMinMaxValue(&sMin, eOutType, TRUE);
}

/************************************************************************/
/*                            GetMaxValue()                             */
/************************************************************************/

const OGRField *FileGDBIndexIterator::GetMaxValue(int &eOutType)
{
    if (eOp != FGSO_ISNOTNULL)
        return FileGDBIterator::GetMinValue(eOutType);
    if (eFieldType == FGFT_STRING || eFieldType == FGFT_GUID ||
        eFieldType == FGFT_GLOBALID)
        sMax.String = szMax;
    return GetMinMaxValue(&sMax, eOutType, FALSE);
}

/************************************************************************/
/*                        GetMinMaxSumCount()                           */
/************************************************************************/

struct Int16Getter
{
  public:
    static double GetAsDouble(const GByte *pBaseAddr, int iOffset)
    {
        return GetInt16(pBaseAddr, iOffset);
    }
};

struct Int32Getter
{
  public:
    static double GetAsDouble(const GByte *pBaseAddr, int iOffset)
    {
        return GetInt32(pBaseAddr, iOffset);
    }
};

struct Int64Getter
{
  public:
    static double GetAsDouble(const GByte *pBaseAddr, int iOffset)
    {
        return static_cast<double>(GetInt64(pBaseAddr, iOffset));
    }
};

struct Float32Getter
{
  public:
    static double GetAsDouble(const GByte *pBaseAddr, int iOffset)
    {
        return GetFloat32(pBaseAddr, iOffset);
    }
};

struct Float64Getter
{
  public:
    static double GetAsDouble(const GByte *pBaseAddr, int iOffset)
    {
        return GetFloat64(pBaseAddr, iOffset);
    }
};

template <class Getter>
void FileGDBIndexIterator::GetMinMaxSumCount(double &dfMin, double &dfMax,
                                             double &dfSum, int &nCount)
{
    int nLocalCount = 0;
    double dfLocalSum = 0.0;
    double dfVal = 0.0;

    while (true)
    {
        if (iCurFeatureInPage >= nFeaturesInPage)
        {
            if (!LoadNextFeaturePage())
            {
                break;
            }
        }

        dfVal = Getter::GetAsDouble(abyPageFeature + m_nOffsetFirstValInPage,
                                    iCurFeatureInPage);

        dfLocalSum += dfVal;
        if (nLocalCount == 0)
            dfMin = dfVal;
        nLocalCount++;
        iCurFeatureInPage++;
    }

    dfSum = dfLocalSum;
    nCount = nLocalCount;
    dfMax = dfVal;
}

bool FileGDBIndexIterator::GetMinMaxSumCount(double &dfMin, double &dfMax,
                                             double &dfSum, int &nCount)
{
    const bool errorRetValue = false;
    dfMin = 0.0;
    dfMax = 0.0;
    dfSum = 0.0;
    nCount = 0;
    returnErrorIf(eOp != FGSO_ISNOTNULL);
    returnErrorIf(eFieldType != FGFT_INT16 && eFieldType != FGFT_INT32 &&
                  eFieldType != FGFT_FLOAT32 && eFieldType != FGFT_FLOAT64 &&
                  eFieldType != FGFT_DATETIME && eFieldType != FGFT_INT64 &&
                  eFieldType != FGFT_DATE && eFieldType != FGFT_TIME &&
                  eFieldType != FGFT_DATETIME_WITH_OFFSET);

    bool bSaveAscending = bAscending;
    bAscending = true;
    Reset();

    switch (eFieldType)
    {
        case FGFT_INT16:
        {
            GetMinMaxSumCount<Int16Getter>(dfMin, dfMax, dfSum, nCount);
            break;
        }
        case FGFT_INT32:
        {
            GetMinMaxSumCount<Int32Getter>(dfMin, dfMax, dfSum, nCount);
            break;
        }
        case FGFT_INT64:
        {
            GetMinMaxSumCount<Int64Getter>(dfMin, dfMax, dfSum, nCount);
            break;
        }
        case FGFT_FLOAT32:
        {
            GetMinMaxSumCount<Float32Getter>(dfMin, dfMax, dfSum, nCount);
            break;
        }
        case FGFT_FLOAT64:
        case FGFT_DATETIME:
        case FGFT_DATE:
        case FGFT_TIME:
        case FGFT_DATETIME_WITH_OFFSET:
        {
            GetMinMaxSumCount<Float64Getter>(dfMin, dfMax, dfSum, nCount);
            break;
        }
        default:
            CPLAssert(false);
            break;
    }

    bAscending = bSaveAscending;
    Reset();

    return true;
}

/************************************************************************/
/*                    FileGDBSpatialIndexIteratorImpl                   */
/************************************************************************/

class FileGDBSpatialIndexIteratorImpl final : public FileGDBIndexIteratorBase,
                                              public FileGDBSpatialIndexIterator
{
    OGREnvelope m_sFilterEnvelope;
    bool m_bHasBuiltSetFID = false;
    std::vector<int64_t> m_oFIDVector{};
    size_t m_nVectorIdx = 0;
    int m_nGridNo = 0;
    GInt64 m_nMinVal = 0;
    GInt64 m_nMaxVal = 0;
    GInt32 m_nCurX = 0;
    GInt32 m_nMaxX = 0;

    virtual bool FindPages(int iLevel, uint64_t nPage) override;
    int GetNextRow();
    bool ReadNewXRange();
    bool ResetInternal();
    double GetScaledCoord(double coord) const;

  protected:
    friend class FileGDBSpatialIndexIterator;

    FileGDBSpatialIndexIteratorImpl(FileGDBTable *poParent,
                                    const OGREnvelope &sFilterEnvelope);
    bool Init();

  public:
    virtual FileGDBTable *GetTable() override
    {
        return poParent;
    }  // avoid MSVC C4250 inherits via dominance warning

    virtual int64_t GetNextRowSortedByFID() override;
    virtual void Reset() override;

    virtual bool SetEnvelope(const OGREnvelope &sFilterEnvelope) override;
};

/************************************************************************/
/*                      FileGDBSpatialIndexIteratorImpl()                   */
/************************************************************************/

FileGDBSpatialIndexIteratorImpl::FileGDBSpatialIndexIteratorImpl(
    FileGDBTable *poParentIn, const OGREnvelope &sFilterEnvelope)
    : FileGDBIndexIteratorBase(poParentIn, true),
      m_sFilterEnvelope(sFilterEnvelope)
{
    double dfYMinClamped;
    double dfYMaxClamped;
    poParentIn->GetMinMaxProjYForSpatialIndex(dfYMinClamped, dfYMaxClamped);
    m_sFilterEnvelope.MinY = std::min(
        std::max(m_sFilterEnvelope.MinY, dfYMinClamped), dfYMaxClamped);
    m_sFilterEnvelope.MaxY = std::min(
        std::max(m_sFilterEnvelope.MaxY, dfYMinClamped), dfYMaxClamped);
}

/************************************************************************/
/*                                  Build()                             */
/************************************************************************/

FileGDBSpatialIndexIterator *
FileGDBSpatialIndexIterator::Build(FileGDBTable *poParent,
                                   const OGREnvelope &sFilterEnvelope)
{
    FileGDBSpatialIndexIteratorImpl *poIterator =
        new FileGDBSpatialIndexIteratorImpl(poParent, sFilterEnvelope);
    if (!poIterator->Init())
    {
        delete poIterator;
        return nullptr;
    }
    return poIterator;
}

/************************************************************************/
/*                         SetEnvelope()                                */
/************************************************************************/

bool FileGDBSpatialIndexIteratorImpl::SetEnvelope(
    const OGREnvelope &sFilterEnvelope)
{
    m_sFilterEnvelope = sFilterEnvelope;
    m_bHasBuiltSetFID = false;
    m_oFIDVector.clear();
    return ResetInternal();
}

/************************************************************************/
/*                              Init()                                  */
/************************************************************************/

bool FileGDBSpatialIndexIteratorImpl::Init()
{
    const bool errorRetValue = false;

    const std::string osSpxName = CPLFormFilenameSafe(
        CPLGetPathSafe(poParent->GetFilename().c_str()).c_str(),
        CPLGetBasenameSafe(poParent->GetFilename().c_str()).c_str(), "spx");

    if (!ReadTrailer(osSpxName.c_str()))
        return false;

    returnErrorIf(m_nValueSize != sizeof(uint64_t));

    const auto IsPositiveInt = [](double x) { return x >= 0 && x <= INT_MAX; };

    const auto &gridRes = poParent->GetSpatialIndexGridResolution();
    const FileGDBGeomField *poGDBGeomField = poParent->GetGeomField();
    if (gridRes.empty() || !(gridRes[0] > 0) ||
        // Check if the center of the layer extent results in valid scaled
        // coords
        !(!std::isnan(poGDBGeomField->GetXMin()) &&
          IsPositiveInt(GetScaledCoord(
              0.5 * (poGDBGeomField->GetXMin() + poGDBGeomField->GetXMax()))) &&
          IsPositiveInt(GetScaledCoord(
              0.5 * (poGDBGeomField->GetYMin() + poGDBGeomField->GetYMax())))))
    {
        // gridRes[0] == 1.61271680278378622e-312 happens on layer
        // Zone18_2014_01_Broadcast of
        // https://coast.noaa.gov/htdata/CMSP/AISDataHandler/2014/01/Zone18_2014_01.zip
        // The FileGDB driver does not use the .spx file in that situation,
        // so do we.
        CPLDebug("OpenFileGDB",
                 "Cannot use %s as the grid resolution is invalid",
                 osSpxName.c_str());
        return false;
    }

    // Detect broken .spx file such as SWISSTLM3D_2022_LV95_LN02.gdb/a00000019.spx
    // from https://data.geo.admin.ch/ch.swisstopo.swisstlm3d/swisstlm3d_2022-03/swisstlm3d_2022-03_2056_5728.gdb.zip
    // which advertises nIndexDepth == 1 whereas it seems to be it should be 2.
    if (nIndexDepth == 1)
    {
        iLastPageIdx[0] = 0;
        LoadNextFeaturePage();
        iFirstPageIdx[0] = iLastPageIdx[0] = -1;
        if (nFeaturesInPage >= 2 &&
            nFeaturesInPage < poParent->GetTotalRecordCount() / 10 &&
            m_nPageCount > static_cast<GUInt32>(nFeaturesInPage))
        {
            // Check if it looks like a non-feature page, that is that the
            // IDs pointed by it are index page IDs and not feature IDs.
            bool bReferenceOtherPages = true;
            for (int i = 0; i < nFeaturesInPage; ++i)
            {
                const GUInt32 nID = GetUInt32(abyPageFeature + 8, i);
                if (!(nID >= 2 && nID <= m_nPageCount))
                {
                    bReferenceOtherPages = false;
                    break;
                }
            }
            if (bReferenceOtherPages)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Cannot use %s as the index depth(=1) is suspicious "
                         "(it should rather be 2)",
                         osSpxName.c_str());
                return false;
            }
        }
    }

    return ResetInternal();
}

/************************************************************************/
/*                         GetScaledCoord()                             */
/************************************************************************/

double FileGDBSpatialIndexIteratorImpl::GetScaledCoord(double coord) const
{
    const auto &gridRes = poParent->GetSpatialIndexGridResolution();
    return (coord / gridRes[0] + (1 << 29)) / (gridRes[m_nGridNo] / gridRes[0]);
}

/************************************************************************/
/*                         ReadNewXRange()                              */
/************************************************************************/

bool FileGDBSpatialIndexIteratorImpl::ReadNewXRange()
{
    const GUInt64 v1 =
        (static_cast<GUInt64>(m_nGridNo) << 62) |
        (static_cast<GUInt64>(m_nCurX) << 31) |
        (static_cast<GUInt64>(
            std::min(std::max(0.0, GetScaledCoord(m_sFilterEnvelope.MinY)),
                     static_cast<double>(INT_MAX))));
    const GUInt64 v2 =
        (static_cast<GUInt64>(m_nGridNo) << 62) |
        (static_cast<GUInt64>(m_nCurX) << 31) |
        (static_cast<GUInt64>(
            std::min(std::max(0.0, GetScaledCoord(m_sFilterEnvelope.MaxY)),
                     static_cast<double>(INT_MAX))));
    if (m_nGridNo < 2)
    {
        m_nMinVal = v1;
        m_nMaxVal = v2;
    }
    else
    {
        // Reverse order due to negative sign
        memcpy(&m_nMinVal, &v2, sizeof(GInt64));
        memcpy(&m_nMaxVal, &v1, sizeof(GInt64));
    }

    const bool errorRetValue = false;
    if (m_nValueCountInIdx > 0)
    {
        if (nIndexDepth == 1)
        {
            iFirstPageIdx[0] = iLastPageIdx[0] = 0;
        }
        else
        {
            returnErrorIf(!FindPages(0, 1));
        }
    }

    FileGDBIndexIteratorBase::Reset();

    return true;
}

/************************************************************************/
/*                         FindMinMaxIdx()                              */
/************************************************************************/

static bool FindMinMaxIdx(const GByte *pBaseAddr, const int nVals,
                          const GInt64 nMinVal, const GInt64 nMaxVal,
                          int &minIdxOut, int &maxIdxOut)
{
    // Find maximum index that is <= nMaxVal
    int nMinIdx = 0;
    int nMaxIdx = nVals - 1;
    while (nMaxIdx - nMinIdx >= 2)
    {
        int nIdx = (nMinIdx + nMaxIdx) / 2;
        const GInt64 nVal = GetInt64(pBaseAddr, nIdx);
        if (nVal <= nMaxVal)
            nMinIdx = nIdx;
        else
            nMaxIdx = nIdx;
    }
    while (GetInt64(pBaseAddr, nMaxIdx) > nMaxVal)
    {
        nMaxIdx--;
        if (nMaxIdx < 0)
        {
            return false;
        }
    }
    maxIdxOut = nMaxIdx;

    // Find minimum index that is >= nMinVal
    nMinIdx = 0;
    while (nMaxIdx - nMinIdx >= 2)
    {
        int nIdx = (nMinIdx + nMaxIdx) / 2;
        const GInt64 nVal = GetInt64(pBaseAddr, nIdx);
        if (nVal >= nMinVal)
            nMaxIdx = nIdx;
        else
            nMinIdx = nIdx;
    }
    while (GetInt64(pBaseAddr, nMinIdx) < nMinVal)
    {
        nMinIdx++;
        if (nMinIdx == nVals)
        {
            return false;
        }
    }
    minIdxOut = nMinIdx;
    return true;
}

/************************************************************************/
/*                             FindPages()                              */
/************************************************************************/

bool FileGDBSpatialIndexIteratorImpl::FindPages(int iLevel, uint64_t nPage)
{
    const bool errorRetValue = false;

    iFirstPageIdx[iLevel] = iLastPageIdx[iLevel] = -1;

    const cpl::NonCopyableVector<GByte> *cachedPagePtr =
        m_oCachePage[iLevel].getPtr(nPage);
    if (cachedPagePtr)
    {
        memcpy(abyPage[iLevel], cachedPagePtr->data(), m_nPageSize);
    }
    else
    {
        cpl::NonCopyableVector<GByte> cachedPage;
        if (m_oCachePage[iLevel].size() == m_oCachePage[iLevel].getMaxSize())
        {
            m_oCachePage[iLevel].removeAndRecycleOldestEntry(cachedPage);
            cachedPage.clear();
        }

        VSIFSeekL(fpCurIdx, static_cast<vsi_l_offset>(nPage - 1) * m_nPageSize,
                  SEEK_SET);
#ifdef DEBUG
        iLoadedPage[iLevel] = nPage;
#endif
        returnErrorIf(VSIFReadL(abyPage[iLevel], m_nPageSize, 1, fpCurIdx) !=
                      1);
        cachedPage.insert(cachedPage.end(), abyPage[iLevel],
                          abyPage[iLevel] + m_nPageSize);
        m_oCachePage[iLevel].insert(nPage, std::move(cachedPage));
    }

    nSubPagesCount[iLevel] = GetUInt32(abyPage[iLevel] + m_nObjectIDSize, 0);
    returnErrorIf(nSubPagesCount[iLevel] == 0 ||
                  nSubPagesCount[iLevel] > nMaxPerPages);

    if (GetInt64(abyPage[iLevel] + m_nOffsetFirstValInPage, 0) > m_nMaxVal)
    {
        iFirstPageIdx[iLevel] = 0;
        // nSubPagesCount[iLevel] == 1 && GetUInt32(abyPage[iLevel] + m_nLeafPageHeaderSize, 0) ==
        // 0 should only happen on non-nominal cases where one forces the depth
        // of the index to be greater than needed.
        if (m_nVersion == 1)
        {
            iLastPageIdx[iLevel] =
                (nSubPagesCount[iLevel] == 1 &&
                 GetUInt32(abyPage[iLevel] + m_nLeafPageHeaderSize, 0) == 0)
                    ? 0
                    : 1;
        }
        else
        {
            iLastPageIdx[iLevel] =
                (nSubPagesCount[iLevel] == 1 &&
                 GetUInt64(abyPage[iLevel] + m_nLeafPageHeaderSize, 0) == 0)
                    ? 0
                    : 1;
        }
    }
    else if (!FindMinMaxIdx(abyPage[iLevel] + m_nOffsetFirstValInPage,
                            static_cast<int>(nSubPagesCount[iLevel]), m_nMinVal,
                            m_nMaxVal, iFirstPageIdx[iLevel],
                            iLastPageIdx[iLevel]))
    {
        iFirstPageIdx[iLevel] = iLastPageIdx[iLevel] = nSubPagesCount[iLevel];
    }
    else if (iLastPageIdx[iLevel] < static_cast<int>(nSubPagesCount[iLevel]))
    {
        // Candidate values might extend to the following sub-page
        iLastPageIdx[iLevel]++;
    }

    return true;
}

/************************************************************************/
/*                              GetNextRow()                            */
/************************************************************************/

int FileGDBSpatialIndexIteratorImpl::GetNextRow()
{
    const int errorRetValue = -1;
    if (bEOF)
        return -1;

    while (true)
    {
        if (iCurFeatureInPage >= nFeaturesInPage)
        {
            int nMinIdx = 0;
            int nMaxIdx = 0;
            if (!LoadNextFeaturePage() ||
                !FindMinMaxIdx(abyPageFeature + m_nOffsetFirstValInPage,
                               nFeaturesInPage, m_nMinVal, m_nMaxVal, nMinIdx,
                               nMaxIdx) ||
                nMinIdx > nMaxIdx)
            {
                if (m_nCurX < m_nMaxX)
                {
                    m_nCurX++;
                    if (ReadNewXRange())
                        continue;
                }
                else
                {
                    const auto &gridRes =
                        poParent->GetSpatialIndexGridResolution();
                    if (m_nGridNo + 1 < static_cast<int>(gridRes.size()) &&
                        gridRes[m_nGridNo + 1] > 0)
                    {
                        m_nGridNo++;
                        m_nCurX = static_cast<GInt32>(std::min(
                            std::max(0.0,
                                     GetScaledCoord(m_sFilterEnvelope.MinX)),
                            static_cast<double>(INT_MAX)));
                        m_nMaxX = static_cast<GInt32>(std::min(
                            std::max(0.0,
                                     GetScaledCoord(m_sFilterEnvelope.MaxX)),
                            static_cast<double>(INT_MAX)));
                        if (ReadNewXRange())
                            continue;
                    }
                }

                bEOF = true;
                return -1;
            }

            iCurFeatureInPage = nMinIdx;
            nFeaturesInPage = nMaxIdx + 1;
        }

#ifdef DEBUG
        const GInt64 nVal = GetInt64(abyPageFeature + m_nOffsetFirstValInPage,
                                     iCurFeatureInPage);
        CPL_IGNORE_RET_VAL(nVal);
        CPLAssert(nVal >= m_nMinVal && nVal <= m_nMaxVal);
#endif

        const GUInt64 nFID =
            m_nVersion == 1 ? GetUInt32(abyPageFeature + m_nLeafPageHeaderSize,
                                        iCurFeatureInPage)
                            : GetUInt64(abyPageFeature + m_nLeafPageHeaderSize,
                                        iCurFeatureInPage);
        iCurFeatureInPage++;
        returnErrorAndCleanupIf(
            nFID < 1 ||
                nFID > static_cast<GUInt64>(poParent->GetTotalRecordCount()),
            bEOF = true);
        return static_cast<int>(nFID - 1);
    }
}

/************************************************************************/
/*                             Reset()                                  */
/************************************************************************/

bool FileGDBSpatialIndexIteratorImpl::ResetInternal()
{
    m_nGridNo = 0;

    const auto &gridRes = poParent->GetSpatialIndexGridResolution();
    if (gridRes.empty() ||  // shouldn't happen
        !(gridRes[0] > 0))
    {
        return false;
    }

    m_nCurX = static_cast<GInt32>(
        std::min(std::max(0.0, GetScaledCoord(m_sFilterEnvelope.MinX)),
                 static_cast<double>(INT_MAX)));
    m_nMaxX = static_cast<GInt32>(
        std::min(std::max(0.0, GetScaledCoord(m_sFilterEnvelope.MaxX)),
                 static_cast<double>(INT_MAX)));
    m_nVectorIdx = 0;
    return ReadNewXRange();
}

void FileGDBSpatialIndexIteratorImpl::Reset()
{
    ResetInternal();
}

/************************************************************************/
/*                        GetNextRowSortedByFID()                       */
/************************************************************************/

int64_t FileGDBSpatialIndexIteratorImpl::GetNextRowSortedByFID()
{
    if (m_nVectorIdx == 0)
    {
        if (!m_bHasBuiltSetFID)
        {
            m_bHasBuiltSetFID = true;
            // Accumulating in a vector and sorting is measurably faster
            // than using a unordered_set (or set)
            while (true)
            {
                const auto nFID = GetNextRow();
                if (nFID < 0)
                    break;
                m_oFIDVector.push_back(nFID);
            }
            std::sort(m_oFIDVector.begin(), m_oFIDVector.end());
        }

        if (m_oFIDVector.empty())
            return -1;
        const auto nFID = m_oFIDVector[m_nVectorIdx];
        ++m_nVectorIdx;
        return nFID;
    }

    const auto nLastFID = m_oFIDVector[m_nVectorIdx - 1];
    while (m_nVectorIdx < m_oFIDVector.size())
    {
        // Do not return consecutive identical FID
        const auto nFID = m_oFIDVector[m_nVectorIdx];
        ++m_nVectorIdx;
        if (nFID == nLastFID)
        {
            continue;
        }
        return nFID;
    }
    return -1;
}

} /* namespace OpenFileGDB */
