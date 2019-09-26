/*****************************************************************************
* Copyright 2016 Pitney Bowes Inc.
*
* Licensed under the MIT License (the “License”); you may not use this file
* except in the compliance with the License.
* You may obtain a copy of the License at https://opensource.org/licenses/MIT

* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an “AS IS” WITHOUT
* WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*****************************************************************************/

#include "cpl_port.h"
#include "OGREFAL.h"
#include "ogrgeopackageutility.h"

#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif
#include <algorithm>
#include <string>
#include <vector>

#include "cpl_conv.h"
#include "cpl_csv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"
#include "ogrsf_frmts.h"
#include "from_mitab.h"

enum class SymbolType : unsigned char
{
    NONE = 0,
    VECTOR = 1,
    FONT = 2,
    EFF = 3,
};

typedef enum TABFontStyle_t
{
    TABFSNone = 0,
    TABFSBold = 0x0001,
    TABFSItalic = 0x0002,
    TABFSUnderline = 0x0004,
    TABFSStrikeout = 0x0008,
    TABFSShadow = 0x0020,
    TABFSHalo = 0x0100,
    TABFSAllCaps = 0x0200,
    TABFSExpanded = 0x0400,
} TABFontStyle;

static bool QueryFontStyle(int style, TABFontStyle eStyleToQuery);
static char *mystrtok(char *str, const char *sep, char** context);

static bool QueryFontStyle(int style, TABFontStyle eStyleToQuery)
{
    return (style & static_cast<int>(eStyleToQuery)) ? true : false;
}

static char *mystrtok(char *str, const char *sep, char** context) {
    char * s = nullptr;
    if (str == nullptr) {
        s = *context;
    }
    else {
        s = str;
    }
    while (*s && strchr(sep, *s) != nullptr) s++;
    if (!*s) return nullptr;
    char *t = s;
    if (*s == '"')
    {
        s++;
        t = s;
        while (*s && *s != '"') s++;
        *s = 0;
        s++;
    }
    else
    {
        while (*s && strchr(sep, *s) == nullptr) s++;
        if (*s)
        {
            *s = 0;
            s++;
        }
    }
    *context = s;
    return t;
}

class EFALFeaturePen : public EFAL_GDAL_DRIVER::ITABFeaturePen
{
public:
    EFALFeaturePen() : ITABFeaturePen() {}
    ~EFALFeaturePen() {}

    const char *GetMapBasicStyleClause();
};
class EFALFeatureBrush : public EFAL_GDAL_DRIVER::ITABFeatureBrush
{
public:
    EFALFeatureBrush() : ITABFeatureBrush() {}
    ~EFALFeatureBrush() {}

    const char *GetMapBasicStyleClause();
};


class EFALFeatureSymbol : public EFAL_GDAL_DRIVER::ITABFeatureSymbol, public EFAL_GDAL_DRIVER::ITABFeatureFont
{
protected:
    double      m_dAngle;
    SymbolType  m_eSymbolType;
    GInt16      m_nFontStyle;           // Bold/shadow/halo/etc.

public:
    EFALFeatureSymbol();
    ~EFALFeatureSymbol() {}

    const char *GetSymbolStyleString();
    void        SetSymbolFromStyleString(const char *pszStyleString);

    const char *GetMapBasicStyleClause();

    int         GetFontStyleTABValue() { return m_nFontStyle; }
    void        SetFontStyleTABValue(int nStyle) { m_nFontStyle = (GInt16)nStyle; }

    SymbolType GetSymbolType() { return m_eSymbolType; }
    void SetSymbolType(SymbolType type) { m_eSymbolType = type; }

    // GetSymbolAngle(): Return angle in degrees counterclockwise
    double      GetSymbolAngle() { return m_dAngle; }
    void        SetSymbolAngle(double dAngle);


};

class EFALFeatureFont : public EFAL_GDAL_DRIVER::ITABFeatureFont
{
protected:
    short m_nFontStyle; // Bold/italic/underlined/shadow/
    short m_nPointSize;
    long m_rgbForeground;
    long m_rgbBackground;

public:
    EFALFeatureFont() : ITABFeatureFont(), m_nFontStyle(0), m_nPointSize(0), m_rgbForeground(0), m_rgbBackground(0)
    {}
    ~EFALFeatureFont() {}

    const char *GetFontStyleString();
    void        SetFontFromStyleString(const char *pszStyleString);

    const char *GetMapBasicStyleClause();

    short GetFontStyle() {
        return m_nFontStyle;
    }
    short GetPointSize() {
        return m_nPointSize;
    }
    long GetForeground() {
        return m_rgbForeground;
    }
    long GetBackground() {
        return m_rgbBackground;
    }

    void SetFontStyle(short style) {
        m_nFontStyle = style;
    }
    void SetPointSize(short size) {
        m_nPointSize = size;
    }
    void SetForeground(long color) {
        m_rgbForeground = color;
    }
    void SetBackground(long color) {
        m_rgbBackground = color;
    }
};

const char *EFALFeatureFont::GetFontStyleString()
{
    const char *pszBGColor = m_rgbBackground > 0 ? CPLSPrintf(",b:#%6.6x", (GInt32)m_rgbBackground) : "";
    const char *pszOColor = QueryFontStyle(m_nFontStyle, TABFontStyle::TABFSHalo) ? CPLSPrintf(",o:#%6.6x",
        (GInt32)m_rgbBackground) : "";
    const char *pszSColor = QueryFontStyle(m_nFontStyle, TABFontStyle::TABFSShadow) ? CPLSPrintf(",h:#%6.6x",
        0x808080) : "";
    const char *pszBold = QueryFontStyle(m_nFontStyle, TABFontStyle::TABFSBold) ? ",bo:1" : "";
    const char *pszItalic = QueryFontStyle(m_nFontStyle, TABFontStyle::TABFSItalic) ? ",it:1" : "";
    const char *pszUnderline = QueryFontStyle(m_nFontStyle, TABFontStyle::TABFSUnderline) ? ",un:1" : "";
    const char *pszStrikethrough = QueryFontStyle(m_nFontStyle, TABFontStyle::TABFSStrikeout) ? ",st:1" : "";
    const char *pszStyle = CPLSPrintf("LABEL(f:\"%s\",s:%dpt,c:#%6.6x%s%s%s%s%s%s%s)", GetFontNameRef(),
        GetPointSize(), (GInt32)GetForeground(), pszBGColor, pszOColor, pszSColor,
        pszBold, pszItalic, pszUnderline, pszStrikethrough);
    return pszStyle;
}

void EFALFeatureFont::SetFontFromStyleString(const char * pszStyleString)
{
    GBool bIsNull = 0;

    // Use the Style Manager to retrieve all the information we need.
    OGRStyleMgr *poStyleMgr = new OGRStyleMgr(nullptr);
    OGRStyleTool *poStylePart = nullptr;

    // Init the StyleMgr with the StyleString.
    poStyleMgr->InitStyleString(pszStyleString);

    // Retrieve the Pen info.
    const int numParts = poStyleMgr->GetPartCount();
    for (int i = 0; i < numParts; i++)
    {
        poStylePart = poStyleMgr->GetPart(i);
        if (poStylePart == nullptr)
            continue;

        if (poStylePart->GetType() == OGRSTCLabel)
        {
            break;
        }
        else
        {
            delete poStylePart;
            poStylePart = nullptr;
        }
    }

    // If the no label found, do nothing.
    if (poStylePart == nullptr)
    {
        delete poStyleMgr;
        return;
    }

    OGRStyleLabel *poLabelStyle = (OGRStyleLabel*)poStylePart;

    // With Symbol, we always want to output points
    //
    // It's very important to set the output unit of the feature.
    // The default value is meter. If we don't do it all numerical values
    // will be assumed to be converted from the input unit to meter when we
    // will get them via GetParam...() functions.
    // See OGRStyleTool::Parse() for more details.
    poLabelStyle->SetUnit(OGRSTUPoints, (72.0 * 39.37));

    const char* fontName = poLabelStyle->FontName(bIsNull);
    if (bIsNull || fontName == nullptr) fontName = nullptr;
    else SetFontName(fontName);

    SetPointSize((short)poLabelStyle->Size(bIsNull));

    const char *pszFgColor = poLabelStyle->ForeColor(bIsNull);
    if (pszFgColor)
    {
        if (pszFgColor[0] == '#')
            pszFgColor++;
        long nFgColor = strtol(pszFgColor, nullptr, 16);
        SetForeground(nFgColor);
    }

    const char *pszBgColor = poLabelStyle->BackColor(bIsNull);
    if (pszBgColor)
    {
        if (pszBgColor[0] == '#')
            pszBgColor++;
        long nBgColor = strtol(pszBgColor, nullptr, 16);
        SetForeground(nBgColor);
    }

    delete poStyleMgr;
    delete poStylePart;

    return;
}

const char * EFALFeatureFont::GetMapBasicStyleClause()
{
    const char *pszBg = m_rgbBackground > 0 ? CPLSPrintf(",%d", (int)GetBackground()) : "";
    const char *pszStyle = nullptr;
    pszStyle = CPLSPrintf("Font(\"%s\",%d,%d,%d%s)", GetFontNameRef(), (int)GetFontStyle(), (int)GetPointSize(), (int)GetForeground(), pszBg);
    return pszStyle;
}

/*=====================================================================
*                      class EFALFeatureSymbolFont
*====================================================================*/
EFALFeatureSymbol::EFALFeatureSymbol() : ITABFeatureFont(), m_dAngle(0), m_eSymbolType(SymbolType::NONE), m_nFontStyle(0)
{}
/**********************************************************************
*                   EFALFeatureSymbolFont::SetSymbolAngle()
*
* Set the symbol angle value in degrees, making sure the value is
* always in the range [0..360]
**********************************************************************/
void EFALFeatureSymbol::SetSymbolAngle(double dAngle)
{
    while (dAngle < 0.0)
        dAngle += 360.0;
    while (dAngle > 360.0)
        dAngle -= 360.0;

    m_dAngle = dAngle;
}
/**********************************************************************
*                EFALFeatureSymbolFont::GetSymbolStyleString()
*
*  Return a Symbol() string. All representations info for the Symbol are here.
**********************************************************************/
const char *EFALFeatureSymbol::GetSymbolStyleString()
{
    const char *pszStyle = nullptr;
    int    nOGRStyle = 1;
    /* char szPattern[20]; */
    int nAngle = 0;
    /* szPattern[0] = '\0'; */

    if (m_sSymbolDef.nSymbolNo == 31)
        nOGRStyle = 0;
    else if (m_sSymbolDef.nSymbolNo == 32)
        nOGRStyle = 6;
    else if (m_sSymbolDef.nSymbolNo == 33)
    {
        nAngle = 45;
        nOGRStyle = 6;
    }
    else if (m_sSymbolDef.nSymbolNo == 34)
        nOGRStyle = 4;
    else if (m_sSymbolDef.nSymbolNo == 35)
        nOGRStyle = 10;
    else if (m_sSymbolDef.nSymbolNo == 36)
        nOGRStyle = 8;
    else if (m_sSymbolDef.nSymbolNo == 37)
    {
        nAngle = 180;
        nOGRStyle = 8;
    }
    else if (m_sSymbolDef.nSymbolNo == 38)
        nOGRStyle = 5;
    else if (m_sSymbolDef.nSymbolNo == 39)
    {
        nAngle = 45;
        nOGRStyle = 5;
    }
    else if (m_sSymbolDef.nSymbolNo == 40)
        nOGRStyle = 3;
    else if (m_sSymbolDef.nSymbolNo == 41)
        nOGRStyle = 9;
    else if (m_sSymbolDef.nSymbolNo == 42)
        nOGRStyle = 7;
    else if (m_sSymbolDef.nSymbolNo == 43)
    {
        nAngle = 180;
        nOGRStyle = 7;
    }
    else if (m_sSymbolDef.nSymbolNo == 44)
        nOGRStyle = 6;
    else if (m_sSymbolDef.nSymbolNo == 45)
        nOGRStyle = 8;
    else if (m_sSymbolDef.nSymbolNo == 46)
        nOGRStyle = 4;
    else if (m_sSymbolDef.nSymbolNo == 49)
        nOGRStyle = 1;
    else if (m_sSymbolDef.nSymbolNo == 50)
        nOGRStyle = 2;

    nAngle += (int)m_dAngle;


    if (m_eSymbolType == SymbolType::VECTOR) {
        pszStyle = CPLSPrintf("SYMBOL(a:%d,c:#%6.6x,s:%dpt,id:\"mapinfo-sym-%d,ogr-sym-%d\")",
            nAngle,
            m_sSymbolDef.rgbColor,
            m_sSymbolDef.nPointSize,
            m_sSymbolDef.nSymbolNo,
            nOGRStyle);
    }
    else if (m_eSymbolType == SymbolType::FONT)
    {
        const char *outlineColor = nullptr;
        if (m_nFontStyle & 16)
            outlineColor = ",o:#000000";
        else if (m_nFontStyle & 256)
            outlineColor = ",o:#ffffff";
        else if (m_nFontStyle & 32)
            outlineColor = ",o:#808080";
        else
            outlineColor = "";

        pszStyle = CPLSPrintf("SYMBOL(a:%d,c:#%6.6x,s:%dpt,id:\"font-sym-%d,ogr-sym-9\"%s,f:\"%s\")",
            nAngle,
            m_sSymbolDef.rgbColor,
            m_sSymbolDef.nPointSize,
            m_sSymbolDef.nSymbolNo,
            outlineColor,
            GetFontNameRef());
    }
    else if (m_eSymbolType == SymbolType::EFF)
    {
        const char *outlineColor = nullptr;
        if (m_nFontStyle & 1)
            outlineColor = ",o:#ffffff";
        else if (m_nFontStyle & 4)
            outlineColor = ",o:#000000";
        else
            outlineColor = "";

        pszStyle = CPLSPrintf("SYMBOL(a:%d,c:#%6.6x,s:%dpt%s,id:\"bmp-%s,ogr-sym-0\")",
            nAngle,
            m_sSymbolDef.rgbColor,
            m_sSymbolDef.nPointSize,
            outlineColor,
            GetFontNameRef());
    }

    return pszStyle;
}

void EFALFeatureSymbol::SetSymbolFromStyleString(const char * pszStyleString)
{
    GBool bIsNull = 0;

    // Use the Style Manager to retrieve all the information we need.
    OGRStyleMgr *poStyleMgr = new OGRStyleMgr(nullptr);
    OGRStyleTool *poStylePart = nullptr;

    // Init the StyleMgr with the StyleString.
    poStyleMgr->InitStyleString(pszStyleString);

    // Retrieve the Symbol info.
    const int numParts = poStyleMgr->GetPartCount();
    for (int i = 0; i < numParts; i++)
    {
        poStylePart = poStyleMgr->GetPart(i);
        if (poStylePart == nullptr)
            continue;

        if (poStylePart->GetType() == OGRSTCSymbol)
        {
            break;
        }
        else
        {
            delete poStylePart;
            poStylePart = nullptr;
        }
    }

    // If the no Symbol found, do nothing.
    if (poStylePart == nullptr)
    {
        delete poStyleMgr;
        return;
    }

    OGRStyleSymbol *poSymbolStyle = (OGRStyleSymbol*)poStylePart;

    // With Symbol, we always want to output points
    //
    // It's very important to set the output unit of the feature.
    // The default value is meter. If we don't do it all numerical values
    // will be assumed to be converted from the input unit to meter when we
    // will get them via GetParam...() functions.
    // See OGRStyleTool::Parse() for more details.
    poSymbolStyle->SetUnit(OGRSTUPoints, (72.0 * 39.37));

    // Set the Symbol Id (SymbolNo)
    const char *pszSymbolId = poSymbolStyle->Id(bIsNull);
    if (bIsNull) pszSymbolId = nullptr;


    if (pszSymbolId && strlen(pszSymbolId) > 0)
    {
        const char* ptr = nullptr;
        if ((ptr = strstr(pszSymbolId, "mapinfo-sym-")) != nullptr)
        {
            // cppcheck-suppress constArgument
            const int nSymbolId = atoi(ptr + 12);
            SetSymbolNo((GByte)nSymbolId);
            SetSymbolType(SymbolType::VECTOR);
        }
        else if ((ptr = strstr(pszSymbolId, "font-sym-")) != nullptr)
        {
            // cppcheck-suppress constArgument
            const int nSymbolId = atoi(ptr + 9);
            SetSymbolNo((GByte)nSymbolId);
            const char* fontName = poSymbolStyle->FontName(bIsNull);
            if (!bIsNull && fontName != nullptr)
            {
                SetFontName(fontName);
            }
            SetSymbolType(SymbolType::FONT);
        }
        else if ((ptr = strstr(pszSymbolId, "bmp-")) != nullptr)
        {
            auto ptr2 = strstr(ptr, ",");
            const char* fontName = nullptr;
            if (ptr2 == nullptr)
            {
                fontName = ptr + 4;
                // cppcheck-suppress knownConditionTrueFalse
                if (fontName != nullptr && strlen(fontName) > 0)
                {
                    SetFontName(fontName);
                    SetSymbolType(SymbolType::EFF);
                }
            }
            else
            {
                size_t diff = ptr2 - ptr;
                memset(m_sFontDef.szFontName, 0, sizeof(m_sFontDef.szFontName));
#ifdef min
#undef min
#endif
                strncpy(m_sFontDef.szFontName, ptr + 4, std::min(sizeof(m_sFontDef.szFontName), diff - 4));

                SetSymbolType(SymbolType::EFF);
            }
        }
        else if (strstr(pszSymbolId, "ogr-sym-") != nullptr)
        {
            const int nSymbolId = atoi(pszSymbolId + 8);
            SetSymbolType(SymbolType::VECTOR);
            // The OGR symbol is not the MapInfo one
            // Here's some mapping
            switch (nSymbolId)
            {
            case 0:
                SetSymbolNo(31);
                break;
            case 1:
                SetSymbolNo(49);
                break;
            case 2:
                SetSymbolNo(50);
                break;
            case 3:
                SetSymbolNo(40);
                break;
            case 4:
                SetSymbolNo(34);
                break;
            case 5:
                SetSymbolNo(38);
                break;
            case 6:
                SetSymbolNo(32);
                break;
            case 7:
                SetSymbolNo(42);
                break;
            case 8:
                SetSymbolNo(36);
                break;
            case 9:
                SetSymbolNo(41);
                break;
            case 10:
                SetSymbolNo(35);
                break;
            default:
                SetSymbolType(SymbolType::NONE);
                break;
            }
        }
    }

    // Set SymbolSize
    const double dSymbolSize = poSymbolStyle->Size(bIsNull);
    if (dSymbolSize != 0.0)
    {
        SetSymbolSize((GInt16)dSymbolSize);
    }

    // Set Symbol Color
    const char *pszSymbolColor = poSymbolStyle->Color(bIsNull);
    if (pszSymbolColor)
    {
        if (pszSymbolColor[0] == '#')
            pszSymbolColor++;
        int nSymbolColor = static_cast<int>(strtol(pszSymbolColor, nullptr, 16));
        SetSymbolColor((GInt32)nSymbolColor);
    }

    // Set Symbol style
    const char* pszSymbolOColor = poSymbolStyle->OColor(bIsNull);
    if (pszSymbolOColor)
    {
        if (pszSymbolOColor[0] == '#')
            pszSymbolOColor++;
        int nSymbolColor = static_cast<int>(strtol(pszSymbolOColor, nullptr, 16));

        if (GetSymbolType() == SymbolType::FONT)
        {
            if (nSymbolColor == 16777215)
            {
                m_nFontStyle |= 16;
            }
            else if (nSymbolColor == 0)
            {
                m_nFontStyle |= 256;
            }
            else if (nSymbolColor == 8421504)
            {
                m_nFontStyle |= 32;
            }
        }
        else if (GetSymbolType() == SymbolType::EFF)
        {
            if (nSymbolColor == 16777215)
            {
                m_nFontStyle |= 1;
            }
            else if (nSymbolColor == 0)
            {
                m_nFontStyle |= 4;
            }
        }
    }

    const double dAngle = poSymbolStyle->Angle(bIsNull);
    SetSymbolAngle(dAngle);

    delete poStyleMgr;
    delete poStylePart;

    return;
}
const char * EFALFeatureSymbol::GetMapBasicStyleClause()
{
    const char *pszStyle = nullptr;
    if (GetSymbolType() == SymbolType::VECTOR)
    {
        pszStyle = CPLSPrintf("Symbol(%d,%d,%d)", (int)m_sSymbolDef.nSymbolNo, (int)m_sSymbolDef.rgbColor, (int)m_sSymbolDef.nPointSize);
    }
    else if (GetSymbolType() == SymbolType::FONT)
    {
        pszStyle = CPLSPrintf("Symbol(%d,%d,%d,\"%s\",%d,%d)", (int)m_sSymbolDef.nSymbolNo, (int)m_sSymbolDef.rgbColor, (int)m_sSymbolDef.nPointSize, GetFontNameRef(),
            GetFontStyleTABValue(), (int)GetSymbolAngle());
    }
    else if (GetSymbolType() == SymbolType::EFF)
    {
        pszStyle = CPLSPrintf("Symbol(\"%s\",%d,%d,%d)", GetFontNameRef(), (int)m_sSymbolDef.rgbColor, (int)m_sSymbolDef.nPointSize,
            GetFontStyleTABValue());
    }
    return pszStyle;
}
/**********************************************************************
*                 EFALFeaturePen::GetMapBasicStyleClause()
*
*  Return MapBasic clause for this style
**********************************************************************/
const char *EFALFeaturePen::GetMapBasicStyleClause()
{
    const char *pszStyle = nullptr;
    pszStyle = CPLSPrintf("Pen(%d,%d,%d)", (int)m_sPenDef.nPixelWidth, (int)m_sPenDef.nLinePattern, (int)m_sPenDef.rgbColor);
    return pszStyle;
}
/**********************************************************************
*                 EFALFeatureBrush::GetMapBasicStyleClause()
*
*  Return MapBasic clause for this style
**********************************************************************/
const char *EFALFeatureBrush::GetMapBasicStyleClause()
{
    const char *pszStyle = nullptr;
    pszStyle = CPLSPrintf("Brush(%d,%d,%d)", (int)m_sBrushDef.nFillPattern, (int)m_sBrushDef.rgbFGColor, (int)m_sBrushDef.rgbBGColor);
    return pszStyle;
}

/************************************************************************/
/*                      MapBasicStyle2OGRStyle()                        */
/************************************************************************/
CPLString OGREFALLayer::MapBasicStyle2OGRStyle(const wchar_t * mbStyle)
{
    if (mbStyle)
    {
        CPLString ogrStyle = "";

        char * mbStyleTokenString = nullptr;
        mbStyleTokenString = CPLRecodeFromWChar(mbStyle, CPL_ENC_UCS2, CPL_ENC_UTF8);

        char seps[] = " ,()";
        char *token = nullptr;
        char *next_token = nullptr;

        token = mystrtok(mbStyleTokenString, seps, &next_token);
        /*
        Point Styles
        Symbol(shape, color, size) => MapInfo 3.0-style symbols => Symbol(35,0,12)
        Symbol(shape,color,size,font,fontstyle,rotation) => TrueType font symbols => Symbol(64,255,12,"MapInfo Weather",17,0)
        Symbol(bitmapname,color,size,custom style) => bitmap symbols => Symbol("sign.bmp", 255, 18, 0)

        Line Styles
        Pen(thickness, pattern, color) => Pen(1, 2, 0)

        Region Styles
        Pen(thickness, pattern, color) => Pen(1, 2, 0)
        Brush(pattern,color,backgroundcolor) => Brush(2, 255, 65535)
        */

        bool hasSymbol = false;
        bool hasPen = false;
        bool hasBrush = false;
        bool hasFont = false;
        EFAL_GDAL_DRIVER::ITABFeaturePen pen;
        EFAL_GDAL_DRIVER::ITABFeatureBrush brush;
        EFALFeatureFont font;
        EFALFeatureSymbol symbol;

        while (token != nullptr)
        {
            if (stricmp(token, "Pen") == 0)
            {
                token = mystrtok(nullptr, seps, &next_token);

                if (token == nullptr)
                {
                    break;
                }

                double thickness = atof(token);

                token = mystrtok(nullptr, seps, &next_token);

                if (token == nullptr)
                {
                    break;
                }

                int pattern = atoi(token);
                if (pattern < 0) {
                    pattern = 0;
                }
                else if (pattern > UCHAR_MAX) {
                    pattern = UCHAR_MAX;
                }


                /* if width is 0 on non hollow pen, convert to single pixel solid line */
                if (((thickness == 0) || (thickness == 10)) && (pattern != 1)) {
                    thickness = 1;
                    pattern = 2;
                }

                token = mystrtok(nullptr, seps, &next_token);

                if (token == nullptr)
                {
                    break;
                }

                long color = atol(token);

                if (thickness > 10)
                {
                    pen.SetPenWidthPoint((double)thickness);
                }
                else
                {
                    pen.SetPenWidthPixel((GByte)thickness);
                }
                pen.SetPenPattern((GByte)pattern);
                pen.SetPenColor((GInt32)color);
                hasPen = true;

                token = mystrtok(nullptr, seps, &next_token);
                continue;
            }
            else if (stricmp(token, "Brush") == 0)
            {
                token = mystrtok(nullptr, seps, &next_token);

                if (token == nullptr)
                {
                    break;
                }

                int pattern = atoi(token);
                if (pattern < 0) {
                    pattern = 0;
                }
                else if (pattern > UCHAR_MAX) {
                    pattern = UCHAR_MAX;
                }

                token = mystrtok(nullptr, seps, &next_token);

                if (token == nullptr)
                {
                    break;
                }

                long forecolor = atol(token);

                token = mystrtok(nullptr, seps, &next_token);

                if (token == nullptr ||
                    stricmp(token, "Pen") == 0 ||
                    stricmp(token, "Font") == 0 ||
                    stricmp(token, "Symbol") == 0)
                {
                    brush.SetBrushPattern((GByte)pattern);
                    brush.SetBrushFGColor((GInt32)forecolor);
                    brush.SetBrushBGColor((GInt32)0);
                    brush.SetBrushTransparent((GByte)1);
                    hasBrush = true;
                    continue;
                }
                else
                {
                    long bgcolor = atol(token);
                    brush.SetBrushPattern((GByte)pattern);
                    brush.SetBrushFGColor((GInt32)forecolor);
                    brush.SetBrushBGColor((GInt32)bgcolor);
                    brush.SetBrushTransparent((GByte)0);
                    hasBrush = true;

                    token = mystrtok(nullptr, seps, &next_token);
                    continue;
                }
            }
            else if (stricmp(token, "Font") == 0)
            {
                token = mystrtok(nullptr, seps, &next_token);
                if (token == nullptr)
                {
                    break;
                }

                char* fontName = token;

                token = mystrtok(nullptr, seps, &next_token);
                if (token == nullptr)
                {
                    break;
                }

                int style = atoi(token);
                if (style < 0) {
                    style = 0;
                }
                else if (style > SHRT_MAX) {
                    style = SHRT_MAX;
                }

                token = mystrtok(nullptr, seps, &next_token);
                if (token == nullptr)
                {
                    break;
                }

                int size = atoi(token);
                if (size < 0) {
                    size = 0;
                }
                else if (size > SHRT_MAX) {
                    size = SHRT_MAX;
                }

                token = mystrtok(nullptr, seps, &next_token);

                if (token == nullptr)
                {
                    break;
                }

                long forecolor = atol(token);

                token = mystrtok(nullptr, seps, &next_token);

                if (token == nullptr ||
                    stricmp(token, "Pen") == 0 ||
                    stricmp(token, "Brush") == 0 ||
                    stricmp(token, "Symbol") == 0)
                {
                    font.SetFontName(fontName);
                    font.SetFontStyle((short)style);
                    font.SetPointSize((short)size);
                    font.SetForeground(forecolor);
                    font.SetBackground(0);
                    hasFont = true;
                    continue;
                }
                else
                {
                    long bgcolor = atol(token);

                    font.SetFontName(fontName);
                    font.SetFontStyle((short)style);
                    font.SetPointSize((short)size);
                    font.SetForeground(forecolor);
                    font.SetBackground(bgcolor);
                    hasFont = true;

                    token = mystrtok(nullptr, seps, &next_token);
                    continue;
                }
            }
            else if (stricmp(token, "Symbol") == 0)
            {
                token = mystrtok(nullptr, seps, &next_token);
                if (token == nullptr)
                {
                    break;
                }

                int code = atoi(token);
                if (code > 0)
                {
                    // non-custom symbol

                    token = mystrtok(nullptr, seps, &next_token);
                    if (token == nullptr)
                    {
                        break;
                    }

                    long color = atol(token);

                    token = mystrtok(nullptr, seps, &next_token);
                    if (token == nullptr)
                    {
                        break;
                    }

                    int size = atoi(token);
                    if (size < 0) {
                        size = 0;
                    }
                    else if (size > SHRT_MAX) {
                        size = SHRT_MAX;
                    }

                    token = mystrtok(nullptr, seps, &next_token);

                    if (token == nullptr ||
                        stricmp(token, "Pen") == 0 ||
                        stricmp(token, "Brush") == 0 ||
                        stricmp(token, "Font") == 0)
                    {
                        symbol.SetSymbolNo((GInt16)code);
                        symbol.SetSymbolColor((GInt32)color);
                        symbol.SetSymbolSize((GInt16)size);
                        symbol.SetSymbolType(SymbolType::VECTOR);
                        hasSymbol = true;
                        continue;
                    }
                    else
                    {
                        /**
                        * If there's no right paren, then we've got more parameters, and that
                        * means a font symbol is being specified.
                        **/
                        char* fontName = token;

                        token = mystrtok(nullptr, seps, &next_token);
                        if (token == nullptr)
                        {
                            break;
                        }

                        int style = atoi(token);
                        if (style < 0) {
                            style = 0;
                        }
                        else if (style > SHRT_MAX) {
                            style = SHRT_MAX;
                        }

                        token = mystrtok(nullptr, seps, &next_token);
                        if (token == nullptr)
                        {
                            break;
                        }
                        double angle = atof(token);

                        symbol.SetSymbolNo((GInt16)code);
                        symbol.SetSymbolColor((GInt32)color);
                        symbol.SetSymbolSize((GInt16)size);
                        symbol.SetFontName(fontName);
                        symbol.SetFontStyleTABValue(style);
                        symbol.SetSymbolAngle(angle);
                        symbol.SetSymbolType(SymbolType::FONT);
                        hasSymbol = true;

                        token = mystrtok(nullptr, seps, &next_token);
                        continue;
                    }
                }
                else
                {
                    // custom symbol

                    char* bitmapName = token;

                    token = mystrtok(nullptr, seps, &next_token);
                    if (token == nullptr)
                    {
                        break;
                    }

                    long color = atol(token);

                    token = mystrtok(nullptr, seps, &next_token);
                    if (token == nullptr)
                    {
                        break;
                    }

                    int size = atoi(token);
                    if (size < 0) {
                        size = 0;
                    }
                    else if (size > SHRT_MAX) {
                        size = SHRT_MAX;
                    }

                    token = mystrtok(nullptr, seps, &next_token);
                    if (token == nullptr)
                    {
                        break;
                    }

                    int style = atoi(token);
                    if (style < 0) {
                        style = 0;
                    }
                    else if (size > SHRT_MAX) {
                        style = SHRT_MAX;
                    }

                    symbol.SetFontName(bitmapName);
                    symbol.SetSymbolColor((GInt32)color);
                    symbol.SetSymbolSize((GInt16)size);
                    symbol.SetFontStyleTABValue(style);
                    symbol.SetSymbolType(SymbolType::EFF);
                    hasSymbol = true;

                    token = mystrtok(nullptr, seps, &next_token);
                    continue;
                }
            }
            else
            {
                break;
            }
        }
        CPLFree(mbStyleTokenString);

        if (hasFont)
        {
            if (ogrStyle.length() > 0) ogrStyle += ";";
            ogrStyle += font.GetFontStyleString();
        }
        if (hasPen)
        {
            if (ogrStyle.length() > 0) ogrStyle += ";";
            ogrStyle += pen.GetPenStyleString();
        }
        if (hasBrush)
        {
            if (ogrStyle.length() > 0) ogrStyle += ";";
            ogrStyle += brush.GetBrushStyleString();
        }
        if (hasSymbol)
        {
            if (ogrStyle.length() > 0) ogrStyle += ";";
            ogrStyle += symbol.GetSymbolStyleString();
        }
        return ogrStyle;
    }
    return nullptr;
}
/************************************************************************/
/*                      OGRStyle2MapBasicStyle()                        */
/************************************************************************/
char* OGREFALLayer::OGRStyle2MapBasicStyle(const char * ogrStyle) const
{
    CPLString mbStyle = "";

    const char * szPEN = "PEN";
    const char * szBRUSH = "BRUSH";
    const char * szSYMBOL = "SYMBOL";
    const char * szLabel = "LABEL";
    char * s = CPLStrdup(ogrStyle);
    char * t = s;
    while (*s)
    {
        if (strnicmp(s, szPEN, strlen(szPEN)) == 0)
        {
            char * p = s + strlen(szPEN);
            while (*p && (*p != ')')) p++;
            if (*p) p++;
            *p = 0;
            EFALFeaturePen pen;
            pen.SetPenFromStyleString(s);
            s = p + 1;
            if (mbStyle.length() > 0) mbStyle += " ";
            auto pszStyle = pen.GetMapBasicStyleClause();
            if (pszStyle != nullptr) {
                // cppcheck-suppress nullPointerArithmeticRedundantCheck
                mbStyle += pszStyle;
            }
        }
        else if (strnicmp(s, szBRUSH, strlen(szBRUSH)) == 0)
        {
            char * p = s + strlen(szBRUSH);
            while (*p && (*p != ')')) p++;
            if (*p) p++;
            *p = 0;
            EFALFeatureBrush brush;
            brush.SetBrushFromStyleString(s);
            s = p + 1;
            if (mbStyle.length() > 0) mbStyle += " ";
            auto pszStyle = brush.GetMapBasicStyleClause();
            if (pszStyle != nullptr) {
                // cppcheck-suppress nullPointerArithmeticRedundantCheck
                mbStyle += pszStyle;
            }
        }
        else if (strnicmp(s, szSYMBOL, strlen(szSYMBOL)) == 0)
        {
            char * p = s + strlen(szSYMBOL);
            while (*p && (*p != ')')) p++;
            if (*p) p++;
            *p = 0;
            EFALFeatureSymbol symbol;
            symbol.SetSymbolFromStyleString(s);
            s = p + 1;
            if (mbStyle.length() > 0) mbStyle += " ";
            auto pszStyle = symbol.GetMapBasicStyleClause();
            if (pszStyle != nullptr) {
                // cppcheck-suppress nullPointerArithmeticRedundantCheck
                mbStyle += pszStyle;
            }
        }
        else if (strnicmp(s, szLabel, strlen(szLabel)) == 0)
        {
            char * p = s + strlen(szLabel);
            while (*p && (*p != ')')) p++;
            if (*p) p++;
            *p = 0;
            EFALFeatureFont font;
            font.SetFontFromStyleString(s);
            s = p + 1;
            if (mbStyle.length() > 0) mbStyle += " ";
            auto pszStyle = font.GetMapBasicStyleClause();
            if (pszStyle != nullptr) {
                // cppcheck-suppress nullPointerArithmeticRedundantCheck
                mbStyle += pszStyle;
            }
        }
        else
            s++;
    }
    CPLFree(t);
    return CPLStrdup(mbStyle);
}
