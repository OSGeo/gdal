/******************************************************************************
 * $Id$
 *
 * Project:  VFK Reader
 * Purpose:  Implements VFKReader class.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2009-2010, 2012, Martin Landa <landa.martin gmail.com>
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

#include "vfkreader.h"
#include "vfkreaderp.h"

#include "cpl_conv.h"
#include "cpl_error.h"

#define SUPPORT_GEOMETRY

#ifdef SUPPORT_GEOMETRY
#  include "ogr_geometry.h"
#endif

/*!
  \brief IVFKReader desctructor
*/
IVFKReader::~IVFKReader()
{
}

/*!
  \brief Create new instance of VFKReader

  \return pointer to VFKReader instance
*/
IVFKReader *CreateVFKReader()
{
#ifdef HAVE_SQLITE
    return new VFKReaderSQLite();
#else
    return new VFKReader();
#endif
}

/*!
  \brief VFKReader constructor
*/
VFKReader::VFKReader()
{
    m_pszFilename     = NULL;

    m_poFD            = NULL;
    
    m_nDataBlockCount = 0;
    m_papoDataBlock   = NULL;
}

/*!
  \brief VFKReader destructor
*/
VFKReader::~VFKReader()
{
    CPLFree(m_pszFilename);

    if (m_poFD)
	VSIFClose(m_poFD);
    
    /* clear data blocks */
    for (int i = 0; i < m_nDataBlockCount; i++)
        delete m_papoDataBlock[i];
    CPLFree(m_papoDataBlock);
    
    m_nDataBlockCount = 0;
    m_papoDataBlock = NULL;
}

/*!
  \brief Open data file

  \return OGRERR_NONE on success
  \return OGRERR_FAILURE on error
*/
OGRErr VFKReader::OpenFile(const char *pszFilename)
{
    CPLAssert(NULL == m_pszFilename);
    
    m_pszFilename = CPLStrdup(pszFilename);
    
    m_poFD = VSIFOpen(m_pszFilename, "rb");
    if (m_poFD == NULL) {
	CPLError(CE_Failure, CPLE_OpenFailed, 
		 "Failed to open file %s.", m_pszFilename);
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

static char *GetDataBlockName(const char *pszLine)
{
    int         n;
    const char *pszLineChar;
    char       *pszBlockName;

    for (pszLineChar = pszLine + 2, n = 0; *pszLineChar != '\0' && *pszLineChar != ';'; pszLineChar++, n++)
	;

    if (*pszLineChar == '\0')
        return NULL;

    pszBlockName = (char *) CPLMalloc(n + 1);
    strncpy(pszBlockName, pszLine + 2, n);
    pszBlockName[n] = '\0';

    return pszBlockName;
}

/*!
  \brief Load data block definitions (&B)

  Call VFKReader::OpenFile() before this function.

  \return number of data blocks
  \return -1 on error
*/
int VFKReader::ReadDataBlocks()
{
    const char *pszLine;
    char       *pszBlockName;

    IVFKDataBlock *poNewDataBlock;
    
    CPLAssert(NULL != m_pszFilename);

    VSIFSeek(m_poFD, 0, SEEK_SET);
    while ((pszLine = CPLReadLine(m_poFD)) != NULL) {
        if (strlen(pszLine) < 2 || pszLine[0] != '&')
	    continue;
	if (pszLine[1] == 'B') {
	    pszBlockName = GetDataBlockName(pszLine);
	    if (pszBlockName == NULL) {	
		CPLError(CE_Failure, CPLE_NotSupported, 
			 "Corrupted data - line\n%s\n", pszLine);
		return -1;
	    }
	    poNewDataBlock = (IVFKDataBlock *) CreateDataBlock(pszBlockName);
	    CPLFree(pszBlockName);
	    poNewDataBlock->SetGeometryType();
	    poNewDataBlock->SetProperties(pszLine);
	    AddDataBlock(poNewDataBlock);
	}
	else if (pszLine[1] == 'H') {
	    /* header - metadata */
	    AddInfo(pszLine);
	}
	else if (pszLine[1] == 'K') {
	    /* end of file */
	    break;
	}
    }
    
    return m_nDataBlockCount;
}


/*!
  \brief Load data records (&D)

  Call VFKReader::OpenFile() before this function.
  
  \return number of data records
  \return -1 on error
*/
int VFKReader::ReadDataRecords(IVFKDataBlock *poDataBlock)
{
    const char *pszRawLine, *pszName, *pszLine;
    char       *pszBlockName;

    VFKFeature    *poNewFeature;
    
    if (poDataBlock->GetFeatureCount() >= 0)
	return -1;

    poDataBlock->SetFeatureCount(0);
    poDataBlock->SetMaxFID(0);
    pszName = poDataBlock->GetName();

    VSIFSeek(m_poFD, 0, SEEK_SET);
    while ((pszRawLine = CPLReadLine(m_poFD)) != NULL) {
	if (strlen(pszRawLine) < 2)
	    continue;
	if (pszRawLine[strlen(pszRawLine) - 1] == '\244') {
	    CPLString pszLine1;
	    
	    /* merge lines */
	    do {
		pszLine1 += pszRawLine;
		pszLine1.erase(pszLine1.size() - 1); /* remove \244 from string */
	    } while ((pszRawLine = CPLReadLine(m_poFD)) != NULL &&
		   pszRawLine[strlen(pszRawLine) - 1] == '\244');
	    pszLine1 += pszRawLine;
	    pszLine = pszLine1.c_str();
	}
	else {
	    pszLine = pszRawLine;
	}
	
	if (pszLine[1] == 'D') {
	    pszBlockName = GetDataBlockName(pszLine);
	    if (pszBlockName && EQUAL(pszBlockName, pszName)) {
		poNewFeature = new VFKFeature(poDataBlock);
		poNewFeature->SetProperties(pszLine);
		AddFeature(poDataBlock, poNewFeature);
	    }
	    CPLFree(pszBlockName);
	}
	else if (pszLine[1] == 'K') {
	    /* end of file */
	    break;
	}
    }
    
    return poDataBlock->GetFeatureCount();
}

IVFKDataBlock *VFKReader::CreateDataBlock(const char *pszBlockName)
{
  return (IVFKDataBlock *) new VFKDataBlock(pszBlockName, (IVFKReader *) this);
}

/*!
  \brief Add new data block

  \param poNewDataBlock pointer to VFKDataBlock instance

  \return number of registred data blocks
*/
void VFKReader::AddDataBlock(IVFKDataBlock *poNewDataBlock)
{
    m_nDataBlockCount++;
    
    m_papoDataBlock = (IVFKDataBlock **)
	CPLRealloc(m_papoDataBlock, sizeof (IVFKDataBlock *) * m_nDataBlockCount);
    m_papoDataBlock[m_nDataBlockCount-1] = poNewDataBlock;
}

/*!
  \brief Add feature

  \param poNewDataBlock pointer to VFKDataBlock instance
  \param poNewFeature pointer to VFKFeature instance
*/
void VFKReader::AddFeature(IVFKDataBlock *poDataBlock, VFKFeature *poFeature)
{
    poDataBlock->AddFeature(poFeature);
}

/*!
  \brief Get data block

  \param i index (starting with 0)

  \return pointer to VFKDataBlock instance
  \return NULL on failure
*/
IVFKDataBlock *VFKReader::GetDataBlock(int i) const
{
    if (i < 0 || i >= m_nDataBlockCount)
        return NULL;
    
    return m_papoDataBlock[i];
}

/*!
  \brief Get data block

  \param pszName data block name

  \return pointer to VFKDataBlock instance
  \return NULL on failure
*/
IVFKDataBlock *VFKReader::GetDataBlock(const char *pszName) const
{
    for (int i = 0; i < m_nDataBlockCount; i++) {
        if (EQUAL(GetDataBlock(i)->GetName(), pszName))
            return GetDataBlock(i);
    }

    return NULL;
}

/*!
  \brief Load geometry (loop datablocks)

  \return number of invalid features
*/
int VFKReader::LoadGeometry()
{
    long int nfeatures;

    nfeatures = 0;
    for (int i = 0; i < m_nDataBlockCount; i++) {
	nfeatures += m_papoDataBlock[i]->LoadGeometry();
    }
    
    CPLDebug("OGR_VFK", "VFKReader::LoadGeometry(): invalid=%ld", nfeatures);
    
    return nfeatures;
}

/*!
  \brief Add info

  \param pszLine pointer to line
*/
void VFKReader::AddInfo(const char *pszLine)
{
    int iKeyLength, iValueLength;
    char *pszKey, *pszValue;
    const char *poChar, *poKey, *poValue;
    std::string key, value;
    
    poChar = poKey = pszLine + 2; /* &H */
    iKeyLength = 0;
    while (*poChar != '\0' && *poChar != ';')
    {
	iKeyLength++;
        poChar ++;
    }
    if( *poChar == '\0' )
        return;

    pszKey = (char *) CPLMalloc(iKeyLength + 1);
    strncpy(pszKey, poKey, iKeyLength);
    pszKey[iKeyLength] = '\0';

    poValue = poChar;
    iValueLength = 0;
    while(*poChar != '\0' && !(*poChar == '\r' && *(poChar+1) == '\n')) {
	iValueLength++;
	poChar++;
    }
    if( *poChar == '\0' )
    {
        CPLFree(pszKey);
        return;
    }

    pszValue = (char *) CPLMalloc(iValueLength + 1);
    strncpy(pszValue, poValue, iValueLength);
    pszValue[iValueLength] = '\0';

    poInfo[pszKey] = pszValue;

    CPLFree(pszKey);
    CPLFree(pszValue);
}

/*!
  \brief Get info

  \param key key string

  \return pointer to value string
  \return NULL if key not found
*/
const char *VFKReader::GetInfo(const char *key)
{
    if (poInfo.find(key) == poInfo.end())
	return NULL;

    return poInfo[key].c_str();
}
