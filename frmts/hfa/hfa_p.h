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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.16  2004/07/16 20:40:32  warmerda
 * Added a series of patches from Andreas Wimmer which:
 *  o Add lots of improved support for metadata.
 *  o Use USE_SPILL only, instead of SPILL_FILE extra creation option.
 *  o Added ability to control block sizes.
 *
 * Revision 1.15  2003/05/13 19:32:10  warmerda
 * support for reading and writing opacity provided by Diana Esch-Mosher
 *
 * Revision 1.14  2003/04/29 08:53:45  dron
 * In Get/SetRasterBlock calculate block offset in place when we have spill file.
 *
 * Revision 1.13  2003/04/22 19:40:36  warmerda
 * fixed email address
 *
 * Revision 1.12  2003/02/25 18:03:20  warmerda
 * added AddType() method to HFADictionary
 *
 * Revision 1.11  2003/02/21 15:40:58  dron
 * Added support for writing large (>4 GB) Erdas Imagine files.
 *
 * Revision 1.10  2001/06/10 20:31:35  warmerda
 * use vsi_l_offset for block offsets
 *
 * Revision 1.9  2000/12/29 16:37:32  warmerda
 * Use GUInt32 for all file offsets
 *
 * Revision 1.8  2000/10/31 18:02:32  warmerda
 * Added external and unnamed overview support
 *
 * Revision 1.7  2000/10/20 04:18:15  warmerda
 * added overviews, stateplane, and u4
 *
 * Revision 1.6  2000/10/12 19:30:32  warmerda
 * substantially improved write support
 *
 * Revision 1.5  2000/09/29 21:42:38  warmerda
 * preliminary write support implemented
 *
 * Revision 1.4  1999/01/28 16:24:09  warmerda
 * Handle HFAStandardWord().
 *
 * Revision 1.3  1999/01/22 17:39:26  warmerda
 * Added HFABand, and other stuff
 *
 * Revision 1.2  1999/01/04 22:52:47  warmerda
 * field access working
 *
 * Revision 1.1  1999/01/04 05:28:13  warmerda
 * New
 *
 */

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

char ** GetHFAAuxMetaDataList();

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

    CPLErr	LoadBlockInfo();
    CPLErr	LoadExternalBlockInfo();

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

    int		nOverviews;
    HFABand     **papoOverviews;
    
    CPLErr	GetRasterBlock( int nXBlock, int nYBlock, void * pData );
    CPLErr	SetRasterBlock( int nXBlock, int nYBlock, void * pData );

    CPLErr	GetPCT( int *, double **, double **, double **, double ** );
    CPLErr	SetPCT( int, double *, double *, double *, double * );
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

    void	*GetFieldValue( const char *, char );
    CPLErr      SetFieldValue( const char *, char, void * );

public:
    		HFAEntry( HFAInfo_t * psHFA, GUInt32 nPos,
                          HFAEntry * poParent, HFAEntry *poPrev);

                HFAEntry( HFAInfo_t *psHFA, 
                          const char *pszNodeName,
                          const char *pszTypeName,
                          HFAEntry *poParent );
                          
    virtual     ~HFAEntry();                

    GUInt32	GetFilePos() { return nFilePos; }

    const char	*GetName() { return szName; }
    const char  *GetType() { return szType; }

    GUInt32	GetDataPos() { return nDataPos; }
    GUInt32	GetDataSize() { return nDataSize; }

    HFAEntry	*GetChild();
    HFAEntry	*GetNext();
    HFAEntry    *GetNamedChild( const char * );

    GInt32	GetIntField( const char *, CPLErr * = NULL );
    double	GetDoubleField( const char *, CPLErr * = NULL );
    const char	*GetStringField( const char *, CPLErr * = NULL );

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

    		HFAField();
                ~HFAField();

    const char *Initialize( const char * );

    void	CompleteDefn( HFADictionary * );

    void	Dump( FILE * );

    void	*ExtractInstValue( const char * pszField, int nIndexValue,
                     GByte *pabyData, GUInt32 nDataOffset, int nDataSize,
                     char chReqType );

    CPLErr      SetInstValue( const char * pszField, int nIndexValue,
                     GByte *pabyData, GUInt32 nDataOffset, int nDataSize,
                     char chReqType, void *pValue );

    void	DumpInstValue( FILE *fpOut, 
                     GByte *pabyData, GUInt32 nDataOffset, int nDataSize,
                     const char *pszPrefix = NULL );
    
    int		GetInstBytes( GByte * pabyData );
    int		GetInstCount( GByte * pabyData );
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

    int		GetInstBytes( GByte * pabyData );
    void	*ExtractInstValue( const char * pszField,
                           GByte *pabyData, GUInt32 nDataOffset, int nDataSize,
                               char chReqType );
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
    
    		HFADictionary( const char * );
                ~HFADictionary();

    HFAType	*FindType( const char * );
    void        AddType( HFAType * );

    static int	GetItemSize( char );

    void	Dump( FILE * );
};


#endif /* ndef _HFA_P_H_INCLUDED */
