/**********************************************************************
 * $Id: mitab_middatafile.cpp,v 1.15 2010-10-12 19:02:40 aboudreault Exp $
 *
 * Name:     mitab_datfile.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation of the MIDDATAFile class used to handle
 *           reading/writing of the MID/MIF files
 * Author:   Stephane Villeneuve, stephane.v@videotron.ca
 *
 **********************************************************************
 * Copyright (c) 1999, 2000, Stephane Villeneuve
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
 * $Log: mitab_middatafile.cpp,v $
 * Revision 1.15  2010-10-12 19:02:40  aboudreault
 * Fixed incomplete patch to handle differently indented lines in MIF
 * files (gdal #3694).
 *
 * Revision 1.14  2006-01-27 13:54:06  fwarmerdam
 * fixed memory leak
 *
 * Revision 1.13  2005/10/04 19:36:10  dmorissette
 * Added support for reading collections from MIF files (bug 1126)
 *
 * Revision 1.12  2005/09/29 19:46:55  dmorissette
 * Use "\t" as default delimiter in constructor (Anthony D - bugs 1155 and 37)
 *
 * Revision 1.11  2004/05/20 13:50:06  fwarmerdam
 * Call CPLReadLine(NULL) in Close() method to clean up working buffer.
 *
 * Revision 1.10  2002/04/26 14:16:49  julien
 * Finishing the implementation of Multipoint (support for MIF)
 *
 * Revision 1.9  2002/04/24 18:37:39  daniel
 * Added return statement at end of GetLastLine()
 *
 * Revision 1.8  2002/04/22 13:49:09  julien
 * Add EOF validation in MIDDATAFile::GetLastLine() (Bug 819)
 *
 * Revision 1.7  2001/09/19 14:49:49  warmerda
 * use VSIRewind() instead of rewind()
 *
 * Revision 1.6  2001/01/22 16:03:58  warmerda
 * expanded tabs
 *
 * Revision 1.5  2000/01/15 22:30:44  daniel
 * Switch to MIT/X-Consortium OpenSource license
 *
 * Revision 1.4  1999/12/19 17:41:29  daniel
 * Fixed a memory leak
 *
 * Revision 1.3  1999/11/14 17:43:32  stephane
 * Add ifdef to remove CPLError if OGR is define
 *
 * Revision 1.2  1999/11/11 01:22:05  stephane
 * Remove DebugFeature call, Point Reading error, add IsValidFeature() to
 * test correctly if we are on a feature
 *
 * Revision 1.1  1999/11/08 04:16:07  stephane
 * First Revision
 *
 *
 **********************************************************************/

#include "mitab.h"

/*=====================================================================
 *                      class MIDDATAFile
 *
 *====================================================================*/

MIDDATAFile::MIDDATAFile()
{
    m_fp = NULL;
    m_szLastRead[0] = '\0';
    m_szSavedLine[0] = '\0';
    m_pszDelimiter = "\t"; // Encom 2003 (was NULL)

    m_dfXMultiplier = 1.0;
    m_dfYMultiplier = 1.0;
    m_dfXDisplacement = 0.0;
    m_dfYDisplacement = 0.0;
    m_pszFname = NULL;
    m_eAccessMode = TABRead;
    m_bEof = FALSE;
}

MIDDATAFile::~MIDDATAFile()
{
    Close();
}

void MIDDATAFile::SaveLine(const char *pszLine)
{
    if (pszLine == NULL)
    {
        m_szSavedLine[0] = '\0';
    }
    else
    {
        CPLStrlcpy(m_szSavedLine,pszLine,MIDMAXCHAR);
    }
}

const char *MIDDATAFile::GetSavedLine()
{
    return m_szSavedLine;
}

int MIDDATAFile::Open(const char *pszFname, const char *pszAccess)
{
   if (m_fp)
   {
       return -1;
   }

    /*-----------------------------------------------------------------
     * Validate access mode and make sure we use Text access.
     *----------------------------------------------------------------*/
    if (STARTS_WITH_CI(pszAccess, "r"))
    {
        m_eAccessMode = TABRead;
        pszAccess = "rt";
    }
    else if (STARTS_WITH_CI(pszAccess, "w"))
    {
        m_eAccessMode = TABWrite;
        pszAccess = "wt";
    }
    else
    {
        return -1;
    }

    /*-----------------------------------------------------------------
     * Open file for reading
     *----------------------------------------------------------------*/
    m_pszFname = CPLStrdup(pszFname);
    m_fp = VSIFOpenL(m_pszFname, pszAccess);

    if (m_fp == NULL)
    {
        CPLFree(m_pszFname);
        m_pszFname = NULL;
        return -1;
    }

    SetEof(FALSE);
    return 0;
}

int MIDDATAFile::Rewind()
{
    if (m_fp == NULL || m_eAccessMode == TABWrite)
        return -1;

    else
    {
        VSIRewindL(m_fp);
        SetEof(FALSE);
    }
    return 0;
}

int MIDDATAFile::Close()
{
    if (m_fp == NULL)
        return 0;

    // Close file
    VSIFCloseL(m_fp);
    m_fp = NULL;

    // clear readline buffer.
    CPLReadLineL( NULL );

    CPLFree(m_pszFname);
    m_pszFname = NULL;

    return 0;

}

const char *MIDDATAFile::GetLine()
{
    const char *pszLine;

    if (m_eAccessMode == TABRead)
    {

        pszLine = CPLReadLineL(m_fp);

        if (pszLine == NULL)
        {
            SetEof(TRUE);
            m_szLastRead[0] = '\0';
        }
        else
        {
            // skip leading spaces and tabs (except is the delimiter is tab)
            while(pszLine && (*pszLine == ' ' || (*m_pszDelimiter != '\t' && *pszLine == '\t')) )
                    pszLine++;

            CPLStrlcpy(m_szLastRead,pszLine,MIDMAXCHAR);
        }
        //if (pszLine)
        //  printf("%s\n",pszLine);
        return pszLine;
    }
    else
    {
      CPLAssert(FALSE);
    }
    return NULL;
}

const char *MIDDATAFile::GetLastLine()
{
    // Return NULL if EOF
    if(GetEof())
    {
        return NULL;
    }
    else if (m_eAccessMode == TABRead)
    {
        // printf("%s\n",m_szLastRead);
        return m_szLastRead;
    }

    // We should never get here (Read/Write mode not implemented)
    CPLAssert(FALSE);
    return NULL;
}

void MIDDATAFile::WriteLine(const char *pszFormat,...)
{
    va_list args;

    if (m_eAccessMode == TABWrite  && m_fp)
    {
        va_start(args, pszFormat);
        CPLString osStr;
        osStr.vPrintf( pszFormat, args );
        VSIFWriteL( osStr.c_str(), 1, osStr.size(), m_fp);
        va_end(args);
    }
    else
    {
        CPLAssert(FALSE);
    }
}


void MIDDATAFile::SetTranslation(double dfXMul,double dfYMul,
                                 double dfXTran,
                                 double dfYTran)
{
    m_dfXMultiplier = dfXMul;
    m_dfYMultiplier = dfYMul;
    m_dfXDisplacement = dfXTran;
    m_dfYDisplacement = dfYTran;
}

double MIDDATAFile::GetXTrans(double dfX)
{
    return (dfX * m_dfXMultiplier) + m_dfXDisplacement;
}

double MIDDATAFile::GetYTrans(double dfY)
{
    return (dfY * m_dfYMultiplier) + m_dfYDisplacement;
}


GBool MIDDATAFile::IsValidFeature(const char *pszString)
{
    char **papszToken ;

    papszToken = CSLTokenizeString(pszString);

    //   printf("%s\n",pszString);

    if (CSLCount(papszToken) == 0)
    {
        CSLDestroy(papszToken);
        return FALSE;
    }

    if (EQUAL(papszToken[0],"NONE")      || EQUAL(papszToken[0],"POINT") ||
        EQUAL(papszToken[0],"LINE")      || EQUAL(papszToken[0],"PLINE") ||
        EQUAL(papszToken[0],"REGION")    || EQUAL(papszToken[0],"ARC") ||
        EQUAL(papszToken[0],"TEXT")      || EQUAL(papszToken[0],"RECT") ||
        EQUAL(papszToken[0],"ROUNDRECT") || EQUAL(papszToken[0],"ELLIPSE") ||
        EQUAL(papszToken[0],"MULTIPOINT")|| EQUAL(papszToken[0],"COLLECTION") )
    {
        CSLDestroy(papszToken);
        return TRUE;
    }

    CSLDestroy(papszToken);
    return FALSE;

}


GBool MIDDATAFile::GetEof()
{
    return m_bEof;
}


void MIDDATAFile::SetEof(GBool bEof)
{
    m_bEof = bEof;
}
