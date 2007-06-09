/******************************************************************************
 * levellerdataset.cpp,v 1.2 
 *
 * Project:  Leveller TER Driver
 * Purpose:  Reader for Leveller TER documents
 * Author:   Ray Gardener, Daylon Graphics Ltd.
 *
 * Portions of this module derived from GDAL drivers by 
 * Frank Warmerdam, see http://www.gdal.org
 *
 ******************************************************************************
 * Copyright (c) 2005-2007 Daylon Graphics Ltd.
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


#include "gdal_pam.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_Leveller(void);
CPL_C_END

#if 1

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

enum
{
	// Leveller coordsys types.
	LEV_COORDSYS_RASTER = 0,
	LEV_COORDSYS_LOCAL,
	LEV_COORDSYS_GEO
};

enum
{
	// Leveller digital axis extent styles.
	LEV_DA_POSITIONED = 0,
	LEV_DA_SIZED,
	LEV_DA_PIXEL_SIZED
};

enum
{
	// Measurement unit IDs, OEM version.
	UNITLABEL_YM		= 0x796D0000,
	UNITLABEL_ZM		= 0x7A6D0000,
	UNITLABEL_AM		= 0x616D0000,
	UNITLABEL_FM		= 0x666D0000,
	UNITLABEL_PM		= 0x706D0000,
	UNITLABEL_A			= 0x41000000,
	UNITLABEL_NM		= 0x6E6D0000,
	UNITLABEL_U			= 0x75000000,
	UNITLABEL_UM		= 0x756D0000,
	UNITLABEL_PPT		= 0x70707400,
	UNITLABEL_PT		= 0x70740000,
	UNITLABEL_MM		= 0x6D6D0000,
	UNITLABEL_P			= 0x70000000,
	UNITLABEL_CM		= 0x636D0000,
	UNITLABEL_IN		= 0x696E0000,
	UNITLABEL_DFT		= 0x64667400,
	UNITLABEL_DM		= 0x646D0000,
	UNITLABEL_LI		= 0x6C690000,
	UNITLABEL_SLI		= 0x736C6900,
	UNITLABEL_SP		= 0x73700000,
	UNITLABEL_FT		= 0x66740000,
	UNITLABEL_SFT		= 0x73667400,
	UNITLABEL_YD		= 0x79640000,
	UNITLABEL_SYD		= 0x73796400,
	UNITLABEL_M			= 0x6D000000,
	UNITLABEL_FATH		= 0x66617468,
	UNITLABEL_R			= 0x72000000,
	UNITLABEL_RD		= UNITLABEL_R,
	UNITLABEL_DAM		= 0x64416D00,
	UNITLABEL_DKM		= UNITLABEL_DAM,
	UNITLABEL_CH		= 0x63680000,
	UNITLABEL_SCH		= 0x73636800,
	UNITLABEL_HM		= 0x686D0000,
	UNITLABEL_F			= 0x66000000,
	UNITLABEL_KM		= 0x6B6D0000,
	UNITLABEL_MI		= 0x6D690000,
	UNITLABEL_SMI		= 0x736D6900,
	UNITLABEL_NMI		= 0x6E6D6900,
	UNITLABEL_MEGAM		= 0x4D6D0000,
	UNITLABEL_LS		= 0x6C730000,
	UNITLABEL_GM		= 0x476D0000,
	UNITLABEL_LM		= 0x6C6D0000,
	UNITLABEL_AU		= 0x41550000,
	UNITLABEL_TM		= 0x546D0000,
	UNITLABEL_LHR		= 0x6C687200,
	UNITLABEL_LD		= 0x6C640000,
	UNITLABEL_PETAM		= 0x506D0000,
	UNITLABEL_LY		= 0x6C790000,
	UNITLABEL_PC		= 0x70630000,
	UNITLABEL_EXAM		= 0x456D0000,
	UNITLABEL_KLY		= 0x6B6C7900,
	UNITLABEL_KPC		= 0x6B706300,
	UNITLABEL_ZETTAM	= 0x5A6D0000,
	UNITLABEL_MLY		= 0x4D6C7900,
	UNITLABEL_MPC		= 0x4D706300,
	UNITLABEL_YOTTAM	= 0x596D0000
};


typedef struct
{
	const char* pszID;
	double dScale;
	GInt32 oemCode;
} measurement_unit;

static const double kdays_per_year = 365.25;
static const double kdLStoM = 299792458.0;
static const double kdLYtoM = kdLStoM * kdays_per_year * 24 * 60 * 60;
static const double kdInch = 0.3048 / 12;

static const measurement_unit kUnits[] =
{
	{ "ym", 1.0e-24, UNITLABEL_YM },
	{ "zm", 1.0e-21, UNITLABEL_ZM }, 
	{ "am", 1.0e-18, UNITLABEL_AM },
	{ "fm", 1.0e-15, UNITLABEL_FM }, 
	{ "pm", 1.0e-12, UNITLABEL_PM }, 
	{ "A",  1.0e-10, UNITLABEL_A }, 
	{ "nm", 1.0e-9, UNITLABEL_NM }, 
	{ "u",  1.0e-6, UNITLABEL_U }, 
	{ "um", 1.0e-6, UNITLABEL_UM }, 
	{ "ppt", kdInch / 72.27, UNITLABEL_PPT }, 
	{ "pt", kdInch / 72.0, UNITLABEL_PT }, 
    { "mm", 1.0e-3, UNITLABEL_MM }, 
	{ "p", kdInch / 6.0, UNITLABEL_P }, 
	{ "cm", 1.0e-2, UNITLABEL_CM }, 
	{ "in", kdInch, UNITLABEL_IN }, 
	{ "dft", 0.03048, UNITLABEL_DFT }, 
	{ "dm", 0.1, UNITLABEL_DM }, 
	{ "li", 0.2011684 /* GDAL 0.20116684023368047 ? */, UNITLABEL_LI },  
	{ "sli", 0.201168402336805, UNITLABEL_SLI }, 
	{ "sp", 0.2286, UNITLABEL_SP }, 
	{ "ft", 0.3048, UNITLABEL_FT }, 
	{ "sft", 1200.0 / 3937.0, UNITLABEL_SFT }, 
    { "yd", 0.9144, UNITLABEL_YD }, 
	{ "syd", 0.914401828803658, UNITLABEL_SYD }, 
	{ "m", 1.0, UNITLABEL_M }, 
	{ "fath", 1.8288, UNITLABEL_FATH }, 
	{ "rd", 5.02921, UNITLABEL_RD }, 
	{ "dam", 10.0, UNITLABEL_DAM }, 
	{ "dkm", 10.0, UNITLABEL_DKM }, 
	{ "ch", 20.1168 /* GDAL: 2.0116684023368047 ? */, UNITLABEL_CH },  
	{ "sch", 20.1168402336805, UNITLABEL_SCH }, 
	{ "hm", 100.0, UNITLABEL_HM },
	{ "f", 201.168, UNITLABEL_F }, 
	{ "km", 1000.0, UNITLABEL_KM }, 
	{ "mi", 1609.344, UNITLABEL_MI }, 
	{ "smi", 1609.34721869444, UNITLABEL_SMI }, 
	{ "nmi", 1853.0, UNITLABEL_NMI }, 
	{ "Mm", 1.0e+6, UNITLABEL_MEGAM }, 
    { "ls", kdLStoM, UNITLABEL_LS }, 
	{ "Gm", 1.0e+9, UNITLABEL_GM }, 
	{ "lm", kdLStoM * 60, UNITLABEL_LM }, 
	{ "AU", 8.317 * kdLStoM * 60, UNITLABEL_AU }, 
	{ "Tm", 1.0e+12, UNITLABEL_TM }, 
	{ "lhr", 60.0 * 60.0 * kdLStoM, UNITLABEL_LHR }, 
	{ "ld", 24 * 60.0 * 60.0 * kdLStoM, UNITLABEL_LD },  
	{ "Pm", 1.0e+15, UNITLABEL_PETAM }, 
	{ "ly", kdLYtoM, UNITLABEL_LY }, 
	{ "pc", 3.2616 * kdLYtoM, UNITLABEL_PC }, 
	{ "Em", 1.0e+18, UNITLABEL_EXAM }, 
	{ "kly", 1.0e+3 * kdLYtoM, UNITLABEL_KLY }, 
	{ "kpc", 3.2616 * 1.0e+3 * kdLYtoM, UNITLABEL_KPC }, 
	{ "Zm", 1.0e+21, UNITLABEL_ZETTAM }, 
	{ "Mly", 1.0e+6 * kdLYtoM, UNITLABEL_MLY }, 
	{ "Mpc", 3.2616 * 1.0e+6 * kdLYtoM, UNITLABEL_MPC }, 
	{ "Ym", 1.0e+24, UNITLABEL_YOTTAM }
};


// ----------------------------------------------------------------

class LevellerRasterBand;

class LevellerDataset : public GDALPamDataset
{
    friend class LevellerRasterBand;
	friend class digital_axis;

    int			m_version;

    char*		m_pszProjection;

	char		m_szElevUnits[8];
    double		m_dElevScale;
    double		m_dElevBase;
    double		m_adfTransform[6];

    FILE*			m_fp;
    vsi_l_offset	m_nDataOffset;

    int         	load_from_file(FILE*);


    int locate_data(vsi_l_offset&, size_t&, FILE*, const char*);
    int get(int&, FILE*, const char*);
    int get(size_t& n, FILE* fp, const char* psz)
		{ return this->get((int&)n, fp, psz); }
    int get(double&, FILE*, const char*);
    int get(char*, size_t, FILE*, const char*);

    double convert_measure(double, const char* pszUnitsFrom);
	int make_local_coordsys(const char* pszName, const char* pszUnits);
	int make_local_coordsys(const char* pszName, int);
	const char* code_to_id(int) const;

public:
    LevellerDataset();
    ~LevellerDataset();
    
    static GDALDataset* Open( GDALOpenInfo* );
	static int Identify( GDALOpenInfo* );
    static GDALDataset* Create( const char* pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char** papszOptions );

    virtual CPLErr 	GetGeoTransform( double* );
    virtual const char*	GetProjectionRef(void);
};


class digital_axis
{
	public:
		digital_axis() : m_eStyle(LEV_DA_PIXEL_SIZED) {}

		int get(LevellerDataset& ds, FILE* fp, int n)
		{
			char szTag[32];
			sprintf(szTag, "coordsys_da%d_style", n);
			if(!ds.get(m_eStyle, fp, szTag))
				return FALSE;
			sprintf(szTag, "coordsys_da%d_fixedend", n);
			if(!ds.get(m_fixedEnd, fp, szTag))
				return FALSE;
			sprintf(szTag, "coordsys_da%d_v0", n);
			if(!ds.get(m_d[0], fp, szTag))
				return FALSE;
			sprintf(szTag, "coordsys_da%d_v1", n);
			if(!ds.get(m_d[1], fp, szTag))
				return FALSE;
			return TRUE;
		}

		double origin(size_t pixels) const
		{
			if(m_fixedEnd == 1)
			{
				switch(m_eStyle)
				{
					case LEV_DA_SIZED:
						return m_d[1] + m_d[0];

					case LEV_DA_PIXEL_SIZED:
						return m_d[1] + (m_d[0] * (pixels-1));
				}					
			}
			return m_d[0];
		}

		double scaling(size_t pixels) const
		{
			CPLAssert(pixels > 1);
			double d;
			if(m_eStyle == LEV_DA_PIXEL_SIZED)
				return m_d[1 - m_fixedEnd];

			return this->length(pixels) / (pixels - 1);
		}

		double length(int pixels) const
		{
			// Return the signed length of the axis.

			switch(m_eStyle)
			{
				case LEV_DA_POSITIONED:
					return m_d[1] - m_d[0];

				case LEV_DA_SIZED:
					return m_d[1 - m_fixedEnd];

				case LEV_DA_PIXEL_SIZED:
					return m_d[1 - m_fixedEnd] * (pixels-1);
					
			}
			CPLAssert(FALSE);
			return 0.0;
		}


	protected:
		int		m_eStyle;
		size_t	m_fixedEnd;
		double	m_d[2];
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
        for(size_t i = 0; i < nBlockXSize; i++)
            pf[i] = (float)pi[i] / 65536;
    }


#if 0
/* -------------------------------------------------------------------- */
/*      Convert raw elevations to realworld elevs.                      */
/* -------------------------------------------------------------------- */
    for(size_t i = 0; i < nBlockXSize; i++)
        pf[i] *= poGDS->m_dWorldscale; //this->GetScale();
#endif

    return CE_None;
}



/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/
const char *LevellerRasterBand::GetUnitType()
{
	// Return elevation units.

	LevellerDataset *poGDS = (LevellerDataset *) poDS;

    return poGDS->m_szElevUnits;
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


const char* LevellerDataset::code_to_id(int code) const
{
	// Convert a measurement unit's OEM ID to its readable ID. 

    for(size_t i = 0; i < array_size(kUnits); i++)
    {
        if(kUnits[i].oemCode == code)
            return kUnits[i].pszID;
    }
    CPLAssert(0);
    return NULL;
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

    for(size_t i = 0; i < array_size(kUnits); i++)
    {
        if(str_equal(pszSpace, kUnits[i].pszID))
            return d * kUnits[i].dScale;
    }
    CPLAssert(0);
    return d;
}

int LevellerDataset::make_local_coordsys(const char* pszName, const char* pszUnits)
{
	OGRSpatialReference sr;

	sr.SetLocalCS(pszName);
	if(OGRERR_NONE != sr.SetLinearUnits(pszUnits, 
		this->convert_measure(1.0, pszUnits)))
		return FALSE;

	return (OGRERR_NONE == sr.exportToWkt(&m_pszProjection));
}


int LevellerDataset::make_local_coordsys(const char* pszName, int code)
{
	return this->make_local_coordsys(pszName, this->code_to_id(code));
}



/************************************************************************/
/*                            load_from_file()                            */
/************************************************************************/

int LevellerDataset::load_from_file(FILE* file)
{
    // get hf dimensions
    if(!this->get(nRasterXSize, file, "hf_w"))
        return FALSE;

    if(!this->get(nRasterYSize, file, "hf_b"))
        return FALSE;

	if(nRasterXSize < 2 || nRasterYSize < 2)
		return FALSE; // Dimensions too small.

    // record start of pixel data
    size_t datalen;
    if(!this->locate_data(m_nDataOffset, datalen, file, "hf_data"))
        return FALSE;

    // sanity check: do we have enough pixels?
    if(datalen != nRasterXSize * nRasterYSize * sizeof(float))
        return FALSE;


	// Defaults for raster coordsys.
    m_adfTransform[0] = 0.0;
    m_adfTransform[1] = 1.0;
    m_adfTransform[2] = 0.0;
    m_adfTransform[3] = 0.0;
    m_adfTransform[4] = 0.0;
    m_adfTransform[5] = 1.0;

	m_dElevScale = 1.0;
    m_dElevBase = 0.0;
	strcpy(m_szElevUnits, "");

	if(m_version == 7)
	{
		// Read coordsys info.
		int csclass = LEV_COORDSYS_RASTER;
	    (void)this->get(csclass, file, "csclass");

		if(csclass != LEV_COORDSYS_RASTER)
		{
			// Get projection details and units.

			CPLAssert(m_pszProjection == NULL);

			if(csclass == LEV_COORDSYS_LOCAL)
			{
				int unitcode;
				//char szLocalUnits[8];
				if(!this->get(unitcode, file, "coordsys_units"))
					unitcode = UNITLABEL_M;

				if(!this->make_local_coordsys("Leveller", unitcode))
					return FALSE;
			}
			else if(csclass == LEV_COORDSYS_GEO)
			{
				char szWKT[1024];
				if(!this->get(szWKT, 1023, file, "coordsys_wkt"))
					return 0;

				m_pszProjection = (char*)CPLMalloc(strlen(szWKT) + 1);
				strcpy(m_pszProjection, szWKT);
			}

			// Get ground extents.
			digital_axis axis_ns, axis_ew;

			if(axis_ns.get(*this, file, 0)
				&& axis_ew.get(*this, file, 1))
			{
				m_adfTransform[0] = axis_ew.origin(nRasterXSize);
				m_adfTransform[1] = axis_ew.scaling(nRasterXSize);
				m_adfTransform[2] = 0.0;

				m_adfTransform[3] = axis_ns.origin(nRasterYSize);
				m_adfTransform[4] = 0.0;
				m_adfTransform[5] = axis_ns.scaling(nRasterYSize);
			}
		}

		// Get vertical (elev) coordsys.
		int bHasVertCS = FALSE;
		if(this->get(bHasVertCS, file, "coordsys_haselevm") && bHasVertCS)
		{
			this->get(m_dElevScale, file, "coordsys_em_scale");
			this->get(m_dElevBase, file, "coordsys_em_base");
			int unitcode;
			if(this->get(unitcode, file, "coordsys_em_units"))
				strcpy(m_szElevUnits, this->code_to_id(unitcode));
			// datum and localcs are currently unused.
		}
	}
	else
	{
		// Legacy files use world units.
	    char szWorldUnits[32];
		strcpy(szWorldUnits, "m");

	    double dWorldscale = 1.0;

		if(this->get(dWorldscale, file, "hf_worldspacing"))
		{
			//m_bHasWorldscale = true;
			if(this->get(szWorldUnits, sizeof(szWorldUnits)-1, file, 
				"hf_worldspacinglabel"))
			{
				// Drop long name, if present.
				char* p = strchr(szWorldUnits, ' ');
				if(p != NULL)
					*p = 0;
			}

#if 0
			// If the units are something besides m/ft/sft, 
			// then convert them to meters.

			if(!str_equal("m", szWorldUnits)
			   && !str_equal("ft", szWorldUnits)
			   && !str_equal("sft", szWorldUnits))
			{
				dWorldscale = this->convert_measure(dWorldscale, szWorldUnits);
				strcpy(szWorldUnits, "m");
			}
#endif

			// Our extents are such that the origin is at the 
			// center of the heightfield.
			m_adfTransform[0] = -0.5 * dWorldscale * (nRasterXSize-1);
			m_adfTransform[3] = -0.5 * dWorldscale * (nRasterYSize-1);
			m_adfTransform[1] = dWorldscale;
			m_adfTransform[5] = dWorldscale;
		}
		m_dElevScale = dWorldscale; // this was 1.0 before because 
		// we were converting to real elevs ourselves, but
		// some callers may want both the raw pixels and the 
		// transform to get real elevs.

		if(!this->make_local_coordsys("Leveller world space", szWorldUnits))
			return FALSE;
	}

    return TRUE;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char*	LevellerDataset::GetProjectionRef(void)
{
    return (m_pszProjection == NULL ? "" : m_pszProjection);
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr LevellerDataset::GetGeoTransform(double* padfTransform)

{
	memcpy(padfTransform, m_adfTransform, sizeof(m_adfTransform));
    return CE_None;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int LevellerDataset::Identify( GDALOpenInfo * poOpenInfo )
{
    if( poOpenInfo->nHeaderBytes < 4 )
        return FALSE;

    return EQUALN((const char *) poOpenInfo->pabyHeader, "trrn", 4);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *LevellerDataset::Open( GDALOpenInfo * poOpenInfo )
{
    // The file should have at least 5 header bytes
    // and hf_w, hf_b, and hf_data tags.
#ifdef DEBUG

#endif
    if( poOpenInfo->nHeaderBytes < 5+13+13+16 )
        return NULL;

	if( !LevellerDataset::Identify(poOpenInfo))
		return NULL;

    const int version = poOpenInfo->pabyHeader[4];
    if(version < 4 || version > 7)
        return NULL;


/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */

	LevellerDataset* poDS = new LevellerDataset();

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
/*	Read the file.							                            */
/* -------------------------------------------------------------------- */
    if( !poDS->load_from_file( poDS->m_fp ) )
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

#else
// stub so that module compiles
class LevellerDataset : public GDALPamDataset
{
	public:
		static GDALDataset* Open( GDALOpenInfo* ) { return NULL; }
};
#endif

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
        
#if GDAL_VERSION_NUM > 1410
        poDriver->pfnIdentify = LevellerDataset::Identify;
#endif
        poDriver->pfnOpen = LevellerDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
