/******************************************************************************
*
* Project:  EFAL Translator
* Purpose:  Implements OGREFALLayer class
* Author:   Pitney Bowes
*
******************************************************************************
* Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
* Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#pragma warning(disable:4251)
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


class EFALFeaturePen : public EFAL_GDAL_DRIVER::ITABFeaturePen
{
public:
	EFALFeaturePen() : ITABFeaturePen() {}
	~EFALFeaturePen() {};

	const char *GetMapBasicStyleClause();
};
class EFALFeatureBrush : public EFAL_GDAL_DRIVER::ITABFeatureBrush
{
public:
	EFALFeatureBrush() : ITABFeatureBrush() {}
	~EFALFeatureBrush() {};

	const char *GetMapBasicStyleClause();
};
class EFALFeatureSymbol : public EFAL_GDAL_DRIVER::ITABFeatureSymbol
{
public:
	EFALFeatureSymbol() : ITABFeatureSymbol() {}
	~EFALFeatureSymbol() {};

	const char *GetMapBasicStyleClause();
};


class EFALFeatureSymbolFont : public EFALFeatureSymbol, public EFAL_GDAL_DRIVER::ITABFeatureFont
{
protected:
protected:
	double      m_dAngle;
	GInt16      m_nFontStyle;           // Bold/shadow/halo/etc.

public:
	EFALFeatureSymbolFont();
	~EFALFeatureSymbolFont() {};

	const char *GetSymbolStyleString();
	//TODO void        SetSymbolFromStyleString(const char *pszStyleString);

	int         GetFontStyleTABValue() { return m_nFontStyle; };
	void        SetFontStyleTABValue(int nStyle) { m_nFontStyle = (GInt16)nStyle; };

	// GetSymbolAngle(): Return angle in degrees counterclockwise
	double      GetSymbolAngle() { return m_dAngle; };
	void        SetSymbolAngle(double dAngle);
};
/*=====================================================================
*                      class EFALFeatureSymbolFont
*====================================================================*/
EFALFeatureSymbolFont::EFALFeatureSymbolFont() :
	EFALFeatureSymbol(),
	ITABFeatureFont()
{}
/**********************************************************************
*                   EFALFeatureSymbolFont::SetSymbolAngle()
*
* Set the symbol angle value in degrees, making sure the value is
* always in the range [0..360]
**********************************************************************/
void EFALFeatureSymbolFont::SetSymbolAngle(double dAngle)
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
const char *EFALFeatureSymbolFont::GetSymbolStyleString()
{
	// TODO: this ignores the font name and font size. This is because we need to use a LABEL string here.... I think the outline color stuff is correct unless it doesn't translate

	/* Get the SymbolStyleString, and add the outline Color
	(halo/border in MapInfo Symbol terminology) */
	char *pszSymbolStyleString =
		CPLStrdup(ITABFeatureSymbol::GetSymbolStyleString(GetSymbolAngle()));
	int nStyleStringlen = static_cast<int>(strlen(pszSymbolStyleString));
	pszSymbolStyleString[nStyleStringlen - 1] = '\0';

	/*

	TODO:Symbol (58,16711680,12,"MapInfo Cartographic",256,0)
	Point style for Washington DC in USA_CAPS
	MAP file stores a FontStyle value of 512. Why do I get 256 in the MapBasic string?

	*/
	const char *outlineColor = NULL;
	if (m_nFontStyle & 16)
		outlineColor = ",o:#000000";
	else if (m_nFontStyle & 512)
		outlineColor = ",o:#ffffff";
	else
		outlineColor = "";

	const char *pszStyle = NULL;
	pszStyle = CPLSPrintf("%s%s)", pszSymbolStyleString, outlineColor);
	return pszStyle;
}
/**********************************************************************
*                 EFALFeaturePen::GetMapBasicStyleClause()
*
*  Return MapBasic clause for this style
**********************************************************************/
const char *EFALFeaturePen::GetMapBasicStyleClause()
{
	const char *pszStyle = NULL;
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
	const char *pszStyle = NULL;
	pszStyle = CPLSPrintf("Brush(%d,%d,%d)", (int)m_sBrushDef.nFillPattern, (int)m_sBrushDef.rgbFGColor, (int)m_sBrushDef.rgbBGColor);
	return pszStyle;
}
/**********************************************************************
*                 EFALFeatureSymbol::GetMapBasicStyleClause()
*
*  Return MapBasic clause for this style
**********************************************************************/
const char *EFALFeatureSymbol::GetMapBasicStyleClause()
{
	const char *pszStyle = NULL;
	pszStyle = CPLSPrintf("Symbol(%d,%d,%d)", (int)m_sSymbolDef.nSymbolNo, (int)m_sSymbolDef.rgbColor, (int)m_sSymbolDef.nPointSize);
	return pszStyle;
}

char *mystrtok(char *str, const char *sep, char** context) {
	char * s = NULL;
	if (str == NULL) {
		s = *context;
	}
	else {
		s = str;
	}
	while (*s && strchr(sep, *s) != NULL) s++;
	if (!*s) return NULL;
	char *t = s;
	if (*s == '"')
	{
		s++;
		t = s;
		while (*s && *s != '"') s++;
		*s = NULL;
		s++;
	}
	else
	{
		while (*s && strchr(sep, *s) == NULL) s++;
		if (*s)
		{
			*s = NULL;
			s++;
		}
	}
	*context = s;
	return t;
}
//static int HexadecimalToDecimal(const char * hex, int hexLength) {
//	double dec = 0;
//
//	for (int i = 0; i < hexLength; ++i)
//	{
//		char b = hex[i];
//
//		if (b >= 48 && b <= 57)
//			b -= 48;
//		else if (b >= 65 && b <= 70)
//			b -= 55;
//
//		dec += b * pow(16, ((hexLength - i) - 1));
//	}
//
//	return (int)dec;
//}
//
//static int hex2rgb(const char * hex) {
//	if (hex[0] == '#') hex++;
//
//	int r = HexadecimalToDecimal(hex+0, 2);
//	int g = HexadecimalToDecimal(hex+2, 2);
//	int b = HexadecimalToDecimal(hex+4, 2);
//	return (r << 16) + (g << 8) + b;
//}
//CPLString rgb2hex(int color)
//{
//	const char * h = "0123456789ABCDEF";
//	int r = color >> 16;
//	int g = (color - (r << 16)) >> 8;
//	int b = (color - (r << 16)) - (g << 8);
//	CPLString hex = "#";
//	hex += h[(r >> 4)];
//	hex += h[(r - ((r >> 4) << 4))];
//	hex += h[(g >> 4)];
//	hex += h[(g - ((g >> 4) << 4))];
//	hex += h[(b >> 4)];
//	hex += h[(b - ((b >> 4) << 4))];
//	return hex;
//}

/************************************************************************/
/*                      MapBasicStyle2OGRStyle()                        */
/************************************************************************/
CPLString OGREFALLayer::MapBasicStyle2OGRStyle(const wchar_t * mbStyle) const
{
	if (mbStyle)
	{
		CPLString ogrStyle = "";

		char * mbStyleTokenString = NULL;
		mbStyleTokenString = CPLRecodeFromWChar(mbStyle, CPL_ENC_UCS2, CPL_ENC_UTF8);

		char seps[] = " ,()";
		char *token = NULL;
		char *next_token = NULL;

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
		bool inSymbol = false, inPen = false, inBrush = false;
		bool haveThickness = false, havePattern = false, haveColor = false, haveBackgroundColor = false, haveShape = false, haveSize = false, haveRotation = false, haveFontStyle = false, haveFontName = false, haveBitMapName = false, haveCustomStyle = false;
		int thickness = 0, pattern = 0, color = 0, backgroundColor = 0, shape = 0, size = 0, fontStyle = 0;
		// TODO: Is custom style in Symbol clause a float for translucency???
		// TODO: Is size in Symbol clauses a float for font point size???
		float rotation = 0, customStyle = 0;
		char * fontName = NULL, *bitmapName = NULL;
		bool done = false;
		while (!done)
		{
			if ((token == NULL)
				|| (stricmp(token, "Symbol") == 0)
				|| (stricmp(token, "Pen") == 0)
				|| (stricmp(token, "Brush") == 0))
			{
				// About to start a new clause, see if we need to finish a previous clause first...
				if (inSymbol)
				{
					// finish the last symbol
					if (haveShape && haveFontName)
					{
						EFALFeatureSymbolFont symbol;
						symbol.SetSymbolNo((GInt16)shape);
						symbol.SetSymbolColor(color);
						symbol.SetSymbolSize((GInt16)size);
						symbol.SetFontStyleTABValue(fontStyle);
						symbol.SetSymbolAngle(rotation);
						symbol.SetFontName(fontName);
						if (ogrStyle.length() > 0) ogrStyle += ";";
						ogrStyle += symbol.GetSymbolStyleString(); // TODO: This impl of this isn't right!!!
					}
					else if (haveShape)
					{
						EFAL_GDAL_DRIVER::ITABFeatureSymbol symbol;
						symbol.SetSymbolNo((GInt16)shape);
						symbol.SetSymbolColor(color);
						symbol.SetSymbolSize((GInt16)size);
						if (ogrStyle.length() > 0) ogrStyle += ";";
						ogrStyle += symbol.GetSymbolStyleString();
					}
					else if (haveBitMapName)
					{
						// TODO!!!
					}
				}
				else if (inPen)
				{
					// finish the last pen
					EFAL_GDAL_DRIVER::ITABFeaturePen pen;
					pen.SetPenColor(color);
					pen.SetPenPattern((GByte)pattern);
					if (thickness > 10)
						pen.SetPenWidthPoint(thickness);
					else
						pen.SetPenWidthPixel((GByte)thickness);
					if (ogrStyle.length() > 0) ogrStyle += ";";
					ogrStyle += pen.GetPenStyleString();
				}
				else if (inBrush)
				{
					// finish the last brush
					EFAL_GDAL_DRIVER::ITABFeatureBrush brush;
					brush.SetBrushFGColor(color);
					brush.SetBrushBGColor(backgroundColor);
					brush.SetBrushPattern((GByte)pattern);
					brush.SetBrushTransparent(pattern >= 3);

					if (ogrStyle.length() > 0) ogrStyle += ";";
					ogrStyle += brush.GetBrushStyleString();
				}
			}
			if (token == NULL)
			{
				done = true;
				continue;
			}
			else if (stricmp(token, "Symbol") == 0)
			{
				inSymbol = true;
				inPen = false;
				inBrush = false;
				haveShape = haveColor = haveSize = haveFontName = haveFontStyle = haveRotation = haveBitMapName = haveCustomStyle = false;
			}
			else if (stricmp(token, "Pen") == 0)
			{
				inSymbol = false;
				inPen = true;
				inBrush = false;
				haveThickness = havePattern = haveColor = false;
			}
			else if (stricmp(token, "Brush") == 0)
			{
				inSymbol = false;
				inPen = false;
				inBrush = true;
				havePattern = haveColor = haveBackgroundColor = false;
			}
			else if (inSymbol)
			{
				if (!haveShape && !haveColor && !haveSize && !haveFontName && !haveFontStyle && !haveRotation && !haveBitMapName && !haveCustomStyle)
				{
					shape = atoi(token);
					if (shape > 0) {
						haveShape = true;
					}
					else {
						bitmapName = token;
						haveBitMapName = true;
					}
				}
				else if (haveShape && !haveColor && !haveSize && !haveFontName && !haveFontStyle && !haveRotation)
				{
					color = atoi(token);
					haveColor = true;
				}
				else if (haveShape && haveColor && !haveSize && !haveFontName && !haveFontStyle && !haveRotation)
				{
					size = atoi(token);
					haveSize = true;
				}
				else if (haveShape && haveColor && haveSize && !haveFontName && !haveFontStyle && !haveRotation)
				{
					fontName = token;
					haveFontName = true;
				}
				else if (haveShape && haveColor && haveSize && haveFontName && !haveFontStyle && !haveRotation)
				{
					fontStyle = atoi(token);
					haveFontStyle = true;
				}
				else if (haveShape && haveColor && haveSize && haveFontName && haveFontStyle && !haveRotation)
				{
					rotation = (float)atof(token);
					haveRotation = true;
				}
				else if (haveBitMapName && !haveColor && !haveSize && !haveCustomStyle)
				{
					color = atoi(token);
					haveColor = true;
				}
				else if (haveBitMapName && haveColor && !haveSize && !haveCustomStyle)
				{
					size = atoi(token);
					haveSize = true;
				}
				else if (haveBitMapName && haveColor && haveSize && !haveCustomStyle)
				{
					customStyle = (float)atof(token);
					haveCustomStyle = true;
				}
				else
				{
					printf("Invalid token (%s) in Symbol clause", token);
				}
			}
			else if (inPen)
			{
				if (!haveThickness && !havePattern && !haveColor)
				{
					thickness = atoi(token);
					haveThickness = true;
				}
				else if (haveThickness && !havePattern && !haveColor)
				{
					pattern = atoi(token);
					havePattern = true;
				}
				else if (haveThickness && havePattern && !haveColor)
				{
					color = atoi(token);
					haveColor = true;
				}
				else
				{
					printf("Invalid token (%s) in Pen clause", token);
				}
			}
			else if (inBrush)
			{
				if (!havePattern && !haveColor && !haveBackgroundColor)
				{
					pattern = atoi(token);
					havePattern = true;
				}
				else if (havePattern && !haveColor && !haveBackgroundColor)
				{
					color = atoi(token);
					haveColor = true;
				}
				else if (havePattern && haveColor && !haveBackgroundColor)
				{
					backgroundColor = atoi(token);
					haveBackgroundColor = true;
				}
				else
				{
					printf("Invalid token (%s) in Brush clause", token);
				}
			}
			if (token != NULL)
			{
				token = mystrtok(NULL, seps, &next_token);
			}
		}
		CPLFree(mbStyleTokenString);

		return ogrStyle;
	}
	return NULL;
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
			mbStyle += pen.GetMapBasicStyleClause();
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
			mbStyle += brush.GetMapBasicStyleClause();
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
			mbStyle += symbol.GetMapBasicStyleClause();
		}
		else
			s++;
	}
	CPLFree(t);
	return CPLStrdup(mbStyle);
}
