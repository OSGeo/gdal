/**********************************************************************
 * $Id: mitab_feature_mif.cpp,v 1.2 1999/11/11 01:22:05 stephane Exp $
 *
 * Name:     mitab_feature.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation of R/W Fcts for (Mid/Mif) in feature classes 
 *           specific to MapInfo files.
 * Author:   Stephane Villeneuve, s.villeneuve@videotron.ca
 *
 **********************************************************************
 * Copyright (c) 1999, Daniel Morissette
 *
 * All rights reserved.  This software may be copied or reproduced, in
 * all or in part, without the prior written consent of its author,
 * Daniel Morissette (danmo@videotron.ca).  However, any material copied
 * or reproduced must bear the original copyright notice (above), this 
 * original paragraph, and the original disclaimer (below).
 * 
 * The entire risk as to the results and performance of the software,
 * supporting text and other information contained in this file
 * (collectively called the "Software") is with the user.  Although 
 * considerable efforts have been used in preparing the Software, the 
 * author does not warrant the accuracy or completeness of the Software.
 * In no event will the author be liable for damages, including loss of
 * profits or consequential damages, arising out of the use of the 
 * Software.
 * 
 **********************************************************************
 *
 * $Log: mitab_feature_mif.cpp,v $
 * Revision 1.2  1999/11/11 01:22:05  stephane
 * Remove DebugFeature call, Point Reading error, add IsValidFeature() to test correctly if we are on a feature
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
/**********************************************************************
 **********************************************************************/
/**********************************************************************
 *                   TABFeature::ReadRecordFromMIDFile()
 *
 *  This methode is used to read the Record (Attributs) for all type of
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
    OGRFieldDefn     *poFDefn  = NULL;

    nFields = GetFieldCount();
    
    pszLine = fp->GetLastLine();

    papszToken = CSLTokenizeStringComplex(pszLine,
					  fp->GetDelimiter(),TRUE,TRUE); 
    if (CSLCount(papszToken) != nFields)
    {
	CSLDestroy(papszToken);
	return -1;
    }

    for (i=0;i<nFields;i++)
    {
	SetField(i,papszToken[i]);
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
    OGRFeatureDefn      *poDefn = GetDefnRef();
    OGRFieldDefn	*poFDefn = NULL;

    CPLAssert(fp);
    
    numFields = GetFieldCount();

    for(iField=0; iField<numFields; iField++)
    {
	if (iField != 0)
	  fp->WriteLine(",");
	poFDefn = GetFieldDefnRef( iField );

        switch(poFDefn->GetType())
        {
	  case OFTString:
	    fp->WriteLine("\"%s\"",GetFieldAsString(iField));
            break;	    
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

int TABPoint::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{  
    OGRGeometry         *poGeometry;
    
    char               **papszToken;
    const char *pszLine;
    double dfX,dfY;
    papszToken = CSLTokenizeString(fp->GetSavedLine());
     
    if (CSLCount(papszToken) !=3)
    {
	CSLDestroy(papszToken);
	return -1;
    }
    
    dfX = fp->GetXTrans(atof(papszToken[1]));
    dfY = fp->GetYTrans(atof(papszToken[2]));

    CSLDestroy(papszToken);

    papszToken = CSLTokenizeStringComplex(fp->GetLastLine()," ,()",
					  TRUE,FALSE);
    if (CSLCount(papszToken) !=4)
    {
	CSLDestroy(papszToken);
	return -1;
    }

    SetSymbolNo(atoi(papszToken[1]));
    SetSymbolColor(atoi(papszToken[2]));
    SetSymbolSize(atoi(papszToken[3]));

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

int TABPoint::WriteGeometryToMIFFile(MIDDATAFile *fp)
{ 
    OGRGeometry         *poGeom;
    OGRPoint            *poPoint;
 
    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && poGeom->getGeometryType() == wkbPoint)
        poPoint = (OGRPoint*)poGeom;
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABPoint: Missing or Invalid Geometry!");
        return -1;
    }

    fp->WriteLine("Point %g %g\n",poPoint->getX(),poPoint->getY());
    fp->WriteLine("    Symbol (%d,%d,%d)\n",GetSymbolNo(),GetSymbolColor(),
		  GetSymbolSize());

    return 0; 
}

int TABFontPoint::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{   
    OGRGeometry         *poGeometry;
    
    char               **papszToken;
    const char *pszLine;
    double dfX,dfY;
    papszToken = CSLTokenizeString(fp->GetSavedLine());

    if (CSLCount(papszToken) !=3)
    {
	CSLDestroy(papszToken);
	return -1;
    }

    dfX = fp->GetXTrans(atof(papszToken[1]));
    dfY = fp->GetYTrans(atof(papszToken[2]));
    
    CSLDestroy(papszToken);
    
    papszToken = CSLTokenizeStringComplex(fp->GetLastLine()," ,()",
					  TRUE,FALSE);

    if (CSLCount(papszToken) !=7)
    {
	CSLDestroy(papszToken);
	return -1;
    }
    
    SetSymbolNo(atoi(papszToken[1]));
    SetSymbolColor(atoi(papszToken[2]));
    SetSymbolSize(atoi(papszToken[3]));
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

int TABFontPoint::WriteGeometryToMIFFile(MIDDATAFile *fp)
{ 
    OGRGeometry         *poGeom;
    OGRPoint            *poPoint;
 
    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && poGeom->getGeometryType() == wkbPoint)
        poPoint = (OGRPoint*)poGeom;
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABPoint: Missing or Invalid Geometry!");
        return -1;
    }

    fp->WriteLine("Point %g %g\n",poPoint->getX(),poPoint->getY());
    fp->WriteLine("    Symbol (%d,%d,%d,\"%s\",%d,%g)\n",
		  GetSymbolNo(),GetSymbolColor(),
		  GetSymbolSize(),GetFontNameRef(),GetFontStyleMIFValue(),
		  GetSymbolAngle());

    return 0; 
}

int TABCustomPoint::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{   
    OGRGeometry         *poGeometry;
    
    char               **papszToken;
    const char          *pszLine;
    double               dfX,dfY;

    papszToken = CSLTokenizeString(fp->GetSavedLine());

    
    if (CSLCount(papszToken) !=3)
    {
	CSLDestroy(papszToken);
	return -1;
    }

    dfX = fp->GetXTrans(atof(papszToken[1]));
    dfY = fp->GetYTrans(atof(papszToken[2]));

    CSLDestroy(papszToken);
    
    papszToken = CSLTokenizeStringComplex(fp->GetLastLine()," ,()",
					  TRUE,FALSE);
    if (CSLCount(papszToken) !=5)
    {
	
	CSLDestroy(papszToken);
	return -1;
    }
    
    SetFontName(papszToken[1]);
    SetSymbolColor(atoi(papszToken[2]));
    SetSymbolSize(atoi(papszToken[3]));
    m_nCustomStyle = atoi(papszToken[4]);
    
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
int TABCustomPoint::WriteGeometryToMIFFile(MIDDATAFile *fp)
{ 
    OGRGeometry         *poGeom;
    OGRPoint            *poPoint;
 
    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && poGeom->getGeometryType() == wkbPoint)
        poPoint = (OGRPoint*)poGeom;
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABPoint: Missing or Invalid Geometry!");
        return -1;
    }
 

    fp->WriteLine("Point %g %g\n",poPoint->getX(),poPoint->getY());
    fp->WriteLine("    Symbol (\"%s\",%d,%d,%d)\n",GetFontNameRef(),
		  GetSymbolColor(), GetSymbolSize(),m_nCustomStyle);

    return 0; 
}

int TABPolyline::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{
    const char          *pszLine;
    char               **papszToken;
    OGRLineString       *poLine;
    OGRMultiLineString  *poMultiLine;
    GBool                bMultiple = FALSE;
    int                  nNumPoints,nNumSec,i,j;
    OGREnvelope          sEnvelope;
    

    papszToken = CSLTokenizeString(fp->GetLastLine());
    
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
		poLine->setNumPoints(nNumPoints);
		for (i=0;i<nNumPoints;i++)
		{
		    papszToken = CSLTokenizeString(fp->GetLine());
		    poLine->setPoint(i,fp->GetXTrans(atof(papszToken[0])),
				     fp->GetYTrans(atof(papszToken[1])));
		}
		poMultiLine->addGeometry(poLine);
		
	    } 
	    SetGeometryDirectly(poMultiLine);
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
		papszToken = CSLTokenizeString(fp->GetLine());
    
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
		    SetPenWidth(atoi(papszToken[1]));
		    SetPenPattern(atoi(papszToken[2]));
		    SetPenColor(atoi(papszToken[3]));
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
    if (poGeom && poGeom->getGeometryType() == wkbLineString)
    {
        /*-------------------------------------------------------------
         * Simple polyline
         *------------------------------------------------------------*/
        poLine = (OGRLineString*)poGeom;
	nNumPoints = poLine->getNumPoints();
        if (nNumPoints == 2)
        {
	    fp->WriteLine("Line %g %g %g %g\n",poLine->getX(0),poLine->getY(0),
			  poLine->getX(1),poLine->getY(1));
        }
        else
        {
	    
	    fp->WriteLine("Pline %d\n",nNumPoints);
	    for (i=0;i<nNumPoints;i++)
	    {
		fp->WriteLine("%g %g\n",poLine->getX(i),poLine->getY(i));
	    }
        }
    }
    else if (poGeom && poGeom->getGeometryType() == wkbMultiLineString)
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
            if (poGeom && poGeom->getGeometryType() != wkbLineString)
            { 
		poLine = (OGRLineString*)poGeom;
		nNumPoints = poLine->getNumPoints();
        
		fp->WriteLine("  %d\n",nNumPoints);
		for (i=0;i<nNumPoints;i++)
		{
		    fp->WriteLine("%g %g\n",poLine->getX(i),poLine->getY(i));
		}
                
		break;
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
      fp->WriteLine("    Pen (%d,%d,%d)\n",GetPenWidth(),GetPenPattern(),
		    GetPenColor());
    if (m_bSmooth)
      fp->WriteLine("    Smooth\n");

    return 0; 

}

int TABRegion::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{
    double               dX, dY;
    OGRLinearRing       *poRing;
    OGRPolygon          *poPolygon;
    int                  i,iSection, numLineSections;
    char               **papszToken;
    const char          *pszLine;
    OGREnvelope          sEnvelope;

    m_bSmooth = FALSE;
    /*=============================================================
     * REGION (Similar to PLINE MULTIPLE)
     *============================================================*/
    papszToken = CSLTokenizeString(fp->GetLastLine());
    
    if (CSLCount(papszToken) ==2)
      numLineSections = atoi(papszToken[1]);
    CSLDestroy(papszToken);
    
    /*-------------------------------------------------------------
     * Create an OGRPolygon with one OGRLinearRing geometry for
     * each coordinates section.  The first ring is the outer ring.
     * __TODO__ MapInfo can probably specify islands inside holes,
     *          but there is no such thing the way OGR works... 
     *          we'll have to look into that later...
     *------------------------------------------------------------*/
    poPolygon = new OGRPolygon();
    
    for(iSection=0; iSection<numLineSections; iSection++)
    {
	int     numSectionVertices;

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
		papszToken = CSLTokenizeStringComplex(pszLine," ,",
						      TRUE,FALSE);
		if (CSLCount(papszToken) == 2)
		{	       
		    dX = fp->GetXTrans(atof(papszToken[0]));
		    dY = fp->GetYTrans(atof(papszToken[1]));
		    poRing->setPoint(i, dX, dY);
		}
		CSLDestroy(papszToken);
	    }	
	}
	poPolygon->addRing(poRing);
	poRing = NULL;
    }
  
  
    SetGeometryDirectly(poPolygon);
    poPolygon->getEnvelope(&sEnvelope);
    
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
		    SetPenWidth(atoi(papszToken[1]));
		    SetPenPattern(atoi(papszToken[2]));
		    SetPenColor(atoi(papszToken[3]));
		}
		
	    }
	    else if (EQUALN(papszToken[0],"BRUSH", 5))
	    {
		if (CSLCount(papszToken) >= 3)
		{
		    SetBrushFGColor(atoi(papszToken[2]));
		    SetBrushPattern(atoi(papszToken[1]));
		    
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
		    OGRPoint *poPoint  = new OGRPoint;
		    poPoint->setX(fp->GetXTrans(atof(papszToken[1])));
		    poPoint->setY(fp->GetYTrans(atof(papszToken[2])));
		    poPolygon->Centroid(poPoint);
		    m_bCentroid = TRUE;
		    m_dfCentroidX = fp->GetXTrans(atof(papszToken[1]));
		    m_dfCentroidY = fp->GetYTrans(atof(papszToken[2]));
		}
	    }
	}
	CSLDestroy(papszToken);
    }
    
    
    return 0; 
}
    
int TABRegion::WriteGeometryToMIFFile(MIDDATAFile *fp)
{ 
    OGRGeometry         *poGeom;
    OGRPolygon          *poPolygon=NULL;

    poGeom = GetGeometryRef();

    if (poGeom && poGeom->getGeometryType() == wkbPolygon)
    {
        /*=============================================================
         * REGIONs are similar to PLINE MULTIPLE
         *============================================================*/
        int     i, iRing, numIntRings, numPoints;
       
        poPolygon = (OGRPolygon*)poGeom;
        numIntRings = poPolygon->getNumInteriorRings();
	
	fp->WriteLine("Region %d\n",numIntRings+1);
	
        // In this loop, iRing=0 for the outer ring.
        for(iRing=0; iRing <= numIntRings; iRing++)
        {
            OGRLinearRing       *poRing;

            if (iRing == 0)
                poRing = poPolygon->getExteriorRing();
            else
                poRing = poPolygon->getInteriorRing(iRing-1); 

            numPoints = poRing->getNumPoints();

	    fp->WriteLine("  %d\n",numPoints);
            for(i=0; i<numPoints; i++)
            {
                fp->WriteLine("%g %g\n",poRing->getX(i), poRing->getY(i));
            }
        }
	
	if (GetPenPattern())
	  fp->WriteLine("    Pen (%d,%d,%d)\n",GetPenWidth(),GetPenPattern(),
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

	if (m_bCentroid)
	{
	    fp->WriteLine("    Center %g %g\n", m_dfCentroidX,
			  m_dfCentroidY);
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

int TABRectangle::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{
    const char          *pszLine;
    char               **papszToken;
    double               dXMin, dYMin, dXMax, dYMax;
    OGRPolygon          *poPolygon;
    OGRLinearRing       *poRing;

    papszToken = CSLTokenizeString(fp->GetLastLine());

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
	    papszToken = CSLTokenizeString(fp->GetLine());
	    if (CSLCount(papszToken) !=1 )
	      m_dRoundXRadius = m_dRoundYRadius = atof(papszToken[1])/2.0;
	}
    }
    CSLDestroy(papszToken);

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
         * large for the MBR
         *------------------------------------------------------------*/
        m_dRoundXRadius = MIN(m_dRoundXRadius, (dXMax-dXMin)/2.0);
        m_dRoundYRadius = MIN(m_dRoundYRadius, (dYMax-dYMin)/2.0);
        TABGenerateArc(poRing, 45, 
                       dXMin + m_dRoundXRadius, dYMin + m_dRoundYRadius,
                       m_dRoundXRadius, m_dRoundYRadius,
                       PI, 3.0*PI/2.0);
        TABGenerateArc(poRing, 45, 
                       dXMax - m_dRoundXRadius, dYMin + m_dRoundYRadius,
                       m_dRoundXRadius, m_dRoundYRadius,
                       3.0*PI/2.0, 2.0*PI);
        TABGenerateArc(poRing, 45, 
                       dXMax - m_dRoundXRadius, dYMax - m_dRoundYRadius,
                       m_dRoundXRadius, m_dRoundYRadius,
                       0.0, PI/2.0);
        TABGenerateArc(poRing, 45, 
                       dXMin + m_dRoundXRadius, dYMax - m_dRoundYRadius,
                       m_dRoundXRadius, m_dRoundYRadius,
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

    poPolygon->addRing(poRing);
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
		   SetPenWidth(atoi(papszToken[1]));
		   SetPenPattern(atoi(papszToken[2]));
		   SetPenColor(atoi(papszToken[3]));
	       }
	      
	   }
	   else if (EQUALN(papszToken[0],"BRUSH", 5))
	   {
	       if (CSLCount(papszToken) >=3)
	       {
		   SetBrushFGColor(atoi(papszToken[2]));
		   SetBrushPattern(atoi(papszToken[1]));

		   if (CSLCount(papszToken) == 4)
		       SetBrushBGColor(atoi(papszToken[3]));
		   else
		      SetBrushTransparent(TRUE);
	       }
	      
	   }
       }
       CSLDestroy(papszToken);
   }
 
   return 0; 

}    
int TABRectangle::WriteGeometryToMIFFile(MIDDATAFile *fp)
{ 
    OGRGeometry         *poGeom;
    OGRPolygon          *poPolygon;
    OGREnvelope         sEnvelope;
    
     /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && poGeom->getGeometryType() == wkbPolygon)
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
	fp->WriteLine("Roundrect %g %g %g %g %g\n", 
		      sEnvelope.MinX, sEnvelope.MinY,
		      sEnvelope.MaxX, sEnvelope.MaxY, m_dRoundXRadius*2.0);
    }
    else
    {
	fp->WriteLine("Rect %g %g %g %g\n", 
		      sEnvelope.MinX, sEnvelope.MinY,
		      sEnvelope.MaxX, sEnvelope.MaxY);
    }
    
    if (GetPenPattern())
      fp->WriteLine("    Pen (%d,%d,%d)\n",GetPenWidth(),GetPenPattern(),
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

int TABEllipse::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{   
    const char *pszLine;
    char **papszToken;
    double              dXMin, dYMin, dXMax, dYMax;
    OGRPolygon          *poPolygon;
    OGRLinearRing       *poRing;

    papszToken = CSLTokenizeString(fp->GetLastLine());

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

    poPolygon->addRing(poRing);
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
		    SetPenWidth(atoi(papszToken[1]));
		    SetPenPattern(atoi(papszToken[2]));
		   SetPenColor(atoi(papszToken[3]));
		}
		
	    }
	    else if (EQUALN(papszToken[0],"BRUSH", 5))
	    {
		if (CSLCount(papszToken) >= 3)
		{
		    SetBrushFGColor(atoi(papszToken[2]));
		    SetBrushPattern(atoi(papszToken[1]));
		    
		    if (CSLCount(papszToken) == 4)
		      SetBrushBGColor(atoi(papszToken[3]));
		    else
		      SetBrushTransparent(TRUE);
		    
		}
		
	    }
	}
	CSLDestroy(papszToken);
    }
    return 0; 
}

int TABEllipse::WriteGeometryToMIFFile(MIDDATAFile *fp)
{
    OGRGeometry         *poGeom;
    OGREnvelope         sEnvelope;
 
    poGeom = GetGeometryRef();
    if ( (poGeom && poGeom->getGeometryType() == wkbPolygon ) ||
         (poGeom && poGeom->getGeometryType() == wkbPoint )  )
        poGeom->getEnvelope(&sEnvelope);
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABEllipse: Missing or Invalid Geometry!");
        return -1;
    }
      
    fp->WriteLine("Ellipse %g %g %g %g\n",sEnvelope.MinX, sEnvelope.MinY,
		  sEnvelope.MaxX,sEnvelope.MaxY);
    
    if (GetPenPattern())
      fp->WriteLine("    Pen (%d,%d,%d)\n",GetPenWidth(),GetPenPattern(),
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

int TABArc::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{
    const char          *pszLine;
    OGRLineString       *poLine;
    char               **papszToken;
    double               dXMin,dXMax, dYMin,dYMax;
    int                  numPts;
    
    papszToken = CSLTokenizeString(fp->GetLastLine());

    if (CSLCount(papszToken) == 5)
    {
	dXMin = fp->GetXTrans(atof(papszToken[1]));
	dXMax = fp->GetXTrans(atof(papszToken[3]));
	dYMin = fp->GetYTrans(atof(papszToken[2]));
	dYMax = fp->GetYTrans(atof(papszToken[4]));

	papszToken = CSLTokenizeString(fp->GetLine());
	if (CSLCount(papszToken) != 2)
	  return -1;

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
		    SetPenWidth(atoi(papszToken[1]));
		    SetPenPattern(atoi(papszToken[2]));
		    SetPenColor(atoi(papszToken[3]));
		}
		
	    }
	}
	CSLDestroy(papszToken);
   }
   return 0; 
}

int TABArc::WriteGeometryToMIFFile(MIDDATAFile *fp)
{ 
    /*-------------------------------------------------------------
     * Start/End angles
     * Since we ALWAYS produce files in quadrant 1 then we can
     * ignore the special angle conversion required by flipped axis.
     *------------------------------------------------------------*/

     
    // Write the Arc's actual MBR
     fp->WriteLine("Arc %g %g %g %g\n", m_dCenterX-m_dXRadius, 
		   m_dCenterY-m_dYRadius, m_dCenterX+m_dXRadius, 
		   m_dCenterY+m_dYRadius);

     fp->WriteLine("  %g %g\n",m_dStartAngle,m_dEndAngle); 
     
     if (GetPenPattern())
       fp->WriteLine("    Pen (%d,%d,%d)\n",GetPenWidth(),GetPenPattern(),
		     GetPenColor());
     
   
    return 0; 

}

static char *GetStringWithCR(const char *pszString)
{
    char *pszNewString = (char *)CPLCalloc(1,sizeof(char) * 
					   (strlen(pszString) +1));
    int i =0;
    int j =0;

    while (pszString[i])
    {
	if (pszString[i] =='\\' && 
	    pszString[i+1] == 'n')
	{
	    pszNewString[j++] = '\n';
	    i++;
	    i++;
	}
	else
	{
	    pszNewString[j++] = pszString[i++];
	}
    }
   
    return pszNewString;
}

static char *GetStringWithoutCR(const char *pszString)
{
    char *pszNewString = (char *)CPLCalloc(2,sizeof(char) * 
					   (strlen(pszString) +1));

    int i =0;
    int j =0;

    while (pszString[i])
    {
	if (pszString[i] =='\n')
	{
	    pszNewString[j++] = '\\';
	    pszNewString[j++] = 'n';
	    i++;
	}
	else
	{
	    pszNewString[j++] = pszString[i++];
	}
    }

    return pszNewString;
    
}

int TABText::ReadGeometryFromMIFFile(MIDDATAFile *fp)
{ 
    double               dXMin, dYMin, dXMax, dYMax;
    OGRGeometry         *poGeometry;
    const char          *pszLine;
    char               **papszToken;
    const char          *pszString;
  
    papszToken = CSLTokenizeString(fp->GetLastLine());

    if (CSLCount(papszToken) == 1)
    {
	papszToken = CSLTokenizeString(fp->GetLine());
	if (CSLCount(papszToken) != 1)
 	{
	    CSLDestroy(papszToken);
	    return -1;
	}
	else
	  pszString = papszToken[0];
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

    CPLFree(m_pszString);
    m_pszString = GetStringWithCR(pszString);

    CSLDestroy(papszToken);
    papszToken = CSLTokenizeString(fp->GetLine());
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
			    m_dfLineX = fp->GetXTrans(atof(papszToken[5]));
			    m_dfLineY = fp->GetYTrans(atof(papszToken[6]));
			}
			else if (EQUALN(papszToken[4],"arrow", 5))
			{
			    SetTextLineType(TABTLArrow);
			    m_dfLineX = fp->GetXTrans(atof(papszToken[5]));
			    m_dfLineY = fp->GetYTrans(atof(papszToken[6]));
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
 		    m_dAngle = atof(papszToken[1]);

		    //SetTextAngle(atof(papszToken[1]));
		}
		
	    }
	    else if (EQUALN(papszToken[0],"LAbel",5))
	    {
		if (CSLCount(papszToken) == 5)
		{    
		    if (EQUALN(papszToken[2],"simple",6))
		    {
			SetTextLineType(TABTLSimple);
			m_dfLineX = fp->GetXTrans(atof(papszToken[3]));
			m_dfLineY = fp->GetYTrans(atof(papszToken[4]));
		    }
		    else if (EQUALN(papszToken[2],"arrow", 5))
		    {
			SetTextLineType(TABTLArrow);
			m_dfLineX = fp->GetXTrans(atof(papszToken[3]));
			m_dfLineY = fp->GetYTrans(atof(papszToken[4]));
		    }
		}
		

		// What I do with the XY coordonate
	    }
	}
	CSLDestroy(papszToken);
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

int TABText::WriteGeometryToMIFFile(MIDDATAFile *fp)
{
    char *pszString;
    double dXMin,dYMin,dXMax,dYMax;
    pszString = GetStringWithoutCR(GetTextString());
    
    fp->WriteLine("Text \"%s\"\n", pszString );
    CPLFree(pszString);
    //    UpdateTextMBR();
    GetMBR(dXMin, dYMin, dXMax, dYMax);
    fp->WriteLine("    %g %g %g %g\n",dXMin, dYMin,dXMax, dYMax); 
 
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

    if ((GetTextAngle() - 0.000001) >0.0)
	fp->WriteLine("    Angle %g\n",GetTextAngle());

    switch (GetTextLineType())
    {
      case TABTLSimple:
	fp->WriteLine("    Label Line Simple %g %g \n",
		      m_dfLineX,m_dfLineY );
	break;
      case TABTLArrow:
	fp->WriteLine("    Label Line Arrow %g %g \n",
		      m_dfLineX,m_dfLineY );
	break;
      case TABTLNoLine:
      default:
	break;
    }
    return 0; 

}

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
int TABDebugFeature::WriteGeometryToMIFFile(MIDDATAFile *fp){ return -1; }




