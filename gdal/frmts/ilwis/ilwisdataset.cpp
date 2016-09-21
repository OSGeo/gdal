/******************************************************************************
 *
 * Project:  ILWIS Driver
 * Purpose:  GDALDataset driver for ILWIS translator for read/write support.
 * Author:   Lichun Wang, lichun@itc.nl
 *
 ******************************************************************************
 * Copyright (c) 2004, ITC
 * Copyright (c) 2008-2010, Even Rouault <even dot rouault at mines-paris dot org>
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


#include "ilwisdataset.h"
#include <cfloat>
#include <climits>

#include <string>

#include "gdal_frmts.h"

using std::string;

/* used by ilwsicoordinatesystem.cpp */
string ReadElement(string section, string entry, string filename);
bool WriteElement(string sSection, string sEntry, string fn, string sValue);
bool WriteElement(string sSection, string sEntry, string fn, int nValue);
bool WriteElement(string sSection, string sEntry, string fn, double dValue);

// IniFile.cpp: implementation of the IniFile class.
//
//////////////////////////////////////////////////////////////////////
bool CompareAsNum::operator() (const string& s1, const string& s2) const
{
    long Num1 = atoi(s1.c_str());
    long Num2 = atoi(s2.c_str());
    return Num1 < Num2;
}

static string TrimSpaces(const string& input)
{
    // find first non space
    if ( input.empty() )
        return string();

    const size_t iFirstNonSpace = input.find_first_not_of(' ');
    const size_t iFindLastSpace = input.find_last_not_of(' ');
    if (iFirstNonSpace == string::npos || iFindLastSpace == string::npos)
        return string();

    return input.substr(iFirstNonSpace, iFindLastSpace - iFirstNonSpace + 1);
}

static string GetLine(VSILFILE* fil)
{
    const char *p = CPLReadLineL( fil );
    if (p == NULL)
        return string();

    CPLString osWrk = p;
    osWrk.Trim();
    return string(osWrk);
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
IniFile::IniFile(const string& filenam)
{
    filename = filenam;
    Load();
    bChanged = false; // Start tracking changes
}

IniFile::~IniFile()
{
    if (bChanged)
    {
        Store();
        bChanged = false;
    }

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
    bChanged = true;
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
        bChanged = true;
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
        bChanged = true;
    }
}

void IniFile::Load()
{
    VSILFILE *filIni = VSIFOpenL(filename.c_str(), "r");
    if (filIni == NULL)
        return;

    string section, key, value;
    enum ParseState { FindSection, FindKey, ReadFindKey, StoreKey, None } state
        = FindSection;
    string s;
    while (!VSIFEofL(filIni) || !s.empty() )
    {
        switch (state)
        {
          case FindSection:
            s = GetLine(filIni);
            if (s.empty())
                continue;

            if (s[0] == '[')
            {
                size_t iLast = s.find_first_of(']');
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
              size_t iEqu = s.find_first_of('=');
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

    VSIFCloseL(filIni);
}

void IniFile::Store()
{
    VSILFILE *filIni = VSIFOpenL(filename.c_str(), "w+");
    if (filIni == NULL)
        return;

    Sections::iterator iterSect;
    for (iterSect = sections.begin(); iterSect != sections.end(); ++iterSect)
    {
        CPLString osLine;

        // write the section name
        osLine.Printf( "[%s]\r\n", (*iterSect).first.c_str());
        VSIFWriteL( osLine.c_str(), 1, strlen(osLine), filIni );
        SectionEntries *entries = (*iterSect).second;
        SectionEntries::iterator iterEntry;
        for (iterEntry = (*entries).begin(); iterEntry != (*entries).end(); ++iterEntry)
        {
            string key = (*iterEntry).first;
            osLine.Printf( "%s=%s\r\n",
                           TrimSpaces(key).c_str(), (*iterEntry).second.c_str());
            VSIFWriteL( osLine.c_str(), 1, strlen(osLine), filIni );
        }

        VSIFWriteL( "\r\n", 1, 2, filIni );
    }

    VSIFCloseL( filIni );
}

// End of the implementation of IniFile class. ///////////////////////
//////////////////////////////////////////////////////////////////////

static long longConv(double x) {
    if ((x == rUNDEF) || (x > LONG_MAX) || (x < LONG_MIN))
        return iUNDEF;

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

    IniFile MyIniFile (filename);

    return MyIniFile.GetKeyValue(section, entry);;
}

bool WriteElement(string sSection, string sEntry,
                             string fn, string sValue)
{
    if (0 == fn.length())
        return false;

    IniFile MyIniFile (fn);

    MyIniFile.SetKeyValue(sSection, sEntry, sValue);
    return true;
}

bool WriteElement(string sSection, string sEntry,
                             string fn, int nValue)
{
    if (0 == fn.length())
        return false;

    char strdouble[45];
    snprintf(strdouble, sizeof(strdouble), "%d", nValue);
    string sValue = string(strdouble);
    return WriteElement(sSection, sEntry, fn, sValue);
}

bool WriteElement(string sSection, string sEntry,
                             string fn, double dValue)
{
    if (0 == fn.length())
        return false;

    char strdouble[45];
    CPLsnprintf(strdouble, sizeof(strdouble), "%.6f", dValue);
    string sValue = string(strdouble);
    return WriteElement(sSection, sEntry, fn, sValue);
}

static CPLErr GetRowCol(string str,int &Row, int &Col)
{
    string delimStr = " ,;";
    size_t iPos = str.find_first_of(delimStr);
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
    string sStoreType = "";
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


ILWISDataset::ILWISDataset() :
    bGeoDirty(FALSE),
    bNewDataset(FALSE)
{
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
        georef = ReadElement("Map", "GeoRef", osFileName);
    else
        georef = ReadElement("MapList", "GeoRef", osFileName);

    //Capture the geotransform, only if the georef is not 'none',
    //otherwise, the default transform should be returned.
    if( (georef.length() != 0) && !EQUAL(georef.c_str(),"none"))
    {
        //Form the geo-referencing name
        string pszBaseName = string(CPLGetBasename(georef.c_str()) );
        string pszPath = string(CPLGetPath( osFileName ));
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
            double deltaX = CPLAtof(sMaxX.c_str()) - CPLAtof(sMinX.c_str());
            double deltaY = CPLAtof(sMaxY.c_str()) - CPLAtof(sMinY.c_str());

            double PixelSizeX = deltaX / (double)nRasterXSize;
            double PixelSizeY = deltaY / (double)nRasterYSize;

            if (EQUAL(IsCorner.c_str(),"Yes"))
            {
                adfGeoTransform[0] = CPLAtof(sMinX.c_str());
                adfGeoTransform[3] = CPLAtof(sMaxY.c_str());
            }
            else
            {
                adfGeoTransform[0] = CPLAtof(sMinX.c_str()) - PixelSizeX/2.0;
                adfGeoTransform[3] = CPLAtof(sMaxY.c_str()) + PixelSizeY/2.0;
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
    // Check whether we should write out a georeference file.
    // Dataset must be north up.
    if( adfGeoTransform[0] != 0.0 || adfGeoTransform[1] != 1.0
        || adfGeoTransform[2] != 0.0 || adfGeoTransform[3] != 0.0
        || adfGeoTransform[4] != 0.0 || fabs(adfGeoTransform[5]) != 1.0 )
    {
        SetGeoTransform( adfGeoTransform ); // is this needed?
        if (adfGeoTransform[2] == 0.0 && adfGeoTransform[4] == 0.0)
        {
            int   nXSize = GetRasterXSize();
            int   nYSize = GetRasterYSize();
            double dLLLat = (adfGeoTransform[3]
                      + nYSize * adfGeoTransform[5] );
            double dLLLong = (adfGeoTransform[0] );
            double dURLat  = (adfGeoTransform[3] );
            double dURLong = (adfGeoTransform[0]
                       + nXSize * adfGeoTransform[1] );

            string grFileName = CPLResetExtension(osFileName, "grf" );
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
            string sBaseName = string(CPLGetBasename(osFileName) );
            string sPath = string(CPLGetPath(osFileName));
            if (nBands == 1)
            {
                WriteElement("Map", "GeoRef", osFileName, sBaseName + ".grf");
            }
            else
            {
                for( int iBand = 0; iBand < nBands; iBand++ )
                {
                    if (iBand == 0)
                      WriteElement("MapList", "GeoRef", osFileName, sBaseName + ".grf");
                    char szName[100];
                    snprintf(szName, sizeof(szName), "%s_band_%d", sBaseName.c_str(),iBand + 1 );
                    string pszODFName = string(CPLFormFilename(sPath.c_str(),szName,"mpr"));
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
    memmove( adfGeoTransform, padfTransform, sizeof(double)*6 );

    if (adfGeoTransform[2] == 0.0 && adfGeoTransform[4] == 0.0)
        bGeoDirty = TRUE;

    return CE_None;
}

static bool CheckASCII(unsigned char * buf, int size)
{
	for (int i = 0; i < size; ++i)
        {
            if (!isascii(buf[i]))
                return false;
        }

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
    if( poOpenInfo->nHeaderBytes < 1 )
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
    const string maptype = ReadElement("BaseMap", "Type", poOpenInfo->pszFilename);
    const string sBaseName = string(CPLGetBasename(poOpenInfo->pszFilename) );
    const string sPath = string(CPLGetPath( poOpenInfo->pszFilename));

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
            snprintf( cBandName, sizeof(cBandName), "Map%d", iBand);
            string sBandName = ReadElement("MapList", string(cBandName), poOpenInfo->pszFilename);
            string pszBandBaseName = string(CPLGetBasename(sBandName.c_str()) );
            string pszBandPath = string(CPLGetPath( sBandName.c_str()));
            if ( 0 == pszBandPath.length() )
            {
                sBandName = string(CPLFormFilename(sPath.c_str(),
                                                   pszBandBaseName.c_str(),"mpr" ));
            }
            // Verify the file extension, it must be an ILWIS raw data file
            // with extension .mp#, otherwise, unsupported
            // This drive only supports a map list which stores a set
            // of ILWIS raster maps,
            string sMapStoreName = ReadElement("MapStore", "Data", sBandName);
            sExt = CPLGetExtension( sMapStoreName.c_str() );
            if ( !STARTS_WITH_CI(sExt.c_str(), "mp#"))
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Unsupported ILWIS data file. \n"
                          "can't treat as raster.\n" );
                return NULL;
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
            return NULL;
        }
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unsupported ILWIS data file. \n"
                  "can't treat as raster.\n" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    ILWISDataset *poDS = new ILWISDataset();

/* -------------------------------------------------------------------- */
/*      Capture raster size from ILWIS file (.mpr).                     */
/* -------------------------------------------------------------------- */
    int Row = 0, Col = 0;
    if ( GetRowCol(mapsize, Row, Col) != CE_None)
    {
        delete poDS;
        return NULL;
    }
    if( !GDALCheckDatasetDimensions(Col, Row) )
    {
        delete poDS;
        return NULL;
    }
    poDS->nRasterXSize = Col;
    poDS->nRasterYSize = Row;
    poDS->osFileName = poOpenInfo->pszFilename;
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
        string pszProj;

        if( (csy.length() != 0) && !EQUAL(csy.c_str(),"unknown.csy"))
        {

            //Form the coordinate system file name
            if( !(STARTS_WITH_CI(csy.c_str(), "latlon.csy")) &&
                !(STARTS_WITH_CI(csy.c_str(), "LatlonWGS84.csy")))
            {
                string pszBaseName = string(CPLGetBasename(csy.c_str()) );
                string pszPath = string(CPLGetPath( poDS->osFileName ));
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

            if( (STARTS_WITH_CI(pszProj.c_str(), "LatLon")) ||
                (STARTS_WITH_CI(pszProj.c_str(), "Projection")))
                poDS->ReadProjection( csy );
        }
    }

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
                                  CPL_UNUSED char** papszParmList)
{
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
/*	Determine store type of ILWIS raster                            */
/* -------------------------------------------------------------------- */
    string sDomain= "value.dom";
    double stepsize = 1;
    string sStoreType = GDALType2ILWIS(eType);
    if( EQUAL(sStoreType.c_str(),""))
        return NULL;
    else if( EQUAL(sStoreType.c_str(),"Real") || EQUAL(sStoreType.c_str(),"float"))
        stepsize = 0;

    const string pszBaseName = string(CPLGetBasename( pszFilename ));
    const string pszPath = string(CPLGetPath( pszFilename ));

/* -------------------------------------------------------------------- */
/*      Write out object definition file for each band                  */
/* -------------------------------------------------------------------- */
    string pszODFName;
    string pszDataBaseName;
    string pszFileName;

    char strsize[45];
    snprintf(strsize, sizeof(strsize), "%d %d", nYSize, nXSize);

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

    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        if ( nBands > 1 )
        {
            char szBandName[100];
            snprintf(szBandName, sizeof(szBandName), "%s_band_%d", pszBaseName.c_str(),iBand + 1 );
            pszODFName = string(szBandName) + ".mpr";
            pszDataBaseName = string(szBandName);
            snprintf(szBandName, sizeof(szBandName), "Map%d", iBand);
            WriteElement("MapList", string(szBandName), string(pszFileName), pszODFName);
            pszODFName = CPLFormFilename(pszPath.c_str(),pszDataBaseName.c_str(),"mpr");
        }
/* -------------------------------------------------------------------- */
/*      Write data definition per band (.mpr)                           */
/* -------------------------------------------------------------------- */

        WriteElement("Ilwis", "Type", pszODFName, "BaseMap");
        WriteElement("BaseMap", "Type", pszODFName, "Map");
        WriteElement("Map", "Type", pszODFName, "MapStore");

        WriteElement("BaseMap", "Domain", pszODFName, sDomain);
        string pszDataName = pszDataBaseName + ".mp#";
        WriteElement("MapStore", "Data", pszODFName, pszDataName);
        WriteElement("MapStore", "Structure", pszODFName, "Line");
        // sStoreType is used by ILWISRasterBand constructor to determine eDataType
        WriteElement("MapStore", "Type", pszODFName, sStoreType);

        // For now write-out a "Range" that is as broad as possible.
        // If later a better range is found (by inspecting metadata in the source dataset),
        // the "Range" will be overwritten by a better version.
        double adfMinMax[2] = {-9999999.9, 9999999.9};
        char strdouble[45];
        CPLsnprintf(strdouble, sizeof(strdouble), "%.3f:%.3f:%3f:offset=0", adfMinMax[0], adfMinMax[1],stepsize);
        string range(strdouble);
        WriteElement("BaseMap", "Range", pszODFName, range);

        WriteElement("Map", "GeoRef", pszODFName, "none.grf");
        WriteElement("Map", "Size", pszODFName, string(strsize));

/* -------------------------------------------------------------------- */
/*      Try to create the data file.                                    */
/* -------------------------------------------------------------------- */
        pszDataName = CPLResetExtension(pszODFName.c_str(), "mp#" );

        VSILFILE  *fp = VSIFOpenL( pszDataName.c_str(), "wb" );

        if( fp == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Unable to create file %s.\n", pszDataName.c_str() );
            return NULL;
        }
        VSIFCloseL( fp );
    }
    ILWISDataset *poDS = new ILWISDataset();
    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->nBands = nBands;
    poDS->eAccess = GA_Update;
    poDS->bNewDataset = TRUE;
    poDS->SetDescription(pszFilename);
    poDS->osFileName = pszFileName;
    poDS->pszIlwFileName = string(pszFileName);
    if ( nBands == 1 )
        poDS->pszFileType = "Map";
    else
        poDS->pszFileType = "MapList";

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */

    for( int iBand = 1; iBand <= poDS->nBands; iBand++ )
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
                          int /* bStrict */, char ** papszOptions,
                          GDALProgressFunc pfnProgress, void * pProgressData )

{
    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();
    const int nBands = poSrcDS->GetRasterCount();

/* -------------------------------------------------------------------- */
/*      Create the basic dataset.                                       */
/* -------------------------------------------------------------------- */
    GDALDataType eType = GDT_Byte;
    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        GDALRasterBand *poBand = poSrcDS->GetRasterBand( iBand+1 );
        eType = GDALDataTypeUnion( eType, poBand->GetRasterDataType() );
    }

    ILWISDataset *poDS = (ILWISDataset *) Create( pszFilename,
                                                  poSrcDS->GetRasterXSize(),
                                                  poSrcDS->GetRasterYSize(),
                                                  nBands,
                                                  eType, papszOptions );

    if( poDS == NULL )
        return NULL;
    const string pszBaseName = string(CPLGetBasename( pszFilename ));
    const string pszPath = string(CPLGetPath( pszFilename ));

/* -------------------------------------------------------------------- */
/*  Copy and geo-transform and projection information.                  */
/* -------------------------------------------------------------------- */
    double adfGeoTransform[6];
    string georef = "none.grf";

    // Check whether we should create georeference file.
    // Source dataset must be north up.
    if( poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None
        && (adfGeoTransform[0] != 0.0 || adfGeoTransform[1] != 1.0
            || adfGeoTransform[2] != 0.0 || adfGeoTransform[3] != 0.0
            || adfGeoTransform[4] != 0.0 || fabs(adfGeoTransform[5]) != 1.0))
    {
        poDS->SetGeoTransform( adfGeoTransform );
        if (adfGeoTransform[2] == 0.0 && adfGeoTransform[4] == 0.0)
            georef = pszBaseName + ".grf";
    }

    const char *pszProj = poSrcDS->GetProjectionRef();
    if( pszProj != NULL && strlen(pszProj) > 0 )
        poDS->SetProjection( pszProj );

/* -------------------------------------------------------------------- */
/*      Create the output raster files for each band                    */
/* -------------------------------------------------------------------- */

    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        VSILFILE *fpData = NULL;

        GDALRasterBand *poBand = poSrcDS->GetRasterBand( iBand+1 );
        ILWISRasterBand *desBand = (ILWISRasterBand *) poDS->GetRasterBand( iBand+1 );

/* -------------------------------------------------------------------- */
/*      Translate the data type.                                        */
/* -------------------------------------------------------------------- */
        int nLineSize =  nXSize * GDALGetDataTypeSize(eType) / 8;

        //Determine the nodata value
        int bHasNoDataValue;
        double dNoDataValue = poBand->GetNoDataValue(&bHasNoDataValue);

        //Determine store type of ILWIS raster
        const string sStoreType = GDALType2ILWIS( eType );
        double stepsize = 1;
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
            char szName[100];
            snprintf(szName, sizeof(szName), "%s_band_%d", pszBaseName.c_str(),iBand + 1 );
            pszODFName = string(CPLFormFilename(pszPath.c_str(),szName,"mpr"));
            pszDataBaseName = string(szName);
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
            CPLsnprintf(strdouble, sizeof(strdouble), "%.3f:%.3f:%3f:offset=0", adfMinMax[0], adfMinMax[1],stepsize);
            string range = string(strdouble);
            WriteElement("BaseMap", "Range", pszODFName, range);
        }
        WriteElement("Map", "GeoRef", pszODFName, georef);

/* -------------------------------------------------------------------- */
/*      Loop over image, copy the image data.                           */
/* -------------------------------------------------------------------- */
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

        GByte *pData = (GByte *) CPLMalloc( nLineSize );

        CPLErr eErr = CE_None;
        for( int iLine = 0; iLine < nYSize && eErr == CE_None; iLine++ )
        {
            eErr = poBand->RasterIO( GF_Read, 0, iLine, nXSize, 1,
                                     pData, nXSize, 1, eType,
                                     0, 0, NULL );

            if( eErr == CE_None )
            {
                if (bHasNoDataValue)
                {
                    // pData may have entries with value = dNoDataValue
                    // ILWIS uses a fixed value for nodata, depending on the data-type
                    // Therefore translate the NoDataValue from each band to ILWIS
                    for (int iCol = 0; iCol < nXSize; iCol++ )
                    {
                        if( EQUAL(sStoreType.c_str(),"Byte"))
                        {
                            if ( ((GByte * )pData)[iCol] == dNoDataValue )
                                (( GByte * )pData)[iCol] = 0;
                        }
                        else if( EQUAL(sStoreType.c_str(),"Int"))
                        {
                            if ( ((GInt16 * )pData)[iCol] == dNoDataValue )
                                (( GInt16 * )pData)[iCol] = shUNDEF;
                        }
                        else if( EQUAL(sStoreType.c_str(),"Long"))
                        {
                            if ( ((GInt32 * )pData)[iCol] == dNoDataValue )
                                (( GInt32 * )pData)[iCol] = iUNDEF;
                        }
                        else if( EQUAL(sStoreType.c_str(),"float"))
                        {
                            if ((((float * )pData)[iCol] == dNoDataValue ) || (CPLIsNan((( float * )pData)[iCol])))
                                (( float * )pData)[iCol] = flUNDEF;
                        }
                        else if( EQUAL(sStoreType.c_str(),"Real"))
                        {
                            if ((((double * )pData)[iCol] == dNoDataValue ) || (CPLIsNan((( double * )pData)[iCol])))
                                (( double * )pData)[iCol] = rUNDEF;
                        }
                    }
                }
                int iSize = static_cast<int>(VSIFWriteL( pData, 1, nLineSize, desBand->fpRaw ));
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
        VSIFFlushL( fpData );
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

ILWISRasterBand::ILWISRasterBand( ILWISDataset *poDSIn, int nBandIn )

{
    this->poDS = poDSIn;
    this->nBand = nBandIn;

    string sBandName;
    if ( EQUAL(poDSIn->pszFileType.c_str(),"Map"))
        sBandName = string(poDSIn->osFileName);
    else //map list
    {
        //Form the band name
        char cBandName[45];
        snprintf( cBandName, sizeof(cBandName), "Map%d", nBand-1);
        sBandName = ReadElement("MapList", string(cBandName), string(poDSIn->osFileName));
        string sInputPath = string(CPLGetPath( poDSIn->osFileName));
        string sBandPath = string(CPLGetPath( sBandName.c_str()));
        string sBandBaseName = string(CPLGetBasename( sBandName.c_str()));
        if ( 0==sBandPath.length() )
            sBandName = string(CPLFormFilename(sInputPath.c_str(),sBandBaseName.c_str(),"mpr" ));
        else
          sBandName = string(CPLFormFilename(sBandPath.c_str(),sBandBaseName.c_str(),"mpr" ));
    }

    if (poDSIn->bNewDataset)
    {
      // Called from Create():
      // eDataType is defaulted to GDT_Byte by GDALRasterBand::GDALRasterBand
      // Here we set it to match the value of sStoreType (that was set in ILWISDataset::Create)
      // Unfortunately we can't take advantage of the ILWIS "ValueRange" object that would use
      // the most compact storeType possible, without going through all values.
        GetStoreType(sBandName, psInfo.stStoreType);
        eDataType = ILWIS2GDALType(psInfo.stStoreType);
    }
    else // Called from Open(), thus convert ILWIS type from ODF to eDataType
        GetILWISInfo(sBandName);

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
/*                          ~ILWISRasterBand()                          */
/************************************************************************/

ILWISRasterBand::~ILWISRasterBand()

{
    if( fpRaw != NULL )
    {
        VSIFCloseL( fpRaw );
        fpRaw = NULL;
    }
}


/************************************************************************/
/*                             ILWISOpen()                             */
/************************************************************************/
void ILWISRasterBand::ILWISOpen( string pszFileName )
{
    ILWISDataset* dataset = (ILWISDataset*) poDS;
    string pszDataFile
        = string(CPLResetExtension( pszFileName.c_str(), "mp#" ));

    fpRaw = VSIFOpenL( pszDataFile.c_str(), (dataset->eAccess == GA_Update) ? "rb+" : "rb");
}

/************************************************************************/
/*                 ReadValueDomainProperties()                          */
/************************************************************************/
// Helper function for GetILWISInfo, to avoid code-duplication
// Unfortunately with side-effect (changes members psInfo and eDataType)
void ILWISRasterBand::ReadValueDomainProperties(string pszFileName)
{
    string rangeString = ReadElement("BaseMap", "Range", pszFileName.c_str());
    psInfo.vr = ValueRange(rangeString);
    double rStep = psInfo.vr.get_rStep();
    if ( rStep != 0 )
    {
        psInfo.bUseValueRange = true; // use ILWIS ValueRange object to convert from "raw" to "value"
        double rMin = psInfo.vr.get_rLo();
        double rMax = psInfo.vr.get_rHi();
        if (rStep - (long)rStep == 0.0) // Integer values
        {
            if ( rMin >= 0 && rMax <= UCHAR_MAX)
              eDataType =  GDT_Byte;
            else if ( rMin >= SHRT_MIN && rMax <= SHRT_MAX)
              eDataType =  GDT_Int16;
            else if ( rMin >= 0 && rMax <= USHRT_MAX)
              eDataType =  GDT_UInt16;
            else if ( rMin >= INT_MIN && rMax <= INT_MAX)
              eDataType =  GDT_Int32;
            else if ( rMin >= 0 && rMax <= UINT_MAX)
              eDataType =  GDT_UInt32;
            else
              eDataType = GDT_Float64;
        }
        else // Floating point values
        {
            if ((rMin >= -FLT_MAX) && (rMax <= FLT_MAX) && (fabs(rStep) >= FLT_EPSILON)) // is "float" good enough?
              eDataType = GDT_Float32;
            else
              eDataType = GDT_Float64;
        }
    }
    else
    {
        if (psInfo.stStoreType == stFloat) // is "float" good enough?
          eDataType = GDT_Float32;
        else
          eDataType = GDT_Float64;
    }
}

/************************************************************************/
/*                       GetILWISInfo()                                 */
/************************************************************************/
// Calculates members psInfo and eDataType
CPLErr ILWISRasterBand::GetILWISInfo(string pszFileName)
{
    // Fill the psInfo struct with defaults.
    // Get the store type from the ODF
    if (GetStoreType(pszFileName, psInfo.stStoreType) != CE_None)
    {
        return CE_Failure;
    }
    psInfo.bUseValueRange = false;
    psInfo.stDomain = "";

    // ILWIS has several (currently 22) predefined "system-domains", that influence the data-type
    // The user can also create domains. The possible types for these are "class", "identifier", "bool" and "value"
    // The last one has Type=DomainValue
    // Here we make an effort to determine the most-compact gdal-type (eDataType) that is suitable
    // for the data in the current ILWIS band.
    // First check against all predefined domain names (the "system-domains")
    // If no match is found, read the domain ODF from disk, and get its type
    // We have hardcoded the system domains here, because ILWIS may not be installed, and even if it is,
    // we don't know where (thus it is useless to attempt to read a system-domain-file).

    string domName = ReadElement("BaseMap", "Domain", pszFileName.c_str());
    string pszBaseName = string(CPLGetBasename( domName.c_str() ));
    string pszPath = string(CPLGetPath( pszFileName.c_str() ));

    // Check against all "system-domains"
    if ( EQUAL(pszBaseName.c_str(),"value") // is it a system domain with Type=DomainValue?
        || EQUAL(pszBaseName.c_str(),"count")
        || EQUAL(pszBaseName.c_str(),"distance")
        || EQUAL(pszBaseName.c_str(),"min1to1")
        || EQUAL(pszBaseName.c_str(),"nilto1")
        || EQUAL(pszBaseName.c_str(),"noaa")
        || EQUAL(pszBaseName.c_str(),"perc")
        || EQUAL(pszBaseName.c_str(),"radar") )
    {
        ReadValueDomainProperties(pszFileName);
    }
    else if( EQUAL(pszBaseName.c_str(),"bool")
             || EQUAL(pszBaseName.c_str(),"byte")
             || EQUAL(pszBaseName.c_str(),"bit")
             || EQUAL(pszBaseName.c_str(),"image")
             || EQUAL(pszBaseName.c_str(),"colorcmp")
             || EQUAL(pszBaseName.c_str(),"flowdirection")
             || EQUAL(pszBaseName.c_str(),"hortonratio")
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
        // No match found. Assume it is a self-created domain. Read its type and decide the GDAL type.
        string pszDomainFileName = string(CPLFormFilename(pszPath.c_str(),pszBaseName.c_str(),"dom" ));
        string domType = ReadElement("Domain", "Type", pszDomainFileName.c_str());
        if EQUAL(domType.c_str(),"domainvalue") // is it a self-created domain of type=DomainValue?
        {
            ReadValueDomainProperties(pszFileName);
        }
        else if((!EQUAL(domType.c_str(),"domainbit"))
                && (!EQUAL(domType.c_str(),"domainstring"))
                && (!EQUAL(domType.c_str(),"domaincolor"))
                && (!EQUAL(domType.c_str(),"domainbinary"))
                && (!EQUAL(domType.c_str(),"domaincoordBuf"))
                && (!EQUAL(domType.c_str(),"domaincoord")))
        {
            // Type is "DomainClass", "DomainBool" or "DomainIdentifier".
            // For now we set the GDAL storeType be the same as the ILWIS storeType
            // The user will have to convert the classes manually.
            eDataType = ILWIS2GDALType(psInfo.stStoreType);
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unsupported ILWIS domain type.");
            return CE_Failure;
        }
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
CPLErr ILWISRasterBand::IReadBlock( CPL_UNUSED int nBlockXOff, int nBlockYOff,
                                    void * pImage )
{
    // pImage is empty; this function fills it with data from fpRaw
    // (ILWIS data to foreign data)

    // If the x block offset is non-zero, something is wrong.
    CPLAssert( nBlockXOff == 0 );

    int nBlockSize =  nBlockXSize * nBlockYSize * nSizePerPixel;
    if( fpRaw == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to open ILWIS data file.");
        return( CE_Failure );
    }

/* -------------------------------------------------------------------- */
/*	Handle the case of a strip in a writable file that doesn't          */
/*	exist yet, but that we want to read.  Just set to zeros and         */
/*	return.                                                             */
/* -------------------------------------------------------------------- */
    ILWISDataset* poIDS = (ILWISDataset*) poDS;

#ifdef notdef
    if( poIDS->bNewDataset && (poIDS->eAccess == GA_Update))
    {
        FillWithNoData(pImage);
        return CE_None;
    }
#endif

    VSIFSeekL( fpRaw, nBlockSize*nBlockYOff, SEEK_SET );
    void *pData = (char *)CPLMalloc(nBlockSize);
    if (VSIFReadL( pData, 1, nBlockSize, fpRaw ) < 1)
    {
        if( poIDS->bNewDataset )
        {
            FillWithNoData(pImage);
            return CE_None;
        }
        else
        {
            CPLFree( pData );
            CPLError( CE_Failure, CPLE_FileIO,
                      "Read of file failed with fread error.");
            return CE_Failure;
        }
    }

    // Copy the data from pData to pImage, and convert the store-type
    // The data in pData has store-type = psInfo.stStoreType
    // The data in pImage has store-type = eDataType
    // They may not match, because we have chosen the most compact store-type,
    // and for GDAL this may be different than for ILWIS.

    switch (psInfo.stStoreType)
    {
      case stByte:
        for( int iCol = 0; iCol < nBlockXSize; iCol++ )
        {
          double rV = psInfo.bUseValueRange ? psInfo.vr.rValue(((GByte *) pData)[iCol]) : ((GByte *) pData)[iCol];
          SetValue(pImage, iCol, rV);
        }
        break;
      case stInt:
        for( int iCol = 0; iCol < nBlockXSize; iCol++ )
        {
          double rV = psInfo.bUseValueRange ? psInfo.vr.rValue(((GInt16 *) pData)[iCol]) : ((GInt16 *) pData)[iCol];
          SetValue(pImage, iCol, rV);
        }
      break;
      case stLong:
        for( int iCol = 0; iCol < nBlockXSize; iCol++ )
        {
          double rV = psInfo.bUseValueRange ? psInfo.vr.rValue(((GInt32 *) pData)[iCol]) : ((GInt32 *) pData)[iCol];
          SetValue(pImage, iCol, rV);
        }
        break;
      case stFloat:
        for( int iCol = 0; iCol < nBlockXSize; iCol++ )
          ((float *) pImage)[iCol] = ((float *) pData)[iCol];
        break;
      case stReal:
        for( int iCol = 0; iCol < nBlockXSize; iCol++ )
          ((double *) pImage)[iCol] = ((double *) pData)[iCol];
        break;
      default:
        CPLAssert(0);
    }

    // Officially we should also translate "nodata" values, but at this point
    // we can't tell what's the "nodata" value of the destination (foreign) dataset

    CPLFree( pData );

    return CE_None;
}

void ILWISRasterBand::SetValue(void *pImage, int i, double rV) {
    switch ( eDataType ) {
    case GDT_Byte:
      ((GByte *)pImage)[i] = (GByte) rV;
      break;
    case GDT_UInt16:
      ((GUInt16 *) pImage)[i] = (GUInt16) rV;
      break;
    case GDT_Int16:
      ((GInt16 *) pImage)[i] = (GInt16) rV;
      break;
    case GDT_UInt32:
      ((GUInt32 *) pImage)[i] = (GUInt32) rV;
      break;
    case GDT_Int32:
      ((GInt32 *) pImage)[i] = (GInt32) rV;
      break;
    case GDT_Float32:
      ((float *) pImage)[i] = (float) rV;
      break;
    case GDT_Float64:
      ((double *) pImage)[i] = rV;
      break;
    default:
      CPLAssert(0);
    }
}

double ILWISRasterBand::GetValue(void *pImage, int i) {
  double rV = 0; // Does GDAL have an official nodata value?
    switch ( eDataType ) {
    case GDT_Byte:
      rV = ((GByte *)pImage)[i];
      break;
    case GDT_UInt16:
      rV = ((GUInt16 *) pImage)[i];
      break;
    case GDT_Int16:
      rV = ((GInt16 *) pImage)[i];
      break;
    case GDT_UInt32:
      rV = ((GUInt32 *) pImage)[i];
      break;
    case GDT_Int32:
      rV = ((GInt32 *) pImage)[i];
      break;
    case GDT_Float32:
      rV = ((float *) pImage)[i];
      break;
    case GDT_Float64:
      rV = ((double *) pImage)[i];
      break;
    default:
      CPLAssert(0);
    }
    return rV;
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

CPLErr ILWISRasterBand::IWriteBlock(CPL_UNUSED int nBlockXOff, int nBlockYOff,
                                    void* pImage)
{
    // pImage has data; this function reads this data and stores it to fpRaw
    // (foreign data to ILWIS data)

    // Note that this function will not overwrite existing data in fpRaw, but
    // it will "fill gaps" marked by "nodata" values

    ILWISDataset* dataset = (ILWISDataset*) poDS;

    CPLAssert( dataset != NULL
               && nBlockXOff == 0
               && nBlockYOff >= 0
               && pImage != NULL );

    int nXSize = dataset->GetRasterXSize();
    int nBlockSize = nBlockXSize * nBlockYSize * nSizePerPixel;
    void *pData = CPLMalloc(nBlockSize);

    VSIFSeekL( fpRaw, nBlockSize * nBlockYOff, SEEK_SET );

    bool fDataExists = (VSIFReadL( pData, 1, nBlockSize, fpRaw ) >= 1);

    // Copy the data from pImage to pData, and convert the store-type
    // The data in pData has store-type = psInfo.stStoreType
    // The data in pImage has store-type = eDataType
    // They may not match, because we have chosen the most compact store-type,
    // and for GDAL this may be different than for ILWIS.

    if( fDataExists )
    {
        // fpRaw (thus pData) already has data
        // Take care to not overwrite it
        // thus only fill in gaps (nodata values)
        switch (psInfo.stStoreType)
        {
          case stByte:
            for (int iCol = 0; iCol < nXSize; iCol++ )
                if ((( GByte * )pData)[iCol] == 0)
                {
                    double rV = GetValue(pImage, iCol);
                    (( GByte * )pData)[iCol] = (GByte)
                        (psInfo.bUseValueRange ? psInfo.vr.iRaw(rV) : rV);
                }
            break;
          case stInt:
            for (int iCol = 0; iCol < nXSize; iCol++ )
                if ((( GInt16 * )pData)[iCol] == shUNDEF)
                {
                    double rV = GetValue(pImage, iCol);
                    (( GInt16 * )pData)[iCol] = (GInt16)
                        (psInfo.bUseValueRange ? psInfo.vr.iRaw(rV) : rV);
                }
            break;
          case stLong:
            for (int iCol = 0; iCol < nXSize; iCol++ )
                if ((( GInt32 * )pData)[iCol] == iUNDEF)
                {
                    double rV = GetValue(pImage, iCol);
                    (( GInt32 * )pData)[iCol] = (GInt32)
                        (psInfo.bUseValueRange ? psInfo.vr.iRaw(rV) : rV);
                }
            break;
          case stFloat:
            for (int iCol = 0; iCol < nXSize; iCol++ )
                if ((( float * )pData)[iCol] == flUNDEF)
                    (( float * )pData)[iCol] = ((float* )pImage)[iCol];
            break;
          case stReal:
            for (int iCol = 0; iCol < nXSize; iCol++ )
                if ((( double * )pData)[iCol] == rUNDEF)
                    (( double * )pData)[iCol] = ((double* )pImage)[iCol];
            break;
        }
    }
    else
    {
        // fpRaw (thus pData) is still empty, just write the data
        switch (psInfo.stStoreType)
        {
          case stByte:
            for (int iCol = 0; iCol < nXSize; iCol++ )
            {
                double rV = GetValue(pImage, iCol);
                (( GByte * )pData)[iCol] = (GByte)
                    (psInfo.bUseValueRange ? psInfo.vr.iRaw(rV) : rV);
            }
            break;
          case stInt:
            for (int iCol = 0; iCol < nXSize; iCol++ )
            {
                double rV = GetValue(pImage, iCol);
                (( GInt16 * )pData)[iCol] = (GInt16)
                    (psInfo.bUseValueRange ? psInfo.vr.iRaw(rV) : rV);
            }
            break;
          case stLong:
            for (int iCol = 0; iCol < nXSize; iCol++ )
            {
                double rV = GetValue(pImage, iCol);
                ((GInt32 *)pData)[iCol] = (GInt32)
                    (psInfo.bUseValueRange ? psInfo.vr.iRaw(rV) : rV);
            }
            break;
          case stFloat:
            for (int iCol = 0; iCol < nXSize; iCol++ )
                (( float * )pData)[iCol] = ((float* )pImage)[iCol];
            break;
          case stReal:
            for (int iCol = 0; iCol < nXSize; iCol++ )
                (( double * )pData)[iCol] = ((double* )pImage)[iCol];
            break;
        }
    }

    // Officially we should also translate "nodata" values, but at this point
    // we can't tell what's the "nodata" value of the source (foreign) dataset

    VSIFSeekL( fpRaw, nBlockSize * nBlockYOff, SEEK_SET );

    if (VSIFWriteL( pData, 1, nBlockSize, fpRaw ) < 1)
    {
        CPLFree( pData );
        CPLError( CE_Failure, CPLE_FileIO,
                  "Write of file failed with fwrite error.");
        return CE_Failure;
    }

    CPLFree( pData );
    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/
double ILWISRasterBand::GetNoDataValue( int *pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = TRUE;

    if( eDataType == GDT_Float64 )
        return rUNDEF;
    if( eDataType == GDT_Int32)
        return iUNDEF;
    if( eDataType == GDT_Int16)
        return shUNDEF;
    if( eDataType == GDT_Float32)
        return flUNDEF;
    if( pbSuccess &&
             (EQUAL(psInfo.stDomain.c_str(),"image")
              || EQUAL(psInfo.stDomain.c_str(),"colorcmp")))
    {
        *pbSuccess = FALSE;
    }

    // TODO: Defaults to pbSuccess TRUE.  Is the unhandled case really success?
    return 0.0;
}

/************************************************************************/
/*                      ValueRange()                                    */
/************************************************************************/

static double doubleConv(const char* s)
{
    if (s == NULL) return rUNDEF;
    char *begin = const_cast<char*>(s);

    // skip leading spaces; strtol will return 0 on a string with only spaces
    // which is not what we want
    while (isspace((unsigned char)*begin)) ++begin;

    if (strlen(begin) == 0) return rUNDEF;
    errno = 0;
    char *endptr;
    const double r = CPLStrtod(begin, &endptr);
    if ((0 == *endptr) && (errno==0))
        return r;
    while (*endptr != 0) { // check trailing spaces
        if (*endptr != ' ')
            return rUNDEF;
        endptr++;
    }
    return r;
}

ValueRange::ValueRange(string sRng) :
    _rLo(0.0), _rHi(0.0), _rStep(0.0), _iDec(0), _r0(0.0), iRawUndef(0),
    _iWidth(0), st(stByte)
{
    char* sRange = new char[sRng.length() + 1];
    for (unsigned int i = 0; i < sRng.length(); ++i)
        sRange[i] = sRng[i];
    sRange[sRng.length()] = 0;

    char *p1 = strchr(sRange, ':');
    if (NULL == p1)
    {
        delete[] sRange;
        init();
        return;
    }

    char *p3 = strstr(sRange, ",offset=");
    if (NULL == p3)
        p3 = strstr(sRange, ":offset=");
    _r0 = rUNDEF;
    if (NULL != p3) {
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
    if (p2 != NULL) {
        *p2 = 0;
        _rLo = CPLAtof(sRange);
        _rHi = CPLAtof(p2+1);
    }
    else {
        _rLo = CPLAtof(sRange);
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
        _iWidth = (short) (iBeforeDec + _iDec);
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

string ValueRange::ToString()
{
    char buffer[200];
    if (fabs(get_rLo()) > 1.0e20 || fabs(get_rHi()) > 1.0e20)
        CPLsnprintf(buffer, sizeof(buffer), "%g:%g:%f:offset=%g", get_rLo(), get_rHi(), get_rStep(), get_rRaw0());
    else if (get_iDec() >= 0)
        CPLsnprintf(buffer, sizeof(buffer), "%.*f:%.*f:%.*f:offset=%.0f", get_iDec(), get_rLo(), get_iDec(), get_rHi(), get_iDec(), get_rStep(), get_rRaw0());
    else
        CPLsnprintf(buffer, sizeof(buffer), "%f:%f:%f:offset=%.0f", get_rLo(), get_rHi(), get_rStep(), get_rRaw0());
    return string(buffer);
}

double ValueRange::rValue(int iRawIn)
{
    if (iRawIn == iUNDEF || iRawIn == iRawUndef)
        return rUNDEF;
    double rVal = iRawIn + _r0;
    rVal *= _rStep;
    if (get_rLo() == get_rHi())
        return rVal;
    const double rEpsilon = _rStep == 0.0 ? 1e-6 : _rStep / 3.0; // avoid any rounding problems with an epsilon directly based on the
    // the stepsize
    if ((rVal - get_rLo() < -rEpsilon) || (rVal - get_rHi() > rEpsilon))
        return rUNDEF;
    return rVal;
}

int ValueRange::iRaw(double rValueIn)
{
    if (rValueIn == rUNDEF) // || !fContains(rValue))
        return iUNDEF;
    const double rEpsilon = _rStep == 0.0 ? 1e-6 : _rStep / 3.0;
    if (rValueIn - get_rLo() < -rEpsilon) // take a little rounding tolerance
        return iUNDEF;
    else if (rValueIn - get_rHi() > rEpsilon) // take a little rounding tolerance
        return iUNDEF;
    rValueIn /= _rStep;
    double rVal = floor(rValueIn+0.5);
    rVal -= _r0;
    long iVal = longConv(rVal);
    return static_cast<int>(iVal);
}


/************************************************************************/
/*                    GDALRegister_ILWIS()                              */
/************************************************************************/

void GDALRegister_ILWIS()

{
    if( GDALGetDriverByName( "ILWIS" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "ILWIS" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "ILWIS Raster Map" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "mpr/mpl" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte Int16 Int32 Float64" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = ILWISDataset::Open;
    poDriver->pfnCreate = ILWISDataset::Create;
    poDriver->pfnCreateCopy = ILWISDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
