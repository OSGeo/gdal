/******************************************************************************
 * $Id$
 *
 * Project:  Hierarchical Data Format Release 4 (HDF4) Reader
 * Purpose:  Header file for HDF4 datasets reader.
 * Author:   Andrey Kiselev, dron@at1895.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2002, Andrey Kiselev <a_kissel@eudoramail.com>
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
 ******************************************************************************
 * 
 * $Log$
 * Revision 1.1  2002/07/16 11:04:11  dron
 * New driver: HDF4 datasets. Initial version.
 *
 *
 */

typedef enum
{
    SEAWIFS_L1A,		// SeaWiFS Level-1A Data
    SEAWIFS_L2,			// SeaWiFS Level-2 Data
    SEAWIFS_L3,			// SeaWiFS Level-3 Standard Mapped Image
    MODIS_L1B,
    MOD02QKM_L1B,
    UNKNOWN
} HDF4Datatype;

/************************************************************************/
/* ==================================================================== */
/*				HDF4Dataset				*/
/* ==================================================================== */
/************************************************************************/

class HDF4Dataset : public GDALDataset
{

  protected:  
	  
    FILE	*fp;
    int32	hHDF4, hSD, hGR;
    int32	nDatasets, nImages, nFileAttrs;
    HDF4Datatype iDataType;
    char	*pszDataType;
    
    char	**papszSubDatasets;

  public:
                HDF4Dataset();
		~HDF4Dataset();
    
    const char *HDF4Dataset::GetDataTypeName( int32 );
    virtual char **GetMetadata( const char * pszDomain = "" );
    void TranslateHDF4Attributes( int32, int32, char *, int32, int32 );
    void TranslateHDF4EOSAttributes( int32, int32, int32 );
    static GDALDataset *Open( GDALOpenInfo * );

};



