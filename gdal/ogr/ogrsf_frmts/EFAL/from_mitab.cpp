/******************************************************************************
 *
 * Project:  CSV Translator
 * Purpose:  Implements MapInfo Tab Styles.
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

#include "from_mitab.h"
#include <algorithm>

#if defined(_WIN32) && !defined(unix)
#include <mbctype.h>  // Multibyte chars stuff.
#endif

namespace EFAL_GDAL_DRIVER
{

    /*=====================================================================
    *                      class ITABFeaturePen
    *====================================================================*/

    /**********************************************************************
    *                   ITABFeaturePen::ITABFeaturePen()
    **********************************************************************/

    // MI default is PEN(1, 2, 0)
    static const TABPenDef csDefaultPen = MITAB_PEN_DEFAULT;

    ITABFeaturePen::ITABFeaturePen() :
        m_nPenDefIndex(-1),
        m_sPenDef(csDefaultPen)
    {}

    /**********************************************************************
    *                   ITABFeaturePen::GetPenWidthPixel()
    *                   ITABFeaturePen::SetPenWidthPixel()
    *                   ITABFeaturePen::GetPenWidthPoint()
    *                   ITABFeaturePen::SetPenWidthPoint()
    *
    * Pen width can be expressed in pixels (value from 1 to 7 pixels) or
    * in points (value from 0.1 to 203.7 points). The default pen width
    * in MapInfo is 1 pixel.  Pen width in points exist only in file version 450.
    *
    * The following methods hide the way the pen width is stored in the files.
    *
    * In order to establish if a given pen def had its width specified in
    * pixels or in points, one should first call GetPenWidthPoint(), and if
    * it returns 0 then the Pixel width should be used instead:
    *    if (GetPenWidthPoint() == 0)
    *       ... use pen width in points ...
    *    else
    *       ... use Pixel width from GetPenWidthPixel()
    *
    * Note that the reverse is not true: the default pixel width is always 1,
    * even when the pen width was actually set in points.
    **********************************************************************/

    GByte ITABFeaturePen::GetPenWidthPixel()
    {
        return m_sPenDef.nPixelWidth;
    }

    void  ITABFeaturePen::SetPenWidthPixel(GByte val)
    {
        const GByte nPixelWidthMin = 1;
        const GByte nPixelWidthMax = 7;
        m_sPenDef.nPixelWidth =
            std::min(std::max(val, nPixelWidthMin), nPixelWidthMax);
        m_sPenDef.nPointWidth = 0;
    }

    double ITABFeaturePen::GetPenWidthPoint()
    {
        // We store point width internally as tenths of points
        return m_sPenDef.nPointWidth / 10.0;
    }

    void  ITABFeaturePen::SetPenWidthPoint(double val)
    {
        m_sPenDef.nPointWidth =
            std::min(std::max(static_cast<int>(val * 10), 1), 2037);
        m_sPenDef.nPixelWidth = 1;
    }

    /**********************************************************************
    *                   ITABFeaturePen::GetPenWidthMIF()
    *                   ITABFeaturePen::SetPenWidthMIF()
    *
    * The MIF representation for pen width is either a value from 1 to 7
    * for a pen width in pixels, or a value from 11 to 2047 for a pen
    * width in points = 10 + (point_width*10)
    **********************************************************************/
    int     ITABFeaturePen::GetPenWidthMIF()
    {
        return (m_sPenDef.nPointWidth > 0 ?
            (m_sPenDef.nPointWidth + 10) : m_sPenDef.nPixelWidth);
    }

    void ITABFeaturePen::SetPenWidthMIF(int val)
    {
        if (val > 10)
        {
            m_sPenDef.nPointWidth = std::min((val - 10), 2037);
            m_sPenDef.nPixelWidth = 0;
        }
        else
        {
            m_sPenDef.nPixelWidth = (GByte)std::min(std::max(val, 1), 7);
            m_sPenDef.nPointWidth = 0;
        }
    }

    /**********************************************************************
    *                   ITABFeaturePen::GetPenStyleString()
    *
    *  Return a PEN() string. All representations info for the pen are here.
    **********************************************************************/
    const char *ITABFeaturePen::GetPenStyleString()
    {
        const char *pszStyle = nullptr;
        int    nOGRStyle = 0;
        char szPattern[20];

        szPattern[0] = '\0';

        // For now, I only add the 25 first styles
        switch (GetPenPattern())
        {
        case 1:
            nOGRStyle = 1;
            break;
        case 2:
            nOGRStyle = 0;
            break;
        case 3:
            nOGRStyle = 3;
            strcpy(szPattern, "1 1");
            break;
        case 4:
            nOGRStyle = 3;
            strcpy(szPattern, "2 1");
            break;
        case 5:
            nOGRStyle = 3;
            strcpy(szPattern, "3 1");
            break;
        case 6:
            nOGRStyle = 3;
            strcpy(szPattern, "6 1");
            break;
        case 7:
            nOGRStyle = 4;
            strcpy(szPattern, "12 2");
            break;
        case 8:
            nOGRStyle = 4;
            strcpy(szPattern, "24 4");
            break;
        case 9:
            nOGRStyle = 3;
            strcpy(szPattern, "4 3");
            break;
        case 10:
            nOGRStyle = 5;
            strcpy(szPattern, "1 4");
            break;
        case 11:
            nOGRStyle = 3;
            strcpy(szPattern, "4 6");
            break;
        case 12:
            nOGRStyle = 3;
            strcpy(szPattern, "6 4");
            break;
        case 13:
            nOGRStyle = 4;
            strcpy(szPattern, "12 12");
            break;
        case 14:
            nOGRStyle = 6;
            strcpy(szPattern, "8 2 1 2");
            break;
        case 15:
            nOGRStyle = 6;
            strcpy(szPattern, "12 1 1 1");
            break;
        case 16:
            nOGRStyle = 6;
            strcpy(szPattern, "12 1 3 1");
            break;
        case 17:
            nOGRStyle = 6;
            strcpy(szPattern, "24 6 4 6");
            break;
        case 18:
            nOGRStyle = 7;
            strcpy(szPattern, "24 3 3 3 3 3");
            break;
        case 19:
            nOGRStyle = 7;
            strcpy(szPattern, "24 3 3 3 3 3 3 3");
            break;
        case 20:
            nOGRStyle = 7;
            strcpy(szPattern, "6 3 1 3 1 3");
            break;
        case 21:
            nOGRStyle = 7;
            strcpy(szPattern, "12 2 1 2 1 2");
            break;
        case 22:
            nOGRStyle = 7;
            strcpy(szPattern, "12 2 1 2 1 2 1 2");
            break;
        case 23:
            nOGRStyle = 6;
            strcpy(szPattern, "4 1 1 1");
            break;
        case 24:
            nOGRStyle = 7;
            strcpy(szPattern, "4 1 1 1 1");
            break;
        case 25:
            nOGRStyle = 6;
            strcpy(szPattern, "4 1 1 1 2 1 1 1");
            break;

        default:
            nOGRStyle = 0;
            break;
        }

        if (strlen(szPattern) != 0)
        {
            if (m_sPenDef.nPointWidth > 0)
                pszStyle = CPLSPrintf("PEN(w:%dpt,c:#%6.6x,id:\"mapinfo-pen-%d,"
                    "ogr-pen-%d\",p:\"%spx\")",
                    ((int)GetPenWidthPoint()),
                    m_sPenDef.rgbColor, GetPenPattern(), nOGRStyle,
                    szPattern);
            else
                pszStyle = CPLSPrintf("PEN(w:%dpx,c:#%6.6x,id:\"mapinfo-pen-%d,"
                    "ogr-pen-%d\",p:\"%spx\")",
                    GetPenWidthPixel(),
                    m_sPenDef.rgbColor, GetPenPattern(), nOGRStyle,
                    szPattern);
        }
        else
        {
            if (m_sPenDef.nPointWidth > 0)
                pszStyle = CPLSPrintf("PEN(w:%dpt,c:#%6.6x,id:\""
                    "mapinfo-pen-%d,ogr-pen-%d\")",
                    ((int)GetPenWidthPoint()),
                    m_sPenDef.rgbColor, GetPenPattern(), nOGRStyle);
            else
                pszStyle = CPLSPrintf("PEN(w:%dpx,c:#%6.6x,id:\""
                    "mapinfo-pen-%d,ogr-pen-%d\")",
                    GetPenWidthPixel(),
                    m_sPenDef.rgbColor, GetPenPattern(), nOGRStyle);
        }

        return pszStyle;
    }

    /**********************************************************************
    *                   ITABFeaturePen::SetPenFromStyleString()
    *
    *  Init the Pen properties from a style string.
    **********************************************************************/
    void  ITABFeaturePen::SetPenFromStyleString(const char *pszStyleString)
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

            if (poStylePart->GetType() == OGRSTCPen)
            {
                break;
            }
            else
            {
                delete poStylePart;
                poStylePart = nullptr;
            }
        }

        // If the no Pen found, do nothing.
        if (poStylePart == nullptr)
        {
            delete poStyleMgr;
            return;
        }

        OGRStylePen *poPenStyle = (OGRStylePen*)poStylePart;

        // With Pen, we always want to output points or pixels (which are the same,
        // so just use points).
        //
        // It's very important to set the output unit of the feature.
        // The default value is meter. If we don't do it all numerical values
        // will be assumed to be converted from the input unit to meter when we
        // will get them via GetParam...() functions.
        // See OGRStyleTool::Parse() for more details.
        poPenStyle->SetUnit(OGRSTUPoints, 1);

        // Get the Pen Id or pattern
        const char *pszPenName = poPenStyle->Id(bIsNull);
        if (bIsNull) pszPenName = nullptr;

        // Set the width
        if (poPenStyle->Width(bIsNull) != 0.0)
        {
            const double nPenWidth = poPenStyle->Width(bIsNull);
            // Width < 10 is a pixel
            if (nPenWidth > 10)
                SetPenWidthPoint(nPenWidth);
            else
                SetPenWidthPixel((GByte)nPenWidth);
        }

        //Set the color
        const char *pszPenColor = poPenStyle->Color(bIsNull);
        if (pszPenColor != nullptr)
        {
            if (pszPenColor[0] == '#')
                pszPenColor++;
            // The Pen color is an Hexa string that need to be convert in a int
            const GInt32 nPenColor =
                static_cast<int>(strtol(pszPenColor, nullptr, 16));
            SetPenColor(nPenColor);
        }

        const char *pszPenPattern = nullptr;

        // Set the Id of the Pen, use Pattern if necessary.
        if (pszPenName &&
            (strstr(pszPenName, "mapinfo-pen-") || strstr(pszPenName, "ogr-pen-")))
        {
            int nPenId;
            const char* pszPenId = strstr(pszPenName, "mapinfo-pen-");
            if (pszPenId != nullptr)
            {
                nPenId = atoi(pszPenId + 12);
                SetPenPattern((GByte)nPenId);
            }
            else
            {
                pszPenId = strstr(pszPenName, "ogr-pen-");
                if (pszPenId != nullptr)
                {
                    nPenId = atoi(pszPenId + 8);
                    if (nPenId == 0)
                        nPenId = 2;
                    SetPenPattern((GByte)nPenId);
                }
            }
        }
        else
        {
            // If no Pen Id, use the Pen Pattern to retrieve the Id.
            pszPenPattern = poPenStyle->Pattern(bIsNull);
            if (bIsNull)
                pszPenPattern = nullptr;
            else
            {
                if (strcmp(pszPenPattern, "1 1") == 0)
                    SetPenPattern(3);
                else if (strcmp(pszPenPattern, "2 1") == 0)
                    SetPenPattern(4);
                else if (strcmp(pszPenPattern, "3 1") == 0)
                    SetPenPattern(5);
                else if (strcmp(pszPenPattern, "6 1") == 0)
                    SetPenPattern(6);
                else if (strcmp(pszPenPattern, "12 2") == 0)
                    SetPenPattern(7);
                else if (strcmp(pszPenPattern, "24 4") == 0)
                    SetPenPattern(8);
                else if (strcmp(pszPenPattern, "4 3") == 0)
                    SetPenPattern(9);
                else if (strcmp(pszPenPattern, "1 4") == 0)
                    SetPenPattern(10);
                else if (strcmp(pszPenPattern, "4 6") == 0)
                    SetPenPattern(11);
                else if (strcmp(pszPenPattern, "6 4") == 0)
                    SetPenPattern(12);
                else if (strcmp(pszPenPattern, "12 12") == 0)
                    SetPenPattern(13);
                else if (strcmp(pszPenPattern, "8 2 1 2") == 0)
                    SetPenPattern(14);
                else if (strcmp(pszPenPattern, "12 1 1 1") == 0)
                    SetPenPattern(15);
                else if (strcmp(pszPenPattern, "12 1 3 1") == 0)
                    SetPenPattern(16);
                else if (strcmp(pszPenPattern, "24 6 4 6") == 0)
                    SetPenPattern(17);
                else if (strcmp(pszPenPattern, "24 3 3 3 3 3") == 0)
                    SetPenPattern(18);
                else if (strcmp(pszPenPattern, "24 3 3 3 3 3 3 3") == 0)
                    SetPenPattern(19);
                else if (strcmp(pszPenPattern, "6 3 1 3 1 3") == 0)
                    SetPenPattern(20);
                else if (strcmp(pszPenPattern, "12 2 1 2 1 2") == 0)
                    SetPenPattern(21);
                else if (strcmp(pszPenPattern, "12 2 1 2 1 2 1 2") == 0)
                    SetPenPattern(22);
                else if (strcmp(pszPenPattern, "4 1 1 1") == 0)
                    SetPenPattern(23);
                else if (strcmp(pszPenPattern, "4 1 1 1 1") == 0)
                    SetPenPattern(24);
                else if (strcmp(pszPenPattern, "4 1 1 1 2 1 1 1") == 0)
                    SetPenPattern(25);
            }
        }

        delete poStyleMgr;
        delete poStylePart;

        return;
    }

    /**********************************************************************
    *                   ITABFeaturePen::DumpPenDef()
    *
    * Dump pen definition information.
    **********************************************************************/
    void ITABFeaturePen::DumpPenDef(FILE *fpOut /*=nullptr*/)
    {
        if (fpOut == nullptr)
            fpOut = stdout;

        fprintf(fpOut, "  m_nPenDefIndex         = %d\n", m_nPenDefIndex);
        fprintf(fpOut, "  m_sPenDef.nRefCount    = %d\n", m_sPenDef.nRefCount);
        fprintf(fpOut, "  m_sPenDef.nPixelWidth  = %u\n", m_sPenDef.nPixelWidth);
        fprintf(fpOut, "  m_sPenDef.nLinePattern = %u\n", m_sPenDef.nLinePattern);
        fprintf(fpOut, "  m_sPenDef.nPointWidth  = %d\n", m_sPenDef.nPointWidth);
        fprintf(fpOut, "  m_sPenDef.rgbColor     = 0x%6.6x (%d)\n",
            m_sPenDef.rgbColor, m_sPenDef.rgbColor);

        fflush(fpOut);
    }

    /*=====================================================================
    *                      class ITABFeatureBrush
    *====================================================================*/

    /**********************************************************************
    *                   ITABFeatureBrush::ITABFeatureBrush()
    **********************************************************************/

    // MI default is BRUSH(2, 16777215, 16777215)
    static const TABBrushDef csDefaultBrush = MITAB_BRUSH_DEFAULT;

    ITABFeatureBrush::ITABFeatureBrush() :
        m_nBrushDefIndex(-1),
        m_sBrushDef(csDefaultBrush)
    {}

    /**********************************************************************
    *                   ITABFeatureBrush::GetBrushStyleString()
    *
    *  Return a Brush() string. All representations info for the Brush are here.
    **********************************************************************/
    const char *ITABFeatureBrush::GetBrushStyleString()
    {
        const char *pszStyle = nullptr;
        int    nOGRStyle = 0;
        /* char szPattern[20]; */
        //* szPattern[0] = '\0'; */

        if (m_sBrushDef.nFillPattern == 1)
            nOGRStyle = 1;
        else if (m_sBrushDef.nFillPattern == 3)
            nOGRStyle = 2;
        else if (m_sBrushDef.nFillPattern == 4)
            nOGRStyle = 3;
        else if (m_sBrushDef.nFillPattern == 5)
            nOGRStyle = 5;
        else if (m_sBrushDef.nFillPattern == 6)
            nOGRStyle = 4;
        else if (m_sBrushDef.nFillPattern == 7)
            nOGRStyle = 6;
        else if (m_sBrushDef.nFillPattern == 8)
            nOGRStyle = 7;

        if (GetBrushTransparent())
        {
            /* Omit BG Color for transparent brushes */
            pszStyle = CPLSPrintf("BRUSH(fc:#%6.6x,id:\"mapinfo-brush-%d,ogr-brush-%d\")",
                m_sBrushDef.rgbFGColor,
                m_sBrushDef.nFillPattern, nOGRStyle);
        }
        else
        {
            pszStyle = CPLSPrintf("BRUSH(fc:#%6.6x,bc:#%6.6x,id:\"mapinfo-brush-%d,ogr-brush-%d\")",
                m_sBrushDef.rgbFGColor,
                m_sBrushDef.rgbBGColor,
                m_sBrushDef.nFillPattern, nOGRStyle);
        }

        return pszStyle;
    }

    /**********************************************************************
    *                   ITABFeatureBrush::SetBrushFromStyleString()
    *
    *  Set all Brush elements from a StyleString.
    *  Use StyleMgr to do so.
    **********************************************************************/
    void  ITABFeatureBrush::SetBrushFromStyleString(const char *pszStyleString)
    {
        GBool bIsNull = 0;

        // Use the Style Manager to retrieve all the information we need.
        OGRStyleMgr *poStyleMgr = new OGRStyleMgr(nullptr);
        OGRStyleTool *poStylePart = nullptr;

        // Init the StyleMgr with the StyleString.
        poStyleMgr->InitStyleString(pszStyleString);

        // Retrieve the Brush info.
        const int numParts = poStyleMgr->GetPartCount();
        for (int i = 0; i < numParts; i++)
        {
            poStylePart = poStyleMgr->GetPart(i);
            if (poStylePart == nullptr)
                continue;

            if (poStylePart->GetType() == OGRSTCBrush)
            {
                break;
            }
            else
            {
                delete poStylePart;
                poStylePart = nullptr;
            }
        }

        // If the no Brush found, do nothing.
        if (poStylePart == nullptr)
        {
            delete poStyleMgr;
            return;
        }

        OGRStyleBrush *poBrushStyle = (OGRStyleBrush*)poStylePart;

        // Set the Brush Id (FillPattern)
        const char *pszBrushId = poBrushStyle->Id(bIsNull);
        if (bIsNull) pszBrushId = nullptr;

        if (pszBrushId &&
            (strstr(pszBrushId, "mapinfo-brush-") ||
                strstr(pszBrushId, "ogr-brush-")))
        {
            if (strstr(pszBrushId, "mapinfo-brush-"))
            {
                const int nBrushId = atoi(pszBrushId + 14);
                SetBrushPattern((GByte)nBrushId);
            }
            else if (strstr(pszBrushId, "ogr-brush-"))
            {
                int nBrushId = atoi(pszBrushId + 10);
                if (nBrushId > 1)
                    nBrushId++;
                SetBrushPattern((GByte)nBrushId);
            }
        }

        // Set the BackColor, if not set, then it's transparent
        const char *pszBrushColor = poBrushStyle->BackColor(bIsNull);
        if (bIsNull) pszBrushColor = nullptr;

        if (pszBrushColor)
        {
            if (pszBrushColor[0] == '#')
                pszBrushColor++;
            const int nBrushColor =
                static_cast<int>(strtol(pszBrushColor, nullptr, 16));
            SetBrushBGColor((GInt32)nBrushColor);
        }
        else
        {
            SetBrushTransparent(1);
        }

        // Set the ForeColor
        pszBrushColor = poBrushStyle->ForeColor(bIsNull);
        if (bIsNull) pszBrushColor = nullptr;

        if (pszBrushColor)
        {
            if (pszBrushColor[0] == '#')
                pszBrushColor++;
            const int nBrushColor =
                static_cast<int>(strtol(pszBrushColor, nullptr, 16));
            SetBrushFGColor((GInt32)nBrushColor);
        }

        delete poStyleMgr;
        delete poStylePart;

        return;
    }

    /**********************************************************************
    *                   ITABFeatureBrush::DumpBrushDef()
    *
    * Dump Brush definition information.
    **********************************************************************/
    void ITABFeatureBrush::DumpBrushDef(FILE *fpOut /*=nullptr*/)
    {
        if (fpOut == nullptr)
            fpOut = stdout;

        fprintf(fpOut, "  m_nBrushDefIndex         = %d\n", m_nBrushDefIndex);
        fprintf(fpOut, "  m_sBrushDef.nRefCount    = %d\n", m_sBrushDef.nRefCount);
        fprintf(fpOut, "  m_sBrushDef.nFillPattern = %d\n",
            (int)m_sBrushDef.nFillPattern);
        fprintf(fpOut, "  m_sBrushDef.bTransparentFill = %d\n",
            (int)m_sBrushDef.bTransparentFill);
        fprintf(fpOut, "  m_sBrushDef.rgbFGColor   = 0x%6.6x (%d)\n",
            m_sBrushDef.rgbFGColor, m_sBrushDef.rgbFGColor);
        fprintf(fpOut, "  m_sBrushDef.rgbBGColor   = 0x%6.6x (%d)\n",
            m_sBrushDef.rgbBGColor, m_sBrushDef.rgbBGColor);

        fflush(fpOut);
    }

    /*=====================================================================
    *                      class ITABFeatureFont
    *====================================================================*/

    /**********************************************************************
    *                   ITABFeatureFont::ITABFeatureFont()
    **********************************************************************/

    // MI default is Font("Arial", 0, 0, 0)
    static const TABFontDef csDefaultFont = MITAB_FONT_DEFAULT;

    ITABFeatureFont::ITABFeatureFont() :
        m_nFontDefIndex(-1),
        m_sFontDef(csDefaultFont)
    {}

    /**********************************************************************
    *                   ITABFeatureFont::SetFontName()
    **********************************************************************/
    void ITABFeatureFont::SetFontName(const char *pszName)
    {
        strncpy(m_sFontDef.szFontName, pszName, sizeof(m_sFontDef.szFontName) - 1);
        m_sFontDef.szFontName[sizeof(m_sFontDef.szFontName) - 1] = '\0';
    }

    /**********************************************************************
    *                   ITABFeatureFont::DumpFontDef()
    *
    * Dump Font definition information.
    **********************************************************************/
    void ITABFeatureFont::DumpFontDef(FILE *fpOut /*=nullptr*/)
    {
        if (fpOut == nullptr)
            fpOut = stdout;

        fprintf(fpOut, "  m_nFontDefIndex       = %d\n", m_nFontDefIndex);
        fprintf(fpOut, "  m_sFontDef.nRefCount  = %d\n", m_sFontDef.nRefCount);
        fprintf(fpOut, "  m_sFontDef.szFontName = '%s'\n", m_sFontDef.szFontName);

        fflush(fpOut);
    }


    /*=====================================================================
    *                      class ITABFeatureSymbol
    *====================================================================*/

    /**********************************************************************
    *                   ITABFeatureSymbol::ITABFeatureSymbol()
    **********************************************************************/

    // MI default is Symbol(35, 0, 12)
    static const TABSymbolDef csDefaultSymbol = MITAB_SYMBOL_DEFAULT;

    ITABFeatureSymbol::ITABFeatureSymbol() :
        m_nSymbolDefIndex(-1),
        m_sSymbolDef(csDefaultSymbol)
    {}

    /**********************************************************************
    *                   ITABFeatureSymbol::GetSymbolStyleString()
    *
    *  Return a Symbol() string. All representations info for the Symbol are here.
    **********************************************************************/
    const char *ITABFeatureSymbol::GetSymbolStyleString(double dfAngle)
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

        nAngle += (int)dfAngle;

        pszStyle = CPLSPrintf("SYMBOL(a:%d,c:#%6.6x,s:%dpt,id:\"mapinfo-sym-%d,ogr-sym-%d\")",
            nAngle,
            m_sSymbolDef.rgbColor,
            m_sSymbolDef.nPointSize,
            m_sSymbolDef.nSymbolNo,
            nOGRStyle);

        return pszStyle;
    }

    /**********************************************************************
    *                   ITABFeatureSymbol::SetSymbolFromStyleString()
    *
    *  Set all Symbol var from a StyleString. Use StyleMgr to do so.
    **********************************************************************/
    void ITABFeatureSymbol::SetSymbolFromStyleString(const char *pszStyleString)
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

        if (pszSymbolId &&
            (strstr(pszSymbolId, "mapinfo-sym-") ||
                strstr(pszSymbolId, "ogr-sym-")))
        {
            if (strstr(pszSymbolId, "mapinfo-sym-"))
            {
                const int nSymbolId = atoi(pszSymbolId + 12);
                SetSymbolNo((GByte)nSymbolId);
            }
            else if (strstr(pszSymbolId, "ogr-sym-"))
            {
                const int nSymbolId = atoi(pszSymbolId + 8);

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

        delete poStyleMgr;
        delete poStylePart;

        return;
    }

    /**********************************************************************
    *                   ITABFeatureSymbol::DumpSymbolDef()
    *
    * Dump Symbol definition information.
    **********************************************************************/
    void ITABFeatureSymbol::DumpSymbolDef(FILE *fpOut /*=nullptr*/)
    {
        if (fpOut == nullptr)
            fpOut = stdout;

        fprintf(fpOut, "  m_nSymbolDefIndex       = %d\n", m_nSymbolDefIndex);
        fprintf(fpOut, "  m_sSymbolDef.nRefCount  = %d\n", m_sSymbolDef.nRefCount);
        fprintf(fpOut, "  m_sSymbolDef.nSymbolNo  = %d\n", m_sSymbolDef.nSymbolNo);
        fprintf(fpOut, "  m_sSymbolDef.nPointSize = %d\n", m_sSymbolDef.nPointSize);
        fprintf(fpOut, "  m_sSymbolDef._unknown_  = %d\n",
            (int)m_sSymbolDef._nUnknownValue_);
        fprintf(fpOut, "  m_sSymbolDef.rgbColor   = 0x%6.6x (%d)\n",
            m_sSymbolDef.rgbColor, m_sSymbolDef.rgbColor);

        fflush(fpOut);
    }

    /**********************************************************************
    *                       TABCleanFieldName()
    *
    * Return a copy of pszSrcName that contains only valid characters for a
    * TAB field name.  All invalid characters are replaced by '_'.
    *
    * The returned string should be freed by the caller.
    **********************************************************************/
    char *TABCleanFieldName(const char *pszSrcName)
    {
        char *pszNewName = CPLStrdup(pszSrcName);

        if (strlen(pszNewName) > 31)
        {
            pszNewName[31] = '\0';
            CPLError(
                CE_Warning, static_cast<CPLErrorNum>(TAB_WarningInvalidFieldName),
                "Field name '%s' is longer than the max of 31 characters. "
                "'%s' will be used instead.", pszSrcName, pszNewName);
        }

#if defined(_WIN32) && !defined(unix)
        // On Windows, check if we're using a double-byte codepage, and
        // if so then just keep the field name as is.
        if (_getmbcp() != 0)
            return pszNewName;
#endif

        // According to the MapInfo User's Guide (p. 240, v5.5).
        // New Table Command:
        //  Name:
        // Displays the field name in the name box. You can also enter new field
        // names here. Defaults are Field1, Field2, etc. A field name can contain
        // up to 31 alphanumeric characters. Use letters, numbers, and the
        // underscore. Do not use spaces; instead, use the underscore character
        // (_) to separate words in a field name. Use upper and lower case for
        // legibility, but MapInfo is not case-sensitive.
        //
        // It was also verified that extended chars with accents are also
        // accepted.
        int numInvalidChars = 0;
        for (int i = 0; pszSrcName && pszSrcName[i] != '\0'; i++)
        {
            if (pszSrcName[i] == '#')
            {
                if (i == 0)
                {
                    pszNewName[i] = '_';
                    numInvalidChars++;
                }
            }
            else if (!(pszSrcName[i] == '_' ||
                (i != 0 && pszSrcName[i] >= '0' && pszSrcName[i] <= '9') ||
                (pszSrcName[i] >= 'a' && pszSrcName[i] <= 'z') ||
                (pszSrcName[i] >= 'A' && pszSrcName[i] <= 'Z') ||
                static_cast<GByte>(pszSrcName[i]) >= 192))
            {
                pszNewName[i] = '_';
                numInvalidChars++;
            }
        }

        if (numInvalidChars > 0)
        {
            CPLError(
                CE_Warning, static_cast<CPLErrorNum>(TAB_WarningInvalidFieldName),
                "Field name '%s' contains invalid characters. "
                "'%s' will be used instead.", pszSrcName, pszNewName);
        }

        return pszNewName;
    }
    /**********************************************************************
    *                       TABGetBasename()
    *
    * Extract the basename part of a complete file path.
    *
    * Returns a newly allocated string without the leading path (dirs) and
    * the extension.  The returned string should be freed using CPLFree().
    **********************************************************************/
    char *TABGetBasename(const char *pszFname)
    {
        // Skip leading path or use whole name if no path dividers are encountered.
        const char *pszTmp = pszFname + strlen(pszFname) - 1;
        while (pszTmp != pszFname
            && *pszTmp != '/' && *pszTmp != '\\')
            pszTmp--;

        if (pszTmp != pszFname)
            pszTmp++;

        // Now allocate our own copy and remove extension.
        char *pszBasename = CPLStrdup(pszTmp);
        for (int i = static_cast<int>(strlen(pszBasename)) - 1; i >= 0; i--)
        {
            if (pszBasename[i] == '.')
            {
                pszBasename[i] = '\0';
                break;
            }
        }

        return pszBasename;
    }
} // end of namespace

