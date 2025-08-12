/******************************************************************************
 *
 * Project:  DRDC Ottawa GEOINT
 * Purpose:  Radarsat Constellation Mission - XML Products (product.xml) driver
 * Author:   Roberto Caron, MDA
 *           on behalf of DRDC Ottawa
 *
 ******************************************************************************
 * Copyright (c) 2020, DRDC Ottawa
 *
 * Based on the RS2 Dataset Class
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include <time.h>
#include <stdio.h>
#include <sstream>

#include "cpl_minixml.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "ogr_spatialref.h"
#include "rcmdataset.h"
#include "rcmdrivercore.h"

#include <limits>

constexpr int max_space_for_string = 32;

/* RCM has a special folder that contains all LUT, Incidence Angle and Noise
 * Level files */
constexpr const char *CALIBRATION_FOLDER = "calibration";

/*** Function to format calibration for unique identification for Layer Name
 * ***/
/*
 *  RCM_CALIB : { SIGMA0 | GAMMA0 | BETA0 | UNCALIB } : product.xml full path
 */
inline CPLString FormatCalibration(const char *pszCalibName,
                                   const char *pszFilename)
{
    CPLString ptr;

    // Always begin by the layer calibration name
    ptr.append(szLayerCalibration);

    // A separator is needed before concat calibration name
    ptr += chLayerSeparator;
    // Add calibration name
    ptr.append(pszCalibName);

    // A separator is needed before concat full filename name
    ptr += chLayerSeparator;
    // Add full filename name
    ptr.append(pszFilename);

    /* return calibration format */
    return ptr;
}

/*** Function to test for valid LUT files ***/
static bool IsValidXMLFile(const char *pszPath)
{
    /* Return true for valid xml file, false otherwise */
    CPLXMLTreeCloser psLut(CPLParseXMLFile(pszPath));

    if (psLut.get() == nullptr)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "ERROR: Failed to open the LUT file %s", pszPath);
    }

    return psLut.get() != nullptr;
}

static double *InterpolateValues(CSLConstList papszList, int tableSize,
                                 int stepSize, int numberOfValues,
                                 int pixelFirstLutValue)
{
    /* Allocate the right LUT size according to the product range pixel */
    double *table =
        static_cast<double *>(VSI_CALLOC_VERBOSE(sizeof(double), tableSize));
    if (!table)
        return nullptr;

    if (stepSize <= 0)
    {
        /* When negative, the range of pixel is calculated from the opposite
         * starting from the end of gains array */
        /* Just step the range with positive value */
        const int positiveStepSize = abs(stepSize);

        int k = 0;

        if (positiveStepSize == 1)
        {
            // Be fast and just copy the values because all gain values
            // represent all image wide pixel
            /* Start at the end position and store in the opposite */
            for (int i = pixelFirstLutValue; i >= 0; i--)
            {
                const double value = CPLAtof(papszList[i]);
                table[k++] = value;
            }
        }
        else
        {

            /* Interpolation between 2 numbers */
            for (int i = numberOfValues - 1; i >= 0; i--)
            {
                // We will consider the same value to cover the case that we
                // will hit the last pixel
                double valueFrom = CPLAtof(papszList[i]);
                double valueTo = valueFrom;

                if (i > 0)
                {
                    // We have room to pick the previous number to interpolate
                    // with
                    valueTo = CPLAtof(papszList[i - 1]);
                }

                // If the valueFrom minus ValueTo equal 0, it means to finish
                // off with the same number until the end of the table size
                double interp = (valueTo - valueFrom) / positiveStepSize;

                // Always begin with the value FROM found
                table[k++] = valueFrom;

                // Then add interpolation, don't forget. The stepSize is
                // actually counting our valueFrom number thus we add
                // interpolation until the last step - 1
                for (int j = 0; j < positiveStepSize - 1; j++)
                {
                    valueFrom += interp;
                    table[k++] = valueFrom;
                }
            }
        }
    }
    else
    {
        /* When positive, the range of pixel is calculated from the beginning of
         * gains array */
        if (stepSize == 1)
        {
            // Be fast and just copy the values because all gain values
            // represent all image wide pixel
            for (int i = 0; i < numberOfValues; i++)
            {
                const double value = CPLAtof(papszList[i]);
                table[i] = value;
            }
        }
        else
        {
            /* Interpolation between 2 numbers */
            int k = 0;
            for (int i = 0; i < numberOfValues; i++)
            {
                // We will consider the same value to cover the case that we
                // will hit the last pixel
                double valueFrom = CPLAtof(papszList[i]);
                double valueTo = valueFrom;

                if (i < (numberOfValues)-1)
                {
                    // We have room to pick the next number to interpolate with
                    valueTo = CPLAtof(papszList[i + 1]);
                }

                // If the valueFrom minus ValueTo equal 0, it means to finish
                // off with the same number until the end of the table size
                double interp = (valueTo - valueFrom) / stepSize;

                // Always begin with the value FROM found
                table[k++] = valueFrom;

                // Then add interpolation, don't forget. The stepSize is
                // actually counting our valueFrom number thus we add
                // interpolation until the last step - 1
                for (int j = 0; j < stepSize - 1; j++)
                {
                    valueFrom += interp;
                    table[k++] = valueFrom;
                }
            }
        }
    }

    return table;
}

/*** check that the referenced dataset for each band has the
correct data type and returns whether a 2 band I+Q dataset should
be mapped onto a single complex band.
Returns BANDERROR for error, STRAIGHT for 1:1 mapping, TWOBANDCOMPLEX for 2
bands -> 1 complex band
*/
typedef enum
{
    BANDERROR,
    STRAIGHT,
    TWOBANDCOMPLEX
} BandMappingRCM;

static BandMappingRCM checkBandFileMappingRCM(GDALDataType dataType,
                                              GDALDataset *poBandFile,
                                              bool isNITF)
{

    GDALRasterBand *band1 = poBandFile->GetRasterBand(1);
    GDALDataType bandfileType = band1->GetRasterDataType();
    // if there is one band and it has the same datatype, the band file gets
    // passed straight through
    if ((poBandFile->GetRasterCount() == 1 ||
         poBandFile->GetRasterCount() == 4) &&
        dataType == bandfileType)
        return STRAIGHT;

    // if the band file has 2 bands, they should represent I+Q
    // and be a compatible data type
    if (poBandFile->GetRasterCount() == 2 && GDALDataTypeIsComplex(dataType))
    {
        GDALRasterBand *band2 = poBandFile->GetRasterBand(2);

        if (bandfileType != band2->GetRasterDataType())
            return BANDERROR;  // both bands must be same datatype

        // check compatible types - there are 4 complex types in GDAL
        if ((dataType == GDT_CInt16 && bandfileType == GDT_Int16) ||
            (dataType == GDT_CInt32 && bandfileType == GDT_Int32) ||
            (dataType == GDT_CFloat32 && bandfileType == GDT_Float32) ||
            (dataType == GDT_CFloat64 && bandfileType == GDT_Float64))
            return TWOBANDCOMPLEX;

        if ((dataType == GDT_CInt16 && bandfileType == GDT_CInt16) ||
            (dataType == GDT_CInt32 && bandfileType == GDT_CInt32) ||
            (dataType == GDT_CFloat32 && bandfileType == GDT_CFloat32) ||
            (dataType == GDT_CFloat64 && bandfileType == GDT_CFloat64))
            return TWOBANDCOMPLEX;
    }

    if (isNITF)
    {
        return STRAIGHT;
    }

    return BANDERROR;  // don't accept any other combinations
}

/************************************************************************/
/*                            RCMRasterBand                             */
/************************************************************************/

RCMRasterBand::RCMRasterBand(RCMDataset *poDSIn, int nBandIn,
                             GDALDataType eDataTypeIn, const char *pszPole,
                             GDALDataset *poBandFileIn, bool bTwoBandComplex,
                             bool isOneFilePerPolIn, bool isNITFIn)
    : poBandFile(poBandFileIn), poRCMDataset(poDSIn),
      twoBandComplex(bTwoBandComplex), isOneFilePerPol(isOneFilePerPolIn),
      isNITF(isNITFIn)
{
    poDS = poDSIn;
    this->nBand = nBandIn;
    eDataType = eDataTypeIn;

    /*Check image type, whether there is one file per polarization or
     *one file containing all polarizations*/
    if (this->isOneFilePerPol)
    {
        poBand = poBandFile->GetRasterBand(1);
    }
    else
    {
        poBand = poBandFile->GetRasterBand(this->nBand);
    }

    poBand->GetBlockSize(&nBlockXSize, &nBlockYSize);

    if (pszPole != nullptr && strlen(pszPole) != 0)
    {
        SetMetadataItem("POLARIMETRIC_INTERP", pszPole);
    }
}

/************************************************************************/
/*                            RCMRasterBand()                            */
/************************************************************************/

RCMRasterBand::~RCMRasterBand()

{
    if (poBandFile != nullptr)
        GDALClose(reinterpret_cast<GDALRasterBandH>(poBandFile));
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr RCMRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff, void *pImage)

{
    int nRequestXSize = 0;
    int nRequestYSize = 0;
    GetActualBlockSize(nBlockXOff, nBlockYOff, &nRequestXSize, &nRequestYSize);

    // Zero initial partial right-most and bottom-most blocks
    if (nRequestXSize < nBlockXSize || nRequestYSize < nBlockYSize)
    {
        memset(pImage, 0,
               static_cast<size_t>(GDALGetDataTypeSizeBytes(eDataType)) *
                   nBlockXSize * nBlockYSize);
    }

    int dataTypeSize = GDALGetDataTypeSizeBytes(eDataType);
    GDALDataType bandFileType =
        poBandFile->GetRasterBand(1)->GetRasterDataType();
    int bandFileSize = GDALGetDataTypeSizeBytes(bandFileType);

    // case: 2 bands representing I+Q -> one complex band
    if (twoBandComplex && !this->isNITF)
    {
        // int bandFileSize = GDALGetDataTypeSizeBytes(bandFileType);
        // this data type is the complex version of the band file
        // Roberto: don't check that for the moment: CPLAssert(dataTypeSize ==
        // bandFileSize * 2);

        return
            // I and Q from each band are pixel-interleaved into this complex
            // band
            poBandFile->RasterIO(
                GF_Read, nBlockXOff * nBlockXSize, nBlockYOff * nBlockYSize,
                nRequestXSize, nRequestYSize, pImage, nRequestXSize,
                nRequestYSize, bandFileType, 2, nullptr, dataTypeSize,
                static_cast<GSpacing>(dataTypeSize) * nBlockXSize, bandFileSize,
                nullptr);
    }
    else if (twoBandComplex && this->isNITF)
    {
        return poBand->RasterIO(
            GF_Read, nBlockXOff * nBlockXSize, nBlockYOff * nBlockYSize,
            nRequestXSize, nRequestYSize, pImage, nRequestXSize, nRequestYSize,
            eDataType, 0, static_cast<GSpacing>(dataTypeSize) * nBlockXSize,
            nullptr);
    }

    if (poRCMDataset->IsComplexData())
    {
        // this data type is the complex version of the band file
        // Roberto: don't check that for the moment: CPLAssert(dataTypeSize ==
        // bandFileSize * 2);
        return
            // I and Q from each band are pixel-interleaved into this complex
            // band
            poBandFile->RasterIO(
                GF_Read, nBlockXOff * nBlockXSize, nBlockYOff * nBlockYSize,
                nRequestXSize, nRequestYSize, pImage, nRequestXSize,
                nRequestYSize, bandFileType, 2, nullptr, dataTypeSize,
                static_cast<GSpacing>(dataTypeSize) * nBlockXSize, bandFileSize,
                nullptr);
    }

    // case: band file == this band
    // NOTE: if the underlying band is opened with the NITF driver, it may
    // combine 2 band I+Q -> complex band
    else if (poBandFile->GetRasterBand(1)->GetRasterDataType() == eDataType)
    {
        return poBand->RasterIO(
            GF_Read, nBlockXOff * nBlockXSize, nBlockYOff * nBlockYSize,
            nRequestXSize, nRequestYSize, pImage, nRequestXSize, nRequestYSize,
            eDataType, 0, static_cast<GSpacing>(dataTypeSize) * nBlockXSize,
            nullptr);
    }
    else
    {
        CPLAssert(FALSE);
        return CE_Failure;
    }
}

/************************************************************************/
/*                            ReadLUT()                                 */
/************************************************************************/
/* Read the provided LUT in to m_ndTable                                */
/* 1. The gains list spans the range extent covered by all              */
/*    beams(if applicable).                                             */
/* 2. The mapping between the entry of gains                            */
/*    list and the range sample index is : the range sample             */
/*    index = gains entry index * stepSize + pixelFirstLutValue,        */
/*    where the gains entry index starts with ‘0’.For ScanSAR SLC,      */
/*    the range sample index refers to the index on the COPG            */
/************************************************************************/
void RCMCalibRasterBand::ReadLUT()
{

    char bandNumber[12];
    snprintf(bandNumber, sizeof(bandNumber), "%d", poDS->GetRasterCount() + 1);

    CPLXMLTreeCloser psLUT(CPLParseXMLFile(m_pszLUTFile));
    if (!psLUT)
        return;

    this->m_nfOffset =
        CPLAtof(CPLGetXMLValue(psLUT.get(), "=lut.offset", "0.0"));

    this->pixelFirstLutValue =
        atoi(CPLGetXMLValue(psLUT.get(), "=lut.pixelFirstLutValue", "0"));

    this->stepSize = atoi(CPLGetXMLValue(psLUT.get(), "=lut.stepSize", "0"));

    this->numberOfValues =
        atoi(CPLGetXMLValue(psLUT.get(), "=lut.numberOfValues", "0"));

    if (this->numberOfValues <= 0)
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "ERROR: The RCM driver does not support the LUT Number Of Values  "
            "equal or lower than zero.");
        return;
    }

    const CPLStringList aosLUTList(
        CSLTokenizeString2(CPLGetXMLValue(psLUT.get(), "=lut.gains", ""), " ",
                           CSLT_HONOURSTRINGS));

    if (this->stepSize <= 0)
    {
        if (this->pixelFirstLutValue <= 0)
        {
            CPLError(
                CE_Failure, CPLE_NotSupported,
                "ERROR: The RCM driver does not support LUT Pixel First Lut "
                "Value equal or lower than zero when the product is "
                "descending.");
            return;
        }
    }

    /* Get the Pixel Per range */
    if (this->stepSize == 0 || this->stepSize == INT_MIN ||
        this->numberOfValues == INT_MIN ||
        abs(this->stepSize) > INT_MAX / abs(this->numberOfValues))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Bad values of stepSize / numberOfValues");
        return;
    }

    this->m_nTableSize = abs(this->stepSize) * abs(this->numberOfValues);

    if (this->m_nTableSize < this->m_poBandDataset->GetRasterXSize())
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "ERROR: The RCM driver does not support range of LUT gain values "
            "lower than the full image pixel range.");
        return;
    }

    // Avoid excessive memory allocation
    if (this->m_nTableSize > 1000 * 1000)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Too many elements in LUT: %d",
                 this->m_nTableSize);
        return;
    }

    /* Allocate the right LUT size according to the product range pixel */
    this->m_nfTable =
        InterpolateValues(aosLUTList.List(), this->m_nTableSize, this->stepSize,
                          this->numberOfValues, this->pixelFirstLutValue);
    if (!this->m_nfTable)
        return;

    // 32 max + space
    char *lut_gains = static_cast<char *>(
        VSI_CALLOC_VERBOSE(this->m_nTableSize, max_space_for_string));
    if (!lut_gains)
        return;

    for (int i = 0; i < this->m_nTableSize; i++)
    {
        char lut[max_space_for_string];
        // 6.123004711900930e+04  %e Scientific annotation
        snprintf(lut, sizeof(lut), "%e ", this->m_nfTable[i]);
        strcat(lut_gains, lut);
    }

    poDS->SetMetadataItem(CPLString("LUT_GAINS_").append(bandNumber).c_str(),
                          lut_gains);
    // Can free this because the function SetMetadataItem takes a copy
    CPLFree(lut_gains);

    char snum[256];
    if (this->m_eCalib == eCalibration::Sigma0)
    {
        poDS->SetMetadataItem(CPLString("LUT_TYPE_").append(bandNumber).c_str(),
                              "SIGMA0");
    }
    else if (this->m_eCalib == eCalibration::Beta0)
    {
        poDS->SetMetadataItem(CPLString("LUT_TYPE_").append(bandNumber).c_str(),
                              "BETA0");
    }
    else if (this->m_eCalib == eCalibration::Gamma)
    {
        poDS->SetMetadataItem(CPLString("LUT_TYPE_").append(bandNumber).c_str(),
                              "GAMMA");
    }
    snprintf(snum, sizeof(snum), "%d", this->m_nTableSize);
    poDS->SetMetadataItem(CPLString("LUT_SIZE_").append(bandNumber).c_str(),
                          snum);
    snprintf(snum, sizeof(snum), "%f", this->m_nfOffset);
    poDS->SetMetadataItem(CPLString("LUT_OFFSET_").append(bandNumber).c_str(),
                          snum);
}

/************************************************************************/
/*                            ReadNoiseLevels()                         */
/************************************************************************/
/* Read the provided LUT in to m_nfTableNoiseLevels                     */
/* 1. The gains list spans the range extent covered by all              */
/*    beams(if applicable).                                             */
/* 2. The mapping between the entry of gains                            */
/*    list and the range sample index is : the range sample             */
/*    index = gains entry index * stepSize + pixelFirstLutValue,        */
/*    where the gains entry index starts with ‘0’.For ScanSAR SLC,      */
/*    the range sample index refers to the index on the COPG            */
/************************************************************************/
void RCMCalibRasterBand::ReadNoiseLevels()
{

    this->m_nfTableNoiseLevels = nullptr;

    if (this->m_pszNoiseLevelsFile == nullptr)
    {
        return;
    }

    char bandNumber[12];
    snprintf(bandNumber, sizeof(bandNumber), "%d", poDS->GetRasterCount() + 1);

    CPLXMLTreeCloser psNoiseLevels(CPLParseXMLFile(this->m_pszNoiseLevelsFile));
    if (!psNoiseLevels)
        return;

    // Load Beta Nought, Sigma Nought, Gamma noise levels
    // Loop through all nodes with spaces
    CPLXMLNode *psReferenceNoiseLevelNode =
        CPLGetXMLNode(psNoiseLevels.get(), "=noiseLevels");
    if (!psReferenceNoiseLevelNode)
        return;

    CPLXMLNode *psNodeInc;
    for (psNodeInc = psReferenceNoiseLevelNode->psChild; psNodeInc != nullptr;
         psNodeInc = psNodeInc->psNext)
    {
        if (EQUAL(psNodeInc->pszValue, "referenceNoiseLevel"))
        {
            CPLXMLNode *psCalibType =
                CPLGetXMLNode(psNodeInc, "sarCalibrationType");
            CPLXMLNode *psPixelFirstNoiseValue =
                CPLGetXMLNode(psNodeInc, "pixelFirstNoiseValue");
            CPLXMLNode *psStepSize = CPLGetXMLNode(psNodeInc, "stepSize");
            CPLXMLNode *psNumberOfValues =
                CPLGetXMLNode(psNodeInc, "numberOfValues");
            CPLXMLNode *psNoiseLevelValues =
                CPLGetXMLNode(psNodeInc, "noiseLevelValues");

            if (psCalibType != nullptr && psPixelFirstNoiseValue != nullptr &&
                psStepSize != nullptr && psNumberOfValues != nullptr &&
                psNoiseLevelValues != nullptr)
            {
                const char *calibType = CPLGetXMLValue(psCalibType, "", "");
                this->pixelFirstLutValueNoiseLevels =
                    atoi(CPLGetXMLValue(psPixelFirstNoiseValue, "", "0"));
                this->stepSizeNoiseLevels =
                    atoi(CPLGetXMLValue(psStepSize, "", "0"));
                this->numberOfValuesNoiseLevels =
                    atoi(CPLGetXMLValue(psNumberOfValues, "", "0"));
                const char *noiseLevelValues =
                    CPLGetXMLValue(psNoiseLevelValues, "", "");
                if (this->stepSizeNoiseLevels > 0 &&
                    this->numberOfValuesNoiseLevels != INT_MIN &&
                    abs(this->numberOfValuesNoiseLevels) <
                        INT_MAX / this->stepSizeNoiseLevels)
                {
                    char **papszNoiseLevelList = CSLTokenizeString2(
                        noiseLevelValues, " ", CSLT_HONOURSTRINGS);
                    /* Get the Pixel Per range */
                    this->m_nTableNoiseLevelsSize =
                        abs(this->stepSizeNoiseLevels) *
                        abs(this->numberOfValuesNoiseLevels);

                    if ((EQUAL(calibType, "Beta Nought") &&
                         this->m_eCalib == Beta0) ||
                        (EQUAL(calibType, "Sigma Nought") &&
                         this->m_eCalib == Sigma0) ||
                        (EQUAL(calibType, "Gamma") && this->m_eCalib == Gamma))
                    {
                        /* Allocate the right Noise Levels size according to the
                         * product range pixel */
                        this->m_nfTableNoiseLevels = InterpolateValues(
                            papszNoiseLevelList, this->m_nTableNoiseLevelsSize,
                            this->stepSizeNoiseLevels,
                            this->numberOfValuesNoiseLevels,
                            this->pixelFirstLutValueNoiseLevels);
                    }

                    CSLDestroy(papszNoiseLevelList);
                }

                if (this->m_nfTableNoiseLevels != nullptr)
                {
                    break;  // We are done
                }
            }
        }
    }
}

/************************************************************************/
/*                        RCMCalibRasterBand()                          */
/************************************************************************/

RCMCalibRasterBand::RCMCalibRasterBand(
    RCMDataset *poDataset, const char *pszPolarization, GDALDataType eType,
    GDALDataset *poBandDataset, eCalibration eCalib, const char *pszLUT,
    const char *pszNoiseLevels, GDALDataType eOriginalType)
    : m_eCalib(eCalib), m_poBandDataset(poBandDataset),
      m_eOriginalType(eOriginalType), m_pszLUTFile(VSIStrdup(pszLUT)),
      m_pszNoiseLevelsFile(VSIStrdup(pszNoiseLevels))
{
    this->poDS = poDataset;

    if (pszPolarization != nullptr && strlen(pszPolarization) != 0)
    {
        SetMetadataItem("POLARIMETRIC_INTERP", pszPolarization);
    }

    if ((eType == GDT_CInt16) || (eType == GDT_CFloat32))
        this->eDataType = GDT_CFloat32;
    else
        this->eDataType = GDT_Float32;

    GDALRasterBand *poRasterBand = poBandDataset->GetRasterBand(1);
    poRasterBand->GetBlockSize(&nBlockXSize, &nBlockYSize);

    ReadLUT();
    ReadNoiseLevels();
}

/************************************************************************/
/*                       ~RCMCalibRasterBand()                          */
/************************************************************************/

RCMCalibRasterBand::~RCMCalibRasterBand()
{
    CPLFree(m_nfTable);
    CPLFree(m_nfTableNoiseLevels);
    CPLFree(m_pszLUTFile);
    CPLFree(m_pszNoiseLevelsFile);
    GDALClose(m_poBandDataset);
}

/************************************************************************/
/*                        IReadBlock()                                  */
/************************************************************************/

CPLErr RCMCalibRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                      void *pImage)
{
    CPLErr eErr;
    int nRequestXSize = 0;
    int nRequestYSize = 0;
    GetActualBlockSize(nBlockXOff, nBlockYOff, &nRequestXSize, &nRequestYSize);

    // Zero initial partial right-most and bottom-most blocks
    if (nRequestXSize < nBlockXSize || nRequestYSize < nBlockYSize)
    {
        memset(pImage, 0,
               static_cast<size_t>(GDALGetDataTypeSizeBytes(eDataType)) *
                   nBlockXSize * nBlockYSize);
    }

    if (this->m_eOriginalType == GDT_CInt16)
    {
        /* read in complex values */
        GInt16 *panImageTmp = static_cast<GInt16 *>(
            VSI_MALLOC3_VERBOSE(nBlockXSize, nBlockYSize,
                                GDALGetDataTypeSizeBytes(m_eOriginalType)));
        if (!panImageTmp)
            return CE_Failure;

        if (m_poBandDataset->GetRasterCount() == 2)
        {
            eErr = m_poBandDataset->RasterIO(
                GF_Read, nBlockXOff * nBlockXSize, nBlockYOff * nBlockYSize,
                nRequestXSize, nRequestYSize, panImageTmp, nRequestXSize,
                nRequestYSize, this->m_eOriginalType, 2, nullptr, 4,
                nBlockXSize * 4, 4, nullptr);

            /*
            eErr = m_poBandDataset->RasterIO(GF_Read,
                nBlockXOff * nBlockXSize,
                nBlockYOff * nBlockYSize,
                nRequestXSize, nRequestYSize,
                panImageTmp, nRequestXSize, nRequestYSize,
                GDT_Int32,
                2, nullptr, 4, nBlockXSize * 4, 2, nullptr);
            */
        }
        else
        {
            eErr = m_poBandDataset->RasterIO(
                GF_Read, nBlockXOff * nBlockXSize, nBlockYOff * nBlockYSize,
                nRequestXSize, nRequestYSize, panImageTmp, nRequestXSize,
                nRequestYSize, this->m_eOriginalType, 1, nullptr, 4,
                nBlockXSize * 4, 0, nullptr);

            /*
            eErr =
                m_poBandDataset->RasterIO(GF_Read,
                    nBlockXOff * nBlockXSize,
                    nBlockYOff * nBlockYSize,
                    nRequestXSize, nRequestYSize,
                    panImageTmp, nRequestXSize, nRequestYSize,
                    GDT_UInt32,
                    1, nullptr, 4, nBlockXSize * 4, 0, nullptr);
            */

#ifdef CPL_LSB
            /* First, undo the 32bit swap. */
            GDALSwapWords(pImage, 4, nBlockXSize * nBlockYSize, 4);

            /* Then apply 16 bit swap. */
            GDALSwapWords(pImage, 2, nBlockXSize * nBlockYSize * 2, 2);
#endif
        }

        /* calibrate the complex values */
        for (int i = 0; i < nRequestYSize; i++)
        {
            for (int j = 0; j < nRequestXSize; j++)
            {
                /* calculate pixel offset in memory*/
                const int nPixOff = 2 * (i * nBlockXSize + j);
                const int nTruePixOff = (i * nBlockXSize) + j;

                // Formula for Complex Q+J
                const float real = static_cast<float>(panImageTmp[nPixOff]);
                const float img = static_cast<float>(panImageTmp[nPixOff + 1]);
                const float digitalValue = (real * real) + (img * img);
                const float lutValue =
                    static_cast<float>(m_nfTable[nBlockXOff * nBlockXSize + j]);
                const float calibValue = digitalValue / (lutValue * lutValue);

                reinterpret_cast<float *>(pImage)[nTruePixOff] = calibValue;
            }
        }

        CPLFree(panImageTmp);
    }

    // If the underlying file is NITF CFloat32
    else if (this->m_eOriginalType == GDT_CFloat32 ||
             this->m_eOriginalType == GDT_CFloat64)
    {
        /* read in complex values */
        const int dataTypeSize =
            GDALGetDataTypeSizeBytes(this->m_eOriginalType);
        const GDALDataType bandFileType = this->m_eOriginalType;
        const int bandFileDataTypeSize = GDALGetDataTypeSizeBytes(bandFileType);

        /* read the original image complex values in a temporary image space */
        float *pafImageTmp = static_cast<float *>(VSI_MALLOC3_VERBOSE(
            nBlockXSize, nBlockYSize, 2 * bandFileDataTypeSize));
        if (!pafImageTmp)
            return CE_Failure;

        eErr =
            // I and Q from each band are pixel-interleaved into this complex
            // band
            m_poBandDataset->RasterIO(
                GF_Read, nBlockXOff * nBlockXSize, nBlockYOff * nBlockYSize,
                nRequestXSize, nRequestYSize, pafImageTmp, nRequestXSize,
                nRequestYSize, bandFileType, 2, nullptr, dataTypeSize,
                static_cast<GSpacing>(dataTypeSize) * nBlockXSize,
                bandFileDataTypeSize, nullptr);

        /* calibrate the complex values */
        for (int i = 0; i < nRequestYSize; i++)
        {
            for (int j = 0; j < nRequestXSize; j++)
            {
                /* calculate pixel offset in memory*/
                const int nPixOff = 2 * (i * nBlockXSize + j);
                const int nTruePixOff = (i * nBlockXSize) + j;

                // Formula for Complex Q+J
                const float real = pafImageTmp[nPixOff];
                const float img = pafImageTmp[nPixOff + 1];
                const float digitalValue = (real * real) + (img * img);
                const float lutValue =
                    static_cast<float>(m_nfTable[nBlockXOff * nBlockXSize + j]);
                const float calibValue = digitalValue / (lutValue * lutValue);

                reinterpret_cast<float *>(pImage)[nTruePixOff] = calibValue;
            }
        }

        CPLFree(pafImageTmp);
    }

    else if (this->m_eOriginalType == GDT_Float32)
    {
        /* A Float32 is actual 4 bytes */
        eErr = m_poBandDataset->RasterIO(
            GF_Read, nBlockXOff * nBlockXSize, nBlockYOff * nBlockYSize,
            nRequestXSize, nRequestYSize, pImage, nRequestXSize, nRequestYSize,
            GDT_Float32, 1, nullptr, 4, nBlockXSize * 4, 0, nullptr);

        /* iterate over detected values */
        for (int i = 0; i < nRequestYSize; i++)
        {
            for (int j = 0; j < nRequestXSize; j++)
            {
                int nPixOff = (i * nBlockXSize) + j;

                /* For detected products, in order to convert the digital number
                of a given range sample to a calibrated value, the digital value
                is first squared, then the offset(B) is added and the result is
                divided by the gains value(A) corresponding to the range sample.
                RCM-SP-53-0419  Issue 2/5:  January 2, 2018  Page 7-56 */
                const float digitalValue =
                    reinterpret_cast<float *>(pImage)[nPixOff];
                const float A =
                    static_cast<float>(m_nfTable[nBlockXOff * nBlockXSize + j]);
                reinterpret_cast<float *>(pImage)[nPixOff] =
                    ((digitalValue * digitalValue) +
                     static_cast<float>(this->m_nfOffset)) /
                    A;
            }
        }
    }

    else if (this->m_eOriginalType == GDT_Float64)
    {
        /* A Float64 is actual 8 bytes */
        eErr = m_poBandDataset->RasterIO(
            GF_Read, nBlockXOff * nBlockXSize, nBlockYOff * nBlockYSize,
            nRequestXSize, nRequestYSize, pImage, nRequestXSize, nRequestYSize,
            GDT_Float64, 1, nullptr, 8, nBlockXSize * 8, 0, nullptr);

        /* iterate over detected values */
        for (int i = 0; i < nRequestYSize; i++)
        {
            for (int j = 0; j < nRequestXSize; j++)
            {
                int nPixOff = (i * nBlockXSize) + j;

                /* For detected products, in order to convert the digital number
                of a given range sample to a calibrated value, the digital value
                is first squared, then the offset(B) is added and the result is
                divided by the gains value(A) corresponding to the range sample.
                RCM-SP-53-0419  Issue 2/5:  January 2, 2018  Page 7-56 */
                const float digitalValue =
                    reinterpret_cast<float *>(pImage)[nPixOff];
                const float A =
                    static_cast<float>(m_nfTable[nBlockXOff * nBlockXSize + j]);
                reinterpret_cast<float *>(pImage)[nPixOff] =
                    ((digitalValue * digitalValue) +
                     static_cast<float>(this->m_nfOffset)) /
                    A;
            }
        }
    }

    else if (this->m_eOriginalType == GDT_UInt16)
    {
        /* read in detected values */
        GUInt16 *panImageTmp = static_cast<GUInt16 *>(VSI_MALLOC3_VERBOSE(
            nBlockXSize, nBlockYSize, GDALGetDataTypeSizeBytes(GDT_UInt16)));
        if (!panImageTmp)
            return CE_Failure;
        eErr = m_poBandDataset->RasterIO(
            GF_Read, nBlockXOff * nBlockXSize, nBlockYOff * nBlockYSize,
            nRequestXSize, nRequestYSize, panImageTmp, nRequestXSize,
            nRequestYSize, GDT_UInt16, 1, nullptr, 2, nBlockXSize * 2, 0,
            nullptr);

        /* iterate over detected values */
        for (int i = 0; i < nRequestYSize; i++)
        {
            for (int j = 0; j < nRequestXSize; j++)
            {
                const int nPixOff = (i * nBlockXSize) + j;

                const float digitalValue =
                    static_cast<float>(panImageTmp[nPixOff]);
                const float A =
                    static_cast<float>(m_nfTable[nBlockXOff * nBlockXSize + j]);
                reinterpret_cast<float *>(pImage)[nPixOff] =
                    ((digitalValue * digitalValue) +
                     static_cast<float>(this->m_nfOffset)) /
                    A;
            }
        }
        CPLFree(panImageTmp);
    } /* Ticket #2104: Support for ScanSAR products */

    else if (this->m_eOriginalType == GDT_Byte)
    {
        GByte *pabyImageTmp =
            static_cast<GByte *>(VSI_MALLOC2_VERBOSE(nBlockXSize, nBlockYSize));
        if (!pabyImageTmp)
            return CE_Failure;
        eErr = m_poBandDataset->RasterIO(
            GF_Read, nBlockXOff * nBlockXSize, nBlockYOff * nBlockYSize,
            nRequestXSize, nRequestYSize, pabyImageTmp, nRequestXSize,
            nRequestYSize, GDT_Byte, 1, nullptr, 1, nBlockXSize, 0, nullptr);

        /* iterate over detected values */
        for (int i = 0; i < nRequestYSize; i++)
        {
            for (int j = 0; j < nRequestXSize; j++)
            {
                const int nPixOff = (i * nBlockXSize) + j;

                const float digitalValue =
                    static_cast<float>(pabyImageTmp[nPixOff]);
                const float A =
                    static_cast<float>(m_nfTable[nBlockXOff * nBlockXSize + j]);
                reinterpret_cast<float *>(pImage)[nPixOff] =
                    ((digitalValue * digitalValue) +
                     static_cast<float>(this->m_nfOffset)) /
                    A;
            }
        }
        CPLFree(pabyImageTmp);
    }
    else
    {
        CPLAssert(FALSE);
        return CE_Failure;
    }
    return eErr;
}

/************************************************************************/
/* ==================================================================== */
/*                              RCMDataset                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             RCMDataset()                             */
/************************************************************************/

RCMDataset::RCMDataset()
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~RCMDataset()                             */
/************************************************************************/

RCMDataset::~RCMDataset()

{
    RCMDataset::FlushCache(true);

    if (nGCPCount > 0)
    {
        GDALDeinitGCPs(nGCPCount, pasGCPList);
        CPLFree(pasGCPList);
    }

    RCMDataset::CloseDependentDatasets();

    if (papszSubDatasets != nullptr)
        CSLDestroy(papszSubDatasets);

    if (papszExtraFiles != nullptr)
        CSLDestroy(papszExtraFiles);

    if (m_nfIncidenceAngleTable != nullptr)
        CPLFree(m_nfIncidenceAngleTable);
}

/************************************************************************/
/*                      CloseDependentDatasets()                        */
/************************************************************************/

int RCMDataset::CloseDependentDatasets()
{
    int bHasDroppedRef = GDALPamDataset::CloseDependentDatasets();

    if (nBands != 0)
        bHasDroppedRef = TRUE;

    for (int iBand = 0; iBand < nBands; iBand++)
    {
        delete papoBands[iBand];
    }
    nBands = 0;

    return bHasDroppedRef;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **RCMDataset::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();

    papszFileList = CSLInsertStrings(papszFileList, -1, papszExtraFiles);

    return papszFileList;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *RCMDataset::Open(GDALOpenInfo *poOpenInfo)

{
    /* -------------------------------------------------------------------- */
    /*      Is this a RCM Product.xml definition?                           */
    /* -------------------------------------------------------------------- */
    if (!RCMDatasetIdentify(poOpenInfo))
    {
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*        Get subdataset information, if relevant                       */
    /* -------------------------------------------------------------------- */
    const char *pszFilename = poOpenInfo->pszFilename;
    eCalibration eCalib = None;

    if (STARTS_WITH_CI(pszFilename, szLayerCalibration) &&
        pszFilename[strlen(szLayerCalibration)] == chLayerSeparator)
    {
        // The calibration name and filename begins after the hard coded layer
        // name
        pszFilename += strlen(szLayerCalibration) + 1;

        if (STARTS_WITH_CI(pszFilename, szBETA0))
        {
            eCalib = Beta0;
        }
        else if (STARTS_WITH_CI(pszFilename, szSIGMA0))
        {
            eCalib = Sigma0;
        }
        else if (STARTS_WITH_CI(pszFilename, szGAMMA) ||
                 STARTS_WITH_CI(pszFilename, "GAMMA0"))  // Cover both situation
        {
            eCalib = Gamma;
        }
        else if (STARTS_WITH_CI(pszFilename, szUNCALIB))
        {
            eCalib = Uncalib;
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported calibration type");
            return nullptr;
        }

        /* advance the pointer to the actual filename */
        while (*pszFilename != '\0' && *pszFilename != ':')
            pszFilename++;

        if (*pszFilename == ':')
            pszFilename++;

        // need to redo the directory check:
        // the GDALOpenInfo check would have failed because of the calibration
        // string on the filename
        VSIStatBufL sStat;
        if (VSIStatL(pszFilename, &sStat) == 0)
            poOpenInfo->bIsDirectory = VSI_ISDIR(sStat.st_mode);
    }

    CPLString osMDFilename;
    if (poOpenInfo->bIsDirectory)
    {
        /* Check for directory access when there is a product.xml file in the
        directory. */
        osMDFilename =
            CPLFormCIFilenameSafe(pszFilename, "product.xml", nullptr);

        VSIStatBufL sStat;
        if (VSIStatL(osMDFilename, &sStat) != 0)
        {
            /* If not, check for directory extra 'metadata' access when there is
            a product.xml file in the directory. */
            osMDFilename = CPLFormCIFilenameSafe(pszFilename,
                                                 GetMetadataProduct(), nullptr);
        }
    }
    else
        osMDFilename = pszFilename;

    /* -------------------------------------------------------------------- */
    /*      Ingest the Product.xml file.                                    */
    /* -------------------------------------------------------------------- */
    CPLXMLTreeCloser psProduct(CPLParseXMLFile(osMDFilename));
    if (!psProduct)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Confirm the requested access is supported.                      */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->eAccess == GA_Update)
    {
        ReportUpdateNotSupportedByDriver("RCM");
        return nullptr;
    }

    CPLXMLNode *psSceneAttributes =
        CPLGetXMLNode(psProduct.get(), "=product.sceneAttributes");
    if (psSceneAttributes == nullptr)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "ERROR: Failed to find <sceneAttributes> in document.");
        return nullptr;
    }

    CPLXMLNode *psImageAttributes = CPLGetXMLNode(
        psProduct.get(), "=product.sceneAttributes.imageAttributes");
    if (psImageAttributes == nullptr)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "ERROR: Failed to find <sceneAttributes.imageAttributes> in "
                 "document.");
        return nullptr;
    }

    int numberOfEntries =
        atoi(CPLGetXMLValue(psSceneAttributes, "numberOfEntries", "0"));
    if (numberOfEntries != 1)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "ERROR: Only RCM with Complex Single-beam is supported.");
        return nullptr;
    }

    CPLXMLNode *psImageReferenceAttributes =
        CPLGetXMLNode(psProduct.get(), "=product.imageReferenceAttributes");
    if (psImageReferenceAttributes == nullptr)
    {
        CPLError(
            CE_Failure, CPLE_OpenFailed,
            "ERROR: Failed to find <imageReferenceAttributes> in document.");
        return nullptr;
    }

    CPLXMLNode *psImageGenerationParameters =
        CPLGetXMLNode(psProduct.get(), "=product.imageGenerationParameters");
    if (psImageGenerationParameters == nullptr)
    {
        CPLError(
            CE_Failure, CPLE_OpenFailed,
            "ERROR: Failed to find <imageGenerationParameters> in document.");
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Create the dataset.                                             */
    /* -------------------------------------------------------------------- */
    auto poDS = std::make_unique<RCMDataset>();

    poDS->psProduct = std::move(psProduct);

    /* -------------------------------------------------------------------- */
    /*      Get overall image information.                                  */
    /* -------------------------------------------------------------------- */
    poDS->nRasterXSize = atoi(CPLGetXMLValue(
        psSceneAttributes, "imageAttributes.samplesPerLine", "-1"));
    poDS->nRasterYSize = atoi(
        CPLGetXMLValue(psSceneAttributes, "imageAttributes.numLines", "-1"));
    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Check product type, as to determine if there are LUTs for       */
    /*      calibration purposes.                                           */
    /* -------------------------------------------------------------------- */

    const char *pszItem =
        CPLGetXMLValue(psImageGenerationParameters,
                       "generalProcessingInformation.productType", "UNK");
    poDS->SetMetadataItem("PRODUCT_TYPE", pszItem);
    const char *pszProductType = pszItem;

    pszItem =
        CPLGetXMLValue(poDS->psProduct.get(), "=product.productId", "UNK");
    poDS->SetMetadataItem("PRODUCT_ID", pszItem);

    pszItem = CPLGetXMLValue(
        poDS->psProduct.get(),
        "=product.securityAttributes.securityClassification", "UNK");
    poDS->SetMetadataItem("SECURITY_CLASSIFICATION", pszItem);

    pszItem =
        CPLGetXMLValue(poDS->psProduct.get(),
                       "=product.sourceAttributes.polarizationDataMode", "UNK");
    poDS->SetMetadataItem("POLARIZATION_DATA_MODE", pszItem);

    pszItem = CPLGetXMLValue(psImageGenerationParameters,
                             "generalProcessingInformation.processingFacility",
                             "UNK");
    poDS->SetMetadataItem("PROCESSING_FACILITY", pszItem);

    pszItem =
        CPLGetXMLValue(psImageGenerationParameters,
                       "generalProcessingInformation.processingTime", "UNK");
    poDS->SetMetadataItem("PROCESSING_TIME", pszItem);

    pszItem = CPLGetXMLValue(psImageGenerationParameters,
                             "sarProcessingInformation.satelliteHeight", "UNK");
    poDS->SetMetadataItem("SATELLITE_HEIGHT", pszItem);

    pszItem = CPLGetXMLValue(
        psImageGenerationParameters,
        "sarProcessingInformation.zeroDopplerTimeFirstLine", "UNK");
    poDS->SetMetadataItem("FIRST_LINE_TIME", pszItem);

    pszItem = CPLGetXMLValue(psImageGenerationParameters,
                             "sarProcessingInformation.zeroDopplerTimeLastLine",
                             "UNK");
    poDS->SetMetadataItem("LAST_LINE_TIME", pszItem);

    pszItem = CPLGetXMLValue(psImageGenerationParameters,
                             "sarProcessingInformation.lutApplied", "");
    poDS->SetMetadataItem("LUT_APPLIED", pszItem);

    /*---------------------------------------------------------------------
           If true, a polarization dependent application LUT has been applied
           for each polarization channel. Otherwise the same application LUT
           has been applied for all polarization channels. Not applicable to
           lookupTable = "Unity*" or if dataType = "Floating-Point".
      --------------------------------------------------------------------- */
    pszItem = CPLGetXMLValue(psImageGenerationParameters,
                             "sarProcessingInformation.perPolarizationScaling",
                             "false");
    poDS->SetMetadataItem("PER_POLARIZATION_SCALING", pszItem);
    if (EQUAL(pszItem, "true") || EQUAL(pszItem, "TRUE"))
    {
        poDS->bPerPolarizationScaling = TRUE;
    }

    /* the following cases can be assumed to have no LUTs, as per
     * RN-RP-51-2713, but also common sense
     * SLC represents a SLant range georeferenced Complex product
     * (i.e., equivalent to a Single-Look Complex product for RADARSAT-1 or
     * RADARSAT-2). • GRD or GRC represent GRound range georeferenced Detected
     * or Complex products (GRD is equivalent to an SGX, SCN or SCW product for
     * RADARSAT1 or RADARSAT-2). • GCD or GCC represent GeoCoded Detected or
     * Complex products (GCD is equivalent to an SSG or SPG product for
     * RADARSAT-1 or RADARSAT-2).
     */
    bool bCanCalib = false;
    if (!(STARTS_WITH_CI(pszProductType, "UNK") ||
          STARTS_WITH_CI(pszProductType, "GCD") ||
          STARTS_WITH_CI(pszProductType, "GCC")))
    {
        bCanCalib = true;
    }

    /* -------------------------------------------------------------------- */
    /*      Get dataType (so we can recognise complex data), and the        */
    /*      bitsPerSample.                                                  */
    /* -------------------------------------------------------------------- */
    const char *pszSampleDataType = CPLGetXMLValue(
        psImageReferenceAttributes, "rasterAttributes.sampleType", "");
    poDS->SetMetadataItem("SAMPLE_TYPE", pszSampleDataType);

    /* Either Integer (16 bits) or Floating-Point (32 bits) */
    const char *pszDataType = CPLGetXMLValue(psImageReferenceAttributes,
                                             "rasterAttributes.dataType", "");
    poDS->SetMetadataItem("DATA_TYPE", pszDataType);

    /* 2 entries for complex data 1 entry for magnitude detected data */
    const char *pszBitsPerSample = CPLGetXMLValue(
        psImageReferenceAttributes, "rasterAttributes.bitsPerSample", "");
    const int nBitsPerSample = atoi(pszBitsPerSample);
    poDS->SetMetadataItem("BITS_PER_SAMPLE", pszBitsPerSample);

    const char *pszSampledPixelSpacingTime =
        CPLGetXMLValue(psImageReferenceAttributes,
                       "rasterAttributes.sampledPixelSpacingTime", "UNK");
    poDS->SetMetadataItem("SAMPLED_PIXEL_SPACING_TIME",
                          pszSampledPixelSpacingTime);

    const char *pszSampledLineSpacingTime =
        CPLGetXMLValue(psImageReferenceAttributes,
                       "rasterAttributes.sampledLineSpacingTime", "UNK");
    poDS->SetMetadataItem("SAMPLED_LINE_SPACING_TIME",
                          pszSampledLineSpacingTime);

    GDALDataType eDataType;

    if (EQUAL(pszSampleDataType, "Mixed")) /* RCM MLC has Mixed sampleType */
    {
        poDS->isComplexData = false; /* RCM MLC is detected, non-complex */
        if (nBitsPerSample == 32)
        {
            eDataType = GDT_Float32; /* 32 bits, check read block */
            poDS->magnitudeBits = 32;
        }
        else
        {
            eDataType = GDT_UInt16; /* 16 bits, check read block */
            poDS->magnitudeBits = 16;
        }
    }
    else if (EQUAL(pszSampleDataType, "Complex"))
    {
        poDS->isComplexData = true;
        /* Usually this is the same bits for both */
        poDS->realBitsComplexData = nBitsPerSample;
        poDS->imaginaryBitsComplexData = nBitsPerSample;

        if (nBitsPerSample == 32)
        {
            eDataType = GDT_CFloat32; /* 32 bits, check read block */
        }
        else
        {
            eDataType = GDT_CInt16; /* 16 bits, check read block */
        }
    }
    else if (nBitsPerSample == 32 &&
             EQUAL(pszSampleDataType, "Magnitude Detected"))
    {
        /* Actually, I don't need to test the 'dataType'=' Floating-Point', we
         * know that a 32 bits per sample */
        eDataType = GDT_Float32;
        poDS->isComplexData = false;
        poDS->magnitudeBits = 32;
    }
    else if (nBitsPerSample == 16 &&
             EQUAL(pszSampleDataType, "Magnitude Detected"))
    {
        /* Actually, I don't need to test the 'dataType'=' Integer', we know
         * that a 16 bits per sample */
        eDataType = GDT_UInt16;
        poDS->isComplexData = false;
        poDS->magnitudeBits = 16;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "ERROR: dataType=%s and bitsPerSample=%d are not a supported "
                 "configuration.",
                 pszDataType, nBitsPerSample);
        return nullptr;
    }

    /* Indicates whether pixel number (i.e., range) increases or decreases with
    range time. For GCD and GCC products, this applies to intermediate ground
    range image data prior to geocoding. */
    const char *pszPixelTimeOrdering =
        CPLGetXMLValue(psImageReferenceAttributes,
                       "rasterAttributes.pixelTimeOrdering", "UNK");
    poDS->SetMetadataItem("PIXEL_TIME_ORDERING", pszPixelTimeOrdering);

    /* Indicates whether line numbers (i.e., azimuth) increase or decrease with
    azimuth time. For GCD and GCC products, this applies to intermediate ground
    range image data prior to geocoding. */
    const char *pszLineTimeOrdering = CPLGetXMLValue(
        psImageReferenceAttributes, "rasterAttributes.lineTimeOrdering", "UNK");
    poDS->SetMetadataItem("LINE_TIME_ORDERING", pszLineTimeOrdering);

    /* while we're at it, extract the pixel spacing information */
    const char *pszPixelSpacing =
        CPLGetXMLValue(psImageReferenceAttributes,
                       "rasterAttributes.sampledPixelSpacing", "UNK");
    poDS->SetMetadataItem("PIXEL_SPACING", pszPixelSpacing);

    const char *pszLineSpacing =
        CPLGetXMLValue(psImageReferenceAttributes,
                       "rasterAttributes.sampledLineSpacing", "UNK");
    poDS->SetMetadataItem("LINE_SPACING", pszLineSpacing);

    /* -------------------------------------------------------------------- */
    /*      Open each of the data files as a complex band.                  */
    /* -------------------------------------------------------------------- */
    char *pszBeta0LUT = nullptr;
    char *pszGammaLUT = nullptr;
    char *pszSigma0LUT = nullptr;
    // Same file for all calibrations except the calibration is plit inside the
    // XML
    std::string osNoiseLevelsValues;

    const CPLString osPath = CPLGetPathSafe(osMDFilename);

    /* Get a list of all polarizations */
    CPLXMLNode *psSourceAttrs =
        CPLGetXMLNode(poDS->psProduct.get(), "=product.sourceAttributes");
    if (psSourceAttrs == nullptr)
    {
        CPLError(
            CE_Failure, CPLE_OpenFailed,
            "ERROR: RCM source attributes is missing. Please contact your data "
            "provider for a corrected dataset.");
        return nullptr;
    }

    CPLXMLNode *psRadarParameters = CPLGetXMLNode(
        poDS->psProduct.get(), "=product.sourceAttributes.radarParameters");
    if (psRadarParameters == nullptr)
    {
        CPLError(
            CE_Failure, CPLE_OpenFailed,
            "ERROR: RCM radar parameters is missing. Please contact your data "
            "provider for a corrected dataset.");
        return nullptr;
    }

    const char *pszPolarizations =
        CPLGetXMLValue(psRadarParameters, "polarizations", "");
    if (pszPolarizations == nullptr || strlen(pszPolarizations) == 0)
    {
        CPLError(
            CE_Failure, CPLE_OpenFailed,
            "ERROR: RCM polarizations list is missing. Please contact your "
            "data provider for a corrected dataset.");
        return nullptr;
    }
    poDS->SetMetadataItem("POLARIZATIONS", pszPolarizations);

    const char *psAcquisitionType =
        CPLGetXMLValue(psRadarParameters, "acquisitionType", "UNK");
    poDS->SetMetadataItem("ACQUISITION_TYPE", psAcquisitionType);

    const char *psBeams = CPLGetXMLValue(psRadarParameters, "beams", "UNK");
    poDS->SetMetadataItem("BEAMS", psBeams);

    const CPLStringList aosPolarizationsGrids(
        CSLTokenizeString2(pszPolarizations, " ", 0));
    CPLStringList imageBandList;
    CPLStringList imageBandFileList;
    const int nPolarizationsGridCount = aosPolarizationsGrids.size();

    /* File names for full resolution IPDFs. For GeoTIFF format, one entry per
    pole; For NITF 2.1 format, only one entry. */
    bool bIsNITF = false;
    const char *pszNITF_Filename = nullptr;
    int imageBandFileCount = 0;
    int imageBandCount = 0;

    /* Split the polarization string*/
    auto iss = std::istringstream((CPLString(pszPolarizations)).c_str());
    auto pol = std::string{};
    /* Count number of polarizations*/
    while (iss >> pol)
        imageBandCount++;

    CPLXMLNode *psNode = psImageAttributes->psChild;
    for (; psNode != nullptr; psNode = psNode->psNext)
    {
        /* Find the tif or ntf filename */
        if (psNode->eType != CXT_Element || !(EQUAL(psNode->pszValue, "ipdf")))
            continue;

        /* --------------------------------------------------------------------
         */
        /*      Fetch ipdf image. Could be either tif or ntf. */
        /*      Replace / by \\ */
        /* --------------------------------------------------------------------
         */
        const char *pszBasedFilename = CPLGetXMLValue(psNode, "", "");
        if (*pszBasedFilename == '\0')
            continue;

        /* Count number of image names within ipdf tag*/
        imageBandFileCount++;

        CPLString pszUpperBasedFilename(CPLString(pszBasedFilename).toupper());

        const bool bEndsWithNTF =
            strlen(pszUpperBasedFilename.c_str()) > 4 &&
            EQUAL(pszUpperBasedFilename.c_str() +
                      strlen(pszUpperBasedFilename.c_str()) - 4,
                  ".NTF");

        if (bEndsWithNTF)
        {
            /* Found it! It would not exist one more */
            bIsNITF = true;
            pszNITF_Filename = pszBasedFilename;
            break;
        }
        else
        {
            /* Keep adding polarizations filename according to the pole */
            const char *pszPole = CPLGetXMLValue(psNode, "pole", "");
            if (*pszPole == '\0')
            {
                /* Guard against case when pole is a null string, skip it */
                imageBandFileCount--;
                continue;
            }

            if (EQUAL(pszPole, "XC"))
            {
                /* Skip RCM MLC's 3rd band file ##XC.tif */
                imageBandFileCount--;
                continue;
            }

            imageBandList.AddString(CPLString(pszPole).toupper());
            imageBandFileList.AddString(pszBasedFilename);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Incidence Angle in a sub-folder                                 */
    /*      called 'calibration' from the 'metadata' folder                 */
    /* -------------------------------------------------------------------- */

    const char *pszIncidenceAngleFileName = CPLGetXMLValue(
        psImageReferenceAttributes, "incidenceAngleFileName", "");

    if (pszIncidenceAngleFileName != nullptr)
    {
        CPLString osIncidenceAngleFilePath = CPLFormFilenameSafe(
            CPLFormFilenameSafe(osPath, CALIBRATION_FOLDER, nullptr).c_str(),
            pszIncidenceAngleFileName, nullptr);

        /* Check if the file exist */
        if (IsValidXMLFile(osIncidenceAngleFilePath))
        {
            CPLXMLTreeCloser psIncidenceAngle(
                CPLParseXMLFile(osIncidenceAngleFilePath));

            int pixelFirstLutValue = atoi(
                CPLGetXMLValue(psIncidenceAngle.get(),
                               "=incidenceAngles.pixelFirstAnglesValue", "0"));

            const int stepSize = atoi(CPLGetXMLValue(
                psIncidenceAngle.get(), "=incidenceAngles.stepSize", "0"));
            const int numberOfValues =
                atoi(CPLGetXMLValue(psIncidenceAngle.get(),
                                    "=incidenceAngles.numberOfValues", "0"));

            if (!(stepSize == 0 || stepSize == INT_MIN ||
                  numberOfValues == INT_MIN ||
                  abs(numberOfValues) > INT_MAX / abs(stepSize)))
            {
                /* Get the Pixel Per range */
                const int tableSize = abs(stepSize) * abs(numberOfValues);

                CPLString angles;
                // Loop through all nodes with spaces
                CPLXMLNode *psNextNode =
                    CPLGetXMLNode(psIncidenceAngle.get(), "=incidenceAngles");

                CPLXMLNode *psNodeInc;
                for (psNodeInc = psNextNode->psChild; psNodeInc != nullptr;
                     psNodeInc = psNodeInc->psNext)
                {
                    if (EQUAL(psNodeInc->pszValue, "angles"))
                    {
                        if (angles.length() > 0)
                        {
                            angles.append(" "); /* separator */
                        }
                        const char *valAngle =
                            CPLGetXMLValue(psNodeInc, "", "");
                        angles.append(valAngle);
                    }
                }

                char **papszAngleList =
                    CSLTokenizeString2(angles, " ", CSLT_HONOURSTRINGS);

                /* Allocate the right LUT size according to the product range pixel
                 */
                poDS->m_IncidenceAngleTableSize = tableSize;
                poDS->m_nfIncidenceAngleTable =
                    InterpolateValues(papszAngleList, tableSize, stepSize,
                                      numberOfValues, pixelFirstLutValue);

                CSLDestroy(papszAngleList);
            }
        }
    }

    for (int iPoleInx = 0; iPoleInx < nPolarizationsGridCount; iPoleInx++)
    {
        // Search for a specific band name
        const CPLString pszPole =
            CPLString(aosPolarizationsGrids[iPoleInx]).toupper();

        // Look if the NoiseLevel file xml exist for the
        CPLXMLNode *psRefNode = psImageReferenceAttributes->psChild;
        for (; psRefNode != nullptr; psRefNode = psRefNode->psNext)
        {
            if (EQUAL(psRefNode->pszValue, "noiseLevelFileName") && bCanCalib)
            {
                /* Determine which incidence angle correction this is */
                const char *pszPoleToMatch =
                    CPLGetXMLValue(psRefNode, "pole", "");
                const char *pszNoiseLevelFile =
                    CPLGetXMLValue(psRefNode, "", "");

                if (*pszPoleToMatch == '\0')
                    continue;

                if (EQUAL(pszPoleToMatch, "XC"))
                    /* Skip noise for RCM MLC's 3rd band XC */
                    continue;

                if (EQUAL(pszNoiseLevelFile, ""))
                    continue;

                /* --------------------------------------------------------------------
                 */
                /*      With RCM, LUT file is different per polarizarion (pole)
                 */
                /*      The following code make sure to loop through all
                 * possible       */
                /*      'noiseLevelFileName' and match the <ipdf 'pole' name */
                /* --------------------------------------------------------------------
                 */
                if (pszPole.compare(pszPoleToMatch) != 0)
                {
                    continue;
                }

                /* --------------------------------------------------------------------
                 */
                /*      LUT calibration is unique per pole in a sub-folder */
                /*      called 'calibration' from the 'metadata' folder */
                /* --------------------------------------------------------------------
                 */

                const CPLString oNoiseLevelPath = CPLFormFilenameSafe(
                    CPLFormFilenameSafe(osPath, CALIBRATION_FOLDER, nullptr)
                        .c_str(),
                    pszNoiseLevelFile, nullptr);
                if (IsValidXMLFile(oNoiseLevelPath))
                {
                    osNoiseLevelsValues = oNoiseLevelPath;
                }
            }
        }

        // Search again with different file
        psRefNode = psImageReferenceAttributes->psChild;
        for (; psRefNode != nullptr; psRefNode = psRefNode->psNext)
        {
            if (EQUAL(psRefNode->pszValue, "lookupTableFileName") && bCanCalib)
            {
                /* Determine which incidence angle correction this is */
                const char *pszLUTType =
                    CPLGetXMLValue(psRefNode, "sarCalibrationType", "");
                const char *pszPoleToMatch =
                    CPLGetXMLValue(psRefNode, "pole", "");
                const char *pszLUTFile = CPLGetXMLValue(psRefNode, "", "");

                if (*pszPoleToMatch == '\0')
                    continue;

                if (EQUAL(pszPoleToMatch, "XC"))
                    /* Skip Calib for RCM MLC's 3rd band XC */
                    continue;

                if (*pszLUTType == '\0')
                    continue;

                if (EQUAL(pszLUTType, ""))
                    continue;

                /* --------------------------------------------------------------------
                 */
                /*      With RCM, LUT file is different per polarizarion (pole)
                 */
                /*      The following code make sure to loop through all
                 * possible       */
                /*      'lookupTableFileName' and match the <ipdf 'pole' name */
                /* --------------------------------------------------------------------
                 */
                if (pszPole.compare(pszPoleToMatch) != 0)
                {
                    continue;
                }

                /* --------------------------------------------------------------------
                 */
                /*      LUT calibration is unique per pole in a sub-folder */
                /*      called 'calibration' from the 'metadata' folder */
                /* --------------------------------------------------------------------
                 */
                const CPLString osLUTFilePath = CPLFormFilenameSafe(
                    CPLFormFilenameSafe(osPath, CALIBRATION_FOLDER, nullptr)
                        .c_str(),
                    pszLUTFile, nullptr);

                if (EQUAL(pszLUTType, "Beta Nought") &&
                    IsValidXMLFile(osLUTFilePath))
                {
                    poDS->papszExtraFiles =
                        CSLAddString(poDS->papszExtraFiles, osLUTFilePath);

                    CPLString pszBuf(
                        FormatCalibration(szBETA0, osMDFilename.c_str()));
                    CPLFree(pszBeta0LUT);
                    pszBeta0LUT = VSIStrdup(osLUTFilePath);

                    const char *oldLut =
                        poDS->GetMetadataItem("BETA_NOUGHT_LUT");
                    if (oldLut == nullptr)
                    {
                        poDS->SetMetadataItem("BETA_NOUGHT_LUT", osLUTFilePath);
                    }
                    else
                    {
                        /* Keep adding LUT file for all bands, should be planty
                         * of space */
                        char *ptrConcatLut =
                            static_cast<char *>(CPLMalloc(2048));
                        ptrConcatLut[0] =
                            '\0'; /* Just initialize the first byte */
                        strcat(ptrConcatLut, oldLut);
                        strcat(ptrConcatLut, ",");
                        strcat(ptrConcatLut, osLUTFilePath);
                        poDS->SetMetadataItem("BETA_NOUGHT_LUT", ptrConcatLut);
                        CPLFree(ptrConcatLut);
                    }

                    poDS->papszSubDatasets = CSLSetNameValue(
                        poDS->papszSubDatasets, "SUBDATASET_3_NAME", pszBuf);
                    poDS->papszSubDatasets = CSLSetNameValue(
                        poDS->papszSubDatasets, "SUBDATASET_3_DESC",
                        "Beta Nought calibrated");
                }
                else if (EQUAL(pszLUTType, "Sigma Nought") &&
                         IsValidXMLFile(osLUTFilePath))
                {
                    poDS->papszExtraFiles =
                        CSLAddString(poDS->papszExtraFiles, osLUTFilePath);

                    CPLString pszBuf(
                        FormatCalibration(szSIGMA0, osMDFilename.c_str()));
                    CPLFree(pszSigma0LUT);
                    pszSigma0LUT = VSIStrdup(osLUTFilePath);

                    const char *oldLut =
                        poDS->GetMetadataItem("SIGMA_NOUGHT_LUT");
                    if (oldLut == nullptr)
                    {
                        poDS->SetMetadataItem("SIGMA_NOUGHT_LUT",
                                              osLUTFilePath);
                    }
                    else
                    {
                        /* Keep adding LUT file for all bands, should be planty
                         * of space */
                        char *ptrConcatLut =
                            static_cast<char *>(CPLMalloc(2048));
                        ptrConcatLut[0] =
                            '\0'; /* Just initialize the first byte */
                        strcat(ptrConcatLut, oldLut);
                        strcat(ptrConcatLut, ",");
                        strcat(ptrConcatLut, osLUTFilePath);
                        poDS->SetMetadataItem("SIGMA_NOUGHT_LUT", ptrConcatLut);
                        CPLFree(ptrConcatLut);
                    }

                    poDS->papszSubDatasets = CSLSetNameValue(
                        poDS->papszSubDatasets, "SUBDATASET_2_NAME", pszBuf);
                    poDS->papszSubDatasets = CSLSetNameValue(
                        poDS->papszSubDatasets, "SUBDATASET_2_DESC",
                        "Sigma Nought calibrated");
                }
                else if (EQUAL(pszLUTType, "Gamma") &&
                         IsValidXMLFile(osLUTFilePath))
                {
                    poDS->papszExtraFiles =
                        CSLAddString(poDS->papszExtraFiles, osLUTFilePath);

                    CPLString pszBuf(
                        FormatCalibration(szGAMMA, osMDFilename.c_str()));
                    CPLFree(pszGammaLUT);
                    pszGammaLUT = VSIStrdup(osLUTFilePath);

                    const char *oldLut = poDS->GetMetadataItem("GAMMA_LUT");
                    if (oldLut == nullptr)
                    {
                        poDS->SetMetadataItem("GAMMA_LUT", osLUTFilePath);
                    }
                    else
                    {
                        /* Keep adding LUT file for all bands, should be planty
                         * of space */
                        char *ptrConcatLut =
                            static_cast<char *>(CPLMalloc(2048));
                        ptrConcatLut[0] =
                            '\0'; /* Just initialize the first byte */
                        strcat(ptrConcatLut, oldLut);
                        strcat(ptrConcatLut, ",");
                        strcat(ptrConcatLut, osLUTFilePath);
                        poDS->SetMetadataItem("GAMMA_LUT", ptrConcatLut);
                        CPLFree(ptrConcatLut);
                    }

                    poDS->papszSubDatasets = CSLSetNameValue(
                        poDS->papszSubDatasets, "SUBDATASET_4_NAME", pszBuf);
                    poDS->papszSubDatasets = CSLSetNameValue(
                        poDS->papszSubDatasets, "SUBDATASET_4_DESC",
                        "Gamma calibrated");
                }
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      Fetch ipdf image. Could be either tif or ntf. */
        /*      Replace / by \\ */
        /* --------------------------------------------------------------------
         */
        const char *pszBasedFilename;
        if (bIsNITF)
        {
            pszBasedFilename = pszNITF_Filename;
        }
        else
        {
            const int bandPositionIndex = imageBandList.FindString(pszPole);
            if (bandPositionIndex < 0)
            {
                CPLError(CE_Failure, CPLE_OpenFailed,
                         "ERROR: RCM cannot find the polarization %s. Please "
                         "contact your data provider for a corrected dataset",
                         pszPole.c_str());
                return nullptr;
            }

            pszBasedFilename = imageBandFileList[bandPositionIndex];
        }

        /* --------------------------------------------------------------------
         */
        /*      Form full filename (path of product.xml + basename). */
        /* --------------------------------------------------------------------
         */
        char *pszFullname = CPLStrdup(
            CPLFormFilenameSafe(osPath, pszBasedFilename, nullptr).c_str());

        /* --------------------------------------------------------------------
         */
        /*      Try and open the file. */
        /* --------------------------------------------------------------------
         */
        auto poBandFile = std::unique_ptr<GDALDataset>(GDALDataset::Open(
            pszFullname, GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR));
        if (poBandFile == nullptr)
        {
            CPLFree(pszFullname);
            continue;
        }
        if (poBandFile->GetRasterCount() == 0)
        {
            CPLFree(pszFullname);
            continue;
        }

        poDS->papszExtraFiles =
            CSLAddString(poDS->papszExtraFiles, pszFullname);

        /* Some CFloat32 NITF files have nBitsPerSample incorrectly reported */
        /* as 16, and get misinterpreted as CInt16.  Check the underlying NITF
         */
        /* and override if this is the case. */
        if (poBandFile->GetRasterBand(1)->GetRasterDataType() == GDT_CFloat32)
            eDataType = GDT_CFloat32;

        BandMappingRCM b =
            checkBandFileMappingRCM(eDataType, poBandFile.get(), bIsNITF);
        if (b == BANDERROR)
        {
            CPLFree(pszFullname);
            CPLError(CE_Failure, CPLE_AppDefined,
                     "The underlying band files do not have an appropriate "
                     "data type.");
            return nullptr;
        }
        bool twoBandComplex = b == TWOBANDCOMPLEX;
        bool isOneFilePerPol = (imageBandCount == imageBandFileCount);

        /* --------------------------------------------------------------------
         */
        /*      Create the band. */
        /* --------------------------------------------------------------------
         */
        int bandNum = poDS->GetRasterCount() + 1;
        if (eCalib == None || eCalib == Uncalib)
        {
            RCMRasterBand *poBand = new RCMRasterBand(
                poDS.get(), bandNum, eDataType, pszPole, poBandFile.release(),
                twoBandComplex, isOneFilePerPol, bIsNITF);

            poDS->SetBand(poDS->GetRasterCount() + 1, poBand);
        }
        else
        {
            const char *pszLUT = nullptr;
            switch (eCalib)
            {
                case Sigma0:
                    pszLUT = pszSigma0LUT;
                    break;
                case Beta0:
                    pszLUT = pszBeta0LUT;
                    break;
                case Gamma:
                    pszLUT = pszGammaLUT;
                    break;
                default:
                    /* we should bomb gracefully... */
                    pszLUT = pszSigma0LUT;
            }
            if (!pszLUT)
            {
                CPLFree(pszFullname);
                CPLError(CE_Failure, CPLE_AppDefined, "LUT missing.");
                return nullptr;
            }

            // The variable 'osNoiseLevelsValues' is always the same for a ban
            // name except the XML contains different calibration name
            if (poDS->isComplexData)
            {
                // If Complex, always 32 bits
                RCMCalibRasterBand *poBand = new RCMCalibRasterBand(
                    poDS.get(), pszPole, GDT_Float32, poBandFile.release(),
                    eCalib, pszLUT, osNoiseLevelsValues.c_str(), eDataType);
                poDS->SetBand(poDS->GetRasterCount() + 1, poBand);
            }
            else
            {
                // Whatever the datatype was previoulsy set
                RCMCalibRasterBand *poBand = new RCMCalibRasterBand(
                    poDS.get(), pszPole, eDataType, poBandFile.release(),
                    eCalib, pszLUT, osNoiseLevelsValues.c_str(), eDataType);
                poDS->SetBand(poDS->GetRasterCount() + 1, poBand);
            }
        }

        CPLFree(pszFullname);
    }

    if (poDS->papszSubDatasets != nullptr && eCalib == None)
    {
        CPLString pszBuf = FormatCalibration(szUNCALIB, osMDFilename.c_str());
        poDS->papszSubDatasets = CSLSetNameValue(poDS->papszSubDatasets,
                                                 "SUBDATASET_1_NAME", pszBuf);
        poDS->papszSubDatasets =
            CSLSetNameValue(poDS->papszSubDatasets, "SUBDATASET_1_DESC",
                            "Uncalibrated digital numbers");
    }
    else if (poDS->papszSubDatasets != nullptr)
    {
        CSLDestroy(poDS->papszSubDatasets);
        poDS->papszSubDatasets = nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Set the appropriate MATRIX_REPRESENTATION.                      */
    /* -------------------------------------------------------------------- */

    if (poDS->GetRasterCount() == 4 &&
        (eDataType == GDT_CInt16 || eDataType == GDT_CFloat32))
    {
        poDS->SetMetadataItem("MATRIX_REPRESENTATION", "SCATTERING");
    }

    /* -------------------------------------------------------------------- */
    /*      Collect a few useful metadata items.                            */
    /* -------------------------------------------------------------------- */

    pszItem = CPLGetXMLValue(psSourceAttrs, "satellite", "");
    poDS->SetMetadataItem("SATELLITE_IDENTIFIER", pszItem);

    pszItem = CPLGetXMLValue(psSourceAttrs, "sensor", "");
    poDS->SetMetadataItem("SENSOR_IDENTIFIER", pszItem);

    /* Get beam mode mnemonic */
    pszItem = CPLGetXMLValue(psSourceAttrs, "beamMode", "UNK");
    poDS->SetMetadataItem("BEAM_MODE", pszItem);

    pszItem = CPLGetXMLValue(psSourceAttrs, "beamModeMnemonic", "UNK");
    poDS->SetMetadataItem("BEAM_MODE_MNEMONIC", pszItem);

    pszItem = CPLGetXMLValue(psSourceAttrs, "beamModeDefinitionId", "UNK");
    poDS->SetMetadataItem("BEAM_MODE_DEFINITION_ID", pszItem);

    pszItem = CPLGetXMLValue(psSourceAttrs, "rawDataStartTime", "UNK");
    poDS->SetMetadataItem("ACQUISITION_START_TIME", pszItem);

    pszItem = CPLGetXMLValue(psSourceAttrs, "inputDatasetFacilityId", "UNK");
    poDS->SetMetadataItem("FACILITY_IDENTIFIER", pszItem);

    pszItem = CPLGetXMLValue(psSourceAttrs,
                             "orbitAndAttitude.orbitInformation.passDirection",
                             "UNK");
    poDS->SetMetadataItem("ORBIT_DIRECTION", pszItem);
    pszItem = CPLGetXMLValue(
        psSourceAttrs, "orbitAndAttitude.orbitInformation.orbitDataSource",
        "UNK");
    poDS->SetMetadataItem("ORBIT_DATA_SOURCE", pszItem);
    pszItem = CPLGetXMLValue(
        psSourceAttrs, "orbitAndAttitude.orbitInformation.orbitDataFileName",
        "UNK");
    poDS->SetMetadataItem("ORBIT_DATA_FILE", pszItem);

    /* Get incidence angle information. DONE */
    pszItem = CPLGetXMLValue(psSceneAttributes, "imageAttributes.incAngNearRng",
                             "UNK");
    poDS->SetMetadataItem("NEAR_RANGE_INCIDENCE_ANGLE", pszItem);

    pszItem = CPLGetXMLValue(psSceneAttributes, "imageAttributes.incAngFarRng",
                             "UNK");
    poDS->SetMetadataItem("FAR_RANGE_INCIDENCE_ANGLE", pszItem);

    pszItem = CPLGetXMLValue(psSceneAttributes,
                             "imageAttributes.slantRangeNearEdge", "UNK");
    poDS->SetMetadataItem("SLANT_RANGE_NEAR_EDGE", pszItem);

    pszItem = CPLGetXMLValue(psSceneAttributes,
                             "imageAttributes.slantRangeFarEdge", "UNK");
    poDS->SetMetadataItem("SLANT_RANGE_FAR_EDGE", pszItem);

    /*--------------------------------------------------------------------- */
    /*      Collect Map projection/Geotransform information, if present.DONE */
    /*      In RCM, there is no file that indicates                         */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psMapProjection = CPLGetXMLNode(
        psImageReferenceAttributes, "geographicInformation.mapProjection");

    if (psMapProjection != nullptr)
    {
        CPLXMLNode *psPos =
            CPLGetXMLNode(psMapProjection, "positioningInformation");

        pszItem =
            CPLGetXMLValue(psMapProjection, "mapProjectionDescriptor", "UNK");
        poDS->SetMetadataItem("MAP_PROJECTION_DESCRIPTOR", pszItem);

        pszItem =
            CPLGetXMLValue(psMapProjection, "mapProjectionOrientation", "UNK");
        poDS->SetMetadataItem("MAP_PROJECTION_ORIENTATION", pszItem);

        pszItem = CPLGetXMLValue(psMapProjection, "resamplingKernel", "UNK");
        poDS->SetMetadataItem("RESAMPLING_KERNEL", pszItem);

        pszItem = CPLGetXMLValue(psMapProjection, "satelliteHeading", "UNK");
        poDS->SetMetadataItem("SATELLITE_HEADING", pszItem);

        if (psPos != nullptr)
        {
            const double tl_x = CPLStrtod(
                CPLGetXMLValue(psPos, "upperLeftCorner.mapCoordinate.easting",
                               "0.0"),
                nullptr);
            const double tl_y = CPLStrtod(
                CPLGetXMLValue(psPos, "upperLeftCorner.mapCoordinate.northing",
                               "0.0"),
                nullptr);
            const double bl_x = CPLStrtod(
                CPLGetXMLValue(psPos, "lowerLeftCorner.mapCoordinate.easting",
                               "0.0"),
                nullptr);
            const double bl_y = CPLStrtod(
                CPLGetXMLValue(psPos, "lowerLeftCorner.mapCoordinate.northing",
                               "0.0"),
                nullptr);
            const double tr_x = CPLStrtod(
                CPLGetXMLValue(psPos, "upperRightCorner.mapCoordinate.easting",
                               "0.0"),
                nullptr);
            const double tr_y = CPLStrtod(
                CPLGetXMLValue(psPos, "upperRightCorner.mapCoordinate.northing",
                               "0.0"),
                nullptr);
            poDS->adfGeoTransform[1] = (tr_x - tl_x) / (poDS->nRasterXSize - 1);
            poDS->adfGeoTransform[4] = (tr_y - tl_y) / (poDS->nRasterXSize - 1);
            poDS->adfGeoTransform[2] = (bl_x - tl_x) / (poDS->nRasterYSize - 1);
            poDS->adfGeoTransform[5] = (bl_y - tl_y) / (poDS->nRasterYSize - 1);
            poDS->adfGeoTransform[0] = (tl_x - 0.5 * poDS->adfGeoTransform[1] -
                                        0.5 * poDS->adfGeoTransform[2]);
            poDS->adfGeoTransform[3] = (tl_y - 0.5 * poDS->adfGeoTransform[4] -
                                        0.5 * poDS->adfGeoTransform[5]);

            /* Use bottom right pixel to test geotransform */
            const double br_x = CPLStrtod(
                CPLGetXMLValue(psPos, "lowerRightCorner.mapCoordinate.easting",
                               "0.0"),
                nullptr);
            const double br_y = CPLStrtod(
                CPLGetXMLValue(psPos, "lowerRightCorner.mapCoordinate.northing",
                               "0.0"),
                nullptr);
            const double testx =
                poDS->adfGeoTransform[0] +
                poDS->adfGeoTransform[1] * (poDS->nRasterXSize - 0.5) +
                poDS->adfGeoTransform[2] * (poDS->nRasterYSize - 0.5);
            const double testy =
                poDS->adfGeoTransform[3] +
                poDS->adfGeoTransform[4] * (poDS->nRasterXSize - 0.5) +
                poDS->adfGeoTransform[5] * (poDS->nRasterYSize - 0.5);

            /* Give 1/4 pixel numerical error leeway in calculating location
            based on affine transform */
            if ((fabs(testx - br_x) >
                 fabs(0.25 *
                      (poDS->adfGeoTransform[1] + poDS->adfGeoTransform[2]))) ||
                (fabs(testy - br_y) > fabs(0.25 * (poDS->adfGeoTransform[4] +
                                                   poDS->adfGeoTransform[5]))))
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "WARNING: Unexpected error in calculating affine "
                         "transform: corner coordinates inconsistent.");
            }
            else
            {
                poDS->bHaveGeoTransform = TRUE;
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Collect Projection String Information.DONE                      */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psEllipsoid =
        CPLGetXMLNode(psImageReferenceAttributes,
                      "geographicInformation.ellipsoidParameters");

    if (psEllipsoid != nullptr)
    {
        OGRSpatialReference oLL, oPrj;

        const char *pszGeodeticTerrainHeight =
            CPLGetXMLValue(psEllipsoid, "geodeticTerrainHeight", "UNK");
        poDS->SetMetadataItem("GEODETIC_TERRAIN_HEIGHT",
                              pszGeodeticTerrainHeight);

        const char *pszEllipsoidName =
            CPLGetXMLValue(psEllipsoid, "ellipsoidName", "");
        double minor_axis =
            CPLAtof(CPLGetXMLValue(psEllipsoid, "semiMinorAxis", "0.0"));
        double major_axis =
            CPLAtof(CPLGetXMLValue(psEllipsoid, "semiMajorAxis", "0.0"));

        if (EQUAL(pszEllipsoidName, "") || (minor_axis == 0.0) ||
            (major_axis == 0.0))
        {
            oLL.SetWellKnownGeogCS("WGS84");
            oPrj.SetWellKnownGeogCS("WGS84");

            CPLError(CE_Warning, CPLE_AppDefined,
                     "WARNING: Incomplete ellipsoid "
                     "information.  Using wgs-84 parameters.");
        }
        else if (EQUAL(pszEllipsoidName, "WGS84") ||
                 EQUAL(pszEllipsoidName, "WGS 1984"))
        {
            oLL.SetWellKnownGeogCS("WGS84");
            oPrj.SetWellKnownGeogCS("WGS84");
        }
        else
        {
            const double inv_flattening =
                major_axis / (major_axis - minor_axis);
            oLL.SetGeogCS("", "", pszEllipsoidName, major_axis, inv_flattening);
            oPrj.SetGeogCS("", "", pszEllipsoidName, major_axis,
                           inv_flattening);
        }

        if (psMapProjection != nullptr)
        {
            const char *pszProj =
                CPLGetXMLValue(psMapProjection, "mapProjectionDescriptor", "");
            bool bUseProjInfo = false;

            CPLXMLNode *psUtmParams =
                CPLGetXMLNode(psMapProjection, "utmProjectionParameters");

            CPLXMLNode *psNspParams =
                CPLGetXMLNode(psMapProjection, "nspProjectionParameters");

            if ((psUtmParams != nullptr) && poDS->bHaveGeoTransform)
            {
                /* double origEasting, origNorthing; */
                bool bNorth = true;

                const int utmZone =
                    atoi(CPLGetXMLValue(psUtmParams, "utmZone", ""));
                const char *pszHemisphere =
                    CPLGetXMLValue(psUtmParams, "hemisphere", "");
#if 0
                origEasting = CPLStrtod(CPLGetXMLValue(
                    psUtmParams, "mapOriginFalseEasting", "0.0"), nullptr);
                origNorthing = CPLStrtod(CPLGetXMLValue(
                    psUtmParams, "mapOriginFalseNorthing", "0.0"), nullptr);
#endif
                if (STARTS_WITH_CI(pszHemisphere, "southern"))
                    bNorth = FALSE;

                if (STARTS_WITH_CI(pszProj, "UTM"))
                {
                    oPrj.SetUTM(utmZone, bNorth);
                    bUseProjInfo = true;
                }
            }
            else if ((psNspParams != nullptr) && poDS->bHaveGeoTransform)
            {
                const double origEasting = CPLStrtod(
                    CPLGetXMLValue(psNspParams, "mapOriginFalseEasting", "0.0"),
                    nullptr);
                const double origNorthing =
                    CPLStrtod(CPLGetXMLValue(psNspParams,
                                             "mapOriginFalseNorthing", "0.0"),
                              nullptr);
                const double copLong = CPLStrtod(
                    CPLGetXMLValue(psNspParams, "centerOfProjectionLongitude",
                                   "0.0"),
                    nullptr);
                const double copLat = CPLStrtod(
                    CPLGetXMLValue(psNspParams, "centerOfProjectionLatitude",
                                   "0.0"),
                    nullptr);
                const double sP1 = CPLStrtod(
                    CPLGetXMLValue(psNspParams, "standardParallels1", "0.0"),
                    nullptr);
                const double sP2 = CPLStrtod(
                    CPLGetXMLValue(psNspParams, "standardParallels2", "0.0"),
                    nullptr);

                if (STARTS_WITH_CI(pszProj, "ARC"))
                {
                    /* Albers Conical Equal Area */
                    oPrj.SetACEA(sP1, sP2, copLat, copLong, origEasting,
                                 origNorthing);
                    bUseProjInfo = true;
                }
                else if (STARTS_WITH_CI(pszProj, "LCC"))
                {
                    /* Lambert Conformal Conic */
                    oPrj.SetLCC(sP1, sP2, copLat, copLong, origEasting,
                                origNorthing);
                    bUseProjInfo = true;
                }
                else if (STARTS_WITH_CI(pszProj, "STPL"))
                {
                    /* State Plate
                    ASSUMPTIONS: "zone" in product.xml matches USGS
                    definition as expected by ogr spatial reference; NAD83
                    zones (versus NAD27) are assumed. */

                    const int nSPZone =
                        atoi(CPLGetXMLValue(psNspParams, "zone", "1"));

                    oPrj.SetStatePlane(nSPZone, TRUE, nullptr, 0.0);
                    bUseProjInfo = true;
                }
            }

            if (bUseProjInfo)
            {
                poDS->m_oSRS = std::move(oPrj);
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "WARNING: Unable to interpret projection information; "
                         "check mapProjection info in product.xml!");
            }
        }

        poDS->m_oGCPSRS = std::move(oLL);
    }

    /* -------------------------------------------------------------------- */
    /*      Collect GCPs.DONE */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psGeoGrid = CPLGetXMLNode(
        psImageReferenceAttributes, "geographicInformation.geolocationGrid");

    if (psGeoGrid != nullptr)
    {
        /* count GCPs */
        poDS->nGCPCount = 0;

        for (psNode = psGeoGrid->psChild; psNode != nullptr;
             psNode = psNode->psNext)
        {
            if (EQUAL(psNode->pszValue, "imageTiePoint"))
                poDS->nGCPCount++;
        }

        poDS->pasGCPList = reinterpret_cast<GDAL_GCP *>(
            CPLCalloc(sizeof(GDAL_GCP), poDS->nGCPCount));

        poDS->nGCPCount = 0;

        for (psNode = psGeoGrid->psChild; psNode != nullptr;
             psNode = psNode->psNext)
        {
            GDAL_GCP *psGCP = poDS->pasGCPList + poDS->nGCPCount;

            if (!EQUAL(psNode->pszValue, "imageTiePoint"))
                continue;

            poDS->nGCPCount++;

            char szID[32];
            snprintf(szID, sizeof(szID), "%d", poDS->nGCPCount);
            psGCP->pszId = CPLStrdup(szID);
            psGCP->pszInfo = CPLStrdup("");
            psGCP->dfGCPPixel =
                CPLAtof(CPLGetXMLValue(psNode, "imageCoordinate.pixel", "0"));
            psGCP->dfGCPLine =
                CPLAtof(CPLGetXMLValue(psNode, "imageCoordinate.line", "0"));
            psGCP->dfGCPX = CPLAtof(
                CPLGetXMLValue(psNode, "geodeticCoordinate.longitude", ""));
            psGCP->dfGCPY = CPLAtof(
                CPLGetXMLValue(psNode, "geodeticCoordinate.latitude", ""));
            psGCP->dfGCPZ = CPLAtof(
                CPLGetXMLValue(psNode, "geodeticCoordinate.height", ""));
        }
    }

    if (pszBeta0LUT)
        CPLFree(pszBeta0LUT);
    if (pszSigma0LUT)
        CPLFree(pszSigma0LUT);
    if (pszGammaLUT)
        CPLFree(pszGammaLUT);

    /* -------------------------------------------------------------------- */
    /*      Collect RPC.DONE */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psRationalFunctions = CPLGetXMLNode(
        psImageReferenceAttributes, "geographicInformation.rationalFunctions");
    if (psRationalFunctions != nullptr)
    {
        char **papszRPC = nullptr;
        static const char *const apszXMLToGDALMapping[] = {
            "biasError",
            "ERR_BIAS",
            "randomError",
            "ERR_RAND",
            //"lineFitQuality", "????",
            //"pixelFitQuality", "????",
            "lineOffset",
            "LINE_OFF",
            "pixelOffset",
            "SAMP_OFF",
            "latitudeOffset",
            "LAT_OFF",
            "longitudeOffset",
            "LONG_OFF",
            "heightOffset",
            "HEIGHT_OFF",
            "lineScale",
            "LINE_SCALE",
            "pixelScale",
            "SAMP_SCALE",
            "latitudeScale",
            "LAT_SCALE",
            "longitudeScale",
            "LONG_SCALE",
            "heightScale",
            "HEIGHT_SCALE",
            "lineNumeratorCoefficients",
            "LINE_NUM_COEFF",
            "lineDenominatorCoefficients",
            "LINE_DEN_COEFF",
            "pixelNumeratorCoefficients",
            "SAMP_NUM_COEFF",
            "pixelDenominatorCoefficients",
            "SAMP_DEN_COEFF",
        };
        for (size_t i = 0; i < CPL_ARRAYSIZE(apszXMLToGDALMapping); i += 2)
        {
            const char *pszValue = CPLGetXMLValue(
                psRationalFunctions, apszXMLToGDALMapping[i], nullptr);
            if (pszValue)
                papszRPC = CSLSetNameValue(
                    papszRPC, apszXMLToGDALMapping[i + 1], pszValue);
        }
        poDS->GDALDataset::SetMetadata(papszRPC, "RPC");
        CSLDestroy(papszRPC);
    }

    /* -------------------------------------------------------------------- */
    /*      Initialize any PAM information.                                 */
    /* -------------------------------------------------------------------- */
    CPLString osDescription;
    CPLString osSubdatasetName;
    bool useSubdatasets = true;

    switch (eCalib)
    {
        case Sigma0:
        {
            osSubdatasetName = szSIGMA0;
            osDescription = FormatCalibration(szSIGMA0, osMDFilename.c_str());
        }
        break;
        case Beta0:
        {
            osSubdatasetName = szBETA0;
            osDescription = FormatCalibration(szBETA0, osMDFilename.c_str());
        }
        break;
        case Gamma:
        {
            osSubdatasetName = szGAMMA;
            osDescription = FormatCalibration(szGAMMA, osMDFilename.c_str());
        }
        break;
        case Uncalib:
        {
            osSubdatasetName = szUNCALIB;
            osDescription = FormatCalibration(szUNCALIB, osMDFilename.c_str());
        }
        break;
        default:
            osSubdatasetName = szUNCALIB;
            osDescription = osMDFilename;
            useSubdatasets = false;
    }

    CPL_IGNORE_RET_VAL(osSubdatasetName);

    if (eCalib != None)
        poDS->papszExtraFiles =
            CSLAddString(poDS->papszExtraFiles, osMDFilename);

    /* -------------------------------------------------------------------- */
    /*      Initialize any PAM information.                                 */
    /* -------------------------------------------------------------------- */
    poDS->SetDescription(osDescription);

    poDS->SetPhysicalFilename(osMDFilename);
    poDS->SetSubdatasetName(osDescription);

    poDS->TryLoadXML();

    /* -------------------------------------------------------------------- */
    /*      Check for overviews.                                            */
    /* -------------------------------------------------------------------- */
    if (useSubdatasets)
        poDS->oOvManager.Initialize(poDS.get(), ":::VIRTUAL:::");
    else
        poDS->oOvManager.Initialize(poDS.get(), osMDFilename);

    return poDS.release();
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int RCMDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPSpatialRef()                          */
/************************************************************************/

const OGRSpatialReference *RCMDataset::GetGCPSpatialRef() const

{
    return m_oGCPSRS.IsEmpty() || nGCPCount == 0 ? nullptr : &m_oGCPSRS;
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *RCMDataset::GetGCPs()

{
    return pasGCPList;
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

const OGRSpatialReference *RCMDataset::GetSpatialRef() const

{
    return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr RCMDataset::GetGeoTransform(double *padfTransform)

{
    memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6);

    if (bHaveGeoTransform)
    {
        return CE_None;
    }

    return CE_Failure;
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **RCMDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALDataset::GetMetadataDomainList(), TRUE,
                                   "SUBDATASETS", nullptr);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **RCMDataset::GetMetadata(const char *pszDomain)

{
    if (pszDomain != nullptr && STARTS_WITH_CI(pszDomain, "SUBDATASETS") &&
        papszSubDatasets != nullptr)
        return papszSubDatasets;

    return GDALDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                         GDALRegister_RCM()                           */
/************************************************************************/

void GDALRegister_RCM()

{
    if (GDALGetDriverByName("RCM") != nullptr)
    {
        return;
    }

    GDALDriver *poDriver = new GDALDriver();
    RCMDriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = RCMDataset::Open;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
