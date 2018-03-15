#include "csf.h" 
#include "csfimpl.h" 

#include <errno.h>
#include <string.h>

/* M_PI */
#include <math.h> 
#ifndef M_PI
# define M_PI        ((double)3.14159265358979323846)
#endif

/* 
 * Create a new CSF-Raster-file
 * The Rcreate function
 * creates a new CSF-Raster-file of nrRows by nrCols where each
 * cell is of type cellRepr. If the file already exists its
 * contents is destroyed. The value of
 * the pixels is undefined. MinMaxStatus is MM_KEEPTRACK. The
 * access mode is M_READ_WRITE. 
 * It is not
 * known if a file is created after a NOSPACE message.
 * Returns
 * if the file is created successfully, Rcreate returns
 * a map handle. In case of an error Rcreate returns NULL.
 *
 * Merrno 
 * NOCORE, BAD_CELLREPR, BAD_PROJECTION, OPENFAILED, NOSPACE.
 * CONFL_CELLREPR and BAD_VALUESCALE will generate a failed assertion in DEBUG mode.
 *
 * Example:  
 * .so examples/create2.tr
 *
 */

MAP *Rcreate(
	const char *fileName,  /* name of the file to be created */
	size_t nrRows,          /* the number of rows */
	size_t nrCols,          /* the number of columns */
	CSF_CR cellRepr,       /* the cell representation must be complaint with the data type
	                        */
	CSF_VS dataType,      /* a.k.a. the value scale.
	                       */
	CSF_PT projection,     /* 
	                        */
	REAL8 xUL,              /* x co-ordinate of upper left */
	REAL8 yUL,              /* y co-ordinate of upper left */
	REAL8 angle,           /* counter clockwise rotation angle
	                        * of the grid top compared to the
	                        * x-axis in radians. Legal value are 
	                        * between -0.5 pi and 0.5 pi
	                        */
	REAL8 cellSize)        /* cell size of pixel */
{
	MAP    *newMap;
	size_t  fileSize;
	char    crap = 0;

	if (! CsfIsBootedCsfKernel())
		CsfBootCsfKernel();

	newMap = (MAP *)CSF_MALLOC(sizeof(MAP));
	if (newMap == NULL)
	{
		M_ERROR(NOCORE);
		goto errorMapAlloc;
	}
	newMap->fileName = (char *)CSF_MALLOC(strlen(fileName)+1);
	if (newMap->fileName == NULL)
	{
		M_ERROR(NOCORE);
		goto errorNameAlloc;
	}

	if (!(
		cellRepr == CR_INT4 ||
		cellRepr == CR_UINT1 ||
		cellRepr == CR_REAL4 ||
		cellRepr == CR_REAL8 ))
	{
		M_ERROR(BAD_CELLREPR);
		goto error_notOpen;
	}

	switch(dataType) {
	 case VS_BOOLEAN: 
	 case VS_LDD: 
	 	if (cellRepr != CR_UINT1)
		{
			PROG_ERROR(CONFL_CELLREPR);
			goto error_notOpen;
		}
		break;
	case VS_NOMINAL: 
	case VS_ORDINAL:
	 	if (IS_REAL(cellRepr))
		{
			PROG_ERROR(CONFL_CELLREPR);
			goto error_notOpen;
		}
		break;
	case VS_SCALAR:
	case VS_DIRECTION:
	 	if (!IS_REAL(cellRepr))
		{
			PROG_ERROR(CONFL_CELLREPR);
			goto error_notOpen;
		}
		break;
	default:
		PROG_ERROR(BAD_VALUESCALE);
		goto error_notOpen;
	}

	if (cellSize <= 0.0)
	{
		M_ERROR(ILL_CELLSIZE);
		goto error_notOpen;
	}

	if ((0.5*-M_PI) >= angle || angle >= (0.5*M_PI))
	{
		M_ERROR(BAD_ANGLE);
		goto error_notOpen;
	}

	newMap->fileAccessMode = M_READ_WRITE;
	(void)strcpy(newMap->fileName, fileName);

	newMap->fp = fopen (fileName, S_CREATE);
	if(newMap->fp == NULL)
	{	
	   /* we should analyse the errno parameter
	    * here to get the reason
	    */
		M_ERROR(OPENFAILED);
		goto error_notOpen;
	}
	/*
	   fflush(newMap->fp); WHY? 
	 */

	(void)memset(&(newMap->main),0, sizeof(CSF_MAIN_HEADER));
	(void)memset(&(newMap->raster),0, sizeof(CSF_RASTER_HEADER));
	/* put defaults values     */

	/* assure signature is padded with 0x0 */
	(void)memset(newMap->main.signature, 0x0, (size_t)CSF_SIG_SPACE);
	(void)strcpy(newMap->main.signature, CSF_SIG);
	newMap->main.version = CSF_VERSION_2;
	newMap->main.gisFileId = 0;
	newMap->main.projection = PROJ_DEC_T2B(projection);
	newMap->main.attrTable = 0; /* initially no attributes */
	newMap->main.mapType = T_RASTER;

	/* write endian mode current machine: */
	newMap->main.byteOrder= ORD_OK;

#ifdef DEBUG
	newMap->read  = (CSF_READ_FUNC)CsfReadPlain;
	newMap->write = (CSF_READ_FUNC)CsfWritePlain;
#else
	newMap->read  = (CSF_READ_FUNC)fread;
	newMap->write = (CSF_READ_FUNC)fwrite;
#endif

	newMap->raster.valueScale = dataType;
	newMap->raster.cellRepr = cellRepr;
	CsfSetVarTypeMV( &(newMap->raster.minVal), cellRepr);
	CsfSetVarTypeMV( &(newMap->raster.maxVal), cellRepr);
	newMap->raster.xUL = xUL;
	newMap->raster.yUL = yUL;
	newMap->raster.nrRows = (UINT4)nrRows;
	newMap->raster.nrCols = (UINT4)nrCols;
	newMap->raster.cellSize = cellSize;
	newMap->raster.cellSizeDupl = cellSize;
	newMap->raster.angle = angle;
	CsfFinishMapInit(newMap);

	/*  set types to value cellRepr 
	newMap->types[STORED_AS]= (UINT1)newMap->raster.cellRepr;
	newMap->types[READ_AS]  = (UINT1)newMap->raster.cellRepr;
	*/
	newMap->appCR  = (UINT1)newMap->raster.cellRepr;
	newMap->app2file = CsfDummyConversion;
	newMap->file2app = CsfDummyConversion;

	/*  make file the size of the header and data */
	fileSize   = nrRows*nrCols;
	fileSize <<= LOG_CELLSIZE(cellRepr);
	fileSize  += ADDR_DATA;

	/* enlarge the file to the length needed by seeking and writing
	one byte of crap */

	if ( csf_fseek(newMap->fp, fileSize-1, SEEK_SET) || /* fsetpos() is better */
	    newMap->write(&crap, (size_t)1, (size_t)1, newMap->fp) != 1 )
	{
		M_ERROR(NOSPACE);
		goto error_open;
	}
	(void)fflush(newMap->fp);
	if ( csf_ftell(newMap->fp) != (CSF_FADDR)fileSize)
	{
		M_ERROR(NOSPACE);
		goto error_open;
	}

	newMap->minMaxStatus = MM_KEEPTRACK;

	CsfRegisterMap(newMap);

	return(newMap);
error_open: 
	(void)fclose(newMap->fp);
error_notOpen: 
	CSF_FREE(newMap->fileName);
errorNameAlloc:
        CSF_FREE(newMap);
errorMapAlloc:
	return(NULL);
}
