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

        FileGDBTable    *m_poParent;

        std::string      m_osName;
        std::string      m_osAlias;
        FileGDBFieldType m_eType;

        int               m_bNullable;
        int               m_nMaxWidth; /* for string */

        OGRField          m_sDefault;

        FileGDBIndex*     m_poIndex;

    public:

        explicit            FileGDBField(FileGDBTable* m_poParent);
        virtual            ~FileGDBField();

        const std::string&  GetName() const { return m_osName; }
        const std::string&  GetAlias() const { return m_osAlias; }
        FileGDBFieldType    GetType() const { return m_eType; }
        int                 IsNullable() const { return m_bNullable; }
        int                 GetMaxWidth() const { return m_nMaxWidth; }
        const OGRField     *GetDefault() const { return &m_sDefault; }

        int                 HasIndex();
        FileGDBIndex       *GetIndex();
};

/************************************************************************/
/*                         FileGDBGeomField                             */
/************************************************************************/

class FileGDBGeomField: public FileGDBField
{
        friend class FileGDBTable;

        std::string       m_osWKT{};
        int               m_bHasZOriginScaleTolerance = 0;
        int               m_bHasMOriginScaleTolerance = 0;
        double            m_dfXOrigin = 0;
        double            m_dfYOrigin = 0;
        double            m_dfXYScale = 0;
        double            m_dfMOrigin = 0;
        double            m_dfMScale = 0;
        double            m_dfZOrigin = 0;
        double            m_dfZScale = 0;
        double            m_dfXYTolerance = 0;
        double            m_dfMTolerance = 0;
        double            m_dfZTolerance = 0;
        double            m_dfXMin = 0;
        double            m_dfYMin = 0;
        double            m_dfZMin = 0;
        double            m_dfMMin = 0;
        double            m_dfXMax = 0;
        double            m_dfYMax = 0;
        double            m_dfZMax = 0;
        double            m_dfMMax = 0;

    public:
        explicit          FileGDBGeomField(FileGDBTable* m_poParent);
        virtual          ~FileGDBGeomField() {}

        const std::string& GetWKT() const { return m_osWKT; }

        double             GetXMin() const { return m_dfXMin; }
        double             GetYMin() const { return m_dfYMin; }
        double             GetZMin() const { return m_dfZMin; } // only valid for m_bGeomTypeHasZ
        double             GetMMin() const { return m_dfMMin; } // only valid for m_bGeomTypeHasM
        double             GetXMax() const { return m_dfXMax; }
        double             GetYMax() const { return m_dfYMax; }
        double             GetZMax() const { return m_dfZMax; } // only valid for m_bGeomTypeHasZ
        double             GetMMax() const { return m_dfMMax; } // only valid for m_bGeomTypeHasM

        int                HasZOriginScaleTolerance() const { return m_bHasZOriginScaleTolerance; }
        int                HasMOriginScaleTolerance() const { return m_bHasMOriginScaleTolerance; }

        double             GetXOrigin() const { return m_dfXOrigin; }
        double             GetYOrigin() const { return m_dfYOrigin; }
        double             GetXYScale() const { return m_dfXYScale; }
        double             GetXYTolerance() const { return m_dfXYTolerance; }

        double             GetZOrigin() const { return m_dfZOrigin; }
        double             GetZScale() const { return m_dfZScale; }
        double             GetZTolerance() const { return m_dfZTolerance; }

        double             GetMOrigin() const { return m_dfMOrigin; }
        double             GetMScale() const { return m_dfMScale; }
        double             GetMTolerance() const { return m_dfMTolerance; }
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

        std::string       m_osRasterColumnName;

        Type              m_eRasterType = Type::EXTERNAL;

    public:
        explicit          FileGDBRasterField(FileGDBTable* poParentIn) : FileGDBGeomField(poParentIn) {}
        virtual          ~FileGDBRasterField() {}

        const std::string& GetRasterColumnName() const { return m_osRasterColumnName; }
        Type GetRasterType() const { return m_eRasterType; }
};

/************************************************************************/
/*                           FileGDBIndex                               */
/************************************************************************/

class FileGDBIndex
{
        friend class FileGDBTable;
        std::string                 m_osIndexName;
        std::string                 m_osFieldName;

    public:
                            FileGDBIndex() {}
        virtual            ~FileGDBIndex() {}

        const std::string&  GetIndexName() const { return m_osIndexName; }
        const std::string&  GetFieldName() const { return m_osFieldName; }
        int                 GetMaxWidthInBytes(const FileGDBTable* poTable) const;
};

/************************************************************************/
/*                           FileGDBTable                               */
/************************************************************************/

class FileGDBTable
{
        VSILFILE                   *m_fpTable = nullptr;
        VSILFILE                   *m_fpTableX = nullptr;
        vsi_l_offset                m_nFileSize = 0; /* only read when needed */

        std::string                 m_osFilename{};
        std::vector<FileGDBField*>  m_apoFields{};
        std::string                 m_osObjectIdColName{};

        int                         m_bHasReadGDBIndexes = FALSE;
        std::vector<FileGDBIndex*>  m_apoIndexes{};

        int                         m_nHasSpatialIndex = -1;

        GUIntBig                    m_nOffsetFieldDesc = 0;
        GUInt32                     m_nFieldDescLength = 0;

        GUInt32                     m_nTablxOffsetSize = 0;
        std::vector<vsi_l_offset>   m_anFeatureOffsets{}; /* MSb set marks deleted feature */

        GByte*                      m_pabyTablXBlockMap = nullptr;
        int                         m_nCountBlocksBeforeIBlockIdx = 0; /* optimization */
        int                         m_nCountBlocksBeforeIBlockValue = 0; /* optimization */

        char                        m_achGUIDBuffer[32 + 6 + 1]{0};
        int                         m_nChSaved = -1;

        int                         m_bError = FALSE;
        int                         m_nCurRow = -1;
        int                         m_bHasDeletedFeaturesListed = FALSE;
        int                         m_bIsDeleted = FALSE;
        int                         m_nLastCol = -1;
        GByte*                      m_pabyIterVals = nullptr;
        int                         m_iAccNullable = 0;
        GUInt32                     m_nRowBlobLength = 0;
        OGRField                    m_sCurField;

        FileGDBTableGeometryType    m_eTableGeomType = FGTGT_NONE;
        bool                        m_bGeomTypeHasZ = false;
        bool                        m_bGeomTypeHasM = false;
        bool                        m_bStringsAreUTF8 = true; // if false, UTF16
        std::string                 m_osTempString{}; // used as a temporary to store strings recoded from UTF16 to UTF8
        int                         m_nValidRecordCount = 0;
        int                         m_nTotalRecordCount = 0;
        int                         m_iGeomField = -1;
        int                         m_nCountNullableFields = 0;
        int                         m_nNullableFieldsSizeInBytes = 0;

        std::vector<double>         m_adfSpatialIndexGridResolution{};

        GUInt32                     m_nBufferMaxSize = 0;
        GByte*                      m_pabyBuffer = nullptr;

        std::string                 m_osCacheRasterFieldPath{};

        GUIntBig                    m_nFilterXMin = 0, m_nFilterXMax = 0, m_nFilterYMin = 0, m_nFilterYMax = 0;

        GUIntBig                    m_nOffsetHeaderEnd = 0;

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

       //! Object should no longer be used after Close()
       void                     Close();

       const std::string&       GetFilename() const { return m_osFilename; }
       FileGDBTableGeometryType GetGeometryType() const { return m_eTableGeomType; }
       bool                     GetGeomTypeHasZ() const { return m_bGeomTypeHasZ; }
       bool                     GetGeomTypeHasM() const { return m_bGeomTypeHasM; }
       int                      GetValidRecordCount() const { return m_nValidRecordCount; }
       int                      GetTotalRecordCount() const { return m_nTotalRecordCount; }
       int                      GetFieldCount() const { return (int)m_apoFields.size(); }
       FileGDBField*            GetField(int i) const { return m_apoFields[i]; }
       int                      GetGeomFieldIdx() const { return m_iGeomField; }
       const FileGDBGeomField*  GetGeomField() const {
           return (m_iGeomField >= 0) ?
               reinterpret_cast<FileGDBGeomField*>(m_apoFields[m_iGeomField]) : nullptr; }
       const std::string&       GetObjectIdColName() const { return m_osObjectIdColName; }

       int                      GetFieldIdx(const std::string& osName) const;

       int                      GetIndexCount();
       const FileGDBIndex*      GetIndex(int i) const { return m_apoIndexes[i]; }
       bool                     HasSpatialIndex();

       vsi_l_offset             GetOffsetInTableForRow(int iRow);

       int                      HasDeletedFeaturesListed() const { return m_bHasDeletedFeaturesListed; }

       /* Next call to SelectRow() or GetFieldValue() invalidates previously returned values */
       int                      SelectRow(int iRow);
       int                      GetAndSelectNextNonEmptyRow(int iRow);
       int                      HasGotError() const { return m_bError; }
       int                      GetCurRow() const { return m_nCurRow; }
       int                      IsCurRowDeleted() const { return m_bIsDeleted; }
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
