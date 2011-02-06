/******************************************************************************
 * $Id: ogr_xplane.h $
 *
 * Project:  X-Plane aeronautical data reader
 * Purpose:  Definition of classes for OGR X-Plane aeronautical data reader.
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

#ifndef _OGR_XPLANE_READER_H_INCLUDED
#define _OGR_XPLANE_READER_H_INCLUDED

#include "cpl_port.h"
#include "cpl_string.h"

#define FEET_TO_METER       0.30479999798832
#define NM_TO_KM            1.852

#define RET_IF_FAIL(x)      if (!(x)) return;

#define SET_IF_INTEREST_LAYER(x)        poReader->x = ((OGRXPlaneLayer*)x == poLayer) ? x : NULL

class OGRXPlaneLayer;
class OGRXPlaneDataSource;

/************************************************************************/
/*                          OGRXPlaneReader                            */
/***********************************************************************/

class OGRXPlaneReader
{
    protected:
        int             nLineNumber;
        char**          papszTokens;
        int             nTokens;
        VSILFILE*       fp;
        char*           pszFilename;
        int             bEOF;
        OGRXPlaneLayer* poInterestLayer;

                                 OGRXPlaneReader();

    public:
        virtual                  ~OGRXPlaneReader();

        virtual OGRXPlaneReader* CloneForLayer(OGRXPlaneLayer* poLayer) = 0;
        virtual int              IsRecognizedVersion( const char* pszVersionString) = 0;
        virtual int              StartParsing( const char * pszFilename );
        virtual void             Read() = 0;
        virtual int              ReadWholeFile();
        virtual int              GetNextFeature();
        virtual void             Rewind();

        int         assertMinCol(int nMinColNum);
        int         readDouble(double* pdfValue, int iToken, const char* pszTokenDesc);
        int         readDoubleWithBounds(double* pdfValue, int iToken, const char* pszTokenDesc,
                                         double dfLowerBound, double dfUpperBound);
        int         readDoubleWithBoundsAndConversion(double* pdfValue, int iToken, const char* pszTokenDesc,
                                                      double dfFactor, double dfLowerBound, double dfUpperBound);
        CPLString   readStringUntilEnd(int iFirstToken);

        int         readLatLon(double* pdfLat, double* pdfLon, int iToken);
        int         readTrueHeading(double* pdfTrueHeading, int iToken, const char* pszTokenDesc = "true heading");
};

/************************************************************************/
/*                       OGRXPlaneEnumeration                           */
/***********************************************************************/

typedef struct
{
    int         eValue;
    const char* pszText;
} sEnumerationElement;

class OGRXPlaneEnumeration
{
    private:
        const char*                 m_pszEnumerationName;
        const sEnumerationElement*  m_osElements;
        int                         m_nElements;

    public:
        OGRXPlaneEnumeration(const char *pszEnumerationName,
                             const sEnumerationElement*  osElements,
                             int nElements);

        const char* GetText(int eValue);

        int         GetValue(const char* pszText);
};

#define DEFINE_XPLANE_ENUMERATION(enumerationName, enumerationValues) \
static OGRXPlaneEnumeration enumerationName( #enumerationName, enumerationValues, \
                    sizeof( enumerationValues ) / sizeof( enumerationValues[0] ) );


/***********************************************************************/
/*                 OGRXPlaneCreateAptFileReader                        */
/***********************************************************************/

OGRXPlaneReader* OGRXPlaneCreateAptFileReader( OGRXPlaneDataSource* poDataSource );

/***********************************************************************/
/*                 OGRXPlaneCreateNavFileReader                        */
/***********************************************************************/

OGRXPlaneReader* OGRXPlaneCreateNavFileReader( OGRXPlaneDataSource* poDataSource );

/***********************************************************************/
/*                 OGRXPlaneCreateAwyFileReader                        */
/***********************************************************************/

OGRXPlaneReader* OGRXPlaneCreateAwyFileReader( OGRXPlaneDataSource* poDataSource );

/***********************************************************************/
/*                 OGRXPlaneCreateFixFileReader                        */
/***********************************************************************/

OGRXPlaneReader* OGRXPlaneCreateFixFileReader( OGRXPlaneDataSource* poDataSource );

#endif
