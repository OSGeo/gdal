/******************************************************************************
 * $Id$
 *
 * Name:     hfa_p.h
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
#  define HFAStandard(n,p)	CPLSwapWord(n,p)
#endif

class HFAEntry;
class HFAType;
class HFADictionary;

typedef struct {
    FILE	*fp;

    GUInt32	nRootPos;
    GUInt32	nDictionaryPos;
    
    GInt16	nEntryHeaderLength;
    GInt32	nVersion;

    HFAEntry	*poRoot;

    HFADictionary *poDictionary;
    char	*pszDictionary;
    
} HFAInfo_t;

#define HFA_PRIVATE

#include "hfa.h"

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
    
    int		*panFieldItemCount;
    GByte	**papbyFieldData;

    int		GetField( const char * );
    void	LoadData();

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

    GInt32	GetIntField( const char *, int = 0 );
    double	GetDoubleField( const char *, int = 0 );
    const char	*GetStringField( const char * );

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
