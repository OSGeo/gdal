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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.18  2006/12/20 14:33:15  lichun
 * Solved one warning for some compilers: an unsigned value was compared with a signed value.
 *
 * Revision 1.17  2006/12/18 03:49:24  fwarmerdam
 * Avoid really big memory leak in readblock.
 *
 * Revision 1.16  2006/11/22 18:20:00  fwarmerdam
 * apply std:: prefix to exception to build on sun compiler
 *
 * Revision 1.15  2005/09/14 13:37:18  dron
 * Avoid warnings.
 *
 * Revision 1.14  2005/08/04 15:26:53  fwarmerdam
 * added log headers
 *
 */


#include "ilwisdataset.h"
#include <float.h>

// IniFile.cpp: implementation of the IniFile class.
//
//////////////////////////////////////////////////////////////////////
bool CompareAsNum::operator() (const string& s1, const string& s2) const
{
	long Num1 = atoi(s1.c_str());
	long Num2 = atoi(s2.c_str());
	return Num1 < Num2;
}

string TrimSpaces(const string& input)
{
    // find first non space
    if ( input.empty()) 
        return string();

    int iFirstNonSpace = input.find_first_not_of(' ');
    int iFindLastSpace = input.find_last_not_of(' ');
    if (iFirstNonSpace == string::npos || iFindLastSpace == string::npos)
        return string();

    return input.substr(iFirstNonSpace, iFindLastSpace - iFirstNonSpace + 1);
}

char line[1024];

string GetLine(FILE* fil)
{
	char *p = fgets(line, 1024, fil);
	if (p == NULL)
		return string();

	p = line + strlen(line) - 1; // move to last char in buffer
	while ((p >= line) && isspace(*p))
		--p;         // isspace is succesful at least once, because of the "\n"
	*(p + 1) = '\0';   // therefore this will not fail

	return string(line);
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
IniFile::IniFile()
{

}

IniFile::~IniFile()
{

}

void IniFile::Open(const string& filenam)
{
    filename = filenam;

    Load();
}

void IniFile::Close()
{
    Flush();

    for (Sections::iterator iter = sections.begin(); iter != sections.end(); ++iter)
    {
        (*(*iter).second).clear();
        delete (*iter).second;
    }

    sections.clear();
}

void IniFile::SetKeyValue(const string& section, const string& key, const string& value)
{
    Sections::iterator iterSect = sections.find(section);
    if (iterSect == sections.end())
    {
        // Add a new section, with one new key/value entry
        SectionEntries *entries = new SectionEntries;
        (*entries)[key] = value;
        sections[section] = entries;
    }
    else
    {
        // Add one new key/value entry in an existing section
        SectionEntries *entries = (*iterSect).second;
        (*entries)[key] = value;
    }
}
string IniFile::GetKeyValue(const string& section, const string& key)
{
	Sections::iterator iterSect = sections.find(section);
	if (iterSect != sections.end())
	{
		SectionEntries *entries = (*iterSect).second;
		SectionEntries::iterator iterEntry = (*entries).find(key);
		if (iterEntry != (*entries).end())
			return (*iterEntry).second;
	}

	return string();
}

void IniFile::RemoveKeyValue(const string& section, const string& key)
{
	Sections::iterator iterSect = sections.find(section);
	if (iterSect != sections.end())
	{
		// The section exists, now erase entry "key"
		SectionEntries *entries = (*iterSect).second;
		(*entries).erase(key);
	}
}

void IniFile::RemoveSection(const string& section)
{
	Sections::iterator iterSect = sections.find(section);
	if (iterSect != sections.end())
	{
		// The section exists, so remove it and all its entries.
		SectionEntries *entries = (*iterSect).second;
		(*entries).clear();
		sections.erase(iterSect);
	}
}

void IniFile::Load()
{
    enum ParseState { FindSection, FindKey, ReadFindKey, StoreKey, None } state;
    FILE *filIni = fopen(filename.c_str(), "r");
    if (filIni == NULL)
        return;

    string section, key, value;
    state = FindSection;
    string s;
    while (!feof(filIni))
    {
        switch (state)
        {
          case FindSection:
            s = GetLine(filIni);
            if (s.empty())
                continue;

            if (s[0] == '[')
            {
                int iLast = s.find_first_of(']');
                if (iLast != string::npos)
                {
                    section = s.substr(1, iLast - 1);
                    state = ReadFindKey;
                }
            }
            else
                state = FindKey;
            break;
          case ReadFindKey:
            s = GetLine(filIni); // fall through (no break)
          case FindKey:
          {
              int iEqu = s.find_first_of('=');
              if (iEqu != string::npos)
              {
                  key = s.substr(0, iEqu);
                  value = s.substr(iEqu + 1);
                  state = StoreKey;
              }
              else
                  state = ReadFindKey;
          }
          break;
          case StoreKey:
            SetKeyValue(section, key, value);
            state = FindSection;
            break;
            
          case None:
            // Do we need to do anything?  Perhaps this never occurs.
            break;
        }
    }

    fclose(filIni);
}

void IniFile::Flush()
{
	FILE *filIni = fopen(filename.c_str(), "w+");
	if (filIni == NULL)
		return;

	Sections::iterator iterSect;
	for (iterSect = sections.begin(); iterSect != sections.end(); ++iterSect)
	{
		// write the section name
		fprintf(filIni, "[%s]\n", (*iterSect).first.c_str());
		SectionEntries *entries = (*iterSect).second;
		SectionEntries::iterator iterEntry;
		for (iterEntry = (*entries).begin(); iterEntry != (*entries).end(); ++iterEntry)
		{
			string key = (*iterEntry).first;
			fprintf(filIni, "%s=%s\n", TrimSpaces(key).c_str(), (*iterEntry).second.c_str());
		}

		fprintf(filIni, "\n");
	}

	fclose(filIni);
}

// End of the implementation of IniFile class. ///////////////////////
//////////////////////////////////////////////////////////////////////

static long longConv(double x) {
    if ((x == rUNDEF) || (x > LONG_MAX) || (x < LONG_MIN))
      return iUNDEF;
    else
      return (long)floor(x + 0.5);
}

string ReadElement(string section, string entry, string filename)
{
		if (section.length() == 0)
			return string();
		if (entry.length() == 0)
			return string();
		if (filename.length() == 0)
			return string();

		IniFile MyIniFile;
		MyIniFile = IniFile();
		MyIniFile.Open(filename);

		return MyIniFile.GetKeyValue(section, entry);
}

bool WriteElement(string sSection, string sEntry,
                             string fn, string sValue)
{
  if (0 == fn.length())
    return false;

	IniFile MyIniFile;
	MyIniFile = IniFile();
	MyIniFile.Open(fn);

	MyIniFile.SetKeyValue(sSection, sEntry, sValue);
	MyIniFile.Close();
	return true;
}

bool WriteElement(string sSection, string sEntry,
                             string fn, int nValue)
{
	if (0 == fn.length())
    return false;

	char strdouble[45];
	sprintf(strdouble, "%d", nValue);
	string sValue = string(strdouble);
  return WriteElement(sSection, sEntry, fn, sValue) != 0;
}

bool WriteElement(string sSection, string sEntry,
                             string fn, double dValue)
{
	if (0 == fn.length())
    return false;

	char strdouble[45];
	sprintf(strdouble, "%.6f", dValue);
	string sValue = string(strdouble);
  return WriteElement(sSection, sEntry, fn, sValue) != 0;
}

static CPLErr GetRowCol(string str,int &Row, int &Col)
{
    string delimStr = " ,;";
    int iPos = str.find_first_of(delimStr);
    if (iPos != string::npos)
    {
        Row = atoi(str.substr(0, iPos).c_str());
    }
    else 
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Read of RowCol failed.");
        return CE_Failure;
    }
    iPos = str.find_last_of(delimStr);
    if (iPos != string::npos)
    {
        Col = atoi(str.substr(iPos+1, str.length()-iPos).c_str());
    }
    return CE_None;
}

//! Converts ILWIS data type to GDAL data type.
static GDALDataType ILWIS2GDALType(ilwisStoreType stStoreType)
{
  GDALDataType eDataType = GDT_Unknown;

  switch (stStoreType){
	  case stByte: {
	    eDataType = GDT_Byte;
      break;
		} 
    case stInt:{
      eDataType = GDT_Int16;
      break;
		}
    case stLong:{
      eDataType = GDT_Int32;
      break;
		}
    case stFloat:{
      eDataType = GDT_Float32;
      break;
    }
    case stReal:{
      eDataType = GDT_Float64;
      break;
    }
    default: {
      break;
    }
  }

  return eDataType;
}

//Determine store type of ILWIS raster
static string GDALType2ILWIS(GDALDataType type)
{
	string sStoreType;
	sStoreType = "";
	switch( type )
  {
	  case GDT_Byte:{
      sStoreType = "Byte";
      break;
		}
    case GDT_Int16:
    case GDT_UInt16:{
      sStoreType = "Int";
      break;
		}
    case GDT_Int32:
    case GDT_UInt32:{
      sStoreType = "Long";
      break;
		}
    case GDT_Float32:{
      sStoreType = "Float";
      break;
    }
    case GDT_Float64:{
      sStoreType = "Real";
      break;
		}
    default:{
      CPLError( CE_Failure, CPLE_NotSupported,
                "Data type %s not supported by ILWIS format.\n",
                GDALGetDataTypeName( type ) );
      break;
		}	
  }
	return sStoreType;
}

static CPLErr GetStoreType(string pszFileName, ilwisStoreType &stStoreType)
{
    string st = ReadElement("MapStore", "Type", pszFileName.c_str());
    transform(st.begin(), st.end(), st.begin(), tolower);
		
    if( EQUAL(st.c_str(),"byte"))
    {
        stStoreType = stByte;
    }
    else if( EQUAL(st.c_str(),"int"))
    {
        stStoreType = stInt;
    }
    else if( EQUAL(st.c_str(),"long"))
    {
        stStoreType = stLong;
    }
    else if( EQUAL(st.c_str(),"float"))
    {
        stStoreType = stFloat;
    }
    else if( EQUAL(st.c_str(),"real"))
    {
        stStoreType = stReal;
    }
    else 
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unsupported ILWIS store type.");
        return CE_Failure;
    }
    return CE_None;
}


ILWISDataset::ILWISDataset()

{
		bGeoDirty = FALSE;
		bNewDataset = FALSE;
    pszProjection = CPLStrdup("");
		adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                  ~ILWISDataset()                                     */
/************************************************************************/

ILWISDataset::~ILWISDataset()

{
    FlushCache();
    CPLFree( pszProjection );
}

/************************************************************************/
/*                        CollectTransformCoef()                        */
/*                                                                      */
/*      Collect the geotransform, support for the GeoRefCorners         */
/*      georeferencing only; We use the extent of the coordinates       */
/*      to determine the pixelsize in X and Y direction. Then calculate */
/*      the transform coefficients from the extent and pixelsize        */
/************************************************************************/

void ILWISDataset::CollectTransformCoef(string &pszRefName)

{
    pszRefName = "";
    string georef;
    if ( EQUAL(pszFileType.c_str(),"Map") )
        georef = ReadElement("Map", "GeoRef", pszFileName);
    else
        georef = ReadElement("MapList", "GeoRef", pszFileName);

    transform(georef.begin(), georef.end(), georef.begin(), tolower);

    //Capture the geotransform, only if the georef is not 'none', 
    //otherwise, the default transform should be returned.
    if( (georef.length() != 0) && !EQUAL(georef.c_str(),"none"))
    {
        //Form the geo-referencing name
        string pszBaseName = string(CPLStrdup( CPLGetBasename(georef.c_str()) ));
        string pszPath = string(CPLStrdup(CPLGetPath( pszFileName )));
        pszRefName = string(CPLFormFilename(pszPath.c_str(),
                                            pszBaseName.c_str(),"grf" ));

        //Check the geo-reference type,support for the GeoRefCorners only 
        string georeftype = ReadElement("GeoRef", "Type", pszRefName);
        if (EQUAL(georeftype.c_str(),"GeoRefCorners"))
        {
            //Center or top-left corner of the pixel approach? 
            string IsCorner = ReadElement("GeoRefCorners", "CornersOfCorners", pszRefName);

            //Collect the extent of the coordinates 
            string sMinX = ReadElement("GeoRefCorners", "MinX", pszRefName);
            string sMinY = ReadElement("GeoRefCorners", "MinY", pszRefName);
            string sMaxX = ReadElement("GeoRefCorners", "MaxX", pszRefName);
            string sMaxY = ReadElement("GeoRefCorners", "MaxY", pszRefName);
				
            //Calculate pixel size in X and Y direction from the extent
            double PixelSizeX = floor((atof(sMaxX.c_str()) - 
                                       atof(sMinX.c_str())) / nRasterXSize + 0.5);
            double PixelSizeY = floor((atof(sMaxY.c_str()) - 
                                       atof(sMinY.c_str())) / nRasterYSize + 0.5);
				
            if (EQUAL(IsCorner.c_str(),"Yes"))
            {
                adfGeoTransform[0] = atof(sMinX.c_str());
                adfGeoTransform[3] = atof(sMaxY.c_str());
            }
            else
            {
                adfGeoTransform[0] = atof(sMinX.c_str()) - PixelSizeX/2.0;
                adfGeoTransform[3] = atof(sMaxY.c_str()) + PixelSizeY/2.0;
            }

            adfGeoTransform[1] = PixelSizeX;
            adfGeoTransform[2] = 0.0;
            adfGeoTransform[4] = 0.0;
            adfGeoTransform[5] = -PixelSizeY;
        }

    }

}

/************************************************************************/
/*                         WriteGeoReference()                          */
/*                                                                      */
/*      Try to write a geo-reference file for the dataset to create     */
/************************************************************************/

CPLErr ILWISDataset::WriteGeoReference()
{
    string grFileName = CPLResetExtension(pszFileName, "grf" );
    double dLLLat, dLLLong, dURLat, dURLong;
    string georef;
		
    int   nXSize = GetRasterXSize();
    int   nYSize = GetRasterYSize();
		
    if( GetGeoTransform( adfGeoTransform ) == CE_None
        && (adfGeoTransform[0] != 0.0 || adfGeoTransform[1] != 1.0
            || adfGeoTransform[2] != 0.0 || adfGeoTransform[3] != 0.0
            || adfGeoTransform[4] != 0.0 || fabs(adfGeoTransform[5]) != 1.0))
    {
        SetGeoTransform( adfGeoTransform );
        if (adfGeoTransform[2] == 0.0 && adfGeoTransform[4] == 0.0)
        {
            //check wheather we should write out a georeference file. 
            //dataset must be north up 
            dLLLat = (adfGeoTransform[3] 
                      + GetRasterYSize() * adfGeoTransform[5] );
            dLLLong = (adfGeoTransform[0] );
            dURLat  = (adfGeoTransform[3] );
            dURLong = (adfGeoTransform[0] 
                       + GetRasterXSize() * adfGeoTransform[1] );
            WriteElement("Ilwis", "Type", grFileName, "GeoRef");
            WriteElement("GeoRef", "lines", grFileName, nYSize);
            WriteElement("GeoRef", "columns", grFileName, nXSize);
            WriteElement("GeoRef", "Type", grFileName, "GeoRefCorners");
            WriteElement("GeoRefCorners", "CornersOfCorners", grFileName, "Yes");
            WriteElement("GeoRefCorners", "MinX", grFileName, dLLLong);
            WriteElement("GeoRefCorners", "MinY", grFileName, dLLLat);
            WriteElement("GeoRefCorners", "MaxX", grFileName, dURLong);
            WriteElement("GeoRefCorners", "MaxY", grFileName, dURLat);

            //Re-write the GeoRef property to raster ODF
            //Form band file name  
            string sBaseName = string(CPLStrdup( CPLGetBasename(pszFileName) ));
            string sPath = string(CPLStrdup(CPLGetPath(pszFileName)));
            if (nBands == 1) 
            {
                WriteElement("Map", "GeoRef", pszFileName, sBaseName + ".grf");
            }
            else
            {
                for( int iBand = 0; iBand < nBands; iBand++ )
                {
                    if (iBand == 0)
                      WriteElement("MapList", "GeoRef", pszFileName, sBaseName + ".grf");
                    char pszName[100];
                    sprintf(pszName, "%s_band_%d", sBaseName.c_str(),iBand + 1 );
                    string pszODFName = string(CPLFormFilename(sPath.c_str(),pszName,"mpr"));
                    WriteElement("Map", "GeoRef", pszODFName, sBaseName + ".grf");
                }
            }
        }
    }
    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *ILWISDataset::GetProjectionRef()

{
   return ( pszProjection );
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr ILWISDataset::SetProjection( const char * pszNewProjection )

{
    CPLFree( pszProjection );
    pszProjection = CPLStrdup( pszNewProjection );
    bGeoDirty = TRUE;

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr ILWISDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform,  adfGeoTransform, sizeof(double) * 6 ); 
    return( CE_None );
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr ILWISDataset::SetGeoTransform( double * padfTransform )

{
    memcpy( adfGeoTransform, padfTransform, sizeof(double)*6 );
		if (adfGeoTransform[2] == 0.0 && adfGeoTransform[4] == 0.0)
			bGeoDirty = TRUE;

    return CE_None;
}

bool CheckASCII(unsigned char * buf, int size)
{
	for (int i = 0; i < size; ++i)
		if (!isascii(buf[i]))
			return false;

	return true;
}
/************************************************************************/
/*                       Open()                                         */
/************************************************************************/

GDALDataset *ILWISDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Does this look like an ILWIS file                               */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->fp == NULL)
        return NULL;

    string sExt = CPLGetExtension( poOpenInfo->pszFilename );
    if (!EQUAL(sExt.c_str(),"mpr") && !EQUAL(sExt.c_str(),"mpl"))
        return NULL;

    if (!CheckASCII(poOpenInfo->pabyHeader, poOpenInfo->nHeaderBytes))
        return NULL;

    string ilwistype = ReadElement("Ilwis", "Type", poOpenInfo->pszFilename);
    if( ilwistype.length() == 0)
        return NULL;
		
    string sFileType;	//map or map list
    int    iBandCount;
    string mapsize;
    string maptype = ReadElement("BaseMap", "Type", poOpenInfo->pszFilename);
    string sBaseName = string(CPLStrdup( CPLGetBasename(poOpenInfo->pszFilename) ));
    string sPath = string(CPLStrdup(CPLGetPath( poOpenInfo->pszFilename)));
							
    //Verify whether it is a map list or a map
    if( EQUAL(ilwistype.c_str(),"MapList") )
    {
        sFileType = string("MapList");
        string sMaps = ReadElement("MapList", "Maps", poOpenInfo->pszFilename);
        iBandCount = atoi(sMaps.c_str());
        mapsize = ReadElement("MapList", "Size", poOpenInfo->pszFilename);
        for (int iBand = 0; iBand < iBandCount; ++iBand )
        {
            //Form the band file name.
            char cBandName[45];
            sprintf( cBandName, "Map%d", iBand);
            string sBandName = ReadElement("MapList", string(cBandName), poOpenInfo->pszFilename);
            string pszBandBaseName = string(CPLStrdup( CPLGetBasename(sBandName.c_str()) ));
            string pszBandPath = string(CPLStrdup(CPLGetPath( sBandName.c_str())));
            if ( 0 == pszBandPath.length() )
            { 
                sBandName = string(CPLFormFilename(sPath.c_str(),
                                                   pszBandBaseName.c_str(),"mpr" ));
            }
            //Verify the file exetension, it must be an ILWIS raw data file
            //with extension .mp#, otherwise, unsupported 
            //This drive only supports a map list which stores a set of ILWIS raster maps, 
            string sMapStoreName = ReadElement("MapStore", "Data", sBandName);
            string sExt = CPLGetExtension( sMapStoreName.c_str() );
            if ( !EQUALN( sExt.c_str(), "mp#", 3 ))
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Unsupported ILWIS data file. \n"
                          "can't treat as raster.\n" );
                return FALSE;
            }
        }
    }	
    else if(EQUAL(ilwistype.c_str(),"BaseMap") && EQUAL(maptype.c_str(),"Map"))
    {
        sFileType = "Map";
        iBandCount = 1;
        mapsize = ReadElement("Map", "Size", poOpenInfo->pszFilename);
        string sMapType = ReadElement("Map", "Type", poOpenInfo->pszFilename);
        ilwisStoreType stStoreType;
        if (  
            GetStoreType(string(poOpenInfo->pszFilename), stStoreType) != CE_None )
        {
            //CPLError( CE_Failure, CPLE_AppDefined,
            //			"Unsupported ILWIS data file. \n"
            //			"can't treat as raster.\n" );
            return FALSE;
        }
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unsupported ILWIS data file. \n"
                  "can't treat as raster.\n" );
        return FALSE;
    }	

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    ILWISDataset 	*poDS;
    poDS = new ILWISDataset();
		
    poOpenInfo->fp = NULL;
    
/* -------------------------------------------------------------------- */
/*      Capture raster size from ILWIS file (.mpr).                     */
/* -------------------------------------------------------------------- */
    int Row = 0, Col = 0;
    if ( GetRowCol(mapsize, Row, Col) != CE_None)
        return FALSE;
    poDS->nRasterXSize = Col;
    poDS->nRasterYSize = Row;
    poDS->pszFileName = poOpenInfo->pszFilename;
    poDS->pszFileType = sFileType;  
/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    //poDS->pszFileName = new char[strlen(poOpenInfo->pszFilename) + 1];
    poDS->nBands = iBandCount;
    for( int iBand = 0; iBand < poDS->nBands; iBand++ )
    {
        poDS->SetBand( iBand+1, new ILWISRasterBand( poDS, iBand+1 ) );
    }
		
/* -------------------------------------------------------------------- */
/*      Collect the geotransform coefficients                           */
/* -------------------------------------------------------------------- */
    string pszGeoRef;
    poDS->CollectTransformCoef(pszGeoRef);

/* -------------------------------------------------------------------- */
/*      Translation from ILWIS coordinate system definition             */
/* -------------------------------------------------------------------- */
    if( (pszGeoRef.length() != 0) && !EQUAL(pszGeoRef.c_str(),"none"))
    {
		
        //	Fetch coordinate system
        string csy = ReadElement("GeoRef", "CoordSystem", pszGeoRef);
        transform(csy.begin(), csy.end(), csy.begin(), tolower);

        string pszProj;
        if( (csy.length() != 0) && !EQUAL(csy.c_str(),"unknown.csy"))
        {

            //Form the coordinate system file name
            if( !(EQUALN( csy.c_str(), "latlon.csy", 10 )) && 
                !(EQUALN( csy.c_str(), "LatlonWGS84.csy", 15 )))			
            {
                string pszBaseName = string(CPLStrdup( CPLGetBasename(csy.c_str()) ));
                string pszPath = string(CPLStrdup(CPLGetPath( poDS->pszFileName )));
                csy = string(CPLFormFilename(pszPath.c_str(),
                                             pszBaseName.c_str(),"csy" ));
                pszProj = ReadElement("CoordSystem", "Type", csy);
                if (pszProj.length() == 0 ) //default to projection
                    pszProj = "Projection";
            }
            else
            {
                pszProj = "LatLon";
            }
					
            if( (EQUALN( pszProj.c_str(), "LatLon", 6 )) || 
                (EQUALN( pszProj.c_str(), "Projection", 10 )))			
                poDS->ReadProjection( csy );
        }
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

    return( poDS );
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void ILWISDataset::FlushCache()

{
    GDALDataset::FlushCache();

    if( bGeoDirty == TRUE )
		{
				WriteGeoReference();
        WriteProjection();
				bGeoDirty = FALSE;
		}
}

/************************************************************************/
/*                               Create()                               */
/*                                                                      */
/*      Create a new ILWIS file.                                         */
/************************************************************************/

GDALDataset *ILWISDataset::Create(const char* pszFilename,
                                  int nXSize, int nYSize, 
                                  int nBands, GDALDataType eType,
                                  char** papszParmList) 
{
    ILWISDataset	*poDS;
    int 		iBand;

/* -------------------------------------------------------------------- */
/*      Verify input options.                                           */
/* -------------------------------------------------------------------- */
    if( eType != GDT_Byte && eType != GDT_Int16 && eType != GDT_Int32
        && eType != GDT_Float32 && eType != GDT_Float64 && eType != GDT_UInt16 && eType != GDT_UInt32)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to create ILWIS dataset with an illegal\n"
                  "data type (%s).\n",
                  GDALGetDataTypeName(eType) );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Translate the data type.                                        */
/*			Determine store type of ILWIS raster                            */
/* -------------------------------------------------------------------- */
    string sDomain= "value.dom";
    double stepsize = 1;
		string sStoreType = GDALType2ILWIS(eType);
		if( EQUAL(sStoreType.c_str(),""))
			return NULL;
		else if( EQUAL(sStoreType.c_str(),"Real") || EQUAL(sStoreType.c_str(),"float"))
			stepsize = 0;

    string pszBaseName = string(CPLStrdup( CPLGetBasename( pszFilename )));
    string pszPath = string(CPLStrdup( CPLGetPath( pszFilename )));
		
/* -------------------------------------------------------------------- */
/*      Write out object definition file for each band                    */
/* -------------------------------------------------------------------- */
    string pszODFName;
    string pszDataBaseName;
    string pszFileName;

    char strsize[45];
    sprintf(strsize, "%d %d", nYSize, nXSize);
    
    //Form map/maplist name. 
    if ( nBands == 1 )
    {
        pszODFName = string(CPLFormFilename(pszPath.c_str(),pszBaseName.c_str(),"mpr"));
        pszDataBaseName = pszBaseName;
        pszFileName = CPLFormFilename(pszPath.c_str(),pszBaseName.c_str(),"mpr");
    }
    else
    {
        pszFileName = CPLFormFilename(pszPath.c_str(),pszBaseName.c_str(),"mpl");
        WriteElement("Ilwis", "Type", string(pszFileName), "MapList");
        WriteElement("MapList", "GeoRef", string(pszFileName), "none.grf");
        WriteElement("MapList", "Size", string(pszFileName), string(strsize));
        WriteElement("MapList", "Maps", string(pszFileName), nBands);
    }

    for( iBand = 0; iBand < nBands; iBand++ )
    {
        if ( nBands > 1 )
        {
            char pszBandName[100];
            sprintf(pszBandName, "%s_band_%d", pszBaseName.c_str(),iBand + 1 );
            pszODFName = string(pszBandName) + ".mpr";
            pszDataBaseName = string(pszBandName);
            sprintf(pszBandName, "Map%d", iBand);	
            WriteElement("MapList", string(pszBandName), string(pszFileName), pszODFName);
            pszODFName = CPLFormFilename(pszPath.c_str(),pszDataBaseName.c_str(),"mpr");
        }
/* -------------------------------------------------------------------- */
/*      Write data definition per band (.mpr)                 */
/* -------------------------------------------------------------------- */
				
        WriteElement("Ilwis", "Type", pszODFName, "BaseMap");
        WriteElement("BaseMap", "Type", pszODFName, "Map");
        WriteElement("Map", "Type", pszODFName, "MapStore");
				
        double adfMinMax[2];
        adfMinMax[0] = -9999999.9;
        adfMinMax[1] = 9999999.9;
        
        WriteElement("BaseMap", "Domain", pszODFName, sDomain);
        string pszDataName = pszDataBaseName + ".mp#";
        WriteElement("MapStore", "Data", pszODFName, pszDataName);
        WriteElement("MapStore", "Structure", pszODFName, "Line");
        WriteElement("MapStore", "Type", pszODFName, sStoreType);

        char strdouble[45];
        sprintf(strdouble, "%.3f:%.3f:%3f:offset=0", adfMinMax[0], adfMinMax[1],stepsize);
        string range = string(strdouble);
        WriteElement("BaseMap", "Range", pszODFName, range);
        WriteElement("Map", "GeoRef", pszODFName, "none.grf");
        WriteElement("Map", "Size", pszODFName, string(strsize));
							
/* -------------------------------------------------------------------- */
/*      Try to create the data file.                                    */
/* -------------------------------------------------------------------- */
        pszDataName = CPLResetExtension(pszODFName.c_str(), "mp#" );
//				FILE  *fp = fopen( pszDataName.c_str(), "wb" );
        FILE  *fp = VSIFOpenL( pszDataName.c_str(), "wb" );

        if( fp == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Unable to create file %s.\n", pszDataName.c_str() );
            return NULL;
        }
        VSIFCloseL( fp );
    }
    poDS = new ILWISDataset();
    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->nBands = nBands;
    poDS->eAccess = GA_Update;
		poDS->bNewDataset = TRUE;
    poDS->SetDescription(pszFilename);
		poDS->pszProjection = CPLStrdup("");
    poDS->pszFileName = pszFileName.c_str();
		poDS->pszIlwFileName = string(pszFileName);
		if ( nBands == 1 )
        poDS->pszFileType = "Map";
    else
        poDS->pszFileType = "MapList";

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */

    for( iBand = 1; iBand <= poDS->nBands; iBand++ )
    {
        poDS->SetBand( iBand, new ILWISRasterBand( poDS, iBand ) );
    }

		return poDS;
    //return (GDALDataset *) GDALOpen( pszFilename, GA_Update );
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *
ILWISDataset::CreateCopy( const char * pszFilename, GDALDataset *poSrcDS,
                        int bStrict, char ** papszOptions,
                        GDALProgressFunc pfnProgress, void * pProgressData )

{
    
    ILWISDataset	*poDS;
    GDALDataType eType = GDT_Byte;
    int iBand;
    (void) bStrict;

		
    int   nXSize = poSrcDS->GetRasterXSize();
    int   nYSize = poSrcDS->GetRasterYSize();
    int   nBands = poSrcDS->GetRasterCount();

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create the basic dataset.                                       */
/* -------------------------------------------------------------------- */
    for( iBand = 0; iBand < nBands; iBand++ )
    {
        GDALRasterBand *poBand = poSrcDS->GetRasterBand( iBand+1 );
        eType = GDALDataTypeUnion( eType, poBand->GetRasterDataType() );
    }

    poDS = (ILWISDataset *) Create( pszFilename,
                                    poSrcDS->GetRasterXSize(),
                                    poSrcDS->GetRasterYSize(),
                                    nBands,
                                    eType, papszOptions );

    if( poDS == NULL )
        return NULL;
    string pszBaseName = string(CPLStrdup( CPLGetBasename( pszFilename )));
    string pszPath = string(CPLStrdup( CPLGetPath( pszFilename )));
				
/* -------------------------------------------------------------------- */
/*  Copy and geo-transform and projection information.                                    */
/* -------------------------------------------------------------------- */
    double adfGeoTransform[6];
    string georef;
    const char  *pszProj;

    if( poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None
        && (adfGeoTransform[0] != 0.0 || adfGeoTransform[1] != 1.0
            || adfGeoTransform[2] != 0.0 || adfGeoTransform[3] != 0.0
            || adfGeoTransform[4] != 0.0 || fabs(adfGeoTransform[5]) != 1.0))
    {
        poDS->SetGeoTransform( adfGeoTransform );
        if (adfGeoTransform[2] == 0.0 && adfGeoTransform[4] == 0.0)
        {
            //check wheather we should create georeference file. 
            //source dataset must be north up 
            georef = pszBaseName + ".grf";
        }
        else
        {
            georef = "none.grf";
        }
    }

    pszProj = poSrcDS->GetProjectionRef();
    if( pszProj != NULL && strlen(pszProj) > 0 )
        poDS->SetProjection( pszProj );

/* -------------------------------------------------------------------- */
/*      Create the output raster files for each band                    */
/* -------------------------------------------------------------------- */
		
    for( iBand = 0; iBand < nBands; iBand++ )
    {
        FILE *fpData;
        GByte *pData;
        
        GDALRasterBand *poBand = poSrcDS->GetRasterBand( iBand+1 );
        ILWISRasterBand *desBand = (ILWISRasterBand *) poDS->GetRasterBand( iBand+1 );
        
/* -------------------------------------------------------------------- */
/*      Translate the data type.                                        */
/* -------------------------------------------------------------------- */
        double stepsize = 1;
        double dNoDataValue;
        int pbSuccess;
        dNoDataValue = poBand->GetNoDataValue(&pbSuccess); 
        int nLineSize =  nXSize * GDALGetDataTypeSize(eType) / 8;
        pData = (GByte *) CPLMalloc( nLineSize );
        				
        //Determine store type of ILWIS raster
        string sStoreType = GDALType2ILWIS( eType );
				if( EQUAL(sStoreType.c_str(),""))
  		  	return NULL;
	    	else if( EQUAL(sStoreType.c_str(),"Real") || EQUAL(sStoreType.c_str(),"float"))
			    stepsize = 0;

        //Form the image file name, create the object definition file. 
        string pszODFName;
        string pszDataBaseName;
        if (nBands == 1) 
        {
            pszODFName = string(CPLFormFilename(pszPath.c_str(),pszBaseName.c_str(),"mpr"));
            pszDataBaseName = pszBaseName;
        }
        else
        {
            char pszName[100];
            sprintf(pszName, "%s_band_%d", pszBaseName.c_str(),iBand + 1 );
            pszODFName = string(CPLFormFilename(pszPath.c_str(),pszName,"mpr"));
            pszDataBaseName = string(pszName);
        }
/* -------------------------------------------------------------------- */
/*      Write data definition file for each band (.mpr)                 */
/* -------------------------------------------------------------------- */
				
        double adfMinMax[2];
        int    bGotMin, bGotMax;

        adfMinMax[0] = poBand->GetMinimum( &bGotMin );
        adfMinMax[1] = poBand->GetMaximum( &bGotMax );
        if( ! (bGotMin && bGotMax) )
            GDALComputeRasterMinMax((GDALRasterBandH)poBand, FALSE, adfMinMax);
				if ((!CPLIsNan(adfMinMax[0])) && CPLIsFinite(adfMinMax[0]) && (!CPLIsNan(adfMinMax[1])) && CPLIsFinite(adfMinMax[1]))
				{
					// only write a range if we got a correct one from the source dataset (otherwise ILWIS can't show the map properly)
					char strdouble[45];
					sprintf(strdouble, "%.3f:%.3f:%3f:offset=0", adfMinMax[0], adfMinMax[1],stepsize);
					string range = string(strdouble);
					WriteElement("BaseMap", "Range", pszODFName, range);
				}
        WriteElement("Map", "GeoRef", pszODFName, georef);
				
/* -------------------------------------------------------------------- */
/*      Loop over image, copy the image data.                           */
/* -------------------------------------------------------------------- */
        CPLErr      eErr = CE_None;
			
        //For file name for raw data, and create binary files. 
        string pszDataFileName = CPLResetExtension(pszODFName.c_str(), "mp#" );
				
        fpData = desBand->fpRaw;
        if( fpData == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Attempt to create file `%s' failed.\n",
                      pszFilename );
            return NULL;
        }

        for( int iLine = 0; iLine < nYSize && eErr == CE_None; iLine++ )
        {
            eErr = poBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                                     pData, nXSize, 1, eType, 
                                     0, 0 );

            if( eErr == CE_None )
            {
                //Translater the NoDataValue from each band to ILWIS
                for (int iCol = 0; iCol < nXSize; iCol++ )
                {
                    if( EQUAL(sStoreType.c_str(),"Byte"))
                    {
                        if ( pbSuccess && ((GByte * )pData)[iCol] == dNoDataValue )
                            (( GByte * )pData)[iCol] = 0;
                    }
                    else if( EQUAL(sStoreType.c_str(),"Int"))	
                    {
                        if ( pbSuccess && ((GInt16 * )pData)[iCol] == dNoDataValue )
                            (( GInt16 * )pData)[iCol] = shUNDEF;
                    }
                    else if( EQUAL(sStoreType.c_str(),"Long"))	
                    {
                        if ( pbSuccess && ((GInt32 * )pData)[iCol] == dNoDataValue )
                            (( GInt32 * )pData)[iCol] = iUNDEF;
                    }
                    else if( EQUAL(sStoreType.c_str(),"float"))	
                    {
                        float fNoDataValue = (float)dNoDataValue; // needed for comparing for NoDataValue
                        if (( pbSuccess && ((float * )pData)[iCol] == fNoDataValue ) || (CPLIsNan((( float * )pData)[iCol])))
                            (( float * )pData)[iCol] = flUNDEF;
                    }
                    else if( EQUAL(sStoreType.c_str(),"Real"))	
                    {
                        if (( pbSuccess && ((double * )pData)[iCol] == dNoDataValue ) || (CPLIsNan((( double * )pData)[iCol])))
                                (( double * )pData)[iCol] = rUNDEF;
                    }
                }
                int iSize = VSIFWrite( pData, 1, nLineSize, desBand->fpRaw );
                if ( iSize < 1 )
                {
                    CPLFree( pData );
                    //CPLFree( pData32 );
                    CPLError( CE_Failure, CPLE_FileIO, 
                              "Write of file failed with fwrite error.");
                    return NULL;
                }
            }
            if( !pfnProgress(iLine / (nYSize * nBands), NULL, pProgressData ) )
                return NULL;
        }
        CPLFree( pData );
		}

    poDS->FlushCache();

    if( !pfnProgress( 1.0, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt,
                  "User terminated" );
        delete poDS;

        GDALDriver *poILWISDriver =
            (GDALDriver *) GDALGetDriverByName( "ILWIS" );
        poILWISDriver->Delete( pszFilename );
        return NULL;
    }

    poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );

    return poDS;
}

/************************************************************************/
/*                       ILWISRasterBand()                              */
/************************************************************************/

ILWISRasterBand::ILWISRasterBand( ILWISDataset *poDS, int nBand )

{
    string sBandName;
    if ( EQUAL(poDS->pszFileType.c_str(),"Map"))  		
        sBandName = string(poDS->pszFileName);
    else //map list
    {
        //Form the band name
        char cBandName[45];
        sprintf( cBandName, "Map%d", nBand-1);
        sBandName = ReadElement("MapList", string(cBandName), string(poDS->pszFileName));
        string sInputPath = string(CPLStrdup(CPLGetPath( poDS->pszFileName)));	
        string sBandPath = string(CPLStrdup(CPLGetPath( sBandName.c_str())));
        string sBandBaseName = string(CPLStrdup(CPLGetBasename( sBandName.c_str())));
        if ( 0==sBandPath.length() )
            sBandName = string(CPLFormFilename(sInputPath.c_str(),sBandBaseName.c_str(),"mpr" ));		
        else
            sBandName = string(CPLFormFilename(sBandPath.c_str(),sBandBaseName.c_str(),"mpr" ));		
    }

		if (poDS->bNewDataset)  //for Create() function
		{
      GetStoreType(sBandName, psInfo.stStoreType);
			eDataType = ILWIS2GDALType(psInfo.stStoreType);
		}					
		else
			GetILWISInfo(sBandName);
    this->poDS = poDS;
    this->nBand = nBand;
    nBlockXSize = poDS->GetRasterXSize();
		nBlockYSize = 1;
    switch (psInfo.stStoreType)
    {
      case stByte: 
        nSizePerPixel = GDALGetDataTypeSize(GDT_Byte) / 8;
        break;
      case stInt:
        nSizePerPixel = GDALGetDataTypeSize(GDT_Int16) / 8;
        break;
      case stLong:
        nSizePerPixel = GDALGetDataTypeSize(GDT_Int32) / 8;
        break;
      case stFloat:
        nSizePerPixel = GDALGetDataTypeSize(GDT_Float32) / 8;
        break;
      case stReal:
        nSizePerPixel = GDALGetDataTypeSize(GDT_Float64) / 8;
        break;
    }
    ILWISOpen(sBandName);
}

/************************************************************************/
/*                             ILWISOpen()                             */
/************************************************************************/
void ILWISRasterBand::ILWISOpen(  string pszFileName)
{
    string pszDataFile;
    pszDataFile = string(CPLResetExtension( pszFileName.c_str(), "mp#" ));
    //both for reading and writing, the file must exist

#ifdef WIN32
    if (_access(pszDataFile.c_str(), 2) == 0)
#else
    if (access(pszDataFile.c_str(), 2) == 0)
#endif
        fpRaw = VSIFOpen( pszDataFile.c_str(), "rb+");
    else
        fpRaw = VSIFOpen( pszDataFile.c_str(), "rb");
}

/************************************************************************/
/*                       GetILWISInfo()                                 */
/************************************************************************/
CPLErr ILWISRasterBand::GetILWISInfo(string pszFileName)
{
    string domName = ReadElement("BaseMap", "Domain", pszFileName.c_str());
    string pszBaseName = string(CPLStrdup( CPLGetBasename( domName.c_str()) ));
    transform(pszBaseName.begin(), pszBaseName.end(), pszBaseName.begin(), tolower);
    string pszPath = string(CPLStrdup( CPLGetPath( pszFileName.c_str()) ));
		
    if (GetStoreType(pszFileName, psInfo.stStoreType) != CE_None)
    {
        return CE_Failure;
    }
    psInfo.bValue = false;
    psInfo.stDomain = "";

    if( EQUAL(pszBaseName.c_str(),"value") 
        || EQUAL(pszBaseName.c_str(),"count") 
        || EQUAL(pszBaseName.c_str(),"distance") 
        || EQUAL(pszBaseName.c_str(),"min1to1") 
        || EQUAL(pszBaseName.c_str(),"noaa") 
        || EQUAL(pszBaseName.c_str(),"perc") 
        || EQUAL(pszBaseName.c_str(),"radar") )
    {
        if (psInfo.stStoreType == stFloat)
          eDataType = GDT_Float32;
        else
          eDataType = GDT_Float64;
        psInfo.bValue = true;
    }
    else if( EQUAL(pszBaseName.c_str(),"bool") 
             || EQUAL(pszBaseName.c_str(),"byte") 
             || EQUAL(pszBaseName.c_str(),"image") 
             || EQUAL(pszBaseName.c_str(),"colorcmp") 
             || EQUAL(pszBaseName.c_str(),"flowdirection") 
             || EQUAL(pszBaseName.c_str(),"yesno") )
    {
        eDataType = GDT_Byte;
        if( EQUAL(pszBaseName.c_str(),"image") 
            || EQUAL(pszBaseName.c_str(),"colorcmp"))
            psInfo.stDomain = pszBaseName;
    }
    else if( EQUAL(pszBaseName.c_str(),"color") 
             || EQUAL(pszBaseName.c_str(),"none" )
             || EQUAL(pszBaseName.c_str(),"coordbuf") 
             || EQUAL(pszBaseName.c_str(),"binary") 
             || EQUAL(pszBaseName.c_str(),"string") )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unsupported ILWIS domain type.");
        return CE_Failure;		
    }
    else
    {
        string pszDomainName;
        pszDomainName = string(CPLFormFilename(pszPath.c_str(),pszBaseName.c_str(),"dom" ));
				
        string domType = ReadElement("Domain", "Type", pszDomainName.c_str());
        transform(domType.begin(), domType.end(), domType.begin(), tolower);
        if EQUAL(domType.c_str(),"domainvalue")  
        {
            if (psInfo.stStoreType == stFloat)
              eDataType = GDT_Float32;
            else
              eDataType = GDT_Float64;
            psInfo.bValue = true; 
        }
        else if((!EQUAL(domType.c_str(),"domainbit")) 
                && (!EQUAL(domType.c_str(),"domainstring")) 
                && (!EQUAL(domType.c_str(),"domaincolor"))
                && (!EQUAL(domType.c_str(),"domainbinary")) 
                && (!EQUAL(domType.c_str(),"domaincoordBuf")) 
                && (!EQUAL(domType.c_str(),"domaincoord")))
        {
            eDataType = ILWIS2GDALType(psInfo.stStoreType);   
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Unsupported ILWIS domain type.");
            return CE_Failure;		
        }
    }
		
    if (psInfo.bValue)
    {
        string rangeString = ReadElement("BaseMap", "Range", pszFileName.c_str());
        psInfo.vr = ValueRange(rangeString);
    }
    return CE_None;
}

/** This driver defines a Block to be the entire raster; The method reads
    each line as a block. it reads the data into pImage.  

    @param nBlockXOff This must be zero for this driver
    @param pImage Dump the data here

    @return A CPLErr code. This implementation returns a CE_Failure if the
    block offsets are non-zero, If successful, returns CE_None. */
/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/
CPLErr ILWISRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
	  char * pBuffer;
		
    // If the x block offsets is non-zero, something is wrong.
    CPLAssert( nBlockXOff == 0 );
		
    int nBlockSize =  nBlockXSize * nBlockYSize * nSizePerPixel;
		if( fpRaw == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to open ILWIS data file.");  
        return( CE_Failure );
    }
    


/* -------------------------------------------------------------------- */
/*	Handle the case of a strip in a writable file that doesn't	*/
/*	exist yet, but that we want to read.  Just set to zeros and	*/
/*	return.								*/
/* -------------------------------------------------------------------- */
		ILWISDataset* poIDS = (ILWISDataset*) poDS;
    if( poIDS->bNewDataset && (poIDS->eAccess == GA_Update))
    {
      FillWithNoData(pImage);
      return CE_None;
    }

		VSIFSeek( fpRaw, nBlockSize*nBlockYOff, SEEK_SET );
    pBuffer = (char *)CPLMalloc(nBlockSize);
    if (VSIFRead( pBuffer, 1, nBlockSize, fpRaw ) < 1)
    {
        CPLFree( pBuffer );
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Read of file failed with fread error.");
        return CE_Failure;
    }

    switch (psInfo.stStoreType)
    {
        int i;
      case stByte:
        for( i = 0; i < nBlockXSize; i++ )
        {
            if (psInfo.bValue)
                ((double *) pImage)[i] = psInfo.vr.rValue( (GByte)pBuffer[i]);
            else
                ((GByte *)pImage)[i] = (GByte)pBuffer[i];

        }
        break;
      case stInt:
        for( i = 0; i < nBlockXSize; i++ )
        {
            if (psInfo.bValue)
                ((double *) pImage)[i] = psInfo.vr.rValue( ((GInt16 *) pBuffer)[i]);
            else
                ((GInt16 *) pImage)[i] = ((GInt16 *) pBuffer)[i];
        }
        break;
      case stLong:
        for( i = 0; i < nBlockXSize; i++ )
            if (psInfo.bValue)
                ((double *) pImage)[i] = psInfo.vr.rValue( ((GInt32 *) pBuffer)[i]);
            else
                ((GInt32 *) pImage)[i] = ((GInt32 *) pBuffer)[i];
        break;
      case stFloat:
        for( i = 0; i < nBlockXSize; i++ )
            ((float *) pImage)[i] = ((float *) pBuffer)[i];
        break;  
      case stReal:
        for( i = 0; i < nBlockXSize; i++ )
            ((double *) pImage)[i] = ((double *) pBuffer)[i];
        break;
    }

    CPLFree( pBuffer );

    return CE_None;
}

void ILWISRasterBand::FillWithNoData(void * pImage)
{
    if (psInfo.stStoreType == stByte)
      memset(pImage, 0, nBlockXSize * nBlockYSize);
    else
    {
      switch (psInfo.stStoreType)
      {
        case stInt:
          ((GInt16*)pImage)[0] = shUNDEF;
          break;
        case stLong:
          ((GInt32*)pImage)[0] = iUNDEF;
          break;
        case stFloat:
          ((float*)pImage)[0] = flUNDEF;
          break;
        case stReal:
          ((double*)pImage)[0] = rUNDEF;
          break;
        default: // should there be handling for stByte? 
          break;
      }
      int iItemSize = GDALGetDataTypeSize(eDataType) / 8;
      for (int i = 1; i < nBlockXSize * nBlockYSize; ++i)
        memcpy( ((char*)pImage) + iItemSize * i, (char*)pImage + iItemSize * (i - 1), iItemSize);
    }
}

/************************************************************************/
/*                            IWriteBlock()                             */
/*                                                                      */
/************************************************************************/

CPLErr ILWISRasterBand::IWriteBlock(int nBlockXOff, int nBlockYOff,
				   void* pImage) {

    ILWISDataset* dataset = (ILWISDataset*) poDS;

    CPLAssert( dataset != NULL
               && nBlockXOff == 0
               && nBlockYOff >= 0
               && pImage != NULL );

    CPLErr eErr = CE_None;
    int nXSize = dataset->GetRasterXSize();
    int nBlockSize = nBlockXSize * nBlockYSize * nSizePerPixel;
    void *pData;
    pData = CPLMalloc(nBlockSize);
    
		VSIFSeek( fpRaw, nBlockSize * nBlockYOff, SEEK_SET );

    bool fDataExists = (VSIFRead( pData, 1, nBlockSize, fpRaw ) >= 1);

    if( eErr == CE_None )
    {
        //Translater the NoDataValue per band to ILWIS
        for (int iCol = 0; iCol < nXSize; iCol++ )
        {
            switch (psInfo.stStoreType)
            {
              case stByte:
                if (fDataExists)
                {
                  if ((( GByte * )pData)[iCol] == 0)
                  {
                    (( GByte * )pData)[iCol] = ((GByte* )pImage)[iCol];
                  } // else do not overwrite the existing data in pData
                }
                else
                {
                  (( GByte * )pData)[iCol] = ((GByte * )pImage)[iCol];
                }
                break;
              case stInt:
                if (fDataExists)
                {
                  if ((( GInt16 * )pData)[iCol] == shUNDEF)
                  {
                    (( GInt16 * )pData)[iCol] = ((GInt16* )pImage)[iCol];
                  } // else do not overwrite the existing data in pData
                }
                else
                {
                  (( GInt16 * )pData)[iCol] = ((GInt16* )pImage)[iCol];
                }
                break;
              case stLong:
                if (fDataExists)
                {
                  if ((( GInt32 * )pData)[iCol] == shUNDEF)
                  {
                    (( GInt32 * )pData)[iCol] = ((GInt32* )pImage)[iCol];
                  } // else do not overwrite the existing data in pData
                }
                else
                {
                  (( GInt32 * )pData)[iCol] = ((GInt32* )pImage)[iCol];
                }
                break;
              case stFloat:
                if (fDataExists)
                {
                  if ((( float * )pData)[iCol] == flUNDEF)
                  {
                      (( float * )pData)[iCol] = ((float* )pImage)[iCol];
                  }
                    // else do not overwrite the existing data in pData
                }
                else
                {  
                      (( float * )pData)[iCol] = ((float* )pImage)[iCol];
                }
                break;
              case stReal:
                if (fDataExists)
                {
                  if ((( double * )pData)[iCol] == rUNDEF)
                  {
                      (( double * )pData)[iCol] = ((double* )pImage)[iCol];
                  }
                    // else do not overwrite the existing data in pData
                }
                else
                {  
                      (( double * )pData)[iCol] = ((double* )pImage)[iCol];
                }
                break;
            }
        }

				VSIFSeek( fpRaw, nBlockSize * nBlockYOff, SEEK_SET );

        if (VSIFWrite( pData, 1, nBlockSize, fpRaw ) < 1)
        {
            CPLFree( pData );
            CPLError( CE_Failure, CPLE_FileIO, 
                      "Write of file failed with fwrite error.");
            return CE_Failure;
        }
    }
    CPLFree( pData );
    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/
double ILWISRasterBand::GetNoDataValue( int *pbSuccess )

{
    if( pbSuccess != NULL )
        *pbSuccess = TRUE;

    if( eDataType == GDT_Float64 )
        return rUNDEF;
    else if( eDataType == GDT_Int32)
        return iUNDEF;
    else if( eDataType == GDT_Int16)
        return shUNDEF;
    else if( eDataType == GDT_Float32)
        return flUNDEF;
    else if( EQUAL(psInfo.stDomain.c_str(),"image") 
             || EQUAL(psInfo.stDomain.c_str(),"colorcmp")) 
    {
        *pbSuccess = false;
        return 0;
    }
    else
        return 0;
}

/************************************************************************/
/*                      ValueRange()                                    */
/************************************************************************/
double Max(double a, double b) 
{ return (a>=b && a!=rUNDEF) ? a : b; }

static double doubleConv(const char* s)
{
    if (s == 0) return rUNDEF;
    char *endptr;
    char *begin = const_cast<char*>(s);

    // skip leading spaces; strtol will return 0 on a string with only spaces
    // which is not what we want
    while (isspace(*begin)) ++begin;

    if (strlen(begin) == 0) return rUNDEF;
    errno = 0;
    double r = strtod(begin, &endptr);
    if ((0 == *endptr) && (errno==0))
        return r;
    while (*endptr != 0) { // check trailing spaces
        if (*endptr != ' ')
            return rUNDEF;
        endptr++;
    }
    return r;
}

ValueRange::ValueRange(string sRng)
{
    char* sRange = new char[sRng.length() + 1];
    for (unsigned int i = 0; i < sRng.length(); ++i)
        sRange[i] = sRng[i];
    sRange[sRng.length()] = 0;
	
    char *p1 = strchr(sRange, ':');
    if (0 == p1)
        return;

    char *p3 = strstr(sRange, ",offset=");
    if (0 == p3)
        p3 = strstr(sRange, ":offset=");
    _r0 = rUNDEF;
    if (0 != p3) {
        _r0 = doubleConv(p3+8);
        *p3 = 0;
    }
    char *p2 = strrchr(sRange, ':');
    _rStep = 1;
    if (p1 != p2) { // step
        _rStep = doubleConv(p2+1);
        *p2 = 0;
    }

    p2 = strchr(sRange, ':');
    if (p2 != 0) {
        *p2 = 0;
        _rLo = atof(sRange);
        _rHi = atof(p2+1);
    }
    else {
        _rLo = atof(sRange);
        _rHi = _rLo;
    }  
    init(_r0);
	 
    delete [] sRange;
}

ValueRange::ValueRange(double min, double max)	// step = 1
{
    _rLo = min;
    _rHi = max;
    _rStep = 1;
    init();
}

ValueRange::ValueRange(double min, double max, double step)	
{
    _rLo = min;
    _rHi = max;
    _rStep = step;
    init();
}

static ilwisStoreType stNeeded(unsigned long iNr)
{
    if (iNr <= 256)
        return stByte;
    if (iNr <= SHRT_MAX)
        return stInt;
    return stLong;
}

void ValueRange::init()
{
    init(rUNDEF);
}

void ValueRange::init(double rRaw0)
{
    try {
        _iDec = 0;
        if (_rStep < 0)
            _rStep = 0;
        double r = _rStep;
        if (r <= 1e-20)
            _iDec = 3;
        else while (r - floor(r) > 1e-20) {
            r *= 10;
            _iDec++;
            if (_iDec > 10)
                break;
        }

        short iBeforeDec = 1;
        double rMax = MAX(fabs(get_rLo()), fabs(get_rHi()));
        if (rMax != 0)
            iBeforeDec = (short)floor(log10(rMax)) + 1;
        if (get_rLo() < 0)
            iBeforeDec++;
        _iWidth = iBeforeDec + _iDec;
        if (_iDec > 0)
            _iWidth++;
        if (_iWidth > 12)
            _iWidth = 12;
        if (_rStep < 1e-06)
        {
            st = stReal;
            _rStep = 0;
        }			
        else {
            r = get_rHi() - get_rLo();
            if (r <= ULONG_MAX) {
                r /= _rStep;
                r += 1;
            }
            r += 1; 
            if (r > LONG_MAX)
                st = stReal;
            else {
                st = stNeeded((unsigned long)floor(r+0.5));
                if (st < stByte)
                    st = stByte;
            }
        }
        if (rUNDEF != rRaw0)
            _r0 = rRaw0;
        else {
            _r0 = 0;
            if (st <= stByte)
                _r0 = -1;
        }
        if (st > stInt)
            iRawUndef = iUNDEF;
        else if (st == stInt)
            iRawUndef = shUNDEF;
        else
            iRawUndef = 0;
    }
    catch (std::exception*) {
        st = stReal;
        _r0 = 0;
        _rStep = 0.0001;
        _rHi = 1e300;
        _rLo = -1e300;
        iRawUndef = iUNDEF;
    }
}

string ValueRange::ToString()
{
    char buffer[200];
    if (fabs(get_rLo()) > 1.0e20 || fabs(get_rHi()) > 1.0e20)
        sprintf(buffer, "%g:%g:%f:offset=%g", get_rLo(), get_rHi(), get_rStep(), get_rRaw0());
    else if (get_iDec() >= 0)
        sprintf(buffer, "%.*f:%.*f:%.*f:offset=%.0f", get_iDec(), get_rLo(), get_iDec(), get_rHi(), get_iDec(), get_rStep(), get_rRaw0());
    else
        sprintf(buffer, "%f:%f:%f:offset=%.0f", get_rLo(), get_rHi(), get_rStep(), get_rRaw0());
    return string(buffer);
}

double ValueRange::rValue(int iRaw)
{
    if (iRaw == iUNDEF || iRaw == iRawUndef)
        return rUNDEF;
    double rVal = iRaw + _r0;
    rVal *= _rStep;
    if (get_rLo() == get_rHi())
        return rVal;
    double rEpsilon = _rStep == 0.0 ? 1e-6 : _rStep / 3.0; // avoid any rounding problems with an epsilon directly based on the
    // the stepsize
    if ((rVal - get_rLo() < -rEpsilon) || (rVal - get_rHi() > rEpsilon))
        return rUNDEF;
    return rVal;
}

int ValueRange::iRaw(double rValue)
{
    if (rValue == rUNDEF) // || !fContains(rValue))
        return iUNDEF;
    double rEpsilon = _rStep == 0.0 ? 1e-6 : _rStep / 3.0;	
    if (rValue - get_rLo() < -rEpsilon) // take a little rounding tolerance
        return iUNDEF;
    else if (rValue - get_rHi() > rEpsilon) // take a little rounding tolerance
        return iUNDEF;
    rValue /= _rStep;
    double rVal = floor(rValue+0.5);
    rVal -= _r0;
    long iVal;
    iVal = longConv(rVal);
    return iVal;
}


/************************************************************************/
/*                    GDALRegister_ILWIS()                              */
/************************************************************************/

void GDALRegister_ILWIS()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "ILWIS" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "ILWIS" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "ILWIS Raster Map" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "mpr/mpl" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte Int16 Int32 Float64" );

        poDriver->pfnOpen = ILWISDataset::Open;
        poDriver->pfnCreate = ILWISDataset::Create;
        poDriver->pfnCreateCopy = ILWISDataset::CreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
