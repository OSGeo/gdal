// Implementation of the CSFCommand
#include "stdafx.h"
#include "SF.h"
#include "SFRS.h"


ATLCOLUMNINFO CShapeFile::colInfo;

CVirtualArray::CVirtualArray()
{
	m_ppasArray = NULL;
	m_nArraySize = 0;
	m_hDBFHandle = NULL;
	m_hSHPHandle = NULL;
}


CVirtualArray::~CVirtualArray()
{
	//RemoveAll();
}

void CVirtualArray::RemoveAll()
{

	int i;

	for (i=0; i < m_nArraySize; i++)
	{
		free(m_ppasArray[i]);
	}

	free(m_ppasArray);

	m_ppasArray = NULL;
	m_nArraySize = 0;

	if (m_hDBFHandle)
		DBFClose(m_hDBFHandle);

	if (m_hSHPHandle)
		SHPClose(m_hSHPHandle);

	m_hDBFHandle = NULL;
	m_hSHPHandle = NULL;
}


void	CVirtualArray::Initialize(int nArraySize, DBFHandle hDBF, SHPHandle hSHP)
{
	m_ppasArray   = (BYTE **) calloc(nArraySize, sizeof(BYTE *));
	m_nArraySize  = nArraySize;
	m_hDBFHandle  = hDBF;
	m_hSHPHandle  = hSHP;

	int i;
	int			nOffset = 0;

	for (i=0;i < DBFGetFieldCount(hDBF); i++)
	{
		SchemaInfo		sInfo;
		DBFFieldType	eType;
		int				nWidth;

		eType = DBFGetFieldInfo(hDBF,i,NULL,&nWidth,NULL);

		sInfo.eType = eType;
		sInfo.nOffset = nOffset;

		switch (eType)
		{
			case FTInteger:	
				nOffset += 4;
				break;
			case FTString:
				nWidth +=1;
				nWidth *= 2;
				nOffset += nWidth;
				if (nWidth %4)
					nOffset += (4 - (nWidth %4));
				break;
			case FTDouble:
				nOffset += 8;
				break;

		}

		aSchemaInfo.Add(sInfo);
	}

	m_nPackedRecordLength = nOffset;
}


BYTE &CVirtualArray::operator[](int iIndex) 
{
	ATLASSERT(iIndex >=0 && iIndex < m_nArraySize);
	int i;
	BYTE *pBuffer;
	const char   *pszStr;

	if (m_ppasArray[iIndex])
		return *(m_ppasArray[iIndex]);

	pBuffer = m_ppasArray[iIndex] = (BYTE *) malloc(m_nPackedRecordLength);

	for (i=0; i < aSchemaInfo.GetSize(); i++)
	{
		switch(aSchemaInfo[i].eType)
		{
			case FTInteger:	
				*((int *) &(pBuffer[aSchemaInfo[i].nOffset])) = DBFReadIntegerAttribute(m_hDBFHandle,iIndex,i);
				break;
			case FTDouble:
				*((double *) &(pBuffer[aSchemaInfo[i].nOffset])) = DBFReadDoubleAttribute(m_hDBFHandle,iIndex,i);
				break;
			case FTString:
				pszStr = DBFReadStringAttribute(m_hDBFHandle,iIndex,i);

				strcpy((char *) &(pBuffer[aSchemaInfo[i].nOffset]),pszStr);
				break;
		}
	}
	return *(m_ppasArray[iIndex]);
}







/////////////////////////////////////////////////////////////////////////////
// CSFCommand
HRESULT CSFCommand::Execute(IUnknown * pUnkOuter, REFIID riid, DBPARAMS * pParams, 
								 LONG * pcRowsAffected, IUnknown ** ppRowset)
{
	CSFRowset* pRowset;
	return CreateRowset(pUnkOuter, riid, pParams, pcRowsAffected, ppRowset, pRowset);
}


DBTYPE DBFType2OLEType(DBFFieldType eDBFType,int *pnOffset, int nWidth)
{
	DBTYPE eDBType;


	switch(eDBFType)
	{
		case FTString:
			eDBType = DBTYPE_STR;
			nWidth++;
			nWidth *= 2;
			*pnOffset += nWidth;
			if (nWidth %4)
				*pnOffset += (4- (nWidth %4));
			break;
		case FTInteger:
			eDBType = DBTYPE_I4;
			*pnOffset += 4;
			break;
		case FTDouble:
			eDBType = DBTYPE_R8;
			*pnOffset += 8;
			break;
	}

	return eDBType;
}
HRESULT CSFRowset::Execute(DBPARAMS * pParams, LONG* pcRowsAffected)
{	
	USES_CONVERSION;
	LPTSTR		szFile = OLE2T(m_strCommandText);
	DBFHandle	hDBF;
	int			nFields;
	int			i;
	int			nOffset = 0;
	

	hDBF = DBFOpen(szFile,"r");

	if (!hDBF)
		return DB_E_ERRORSINCOMMAND;

	*pcRowsAffected = DBFGetRecordCount(hDBF);
	nFields = DBFGetFieldCount(hDBF);

	for (i=0; i < nFields; i++)
	{
		char	szFieldName[32];
		int		nWidth = 0;
		int		nDecimals = 0;

		DBFFieldType	eType;
		
		eType = DBFGetFieldInfo(hDBF,i,szFieldName,&nWidth, &nDecimals);


		ATLCOLUMNINFO colInfo;
		memset(&colInfo, 0, sizeof(ATLCOLUMNINFO));
		
		colInfo.pwszName	= ::SysAllocString(T2OLE(szFieldName));
		colInfo.iOrdinal	= i+1;
		colInfo.dwFlags		= DBCOLUMNFLAGS_ISFIXEDLENGTH;
		colInfo.ulColumnSize= (nWidth > 8 ? nWidth : 8);
		colInfo.bPrecision  = nDecimals;
		colInfo.bScale		= 0;
		colInfo.columnid.uName.pwszName = colInfo.pwszName;
		colInfo.cbOffset	= nOffset;
		colInfo.wType		= DBFType2OLEType(eType,&nOffset,nWidth);	

		m_paColInfo.Add(colInfo);
	}


	m_rgRowData.Initialize(*pcRowsAffected,hDBF,NULL);

	return S_OK;
}
