/**********************************************************************
 * $Id: mitab_tabfile.cpp,v 1.16 1999/11/08 19:18:09 stephane Exp $
 *
 * Name:     mitab_tabfile.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation of the TABFile class, the main class of the lib.
 *           To be used by external programs to handle reading/writing of
 *           features from/to TAB datasets.
 * Author:   Daniel Morissette, danmo@videotron.ca
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
 * $Log: mitab_tabfile.cpp,v $
 * Revision 1.16  1999/11/08 19:18:09  stephane
 * remove multiply definition
 *
 * Revision 1.15  1999/11/08 04:36:28  stephane
 * add ogr support
 *
 * Revision 1.14  1999/10/19 06:14:52  daniel
 * Check that new tables contain at least one column (MapInfo requirement)
 *
 * Revision 1.13  1999/10/06 15:09:58  daniel
 * Removed unused variables
 *
 * Revision 1.12  1999/10/06 13:16:50  daniel
 * Added GetBounds()
 *
 * Revision 1.11  1999/10/01 03:50:00  daniel
 * Increment RefCount for OGRFeatureDefn in ParseTABFile()
 *
 * Revision 1.10  1999/10/01 02:12:17  warmerda
 * fixed OGRFieldDefn leak
 *
 * Revision 1.9  1999/09/28 13:32:51  daniel
 * Added AddFieldNative()
 *
 * Revision 1.8  1999/09/26 14:59:37  daniel
 * Implemented write support
 *
 * Revision 1.7  1999/09/23 19:51:43  warmerda
 * moved GetSpatialRef() to mitab_spatialref.cpp
 *
 * Revision 1.6  1999/09/20 18:42:20  daniel
 * Use binary access to open files.
 *
 * Revision 1.5  1999/09/17 17:36:05  warmerda
 * The appropriate default value for RectifiedGridAngle in HOM is 90.0.
 *
 * Revision 1.4  1999/09/16 02:39:17  daniel
 * Completed read support for most feature types
 *
 * Revision 1.3  1999/09/01 17:50:28  daniel
 * Added GetNativeFieldType() and GetFeatureDefn()
 *
 * Revision 1.2  1999/07/14 05:20:42  warmerda
 * added first pass of projection creation
 *
 * Revision 1.1  1999/07/12 04:18:25  daniel
 * Initial checkin
 *
 **********************************************************************/

#include "mitab.h"
#include "mitab_utils.h"

/*=====================================================================
 *                      class TABFile
 *====================================================================*/


/**********************************************************************
 *                   TABFile::TABFile()
 *
 * Constructor.
 **********************************************************************/
TABFile::TABFile()
{
    m_pszFname = NULL;
    m_papszTABFile = NULL;
    m_pszVersion = NULL;
    m_pszCharset = NULL;

    m_poMAPFile = NULL;
    m_poDATFile = NULL;
    m_poDefn = NULL;
    m_poSpatialRef = NULL;
    m_poCurFeature = NULL;
    m_nCurFeatureId = 0;
    m_nLastFeatureId = 0;

    m_bBoundsSet = FALSE;
}

/**********************************************************************
 *                   TABFile::~TABFile()
 *
 * Destructor.
 **********************************************************************/
TABFile::~TABFile()
{
    Close();
}


int TABFile::GetFeatureCount (int bForce)
{
    
    if( m_poFilterGeom != NULL )
        return OGRLayer::GetFeatureCount( bForce );
    else
        return m_nLastFeatureId;

}

void TABFile::ResetReading()
{
    m_nCurFeatureId = 0;
}


/**********************************************************************
 *                   TABFile::Open()
 *
 * Open a .TAB dataset and the associated files, and initialize the 
 * structures to be ready to read features from (or write to) it.
 *
 * Supported access modes are "r" (read-only) and "w" (create new dataset).
 *
 * Note that dataset extents will have to be set using SetBounds() before
 * any feature can be written to a newly created dataset.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABFile::Open(const char *pszFname, const char *pszAccess)
{
    char *pszTmpFname = NULL;
    int nFnameLen = 0;

   
    if (m_poMAPFile)
    {
        return -1;
    }

    /*-----------------------------------------------------------------
     * Validate access mode
     *----------------------------------------------------------------*/
    if (EQUALN(pszAccess, "r", 1))
    {
        m_eAccessMode = TABRead;
        pszAccess = "rb";
    }
    else if (EQUALN(pszAccess, "w", 1))
    {
        m_eAccessMode = TABWrite;
        pszAccess = "wb";
    }
    else
    {
        return -1;
    }

    /*-----------------------------------------------------------------
     * Make sure filename has a .TAB extension... 
     *----------------------------------------------------------------*/
    m_pszFname = CPLStrdup(pszFname);
    nFnameLen = strlen(m_pszFname);

    if (nFnameLen > 4 && (strcmp(m_pszFname+nFnameLen-4, ".TAB")==0 ||
                     strcmp(m_pszFname+nFnameLen-4, ".MAP")==0 ||
                     strcmp(m_pszFname+nFnameLen-4, ".DAT")==0 ) )
        strcpy(m_pszFname+nFnameLen-4, ".TAB");
    else if (nFnameLen > 4 && (strcmp(m_pszFname+nFnameLen-4, ".tab")==0 ||
                          strcmp(m_pszFname+nFnameLen-4, ".map")==0 ||
                          strcmp(m_pszFname+nFnameLen-4, ".dat")==0 ) )
        strcpy(m_pszFname+nFnameLen-4, ".tab");
    else
    {
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

    /*-----------------------------------------------------------------
     * Handle .TAB file... depends on access mode.
     *----------------------------------------------------------------*/
    if (m_eAccessMode == TABRead)
    {
        /*-------------------------------------------------------------
         * Open .TAB file... since it's a small text file, we will just load
         * it as a stringlist in memory.
         *------------------------------------------------------------*/
        m_papszTABFile = CSLLoad(m_pszFname);
        if (m_papszTABFile == NULL)
        {
            // Failed... an error has already been produced.
            CPLFree(m_pszFname);
            return -1;
        }
    }
    else
    {
        /*-------------------------------------------------------------
         * In Write access mode, the .TAB file will be written during the 
         * Close() call... we will just set some defaults here.
         *------------------------------------------------------------*/
        m_pszVersion = CPLStrdup("300");
        m_pszCharset = CPLStrdup("Neutral");
    }

    /*-----------------------------------------------------------------
     * Open .MAP file
     *----------------------------------------------------------------*/
    if (nFnameLen > 4 && strcmp(pszTmpFname+nFnameLen-4, ".TAB")==0)
        strcpy(pszTmpFname+nFnameLen-4, ".MAP");
    else 
        strcpy(pszTmpFname+nFnameLen-4, ".map");

#ifndef _WIN32
    TABAdjustFilenameExtension(pszTmpFname);
#endif

    m_poMAPFile = new TABMAPFile;
    m_poMAPFile->Open(pszTmpFname, pszAccess);

    if (m_poMAPFile == NULL)
    {
        // Open Failed... an error has already been reported, just return.
        CPLFree(pszTmpFname);
        Close();
        return -1;
    }

    m_nLastFeatureId = m_poMAPFile->GetMaxObjId();

    /*-----------------------------------------------------------------
     * Open .DAT file
     *----------------------------------------------------------------*/
    if (nFnameLen > 4 && strcmp(pszTmpFname+nFnameLen-4, ".MAP")==0)
        strcpy(pszTmpFname+nFnameLen-4, ".DAT");
    else 
        strcpy(pszTmpFname+nFnameLen-4, ".dat");

#ifndef _WIN32
    TABAdjustFilenameExtension(pszTmpFname);
#endif

    m_poDATFile = new TABDATFile;
    m_poDATFile->Open(pszTmpFname, pszAccess);


    if (m_poDATFile == NULL)
    {
        // Open Failed... an error has already been reported, just return.
        CPLFree(pszTmpFname);
        Close();
        return -1;
    }

    CPLFree(pszTmpFname);
    pszTmpFname = NULL;

    /*-----------------------------------------------------------------
     * Build FeatureDefn (only in read access)
     *----------------------------------------------------------------*/
    if (m_eAccessMode == TABRead && ParseTABFile() != 0)
    {
        // Failed... an error has already been reported, just return.
        CPLFree(pszTmpFname);
        Close();
        return -1;
    }

    /*-----------------------------------------------------------------
     * __TODO__ we could probably call GetSpatialRef() here to force
     * parsing the projection information... this would allow us to 
     * assignSpatialReference() on the geometries that we return.
     *----------------------------------------------------------------*/

    return 0;
}


/**********************************************************************
 *                   TABFile::ParseTABFile()
 *
 * Scan the lines of the TAB file, and store any useful information into
 * class members.  The main piece of information being the fields 
 * definition that we use to build the OGRFeatureDefn for this file.
 *
 * This private method should be used only during the Open() call.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABFile::ParseTABFile()
{
    int         iLine, numLines, numTok, nStatus;
    char        **papszTok=NULL;
    GBool       bInsideTableDef = FALSE;
    OGRFieldDefn *poFieldDefn;

    if (m_eAccessMode != TABRead)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "ParseTABFile() can be used only with Read access.");
        return -1;
    }

    m_poDefn = new OGRFeatureDefn("TABFeature");
    // Ref count defaults to 0... set it to 1
    m_poDefn->Reference();

    numLines = CSLCount(m_papszTABFile);

    for(iLine=0; iLine<numLines; iLine++)
    {
        /*-------------------------------------------------------------
         * Tokenize the next .TAB line, and check first keyword
         *------------------------------------------------------------*/
        CSLDestroy(papszTok);
        papszTok = CSLTokenizeStringComplex(m_papszTABFile[iLine], " \t(),;",
                                            TRUE, FALSE);
        if (CSLCount(papszTok) < 2)
            continue;   // All interesting lines have at least 2 tokens

        if (EQUAL(papszTok[0], "!version"))
        {
            m_pszVersion = CPLStrdup(papszTok[1]);
        }
        else if (EQUAL(papszTok[0], "!charset"))
        {
            m_pszCharset = CPLStrdup(papszTok[1]);
        }
        else if (EQUAL(papszTok[0], "Definition") &&
                 EQUAL(papszTok[1], "Table") )
        {
            bInsideTableDef = TRUE;
        }
        else if (bInsideTableDef &&
                 EQUAL(papszTok[0], "Fields"))
        {
            /*---------------------------------------------------------
             * We found the list of table fields
             *--------------------------------------------------------*/
            int iField, numFields;
            numFields = atoi(papszTok[1]);
            if (numFields < 1 || iLine+numFields >= numLines)
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Invalid number of fields (%s) at line %d in file %s",
                         papszTok[1], iLine+1, m_pszFname);
                CSLDestroy(papszTok);
                return -1;
            }

            iLine++;
            poFieldDefn = NULL;
            for(iField=0; iField<numFields; iField++, iLine++)
            {
                /*-----------------------------------------------------
                 * For each field definition found in the .TAB:
                 * Pass the info to the DAT file object.  It will validate
                 * the info with what is found in the .DAT header, and will
                 * also use this info later to interpret field values.
                 *
                 * We also create the OGRFieldDefn at the same time to 
                 * initialize the OGRFeatureDefn
                 *----------------------------------------------------*/
                CSLDestroy(papszTok);
                papszTok = CSLTokenizeStringComplex(m_papszTABFile[iLine], 
                                                    " \t(),;",
                                                    TRUE, FALSE);
                numTok = CSLCount(papszTok);
                nStatus = -1;
                CPLAssert(m_poDefn);
                if (numTok >= 3 && EQUAL(papszTok[1], "char"))
                {
                    /*-------------------------------------------------
                     * CHAR type
                     *------------------------------------------------*/
                    nStatus = m_poDATFile->ValidateFieldInfoFromTAB(iField, 
                                                               papszTok[0],
                                                               TABFChar,
                                                            atoi(papszTok[2]),
                                                               0);
                    poFieldDefn = new OGRFieldDefn(papszTok[0], OFTString);
                    poFieldDefn->SetWidth(atoi(papszTok[2]));
                }
                else if (numTok >= 2 && EQUAL(papszTok[1], "integer"))
                {
                    /*-------------------------------------------------
                     * INTEGER type
                     *------------------------------------------------*/
                    nStatus = m_poDATFile->ValidateFieldInfoFromTAB(iField, 
                                                               papszTok[0],
                                                               TABFInteger,
                                                               0,
                                                               0);
                    poFieldDefn = new OGRFieldDefn(papszTok[0], OFTInteger);
                }
                else if (numTok >= 2 && EQUAL(papszTok[1], "smallint"))
                {
                    /*-------------------------------------------------
                     * SMALLINT type
                     *------------------------------------------------*/
                    nStatus = m_poDATFile->ValidateFieldInfoFromTAB(iField, 
                                                               papszTok[0],
                                                               TABFSmallInt,
                                                               0,
                                                               0);
                    poFieldDefn = new OGRFieldDefn(papszTok[0], OFTInteger);
                }
                else if (numTok >= 4 && EQUAL(papszTok[1], "decimal"))
                {
                    /*-------------------------------------------------
                     * DECIMAL type
                     *------------------------------------------------*/
                    nStatus = m_poDATFile->ValidateFieldInfoFromTAB(iField, 
                                                               papszTok[0],
                                                               TABFDecimal,
                                                           atoi(papszTok[2]),
                                                           atoi(papszTok[3]));
                    poFieldDefn = new OGRFieldDefn(papszTok[0], OFTReal);
                    poFieldDefn->SetWidth(atoi(papszTok[2]));
                    poFieldDefn->SetPrecision(atoi(papszTok[3]));
                }
                else if (numTok >= 2 && EQUAL(papszTok[1], "float"))
                {
                    /*-------------------------------------------------
                     * FLOAT type
                     *------------------------------------------------*/
                    nStatus = m_poDATFile->ValidateFieldInfoFromTAB(iField, 
                                                               papszTok[0],
                                                               TABFFloat,
                                                               0, 0);
                    poFieldDefn = new OGRFieldDefn(papszTok[0], OFTReal);
                }
                else if (numTok >= 2 && EQUAL(papszTok[1], "date"))
                {
                    /*-------------------------------------------------
                     * DATE type (returned as a string: "DD/MM/YYYY")
                     *------------------------------------------------*/
                    nStatus = m_poDATFile->ValidateFieldInfoFromTAB(iField, 
                                                               papszTok[0],
                                                               TABFDate,
                                                               0,
                                                               0);
                    poFieldDefn = new OGRFieldDefn(papszTok[0], OFTString);
                    poFieldDefn->SetWidth(10);
                }
                else if (numTok >= 2 && EQUAL(papszTok[1], "logical"))
                {
                    /*-------------------------------------------------
                     * LOGICAL type (value "T" or "F")
                     *------------------------------------------------*/
                    nStatus = m_poDATFile->ValidateFieldInfoFromTAB(iField, 
                                                               papszTok[0],
                                                               TABFLogical,
                                                               0,
                                                               0);
                    poFieldDefn = new OGRFieldDefn(papszTok[0], OFTString);
                    poFieldDefn->SetWidth(1);
                }
                else 
                    nStatus = -1; // Unrecognized field type or line corrupt

                if (nStatus != 0)
                {
                    CPLError(CE_Failure, CPLE_FileIO,
                     "Failed to parse field definition at line %d in file %s", 
                             iLine+1, m_pszFname);
                    CSLDestroy(papszTok);
                    return -1;
                }
                /*-----------------------------------------------------
                 * Add the FieldDefn to the FeatureDefn and continue with
                 * the next one.
                 *----------------------------------------------------*/
                m_poDefn->AddFieldDefn(poFieldDefn);
                poFieldDefn = NULL;
            }

            bInsideTableDef = FALSE;
        }/* end of fields section*/
        else
        {
            // Simply Ignore unrecognized lines
        }
    }

    CSLDestroy(papszTok);

    return 0;
}

/**********************************************************************
 *                   TABFile::WriteTABFile()
 *
 * Generate the .TAB file using mainly the attribute fields definition.
 *
 * This private method should be used only during the Close() call with
 * write access mode.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABFile::WriteTABFile()
{
    FILE *fp;

    if (m_eAccessMode != TABWrite)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "WriteTABFile() can be used only with Write access.");
        return -1;
    }

    if ( (fp = VSIFOpen(m_pszFname, "wt")) != NULL)
    {
        fprintf(fp, "!table\n");
        fprintf(fp, "!version %s\n", m_pszVersion);
        fprintf(fp, "!charset %s\n", m_pszCharset);
        fprintf(fp, "\n");

        if (m_poDefn && m_poDefn->GetFieldCount() > 0)
        {
            int iField;
            OGRFieldDefn *poFieldDefn;
            const char *pszFieldType;

            fprintf(fp, "Definition Table\n");
            fprintf(fp, "  Type NATIVE Charset \"%s\"\n", m_pszCharset);
            fprintf(fp, "  Fields %d\n", m_poDefn->GetFieldCount());

            for(iField=0; iField<m_poDefn->GetFieldCount(); iField++)
            {
                poFieldDefn = m_poDefn->GetFieldDefn(iField);
                switch(GetNativeFieldType(iField))
                {
                  case TABFChar:
                    pszFieldType = CPLSPrintf("Char (%d)", 
                                              poFieldDefn->GetWidth());
                    break;
                  case TABFDecimal:
                    pszFieldType = CPLSPrintf("Decimal (%d,%d)",
                                              poFieldDefn->GetWidth(),
                                              poFieldDefn->GetPrecision());
                    break;
                  case TABFInteger:
                    pszFieldType = "Integer";
                    break;
                  case TABFSmallInt:
                    pszFieldType = "SmallInt";
                    break;
                  case TABFFloat:
                    pszFieldType = "Float";
                    break;
                  case TABFLogical:
                    pszFieldType = "Logical";
                    break;
                  case TABFDate:
                    pszFieldType = "Date";
                    break;
                  default:
                    // Unsupported field type!!!  This should never happen.
                    CPLError(CE_Failure, CPLE_AssertionFailed,
                             "WriteTABFile(): Unsupported field type");
                    VSIFClose(fp);
                    return -1;
                }

                fprintf(fp, "    %s %s ;\n", poFieldDefn->GetNameRef(), 
                                            pszFieldType );
            }
        }

        VSIFClose(fp);
    }
    else
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Failed to create file `%s'", m_pszFname);
        return -1;
    }

    return 0;
}

/**********************************************************************
 *                   TABFile::Close()
 *
 * Close current file, and release all memory used.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABFile::Close()
{
    if (m_poMAPFile == NULL)
        return 0;

    // Commit the latest changes to the file...
    
    // In Write access, it's time to write the .TAB file.
    if (m_eAccessMode == TABWrite && m_poMAPFile)
    {
        WriteTABFile();
    }

    if (m_poMAPFile)
    {
        m_poMAPFile->Close();
        delete m_poMAPFile;
        m_poMAPFile = NULL;
    }

    if (m_poDATFile)
    {
        m_poDATFile->Close();
        delete m_poDATFile;
        m_poDATFile = NULL;
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
    

    CPLFree(m_papszTABFile);
    m_papszTABFile = NULL;

    CPLFree(m_pszFname);
    m_pszFname = NULL;

    CPLFree(m_pszVersion);
    m_pszVersion = NULL;
    CPLFree(m_pszCharset);
    m_pszCharset = NULL;

    return 0;
}

/**********************************************************************
 *                   TABFile::GetNextFeatureId()
 *
 * Returns feature id that follows nPrevId, or -1 if it is the
 * last feature id.  Pass nPrevId=-1 to fetch the first valid feature id.
 **********************************************************************/
int TABFile::GetNextFeatureId(int nPrevId)
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

/**********************************************************************
 *                   TABFile::GetFeatureRef()
 *
 * Fill and return a TABFeature object for the specified feature id.
 *
 * The retruned pointer is a reference to an object owned and maintained
 * by this TABFile object.  It should not be altered or freed by the 
 * caller and its contents is guaranteed to be valid only until the next
 * call to GetFeatureRef() or Close().
 *
 * Returns NULL if the specified feature id does not exist of if an
 * error happened.  In any case, CPLError() will have been called to
 * report the reason of the failure.
 **********************************************************************/
TABFeature *TABFile::GetFeatureRef(int nFeatureId)
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
    if (m_poMAPFile == NULL)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "GetFeatureRef() failed: file is not opened!");
        return NULL;
    }


    if (nFeatureId <= 0 || nFeatureId > m_nLastFeatureId ||
        m_poMAPFile->MoveToObjId(nFeatureId) != 0 ||
        m_poDATFile->GetRecordBlock(nFeatureId) == NULL )
    {
	//     CPLError(CE_Failure, CPLE_IllegalArg,
	//    "GetFeatureRef() failed: invalid feature id %d", 
	//    nFeatureId);
        return NULL;
    }
    
    /*-----------------------------------------------------------------
     * Flush current feature object
     * __TODO__ try to reuse if it is already of the right type
     *----------------------------------------------------------------*/
    if (m_poCurFeature)
    {
        delete m_poCurFeature;
        m_poCurFeature = NULL;
    }

    /*-----------------------------------------------------------------
     * Create new feature object of the right type
     *----------------------------------------------------------------*/
    switch(m_poMAPFile->GetCurObjType())
    {
      case TAB_GEOM_NONE:
        m_poCurFeature = new TABFeature(m_poDefn);
        break;
      case TAB_GEOM_SYMBOL_C:
      case TAB_GEOM_SYMBOL:
        m_poCurFeature = new TABPoint(m_poDefn);
        break;
      case TAB_GEOM_FONTSYMBOL_C:
      case TAB_GEOM_FONTSYMBOL:
        m_poCurFeature = new TABFontPoint(m_poDefn);
        break;
      case TAB_GEOM_CUSTOMSYMBOL_C:
      case TAB_GEOM_CUSTOMSYMBOL:
        m_poCurFeature = new TABCustomPoint(m_poDefn);
        break;
      case TAB_GEOM_LINE_C:
      case TAB_GEOM_LINE:
      case TAB_GEOM_PLINE_C:
      case TAB_GEOM_PLINE:
      case TAB_GEOM_MULTIPLINE_C:
      case TAB_GEOM_MULTIPLINE:
       m_poCurFeature = new TABPolyline(m_poDefn);
        break;
      case TAB_GEOM_ARC_C:
      case TAB_GEOM_ARC:
        m_poCurFeature = new TABArc(m_poDefn);
        break;

      case TAB_GEOM_REGION_C:
      case TAB_GEOM_REGION:
        m_poCurFeature = new TABRegion(m_poDefn);
        break;
      case TAB_GEOM_RECT_C:
      case TAB_GEOM_RECT:
      case TAB_GEOM_ROUNDRECT_C:
      case TAB_GEOM_ROUNDRECT:
        m_poCurFeature = new TABRectangle(m_poDefn);
        break;
      case TAB_GEOM_ELLIPSE_C:
      case TAB_GEOM_ELLIPSE:
        m_poCurFeature = new TABEllipse(m_poDefn);
        break;
      case TAB_GEOM_TEXT_C:
      case TAB_GEOM_TEXT:
        m_poCurFeature = new TABText(m_poDefn);
        break;
      default:
//        m_poCurFeature = new TABDebugFeature(m_poDefn);

        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported object type %d (0x%2.2x)", 
                 m_poMAPFile->GetCurObjType(), m_poMAPFile->GetCurObjType() );
        return NULL;
    }

    /*-----------------------------------------------------------------
     * Read fields from the .DAT file
     * GetRecordBlock() has already been called above...
     *----------------------------------------------------------------*/
    if (m_poCurFeature->ReadRecordFromDATFile(m_poDATFile) != 0)
    {
        delete m_poCurFeature;
        m_poCurFeature = NULL;
        return NULL;
    }

    /*-----------------------------------------------------------------
     * Read geometry from the .MAP file
     * MoveToObjId() has already been called above...
     *----------------------------------------------------------------*/
    if (m_poCurFeature->ReadGeometryFromMAPFile(m_poMAPFile) != 0)
    {
        delete m_poCurFeature;
        m_poCurFeature = NULL;
        return NULL;
    }

    m_nCurFeatureId = nFeatureId;
    m_poCurFeature->SetFID(m_nCurFeatureId);
    return m_poCurFeature;
}

/**********************************************************************
 *                   TABFile::SetFeature()
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
int TABFile::SetFeature(TABFeature *poFeature, int nFeatureId /*=-1*/)
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
    if (m_poMAPFile == NULL)
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
            SetFeatureDefn(poFeature->GetDefnRef(), NULL);

        /*-------------------------------------------------------------
         * Make sure table contains at least one field... this is a
         * MAPInfo requirement.
         *------------------------------------------------------------*/
        if (m_poDefn == NULL || m_poDefn->GetFieldCount() == 0)
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "MapInfo tables must contain at least 1 column.");
            return -1;
        }

        nFeatureId = m_nLastFeatureId = 1;
    }
    else
    {
        nFeatureId = ++ m_nLastFeatureId;
    }


    /*-----------------------------------------------------------------
     * Write fields to the .DAT file
     *----------------------------------------------------------------*/
    if (m_poDATFile == NULL ||
        m_poDATFile->GetRecordBlock(nFeatureId) == NULL ||
        poFeature->WriteRecordToDATFile(m_poDATFile) != 0 ||
        m_poDATFile->CommitRecordToFile() != 0 )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Failed writing attributes for feature id %d in %s",
                 nFeatureId, m_pszFname);
        return -1;
    }

    /*-----------------------------------------------------------------
     * Write geometry to the .MAP file
     * The call to PrepareNewObj() takes care of the .ID file.
     *----------------------------------------------------------------*/
    if (m_poMAPFile == NULL ||
        m_poMAPFile->PrepareNewObj(nFeatureId,
                                   poFeature->ValidateMapInfoType()) != 0 ||
        poFeature->WriteGeometryToMAPFile(m_poMAPFile) != 0)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Failed writing geometry for feature id %d in %s",
                 nFeatureId, m_pszFname);
        return -1;
    }

    return nFeatureId;
}



/**********************************************************************
 *                   TABFile::GetLayerDefn()
 *
 * Returns a reference to the OGRFeatureDefn that will be used to create
 * features in this dataset.
 *
 * Returns a reference to an object that is maintained by this TABFile
 * object (and thus should not be modified or freed by the caller) or
 * NULL if the OGRFeatureDefn has not been initialized yet (i.e. no file
 * opened yet)
 **********************************************************************/
OGRFeatureDefn *TABFile::GetLayerDefn()
{
    return m_poDefn;
}

/**********************************************************************
 *                   TABFile::SetFeatureDefn()
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
int TABFile::SetFeatureDefn(OGRFeatureDefn *poFeatureDefn,
                         TABFieldType *paeMapInfoNativeFieldTypes /* =NULL */)
{
    int           iField, numFields;
    OGRFieldDefn *poFieldDefn;
    TABFieldType eMapInfoType = TABFUnknown;
    int nStatus = 0;

    if (m_eAccessMode != TABWrite)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetFeatureDefn() can be used only with Write access.");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Keep a reference to the OGRFeatureDefn... we'll have to take the
     * reference count into account when we are done with it.
     *----------------------------------------------------------------*/
    if (m_poDefn && m_poDefn->Dereference() == 0)
        delete m_poDefn;

    m_poDefn = poFeatureDefn;
    m_poDefn->Reference();

    /*-----------------------------------------------------------------
     * Pass field information to the .DAT file, after making sure that
     * it has been created and that it does not contain any field
     * definition yet.
     *----------------------------------------------------------------*/
    if (m_poDATFile== NULL || m_poDATFile->GetNumFields() > 0 )
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "SetFeatureDefn() can be called only once in a newly "
                 "created dataset.");
        return -1;
    }

    numFields = poFeatureDefn->GetFieldCount();
    for(iField=0; nStatus==0 && iField < numFields; iField++)
    {
        poFieldDefn = m_poDefn->GetFieldDefn(iField);

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
              case OFTString:
              default:
                eMapInfoType = TABFChar;
            }
        }

        nStatus = m_poDATFile->AddField(poFieldDefn->GetNameRef(),
                                            eMapInfoType,
                                            poFieldDefn->GetWidth(),
                                            poFieldDefn->GetPrecision());
    }

    return nStatus;
}

/**********************************************************************
 *                   TABFile::AddFieldNative()
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
int TABFile::AddFieldNative(const char *pszName, TABFieldType eMapInfoType,
                            int nWidth, int nPrecision /*=0*/)
{
    OGRFieldDefn *poFieldDefn;
    int nStatus = 0;

    if (m_eAccessMode != TABWrite)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetFeatureDefn() can be used only with Write access.");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Check that call happens at the right time in dataset's life.
     *----------------------------------------------------------------*/
    if (m_eAccessMode != TABWrite || 
        m_nLastFeatureId > 0 || m_poDATFile == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "AddFieldNative() must be called after opening a new "
                 "dataset, but before writing the first feature to it.");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Create new OGRFeatureDefn if not done yet...
     *----------------------------------------------------------------*/
    if (m_poDefn== NULL)
    {
        m_poDefn = new OGRFeatureDefn("TABFeature");
    }

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
        poFieldDefn = new OGRFieldDefn(pszName, OFTString);
        poFieldDefn->SetWidth(nWidth);
        break;
      case TABFInteger:
        /*-------------------------------------------------
         * INTEGER type
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(pszName, OFTInteger);
        break;
      case TABFSmallInt:
        /*-------------------------------------------------
         * SMALLINT type
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(pszName, OFTInteger);
        break;
      case TABFDecimal:
        /*-------------------------------------------------
         * DECIMAL type
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(pszName, OFTReal);
        poFieldDefn->SetWidth(nWidth);
        poFieldDefn->SetPrecision(nPrecision);
        break;
      case TABFFloat:
        /*-------------------------------------------------
         * FLOAT type
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(pszName, OFTReal);
        break;
      case TABFDate:
        /*-------------------------------------------------
         * DATE type (returned as a string: "DD/MM/YYYY")
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(pszName, OFTString);
        poFieldDefn->SetWidth(10);
        break;
      case TABFLogical:
        /*-------------------------------------------------
         * LOGICAL type (value "T" or "F")
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(pszName, OFTString);
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

    /*-----------------------------------------------------
     * ... and pass field info to the .DAT file.
     *----------------------------------------------------*/
    nStatus = m_poDATFile->AddField(pszName, eMapInfoType, nWidth, nPrecision);
 
    return nStatus;
}


/**********************************************************************
 *                   TABFile::GetNativeFieldType()
 *
 * Returns the native MapInfo field type for the specified field.
 *
 * Returns TABFUnknown if file is not opened, or if specified field index is
 * invalid.
 **********************************************************************/
TABFieldType TABFile::GetNativeFieldType(int nFieldId)
{
    if (m_poDATFile)
    {
        return m_poDATFile->GetFieldType(nFieldId);
    }
    return TABFUnknown;
}


/**********************************************************************
 *                   TABFile::SetBounds()
 *
 * Set projection coordinates bounds of the newly created dataset.
 *
 * This function must be called after creating a new dataset and before any
 * feature can be written to it.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABFile::SetBounds(double dXMin, double dYMin, 
                       double dXMax, double dYMax)
{
    if (m_eAccessMode != TABWrite)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetBounds() can be used only with Write access.");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Check that dataset has been created but no feature set yet.
     *----------------------------------------------------------------*/
    if (m_poMAPFile && m_nLastFeatureId < 1)
    {
        m_poMAPFile->SetCoordsysBounds(dXMin, dYMin, dXMax, dYMax);

        m_bBoundsSet = TRUE;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "SetBounds() can be called only after dataset has been "
                 "created and before any feature is set.");
        return -1;
    }

    return 0;
}


/**********************************************************************
 *                   TABFile::GetBounds()
 *
 * Fetch projection coordinates bounds of a dataset.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABFile::GetBounds(double &dXMin, double &dYMin, 
                       double &dXMax, double &dYMax)
{
    TABMAPHeaderBlock *poHeader;

    if (m_poMAPFile && (poHeader=m_poMAPFile->GetHeaderBlock()) != NULL)
    {
        m_poMAPFile->Int2Coordsys(poHeader->m_nXMin, poHeader->m_nYMin, 
                                  dXMin, dYMin);
        m_poMAPFile->Int2Coordsys(poHeader->m_nXMax, poHeader->m_nYMax, 
                                  dXMax, dYMax);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
             "GetBounds() can be called only after dataset has been opened.");
        return -1;
    }

    return 0;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int TABFile::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    else if( EQUAL(pszCap,OLCSequentialWrite) 
             || EQUAL(pszCap,OLCRandomWrite) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return m_poFilterGeom == NULL;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return FALSE;

    else 
        return FALSE;
}






/**********************************************************************
 *                   TABFile::Dump()
 *
 * Dump block contents... available only in DEBUG mode.
 **********************************************************************/
#ifdef DEBUG

void TABFile::Dump(FILE *fpOut /*=NULL*/)
{
    if (fpOut == NULL)
        fpOut = stdout;

    fprintf(fpOut, "----- TABFile::Dump() -----\n");

    if (m_poMAPFile == NULL)
    {
        fprintf(fpOut, "File is not opened.\n");
    }
    else
    {
        fprintf(fpOut, "File is opened: %s\n", m_pszFname);
        fprintf(fpOut, "Associated .DAT file ...\n\n");
        m_poDATFile->Dump(fpOut);
        fprintf(fpOut, "... end of .DAT file dump.\n\n");
        if( GetSpatialRef() != NULL )
        {
            char	*pszWKT;

            GetSpatialRef()->exportToWkt( &pszWKT );
            fprintf( fpOut, "SRS = %s\n", pszWKT );
            OGRFree( pszWKT );						
        }
        fprintf(fpOut, "Associated .MAP file ...\n\n");
        m_poMAPFile->Dump(fpOut);
        fprintf(fpOut, "... end of .MAP file dump.\n\n");

    }

    fflush(fpOut);
}

#endif // DEBUG
