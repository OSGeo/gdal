/**********************************************************************
 * $Id: mitab_feature_mif.cpp,v 1.39 2010-09-07 16:07:53 aboudreault Exp $
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
 **********************************************************************
 *
 * $Log: mitab_feature_mif.cpp,v $
 * Revision 1.39  2010-09-07 16:07:53  aboudreault
 * Added the use of OGRGeometryFactory::organizePolygons for mif features
 *
 * Revision 1.38  2010-07-07 19:00:15  aboudreault
 * Cleanup Win32 Compile Warnings (GDAL bug #2930)
 *
 * Revision 1.37  2008-12-17 14:55:20  aboudreault
 * Fixed mitab mif/mid importer fails when a Text geometry have an empty
 * text value (bug 1978)
 *
 * Revision 1.36  2008-11-27 20:50:22  aboudreault
 * Improved support for OGR date/time types. New Read/Write methods (bug 1948)
 * Added support of OGR date/time types for MIF features.
 *
 * Revision 1.35  2008/09/23 14:56:03  aboudreault
 * Fixed an error related to the " character when converting mif to tab file.
 *
 * Revision 1.34  2008/09/23 13:45:03  aboudreault
 * Fixed bug with the characters ",\n in the tab2tab application. (bug 1945)
 *
 * Revision 1.33  2008/02/01 20:30:59  dmorissette
 * Use %.15g instead of %.16g as number precision in .MIF output
 *
 * Revision 1.32  2007/06/07 20:27:21  dmorissette
 * Fixed memory leaks when reading multipoint objects from .MIF files
 *
 * Revision 1.31  2006/01/27 13:44:44  fwarmerdam
 * fixed Mills.mif reading, crash at file end
 *
 * Revision 1.30  2006/01/26 21:26:36  fwarmerdam
 * fixed bug with multi character delimeters in .mid file
 *
 * Revision 1.29  2005/10/04 19:36:10  dmorissette
 * Added support for reading collections from MIF files (bug 1126)
 *
 * Revision 1.28  2005/10/04 15:44:31  dmorissette
 * First round of support for Collection objects. Currently supports reading
 * from .TAB/.MAP and writing to .MIF. Still lacks symbol support and write
 * support. (Based in part on patch and docs from Jim Hope, bug 1126)
 *
 * Revision 1.27  2005/10/04 15:35:52  dmorissette
 * Fixed an instance of hardcoded delimiter (",") in WriteRecordToMIDFile()
 * (patch by KB Kieron, bug 1126)
 *
 * Revision 1.26  2005/07/14 16:15:05  jlacroix
 * \n and \ are now unescaped internally.
 *
 * Revision 1.25  2003/12/19 07:52:34  fwarmerdam
 * write 3d as 2d
 *
 * Revision 1.24  2002/11/27 22:51:52  daniel
 * Bug 1631:Do not produce an error if .mid data records end with a stray ','
 * Treat tabs (\t) as a blank space delimiter when reading .mif coordinates
 *
 * Revision 1.23  2002/10/29 21:09:20  warmerda
 * Ensure that a blank line in a mid file is treated as one field containing
 * an empty string.
 *
 * Revision 1.22  2002/04/26 14:16:49  julien
 * Finishing the implementation of Multipoint (support for MIF)
 *
 * Revision 1.21  2002/03/26 01:48:40  daniel
 * Added Multipoint object type (V650)
 *
 * Revision 1.20  2002/01/23 20:31:21  daniel
 * Fixed warning produced by CPLAssert() in non-DEBUG mode.
 *
 * Revision 1.19  2001/06/25 01:50:42  daniel
 * Fixed MIF Text object output: negative text angles were lost.  Also use
 * TABText::SetTextAngle() when reading MIF instead of setting class members
 * directly so that negative angles get converted to the [0..360] range.
 *
 * Revision 1.18  2001/02/28 07:15:09  daniel
 * Added support for text label line end point
 *
 * Revision 1.17  2001/01/22 16:03:58  warmerda
 * expanded tabs
 *
 * Revision 1.16  2000/10/03 19:29:51  daniel
 * Include OGR StyleString stuff (implemented by Stephane)
 *
 * Revision 1.15  2000/09/28 16:39:44  warmerda
 * avoid warnings for unused, and unitialized variables
 *
 * Revision 1.14  2000/09/19 17:23:53  daniel
 * Maintain and/or compute valid region and polyline center/label point
 *
 * Revision 1.13  2000/03/27 03:33:45  daniel
 * Treat SYMBOL line as optional when reading TABPoint
 *
 * Revision 1.12  2000/02/28 16:56:32  daniel
 * Support pen width in points (width values 11 to 2047)
 *
 * Revision 1.11  2000/01/15 22:30:44  daniel
 * Switch to MIT/X-Consortium OpenSource license
 *
 * Revision 1.10  2000/01/14 23:51:37  daniel
 * Fixed handling of "\n" in TABText strings... now the external interface
 * of the lib returns and expects escaped "\"+"n" as described in MIF specs
 *
 * Revision 1.9  1999/12/19 17:37:14  daniel
 * Fixed memory leaks
 *
 * Revision 1.8  1999/12/19 01:02:50  stephane
 * Add a test on the CENTER information
 *
 * Revision 1.7  1999/12/18 23:23:23  stephane
 * Change the format of the output double from %g to %.16g
 *
 * Revision 1.6  1999/12/18 08:22:57  daniel
 * Removed stray break statement in PLINE MULTIPLE write code
 *
 * Revision 1.5  1999/12/18 07:21:30  daniel
 * Fixed test on geometry type when writing OGRMultiLineStrings
 *
 * Revision 1.4  1999/12/18 07:11:57  daniel
 * Return regions as OGRMultiPolygons instead of multiple rings OGRPolygons
 *
 * Revision 1.3  1999/12/16 17:16:44  daniel
 * Use addRing/GeometryDirectly() (prevents leak), and rounded rectangles
 * always return real corner radius from file even if it is bigger than MBR
 *
 * Revision 1.2  1999/11/11 01:22:05  stephane
 * Remove DebugFeature call, Point Reading error, add IsValidFeature() to 
 * test correctly if we are on a feature
 *
 * Revision 1.1  1999/11/08 19:20:30  stephane
 * First version
 *
 * Revision 1.1  1999/11/08 04:16:07  stephane
 * First Revision
 *
 *
 **********************************************************************/

#include "mitab.h"
#include "mitab_utils.h"
#include <ctype.h>

/*=====================================================================
 *                      class TABFeature
 *====================================================================*/

/************************************************************************/
/*                            MIDTokenize()                             */
/*                                                                      */
/*      We implement a special tokenize function so we can handle       */
/*      multibyte delimeters (ie. MITAB bug 1266).                      */
/*                                                                      */
/*      http://bugzilla.maptools.org/show_bug.cgi?id=1266               */
/************************************************************************/
static char **MIDTokenize( const char *pszLine, const char *pszDelim )

{
    char **papszResult = NULL;
    int iChar, iTokenChar = 0, bInQuotes = FALSE;
    char *pszToken = (char *) CPLMalloc(strlen(pszLine)+1);
    int nDelimLen = strlen(pszDelim);

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
            pszToken[iTokenChar++] = '\0';
            papszResult = CSLAddString( papszResult, pszToken );
            
            iChar += strlen(pszDelim) - 1;
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
 *  This method is used to read the Record (Attributs) for all type of
 *  feature included in a mid/mif file.
 * 
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABFeature::ReadRecordFromMIDFile(MIDDATAFile *fp)
{
    const char       *pszLine;
    char            **papszToken;
    int               nFields,i;
    OGRFieldDefn        *poFDefn = NULL;
#ifdef MITAB_USE_OFTDATETIME
    int nYear, nMonth, nDay, nHour, nMin, nSec, nMS, nTZFlag;
    nYear = nMonth = nDay = nHour = nMin = nSec = nMS = nTZFlag = 0;
#endif

    nFields = GetFieldCount();
    
    pszLine = fp->GetLastLine();

    if (pszLine == NULL)
    {
        CPLError(CE_Failure, CPLE_FileIO,
               "Unexpected EOF while reading attribute record from MID file.");
        return -1;
    }

    papszToken = MIDTokenize( pszLine, fp->GetDelimiter() );

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

    for (i=0;i<nFields;i++)
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
                    SetField(i, nYear, nMonth, nDay, nHour, nMin, nSec, 0);
                }
                break;
            }
            case OFTDate:
            {
                if (strlen(papszToken[i]) == 8)
                {
                    sscanf(papszToken[i], "%4d%2d%2d", &nYear, &nMonth, &nDay);
                    SetField(i, nYear, nMonth, nDay, nHour, nMin, nSec, 0);
                }
                break;
            }
            case OFTDateTime:
            {
                if (strlen(papszToken[i]) == 17)
                {
                    sscanf(papszToken[i], "%4d%2d%2d%2d%2d%2d%3d",
                           &nYear, &nMonth, &nDay, &nHour, &nMin, &nSec, &nMS);
                    SetField(i, nYear, nMonth, nDay, nHour, nMin, nSec, 0);
                }
                break;
            }
#endif
  
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
 *  This methode is used to write the Record (Attributs) for all type
 *  of feature included in a mid file.
 *
 *  Return 0 on success, -1 on error
 **********************************************************************/
int TABFeature::WriteRecordToMIDFile(MIDDATAFile *fp)
{
    int                  iField, numFields;
    OGRFieldDefn        *poFDefn = NULL;
#ifdef MITAB_USE_OFTDATETIME
    char szBuffer[20];
    int nYear, nMonth, nDay, nHour, nMin, nSec, nMS, nTZFlag;
    nYear = nMonth = nDay = nHour = nMin = nSec = nMS = nTZFlag = 0;
#endif

    CPLAssert(fp);
    
    const char *delimiter = fp->GetDelimiter();

    numFields = GetFieldCount();

    for(iField=0; iField<numFields; iField++)
    {
        if (iField != 0)
          fp->WriteLine(delimiter);
        poFDefn = GetFieldDefnRef( iField );

        switch(poFDefn->GetType())
        {
          case OFTString: 
          {
            int nStringLen = strlen(GetFieldAsString(iField));
            char *pszString = (char*)CPLMalloc((nStringLen+1)*sizeof(char));
            strcpy(pszString, GetFieldAsString(iField));
            char *pszWorkString = (char*)CPLMalloc((2*(nStringLen)+1)*sizeof(char));
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
            CPLFree(pszString);
            pszString = (char*)CPLMalloc((strlen(pszWorkString)+1)*sizeof(char));
            strcpy(pszString, pszWorkString);
            CPLFree(pszWorkString);
            fp->WriteLine("\"%s\"",pszString);
            CPLFree(pszString);
            break;
          }
#ifdef MITAB_USE_OFTDATETIME
          case OFTTime:
          {
              if (!IsFieldSet(iField)) 
              {
                 szBuffer[0] = '\0';
              }
              else
              {
                  GetFieldAsDateTime(iField, &nYear, &nMonth, &nDay,
                                     &nHour, &nMin, &nSec, &nTZFlag);
                  sprintf(szBuffer, "%2.2d%2.2d%2.2d%3.3d", nHour, nMin, nSec, nMS);
              }
              fp->WriteLine("%s",szBuffer);
              break;
          }
          case OFTDate:
          {
              if (!IsFieldSet(iField)) 
              {
                 szBuffer[0] = '\0';
              }
              else
              {
                  GetFieldAsDateTime(iField, &nYear, &nMonth, &nDay,
                                     &nHour, &nMin, &nSec, &nTZFlag);
                  sprintf(szBuffer, "%4.4d%2.2d%2.2d", nYear, nMonth, nDay);
              }
              fp->WriteLine("%s",szBuffer);
              break;
          }
          case OFTDateTime:
          {
              if (!IsFieldSet(iField)) 
              {
                 szBuffer[0] = '\0';
              }
              else
              {
                  GetFieldAsDateTime(iField, &nYear, &nMonth, &nDay,
                                     &nHour, &nMin, &nSec, &nTZFlag);
                  sprintf(szBuffer, "%4.4d%2.2d%2.2d%2.2d%2.2d%2.2d%3.3d", 
                          nYear, nMonth, nDay, nHour, nMin, nSec, nMS);
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
    const char *pszLine;
    
    /* Go to the first line of the next feature */

    while (((pszLine = fp->GetLine()) != NULL) && 
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
    OGRGeometry         *poGeometry;
    
    char               **papszToken;
    const char *pszLine;
    double dfX,dfY;
    papszToken = CSLTokenizeString2(fp->GetSavedLine(), 
                                    " \t", CSLT_HONOURSTRINGS);
     
    if (CSLCount(papszToken) !=3)
    {
        CSLDestroy(papszToken);
        return -1;
    }
    
    dfX = fp->GetXTrans(atof(papszToken[1]));
    dfY = fp->GetYTrans(atof(papszToken[2]));

    CSLDestroy(papszToken);
    papszToken = NULL;

    // Read optional SYMBOL line...
    pszLine = fp->GetLastLine();
    if( pszLine != NULL )
        papszToken = CSLTokenizeStringComplex(pszLine," ,()\t",
                                              TRUE,FALSE);
    if (CSLCount(papszToken) == 4 && EQUAL(papszToken[0], "SYMBOL") )
    {
        SetSymbolNo((GInt16)atoi(papszToken[1]));
        SetSymbolColor((GInt32)atoi(papszToken[2]));
        SetSymbolSize((GInt16)atoi(papszToken[3]));
    }

    CSLDestroy(papszToken); 
    papszToken = NULL;

    // scan until we reach 1st line of next feature
    // Since SYMBOL is optional, we have to test IsValidFeature() on that
    // line as well.
    while (pszLine && fp->IsValidFeature(pszLine) == FALSE)
    {
        pszLine = fp->GetLine();
    }
    
    poGeometry = new OGRPoint(dfX, dfY);
    
    SetGeometryDirectly(poGeometry);

    SetMBR(dfX, dfY, dfX, dfY);
    

    return 0; 
}

/**********************************************************************
 *
 **********************************************************************/
int TABPoint::WriteGeometryToMIFFile(MIDDATAFile *fp)
{ 
    OGRGeometry         *poGeom;
    OGRPoint            *poPoint;
 
    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
        poPoint = (OGRPoint*)poGeom;
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
    OGRGeometry         *poGeometry;
    
    char               **papszToken;
    const char *pszLine;
    double dfX,dfY;
    papszToken = CSLTokenizeString2(fp->GetSavedLine(), 
                                    " \t", CSLT_HONOURSTRINGS);

    if (CSLCount(papszToken) !=3)
    {
        CSLDestroy(papszToken);
        return -1;
    }

    dfX = fp->GetXTrans(atof(papszToken[1]));
    dfY = fp->GetYTrans(atof(papszToken[2]));
    
    CSLDestroy(papszToken);
    
    papszToken = CSLTokenizeStringComplex(fp->GetLastLine()," ,()\t",
                                          TRUE,FALSE);

    if (CSLCount(papszToken) !=7)
    {
        CSLDestroy(papszToken);
        return -1;
    }
    
    SetSymbolNo((GInt16)atoi(papszToken[1]));
    SetSymbolColor((GInt32)atoi(papszToken[2]));
    SetSymbolSize((GInt16)atoi(papszToken[3]));
    SetFontName(papszToken[4]);
    SetFontStyleMIFValue(atoi(papszToken[5]));
    SetSymbolAngle(atof(papszToken[6]));

    CSLDestroy(papszToken);
    
    poGeometry = new OGRPoint(dfX, dfY);
    
    SetGeometryDirectly(poGeometry);

    SetMBR(dfX, dfY, dfX, dfY);

    /* Go to the first line of the next feature */

    while (((pszLine = fp->GetLine()) != NULL) && 
           fp->IsValidFeature(pszLine) == FALSE)
      ;
    return 0; 
}

/**********************************************************************
 *
 **********************************************************************/
int TABFontPoint::WriteGeometryToMIFFile(MIDDATAFile *fp)
{ 
    OGRGeometry         *poGeom;
    OGRPoint            *poPoint;
 
    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
        poPoint = (OGRPoint*)poGeom;
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
    OGRGeometry         *poGeometry;
    
    char               **papszToken;
    const char          *pszLine;
    double               dfX,dfY;

    papszToken = CSLTokenizeString2(fp->GetSavedLine(), 
                                    " \t", CSLT_HONOURSTRINGS);

    
    if (CSLCount(papszToken) !=3)
    {
        CSLDestroy(papszToken);
        return -1;
    }

    dfX = fp->GetXTrans(atof(papszToken[1]));
    dfY = fp->GetYTrans(atof(papszToken[2]));

    CSLDestroy(papszToken);
    
    papszToken = CSLTokenizeStringComplex(fp->GetLastLine()," ,()\t",
                                          TRUE,FALSE);
    if (CSLCount(papszToken) !=5)
    {
        
        CSLDestroy(papszToken);
        return -1;
    }
    
    SetFontName(papszToken[1]);
    SetSymbolColor((GInt32)atoi(papszToken[2]));
    SetSymbolSize((GInt16)atoi(papszToken[3]));
    m_nCustomStyle = (GByte)atoi(papszToken[4]);
    
    CSLDestroy(papszToken);
    
    poGeometry = new OGRPoint(dfX, dfY);
    
    SetGeometryDirectly(poGeometry);

    SetMBR(dfX, dfY, dfX, dfY);

    /* Go to the first line of the next feature */

    while (((pszLine = fp->GetLine()) != NULL) && 
           fp->IsValidFeature(pszLine) == FALSE)
      ;
 
    return 0; 

}

/**********************************************************************
 *
 **********************************************************************/
int TABCustomPoint::WriteGeometryToMIFFile(MIDDATAFile *fp)
{ 
    OGRGeometry         *poGeom;
    OGRPoint            *poPoint;
 
    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
        poPoint = (OGRPoint*)poGeom;
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
    const char          *pszLine;
    char               **papszToken;
    OGRLineString       *poLine;
    OGRMultiLineString  *poMultiLine;
    GBool                bMultiple = FALSE;
    int                  nNumPoints,nNumSec=0,i,j;
    OGREnvelope          sEnvelope;
    

    papszToken = CSLTokenizeString2(fp->GetLastLine(), 
                                    " \t", CSLT_HONOURSTRINGS);
    
    if (CSLCount(papszToken) < 1)
    {
        CSLDestroy(papszToken);
        return -1;
    }

    if (EQUALN(papszToken[0],"LINE",4))
    {
        if (CSLCount(papszToken) != 5)
          return -1;

        poLine = new OGRLineString();
        poLine->setNumPoints(2);
        poLine->setPoint(0, fp->GetXTrans(atof(papszToken[1])),
                         fp->GetYTrans(atof(papszToken[2])));
        poLine->setPoint(1, fp->GetXTrans(atof(papszToken[3])),
                         fp->GetYTrans(atof(papszToken[4])));
        SetGeometryDirectly(poLine);
        poLine->getEnvelope(&sEnvelope);
        SetMBR(sEnvelope.MinX, sEnvelope.MinY,sEnvelope.MaxX,sEnvelope.MaxY);
    }
    else if (EQUALN(papszToken[0],"PLINE",5))
    {
        switch (CSLCount(papszToken))
        {
          case 1:
            bMultiple = FALSE;
            pszLine = fp->GetLine();
            nNumPoints = atoi(pszLine);
            break;
          case 2:
            bMultiple = FALSE;
            nNumPoints = atoi(papszToken[1]);
            break;
          case 3:
            if (EQUALN(papszToken[1],"MULTIPLE",8))
            {
                bMultiple = TRUE;
                nNumSec = atoi(papszToken[2]);
                pszLine = fp->GetLine();
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
            if (EQUALN(papszToken[1],"MULTIPLE",8))
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
            for (j=0;j<nNumSec;j++)
            {
                poLine = new OGRLineString();
                if (j != 0)
                    nNumPoints = atoi(fp->GetLine());
                if (nNumPoints < 2)
                {
                    CPLError(CE_Failure, CPLE_FileIO,
                             "Invalid number of vertices (%d) in PLINE "
                             "MULTIPLE segment.", nNumPoints);
                    return -1;
                }
                poLine->setNumPoints(nNumPoints);
                for (i=0;i<nNumPoints;i++)
                {
                    CSLDestroy(papszToken);
                    papszToken = CSLTokenizeString2(fp->GetLine(), 
                                                    " \t", CSLT_HONOURSTRINGS);
                    poLine->setPoint(i,fp->GetXTrans(atof(papszToken[0])),
                                     fp->GetYTrans(atof(papszToken[1])));
                }
                if (poMultiLine->addGeometryDirectly(poLine) != OGRERR_NONE)
                {
                    CPLAssert(FALSE); // Just in case OGR is modified
                }
            }
            if (SetGeometryDirectly(poMultiLine) != OGRERR_NONE)
            {
                CPLAssert(FALSE); // Just in case OGR is modified
            }
            poMultiLine->getEnvelope(&sEnvelope);
            SetMBR(sEnvelope.MinX, sEnvelope.MinY,
                   sEnvelope.MaxX,sEnvelope.MaxY);
        }
        else
        {
            poLine = new OGRLineString();
            poLine->setNumPoints(nNumPoints);
            for (i=0;i<nNumPoints;i++)
            {
                CSLDestroy(papszToken);
                papszToken = CSLTokenizeString2(fp->GetLine(), 
                                                " \t", CSLT_HONOURSTRINGS);
    
                if (CSLCount(papszToken) != 2)
                  return -1;
                poLine->setPoint(i,fp->GetXTrans(atof(papszToken[0])),
                                 fp->GetYTrans(atof(papszToken[1])));
            }
            SetGeometryDirectly(poLine);
            poLine->getEnvelope(&sEnvelope);
            SetMBR(sEnvelope.MinX, sEnvelope.MinY,
                   sEnvelope.MaxX,sEnvelope.MaxY);
        }
    }    
    
    CSLDestroy(papszToken);
    papszToken = NULL;
    
    while (((pszLine = fp->GetLine()) != NULL) && 
           fp->IsValidFeature(pszLine) == FALSE)
    {
        papszToken = CSLTokenizeStringComplex(pszLine,"() ,",
                                              TRUE,FALSE);
        
        if (CSLCount(papszToken) >= 1)
        {
            if (EQUALN(papszToken[0],"PEN",3))
            {
                
                if (CSLCount(papszToken) == 4)
                {                   
                    SetPenWidthMIF(atoi(papszToken[1]));
                    SetPenPattern((GByte)atoi(papszToken[2]));
                    SetPenColor((GInt32)atoi(papszToken[3]));
                }
                
            }
            else if (EQUALN(papszToken[0],"SMOOTH",6))
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
    OGRGeometry   *poGeom;
    OGRMultiLineString *poMultiLine = NULL;
    OGRLineString *poLine = NULL;
    int nNumPoints,i;

  
    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString)
    {
        /*-------------------------------------------------------------
         * Simple polyline
         *------------------------------------------------------------*/
        poLine = (OGRLineString*)poGeom;
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
        poMultiLine = (OGRMultiLineString*)poGeom;
        numLines = poMultiLine->getNumGeometries();

        fp->WriteLine("PLINE MULTIPLE %d\n", numLines);

        for(iLine=0; iLine < numLines; iLine++)
        {
            poGeom = poMultiLine->getGeometryRef(iLine);
            if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbLineString)
            { 
                poLine = (OGRLineString*)poGeom;
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
    double               dX, dY;
    OGRLinearRing       *poRing;
    OGRGeometry         *poGeometry = NULL;
    OGRPolygon          **tabPolygons = NULL;
    int                  i,iSection, numLineSections=0;
    char               **papszToken;
    const char          *pszLine;
    OGREnvelope          sEnvelope;

    m_bSmooth = FALSE;
    /*=============================================================
     * REGION (Similar to PLINE MULTIPLE)
     *============================================================*/
    papszToken = CSLTokenizeString2(fp->GetLastLine(), 
                                    " \t", CSLT_HONOURSTRINGS);
    
    if (CSLCount(papszToken) ==2)
      numLineSections = atoi(papszToken[1]);
    CSLDestroy(papszToken);
    papszToken = NULL;

    if (numLineSections > 0) 
        tabPolygons = new OGRPolygon*[numLineSections];

    for(iSection=0; iSection<numLineSections; iSection++)
    {
        int     numSectionVertices = 0;

        tabPolygons[iSection] = new OGRPolygon();

        if ((pszLine = fp->GetLine()) != NULL)
        {
            numSectionVertices = atoi(pszLine);
        }

        poRing = new OGRLinearRing();
        poRing->setNumPoints(numSectionVertices);

        for(i=0; i<numSectionVertices; i++)
        {
            pszLine = fp->GetLine();
            if (pszLine)
            {
                papszToken = CSLTokenizeStringComplex(pszLine," ,\t",
                                                      TRUE,FALSE);
                if (CSLCount(papszToken) == 2)
                {              
                    dX = fp->GetXTrans(atof(papszToken[0]));
                    dY = fp->GetYTrans(atof(papszToken[1]));
                    poRing->setPoint(i, dX, dY);
                }
                CSLDestroy(papszToken);
                papszToken = NULL;
            }   
        }
        
        poRing->closeRings();

        tabPolygons[iSection]->addRingDirectly(poRing);

        if (numLineSections == 1)
            poGeometry = tabPolygons[iSection];
        
        poRing = NULL;
    }
  
    if (numLineSections > 1)
    {
        int isValidGeometry;
        const char* papszOptions[] = { "METHOD=DEFAULT", NULL };
        poGeometry = OGRGeometryFactory::organizePolygons( 
            (OGRGeometry**)tabPolygons, numLineSections, &isValidGeometry, papszOptions );

        if (!isValidGeometry)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Geometry of polygon cannot be translated to Simple Geometry. "
                     "All polygons will be contained in a multipolygon.\n");
        }
    }

    if (tabPolygons)
        delete[] tabPolygons;

    SetGeometryDirectly(poGeometry);
    poGeometry->getEnvelope(&sEnvelope);
    
    SetMBR(sEnvelope.MinX, sEnvelope.MinY, sEnvelope.MaxX, sEnvelope.MaxY);

    while (((pszLine = fp->GetLine()) != NULL) && 
           fp->IsValidFeature(pszLine) == FALSE)
    {
        papszToken = CSLTokenizeStringComplex(pszLine,"() ,",
                                              TRUE,FALSE);
        
        if (CSLCount(papszToken) > 1)
        {
            if (EQUALN(papszToken[0],"PEN",3))
            {
                
                if (CSLCount(papszToken) == 4)
                {           
                    SetPenWidthMIF(atoi(papszToken[1]));
                    SetPenPattern((GByte)atoi(papszToken[2]));
                    SetPenColor((GInt32)atoi(papszToken[3]));
                }
                
            }
            else if (EQUALN(papszToken[0],"BRUSH", 5))
            {
                if (CSLCount(papszToken) >= 3)
                {
                    SetBrushFGColor((GInt32)atoi(papszToken[2]));
                    SetBrushPattern((GByte)atoi(papszToken[1]));
                    
                    if (CSLCount(papszToken) == 4)
                       SetBrushBGColor(atoi(papszToken[3]));
                    else
                      SetBrushTransparent(TRUE);
                }
                
            }
            else if (EQUALN(papszToken[0],"CENTER",6))
            {
                if (CSLCount(papszToken) == 3)
                {
                    SetCenter(fp->GetXTrans(atof(papszToken[1])),
                              fp->GetYTrans(atof(papszToken[2])) );
                }
            }
        }
        CSLDestroy(papszToken);
        papszToken = NULL;
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
    OGRGeometry         *poGeom;

    poGeom = GetGeometryRef();

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
            OGRLinearRing       *poRing;

            poRing = GetRingRef(iRing);
            if (poRing == NULL)
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
    const char          *pszLine;
    char               **papszToken;
    double               dXMin, dYMin, dXMax, dYMax;
    OGRPolygon          *poPolygon;
    OGRLinearRing       *poRing;

    papszToken = CSLTokenizeString2(fp->GetLastLine(), 
                                    " \t", CSLT_HONOURSTRINGS);

    if (CSLCount(papszToken) <  5)
    {
        CSLDestroy(papszToken);
        return -1;
    }

    dXMin = fp->GetXTrans(atof(papszToken[1]));
    dXMax = fp->GetXTrans(atof(papszToken[3]));
    dYMin = fp->GetYTrans(atof(papszToken[2]));
    dYMax = fp->GetYTrans(atof(papszToken[4]));
    
    /*-----------------------------------------------------------------
     * Call SetMBR() and GetMBR() now to make sure that min values are
     * really smaller than max values.
     *----------------------------------------------------------------*/
    SetMBR(dXMin, dYMin, dXMax, dYMax);
    GetMBR(dXMin, dYMin, dXMax, dYMax);
    
    m_bRoundCorners = FALSE;
    m_dRoundXRadius  = 0.0;
    m_dRoundYRadius  = 0.0;
    
    if (EQUALN(papszToken[0],"ROUNDRECT",9))
    {
        m_bRoundCorners = TRUE;
        if (CSLCount(papszToken) == 6)
          m_dRoundXRadius = m_dRoundYRadius = atof(papszToken[5])/2.0;
        else
        {
            CSLDestroy(papszToken);
            papszToken = CSLTokenizeString2(fp->GetLine(), 
                                            " \t", CSLT_HONOURSTRINGS);
            if (CSLCount(papszToken) !=1 )
              m_dRoundXRadius = m_dRoundYRadius = atof(papszToken[1])/2.0;
        }
    }
    CSLDestroy(papszToken);
    papszToken = NULL;

    /*-----------------------------------------------------------------
     * Create and fill geometry object
     *----------------------------------------------------------------*/
        
    poPolygon = new OGRPolygon;
    poRing = new OGRLinearRing();
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
        double dXRadius = MIN(m_dRoundXRadius, (dXMax-dXMin)/2.0);
        double dYRadius = MIN(m_dRoundYRadius, (dYMax-dYMin)/2.0);
        TABGenerateArc(poRing, 45, 
                       dXMin + dXRadius, dYMin + dYRadius, dXRadius, dYRadius,
                       PI, 3.0*PI/2.0);
        TABGenerateArc(poRing, 45, 
                       dXMax - dXRadius, dYMin + dYRadius, dXRadius, dYRadius,
                       3.0*PI/2.0, 2.0*PI);
        TABGenerateArc(poRing, 45, 
                       dXMax - dXRadius, dYMax - dYRadius, dXRadius, dYRadius,
                       0.0, PI/2.0);
        TABGenerateArc(poRing, 45, 
                       dXMin + dXRadius, dYMax - dYRadius, dXRadius, dYRadius,
                       PI/2.0, PI);
                       
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


   while (((pszLine = fp->GetLine()) != NULL) && 
          fp->IsValidFeature(pszLine) == FALSE)
   {
       papszToken = CSLTokenizeStringComplex(pszLine,"() ,",
                                             TRUE,FALSE);

       if (CSLCount(papszToken) > 1)
       {
           if (EQUALN(papszToken[0],"PEN",3))
           {       
               if (CSLCount(papszToken) == 4)
               {   
                   SetPenWidthMIF(atoi(papszToken[1]));
                   SetPenPattern((GByte)atoi(papszToken[2]));
                   SetPenColor((GInt32)atoi(papszToken[3]));
               }
              
           }
           else if (EQUALN(papszToken[0],"BRUSH", 5))
           {
               if (CSLCount(papszToken) >=3)
               {
                   SetBrushFGColor((GInt32)atoi(papszToken[2]));
                   SetBrushPattern((GByte)atoi(papszToken[1]));

                   if (CSLCount(papszToken) == 4)
                       SetBrushBGColor(atoi(papszToken[3]));
                   else
                      SetBrushTransparent(TRUE);
               }
              
           }
       }
       CSLDestroy(papszToken);
       papszToken = NULL;
   }
 
   return 0; 

}    


/**********************************************************************
 *
 **********************************************************************/
int TABRectangle::WriteGeometryToMIFFile(MIDDATAFile *fp)
{ 
    OGRGeometry         *poGeom;
    OGRPolygon          *poPolygon;
    OGREnvelope         sEnvelope;
    
     /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPolygon)
        poPolygon = (OGRPolygon*)poGeom;
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
    const char *pszLine;
    char **papszToken;
    double              dXMin, dYMin, dXMax, dYMax;
    OGRPolygon          *poPolygon;
    OGRLinearRing       *poRing;

    papszToken = CSLTokenizeString2(fp->GetLastLine(), 
                                    " \t", CSLT_HONOURSTRINGS);

    if (CSLCount(papszToken) != 5)
    {
        CSLDestroy(papszToken);
        return -1;
    }

    dXMin = fp->GetXTrans(atof(papszToken[1]));
    dXMax = fp->GetXTrans(atof(papszToken[3]));
    dYMin = fp->GetYTrans(atof(papszToken[2]));
    dYMax = fp->GetYTrans(atof(papszToken[4]));

    CSLDestroy(papszToken);
    papszToken = NULL;

     /*-----------------------------------------------------------------
     * Save info about the ellipse def. inside class members
     *----------------------------------------------------------------*/
    m_dCenterX = (dXMin + dXMax) / 2.0;
    m_dCenterY = (dYMin + dYMax) / 2.0;
    m_dXRadius = ABS( (dXMax - dXMin) / 2.0 );
    m_dYRadius = ABS( (dYMax - dYMin) / 2.0 );

    SetMBR(dXMin, dYMin, dXMax, dYMax);

    /*-----------------------------------------------------------------
     * Create and fill geometry object
     *----------------------------------------------------------------*/
    poPolygon = new OGRPolygon;
    poRing = new OGRLinearRing();

    /*-----------------------------------------------------------------
     * For the OGR geometry, we generate an ellipse with 2 degrees line
     * segments.
     *----------------------------------------------------------------*/
    TABGenerateArc(poRing, 180, 
                   m_dCenterX, m_dCenterY,
                   m_dXRadius, m_dYRadius,
                   0.0, 2.0*PI);
    TABCloseRing(poRing);

    poPolygon->addRingDirectly(poRing);
    SetGeometryDirectly(poPolygon);

    while (((pszLine = fp->GetLine()) != NULL) && 
           fp->IsValidFeature(pszLine) == FALSE)
    {
        papszToken = CSLTokenizeStringComplex(pszLine,"() ,",
                                              TRUE,FALSE);
        
        if (CSLCount(papszToken) > 1)
        {
            if (EQUALN(papszToken[0],"PEN",3))
            {       
                if (CSLCount(papszToken) == 4)
                {   
                    SetPenWidthMIF(atoi(papszToken[1]));
                    SetPenPattern((GByte)atoi(papszToken[2]));
                    SetPenColor((GInt32)atoi(papszToken[3]));
                }
                
            }
            else if (EQUALN(papszToken[0],"BRUSH", 5))
            {
                if (CSLCount(papszToken) >= 3)
                {
                    SetBrushFGColor((GInt32)atoi(papszToken[2]));
                    SetBrushPattern((GByte)atoi(papszToken[1]));
                    
                    if (CSLCount(papszToken) == 4)
                      SetBrushBGColor(atoi(papszToken[3]));
                    else
                      SetBrushTransparent(TRUE);
                    
                }
                
            }
        }
        CSLDestroy(papszToken);
        papszToken = NULL;
    }
    return 0; 
}

/**********************************************************************
 *
 **********************************************************************/
int TABEllipse::WriteGeometryToMIFFile(MIDDATAFile *fp)
{
    OGRGeometry         *poGeom;
    OGREnvelope         sEnvelope;
 
    poGeom = GetGeometryRef();
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
    const char          *pszLine;
    OGRLineString       *poLine;
    char               **papszToken;
    double               dXMin,dXMax, dYMin,dYMax;
    int                  numPts;
    
    papszToken = CSLTokenizeString2(fp->GetLastLine(), 
                                    " \t", CSLT_HONOURSTRINGS);

    if (CSLCount(papszToken) == 5)
    {
        dXMin = fp->GetXTrans(atof(papszToken[1]));
        dXMax = fp->GetXTrans(atof(papszToken[3]));
        dYMin = fp->GetYTrans(atof(papszToken[2]));
        dYMax = fp->GetYTrans(atof(papszToken[4]));

        CSLDestroy(papszToken);
        papszToken = CSLTokenizeString2(fp->GetLine(), 
                                        " \t", CSLT_HONOURSTRINGS);
        if (CSLCount(papszToken) != 2)
        {
            CSLDestroy(papszToken);
            return -1;
        }

        m_dStartAngle = atof(papszToken[0]);
        m_dEndAngle = atof(papszToken[1]);
    }
    else if (CSLCount(papszToken) == 7)
    {
        dXMin = fp->GetXTrans(atof(papszToken[1]));
        dXMax = fp->GetXTrans(atof(papszToken[3]));
        dYMin = fp->GetYTrans(atof(papszToken[2]));
        dYMax = fp->GetYTrans(atof(papszToken[4]));
        m_dStartAngle = atof(papszToken[5]);
        m_dEndAngle = atof(papszToken[6]);
    }
    else
    {
        CSLDestroy(papszToken);
        return -1;
    }

    CSLDestroy(papszToken);
    papszToken = NULL;

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
    m_dXRadius = ABS( (dXMax - dXMin) / 2.0 );
    m_dYRadius = ABS( (dYMax - dYMin) / 2.0 );

    /*-----------------------------------------------------------------
     * Create and fill geometry object
     * For the OGR geometry, we generate an arc with 2 degrees line
     * segments.
     *----------------------------------------------------------------*/
    poLine = new OGRLineString;

    if (m_dEndAngle < m_dStartAngle)
        numPts = (int) ABS( ((m_dEndAngle+360.0)-m_dStartAngle)/2.0 ) + 1;
    else
        numPts = (int) ABS( (m_dEndAngle-m_dStartAngle)/2.0 ) + 1;
    numPts = MAX(2, numPts);

    TABGenerateArc(poLine, numPts,
                   m_dCenterX, m_dCenterY,
                   m_dXRadius, m_dYRadius,
                   m_dStartAngle*PI/180.0, m_dEndAngle*PI/180.0);

    SetMBR(dXMin, dYMin, dXMax, dYMax);
    SetGeometryDirectly(poLine);

    while (((pszLine = fp->GetLine()) != NULL) && 
           fp->IsValidFeature(pszLine) == FALSE)
    {
        papszToken = CSLTokenizeStringComplex(pszLine,"() ,",
                                              TRUE,FALSE);
        
        if (CSLCount(papszToken) > 1)
        {
            if (EQUALN(papszToken[0],"PEN",3))
            {
                
                if (CSLCount(papszToken) == 4)
                {    
                    SetPenWidthMIF(atoi(papszToken[1]));
                    SetPenPattern((GByte)atoi(papszToken[2]));
                    SetPenColor((GInt32)atoi(papszToken[3]));
                }
                
            }
        }
        CSLDestroy(papszToken);
        papszToken = NULL;
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
    double               dXMin, dYMin, dXMax, dYMax;
    OGRGeometry         *poGeometry;
    const char          *pszLine;
    char               **papszToken;
    const char          *pszString;
    char                *pszTmpString;
    int                  bXYBoxRead = 0;
    int                  tokenLen;

    papszToken = CSLTokenizeString2(fp->GetLastLine(), 
                                    " \t", CSLT_HONOURSTRINGS);
    if (CSLCount(papszToken) == 1)
    {
        CSLDestroy(papszToken);
        papszToken = CSLTokenizeString2(fp->GetLine(), 
                                        " \t", CSLT_HONOURSTRINGS);
        tokenLen = CSLCount(papszToken);
        if (tokenLen == 4)
        {
           pszString = NULL;
           bXYBoxRead = 1;
        }
        else if (tokenLen == 0)
        {
            pszString = NULL;
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
    pszTmpString = CPLStrdup(pszString);
    m_pszString = TABUnEscapeString(pszTmpString, TRUE);
    if (pszTmpString != m_pszString)
        CPLFree(pszTmpString);

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
    else
    {
        dXMin = fp->GetXTrans(atof(papszToken[0]));
        dXMax = fp->GetXTrans(atof(papszToken[2]));
        dYMin = fp->GetYTrans(atof(papszToken[1]));
        dYMax = fp->GetYTrans(atof(papszToken[3]));

        m_dHeight = dYMax - dYMin;  //SetTextBoxHeight(dYMax - dYMin);
        m_dWidth  = dXMax - dXMin;  //SetTextBoxWidth(dXMax - dXMin);
        
        if (m_dHeight <0.0)
          m_dHeight*=-1.0;
        if (m_dWidth <0.0)
          m_dWidth*=-1.0;
    }

    CSLDestroy(papszToken);
    papszToken = NULL;

    /* Set/retrieve the MBR to make sure Mins are smaller than Maxs
     */

    SetMBR(dXMin, dYMin, dXMax, dYMax);
    GetMBR(dXMin, dYMin, dXMax, dYMax);
    
    while (((pszLine = fp->GetLine()) != NULL) && 
           fp->IsValidFeature(pszLine) == FALSE)
    {
        papszToken = CSLTokenizeStringComplex(pszLine,"() ,",
                                              TRUE,FALSE);
        
        if (CSLCount(papszToken) > 1)
        {
            if (EQUALN(papszToken[0],"FONT",4))
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
            else if (EQUALN(papszToken[0],"SPACING",7))
            {
                if (CSLCount(papszToken) >= 2)
                {   
                    if (EQUALN(papszToken[1],"2",1))
                    {
                        SetTextSpacing(TABTSDouble);
                    }
                    else if (EQUALN(papszToken[1],"1.5",3))
                    {
                        SetTextSpacing(TABTS1_5);
                    }
                }
                
                if (CSLCount(papszToken) == 7)
                {
                    if (EQUALN(papszToken[2],"LAbel",5))
                    {
                        if (EQUALN(papszToken[4],"simple",6))
                        {
                            SetTextLineType(TABTLSimple);
                            SetTextLineEndPoint(fp->GetXTrans(atof(papszToken[5])),
                                                fp->GetYTrans(atof(papszToken[6])));
                        }
                        else if (EQUALN(papszToken[4],"arrow", 5))
                        {
                            SetTextLineType(TABTLArrow);
                            SetTextLineEndPoint(fp->GetXTrans(atof(papszToken[5])),
                                                fp->GetYTrans(atof(papszToken[6])));
                        }
                    }
                }               
            }
            else if (EQUALN(papszToken[0],"Justify",7))
            {
                if (CSLCount(papszToken) == 2)
                {
                    if (EQUALN( papszToken[1],"Center",6))
                    {
                        SetTextJustification(TABTJCenter);
                    }
                    else  if (EQUALN( papszToken[1],"Right",5))
                    {
                        SetTextJustification(TABTJRight);
                    }
                    
                }
                
            }
            else if (EQUALN(papszToken[0],"Angle",5))
            {
                if (CSLCount(papszToken) == 2)
                {    
                    SetTextAngle(atof(papszToken[1]));
                }
                
            }
            else if (EQUALN(papszToken[0],"LAbel",5))
            {
                if (CSLCount(papszToken) == 5)
                {    
                    if (EQUALN(papszToken[2],"simple",6))
                    {
                        SetTextLineType(TABTLSimple);
                        SetTextLineEndPoint(fp->GetXTrans(atof(papszToken[3])),
                                           fp->GetYTrans(atof(papszToken[4])));
                    }
                    else if (EQUALN(papszToken[2],"arrow", 5))
                    {
                        SetTextLineType(TABTLArrow);
                        SetTextLineEndPoint(fp->GetXTrans(atof(papszToken[3])),
                                           fp->GetYTrans(atof(papszToken[4])));
                    }
                }
                

                // What I do with the XY coordonate
            }
        }
        CSLDestroy(papszToken);
        papszToken = NULL;
    }
    /*-----------------------------------------------------------------
     * Create an OGRPoint Geometry... 
     * The point X,Y values will be the coords of the lower-left corner before
     * rotation is applied.  (Note that the rotation in MapInfo is done around
     * the upper-left corner)
     * We need to calculate the true lower left corner of the text based
     * on the MBR after rotation, the text height and the rotation angle.
     *---------------------------------------------------------------- */
    double dCos, dSin, dX, dY;
    dSin = sin(m_dAngle*PI/180.0);
    dCos = cos(m_dAngle*PI/180.0);
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
    
    
    poGeometry = new OGRPoint(dX, dY);

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
    dSin = ABS(dSin);
    dCos = ABS(dCos);
    if (m_dHeight == 0.0)
        m_dWidth = 0.0;
    else if ( dCos > dSin )
        m_dWidth = m_dHeight * ((dXMax-dXMin) - m_dHeight*dSin) / 
                                                        (m_dHeight*dCos);
    else
        m_dWidth = m_dHeight * ((dYMax-dYMin) - m_dHeight*dCos) /
                                                        (m_dHeight*dSin);
    m_dWidth = ABS(m_dWidth);
    
   return 0; 
}

/**********************************************************************
 *
 **********************************************************************/
int TABText::WriteGeometryToMIFFile(MIDDATAFile *fp)
{
    double dXMin,dYMin,dXMax,dYMax;
    char   *pszTmpString;

    /*-------------------------------------------------------------
     * Note: The text string may contain unescaped "\n" chars or 
     * "\\" chars and we expect to receive them in an unescaped 
     * form. Those characters are unescaped in memory to be like
     * other OGR drivers. See MapTools bug 1107 for more details.
     *------------------------------------------------------------*/
    pszTmpString = TABEscapeString(m_pszString);
    if(pszTmpString == NULL)
        fp->WriteLine("Text \"\"\n" );
    else
        fp->WriteLine("Text \"%s\"\n", pszTmpString );
    if (pszTmpString != m_pszString)
        CPLFree(pszTmpString);

    //    UpdateTextMBR();
    GetMBR(dXMin, dYMin, dXMax, dYMax);
    fp->WriteLine("    %.15g %.15g %.15g %.15g\n",dXMin, dYMin,dXMax, dYMax); 
 
    if (IsFontBGColorUsed())
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

    if (ABS(GetTextAngle()) >  0.000001)
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
    OGRPoint            *poPoint;
    OGRMultiPoint       *poMultiPoint;
    char                **papszToken;
    const char          *pszLine;
    int                 nNumPoint, i;
    double              dfX,dfY;
    OGREnvelope         sEnvelope;

    papszToken = CSLTokenizeString2(fp->GetLastLine(), 
                                    " \t", CSLT_HONOURSTRINGS);
     
    if (CSLCount(papszToken) !=2)
    {
        CSLDestroy(papszToken);
        return -1;
    }
    
    nNumPoint = atoi(papszToken[1]);
    poMultiPoint = new OGRMultiPoint;

    CSLDestroy(papszToken);
    papszToken = NULL;

    // Get each point and add them to the multipoint feature
    for(i=0; i<nNumPoint; i++)
    {
        pszLine = fp->GetLine();
        papszToken = CSLTokenizeString2(fp->GetLastLine(), 
                                        " \t", CSLT_HONOURSTRINGS);
        if (CSLCount(papszToken) !=2)
        {
            CSLDestroy(papszToken);
            return -1;
        }

        dfX = fp->GetXTrans(atof(papszToken[0]));
        dfY = fp->GetXTrans(atof(papszToken[1]));
        poPoint = new OGRPoint(dfX, dfY);
        if ( poMultiPoint->addGeometryDirectly( poPoint ) != OGRERR_NONE)
        {
            CPLAssert(FALSE); // Just in case OGR is modified
        }

        // Set center
        if(i == 0)
        {
            SetCenter( dfX, dfY );
        }
        CSLDestroy(papszToken);
    }

    if( SetGeometryDirectly( poMultiPoint ) != OGRERR_NONE)
    {
        CPLAssert(FALSE); // Just in case OGR is modified
    }

    poMultiPoint->getEnvelope(&sEnvelope);
    SetMBR(sEnvelope.MinX, sEnvelope.MinY,
           sEnvelope.MaxX,sEnvelope.MaxY);

    // Read optional SYMBOL line...

    while (((pszLine = fp->GetLine()) != NULL) && 
           fp->IsValidFeature(pszLine) == FALSE)
    {
        papszToken = CSLTokenizeStringComplex(pszLine," ,()\t",
                                              TRUE,FALSE);
        if (CSLCount(papszToken) == 4 && EQUAL(papszToken[0], "SYMBOL") )
        {
            SetSymbolNo((GInt16)atoi(papszToken[1]));
            SetSymbolColor((GInt32)atoi(papszToken[2]));
            SetSymbolSize((GInt16)atoi(papszToken[3]));
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
    OGRGeometry         *poGeom;
    OGRPoint            *poPoint;
    OGRMultiPoint       *poMultiPoint;
    int                 nNumPoints, iPoint;
 
    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbMultiPoint)
    {
        poMultiPoint = (OGRMultiPoint*)poGeom;
        nNumPoints = poMultiPoint->getNumGeometries();

        fp->WriteLine("MultiPoint %d\n", nNumPoints);

        for(iPoint=0; iPoint < nNumPoints; iPoint++)
        {
            /*------------------------------------------------------------
             * Validate each point
             *-----------------------------------------------------------*/
            poGeom = poMultiPoint->getGeometryRef(iPoint);
            if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
            { 
                poPoint = (OGRPoint*)poGeom;
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
    char                **papszToken;
    const char          *pszLine;
    int                 numParts, i;
    OGREnvelope         sEnvelope;

    /*-----------------------------------------------------------------
     * Fetch number of parts in "COLLECTION %d" line
     *----------------------------------------------------------------*/
    papszToken = CSLTokenizeString2(fp->GetLastLine(), 
                                    " \t", CSLT_HONOURSTRINGS);
     
    if (CSLCount(papszToken) !=2)
    {
        CSLDestroy(papszToken);
        return -1;
    }
    
    numParts = atoi(papszToken[1]);
    CSLDestroy(papszToken);
    papszToken = NULL;

    // Make sure collection is empty
    EmptyCollection();

    pszLine = fp->GetLine();

    /*-----------------------------------------------------------------
     * Read each part and add them to the feature
     *----------------------------------------------------------------*/
    for (i=0; i < numParts; i++)
    {
        if (pszLine == NULL)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                  "Unexpected EOF while reading TABCollection from MIF file.");
            return -1;
         }

        while(*pszLine == ' ' || *pszLine == '\t')
            pszLine++;  // skip leading spaces

        if (*pszLine == '\0')
            continue;  // Skip blank lines

        if (EQUALN(pszLine,"REGION",6))
        {
            m_poRegion = new TABRegion(GetDefnRef());
            if (m_poRegion->ReadGeometryFromMIFFile(fp) != 0)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "TABCollection: Error reading REGION part.");
                delete m_poRegion;
                m_poRegion = NULL;
                return -1;
            }
        }  
        else if (EQUALN(pszLine,"LINE",4) ||
                 EQUALN(pszLine,"PLINE",5))
        {
            m_poPline = new TABPolyline(GetDefnRef());
            if (m_poPline->ReadGeometryFromMIFFile(fp) != 0)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "TABCollection: Error reading PLINE part.");
                delete m_poPline;
                m_poPline = NULL;
                return -1;
            }
        }
        else if (EQUALN(pszLine,"MULTIPOINT",10))
        {
            m_poMpoint = new TABMultiPoint(GetDefnRef());
            if (m_poMpoint->ReadGeometryFromMIFFile(fp) != 0)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "TABCollection: Error reading MULTIPOINT part.");
                delete m_poMpoint;
                m_poMpoint = NULL;
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
    if(m_poRegion && m_poRegion->GetGeometryRef() != NULL)
        poGeomColl->addGeometry(m_poRegion->GetGeometryRef());
    
    if(m_poPline && m_poPline->GetGeometryRef() != NULL)
        poGeomColl->addGeometry(m_poPline->GetGeometryRef());

    if(m_poMpoint && m_poMpoint->GetGeometryRef() != NULL)
        poGeomColl->addGeometry(m_poMpoint->GetGeometryRef());

    this->SetGeometryDirectly(poGeomColl);

    poGeomColl->getEnvelope(&sEnvelope);
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
int TABDebugFeature::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{ 
   const char *pszLine;
  
   
  /* Go to the first line of the next feature */
   printf("%s\n", fp->GetLastLine());

   while (((pszLine = fp->GetLine()) != NULL) && 
          fp->IsValidFeature(pszLine) == FALSE)
     ;
  
   return 0; 
}


/**********************************************************************
 *
 **********************************************************************/
int TABDebugFeature::WriteGeometryToMIFFile(CPL_UNUSED MIDDATAFile *fp){ return -1; }
