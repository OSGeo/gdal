/******************************************************************************
 * $Id: ogr_xplane.h $
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

#include "ogr_xplane_reader.h"


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
/*                             readLatLon()                            */
/***********************************************************************/

int OGRXPlaneReader::readLatLon(double* pdfLat, double* pdfLon, int iToken)
{
    int bRet = readDoubleWithBounds(pdfLat, iToken, "latitude", -90., 90.);
    bRet    &= readDoubleWithBounds(pdfLon, iToken + 1, "longitude", -180., 180.);
    return bRet;
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
        osResult = papszTokens[iFirstTokenIndice];
        for(i=1;i<nIDsToSum;i++)
        {
            osResult += " ";
            osResult += papszTokens[iFirstTokenIndice + i];
        }
    }
    return osResult;
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
