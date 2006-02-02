/******************************************************************************
 * levellerdataset.cpp,v 1.0 
 *
 * Project:  Leveller TER Driver
 * Purpose:  Reader for Leveller TER documents
 * Author:   Ray Gardener, Daylon Graphics Ltd.
 *
 * Portions of this module derived from GDAL drivers by 
 * Frank Warmerdam, see http://www.gdal.org
 *
 ******************************************************************************
 * Copyright (c) 2005 Daylon Graphics Ltd.
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
 * Revision 1.4  2006/02/02 22:30:49  fwarmerdam
 * byte swapping fix for mac
 *
 * Revision 1.3  2005/10/21 00:03:41  fwarmerdam
 * added coordinate system support from Ray
 *
 * Revision 1.2  2005/10/20 20:18:14  fwarmerdam
 * Applied some patches from Ray.
 *
 * Revision 1.1  2005/10/20 13:44:29  fwarmerdam
 * New
 *
 */


#include "gdal_pam.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_Leveller(void);
CPL_C_END


#define str_equal(_s1, _s2)	(0 == strcmp((_s1),(_s2)))
#define array_size(_a)		(sizeof(_a) / sizeof(_a[0]))

/*GDALDataset *LevellerCreateCopy( const char *, GDALDataset *, int, char **,
                                GDALProgressFunc pfnProgress, 
                                void * pProgressData );

*/

/************************************************************************/
/* ==================================================================== */
/*				LevellerDataset				*/
/* ==================================================================== */
/************************************************************************/

class LevellerRasterBand;

class LevellerDataset : public GDALPamDataset
{
    friend class LevellerRasterBand;

    int			 m_version;

    char*		m_pszProjection;

    char		m_szWorldscaleUnits[32];
    double		m_dWorldscale;
    double		m_dElevScale;
    double		m_dElevBase;
    double		m_adfTransform[6];

    FILE*		m_fp;
    vsi_l_offset	m_nDataOffset;

    int         	LoadFromFile(FILE*);


    int locate_data(vsi_l_offset&, size_t&, FILE*, const char*);
    int get(int&, FILE*, const char*);
    int get(double&, FILE*, const char*);
    int get(char*, size_t, FILE*, const char*);

    double convert_measure(double, const char* pszUnitsFrom);

public:
    LevellerDataset();
    ~LevellerDataset();
    
    static GDALDataset* Open( GDALOpenInfo* );
    static GDALDataset* Create( const char* pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char** papszOptions );

    virtual CPLErr 	GetGeoTransform( double* );
    virtual const char*	GetProjectionRef(void);
};

/************************************************************************/
/* ==================================================================== */
/*                            LevellerRasterBand                        */
/* ==================================================================== */
/************************************************************************/

class LevellerRasterBand : public GDALPamRasterBand
{
    friend class LevellerDataset;

public:

    LevellerRasterBand(LevellerDataset*);
    
    // Geomeasure support.
    virtual const char* GetUnitType();
    virtual double GetScale(int* pbSuccess = NULL);
    virtual double GetOffset(int* pbSuccess = NULL);

    virtual CPLErr IReadBlock( int, int, void * );
};


/************************************************************************/
/*                         LevellerRasterBand()                         */
/************************************************************************/

LevellerRasterBand::LevellerRasterBand( LevellerDataset *poDS )
{
    this->poDS = poDS;
    this->nBand = 1;

    eDataType = GDT_Float32;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;//poDS->GetRasterYSize();
}


/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr LevellerRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                       void* pImage )

{
    CPLAssert( sizeof(float) == sizeof(GInt32) );
    CPLAssert( nBlockXOff == 0  );
    CPLAssert( pImage != NULL );

    LevellerDataset *poGDS = (LevellerDataset *) poDS;

/* -------------------------------------------------------------------- */
/*      Seek to scanline.                                               */
/* -------------------------------------------------------------------- */
    const size_t rowbytes = nBlockXSize * sizeof(float);

    if(0 != VSIFSeekL(
           poGDS->m_fp, 
           poGDS->m_nDataOffset + nBlockYOff * rowbytes, 
           SEEK_SET))
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  ".bt Seek failed:%s", VSIStrerror( errno ) );
        return CE_Failure;
    }


/* -------------------------------------------------------------------- */
/*      Read the scanline into the image buffer.                        */
/* -------------------------------------------------------------------- */

    if( VSIFReadL( pImage, rowbytes, 1, poGDS->m_fp ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Leveller read failed:%s", VSIStrerror( errno ) );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Swap on MSB platforms.                                          */
/* -------------------------------------------------------------------- */
#ifdef CPL_MSB 
    GDALSwapWords( pImage, 4, nRasterXSize, 4 );
#endif    

/* -------------------------------------------------------------------- */
/*      Convert from legacy-format fixed-point if necessary.            */
/* -------------------------------------------------------------------- */
    float* pf = (float*)pImage;
	
    if(poGDS->m_version < 6)
    {
        GInt32* pi = (int*)pImage;
        for(int i = 0; i < nBlockXSize; i++)
            pf[i] = (float)pi[i] / 65536;
    }


/* -------------------------------------------------------------------- */
/*      Convert raw elevations to realworld elevs.                      */
/* -------------------------------------------------------------------- */
    for(int i = 0; i < nBlockXSize; i++)
        pf[i] *= poGDS->m_dWorldscale; //this->GetScale();

    return CE_None;
}



/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/
const char *LevellerRasterBand::GetUnitType()
{
	// Return elevation units.
	// For Leveller documents, it's the same as the ground units.
    LevellerDataset *poGDS = (LevellerDataset *) poDS;

    return poGDS->m_szWorldscaleUnits;
}


/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double LevellerRasterBand::GetScale(int* pbSuccess)
{
    LevellerDataset *poGDS = (LevellerDataset *) poDS;
	if(pbSuccess != NULL)
		*pbSuccess = TRUE;
	return poGDS->m_dElevScale;
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

double LevellerRasterBand::GetOffset(int* pbSuccess)
{
    LevellerDataset *poGDS = (LevellerDataset *) poDS;
	if(pbSuccess != NULL)
		*pbSuccess = TRUE;
	return poGDS->m_dElevBase;
}


/************************************************************************/
/* ==================================================================== */
/*				LevellerDataset		                */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          LevellerDataset()                           */
/************************************************************************/

LevellerDataset::LevellerDataset()

{
    m_fp = NULL;
	m_pszProjection = NULL;
}

/************************************************************************/
/*                          ~LevellerDataset()                          */
/************************************************************************/

LevellerDataset::~LevellerDataset()

{
    FlushCache();

	CPLFree(m_pszProjection);

    if( m_fp != NULL )
        VSIFCloseL( m_fp );
}

/************************************************************************/
/*                            LoadFromFile()                            */
/*                                                                      */
/*      If the data from DEM is in meters, then values are stored as    */
/*      shorts. If DEM data is in feet, then height data will be        */
/*      stored in float, to preserve the precision of the original      */
/*      data. returns true if the file was successfully opened and      */
/*      read.                                                           */
/************************************************************************/

int LevellerDataset::locate_data(vsi_l_offset& offset, size_t& len, FILE* fp, const char* pszTag)
{
    // Locate the file offset of the desired tag's data.
    // If it is not available, return false.
    // If the tag is found, leave the filemark at the 
    // start of its data.

    if(0 != VSIFSeekL(fp, 5, SEEK_SET))
        return 0;

    const int kMaxDescLen = 64;
    for(;;)
    {
        unsigned char c;
        if(1 != VSIFReadL(&c, sizeof(c), 1, fp))
            return 0;
        const int descriptorLen = c;
        if(descriptorLen == 0 || descriptorLen > kMaxDescLen)
            return 0;

        char descriptor[kMaxDescLen+1];
        if(1 != VSIFReadL(descriptor, descriptorLen, 1, fp))
            return 0;

        GInt32 datalen;
        if(1 != VSIFReadL(&datalen, sizeof(datalen), 1, fp))
            return 0;

        datalen = CPL_LSBWORD32(datalen);
        descriptor[descriptorLen] = 0;
        if(str_equal(descriptor, pszTag))
        {
            len = (size_t)datalen;
            offset = VSIFTellL(fp);
            return 1;
        }
        else
        {
            // Seek to next tag.
            if(0 != VSIFSeekL(fp, (vsi_l_offset)datalen, SEEK_CUR))
                return 0;
        }
    }
    return 0;
}

/************************************************************************/
/*                                get()                                 */
/************************************************************************/

int LevellerDataset::get(int& n, FILE* fp, const char* psz)
{
    vsi_l_offset offset;
    size_t		 len;

    if(this->locate_data(offset, len, fp, psz))
    {
        GInt32 value;
        if(1 == VSIFReadL(&value, sizeof(value), 1, fp))
        {
            CPL_LSBPTR32(&value);
            n = (int)value;
            return 1;
        }
    }	
    return 0;
}

/************************************************************************/
/*                                get()                                 */
/************************************************************************/

int LevellerDataset::get(double& d, FILE* fp, const char* pszTag)
{
    vsi_l_offset offset;
    size_t		 len;

    if(this->locate_data(offset, len, fp, pszTag))
    {
        if(1 == VSIFReadL(&d, sizeof(d), 1, fp))
        {
            CPL_LSBPTR64(&d);
            return 1;
        }
    }	
    return 0;
}


/************************************************************************/
/*                                get()                                 */
/************************************************************************/
int LevellerDataset::get(char* pszValue, size_t maxchars, FILE* fp, const char* pszTag)
{
    char szTag[65];

    // We can assume 8-bit encoding, so just go straight
    // to the *_d tag.
    sprintf(szTag, "%sd", pszTag);

    vsi_l_offset offset;
    size_t		 len;

    if(this->locate_data(offset, len, fp, szTag))
    {
        if(len > maxchars)
            return 0;

        if(1 == VSIFReadL(pszValue, len, 1, fp))
        {
            pszValue[len] = 0; // terminate C-string
            return 1;
        }
    }	

    return 0;
}


/************************************************************************/
/*                          convert_measure()                           */
/************************************************************************/

double LevellerDataset::convert_measure
(
	double d, 
	const char* pszSpace 
)
{
    // Convert a measure to meters.

    const char* szLabels[] = 
	{
            "fm", "pm", "A", "nm", "u", "um", 
            "mm", "cm", "in", "dft", "dm", "sp", "ft", "sft", 
            "yd", "m", "r", "dam", "dkm", "f", "km", "mi", "nmi", 
            "ls", "lm", "AU", "lhr", "ld",  "ly", "pc", "kly"
	};

    const double dLYtoM = 9.4608953536e+15;

    const double dScales[] = 
	{
            1.0e-15, // fm (femtometer)
            1.0e-12, // pm (picometer)
            1.0e-10, // A (angstrom)
            1.0e-9, // nm (nanometer)
            1.0e-6, // u (micron)
            1.0e-6, // um (micrometer)
            1.0e-3, // mm (millimeter)
            1.0e-2, // cm (centimeter)
            0.0254, // in (inch)
            0.03048, // dft (decifoot)
            1.0e-1, // dm (decimeter)
            0.2286, // sp (span)
            0.3048,	// ft (foot)
            1200.0 / 3937.0, // sft (survey foot)
            0.9144, // yd (yard)
            1.0, // m (meter)
            5.029, // r (rod)
            10.0, // dam (decameter)
            10.0, // dkm (decameter)
            201.168, // f (furlong)
            1.0e+3, // km
            1609.344,// mi (mile) //1611.7874086021505376344086021505, 
            1853.0, // nmi (nautical mile)
            dLYtoM / 3.16E+07, // ls (light second)
            // 299,367,088.60759493670886075949367 meters.
            // 299,791,819.008  if we use 186,282 miles per ls.
            dLYtoM / 5.26E+05, // lm (light minute)
            1.50e+11, // AU (astronomical units)
            dLYtoM / 5.26E+05 * 60 , // lhr (light hour)
            dLYtoM / 5.26E+05 * 60 * 24 , // ld (light day)
            dLYtoM, // ly (light year)
            dLYtoM * 3.26, // 3.08e+16, // pc (parsec)
            dLYtoM * 1000, // kly (kilo light year)
	};

    CPLAssert(array_size(dScales) == array_size(szLabels));

    for(size_t i = 0; i < array_size(dScales); i++)
    {
        if(str_equal(pszSpace, szLabels[i]))
            return d * dScales[i];
    }
    CPLAssert(0);
    return d;
}

/************************************************************************/
/*                            LoadFromFile()                            */
/************************************************************************/

int LevellerDataset::LoadFromFile(FILE* file)
{
    // get hf dimensions
    if(!this->get(nRasterXSize, file, "hf_w"))
        return 0;

    if(!this->get(nRasterYSize, file, "hf_b"))
        return 0;

    // record start of pixel data
    size_t datalen;
    if(!this->locate_data(m_nDataOffset, datalen, file, "hf_data"))
        return 0;

    // sanity check: do we have enough pixels?
    if(datalen != nRasterXSize * nRasterYSize * sizeof(float))
        return 0;

    // Read any world scaling.
    m_dWorldscale = 1.0;
    strcpy(m_szWorldscaleUnits, "m");

    m_dElevBase = 0.0;

    m_adfTransform[0] = 0.0;
    m_adfTransform[1] = 1.0;
    m_adfTransform[2] = 0.0;
    m_adfTransform[3] = 0.0;
    m_adfTransform[4] = 0.0;
    m_adfTransform[5] = 1.0;

    if(this->get(m_dWorldscale, file, "hf_worldspacing"))
    {
        //m_bHasWorldscale = true;
        if(this->get(m_szWorldscaleUnits, sizeof(m_szWorldscaleUnits)-1, file, "hf_worldspacinglabel"))
        {
            // Drop long name, if present.
            char* p = strchr(m_szWorldscaleUnits, ' ');
            if(p != NULL)
                *p = 0;
        }

        // If the units are something besides m/ft/sft, 
        // then convert them to meters.

        if(!str_equal("m", m_szWorldscaleUnits)
           && !str_equal("ft", m_szWorldscaleUnits)
           && !str_equal("sft", m_szWorldscaleUnits))
        {
            m_dWorldscale = this->convert_measure(m_dWorldscale, m_szWorldscaleUnits);
            strcpy(m_szWorldscaleUnits, "m");
        }

        // Our extents are such that the origin is at the 
        // center of the heightfield.
        m_adfTransform[0] = -0.5 * m_dWorldscale * nRasterXSize;
        m_adfTransform[3] = -0.5 * m_dWorldscale * nRasterYSize;
        m_adfTransform[1] = m_dWorldscale;
        m_adfTransform[5] = m_dWorldscale;
    }
    m_dElevScale = 1.0;//m_dWorldscale;


/* -------------------------------------------------------------------- */
/*      Set projection.													*/
/* -------------------------------------------------------------------- */
	// Leveller files as of Oct 2005 are not currently georeferenced,
	// but we can't indicate ground units without a spatialref,
	// so make a default geocs (local).
    OGRSpatialReference sr;

    sr.SetLocalCS("Leveller world space");
	if(OGRERR_NONE != sr.SetLinearUnits(m_szWorldscaleUnits, 
		this->convert_measure(1.0, m_szWorldscaleUnits)))
		return 0;

    if(OGRERR_NONE != sr.exportToWkt(&m_pszProjection))
		return 0;


    return TRUE;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char*	LevellerDataset::GetProjectionRef(void)
{
    if(m_pszProjection == NULL )
        return "";
    else
        return m_pszProjection;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr LevellerDataset::GetGeoTransform(double* padfTransform)

{
    // Return identity transform, since Leveller heightfields 
    // do not currently have geodata.

    memcpy(padfTransform, m_adfTransform, sizeof(m_adfTransform));

    return CE_None;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *LevellerDataset::Open( GDALOpenInfo * poOpenInfo )

{
    // The file should have at least 5 header bytes
    // and hf_w, hf_b, and hf_data tags.
    if( poOpenInfo->nHeaderBytes < 5+13+13+16 )
        return NULL;

    if( !EQUALN((const char *) poOpenInfo->pabyHeader, "trrn",4) )
        return NULL;

    const int version = poOpenInfo->pabyHeader[4];
    if(version < 4 || version > 6)
        return NULL;


/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    LevellerDataset 	*poDS;

    poDS = new LevellerDataset();

    poDS->m_version = version;

    // Reopen for large file access.
    if( poOpenInfo->eAccess == GA_Update )
        poDS->m_fp = VSIFOpenL( poOpenInfo->pszFilename, "rb+" );
    else
        poDS->m_fp = VSIFOpenL( poOpenInfo->pszFilename, "rb" );

    if( poDS->m_fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to re-open %s within Leveller driver.\n",
                  poOpenInfo->pszFilename );
        return NULL;
    }
    poDS->eAccess = poOpenInfo->eAccess;

    
/* -------------------------------------------------------------------- */
/*	Read the file.							*/
/* -------------------------------------------------------------------- */
    if( !poDS->LoadFromFile( poDS->m_fp ) )
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->SetBand( 1, new LevellerRasterBand( poDS ));

    poDS->SetMetadataItem( GDALMD_AREA_OR_POINT, GDALMD_AOP_POINT );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

    return( poDS );
}

/************************************************************************/
/*                        GDALRegister_Leveller()                       */
/************************************************************************/

void GDALRegister_Leveller()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "Leveller" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "Leveller" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, 
                                   "ter" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Leveller heightfield" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_leveller.html" );
        
        poDriver->pfnOpen = LevellerDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
