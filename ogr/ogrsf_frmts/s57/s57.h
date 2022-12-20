/******************************************************************************
 * $Id$
 *
 * Project:  S-57 Translator
 * Purpose:  Declarations for S-57 translator not including the
 *           binding onto OGRLayer/DataSource/Driver which are found in
 *           ogr_s57.h.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef S57_H_INCLUDED
#define S57_H_INCLUDED

#include <string>
#include <vector>
#include "ogr_feature.h"
#include "iso8211.h"

class S57Reader;

char **S57FileCollector(const char *pszDataset);

#define EMPTY_NUMBER_MARKER 2147483641 /* MAXINT-6 */

/* -------------------------------------------------------------------- */
/*      Various option strings.                                         */
/* -------------------------------------------------------------------- */
#define S57O_UPDATES "UPDATES"
#define S57O_LNAM_REFS "LNAM_REFS"
#define S57O_SPLIT_MULTIPOINT "SPLIT_MULTIPOINT"
#define S57O_ADD_SOUNDG_DEPTH "ADD_SOUNDG_DEPTH"
#define S57O_PRESERVE_EMPTY_NUMBERS "PRESERVE_EMPTY_NUMBERS"
#define S57O_RETURN_PRIMITIVES "RETURN_PRIMITIVES"
#define S57O_RETURN_LINKAGES "RETURN_LINKAGES"
#define S57O_RETURN_DSID "RETURN_DSID"
#define S57O_RECODE_BY_DSSI "RECODE_BY_DSSI"
#define S57O_LIST_AS_STRING "LIST_AS_STRING"

#define S57M_UPDATES 0x01
#define S57M_LNAM_REFS 0x02
#define S57M_SPLIT_MULTIPOINT 0x04
#define S57M_ADD_SOUNDG_DEPTH 0x08
#define S57M_PRESERVE_EMPTY_NUMBERS 0x10
#define S57M_RETURN_PRIMITIVES 0x20
#define S57M_RETURN_LINKAGES 0x40
#define S57M_RETURN_DSID 0x80
#define S57M_RECODE_BY_DSSI 0x100
#define S57M_LIST_AS_STRING 0x200

/* -------------------------------------------------------------------- */
/*      RCNM values.                                                    */
/* -------------------------------------------------------------------- */

#define RCNM_FE 100 /* Feature record */

#define RCNM_VI 110 /* Isolated Node */
#define RCNM_VC 120 /* Connected Node */
#define RCNM_VE 130 /* Edge */
#define RCNM_VF 140 /* Face */

#define RCNM_DSID 10

#define OGRN_VI "IsolatedNode"
#define OGRN_VC "ConnectedNode"
#define OGRN_VE "Edge"
#define OGRN_VF "Face"

/* -------------------------------------------------------------------- */
/*      FRID PRIM values.                                               */
/* -------------------------------------------------------------------- */
#define PRIM_P 1 /* point feature */
#define PRIM_L 2 /* line feature */
#define PRIM_A 3 /* area feature */
#define PRIM_N 4 /* non-spatial feature  */

/************************************************************************/
/*                          S57ClassRegistrar                           */
/************************************************************************/

class S57ClassContentExplorer;

class CPL_DLL S57AttrInfo
{
  public:
    CPLString osName;
    CPLString osAcronym;
    char chType;
    char chClass;
};

class CPL_DLL S57ClassRegistrar
{
    friend class S57ClassContentExplorer;

    // Class information:
    int nClasses;
    CPLStringList apszClassesInfo;

    // Attribute Information:
    int nAttrCount;
    std::vector<S57AttrInfo *> aoAttrInfos;
    std::vector<int> anAttrIndex;  // sorted by acronym.

    static bool FindFile(const char *pszTarget, const char *pszDirectory,
                         bool bReportErr, VSILFILE **fp);

    const char *ReadLine(VSILFILE *fp);
    char **papszNextLine;

  public:
    S57ClassRegistrar();
    ~S57ClassRegistrar();

    bool LoadInfo(const char *, const char *, bool);

    // attribute table methods.
    // int         GetMaxAttrIndex() { return nAttrMax; }
    const S57AttrInfo *GetAttrInfo(int i);
    const char *GetAttrName(int i)
    {
        return GetAttrInfo(i) == nullptr ? nullptr
                                         : aoAttrInfos[i]->osName.c_str();
    }
    const char *GetAttrAcronym(int i)
    {
        return GetAttrInfo(i) == nullptr ? nullptr
                                         : aoAttrInfos[i]->osAcronym.c_str();
    }
    char GetAttrType(int i)
    {
        return GetAttrInfo(i) == nullptr ? '\0' : aoAttrInfos[i]->chType;
    }
#define SAT_ENUM 'E'
#define SAT_LIST 'L'
#define SAT_FLOAT 'F'
#define SAT_INT 'I'
#define SAT_CODE_STRING 'A'
#define SAT_FREE_TEXT 'S'

    char GetAttrClass(int i)
    {
        return GetAttrInfo(i) == nullptr ? '\0' : aoAttrInfos[i]->chClass;
    }
    int FindAttrByAcronym(const char *);
};

/************************************************************************/
/*                       S57ClassContentExplorer                        */
/************************************************************************/

class S57ClassContentExplorer
{
    S57ClassRegistrar *poRegistrar;

    char ***papapszClassesFields;

    int iCurrentClass;

    char **papszCurrentFields;

    char **papszTempResult;

  public:
    explicit S57ClassContentExplorer(S57ClassRegistrar *poRegistrar);
    ~S57ClassContentExplorer();

    bool SelectClassByIndex(int);
    bool SelectClass(int);
    bool SelectClass(const char *);

    bool Rewind()
    {
        return SelectClassByIndex(0);
    }
    bool NextClass()
    {
        return SelectClassByIndex(iCurrentClass + 1);
    }

    int GetOBJL();
    const char *GetDescription() const;
    const char *GetAcronym() const;

    char **GetAttributeList(const char * = nullptr);

    char GetClassCode() const;
    char **GetPrimitives();
};

/************************************************************************/
/*                            DDFRecordIndex                            */
/*                                                                      */
/*      Maintain an index of DDF records based on an integer key.       */
/************************************************************************/

typedef struct
{
    int nKey;
    DDFRecord *poRecord;
    void *pClientData;
} DDFIndexedRecord;

class CPL_DLL DDFRecordIndex
{
    bool bSorted;

    int nRecordCount;
    int nRecordMax;

    int nLastObjlPos;  // Added for FindRecordByObjl().
    int nLastObjl;     // Added for FindRecordByObjl().

    DDFIndexedRecord *pasRecords;

    void Sort();

  public:
    DDFRecordIndex();
    ~DDFRecordIndex();

    void AddRecord(int nKey, DDFRecord *);
    bool RemoveRecord(int nKey);

    DDFRecord *FindRecord(int nKey);

    DDFRecord *FindRecordByObjl(int nObjl);  // Added for FindRecordByObjl().

    void Clear();

    int GetCount()
    {
        return nRecordCount;
    }

    DDFRecord *GetByIndex(int i);
    void *GetClientInfoByIndex(int i);
    void SetClientInfoByIndex(int i, void *pClientInfo);
};

/************************************************************************/
/*                              S57Reader                               */
/************************************************************************/

class CPL_DLL S57Reader
{
    S57ClassRegistrar *poRegistrar;
    S57ClassContentExplorer *poClassContentExplorer;

    int nFDefnCount;
    OGRFeatureDefn **papoFDefnList;

    std::vector<OGRFeatureDefn *> apoFDefnByOBJL;

    char *pszModuleName;
    char *pszDSNM;

    DDFModule *poModule;

    int nCOMF; /* Coordinate multiplier */
    int nSOMF; /* Vertical (sounding) multiplier */

    bool bFileIngested;
    DDFRecordIndex oVI_Index;
    DDFRecordIndex oVC_Index;
    DDFRecordIndex oVE_Index;
    DDFRecordIndex oVF_Index;

    int nNextVIIndex;
    int nNextVCIndex;
    int nNextVEIndex;
    int nNextVFIndex;

    int nNextFEIndex;
    DDFRecordIndex oFE_Index;

    int nNextDSIDIndex;
    DDFRecord *poDSIDRecord;
    DDFRecord *poDSPMRecord;
    std::string m_osEDTNUpdate;
    std::string m_osUPDNUpdate;
    std::string m_osISDTUpdate;

    char **papszOptions;

    int nOptionFlags;

    int iPointOffset;
    OGRFeature *poMultiPoint;

    int Aall;                // see RecodeByDSSI() function
    int Nall;                // see RecodeByDSSI() function
    bool needAallNallSetup;  // see RecodeByDSSI() function

    void ClearPendingMultiPoint();
    OGRFeature *NextPendingMultiPoint();

    OGRFeature *AssembleFeature(DDFRecord *, OGRFeatureDefn *);

    void ApplyObjectClassAttributes(DDFRecord *, OGRFeature *);
    // cppcheck-suppress functionStatic
    void GenerateLNAMAndRefs(DDFRecord *, OGRFeature *);
    void GenerateFSPTAttributes(DDFRecord *, OGRFeature *);

    void AssembleSoundingGeometry(DDFRecord *, OGRFeature *);
    // cppcheck-suppress functionStatic
    void AssemblePointGeometry(DDFRecord *, OGRFeature *);
    void AssembleLineGeometry(DDFRecord *, OGRFeature *);
    void AssembleAreaGeometry(DDFRecord *, OGRFeature *);

    bool FetchPoint(int, int, double *, double *, double * = nullptr);
    bool FetchLine(DDFRecord *, int, int, OGRLineString *);

    OGRFeatureDefn *FindFDefn(DDFRecord *);
    int ParseName(DDFField *, int = 0, int * = nullptr);

    // cppcheck-suppress functionStatic
    bool ApplyRecordUpdate(DDFRecord *, DDFRecord *);

    bool bMissingWarningIssued;
    bool bAttrWarningIssued;

  public:
    explicit S57Reader(const char *);
    ~S57Reader();

    void SetClassBased(S57ClassRegistrar *, S57ClassContentExplorer *);
    bool SetOptions(char **);
    int GetOptionFlags()
    {
        return nOptionFlags;
    }

    int Open(int bTestOpen);
    void Close();
    DDFModule *GetModule()
    {
        return poModule;
    }
    const char *GetDSNM()
    {
        return pszDSNM;
    }

    bool Ingest();
    bool ApplyUpdates(DDFModule *);
    bool FindAndApplyUpdates(const char *pszPath = nullptr);

    void Rewind();
    OGRFeature *ReadNextFeature(OGRFeatureDefn * = nullptr);
    OGRFeature *ReadFeature(int nFID, OGRFeatureDefn * = nullptr);
    OGRFeature *ReadVector(int nFID, int nRCNM);
    OGRFeature *ReadDSID();

    int GetNextFEIndex(int nRCNM = 100);
    void SetNextFEIndex(int nNewIndex, int nRCNM = 100);

    void AddFeatureDefn(OGRFeatureDefn *);

    bool CollectClassList(std::vector<int> &anClassCount);

    OGRErr GetExtent(OGREnvelope *psExtent, int bForce);

    char *RecodeByDSSI(const char *SourceString, bool LookAtAALL_NALL);
};

/************************************************************************/
/*                              S57Writer                               */
/************************************************************************/

class CPL_DLL S57Writer
{
  public:
    static const int nDEFAULT_EXPP = 1;
    static const int nDEFAULT_INTU = 4;
    static const int nDEFAULT_AGEN = 540;

    static const int nDEFAULT_HDAT = 2;
    static const int nDEFAULT_VDAT = 7;
    static const int nDEFAULT_SDAT = 23;
    static const int nDEFAULT_CSCL = 52000;
    static const int nDEFAULT_COMF = 10000000;
    static const int nDEFAULT_SOMF = 10;

    S57Writer();
    ~S57Writer();

    void SetClassBased(S57ClassRegistrar *, S57ClassContentExplorer *);
    bool CreateS57File(const char *pszFilename);
    bool Close();

    bool WriteGeometry(DDFRecord *, int, const double *, const double *,
                       const double *);
    bool WriteATTF(DDFRecord *, OGRFeature *);
    bool WritePrimitive(OGRFeature *poFeature);
    bool WriteCompleteFeature(OGRFeature *poFeature);
    bool WriteDSID(int nEXPP = nDEFAULT_EXPP, int nINTU = nDEFAULT_INTU,
                   const char *pszDSNM = nullptr, const char *pszEDTN = nullptr,
                   const char *pszUPDN = nullptr, const char *pszUADT = nullptr,
                   const char *pszISDT = nullptr, const char *pszSTED = nullptr,
                   int nAGEN = nDEFAULT_AGEN, const char *pszCOMT = nullptr,
                   int nAALL = 0, int nNALL = 0, int nNOMR = 0, int nNOGR = 0,
                   int nNOLR = 0, int nNOIN = 0, int nNOCN = 0, int nNOED = 0);
    bool WriteDSPM(int nHDAT = nDEFAULT_HDAT, int nVDAT = nDEFAULT_VDAT,
                   int nSDAT = nDEFAULT_SDAT, int nCSCL = nDEFAULT_CSCL,
                   int nCOMF = nDEFAULT_COMF, int nSOMF = nDEFAULT_SOMF);

    // semi-private - for sophisticated writers.
    DDFRecord *MakeRecord();
    DDFModule *poModule;

  private:
    int nNext0001Index;
    S57ClassRegistrar *poRegistrar;
    S57ClassContentExplorer *poClassContentExplorer;

    int m_nCOMF; /* Coordinate multiplier */
    int m_nSOMF; /* Vertical (sounding) multiplier */
};

/* -------------------------------------------------------------------- */
/*      Functions to create OGRFeatureDefns.                            */
/* -------------------------------------------------------------------- */
void CPL_DLL S57GenerateStandardAttributes(OGRFeatureDefn *, int);
OGRFeatureDefn CPL_DLL *S57GenerateGeomFeatureDefn(OGRwkbGeometryType, int);
OGRFeatureDefn CPL_DLL *
S57GenerateObjectClassDefn(S57ClassRegistrar *,
                           S57ClassContentExplorer *poClassContentExplorer, int,
                           int);
OGRFeatureDefn CPL_DLL *S57GenerateVectorPrimitiveFeatureDefn(int, int);
OGRFeatureDefn CPL_DLL *S57GenerateDSIDFeatureDefn(void);

#endif /* ndef S57_H_INCLUDED */
