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

/** OGR Style type */
typedef enum ogr_style_type
{
    OGRSTypeUnused = -1,
    OGRSTypeString,
    OGRSTypeDouble,
    OGRSTypeInteger,
    OGRSTypeBoolean
} OGRSType;

//! @cond Doxygen_Suppress
typedef struct ogr_style_param
{
    int eParam;
    const char *pszToken;
    bool bGeoref;
    OGRSType eType;
} OGRStyleParamId;

typedef struct ogr_style_value
{
    char *pszValue;
    double dfValue;
    int nValue;  // Used for both integer and boolean types
    bool bValid;
    OGRSTUnitId eUnit;
} OGRStyleValue;
//! @endcond

// Every time a pszStyleString given in parameter is NULL,
// the StyleString defined in the Mgr will be use.

/**
 * This class represents a style table
 */
class CPL_DLL OGRStyleTable
{
  private:
    char **m_papszStyleTable = nullptr;

    CPLString osLastRequestedStyleName{};
    int iNextStyle = 0;

    CPL_DISALLOW_COPY_ASSIGN(OGRStyleTable)

  public:
    OGRStyleTable();
    ~OGRStyleTable();
    bool AddStyle(const char *pszName, const char *pszStyleString);
    bool RemoveStyle(const char *pszName);
    bool ModifyStyle(const char *pszName, const char *pszStyleString);

    bool SaveStyleTable(const char *pszFilename);
    bool LoadStyleTable(const char *pszFilename);
    const char *Find(const char *pszStyleString);
    int IsExist(const char *pszName);
    const char *GetStyleName(const char *pszName);
    void Print(FILE *fpOut);
    void Clear();
    OGRStyleTable *Clone();
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
    OGRStyleTable *m_poDataSetStyleTable = nullptr;
    char *m_pszStyleString = nullptr;

    CPL_DISALLOW_COPY_ASSIGN(OGRStyleMgr)

  public:
    explicit OGRStyleMgr(OGRStyleTable *poDataSetStyleTable = nullptr);
    ~OGRStyleMgr();

    bool SetFeatureStyleString(OGRFeature *,
                               const char *pszStyleString = nullptr,
                               bool bNoMatching = FALSE);
    /* It will set in the given feature the pszStyleString with
            the style or will set the style name found in
            dataset StyleTable (if bNoMatching == FALSE). */

    const char *InitFromFeature(OGRFeature *);
    bool InitStyleString(const char *pszStyleString = nullptr);

    const char *GetStyleName(const char *pszStyleString = nullptr);
    const char *GetStyleByName(const char *pszStyleName);

    bool AddStyle(const char *pszStyleName,
                  const char *pszStyleString = nullptr);

    const char *GetStyleString(OGRFeature * = nullptr);

    bool AddPart(OGRStyleTool *);
    bool AddPart(const char *);

    int GetPartCount(const char *pszStyleString = nullptr);
    OGRStyleTool *GetPart(int hPartId, const char *pszStyleString = nullptr);

    /* It could have a reference counting process us for the OGRStyleTable, if
      needed. */
    //! @cond Doxygen_Suppress
    OGRStyleTable *GetDataSetStyleTable()
    {
        return m_poDataSetStyleTable;
    }

    static OGRStyleTool *
    CreateStyleToolFromStyleString(const char *pszStyleString);
    //! @endcond
};

/**
 * This class represents a style tool
 */
class CPL_DLL OGRStyleTool
{
  private:
    bool m_bModified = false;
    bool m_bParsed = false;
    double m_dfScale = 1.0;
    OGRSTUnitId m_eUnit = OGRSTUMM;
    OGRSTClassId m_eClassId = OGRSTCNone;
    char *m_pszStyleString = nullptr;

    virtual bool Parse() = 0;

    CPL_DISALLOW_COPY_ASSIGN(OGRStyleTool)

  protected:
#ifndef DOXYGEN_SKIP
    bool Parse(const OGRStyleParamId *pasStyle, OGRStyleValue *pasValue,
               int nCount);
#endif

  public:
    OGRStyleTool()
        : m_bModified(FALSE), m_bParsed(FALSE), m_dfScale(0.0),
          m_eUnit(OGRSTUGround), m_eClassId(OGRSTCNone),
          m_pszStyleString(nullptr)
    {
    }
    explicit OGRStyleTool(OGRSTClassId eClassId);
    virtual ~OGRStyleTool();

    static bool GetRGBFromString(const char *pszColor, int &nRed, int &nGreen,
                                 int &nBlue, int &nTransparence);
    static int GetSpecificId(const char *pszId, const char *pszWanted);

#ifndef DOXYGEN_SKIP
    bool IsStyleModified()
    {
        return m_bModified;
    }
    void StyleModified()
    {
        m_bModified = TRUE;
    }

    bool IsStyleParsed()
    {
        return m_bParsed;
    }
    void StyleParsed()
    {
        m_bParsed = TRUE;
    }
#endif

    OGRSTClassId GetType();

#ifndef DOXYGEN_SKIP
    void SetInternalInputUnitFromParam(char *pszString);
#endif

    void SetUnit(OGRSTUnitId,
                 double dfScale = 1.0);  // the dfScale will be
                                         // used if we are working with Ground
                                         // Unit ( ground = paper * scale);

    OGRSTUnitId GetUnit()
    {
        return m_eUnit;
    }

    // There are two way to set the parameters in the Style, with generic
    // methods (using a defined enumeration) or with the reel method specific
    // for Each style tools.

    virtual const char *GetStyleString() = 0;
    void SetStyleString(const char *pszStyleString);
    const char *GetStyleString(const OGRStyleParamId *pasStyleParam,
                               OGRStyleValue *pasStyleValue, int nSize);

    const char *GetParamStr(const OGRStyleParamId &sStyleParam,
                            OGRStyleValue &sStyleValue, bool &bValueIsNull);

    int GetParamNum(const OGRStyleParamId &sStyleParam,
                    OGRStyleValue &sStyleValue, bool &bValueIsNull);

    double GetParamDbl(const OGRStyleParamId &sStyleParam,
                       OGRStyleValue &sStyleValue, bool &bValueIsNull);

    void SetParamStr(const OGRStyleParamId &sStyleParam,
                     OGRStyleValue &sStyleValue, const char *pszParamString);

    void SetParamNum(const OGRStyleParamId &sStyleParam,
                     OGRStyleValue &sStyleValue, int nParam);

    void SetParamDbl(const OGRStyleParamId &sStyleParam,
                     OGRStyleValue &sStyleValue, double dfParam);
#ifndef DOXYGEN_SKIP
    double ComputeWithUnit(double, OGRSTUnitId);
    int ComputeWithUnit(int, OGRSTUnitId);
#endif
};

//! @cond Doxygen_Suppress

/**
 * This class represents a style pen
 */
class CPL_DLL OGRStylePen : public OGRStyleTool
{
  private:
    OGRStyleValue *m_pasStyleValue;

    bool Parse() override;

    CPL_DISALLOW_COPY_ASSIGN(OGRStylePen)

  public:
    OGRStylePen();
    ~OGRStylePen() override;

    /**********************************************************************/
    /* Explicit fct for all parameters defined in the Drawing tools  Pen  */
    /**********************************************************************/

    const char *Color(bool &bDefault)
    {
        return GetParamStr(OGRSTPenColor, bDefault);
    }
    void SetColor(const char *pszColor)
    {
        SetParamStr(OGRSTPenColor, pszColor);
    }
    double Width(bool &bDefault)
    {
        return GetParamDbl(OGRSTPenWidth, bDefault);
    }
    void SetWidth(double dfWidth)
    {
        SetParamDbl(OGRSTPenWidth, dfWidth);
    }
    const char *Pattern(bool &bDefault)
    {
        return GetParamStr(OGRSTPenPattern, bDefault);
    }
    void SetPattern(const char *pszPattern)
    {
        SetParamStr(OGRSTPenPattern, pszPattern);
    }
    const char *Id(bool &bDefault)
    {
        return GetParamStr(OGRSTPenId, bDefault);
    }
    void SetId(const char *pszId)
    {
        SetParamStr(OGRSTPenId, pszId);
    }
    double PerpendicularOffset(bool &bDefault)
    {
        return GetParamDbl(OGRSTPenPerOffset, bDefault);
    }
    void SetPerpendicularOffset(double dfPerp)
    {
        SetParamDbl(OGRSTPenPerOffset, dfPerp);
    }
    const char *Cap(bool &bDefault)
    {
        return GetParamStr(OGRSTPenCap, bDefault);
    }
    void SetCap(const char *pszCap)
    {
        SetParamStr(OGRSTPenCap, pszCap);
    }
    const char *Join(bool &bDefault)
    {
        return GetParamStr(OGRSTPenJoin, bDefault);
    }
    void SetJoin(const char *pszJoin)
    {
        SetParamStr(OGRSTPenJoin, pszJoin);
    }
    int Priority(bool &bDefault)
    {
        return GetParamNum(OGRSTPenPriority, bDefault);
    }
    void SetPriority(int nPriority)
    {
        SetParamNum(OGRSTPenPriority, nPriority);
    }

    /*****************************************************************/

    const char *GetParamStr(OGRSTPenParam eParam, bool &bValueIsNull);
    int GetParamNum(OGRSTPenParam eParam, bool &bValueIsNull);
    double GetParamDbl(OGRSTPenParam eParam, bool &bValueIsNull);
    void SetParamStr(OGRSTPenParam eParam, const char *pszParamString);
    void SetParamNum(OGRSTPenParam eParam, int nParam);
    void SetParamDbl(OGRSTPenParam eParam, double dfParam);
    const char *GetStyleString() override;
};

/**
 * This class represents a style brush
 */
class CPL_DLL OGRStyleBrush : public OGRStyleTool
{
  private:
    OGRStyleValue *m_pasStyleValue;

    bool Parse() override;

    CPL_DISALLOW_COPY_ASSIGN(OGRStyleBrush)

  public:
    OGRStyleBrush();
    ~OGRStyleBrush() override;

    /* Explicit fct for all parameters defined in the Drawing tools Brush */

    const char *ForeColor(bool &bDefault)
    {
        return GetParamStr(OGRSTBrushFColor, bDefault);
    }
    void SetForeColor(const char *pszColor)
    {
        SetParamStr(OGRSTBrushFColor, pszColor);
    }
    const char *BackColor(bool &bDefault)
    {
        return GetParamStr(OGRSTBrushBColor, bDefault);
    }
    void SetBackColor(const char *pszColor)
    {
        SetParamStr(OGRSTBrushBColor, pszColor);
    }
    const char *Id(bool &bDefault)
    {
        return GetParamStr(OGRSTBrushId, bDefault);
    }
    void SetId(const char *pszId)
    {
        SetParamStr(OGRSTBrushId, pszId);
    }
    double Angle(bool &bDefault)
    {
        return GetParamDbl(OGRSTBrushAngle, bDefault);
    }
    void SetAngle(double dfAngle)
    {
        SetParamDbl(OGRSTBrushAngle, dfAngle);
    }
    double Size(bool &bDefault)
    {
        return GetParamDbl(OGRSTBrushSize, bDefault);
    }
    void SetSize(double dfSize)
    {
        SetParamDbl(OGRSTBrushSize, dfSize);
    }
    double SpacingX(bool &bDefault)
    {
        return GetParamDbl(OGRSTBrushDx, bDefault);
    }
    void SetSpacingX(double dfX)
    {
        SetParamDbl(OGRSTBrushDx, dfX);
    }
    double SpacingY(bool &bDefault)
    {
        return GetParamDbl(OGRSTBrushDy, bDefault);
    }
    void SetSpacingY(double dfY)
    {
        SetParamDbl(OGRSTBrushDy, dfY);
    }
    int Priority(bool &bDefault)
    {
        return GetParamNum(OGRSTBrushPriority, bDefault);
    }
    void SetPriority(int nPriority)
    {
        SetParamNum(OGRSTBrushPriority, nPriority);
    }

    /*****************************************************************/

    const char *GetParamStr(OGRSTBrushParam eParam, bool &bValueIsNull);
    int GetParamNum(OGRSTBrushParam eParam, bool &bValueIsNull);
    double GetParamDbl(OGRSTBrushParam eParam, bool &bValueIsNull);
    void SetParamStr(OGRSTBrushParam eParam, const char *pszParamString);
    void SetParamNum(OGRSTBrushParam eParam, int nParam);
    void SetParamDbl(OGRSTBrushParam eParam, double dfParam);
    const char *GetStyleString() override;
};

/**
 * This class represents a style symbol
 */
class CPL_DLL OGRStyleSymbol : public OGRStyleTool
{
  private:
    OGRStyleValue *m_pasStyleValue;

    bool Parse() override;

    CPL_DISALLOW_COPY_ASSIGN(OGRStyleSymbol)

  public:
    OGRStyleSymbol();
    ~OGRStyleSymbol() override;

    /*****************************************************************/
    /* Explicit fct for all parameters defined in the Drawing tools  */
    /*****************************************************************/

    const char *Id(bool &bDefault)
    {
        return GetParamStr(OGRSTSymbolId, bDefault);
    }
    void SetId(const char *pszId)
    {
        SetParamStr(OGRSTSymbolId, pszId);
    }
    double Angle(bool &bDefault)
    {
        return GetParamDbl(OGRSTSymbolAngle, bDefault);
    }
    void SetAngle(double dfAngle)
    {
        SetParamDbl(OGRSTSymbolAngle, dfAngle);
    }
    const char *Color(bool &bDefault)
    {
        return GetParamStr(OGRSTSymbolColor, bDefault);
    }
    void SetColor(const char *pszColor)
    {
        SetParamStr(OGRSTSymbolColor, pszColor);
    }
    double Size(bool &bDefault)
    {
        return GetParamDbl(OGRSTSymbolSize, bDefault);
    }
    void SetSize(double dfSize)
    {
        SetParamDbl(OGRSTSymbolSize, dfSize);
    }
    double SpacingX(bool &bDefault)
    {
        return GetParamDbl(OGRSTSymbolDx, bDefault);
    }
    void SetSpacingX(double dfX)
    {
        SetParamDbl(OGRSTSymbolDx, dfX);
    }
    double SpacingY(bool &bDefault)
    {
        return GetParamDbl(OGRSTSymbolDy, bDefault);
    }
    void SetSpacingY(double dfY)
    {
        SetParamDbl(OGRSTSymbolDy, dfY);
    }
    double Step(bool &bDefault)
    {
        return GetParamDbl(OGRSTSymbolStep, bDefault);
    }
    void SetStep(double dfStep)
    {
        SetParamDbl(OGRSTSymbolStep, dfStep);
    }
    double Offset(bool &bDefault)
    {
        return GetParamDbl(OGRSTSymbolOffset, bDefault);
    }
    void SetOffset(double dfOffset)
    {
        SetParamDbl(OGRSTSymbolOffset, dfOffset);
    }
    double Perp(bool &bDefault)
    {
        return GetParamDbl(OGRSTSymbolPerp, bDefault);
    }
    void SetPerp(double dfPerp)
    {
        SetParamDbl(OGRSTSymbolPerp, dfPerp);
    }
    int Priority(bool &bDefault)
    {
        return GetParamNum(OGRSTSymbolPriority, bDefault);
    }
    void SetPriority(int nPriority)
    {
        SetParamNum(OGRSTSymbolPriority, nPriority);
    }
    const char *FontName(bool &bDefault)
    {
        return GetParamStr(OGRSTSymbolFontName, bDefault);
    }
    void SetFontName(const char *pszFontName)
    {
        SetParamStr(OGRSTSymbolFontName, pszFontName);
    }
    const char *OColor(bool &bDefault)
    {
        return GetParamStr(OGRSTSymbolOColor, bDefault);
    }
    void SetOColor(const char *pszColor)
    {
        SetParamStr(OGRSTSymbolOColor, pszColor);
    }

    /*****************************************************************/

    const char *GetParamStr(OGRSTSymbolParam eParam, bool &bValueIsNull);
    int GetParamNum(OGRSTSymbolParam eParam, bool &bValueIsNull);
    double GetParamDbl(OGRSTSymbolParam eParam, bool &bValueIsNull);
    void SetParamStr(OGRSTSymbolParam eParam, const char *pszParamString);
    void SetParamNum(OGRSTSymbolParam eParam, int nParam);
    void SetParamDbl(OGRSTSymbolParam eParam, double dfParam);
    const char *GetStyleString() override;
};

/**
 * This class represents a style label
 */
class CPL_DLL OGRStyleLabel : public OGRStyleTool
{
  private:
    OGRStyleValue *m_pasStyleValue;

    bool Parse() override;

    CPL_DISALLOW_COPY_ASSIGN(OGRStyleLabel)

  public:
    OGRStyleLabel();
    ~OGRStyleLabel() override;

    /*****************************************************************/
    /* Explicit fct for all parameters defined in the Drawing tools  */
    /*****************************************************************/

    const char *FontName(bool &bDefault)
    {
        return GetParamStr(OGRSTLabelFontName, bDefault);
    }
    void SetFontName(const char *pszFontName)
    {
        SetParamStr(OGRSTLabelFontName, pszFontName);
    }
    double Size(bool &bDefault)
    {
        return GetParamDbl(OGRSTLabelSize, bDefault);
    }
    void SetSize(double dfSize)
    {
        SetParamDbl(OGRSTLabelSize, dfSize);
    }
    const char *TextString(bool &bDefault)
    {
        return GetParamStr(OGRSTLabelTextString, bDefault);
    }
    void SetTextString(const char *pszTextString)
    {
        SetParamStr(OGRSTLabelTextString, pszTextString);
    }
    double Angle(bool &bDefault)
    {
        return GetParamDbl(OGRSTLabelAngle, bDefault);
    }
    void SetAngle(double dfAngle)
    {
        SetParamDbl(OGRSTLabelAngle, dfAngle);
    }
    const char *ForeColor(bool &bDefault)
    {
        return GetParamStr(OGRSTLabelFColor, bDefault);
    }
    void SetForColor(const char *pszForColor)
    {
        SetParamStr(OGRSTLabelFColor, pszForColor);
    }
    const char *BackColor(bool &bDefault)
    {
        return GetParamStr(OGRSTLabelBColor, bDefault);
    }
    void SetBackColor(const char *pszBackColor)
    {
        SetParamStr(OGRSTLabelBColor, pszBackColor);
    }
    const char *Placement(bool &bDefault)
    {
        return GetParamStr(OGRSTLabelPlacement, bDefault);
    }
    void SetPlacement(const char *pszPlacement)
    {
        SetParamStr(OGRSTLabelPlacement, pszPlacement);
    }
    int Anchor(bool &bDefault)
    {
        return GetParamNum(OGRSTLabelAnchor, bDefault);
    }
    void SetAnchor(int nAnchor)
    {
        SetParamNum(OGRSTLabelAnchor, nAnchor);
    }
    double SpacingX(bool &bDefault)
    {
        return GetParamDbl(OGRSTLabelDx, bDefault);
    }
    void SetSpacingX(double dfX)
    {
        SetParamDbl(OGRSTLabelDx, dfX);
    }
    double SpacingY(bool &bDefault)
    {
        return GetParamDbl(OGRSTLabelDy, bDefault);
    }
    void SetSpacingY(double dfY)
    {
        SetParamDbl(OGRSTLabelDy, dfY);
    }
    double Perp(bool &bDefault)
    {
        return GetParamDbl(OGRSTLabelPerp, bDefault);
    }
    void SetPerp(double dfPerp)
    {
        SetParamDbl(OGRSTLabelPerp, dfPerp);
    }
    bool Bold(bool &bDefault)
    {
        return GetParamNum(OGRSTLabelBold, bDefault);
    }
    void SetBold(bool bBold)
    {
        SetParamNum(OGRSTLabelBold, bBold);
    }
    bool Italic(bool &bDefault)
    {
        return GetParamNum(OGRSTLabelItalic, bDefault);
    }
    void SetItalic(bool bItalic)
    {
        SetParamNum(OGRSTLabelItalic, bItalic);
    }
    bool Underline(bool &bDefault)
    {
        return GetParamNum(OGRSTLabelUnderline, bDefault);
    }
    void SetUnderline(bool bUnderline)
    {
        SetParamNum(OGRSTLabelUnderline, bUnderline);
    }
    int Priority(bool &bDefault)
    {
        return GetParamNum(OGRSTLabelPriority, bDefault);
    }
    void SetPriority(int nPriority)
    {
        SetParamNum(OGRSTLabelPriority, nPriority);
    }
    bool Strikeout(bool &bDefault)
    {
        return GetParamNum(OGRSTLabelStrikeout, bDefault);
    }
    void SetStrikeout(bool bStrikeout)
    {
        SetParamNum(OGRSTLabelStrikeout, bStrikeout);
    }
    double Stretch(bool &bDefault)
    {
        return GetParamDbl(OGRSTLabelStretch, bDefault);
    }
    void SetStretch(double dfStretch)
    {
        SetParamDbl(OGRSTLabelStretch, dfStretch);
    }
    const char *ShadowColor(bool &bDefault)
    {
        return GetParamStr(OGRSTLabelHColor, bDefault);
    }
    void SetShadowColor(const char *pszShadowColor)
    {
        SetParamStr(OGRSTLabelHColor, pszShadowColor);
    }
    const char *OutlineColor(bool &bDefault)
    {
        return GetParamStr(OGRSTLabelOColor, bDefault);
    }
    void SetOutlineColor(const char *pszOutlineColor)
    {
        SetParamStr(OGRSTLabelOColor, pszOutlineColor);
    }

    /*****************************************************************/

    const char *GetParamStr(OGRSTLabelParam eParam, bool &bValueIsNull);
    int GetParamNum(OGRSTLabelParam eParam, bool &bValueIsNull);
    double GetParamDbl(OGRSTLabelParam eParam, bool &bValueIsNull);
    void SetParamStr(OGRSTLabelParam eParam, const char *pszParamString);
    void SetParamNum(OGRSTLabelParam eParam, int nParam);
    void SetParamDbl(OGRSTLabelParam eParam, double dfParam);
    const char *GetStyleString() override;
};

//! @endcond

#endif /* OGR_FEATURESTYLE_INCLUDE */
