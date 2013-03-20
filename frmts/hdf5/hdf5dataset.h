/******************************************************************************
 * $Id$
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  Header file for HDF5 datasets reader.
 * Author:   Denis Nadeau (denis.nadeau@gmail.com)
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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

#ifndef _HDF5DATASET_H_INCLUDED_
#define _HDF5DATASET_H_INCLUDED_

#include "gdal_pam.h"
#include "cpl_list.h"

typedef struct HDF5GroupObjects {
  char         *pszName;
  char         *pszPath;
  char         *pszUnderscorePath;
  char         *pszTemp;
  int           nType;
  int           nIndex;
  hsize_t       nbObjs;
  int           nbAttrs;
  int           nRank;
  hsize_t       *paDims;
  hid_t          native;
  hid_t          HDatatype;
  unsigned long  objno[2];
  struct HDF5GroupObjects *poHparent;
  struct HDF5GroupObjects *poHchild;
} HDF5GroupObjects;


herr_t HDF5CreateGroupObjs(hid_t, const char *,void *);

/************************************************************************/
/* ==================================================================== */
/*                              HDF5Dataset                             */
/* ==================================================================== */
/************************************************************************/
class HDF5Dataset : public GDALPamDataset
{
  protected:

  hid_t            hHDF5;
  hid_t            hDatasetID;
  hid_t            hGroupID; /* H handler interface */
  char             **papszSubDatasets;
  int              bIsHDFEOS;
  int              nDatasetType;
  int              nSubDataCount;


  HDF5GroupObjects *poH5RootGroup; /* Contain hdf5 Groups information */

  CPLErr ReadGlobalAttributes(int);
  CPLErr HDF5ListGroupObjects(HDF5GroupObjects *, int );
  CPLErr CreateMetadata( HDF5GroupObjects *, int );

  HDF5GroupObjects* HDF5FindDatasetObjects( HDF5GroupObjects *, const char * );
  HDF5GroupObjects* HDF5FindDatasetObjectsbyPath( HDF5GroupObjects *, const char * );
  char* CreatePath(HDF5GroupObjects *);
  void DestroyH5Objects(HDF5GroupObjects *);

  GDALDataType GetDataType(hid_t);
  const char * GetDataTypeName(hid_t);

  /**
   * Reads an array of double attributes from the HDF5 metadata.
   * It reads the attributes directly on it's binary form directly,
   * thus avoiding string conversions.
   *
   * Important: It allocates the memory for the attributes internally,
   * so the caller must free the returned array after using it.
   * @param pszAttrName Name of the attribute to be read.
   * 			the attribute name must be the form:
   * 					root attribute name
   * 					SUBDATASET/subdataset attribute name
   * @param pdfValues pointer wich will store the array of doubles read.
   * @param nLen it stores the length of the array read. If NULL it doesn't inform
   *        the lenght of the array.
   * @return CPLErr CE_None in case of success, CE_Failure in case of failure
   */
  CPLErr HDF5ReadDoubleAttr(const char* pszAttrName,double **pdfValues,int *nLen=NULL);

  public:

  char	           **papszMetadata;
  HDF5GroupObjects *poH5CurrentObject;

  HDF5Dataset();
  ~HDF5Dataset();

  static GDALDataset *Open(GDALOpenInfo *);
  static int Identify(GDALOpenInfo *);
};



#endif /* _HDF5DATASET_H_INCLUDED_ */

