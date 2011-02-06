/******************************************************************************
 * $Id$
 *
 * Project:  X-Plane aeronautical data reader
 * Purpose:  Definition of classes for OGR X-Plane aeronautical data driver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2008, Even Rouault
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

#include "cpl_vsi_virtual.h"

#include "ogr_xplane_reader.h"

CPL_CVSID("$Id$");

/***********************************************************************/
/*                       OGRXPlaneReader()                             */
/***********************************************************************/

OGRXPlaneReader::OGRXPlaneReader()
{
    papszTokens = NULL;
    fp = NULL;
    pszFilename = NULL;
    bEOF = FALSE;
    nLineNumber = 0;
    poInterestLayer = NULL;
    nTokens = 0;
}

/***********************************************************************/
/*                         ~OGRXPlaneReader()                          */
/***********************************************************************/

OGRXPlaneReader::~OGRXPlaneReader()
{
    CPLFree(pszFilename);
    pszFilename = NULL;

    CSLDestroy(papszTokens);
    papszTokens = NULL;

    if (fp != NULL)
        VSIFCloseL(fp);
    fp = NULL;
}

/************************************************************************/
/*                         StartParsing()                               */
/************************************************************************/

int OGRXPlaneReader::StartParsing( const char * pszFilename )
{
    fp = VSIFOpenL( pszFilename, "rb" );
    if (fp == NULL)
        return FALSE;

    fp = (VSILFILE*) VSICreateBufferedReaderHandle ( (VSIVirtualHandle*) fp );

    const char* pszLine = CPLReadLineL(fp);
    if (!pszLine || (strcmp(pszLine, "I") != 0 &&
                     strcmp(pszLine, "A") != 0))
    {
        VSIFCloseL(fp);
        fp = NULL;
        return FALSE;
    }

    pszLine = CPLReadLineL(fp);
    if (!pszLine || IsRecognizedVersion(pszLine) == FALSE)
    {
        VSIFCloseL(fp);
        fp = NULL;
        return FALSE;
    }

    CPLFree(this->pszFilename);
    this->pszFilename = CPLStrdup(pszFilename);

    nLineNumber = 2;
    CPLDebug("XPlane", "Version/Copyright : %s", pszLine);

    Rewind();

    return TRUE;
}

/************************************************************************/
/*                               Rewind()                               */
/************************************************************************/

void OGRXPlaneReader::Rewind()
{
    if (fp != NULL)
    {
        VSIRewindL(fp);
        CPLReadLineL(fp);
        CPLReadLineL(fp);

        nLineNumber = 2;

        CSLDestroy(papszTokens);
        papszTokens = NULL;

        bEOF = FALSE;
    }
}

/************************************************************************/
/*                         GetNextFeature()                             */
/************************************************************************/

int OGRXPlaneReader::GetNextFeature()
{
    if (fp == NULL || bEOF == TRUE || poInterestLayer == NULL)
        return FALSE;

    Read();
    return TRUE;
}

/************************************************************************/
/*                          ReadWholeFile()                             */
/************************************************************************/

int OGRXPlaneReader::ReadWholeFile()
{
    if (fp == NULL || bEOF == TRUE || nLineNumber != 2 || poInterestLayer != NULL)
        return FALSE;

    Read();
    return TRUE;
}


/***********************************************************************/
/*                          assertMinCol()                             */
/***********************************************************************/

int OGRXPlaneReader::assertMinCol(int nMinColNum)
{
    if (nTokens < nMinColNum)
    {
        CPLDebug("XPlane", "Line %d : not enough columns : %d. %d is the minimum required",
                nLineNumber, nTokens, nMinColNum);
        return FALSE;
    }
    return TRUE;
}


/***********************************************************************/
/*                           readDouble()                              */
/***********************************************************************/

int OGRXPlaneReader::readDouble(double* pdfValue, int iToken, const char* pszTokenDesc)
{
    char* pszNext;
    *pdfValue = CPLStrtod(papszTokens[iToken], &pszNext);
    if (*pszNext != '\0' )
    {
        CPLDebug("XPlane", "Line %d : invalid %s '%s'",
                    nLineNumber, pszTokenDesc, papszTokens[iToken]);
        return FALSE;
    }
    return TRUE;
}

/***********************************************************************/
/*                  readDoubleWithBoundsAndConversion()                */
/***********************************************************************/

int OGRXPlaneReader::readDoubleWithBoundsAndConversion(
                double* pdfValue, int iToken, const char* pszTokenDesc,
                double dfFactor, double dfLowerBound, double dfUpperBound)
{
    int bRet = readDouble(pdfValue, iToken, pszTokenDesc);
    if (bRet)
    {
        *pdfValue *= dfFactor;
        if (*pdfValue < dfLowerBound || *pdfValue > dfUpperBound)
        {
            CPLDebug("XPlane", "Line %d : %s '%s' out of bounds [%f, %f]",
                     nLineNumber, pszTokenDesc, papszTokens[iToken],
                     dfLowerBound / dfFactor, dfUpperBound / dfFactor);
            return FALSE;
        }
    }
    return bRet;
}

/***********************************************************************/
/*                     readDoubleWithBounds()                          */
/***********************************************************************/

int OGRXPlaneReader::readDoubleWithBounds(
                        double* pdfValue, int iToken, const char* pszTokenDesc,
                        double dfLowerBound, double dfUpperBound)
{
    return readDoubleWithBoundsAndConversion(pdfValue, iToken, pszTokenDesc,
                                             1., dfLowerBound, dfUpperBound);
}

/***********************************************************************/
/*                        readStringUntilEnd()                         */
/***********************************************************************/

CPLString OGRXPlaneReader::readStringUntilEnd(int iFirstTokenIndice)
{
    CPLString osResult;
    if (nTokens > iFirstTokenIndice)
    {
        int i;
        int nIDsToSum = nTokens - iFirstTokenIndice;
        const unsigned char* pszStr = (const unsigned char*)papszTokens[iFirstTokenIndice];
        for(int j=0;pszStr[j];j++)
        {
            if (pszStr[j] >= 32 && pszStr[j] <= 127)
                osResult += pszStr[j];
            else
                CPLDebug("XPlane", "Line %d : string with non ASCII characters", nLineNumber);
        }
        for(i=1;i<nIDsToSum;i++)
        {
            osResult += " ";
            pszStr = (const unsigned char*)papszTokens[iFirstTokenIndice + i];
            for(int j=0;pszStr[j];j++)
            {
                if (pszStr[j] >= 32 && pszStr[j] <= 127)
                    osResult += pszStr[j];
                else
                    CPLDebug("XPlane", "Line %d : string with non ASCII characters", nLineNumber);
            }
        }
    }
    return osResult;
}


/***********************************************************************/
/*                             readLatLon()                            */
/***********************************************************************/

int OGRXPlaneReader::readLatLon(double* pdfLat, double* pdfLon, int iToken)
{
    int bRet = readDoubleWithBounds(pdfLat, iToken, "latitude", -90., 90.);
    bRet    &= readDoubleWithBounds(pdfLon, iToken + 1, "longitude", -180., 180.);
    return bRet;
}

/***********************************************************************/
/*                             readTrueHeading()                       */
/***********************************************************************/

int OGRXPlaneReader::readTrueHeading(double* pdfTrueHeading, int iToken, const char* pszTokenDesc)
{
    int bRet = readDoubleWithBounds(pdfTrueHeading, iToken, pszTokenDesc, -180., 360.);
    if (bRet)
    {
        if (*pdfTrueHeading < 0.)
            *pdfTrueHeading += 180.;
    }
    return bRet;
}



/***********************************************************************/
/*                       OGRXPlaneEnumeration()                        */
/***********************************************************************/


OGRXPlaneEnumeration::OGRXPlaneEnumeration(const char *pszEnumerationName,
                            const sEnumerationElement*  osElements,
                            int nElements) :
                                 m_pszEnumerationName(pszEnumerationName),
                                 m_osElements(osElements),
                                 m_nElements(nElements)
{
}

/***********************************************************************/
/*                              GetText()                              */
/***********************************************************************/

const char* OGRXPlaneEnumeration::GetText(int eValue)
{
    int i;
    for(i=0;i<m_nElements;i++)
    {
        if (m_osElements[i].eValue == eValue)
            return m_osElements[i].pszText;
    }
    CPLDebug("XPlane", "Unknown value (%d) for enumeration %s",
             eValue, m_pszEnumerationName);
    return NULL;
}

/***********************************************************************/
/*                             GetValue()                              */
/***********************************************************************/

int OGRXPlaneEnumeration::GetValue(const char* pszText)
{
    int i;
    if (pszText != NULL)
    {
        for(i=0;i<m_nElements;i++)
        {
            if (strcmp(m_osElements[i].pszText, pszText) == 0)
                return m_osElements[i].eValue;
        }
    }
    CPLDebug("XPlane", "Unknown text (%s) for enumeration %s",
             pszText, m_pszEnumerationName);
    return -1;
}
