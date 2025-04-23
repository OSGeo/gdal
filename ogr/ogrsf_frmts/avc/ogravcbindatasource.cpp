/******************************************************************************
 *
 * Project:  OGR
 * Purpose:  Implements OGRAVCBinDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_avc.h"
#include "cpl_conv.h"
#include "cpl_string.h"

/************************************************************************/
/*                        OGRAVCBinDataSource()                         */
/************************************************************************/

OGRAVCBinDataSource::OGRAVCBinDataSource()
    : papoLayers(nullptr), nLayers(0), psAVC(nullptr)
{
    poSRS = nullptr;
}

/************************************************************************/
/*                        ~OGRAVCBinDataSource()                        */
/************************************************************************/

OGRAVCBinDataSource::~OGRAVCBinDataSource()

{
    if (psAVC)
    {
        AVCE00ReadClose(psAVC);
        psAVC = nullptr;
    }

    for (int i = 0; i < nLayers; i++)
        delete papoLayers[i];

    CPLFree(papoLayers);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRAVCBinDataSource::Open(const char *pszNewName, int bTestOpen)

{
    /* -------------------------------------------------------------------- */
    /*      Open the source file.  Suppress error reporting if we are in    */
    /*      TestOpen mode.                                                  */
    /* -------------------------------------------------------------------- */
    if (bTestOpen)
        CPLPushErrorHandler(CPLQuietErrorHandler);

    psAVC = AVCE00ReadOpen(pszNewName);

    if (bTestOpen)
    {
        CPLPopErrorHandler();
        CPLErrorReset();
    }

    if (psAVC == nullptr)
        return FALSE;

    pszCoverageName = CPLStrdup(psAVC->pszCoverName);

    // Read SRS first
    for (int iSection = 0; iSection < psAVC->numSections; iSection++)
    {
        AVCE00Section *psSec = psAVC->pasSections + iSection;

        switch (psSec->eType)
        {
            case AVCFilePRJ:
            {
                AVCBinFile *hFile = AVCBinReadOpen(
                    psAVC->pszCoverPath, psSec->pszFilename, psAVC->eCoverType,
                    psSec->eType, psAVC->psDBCSInfo);
                if (hFile && poSRS == nullptr)
                {
                    char **papszPRJ = AVCBinReadNextPrj(hFile);

                    poSRS = new OGRSpatialReference();
                    poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                    if (poSRS->importFromESRI(papszPRJ) != OGRERR_NONE)
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Failed to parse PRJ section, ignoring.");
                        delete poSRS;
                        poSRS = nullptr;
                    }
                }
                if (hFile)
                {
                    AVCBinReadClose(hFile);
                }
            }
            break;

            default:
                break;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Create layers for the "interesting" sections of the coverage.   */
    /* -------------------------------------------------------------------- */

    papoLayers = static_cast<OGRLayer **>(
        CPLCalloc(sizeof(OGRLayer *), psAVC->numSections));
    nLayers = 0;

    for (int iSection = 0; iSection < psAVC->numSections; iSection++)
    {
        AVCE00Section *psSec = psAVC->pasSections + iSection;

        switch (psSec->eType)
        {
            case AVCFileARC:
            case AVCFilePAL:
            case AVCFileCNT:
            case AVCFileLAB:
            case AVCFileRPL:
            case AVCFileTXT:
            case AVCFileTX6:
                papoLayers[nLayers++] = new OGRAVCBinLayer(this, psSec);
                break;

            default:
                break;
        }
    }

    return nLayers > 0;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRAVCBinDataSource::TestCapability(CPL_UNUSED const char *pszCap)
{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRAVCBinDataSource::GetLayer(int iLayer)

{
    if (iLayer < 0 || iLayer >= nLayers)
        return nullptr;

    return papoLayers[iLayer];
}
