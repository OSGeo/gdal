/******************************************************************************
 *
 * Project:  ISO 8211 Access
 * Purpose:  Implements the DDFRecord class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "iso8211.h"

#include <cstddef>
#include <cstdio>
#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi.h"

constexpr int nLeaderSize = 24;

/************************************************************************/
/*                             DDFRecord()                              */
/************************************************************************/

DDFRecord::DDFRecord(DDFModule *poModuleIn)
    : poModule(poModuleIn), _sizeFieldTag(poModuleIn->GetSizeFieldTag())
{
}

/************************************************************************/
/*                             ~DDFRecord()                             */
/************************************************************************/

DDFRecord::~DDFRecord()

{
    Clear();
}

/************************************************************************/
/*                             GetFields()                              */
/************************************************************************/

/**
 * Return all fields of the specified name.
 */

std::vector<const DDFField *>
DDFRecord::GetFields(const char *pszFieldName) const
{
    std::vector<const DDFField *> res;
    for (auto &field : apoFields)
    {
        if (strcmp(field->GetFieldDefn()->GetName(), pszFieldName) == 0)
            res.push_back(field.get());
    }
    return res;
}

/************************************************************************/
/*                             GetFields()                              */
/************************************************************************/

/**
 * Return all fields of the specified name.
 */

std::vector<DDFField *> DDFRecord::GetFields(const char *pszFieldName)
{
    std::vector<DDFField *> res;
    for (auto &field : apoFields)
    {
        if (strcmp(field->GetFieldDefn()->GetName(), pszFieldName) == 0)
            res.push_back(field.get());
    }
    return res;
}

/************************************************************************/
/*                                Dump()                                */
/************************************************************************/

/**
 * Write out record contents to debugging file.
 *
 * A variety of information about this record, and all its fields and
 * subfields is written to the given debugging file handle.  Note that
 * field definition information (ala DDFFieldDefn) isn't written.
 *
 * @param fp The standard IO file handle to write to.  i.e. stderr
 */

void DDFRecord::Dump(FILE *fp, int nNestingLevel) const

{
    std::string osIndent;
    for (int i = 0; i < nNestingLevel; ++i)
        osIndent += "  ";

#define Print(...)                                                             \
    do                                                                         \
    {                                                                          \
        fprintf(fp, "%s", osIndent.c_str());                                   \
        fprintf(fp, __VA_ARGS__);                                              \
    } while (0)

    Print("DDFRecord:\n");
    Print("    bReuseHeader = %d\n", bReuseHeader);
    Print("    nDataSize = %d\n", GetDataSize());
    Print("    _sizeFieldLength=%d, _sizeFieldPos=%d, _sizeFieldTag=%d\n",
          _sizeFieldLength, _sizeFieldPos, _sizeFieldTag);

    for (const auto &poField : apoFields)
    {
        poField->Dump(fp, nNestingLevel + 1);
    }
}

/************************************************************************/
/*                                Read()                                */
/*                                                                      */
/*      Read a record of data from the file, and parse the header to    */
/*      build a field list for the record (or reuse the existing one    */
/*      if reusing headers).  It is expected that the file pointer      */
/*      will be positioned at the beginning of a data record.  It is    */
/*      the DDFModule's responsibility to do so.                        */
/*                                                                      */
/*      This method should only be called by the DDFModule class.       */
/************************************************************************/

int DDFRecord::Read()

{
    /* -------------------------------------------------------------------- */
    /*      Redefine the record on the basis of the header if needed.       */
    /*      As a side effect this will read the data for the record as well.*/
    /* -------------------------------------------------------------------- */
    if (!bReuseHeader)
    {
        return ReadHeader();
    }
    if (nFieldOffset < 0)
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      Otherwise we read just the data and carefully overlay it on     */
    /*      the previous records data without disturbing the rest of the    */
    /*      record.                                                         */
    /* -------------------------------------------------------------------- */
    size_t nReadBytes;

    CPLAssert(nFieldOffset <= static_cast<int>(osData.size()));
    nReadBytes = VSIFReadL(osData.data() + nFieldOffset, 1,
                           osData.size() - nFieldOffset, poModule->GetFP());
    if (nReadBytes != osData.size() - nFieldOffset && nReadBytes == 0 &&
        VSIFEofL(poModule->GetFP()))
    {
        return FALSE;
    }
    else if (nReadBytes != osData.size() - nFieldOffset)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Data record is short on DDF file.");

        return FALSE;
    }

    // notdef: eventually we may have to do something at this point to
    // notify the DDFField's that their data values have changed.

    return TRUE;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

/**
 * Write record out to module.
 *
 * This method writes the current record to the module to which it is
 * attached.  Normally this would be at the end of the file, and only used
 * for modules newly created with DDFModule::Create().  Rewriting existing
 * records is not supported at this time.  Calling Write() multiple times
 * on a DDFRecord will result it multiple copies being written at the end of
 * the module.
 *
 * @return TRUE on success or FALSE on failure.
 */

int DDFRecord::Write()

{
    ResetDirectory();

    /* -------------------------------------------------------------------- */
    /*      Prepare leader.                                                 */
    /* -------------------------------------------------------------------- */
    char szLeader[nLeaderSize + 1];

    memset(szLeader, ' ', nLeaderSize);

    snprintf(szLeader + 0, sizeof(szLeader) - 0, "%05d",
             static_cast<int>(osData.size() + nLeaderSize));
    szLeader[5] = ' ';
    szLeader[6] = 'D';

    snprintf(szLeader + 12, sizeof(szLeader) - 12, "%05d",
             static_cast<int>(nFieldOffset + nLeaderSize));
    szLeader[17] = ' ';

    szLeader[20] = static_cast<char>('0' + _sizeFieldLength);
    szLeader[21] = static_cast<char>('0' + _sizeFieldPos);
    szLeader[22] = '0';
    szLeader[23] = static_cast<char>('0' + _sizeFieldTag);

    /* notdef: lots of stuff missing */

    /* -------------------------------------------------------------------- */
    /*      Write the leader.                                               */
    /* -------------------------------------------------------------------- */
    int bRet = VSIFWriteL(szLeader, nLeaderSize, 1, poModule->GetFP()) > 0;

    /* -------------------------------------------------------------------- */
    /*      Write the remainder of the record.                              */
    /* -------------------------------------------------------------------- */
    bRet &= VSIFWriteL(osData.data(), osData.size(), 1, poModule->GetFP()) > 0;

    return bRet ? TRUE : FALSE;
}

/************************************************************************/
/*                               Clear()                                */
/*                                                                      */
/*      Clear any information associated with the last header in        */
/*      preparation for reading a new header.                           */
/************************************************************************/

void DDFRecord::Clear()

{
    apoFields.clear();
    osData.clear();
    bReuseHeader = FALSE;
}

/************************************************************************/
/*                             ReadHeader()                             */
/*                                                                      */
/*      This perform the header reading and parsing job for the         */
/*      Read() method.  It reads the header, and builds a field         */
/*      list.                                                           */
/************************************************************************/

int DDFRecord::ReadHeader()

{
    /* -------------------------------------------------------------------- */
    /*      Clear any existing information.                                 */
    /* -------------------------------------------------------------------- */
    Clear();

    /* -------------------------------------------------------------------- */
    /*      Read the 24 byte leader.                                        */
    /* -------------------------------------------------------------------- */
    char achLeader[nLeaderSize];
    int nReadBytes;

    nReadBytes = static_cast<int>(
        VSIFReadL(achLeader, 1, nLeaderSize, poModule->GetFP()));
    if (nReadBytes == 0 && VSIFEofL(poModule->GetFP()))
    {
        nFieldOffset = -1;
        return FALSE;
    }
    // The ASRP and USRP specifications mentions that 0x5E / ^ character can be
    // used as a padding byte so that the file size is a multiple of 8192.
    else if (achLeader[0] == '^')
    {
        nFieldOffset = -1;
        return FALSE;
    }
    else if (nReadBytes != static_cast<int>(nLeaderSize))
    {
        CPLError(CE_Failure, CPLE_FileIO, "Leader is short on DDF file.");
        nFieldOffset = -1;
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      Extract information from leader.                                */
    /* -------------------------------------------------------------------- */
    int _recLength, _fieldAreaStart;
    char _leaderIden;

    _recLength = DDFScanInt(achLeader + 0, 5);
    _leaderIden = achLeader[6];
    _fieldAreaStart = DDFScanInt(achLeader + 12, 5);

    _sizeFieldLength = achLeader[20] - '0';
    _sizeFieldPos = achLeader[21] - '0';
    _sizeFieldTag = achLeader[23] - '0';

    if (_sizeFieldLength <= 0 || _sizeFieldLength > 9 || _sizeFieldPos <= 0 ||
        _sizeFieldPos > 9 || _sizeFieldTag <= 0 || _sizeFieldTag > 9)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "ISO8211 record leader appears to be corrupt.");
        nFieldOffset = -1;
        return FALSE;
    }

    if (_leaderIden == 'R')
        bReuseHeader = TRUE;

    nFieldOffset = _fieldAreaStart - nLeaderSize;

    /* -------------------------------------------------------------------- */
    /*      Is there anything seemly screwy about this record?              */
    /* -------------------------------------------------------------------- */
    if (((_recLength <= 24 || _recLength > 100000000) && (_recLength != 0)) ||
        _fieldAreaStart < 24 || _fieldAreaStart > 100000)
    {
        CPLError(
            CE_Failure, CPLE_FileIO,
            "Data record appears to be corrupt on DDF file.\n"
            " -- ensure that the files were uncompressed without modifying\n"
            "carriage return/linefeeds (by default WINZIP does this).");
        nFieldOffset = -1;
        return FALSE;
    }

    /* ==================================================================== */
    /*      Handle the normal case with the record length available.        */
    /* ==================================================================== */
    if (_recLength != 0)
    {
        /* --------------------------------------------------------------------
         */
        /*      Read the remainder of the record. */
        /* --------------------------------------------------------------------
         */
        int nDataSize = _recLength - nLeaderSize;
        osData.resize(nDataSize);

        if (VSIFReadL(osData.data(), 1, osData.size(), poModule->GetFP()) !=
            osData.size())
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Data record is short on DDF file.");
            nFieldOffset = -1;
            return FALSE;
        }

        /* --------------------------------------------------------------------
         */
        /*      If we don't find a field terminator at the end of the record */
        /*      we will read extra bytes till we get to it. */
        /* --------------------------------------------------------------------
         */
        while (!osData.empty() && osData.back() != DDF_FIELD_TERMINATOR &&
               (osData.size() < 2 ||
                osData[osData.size() - 2] != DDF_FIELD_TERMINATOR))
        {
            osData.resize(osData.size() + 1);

            if (VSIFReadL(&(osData.back()), 1, 1, poModule->GetFP()) != 1)
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Data record is short on DDF file.");
                nFieldOffset = -1;
                return FALSE;
            }
            static bool bFirstTime = true;
            if (bFirstTime)
            {
                bFirstTime = false;
                CPLDebug("ISO8211",
                         "Didn't find field terminator, read one more byte.");
            }
        }

        if (nFieldOffset >= static_cast<int>(osData.size()))
        {
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "nFieldOffset < static_cast<int>(osData.size())");
            nFieldOffset = -1;
            return FALSE;
        }

        /* --------------------------------------------------------------------
         */
        /*      Loop over the directory entries, making a pass counting them. */
        /* --------------------------------------------------------------------
         */
        int i;
        int nFieldEntryWidth;

        nFieldEntryWidth = _sizeFieldLength + _sizeFieldPos + _sizeFieldTag;
        if (nFieldEntryWidth <= 0)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Invalid entry width = %d",
                     nFieldEntryWidth);
            nFieldOffset = -1;
            return FALSE;
        }

        int nFieldCount = 0;
        for (i = 0; i + nFieldEntryWidth <= static_cast<int>(osData.size());
             i += nFieldEntryWidth)
        {
            if (osData[i] == DDF_FIELD_TERMINATOR)
                break;

            nFieldCount++;
        }

        /* --------------------------------------------------------------------
         */
        /*      Allocate, and read field definitions. */
        /* --------------------------------------------------------------------
         */
        apoFields.resize(nFieldCount);

        int nLastFieldPos = 0;
        int nLastFieldLength = 0;
        for (i = 0; i < nFieldCount; i++)
        {
            char szTag[128];
            int nEntryOffset = i * nFieldEntryWidth;
            int nFieldLength, nFieldPos;

            /* --------------------------------------------------------------------
             */
            /*      Read the position information and tag. */
            /* --------------------------------------------------------------------
             */
            strncpy(szTag, osData.c_str() + nEntryOffset, _sizeFieldTag);
            szTag[_sizeFieldTag] = '\0';

            nEntryOffset += _sizeFieldTag;
            nFieldLength =
                DDFScanInt(osData.c_str() + nEntryOffset, _sizeFieldLength);

            nEntryOffset += _sizeFieldLength;
            nFieldPos =
                DDFScanInt(osData.c_str() + nEntryOffset, _sizeFieldPos);

            /* --------------------------------------------------------------------
             */
            /*      Find the corresponding field in the module directory. */
            /* --------------------------------------------------------------------
             */
            DDFFieldDefn *poFieldDefn = poModule->FindFieldDefn(szTag);

            if (poFieldDefn == nullptr || nFieldLength < 0 || nFieldPos < 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Undefined field `%s' encountered in data record.",
                         szTag);
                return FALSE;
            }

            if (_fieldAreaStart + nFieldPos - nLeaderSize < 0 ||
                static_cast<int>(osData.size()) -
                        (_fieldAreaStart + nFieldPos - nLeaderSize) <
                    nFieldLength)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Not enough byte to initialize field `%s'.", szTag);
                nFieldOffset = -1;
                return FALSE;
            }

            // This check is not strictly needed for reading scenarios, but
            // in update scenarios (such as S57/S101), it is essential to avoid
            // issues when resizing fields.
            if (nFieldPos < nLastFieldPos + nLastFieldLength)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Field `%s' overlapping with previous one.", szTag);
                nFieldOffset = -1;
                return FALSE;
            }

            /* --------------------------------------------------------------------
             */
            /*      Assign info the DDFField. */
            /* --------------------------------------------------------------------
             */
            apoFields[i] = std::make_unique<DDFField>();
            if (!apoFields[i]->Initialize(poFieldDefn,
                                          osData.c_str() + _fieldAreaStart +
                                              nFieldPos - nLeaderSize,
                                          nFieldLength, true))
            {
                // Error message emitted by Initialize()
                nFieldOffset = -1;
                return FALSE;
            }

            nLastFieldPos = nFieldPos;
            nLastFieldLength = nFieldLength;
        }

        return TRUE;
    }
    /* ==================================================================== */
    /*      Handle the exceptional case where the record length is          */
    /*      zero.  In this case we have to read all the data based on       */
    /*      the size of data items as per ISO8211 spec Annex C, 1.5.1.      */
    /*                                                                      */
    /*      See Bugzilla bug 181 and test with file US4CN21M.000.           */
    /* ==================================================================== */
    else
    {
        CPLDebug("ISO8211",
                 "Record with zero length, use variant (C.1.5.1) logic.");

        /* ----------------------------------------------------------------- */
        /*   _recLength == 0, handle the large record.                       */
        /*                                                                   */
        /*   Read the remainder of the record.                               */
        /* ----------------------------------------------------------------- */
        osData.clear();

        /* ----------------------------------------------------------------- */
        /*   Loop over the directory entries, making a pass counting them.   */
        /* ----------------------------------------------------------------- */
        int nFieldEntryWidth = _sizeFieldLength + _sizeFieldPos + _sizeFieldTag;
        int nFieldCount = 0;
        int i = 0;

        if (nFieldEntryWidth == 0)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Invalid record buffer size : %d.", nFieldEntryWidth);
            nFieldOffset = -1;
            return FALSE;
        }

        std::string osEntry;
        osEntry.resize(nFieldEntryWidth);

        // while we're not at the end, store this entry,
        // and keep on reading...
        do
        {
            // read an Entry:
            if (nFieldEntryWidth !=
                static_cast<int>(VSIFReadL(osEntry.data(), 1, nFieldEntryWidth,
                                           poModule->GetFP())))
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Data record is short on DDF file.");
                nFieldOffset = -1;
                return FALSE;
            }

            osData.append(osEntry.c_str(), nFieldEntryWidth);

            if (DDF_FIELD_TERMINATOR != osEntry[0])
            {
                nFieldCount++;
                if (nFieldCount == 1000)
                {
                    CPLError(CE_Failure, CPLE_FileIO,
                             "Too many fields in DDF file.");
                    nFieldOffset = -1;
                    return FALSE;
                }
            }
        } while (DDF_FIELD_TERMINATOR != osEntry[0]);

        // --------------------------------------------------------------------
        // Now, rewind a little.  Only the TERMINATOR should have been read
        // --------------------------------------------------------------------
        int rewindSize = nFieldEntryWidth - 1;
        VSILFILE *fp = poModule->GetFP();
        vsi_l_offset pos = VSIFTellL(fp) - rewindSize;
        if (VSIFSeekL(fp, pos, SEEK_SET) < 0)
            return FALSE;
        osData.resize(osData.size() - rewindSize);

        // --------------------------------------------------------------------
        // Okay, now let's populate the heck out of osData...
        // --------------------------------------------------------------------
        for (i = 0; i < nFieldCount; i++)
        {
            int nEntryOffset = (i * nFieldEntryWidth) + _sizeFieldTag;
            int nFieldLength =
                DDFScanInt(osData.c_str() + nEntryOffset, _sizeFieldLength);
            if (nFieldLength < 0)
            {
                nFieldOffset = -1;
                return FALSE;
            }

            osEntry.resize(nFieldLength);

            // read an Entry:
            if (nFieldLength !=
                static_cast<int>(VSIFReadL(osEntry.data(), 1, nFieldLength,
                                           poModule->GetFP())))
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Data record is short on DDF file.");
                nFieldOffset = -1;
                return FALSE;
            }

            // move this temp buffer into more permanent storage:
            osData.append(osEntry.data(), nFieldLength);
        }

        if (nFieldOffset >= static_cast<int>(osData.size()))
        {
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "nFieldOffset < static_cast<int>(osData.size())");
            nFieldOffset = -1;
            return FALSE;
        }

        /* ----------------------------------------------------------------- */
        /*     Allocate, and read field definitions.                         */
        /* ----------------------------------------------------------------- */
        apoFields.resize(nFieldCount);

        int nLastFieldPos = 0;
        int nLastFieldLength = 0;
        for (i = 0; i < nFieldCount; i++)
        {
            char szTag[128];
            int nEntryOffset = i * nFieldEntryWidth;
            int nFieldLength, nFieldPos;

            /* ------------------------------------------------------------- */
            /* Read the position information and tag.                        */
            /* ------------------------------------------------------------- */
            strncpy(szTag, osData.c_str() + nEntryOffset, _sizeFieldTag);
            szTag[_sizeFieldTag] = '\0';

            nEntryOffset += _sizeFieldTag;
            nFieldLength =
                DDFScanInt(osData.c_str() + nEntryOffset, _sizeFieldLength);

            nEntryOffset += _sizeFieldLength;
            nFieldPos =
                DDFScanInt(osData.c_str() + nEntryOffset, _sizeFieldPos);

            /* ------------------------------------------------------------- */
            /* Find the corresponding field in the module directory.         */
            /* ------------------------------------------------------------- */
            DDFFieldDefn *poFieldDefn = poModule->FindFieldDefn(szTag);

            if (poFieldDefn == nullptr || nFieldLength < 0 || nFieldPos < 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Undefined field `%s' encountered in data record.",
                         szTag);
                nFieldOffset = -1;
                return FALSE;
            }

            if (_fieldAreaStart + nFieldPos - nLeaderSize < 0 ||
                static_cast<int>(osData.size()) -
                        (_fieldAreaStart + nFieldPos - nLeaderSize) <
                    nFieldLength)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Not enough byte to initialize field `%s'.", szTag);
                nFieldOffset = -1;
                return FALSE;
            }

            // This check is not strictly needed for reading scenarios, but
            // in update scenarios (such as S57/S101), it is essential to avoid
            // issues when resizing fields.
            if (nFieldPos < nLastFieldPos + nLastFieldLength)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Field `%s' overlapping with previous one.", szTag);
                nFieldOffset = -1;
                return FALSE;
            }

            /* ------------------------------------------------------------- */
            /* Assign info the DDFField.                                     */
            /* ------------------------------------------------------------- */

            apoFields[i] = std::make_unique<DDFField>();
            if (!apoFields[i]->Initialize(poFieldDefn,
                                          osData.c_str() + _fieldAreaStart +
                                              nFieldPos - nLeaderSize,
                                          nFieldLength, true))
            {
                // Error message emitted by Initialize()
                nFieldOffset = -1;
                return FALSE;
            }

            nLastFieldPos = nFieldPos;
            nLastFieldLength = nFieldLength;
        }

        return TRUE;
    }
}

/************************************************************************/
/*                             FindField()                              */
/************************************************************************/

/**
 * Find the named field within this record.
 *
 * @param pszName The name of the field to fetch.  The comparison is
 * case insensitive.
 * @param iFieldIndex The instance of this field to fetch. Use zero (the
 * default) for the first instance.
 *
 * @return Pointer to the requested DDFField. This pointer is to an
 * internal object, and should not be freed.  It remains valid until
 * the next record read.
 */

const DDFField *DDFRecord::FindField(const char *pszName, int iFieldIndex) const

{
    for (const auto &poField : apoFields)
    {
        const DDFFieldDefn *poFieldDefn = poField->GetFieldDefn();
        if (poFieldDefn && EQUAL(poFieldDefn->GetName(), pszName))
        {
            if (iFieldIndex == 0)
                return poField.get();
            else
                iFieldIndex--;
        }
    }

    return nullptr;
}

/************************************************************************/
/*                              GetField()                              */
/************************************************************************/

/**
 * Fetch field object based on index.
 *
 * @param i The index of the field to fetch.  Between 0 and GetFieldCount()-1.
 *
 * @return A DDFField pointer, or NULL if the index is out of range.
 */

const DDFField *DDFRecord::GetField(int i) const

{
    if (i < 0 || static_cast<size_t>(i) >= apoFields.size())
        return nullptr;
    else
        return apoFields[i].get();
}

/************************************************************************/
/*                          FindSubfieldDefn()                          */
/************************************************************************/

/* static */ std::tuple<const DDFField *, const DDFSubfieldDefn *>
DDFRecord::FindSubfieldDefn(const DDFField *poField, const char *pszSubfield,
                            bool bEmitError)
{
    if (poField->GetParts().empty())
    {
        /* -------------------------------------------------------------------- */
        /*      Get the subfield definition                                     */
        /* -------------------------------------------------------------------- */
        const DDFSubfieldDefn *poSFDefn =
            poField->GetFieldDefn()->FindSubfieldDefn(pszSubfield);
        if (poSFDefn == nullptr)
        {
            if (bEmitError)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot find subfield %s of %s", pszSubfield,
                         poField->GetFieldDefn()->GetName());
            }
            return {nullptr, nullptr};
        }

        return {nullptr, poSFDefn};
    }
    else
    {
        for (auto &poPart : poField->GetParts())
        {
            const DDFSubfieldDefn *poSFDefn =
                poPart->GetFieldDefn()->FindSubfieldDefn(pszSubfield);
            if (poSFDefn)
                return {poPart.get(), poSFDefn};
        }
        if (bEmitError)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find subfield %s of %s", pszSubfield,
                     poField->GetFieldDefn()->GetName());
        }
        return {nullptr, nullptr};
    }
}

/************************************************************************/
/*                          FindSubfieldDefn()                          */
/************************************************************************/

std::tuple<const DDFField *, const DDFField *, const DDFSubfieldDefn *>
DDFRecord::FindSubfieldDefn(const char *pszField, int iFieldIndex,
                            const char *pszSubfield, bool bEmitError) const
{
    /* -------------------------------------------------------------------- */
    /*      Fetch the field. If this fails, return zero.                    */
    /* -------------------------------------------------------------------- */
    const DDFField *poField = FindField(pszField, iFieldIndex);
    if (poField == nullptr)
    {
        if (bEmitError)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find field index %d of %s", iFieldIndex, pszField);
        }
        return {nullptr, nullptr, nullptr};
    }

    auto [poPartField, poSFDefn] =
        FindSubfieldDefn(poField, pszSubfield, bEmitError);
    return {poSFDefn ? poField : nullptr, poPartField, poSFDefn};
}

/************************************************************************/
/*                           GetIntSubfield()                           */
/************************************************************************/

/**
 * Fetch value of a subfield as an integer. This is a convenience
 * function for fetching a subfield of a field within this record.
 *
 * @param poField The field containing the subfield.
 * @param iSubfieldIndex The instance of this subfield within the record.
 * Use zero for the first instance.
 * @param pnSuccess Pointer to an int which will be set to TRUE if the fetch
 * succeeds, or FALSE if it fails. Use NULL if you don't want to check
 * success.
 * @return The value of the subfield, or zero if it failed for some reason.
 */

int DDFRecord::GetIntSubfield(const DDFField *poField, const char *pszSubfield,
                              int iSubfieldIndex, int *pnSuccess) const

{
    int nDummyErr = FALSE;

    if (pnSuccess == nullptr)
        pnSuccess = &nDummyErr;

    *pnSuccess = FALSE;

    /* -------------------------------------------------------------------- */
    /*      Get the subfield definition                                     */
    /* -------------------------------------------------------------------- */
    auto [poPartField, poSFDefn] =
        FindSubfieldDefn(poField, pszSubfield, false);
    if (poSFDefn == nullptr)
        return 0;
    if (poPartField)
        poField = poPartField;

    /* -------------------------------------------------------------------- */
    /*      Get a pointer to the data.                                      */
    /* -------------------------------------------------------------------- */
    int nBytesRemaining;

    const char *l_pachData =
        poField->GetSubfieldData(poSFDefn, &nBytesRemaining, iSubfieldIndex);
    if (l_pachData == nullptr)
        return 0;

    /* -------------------------------------------------------------------- */
    /*      Return the extracted value.                                     */
    /*                                                                      */
    /*      Assume an error has occurred if no bytes are consumed.           */
    /* -------------------------------------------------------------------- */
    int nConsumedBytes = 0;
    int nResult =
        poSFDefn->ExtractIntData(l_pachData, nBytesRemaining, &nConsumedBytes);

    if (nConsumedBytes > 0)
        *pnSuccess = TRUE;

    return nResult;
}

/************************************************************************/
/*                           GetIntSubfield()                           */
/************************************************************************/

/**
 * Fetch value of a subfield as an integer. This is a convenience
 * function for fetching a subfield of a field within this record.
 *
 * @param pszField The name of the field containing the subfield.
 * @param iFieldIndex The instance of this field within the record. Use
 * zero for the first instance of this field.
 * @param pszSubfield The name of the subfield within the selected field.
 * @param iSubfieldIndex The instance of this subfield within the record.
 * Use zero for the first instance.
 * @param pnSuccess Pointer to an int which will be set to TRUE if the fetch
 * succeeds, or FALSE if it fails. Use NULL if you don't want to check
 * success.
 * @return The value of the subfield, or zero if it failed for some reason.
 */

int DDFRecord::GetIntSubfield(const char *pszField, int iFieldIndex,
                              const char *pszSubfield, int iSubfieldIndex,
                              int *pnSuccess) const

{
    const DDFField *poField = FindField(pszField, iFieldIndex);
    if (poField == nullptr)
    {
        if (pnSuccess)
            *pnSuccess = FALSE;
        return 0;
    }

    return GetIntSubfield(poField, pszSubfield, iSubfieldIndex, pnSuccess);
}

/************************************************************************/
/*                          GetFloatSubfield()                          */
/************************************************************************/

/**
 * Fetch value of a subfield as a float (double). This is a convenience
 * function for fetching a subfield of a field within this record.
 *
 * @param pszField The name of the field containing the subfield.
 * @param iFieldIndex The instance of this field within the record. Use
 * zero for the first instance of this field.
 * @param pszSubfield The name of the subfield within the selected field.
 * @param iSubfieldIndex The instance of this subfield within the record.
 * Use zero for the first instance.
 * @param pnSuccess Pointer to an int which will be set to TRUE if the fetch
 * succeeds, or FALSE if it fails. Use NULL if you don't want to check
 * success.
 * @return The value of the subfield, or zero if it failed for some reason.
 */

double DDFRecord::GetFloatSubfield(const char *pszField, int iFieldIndex,
                                   const char *pszSubfield, int iSubfieldIndex,
                                   int *pnSuccess) const

{
    int nDummyErr = FALSE;

    if (pnSuccess == nullptr)
        pnSuccess = &nDummyErr;

    *pnSuccess = FALSE;

    /* -------------------------------------------------------------------- */
    /*      Fetch the field. If this fails, return zero.                    */
    /* -------------------------------------------------------------------- */
    auto [poField, poPartField, poSFDefn] =
        FindSubfieldDefn(pszField, iFieldIndex, pszSubfield, false);
    if (poSFDefn == nullptr)
        return 0;
    if (poPartField)
        poField = poPartField;

    /* -------------------------------------------------------------------- */
    /*      Get a pointer to the data.                                      */
    /* -------------------------------------------------------------------- */
    int nBytesRemaining;

    const char *l_pachData =
        poField->GetSubfieldData(poSFDefn, &nBytesRemaining, iSubfieldIndex);
    if (l_pachData == nullptr)
        return 0;

    /* -------------------------------------------------------------------- */
    /*      Return the extracted value.                                     */
    /* -------------------------------------------------------------------- */
    int nConsumedBytes = 0;
    double dfResult = poSFDefn->ExtractFloatData(l_pachData, nBytesRemaining,
                                                 &nConsumedBytes);

    if (nConsumedBytes > 0)
        *pnSuccess = TRUE;

    return dfResult;
}

/************************************************************************/
/*                         GetStringSubfield()                          */
/************************************************************************/

/**
 * Fetch value of a subfield as a string. This is a convenience
 * function for fetching a subfield of a field within this record.
 *
 * @param poField The field containing the subfield.
 * @param pszSubfield The name of the subfield within the selected field.
 * @param iSubfieldIndex The instance of this subfield within the record.
 * Use zero for the first instance.
 * @param pnSuccess Pointer to an int which will be set to TRUE if the fetch
 * succeeds, or FALSE if it fails. Use NULL if you don't want to check
 * success.
 * @return The value of the subfield, or NULL if it failed for some reason.
 * The returned pointer is to internal data and should not be modified or
 * freed by the application.
 */

const char *DDFRecord::GetStringSubfield(const DDFField *poField,
                                         const char *pszSubfield,
                                         int iSubfieldIndex,
                                         int *pnSuccess) const

{
    int nDummyErr = FALSE;

    if (pnSuccess == nullptr)
        pnSuccess = &nDummyErr;

    *pnSuccess = FALSE;

    /* -------------------------------------------------------------------- */
    /*      Fetch the field. If this fails, return zero.                    */
    /* -------------------------------------------------------------------- */
    auto [poPartField, poSFDefn] =
        FindSubfieldDefn(poField, pszSubfield, false);
    if (poSFDefn == nullptr)
        return nullptr;
    if (poPartField)
        poField = poPartField;

    /* -------------------------------------------------------------------- */
    /*      Get a pointer to the data.                                      */
    /* -------------------------------------------------------------------- */
    int nBytesRemaining;

    const char *l_pachData =
        poField->GetSubfieldData(poSFDefn, &nBytesRemaining, iSubfieldIndex);
    if (l_pachData == nullptr)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Return the extracted value.                                     */
    /* -------------------------------------------------------------------- */
    *pnSuccess = TRUE;

    return poSFDefn->ExtractStringData(l_pachData, nBytesRemaining, nullptr);
}

/************************************************************************/
/*                         GetStringSubfield()                          */
/************************************************************************/

/**
 * Fetch value of a subfield as a string. This is a convenience
 * function for fetching a subfield of a field within this record.
 *
 * @param pszField The name of the field containing the subfield.
 * @param iFieldIndex The instance of this field within the record. Use
 * zero for the first instance of this field.
 * @param pszSubfield The name of the subfield within the selected field.
 * @param iSubfieldIndex The instance of this subfield within the record.
 * Use zero for the first instance.
 * @param pnSuccess Pointer to an int which will be set to TRUE if the fetch
 * succeeds, or FALSE if it fails. Use NULL if you don't want to check
 * success.
 * @return The value of the subfield, or NULL if it failed for some reason.
 * The returned pointer is to internal data and should not be modified or
 * freed by the application.
 */

const char *DDFRecord::GetStringSubfield(const char *pszField, int iFieldIndex,
                                         const char *pszSubfield,
                                         int iSubfieldIndex,
                                         int *pnSuccess) const

{
    const DDFField *poField = FindField(pszField, iFieldIndex);
    if (poField == nullptr)
    {
        if (pnSuccess)
            *pnSuccess = FALSE;
        return nullptr;
    }

    return GetStringSubfield(poField, pszSubfield, iSubfieldIndex, pnSuccess);
}

/************************************************************************/
/*                               Clone()                                */
/************************************************************************/

/**
 * Make a copy of a record.
 *
 * This method is used to make a copy of a record that will become (mostly)
 * the properly of application.
 *
 * @return A new copy of the DDFRecord. Its lifetime must not extend the one
 * of the DDFModule of the original record, unless TransferTo() is called.
 */

std::unique_ptr<DDFRecord> DDFRecord::Clone() const

{
    auto poNR = std::make_unique<DDFRecord>(poModule);

    poNR->bReuseHeader = false;
    poNR->nFieldOffset = nFieldOffset;

    poNR->osData = osData;

    poNR->apoFields.resize(apoFields.size());
    for (size_t i = 0; i < apoFields.size(); i++)
    {
        int nOffset;

        nOffset = static_cast<int>(apoFields[i]->GetData() - osData.c_str());
        poNR->apoFields[i] = std::make_unique<DDFField>();
        poNR->apoFields[i]->Initialize(apoFields[i]->GetFieldDefn(),
                                       poNR->osData.c_str() + nOffset,
                                       apoFields[i]->GetDataSize(), true);
    }

    return poNR;
}

/************************************************************************/
/*                             TransferTo()                             */
/************************************************************************/

/**
 * Transfer this record to another module.
 *
 * All DDFFieldDefn
 * references are transcribed onto the new module based on field names.
 * If any fields don't have a similarly named field on the target module
 * the operation will fail.  No validation of field types and properties
 * is done, but this operation is intended only to be used between
 * modules with matching definitions of all affected fields.
 *
 * @param poTargetModule the module to which the record should be transferred
 *
 * @return true on success
 */

bool DDFRecord::TransferTo(DDFModule *poTargetModule)

{
    /* -------------------------------------------------------------------- */
    /*      Verify that all fields have a corresponding field definition    */
    /*      on the target module.                                           */
    /* -------------------------------------------------------------------- */
    for (const auto &poField : apoFields)
    {
        const DDFFieldDefn *poDefn = poField->GetFieldDefn();

        if (poTargetModule->FindFieldDefn(poDefn->GetName()) == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find field definition %s in target module",
                     poDefn->GetName());
            return false;
        }

        //TODO? check equality between source and target field definitions
    }

    /* -------------------------------------------------------------------- */
    /*      Update all internal information to reference other module.      */
    /* -------------------------------------------------------------------- */
    for (auto &poField : apoFields)
    {
        DDFFieldDefn *poDefn =
            poTargetModule->FindFieldDefn(poField->GetFieldDefn()->GetName());

        poField->Initialize(poDefn, poField->GetData(), poField->GetDataSize(),
                            true);
    }

    poModule = poTargetModule;

    return true;
}

/************************************************************************/
/*                            DeleteField()                             */
/************************************************************************/

/**
 * Delete a field instance from a record.
 *
 * Remove a field from this record, cleaning up the data
 * portion and repacking the fields list.  We don't try to
 * reallocate the data area of the record to be smaller.
 *
 * NOTE: This method doesn't actually remove the header
 * information for this field from the record tag list yet.
 * This should be added if the resulting record is even to be
 * written back to disk!
 *
 * @param poTarget the field instance on this record to delete.
 *
 * @return TRUE on success, or FALSE on failure.  Failure can occur if
 * poTarget isn't really a field on this record.
 */

int DDFRecord::DeleteField(DDFField *poTarget)

{
    int iTarget;

    /* -------------------------------------------------------------------- */
    /*      Find which field we are to delete.                              */
    /* -------------------------------------------------------------------- */
    for (iTarget = 0; iTarget < GetFieldCount(); iTarget++)
    {
        if (apoFields[iTarget].get() == poTarget)
            break;
    }

    if (iTarget == GetFieldCount())
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      Change the target fields data size to zero. This takes care     */
    /*      of repacking the data array, and updating all the following     */
    /*      field data pointers.                                            */
    /* -------------------------------------------------------------------- */
    ResizeField(poTarget, 0);

    /* -------------------------------------------------------------------- */
    /*      remove the target field, moving down all the other fields       */
    /*      one step in the field list.                                     */
    /* -------------------------------------------------------------------- */
    apoFields.erase(apoFields.begin() + iTarget);

    for (auto &poField : apoFields)
        poField->InitializeParts();

    return TRUE;
}

/************************************************************************/
/*                            ResizeField()                             */
/************************************************************************/

/**
 * Alter field data size within record.
 *
 * This method will rearrange a DDFRecord altering the amount of space
 * reserved for one of the existing fields.  All following fields will
 * be shifted accordingly. This includes updating the DDFField infos,
 * and actually moving stuff within the data array after reallocating
 * to the desired size.
 *
 * @param poField the field to alter.
 * @param nNewDataSize the number of data bytes to be reserved for the field.
 *
 * @return TRUE on success or FALSE on failure.
 */

int DDFRecord::ResizeField(DDFField *poField, int nNewDataSize)

{
    /* -------------------------------------------------------------------- */
    /*      Find which field we are to resize.                              */
    /* -------------------------------------------------------------------- */
    int iTarget;
    for (iTarget = 0; iTarget < GetFieldCount(); iTarget++)
    {
        if (apoFields[iTarget].get() == poField)
            break;
    }

    if (iTarget == GetFieldCount())
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      How much data needs to be shifted up or down after this field?  */
    /* -------------------------------------------------------------------- */
    const int nOldDataSize = poField->GetDataSize();
    const int nBytesToMove =
        static_cast<int>(osData.size()) -
        static_cast<int>(poField->GetData() + nOldDataSize - osData.data());

    /* -------------------------------------------------------------------- */
    /*      Store field offsets                                             */
    /* -------------------------------------------------------------------- */
    std::vector<int> anOffsets;
    for (auto &poIterField : apoFields)
    {
        anOffsets.push_back(
            static_cast<int>(poIterField->GetData() - osData.data()));
    }

    /* -------------------------------------------------------------------- */
    /*      Reallocate the data buffer accordingly.                         */
    /* -------------------------------------------------------------------- */
    // Don't realloc things smaller ... we will cut off some data.
    const int nBytesToAdd = nNewDataSize - poField->GetDataSize();
    if (nBytesToAdd > 0)
    {
        osData.resize(osData.size() + nBytesToAdd);

        /* ---------------------------------------------------------------- */
        /*      Update the target fields info.                              */
        /* ---------------------------------------------------------------- */
        poField->Initialize(poField->GetFieldDefn(),
                            osData.c_str() + anOffsets[iTarget], nNewDataSize,
                            false);
    }

    /* -------------------------------------------------------------------- */
    /*      Shift the data beyond this field up or down as needed.          */
    /* -------------------------------------------------------------------- */
    if (nBytesToMove > 0)
        memmove(const_cast<char *>(poField->GetData()) + nNewDataSize,
                poField->GetData() + nOldDataSize, nBytesToMove);

    /* -------------------------------------------------------------------- */
    /*      Shift all following fields down, and update their data          */
    /*      locations.                                                      */
    /* -------------------------------------------------------------------- */
    if (nBytesToAdd < 0)
    {
        osData.resize(osData.size() - (-nBytesToAdd));

        /* ---------------------------------------------------------------- */
        /*      Update the target fields info.                              */
        /* ---------------------------------------------------------------- */
        poField->Initialize(poField->GetFieldDefn(),
                            osData.c_str() + anOffsets[iTarget], nNewDataSize,
                            false);
    }

    /* -------------------------------------------------------------------- */
    /*      Update fields up to the resized one to point into newly         */
    /*      allocated buffer.                                               */
    /* -------------------------------------------------------------------- */
    for (int i = 0; i < iTarget; i++)
    {
        auto &poIterField = apoFields[i];
        poIterField->Initialize(poIterField->GetFieldDefn(),
                                osData.c_str() + anOffsets[i],
                                poIterField->GetDataSize(), false);
    }

    /* -------------------------------------------------------------------- */
    /*      Shift all following fields down, and update their data          */
    /*      locations.                                                      */
    /* -------------------------------------------------------------------- */
    for (int i = iTarget + 1; i < GetFieldCount(); i++)
    {
        apoFields[i]->Initialize(apoFields[i]->GetFieldDefn(),
                                 osData.c_str() + anOffsets[i] + nBytesToAdd,
                                 apoFields[i]->GetDataSize(), false);
    }

    return TRUE;
}

/************************************************************************/
/*                              AddField()                              */
/************************************************************************/

/**
 * Add a new field to record.
 *
 * Add a new zero sized field to the record.  The new field is always
 * added at the end of the record.
 *
 * NOTE: This method doesn't currently update the header information for
 * the record to include the field information for this field, so the
 * resulting record image isn't suitable for writing to disk.  However,
 * everything else about the record state should be updated properly to
 * reflect the new field.
 *
 * @param poDefn the definition of the field to be added.
 *
 * @return the field object on success, or NULL on failure.
 */

DDFField *DDFRecord::AddField(const DDFFieldDefn *poDefn)

{
    /* -------------------------------------------------------------------- */
    /*      Reallocate the fields array larger by one, and initialize       */
    /*      the new field.                                                  */
    /* -------------------------------------------------------------------- */
    apoFields.resize(apoFields.size() + 1);
    apoFields.back() = std::make_unique<DDFField>();

    /* -------------------------------------------------------------------- */
    /*      Initialize the new field properly.                              */
    /* -------------------------------------------------------------------- */
    if (apoFields.size() == 1)
    {
        apoFields[0]->Initialize(poDefn, GetData(), 0, false);
    }
    else
    {
        apoFields.back()->Initialize(
            poDefn,
            apoFields[GetFieldCount() - 2]->GetData() +
                apoFields[GetFieldCount() - 2]->GetDataSize(),
            0, false);
    }

    /* -------------------------------------------------------------------- */
    /*      Initialize field.                                               */
    /* -------------------------------------------------------------------- */
    CreateDefaultFieldInstance(apoFields[GetFieldCount() - 1].get(), 0);

    return apoFields.back().get();
}

/************************************************************************/
/*                            SetFieldRaw()                             */
/************************************************************************/

/**
 * Set the raw contents of a field instance.
 *
 * @param poField the field to set data within.
 * @param iIndexWithinField The instance of this field to replace.  Must
 * be a value between 0 and GetRepeatCount().  If GetRepeatCount() is used, a
 * new instance of the field is appended.
 * @param pachRawData the raw data to replace this field instance with.
 * @param nRawDataSize the number of bytes pointed to by pachRawData.
 *
 * @return TRUE on success or FALSE on failure.
 */

int DDFRecord::SetFieldRaw(DDFField *poField, int iIndexWithinField,
                           const char *pachRawData, int nRawDataSize)

{
    int iTarget, nRepeatCount;

    /* -------------------------------------------------------------------- */
    /*      Find which field we are to update.                              */
    /* -------------------------------------------------------------------- */
    for (iTarget = 0; iTarget < GetFieldCount(); iTarget++)
    {
        if (apoFields[iTarget].get() == poField)
            break;
    }

    if (iTarget == GetFieldCount())
        return FALSE;

    nRepeatCount = poField->GetRepeatCount();

    if (iIndexWithinField < 0 || iIndexWithinField > nRepeatCount)
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      Are we adding an instance? This is easier and different         */
    /*      than replacing an existing instance.                            */
    /* -------------------------------------------------------------------- */
    if ((iIndexWithinField == nRepeatCount ||
         !poField->GetFieldDefn()->IsRepeating()) &&
        !(nRepeatCount == 0 && poField->GetDataSize() > 0))
    {
        if (!poField->GetFieldDefn()->IsRepeating() && iIndexWithinField != 0)
            return FALSE;

        int nOldSize = poField->GetDataSize();
        if (nOldSize == 0)
            nOldSize++;  // for added DDF_FIELD_TERMINATOR.

        if (!ResizeField(poField, nOldSize + nRawDataSize))
            return FALSE;

        char *pachFieldData = const_cast<char *>(poField->GetData());
        memcpy(pachFieldData + nOldSize - 1, pachRawData, nRawDataSize);
        pachFieldData[nOldSize + nRawDataSize - 1] = DDF_FIELD_TERMINATOR;

        for (auto &poIterField : apoFields)
            poIterField->InitializeParts();

        return TRUE;
    }

    /* -------------------------------------------------------------------- */
    /*      Get a pointer to the start of the existing data for this        */
    /*      iteration of the field.                                         */
    /* -------------------------------------------------------------------- */
    const char *pachWrkData = nullptr;
    int nInstanceSize = 0;

    // We special case this to avoid a lot of warnings when initializing
    // the field the first time.
    if (poField->GetDataSize() == 0)
    {
        pachWrkData = poField->GetData();
    }
    else
    {
        pachWrkData =
            poField->GetInstanceData(iIndexWithinField, &nInstanceSize);
    }

    /* -------------------------------------------------------------------- */
    /*      Create new image of this whole field.                           */
    /* -------------------------------------------------------------------- */
    int nNewFieldSize = poField->GetDataSize() - nInstanceSize + nRawDataSize;

    std::string osNewImage;
    osNewImage.resize(nNewFieldSize);

    int nPreBytes = static_cast<int>(pachWrkData - poField->GetData());
    int nPostBytes = poField->GetDataSize() - nPreBytes - nInstanceSize;

    memcpy(osNewImage.data(), poField->GetData(), nPreBytes);
    memcpy(osNewImage.data() + nPreBytes + nRawDataSize,
           poField->GetData() + nPreBytes + nInstanceSize, nPostBytes);
    memcpy(osNewImage.data() + nPreBytes, pachRawData, nRawDataSize);

    /* -------------------------------------------------------------------- */
    /*      Resize the field to the desired new size.                       */
    /* -------------------------------------------------------------------- */
    ResizeField(poField, nNewFieldSize);

    memcpy(const_cast<char *>(poField->GetData()), osNewImage.data(),
           nNewFieldSize);

    for (auto &poIterField : apoFields)
        poIterField->InitializeParts();

    return TRUE;
}

/************************************************************************/
/*                            SetFieldRaw()                             */
/************************************************************************/

/**
 * Set the raw contents of a field (all instances in case it is a repeated one)
 *
 * A DDF_FIELD_TERMINATOR will be automatically added at the end of the raw data
 * if not already present.
 *
 * @param poField the field to set data within.
 * @param pachRawData the raw data to replace this field with.
 * @param nRawDataSize the number of bytes pointed to by pachRawData.
 *
 * @return TRUE on success or FALSE on failure.
 */

int DDFRecord::SetFieldRaw(DDFField *poField, const char *pachRawData,
                           int nRawDataSize)

{
    const bool bAddFieldTerminator =
        (nRawDataSize == 0 ||
         pachRawData[nRawDataSize - 1] != DDF_FIELD_TERMINATOR);

    /* -------------------------------------------------------------------- */
    /*      Resize the field to the desired new size.                       */
    /* -------------------------------------------------------------------- */
    if (!ResizeField(poField, nRawDataSize + (bAddFieldTerminator ? 1 : 0)))
        return FALSE;

    memcpy(const_cast<char *>(poField->GetData()), pachRawData, nRawDataSize);
    if (bAddFieldTerminator)
        const_cast<char *>(poField->GetData())[nRawDataSize] =
            DDF_FIELD_TERMINATOR;

    for (auto &poIterField : apoFields)
        poIterField->InitializeParts();

    return TRUE;
}

/************************************************************************/
/*                           UpdateFieldRaw()                           */
/************************************************************************/

int DDFRecord::UpdateFieldRaw(DDFField *poField, DDFField *poPartField,
                              int iIndexWithinField, int nStartOffset,
                              int nOldSize, const char *pachRawData,
                              int nRawDataSize)

{
    int iTarget;

    /* -------------------------------------------------------------------- */
    /*      Find which field we are to update.                              */
    /* -------------------------------------------------------------------- */
    for (iTarget = 0; iTarget < GetFieldCount(); iTarget++)
    {
        if (apoFields[iTarget].get() == poField)
            break;
    }

    if (iTarget == GetFieldCount())
        return FALSE;

    auto poLowLevelField = poPartField ? poPartField : poField;
    const int nRepeatCount = poLowLevelField->GetRepeatCount();

    if (iIndexWithinField < 0 || iIndexWithinField >= nRepeatCount)
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      Figure out how much pre and post data there is.                 */
    /* -------------------------------------------------------------------- */
    char *const pachWrkData =
        const_cast<char *>(poLowLevelField->GetInstanceData(iIndexWithinField));
    const int nPreBytes =
        static_cast<int>(pachWrkData - poField->GetData() + nStartOffset);
    const int nPostBytes = poField->GetDataSize() - nPreBytes - nOldSize;

    /* -------------------------------------------------------------------- */
    /*      If we aren't changing the size, just copy over the existing     */
    /*      data.                                                           */
    /* -------------------------------------------------------------------- */
    if (nOldSize == nRawDataSize)
    {
        memcpy(pachWrkData + nStartOffset, pachRawData, nRawDataSize);
        return TRUE;
    }

    /* -------------------------------------------------------------------- */
    /*      If we are shrinking, move in the new data, and shuffle down     */
    /*      the old before resizing.                                        */
    /* -------------------------------------------------------------------- */
    char *pabyFieldData = const_cast<char *>(poField->GetData());
    if (nRawDataSize < nOldSize)
    {
        memcpy(pabyFieldData + nPreBytes, pachRawData, nRawDataSize);
        memmove(pabyFieldData + nPreBytes + nRawDataSize,
                pabyFieldData + nPreBytes + nOldSize, nPostBytes);
    }

    /* -------------------------------------------------------------------- */
    /*      Resize the whole buffer.                                        */
    /* -------------------------------------------------------------------- */
    if (!ResizeField(poField, poField->GetDataSize() - nOldSize + nRawDataSize))
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      If we growing the buffer, shuffle up the post data, and         */
    /*      move in our new values.                                         */
    /* -------------------------------------------------------------------- */
    pabyFieldData = const_cast<char *>(poField->GetData());
    if (nRawDataSize >= nOldSize)
    {
        memmove(pabyFieldData + nPreBytes + nRawDataSize,
                pabyFieldData + nPreBytes + nOldSize, nPostBytes);
        memcpy(pabyFieldData + nPreBytes, pachRawData, nRawDataSize);
    }

    for (auto &poIterField : apoFields)
        poIterField->InitializeParts();

    return TRUE;
}

/************************************************************************/
/*                           ResetDirectory()                           */
/*                                                                      */
/*      Re-prepares the directory information for the record.           */
/************************************************************************/

void DDFRecord::ResetDirectory()

{
    /* -------------------------------------------------------------------- */
    /*      Eventually we should try to optimize the size of offset and     */
    /*      field length.                                                   */
    /* -------------------------------------------------------------------- */

    /* -------------------------------------------------------------------- */
    /*      Compute how large the directory needs to be.                    */
    /* -------------------------------------------------------------------- */
    int nEntrySize, nDirSize;

    nEntrySize = _sizeFieldPos + _sizeFieldLength + _sizeFieldTag;
    nDirSize = nEntrySize * GetFieldCount() + 1;

    /* -------------------------------------------------------------------- */
    /*      If the directory size is different than what is currently       */
    /*      reserved for it, we must resize.                                */
    /* -------------------------------------------------------------------- */
    if (nDirSize != nFieldOffset)
    {
        const int nNewDataSize =
            static_cast<int>(osData.size()) - nFieldOffset + nDirSize;
        std::string osNewData;
        osNewData.resize(nNewDataSize);
        memcpy(osNewData.data() + nDirSize, osData.c_str() + nFieldOffset,
               nNewDataSize - nDirSize);

        std::vector<int> anOffsets;
        for (auto &poField : apoFields)
        {
            anOffsets.push_back(static_cast<int>(
                poField->GetData() - osData.c_str() - nFieldOffset + nDirSize));
        }

        osData = std::move(osNewData);
        nFieldOffset = nDirSize;

        for (size_t i = 0; i < apoFields.size(); ++i)
        {
            auto &poField = apoFields[i];
            poField->Initialize(poField->GetFieldDefn(),
                                osData.c_str() + anOffsets[i],
                                poField->GetDataSize(), true);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Now set each directory entry.                                   */
    /* -------------------------------------------------------------------- */
    int iField = 0;
    for (auto &poField : apoFields)
    {
        const DDFFieldDefn *poDefn = poField->GetFieldDefn();
        char szFormat[128];

        snprintf(szFormat, sizeof(szFormat), "%%%ds%%0%dd%%0%dd", _sizeFieldTag,
                 _sizeFieldLength, _sizeFieldPos);

        snprintf(osData.data() + nEntrySize * iField, nEntrySize + 1, szFormat,
                 poDefn->GetName(), poField->GetDataSize(),
                 poField->GetData() - osData.data() - nFieldOffset);
        ++iField;
    }

    osData[nEntrySize * GetFieldCount()] = DDF_FIELD_TERMINATOR;
}

/************************************************************************/
/*                     CreateDefaultFieldInstance()                     */
/************************************************************************/

/**
 * Initialize default instance.
 *
 * This method is normally only used internally by the AddField() method
 * to initialize the new field instance with default subfield values.  It
 * installs default data for one instance of the field in the record
 * using the DDFFieldDefn::GetDefaultValue() method and
 * DDFRecord::SetFieldRaw().
 *
 * @param poField the field within the record to be assign a default
 * instance.
 * @param iIndexWithinField the instance to set (may not have been tested with
 * values other than 0).
 *
 * @return TRUE on success or FALSE on failure.
 */

int DDFRecord::CreateDefaultFieldInstance(DDFField *poField,
                                          int iIndexWithinField)

{
    int nRawSize = 0;
    char *pachRawData = poField->GetFieldDefn()->GetDefaultValue(&nRawSize);
    if (pachRawData == nullptr)
        return FALSE;

    const int nSuccess =
        SetFieldRaw(poField, iIndexWithinField, pachRawData, nRawSize);

    CPLFree(pachRawData);

    return nSuccess;
}

/************************************************************************/
/*                   GetSubfieldDataForSetSubfield()                    */
/************************************************************************/

char *DDFRecord::GetSubfieldDataForSetSubfield(DDFField *poField,
                                               DDFField *poPartField,
                                               const DDFSubfieldDefn *poSFDefn,
                                               int iSubfieldIndex,
                                               int &nMaxBytes)
{
    nMaxBytes = 0;

    char *pachSubfieldData = nullptr;

    const auto poLowLevelField = poPartField ? poPartField : poField;
    if (poPartField && poLowLevelField->GetFieldDefn()->IsRepeating() &&
        iSubfieldIndex == poLowLevelField->GetRepeatCount())
    {
        if (poPartField != poField->GetParts().back().get())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Can only append new field values to last part of field");
            return nullptr;
        }

        int nRawSize = 0;
        char *pachRawData =
            poLowLevelField->GetFieldDefn()->GetDefaultValue(&nRawSize);
        if (pachRawData == nullptr)
            return nullptr;

        std::string osNewData(poField->GetData(), poField->GetDataSize());
        if (!osNewData.empty() && osNewData.back() == DDF_FIELD_TERMINATOR)
            osNewData.pop_back();
        osNewData.append(pachRawData, nRawSize);
        osNewData += static_cast<char>(DDF_FIELD_TERMINATOR);
        CPLFree(pachRawData);

        if (!SetFieldRaw(poField, 0, osNewData.c_str(),
                         static_cast<int>(osNewData.size())))
            return nullptr;

        pachSubfieldData = const_cast<char *>(poLowLevelField->GetSubfieldData(
            poSFDefn, &nMaxBytes, iSubfieldIndex));
        if (pachSubfieldData == nullptr)
            return nullptr;
    }
    else
    {
        pachSubfieldData = const_cast<char *>(poLowLevelField->GetSubfieldData(
            poSFDefn, &nMaxBytes, iSubfieldIndex));
        if (pachSubfieldData == nullptr)
            return nullptr;

        /* -------------------------------------------------------------------- */
        /*      Add new instance if we have run out of data.                    */
        /* -------------------------------------------------------------------- */
        if (poPartField == nullptr &&
            (nMaxBytes == 0 ||
             (nMaxBytes == 1 && pachSubfieldData[0] == DDF_FIELD_TERMINATOR)))
        {
            CreateDefaultFieldInstance(poField, iSubfieldIndex);

            // Refetch.
            pachSubfieldData = const_cast<char *>(
                poField->GetSubfieldData(poSFDefn, &nMaxBytes, iSubfieldIndex));
            if (pachSubfieldData == nullptr)
                return nullptr;
        }
    }

    return pachSubfieldData;
}

/************************************************************************/
/*                         SetStringSubfield()                          */
/************************************************************************/

/**
 * Set a string subfield in record.
 *
 * The value of a given subfield is replaced with a new string value
 * formatted appropriately.
 *
 * @param pszField the field name to operate on.
 * @param iFieldIndex the field index to operate on (zero based).
 * @param pszSubfield the subfield name to operate on.
 * @param iSubfieldIndex the subfield index to operate on (zero based).
 * @param pszValue the new string to place in the subfield. This may be
 * arbitrary binary bytes if nValueLength is specified.
 * @param nValueLength the number of valid bytes in pszValue, may be -1 to
 * internally fetch with strlen().
 *
 * @return TRUE if successful, and FALSE if not.
 */

int DDFRecord::SetStringSubfield(const char *pszField, int iFieldIndex,
                                 const char *pszSubfield, int iSubfieldIndex,
                                 const char *pszValue, int nValueLength)

{
    /* -------------------------------------------------------------------- */
    /*      Get the subfield definition                                     */
    /* -------------------------------------------------------------------- */
    auto [poField, poPartField, poSFDefn] =
        FindSubfieldDefn(pszField, iFieldIndex, pszSubfield);
    if (!poSFDefn)
    {
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      How long will the formatted value be?                           */
    /* -------------------------------------------------------------------- */
    int nFormattedLen;

    if (!poSFDefn->FormatStringValue(nullptr, 0, &nFormattedLen, pszValue,
                                     nValueLength))
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      Get a pointer to the data.                                      */
    /* -------------------------------------------------------------------- */
    int nMaxBytes = 0;
    char *pachSubfieldData = GetSubfieldDataForSetSubfield(
        poField, poPartField, poSFDefn, iSubfieldIndex, nMaxBytes);
    if (!pachSubfieldData)
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      If the new length matches the existing length, just overlay     */
    /*      and return.                                                     */
    /* -------------------------------------------------------------------- */
    int nExistingLength;

    poSFDefn->GetDataLength(pachSubfieldData, nMaxBytes, &nExistingLength);

    if (nExistingLength == nFormattedLen)
    {
        return poSFDefn->FormatStringValue(pachSubfieldData, nFormattedLen,
                                           nullptr, pszValue, nValueLength);
    }

    /* -------------------------------------------------------------------- */
    /*      We will need to resize the raw data.                            */
    /* -------------------------------------------------------------------- */
    const auto poLowLevelField = poPartField ? poPartField : poField;
    const char *pachFieldInstData =
        poLowLevelField->GetInstanceData(iSubfieldIndex);

    const int nStartOffset =
        static_cast<int>(pachSubfieldData - pachFieldInstData);

    std::string osNewData;
    osNewData.resize(nFormattedLen);
    poSFDefn->FormatStringValue(osNewData.data(), nFormattedLen, nullptr,
                                pszValue, nValueLength);

    return UpdateFieldRaw(poField, poPartField, iSubfieldIndex, nStartOffset,
                          nExistingLength, osNewData.data(), nFormattedLen);
}

/************************************************************************/
/*                           SetIntSubfield()                           */
/************************************************************************/

/**
 * Set an integer subfield in record.
 *
 * The value of a given subfield is replaced with a new integer value
 * formatted appropriately.
 *
 * @param pszField the field name to operate on.
 * @param iFieldIndex the field index to operate on (zero based).
 * @param pszSubfield the subfield name to operate on.
 * @param iSubfieldIndex the subfield index to operate on (zero based).
 * @param nNewValue the new value to place in the subfield.
 *
 * @return TRUE if successful, and FALSE if not.
 */

int DDFRecord::SetIntSubfield(const char *pszField, int iFieldIndex,
                              const char *pszSubfield, int iSubfieldIndex,
                              int nNewValue)

{
    /* -------------------------------------------------------------------- */
    /*      Get the subfield definition                                     */
    /* -------------------------------------------------------------------- */
    auto [poField, poPartField, poSFDefn] =
        FindSubfieldDefn(pszField, iFieldIndex, pszSubfield);
    if (!poSFDefn)
    {
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      How long will the formatted value be?                           */
    /* -------------------------------------------------------------------- */
    int nFormattedLen;

    if (!poSFDefn->FormatIntValue(nullptr, 0, &nFormattedLen, nNewValue))
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      Get a pointer to the data.                                      */
    /* -------------------------------------------------------------------- */
    int nMaxBytes = 0;
    char *pachSubfieldData = GetSubfieldDataForSetSubfield(
        poField, poPartField, poSFDefn, iSubfieldIndex, nMaxBytes);
    if (!pachSubfieldData)
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      If the new length matches the existing length, just overlay     */
    /*      and return.                                                     */
    /* -------------------------------------------------------------------- */
    int nExistingLength;

    poSFDefn->GetDataLength(pachSubfieldData, nMaxBytes, &nExistingLength);

    if (nExistingLength == nFormattedLen)
    {
        return poSFDefn->FormatIntValue(pachSubfieldData, nFormattedLen,
                                        nullptr, nNewValue);
    }

    /* -------------------------------------------------------------------- */
    /*      We will need to resize the raw data.                            */
    /* -------------------------------------------------------------------- */
    const auto poLowLevelField = poPartField ? poPartField : poField;
    const char *pachFieldInstData =
        poLowLevelField->GetInstanceData(iSubfieldIndex);

    const int nStartOffset =
        static_cast<int>(pachSubfieldData - pachFieldInstData);

    std::string osNewData;
    osNewData.resize(nFormattedLen);
    poSFDefn->FormatIntValue(osNewData.data(), nFormattedLen, nullptr,
                             nNewValue);

    return UpdateFieldRaw(poField, poPartField, iSubfieldIndex, nStartOffset,
                          nExistingLength, osNewData.data(), nFormattedLen);
}

/************************************************************************/
/*                          SetFloatSubfield()                          */
/************************************************************************/

/**
 * Set a float subfield in record.
 *
 * The value of a given subfield is replaced with a new float value
 * formatted appropriately.
 *
 * @param pszField the field name to operate on.
 * @param iFieldIndex the field index to operate on (zero based).
 * @param pszSubfield the subfield name to operate on.
 * @param iSubfieldIndex the subfield index to operate on (zero based).
 * @param dfNewValue the new value to place in the subfield.
 *
 * @return TRUE if successful, and FALSE if not.
 */

int DDFRecord::SetFloatSubfield(const char *pszField, int iFieldIndex,
                                const char *pszSubfield, int iSubfieldIndex,
                                double dfNewValue)

{
    /* -------------------------------------------------------------------- */
    /*      Get the subfield definition                                     */
    /* -------------------------------------------------------------------- */
    auto [poField, poPartField, poSFDefn] =
        FindSubfieldDefn(pszField, iFieldIndex, pszSubfield);
    if (!poSFDefn)
    {
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      How long will the formatted value be?                           */
    /* -------------------------------------------------------------------- */
    int nFormattedLen;

    if (!poSFDefn->FormatFloatValue(nullptr, 0, &nFormattedLen, dfNewValue))
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      Get a pointer to the data.                                      */
    /* -------------------------------------------------------------------- */
    int nMaxBytes = 0;
    char *pachSubfieldData = GetSubfieldDataForSetSubfield(
        poField, poPartField, poSFDefn, iSubfieldIndex, nMaxBytes);
    if (!pachSubfieldData)
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      If the new length matches the existing length, just overlay     */
    /*      and return.                                                     */
    /* -------------------------------------------------------------------- */
    int nExistingLength;

    poSFDefn->GetDataLength(pachSubfieldData, nMaxBytes, &nExistingLength);

    if (nExistingLength == nFormattedLen)
    {
        return poSFDefn->FormatFloatValue(pachSubfieldData, nFormattedLen,
                                          nullptr, dfNewValue);
    }

    /* -------------------------------------------------------------------- */
    /*      We will need to resize the raw data.                            */
    /* -------------------------------------------------------------------- */
    const auto poLowLevelField = poPartField ? poPartField : poField;
    const char *pachFieldInstData =
        poLowLevelField->GetInstanceData(iSubfieldIndex);

    const int nStartOffset =
        static_cast<int>(pachSubfieldData - pachFieldInstData);

    std::string osNewData;
    osNewData.resize(nFormattedLen);
    poSFDefn->FormatFloatValue(osNewData.data(), nFormattedLen, nullptr,
                               dfNewValue);

    return UpdateFieldRaw(poField, poPartField, iSubfieldIndex, nStartOffset,
                          nExistingLength, osNewData.data(), nFormattedLen);
}
