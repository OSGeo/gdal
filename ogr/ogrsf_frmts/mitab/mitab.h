/**********************************************************************
 * $Id: mitab.h,v 1.13 1999/11/08 04:34:55 stephane Exp $
 *
 * Name:     mitab.h
 * Project:  MapInfo MIF Read/Write library
 * Language: C++
 * Purpose:  Header file containing public definitions for the library.
 * Author:   Daniel Morissette, danmo@videotron.ca
 *
 **********************************************************************
 * Copyright (c) 1999, Daniel Morissette
 *
 * All rights reserved.  This software may be copied or reproduced, in
 * all or in part, without the prior written consent of its author,
 * Daniel Morissette (danmo@videotron.ca).  However, any material copied
 * or reproduced must bear the original copyright notice (above), this 
 * original paragraph, and the original disclaimer (below).
 * 
 * The entire risk as to the results and performance of the software,
 * supporting text and other information contained in this file
 * (collectively called the "Software") is with the user.  Although 
 * considerable efforts have been used in preparing the Software, the 
 * author does not warrant the accuracy or completeness of the Software.
 * In no event will the author be liable for damages, including loss of
 * profits or consequential damages, arising out of the use of the 
 * Software.
 * 
 **********************************************************************
 *
 * $Log: mitab.h,v $
 * Revision 1.13  1999/11/08 04:34:55  stephane
 * mid/mif support
 *
 * Revision 1.12  1999/10/18 15:44:47  daniel
 * Several fixes/improvements mostly for writing of Arc/Ellipses/Text
 * and also added more complete description for each TABFeature type
 *
 * Revision 1.11  1999/10/12 14:30:19  daniel
 * Added IMapInfoFile class to be used as a base for TABFile and MIFFile
 *
 * Revision 1.10  1999/10/06 13:13:47  daniel
 * Added several Get/Set() methods to feature classes.
 *
 * Revision 1.9  1999/09/29 04:27:14  daniel
 * Changed some TABFeatureClass names
 *
 * Revision 1.8  1999/09/28 13:32:10  daniel
 * Added TABFile::AddFieldNative()
 *
 * Revision 1.7  1999/09/28 02:52:47  warmerda
 * Added SetProjInfo().
 *
 * Revision 1.6  1999/09/26 14:59:36  daniel
 * Implemented write support
 *
 * Revision 1.5  1999/09/24 20:23:09  warmerda
 * added GetProjInfo method
 *
 * Revision 1.4  1999/09/23 19:49:47  warmerda
 * Added setspatialref()
 *
 * Revision 1.3  1999/09/16 02:39:16  daniel
 * Completed read support for most feature types
 *
 * Revision 1.2  1999/09/01 17:46:49  daniel
 * Added GetNativeFieldType() and GetFeatureDefn() to TABFile
 *
 * Revision 1.1  1999/07/12 04:18:23  daniel
 * Initial checkin
 *
 **********************************************************************/

#ifndef _MITAB_H_INCLUDED_
#define _MITAB_H_INCLUDED_

#include "mitab_priv.h"
#include "ogr_feature.h"
#include "ogrsf_frmts.h"

#ifndef PI
#  define PI 3.14159265358979323846
#endif


class TABFeature;

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
  private:

  protected: 
    OGRGeometry		*m_poFilterGeom;
    int                  m_nCurFeatureId;
 

  public:
    IMapInfoFile() ;
    virtual ~IMapInfoFile();

    virtual int Open(const char *pszFname, const char *pszAccess) = 0;
    virtual int Close() = 0;

    //  Ogr methode for MapInfo support of read

    OGRGeometry *	GetSpatialFilter();
    void		SetSpatialFilter( OGRGeometry * );
    void		ResetReading() = 0;
    int                 GetFeatureCount (int bForce) = 0;
    virtual OGRFeature *GetNextFeature();
    OGRFeature         *GetFeature(long nFeatureId);
    OGRErr              CreateFeature(OGRFeature *poFeature);
    int                 TestCapability( const char * pszCap ) =0;
    
    ///////////////
    // Read access specific stuff
    //
    virtual int GetNextFeatureId(int nPrevId) = 0;
    virtual TABFeature *GetFeatureRef(int nFeatureId) = 0;
    virtual OGRFeatureDefn *GetLayerDefn() = 0;

    virtual TABFieldType GetNativeFieldType(int nFieldId) = 0;

    virtual int GetBounds(double &dXMin, double &dYMin, 
                          double &dXMax, double &dYMax) = 0;
    
    virtual OGRSpatialReference *GetSpatialRef() = 0;

    ///////////////
    // Write access specific stuff
    //
    virtual int SetBounds(double dXMin, double dYMin, 
                          double dXMax, double dYMax) = 0;
    virtual int SetFeatureDefn(OGRFeatureDefn *poFeatureDefn,
                            TABFieldType *paeMapInfoNativeFieldTypes = NULL)=0;
    virtual int AddFieldNative(const char *pszName, TABFieldType eMapInfoType,
                               int nWidth, int nPrecision=0) = 0;
    virtual int SetSpatialRef(OGRSpatialReference *poSpatialRef) = 0;

    virtual int SetFeature(TABFeature *poFeature, int nFeatureId = -1) = 0;
};

/*---------------------------------------------------------------------
 *                      class TABFile
 *
 * The main class for TAB datasets.  External programs should use this
 * class to open a TAB dataset and read/write features from/to it.
 *
 *--------------------------------------------------------------------*/
class TABFile: public IMapInfoFile
{
  private:
    char        *m_pszFname;
    TABAccess   m_eAccessMode;
    char        **m_papszTABFile;
    char        *m_pszVersion;
    char        *m_pszCharset;
    GBool       m_bBoundsSet;
    

    TABDATFile  *m_poDATFile;   // Attributes file
    TABMAPFile  *m_poMAPFile;   // Object Geometry file

    OGRFeatureDefn *m_poDefn;
    OGRSpatialReference *m_poSpatialRef;

    TABFeature *m_poCurFeature;
    int         m_nLastFeatureId;


    ///////////////
    // Private Read access specific stuff
    //
    int         ReadFeatureDefn();
    int         ParseTABFile();

     ///////////////
    // Private Write access specific stuff
    //
    int         WriteTABFile();

  public:
    TABFile();
    virtual ~TABFile();

    virtual int Open(const char *pszFname, const char *pszAccess);
    virtual int Close();

    void		ResetReading();
    int                 TestCapability( const char * pszCap );
    int                 GetFeatureCount (int bForce);
    
    ///////////////
    // Read access specific stuff
    //
    

    virtual int GetNextFeatureId(int nPrevId);
    virtual TABFeature *GetFeatureRef(int nFeatureId);
    virtual OGRFeatureDefn *GetLayerDefn();

    virtual TABFieldType GetNativeFieldType(int nFieldId);

    virtual int GetBounds(double &dXMin, double &dYMin, 
                          double &dXMax, double &dYMax);
    
    virtual OGRSpatialReference *GetSpatialRef();

    ///////////////
    // Write access specific stuff
    //
    virtual int SetBounds(double dXMin, double dYMin, 
                          double dXMax, double dYMax);
    virtual int SetFeatureDefn(OGRFeatureDefn *poFeatureDefn,
                            TABFieldType *paeMapInfoNativeFieldTypes = NULL);
    virtual int AddFieldNative(const char *pszName, TABFieldType eMapInfoType,
                               int nWidth, int nPrecision=0);
    virtual int SetSpatialRef(OGRSpatialReference *poSpatialRef);

    virtual int SetFeature(TABFeature *poFeature, int nFeatureId = -1);


    ///////////////
    // semi-private.
    int          GetProjInfo(TABProjInfo *poPI)
	    { return m_poMAPFile->GetHeaderBlock()->GetProjInfo( poPI ); }
    int          SetProjInfo(TABProjInfo *poPI)
	    { return m_poMAPFile->GetHeaderBlock()->SetProjInfo( poPI ); }

#ifdef DEBUG
    void Dump(FILE *fpOut = NULL);
#endif
};

/*---------------------------------------------------------------------
 *                      class MIFFile
 *
 * The main class for (MID/MIF) datasets.  External programs should use this
 * class to open a (MID/MIF) dataset and read/write features from/to it.
 *
 *--------------------------------------------------------------------*/
class MIFFile: public IMapInfoFile
{
  private:
    char        *m_pszFname;
    TABAccess    m_eAccessMode;
    char        *m_pszVersion;
    char        *m_pszCharset;
    char        *m_pszDelimiter;
    char        *m_pszUnique;
    char        *m_pszIndex;
    char        *m_pszCoordSys;
    
    double       m_dfXMultiplier;
    double       m_dfYMultiplier;
    double       m_dfXDisplacement;
    double       m_dfYDisplacement;

    double      m_dXMin;
    double      m_dYMin;
    double      m_dXMax;
    double      m_dYMax;
 
    MIDDATAFile  *m_poMIDFile;   // Mid file
    MIDDATAFile  *m_poMIFFile;   // Mif File

    OGRFeatureDefn *m_poDefn;
    OGRSpatialReference *m_poSpatialRef;

    TABFeature *m_poCurFeature;
    int         m_nLastFeatureId;
    int         m_nAttribut;

    ///////////////
    // Private Read access specific stuff
    //
    int         ReadFeatureDefn();
    int         ParseMIFHeader();
    int         CountNumberFeature();
    int         AddFields(const char *pszLine);
    int         GotoFeature(int nFeatureId);
    int         NextFeature();

    ///////////////
    // Private Write access specific stuff
    //
    GBool       m_bBoundsSet;
    
    int         WriteMIFHeader();
    void UpdateBounds(double dfX,double dfY);

  public:
    MIFFile();
    virtual ~MIFFile();

    virtual int Open(const char *pszFname, const char *pszAccess);
    virtual int Close();


    int                 TestCapability( const char * pszCap ) ;
    int                 GetFeatureCount (int bForce);
    void		ResetReading();

    ///////////////
    // Read access specific stuff
    //
    
    virtual int GetNextFeatureId(int nPrevId);
    virtual TABFeature *GetFeatureRef(int nFeatureId);
    virtual OGRFeatureDefn *GetLayerDefn();

    virtual TABFieldType GetNativeFieldType(int nFieldId);

    virtual int GetBounds(double &dXMin, double &dYMin, 
                          double &dXMax, double &dYMax);
    
    /* TODO */
    virtual OGRSpatialReference *GetSpatialRef(){return NULL;}

    ///////////////
    // Write access specific stuff
    //
    virtual int SetBounds(double dXMin, double dYMin, 
                          double dXMax, double dYMax);
    virtual int SetFeatureDefn(OGRFeatureDefn *poFeatureDefn,
                            TABFieldType *paeMapInfoNativeFieldTypes = NULL);
    virtual int AddFieldNative(const char *pszName, TABFieldType eMapInfoType,
                               int nWidth, int nPrecision=0);
    /* TODO */
    virtual int SetSpatialRef(OGRSpatialReference *poSpatialRef){return NULL;}

    virtual int SetFeature(TABFeature *poFeature, int nFeatureId = -1);

    ///////////////
    // semi-private.
	   int          GetProjInfo(TABProjInfo *poPI){return 0;}
    /*  { return m_poMAPFile->GetHeaderBlock()->GetProjInfo( poPI ); }*/
	   int          SetProjInfo(TABProjInfo *poPI){return 0;}
	   /*  { return m_poMAPFile->GetHeaderBlock()->SetProjInfo( poPI ); }*/

};

/*---------------------------------------------------------------------
 * Codes for the known MapInfo Geometry types
 *--------------------------------------------------------------------*/
#define TAB_GEOM_NONE           0
#define TAB_GEOM_SYMBOL_C       0x01
#define TAB_GEOM_SYMBOL         0x02
#define TAB_GEOM_LINE_C         0x04
#define TAB_GEOM_LINE           0x05
#define TAB_GEOM_PLINE_C        0x07
#define TAB_GEOM_PLINE          0x08
#define TAB_GEOM_ARC_C          0x0a
#define TAB_GEOM_ARC            0x0b
#define TAB_GEOM_REGION_C       0x0d
#define TAB_GEOM_REGION         0x0e
#define TAB_GEOM_TEXT_C         0x10
#define TAB_GEOM_TEXT           0x11
#define TAB_GEOM_RECT_C         0x13
#define TAB_GEOM_RECT           0x14
#define TAB_GEOM_ROUNDRECT_C    0x16
#define TAB_GEOM_ROUNDRECT      0x17
#define TAB_GEOM_ELLIPSE_C      0x19
#define TAB_GEOM_ELLIPSE        0x1a
#define TAB_GEOM_MULTIPLINE_C   0x25
#define TAB_GEOM_MULTIPLINE     0x26
#define TAB_GEOM_FONTSYMBOL_C   0x28 
#define TAB_GEOM_FONTSYMBOL     0x29
#define TAB_GEOM_CUSTOMSYMBOL_C 0x2b
#define TAB_GEOM_CUSTOMSYMBOL   0x2c


/*---------------------------------------------------------------------
 * Codes for the feature classes
 *--------------------------------------------------------------------*/
typedef enum
{
    TABFCNoGeomFeature = 0,
    TABFCPoint,
    TABFCFontPoint,
    TABFCCustomPoint,
    TABFCText,
    TABFCPolyline,
    TABFCArc,
    TABFCRegion,
    TABFCRectangle,
    TABFCEllipse,
    TABFCDebugFeature
} TABFeatureClass;

/*---------------------------------------------------------------------
 * Definitions for text attributes
 *--------------------------------------------------------------------*/
typedef enum TABTextJust_t
{
    TABTJLeft,          // Default: Left Justification
    TABTJCenter,
    TABTJRight
} TABTextJust;

typedef enum TABTextSpacing_t
{
    TABTSSingle,        // Default: Single spacing
    TABTS1_5,           // 1.5
    TABTSDouble
} TABTextSpacing;

typedef enum TABTextLineType_t
{
    TABTLNoLine,        // Default: No line
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
    TABCSApplyColor = 0x02,     // non-white pixels drawn using symbol color
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
    ITABFeaturePen() { m_nPenDefIndex=-1;
                      memset(&m_sPenDef, 0, sizeof(TABPenDef)); };
    ~ITABFeaturePen() {};
    int         GetPenDefIndex() {return m_nPenDefIndex;};
    TABPenDef  *GetPenDefRef() {return &m_sPenDef;};

    GByte       GetPenWidth()   {return m_sPenDef.nLineWidth;};
    GByte       GetPenPattern() {return m_sPenDef.nLinePattern;};
    GByte       GetPenStyle()   {return m_sPenDef.nLineStyle;};
    GInt32      GetPenColor()   {return m_sPenDef.rgbColor;};

    void        SetPenWidth(GByte val)   {m_sPenDef.nLineWidth = val;};
    void        SetPenPattern(GByte val) {m_sPenDef.nLinePattern=val;};
    void        SetPenStyle(GByte val)   {m_sPenDef.nLineStyle = val;};
    void        SetPenColor(GInt32 clr)  {m_sPenDef.rgbColor = clr;};

    void        DumpPenDef(FILE *fpOut = NULL);
};

class ITABFeatureBrush
{
  protected:
    int         m_nBrushDefIndex;
    TABBrushDef m_sBrushDef;
  public:
    ITABFeatureBrush() { m_nBrushDefIndex=-1;
                        memset(&m_sBrushDef, 0, sizeof(TABBrushDef)); };
    ~ITABFeatureBrush() {};
    int         GetBrushDefIndex() {return m_nBrushDefIndex;};
    TABBrushDef *GetBrushDefRef() {return &m_sBrushDef;};

    GInt32      GetBrushFGColor()     {return m_sBrushDef.rgbFGColor;};
    GInt32      GetBrushBGColor()     {return m_sBrushDef.rgbBGColor;};
    GByte       GetBrushPattern()     {return m_sBrushDef.nFillPattern;};
    GByte       GetBrushTransparent() {return m_sBrushDef.bTransparentFill;};

    void        SetBrushFGColor(GInt32 clr)  { m_sBrushDef.rgbFGColor = clr;};
    void        SetBrushBGColor(GInt32 clr)  { m_sBrushDef.rgbBGColor = clr;};
    void        SetBrushPattern(GByte val)   { m_sBrushDef.nFillPattern=val;};
    void        SetBrushTransparent(GByte val)
                                          {m_sBrushDef.bTransparentFill=val;};

    void        DumpBrushDef(FILE *fpOut = NULL);
};

class ITABFeatureFont
{
  protected:
    int         m_nFontDefIndex;
    TABFontDef  m_sFontDef;
  public:
    ITABFeatureFont() { m_nFontDefIndex=-1;
                       memset(&m_sFontDef, 0, sizeof(TABFontDef)); };
    ~ITABFeatureFont() {};
    int         GetFontDefIndex() {return m_nFontDefIndex;};
    TABFontDef *GetFontDefRef() {return &m_sFontDef;};

    const char *GetFontNameRef() {return m_sFontDef.szFontName;};

    void        SetFontName(const char *pszName)
                              { strncpy( m_sFontDef.szFontName, pszName, 32);
                                m_sFontDef.szFontName[32] = '\0';  };

    void        DumpFontDef(FILE *fpOut = NULL);
};

class ITABFeatureSymbol
{
  protected:
    int         m_nSymbolDefIndex;
    TABSymbolDef m_sSymbolDef;
  public:
    ITABFeatureSymbol() { m_nSymbolDefIndex=-1;
                         memset(&m_sSymbolDef, 0, sizeof(TABSymbolDef)); };
    ~ITABFeatureSymbol() {};
    int         GetSymbolDefIndex() {return m_nSymbolDefIndex;};
    TABSymbolDef *GetSymbolDefRef() {return &m_sSymbolDef;};

    GInt16      GetSymbolNo()    {return m_sSymbolDef.nSymbolNo;};
    GInt16      GetSymbolSize()  {return m_sSymbolDef.nPointSize;};
    GInt32      GetSymbolColor() {return m_sSymbolDef.rgbColor;};

    void        SetSymbolNo(GInt16 val)     { m_sSymbolDef.nSymbolNo = val;};
    void        SetSymbolSize(GInt16 val)   { m_sSymbolDef.nPointSize = val;};
    void        SetSymbolColor(GInt32 clr)  { m_sSymbolDef.rgbColor = clr;};

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
 * This class will also be used to instanciate objects with no Geometry
 * (i.e. type TAB_GEOM_NONE) which is a valid case in MapInfo.
 *
 * The logic to read/write the object from/to the .DAT and .MAP files is also
 * implemented as part of this class and derived classes.
 *--------------------------------------------------------------------*/
class TABFeature: public OGRFeature
{
  protected:
    int         m_nMapInfoType;

    double      m_dXMin;
    double      m_dYMin;
    double      m_dXMax;
    double      m_dYMax;

  public:
             TABFeature(OGRFeatureDefn *poDefnIn );
    virtual ~TABFeature();

    virtual TABFeatureClass GetFeatureClass() { return TABFCNoGeomFeature; };
    virtual int             GetMapInfoType()  { return m_nMapInfoType; };
    virtual int            ValidateMapInfoType(){m_nMapInfoType=TAB_GEOM_NONE;
                                                 return m_nMapInfoType;};

    /*-----------------------------------------------------------------
     * TAB Support
     *----------------------------------------------------------------*/

    virtual int ReadRecordFromDATFile(TABDATFile *poDATFile);
    virtual int ReadGeometryFromMAPFile(TABMAPFile *poMapFile);

    virtual int WriteRecordToDATFile(TABDATFile *poDATFile);
    virtual int WriteGeometryToMAPFile(TABMAPFile *poMapFile);

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
             TABPoint(OGRFeatureDefn *poDefnIn);
    virtual ~TABPoint();

    virtual TABFeatureClass GetFeatureClass() { return TABFCPoint; };
    virtual int             ValidateMapInfoType();

    double	GetX();
    double	GetY();

    virtual int ReadGeometryFromMAPFile(TABMAPFile *poMapFile);
    virtual int WriteGeometryToMAPFile(TABMAPFile *poMapFile);

    virtual int ReadGeometryFromMIFFile(MIDDATAFile *fp);
    virtual int WriteGeometryToMIFFile(MIDDATAFile *fp);

    virtual void DumpMIF(FILE *fpOut = NULL);
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
class TABFontPoint: public TABPoint, 
                    public ITABFeatureFont
{
  protected:
    double      m_dAngle;
    GInt16      m_nFontStyle;           // Bold/shadow/halo/etc.

  public:
             TABFontPoint(OGRFeatureDefn *poDefnIn);
    virtual ~TABFontPoint();

    virtual TABFeatureClass GetFeatureClass() { return TABFCFontPoint; };

    virtual int ReadGeometryFromMAPFile(TABMAPFile *poMapFile);
    virtual int WriteGeometryToMAPFile(TABMAPFile *poMapFile);

    virtual int ReadGeometryFromMIFFile(MIDDATAFile *fp);
    virtual int WriteGeometryToMIFFile(MIDDATAFile *fp);

    GBool       QueryFontStyle(TABFontStyle eStyleToQuery);
    void        ToggleFontStyle(TABFontStyle eStyleToToggle, GBool bStatus);

    int         GetFontStyleMIFValue();
    void        SetFontStyleMIFValue(int nStyle);
    int         GetFontStyleTABValue()           {return m_nFontStyle;};
    void        SetFontStyleTABValue(int nStyle){m_nFontStyle=(GInt16)nStyle;};

    // GetSymbolAngle(): Return angle in degrees counterclockwise
    double      GetSymbolAngle()        {return m_dAngle;};
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
class TABCustomPoint: public TABPoint, 
                      public ITABFeatureFont
{
  public:
    GByte       m_nUnknown_;
    GByte       m_nCustomStyle;         // Show BG/Apply Color		       


  public:
             TABCustomPoint(OGRFeatureDefn *poDefnIn);
    virtual ~TABCustomPoint();

    virtual TABFeatureClass GetFeatureClass() { return TABFCCustomPoint; };

    virtual int ReadGeometryFromMAPFile(TABMAPFile *poMapFile);
    virtual int WriteGeometryToMAPFile(TABMAPFile *poMapFile);

    virtual int ReadGeometryFromMIFFile(MIDDATAFile *fp);
    virtual int WriteGeometryToMIFFile(MIDDATAFile *fp);

    const char *GetSymbolNameRef()      { return GetFontNameRef(); };
    void        SetSymbolName(const char *pszName) {SetFontName(pszName);};
    

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
 *
 * Feature geometry can be either a OGRLineString or a OGRMultiLineString
 *--------------------------------------------------------------------*/
class TABPolyline: public TABFeature, 
                   public ITABFeaturePen
{
  public:
             TABPolyline(OGRFeatureDefn *poDefnIn);
    virtual ~TABPolyline();

    virtual TABFeatureClass GetFeatureClass() { return TABFCPolyline; };
    virtual int             ValidateMapInfoType();

    virtual int ReadGeometryFromMAPFile(TABMAPFile *poMapFile);
    virtual int WriteGeometryToMAPFile(TABMAPFile *poMapFile);

    virtual int ReadGeometryFromMIFFile(MIDDATAFile *fp);
    virtual int WriteGeometryToMIFFile(MIDDATAFile *fp);

    virtual void DumpMIF(FILE *fpOut = NULL);

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
 *
 * Feature geometry will be OGRPolygon
 *--------------------------------------------------------------------*/
class TABRegion: public TABFeature, 
                 public ITABFeaturePen, 
                 public ITABFeatureBrush
{
    GBool       m_bSmooth;
    GBool       m_bCentroid;
    double      m_dfCentroidX, m_dfCentroidY;
  public:
             TABRegion(OGRFeatureDefn *poDefnIn);
    virtual ~TABRegion();

    virtual TABFeatureClass GetFeatureClass() { return TABFCRegion; };
    virtual int             ValidateMapInfoType();

    virtual int ReadGeometryFromMAPFile(TABMAPFile *poMapFile);
    virtual int WriteGeometryToMAPFile(TABMAPFile *poMapFile);

    virtual int ReadGeometryFromMIFFile(MIDDATAFile *fp);
    virtual int WriteGeometryToMIFFile(MIDDATAFile *fp);

    virtual void DumpMIF(FILE *fpOut = NULL);
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
 * Its corners can optionaly be rounded, in which case a X and Y rounding
 * radius will be defined.
 *
 * Feature geometry will be OGRPolygon
 *--------------------------------------------------------------------*/
class TABRectangle: public TABFeature, 
                    public ITABFeaturePen, 
                    public ITABFeatureBrush
{
  public:
             TABRectangle(OGRFeatureDefn *poDefnIn);
    virtual ~TABRectangle();

    virtual TABFeatureClass GetFeatureClass() { return TABFCRectangle; };
    virtual int             ValidateMapInfoType();

    virtual int ReadGeometryFromMAPFile(TABMAPFile *poMapFile);
    virtual int WriteGeometryToMAPFile(TABMAPFile *poMapFile);

    virtual int ReadGeometryFromMIFFile(MIDDATAFile *fp);
    virtual int WriteGeometryToMIFFile(MIDDATAFile *fp);

    virtual void DumpMIF(FILE *fpOut = NULL);

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
class TABEllipse: public TABFeature, 
                  public ITABFeaturePen, 
                  public ITABFeatureBrush
{

  public:
             TABEllipse(OGRFeatureDefn *poDefnIn);
    virtual ~TABEllipse();

    virtual TABFeatureClass GetFeatureClass() { return TABFCEllipse; };
    virtual int             ValidateMapInfoType();

    virtual int ReadGeometryFromMAPFile(TABMAPFile *poMapFile);
    virtual int WriteGeometryToMAPFile(TABMAPFile *poMapFile);

    virtual int ReadGeometryFromMIFFile(MIDDATAFile *fp);
    virtual int WriteGeometryToMIFFile(MIDDATAFile *fp);

    virtual void DumpMIF(FILE *fpOut = NULL);

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
class TABArc: public TABFeature, 
              public ITABFeaturePen
{
  private:
    double      m_dStartAngle;  // In degrees, counterclockwise, 
    double      m_dEndAngle;    // starting at 3 o'clock

  public:
             TABArc(OGRFeatureDefn *poDefnIn);
    virtual ~TABArc();

    virtual TABFeatureClass GetFeatureClass() { return TABFCArc; };
    virtual int             ValidateMapInfoType();

    virtual int ReadGeometryFromMAPFile(TABMAPFile *poMapFile);
    virtual int WriteGeometryToMAPFile(TABMAPFile *poMapFile);

    virtual int ReadGeometryFromMIFFile(MIDDATAFile *fp);
    virtual int WriteGeometryToMIFFile(MIDDATAFile *fp);

    virtual void DumpMIF(FILE *fpOut = NULL);

    double      GetStartAngle() { return m_dStartAngle; };
    double      GetEndAngle()   { return m_dEndAngle; };
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
class TABText: public TABFeature, 
               public ITABFeatureFont,
               public ITABFeaturePen
{
  protected:
    char        *m_pszString;

    double      m_dAngle;
    double      m_dHeight;
    double      m_dWidth;
    double      m_dfLineX;
    double      m_dfLineY;
    void        UpdateTextMBR();

    GInt32      m_rgbForeground;
    GInt32      m_rgbBackground;

    GInt16      m_nTextAlignment;       // Justification/Vert.Spacing/arrow
    GInt16      m_nFontStyle;           // Bold/italic/underlined/shadow/...

  public:
             TABText(OGRFeatureDefn *poDefnIn);
    virtual ~TABText();

    virtual TABFeatureClass GetFeatureClass() { return TABFCText; };
    virtual int             ValidateMapInfoType();

    virtual int ReadGeometryFromMAPFile(TABMAPFile *poMapFile);
    virtual int WriteGeometryToMAPFile(TABMAPFile *poMapFile);

    virtual int ReadGeometryFromMIFFile(MIDDATAFile *fp);
    virtual int WriteGeometryToMIFFile(MIDDATAFile *fp);

    virtual void DumpMIF(FILE *fpOut = NULL);

    const char *GetTextString();
    double      GetTextAngle();
    double      GetTextBoxHeight();
    double      GetTextBoxWidth();
    GInt32      GetFontFGColor();
    GInt32      GetFontBGColor();

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

    void        SetTextJustification(TABTextJust eJust);
    void        SetTextSpacing(TABTextSpacing eSpacing);
    void        SetTextLineType(TABTextLineType eLineType);
    void        ToggleFontStyle(TABFontStyle eStyleToToggle, GBool bStatus);

    int         GetFontStyleMIFValue();
    void        SetFontStyleMIFValue(int nStyle, GBool bBGColorSet=FALSE);
    GBool       IsFontBGColorUsed();
    int         GetFontStyleTABValue()           {return m_nFontStyle;};
    void        SetFontStyleTABValue(int nStyle){m_nFontStyle=(GInt16)nStyle;};

};



/*---------------------------------------------------------------------
 *                      class TABDebugFeature
 *
 * Feature class to use for testing purposes... this one does not 
 * correspond to any MapInfo type... it's just used to dump info about
 * feature types that are not implemented yet.
 *--------------------------------------------------------------------*/
class TABDebugFeature: public TABFeature
{
  private:
    GByte       m_abyBuf[512];
    int         m_nSize;
    int         m_nCoordDataPtr;  // -1 if none
    int         m_nCoordDataSize;

  public:
             TABDebugFeature(OGRFeatureDefn *poDefnIn);
    virtual ~TABDebugFeature();

    virtual TABFeatureClass GetFeatureClass() { return TABFCDebugFeature; };

    virtual int ReadGeometryFromMAPFile(TABMAPFile *poMapFile);
    virtual int WriteGeometryToMAPFile(TABMAPFile *poMapFile);

    virtual int ReadGeometryFromMIFFile(MIDDATAFile *fp);
    virtual int WriteGeometryToMIFFile(MIDDATAFile *fp);

    virtual void DumpMIF(FILE *fpOut = NULL);
};


#endif /* _MITAB_H_INCLUDED_ */

