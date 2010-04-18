/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Define of Feature Representation
 * Author:   Stephane Villeneuve, stephane.v@videtron.ca
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
 ****************************************************************************/

#ifndef OGR_FEATURESTYLE_INCLUDE
#define OGR_FEATURESTYLE_INCLUDE

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_core.h"

class OGRFeature;

/**
 * \file ogr_featurestyle.h
 *
 * Simple feature style classes.
 */

/*
 * All OGRStyleTool param lists are defined in ogr_core.h.
 */

typedef enum ogr_style_type
{
    OGRSTypeString,
    OGRSTypeDouble,
    OGRSTypeInteger,
    OGRSTypeBoolean
}  OGRSType;

typedef struct ogr_style_param
{
    int              eParam;
    const char       *pszToken;
    GBool            bGeoref;
    OGRSType         eType;
} OGRStyleParamId;


typedef struct ogr_style_value
{
    char            *pszValue;
    double           dfValue;
    int              nValue; // Used for both integer and boolean types
    GBool            bValid;
    OGRSTUnitId      eUnit;
} OGRStyleValue;


//Everytime a pszStyleString gived in parameter is NULL, 
//    the StyleString defined in the Mgr will be use.
/**
 * This class represents a style table
 */
class CPL_DLL OGRStyleTable
{
  private:
    char **m_papszStyleTable;

    CPLString osLastRequestedStyleName;
    int iNextStyle;

  public:
    OGRStyleTable();
    ~OGRStyleTable();
    GBool AddStyle(const char *pszName,const char *pszStyleString);
    GBool RemoveStyle(const char *pszName);
    GBool ModifyStyle(const char *pszName, const char *pszStyleString);
    
    GBool SaveStyleTable(const char *pszFilename);
    GBool LoadStyleTable(const char *pszFilename);
    const char *Find(const char *pszStyleString);
    GBool IsExist(const char *pszName);
    const char *GetStyleName(const char *pszName);
    void  Print(FILE *fpOut);
    void  Clear();
    OGRStyleTable   *Clone();
    void ResetStyleStringReading();
    const char *GetNextStyle();
    const char *GetLastStyleName();
};


class OGRStyleTool;

/**
 * This class represents a style manager
 */
class CPL_DLL OGRStyleMgr
{
  private:
    OGRStyleTable   *m_poDataSetStyleTable;
    char            *m_pszStyleString;

  public:
    OGRStyleMgr(OGRStyleTable *poDataSetStyleTable = NULL);
    ~OGRStyleMgr();

    GBool SetFeatureStyleString(OGRFeature *,const char *pszStyleString=NULL,
                                GBool bNoMatching = FALSE);
    /*it will set in the gived feature the pszStyleString with 
            the style or will set the style name found in 
            dataset StyleTable (if bNoMatching == FALSE)*/
              
    const char *InitFromFeature(OGRFeature *);
    GBool InitStyleString(const char *pszStyleString = NULL);
    
    const char *GetStyleName(const char *pszStyleString= NULL);
    const char *GetStyleByName(const char *pszStyleName);
    
    GBool AddStyle(const char *pszStyleName, const char *pszStyleString=NULL);
    
    const char *GetStyleString(OGRFeature * = NULL);
 
    GBool AddPart(OGRStyleTool *);
    GBool AddPart(const char *);

    int GetPartCount(const char *pszStyleString = NULL);
    OGRStyleTool *GetPart(int hPartId, const char *pszStyleString = NULL);
    
    /*It could have a reference counting processus for the OGRStyleTable, if
      needed */
      
    OGRStyleTable *GetDataSetStyleTable(){return m_poDataSetStyleTable;}
    
    OGRStyleTool *CreateStyleToolFromStyleString(const char *pszStyleString);

};

/**
 * This class represents a style tool
 */
class CPL_DLL OGRStyleTool
{
  private:
    GBool m_bModified;
    GBool m_bParsed;
    double m_dfScale;
    OGRSTUnitId m_eUnit;
    OGRSTClassId m_eClassId;
    char *m_pszStyleString;

    virtual GBool Parse() = 0;

  protected:
    GBool Parse(const OGRStyleParamId* pasStyle,
                OGRStyleValue* pasValue,
                int nCount);

  public:
    
    OGRStyleTool(){}
    OGRStyleTool(OGRSTClassId eClassId);
    virtual ~OGRStyleTool();

    GBool GetRGBFromString(const char *pszColor, int &nRed, int &nGreen, 
                           int &nBlue, int &nTransparence);
    int   GetSpecificId(const char *pszId, const char *pszWanted);

    GBool IsStyleModified() {return m_bModified;}
    void  StyleModified() {m_bModified = TRUE;}

    GBool IsStyleParsed() {return m_bParsed;}
    void  StyleParsed() {m_bParsed = TRUE;}
    
    OGRSTClassId GetType();

    void SetInternalInputUnitFromParam(char *pszString);
    
    void SetUnit(OGRSTUnitId,double dfScale = 1.0); //the dfScale will be
         //used if we are working with Ground Unit ( ground = paper * scale);

    OGRSTUnitId GetUnit(){return m_eUnit;}
    
    /* It's existe two way to set the parameters in the Style, with generic
methodes (using a defined enumeration) or with the reel method specific
for Each style tools.*/
    
    virtual const char *GetStyleString() = 0;
    void SetStyleString(const char *pszStyleString);
    const char *GetStyleString(const OGRStyleParamId *pasStyleParam ,
                            OGRStyleValue *pasStyleValue, int nSize);

    const char *GetParamStr(const OGRStyleParamId &sStyleParam ,
                            OGRStyleValue &sStyleValue,
                            GBool &bValueIsNull);

    int GetParamNum(const OGRStyleParamId &sStyleParam ,
                    OGRStyleValue &sStyleValue,
                    GBool &bValueIsNull);

    double GetParamDbl(const OGRStyleParamId &sStyleParam ,
                       OGRStyleValue &sStyleValue,
                       GBool &bValueIsNull);
    
    void SetParamStr(const OGRStyleParamId &sStyleParam ,
                     OGRStyleValue &sStyleValue,
                     const char *pszParamString);
    
    void SetParamNum(const OGRStyleParamId &sStyleParam ,
                     OGRStyleValue &sStyleValue,
                     int nParam);

    void SetParamDbl(const OGRStyleParamId &sStyleParam ,
                     OGRStyleValue &sStyleValue,
                     double dfParam);

    double ComputeWithUnit(double, OGRSTUnitId);
    int    ComputeWithUnit(int , OGRSTUnitId);

};

/**
 * This class represents a style pen
 */
class CPL_DLL OGRStylePen : public OGRStyleTool
{
  private:

    OGRStyleValue    *m_pasStyleValue;

    GBool Parse();

  public:

    OGRStylePen();
    virtual ~OGRStylePen(); 

    /**********************************************************************/
    /* Explicit fct for all parameters defined in the Drawing tools  Pen  */
    /**********************************************************************/
     
    const char *Color(GBool &bDefault){return GetParamStr(OGRSTPenColor,bDefault);}
    void SetColor(const char *pszColor){SetParamStr(OGRSTPenColor,pszColor);}
    double Width(GBool &bDefault){return GetParamDbl(OGRSTPenWidth,bDefault);}
    void SetWidth(double dfWidth){SetParamDbl(OGRSTPenWidth,dfWidth);}
    const char *Pattern(GBool &bDefault){return (const char *)GetParamStr(OGRSTPenPattern,bDefault);}
    void SetPattern(const char *pszPattern){SetParamStr(OGRSTPenPattern,pszPattern);}
    const char *Id(GBool &bDefault){return GetParamStr(OGRSTPenId,bDefault);}
    void SetId(const char *pszId){SetParamStr(OGRSTPenId,pszId);}
    double PerpendicularOffset(GBool &bDefault){return GetParamDbl(OGRSTPenPerOffset,bDefault);}
    void SetPerpendicularOffset(double dfPerp){SetParamDbl(OGRSTPenPerOffset,dfPerp);}
    const char *Cap(GBool &bDefault){return GetParamStr(OGRSTPenCap,bDefault);}
    void SetCap(const char *pszCap){SetParamStr(OGRSTPenCap,pszCap);}
    const char *Join(GBool &bDefault){return GetParamStr(OGRSTPenJoin,bDefault);}
    void SetJoin(const char *pszJoin){SetParamStr(OGRSTPenJoin,pszJoin);}
    int  Priority(GBool &bDefault){return GetParamNum(OGRSTPenPriority,bDefault);}
    void SetPriority(int nPriority){SetParamNum(OGRSTPenPriority,nPriority);}
    
    /*****************************************************************/
    
    const char *GetParamStr(OGRSTPenParam eParam, GBool &bValueIsNull);
    int GetParamNum(OGRSTPenParam eParam,GBool &bValueIsNull);
    double GetParamDbl(OGRSTPenParam eParam,GBool &bValueIsNull);
    void SetParamStr(OGRSTPenParam eParam, const char *pszParamString);
    void SetParamNum(OGRSTPenParam eParam, int nParam);
    void SetParamDbl(OGRSTPenParam eParam, double dfParam);
    const char *GetStyleString();
};

/**
 * This class represents a style brush
 */
class CPL_DLL OGRStyleBrush : public OGRStyleTool
{
  private:

    OGRStyleValue    *m_pasStyleValue;

    GBool Parse();

  public:

    OGRStyleBrush();
    virtual ~OGRStyleBrush();

    /* Explicit fct for all parameters defined in the Drawing tools Brush */

    const char *ForeColor(GBool &bDefault){return GetParamStr(OGRSTBrushFColor,bDefault);}
    void SetForeColor(const char *pszColor){SetParamStr(OGRSTBrushFColor,pszColor);}
    const char *BackColor(GBool &bDefault){return GetParamStr(OGRSTBrushBColor,bDefault);}
    void SetBackColor(const char *pszColor){SetParamStr(OGRSTBrushBColor,pszColor);}
    const char *Id(GBool &bDefault){ return GetParamStr(OGRSTBrushId,bDefault);}
    void  SetId(const char *pszId){SetParamStr(OGRSTBrushId,pszId);}
    double Angle(GBool &bDefault){return GetParamDbl(OGRSTBrushAngle,bDefault);}
    void SetAngle(double dfAngle){SetParamDbl(OGRSTBrushAngle,dfAngle );}
    double Size(GBool &bDefault){return GetParamDbl(OGRSTBrushSize,bDefault);}
    void SetSize(double dfSize){SetParamDbl(OGRSTBrushSize,dfSize  );}
    double SpacingX(GBool &bDefault){return GetParamDbl(OGRSTBrushDx,bDefault);}
    void SetSpacingX(double dfX){SetParamDbl(OGRSTBrushDx,dfX );}
    double SpacingY(GBool &bDefault){return GetParamDbl(OGRSTBrushDy,bDefault);}
    void SetSpacingY(double dfY){SetParamDbl(OGRSTBrushDy,dfY  );}
    int  Priority(GBool &bDefault){ return GetParamNum(OGRSTBrushPriority,bDefault);}
    void SetPriority(int nPriority){ SetParamNum(OGRSTBrushPriority,nPriority);}
    

    /*****************************************************************/
    
     const char *GetParamStr(OGRSTBrushParam eParam, GBool &bValueIsNull);
     int GetParamNum(OGRSTBrushParam eParam,GBool &bValueIsNull);
     double GetParamDbl(OGRSTBrushParam eParam,GBool &bValueIsNull);
     void SetParamStr(OGRSTBrushParam eParam, const char *pszParamString);
     void SetParamNum(OGRSTBrushParam eParam, int nParam);
     void SetParamDbl(OGRSTBrushParam eParam, double dfParam);
     const char *GetStyleString();
};

/**
 * This class represents a style symbol
 */
class CPL_DLL OGRStyleSymbol : public OGRStyleTool
{
  private:

    OGRStyleValue    *m_pasStyleValue;

    GBool Parse();

  public:

    OGRStyleSymbol();
    virtual ~OGRStyleSymbol();

    /*****************************************************************/
    /* Explicit fct for all parameters defined in the Drawing tools  */
    /*****************************************************************/
    
    const char *Id(GBool &bDefault){return GetParamStr(OGRSTSymbolId,bDefault);}
    void  SetId(const char *pszId){ SetParamStr(OGRSTSymbolId,pszId);}
    double Angle(GBool &bDefault){ return GetParamDbl(OGRSTSymbolAngle,bDefault);}
    void SetAngle(double dfAngle){SetParamDbl(OGRSTSymbolAngle,dfAngle );}
    const char *Color(GBool &bDefault){return GetParamStr(OGRSTSymbolColor,bDefault);}
    void SetColor(const char *pszColor){SetParamStr(OGRSTSymbolColor,pszColor);}
    double Size(GBool &bDefault){  return GetParamDbl(OGRSTSymbolSize,bDefault);}
    void SetSize(double dfSize){  SetParamDbl(OGRSTSymbolSize,dfSize  );}
    double SpacingX(GBool &bDefault){return GetParamDbl(OGRSTSymbolDx,bDefault);}
    void SetSpacingX(double dfX){SetParamDbl(OGRSTSymbolDx,dfX  );}
    double SpacingY(GBool &bDefault){return GetParamDbl(OGRSTSymbolDy,bDefault);}
    void SetSpacingY(double dfY){SetParamDbl(OGRSTSymbolDy,dfY  );}
    double Step(GBool &bDefault){return GetParamDbl(OGRSTSymbolStep,bDefault);}
    void SetStep(double dfStep){SetParamDbl(OGRSTSymbolStep,dfStep  );}
    double Offset(GBool &bDefault){return GetParamDbl(OGRSTSymbolOffset,bDefault);}
    void SetOffset(double dfOffset){SetParamDbl(OGRSTSymbolOffset,dfOffset  );}
    double Perp(GBool &bDefault){return GetParamDbl(OGRSTSymbolPerp,bDefault);}
    void SetPerp(double dfPerp){SetParamDbl(OGRSTSymbolPerp,dfPerp  );}  
    int  Priority(GBool &bDefault){return GetParamNum(OGRSTSymbolPriority,bDefault);}
    void SetPriority(int nPriority){SetParamNum(OGRSTSymbolPriority,nPriority);}
    const char *FontName(GBool &bDefault)
        {return GetParamStr(OGRSTSymbolFontName,bDefault);}
    void SetFontName(const char *pszFontName)
        {SetParamStr(OGRSTSymbolFontName,pszFontName);}
    const char *OColor(GBool &bDefault){return GetParamStr(OGRSTSymbolOColor,bDefault);}
    void SetOColor(const char *pszColor){SetParamStr(OGRSTSymbolOColor,pszColor);}

    /*****************************************************************/
    
     const char *GetParamStr(OGRSTSymbolParam eParam, GBool &bValueIsNull);
     int GetParamNum(OGRSTSymbolParam eParam,GBool &bValueIsNull);
     double GetParamDbl(OGRSTSymbolParam eParam,GBool &bValueIsNull);
     void SetParamStr(OGRSTSymbolParam eParam, const char *pszParamString);
     void SetParamNum(OGRSTSymbolParam eParam, int nParam);
     void SetParamDbl(OGRSTSymbolParam eParam, double dfParam);
     const char *GetStyleString();
};

/**
 * This class represents a style label
 */
class CPL_DLL OGRStyleLabel : public OGRStyleTool
{
  private:

    OGRStyleValue    *m_pasStyleValue;

    GBool Parse();

  public:

    OGRStyleLabel();
    virtual ~OGRStyleLabel();

    /*****************************************************************/
    /* Explicit fct for all parameters defined in the Drawing tools  */
    /*****************************************************************/
    
    const char *FontName(GBool &bDefault){return GetParamStr(OGRSTLabelFontName,bDefault);}
    void  SetFontName(const char *pszFontName){SetParamStr(OGRSTLabelFontName,pszFontName);}
    double Size(GBool &bDefault){return GetParamDbl(OGRSTLabelSize,bDefault);}
    void SetSize(double dfSize){SetParamDbl(OGRSTLabelSize,dfSize);}
    const char *TextString(GBool &bDefault){return GetParamStr(OGRSTLabelTextString,bDefault);}
    void SetTextString(const char *pszTextString){SetParamStr(OGRSTLabelTextString,pszTextString);}
    double Angle(GBool &bDefault){return GetParamDbl(OGRSTLabelAngle,bDefault);}
    void SetAngle(double dfAngle){SetParamDbl(OGRSTLabelAngle,dfAngle);}
    const char *ForeColor(GBool &bDefault){return GetParamStr(OGRSTLabelFColor,bDefault);}
    void SetForColor(const char *pszForColor){SetParamStr(OGRSTLabelFColor,pszForColor);}
    const char *BackColor(GBool &bDefault){return GetParamStr(OGRSTLabelBColor,bDefault);}
    void SetBackColor(const char *pszBackColor){SetParamStr(OGRSTLabelBColor,pszBackColor);}
    const char *Placement(GBool &bDefault){return GetParamStr(OGRSTLabelPlacement,bDefault);}
    void SetPlacement(const char *pszPlacement){SetParamStr(OGRSTLabelPlacement,pszPlacement);}
    int  Anchor(GBool &bDefault){return GetParamNum(OGRSTLabelAnchor,bDefault);}
    void SetAnchor(int nAnchor){SetParamNum(OGRSTLabelAnchor,nAnchor);}
    double SpacingX(GBool &bDefault){return GetParamDbl(OGRSTLabelDx,bDefault);}
    void SetSpacingX(double dfX){SetParamDbl(OGRSTLabelDx,dfX);}
    double SpacingY(GBool &bDefault){return GetParamDbl(OGRSTLabelDy,bDefault);}
    void SetSpacingY(double dfY){SetParamDbl(OGRSTLabelDy,dfY);}
    double Perp(GBool &bDefault){return GetParamDbl(OGRSTLabelPerp,bDefault);}
    void SetPerp(double dfPerp){SetParamDbl(OGRSTLabelPerp,dfPerp);}  
    GBool Bold(GBool &bDefault){return GetParamNum(OGRSTLabelBold,bDefault);}
    void SetBold(GBool bBold){SetParamNum(OGRSTLabelBold,bBold);}
    GBool Italic(GBool &bDefault){return GetParamNum(OGRSTLabelItalic,bDefault);}
    void SetItalic(GBool bItalic){SetParamNum(OGRSTLabelItalic,bItalic);}
    GBool Underline(GBool &bDefault){return GetParamNum(OGRSTLabelUnderline,bDefault);}
    void SetUnderline(GBool bUnderline){SetParamNum(OGRSTLabelUnderline,bUnderline);}
    int  Priority(GBool &bDefault){return GetParamNum(OGRSTLabelPriority,bDefault);}
    void SetPriority(int nPriority){SetParamNum(OGRSTLabelPriority,nPriority);}
    GBool Strikeout(GBool &bDefault){return GetParamNum(OGRSTLabelStrikeout,bDefault);}
    void SetStrikeout(GBool bStrikeout){SetParamNum(OGRSTLabelStrikeout,bStrikeout);}
    double Stretch(GBool &bDefault){return GetParamDbl(OGRSTLabelStretch,bDefault);}
    void SetStretch(double dfStretch){SetParamDbl(OGRSTLabelStretch,dfStretch);}
    const char *AdjustmentHor(GBool &bDefault){return GetParamStr(OGRSTLabelAdjHor,bDefault);}
    void SetAdjustmentHor(const char *pszAdjustmentHor){SetParamStr(OGRSTLabelAdjHor,pszAdjustmentHor);}
    const char *AdjustmentVert(GBool &bDefault){return GetParamStr(OGRSTLabelAdjVert,bDefault);}
    void SetAdjustmentVert(const char *pszAdjustmentVert){SetParamStr(OGRSTLabelAdjHor,pszAdjustmentVert);}
    const char *ShadowColor(GBool &bDefault){return GetParamStr(OGRSTLabelHColor,bDefault);}
    void SetShadowColor(const char *pszShadowColor){SetParamStr(OGRSTLabelHColor,pszShadowColor);}
    const char *OutlineColor(GBool &bDefault){return GetParamStr(OGRSTLabelOColor,bDefault);}
    void SetOutlineColor(const char *pszOutlineColor){SetParamStr(OGRSTLabelOColor,pszOutlineColor);}
    
    /*****************************************************************/
    
     const char *GetParamStr(OGRSTLabelParam eParam, GBool &bValueIsNull);
     int GetParamNum(OGRSTLabelParam eParam,GBool &bValueIsNull);
     double GetParamDbl(OGRSTLabelParam eParam,GBool &bValueIsNull);
     void SetParamStr(OGRSTLabelParam eParam, const char *pszParamString);
     void SetParamNum(OGRSTLabelParam eParam, int nParam);
     void SetParamDbl(OGRSTLabelParam eParam, double dfParam);
     const char *GetStyleString();
};

#endif /* OGR_FEATURESTYLE_INCLUDE */
