/**********************************************************************
 * $Id: mitab_miffile.cpp,v 1.5 1999/11/14 18:12:47 stephane Exp $
 *
 * Name:     mitab_tabfile.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation of the MIDFile class.
 *           To be used by external programs to handle reading/writing of
 *           features from/to MID/MIF datasets.
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
 * $Log: mitab_miffile.cpp,v $
 * Revision 1.5  1999/11/14 18:12:47  stephane
 * add a test if it's a empty line for unknown feature type
 *
 * Revision 1.4  1999/11/14 17:43:32  stephane
 * Add ifdef to remove CPLError if OGR is define
 *
 * Revision 1.3  1999/11/11 01:22:05  stephane
 * Remove DebugFeature call, Point Reading error, add IsValidFeature() to test correctly if we are on a feature
 *
 * Revision 1.2  1999/11/09 22:31:38  warmerda
 * initial implementation of MIF CoordSys support
 *
 * Revision 1.1  1999/11/08 19:19:34  stephane
 * Add CoordSys string support
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
    m_pszVersion = NULL;
    m_pszCharset = NULL;
    m_pszDelimiter = CPLStrdup(",");
    m_pszUnique = NULL;
    m_pszIndex = NULL;
    m_pszCoordSys = NULL;
    
    m_dfXMultiplier = 1.0;
    m_dfYMultiplier = 1.0;
    m_dfXDisplacement = 0.0;
    m_dfYDisplacement = 0.0;

    m_poMIDFile = NULL;
    m_poMIFFile = NULL;

    m_poDefn = NULL;
    m_poSpatialRef = NULL;

    m_nCurFeatureId = 0;
    m_nLastFeatureId = -1;
    m_poCurFeature = NULL;
   
    m_bBoundsSet = FALSE;
    m_nAttribut = 0;
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
int MIFFile::Open(const char *pszFname, const char *pszAccess)
{
    char *pszTmpFname = NULL;
    int nFnameLen = 0;
    
    if (m_poMIDFile)
    {
#ifndef OGR
        CPLError(CE_Failure, CPLE_FileIO,
                 "Open() failed: object already contains an open file");
#else
	CPLErrorReset();
#endif
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
    }
    else
    {
#ifndef OGR
        CPLError(CE_Failure, CPLE_FileIO,
                 "Open() failed: access mode \"%s\" not supported", pszAccess);
#else
	CPLErrorReset();
#endif
        return -1;
    }

    /*-----------------------------------------------------------------
     * Make sure filename has a .MID extension... 
     *----------------------------------------------------------------*/
    m_pszFname = CPLStrdup(pszFname);
    nFnameLen = strlen(m_pszFname);
    if (nFnameLen > 4 && (strcmp(m_pszFname+nFnameLen-4, ".MID")==0 ||
                     strcmp(m_pszFname+nFnameLen-4, ".MIF")==0 ) )
        strcpy(m_pszFname+nFnameLen-4, ".MID");
    else if (nFnameLen > 4 && (strcmp(m_pszFname+nFnameLen-4, ".mid")==0 ||
                          strcmp(m_pszFname+nFnameLen-4, ".mif")==0 ) )
        strcpy(m_pszFname+nFnameLen-4, ".mid");
    else
    {
#ifndef OGR
        CPLError(CE_Failure, CPLE_FileIO,
                 "Open() failed for %s: invalid filename extension",
                 m_pszFname);
#else
	CPLErrorReset();
#endif
        CPLFree(m_pszFname);
        return -1;
    }

    pszTmpFname = CPLStrdup(m_pszFname);


#ifndef _WIN32
    /*-----------------------------------------------------------------
     * On Unix, make sure extension uses the right cases
     * We do it even for write access because if a file with the same
     * extension already exists we want to overwrite it.
     *----------------------------------------------------------------*/
    TABAdjustFilenameExtension(m_pszFname);
#endif

    m_poMIDFile = new MIDDATAFile;

    if (m_poMIDFile->Open(pszTmpFname, pszAccess) !=0)
    {
	CPLFree(pszTmpFname);
	Close();
#ifndef OGR	
	CPLError(CE_Failure, CPLE_NotSupported,
                 "Unable to open the MID file.");
#else
	CPLErrorReset();
#endif

	return -1;
    }
    /*-----------------------------------------------------------------
     * Handle .MID file... depends on access mode.
     *----------------------------------------------------------------*/
    if (m_eAccessMode == TABWrite)
    {
	m_pszVersion = CPLStrdup("300");
	m_pszCharset = CPLStrdup("Neutral");
    }

    /*-----------------------------------------------------------------
     * Open .MIF file
     *----------------------------------------------------------------*/
    if (nFnameLen > 4 && strcmp(pszTmpFname+nFnameLen-4, ".MID")==0)
        strcpy(pszTmpFname+nFnameLen-4, ".MIF");
    else 
        strcpy(pszTmpFname+nFnameLen-4, ".mif");

#ifndef _WIN32
    TABAdjustFilenameExtension(pszTmpFname);
#endif

    m_poMIFFile = new MIDDATAFile;

    if (m_poMIFFile->Open(pszTmpFname, pszAccess) != 0)
    {
        CPLFree(pszTmpFname);
        Close();
#ifndef OGR
	CPLError(CE_Failure, CPLE_NotSupported,
                 "Unable to open the MID file.");
#else
	CPLErrorReset();
#endif

        return -1;
    }

    CPLFree(pszTmpFname);
    pszTmpFname = NULL;

    /*-----------------------------------------------------------------
     * Read MIF File Header
     *----------------------------------------------------------------*/
    if (m_eAccessMode == TABRead && ParseMIFHeader() != 0)
    {
        CPLFree(pszTmpFname);
        Close();

#ifndef OGR  
	CPLError(CE_Failure, CPLE_NotSupported,
                 "Unable to read the header file.");
#else
	CPLErrorReset();
#endif
	return -1;
    }

    m_nLastFeatureId  = 0;
    if (m_eAccessMode == TABRead && CountNumberFeature() != 0)
    {
	CPLFree(pszTmpFname);
        Close();
#ifndef OGR
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unable to count the number of feature.");
#else
	CPLErrorReset();
#endif
       return -1;
    }

    /* Put the MID file at the correct location, on the first feature */
    if (m_eAccessMode == TABRead && (m_poMIDFile->GetLine() == NULL))
    {
	CPLFree(pszTmpFname);
        Close();
#ifdef OGR
	CPLErrorReset();
#endif
        return -1;
    }

    m_poMIFFile->SetTranslation(m_dfXMultiplier,m_dfYMultiplier,
				m_dfXDisplacement, m_dfYDisplacement);
    m_poMIDFile->SetTranslation(m_dfXMultiplier,m_dfYMultiplier,
				m_dfXDisplacement, m_dfYDisplacement);
    m_poMIFFile->SetDelimiter(m_pszDelimiter);
    m_poMIDFile->SetDelimiter(m_pszDelimiter);

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
    GBool  bColumns = FALSE;
    int    nColumns = 0;
    GBool  bCoordSys = FALSE;
    char  *pszTmp;
	    
    
    const char *pszLine;
    char **papszToken;
    int i = 0;

    m_poDefn = new OGRFeatureDefn("TABFeature");
    // Ref count defaults to 0... set it to 1
    m_poDefn->Reference();

    
    if (m_eAccessMode != TABRead)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "ParseMIDFile() can be used only with Read access.");
        return -1;
    }
    

    // Parse untin we found the "Data" Tokenize
    while (((pszLine = m_poMIFFile->GetLine()) != NULL) && 
	   !(EQUALN(pszLine,"Data",4)))
    {
	if (EQUALN(pszLine,"VERSION",7))
	{
	    papszToken = CSLTokenizeStringComplex(pszLine," ()",TRUE,FALSE); 
	    bColumns = FALSE; bCoordSys = FALSE;
	    if (CSLCount(papszToken)  == 2)
	      m_pszVersion = CPLStrdup(papszToken[1]);

	    CSLDestroy(papszToken);
	
	}
	else if (EQUALN(pszLine,"CHARSET",7))
	{
	    papszToken = CSLTokenizeStringComplex(pszLine," ()",TRUE,FALSE); 
	     bColumns = FALSE; bCoordSys = FALSE;
	  
	    if (CSLCount(papszToken)  == 2)
	      m_pszCharset = CPLStrdup(papszToken[1]);

	    CSLDestroy(papszToken);
	
	}
	else if (EQUALN(pszLine,"DELIMITER",9))
	{
	    papszToken = CSLTokenizeStringComplex(pszLine," ()",TRUE,FALSE); 
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
	    papszToken = CSLTokenizeStringComplex(pszLine," ()",TRUE,FALSE); 
	     bColumns = FALSE; bCoordSys = FALSE;
	  
	    if (CSLCount(papszToken) == 2)
	      m_pszUnique = CPLStrdup(papszToken[1]);
	    
	    CSLDestroy(papszToken);
	
	}
	else if (EQUALN(pszLine,"INDEX",5))
	{
	    papszToken = CSLTokenizeStringComplex(pszLine," ()",TRUE,FALSE); 
	     bColumns = FALSE; bCoordSys = FALSE;
	  
	    if (CSLCount(papszToken) == 2)
	      m_pszIndex = CPLStrdup(papszToken[1]);
	    
	    CSLDestroy(papszToken);
	
	}
	else if (EQUALN(pszLine,"COORDSYS",8) )
        {
	    bCoordSys = TRUE;
	    m_pszCoordSys = CPLStrdup(pszLine + 9);
        }
        else if( EQUALN(pszLine," COORDSYS",9) )
	{
	    bCoordSys = TRUE;
	    m_pszCoordSys = CPLStrdup(pszLine+10);
	}
	else if (EQUALN(pszLine,"TRANSFORM",9))
	{
	    papszToken = CSLTokenizeStringComplex(pszLine," ,",TRUE,FALSE); 
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
	    papszToken = CSLTokenizeStringComplex(pszLine," ()",TRUE,FALSE); 
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
	else if (bColumns == TRUE && nColumns >0)
	{
	    if (nColumns == 0)
	    {
		// Permit to 0 columns
		bColumns = FALSE;
	    }
	    else if (AddFields(pszLine) == 0)
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
    
    if (EQUALN(m_poMIFFile->GetLastLine(),"DATA",4) == FALSE)
    {
	CPLError(CE_Failure, CPLE_NotSupported,
                 "The file is corrupted, I don't received the DATA token.");
    }
    
    
    return 0;

}


int  MIFFile::AddFields(const char *pszLine)
{
    char **papszToken;
    int nStatus = 0,numTok;
    OGRFieldDefn *poFieldDefn = NULL;

    CPLAssert(m_poDefn);
    papszToken = CSLTokenizeStringComplex(pszLine," (,)",TRUE,FALSE); 
    numTok = CSLCount(papszToken);

                
    if (numTok >= 3 && EQUAL(papszToken[1], "char"))
    {
	/*-------------------------------------------------
	 * CHAR type
	 *------------------------------------------------*/
	poFieldDefn = new OGRFieldDefn(papszToken[0], OFTString);
	poFieldDefn->SetWidth(atoi(papszToken[2]));
    }
    else if (numTok >= 2 && EQUAL(papszToken[1], "integer"))
    {
	/*-------------------------------------------------
	 * INTEGER type
	 *------------------------------------------------*/
	  poFieldDefn = new OGRFieldDefn(papszToken[0], OFTInteger);
    }
    else if (numTok >= 2 && EQUAL(papszToken[1], "smallint"))
    {
	/*-------------------------------------------------
	 * SMALLINT type
	 *------------------------------------------------*/
	poFieldDefn = new OGRFieldDefn(papszToken[0], OFTInteger);
    }
    else if (numTok >= 4 && EQUAL(papszToken[1], "decimal"))
    {
	/*-------------------------------------------------
	 * DECIMAL type
	 *------------------------------------------------*/
	poFieldDefn = new OGRFieldDefn(papszToken[0], OFTReal);
	poFieldDefn->SetWidth(atoi(papszToken[2]));
	poFieldDefn->SetPrecision(atoi(papszToken[3]));
    }
    else if (numTok >= 2 && EQUAL(papszToken[1], "float"))
    {
	/*-------------------------------------------------
	 * FLOAT type
	 *------------------------------------------------*/
	poFieldDefn = new OGRFieldDefn(papszToken[0], OFTReal);
    }
    else if (numTok >= 2 && EQUAL(papszToken[1], "date"))
    {
	/*-------------------------------------------------
	 * DATE type (returned as a string: "DD/MM/YYYY")
	 *------------------------------------------------*/
	poFieldDefn = new OGRFieldDefn(papszToken[0], OFTString);
	poFieldDefn->SetWidth(10);
    }
    else if (numTok >= 2 && EQUAL(papszToken[1], "logical"))
    {
	/*-------------------------------------------------
	 * LOGICAL type (value "T" or "F")
	 *------------------------------------------------*/
	poFieldDefn = new OGRFieldDefn(papszToken[0], OFTString);
	poFieldDefn->SetWidth(1);
    }
    else 
      nStatus = -1; // Unrecognized field type or line corrupt
    
    if (nStatus != 0)
    {
	CPLError(CE_Failure, CPLE_FileIO,
		 "Failed to parse field definition in file %s", m_pszFname);
	CSLDestroy(papszToken);
	if (poFieldDefn)
	  delete poFieldDefn;
	return -1;
    }
    /*-----------------------------------------------------
     * Add the FieldDefn to the FeatureDefn and continue with
     * the next one.
     *----------------------------------------------------*/
    m_poDefn->AddFieldDefn(poFieldDefn);
    CSLDestroy(papszToken);
    
    return 0;
}


int MIFFile::GetFeatureCount (int bForce)
{
    
    if( m_poFilterGeom != NULL )
        return OGRLayer::GetFeatureCount( bForce );
    else
        return m_nLastFeatureId ;

}

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
    
    m_nCurFeatureId = 0;
}


int MIFFile::CountNumberFeature()
{
    char **papszToken;
    const char *pszLine;
    
    GBool bPLine = FALSE;
    GBool bText = FALSE;

    while ((pszLine = m_poMIFFile->GetLine()) != NULL)
    {
	if (m_poMIFFile->IsValidFeature(pszLine))
	{
	    bPLine = FALSE;
	    bText = FALSE;
	    m_nLastFeatureId++;
	}

	papszToken = CSLTokenizeString(pszLine);

	if (EQUALN(pszLine,"POINT",5))
	{
	    if (CSLCount(papszToken) == 3)
	    {
		UpdateBounds(m_poMIFFile->GetXTrans(atof(papszToken[1])),
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
		UpdateBounds(m_poMIFFile->GetXTrans(atof(papszToken[1])), 
			     m_poMIFFile->GetYTrans(atof(papszToken[2])));
		UpdateBounds(m_poMIFFile->GetXTrans(atof(papszToken[3])), 
			     m_poMIFFile->GetYTrans(atof(papszToken[4])));
	    }
	}
	else if (EQUALN(pszLine,"REGION",6) ||
		 EQUALN(pszLine,"PLINE",5))
	{
	    bPLine = TRUE;
	}
	else if (EQUALN(pszLine,"TEXT",4)) 
	{
	    bText = TRUE;
	}
	else if (bPLine == TRUE)
	{
	    if (CSLCount(papszToken) == 2)
	    {
		UpdateBounds( m_poMIFFile->GetXTrans(atof(papszToken[0])),
			      m_poMIFFile->GetYTrans(atof(papszToken[1])));
	    }
	}
	else if (bText == TRUE)
	{
	   if (CSLCount(papszToken) == 4)
	    {
		UpdateBounds(m_poMIFFile->GetXTrans(atof(papszToken[0])),
			     m_poMIFFile->GetYTrans(atof(papszToken[1])));
		UpdateBounds(m_poMIFFile->GetXTrans(atof(papszToken[2])),
			     m_poMIFFile->GetYTrans(atof(papszToken[3])));
	    } 
	}
	
      }
    
    m_poMIFFile->Rewind();

    while ((pszLine = m_poMIFFile->GetLine()) != NULL)
      if (EQUALN(pszLine,"DATA",4))
	break;

    while ((pszLine = m_poMIFFile->GetLine()) != NULL)
    {
	if (m_poMIFFile->IsValidFeature(pszLine))
	  break;
    }
    
    return 0;
}

/**********************************************************************
 *                   MIFFile::WriteMIFHeader()
 *
 * Generate the .TAB file using mainly the attribute fields definition.
 *
 * This private method should be used only during the Close() call with
 * write access mode.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int MIFFile::WriteMIFHeader()
{
    if (m_eAccessMode != TABWrite)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "WriteMIFHeader() can be used only with Write access.");
        return -1;
    }

    m_poMIFFile->WriteLine("Version %s\n", m_pszVersion);
    m_poMIFFile->WriteLine("Charset \"%s\"\n", m_pszCharset);
    m_poMIFFile->WriteLine("Delimiter \"%s\"\n", m_pszDelimiter);
    if (m_pszCoordSys)
      m_poMIFFile->WriteLine("CoordSys %s\n",m_pszCoordSys);

    
    if (m_poDefn && m_poDefn->GetFieldCount() > 0)
    {
	int iField;
	OGRFieldDefn *poFieldDefn;
	
	m_poMIFFile->WriteLine("Columns %d\n", m_poDefn->GetFieldCount());
	
	for(iField=0; iField<m_poDefn->GetFieldCount(); iField++)
	{
	    poFieldDefn = m_poDefn->GetFieldDefn(iField);
	    
	    switch(poFieldDefn->GetType())
            {
              case OFTInteger:
		m_poMIFFile->WriteLine("  %s Integer\n",
				       poFieldDefn->GetNameRef());
		break;
              case OFTReal:
		m_poMIFFile->WriteLine("  %s Decimal(%d,%d)\n",
				       poFieldDefn->GetNameRef(),
				       poFieldDefn->GetWidth(),
				       poFieldDefn->GetPrecision());    
		break;
              case OFTString:
              default:
		m_poMIFFile->WriteLine("  %s Char(%d)\n",
				       poFieldDefn->GetNameRef(),
				       poFieldDefn->GetWidth());
	    }
		    
	}
	m_poMIFFile->WriteLine("Data\n\n");
    }
    else
    {
	CPLError(CE_Failure, CPLE_NotSupported,
                 "The file must have 1 attribut minimum.");
	return -1;
    }
   
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
    if (m_poMIDFile == NULL)
        return 0;

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
    

    CPLFree(m_pszFname);
    m_pszFname = NULL;

    CPLFree(m_pszVersion);
    m_pszVersion = NULL;
    CPLFree(m_pszCharset);
    m_pszCharset = NULL;

    m_nCurFeatureId = 0;
    m_nLastFeatureId = -1;

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

    if (nPrevId <= 0 && m_nLastFeatureId > 0)
        return 1;       // Feature Ids start at 1
    else if (nPrevId > 0 && nPrevId < m_nLastFeatureId)
        return nPrevId + 1;
    else
        return -1;

    return 0;
}

int MIFFile::GotoFeature(int nFeatureId)
{
    int i;
    
    if (nFeatureId <= 0 || nFeatureId-1 > m_nLastFeatureId)
      return -1;

    if ((nFeatureId -1) == m_nCurFeatureId) //CorrectPosition
    {
	return 0;
    }
    else
    {
	ResetReading();
	for (i=0;i<nFeatureId;i++)
	{
	    NextFeature();
	}
	return 0;
    }
}

int MIFFile::NextFeature()
{
    const char *pszLine;
    while ((pszLine = m_poMIFFile->GetLine()) != NULL)
    {
	if (m_poMIFFile->IsValidFeature(pszLine))
	  break;
    }
    
    m_poMIDFile->GetLine();
    m_nCurFeatureId++;
    return 0;
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
    char **papszToken;
    
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
	if (EQUALN(pszLine,"NONE",4))
	{
	    m_poCurFeature = new TABFeature(m_poDefn);
	}
	else if (EQUALN(pszLine,"POINT",5))
	{
	    // Special case, we need to know two lines to decide the type

	    papszToken = CSLTokenizeString(pszLine);
	    
	    if (CSLCount(papszToken) !=3)
	      return NULL;
	    
	    m_poMIFFile->SaveLine(pszLine);

	    if ((pszLine = m_poMIFFile->GetLine()) != NULL)
	    {
		papszToken = CSLTokenizeStringComplex(pszLine," ,()",
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
			return NULL;
			break;
		    }
		}
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
   
    /* Update the Current Feature ID */
    m_poCurFeature->SetFID(m_nCurFeatureId++);

    return m_poCurFeature;
}

/**********************************************************************
 *                   MIFFile::SetFeature()
 *
 * Write a feature to this dataset.  
 *
 * For now only sequential writes are supported (i.e. with nFeatureId=-1)
 * but eventually we should be able to do random access by specifying
 * a value through nFeatureId.
 *
 * Returns the new featureId (> 0) on success, or -1 if an
 * error happened in which case, CPLError() will have been called to
 * report the reason of the failure.
 **********************************************************************/
int MIFFile::SetFeature(TABFeature *poFeature, int nFeatureId /*=-1*/)
{
    
    if (m_eAccessMode != TABWrite)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetFeature() can be used only with Write access.");
        return -1;
    }

    if (nFeatureId != -1)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetFeature(): random access not implemented yet.");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Make sure file is opened and establish new feature id.
     *----------------------------------------------------------------*/
    if (m_poMIDFile == NULL)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "SetFeature() failed: file is not opened!");
        return -1;
    }

    if (m_nLastFeatureId < 1)
    {
        /*-------------------------------------------------------------
         * OK, this is the first feature in the dataset... make sure the
         * .DAT schema has been initialized.
         *------------------------------------------------------------*/
	 if (m_poDefn == NULL)
	 {
	     m_poDefn = poFeature->GetDefnRef();
	     m_poDefn->Reference();
	 }
	 
	 WriteMIFHeader();     

	 nFeatureId = m_nLastFeatureId = 1;
    }
    else
    {
        nFeatureId = ++ m_nLastFeatureId;
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
        return -1;
    }

    if (m_poMIDFile == NULL ||
        poFeature->WriteRecordToMIDFile(m_poMIDFile) != 0 )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Failed writing attributes for feature id %d in %s",
                 nFeatureId, m_pszFname);
        return -1;
    }

   
    return nFeatureId; 
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
 * A reference to the OGRFeatureDefn will be kept and will be used to
 * build the .DAT file, etc.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int MIFFile::SetFeatureDefn(OGRFeatureDefn *poFeatureDefn,
                         TABFieldType *paeMapInfoNativeFieldTypes /* =NULL */)
{
    return -1;
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
                            int nWidth, int nPrecision /*=0*/)
{
    return -1;
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
  return TABFUnknown;
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
/*                       MIFFile::GetSpatialRef()                       */
/************************************************************************/

OGRSpatialReference *MIFFile::GetSpatialRef()

{
    if( m_poSpatialRef == NULL )
        m_poSpatialRef = MITABCoordSys2SpatialRef( m_pszCoordSys );

    return m_poSpatialRef;
}

/**********************************************************************
 *                   MIFFile::UpdateBounds()
 *
 * Private Methode used to update the Bounds values
 **********************************************************************/
void MIFFile::UpdateBounds(double dfX, double dfY)
{
    if (m_bBoundsSet == FALSE)
    {
	m_bBoundsSet = TRUE;
	m_dXMin = m_dXMax = dfX;
	m_dYMin = m_dYMax = dfY;
    }
    else
    {
	if (dfX < m_dXMin)
	  m_dXMin = dfX;
	if (dfX > m_dXMax)
	  m_dXMax = dfX;
	if (dfY < m_dYMin)
	  m_dYMin = dfY;
	if (dfY > m_dYMax)
	  m_dYMax = dfY;
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
    }

    /* We don't need the Bounds for the Mid/Mif write. */
    
    return 0; 
}


/**********************************************************************
 *                   MIFFile::GetBounds()
 *
 * Fetch projection coordinates bounds of a dataset.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int MIFFile::GetBounds(double &dXMin, double &dYMin, 
                       double &dXMax, double &dYMax)
{
    
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


/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int MIFFile::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    else if( EQUAL(pszCap,OLCSequentialWrite) 
             || EQUAL(pszCap,OLCRandomWrite) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return FALSE;

    else 
        return FALSE;
}

