/******************************************************************************
 * $Id$
 *
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Private class declarations for the HFA classes used to read
 *           Erdas Imagine (.img) files.  Public (C callable) declarations
 *           are in hfa.h.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Intergraph Corporation
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

#ifndef HFA_P_H_INCLUDED
#define HFA_P_H_INCLUDED

#include "cpl_port.h"
#include "hfa.h"

#include <cstdio>
#include <memory>
#include <vector>
#include <set>

#include "cpl_error.h"
#include "cpl_vsi.h"
#include "ogr_spatialref.h"

#ifdef CPL_LSB
#  define HFAStandard(n,p) {}
#else
   void HFAStandard( int, void *);
#endif

#include "hfa.h"

class HFABand;
class HFADictionary;
class HFAEntry;
class HFASpillFile;
class HFAType;

/************************************************************************/
/*      Flag indicating read/write, or read-only access to data.        */
/************************************************************************/
typedef enum {
    /*! Read only (no update) access */ HFA_ReadOnly = 0,
    /*! Read/write access. */           HFA_Update = 1
} HFAAccess;

/************************************************************************/
/*                              HFAInfo_t                               */
/*                                                                      */
/*      This is just a structure, and used hold info about the whole    */
/*      dataset within hfaopen.cpp                                      */
/************************************************************************/
struct hfainfo {
    VSILFILE    *fp;

    char        *pszPath;
    char        *pszFilename;  // Sans path.
    char        *pszIGEFilename;  // Sans path.

    HFAAccess   eAccess;

    GUInt32     nEndOfFile;
    GUInt32     nRootPos;
    GUInt32     nDictionaryPos;

    GInt16      nEntryHeaderLength;
    GInt32      nVersion;

    bool        bTreeDirty;
    HFAEntry    *poRoot;

    HFADictionary *poDictionary;
    char        *pszDictionary;

    int         nXSize;
    int         nYSize;

    int         nBands;
    HFABand     **papoBand;

    void        *pMapInfo;
    void        *pDatum;
    void        *pProParameters;

    struct hfainfo *psDependent;
};

typedef struct hfainfo HFAInfo_t;

GUInt32 HFAAllocateSpace( HFAInfo_t *, GUInt32 );
CPLErr  HFAParseBandInfo( HFAInfo_t * );
HFAInfo_t *HFAGetDependent( HFAInfo_t *, const char * );
HFAInfo_t *HFACreateDependent( HFAInfo_t *psBase );
bool HFACreateSpillStack( HFAInfo_t *, int nXSize, int nYSize, int nLayers,
                          int nBlockSize, EPTType eDataType,
                          GIntBig *pnValidFlagsOffset,
                          GIntBig *pnDataOffset );

const char * const * GetHFAAuxMetaDataList();

double *HFAReadBFUniqueBins( HFAEntry *poBinFunc, int nPCTColors );

int CPL_DLL
HFACreateLayer( HFAHandle psInfo, HFAEntry *poParent,
                const char *pszLayerName,
                int bOverview, int nBlockSize,
                int bCreateCompressed, int bCreateLargeRaster,
                int bDependentLayer,
                int nXSize, int nYSize, EPTType eDataType,
                char **papszOptions,

                // These are only related to external (large) files.
                GIntBig nStackValidFlagsOffset,
                GIntBig nStackDataOffset,
                int nStackCount, int nStackIndex );

std::unique_ptr<OGRSpatialReference>
HFAPCSStructToOSR( const Eprj_Datum *psDatum,
                   const Eprj_ProParameters *psPro,
                   const Eprj_MapInfo *psMapInfo,
                   HFAEntry *poMapInformation );

const char *const* HFAGetDatumMap();
const char *const* HFAGetUnitMap();

/************************************************************************/
/*                               HFABand                                */
/************************************************************************/

class HFABand
{
    int         nBlocks;

    // Used for single-file modification.
    vsi_l_offset *panBlockStart;
    int         *panBlockSize;
    int         *panBlockFlag;

    // Used for spill-file modification.
    vsi_l_offset nBlockStart;
    vsi_l_offset nBlockSize;
    int         nLayerStackCount;
    int         nLayerStackIndex;

#define BFLG_VALID      0x01
#define BFLG_COMPRESSED 0x02

    int         nPCTColors;
    double      *apadfPCT[4];
    double      *padfPCTBins;

    CPLErr      LoadBlockInfo();
    CPLErr      LoadExternalBlockInfo();

    void ReAllocBlock( int iBlock, int nSize );
    void NullBlock( void * );

    CPLString   osOverName;

  public:
                HFABand( HFAInfo_t *, HFAEntry * );
                ~HFABand();

    HFAInfo_t   *psInfo;

    VSILFILE    *fpExternal;

    EPTType     eDataType;
    HFAEntry    *poNode;

    int         nBlockXSize;
    int         nBlockYSize;

    int         nWidth;
    int         nHeight;

    int         nBlocksPerRow;
    int         nBlocksPerColumn;

    bool        bNoDataSet;
    double      dfNoData;

    bool        bOverviewsPending;
    int         nOverviews;
    HFABand     **papoOverviews;

    CPLErr      GetRasterBlock( int nXBlock, int nYBlock, void * pData,
                                int nDataSize );
    CPLErr      SetRasterBlock( int nXBlock, int nYBlock, void * pData );

    const char * GetBandName();
    void SetBandName(const char *pszName);

    CPLErr  SetNoDataValue( double dfValue );

    CPLErr      GetPCT( int *, double **, double **, double **, double **,
                        double ** );
    CPLErr      SetPCT( int, double *, double *, double *, double * );

    int         CreateOverview( int nOverviewLevel, const char *pszResampling );
    CPLErr      CleanOverviews();

    CPLErr      LoadOverviews();
};

/************************************************************************/
/*                               HFAEntry                               */
/*                                                                      */
/*      Base class for all entry types.  Most entry types do not        */
/*      have a subclass, and are just handled generically with this     */
/*      class.                                                          */
/************************************************************************/
class HFAEntry
{
    bool        bDirty;
    GUInt32     nFilePos;

    HFAInfo_t   *psHFA;
    HFAEntry    *poParent;
    HFAEntry    *poPrev;

    GUInt32     nNextPos;
    HFAEntry    *poNext;

    GUInt32     nChildPos;
    HFAEntry    *poChild;

    char        szName[64];
    char        szType[32];

    HFAType     *poType;

    GUInt32     nDataPos;
    GUInt32     nDataSize;
    GByte      *pabyData;

    void        LoadData();

    bool        GetFieldValue( const char *, char, void *,
                               int *pnRemainingDataSize );
    CPLErr      SetFieldValue( const char *, char, void * );

    bool        bIsMIFObject;

                HFAEntry();
                HFAEntry( const char * pszDictionary,
                          const char * pszTypeName,
                          int nDataSizeIn,
                          GByte* pabyDataIn );
    std::vector<HFAEntry*> FindChildren( const char *pszName,
                                         const char *pszType,
                                         int nRecLevel,
                                         int* pbErrorDetected);

public:
    static HFAEntry* New( HFAInfo_t * psHFA, GUInt32 nPos,
                          HFAEntry * poParent,
                          HFAEntry *poPrev) CPL_WARN_UNUSED_RESULT;

                HFAEntry( HFAInfo_t *psHFA,
                          const char *pszNodeName,
                          const char *pszTypeName,
                          HFAEntry *poParent );

    static HFAEntry* New( HFAInfo_t *psHFA,
                          const char *pszNodeName,
                          const char *pszTypeName,
                          HFAEntry *poParent ) CPL_WARN_UNUSED_RESULT;

    virtual     ~HFAEntry();

    static HFAEntry*  BuildEntryFromMIFObject(
                          HFAEntry *poContainer,
                          const char *pszMIFObjectPath ) CPL_WARN_UNUSED_RESULT;

    CPLErr      RemoveAndDestroy();

    GUInt32     GetFilePos() const CPL_WARN_UNUSED_RESULT { return nFilePos; }

    const char  *GetName() const CPL_WARN_UNUSED_RESULT { return szName; }
    void SetName( const char *pszNodeName );

    const char  *GetType() const CPL_WARN_UNUSED_RESULT { return szType; }
    HFAType     *GetTypeObject() CPL_WARN_UNUSED_RESULT;

    GByte      *GetData() CPL_WARN_UNUSED_RESULT { LoadData(); return pabyData; }
    GUInt32     GetDataPos() const CPL_WARN_UNUSED_RESULT { return nDataPos; }
    GUInt32     GetDataSize() const CPL_WARN_UNUSED_RESULT { return nDataSize; }

    HFAEntry    *GetChild() CPL_WARN_UNUSED_RESULT;
    HFAEntry    *GetNext() CPL_WARN_UNUSED_RESULT;
    HFAEntry    *GetNamedChild( const char * ) CPL_WARN_UNUSED_RESULT;
    std::vector<HFAEntry*> FindChildren( const char *pszName,
                                         const char *pszType) CPL_WARN_UNUSED_RESULT;

    GInt32      GetIntField( const char *, CPLErr * = nullptr ) CPL_WARN_UNUSED_RESULT;
    double      GetDoubleField( const char *, CPLErr * = nullptr ) CPL_WARN_UNUSED_RESULT;
    const char  *GetStringField( const char *, CPLErr * = nullptr, int *pnRemainingDataSize = nullptr ) CPL_WARN_UNUSED_RESULT;
    GIntBig     GetBigIntField( const char *, CPLErr * = nullptr ) CPL_WARN_UNUSED_RESULT;
    int         GetFieldCount( const char *, CPLErr * = nullptr ) CPL_WARN_UNUSED_RESULT;

    CPLErr      SetIntField( const char *, int );
    CPLErr      SetDoubleField( const char *, double );
    CPLErr      SetStringField( const char *, const char * );

    void        DumpFieldValues( FILE *, const char * = nullptr );

    void        SetPosition();
    CPLErr      FlushToDisk();

    void        MarkDirty();
    GByte      *MakeData( int nSize = 0 );
};

/************************************************************************/
/*                               HFAField                               */
/*                                                                      */
/*      A field in a HFAType in the dictionary.                         */
/************************************************************************/

class HFAField
{
  public:
    int         nBytes;

    int         nItemCount;
    // TODO(schwehr): Rename chPointer to something more meaningful.
    // It's not a pointer.
    char        chPointer;      // '\0', '*' or 'p'
    char        chItemType;     // 1|2|4|e|...

    char        *pszItemObjectType;  // if chItemType == 'o'
    HFAType     *poItemObjectType;

    char        **papszEnumNames;  // Normally NULL if not an enum.

    char        *pszFieldName;

    char        szNumberString[36];  // Buffer used to return int as a string.

                HFAField();
                ~HFAField();

    const char *Initialize( const char * );

    bool        CompleteDefn( HFADictionary * );

    void        Dump( FILE * );

    bool        ExtractInstValue( const char * pszField, int nIndexValue,
                                  GByte *pabyData, GUInt32 nDataOffset,
                                  int nDataSize, char chReqType,
                                  void *pReqReturn,
                                  int *pnRemainingDataSize = nullptr );

    CPLErr      SetInstValue( const char * pszField, int nIndexValue,
                              GByte *pabyData, GUInt32 nDataOffset,
                              int nDataSize,
                              char chReqType, void *pValue );

    void        DumpInstValue( FILE *fpOut, GByte *pabyData,
                               GUInt32 nDataOffset, int nDataSize,
                               const char *pszPrefix = nullptr );

    int         GetInstBytes( GByte *, int, std::set<HFAField*>& oVisitedFields );
    int         GetInstCount( GByte * pabyData, int nDataSize ) const;
};

/************************************************************************/
/*                               HFAType                                */
/*                                                                      */
/*      A type in the dictionary.                                       */
/************************************************************************/

class HFAType
{
    bool bInCompleteDefn;

  public:
    int         nBytes;

    std::vector<std::unique_ptr<HFAField>> apoFields;

    char        *pszTypeName;

                HFAType();
                ~HFAType();

    const char *Initialize( const char * );

    bool        CompleteDefn( HFADictionary * );

    void        Dump( FILE * );

    int         GetInstBytes( GByte *, int, std::set<HFAField*>& oVisitedFields ) const;
    int         GetInstCount( const char *pszField, GByte *pabyData,
                              GUInt32 nDataOffset, int nDataSize );
    bool        ExtractInstValue( const char * pszField,
                                  GByte *pabyData, GUInt32 nDataOffset,
                                  int nDataSize, char chReqType,
                                  void *pReqReturn, int *pnRemainingDataSize );
    CPLErr      SetInstValue( const char * pszField, GByte *pabyData,
                              GUInt32 nDataOffset, int nDataSize,
                              char chReqType, void * pValue );
    void        DumpInstValue( FILE *fpOut, GByte *pabyData,
                               GUInt32 nDataOffset, int nDataSize,
                               const char *pszPrefix = nullptr ) const;
};

/************************************************************************/
/*                            HFADictionary                             */
/************************************************************************/

class HFADictionary
{
  public:
    explicit     HFADictionary( const char *pszDict );
                ~HFADictionary();

    HFAType     *FindType( const char * );
    void        AddType( HFAType * );

    static int  GetItemSize( char );

    void        Dump( FILE * );

  private:
    int         nTypes;
    int         nTypesMax;
    HFAType     **papoTypes;

  public:
    // TODO(schwehr): Make these members private.
    CPLString   osDictionaryText;
    bool        bDictionaryTextDirty;
};

/************************************************************************/
/*                             HFACompress                              */
/*                                                                      */
/*      Class that given a block of memory compresses the contents      */
/*      using run length encoding (RLE) as used by Imagine.             */
/************************************************************************/

class HFACompress
{
public:
  HFACompress( void *pData, GUInt32 nBlockSize, EPTType eDataType );
  ~HFACompress();

  // This is the method that does the work.
  bool compressBlock();

  // Static method to allow us to query whether HFA type supported.
  static bool QueryDataTypeSupported( EPTType eHFADataType );

  // Get methods - only valid after compressBlock has been called.
  GByte*  getCounts() const { return m_pCounts; }
  GUInt32 getCountSize() const { return m_nSizeCounts; }
  GByte*  getValues() const { return m_pValues; }
  GUInt32 getValueSize() const { return m_nSizeValues; }
  GUInt32 getMin() const { return m_nMin; }
  GUInt32 getNumRuns() const { return m_nNumRuns; }
  GByte   getNumBits() const { return m_nNumBits; }

private:
  static void makeCount( GUInt32 count, GByte *pCounter, GUInt32 *pnSizeCount );
  GUInt32 findMin( GByte *pNumBits );
  GUInt32 valueAsUInt32( GUInt32 index );
  void encodeValue( GUInt32 val, GUInt32 repeat );

  void *m_pData;
  GUInt32 m_nBlockSize;
  GUInt32 m_nBlockCount;
  EPTType m_eDataType;
  // The number of bits the datatype we are trying to compress takes.
  int m_nDataTypeNumBits;

  GByte   *m_pCounts;
  GByte   *m_pCurrCount;
  GUInt32  m_nSizeCounts;

  GByte   *m_pValues;
  GByte   *m_pCurrValues;
  GUInt32  m_nSizeValues;

  GUInt32  m_nMin;
  GUInt32  m_nNumRuns;
  // The number of bits needed to compress the range of values in the block.
  GByte    m_nNumBits;
};

#endif /* ndef HFA_P_H_INCLUDED */
