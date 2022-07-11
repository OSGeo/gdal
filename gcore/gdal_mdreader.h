/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata (mainly the remote sensing imagery) from files of
 *           different providers like DigitalGlobe, GeoEye etc.
 * Author:   Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) 2014-2015, NextGIS info@nextgis.ru
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

#ifndef GDAL_MDREADER_H_INCLUDED
#define GDAL_MDREADER_H_INCLUDED

#include "cpl_port.h"
#include "gdal_priv.h"

#include <map>

#define MD_DOMAIN_IMD "IMD"              /**< image metadata section */
#define MD_DOMAIN_RPC "RPC"              /**< rpc metadata section */
#define MD_DOMAIN_IMAGERY "IMAGERY"      /**< imagery metadata section */
#define MD_DOMAIN_DEFAULT ""             /**< default metadata section */

#define MD_NAME_ACQDATETIME "ACQUISITIONDATETIME"  /**< Acquisition Date Time property name. The time should be in UTC */
#define MD_NAME_SATELLITE   "SATELLITEID"          /**< Satellite identificator property name */
#define MD_NAME_CLOUDCOVER  "CLOUDCOVER"           /**< Cloud coverage property name. The value between 0 - 100 or 999 if n/a */
#define MD_NAME_MDTYPE      "METADATATYPE"         /**< Metadata reader type property name. The reader processed this metadata */

#define MD_DATETIMEFORMAT "%Y-%m-%d %H:%M:%S"      /**< Date time format */
#define MD_CLOUDCOVER_NA "999"           /**< The value if cloud cover is n/a */

/**
 * RPC/RPB specific defines
 */

#define RPC_ERR_BIAS        "ERR_BIAS"
#define RPC_ERR_RAND        "ERR_RAND"
#define RPC_LINE_OFF        "LINE_OFF"
#define RPC_SAMP_OFF        "SAMP_OFF"
#define RPC_LAT_OFF         "LAT_OFF"
#define RPC_LONG_OFF        "LONG_OFF"
#define RPC_HEIGHT_OFF      "HEIGHT_OFF"
#define RPC_LINE_SCALE      "LINE_SCALE"
#define RPC_SAMP_SCALE      "SAMP_SCALE"
#define RPC_LAT_SCALE       "LAT_SCALE"
#define RPC_LONG_SCALE      "LONG_SCALE"
#define RPC_HEIGHT_SCALE    "HEIGHT_SCALE"
#define RPC_LINE_NUM_COEFF  "LINE_NUM_COEFF"
#define RPC_LINE_DEN_COEFF  "LINE_DEN_COEFF"
#define RPC_SAMP_NUM_COEFF  "SAMP_NUM_COEFF"
#define RPC_SAMP_DEN_COEFF  "SAMP_DEN_COEFF"

/* Optional */
#define RPC_MIN_LONG        "MIN_LONG"
#define RPC_MIN_LAT         "MIN_LAT"
#define RPC_MAX_LONG        "MAX_LONG"
#define RPC_MAX_LAT         "MAX_LAT"

/* Pleiades Neo nomenclature */
#define RPC_LAT_NUM_COEFF  "LAT_NUM_COEFF"
#define RPC_LAT_DEN_COEFF  "LAT_DEN_COEFF"
#define RPC_LON_NUM_COEFF  "LON_NUM_COEFF"
#define RPC_LON_DEN_COEFF  "LON_DEN_COEFF"

/**
 * Enumerator of metadata readers
 */

typedef enum {
    MDR_None     = 0x00000000,    /**< no reader */
    MDR_DG       = 0x00000001,    /**< Digital Globe, METADATATYPE=DG */
    MDR_GE       = 0x00000002,    /**< Geo Eye,       METADATATYPE=GE */
    MDR_OV       = 0x00000004,    /**< Orb View,      METADATATYPE=OV */
    MDR_PLEIADES = 0x00000008,    /**< Pleiades,      METADATATYPE=DIMAP */
    MDR_SPOT     = 0x00000010,    /**< Spot,          METADATATYPE=DIMAP */
    MDR_RDK1     = 0x00000020,    /**< Resurs DK1,    METADATATYPE=MSP */
    MDR_LS       = 0x00000040,    /**< Landsat,       METADATATYPE=ODL */
    MDR_RE       = 0x00000080,    /**< RapidEye,      METADATATYPE=RE */
    MDR_KOMPSAT  = 0x00000100,    /**< Kompsat,       METADATATYPE=KARI */
    MDR_EROS     = 0x00000200,    /**< EROS,          METADATATYPE=EROS */
    MDR_ALOS     = 0x00000400,    /**< ALOS,          METADATATYPE=ALOS */
    MDR_ANY  = MDR_DG | MDR_GE | MDR_OV | MDR_PLEIADES | MDR_SPOT | MDR_RDK1 |
               MDR_LS | MDR_RE | MDR_KOMPSAT | MDR_EROS | MDR_ALOS /**< any reader */
} MDReaders;

/**
 * The base class for all metadata readers
 */
class CPL_DLL GDALMDReaderBase{

    CPL_DISALLOW_COPY_ASSIGN(GDALMDReaderBase)

    void ReadXMLToListFirstPass(const CPLXMLNode* psNode,
                                std::map<std::string, int>& oMapCountKeysFull,
                                const std::string& osPrefixFull);

    char** ReadXMLToList(const CPLXMLNode* psNode,
                         char** papszList,
                         const std::map<std::string, int>& oMapCountKeysFullRef,
                         std::map<std::string, int>& oMapCountKeysFull,
                         std::map<std::string, int>& oMapCountKeys,
                         const std::string& osPrefix,
                         const std::string& osPrefixFull);

public:
    GDALMDReaderBase(const char *pszPath, char **papszSiblingFiles);
    virtual ~GDALMDReaderBase();

    /**
     * @brief Get specified metadata domain
     * @param pszDomain The metadata domain to return
     * @return List of metadata items
     */
    virtual char ** GetMetadataDomain(const char *pszDomain);
    /**
     * @brief Fill provided metadata store class
     * @param poMDMD Metadata store class
     * @return true on success or false
     */
    virtual bool FillMetadata(GDALMultiDomainMetadata* poMDMD);
    /**
      * @brief Determine whether the input parameter correspond to the particular
      *        provider of remote sensing data completely
      * @return True if all needed sources files found
      */
    virtual bool HasRequiredFiles() const = 0;
    /**
     * @brief Get metadata file names. The caller become owner of returned list
     *        and have to free it via CSLDestroy.
     * @return A file name list
     */
    virtual char** GetMetadataFiles() const = 0;
protected:
    /**
     * @brief Load metadata to the correspondent IMD, RPB, IMAGERY and DEFAULT
     *        domains
     */
    virtual void LoadMetadata();
    /**
     * @brief Convert string like 2012-02-25T00:25:59.9440000Z to time
     * @param pszDateTime String to convert
     * @return value in second sinc epoch 1970-01-01 00:00:00
     */
    virtual GIntBig GetAcquisitionTimeFromString(const char* pszDateTime);
    /**
     * @brief ReadXMLToList Transform xml to list of NULL terminated name=value
     *        strings
     * @param psNode A xml node to process
     * @param papszList A list to fill with name=value strings
     * @param pszName A name of parent node. For root xml node should be empty.
     *        If name is not empty, the sibling nodes will not proceed
     * @return An input list filled with values
     */
    virtual char** ReadXMLToList(CPLXMLNode* psNode, char** papszList,
                         const char* pszName = "");
    /**
     * @brief AddXMLNameValueToList Execute from ReadXMLToList to add name and
     *        value to list. One can override this function for special
     *        processing input values before add to list.
     * @param papszList A list to fill with name=value strings
     * @param pszName A name to add
     * @param pszValue A value to add
     * @return An input list filled with values
     */
    virtual char** AddXMLNameValueToList(char** papszList, const char *pszName,
                                         const char *pszValue);
protected:
//! @cond Doxygen_Suppress
    char **m_papszIMDMD = nullptr;
    char **m_papszRPCMD = nullptr;
    char **m_papszIMAGERYMD = nullptr;
    char **m_papszDEFAULTMD = nullptr;
    bool m_bIsMetadataLoad = false;
//! @endcond
};

/**
 * The metadata reader main class.
 * The main purpose of this class is to provide an correspondent reader
 * for provided path.
 */
class CPL_DLL GDALMDReaderManager{

    CPL_DISALLOW_COPY_ASSIGN(GDALMDReaderManager)

public:
    GDALMDReaderManager();
    virtual ~GDALMDReaderManager();

    /**
     * @brief Try to detect metadata reader correspondent to the provided
     *        datasource path
     * @param pszPath a path to GDALDataset
     * @param papszSiblingFiles file list for metadata search purposes
     * @param nType a preferable reader type (may be the OR of MDReaders)
     * @return an appropriate reader or NULL if no such reader or error.
     * The pointer delete by the GDALMDReaderManager, so the user have not
     * delete it.
     */
    virtual GDALMDReaderBase* GetReader(const char *pszPath,
                                        char **papszSiblingFiles,
                                        GUInt32 nType = MDR_ANY);
protected:
//! @cond Doxygen_Suppress
    GDALMDReaderBase *m_pReader = nullptr;
//! @endcond
};

// misc
CPLString CPLStrip(const CPLString& osString, const char cChar);
CPLString CPLStripQuotes(const CPLString& osString);
char** GDALLoadRPBFile( const CPLString& osFilePath );
char** GDALLoadRPCFile( const CPLString& osFilePath );
char** GDALLoadIMDFile( const CPLString& osFilePath );
bool GDALCheckFileHeader(const CPLString& soFilePath,
                               const char * pszTestString,
                               int nBufferSize = 256);

CPLErr GDALWriteRPBFile( const char *pszFilename, char **papszMD );
CPLErr GDALWriteRPCTXTFile( const char *pszFilename, char **papszMD );
CPLErr GDALWriteIMDFile( const char *pszFilename, char **papszMD );

#endif //GDAL_MDREADER_H_INCLUDED
