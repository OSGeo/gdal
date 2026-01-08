/******************************************************************************
 *
 * Project:  PDF driver
 * Purpose:  GDALDataset driver for PDF dataset.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef PDFIO_H_INCLUDED
#define PDFIO_H_INCLUDED

#include "cpl_vsi_virtual.h"

/************************************************************************/
/*                         VSIPDFFileStream                             */
/************************************************************************/

#define BUFFER_SIZE 1024

class VSIPDFFileStream final : public BaseStream
{
  public:
    VSIPDFFileStream(VSILFILE *f, const char *pszFilename, Object &&dictA);
    VSIPDFFileStream(VSIPDFFileStream *poParent, vsi_l_offset startA,
                     bool limitedA, vsi_l_offset lengthA, Object &&dictA);
    ~VSIPDFFileStream() override;

    BaseStream *copy() override;

#if POPPLER_MAJOR_VERSION > 25 ||                                              \
    (POPPLER_MAJOR_VERSION == 25 && POPPLER_MINOR_VERSION >= 5)
    virtual std::unique_ptr<Stream> makeSubStream(Goffset startA, bool limitedA,
                                                  Goffset lengthA,
                                                  Object &&dictA) override;
#else
    virtual Stream *makeSubStream(Goffset startA, bool limitedA,
                                  Goffset lengthA, Object &&dictA) override;

#endif
    Goffset getPos() override;
    Goffset getStart() override;

    void setPos(Goffset pos, int dir = 0) override;
    void moveStart(Goffset delta) override;

    StreamKind getKind() const override;

    GooString *getFileName() override;

    int getChar() override;
    int getUnfilteredChar() override;
    int lookChar() override;

#if POPPLER_MAJOR_VERSION > 25
    bool rewind() override;
#elif POPPLER_MAJOR_VERSION == 25 && POPPLER_MINOR_VERSION >= 2
    bool reset() override;
#else
    void reset() override;
#endif

    static void resetNoCheckReturnValue(Stream *str)
    {
#if POPPLER_MAJOR_VERSION > 25
        CPL_IGNORE_RET_VAL(str->rewind());
#elif POPPLER_MAJOR_VERSION == 25 && POPPLER_MINOR_VERSION >= 2
        CPL_IGNORE_RET_VAL(str->reset());
#else
        str->reset();
#endif
    }

#if POPPLER_MAJOR_VERSION > 25
    bool unfilteredRewind() override;
#elif POPPLER_MAJOR_VERSION == 25 && POPPLER_MINOR_VERSION >= 2
    bool unfilteredReset() override;
#else
    void unfilteredReset() override;
#endif

    void close() override;

    bool FoundLinearizedHint() const
    {
        return bFoundLinearizedHint;
    }

  private:
    bool hasGetChars() override;
    int getChars(int nChars, unsigned char *buffer) override;

    VSIPDFFileStream *poParent = nullptr;
    GooString *poFilename = nullptr;
    VSILFILE *f = nullptr;
    vsi_l_offset nStart = 0;
    bool bLimited = false;
    vsi_l_offset nLength = 0;

    vsi_l_offset nCurrentPos = VSI_L_OFFSET_MAX;
    int bHasSavedPos = FALSE;
    vsi_l_offset nSavedPos = 0;

    GByte abyBuffer[BUFFER_SIZE];
    int nPosInBuffer = -1;
    int nBufferLength = -1;

    bool bFoundLinearizedHint = false;

    int FillBuffer();

    CPL_DISALLOW_COPY_ASSIGN(VSIPDFFileStream)
};

#endif  // PDFIO_H_INCLUDED
