/**********************************************************************
 *
 * Name:     mitab_idfile.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation of the TABIDFile class used to handle
 *           reading/writing of the .ID file attached to a .MAP file
 * Author:   Daniel Morissette, dmorissette@dmsolutions.ca
 *
 **********************************************************************
 * Copyright (c) 1999, 2000, Daniel Morissette
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
 **********************************************************************/

#include "cpl_port.h"
#include "mitab.h"

#include <algorithm>
#include <limits.h>
#include <string.h>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi.h"
#include "mitab_priv.h"
#include "mitab_utils.h"

CPL_CVSID("$Id$")

/*=====================================================================
 *                      class TABIDFile
 *====================================================================*/

/**********************************************************************
 *                   TABIDFile::TABIDFile()
 *
 * Constructor.
 **********************************************************************/
TABIDFile::TABIDFile() :
    m_pszFname(NULL),
    m_fp(NULL),
    m_eAccessMode(TABRead),
    m_poIDBlock(NULL),
    m_nBlockSize(0),
    m_nMaxId(-1)
{}

/**********************************************************************
 *                   TABIDFile::~TABIDFile()
 *
 * Destructor.
 **********************************************************************/
TABIDFile::~TABIDFile() { Close(); }

/**********************************************************************
 *                   TABIDFile::Open()
 *
 * Compatibility layer with new interface.
 * Return 0 on success, -1 in case of failure.
 **********************************************************************/

int TABIDFile::Open(const char *pszFname, const char *pszAccess)
{
    // cppcheck-suppress nullPointer
    if( STARTS_WITH_CI(pszAccess, "r") )
        return Open(pszFname, TABRead);
    if( STARTS_WITH_CI(pszAccess, "w") )
        return Open(pszFname, TABWrite);

    CPLError(CE_Failure, CPLE_FileIO,
             "Open() failed: access mode \"%s\" not supported", pszAccess);
    return -1;
}

/**********************************************************************
 *                   TABIDFile::Open()
 *
 * Open a .ID file, and initialize the structures to be ready to read
 * objects from it.
 *
 * If the filename that is passed in contains a .MAP extension then
 * the extension will be changed to .ID before trying to open the file.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABIDFile::Open(const char *pszFname, TABAccess eAccess)
{
    if (m_fp)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Open() failed: object already contains an open file");
        return -1;
    }

    // Validate access mode and make sure we use binary access.
    // Note that in Write mode we need TABReadWrite since we do random
    // updates in the index as data blocks are split.
    const char *pszAccess = NULL;
    if (eAccess == TABRead)
    {
        m_eAccessMode = TABRead;
        pszAccess = "rb";
    }
    else if (eAccess == TABWrite)
    {
        m_eAccessMode = TABReadWrite;
        pszAccess = "wb+";
    }
    else if (eAccess == TABReadWrite)
    {
        m_eAccessMode = TABReadWrite;
        pszAccess = "rb+";
    }
    else
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Open() failed: access mode \"%d\" not supported", eAccess);
        return -1;
    }

    // Change .MAP extension to .ID if necessary.
    m_pszFname = CPLStrdup(pszFname);

    const int nLen = static_cast<int>(strlen(m_pszFname));
    if (nLen > 4 && strcmp(m_pszFname + nLen - 4, ".MAP") == 0)
        strcpy(m_pszFname + nLen - 4, ".ID");
    else if (nLen > 4 && strcmp(m_pszFname + nLen - 4, ".map") == 0)
        strcpy(m_pszFname + nLen - 4, ".id");

#ifndef _WIN32
    // Change .MAP extension to .ID if necessary.
    TABAdjustFilenameExtension(m_pszFname);
#endif

    // Open file.
    m_fp = VSIFOpenL(m_pszFname, pszAccess);

    if (m_fp == NULL)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Open() failed for %s", m_pszFname);
        CPLFree(m_pszFname);
        m_pszFname = NULL;
        return -1;
    }

    if (m_eAccessMode == TABRead || m_eAccessMode == TABReadWrite)
    {
        // READ access:
        // Establish the number of object IDs from the size of the file.
        VSIStatBufL sStatBuf;
        if ( VSIStatL(m_pszFname, &sStatBuf) == -1 )
        {
            CPLError(CE_Failure, CPLE_FileIO, "stat() failed for %s",
                     m_pszFname);
            Close();
            return -1;
        }

        if( static_cast<vsi_l_offset>(sStatBuf.st_size) >
            static_cast<vsi_l_offset>(INT_MAX / 4) )
            m_nMaxId = INT_MAX / 4;
        else
            m_nMaxId = static_cast<int>(sStatBuf.st_size / 4);
        m_nBlockSize = std::min(1024, m_nMaxId * 4);

        // Read the first block from the file.
        m_poIDBlock = new TABRawBinBlock(m_eAccessMode, FALSE);

        if (m_nMaxId == 0)
        {
            // .ID file size = 0 ... just allocate a blank block but
            // it won't get really used anyways.
            m_nBlockSize = 512;
            m_poIDBlock->InitNewBlock(m_fp, m_nBlockSize, 0);
        }
        else if (m_poIDBlock->ReadFromFile(m_fp, 0, m_nBlockSize) != 0)
        {
            // CPLError() has already been called.
            Close();
            return -1;
        }
    }
    else
    {
        // WRITE access:
        // Get ready to write to the file.
        m_poIDBlock = new TABRawBinBlock(m_eAccessMode, FALSE);
        m_nMaxId = 0;
        m_nBlockSize = 1024;
        m_poIDBlock->InitNewBlock(m_fp, m_nBlockSize, 0);
    }

    return 0;
}

/**********************************************************************
 *                   TABIDFile::Close()
 *
 * Close current file, and release all memory used.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABIDFile::Close()
{
    if (m_fp == NULL)
        return 0;

    // Write access: commit latest changes to the file.
    if (m_eAccessMode != TABRead)
        SyncToDisk();

    // Delete all structures
    delete m_poIDBlock;
    m_poIDBlock = NULL;

    // Close file
    VSIFCloseL(m_fp);
    m_fp = NULL;

    CPLFree(m_pszFname);
    m_pszFname = NULL;

    return 0;
}

/************************************************************************/
/*                            SyncToDisk()                             */
/************************************************************************/

int TABIDFile::SyncToDisk()
{
    if( m_eAccessMode == TABRead )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SyncToDisk() can be used only with Write access.");
        return -1;
    }

    if( m_poIDBlock == NULL)
        return 0;

    return m_poIDBlock->CommitToFile();
}

/**********************************************************************
 *                   TABIDFile::GetObjPtr()
 *
 * Return the offset in the .MAP file where the map object with the
 * specified id is located.
 *
 * Note that object ids are positive and start at 1.
 *
 * An object Id of '0' means that object has no geometry.
 *
 * Returns a value >= 0 on success, -1 on error.
 **********************************************************************/
GInt32 TABIDFile::GetObjPtr(GInt32 nObjId)
{
    if (m_poIDBlock == NULL)
        return -1;

    if (nObjId < 1 || nObjId > m_nMaxId)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "GetObjPtr(): Invalid object ID %d (valid range is [1..%d])",
                 nObjId, m_nMaxId);
        return -1;
    }

    if (m_poIDBlock->GotoByteInFile((nObjId - 1) * 4) != 0)
        return -1;

    return m_poIDBlock->ReadInt32();
}

/**********************************************************************
 *                   TABIDFile::SetObjPtr()
 *
 * Set the offset in the .MAP file where the map object with the
 * specified id is located.
 *
 * Note that object ids are positive and start at 1.
 *
 * An object Id of '0' means that object has no geometry.
 *
 * Returns a value of 0 on success, -1 on error.
 **********************************************************************/
int TABIDFile::SetObjPtr(GInt32 nObjId, GInt32 nObjPtr)
{
    if (m_poIDBlock == NULL)
        return -1;

    if (m_eAccessMode == TABRead)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetObjPtr() can be used only with Write access.");
        return -1;
    }

    if (nObjId < 1)
    {
        CPLError(
            CE_Failure, CPLE_IllegalArg,
            "SetObjPtr(): Invalid object ID %d (must be greater than zero)",
            nObjId);
        return -1;
    }

    // GotoByteInFile() will automagically commit current block and init
    // a new one if necessary.
    const GInt32 nLastIdBlock = ((m_nMaxId - 1) * 4) / m_nBlockSize;
    const GInt32 nTargetIdBlock = ((nObjId - 1) * 4) / m_nBlockSize;
    if (m_nMaxId > 0 && nTargetIdBlock <= nLastIdBlock)
    {
        // Pass second arg to GotoByteInFile() to force reading from file
        // when going back to blocks already committed.
        if (m_poIDBlock->GotoByteInFile((nObjId - 1) * 4, TRUE) != 0)
            return -1;
    }
    else
    {
        // If we reach EOF then a new empty block will have to be allocated.
        if (m_poIDBlock->GotoByteInFile((nObjId - 1) * 4) != 0)
            return -1;
    }

    m_nMaxId = std::max(m_nMaxId, nObjId);

    return m_poIDBlock->WriteInt32(nObjPtr);
}

/**********************************************************************
 *                   TABIDFile::GetMaxObjId()
 *
 * Return the value of the biggest valid object id.
 *
 * Note that object ids are positive and start at 1.
 *
 * Returns a value >= 0 on success, -1 on error.
 **********************************************************************/
GInt32 TABIDFile::GetMaxObjId() { return m_nMaxId; }

/**********************************************************************
 *                   TABIDFile::Dump()
 *
 * Dump block contents... available only in DEBUG mode.
 **********************************************************************/
#ifdef DEBUG

void TABIDFile::Dump(FILE *fpOut /*=NULL*/)
{
    if (fpOut == NULL)
        fpOut = stdout;

    fprintf(fpOut, "----- TABIDFile::Dump() -----\n");

    if (m_fp == NULL)
    {
        fprintf(fpOut, "File is not opened.\n");
    }
    else
    {
        fprintf(fpOut, "File is opened: %s\n", m_pszFname);
        fprintf(fpOut, "Current index block follows ...\n\n");
        m_poIDBlock->Dump(fpOut);
        fprintf(fpOut, "... end of index block.\n\n");
    }

    fflush(fpOut);
}

#endif  // DEBUG
