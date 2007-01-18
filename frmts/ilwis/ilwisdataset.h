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
//#include "Windows.h"
#include <string>
#include <algorithm>
#include <map>

#ifdef WIN32
#include  <io.h>
#endif

#include  <stdio.h>
#include  <stdlib.h>


using namespace std;

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
	ValueRange(string str);
	string ToString();
	double get_Minimum() { return _rLo; }
	double get_Maximum() { return _rHi; }
	double get_StepSize() { return _rStep; }

public:
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
		ILWISInfo() : vr(0, 0){}
		bool bValue;
		ValueRange vr;
		ilwisStoreType stStoreType;
		string stDomain;
};

/************************************************************************/
/*                           ILWISRasterBand                            */
/************************************************************************/

class ILWISDataset;

class ILWISRasterBand : public GDALPamRasterBand
{
  friend class ILWISDataset;
	public:
				FILE *fpRaw;
				ILWISInfo psInfo;
				int nSizePerPixel;

				ILWISRasterBand( ILWISDataset *, int );
				CPLErr GetILWISInfo(string pszFileName);
				void ILWISOpen( string pszFilename);
				
  virtual CPLErr IReadBlock( int, int, void * );
	virtual CPLErr IWriteBlock( int, int, void * ); 
	virtual double GetNoDataValue( int *pbSuccess );

  private:
        void FillWithNoData(void * pImage);
};

/************************************************************************/
/*	                   ILWISDataset					*/
/************************************************************************/
class ILWISDataset : public GDALPamDataset
{
    friend class ILWISRasterBand;
    const char *pszFileName;
    string pszIlwFileName;
    char	 *pszProjection;
    double adfGeoTransform[6];
    int    bGeoDirty;
    int		 bNewDataset;            /* product of Create() */
    string pszFileType; //indicating the input dataset: Map/MapList
    CPLErr ReadProjection( string csyFileName);
    CPLErr WriteProjection();
    CPLErr WriteGeoReference();
    void   CollectTransformCoef(string &pszRefFile );
		
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

using std::map;

class CompareAsNum
{
public:
	bool operator() (const string&, const string&) const;
};

typedef map<string, string>          SectionEntries;
typedef map<string, SectionEntries*> Sections;

class IniFile  
{
public:
	IniFile();
	virtual ~IniFile();

	void Open(const string& filename);
	void Close();

	void SetKeyValue(const string& section, const string& key, const string& value);
	string GetKeyValue(const string& section, const string& key);

	void RemoveKeyValue(const string& section, const string& key);
	void RemoveSection(const string& section);

private:
	string filename;
	Sections sections;

	void Load();
	void Flush();
};


