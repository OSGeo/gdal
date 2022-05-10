/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements reading of FileGDB tables
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef FILEGDBTABLE_H_INCLUDED
#define FILEGDBTABLE_H_INCLUDED

#include "ogr_core.h"
#include "cpl_vsi.h"
#include "ogr_geometry.h"

#include <string>
#include <vector>

namespace OpenFileGDB
{

/************************************************************************/
/*                        FileGDBTableGeometryType                      */
/************************************************************************/

/* FGTGT = (F)ile(G)DB(T)able(G)eometry(T)ype */
typedef enum
{
    FGTGT_NONE = 0,
    FGTGT_POINT = 1,
    FGTGT_MULTIPOINT = 2,
    FGTGT_LINE = 3,
    FGTGT_POLYGON = 4,
    FGTGT_MULTIPATCH = 9
} FileGDBTableGeometryType;

/************************************************************************/
/*                          FileGDBFieldType                            */
/************************************************************************/

/* FGFT = (F)ile(G)DB(F)ield(T)ype */
typedef enum
{
    FGFT_UNDEFINED = -1,
    FGFT_INT16 = 0,
    FGFT_INT32 = 1,
    FGFT_FLOAT32 = 2,
    FGFT_FLOAT64 = 3,
    FGFT_STRING = 4,
    FGFT_DATETIME = 5,
    FGFT_OBJECTID = 6,
    FGFT_GEOMETRY = 7,
    FGFT_BINARY = 8,
    FGFT_RASTER = 9,
    FGFT_UUID_1 = 10,
    FGFT_UUID_2 = 11,
    FGFT_XML = 12
} FileGDBFieldType;

/************************************************************************/
/*                          FileGDBField                                */
/************************************************************************/

class FileGDBTable;
class FileGDBIndex;

class FileGDBField
{
        friend class FileGDBTable;

        FileGDBTable    *poParent;

        std::string      osName;
        std::string      osAlias;
        FileGDBFieldType eType;

        int               bNullable;
        int               nMaxWidth; /* for string */

        OGRField          sDefault;

        FileGDBIndex*     poIndex;

    public:

        explicit            FileGDBField(FileGDBTable* poParent);
        virtual            ~FileGDBField();

        const std::string&  GetName() const { return osName; }
        const std::string&  GetAlias() const { return osAlias; }
        FileGDBFieldType    GetType() const { return eType; }
        int                 IsNullable() const { return bNullable; }
        int                 GetMaxWidth() const { return nMaxWidth; }
        const OGRField     *GetDefault() const { return &sDefault; }

        int                 HasIndex();
        FileGDBIndex       *GetIndex();
};

/************************************************************************/
/*                         FileGDBGeomField                             */
/************************************************************************/

class FileGDBGeomField: public FileGDBField
{
        friend class FileGDBTable;

        std::string       osWKT{};
        int               bHasZOriginScaleTolerance = 0;
        int               bHasMOriginScaleTolerance = 0;
        double            dfXOrigin = 0;
        double            dfYOrigin = 0;
        double            dfXYScale = 0;
        double            dfMOrigin = 0;
        double            dfMScale = 0;
        double            dfZOrigin = 0;
        double            dfZScale = 0;
        double            dfXYTolerance = 0;
        double            dfMTolerance = 0;
        double            dfZTolerance = 0;
        double            dfXMin = 0;
        double            dfYMin = 0;
        double            dfZMin = 0;
        double            dfMMin = 0;
        double            dfXMax = 0;
        double            dfYMax = 0;
        double            dfZMax = 0;
        double            dfMMax = 0;

    public:
        explicit          FileGDBGeomField(FileGDBTable* poParent);
        virtual          ~FileGDBGeomField() {}

        const std::string& GetWKT() const { return osWKT; }

        double             GetXMin() const { return dfXMin; }
        double             GetYMin() const { return dfYMin; }
        double             GetZMin() const { return dfZMin; } // only valid for m_bGeomTypeHasZ
        double             GetMMin() const { return dfMMin; } // only valid for m_bGeomTypeHasM
        double             GetXMax() const { return dfXMax; }
        double             GetYMax() const { return dfYMax; }
        double             GetZMax() const { return dfZMax; } // only valid for m_bGeomTypeHasZ
        double             GetMMax() const { return dfMMax; } // only valid for m_bGeomTypeHasM

        int                HasZOriginScaleTolerance() const { return bHasZOriginScaleTolerance; }
        int                HasMOriginScaleTolerance() const { return bHasMOriginScaleTolerance; }

        double             GetXOrigin() const { return dfXOrigin; }
        double             GetYOrigin() const { return dfYOrigin; }
        double             GetXYScale() const { return dfXYScale; }
        double             GetXYTolerance() const { return dfXYTolerance; }

        double             GetZOrigin() const { return dfZOrigin; }
        double             GetZScale() const { return dfZScale; }
        double             GetZTolerance() const { return dfZTolerance; }

        double             GetMOrigin() const { return dfMOrigin; }
        double             GetMScale() const { return dfMScale; }
        double             GetMTolerance() const { return dfMTolerance; }
};

/************************************************************************/
/*                         FileGDBRasterField                           */
/************************************************************************/

class FileGDBRasterField: public FileGDBGeomField
{
    public:
        enum class Type
        {
            EXTERNAL,
            MANAGED,
            INLINE,
        };

    private:
        friend class FileGDBTable;

        std::string       osRasterColumnName;

        Type              m_eType = Type::EXTERNAL;

    public:
        explicit          FileGDBRasterField(FileGDBTable* poParentIn) : FileGDBGeomField(poParentIn) {}
        virtual          ~FileGDBRasterField() {}

        const std::string& GetRasterColumnName() const { return osRasterColumnName; }
        Type GetType() const { return m_eType; }
};

/************************************************************************/
/*                           FileGDBIndex                               */
/************************************************************************/

class FileGDBIndex
{
        friend class FileGDBTable;
        std::string                 osIndexName;
        std::string                 osFieldName;

    public:
                            FileGDBIndex() {}
        virtual            ~FileGDBIndex() {}

        const std::string&  GetIndexName() const { return osIndexName; }
        const std::string&  GetFieldName() const { return osFieldName; }
};

/************************************************************************/
/*                           FileGDBTable                               */
/************************************************************************/

class FileGDBTable
{
        VSILFILE                   *fpTable;
        VSILFILE                   *fpTableX;
        vsi_l_offset                nFileSize; /* only read when needed */

        std::string                 osFilename;
        std::vector<FileGDBField*>  apoFields;
        std::string                 osObjectIdColName;

        int                         bHasReadGDBIndexes;
        std::vector<FileGDBIndex*>  apoIndexes;

        int                         m_nHasSpatialIndex = -1;

        GUIntBig                    nOffsetFieldDesc;
        GUInt32                     nFieldDescLength;

        GUInt32                     nTablxOffsetSize;
        std::vector<vsi_l_offset>   anFeatureOffsets; /* MSb set marks deleted feature */

        GByte*                      pabyTablXBlockMap;
        int                         nCountBlocksBeforeIBlockIdx; /* optimization */
        int                         nCountBlocksBeforeIBlockValue; /* optimization */

        char                        achGUIDBuffer[32 + 6 + 1];
        int                         nChSaved;

        int                         bError;
        int                         nCurRow;
        int                         bHasDeletedFeaturesListed;
        int                         bIsDeleted;
        int                         nLastCol;
        GByte*                      pabyIterVals;
        int                         iAccNullable;
        GUInt32                     nRowBlobLength;
        OGRField                    sCurField;
        /* OGRFieldType                eCurFieldType; */

        FileGDBTableGeometryType    eTableGeomType;
        bool                        m_bGeomTypeHasZ = false;
        bool                        m_bGeomTypeHasM = false;
        bool                        m_bStringsAreUTF8 = true; // if false, UTF16
        std::string                 m_osTempString{}; // used as a temporary to store strings recoded from UTF16 to UTF8
        int                         nValidRecordCount;
        int                         nTotalRecordCount;
        int                         iGeomField;
        int                         nCountNullableFields;
        int                         nNullableFieldsSizeInBytes;

        std::vector<double>         m_adfSpatialIndexGridResolution{};

        GUInt32                     nBufferMaxSize;
        GByte*                      pabyBuffer;

        std::string                 m_osCacheRasterFieldPath{};

        void                        Init();

        GUIntBig                    nFilterXMin, nFilterXMax, nFilterYMin, nFilterYMax;

        GUIntBig                    nOffsetHeaderEnd;

        int                         ReadTableXHeader();
        int                         IsLikelyFeatureAtOffset(
                                                vsi_l_offset nOffset, GUInt32* pnSize,
                                                int* pbDeletedRecord);
        int                         GuessFeatureLocations();

    public:

                                FileGDBTable();
                               ~FileGDBTable();

       int                      Open(const char* pszFilename,
                                     const char* pszLayerName = nullptr);
       void                     Close();

       const std::string&       GetFilename() const { return osFilename; }
       FileGDBTableGeometryType GetGeometryType() const { return eTableGeomType; }
       bool                     GetGeomTypeHasZ() const { return m_bGeomTypeHasZ; }
       bool                     GetGeomTypeHasM() const { return m_bGeomTypeHasM; }
       int                      GetValidRecordCount() const { return nValidRecordCount; }
       int                      GetTotalRecordCount() const { return nTotalRecordCount; }
       int                      GetFieldCount() const { return (int)apoFields.size(); }
       FileGDBField*            GetField(int i) const { return apoFields[i]; }
       int                      GetGeomFieldIdx() const { return iGeomField; }
       const FileGDBGeomField*  GetGeomField() const {
           return (iGeomField >= 0) ?
               reinterpret_cast<FileGDBGeomField*>(apoFields[iGeomField]) : nullptr; }
       const std::string&       GetObjectIdColName() const { return osObjectIdColName; }

       int                      GetFieldIdx(const std::string& osName) const;

       int                      GetIndexCount();
       const FileGDBIndex*      GetIndex(int i) const { return apoIndexes[i]; }
       bool                     HasSpatialIndex();

       vsi_l_offset             GetOffsetInTableForRow(int iRow);

       int                      HasDeletedFeaturesListed() const { return bHasDeletedFeaturesListed; }

       /* Next call to SelectRow() or GetFieldValue() invalidates previously returned values */
       int                      SelectRow(int iRow);
       int                      GetAndSelectNextNonEmptyRow(int iRow);
       int                      HasGotError() const { return bError; }
       int                      GetCurRow() const { return nCurRow; }
       int                      IsCurRowDeleted() const { return bIsDeleted; }
       OGRField*                GetFieldValue(int iCol);

       int                      GetFeatureExtent(const OGRField* psGeomField,
                                                 OGREnvelope* psOutFeatureEnvelope);

       const std::vector<double>& GetSpatialIndexGridResolution() const { return m_adfSpatialIndexGridResolution; }
       void                     InstallFilterEnvelope(const OGREnvelope* psFilterEnvelope);
       int                      DoesGeometryIntersectsFilterEnvelope(const OGRField* psGeomField);
};

/************************************************************************/
/*                           FileGDBSQLOp                               */
/************************************************************************/

typedef enum
{
    FGSO_ISNOTNULL,
    FGSO_LT,
    FGSO_LE,
    FGSO_EQ,
    FGSO_GE,
    FGSO_GT
} FileGDBSQLOp;

/************************************************************************/
/*                           FileGDBIterator                            */
/************************************************************************/

class FileGDBIterator
{
    public:
        virtual                     ~FileGDBIterator() {}

        virtual FileGDBTable        *GetTable() = 0;
        virtual void                 Reset() = 0;
        virtual int                  GetNextRowSortedByFID() = 0;
        virtual int                  GetRowCount();

        /* Only available on a BuildIsNotNull() iterator */
        virtual const OGRField*      GetMinValue(int& eOutOGRFieldType);
        virtual const OGRField*      GetMaxValue(int& eOutOGRFieldType);
        /* will reset the iterator */
        virtual int                  GetMinMaxSumCount(double& dfMin, double& dfMax,
                                                       double& dfSum, int& nCount);

        /* Only available on a BuildIsNotNull() or Build() iterator */
        virtual int                  GetNextRowSortedByValue();

        static FileGDBIterator*      Build(FileGDBTable* poParent,
                                           int nFieldIdx,
                                           int bAscending,
                                           FileGDBSQLOp op,
                                           OGRFieldType eOGRFieldType,
                                           const OGRField* psValue);
        static FileGDBIterator*      BuildIsNotNull(FileGDBTable* poParent,
                                                    int nFieldIdx,
                                                    int bAscending);
        static FileGDBIterator*      BuildNot(FileGDBIterator* poIterBase);
        static FileGDBIterator*      BuildAnd(FileGDBIterator* poIter1,
                                              FileGDBIterator* poIter2,
                                              bool bTakeOwnershipOfIterators);
        static FileGDBIterator*      BuildOr(FileGDBIterator* poIter1,
                                             FileGDBIterator* poIter2,
                                             int bIteratorAreExclusive = FALSE);
};

/************************************************************************/
/*                      FileGDBSpatialIndexIterator                     */
/************************************************************************/

class FileGDBSpatialIndexIterator: virtual public FileGDBIterator
{
    public:
        virtual bool                 SetEnvelope(const OGREnvelope& sFilterEnvelope) = 0;

        static FileGDBSpatialIndexIterator* Build(FileGDBTable* poParent,
                                                  const OGREnvelope& sFilterEnvelope);
};

/************************************************************************/
/*                       FileGDBOGRGeometryConverter                    */
/************************************************************************/

class FileGDBOGRGeometryConverter
{
    public:
       virtual                            ~FileGDBOGRGeometryConverter() {}

       virtual OGRGeometry*                GetAsGeometry(const OGRField* psField) = 0;

       static FileGDBOGRGeometryConverter* BuildConverter(const FileGDBGeomField* poGeomField);
       static OGRwkbGeometryType           GetGeometryTypeFromESRI(const char* pszESRIGeometryType);
};

int FileGDBDoubleDateToOGRDate(double dfVal, OGRField* psField);

} /* namespace OpenFileGDB */

#endif /* ndef FILEGDBTABLE_H_INCLUDED */
