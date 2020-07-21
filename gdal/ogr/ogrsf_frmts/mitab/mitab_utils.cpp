/**********************************************************************
 *
 * Name:     mitab_utils.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Misc. util. functions for the library
 * Author:   Daniel Morissette, dmorissette@dmsolutions.ca
 *
 **********************************************************************
 * Copyright (c) 1999-2001, Daniel Morissette
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#include "cpl_port.h"
#include "mitab_utils.h"

#include <cctype>
#include <climits>
#include <cmath>
#include <cstring>
#include <limits>

#include "mitab.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"

CPL_CVSID("$Id$")

/**********************************************************************
 *                       TABGenerateArc()
 *
 * Generate the coordinates for an arc and ADD the coordinates to the
 * geometry object.  If the geometry already contains some points then
 * these won't be lost.
 *
 * poLine can be a OGRLineString or one of its derived classes, such as
 *        OGRLinearRing
 * numPoints is the number of points to generate.
 * Angles are specified in radians, valid values are in the range [0..2*PI]
 *
 * Arcs are always generated counterclockwise, even if StartAngle > EndAngle
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABGenerateArc(OGRLineString *poLine, int numPoints,
                   double dCenterX, double dCenterY,
                   double dXRadius, double dYRadius,
                   double dStartAngle, double dEndAngle)
{
    // Adjust angles to go counterclockwise
    if (dEndAngle < dStartAngle)
        dEndAngle += 2.0*M_PI;

    const double dAngleStep = (dEndAngle - dStartAngle) / (numPoints - 1.0);

    double dAngle = 0.0;
    for( int i = 0; i<numPoints; i++ )
    {
        dAngle = dStartAngle + i * dAngleStep;
        const double dX = dCenterX + dXRadius*cos(dAngle);
        const double dY = dCenterY + dYRadius*sin(dAngle);
        poLine->addPoint(dX, dY);
    }

    // Complete the arc with the last EndAngle, to make sure that
    // the arc is correctly closed.
    const double dX = dCenterX + dXRadius*cos(dAngle);
    const double dY = dCenterY + dYRadius*sin(dAngle);
    poLine->addPoint(dX,dY);

    return 0;
}

/**********************************************************************
 *                       TABCloseRing()
 *
 * Check if a ring is closed, and add a point to close it if necessary.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABCloseRing(OGRLineString *poRing)
{
    if ( poRing->getNumPoints() > 0 && !poRing->get_IsClosed() )
    {
        poRing->addPoint(poRing->getX(0), poRing->getY(0));
    }

    return 0;
}

/**********************************************************************
 *                     TABAdjustCaseSensitiveFilename()
 *
 * Scan a filename and its path, adjust uppercase/lowercases if
 * necessary.
 *
 * Returns TRUE if file found, or FALSE if it could not be located with
 * a case-insensitive search.
 *
 * This function works on the original buffer and returns a reference to it.
 * It does nothing on Windows systems where filenames are not case sensitive.
 **********************************************************************/
#ifdef _WIN32
static bool TABAdjustCaseSensitiveFilename(char * /* pszFname */ )
{
    // Nothing to do on Windows.
    return true;
}
#else
// Unix case.
static bool TABAdjustCaseSensitiveFilename(char *pszFname)
{
    VSIStatBufL sStatBuf;

    // First check if the filename is OK as is.
    if (VSIStatL(pszFname, &sStatBuf) == 0)
    {
        return true;
    }

    // File either does not exist or has the wrong cases.
    // Go backwards until we find a portion of the path that is valid.
    char *pszTmpPath = CPLStrdup(pszFname);
    const int nTotalLen = static_cast<int>(strlen(pszTmpPath));
    int iTmpPtr = nTotalLen;
    bool bValidPath = false;

    while(iTmpPtr > 0 && !bValidPath)
    {
        // Move back to the previous '/' separator.
        pszTmpPath[--iTmpPtr] = '\0';
        while( iTmpPtr > 0 && pszTmpPath[iTmpPtr-1] != '/' )
        {
            pszTmpPath[--iTmpPtr] = '\0';
        }

        if (iTmpPtr > 0 && VSIStatL(pszTmpPath, &sStatBuf) == 0)
            bValidPath = true;
    }

    CPLAssert(iTmpPtr >= 0);

    // Assume that CWD is valid.  Therefore an empty path is a valid.
    if (iTmpPtr == 0)
        bValidPath = true;

    // Now that we have a valid base, reconstruct the whole path
    // by scanning all the sub-directories.
    // If we get to a point where a path component does not exist then
    // we simply return the rest of the path as is.
    while(bValidPath && static_cast<int>(strlen(pszTmpPath)) < nTotalLen)
    {
        int iLastPartStart = iTmpPtr;
        char **papszDir = VSIReadDir(pszTmpPath);

        // Add one component to the current path.
        pszTmpPath[iTmpPtr] = pszFname[iTmpPtr];
        iTmpPtr++;
        for( ; pszFname[iTmpPtr] != '\0' && pszFname[iTmpPtr]!='/'; iTmpPtr++)
        {
            pszTmpPath[iTmpPtr] = pszFname[iTmpPtr];
        }

        while(iLastPartStart < iTmpPtr && pszTmpPath[iLastPartStart] == '/')
            iLastPartStart++;

        // And do a case insensitive search in the current dir.
        for(int iEntry = 0; papszDir && papszDir[iEntry]; iEntry++)
        {
            if (EQUAL(pszTmpPath + iLastPartStart, papszDir[iEntry]))
            {
                // Fount it.
                strcpy(pszTmpPath+iLastPartStart, papszDir[iEntry]);
                break;
            }
        }

        if (iTmpPtr > 0 && VSIStatL(pszTmpPath, &sStatBuf) != 0)
            bValidPath = false;

        CSLDestroy(papszDir);
    }

    // We reached the last valid path component... just copy the rest
    // of the path as is.
    if (iTmpPtr < nTotalLen-1)
    {
        strncpy(pszTmpPath+iTmpPtr, pszFname+iTmpPtr, nTotalLen-iTmpPtr);
    }

    // Update the source buffer and return.
    strcpy(pszFname, pszTmpPath);
    CPLFree(pszTmpPath);

    return bValidPath;
}
#endif // Not win32.

/**********************************************************************
 *                       TABAdjustFilenameExtension()
 *
 * Because Unix filenames are case sensitive and MapInfo datasets often have
 * mixed cases filenames, we use this function to find the right filename
 * to use to open a specific file.
 *
 * This function works directly on the source string, so the filename it
 * contains at the end of the call is the one that should be used.
 *
 * Returns TRUE if one of the extensions worked, and FALSE otherwise.
 * If none of the extensions worked then the original extension will NOT be
 * restored.
 **********************************************************************/
GBool TABAdjustFilenameExtension(char *pszFname)
{
    VSIStatBufL  sStatBuf;

    // First try using filename as provided
    if (VSIStatL(pszFname, &sStatBuf) == 0)
    {
        return TRUE;
    }

    // Try using uppercase extension (we assume that fname contains a '.')
    for( int i = static_cast<int>(strlen(pszFname)) - 1;
         i >= 0 && pszFname[i] != '.';
         i-- )
    {
        pszFname[i] = static_cast<char>(toupper(pszFname[i]));
    }

    if (VSIStatL(pszFname, &sStatBuf) == 0)
    {
        return TRUE;
    }

    // Try using lowercase extension.
    for( int i = static_cast<int>(strlen(pszFname))-1;
         i >= 0 && pszFname[i] != '.';
         i-- )
    {
        pszFname[i] = static_cast<char>(tolower(pszFname[i]));
    }

    if (VSIStatL(pszFname, &sStatBuf) == 0)
    {
        return TRUE;
    }

    // None of the extensions worked.
    // Try adjusting cases in the whole path and filename.
    return TABAdjustCaseSensitiveFilename(pszFname);
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
    while ( pszTmp != pszFname
            && *pszTmp != '/' && *pszTmp != '\\' )
        pszTmp--;

    if( pszTmp != pszFname )
        pszTmp++;

    // Now allocate our own copy and remove extension.
    char *pszBasename = CPLStrdup(pszTmp);
    for( int i = static_cast<int>(strlen(pszBasename))-1; i >= 0; i-- )
    {
        if (pszBasename[i] == '.')
        {
            pszBasename[i] = '\0';
            break;
        }
    }

    return pszBasename;
}

/**********************************************************************
 *                       TAB_CSLLoad()
 *
 * Same as CSLLoad(), but does not produce an error if it fails... it
 * just returns NULL silently instead.
 *
 * Load a test file into a stringlist.
 *
 * Lines are limited in length by the size of the CPLReadLine() buffer.
 **********************************************************************/
char **TAB_CSLLoad(const char *pszFname)
{
    CPLStringList oList;

    VSILFILE *fp = VSIFOpenL(pszFname, "rt");

    if( fp )
    {
        while(!VSIFEofL(fp))
        {
            const char *pszLine = nullptr;
            if ( (pszLine = CPLReadLineL(fp)) != nullptr )
            {
                oList.AddString(pszLine);
            }
        }

        VSIFCloseL(fp);
    }

    return oList.StealList();
}

/**********************************************************************
 *                       TABUnEscapeString()
 *
 * Convert a string that can possibly contain escaped "\n" chars in
 * into into a new one with binary newlines in it.
 *
 * Tries to work on the original buffer unless bSrcIsConst=TRUE, in
 * which case the original is always untouched and a copy is allocated
 * ONLY IF NECESSARY.  This means that the caller should compare the
 * return value and the source (pszString) to see if a copy was returned,
 * in which case the caller becomes responsible of freeing both the
 * source and the copy.
 **********************************************************************/
char *TABUnEscapeString(char *pszString, GBool bSrcIsConst)
{
    // First check if we need to do any replacement.
    if (pszString == nullptr || strstr(pszString, "\\n") == nullptr)
    {
        return pszString;
    }

    // Yes, we need to replace at least one "\n".
    // We try to work on the original buffer unless we have bSrcIsConst=TRUE.
    //
    // Note that we do not worry about freeing the source buffer when we
    // return a copy.  It is up to the caller to decide if the source needs
    // to be freed based on context and by comparing pszString with
    // the returned pointer (pszWorkString) to see if they are identical.
    char *pszWorkString = nullptr;
    if (bSrcIsConst)
    {
        // We have to create a copy to work on.
        pszWorkString = static_cast<char *>(
            CPLMalloc(sizeof(char) * (strlen(pszString) +1)));
    }
    else
    {
        // Work on the original.
        pszWorkString = pszString;
    }

    int i = 0;
    int j = 0;
    while (pszString[i])
    {
        if (pszString[i] =='\\' &&
            pszString[i+1] == 'n')
        {
            pszWorkString[j++] = '\n';
            i+= 2;
        }
        else if (pszString[i] =='\\' &&
                 pszString[i+1] == '\\')
        {
            pszWorkString[j++] = '\\';
            i+= 2;
        }
        else
        {
            pszWorkString[j++] = pszString[i++];
        }
    }
    pszWorkString[j++] = '\0';

    return pszWorkString;
}

/**********************************************************************
 *                       TABEscapeString()
 *
 * Convert a string that can possibly contain binary "\n" chars in
 * into into a new one with escaped newlines ("\\" + "n") in it.
 *
 * The function returns the original string pointer if it did not need to
 * be modified, or a copy that has to be freed by the caller if the
 * string had to be modified.
 *
 * It is up to the caller to decide if the returned string needs to be
 * freed by comparing the source (pszString) pointer with the returned
 * pointer (pszWorkString) to see if they are identical.
 **********************************************************************/
char *TABEscapeString(char *pszString)
{
    // First check if we need to do any replacement
    if (pszString == nullptr || strchr(pszString, '\n') == nullptr)
    {
        return pszString;
    }

    // Need to do some replacements.  Alloc a copy big enough
    // to hold the worst possible case.
    char *pszWorkString = static_cast<char *>(CPLMalloc(2*sizeof(char) *
                                                        (strlen(pszString) +1)));

    int i = 0;
    int j = 0;

    while (pszString[i])
    {
        if (pszString[i] =='\n')
        {
            pszWorkString[j++] = '\\';
            pszWorkString[j++] = 'n';
            i++;
        }
        else if (pszString[i] =='\\')
        {
            pszWorkString[j++] = '\\';
            pszWorkString[j++] = '\\';
            i++;
        }
        else
        {
            pszWorkString[j++] = pszString[i++];
        }
    }
    pszWorkString[j++] = '\0';

    return pszWorkString;
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
    for( int i = 0; pszSrcName && pszSrcName[i] != '\0'; i++ )
    {
        if ( pszSrcName[i]=='#' )
        {
            if (i == 0)
            {
                pszNewName[i] = '_';
                numInvalidChars++;
            }
        }
        else if ( !(pszSrcName[i] == '_' ||
                    (i!=0 && pszSrcName[i]>='0' && pszSrcName[i]<='9') ||
                    (pszSrcName[i]>='a' && pszSrcName[i]<='z') ||
                    (pszSrcName[i]>='A' && pszSrcName[i]<='Z') ||
                    static_cast<GByte>(pszSrcName[i])>=192) )
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
 * MapInfo Units string to numeric ID conversion
 **********************************************************************/
typedef struct
{
    int         nUnitId;
    const char *pszAbbrev;
} MapInfoUnitsInfo;

static const MapInfoUnitsInfo gasUnitsList[] =
{
    {0, "mi"},
    {1, "km"},
    {2, "in"},
    {3, "ft"},
    {4, "yd"},
    {5, "mm"},
    {6, "cm"},
    {7, "m"},
    {8, "survey ft"},
    {8, "survey foot"}, // alternate
    {13, nullptr},
    {9, "nmi"},
    {30, "li"},
    {31, "ch"},
    {32, "rd"},
    {-1, nullptr}
};

/**********************************************************************
 *                       TABUnitIdToString()
 *
 * Return the MIF units name for specified units id.
 * Return "" if no match found.
 *
 * The returned string should not be freed by the caller.
 **********************************************************************/
const char *TABUnitIdToString(int nId)
{
    const MapInfoUnitsInfo *psList = gasUnitsList;

    while(psList->nUnitId != -1)
    {
        if (psList->nUnitId == nId)
            return psList->pszAbbrev;
        psList++;
    }

    return "";
}

/**********************************************************************
 *                       TABUnitIdFromString()
 *
 * Return the units ID for specified MIF units name
 *
 * Returns -1 if no match found.
 **********************************************************************/
int TABUnitIdFromString(const char *pszName)
{
    if( pszName == nullptr )
        return 13;

    const MapInfoUnitsInfo *psList = gasUnitsList;

    while(psList->nUnitId != -1)
    {
        if (psList->pszAbbrev != nullptr &&
            EQUAL(psList->pszAbbrev, pszName))
            return psList->nUnitId;
        psList++;
    }

    return -1;
}

/**********************************************************************
 *                       TABSaturatedAdd()
 ***********************************************************************/

void TABSaturatedAdd(GInt32& nVal, GInt32 nAdd)
{
    const GInt32 int_max = std::numeric_limits<GInt32>::max();
    const GInt32 int_min = std::numeric_limits<GInt32>::min();

    if( nAdd >= 0 && nVal > int_max - nAdd )
        nVal = int_max;
    else if( nAdd == int_min && nVal < 0 )
        nVal = int_min;
    else if( nAdd != int_min && nAdd < 0 && nVal < int_min - nAdd )
        nVal = int_min;
    else
        nVal += nAdd;
}

/**********************************************************************
 *                           TABInt16Diff()
 **********************************************************************/

GInt16 TABInt16Diff(int a, int b)
{
    GIntBig nDiff = static_cast<GIntBig>(a) - b;
    // Maybe we should error out instead of saturating ???
    if( nDiff < -32768 )
        return -32768;
    if( nDiff > 32767 )
        return 32767;
    return static_cast<GInt16>(nDiff);
}
