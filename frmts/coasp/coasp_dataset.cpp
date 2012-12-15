/******************************************************************************
 * $Id$
 *
 * Project:  DRDC Configurable Airborne SAR Processor (COASP) data reader
 * Purpose:  Support in GDAL for the DRDC COASP format data, both Metadata
 *	     and complex imagery.
 * Author:   Philippe Vachon <philippe@cowpig.ca>
 * Notes:    I have seen a grand total of 2 COASP scenes (3 sets of headers).
 *	     This is based on my best observations, some educated guesses and
 *	     such. So if you have a scene that doesn't work, send it to me
 *	     please and I will make it work... with violence.
 *
 ******************************************************************************
 * Copyright (c) 2007, Philippe Vachon
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "gdal_priv.h"
#include "cpl_port.h"
#include "cpl_conv.h"
#include "cpl_vsi.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_COASP(void);
CPL_C_END

#define TYPE_GENERIC 0
#define TYPE_GEOREF  1



enum ePolarization {
	hh = 0,
	hv,
	vh,
	vv
};


/*******************************************************************
 * Declaration of the COASPMetadata classes                        *
 *******************************************************************/

class COASPMetadataItem;

class COASPMetadataReader
{
	VSILFILE *fp;
	char **papszMetadata;
	int nMetadataCount;
	int nCurrentItem;
public:
	COASPMetadataReader(char *pszFname);
	COASPMetadataItem *GetNextItem();
	COASPMetadataItem *GetItem(int nItem);
	int GotoMetadataItem(int nItemNumber);
	int GotoMetadataItem(const char *pszName);
	int GetCurrentItem() { return nCurrentItem; }
};

/* Your average metadata item */
class COASPMetadataItem
{
protected:
	char *pszItemName;
	char *pszItemValue;
public:
	COASPMetadataItem() { }
	COASPMetadataItem(char *pszItemName, char *pszItemValue);
	char *GetItemName();
	char *GetItemValue();
	int GetType() { return TYPE_GENERIC; }
};

/* Same as MetadataItem class except parses GCP properly and returns
 * a GDAL_GCP struct 
 */
class COASPMetadataGeorefGridItem : public COASPMetadataItem
{
	int nId;
	int nPixels;
	int nLines;
	double ndLat;
	double ndLong;
public:
	COASPMetadataGeorefGridItem(int nId, int nPixels, int nLines, 
		double ndLat, double ndLong);
	const char *GetItemName() { return "georef_grid"; }
	GDAL_GCP *GetItemValue();
	int GetType() { return TYPE_GEOREF; }
};

/********************************************************************
 * ================================================================ *
 * Implementation of the COASPMetadataItem Classes                  *
 * ================================================================ *
 ********************************************************************/

COASPMetadataItem::COASPMetadataItem(char *pszItemName, char *pszItemValue) 
{
	this->pszItemName = VSIStrdup(pszItemName);
	this->pszItemValue = VSIStrdup(pszItemValue);
}

char *COASPMetadataItem::GetItemName() 
{
	return VSIStrdup(pszItemName);
}

char *COASPMetadataItem::GetItemValue() 
{
	return VSIStrdup(pszItemValue);
}

COASPMetadataGeorefGridItem::COASPMetadataGeorefGridItem(int nId, int nPixels, 
	int nLines, double ndLat, double ndLong)
{
	this->nId = nId;
	this->nPixels = nPixels;
	this->nLines = nLines;
	this->ndLat = ndLat;
	this->ndLong = ndLong;
	this->pszItemName = VSIStrdup("georef_grid");

}

GDAL_GCP *COASPMetadataGeorefGridItem::GetItemValue() 
{
	return NULL;
}


/********************************************************************
 * ================================================================ *
 * Implementation of the COASPMetadataReader Class                  *
 * ================================================================ *
 ********************************************************************/

COASPMetadataReader::COASPMetadataReader(char *pszFname) 
{
	this->fp = NULL;
	this->nCurrentItem = 0;
	this->papszMetadata = CSLLoad(pszFname);
	this->nMetadataCount = CSLCount(this->papszMetadata);
}

COASPMetadataItem *COASPMetadataReader::GetNextItem() 
{
	COASPMetadataItem *poMetadata;
	char **papszMDTokens;
	char *pszItemName;
	char *pszItemValue;
	if (nCurrentItem >= nMetadataCount)
		return NULL;


	papszMDTokens = CSLTokenizeString2(papszMetadata[nCurrentItem], " ", 
		CSLT_HONOURSTRINGS );
	pszItemName = papszMDTokens[0];
	if (EQUALN(pszItemName, "georef_grid", 11)) {
		double ndLat, ndLong;
		int nPixels, nLines;
		// georef_grid ( pixels lines ) ( lat long )
		// 0           1 2      3     4 5 6   7    8
		nPixels = atoi(papszMDTokens[2]);
		nLines = atoi(papszMDTokens[3]);
		ndLat = CPLAtof(papszMDTokens[6]);
		ndLong = CPLAtof(papszMDTokens[7]);
		poMetadata = new COASPMetadataGeorefGridItem(nCurrentItem, nPixels, 
			nLines, ndLat, ndLong);
	}
	else {
		int nCount = CSLCount(papszMDTokens);
		pszItemValue = strdup(papszMDTokens[1]); 
		for (int i = 2; i < nCount; i++) {
			int nSize = strlen(papszMDTokens[i]);
			pszItemValue = (char *)CPLRealloc(pszItemValue, 
				strlen(pszItemValue) + 1 + nSize);
			sprintf(pszItemValue,"%s %s",pszItemValue, 
				papszMDTokens[i]);
		}
		
		poMetadata = new COASPMetadataItem(pszItemName,
			pszItemValue);

		free(pszItemValue);
	}
	free(pszItemName);
	nCurrentItem++;
	return poMetadata;
}

/* Goto a particular metadata item, listed by number */
int COASPMetadataReader::GotoMetadataItem(int nItemNumber)
{
	if (nItemNumber > nMetadataCount || nItemNumber < 0) {
		nItemNumber = 0;
		return 0;
	}
	nCurrentItem = nItemNumber;
	return nCurrentItem;
}

/* Goto the first metadata item with a particular name */
int COASPMetadataReader::GotoMetadataItem(const char *pszName)
{
	nCurrentItem = CSLPartialFindString(papszMetadata, pszName);
	return nCurrentItem;
}

/*******************************************************************
 * Declaration of the COASPDataset class                           *
 *******************************************************************/


class COASPRasterBand;

/* A couple of observations based on the data I have available to me:
 * a) the headers don't really change, beyond indicating data sources
 *    and such. As such, I only read the first header specified by the 
 *    user. Note that this is agnostic: you can specify hh, vv, vh, hv and
 *    all the data needed will be immediately available.
 * b) Lots of GCPs are present in the headers. This is most excellent.
 * c) There is no documentation on this format. All the knowledge contained
 *    herein is from harassing various Defence Scientists at DRDC Ottawa.
 */

class COASPDataset : public GDALDataset
{
	friend class COASPRasterBand;
	FILE *fpHdr; /* File pointer for the header file */
	VSILFILE *fpBinHH; /* File pointer for the binary matrix */
	VSILFILE *fpBinHV;
	VSILFILE *fpBinVH;
	VSILFILE *fpBinVV;

	char *pszFileName; /* line and mission ID, mostly, i.e. l27p7 */

	int nGCPCount;
	GDAL_GCP *pasGCP;
public:
	static GDALDataset *Open( GDALOpenInfo * );
	static int Identify( GDALOpenInfo * poOpenInfo );
	int GetGCPCount();
	const GDAL_GCP *GetGCPs();
};

/********************************************************************
 * ================================================================ *
 * Declaration and implementation of the COASPRasterBand Class      *
 * ================================================================ *
 ********************************************************************/

class COASPRasterBand : public GDALRasterBand {
	VSILFILE *fp;
	int ePol;
public:
	COASPRasterBand( COASPDataset *poDS, GDALDataType eDataType, int ePol, VSILFILE *fp );
	virtual CPLErr IReadBlock( int nBlockXOff, int nBlockYOff, 
		void *pImage);
};

COASPRasterBand::COASPRasterBand( COASPDataset *poDS, GDALDataType eDataType,
	int ePol, VSILFILE *fp)
{
	this->fp = fp;
	this->ePol = ePol;
	this->poDS = poDS;
	this->eDataType = eDataType;
	this->nBlockXSize = poDS->GetRasterXSize();
	this->nBlockYSize = 1;
}

CPLErr COASPRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff, 
	void *pImage )
{
	if (this->fp == NULL) {
		CPLError(CE_Fatal, 1, "file pointer freed unexpectedly\n");
		return CE_Fatal;
	}

	/* 8 bytes per pixel: 4 bytes I, 4 bytes Q */
	unsigned long nByteNum = poDS->GetRasterXSize() * 8 * nBlockYOff;

	VSIFSeekL(this->fp, nByteNum, SEEK_SET);
	int nReadSize = (GDALGetDataTypeSize(eDataType)/8) * poDS->GetRasterXSize();
	VSIFReadL((char *)pImage, 1, nReadSize, 
		this->fp);

#ifdef CPL_LSB
	GDALSwapWords( pImage, 4, nBlockXSize * 2, 4 ); 
#endif


	return CE_None;
	
}


/********************************************************************
 * ================================================================ *
 * Implementation of the COASPDataset Class                         *
 * ================================================================ *
 ********************************************************************/


/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int COASPDataset::GetGCPCount()
{
	return nGCPCount;
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *COASPDataset::GetGCPs()
{
	return pasGCP;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int COASPDataset::Identify( GDALOpenInfo *poOpenInfo ) 
{
	if(poOpenInfo->fp == NULL || poOpenInfo->nHeaderBytes < 256)
		return 0;

	/* With a COASP .hdr file, the first line or so is:
 	 * time_first_datarec
 	 */
	if(EQUALN((char *)poOpenInfo->pabyHeader,"time_first_datarec",18))
		return 1;

	return 0;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *COASPDataset::Open( GDALOpenInfo *poOpenInfo ) 
{
	if (!COASPDataset::Identify(poOpenInfo))
		return NULL;
        
/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The COASP driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }
	/* Create a fresh dataset for us to work with */
	COASPDataset *poDS;
	poDS = new COASPDataset();
	
	if (poDS == NULL)
		return NULL;

	/* Steal the file pointer for the header */
	poDS->fpHdr = poOpenInfo->fp;
	poOpenInfo->fp = NULL;
	
	/* Set the binary matrix file pointers to NULL, for now */
	poDS->fpBinHH = NULL;
	poDS->fpBinHV = NULL;
	poDS->fpBinVH = NULL;
	poDS->fpBinVV = NULL;

	poDS->pszFileName = VSIStrdup(poOpenInfo->pszFilename);

	/* determine the file name prefix */
	const char *pszFilename;
	char *pszBaseName = VSIStrdup(CPLGetBasename(poDS->pszFileName));
	char *pszDir = VSIStrdup(CPLGetPath(poDS->pszFileName));
	const char *pszExt = "rc";
	int nNull = strlen(pszBaseName) - 1;
	char *pszBase = (char *)CPLMalloc(nNull);
	strncpy(pszBase, pszBaseName, nNull);
	pszBase[nNull - 1] = '\0';
	free(pszBaseName);

	char *psChan = strstr(pszBase,"hh");;
	if (psChan == NULL) {
		psChan = strstr(pszBase, "hv");
	}
	if (psChan == NULL) {
		psChan = strstr(pszBase, "vh");
	}
	if (psChan == NULL) {
		psChan = strstr(pszBase, "vv");
	}

	if (psChan == NULL) {
		CPLError(CE_Fatal, 1, "unable to recognize file as COASP.\n");
		free(poDS->pszFileName);
		free(pszBase);
		free(pszDir);
		delete poDS;
		return NULL;
	}

    /* Read Metadata, set GCPs as is appropriate */
    COASPMetadataReader *poReader = new COASPMetadataReader(
        poDS->pszFileName);


    /* Get Image X and Y widths */
    poReader->GotoMetadataItem("number_lines");
    COASPMetadataItem *poItem = poReader->GetNextItem();
    char *nValue = poItem->GetItemValue();
    poDS->nRasterYSize = atoi(nValue);
    free(nValue);

    poReader->GotoMetadataItem("number_samples");
    poItem = poReader->GetNextItem();
    nValue = poItem->GetItemValue();
    poDS->nRasterXSize = atoi(nValue);
    free(nValue);


	/* Horizontal transmit, horizontal receive */
	psChan[0] = 'h';
	psChan[1] = 'h';
	pszFilename = CPLFormFilename(pszDir, pszBase, pszExt); 

	poDS->fpBinHH = VSIFOpenL(pszFilename, "r");

	if (poDS->fpBinHH != 0) {
		/* Set raster band */
		poDS->SetBand(1, new COASPRasterBand(poDS, GDT_CFloat32, 
			hh , poDS->fpBinHH));
	}
	
	/* Horizontal transmit, vertical receive */
    psChan[0] = 'h'; 
    psChan[1] = 'v'; 
    pszFilename = CPLFormFilename(pszDir, pszBase, pszExt);
 
	poDS->fpBinHV = VSIFOpenL(pszFilename, "r");

	if (poDS->fpBinHV != 0) {
		poDS->SetBand(2, new COASPRasterBand(poDS, GDT_CFloat32,
			hv, poDS->fpBinHV));
	}

	/* Vertical transmit, horizontal receive */
    psChan[0] = 'v'; 
    psChan[1] = 'h'; 
    pszFilename = CPLFormFilename(pszDir, pszBase, pszExt);
 
	poDS->fpBinVH = VSIFOpenL(pszFilename, "r");

	if (poDS->fpBinVH != 0) {
    	poDS->SetBand(3, new COASPRasterBand(poDS, GDT_CFloat32,
			vh, poDS->fpBinVH));
	}

	/* Vertical transmit, vertical receive */
    psChan[0] = 'v'; 
    psChan[1] = 'v'; 
    pszFilename = CPLFormFilename(pszDir, pszBase, pszExt);

	poDS->fpBinVV = VSIFOpenL(pszFilename, "r");

	if (poDS->fpBinVV != 0) {
		poDS->SetBand(4, new COASPRasterBand(poDS, GDT_CFloat32,
			vv, poDS->fpBinVV));
	}


	/* Oops, missing all the data? */

	if (poDS->fpBinHH == NULL && poDS->fpBinHV == NULL 
		&& poDS->fpBinVH == NULL && poDS->fpBinVV == NULL) 
	{
		CPLError(CE_Fatal,1,"Unable to find any data! Aborting.");
		free(pszBase);
		free(pszDir);
		delete poDS;

		return NULL;
	}

    if ( poDS->GetRasterCount() == 4 ) {
        poDS->SetMetadataItem( "MATRIX_REPRESENTATION", "SCATTERING" );
    }

	free(pszBase);
	free(pszDir);
	
	poDS->nGCPCount = 0;
	poDS->pasGCP = NULL;

	delete poItem;
	delete poReader; 

	return poDS;

}

/************************************************************************/
/*                         GDALRegister_COASP()                         */
/************************************************************************/

void GDALRegister_COASP(void)
{
	GDALDriver *poDriver;
	if ( GDALGetDriverByName( "COASP" ) == NULL ) {
		poDriver = new GDALDriver();
		poDriver->SetDescription( "COASP" );
		poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
			"DRDC COASP SAR Processor Raster" );
		poDriver->SetMetadataItem( GDAL_DMD_EXTENSION,
			"hdr" );
/*		poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
			"frmt_coasp.html"); */
		poDriver->pfnIdentify = COASPDataset::Identify;
		poDriver->pfnOpen = COASPDataset::Open;
		GetGDALDriverManager()->RegisterDriver( poDriver );
	}

}
