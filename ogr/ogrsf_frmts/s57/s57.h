/******************************************************************************
 * $Id$
 *
 * Project:  S-57 Translator
 * Purpose:  Declarations for S-57 translator not including the
 *           binding onto OGRLayer/DataSource/Driver which are found in
 *           ogr_s57.h.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 * Revision 1.23  2003/09/05 19:12:05  warmerda
 * added RETURN_PRIMITIVES support to get low level prims
 *
 * Revision 1.22  2003/08/21 21:25:30  warmerda
 * Rodney Jensen: Addd FindRecordByObjl()
 *
 * Revision 1.21  2002/10/28 22:30:24  warmerda
 * tab expanded
 *
 * Revision 1.20  2002/05/14 20:34:27  warmerda
 * added PRESERVE_EMPTY_NUMBERS support
 *
 * Revision 1.19  2002/03/05 14:25:43  warmerda
 * expanded tabs
 *
 * Revision 1.18  2002/02/18 21:26:34  warmerda
 * removed declaration for OGRBuildPolygonFromEdges
 *
 * Revision 1.17  2001/12/19 22:44:53  warmerda
 * added ADD_SOUNDG_DEPTH support
 *
 * Revision 1.16  2001/12/17 22:35:16  warmerda
 * added ReadFeature method
 *
 * Revision 1.15  2001/12/14 19:40:18  warmerda
 * added optimized feature counting, and extents collection
 *
 * Revision 1.14  2001/09/12 17:03:21  warmerda
 * auto update support
 *
 * Revision 1.13  2001/08/30 21:18:39  warmerda
 * fixed typedef
 *
 * Revision 1.12  2001/08/30 21:06:55  warmerda
 * expand tabs
 *
 * Revision 1.11  2001/08/30 03:48:43  warmerda
 * preliminary implementation of S57 Update Support
 *
 * Revision 1.10  2000/06/16 18:10:05  warmerda
 * expanded tabs
 *
 * Revision 1.9  1999/11/26 16:17:58  warmerda
 * added DSNM
 *
 * Revision 1.8  1999/11/26 15:08:38  warmerda
 * added setoptions, and LNAM support
 *
 * Revision 1.7  1999/11/25 20:53:49  warmerda
 * added sounding and S57_SPLIT_MULTIPOINT support
 *
 * Revision 1.6  1999/11/18 19:01:25  warmerda
 * expanded tabs
 *
 * Revision 1.5  1999/11/18 18:58:37  warmerda
 * added s57FileCollector()
 *
 * Revision 1.4  1999/11/16 21:47:32  warmerda
 * updated class occurance collection
 *
 * Revision 1.3  1999/11/08 22:23:00  warmerda
 * added object class support
 *
 * Revision 1.2  1999/11/04 21:19:13  warmerda
 * added polygon support
 *
 * Revision 1.1  1999/11/03 22:12:43  warmerda
 * New
 *
 */

#ifndef _S57_H_INCLUDED
#define _S57_H_INCLUDED

#include "ogr_feature.h"
#include "iso8211.h"

class S57Reader;

char **S57FileCollector( const char * pszDataset );

#define EMPTY_NUMBER_MARKER 2147483641  /* MAXINT-6 */

/* -------------------------------------------------------------------- */
/*      Various option strings.                                         */
/* -------------------------------------------------------------------- */
#define S57O_UPDATES "UPDATES"
#define S57O_LNAM_REFS "LNAM_REFS"
#define S57O_SPLIT_MULTIPOINT "SPLIT_MULTIPOINT"
#define S57O_ADD_SOUNDG_DEPTH "ADD_SOUNDG_DEPTH"
#define S57O_PRESERVE_EMPTY_NUMBERS "PRESERVE_EMPTY_NUMBERS"
#define S57O_RETURN_PRIMITIVES "RETURN_PRIMITIVES"

/* -------------------------------------------------------------------- */
/*      RCNM values.                                                    */
/* -------------------------------------------------------------------- */

#define RCNM_FE         100     /* Feature record */

#define RCNM_VI         110     /* Isolated Node */
#define RCNM_VC         120     /* Connected Node */
#define RCNM_VE         130     /* Edge */
#define RCNM_VF         140     /* Face */

#define OGRN_VI         "IsolatedNode"
#define OGRN_VC         "ConnectedNode"
#define OGRN_VE         "Edge"
#define OGRN_VF         "Face"

/* -------------------------------------------------------------------- */
/*      FRID PRIM values.                                               */
/* -------------------------------------------------------------------- */
#define PRIM_P          1       /* point feature */
#define PRIM_L          2       /* line feature */
#define PRIM_A          3       /* area feature */
#define PRIM_N          4       /* non-spatial feature  */

/************************************************************************/
/*                          S57ClassRegistrar                           */
/************************************************************************/

#define MAX_CLASSES 23000
#define MAX_ATTRIBUTES 25000

class S57ClassRegistrar
{
    // Class information:
    int         nClasses;
    char      **papszClassesInfo;

    int         iCurrentClass;

    char      **papszCurrentFields;

    char      **papszTempResult;

    // Attribute Information:
    int         nAttrMax;
    int         nAttrCount;
    char      **papszAttrNames;
    char      **papszAttrAcronym;
    char     ***papapszAttrValues;
    char       *pachAttrType;
    char       *pachAttrClass;
    int        *panAttrIndex; // sorted by acronym.

    int         FindFile( const char *pszTarget, const char *pszDirectory,
                          int bReportErr, FILE **fp );

    const char *ReadLine( FILE * fp );
    char      **papszNextLine;

public:
                S57ClassRegistrar();
               ~S57ClassRegistrar();

    int         LoadInfo( const char *, int );

    // class table methods.
    int         SelectClassByIndex( int );
    int         SelectClass( int );
    int         SelectClass( const char * );

    int         Rewind() { return SelectClassByIndex(0); }
    int         NextClass() { return SelectClassByIndex(iCurrentClass+1); }

    int         GetOBJL();
    const char *GetDescription();
    const char *GetAcronym();

    char      **GetAttributeList( const char * = NULL );

    char        GetClassCode();
    char      **GetPrimitives();

    // attribute table methods.
    int         GetMaxAttrIndex() { return nAttrMax; }
    const char *GetAttrName( int i ) { return papszAttrNames[i]; }
    const char *GetAttrAcronym( int i ) { return papszAttrAcronym[i]; }
    char      **GetAttrValues( int i ) { return papapszAttrValues[i]; }
    char        GetAttrType( int i ) { return pachAttrType[i]; }
#define SAT_ENUM        'E'
#define SAT_LIST        'L'
#define SAT_FLOAT       'F'
#define SAT_INT         'I'
#define SAT_CODE_STRING 'A'
#define SAT_FREE_TEXT   'S'

    char        GetAttrClass( int i ) { return pachAttrClass[i]; }
    int         FindAttrByAcronym( const char * );

};



/************************************************************************/
/*                            DDFRecordIndex                            */
/*                                                                      */
/*      Maintain an index of DDF records based on an integer key.       */
/************************************************************************/

typedef struct
{
    int         nKey;
    DDFRecord   *poRecord;
} DDFIndexedRecord;

class DDFRecordIndex
{
    int         bSorted;

    int         nRecordCount;
    int         nRecordMax;

    int         nLastObjlPos;            /* rjensen. added for FindRecordByObjl() */
    int         nLastObjl;                  /* rjensen. added for FindRecordByObjl() */

    DDFIndexedRecord *pasRecords;

    void        Sort();

public:
                DDFRecordIndex();
               ~DDFRecordIndex();

    void        AddRecord( int nKey, DDFRecord * );
    int         RemoveRecord( int nKey );

    DDFRecord  *FindRecord( int nKey );

    DDFRecord  *FindRecordByObjl( int nObjl );    /* rjensen. added for FindRecordByObjl() */

    void        Clear();

    int         GetCount() { return nRecordCount; }
    DDFRecord  *GetByIndex( int i );
};

/************************************************************************/
/*                              S57Reader                               */
/************************************************************************/

class S57Reader
{
    S57ClassRegistrar  *poRegistrar;

    int                 nFDefnCount;
    OGRFeatureDefn      **papoFDefnList;

    char                *pszModuleName;
    char                *pszDSNM;

    DDFModule           *poModule;

    int                 nCOMF;  /* Coordinate multiplier */
    int                 nSOMF;  /* Vertical (sounding) multiplier */

    int                 bFileIngested;
    DDFRecordIndex      oVI_Index;
    DDFRecordIndex      oVC_Index;
    DDFRecordIndex      oVE_Index;
    DDFRecordIndex      oVF_Index;

    int                 nNextVIIndex;
    int                 nNextVCIndex;
    int                 nNextVEIndex;
    int                 nNextVFIndex;

    int                 nNextFEIndex;
    DDFRecordIndex      oFE_Index;

    char                **papszOptions;
    int                 bGenerateLNAM;

    int                 bReturnPrimitives;
    int                 bSplitMultiPoint;
    int                 bAddSOUNDGDepth;
    int                 iPointOffset;
    OGRFeature          *poMultiPoint;

    int                 bAutoReadUpdates;
    int                 bPreserveEmptyNumbers;

    void                ClearPendingMultiPoint();
    OGRFeature         *NextPendingMultiPoint();

    OGRFeature         *AssembleFeature( DDFRecord  *, OGRFeatureDefn * );

    void                ApplyObjectClassAttributes( DDFRecord *, OGRFeature *);
    void                GenerateLNAMAndRefs( DDFRecord *, OGRFeature * );

    void                AssembleSoundingGeometry( DDFRecord *, OGRFeature * );
    void                AssemblePointGeometry( DDFRecord *, OGRFeature * );
    void                AssembleLineGeometry( DDFRecord *, OGRFeature * );
    void                AssembleAreaGeometry( DDFRecord *, OGRFeature * );

    int                 FetchPoint( int, int,
                                    double *, double *, double * = NULL );

    OGRFeatureDefn     *FindFDefn( DDFRecord * );
    int                 ParseName( DDFField *, int = 0, int * = NULL );

    void                GenerateStandardAttributes( OGRFeatureDefn * );

    int                 ApplyRecordUpdate( DDFRecord *, DDFRecord * );

    int                 bMissingWarningIssued;
    int                 bAttrWarningIssued;

  public:
                        S57Reader( const char * );
                       ~S57Reader();

    void                SetClassBased( S57ClassRegistrar * );
    void                SetOptions( char ** );

    int                 Open( int bTestOpen );
    void                Close();
    DDFModule           *GetModule() { return poModule; }
    const char          *GetDSNM() { return pszDSNM; }

    void                Ingest();
    int                 ApplyUpdates( DDFModule * );
    int                 FindAndApplyUpdates( const char *pszPath=NULL );

    void                Rewind();
    OGRFeature          *ReadNextFeature( OGRFeatureDefn * = NULL );
    OGRFeature          *ReadFeature( int nFID, OGRFeatureDefn * = NULL );
    OGRFeature          *ReadVector( int nFID, int nRCNM );

    int                 GetNextFEIndex( int nRCNM = 100 );
    void                SetNextFEIndex( int nNewIndex, int nRCNM = 100 );

    void                AddFeatureDefn( OGRFeatureDefn * );

    int                 CollectClassList( int *, int);

    OGRFeatureDefn  *GenerateGeomFeatureDefn( OGRwkbGeometryType );
    OGRFeatureDefn  *GenerateObjectClassDefn( S57ClassRegistrar *, int );
    OGRFeatureDefn  *GenerateVectorPrimitiveFeatureDefn( int );

    OGRErr              GetExtent( OGREnvelope *psExtent, int bForce );
};

#endif /* ndef _S57_H_INCLUDED */
