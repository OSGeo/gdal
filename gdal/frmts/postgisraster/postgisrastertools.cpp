/***********************************************************************
 * File :    postgisrastertools.cpp
 * Project:  PostGIS Raster driver
 * Purpose:  Tools for PostGIS Raster driver
 * Author:   Jorge Arevalo, jorge.arevalo@deimos-space.com
 *                          jorgearevalo@libregis.org
 *
 * Author:       David Zwarg, dzwarg@azavea.com
 *
 * Last changes: $Id$
 *
 ***********************************************************************
 * Copyright (c) 2009 - 2013, Jorge Arevalo, David Zwarg
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 **********************************************************************/
 #include "postgisraster.h"

CPL_CVSID("$Id$")

 /**********************************************************************
 * \brief Replace the quotes by single quotes in the input string
 *
 * Needed in the 'where' part of the input string
 **********************************************************************/
char * ReplaceQuotes(const char * pszInput, int nLength) {
    int i;
    char * pszOutput = nullptr;

    if (nLength == -1)
        nLength = static_cast<int>(strlen(pszInput));

    pszOutput = static_cast<char*>(CPLCalloc(nLength + 1, sizeof (char)));

    for (i = 0; i < nLength; i++) {
        if (pszInput[i] == '"')
            pszOutput[i] = '\'';
        else
            pszOutput[i] = pszInput[i];
    }

    return pszOutput;
}

/**************************************************************
 * \brief Replace the single quotes by " in the input string
 *
 * Needed before tokenize function
 *************************************************************/
char * ReplaceSingleQuotes(const char * pszInput, int nLength) {
    int i;
    char* pszOutput = nullptr;

    if (nLength == -1)
        nLength = static_cast<int>(strlen(pszInput));

    pszOutput = static_cast<char*>(CPLCalloc(nLength + 1, sizeof (char)));

    for (i = 0; i < nLength; i++)
    {
        if (pszInput[i] == '\'')
            pszOutput[i] = '"';
        else
            pszOutput[i] = pszInput[i];
    }

    return pszOutput;
}

/***********************************************************************
 * \brief Split connection string into user, password, host, database...
 *
 * The parameters separated by spaces are return as a list of strings.
 * The function accepts all the PostgreSQL recognized parameter keywords.
 *
 * The returned list must be freed with CSLDestroy when no longer needed
 **********************************************************************/
char** ParseConnectionString(const char * pszConnectionString) {

    /* Escape string following SQL scheme */
    char* pszEscapedConnectionString =
        ReplaceSingleQuotes(pszConnectionString, -1);

    /* Avoid PG: part */
    char* pszStartPos = strstr(pszEscapedConnectionString, ":") + 1;

    /* Tokenize */
    char** papszParams =
        CSLTokenizeString2(pszStartPos, " ", CSLT_HONOURSTRINGS);

    /* Free */
    CPLFree(pszEscapedConnectionString);

    return papszParams;
}

/***********************************************************************
 * \brief Translate a PostGIS Raster datatype string in a valid
 * GDALDataType object.
 **********************************************************************/
GBool TranslateDataType(const char * pszDataType,
        GDALDataType * poDataType = nullptr, int * pnBitsDepth = nullptr,
        GBool * pbSignedByte = nullptr)
{
    if (!pszDataType)
        return false;

    if (pbSignedByte)
        *pbSignedByte = false;

    if (EQUAL(pszDataType, "1BB")) {
        if (pnBitsDepth)
            *pnBitsDepth = 1;
        if (poDataType)
            *poDataType = GDT_Byte;
    }

    else if (EQUAL(pszDataType, "2BUI")) {
        if (pnBitsDepth)
            *pnBitsDepth = 2;
        if (poDataType)
            *poDataType = GDT_Byte;
    }

    else if (EQUAL(pszDataType, "4BUI")) {
        if (pnBitsDepth)
            *pnBitsDepth = 4;
        if (poDataType)
            *poDataType = GDT_Byte;
    }

    else if (EQUAL(pszDataType, "8BUI")) {
        if (pnBitsDepth)
            *pnBitsDepth = 8;
        if (poDataType)
            *poDataType = GDT_Byte;
    }

    else if (EQUAL(pszDataType, "8BSI")) {
        if (pnBitsDepth)
            *pnBitsDepth = 8;
        if (poDataType)
            *poDataType = GDT_Byte;

        /**
         * To indicate the unsigned byte values between 128 and 255
         * should be interpreted as being values between -128 and -1 for
         * applications that recognize the SIGNEDBYTE type.
         **/
        if (pbSignedByte)
            *pbSignedByte = true;
    }
    else if (EQUAL(pszDataType, "16BSI")) {
        if (pnBitsDepth)
            *pnBitsDepth = 16;
        if (poDataType)
            *poDataType = GDT_Int16;
    }

    else if (EQUAL(pszDataType, "16BUI")) {
        if (pnBitsDepth)
            *pnBitsDepth = 16;
        if (poDataType)
            *poDataType = GDT_UInt16;
    }

    else if (EQUAL(pszDataType, "32BSI")) {
        if (pnBitsDepth)
            *pnBitsDepth = 32;
        if (poDataType)
            *poDataType = GDT_Int32;
    }

    else if (EQUAL(pszDataType, "32BUI")) {
        if (pnBitsDepth)
            *pnBitsDepth = 32;
        if (poDataType)
            *poDataType = GDT_UInt32;
    }

    else if (EQUAL(pszDataType, "32BF")) {
        if (pnBitsDepth)
            *pnBitsDepth = 32;
        if (poDataType)
            *poDataType = GDT_Float32;
    }

    else if (EQUAL(pszDataType, "64BF")) {
        if (pnBitsDepth)
            *pnBitsDepth = 64;
        if (poDataType)
            *poDataType = GDT_Float64;
    }

    else {
        if (pnBitsDepth)
            *pnBitsDepth = -1;
        if (poDataType)
            *poDataType = GDT_Unknown;

        return false;
    }

    return true;
}
