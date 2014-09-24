
/*******************************************************/
/*	FUNCTIE   MOPEN.C	                       */
/*******************************************************/
/*******************************************************/


#include <string.h>

#include "csf.h"
#include "csfimpl.h"


static const char *openModes[3] = {
	S_READ,
	S_WRITE,
	S_READ_WRITE
	};

/* Return the access mode of m
 * MopenPerm returns the permission.
 * Note that M_WRITE is deprecated
 */
enum MOPEN_PERM MopenPerm(
	const MAP *m)
{
 return m->fileAccessMode;
}

/* open an existing CSF file
 * Mopen opens a CSF file. It allocates space for
 * the MAP runtime-structure, reads the header file
 * and performs test to determine if it is a CSF file.
 * The MinMaxStatus is set to MM_KEEPTRACK if the min/max
 * header fields are not MV or MM_WRONGVALUE if one of them
 * contains a MV.
 * returns a pointer the MAP runtime structure if the file is
 * successfully opened as a CSF file, NULL if not.
 *
 * Merrno
 * NOCORE BADACCESMODE OPENFAILED NOT_CSF BAD_VERSION
 *
 * EXAMPLE
 * .so examples/testcsf.tr
 */
MAP  *Mopen(
	const char *fileName, /* file name */
	enum MOPEN_PERM mode) /*  file permission */
{
 MAP *m;
 UINT4 s; /* swap detection field */
 
 if (! CsfIsBootedCsfKernel())
 	CsfBootCsfKernel();
 
 m = (MAP *)CSF_MALLOC(sizeof(MAP));
 
 if (m == NULL)
 {
 	M_ERROR(NOCORE);
 	goto error_mapMalloc;
 }
 
 m->fileName = (char *)CSF_MALLOC(strlen(fileName)+1);
 if (m->fileName == NULL)
 {
 	M_ERROR(NOCORE);
 	goto error_fnameMalloc;
 }
 (void)strcpy(m->fileName,fileName);
 
 /* check file mode validation */
 if ( IS_BAD_ACCESS_MODE(mode))
 {
 	M_ERROR(BADACCESMODE);
 	goto error_notOpen;
 }
 m->fileAccessMode = mode;
 
 
 /*  check if file can be opened or exists */
 m->fp = fopen(fileName, openModes[mode-1]);
 if (m->fp == NULL)
 {
 	M_ERROR(OPENFAILED);
 	goto error_notOpen;
 }
 
 /*  check if file could be C.S.F.-file 
  *   (at least 256 bytes long) 
  *  otherwise the signature comparison will
  *  fail
  */
 
 (void)fseek(m->fp,0L, SEEK_END);
 if (ftell(m->fp) < (long)ADDR_DATA)
 {
 	M_ERROR(NOT_CSF);
 	goto error_open;
 }

 (void)fseek(m->fp, 14+CSF_SIG_SPACE, SEEK_SET);
 (void)fread((void *)&s, sizeof(UINT4),(size_t)1,m->fp);
 if (s != ORD_OK) {
	m->write = CsfWriteSwapped;
	m->read  = CsfReadSwapped;
 }
 else {
#ifdef DEBUG
	m->read  = (CSF_READ_FUNC)CsfReadPlain;
	m->write = (CSF_READ_FUNC)CsfWritePlain;
#else
	m->read  = (CSF_READ_FUNC)fread;
	m->write = (CSF_READ_FUNC)fwrite;
#endif
 }
 
 (void)fseek(m->fp, ADDR_MAIN_HEADER, SEEK_SET);
 m->read((void *)&(m->main.signature), sizeof(char), CSF_SIG_SPACE,m->fp);
 m->read((void *)&(m->main.version),   sizeof(UINT2),(size_t)1,m->fp);
 m->read((void *)&(m->main.gisFileId), sizeof(UINT4),(size_t)1,m->fp);
 m->read((void *)&(m->main.projection),sizeof(UINT2),(size_t)1,m->fp);
 m->read((void *)&(m->main.attrTable), sizeof(UINT4),(size_t)1,m->fp);
 m->read((void *)&(m->main.mapType),  sizeof(UINT2),(size_t)1,m->fp);
 m->read((void *)&(m->main.byteOrder), sizeof(UINT4),(size_t)1,m->fp);
 /*                                             14+CSF_SIG_SPACE
  */
 
 (void)fseek(m->fp, ADDR_SECOND_HEADER, SEEK_SET);
 m->read((void *)&(m->raster.valueScale), sizeof(UINT2),(size_t)1,m->fp);
 m->read((void *)&(m->raster.cellRepr), sizeof(UINT2),(size_t)1,m->fp);

 (void)fread((void *)&(m->raster.minVal), sizeof(CSF_VAR_TYPE),(size_t)1,m->fp);
 (void)fread((void *)&(m->raster.maxVal), sizeof(CSF_VAR_TYPE),(size_t)1,m->fp);
 if (s != ORD_OK) {
  CsfSwap((void *)&(m->raster.minVal), CELLSIZE(m->raster.cellRepr),(size_t)1);
  CsfSwap((void *)&(m->raster.maxVal), CELLSIZE(m->raster.cellRepr),(size_t)1);
 }

 m->read((void *)&(m->raster.xUL), sizeof(REAL8),(size_t)1,m->fp);
 m->read((void *)&(m->raster.yUL), sizeof(REAL8),(size_t)1,m->fp);
 m->read((void *)&(m->raster.nrRows), sizeof(UINT4),(size_t)1,m->fp);
 m->read((void *)&(m->raster.nrCols), sizeof(UINT4),(size_t)1,m->fp);
 m->read((void *)&(m->raster.cellSize), sizeof(REAL8),(size_t)1,m->fp);
 m->read((void *)&(m->raster.cellSizeDupl), sizeof(REAL8),(size_t)1,m->fp);

 m->read((void *)&(m->raster.angle), sizeof(REAL8),(size_t)1,m->fp);


 /*  check signature C.S.F.file	
  */
 if(strncmp(m->main.signature,CSF_SIG,CSF_SIZE_SIG)!=0)
 {
 	M_ERROR(NOT_CSF);
 	goto error_open;
 }
 /* should be read right
  */
 POSTCOND(m->main.byteOrder == ORD_OK);
 /*  restore byteOrder C.S.F.file (Intel or Motorola)  */
 m->main.byteOrder=s;
 
 /*  check version C.S.F.file	
  */
 if (m->main.version != CSF_VERSION_1
    && (m->main.version != CSF_VERSION_2))
 {
 	M_ERROR(BAD_VERSION);
 	goto error_open;
 }
 
 if (m->main.version == CSF_VERSION_1)
 	m->raster.angle = 0.0;
 
 CsfFinishMapInit(m);
 
 CsfRegisterMap(m);
 
 /* install cell value converters: (app2file,file2app)
  */
 m->app2file = CsfDummyConversion;
 m->file2app = CsfDummyConversion;
 m->appCR    = m->raster.cellRepr;
 
 if (IsMV(m,&(m->raster.minVal)) ||
     IsMV(m,&(m->raster.maxVal))   )
 	m->minMaxStatus = MM_WRONGVALUE;
 else
 	m->minMaxStatus = MM_KEEPTRACK;
 
 return(m);
 
 error_open:
 PRECOND(m->fp != NULL);
 (void)fclose(m->fp);
 error_notOpen: 
 CSF_FREE(m->fileName);
 error_fnameMalloc: 
 CSF_FREE(m);
error_mapMalloc:
	 return(NULL);
}
