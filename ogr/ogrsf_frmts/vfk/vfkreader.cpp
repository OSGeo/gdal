/******************************************************************************
 * $Id$
 *
 * Project:  VFK Reader
 * Purpose:  Implements VFKReader class.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2009-2010, Martin Landa <landa.martin gmail.com>
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
    return new VFKReader();
}

/*!
  \brief VFKReader constructor
*/
VFKReader::VFKReader()
{
    m_pszFilename     = NULL;

    m_pszWholeText    = NULL;
    
    m_nDataBlockCount = 0;
    m_papoDataBlock   = NULL;
}

/*!
  \brief VFKReader destructor
*/
VFKReader::~VFKReader()
{
    CPLFree(m_pszFilename);

    VSIFree(m_pszWholeText);
    
    /* clear data blocks */
    for (int i = 0; i < m_nDataBlockCount; i++)
        delete m_papoDataBlock[i];
    CPLFree(m_papoDataBlock);

    m_nDataBlockCount = 0;
    m_papoDataBlock = NULL;
}

/*!
  \brief Set source file

  \param pszFilename source filename
*/
void VFKReader::SetSourceFile(const char *pszFilename)
{
    CPLFree(m_pszFilename);
    m_pszFilename = CPLStrdup(pszFilename);
}

/*!
  \brief Read the file & load the data

  \param pszFile pointer to a filename

  \return TRUE on success
  \return FALSE on error
*/
int VFKReader::LoadData()
{
    FILE *fp;
    long          nLength;

    if (m_pszFilename == NULL)
	return FALSE;

    /* load vfk file */
    fp = VSIFOpen(m_pszFilename, "rb");
    if (fp == NULL) {
	CPLError(CE_Failure, CPLE_OpenFailed, 
		 "Failed to open file %s.", m_pszFilename);
        return FALSE;
    }

    /* get file length */
    VSIFSeek(fp, 0, SEEK_END);
    nLength = VSIFTell(fp);
    VSIFSeek(fp, 0, SEEK_SET);

    /* read file - is necessary to read the whole file? */
    m_pszWholeText = (char *) VSIMalloc(nLength+1);
    
    if (m_pszWholeText == NULL) {
        CPLError(CE_Failure, CPLE_AppDefined, 
		 "Failed to allocate %ld byte buffer for %s,\n"
		 "is this really a VFK file?",
		 nLength, m_pszFilename);
        VSIFClose(fp);
        return FALSE;
    }
    
    if (VSIFRead(m_pszWholeText, nLength, 1, fp) != 1) {
        VSIFree(m_pszWholeText);
        m_pszWholeText = NULL;
        VSIFClose(fp);
        CPLError(CE_Failure, CPLE_AppDefined, 
		 "Read failed on %s.", m_pszFilename);
        return FALSE;
    }
    
    m_pszWholeText[nLength] = '\0';

    VSIFClose(fp);

    /* split lines */
    /* TODO: reduce chars */
    for (char *poChar = m_pszWholeText; *poChar != '\0'; poChar++) {
	if (*poChar == '\244' && *(poChar+1) != '\0' && *(poChar+2) != '\0') { 
	    *(poChar++) = ' '; // \r
	    *(poChar++) = ' '; // \n
	    *poChar = ' ';
	}
    }
    
    CPLDebug("OGR_VFK", "VFKReader::LoadData(): length=%ld", nLength);

    return TRUE;
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

    pszBlockName = (char *) CPLMalloc(n+1);
    strncpy(pszBlockName, pszLine + 2, n);
    pszBlockName[n] = '\0';

    return pszBlockName;
}

/*!
  \brief Get data blocks (&B)

  Call LoadData() before this function.

  \return FALSE on error
  \return TRUE on success
*/
int VFKReader::LoadDataBlocks()
{ 
    char         *pszChar;
    char         *pszLine;
    char         *pszBlockName;
    int           nRow;
    
    VFKDataBlock *poNewDataBlock;

    if (m_pszWholeText == NULL)
        return FALSE;

    poNewDataBlock = NULL;
    pszBlockName = NULL;
    nRow = 0;

    /* read lines */
    pszChar = m_pszWholeText;
    pszLine = m_pszWholeText;
    while (*pszChar != '\0') {
	if (*pszChar == '\r' && *(pszChar+1) == '\n') {
	    nRow++;
	    if (*pszLine == '&' && *(pszLine+1) == 'B') {
		/* add data block */
		pszBlockName = GetDataBlockName(pszLine);
                if (pszBlockName == NULL)
                    break;

		poNewDataBlock = new VFKDataBlock(pszBlockName, this);
		CPLFree(pszBlockName);
		pszBlockName = NULL;
		poNewDataBlock->SetGeometryType();
		poNewDataBlock->SetProperties(pszLine);
		AddDataBlock(poNewDataBlock);
	    }
	    else if (*pszLine == '&' && *(pszLine+1) == 'D') {
		/* data row */
		pszBlockName = GetDataBlockName(pszLine);
                if (pszBlockName == NULL)
                    break;

		poNewDataBlock = GetDataBlock(pszBlockName);
		if (poNewDataBlock == NULL) {
		    if (!EQUAL(pszBlockName, "KATUZE")) {
			/* ignore KATUZE block */
			CPLError(CE_Warning, CPLE_AppDefined, 
				 "Data block '%s' not found.\n", pszBlockName);
		    }
		}
		else 
		    poNewDataBlock->AddFeature(pszLine);

		CPLFree(pszBlockName);
		pszBlockName = NULL;
	    }
	    else if (*pszLine == '&' && *(pszLine+1) == 'H') {
		/* header - metadata */
		AddInfo(pszLine);
	    }
	    else if (*pszLine == '&' && *(pszLine+1) == 'K') {
		/* end of file */
		break;
	    }
	    pszChar++;
	    pszLine = pszChar + 1;
	}
	pszChar++;
    }

    return TRUE;
}

/*!
  \brief Add new data block

  \param poNewDataBlock pointer to VFKDataBlock instance

  \return number of registred data blocks
*/
int VFKReader::AddDataBlock(VFKDataBlock *poNewDataBlock)
{
    m_nDataBlockCount++;
    
    // CPLDebug("OGR_VFK", "VFKReader::AddDataBlock(): i=%d", m_nDataBlockCount);

    m_papoDataBlock = (VFKDataBlock **)
	CPLRealloc(m_papoDataBlock, sizeof (VFKDataBlock *) * m_nDataBlockCount);
    m_papoDataBlock[m_nDataBlockCount-1] = poNewDataBlock;

    return m_nDataBlockCount;
}

/*!
  \brief Get data block

  \param i index (starting with 0)

  \return pointer to VFKDataBlock instance
  \return NULL on failure
*/
VFKDataBlock *VFKReader::GetDataBlock(int i) const
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
VFKDataBlock *VFKReader::GetDataBlock(const char *pszName) const
{
    for (int i = 0; i < m_nDataBlockCount; i++) {
        if (EQUAL(GetDataBlock(i)->GetName(), pszName))
            return GetDataBlock(i);
    }

    return NULL;
}

/*!
  \brief Load geometry (loop datablocks)

  \return number of processed features
*/
long VFKReader::LoadGeometry()
{
    long int nfeatures;

    nfeatures = 0;
    for (int i = 0; i < m_nDataBlockCount; i++) {
	nfeatures += m_papoDataBlock[i]->LoadGeometry();
    }
    
    CPLDebug("OGR_VFK", "VFKReader::LoadGeometry(): n=%ld", nfeatures);
    
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
