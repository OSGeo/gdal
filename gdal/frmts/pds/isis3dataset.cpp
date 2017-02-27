/******************************************************************************
 *
 * Project:  ISIS Version 3 Driver
 * Purpose:  Implementation of ISIS3Dataset
 * Author:   Trent Hare (thare@usgs.gov)
 *           Frank Warmerdam (warmerdam@pobox.com)
 *           Even Rouault (even.rouault at spatialys.com)
 *
 * NOTE: Original code authored by Trent and placed in the public domain as
 * per US government policy.  I have (within my rights) appropriated it and
 * placed it under the following license.  This is not intended to diminish
 * Trents contribution.
 ******************************************************************************
 * Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2010, Even Rouault <even.rouault at spatialys.com>
 * Copyright (c) 2017 Hobu Inc
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

#include "cpl_string.h"
#include "cpl_vsi_error.h"
#include "gdal_frmts.h"
#include "gdal_proxy.h"
#include "nasakeywordhandler.h"
#include "ogrgeojsonreader.h"
#include "ogr_json_header.h"
#include "ogr_spatialref.h"
#include "rawdataset.h"
#include "vrtdataset.h"

#include <algorithm>
#include <map>
#include <utility> // pair
#include <vector>

// Constants coming from ISIS3 source code
// in isis/src/base/objs/SpecialPixel/SpecialPixel.h

//There are several types of special pixels
//   *   Isis::Null Pixel has no data available
//   *   Isis::Lis Pixel was saturated on the instrument
//   *   Isis::His Pixel was saturated on the instrument
//   *   Isis::Lrs Pixel was saturated during a computation
//   *   Isis::Hrs Pixel was saturated during a computation

// 1-byte special pixel values
const unsigned char NULL1           = 0;
const unsigned char LOW_REPR_SAT1   = 0;
const unsigned char LOW_INSTR_SAT1  = 0;
const unsigned char HIGH_INSTR_SAT1 = 255;
const unsigned char HIGH_REPR_SAT1  = 255;

// 2-byte unsigned special pixel values
const unsigned short NULLU2           = 0;
const unsigned short LOW_REPR_SATU2   = 1;
const unsigned short LOW_INSTR_SATU2  = 2;
const unsigned short HIGH_INSTR_SATU2 = 65534;
const unsigned short HIGH_REPR_SATU2  = 65535;

// 2-byte signed special pixel values
const short NULL2           = -32768;
const short LOW_REPR_SAT2   = -32767;
const short LOW_INSTR_SAT2  = -32766;
const short HIGH_INSTR_SAT2 = -32765;
const short HIGH_REPR_SAT2  = -32764;

// Define 4-byte special pixel values for IEEE floating point
const float NULL4           = -3.4028226550889045e+38f; // 0xFF7FFFFB;
const float LOW_REPR_SAT4   = -3.4028228579130005e+38f; // 0xFF7FFFFC;
const float LOW_INSTR_SAT4  = -3.4028230607370965e+38f; // 0xFF7FFFFD;
const float HIGH_INSTR_SAT4 = -3.4028232635611926e+38f; // 0xFF7FFFFE;
const float HIGH_REPR_SAT4  = -3.4028234663852886e+38f; // 0xFF7FFFFF;

// Must be large enough to hold an integer
static const char* const pszSTARTBYTE_PLACEHOLDER = "!*^STARTBYTE^*!";
// Must be large enough to hold an integer
static const char* const pszLABEL_BYTES_PLACEHOLDER = "!*^LABEL_BYTES^*!";

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*                             CPLJsonObject                            */
/* ==================================================================== */
/************************************************************************/

// This class is intended to be general purpose

class CPLJsonObject
{
    public:
        enum Type
        {
            UNINIT,
            JSON_NULL,
            INT,
            BOOLEAN,
            DOUBLE,
            STRING,
            OBJECT,
            ARRAY
        };

    private:
        static const CPLJsonObject gUninitObject;

        Type m_eType;
        // Maintain both a list and a map to keep insertion order
        std::vector< std::pair< CPLString, CPLJsonObject* > > m_oList;
        std::map< CPLString, int> m_oMap;
        GInt64 m_nVal;
        double m_dfVal;
        CPLString m_osVal;

        void add( json_object* poObj )
        {
            CPLAssert( m_eType == UNINIT || m_eType == ARRAY );
            m_eType = ARRAY;
            m_oList.push_back( std::pair<CPLString,CPLJsonObject*>(
                                CPLString(), new CPLJsonObject(poObj)) );
        }

    public:

        static const CPLJsonObject JSON_NULL_CST;

        CPLJsonObject() : m_eType(UNINIT), m_nVal(0), m_dfVal(0) {}

        ~CPLJsonObject()
        {
            clear();
        }

        CPLJsonObject(const CPLJsonObject& other) :
            m_eType(other.m_eType),
            m_oList(other.m_oList),
            m_oMap(other.m_oMap),
            m_nVal(other.m_nVal),
            m_dfVal(other.m_dfVal),
            m_osVal(other.m_osVal)
        {
            for(size_t i=0;i<m_oList.size();++i)
                m_oList[i].second = new CPLJsonObject( *m_oList[i].second );
        }

        explicit CPLJsonObject(int nVal) :
            m_eType(INT), m_nVal(nVal), m_dfVal(0.0) {}

        explicit CPLJsonObject(GInt64 nVal) :
            m_eType(INT), m_nVal(nVal), m_dfVal(0.0) {}

        explicit CPLJsonObject(bool bVal) :
            m_eType(BOOLEAN), m_nVal(bVal), m_dfVal(0.0) {}

        explicit CPLJsonObject(double dfVal) :
            m_eType(DOUBLE), m_nVal(0), m_dfVal(dfVal) {}

        explicit CPLJsonObject(const char* pszVal) :
            m_eType(STRING), m_nVal(0), m_dfVal(0.0), m_osVal(pszVal) {}

        explicit CPLJsonObject(Type eType):
            m_eType(eType), m_nVal(0), m_dfVal(0) {}

        explicit CPLJsonObject(json_object* poObj):
            m_eType(UNINIT), m_nVal(0), m_dfVal(0.0)
        {
            *this = poObj;
        }

        void clear()
        {
            if( m_eType != UNINIT )
            {
                m_eType = UNINIT;
                for(size_t i=0;i<m_oList.size();++i)
                    delete m_oList[i].second;
                m_oList.clear();
                m_oMap.clear();
                m_nVal = 0;
                m_dfVal = 0.0;
                m_osVal.clear();
            }
        }

        Type getType() const { return m_eType; }
        bool getBool() const { return m_nVal == 1; }
        GInt64 getInt() const { return m_nVal; }
        double getDouble() const { return m_dfVal; }
        const char* getString() const { return m_osVal.c_str(); }

        json_object* asLibJsonObj() const
        {
            json_object* obj = NULL;
            if( m_eType == INT )
                obj = json_object_new_int64(m_nVal);
            else if( m_eType == BOOLEAN )
                obj = json_object_new_boolean( m_nVal == 1 );
            else if( m_eType == DOUBLE )
                obj = json_object_new_double(m_dfVal);
            else if( m_eType == STRING )
                obj = json_object_new_string(m_osVal);
            else if( m_eType == OBJECT )
            {
                obj = json_object_new_object();
                for( size_t i=0; i < m_oList.size(); ++i )
                {
                    json_object_object_add(obj,
                                           m_oList[i].first.c_str(),
                                           m_oList[i].second->asLibJsonObj());
                }
            }
            else if( m_eType == ARRAY )
            {
                obj = json_object_new_array();
                for( size_t i=0; i < m_oList.size(); ++i )
                {
                    json_object_array_add(obj,
                                          m_oList[i].second->asLibJsonObj());
                }
            }
            return obj;
        }

        // Non-const accessor. Creates then entry if needed
        CPLJsonObject& operator[](const char* pszKey)
        {
            CPLAssert( m_eType == UNINIT || m_eType == OBJECT );
            m_eType = OBJECT;
            std::map<CPLString,int>::const_iterator oIter =
                                                    m_oMap.find(pszKey);
            if( oIter != m_oMap.end() )
                return *(m_oList[ oIter->second ].second);
            m_oList.push_back( std::pair<CPLString,CPLJsonObject*>(
                                                pszKey, new CPLJsonObject()) );
            m_oMap[pszKey] = static_cast<int>(m_oList.size()) - 1;
            return *(m_oList.back().second);
        }

        void insert(int nPos, const char* pszKey, const CPLJsonObject& obj )
        {
            CPLAssert( m_eType == OBJECT );
            CPLAssert( nPos >= 0 &&
                       static_cast<size_t>(nPos) <= m_oList.size() );
            del(pszKey);
            std::map<CPLString,int>::iterator oIter = m_oMap.begin();
            for( ; oIter != m_oMap.end(); ++oIter )
            {
                if( oIter->second >= nPos )
                    oIter->second ++;
            }
            m_oList.insert( m_oList.begin() + nPos,
                            std::pair<CPLString,CPLJsonObject*>(
                                            pszKey, new CPLJsonObject(obj)) );
            m_oMap[pszKey] = nPos;
        }

        bool has(const char* pszKey) const
        {
            CPLAssert( m_eType == OBJECT );
            std::map<CPLString,int>::const_iterator oIter =
                                                    m_oMap.find(pszKey);
            return oIter != m_oMap.end();
        }

        void del(const char* pszKey)
        {
            CPLAssert( m_eType == OBJECT );
            std::map<CPLString,int>::iterator oIter = m_oMap.find(pszKey);
            if( oIter != m_oMap.end() )
            {
                const int nIdx = oIter->second;
                delete m_oList[nIdx].second;
                m_oList.erase( m_oList.begin() + nIdx );
                m_oMap.erase(oIter);

                oIter = m_oMap.begin();
                for( ; oIter != m_oMap.end(); ++oIter )
                {
                    if( oIter->second > nIdx )
                        oIter->second --;
                }
            }
        }

        const CPLJsonObject& operator[](const char* pszKey) const
        {
            CPLAssert( m_eType == OBJECT );
            std::map<CPLString,int>::const_iterator oIter =
                                                    m_oMap.find(pszKey);
            if( oIter != m_oMap.end() )
                return *(m_oList[ oIter->second ].second);
            return gUninitObject;
        }

        CPLJsonObject& operator[](int i)
        {
            CPLAssert( m_eType == ARRAY );
            CPLAssert( i >= 0 && static_cast<size_t>(i) < m_oList.size() );
            return *(m_oList[i].second);
        }

        CPLJsonObject& operator[](size_t i)
        {
            CPLAssert( m_eType == ARRAY );
            CPLAssert( i < m_oList.size() );
            return *(m_oList[i].second);
        }

        const CPLJsonObject& operator[](int i) const
        {
            CPLAssert( m_eType == ARRAY );
            CPLAssert( i >= 0 && static_cast<size_t>(i) < m_oList.size() );
            return *(m_oList[i].second);
        }

        const CPLJsonObject& operator[](size_t i) const
        {
            CPLAssert( m_eType == ARRAY );
            CPLAssert( i < m_oList.size() );
            return *(m_oList[i].second);
        }

        void add( const CPLJsonObject& newChild )
        {
            CPLAssert( m_eType == UNINIT || m_eType == ARRAY );
            m_eType = ARRAY;
            m_oList.push_back( std::pair<CPLString,CPLJsonObject*>(
                                CPLString(), new CPLJsonObject(newChild)) );
        }

        size_t size() const
        {
            CPLAssert( m_eType == OBJECT || m_eType == ARRAY );
            return m_oList.size();
        }

        CPLJsonObject& operator= (int nVal)
        {
            m_eType = INT;
            m_nVal = nVal;
            return *this;
        }

        CPLJsonObject& operator= (GInt64 nVal)
        {
            m_eType = INT;
            m_nVal = nVal;
            return *this;
        }

        CPLJsonObject& operator= (bool bVal)
        {
            m_eType = BOOLEAN;
            m_nVal = bVal;
            return *this;
        }
        CPLJsonObject& operator= (double dfVal)
        {
            m_eType = DOUBLE;
            m_dfVal = dfVal;
            return *this;
        }

        CPLJsonObject& operator= (const char* pszVal)
        {
            m_eType = STRING;
            m_osVal = pszVal;
            return *this;
        }

        CPLJsonObject& operator= (const CPLJsonObject& other)
        {
            if( &other != this )
            {
                clear();
                m_eType = other.m_eType;
                m_oList = other.m_oList;
                m_oMap = other.m_oMap;
                m_nVal = other.m_nVal;
                m_dfVal = other.m_dfVal;
                m_osVal = other.m_osVal;
                for(size_t i=0;i<m_oList.size();++i)
                    m_oList[i].second = new CPLJsonObject(*m_oList[i].second);
            }
            return *this;
        }

        CPLJsonObject& operator= (json_object* poObj)
        {
            clear();
            if( poObj == NULL )
            {
                m_eType = JSON_NULL;
                return *this;
            }
            int eType = json_object_get_type(poObj);
            if( eType == json_type_boolean)
            {
                m_eType = BOOLEAN;
                m_nVal = json_object_get_boolean(poObj);
                return *this;
            }
            if( eType == json_type_int )
            {
                m_eType = INT;
                m_nVal = json_object_get_int64(poObj);
                return *this;
            }
            if( eType == json_type_double )
            {
                m_eType = DOUBLE;
                m_dfVal = json_object_get_double(poObj);
                return *this;
            }
            if( eType == json_type_string )
            {
                m_eType = STRING;
                m_osVal = json_object_get_string(poObj);
                return *this;
            }
            if( eType == json_type_object )
            {
                m_eType = OBJECT;
                json_object_iter it;
                it.key = NULL;
                it.val = NULL;
                it.entry = NULL;
                json_object_object_foreachC( poObj, it )
                {
                    (*this)[it.key] = it.val;
                }
                return *this;
            }
            if( eType == json_type_array )
            {
                m_eType = ARRAY;
                const int nLength = json_object_array_length(poObj);
                for( int i = 0; i < nLength; i++ )
                {
                    add( json_object_array_get_idx(poObj, i) );
                }
                return *this;
            }
            CPLAssert(false);
            return *this;
        }

        bool operator== (const CPLJsonObject& other) const
        {
            if( m_eType != other.m_eType )
                return false;

            if( m_eType == INT || m_eType == BOOLEAN )
                return m_nVal == other.m_nVal;
            if( m_eType == DOUBLE )
                return m_dfVal == other.m_dfVal;
            if( m_eType == STRING )
                return m_osVal == other.m_osVal;
            if( m_eType == OBJECT )
            {
                if( m_oList.size() != other.m_oList.size() )
                    return false;
                for(size_t i=0;i<m_oList.size();++i)
                {
                    if( *m_oList[i].second != other[m_oList[i].first] )
                        return false;
                }
            }
            if( m_eType == ARRAY )
            {
                if( m_oList.size() != other.m_oList.size() )
                    return false;
                for(size_t i=0;i<m_oList.size();++i)
                {
                    if( *m_oList[i].second != *other.m_oList[i].second )
                        return false;
                }
            }
            return true;
        }

        bool operator!= (const CPLJsonObject& other) const
        {
            return !((*this) == other);
        }
};

CPLJsonObject gUninitObject;
CPLJsonObject JSON_NULL_CST(CPLJsonObject::JSON_NULL);

/************************************************************************/
/* ==================================================================== */
/*                             ISISDataset                              */
/* ==================================================================== */
/************************************************************************/

class ISIS3Dataset : public RawDataset
{
    friend class ISIS3RawRasterBand;
    friend class ISISTiledBand;
    friend class ISIS3WrapperRasterBand;

    VSILFILE    *m_fpLabel;  // label file (only used for writing)
    VSILFILE    *m_fpImage;  // image data file. May be == fpLabel
    GDALDataset *m_poExternalDS; // external dataset (GeoTIFF)
    bool         m_bGeoTIFFAsRegularExternal; // creation only
    bool         m_bGeoTIFFInitDone; // creation only

    CPLString    m_osExternalFilename;
    bool         m_bIsLabelWritten; // creation only

    bool         m_bIsTiled;
    bool         m_bInitToNodata; // creation only

    NASAKeywordHandler m_oKeywords;

    bool        m_bGotTransform;
    double      m_adfGeoTransform[6];

    bool        m_bHasSrcNoData; // creation only
    double      m_dfSrcNoData; // creation only

    CPLString   m_osProjection;

    // creation only variables
    CPLString   m_osComment;
    CPLString   m_osLatitudeType;
    CPLString   m_osLongitudeDirection;
    CPLString   m_osLongitudeDomain;
    bool        m_bWriteBoundingDegrees;
    CPLString   m_osBoundingDegrees;

    json_object  *m_poJSonLabel;
    bool          m_bUseSrcLabel; // creation only
    bool          m_bUseSrcMapping; // creation only
    json_object  *m_poSrcJSonLabel; // creation only
    CPLStringList m_aosISIS3MD;
    CPLStringList m_aosAdditionalFiles;

    const char *GetKeyword( const char *pszPath,
                            const char *pszDefault = "");

    json_object *BuildLabel();
    void         WriteLabel();

    static void SerializeAsPDL( VSILFILE* fp, json_object* poObj,
                                int nDepth = 0 );

public:
    ISIS3Dataset();
    virtual ~ISIS3Dataset();

    virtual int CloseDependentDatasets() override;

    virtual CPLErr GetGeoTransform( double * padfTransform ) override;
    virtual CPLErr SetGeoTransform( double * padfTransform ) override;

    virtual const char *GetProjectionRef(void) override;
    virtual CPLErr SetProjection( const char* pszProjection ) override;

    virtual char **GetFileList() override;

    virtual char **GetMetadataDomainList() override;
    virtual char **GetMetadata( const char* pszDomain = "" ) override;
    virtual CPLErr SetMetadata( char** papszMD, const char* pszDomain = "" )
                                                                     override;

    static int          Identify( GDALOpenInfo * );
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszOptions );
    static GDALDataset* CreateCopy( const char *pszFilename,
                                       GDALDataset *poSrcDS,
                                       int bStrict,
                                       char ** papszOptions,
                                       GDALProgressFunc pfnProgress,
                                       void * pProgressData );
};

/************************************************************************/
/* ==================================================================== */
/*                             ISISTiledBand                            */
/* ==================================================================== */
/************************************************************************/

class ISISTiledBand : public GDALPamRasterBand
{
        friend class ISIS3Dataset;

        VSILFILE *m_fpVSIL;
        GIntBig   m_nFirstTileOffset;
        GIntBig   m_nXTileOffset;
        GIntBig   m_nYTileOffset;
        int       m_bNativeOrder;
        bool      m_bHasOffset;
        bool      m_bHasScale;
        double    m_dfOffset;
        double    m_dfScale;
        double    m_dfNoData;

  public:

                ISISTiledBand( GDALDataset *poDS, VSILFILE *fpVSIL,
                               int nBand, GDALDataType eDT,
                               int nTileXSize, int nTileYSize,
                               GIntBig nFirstTileOffset,
                               GIntBig nXTileOffset,
                               GIntBig nYTileOffset,
                               int bNativeOrder );
        virtual     ~ISISTiledBand() {}

        virtual CPLErr          IReadBlock( int, int, void * ) override;
        virtual CPLErr          IWriteBlock( int, int, void * ) override;

        virtual double GetOffset( int *pbSuccess = NULL ) override;
        virtual double GetScale( int *pbSuccess = NULL ) override;
        virtual CPLErr SetOffset( double dfNewOffset ) override;
        virtual CPLErr SetScale( double dfNewScale ) override;
        virtual double GetNoDataValue( int *pbSuccess = NULL ) override;
        virtual CPLErr SetNoDataValue( double dfNewNoData ) override;

        void    SetMaskBand(GDALRasterBand* poMaskBand);
};

/************************************************************************/
/* ==================================================================== */
/*                        ISIS3RawRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class ISIS3RawRasterBand: public RawRasterBand
{
        friend class ISIS3Dataset;

        bool      m_bHasOffset;
        bool      m_bHasScale;
        double    m_dfOffset;
        double    m_dfScale;
        double    m_dfNoData;

    public:
                 ISIS3RawRasterBand( GDALDataset *l_poDS, int l_nBand,
                                     void * l_fpRaw,
                                     vsi_l_offset l_nImgOffset,
                                     int l_nPixelOffset,
                                     int l_nLineOffset,
                                     GDALDataType l_eDataType,
                                     int l_bNativeOrder,
                                     int l_bIsVSIL = FALSE,
                                     int l_bOwnsFP = FALSE );
        virtual ~ISIS3RawRasterBand() {}

        virtual CPLErr          IReadBlock( int, int, void * ) override;
        virtual CPLErr          IWriteBlock( int, int, void * ) override;

        virtual CPLErr  IRasterIO( GDALRWFlag, int, int, int, int,
                                void *, int, int, GDALDataType,
                                GSpacing nPixelSpace, GSpacing nLineSpace,
                                GDALRasterIOExtraArg* psExtraArg ) override;

        virtual double GetOffset( int *pbSuccess = NULL ) override;
        virtual double GetScale( int *pbSuccess = NULL ) override;
        virtual CPLErr SetOffset( double dfNewOffset ) override;
        virtual CPLErr SetScale( double dfNewScale ) override;
        virtual double GetNoDataValue( int *pbSuccess = NULL ) override;
        virtual CPLErr SetNoDataValue( double dfNewNoData ) override;

        void    SetMaskBand(GDALRasterBand* poMaskBand);
};

/************************************************************************/
/* ==================================================================== */
/*                         ISIS3WrapperRasterBand                       */
/*                                                                      */
/*      proxy for bands stored in other formats.                        */
/* ==================================================================== */
/************************************************************************/
class ISIS3WrapperRasterBand : public GDALProxyRasterBand
{
        friend class ISIS3Dataset;

        GDALRasterBand* m_poBaseBand;
        bool      m_bHasOffset;
        bool      m_bHasScale;
        double    m_dfOffset;
        double    m_dfScale;
        double    m_dfNoData;

  protected:
    virtual GDALRasterBand* RefUnderlyingRasterBand() override
                                                    { return m_poBaseBand; }

  public:
            explicit ISIS3WrapperRasterBand( GDALRasterBand* poBaseBandIn );
            ~ISIS3WrapperRasterBand() {}

        void    InitFile();

        virtual CPLErr          IWriteBlock( int, int, void * ) override;

        virtual CPLErr  IRasterIO( GDALRWFlag, int, int, int, int,
                                void *, int, int, GDALDataType,
                                GSpacing nPixelSpace, GSpacing nLineSpace,
                                GDALRasterIOExtraArg* psExtraArg ) override;

        virtual double GetOffset( int *pbSuccess = NULL ) override;
        virtual double GetScale( int *pbSuccess = NULL ) override;
        virtual CPLErr SetOffset( double dfNewOffset ) override;
        virtual CPLErr SetScale( double dfNewScale ) override;
        virtual double GetNoDataValue( int *pbSuccess = NULL ) override;
        virtual CPLErr SetNoDataValue( double dfNewNoData ) override;

        int             GetMaskFlags() override { return nMaskFlags; }
        GDALRasterBand* GetMaskBand() override { return poMask; }
        void            SetMaskBand(GDALRasterBand* poMaskBand);
};

/************************************************************************/
/* ==================================================================== */
/*                             ISISMaskBand                             */
/* ==================================================================== */

class ISISMaskBand : public GDALRasterBand
{
    GDALRasterBand  *m_poBaseBand;
    void            *m_pBuffer;

  public:

                            ISISMaskBand( GDALRasterBand* poBaseBand );
                           ~ISISMaskBand();

    virtual CPLErr          IReadBlock( int, int, void * ) override;

};

/************************************************************************/
/*                           ISISTiledBand()                            */
/************************************************************************/

ISISTiledBand::ISISTiledBand( GDALDataset *poDSIn, VSILFILE *fpVSILIn,
                              int nBandIn, GDALDataType eDT,
                              int nTileXSize, int nTileYSize,
                              GIntBig nFirstTileOffsetIn,
                              GIntBig nXTileOffsetIn,
                              GIntBig nYTileOffsetIn,
                              int bNativeOrderIn ) :
    m_fpVSIL(fpVSILIn),
    m_nFirstTileOffset(0),
    m_nXTileOffset(nXTileOffsetIn),
    m_nYTileOffset(nYTileOffsetIn),
    m_bNativeOrder(bNativeOrderIn),
    m_bHasOffset(false),
    m_bHasScale(false),
    m_dfOffset(0.0),
    m_dfScale(1.0),
    m_dfNoData(0.0)
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = eDT;
    nBlockXSize = nTileXSize;
    nBlockYSize = nTileYSize;
    nRasterXSize = poDSIn->GetRasterXSize();
    nRasterYSize = poDSIn->GetRasterYSize();

    const int l_nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, nBlockXSize);
    const int l_nBlocksPerColumn = DIV_ROUND_UP(nRasterYSize, nBlockYSize);

    if( m_nXTileOffset == 0 && m_nYTileOffset == 0 )
    {
        m_nXTileOffset =
            static_cast<GIntBig>(GDALGetDataTypeSizeBytes(eDT)) *
            nTileXSize * nTileYSize;
        m_nYTileOffset = m_nXTileOffset * l_nBlocksPerRow;
    }

    m_nFirstTileOffset = nFirstTileOffsetIn
        + (nBand-1) * m_nYTileOffset * l_nBlocksPerColumn;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr ISISTiledBand::IReadBlock( int nXBlock, int nYBlock, void *pImage )

{
    ISIS3Dataset* poGDS = reinterpret_cast<ISIS3Dataset*>(poDS);
    if( poGDS->m_osExternalFilename.empty() )
    {
        if( !poGDS->m_bIsLabelWritten )
            poGDS->WriteLabel();
    }

    const GIntBig  nOffset = m_nFirstTileOffset +
        nXBlock * m_nXTileOffset + nYBlock * m_nYTileOffset;
    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
    const size_t nBlockSize = static_cast<size_t>(nDTSize)
                                            * nBlockXSize * nBlockYSize;

    if( VSIFSeekL( m_fpVSIL, nOffset, SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to seek to offset %d to read tile %d,%d.",
                  static_cast<int>( nOffset ), nXBlock, nYBlock );
        return CE_Failure;
    }

    if( VSIFReadL( pImage, 1, nBlockSize, m_fpVSIL ) != nBlockSize )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to read %d bytes for tile %d,%d.",
                  static_cast<int>( nBlockSize ), nXBlock, nYBlock );
        return CE_Failure;
    }

    if( !m_bNativeOrder && eDataType != GDT_Byte )
        GDALSwapWords( pImage, nDTSize,
                       nBlockXSize*nBlockYSize,
                       nDTSize );

    return CE_None;
}

/************************************************************************/
/*                           RemapNoDataT()                             */
/************************************************************************/

template<class T> static void RemapNoDataT( T* pBuffer, int nItems,
                                            T srcNoData, T dstNoData )
{
    for( int i = 0; i < nItems; i++ )
    {
        if( pBuffer[i] == srcNoData )
            pBuffer[i] = dstNoData;
    }
}

/************************************************************************/
/*                            RemapNoData()                             */
/************************************************************************/

static void RemapNoData( GDALDataType eDataType,
                         void* pBuffer, int nItems, double dfSrcNoData,
                         double dfDstNoData )
{
    if( eDataType == GDT_Byte )
    {
        RemapNoDataT( reinterpret_cast<GByte*>(pBuffer),
                      nItems,
                      static_cast<GByte>(dfSrcNoData),
                      static_cast<GByte>(dfDstNoData) );
    }
    else if( eDataType == GDT_UInt16 )
    {
        RemapNoDataT( reinterpret_cast<GUInt16*>(pBuffer),
                      nItems,
                      static_cast<GUInt16>(dfSrcNoData),
                      static_cast<GUInt16>(dfDstNoData) );
    }
    else if( eDataType == GDT_Int16)
    {
        RemapNoDataT( reinterpret_cast<GInt16*>(pBuffer),
                      nItems,
                      static_cast<GInt16>(dfSrcNoData),
                      static_cast<GInt16>(dfDstNoData) );
    }
    else
    {
        CPLAssert( eDataType == GDT_Float32 );
        RemapNoDataT( reinterpret_cast<float*>(pBuffer),
                      nItems,
                      static_cast<float>(dfSrcNoData),
                      static_cast<float>(dfDstNoData) );
    }
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr ISISTiledBand::IWriteBlock( int nXBlock, int nYBlock, void *pImage )

{
    ISIS3Dataset* poGDS = reinterpret_cast<ISIS3Dataset*>(poDS);
    if( poGDS->m_osExternalFilename.empty() )
    {
        if( !poGDS->m_bIsLabelWritten )
            poGDS->WriteLabel();
    }

    if( poGDS->m_bHasSrcNoData && poGDS->m_dfSrcNoData != m_dfNoData )
    {
        RemapNoData( eDataType, pImage, nBlockXSize * nBlockYSize,
                     poGDS->m_dfSrcNoData, m_dfNoData );
    }

    const GIntBig  nOffset = m_nFirstTileOffset +
        nXBlock * m_nXTileOffset + nYBlock * m_nYTileOffset;
    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
    const size_t nBlockSize = static_cast<size_t>(nDTSize)
                                            * nBlockXSize * nBlockYSize;

    const int l_nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, nBlockXSize);
    const int l_nBlocksPerColumn = DIV_ROUND_UP(nRasterYSize, nBlockYSize);

    // Pad partial blocks to nodata value
    if( nXBlock == l_nBlocksPerRow - 1 && (nRasterXSize % nBlockXSize) != 0 )
    {
        GByte* pabyImage = static_cast<GByte*>(pImage);
        int nXStart = nRasterXSize % nBlockXSize;
        for( int iY = 0; iY < nBlockXSize; iY++ )
        {
            GDALCopyWords( &m_dfNoData, GDT_Float64, 0,
                           pabyImage + (iY * nBlockXSize + nXStart) * nDTSize,
                           eDataType, nDTSize,
                           nBlockXSize - nXStart );
        }
    }
    if( nYBlock == l_nBlocksPerColumn - 1 &&
        (nRasterYSize % nBlockYSize) != 0 )
    {
        GByte* pabyImage = static_cast<GByte*>(pImage);
        for( int iY = nRasterYSize % nBlockYSize; iY < nBlockYSize; iY++ )
        {
            GDALCopyWords( &m_dfNoData, GDT_Float64, 0,
                           pabyImage + iY * nBlockXSize * nDTSize,
                           eDataType, nDTSize,
                           nBlockXSize );
        }
    }

    if( VSIFSeekL( m_fpVSIL, nOffset, SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to seek to offset %d to read tile %d,%d.",
                  static_cast<int>( nOffset ), nXBlock, nYBlock );
        return CE_Failure;
    }

    if( !m_bNativeOrder && eDataType != GDT_Byte )
        GDALSwapWords( pImage, nDTSize,
                       nBlockXSize*nBlockYSize,
                       nDTSize );

    if( VSIFWriteL( pImage, 1, nBlockSize, m_fpVSIL ) != nBlockSize )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to write %d bytes for tile %d,%d.",
                  static_cast<int>( nBlockSize ), nXBlock, nYBlock );
        return CE_Failure;
    }

    if( !m_bNativeOrder && eDataType != GDT_Byte )
        GDALSwapWords( pImage, nDTSize,
                       nBlockXSize*nBlockYSize,
                       nDTSize );

    return CE_None;
}

/************************************************************************/
/*                             SetMaskBand()                            */
/************************************************************************/

void ISISTiledBand::SetMaskBand(GDALRasterBand* poMaskBand)
{
    bOwnMask = true;
    poMask = poMaskBand;
    nMaskFlags = 0;
}

/************************************************************************/
/*                              GetOffset()                             */
/************************************************************************/

double ISISTiledBand::GetOffset( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = m_bHasOffset;
    return m_dfOffset;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double ISISTiledBand::GetScale( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = m_bHasScale;
    return m_dfScale;
}

/************************************************************************/
/*                              SetOffset()                             */
/************************************************************************/

CPLErr ISISTiledBand::SetOffset( double dfNewOffset )
{
    m_dfOffset = dfNewOffset;
    m_bHasOffset = true;
    return CE_None;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr ISISTiledBand::SetScale( double dfNewScale )
{
    m_dfScale = dfNewScale;
    m_bHasScale = true;
    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double ISISTiledBand::GetNoDataValue( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = true;
    return m_dfNoData;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr ISISTiledBand::SetNoDataValue( double dfNewNoData )
{
    m_dfNoData = dfNewNoData;
    return CE_None;
}

/************************************************************************/
/*                       ISIS3RawRasterBand()                           */
/************************************************************************/

ISIS3RawRasterBand::ISIS3RawRasterBand( GDALDataset *l_poDS, int l_nBand,
                                        void * l_fpRaw,
                                        vsi_l_offset l_nImgOffset,
                                        int l_nPixelOffset,
                                        int l_nLineOffset,
                                        GDALDataType l_eDataType,
                                        int l_bNativeOrder,
                                        int l_bIsVSIL, int l_bOwnsFP )
    : RawRasterBand(l_poDS, l_nBand, l_fpRaw, l_nImgOffset, l_nPixelOffset,
                    l_nLineOffset,
                    l_eDataType, l_bNativeOrder, l_bIsVSIL, l_bOwnsFP),
    m_bHasOffset(false),
    m_bHasScale(false),
    m_dfOffset(0.0),
    m_dfScale(1.0),
    m_dfNoData(0.0)
{
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr ISIS3RawRasterBand::IReadBlock( int nXBlock, int nYBlock, void *pImage )

{
    ISIS3Dataset* poGDS = reinterpret_cast<ISIS3Dataset*>(poDS);
    if( poGDS->m_osExternalFilename.empty() )
    {
        if( !poGDS->m_bIsLabelWritten )
            poGDS->WriteLabel();
    }
    return RawRasterBand::IReadBlock( nXBlock, nYBlock, pImage );
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr ISIS3RawRasterBand::IWriteBlock( int nXBlock, int nYBlock,
                                        void *pImage )

{
    ISIS3Dataset* poGDS = reinterpret_cast<ISIS3Dataset*>(poDS);
    if( poGDS->m_osExternalFilename.empty() )
    {
        if( !poGDS->m_bIsLabelWritten )
            poGDS->WriteLabel();
    }

    if( poGDS->m_bHasSrcNoData && poGDS->m_dfSrcNoData != m_dfNoData )
    {
        RemapNoData( eDataType, pImage, nBlockXSize * nBlockYSize,
                     poGDS->m_dfSrcNoData, m_dfNoData );
    }

    return RawRasterBand::IWriteBlock( nXBlock, nYBlock, pImage );
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr ISIS3RawRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                 int nXOff, int nYOff, int nXSize, int nYSize,
                                 void * pData, int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType,
                                 GSpacing nPixelSpace, GSpacing nLineSpace,
                                 GDALRasterIOExtraArg* psExtraArg )

{
    ISIS3Dataset* poGDS = reinterpret_cast<ISIS3Dataset*>(poDS);
    if( poGDS->m_osExternalFilename.empty() )
    {
        if( !poGDS->m_bIsLabelWritten )
            poGDS->WriteLabel();
    }
    if( eRWFlag == GF_Write && 
        poGDS->m_bHasSrcNoData && poGDS->m_dfSrcNoData != m_dfNoData )
    {
        const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
        if( eBufType == eDataType && nPixelSpace == nDTSize &&
            nLineSpace == nPixelSpace * nBufXSize )
        {
            RemapNoData( eDataType, pData, nBufXSize * nBufYSize,
                         poGDS->m_dfSrcNoData, m_dfNoData );
        }
        else
        {
            const GByte* pabySrc = reinterpret_cast<GByte*>(pData);
            GByte* pabyTemp = reinterpret_cast<GByte*>(
                VSI_MALLOC3_VERBOSE(nDTSize, nBufXSize, nBufYSize));
            for( int i = 0; i < nBufYSize; i++ )
            {
                GDALCopyWords( pabySrc + i * nLineSpace, eBufType,
                               static_cast<int>(nPixelSpace),
                               pabyTemp + i * nBufXSize * nDTSize,
                               eDataType, nDTSize,
                               nBufXSize );
            }
            RemapNoData( eDataType, pabyTemp, nBufXSize * nBufYSize,
                         poGDS->m_dfSrcNoData, m_dfNoData );
            CPLErr eErr = RawRasterBand::IRasterIO( eRWFlag,
                                     nXOff, nYOff, nXSize, nYSize,
                                     pabyTemp, nBufXSize, nBufYSize,
                                     eDataType,
                                     nDTSize, nDTSize*nBufXSize,
                                     psExtraArg );
            VSIFree(pabyTemp);
            return eErr;
        }
    }
    return RawRasterBand::IRasterIO( eRWFlag,
                                     nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize,
                                     eBufType,
                                     nPixelSpace, nLineSpace,
                                     psExtraArg );
}

/************************************************************************/
/*                             SetMaskBand()                            */
/************************************************************************/

void ISIS3RawRasterBand::SetMaskBand(GDALRasterBand* poMaskBand)
{
    bOwnMask = true;
    poMask = poMaskBand;
    nMaskFlags = 0;
}

/************************************************************************/
/*                              GetOffset()                             */
/************************************************************************/

double ISIS3RawRasterBand::GetOffset( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = m_bHasOffset;
    return m_dfOffset;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double ISIS3RawRasterBand::GetScale( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = m_bHasScale;
    return m_dfScale;
}

/************************************************************************/
/*                              SetOffset()                             */
/************************************************************************/

CPLErr ISIS3RawRasterBand::SetOffset( double dfNewOffset )
{
    m_dfOffset = dfNewOffset;
    m_bHasOffset = true;
    return CE_None;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr ISIS3RawRasterBand::SetScale( double dfNewScale )
{
    m_dfScale = dfNewScale;
    m_bHasScale = true;
    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double ISIS3RawRasterBand::GetNoDataValue( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = true;
    return m_dfNoData;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr ISIS3RawRasterBand::SetNoDataValue( double dfNewNoData )
{
    m_dfNoData = dfNewNoData;
    return CE_None;
}

/************************************************************************/
/*                        ISIS3WrapperRasterBand()                      */
/************************************************************************/

ISIS3WrapperRasterBand::ISIS3WrapperRasterBand( GDALRasterBand* poBaseBandIn ) :
    m_poBaseBand(poBaseBandIn),
    m_bHasOffset(false),
    m_bHasScale(false),
    m_dfOffset(0.0),
    m_dfScale(1.0),
    m_dfNoData(0.0)
{
    eDataType = m_poBaseBand->GetRasterDataType();
    m_poBaseBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
}

/************************************************************************/
/*                             SetMaskBand()                            */
/************************************************************************/

void ISIS3WrapperRasterBand::SetMaskBand(GDALRasterBand* poMaskBand)
{
    bOwnMask = true;
    poMask = poMaskBand;
    nMaskFlags = 0;
}

/************************************************************************/
/*                              GetOffset()                             */
/************************************************************************/

double ISIS3WrapperRasterBand::GetOffset( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = m_bHasOffset;
    return m_dfOffset;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double ISIS3WrapperRasterBand::GetScale( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = m_bHasScale;
    return m_dfScale;
}

/************************************************************************/
/*                              SetOffset()                             */
/************************************************************************/

CPLErr ISIS3WrapperRasterBand::SetOffset( double dfNewOffset )
{
    m_dfOffset = dfNewOffset;
    m_bHasOffset = true;

    ISIS3Dataset* poGDS = reinterpret_cast<ISIS3Dataset*>(poDS);
    if( poGDS->m_poExternalDS && eAccess == GA_Update )
        poGDS->m_poExternalDS->GetRasterBand(nBand)->SetOffset(dfNewOffset);

    return CE_None;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr ISIS3WrapperRasterBand::SetScale( double dfNewScale )
{
    m_dfScale = dfNewScale;
    m_bHasScale = true;

    ISIS3Dataset* poGDS = reinterpret_cast<ISIS3Dataset*>(poDS);
    if( poGDS->m_poExternalDS && eAccess == GA_Update )
        poGDS->m_poExternalDS->GetRasterBand(nBand)->SetScale(dfNewScale);

    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double ISIS3WrapperRasterBand::GetNoDataValue( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = true;
    return m_dfNoData;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr ISIS3WrapperRasterBand::SetNoDataValue( double dfNewNoData )
{
    m_dfNoData = dfNewNoData;

    ISIS3Dataset* poGDS = reinterpret_cast<ISIS3Dataset*>(poDS);
    if( poGDS->m_poExternalDS && eAccess == GA_Update )
        poGDS->m_poExternalDS->GetRasterBand(nBand)->SetNoDataValue(dfNewNoData);

    return CE_None;
}

/************************************************************************/
/*                              InitFile()                              */
/************************************************************************/

void ISIS3WrapperRasterBand::InitFile()
{
    ISIS3Dataset* poGDS = reinterpret_cast<ISIS3Dataset*>(poDS);
    if( poGDS->m_bGeoTIFFAsRegularExternal && !poGDS->m_bGeoTIFFInitDone )
    {
        poGDS->m_bGeoTIFFInitDone = true;

        const int nBands = poGDS->GetRasterCount();
        // We need to make sure that blocks are written in the right order
        for( int i = 0; i < nBands; i++ )
        {
            poGDS->m_poExternalDS->GetRasterBand(i+1)->Fill(m_dfNoData);
        }
        poGDS->m_poExternalDS->FlushCache();

        // Check that blocks are effectively written in expected order.
        const int nBlockSizeBytes = nBlockXSize * nBlockYSize *
                                        GDALGetDataTypeSizeBytes(eDataType);

        GIntBig nLastOffset = 0;
        bool bGoOn = true;
        const int l_nBlocksPerRow = DIV_ROUND_UP(nRasterXSize, nBlockXSize);
        const int l_nBlocksPerColumn = DIV_ROUND_UP(nRasterYSize, nBlockYSize);
        for( int i = 0; i < nBands && bGoOn; i++ )
        {
            for( int y = 0; y < l_nBlocksPerColumn && bGoOn; y++ )
            {
                for( int x = 0; x < l_nBlocksPerRow && bGoOn; x++ )
                {
                    const char* pszBlockOffset =  poGDS->m_poExternalDS->
                        GetRasterBand(i+1)->GetMetadataItem(
                            CPLSPrintf("BLOCK_OFFSET_%d_%d", x, y), "TIFF");
                    if( pszBlockOffset )
                    {
                        GIntBig nOffset = CPLAtoGIntBig(pszBlockOffset);
                        if( i != 0 || x != 0 || y != 0 )
                        {
                            if( nOffset != nLastOffset + nBlockSizeBytes )
                            {
                                CPLError(CE_Warning, CPLE_AppDefined,
                                         "Block %d,%d band %d not at expected "
                                         "offset",
                                         x, y, i+1);
                                bGoOn = false;
                                poGDS->m_bGeoTIFFAsRegularExternal = false;
                            }
                        }
                        nLastOffset = nOffset;
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                         "Block %d,%d band %d not at expected "
                                         "offset",
                                         x, y, i+1);
                        bGoOn = false;
                        poGDS->m_bGeoTIFFAsRegularExternal = false;
                    }
                }
            }
        }
    }
}

/************************************************************************/
/*                             IWriteBlock()                             */
/************************************************************************/

CPLErr ISIS3WrapperRasterBand::IWriteBlock( int nXBlock, int nYBlock,
                                            void *pImage )

{
    ISIS3Dataset* poGDS = reinterpret_cast<ISIS3Dataset*>(poDS);
    if( poGDS->m_bHasSrcNoData && poGDS->m_dfSrcNoData != m_dfNoData )
    {
        RemapNoData( eDataType, pImage, nBlockXSize * nBlockYSize,
                     poGDS->m_dfSrcNoData, m_dfNoData );
    }
    if( poGDS->m_bGeoTIFFAsRegularExternal && poGDS->m_bGeoTIFFInitDone )
    {
        InitFile();
    }

    return GDALProxyRasterBand::IWriteBlock( nXBlock, nYBlock, pImage );
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr ISIS3WrapperRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                 int nXOff, int nYOff, int nXSize, int nYSize,
                                 void * pData, int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType,
                                 GSpacing nPixelSpace, GSpacing nLineSpace,
                                 GDALRasterIOExtraArg* psExtraArg )

{
    ISIS3Dataset* poGDS = reinterpret_cast<ISIS3Dataset*>(poDS);
    if( eRWFlag == GF_Write && poGDS->m_bGeoTIFFAsRegularExternal &&
        !poGDS->m_bGeoTIFFInitDone )
    {
        InitFile();
    }
    if( eRWFlag == GF_Write && 
        poGDS->m_bHasSrcNoData && poGDS->m_dfSrcNoData != m_dfNoData )
    {
        const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
        if( eBufType == eDataType && nPixelSpace == nDTSize &&
            nLineSpace == nPixelSpace * nBufXSize )
        {
            RemapNoData( eDataType, pData, nBufXSize * nBufYSize,
                         poGDS->m_dfSrcNoData, m_dfNoData );
        }
        else
        {
            const GByte* pabySrc = reinterpret_cast<GByte*>(pData);
            GByte* pabyTemp = reinterpret_cast<GByte*>(
                VSI_MALLOC3_VERBOSE(nDTSize, nBufXSize, nBufYSize));
            for( int i = 0; i < nBufYSize; i++ )
            {
                GDALCopyWords( pabySrc + i * nLineSpace, eBufType,
                               static_cast<int>(nPixelSpace),
                               pabyTemp + i * nBufXSize * nDTSize,
                               eDataType, nDTSize,
                               nBufXSize );
            }
            RemapNoData( eDataType, pabyTemp, nBufXSize * nBufYSize,
                         poGDS->m_dfSrcNoData, m_dfNoData );
            CPLErr eErr = GDALProxyRasterBand::IRasterIO( eRWFlag,
                                     nXOff, nYOff, nXSize, nYSize,
                                     pabyTemp, nBufXSize, nBufYSize,
                                     eDataType,
                                     nDTSize, nDTSize*nBufXSize,
                                     psExtraArg );
            VSIFree(pabyTemp);
            return eErr;
        }
    }
    return GDALProxyRasterBand::IRasterIO( eRWFlag,
                                     nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize,
                                     eBufType,
                                     nPixelSpace, nLineSpace,
                                     psExtraArg );
}

/************************************************************************/
/*                            ISISMaskBand()                            */
/************************************************************************/

ISISMaskBand::ISISMaskBand( GDALRasterBand* poBaseBand )
    : m_poBaseBand(poBaseBand)
    , m_pBuffer(NULL)
{
    eDataType = GDT_Byte;
    poBaseBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
    nRasterXSize = poBaseBand->GetXSize();
    nRasterYSize = poBaseBand->GetYSize();
}

/************************************************************************/
/*                           ~ISISMaskBand()                            */
/************************************************************************/

ISISMaskBand::~ISISMaskBand()
{
    VSIFree(m_pBuffer);
}

/************************************************************************/
/*                             FillMask()                               */
/************************************************************************/

template<class T>
static void FillMask      (void* pvBuffer,
                           GByte* pabyDst,
                           int nReqXSize, int nReqYSize,
                           int nBlockXSize,
                           T NULL_VAL, T LOW_REPR_SAT, T LOW_INSTR_SAT,
                           T HIGH_INSTR_SAT, T HIGH_REPR_SAT)
{
    const T* pSrc = static_cast<T*>(pvBuffer);
    for( int y = 0; y < nReqYSize; y++ )
    {
        for( int x = 0; x < nReqXSize; x++ )
        {
            const T nSrc = pSrc[y * nBlockXSize + x];
            if( nSrc == NULL_VAL ||
                nSrc == LOW_REPR_SAT ||
                nSrc == LOW_INSTR_SAT ||
                nSrc == HIGH_INSTR_SAT ||
                nSrc == HIGH_REPR_SAT )
            {
                pabyDst[y * nBlockXSize + x] = 0;
            }
            else
            {
                pabyDst[y * nBlockXSize + x] = 255;
            }
        }
    }
}

/************************************************************************/
/*                           IReadBlock()                               */
/************************************************************************/

CPLErr ISISMaskBand::IReadBlock( int nXBlock, int nYBlock, void *pImage )

{
    const GDALDataType eSrcDT = m_poBaseBand->GetRasterDataType();
    const int nSrcDTSize = GDALGetDataTypeSizeBytes(eSrcDT);
    if( m_pBuffer == NULL )
    {
        m_pBuffer = VSI_MALLOC3_VERBOSE(nBlockXSize, nBlockYSize, nSrcDTSize);
        if( m_pBuffer == NULL )
            return CE_Failure;
    }

    int nXOff = nXBlock * nBlockXSize;
    int nReqXSize = nBlockXSize;
    if( nXOff + nReqXSize > nRasterXSize )
        nReqXSize = nRasterXSize - nXOff;
    int nYOff = nYBlock * nBlockYSize;
    int nReqYSize = nBlockYSize;
    if( nYOff + nReqYSize > nRasterYSize )
        nReqYSize = nRasterYSize - nYOff;

    if( m_poBaseBand->RasterIO( GF_Read,
                                nXOff, nYOff, nReqXSize, nReqYSize,
                                m_pBuffer,
                                nReqXSize, nReqYSize,
                                eSrcDT,
                                nSrcDTSize,
                                nSrcDTSize * nBlockXSize,
                                NULL ) != CE_None )
    {
        return CE_Failure;
    }

    GByte* pabyDst = static_cast<GByte*>(pImage);
    if( eSrcDT == GDT_Byte )
    {
        FillMask<GByte>(m_pBuffer, pabyDst, nReqXSize, nReqYSize, nBlockXSize,
                        NULL1, LOW_REPR_SAT1, LOW_INSTR_SAT1,
                        HIGH_INSTR_SAT1, HIGH_REPR_SAT1);
    }
    else if( eSrcDT == GDT_UInt16 )
    {
        FillMask<GUInt16>(m_pBuffer, pabyDst, nReqXSize, nReqYSize, nBlockXSize,
                        NULLU2, LOW_REPR_SATU2, LOW_INSTR_SATU2,
                        HIGH_INSTR_SATU2, HIGH_REPR_SATU2);
    }
    else if( eSrcDT == GDT_Int16 )
    {
        FillMask<GInt16>(m_pBuffer, pabyDst, nReqXSize, nReqYSize, nBlockXSize,
                        NULL2, LOW_REPR_SAT2, LOW_INSTR_SAT2,
                        HIGH_INSTR_SAT2, HIGH_REPR_SAT2);
    }
    else
    {
        CPLAssert( eSrcDT == GDT_Float32 );
        FillMask<float>(m_pBuffer, pabyDst, nReqXSize, nReqYSize, nBlockXSize,
                        NULL4, LOW_REPR_SAT4, LOW_INSTR_SAT4,
                        HIGH_INSTR_SAT4, HIGH_REPR_SAT4);
    }

    return CE_None;
}

/************************************************************************/
/*                            ISIS3Dataset()                            */
/************************************************************************/

ISIS3Dataset::ISIS3Dataset() :
    m_fpLabel(NULL),
    m_fpImage(NULL),
    m_poExternalDS(NULL),
    m_bGeoTIFFAsRegularExternal(false),
    m_bGeoTIFFInitDone(true),
    m_bIsLabelWritten(true),
    m_bIsTiled(false),
    m_bInitToNodata(false),
    m_bGotTransform(false),
    m_bHasSrcNoData(false),
    m_dfSrcNoData(0.0),
    m_bWriteBoundingDegrees(true),
    m_poJSonLabel(NULL),
    m_bUseSrcLabel(true),
    m_bUseSrcMapping(false),
    m_poSrcJSonLabel(NULL)
{
    m_oKeywords.SetStripSurroundingQuotes(true);
    m_adfGeoTransform[0] = 0.0;
    m_adfGeoTransform[1] = 1.0;
    m_adfGeoTransform[2] = 0.0;
    m_adfGeoTransform[3] = 0.0;
    m_adfGeoTransform[4] = 0.0;
    m_adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                           ~ISIS3Dataset()                            */
/************************************************************************/

ISIS3Dataset::~ISIS3Dataset()

{
    if( !m_bIsLabelWritten )
        WriteLabel();
    if( m_poExternalDS && m_bGeoTIFFAsRegularExternal && !m_bGeoTIFFInitDone )
    {
        reinterpret_cast<ISIS3WrapperRasterBand*>(GetRasterBand(1))->
            InitFile();
    }
    FlushCache();
    if( m_fpLabel != NULL )
        VSIFCloseL( m_fpLabel );
    if( m_fpImage != NULL && m_fpImage != m_fpLabel )
        VSIFCloseL( m_fpImage );
    if( m_poJSonLabel != NULL )
        json_object_put(m_poJSonLabel);
    if( m_poSrcJSonLabel != NULL )
        json_object_put(m_poSrcJSonLabel);
    CloseDependentDatasets();
}

/************************************************************************/
/*                        CloseDependentDatasets()                      */
/************************************************************************/

int ISIS3Dataset::CloseDependentDatasets()
{
    int bHasDroppedRef = GDALPamDataset::CloseDependentDatasets();

    if( m_poExternalDS )
    {
        bHasDroppedRef = FALSE;
        delete m_poExternalDS;
        m_poExternalDS = NULL;
    }

    for( int iBand = 0; iBand < nBands; iBand++ )
    {
       delete papoBands[iBand];
    }
    nBands = 0;

    return bHasDroppedRef;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **ISIS3Dataset::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();

    if( !m_osExternalFilename.empty() )
        papszFileList = CSLAddString( papszFileList, m_osExternalFilename );
    for( int i = 0; i < m_aosAdditionalFiles.Count(); ++i )
    {
        if( CSLFindString(papszFileList, m_aosAdditionalFiles[i]) < 0 )
        {
            papszFileList = CSLAddString( papszFileList,
                                          m_aosAdditionalFiles[i] );
        }
    }

    return papszFileList;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *ISIS3Dataset::GetProjectionRef()

{
    if( !m_osProjection.empty() )
        return m_osProjection;

    return GDALPamDataset::GetProjectionRef();
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr ISIS3Dataset::SetProjection( const char* pszProjection )
{
    if( eAccess == GA_ReadOnly )
        return GDALPamDataset::SetProjection( pszProjection );
    m_osProjection = pszProjection ? pszProjection : "";
    if( m_poExternalDS )
        m_poExternalDS->SetProjection(pszProjection);
    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr ISIS3Dataset::GetGeoTransform( double * padfTransform )

{
    if( m_bGotTransform )
    {
        memcpy( padfTransform, m_adfGeoTransform, sizeof(double) * 6 );
        return CE_None;
    }

    return GDALPamDataset::GetGeoTransform( padfTransform );
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr ISIS3Dataset::SetGeoTransform( double * padfTransform )

{
    if( eAccess == GA_ReadOnly )
        return GDALPamDataset::SetGeoTransform( padfTransform );
    if( padfTransform[1] <= 0.0 || padfTransform[1] != -padfTransform[5] ||
        padfTransform[2] != 0.0 || padfTransform[4] != 0.0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only north-up geotransform with square pixels supported");
        return CE_Failure;
    }
    m_bGotTransform = true;
    memcpy( m_adfGeoTransform, padfTransform, sizeof(double) * 6 );
    if( m_poExternalDS )
        m_poExternalDS->SetGeoTransform(padfTransform);
    return CE_None;
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **ISIS3Dataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(
        NULL, FALSE, "", "json:ISIS3", NULL);
}

/************************************************************************/
/*                             GetMetadata()                            */
/************************************************************************/

char **ISIS3Dataset::GetMetadata( const char* pszDomain )
{
    if( pszDomain != NULL && EQUAL( pszDomain, "json:ISIS3" ) )
    {
        if( m_aosISIS3MD.empty() )
        {
            if( eAccess == GA_Update && m_poJSonLabel == NULL )
            {
                m_poJSonLabel = BuildLabel();
            }
            CPLAssert( m_poJSonLabel != NULL );
            const char* pszJSon = json_object_to_json_string_ext(
                m_poJSonLabel, JSON_C_TO_STRING_PRETTY);
            m_aosISIS3MD.InsertString(0, pszJSon);
        }
        return m_aosISIS3MD.List();
    }
    return GDALPamDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                             SetMetadata()                            */
/************************************************************************/

CPLErr ISIS3Dataset::SetMetadata( char** papszMD, const char* pszDomain )
{
    if( m_bUseSrcLabel && eAccess == GA_Update && pszDomain != NULL &&
        EQUAL( pszDomain, "json:ISIS3" ) )
    {
        if( m_poJSonLabel )
            json_object_put(m_poJSonLabel);
        m_poJSonLabel = NULL;
        if( m_poSrcJSonLabel )
            json_object_put(m_poSrcJSonLabel);
        m_poSrcJSonLabel = NULL;
        m_aosISIS3MD.Clear();
        if( papszMD != NULL && papszMD[0] != NULL )
        {
            if( !OGRJSonParse( papszMD[0], &m_poSrcJSonLabel, true ) )
            {
                return CE_Failure;
            }
        }
        return CE_None;
    }
    return GDALPamDataset::SetMetadata(papszMD, pszDomain);
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/
int ISIS3Dataset::Identify( GDALOpenInfo * poOpenInfo )

{
    if( poOpenInfo->pabyHeader != NULL
        && strstr((const char *)poOpenInfo->pabyHeader,"IsisCube") != NULL )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *ISIS3Dataset::Open( GDALOpenInfo * poOpenInfo )
{
/* -------------------------------------------------------------------- */
/*      Does this look like a CUBE dataset?                             */
/* -------------------------------------------------------------------- */
    if( !Identify( poOpenInfo ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Open the file using the large file API.                         */
/* -------------------------------------------------------------------- */
    VSILFILE *fpQube = VSIFOpenL( poOpenInfo->pszFilename, "rb" );

    if( fpQube == NULL )
        return NULL;

    ISIS3Dataset *poDS = new ISIS3Dataset();

    if( ! poDS->m_oKeywords.Ingest( fpQube, 0 ) )
    {
        VSIFCloseL( fpQube );
        delete poDS;
        return NULL;
    }
    poDS->m_poJSonLabel = poDS->m_oKeywords.StealJSon();

    VSIFCloseL( fpQube );

/* -------------------------------------------------------------------- */
/* Assume user is pointing to label (i.e. .lbl) file for detached option */
/* -------------------------------------------------------------------- */
    //  Image can be inline or detached and point to an image name
    //  the Format can be Tiled or Raw
    //  Object = Core
    //      StartByte   = 65537
    //      Format      = Tile
    //      TileSamples = 128
    //      TileLines   = 128
    //OR-----
    //  Object = Core
    //      StartByte = 1
    //      ^Core     = r0200357_detatched.cub
    //      Format    = BandSequential
    //OR-----
    //  Object = Core
    //      StartByte = 1
    //      ^Core     = r0200357_detached_tiled.cub
    //      Format      = Tile
    //      TileSamples = 128
    //      TileLines   = 128
    //OR-----
    //  Object = Core
    //      StartByte = 1
    //      ^Core     = some.tif
    //      Format    = GeoTIFF

/* -------------------------------------------------------------------- */
/*      What file contains the actual data?                             */
/* -------------------------------------------------------------------- */
    const char *pszCore = poDS->GetKeyword( "IsisCube.Core.^Core" );
    CPLString osQubeFile;

    if( EQUAL(pszCore,"") )
        osQubeFile = poOpenInfo->pszFilename;
    else
    {
        CPLString osPath = CPLGetPath( poOpenInfo->pszFilename );
        osQubeFile = CPLFormFilename( osPath, pszCore, NULL );
        poDS->m_osExternalFilename = osQubeFile;
    }

/* -------------------------------------------------------------------- */
/*      Check if file an ISIS3 header file?  Read a few lines of text   */
/*      searching for something starting with nrows or ncols.           */
/* -------------------------------------------------------------------- */

    /*************   Skipbytes     *****************************/
    const int nSkipBytes =
            atoi(poDS->GetKeyword("IsisCube.Core.StartByte", "1")) - 1;

    /*******   Grab format type (BandSequential, Tiled)  *******/
    CPLString osFormat = poDS->GetKeyword( "IsisCube.Core.Format" );

    int tileSizeX = 0;
    int tileSizeY = 0;

    if (EQUAL(osFormat,"Tile") ) 
    {
       poDS->m_bIsTiled = true;
       /******* Get Tile Sizes *********/
       tileSizeX = atoi(poDS->GetKeyword("IsisCube.Core.TileSamples"));
       tileSizeY = atoi(poDS->GetKeyword("IsisCube.Core.TileLines"));
       if (tileSizeX <= 0 || tileSizeY <= 0)
       {
           CPLError( CE_Failure, CPLE_OpenFailed,
                     "Wrong tile dimensions : %d x %d",
                     tileSizeX, tileSizeY);
           delete poDS;
           return NULL;
       }
    }
    else if (!EQUAL(osFormat,"BandSequential") &&
             !EQUAL(osFormat,"GeoTIFF") )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "%s format not supported.", osFormat.c_str());
        delete poDS;
        return NULL;
    }

    /***********   Grab samples lines band ************/
    const int nCols = atoi(poDS->GetKeyword("IsisCube.Core.Dimensions.Samples"));
    const int nRows = atoi(poDS->GetKeyword("IsisCube.Core.Dimensions.Lines"));
    const int nBands = atoi(poDS->GetKeyword("IsisCube.Core.Dimensions.Bands"));

    /****** Grab format type - ISIS3 only supports 8,U16,S16,32 *****/
    GDALDataType eDataType = GDT_Byte;
    double dfNoData = 0.0;

    const char *itype = poDS->GetKeyword( "IsisCube.Core.Pixels.Type" );
    if (EQUAL(itype,"UnsignedByte") ) {
        eDataType = GDT_Byte;
        dfNoData = NULL1;
    }
    else if (EQUAL(itype,"UnsignedWord") ) {
        eDataType = GDT_UInt16;
        dfNoData = NULLU2;
    }
    else if (EQUAL(itype,"SignedWord") ) {
        eDataType = GDT_Int16;
        dfNoData = NULL2;
    }
    else if (EQUAL(itype,"Real") || EQUAL(osFormat,"") ) {
        eDataType = GDT_Float32;
        dfNoData = NULL4;
    }
    else {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "%s layout type not supported.", itype);
        delete poDS;
        return NULL;
    }

    /***********   Grab samples lines band ************/

    //default to MSB
    const bool bIsLSB = EQUAL(
            poDS->GetKeyword( "IsisCube.Core.Pixels.ByteOrder"),"Lsb");

    /***********   Grab Cellsize ************/
    double dfXDim = 1.0;
    double dfYDim = 1.0;

    const char* pszRes = poDS->GetKeyword("IsisCube.Mapping.PixelResolution");
    if (strlen(pszRes) > 0 ) {
        dfXDim = CPLAtof(pszRes); /* values are in meters */
        dfYDim = -CPLAtof(pszRes);
    }

    /***********   Grab UpperLeftCornerY ************/
    double dfULYMap = 0.5;

    const char* pszULY = poDS->GetKeyword("IsisCube.Mapping.UpperLeftCornerY");
    if (strlen(pszULY) > 0) {
        dfULYMap = CPLAtof(pszULY);
    }

    /***********   Grab UpperLeftCornerX ************/
    double dfULXMap = 0.5;

    const char* pszULX = poDS->GetKeyword("IsisCube.Mapping.UpperLeftCornerX");
    if( strlen(pszULX) > 0 ) {
        dfULXMap = CPLAtof(pszULX);
    }

    /***********  Grab TARGET_NAME  ************/
    /**** This is the planets name i.e. Mars ***/
    const char *target_name = poDS->GetKeyword("IsisCube.Mapping.TargetName");

    /***********   Grab MAP_PROJECTION_TYPE ************/
     const char *map_proj_name =
        poDS->GetKeyword( "IsisCube.Mapping.ProjectionName");

    /***********   Grab SEMI-MAJOR ************/
    const double semi_major =
        CPLAtof(poDS->GetKeyword( "IsisCube.Mapping.EquatorialRadius"));

    /***********   Grab semi-minor ************/
    const double semi_minor =
        CPLAtof(poDS->GetKeyword( "IsisCube.Mapping.PolarRadius"));

    /***********   Grab CENTER_LAT ************/
    const double center_lat =
        CPLAtof(poDS->GetKeyword( "IsisCube.Mapping.CenterLatitude"));

    /***********   Grab CENTER_LON ************/
    const double center_lon =
        CPLAtof(poDS->GetKeyword( "IsisCube.Mapping.CenterLongitude"));

    /***********   Grab 1st std parallel ************/
    const double first_std_parallel =
        CPLAtof(poDS->GetKeyword( "IsisCube.Mapping.FirstStandardParallel"));

    /***********   Grab 2nd std parallel ************/
    const double second_std_parallel =
        CPLAtof(poDS->GetKeyword( "IsisCube.Mapping.SecondStandardParallel"));

    /***********   Grab scaleFactor ************/
    const double scaleFactor =
        CPLAtof(poDS->GetKeyword( "IsisCube.Mapping.scaleFactor", "1.0"));

    /*** grab      LatitudeType = Planetographic ****/
    // Need to further study how ocentric/ographic will effect the gdal library
    // So far we will use this fact to define a sphere or ellipse for some
    // projections

    // Frank - may need to talk this over
    bool bIsGeographic = true;
    if (EQUAL( poDS->GetKeyword("IsisCube.Mapping.LatitudeType"),
               "Planetocentric" ))
        bIsGeographic = false;

    //Set oSRS projection and parameters
    //############################################################
    //ISIS3 Projection types
    //  Equirectangular
    //  LambertConformal
    //  Mercator
    //  ObliqueCylindrical //Todo
    //  Orthographic
    //  PolarStereographic
    //  SimpleCylindrical
    //  Sinusoidal
    //  TransverseMercator

#ifdef DEBUG
    CPLDebug( "ISIS3", "using projection %s", map_proj_name);
#endif

    OGRSpatialReference oSRS;
    bool bProjectionSet = true;

    if ((EQUAL( map_proj_name, "Equirectangular" )) ||
        (EQUAL( map_proj_name, "SimpleCylindrical" )) )  {
        oSRS.OGRSpatialReference::SetEquirectangular2 ( 0.0, center_lon, center_lat, 0, 0 );
    } else if (EQUAL( map_proj_name, "Orthographic" )) {
        oSRS.OGRSpatialReference::SetOrthographic ( center_lat, center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "Sinusoidal" )) {
        oSRS.OGRSpatialReference::SetSinusoidal ( center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "Mercator" )) {
        oSRS.OGRSpatialReference::SetMercator ( center_lat, center_lon, scaleFactor, 0, 0 );
    } else if (EQUAL( map_proj_name, "PolarStereographic" )) {
        oSRS.OGRSpatialReference::SetPS ( center_lat, center_lon, scaleFactor, 0, 0 );
    } else if (EQUAL( map_proj_name, "TransverseMercator" )) {
        oSRS.OGRSpatialReference::SetTM ( center_lat, center_lon, scaleFactor, 0, 0 );
    } else if (EQUAL( map_proj_name, "LambertConformal" )) {
        oSRS.OGRSpatialReference::SetLCC ( first_std_parallel, second_std_parallel, center_lat, center_lon, 0, 0 );
    } else {
        CPLDebug( "ISIS3",
                  "Dataset projection %s is not supported. Continuing...",
                  map_proj_name );
        bProjectionSet = false;
    }

    if (bProjectionSet) {
        //Create projection name, i.e. MERCATOR MARS and set as ProjCS keyword
        CPLString osProjTargetName(map_proj_name);
        osProjTargetName += " ";
        osProjTargetName += target_name;
        oSRS.SetProjCS(osProjTargetName); //set ProjCS keyword

        //The geographic/geocentric name will be the same basic name as the body name
        //'GCS' = Geographic/Geocentric Coordinate System
        CPLString osGeogName("GCS_");
        osGeogName += target_name;

        //The datum name will be the same basic name as the planet
        CPLString osDatumName("D_");
        osDatumName += target_name;

        CPLString osSphereName(target_name);
        //strcat(osSphereName, "_IAU_IAG");  //Might not be IAU defined so don't add

        //calculate inverse flattening from major and minor axis: 1/f = a/(a-b)
        double iflattening = 0.0;
        if ((semi_major - semi_minor) < 0.0000001)
           iflattening = 0;
        else
           iflattening = semi_major / (semi_major - semi_minor);

        //Set the body size but take into consideration which proj is being used to help w/ proj4 compatibility
        //The use of a Sphere, polar radius or ellipse here is based on how ISIS does it internally
        if ( ( (EQUAL( map_proj_name, "Stereographic" ) && (fabs(center_lat) == 90)) ) ||
             (EQUAL( map_proj_name, "PolarStereographic" )) )
         {
            if (bIsGeographic) {
                //Geograpraphic, so set an ellipse
                oSRS.SetGeogCS( osGeogName, osDatumName, osSphereName,
                                semi_major, iflattening,
                               "Reference_Meridian", 0.0 );
            } else {
              //Geocentric, so force a sphere using the semi-minor axis. I hope...
              osSphereName += "_polarRadius";
              oSRS.SetGeogCS( osGeogName, osDatumName, osSphereName,
                              semi_minor, 0.0,
                              "Reference_Meridian", 0.0 );
            }
        }
        else if ( (EQUAL( map_proj_name, "SimpleCylindrical" )) ||
                  (EQUAL( map_proj_name, "Orthographic" )) ||
                  (EQUAL( map_proj_name, "Stereographic" )) ||
                  (EQUAL( map_proj_name, "Sinusoidal" )) ) {
            // ISIS uses the spherical equation for these projections
            // so force a sphere.
            oSRS.SetGeogCS( osGeogName, osDatumName, osSphereName,
                            semi_major, 0.0,
                            "Reference_Meridian", 0.0 );
        }
        else if  (EQUAL( map_proj_name, "Equirectangular" )) {
            //Calculate localRadius using ISIS3 simple elliptical method
            //  not the more standard Radius of Curvature method
            //PI = 4 * atan(1);
            const double radLat = center_lat * M_PI / 180;  // in radians
            const double localRadius
                = semi_major * semi_minor
                / sqrt( pow( semi_minor * cos( radLat ), 2)
                       + pow( semi_major * sin( radLat ), 2) );
            osSphereName += "_localRadius";
            oSRS.SetGeogCS( osGeogName, osDatumName, osSphereName,
                            localRadius, 0.0,
                            "Reference_Meridian", 0.0 );
        }
        else {
            //All other projections: Mercator, Transverse Mercator, Lambert Conformal, etc.
            //Geographic, so set an ellipse
            if (bIsGeographic) {
                oSRS.SetGeogCS( osGeogName, osDatumName, osSphereName,
                                semi_major, iflattening,
                                "Reference_Meridian", 0.0 );
            } else {
                //Geocentric, so force a sphere. I hope...
                oSRS.SetGeogCS( osGeogName, osDatumName, osSphereName,
                                semi_major, 0.0,
                                "Reference_Meridian", 0.0 );
            }
        }

        // translate back into a projection string.
        char *pszResult = NULL;
        oSRS.exportToWkt( &pszResult );
        poDS->m_osProjection = pszResult;
        CPLFree( pszResult );
    }

/* END ISIS3 Label Read */
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/* -------------------------------------------------------------------- */
/*      Did we get the required keywords?  If not we return with        */
/*      this never having been considered to be a match. This isn't     */
/*      an error!                                                       */
/* -------------------------------------------------------------------- */
    if( !GDALCheckDatasetDimensions(nCols, nRows) ||
        !GDALCheckBandCount(nBands, false) )
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = nCols;
    poDS->nRasterYSize = nRows;

/* -------------------------------------------------------------------- */
/*      Open target binary file.                                        */
/* -------------------------------------------------------------------- */
    if( EQUAL(osFormat,"GeoTIFF") )
    {
        if( nSkipBytes != 0 )
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Ignoring StartByte=%d for format=GeoTIFF",
                     1+nSkipBytes);
        }
        poDS->m_poExternalDS = reinterpret_cast<GDALDataset *>(
                                GDALOpen( osQubeFile, poOpenInfo->eAccess ) );
        if( poDS->m_poExternalDS == NULL )
        {
            delete poDS;
            return NULL;
        }
        if( poDS->m_poExternalDS->GetRasterXSize() != poDS->nRasterXSize ||
            poDS->m_poExternalDS->GetRasterYSize() != poDS->nRasterYSize ||
            poDS->m_poExternalDS->GetRasterCount() != nBands ||
            poDS->m_poExternalDS->GetRasterBand(1)->GetRasterDataType() != 
                                                                    eDataType )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s has incompatible characteristics with the ones "
                      "declared in the label.",
                      osQubeFile.c_str() );
            delete poDS;
            return NULL;
        }
    }
    else
    {
        if( poOpenInfo->eAccess == GA_ReadOnly )
            poDS->m_fpImage = VSIFOpenL( osQubeFile, "r" );
        else
            poDS->m_fpImage = VSIFOpenL( osQubeFile, "r+" );

        if( poDS->m_fpImage == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                    "Failed to open %s: %s.",
                    osQubeFile.c_str(),
                    VSIStrerror( errno ) );
            delete poDS;
            return NULL;
        }

        // Sanity checks in case the external raw file appears to be a
        // TIFF file
        if( EQUAL(CPLGetExtension(osQubeFile), "tif") )
        {
            GDALDataset* poTIF_DS = reinterpret_cast<GDALDataset*>(
                GDALOpen(osQubeFile, GA_ReadOnly));
            if( poTIF_DS )
            {
                bool bWarned = false;
                if( poTIF_DS->GetRasterXSize() != poDS->nRasterXSize ||
                    poTIF_DS->GetRasterYSize() != poDS->nRasterYSize ||
                    poTIF_DS->GetRasterCount() != nBands ||
                    poTIF_DS->GetRasterBand(1)->GetRasterDataType() != 
                                                                eDataType ||
                    poTIF_DS->GetMetadataItem("COMPRESSION",
                                              "IMAGE_STRUCTURE") != NULL )
                {
                    bWarned = true;
                    CPLError( CE_Warning, CPLE_AppDefined,
                        "%s has incompatible characteristics with the ones "
                        "declared in the label.",
                        osQubeFile.c_str() );
                }
                int nBlockXSize = 1, nBlockYSize = 1;
                poTIF_DS->GetRasterBand(1)->GetBlockSize(&nBlockXSize,
                                                         &nBlockYSize);
                if( (poDS->m_bIsTiled && (nBlockXSize != tileSizeX ||
                                          nBlockYSize != tileSizeY) ) ||
                    (!poDS->m_bIsTiled && (nBlockXSize != nCols ||
                                        (nBands > 1 && nBlockYSize != 1))) )
                {
                    if( !bWarned )
                    {
                        bWarned = true;
                        CPLError( CE_Warning, CPLE_AppDefined,
                            "%s has incompatible characteristics with the ones "
                            "declared in the label.",
                            osQubeFile.c_str() );
                    }
                }

                // Check that blocks are effectively written in expected order.
                const int nBlockSizeBytes = nBlockXSize * nBlockYSize *
                                        GDALGetDataTypeSizeBytes(eDataType);
                bool bGoOn = !bWarned;
                const int l_nBlocksPerRow =
                        DIV_ROUND_UP(nCols, nBlockXSize);
                const int l_nBlocksPerColumn =
                        DIV_ROUND_UP(nRows, nBlockYSize);
                int nBlockNo = 0;
                for( int i = 0; i < nBands && bGoOn; i++ )
                {
                    for( int y = 0; y < l_nBlocksPerColumn && bGoOn; y++ )
                    {
                        for( int x = 0; x < l_nBlocksPerRow && bGoOn; x++ )
                        {
                            const char* pszBlockOffset =  poTIF_DS->
                                GetRasterBand(i+1)->GetMetadataItem(
                                    CPLSPrintf("BLOCK_OFFSET_%d_%d", x, y),
                                    "TIFF");
                            if( pszBlockOffset )
                            {
                                GIntBig nOffset = CPLAtoGIntBig(pszBlockOffset);
                                if( nOffset != nSkipBytes + nBlockNo *
                                                            nBlockSizeBytes )
                                {
                                    bWarned = true;
                                    CPLError( CE_Warning, CPLE_AppDefined,
                                        "%s has incompatible "
                                        "characteristics with the ones "
                                        "declared in the label.",
                                        osQubeFile.c_str() );
                                    bGoOn = false;
                                }
                            }
                            nBlockNo ++;
                        }
                    }
                }

                delete poTIF_DS;
            }
        }
    }

    poDS->eAccess = poOpenInfo->eAccess;

/* -------------------------------------------------------------------- */
/*      Compute the line offset.                                        */
/* -------------------------------------------------------------------- */
    int nLineOffset = 0;
    int nPixelOffset = 0;
    vsi_l_offset nBandOffset = 0;

    if( EQUAL(osFormat,"BandSequential") )
    {
        const int nItemSize = GDALGetDataTypeSizeBytes(eDataType);
        nPixelOffset = nItemSize;
        nLineOffset = nPixelOffset * nCols;
        nBandOffset = static_cast<vsi_l_offset>(nLineOffset) * nRows;
    }
    /* else Tiled or external */

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
#ifdef CPL_LSB
    const bool bNativeOrder = bIsLSB;
#else
    const bool bNativeOrder = !bIsLSB;
#endif

    for( int i = 0; i < nBands; i++ )
    {
        GDALRasterBand *poBand = NULL;

        if( poDS->m_poExternalDS != NULL )
        {
            ISIS3WrapperRasterBand* poISISBand =
                new ISIS3WrapperRasterBand(
                            poDS->m_poExternalDS->GetRasterBand( i+1 ) );
            poBand = poISISBand;
            poDS->SetBand( i+1, poBand );

            poISISBand->SetMaskBand( new ISISMaskBand(poISISBand) );
        }
        else if( poDS->m_bIsTiled )
        {
            ISISTiledBand* poISISBand =
                new ISISTiledBand( poDS, poDS->m_fpImage, i+1, eDataType,
                                        tileSizeX, tileSizeY,
                                        nSkipBytes, 0, 0,
                                        bNativeOrder );

            poBand = poISISBand;
            poDS->SetBand( i+1, poBand );

            poISISBand->SetMaskBand( new ISISMaskBand(poISISBand) );
        }
        else
        {
            ISIS3RawRasterBand* poISISBand =
                new ISIS3RawRasterBand( poDS, i+1, poDS->m_fpImage,
                                   nSkipBytes + nBandOffset * i,
                                   nPixelOffset, nLineOffset, eDataType,
                                   bNativeOrder,
                                   TRUE );

            poBand = poISISBand;
            poDS->SetBand( i+1, poBand );

            poISISBand->SetMaskBand( new ISISMaskBand(poISISBand) );
        }

        poBand->SetNoDataValue( dfNoData );

        // Set offset/scale values.
        const double dfOffset =
            CPLAtofM(poDS->GetKeyword("IsisCube.Core.Pixels.Base","0.0"));
        const double dfScale =
            CPLAtofM(poDS->GetKeyword("IsisCube.Core.Pixels.Multiplier","1.0"));
        if( dfOffset != 0.0 || dfScale != 1.0 )
        {
            poBand->SetOffset(dfOffset);
            poBand->SetScale(dfScale);
        }
    }

/* -------------------------------------------------------------------- */
/*      Check for a .prj file. For ISIS3 I would like to keep this in   */
/* -------------------------------------------------------------------- */
    const CPLString osPath = CPLGetPath( poOpenInfo->pszFilename );
    const CPLString osName = CPLGetBasename(poOpenInfo->pszFilename);
    const char  *pszPrjFile = CPLFormCIFilename( osPath, osName, "prj" );

    VSILFILE *fp = VSIFOpenL( pszPrjFile, "r" );
    if( fp != NULL )
    {
        VSIFCloseL( fp );

        char **papszLines = CSLLoad( pszPrjFile );

        OGRSpatialReference oSRS2;
        if( oSRS2.importFromESRI( papszLines ) == OGRERR_NONE )
        {
            poDS->m_aosAdditionalFiles.AddString( pszPrjFile );
            char *pszResult = NULL;
            oSRS2.exportToWkt( &pszResult );
            poDS->m_osProjection = pszResult;
            CPLFree( pszResult );
        }

        CSLDestroy( papszLines );
    }

    if( dfULXMap != 0.5 || dfULYMap != 0.5 || dfXDim != 1.0 || dfYDim != 1.0 )
    {
        poDS->m_bGotTransform = true;
        poDS->m_adfGeoTransform[0] = dfULXMap;
        poDS->m_adfGeoTransform[1] = dfXDim;
        poDS->m_adfGeoTransform[2] = 0.0;
        poDS->m_adfGeoTransform[3] = dfULYMap;
        poDS->m_adfGeoTransform[4] = 0.0;
        poDS->m_adfGeoTransform[5] = dfYDim;
    }

    if( !poDS->m_bGotTransform )
    {
        poDS->m_bGotTransform =
            CPL_TO_BOOL(GDALReadWorldFile( poOpenInfo->pszFilename, "cbw",
                               poDS->m_adfGeoTransform ));
        if( poDS->m_bGotTransform )
        {
            poDS->m_aosAdditionalFiles.AddString(
                        CPLResetExtension(poOpenInfo->pszFilename, "cbw") );
        }
    }

    if( !poDS->m_bGotTransform )
    {
        poDS->m_bGotTransform =
            CPL_TO_BOOL(GDALReadWorldFile( poOpenInfo->pszFilename, "wld",
                               poDS->m_adfGeoTransform ));
        if( poDS->m_bGotTransform )
        {
            poDS->m_aosAdditionalFiles.AddString(
                        CPLResetExtension(poOpenInfo->pszFilename, "wld") );
        }
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return poDS;
}

/************************************************************************/
/*                             GetKeyword()                             */
/************************************************************************/

const char *ISIS3Dataset::GetKeyword( const char *pszPath,
                                      const char *pszDefault )

{
    return m_oKeywords.GetKeyword( pszPath, pszDefault );
}

/************************************************************************/
/*                           BuildLabel()                               */
/************************************************************************/

json_object* ISIS3Dataset::BuildLabel()
{
    // If we have a source label, then edit it directly
    CPLJsonObject oLabel( m_poSrcJSonLabel );
    if( m_poSrcJSonLabel == NULL )
        oLabel.clear();

    if( oLabel.getType() == CPLJsonObject::OBJECT && oLabel.has("Label") )
    {
        CPLJsonObject& oLabelLabel = oLabel["Label"];
        if( oLabelLabel.getType() == CPLJsonObject::OBJECT &&
            oLabelLabel.has("Bytes") )
        {
            oLabelLabel["Bytes"] = pszLABEL_BYTES_PLACEHOLDER;
        }
    }

    CPLJsonObject& oIsisCube = oLabel["IsisCube"];
    if( oIsisCube.getType() != CPLJsonObject::OBJECT )
        oIsisCube.clear();
    oIsisCube["_type"] = "object";

    if( !m_osComment.empty() )
        oIsisCube.insert(0, "_comment", CPLJsonObject(m_osComment));

    CPLJsonObject& oCore = oIsisCube["Core"];
    if( oCore.getType() != CPLJsonObject::OBJECT )
        oCore.clear();
    oCore["_type"] = "object";

    if( !m_osExternalFilename.empty() )
    {
        if( m_poExternalDS && m_bGeoTIFFAsRegularExternal )
        {
            if( !m_bGeoTIFFInitDone )
            {
                reinterpret_cast<ISIS3WrapperRasterBand*>(GetRasterBand(1))->
                    InitFile();
            }

            const char* pszOffset = m_poExternalDS->GetRasterBand(1)->
                                GetMetadataItem("BLOCK_OFFSET_0_0", "TIFF");
            if( pszOffset )
            {
                oCore["StartByte"] = 1 + atoi(pszOffset);
            }
            else
            {
                // Shouldn't happen normally
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Missing BLOCK_OFFSET_0_0");
                m_bGeoTIFFAsRegularExternal = false;
                oCore["StartByte"] = 1;
            }
        }
        else
        {
            oCore["StartByte"] = 1;
        }
        oCore["^Core"] = CPLGetFilename(m_osExternalFilename);
    }
    else
    {
        oCore["StartByte"] = pszSTARTBYTE_PLACEHOLDER;
        oCore.del("^Core");
    }

    if( m_poExternalDS && !m_bGeoTIFFAsRegularExternal )
    {
        oCore["Format"] = "GeoTIFF";
        oCore.del("TileSamples");
        oCore.del("TileLines");
    }
    else if( m_bIsTiled )
    {
        oCore["Format"] = "Tile";
        int nBlockXSize = 1, nBlockYSize = 1;
        GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);
        oCore["TileSamples"] = nBlockXSize;
        oCore["TileLines"] = nBlockYSize;
    }
    else
    {
        oCore["Format"] = "BandSequential";
        oCore.del("TileSamples");
        oCore.del("TileLines");
    }

    CPLJsonObject& oDimensions = oCore["Dimensions"];
    if( oDimensions.getType() != CPLJsonObject::OBJECT )
        oDimensions.clear();
    oDimensions["_type"] = "group";
    oDimensions["Samples"] = nRasterXSize;
    oDimensions["Lines"] = nRasterYSize;
    oDimensions["Bands"] = nBands;

    CPLJsonObject& oPixels = oCore["Pixels"];
    if( oPixels.getType() != CPLJsonObject::OBJECT )
        oPixels.clear();
    oPixels["_type"] = "group";
        const GDALDataType eDT = GetRasterBand(1)->GetRasterDataType();
    oPixels["Type"] =
        (eDT == GDT_Byte) ?   "UnsignedByte" :
        (eDT == GDT_UInt16) ? "UnsignedWord" :
        (eDT == GDT_Int16) ?  "SignedWord" :
                                "Real";

    oPixels["ByteOrder"] = "Lsb";
    oPixels["Base"] = GetRasterBand(1)->GetOffset();
    oPixels["Multiplier"] = GetRasterBand(1)->GetScale();

    OGRSpatialReference oSRS;

    if( !m_bUseSrcMapping && oIsisCube.has("Mapping") )
        oIsisCube["Mapping"].clear();

    if( !m_bUseSrcMapping && !m_osProjection.empty() )
    {
        CPLJsonObject& oMapping = oIsisCube["Mapping"];
        oMapping["_type"] = "group";

        oSRS.SetFromUserInput(m_osProjection);
        if( oSRS.IsProjected() )
        {
            const char* pszDatum = oSRS.GetAttrValue("DATUM");
            CPLString osTargetName;
            if( pszDatum && STARTS_WITH(pszDatum, "D_") )
            {
                osTargetName = pszDatum + 2;
            }
            else if( pszDatum )
            {
                osTargetName = pszDatum;
            }
            if( !osTargetName.empty() )
                oMapping["TargetName"] = osTargetName;

            oMapping["EquatorialRadius"]["value"] = oSRS.GetSemiMajor();
            oMapping["EquatorialRadius"]["unit"] = "meters";
            oMapping["PolarRadius"]["value"] = oSRS.GetSemiMinor();
            oMapping["PolarRadius"]["unit"] = "meters";

            if( !m_osLatitudeType.empty() )
                oMapping["LatitudeType"] = m_osLatitudeType;
            else
                oMapping["LatitudeType"] = "Planetographic";

            if( !m_osLongitudeDirection.empty() )
                oMapping["LongitudeDirection"] = m_osLongitudeDirection;
            else
                oMapping["LongitudeDirection"] = "PositiveEast";

            double adfX[4] = {0};
            double adfY[4] = {0};
            bool bLongLatCorners = false;
            if( m_bGotTransform )
            {
                OGRSpatialReference* poSRSLongLat = oSRS.CloneGeogCS();
                OGRCoordinateTransformation* poCT =
                    OGRCreateCoordinateTransformation(&oSRS, poSRSLongLat);
                if( poCT )
                {
                    for( int i = 0; i < 4; i++ )
                    {
                        adfX[i] = m_adfGeoTransform[0] + (i%2) *
                                        nRasterXSize * m_adfGeoTransform[1];
                        adfY[i] = m_adfGeoTransform[3] +
                                ( (i == 0 || i == 3) ? 0 : 1 ) *
                                nRasterYSize * m_adfGeoTransform[5];
                    }
                    if( poCT->Transform(4, adfX, adfY) )
                    {
                        bLongLatCorners = true;
                    }
                    delete poCT;
                }
                delete poSRSLongLat;
            }

            if( !m_osLongitudeDomain.empty() )
                oMapping["LongitudeDomain"] = atoi(m_osLongitudeDomain);
            else
            {
                if( bLongLatCorners && adfX[3] > 180.0 )
                    oMapping["LongitudeDomain"] = 360;
                else
                    oMapping["LongitudeDomain"] = 180;
            }

            if( m_bWriteBoundingDegrees && !m_osBoundingDegrees.empty() )
            {
                char** papszTokens =
                        CSLTokenizeString2(m_osBoundingDegrees, ",", 0);
                if( CSLCount(papszTokens) == 4 )
                {
                    oMapping["MinimumLatitude"]  = CPLAtof(papszTokens[1]);
                    oMapping["MinimumLongitude"] = CPLAtof(papszTokens[0]);
                    oMapping["MaximumLatitude"]  = CPLAtof(papszTokens[3]);
                    oMapping["MaximumLongitude"] = CPLAtof(papszTokens[2]);
                }
                CSLDestroy(papszTokens);
            }
            else if( m_bWriteBoundingDegrees && bLongLatCorners )
            {
                oMapping["MinimumLatitude"] = std::min(
                    std::min(adfY[0], adfY[1]), std::min(adfY[2],adfY[3]));
                oMapping["MinimumLongitude"] = std::min(
                    std::min(adfX[0], adfX[1]), std::min(adfX[2],adfX[3]));
                oMapping["MaximumLatitude"] = std::max(
                    std::max(adfY[0], adfY[1]), std::max(adfY[2],adfY[3]));
                oMapping["MaximumLongitude"] = std::max(
                    std::max(adfX[0], adfX[1]), std::max(adfX[2],adfX[3]));
            }

            const char* pszProjection = oSRS.GetAttrValue("PROJECTION");
            if( EQUAL(pszProjection, SRS_PT_EQUIRECTANGULAR) )
            {
                oMapping["ProjectionName"] = "Equirectangular";
                if( oSRS.GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 )
                                                                    != 0.0 )
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                             "Ignoring %s. Only 0 value supported",
                             SRS_PP_LATITUDE_OF_ORIGIN);
                }
                oMapping["CenterLongitude"] =
                    oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
                const double dfCenterLat =
                        oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, 0.0);
                oMapping["CenterLatitude"] = dfCenterLat;

                  // in radians
                const double radLat = dfCenterLat * M_PI / 180;
                const double semi_major = oSRS.GetSemiMajor();
                const double semi_minor = oSRS.GetSemiMinor();
                const double localRadius
                        = semi_major * semi_minor
                        / sqrt( pow( semi_minor * cos( radLat ), 2)
                            + pow( semi_major * sin( radLat ), 2) );
                oMapping["CenterLatitudeRadius"] = localRadius;
            }

            else if( EQUAL(pszProjection, SRS_PT_ORTHOGRAPHIC) )
            {
                oMapping["ProjectionName"] = "Orthographic";
                oMapping["CenterLongitude"] =
                        oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
                oMapping["CenterLatitude"] =
                        oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0);
            }

            else if( EQUAL(pszProjection, SRS_PT_SINUSOIDAL) )
            {
                oMapping["ProjectionName"] = "Sinusoidal";
                oMapping["CenterLongitude"] =
                        oSRS.GetNormProjParm(SRS_PP_LONGITUDE_OF_CENTER, 0.0);
            }

            else if( EQUAL(pszProjection, SRS_PT_MERCATOR_1SP) )
            {
                oMapping["ProjectionName"] = "Mercator";
                oMapping["CenterLongitude"] =
                        oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
                oMapping["CenterLatitude"] =
                        oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0);
                oMapping["scaleFactor"] =
                        oSRS.GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0);
            }

            else if( EQUAL(pszProjection, SRS_PT_POLAR_STEREOGRAPHIC) )
            {
                oMapping["ProjectionName"] = "PolarStereographic";
                oMapping["CenterLongitude"] =
                        oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
                oMapping["CenterLatitude"] =
                        oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0);
                oMapping["scaleFactor"] =
                        oSRS.GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0);
            }

            else if( EQUAL(pszProjection, SRS_PT_TRANSVERSE_MERCATOR) )
            {
                oMapping["ProjectionName"] = "TransverseMercator";
                oMapping["CenterLongitude"] =
                        oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
                oMapping["CenterLatitude"] =
                        oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0);
                oMapping["scaleFactor"] =
                        oSRS.GetNormProjParm(SRS_PP_SCALE_FACTOR, 1.0);
            }

            else if( EQUAL(pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) )
            {
                oMapping["ProjectionName"] = "LambertConformal";
                oMapping["CenterLongitude"] =
                        oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0);
                oMapping["CenterLatitude"] =
                        oSRS.GetNormProjParm(SRS_PP_LATITUDE_OF_ORIGIN, 0.0);
                oMapping["FirstStandardParallel"] =
                        oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, 0.0);
                oMapping["SecondStandardParallel"] =
                        oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_2, 0.0);
            }

            else
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                         "Projection %s not supported",
                         pszProjection);
            }

            if( oMapping.has("ProjectionName") )
            {
                if( oSRS.GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 ) != 0.0 )
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                             "Ignoring %s. Only 0 value supported",
                             SRS_PP_FALSE_EASTING);
                }
                if( oSRS.GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 ) != 0.0 )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                            "Ignoring %s. Only 0 value supported",
                             SRS_PP_FALSE_NORTHING);
                }
            }
        }
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Geodetic SRS not supported");
        }
    }

    if( !m_bUseSrcMapping && m_bGotTransform )
    {
        CPLJsonObject& oMapping = oIsisCube["Mapping"];
        oMapping["_type"] = "group";

        oMapping["UpperLeftCornerX"] = m_adfGeoTransform[0];
        oMapping["UpperLeftCornerY"] = m_adfGeoTransform[3];
        if( !m_osProjection.empty() && oSRS.IsProjected() )
        {
            const double dfLinearUnits = oSRS.GetLinearUnits();
            // Maybe we should deal differently with non meter units ?
            const double dfRes = m_adfGeoTransform[1] * dfLinearUnits;
            oMapping["PixelResolution"]["value"] = dfRes;
            oMapping["PixelResolution"]["unit"] = "meters/pixel";
            oMapping["Scale"]["value"] =
                oSRS.GetSemiMajor() * M_PI / 180. / dfRes;
            oMapping["Scale"]["unit"] = "pixels/degree";
        }
        else
        {
            oMapping["PixelResolution"] = m_adfGeoTransform[1];
        }
    }

    return oLabel.asLibJsonObj();
}

/************************************************************************/
/*                           WriteLabel()                               */
/************************************************************************/

void ISIS3Dataset::WriteLabel()
{
    m_bIsLabelWritten = true;

    if( m_poJSonLabel == NULL )
        m_poJSonLabel = BuildLabel();

    // Serialize label into temporary file
    CPLString osTmpFile( CPLSPrintf("/vsimem/isis3_%p", this) );
    VSILFILE* fpTmp = VSIFOpenL( osTmpFile, "wb+" );
    SerializeAsPDL( fpTmp, m_poJSonLabel );
    VSIFCloseL( fpTmp );
    char *pszLabel = reinterpret_cast<char*>(
                            VSIGetMemFileBuffer( osTmpFile, NULL, TRUE ));
    const int nLabelSize = static_cast<int>(strlen(pszLabel));

    // Hack back StartByte value
    char *pszStartByte = strstr(pszLabel, pszSTARTBYTE_PLACEHOLDER);
    if( pszStartByte != NULL )
    {
        const char* pszOffset = CPLSPrintf("%d", 1 + nLabelSize);
        memcpy(pszStartByte, pszOffset, strlen(pszOffset));
        memset(pszStartByte + strlen(pszOffset), ' ',
                strlen(pszSTARTBYTE_PLACEHOLDER) - strlen(pszOffset));
    }

    // Hack back Label.Bytes value
    char* pszLabelBytes = strstr(pszLabel, pszLABEL_BYTES_PLACEHOLDER);
    if( pszLabelBytes != NULL )
    {
        const char* pszBytes = CPLSPrintf("%d", nLabelSize);
        memcpy(pszLabelBytes, pszBytes, strlen(pszBytes));
        memset(pszLabelBytes + strlen(pszBytes), ' ',
                strlen(pszLABEL_BYTES_PLACEHOLDER) - strlen(pszBytes));
    }

    VSIUnlink(osTmpFile);

    // Write to final file
    VSIFSeekL( m_fpLabel, 0, SEEK_SET );
    VSIFWriteL( pszLabel, 1, strlen(pszLabel), m_fpLabel);
    VSIFree(pszLabel);

    if( m_osExternalFilename.empty() )
    {
        // Update image offset in bands
        if( m_bIsTiled )
        {
            for(int i=0;i<nBands;i++)
            {
                ISISTiledBand* poBand =
                    reinterpret_cast<ISISTiledBand*>(GetRasterBand(i+1));
                poBand->m_nFirstTileOffset += nLabelSize;
            }
        }
        else
        {
            for(int i=0;i<nBands;i++)
            {
                ISIS3RawRasterBand* poBand =
                    reinterpret_cast<ISIS3RawRasterBand*>(GetRasterBand(i+1));
                poBand->nImgOffset += nLabelSize;
            }
        }
    }

    if( m_bInitToNodata && m_poExternalDS == NULL )
    {
        // Initialize the image to nodata
        const GDALDataType eType = GetRasterBand(1)->GetRasterDataType();
        const int nDTSize = GDALGetDataTypeSizeBytes(eType);

        vsi_l_offset nPixelsToInit;
        if( m_bIsTiled )
        {
            int nBlockXSize = 1, nBlockYSize = 1;
            GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);
            nPixelsToInit = static_cast<vsi_l_offset>(nBlockXSize) *
                            nBlockYSize * nBands *
                            DIV_ROUND_UP(nRasterXSize, nBlockXSize) *
                            DIV_ROUND_UP(nRasterYSize, nBlockYSize);
        }
        else
        {
            nPixelsToInit = static_cast<vsi_l_offset>(nRasterXSize) *
                            nRasterYSize * nBands;
        }

        const double dfNoData = GetRasterBand(1)->GetNoDataValue();
        if( dfNoData == 0.0 )
        {
            VSIFTruncateL( m_fpImage, VSIFTellL(m_fpImage) + 
                                                nPixelsToInit * nDTSize );
        }
        else
        {
            const int nPageSize = 4096; // Must be multiple of 4 since
                                        // Float32 is the largest type
            CPLAssert( (nPageSize % nDTSize) == 0 );
            const int nMaxPerPage = nPageSize / nDTSize;
            GByte* pabyTemp = static_cast<GByte*>(CPLMalloc(nPageSize));
            GDALCopyWords( &dfNoData, GDT_Float64, 0,
                           pabyTemp, eType, nDTSize,
                           nMaxPerPage );
#ifdef CPL_MSB
            GDALSwapWords( pabyTemp, nDTSize, nMaxPerPage, nDTSize );
#endif
            for( vsi_l_offset i = 0; i < nPixelsToInit; i += nMaxPerPage )
            {
                int n;
                if( i + nMaxPerPage <= nPixelsToInit )
                    n = nMaxPerPage;
                else
                    n = static_cast<int>(nPixelsToInit - i);
                if( VSIFWriteL( pabyTemp, n * nDTSize, 1, m_fpImage ) != 1 )
                {
                    CPLError(CE_Failure, CPLE_FileIO,
                            "Cannot initialize imagery to null");
                    break;
                }
            }

            CPLFree( pabyTemp );
        }
    }
}

/************************************************************************/
/*                      SerializeAsPDL()                                */
/************************************************************************/

void ISIS3Dataset::SerializeAsPDL( VSILFILE* fp, json_object* poObj,
                                   int nDepth )
{
    CPLString osIndentation;
    for( int i = 0; i < nDepth; i++ )
        osIndentation += "  ";
    const size_t WIDTH = 79;

    json_object_iter it;
    it.key = NULL;
    it.val = NULL;
    it.entry = NULL;
    size_t nMaxKeyLength = 0;
    json_object_object_foreachC( poObj, it )
    {
        if( it.val == NULL || strcmp(it.key, "_type") == 0 )
            continue;
        const int nValType = json_object_get_type(it.val);
        if( nValType == json_type_string ||
            nValType == json_type_int ||
            nValType == json_type_double ||
            nValType == json_type_array )
        {
            if( strlen(it.key) > nMaxKeyLength )
                nMaxKeyLength = strlen(it.key);
        }
        else if( nValType == json_type_object )
        {
            json_object* poValue = CPL_json_object_object_get(it.val,
                                                                  "value");
            json_object* poUnit = CPL_json_object_object_get(it.val,
                                                                "unit");
            if( poValue && poUnit &&
                json_object_get_type(poUnit) == json_type_string )
            {
                if( strlen(it.key) > nMaxKeyLength )
                    nMaxKeyLength = strlen(it.key);
            }
        }
    }

    it.key = NULL;
    it.val = NULL;
    it.entry = NULL;
    json_object_object_foreachC( poObj, it )
    {
        if( it.val == NULL || strcmp(it.key, "_type") == 0 )
            continue;
        if( STARTS_WITH(it.key, "_comment") )
        {
            if(json_object_get_type(it.val) == json_type_string )
            {
                VSIFPrintfL(fp, "#%s\n", 
                            json_object_get_string(it.val));
            }
            continue;
        }
        std::string osPadding;
        if( strlen(it.key) < nMaxKeyLength )
            osPadding.append( nMaxKeyLength - strlen(it.key), ' ' );
        const int nValType = json_object_get_type(it.val);
        if( nValType == json_type_object )
        {
            json_object* poType = CPL_json_object_object_get(it.val, "_type");
            if( poType && json_object_get_type(poType) == json_type_string )
            {
                const char* pszType = json_object_get_string(poType);
                if( EQUAL(pszType, "Object") )
                {
                    VSIFPrintfL(fp, "%sObject = %s\n",
                                osIndentation.c_str(), it.key);
                    SerializeAsPDL( fp, it.val, nDepth + 1 );
                    VSIFPrintfL(fp, "%sEnd_Object\n",
                                osIndentation.c_str());
                }
                else if( EQUAL(pszType, "Group") )
                {
                    VSIFPrintfL(fp, "%sGroup = %s\n",
                                osIndentation.c_str(), it.key);
                    SerializeAsPDL( fp, it.val, nDepth + 1 );
                    VSIFPrintfL(fp, "%sEnd_Group\n",
                                osIndentation.c_str());
                }
            }
            else
            {
                json_object* poValue = CPL_json_object_object_get(it.val,
                                                                  "value");
                json_object* poUnit = CPL_json_object_object_get(it.val,
                                                                 "unit");
                if( poValue && poUnit &&
                    json_object_get_type(poUnit) == json_type_string )
                {
                    const char* pszUnit = json_object_get_string(poUnit);
                    const int nValueType = json_object_get_type(poValue);
                    if( nValueType == json_type_int )
                    {
                        const int nVal = json_object_get_int(poValue);
                        VSIFPrintfL(fp, "%s%s%s = %d <%s>\n",
                                    osIndentation.c_str(), it.key,
                                    osPadding.c_str(),
                                    nVal, pszUnit);
                    }
                    else if( nValueType == json_type_double )
                    {
                        const double dfVal = json_object_get_double(poValue);
                        if( dfVal >= INT_MIN && dfVal <= INT_MAX &&
                            static_cast<int>(dfVal) == dfVal )
                        {
                            VSIFPrintfL(fp, "%s%s%s = %d.0 <%s>\n",
                                        osIndentation.c_str(), it.key,
                                        osPadding.c_str(),
                                        static_cast<int>(dfVal), pszUnit);
                        }
                        else
                        {
                            VSIFPrintfL(fp, "%s%s%s = %.18g <%s>\n",
                                        osIndentation.c_str(), it.key,
                                        osPadding.c_str(),
                                        dfVal, pszUnit);
                        }
                    }
                }
            }
        }
        else if( nValType == json_type_string )
        {
            const char* pszVal = json_object_get_string(it.val);
            if( pszVal[0] == '\0' ||
                strchr(pszVal, ' ') || strstr(pszVal, "\\n") ||
                strstr(pszVal, "\\r") )
            {
                CPLString osVal(pszVal);
                osVal.replaceAll("\\n", "\n");
                osVal.replaceAll("\\r", "\r");
                VSIFPrintfL(fp, "%s%s%s = \"%s\"\n",
                            osIndentation.c_str(), it.key,
                            osPadding.c_str(), osVal.c_str());
            }
            else
            {
                if( osIndentation.size() + strlen(it.key) + osPadding.size() +
                    strlen(" = ") + strlen(pszVal) > WIDTH &&
                    osIndentation.size() + strlen(it.key) + osPadding.size() +
                    strlen(" = ") < WIDTH )
                {
                    size_t nFirstPos = osIndentation.size() + strlen(it.key) +
                                     osPadding.size() + strlen(" = ");
                    VSIFPrintfL(fp, "%s%s%s = ",
                                osIndentation.c_str(), it.key,
                                osPadding.c_str());
                    size_t nCurPos = nFirstPos;
                    for( int i = 0; pszVal[i] != '\0'; i++ )
                    {
                        nCurPos ++;
                        if( nCurPos == WIDTH && pszVal[i+1] != '\0' )
                        {
                            VSIFPrintfL( fp, "-\n" );
                            for( size_t j=0;j<nFirstPos;j++ )
                            {
                                const char chSpace = ' ';
                                VSIFWriteL(&chSpace, 1, 1, fp);
                            }
                            nCurPos = nFirstPos + 1;
                        }
                        VSIFWriteL( &pszVal[i], 1, 1, fp );
                    }
                    VSIFPrintfL(fp, "\n");
                }
                else
                {
                    VSIFPrintfL(fp, "%s%s%s = %s\n",
                                osIndentation.c_str(), it.key,
                                osPadding.c_str(), pszVal);
                }
            }
        }
        else if( nValType == json_type_int )
        {
            const int nVal = json_object_get_int(it.val);
            VSIFPrintfL(fp, "%s%s%s = %d\n",
                        osIndentation.c_str(), it.key,
                        osPadding.c_str(), nVal);
        }
        else if( nValType == json_type_double )
        {
            const double dfVal = json_object_get_double(it.val);
            if( dfVal >= INT_MIN && dfVal <= INT_MAX &&
                static_cast<int>(dfVal) == dfVal )
            {
                VSIFPrintfL(fp, "%s%s%s = %d.0\n",
                        osIndentation.c_str(), it.key,
                        osPadding.c_str(), static_cast<int>(dfVal));
            }
            else
            {
                VSIFPrintfL(fp, "%s%s%s = %.18g\n",
                            osIndentation.c_str(), it.key,
                            osPadding.c_str(), dfVal);
            }
        }
        else if( nValType == json_type_array )
        {
            const int nLength = json_object_array_length(it.val);
            size_t nFirstPos = osIndentation.size() + strlen(it.key) +
                                     osPadding.size() + strlen(" = (");
            VSIFPrintfL(fp, "%s%s%s = (",
                        osIndentation.c_str(), it.key,
                        osPadding.c_str());
            size_t nCurPos = nFirstPos;
            for( int idx = 0; idx < nLength; idx++ )
            {
                json_object* poItem = json_object_array_get_idx(it.val, idx);
                const int nItemType = json_object_get_type(poItem);
                if( nItemType == json_type_string )
                {
                    const char* pszVal = json_object_get_string(poItem);
                    if( nFirstPos < WIDTH && nCurPos + strlen(pszVal) > WIDTH )
                    {
                        if( idx > 0 )
                        {
                            VSIFPrintfL( fp, "\n" );
                            for( size_t j=0;j<nFirstPos;j++ )
                            {
                                const char chSpace = ' ';
                                VSIFWriteL(&chSpace, 1, 1, fp);
                            }
                            nCurPos = nFirstPos;
                        }

                        for( int i = 0; pszVal[i] != '\0'; i++ )
                        {
                            nCurPos ++;
                            if( nCurPos == WIDTH && pszVal[i+1] != '\0' )
                            {
                                VSIFPrintfL( fp, "-\n" );
                                for( size_t j=0;j<nFirstPos;j++ )
                                {
                                    const char chSpace = ' ';
                                    VSIFWriteL(&chSpace, 1, 1, fp);
                                }
                                nCurPos = nFirstPos + 1;
                            }
                            VSIFWriteL( &pszVal[i], 1, 1, fp );
                        }
                    }
                    else
                    {
                        VSIFPrintfL( fp, "%s", pszVal );
                        nCurPos += strlen(pszVal);
                    }
                }
                else if( nItemType == json_type_int )
                {
                    const int nVal = json_object_get_int(poItem);
                    const char* pszVal = CPLSPrintf("%d", nVal);
                    const size_t nValLen = strlen(pszVal);
                    if( nFirstPos < WIDTH && idx > 0 &&
                        nCurPos + nValLen > WIDTH )
                    {
                        VSIFPrintfL( fp, "\n" );
                        for( size_t j=0;j<nFirstPos;j++ )
                        {
                            const char chSpace = ' ';
                            VSIFWriteL(&chSpace, 1, 1, fp);
                        }
                        nCurPos = nFirstPos;
                    }
                    VSIFPrintfL( fp, "%d", nVal );
                    nCurPos += nValLen;
                }
                else if( nItemType == json_type_double )
                {
                    const double dfVal = json_object_get_double(poItem);
                    CPLString osVal;
                    if( dfVal >= INT_MIN && dfVal <= INT_MAX &&
                        static_cast<int>(dfVal) == dfVal )
                    {
                        osVal = CPLSPrintf("%d.0", static_cast<int>(dfVal));
                    }
                    else
                    {
                        osVal = CPLSPrintf("%.18g", dfVal);
                    }
                    const size_t nValLen = osVal.size();
                    if( nFirstPos < WIDTH && idx > 0 &&
                        nCurPos + nValLen > WIDTH )
                    {
                        VSIFPrintfL( fp, "\n" );
                        for( size_t j=0;j<nFirstPos;j++ )
                        {
                            const char chSpace = ' ';
                            VSIFWriteL(&chSpace, 1, 1, fp);
                        }
                        nCurPos = nFirstPos;
                    }
                    VSIFPrintfL( fp, "%s", osVal.c_str() );
                    nCurPos += nValLen;
                }
                if( idx < nLength - 1 )
                {
                    VSIFPrintfL( fp, ", " );
                    nCurPos += 2;
                }
            }
            VSIFPrintfL(fp, ")\n" );
        }
    }

    if( nDepth == 0 )
        VSIFPrintfL(fp, "End\n");
}


/************************************************************************/
/*                           Create()                                   */
/************************************************************************/

GDALDataset *ISIS3Dataset::Create(const char* pszFilename,
                                  int nXSize, int nYSize, int nBands,
                                  GDALDataType eType,
                                  char** papszOptions)
{
    if( eType != GDT_Byte && eType != GDT_UInt16 && eType != GDT_Int16 &&
        eType != GDT_Float32 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported data type");
        return NULL;
    }
    if( nBands == 0 || nBands > 32767 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported band count");
        return NULL;
    }

    const char* pszDataLocation = CSLFetchNameValueDef(papszOptions,
                                                       "DATA_LOCATION",
                                                       "LABEL");
    const bool bIsTiled = CPLFetchBool(papszOptions, "TILED", false);
    const int nBlockXSize = atoi(CSLFetchNameValueDef(papszOptions,
                                                      "BLOCKXSIZE", "256"));
    const int nBlockYSize = atoi(CSLFetchNameValueDef(papszOptions,
                                                      "BLOCKYSIZE", "256"));
    if( !EQUAL(pszDataLocation, "LABEL") &&
        !EQUAL( CPLGetExtension(pszFilename), "LBL") )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                    "For DATA_LOCATION=%s, "
                    "the main filename should have a .lbl extension",
                    pszDataLocation);
        return NULL;
    }

    VSILFILE* fp = VSIFOpenExL(pszFilename, "wb", true);
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Cannot create %s: %s",
                  pszFilename, VSIGetLastErrorMsg() );
        return NULL;
    }
    VSILFILE* fpImage = NULL;
    CPLString osExternalFilename;
    GDALDataset* poExternalDS = NULL;
    bool bGeoTIFFAsRegularExternal = false;
    if( EQUAL(pszDataLocation, "EXTERNAL") )
    {
        osExternalFilename = CSLFetchNameValueDef(papszOptions,
                                        "EXTERNAL_FILENAME",
                                        CPLResetExtension(pszFilename, "cub"));
        fpImage = VSIFOpenExL(osExternalFilename, "wb", true);
        if( fpImage == NULL )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Cannot create %s: %s",
                      osExternalFilename.c_str(), VSIGetLastErrorMsg() );
            VSIFCloseL(fp);
            return NULL;
        }
    }
    else if( EQUAL(pszDataLocation, "GEOTIFF") )
    {
        osExternalFilename = CSLFetchNameValueDef(papszOptions,
                                        "EXTERNAL_FILENAME",
                                        CPLResetExtension(pszFilename, "tif"));
        GDALDriver* poDrv = static_cast<GDALDriver*>(
                                            GDALGetDriverByName("GTiff"));
        if( poDrv == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Cannot find GTiff driver" );
            VSIFCloseL(fp);
            return NULL;
        }
        char** papszGTiffOptions = NULL;
        papszGTiffOptions = CSLSetNameValue(papszGTiffOptions,
                                            "ENDIANNESS", "LITTLE");
        if( bIsTiled )
        {
            papszGTiffOptions = CSLSetNameValue(papszGTiffOptions,
                                                "TILED", "YES");
            papszGTiffOptions = CSLSetNameValue(papszGTiffOptions,
                                                "BLOCKXSIZE",
                                                CPLSPrintf("%d", nBlockXSize));
            papszGTiffOptions = CSLSetNameValue(papszGTiffOptions,
                                                "BLOCKYSIZE",
                                                CPLSPrintf("%d", nBlockYSize));
        }
        const char* pszGTiffOptions = CSLFetchNameValueDef(papszOptions,
                                                    "GEOTIFF_OPTIONS", "");
        char** papszTokens = CSLTokenizeString2( pszGTiffOptions, ",", 0 );
        for( int i = 0; papszTokens[i] != NULL; i++ )
        {
            papszGTiffOptions = CSLAddString(papszGTiffOptions,
                                             papszTokens[i]);
        }
        CSLDestroy(papszTokens);

        // If the user didn't specify any compression and
        // GEOTIFF_AS_REGULAR_EXTERNAL is set (or unspecified), then the
        // GeoTIFF file can be seen as a regular external raw file, provided
        // we make some provision on its organization.
        if( CSLFetchNameValue(papszGTiffOptions, "COMPRESS") == NULL &&
            CPLFetchBool(papszOptions, "GEOTIFF_AS_REGULAR_EXTERNAL", true) )
        {
            bGeoTIFFAsRegularExternal = true;
            papszGTiffOptions = CSLSetNameValue(papszGTiffOptions,
                                                "INTERLEAVE", "BAND");
            // Will make sure that our blocks at nodata are not optimized
            // away but indeed well written
            papszGTiffOptions = CSLSetNameValue(papszGTiffOptions,
                                    "@WRITE_EMPTY_TILES_SYNCHRONOUSLY", "YES");
            if( !bIsTiled && nBands > 1 )
            {
                papszGTiffOptions = CSLSetNameValue(papszGTiffOptions,
                                                    "BLOCKYSIZE", "1");
            }
        }

        poExternalDS = poDrv->Create( osExternalFilename, nXSize, nYSize,
                                      nBands,
                                      eType, papszGTiffOptions );
        CSLDestroy(papszGTiffOptions);
        if( poExternalDS == NULL )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Cannot create %s",
                      osExternalFilename.c_str() );
            VSIFCloseL(fp);
            return NULL;
        }
    }

    ISIS3Dataset* poDS = new ISIS3Dataset();
    poDS->eAccess = GA_Update;
    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->m_osExternalFilename = osExternalFilename;
    poDS->m_poExternalDS = poExternalDS;
    poDS->m_bGeoTIFFAsRegularExternal = bGeoTIFFAsRegularExternal;
    if( bGeoTIFFAsRegularExternal )
        poDS->m_bGeoTIFFInitDone = false;
    poDS->m_fpLabel = fp;
    poDS->m_fpImage = fpImage ? fpImage: fp;
    poDS->m_bIsLabelWritten = false;
    poDS->m_bIsTiled = bIsTiled;
    poDS->m_bInitToNodata = true;
    poDS->m_osComment = CSLFetchNameValueDef(papszOptions, "COMMENT", "");
    poDS->m_osLatitudeType = CSLFetchNameValueDef(papszOptions,
                                                  "LATITUDE_TYPE", "");
    poDS->m_osLongitudeDirection = CSLFetchNameValueDef(papszOptions,
                                                  "LONGITUDE_DIRECTION", "");
    poDS->m_osLongitudeDomain = CSLFetchNameValueDef(papszOptions,
                                                  "LONGITUDE_DOMAIN", "");
    poDS->m_bWriteBoundingDegrees = CPLFetchBool(papszOptions,
                                                 "WRITE_BOUNDING_DEGREES",
                                                 true);
    poDS->m_osBoundingDegrees = CSLFetchNameValueDef(papszOptions,
                                                     "BOUNDING_DEGREES", "");
    poDS->m_bUseSrcLabel = CPLFetchBool(papszOptions, "USE_SRC_LABEL", true);
    poDS->m_bUseSrcMapping =
                        CPLFetchBool(papszOptions, "USE_SRC_MAPPING", false);
    const double dfNoData = (eType == GDT_Byte)    ? NULL1:
                            (eType == GDT_UInt16)  ? NULLU2:
                            (eType == GDT_Int16)   ? NULL2:
                            /*(eType == GDT_Float32) ?*/ NULL4;

    for( int i = 0; i < nBands; i++ )
    {
        GDALRasterBand *poBand = NULL;

        if( EQUAL(pszDataLocation, "GEOTIFF") )
        {
            ISIS3WrapperRasterBand* poISISBand =
                new ISIS3WrapperRasterBand(
                            poDS->m_poExternalDS->GetRasterBand( i+1 ) );
            poBand = poISISBand;
        }
        else if( bIsTiled  )
        {
            ISISTiledBand* poISISBand =
                new ISISTiledBand( poDS, poDS->m_fpImage, i+1, eType,
                                   nBlockXSize, nBlockYSize,
                                   0, //nSkipBytes, to be hacked
                                   // afterwards for in-label imagery
                                   0, 0,
                                   CPL_IS_LSB );

            poBand = poISISBand;
        }
        else
        {
            const int nPixelOffset = GDALGetDataTypeSizeBytes(eType);
            const int nLineOffset = nPixelOffset * nXSize;
            const vsi_l_offset nBandOffset = 
                static_cast<vsi_l_offset>(nLineOffset) * nYSize;
            ISIS3RawRasterBand* poISISBand =
                new ISIS3RawRasterBand( poDS, i+1, poDS->m_fpImage,
                                   nBandOffset * i, // nImgOffset, to be
                                   //hacked afterwards for in-label imagery
                                   nPixelOffset, nLineOffset, eType,
                                   CPL_IS_LSB,
                                   TRUE );

            poBand = poISISBand;
        }
        poDS->SetBand( i+1, poBand );
        poBand->SetNoDataValue(dfNoData);
    }

    return poDS;
}

/************************************************************************/
/*                      GetUnderlyingDataset()                          */
/************************************************************************/

static GDALDataset* GetUnderlyingDataset( GDALDataset* poSrcDS )
{
    if( poSrcDS->GetDriver() != NULL &&
        poSrcDS->GetDriver() == GDALGetDriverByName("VRT") )
    {
        VRTDataset* poVRTDS = reinterpret_cast<VRTDataset* >(poSrcDS);
        poSrcDS = poVRTDS->GetSingleSimpleSource();
    }

    return poSrcDS;
}

/************************************************************************/
/*                            CreateCopy()                              */
/************************************************************************/

GDALDataset* ISIS3Dataset::CreateCopy( const char *pszFilename,
                                       GDALDataset *poSrcDS,
                                       int /*bStrict*/,
                                       char ** papszOptions,
                                       GDALProgressFunc pfnProgress,
                                       void * pProgressData )
{
    const char* pszDataLocation = CSLFetchNameValueDef(papszOptions,
                                                       "DATA_LOCATION",
                                                       "LABEL");
    GDALDataset* poSrcUnderlyingDS = GetUnderlyingDataset(poSrcDS);
    if( poSrcUnderlyingDS == NULL )
        poSrcUnderlyingDS = poSrcDS;
    if( EQUAL(pszDataLocation, "GEOTIFF") &&
        strcmp(poSrcUnderlyingDS->GetDescription(),
               CSLFetchNameValueDef(papszOptions, "EXTERNAL_FILENAME",
                                    CPLResetExtension(pszFilename, "tif"))
              ) == 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Output file has same name as input file");
        return NULL;
    }
    if( poSrcDS->GetRasterCount() == 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported band count");
        return NULL;
    }

    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();
    const int nBands = poSrcDS->GetRasterCount();
    GDALDataType eType = poSrcDS->GetRasterBand(1)->GetRasterDataType();
    ISIS3Dataset *poDS = reinterpret_cast<ISIS3Dataset*>(
        Create( pszFilename, nXSize, nYSize, nBands, eType, papszOptions ));
    if( poDS == NULL )
        return NULL;

    double adfGeoTransform[6] = { 0.0 };
    if( poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None
        && (adfGeoTransform[0] != 0.0
            || adfGeoTransform[1] != 1.0
            || adfGeoTransform[2] != 0.0
            || adfGeoTransform[3] != 0.0
            || adfGeoTransform[4] != 0.0
            || adfGeoTransform[5] != 1.0) )
    {
        poDS->SetGeoTransform( adfGeoTransform );
    }

    if( poSrcDS->GetProjectionRef() != NULL
        && strlen(poSrcDS->GetProjectionRef()) > 0 )
    {
        poDS->SetProjection( poSrcDS->GetProjectionRef() );
    }

    for(int i=1;i<=nBands;i++)
    {
        const double dfOffset = poSrcDS->GetRasterBand(i)->GetOffset();
        if( dfOffset != 0.0 )
            poDS->GetRasterBand(i)->SetOffset(dfOffset);

        const double dfScale = poSrcDS->GetRasterBand(i)->GetScale();
        if( dfScale != 1.0 )
            poDS->GetRasterBand(i)->SetScale(dfScale);
    }

    // Do we need to remap nodata ?
    int bHasNoData = FALSE;
    poDS->m_dfSrcNoData =
        poSrcDS->GetRasterBand(1)->GetNoDataValue(&bHasNoData);
    poDS->m_bHasSrcNoData = CPL_TO_BOOL(bHasNoData);

    if( poDS->m_bUseSrcLabel )
    {
        char** papszMD_ISIS3 = poSrcDS->GetMetadata("json:ISIS3");
        if( papszMD_ISIS3 != NULL )
        {
            poDS->SetMetadata( papszMD_ISIS3, "json:ISIS3" );
        }
    }

    // We don't need to initialize the imagery as we are going to copy it
    // completely
    poDS->m_bInitToNodata = false;
    CPLErr eErr = GDALDatasetCopyWholeRaster( poSrcDS, poDS,
                                           NULL, pfnProgress, pProgressData );
    poDS->m_bHasSrcNoData = false;
    if( eErr != CE_None )
    {
        delete poDS;
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                         GDALRegister_ISIS3()                         */
/************************************************************************/

void GDALRegister_ISIS3()

{
    if( GDALGetDriverByName( "ISIS3" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "ISIS3" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "USGS Astrogeology ISIS cube (Version 3)" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                               "frmt_isis3.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "lbl cub" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte UInt16 Int16 Float32" );
    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST, "<OpenOptionList/>");
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"  <Option name='DATA_LOCATION' type='string-select' "
                "description='Location of pixel data' default='LABEL'>"
"     <Value>LABEL</Value>"
"     <Value>EXTERNAL</Value>"
"     <Value>GEOTIFF</Value>"
"  </Option>"
"  <Option name='GEOTIFF_AS_REGULAR_EXTERNAL' type='boolean'"
    "description='Whether the GeoTIFF file, if uncompressed, should be "
    "registered as a regular raw file' default='YES'/>"
"  <Option name='GEOTIFF_OPTIONS' type='string' "
    "description='Comma separated list of KEY=VALUE tuples to forward "
    "to the GeoTIFF driver'/>"
"  <Option name='EXTERNAL_FILENAME' type='string' "
                "description='Override default external filename. "
                "Only for DATA_LOCATION=EXTERNAL or GEOTIFF'/>"
"  <Option name='TILED' type='boolean' "
        "description='Whether the pixel data should be tiled' default='NO'/>"
"  <Option name='BLOCKXSIZE' type='int' "
                            "description='Tile width' default='256'/>"
"  <Option name='BLOCKYSIZE' type='int' "
                            "description='Tile height' default='256'/>"
"  <Option name='COMMENT' type='string' "
    "description='Comment to add into the label'/>"
"  <Option name='LATITUDE_TYPE' type='string' "
    "description='Value of Mapping.LatitudeType' default='Planetographic'/>"
"  <Option name='LONGITUDE_DIRECTION' type='string' "
    "description='Value of Mapping.LongitudeDirection' "
    "default='PositiveEast'/>"
"  <Option name='LONGITUDE_DOMAIN' type='float' "
    "description='Value of Mapping.LongitudeDomain'/>"
"  <Option name='WRITE_BOUNDING_DEGREES' type='boolean'"
    "description='Whether to write Min/MaximumLong/Latitude values' "
    "default='YES'/>"
"  <Option name='BOUNDING_DEGREES' type='string'"
    "description='Manually set bounding box with the syntax "
    "min_long,min_lat,max_long,max_lat'/>"
"  <Option name='USE_SRC_LABEL' type='boolean'"
    "description='Whether to use source label in ISIS3 to ISIS3 conversions' "
    "default='YES'/>"
"  <Option name='USE_SRC_MAPPING' type='boolean'"
    "description='Whether to use Mapping group from source label in "
                 "ISIS3 to ISIS3 conversions' "
    "default='NO'/>"
"</CreationOptionList>"
    );

    poDriver->pfnOpen = ISIS3Dataset::Open;
    poDriver->pfnIdentify = ISIS3Dataset::Identify;
    poDriver->pfnCreate = ISIS3Dataset::Create;
    poDriver->pfnCreateCopy = ISIS3Dataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
