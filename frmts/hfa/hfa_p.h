/******************************************************************************
 * $Id$
 *
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Private class declarations for the HFA classes used to read
 *           Erdas Imagine (.img) files.  Public (C callable) declarations
 *           are in hfa.h.
 * Author:   Frank Warmerdam, warmerda@home.com
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
/*                              HFAInfo_t                               */
/*                                                                      */
/*      This is just a structure, and used hold info about the whole    */
/*      dataset within hfaopen.cpp                                      */
/************************************************************************/
typedef struct {
    FILE	*fp;

    GUInt32	nRootPos;
    GUInt32	nDictionaryPos;
    
    GInt16	nEntryHeaderLength;
    GInt32	nVersion;

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

} HFAInfo_t;

#define HFA_PRIVATE

#include "hfa.h"

/************************************************************************/
/*                               HFABand                                */
/************************************************************************/

class HFABand
{
    int		nBlocks;
    int		*panBlockStart;
    int		*panBlockSize;
    int		*panBlockFlag;

#define BFLG_VALID	0x01    
#define BFLG_COMPRESSED	0x02

    int		nPCTColors;
    double	*apadfPCT[3];

    CPLErr	LoadBlockInfo();
    
  public:
    		HFABand( HFAInfo_t *, HFAEntry * );
                ~HFABand();
                
    HFAInfo_t	*psInfo;
                         
    int		nDataType;
    HFAEntry	*poNode;
    
    int		nBlockXSize;
    int		nBlockYSize;

    int		nWidth;
    int		nHeight;

    int		nBlocksPerRow;
    int		nBlocksPerColumn;

    CPLErr	GetRasterBlock( int nXBlock, int nYBlock, void * pData );

    CPLErr	GetPCT( int *, double **, double **, double ** );
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
    GInt32	nFilePos;
    
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

public:
    		HFAEntry( HFAInfo_t * psHFA, GUInt32 nPos,
                          HFAEntry * poParent, HFAEntry *poPrev);

    virtual     ~HFAEntry();                

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

    void	DumpFieldValues( FILE *, const char * = NULL );
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
                               GByte *pabyData, int nDataOffset, int nDataSize,
                               char chReqType );

    void	DumpInstValue( FILE *fpOut, 
                               GByte *pabyData, int nDataOffset, int nDataSize,
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
                               GByte *pabyData, int nDataOffset, int nDataSize,
                               char chReqType );
    void	DumpInstValue( FILE *fpOut, 
                               GByte *pabyData, int nDataOffset, int nDataSize,
                               const char *pszPrefix = NULL );
};

/************************************************************************/
/*                            HFADictionary                             */
/************************************************************************/

class HFADictionary
{
  public:
    int		nTypes;
    HFAType	**papoTypes;
    
    		HFADictionary( const char * );
                ~HFADictionary();

    HFAType	*FindType( const char * );

    static int	GetItemSize( char );

    void	Dump( FILE * );
};


#endif /* ndef _HFA_P_H_INCLUDED */
