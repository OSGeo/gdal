/******************************************************************************
 *
 * Project:  ILWIS Driver
 * Purpose:  GDALDataset driver for ILWIS translator for read/write support.
 * Author:   Lichun Wang, lichun@itc.nl
 *
 ******************************************************************************
 * Copyright (c) 2004, ITC
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

#ifdef _MSC_VER
#pragma warning(disable : 4786)
#pragma warning(disable : 4503)
#endif

#include "gdal_pam.h"
#include "cpl_csv.h"
#include "ogr_spatialref.h"

#ifdef WIN32
#include  <io.h>
#endif

#include  <cstdio>
#include  <cstdlib>
#include <string>
#include <map>

CPL_C_START
void	GDALRegister_ILWIS(void);
CPL_C_END

#define shUNDEF	-32767
#define iUNDEF  -2147483647
#define flUNDEF ((float)-1e38)
#define	rUNDEF  ((double)-1e308)

enum ilwisStoreType
{	
    stByte,
    stInt,
    stLong,
    stFloat,
    stReal
};

class ValueRange
{
public:
    ValueRange(double min, double max);	// step = 1
    ValueRange(double min, double max, double step);	
    ValueRange(std::string str);
    std::string ToString();
    ilwisStoreType get_NeededStoreType() { return st; }
    double get_rLo() { return _rLo; }
    double get_rHi() { return _rHi; }
    double get_rStep() { return _rStep; }
    double get_rRaw0() { return _r0; }	
    int get_iDec() { return _iDec; }	
    double rValue(int raw);
    int iRaw(double value);

private:
    void init(double rRaw0);
    void init();
    double _rLo, _rHi;
    double _rStep;
    int _iDec;
    double _r0;
    int iRawUndef;
    short _iWidth;
    ilwisStoreType st;
};

/************************************************************************/
/*                     ILWISInfo                                        */
/************************************************************************/

struct ILWISInfo
{
    ILWISInfo() : bUseValueRange(false), vr(0, 0) {}
    bool bUseValueRange;
    ValueRange vr;
    ilwisStoreType stStoreType;
    std::string stDomain;
};

/************************************************************************/
/*                           ILWISRasterBand                            */
/************************************************************************/

class ILWISDataset;

class ILWISRasterBand : public GDALPamRasterBand
{
    friend class ILWISDataset;
public:
    VSILFILE *fpRaw;
    ILWISInfo psInfo;
    int nSizePerPixel;

    ILWISRasterBand( ILWISDataset *, int );
    ~ILWISRasterBand();
    CPLErr GetILWISInfo(std::string pszFileName);
    void ILWISOpen( std::string pszFilename);
				
    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * ); 
    virtual double GetNoDataValue( int *pbSuccess );

private:
    void FillWithNoData(void * pImage);
    void SetValue(void *pImage, int i, double rV);
    double GetValue(void *pImage, int i);
    void ReadValueDomainProperties(std::string pszFileName);
};

/************************************************************************/
/*	                   ILWISDataset					*/
/************************************************************************/
class ILWISDataset : public GDALPamDataset
{
    friend class ILWISRasterBand;
    CPLString osFileName;
    std::string pszIlwFileName;
    char	 *pszProjection;
    double adfGeoTransform[6];
    int    bGeoDirty;
    int		 bNewDataset;            /* product of Create() */
    std::string pszFileType; //indicating the input dataset: Map/MapList
    CPLErr ReadProjection( std::string csyFileName);
    CPLErr WriteProjection();
    CPLErr WriteGeoReference();
    void   CollectTransformCoef(std::string &pszRefFile );
		
public:
    ILWISDataset();
    ~ILWISDataset();

    static GDALDataset *Open( GDALOpenInfo * );
		
    static GDALDataset *CreateCopy( const char * pszFilename,
                                    GDALDataset *poSrcDS,
                                    int bStrict, char ** papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void * pProgressData );

    static GDALDataset *Create(const char* pszFilename,
                               int nXSize, int nYSize, 
                               int nBands, GDALDataType eType,
                               char** papszParmList); 
		
    virtual CPLErr 	GetGeoTransform( double * padfTransform );
    virtual CPLErr  SetGeoTransform( double * );

    virtual const char *GetProjectionRef(void);
    virtual CPLErr SetProjection( const char * );

    virtual void   FlushCache( void );
};

// IniFile.h: interface for the IniFile class.
// 
//////////////////////////////////////////////////////////////////////

class CompareAsNum
{
public:
    bool operator() (const std::string&, const std::string&) const;
};

typedef std::map<std::string, std::string>          SectionEntries;
typedef std::map<std::string, SectionEntries*> Sections;

class IniFile  
{
public:
    IniFile(const std::string& filename);
    virtual ~IniFile();

    void SetKeyValue(const std::string& section, const std::string& key, const std::string& value);
    std::string GetKeyValue(const std::string& section, const std::string& key);

    void RemoveKeyValue(const std::string& section, const std::string& key);
    void RemoveSection(const std::string& section);

private:
    std::string filename;
    Sections sections;
    bool bChanged;

    void Load();
    void Store();
};


