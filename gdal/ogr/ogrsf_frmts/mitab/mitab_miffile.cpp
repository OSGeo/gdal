/**********************************************************************
 *
 * Name:     mitab_miffile.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation of the MIDFile class.
 *           To be used by external programs to handle reading/writing of
 *           features from/to MID/MIF datasets.
 * Author:   Stephane Villeneuve, stephane.v@videotron.ca
 *
 **********************************************************************
 * Copyright (c) 1999-2003, Stephane Villeneuve
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
 * Copyright (c) 2014, Even Rouault <even.rouault at spatialys.com>
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
#include "mitab.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "mitab_priv.h"
#include "mitab_utils.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_spatialref.h"
#include "ogrsf_frmts.h"

CPL_CVSID("$Id$");

/*=====================================================================
 *                      class MIFFile
 *====================================================================*/

/**********************************************************************
 *                   MIFFile::MIFFile()
 *
 * Constructor.
 **********************************************************************/
MIFFile::MIFFile() :
    m_pszFname(NULL),
    m_eAccessMode(TABRead),
    m_nVersion(300),
    // Tab is default delimiter in MIF spec if not explicitly specified.  Use
    // that by default for read mode. In write mode, we will use "," as
    // delimiter since it's more common than tab (we do this in Open())
    m_pszDelimiter(CPLStrdup("\t")),
    m_pszUnique(NULL),
    m_pszIndex(NULL),
    m_pszCoordSys(NULL),
    m_paeFieldType(NULL),
    m_pabFieldIndexed(NULL),
    m_pabFieldUnique(NULL),
    m_dfXMultiplier(1.0),
    m_dfYMultiplier(1.0),
    m_dfXDisplacement(0.0),
    m_dfYDisplacement(0.0),
    m_dXMin(0),
    m_dYMin(0),
    m_dXMax(0),
    m_dYMax(0),
    m_bExtentsSet(FALSE),
    m_nPoints(0),
    m_nLines(0),
    m_nRegions(0),
    m_nTexts(0),
    m_nPreloadedId(0),
    m_poMIDFile(NULL),
    m_poMIFFile(NULL),
    m_poDefn(NULL),
    m_poSpatialRef(NULL),
    m_nFeatureCount(0),
    m_nWriteFeatureId(-1),
    m_nAttribute(0),
    m_bPreParsed(FALSE),
    m_bHeaderWrote(FALSE)
{
    m_nCurFeatureId = 0;
    m_poCurFeature = NULL;
}

/**********************************************************************
 *                   MIFFile::~MIFFile()
 *
 * Destructor.
 **********************************************************************/
MIFFile::~MIFFile()
{
    Close();
}

/**********************************************************************
 *                   MIFFile::Open()
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int MIFFile::Open(const char *pszFname, TABAccess eAccess,
                  GBool bTestOpenNoError /*=FALSE*/ )
{
    char *pszTmpFname = NULL;
    int nFnameLen = 0;

    CPLErrorReset();

    if (m_poMIFFile)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                     "Open() failed: object already contains an open file");

        return -1;
    }

    /*-----------------------------------------------------------------
     * Validate access mode
     *----------------------------------------------------------------*/
    const char* pszAccess = NULL;
    if (eAccess == TABRead)
    {
        m_eAccessMode = TABRead;
        pszAccess = "rt";
    }
    else if (eAccess == TABWrite)
    {
        m_eAccessMode = TABWrite;
        pszAccess = "wt";

        // In write mode, use "," as delimiter since it's more common than tab
        CPLFree(m_pszDelimiter);
        m_pszDelimiter = CPLStrdup(",");
    }
    else
    {
        if (!bTestOpenNoError)
            CPLError(CE_Failure, CPLE_FileIO,
                 "Open() failed: access mode \"%d\" not supported", eAccess);
        else
            CPLErrorReset();

        return -1;
    }

    /*-----------------------------------------------------------------
     * Make sure filename has a .MIF or .MID extension...
     *----------------------------------------------------------------*/
    m_pszFname = CPLStrdup(pszFname);
    nFnameLen = static_cast<int>(strlen(m_pszFname));
    if (nFnameLen > 4 && (strcmp(m_pszFname+nFnameLen-4, ".MID")==0 ||
                     strcmp(m_pszFname+nFnameLen-4, ".MIF")==0 ) )
        strcpy(m_pszFname+nFnameLen-4, ".MIF");
    else if (nFnameLen > 4 && (EQUAL(m_pszFname+nFnameLen-4, ".mid") ||
                               EQUAL(m_pszFname+nFnameLen-4, ".mif") ) )
        strcpy(m_pszFname+nFnameLen-4, ".mif");
    else
    {
        if (!bTestOpenNoError)
            CPLError(CE_Failure, CPLE_FileIO,
                     "Open() failed for %s: invalid filename extension",
                     m_pszFname);
        else
            CPLErrorReset();

        return -1;
    }

    pszTmpFname = CPLStrdup(m_pszFname);

    /*-----------------------------------------------------------------
     * Open .MIF file
     *----------------------------------------------------------------*/

#ifndef _WIN32
    /*-----------------------------------------------------------------
     * On Unix, make sure extension uses the right cases
     * We do it even for write access because if a file with the same
     * extension already exists we want to overwrite it.
     *----------------------------------------------------------------*/
    TABAdjustFilenameExtension(pszTmpFname);
#endif

    m_poMIFFile = new MIDDATAFile;

    if (m_poMIFFile->Open(pszTmpFname, pszAccess) != 0)
    {
        if (!bTestOpenNoError)
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unable to open %s.", pszTmpFname);
        else
            CPLErrorReset();

        CPLFree(pszTmpFname);
        Close();

        return -1;
    }

    /*-----------------------------------------------------------------
     * Read MIF File Header
     *----------------------------------------------------------------*/
    int bIsEmpty = FALSE;
    if (m_eAccessMode == TABRead && ParseMIFHeader(&bIsEmpty) != 0)
    {
        Close();

        if (!bTestOpenNoError)
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Failed parsing header in %s.", m_pszFname);
        else
            CPLErrorReset();

        CPLFree(pszTmpFname);

        return -1;
    }

    if ( m_nAttribute > 0 || m_eAccessMode == TABWrite )
    {
        /*-----------------------------------------------------------------
        * Open .MID file
        *----------------------------------------------------------------*/
        if (nFnameLen > 4 && strcmp(pszTmpFname+nFnameLen-4, ".MIF")==0)
            strcpy(pszTmpFname+nFnameLen-4, ".MID");
        else
            strcpy(pszTmpFname+nFnameLen-4, ".mid");

#ifndef _WIN32
        TABAdjustFilenameExtension(pszTmpFname);
#endif

        m_poMIDFile = new MIDDATAFile;

        if (m_poMIDFile->Open(pszTmpFname, pszAccess) !=0)
        {
            if (m_eAccessMode == TABWrite)
            {
                if (!bTestOpenNoError)
                    CPLError(CE_Failure, CPLE_NotSupported,
                            "Unable to open %s.", pszTmpFname);
                else
                    CPLErrorReset();

                CPLFree(pszTmpFname);
                Close();

                return -1;
            }
            else
            {
                CPLDebug("MITAB",
                         "%s is not found, although %d attributes are declared",
                         pszTmpFname, m_nAttribute);
                delete m_poMIDFile;
                m_poMIDFile = NULL;
            }
        }
    }

    CPLFree(pszTmpFname);
    pszTmpFname = NULL;

    /*-----------------------------------------------------------------
     * In write access, set some defaults
     *----------------------------------------------------------------*/
    if (m_eAccessMode == TABWrite)
    {
        m_nVersion = 300;
        m_pszCharset = CPLStrdup("Neutral");
    }

    /* Put the MID file at the correct location, on the first feature */
    if (m_eAccessMode == TABRead && (m_poMIDFile != NULL && !bIsEmpty && m_poMIDFile->GetLine() == NULL))
    {
        Close();

        if (bTestOpenNoError)
            CPLErrorReset();

        return -1;
    }

    m_poMIFFile->SetTranslation(m_dfXMultiplier,m_dfYMultiplier,
                                m_dfXDisplacement, m_dfYDisplacement);
    if( m_poMIDFile != NULL )
        m_poMIDFile->SetTranslation(m_dfXMultiplier,m_dfYMultiplier,
                                    m_dfXDisplacement, m_dfYDisplacement);
    m_poMIFFile->SetDelimiter(m_pszDelimiter);
    if( m_poMIDFile != NULL )
        m_poMIDFile->SetDelimiter(m_pszDelimiter);

    /*-------------------------------------------------------------
     * Set geometry type if the geometry objects are uniform.
     *------------------------------------------------------------*/
    int numPoints=0, numRegions=0, numTexts=0, numLines=0;

    if( GetFeatureCountByType( numPoints, numLines, numRegions, numTexts,
                               FALSE ) == 0 )
    {
        numPoints += numTexts;
        if( numPoints > 0 && numLines == 0 && numRegions == 0 )
            m_poDefn->SetGeomType( wkbPoint );
        else if( numPoints == 0 && numLines > 0 && numRegions == 0 )
            m_poDefn->SetGeomType( wkbLineString );
        else
        {
            /* we leave it unknown indicating a mixture */
        }
    }

    /* A newly created layer should have OGRFeatureDefn */
    if (m_poDefn == NULL)
    {
        char *pszFeatureClassName = TABGetBasename(m_pszFname);
        m_poDefn = new OGRFeatureDefn(pszFeatureClassName);
        CPLFree(pszFeatureClassName);
        // Ref count defaults to 0... set it to 1
        m_poDefn->Reference();
    }

    return 0;
}

/**********************************************************************
 *                   MIFFile::ParseMIFHeader()
 *
 * Scan the header of a MIF file, and store any useful information into
 * class members.  The main piece of information being the fields
 * definition that we use to build the OGRFeatureDefn for this file.
 *
 * This private method should be used only during the Open() call.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int MIFFile::ParseMIFHeader(int* pbIsEmpty)
{
    *pbIsEmpty = FALSE;

    char *pszFeatureClassName = TABGetBasename(m_pszFname);
    m_poDefn = new OGRFeatureDefn(pszFeatureClassName);
    CPLFree(pszFeatureClassName);
    // Ref count defaults to 0... set it to 1
    m_poDefn->Reference();

    if (m_eAccessMode != TABRead)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "ParseMIDFile() can be used only with Read access.");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Parse header until we find the "Data" line
     *----------------------------------------------------------------*/
    char **papszToken = NULL;
    GBool bColumns = FALSE;
    GBool bAllColumnsRead =  FALSE;
    int nColumns = 0;
    GBool bCoordSys = FALSE;

    const char *pszLine = NULL;
    while (((pszLine = m_poMIFFile->GetLine()) != NULL) &&
           ((bAllColumnsRead == FALSE) || !STARTS_WITH_CI(pszLine, "Data")))
    {
        if (bColumns == TRUE && nColumns >0)
        {
            if (AddFields(pszLine) == 0)
            {
                nColumns--;
                if (nColumns == 0)
                {
                  bAllColumnsRead = TRUE;
                  bColumns = FALSE;
                }
            }
            else
            {
                bColumns = FALSE;
            }
        }
        else if (STARTS_WITH_CI(pszLine, "VERSION"))
        {
            papszToken = CSLTokenizeStringComplex(pszLine," ()\t",TRUE,FALSE);
            bColumns = FALSE; bCoordSys = FALSE;
            if (CSLCount(papszToken)  == 2)
              m_nVersion = atoi(papszToken[1]);

            CSLDestroy(papszToken);
        }
        else if (STARTS_WITH_CI(pszLine, "CHARSET"))
        {
            papszToken = CSLTokenizeStringComplex(pszLine," ()\t",TRUE,FALSE);
            bColumns = FALSE; bCoordSys = FALSE;

            if (CSLCount(papszToken)  == 2)
            {
                CPLFree(m_pszCharset);
                m_pszCharset = CPLStrdup(papszToken[1]);
            }
            CSLDestroy(papszToken);
        }
        else if (STARTS_WITH_CI(pszLine, "DELIMITER"))
        {
            papszToken = CSLTokenizeStringComplex(pszLine," ()\t",TRUE,FALSE);
             bColumns = FALSE; bCoordSys = FALSE;

           if (CSLCount(papszToken)  == 2)
           {
               CPLFree(m_pszDelimiter);
               m_pszDelimiter = CPLStrdup(papszToken[1]);
           }
          CSLDestroy(papszToken);
        }
        else if (m_pszUnique == NULL &&
                 STARTS_WITH_CI(pszLine, "UNIQUE"))
        {
            bColumns = FALSE; bCoordSys = FALSE;

            m_pszUnique = CPLStrdup(pszLine + 6);
        }
        else if (m_pszIndex == NULL &&
                 STARTS_WITH_CI(pszLine, "INDEX"))
        {
            bColumns = FALSE; bCoordSys = FALSE;

            m_pszIndex = CPLStrdup(pszLine + 5);
        }
        else if (m_pszCoordSys == NULL &&
                 STARTS_WITH_CI(pszLine, "COORDSYS") &&
                 CPLStrnlen(pszLine, 9) >= 9)
        {
            bCoordSys = TRUE;
            m_pszCoordSys = CPLStrdup(pszLine + 9);

            // Extract bounds if present
            char  **papszFields =
                CSLTokenizeStringComplex(m_pszCoordSys, " ,()\t", TRUE, FALSE );
            int iBounds = CSLFindString( papszFields, "Bounds" );
            if (iBounds >= 0 && iBounds + 4 < CSLCount(papszFields))
            {
                m_dXMin = CPLAtof(papszFields[++iBounds]);
                m_dYMin = CPLAtof(papszFields[++iBounds]);
                m_dXMax = CPLAtof(papszFields[++iBounds]);
                m_dYMax = CPLAtof(papszFields[++iBounds]);
                m_bBoundsSet = TRUE;
            }
            CSLDestroy( papszFields );
        }
        else if (STARTS_WITH_CI(pszLine, "TRANSFORM"))
        {
            papszToken = CSLTokenizeStringComplex(pszLine," ,\t",TRUE,FALSE);
            bColumns = FALSE; bCoordSys = FALSE;

            if (CSLCount(papszToken) == 5)
            {
                m_dfXMultiplier   = CPLAtof(papszToken[1]);
                m_dfYMultiplier   = CPLAtof(papszToken[2]);
                m_dfXDisplacement = CPLAtof(papszToken[3]);
                m_dfYDisplacement = CPLAtof(papszToken[4]);

                if (m_dfXMultiplier == 0.0)
                  m_dfXMultiplier = 1.0;
                if (m_dfYMultiplier == 0.0)
                  m_dfYMultiplier = 1.0;
            }
            CSLDestroy(papszToken);
        }
        else if (STARTS_WITH_CI(pszLine, "COLUMNS"))
        {
            papszToken = CSLTokenizeStringComplex(pszLine," ()\t",TRUE,FALSE);
            bCoordSys = FALSE;
            bColumns = TRUE;
            if (CSLCount(papszToken) == 2)
            {
                nColumns = atoi(papszToken[1]);
                m_nAttribute = nColumns;
                if (nColumns == 0)
                {
                    // Permit to 0 columns
                    bAllColumnsRead = TRUE;
                    bColumns = FALSE;
                }
            }
            else
            {
                bColumns = FALSE;
                m_nAttribute = 0;
            }
            CSLDestroy(papszToken);
        }
        else if (bCoordSys == TRUE)
        {
            char *pszTmp = m_pszCoordSys;
            m_pszCoordSys = CPLStrdup(CPLSPrintf("%s %s",m_pszCoordSys,
                                                 pszLine));
            CPLFree(pszTmp);
            //printf("Reading CoordSys\n");
            // Reading CoordSys
        }
    }

    if (!bAllColumnsRead)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "COLUMNS keyword not found or invalid number of columns read in %s.  File may be corrupt.",
                 m_pszFname);
        return -1;
    }

    if ((pszLine = m_poMIFFile->GetLastLine()) == NULL ||
        STARTS_WITH_CI(m_poMIFFile->GetLastLine(), "DATA") == FALSE)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "DATA keyword not found in %s.  File may be corrupt.",
                 m_pszFname);
        return -1;
    }

    /*-----------------------------------------------------------------
     * Move pointer to first line of first object
     *----------------------------------------------------------------*/
    while (((pszLine = m_poMIFFile->GetLine()) != NULL) &&
           m_poMIFFile->IsValidFeature(pszLine) == FALSE)
        ;

    *pbIsEmpty = (pszLine == NULL);

    /*-----------------------------------------------------------------
     * Check for Unique and Indexed flags
     *----------------------------------------------------------------*/
    if (m_pszIndex)
    {
        papszToken = CSLTokenizeStringComplex(m_pszIndex," ,\t",TRUE,FALSE);
        for(int i=0; papszToken && papszToken[i]; i++)
        {
            int nVal = atoi(papszToken[i]);
            if (nVal > 0 && nVal <= m_poDefn->GetFieldCount())
                m_pabFieldIndexed[nVal-1] = TRUE;
        }
        CSLDestroy(papszToken);
    }

    if (m_pszUnique)
    {
        papszToken = CSLTokenizeStringComplex(m_pszUnique," ,\t",TRUE,FALSE);
        for(int i=0; papszToken && papszToken[i]; i++)
        {
            int nVal = atoi(papszToken[i]);
            if (nVal > 0 && nVal <= m_poDefn->GetFieldCount())
                m_pabFieldUnique[nVal-1] = TRUE;
        }
        CSLDestroy(papszToken);
    }

    return 0;
}

/************************************************************************/
/*                             AddFields()                              */
/************************************************************************/

int  MIFFile::AddFields(const char *pszLine)
{
    int nStatus = 0;

    CPLAssert(m_bHeaderWrote == FALSE);
    char **papszToken =
        CSLTokenizeStringComplex(pszLine, " (,)\t", TRUE, FALSE);
    int numTok = CSLCount(papszToken);

    if (numTok >= 3 && EQUAL(papszToken[1], "char"))
    {
        /*-------------------------------------------------
         * CHAR type
         *------------------------------------------------*/
        nStatus = AddFieldNative(papszToken[0], TABFChar,
                                 atoi(papszToken[2]));
    }
    else if (numTok >= 2 && EQUAL(papszToken[1], "integer"))
    {
        if (numTok == 2)
        {
            /*-------------------------------------------------
             * INTEGER type without a specified width
             *------------------------------------------------*/
            nStatus = AddFieldNative(papszToken[0], TABFInteger);
        }
        else if (numTok > 2)
        {
            /*-------------------------------------------------
             * INTEGER type with a specified width
             *------------------------------------------------*/
            nStatus = AddFieldNative(papszToken[0], TABFInteger, atoi(papszToken[2]));
        }
    }
    else if (numTok >= 2 && EQUAL(papszToken[1], "smallint"))
    {
        if (numTok == 2)
        {
            /*-------------------------------------------------
             * SMALLINT type without a specified width
             *------------------------------------------------*/
            nStatus = AddFieldNative(papszToken[0], TABFSmallInt);
        }
        else if (numTok > 2)
        {
            /*-------------------------------------------------
             * SMALLINT type with a specified width
             *------------------------------------------------*/
            nStatus = AddFieldNative(papszToken[0], TABFSmallInt, atoi(papszToken[2]));
        }
    }
    else if (numTok >= 4 && EQUAL(papszToken[1], "decimal"))
    {
        /*-------------------------------------------------
         * DECIMAL type
         *------------------------------------------------*/
        nStatus = AddFieldNative(papszToken[0], TABFDecimal,
                                 atoi(papszToken[2]), atoi(papszToken[3]));
    }
    else if (numTok >= 2 && EQUAL(papszToken[1], "float"))
    {
        /*-------------------------------------------------
         * FLOAT type
         *------------------------------------------------*/
        nStatus = AddFieldNative(papszToken[0], TABFFloat);
    }
    else if (numTok >= 2 && EQUAL(papszToken[1], "date"))
    {
        /*-------------------------------------------------
         * DATE type (returned as a string: "DD/MM/YYYY" or "YYYYMMDD")
         *------------------------------------------------*/
        nStatus = AddFieldNative(papszToken[0], TABFDate);
    }
    else if (numTok >= 2 && EQUAL(papszToken[1], "time"))
    {
        /*-------------------------------------------------
         *  TIME type (v900, returned as a string: "HH:MM:SS" or "HHMMSSmmm")
         *------------------------------------------------*/
        nStatus = AddFieldNative(papszToken[0], TABFTime);
    }
    else if (numTok >= 2 && EQUAL(papszToken[1], "datetime"))
    {
        /*-------------------------------------------------
         * DATETIME type (v900, returned as a string: "DD/MM/YYYY HH:MM:SS",
         * "YYYY/MM/DD HH:MM:SS" or "YYYYMMDDHHMMSSmmm")
         *------------------------------------------------*/
        nStatus = AddFieldNative(papszToken[0], TABFDateTime);
    }
    else if (numTok >= 2 && EQUAL(papszToken[1], "logical"))
    {
        /*-------------------------------------------------
         * LOGICAL type (value "T" or "F")
         *------------------------------------------------*/
        nStatus = AddFieldNative(papszToken[0], TABFLogical);
    }
    else
      nStatus = -1; // Unrecognized field type or line corrupt

    CSLDestroy(papszToken);
    papszToken = NULL;

    if (nStatus != 0)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Failed to parse field definition in file %s", m_pszFname);
        return -1;
    }

    return 0;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig MIFFile::GetFeatureCount (int bForce)
{

    if( m_poFilterGeom != NULL || m_poAttrQuery != NULL )
        return OGRLayer::GetFeatureCount( bForce );
    else
    {
        if (bForce == TRUE)
            PreParseFile();

        if (m_bPreParsed)
            return m_nFeatureCount;
        else
            return -1;
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void MIFFile::ResetReading()

{
    m_poMIFFile->Rewind();

    const char *pszLine = NULL;
    while ((pszLine = m_poMIFFile->GetLine()) != NULL)
      if (STARTS_WITH_CI(pszLine, "DATA"))
        break;

    while ((pszLine = m_poMIFFile->GetLine()) != NULL)
    {
        if (m_poMIFFile->IsValidFeature(pszLine))
          break;
    }

    if( m_poMIDFile != NULL )
    {
        m_poMIDFile->Rewind();
        m_poMIDFile->GetLine();
    }

    // We're positioned on first feature.  Feature Ids start at 1.
    if (m_poCurFeature)
    {
        delete m_poCurFeature;
        m_poCurFeature = NULL;
    }

    m_nCurFeatureId = 0;
    m_nPreloadedId = 1;
}

/************************************************************************/
/*                            PreParseFile()                            */
/************************************************************************/

void MIFFile::PreParseFile()
{
    char **papszToken = NULL;

    GBool bPLine = FALSE;
    GBool bText = FALSE;

    if (m_bPreParsed == TRUE)
      return;

    m_poMIFFile->Rewind();

    const char *pszLine = NULL;
    while ((pszLine = m_poMIFFile->GetLine()) != NULL)
      if (STARTS_WITH_CI(pszLine, "DATA"))
        break;

    m_nPoints = m_nLines = m_nRegions = m_nTexts = 0;

    while ((pszLine = m_poMIFFile->GetLine()) != NULL)
    {
        if (m_poMIFFile->IsValidFeature(pszLine))
        {
            bPLine = FALSE;
            bText = FALSE;
            m_nFeatureCount++;
        }

        CSLDestroy(papszToken);
        papszToken = CSLTokenizeString2(pszLine, " \t", CSLT_HONOURSTRINGS);

        if (STARTS_WITH_CI(pszLine, "POINT"))
        {
            m_nPoints++;
            if (CSLCount(papszToken) == 3)
            {
                UpdateExtents(m_poMIFFile->GetXTrans(CPLAtof(papszToken[1])),
                             m_poMIFFile->GetYTrans(CPLAtof(papszToken[2])));
            }
        }
        else if (STARTS_WITH_CI(pszLine, "LINE") ||
                 STARTS_WITH_CI(pszLine, "RECT") ||
                 STARTS_WITH_CI(pszLine, "ROUNDRECT") ||
                 STARTS_WITH_CI(pszLine, "ARC") ||
                 STARTS_WITH_CI(pszLine, "ELLIPSE"))
        {
            if (CSLCount(papszToken) == 5)
            {
                m_nLines++;
                UpdateExtents(m_poMIFFile->GetXTrans(CPLAtof(papszToken[1])),
                             m_poMIFFile->GetYTrans(CPLAtof(papszToken[2])));
                UpdateExtents(m_poMIFFile->GetXTrans(CPLAtof(papszToken[3])),
                             m_poMIFFile->GetYTrans(CPLAtof(papszToken[4])));
            }
        }
        else if (STARTS_WITH_CI(pszLine, "REGION") )
        {
            m_nRegions++;
            bPLine = TRUE;
        }
        else if( STARTS_WITH_CI(pszLine, "PLINE"))
        {
            m_nLines++;
            bPLine = TRUE;
        }
        else if (STARTS_WITH_CI(pszLine, "TEXT"))
        {
            m_nTexts++;
            bText = TRUE;
        }
        else if (bPLine == TRUE)
        {
            if (CSLCount(papszToken) == 2 &&
                strchr("-.0123456789", papszToken[0][0]) != NULL)
            {
                UpdateExtents( m_poMIFFile->GetXTrans(CPLAtof(papszToken[0])),
                              m_poMIFFile->GetYTrans(CPLAtof(papszToken[1])));
            }
        }
        else if (bText == TRUE)
        {
           if (CSLCount(papszToken) == 4 &&
                strchr("-.0123456789", papszToken[0][0]) != NULL)
            {
                UpdateExtents(m_poMIFFile->GetXTrans(CPLAtof(papszToken[0])),
                             m_poMIFFile->GetYTrans(CPLAtof(papszToken[1])));
                UpdateExtents(m_poMIFFile->GetXTrans(CPLAtof(papszToken[2])),
                             m_poMIFFile->GetYTrans(CPLAtof(papszToken[3])));
            }
        }
      }

    CSLDestroy(papszToken);

    m_poMIFFile->Rewind();

    while ((pszLine = m_poMIFFile->GetLine()) != NULL)
      if (STARTS_WITH_CI(pszLine, "DATA"))
        break;

    while ((pszLine = m_poMIFFile->GetLine()) != NULL)
    {
        if (m_poMIFFile->IsValidFeature(pszLine))
          break;
    }

    if( m_poMIDFile != NULL )
    {
        m_poMIDFile->Rewind();
        m_poMIDFile->GetLine();
    }

    m_bPreParsed = TRUE;
}

/**********************************************************************
 *                   MIFFile::WriteMIFHeader()
 *
 * Generate the .MIF header.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int MIFFile::WriteMIFHeader()
{
    GBool bFound;

    if (m_eAccessMode != TABWrite)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "WriteMIFHeader() can be used only with Write access.");
        return -1;
    }

    if (m_poDefn==NULL || m_poDefn->GetFieldCount() == 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "File %s must contain at least 1 attribute field.",
                 m_pszFname);
        return -1;
    }

    /*-----------------------------------------------------------------
     * Start writing header.
     *----------------------------------------------------------------*/
    m_bHeaderWrote = TRUE;
    m_poMIFFile->WriteLine("Version %d\n", m_nVersion);
    m_poMIFFile->WriteLine("Charset \"%s\"\n", m_pszCharset);

    // Delimiter is not required if you use \t as delimiter
    if ( !EQUAL(m_pszDelimiter, "\t") )
        m_poMIFFile->WriteLine("Delimiter \"%s\"\n", m_pszDelimiter);

    bFound = FALSE;

    for( int iField = 0; iField<m_poDefn->GetFieldCount(); iField++ )
    {
        if (m_pabFieldUnique[iField])
        {
            if (!bFound)
                m_poMIFFile->WriteLine("Unique %d", iField+1);
            else
                m_poMIFFile->WriteLine(",%d", iField+1);
            bFound = TRUE;
        }
    }
    if (bFound)
        m_poMIFFile->WriteLine("\n");

    bFound = FALSE;
    for( int iField = 0; iField < m_poDefn->GetFieldCount(); iField++ )
    {
        if (m_pabFieldIndexed[iField])
        {
            if (!bFound)
                m_poMIFFile->WriteLine("Index  %d", iField+1);
            else
                m_poMIFFile->WriteLine(",%d", iField+1);
            bFound = TRUE;
        }
    }
    if (bFound)
        m_poMIFFile->WriteLine("\n");

    if (m_pszCoordSys && m_bBoundsSet)
    {
        m_poMIFFile->WriteLine("CoordSys %s "
                               "Bounds (%.15g, %.15g) (%.15g, %.15g)\n",
                               m_pszCoordSys,
                               m_dXMin, m_dYMin, m_dXMax, m_dYMax);
    }
    else if (m_pszCoordSys)
    {
        m_poMIFFile->WriteLine("CoordSys %s\n",m_pszCoordSys);
    }

    /*-----------------------------------------------------------------
     * Column definitions
     *----------------------------------------------------------------*/
    CPLAssert(m_paeFieldType);

    m_poMIFFile->WriteLine("Columns %d\n", m_poDefn->GetFieldCount());

    for( int iField = 0; iField < m_poDefn->GetFieldCount(); iField++ )
    {
        OGRFieldDefn *poFieldDefn = m_poDefn->GetFieldDefn(iField);

        switch(m_paeFieldType[iField])
        {
          case TABFInteger:
            m_poMIFFile->WriteLine("  %s Integer\n",
                                poFieldDefn->GetNameRef());
            break;
          case TABFSmallInt:
            m_poMIFFile->WriteLine("  %s SmallInt\n",
                                   poFieldDefn->GetNameRef());
            break;
          case TABFFloat:
            m_poMIFFile->WriteLine("  %s Float\n",
                                   poFieldDefn->GetNameRef());
            break;
          case TABFDecimal:
            m_poMIFFile->WriteLine("  %s Decimal(%d,%d)\n",
                                   poFieldDefn->GetNameRef(),
                                   poFieldDefn->GetWidth(),
                                   poFieldDefn->GetPrecision());
            break;
          case TABFLogical:
            m_poMIFFile->WriteLine("  %s Logical\n",
                                   poFieldDefn->GetNameRef());
            break;
          case TABFDate:
            m_poMIFFile->WriteLine("  %s Date\n",
                                   poFieldDefn->GetNameRef());
            break;
          case TABFTime:
            m_poMIFFile->WriteLine("  %s Time\n",
                                   poFieldDefn->GetNameRef());
            break;
          case TABFDateTime:
            m_poMIFFile->WriteLine("  %s DateTime\n",
                                   poFieldDefn->GetNameRef());
            break;
          case TABFChar:
          default:
            m_poMIFFile->WriteLine("  %s Char(%d)\n",
                                   poFieldDefn->GetNameRef(),
                                   poFieldDefn->GetWidth());
        }
    }

    /*-----------------------------------------------------------------
     * Ready to write objects
     *----------------------------------------------------------------*/
    m_poMIFFile->WriteLine("Data\n\n");

    return 0;
}

/**********************************************************************
 *                   MIFFile::Close()
 *
 * Close current file, and release all memory used.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int MIFFile::Close()
{
    /* flush .mif header if not already written */
    if ( m_poDefn != NULL && m_bHeaderWrote == FALSE
         && m_eAccessMode != TABRead )
    {
        WriteMIFHeader();
    }

    if (m_poMIDFile)
    {
        m_poMIDFile->Close();
        delete m_poMIDFile;
        m_poMIDFile = NULL;
    }

    if (m_poMIFFile)
    {
        m_poMIFFile->Close();
        delete m_poMIFFile;
        m_poMIFFile = NULL;
    }

    if (m_poCurFeature)
    {
        delete m_poCurFeature;
        m_poCurFeature = NULL;
    }

    /*-----------------------------------------------------------------
     * Note: we have to check the reference count before deleting
     * m_poSpatialRef and m_poDefn
     *----------------------------------------------------------------*/
    if (m_poDefn && m_poDefn->Dereference() == 0)
        delete m_poDefn;
    m_poDefn = NULL;

    if (m_poSpatialRef && m_poSpatialRef->Dereference() == 0)
        delete m_poSpatialRef;
    m_poSpatialRef = NULL;

    CPLFree(m_pszCoordSys);
    m_pszCoordSys = NULL;

    CPLFree(m_pszDelimiter);
    m_pszDelimiter = NULL;

    CPLFree(m_pszUnique);
    m_pszUnique = NULL;

    CPLFree(m_pszFname);
    m_pszFname = NULL;

    m_nVersion = 0;

    CPLFree(m_pszCharset);
    m_pszCharset = NULL;

    CPLFree(m_pabFieldIndexed);
    m_pabFieldIndexed = NULL;
    CPLFree(m_pabFieldUnique);
    m_pabFieldUnique = NULL;

    CPLFree( m_pszIndex );
    m_pszIndex = NULL;

    CPLFree(m_paeFieldType);
    m_paeFieldType = NULL;

    m_nCurFeatureId = 0;
    m_nPreloadedId = 0;
    m_nFeatureCount =0;

    m_bBoundsSet = FALSE;

    return 0;
}

/**********************************************************************
 *                   MIFFile::GetNextFeatureId()
 *
 * Returns feature id that follows nPrevId, or -1 if it is the
 * last feature id.  Pass nPrevId=-1 to fetch the first valid feature id.
 **********************************************************************/
GIntBig MIFFile::GetNextFeatureId(GIntBig nPrevId)
{
    if (m_eAccessMode != TABRead)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GetNextFeatureId() can be used only with Read access.");
        return -1;
    }

    if (nPrevId <= 0 && m_poMIFFile->GetLastLine() != NULL)
        return 1;       // Feature Ids start at 1
    else if (nPrevId > 0 && m_poMIFFile->GetLastLine() != NULL)
        return nPrevId + 1;
    else
        return -1;
}

/**********************************************************************
 *                   MIFFile::GotoFeature()
 *
 * Private method to move MIF and MID pointers ready to read specified
 * feature.  Note that Feature Ids start at 1.
 *
 * Returns 0 on success, -1 on error (likely request for invalid feature id)
 **********************************************************************/
int MIFFile::GotoFeature(int nFeatureId)
{

    if (nFeatureId < 1)
      return -1;

    if (nFeatureId == m_nPreloadedId) // CorrectPosition
    {
        return 0;
    }
    else
    {
        if (nFeatureId < m_nPreloadedId || m_nCurFeatureId == 0)
            ResetReading();

        while(m_nPreloadedId < nFeatureId)
        {
            if (NextFeature() == FALSE)
              return -1;
        }

        CPLAssert(m_nPreloadedId == nFeatureId);

        return 0;
    }
}

/**********************************************************************
 *                   MIFFile::NextFeature()
 **********************************************************************/

GBool MIFFile::NextFeature()
{
    const char *pszLine = NULL;
    while ((pszLine = m_poMIFFile->GetLine()) != NULL)
    {
        if (m_poMIFFile->IsValidFeature(pszLine))
        {
            if( m_poMIDFile != NULL )
                m_poMIDFile->GetLine();
            m_nPreloadedId++;
            return TRUE;
        }
    }
    return FALSE;
}

/**********************************************************************
 *                   MIFFile::GetFeatureRef()
 *
 * Fill and return a TABFeature object for the specified feature id.
 *
 * The returned pointer is a reference to an object owned and maintained
 * by this MIFFile object.  It should not be altered or freed by the
 * caller and its contents is guaranteed to be valid only until the next
 * call to GetFeatureRef() or Close().
 *
 * Returns NULL if the specified feature id does not exist of if an
 * error happened.  In any case, CPLError() will have been called to
 * report the reason of the failure.
 **********************************************************************/
TABFeature *MIFFile::GetFeatureRef(GIntBig nFeatureId)
{
    if (m_eAccessMode != TABRead)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GetFeatureRef() can be used only with Read access.");
        return NULL;
    }

    /*-----------------------------------------------------------------
     * Make sure file is opened and Validate feature id by positioning
     * the read pointers for the .MAP and .DAT files to this feature id.
     *----------------------------------------------------------------*/
    if (m_poMIFFile == NULL)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "GetFeatureRef() failed: file is not opened!");
        return NULL;
    }

    if ( !CPL_INT64_FITS_ON_INT32(nFeatureId) || GotoFeature((int)nFeatureId)!= 0 )
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "GetFeatureRef() failed: invalid feature id " CPL_FRMT_GIB,
                 nFeatureId);
        return NULL;
    }

    /*-----------------------------------------------------------------
     * Create new feature object of the right type
     *----------------------------------------------------------------*/
    const char *pszLine = NULL;
    if ((pszLine = m_poMIFFile->GetLastLine()) != NULL)
    {
        // Delete previous feature... we'll start we a clean one.
        if (m_poCurFeature)
            delete m_poCurFeature;
        m_poCurFeature = NULL;

        m_nCurFeatureId = m_nPreloadedId;

        if (STARTS_WITH_CI(pszLine, "NONE"))
        {
            m_poCurFeature = new TABFeature(m_poDefn);
        }
        else if (STARTS_WITH_CI(pszLine, "POINT"))
        {
            // Special case, we need to know two lines to decide the type
            char **papszToken =
                CSLTokenizeString2(pszLine, " \t", CSLT_HONOURSTRINGS);

            if (CSLCount(papszToken) !=3)
            {
                CSLDestroy(papszToken);
                CPLError(CE_Failure, CPLE_NotSupported,
                         "GetFeatureRef() failed: invalid point line: '%s'",
                         pszLine);
                return NULL;
            }

            m_poMIFFile->SaveLine(pszLine);

            if ((pszLine = m_poMIFFile->GetLine()) != NULL)
            {
                CSLDestroy(papszToken);
                papszToken = CSLTokenizeStringComplex(pszLine," ,()\t",
                                                      TRUE,FALSE);
                if (CSLCount(papszToken)> 0 &&STARTS_WITH_CI(papszToken[0], "SYMBOL"))
                {
                    switch (CSLCount(papszToken))
                    {
                      case 4:
                        m_poCurFeature = new TABPoint(m_poDefn);
                        break;
                      case 7:
                        m_poCurFeature = new TABFontPoint(m_poDefn);
                        break;
                      case 5:
                        m_poCurFeature = new TABCustomPoint(m_poDefn);
                        break;
                      default:
                        CSLDestroy(papszToken);
                        CPLError(CE_Failure, CPLE_NotSupported,
                                 "GetFeatureRef() failed: invalid symbol "
                                 "line: '%s'", pszLine);
                        return NULL;
                        break;
                    }
                }
            }
            CSLDestroy(papszToken);

            if (m_poCurFeature == NULL)
            {
                // No symbol clause... default to TABPoint
                m_poCurFeature = new TABPoint(m_poDefn);
            }
        }
        else if (STARTS_WITH_CI(pszLine, "LINE") ||
                 STARTS_WITH_CI(pszLine, "PLINE"))
        {
            m_poCurFeature = new TABPolyline(m_poDefn);
        }
        else if (STARTS_WITH_CI(pszLine, "REGION"))
        {
            m_poCurFeature = new TABRegion(m_poDefn);
        }
        else if (STARTS_WITH_CI(pszLine, "ARC"))
        {
            m_poCurFeature = new TABArc(m_poDefn);
        }
        else if (STARTS_WITH_CI(pszLine, "TEXT"))
        {
            m_poCurFeature = new TABText(m_poDefn);
        }
        else if (STARTS_WITH_CI(pszLine, "RECT") ||
                 STARTS_WITH_CI(pszLine, "ROUNDRECT"))
        {
            m_poCurFeature = new TABRectangle(m_poDefn);
        }
        else if (STARTS_WITH_CI(pszLine, "ELLIPSE"))
        {
            m_poCurFeature = new TABEllipse(m_poDefn);
        }
        else if (STARTS_WITH_CI(pszLine, "MULTIPOINT"))
        {
            m_poCurFeature = new TABMultiPoint(m_poDefn);
        }
        else if (STARTS_WITH_CI(pszLine, "COLLECTION"))
        {
            m_poCurFeature = new TABCollection(m_poDefn);
        }
        else
        {
            if (!EQUAL(pszLine,""))
               CPLError(CE_Failure, CPLE_NotSupported,
                   "Error during reading, unknown type %s.",
                     pszLine);

            //m_poCurFeature = new TABDebugFeature(m_poDefn);
            return NULL;
        }
    }

    CPLAssert(m_poCurFeature);
    if (m_poCurFeature == NULL)
        return NULL;

   /*-----------------------------------------------------------------
     * Read fields from the .DAT file
     * GetRecordBlock() has already been called above...
     *----------------------------------------------------------------*/
    if (m_poMIDFile != NULL && m_poCurFeature->ReadRecordFromMIDFile(m_poMIDFile) != 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Error during reading Record.");

        delete m_poCurFeature;
        m_poCurFeature = NULL;
        return NULL;
    }

    /*-----------------------------------------------------------------
     * Read geometry from the .MAP file
     * MoveToObjId() has already been called above...
     *----------------------------------------------------------------*/
    if (m_poCurFeature->ReadGeometryFromMIFFile(m_poMIFFile) != 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Error during reading Geometry.");

        delete m_poCurFeature;
        m_poCurFeature = NULL;
        return NULL;
    }

    /* If the feature geometry is Text, and the value is empty(""), transform
       it to a geometry none */
    if (m_poCurFeature->GetFeatureClass() == TABFCText)
    {
       TABText *poTextFeature = (TABText*)m_poCurFeature;
       if (strlen(poTextFeature->GetTextString()) == 0)
       {
           TABFeature *poTmpFeature = new TABFeature(m_poDefn);
           for( int i = 0; i < m_poDefn->GetFieldCount(); i++ )
           {
               poTmpFeature->SetField( i, m_poCurFeature->GetRawFieldRef( i ) );
           }
           delete m_poCurFeature;
           m_poCurFeature = poTmpFeature;
       }
    }

    /*---------------------------------------------------------------------
     * The act of reading the geometry causes the first line of the
     * next object to be preloaded.  Set the preloaded id appropriately.
     *--------------------------------------------------------------------- */
    if( m_poMIFFile->GetLastLine() != NULL )
        m_nPreloadedId++;
    else
        m_nPreloadedId = 0;

    /* Update the Current Feature ID */
    m_poCurFeature->SetFID(m_nCurFeatureId);

    return m_poCurFeature;
}

/**********************************************************************
 *                   MIFFile::CreateFeature()
 *
 * Write a new feature to this dataset. The passed in feature is updated
 * with the new feature id.
 *
 * Returns OGRERR_NONE on success, or an appropriate OGRERR_ code if an
 * error happened in which case, CPLError() will have been called to
 * report the reason of the failure.
 **********************************************************************/
OGRErr MIFFile::CreateFeature(TABFeature *poFeature)
{
    int nFeatureId = -1;

    if (m_eAccessMode != TABWrite)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "CreateFeature() can be used only with Write access.");
        return OGRERR_UNSUPPORTED_OPERATION;
    }

    /*-----------------------------------------------------------------
     * Make sure file is opened and establish new feature id.
     *----------------------------------------------------------------*/
    if (m_poMIDFile == NULL)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "CreateFeature() failed: file is not opened!");
        return OGRERR_FAILURE;
    }

    if (m_bHeaderWrote == FALSE)
    {
        /*-------------------------------------------------------------
         * OK, this is the first feature in the dataset... make sure the
         * .MID schema has been initialized.
         *------------------------------------------------------------*/
        if (m_poDefn == NULL)
            SetFeatureDefn(poFeature->GetDefnRef(), NULL);

         WriteMIFHeader();
         nFeatureId = 1;
    }
    else
    {
        nFeatureId = ++ m_nWriteFeatureId;
    }

    /*-----------------------------------------------------------------
     * Write geometry to the .Mif file
     *----------------------------------------------------------------*/
    if (m_poMIFFile == NULL ||
        poFeature->WriteGeometryToMIFFile(m_poMIFFile) != 0)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Failed writing geometry for feature id %d in %s",
                 nFeatureId, m_pszFname);
        return OGRERR_FAILURE;
    }

    if (m_poMIDFile == NULL ||
        poFeature->WriteRecordToMIDFile(m_poMIDFile) != 0 )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Failed writing attributes for feature id %d in %s",
                 nFeatureId, m_pszFname);
        return OGRERR_FAILURE;
    }

    poFeature->SetFID(nFeatureId);

    return OGRERR_NONE;
}

/**********************************************************************
 *                   MIFFile::GetLayerDefn()
 *
 * Returns a reference to the OGRFeatureDefn that will be used to create
 * features in this dataset.
 *
 * Returns a reference to an object that is maintained by this MIFFile
 * object (and thus should not be modified or freed by the caller) or
 * NULL if the OGRFeatureDefn has not been initialized yet (i.e. no file
 * opened yet)
 **********************************************************************/
OGRFeatureDefn *MIFFile::GetLayerDefn()
{
    return m_poDefn;
}

/**********************************************************************
 *                   MIFFile::SetFeatureDefn()
 *
 * Pass a reference to the OGRFeatureDefn that will be used to create
 * features in this dataset.  This function should be called after
 * creating a new dataset, but before writing the first feature.
 * All features that will be written to this dataset must share this same
 * OGRFeatureDefn.
 *
 * This function will use poFeatureDefn to create a local copy that
 * will be used to build the .MID file, etc.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int MIFFile::SetFeatureDefn(OGRFeatureDefn *poFeatureDefn,
                         TABFieldType *paeMapInfoNativeFieldTypes /* =NULL */)
{
    /*-----------------------------------------------------------------
     * Check that call happens at the right time in dataset's life.
     *----------------------------------------------------------------*/
    if ( m_eAccessMode == TABWrite && m_bHeaderWrote )
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "SetFeatureDefn() must be called after opening a new "
                 "dataset, but before writing the first feature to it.");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Delete current feature defn if there is already one.
     * AddFieldNative() will take care of creating a new one for us.
     *----------------------------------------------------------------*/
    if (m_poDefn && m_poDefn->Dereference() == 0)
        delete m_poDefn;
    m_poDefn = NULL;

    /*-----------------------------------------------------------------
     * Copy field information
     *----------------------------------------------------------------*/
    const int numFields = poFeatureDefn->GetFieldCount();
    int nStatus = 0;

    for( int iField = 0; iField<numFields; iField++ )
    {
        TABFieldType eMapInfoType;
        OGRFieldDefn *poFieldDefn = poFeatureDefn->GetFieldDefn(iField);

        if (paeMapInfoNativeFieldTypes)
        {
            eMapInfoType = paeMapInfoNativeFieldTypes[iField];
        }
        else
        {
            /*---------------------------------------------------------
             * Map OGRFieldTypes to MapInfo native types
             *--------------------------------------------------------*/
            switch(poFieldDefn->GetType())
            {
              case OFTInteger:
                eMapInfoType = TABFInteger;
                break;
              case OFTReal:
                eMapInfoType = TABFFloat;
                break;
              case OFTDateTime:
                eMapInfoType = TABFDateTime;
                break;
              case OFTDate:
                eMapInfoType = TABFDate;
                break;
              case OFTTime:
                eMapInfoType = TABFTime;
                break;
              case OFTString:
              default:
                eMapInfoType = TABFChar;
            }
        }

        nStatus = AddFieldNative(poFieldDefn->GetNameRef(), eMapInfoType,
                                 poFieldDefn->GetWidth(),
                                 poFieldDefn->GetPrecision(), FALSE, FALSE);
    }

    return nStatus;
}

/**********************************************************************
 *                   MIFFile::AddFieldNative()
 *
 * Create a new field using a native mapinfo data type... this is an
 * alternative to defining fields through the OGR interface.
 * This function should be called after creating a new dataset, but before
 * writing the first feature.
 *
 * This function will build/update the OGRFeatureDefn that will have to be
 * used when writing features to this dataset.
 *
 * A reference to the OGRFeatureDefn can be obtained using GetLayerDefn().
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int MIFFile::AddFieldNative(const char *pszName, TABFieldType eMapInfoType,
                            int nWidth /*=0*/, int nPrecision /*=0*/,
                            GBool bIndexed /*=FALSE*/, GBool bUnique/*=FALSE*/, int bApproxOK )
{
    char *pszCleanName = NULL;
    int nStatus = 0;
    char szNewFieldName[31+1]; /* 31 is the max characters for a field name*/
    unsigned int nRenameNum = 1;

    /*-----------------------------------------------------------------
     * Check that call happens at the right time in dataset's life.
     *----------------------------------------------------------------*/
    if ( m_eAccessMode == TABWrite && m_bHeaderWrote )
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "AddFieldNative() must be called after opening a new "
                 "dataset, but before writing the first feature to it.");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Validate field width... must be <= 254
     *----------------------------------------------------------------*/
    if (nWidth > 254)
    {
        CPLError(CE_Warning, CPLE_IllegalArg,
                 "Invalid size (%d) for field '%s'.  "
                 "Size must be 254 or less.", nWidth, pszName);
        nWidth = 254;
    }

    /*-----------------------------------------------------------------
     * Map fields with width=0 (variable length in OGR) to a valid default
     *----------------------------------------------------------------*/
    if (eMapInfoType == TABFDecimal && nWidth == 0)
        nWidth=20;
    else if (eMapInfoType == TABFChar && nWidth == 0)
        nWidth=254; /* char fields */

    /*-----------------------------------------------------------------
     * Create new OGRFeatureDefn if not done yet...
     *----------------------------------------------------------------*/
    if (m_poDefn == NULL)
    {
        char *pszFeatureClassName = TABGetBasename(m_pszFname);
        m_poDefn = new OGRFeatureDefn(pszFeatureClassName);
        CPLFree(pszFeatureClassName);
        // Ref count defaults to 0... set it to 1
        m_poDefn->Reference();
    }

    /*-----------------------------------------------------------------
     * Make sure field name is valid... check for special chars, etc.
     * (pszCleanName will have to be freed.)
     *----------------------------------------------------------------*/
    pszCleanName = TABCleanFieldName(pszName);

    if( !bApproxOK &&
        ( m_poDefn->GetFieldIndex(pszCleanName) >= 0 ||
          !EQUAL(pszName, pszCleanName) ) )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Failed to add field named '%s'",
                  pszName );
    }

    strncpy(szNewFieldName, pszCleanName, sizeof(szNewFieldName)-1);
    szNewFieldName[sizeof(szNewFieldName)-1] = '\0';

    while (m_poDefn->GetFieldIndex(szNewFieldName) >= 0 && nRenameNum < 10)
    {
      CPLsnprintf( szNewFieldName, sizeof(szNewFieldName), "%.29s_%.1u", pszCleanName, nRenameNum );
      nRenameNum ++;
    }

    while (m_poDefn->GetFieldIndex(szNewFieldName) >= 0 && nRenameNum < 100)
    {
      CPLsnprintf( szNewFieldName, sizeof(szNewFieldName), "%.29s%.2u", pszCleanName, nRenameNum );
      nRenameNum ++;
    }

    if (m_poDefn->GetFieldIndex(szNewFieldName) >= 0)
    {
      CPLError( CE_Failure, CPLE_NotSupported,
                "Too many field names like '%s' when truncated to 31 letters "
                "for MapInfo format.", pszCleanName );
    }

    if( !EQUAL(pszCleanName,szNewFieldName) )
    {
      CPLError( CE_Warning, CPLE_NotSupported,
                "Normalized/laundered field name: '%s' to '%s'",
                pszCleanName,
                szNewFieldName );
    }

    /*-----------------------------------------------------------------
     * Map MapInfo native types to OGR types
     *----------------------------------------------------------------*/
    OGRFieldDefn *poFieldDefn = NULL;

    switch(eMapInfoType)
    {
      case TABFChar:
        /*-------------------------------------------------
         * CHAR type
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(szNewFieldName, OFTString);
        poFieldDefn->SetWidth(nWidth);
        break;
      case TABFInteger:
        /*-------------------------------------------------
         * INTEGER type
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(szNewFieldName, OFTInteger);
        poFieldDefn->SetWidth(nWidth);
        break;
      case TABFSmallInt:
        /*-------------------------------------------------
         * SMALLINT type
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(szNewFieldName, OFTInteger);
        poFieldDefn->SetWidth(nWidth);
        break;
      case TABFDecimal:
        /*-------------------------------------------------
         * DECIMAL type
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(szNewFieldName, OFTReal);
        poFieldDefn->SetWidth(nWidth);
        poFieldDefn->SetPrecision(nPrecision);
        break;
      case TABFFloat:
        /*-------------------------------------------------
         * FLOAT type
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(szNewFieldName, OFTReal);
        break;
      case TABFDate:
        /*-------------------------------------------------
         * DATE type (V450, returned as a string: "DD/MM/YYYY" or "YYYYMMDD")
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(szNewFieldName,
#ifdef MITAB_USE_OFTDATETIME
                                                   OFTDate);
#else
                                                   OFTString);
#endif
        poFieldDefn->SetWidth(10);
        m_nVersion = std::max(m_nVersion, 450);
        break;
      case TABFTime:
        /*-------------------------------------------------
         * TIME type (v900, returned as a string: "HH:MM:SS" or "HHMMSSmmm")
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(szNewFieldName,
#ifdef MITAB_USE_OFTDATETIME
                                                   OFTTime);
#else
                                                   OFTString);
#endif
        poFieldDefn->SetWidth(9);
        m_nVersion = std::max(m_nVersion, 900);
        break;
      case TABFDateTime:
        /*-------------------------------------------------
         * DATETIME type (v900, returned as a string: "DD/MM/YYYY HH:MM:SS",
         * "YYYY/MM/DD HH:MM:SS" or "YYYYMMDDHHMMSSmmm")
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(szNewFieldName,
#ifdef MITAB_USE_OFTDATETIME
                                                   OFTDateTime);
#else
                                                   OFTString);
#endif
        poFieldDefn->SetWidth(19);
        m_nVersion = std::max(m_nVersion, 900);
        break;
      case TABFLogical:
        /*-------------------------------------------------
         * LOGICAL type (value "T" or "F")
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(szNewFieldName, OFTString);
        poFieldDefn->SetWidth(1);
        break;
      default:
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported type for field %s", pszName);
        return -1;
    }

    /*-----------------------------------------------------
     * Add the FieldDefn to the FeatureDefn
     *----------------------------------------------------*/
    m_poDefn->AddFieldDefn(poFieldDefn);
    delete poFieldDefn;

    /*-----------------------------------------------------------------
     * Keep track of native field type
     *----------------------------------------------------------------*/
    m_paeFieldType = (TABFieldType *)CPLRealloc(m_paeFieldType,
                                                m_poDefn->GetFieldCount()*
                                                sizeof(TABFieldType));
    m_paeFieldType[m_poDefn->GetFieldCount()-1] = eMapInfoType;

    /*-----------------------------------------------------------------
     * Extend array of Indexed/Unique flags
     *----------------------------------------------------------------*/
    m_pabFieldIndexed = (GBool *)CPLRealloc(m_pabFieldIndexed,
                                            m_poDefn->GetFieldCount()*
                                            sizeof(GBool));
    m_pabFieldUnique  = (GBool *)CPLRealloc(m_pabFieldUnique,
                                            m_poDefn->GetFieldCount()*
                                            sizeof(GBool));
    m_pabFieldIndexed[m_poDefn->GetFieldCount()-1] = bIndexed;
    m_pabFieldUnique[m_poDefn->GetFieldCount()-1] = bUnique;

    CPLFree(pszCleanName);
    return nStatus;
}

/**********************************************************************
 *                   MIFFile::GetNativeFieldType()
 *
 * Returns the native MapInfo field type for the specified field.
 *
 * Returns TABFUnknown if file is not opened, or if specified field index is
 * invalid.
 **********************************************************************/
TABFieldType MIFFile::GetNativeFieldType(int nFieldId)
{
    if ( m_poDefn==NULL || m_paeFieldType==NULL ||
         nFieldId < 0 || nFieldId >= m_poDefn->GetFieldCount())
        return TABFUnknown;

    return m_paeFieldType[nFieldId];
}

/************************************************************************
 *                       MIFFile::SetFieldIndexed()
 ************************************************************************/

int MIFFile::SetFieldIndexed( int nFieldId )

{
    if ( m_poDefn==NULL || m_pabFieldIndexed==NULL ||
         nFieldId < 0 || nFieldId >= m_poDefn->GetFieldCount())
        return -1;

    m_pabFieldIndexed[nFieldId] = TRUE;

    return 0;
}

/************************************************************************
 *                       MIFFile::IsFieldIndexed()
 ************************************************************************/

GBool MIFFile::IsFieldIndexed( int nFieldId )

{
    if ( m_poDefn==NULL || m_pabFieldIndexed==NULL ||
         nFieldId < 0 || nFieldId >= m_poDefn->GetFieldCount())
        return FALSE;

    return m_pabFieldIndexed[nFieldId];
}

/************************************************************************
 *                       MIFFile::IsFieldUnique()
 ************************************************************************/

GBool MIFFile::IsFieldUnique( int nFieldId )

{
    if ( m_poDefn==NULL || m_pabFieldUnique==NULL ||
         nFieldId < 0 || nFieldId >= m_poDefn->GetFieldCount())
        return FALSE;

    return m_pabFieldUnique[nFieldId];
}

/************************************************************************/
/*                       MIFFile::SetSpatialRef()                       */
/************************************************************************/

int MIFFile::SetSpatialRef( OGRSpatialReference * poSpatialRef )

{
    CPLFree( m_pszCoordSys );

    char* pszCoordSys = MITABSpatialRef2CoordSys( poSpatialRef );
    if( pszCoordSys )
    {
        SetMIFCoordSys(pszCoordSys);
        CPLFree(pszCoordSys);
    }

    return m_pszCoordSys != NULL;
}

/************************************************************************/
/*                      MIFFile::SetMIFCoordSys()                       */
/************************************************************************/

int MIFFile::SetMIFCoordSys(const char * pszMIFCoordSys)

{
    char *pszCoordSys = NULL;

    // Extract the word 'COORDSYS' if present
    if (STARTS_WITH_CI(pszMIFCoordSys, "COORDSYS") )
    {
        pszCoordSys = CPLStrdup(pszMIFCoordSys + 9);
    }
    else
    {
        pszCoordSys = CPLStrdup(pszMIFCoordSys);
    }

    // Extract bounds if present
    char **papszFields =
        CSLTokenizeStringComplex(pszCoordSys, " ,()\t", TRUE, FALSE );
    int iBounds = CSLFindString( papszFields, "Bounds" );
    if (iBounds >= 0 && iBounds + 4 < CSLCount(papszFields))
    {
        m_dXMin = CPLAtof(papszFields[++iBounds]);
        m_dYMin = CPLAtof(papszFields[++iBounds]);
        m_dXMax = CPLAtof(papszFields[++iBounds]);
        m_dYMax = CPLAtof(papszFields[++iBounds]);
        m_bBoundsSet = TRUE;

        char* pszBounds = strstr(pszCoordSys, " Bounds");
        if( pszBounds == NULL )
            pszBounds = strstr(pszCoordSys, "Bounds");
        pszCoordSys[pszBounds - pszCoordSys] = '\0';
    }
    CSLDestroy( papszFields );

    // Assign the CoordSys
    CPLFree( m_pszCoordSys );

    m_pszCoordSys = CPLStrdup(pszCoordSys);
    CPLFree(pszCoordSys);

    return m_pszCoordSys != NULL;
}

/************************************************************************/
/*                       MIFFile::GetSpatialRef()                       */
/************************************************************************/

OGRSpatialReference *MIFFile::GetSpatialRef()

{
    if( m_poSpatialRef == NULL )
        m_poSpatialRef = MITABCoordSys2SpatialRef( m_pszCoordSys );

    return m_poSpatialRef;
}

/**********************************************************************
 *                   MIFFile::UpdateExtents()
 *
 * Private method used to update the dataset extents.
 **********************************************************************/
void MIFFile::UpdateExtents(double dfX, double dfY)
{
    if (m_bExtentsSet == FALSE)
    {
        m_bExtentsSet = TRUE;
        m_sExtents.MinX = m_sExtents.MaxX = dfX;
        m_sExtents.MinY = m_sExtents.MaxY = dfY;
    }
    else
    {
        if (dfX < m_sExtents.MinX)
            m_sExtents.MinX = dfX;
        if (dfX > m_sExtents.MaxX)
          m_sExtents.MaxX = dfX;
        if (dfY < m_sExtents.MinY)
          m_sExtents.MinY = dfY;
        if (dfY > m_sExtents.MaxY)
          m_sExtents.MaxY = dfY;
    }
}

/**********************************************************************
 *                   MIFFile::SetBounds()
 *
 * Set projection coordinates bounds of the newly created dataset.
 *
 * This function must be called after creating a new dataset and before any
 * feature can be written to it.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int MIFFile::SetBounds( double dXMin, double dYMin,
                        double dXMax, double dYMax )
{
    if (m_eAccessMode != TABWrite)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetBounds() can be used only with Write access.");
        return -1;
    }

    m_dXMin = dXMin;
    m_dXMax = dXMax;
    m_dYMin = dYMin;
    m_dYMax = dYMax;
    m_bBoundsSet = TRUE;

    return 0;
}

/**********************************************************************
 *                   MIFFile::GetFeatureCountByType()
 *
 * Return number of features of each type.
 *
 * NOTE: The current implementation always returns -1 for MIF files
 *       since this would require scanning the whole file.
 *
 * When properly implemented, the bForce flag will force scanning the
 * whole file by default.
 *
 * Returns 0 on success, or silently returns -1 (with no error) if this
 * information is not available.
 **********************************************************************/
int MIFFile::GetFeatureCountByType(int &numPoints, int &numLines,
                                   int &numRegions, int &numTexts,
                                   GBool bForce )
{
    if( m_bPreParsed || bForce )
    {
        PreParseFile();

        numPoints = m_nPoints;
        numLines = m_nLines;
        numRegions = m_nRegions;
        numTexts = m_nTexts;
        return 0;
    }
    else
    {
        numPoints = numLines = numRegions = numTexts = 0;
        return -1;
    }
}

/**********************************************************************
 *                   MIFFile::GetBounds()
 *
 * Fetch projection coordinates bounds of a dataset.
 *
 * Pass bForce=FALSE to avoid a scan of the whole file if the bounds
 * are not already available.
 *
 * Returns 0 on success, -1 on error or if bounds are not available and
 * bForce=FALSE.
 **********************************************************************/
int MIFFile::GetBounds(double &dXMin, double &dYMin,
                       double &dXMax, double &dYMax,
                       GBool bForce /*= TRUE*/ )
{
    if (m_bBoundsSet == FALSE && bForce == FALSE)
    {
        return -1;
    }
    else if (m_bBoundsSet == FALSE)
    {
        PreParseFile();
    }

    if (m_bBoundsSet == FALSE)
    {
        return -1;
    }

    dXMin = m_dXMin;
    dXMax = m_dXMax;
    dYMin = m_dYMin;
    dYMax = m_dYMax;

    return 0;
}

/**********************************************************************
 *                   MIFFile::GetExtent()
 *
 * Fetch extent of the data currently stored in the dataset.  We collect
 * this information while preparsing the file ... often already done for
 * other reasons, and if not it is still faster than fully reading all
 * the features just to count them.
 *
 * Returns OGRERR_NONE/OGRRERR_FAILURE.
 **********************************************************************/
OGRErr MIFFile::GetExtent (OGREnvelope *psExtent, int bForce)
{
    if (bForce == TRUE)
        PreParseFile();

    if (m_bPreParsed)
    {
        *psExtent = m_sExtents;
        return OGRERR_NONE;
    }
    else
        return OGRERR_FAILURE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int MIFFile::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    else if( EQUAL(pszCap,OLCSequentialWrite) )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return m_bPreParsed;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastGetExtent) )
        return m_bPreParsed;

    else if( EQUAL(pszCap,OLCCreateField) )
        return TRUE;

    else
        return FALSE;
}
