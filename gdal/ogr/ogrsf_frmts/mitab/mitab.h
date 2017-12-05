/**********************************************************************
 * $Id$
 *
 * Name:     mitab.h
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Header file containing public definitions for the library.
 * Author:   Daniel Morissette, dmorissette@dmsolutions.ca
 *
 **********************************************************************
 * Copyright (c) 1999-2005, Daniel Morissette
 * Copyright (c) 2014, Even Rouault <even.rouault at spatialys.com>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#ifndef MITAB_H_INCLUDED_
#define MITAB_H_INCLUDED_

#include "mitab_priv.h"
#include "ogr_feature.h"
#include "ogr_featurestyle.h"
#include "ogrsf_frmts.h"

/*---------------------------------------------------------------------
 * Current version of the MITAB library... always useful!
 *--------------------------------------------------------------------*/
#define MITAB_VERSION      "2.0.0-dev (2008-10)"
#define MITAB_VERSION_INT  2000000  /* version x.y.z -> xxxyyyzzz */

#ifndef ROUND_INT
#  define ROUND_INT(dX) ((int)((dX) < 0.0 ? (dX)-0.5 : (dX)+0.5 ))
#endif

#define MITAB_AREA(x1, y1, x2, y2)  ((double)((x2)-(x1))*(double)((y2)-(y1)))

class TABFeature;

/*---------------------------------------------------------------------
 * Codes for the GetFileClass() in the IMapInfoFile-derived  classes
 *--------------------------------------------------------------------*/
typedef enum
{
    TABFC_IMapInfoFile = 0,
    TABFC_TABFile,
    TABFC_TABView,
    TABFC_TABSeamless,
    TABFC_MIFFile
} TABFileClass;

/*---------------------------------------------------------------------
 *                      class IMapInfoFile
 *
 * Virtual base class for the TABFile and MIFFile classes.
 *
 * This is the definition of the public interface methods that should
 * be available for any type of MapInfo dataset.
 *--------------------------------------------------------------------*/

class IMapInfoFile : public OGRLayer
{
  protected:
    GIntBig             m_nCurFeatureId;
    TABFeature         *m_poCurFeature;
    GBool               m_bBoundsSet;

    char                *m_pszCharset;

    TABFeature*         CreateTABFeature(OGRFeature *poFeature);

  public:
    IMapInfoFile() ;
    virtual ~IMapInfoFile();

    virtual TABFileClass GetFileClass() {return TABFC_IMapInfoFile;}

    virtual int Open(const char *pszFname, const char* pszAccess,
                     GBool bTestOpenNoError = FALSE,
                     const char* pszCharset = NULL );

    virtual int Open(const char *pszFname, TABAccess eAccess,
                     GBool bTestOpenNoError = FALSE,
                     const char* pszCharset = NULL ) = 0;
    virtual int Close() = 0;

    virtual int SetQuickSpatialIndexMode(CPL_UNUSED GBool bQuickSpatialIndexMode=TRUE) {return -1;}

    virtual const char *GetTableName() = 0;

    ///////////////
    // Static method to detect file type, create an object to read that
    // file and open it.
    static IMapInfoFile *SmartOpen(const char *pszFname,
                                   GBool bUpdate = FALSE,
                                   GBool bTestOpenNoError = FALSE);

    ///////////////
    //  OGR methods for read support
    virtual void        ResetReading() override = 0;
    virtual GIntBig     GetFeatureCount (int bForce) override = 0;
    virtual OGRFeature *GetNextFeature() override;
    virtual OGRFeature *GetFeature(GIntBig nFeatureId) override;
    virtual OGRErr      ICreateFeature(OGRFeature *poFeature) override;
    virtual int         TestCapability( const char * pszCap ) override =0;
    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce) override =0;
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

    ///////////////
    // Read access specific stuff
    //
    virtual GIntBig GetNextFeatureId(GIntBig nPrevId) = 0;
    virtual TABFeature *GetFeatureRef(GIntBig nFeatureId) = 0;
    virtual OGRFeatureDefn *GetLayerDefn() override = 0;

    virtual TABFieldType GetNativeFieldType(int nFieldId) = 0;

    virtual int GetBounds(double &dXMin, double &dYMin,
                          double &dXMax, double &dYMax,
                          GBool bForce = TRUE ) = 0;

    virtual OGRSpatialReference *GetSpatialRef() override = 0;

    virtual int GetFeatureCountByType(int &numPoints, int &numLines,
                                      int &numRegions, int &numTexts,
                                      GBool bForce = TRUE ) = 0;

    virtual GBool IsFieldIndexed(int nFieldId) = 0;
    virtual GBool IsFieldUnique(int nFieldId) = 0;

    ///////////////
    // Write access specific stuff
    //
    GBool       IsBoundsSet()            {return m_bBoundsSet;}
    virtual int SetBounds(double dXMin, double dYMin,
                          double dXMax, double dYMax) = 0;
    virtual int SetFeatureDefn(OGRFeatureDefn *poFeatureDefn,
                            TABFieldType *paeMapInfoNativeFieldTypes = NULL)=0;
    virtual int AddFieldNative(const char *pszName, TABFieldType eMapInfoType,
                               int nWidth=0, int nPrecision=0,
                               GBool bIndexed=FALSE, GBool bUnique=FALSE,
                               int bApproxOK = TRUE) = 0;
    virtual OGRErr CreateField( OGRFieldDefn *poField, int bApproxOK = TRUE ) override;

    virtual int SetSpatialRef(OGRSpatialReference *poSpatialRef) = 0;

    virtual OGRErr CreateFeature(TABFeature *poFeature) = 0;

    virtual int SetFieldIndexed(int nFieldId) = 0;

    virtual int SetCharset(const char* charset);

    virtual const char* GetCharset() const;

    static const char* CharsetToEncoding( const char* );
    static const char* EncodingToCharset( const char* );

    void SetEncoding( const char* );
    const char* GetEncoding() const;
    int TestUtf8Capability() const;
    ///////////////
    // semi-private.
    virtual int  GetProjInfo(TABProjInfo *poPI) = 0;
    virtual int  SetProjInfo(TABProjInfo *poPI) = 0;
    virtual int  SetMIFCoordSys(const char *pszMIFCoordSys) = 0;

    static int GetTABType( OGRFieldDefn *poField, TABFieldType* peTABType,
                           int *pnWidth, int *pnPrecision);

#ifdef DEBUG
    virtual void Dump(FILE *fpOut = NULL) = 0;
#endif
};

/*---------------------------------------------------------------------
 *                      class TABFile
 *
 * The main class for TAB datasets.  External programs should use this
 * class to open a TAB dataset and read/write features from/to it.
 *
 *--------------------------------------------------------------------*/
class TABFile CPL_FINAL : public IMapInfoFile
{
  private:
    char        *m_pszFname;
    TABAccess   m_eAccessMode;
    char        **m_papszTABFile;
    int         m_nVersion;
    int         *m_panIndexNo;
    TABTableType m_eTableType;  // NATIVE (.DAT) or DBF

    TABDATFile  *m_poDATFile;   // Attributes file
    TABMAPFile  *m_poMAPFile;   // Object Geometry file
    TABINDFile  *m_poINDFile;   // Attributes index file

    OGRFeatureDefn *m_poDefn;
    OGRSpatialReference *m_poSpatialRef;
    int         bUseSpatialTraversal;

    int         m_nLastFeatureId;

    GIntBig    *m_panMatchingFIDs;
    int         m_iMatchingFID;

    int         m_bNeedTABRewrite;

    int         m_bLastOpWasRead;
    int         m_bLastOpWasWrite;
    ///////////////
    // Private Read access specific stuff
    //
    int         ParseTABFileFirstPass(GBool bTestOpenNoError);
    int         ParseTABFileFields();

     ///////////////
    // Private Write access specific stuff
    //
    int         WriteTABFile();

  public:
    TABFile();
    virtual ~TABFile();

    virtual TABFileClass GetFileClass() override {return TABFC_TABFile;}

    virtual int Open(const char *pszFname, const char* pszAccess,
                     GBool bTestOpenNoError = FALSE,
                     const char* pszCharset = NULL ) override
            { return IMapInfoFile::Open(pszFname, pszAccess, bTestOpenNoError, pszCharset); }
    virtual int Open(const char *pszFname, TABAccess eAccess,
                     GBool bTestOpenNoError = FALSE,
                     const char* pszCharset = NULL ) override
            { return Open(pszFname, eAccess, bTestOpenNoError, 512, pszCharset); }

    virtual int Open(const char *pszFname, TABAccess eAccess,
                     GBool bTestOpenNoError,
                     int nBlockSizeForCreate,
                     const char* pszCharset );

    virtual int Close() override;

    virtual int SetQuickSpatialIndexMode(GBool bQuickSpatialIndexMode=TRUE) override;

    virtual const char *GetTableName() override
                            {return m_poDefn?m_poDefn->GetName():"";}

    virtual void        ResetReading() override;
    virtual int         TestCapability( const char * pszCap ) override;
    virtual GIntBig     GetFeatureCount (int bForce) override;
    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce) override;
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

    /* Implement OGRLayer's SetFeature() for random write, only with TABFile */
    virtual OGRErr      ISetFeature( OGRFeature * ) override;
    virtual OGRErr      DeleteFeature(GIntBig nFeatureId) override;

    virtual OGRErr      DeleteField( int iField ) override;
    virtual OGRErr      ReorderFields( int* panMap ) override;
    virtual OGRErr      AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlags ) override;

    virtual OGRErr      SyncToDisk() override;

    ///////////////
    // Read access specific stuff
    //

    int         GetNextFeatureId_Spatial( int nPrevId );

    virtual GIntBig GetNextFeatureId(GIntBig nPrevId) override;
    virtual TABFeature *GetFeatureRef(GIntBig nFeatureId) override;
    virtual OGRFeatureDefn *GetLayerDefn() override;

    virtual TABFieldType GetNativeFieldType(int nFieldId) override;

    virtual int GetBounds(double &dXMin, double &dYMin,
                          double &dXMax, double &dYMax,
                          GBool bForce = TRUE ) override;

    virtual OGRSpatialReference *GetSpatialRef() override;

    static OGRSpatialReference* GetSpatialRefFromTABProj(const TABProjInfo& sTABProj);
    static int                  GetTABProjFromSpatialRef(const OGRSpatialReference* poSpatialRef,
                                                         TABProjInfo& sTABProj, int& nParmCount);

    virtual int GetFeatureCountByType(int &numPoints, int &numLines,
                                      int &numRegions, int &numTexts,
                                      GBool bForce = TRUE) override;

    virtual GBool IsFieldIndexed(int nFieldId) override;
    virtual GBool IsFieldUnique(int /*nFieldId*/) override   {return FALSE;}

    virtual int GetVersion() { return m_nVersion; }

    ///////////////
    // Write access specific stuff
    //
    virtual int SetBounds(double dXMin, double dYMin,
                          double dXMax, double dYMax) override;
    virtual int SetFeatureDefn(OGRFeatureDefn *poFeatureDefn,
                            TABFieldType *paeMapInfoNativeFieldTypes = NULL) override;
    virtual int AddFieldNative(const char *pszName, TABFieldType eMapInfoType,
                               int nWidth=0, int nPrecision=0,
                               GBool bIndexed=FALSE, GBool bUnique=FALSE,
                               int bApproxOK = TRUE) override;
    virtual int SetSpatialRef(OGRSpatialReference *poSpatialRef) override;

    virtual OGRErr CreateFeature(TABFeature *poFeature) override;

    virtual int SetFieldIndexed(int nFieldId) override;

    ///////////////
    // semi-private.
    virtual int  GetProjInfo(TABProjInfo *poPI) override
            { return m_poMAPFile->GetHeaderBlock()->GetProjInfo( poPI ); }
    virtual int  SetProjInfo(TABProjInfo *poPI) override;
    virtual int  SetMIFCoordSys(const char *pszMIFCoordSys) override;

    int         GetFieldIndexNumber(int nFieldId);
    TABINDFile  *GetINDFileRef();

    TABMAPFile  *GetMAPFileRef() { return m_poMAPFile; }

    int         WriteFeature(TABFeature *poFeature);
    virtual int SetCharset(const char* pszCharset) override;
#ifdef DEBUG
    virtual void Dump(FILE *fpOut = NULL) override;
#endif
};

/*---------------------------------------------------------------------
 *                      class TABView
 *
 * TABView is used to handle special type of .TAB files that are
 * composed of a number of .TAB datasets linked through some indexed
 * fields.
 *
 * NOTE: The current implementation supports only TABViews composed
 *       of 2 TABFiles linked through an indexed field of integer type.
 *       It is unclear if any other type of views could exist anyways.
 *--------------------------------------------------------------------*/
class TABView CPL_FINAL : public IMapInfoFile
{
  private:
    char        *m_pszFname;
    TABAccess   m_eAccessMode;
    char        **m_papszTABFile;
    char        *m_pszVersion;

    char        **m_papszTABFnames;
    TABFile     **m_papoTABFiles;
    int         m_numTABFiles;
    int         m_nMainTableIndex; // The main table is the one that also
                                   // contains the geometries
    char        **m_papszFieldNames;
    char        **m_papszWhereClause;

    TABRelation *m_poRelation;
    GBool       m_bRelFieldsCreated;

    ///////////////
    // Private Read access specific stuff
    //
    int         ParseTABFile(const char *pszDatasetPath,
                             GBool bTestOpenNoError = FALSE);

    int         OpenForRead(const char *pszFname,
                            GBool bTestOpenNoError = FALSE );

    ///////////////
    // Private Write access specific stuff
    //
    int         OpenForWrite(const char *pszFname );
    int         WriteTABFile();

  public:
    TABView();
    virtual ~TABView();

    virtual TABFileClass GetFileClass() override {return TABFC_TABView;}

    virtual int Open(const char *pszFname, const char* pszAccess,
                     GBool bTestOpenNoError = FALSE,
                     const char* pszCharset = NULL ) override { return IMapInfoFile::Open(pszFname, pszAccess, bTestOpenNoError, pszCharset); }
    virtual int Open(const char *pszFname, TABAccess eAccess,
                     GBool bTestOpenNoError = FALSE,
                     const char* pszCharset = NULL ) override;
    virtual int Close() override;

    virtual int SetQuickSpatialIndexMode(GBool bQuickSpatialIndexMode=TRUE) override;

    virtual const char *GetTableName() override
           {return m_poRelation?m_poRelation->GetFeatureDefn()->GetName():"";}

    virtual void        ResetReading() override;
    virtual int         TestCapability( const char * pszCap ) override;
    virtual GIntBig     GetFeatureCount (int bForce) override;
    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce) override;
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

    ///////////////
    // Read access specific stuff
    //

    virtual GIntBig GetNextFeatureId(GIntBig nPrevId) override;
    virtual TABFeature *GetFeatureRef(GIntBig nFeatureId) override;
    virtual OGRFeatureDefn *GetLayerDefn() override;

    virtual TABFieldType GetNativeFieldType(int nFieldId) override;

    virtual int GetBounds(double &dXMin, double &dYMin,
                          double &dXMax, double &dYMax,
                          GBool bForce = TRUE ) override;

    virtual OGRSpatialReference *GetSpatialRef() override;

    virtual int GetFeatureCountByType(int &numPoints, int &numLines,
                                      int &numRegions, int &numTexts,
                                      GBool bForce = TRUE) override;

    virtual GBool IsFieldIndexed(int nFieldId) override;
    virtual GBool IsFieldUnique(int nFieldId) override;

    ///////////////
    // Write access specific stuff
    //
    virtual int SetBounds(double dXMin, double dYMin,
                          double dXMax, double dYMax) override;
    virtual int SetFeatureDefn(OGRFeatureDefn *poFeatureDefn,
                           TABFieldType *paeMapInfoNativeFieldTypes=NULL) override;
    virtual int AddFieldNative(const char *pszName,
                               TABFieldType eMapInfoType,
                               int nWidth=0, int nPrecision=0,
                               GBool bIndexed=FALSE, GBool bUnique=FALSE,
                               int bApproxOK = TRUE) override;
    virtual int SetSpatialRef(OGRSpatialReference *poSpatialRef) override;

    virtual OGRErr CreateFeature(TABFeature *poFeature) override;

    virtual int SetFieldIndexed(int nFieldId) override;

    ///////////////
    // semi-private.
    virtual int  GetProjInfo(TABProjInfo *poPI) override
            { return m_nMainTableIndex!=-1?
                     m_papoTABFiles[m_nMainTableIndex]->GetProjInfo(poPI):-1; }
    virtual int  SetProjInfo(TABProjInfo *poPI) override
            { return m_nMainTableIndex!=-1?
                     m_papoTABFiles[m_nMainTableIndex]->SetProjInfo(poPI):-1; }
    virtual int  SetMIFCoordSys(const char * /*pszMIFCoordSys*/) override {return -1;}
    virtual int SetCharset(const char* pszCharset) override;

#ifdef DEBUG
    virtual void Dump(FILE *fpOut = NULL) override;
#endif
};

/*---------------------------------------------------------------------
 *                      class TABSeamless
 *
 * TABSeamless is used to handle seamless .TAB files that are
 * composed of a main .TAB file in which each feature is the MBR of
 * a base table.
 *
 * TABSeamless are supported for read access only.
 *--------------------------------------------------------------------*/
class TABSeamless CPL_FINAL : public IMapInfoFile
{
  private:
    char        *m_pszFname;
    char        *m_pszPath;
    TABAccess   m_eAccessMode;
    OGRFeatureDefn *m_poFeatureDefnRef;

    TABFile     *m_poIndexTable;
    int         m_nTableNameField;
    int         m_nCurBaseTableId;
    TABFile     *m_poCurBaseTable;
    GBool       m_bEOF;

    ///////////////
    // Private Read access specific stuff
    //
    int         OpenForRead(const char *pszFname,
                            GBool bTestOpenNoError = FALSE );
    int         OpenBaseTable(TABFeature *poIndexFeature,
                              GBool bTestOpenNoError = FALSE);
    int         OpenBaseTable(int nTableId, GBool bTestOpenNoError = FALSE);
    int         OpenNextBaseTable(GBool bTestOpenNoError =FALSE);
    static GIntBig     EncodeFeatureId(int nTableId, int nBaseFeatureId);
    static int         ExtractBaseTableId(GIntBig nEncodedFeatureId);
    static int         ExtractBaseFeatureId(GIntBig nEncodedFeatureId);

  public:
    TABSeamless();
    virtual ~TABSeamless();

    virtual TABFileClass GetFileClass() override {return TABFC_TABSeamless;}

    virtual int Open(const char *pszFname, const char* pszAccess,
                     GBool bTestOpenNoError = FALSE,
                     const char* pszCharset = NULL ) override { return IMapInfoFile::Open(pszFname, pszAccess, bTestOpenNoError, pszCharset); }
    virtual int Open(const char *pszFname, TABAccess eAccess,
                     GBool bTestOpenNoError = FALSE,
                     const char* pszCharset = NULL ) override;
    virtual int Close() override;

    virtual const char *GetTableName() override
           {return m_poFeatureDefnRef?m_poFeatureDefnRef->GetName():"";}

    virtual void        SetSpatialFilter( OGRGeometry * ) override;
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom ) override
                { OGRLayer::SetSpatialFilter(iGeomField, poGeom); }

    virtual void        ResetReading() override;
    virtual int         TestCapability( const char * pszCap ) override;
    virtual GIntBig     GetFeatureCount (int bForce) override;
    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce) override;
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

    ///////////////
    // Read access specific stuff
    //

    virtual GIntBig GetNextFeatureId(GIntBig nPrevId) override;
    virtual TABFeature *GetFeatureRef(GIntBig nFeatureId) override;
    virtual OGRFeatureDefn *GetLayerDefn() override;

    virtual TABFieldType GetNativeFieldType(int nFieldId) override;

    virtual int GetBounds(double &dXMin, double &dYMin,
                          double &dXMax, double &dYMax,
                          GBool bForce = TRUE ) override;

    virtual OGRSpatialReference *GetSpatialRef() override;

    virtual int GetFeatureCountByType(int &numPoints, int &numLines,
                                      int &numRegions, int &numTexts,
                                      GBool bForce = TRUE) override;

    virtual GBool IsFieldIndexed(int nFieldId) override;
    virtual GBool IsFieldUnique(int nFieldId) override;

    ///////////////
    // Write access specific stuff
    //
    virtual int SetBounds(CPL_UNUSED double dXMin, CPL_UNUSED double dYMin,
                          CPL_UNUSED double dXMax, CPL_UNUSED double dYMax) override   {return -1;}
    virtual int SetFeatureDefn(CPL_UNUSED OGRFeatureDefn *poFeatureDefn,
                               CPL_UNUSED TABFieldType *paeMapInfoNativeFieldTypes=NULL) override
                                                        {return -1;}
    virtual int AddFieldNative(CPL_UNUSED const char *pszName,
                               CPL_UNUSED TABFieldType eMapInfoType,
                               CPL_UNUSED int nWidth=0,
                               CPL_UNUSED int nPrecision=0,
                               CPL_UNUSED GBool bIndexed=FALSE,
                               CPL_UNUSED GBool bUnique=FALSE,
                               CPL_UNUSED int bApproxOK = TRUE) override {return -1;}

    virtual int SetSpatialRef(CPL_UNUSED OGRSpatialReference *poSpatialRef) override {return -1;}

    virtual OGRErr CreateFeature(CPL_UNUSED TABFeature *poFeature) override
                                        {return OGRERR_UNSUPPORTED_OPERATION;}

    virtual int SetFieldIndexed(CPL_UNUSED int nFieldId) override {return -1;}

    ///////////////
    // semi-private.
    virtual int  GetProjInfo(TABProjInfo *poPI) override
            { return m_poIndexTable?m_poIndexTable->GetProjInfo(poPI):-1; }
    virtual int SetProjInfo(CPL_UNUSED TABProjInfo *poPI) override         { return -1; }
    virtual int SetMIFCoordSys(const char * /*pszMIFCoordSys*/) override {return -1;}

#ifdef DEBUG
    virtual void Dump(FILE *fpOut = NULL) override;
#endif
};

/*---------------------------------------------------------------------
 *                      class MIFFile
 *
 * The main class for (MID/MIF) datasets.  External programs should use this
 * class to open a (MID/MIF) dataset and read/write features from/to it.
 *
 *--------------------------------------------------------------------*/
class MIFFile CPL_FINAL : public IMapInfoFile
{
  private:
    char        *m_pszFname;
    TABAccess    m_eAccessMode;
    int          m_nVersion;   /* Dataset version: 300, 450, 600, 900, etc. */
    char        *m_pszDelimiter;
    char        *m_pszUnique;
    char        *m_pszIndex;
    char        *m_pszCoordSys;

    TABFieldType *m_paeFieldType;
    GBool       *m_pabFieldIndexed;
    GBool       *m_pabFieldUnique;

    double       m_dfXMultiplier;
    double       m_dfYMultiplier;
    double       m_dfXDisplacement;
    double       m_dfYDisplacement;

    /* these are the projection bounds, possibly much broader than extents */
    double      m_dXMin;
    double      m_dYMin;
    double      m_dXMax;
    double      m_dYMax;

    /* extents, as cached by MIFFile::PreParseFile() */
    int         m_bExtentsSet;
    OGREnvelope m_sExtents;

    int         m_nPoints;
    int         m_nLines;
    int         m_nRegions;
    int         m_nTexts;

    int         m_nPreloadedId;  // preloaded mif line is for this feature id
    MIDDATAFile  *m_poMIDFile;   // Mid file
    MIDDATAFile  *m_poMIFFile;   // Mif File

    OGRFeatureDefn *m_poDefn;
    OGRSpatialReference *m_poSpatialRef;

    int         m_nFeatureCount;
    int         m_nWriteFeatureId;
    int         m_nAttribute;

    ///////////////
    // Private Read access specific stuff
    //
    int         ReadFeatureDefn();
    int         ParseMIFHeader(int* pbIsEmpty);
    void        PreParseFile();
    int         AddFields(const char *pszLine);
    int         GotoFeature(int nFeatureId);
    int         NextFeature();

    ///////////////
    // Private Write access specific stuff
    //
    GBool       m_bPreParsed;
    GBool       m_bHeaderWrote;

    int         WriteMIFHeader();
    void UpdateExtents(double dfX,double dfY);

  public:
    MIFFile();
    virtual ~MIFFile();

    virtual TABFileClass GetFileClass() override {return TABFC_MIFFile;}

    virtual int Open(const char *pszFname, const char* pszAccess,
                     GBool bTestOpenNoError = FALSE,
                     const char* pszCharset = NULL ) override { return IMapInfoFile::Open(pszFname, pszAccess, bTestOpenNoError, pszCharset); }
    virtual int Open(const char *pszFname, TABAccess eAccess,
                     GBool bTestOpenNoError = FALSE,
                     const char* pszCharset = NULL ) override;
    virtual int Close() override;

    virtual const char *GetTableName() override
                           {return m_poDefn?m_poDefn->GetName():"";}

    virtual int         TestCapability( const char * pszCap ) override ;
    virtual GIntBig     GetFeatureCount (int bForce) override;
    virtual void        ResetReading() override;
    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce) override;
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

    ///////////////
    // Read access specific stuff
    //

    virtual GIntBig GetNextFeatureId(GIntBig nPrevId) override;
    virtual TABFeature *GetFeatureRef(GIntBig nFeatureId) override;
    virtual OGRFeatureDefn *GetLayerDefn() override;

    virtual TABFieldType GetNativeFieldType(int nFieldId) override;

    virtual int GetBounds(double &dXMin, double &dYMin,
                          double &dXMax, double &dYMax,
                          GBool bForce = TRUE ) override;

    virtual OGRSpatialReference *GetSpatialRef() override;

    virtual int GetFeatureCountByType(int &numPoints, int &numLines,
                                      int &numRegions, int &numTexts,
                                      GBool bForce = TRUE) override;

    virtual GBool IsFieldIndexed(int nFieldId) override;
    virtual GBool IsFieldUnique(int nFieldId) override;

    virtual int GetVersion() { return m_nVersion; }

    ///////////////
    // Write access specific stuff
    //
    virtual int SetBounds(double dXMin, double dYMin,
                          double dXMax, double dYMax) override;
    virtual int SetFeatureDefn(OGRFeatureDefn *poFeatureDefn,
                            TABFieldType *paeMapInfoNativeFieldTypes = NULL) override;
    virtual int AddFieldNative(const char *pszName, TABFieldType eMapInfoType,
                               int nWidth=0, int nPrecision=0,
                               GBool bIndexed=FALSE, GBool bUnique=FALSE,
                               int bApproxOK = TRUE) override;
    /* TODO */
    virtual int SetSpatialRef(OGRSpatialReference *poSpatialRef) override;

    virtual OGRErr CreateFeature(TABFeature *poFeature) override;

    virtual int SetFieldIndexed(int nFieldId) override;

    ///////////////
    // semi-private.
    virtual int  GetProjInfo(TABProjInfo * /*poPI*/) override{return -1;}
    /*  { return m_poMAPFile->GetHeaderBlock()->GetProjInfo( poPI ); }*/
    virtual int  SetProjInfo(TABProjInfo * /*poPI*/) override{return -1;}
    /*  { return m_poMAPFile->GetHeaderBlock()->SetProjInfo( poPI ); }*/
    virtual int  SetMIFCoordSys(const char * pszMIFCoordSys) override;
    virtual int SetCharset(const char* pszCharset) override;

#ifdef DEBUG
    virtual void Dump(FILE * /*fpOut*/ = NULL) override {}
#endif
};

/*---------------------------------------------------------------------
 * Define some error codes specific to this lib.
 *--------------------------------------------------------------------*/
#define TAB_WarningFeatureTypeNotSupported     501
#define TAB_WarningInvalidFieldName            502
#define TAB_WarningBoundsOverflow              503

/*---------------------------------------------------------------------
 * Codes for the feature classes
 *--------------------------------------------------------------------*/
typedef enum
{
    TABFCNoGeomFeature = 0,
    TABFCPoint = 1,
    TABFCFontPoint = 2,
    TABFCCustomPoint = 3,
    TABFCText = 4,
    TABFCPolyline = 5,
    TABFCArc = 6,
    TABFCRegion = 7,
    TABFCRectangle = 8,
    TABFCEllipse = 9,
    TABFCMultiPoint = 10,
    TABFCCollection = 11,
    TABFCDebugFeature
} TABFeatureClass;

/*---------------------------------------------------------------------
 * Definitions for text attributes
 *--------------------------------------------------------------------*/
typedef enum TABTextJust_t
{
    TABTJLeft = 0,      // Default: Left Justification
    TABTJCenter,
    TABTJRight
} TABTextJust;

typedef enum TABTextSpacing_t
{
    TABTSSingle = 0,    // Default: Single spacing
    TABTS1_5,           // 1.5
    TABTSDouble
} TABTextSpacing;

typedef enum TABTextLineType_t
{
    TABTLNoLine = 0,    // Default: No line
    TABTLSimple,
    TABTLArrow
} TABTextLineType;

typedef enum TABFontStyle_t     // Can be OR'ed
{                               // except box and halo are mutually exclusive
    TABFSNone       = 0,
    TABFSBold       = 0x0001,
    TABFSItalic     = 0x0002,
    TABFSUnderline  = 0x0004,
    TABFSStrikeout  = 0x0008,
    TABFSOutline    = 0x0010,
    TABFSShadow     = 0x0020,
    TABFSInverse    = 0x0040,
    TABFSBlink      = 0x0080,
    TABFSBox        = 0x0100,   // See note about box vs halo below.
    TABFSHalo       = 0x0200,   // MIF uses 256, see MIF docs, App.A
    TABFSAllCaps    = 0x0400,   // MIF uses 512
    TABFSExpanded   = 0x0800    // MIF uses 1024
} TABFontStyle;

/* TABFontStyle enum notes:
 *
 * The enumeration values above correspond to the values found in a .MAP
 * file. However, they differ a little from what is found in a MIF file:
 * Values 0x01 to 0x80 are the same in .MIF and .MAP files.
 * Values 0x200 to 0x800 in .MAP are 0x100 to 0x400 in .MIF
 *
 * What about TABFSBox (0x100) ?
 * TABFSBox is stored just like the other styles in .MAP files but it is not
 * explicitly stored in a MIF file.
 * If a .MIF FONT() clause contains the optional BG color, then this implies
 * that either Halo or Box was set.  Thus if TABFSHalo (value 256 in MIF)
 * is not set in the style, then this implies that TABFSBox should be set.
 */

typedef enum TABCustSymbStyle_t // Can be OR'ed
{
    TABCSNone       = 0,        // Transparent BG, use default colors
    TABCSBGOpaque   = 0x01,     // White pixels are opaque
    TABCSApplyColor = 0x02      // non-white pixels drawn using symbol color
} TABCustSymbStyle;

/*=====================================================================
  Base classes to be used to add supported drawing tools to each feature type
 =====================================================================*/

class ITABFeaturePen
{
  protected:
    int         m_nPenDefIndex;
    TABPenDef   m_sPenDef;
  public:
    ITABFeaturePen();
    ~ITABFeaturePen() {}
    int         GetPenDefIndex() {return m_nPenDefIndex;}
    TABPenDef  *GetPenDefRef() {return &m_sPenDef;}

    GByte       GetPenWidthPixel();
    double      GetPenWidthPoint();
    int         GetPenWidthMIF();
    GByte       GetPenPattern() {return m_sPenDef.nLinePattern;}
    GInt32      GetPenColor()   {return m_sPenDef.rgbColor;}

    void        SetPenWidthPixel(GByte val);
    void        SetPenWidthPoint(double val);
    void        SetPenWidthMIF(int val);

    void        SetPenPattern(GByte val) {m_sPenDef.nLinePattern=val;}
    void        SetPenColor(GInt32 clr)  {m_sPenDef.rgbColor = clr;}

    const char *GetPenStyleString();
    void        SetPenFromStyleString(const char *pszStyleString);

    void        DumpPenDef(FILE *fpOut = NULL);
};

class ITABFeatureBrush
{
  protected:
    int         m_nBrushDefIndex;
    TABBrushDef m_sBrushDef;
  public:
    ITABFeatureBrush();
    ~ITABFeatureBrush() {}
    int         GetBrushDefIndex() {return m_nBrushDefIndex;}
    TABBrushDef *GetBrushDefRef() {return &m_sBrushDef;}

    GInt32      GetBrushFGColor()     {return m_sBrushDef.rgbFGColor;}
    GInt32      GetBrushBGColor()     {return m_sBrushDef.rgbBGColor;}
    GByte       GetBrushPattern()     {return m_sBrushDef.nFillPattern;}
    GByte       GetBrushTransparent() {return m_sBrushDef.bTransparentFill;}

    void        SetBrushFGColor(GInt32 clr)  { m_sBrushDef.rgbFGColor = clr;}
    void        SetBrushBGColor(GInt32 clr)  { m_sBrushDef.rgbBGColor = clr;}
    void        SetBrushPattern(GByte val)   { m_sBrushDef.nFillPattern=val;}
    void        SetBrushTransparent(GByte val)
                                          {m_sBrushDef.bTransparentFill=val;}

    const char *GetBrushStyleString();
    void        SetBrushFromStyleString(const char *pszStyleString);

    void        DumpBrushDef(FILE *fpOut = NULL);
};

class ITABFeatureFont
{
  protected:
    int         m_nFontDefIndex;
    TABFontDef  m_sFontDef;
  public:
    ITABFeatureFont();
    ~ITABFeatureFont() {}
    int         GetFontDefIndex() {return m_nFontDefIndex;}
    TABFontDef *GetFontDefRef() {return &m_sFontDef;}

    const char *GetFontNameRef() {return m_sFontDef.szFontName;}

    void        SetFontName(const char *pszName);

    void        DumpFontDef(FILE *fpOut = NULL);
};

class ITABFeatureSymbol
{
  protected:
    int         m_nSymbolDefIndex;
    TABSymbolDef m_sSymbolDef;
  public:
    ITABFeatureSymbol();
    ~ITABFeatureSymbol() {}
    int         GetSymbolDefIndex() {return m_nSymbolDefIndex;}
    TABSymbolDef *GetSymbolDefRef() {return &m_sSymbolDef;}

    GInt16      GetSymbolNo()    {return m_sSymbolDef.nSymbolNo;}
    GInt16      GetSymbolSize()  {return m_sSymbolDef.nPointSize;}
    GInt32      GetSymbolColor() {return m_sSymbolDef.rgbColor;}

    void        SetSymbolNo(GInt16 val)     { m_sSymbolDef.nSymbolNo = val;}
    void        SetSymbolSize(GInt16 val)   { m_sSymbolDef.nPointSize = val;}
    void        SetSymbolColor(GInt32 clr)  { m_sSymbolDef.rgbColor = clr;}

    const char *GetSymbolStyleString(double dfAngle = 0.0);
    void        SetSymbolFromStyleString(const char *pszStyleString);

    void        DumpSymbolDef(FILE *fpOut = NULL);
};

/*=====================================================================
                        Feature Classes
 =====================================================================*/

/*---------------------------------------------------------------------
 *                      class TABFeature
 *
 * Extend the OGRFeature to support MapInfo specific extensions related
 * to geometry types, representation strings, etc.
 *
 * TABFeature will be used as a base class for all the feature classes.
 *
 * This class will also be used to instantiate objects with no Geometry
 * (i.e. type TAB_GEOM_NONE) which is a valid case in MapInfo.
 *
 * The logic to read/write the object from/to the .DAT and .MAP files is also
 * implemented as part of this class and derived classes.
 *--------------------------------------------------------------------*/
class TABFeature : public OGRFeature
{
  protected:
    TABGeomType m_nMapInfoType;

    double      m_dXMin;
    double      m_dYMin;
    double      m_dXMax;
    double      m_dYMax;

    GBool       m_bDeletedFlag;

    void        CopyTABFeatureBase(TABFeature *poDestFeature);

    // Compr. Origin is set for TAB files by ValidateCoordType()
    GInt32      m_nXMin;
    GInt32      m_nYMin;
    GInt32      m_nXMax;
    GInt32      m_nYMax;
    GInt32      m_nComprOrgX;
    GInt32      m_nComprOrgY;

    virtual int UpdateMBR(TABMAPFile *poMapFile = NULL);

  public:
    explicit TABFeature(OGRFeatureDefn *poDefnIn );
    virtual ~TABFeature();

    static  TABFeature     *CreateFromMapInfoType(int nMapInfoType,
                                                  OGRFeatureDefn *poDefn);

    virtual TABFeature     *CloneTABFeature(OGRFeatureDefn *pNewDefn = NULL);
    virtual TABFeatureClass GetFeatureClass() { return TABFCNoGeomFeature; }
    virtual TABGeomType     GetMapInfoType()  { return m_nMapInfoType; }
    virtual TABGeomType     ValidateMapInfoType(CPL_UNUSED TABMAPFile *poMapFile = NULL)
                                                {m_nMapInfoType=TAB_GEOM_NONE;
                                                 return m_nMapInfoType;}
    GBool       IsRecordDeleted() { return m_bDeletedFlag; }
    void        SetRecordDeleted(GBool bDeleted) { m_bDeletedFlag=bDeleted; }

    /*-----------------------------------------------------------------
     * TAB Support
     *----------------------------------------------------------------*/

    virtual int ReadRecordFromDATFile(TABDATFile *poDATFile);
    virtual int ReadGeometryFromMAPFile(TABMAPFile *poMapFile, TABMAPObjHdr *,
                                        GBool bCoordDataOnly=FALSE,
                                        TABMAPCoordBlock **ppoCoordBlock=NULL);

    virtual int WriteRecordToDATFile(TABDATFile *poDATFile,
                                     TABINDFile *poINDFile, int *panIndexNo);
    virtual int WriteGeometryToMAPFile(TABMAPFile *poMapFile, TABMAPObjHdr *,
                                       GBool bCoordDataOnly=FALSE,
                                       TABMAPCoordBlock **ppoCoordBlock=NULL);
    GBool       ValidateCoordType(TABMAPFile * poMapFile);
    void        ForceCoordTypeAndOrigin(TABGeomType nMapInfoType, GBool bCompr,
                                        GInt32 nComprOrgX, GInt32 nComprOrgY,
                                        GInt32 nXMin, GInt32 nYMin,
                                        GInt32 nXMax, GInt32 nYMax);

    /*-----------------------------------------------------------------
     * Mid/Mif Support
     *----------------------------------------------------------------*/

    virtual int ReadRecordFromMIDFile(MIDDATAFile *fp);
    virtual int ReadGeometryFromMIFFile(MIDDATAFile *fp);

    virtual int WriteRecordToMIDFile(MIDDATAFile *fp);
    virtual int WriteGeometryToMIFFile(MIDDATAFile *fp);

    void ReadMIFParameters(MIDDATAFile *fp);
    void WriteMIFParameters(MIDDATAFile *fp);

    /*-----------------------------------------------------------------
     *----------------------------------------------------------------*/

    void        SetMBR(double dXMin, double dYMin,
                       double dXMax, double dYMax);
    void        GetMBR(double &dXMin, double &dYMin,
                       double &dXMax, double &dYMax);
    void        SetIntMBR(GInt32 nXMin, GInt32 nYMin,
                          GInt32 nXMax, GInt32 nYMax);
    void        GetIntMBR(GInt32 &nXMin, GInt32 &nYMin,
                          GInt32 &nXMax, GInt32 &nYMax);

    virtual void DumpMID(FILE *fpOut = NULL);
    virtual void DumpMIF(FILE *fpOut = NULL);
};

/*---------------------------------------------------------------------
 *                      class TABPoint
 *
 * Feature class to handle old style MapInfo point symbols:
 *
 *     TAB_GEOM_SYMBOL_C        0x01
 *     TAB_GEOM_SYMBOL          0x02
 *
 * Feature geometry will be a OGRPoint
 *
 * The symbol number is in the range [31..67], with 31=None and corresponds
 * to one of the 35 predefined "Old MapInfo Symbols"
 *
 * NOTE: This class is also used as a base class for the other point
 * symbol types TABFontPoint and TABCustomPoint.
 *--------------------------------------------------------------------*/
class TABPoint: public TABFeature,
                public ITABFeatureSymbol
{
  public:
    explicit TABPoint(OGRFeatureDefn *poDefnIn);
    virtual ~TABPoint();

    virtual TABFeatureClass GetFeatureClass() override { return TABFCPoint; }
    virtual TABGeomType     ValidateMapInfoType(TABMAPFile *poMapFile = NULL) override;

    virtual TABFeature *CloneTABFeature(OGRFeatureDefn *poNewDefn = NULL ) override;

    double      GetX();
    double      GetY();

    virtual int ReadGeometryFromMAPFile(TABMAPFile *poMapFile, TABMAPObjHdr *,
                                        GBool bCoordDataOnly=FALSE,
                                        TABMAPCoordBlock **ppoCoordBlock=NULL) override;
    virtual int WriteGeometryToMAPFile(TABMAPFile *poMapFile, TABMAPObjHdr *,
                                       GBool bCoordDataOnly=FALSE,
                                       TABMAPCoordBlock **ppoCoordBlock=NULL) override;

    virtual int ReadGeometryFromMIFFile(MIDDATAFile *fp) override;
    virtual int WriteGeometryToMIFFile(MIDDATAFile *fp) override;

    virtual const char *GetStyleString() override;

    virtual void DumpMIF(FILE *fpOut = NULL) override;
};

/*---------------------------------------------------------------------
 *                      class TABFontPoint
 *
 * Feature class to handle MapInfo Font Point Symbol types:
 *
 *     TAB_GEOM_FONTSYMBOL_C    0x28
 *     TAB_GEOM_FONTSYMBOL      0x29
 *
 * Feature geometry will be a OGRPoint
 *
 * The symbol number refers to a character code in the specified Windows
 * Font (e.g. "Windings").
 *--------------------------------------------------------------------*/
class TABFontPoint CPL_FINAL : public TABPoint,
                    public ITABFeatureFont
{
  protected:
    double      m_dAngle;
    GInt16      m_nFontStyle;           // Bold/shadow/halo/etc.

  public:
    explicit TABFontPoint(OGRFeatureDefn *poDefnIn);
    virtual ~TABFontPoint();

    virtual TABFeatureClass GetFeatureClass() override { return TABFCFontPoint; }

    virtual TABFeature *CloneTABFeature(OGRFeatureDefn *poNewDefn = NULL ) override;

    virtual int ReadGeometryFromMAPFile(TABMAPFile *poMapFile, TABMAPObjHdr *,
                                        GBool bCoordDataOnly=FALSE,
                                        TABMAPCoordBlock **ppoCoordBlock=NULL) override;
    virtual int WriteGeometryToMAPFile(TABMAPFile *poMapFile, TABMAPObjHdr *,
                                       GBool bCoordDataOnly=FALSE,
                                       TABMAPCoordBlock **ppoCoordBlock=NULL) override;

    virtual int ReadGeometryFromMIFFile(MIDDATAFile *fp) override;
    virtual int WriteGeometryToMIFFile(MIDDATAFile *fp) override;

    virtual const char *GetStyleString() override;

    GBool       QueryFontStyle(TABFontStyle eStyleToQuery);
    void        ToggleFontStyle(TABFontStyle eStyleToToggle, GBool bStatus);

    int         GetFontStyleMIFValue();
    void        SetFontStyleMIFValue(int nStyle);
    int         GetFontStyleTABValue()           {return m_nFontStyle;}
    void        SetFontStyleTABValue(int nStyle){m_nFontStyle=(GInt16)nStyle;}

    // GetSymbolAngle(): Return angle in degrees counterclockwise
    double      GetSymbolAngle()        {return m_dAngle;}
    void        SetSymbolAngle(double dAngle);
};

/*---------------------------------------------------------------------
 *                      class TABCustomPoint
 *
 * Feature class to handle MapInfo Custom Point Symbol (Bitmap) types:
 *
 *     TAB_GEOM_CUSTOMSYMBOL_C  0x2b
 *     TAB_GEOM_CUSTOMSYMBOL    0x2c
 *
 * Feature geometry will be a OGRPoint
 *
 * The symbol name is the name of a BMP file stored in the "CustSymb"
 * directory (e.g. "arrow.BMP").  The symbol number has no meaning for
 * this symbol type.
 *--------------------------------------------------------------------*/
class TABCustomPoint CPL_FINAL : public TABPoint,
                      public ITABFeatureFont
{
  protected:
    GByte       m_nCustomStyle;         // Show BG/Apply Color

  public:
    GByte       m_nUnknown_;

  public:
    explicit TABCustomPoint(OGRFeatureDefn *poDefnIn);
    virtual ~TABCustomPoint();

    virtual TABFeatureClass GetFeatureClass() override { return TABFCCustomPoint; }

    virtual TABFeature *CloneTABFeature(OGRFeatureDefn *poNewDefn = NULL ) override;

    virtual int ReadGeometryFromMAPFile(TABMAPFile *poMapFile, TABMAPObjHdr *,
                                        GBool bCoordDataOnly=FALSE,
                                        TABMAPCoordBlock **ppoCoordBlock=NULL) override;
    virtual int WriteGeometryToMAPFile(TABMAPFile *poMapFile, TABMAPObjHdr *,
                                       GBool bCoordDataOnly=FALSE,
                                       TABMAPCoordBlock **ppoCoordBlock=NULL) override;

    virtual int ReadGeometryFromMIFFile(MIDDATAFile *fp) override;
    virtual int WriteGeometryToMIFFile(MIDDATAFile *fp) override;

    virtual const char *GetStyleString() override;

    const char *GetSymbolNameRef()      { return GetFontNameRef(); }
    void        SetSymbolName(const char *pszName) {SetFontName(pszName);}

    GByte       GetCustomSymbolStyle()              {return m_nCustomStyle;}
    void        SetCustomSymbolStyle(GByte nStyle)  {m_nCustomStyle = nStyle;}
};

/*---------------------------------------------------------------------
 *                      class TABPolyline
 *
 * Feature class to handle the various MapInfo line types:
 *
 *     TAB_GEOM_LINE_C         0x04
 *     TAB_GEOM_LINE           0x05
 *     TAB_GEOM_PLINE_C        0x07
 *     TAB_GEOM_PLINE          0x08
 *     TAB_GEOM_MULTIPLINE_C   0x25
 *     TAB_GEOM_MULTIPLINE     0x26
 *     TAB_GEOM_V450_MULTIPLINE_C 0x31
 *     TAB_GEOM_V450_MULTIPLINE   0x32
 *
 * Feature geometry can be either a OGRLineString or a OGRMultiLineString
 *--------------------------------------------------------------------*/
class TABPolyline CPL_FINAL : public TABFeature,
                   public ITABFeaturePen
{
  private:
    GBool       m_bCenterIsSet;
    double      m_dCenterX;
    double      m_dCenterY;
    GBool       m_bWriteTwoPointLineAsPolyline;

  public:
    explicit TABPolyline(OGRFeatureDefn *poDefnIn);
    virtual ~TABPolyline();

    virtual TABFeatureClass GetFeatureClass() override { return TABFCPolyline; }
    virtual TABGeomType     ValidateMapInfoType(TABMAPFile *poMapFile = NULL) override;

    virtual TABFeature *CloneTABFeature(OGRFeatureDefn *poNewDefn = NULL ) override;

    /* 2 methods to simplify access to rings in a multiple polyline
     */
    int                 GetNumParts();
    OGRLineString      *GetPartRef(int nPartIndex);

    GBool       TwoPointLineAsPolyline();
    void        TwoPointLineAsPolyline(GBool bTwoPointLineAsPolyline);

    virtual int ReadGeometryFromMAPFile(TABMAPFile *poMapFile, TABMAPObjHdr *,
                                        GBool bCoordDataOnly=FALSE,
                                        TABMAPCoordBlock **ppoCoordBlock=NULL) override;
    virtual int WriteGeometryToMAPFile(TABMAPFile *poMapFile, TABMAPObjHdr *,
                                       GBool bCoordDataOnly=FALSE,
                                       TABMAPCoordBlock **ppoCoordBlock=NULL) override;

    virtual int ReadGeometryFromMIFFile(MIDDATAFile *fp) override;
    virtual int WriteGeometryToMIFFile(MIDDATAFile *fp) override;

    virtual const char *GetStyleString() override;

    virtual void DumpMIF(FILE *fpOut = NULL) override;

    int         GetCenter(double &dX, double &dY);
    void        SetCenter(double dX, double dY);

    // MapInfo-specific attributes... made available through public vars
    // for now.
    GBool       m_bSmooth;
};

/*---------------------------------------------------------------------
 *                      class TABRegion
 *
 * Feature class to handle the MapInfo region types:
 *
 *     TAB_GEOM_REGION_C         0x0d
 *     TAB_GEOM_REGION           0x0e
 *     TAB_GEOM_V450_REGION_C    0x2e
 *     TAB_GEOM_V450_REGION      0x2f
 *
 * Feature geometry will be returned as OGRPolygon (with a single ring)
 * or OGRMultiPolygon (for multiple rings).
 *
 * REGIONs with multiple rings are returned as OGRMultiPolygon instead of
 * as OGRPolygons since OGRPolygons require that the first ring be the
 * outer ring, and the other all be inner rings, but this is not guaranteed
 * inside MapInfo files.  However, when writing features, OGRPolygons with
 * multiple rings will be accepted without problem.
 *--------------------------------------------------------------------*/
class TABRegion CPL_FINAL : public TABFeature,
                 public ITABFeaturePen,
                 public ITABFeatureBrush
{
  private:
    GBool       m_bSmooth;
    GBool       m_bCenterIsSet;
    double      m_dCenterX;
    double      m_dCenterY;

    int     ComputeNumRings(TABMAPCoordSecHdr **ppasSecHdrs,
                            TABMAPFile *poMAPFile);
    static int     AppendSecHdrs(OGRPolygon *poPolygon,
                          TABMAPCoordSecHdr * &pasSecHdrs,
                          TABMAPFile *poMAPFile,
                          int &iLastRing);

  public:
    explicit TABRegion(OGRFeatureDefn *poDefnIn);
    virtual ~TABRegion();

    virtual TABFeatureClass GetFeatureClass() override { return TABFCRegion; }
    virtual TABGeomType     ValidateMapInfoType(TABMAPFile *poMapFile = NULL) override;

    virtual TABFeature *CloneTABFeature(OGRFeatureDefn *poNewDefn = NULL ) override;

    /* 2 methods to make the REGION's geometry look like a single collection
     * of OGRLinearRings
     */
    int                 GetNumRings();
    OGRLinearRing      *GetRingRef(int nRequestedRingIndex);
    GBool               IsInteriorRing(int nRequestedRingIndex);

    virtual int ReadGeometryFromMAPFile(TABMAPFile *poMapFile, TABMAPObjHdr *,
                                        GBool bCoordDataOnly=FALSE,
                                        TABMAPCoordBlock **ppoCoordBlock=NULL) override;
    virtual int WriteGeometryToMAPFile(TABMAPFile *poMapFile, TABMAPObjHdr *,
                                       GBool bCoordDataOnly=FALSE,
                                       TABMAPCoordBlock **ppoCoordBlock=NULL) override;

    virtual int ReadGeometryFromMIFFile(MIDDATAFile *fp) override;
    virtual int WriteGeometryToMIFFile(MIDDATAFile *fp) override;

    virtual const char *GetStyleString() override;

    virtual void DumpMIF(FILE *fpOut = NULL) override;

    int         GetCenter(double &dX, double &dY);
    void        SetCenter(double dX, double dY);
};

/*---------------------------------------------------------------------
 *                      class TABRectangle
 *
 * Feature class to handle the MapInfo rectangle types:
 *
 *     TAB_GEOM_RECT_C         0x13
 *     TAB_GEOM_RECT           0x14
 *     TAB_GEOM_ROUNDRECT_C    0x16
 *     TAB_GEOM_ROUNDRECT      0x17
 *
 * A rectangle is defined by the coords of its 2 opposite corners (the MBR)
 * Its corners can optionally be rounded, in which case a X and Y rounding
 * radius will be defined.
 *
 * Feature geometry will be OGRPolygon
 *--------------------------------------------------------------------*/
class TABRectangle CPL_FINAL : public TABFeature,
                    public ITABFeaturePen,
                    public ITABFeatureBrush
{
  private:
    virtual int UpdateMBR(TABMAPFile *poMapFile = NULL) override;

  public:
    explicit TABRectangle(OGRFeatureDefn *poDefnIn);
    virtual ~TABRectangle();

    virtual TABFeatureClass GetFeatureClass() override { return TABFCRectangle; }
    virtual TABGeomType     ValidateMapInfoType(TABMAPFile *poMapFile = NULL) override;

    virtual TABFeature *CloneTABFeature(OGRFeatureDefn *poNewDefn = NULL ) override;

    virtual int ReadGeometryFromMAPFile(TABMAPFile *poMapFile, TABMAPObjHdr *,
                                        GBool bCoordDataOnly=FALSE,
                                        TABMAPCoordBlock **ppoCoordBlock=NULL) override;
    virtual int WriteGeometryToMAPFile(TABMAPFile *poMapFile, TABMAPObjHdr *,
                                       GBool bCoordDataOnly=FALSE,
                                       TABMAPCoordBlock **ppoCoordBlock=NULL) override;

    virtual int ReadGeometryFromMIFFile(MIDDATAFile *fp) override;
    virtual int WriteGeometryToMIFFile(MIDDATAFile *fp) override;

    virtual const char *GetStyleString() override;

    virtual void DumpMIF(FILE *fpOut = NULL) override;

    // MapInfo-specific attributes... made available through public vars
    // for now.
    GBool       m_bRoundCorners;
    double      m_dRoundXRadius;
    double      m_dRoundYRadius;
};

/*---------------------------------------------------------------------
 *                      class TABEllipse
 *
 * Feature class to handle the MapInfo ellipse types:
 *
 *     TAB_GEOM_ELLIPSE_C      0x19
 *     TAB_GEOM_ELLIPSE        0x1a
 *
 * An ellipse is defined by the coords of its 2 opposite corners (the MBR)
 *
 * Feature geometry can be either an OGRPoint defining the center of the
 * ellipse, or an OGRPolygon defining the ellipse itself.
 *
 * When an ellipse is read, the returned geometry is a OGRPolygon representing
 * the ellipse with 2 degrees line segments.
 *
 * In the case of the OGRPoint, then the X/Y Radius MUST be set, but.
 * However with an OGRPolygon, if the X/Y radius are not set (== 0) then
 * the MBR of the polygon will be used to define the ellipse parameters
 * and the center of the MBR is used as the center of the ellipse...
 * (i.e. the polygon vertices themselves will be ignored).
 *--------------------------------------------------------------------*/
class TABEllipse CPL_FINAL : public TABFeature,
                  public ITABFeaturePen,
                  public ITABFeatureBrush
{
  private:
    virtual int UpdateMBR(TABMAPFile *poMapFile = NULL) override;

  public:
    explicit TABEllipse(OGRFeatureDefn *poDefnIn);
    virtual ~TABEllipse();

    virtual TABFeatureClass GetFeatureClass() override { return TABFCEllipse; }
    virtual TABGeomType     ValidateMapInfoType(TABMAPFile *poMapFile = NULL) override;

    virtual TABFeature *CloneTABFeature(OGRFeatureDefn *poNewDefn = NULL ) override;

    virtual int ReadGeometryFromMAPFile(TABMAPFile *poMapFile, TABMAPObjHdr *,
                                        GBool bCoordDataOnly=FALSE,
                                        TABMAPCoordBlock **ppoCoordBlock=NULL) override;
    virtual int WriteGeometryToMAPFile(TABMAPFile *poMapFile, TABMAPObjHdr *,
                                       GBool bCoordDataOnly=FALSE,
                                       TABMAPCoordBlock **ppoCoordBlock=NULL) override;

    virtual int ReadGeometryFromMIFFile(MIDDATAFile *fp) override;
    virtual int WriteGeometryToMIFFile(MIDDATAFile *fp) override;

    virtual const char *GetStyleString() override;

    virtual void DumpMIF(FILE *fpOut = NULL) override;

    // MapInfo-specific attributes... made available through public vars
    // for now.
    double      m_dCenterX;
    double      m_dCenterY;
    double      m_dXRadius;
    double      m_dYRadius;
};

/*---------------------------------------------------------------------
 *                      class TABArc
 *
 * Feature class to handle the MapInfo arc types:
 *
 *     TAB_GEOM_ARC_C      0x0a
 *     TAB_GEOM_ARC        0x0b
 *
 * In MapInfo, an arc is defined by the coords of the MBR corners of its
 * defining ellipse, which in this case is different from the arc's MBR,
 * and a start and end angle in degrees.
 *
 * Feature geometry can be either an OGRLineString or an OGRPoint.
 *
 * In any case, X/Y radius X/Y center, and start/end angle (in degrees
 * counterclockwise) MUST be set.
 *
 * When an arc is read, the returned geometry is an OGRLineString
 * representing the arc with 2 degrees line segments.
 *--------------------------------------------------------------------*/
class TABArc CPL_FINAL : public TABFeature,
              public ITABFeaturePen
{
  private:
    double      m_dStartAngle;  // In degrees, counterclockwise,
    double      m_dEndAngle;    // starting at 3 o'clock

    virtual int UpdateMBR(TABMAPFile *poMapFile = NULL) override;

  public:
    explicit TABArc(OGRFeatureDefn *poDefnIn);
    virtual ~TABArc();

    virtual TABFeatureClass GetFeatureClass() override { return TABFCArc; }
    virtual TABGeomType     ValidateMapInfoType(TABMAPFile *poMapFile = NULL) override;

    virtual TABFeature *CloneTABFeature(OGRFeatureDefn *poNewDefn = NULL ) override;

    virtual int ReadGeometryFromMAPFile(TABMAPFile *poMapFile, TABMAPObjHdr *,
                                        GBool bCoordDataOnly=FALSE,
                                        TABMAPCoordBlock **ppoCoordBlock=NULL) override;
    virtual int WriteGeometryToMAPFile(TABMAPFile *poMapFile, TABMAPObjHdr *,
                                       GBool bCoordDataOnly=FALSE,
                                       TABMAPCoordBlock **ppoCoordBlock=NULL) override;

    virtual int ReadGeometryFromMIFFile(MIDDATAFile *fp) override;
    virtual int WriteGeometryToMIFFile(MIDDATAFile *fp) override;

    virtual const char *GetStyleString() override;

    virtual void DumpMIF(FILE *fpOut = NULL) override;

    double      GetStartAngle() { return m_dStartAngle; }
    double      GetEndAngle()   { return m_dEndAngle; }
    void        SetStartAngle(double dAngle);
    void        SetEndAngle(double dAngle);

    // MapInfo-specific attributes... made available through public vars
    // for now.
    double      m_dCenterX;
    double      m_dCenterY;
    double      m_dXRadius;
    double      m_dYRadius;
};

/*---------------------------------------------------------------------
 *                      class TABText
 *
 * Feature class to handle the MapInfo text types:
 *
 *     TAB_GEOM_TEXT_C         0x10
 *     TAB_GEOM_TEXT           0x11
 *
 * Feature geometry is an OGRPoint corresponding to the lower-left
 * corner of the text MBR BEFORE ROTATION.
 *
 * Text string, and box height/width (box before rotation is applied)
 * are required in a valid text feature and MUST be set.
 * Text angle and other styles are optional.
 *--------------------------------------------------------------------*/
class TABText CPL_FINAL : public TABFeature,
               public ITABFeatureFont,
               public ITABFeaturePen
{
  protected:
    char        *m_pszString;

    double      m_dAngle;
    double      m_dHeight;
    double      m_dWidth;
    double      m_dfLineEndX;
    double      m_dfLineEndY;
    GBool       m_bLineEndSet;
    void        UpdateTextMBR();

    GInt32      m_rgbForeground;
    GInt32      m_rgbBackground;
    GInt32      m_rgbOutline;
    GInt32      m_rgbShadow;

    GInt16      m_nTextAlignment;       // Justification/Vert.Spacing/arrow
    GInt16      m_nFontStyle;           // Bold/italic/underlined/shadow/...

    const char *GetLabelStyleString();

    virtual int UpdateMBR(TABMAPFile *poMapFile = NULL) override;

  public:
    explicit TABText(OGRFeatureDefn *poDefnIn);
    virtual ~TABText();

    virtual TABFeatureClass GetFeatureClass() override { return TABFCText; }
    virtual TABGeomType     ValidateMapInfoType(TABMAPFile *poMapFile = NULL) override;

    virtual TABFeature *CloneTABFeature(OGRFeatureDefn *poNewDefn = NULL ) override;

    virtual int ReadGeometryFromMAPFile(TABMAPFile *poMapFile, TABMAPObjHdr *,
                                        GBool bCoordDataOnly=FALSE,
                                        TABMAPCoordBlock **ppoCoordBlock=NULL) override;
    virtual int WriteGeometryToMAPFile(TABMAPFile *poMapFile, TABMAPObjHdr *,
                                       GBool bCoordDataOnly=FALSE,
                                       TABMAPCoordBlock **ppoCoordBlock=NULL) override;

    virtual int ReadGeometryFromMIFFile(MIDDATAFile *fp) override;
    virtual int WriteGeometryToMIFFile(MIDDATAFile *fp) override;

    virtual const char *GetStyleString() override;

    virtual void DumpMIF(FILE *fpOut = NULL) override;

    const char *GetTextString();
    double      GetTextAngle();
    double      GetTextBoxHeight();
    double      GetTextBoxWidth();
    GInt32      GetFontFGColor();
    GInt32      GetFontBGColor();
    GInt32      GetFontOColor();
    GInt32      GetFontSColor();
    void        GetTextLineEndPoint(double &dX, double &dY);

    TABTextJust GetTextJustification();
    TABTextSpacing  GetTextSpacing();
    TABTextLineType GetTextLineType();
    GBool       QueryFontStyle(TABFontStyle eStyleToQuery);

    void        SetTextString(const char *pszStr);
    void        SetTextAngle(double dAngle);
    void        SetTextBoxHeight(double dHeight);
    void        SetTextBoxWidth(double dWidth);
    void        SetFontFGColor(GInt32 rgbColor);
    void        SetFontBGColor(GInt32 rgbColor);
    void        SetFontOColor(GInt32 rgbColor);
    void        SetFontSColor(GInt32 rgbColor);
    void        SetTextLineEndPoint(double dX, double dY);

    void        SetTextJustification(TABTextJust eJust);
    void        SetTextSpacing(TABTextSpacing eSpacing);
    void        SetTextLineType(TABTextLineType eLineType);
    void        ToggleFontStyle(TABFontStyle eStyleToToggle, GBool bStatus);

    int         GetFontStyleMIFValue();
    void        SetFontStyleMIFValue(int nStyle, GBool bBGColorSet=FALSE);
    GBool       IsFontBGColorUsed();
    GBool       IsFontOColorUsed();
    GBool       IsFontSColorUsed();
    GBool       IsFontBold();
    GBool       IsFontItalic();
    GBool       IsFontUnderline();
    int         GetFontStyleTABValue()           {return m_nFontStyle;}
    void        SetFontStyleTABValue(int nStyle){m_nFontStyle=(GInt16)nStyle;}
};

/*---------------------------------------------------------------------
 *                      class TABMultiPoint
 *
 * Feature class to handle MapInfo Multipoint features:
 *
 *     TAB_GEOM_MULTIPOINT_C        0x34
 *     TAB_GEOM_MULTIPOINT          0x35
 *
 * Feature geometry will be a OGRMultiPoint
 *
 * The symbol number is in the range [31..67], with 31=None and corresponds
 * to one of the 35 predefined "Old MapInfo Symbols"
 *--------------------------------------------------------------------*/
class TABMultiPoint CPL_FINAL : public TABFeature,
                     public ITABFeatureSymbol
{
  private:
    // We call it center, but it's more like a label point
    // Its value default to be the location of the first point
    GBool       m_bCenterIsSet;
    double      m_dCenterX;
    double      m_dCenterY;

  public:
    explicit TABMultiPoint(OGRFeatureDefn *poDefnIn);
    virtual ~TABMultiPoint();

    virtual TABFeatureClass GetFeatureClass() override { return TABFCMultiPoint; }
    virtual TABGeomType     ValidateMapInfoType(TABMAPFile *poMapFile = NULL) override;

    virtual TABFeature *CloneTABFeature(OGRFeatureDefn *poNewDefn = NULL ) override;

    int         GetXY(int i, double &dX, double &dY);
    int         GetNumPoints();

    int         GetCenter(double &dX, double &dY);
    void        SetCenter(double dX, double dY);

    virtual int ReadGeometryFromMAPFile(TABMAPFile *poMapFile, TABMAPObjHdr *,
                                        GBool bCoordDataOnly=FALSE,
                                        TABMAPCoordBlock **ppoCoordBlock=NULL) override;
    virtual int WriteGeometryToMAPFile(TABMAPFile *poMapFile, TABMAPObjHdr *,
                                       GBool bCoordDataOnly=FALSE,
                                       TABMAPCoordBlock **ppoCoordBlock=NULL) override;

    virtual int ReadGeometryFromMIFFile(MIDDATAFile *fp) override;
    virtual int WriteGeometryToMIFFile(MIDDATAFile *fp) override;

    virtual const char *GetStyleString() override;

    virtual void DumpMIF(FILE *fpOut = NULL) override;
};

/*---------------------------------------------------------------------
 *
 *                      class TABCollection
 *
 * Feature class to handle MapInfo Collection features:
 *
 *     TAB_GEOM_COLLECTION_C        0x37
 *     TAB_GEOM_COLLECTION          0x38
 *
 * Feature geometry will be a OGRCollection
 *
 * **** IMPORTANT NOTE: ****
 *
 * The current implementation does not allow setting the Geometry via
 * OGRFeature::SetGeometry*(). The geometries must be set via the
 * TABCollection::SetRegion/Pline/MpointDirectly() methods which will take
 * care of keeping the OGRFeature's geometry in sync.
 *
 * If we ever want to support creating collections via the OGR interface then
 * something should be added in TABCollection::WriteGeometryToMapFile(), or
 * perhaps in ValidateMapInfoType(), or even better in a custom
 * TABCollection::SetGeometry*()... but then this last option may not work
 * unless OGRFeature::SetGeometry*() are made virtual in OGR.
 *
 *--------------------------------------------------------------------*/
class TABCollection CPL_FINAL : public TABFeature,
                     public ITABFeatureSymbol
{
  private:
    TABRegion       *m_poRegion;
    TABPolyline     *m_poPline;
    TABMultiPoint   *m_poMpoint;

    void    EmptyCollection();
    static int     ReadLabelAndMBR(TABMAPCoordBlock *poCoordBlock,
                            GBool bComprCoord,
                            GInt32 nComprOrgX, GInt32 nComprOrgY,
                            GInt32 &pnMinX, GInt32 &pnMinY,
                            GInt32 &pnMaxX, GInt32 &pnMaxY,
                            GInt32 &pnLabelX, GInt32 &pnLabelY );
    static int     WriteLabelAndMBR(TABMAPCoordBlock *poCoordBlock,
                             GBool bComprCoord,
                             GInt32 nMinX, GInt32 nMinY,
                             GInt32 nMaxX, GInt32 nMaxY,
                             GInt32 nLabelX, GInt32 nLabelY );
    int     SyncOGRGeometryCollection(GBool bSyncRegion,
                                      GBool bSyncPline,
                                      GBool bSyncMpoint);

  public:
    explicit TABCollection(OGRFeatureDefn *poDefnIn);
    virtual ~TABCollection();

    virtual TABFeatureClass GetFeatureClass() override { return TABFCCollection; }
    virtual TABGeomType     ValidateMapInfoType(TABMAPFile *poMapFile = NULL) override;

    virtual TABFeature *CloneTABFeature(OGRFeatureDefn *poNewDefn = NULL ) override;

    virtual int ReadGeometryFromMAPFile(TABMAPFile *poMapFile, TABMAPObjHdr *,
                                        GBool bCoordDataOnly=FALSE,
                                        TABMAPCoordBlock **ppoCoordBlock=NULL) override;
    virtual int WriteGeometryToMAPFile(TABMAPFile *poMapFile, TABMAPObjHdr *,
                                       GBool bCoordDataOnly=FALSE,
                                       TABMAPCoordBlock **ppoCoordBlock=NULL) override;

    virtual int ReadGeometryFromMIFFile(MIDDATAFile *fp) override;
    virtual int WriteGeometryToMIFFile(MIDDATAFile *fp) override;

    virtual const char *GetStyleString() override;

    virtual void DumpMIF(FILE *fpOut = NULL) override;

    TABRegion           *GetRegionRef()         {return m_poRegion; }
    TABPolyline         *GetPolylineRef()       {return m_poPline; }
    TABMultiPoint       *GetMultiPointRef()     {return m_poMpoint; }

    int                 SetRegionDirectly(TABRegion *poRegion);
    int                 SetPolylineDirectly(TABPolyline *poPline);
    int                 SetMultiPointDirectly(TABMultiPoint *poMpoint);
};

/*---------------------------------------------------------------------
 *                      class TABDebugFeature
 *
 * Feature class to use for testing purposes... this one does not
 * correspond to any MapInfo type... it's just used to dump info about
 * feature types that are not implemented yet.
 *--------------------------------------------------------------------*/
class TABDebugFeature CPL_FINAL : public TABFeature
{
  private:
    GByte       m_abyBuf[512];
    int         m_nSize;
    int         m_nCoordDataPtr;  // -1 if none
    int         m_nCoordDataSize;

  public:
    explicit TABDebugFeature(OGRFeatureDefn *poDefnIn);
    virtual ~TABDebugFeature();

    virtual TABFeatureClass GetFeatureClass() override { return TABFCDebugFeature; }

    virtual int ReadGeometryFromMAPFile(TABMAPFile *poMapFile, TABMAPObjHdr *,
                                        GBool bCoordDataOnly=FALSE,
                                        TABMAPCoordBlock **ppoCoordBlock=NULL) override;
    virtual int WriteGeometryToMAPFile(TABMAPFile *poMapFile, TABMAPObjHdr *,
                                       GBool bCoordDataOnly=FALSE,
                                       TABMAPCoordBlock **ppoCoordBlock=NULL) override;

    virtual int ReadGeometryFromMIFFile(MIDDATAFile *fp) override;
    virtual int WriteGeometryToMIFFile(MIDDATAFile *fp) override;

    virtual void DumpMIF(FILE *fpOut = NULL) override;
};

/* -------------------------------------------------------------------- */
/*      Some stuff related to spatial reference system handling.        */
/*                                                                      */
/*      In GDAL we make use of the coordsys transformation from         */
/*      other places (sometimes even from plugins), so we               */
/*      deliberately export these two functions from the DLL.           */
/* -------------------------------------------------------------------- */

char CPL_DLL *MITABSpatialRef2CoordSys( OGRSpatialReference * );
OGRSpatialReference CPL_DLL * MITABCoordSys2SpatialRef( const char * );

bool MITABExtractCoordSysBounds( const char * pszCoordSys,
                                 double &dXMin, double &dYMin,
                                 double &dXMax, double &dYMax );
int MITABCoordSys2TABProjInfo(const char * pszCoordSys, TABProjInfo *psProj);

typedef struct {
    int         nDatumEPSGCode;
    int         nMapInfoDatumID;
    const char  *pszOGCDatumName;
    int         nEllipsoid;
    double      dfShiftX;
    double      dfShiftY;
    double      dfShiftZ;
    double      dfDatumParm0; /* RotX */
    double      dfDatumParm1; /* RotY */
    double      dfDatumParm2; /* RotZ */
    double      dfDatumParm3; /* Scale Factor */
    double      dfDatumParm4; /* Prime Meridian */
} MapInfoDatumInfo;

typedef struct
{
    int         nMapInfoId;
    const char *pszMapinfoName;
    double      dfA; /* semi major axis in meters */
    double      dfInvFlattening; /* Inverse flattening */
} MapInfoSpheroidInfo;

/*---------------------------------------------------------------------
 * The following are used for coordsys bounds lookup
 *--------------------------------------------------------------------*/

bool    MITABLookupCoordSysBounds(TABProjInfo *psCS,
                                  double &dXMin, double &dYMin,
                                  double &dXMax, double &dYMax,
                                  bool bOnlyUserTable = false);
int     MITABLoadCoordSysTable(const char *pszFname);
void    MITABFreeCoordSysTable();
bool    MITABCoordSysTableLoaded();  // TODO(schwehr): Unused?

#endif /* MITAB_H_INCLUDED_ */
