/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements reading of FileGDB indexes
 * Author:   Even Rouault, <even dot rouault at mines-dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "filegdbtable_priv.h"
#include "cpl_string.h"
#include "cpl_time.h"
#include <algorithm>

CPL_CVSID("$Id");

namespace OpenFileGDB
{

/************************************************************************/
/*                     FileGDBOGRDateToDoubleDate()                     */
/************************************************************************/

static int FileGDBOGRDateToDoubleDate( const OGRField* psField, double *pdfVal )
{
    struct tm brokendowntime;

    brokendowntime.tm_year = psField->Date.Year - 1900;
    brokendowntime.tm_mon = psField->Date.Month - 1;
    brokendowntime.tm_mday = psField->Date.Day;
    brokendowntime.tm_hour = psField->Date.Hour;
    brokendowntime.tm_min = psField->Date.Minute;
    brokendowntime.tm_sec = psField->Date.Second;

    GIntBig nTime = CPLYMDHMSToUnixTime(&brokendowntime);

    *pdfVal = nTime / 3600. / 24 + 25569;

    return TRUE;
}

/************************************************************************/
/*                        FileGDBTrivialIterator                        */
/************************************************************************/

class FileGDBTrivialIterator : public FileGDBIterator
{
        FileGDBIterator            *poParentIter;
        FileGDBTable               *poTable;
        int                         iRow;

    public:
                                     FileGDBTrivialIterator(FileGDBIterator *poParentIter);
        virtual                     ~FileGDBTrivialIterator() { delete poParentIter; }

        virtual FileGDBTable        *GetTable() { return poTable; }
        virtual void                 Reset() { iRow = 0; poParentIter->Reset(); }
        virtual int                  GetNextRowSortedByFID();
        virtual int                  GetRowCount()
                { return poTable->GetTotalRecordCount(); }

        virtual int                  GetNextRowSortedByValue()
                { return poParentIter->GetNextRowSortedByValue(); }

        virtual const OGRField*      GetMinValue(int& eOutType)
                { return poParentIter->GetMinValue(eOutType); }
        virtual const OGRField*      GetMaxValue(int& eOutType)
                { return poParentIter->GetMaxValue(eOutType); }
        virtual int                  GetMinMaxSumCount(double& dfMin, double& dfMax,
                                                       double& dfSum, int& nCount)
            { return poParentIter->GetMinMaxSumCount(dfMin, dfMax, dfSum, nCount); }
};

/************************************************************************/
/*                        FileGDBNotIterator                            */
/************************************************************************/

class FileGDBNotIterator : public FileGDBIterator
{
        FileGDBIterator            *poIterBase;
        FileGDBTable               *poTable;
        int                         iRow;
        int                         iNextRowBase;
        int                         bNoHoles;

    public:
                                     FileGDBNotIterator(FileGDBIterator* poIterBase);
        virtual                     ~FileGDBNotIterator();

        virtual FileGDBTable        *GetTable() { return poTable; }
        virtual void                 Reset();
        virtual int                  GetNextRowSortedByFID();
        virtual int                  GetRowCount();
};

/************************************************************************/
/*                        FileGDBAndIterator                            */
/************************************************************************/

class FileGDBAndIterator : public FileGDBIterator
{
        FileGDBIterator             *poIter1;
        FileGDBIterator             *poIter2;
        int                          iNextRow1;
        int                          iNextRow2;

    public:
                                     FileGDBAndIterator(FileGDBIterator* poIter1,
                                                        FileGDBIterator* poIter2);
        virtual                     ~FileGDBAndIterator();

        virtual FileGDBTable        *GetTable() { return poIter1->GetTable(); }
        virtual void                 Reset();
        virtual int                  GetNextRowSortedByFID();
};

/************************************************************************/
/*                        FileGDBOrIterator                             */
/************************************************************************/

class FileGDBOrIterator : public FileGDBIterator
{
        FileGDBIterator             *poIter1;
        FileGDBIterator             *poIter2;
        int                          bIteratorAreExclusive;
        int                          iNextRow1;
        int                          iNextRow2;
        int                          bHasJustReset;

    public:
                                     FileGDBOrIterator(FileGDBIterator* poIter1,
                                                       FileGDBIterator* poIter2,
                                                       int bIteratorAreExclusive = FALSE);
        virtual                     ~FileGDBOrIterator();

        virtual FileGDBTable        *GetTable() { return poIter1->GetTable(); }
        virtual void                 Reset();
        virtual int                  GetNextRowSortedByFID();
        virtual int                  GetRowCount();
};

/************************************************************************/
/*                        FileGDBIndexIterator                          */
/************************************************************************/

#define MAX_DEPTH               3
#define UUID_LEN_AS_STRING      38
#define MAX_CAR_COUNT_STR       80
#define MAX_UTF8_LEN_STR        (4 * MAX_CAR_COUNT_STR)
#define FGDB_PAGE_SIZE          4096

class FileGDBIndexIterator : public FileGDBIterator
{
        FileGDBTable        *poParent;
        int                  bAscending;
        VSILFILE            *fpCurIdx;
        FileGDBFieldType     eFieldType;
        GUInt32              nMaxPerPages;
        GUInt32              nOffsetFirstValInPage;
        GUInt32              nValueCountInIdx;
        GUInt32              nIndexDepth;
        FileGDBSQLOp         eOp;
        OGRField             sValue;

        int                  iFirstPageIdx[MAX_DEPTH],
                             iLastPageIdx[MAX_DEPTH],
                             iCurPageIdx[MAX_DEPTH];
        GUInt32              nSubPagesCount[MAX_DEPTH];
        GUInt32              nLastPageAccessed[MAX_DEPTH];

        int                  iCurFeatureInPage, nFeaturesInPage;

        int                  bEvaluateToFALSE;
        int                  bEOF;

        int                  iSorted;
        int                  nSortedCount;
        int                 *panSortedRows;
        int                  SortRows();

        GUInt16              asUTF16Str[MAX_CAR_COUNT_STR];
        int                  nStrLen;
        char                 szUUID[UUID_LEN_AS_STRING + 1];
        GByte                abyPage[MAX_DEPTH][FGDB_PAGE_SIZE];
        GByte                abyPageFeature[FGDB_PAGE_SIZE];

        OGRField             sMin, sMax;
        char                 szMin[MAX_UTF8_LEN_STR+1];
        char                 szMax[MAX_UTF8_LEN_STR+1];
        const OGRField*      GetMinMaxValue(OGRField* psField,
                                            int& eOutType,
                                            int bIsMin);

        int                  ReadPageNumber(int iLevel);
        int                  LoadNextPage(int iLevel);
        int                  FindPages(int iLevel, int nPage);
        int                  LoadNextFeaturePage();

        int                  GetNextRow();

                             FileGDBIndexIterator(FileGDBTable* poParent,
                                                  int bAscending);
        int                  SetConstraint(int nFieldIdx, FileGDBSQLOp op,
                                           OGRFieldType eOGRFieldType,
                                           const OGRField* psValue);

        template <class Getter> void GetMinMaxSumCount(
                                                  double& dfMin, double& dfMax,
                                                  double& dfSum, int& nCount);
    public:
        virtual             ~FileGDBIndexIterator();

        static FileGDBIterator*      Build(FileGDBTable* poParent,
                                           int nFieldIdx,
                                           int bAscending,
                                           FileGDBSQLOp op,
                                           OGRFieldType eOGRFieldType,
                                           const OGRField* psValue);

        virtual FileGDBTable        *GetTable() { return poParent; }
        virtual void                 Reset();
        virtual int                  GetNextRowSortedByFID();
        virtual int                  GetRowCount();

        virtual int                  GetNextRowSortedByValue() { return GetNextRow(); }

        virtual const OGRField*      GetMinValue(int& eOutType);
        virtual const OGRField*      GetMaxValue(int& eOutType);
        virtual int                  GetMinMaxSumCount(double& dfMin, double& dfMax,
                                                       double& dfSum, int& nCount);
};

/************************************************************************/
/*                            GetMinValue()                             */
/************************************************************************/

const OGRField* FileGDBIterator::GetMinValue(int& eOutType)
{
    PrintError();
    eOutType = -1;
    return NULL;
}

/************************************************************************/
/*                            GetMaxValue()                             */
/************************************************************************/

const OGRField* FileGDBIterator::GetMaxValue(int& eOutType)
{
    PrintError();
    eOutType = -1;
    return NULL;
}

/************************************************************************/
/*                       GetNextRowSortedByValue()                      */
/************************************************************************/

int FileGDBIterator::GetNextRowSortedByValue()
{
    PrintError();
    return -1;
}

/************************************************************************/
/*                        GetMinMaxSumCount()                           */
/************************************************************************/

int FileGDBIterator::GetMinMaxSumCount(double& dfMin, double& dfMax,
                                       double& dfSum, int& nCount)
{
    PrintError();
    dfMin = 0.0;
    dfMax = 0.0;
    dfSum = 0.0;
    nCount = 0;
    return FALSE;
}

/************************************************************************/
/*                             Build()                                  */
/************************************************************************/

FileGDBIterator* FileGDBIterator::Build(FileGDBTable* poParent,
                                        int nFieldIdx,
                                        int bAscending,
                                        FileGDBSQLOp op,
                                        OGRFieldType eOGRFieldType,
                                        const OGRField* psValue)
{
    return FileGDBIndexIterator::Build(poParent, nFieldIdx, bAscending,
                                       op, eOGRFieldType, psValue);
}

/************************************************************************/
/*                           BuildIsNotNull()                           */
/************************************************************************/

FileGDBIterator* FileGDBIterator::BuildIsNotNull(FileGDBTable* poParent,
                                                 int nFieldIdx,
                                                 int bAscending)
{
    FileGDBIterator* poIter = Build(poParent, nFieldIdx, bAscending,
                                    FGSO_ISNOTNULL, OFTMaxType, NULL);
    if( poIter != NULL )
    {
        /* Optimization */
        if( poIter->GetRowCount() == poParent->GetTotalRecordCount() )
        {
            CPLAssert(poParent->GetValidRecordCount() == poParent->GetTotalRecordCount());
            poIter = new FileGDBTrivialIterator(poIter);
        }
    }
    return poIter;
}

/************************************************************************/
/*                              BuildNot()                              */
/************************************************************************/

FileGDBIterator* FileGDBIterator::BuildNot(FileGDBIterator* poIterBase)
{
    return new FileGDBNotIterator(poIterBase);
}

/************************************************************************/
/*                               BuildAnd()                             */
/************************************************************************/

FileGDBIterator* FileGDBIterator::BuildAnd(FileGDBIterator* poIter1,
                                           FileGDBIterator* poIter2)
{
    return new FileGDBAndIterator(poIter1, poIter2);
}

/************************************************************************/
/*                               BuildOr()                              */
/************************************************************************/

FileGDBIterator* FileGDBIterator::BuildOr(FileGDBIterator* poIter1,
                                          FileGDBIterator* poIter2,
                                          int bIteratorAreExclusive)
{
    return new FileGDBOrIterator(poIter1, poIter2, bIteratorAreExclusive);
}

/************************************************************************/
/*                           GetRowCount()                              */
/************************************************************************/

int FileGDBIterator::GetRowCount()
{
    Reset();
    int nCount = 0;
    while( GetNextRowSortedByFID() >= 0 )
        nCount ++;
    Reset();
    return nCount;
}

/************************************************************************/
/*                         FileGDBTrivialIterator()                     */
/************************************************************************/

FileGDBTrivialIterator::FileGDBTrivialIterator(FileGDBIterator* poParentIter) :
        poParentIter(poParentIter), poTable(poParentIter->GetTable()), iRow(0)
{
}

/************************************************************************/
/*                        GetNextRowSortedByFID()                       */
/************************************************************************/

int FileGDBTrivialIterator::GetNextRowSortedByFID()
{
    if( iRow < poTable->GetTotalRecordCount() )
        return iRow ++;
    else
        return -1;
}

/************************************************************************/
/*                           FileGDBNotIterator()                       */
/************************************************************************/

FileGDBNotIterator::FileGDBNotIterator(FileGDBIterator* poIterBase) :
    poIterBase(poIterBase), poTable(poIterBase->GetTable()), iRow(0), iNextRowBase(-1)
{
    bNoHoles = (poTable->GetValidRecordCount() == poTable->GetTotalRecordCount());
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

int FileGDBNotIterator::GetNextRowSortedByFID()
{
    if( iNextRowBase < 0 )
    {
        iNextRowBase = poIterBase->GetNextRowSortedByFID();
        if( iNextRowBase < 0 )
            iNextRowBase = poTable->GetTotalRecordCount();
    }

    while( TRUE )
    {
        if( iRow < iNextRowBase )
        {
            if( bNoHoles )
                return iRow ++;
            else if( poTable->GetOffsetInTableForRow(iRow) )
                return iRow ++;
            else if( !poTable->HasGotError() )
                iRow ++;
            else
                return -1;
        }
        else if( iRow == poTable->GetTotalRecordCount() )
            return -1;
        else
        {
            iRow = iNextRowBase + 1;
            iNextRowBase = poIterBase->GetNextRowSortedByFID();
            if( iNextRowBase < 0 )
                iNextRowBase = poTable->GetTotalRecordCount();
        }
    }
}

/************************************************************************/
/*                           GetRowCount()                              */
/************************************************************************/

int FileGDBNotIterator::GetRowCount()
{
    return poTable->GetValidRecordCount() - poIterBase->GetRowCount();
}


/************************************************************************/
/*                          FileGDBAndIterator()                        */
/************************************************************************/

FileGDBAndIterator::FileGDBAndIterator(FileGDBIterator* poIter1,
                                       FileGDBIterator* poIter2) :
                                       poIter1(poIter1), poIter2(poIter2),
                                       iNextRow1(-1), iNextRow2(-1)
{
    CPLAssert(poIter1->GetTable() == poIter2->GetTable());
}

/************************************************************************/
/*                          ~FileGDBAndIterator()                       */
/************************************************************************/

FileGDBAndIterator::~FileGDBAndIterator()
{
    delete poIter1;
    delete poIter2;
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

int FileGDBAndIterator::GetNextRowSortedByFID()
{
    if( iNextRow1 == iNextRow2 )
    {
        iNextRow1 = poIter1->GetNextRowSortedByFID();
        iNextRow2 = poIter2->GetNextRowSortedByFID();
        if( iNextRow1 < 0 || iNextRow2 < 0 )
        {
            return -1;
        }
    }

    while( TRUE )
    {
        if( iNextRow1 < iNextRow2 )
        {
            iNextRow1 = poIter1->GetNextRowSortedByFID();
            if( iNextRow1 < 0 )
                return -1;
        }
        else if( iNextRow2 < iNextRow1 )
        {
            iNextRow2 = poIter2->GetNextRowSortedByFID();
            if( iNextRow2 < 0 )
                return -1;
        }
        else
            return iNextRow1;
    }
}

/************************************************************************/
/*                          FileGDBOrIterator()                         */
/************************************************************************/


FileGDBOrIterator::FileGDBOrIterator(FileGDBIterator* poIter1,
                                     FileGDBIterator* poIter2,
                                     int bIteratorAreExclusive) :
                            poIter1(poIter1), poIter2(poIter2),
                            bIteratorAreExclusive(bIteratorAreExclusive),
                            iNextRow1(-1), iNextRow2(-1), bHasJustReset(TRUE)
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
    bHasJustReset = TRUE;
}

/************************************************************************/
/*                        GetNextRowSortedByFID()                       */
/************************************************************************/

int FileGDBOrIterator::GetNextRowSortedByFID()
{
    if( bHasJustReset )
    {
        bHasJustReset = FALSE;
        iNextRow1 = poIter1->GetNextRowSortedByFID();
        iNextRow2 = poIter2->GetNextRowSortedByFID();
    }

    if( iNextRow1 < 0 )
    {
        int iVal = iNextRow2;
        iNextRow2 = poIter2->GetNextRowSortedByFID();
        return iVal;
    }
    if( iNextRow2 < 0 || iNextRow1 < iNextRow2 )
    {
        int iVal = iNextRow1;
        iNextRow1 = poIter1->GetNextRowSortedByFID();
        return iVal;
    }
    if( iNextRow2 < iNextRow1 )
    {
        int iVal = iNextRow2;
        iNextRow2 = poIter2->GetNextRowSortedByFID();
        return iVal;
    }

    if( bIteratorAreExclusive )
        PrintError();

    int iVal = iNextRow1;
    iNextRow1 = poIter1->GetNextRowSortedByFID();
    iNextRow2 = poIter2->GetNextRowSortedByFID();
    return iVal;
}

/************************************************************************/
/*                           GetRowCount()                              */
/************************************************************************/

int FileGDBOrIterator::GetRowCount()
{
    if( bIteratorAreExclusive )
        return poIter1->GetRowCount() + poIter2->GetRowCount();
    else
        return FileGDBIterator::GetRowCount();
}

/************************************************************************/
/*                         FileGDBIndexIterator()                       */
/************************************************************************/

FileGDBIndexIterator::FileGDBIndexIterator(FileGDBTable* poParent, int bAscending) :
                    poParent(poParent), bAscending(bAscending),
                    fpCurIdx(NULL), eFieldType(FGFT_UNDEFINED),
                    nMaxPerPages(0), nOffsetFirstValInPage(0),
                    nValueCountInIdx(0), nIndexDepth(0), eOp(FGSO_ISNOTNULL),
                    iCurFeatureInPage(-1), nFeaturesInPage(0),
                    bEvaluateToFALSE(FALSE), bEOF(FALSE),
                    iSorted(0), nSortedCount(-1), panSortedRows(NULL),
                    nStrLen(0)
{
    memset(iFirstPageIdx, 0xFF, MAX_DEPTH * sizeof(int));
    memset(iLastPageIdx, 0xFF, MAX_DEPTH * sizeof(int));
    memset(iCurPageIdx, 0xFF, MAX_DEPTH * sizeof(int));
    memset(nSubPagesCount, 0, MAX_DEPTH * sizeof(int));
    memset(nLastPageAccessed, 0, MAX_DEPTH * sizeof(int));
    memset(&sValue, 0, sizeof(sValue));
}

/************************************************************************/
/*                         ~FileGDBIndexIterator()                      */
/************************************************************************/

FileGDBIndexIterator::~FileGDBIndexIterator()
{
    if( fpCurIdx )
        VSIFCloseL(fpCurIdx);
    fpCurIdx = NULL;
    VSIFree(panSortedRows);
}

/************************************************************************/
/*                             Build()                                  */
/************************************************************************/

FileGDBIterator* FileGDBIndexIterator::Build( FileGDBTable* poParent,
                                              int nFieldIdx,
                                              int bAscending,
                                              FileGDBSQLOp op,
                                              OGRFieldType eOGRFieldType,
                                              const OGRField* psValue )
{
    FileGDBIndexIterator* poIndexIterator =
                new FileGDBIndexIterator(poParent, bAscending);
    if( poIndexIterator->SetConstraint(nFieldIdx, op, eOGRFieldType, psValue) )
    {
        return poIndexIterator;
    }
    delete poIndexIterator;
    return NULL;
}

/************************************************************************/
/*                           FileGDBSQLOpToStr()                        */
/************************************************************************/

static const char* FileGDBSQLOpToStr(FileGDBSQLOp op)
{
    switch( op )
    {
        case FGSO_ISNOTNULL : return "IS NOT NULL";
        case FGSO_LT: return "<";;
        case FGSO_LE: return "<=";
        case FGSO_EQ: return "=";
        case FGSO_GE: return ">=";
        case FGSO_GT: return ">";
    }
    return "unknown_op";
}

/************************************************************************/
/*                           FileGDBValueToStr()                        */
/************************************************************************/

static const char* FileGDBValueToStr(OGRFieldType eOGRFieldType,
                                     const OGRField* psValue)
{
    if( psValue == NULL )
        return "";

    switch( eOGRFieldType )
    {
        case OFTInteger: return CPLSPrintf("%d", psValue->Integer);
        case OFTReal: return CPLSPrintf("%.18g", psValue->Real);
        case OFTString: return psValue->String;
        case OFTDateTime: return CPLSPrintf("%04d/%02d/%02d %02d:%02d:%02d",
                                            psValue->Date.Year,
                                            psValue->Date.Month,
                                            psValue->Date.Day,
                                            psValue->Date.Hour,
                                            psValue->Date.Minute,
                                            (int)psValue->Date.Second);
        case OFTDate: return CPLSPrintf("%04d/%02d/%02d",
                                            psValue->Date.Year,
                                            psValue->Date.Month,
                                            psValue->Date.Day);
        case OFTTime: return CPLSPrintf("%02d:%02d:%02d",
                                            psValue->Date.Hour,
                                            psValue->Date.Minute,
                                            (int)psValue->Date.Second);
        default:
            break;
    }
    return "";
}

/************************************************************************/
/*                           SetConstraint()                            */
/************************************************************************/

int FileGDBIndexIterator::SetConstraint(int nFieldIdx,
                                        FileGDBSQLOp op,
                                        OGRFieldType eOGRFieldType,
                                        const OGRField* psValue)
{
    const int errorRetValue = FALSE;
    CPLAssert(fpCurIdx == NULL);

    returnErrorIf(nFieldIdx < 0 || nFieldIdx >= poParent->GetFieldCount() );
    FileGDBField* poField = poParent->GetField(nFieldIdx);
    returnErrorIf(!(poField->HasIndex()) );

    eFieldType = poField->GetType();
    eOp = op;

    returnErrorIf(eFieldType != FGFT_INT16 && eFieldType != FGFT_INT32 &&
                  eFieldType != FGFT_FLOAT32 && eFieldType != FGFT_FLOAT64 &&
                  eFieldType != FGFT_STRING && eFieldType != FGFT_DATETIME &&
                  eFieldType != FGFT_UUID_1 && eFieldType != FGFT_UUID_2 );

    const char* pszAtxName = CPLFormFilename(CPLGetPath(poParent->GetFilename().c_str()),
                    CPLGetBasename(poParent->GetFilename().c_str()), CPLSPrintf("%s.atx",
                    poField->GetIndex()->GetIndexName().c_str()));
    fpCurIdx = VSIFOpenL( pszAtxName, "rb" );
    returnErrorIf(fpCurIdx == NULL );

    VSIFSeekL(fpCurIdx, 0, SEEK_END);
    vsi_l_offset nFileSize = VSIFTellL(fpCurIdx);
    returnErrorIf(nFileSize < FGDB_PAGE_SIZE + 22 );

    VSIFSeekL(fpCurIdx, nFileSize - 22, SEEK_SET);
    GByte abyTrailer[22];
    returnErrorIf(VSIFReadL( abyTrailer, 22, 1, fpCurIdx ) != 1 );

    nMaxPerPages = (FGDB_PAGE_SIZE - 12) / (4 + abyTrailer[0]);
    nOffsetFirstValInPage = 12 + nMaxPerPages * 4;

    GUInt32 nMagic1 = GetUInt32(abyTrailer + 2, 0);
    returnErrorIf(nMagic1 != 1 );

    nIndexDepth = GetUInt32(abyTrailer + 6, 0);
    /* CPLDebug("OpenFileGDB", "nIndexDepth = %u", nIndexDepth); */
    returnErrorIf(!(nIndexDepth >= 1 && nIndexDepth <= MAX_DEPTH + 1) );

    nValueCountInIdx = GetUInt32(abyTrailer + 10, 0);
    /* CPLDebug("OpenFileGDB", "nValueCountInIdx = %u", nValueCountInIdx); */
    /* negative like in sample_clcV15_esri_v10.gdb/a00000005.FDO_UUID.atx */
    if( (int)nValueCountInIdx < 0 )
        return FALSE;
    /* QGIS_TEST_101.gdb/a00000006.FDO_UUID.atx */
    if( nValueCountInIdx == 0 )
    {
        VSIFSeekL(fpCurIdx, 4, SEEK_SET);
        GByte abyBuffer[4];
        returnErrorIf(VSIFReadL( abyBuffer, 4, 1, fpCurIdx ) != 1 );
        nValueCountInIdx = GetUInt32(abyBuffer, 0);
    }
    /* PreNIS.gdb/a00000006.FDO_UUID.atx has depth 2 and the value of */
    /* nValueCountInIdx is 11 which is not the number of non-null values */
    else if( nValueCountInIdx < nMaxPerPages && nIndexDepth > 1 )
        return FALSE;
    returnErrorIf(nValueCountInIdx > (GUInt32)poParent->GetValidRecordCount() );

    switch( eFieldType )
    {
        case FGFT_INT16:
            returnErrorIf(abyTrailer[0] != sizeof(GUInt16));
            if( eOp != FGSO_ISNOTNULL )
            {
                returnErrorIf(eOGRFieldType != OFTInteger);
                sValue.Integer = psValue->Integer;
            }
            break;
        case FGFT_INT32:
            returnErrorIf(abyTrailer[0] != sizeof(GUInt32));
            if( eOp != FGSO_ISNOTNULL )
            {
                returnErrorIf(eOGRFieldType != OFTInteger);
                sValue.Integer = psValue->Integer;
            }
            break;
        case FGFT_FLOAT32:
            returnErrorIf(abyTrailer[0] != sizeof(float));
            if( eOp != FGSO_ISNOTNULL )
            {
                returnErrorIf(eOGRFieldType != OFTReal);
                sValue.Real = psValue->Real;
            }
            break;
        case FGFT_FLOAT64:
            returnErrorIf(abyTrailer[0] != sizeof(double));
            if( eOp != FGSO_ISNOTNULL )
            {
                returnErrorIf(eOGRFieldType != OFTReal);
                sValue.Real = psValue->Real;
            }
            break;
        case FGFT_STRING:
        {
            returnErrorIf((abyTrailer[0] % 2) != 0);
            returnErrorIf(abyTrailer[0] == 0);
            returnErrorIf(abyTrailer[0] > 2 * MAX_CAR_COUNT_STR);
            nStrLen = abyTrailer[0] / 2;
            if( eOp != FGSO_ISNOTNULL )
            {
                returnErrorIf(eOGRFieldType != OFTString);
                wchar_t *pWide = CPLRecodeToWChar( psValue->String,
                                                CPL_ENC_UTF8,
                                                CPL_ENC_UCS2 );
                returnErrorIf(pWide == NULL);
                int nCount = 0;
                while( pWide[nCount] != 0 )
                {
                    returnErrorAndCleanupIf(nCount == nStrLen, CPLFree(pWide));
                    asUTF16Str[nCount] = pWide[nCount];
                    nCount ++;
                }
                while( nCount < nStrLen )
                {
                    asUTF16Str[nCount] = 32; /* space character */
                    nCount ++;
                }
                CPLFree(pWide);
            }
            break;
        }

        case FGFT_DATETIME:
        {
            returnErrorIf( abyTrailer[0] != sizeof(double));
            if( eOp != FGSO_ISNOTNULL )
            {
                returnErrorIf(eOGRFieldType != OFTReal &&
                              eOGRFieldType != OFTDateTime &&
                              eOGRFieldType != OFTDate &&
                              eOGRFieldType != OFTTime);
                if( eOGRFieldType == OFTReal )
                    sValue.Real = psValue->Real;
                else
                    FileGDBOGRDateToDoubleDate(psValue, &(sValue.Real));
            }
            break;
        }

        case FGFT_UUID_1:
        case FGFT_UUID_2:
        {
            returnErrorIf(abyTrailer[0] != UUID_LEN_AS_STRING);
            if( eOp != FGSO_ISNOTNULL )
            {
                returnErrorIf(eOGRFieldType != OFTString);
                memset(szUUID, 0, UUID_LEN_AS_STRING + 1);
                strncpy(szUUID, psValue->String, UUID_LEN_AS_STRING);
                bEvaluateToFALSE = (eOp == FGSO_EQ &&
                        strlen(psValue->String) != UUID_LEN_AS_STRING);
            }
            break;
        }

        default:
            CPLAssert(FALSE);
            break;
    }

    if( nValueCountInIdx > 0 )
    {
        if( nIndexDepth == 1 )
        {
            iFirstPageIdx[0] = iLastPageIdx[0] = 0;
        }
        else
        {
            returnErrorIf(!FindPages(0, 1) );
        }
    }

    CPLDebug("OpenFileGDB", "Using index on field %s (%s %s)",
            poField->GetName().c_str(),
            FileGDBSQLOpToStr(eOp),
            FileGDBValueToStr(eOGRFieldType, psValue));

    Reset();

    return TRUE;
}

/************************************************************************/
/*                          FileGDBUTF16StrCompare()                    */
/************************************************************************/

static int FileGDBUTF16StrCompare(const GUInt16* pasFirst,
                                  const GUInt16* pasSecond,
                                  int nStrLen)
{
    for(int i=0;i<nStrLen;i++)
    {
        if( pasFirst[i] < pasSecond[i] )
            return -1;
        if( pasFirst[i] > pasSecond[i] )
            return 1;
    }
    return 0;
}

/************************************************************************/
/*                              COMPARE()                               */
/************************************************************************/

#define COMPARE(a,b) (((a)<(b)) ? -1 : ((a)==(b)) ? 0 : 1)

/************************************************************************/
/*                             FindPages()                              */
/************************************************************************/

int FileGDBIndexIterator::FindPages(int iLevel, int nPage)
{
    const int errorRetValue = FALSE;
    VSIFSeekL(fpCurIdx, (nPage - 1) * FGDB_PAGE_SIZE, SEEK_SET);
    returnErrorIf(VSIFReadL( abyPage[iLevel], FGDB_PAGE_SIZE, 1, fpCurIdx ) != 1 );

    nSubPagesCount[iLevel] = GetUInt32(abyPage[iLevel] + 4, 0);
    returnErrorIf(nSubPagesCount[iLevel] == 0 ||
                  nSubPagesCount[iLevel] > nMaxPerPages);
    if( nIndexDepth == 2 )
        returnErrorIf(nValueCountInIdx > nMaxPerPages * (nSubPagesCount[0] + 1));

    if( eOp == FGSO_ISNOTNULL )
    {
        iFirstPageIdx[iLevel] = 0;
        iLastPageIdx[iLevel] = nSubPagesCount[iLevel];
        return TRUE;
    }

    GUInt32 i;
#ifdef DEBUG_INDEX_CONSISTENCY
    double dfLastMax = 0.0;
    int nLastMax = 0;
    GUInt16 asLastMax[MAX_CAR_COUNT_STR] = { 0 };
    char szLastMaxUUID[UUID_LEN_AS_STRING + 1] = { 0 };
#endif
    iFirstPageIdx[iLevel] = iLastPageIdx[iLevel] = -1;

    for( i = 0; i < nSubPagesCount[iLevel]; i ++ )
    {
        int nComp;

        switch( eFieldType )
        {
            case FGFT_INT16:
            {
                GInt16 nVal = GetInt16(abyPage[iLevel] + nOffsetFirstValInPage, i);
#ifdef DEBUG_INDEX_CONSISTENCY
                returnErrorIf(i > 0 && nVal < nLastMax);
                nLastMax = nVal;
#endif
                nComp = COMPARE(sValue.Integer, nVal);
                break;
            }

            case FGFT_INT32:
            {
                GInt32 nVal = GetInt32(abyPage[iLevel] + nOffsetFirstValInPage, i);
#ifdef DEBUG_INDEX_CONSISTENCY
                returnErrorIf(i > 0 && nVal < nLastMax);
                nLastMax = nVal;
#endif
                nComp = COMPARE(sValue.Integer, nVal);
                break;
            }

            case FGFT_FLOAT32:
            {
                float fVal = GetFloat32(abyPage[iLevel] + nOffsetFirstValInPage, i);
#ifdef DEBUG_INDEX_CONSISTENCY
                returnErrorIf(i > 0 && fVal < dfLastMax);
                dfLastMax = fVal;
#endif
                nComp = COMPARE(sValue.Real, fVal);
                break;
            }

            case FGFT_FLOAT64:
            case FGFT_DATETIME:
            {
                double dfVal = GetFloat64(abyPage[iLevel] + nOffsetFirstValInPage, i);
#ifdef DEBUG_INDEX_CONSISTENCY
                returnErrorIf(i > 0 && dfVal < dfLastMax);
                dfLastMax = dfVal;
#endif
                nComp = COMPARE(sValue.Real, dfVal);
                break;
            }

            case FGFT_STRING:
            {
                GUInt16* pasMax;
#ifdef CPL_MSB
                GUInt16 asMax[MAX_CAR_COUNT_STR];
                pasMax = asMax;
                memcpy(asMax, abyPage[iLevel] + nOffsetFirstValInPage +
                        nStrLen * sizeof(GUInt16) * i, nStrLen * sizeof(GUInt16));
                for(int j=0;j<nStrLen;j++)
                    CPL_LSBPTR16(&asMax[j]);
#else
                pasMax = (GUInt16*)(abyPage[iLevel] + nOffsetFirstValInPage +
                                                nStrLen * sizeof(GUInt16) * i);
#endif
#ifdef DEBUG_INDEX_CONSISTENCY
                returnErrorIf(i > 0 &&
                        FileGDBUTF16StrCompare(pasMax, asLastMax, nStrLen) < 0);
                memcpy(asLastMax, pasMax, nStrLen * 2);
#endif
                nComp = FileGDBUTF16StrCompare(asUTF16Str, pasMax, nStrLen);
                break;
            }

            case FGFT_UUID_1:
            case FGFT_UUID_2:
            {
                const char* psNonzMaxUUID = (char*)(abyPage[iLevel] +
                        nOffsetFirstValInPage + UUID_LEN_AS_STRING * i);
#ifdef DEBUG_INDEX_CONSISTENCY
                returnErrorIf(i > 0 &&
                    memcmp(psNonzMaxUUID, szLastMaxUUID, UUID_LEN_AS_STRING) < 0);
                memcpy(szLastMaxUUID, psNonzMaxUUID, UUID_LEN_AS_STRING);
#endif
                nComp = memcmp(szUUID, psNonzMaxUUID, UUID_LEN_AS_STRING);
                break;
            }

            default:
                CPLAssert(FALSE);
                nComp = 0;
                break;
        }

        int bStop = FALSE;
        switch( eOp )
        {
            /* dfVal = 1 2 2 3 3 4 */
            /* sValue.Real = 3 */
            /* nComp = (sValue.Real < dfVal) ? -1 : (sValue.Real == dfVal) ? 0 : 1; */
            case FGSO_LT:
            case FGSO_LE:
                if( iFirstPageIdx[iLevel] < 0 )
                {
                    iFirstPageIdx[iLevel] = iLastPageIdx[iLevel] = (int)i;
                }
                else
                {
                    iLastPageIdx[iLevel] = (int)i;
                    if( nComp < 0 )
                    {
                        bStop = TRUE;
                    }
                }
                break;

            case FGSO_EQ:
                if( iFirstPageIdx[iLevel] < 0 )
                {
                    if( nComp <= 0 )
                        iFirstPageIdx[iLevel] = iLastPageIdx[iLevel] = (int)i;
                }
                else
                {
                    if( nComp == 0 )
                        iLastPageIdx[iLevel] = (int)i;
                    else
                        bStop = TRUE;
                }
                break;

            case FGSO_GE:
                if( iFirstPageIdx[iLevel] < 0 )
                {
                    if( nComp <= 0 )
                    {
                        iFirstPageIdx[iLevel] = (int)i;
                        iLastPageIdx[iLevel] = nSubPagesCount[iLevel];
                        bStop = TRUE;
                    }
                }
                break;

            case FGSO_GT:
                if( iFirstPageIdx[iLevel] < 0 )
                {
                    if( nComp < 0 )
                    {
                        iFirstPageIdx[iLevel] = (int)i;
                        iLastPageIdx[iLevel] = nSubPagesCount[iLevel];
                        bStop = TRUE;
                    }
                }
                break;

            default:
                CPLAssert(FALSE);
                break;
        }
        if( bStop )
            break;
    }

    if( iFirstPageIdx[iLevel] < 0 )
    {
        iFirstPageIdx[iLevel] = iLastPageIdx[iLevel] = nSubPagesCount[iLevel];
    }
    else if( iLastPageIdx[iLevel] < (int)nSubPagesCount[iLevel] )
    {
        iLastPageIdx[iLevel] ++;
    }

    return TRUE;
}

/************************************************************************/
/*                             Reset()                                  */
/************************************************************************/

void FileGDBIndexIterator::Reset()
{
    iCurPageIdx[0] = (bAscending) ? iFirstPageIdx[0] -1 : iLastPageIdx[0] + 1;
    memset(iFirstPageIdx + 1, 0xFF, (MAX_DEPTH - 1) * sizeof(int));
    memset(iLastPageIdx + 1, 0xFF, (MAX_DEPTH - 1) * sizeof(int));
    memset(iCurPageIdx + 1, 0xFF, (MAX_DEPTH - 1) * sizeof(int));
    memset(nLastPageAccessed, 0, MAX_DEPTH * sizeof(int));
    iCurFeatureInPage = 0;
    nFeaturesInPage = 0;
    iSorted = 0;

    bEOF = ( nValueCountInIdx == 0 || bEvaluateToFALSE );
}

/************************************************************************/
/*                           ReadPageNumber()                           */
/************************************************************************/

int FileGDBIndexIterator::ReadPageNumber(int iLevel)
{
    const int errorRetValue = 0;
    GUInt32 nPage = GetUInt32(abyPage[iLevel] + 8, iCurPageIdx[iLevel]);
    if( nPage == nLastPageAccessed[iLevel] )
    {
        if( !LoadNextPage(iLevel) )
            return 0;
        nPage = GetUInt32(abyPage[iLevel] + 8, iCurPageIdx[iLevel]);
    }
    nLastPageAccessed[iLevel] = nPage;
    returnErrorIf(nPage < 2);
    return nPage;
}

/************************************************************************/
/*                           LoadNextPage()                             */
/************************************************************************/

int FileGDBIndexIterator::LoadNextPage(int iLevel)
{
    const int errorRetValue = FALSE;
    if( (bAscending && iCurPageIdx[iLevel] == iLastPageIdx[iLevel]) ||
        (!bAscending && iCurPageIdx[iLevel] == iFirstPageIdx[iLevel]) )
    {
        if( iLevel == 0 || !LoadNextPage(iLevel - 1) )
            return FALSE;

        GUInt32 nPage = ReadPageNumber(iLevel-1);
        returnErrorIf(!FindPages(iLevel, nPage) );

        iCurPageIdx[iLevel] = (bAscending) ? iFirstPageIdx[iLevel] :
                                             iLastPageIdx[iLevel];
    }
    else
    {
        if( bAscending )
            iCurPageIdx[iLevel] ++;
        else
            iCurPageIdx[iLevel] --;
    }

    return TRUE;
}

/************************************************************************/
/*                        LoadNextFeaturePage()                         */
/************************************************************************/

int FileGDBIndexIterator::LoadNextFeaturePage()
{
    const int errorRetValue = FALSE;
    GUInt32 nPage;

    if( nIndexDepth == 1 )
    {
        if( iCurPageIdx[0] == iLastPageIdx[0] )
        {
            return FALSE;
        }
        if( bAscending )
            iCurPageIdx[0] ++;
        else
            iCurPageIdx[0] --;
        nPage = 1;
    }
    else
    {
        if( !LoadNextPage( nIndexDepth - 2 ) )
        {
            return FALSE;
        }
        nPage = ReadPageNumber(nIndexDepth - 2);
        returnErrorIf(nPage < 2);
    }

    VSIFSeekL(fpCurIdx, (nPage - 1) * FGDB_PAGE_SIZE, SEEK_SET);
    returnErrorIf(VSIFReadL( abyPageFeature, FGDB_PAGE_SIZE, 1, fpCurIdx ) != 1);

    GUInt32 nFeatures = GetUInt32(abyPageFeature + 4, 0);
    returnErrorIf(nFeatures > nMaxPerPages);

    nFeaturesInPage = (int)nFeatures;
    iCurFeatureInPage = (bAscending) ? 0 : nFeaturesInPage - 1;
    if( nFeatures == 0 )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                              GetNextRow()                            */
/************************************************************************/

int FileGDBIndexIterator::GetNextRow()
{
    const int errorRetValue = -1;
    if( bEOF )
        return -1;

    while( TRUE )
    {
        if( iCurFeatureInPage >= nFeaturesInPage || iCurFeatureInPage < 0 )
        {
            if( !LoadNextFeaturePage() )
            {
                bEOF = TRUE;
                return -1;
            }
        }

        int bMatch;
        if( eOp == FGSO_ISNOTNULL )
        {
            bMatch = TRUE;
        }
        else
        {
            int nComp;
            bMatch = FALSE;
            switch( eFieldType )
            {
                case FGFT_INT16:
                {
                    GInt16 nVal = GetInt16(abyPageFeature + nOffsetFirstValInPage,
                                           iCurFeatureInPage);
                    nComp = COMPARE(sValue.Integer, nVal);
                    break;
                }

                case FGFT_INT32:
                {
                    GInt32 nVal = GetInt32(abyPageFeature + nOffsetFirstValInPage,
                                           iCurFeatureInPage);
                    nComp = COMPARE(sValue.Integer, nVal);
                    break;
                }

                case FGFT_FLOAT32:
                {
                    float fVal = GetFloat32(abyPageFeature + nOffsetFirstValInPage,
                                           iCurFeatureInPage);
                    nComp = COMPARE(sValue.Real, fVal);
                    break;
                }

                case FGFT_FLOAT64:
                case FGFT_DATETIME:
                {
                    double dfVal = GetFloat64(abyPageFeature + nOffsetFirstValInPage,
                                           iCurFeatureInPage);
                    nComp = COMPARE(sValue.Real, dfVal);
                    break;
                }

                case FGFT_STRING:
                {
#ifdef CPL_MSB
                    GUInt16 asVal[MAX_CAR_COUNT_STR];
                    memcpy(asVal, abyPageFeature + nOffsetFirstValInPage +
                                    nStrLen * 2 * iCurFeatureInPage, nStrLen * 2);
                    for(int j=0;j<nStrLen;j++)
                        CPL_LSBPTR16(&asVal[j]);
                    nComp = FileGDBUTF16StrCompare(asUTF16Str, asVal, nStrLen);
#else
                    nComp = FileGDBUTF16StrCompare(asUTF16Str,
                                (GUInt16*)(abyPageFeature + nOffsetFirstValInPage +
                                    nStrLen * 2 * iCurFeatureInPage), nStrLen);
#endif
                    break;
                }

                case FGFT_UUID_1:
                case FGFT_UUID_2:
                {
                    nComp = memcmp(szUUID,
                                abyPageFeature + nOffsetFirstValInPage +
                                UUID_LEN_AS_STRING *iCurFeatureInPage,
                                UUID_LEN_AS_STRING);
                    break;
                }

                default:
                    CPLAssert(FALSE);
                    nComp = 0;
                    break;
            }

            switch( eOp )
            {
                case FGSO_LT:
                    if( nComp <= 0 && bAscending )
                    {
                        bEOF = TRUE;
                        return -1;
                    }
                    bMatch = TRUE;
                    break;

                case FGSO_LE:
                    if( nComp < 0 && bAscending )
                    {
                        bEOF = TRUE;
                        return -1;
                    }
                    bMatch = TRUE;
                    break;

                case FGSO_EQ:
                    if( nComp < 0 && bAscending )
                    {
                        bEOF = TRUE;
                        return -1;
                    }
                    bMatch = ( nComp == 0 );
                    break;

                case FGSO_GE:
                    bMatch = ( nComp <= 0 );
                    break;

                case FGSO_GT:
                    bMatch = ( nComp < 0 );
                    break;

                default:
                    CPLAssert(FALSE);
                    break;
            }
        }

        if( bMatch )
        {
            GUInt32 nFID = GetUInt32(abyPageFeature + 12, iCurFeatureInPage);
            if( bAscending )
                iCurFeatureInPage ++;
            else
                iCurFeatureInPage --;
            returnErrorAndCleanupIf(nFID < 1 ||
                nFID > (GUInt32)poParent->GetTotalRecordCount(), bEOF = TRUE);
            return (int) (nFID - 1);
        }
        else
        {
            if( bAscending )
                iCurFeatureInPage ++;
            else
                iCurFeatureInPage --;
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
    while( TRUE )
    {
        int nRow = GetNextRow();
        if( nRow < 0 )
            break;
        if( nSortedCount == nSortedAlloc )
        {
            int nNewSortedAlloc = 4 * nSortedAlloc / 3 + 16;
            int* panNewSortedRows = (int*)VSIRealloc(panSortedRows,
                                            sizeof(int) * nNewSortedAlloc);
            if( panNewSortedRows == NULL )
            {
                nSortedCount = 0;
                return FALSE;
            }
            nSortedAlloc = nNewSortedAlloc;
            panSortedRows = panNewSortedRows;
        }
        panSortedRows[nSortedCount ++] = nRow;
    }
    if( nSortedCount == 0 )
        return FALSE;
    std::sort(panSortedRows, panSortedRows + nSortedCount);
    if( eOp == FGSO_ISNOTNULL && (int)nValueCountInIdx != nSortedCount )
        PrintError();
    return TRUE;
}

/************************************************************************/
/*                        GetNextRowSortedByFID()                       */
/************************************************************************/

int FileGDBIndexIterator::GetNextRowSortedByFID()
{
    if( eOp == FGSO_EQ )
        return GetNextRow();

    if( iSorted < nSortedCount )
        return panSortedRows[iSorted ++];

    if( nSortedCount < 0 )
    {
        if( !SortRows() )
            return -1;
        return panSortedRows[iSorted ++];
    }
    else
    {
        return -1;
    }
}

/************************************************************************/
/*                           GetRowCount()                              */
/************************************************************************/

int FileGDBIndexIterator::GetRowCount()
{
    if( eOp == FGSO_ISNOTNULL )
        return (int)nValueCountInIdx;

    if( nSortedCount >= 0 )
        return nSortedCount;

    int nRowCount = 0;
    int bSaveAscending = bAscending;
    bAscending = TRUE; /* for a tiny bit of more efficiency */
    Reset();
    while( GetNextRow() >= 0 )
        nRowCount ++;
    bAscending = bSaveAscending;
    Reset();
    return nRowCount;
}

/************************************************************************/
/*                            GetMinMaxValue()                          */
/************************************************************************/

const OGRField* FileGDBIndexIterator::GetMinMaxValue(OGRField* psField,
                                                     int& eOutType,
                                                     int bIsMin)
{
    const OGRField* errorRetValue = NULL;
    eOutType = -1;
    if( nValueCountInIdx == 0 )
        return NULL;

    GByte abyPage[FGDB_PAGE_SIZE];
    GUInt32 nPage = 1;
    for( GUInt32 iLevel = 0; iLevel < nIndexDepth - 1; iLevel ++ )
    {
        VSIFSeekL(fpCurIdx, (nPage - 1) * FGDB_PAGE_SIZE, SEEK_SET);
        returnErrorIf(VSIFReadL( abyPage, FGDB_PAGE_SIZE, 1, fpCurIdx ) != 1 );
        GUInt32 nSubPagesCount = GetUInt32(abyPage + 4, 0);
        returnErrorIf(nSubPagesCount == 0 || nSubPagesCount > nMaxPerPages);

        if( bIsMin )
            nPage = GetUInt32(abyPage + 8, 0);
        else
            nPage = GetUInt32(abyPage + 8, nSubPagesCount);
        returnErrorIf(nPage < 2 );
    }

    VSIFSeekL(fpCurIdx, (nPage - 1) * FGDB_PAGE_SIZE, SEEK_SET);
    returnErrorIf(VSIFReadL( abyPage, FGDB_PAGE_SIZE, 1, fpCurIdx ) != 1);

    GUInt32 nFeatures = GetUInt32(abyPage + 4, 0);
    returnErrorIf(nFeatures < 1 || nFeatures > nMaxPerPages);

    int iFeature = (bIsMin) ? 0 : nFeatures-1;

    switch( eFieldType )
    {
        case FGFT_INT16:
        {
            GInt16 nVal = GetInt16(abyPage + nOffsetFirstValInPage, iFeature);
            psField->Integer = nVal;
            eOutType = OFTInteger;
            return psField;
        }

        case FGFT_INT32:
        {
            GInt32 nVal = GetInt32(abyPage + nOffsetFirstValInPage, iFeature);
            psField->Integer = nVal;
            eOutType = OFTInteger;
            return psField;
        }

        case FGFT_FLOAT32:
        {
            float fVal = GetFloat32(abyPage + nOffsetFirstValInPage, iFeature);
            psField->Real = fVal;
            eOutType = OFTReal;
            return psField;
        }

        case FGFT_FLOAT64:
        {
            double dfVal = GetFloat64(abyPage + nOffsetFirstValInPage, iFeature);
            psField->Real = dfVal;
            eOutType = OFTReal;
            return psField;
        }

        case FGFT_DATETIME:
        {
            double dfVal = GetFloat64(abyPage + nOffsetFirstValInPage, iFeature);
            FileGDBDoubleDateToOGRDate(dfVal, psField);
            eOutType = OFTDateTime;
            return psField;
        }

        case FGFT_STRING:
        {
            wchar_t awsVal[MAX_CAR_COUNT_STR+1];
            for(int j=0;j<nStrLen;j++)
            {
                GUInt16 nCh = GetUInt16(abyPage + nOffsetFirstValInPage +
                    nStrLen * sizeof(GUInt16) * iFeature, j);
                awsVal[j] = nCh;
            }
            awsVal[nStrLen] = 0;
            char* pszOut = CPLRecodeFromWChar(awsVal, CPL_ENC_UCS2, CPL_ENC_UTF8);
            returnErrorIf(pszOut == NULL );
            returnErrorAndCleanupIf(strlen(pszOut) >
                                        MAX_UTF8_LEN_STR, VSIFree(pszOut) );
            strcpy(psField->String, pszOut);
            CPLFree(pszOut);
            eOutType = OFTString;
            return psField;
        }

        case FGFT_UUID_1:
        case FGFT_UUID_2:
        {
            memcpy(psField->String, abyPage + nOffsetFirstValInPage +
                   UUID_LEN_AS_STRING *iFeature, UUID_LEN_AS_STRING);
            psField->String[UUID_LEN_AS_STRING] = 0;
            eOutType = OFTString;
            return psField;
        }

        default:
            CPLAssert(FALSE);
            break;
    }
    return NULL;
}

/************************************************************************/
/*                            GetMinValue()                             */
/************************************************************************/

const OGRField* FileGDBIndexIterator::GetMinValue(int& eOutType)
{
    if( eOp != FGSO_ISNOTNULL )
        return FileGDBIterator::GetMinValue(eOutType);
    if( eFieldType == FGFT_STRING || eFieldType == FGFT_UUID_1 ||
        eFieldType == FGFT_UUID_2 )
        sMin.String = szMin;
    return GetMinMaxValue(&sMin, eOutType, TRUE);
}

/************************************************************************/
/*                            GetMaxValue()                             */
/************************************************************************/

const OGRField* FileGDBIndexIterator::GetMaxValue(int& eOutType)
{
    if( eOp != FGSO_ISNOTNULL )
        return FileGDBIterator::GetMinValue(eOutType);
    if( eFieldType == FGFT_STRING || eFieldType == FGFT_UUID_1 ||
        eFieldType == FGFT_UUID_2 )
        sMax.String = szMax;
    return GetMinMaxValue(&sMax, eOutType, FALSE);
}

/************************************************************************/
/*                        GetMinMaxSumCount()                           */
/************************************************************************/

struct Int16Getter
{
    public:
        static double GetAsDouble(const GByte* pBaseAddr, int iOffset)
        {
            return GetInt16(pBaseAddr, iOffset);
        }
};

struct Int32Getter
{
    public:
        static double GetAsDouble(const GByte* pBaseAddr, int iOffset)
        {
            return GetInt32(pBaseAddr, iOffset);
        }
};

struct Float32Getter
{
    public:
        static double GetAsDouble(const GByte* pBaseAddr, int iOffset)
        {
            return GetFloat32(pBaseAddr, iOffset);
        }
};

struct Float64Getter
{
    public:
        static double GetAsDouble(const GByte* pBaseAddr, int iOffset)
        {
            return GetFloat64(pBaseAddr, iOffset);
        }
};

template <class Getter> void FileGDBIndexIterator::GetMinMaxSumCount(
        double& dfMin, double& dfMax, double& dfSum, int& nCount)
{
    int nLocalCount = 0;
    double dfLocalSum = 0.0;
    double dfVal = 0.0;

    while( TRUE )
    {
        if( iCurFeatureInPage >= nFeaturesInPage )
        {
            if( !LoadNextFeaturePage() )
            {
                break;
            }
        }

        dfVal = Getter::GetAsDouble(abyPageFeature + nOffsetFirstValInPage,
                                   iCurFeatureInPage);

        dfLocalSum += dfVal;
        if( nLocalCount == 0 )
            dfMin = dfVal;
        nLocalCount ++;
        iCurFeatureInPage ++;
    }

    dfSum = dfLocalSum;
    nCount = nLocalCount;
    dfMax = dfVal;
}

int FileGDBIndexIterator::GetMinMaxSumCount(double& dfMin, double& dfMax,
                                            double& dfSum, int& nCount)
{
    const int errorRetValue = FALSE;
    dfMin = 0.0;
    dfMax = 0.0;
    dfSum = 0.0;
    nCount = 0;
    returnErrorIf(eOp != FGSO_ISNOTNULL );
    returnErrorIf(eFieldType != FGFT_INT16 && eFieldType != FGFT_INT32 &&
                  eFieldType != FGFT_FLOAT32 && eFieldType != FGFT_FLOAT64 &&
                  eFieldType != FGFT_DATETIME );

    int bSaveAscending = bAscending;
    bAscending = TRUE;
    Reset();

    switch( eFieldType )
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
        case FGFT_FLOAT32:
        {
            GetMinMaxSumCount<Float32Getter>(dfMin, dfMax, dfSum, nCount);
            break;
        }
        case FGFT_FLOAT64:
        case FGFT_DATETIME:
        {
            GetMinMaxSumCount<Float64Getter>(dfMin, dfMax, dfSum, nCount);
            break;
        }
        default:
            CPLAssert(FALSE);
            break;
    }

    bAscending = bSaveAscending;
    Reset();

    return TRUE;
}

}; /* namespace OpenFileGDB */
