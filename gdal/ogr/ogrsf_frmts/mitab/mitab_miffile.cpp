/**********************************************************************
 * $Id: mitab_miffile.cpp,v 1.49 2008/11/17 22:06:21 aboudreault Exp $
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
 * $Log: mitab_miffile.cpp,v $
 * Revision 1.49  2008/11/17 22:06:21  aboudreault
 * Added support to use OFTDateTime/OFTDate/OFTTime type when compiled with
 * OGR and fixed reading/writing support for these types.
 *
 * Revision 1.48  2008/09/26 14:40:24  aboudreault
 * Fixed bug: MITAB doesn't support writing DateTime type (bug 1948)
 *
 * Revision 1.47  2008/03/05 20:35:39  dmorissette
 * Replace MITAB 1.x SetFeature() with a CreateFeature() for V2.x (bug 1859)
 *
 * Revision 1.46  2008/02/01 20:30:59  dmorissette
 * Use %.15g instead of %.16g as number precision in .MIF output
 *
 * Revision 1.45  2008/01/29 21:56:39  dmorissette
 * Update dataset version properly for Date/Time/DateTime field types (#1754)
 *
 * Revision 1.44  2008/01/29 20:46:32  dmorissette
 * Added support for v9 Time and DateTime fields (byg 1754)
 *
 * Revision 1.43  2007/09/14 15:35:21  dmorissette
 * Fixed problem with MIF parser being confused by special attribute
 * names (bug 1795)
 *
 * Revision 1.42  2007/06/12 13:52:37  dmorissette
 * Added IMapInfoFile::SetCharset() method (bug 1734)
 *
 * Revision 1.41  2005/10/13 20:12:03  fwarmerdam
 * layers with just regions can't be set as type wkbPolygon because they may
 * have multipolygons (bug GDAL:958)
 *     http://bugzilla.remotesensing.org/show_bug.cgi?id=958
 *
 * Revision 1.40  2005/10/12 14:03:02  fwarmerdam
 * Fixed problem with white space parsing in mitab_miffile.cpp (bug GDAL:954)
 *
 * Revision 1.39  2005/10/04 19:36:10  dmorissette
 * Added support for reading collections from MIF files (bug 1126)
 *
 * Revision 1.38  2004/02/27 21:04:14  fwarmerdam
 * dont write MIF header if file is readonly - gdal bugzilla 509
 *
 * Revision 1.37  2003/12/19 07:54:50  fwarmerdam
 * write mif header on close if not already written out
 *
 * Revision 1.36  2003/08/13 02:49:02  dmorissette
 * Use tab as default delimiter if not explicitly specified (Anthony D, bug 37)
 *
 * Revision 1.35  2003/01/30 22:42:39  daniel
 * Fixed crash in ParseMIFHeader() when .mif doesn't contain a DATA line
 *
 * Revision 1.34  2002/09/23 12:53:29  warmerda
 * fix memory leak of m_pszIndex
 *
 * Revision 1.33  2002/05/08 15:10:48  julien
 * Implement MIFFile::SetMIFCoordSys in mitab_capi.cpp (Bug 984)
 *
 * Revision 1.32  2002/04/26 14:16:49  julien
 * Finishing the implementation of Multipoint (support for MIF)
 *
 * Revision 1.31  2001/09/19 21:39:15  warmerda
 * get extents efficiently
 *
 * Revision 1.30  2001/09/19 14:31:22  warmerda
 * added m_nPreloadedId to keep track of preloaded line
 *
 * Revision 1.29  2001/09/14 19:14:43  warmerda
 * added attribute query support
 *
 * Revision 1.28  2001/08/10 17:49:01  warmerda
 * fixed a few memory leaks
 *
 * Revision 1.27  2001/03/15 03:57:51  daniel
 * Added implementation for new OGRLayer::GetExtent(), returning data MBR.
 *
 * Revision 1.26  2001/03/09 04:14:19  daniel
 * Fixed problem creating new files with mixed case extensions (e.g. ".Tab")
 *
 * Revision 1.25  2001/03/09 03:51:48  daniel
 * Fixed writing MIF header: missing break; for decimal fields
 *
 * Revision 1.24  2001/02/27 19:59:05  daniel
 * Enabled spatial filter in IMapInfoFile::GetNextFeature(), and avoid
 * unnecessary feature cloning in GetNextFeature() and GetFeature()
 *
 * Revision 1.23  2001/01/23 21:23:42  daniel
 * Added projection bounds lookup table, called from TABFile::SetProjInfo()
 *
 * Revision 1.22  2001/01/22 16:03:58  warmerda
 * expanded tabs
 *
 * Revision 1.21  2000/12/15 05:38:38  daniel
 * Produce Warning instead of an error when nWidth>254 in AddFieldNative()
 *
 * Revision 1.20  2000/11/14 06:15:37  daniel
 * Handle '\t' as spaces in parsing, and fixed GotoFeature() to avoid calling
 * ResetReading() when reading forward.
 *
 * Revision 1.19  2000/07/04 01:50:40  warmerda
 * Removed unprotected debugging printf.
 *
 * Revision 1.18  2000/06/28 00:32:04  warmerda
 * Make GetFeatureCountByType() actually work if bForce is TRUE
 * Collect detailed (by feature type) feature counts in PreParse().
 *
 * Revision 1.17  2000/04/27 15:46:25  daniel
 * Make SetFeatureDefn() use AddFieldNative(), scan field names for invalid
 * chars, and map field width=0 (variable length in OGR) to valid defaults
 *
 * Revision 1.16  2000/03/27 03:37:59  daniel
 * Handle bounds in CoordSys for read and write, + handle point SYMBOL line as
 * optional + fixed reading of bounds in PreParseFile()
 *
 * Revision 1.15  2000/02/28 17:05:06  daniel
 * Added support for index and unique directives for read and write
 *
 * Revision 1.14  2000/01/28 07:32:25  daniel
 * Validate char field width (must be <= 254 chars)
 *
 * Revision 1.13  2000/01/24 19:51:33  warmerda
 * AddFieldNative should not fail for read-only datasets
 *
 * Revision 1.12  2000/01/18 23:13:41  daniel
 * Implemented AddFieldNative()
 *
 * ...
 *
 * Revision 1.1  1999/11/08 04:16:07  stephane
 * First Revision
 *
 **********************************************************************/

#include "mitab.h"
#include "mitab_utils.h"
#include <ctype.h>

/*=====================================================================
 *                      class MIFFile
 *====================================================================*/


/**********************************************************************
 *                   MIFFile::MIFFile()
 *
 * Constructor.
 **********************************************************************/
MIFFile::MIFFile()
{
    m_pszFname = NULL;
    m_nVersion = 300;

    // Tab is default delimiter in MIF spec if not explicitly specified.  Use
    // that by default for read mode. In write mode, we will use "," as 
    // delimiter since it's more common than tab (we do this in Open())
    m_pszDelimiter = CPLStrdup("\t");

    m_pszUnique = NULL;
    m_pszIndex = NULL;
    m_pszCoordSys = NULL;

    m_paeFieldType = NULL;
    m_pabFieldIndexed = NULL;
    m_pabFieldUnique = NULL;

    m_dfXMultiplier = 1.0;
    m_dfYMultiplier = 1.0;
    m_dfXDisplacement = 0.0;
    m_dfYDisplacement = 0.0;

    m_poMIDFile = NULL;
    m_poMIFFile = NULL;
    m_nPreloadedId = 0;

    m_poDefn = NULL;
    m_poSpatialRef = NULL;

    m_nCurFeatureId = 0;
    m_nFeatureCount = 0;
    m_nWriteFeatureId = -1;
    m_poCurFeature = NULL;
   
    m_bPreParsed = FALSE;
    m_nAttribut = 0;
    m_bHeaderWrote = FALSE;
    m_nPoints = m_nLines = m_nRegions = m_nTexts = 0;

    m_bExtentsSet = FALSE;
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
int MIFFile::Open(const char *pszFname, const char *pszAccess,
                  GBool bTestOpenNoError /*=FALSE*/ )
{
    char *pszTmpFname = NULL;
    int nFnameLen = 0;
    
    CPLErrorReset();

    if (m_poMIDFile)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                     "Open() failed: object already contains an open file");

        return -1;
    }

    /*-----------------------------------------------------------------
     * Validate access mode
     *----------------------------------------------------------------*/
    if (EQUALN(pszAccess, "r", 1))
    {
        m_eAccessMode = TABRead;
        pszAccess = "rt";
    }
    else if (EQUALN(pszAccess, "w", 1))
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
                 "Open() failed: access mode \"%s\" not supported", pszAccess);
        else
            CPLErrorReset();

        return -1;
    }

    /*-----------------------------------------------------------------
     * Make sure filename has a .MIF or .MID extension... 
     *----------------------------------------------------------------*/
    m_pszFname = CPLStrdup(pszFname);
    nFnameLen = strlen(m_pszFname);
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

        CPLFree(m_pszFname);
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
        if (!bTestOpenNoError)
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unable to open %s.", pszTmpFname);
        else
            CPLErrorReset();

        CPLFree(pszTmpFname);
        Close();

        return -1;
    }


    CPLFree(pszTmpFname);
    pszTmpFname = NULL;

    /*-----------------------------------------------------------------
     * Read MIF File Header
     *----------------------------------------------------------------*/
    if (m_eAccessMode == TABRead && ParseMIFHeader() != 0)
    {
        Close();

        if (!bTestOpenNoError)
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Failed parsing header in %s.", m_pszFname);
        else
            CPLErrorReset();

        return -1;
    }

    /*-----------------------------------------------------------------
     * In write access, set some defaults
     *----------------------------------------------------------------*/
    if (m_eAccessMode == TABWrite)
    {
        m_nVersion = 300;
        m_pszCharset = CPLStrdup("Neutral");
    }

    /* Put the MID file at the correct location, on the first feature */
    if (m_eAccessMode == TABRead && (m_poMIDFile->GetLine() == NULL))
    {
        Close();

        if (bTestOpenNoError)
            CPLErrorReset();

        return -1;
    }

    m_poMIFFile->SetTranslation(m_dfXMultiplier,m_dfYMultiplier,
                                m_dfXDisplacement, m_dfYDisplacement);
    m_poMIDFile->SetTranslation(m_dfXMultiplier,m_dfYMultiplier,
                                m_dfXDisplacement, m_dfYDisplacement);
    m_poMIFFile->SetDelimiter(m_pszDelimiter);
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
            /* we leave it unknown indicating a mixture */;
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
int MIFFile::ParseMIFHeader()
{  
    GBool  bColumns = FALSE, bDataFound = FALSE;
    int    nColumns = 0;
    GBool  bCoordSys = FALSE;
    char  *pszTmp;
            
    
    const char *pszLine;
    char **papszToken;

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
    while (((pszLine = m_poMIFFile->GetLine()) != NULL))
    {
        while(pszLine && (*pszLine == ' ' || *pszLine == '\t') )
            pszLine++;  // skip leading spaces

        if( EQUALN(pszLine,"Data",4) && !bColumns )
        {
            bDataFound = TRUE;
            break;
        }

        if (bColumns == TRUE && nColumns >0)
        {
            if (AddFields(pszLine) == 0)
            {
                nColumns--;
                if (nColumns == 0)
                  bColumns = FALSE;
            }
            else
            {
                bColumns = FALSE;
            }
        }
        else if (EQUALN(pszLine,"VERSION",7))
        {
            papszToken = CSLTokenizeStringComplex(pszLine," ()\t",TRUE,FALSE); 
            bColumns = FALSE; bCoordSys = FALSE;
            if (CSLCount(papszToken)  == 2)
              m_nVersion = atoi(papszToken[1]);

            CSLDestroy(papszToken);
        
        }
        else if (EQUALN(pszLine,"CHARSET",7))
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
        else if (EQUALN(pszLine,"DELIMITER",9))
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
        else if (EQUALN(pszLine,"UNIQUE",6))
        {
            bColumns = FALSE; bCoordSys = FALSE;
          
            m_pszUnique = CPLStrdup(pszLine + 6);
        }
        else if (EQUALN(pszLine,"INDEX",5))
        {
            bColumns = FALSE; bCoordSys = FALSE;
          
            m_pszIndex = CPLStrdup(pszLine + 5);
        }
        else if (EQUALN(pszLine,"COORDSYS",8) )
        {
            bCoordSys = TRUE;
            m_pszCoordSys = CPLStrdup(pszLine + 9);

            // Extract bounds if present
            char  **papszFields;
            papszFields = CSLTokenizeStringComplex(m_pszCoordSys, " ,()\t",
                                                   TRUE, FALSE );
            int iBounds = CSLFindString( papszFields, "Bounds" );
            if (iBounds >= 0 && iBounds + 4 < CSLCount(papszFields))
            {
                m_dXMin = atof(papszFields[++iBounds]);
                m_dYMin = atof(papszFields[++iBounds]);
                m_dXMax = atof(papszFields[++iBounds]);
                m_dYMax = atof(papszFields[++iBounds]);
                m_bBoundsSet = TRUE;
            }
            CSLDestroy( papszFields );
        }
        else if (EQUALN(pszLine,"TRANSFORM",9))
        {
            papszToken = CSLTokenizeStringComplex(pszLine," ,\t",TRUE,FALSE); 
            bColumns = FALSE; bCoordSys = FALSE;
          
            if (CSLCount(papszToken) == 5)
            {
                m_dfXMultiplier   = atof(papszToken[1]);
                m_dfYMultiplier   = atof(papszToken[2]);
                m_dfXDisplacement = atof(papszToken[3]);
                m_dfYDisplacement = atof(papszToken[4]);
                
                if (m_dfXMultiplier == 0.0)
                  m_dfXMultiplier = 1.0;
                if (m_dfYMultiplier == 0.0)
                  m_dfYMultiplier = 1.0;
            }
            CSLDestroy(papszToken);
        }
        else if (EQUALN(pszLine,"COLUMNS",7))
        {
            papszToken = CSLTokenizeStringComplex(pszLine," ()\t",TRUE,FALSE); 
            bCoordSys = FALSE;
            bColumns = TRUE;
            if (CSLCount(papszToken) == 2)
            {
                nColumns = atoi(papszToken[1]);
                m_nAttribut = nColumns;
            }
            else
            {
                bColumns = FALSE;
                m_nAttribut = 0;
            }
            CSLDestroy(papszToken);
        }
        else if (bCoordSys == TRUE)
        {
            pszTmp = m_pszCoordSys;
            m_pszCoordSys = CPLStrdup(CPLSPrintf("%s %s",m_pszCoordSys,
                                                 pszLine));
            CPLFree(pszTmp);
            //printf("Reading CoordSys\n");
            // Reading CoordSys
        }

    }
    
    if ( !bDataFound )
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
    char **papszToken;
    int nStatus = 0,numTok;

    CPLAssert(m_bHeaderWrote == FALSE);
    papszToken = CSLTokenizeStringComplex(pszLine," (,)\t",TRUE,FALSE); 
    numTok = CSLCount(papszToken);

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
        /*-------------------------------------------------
         * INTEGER type
         *------------------------------------------------*/
        nStatus = AddFieldNative(papszToken[0], TABFInteger);
    }
    else if (numTok >= 2 && EQUAL(papszToken[1], "smallint"))
    {
        /*-------------------------------------------------
         * SMALLINT type
         *------------------------------------------------*/
        nStatus = AddFieldNative(papszToken[0], TABFSmallInt);
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

int MIFFile::GetFeatureCount (int bForce)
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
    const char *pszLine;

    m_poMIFFile->Rewind();

    while ((pszLine = m_poMIFFile->GetLine()) != NULL)
      if (EQUALN(pszLine,"DATA",4))
        break;

    while ((pszLine = m_poMIFFile->GetLine()) != NULL)
    {
        if (m_poMIFFile->IsValidFeature(pszLine))
          break;
    }

    m_poMIDFile->Rewind();
    m_poMIDFile->GetLine();
    
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
    const char *pszLine;
    
    GBool bPLine = FALSE;
    GBool bText = FALSE;

    if (m_bPreParsed == TRUE)
      return;

    m_poMIFFile->Rewind();

    while ((pszLine = m_poMIFFile->GetLine()) != NULL)
      if (EQUALN(pszLine,"DATA",4))
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
        papszToken = CSLTokenizeString(pszLine);

        if (EQUALN(pszLine,"POINT",5))
        {
            m_nPoints++;
            if (CSLCount(papszToken) == 3)
            {
                UpdateExtents(m_poMIFFile->GetXTrans(atof(papszToken[1])),
                             m_poMIFFile->GetYTrans(atof(papszToken[2])));
            }
              
        }
        else if (EQUALN(pszLine,"LINE",4) ||
                 EQUALN(pszLine,"RECT",4) ||
                 EQUALN(pszLine,"ROUNDRECT",9) ||
                 EQUALN(pszLine,"ARC",3) ||
                 EQUALN(pszLine,"ELLIPSE",7))
        {
            if (CSLCount(papszToken) == 5)
            {
                m_nLines++;
                UpdateExtents(m_poMIFFile->GetXTrans(atof(papszToken[1])), 
                             m_poMIFFile->GetYTrans(atof(papszToken[2])));
                UpdateExtents(m_poMIFFile->GetXTrans(atof(papszToken[3])), 
                             m_poMIFFile->GetYTrans(atof(papszToken[4])));
            }
        }
        else if (EQUALN(pszLine,"REGION",6) )
        {
            m_nRegions++;
            bPLine = TRUE;
        }
        else if( EQUALN(pszLine,"PLINE",5))
        {
            m_nLines++;
            bPLine = TRUE;
        }
        else if (EQUALN(pszLine,"TEXT",4)) 
        {
            m_nTexts++;
            bText = TRUE;
        }
        else if (bPLine == TRUE)
        {
            if (CSLCount(papszToken) == 2 &&
                strchr("-.0123456789", papszToken[0][0]) != NULL)
            {
                UpdateExtents( m_poMIFFile->GetXTrans(atof(papszToken[0])),
                              m_poMIFFile->GetYTrans(atof(papszToken[1])));
            }
        }
        else if (bText == TRUE)
        {
           if (CSLCount(papszToken) == 4 &&
                strchr("-.0123456789", papszToken[0][0]) != NULL)
            {
                UpdateExtents(m_poMIFFile->GetXTrans(atof(papszToken[0])),
                             m_poMIFFile->GetYTrans(atof(papszToken[1])));
                UpdateExtents(m_poMIFFile->GetXTrans(atof(papszToken[2])),
                             m_poMIFFile->GetYTrans(atof(papszToken[3])));
            } 
        }
        
      }

    CSLDestroy(papszToken);
    
    m_poMIFFile->Rewind();

    while ((pszLine = m_poMIFFile->GetLine()) != NULL)
      if (EQUALN(pszLine,"DATA",4))
        break;

    while ((pszLine = m_poMIFFile->GetLine()) != NULL)
    {
        if (m_poMIFFile->IsValidFeature(pszLine))
          break;
    }

    m_poMIDFile->Rewind();
    m_poMIDFile->GetLine();
 
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
    int iField;
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
    for(iField=0; iField<m_poDefn->GetFieldCount(); iField++)
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
    for(iField=0; iField<m_poDefn->GetFieldCount(); iField++)
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
        
    for(iField=0; iField<m_poDefn->GetFieldCount(); iField++)
    {
        OGRFieldDefn *poFieldDefn;
        poFieldDefn = m_poDefn->GetFieldDefn(iField);
        
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
int MIFFile::GetNextFeatureId(int nPrevId)
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

    return 0;
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
        if (nFeatureId < m_nCurFeatureId || m_nCurFeatureId == 0)
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
    const char *pszLine;
    while ((pszLine = m_poMIFFile->GetLine()) != NULL)
    {
        if (m_poMIFFile->IsValidFeature(pszLine))
        {
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
 * The retruned pointer is a reference to an object owned and maintained
 * by this MIFFile object.  It should not be altered or freed by the 
 * caller and its contents is guaranteed to be valid only until the next
 * call to GetFeatureRef() or Close().
 *
 * Returns NULL if the specified feature id does not exist of if an
 * error happened.  In any case, CPLError() will have been called to
 * report the reason of the failure.
 **********************************************************************/
TABFeature *MIFFile::GetFeatureRef(int nFeatureId)
{
    const char *pszLine;

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
    if (m_poMIDFile == NULL)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "GetFeatureRef() failed: file is not opened!");
        return NULL;
    }

    if (GotoFeature(nFeatureId)!= 0 )
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "GetFeatureRef() failed: invalid feature id %d", 
                 nFeatureId);
        return NULL;
    }


    /*-----------------------------------------------------------------
     * Create new feature object of the right type
     *----------------------------------------------------------------*/
    if ((pszLine = m_poMIFFile->GetLastLine()) != NULL)
    {
        // Delete previous feature... we'll start we a clean one.
        if (m_poCurFeature)
            delete m_poCurFeature;
        m_poCurFeature = NULL;

        m_nCurFeatureId = m_nPreloadedId;

        if (EQUALN(pszLine,"NONE",4))
        {
            m_poCurFeature = new TABFeature(m_poDefn);
        }
        else if (EQUALN(pszLine,"POINT",5))
        {
            // Special case, we need to know two lines to decide the type
            char **papszToken;
            papszToken = CSLTokenizeString(pszLine);
            
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
                if (CSLCount(papszToken)> 0 &&EQUALN(papszToken[0],"SYMBOL",6))
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
        else if (EQUALN(pszLine,"LINE",4) ||
                 EQUALN(pszLine,"PLINE",5))
        {
            m_poCurFeature = new TABPolyline(m_poDefn);
        }
        else if (EQUALN(pszLine,"REGION",6))
        {
            m_poCurFeature = new TABRegion(m_poDefn);
        }  
        else if (EQUALN(pszLine,"ARC",3))
        { 
            m_poCurFeature = new TABArc(m_poDefn);
        }
        else if (EQUALN(pszLine,"TEXT",4))
        {
            m_poCurFeature = new TABText(m_poDefn);
        }
        else if (EQUALN(pszLine,"RECT",4) ||
                 EQUALN(pszLine,"ROUNDRECT",9))
        {
            m_poCurFeature = new TABRectangle(m_poDefn);
        }
        else if (EQUALN(pszLine,"ELLIPSE",7))
        {
            m_poCurFeature = new TABEllipse(m_poDefn);       
        }
        else if (EQUALN(pszLine,"MULTIPOINT",10))
        {
            m_poCurFeature = new TABMultiPoint(m_poDefn);       
        }
        else if (EQUALN(pszLine,"COLLECTION",10))
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
    if (m_poCurFeature->ReadRecordFromMIDFile(m_poMIDFile) != 0)
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
    int numFields;
    int nStatus = 0;

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
    numFields = poFeatureDefn->GetFieldCount();

    for(int iField=0; iField<numFields; iField++)
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
                            GBool bIndexed /*=FALSE*/, GBool bUnique/*=FALSE*/)
{
    OGRFieldDefn *poFieldDefn;
    char *pszCleanName = NULL;
    int nStatus = 0;

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
    else if (nWidth == 0)
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

    /*-----------------------------------------------------------------
     * Map MapInfo native types to OGR types
     *----------------------------------------------------------------*/
    poFieldDefn = NULL;

    switch(eMapInfoType)
    {
      case TABFChar:
        /*-------------------------------------------------
         * CHAR type
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(pszCleanName, OFTString);
        poFieldDefn->SetWidth(nWidth);
        break;
      case TABFInteger:
        /*-------------------------------------------------
         * INTEGER type
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(pszCleanName, OFTInteger);
        break;
      case TABFSmallInt:
        /*-------------------------------------------------
         * SMALLINT type
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(pszCleanName, OFTInteger);
        break;
      case TABFDecimal:
        /*-------------------------------------------------
         * DECIMAL type
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(pszCleanName, OFTReal);
        poFieldDefn->SetWidth(nWidth);
        poFieldDefn->SetPrecision(nPrecision);
        break;
      case TABFFloat:
        /*-------------------------------------------------
         * FLOAT type
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(pszCleanName, OFTReal);
        break;
      case TABFDate:
        /*-------------------------------------------------
         * DATE type (V450, returned as a string: "DD/MM/YYYY" or "YYYYMMDD")
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(pszCleanName, 
#ifdef MITAB_USE_OFTDATETIME
                                                   OFTDate);
#else
                                                   OFTString);
#endif
        poFieldDefn->SetWidth(10);
        m_nVersion = MAX(m_nVersion, 450);
        break;
      case TABFTime:
        /*-------------------------------------------------
         * TIME type (v900, returned as a string: "HH:MM:SS" or "HHMMSSmmm")
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(pszCleanName, 
#ifdef MITAB_USE_OFTDATETIME
                                                   OFTTime);
#else
                                                   OFTString);
#endif
        poFieldDefn->SetWidth(9);
        m_nVersion = MAX(m_nVersion, 900);
        break;
      case TABFDateTime:
        /*-------------------------------------------------
         * DATETIME type (v900, returned as a string: "DD/MM/YYYY HH:MM:SS",
         * "YYYY/MM/DD HH:MM:SS" or "YYYYMMDDHHMMSSmmm")
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(pszCleanName, 
#ifdef MITAB_USE_OFTDATETIME
                                                   OFTDateTime);
#else
                                                   OFTString);
#endif
        poFieldDefn->SetWidth(19);
        break;
        m_nVersion = MAX(m_nVersion, 900);
      case TABFLogical:
        /*-------------------------------------------------
         * LOGICAL type (value "T" or "F")
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(pszCleanName, OFTString);
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

    m_pszCoordSys = MITABSpatialRef2CoordSys( poSpatialRef );

    return( m_pszCoordSys != NULL );
}


/************************************************************************/
/*                      MIFFile::SetMIFCoordSys()                       */
/************************************************************************/

int MIFFile::SetMIFCoordSys(const char * pszMIFCoordSys)

{
    char        **papszFields, *pszCoordSys;
    int         iBounds;

    // Extract the word 'COORDSYS' if present
    if (EQUALN(pszMIFCoordSys,"COORDSYS",8) )
    {
        pszCoordSys = CPLStrdup(pszMIFCoordSys + 9);
    }
    else
    {
        pszCoordSys = CPLStrdup(pszMIFCoordSys);
    }

    // Extract bounds if present
    papszFields = CSLTokenizeStringComplex(pszCoordSys, " ,()\t",
                                           TRUE, FALSE );
    iBounds = CSLFindString( papszFields, "Bounds" );
    if (iBounds >= 0 && iBounds + 4 < CSLCount(papszFields))
    {
        m_dXMin = atof(papszFields[++iBounds]);
        m_dYMin = atof(papszFields[++iBounds]);
        m_dXMax = atof(papszFields[++iBounds]);
        m_dYMax = atof(papszFields[++iBounds]);
        m_bBoundsSet = TRUE;

        pszCoordSys[strstr(pszCoordSys, "Bounds") - pszCoordSys] = '\0';
    }
    CSLDestroy( papszFields );

    // Assign the CoordSys
    CPLFree( m_pszCoordSys );

    m_pszCoordSys = CPLStrdup(pszCoordSys);
    CPLFree(pszCoordSys);

    return( m_pszCoordSys != NULL );
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
 * Private Methode used to update the dataset extents
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
int MIFFile::SetBounds(double dXMin, double dYMin, 
                       double dXMax, double dYMax)
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

    else if( EQUAL(pszCap,OLCSequentialWrite) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return m_bPreParsed;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastGetExtent) )
        return m_bPreParsed;

    else 
        return FALSE;
}
