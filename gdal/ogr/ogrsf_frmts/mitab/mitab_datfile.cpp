/**********************************************************************
 *
 * Name:     mitab_datfile.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation of the TABIDFile class used to handle
 *           reading/writing of the .DAT file
 * Author:   Daniel Morissette, dmorissette@dmsolutions.ca
 *
 **********************************************************************
 * Copyright (c) 1999-2001, Daniel Morissette
 * Copyright (c) 2014, Even Rouault <even.rouault at spatialys.com>
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
 * $Log: mitab_datfile.cpp,v $
 * Revision 1.22  2010-07-07 19:00:15  aboudreault
 * Cleanup Win32 Compile Warnings (GDAL bug #2930)
 *
 * Revision 1.21  2009-06-08 20:30:46  dmorissette
 * Fixed threading issue (static buffer) in Date and DateTime code (GDAL
 * ticket #1883)
 *
 * Revision 1.20  2008-11-27 20:50:22  aboudreault
 * Improved support for OGR date/time types. New Read/Write methods (bug 1948)
 * Added support of OGR date/time types for MIF features.
 *
 * Revision 1.19  2008/01/29 20:46:32  dmorissette
 * Added support for v9 Time and DateTime fields (byg 1754)
 *
 * Revision 1.18  2007/10/09 17:43:16  fwarmerdam
 * Remove static variables that interfere with reentrancy. (GDAL #1883)
 *
 * Revision 1.17  2004/06/30 20:29:03  dmorissette
 * Fixed refs to old address danmo@videotron.ca
 *
 * Revision 1.16  2001/05/01 12:32:03  daniel
 * Get rid of leading spaces in WriteDateField().
 *
 * Revision 1.15  2000/04/27 15:42:03  daniel
 * Map variable field length (width=0) coming from OGR to acceptable default
 *
 * Revision 1.14  2000/02/28 16:52:52  daniel
 * Added support for writing indexes, removed validation on field name in
 * NATIVE tables, and remove trailing spaces in DBF char field values
 *
 * Revision 1.13  2000/01/28 07:31:49  daniel
 * Validate char field width (must be <= 254 chars)
 *
 * Revision 1.12  2000/01/16 19:08:48  daniel
 * Added support for reading 'Table Type DBF' tables
 *
 * Revision 1.11  2000/01/15 22:30:43  daniel
 * Switch to MIT/X-Consortium OpenSource license
 *
 * Revision 1.10  1999/12/20 18:59:20  daniel
 * Dates again... now returned as "YYYYMMDD"
 *
 * Revision 1.9  1999/12/16 17:11:45  daniel
 * Date fields: return as "YYYY/MM/DD", and accept 3 diff. formats as input
 *
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
#include "ogr_p.h"

#include <algorithm>

CPL_CVSID("$Id$");

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
TABDATFile::TABDATFile() :
    m_pszFname(NULL),
    m_fp(NULL),
    m_eAccessMode(TABRead),
    m_eTableType(TABTableNative),
    m_poHeaderBlock(NULL),
    m_numFields(-1),
    m_pasFieldDef(NULL),
    m_poRecordBlock(NULL),
    m_nBlockSize(0),
    m_nRecordSize(-1),
    m_nCurRecordId(-1),
    m_bCurRecordDeletedFlag(FALSE),
    m_numRecords(-1),
    m_nFirstRecordPtr(0),
    m_bWriteHeaderInitialized(FALSE),
    m_bWriteEOF(FALSE),
    m_bUpdated(FALSE)
    // TODO(schwehr): m_szBuffer({})
{}

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
 * Compatibility layer with new interface.
 * Return 0 on success, -1 in case of failure.
 **********************************************************************/

int TABDATFile::Open(const char *pszFname, const char* pszAccess, TABTableType eTableType)
{
    if( STARTS_WITH_CI(pszAccess, "r") )
        return Open(pszFname, TABRead, eTableType);
    else if( STARTS_WITH_CI(pszAccess, "w") )
        return Open(pszFname, TABWrite, eTableType);
    else
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Open() failed: access mode \"%s\" not supported", pszAccess);
        return -1;
    }
}

/**********************************************************************
 *                   TABDATFile::Open()
 *
 * Open a .DAT file, and initialize the structures to be ready to read
 * records from it.
 *
 * We currently support NATIVE and DBF tables for reading, and only
 * NATIVE tables for writing.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABDATFile::Open(const char *pszFname, TABAccess eAccess,
                     TABTableType eTableType /*=TABNativeTable*/)
{
    if (m_fp)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Open() failed: object already contains an open file");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Validate access mode and make sure we use binary access.
     *----------------------------------------------------------------*/
    const char* pszAccess = NULL;
    if (eAccess == TABRead && (eTableType==TABTableNative ||
                                      eTableType==TABTableDBF)  )
    {
        pszAccess = "rb";
    }
    else if (eAccess == TABWrite && eTableType==TABTableNative)
    {
        pszAccess = "wb+";
    }
    else if (eAccess == TABReadWrite && eTableType==TABTableNative)
    {
        pszAccess = "rb+";
    }
    else
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Open() failed: access mode \"%d\" not supported with eTableType=%d",
                 eAccess, eTableType);
        return -1;
    }
    m_eAccessMode = eAccess;

    /*-----------------------------------------------------------------
     * Open file for reading
     *----------------------------------------------------------------*/
    m_pszFname = CPLStrdup(pszFname);
    m_fp = VSIFOpenL(m_pszFname, pszAccess);
    m_eTableType = eTableType;

    if (m_fp == NULL)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Open() failed for %s", m_pszFname);
        CPLFree(m_pszFname);
        m_pszFname = NULL;
        return -1;
    }

    if (m_eAccessMode == TABRead || m_eAccessMode == TABReadWrite)
    {
        /*------------------------------------------------------------
         * READ ACCESS:
         * Read .DAT file header (record size, num records, etc...)
         * m_poHeaderBlock will be reused later to read field definition
         *-----------------------------------------------------------*/
        m_poHeaderBlock = new TABRawBinBlock(m_eAccessMode, TRUE);
        CPL_IGNORE_RET_VAL(m_poHeaderBlock->ReadFromFile(m_fp, 0, 32));

        m_poHeaderBlock->ReadByte();       // Table type ??? 0x03
        m_poHeaderBlock->ReadByte();       // Last update year
        m_poHeaderBlock->ReadByte();       // Last update month
        m_poHeaderBlock->ReadByte();       // Last update day

        m_numRecords      = m_poHeaderBlock->ReadInt32();
        m_nFirstRecordPtr = m_poHeaderBlock->ReadInt16();
        m_nRecordSize     = m_poHeaderBlock->ReadInt16();
        if( m_nFirstRecordPtr < 32 || m_nRecordSize <= 0 || m_numRecords < 0 )
        {
            VSIFCloseL(m_fp);
            m_fp = NULL;
            CPLFree(m_pszFname);
            m_pszFname = NULL;
            delete m_poHeaderBlock;
            m_poHeaderBlock = NULL;
            return -1;
        }
        m_numFields = m_nFirstRecordPtr/32 - 1;

        /*-------------------------------------------------------------
         * Read the field definitions
         * First 32 bytes field definition starts at byte 32 in file
         *------------------------------------------------------------*/
        m_pasFieldDef = (TABDATFieldDef*)CPLCalloc(m_numFields,
                                                   sizeof(TABDATFieldDef));

        for( int i = 0; i < m_numFields; i++ )
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
        if( m_numRecords < INT_MAX / m_nRecordSize )
            m_nBlockSize = std::min(m_nBlockSize, (m_numRecords*m_nRecordSize));

        CPLAssert( m_poRecordBlock == NULL );
        m_poRecordBlock = new TABRawBinBlock(m_eAccessMode, FALSE);
        m_poRecordBlock->InitNewBlock(m_fp, m_nBlockSize);
        m_poRecordBlock->SetFirstBlockPtr(m_nFirstRecordPtr);

        m_bWriteHeaderInitialized = TRUE;
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
    if (m_eAccessMode != TABRead )
    {
        SyncToDisk();
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
    VSIFCloseL(m_fp);
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
    m_bWriteEOF = FALSE;
    m_bUpdated = FALSE;

    return 0;
}

/************************************************************************/
/*                            SyncToDisk()                             */
/************************************************************************/

int TABDATFile::SyncToDisk()
{
    if( m_eAccessMode == TABRead )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SyncToDisk() can be used only with Write access.");
        return -1;
    }

    if( !m_bUpdated && m_bWriteHeaderInitialized )
        return 0;

    // No need to call. CommitRecordToFile(). It is normally called by
    // TABFeature::WriteRecordToDATFile()
    if( WriteHeader() != 0 )
        return -1;

    m_bUpdated = FALSE;
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
    if (m_eAccessMode == TABRead || m_bWriteHeaderInitialized)
        return 0;

    /*------------------------------------------------------------
     * Compute values for Record size, header size, etc.
     *-----------------------------------------------------------*/
    m_nFirstRecordPtr = (m_numFields+1)*32 + 1;

    m_nRecordSize = 1;
    for( int i = 0; i < m_numFields; i++ )
    {
        m_nRecordSize += m_pasFieldDef[i].byLength;
    }

    /*-------------------------------------------------------------
     * Create m_poRecordBlock the size of a data record.
     *------------------------------------------------------------*/
    m_nBlockSize = m_nRecordSize;

    CPLAssert( m_poRecordBlock == NULL );
    m_poRecordBlock = new TABRawBinBlock(TABReadWrite, FALSE);
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
    if (m_eAccessMode == TABRead)
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
    m_poHeaderBlock->WriteInt16((GInt16)m_nFirstRecordPtr);
    m_poHeaderBlock->WriteInt16((GInt16)m_nRecordSize);

    m_poHeaderBlock->WriteZeros(20);    // Pad rest with zeros

    /*-------------------------------------------------------------
     * Field definitions follow.  Each field def is 32 bytes.
     *------------------------------------------------------------*/
    for( int i = 0; i < m_numFields; i++ )
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
    m_bWriteEOF = FALSE;

    if (m_eAccessMode == TABRead || nRecordId <= m_numRecords)
    {
        /*-------------------------------------------------------------
         * READ ACCESS
         *------------------------------------------------------------*/
        const int nFileOffset = m_nFirstRecordPtr+(nRecordId-1)*m_nRecordSize;

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
    else if (nRecordId > 0)
    {
        /*-------------------------------------------------------------
         * WRITE ACCESS
         *------------------------------------------------------------*/

        /*-------------------------------------------------------------
         * Before writing the first record, we must generate the file
         * header.  We will also initialize class members such as record
         * size, etc. and will create m_poRecordBlock.
         *------------------------------------------------------------*/
        if (!m_bWriteHeaderInitialized)
        {
            WriteHeader();
        }

        m_bUpdated = TRUE;

        m_numRecords = std::max(nRecordId, m_numRecords);
        if( nRecordId == m_numRecords )
            m_bWriteEOF = TRUE;

        const int nFileOffset = m_nFirstRecordPtr+(nRecordId-1)*m_nRecordSize;

        m_poRecordBlock->InitNewBlock(m_fp, m_nRecordSize, nFileOffset);

        /*-------------------------------------------------------------
         * The first char of the record is the active/deleted flag.
         * Automatically set it to ' ' (active).
         *------------------------------------------------------------*/
        m_poRecordBlock->WriteByte(' ');
    }

    m_nCurRecordId = nRecordId;

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
    if (m_eAccessMode == TABRead || m_poRecordBlock == NULL)
        return -1;

    if (m_poRecordBlock->CommitToFile() != 0)
        return -1;

    /* If this is the end of file, write EOF character */
    if (m_bWriteEOF)
    {
        m_bWriteEOF = FALSE;
        char cEOF = 26;
        if (VSIFSeekL(m_fp, 0L, SEEK_END) == 0)
            VSIFWriteL(&cEOF, 1, 1, m_fp);
    }

    return 0;
}

/**********************************************************************
 *                   TABDATFile::MarkAsDeleted()
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABDATFile::MarkAsDeleted()
{
    if (m_eAccessMode == TABRead || m_poRecordBlock == NULL)
        return -1;

    const int nFileOffset = m_nFirstRecordPtr+(m_nCurRecordId-1)*m_nRecordSize;

    if (m_poRecordBlock->GotoByteInFile(nFileOffset) != 0)
        return -1;

    m_poRecordBlock->WriteByte('*');

    if (m_poRecordBlock->CommitToFile() != 0)
        return -1;

    m_bCurRecordDeletedFlag = TRUE;
    m_bUpdated = TRUE;

    return 0;
}

/**********************************************************************
 *                   TABDATFile::MarkRecordAsExisting()
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABDATFile::MarkRecordAsExisting()
{
    if (m_eAccessMode == TABRead || m_poRecordBlock == NULL)
        return -1;

    const int nFileOffset = m_nFirstRecordPtr+(m_nCurRecordId-1)*m_nRecordSize;

    if (m_poRecordBlock->GotoByteInFile(nFileOffset) != 0)
        return -1;

    m_poRecordBlock->WriteByte(' ');

    m_bCurRecordDeletedFlag = FALSE;
    m_bUpdated = TRUE;

    return 0;
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
 * For TABTableDBF, we actually have no validation to do since all types
 * are stored as strings internally, so we'll just convert from string.
 *
 * Returns a value >= 0 if OK, -1 on error.
 **********************************************************************/
int  TABDATFile::ValidateFieldInfoFromTAB(int iField, const char *pszName,
                                          TABFieldType eType,
                                          int nWidth, int nPrecision)
{
    int i = iField;  // Just to make things shorter

    if (m_pasFieldDef == NULL || iField < 0 || iField >= m_numFields)
    {
        CPLError(CE_Failure, CPLE_FileIO,
          "Invalid field %d (%s) in .TAB header. %s contains only %d fields.",
                 iField+1, pszName, m_pszFname, m_pasFieldDef? m_numFields:0);
        return -1;
    }

    /*-----------------------------------------------------------------
     * We used to check that the .TAB field name matched the .DAT
     * name stored internally, but apparently some tools that rename table
     * field names only update the .TAB file and not the .DAT, so we won't
     * do that name validation any more... we'll just check the type.
     *
     * With TABTableNative, we have to validate the field sizes as well
     * because .DAT files use char fields to store binary values.
     * With TABTableDBF, no need to validate field type since all
     * fields are stored as strings internally.
     *----------------------------------------------------------------*/
    if ((m_eTableType == TABTableNative &&
         ((eType == TABFChar && (m_pasFieldDef[i].cType != 'C' ||
                                m_pasFieldDef[i].byLength != nWidth )) ||
          (eType == TABFDecimal && (m_pasFieldDef[i].cType != 'N' ||
                                  m_pasFieldDef[i].byLength != nWidth||
                                   m_pasFieldDef[i].byDecimals!=nPrecision)) ||
          (eType == TABFInteger && (m_pasFieldDef[i].cType != 'C' ||
                                   m_pasFieldDef[i].byLength != 4  )) ||
          (eType == TABFSmallInt && (m_pasFieldDef[i].cType != 'C' ||
                                    m_pasFieldDef[i].byLength != 2 )) ||
          (eType == TABFFloat && (m_pasFieldDef[i].cType != 'C' ||
                                 m_pasFieldDef[i].byLength != 8    )) ||
          (eType == TABFDate && (m_pasFieldDef[i].cType != 'C' ||
                                m_pasFieldDef[i].byLength != 4     )) ||
          (eType == TABFTime && (m_pasFieldDef[i].cType != 'C' ||
                                 m_pasFieldDef[i].byLength != 4    )) ||
          (eType == TABFDateTime && (m_pasFieldDef[i].cType != 'C' ||
                                    m_pasFieldDef[i].byLength != 8 )) ||
          (eType == TABFLogical && (m_pasFieldDef[i].cType != 'L' ||
                                   m_pasFieldDef[i].byLength != 1  ))   ) ))
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
 *                  TABDATFileSetFieldDefinition()
 *
 **********************************************************************/
static int TABDATFileSetFieldDefinition(TABDATFieldDef* psFieldDef,
                                        const char *pszName, TABFieldType eType,
                                        int nWidth, int nPrecision)
{

    /*-----------------------------------------------------------------
     * Validate field width... must be <= 254
     *----------------------------------------------------------------*/
    if (nWidth > 254)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "Invalid size (%d) for field '%s'.  "
                 "Size must be 254 or less.", nWidth, pszName);
        return -1;
    }

    /*-----------------------------------------------------------------
     * Map fields with width=0 (variable length in OGR) to a valid default
     *----------------------------------------------------------------*/
    if (eType == TABFDecimal && nWidth == 0)
        nWidth=20;
    else if (nWidth == 0)
        nWidth=254; /* char fields */

    strncpy(psFieldDef->szName, pszName, 10);
    psFieldDef->szName[10] = '\0';
    psFieldDef->eTABType = eType;
    psFieldDef->byLength = (GByte)nWidth;
    psFieldDef->byDecimals = (GByte)nPrecision;

    switch(eType)
    {
      case TABFChar:
        psFieldDef->cType = 'C';
        break;
      case TABFDecimal:
        psFieldDef->cType = 'N';
        break;
      case TABFInteger:
        psFieldDef->cType = 'C';
        psFieldDef->byLength = 4;
        break;
      case TABFSmallInt:
        psFieldDef->cType = 'C';
        psFieldDef->byLength = 2;
        break;
      case TABFFloat:
        psFieldDef->cType = 'C';
        psFieldDef->byLength = 8;
        break;
      case TABFDate:
        psFieldDef->cType = 'C';
        psFieldDef->byLength = 4;
        break;
      case TABFTime:
        psFieldDef->cType = 'C';
        psFieldDef->byLength = 4;
        break;
      case TABFDateTime:
        psFieldDef->cType = 'C';
        psFieldDef->byLength = 8;
        break;
      case TABFLogical:
        psFieldDef->cType = 'L';
        psFieldDef->byLength = 1;
        break;
      default:
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported field type for field `%s'", pszName);
        return -1;
    }

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
    if (m_eAccessMode == TABRead || m_eTableType != TABTableNative)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Operation not supported on read-only files or on non-native table.");
        return -1;
    }

    TABDATFieldDef sFieldDef;
    if( TABDATFileSetFieldDefinition(&sFieldDef, pszName, eType,
                                     nWidth, nPrecision) < 0 )
        return -1;

    if (m_numFields < 0)
        m_numFields = 0;

    m_numFields++;
    m_pasFieldDef = (TABDATFieldDef*)CPLRealloc(m_pasFieldDef,
                                          m_numFields*sizeof(TABDATFieldDef));
    memcpy(&m_pasFieldDef[m_numFields-1], &sFieldDef, sizeof(sFieldDef));

    /* If there are already records, we cannot update in place */
    /* so create a temporary .dat.tmp in which we create the new structure */
    /* and then copy the widen records */
    if( m_numRecords > 0 )
    {
        TABDATFile oTempFile;
        CPLString osOriginalFile(m_pszFname);
        CPLString osTmpFile(m_pszFname);
        osTmpFile += ".tmp";
        if( oTempFile.Open( osTmpFile.c_str(), TABWrite ) != 0 )
            return -1;

        /* Create field structure */
        for( int i = 0; i < m_numFields; i++ )
        {
            oTempFile.AddField(m_pasFieldDef[i].szName,
                               m_pasFieldDef[i].eTABType,
                               m_pasFieldDef[i].byLength,
                               m_pasFieldDef[i].byDecimals);
        }

        GByte* pabyRecord = (GByte*)CPLMalloc(m_nRecordSize);

        /* Copy records */
        for(int j = 0; j < m_numRecords; j++)
        {
            if( GetRecordBlock(1+j) == NULL ||
                oTempFile.GetRecordBlock(1+j) == NULL )
            {
                CPLFree(pabyRecord);
                oTempFile.Close();
                VSIUnlink(osTmpFile);
                return -1;
            }
            if (m_bCurRecordDeletedFlag)
            {
                oTempFile.MarkAsDeleted();
            }
            else
            {
                if( m_poRecordBlock->ReadBytes(m_nRecordSize-1, pabyRecord) != 0 ||
                    oTempFile.m_poRecordBlock->WriteBytes(m_nRecordSize-1, pabyRecord) != 0 ||
                    oTempFile.m_poRecordBlock->WriteZeros(m_pasFieldDef[m_numFields-1].byLength) != 0 )
                {
                    CPLFree(pabyRecord);
                    oTempFile.Close();
                    VSIUnlink(osTmpFile);
                    return -1;
                }
                oTempFile.CommitRecordToFile();
            }
        }

        CPLFree(pabyRecord);

        /* Close temporary file */
        oTempFile.Close();

        /* Backup field definitions as we will need to set the TABFieldType */
        TABDATFieldDef* pasFieldDefTmp = (TABDATFieldDef*)CPLMalloc(m_numFields*sizeof(TABDATFieldDef));
        memcpy(pasFieldDefTmp, m_pasFieldDef, m_numFields*sizeof(TABDATFieldDef));

        /* Close ourselves */
        m_numFields--; /* so that Close() doesn't see the new field */
        Close();

        /* Move temporary file as main .data file and reopen it */
        VSIUnlink(osOriginalFile);
        VSIRename(osTmpFile, osOriginalFile);
        if( Open( osOriginalFile, TABReadWrite ) < 0 )
        {
            CPLFree(pasFieldDefTmp);
            return -1;
        }

        /* Restore saved TABFieldType */
        for( int i = 0; i < m_numFields; i++ )
        {
            m_pasFieldDef[i].eTABType = pasFieldDefTmp[i].eTABType;
        }
        CPLFree(pasFieldDefTmp);
    }

    return 0;
}

/************************************************************************/
/*                            DeleteField()                             */
/************************************************************************/

int TABDATFile::DeleteField( int iField )
{
    if (m_eAccessMode == TABRead || m_eTableType != TABTableNative)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Operation not supported on read-only files or on non-native table.");
        return -1;
    }

    if( iField < 0 || iField >= m_numFields )
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "Invalid field index: %d", iField);
        return -1;
    }

    /* If no records have been written, then just remove from the field */
    /* definition array */
    if( m_numRecords <= 0 )
    {
        if( iField < m_numFields-1 )
        {
            memmove(m_pasFieldDef + iField, m_pasFieldDef + iField + 1,
                    (m_numFields-1-iField)*sizeof(TABDATFieldDef));
        }
        m_numFields --;
        return 0;
    }

    if( m_numFields == 1 )
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "Cannot delete the single remaining field.");
        return -1;
    }

    /* Otherwise we need to do a temporary file */
    TABDATFile oTempFile;
    CPLString osOriginalFile(m_pszFname);
    CPLString osTmpFile(m_pszFname);
    osTmpFile += ".tmp";
    if( oTempFile.Open( osTmpFile.c_str(), TABWrite ) != 0 )
        return -1;

    /* Create field structure */
    int nRecordSizeBefore = 0;
    int nRecordSizeAfter = 0;
    for( int i = 0; i < m_numFields; i++ )
    {
        if( i != iField )
        {
            if( i < iField ) nRecordSizeBefore += m_pasFieldDef[i].byLength;
            else if( i > iField ) nRecordSizeAfter += m_pasFieldDef[i].byLength;
            oTempFile.AddField(m_pasFieldDef[i].szName,
                                m_pasFieldDef[i].eTABType,
                                m_pasFieldDef[i].byLength,
                                m_pasFieldDef[i].byDecimals);
        }
    }

    CPLAssert(nRecordSizeBefore + m_pasFieldDef[iField].byLength + nRecordSizeAfter == m_nRecordSize - 1);

    GByte* pabyRecord = (GByte*)CPLMalloc(m_nRecordSize);

    /* Copy records */
    for(int j = 0; j < m_numRecords; j++)
    {
        if( GetRecordBlock(1+j) == NULL ||
            oTempFile.GetRecordBlock(1+j) == NULL )
        {
            CPLFree(pabyRecord);
            oTempFile.Close();
            VSIUnlink(osTmpFile);
            return -1;
        }
        if (m_bCurRecordDeletedFlag)
        {
            oTempFile.MarkAsDeleted();
        }
        else
        {
            if( m_poRecordBlock->ReadBytes(m_nRecordSize-1, pabyRecord) != 0 ||
                (nRecordSizeBefore > 0 && oTempFile.m_poRecordBlock->WriteBytes(nRecordSizeBefore, pabyRecord) != 0) ||
                (nRecordSizeAfter > 0 && oTempFile.m_poRecordBlock->WriteBytes(nRecordSizeAfter,
                    pabyRecord + nRecordSizeBefore + m_pasFieldDef[iField].byLength) != 0) )
            {
                CPLFree(pabyRecord);
                oTempFile.Close();
                VSIUnlink(osTmpFile);
                return -1;
            }
            oTempFile.CommitRecordToFile();
        }
    }

    CPLFree(pabyRecord);

    /* Close temporary file */
    oTempFile.Close();

    /* Backup field definitions as we will need to set the TABFieldType */
    TABDATFieldDef* pasFieldDefTmp = (TABDATFieldDef*)CPLMalloc(m_numFields*sizeof(TABDATFieldDef));
    memcpy(pasFieldDefTmp, m_pasFieldDef, m_numFields*sizeof(TABDATFieldDef));

    /* Close ourselves */
    Close();

    /* Move temporary file as main .data file and reopen it */
    VSIUnlink(osOriginalFile);
    VSIRename(osTmpFile, osOriginalFile);
    if( Open( osOriginalFile, TABReadWrite ) < 0 )
    {
        CPLFree(pasFieldDefTmp);
        return -1;
    }

    /* Restore saved TABFieldType */
    for( int i = 0; i < m_numFields; i++ )
    {
        if( i < iField )
            m_pasFieldDef[i].eTABType = pasFieldDefTmp[i].eTABType;
        else
            m_pasFieldDef[i].eTABType = pasFieldDefTmp[i+1].eTABType;
    }
    CPLFree(pasFieldDefTmp);

    return 0;
}

/************************************************************************/
/*                           ReorderFields()                            */
/************************************************************************/

int TABDATFile::ReorderFields( int* panMap )
{
    if (m_eAccessMode == TABRead || m_eTableType != TABTableNative)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Operation not supported on read-only files or on non-native table.");
        return -1;
    }

    if( m_numFields == 0)
        return 0;

    OGRErr eErr = OGRCheckPermutation(panMap, m_numFields);
    if (eErr != OGRERR_NONE)
        return -1;

    /* If no records have been written, then just reorder the field */
    /* definition array */
    if( m_numRecords <= 0 )
    {
        TABDATFieldDef* pasFieldDefTmp = (TABDATFieldDef*)CPLMalloc(m_numFields*sizeof(TABDATFieldDef));
        memcpy(pasFieldDefTmp, m_pasFieldDef, m_numFields*sizeof(TABDATFieldDef));
        for(int i = 0; i < m_numFields; i++)
        {
            memcpy(m_pasFieldDef + i, pasFieldDefTmp + panMap[i],
                   sizeof(TABDATFieldDef));
        }
        CPLFree(pasFieldDefTmp);
        return 0;
    }

    // We could theoretically update in place, but a sudden interruption
    // would leave the file in a undefined state.

    TABDATFile oTempFile;
    CPLString osOriginalFile(m_pszFname);
    CPLString osTmpFile(m_pszFname);
    osTmpFile += ".tmp";
    if( oTempFile.Open( osTmpFile.c_str(), TABWrite ) != 0 )
        return -1;

    /* Create field structure */
    int* panOldOffset = (int*)CPLMalloc(m_numFields * sizeof(int));
    for( int i = 0; i < m_numFields; i++ )
    {
        int iBefore = panMap[i];
        if( i == 0 )
            panOldOffset[i] = 0;
        else
            panOldOffset[i] = panOldOffset[i-1] + m_pasFieldDef[i-1].byLength;
        oTempFile.AddField(m_pasFieldDef[iBefore].szName,
                            m_pasFieldDef[iBefore].eTABType,
                            m_pasFieldDef[iBefore].byLength,
                            m_pasFieldDef[iBefore].byDecimals);
    }

    GByte* pabyRecord = (GByte*)CPLMalloc(m_nRecordSize);

    /* Copy records */
    for(int j = 0; j < m_numRecords; j++)
    {
        if( GetRecordBlock(1+j) == NULL ||
            oTempFile.GetRecordBlock(1+j) == NULL )
        {
            CPLFree(pabyRecord);
            CPLFree(panOldOffset);
            oTempFile.Close();
            VSIUnlink(osTmpFile);
            return -1;
        }
        if (m_bCurRecordDeletedFlag)
        {
            oTempFile.MarkAsDeleted();
        }
        else
        {
            if( m_poRecordBlock->ReadBytes(m_nRecordSize-1, pabyRecord) != 0 )
            {
                CPLFree(pabyRecord);
                CPLFree(panOldOffset);
                oTempFile.Close();
                VSIUnlink(osTmpFile);
                return -1;
            }
            for( int i = 0; i < m_numFields; i++ )
            {
                int iBefore = panMap[i];
                if( oTempFile.m_poRecordBlock->WriteBytes(
                        m_pasFieldDef[iBefore].byLength, pabyRecord + panOldOffset[iBefore]) != 0 )
                {
                    CPLFree(pabyRecord);
                    CPLFree(panOldOffset);
                    oTempFile.Close();
                    VSIUnlink(osTmpFile);
                    return -1;
                }
            }

            oTempFile.CommitRecordToFile();
        }
    }

    CPLFree(pabyRecord);
    CPLFree(panOldOffset);

    /* Close temporary file */
    oTempFile.Close();

    /* Backup field definitions as we will need to set the TABFieldType */
    TABDATFieldDef* pasFieldDefTmp = (TABDATFieldDef*)CPLMalloc(m_numFields*sizeof(TABDATFieldDef));
    memcpy(pasFieldDefTmp, m_pasFieldDef, m_numFields*sizeof(TABDATFieldDef));

    /* Close ourselves */
    Close();

    /* Move temporary file as main .data file and reopen it */
    VSIUnlink(osOriginalFile);
    VSIRename(osTmpFile, osOriginalFile);
    if( Open( osOriginalFile, TABReadWrite ) < 0 )
    {
        CPLFree(pasFieldDefTmp);
        return -1;
    }

    /* Restore saved TABFieldType */
    for( int i = 0; i < m_numFields; i++ )
    {
        int iBefore = panMap[i];
        m_pasFieldDef[i].eTABType = pasFieldDefTmp[iBefore].eTABType;
    }
    CPLFree(pasFieldDefTmp);

    return 0;
}

/************************************************************************/
/*                           AlterFieldDefn()                           */
/************************************************************************/

int TABDATFile::AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlags )
{
    if (m_eAccessMode == TABRead || m_eTableType != TABTableNative)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Operation not supported on read-only files or on non-native table.");
        return -1;
    }

    if( iField < 0 || iField >= m_numFields )
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "Invalid field index: %d", iField);
        return -1;
    }

    TABFieldType eTABType = m_pasFieldDef[iField].eTABType;
    int nWidth = m_pasFieldDef[iField].byLength ;
    int nPrecision = m_pasFieldDef[iField].byDecimals ;
    int nWidthDummy = 0;
    int nPrecisionDummy = 0;
    if( nFlags & ALTER_TYPE_FLAG )
    {
        if( IMapInfoFile::GetTABType( poNewFieldDefn, &eTABType, &nWidthDummy, &nPrecisionDummy ) < 0 )
            return -1;
    }
    if( (nFlags & ALTER_WIDTH_PRECISION_FLAG) )
    {
        TABFieldType eTABTypeDummy;
        if( IMapInfoFile::GetTABType( poNewFieldDefn, &eTABTypeDummy, &nWidth, &nPrecision ) < 0 )
            return -1;
    }

    if ((nFlags & ALTER_TYPE_FLAG) &&
        eTABType != m_pasFieldDef[iField].eTABType)
    {
        if ( eTABType != TABFChar )
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "Can only convert to OFTString");
            return -1;
        }
        if( (nFlags & ALTER_WIDTH_PRECISION_FLAG) == 0 )
            nWidth = 254;
    }

    if (nFlags & ALTER_WIDTH_PRECISION_FLAG)
    {
        if( eTABType != TABFChar && nWidth != m_pasFieldDef[iField].byLength )
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "Resizing only supported on String fields");
            return -1;
        }
    }

    if (nFlags & ALTER_NAME_FLAG)
    {
        strncpy(m_pasFieldDef[iField].szName, poNewFieldDefn->GetNameRef(), 10);
        m_pasFieldDef[iField].szName[10] = '\0';
        /* If renaming is the only operation, then nothing more to do */
        if( nFlags == ALTER_NAME_FLAG )
        {
            m_bUpdated = TRUE;
            return 0;
        }
    }

    if( m_numRecords <= 0)
    {
        if( (nFlags & ALTER_TYPE_FLAG) &&
            eTABType != m_pasFieldDef[iField].eTABType)
        {
            TABDATFieldDef sFieldDef;
            TABDATFileSetFieldDefinition(&sFieldDef,
                                         m_pasFieldDef[iField].szName, eTABType,
                                         m_pasFieldDef[iField].byLength,
                                         m_pasFieldDef[iField].byDecimals);
            memcpy(&m_pasFieldDef[iField], &sFieldDef, sizeof(sFieldDef));
        }
        if (nFlags & ALTER_WIDTH_PRECISION_FLAG)
        {
            m_pasFieldDef[iField].byLength = (GByte)nWidth;
            m_pasFieldDef[iField].byDecimals = (GByte)nPrecision;
        }
        return 0;
    }

    /* Otherwise we need to do a temporary file */
    TABDATFile oTempFile;
    CPLString osOriginalFile(m_pszFname);
    CPLString osTmpFile(m_pszFname);
    osTmpFile += ".tmp";
    if( oTempFile.Open( osTmpFile.c_str(), TABWrite ) != 0 )
        return -1;

    /* Create field structure */
    int nRecordSizeBefore = 0;
    int nRecordSizeAfter = 0;
    TABDATFieldDef sFieldDef;
    TABDATFileSetFieldDefinition(&sFieldDef,
                                 m_pasFieldDef[iField].szName,
                                 eTABType,
                                 nWidth,
                                 nPrecision);

    for( int i = 0; i < m_numFields; i++ )
    {
        if( i != iField )
        {
            if( i < iField ) nRecordSizeBefore += m_pasFieldDef[i].byLength;
            else if( i > iField ) nRecordSizeAfter += m_pasFieldDef[i].byLength;
            oTempFile.AddField(m_pasFieldDef[i].szName,
                                m_pasFieldDef[i].eTABType,
                                m_pasFieldDef[i].byLength,
                                m_pasFieldDef[i].byDecimals);
        }
        else
        {
            oTempFile.AddField(sFieldDef.szName,
                               sFieldDef.eTABType,
                               sFieldDef.byLength,
                               sFieldDef.byDecimals);
        }
    }

    GByte* pabyRecord = (GByte*)CPLMalloc(m_nRecordSize);
    char* pabyNewField = (char*)CPLMalloc(sFieldDef.byLength + 1);

    /* Copy records */
    for(int j = 0; j < m_numRecords; j++)
    {
        if( GetRecordBlock(1+j) == NULL ||
            oTempFile.GetRecordBlock(1+j) == NULL )
        {
            CPLFree(pabyRecord);
            CPLFree(pabyNewField);
            oTempFile.Close();
            VSIUnlink(osTmpFile);
            return -1;
        }
        if (m_bCurRecordDeletedFlag)
        {
            oTempFile.MarkAsDeleted();
        }
        else
        {
            if( nRecordSizeBefore > 0 &&
                (m_poRecordBlock->ReadBytes(nRecordSizeBefore, pabyRecord) != 0 ||
                 oTempFile.m_poRecordBlock->WriteBytes(nRecordSizeBefore, pabyRecord) != 0) )
            {
                CPLFree(pabyRecord);
                CPLFree(pabyNewField);
                oTempFile.Close();
                VSIUnlink(osTmpFile);
                return -1;
            }

            memset(pabyNewField, 0, sFieldDef.byLength + 1);
            if( m_pasFieldDef[iField].eTABType == TABFChar )
            {
                strncpy(pabyNewField, ReadCharField(m_pasFieldDef[iField].byLength), sFieldDef.byLength);
            }
            else if( m_pasFieldDef[iField].eTABType == TABFInteger )
            {
                snprintf(pabyNewField, sFieldDef.byLength, "%d", ReadIntegerField(m_pasFieldDef[iField].byLength));
            }
            else if( m_pasFieldDef[iField].eTABType == TABFSmallInt )
            {
                snprintf(pabyNewField, sFieldDef.byLength, "%d", ReadSmallIntField(m_pasFieldDef[iField].byLength));
            }
            else if( m_pasFieldDef[iField].eTABType == TABFFloat )
            {
                CPLsnprintf(pabyNewField, sFieldDef.byLength, "%.18f", ReadFloatField(m_pasFieldDef[iField].byLength));
            }
            else if( m_pasFieldDef[iField].eTABType == TABFDecimal )
            {
                CPLsnprintf(pabyNewField, sFieldDef.byLength, "%.18f", ReadFloatField(m_pasFieldDef[iField].byLength));
            }
            else if( m_pasFieldDef[iField].eTABType == TABFLogical )
            {
                strncpy(pabyNewField, ReadLogicalField(m_pasFieldDef[iField].byLength), sFieldDef.byLength);
            }
            else if( m_pasFieldDef[iField].eTABType == TABFDate )
            {
                strncpy(pabyNewField, ReadDateField(m_pasFieldDef[iField].byLength), sFieldDef.byLength);
            }
            else if( m_pasFieldDef[iField].eTABType == TABFTime )
            {
                strncpy(pabyNewField, ReadTimeField(m_pasFieldDef[iField].byLength), sFieldDef.byLength);
            }
            else if( m_pasFieldDef[iField].eTABType == TABFDateTime )
            {
                strncpy(pabyNewField, ReadDateTimeField(m_pasFieldDef[iField].byLength), sFieldDef.byLength);
            }

            if( oTempFile.m_poRecordBlock->WriteBytes(sFieldDef.byLength, (GByte*)pabyNewField) != 0 ||
                (nRecordSizeAfter > 0 &&
                (m_poRecordBlock->ReadBytes(nRecordSizeAfter, pabyRecord) != 0 ||
                 oTempFile.m_poRecordBlock->WriteBytes(nRecordSizeAfter, pabyRecord) != 0)) )
            {
                CPLFree(pabyRecord);
                CPLFree(pabyNewField);
                oTempFile.Close();
                VSIUnlink(osTmpFile);
                return -1;
            }
            oTempFile.CommitRecordToFile();
        }
    }

    CPLFree(pabyRecord);
    CPLFree(pabyNewField);

    /* Close temporary file */
    oTempFile.Close();

    /* Backup field definitions as we will need to set the TABFieldType */
    TABDATFieldDef* pasFieldDefTmp = (TABDATFieldDef*)CPLMalloc(m_numFields*sizeof(TABDATFieldDef));
    memcpy(pasFieldDefTmp, m_pasFieldDef, m_numFields*sizeof(TABDATFieldDef));

    /* Close ourselves */
    Close();

    /* Move temporary file as main .data file and reopen it */
    VSIUnlink(osOriginalFile);
    VSIRename(osTmpFile, osOriginalFile);
    if( Open( osOriginalFile, TABReadWrite ) < 0 )
    {
        CPLFree(pasFieldDefTmp);
        return -1;
    }

    /* Restore saved TABFieldType */
    for( int i = 0; i < m_numFields; i++ )
    {
        if( i != iField )
            m_pasFieldDef[i].eTABType = pasFieldDefTmp[i].eTABType;
        else
            m_pasFieldDef[i].eTABType = eTABType;
    }
    CPLFree(pasFieldDefTmp);

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

    if (m_poRecordBlock->ReadBytes(nWidth, (GByte*)m_szBuffer) != 0)
        return "";

    m_szBuffer[nWidth] = '\0';

    // NATIVE tables are padded with '\0' chars, but DBF tables are padded
    // with spaces... get rid of the trailing spaces.
    if (m_eTableType == TABTableDBF)
    {
        int nLen = static_cast<int>(strlen(m_szBuffer))-1;
        while(nLen>=0 && m_szBuffer[nLen] == ' ')
            m_szBuffer[nLen--] = '\0';
    }

    return m_szBuffer;
}

/**********************************************************************
 *                   TABDATFile::ReadIntegerField()
 *
 * Read the integer field value at the current position in the data
 * block.
 *
 * Note: nWidth is used only with TABTableDBF types.
 *
 * CPLError() will have been called if something fails.
 **********************************************************************/
GInt32 TABDATFile::ReadIntegerField(int nWidth)
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

    if (m_eTableType == TABTableDBF)
        return atoi(ReadCharField(nWidth));

    return m_poRecordBlock->ReadInt32();
}

/**********************************************************************
 *                   TABDATFile::ReadSmallIntField()
 *
 * Read the smallint field value at the current position in the data
 * block.
 *
 * Note: nWidth is used only with TABTableDBF types.
 *
 * CPLError() will have been called if something fails.
 **********************************************************************/
GInt16 TABDATFile::ReadSmallIntField(int nWidth)
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

    if (m_eTableType == TABTableDBF)
        return (GInt16)atoi(ReadCharField(nWidth));

    return m_poRecordBlock->ReadInt16();
}

/**********************************************************************
 *                   TABDATFile::ReadFloatField()
 *
 * Read the float field value at the current position in the data
 * block.
 *
 * Note: nWidth is used only with TABTableDBF types.
 *
 * CPLError() will have been called if something fails.
 **********************************************************************/
double TABDATFile::ReadFloatField(int nWidth)
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

    if (m_eTableType == TABTableDBF)
        return CPLAtof(ReadCharField(nWidth));

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
 * Note: nWidth is used only with TABTableDBF types.
 *
 * CPLError() will have been called if something fails.
 **********************************************************************/
const char *TABDATFile::ReadLogicalField( int nWidth )
{
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

    bool bValue = false;
    if (m_eTableType == TABTableDBF)
    {
        const char *pszVal = ReadCharField(nWidth);
        bValue = pszVal && strchr("1YyTt", pszVal[0]) != NULL;
    }
    else
    {
        // In Native tables, we are guaranteed it is 1 byte with 0/1 value
        bValue = CPL_TO_BOOL(m_poRecordBlock->ReadByte());
    }

    return bValue ? "T" : "F";
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
 * We return an 8 chars string in the format "YYYYMMDD"
 *
 * Note: nWidth is used only with TABTableDBF types.
 *
 * Returns a reference to an internal buffer that will be valid only until
 * the next field is read, or "" if the operation failed, in which case
 * CPLError() will have been called.
 **********************************************************************/
const char *TABDATFile::ReadDateField(int nWidth)
{
    int nDay = 0;
    int nMonth = 0;
    int nYear = 0;
    int status = 0;

    if ((status = ReadDateField(nWidth, &nYear, &nMonth, &nDay)) == -1)
       return "";

    snprintf(m_szBuffer, sizeof(m_szBuffer), "%4.4d%2.2d%2.2d", nYear, nMonth, nDay);

    return m_szBuffer;
}

int TABDATFile::ReadDateField(int nWidth, int *nYear, int *nMonth, int *nDay)
{
    // If current record has been deleted, then return an acceptable
    // default value.
    if (m_bCurRecordDeletedFlag)
        return -1;

    if (m_poRecordBlock == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "Can't read field value: file is not opened.");
        return -1;
    }

    // With .DBF files, the value should already be
    // stored in YYYYMMDD format according to DBF specs.
    if (m_eTableType == TABTableDBF)
    {
       strcpy(m_szBuffer,ReadCharField(nWidth));
       sscanf(m_szBuffer, "%4d%2d%2d", nYear, nMonth, nDay);
    }
    else
    {
       *nYear  = m_poRecordBlock->ReadInt16();
       *nMonth = m_poRecordBlock->ReadByte();
       *nDay   = m_poRecordBlock->ReadByte();
    }

    if (CPLGetLastErrorNo() != 0 || (*nYear==0 && *nMonth==0 && *nDay==0))
       return -1;

    return 0;
}

/**********************************************************************
 *                   TABDATFile::ReadTimeField()
 *
 * Read the Time field value at the current position in the data
 * block.
 *
 * A time field is a 4 bytes binary value which represents the number
 * of milliseconds since midnight.
 *
 * We return a 9 char string in the format "HHMMSSMMM"
 *
 * Note: nWidth is used only with TABTableDBF types.
 *
 * Returns a reference to an internal buffer that will be valid only until
 * the next field is read, or "" if the operation failed, in which case
 * CPLError() will have been called.
 **********************************************************************/
const char *TABDATFile::ReadTimeField(int nWidth)
{
    int nHour = 0;
    int nMinute = 0;
    int nSecond = 0;
    int nMS = 0;
    int status = 0;

    if ((status = ReadTimeField(nWidth, &nHour, &nMinute, &nSecond, &nMS)) == -1)
       return "";

    snprintf(m_szBuffer, sizeof(m_szBuffer), "%2.2d%2.2d%2.2d%3.3d", nHour, nMinute, nSecond, nMS);

    return m_szBuffer;
}

int TABDATFile::ReadTimeField(int nWidth, int *nHour, int *nMinute,
                              int *nSecond, int *nMS)
{
    GInt32 nS = 0;
    // If current record has been deleted, then return an acceptable
    // default value.
    if (m_bCurRecordDeletedFlag)
        return -1;

    if (m_poRecordBlock == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "Can't read field value: file is not opened.");
        return -1;
    }

    // With .DBF files, the value should already be stored in
    // HHMMSSMMM format according to DBF specs.
    if (m_eTableType == TABTableDBF)
    {
       strcpy(m_szBuffer,ReadCharField(nWidth));
       sscanf(m_szBuffer,"%2d%2d%2d%3d",
              nHour, nMinute, nSecond, nMS);
    }
    else
    {
       nS  = m_poRecordBlock->ReadInt32(); // Convert time from ms to sec
    }

    // nS is set to -1 when the value is 'not set'
    if (CPLGetLastErrorNo() != 0 || nS < 0 || (nS>86400000))
       return -1;

    *nHour = int(nS/3600000);
    *nMinute  = int((nS/1000 - *nHour*3600)/60);
    *nSecond  = int(nS/1000 - *nHour*3600 - *nMinute*60);
    *nMS   = int(nS-*nHour*3600000-*nMinute*60000-*nSecond*1000);

    return 0;
}

/**********************************************************************
 *                   TABDATFile::ReadDateTimeField()
 *
 * Read the DateTime field value at the current position in the data
 * block.
 *
 * A datetime field is an 8 bytes binary value in which the first byte is
 * the day, followed by 1 byte for the month, and 2 bytes for the year. After
 * this is 4 bytes which represents the number of milliseconds since midnight.
 *
 * We return an 17 chars string in the format "YYYYMMDDhhmmssmmm"
 *
 * Note: nWidth is used only with TABTableDBF types.
 *
 * Returns a reference to an internal buffer that will be valid only until
 * the next field is read, or "" if the operation failed, in which case
 * CPLError() will have been called.
 **********************************************************************/
const char *TABDATFile::ReadDateTimeField(int nWidth)
{
    int nDay = 0;
    int nMonth = 0;
    int nYear = 0;
    int nHour = 0;
    int nMinute = 0;
    int nSecond = 0;
    int nMS = 0;
    int status = 0;

    if ((status = ReadDateTimeField(nWidth, &nYear, &nMonth, &nDay, &nHour,
                                    &nMinute, &nSecond, &nMS)) == -1)
       return "";

    snprintf(m_szBuffer, sizeof(m_szBuffer), "%4.4d%2.2d%2.2d%2.2d%2.2d%2.2d%3.3d",
            nYear, nMonth, nDay, nHour, nMinute, nSecond, nMS);

    return m_szBuffer;
}

int TABDATFile::ReadDateTimeField(int nWidth, int *nYear, int *nMonth, int *nDay,
                                 int *nHour, int *nMinute, int *nSecond, int *nMS)
{
    GInt32 nS = 0;
    // If current record has been deleted, then return an acceptable
    // default value.
    if (m_bCurRecordDeletedFlag)
        return -1;

    if (m_poRecordBlock == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "Can't read field value: file is not opened.");
        return -1;
    }

    // With .DBF files, the value should already be stored in
    // YYYYMMDD format according to DBF specs.
    if (m_eTableType == TABTableDBF)
    {
       strcpy(m_szBuffer,ReadCharField(nWidth));
       sscanf(m_szBuffer, "%4d%2d%2d%2d%2d%2d%3d",
              nYear, nMonth, nDay, nHour, nMinute, nSecond, nMS);
    }
    else
    {
       *nYear  = m_poRecordBlock->ReadInt16();
       *nMonth = m_poRecordBlock->ReadByte();
       *nDay   = m_poRecordBlock->ReadByte();
       nS      = m_poRecordBlock->ReadInt32();
    }

    if (CPLGetLastErrorNo() != 0 ||
        (*nYear==0 && *nMonth==0 && *nDay==0) || (nS>86400000))
        return -1;

    *nHour = int(nS/3600000);
    *nMinute  = int((nS/1000 - *nHour*3600)/60);
    *nSecond  = int(nS/1000 - *nHour*3600 - *nMinute*60);
    *nMS   = int(nS-*nHour*3600000-*nMinute*60000-*nSecond*1000);

    return 0;
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
    // If current record has been deleted, then return an acceptable
    // default value.
    if (m_bCurRecordDeletedFlag)
        return 0.0;

    const char *pszVal = ReadCharField(nWidth);

    return CPLAtof(pszVal);
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
int TABDATFile::WriteCharField(const char *pszStr, int nWidth,
                               TABINDFile *poINDFile, int nIndexNo)
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
    const int nLen = std::min(static_cast<int>(strlen(pszStr)), nWidth);

    if ((nLen>0 && m_poRecordBlock->WriteBytes(nLen, (GByte*)pszStr) != 0) ||
        (nWidth-nLen > 0 && m_poRecordBlock->WriteZeros(nWidth-nLen)!=0) )
        return -1;

    // Update Index
    if (poINDFile && nIndexNo > 0)
    {
        GByte *pKey = poINDFile->BuildKey(nIndexNo, pszStr);
        if (poINDFile->AddEntry(nIndexNo, pKey, m_nCurRecordId) != 0)
            return -1;
    }

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
int TABDATFile::WriteIntegerField(GInt32 nValue,
                                  TABINDFile *poINDFile, int nIndexNo)
{
    if (m_poRecordBlock == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
            "Can't write field value: GetRecordBlock() has not been called.");
        return -1;
    }

    // Update Index
    if (poINDFile && nIndexNo > 0)
    {
        GByte *pKey = poINDFile->BuildKey(nIndexNo, nValue);
        if (poINDFile->AddEntry(nIndexNo, pKey, m_nCurRecordId) != 0)
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
int TABDATFile::WriteSmallIntField(GInt16 nValue,
                                   TABINDFile *poINDFile, int nIndexNo)
{
    if (m_poRecordBlock == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
            "Can't write field value: GetRecordBlock() has not been called.");
        return -1;
    }

    // Update Index
    if (poINDFile && nIndexNo > 0)
    {
        GByte *pKey = poINDFile->BuildKey(nIndexNo, nValue);
        if (poINDFile->AddEntry(nIndexNo, pKey, m_nCurRecordId) != 0)
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
int TABDATFile::WriteFloatField( double dValue,
                                 TABINDFile *poINDFile, int nIndexNo )
{
    if (m_poRecordBlock == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
            "Can't write field value: GetRecordBlock() has not been called.");
        return -1;
    }

    // Update Index
    if (poINDFile && nIndexNo > 0)
    {
        GByte *pKey = poINDFile->BuildKey(nIndexNo, dValue);
        if (poINDFile->AddEntry(nIndexNo, pKey, m_nCurRecordId) != 0)
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
int TABDATFile::WriteLogicalField(const char *pszValue,
                                  TABINDFile *poINDFile, int nIndexNo)
{
    if (m_poRecordBlock == NULL)
    {
        CPLError(
            CE_Failure, CPLE_AssertionFailed,
            "Can't write field value: GetRecordBlock() has not been called.");
        return -1;
    }

    // TODO(schwehr): bValue should be a bool.
    const GByte bValue = STARTS_WITH_CI(pszValue, "T") ? 1 : 0;

    // Update Index
    if (poINDFile && nIndexNo > 0)
    {
        GByte *pKey = poINDFile->BuildKey(nIndexNo, (int)bValue);
        if (poINDFile->AddEntry(nIndexNo, pKey, m_nCurRecordId) != 0)
            return -1;
    }

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
 * The expected input is a 10 chars string in the format "YYYY/MM/DD"
 * or "DD/MM/YYYY" or "YYYYMMDD"
 *
 * Returns 0 on success, or -1 if the operation failed, in which case
 * CPLError() will have been called.
 **********************************************************************/
int TABDATFile::WriteDateField(const char *pszValue,
                               TABINDFile *poINDFile, int nIndexNo)
{
    char **papszTok = NULL;

    /*-----------------------------------------------------------------
     * Get rid of leading spaces.
     *----------------------------------------------------------------*/
    while ( *pszValue == ' ' ) { pszValue++; }

    /*-----------------------------------------------------------------
     * Try to automagically detect date format, one of:
     * "YYYY/MM/DD", "DD/MM/YYYY", or "YYYYMMDD"
     *----------------------------------------------------------------*/
    int nDay = 0;
    int nMonth = 0;
    int nYear = 0;

    if (strlen(pszValue) == 8)
    {
        /*-------------------------------------------------------------
         * "YYYYMMDD"
         *------------------------------------------------------------*/
        char szBuf[9];
        strcpy(szBuf, pszValue);
        nDay = atoi(szBuf+6);
        szBuf[6] = '\0';
        nMonth = atoi(szBuf+4);
        szBuf[4] = '\0';
        nYear = atoi(szBuf);
    }
    else if (strlen(pszValue) == 10 &&
             (papszTok = CSLTokenizeStringComplex(pszValue, "/",
                                                  FALSE, FALSE)) != NULL &&
             CSLCount(papszTok) == 3 &&
             (strlen(papszTok[0]) == 4 || strlen(papszTok[2]) == 4) )
    {
        /*-------------------------------------------------------------
         * Either "YYYY/MM/DD" or "DD/MM/YYYY"
         *------------------------------------------------------------*/
        if (strlen(papszTok[0]) == 4)
        {
            nYear = atoi(papszTok[0]);
            nMonth = atoi(papszTok[1]);
            nDay = atoi(papszTok[2]);
        }
        else
        {
            nYear = atoi(papszTok[2]);
            nMonth = atoi(papszTok[1]);
            nDay = atoi(papszTok[0]);
        }
    }
    else if (strlen(pszValue) == 0)
    {
        nYear = 0;
        nMonth = 0;
        nDay = 0;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid date field value `%s'.  Date field values must "
                 "be in the format `YYYY/MM/DD', `MM/DD/YYYY' or `YYYYMMDD'",
                 pszValue);
        CSLDestroy(papszTok);
        return -1;
    }
    CSLDestroy(papszTok);

    return WriteDateField(nYear, nMonth, nDay, poINDFile, nIndexNo);
}

int TABDATFile::WriteDateField(int nYear, int nMonth, int nDay,
                               TABINDFile *poINDFile, int nIndexNo)
{
    if (m_poRecordBlock == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "Can't write field value: GetRecordBlock() has not been called.");
        return -1;
    }

    m_poRecordBlock->WriteInt16((GInt16)nYear);
    m_poRecordBlock->WriteByte((GByte)nMonth);
    m_poRecordBlock->WriteByte((GByte)nDay);

    if (CPLGetLastErrorNo() != 0)
        return -1;

    // Update Index
    if (poINDFile && nIndexNo > 0)
    {
        GByte *pKey = poINDFile->BuildKey(nIndexNo, (nYear*0x10000 +
                                                     nMonth * 0x100 + nDay));
        if (poINDFile->AddEntry(nIndexNo, pKey, m_nCurRecordId) != 0)
            return -1;
    }

    return 0;
}

/**********************************************************************
 *                   TABDATFile::WriteTimeField()
 *
 * Write the date field value at the current position in the data
 * block.
 *
 * A time field is a 4 byte binary value which represents the number
 * of milliseconds since midnight.
 *
 * The expected input is a 10 chars string in the format "HH:MM:SS"
 * or "HHMMSSmmm"
 *
 * Returns 0 on success, or -1 if the operation failed, in which case
 * CPLError() will have been called.
 **********************************************************************/
int TABDATFile::WriteTimeField(const char *pszValue,
                               TABINDFile *poINDFile, int nIndexNo)
{
    /*-----------------------------------------------------------------
     * Get rid of leading spaces.
     *----------------------------------------------------------------*/
    while ( *pszValue == ' ' ) { pszValue++; }

    /*-----------------------------------------------------------------
     * Try to automagically detect time format, one of:
     * "HH:MM:SS", or "HHMMSSmmm"
     *----------------------------------------------------------------*/
    int nHour = 0;
    int nMin = 0;
    int nSec = 0;
    int nMS = 0;

    if (strlen(pszValue) == 8)
    {
        /*-------------------------------------------------------------
         * "HH:MM:SS"
         *------------------------------------------------------------*/
        char szBuf[9];
        strcpy(szBuf, pszValue);
        szBuf[2]=0;
        szBuf[5]=0;
        nHour = atoi(szBuf);
        nMin  = atoi(szBuf+3);
        nSec  = atoi(szBuf+6);
        nMS   = 0;
    }
    else if (strlen(pszValue) == 9)
    {
        /*-------------------------------------------------------------
         * "HHMMSSmmm"
         *------------------------------------------------------------*/
        char szBuf[4];
        strncpy(szBuf,pszValue,2);
        szBuf[2]=0;
        nHour = atoi(szBuf);

        strncpy(szBuf,pszValue+2,2);
        szBuf[2]=0;
        nMin = atoi(szBuf);

        strncpy(szBuf,pszValue+4,2);
        szBuf[2]=0;
        nSec = atoi(szBuf);

        strncpy(szBuf,pszValue+6,3);
        szBuf[3]=0;
        nMS = atoi(szBuf);
    }
    else if (strlen(pszValue) == 0)
    {
        // Write -1 to .DAT file if value is not set
        nHour = -1;
        nMin = -1;
        nSec = -1;
        nMS = -1;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid time field value `%s'.  Time field values must "
                 "be in the format `HH:MM:SS', or `HHMMSSmmm'",
                 pszValue);
        return -1;
    }

    return WriteTimeField(nHour, nMin, nSec, nMS, poINDFile, nIndexNo);
}

int TABDATFile::WriteTimeField(int nHour, int nMinute, int nSecond, int nMS,
                               TABINDFile *poINDFile, int nIndexNo)
{
    GInt32 nS = -1;

    if (m_poRecordBlock == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
            "Can't write field value: GetRecordBlock() has not been called.");
        return -1;
    }

    nS = (nHour*3600+nMinute*60+nSecond)*1000+nMS;
    if (nS < 0)
       nS = -1;
    m_poRecordBlock->WriteInt32(nS);

    if (CPLGetLastErrorNo() != 0)
        return -1;

    // Update Index
    if (poINDFile && nIndexNo > 0)
    {
        GByte *pKey = poINDFile->BuildKey(nIndexNo, (nS));
        if (poINDFile->AddEntry(nIndexNo, pKey, m_nCurRecordId) != 0)
            return -1;
    }

    return 0;
}

/**********************************************************************
 *                   TABDATFile::WriteDateTimeField()
 *
 * Write the DateTime field value at the current position in the data
 * block.
 *
 * A datetime field is a 8 bytes binary value in which the first byte is
 * the day, followe
d by 1 byte for the month, and 2 bytes for the year.
 * After this the time value is stored as a 4 byte integer
 * (milliseconds since midnight)
 *
 * The expected input is a 10 chars string in the format "YYYY/MM/DD HH:MM:SS"
 * or "DD/MM/YYYY HH:MM:SS" or "YYYYMMDDhhmmssmmm"
 *
 * Returns 0 on success, or -1 if the operation failed, in which case
 * CPLError() will have been called.
 **********************************************************************/
int TABDATFile::WriteDateTimeField(const char *pszValue,
                                   TABINDFile *poINDFile, int nIndexNo)
{
    /*-----------------------------------------------------------------
     * Get rid of leading spaces.
     *----------------------------------------------------------------*/
    while ( *pszValue == ' ' ) { pszValue++; }

    /*-----------------------------------------------------------------
     * Try to automagically detect date format, one of:
     * "YYYY/MM/DD HH:MM:SS", "DD/MM/YYYY HH:MM:SS", or "YYYYMMDDhhmmssmmm"
     *----------------------------------------------------------------*/
    int nDay, nMonth, nYear, nHour, nMin, nSec, nMS;
    char **papszTok = NULL;

    if (strlen(pszValue) == 17)
    {
        /*-------------------------------------------------------------
         * "YYYYMMDDhhmmssmmm"
         *------------------------------------------------------------*/
        char szBuf[18];
        strcpy(szBuf, pszValue);
        nMS  = atoi(szBuf+14);
        szBuf[14]=0;
        nSec = atoi(szBuf+12);
        szBuf[12]=0;
        nMin = atoi(szBuf+10);
        szBuf[10]=0;
        nHour = atoi(szBuf+8);
        szBuf[8]=0;
        nDay = atoi(szBuf+6);
        szBuf[6] = 0;
        nMonth = atoi(szBuf+4);
        szBuf[4] = 0;
        nYear = atoi(szBuf);
    }
    else if (strlen(pszValue) == 19 &&
             (papszTok = CSLTokenizeStringComplex(pszValue, "/ :",
                                                  FALSE, FALSE)) != NULL &&
             CSLCount(papszTok) == 6 &&
             (strlen(papszTok[0]) == 4 || strlen(papszTok[2]) == 4) )
    {
        /*-------------------------------------------------------------
         * Either "YYYY/MM/DD HH:MM:SS" or "DD/MM/YYYY HH:MM:SS"
         *------------------------------------------------------------*/
        if (strlen(papszTok[0]) == 4)
        {
            nYear = atoi(papszTok[0]);
            nMonth= atoi(papszTok[1]);
            nDay  = atoi(papszTok[2]);
            nHour = atoi(papszTok[3]);
            nMin  = atoi(papszTok[4]);
            nSec  = atoi(papszTok[5]);
            nMS   = 0;
        }
        else
        {
            nYear = atoi(papszTok[2]);
            nMonth= atoi(papszTok[1]);
            nDay  = atoi(papszTok[0]);
            nHour = atoi(papszTok[3]);
            nMin  = atoi(papszTok[4]);
            nSec  = atoi(papszTok[5]);
            nMS   = 0;
        }
    }
    else if (strlen(pszValue) == 0)
    {
        nYear = 0;
        nMonth = 0;
        nDay = 0;
        nHour = 0;
        nMin = 0;
        nSec = 0;
        nMS = 0;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid date field value `%s'.  Date field values must "
                 "be in the format `YYYY/MM/DD HH:MM:SS', "
                 "`MM/DD/YYYY HH:MM:SS' or `YYYYMMDDhhmmssmmm'",
                 pszValue);
        CSLDestroy(papszTok);
        return -1;
    }
    CSLDestroy(papszTok);

    return WriteDateTimeField(nYear, nMonth, nDay, nHour, nMin, nSec, nMS,
                              poINDFile, nIndexNo);
}

int TABDATFile::WriteDateTimeField(int nYear, int nMonth, int nDay,
                                   int nHour, int nMinute, int nSecond, int nMS,
                                   TABINDFile *poINDFile, int nIndexNo)
{
    GInt32 nS = (nHour*3600+nMinute*60+nSecond)*1000+nMS;

    if (m_poRecordBlock == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
            "Can't write field value: GetRecordBlock() has not been called.");
        return -1;
    }

    m_poRecordBlock->WriteInt16((GInt16)nYear);
    m_poRecordBlock->WriteByte((GByte)nMonth);
    m_poRecordBlock->WriteByte((GByte)nDay);
    m_poRecordBlock->WriteInt32(nS);

    if (CPLGetLastErrorNo() != 0)
        return -1;

    // Update Index
    if (poINDFile && nIndexNo > 0)
    {
        // __TODO__  (see bug #1844)
        // Indexing on DateTime Fields not currently supported, that will
        // require passing the 8 bytes datetime value to BuildKey() here...
        CPLAssert(false);
        GByte *pKey = poINDFile->BuildKey(nIndexNo, (nYear*0x10000 +
                                                     nMonth * 0x100 + nDay));
        if (poINDFile->AddEntry(nIndexNo, pKey, m_nCurRecordId) != 0)
            return -1;
    }

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
int TABDATFile::WriteDecimalField(double dValue, int nWidth, int nPrec,
                                  TABINDFile *poINDFile, int nIndexNo)
{
    char szFormat[10];

    snprintf(szFormat, sizeof(szFormat), "%%%d.%df", nWidth, nPrec);
    const char *pszVal = CPLSPrintf(szFormat, dValue);
    if ((int)strlen(pszVal) > nWidth)
        pszVal += strlen(pszVal) - nWidth;

    // Update Index
    if (poINDFile && nIndexNo > 0)
    {
        GByte *pKey = poINDFile->BuildKey(nIndexNo, dValue);
        if (poINDFile->AddEntry(nIndexNo, pKey, m_nCurRecordId) != 0)
            return -1;
    }

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
