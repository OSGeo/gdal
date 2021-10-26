/**********************************************************************
 *
 * Name:     mitab_feature.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation of R/W Fcts for (Mid/Mif) in feature classes
 *           specific to MapInfo files.
 * Author:   Stephane Villeneuve, stephane.v@videotron.ca
 *
 **********************************************************************
 * Copyright (c) 1999-2002, Stephane Villeneuve
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
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "mitab_priv.h"
#include "mitab_utils.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"

CPL_CVSID("$Id$")

/*=====================================================================
 *                      class TABFeature
 *====================================================================*/

/************************************************************************/
/*                            MIDTokenize()                             */
/*                                                                      */
/*      We implement a special tokenize function so we can handle       */
/*      multi-byte delimiters (i.e. MITAB bug 1266).                      */
/*                                                                      */
/*      http://bugzilla.maptools.org/show_bug.cgi?id=1266               */
/************************************************************************/
static char **MIDTokenize( const char *pszLine, const char *pszDelim )

{
    char **papszResult = nullptr;
    int iChar;
    int iTokenChar = 0;
    int bInQuotes = FALSE;
    char *pszToken = static_cast<char *>(CPLMalloc(strlen(pszLine)+1));
    int nDelimLen = static_cast<int>(strlen(pszDelim));

    for( iChar = 0; pszLine[iChar] != '\0'; iChar++ )
    {
        if( bInQuotes && pszLine[iChar] == '"' && pszLine[iChar+1] == '"' )
        {
            pszToken[iTokenChar++] = '"';
            iChar++;
        }
        else if( pszLine[iChar] == '"' )
        {
            bInQuotes = !bInQuotes;
        }
        else if( !bInQuotes && strncmp(pszLine+iChar,pszDelim,nDelimLen) == 0 )
        {
            pszToken[iTokenChar] = '\0';
            papszResult = CSLAddString( papszResult, pszToken );

            iChar += static_cast<int>(strlen(pszDelim)) - 1;
            iTokenChar = 0;
        }
        else
        {
            pszToken[iTokenChar++] = pszLine[iChar];
        }
    }

    pszToken[iTokenChar++] = '\0';
    papszResult = CSLAddString( papszResult, pszToken );

    CPLFree( pszToken );

    return papszResult;
}

/**********************************************************************
 *                   TABFeature::ReadRecordFromMIDFile()
 *
 *  This method is used to read the Record (Attributes) for all type of
 *  features included in a mid/mif file.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABFeature::ReadRecordFromMIDFile(MIDDATAFile *fp)
{
#ifdef MITAB_USE_OFTDATETIME
    int nYear = 0;
    int nMonth = 0;
    int nDay = 0;
    int nHour = 0;
    int nMin = 0;
    int nSec = 0;
    int nMS = 0;
    // int nTZFlag = 0;
#endif

    const int nFields = GetFieldCount();

    const char *pszLine = fp->GetLastLine();

    if (pszLine == nullptr)
    {
        CPLError(CE_Failure, CPLE_FileIO,
               "Unexpected EOF while reading attribute record from MID file.");
        return -1;
    }

    char **papszToken = MIDTokenize( pszLine, fp->GetDelimiter() );

    // Ensure that a blank line in a mid file is treated as one field
    // containing an empty string.
    if( nFields == 1 && CSLCount(papszToken) == 0 && pszLine[0] == '\0' )
        papszToken = CSLAddString(papszToken,"");

    // Make sure we found at least the expected number of field values.
    // Note that it is possible to have a stray delimiter at the end of
    // the line (mif/mid files from Geomedia), so don't produce an error
    // if we find more tokens than expected.
    if (CSLCount(papszToken) < nFields)
    {
        CSLDestroy(papszToken);
        return -1;
    }

    OGRFieldDefn *poFDefn = nullptr;
    for( int i = 0; i < nFields; i++ )
    {
        poFDefn = GetFieldDefnRef(i);
        switch(poFDefn->GetType())
        {
#ifdef MITAB_USE_OFTDATETIME
            case OFTTime:
            {
                if (strlen(papszToken[i]) == 9)
                {
                    sscanf(papszToken[i],"%2d%2d%2d%3d",&nHour, &nMin, &nSec, &nMS);
                    SetField(i, nYear, nMonth, nDay, nHour, nMin, static_cast<float>(nSec + nMS / 1000.0f),
                             0);
                }
                break;
            }
            case OFTDate:
            {
                if (strlen(papszToken[i]) == 8)
                {
                    sscanf(papszToken[i], "%4d%2d%2d", &nYear, &nMonth, &nDay);
                    SetField(i, nYear, nMonth, nDay, nHour, nMin, static_cast<float>(nSec), 0);
                }
                break;
            }
            case OFTDateTime:
            {
                if (strlen(papszToken[i]) == 17)
                {
                    sscanf(papszToken[i], "%4d%2d%2d%2d%2d%2d%3d",
                           &nYear, &nMonth, &nDay, &nHour, &nMin, &nSec, &nMS);
                    SetField(i, nYear, nMonth, nDay, nHour, nMin, static_cast<float>(nSec + nMS / 1000.0f),
                             0);
                }
                break;
            }
#endif
            case OFTString:
            {
                CPLString   osValue( papszToken[i] );
                if( !fp->GetEncoding().empty() )
                {
                    osValue.Recode( fp->GetEncoding(), CPL_ENC_UTF8 );
                }
                SetField(i,osValue);
                break;
            }
          default:
             SetField(i,papszToken[i]);
       }
    }

    fp->GetLine();

    CSLDestroy(papszToken);

    return 0;
}

/**********************************************************************
 *                   TABFeature::WriteRecordToMIDFile()
 *
 *  This method is used to write the Record (Attributes) for all type
 *  of feature included in a mid file.
 *
 *  Return 0 on success, -1 on error
 **********************************************************************/
int TABFeature::WriteRecordToMIDFile(MIDDATAFile *fp)
{
    CPLAssert(fp);

#ifdef MITAB_USE_OFTDATETIME
    char szBuffer[20];
    int nYear = 0;
    int nMonth = 0;
    int nDay = 0;
    int nHour = 0;
    int nMin = 0;
    // int nMS = 0;
    int nTZFlag = 0;
    float fSec = 0.0f;
#endif

    const char *delimiter = fp->GetDelimiter();

    OGRFieldDefn *poFDefn = nullptr;
    const int numFields = GetFieldCount();

    for( int iField = 0; iField < numFields; iField++ )
    {
        if (iField != 0)
          fp->WriteLine("%s", delimiter);
        poFDefn = GetFieldDefnRef( iField );

        switch(poFDefn->GetType())
        {
          case OFTString:
          {
            CPLString   osString( GetFieldAsString(iField) );

            if( !fp->GetEncoding().empty() )
            {
                osString.Recode( CPL_ENC_UTF8, fp->GetEncoding() );
            }

            int nStringLen = static_cast<int>( osString.length() );
            const char *pszString = osString.c_str();
            char *pszWorkString = static_cast<char*>(CPLMalloc((2*(nStringLen)+1)*sizeof(char)));
            int j = 0;
            for (int i =0; i < nStringLen; ++i)
            {
              if (pszString[i] == '"')
              {
                pszWorkString[j] = pszString[i];
                ++j;
                pszWorkString[j] = pszString[i];
              }
              else if (pszString[i] == '\n')
              {
                pszWorkString[j] = '\\';
                ++j;
                pszWorkString[j] = 'n';
              }
              else
                pszWorkString[j] = pszString[i];
              ++j;
            }

            pszWorkString[j] = '\0';
            fp->WriteLine("\"%s\"",pszWorkString);
            CPLFree(pszWorkString);
            break;
          }
#ifdef MITAB_USE_OFTDATETIME
          case OFTTime:
          {
              if (!IsFieldSetAndNotNull(iField))
              {
                 szBuffer[0] = '\0';
              }
              else
              {
                  GetFieldAsDateTime(iField, &nYear, &nMonth, &nDay,
                                     &nHour, &nMin, &fSec, &nTZFlag);
                  snprintf(szBuffer, sizeof(szBuffer), "%2.2d%2.2d%2.2d%3.3d", nHour, nMin,
                          static_cast<int>(fSec), OGR_GET_MS(fSec));
              }
              fp->WriteLine("%s",szBuffer);
              break;
          }
          case OFTDate:
          {
              if (!IsFieldSetAndNotNull(iField))
              {
                 szBuffer[0] = '\0';
              }
              else
              {
                  GetFieldAsDateTime(iField, &nYear, &nMonth, &nDay,
                                     &nHour, &nMin, &fSec, &nTZFlag);
                  snprintf(szBuffer, sizeof(szBuffer), "%4.4d%2.2d%2.2d", nYear, nMonth, nDay);
              }
              fp->WriteLine("%s",szBuffer);
              break;
          }
          case OFTDateTime:
          {
              if (!IsFieldSetAndNotNull(iField))
              {
                 szBuffer[0] = '\0';
              }
              else
              {
                  GetFieldAsDateTime(iField, &nYear, &nMonth, &nDay,
                                     &nHour, &nMin, &fSec, &nTZFlag);
                  snprintf(szBuffer, sizeof(szBuffer), "%4.4d%2.2d%2.2d%2.2d%2.2d%2.2d%3.3d",
                          nYear, nMonth, nDay, nHour, nMin,
                          static_cast<int>(fSec), OGR_GET_MS(fSec));
              }
              fp->WriteLine("%s",szBuffer);
              break;
          }
#endif
          default:
            fp->WriteLine("%s",GetFieldAsString(iField));
        }
    }

    fp->WriteLine("\n");

    return 0;
}

/**********************************************************************
 *                   TABFeature::ReadGeometryFromMIFFile()
 *
 * In derived classes, this method should be reimplemented to
 * fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that before calling ReadGeometryFromMAPFile(), poMAPFile
 * currently points to the beginning of a map object.
 *
 * The current implementation does nothing since instances of TABFeature
 * objects contain no geometry (i.e. TAB_GEOM_NONE).
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABFeature::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{
    const char *pszLine = nullptr;

    /* Go to the first line of the next feature */

    while (((pszLine = fp->GetLine()) != nullptr) &&
           fp->IsValidFeature(pszLine) == FALSE)
      ;

    return 0;
}

/**********************************************************************
 *                   TABFeature::WriteGeometryToMIFFile()
 *
 *
 * In derived classes, this method should be reimplemented to
 * write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that before calling WriteGeometryToMAPFile(), poMAPFile
 * currently points to a valid map object.
 *
 * The current implementation does nothing since instances of TABFeature
 * objects contain no geometry.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABFeature::WriteGeometryToMIFFile(MIDDATAFile *fp)
{
    fp->WriteLine("NONE\n");
    return 0;
}

/**********************************************************************
 *
 **********************************************************************/
int TABPoint::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{
    char **papszToken =
        CSLTokenizeString2(fp->GetSavedLine(), " \t", CSLT_HONOURSTRINGS);

    if (CSLCount(papszToken) !=3)
    {
        CSLDestroy(papszToken);
        return -1;
    }

    const double dfX = fp->GetXTrans(CPLAtof(papszToken[1]));
    const double dfY = fp->GetYTrans(CPLAtof(papszToken[2]));

    CSLDestroy(papszToken);
    papszToken = nullptr;

    // Read optional SYMBOL line...
    const char *pszLine = fp->GetLastLine();
    if( pszLine != nullptr )
        papszToken = CSLTokenizeStringComplex(pszLine," ,()\t",
                                              TRUE,FALSE);
    if (papszToken != nullptr && CSLCount(papszToken) == 4 && EQUAL(papszToken[0], "SYMBOL") )
    {
        SetSymbolNo(static_cast<GInt16>(atoi(papszToken[1])));
        SetSymbolColor(atoi(papszToken[2]));
        SetSymbolSize(static_cast<GInt16>(atoi(papszToken[3])));
    }

    CSLDestroy(papszToken);
    papszToken = nullptr;

    // scan until we reach 1st line of next feature
    // Since SYMBOL is optional, we have to test IsValidFeature() on that
    // line as well.
    while (pszLine && fp->IsValidFeature(pszLine) == FALSE)
    {
        pszLine = fp->GetLine();
    }

    OGRGeometry *poGeometry = new OGRPoint(dfX, dfY);

    SetGeometryDirectly(poGeometry);

    SetMBR(dfX, dfY, dfX, dfY);

    return 0;
}

/**********************************************************************
 *
 **********************************************************************/
int TABPoint::WriteGeometryToMIFFile(MIDDATAFile *fp)
{
    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    OGRPoint *poPoint = nullptr;
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
        poPoint = poGeom->toPoint();
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABPoint: Missing or Invalid Geometry!");
        return -1;
    }

    fp->WriteLine("Point %.15g %.15g\n",poPoint->getX(),poPoint->getY());
    fp->WriteLine("    Symbol (%d,%d,%d)\n",GetSymbolNo(),GetSymbolColor(),
                  GetSymbolSize());

    return 0;
}

/**********************************************************************
 *
 **********************************************************************/
int TABFontPoint::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{
    char **papszToken =
        CSLTokenizeString2(fp->GetSavedLine(), " \t", CSLT_HONOURSTRINGS);

    if (CSLCount(papszToken) !=3)
    {
        CSLDestroy(papszToken);
        return -1;
    }

    const double dfX = fp->GetXTrans(CPLAtof(papszToken[1]));
    const double dfY = fp->GetYTrans(CPLAtof(papszToken[2]));

    CSLDestroy(papszToken);

    papszToken = CSLTokenizeStringComplex(fp->GetLastLine()," ,()\t",
                                          TRUE,FALSE);

    if (CSLCount(papszToken) !=7)
    {
        CSLDestroy(papszToken);
        return -1;
    }

    SetSymbolNo(static_cast<GInt16>(atoi(papszToken[1])));
    SetSymbolColor(atoi(papszToken[2]));
    SetSymbolSize(static_cast<GInt16>(atoi(papszToken[3])));
    SetFontName(papszToken[4]);
    SetFontStyleMIFValue(atoi(papszToken[5]));
    SetSymbolAngle(CPLAtof(papszToken[6]));

    CSLDestroy(papszToken);

    OGRGeometry *poGeometry = new OGRPoint(dfX, dfY);

    SetGeometryDirectly(poGeometry);

    SetMBR(dfX, dfY, dfX, dfY);

    /* Go to the first line of the next feature */

    const char *pszLine = nullptr;
    while (((pszLine = fp->GetLine()) != nullptr) &&
           fp->IsValidFeature(pszLine) == FALSE)
      ;
    return 0;
}

/**********************************************************************
 *
 **********************************************************************/
int TABFontPoint::WriteGeometryToMIFFile(MIDDATAFile *fp)
{
    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    OGRPoint *poPoint = nullptr;
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
        poPoint = poGeom->toPoint();
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABFontPoint: Missing or Invalid Geometry!");
        return -1;
    }

    fp->WriteLine("Point %.15g %.15g\n",poPoint->getX(),poPoint->getY());
    fp->WriteLine("    Symbol (%d,%d,%d,\"%s\",%d,%.15g)\n",
                  GetSymbolNo(),GetSymbolColor(),
                  GetSymbolSize(),GetFontNameRef(),GetFontStyleMIFValue(),
                  GetSymbolAngle());

    return 0;
}

/**********************************************************************
 *
 **********************************************************************/
int TABCustomPoint::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{
    char **papszToken =
        CSLTokenizeString2(fp->GetSavedLine(), " \t", CSLT_HONOURSTRINGS);

    if (CSLCount(papszToken) !=3)
    {
        CSLDestroy(papszToken);
        return -1;
    }

    double dfX = fp->GetXTrans(CPLAtof(papszToken[1]));
    double dfY = fp->GetYTrans(CPLAtof(papszToken[2]));

    CSLDestroy(papszToken);

    papszToken = CSLTokenizeStringComplex(fp->GetLastLine()," ,()\t",
                                          TRUE,FALSE);
    if (CSLCount(papszToken) !=5)
    {

        CSLDestroy(papszToken);
        return -1;
    }

    SetFontName(papszToken[1]);
    SetSymbolColor(atoi(papszToken[2]));
    SetSymbolSize(static_cast<GInt16>(atoi(papszToken[3])));
    m_nCustomStyle = static_cast<GByte>(atoi(papszToken[4]));

    CSLDestroy(papszToken);

    OGRGeometry *poGeometry = new OGRPoint(dfX, dfY);

    SetGeometryDirectly(poGeometry);

    SetMBR(dfX, dfY, dfX, dfY);

    /* Go to the first line of the next feature */

    const char *pszLine = nullptr;
    while (((pszLine = fp->GetLine()) != nullptr) &&
           fp->IsValidFeature(pszLine) == FALSE)
      ;

    return 0;
}

/**********************************************************************
 *
 **********************************************************************/
int TABCustomPoint::WriteGeometryToMIFFile(MIDDATAFile *fp)
{
    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    OGRPoint *poPoint = nullptr;
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
        poPoint = poGeom->toPoint();
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABCustomPoint: Missing or Invalid Geometry!");
        return -1;
    }

    fp->WriteLine("Point %.15g %.15g\n",poPoint->getX(),poPoint->getY());
    fp->WriteLine("    Symbol (\"%s\",%d,%d,%d)\n",GetFontNameRef(),
                  GetSymbolColor(), GetSymbolSize(),m_nCustomStyle);

    return 0;
}

/**********************************************************************
 *
 **********************************************************************/
int TABPolyline::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{
    char **papszToken =
        CSLTokenizeString2(fp->GetLastLine(), " \t", CSLT_HONOURSTRINGS);

    if (CSLCount(papszToken) < 1)
    {
        CSLDestroy(papszToken);
        return -1;
    }

    const char          *pszLine = nullptr;
    OGRLineString       *poLine = nullptr;
    OGRMultiLineString  *poMultiLine = nullptr;
    GBool bMultiple = FALSE;
    int nNumPoints=0;
    int nNumSec=0;
    OGREnvelope sEnvelope;

    if (STARTS_WITH_CI(papszToken[0], "LINE"))
    {
        if (CSLCount(papszToken) != 5)
        {
          CSLDestroy(papszToken);
          return -1;
        }

        poLine = new OGRLineString();
        poLine->setNumPoints(2);
        poLine->setPoint(0, fp->GetXTrans(CPLAtof(papszToken[1])),
                         fp->GetYTrans(CPLAtof(papszToken[2])));
        poLine->setPoint(1, fp->GetXTrans(CPLAtof(papszToken[3])),
                         fp->GetYTrans(CPLAtof(papszToken[4])));
        poLine->getEnvelope(&sEnvelope);
        SetGeometryDirectly(poLine);
        SetMBR(sEnvelope.MinX, sEnvelope.MinY,sEnvelope.MaxX,sEnvelope.MaxY);
    }
    else if (STARTS_WITH_CI(papszToken[0], "PLINE"))
    {
        switch (CSLCount(papszToken))
        {
          case 1:
            bMultiple = FALSE;
            pszLine = fp->GetLine();
            if( pszLine == nullptr )
            {
                CSLDestroy(papszToken);
                return -1;
            }
            nNumPoints = atoi(pszLine);
            break;
          case 2:
            bMultiple = FALSE;
            nNumPoints = atoi(papszToken[1]);
            break;
          case 3:
            if (STARTS_WITH_CI(papszToken[1], "MULTIPLE"))
            {
                bMultiple = TRUE;
                nNumSec = atoi(papszToken[2]);
                pszLine = fp->GetLine();
                if( pszLine == nullptr )
                {
                    CSLDestroy(papszToken);
                    return -1;
                }
                nNumPoints = atoi(pszLine);
                break;
            }
            else
            {
              CSLDestroy(papszToken);
              return -1;
            }
            break;
          case 4:
            if (STARTS_WITH_CI(papszToken[1], "MULTIPLE"))
            {
                bMultiple = TRUE;
                nNumSec = atoi(papszToken[2]);
                nNumPoints = atoi(papszToken[3]);
                break;
            }
            else
            {
                CSLDestroy(papszToken);
                return -1;
            }
            break;
          default:
            CSLDestroy(papszToken);
            return -1;
            break;
        }

        if (bMultiple)
        {
            poMultiLine = new OGRMultiLineString();
            for( int j = 0; j < nNumSec; j++ )
            {
                if( j != 0 )
                {
                    pszLine = fp->GetLine();
                    if( pszLine == nullptr )
                    {
                        delete poMultiLine;
                        CSLDestroy(papszToken);
                        return -1;
                    }
                    nNumPoints = atoi(pszLine);
                }
                if (nNumPoints < 2)
                {
                    CPLError(CE_Failure, CPLE_FileIO,
                             "Invalid number of vertices (%d) in PLINE "
                             "MULTIPLE segment.", nNumPoints);
                    delete poMultiLine;
                    CSLDestroy(papszToken);
                    return -1;
                }
                poLine = new OGRLineString();
                const int MAX_INITIAL_POINTS = 100000;
                const int nInitialNumPoints = ( nNumPoints < MAX_INITIAL_POINTS ) ? nNumPoints : MAX_INITIAL_POINTS;
                /* Do not allocate too much memory to begin with */
                poLine->setNumPoints(nInitialNumPoints);
                if( poLine->getNumPoints() != nInitialNumPoints )
                {
                    delete poLine;
                    delete poMultiLine;
                    CSLDestroy(papszToken);
                    return -1;
                }
                for( int i = 0; i < nNumPoints; i++ )
                {
                    if( i == MAX_INITIAL_POINTS )
                    {
                        poLine->setNumPoints(nNumPoints);
                        if( poLine->getNumPoints() != nNumPoints )
                        {
                            delete poLine;
                            delete poMultiLine;
                            CSLDestroy(papszToken);
                            return -1;
                        }
                    }
                    CSLDestroy(papszToken);
                    papszToken = CSLTokenizeString2(fp->GetLine(),
                                                    " \t", CSLT_HONOURSTRINGS);
                    if( CSLCount(papszToken) != 2 )
                    {
                        CSLDestroy(papszToken);
                        delete poLine;
                        delete poMultiLine;
                        return -1;
                    }
                    poLine->setPoint(i,fp->GetXTrans(CPLAtof(papszToken[0])),
                                     fp->GetYTrans(CPLAtof(papszToken[1])));
                }
                if (poMultiLine->addGeometryDirectly(poLine) != OGRERR_NONE)
                {
                    CPLAssert(false); // Just in case OGR is modified
                }
            }
            poMultiLine->getEnvelope(&sEnvelope);
            if (SetGeometryDirectly(poMultiLine) != OGRERR_NONE)
            {
                CPLAssert(false); // Just in case OGR is modified
            }
            SetMBR(sEnvelope.MinX, sEnvelope.MinY,
                   sEnvelope.MaxX,sEnvelope.MaxY);
        }
        else
        {
            if (nNumPoints < 2)
            {
                CPLError(CE_Failure, CPLE_FileIO,
                            "Invalid number of vertices (%d) in PLINE "
                            "segment.", nNumPoints);
                CSLDestroy(papszToken);
                return -1;
            }
            poLine = new OGRLineString();
            const int MAX_INITIAL_POINTS = 100000;
            const int nInitialNumPoints = ( nNumPoints < MAX_INITIAL_POINTS ) ? nNumPoints : MAX_INITIAL_POINTS;
            /* Do not allocate too much memory to begin with */
            poLine->setNumPoints(nInitialNumPoints);
            if( poLine->getNumPoints() != nInitialNumPoints )
            {
                delete poLine;
                CSLDestroy(papszToken);
                return -1;
            }
            for( int i = 0; i < nNumPoints; i++ )
            {
                if( i == MAX_INITIAL_POINTS )
                {
                    poLine->setNumPoints(nNumPoints);
                    if( poLine->getNumPoints() != nNumPoints )
                    {
                        delete poLine;
                        CSLDestroy(papszToken);
                        return -1;
                    }
                }
                CSLDestroy(papszToken);
                papszToken = CSLTokenizeString2(fp->GetLine(),
                                                " \t", CSLT_HONOURSTRINGS);

                if (CSLCount(papszToken) != 2)
                {
                  CSLDestroy(papszToken);
                  delete poLine;
                  return -1;
                }
                poLine->setPoint(i,fp->GetXTrans(CPLAtof(papszToken[0])),
                                 fp->GetYTrans(CPLAtof(papszToken[1])));
            }
            poLine->getEnvelope(&sEnvelope);
            SetGeometryDirectly(poLine);
            SetMBR(sEnvelope.MinX, sEnvelope.MinY,
                   sEnvelope.MaxX,sEnvelope.MaxY);
        }
    }

    CSLDestroy(papszToken);
    papszToken = nullptr;

    while (((pszLine = fp->GetLine()) != nullptr) &&
           fp->IsValidFeature(pszLine) == FALSE)
    {
        papszToken = CSLTokenizeStringComplex(pszLine,"() ,",
                                              TRUE,FALSE);

        if (CSLCount(papszToken) >= 1)
        {
            if (STARTS_WITH_CI(papszToken[0], "PEN"))
            {

                if (CSLCount(papszToken) == 4)
                {
                    SetPenWidthMIF(atoi(papszToken[1]));
                    SetPenPattern(static_cast<GByte>(atoi(papszToken[2])));
                    SetPenColor(atoi(papszToken[3]));
                }
            }
            else if (STARTS_WITH_CI(papszToken[0], "SMOOTH"))
            {
                m_bSmooth = TRUE;
            }
        }
        CSLDestroy(papszToken);
    }
    return 0;
}

/**********************************************************************
 *
 **********************************************************************/
int TABPolyline::WriteGeometryToMIFFile(MIDDATAFile *fp)
{
    OGRMultiLineString *poMultiLine = nullptr;
    OGRLineString *poLine = nullptr;
    int nNumPoints,i;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString)
    {
        /*-------------------------------------------------------------
         * Simple polyline
         *------------------------------------------------------------*/
        poLine = poGeom->toLineString();
        nNumPoints = poLine->getNumPoints();
        if (nNumPoints == 2)
        {
            fp->WriteLine("Line %.15g %.15g %.15g %.15g\n",poLine->getX(0),poLine->getY(0),
                          poLine->getX(1),poLine->getY(1));
        }
        else
        {

            fp->WriteLine("Pline %d\n",nNumPoints);
            for (i=0;i<nNumPoints;i++)
            {
                fp->WriteLine("%.15g %.15g\n",poLine->getX(i),poLine->getY(i));
            }
        }
    }
    else if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbMultiLineString)
    {
        /*-------------------------------------------------------------
         * Multiple polyline... validate all components
         *------------------------------------------------------------*/
        int iLine, numLines;
        poMultiLine = poGeom->toMultiLineString();
        numLines = poMultiLine->getNumGeometries();

        fp->WriteLine("PLINE MULTIPLE %d\n", numLines);

        for(iLine=0; iLine < numLines; iLine++)
        {
            poGeom = poMultiLine->getGeometryRef(iLine);
            if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString)
            {
                poLine = poGeom->toLineString();
                nNumPoints = poLine->getNumPoints();

                fp->WriteLine("  %d\n",nNumPoints);
                for (i=0;i<nNumPoints;i++)
                {
                    fp->WriteLine("%.15g %.15g\n",poLine->getX(i),poLine->getY(i));
                }
            }
            else
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "TABPolyline: Object contains an invalid Geometry!");
            }
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABPolyline: Missing or Invalid Geometry!");
    }

    if (GetPenPattern())
      fp->WriteLine("    Pen (%d,%d,%d)\n",GetPenWidthMIF(),GetPenPattern(),
                    GetPenColor());
    if (m_bSmooth)
      fp->WriteLine("    Smooth\n");

    return 0;
}

/**********************************************************************
 *                   TABRegion::ReadGeometryFromMIFFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MIF file
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABRegion::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{
    m_bSmooth = FALSE;

    /*=============================================================
     * REGION (Similar to PLINE MULTIPLE)
     *============================================================*/
    char **papszToken =
        CSLTokenizeString2(fp->GetLastLine(), " \t", CSLT_HONOURSTRINGS);

    int numLineSections = (CSLCount(papszToken) == 2) ? atoi(papszToken[1]) : 0;
    CSLDestroy(papszToken);
    papszToken = nullptr;
    if( numLineSections < 0 ||
        numLineSections > INT_MAX / static_cast<int>(sizeof(OGRPolygon*)) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid number of sections: %d", numLineSections);
        return -1;
    }

    OGRPolygon **tabPolygons = nullptr;
    const int MAX_INITIAL_SECTIONS = 100000;
    const int numInitialLineSections =
        ( numLineSections < MAX_INITIAL_SECTIONS ) ?
                            numLineSections : MAX_INITIAL_SECTIONS;
    if (numLineSections > 0)
    {
        tabPolygons = static_cast<OGRPolygon**>(
            VSI_MALLOC2_VERBOSE(numInitialLineSections, sizeof(OGRPolygon*)));
        if( tabPolygons == nullptr )
            return -1;
    }

    OGRLinearRing *poRing = nullptr;
    OGRGeometry         *poGeometry = nullptr;
    int                  i,iSection;
    const char          *pszLine = nullptr;
    OGREnvelope          sEnvelope;

    for(iSection=0; iSection<numLineSections; iSection++)
    {
        if( iSection == MAX_INITIAL_SECTIONS )
        {
            OGRPolygon** newTabPolygons = static_cast<OGRPolygon**>(
                    VSI_REALLOC_VERBOSE(tabPolygons,
                                        numLineSections *sizeof(OGRPolygon*)));
            if( newTabPolygons == nullptr )
            {
                iSection --;
                for( ; iSection >= 0; --iSection )
                    delete tabPolygons[iSection];
                VSIFree(tabPolygons);
                return -1;
            }
            tabPolygons = newTabPolygons;
        }

        int numSectionVertices = 0;

        tabPolygons[iSection] = new OGRPolygon();

        if ((pszLine = fp->GetLine()) != nullptr)
        {
            numSectionVertices = atoi(pszLine);
        }
        if (numSectionVertices < 2)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                    "Invalid number of points (%d) in REGION "
                    "segment.", numSectionVertices);
            for( ; iSection >= 0; --iSection )
                delete tabPolygons[iSection];
            VSIFree(tabPolygons);
            return -1;
        }

        poRing = new OGRLinearRing();

        const int MAX_INITIAL_POINTS = 100000;
        const int nInitialNumPoints = ( numSectionVertices < MAX_INITIAL_POINTS ) ? numSectionVertices : MAX_INITIAL_POINTS;
        /* Do not allocate too much memory to begin with */
        poRing->setNumPoints(nInitialNumPoints);
        if( poRing->getNumPoints() != nInitialNumPoints )
        {
            delete poRing;
            for( ; iSection >= 0; --iSection )
                delete tabPolygons[iSection];
            VSIFree(tabPolygons);
            return -1;
        }
        for(i=0; i<numSectionVertices; i++)
        {
            if( i == MAX_INITIAL_POINTS )
            {
                poRing->setNumPoints(numSectionVertices);
                if( poRing->getNumPoints() != numSectionVertices )
                {
                    delete poRing;
                    for( ; iSection >= 0; --iSection )
                        delete tabPolygons[iSection];
                    VSIFree(tabPolygons);
                    return -1;
                }
            }

            papszToken = CSLTokenizeStringComplex(fp->GetLine()," ,\t",
                                                    TRUE,FALSE);
            if (CSLCount(papszToken) < 2)
            {
                CSLDestroy(papszToken);
                papszToken = nullptr;
                delete poRing;
                for( ; iSection >= 0; --iSection )
                    delete tabPolygons[iSection];
                VSIFree(tabPolygons);
                return -1;
            }

            const double dX = fp->GetXTrans(CPLAtof(papszToken[0]));
            const double dY = fp->GetYTrans(CPLAtof(papszToken[1]));
            poRing->setPoint(i, dX, dY);

            CSLDestroy(papszToken);
            papszToken = nullptr;
        }

        poRing->closeRings();

        tabPolygons[iSection]->addRingDirectly(poRing);

        if (numLineSections == 1)
            poGeometry = tabPolygons[iSection];

        poRing = nullptr;
    }

    if (numLineSections > 1)
    {
        int isValidGeometry = FALSE;
        const char* papszOptions[] = { "METHOD=DEFAULT", nullptr };
        poGeometry = OGRGeometryFactory::organizePolygons(
            reinterpret_cast<OGRGeometry**>(tabPolygons), numLineSections, &isValidGeometry, papszOptions );

        if (!isValidGeometry)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Geometry of polygon cannot be translated to Simple Geometry. "
                     "All polygons will be contained in a multipolygon.\n");
        }
    }

    VSIFree(tabPolygons);

    if( poGeometry )
    {
        poGeometry->getEnvelope(&sEnvelope);
        SetGeometryDirectly(poGeometry);

        SetMBR(sEnvelope.MinX, sEnvelope.MinY, sEnvelope.MaxX, sEnvelope.MaxY);
    }

    while (((pszLine = fp->GetLine()) != nullptr) &&
           fp->IsValidFeature(pszLine) == FALSE)
    {
        papszToken = CSLTokenizeStringComplex(pszLine,"() ,",
                                              TRUE,FALSE);

        if (CSLCount(papszToken) > 1)
        {
            if (STARTS_WITH_CI(papszToken[0], "PEN"))
            {

                if (CSLCount(papszToken) == 4)
                {
                    SetPenWidthMIF(atoi(papszToken[1]));
                    SetPenPattern(static_cast<GByte>(atoi(papszToken[2])));
                    SetPenColor(atoi(papszToken[3]));
                }
            }
            else if (STARTS_WITH_CI(papszToken[0], "BRUSH"))
            {
                if (CSLCount(papszToken) >= 3)
                {
                    SetBrushFGColor(atoi(papszToken[2]));
                    SetBrushPattern(static_cast<GByte>(atoi(papszToken[1])));

                    if (CSLCount(papszToken) == 4)
                       SetBrushBGColor(atoi(papszToken[3]));
                    else
                      SetBrushTransparent(TRUE);
                }
            }
            else if (STARTS_WITH_CI(papszToken[0], "CENTER"))
            {
                if (CSLCount(papszToken) == 3)
                {
                    SetCenter(fp->GetXTrans(CPLAtof(papszToken[1])),
                              fp->GetYTrans(CPLAtof(papszToken[2])) );
                }
            }
        }
        CSLDestroy(papszToken);
        papszToken = nullptr;
    }

    return 0;
}

/**********************************************************************
 *                   TABRegion::WriteGeometryToMIFFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MIF file
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABRegion::WriteGeometryToMIFFile(MIDDATAFile *fp)
{
    OGRGeometry *poGeom = GetGeometryRef();

    if (poGeom && (wkbFlatten(poGeom->getGeometryType()) == wkbPolygon ||
                   wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon ) )
    {
        /*=============================================================
         * REGIONs are similar to PLINE MULTIPLE
         *
         * We accept both OGRPolygons (with one or multiple rings) and
         * OGRMultiPolygons as input.
         *============================================================*/
        int     i, iRing, numRingsTotal, numPoints;

        numRingsTotal = GetNumRings();

        fp->WriteLine("Region %d\n",numRingsTotal);

        for(iRing=0; iRing < numRingsTotal; iRing++)
        {
            OGRLinearRing *poRing = GetRingRef(iRing);
            if (poRing == nullptr)
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "TABRegion: Object Geometry contains NULL rings!");
                return -1;
            }

            numPoints = poRing->getNumPoints();

            fp->WriteLine("  %d\n",numPoints);
            for(i=0; i<numPoints; i++)
            {
                fp->WriteLine("%.15g %.15g\n",poRing->getX(i), poRing->getY(i));
            }
        }

        if (GetPenPattern())
          fp->WriteLine("    Pen (%d,%d,%d)\n",
                          GetPenWidthMIF(),GetPenPattern(),
                          GetPenColor());

        if (GetBrushPattern())
        {
            if (GetBrushTransparent() == 0)
              fp->WriteLine("    Brush (%d,%d,%d)\n",GetBrushPattern(),
                            GetBrushFGColor(),GetBrushBGColor());
            else
              fp->WriteLine("    Brush (%d,%d)\n",GetBrushPattern(),
                            GetBrushFGColor());
        }

        if (m_bCenterIsSet)
        {
            fp->WriteLine("    Center %.15g %.15g\n", m_dCenterX, m_dCenterY);
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABRegion: Object contains an invalid Geometry!");
        return -1;
    }

    return 0;
}

/**********************************************************************
 *
 **********************************************************************/
int TABRectangle::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{
    char **papszToken =
        CSLTokenizeString2(fp->GetLastLine(), " \t", CSLT_HONOURSTRINGS);

    if (CSLCount(papszToken) <  5)
    {
        CSLDestroy(papszToken);
        return -1;
    }

    double dXMin = fp->GetXTrans(CPLAtof(papszToken[1]));
    double dXMax = fp->GetXTrans(CPLAtof(papszToken[3]));
    double dYMin = fp->GetYTrans(CPLAtof(papszToken[2]));
    double dYMax = fp->GetYTrans(CPLAtof(papszToken[4]));

    /*-----------------------------------------------------------------
     * Call SetMBR() and GetMBR() now to make sure that min values are
     * really smaller than max values.
     *----------------------------------------------------------------*/
    SetMBR(dXMin, dYMin, dXMax, dYMax);
    GetMBR(dXMin, dYMin, dXMax, dYMax);

    m_bRoundCorners = FALSE;
    m_dRoundXRadius = 0.0;
    m_dRoundYRadius = 0.0;

    if (STARTS_WITH_CI(papszToken[0], "ROUNDRECT"))
    {
        m_bRoundCorners = TRUE;
        if (CSLCount(papszToken) == 6)
        {
          m_dRoundXRadius = CPLAtof(papszToken[5]) / 2.0;
          m_dRoundYRadius = m_dRoundXRadius;
        }
        else
        {
            CSLDestroy(papszToken);
            papszToken = CSLTokenizeString2(fp->GetLine(),
                                            " \t", CSLT_HONOURSTRINGS);
            if (CSLCount(papszToken) ==1 )
              m_dRoundXRadius = m_dRoundYRadius = CPLAtof(papszToken[0])/2.0;
        }
    }
    CSLDestroy(papszToken);
    papszToken = nullptr;

    /*-----------------------------------------------------------------
     * Create and fill geometry object
     *----------------------------------------------------------------*/

    OGRPolygon *poPolygon = new OGRPolygon;
    OGRLinearRing *poRing = new OGRLinearRing();
    if (m_bRoundCorners && m_dRoundXRadius != 0.0 && m_dRoundYRadius != 0.0)
    {
        /*-------------------------------------------------------------
         * For rounded rectangles, we generate arcs with 45 line
         * segments for each corner.  We start with lower-left corner
         * and proceed counterclockwise
         * We also have to make sure that rounding radius is not too
         * large for the MBR however, we
         * always return the true X/Y radius (not adjusted) since this
         * is the way MapInfo seems to do it when a radius bigger than
         * the MBR is passed from TBA to MIF.
         *------------------------------------------------------------*/
        const double dXRadius =
            std::min(m_dRoundXRadius, (dXMax - dXMin) / 2.0);
        const double dYRadius =
            std::min(m_dRoundYRadius, (dYMax - dYMin) / 2.0);
        TABGenerateArc(poRing, 45,
                       dXMin + dXRadius, dYMin + dYRadius, dXRadius, dYRadius,
                       M_PI, 3.0*M_PI/2.0);
        TABGenerateArc(poRing, 45,
                       dXMax - dXRadius, dYMin + dYRadius, dXRadius, dYRadius,
                       3.0*M_PI/2.0, 2.0*M_PI);
        TABGenerateArc(poRing, 45,
                       dXMax - dXRadius, dYMax - dYRadius, dXRadius, dYRadius,
                       0.0, M_PI/2.0);
        TABGenerateArc(poRing, 45,
                       dXMin + dXRadius, dYMax - dYRadius, dXRadius, dYRadius,
                       M_PI/2.0, M_PI);

        TABCloseRing(poRing);
    }
    else
    {
        poRing->addPoint(dXMin, dYMin);
        poRing->addPoint(dXMax, dYMin);
        poRing->addPoint(dXMax, dYMax);
        poRing->addPoint(dXMin, dYMax);
        poRing->addPoint(dXMin, dYMin);
    }

    poPolygon->addRingDirectly(poRing);
    SetGeometryDirectly(poPolygon);

    const char *pszLine = nullptr;
    while (((pszLine = fp->GetLine()) != nullptr) &&
           fp->IsValidFeature(pszLine) == FALSE)
    {
       papszToken = CSLTokenizeStringComplex(pszLine,"() ,",
                                             TRUE,FALSE);

       if (CSLCount(papszToken) > 1)
       {
           if (STARTS_WITH_CI(papszToken[0], "PEN"))
           {
               if (CSLCount(papszToken) == 4)
               {
                   SetPenWidthMIF(atoi(papszToken[1]));
                   SetPenPattern(static_cast<GByte>(atoi(papszToken[2])));
                   SetPenColor(atoi(papszToken[3]));
               }
           }
           else if (STARTS_WITH_CI(papszToken[0], "BRUSH"))
           {
               if (CSLCount(papszToken) >=3)
               {
                   SetBrushFGColor(atoi(papszToken[2]));
                   SetBrushPattern(static_cast<GByte>(atoi(papszToken[1])));

                   if (CSLCount(papszToken) == 4)
                       SetBrushBGColor(atoi(papszToken[3]));
                   else
                      SetBrushTransparent(TRUE);
               }
           }
       }
       CSLDestroy(papszToken);
       papszToken = nullptr;
   }

   return 0;
}

/**********************************************************************
 *
 **********************************************************************/
int TABRectangle::WriteGeometryToMIFFile(MIDDATAFile *fp)
{
    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    OGRPolygon *poPolygon = nullptr;
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPolygon)
        poPolygon = poGeom->toPolygon();
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABRectangle: Missing or Invalid Geometry!");
        return -1;
    }
    /*-----------------------------------------------------------------
     * Note that we will simply use the rectangle's MBR and don't really
     * read the polygon geometry... this should be OK unless the
     * polygon geometry was not really a rectangle.
     *----------------------------------------------------------------*/
    OGREnvelope sEnvelope;
    poPolygon->getEnvelope(&sEnvelope);

    if (m_bRoundCorners == TRUE)
    {
        fp->WriteLine("Roundrect %.15g %.15g %.15g %.15g %.15g\n",
                      sEnvelope.MinX, sEnvelope.MinY,
                      sEnvelope.MaxX, sEnvelope.MaxY, m_dRoundXRadius*2.0);
    }
    else
    {
        fp->WriteLine("Rect %.15g %.15g %.15g %.15g\n",
                      sEnvelope.MinX, sEnvelope.MinY,
                      sEnvelope.MaxX, sEnvelope.MaxY);
    }

    if (GetPenPattern())
      fp->WriteLine("    Pen (%d,%d,%d)\n",GetPenWidthMIF(),GetPenPattern(),
                    GetPenColor());

    if (GetBrushPattern())
    {
        if (GetBrushTransparent() == 0)
          fp->WriteLine("    Brush (%d,%d,%d)\n",GetBrushPattern(),
                        GetBrushFGColor(),GetBrushBGColor());
        else
          fp->WriteLine("    Brush (%d,%d)\n",GetBrushPattern(),
                        GetBrushFGColor());
    }
    return 0;
}

/**********************************************************************
 *
 **********************************************************************/
int TABEllipse::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{
    char **papszToken =
        CSLTokenizeString2(fp->GetLastLine(), " \t", CSLT_HONOURSTRINGS);

    if (CSLCount(papszToken) != 5)
    {
        CSLDestroy(papszToken);
        return -1;
    }

    double dXMin = fp->GetXTrans(CPLAtof(papszToken[1]));
    double dXMax = fp->GetXTrans(CPLAtof(papszToken[3]));
    double dYMin = fp->GetYTrans(CPLAtof(papszToken[2]));
    double dYMax = fp->GetYTrans(CPLAtof(papszToken[4]));

    CSLDestroy(papszToken);
    papszToken = nullptr;

     /*-----------------------------------------------------------------
     * Save info about the ellipse def. inside class members
     *----------------------------------------------------------------*/
    m_dCenterX = (dXMin + dXMax) / 2.0;
    m_dCenterY = (dYMin + dYMax) / 2.0;
    m_dXRadius = std::abs( (dXMax - dXMin) / 2.0 );
    m_dYRadius = std::abs( (dYMax - dYMin) / 2.0 );

    SetMBR(dXMin, dYMin, dXMax, dYMax);

    /*-----------------------------------------------------------------
     * Create and fill geometry object
     *----------------------------------------------------------------*/
    OGRPolygon *poPolygon = new OGRPolygon;
    OGRLinearRing *poRing = new OGRLinearRing();

    /*-----------------------------------------------------------------
     * For the OGR geometry, we generate an ellipse with 2 degrees line
     * segments.
     *----------------------------------------------------------------*/
    TABGenerateArc(poRing, 180,
                   m_dCenterX, m_dCenterY,
                   m_dXRadius, m_dYRadius,
                   0.0, 2.0*M_PI);
    TABCloseRing(poRing);

    poPolygon->addRingDirectly(poRing);
    SetGeometryDirectly(poPolygon);

    const char *pszLine = nullptr;
    while (((pszLine = fp->GetLine()) != nullptr) &&
           fp->IsValidFeature(pszLine) == FALSE)
    {
        papszToken = CSLTokenizeStringComplex(pszLine,"() ,",
                                              TRUE,FALSE);

        if (CSLCount(papszToken) > 1)
        {
            if (STARTS_WITH_CI(papszToken[0], "PEN"))
            {
                if (CSLCount(papszToken) == 4)
                {
                    SetPenWidthMIF(atoi(papszToken[1]));
                    SetPenPattern(static_cast<GByte>(atoi(papszToken[2])));
                    SetPenColor(atoi(papszToken[3]));
                }
            }
            else if (STARTS_WITH_CI(papszToken[0], "BRUSH"))
            {
                if (CSLCount(papszToken) >= 3)
                {
                    SetBrushFGColor(atoi(papszToken[2]));
                    SetBrushPattern(static_cast<GByte>(atoi(papszToken[1])));

                    if (CSLCount(papszToken) == 4)
                      SetBrushBGColor(atoi(papszToken[3]));
                    else
                      SetBrushTransparent(TRUE);
                }
            }
        }
        CSLDestroy(papszToken);
        papszToken = nullptr;
    }
    return 0;
}

/**********************************************************************
 *
 **********************************************************************/
int TABEllipse::WriteGeometryToMIFFile(MIDDATAFile *fp)
{
    OGREnvelope sEnvelope;
    OGRGeometry *poGeom = GetGeometryRef();
    if ( (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPolygon ) ||
         (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint )  )
        poGeom->getEnvelope(&sEnvelope);
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABEllipse: Missing or Invalid Geometry!");
        return -1;
    }

    fp->WriteLine("Ellipse %.15g %.15g %.15g %.15g\n",sEnvelope.MinX, sEnvelope.MinY,
                  sEnvelope.MaxX,sEnvelope.MaxY);

    if (GetPenPattern())
      fp->WriteLine("    Pen (%d,%d,%d)\n",GetPenWidthMIF(),GetPenPattern(),
                    GetPenColor());

    if (GetBrushPattern())
    {
        if (GetBrushTransparent() == 0)
          fp->WriteLine("    Brush (%d,%d,%d)\n",GetBrushPattern(),
                        GetBrushFGColor(),GetBrushBGColor());
        else
          fp->WriteLine("    Brush (%d,%d)\n",GetBrushPattern(),
                        GetBrushFGColor());
    }
    return 0;
}

/**********************************************************************
 *
 **********************************************************************/
int TABArc::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{
    double dXMin = 0.0;
    double dXMax = 0.0;
    double dYMin = 0.0;
    double dYMax = 0.0;

    char **papszToken =
        CSLTokenizeString2(fp->GetLastLine(), " \t", CSLT_HONOURSTRINGS);

    if (CSLCount(papszToken) == 5)
    {
        dXMin = fp->GetXTrans(CPLAtof(papszToken[1]));
        dXMax = fp->GetXTrans(CPLAtof(papszToken[3]));
        dYMin = fp->GetYTrans(CPLAtof(papszToken[2]));
        dYMax = fp->GetYTrans(CPLAtof(papszToken[4]));

        CSLDestroy(papszToken);
        papszToken = CSLTokenizeString2(fp->GetLine(),
                                        " \t", CSLT_HONOURSTRINGS);
        if (CSLCount(papszToken) != 2)
        {
            CSLDestroy(papszToken);
            return -1;
        }

        m_dStartAngle = CPLAtof(papszToken[0]);
        m_dEndAngle = CPLAtof(papszToken[1]);
    }
    else if (CSLCount(papszToken) == 7)
    {
        dXMin = fp->GetXTrans(CPLAtof(papszToken[1]));
        dXMax = fp->GetXTrans(CPLAtof(papszToken[3]));
        dYMin = fp->GetYTrans(CPLAtof(papszToken[2]));
        dYMax = fp->GetYTrans(CPLAtof(papszToken[4]));
        m_dStartAngle = CPLAtof(papszToken[5]);
        m_dEndAngle = CPLAtof(papszToken[6]);
    }
    else
    {
        CSLDestroy(papszToken);
        return -1;
    }

    CSLDestroy(papszToken);
    papszToken = nullptr;

    if( fabs(m_dEndAngle - m_dStartAngle) >= 721 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Wrong start and end angles: %f %f",
                 m_dStartAngle, m_dEndAngle);
        return -1;
    }

    /*-------------------------------------------------------------
     * Start/End angles
     * Since the angles are specified for integer coordinates, and
     * that these coordinates can have the X axis reversed, we have to
     * adjust the angle values for the change in the X axis
     * direction.
     *
     * This should be necessary only when X axis is flipped.
     * __TODO__ Why is order of start/end values reversed as well???
     *------------------------------------------------------------*/

    if (fp->GetXMultiplier() <= 0.0)
    {
        m_dStartAngle = 360.0 - m_dStartAngle;
        m_dEndAngle = 360.0 - m_dEndAngle;
    }

    m_dCenterX = (dXMin + dXMax) / 2.0;
    m_dCenterY = (dYMin + dYMax) / 2.0;
    m_dXRadius = std::abs( (dXMax - dXMin) / 2.0 );
    m_dYRadius = std::abs( (dYMax - dYMin) / 2.0 );

    /*-----------------------------------------------------------------
     * Create and fill geometry object
     * For the OGR geometry, we generate an arc with 2 degrees line
     * segments.
     *----------------------------------------------------------------*/
    OGRLineString *poLine = new OGRLineString;

    int numPts =
         std::max(2,
             (m_dEndAngle < m_dStartAngle
              ? static_cast<int>(std::abs( ((m_dEndAngle+360.0)-m_dStartAngle)/2.0 ) + 1)
              : static_cast<int>(std::abs( (m_dEndAngle-m_dStartAngle)/2.0 ) + 1)));

    TABGenerateArc(poLine, numPts,
                   m_dCenterX, m_dCenterY,
                   m_dXRadius, m_dYRadius,
                   m_dStartAngle*M_PI/180.0, m_dEndAngle*M_PI/180.0);

    SetMBR(dXMin, dYMin, dXMax, dYMax);
    SetGeometryDirectly(poLine);

    const char *pszLine = nullptr;
    while (((pszLine = fp->GetLine()) != nullptr) &&
           fp->IsValidFeature(pszLine) == FALSE)
    {
        papszToken = CSLTokenizeStringComplex(pszLine,"() ,",
                                              TRUE,FALSE);

        if (CSLCount(papszToken) > 1)
        {
            if (STARTS_WITH_CI(papszToken[0], "PEN"))
            {

                if (CSLCount(papszToken) == 4)
                {
                    SetPenWidthMIF(atoi(papszToken[1]));
                    SetPenPattern(static_cast<GByte>(atoi(papszToken[2])));
                    SetPenColor(atoi(papszToken[3]));
                }
            }
        }
        CSLDestroy(papszToken);
        papszToken = nullptr;
   }
   return 0;
}

/**********************************************************************
 *
 **********************************************************************/
int TABArc::WriteGeometryToMIFFile(MIDDATAFile *fp)
{
    /*-------------------------------------------------------------
     * Start/End angles
     * Since we ALWAYS produce files in quadrant 1 then we can
     * ignore the special angle conversion required by flipped axis.
     *------------------------------------------------------------*/

    // Write the Arc's actual MBR
     fp->WriteLine("Arc %.15g %.15g %.15g %.15g\n", m_dCenterX-m_dXRadius,
                   m_dCenterY-m_dYRadius, m_dCenterX+m_dXRadius,
                   m_dCenterY+m_dYRadius);

     fp->WriteLine("  %.15g %.15g\n",m_dStartAngle,m_dEndAngle);

     if (GetPenPattern())
       fp->WriteLine("    Pen (%d,%d,%d)\n",GetPenWidthMIF(),GetPenPattern(),
                     GetPenColor());

    return 0;
}

/**********************************************************************
 *
 **********************************************************************/
int TABText::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{
    const char *pszString = nullptr;
    int bXYBoxRead = 0;

    char **papszToken =
        CSLTokenizeString2(fp->GetLastLine(), " \t", CSLT_HONOURSTRINGS);
    if (CSLCount(papszToken) == 1)
    {
        CSLDestroy(papszToken);
        papszToken = CSLTokenizeString2(fp->GetLine(),
                                        " \t", CSLT_HONOURSTRINGS);
        const int tokenLen = CSLCount(papszToken);
        if (tokenLen == 4)
        {
           pszString = nullptr;
           bXYBoxRead = 1;
        }
        else if (tokenLen == 0)
        {
            pszString = nullptr;
        }
        else if (tokenLen != 1)
        {
            CSLDestroy(papszToken);
            return -1;
        }
        else
        {
          pszString = papszToken[0];
        }
    }
    else if (CSLCount(papszToken) == 2)
    {
        pszString = papszToken[1];
    }
    else
    {
        CSLDestroy(papszToken);
        return -1;
    }

    /*-------------------------------------------------------------
     * Note: The text string may contain escaped "\n" chars, and we
     * sstore them in memory in the UnEscaped form to be OGR
     * compliant. See Maptools bug 1107 for more details.
     *------------------------------------------------------------*/
    char *pszTmpString = CPLStrdup(pszString);
    m_pszString = TABUnEscapeString(pszTmpString, TRUE);
    if (pszTmpString != m_pszString)
        CPLFree(pszTmpString);
    if (!fp->GetEncoding().empty())
    {
        char *pszUtf8String =
            CPLRecode(m_pszString, fp->GetEncoding(), CPL_ENC_UTF8);
        CPLFree(m_pszString);
        m_pszString = pszUtf8String;
    }
    if (!bXYBoxRead)
    {
        CSLDestroy(papszToken);
        papszToken = CSLTokenizeString2(fp->GetLine(),
                                        " \t", CSLT_HONOURSTRINGS);
    }

    if (CSLCount(papszToken) != 4)
    {
        CSLDestroy(papszToken);
        return -1;
    }

    double dXMin = fp->GetXTrans(CPLAtof(papszToken[0]));
    double dXMax = fp->GetXTrans(CPLAtof(papszToken[2]));
    double dYMin = fp->GetYTrans(CPLAtof(papszToken[1]));
    double dYMax = fp->GetYTrans(CPLAtof(papszToken[3]));

    m_dHeight = dYMax - dYMin;  //SetTextBoxHeight(dYMax - dYMin);
    m_dWidth  = dXMax - dXMin;  //SetTextBoxWidth(dXMax - dXMin);

    if (m_dHeight <0.0)
      m_dHeight*=-1.0;
    if (m_dWidth <0.0)
      m_dWidth*=-1.0;

    CSLDestroy(papszToken);
    papszToken = nullptr;

    /* Set/retrieve the MBR to make sure Mins are smaller than Maxs
     */

    SetMBR(dXMin, dYMin, dXMax, dYMax);
    GetMBR(dXMin, dYMin, dXMax, dYMax);

    const char *pszLine = nullptr;
    while (((pszLine = fp->GetLine()) != nullptr) &&
           fp->IsValidFeature(pszLine) == FALSE)
    {
        papszToken = CSLTokenizeStringComplex(pszLine,"() ,",
                                              TRUE,FALSE);

        if (CSLCount(papszToken) > 1)
        {
            if (STARTS_WITH_CI(papszToken[0], "FONT"))
            {
                if (CSLCount(papszToken) >= 5)
                {
                    SetFontName(papszToken[1]);
                    SetFontFGColor(atoi(papszToken[4]));
                    if (CSLCount(papszToken) ==6)
                    {
                        SetFontBGColor(atoi(papszToken[5]));
                        SetFontStyleMIFValue(atoi(papszToken[2]),TRUE);
                    }
                    else
                      SetFontStyleMIFValue(atoi(papszToken[2]));

                    // papsztoken[3] = Size ???
                }
            }
            else if (STARTS_WITH_CI(papszToken[0], "SPACING"))
            {
                if (CSLCount(papszToken) >= 2)
                {
                    if (STARTS_WITH_CI(papszToken[1], "2"))
                    {
                        SetTextSpacing(TABTSDouble);
                    }
                    else if (STARTS_WITH_CI(papszToken[1], "1.5"))
                    {
                        SetTextSpacing(TABTS1_5);
                    }
                }

                if (CSLCount(papszToken) == 7)
                {
                    if (STARTS_WITH_CI(papszToken[2], "LAbel"))
                    {
                        if (STARTS_WITH_CI(papszToken[4], "simple"))
                        {
                            SetTextLineType(TABTLSimple);
                            SetTextLineEndPoint(fp->GetXTrans(CPLAtof(papszToken[5])),
                                                fp->GetYTrans(CPLAtof(papszToken[6])));
                        }
                        else if (STARTS_WITH_CI(papszToken[4], "arrow"))
                        {
                            SetTextLineType(TABTLArrow);
                            SetTextLineEndPoint(fp->GetXTrans(CPLAtof(papszToken[5])),
                                                fp->GetYTrans(CPLAtof(papszToken[6])));
                        }
                    }
                }
            }
            else if (STARTS_WITH_CI(papszToken[0], "Justify"))
            {
                if (CSLCount(papszToken) == 2)
                {
                    if (STARTS_WITH_CI(papszToken[1], "Center"))
                    {
                        SetTextJustification(TABTJCenter);
                    }
                    else  if (STARTS_WITH_CI(papszToken[1], "Right"))
                    {
                        SetTextJustification(TABTJRight);
                    }
                }
            }
            else if (STARTS_WITH_CI(papszToken[0], "Angle"))
            {
                if (CSLCount(papszToken) == 2)
                {
                    SetTextAngle(CPLAtof(papszToken[1]));
                }
            }
            else if (STARTS_WITH_CI(papszToken[0], "LAbel"))
            {
                if (CSLCount(papszToken) == 5)
                {
                    if (STARTS_WITH_CI(papszToken[2], "simple"))
                    {
                        SetTextLineType(TABTLSimple);
                        SetTextLineEndPoint(fp->GetXTrans(CPLAtof(papszToken[3])),
                                           fp->GetYTrans(CPLAtof(papszToken[4])));
                    }
                    else if (STARTS_WITH_CI(papszToken[2], "arrow"))
                    {
                        SetTextLineType(TABTLArrow);
                        SetTextLineEndPoint(fp->GetXTrans(CPLAtof(papszToken[3])),
                                           fp->GetYTrans(CPLAtof(papszToken[4])));
                    }
                }
                // What I do with the XY coordinate
            }
        }
        CSLDestroy(papszToken);
        papszToken = nullptr;
    }
    /*-----------------------------------------------------------------
     * Create an OGRPoint Geometry...
     * The point X,Y values will be the coords of the lower-left corner before
     * rotation is applied.  (Note that the rotation in MapInfo is done around
     * the upper-left corner)
     * We need to calculate the true lower left corner of the text based
     * on the MBR after rotation, the text height and the rotation angle.
     *---------------------------------------------------------------- */
    double dSin = sin(m_dAngle*M_PI/180.0);
    double dCos = cos(m_dAngle*M_PI/180.0);
    double dX = 0.0;
    double dY = 0.0;
    if (dSin > 0.0  && dCos > 0.0)
    {
        dX = dXMin + m_dHeight * dSin;
        dY = dYMin;
    }
    else if (dSin > 0.0  && dCos < 0.0)
    {
        dX = dXMax;
        dY = dYMin - m_dHeight * dCos;
    }
    else if (dSin < 0.0  && dCos < 0.0)
    {
        dX = dXMax + m_dHeight * dSin;
        dY = dYMax;
    }
    else  // dSin < 0 && dCos > 0
    {
        dX = dXMin;
        dY = dYMax - m_dHeight * dCos;
    }

    OGRGeometry *poGeometry = new OGRPoint(dX, dY);

    SetGeometryDirectly(poGeometry);

    /*-----------------------------------------------------------------
     * Compute Text Width: the width of the Text MBR before rotation
     * in ground units... unfortunately this value is not stored in the
     * file, so we have to compute it with the MBR after rotation and
     * the height of the MBR before rotation:
     * With  W = Width of MBR before rotation
     *       H = Height of MBR before rotation
     *       dX = Width of MBR after rotation
     *       dY = Height of MBR after rotation
     *       teta = rotation angle
     *
     *  For [-PI/4..teta..+PI/4] or [3*PI/4..teta..5*PI/4], we'll use:
     *   W = H * (dX - H * sin(teta)) / (H * cos(teta))
     *
     * and for other teta values, use:
     *   W = H * (dY - H * cos(teta)) / (H * sin(teta))
     *---------------------------------------------------------------- */
    dSin = std::abs(dSin);
    dCos = std::abs(dCos);
    if (m_dHeight == 0.0)
        m_dWidth = 0.0;
    else if ( dCos > dSin )
        m_dWidth = m_dHeight * ((dXMax-dXMin) - m_dHeight*dSin) /
                                                        (m_dHeight*dCos);
    else
        m_dWidth = m_dHeight * ((dYMax-dYMin) - m_dHeight*dCos) /
                                                        (m_dHeight*dSin);
    m_dWidth = std::abs(m_dWidth);

   return 0;
}

/**********************************************************************
 *
 **********************************************************************/
int TABText::WriteGeometryToMIFFile(MIDDATAFile *fp)
{
    /*-------------------------------------------------------------
     * Note: The text string may contain unescaped "\n" chars or
     * "\\" chars and we expect to receive them in an unescaped
     * form. Those characters are unescaped in memory to be like
     * other OGR drivers. See MapTools bug 1107 for more details.
     *------------------------------------------------------------*/
    char *pszTmpString;
    if(fp->GetEncoding().empty())
    {
        pszTmpString = TABEscapeString(m_pszString);
    }
    else
    {
        char *pszEncString =
            CPLRecode(m_pszString, CPL_ENC_UTF8, fp->GetEncoding());
        pszTmpString = TABEscapeString(pszEncString);
        if(pszTmpString != pszEncString)
            CPLFree(pszEncString);
    }

    if(pszTmpString == nullptr)
        fp->WriteLine("Text \"\"\n" );
    else
        fp->WriteLine("Text \"%s\"\n", pszTmpString );
    if (pszTmpString != m_pszString)
        CPLFree(pszTmpString);

    //    UpdateTextMBR();
    double dXMin = 0.0;
    double dYMin = 0.0;
    double dXMax = 0.0;
    double dYMax = 0.0;
    GetMBR(dXMin, dYMin, dXMax, dYMax);
    fp->WriteLine("    %.15g %.15g %.15g %.15g\n", dXMin, dYMin, dXMax, dYMax);

    if( IsFontBGColorUsed() )
      fp->WriteLine("    Font (\"%s\",%d,%d,%d,%d)\n", GetFontNameRef(),
                    GetFontStyleMIFValue(),0,GetFontFGColor(),
                    GetFontBGColor());
    else
      fp->WriteLine("    Font (\"%s\",%d,%d,%d)\n", GetFontNameRef(),
                    GetFontStyleMIFValue(),0,GetFontFGColor());

    switch (GetTextSpacing())
    {
      case   TABTS1_5:
        fp->WriteLine("    Spacing 1.5\n");
        break;
      case TABTSDouble:
        fp->WriteLine("    Spacing 2.0\n");
        break;
      case TABTSSingle:
      default:
        break;
    }

    switch (GetTextJustification())
    {
      case TABTJCenter:
        fp->WriteLine("    Justify Center\n");
        break;
      case TABTJRight:
        fp->WriteLine("    Justify Right\n");
        break;
      case TABTJLeft:
      default:
        break;
    }

    if (std::abs(GetTextAngle()) >  0.000001)
        fp->WriteLine("    Angle %.15g\n",GetTextAngle());

    switch (GetTextLineType())
    {
      case TABTLSimple:
        if (m_bLineEndSet)
            fp->WriteLine("    Label Line Simple %.15g %.15g \n",
                          m_dfLineEndX, m_dfLineEndY );
        break;
      case TABTLArrow:
        if (m_bLineEndSet)
            fp->WriteLine("    Label Line Arrow %.15g %.15g \n",
                          m_dfLineEndX, m_dfLineEndY );
        break;
      case TABTLNoLine:
      default:
        break;
    }
    return 0;
}

/**********************************************************************
 *
 **********************************************************************/
int TABMultiPoint::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{
    char **papszToken =
        CSLTokenizeString2(fp->GetLastLine(), " \t", CSLT_HONOURSTRINGS);

    if (CSLCount(papszToken) !=2)
    {
        CSLDestroy(papszToken);
        return -1;
    }

    int nNumPoint = atoi(papszToken[1]);
    OGRMultiPoint *poMultiPoint = new OGRMultiPoint;

    CSLDestroy(papszToken);
    papszToken = nullptr;

    // Get each point and add them to the multipoint feature
    for( int i = 0; i<nNumPoint; i++ )
    {
        papszToken = CSLTokenizeString2(fp->GetLine(),
                                        " \t", CSLT_HONOURSTRINGS);
        if (CSLCount(papszToken) !=2)
        {
            CSLDestroy(papszToken);
            delete poMultiPoint;
            return -1;
        }

        const double dfX = fp->GetXTrans(CPLAtof(papszToken[0]));
        const double dfY = fp->GetXTrans(CPLAtof(papszToken[1]));
        OGRPoint *poPoint = new OGRPoint(dfX, dfY);
        if ( poMultiPoint->addGeometryDirectly( poPoint ) != OGRERR_NONE)
        {
            CPLAssert(false); // Just in case OGR is modified
        }

        // Set center
        if(i == 0)
        {
            SetCenter( dfX, dfY );
        }
        CSLDestroy(papszToken);
    }

    OGREnvelope sEnvelope;
    poMultiPoint->getEnvelope(&sEnvelope);
    if( SetGeometryDirectly( poMultiPoint ) != OGRERR_NONE)
    {
        CPLAssert(false); // Just in case OGR is modified
    }

    SetMBR(sEnvelope.MinX, sEnvelope.MinY,
           sEnvelope.MaxX,sEnvelope.MaxY);

    // Read optional SYMBOL line...

    const char *pszLine = nullptr;
    while (((pszLine = fp->GetLine()) != nullptr) &&
           fp->IsValidFeature(pszLine) == FALSE)
    {
        papszToken = CSLTokenizeStringComplex(pszLine," ,()\t",
                                              TRUE,FALSE);
        if (CSLCount(papszToken) == 4 && EQUAL(papszToken[0], "SYMBOL") )
        {
            SetSymbolNo(static_cast<GInt16>(atoi(papszToken[1])));
            SetSymbolColor(atoi(papszToken[2]));
            SetSymbolSize(static_cast<GInt16>(atoi(papszToken[3])));
        }
        CSLDestroy(papszToken);
    }

    return 0;
}

/**********************************************************************
 *
 **********************************************************************/
int TABMultiPoint::WriteGeometryToMIFFile(MIDDATAFile *fp)
{
    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    OGRGeometry *poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbMultiPoint)
    {
        OGRMultiPoint *poMultiPoint = poGeom->toMultiPoint();
        const int nNumPoints = poMultiPoint->getNumGeometries();

        fp->WriteLine("MultiPoint %d\n", nNumPoints);

        for( int iPoint = 0; iPoint < nNumPoints; iPoint++ )
        {
            /*------------------------------------------------------------
             * Validate each point
             *-----------------------------------------------------------*/
            poGeom = poMultiPoint->getGeometryRef(iPoint);
            if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
            {
                OGRPoint *poPoint = poGeom->toPoint();
                fp->WriteLine("%.15g %.15g\n",poPoint->getX(),poPoint->getY());
            }
            else
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "TABMultiPoint: Missing or Invalid Geometry!");
                return -1;
            }
        }
        // Write symbol
        fp->WriteLine("    Symbol (%d,%d,%d)\n",GetSymbolNo(),GetSymbolColor(),
                      GetSymbolSize());
    }

    return 0;
}

/**********************************************************************
 *
 **********************************************************************/
int TABCollection::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{
    /*-----------------------------------------------------------------
     * Fetch number of parts in "COLLECTION %d" line
     *----------------------------------------------------------------*/
    char **papszToken =
        CSLTokenizeString2(fp->GetLastLine(), " \t", CSLT_HONOURSTRINGS);

    if (CSLCount(papszToken) !=2)
    {
        CSLDestroy(papszToken);
        return -1;
    }

    int numParts = atoi(papszToken[1]);
    CSLDestroy(papszToken);
    papszToken = nullptr;

    // Make sure collection is empty
    EmptyCollection();

    const char *pszLine = fp->GetLine();

    /*-----------------------------------------------------------------
     * Read each part and add them to the feature
     *----------------------------------------------------------------*/
    for( int i=0; i < numParts; i++ )
    {
        if (pszLine == nullptr)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                  "Unexpected EOF while reading TABCollection from MIF file.");
            return -1;
         }

        while(*pszLine == ' ' || *pszLine == '\t')
            pszLine++;  // skip leading spaces

        if (*pszLine == '\0')
        {
            pszLine = fp->GetLine();
            continue;  // Skip blank lines
        }

        if (STARTS_WITH_CI(pszLine, "REGION"))
        {
            delete m_poRegion;
            m_poRegion = new TABRegion(GetDefnRef());
            if (m_poRegion->ReadGeometryFromMIFFile(fp) != 0)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "TABCollection: Error reading REGION part.");
                delete m_poRegion;
                m_poRegion = nullptr;
                return -1;
            }
        }
        else if (STARTS_WITH_CI(pszLine, "LINE") ||
                 STARTS_WITH_CI(pszLine, "PLINE"))
        {
            delete m_poPline;
            m_poPline = new TABPolyline(GetDefnRef());
            if (m_poPline->ReadGeometryFromMIFFile(fp) != 0)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "TABCollection: Error reading PLINE part.");
                delete m_poPline;
                m_poPline = nullptr;
                return -1;
            }
        }
        else if (STARTS_WITH_CI(pszLine, "MULTIPOINT"))
        {
            delete m_poMpoint;
            m_poMpoint = new TABMultiPoint(GetDefnRef());
            if (m_poMpoint->ReadGeometryFromMIFFile(fp) != 0)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "TABCollection: Error reading MULTIPOINT part.");
                delete m_poMpoint;
                m_poMpoint = nullptr;
                return -1;
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Reading TABCollection from MIF failed, expecting one "
                     "of REGION, PLINE or MULTIPOINT, got: '%s'",
                     pszLine);
            return -1;
        }

        pszLine = fp->GetLastLine();
    }

    /*-----------------------------------------------------------------
     * Set the main OGRFeature Geometry
     * (this is actually duplicating geometries from each member)
     *----------------------------------------------------------------*/
    // use addGeometry() rather than addGeometryDirectly() as this clones
    // the added geometry so won't leave dangling ptrs when the above features
    // are deleted

    OGRGeometryCollection *poGeomColl = new OGRGeometryCollection();
    if(m_poRegion && m_poRegion->GetGeometryRef() != nullptr)
        poGeomColl->addGeometry(m_poRegion->GetGeometryRef());

    if(m_poPline && m_poPline->GetGeometryRef() != nullptr)
        poGeomColl->addGeometry(m_poPline->GetGeometryRef());

    if(m_poMpoint && m_poMpoint->GetGeometryRef() != nullptr)
        poGeomColl->addGeometry(m_poMpoint->GetGeometryRef());

    OGREnvelope sEnvelope;
    poGeomColl->getEnvelope(&sEnvelope);
    this->SetGeometryDirectly(poGeomColl);

    SetMBR(sEnvelope.MinX, sEnvelope.MinY,
           sEnvelope.MaxX, sEnvelope.MaxY);

    return 0;
}

/**********************************************************************
 *
 **********************************************************************/
int TABCollection::WriteGeometryToMIFFile(MIDDATAFile *fp)
{
    int numParts = 0;
    if (m_poRegion)     numParts++;
    if (m_poPline)      numParts++;
    if (m_poMpoint)     numParts++;

    fp->WriteLine("COLLECTION %d\n", numParts);

    if (m_poRegion)
    {
        if (m_poRegion->WriteGeometryToMIFFile(fp) != 0)
            return -1;
    }

    if (m_poPline)
    {
        if (m_poPline->WriteGeometryToMIFFile(fp) != 0)
            return -1;
    }

    if (m_poMpoint)
    {
        if (m_poMpoint->WriteGeometryToMIFFile(fp) != 0)
            return -1;
    }

    return 0;
}

/**********************************************************************
 *
 **********************************************************************/
int TABDebugFeature::ReadGeometryFromMIFFile( MIDDATAFile *fp )
{
    // Go to the first line of the next feature.
    printf("%s\n", fp->GetLastLine());/*ok*/

    const char *pszLine = nullptr;
    while (((pszLine = fp->GetLine()) != nullptr) &&
           fp->IsValidFeature(pszLine) == FALSE)
    {}

    return 0;
}

/**********************************************************************
 *
 **********************************************************************/
int TABDebugFeature::WriteGeometryToMIFFile( MIDDATAFile * /* fp */ )
{
    return -1;
}
