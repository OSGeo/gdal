/**********************************************************************
 * $Id: mitab_datfile.cpp,v 1.8 1999/12/14 03:58:29 daniel Exp $
 *
 * Name:     mitab_datfile.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation of the TABIDFile class used to handle
 *           reading/writing of the .DAT file
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
 * $Log: mitab_datfile.cpp,v $
 * Revision 1.8  1999/12/14 03:58:29  daniel
 * Fixed date read/write (bytes were reversed)
 *
 * Revision 1.7  1999/11/09 07:34:35  daniel
 * Return default values when deleted attribute records are encountered
 *
 * Revision 1.6  1999/10/19 06:09:25  daniel
 * Removed obsolete GetFieldDef() method
 *
 * Revision 1.5  1999/10/01 03:56:28  daniel
 * Avoid multiple InitWriteHeader() calls (caused a leak) and added a fix
 * in WriteCharField() to prevent reading bytes past end of string buffer
 *
 * Revision 1.4  1999/10/01 02:02:36  warmerda
 * Added assertions to try and track TABRawBinBlock leak.
 *
 * Revision 1.3  1999/09/26 14:59:36  daniel
 * Implemented write support
 *
 * Revision 1.2  1999/09/20 18:43:20  daniel
 * Use binary access to open file.
 *
 * Revision 1.1  1999/07/12 04:18:23  daniel
 * Initial checkin
 *
 **********************************************************************/

#include "mitab.h"

/*=====================================================================
 *                      class TABDATFile
 *
 * Note that the .DAT files are .DBF files with some exceptions:
 *
 * All fields in the DBF header are defined as 'C' type (strings),
 * even for binary integers.  So we have to look in the associated .TAB
 * file to find the real field definition.
 *
 * Even though binary integers are defined as 'C' type, they are stored
 * in binary form inside a 4 bytes string field.
 *====================================================================*/


/**********************************************************************
 *                   TABDATFile::TABDATFile()
 *
 * Constructor.
 **********************************************************************/
TABDATFile::TABDATFile()
{
    m_fp = NULL;
    m_pszFname = NULL;

    m_poHeaderBlock = NULL;
    m_poRecordBlock = NULL;
    m_pasFieldDef = NULL;

    m_numFields = -1;
    m_numRecords = -1;
    m_nFirstRecordPtr = 0;
    m_nBlockSize = 0;
    m_nRecordSize = -1;
    m_nCurRecordId = -1;
    m_bCurRecordDeletedFlag = FALSE;
    m_bWriteHeaderInitialized = FALSE;
}

/**********************************************************************
 *                   TABDATFile::~TABDATFile()
 *
 * Destructor.
 **********************************************************************/
TABDATFile::~TABDATFile()
{
    Close();
}

/**********************************************************************
 *                   TABDATFile::Open()
 *
 * Open a .DAT file, and initialize the structures to be ready to read
 * records from it.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABDATFile::Open(const char *pszFname, const char *pszAccess)
{
    int i;

    if (m_fp)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Open() failed: object already contains an open file");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Validate access mode and make sure we use binary access.
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
        CPLError(CE_Failure, CPLE_FileIO,
                 "Open() failed: access mode \"%s\" not supported", pszAccess);
        return -1;
    }

    /*-----------------------------------------------------------------
     * Open file for reading
     *----------------------------------------------------------------*/
    m_pszFname = CPLStrdup(pszFname);
    m_fp = VSIFOpen(m_pszFname, pszAccess);

    if (m_fp == NULL)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Open() failed for %s", m_pszFname);
        CPLFree(m_pszFname);
        m_pszFname = NULL;
        return -1;
    }

    if (m_eAccessMode == TABRead)
    {
        /*------------------------------------------------------------
         * READ ACCESS:
         * Read .DAT file header (record size, num records, etc...)
         * m_poHeaderBlock will be reused later to read field definition
         *-----------------------------------------------------------*/
        m_poHeaderBlock = new TABRawBinBlock(m_eAccessMode, TRUE);
        m_poHeaderBlock->ReadFromFile(m_fp, 0, 32);

        m_poHeaderBlock->ReadByte();       // Table type ??? 0x03
        m_poHeaderBlock->ReadByte();       // Last update year
        m_poHeaderBlock->ReadByte();       // Last update month
        m_poHeaderBlock->ReadByte();       // Last update day

        m_numRecords      = m_poHeaderBlock->ReadInt32();
        m_nFirstRecordPtr = m_poHeaderBlock->ReadInt16();
        m_nRecordSize     = m_poHeaderBlock->ReadInt16();

        m_numFields = m_nFirstRecordPtr/32 - 1;

        /*-------------------------------------------------------------
         * Read the field definitions
         * First 32 bytes field definition starts at byte 32 in file
         *------------------------------------------------------------*/
        m_pasFieldDef = (TABDATFieldDef*)CPLCalloc(m_numFields, 
                                                   sizeof(TABDATFieldDef));

        for(i=0; i<m_numFields; i++)
        {
            m_poHeaderBlock->GotoByteInFile((i+1)*32);
            m_poHeaderBlock->ReadBytes(11, (GByte*)m_pasFieldDef[i].szName);
            m_pasFieldDef[i].szName[10] = '\0';
            m_pasFieldDef[i].cType = (char)m_poHeaderBlock->ReadByte();

            m_poHeaderBlock->ReadInt32();       // Skip Bytes 12-15
            m_pasFieldDef[i].byLength = m_poHeaderBlock->ReadByte();
            m_pasFieldDef[i].byDecimals = m_poHeaderBlock->ReadByte();

            m_pasFieldDef[i].eTABType = TABFUnknown;
        }

        /*-------------------------------------------------------------
         * Establish a good record block size to use based on record size, and 
         * then create m_poRecordBlock
         * Record block size has to be a multiple of record size.
         *------------------------------------------------------------*/
        m_nBlockSize = ((1024/m_nRecordSize)+1)*m_nRecordSize;
        m_nBlockSize = MIN(m_nBlockSize, (m_numRecords*m_nRecordSize));

        CPLAssert( m_poRecordBlock == NULL );
        m_poRecordBlock = new TABRawBinBlock(m_eAccessMode, FALSE);
        m_poRecordBlock->InitNewBlock(m_fp, m_nBlockSize);
        m_poRecordBlock->SetFirstBlockPtr(m_nFirstRecordPtr);
    }
    else
    {
        /*------------------------------------------------------------
         * WRITE ACCESS:
         * Set acceptable defaults for all class members.
         * The real header initialization will be done when the first
         * record is written
         *-----------------------------------------------------------*/
        m_poHeaderBlock = NULL;

        m_numRecords      = 0;
        m_nFirstRecordPtr = 0;
        m_nRecordSize     = 0;
        m_numFields = 0;
        m_pasFieldDef = NULL;
        m_bWriteHeaderInitialized = FALSE;
    }

    return 0;
}

/**********************************************************************
 *                   TABDATFile::Close()
 *
 * Close current file, and release all memory used.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABDATFile::Close()
{
    if (m_fp == NULL)
        return 0;

    /*----------------------------------------------------------------
     * Write access: Update the header with number of records, etc.
     * and add a CTRL-Z char at the end of the file.
     *---------------------------------------------------------------*/
    if (m_eAccessMode == TABWrite)
    {
        WriteHeader();

        char cEOF = 26;
        if (VSIFSeek(m_fp, 0L, SEEK_END) == 0)
            VSIFWrite(&cEOF, 1, 1, m_fp);
    }
    
    // Delete all structures 
    if (m_poHeaderBlock)
    {
        delete m_poHeaderBlock;
        m_poHeaderBlock = NULL;
    }

    if (m_poRecordBlock)
    {
        delete m_poRecordBlock;
        m_poRecordBlock = NULL;
    }

    // Close file
    VSIFClose(m_fp);
    m_fp = NULL;

    CPLFree(m_pszFname);
    m_pszFname = NULL;

    CPLFree(m_pasFieldDef);
    m_pasFieldDef = NULL;

    m_numFields = -1;
    m_numRecords = -1;
    m_nFirstRecordPtr = 0;
    m_nBlockSize = 0;
    m_nRecordSize = -1;
    m_nCurRecordId = -1;
    m_bWriteHeaderInitialized = FALSE;

    return 0;
}


/**********************************************************************
 *                   TABDATFile::InitWriteHeader()
 *
 * Init the header members to be ready to write the header and data records
 * to a newly created data file.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int  TABDATFile::InitWriteHeader()
{
    int i;

    if (m_eAccessMode != TABWrite || m_bWriteHeaderInitialized)
        return 0;

    /*------------------------------------------------------------
     * Compute values for Record size, header size, etc.
     *-----------------------------------------------------------*/
    m_nFirstRecordPtr = (m_numFields+1)*32 + 1;

    m_nRecordSize = 1;
    for(i=0; i<m_numFields; i++)
    {
        m_nRecordSize += m_pasFieldDef[i].byLength;
    }

    /*-------------------------------------------------------------
     * Create m_poRecordBlock the size of a data record.
     *------------------------------------------------------------*/
    m_nBlockSize = m_nRecordSize;

    CPLAssert( m_poRecordBlock == NULL );
    m_poRecordBlock = new TABRawBinBlock(m_eAccessMode, FALSE);
    m_poRecordBlock->InitNewBlock(m_fp, m_nBlockSize);
    m_poRecordBlock->SetFirstBlockPtr(m_nFirstRecordPtr);

    /*-------------------------------------------------------------
     * Make sure this init. will be performed only once
     *------------------------------------------------------------*/
    m_bWriteHeaderInitialized = TRUE;

    return 0;
}

/**********************************************************************
 *                   TABDATFile::WriteHeader()
 *
 * Init the header members to be ready to write the header and data records
 * to a newly created data file.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int  TABDATFile::WriteHeader()
{
    int i;

    if (m_eAccessMode != TABWrite)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "WriteHeader() can be used only with Write access.");
        return -1;
    }

    if (!m_bWriteHeaderInitialized)
        InitWriteHeader();

    /*------------------------------------------------------------
     * Create a single block that will be used to generate the whole header.
     *-----------------------------------------------------------*/
    if (m_poHeaderBlock == NULL)
        m_poHeaderBlock = new TABRawBinBlock(m_eAccessMode, TRUE);
    m_poHeaderBlock->InitNewBlock(m_fp, m_nFirstRecordPtr, 0);

    /*------------------------------------------------------------
     * First 32 bytes: main header block
     *-----------------------------------------------------------*/
    m_poHeaderBlock->WriteByte(0x03);  // Table type ??? 0x03

    // __TODO__ Write the correct update date value
    m_poHeaderBlock->WriteByte(99);    // Last update year
    m_poHeaderBlock->WriteByte(9);     // Last update month
    m_poHeaderBlock->WriteByte(9);     // Last update day

    m_poHeaderBlock->WriteInt32(m_numRecords);
    m_poHeaderBlock->WriteInt16(m_nFirstRecordPtr);
    m_poHeaderBlock->WriteInt16(m_nRecordSize);

    m_poHeaderBlock->WriteZeros(20);    // Pad rest with zeros

    /*-------------------------------------------------------------
     * Field definitions follow.  Each field def is 32 bytes.
     *------------------------------------------------------------*/
    for(i=0; i<m_numFields; i++)
    {
        m_poHeaderBlock->WriteBytes(11, (GByte*)m_pasFieldDef[i].szName);
        m_poHeaderBlock->WriteByte(m_pasFieldDef[i].cType);

        m_poHeaderBlock->WriteInt32(0);       // Skip Bytes 12-15

        m_poHeaderBlock->WriteByte(m_pasFieldDef[i].byLength);
        m_poHeaderBlock->WriteByte(m_pasFieldDef[i].byDecimals);

        m_poHeaderBlock->WriteZeros(14);    // Pad rest with zeros
    }

    /*-------------------------------------------------------------
     * Header ends with a 0x0d character.
     *------------------------------------------------------------*/
    m_poHeaderBlock->WriteByte(0x0d);

    /*-------------------------------------------------------------
     * Write the block to the file and return.
     *------------------------------------------------------------*/
    return m_poHeaderBlock->CommitToFile();
}



/**********************************************************************
 *                   TABDATFile::GetNumFields()
 *
 * Return the number of fields in this table.
 *
 * Returns a value >= 0 on success, -1 on error.
 **********************************************************************/
int  TABDATFile::GetNumFields()
{
    return m_numFields;
}

/**********************************************************************
 *                   TABDATFile::GetNumRecords()
 *
 * Return the number of records in this table.
 *
 * Returns a value >= 0 on success, -1 on error.
 **********************************************************************/
int  TABDATFile::GetNumRecords()
{
    return m_numRecords;
}

/**********************************************************************
 *                   TABDATFile::GetRecordBlock()
 *
 * Return a TABRawBinBlock reference positioned at the beginning of the
 * specified record and ready to read (or write) field values from/to it.
 * In read access, the returned block is guaranteed to contain at least one
 * full record of data, and in write access, it is at least big enough to
 * hold one full record.
 * 
 * Note that record ids are positive and start at 1.
 *
 * In Write access, CommitRecordToFile() MUST be called after the
 * data items have been written to the record, otherwise the record 
 * will never make it to the file.
 *
 * Returns a reference to the TABRawBinBlock on success or NULL on error.
 * The returned pointer is a reference to a block object owned by this 
 * TABDATFile object and should not be freed by the caller.
 **********************************************************************/
TABRawBinBlock *TABDATFile::GetRecordBlock(int nRecordId)
{
    m_bCurRecordDeletedFlag = FALSE;

    if (m_eAccessMode == TABRead)
    {
        /*-------------------------------------------------------------
         * READ ACCESS
         *------------------------------------------------------------*/
        int nFileOffset;

        nFileOffset = m_nFirstRecordPtr+(nRecordId-1)*m_nRecordSize;

        /*-------------------------------------------------------------
         * Move record block pointer to the right location
         *------------------------------------------------------------*/
        if ( m_poRecordBlock == NULL || 
             nRecordId < 1 || nRecordId > m_numRecords ||
             m_poRecordBlock->GotoByteInFile(nFileOffset) != 0 )
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed reading .DAT record block for record #%d in %s",
                     nRecordId, m_pszFname);
            return NULL;
        }

        /*-------------------------------------------------------------
         * The first char of the record is a ' ' for an active record, or
         * '*' for a deleted one.
         * In the case of a deleted record, we simply return default
         * values for each attribute... this is what MapInfo seems to do
         * when it takes a .TAB with deleted records and exports it to .MIF
         *------------------------------------------------------------*/
        if (m_poRecordBlock->ReadByte() != ' ')
        {
            m_bCurRecordDeletedFlag = TRUE;
        }
    }
    else if (m_eAccessMode == TABWrite && nRecordId > 0)
    {
        /*-------------------------------------------------------------
         * WRITE ACCESS
         *------------------------------------------------------------*/
        int nFileOffset;

        /*-------------------------------------------------------------
         * Before writing the first record, we must generate the file 
         * header.  We will also initialize class members such as record
         * size, etc. and will create m_poRecordBlock.
         *------------------------------------------------------------*/
        if (!m_bWriteHeaderInitialized)
        {
            WriteHeader();
        }

        m_numRecords = MAX(nRecordId, m_numRecords);

        nFileOffset = m_nFirstRecordPtr+(nRecordId-1)*m_nRecordSize;

        m_poRecordBlock->InitNewBlock(m_fp, m_nRecordSize, nFileOffset);

        /*-------------------------------------------------------------
         * The first char of the record is the active/deleted flag.
         * Automatically set it to ' ' (active).
         *------------------------------------------------------------*/
        m_poRecordBlock->WriteByte(' ');

    }

    return m_poRecordBlock;
}

/**********************************************************************
 *                   TABDATFile::CommitRecordToFile()
 *
 * Commit the data record previously initialized with GetRecordBlock()
 * to the file.  This function must be called after writing the data
 * values to a record otherwise the record will never make it to the
 * file.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int  TABDATFile::CommitRecordToFile()
{
    if (m_eAccessMode != TABWrite || m_poRecordBlock == NULL)
        return -1;

    return m_poRecordBlock->CommitToFile();
}


/**********************************************************************
 *                   TABDATFile::ValidateFieldInfoFromTAB()
 *
 * Check that the value read from the .TAB file by the caller are 
 * consistent with what is found in the .DAT header.
 *
 * Note that field ids are positive and start at 0.
 *
 * We have to use this function when opening a file for reading since 
 * the .DAT file does not contain the full field types information...
 * a .DAT file is actually a .DBF file in which the .DBF types are
 * handled in a special way... type 'C' fields are used to store binary 
 * values for most MapInfo types.
 *
 * Returns a value >= 0 if OK, -1 on error.
 **********************************************************************/
int  TABDATFile::ValidateFieldInfoFromTAB(int iField, const char *pszName,
                                          TABFieldType eType,
                                          int nWidth, int nPrecision)
{
    int i = iField;  // Just to make things shorter

    CPLAssert(m_pasFieldDef);
    CPLAssert(iField >= 0 && iField < m_numFields);

    if (m_pasFieldDef == NULL ||
        !EQUALN(pszName, 
                m_pasFieldDef[i].szName, strlen(m_pasFieldDef[i].szName)) ||
        (eType == TABFChar && (m_pasFieldDef[i].cType != 'C' ||
                               m_pasFieldDef[i].byLength != nWidth )) ||
        (eType == TABFDecimal && (m_pasFieldDef[i].cType != 'N' ||
                                  m_pasFieldDef[i].byLength != nWidth||
                                 m_pasFieldDef[i].byDecimals != nPrecision)) ||
        (eType == TABFInteger && (m_pasFieldDef[i].cType != 'C' ||
                                  m_pasFieldDef[i].byLength != 4  )) ||
        (eType == TABFSmallInt && (m_pasFieldDef[i].cType != 'C' ||
                                   m_pasFieldDef[i].byLength != 2 )) ||
        (eType == TABFFloat && (m_pasFieldDef[i].cType != 'C' ||
                                m_pasFieldDef[i].byLength != 8    )) ||
        (eType == TABFDate && (m_pasFieldDef[i].cType != 'C' ||
                               m_pasFieldDef[i].byLength != 4     )) ||
        (eType == TABFLogical && (m_pasFieldDef[i].cType != 'L' ||
                                  m_pasFieldDef[i].byLength != 1  ))   )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Definition of field %d (%s) from .TAB file does not match "
                 "what is found in %s (name=%s, type=%c, width=%d, prec=%d)",
                 iField+1, pszName, m_pszFname,
                 m_pasFieldDef[i].szName, m_pasFieldDef[i].cType, 
                 m_pasFieldDef[i].byLength, m_pasFieldDef[i].byDecimals);
        return -1;
    }

    m_pasFieldDef[i].eTABType = eType;

    return 0;
}

/**********************************************************************
 *                   TABDATFile::AddField()
 *
 * Create a new field (column) in a newly created table.  This function
 * must be called after the file has been opened, but before writing the
 * first record.
 *
 * Returns the new field index (a value >= 0) if OK, -1 on error.
 **********************************************************************/
int  TABDATFile::AddField(const char *pszName, TABFieldType eType,
                          int nWidth, int nPrecision /*=0*/)
{
    if (m_eAccessMode != TABWrite || m_bWriteHeaderInitialized)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Addition of new table fields is not supported after the "
                 "first data item has been written.");
        return -1;
    }

    if (m_numFields < 0)
        m_numFields = 0;

    m_numFields++;
    m_pasFieldDef = (TABDATFieldDef*)CPLRealloc(m_pasFieldDef, 
                                          m_numFields*sizeof(TABDATFieldDef));

    strncpy(m_pasFieldDef[m_numFields-1].szName, pszName, 10);
    m_pasFieldDef[m_numFields-1].szName[10] = '\0';
    m_pasFieldDef[m_numFields-1].eTABType = eType;
    m_pasFieldDef[m_numFields-1].byLength = (GByte)nWidth;
    m_pasFieldDef[m_numFields-1].byDecimals = (GByte)nPrecision;

    switch(eType)
    {
      case TABFChar:
        m_pasFieldDef[m_numFields-1].cType = 'C';
        break;
      case TABFDecimal:
        m_pasFieldDef[m_numFields-1].cType = 'N';
        break;
      case TABFInteger:
        m_pasFieldDef[m_numFields-1].cType = 'C';
        m_pasFieldDef[m_numFields-1].byLength = 4;
        break;
      case TABFSmallInt:
        m_pasFieldDef[m_numFields-1].cType = 'C';
        m_pasFieldDef[m_numFields-1].byLength = 2;
        break;
      case TABFFloat:
        m_pasFieldDef[m_numFields-1].cType = 'C';
        m_pasFieldDef[m_numFields-1].byLength = 8;
        break;
      case TABFDate:
        m_pasFieldDef[m_numFields-1].cType = 'C';
        m_pasFieldDef[m_numFields-1].byLength = 4;
        break;
      case TABFLogical:
        m_pasFieldDef[m_numFields-1].cType = 'L';
        m_pasFieldDef[m_numFields-1].byLength = 1;
        break;
      default:
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported field type for field `%s'", pszName);
        return -1;
    }

    return 0;
}

/**********************************************************************
 *                   TABDATFile::GetFieldType()
 *
 * Returns the native field type for field # nFieldId as previously set
 * by ValidateFieldInfoFromTAB().
 *
 * Note that field ids are positive and start at 0.
 **********************************************************************/
TABFieldType TABDATFile::GetFieldType(int nFieldId)
{
    if (m_pasFieldDef == NULL || nFieldId < 0 || nFieldId >= m_numFields)
        return TABFUnknown;

    return m_pasFieldDef[nFieldId].eTABType;
}

/**********************************************************************
 *                   TABDATFile::GetFieldWidth()
 *
 * Returns the width for field # nFieldId as previously read from the
 * .DAT header.
 *
 * Note that field ids are positive and start at 0.
 **********************************************************************/
int   TABDATFile::GetFieldWidth(int nFieldId)
{
    if (m_pasFieldDef == NULL || nFieldId < 0 || nFieldId >= m_numFields)
        return 0;

    return m_pasFieldDef[nFieldId].byLength;
}

/**********************************************************************
 *                   TABDATFile::GetFieldPrecision()
 *
 * Returns the precision for field # nFieldId as previously read from the
 * .DAT header.
 *
 * Note that field ids are positive and start at 0.
 **********************************************************************/
int   TABDATFile::GetFieldPrecision(int nFieldId)
{
    if (m_pasFieldDef == NULL || nFieldId < 0 || nFieldId >= m_numFields)
        return 0;

    return m_pasFieldDef[nFieldId].byDecimals;
}

/**********************************************************************
 *                   TABDATFile::ReadCharField()
 *
 * Read the character field value at the current position in the data 
 * block.
 * 
 * Use GetRecordBlock() to position the data block to the beginning of
 * a record before attempting to read values.
 *
 * nWidth is the field length, as defined in the .DAT header.
 *
 * Returns a reference to an internal buffer that will be valid only until
 * the next field is read, or "" if the operation failed, in which case
 * CPLError() will have been called.
 **********************************************************************/
const char *TABDATFile::ReadCharField(int nWidth)
{
    // We know that character strings are limited to 254 chars in MapInfo
    static char szBuf[256];

    // If current record has been deleted, then return an acceptable 
    // default value.
    if (m_bCurRecordDeletedFlag)
        return "";

    if (m_poRecordBlock == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "Can't read field value: file is not opened.");
        return "";
    }

    if (nWidth < 1 || nWidth > 255)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "Illegal width for a char field: %d", nWidth);
        return "";
    }

    if (m_poRecordBlock->ReadBytes(nWidth, (GByte*)szBuf) != 0)
        return "";

    szBuf[nWidth] = '\0';

    return szBuf;
}

/**********************************************************************
 *                   TABDATFile::ReadIntegerField()
 *
 * Read the integer field value at the current position in the data 
 * block.
 * 
 * CPLError() will have been called if something fails.
 **********************************************************************/
GInt32 TABDATFile::ReadIntegerField()
{
    // If current record has been deleted, then return an acceptable 
    // default value.
    if (m_bCurRecordDeletedFlag)
        return 0;

    if (m_poRecordBlock == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "Can't read field value: file is not opened.");
        return 0;
    }

    return m_poRecordBlock->ReadInt32();
}

/**********************************************************************
 *                   TABDATFile::ReadSmallIntField()
 *
 * Read the smallint field value at the current position in the data 
 * block.
 * 
 * CPLError() will have been called if something fails.
 **********************************************************************/
GInt16 TABDATFile::ReadSmallIntField()
{
    // If current record has been deleted, then return an acceptable 
    // default value.
    if (m_bCurRecordDeletedFlag)
        return 0;

    if (m_poRecordBlock == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "Can't read field value: file is not opened.");
        return 0;
    }

    return m_poRecordBlock->ReadInt16();
}

/**********************************************************************
 *                   TABDATFile::ReadFloatField()
 *
 * Read the float field value at the current position in the data 
 * block.
 * 
 * CPLError() will have been called if something fails.
 **********************************************************************/
double TABDATFile::ReadFloatField()
{
    // If current record has been deleted, then return an acceptable 
    // default value.
    if (m_bCurRecordDeletedFlag)
        return 0.0;

    if (m_poRecordBlock == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "Can't read field value: file is not opened.");
        return 0.0;
    }

    return m_poRecordBlock->ReadDouble();
}

/**********************************************************************
 *                   TABDATFile::ReadLogicalField()
 *
 * Read the logical field value at the current position in the data 
 * block.
 *
 * The file contains either 0 or 1, and we return a string with 
 * "F" (false) or "T" (true)
 * 
 * CPLError() will have been called if something fails.
 **********************************************************************/
const char *TABDATFile::ReadLogicalField()
{
    GByte bValue;

    // If current record has been deleted, then return an acceptable 
    // default value.
    if (m_bCurRecordDeletedFlag)
        return "F";

    if (m_poRecordBlock == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "Can't read field value: file is not opened.");
        return "";
    }

    bValue =  m_poRecordBlock->ReadByte();

    return bValue? "T":"F";
}

/**********************************************************************
 *                   TABDATFile::ReadDateField()
 *
 * Read the logical field value at the current position in the data 
 * block.
 *
 * A date field is a 4 bytes binary value in which the first byte is
 * the day, followed by 1 byte for the month, and 2 bytes for the year.
 *
 * We return a 10 chars string in the format "DD/MM/YYYY"
 * 
 * Returns a reference to an internal buffer that will be valid only until
 * the next field is read, or "" if the operation failed, in which case
 * CPLError() will have been called.
 **********************************************************************/
const char *TABDATFile::ReadDateField()
{
    int nDay, nMonth, nYear;
    static char szBuf[20];

    // If current record has been deleted, then return an acceptable 
    // default value.
    if (m_bCurRecordDeletedFlag)
        return "00/00/0000";

    if (m_poRecordBlock == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "Can't read field value: file is not opened.");
        return "";
    }

    nYear  = m_poRecordBlock->ReadInt16();
    nMonth = m_poRecordBlock->ReadByte();
    nDay   = m_poRecordBlock->ReadByte();

    if (CPLGetLastErrorNo() != 0)
        return "";

    sprintf(szBuf, "%2.2d/%2.2d/%4.4d", nDay, nMonth, nYear);

    return szBuf;
}

/**********************************************************************
 *                   TABDATFile::ReadDecimalField()
 *
 * Read the decimal field value at the current position in the data 
 * block.
 *
 * A decimal field is a floating point value with a fixed number of digits
 * stored as a character string.
 *
 * nWidth is the field length, as defined in the .DAT header.
 *
 * We return the value as a binary double.
 * 
 * CPLError() will have been called if something fails.
 **********************************************************************/
double TABDATFile::ReadDecimalField(int nWidth)
{
    const char *pszVal;

    // If current record has been deleted, then return an acceptable 
    // default value.
    if (m_bCurRecordDeletedFlag)
        return 0.0;

    pszVal = ReadCharField(nWidth);

    return atof(pszVal);
}


/**********************************************************************
 *                   TABDATFile::WriteCharField()
 *
 * Write the character field value at the current position in the data 
 * block.
 * 
 * Use GetRecordBlock() to position the data block to the beginning of
 * a record before attempting to write values.
 *
 * nWidth is the field length, as defined in the .DAT header.
 *
 * Returns 0 on success, or -1 if the operation failed, in which case
 * CPLError() will have been called.
 **********************************************************************/
int TABDATFile::WriteCharField(const char *pszStr, int nWidth)
{
    if (m_poRecordBlock == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
            "Can't write field value: GetRecordBlock() has not been called.");
        return -1;
    }

    if (nWidth < 1 || nWidth > 255)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "Illegal width for a char field: %d", nWidth);
        return -1;
    }
    
    //
    // Write the buffer after making sure that we don't try to read
    // past the end of the source buffer.  The rest of the field will
    // be padded with zeros if source string is shorter than specified
    // field width.
    //
    int nLen = strlen(pszStr);
    nLen = MIN(nLen, nWidth);

    if ((nLen>0 && m_poRecordBlock->WriteBytes(nLen, (GByte*)pszStr) != 0) ||
        (nWidth-nLen > 0 && m_poRecordBlock->WriteZeros(nWidth-nLen)!=0) )
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABDATFile::WriteIntegerField()
 *
 * Write the integer field value at the current position in the data 
 * block.
 * 
 * CPLError() will have been called if something fails.
 **********************************************************************/
int TABDATFile::WriteIntegerField(GInt32 nValue)
{
    if (m_poRecordBlock == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
            "Can't write field value: GetRecordBlock() has not been called.");
        return -1;
    }

    return m_poRecordBlock->WriteInt32(nValue);
}

/**********************************************************************
 *                   TABDATFile::WriteSmallIntField()
 *
 * Write the smallint field value at the current position in the data 
 * block.
 * 
 * CPLError() will have been called if something fails.
 **********************************************************************/
int TABDATFile::WriteSmallIntField(GInt16 nValue)
{
    if (m_poRecordBlock == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
            "Can't write field value: GetRecordBlock() has not been called.");
        return -1;
    }

    return m_poRecordBlock->WriteInt16(nValue);
}

/**********************************************************************
 *                   TABDATFile::WriteFloatField()
 *
 * Write the float field value at the current position in the data 
 * block.
 * 
 * CPLError() will have been called if something fails.
 **********************************************************************/
int TABDATFile::WriteFloatField(double dValue)
{
    if (m_poRecordBlock == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
            "Can't write field value: GetRecordBlock() has not been called.");
        return -1;
    }

    return m_poRecordBlock->WriteDouble(dValue);
}

/**********************************************************************
 *                   TABDATFile::WriteLogicalField()
 *
 * Write the logical field value at the current position in the data 
 * block.
 *
 * The value written to the file is either 0 or 1, but this function
 * takes as input a string with "F" (false) or "T" (true)
 * 
 * CPLError() will have been called if something fails.
 **********************************************************************/
int TABDATFile::WriteLogicalField(const char *pszValue)
{
    GByte bValue;

    if (m_poRecordBlock == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
            "Can't write field value: GetRecordBlock() has not been called.");
        return -1;
    }

    if (EQUALN(pszValue, "T", 1))
        bValue = 1;
    else
        bValue = 0;

    return m_poRecordBlock->WriteByte(bValue);
}

/**********************************************************************
 *                   TABDATFile::WriteDateField()
 *
 * Write the date field value at the current position in the data 
 * block.
 *
 * A date field is a 4 bytes binary value in which the first byte is
 * the day, followed by 1 byte for the month, and 2 bytes for the year.
 *
 * The expected input is a 10 chars string in the format "DD/MM/YYYY"
 * 
 * Returns 0 on success, or -1 if the operation failed, in which case
 * CPLError() will have been called.
 **********************************************************************/
int TABDATFile::WriteDateField(const char *pszValue)
{
    int nDay, nMonth, nYear;
    char **papszTok = NULL;

    if (m_poRecordBlock == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
            "Can't write field value: GetRecordBlock() has not been called.");
        return -1;
    }

    papszTok = CSLTokenizeStringComplex(pszValue, "/", FALSE, FALSE);
    
    if (CSLCount(papszTok) != 3 ||
        (nDay = atoi(papszTok[0])) < 1 || nDay > 31 ||
        (nMonth = atoi(papszTok[1])) < 1 || nMonth > 12 ||
        strlen(papszTok[2]) != 4 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid date field value `%s'.  Date field values must "
                 "be in the format `MM/DD/YYYY'");
        CSLDestroy(papszTok);
        return -1;
    }

    nYear = atoi(papszTok[2]);

    CSLDestroy(papszTok);

    m_poRecordBlock->WriteInt16(nYear);
    m_poRecordBlock->WriteByte(nMonth);
    m_poRecordBlock->WriteByte(nDay);

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABDATFile::WriteDecimalField()
 *
 * Write the decimal field value at the current position in the data 
 * block.
 *
 * A decimal field is a floating point value with a fixed number of digits
 * stored as a character string.
 *
 * nWidth is the field length, as defined in the .DAT header.
 *
 * CPLError() will have been called if something fails.
 **********************************************************************/
int TABDATFile::WriteDecimalField(double dValue, int nWidth, int nPrec)
{
    const char *pszVal;

    pszVal = CPLSPrintf("%*.*f", nWidth, nPrec, dValue);
    if ((int)strlen(pszVal) > nWidth)
        pszVal += strlen(pszVal) - nWidth;

    return m_poRecordBlock->WriteBytes(nWidth, (GByte*)pszVal);
}



/**********************************************************************
 *                   TABDATFile::Dump()
 *
 * Dump block contents... available only in DEBUG mode.
 **********************************************************************/
#ifdef DEBUG

void TABDATFile::Dump(FILE *fpOut /*=NULL*/)
{
    if (fpOut == NULL)
        fpOut = stdout;

    fprintf(fpOut, "----- TABDATFile::Dump() -----\n");

    if (m_fp == NULL)
    {
        fprintf(fpOut, "File is not opened.\n");
    }
    else
    {
        fprintf(fpOut, "File is opened: %s\n", m_pszFname);
        fprintf(fpOut, "m_numFields  = %d\n", m_numFields);
        fprintf(fpOut, "m_numRecords = %d\n", m_numRecords);
    }

    fflush(fpOut);
}

#endif // DEBUG





