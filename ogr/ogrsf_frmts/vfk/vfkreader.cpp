/******************************************************************************
 *
 * Project:  VFK Reader
 * Purpose:  Implements VFKReader class.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2009-2018, Martin Landa <landa.martin gmail.com>
 * Copyright (c) 2012-2018, Even Rouault <even dot rouault at spatialys.com>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ****************************************************************************/

#include <sys/stat.h>

#include "vfkreader.h"
#include "vfkreaderp.h"

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"

#include "ogr_geometry.h"

CPL_CVSID("$Id$")

static char *GetDataBlockName(const char *);

/*!
  \brief IVFKReader destructor
*/
IVFKReader::~IVFKReader()
{
}

/*!
  \brief Create new instance of VFKReader

  \return pointer to VFKReader instance
*/
IVFKReader *CreateVFKReader( const GDALOpenInfo *poOpenInfo )
{
    return new VFKReaderSQLite( poOpenInfo );
}

/*!
  \brief VFKReader constructor
*/
VFKReader::VFKReader( const GDALOpenInfo* poOpenInfo ) :
    m_bLatin2(true),  // Encoding ISO-8859-2 or WINDOWS-1250.
    m_poFD(nullptr),
    m_pszFilename(CPLStrdup(poOpenInfo->pszFilename)),
    m_poFStat((VSIStatBufL*) CPLCalloc(1, sizeof(VSIStatBufL))),
    // VFK is provided in two forms - stative and amendment data.
    m_bAmendment(false),
    m_bFileField( CPLFetchBool( poOpenInfo->papszOpenOptions,
                                "FILE_FIELD", false ) ),
    m_nDataBlockCount(0),
    m_papoDataBlock(nullptr)
{
    // Open VFK file for reading.
    CPLAssert(nullptr != m_pszFilename);

    if (VSIStatL(m_pszFilename, m_poFStat) != 0 ||
        !VSI_ISREG(m_poFStat->st_mode)) {
      CPLError(CE_Failure, CPLE_OpenFailed,
               "%s is not a regular file.", m_pszFilename);
    }

    m_poFD = VSIFOpenL(m_pszFilename, "rb");
    if (m_poFD == nullptr) {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Failed to open file %s.", m_pszFilename);
    }
}

/*!
  \brief VFKReader destructor
*/
VFKReader::~VFKReader()
{
    CPLFree(m_pszFilename);

    if (m_poFD)
        VSIFCloseL(m_poFD);
    CPLFree(m_poFStat);

    /* clear data blocks */
    for (int i = 0; i < m_nDataBlockCount; i++)
        delete m_papoDataBlock[i];
    CPLFree(m_papoDataBlock);
}

char *GetDataBlockName(const char *pszLine)
{
    int n = 0; // Used after for.
    const char *pszLineChar = pszLine + 2;

    for( ; *pszLineChar != '\0' && *pszLineChar != ';'; pszLineChar++, n++)
        ;

    if( *pszLineChar == '\0' )
        return nullptr;

    char *pszBlockName = (char *) CPLMalloc(n + 1);
    strncpy(pszBlockName, pszLine + 2, n);
    pszBlockName[n] = '\0';

    return pszBlockName;
}

/*!
  \brief Read a line from file

  \return a NULL terminated string which should be freed with CPLFree().
*/
char *VFKReader::ReadLine()
{
    int nBufLength;
    const char *pszRawLine = CPLReadLine3L( m_poFD, 100 * 1024, &nBufLength, nullptr );
    if( pszRawLine == nullptr )
        return nullptr;

    char *pszLine = (char *) CPLMalloc(nBufLength + 1);
    memcpy(pszLine, pszRawLine, nBufLength + 1);

    const int nLineLength = static_cast<int>(strlen(pszRawLine));
    if( nLineLength != nBufLength ) {
        /* replace nul characters in line by spaces */
        for( int i = nLineLength; i < nBufLength; i++ ) {
            if( pszLine[i] == '\0' )
                pszLine[i] = ' ';
        }
    }

    return pszLine;
}

/*!
  \brief Load data block definitions (&B)

  Call VFKReader::OpenFile() before this function.

  \param bSuppressGeometry True for skipping geometry resolver (force wkbNone type)

  \return number of data blocks or -1 on error
*/
int VFKReader::ReadDataBlocks(bool bSuppressGeometry)
{
    CPLAssert(nullptr != m_pszFilename);

    VSIFSeekL(m_poFD, 0, SEEK_SET);
    bool bInHeader = true;
    char *pszLine = nullptr;
    while ((pszLine = ReadLine()) != nullptr) {
        if (strlen(pszLine) < 2 || pszLine[0] != '&') {
            CPLFree(pszLine);
            continue;
        }

        if (pszLine[1] == 'B') {
            if( bInHeader )
                bInHeader = false; /* 'B' record closes the header section */

            char *pszBlockName = GetDataBlockName(pszLine);
            if (pszBlockName == nullptr) {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Corrupted data - line\n%s\n", pszLine);
                CPLFree(pszLine);
                return -1;
            }

            /* skip duplicated data blocks (when reading multiple files into single DB)  */
            if( !GetDataBlock(pszBlockName) )
            {
                IVFKDataBlock *poNewDataBlock =
                    (IVFKDataBlock *) CreateDataBlock(pszBlockName);
                poNewDataBlock->SetGeometryType(bSuppressGeometry);
                poNewDataBlock->SetProperties(pszLine); /* TODO: check consistency on property level */

                AddDataBlock(poNewDataBlock, pszLine);
            }
            CPLFree(pszBlockName);
        }
        else if (pszLine[1] == 'H') {
            /* check for amendment file */
            if (EQUAL(pszLine, "&HZMENY;1")) {
                m_bAmendment = true;
            }

            /* header - metadata */
            AddInfo(pszLine);
        }
        else if (pszLine[1] == 'K' && strlen(pszLine) == 2) {
            /* end of file */
            CPLFree(pszLine);
            break;
        }
        else if( bInHeader && pszLine[1] == 'D' )
        {
            /* process 'D' records in the header section */
            AddInfo(pszLine);
        }

        CPLFree(pszLine);
    }

    return m_nDataBlockCount;
}

/*!
  \brief Load data records (&D)

  Call VFKReader::OpenFile() before this function.

  \param poDataBlock limit to selected data block or NULL for all

  \return number of data records or -1 on error
*/
int VFKReader::ReadDataRecords(IVFKDataBlock *poDataBlock)
{
    const char *pszName = nullptr;
    IVFKDataBlock *poDataBlockCurrent = nullptr;

    if (poDataBlock) {  /* read only given data block */
        poDataBlockCurrent = poDataBlock;
        if (poDataBlockCurrent->GetFeatureCount(FALSE) < 0)
            poDataBlockCurrent->SetFeatureCount(0);
        pszName = poDataBlockCurrent->GetName();
    }
    else {              /* read all data blocks */
        for (int iDataBlock = 0; iDataBlock < GetDataBlockCount(); iDataBlock++) {
            poDataBlockCurrent = GetDataBlock(iDataBlock);
            if (poDataBlockCurrent->GetFeatureCount(FALSE) < 0)
                poDataBlockCurrent->SetFeatureCount(0);
        }
        poDataBlockCurrent = nullptr;
    }

    VSIFSeekL(m_poFD, 0, SEEK_SET);

    int iLine = 0;
    int nSkipped = 0;
    int nDupl = 0;
    int nRecords = 0;
    bool bInHeader = true;
    CPLString osBlockNameLast;
    char *pszLine = nullptr;

    while ((pszLine = ReadLine()) != nullptr) {
        iLine++;
        size_t nLength = strlen(pszLine);
        if (nLength < 2) {
            CPLFree(pszLine);
            continue;
        }

        if (bInHeader && pszLine[1] == 'B')
            bInHeader = false; /* 'B' record closes the header section */

        if (pszLine[1] == 'D') {
            if (bInHeader) {
                /* skip 'D' records from the header section, already
                 * processed as metadata */
                CPLFree(pszLine);
                continue;
            }

            char *pszBlockName = GetDataBlockName(pszLine);

            if (pszBlockName && (!pszName || EQUAL(pszBlockName, pszName))) {
                /* merge lines if needed

                   See http://en.wikipedia.org/wiki/ISO/IEC_8859
                   - \244 - general currency sign
                */
                if (pszLine[nLength - 1] == '\244') {
                    /* remove \244 (currency sign) from string */
                    pszLine[nLength - 1] = '\0';

                    CPLString osMultiLine(pszLine);
                    CPLFree(pszLine);

                    while ((pszLine = ReadLine()) != nullptr &&
                           pszLine[0] != '\0' &&
                           pszLine[strlen(pszLine) - 1] == '\244') {
                        /* append line */
                        osMultiLine += pszLine;
                        /* remove 0244 (currency sign) from string */
                        osMultiLine.erase(osMultiLine.size() - 1);

                        CPLFree(pszLine);
                        if( osMultiLine.size() > 100U * 1024U * 1024U )
                        {
                            CPLFree(pszBlockName);
                            return -1;
                        }
                    }
                    if( pszLine )
                        osMultiLine += pszLine;
                    CPLFree(pszLine);

                    nLength = osMultiLine.size();
                    if( nLength > 100U * 1024U * 1024U )
                    {
                        CPLFree(pszBlockName);
                        return -1;
                    }
                    pszLine = (char *) CPLMalloc(nLength + 1);
                    strncpy(pszLine, osMultiLine.c_str(), nLength);
                    pszLine[nLength] = '\0';
                }

                if (!poDataBlock) { /* read all data blocks */
                    if (osBlockNameLast.empty() ||
                        !EQUAL(pszBlockName, osBlockNameLast.c_str())) {
                        poDataBlockCurrent = GetDataBlock(pszBlockName);
                        osBlockNameLast = CPLString(pszBlockName);
                    }
                }
                if (!poDataBlockCurrent) {
                    CPLFree(pszBlockName);
                    CPLFree(pszLine);
                    continue; // assert ?
                }

                VFKFeature *poNewFeature = new VFKFeature(poDataBlockCurrent,
                                                          poDataBlockCurrent->GetFeatureCount() + 1);
                if (poNewFeature->SetProperties(pszLine)) {
                    if (AddFeature(poDataBlockCurrent, poNewFeature) != OGRERR_NONE) {
                        CPLDebug( "OGR-VFK",
                                  "%s: duplicated VFK data record skipped "
                                  "(line %d).\n%s\n",
                                  pszBlockName, iLine, pszLine);
                        poDataBlockCurrent->SetIncRecordCount(RecordDuplicated);
                    }
                    else {
                        nRecords++;
                        poDataBlockCurrent->SetIncRecordCount(RecordValid);
                    }
                    delete poNewFeature;
                }
                else {
                    CPLDebug("OGR-VFK",
                             "Invalid VFK data record skipped (line %d).\n%s\n", iLine, pszLine);
                    poDataBlockCurrent->SetIncRecordCount(RecordSkipped);
                    delete poNewFeature;
                }
            }
            CPLFree(pszBlockName);
        }
        else if (pszLine[1] == 'K' && strlen(pszLine) == 2) {
            /* end of file */
            CPLFree(pszLine);
            break;
        }

        CPLFree(pszLine);
    }

    for (int iDataBlock = 0; iDataBlock < GetDataBlockCount(); iDataBlock++) {
        poDataBlockCurrent = GetDataBlock(iDataBlock);

        if (poDataBlock && poDataBlock != poDataBlockCurrent)
            continue;

        nSkipped = poDataBlockCurrent->GetRecordCount(RecordSkipped);
        nDupl    = poDataBlockCurrent->GetRecordCount(RecordDuplicated);
        if (nSkipped > 0)
            CPLError(CE_Warning, CPLE_AppDefined,
                     "%s: %d invalid VFK data records skipped",
                     poDataBlockCurrent->GetName(), nSkipped);
        if (nDupl > 0)
            CPLError(CE_Warning, CPLE_AppDefined,
                     "%s: %d duplicated VFK data records skipped",
                     poDataBlockCurrent->GetName(), nDupl);

        CPLDebug("OGR-VFK", "VFKReader::ReadDataRecords(): name=%s n=%d",
                 poDataBlockCurrent->GetName(),
                 poDataBlockCurrent->GetRecordCount(RecordValid));
    }

    return nRecords;
}

IVFKDataBlock *VFKReader::CreateDataBlock(const char *pszBlockName)
{
  return (IVFKDataBlock *) new VFKDataBlock(pszBlockName, (IVFKReader *) this);
}

/*!
  \brief Add new data block

  \param poNewDataBlock pointer to VFKDataBlock instance
  \param pszDefn unused (see VFKReaderSQLite::AddDataBlock)
*/
void VFKReader::AddDataBlock(IVFKDataBlock *poNewDataBlock,
                             CPL_UNUSED const char *pszDefn)
{
    m_nDataBlockCount++;

    m_papoDataBlock = (IVFKDataBlock **)
        CPLRealloc(m_papoDataBlock, sizeof (IVFKDataBlock *) * m_nDataBlockCount);
    m_papoDataBlock[m_nDataBlockCount-1] = poNewDataBlock;
}

/*!
  \brief Add feature

  \param poDataBlock pointer to VFKDataBlock instance
  \param poFeature pointer to VFKFeature instance
*/
OGRErr VFKReader::AddFeature(IVFKDataBlock *poDataBlock, VFKFeature *poFeature)
{
    poDataBlock->AddFeature(poFeature);
    return OGRERR_NONE;
}

/*!
  \brief Get data block

  \param i index (starting with 0)

  \return pointer to VFKDataBlock instance or NULL on failure
*/
IVFKDataBlock *VFKReader::GetDataBlock(int i) const
{
    if (i < 0 || i >= m_nDataBlockCount)
        return nullptr;

    return m_papoDataBlock[i];
}

/*!
  \brief Get data block

  \param pszName data block name

  \return pointer to VFKDataBlock instance or NULL on failure
*/
IVFKDataBlock *VFKReader::GetDataBlock(const char *pszName) const
{
    for (int i = 0; i < m_nDataBlockCount; i++) {
        if (EQUAL(GetDataBlock(i)->GetName(), pszName))
            return GetDataBlock(i);
    }

    return nullptr;
}

/*!
  \brief Load geometry (loop datablocks)

  \return number of invalid features
*/
int VFKReader::LoadGeometry()
{
    long int nfeatures = 0;
    for (int i = 0; i < m_nDataBlockCount; i++) {
        nfeatures += m_papoDataBlock[i]->LoadGeometry();
    }

    CPLDebug("OGR_VFK", "VFKReader::LoadGeometry(): invalid=%ld", nfeatures);

    return static_cast<int>(nfeatures);
}

/*!
  \brief Add info

  \param pszLine pointer to line
*/
void VFKReader::AddInfo(const char *pszLine)
{
    const int nOffset = pszLine[1] == 'H' ? 2 : 1;  // &DKATUZE

    const char *poKey = pszLine + nOffset; /* &H */
    const char *poChar = poKey;
    int iKeyLength = 0;
    while (*poChar != '\0' && *poChar != ';') {
        iKeyLength++;
        poChar ++;
    }
    if (*poChar == '\0')
        return;

    char *pszKey = (char *) CPLMalloc(iKeyLength + 1);
    strncpy(pszKey, poKey, iKeyLength);
    pszKey[iKeyLength] = '\0';

    poChar++; /* skip ; */

    int iValueLength = 0;
    int nSkip = 3; /* &H + ; */
    while (*poChar != '\0') {
        if (*poChar == '"' && iValueLength == 0) {
            nSkip++;
        }
        else {
            iValueLength++;
        }
        poChar++;
    }
    if (nSkip > 3 && iValueLength > 0 )
        iValueLength--;

    char *pszValue = (char *) CPLMalloc(iValueLength + 1);
    for (int i = 0; i < iValueLength; i++) {
        pszValue[i] = pszLine[iKeyLength+nSkip+i];
        if (pszValue[i] == '"') {
            pszValue[i] = '\''; /* " -> ' */
        }
    }

    pszValue[iValueLength] = '\0';

    /* recode values, assuming Latin2 */
    if (EQUAL(pszKey, "CODEPAGE")) {
        if (!EQUAL(pszValue, "WE8ISO8859P2"))
            m_bLatin2 = false;
    }

    char *pszValueEnc = CPLRecode(pszValue,
                            m_bLatin2 ? "ISO-8859-2" : "WINDOWS-1250",
                            CPL_ENC_UTF8);
    if (poInfo.find(pszKey) == poInfo.end() ) {
        poInfo[pszKey] = pszValueEnc;
    }
    else {
        /* max. number of duplicated keys can be 101 */
        const size_t nLen = strlen(pszKey) + 5;
        char *pszKeyUniq = (char *) CPLMalloc(nLen);

        int nCount = 1; /* assuming at least one match */
        for(std::map<CPLString, CPLString>::iterator i = poInfo.begin();
            i != poInfo.end(); ++i) {
            size_t iFound = i->first.find("_");
            if (iFound != std::string::npos &&
                EQUALN(pszKey, i->first.c_str(), iFound))
                nCount += 1;
        }

        snprintf(pszKeyUniq, nLen, "%s_%d", pszKey, nCount);
        poInfo[pszKeyUniq] = pszValueEnc;
        CPLFree(pszKeyUniq);
    }

    CPLFree(pszKey);
    CPLFree(pszValue);
    CPLFree(pszValueEnc);
}

/*!
  \brief Get info

  \param key key string

  \return pointer to value string or NULL if key not found
*/
const char *VFKReader::GetInfo(const char *key)
{
    if (poInfo.find(key) == poInfo.end())
        return nullptr;

    return poInfo[key].c_str();
}
