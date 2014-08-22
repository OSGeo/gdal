/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements reading of FileGDB tables
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

#ifndef _FILEGDBTABLE_H_INCLUDED
#define _FILEGDBTABLE_H_INCLUDED

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

        FileGDBIndex*     poIndex;

    public:

                            FileGDBField(FileGDBTable* poParent);
        virtual            ~FileGDBField();

        const std::string&  GetName() const { return osName; }
        const std::string&  GetAlias() const { return osAlias; }
        FileGDBFieldType    GetType() const { return eType; }
        int                 IsNullable() const { return bNullable; }
        int                 GetMaxWidth() const { return nMaxWidth; }

        int                 HasIndex();
        FileGDBIndex       *GetIndex();
};

/************************************************************************/
/*                         FileGDBGeomField                             */
/************************************************************************/

class FileGDBGeomField: public FileGDBField
{
        friend class FileGDBTable;

        std::string       osWKT;
        int               bHasZ;
        int               bHasM;
        double            dfXOrigin;
        double            dfYOrigin;
        double            dfXYScale;
        double            dfMOrigin;
        double            dfMScale;
        double            dfZOrigin;
        double            dfZScale;
        double            dfXYTolerance;
        double            dfMTolerance;
        double            dfZTolerance;
        double            dfXMin;
        double            dfYMin;
        double            dfXMax;
        double            dfYMax;

    public:
                          FileGDBGeomField(FileGDBTable* poParent);
        virtual          ~FileGDBGeomField() {}

        const std::string& GetWKT() const { return osWKT; }

        double             GetXMin() const { return dfXMin; }
        double             GetYMin() const { return dfYMin; }
        double             GetXMax() const { return dfXMax; }
        double             GetYMax() const { return dfYMax; }

        int                HasZ() const { return bHasZ; }
        int                HasM() const { return bHasM; }

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
        friend class FileGDBTable;

        std::string       osRasterColumnName;

    public:
                          FileGDBRasterField(FileGDBTable* poParent) : FileGDBGeomField(poParent) {}
        virtual          ~FileGDBRasterField() {}

        const std::string& GetRasterColumnName() const { return osRasterColumnName; }

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
        std::string                 osFilename;
        std::vector<FileGDBField*>  apoFields;
        std::string                 osObjectIdColName;

        int                         bHasReadGDBIndexes;
        std::vector<FileGDBIndex*>  apoIndexes;

        GUInt32                     nOffsetFieldDesc;
        GUInt32                     nFieldDescLength;

        std::vector<vsi_l_offset>   anFeatureOffsets;

        GByte*                      pabyTablXBlockMap;

        char                        achGUIDBuffer[32 + 6 + 1];
        int                         nChSaved;

        int                         bError;
        int                         nCurRow;
        int                         nLastCol;
        GByte*                      pabyIterVals;
        int                         iAccNullable;
        GUInt32                     nRowBlobLength;
        OGRField                    sCurField;
        /* OGRFieldType                eCurFieldType; */

        FileGDBTableGeometryType    eTableGeomType;
        int                         nValidRecordCount;
        int                         nTotalRecordCount;
        int                         iGeomField;
        int                         nCountNullableFields;
        int                         nNullableFieldsSizeInBytes;

        GUInt32                     nBufferMaxSize;
        GByte*                      pabyBuffer;

        void                        Init();

        GUIntBig                    nFilterXMin, nFilterXMax, nFilterYMin, nFilterYMax;
        
        GUInt32                     nOffsetHeaderEnd;

        int                         ReadTableXHeader();
        int                         IsLikelyFeatureAtOffset(vsi_l_offset nFileSize,
                                                vsi_l_offset nOffset, GUInt32* pnSize,
                                                int* pbDeletedRecord);
        int                         GuessFeatureLocations();

    public:

                                FileGDBTable();
                               ~FileGDBTable();

       int                      Open(const char* pszFilename);
       void                     Close();

       const std::string&       GetFilename() const { return osFilename; }
       FileGDBTableGeometryType GetGeometryType() const { return eTableGeomType; }
       int                      GetValidRecordCount() const { return nValidRecordCount; }
       int                      GetTotalRecordCount() const { return nTotalRecordCount; }
       int                      GetFieldCount() const { return (int)apoFields.size(); }
       FileGDBField*            GetField(int i) const { return apoFields[i]; }
       int                      GetGeomFieldIdx() const { return iGeomField; }
       const FileGDBGeomField*  GetGeomField() const { return (iGeomField >= 0) ? (FileGDBGeomField*)apoFields[iGeomField] : NULL; }
       const std::string&       GetObjectIdColName() const { return osObjectIdColName; }

       int                      GetFieldIdx(const std::string& osName) const;

       int                      GetIndexCount();
       const FileGDBIndex*      GetIndex(int i) const { return apoIndexes[i]; }

       vsi_l_offset             GetOffsetInTableForRow(int iRow);

       /* Next call to SelectRow() or GetFieldValue() invalidates previously returned values */
       int                      SelectRow(int iRow);
       int                      HasGotError() const { return bError; }
       int                      GetCurRow() const { return nCurRow; }
       const OGRField*          GetFieldValue(int iCol);

       int                      GetFeatureExtent(const OGRField* psGeomField,
                                                 OGREnvelope* psOutFeatureEnvelope);

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
                                              FileGDBIterator* poIter2);
        static FileGDBIterator*      BuildOr(FileGDBIterator* poIter1,
                                             FileGDBIterator* poIter2,
                                             int bIteratorAreExclusive = FALSE);
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
       static OGRwkbGeometryType           GetGeometryTypeFromESRI(const char* pszESRIGeometyrType);
};

int FileGDBDoubleDateToOGRDate(double dfVal, OGRField* psField);

}; /* namespace OpenFileGDB */

#endif /* ndef _FILEGDBTABLE_H_INCLUDED */
