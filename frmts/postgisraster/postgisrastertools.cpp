/***********************************************************************
 * File :    postgisrastertools.cpp
 * Project:  PostGIS Raster driver
 * Purpose:  Tools for PostGIS Raster driver
 * Author:   Jorge Arevalo, jorge.arevalo@deimos-space.com
 *                          jorgearevalo@libregis.org
 *
 * Author:       David Zwarg, dzwarg@azavea.com
 *
 *
 ***********************************************************************
 * Copyright (c) 2009 - 2013, Jorge Arevalo, David Zwarg
 *
 * SPDX-License-Identifier: MIT
 **********************************************************************/
#include "postgisraster.h"

/**********************************************************************
 * \brief Replace the quotes by single quotes in the input string
 *
 * Needed in the 'where' part of the input string
 **********************************************************************/
char *ReplaceQuotes(const char *pszInput, int nLength)
{
    int i;
    char *pszOutput = nullptr;

    if (nLength == -1)
        nLength = static_cast<int>(strlen(pszInput));

    pszOutput = static_cast<char *>(CPLCalloc(nLength + 1, sizeof(char)));

    for (i = 0; i < nLength; i++)
    {
        if (pszInput[i] == '"')
            pszOutput[i] = '\'';
        else
            pszOutput[i] = pszInput[i];
    }

    return pszOutput;
}

/***********************************************************************
 * \brief Translate a PostGIS Raster datatype string in a valid
 * GDALDataType object.
 **********************************************************************/
GBool TranslateDataType(const char *pszDataType,
                        GDALDataType *poDataType = nullptr,
                        int *pnBitsDepth = nullptr)
{
    if (!pszDataType)
        return false;

    if (EQUAL(pszDataType, "1BB"))
    {
        if (pnBitsDepth)
            *pnBitsDepth = 1;
        if (poDataType)
            *poDataType = GDT_Byte;
    }

    else if (EQUAL(pszDataType, "2BUI"))
    {
        if (pnBitsDepth)
            *pnBitsDepth = 2;
        if (poDataType)
            *poDataType = GDT_Byte;
    }

    else if (EQUAL(pszDataType, "4BUI"))
    {
        if (pnBitsDepth)
            *pnBitsDepth = 4;
        if (poDataType)
            *poDataType = GDT_Byte;
    }

    else if (EQUAL(pszDataType, "8BUI"))
    {
        if (pnBitsDepth)
            *pnBitsDepth = 8;
        if (poDataType)
            *poDataType = GDT_Byte;
    }

    else if (EQUAL(pszDataType, "8BSI"))
    {
        if (pnBitsDepth)
            *pnBitsDepth = 8;
        if (poDataType)
            *poDataType = GDT_Int8;
    }
    else if (EQUAL(pszDataType, "16BSI"))
    {
        if (pnBitsDepth)
            *pnBitsDepth = 16;
        if (poDataType)
            *poDataType = GDT_Int16;
    }

    else if (EQUAL(pszDataType, "16BUI"))
    {
        if (pnBitsDepth)
            *pnBitsDepth = 16;
        if (poDataType)
            *poDataType = GDT_UInt16;
    }

    else if (EQUAL(pszDataType, "32BSI"))
    {
        if (pnBitsDepth)
            *pnBitsDepth = 32;
        if (poDataType)
            *poDataType = GDT_Int32;
    }

    else if (EQUAL(pszDataType, "32BUI"))
    {
        if (pnBitsDepth)
            *pnBitsDepth = 32;
        if (poDataType)
            *poDataType = GDT_UInt32;
    }

    else if (EQUAL(pszDataType, "32BF"))
    {
        if (pnBitsDepth)
            *pnBitsDepth = 32;
        if (poDataType)
            *poDataType = GDT_Float32;
    }

    else if (EQUAL(pszDataType, "64BF"))
    {
        if (pnBitsDepth)
            *pnBitsDepth = 64;
        if (poDataType)
            *poDataType = GDT_Float64;
    }

    else
    {
        if (pnBitsDepth)
            *pnBitsDepth = -1;
        if (poDataType)
            *poDataType = GDT_Unknown;

        return false;
    }

    return true;
}
