/******************************************************************************
 * levellerdataset.cpp,v 1.22
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
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

static const size_t kMaxTagNameLen = 63;

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

typedef enum
{
	// Measurement unit IDs, OEM version.
	UNITLABEL_UNKNOWN	= 0x00000000,
	UNITLABEL_PIXEL		= 0x70780000,
	UNITLABEL_PERCENT	= 0x25000000,

	UNITLABEL_RADIAN	= 0x72616400,
	UNITLABEL_DEGREE	= 0x64656700,
	UNITLABEL_ARCMINUTE	= 0x6172636D,
	UNITLABEL_ARCSECOND	= 0x61726373,

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
} UNITLABEL;


typedef struct
{
	const char* pszID;
	double		dScale;
	UNITLABEL	oemCode;
} measurement_unit;

static const double kdays_per_year = 365.25;
static const double kdLStoM = 299792458.0;
static const double kdLYtoM = kdLStoM * kdays_per_year * 24 * 60 * 60;
static const double kdInch = 0.3048 / 12;
static const double kPI = 3.1415926535897932384626433832795;

static const int kFirstLinearMeasureIdx = 9;

static const measurement_unit kUnits[] =
{
	{ "", 1.0, UNITLABEL_UNKNOWN },
	{ "px", 1.0, UNITLABEL_PIXEL },
	{ "%", 1.0, UNITLABEL_PERCENT }, // not actually used

	{ "rad", 1.0, UNITLABEL_RADIAN },
	{ "\xB0", kPI / 180.0, UNITLABEL_DEGREE }, // \xB0 is Unicode degree symbol 
	{ "d", kPI / 180.0, UNITLABEL_DEGREE },
	{ "deg", kPI / 180.0, UNITLABEL_DEGREE },
	{ "'", kPI / (60.0 * 180.0), UNITLABEL_ARCMINUTE },
	{ "\"", kPI / (3600.0 * 180.0), UNITLABEL_ARCSECOND },

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

static bool approx_equal(double a, double b)
{
	const double epsilon = 1e-5;
	return (fabs(a-b) <= epsilon);
}


// ----------------------------------------------------------------

class LevellerRasterBand;

class LevellerDataset : public GDALPamDataset
{
    friend class LevellerRasterBand;
	friend class digital_axis;

    int			m_version;

	char*		m_pszFilename;
    char*		m_pszProjection;

    //char		m_szUnits[8];
	char		m_szElevUnits[8];
    double		m_dElevScale;	// physical-to-logical scaling.
    double		m_dElevBase;	// logical offset.
    double		m_adfTransform[6];
	//double		m_dMeasurePerPixel;
	double		m_dLogSpan[2];

    VSILFILE*			m_fp;
    vsi_l_offset	m_nDataOffset;

    bool load_from_file(VSILFILE*, const char*);


    bool locate_data(vsi_l_offset&, size_t&, VSILFILE*, const char*);
    bool get(int&, VSILFILE*, const char*);
    bool get(size_t& n, VSILFILE* fp, const char* psz)
		{ return this->get((int&)n, fp, psz); }
    bool get(double&, VSILFILE*, const char*);
    bool get(char*, size_t, VSILFILE*, const char*);

	bool write_header();
	bool write_tag(const char*, int);
	bool write_tag(const char*, size_t);
	bool write_tag(const char*, double);
	bool write_tag(const char*, const char*);
	bool write_tag_start(const char*, size_t);
	bool write(int);
	bool write(size_t);
	bool write(double);
	bool write_byte(size_t);

	const measurement_unit* get_uom(const char*) const;
	const measurement_unit* get_uom(UNITLABEL) const;
	const measurement_unit* get_uom(double) const;

    bool convert_measure(double, double&, const char* pszUnitsFrom);
	bool make_local_coordsys(const char* pszName, const char* pszUnits);
	bool make_local_coordsys(const char* pszName, UNITLABEL);
	const char* code_to_id(UNITLABEL) const;
	UNITLABEL id_to_code(const char*) const;
	UNITLABEL meter_measure_to_code(double) const;
	bool compute_elev_scaling(const OGRSpatialReference&);
	void raw_to_proj(double, double, double&, double&);

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

    virtual CPLErr 	SetGeoTransform( double* );
    virtual CPLErr	SetProjection(const char*);
};


class digital_axis
{
	public:
		digital_axis() : m_eStyle(LEV_DA_PIXEL_SIZED) {}

		bool get(LevellerDataset& ds, VSILFILE* fp, int n)
		{
			char szTag[32];
			sprintf(szTag, "coordsys_da%d_style", n);
			if(!ds.get(m_eStyle, fp, szTag))
				return false;
			sprintf(szTag, "coordsys_da%d_fixedend", n);
			if(!ds.get(m_fixedEnd, fp, szTag))
				return false;
			sprintf(szTag, "coordsys_da%d_v0", n);
			if(!ds.get(m_d[0], fp, szTag))
				return false;
			sprintf(szTag, "coordsys_da%d_v1", n);
			if(!ds.get(m_d[1], fp, szTag))
				return false;
			return true;
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

	float*	m_pLine;
	bool	m_bFirstTime;

public:

    LevellerRasterBand(LevellerDataset*);
    ~LevellerRasterBand();
    
    // Geomeasure support.
    virtual const char* GetUnitType();
    virtual double GetScale(int* pbSuccess = NULL);
    virtual double GetOffset(int* pbSuccess = NULL);

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );
	virtual CPLErr SetUnitType( const char* );
};


/************************************************************************/
/*                         LevellerRasterBand()                         */
/************************************************************************/

LevellerRasterBand::LevellerRasterBand( LevellerDataset *poDS )
	:
	m_pLine(NULL),
	m_bFirstTime(true)
{
    this->poDS = poDS;
    this->nBand = 1;

    eDataType = GDT_Float32;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;//poDS->GetRasterYSize();

	m_pLine = (float*)CPLMalloc(sizeof(float) * nBlockXSize);
}


LevellerRasterBand::~LevellerRasterBand()
{
	if(m_pLine != NULL)
		CPLFree(m_pLine);
}

/************************************************************************/
/*                             IWriteBlock()                            */
/************************************************************************/

CPLErr LevellerRasterBand::IWriteBlock
( 
	int nBlockXOff, 
	int nBlockYOff,
    void* pImage
)
{
    CPLAssert( nBlockXOff == 0  );
    CPLAssert( pImage != NULL );
	CPLAssert( m_pLine != NULL );

/*	#define sgn(_n) ((_n) < 0 ? -1 : ((_n) > 0 ? 1 : 0) )
	#define sround(_f)	\
		(int)((_f) + (0.5 * sgn(_f)))
*/
	const size_t pixelsize = sizeof(float);

	LevellerDataset& ds = *(LevellerDataset*)poDS;
	if(m_bFirstTime)
	{
		m_bFirstTime = false;
		if(!ds.write_header())
			return CE_Failure;
		ds.m_nDataOffset = VSIFTellL(ds.m_fp);
	}
	const size_t rowbytes = nBlockXSize * pixelsize;
	const float* pfImage = (float*)pImage;

	if(0 == VSIFSeekL(
       ds.m_fp, ds.m_nDataOffset + nBlockYOff * rowbytes, 
       SEEK_SET))
	{
		for(size_t x = 0; x < (size_t)nBlockXSize; x++)
		{
			// Convert logical elevations to physical.
                    m_pLine[x] = (float) 
				((pfImage[x] - ds.m_dElevBase) / ds.m_dElevScale);
		}

#ifdef CPL_MSB 
		GDALSwapWords( m_pLine, pixelsize, nBlockXSize, pixelsize );
#endif    
		if(1 == VSIFWriteL(m_pLine, rowbytes, 1, ds.m_fp))
			return CE_None;
	}

	return CE_Failure;
}


CPLErr LevellerRasterBand::SetUnitType( const char* psz )
{
	LevellerDataset& ds = *(LevellerDataset*)poDS;

	if(strlen(psz) >= sizeof(ds.m_szElevUnits))
		return CE_Failure;

	strcpy(ds.m_szElevUnits, psz);

	return CE_None;
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
        for(size_t i = 0; i < (size_t)nBlockXSize; i++)
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
    m_pszFilename = NULL;
}

/************************************************************************/
/*                          ~LevellerDataset()                          */
/************************************************************************/

LevellerDataset::~LevellerDataset()
{
    FlushCache();

    CPLFree(m_pszProjection);
    CPLFree(m_pszFilename);

    if( m_fp != NULL )
        VSIFCloseL( m_fp );
}

static double degrees_to_radians(double d) 
{
	return (d * 0.017453292);
}


static double average(double a, double b)
{
	return 0.5 * (a + b);
}


void LevellerDataset::raw_to_proj(double x, double y, double& xp, double& yp)
{
	xp = x * m_adfTransform[1] + m_adfTransform[0];
	yp = y * m_adfTransform[5] + m_adfTransform[3];
}


bool LevellerDataset::compute_elev_scaling
(
	const OGRSpatialReference& sr
)
{
	const char* pszGroundUnits;

	if(!sr.IsGeographic())
	{
		// For projected or local CS, the elev scale is
		// the average ground scale.
		m_dElevScale = average(m_adfTransform[1], m_adfTransform[5]);

		const double dfLinear = sr.GetLinearUnits();
		const measurement_unit* pu = this->get_uom(dfLinear);
		if(pu == NULL)
			return false;

		pszGroundUnits = pu->pszID;
	}
	else
	{
		pszGroundUnits = "m";

		const double kdEarthCircumPolar = 40007849;
		const double kdEarthCircumEquat = 40075004;

		double xr, yr;
		xr = 0.5 * this->nRasterXSize;
		yr = 0.5 * this->nRasterYSize;

		double xg[2], yg[2];
		this->raw_to_proj(xr, yr, xg[0], yg[0]);
		this->raw_to_proj(xr+1, yr+1, xg[1], yg[1]);
		
		// The earths' circumference shrinks using a sin()
		// curve as we go up in latitude.
		const double dLatCircum = kdEarthCircumEquat 
			* sin(degrees_to_radians(90.0 - yg[0]));

		// Derive meter distance between geolongitudes
		// in xg[0] and xg[1].
		double dx = fabs(xg[1] - xg[0]) / 360.0 * dLatCircum;
		double dy = fabs(yg[1] - yg[0]) / 360.0 * kdEarthCircumPolar;

		m_dElevScale = average(dx, dy);
	}

	m_dElevBase = m_dLogSpan[0];

	// Convert from ground units to elev units.
	const measurement_unit* puG = this->get_uom(pszGroundUnits);
	const measurement_unit* puE = this->get_uom(m_szElevUnits);

	if(puG == NULL || puE == NULL)
		return false;

	const double g_to_e = puG->dScale / puE->dScale;

	m_dElevScale *= g_to_e;
	return true;
}


bool LevellerDataset::write_header()
{
	char szHeader[5];
	strcpy(szHeader, "trrn");
	szHeader[4] = 7; // TER v7 introduced w/ Lev 2.6.

	if(1 != VSIFWriteL(szHeader, 5, 1, m_fp)
		|| !this->write_tag("hf_w", (size_t)nRasterXSize)
		|| !this->write_tag("hf_b", (size_t)nRasterYSize))
	{
        CPLError( CE_Failure, CPLE_FileIO, "Could not write header" );
        return false;
	}

	m_dElevBase = 0.0;
	m_dElevScale = 1.0;

	if(m_pszProjection == NULL || m_pszProjection[0] == 0)
	{
		this->write_tag("csclass", LEV_COORDSYS_RASTER);
	}
	else
	{
		this->write_tag("coordsys_wkt", m_pszProjection);
		const UNITLABEL units_elev = this->id_to_code(m_szElevUnits);
		
		const int bHasECS = 
			(units_elev != UNITLABEL_PIXEL && units_elev != UNITLABEL_UNKNOWN);

		this->write_tag("coordsys_haselevm", bHasECS);

	    OGRSpatialReference sr(m_pszProjection);

		if(bHasECS)
		{
			if(!this->compute_elev_scaling(sr))
				return false;

			// Raw-to-real units scaling.
			this->write_tag("coordsys_em_scale", m_dElevScale);

			//elev offset, in real units.
			this->write_tag("coordsys_em_base", m_dElevBase);
				
			this->write_tag("coordsys_em_units", units_elev);
		}


		if(sr.IsLocal())
		{
			this->write_tag("csclass", LEV_COORDSYS_LOCAL);

			const double dfLinear = sr.GetLinearUnits();
			const int n = this->meter_measure_to_code(dfLinear);
			this->write_tag("coordsys_units", n);
		}
		else
		{
			this->write_tag("csclass", LEV_COORDSYS_GEO);
		}

		if( m_adfTransform[2] != 0.0 || m_adfTransform[4] != 0.0)
		{
			CPLError( CE_Failure, CPLE_IllegalArg, 
				"Cannot handle rotated geotransform" );
			return false;
		}

		// todo: GDAL gridpost spacing is based on extent / rastersize
		// instead of extent / (rastersize-1) like Leveller.
		// We need to look into this and adjust accordingly.

		// Write north-south digital axis.
		this->write_tag("coordsys_da0_style", LEV_DA_PIXEL_SIZED);
		this->write_tag("coordsys_da0_fixedend", 0);
		this->write_tag("coordsys_da0_v0", m_adfTransform[3]);
		this->write_tag("coordsys_da0_v1", m_adfTransform[5]);

		// Write east-west digital axis.
		this->write_tag("coordsys_da1_style", LEV_DA_PIXEL_SIZED);
		this->write_tag("coordsys_da1_fixedend", 0);
		this->write_tag("coordsys_da1_v0", m_adfTransform[0]);
		this->write_tag("coordsys_da1_v1", m_adfTransform[1]);
	}


	this->write_tag_start("hf_data", 
		sizeof(float) * nRasterXSize * nRasterYSize);

	return true;
}


/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr LevellerDataset::SetGeoTransform( double *padfGeoTransform )
{
	memcpy(m_adfTransform, padfGeoTransform, 
		sizeof(m_adfTransform));

	return CE_None;
}


/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr LevellerDataset::SetProjection( const char * pszNewProjection )
{
	if(m_pszProjection != NULL)
		CPLFree(m_pszProjection);

	m_pszProjection = CPLStrdup(pszNewProjection);

	return CE_None;
}


/************************************************************************/
/*                           Create()                                   */
/************************************************************************/
GDALDataset* LevellerDataset::Create
(
	const char* pszFilename,
    int nXSize, int nYSize, int nBands,
    GDALDataType eType, char** papszOptions 
)
{
	if(nBands != 1)
	{
		CPLError( CE_Failure, CPLE_IllegalArg, "Band count must be 1" );
		return NULL;
	}

	if(eType != GDT_Float32)
	{
		CPLError( CE_Failure, CPLE_IllegalArg, "Pixel type must be Float32" );
		return NULL;
	}

	if(nXSize < 2 || nYSize < 2)
	{
		CPLError( CE_Failure, CPLE_IllegalArg, "One or more raster dimensions too small" );
		return NULL;
	}


	LevellerDataset* poDS = new LevellerDataset;

    poDS->eAccess = GA_Update;
	
	poDS->m_pszFilename = CPLStrdup(pszFilename);

    poDS->m_fp = VSIFOpenL( pszFilename, "wb+" );

    if( poDS->m_fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file `%s' failed.",
                  pszFilename );
		delete poDS;
        return NULL;
    }

	// Header will be written the first time IWriteBlock
	// is called.

    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;


    const char* pszValue = CSLFetchNameValue( 
		papszOptions,"MINUSERPIXELVALUE");
    if( pszValue != NULL )
        poDS->m_dLogSpan[0] = atof( pszValue );
	else
	{
		delete poDS;
		CPLError( CE_Failure, CPLE_IllegalArg, 
			"MINUSERPIXELVALUE must be specified." );
		return NULL;
	}

    pszValue = CSLFetchNameValue( 
		papszOptions,"MAXUSERPIXELVALUE");
    if( pszValue != NULL )
        poDS->m_dLogSpan[1] = atof( pszValue );

	if(poDS->m_dLogSpan[1] < poDS->m_dLogSpan[0])
	{
		double t = poDS->m_dLogSpan[0];
		poDS->m_dLogSpan[0] = poDS->m_dLogSpan[1];
		poDS->m_dLogSpan[1] = t;
	}

// -------------------------------------------------------------------- 
//      Instance a band.                                                
// -------------------------------------------------------------------- 
    poDS->SetBand( 1, new LevellerRasterBand( poDS ) );

	return poDS;
}								


bool LevellerDataset::write_byte(size_t n)
{
	unsigned char uch = (unsigned char)n;
	return (1 == VSIFWriteL(&uch, 1, 1, m_fp));
}


bool LevellerDataset::write(int n)
{
	CPL_LSBPTR32(&n);
	return (1 == VSIFWriteL(&n, sizeof(n), 1, m_fp));
}


bool LevellerDataset::write(size_t n)
{
	CPL_LSBPTR32(&n);
	return (1 == VSIFWriteL(&n, sizeof(n), 1, m_fp));
}


bool LevellerDataset::write(double d)
{
	CPL_LSBPTR64(&d);
	return (1 == VSIFWriteL(&d, sizeof(d), 1, m_fp));
}



bool LevellerDataset::write_tag_start(const char* pszTag, size_t n)
{
	if(this->write_byte(strlen(pszTag)))
	{
		return (1 == VSIFWriteL(pszTag, strlen(pszTag), 1, m_fp)
			&& this->write(n));
	}
	
	return false;
}


bool LevellerDataset::write_tag(const char* pszTag, int n)
{
	return (this->write_tag_start(pszTag, sizeof(n))
			&& this->write(n));
}


bool LevellerDataset::write_tag(const char* pszTag, size_t n)
{
	return (this->write_tag_start(pszTag, sizeof(n))
			&& this->write(n));
}


bool LevellerDataset::write_tag(const char* pszTag, double d)
{
	return (this->write_tag_start(pszTag, sizeof(d))
			&& this->write(d));
}


bool LevellerDataset::write_tag(const char* pszTag, const char* psz)
{
	CPLAssert(strlen(pszTag) <= kMaxTagNameLen);

	char sz[kMaxTagNameLen + 1];
	sprintf(sz, "%sl", pszTag);
	const size_t len = strlen(psz);

	if(len > 0 && this->write_tag(sz, len))
	{
		sprintf(sz, "%sd", pszTag);
		this->write_tag_start(sz, len);
		return (1 == VSIFWriteL(psz, len, 1, m_fp));
	}
	return false;
}


bool LevellerDataset::locate_data(vsi_l_offset& offset, size_t& len, VSILFILE* fp, const char* pszTag)
{
    // Locate the file offset of the desired tag's data.
    // If it is not available, return false.
    // If the tag is found, leave the filemark at the 
    // start of its data.

    if(0 != VSIFSeekL(fp, 5, SEEK_SET))
        return false;

    const int kMaxDescLen = 64;
    for(;;)
    {
        unsigned char c;
        if(1 != VSIFReadL(&c, sizeof(c), 1, fp))
            return false;

        const size_t descriptorLen = c;
        if(descriptorLen == 0 || descriptorLen > (size_t)kMaxDescLen)
            return false;

        char descriptor[kMaxDescLen+1];
        if(1 != VSIFReadL(descriptor, descriptorLen, 1, fp))
            return false;

        GUInt32 datalen;
        if(1 != VSIFReadL(&datalen, sizeof(datalen), 1, fp))
            return false;

        datalen = CPL_LSBWORD32(datalen);
        descriptor[descriptorLen] = 0;
        if(str_equal(descriptor, pszTag))
        {
            len = (size_t)datalen;
            offset = VSIFTellL(fp);
            return true;
        }
        else
        {
            // Seek to next tag.
            if(0 != VSIFSeekL(fp, (vsi_l_offset)datalen, SEEK_CUR))
                return false;
        }
    }
}

/************************************************************************/
/*                                get()                                 */
/************************************************************************/

bool LevellerDataset::get(int& n, VSILFILE* fp, const char* psz)
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
            return true;
        }
    }	
    return false;
}

/************************************************************************/
/*                                get()                                 */
/************************************************************************/

bool LevellerDataset::get(double& d, VSILFILE* fp, const char* pszTag)
{
    vsi_l_offset offset;
    size_t		 len;

    if(this->locate_data(offset, len, fp, pszTag))
    {
        if(1 == VSIFReadL(&d, sizeof(d), 1, fp))
        {
            CPL_LSBPTR64(&d);
            return true;
        }
    }	
    return false;
}


/************************************************************************/
/*                                get()                                 */
/************************************************************************/
bool LevellerDataset::get(char* pszValue, size_t maxchars, VSILFILE* fp, const char* pszTag)
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
            return false;

        if(1 == VSIFReadL(pszValue, len, 1, fp))
        {
            pszValue[len] = 0; // terminate C-string
            return true;
        }
    }	

    return false;
}



UNITLABEL LevellerDataset::meter_measure_to_code(double dM) const
{
	// Convert a meter conversion factor to its UOM OEM code.
	// If the factor is close to the approximation margin, then
	// require exact equality, otherwise be loose.

	const measurement_unit* pu = this->get_uom(dM);
	return (pu != NULL ? pu->oemCode : UNITLABEL_UNKNOWN);
}


UNITLABEL LevellerDataset::id_to_code(const char* pszUnits) const
{
	// Convert a readable UOM to its OEM code. 

	const measurement_unit* pu = this->get_uom(pszUnits);
	return (pu != NULL ? pu->oemCode : UNITLABEL_UNKNOWN);
}


const char* LevellerDataset::code_to_id(UNITLABEL code) const
{
	// Convert a measurement unit's OEM ID to its readable ID. 

	const measurement_unit* pu = this->get_uom(code);
	return (pu != NULL ? pu->pszID : NULL);
}


const measurement_unit* LevellerDataset::get_uom(const char* pszUnits) const
{
    for(size_t i = 0; i < array_size(kUnits); i++)
    {
        if(strcmp(pszUnits, kUnits[i].pszID) == 0)
            return &kUnits[i];
    }
    CPLError( CE_Failure, CPLE_AppDefined, 
              "Unknown measurement units: %s", pszUnits );
    return NULL;
}


const measurement_unit* LevellerDataset::get_uom(UNITLABEL code) const
{
    for(size_t i = 0; i < array_size(kUnits); i++)
    {
        if(kUnits[i].oemCode == code)
            return &kUnits[i];
    }
    CPLError( CE_Failure, CPLE_AppDefined, 
              "Unknown measurement unit code: %08x", code );
    return NULL;
}


const measurement_unit* LevellerDataset::get_uom(double dM) const
{
    for(size_t i = kFirstLinearMeasureIdx; i < array_size(kUnits); i++)
    {
		if(dM >= 1.0e-4)
		{
			if(approx_equal(dM, kUnits[i].dScale))
				return &kUnits[i];
		}
		else if(dM == kUnits[i].dScale)
			return &kUnits[i];
    }
    CPLError( CE_Failure, CPLE_AppDefined, 
              "Unknown measurement conversion factor: %f", dM );
    return NULL;
}

/************************************************************************/
/*                          convert_measure()                           */
/************************************************************************/


bool LevellerDataset::convert_measure
(
	double d, 
	double& dResult,
	const char* pszSpace 
)
{
    // Convert a measure to meters.

    for(size_t i = kFirstLinearMeasureIdx; i < array_size(kUnits); i++)
    {
        if(str_equal(pszSpace, kUnits[i].pszID))
		{
            dResult = d * kUnits[i].dScale;
			return true;
		}
    }
    CPLError( CE_Failure, CPLE_FileIO, 
              "Unknown linear measurement unit: '%s'", pszSpace );
    return false;
}


bool LevellerDataset::make_local_coordsys(const char* pszName, const char* pszUnits)
{
	OGRSpatialReference sr;

	sr.SetLocalCS(pszName);
	double d;
	return ( this->convert_measure(1.0, d, pszUnits)
		&& OGRERR_NONE == sr.SetLinearUnits(pszUnits, d) 
		&& OGRERR_NONE == sr.exportToWkt(&m_pszProjection) );
}


bool LevellerDataset::make_local_coordsys(const char* pszName, UNITLABEL code)
{
	const char* pszUnitID = this->code_to_id(code);
	return ( pszUnitID != NULL
		&& this->make_local_coordsys(pszName, pszUnitID));
}



/************************************************************************/
/*                            load_from_file()                            */
/************************************************************************/

bool LevellerDataset::load_from_file(VSILFILE* file, const char* pszFilename)
{
    // get hf dimensions
    if(!this->get(nRasterXSize, file, "hf_w"))
	{
		CPLError( CE_Failure, CPLE_OpenFailed,
					  "Cannot determine heightfield width." );
        return false;
	}

    if(!this->get(nRasterYSize, file, "hf_b"))
	{
		CPLError( CE_Failure, CPLE_OpenFailed,
					  "Cannot determine heightfield breadth." );
        return false;
	}

	if(nRasterXSize < 2 || nRasterYSize < 2)
	{
		CPLError( CE_Failure, CPLE_OpenFailed,
					  "Heightfield raster dimensions too small." );
        return false;
	}

    // Record start of pixel data
    size_t datalen;
    if(!this->locate_data(m_nDataOffset, datalen, file, "hf_data"))
	{
		CPLError( CE_Failure, CPLE_OpenFailed,
					  "Cannot locate elevation data." );
        return false;
	}

    // Sanity check: do we have enough pixels?
    if(datalen != nRasterXSize * nRasterYSize * sizeof(float))
	{
		CPLError( CE_Failure, CPLE_OpenFailed,
					  "File does not have enough data." );
        return false;
	}


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
				UNITLABEL unitcode;
				//char szLocalUnits[8];
				if(!this->get((int&)unitcode, file, "coordsys_units"))
					unitcode = UNITLABEL_M;

				if(!this->make_local_coordsys("Leveller", unitcode))
				{
					CPLError( CE_Failure, CPLE_OpenFailed,
								  "Cannot define local coordinate system." );
					return false;
				}
			}
			else if(csclass == LEV_COORDSYS_GEO)
			{
				char szWKT[1024];
				if(!this->get(szWKT, 1023, file, "coordsys_wkt"))
					return 0;

				m_pszProjection = (char*)CPLMalloc(strlen(szWKT) + 1);
				strcpy(m_pszProjection, szWKT);
			}
			else
			{
				CPLError( CE_Failure, CPLE_OpenFailed,
						  "Unknown coordinate system type in %s.",
						  pszFilename );
				return false;
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
			UNITLABEL unitcode;
			if(this->get((int&)unitcode, file, "coordsys_em_units"))
			{
				const char* pszUnitID = this->code_to_id(unitcode);
				if(pszUnitID != NULL)
					strcpy(m_szElevUnits, pszUnitID);
				else
				{
					CPLError( CE_Failure, CPLE_OpenFailed,
								  "Unknown OEM elevation unit of measure (%d)",
									unitcode );
					return false;
				}
			}
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
		{
			CPLError( CE_Failure, CPLE_OpenFailed,
						  "Cannot define local coordinate system." );
			return false;
		}
	}

    return true;
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
                  "Failed to re-open %s within Leveller driver.",
                  poOpenInfo->pszFilename );
        return NULL;
    }
    poDS->eAccess = poOpenInfo->eAccess;

    
/* -------------------------------------------------------------------- */
/*	Read the file.							                            */
/* -------------------------------------------------------------------- */
    if( !poDS->load_from_file( poDS->m_fp, poOpenInfo->pszFilename ) )
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

/* -------------------------------------------------------------------- */
/*      Check for external overviews.                                   */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename, poOpenInfo->GetSiblingFiles() );

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
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, 
                                   "ter" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Leveller heightfield" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_leveller.html" );
        
#if GDAL_VERSION_NUM >= 1500
        poDriver->pfnIdentify = LevellerDataset::Identify;
#endif
        poDriver->pfnOpen = LevellerDataset::Open;
        poDriver->pfnCreate = LevellerDataset::Create;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
