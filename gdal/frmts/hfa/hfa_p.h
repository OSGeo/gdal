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

#ifndef _HFA_P_H_INCLUDED
#define _HFA_P_H_INCLUDED

#include "cpl_port.h"
#include "cpl_error.h"
#include "cpl_vsi.h"

#ifdef CPL_LSB
#  define HFAStandard(n,p)	{}
#else
   void HFAStandard( int, void *);
#endif

class HFAEntry;
class HFAType;
class HFADictionary;
class HFABand;
class HFASpillFile;

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
typedef struct hfainfo {
    FILE	*fp;

    char	*pszPath;
    char        *pszFilename; /* sans path */
    char        *pszIGEFilename; /* sans path */

    HFAAccess	eAccess;

    GUInt32     nEndOfFile;
    GUInt32	nRootPos;
    GUInt32	nDictionaryPos;
    
    GInt16	nEntryHeaderLength;
    GInt32	nVersion;

    int         bTreeDirty;
    HFAEntry	*poRoot;

    HFADictionary *poDictionary;
    char	*pszDictionary;

    int		nXSize;
    int		nYSize;

    int		nBands;
    HFABand	**papoBand;

    void	*pMapInfo;
    void        *pDatum;
    void        *pProParameters;

    struct hfainfo *psDependent;
} HFAInfo_t;

GUInt32 HFAAllocateSpace( HFAInfo_t *, GUInt32 );
CPLErr  HFAParseBandInfo( HFAInfo_t * );
HFAInfo_t *HFAGetDependent( HFAInfo_t *, const char * );
HFAInfo_t *HFACreateDependent( HFAInfo_t *psBase );
int HFACreateSpillStack( HFAInfo_t *, int nXSize, int nYSize, int nLayers, 
                         int nBlockSize, int nDataType,
                         GIntBig *pnValidFlagsOffset, 
                         GIntBig *pnDataOffset );

const char ** GetHFAAuxMetaDataList();

double *HFAReadBFUniqueBins( HFAEntry *poBinFunc, int nPCTColors );

#define HFA_PRIVATE

#include "hfa.h"

/************************************************************************/
/*                               HFABand                                */
/************************************************************************/

class HFABand
{
    int		nBlocks;

    // Used for single-file modification
    vsi_l_offset *panBlockStart;
    int		*panBlockSize;
    int		*panBlockFlag;

    // Used for spill-file modification
    vsi_l_offset nBlockStart;
    vsi_l_offset nBlockSize;
    int         nLayerStackCount;
    int         nLayerStackIndex;

#define BFLG_VALID	0x01    
#define BFLG_COMPRESSED	0x02

    int		nPCTColors;
    double	*apadfPCT[4];
    double      *padfPCTBins;

    CPLErr	LoadBlockInfo();
    CPLErr	LoadExternalBlockInfo();
    
    void ReAllocBlock( int iBlock, int nSize );
    void NullBlock( void * );

    CPLString   osOverName;

  public:
    		HFABand( HFAInfo_t *, HFAEntry * );
                ~HFABand();
                
    HFAInfo_t	*psInfo;

    FILE	*fpExternal;
                         
    int		nDataType;
    HFAEntry	*poNode;

    int		nBlockXSize;
    int		nBlockYSize;

    int		nWidth;
    int		nHeight;

    int		nBlocksPerRow;
    int		nBlocksPerColumn;

    int         bNoDataSet;
    double      dfNoData;

    int         bOverviewsPending;
    int		nOverviews;
    HFABand     **papoOverviews;
    
    CPLErr	GetRasterBlock( int nXBlock, int nYBlock, void * pData, int nDataSize );
    CPLErr	SetRasterBlock( int nXBlock, int nYBlock, void * pData );
    
    const char * GetBandName();
    void SetBandName(const char *pszName);

    CPLErr  SetNoDataValue( double dfValue );

    CPLErr	GetPCT( int *, double **, double **, double **, double **,
                        double ** );
    CPLErr	SetPCT( int, double *, double *, double *, double * );

    int         CreateOverview( int nOverviewLevel );
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
    int         bDirty;
    GUInt32	nFilePos;
    
    HFAInfo_t	*psHFA;
    HFAEntry	*poParent;
    HFAEntry	*poPrev;

    GUInt32	nNextPos;
    HFAEntry	*poNext;
    
    GUInt32	nChildPos;
    HFAEntry	*poChild;

    char	szName[64];
    char	szType[32];

    HFAType	*poType;

    GUInt32	nDataPos;
    GUInt32	nDataSize;
    GByte	*pabyData;

    void	LoadData();

    int 	GetFieldValue( const char *, char, void * );
    CPLErr      SetFieldValue( const char *, char, void * );

    int         bIsMIFObject;

public:
    		HFAEntry( HFAInfo_t * psHFA, GUInt32 nPos,
                          HFAEntry * poParent, HFAEntry *poPrev);

                HFAEntry( HFAInfo_t *psHFA, 
                          const char *pszNodeName,
                          const char *pszTypeName,
                          HFAEntry *poParent );

                HFAEntry( HFAEntry *poContainer, const char *pszMIFObjectPath );
                          
    virtual     ~HFAEntry();                

    CPLErr      RemoveAndDestroy();

    GUInt32	GetFilePos() { return nFilePos; }

    const char	*GetName() { return szName; }
    void SetName( const char *pszNodeName );
    
    const char  *GetType() { return szType; }
    HFAType     *GetTypeObject();

    GByte      *GetData() { LoadData(); return pabyData; }
    GUInt32	GetDataPos() { return nDataPos; }
    GUInt32	GetDataSize() { return nDataSize; }

    HFAEntry	*GetChild();
    HFAEntry	*GetNext();
    HFAEntry    *GetNamedChild( const char * );

    GInt32	GetIntField( const char *, CPLErr * = NULL );
    double	GetDoubleField( const char *, CPLErr * = NULL );
    const char	*GetStringField( const char *, CPLErr * = NULL );
    GIntBig     GetBigIntField( const char *, CPLErr * = NULL );
    int         GetFieldCount( const char *, CPLErr * = NULL );

    CPLErr      SetIntField( const char *, int );
    CPLErr      SetDoubleField( const char *, double );
    CPLErr      SetStringField( const char *, const char * );

    void	DumpFieldValues( FILE *, const char * = NULL );

    void        SetPosition();
    CPLErr      FlushToDisk();

    void	MarkDirty();
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
    int		nBytes;
    
    int		nItemCount;
    char	chPointer; 	/* '\0', '*' or 'p' */
    char	chItemType;	/* 1|2|4|e|... */

    char	*pszItemObjectType; /* if chItemType == 'o' */
    HFAType	*poItemObjectType;

    char	**papszEnumNames; /* normally NULL if not an enum */

    char	*pszFieldName;

    char        szNumberString[28]; /* buffer used to return an int as a string */

    		HFAField();
                ~HFAField();

    const char *Initialize( const char * );

    void	CompleteDefn( HFADictionary * );

    void	Dump( FILE * );

    int 	ExtractInstValue( const char * pszField, int nIndexValue,
                     GByte *pabyData, GUInt32 nDataOffset, int nDataSize,
                     char chReqType, void *pReqReturn );

    CPLErr      SetInstValue( const char * pszField, int nIndexValue,
                     GByte *pabyData, GUInt32 nDataOffset, int nDataSize,
                     char chReqType, void *pValue );

    void	DumpInstValue( FILE *fpOut, 
                     GByte *pabyData, GUInt32 nDataOffset, int nDataSize,
                     const char *pszPrefix = NULL );
    
    int		GetInstBytes( GByte *, int );
    int		GetInstCount( GByte * pabyData, int nDataSize );
};


/************************************************************************/
/*                               HFAType                                */
/*                                                                      */
/*      A type in the dictionary.                                       */
/************************************************************************/

class HFAType
{
  public:
    int		nBytes;
    
    int		nFields;
    HFAField	**papoFields;

    char	*pszTypeName;

    		HFAType();
                ~HFAType();
                
    const char *Initialize( const char * );

    void	CompleteDefn( HFADictionary * );

    void	Dump( FILE * );

    int		GetInstBytes( GByte *, int );
    int         GetInstCount( const char *pszField, 
                          GByte *pabyData, GUInt32 nDataOffset, int nDataSize);
    int         ExtractInstValue( const char * pszField,
                                  GByte *pabyData, GUInt32 nDataOffset, int nDataSize,
                               char chReqType, void *pReqReturn );
    CPLErr      SetInstValue( const char * pszField,
                           GByte *pabyData, GUInt32 nDataOffset, int nDataSize,
                           char chReqType, void * pValue );
    void	DumpInstValue( FILE *fpOut, 
                           GByte *pabyData, GUInt32 nDataOffset, int nDataSize,
                           const char *pszPrefix = NULL );
};

/************************************************************************/
/*                            HFADictionary                             */
/************************************************************************/

class HFADictionary
{
  public:
    int		nTypes;
    int         nTypesMax;
    HFAType	**papoTypes;

    CPLString   osDictionaryText;
    int         bDictionaryTextDirty;
    
                HFADictionary( const char *pszDict );
                ~HFADictionary();

    HFAType	*FindType( const char * );
    void        AddType( HFAType * );

    static int	GetItemSize( char );

    void	Dump( FILE * );
};

/************************************************************************/
/*                             HFACompress                              */
/*                                                                      */
/*      Class that given a block of memory compresses the contents      */
/*      using run  length encoding as used by Imagine.                  */
/************************************************************************/

class HFACompress
{
public:
  HFACompress( void *pData, GUInt32 nBlockSize, int nDataType );
  ~HFACompress();
  
  // This is the method that does the work.
  bool compressBlock();

  // static method to allow us to query whether HFA type supported
  static bool QueryDataTypeSupported( int nHFADataType );

  // Get methods - only valid after compressBlock has been called.
  GByte*  getCounts()     { return m_pCounts; };
  GUInt32 getCountSize()  { return m_nSizeCounts; };
  GByte*  getValues()     { return m_pValues; };
  GUInt32 getValueSize()  { return m_nSizeValues; };
  GUInt32 getMin()        { return m_nMin; };
  GUInt32 getNumRuns()    { return m_nNumRuns; };
  GByte   getNumBits()    { return m_nNumBits; };
  
private:
  void makeCount( GUInt32 count, GByte *pCounter, GUInt32 *pnSizeCount );
  GUInt32 findMin( GByte *pNumBits );
  GUInt32 valueAsUInt32( GUInt32 index );
  void encodeValue( GUInt32 val, GUInt32 repeat );

  void *m_pData;
  GUInt32 m_nBlockSize;
  GUInt32 m_nBlockCount;
  int m_nDataType;
  int m_nDataTypeNumBits; // the number of bits the datatype we are trying to compress takes
  
  GByte   *m_pCounts;
  GByte   *m_pCurrCount;
  GUInt32  m_nSizeCounts;
  
  GByte   *m_pValues;
  GByte   *m_pCurrValues;
  GUInt32  m_nSizeValues;
  
  GUInt32  m_nMin;
  GUInt32  m_nNumRuns;
  GByte    m_nNumBits; // the number of bits needed to compress the range of values in the block
  
};

#endif /* ndef _HFA_P_H_INCLUDED */
