/******************************************************************************
 *
 * Project:  DXF and DWG Translators
 * Purpose:  Implements various generic services shared between autocad related
 *           drivers.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_autocad_services.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                           ACTextUnescape()                           */
/*                                                                      */
/*      Unexcape DXF/DWG style escape sequences such as %%d for the     */
/*      degree sign, and do the recoding to UTF8.  Set bIsMText to      */
/*      true to translate MTEXT-specific sequences like \P and \~.      */
/************************************************************************/

CPLString ACTextUnescape( const char *pszRawInput, const char *pszEncoding,
    bool bIsMText )

{
    CPLString osResult;
    CPLString osInput = pszRawInput;

/* -------------------------------------------------------------------- */
/*      Translate text from Win-1252 to UTF8.  We approximate this      */
/*      by treating Win-1252 as Latin-1.  Note that we likely ought     */
/*      to be consulting the $DWGCODEPAGE header variable which         */
/*      defaults to ANSI_1252 if not set.                               */
/* -------------------------------------------------------------------- */
    osInput.Recode( pszEncoding, CPL_ENC_UTF8 );

    const char *pszInput = osInput.c_str();

/* -------------------------------------------------------------------- */
/*      Now translate low-level escape sequences.  They are all plain   */
/*      ASCII characters and won't have been affected by the UTF8       */
/*      recoding.                                                       */
/* -------------------------------------------------------------------- */
    while( *pszInput != '\0' )
    {
        if( pszInput[0] == '^' && pszInput[1] != '\0' )
        {
            if( pszInput[1] == ' ' )
                osResult += '^';
            else
                osResult += static_cast<char>( toupper(pszInput[1]) ) ^ 0x40;
            pszInput++;
        }
        else if( STARTS_WITH_CI(pszInput, "%%c")
            || STARTS_WITH_CI(pszInput, "%%d")
            || STARTS_WITH_CI(pszInput, "%%p") )
        {
            wchar_t anWCharString[2];

            anWCharString[1] = 0;

            // These are special symbol representations for AutoCAD.
            if( STARTS_WITH_CI(pszInput, "%%c") )
                anWCharString[0] = 0x2300; // diameter (0x00F8 is a good approx)
            else if( STARTS_WITH_CI(pszInput, "%%d") )
                anWCharString[0] = 0x00B0; // degree
            else if( STARTS_WITH_CI(pszInput, "%%p") )
                anWCharString[0] = 0x00B1; // plus/minus

            char *pszUTF8Char = CPLRecodeFromWChar( anWCharString,
                CPL_ENC_UCS2,
                CPL_ENC_UTF8 );

            osResult += pszUTF8Char;
            CPLFree( pszUTF8Char );

            pszInput += 2;
        }
        else if( !bIsMText && ( STARTS_WITH_CI(pszInput, "%%u")
            || STARTS_WITH_CI(pszInput, "%%o")
            || STARTS_WITH_CI(pszInput, "%%k") ) )
        {
            // Underline, overline, and strikethrough markers.
            // These have no effect in MTEXT
            pszInput += 2;
        }
        else
        {
            osResult += pszInput[0];
        }

        pszInput++;
    }

    if( !bIsMText )
        return osResult;

/* -------------------------------------------------------------------- */
/*      If this is MTEXT, or something similar (e.g. DIMENSION text),   */
/*      do a second pass to strip additional MTEXT format codes.        */
/* -------------------------------------------------------------------- */
    pszInput = osResult.c_str();
    CPLString osMtextResult;

    while( *pszInput != '\0' )
    {
        if( pszInput[0] == '\\' && pszInput[1] == 'P' )
        {
            osMtextResult += '\n';
            pszInput++;
        }
        else if( pszInput[0] == '\\' && pszInput[1] == '~' )
        {
            osMtextResult += ' ';
            pszInput++;
        }
        else if( pszInput[0] == '\\' && pszInput[1] == 'U'
                 && pszInput[2] == '+' && CPLStrnlen(pszInput, 7) >= 7 )
        {
            CPLString osHex;
            unsigned int iChar = 0;

            osHex.assign( pszInput+3, 4 );
            sscanf( osHex.c_str(), "%x", &iChar );

            wchar_t anWCharString[2];
            anWCharString[0] = (wchar_t) iChar;
            anWCharString[1] = 0;

            char *pszUTF8Char = CPLRecodeFromWChar( anWCharString,
                                                    CPL_ENC_UCS2,
                                                    CPL_ENC_UTF8 );

            osMtextResult += pszUTF8Char;
            CPLFree( pszUTF8Char );

            pszInput += 6;
        }
        else if( pszInput[0] == '{' || pszInput[0] == '}' )
        {
            // Skip braces, which are used for grouping
        }
        else if( pszInput[0] == '\\'
                 && strchr( "WTAHFfCcQp", pszInput[1] ) != nullptr )
        {
            // eg. \W1.073172x;\T1.099;Bonneuil de Verrines
            // See data/dwg/EP/42002.dwg
            // These are all inline formatting codes which take an argument
            // up to the first semicolon (\W for width, \f for font, etc)

            while( *pszInput != ';' && *pszInput != '\0' )
                pszInput++;
            if( *pszInput == '\0' )
                break;
        }
        else if( pszInput[0] == '\\'
                && strchr( "KkLlOo", pszInput[1] ) != nullptr )
        {
            // Inline formatting codes that don't take an argument

            pszInput++;
        }
        else if( pszInput[0] == '\\' && pszInput[1] == 'S' )
        {
            // Stacked text. Normal escapes don't work inside a stack

            pszInput += 2;
            while( *pszInput != ';' && *pszInput != '\0' )
            {
                if( pszInput[0] == '\\' &&
                    strchr( "^/#~", pszInput[1] ) != nullptr )
                {
                    osMtextResult += pszInput[1];
                    pszInput++;
                    if( pszInput[0] == '\0' )
                        break;
                }
                else if( strchr( "^/#~", pszInput[0] ) == nullptr )
                {
                    osMtextResult += pszInput[0];
                }
                pszInput++;
            }
            if( pszInput[0] == ';' )
                pszInput++;
            if( pszInput[0] == '\0' )
                break;
        }
        else if( pszInput[0] == '\\'
                 && strchr( "\\{}", pszInput[1] ) != nullptr )
        {
            // MTEXT character escapes

            osMtextResult += pszInput[1];
            pszInput++;
            if( pszInput[0] == '\0' )
                break;
        }
        else
        {
            osMtextResult += *pszInput;
        }

        pszInput++;
    }

    return osMtextResult;
}

/************************************************************************/
/*                          ACGetColorTable()                           */
/************************************************************************/

const unsigned char *ACGetColorTable()

{
    static const unsigned char abyDXFColors[768] = {
          0,  0,  0, // 0
        255,  0,  0, // 1
        255,255,  0, // 2
          0,255,  0, // 3
          0,255,255, // 4
          0,  0,255, // 5
        255,  0,255, // 6
          0,  0,  0, // 7 - it should be white, but that plots poorly
        127,127,127, // 8
        191,191,191, // 9
        255,  0,  0, // 10
        255,127,127, // 11
        165,  0,  0, // 12
        165, 82, 82, // 13
        127,  0,  0, // 14
        127, 63, 63, // 15
         76,  0,  0, // 16
         76, 38, 38, // 17
         38,  0,  0, // 18
         38, 19, 19, // 19
        255, 63,  0, // 20
        255,159,127, // 21
        165, 41,  0, // 22
        165,103, 82, // 23
        127, 31,  0, // 24
        127, 79, 63, // 25
         76, 19,  0, // 26
         76, 47, 38, // 27
         38,  9,  0, // 28
         38, 23, 19, // 29
        255,127,  0, // 30
        255,191,127, // 31
        165, 82,  0, // 32
        165,124, 82, // 33
        127, 63,  0, // 34
        127, 95, 63, // 35
         76, 38,  0, // 36
         76, 57, 38, // 37
         38, 19,  0, // 38
         38, 28, 19, // 39
        255,191,  0, // 40
        255,223,127, // 41
        165,124,  0, // 42
        165,145, 82, // 43
        127, 95,  0, // 44
        127,111, 63, // 45
         76, 57,  0, // 46
         76, 66, 38, // 47
         38, 28,  0, // 48
         38, 33, 19, // 49
        255,255,  0, // 50
        255,255,127, // 51
        165,165,  0, // 52
        165,165, 82, // 53
        127,127,  0, // 54
        127,127, 63, // 55
         76, 76,  0, // 56
         76, 76, 38, // 57
         38, 38,  0, // 58
         38, 38, 19, // 59
        191,255,  0, // 60
        223,255,127, // 61
        124,165,  0, // 62
        145,165, 82, // 63
         95,127,  0, // 64
        111,127, 63, // 65
         57, 76,  0, // 66
         66, 76, 38, // 67
         28, 38,  0, // 68
         33, 38, 19, // 69
        127,255,  0, // 70
        191,255,127, // 71
         82,165,  0, // 72
        124,165, 82, // 73
         63,127,  0, // 74
         95,127, 63, // 75
         38, 76,  0, // 76
         57, 76, 38, // 77
         19, 38,  0, // 78
         28, 38, 19, // 79
         63,255,  0, // 80
        159,255,127, // 81
         41,165,  0, // 82
        103,165, 82, // 83
         31,127,  0, // 84
         79,127, 63, // 85
         19, 76,  0, // 86
         47, 76, 38, // 87
          9, 38,  0, // 88
         23, 38, 19, // 89
          0,255,  0, // 90
        127,255,127, // 91
          0,165,  0, // 92
         82,165, 82, // 93
          0,127,  0, // 94
         63,127, 63, // 95
          0, 76,  0, // 96
         38, 76, 38, // 97
          0, 38,  0, // 98
         19, 38, 19, // 99
          0,255, 63, // 100
        127,255,159, // 101
          0,165, 41, // 102
         82,165,103, // 103
          0,127, 31, // 104
        63,127, 79, // 105
        0, 76, 19, // 106
        38, 76, 47, // 107
        0, 38,  9, // 108
        19, 38, 23, // 109
        0,255,127, // 110
        127,255,191, // 111
        0,165, 82, // 112
        82,165,124, // 113
        0,127, 63, // 114
        63,127, 95, // 115
        0, 76, 38, // 116
        38, 76, 57, // 117
        0, 38, 19, // 118
        19, 38, 28, // 119
        0,255,191, // 120
        127,255,223, // 121
        0,165,124, // 122
        82,165,145, // 123
        0,127, 95, // 124
        63,127,111, // 125
        0, 76, 57, // 126
        38, 76, 66, // 127
        0, 38, 28, // 128
        19, 38, 33, // 129
        0,255,255, // 130
        127,255,255, // 131
        0,165,165, // 132
        82,165,165, // 133
        0,127,127, // 134
        63,127,127, // 135
        0, 76, 76, // 136
        38, 76, 76, // 137
        0, 38, 38, // 138
        19, 38, 38, // 139
        0,191,255, // 140
        127,223,255, // 141
        0,124,165, // 142
        82,145,165, // 143
        0, 95,127, // 144
        63,111,127, // 145
        0, 57, 76, // 146
        38, 66, 76, // 147
        0, 28, 38, // 148
        19, 33, 38, // 149
        0,127,255, // 150
        127,191,255, // 151
        0, 82,165, // 152
        82,124,165, // 153
        0, 63,127, // 154
        63, 95,127, // 155
        0, 38, 76, // 156
        38, 57, 76, // 157
        0, 19, 38, // 158
        19, 28, 38, // 159
        0, 63,255, // 160
        127,159,255, // 161
        0, 41,165, // 162
        82,103,165, // 163
        0, 31,127, // 164
        63, 79,127, // 165
        0, 19, 76, // 166
        38, 47, 76, // 167
        0,  9, 38, // 168
        19, 23, 38, // 169
        0,  0,255, // 170
        127,127,255, // 171
        0,  0,165, // 172
        82, 82,165, // 173
        0,  0,127, // 174
        63, 63,127, // 175
        0,  0, 76, // 176
        38, 38, 76, // 177
        0,  0, 38, // 178
        19, 19, 38, // 179
        63,  0,255, // 180
        159,127,255, // 181
        41,  0,165, // 182
        103, 82,165, // 183
        31,  0,127, // 184
        79, 63,127, // 185
        19,  0, 76, // 186
        47, 38, 76, // 187
        9,  0, 38, // 188
        23, 19, 38, // 189
        127,  0,255, // 190
        191,127,255, // 191
        82,  0,165, // 192
        124, 82,165, // 193
        63,  0,127, // 194
        95, 63,127, // 195
        38,  0, 76, // 196
        57, 38, 76, // 197
        19,  0, 38, // 198
        28, 19, 38, // 199
        191,  0,255, // 200
        223,127,255, // 201
        124,  0,165, // 202
        145, 82,165, // 203
        95,  0,127, // 204
        111, 63,127, // 205
        57,  0, 76, // 206
        66, 38, 76, // 207
        28,  0, 38, // 208
        33, 19, 38, // 209
        255,  0,255, // 210
        255,127,255, // 211
        165,  0,165, // 212
        165, 82,165, // 213
        127,  0,127, // 214
        127, 63,127, // 215
        76,  0, 76, // 216
        76, 38, 76, // 217
        38,  0, 38, // 218
        38, 19, 38, // 219
        255,  0,191, // 220
        255,127,223, // 221
        165,  0,124, // 222
        165, 82,145, // 223
        127,  0, 95, // 224
        127, 63,111, // 225
        76,  0, 57, // 226
        76, 38, 66, // 227
        38,  0, 28, // 228
        38, 19, 33, // 229
        255,  0,127, // 230
        255,127,191, // 231
        165,  0, 82, // 232
        165, 82,124, // 233
        127,  0, 63, // 234
        127, 63, 95, // 235
        76,  0, 38, // 236
        76, 38, 57, // 237
        38,  0, 19, // 238
        38, 19, 28, // 239
        255,  0, 63, // 240
        255,127,159, // 241
        165,  0, 41, // 242
        165, 82,103, // 243
        127,  0, 31, // 244
        127, 63, 79, // 245
        76,  0, 19, // 246
        76, 38, 47, // 247
        38,  0,  9, // 248
        38, 19, 23, // 249
        84, 84, 84, // 250
        118,118,118, // 251
        152,152,152, // 252
        186,186,186, // 253
        220,220,220, // 254
        255,255,255  // 255
    };

    return abyDXFColors;
}

/************************************************************************/
/*                       ACGetKnownDimStyleCodes()                      */
/*                                                                      */
/*      Gets a list of the DIMSTYLE codes that we care about. Array     */
/*      terminates with a zero value.                                   */
/************************************************************************/

const int* ACGetKnownDimStyleCodes()
{
    static const int aiKnownCodes[] = {
        40, 41, 42, 44, 75, 76, 77, 140, 147, 176, 178, 271, 341, 0
    };

    return aiKnownCodes;
}

/************************************************************************/
/*                      ACGetDimStylePropertyName()                     */
/************************************************************************/

const char *ACGetDimStylePropertyName( const int iDimStyleCode )

{
    // We are only interested in properties required by the DIMENSION
    // and LEADER code. Return NULL for other properties.
    switch (iDimStyleCode)
    {
        case 40: return "DIMSCALE";
        case 41: return "DIMASZ";
        case 42: return "DIMEXO";
        case 44: return "DIMEXE";
        case 75: return "DIMSE1";
        case 76: return "DIMSE2";
        case 77: return "DIMTAD";
        case 140: return "DIMTXT";
        case 147: return "DIMGAP";
        case 176: return "DIMCLRD";
        case 178: return "DIMCLRT";
        case 271: return "DIMDEC";
        case 341: return "DIMLDRBLK";
        default: return nullptr;
    }
}

/************************************************************************/
/*                    ACGetDimStylePropertyDefault()                    */
/************************************************************************/

const char *ACGetDimStylePropertyDefault( const int iDimStyleCode )

{
    // We are only interested in properties required by the DIMENSION
    // and LEADER code. Return "0" for other, unknown properties.
    // These defaults were obtained from the Express\defaults.scr file
    // in an AutoCAD installation.
    switch (iDimStyleCode)
    {
        case 40: return "1.0";
        case 41: return "0.18";
        case 42: return "0.0625";
        case 44: return "0.18";
        case 75: return "0";
        case 76: return "0";
        case 77: return "0";
        case 140: return "0.18";
        case 147: return "0.09";
        case 176: return "0";
        case 178: return "0";
        case 271: return "4";
        case 341: return "";
        default: return "0";
    }
}

/************************************************************************/
/*                            ACAdjustText()                            */
/*                                                                      */
/*      Rotate and scale text features by the designated amount by      */
/*      adjusting the style string.                                     */
/************************************************************************/

void ACAdjustText( const double dfAngle, const double dfScaleX,
    const double dfScaleY, OGRFeature* const poFeature )

{
/* -------------------------------------------------------------------- */
/*      We only try to alter text elements (LABEL styles).              */
/* -------------------------------------------------------------------- */
    if( poFeature->GetStyleString() == nullptr )
        return;

    CPLString osOldStyle = poFeature->GetStyleString();

    if( !STARTS_WITH( osOldStyle, "LABEL(" ) )
        return;

    // Split the style string up into its parts
    osOldStyle.erase( 0, 6 );
    osOldStyle.erase( osOldStyle.size() - 1 );
    char **papszTokens = CSLTokenizeString2( osOldStyle, ",",
        CSLT_HONOURSTRINGS | CSLT_PRESERVEQUOTES | CSLT_PRESERVEESCAPES );

/* -------------------------------------------------------------------- */
/*      Update the text angle.                                          */
/* -------------------------------------------------------------------- */
    char szBuffer[64];

    if( dfAngle != 0.0 )
    {
        double dfOldAngle = 0.0;

        const char *pszAngle = CSLFetchNameValue( papszTokens, "a" );
        if( pszAngle )
            dfOldAngle = CPLAtof( pszAngle );

        CPLsnprintf( szBuffer, sizeof(szBuffer), "%.3g", dfOldAngle + dfAngle );
        papszTokens = CSLSetNameValue( papszTokens, "a", szBuffer );
    }

/* -------------------------------------------------------------------- */
/*      Update the text width and height.                               */
/* -------------------------------------------------------------------- */

    if( dfScaleY != 1.0 )
    {
        const char *pszHeight = CSLFetchNameValue( papszTokens, "s" );
        if( pszHeight )
        {
            const double dfOldHeight = CPLAtof( pszHeight );

            CPLsnprintf( szBuffer, sizeof(szBuffer), "%.3gg",
                dfOldHeight * dfScaleY );
            papszTokens = CSLSetNameValue( papszTokens, "s", szBuffer );
        }
    }

    if( dfScaleX != dfScaleY && dfScaleY != 0.0 )
    {
        const double dfWidthFactor = dfScaleX / dfScaleY;
        double dfOldWidth = 100.0;

        const char *pszWidth = CSLFetchNameValue( papszTokens, "w" );
        if( pszWidth )
            dfOldWidth = CPLAtof( pszWidth );

        CPLsnprintf( szBuffer, sizeof(szBuffer), "%.4g",
            dfOldWidth * dfWidthFactor );
        papszTokens = CSLSetNameValue( papszTokens, "w", szBuffer );
    }

/* -------------------------------------------------------------------- */
/*      Update the text offsets.                                        */
/* -------------------------------------------------------------------- */

    if( dfScaleX != 1.0 || dfScaleY != 1.0 || dfAngle != 0.0 )
    {
        double dfOldDx = 0.0;
        double dfOldDy = 0.0;

        const char *pszDx = CSLFetchNameValue( papszTokens, "dx" );
        if( pszDx )
            dfOldDx = CPLAtof( pszDx );
        const char *pszDy = CSLFetchNameValue( papszTokens, "dy" );
        if( pszDy )
            dfOldDy = CPLAtof( pszDy );

        if( dfOldDx != 0.0 || dfOldDy != 0.0 )
        {
            const double dfAngleRadians = dfAngle * M_PI / 180.0;

            CPLsnprintf( szBuffer, sizeof(szBuffer), "%.6gg",
                dfScaleX * dfOldDx * cos( dfAngleRadians ) +
                dfScaleY * dfOldDy * -sin( dfAngleRadians ) );
            papszTokens = CSLSetNameValue( papszTokens, "dx", szBuffer );

            CPLsnprintf( szBuffer, sizeof(szBuffer), "%.6gg",
                dfScaleX * dfOldDx * sin( dfAngleRadians ) +
                dfScaleY * dfOldDy * cos( dfAngleRadians ) );
            papszTokens = CSLSetNameValue( papszTokens, "dy", szBuffer );
        }
    }

    CSLSetNameValueSeparator( papszTokens, ":" );

    CPLString osNewStyle = "LABEL(";
    int iIndex = 0;
    while( papszTokens[iIndex] )
    {
        if( iIndex > 0 )
            osNewStyle += ",";
        osNewStyle += papszTokens[iIndex++];
    }
    osNewStyle += ")";

    poFeature->SetStyleString( osNewStyle );

    CSLDestroy( papszTokens );
}
