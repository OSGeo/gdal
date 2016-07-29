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

CPL_CVSID("$Id$");

/************************************************************************/
/*                           ACTextUnescape()                           */
/*                                                                      */
/*      Unexcape DXF/DWG style escape sequences such as \P for newline  */
/*      and \~ for space, and do the recoding to UTF8.                  */
/************************************************************************/

CPLString ACTextUnescape( const char *pszRawInput, const char *pszEncoding )

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
/*      Now translate escape sequences.  They are all plain ascii       */
/*      characters and won't have been affected by the UTF8             */
/*      recoding.                                                       */
/* -------------------------------------------------------------------- */
    while( *pszInput != '\0' )
    {
        if( pszInput[0] == '\\' && pszInput[1] == 'P' )
        {
            osResult += '\n';
            pszInput++;
        }
        else if( pszInput[0] == '\\' && pszInput[1] == '~' )
        {
            osResult += ' ';
            pszInput++;
        }
        else if( pszInput[0] == '\\' && pszInput[1] == 'U'
                 && pszInput[2] == '+' )
        {
            CPLString osHex;
            int iChar;

            osHex.assign( pszInput+3, 4 );
            sscanf( osHex.c_str(), "%x", &iChar );

            wchar_t anWCharString[2];
            anWCharString[0] = (wchar_t) iChar;
            anWCharString[1] = 0;

            char *pszUTF8Char = CPLRecodeFromWChar( anWCharString,
                                                    CPL_ENC_UCS2,
                                                    CPL_ENC_UTF8 );

            osResult += pszUTF8Char;
            CPLFree( pszUTF8Char );

            pszInput += 6;
        }
        else if( pszInput[0] == '\\'
                 && (pszInput[1] == 'W'
                     || pszInput[1] == 'T'
                     || pszInput[1] == 'A' ) )
        {
            // eg. \W1.073172x;\T1.099;Bonneuil de Verrines
            // See data/dwg/EP/42002.dwg
            // Not sure what \W and \T do, but we skip them.
            // According to qcad rs_text.cpp, \A values are vertical
            // alignment, 0=bottom, 1=mid, 2=top but we ignore for now.

            while( *pszInput != ';' && *pszInput != '\0' )
                pszInput++;
        }
        else if( pszInput[0] == '\\' && pszInput[1] == '\\' )
        {
            osResult += '\\';
            pszInput++;
        }
        else if( STARTS_WITH_CI(pszInput, "%%c")
                 || STARTS_WITH_CI(pszInput, "%%d")
                 || STARTS_WITH_CI(pszInput, "%%p") )
        {
            wchar_t anWCharString[2];

            anWCharString[1] = 0;

            // These are especial symbol representations for autocad.
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
        else
            osResult += *pszInput;

        pszInput++;
    }

    return osResult;
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
/*                            ACAdjustText()                            */
/*                                                                      */
/*      Rotate and scale text features by the designated amount by      */
/*      adjusting the style string.                                     */
/************************************************************************/

void ACAdjustText( double dfAngle, double dfScale, OGRFeature *poFeature )

{
/* -------------------------------------------------------------------- */
/*      We only try to alter text elements (LABEL styles).              */
/* -------------------------------------------------------------------- */
    if( poFeature->GetStyleString() == NULL )
        return;

    CPLString osOldStyle = poFeature->GetStyleString();

    if( strstr(osOldStyle,"LABEL") == NULL )
        return;

/* -------------------------------------------------------------------- */
/*      Is there existing angle text?                                   */
/* -------------------------------------------------------------------- */
    double dfOldAngle = 0.0;
    CPLString osPreAngle, osPostAngle;
    size_t nAngleOff = osOldStyle.find( ",a:" );

    if( nAngleOff != std::string::npos )
    {
        size_t nEndOfAngleOff = osOldStyle.find( ",", nAngleOff + 1 );

        if( nEndOfAngleOff == std::string::npos )
            nEndOfAngleOff = osOldStyle.find( ")", nAngleOff + 1 );

        osPreAngle.assign( osOldStyle, 0, nAngleOff );
        osPostAngle.assign( osOldStyle, nEndOfAngleOff, std::string::npos );

        dfOldAngle = CPLAtof( osOldStyle.c_str() + nAngleOff + 3 );
    }
    else
    {
        CPLAssert( osOldStyle[osOldStyle.size()-1] == ')' );
        osPreAngle.assign( osOldStyle, 0, osOldStyle.size() - 1 );
        osPostAngle = ")";
    }

/* -------------------------------------------------------------------- */
/*      Format with the new angle.                                      */
/* -------------------------------------------------------------------- */
    CPLString osNewStyle;

    osNewStyle.Printf( "%s,a:%g%s",
                       osPreAngle.c_str(),
                       dfOldAngle + dfAngle,
                       osPostAngle.c_str() );

    osOldStyle = osNewStyle;

/* -------------------------------------------------------------------- */
/*      Is there existing scale text?                                   */
/* -------------------------------------------------------------------- */
    double dfOldScale = 1.0;
    CPLString osPreScale, osPostScale;
    size_t nScaleOff = osOldStyle.find( ",s:" );

    if( nScaleOff != std::string::npos )
    {
        size_t nEndOfScaleOff = osOldStyle.find( ",", nScaleOff + 1 );

        if( nEndOfScaleOff == std::string::npos )
            nEndOfScaleOff = osOldStyle.find( ")", nScaleOff + 1 );

        osPreScale.assign( osOldStyle, 0, nScaleOff );
        osPostScale.assign( osOldStyle, nEndOfScaleOff, std::string::npos );

        dfOldScale = CPLAtof( osOldStyle.c_str() + nScaleOff + 3 );
    }
    else
    {
        CPLAssert( osOldStyle[osOldStyle.size()-1] == ')' );
        osPreScale.assign( osOldStyle, 0, osOldStyle.size() - 1 );
        osPostScale = ")";
    }

/* -------------------------------------------------------------------- */
/*      Format with the new scale.                                      */
/* -------------------------------------------------------------------- */
    osNewStyle.Printf( "%s,s:%gg%s",
                       osPreScale.c_str(),
                       dfOldScale * dfScale,
                       osPostScale.c_str() );

    poFeature->SetStyleString( osNewStyle );
}
