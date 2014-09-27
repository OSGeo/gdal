/******************************************************************************
 * terragendataset.cpp,v 1.2
 *
 * Project:  Terragen(tm) TER Driver
 * Purpose:  Reader for Terragen TER documents
 * Author:   Ray Gardener, Daylon Graphics Ltd.
 *
 * Portions of this module derived from GDAL drivers by 
 * Frank Warmerdam, see http://www.gdal.org

 rcg	apr 19/06	Fixed bug with hf size being misread by one
					if xpts/ypts tags not included in file.
					Added Create() support.
					Treat pixels as points.

 rcg	jun  6/07	Better heightscale/baseheight determination
                    when writing.

 *
 ******************************************************************************
 * Copyright (c) 2006-2007 Daylon Graphics Ltd.
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
 */

/*
	Terragen format notes:

	Based on official Planetside specs.

	All distances along all three axes are in
	terrain units, which are 30m by default.
	If a SCAL chunk is present, however, it
	can indicate something other than 30.
	Note that uniform scaling should be used.

	The offset (base height) in the ALTW chunk 
	is in terrain units, and the scale (height scale)
	is a normalized value using unsigned 16-bit notation.
	The physical terrain value for a read pixel is
		hv' = hv * scale / 65536 + offset.
		It still needs to be scaled by SCAL to 
		get to meters.

	For writing:
			SCAL = gridpost distance in meters
			hv_px = hv_m / SCAL
			span_px = span_m / SCAL
			offset = see TerragenDataset::write_header()
			scale = see TerragenDataset::write_header()
			physical hv =
				(hv_px - offset) * 65536.0/scale


	We tell callers that:

		Elevations are Int16 when reading, 
		and Float32 when writing. We need logical 
		elevations when writing so that we can 
		encode them with as much precision as possible 
		when going down to physical 16-bit ints.
		Implementing band::SetScale/SetOffset won't work because 
		it requires callers to know format write details.
		So we've added two Create() options that let the 
		caller tell us the span's logical extent, and with 
		those two values we can convert to physical pixels.

		band::GetUnitType() returns meters.
		band::GetScale() returns SCAL * (scale/65536)
		band::GetOffset() returns SCAL * offset 
		ds::GetProjectionRef() returns a local CS
			using meters.
		ds::GetGeoTransform() returns a scale matrix
			having SCAL sx,sy members.

		ds::SetGeoTransform() lets us establish the
			size of ground pixels.
		ds::SetProjection() lets us establish what
			units ground measures are in (also needed 
			to calc the size of ground pixels).
		band::SetUnitType() tells us what units
			the given Float32 elevations are in.
		band::SetScale() is unused.
		band::SetOffset() is unused.
*/

#include "cpl_string.h"
#include "gdal_pam.h"
#include "ogr_spatialref.h"

// CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_Terragen(void);
CPL_C_END


const double kdEarthCircumPolar = 40007849;
const double kdEarthCircumEquat = 40075004;

#define str_equal(_s1, _s2)	(0 == strcmp((_s1),(_s2)))
#define array_size(_a)		(sizeof(_a) / sizeof(_a[0]))

#ifndef min
	#define min(a, b)	( (a) < (b) ? (a) : (b) )
#endif

static double average(double a, double b)
{
	return 0.5 * (a + b);
}


static double degrees_to_radians(double d) 
{
	return (d * 0.017453292);
}

static bool approx_equal(double a, double b)
{
	const double epsilon = 1e-5;
	return (fabs(a-b) <= epsilon);
}



/************************************************************************/
/* ==================================================================== */
/*				TerragenDataset				*/
/* ==================================================================== */
/************************************************************************/

class TerragenRasterBand;

class TerragenDataset : public GDALPamDataset
{
    friend class TerragenRasterBand;


	double		m_dScale,
				m_dOffset,
				m_dSCAL, // 30.0 normally, from SCAL chunk
	   			m_adfTransform[6],
				m_dGroundScale,
				m_dMetersPerGroundUnit,
				m_dMetersPerElevUnit,
				m_dLogSpan[2], 
				m_span_m[2], 
				m_span_px[2];

    VSILFILE*			m_fp;
    vsi_l_offset	m_nDataOffset;

    GInt16		m_nHeightScale;
    GInt16		m_nBaseHeight;

	char*		m_pszFilename;
    char*		m_pszProjection;
    char		m_szUnits[32];

	bool		m_bIsGeo;


    int         LoadFromFile();


	public:
		TerragenDataset();
		~TerragenDataset();
    
		static GDALDataset* Open( GDALOpenInfo* );
		static GDALDataset* Create( const char* pszFilename,
									int nXSize, int nYSize, int nBands,
									GDALDataType eType, char** papszOptions );

		virtual CPLErr 	GetGeoTransform( double* );
		virtual const char*	GetProjectionRef(void);
		virtual CPLErr SetProjection( const char * );
		virtual CPLErr SetGeoTransform( double * );

	protected:
		bool get(GInt16&);
		bool get(GUInt16&);
		bool get(float&);
		bool put(GInt16);
		bool put(float);
		bool skip(size_t n) { return ( 0 == VSIFSeekL(m_fp, n, SEEK_CUR) ); }
		bool pad(size_t n) { return this->skip(n); }

		bool read_next_tag(char*);
		bool write_next_tag(const char*);
		bool tag_is(const char* szTag, const char*);

		bool write_header(void);
};

/************************************************************************/
/* ==================================================================== */
/*                            TerragenRasterBand                        */
/* ==================================================================== */
/************************************************************************/

class TerragenRasterBand : public GDALPamRasterBand
{
    friend class TerragenDataset;

	void*	        m_pvLine;
	bool            m_bFirstTime;

public:

    TerragenRasterBand(TerragenDataset*);
	virtual ~TerragenRasterBand()
	{
		if(m_pvLine != NULL)
			CPLFree(m_pvLine);
	}
    
    // Geomeasure support.
    virtual CPLErr IReadBlock( int, int, void * );
    virtual const char* GetUnitType();
    virtual double GetOffset(int* pbSuccess = NULL);
    virtual double GetScale(int* pbSuccess = NULL);

    virtual CPLErr IWriteBlock( int, int, void * );
	virtual CPLErr SetUnitType( const char* );
};


/************************************************************************/
/*                         TerragenRasterBand()                         */
/************************************************************************/

TerragenRasterBand::TerragenRasterBand( TerragenDataset *poDS )
{
	m_bFirstTime = true;
    this->poDS = poDS;
    this->nBand = 1;

    eDataType = poDS->GetAccess() == GA_ReadOnly 
		? GDT_Int16
		: GDT_Float32;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

	m_pvLine = CPLMalloc(sizeof(GInt16) * nBlockXSize);
}


/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr TerragenRasterBand::IReadBlock( CPL_UNUSED int nBlockXOff,
                                       int nBlockYOff,
                                       void* pImage )
{
    //CPLAssert( sizeof(float) == sizeof(GInt32) );
    CPLAssert( nBlockXOff == 0  );
    CPLAssert( pImage != NULL );

    TerragenDataset& ds = *(TerragenDataset *) poDS;

/* -------------------------------------------------------------------- */
/*      Seek to scanline.                                               
	Terragen is a bottom-top format, so we have to
	invert the row location.
 -------------------------------------------------------------------- */
    const size_t rowbytes = nBlockXSize * sizeof(GInt16);

    if(0 != VSIFSeekL(
           ds.m_fp, 
           ds.m_nDataOffset + 
			  (ds.GetRasterYSize() -1 - nBlockYOff) * rowbytes, 
           SEEK_SET))
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Terragen Seek failed:%s", VSIStrerror( errno ) );
        return CE_Failure;
    }


/* -------------------------------------------------------------------- */
/*      Read the scanline into the line buffer.                        */
/* -------------------------------------------------------------------- */

    if( VSIFReadL( pImage, rowbytes, 1, ds.m_fp ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Terragen read failed:%s", VSIStrerror( errno ) );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Swap on MSB platforms.                                          */
/* -------------------------------------------------------------------- */
#ifdef CPL_MSB 
    GDALSwapWords( pImage, sizeof(GInt16), nRasterXSize, sizeof(GInt16) );
#endif    

    return CE_None;
}



/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/
const char *TerragenRasterBand::GetUnitType()
{
	// todo: Return elevation units.
	// For Terragen documents, it's the same as the ground units.
    TerragenDataset *poGDS = (TerragenDataset *) poDS;

    return poGDS->m_szUnits;
}


/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double TerragenRasterBand::GetScale(int* pbSuccess)
{
    const TerragenDataset& ds = *(TerragenDataset *) poDS;
	if(pbSuccess != NULL)
		*pbSuccess = TRUE;
	return ds.m_dScale;
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

double TerragenRasterBand::GetOffset(int* pbSuccess)
{
    const TerragenDataset& ds = *(TerragenDataset *) poDS;
	if(pbSuccess != NULL)
		*pbSuccess = TRUE;
	return ds.m_dOffset;
}



/************************************************************************/
/*                             IWriteBlock()                            */
/************************************************************************/

CPLErr TerragenRasterBand::IWriteBlock
(
    CPL_UNUSED int nBlockXOff,
    int nBlockYOff,
    void* pImage
)
{
    CPLAssert( nBlockXOff == 0  );
    CPLAssert( pImage != NULL );
	CPLAssert( m_pvLine != NULL );

	#define sgn(_n) ((_n) < 0 ? -1 : ((_n) > 0 ? 1 : 0) )
	#define sround(_f)	\
		(int)((_f) + (0.5 * sgn(_f)))

	const size_t pixelsize = sizeof(GInt16);


	TerragenDataset& ds = *(TerragenDataset*)poDS;
	if(m_bFirstTime)
	{
		m_bFirstTime = false;
		ds.write_header();
		ds.m_nDataOffset = VSIFTellL(ds.m_fp);
	}
	const size_t rowbytes = nBlockXSize * pixelsize;

	GInt16* pLine = (GInt16*)m_pvLine;


	if(0 == VSIFSeekL(
       ds.m_fp, 
       ds.m_nDataOffset + 
		// Terragen is Y inverted.
		(ds.GetRasterYSize()-1-nBlockYOff) * rowbytes, 
       SEEK_SET))
	{
		// Convert each float32 to int16.
		float* pfImage = (float*)pImage;
		for(size_t x = 0; x < (size_t)nBlockXSize; x++)
		{
			double f = pfImage[x];
			f *= ds.m_dMetersPerElevUnit;
			f /= ds.m_dSCAL;
			GInt16 hv = 
				(GInt16)((f - ds.m_nBaseHeight) *
					65536.0 / ds.m_nHeightScale /*+ ds.m_nShift*/);
			pLine[x] = hv;
		}

#ifdef CPL_MSB 
		GDALSwapWords( m_pvLine, pixelsize, nBlockXSize, pixelsize );
#endif    
		if(1 == VSIFWriteL(m_pvLine, rowbytes, 1, ds.m_fp))
			return CE_None;
	}

	return CE_Failure;
}


CPLErr TerragenRasterBand::SetUnitType( const char* psz )
{
	TerragenDataset& ds = *(TerragenDataset*)poDS;

	if(EQUAL(psz, "m"))
		ds.m_dMetersPerElevUnit = 1.0;
	else if(EQUAL(psz, "ft"))
		ds.m_dMetersPerElevUnit = 0.3048;
	else if(EQUAL(psz, "sft"))
		ds.m_dMetersPerElevUnit = 1200.0 / 3937.0;
	else
		return CE_Failure;

	return CE_None;
}



/************************************************************************/
/* ==================================================================== */
/*				TerragenDataset		                                    */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          TerragenDataset()                           */
/************************************************************************/

TerragenDataset::TerragenDataset()
{
	m_pszFilename = NULL;
    m_fp = NULL;
	m_bIsGeo = false;
	m_pszProjection = NULL;
	m_nHeightScale = 0; // fix for ticket 2119
	m_nBaseHeight = 0;
	m_dMetersPerGroundUnit = 1.0;
	m_dSCAL = 30.0;
	m_dLogSpan[0] = m_dLogSpan[1] = 0.0;

    m_adfTransform[0] = 0.0;
    m_adfTransform[1] = m_dSCAL;
    m_adfTransform[2] = 0.0;
    m_adfTransform[3] = 0.0;
    m_adfTransform[4] = 0.0;
    m_adfTransform[5] = m_dSCAL;
}

/************************************************************************/
/*                          ~TerragenDataset()                          */
/************************************************************************/

TerragenDataset::~TerragenDataset()

{
    FlushCache();

	CPLFree(m_pszProjection);
	CPLFree(m_pszFilename);

    if( m_fp != NULL )
        VSIFCloseL( m_fp );
}



bool TerragenDataset::write_header()
{
	char szHeader[16];
	memcpy(szHeader, "TERRAGENTERRAIN ", sizeof(szHeader));

    if(1 != VSIFWriteL( (void *) szHeader, sizeof(szHeader), 1, m_fp ))
	{
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Couldn't write to Terragen file %s.\n"
                  "Is file system full?",
                  m_pszFilename );
        VSIFCloseL( m_fp );

        return false;
    }

    
// -------------------------------------------------------------------- 
//      Write out the heightfield dimensions, etc.
// -------------------------------------------------------------------- 

	const int nXSize = this->GetRasterXSize();
	const int nYSize = this->GetRasterYSize();

	this->write_next_tag("SIZE");
	this->put((GInt16)(min(nXSize, nYSize)-1));
	this->pad(sizeof(GInt16));

	if(nXSize != nYSize)
	{
		this->write_next_tag("XPTS");
		this->put((GInt16)nXSize); this->pad(sizeof(GInt16));
		this->write_next_tag("YPTS");
		this->put((GInt16)nYSize); this->pad(sizeof(GInt16));
	}

	if(m_bIsGeo)
	{
		/*
			With a geographic projection (degrees),
			m_dGroundScale will be in degrees and
			m_dMetersPerGroundUnit is undefined.
			So we're going to estimate a m_dMetersPerGroundUnit
			value here (i.e., meters per degree).

			We figure out the degree size of one 
			pixel, and then the latitude degrees 
			of the heightfield's center. The circumference of
			the latitude's great circle lets us know how
			wide the pixel is in meters, and we 
			average that with the pixel's meter breadth,
			which is based on the polar circumference.
		*/

		/*const double m_dDegLongPerPixel = 
			fabs(m_adfTransform[1]);*/

		const double m_dDegLatPerPixel = 
			fabs(m_adfTransform[5]);

		/*const double m_dCenterLongitude =
			m_adfTransform[0] + 
			(0.5 * m_dDegLongPerPixel * (nXSize-1));*/

		const double m_dCenterLatitude =
			m_adfTransform[3] + 
			(0.5 * m_dDegLatPerPixel * (nYSize-1));


		const double dLatCircum = kdEarthCircumEquat 
			* sin(degrees_to_radians(90.0 - m_dCenterLatitude));

		const double dMetersPerDegLongitude = dLatCircum / 360;
		/*const double dMetersPerPixelX = 
			(m_dDegLongPerPixel / 360) * dLatCircum;*/

		const double dMetersPerDegLatitude = 
			kdEarthCircumPolar / 360;
		/*const double dMetersPerPixelY = 
			(m_dDegLatPerPixel / 360) * kdEarthCircumPolar;*/

		m_dMetersPerGroundUnit = 
			average(dMetersPerDegLongitude, dMetersPerDegLatitude);

	}
		
	m_dSCAL = m_dGroundScale * m_dMetersPerGroundUnit;

	if(m_dSCAL != 30.0)
	{
		const float sc = (float)m_dSCAL;
		this->write_next_tag("SCAL");
		this->put(sc);
		this->put(sc);
		this->put(sc);
	}

	if(!this->write_next_tag("ALTW"))
	{
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Couldn't write to Terragen file %s.\n"
                  "Is file system full?",
                  m_pszFilename );
        VSIFCloseL( m_fp );

        return false;
	}

	// Compute physical scales and offsets.
	m_span_m[0] = m_dLogSpan[0] * m_dMetersPerElevUnit;
	m_span_m[1] = m_dLogSpan[1] * m_dMetersPerElevUnit;

	m_span_px[0] = m_span_m[0] / m_dSCAL;
	m_span_px[1] = m_span_m[1] / m_dSCAL;

	const double span_px = m_span_px[1] - m_span_px[0];
	m_nHeightScale = (GInt16)span_px;
	if(m_nHeightScale == 0)
		m_nHeightScale++;

	#define P2L_PX(n, hs, bh)	\
		((double)(n) / 65536.0 * (hs) + (bh))

	#define L2P_PX(n, hs, bh)	\
		((int)(((n)-(bh)) * 65536.0 / (hs)))

	// Increase the heightscale until the physical span
	// fits within a 16-bit range. The smaller the logical span,
	// the more necessary this becomes.
	int hs, bh=0;
	for(hs = m_nHeightScale; hs <= 32767; hs++)
	{
		double prevdelta = 1.0e30;
		for(bh = -32768; bh <= 32767; bh++)
		{
			int nValley = L2P_PX(m_span_px[0], hs, bh);
			if(nValley < -32768) continue;
			int nPeak = L2P_PX(m_span_px[1], hs, bh);
			if(nPeak > 32767) continue;

			// now see how closely the baseheight gets
			// to the pixel span.
			double d = P2L_PX(nValley, hs, bh);
			double delta = fabs(d - m_span_px[0]);
			if(delta < prevdelta) // Converging?
				prevdelta = delta;
			else 
			{
				// We're diverging, so use the previous bh
				// and stop looking.
				bh--;
				break;
			}
		}
		if(bh != 32768) break;
	}
	if(hs == 32768)
	{
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Couldn't write to Terragen file %s.\n"
                  "Cannot find adequate heightscale/baseheight combination.",
                  m_pszFilename );
        VSIFCloseL( m_fp );

        return false;
	}
		
	m_nHeightScale = (GInt16) hs;
	m_nBaseHeight = (GInt16) bh;


	// m_nHeightScale is the one that gives us the 
	// widest use of the 16-bit space. However, there
	// might be larger heightscales that, even though
	// the reduce the space usage, give us a better fit
	// for preserving the span extents.


	return (this->put(m_nHeightScale) &&
			this->put(m_nBaseHeight));
}



/************************************************************************/
/*                                get()                                 */
/************************************************************************/

bool TerragenDataset::get(GInt16& value)
{
    if(1 == VSIFReadL(&value, sizeof(value), 1, m_fp))
    {
        CPL_LSBPTR16(&value);
        return true;
    }
    return false;
}


bool TerragenDataset::get(GUInt16& value)
{
    if(1 == VSIFReadL(&value, sizeof(value), 1, m_fp))
    {
        CPL_LSBPTR16(&value);
        return true;
    }
    return false;
}


bool TerragenDataset::get(float& value)
{
    if(1 == VSIFReadL(&value, sizeof(value), 1, m_fp))
    {
        CPL_LSBPTR32(&value);
        return true;
    }
    return false;
}


/************************************************************************/
/*                                put()                                 */
/************************************************************************/

bool TerragenDataset::put(GInt16 n)
{
    CPL_LSBPTR16(&n);
    return (1 == VSIFWriteL(&n, sizeof(n), 1, m_fp));
}


bool TerragenDataset::put(float f)
{
    CPL_LSBPTR32(&f);
    return( 1 == VSIFWriteL(&f, sizeof(f), 1, m_fp) );
}

/************************************************************************/
/*                              tag stuff                               */
/************************************************************************/


bool TerragenDataset::read_next_tag(char* szTag)
{
	return (1 == VSIFReadL(szTag, 4, 1, m_fp));
}


bool TerragenDataset::write_next_tag(const char* szTag)
{
	return (1 == VSIFWriteL((void*)szTag, 4, 1, m_fp));
}


bool TerragenDataset::tag_is(const char* szTag, const char* sz)
{
	return (0 == memcmp(szTag, sz, 4));
}



/************************************************************************/
/*                            LoadFromFile()                            */
/************************************************************************/

int TerragenDataset::LoadFromFile()
{
    GUInt16	nSize, xpts=0, ypts=0;
    m_dSCAL = 30.0;
    m_nDataOffset = 0;

    if(0 != VSIFSeekL(m_fp, 16, SEEK_SET))
        return 0;

    char szTag[4];
    if(!this->read_next_tag(szTag) || !tag_is(szTag, "SIZE"))
        return 0;

    if(!this->get(nSize) || !this->skip(2))
        return 0;

    // Set dimensions to SIZE chunk. If we don't 
    // encounter XPTS/YPTS chunks, we can assume
    // the terrain to be square.
    xpts = ypts = nSize+1;

    while(this->read_next_tag(szTag))
    {
        if(this->tag_is(szTag, "XPTS"))
        {
            this->get(xpts);
            if(xpts < nSize || !this->skip(2))
                return 0;
            continue;
        }

        if(this->tag_is(szTag, "YPTS"))
        {
            this->get(ypts);
            if(ypts < nSize || !this->skip(2))
                return 0;
            continue;
        }

        if(this->tag_is(szTag, "SCAL"))
        {
            float sc[3];
            this->get(sc[0]);
            this->get(sc[1]);
            this->get(sc[2]);
            m_dSCAL = sc[1];
            continue;
        }

        if(this->tag_is(szTag, "CRAD"))
        {
            if(!this->skip(sizeof(float)))
                return 0;
            continue;
        }
        if(this->tag_is(szTag, "CRVM"))
        {
            if(!this->skip(sizeof(GUInt32)))
                return 0;
            continue;
        }
        if(this->tag_is(szTag, "ALTW"))
        {
            this->get(m_nHeightScale);
            this->get(m_nBaseHeight);
            m_nDataOffset = VSIFTellL(m_fp);
            if(!this->skip(xpts * ypts * sizeof(GInt16)))
                return 0;
            continue;
        }
        if(this->tag_is(szTag, "EOF "))
        {
            break;
        }
    }


    if(xpts == 0 || ypts == 0 || m_nDataOffset == 0)
        return 0;

    nRasterXSize = xpts;
    nRasterYSize = ypts;

    // todo: sanity check: do we have enough pixels?

    // Cache realworld scaling and offset.
    m_dScale = m_dSCAL / 65536 * m_nHeightScale;
    m_dOffset = m_dSCAL * m_nBaseHeight;
    strcpy(m_szUnits, "m");

    // Make our projection to have origin at the
    // NW corner, and groundscale to match elev scale
    // (i.e., uniform voxels).
    m_adfTransform[0] = 0.0;
    m_adfTransform[1] = m_dSCAL;
    m_adfTransform[2] = 0.0;
    m_adfTransform[3] = 0.0;
    m_adfTransform[4] = 0.0;
    m_adfTransform[5] = m_dSCAL;


/* -------------------------------------------------------------------- */
/*      Set projection.							*/
/* -------------------------------------------------------------------- */
    // Terragen files as of Apr 2006 are partially georeferenced,
    // we can declare a local coordsys that uses meters.
    OGRSpatialReference sr;

    sr.SetLocalCS("Terragen world space");
    if(OGRERR_NONE != sr.SetLinearUnits("m", 1.0))
        return 0;

    if(OGRERR_NONE != sr.exportToWkt(&m_pszProjection))
        return 0;

    return TRUE;
}




/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr TerragenDataset::SetProjection( const char * pszNewProjection )
{
    // Terragen files aren't really georeferenced, but 
    // we should get the projection's linear units so 
    // that we can scale elevations correctly.

    //m_dSCAL = 30.0; // default

    OGRSpatialReference oSRS( pszNewProjection );

/* -------------------------------------------------------------------- */
/*      Linear units.                                                   */
/* -------------------------------------------------------------------- */
    m_bIsGeo = (oSRS.IsGeographic() != FALSE);
    if(m_bIsGeo)
    {
        // The caller is using degrees. We need to convert 
        // to meters, otherwise we can't derive a SCAL
        // value to scale elevations with.
        m_bIsGeo = true;
    }
    else
    {
        double dfLinear = oSRS.GetLinearUnits();

        if( approx_equal(dfLinear, 0.3048))
            m_dMetersPerGroundUnit = 0.3048;
        else if( approx_equal(dfLinear, atof(SRS_UL_US_FOOT_CONV)) )
            m_dMetersPerGroundUnit = atof(SRS_UL_US_FOOT_CONV);
        else
            m_dMetersPerGroundUnit = 1.0;
    }

    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char*	TerragenDataset::GetProjectionRef(void)
{
    if(m_pszProjection == NULL )
        return "";
    else
        return m_pszProjection;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr TerragenDataset::SetGeoTransform( double *padfGeoTransform )
{
	memcpy(m_adfTransform, padfGeoTransform, 
		sizeof(m_adfTransform));

	// Average the projection scales.
	m_dGroundScale = 
		average(fabs(m_adfTransform[1]), fabs(m_adfTransform[5]));
	return CE_None;
}



/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr TerragenDataset::GetGeoTransform(double* padfTransform)
{
    memcpy(padfTransform, m_adfTransform, sizeof(m_adfTransform));
    return CE_None;
}


/************************************************************************/
/*                                Create()                                */
/************************************************************************/
GDALDataset* TerragenDataset::Create
(
	const char* pszFilename,
    int nXSize, int nYSize, int nBands,
    GDALDataType eType, char** papszOptions 
)
{
    TerragenDataset* poDS = new TerragenDataset();

    poDS->eAccess = GA_Update;
	
	poDS->m_pszFilename = CPLStrdup(pszFilename);

	// -------------------------------------------------------------------- 
	//      Verify input options.                                           
	// -------------------------------------------------------------------- 
    const char* pszValue = CSLFetchNameValue( 
		papszOptions,"MINUSERPIXELVALUE");
    if( pszValue != NULL )
        poDS->m_dLogSpan[0] = atof( pszValue );

    pszValue = CSLFetchNameValue( 
		papszOptions,"MAXUSERPIXELVALUE");
    if( pszValue != NULL )
        poDS->m_dLogSpan[1] = atof( pszValue );


	if( poDS->m_dLogSpan[1] <= poDS->m_dLogSpan[0] )
	{
        CPLError( CE_Failure, CPLE_AppDefined,
              "Inverted, flat, or unspecified span for Terragen file." );

		delete poDS;
        return NULL;
	}

    if( eType != GDT_Float32 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
              "Attempt to create Terragen dataset with a non-float32\n"
              "data type (%s).\n",
              GDALGetDataTypeName(eType) );

		delete poDS;
        return NULL;
    }


    if( nBands != 1 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Terragen driver doesn't support %d bands. Must be 1.\n",
                  nBands );

		delete poDS;
        return NULL;
    }


// -------------------------------------------------------------------- 
//      Try to create the file.                                         
// -------------------------------------------------------------------- 
    

    poDS->m_fp = VSIFOpenL( pszFilename, "wb+" );

    if( poDS->m_fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file `%s' failed.\n",
                  pszFilename );
		delete poDS;
        return NULL;
    }

    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;

    // Don't bother writing the header here; the first 
	// call to IWriteBlock will do that instead, since 
	// the elevation data's location depends on the
	// header size.


// -------------------------------------------------------------------- 
//      Instance a band.                                                
// -------------------------------------------------------------------- 
    poDS->SetBand( 1, new TerragenRasterBand( poDS ) );
	 

    //VSIFClose( poDS->m_fp );

    //return (GDALDataset *) GDALOpen( pszFilename, GA_Update );
    return (GDALDataset *) poDS;

}


/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *TerragenDataset::Open( GDALOpenInfo * poOpenInfo )

{
    // The file should have at least 32 header bytes
    if( poOpenInfo->nHeaderBytes < 32 )
        return NULL;

    if( !EQUALN((const char *) poOpenInfo->pabyHeader, 
		"TERRAGENTERRAIN ", 16) )
        return NULL;


/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    TerragenDataset 	*poDS;

    poDS = new TerragenDataset();

    // Reopen for large file access.
    if( poOpenInfo->eAccess == GA_Update )
        poDS->m_fp = VSIFOpenL( poOpenInfo->pszFilename, "rb+" );
    else
        poDS->m_fp = VSIFOpenL( poOpenInfo->pszFilename, "rb" );

    if( poDS->m_fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to re-open %s within Terragen driver.\n",
                  poOpenInfo->pszFilename );
        return NULL;
    }
    poDS->eAccess = poOpenInfo->eAccess;

    
/* -------------------------------------------------------------------- */
/*	Read the file.							*/
/* -------------------------------------------------------------------- */
    if( !poDS->LoadFromFile() )
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->SetBand( 1, new TerragenRasterBand( poDS ));

    poDS->SetMetadataItem( GDALMD_AREA_OR_POINT, GDALMD_AOP_POINT );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Support overviews.                                              */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/*                        GDALRegister_Terragen()                       */
/************************************************************************/

void GDALRegister_Terragen()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "Terragen" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "Terragen" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, 
                                   "ter" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Terragen heightfield" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_terragen.html" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='MINUSERPIXELVALUE' type='float' description='Lowest logical elevation'/>"
"   <Option name='MAXUSERPIXELVALUE' type='float' description='Highest logical elevation'/>"
"</CreationOptionList>" );
        
        poDriver->pfnOpen = TerragenDataset::Open;
        poDriver->pfnCreate = TerragenDataset::Create;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
