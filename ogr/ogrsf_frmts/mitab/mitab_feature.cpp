/**********************************************************************
 * $Id: mitab_feature.cpp,v 1.14 1999/11/14 04:47:54 daniel Exp $
 *
 * Name:     mitab_feature.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation of the feature classes specific to MapInfo files.
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
 * $Log: mitab_feature.cpp,v $
 * Revision 1.14  1999/11/14 04:47:54  daniel
 * Fixed precision in writing angles.  Also changed the way ARCs start/end
 * angles are handled on read and write.
 *
 * Revision 1.13  1999/11/08 04:41:46  stephane
 * Modification in arc GeometryType
 *
 * Revision 1.12  1999/11/05 05:54:14  daniel
 * Fixed TABArc to expect wkbLineString instead of wkbPolygon geometry
 *
 * Revision 1.11  1999/10/19 06:13:38  daniel
 * Removed obsolete code and comments related to angles vs flipped axis
 *
 * Revision 1.10  1999/10/18 15:43:03  daniel
 * Several fixes/improvements mostly for writing of Arc/Ellipses/Text/Symbols
 *
 * Revision 1.9  1999/10/06 15:17:59  daniel
 * Fixed order of args in calls to GetFeatureMBR()
 *
 * Revision 1.8  1999/10/06 13:15:54  daniel
 * Added several Get/Set() methods to feature classes
 *
 * Revision 1.7  1999/10/01 03:54:46  daniel
 * Moved fix for writing string fields down in TABDATFile::WriteCharField()
 *
 * Revision 1.6  1999/10/01 02:09:25  warmerda
 * Ensure that WriteRecordToDATFile() doesn't try to write more bytes than
 * are returned by GetFieldAsString().
 *
 * Revision 1.5  1999/09/29 17:37:18  daniel
 * Fixed warnings
 *
 * Revision 1.4  1999/09/26 14:59:36  daniel
 * Implemented write support
 *
 * Revision 1.3  1999/09/16 02:39:16  daniel
 * Completed read support for most feature types
 *
 * Revision 1.2  1999/09/01 17:49:24  daniel
 * Changes to work with latest OGR
 *
 * Revision 1.1  1999/07/12 04:18:24  daniel
 * Initial checkin
 *
 **********************************************************************/

#include "mitab.h"
#include "mitab_utils.h"

/*=====================================================================
 *                      class TABFeature
 *====================================================================*/


/**********************************************************************
 *                   TABFeature::TABFeature()
 *
 * Constructor.
 **********************************************************************/
TABFeature::TABFeature(OGRFeatureDefn *poDefnIn):
               OGRFeature(poDefnIn)
{
    m_nMapInfoType = TAB_GEOM_NONE;

    SetMBR(0.0, 0.0, 0.0, 0.0);
}

/**********************************************************************
 *                   TABFeature::~TABFeature()
 *
 * Destructor.
 **********************************************************************/
TABFeature::~TABFeature()
{
}

/**********************************************************************
 *                   TABFeature::SetMBR()
 *
 * Set the values for the MBR corners for this feature.
 **********************************************************************/
void TABFeature::SetMBR(double dXMin, double dYMin, 
                        double dXMax, double dYMax)
{
    m_dXMin = MIN(dXMin, dXMax);
    m_dYMin = MIN(dYMin, dYMax);
    m_dXMax = MAX(dXMin, dXMax);
    m_dYMax = MAX(dYMin, dYMax);
}

/**********************************************************************
 *                   TABFeature::GetMBR()
 *
 * Return the values for the MBR corners for this feature.
 **********************************************************************/
void TABFeature::GetMBR(double &dXMin, double &dYMin, 
                        double &dXMax, double &dYMax)
{
    dXMin = m_dXMin;
    dYMin = m_dYMin;
    dXMax = m_dXMax;
    dYMax = m_dYMax;
}

/**********************************************************************
 *                   TABFeature::ReadRecordFromDATFile()
 *
 * Fill the fields part of the feature from the contents of the 
 * table record pointed to by poDATFile.
 *
 * It is assumed that poDATFile currently points to the beginning of
 * the table record and that this feature's OGRFeatureDefn has been 
 * properly initialized for this table.
 **********************************************************************/
int TABFeature::ReadRecordFromDATFile(TABDATFile *poDATFile)
{
    int         iField, numFields, nValue;
    double      dValue;
    const char *pszValue;

    CPLAssert(poDATFile);

    numFields = poDATFile->GetNumFields();

    for(iField=0; iField<numFields; iField++)
    {
        switch(poDATFile->GetFieldType(iField))
        {
          case TABFChar:
            pszValue = poDATFile->ReadCharField(poDATFile->
                                                GetFieldWidth(iField));
            SetField(iField, pszValue);
            break;
          case TABFDecimal:
            dValue = poDATFile->ReadDecimalField(poDATFile->
                                                 GetFieldWidth(iField));
            SetField(iField, dValue);
            break;
          case TABFInteger:
            nValue = poDATFile->ReadIntegerField();
            SetField(iField, nValue);
            break;
          case TABFSmallInt:
            nValue = poDATFile->ReadSmallIntField();
            SetField(iField, nValue);
            break;
          case TABFFloat:
            dValue = poDATFile->ReadFloatField();
            SetField(iField, dValue);
            break;
          case TABFLogical:
            pszValue = poDATFile->ReadLogicalField();
            SetField(iField, pszValue);
            break;
          case TABFDate:
            pszValue = poDATFile->ReadDateField();
            SetField(iField, pszValue);
            break;
          default:
            // Other type???  Impossible!
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "Unsupported field type!");
        }

    }

    return 0;
}

/**********************************************************************
 *                   TABFeature::WriteRecordToDATFile()
 *
 * Write the attribute part of the feature to the .DAT file.
 *
 * It is assumed that poDATFile currently points to the beginning of
 * the table record and that this feature's OGRFeatureDefn has been 
 * properly initialized for this table.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABFeature::WriteRecordToDATFile(TABDATFile *poDATFile)
{
    int         iField, numFields, nStatus=0;

    CPLAssert(poDATFile);

    numFields = poDATFile->GetNumFields();

    for(iField=0; nStatus == 0 && iField<numFields; iField++)
    {
        switch(poDATFile->GetFieldType(iField))
        {
          case TABFChar:
            nStatus = poDATFile->WriteCharField(GetFieldAsString(iField),
                                      poDATFile->GetFieldWidth(iField));
            break;
          case TABFDecimal:
            nStatus = poDATFile->WriteDecimalField(GetFieldAsDouble(iField),
                                      poDATFile->GetFieldWidth(iField),
                                      poDATFile->GetFieldPrecision(iField));
            break;
          case TABFInteger:
            nStatus = poDATFile->WriteIntegerField(GetFieldAsInteger(iField));
            break;
          case TABFSmallInt:
            nStatus = poDATFile->WriteSmallIntField(GetFieldAsInteger(iField));
            break;
          case TABFFloat:
            nStatus = poDATFile->WriteFloatField(GetFieldAsDouble(iField));
            break;
          case TABFLogical:
            nStatus = poDATFile->WriteLogicalField(GetFieldAsString(iField));
            break;
          case TABFDate:
            nStatus = poDATFile->WriteDateField(GetFieldAsString(iField));
            break;
          default:
            // Other type???  Impossible!
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "Unsupported field type!");
        }

    }

    return 0;
}

/**********************************************************************
 *                   TABFeature::ReadGeometryFromMAPFile()
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
int TABFeature::ReadGeometryFromMAPFile(TABMAPFile * /*poMapFile*/)
{
    /*-----------------------------------------------------------------
     * Nothing to do... instances of TABFeature objects contain no geometry.
     *----------------------------------------------------------------*/

    return 0;
}

/**********************************************************************
 *                   TABFeature::WriteGeometryToMAPFile()
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
 * objects contain no geometry (i.e. TAB_GEOM_NONE).
 * 
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABFeature::WriteGeometryToMAPFile(TABMAPFile * /* poMapFile*/)
{
    /*-----------------------------------------------------------------
     * Nothing to do... instances of TABFeature objects contain no geometry.
     *----------------------------------------------------------------*/

    return 0;
}

/**********************************************************************
 *                   TABFeature::DumpMID()
 *
 * Dump feature attributes in a format similar to .MID data records.
 **********************************************************************/
void TABFeature::DumpMID(FILE *fpOut /*=NULL*/)
{
    OGRFeatureDefn 	*poDefn = GetDefnRef();

    if (fpOut == NULL)
        fpOut = stdout;

    for( int iField = 0; iField < GetFieldCount(); iField++ )
    {
        OGRFieldDefn	*poFDefn = poDefn->GetFieldDefn(iField);
        
        fprintf( fpOut, "  %s (%s) = %s\n",
                 poFDefn->GetNameRef(),
                 OGRFieldDefn::GetFieldTypeName(poFDefn->GetType()),
                 GetFieldAsString( iField ) );
    }

    fflush(fpOut);
}

/**********************************************************************
 *                   TABFeature::DumpMIF()
 *
 * Dump feature geometry in a format similar to .MIF files.
 **********************************************************************/
void TABFeature::DumpMIF(FILE *fpOut /*=NULL*/)
{
    if (fpOut == NULL)
        fpOut = stdout;

    /*-----------------------------------------------------------------
     * Generate output... not much to do, feature contains no geometry.
     *----------------------------------------------------------------*/
    fprintf(fpOut, "NONE\n" );

    fflush(fpOut);
}


/*=====================================================================
 *                      class TABPoint
 *====================================================================*/


/**********************************************************************
 *                   TABPoint::TABPoint()
 *
 * Constructor.
 **********************************************************************/
TABPoint::TABPoint(OGRFeatureDefn *poDefnIn):
              TABFeature(poDefnIn)
{
}

/**********************************************************************
 *                   TABPoint::~TABPoint()
 *
 * Destructor.
 **********************************************************************/
TABPoint::~TABPoint()
{
}

/**********************************************************************
 *                   TABPoint::ValidateMapInfoType()
 *
 * Check the feature's geometry part and return the corresponding
 * mapinfo object type code.  The m_nMapInfoType member will also
 * be updated for further calls to GetMapInfoType();
 *
 * Returns TAB_GEOM_NONE if the geometry is not compatible with what
 * is expected for this object class.
 **********************************************************************/
int  TABPoint::ValidateMapInfoType()
{
    OGRGeometry *poGeom;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && poGeom->getGeometryType() == wkbPoint)
    {
        switch(GetFeatureClass())
        {
          case TABFCFontPoint:
            m_nMapInfoType = TAB_GEOM_FONTSYMBOL;
            break;
          case TABFCCustomPoint:
            m_nMapInfoType = TAB_GEOM_CUSTOMSYMBOL;
            break;
          case TABFCPoint:
          default:
            m_nMapInfoType = TAB_GEOM_SYMBOL;
            break;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABPoint: Missing or Invalid Geometry!");
        m_nMapInfoType = TAB_GEOM_NONE;
    }

    return m_nMapInfoType;
}

/**********************************************************************
 *                   TABPoint::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABPoint::ReadGeometryFromMAPFile(TABMAPFile *poMapFile)
{
    GInt32              nX, nY;
    double              dX, dY;
    OGRGeometry         *poGeometry;
    TABMAPObjectBlock   *poObjBlock;
    GBool               bComprCoord;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poMapFile->GetCurObjType();

    poObjBlock = poMapFile->GetCurObjBlock();

    bComprCoord = (m_nMapInfoType == TAB_GEOM_SYMBOL_C);

    /*-----------------------------------------------------------------
     * Read object information
     *----------------------------------------------------------------*/
    if (m_nMapInfoType == TAB_GEOM_SYMBOL ||
        m_nMapInfoType == TAB_GEOM_SYMBOL_C )
    {
        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);
        m_nSymbolDefIndex = poObjBlock->ReadByte();   // Symbol index
        poMapFile->ReadSymbolDef(m_nSymbolDefIndex, &m_sSymbolDef);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
           "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
                 m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

    /*-----------------------------------------------------------------
     * Create and fill geometry object
     *----------------------------------------------------------------*/
    poMapFile->Int2Coordsys(nX, nY, dX, dY);
    poGeometry = new OGRPoint(dX, dY);
    
    SetGeometryDirectly(poGeometry);

    SetMBR(dX, dY, dX, dY);

    return 0;
}

/**********************************************************************
 *                   TABPoint::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABPoint::WriteGeometryToMAPFile(TABMAPFile *poMapFile)
{
    GInt32              nX, nY;
    OGRGeometry         *poGeom;
    OGRPoint            *poPoint;
    TABMAPObjectBlock   *poObjBlock;

    if (ValidateMapInfoType() == TAB_GEOM_NONE)
        return -1;      // Invalid Geometry... an error has already been sent

    poObjBlock = poMapFile->GetCurObjBlock();

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

    poMapFile->Coordsys2Int(poPoint->getX(), poPoint->getY(), nX, nY);

    /*-----------------------------------------------------------------
     * Write object information
     *----------------------------------------------------------------*/
    poObjBlock->WriteIntCoord(nX, nY);

    m_nSymbolDefIndex = poMapFile->WriteSymbolDef(&m_sSymbolDef);
    poObjBlock->WriteByte(m_nSymbolDefIndex);      // Symbol index

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}


/**********************************************************************
 *                   TABPoint::GetX()
 *
 * Return this point's X coordinate.
 **********************************************************************/
double TABPoint::GetX()
{
    OGRGeometry *poGeom;
    OGRPoint    *poPoint=NULL;

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
        return 0.0;
    }

    return poPoint->getX();
}

/**********************************************************************
 *                   TABPoint::GetY()
 *
 * Return this point's Y coordinate.
 **********************************************************************/
double TABPoint::GetY()
{
    OGRGeometry *poGeom;
    OGRPoint    *poPoint=NULL;

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
        return 0.0;
    }

    return poPoint->getY();
}



/**********************************************************************
 *                   TABPoint::DumpMIF()
 *
 * Dump feature geometry in a format similar to .MIF POINTs.
 **********************************************************************/
void TABPoint::DumpMIF(FILE *fpOut /*=NULL*/)
{
    OGRGeometry *poGeom;
    OGRPoint    *poPoint;

    if (fpOut == NULL)
        fpOut = stdout;

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
        return;
    }

    /*-----------------------------------------------------------------
     * Generate output
     *----------------------------------------------------------------*/
    fprintf(fpOut, "POINT %g %g\n", poPoint->getX(), poPoint->getY() );

    DumpSymbolDef(fpOut);

    /*-----------------------------------------------------------------
     * Handle stuff specific to derived classes
     *----------------------------------------------------------------*/
    if (GetFeatureClass() == TABFCFontPoint)
    {
        TABFontPoint *poFeature = (TABFontPoint *)this;
        fprintf(fpOut, "  m_nFontStyle     = 0x%2.2x (%d)\n", 
                poFeature->GetFontStyleTABValue(),
                poFeature->GetFontStyleTABValue());

        poFeature->DumpFontDef(fpOut);
    }
    if (GetFeatureClass() == TABFCCustomPoint)
    {
        TABCustomPoint *poFeature = (TABCustomPoint *)this;

        fprintf(fpOut, "  m_nUnknown_      = 0x%2.2x (%d)\n", 
                poFeature->m_nUnknown_, poFeature->m_nUnknown_);
        fprintf(fpOut, "  m_nCustomStyle   = 0x%2.2x (%d)\n", 
                poFeature->m_nCustomStyle, poFeature->m_nCustomStyle);

        poFeature->DumpFontDef(fpOut);
    }

    fflush(fpOut);
}

/*=====================================================================
 *                      class TABFontPoint
 *====================================================================*/


/**********************************************************************
 *                   TABFontPoint::TABFontPoint()
 *
 * Constructor.
 **********************************************************************/
TABFontPoint::TABFontPoint(OGRFeatureDefn *poDefnIn):
              TABPoint(poDefnIn)
{
    m_nFontStyle = 0;
    m_dAngle = 0.0;
}

/**********************************************************************
 *                   TABFontPoint::~TABFontPoint()
 *
 * Destructor.
 **********************************************************************/
TABFontPoint::~TABFontPoint()
{
}

/**********************************************************************
 *                   TABFontPoint::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABFontPoint::ReadGeometryFromMAPFile(TABMAPFile *poMapFile)
{
    GInt32              nX, nY;
    double              dX, dY;
    OGRGeometry         *poGeometry;
    TABMAPObjectBlock   *poObjBlock;
    GBool               bComprCoord;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poMapFile->GetCurObjType();

    poObjBlock = poMapFile->GetCurObjBlock();

    bComprCoord = (m_nMapInfoType == TAB_GEOM_FONTSYMBOL_C );

    /*-----------------------------------------------------------------
     * Read object information
     * NOTE: This symbol type does not contain a reference to a
     * SymbolDef block in the file, but we still use the m_sSymbolDef
     * structure to store the information inside the class so that the
     * ITABFeatureSymbol methods work properly for the class user.
     *----------------------------------------------------------------*/
    if (m_nMapInfoType == TAB_GEOM_FONTSYMBOL ||
        m_nMapInfoType == TAB_GEOM_FONTSYMBOL_C )
    {
        m_nSymbolDefIndex = -1;
        m_sSymbolDef.nRefCount = 0;

        m_sSymbolDef.nSymbolNo  = poObjBlock->ReadByte();  // shape
        m_sSymbolDef.nPointSize = poObjBlock->ReadByte();  // point size

        m_nFontStyle            = poObjBlock->ReadInt16();  // font style

        m_sSymbolDef.rgbColor   = poObjBlock->ReadByte()*256*256 +
                                  poObjBlock->ReadByte()*256 +
                                  poObjBlock->ReadByte();

        poObjBlock->ReadByte();         // ??? BG Color ???
        poObjBlock->ReadByte();         // ???
        poObjBlock->ReadByte();         // ???

        /*-------------------------------------------------------------
         * Symbol Angle, in thenths of degree.
         * Contrary to arc start/end angles, no conversion based on 
         * origin quadrant is required here
         *------------------------------------------------------------*/
        m_dAngle       = poObjBlock->ReadInt16()/10.0;

        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);

        m_nFontDefIndex = poObjBlock->ReadByte();      // Font name index
        poMapFile->ReadFontDef(m_nFontDefIndex, &m_sFontDef);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
           "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
                 m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

    /*-----------------------------------------------------------------
     * Create and fill geometry object
     *----------------------------------------------------------------*/
    poMapFile->Int2Coordsys(nX, nY, dX, dY);
    poGeometry = new OGRPoint(dX, dY);
    
    SetGeometryDirectly(poGeometry);

    SetMBR(dX, dY, dX, dY);

    return 0;
}

/**********************************************************************
 *                   TABFontPoint::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABFontPoint::WriteGeometryToMAPFile(TABMAPFile *poMapFile)
{
    GInt32              nX, nY;
    TABMAPObjectBlock   *poObjBlock;
    OGRGeometry         *poGeom;
    OGRPoint            *poPoint;

    if (ValidateMapInfoType() == TAB_GEOM_NONE)
        return -1;      // Invalid Geometry... an error has already been sent

    poObjBlock = poMapFile->GetCurObjBlock();

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && poGeom->getGeometryType() == wkbPoint)
        poPoint = (OGRPoint*)poGeom;
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABFontPoint: Missing or Invalid Geometry!");
        return -1;
    }

    poMapFile->Coordsys2Int(poPoint->getX(), poPoint->getY(), nX, nY);

    /*-----------------------------------------------------------------
     * Write object information
     * NOTE: This symbol type does not contain a reference to a
     * SymbolDef block in the file, but we still use the m_sSymbolDef
     * structure to store the information inside the class so that the
     * ITABFeatureSymbol methods work properly for the class user.
     *----------------------------------------------------------------*/
    poObjBlock->WriteByte((GByte)m_sSymbolDef.nSymbolNo);   // shape
    poObjBlock->WriteByte((GByte)m_sSymbolDef.nPointSize);  // point size

    poObjBlock->WriteInt16(m_nFontStyle);            // font style

    poObjBlock->WriteByte( COLOR_R(m_sSymbolDef.rgbColor) );
    poObjBlock->WriteByte( COLOR_G(m_sSymbolDef.rgbColor) );
    poObjBlock->WriteByte( COLOR_B(m_sSymbolDef.rgbColor) );

    poObjBlock->WriteByte( 0 );
    poObjBlock->WriteByte( 0 );
    poObjBlock->WriteByte( 0 );
    
    /*-------------------------------------------------------------
     * Symbol Angle, in thenths of degree.
     * Contrary to arc start/end angles, no conversion based on 
     * origin quadrant is required here
     *------------------------------------------------------------*/
   poObjBlock->WriteInt16(ROUND_INT(m_dAngle * 10.0));

    poObjBlock->WriteIntCoord(nX, nY);

    // Write Font Def
    m_nFontDefIndex = poMapFile->WriteFontDef(&m_sFontDef);
    poObjBlock->WriteByte(m_nFontDefIndex);      // Font name index

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABFontPoint::QueryFontStyle()
 *
 * Return TRUE if the specified font style attribute is turned ON,
 * or FALSE otherwise.  See enum TABFontStyle for the list of styles
 * that can be queried on.
 **********************************************************************/
GBool TABFontPoint::QueryFontStyle(TABFontStyle eStyleToQuery)
{
    return (m_nFontStyle & (int)eStyleToQuery) ? TRUE: FALSE;
}

void TABFontPoint::ToggleFontStyle(TABFontStyle eStyleToToggle, GBool bStyleOn)
{
    if (bStyleOn)
        m_nFontStyle |=  (int)eStyleToToggle;
    else
        m_nFontStyle &=  ~(int)eStyleToToggle;
}

/**********************************************************************
 *                   TABFontPoint::GetFontStyleMIFValue()
 *
 * Return the Font Style value for this object using the style values
 * that are used in a MIF FONT() clause.  See MIF specs (appendix A).
 *
 * The reason why we have to differentiate between the TAB and the MIF font
 * style values is that in TAB, TABFSBox is included in the style value
 * as code 0x100, but in MIF it is not included, instead it is implied by
 * the presence of the BG color in the FONT() clause (the BG color is 
 * present only when TABFSBox or TABFSHalo is set).
 * This also has the effect of shifting all the other style values > 0x100
 * by 1 byte.
 *
 * NOTE: Even if there is no BG color for font symbols, we inherit this
 * problem because Font Point styles use the same codes as Text Font styles.
 **********************************************************************/
int TABFontPoint::GetFontStyleMIFValue()
{
    // The conversion is simply to remove bit 0x100 from the value and shift
    // down all values past this bit.
    return (m_nFontStyle & 0xff) + (m_nFontStyle & (0xff00-0x0100))/2;
}

void TABFontPoint:: SetFontStyleMIFValue(int nStyle)
{
    m_nFontStyle = (nStyle & 0xff) + (nStyle & 0x7f00)*2;
}

/**********************************************************************
 *                   TABFontPoint::SetSymbolAngle()
 *
 * Set the symbol angle value in degrees, making sure the value is
 * always in the range [0..360]
 **********************************************************************/
void TABFontPoint::SetSymbolAngle(double dAngle)
{
    while(dAngle < 0.0)   dAngle += 360.0;
    while(dAngle > 360.0) dAngle -= 360.0;

    m_dAngle = dAngle;
}



/*=====================================================================
 *                      class TABCustomPoint
 *====================================================================*/


/**********************************************************************
 *                   TABCustomPoint::TABCustomPoint()
 *
 * Constructor.
 **********************************************************************/
TABCustomPoint::TABCustomPoint(OGRFeatureDefn *poDefnIn):
                    TABPoint(poDefnIn)
{
    m_nUnknown_ = m_nCustomStyle = 0;
}

/**********************************************************************
 *                   TABCustomPoint::~TABCustomPoint()
 *
 * Destructor.
 **********************************************************************/
TABCustomPoint::~TABCustomPoint()
{
}

/**********************************************************************
 *                   TABCustomPoint::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABCustomPoint::ReadGeometryFromMAPFile(TABMAPFile *poMapFile)
{
    GInt32              nX, nY;
    double              dX, dY;
    OGRGeometry         *poGeometry;
    TABMAPObjectBlock   *poObjBlock;
    GBool               bComprCoord;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poMapFile->GetCurObjType();

    poObjBlock = poMapFile->GetCurObjBlock();

    bComprCoord = (m_nMapInfoType == TAB_GEOM_CUSTOMSYMBOL_C);

    /*-----------------------------------------------------------------
     * Read object information
     *----------------------------------------------------------------*/
    if (m_nMapInfoType == TAB_GEOM_CUSTOMSYMBOL ||
        m_nMapInfoType == TAB_GEOM_CUSTOMSYMBOL_C )
    {
        m_nUnknown_    = poObjBlock->ReadByte();  // ??? 
        m_nCustomStyle = poObjBlock->ReadByte();  // 0x01=Show BG,
                                                  // 0x02=Apply Color

        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);

        m_nSymbolDefIndex = poObjBlock->ReadByte();   // Symbol index
        poMapFile->ReadSymbolDef(m_nSymbolDefIndex, &m_sSymbolDef);

        m_nFontDefIndex = poObjBlock->ReadByte();    // Font index
        poMapFile->ReadFontDef(m_nFontDefIndex, &m_sFontDef);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
           "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
                 m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

    /*-----------------------------------------------------------------
     * Create and fill geometry object
     *----------------------------------------------------------------*/
    poMapFile->Int2Coordsys(nX, nY, dX, dY);
    poGeometry = new OGRPoint(dX, dY);
    
    SetGeometryDirectly(poGeometry);

    SetMBR(dX, dY, dX, dY);

    return 0;
}

/**********************************************************************
 *                   TABCustomPoint::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABCustomPoint::WriteGeometryToMAPFile(TABMAPFile *poMapFile)
{
    GInt32              nX, nY;
    TABMAPObjectBlock   *poObjBlock;
    OGRGeometry         *poGeom;
    OGRPoint            *poPoint;

    if (ValidateMapInfoType() == TAB_GEOM_NONE)
        return -1;      // Invalid Geometry... an error has already been sent

    poObjBlock = poMapFile->GetCurObjBlock();

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && poGeom->getGeometryType() == wkbPoint)
        poPoint = (OGRPoint*)poGeom;
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABCustomPoint: Missing or Invalid Geometry!");
        return -1;
    }

    poMapFile->Coordsys2Int(poPoint->getX(), poPoint->getY(), nX, nY);

    /*-----------------------------------------------------------------
     * Write object information
     *----------------------------------------------------------------*/
    poObjBlock->WriteByte(m_nUnknown_);  // ??? 
    poObjBlock->WriteByte(m_nCustomStyle);   // 0x01=Show BG,
                                         // 0x02=Apply Color
    poObjBlock->WriteIntCoord(nX, nY);

    m_nSymbolDefIndex = poMapFile->WriteSymbolDef(&m_sSymbolDef);
    poObjBlock->WriteByte(m_nSymbolDefIndex);      // Symbol index

    m_nFontDefIndex = poMapFile->WriteFontDef(&m_sFontDef);
    poObjBlock->WriteByte(m_nFontDefIndex);      // Font index

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}


/*=====================================================================
 *                      class TABPolyline
 *====================================================================*/


/**********************************************************************
 *                   TABPolyline::TABPolyline()
 *
 * Constructor.
 **********************************************************************/
TABPolyline::TABPolyline(OGRFeatureDefn *poDefnIn):
              TABFeature(poDefnIn)
{
    m_bSmooth = FALSE;
}

/**********************************************************************
 *                   TABPolyline::~TABPolyline()
 *
 * Destructor.
 **********************************************************************/
TABPolyline::~TABPolyline()
{
}

/**********************************************************************
 *                   TABPolyline::ValidateMapInfoType()
 *
 * Check the feature's geometry part and return the corresponding
 * mapinfo object type code.  The m_nMapInfoType member will also
 * be updated for further calls to GetMapInfoType();
 *
 * Returns TAB_GEOM_NONE if the geometry is not compatible with what
 * is expected for this object class.
 **********************************************************************/
int  TABPolyline::ValidateMapInfoType()
{
    OGRGeometry   *poGeom;
    OGRMultiLineString *poMultiLine = NULL;
    OGRLineString *poLine = NULL;

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
        if ( poLine->getNumPoints() > 2)
        {
            m_nMapInfoType = TAB_GEOM_PLINE;
        }
        else
        {
            m_nMapInfoType = TAB_GEOM_LINE;
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

        m_nMapInfoType = TAB_GEOM_MULTIPLINE;

        for(iLine=0; iLine < numLines; iLine++)
        {
            poGeom = poMultiLine->getGeometryRef(iLine);
            if (poGeom && poGeom->getGeometryType() != wkbLineString)
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "TABPolyline: Object contains an invalid Geometry!");
                m_nMapInfoType = TAB_GEOM_NONE;
                break;
            }
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABPolyline: Missing or Invalid Geometry!");
        m_nMapInfoType = TAB_GEOM_NONE;
    }

    return m_nMapInfoType;
}


/**********************************************************************
 *                   TABPolyline::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABPolyline::ReadGeometryFromMAPFile(TABMAPFile *poMapFile)
{
    GInt32              nX, nY;
    double              dX, dY, dXMin, dYMin, dXMax, dYMax;
    OGRGeometry         *poGeometry;
    OGRLineString       *poLine;
    TABMAPObjectBlock   *poObjBlock;
    GBool               bComprCoord;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poMapFile->GetCurObjType();

    poObjBlock = poMapFile->GetCurObjBlock();

    bComprCoord = (m_nMapInfoType == TAB_GEOM_LINE_C ||
                   m_nMapInfoType == TAB_GEOM_PLINE_C ||
                   m_nMapInfoType == TAB_GEOM_MULTIPLINE_C);

    m_bSmooth = FALSE;

    if (m_nMapInfoType == TAB_GEOM_LINE ||
        m_nMapInfoType == TAB_GEOM_LINE_C )
    {
        /*=============================================================
         * LINE (2 vertices)
         *============================================================*/
        poGeometry = poLine = new OGRLineString();
        poLine->setNumPoints(2);

        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);
        poMapFile->Int2Coordsys(nX, nY, dXMin, dYMin);
        poLine->setPoint(0, dXMin, dYMin);

        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);
        poMapFile->Int2Coordsys(nX, nY, dXMax, dYMax);
        poLine->setPoint(1, dXMax, dYMax);

        m_nPenDefIndex = poObjBlock->ReadByte();      // Pen index
        poMapFile->ReadPenDef(m_nPenDefIndex, &m_sPenDef);
    }
    else if (m_nMapInfoType == TAB_GEOM_PLINE ||
             m_nMapInfoType == TAB_GEOM_PLINE_C )
    {
        /*=============================================================
         * PLINE ( > 2 vertices)
         *============================================================*/
        int     i, numPoints, nStatus;
        GUInt32 nCoordDataSize;
        GInt32  nCoordBlockPtr, nCenterX, nCenterY;
        TABMAPCoordBlock *poCoordBlock;

        /*-------------------------------------------------------------
         * Read data from poObjBlock
         *------------------------------------------------------------*/
        nCoordBlockPtr = poObjBlock->ReadInt32();
        nCoordDataSize = poObjBlock->ReadInt32();
        if (bComprCoord)
        {
            poObjBlock->ReadInt16();    // ??? Polyline centroid ???
            poObjBlock->ReadInt16();    // Present only in compressed PLINE
        }
        nCenterX = poObjBlock->ReadInt32();
        nCenterY = poObjBlock->ReadInt32();

        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);    // Read MBR
        poMapFile->Int2Coordsys(nX, nY, dXMin, dYMin);
        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);
        poMapFile->Int2Coordsys(nX, nY, dXMax, dYMax);

        m_nPenDefIndex = poObjBlock->ReadByte();      // Pen index
        poMapFile->ReadPenDef(m_nPenDefIndex, &m_sPenDef);

        /*-------------------------------------------------------------
         * Create Geometry and read coordinates
         *------------------------------------------------------------*/
        if (nCoordDataSize & 0x80000000)
        {
            m_bSmooth = TRUE;
            nCoordDataSize &= 0x7FFFFFFF; //Take smooth flag out of the value
        }
        numPoints = nCoordDataSize/(bComprCoord?4:8);

        poCoordBlock = poMapFile->GetCoordBlock(nCoordBlockPtr);
        if (poCoordBlock == NULL)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Can't access coordinate block at offset %d", 
                     nCoordBlockPtr);
            return -1;
        }

        poCoordBlock->SetComprCoordOrigin(nCenterX, nCenterY);

        poGeometry = poLine = new OGRLineString();
        poLine->setNumPoints(numPoints);

        nStatus = 0;
        for(i=0; nStatus == 0 && i<numPoints; i++)
        {
            nStatus = poCoordBlock->ReadIntCoord(bComprCoord, nX, nY);
            if (nStatus != 0)
                break;
            poMapFile->Int2Coordsys(nX, nY, dX, dY);
            poLine->setPoint(i, dX, dY);
        }

        if (nStatus != 0)
        {
            // Failed ... error message has already been produced
            delete poGeometry;
            return nStatus;
        }   

    }
    else if (m_nMapInfoType == TAB_GEOM_MULTIPLINE ||
             m_nMapInfoType == TAB_GEOM_MULTIPLINE_C )
    {
        /*=============================================================
         * PLINE MULTIPLE
         *============================================================*/
        int     i, numPointsTotal, iSection;
        GInt32  nCoordBlockPtr, numLineSections, nCenterX, nCenterY;
        GInt32  *panXY;
        TABMAPCoordBlock        *poCoordBlock;
        OGRMultiLineString      *poMultiLine;
        TABMAPCoordSecHdr       *pasSecHdrs;

        /*-------------------------------------------------------------
         * Read data from poObjBlock
         *------------------------------------------------------------*/
        nCoordBlockPtr = poObjBlock->ReadInt32();
                         poObjBlock->ReadInt32();  // Skip Coord. data size
        numLineSections = poObjBlock->ReadInt16();

        if (bComprCoord)
        {
            poObjBlock->ReadInt16();    // ??? Polyline centroid ???
            poObjBlock->ReadInt16();    // Present only in compressed case
        }
        nCenterX = poObjBlock->ReadInt32();
        nCenterY = poObjBlock->ReadInt32();

        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);    // Read MBR
        poMapFile->Int2Coordsys(nX, nY, dXMin, dYMin);
        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);
        poMapFile->Int2Coordsys(nX, nY, dXMax, dYMax);

        m_nPenDefIndex = poObjBlock->ReadByte();      // Pen index
        poMapFile->ReadPenDef(m_nPenDefIndex, &m_sPenDef);

        /*-------------------------------------------------------------
         * Read data from the coord. block
         *------------------------------------------------------------*/
        pasSecHdrs = (TABMAPCoordSecHdr*)CPLMalloc(numLineSections*
                                                   sizeof(TABMAPCoordSecHdr));

        poCoordBlock = poMapFile->GetCoordBlock(nCoordBlockPtr);
        if (poCoordBlock == NULL ||
            poCoordBlock->ReadCoordSecHdrs(bComprCoord, numLineSections,
                                           pasSecHdrs, numPointsTotal) != 0)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed reading coordinate data at offset %d", 
                     nCoordBlockPtr);
            return -1;
        }

        poCoordBlock->SetComprCoordOrigin(nCenterX, nCenterY);

        panXY = (GInt32*)CPLMalloc(numPointsTotal*2*sizeof(GInt32));

        if (poCoordBlock->ReadIntCoords(bComprCoord,numPointsTotal,panXY) != 0)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed reading coordinate data at offset %d", 
                     nCoordBlockPtr);
            return -1;
        }

        /*-------------------------------------------------------------
         * Create a Geometry collection with one line geometry for
         * each coordinates section
         *------------------------------------------------------------*/
        poGeometry = poMultiLine = new OGRMultiLineString();

        for(iSection=0; iSection<numLineSections; iSection++)
        {
            GInt32 *pnXYPtr;
            int     numSectionVertices;

            numSectionVertices = pasSecHdrs[iSection].numVertices;
            pnXYPtr = panXY + (pasSecHdrs[iSection].nVertexOffset * 2);

            poLine = new OGRLineString();
            poLine->setNumPoints(numSectionVertices);

            for(i=0; i<numSectionVertices; i++)
            {
                poMapFile->Int2Coordsys(*pnXYPtr, *(pnXYPtr+1), dX, dY);
                poLine->setPoint(i, dX, dY);
                pnXYPtr += 2;
            }

            poMultiLine->addGeometry(poLine);
            poLine = NULL;
        }

        CPLFree(pasSecHdrs);
        CPLFree(panXY);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
           "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
                 m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

    SetGeometryDirectly(poGeometry);

    SetMBR(dXMin, dYMin, dXMax, dYMax);

    return 0;
}

/**********************************************************************
 *                   TABPolyline::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABPolyline::WriteGeometryToMAPFile(TABMAPFile *poMapFile)
{
    GInt32              nX, nY;
    TABMAPObjectBlock   *poObjBlock;
    OGRGeometry         *poGeom;
    OGRLineString       *poLine=NULL;

    if (ValidateMapInfoType() == TAB_GEOM_NONE)
        return -1;      // Invalid Geometry... an error has already been sent

    poObjBlock = poMapFile->GetCurObjBlock();

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();

    if (m_nMapInfoType == TAB_GEOM_LINE &&
        poGeom && poGeom->getGeometryType() == wkbLineString &&
        (poLine = (OGRLineString*)poGeom)->getNumPoints() == 2)
    {
        /*=============================================================
         * LINE (2 vertices)
         *============================================================*/
        poMapFile->Coordsys2Int(poLine->getX(0), poLine->getY(0), nX, nY);
        poObjBlock->WriteIntCoord(nX, nY);

        poMapFile->Coordsys2Int(poLine->getX(1), poLine->getY(1), nX, nY);
        poObjBlock->WriteIntCoord(nX, nY);

        m_nPenDefIndex = poMapFile->WritePenDef(&m_sPenDef);
        poObjBlock->WriteByte(m_nPenDefIndex);      // Pen index

    }
    else if (m_nMapInfoType == TAB_GEOM_PLINE &&
             poGeom && poGeom->getGeometryType() == wkbLineString)
    {
        /*=============================================================
         * PLINE ( > 2 vertices)
         *============================================================*/
        int     i, numPoints, nStatus;
        GUInt32 nCoordDataSize;
        GInt32  nCoordBlockPtr, nXMin, nYMin, nXMax, nYMax;
        TABMAPCoordBlock *poCoordBlock;

        /*-------------------------------------------------------------
         * Process geometry first...
         *------------------------------------------------------------*/
        poLine = (OGRLineString*)poGeom;
        numPoints = poLine->getNumPoints();

        poCoordBlock = poMapFile->GetCurCoordBlock();
        poCoordBlock->StartNewFeature();
        nCoordBlockPtr = poCoordBlock->GetCurAddress();

        nStatus = 0;
        for(i=0; nStatus == 0 && i<numPoints; i++)
        {
            poMapFile->Coordsys2Int(poLine->getX(i), poLine->getY(i), nX, nY);
            if ((nStatus = poCoordBlock->WriteIntCoord(nX, nY)) != 0)
            {
                // Failed ... error message has already been produced
                return nStatus;
            }   
        }

        nCoordDataSize = poCoordBlock->GetFeatureDataSize();

        // Combine smooth flag in the coord data size.
        if (m_bSmooth)
            nCoordDataSize |= 0x80000000;

        poCoordBlock->GetFeatureMBR(nXMin, nYMin, nXMax, nYMax);

        /*-------------------------------------------------------------
         * Write info to poObjBlock
         *------------------------------------------------------------*/
        poObjBlock->WriteInt32(nCoordBlockPtr);
        poObjBlock->WriteInt32(nCoordDataSize);

        // Polyline center
        poObjBlock->WriteIntCoord((nXMin+nXMax)/2, (nYMin+nYMax)/2);

        // MBR
        poObjBlock->WriteIntMBRCoord(nXMin, nYMin, nXMax, nYMax);

        m_nPenDefIndex = poMapFile->WritePenDef(&m_sPenDef);
        poObjBlock->WriteByte(m_nPenDefIndex);      // Pen index

    }
    else if (poGeom && poGeom->getGeometryType() == wkbMultiLineString)
    {
        /*=============================================================
         * PLINE MULTIPLE
         *============================================================*/
        int     nStatus=0, i, iLine, numPointsTotal, numPoints;
        GUInt32 nCoordDataSize;
        GInt32  nCoordBlockPtr, numLines;
        GInt32  nXMin, nYMin, nXMax, nYMax;
        TABMAPCoordBlock        *poCoordBlock;
        OGRMultiLineString      *poMultiLine;
        TABMAPCoordSecHdr       *pasSecHdrs;
        OGREnvelope             sEnvelope;

        /*-------------------------------------------------------------
         * Process geometry first...
         *------------------------------------------------------------*/
        poCoordBlock = poMapFile->GetCurCoordBlock();
        poCoordBlock->StartNewFeature();
        nCoordBlockPtr = poCoordBlock->GetCurAddress();

        poMultiLine = (OGRMultiLineString*)poGeom;
        numLines = poMultiLine->getNumGeometries();

        /*-------------------------------------------------------------
         * Build and write array of coord sections headers
         *------------------------------------------------------------*/
        pasSecHdrs = (TABMAPCoordSecHdr*)CPLCalloc(numLines,
                                                   sizeof(TABMAPCoordSecHdr));

        numPointsTotal = 0;
        for(iLine=0; iLine < numLines; iLine++)
        {
            poGeom = poMultiLine->getGeometryRef(iLine);
            if (poGeom && poGeom->getGeometryType() == wkbLineString)
            {
                poLine = (OGRLineString*)poGeom;
                numPoints = poLine->getNumPoints();
                poLine->getEnvelope(&sEnvelope);

                pasSecHdrs[iLine].numVertices = poLine->getNumPoints();
                pasSecHdrs[iLine].numHoles = 0; // It's a line!

                poMapFile->Coordsys2Int(sEnvelope.MinX, sEnvelope.MinY,
                                        pasSecHdrs[iLine].nXMin,
                                        pasSecHdrs[iLine].nYMin);
                poMapFile->Coordsys2Int(sEnvelope.MaxX, sEnvelope.MaxY,
                                        pasSecHdrs[iLine].nXMax,
                                        pasSecHdrs[iLine].nYMax);
                pasSecHdrs[iLine].nDataOffset = numLines * 24 +
                                                numPointsTotal*4*2;
                pasSecHdrs[iLine].nVertexOffset = numPointsTotal;

                numPointsTotal += numPoints;
            }
            else
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "TABPolyline: Object contains an invalid Geometry!");
                nStatus = -1;
            }

        }
         
        if (nStatus == 0)
            nStatus = poCoordBlock->WriteCoordSecHdrs(numLines, pasSecHdrs);

        CPLFree(pasSecHdrs);
        pasSecHdrs = NULL;

        if (nStatus != 0)
            return nStatus;  // Error has already been reported.

        /*-------------------------------------------------------------
         * Then write the coordinates themselves...
         *------------------------------------------------------------*/
        for(iLine=0; nStatus == 0 && iLine < numLines; iLine++)
        {
            poGeom = poMultiLine->getGeometryRef(iLine);
            if (poGeom && poGeom->getGeometryType() == wkbLineString)
            {
                poLine = (OGRLineString*)poGeom;
                numPoints = poLine->getNumPoints();

                for(i=0; nStatus == 0 && i<numPoints; i++)
                {
                    poMapFile->Coordsys2Int(poLine->getX(i), poLine->getY(i),
                                            nX, nY);
                    if ((nStatus=poCoordBlock->WriteIntCoord(nX, nY)) != 0)
                    {
                        // Failed ... error message has already been produced
                        return nStatus;
                    }   
                }
            }
            else
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "TABPolyline: Object contains an invalid Geometry!");
                return -1;
            }

        }

        nCoordDataSize = poCoordBlock->GetFeatureDataSize();

        poCoordBlock->GetFeatureMBR(nXMin, nYMin, nXMax, nYMax);

        /*-------------------------------------------------------------
         * ... and finally write info to poObjBlock
         *------------------------------------------------------------*/
        poObjBlock->WriteInt32(nCoordBlockPtr);
        poObjBlock->WriteInt32(nCoordDataSize);
        poObjBlock->WriteInt16(numLines);

        // Polyline center
        poObjBlock->WriteIntCoord((nXMin+nXMax)/2, (nYMin+nYMax)/2);

        // MBR
        poObjBlock->WriteIntMBRCoord(nXMin, nYMin, nXMax, nYMax);

        m_nPenDefIndex = poMapFile->WritePenDef(&m_sPenDef);
        poObjBlock->WriteByte(m_nPenDefIndex);      // Pen index
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABPolyline: Object contains an invalid Geometry!");
        return -1;
    }

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}


/**********************************************************************
 *                   TABPolyline::DumpMIF()
 *
 * Dump feature geometry in a format similar to .MIF PLINEs.
 **********************************************************************/
void TABPolyline::DumpMIF(FILE *fpOut /*=NULL*/)
{
    OGRGeometry   *poGeom;
    OGRMultiLineString *poMultiLine = NULL;
    OGRLineString *poLine = NULL;
    int i, numPoints;

    if (fpOut == NULL)
        fpOut = stdout;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && poGeom->getGeometryType() == wkbLineString)
    {
        /*-------------------------------------------------------------
         * Generate output for simple polyline
         *------------------------------------------------------------*/
        poLine = (OGRLineString*)poGeom;
        numPoints = poLine->getNumPoints();
        fprintf(fpOut, "PLINE %d\n", numPoints);
        for(i=0; i<numPoints; i++)
            fprintf(fpOut, "%g %g\n", poLine->getX(i), poLine->getY(i));
    }
    else if (poGeom && poGeom->getGeometryType() == wkbMultiLineString)
    {
        /*-------------------------------------------------------------
         * Generate output for multiple polyline
         *------------------------------------------------------------*/
        int iLine, numLines;
        poMultiLine = (OGRMultiLineString*)poGeom;
        numLines = poMultiLine->getNumGeometries();
        fprintf(fpOut, "PLINE MULTIPLE %d\n", numLines);
        for(iLine=0; iLine < numLines; iLine++)
        {
            poGeom = poMultiLine->getGeometryRef(iLine);
            if (poGeom && poGeom->getGeometryType() == wkbLineString)
            {
                poLine = (OGRLineString*)poGeom;
                numPoints = poLine->getNumPoints();
                fprintf(fpOut, " %d\n", numPoints);
                for(i=0; i<numPoints; i++)
                    fprintf(fpOut, "%g %g\n",poLine->getX(i),poLine->getY(i));
            }
            else
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "TABPolyline: Object contains an invalid Geometry!");
                return;
            }

        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABPolyline: Missing or Invalid Geometry!");
        return;
    }

    // Finish with PEN/BRUSH/etc. clauses
    DumpPenDef();

    fflush(fpOut);
}


/*=====================================================================
 *                      class TABRegion
 *====================================================================*/

/**********************************************************************
 *                   TABRegion::TABRegion()
 *
 * Constructor.
 **********************************************************************/
TABRegion::TABRegion(OGRFeatureDefn *poDefnIn):
              TABFeature(poDefnIn)
{
    m_bCentroid = FALSE;
    m_bSmooth = FALSE;
}

/**********************************************************************
 *                   TABRegion::~TABRegion()
 *
 * Destructor.
 **********************************************************************/
TABRegion::~TABRegion()
{
}

/**********************************************************************
 *                   TABRegion::ValidateMapInfoType()
 *
 * Check the feature's geometry part and return the corresponding
 * mapinfo object type code.  The m_nMapInfoType member will also
 * be updated for further calls to GetMapInfoType();
 *
 * Returns TAB_GEOM_NONE if the geometry is not compatible with what
 * is expected for this object class.
 **********************************************************************/
int  TABRegion::ValidateMapInfoType()
{
    OGRGeometry *poGeom;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && poGeom->getGeometryType() == wkbPolygon)
    {
        m_nMapInfoType = TAB_GEOM_REGION;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABRegion: Missing or Invalid Geometry!");
        m_nMapInfoType = TAB_GEOM_NONE;
    }

    return m_nMapInfoType;
}

/**********************************************************************
 *                   TABRegion::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABRegion::ReadGeometryFromMAPFile(TABMAPFile *poMapFile)
{
    double              dX, dY, dXMin, dYMin, dXMax, dYMax;
    OGRGeometry         *poGeometry;
    OGRLinearRing       *poRing;
    TABMAPObjectBlock   *poObjBlock;
    GBool               bComprCoord;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poMapFile->GetCurObjType();

    poObjBlock = poMapFile->GetCurObjBlock();

    bComprCoord = (m_nMapInfoType == TAB_GEOM_REGION_C);

    m_bSmooth = FALSE;

    if (m_nMapInfoType == TAB_GEOM_REGION ||
        m_nMapInfoType == TAB_GEOM_REGION_C )
    {
        /*=============================================================
         * REGION (Similar to PLINE MULTIPLE)
         *============================================================*/
        int     i, numPointsTotal, iSection;
        GInt32  nCoordBlockPtr, numLineSections, nCenterX, nCenterY;
        GInt32  *panXY, nX, nY;
        TABMAPCoordBlock        *poCoordBlock;
        OGRPolygon              *poPolygon;
        TABMAPCoordSecHdr       *pasSecHdrs;

        /*-------------------------------------------------------------
         * Read data from poObjBlock
         *------------------------------------------------------------*/
        nCoordBlockPtr = poObjBlock->ReadInt32();
                         poObjBlock->ReadInt32();  // Skip Coord. data size
        numLineSections = poObjBlock->ReadInt16();

        if (bComprCoord)
        {
            poObjBlock->ReadInt16();    // ??? Polyline centroid ???
            poObjBlock->ReadInt16();    // Present only in compressed case
        }
        nCenterX = poObjBlock->ReadInt32();
        nCenterY = poObjBlock->ReadInt32();

        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);    // Read MBR
        poMapFile->Int2Coordsys(nX, nY, dXMin, dYMin);
        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);
        poMapFile->Int2Coordsys(nX, nY, dXMax, dYMax);

        m_nPenDefIndex = poObjBlock->ReadByte();      // Pen index
        poMapFile->ReadPenDef(m_nPenDefIndex, &m_sPenDef);
        m_nBrushDefIndex = poObjBlock->ReadByte();    // Brush index
        poMapFile->ReadBrushDef(m_nBrushDefIndex, &m_sBrushDef);

        /*-------------------------------------------------------------
         * Read data from the coord. block
         *------------------------------------------------------------*/
        pasSecHdrs = (TABMAPCoordSecHdr*)CPLMalloc(numLineSections*
                                                   sizeof(TABMAPCoordSecHdr));

        poCoordBlock = poMapFile->GetCoordBlock(nCoordBlockPtr);
        if (poCoordBlock == NULL ||
            poCoordBlock->ReadCoordSecHdrs(bComprCoord, numLineSections,
                                           pasSecHdrs, numPointsTotal) != 0)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed reading coordinate data at offset %d", 
                     nCoordBlockPtr);
            return -1;
        }

        poCoordBlock->SetComprCoordOrigin(nCenterX, nCenterY);

        panXY = (GInt32*)CPLMalloc(numPointsTotal*2*sizeof(GInt32));

        if (poCoordBlock->ReadIntCoords(bComprCoord,numPointsTotal,panXY) != 0)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed reading coordinate data at offset %d", 
                     nCoordBlockPtr);
            return -1;
        }

        /*-------------------------------------------------------------
         * Create an OGRPolygon with one OGRLinearRing geometry for
         * each coordinates section.  The first ring is the outer ring.
         *------------------------------------------------------------*/
        poGeometry = poPolygon = new OGRPolygon();

        for(iSection=0; iSection<numLineSections; iSection++)
        {
            GInt32 *pnXYPtr;
            int     numSectionVertices;

            numSectionVertices = pasSecHdrs[iSection].numVertices;
            pnXYPtr = panXY + (pasSecHdrs[iSection].nVertexOffset * 2);

            poRing = new OGRLinearRing();
            poRing->setNumPoints(numSectionVertices);

            for(i=0; i<numSectionVertices; i++)
            {
                poMapFile->Int2Coordsys(*pnXYPtr, *(pnXYPtr+1), dX, dY);
                poRing->setPoint(i, dX, dY);
                pnXYPtr += 2;
            }

            poPolygon->addRing(poRing);
            poRing = NULL;
        }

        CPLFree(pasSecHdrs);
        CPLFree(panXY);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
           "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
                 m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

    SetGeometryDirectly(poGeometry);

    SetMBR(dXMin, dYMin, dXMax, dYMax);

    return 0;
}

/**********************************************************************
 *                   TABRegion::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABRegion::WriteGeometryToMAPFile(TABMAPFile *poMapFile)
{
    GInt32              nX, nY;
    TABMAPObjectBlock   *poObjBlock;
    OGRGeometry         *poGeom;
    OGRPolygon          *poPolygon=NULL;

    if (ValidateMapInfoType() == TAB_GEOM_NONE)
        return -1;      // Invalid Geometry... an error has already been sent

    poObjBlock = poMapFile->GetCurObjBlock();

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();

    if (poGeom && poGeom->getGeometryType() == wkbPolygon)
    {
        /*=============================================================
         * REGIONs are similar to PLINE MULTIPLE
         *============================================================*/
        int     nStatus=0, i, iRing, numIntRings, numPointsTotal, numPoints;
        GUInt32 nCoordDataSize;
        GInt32  nCoordBlockPtr;
        GInt32  nXMin, nYMin, nXMax, nYMax;
        TABMAPCoordBlock        *poCoordBlock;
        TABMAPCoordSecHdr       *pasSecHdrs;
        OGREnvelope             sEnvelope;

        /*-------------------------------------------------------------
         * Process geometry first...
         *------------------------------------------------------------*/
        poCoordBlock = poMapFile->GetCurCoordBlock();
        poCoordBlock->StartNewFeature();
        nCoordBlockPtr = poCoordBlock->GetCurAddress();

        poPolygon = (OGRPolygon*)poGeom;
        numIntRings = poPolygon->getNumInteriorRings();

        /*-------------------------------------------------------------
         * Build and write array of coord sections headers
         *------------------------------------------------------------*/
        pasSecHdrs = (TABMAPCoordSecHdr*)CPLCalloc(numIntRings+1,
                                                   sizeof(TABMAPCoordSecHdr));

        numPointsTotal = 0;

        // In this loop, iRing=0 for the outer ring.
        for(iRing=0; iRing <= numIntRings; iRing++)
        {
            OGRLinearRing       *poRing;

            if (iRing == 0)
                poRing = poPolygon->getExteriorRing();
            else
                poRing = poPolygon->getInteriorRing(iRing-1);

            if (poRing == NULL)
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "TABRegion: Object Geometry contains NULL rings!");
                return -1;
            }

            numPoints = poRing->getNumPoints();

            poRing->getEnvelope(&sEnvelope);

            pasSecHdrs[iRing].numVertices = poRing->getNumPoints();
            if (iRing == -1)
                pasSecHdrs[iRing].numHoles = numIntRings;
            else
                pasSecHdrs[iRing].numHoles = 0;

            poMapFile->Coordsys2Int(sEnvelope.MinX, sEnvelope.MinY,
                                        pasSecHdrs[iRing].nXMin,
                                        pasSecHdrs[iRing].nYMin);
            poMapFile->Coordsys2Int(sEnvelope.MaxX, sEnvelope.MaxY,
                                        pasSecHdrs[iRing].nXMax,
                                        pasSecHdrs[iRing].nYMax);
            pasSecHdrs[iRing].nDataOffset = (numIntRings+1) * 24 +
                                                numPointsTotal*4*2;
            pasSecHdrs[iRing].nVertexOffset = numPointsTotal;

            numPointsTotal += numPoints;
        }

        if (nStatus == 0)
            nStatus = poCoordBlock->WriteCoordSecHdrs(numIntRings+1,
                                                      pasSecHdrs);

        CPLFree(pasSecHdrs);
        pasSecHdrs = NULL;

        if (nStatus != 0)
            return nStatus;  // Error has already been reported.

        /*-------------------------------------------------------------
         * Then write the coordinates themselves...
         *------------------------------------------------------------*/
        // In this loop, iRing=0 for the outer ring.
        for(iRing=0; iRing <= numIntRings; iRing++)
        {
            OGRLinearRing       *poRing;

            if (iRing == 0)
                poRing = poPolygon->getExteriorRing();
            else
                poRing = poPolygon->getInteriorRing(iRing-1);

            numPoints = poRing->getNumPoints();

            for(i=0; nStatus == 0 && i<numPoints; i++)
            {
                poMapFile->Coordsys2Int(poRing->getX(i), poRing->getY(i),
                                        nX, nY);
                if ((nStatus=poCoordBlock->WriteIntCoord(nX, nY)) != 0)
                {
                    // Failed ... error message has already been produced
                    return nStatus;
                }   
            }
        }

        nCoordDataSize = poCoordBlock->GetFeatureDataSize();

        poCoordBlock->GetFeatureMBR(nXMin, nYMin, nXMax, nYMax);

        /*-------------------------------------------------------------
         * ... and finally write info to poObjBlock
         *------------------------------------------------------------*/
        poObjBlock->WriteInt32(nCoordBlockPtr);
        poObjBlock->WriteInt32(nCoordDataSize);
        poObjBlock->WriteInt16(numIntRings+1);

        // Polyline center
        poObjBlock->WriteIntCoord((nXMin+nXMax)/2, (nYMin+nYMax)/2);

        // MBR
        poObjBlock->WriteIntMBRCoord(nXMin, nYMin, nXMax, nYMax);

        m_nPenDefIndex = poMapFile->WritePenDef(&m_sPenDef);
        poObjBlock->WriteByte(m_nPenDefIndex);      // Pen index

        m_nBrushDefIndex = poMapFile->WriteBrushDef(&m_sBrushDef);
        poObjBlock->WriteByte(m_nBrushDefIndex);      // Brush index
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABRegion: Object contains an invalid Geometry!");
        return -1;
    }

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}


/**********************************************************************
 *                   TABRegion::DumpMIF()
 *
 * Dump feature geometry in a format similar to .MIF REGIONs.
 **********************************************************************/
void TABRegion::DumpMIF(FILE *fpOut /*=NULL*/)
{
    OGRGeometry   *poGeom;
    OGRPolygon    *poPolygon = NULL;
    int i, numPoints;

    if (fpOut == NULL)
        fpOut = stdout;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && poGeom->getGeometryType() == wkbPolygon)
    {
        /*-------------------------------------------------------------
         * Generate output for region
         *------------------------------------------------------------*/
        int iRing, numIntRings;
        poPolygon = (OGRPolygon*)poGeom;
        numIntRings = poPolygon->getNumInteriorRings();
        fprintf(fpOut, "REGION %d\n", numIntRings+1);
        // In this loop, iRing=-1 for the outer ring.
        for(iRing=-1; iRing < numIntRings; iRing++)
        {
            OGRLinearRing       *poRing;

            if (iRing == -1)
                poRing = poPolygon->getExteriorRing();
            else
                poRing = poPolygon->getInteriorRing(iRing);

            if (poRing == NULL)
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "TABRegion: Object Geometry contains NULL rings!");
                return;
            }

            numPoints = poRing->getNumPoints();
            fprintf(fpOut, " %d\n", numPoints);
            for(i=0; i<numPoints; i++)
                fprintf(fpOut, "%g %g\n",poRing->getX(i),poRing->getY(i));
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABRegion: Missing or Invalid Geometry!");
        return;
    }

    // Finish with PEN/BRUSH/etc. clauses
    DumpPenDef();
    DumpBrushDef();

    fflush(fpOut);
}

/*=====================================================================
 *                      class TABRectangle
 *====================================================================*/

/**********************************************************************
 *                   TABRectangle::TABRectangle()
 *
 * Constructor.
 **********************************************************************/
TABRectangle::TABRectangle(OGRFeatureDefn *poDefnIn):
              TABFeature(poDefnIn)
{
    m_bRoundCorners = FALSE;
    m_dRoundXRadius = m_dRoundYRadius = 0.0;
}

/**********************************************************************
 *                   TABRectangle::~TABRectangle()
 *
 * Destructor.
 **********************************************************************/
TABRectangle::~TABRectangle()
{
}

/**********************************************************************
 *                   TABRectangle::ValidateMapInfoType()
 *
 * Check the feature's geometry part and return the corresponding
 * mapinfo object type code.  The m_nMapInfoType member will also
 * be updated for further calls to GetMapInfoType();
 *
 * Returns TAB_GEOM_NONE if the geometry is not compatible with what
 * is expected for this object class.
 **********************************************************************/
int  TABRectangle::ValidateMapInfoType()
{
    OGRGeometry *poGeom;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && poGeom->getGeometryType() == wkbPolygon)
    {
        if (m_bRoundCorners && m_dRoundXRadius!=0.0 && m_dRoundYRadius!=0.0)
            m_nMapInfoType = TAB_GEOM_ROUNDRECT;
        else
            m_nMapInfoType = TAB_GEOM_RECT;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABRectangle: Missing or Invalid Geometry!");
        m_nMapInfoType = TAB_GEOM_NONE;
    }

    return m_nMapInfoType;
}


/**********************************************************************
 *                   TABRectangle::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABRectangle::ReadGeometryFromMAPFile(TABMAPFile *poMapFile)
{
    GInt32              nX, nY;
    double              dXMin, dYMin, dXMax, dYMax;
    OGRPolygon          *poPolygon;
    OGRLinearRing       *poRing;
    TABMAPObjectBlock   *poObjBlock;
    GBool               bComprCoord;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poMapFile->GetCurObjType();

    poObjBlock = poMapFile->GetCurObjBlock();

    bComprCoord = (m_nMapInfoType == TAB_GEOM_RECT_C ||
                   m_nMapInfoType == TAB_GEOM_ROUNDRECT_C );

    /*-----------------------------------------------------------------
     * Read object information
     *----------------------------------------------------------------*/
    if (m_nMapInfoType == TAB_GEOM_RECT ||
        m_nMapInfoType == TAB_GEOM_RECT_C ||
        m_nMapInfoType == TAB_GEOM_ROUNDRECT ||
        m_nMapInfoType == TAB_GEOM_ROUNDRECT_C)
    {

        // Read the corners radius

        if (m_nMapInfoType == TAB_GEOM_ROUNDRECT ||
            m_nMapInfoType == TAB_GEOM_ROUNDRECT_C)
        {
            // Read the corner's diameters
            nX = bComprCoord? poObjBlock->ReadInt16():poObjBlock->ReadInt32();
            nY = bComprCoord? poObjBlock->ReadInt16():poObjBlock->ReadInt32();
            poMapFile->Int2CoordsysDist(nX, nY, 
                                        m_dRoundXRadius, m_dRoundYRadius);
            // Divide by 2 since we store the corner's radius
            m_dRoundXRadius /= 2.0;
            m_dRoundYRadius /= 2.0;

            m_bRoundCorners = TRUE;
        }
        else
        {
            m_bRoundCorners = FALSE;
            m_dRoundXRadius = m_dRoundYRadius = 0.0;
        }

        // A rectangle is defined by its MBR

        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);
        poMapFile->Int2Coordsys(nX, nY, dXMin, dYMin);
        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);
        poMapFile->Int2Coordsys(nX, nY, dXMax, dYMax);

        m_nPenDefIndex = poObjBlock->ReadByte();      // Pen index
        poMapFile->ReadPenDef(m_nPenDefIndex, &m_sPenDef);
        m_nBrushDefIndex = poObjBlock->ReadByte();    // Brush index
        poMapFile->ReadBrushDef(m_nBrushDefIndex, &m_sBrushDef);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
           "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
                 m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

    /*-----------------------------------------------------------------
     * Call SetMBR() and GetMBR() now to make sure that min values are
     * really smaller than max values.
     *----------------------------------------------------------------*/
    SetMBR(dXMin, dYMin, dXMax, dYMax);
    GetMBR(dXMin, dYMin, dXMax, dYMax);

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


    return 0;
}

/**********************************************************************
 *                   TABRectangle::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABRectangle::WriteGeometryToMAPFile(TABMAPFile *poMapFile)
{
    TABMAPObjectBlock   *poObjBlock;
    OGRGeometry         *poGeom;
    OGRPolygon          *poPolygon;
    OGREnvelope         sEnvelope;

    if (ValidateMapInfoType() == TAB_GEOM_NONE)
        return -1;      // Invalid Geometry... an error has already been sent

    poObjBlock = poMapFile->GetCurObjBlock();

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

    /*-----------------------------------------------------------------
     * Write object information
     *----------------------------------------------------------------*/
    if (m_nMapInfoType == TAB_GEOM_ROUNDRECT)
    {
        GInt32  nX, nY;
        poMapFile->Coordsys2IntDist(m_dRoundXRadius*2.0, m_dRoundYRadius*2.0,
                                    nX, nY);
        poObjBlock->WriteInt32(nX);     // Oval width
        poObjBlock->WriteInt32(nY);     // Oval height
    }

    // A rectangle is defined by its MBR
    GInt32  nXMin, nYMin, nXMax, nYMax;
    poMapFile->Coordsys2Int(sEnvelope.MinX, sEnvelope.MinY, nXMin, nYMin);
    poMapFile->Coordsys2Int(sEnvelope.MaxX, sEnvelope.MaxY, nXMax, nYMax);
    poObjBlock->WriteIntMBRCoord(nXMin, nYMin, nXMax, nYMax);

    m_nPenDefIndex = poMapFile->WritePenDef(&m_sPenDef);
    poObjBlock->WriteByte(m_nPenDefIndex);      // Pen index

    m_nBrushDefIndex = poMapFile->WriteBrushDef(&m_sBrushDef);
    poObjBlock->WriteByte(m_nBrushDefIndex);      // Brush index

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}


/**********************************************************************
 *                   TABRectangle::DumpMIF()
 *
 * Dump feature geometry in a format similar to .MIF REGIONs.
 **********************************************************************/
void TABRectangle::DumpMIF(FILE *fpOut /*=NULL*/)
{
    OGRGeometry   *poGeom;
    OGRPolygon    *poPolygon = NULL;
    int i, numPoints;

    if (fpOut == NULL)
        fpOut = stdout;

    /*-----------------------------------------------------------------
     * Output RECT or ROUNDRECT parameters
     *----------------------------------------------------------------*/
    double dXMin, dYMin, dXMax, dYMax;
    GetMBR(dXMin, dYMin, dXMax, dYMax);
    if (m_bRoundCorners)
        fprintf(fpOut, "(ROUNDRECT %g %g %g %g    %g %g)\n", 
                dXMin, dYMin, dXMax, dYMax, 
                m_dRoundXRadius, m_dRoundYRadius);
    else
        fprintf(fpOut, "(RECT %g %g %g %g)\n", dXMin, dYMin, dXMax, dYMax);

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && poGeom->getGeometryType() == wkbPolygon)
    {
        /*-------------------------------------------------------------
         * Generate rectangle output as a region
         * We could also output as a RECT or ROUNDRECT in a real MIF generator
         *------------------------------------------------------------*/
        int iRing, numIntRings;
        poPolygon = (OGRPolygon*)poGeom;
        numIntRings = poPolygon->getNumInteriorRings();
        fprintf(fpOut, "REGION %d\n", numIntRings+1);
        // In this loop, iRing=-1 for the outer ring.
        for(iRing=-1; iRing < numIntRings; iRing++)
        {
            OGRLinearRing       *poRing;

            if (iRing == -1)
                poRing = poPolygon->getExteriorRing();
            else
                poRing = poPolygon->getInteriorRing(iRing);

            if (poRing == NULL)
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "TABRectangle: Object Geometry contains NULL rings!");
                return;
            }

            numPoints = poRing->getNumPoints();
            fprintf(fpOut, " %d\n", numPoints);
            for(i=0; i<numPoints; i++)
                fprintf(fpOut, "%g %g\n",poRing->getX(i),poRing->getY(i));
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABRectangle: Missing or Invalid Geometry!");
        return;
    }

    // Finish with PEN/BRUSH/etc. clauses
    DumpPenDef();
    DumpBrushDef();

    fflush(fpOut);
}


/*=====================================================================
 *                      class TABEllipse
 *====================================================================*/

/**********************************************************************
 *                   TABEllipse::TABEllipse()
 *
 * Constructor.
 **********************************************************************/
TABEllipse::TABEllipse(OGRFeatureDefn *poDefnIn):
              TABFeature(poDefnIn)
{
}

/**********************************************************************
 *                   TABEllipse::~TABEllipse()
 *
 * Destructor.
 **********************************************************************/
TABEllipse::~TABEllipse()
{
}

/**********************************************************************
 *                   TABEllipse::ValidateMapInfoType()
 *
 * Check the feature's geometry part and return the corresponding
 * mapinfo object type code.  The m_nMapInfoType member will also
 * be updated for further calls to GetMapInfoType();
 *
 * Returns TAB_GEOM_NONE if the geometry is not compatible with what
 * is expected for this object class.
 **********************************************************************/
int  TABEllipse::ValidateMapInfoType()
{
    OGRGeometry *poGeom;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if ( (poGeom && poGeom->getGeometryType() == wkbPolygon ) ||
         (poGeom && poGeom->getGeometryType() == wkbPoint ) )
    {
        m_nMapInfoType = TAB_GEOM_ELLIPSE;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABEllipse: Missing or Invalid Geometry!");
        m_nMapInfoType = TAB_GEOM_NONE;
    }

    return m_nMapInfoType;
}

/**********************************************************************
 *                   TABEllipse::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABEllipse::ReadGeometryFromMAPFile(TABMAPFile *poMapFile)
{
    GInt32              nX, nY;
    double              dXMin, dYMin, dXMax, dYMax;
    OGRPolygon          *poPolygon;
    OGRLinearRing       *poRing;
    TABMAPObjectBlock   *poObjBlock;
    GBool               bComprCoord;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poMapFile->GetCurObjType();

    poObjBlock = poMapFile->GetCurObjBlock();

    bComprCoord = (m_nMapInfoType == TAB_GEOM_ELLIPSE_C );

    /*-----------------------------------------------------------------
     * Read object information
     *----------------------------------------------------------------*/
    if (m_nMapInfoType == TAB_GEOM_ELLIPSE ||
        m_nMapInfoType == TAB_GEOM_ELLIPSE_C )
    {

        // An ellipse is defined by its MBR

        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);
        poMapFile->Int2Coordsys(nX, nY, dXMin, dYMin);
        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);
        poMapFile->Int2Coordsys(nX, nY, dXMax, dYMax);

        m_nPenDefIndex = poObjBlock->ReadByte();      // Pen index
        poMapFile->ReadPenDef(m_nPenDefIndex, &m_sPenDef);
        m_nBrushDefIndex = poObjBlock->ReadByte();    // Brush index
        poMapFile->ReadBrushDef(m_nBrushDefIndex, &m_sBrushDef);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
           "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
                 m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

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

    return 0;
}

/**********************************************************************
 *                   TABEllipse::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABEllipse::WriteGeometryToMAPFile(TABMAPFile *poMapFile)
{
    TABMAPObjectBlock   *poObjBlock;
    OGRGeometry         *poGeom;
    OGREnvelope         sEnvelope;

    if (ValidateMapInfoType() == TAB_GEOM_NONE)
        return -1;      // Invalid Geometry... an error has already been sent

    poObjBlock = poMapFile->GetCurObjBlock();

    /*-----------------------------------------------------------------
     * Fetch and validate geometry... Polygon and point are accepted.
     * Note that we will simply use the ellipse's MBR and don't really 
     * read the polygon geometry... this should be OK unless the 
     * polygon geometry was not really an ellipse.
     *----------------------------------------------------------------*/
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

    /*-----------------------------------------------------------------
     * Write object information
     *
     * We use the center of the MBR as the ellipse center, and the 
     * X/Y radius to define the MBR size.  If X/Y radius are null then
     * we'll try to use the MBR to recompute them.
     *----------------------------------------------------------------*/
    // An ellipse is defined by its MBR
    GInt32      nXMin, nYMin, nXMax, nYMax;
    double      dXCenter, dYCenter;
    dXCenter = (sEnvelope.MaxX + sEnvelope.MinX)/2.0;
    dYCenter = (sEnvelope.MaxY + sEnvelope.MinY)/2.0;
    if (m_dXRadius == 0.0 && m_dYRadius == 0.0)
    {
        m_dXRadius = ABS(sEnvelope.MaxX - sEnvelope.MinX) / 2.0;
        m_dYRadius = ABS(sEnvelope.MaxY - sEnvelope.MinY);
    }

    poMapFile->Coordsys2Int(dXCenter - m_dXRadius, dYCenter - m_dYRadius,
                            nXMin, nYMin);
    poMapFile->Coordsys2Int(dXCenter + m_dXRadius, dYCenter + m_dYRadius,
                            nXMax, nYMax);
    poObjBlock->WriteIntMBRCoord(nXMin, nYMin, nXMax, nYMax);

    m_nPenDefIndex = poMapFile->WritePenDef(&m_sPenDef);
    poObjBlock->WriteByte(m_nPenDefIndex);      // Pen index

    m_nBrushDefIndex = poMapFile->WriteBrushDef(&m_sBrushDef);
    poObjBlock->WriteByte(m_nBrushDefIndex);      // Brush index

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}


/**********************************************************************
 *                   TABEllipse::DumpMIF()
 *
 * Dump feature geometry in a format similar to .MIF REGIONs.
 **********************************************************************/
void TABEllipse::DumpMIF(FILE *fpOut /*=NULL*/)
{
    OGRGeometry   *poGeom;
    OGRPolygon    *poPolygon = NULL;
    int i, numPoints;

    if (fpOut == NULL)
        fpOut = stdout;

    /*-----------------------------------------------------------------
     * Output ELLIPSE parameters
     *----------------------------------------------------------------*/
    double dXMin, dYMin, dXMax, dYMax;
    GetMBR(dXMin, dYMin, dXMax, dYMax);
    fprintf(fpOut, "(ELLIPSE %g %g %g %g)\n", dXMin, dYMin, dXMax, dYMax);

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && poGeom->getGeometryType() == wkbPolygon)
    {
        /*-------------------------------------------------------------
         * Generate ellipse output as a region
         * We could also output as an ELLIPSE in a real MIF generator
         *------------------------------------------------------------*/
        int iRing, numIntRings;
        poPolygon = (OGRPolygon*)poGeom;
        numIntRings = poPolygon->getNumInteriorRings();
        fprintf(fpOut, "REGION %d\n", numIntRings+1);
        // In this loop, iRing=-1 for the outer ring.
        for(iRing=-1; iRing < numIntRings; iRing++)
        {
            OGRLinearRing       *poRing;

            if (iRing == -1)
                poRing = poPolygon->getExteriorRing();
            else
                poRing = poPolygon->getInteriorRing(iRing);

            if (poRing == NULL)
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "TABEllipse: Object Geometry contains NULL rings!");
                return;
            }

            numPoints = poRing->getNumPoints();
            fprintf(fpOut, " %d\n", numPoints);
            for(i=0; i<numPoints; i++)
                fprintf(fpOut, "%g %g\n",poRing->getX(i),poRing->getY(i));
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABEllipse: Missing or Invalid Geometry!");
        return;
    }

    // Finish with PEN/BRUSH/etc. clauses
    DumpPenDef();
    DumpBrushDef();

    fflush(fpOut);
}


/*=====================================================================
 *                      class TABArc
 *====================================================================*/

/**********************************************************************
 *                   TABArc::TABArc()
 *
 * Constructor.
 **********************************************************************/
TABArc::TABArc(OGRFeatureDefn *poDefnIn):
              TABFeature(poDefnIn)
{
    m_dStartAngle = m_dEndAngle = 0.0;
    m_dCenterX = m_dCenterY = m_dXRadius = m_dYRadius = 0.0;

}

/**********************************************************************
 *                   TABArc::~TABArc()
 *
 * Destructor.
 **********************************************************************/
TABArc::~TABArc()
{
}

/**********************************************************************
 *                   TABArc::ValidateMapInfoType()
 *
 * Check the feature's geometry part and return the corresponding
 * mapinfo object type code.  The m_nMapInfoType member will also
 * be updated for further calls to GetMapInfoType();
 *
 * Returns TAB_GEOM_NONE if the geometry is not compatible with what
 * is expected for this object class.
 **********************************************************************/
int  TABArc::ValidateMapInfoType()
{
    OGRGeometry *poGeom;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if ( (poGeom && poGeom->getGeometryType() == wkbLineString ) ||
         (poGeom && poGeom->getGeometryType() == wkbPoint ) )
    {
        m_nMapInfoType = TAB_GEOM_ARC;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABArc: Missing or Invalid Geometry!");
        m_nMapInfoType = TAB_GEOM_NONE;
    }

    return m_nMapInfoType;
}

/**********************************************************************
 *                   TABArc::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABArc::ReadGeometryFromMAPFile(TABMAPFile *poMapFile)
{
    GInt32              nX, nY;
    double              dXMin, dYMin, dXMax, dYMax;
    OGRLineString       *poLine;
    TABMAPObjectBlock   *poObjBlock;
    GBool               bComprCoord;
    int                 numPts;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poMapFile->GetCurObjType();

    poObjBlock = poMapFile->GetCurObjBlock();

    bComprCoord = (m_nMapInfoType == TAB_GEOM_ARC_C );

    /*-----------------------------------------------------------------
     * Read object information
     *----------------------------------------------------------------*/
    if (m_nMapInfoType == TAB_GEOM_ARC ||
        m_nMapInfoType == TAB_GEOM_ARC_C )
    {
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

        /*-------------------------------------------------------------
         * OK, Arc angles again!!!!!!!!!!!!
         * After further tests, it appears that the angle values ALWAYS
         * have to be flipped, no matter which quadrant the file is in.
         * This does not make any sense, so I suspect that there is something
         * that we are missing here!
         *------------------------------------------------------------*/

        if (TRUE
            /* poMapFile->GetHeaderBlock()->m_nCoordOriginQuadrant==2 ||
               poMapFile->GetHeaderBlock()->m_nCoordOriginQuadrant==3 */ )
        {
            // X axis direction is flipped... adjust angle
            m_dEndAngle = poObjBlock->ReadInt16()/10.0;
            m_dStartAngle = poObjBlock->ReadInt16()/10.0;

            m_dStartAngle = (m_dStartAngle<=180.0) ? (180.0-m_dStartAngle):
                                                 (540.0-m_dStartAngle);
            m_dEndAngle   = (m_dEndAngle<=180.0) ? (180.0-m_dEndAngle):
                                               (540.0-m_dEndAngle);
        }
        else
        {
            m_dStartAngle = poObjBlock->ReadInt16()/10.0;
            m_dEndAngle = poObjBlock->ReadInt16()/10.0;
        }

        if (poMapFile->GetHeaderBlock()->m_nCoordOriginQuadrant==3 ||
            poMapFile->GetHeaderBlock()->m_nCoordOriginQuadrant==4)
        {
            // Y axis direction is flipped... this reverses angle direction
            // Unfortunately we never found any file that contains this case,
            // but this should be the behavior to expect!!!
            m_dStartAngle = 360.0 - m_dStartAngle;
            m_dEndAngle = 360.0 - m_dEndAngle;
        }

        // An arc is defined by its defining ellipse's MBR:

        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);
        poMapFile->Int2Coordsys(nX, nY, dXMin, dYMin);
        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);
        poMapFile->Int2Coordsys(nX, nY, dXMax, dYMax);

        m_dCenterX = (dXMin + dXMax) / 2.0;
        m_dCenterY = (dYMin + dYMax) / 2.0;
        m_dXRadius = ABS( (dXMax - dXMin) / 2.0 );
        m_dYRadius = ABS( (dYMax - dYMin) / 2.0 );

        // Read the Arc's MBR and use that as this feature's MBR

        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);
        poMapFile->Int2Coordsys(nX, nY, dXMin, dYMin);
        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);
        poMapFile->Int2Coordsys(nX, nY, dXMax, dYMax);

        SetMBR(dXMin, dYMin, dXMax, dYMax);

        m_nPenDefIndex = poObjBlock->ReadByte();      // Pen index
        poMapFile->ReadPenDef(m_nPenDefIndex, &m_sPenDef);


    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
           "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
                 m_nMapInfoType, m_nMapInfoType);
        return -1;
    }

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

    SetGeometryDirectly(poLine);

    return 0;
}

/**********************************************************************
 *                   TABArc::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABArc::WriteGeometryToMAPFile(TABMAPFile *poMapFile)
{
    GInt32              nXMin, nYMin, nXMax, nYMax;
    TABMAPObjectBlock   *poObjBlock;
    OGRGeometry         *poGeom;
    OGREnvelope         sEnvelope;

    if (ValidateMapInfoType() == TAB_GEOM_NONE)
        return -1;      // Invalid Geometry... an error has already been sent

    poObjBlock = poMapFile->GetCurObjBlock();

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if ( (poGeom && poGeom->getGeometryType() == wkbLineString ) )
    {
        /*-------------------------------------------------------------
         * POLYGON geometry:
         * Note that we will simply use the ellipse's MBR and don't really 
         * read the polygon geometry... this should be OK unless the 
         * polygon geometry was not really an ellipse.
         * In the case of a polygon geometry. the m_dCenterX/Y values MUST
         * have been set by the caller.
         *------------------------------------------------------------*/
        poGeom->getEnvelope(&sEnvelope);
    }
    else if ( (poGeom && poGeom->getGeometryType() == wkbPoint ) ) 
    {
        /*-------------------------------------------------------------
         * In the case of a POINT GEOMETRY, we will make sure the the 
         * feature's m_dCenterX/Y are in sync with the point's X,Y coords.
         *
         * In this case we have to reconstruct the arc inside a temporary
         * geometry object in order to find its real MBR.
         *------------------------------------------------------------*/
        OGRPoint *poPoint = (OGRPoint *)poGeom;
        m_dCenterX = poPoint->getX();
        m_dCenterY = poPoint->getY();

        OGRLineString oTmpLine;
        int numPts=0;
        if (m_dEndAngle < m_dStartAngle)
            numPts = (int) ABS( ((m_dEndAngle+360)-m_dStartAngle)/2 ) + 1;
        else
            numPts = (int) ABS( (m_dEndAngle-m_dStartAngle)/2 ) + 1;
        numPts = MAX(2, numPts);

        TABGenerateArc(&oTmpLine, numPts,
                       m_dCenterX, m_dCenterY,
                       m_dXRadius, m_dYRadius,
                       m_dStartAngle*PI/180.0, m_dEndAngle*PI/180.0);

        oTmpLine.getEnvelope(&sEnvelope);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABArc: Missing or Invalid Geometry!");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Write object information
     *----------------------------------------------------------------*/

    /*-------------------------------------------------------------
     * Start/End angles
     * Since we ALWAYS produce files in quadrant 1 then we can
     * ignore the special angle conversion required by flipped axis.
     *------------------------------------------------------------*/
    CPLAssert(poMapFile->GetHeaderBlock()->m_nCoordOriginQuadrant == 1);

    if (TRUE)
    {
        /*-------------------------------------------------------------
         * OK, Arc angles again!!!!!!!!!!!!
         * After further tests, it appears that the angle values ALWAYS
         * have to be flipped, no matter which quadrant the file is in.
         * This does not make any sense, so I suspect that there is something
         * that we are missing here!
         *------------------------------------------------------------*/
        double dAdjustedStartAngle, dAdjustedEndAngle;

        dAdjustedStartAngle = (m_dStartAngle<=180.0) ? (180.0-m_dStartAngle):
                                               (540.0-m_dStartAngle);
        dAdjustedEndAngle   = (m_dEndAngle<=180.0) ? (180.0-m_dEndAngle):
                                               (540.0-m_dEndAngle);
        poObjBlock->WriteInt16(ROUND_INT(dAdjustedEndAngle*10.0));
        poObjBlock->WriteInt16(ROUND_INT(dAdjustedStartAngle*10.0));
    }
    else
    {
        /* This is what we should logically do... but looks like MapInfo
         * does not like arc angles written this way...
         */
        poObjBlock->WriteInt16(ROUND_INT(m_dStartAngle*10.0));
        poObjBlock->WriteInt16(ROUND_INT(m_dEndAngle*10.0));
    }

    // An arc is defined by its defining ellipse's MBR:
    poMapFile->Coordsys2Int(m_dCenterX-m_dXRadius, m_dCenterY-m_dYRadius,
                            nXMin, nYMin);
    poMapFile->Coordsys2Int(m_dCenterX+m_dXRadius, m_dCenterY+m_dYRadius,
                            nXMax, nYMax);
    poObjBlock->WriteIntMBRCoord(nXMin, nYMin, nXMax, nYMax);

    // Write the Arc's actual MBR
    poMapFile->Coordsys2Int(sEnvelope.MinX, sEnvelope.MinY, nXMin, nYMin);
    poMapFile->Coordsys2Int(sEnvelope.MaxX, sEnvelope.MaxY, nXMax, nYMax);
    poObjBlock->WriteIntMBRCoord(nXMin, nYMin, nXMax, nYMax);

    m_nPenDefIndex = poMapFile->WritePenDef(&m_sPenDef);
    poObjBlock->WriteByte(m_nPenDefIndex);      // Pen index

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABArc::SetStart/EndAngle()
 *
 * Set the start/end angle values in degrees, making sure the values are
 * always in the range [0..360]
 **********************************************************************/
void TABArc::SetStartAngle(double dAngle)
{
    while(dAngle < 0.0)   dAngle += 360.0;
    while(dAngle > 360.0) dAngle -= 360.0;

    m_dStartAngle = dAngle;
}

void TABArc::SetEndAngle(double dAngle)
{
    while(dAngle < 0.0)   dAngle += 360.0;
    while(dAngle > 360.0) dAngle -= 360.0;

    m_dEndAngle = dAngle;
}

/**********************************************************************
 *                   TABArc::DumpMIF()
 *
 * Dump feature geometry in a format similar to .MIF REGIONs.
 **********************************************************************/
void TABArc::DumpMIF(FILE *fpOut /*=NULL*/)
{
    OGRGeometry   *poGeom;
    OGRLineString *poLine = NULL;
    int i, numPoints;

    if (fpOut == NULL)
        fpOut = stdout;

    /*-----------------------------------------------------------------
     * Output ARC parameters
     *----------------------------------------------------------------*/
    fprintf(fpOut, "(ARC %g %g %g %g   %d %d)\n",
            m_dCenterX - m_dXRadius, m_dCenterY - m_dYRadius,
            m_dCenterX + m_dXRadius, m_dCenterY + m_dYRadius,
            (int)m_dStartAngle, (int)m_dEndAngle);

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && poGeom->getGeometryType() == wkbLineString)
    {
        /*-------------------------------------------------------------
         * Generate arc output as a simple polyline
         * We could also output as an ELLIPSE in a real MIF generator
         *------------------------------------------------------------*/
        poLine = (OGRLineString*)poGeom;
        numPoints = poLine->getNumPoints();
        fprintf(fpOut, "PLINE %d\n", numPoints);
        for(i=0; i<numPoints; i++)
            fprintf(fpOut, "%g %g\n", poLine->getX(i), poLine->getY(i));
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABArc: Missing or Invalid Geometry!");
        return;
    }

    // Finish with PEN/BRUSH/etc. clauses
    DumpPenDef();

    fflush(fpOut);
}



/*=====================================================================
 *                      class TABText
 *====================================================================*/

/**********************************************************************
 *                   TABText::TABText()
 *
 * Constructor.
 **********************************************************************/
TABText::TABText(OGRFeatureDefn *poDefnIn):
              TABFeature(poDefnIn)
{
    m_pszString = NULL;

    m_dAngle = m_dHeight = 0.0;

    m_rgbForeground = 0x000000;
    m_rgbBackground = 0xffffff;

    m_nTextAlignment = 0;
    m_nFontStyle = 0;
}

/**********************************************************************
 *                   TABText::~TABText()
 *
 * Destructor.
 **********************************************************************/
TABText::~TABText()
{
    CPLFree(m_pszString);
}

/**********************************************************************
 *                   TABText::ValidateMapInfoType()
 *
 * Check the feature's geometry part and return the corresponding
 * mapinfo object type code.  The m_nMapInfoType member will also
 * be updated for further calls to GetMapInfoType();
 *
 * Returns TAB_GEOM_NONE if the geometry is not compatible with what
 * is expected for this object class.
 **********************************************************************/
int  TABText::ValidateMapInfoType()
{
    OGRGeometry *poGeom;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && poGeom->getGeometryType() == wkbPoint)
    {
        m_nMapInfoType = TAB_GEOM_TEXT;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABText: Missing or Invalid Geometry!");
        m_nMapInfoType = TAB_GEOM_NONE;
    }

    return m_nMapInfoType;
}

/**********************************************************************
 *                   TABText::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABText::ReadGeometryFromMAPFile(TABMAPFile *poMapFile)
{
    double              dXMin, dYMin, dXMax, dYMax;
    OGRGeometry         *poGeometry;
    TABMAPObjectBlock   *poObjBlock;
    GBool               bComprCoord;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poMapFile->GetCurObjType();

    poObjBlock = poMapFile->GetCurObjBlock();

    bComprCoord = (m_nMapInfoType == TAB_GEOM_TEXT_C);

    if (m_nMapInfoType == TAB_GEOM_TEXT ||
        m_nMapInfoType == TAB_GEOM_TEXT_C )
    {
        /*=============================================================
         * TEXT
         *============================================================*/
        int     nStringLen;
        GInt32  nCoordBlockPtr;
        GInt32  nX, nY;
        double  dJunk;
        TABMAPCoordBlock        *poCoordBlock;

        /*-------------------------------------------------------------
         * Read data from poObjBlock
         *------------------------------------------------------------*/
        nCoordBlockPtr = poObjBlock->ReadInt32();  // String position
        nStringLen     = poObjBlock->ReadInt16();  // String length
        m_nTextAlignment = poObjBlock->ReadInt16();  // just./spacing/arrow

        /*-------------------------------------------------------------
         * Text Angle, in thenths of degree.
         * Contrary to arc start/end angles, no conversion based on 
         * origin quadrant is required here
         *------------------------------------------------------------*/
        m_dAngle       = poObjBlock->ReadInt16()/10.0;

        m_nFontStyle = poObjBlock->ReadInt16();          // Font style

        m_rgbForeground = poObjBlock->ReadByte()*256*256 +
                          poObjBlock->ReadByte()*256 +
                          poObjBlock->ReadByte();
        m_rgbBackground = poObjBlock->ReadByte()*256*256 +
                          poObjBlock->ReadByte()*256 +
                          poObjBlock->ReadByte();

        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);    // arrow endpoint

        // Text Height
        nY = bComprCoord? poObjBlock->ReadInt16():poObjBlock->ReadInt32();
        poMapFile->Int2CoordsysDist(0, nY, dJunk, m_dHeight);

        m_nFontDefIndex = poObjBlock->ReadByte();      // Font name index
        poMapFile->ReadFontDef(m_nFontDefIndex, &m_sFontDef);

        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);    // Read MBR
        poMapFile->Int2Coordsys(nX, nY, dXMin, dYMin);
        poObjBlock->ReadIntCoord(bComprCoord, nX, nY);
        poMapFile->Int2Coordsys(nX, nY, dXMax, dYMax);

        m_nPenDefIndex = poObjBlock->ReadByte();      // Pen index for line
        poMapFile->ReadPenDef(m_nPenDefIndex, &m_sPenDef);

        /*-------------------------------------------------------------
         * Read text string from the coord. block
         *------------------------------------------------------------*/
        CPLFree(m_pszString);
        m_pszString = (char*)CPLMalloc((nStringLen+1)*sizeof(char));
        poCoordBlock = poMapFile->GetCoordBlock(nCoordBlockPtr);

        if (nStringLen > 0 && 
            (poCoordBlock == NULL ||
             poCoordBlock->ReadBytes(nStringLen, (GByte*)m_pszString) != 0))
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed reading text string at offset %d", 
                     nCoordBlockPtr);
            return -1;
        }

        m_pszString[nStringLen] = '\0';

    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
           "ReadGeometryFromMAPFile(): unsupported geometry type %d (0x%2.2x)",
                 m_nMapInfoType, m_nMapInfoType);
        return -1;
    }
    
    /* Set/retrieve the MBR to make sure Mins are smaller than Maxs
     */
    SetMBR(dXMin, dYMin, dXMax, dYMax);
    GetMBR(dXMin, dYMin, dXMax, dYMax);

    /*-----------------------------------------------------------------
     * Create an OGRPoint Geometry... 
     * The point X,Y values will be the coords of the lower-left corner before
     * rotation is applied.  (Note that the rotation in MapInfo is done around
     * the upper-left corner)
     * We need to calculate the true lower left corner of the text based
     * on the MBR after rotation, the text height and the rotation angle.
     *----------------------------------------------------------------*/
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
     *----------------------------------------------------------------*/
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
 *                   TABText::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABText::WriteGeometryToMAPFile(TABMAPFile *poMapFile)
{
    GInt32              nX, nY, nXMin, nYMin, nXMax, nYMax;
    TABMAPObjectBlock   *poObjBlock;
    OGRGeometry         *poGeom;
    OGRPoint            *poPoint;
    GInt32              nCoordBlockPtr;
    TABMAPCoordBlock    *poCoordBlock;
    int                 nStringLen;
  
    if (ValidateMapInfoType() == TAB_GEOM_NONE)
        return -1;      // Invalid Geometry... an error has already been sent

    poObjBlock = poMapFile->GetCurObjBlock();

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && poGeom->getGeometryType() == wkbPoint)
        poPoint = (OGRPoint*)poGeom;
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABText: Missing or Invalid Geometry!");
        return -1;
    }

    poMapFile->Coordsys2Int(poPoint->getX(), poPoint->getY(), nX, nY);

    /*-----------------------------------------------------------------
     * Write string to a coord block first...
     *----------------------------------------------------------------*/
    poCoordBlock = poMapFile->GetCurCoordBlock();
    poCoordBlock->StartNewFeature();
    nCoordBlockPtr = poCoordBlock->GetCurAddress();

    nStringLen = strlen(m_pszString);

    if (nStringLen > 0)
    {
        poCoordBlock->WriteBytes(nStringLen, (GByte *)m_pszString);
    }
    else
    {
        nCoordBlockPtr = 0;
    }

    /*-----------------------------------------------------------------
     * Write object information
     *----------------------------------------------------------------*/
    poObjBlock->WriteInt32(nCoordBlockPtr);     // String position
    poObjBlock->WriteInt16(nStringLen);         // String length
    poObjBlock->WriteInt16(m_nTextAlignment);   // just./spacing/arrow


    /*-----------------------------------------------------------------
     * Text Angle, (written in thenths of degrees)
     * Contrary to arc start/end angles, no conversion based on 
     * origin quadrant is required here
     *----------------------------------------------------------------*/
    poObjBlock->WriteInt16(ROUND_INT(m_dAngle*10.0));

    poObjBlock->WriteInt16(m_nFontStyle);          // Font style/effect

    poObjBlock->WriteByte( COLOR_R(m_rgbForeground) );
    poObjBlock->WriteByte( COLOR_G(m_rgbForeground) );
    poObjBlock->WriteByte( COLOR_B(m_rgbForeground) );

    poObjBlock->WriteByte( COLOR_R(m_rgbBackground) );
    poObjBlock->WriteByte( COLOR_G(m_rgbBackground) );
    poObjBlock->WriteByte( COLOR_B(m_rgbBackground) );

    /*-----------------------------------------------------------------
     * The OGRPoint's X,Y values were the coords of the lower-left corner
     * before rotation was applied.  (Note that the rotation in MapInfo is
     * done around the upper-left corner)
     * The Feature's MBR is the MBR of the text after rotation... that's
     * what MapInfo uses to define the text location.
     *----------------------------------------------------------------*/
    double dXMin, dYMin, dXMax, dYMax;
    // Make sure Feature MBR is in sync with other params
 
    UpdateTextMBR();
    GetMBR(dXMin, dYMin, dXMax, dYMax);

    poMapFile->Coordsys2Int(dXMin, dYMin, nXMin, nYMin);
    poMapFile->Coordsys2Int(dXMax, dYMax, nXMax, nYMax);

    // Line/arrow endpoint... default to bounding box center
    poObjBlock->WriteIntCoord((nXMin+nXMax)/2, (nYMin+nYMax)/2);

        // Text Height
    poMapFile->Coordsys2IntDist(0.0, m_dHeight, nX, nY);
    poObjBlock->WriteInt32(nY);

    // Font name
    m_nFontDefIndex = poMapFile->WriteFontDef(&m_sFontDef);
    poObjBlock->WriteByte(m_nFontDefIndex);      // Font name index

    // MBR after rotation
    poObjBlock->WriteIntMBRCoord(nXMin, nYMin, nXMax, nYMax);

    m_nPenDefIndex = poMapFile->WritePenDef(&m_sPenDef);
    poObjBlock->WriteByte(m_nPenDefIndex);      // Pen index for line/arrow


    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}


/**********************************************************************
 *                   TABText::GetTextString()
 *
 * Return ref to text string value.
 *
 * Returned string is a reference to the internal string buffer and should
 * not be modified or freed by the caller.
 **********************************************************************/
const char *TABText::GetTextString()
{
    if (m_pszString == NULL)
        return "";

    return m_pszString;
}

/**********************************************************************
 *                   TABText::SetTextString()
 *
 * Set new text string value.
 **********************************************************************/
void TABText::SetTextString(const char *pszNewStr)
{
    CPLFree(m_pszString);
    m_pszString = CPLStrdup(pszNewStr);
}

/**********************************************************************
 *                   TABText::GetTextAngle()
 *
 * Return text angle in degrees.
 **********************************************************************/
double TABText::GetTextAngle()
{
    return m_dAngle;
}

void TABText::SetTextAngle(double dAngle)
{
    // Make sure angle is in the range [0..360]
    while(dAngle < 0.0)   dAngle += 360.0;
    while(dAngle > 360.0) dAngle -= 360.0;
    m_dAngle = dAngle;
    UpdateTextMBR();
}

/**********************************************************************
 *                   TABText::GetTextBoxHeight()
 *
 * Return text height in Y axis coord. units of the text box before rotation.
 **********************************************************************/
double TABText::GetTextBoxHeight()
{
    return m_dHeight;
}

void TABText::SetTextBoxHeight(double dHeight)
{
    m_dHeight = dHeight;
    UpdateTextMBR();
}

/**********************************************************************
 *                   TABText::GetTextBoxWidth()
 *
 * Return text width in X axis coord. units. of the text box before rotation.
 *
 * If value has not been set, then we force a default value that assumes
 * that one char's box width is 60% of its height... and we ignore
 * the multiline case.  This should not matter when the user PROPERLY sets
 * the value.
 **********************************************************************/
double TABText::GetTextBoxWidth()
{
    if (m_dWidth == 0.0 && m_pszString)
    {
        m_dWidth = 0.6 * m_dHeight * strlen(m_pszString);
    }
    return m_dWidth;
}

void TABText::SetTextBoxWidth(double dWidth)
{
    m_dWidth = dWidth;
    UpdateTextMBR();
}

/**********************************************************************
 *                   TABText::UpdateTextMBR()
 *
 * Update the feature MBR using the text origin (OGRPoint geometry), the
 * rotation angle, and the Width/height before rotation.
 *
 * This function cannot perform properly unless all the above have been set.
 **********************************************************************/
void TABText::UpdateTextMBR()
{
    OGRGeometry *poGeom;
    OGRPoint *poPoint=NULL;

    poGeom = GetGeometryRef();
    if (poGeom && poGeom->getGeometryType() == wkbPoint)
    {
        double dSin, dCos, dX0, dY0, dX1, dY1;
        double dX[4], dY[4];
        poPoint = (OGRPoint *)poGeom;

        dX0 = poPoint->getX();
        dY0 = poPoint->getY();

        dSin = sin(m_dAngle*PI/180.0);
        dCos = cos(m_dAngle*PI/180.0);

        GetTextBoxWidth();  // Force default width value if necessary.
        
        dX[0] = dX0;
        dY[0] = dY0;
        dX[1] = dX0 + m_dWidth;
        dY[1] = dY0;
        dX[2] = dX0 + m_dWidth;
        dY[2] = dY0 + m_dHeight;
        dX[3] = dX0;
        dY[3] = dY0 + m_dHeight;

        SetMBR(dX0, dY0, dX0, dY0);
        for(int i=0; i<4; i++)
        {
            // Rotate one of the box corners
            dX1 = dX0 + (dX[i]-dX0)*dCos - (dY[i]-dY0)*dSin;
            dY1 = dY0 + (dX[i]-dX0)*dSin + (dY[i]-dY0)*dCos;

            // And update feature MBR with rotated coordinate
            if (dX1 < m_dXMin) m_dXMin = dX1;
            if (dX1 > m_dXMax) m_dXMax = dX1;
            if (dY1 < m_dYMin) m_dYMin = dY1;
            if (dY1 > m_dYMax) m_dYMax = dY1;
        }
    }
}

/**********************************************************************
 *                   TABText::GetFontBGColor()
 *
 * Return background color.
 **********************************************************************/
GInt32 TABText::GetFontBGColor()
{
    return m_rgbBackground;
}

void TABText::SetFontBGColor(GInt32 rgbColor)
{
    m_rgbBackground = rgbColor;
}

/**********************************************************************
 *                   TABText::GetFontFGColor()
 *
 * Return foreground color.
 **********************************************************************/
GInt32 TABText::GetFontFGColor()
{
    return m_rgbForeground;
}

void TABText::SetFontFGColor(GInt32 rgbColor)
{
    m_rgbForeground = rgbColor;
}

/**********************************************************************
 *                   TABText::GetTextJustification()
 *
 * Return text justification.  Default is TABTJLeft
 **********************************************************************/
TABTextJust TABText::GetTextJustification()
{
    TABTextJust eJust = TABTJLeft;

    if (m_nTextAlignment & 0x0200)
        eJust = TABTJCenter;
    else if (m_nTextAlignment & 0x0400)
        eJust = TABTJRight;

    return eJust;
}

void TABText::SetTextJustification(TABTextJust eJustification)
{
    // Flush current value... default is TABTJLeft
    m_nTextAlignment &= ~ 0x0600;
    // ... and set new one.
    if (eJustification == TABTJCenter)
        m_nTextAlignment |= 0x0200;
    else if (eJustification == TABTJRight)
        m_nTextAlignment |= 0x0400;
}

/**********************************************************************
 *                   TABText::GetTextSpacing()
 *
 * Return text vertical spacing factor.  Default is TABTSSingle
 **********************************************************************/
TABTextSpacing TABText::GetTextSpacing()
{
    TABTextSpacing eSpacing = TABTSSingle;

    if (m_nTextAlignment & 0x0800)
        eSpacing = TABTS1_5;
    else if (m_nTextAlignment & 0x1000)
        eSpacing = TABTSDouble;

    return eSpacing;
}

void TABText::SetTextSpacing(TABTextSpacing eSpacing)
{
    // Flush current value... default is TABTSSingle
    m_nTextAlignment &= ~ 0x1800;
    // ... and set new one.
    if (eSpacing == TABTS1_5)
        m_nTextAlignment |= 0x0800;
    else if (eSpacing == TABTSDouble)
        m_nTextAlignment |= 0x1000;
}

/**********************************************************************
 *                   TABText::GetTextLineType()
 *
 * Return text line (arrow) type.  Default is TABTLNoLine
 **********************************************************************/
TABTextLineType TABText::GetTextLineType()
{
    TABTextLineType eLine = TABTLNoLine;

    if (m_nTextAlignment & 0x2000)
        eLine = TABTLSimple;
    else if (m_nTextAlignment & 0x4000)
        eLine = TABTLArrow;

    return eLine;
}

void TABText::SetTextLineType(TABTextLineType eLineType)
{
    // Flush current value... default is TABTLNoLine
    m_nTextAlignment &= ~ 0x6000;
    // ... and set new one.
    if (eLineType == TABTLSimple)
        m_nTextAlignment |= 0x2000;
    else if (eLineType == TABTLArrow)
        m_nTextAlignment |= 0x4000;
}

/**********************************************************************
 *                   TABText::QueryFontStyle()
 *
 * Return TRUE if the specified font style attribute is turned ON,
 * or FALSE otherwise.  See enum TABFontStyle for the list of styles
 * that can be queried on.
 **********************************************************************/
GBool TABText::QueryFontStyle(TABFontStyle eStyleToQuery)
{
    return (m_nFontStyle & (int)eStyleToQuery) ? TRUE: FALSE;
}

void TABText::ToggleFontStyle(TABFontStyle eStyleToToggle, GBool bStyleOn)
{
    if (bStyleOn)
        m_nFontStyle |=  (int)eStyleToToggle;
    else
        m_nFontStyle &=  ~ (int)eStyleToToggle;
}


/**********************************************************************
 *                   TABText::GetFontStyleMIFValue()
 *
 * Return the Font Style value for this object using the style values
 * that are used in a MIF FONT() clause.  See MIF specs (appendix A).
 *
 * The reason why we have to differentiate between the TAB and the MIF font
 * style values is that in TAB, TABFSBox is included in the style value
 * as code 0x100, but in MIF it is not included, instead it is implied by
 * the presence of the BG color in the FONT() clause (the BG color is 
 * present only when TABFSBox or TABFSHalo is set).
 * This also has the effect of shifting all the other style values > 0x100
 * by 1 byte.
 **********************************************************************/
int TABText::GetFontStyleMIFValue()
{
    // The conversion is simply to remove bit 0x100 from the value and shift
    // down all values past this bit.
    return (m_nFontStyle & 0xff) + (m_nFontStyle & (0xff00-0x0100))/2;
}

void TABText:: SetFontStyleMIFValue(int nStyle, GBool bBGColorSet)
{
    m_nFontStyle = (nStyle & 0xff) + (nStyle & 0x7f00)*2;
    // When BG color is set, then either BOX or HALO should be set.
    if (bBGColorSet && !QueryFontStyle(TABFSHalo))
        ToggleFontStyle(TABFSBox, TRUE);
}

int TABText::IsFontBGColorUsed()
{
    // Font BG color is used only when BOX or HALO are set.
    return (QueryFontStyle(TABFSBox) || QueryFontStyle(TABFSHalo));
}


/**********************************************************************
 *                   TABText::DumpMIF()
 *
 * Dump feature geometry in a format similar to .MIF REGIONs.
 **********************************************************************/
void TABText::DumpMIF(FILE *fpOut /*=NULL*/)
{
    OGRGeometry   *poGeom;
    OGRPoint      *poPoint = NULL;

    if (fpOut == NULL)
        fpOut = stdout;

    /*-----------------------------------------------------------------
     * Fetch and validate geometry
     *----------------------------------------------------------------*/
    poGeom = GetGeometryRef();
    if (poGeom && poGeom->getGeometryType() == wkbPoint)
    {
        /*-------------------------------------------------------------
         * Generate output for text object
         *------------------------------------------------------------*/
        poPoint = (OGRPoint*)poGeom;

        fprintf(fpOut, "TEXT \"%s\" %g %g\n", m_pszString?m_pszString:"",
                poPoint->getX(), poPoint->getY());

        fprintf(fpOut, "  m_pszString = '%s'\n", m_pszString);
        fprintf(fpOut, "  m_dAngle    = %g\n",   m_dAngle);
        fprintf(fpOut, "  m_dHeight   = %g\n",   m_dHeight);
        fprintf(fpOut, "  m_rgbForeground  = 0x%6.6x (%d)\n", 
                                             m_rgbForeground, m_rgbForeground);
        fprintf(fpOut, "  m_rgbBackground  = 0x%6.6x (%d)\n", 
                                             m_rgbBackground, m_rgbBackground);
        fprintf(fpOut, "  m_nTextAlignment = 0x%4.4x\n",  m_nTextAlignment);
        fprintf(fpOut, "  m_nFontStyle     = 0x%4.4x\n",  m_nFontStyle);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABText: Missing or Invalid Geometry!");
        return;
    }

    // Finish with PEN/BRUSH/etc. clauses
    DumpPenDef();
    DumpFontDef();

    fflush(fpOut);
}


/*=====================================================================
 *                      class TABDebugFeature
 *====================================================================*/

/**********************************************************************
 *                   TABDebugFeature::TABDebugFeature()
 *
 * Constructor.
 **********************************************************************/
TABDebugFeature::TABDebugFeature(OGRFeatureDefn *poDefnIn):
              TABFeature(poDefnIn)
{
}

/**********************************************************************
 *                   TABDebugFeature::~TABDebugFeature()
 *
 * Destructor.
 **********************************************************************/
TABDebugFeature::~TABDebugFeature()
{
}

/**********************************************************************
 *                   TABDebugFeature::ReadGeometryFromMAPFile()
 *
 * Fill the geometry and representation (color, etc...) part of the
 * feature from the contents of the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to the beginning of
 * a map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABDebugFeature::ReadGeometryFromMAPFile(TABMAPFile *poMapFile)
{
    TABMAPObjectBlock   *poObjBlock;
    TABMAPHeaderBlock   *poHeader;

    /*-----------------------------------------------------------------
     * Fetch geometry type
     *----------------------------------------------------------------*/
    m_nMapInfoType = poMapFile->GetCurObjType();

    poObjBlock = poMapFile->GetCurObjBlock();
    poHeader = poMapFile->GetHeaderBlock();

    /*-----------------------------------------------------------------
     * If object type has coords in a type 3 block, then its position 
     * follows
     *----------------------------------------------------------------*/
    if (poHeader->MapObjectUsesCoordBlock(m_nMapInfoType))
    {
        m_nCoordDataPtr = poObjBlock->ReadInt32();
        m_nCoordDataSize = poObjBlock->ReadInt32();
    }
    else
    {
        m_nCoordDataPtr = -1;
        m_nCoordDataSize = 0;
    }

    m_nSize = poHeader->GetMapObjectSize(m_nMapInfoType);
    if (m_nSize > 0)
    {
        poObjBlock->GotoByteRel(-5);    // Go back to beginning of header
        poObjBlock->ReadBytes(m_nSize, m_abyBuf);
    }

    return 0;
}

/**********************************************************************
 *                   TABDebugFeature::WriteGeometryToMAPFile()
 *
 * Write the geometry and representation (color, etc...) part of the
 * feature to the .MAP object pointed to by poMAPFile.
 *
 * It is assumed that poMAPFile currently points to a valid map object.
 *
 * Returns 0 on success, -1 on error, in which case CPLError() will have
 * been called.
 **********************************************************************/
int TABDebugFeature::WriteGeometryToMAPFile(TABMAPFile * /*poMapFile*/)
{
    // Nothing to do here!

    CPLError(CE_Failure, CPLE_NotSupported,
             "TABDebugFeature::WriteGeometryToMAPFile() not implemented.\n");

    return -1;
}

/**********************************************************************
 *                   TABDebugFeature::DumpMIF()
 *
 * Dump feature contents... available only in DEBUG mode.
 **********************************************************************/
void TABDebugFeature::DumpMIF(FILE *fpOut /*=NULL*/)
{
    int i;

    if (fpOut == NULL)
        fpOut = stdout;

    fprintf(fpOut, "----- TABDebugFeature (type = 0x%2.2x) -----\n",
            GetMapInfoType());
    fprintf(fpOut, "  Object size: %d bytes\n", m_nSize);
    fprintf(fpOut, "  m_nCoordDataPtr  = %d\n", m_nCoordDataPtr);
    fprintf(fpOut, "  m_nCoordDataSize = %d\n", m_nCoordDataSize);
    fprintf(fpOut, "  ");

    for(i=0; i<m_nSize; i++)
        fprintf(fpOut, " %2.2x", m_abyBuf[i]);

    fprintf(fpOut, "  \n");


    fflush(fpOut);
}


/*=====================================================================
 *                      class ITABFeaturePen
 *====================================================================*/

/**********************************************************************
 *                   ITABFeaturePen::DumpPenDef()
 *
 * Dump pen definition information.
 **********************************************************************/
void ITABFeaturePen::DumpPenDef(FILE *fpOut /*=NULL*/)
{
    if (fpOut == NULL)
        fpOut = stdout;

    fprintf(fpOut, "  m_nPenDefIndex         = %d\n", m_nPenDefIndex);
    fprintf(fpOut, "  m_sPenDef.nRefCount    = %d\n", m_sPenDef.nRefCount);
    fprintf(fpOut, "  m_sPenDef.nLineWidth   = %d\n", m_sPenDef.nLineWidth);
    fprintf(fpOut, "  m_sPenDef.nLinePattern = %d\n", m_sPenDef.nLinePattern);
    fprintf(fpOut, "  m_sPenDef.nLineStyle   = %d\n", m_sPenDef.nLineStyle);
    fprintf(fpOut, "  m_sPenDef.rgbColor     = 0x%6.6x (%d)\n",
                                     m_sPenDef.rgbColor, m_sPenDef.rgbColor);

    fflush(fpOut);
}

/*=====================================================================
 *                      class ITABFeatureBrush
 *====================================================================*/

/**********************************************************************
 *                   ITABFeatureBrush::DumpBrushDef()
 *
 * Dump Brush definition information.
 **********************************************************************/
void ITABFeatureBrush::DumpBrushDef(FILE *fpOut /*=NULL*/)
{
    if (fpOut == NULL)
        fpOut = stdout;

    fprintf(fpOut, "  m_nBrushDefIndex         = %d\n", m_nBrushDefIndex);
    fprintf(fpOut, "  m_sBrushDef.nRefCount    = %d\n", m_sBrushDef.nRefCount);
    fprintf(fpOut, "  m_sBrushDef.nFillPattern = %d\n", 
                                                (int)m_sBrushDef.nFillPattern);
    fprintf(fpOut, "  m_sBrushDef.bTransparentFill = %d\n", 
                                            (int)m_sBrushDef.bTransparentFill);
    fprintf(fpOut, "  m_sBrushDef.rgbFGColor   = 0x%6.6x (%d)\n",
                               m_sBrushDef.rgbFGColor, m_sBrushDef.rgbFGColor);
    fprintf(fpOut, "  m_sBrushDef.rgbBGColor   = 0x%6.6x (%d)\n",
                               m_sBrushDef.rgbBGColor, m_sBrushDef.rgbBGColor);

    fflush(fpOut);
}

/*=====================================================================
 *                      class ITABFeatureFont
 *====================================================================*/

/**********************************************************************
 *                   ITABFeatureFont::DumpFontDef()
 *
 * Dump Font definition information.
 **********************************************************************/
void ITABFeatureFont::DumpFontDef(FILE *fpOut /*=NULL*/)
{
    if (fpOut == NULL)
        fpOut = stdout;

    fprintf(fpOut, "  m_nFontDefIndex       = %d\n", m_nFontDefIndex);
    fprintf(fpOut, "  m_sFontDef.nRefCount  = %d\n", m_sFontDef.nRefCount);
    fprintf(fpOut, "  m_sFontDef.szFontName = '%s'\n", m_sFontDef.szFontName);

    fflush(fpOut);
}


/*=====================================================================
 *                      class ITABFeatureSymbol
 *====================================================================*/

/**********************************************************************
 *                   ITABFeatureSymbol::DumpSymbolDef()
 *
 * Dump Symbol definition information.
 **********************************************************************/
void ITABFeatureSymbol::DumpSymbolDef(FILE *fpOut /*=NULL*/)
{
    if (fpOut == NULL)
        fpOut = stdout;

    fprintf(fpOut, "  m_nSymbolDefIndex       = %d\n", m_nSymbolDefIndex);
    fprintf(fpOut, "  m_sSymbolDef.nRefCount  = %d\n", m_sSymbolDef.nRefCount);
    fprintf(fpOut, "  m_sSymbolDef.nSymbolNo  = %d\n", m_sSymbolDef.nSymbolNo);
    fprintf(fpOut, "  m_sSymbolDef.nPointSize = %d\n",m_sSymbolDef.nPointSize);
    fprintf(fpOut, "  m_sSymbolDef._unknown_  = %d\n", 
                                            (int)m_sSymbolDef._nUnknownValue_);
    fprintf(fpOut, "  m_sSymbolDef.rgbColor   = 0x%6.6x (%d)\n",
                                m_sSymbolDef.rgbColor, m_sSymbolDef.rgbColor);

    fflush(fpOut);
}
