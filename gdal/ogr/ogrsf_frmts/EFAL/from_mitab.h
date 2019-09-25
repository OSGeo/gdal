/******************************************************************************
 *
 * Project:  CSV Translator
 * Purpose:  MapInfo Tab Styles Heaader.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Frank Warmerdam <warmerdam@pobox.com>
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

#ifndef OGR_FROM_MITAB_INCLUDED
#define OGR_FROM_MITAB_INCLUDED

#include "ogrsf_frmts.h"

namespace EFAL_GDAL_DRIVER
{

    /*---------------------------------------------------------------------
    * TABPenDef - Pen definition information
    *--------------------------------------------------------------------*/
    typedef struct TABPenDef_t
    {
        GInt32      nRefCount;
        GByte       nPixelWidth;
        GByte       nLinePattern;
        int         nPointWidth;
        GInt32      rgbColor;
    } TABPenDef;

    /* MI Default = PEN(1,2,0) */
#define MITAB_PEN_DEFAULT {0, 1, 2, 0, 0x000000}

    /*---------------------------------------------------------------------
    * TABBrushDef - Brush definition information
    *--------------------------------------------------------------------*/
    typedef struct TABBrushDef_t
    {
        GInt32      nRefCount;
        GByte       nFillPattern;
        GByte       bTransparentFill; // 1 = Transparent
        GInt32      rgbFGColor;
        GInt32      rgbBGColor;
    } TABBrushDef;

    /* MI Default = BRUSH(1,0,16777215) */
#define MITAB_BRUSH_DEFAULT {0, 1, 0, 0, 0xffffff}

    /*---------------------------------------------------------------------
    * TABFontDef - Font Name information
    *--------------------------------------------------------------------*/
    typedef struct TABFontDef_t
    {
        GInt32      nRefCount;
        char        szFontName[256];
    } TABFontDef;

    /* MI Default = FONT("Arial",0,0,0) */
#define MITAB_FONT_DEFAULT {0, "Arial"}

    /*---------------------------------------------------------------------
    * TABSymbolDef - Symbol definition information
    *--------------------------------------------------------------------*/
    typedef struct TABSymbolDef_t
    {
        GInt32      nRefCount;
        GInt16      nSymbolNo;
        GInt16      nPointSize;
        GByte       _nUnknownValue_;// Style???
        GInt32      rgbColor;
    } TABSymbolDef;

    /* MI Default = SYMBOL(35,0,12) */
#define MITAB_SYMBOL_DEFAULT {0, 35, 12, 0, 0x000000}

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
        int         GetPenDefIndex() { return m_nPenDefIndex; }
        TABPenDef  *GetPenDefRef() { return &m_sPenDef; }

        GByte       GetPenWidthPixel();
        double      GetPenWidthPoint();
        int         GetPenWidthMIF();
        GByte       GetPenPattern() { return m_sPenDef.nLinePattern; }
        GInt32      GetPenColor() { return m_sPenDef.rgbColor; }

        void        SetPenWidthPixel(GByte val);
        void        SetPenWidthPoint(double val);
        void        SetPenWidthMIF(int val);

        void        SetPenPattern(GByte val) { m_sPenDef.nLinePattern = val; }
        void        SetPenColor(GInt32 clr) { m_sPenDef.rgbColor = clr; }

        const char *GetPenStyleString();
        void        SetPenFromStyleString(const char *pszStyleString);

        void        DumpPenDef(FILE *fpOut = nullptr);
    };

    class ITABFeatureBrush
    {
    protected:
        int         m_nBrushDefIndex;
        TABBrushDef m_sBrushDef;
    public:
        ITABFeatureBrush();
        ~ITABFeatureBrush() {}
        int         GetBrushDefIndex() { return m_nBrushDefIndex; }
        TABBrushDef *GetBrushDefRef() { return &m_sBrushDef; }

        GInt32      GetBrushFGColor() { return m_sBrushDef.rgbFGColor; }
        GInt32      GetBrushBGColor() { return m_sBrushDef.rgbBGColor; }
        GByte       GetBrushPattern() { return m_sBrushDef.nFillPattern; }
        GByte       GetBrushTransparent() { return m_sBrushDef.bTransparentFill; }

        void        SetBrushFGColor(GInt32 clr) { m_sBrushDef.rgbFGColor = clr; }
        void        SetBrushBGColor(GInt32 clr) { m_sBrushDef.rgbBGColor = clr; }
        void        SetBrushPattern(GByte val) { m_sBrushDef.nFillPattern = val; }
        void        SetBrushTransparent(GByte val)
        {
            m_sBrushDef.bTransparentFill = val;
        }

        const char *GetBrushStyleString();
        void        SetBrushFromStyleString(const char *pszStyleString);

        void        DumpBrushDef(FILE *fpOut = nullptr);
    };

    class ITABFeatureFont
    {
    protected:
        int         m_nFontDefIndex;
        TABFontDef  m_sFontDef;
    public:
        ITABFeatureFont();
        ~ITABFeatureFont() {}
        int         GetFontDefIndex() { return m_nFontDefIndex; }
        TABFontDef *GetFontDefRef() { return &m_sFontDef; }

        const char *GetFontNameRef() { return m_sFontDef.szFontName; }

        void        SetFontName(const char *pszName);

        void        DumpFontDef(FILE *fpOut = nullptr);
    };

    class ITABFeatureSymbol
    {
    protected:
        int         m_nSymbolDefIndex;
        TABSymbolDef m_sSymbolDef;
    public:
        ITABFeatureSymbol();
        ~ITABFeatureSymbol() {}
        int         GetSymbolDefIndex() { return m_nSymbolDefIndex; }
        TABSymbolDef *GetSymbolDefRef() { return &m_sSymbolDef; }

        GInt16      GetSymbolNo() { return m_sSymbolDef.nSymbolNo; }
        GInt16      GetSymbolSize() { return m_sSymbolDef.nPointSize; }
        GInt32      GetSymbolColor() { return m_sSymbolDef.rgbColor; }

        void        SetSymbolNo(GInt16 val) { m_sSymbolDef.nSymbolNo = val; }
        void        SetSymbolSize(GInt16 val) { m_sSymbolDef.nPointSize = val; }
        void        SetSymbolColor(GInt32 clr) { m_sSymbolDef.rgbColor = clr; }

        const char *GetSymbolStyleString(double dfAngle = 0.0);
        void        SetSymbolFromStyleString(const char *pszStyleString);

        void        DumpSymbolDef(FILE *fpOut = nullptr);
    };

    char *TABGetBasename(const char *pszFname);
    char *TABCleanFieldName(const char *pszSrcName);

    /*---------------------------------------------------------------------
    * Define some error codes specific to this lib.
    *--------------------------------------------------------------------*/
#define TAB_WarningFeatureTypeNotSupported     501
#define TAB_WarningInvalidFieldName            502
#define TAB_WarningBoundsOverflow              503

} // end of namespace
#endif
