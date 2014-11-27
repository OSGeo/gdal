/**********************************************************************
 * $Id: mitab_utils.cpp,v 1.26 2011-06-16 15:53:12 fwarmerdam Exp $
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
 **********************************************************************
 *
 * $Log: mitab_utils.cpp,v $
 * Revision 1.26  2011-06-16 15:53:12  fwarmerdam
 * improve TABBasename() for filenames with an embedded dot (gdal #4123)
 *
 * Revision 1.25  2010-07-07 19:00:15  aboudreault
 * Cleanup Win32 Compile Warnings (GDAL bug #2930)
 *
 * Revision 1.24  2010-07-05 17:41:07  aboudreault
 * Fixed TABCleanFieldName() function should allow char '#' in field name (bug 2231)
 *
 * Revision 1.23  2010-01-07 20:39:12  aboudreault
 * Added support to handle duplicate field names, Added validation to check if a field name start with a number (bug 2141)
 *
 * Revision 1.22  2008-07-21 16:04:58  dmorissette
 * Fixed const char * warnings with GCC 4.3 (GDAL ticket #2325)
 *
 * Revision 1.21  2006/12/01 16:53:15  dmorissette
 * Wrapped <mbctype.h> stuff with !defined(WIN32CE) (done by mloskot in OGR)
 *
 * Revision 1.20  2005/08/07 21:02:14  fwarmerdam
 * avoid warnings about testing for characters > 255.
 *
 * Revision 1.19  2004/06/30 20:29:04  dmorissette
 * Fixed refs to old address danmo@videotron.ca
 *
 * Revision 1.18  2002/08/28 14:19:22  warmerda
 * fix TABGetBasename() for mixture of path divider types like 'mi/abc\def.tab'
 *
 * Revision 1.17  2001/06/27 19:52:54  warmerda
 * avoid multi byte support if _WIN32 and unix defined for cygwin support
 *
 * Revision 1.16  2001/01/23 21:23:42  daniel
 * Added projection bounds lookup table, called from TABFile::SetProjInfo()
 *
 * Revision 1.15  2001/01/19 06:06:18  daniel
 * Don't filter chars in TABCleanFieldName() if we're on a DBCS system
 *
 * Revision 1.14  2000/09/28 16:39:44  warmerda
 * avoid warnings for unused, and unitialized variables
 *
 * Revision 1.13  2000/09/20 18:35:51  daniel
 * Fixed TABAdjustFilenameExtension() to also handle basename and path
 * using TABAdjustCaseSensitiveFilename()
 *
 * Revision 1.12  2000/04/18 04:19:22  daniel
 * Now accept extended chars with accents in TABCleanFieldName()
 *
 * Revision 1.11  2000/02/28 17:08:56  daniel
 * Avoid using isalnum() in TABCleanFieldName
 *
 * Revision 1.10  2000/02/18 20:46:35  daniel
 * Added TABCleanFieldName()
 *
 * Revision 1.9  2000/01/15 22:30:45  daniel
 * Switch to MIT/X-Consortium OpenSource license
 *
 * Revision 1.8  2000/01/14 23:46:59  daniel
 * Added TABEscapeString()/TABUnEscapeString()
 *
 * Revision 1.7  1999/12/16 06:10:24  daniel
 * TABGetBasename(): make sure last '/' of path is removed
 *
 * Revision 1.6  1999/12/14 02:08:37  daniel
 * Added TABGetBasename() + TAB_CSLLoad()
 *
 * Revision 1.5  1999/11/08 04:30:59  stephane
 * Modify TABGenerateArc()
 *
 * Revision 1.4  1999/09/29 17:59:21  daniel
 * Definition for PI was gone on Windows
 *
 * Revision 1.3  1999/09/16 02:39:17  daniel
 * Completed read support for most feature types
 *
 * Revision 1.2  1999/07/12 05:44:59  daniel
 * Added include math.h for VC++
 *
 * Revision 1.1  1999/07/12 04:18:25  daniel
 * Initial checkin
 *
 **********************************************************************/

#include "mitab.h"
#include "mitab_utils.h"
#include "cpl_conv.h"

#include <math.h>       /* sin()/cos() */
#include <ctype.h>      /* toupper()/tolower() */

#if defined(_WIN32) && !defined(unix) && !defined(WIN32CE)
#  include <mbctype.h>  /* Multibyte chars stuff */
#endif


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
    double dX, dY, dAngleStep, dAngle=0.0;
    int i;

    // Adjust angles to go counterclockwise
    if (dEndAngle < dStartAngle)
        dEndAngle += 2.0*PI;

    dAngleStep = (dEndAngle-dStartAngle)/(numPoints-1.0);

    for(i=0; i<numPoints; i++)
    {
        dAngle = (dStartAngle + (double)i*dAngleStep);
        dX = dCenterX + dXRadius*cos(dAngle);
        dY = dCenterY + dYRadius*sin(dAngle);
        poLine->addPoint(dX, dY);
    }

    // Complete the arc with the last EndAngle, to make sure that 
    // the arc is correcly close.

    dX = dCenterX + dXRadius*cos(dAngle);
    dY = dCenterY + dYRadius*sin(dAngle);
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
GBool TABAdjustCaseSensitiveFilename(char *pszFname)
{

#ifdef _WIN32
    /*-----------------------------------------------------------------
     * Nothing to do on Windows
     *----------------------------------------------------------------*/
    return TRUE;

#else
    /*-----------------------------------------------------------------
     * Unix case.
     *----------------------------------------------------------------*/
    VSIStatBufL  sStatBuf;
    char        *pszTmpPath = NULL;
    int         nTotalLen, iTmpPtr;
    GBool       bValidPath;

    /*-----------------------------------------------------------------
     * First check if the filename is OK as is.
     *----------------------------------------------------------------*/
    if (VSIStatL(pszFname, &sStatBuf) == 0)
    {
        return TRUE;
    }

    /*-----------------------------------------------------------------
     * OK, file either does not exist or has the wrong cases... we'll
     * go backwards until we find a portion of the path that is valid.
     *----------------------------------------------------------------*/
    pszTmpPath = CPLStrdup(pszFname);
    nTotalLen = strlen(pszTmpPath);
    iTmpPtr = nTotalLen;
    bValidPath = FALSE;

    while(iTmpPtr > 0 && !bValidPath)
    {
        /*-------------------------------------------------------------
         * Move back to the previous '/' separator
         *------------------------------------------------------------*/
        pszTmpPath[--iTmpPtr] = '\0';
        while( iTmpPtr > 0 && pszTmpPath[iTmpPtr-1] != '/' )
        {
            pszTmpPath[--iTmpPtr] = '\0';
        }

        if (iTmpPtr > 0 && VSIStatL(pszTmpPath, &sStatBuf) == 0)
            bValidPath = TRUE;
    }

    CPLAssert(iTmpPtr >= 0);

    /*-----------------------------------------------------------------
     * Assume that CWD is valid... so an empty path is a valid path
     *----------------------------------------------------------------*/
    if (iTmpPtr == 0)
        bValidPath = TRUE;

    /*-----------------------------------------------------------------
     * OK, now that we have a valid base, reconstruct the whole path
     * by scanning all the sub-directories.  
     * If we get to a point where a path component does not exist then
     * we simply return the rest of the path as is.
     *----------------------------------------------------------------*/
    while(bValidPath && (int)strlen(pszTmpPath) < nTotalLen)
    {
        char    **papszDir=NULL;
        int     iEntry, iLastPartStart;

        iLastPartStart = iTmpPtr;
        papszDir = CPLReadDir(pszTmpPath);

        /*-------------------------------------------------------------
         * Add one component to the current path
         *------------------------------------------------------------*/
        pszTmpPath[iTmpPtr] = pszFname[iTmpPtr];
        iTmpPtr++;
        for( ; pszFname[iTmpPtr] != '\0' && pszFname[iTmpPtr]!='/'; iTmpPtr++)
        {
            pszTmpPath[iTmpPtr] = pszFname[iTmpPtr];
        }

        while(iLastPartStart < iTmpPtr && pszTmpPath[iLastPartStart] == '/')
            iLastPartStart++;

        /*-------------------------------------------------------------
         * And do a case insensitive search in the current dir...
         *------------------------------------------------------------*/
        for(iEntry=0; papszDir && papszDir[iEntry]; iEntry++)
        {
            if (EQUAL(pszTmpPath+iLastPartStart, papszDir[iEntry]))
            {
                /* Fount it! */
                strcpy(pszTmpPath+iLastPartStart, papszDir[iEntry]);
                break;
            }
        }

        if (iTmpPtr > 0 && VSIStatL(pszTmpPath, &sStatBuf) != 0)
            bValidPath = FALSE;

        CSLDestroy(papszDir);
    }

    /*-----------------------------------------------------------------
     * We reached the last valid path component... just copy the rest
     * of the path as is.
     *----------------------------------------------------------------*/
    if (iTmpPtr < nTotalLen-1)
    {
        strncpy(pszTmpPath+iTmpPtr, pszFname+iTmpPtr, nTotalLen-iTmpPtr);
    }

    /*-----------------------------------------------------------------
     * Update the source buffer and return.
     *----------------------------------------------------------------*/
    strcpy(pszFname, pszTmpPath);
    CPLFree(pszTmpPath);

    return bValidPath;

#endif
}




/**********************************************************************
 *                       TABAdjustFilenameExtension()
 *
 * Because Unix filenames are case sensitive and MapInfo datasets often have
 * mixed cases filenames, we use this function to find the right filename
 * to use ot open a specific file.
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
    int         i;
    
    /*-----------------------------------------------------------------
     * First try using filename as provided
     *----------------------------------------------------------------*/
    if (VSIStatL(pszFname, &sStatBuf) == 0)
    {
        return TRUE;
    }     

    /*-----------------------------------------------------------------
     * Try using uppercase extension (we assume that fname contains a '.')
     *----------------------------------------------------------------*/
    for(i = strlen(pszFname)-1; i >= 0 && pszFname[i] != '.'; i--)
    {
        pszFname[i] = (char)toupper(pszFname[i]);
    }

    if (VSIStatL(pszFname, &sStatBuf) == 0)
    {
        return TRUE;
    }     
    
    /*-----------------------------------------------------------------
     * Try using lowercase extension
     *----------------------------------------------------------------*/
    for(i = strlen(pszFname)-1; i >= 0 && pszFname[i] != '.'; i--)
    {
        pszFname[i] = (char)tolower(pszFname[i]);
    }

    if (VSIStatL(pszFname, &sStatBuf) == 0)
    {
        return TRUE;
    }     

    /*-----------------------------------------------------------------
     * None of the extensions worked!  
     * Try adjusting cases in the whole path and filename 
     *----------------------------------------------------------------*/
    return TABAdjustCaseSensitiveFilename(pszFname);
}



/**********************************************************************
 *                       TABGetBasename()
 *
 * Extract the basename part of a complete file path.
 *
 * Returns a newly allocated string without the leading path (dirs) and
 * the extenstion.  The returned string should be freed using CPLFree().
 **********************************************************************/
char *TABGetBasename(const char *pszFname)
{
    const char *pszTmp = NULL;

    /*-----------------------------------------------------------------
     * Skip leading path or use whole name if no path dividers are
     * encountered.
     *----------------------------------------------------------------*/
    pszTmp = pszFname + strlen(pszFname) - 1;
    while ( pszTmp != pszFname 
            && *pszTmp != '/' && *pszTmp != '\\' ) 
        pszTmp--;

    if( pszTmp != pszFname )
        pszTmp++;

    /*-----------------------------------------------------------------
     * Now allocate our own copy and remove extension
     *----------------------------------------------------------------*/
    char *pszBasename = CPLStrdup(pszTmp);
    int i;
    for(i=strlen(pszBasename)-1; i >= 0; i-- )
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
    VSILFILE    *fp;
    const char  *pszLine;
    char        **papszStrList=NULL;

    fp = VSIFOpenL(pszFname, "rt");

    if (fp)
    {
        while(!VSIFEofL(fp))
        {
            if ( (pszLine = CPLReadLineL(fp)) != NULL )
            {
                papszStrList = CSLAddString(papszStrList, pszLine);
            }
        }

        VSIFCloseL(fp);
    }

    return papszStrList;
}



/**********************************************************************
 *                       TABUnEscapeString()
 *
 * Convert a string that can possibly contain escaped "\n" chars in
 * into into a new one with binary newlines in it.
 *
 * Tries to work on hte original buffer unless bSrcIsConst=TRUE, in
 * which case the original is always untouched and a copy is allocated
 * ONLY IF NECESSARY.  This means that the caller should compare the
 * return value and the source (pszString) to see if a copy was returned,
 * in which case the caller becomes responsible of freeing both the
 * source and the copy.
 **********************************************************************/
char *TABUnEscapeString(char *pszString, GBool bSrcIsConst)
{

    /*-----------------------------------------------------------------
     * First check if we need to do any replacement
     *----------------------------------------------------------------*/
    if (pszString == NULL || strstr(pszString, "\\n") == NULL)
    {
        return pszString;
    }

    /*-----------------------------------------------------------------
     * Yes, we need to replace at least one "\n"
     * We try to work on the original buffer unless we have bSrcIsConst=TRUE
     *
     * Note that we do not worry about freeing the source buffer when we
     * return a copy... it is up to the caller to decide if he needs to 
     * free the source based on context and by comparing pszString with 
     * the returned pointer (pszWorkString) to see if they are identical.
     *----------------------------------------------------------------*/
    char *pszWorkString = NULL;
    int i =0;
    int j =0;

    if (bSrcIsConst)
    {
        // We have to create a copy to work on.
        pszWorkString = (char *)CPLMalloc(sizeof(char) * 
                                          (strlen(pszString) +1));
    }
    else
    {
        // We'll work on the original.
        pszWorkString = pszString;
    }


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
 * It is up to the caller to decide if he needs to free the returned 
 * string by comparing the source (pszString) pointer with the returned
 * pointer (pszWorkString) to see if they are identical.
 **********************************************************************/
char *TABEscapeString(char *pszString)
{
    /*-----------------------------------------------------------------
     * First check if we need to do any replacement
     *----------------------------------------------------------------*/
    if (pszString == NULL || strchr(pszString, '\n') == NULL)
    {
        return pszString;
    }

    /*-----------------------------------------------------------------
     * OK, we need to do some replacements... alloc a copy big enough
     * to hold the worst possible case
     *----------------------------------------------------------------*/
    char *pszWorkString = (char *)CPLMalloc(2*sizeof(char) * 
                                            (strlen(pszString) +1));

    int i =0;
    int j =0;

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
    char *pszNewName;
    int numInvalidChars = 0;

    pszNewName = CPLStrdup(pszSrcName);

    if (strlen(pszNewName) > 31)
    {
        pszNewName[31] = '\0';
        CPLError(CE_Warning, TAB_WarningInvalidFieldName,
                 "Field name '%s' is longer than the max of 31 characters. "
                 "'%s' will be used instead.", pszSrcName, pszNewName);
    }

#if defined(_WIN32) && !defined(unix) && !defined(WIN32CE)
    /*-----------------------------------------------------------------
     * On Windows, check if we're using a double-byte codepage, and
     * if so then just keep the field name as is... 
     *----------------------------------------------------------------*/
    if (_getmbcp() != 0)
        return pszNewName;
#endif

    /*-----------------------------------------------------------------
     * According to the MapInfo User's Guide (p. 240, v5.5)
     * New Table Command:
     *  Name:
     * Displays the field name in the name box. You can also enter new field
     * names here. Defaults are Field1, Field2, etc. A field name can contain
     * up to 31 alphanumeric characters. Use letters, numbers, and the 
     * underscore. Do not use spaces; instead, use the underscore character
     * (_) to separate words in a field name. Use upper and lower case for
     * legibility, but MapInfo is not case-sensitive.
     *
     * It was also verified that extended chars with accents are also 
     * accepted.
     *----------------------------------------------------------------*/
    for(int i=0; pszSrcName && pszSrcName[i] != '\0'; i++)
    {
        if ( pszSrcName[i]=='#' )
	{
            if (i == 0)
            {
                pszNewName[i] = '_';
                numInvalidChars++;
            }
        }
        else if ( !( pszSrcName[i] == '_' ||
                     (i!=0 && pszSrcName[i]>='0' && pszSrcName[i]<='9') || 
                     (pszSrcName[i]>='a' && pszSrcName[i]<='z') || 
                     (pszSrcName[i]>='A' && pszSrcName[i]<='Z') ||
                     (GByte)pszSrcName[i]>=192 ) )
        {
            pszNewName[i] = '_';
            numInvalidChars++;
        }
    }

    if (numInvalidChars > 0)
    {
        CPLError(CE_Warning, TAB_WarningInvalidFieldName,
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

static MapInfoUnitsInfo gasUnitsList[] = 
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
    {13, NULL},
    {9, "nmi"},
    {30, "li"},
    {31, "ch"},
    {32, "rd"},
    {-1, NULL}
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
    MapInfoUnitsInfo *psList;

    psList = gasUnitsList;

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
    MapInfoUnitsInfo *psList;

    psList = gasUnitsList;
    
    if( pszName == NULL )
        return 13;

    while(psList->nUnitId != -1)
    {
        if (psList->pszAbbrev != NULL &&
            EQUAL(psList->pszAbbrev, pszName)) 
            return psList->nUnitId;
        psList++;
    }

    return -1;
}

