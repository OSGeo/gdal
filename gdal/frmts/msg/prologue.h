/******************************************************************************
 * $Id$
 *
 * Purpose:  Interface of Prologue class. Parse the prologue of one repeat
 *           cycle and keep the interesting info.
 * Author:   Bas Retsios, retsios@itc.nl
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
 ******************************************************************************/

#if !defined(AFX_PROLOGUE_H__777B5B86_04F4_4A01_86F6_24615DCD8446__INCLUDED_)
#define AFX_PROLOGUE_H__777B5B86_04F4_4A01_86F6_24615DCD8446__INCLUDED_

#include <fstream>

class ReferenceGridRecord
{
public:
  explicit ReferenceGridRecord(std::ifstream & ifile);

  int NumberOfLines;
  int NumberOfColumns;
  double LineDirGridStep;
  double ColumnDirGridStep;
  unsigned char GridOrigin; // 0 == north-west corner; 1 == sw; 2 == se; 3 == ne;
};

class PlannedCoverageVIS_IRRecord
{
public:
  explicit PlannedCoverageVIS_IRRecord(std::ifstream & ifile);

  int SouthernLinePlanned;
  int NorthernLinePlanned;
  int EasternColumnPlanned;
  int WesternColumnPlanned;
};

class PlannedCoverageHRVRecord
{
public:
  explicit PlannedCoverageHRVRecord(std::ifstream & ifile);
  int LowerSouthLinePlanned;
  int LowerNorthLinePlanned;
  int LowerEastColumnPlanned;
  int LowerWestColumnPlanned;
  int UpperSouthLinePlanned;
  int UpperNorthLinePlanned;
  int UpperEastColumnPlanned;
  int UpperWestColumnPlanned;
};

class ImageDescriptionRecord
{
  CPL_DISALLOW_COPY_ASSIGN( ImageDescriptionRecord )

public:
  explicit ImageDescriptionRecord(std::ifstream & ifile);
  virtual ~ImageDescriptionRecord();

  unsigned char TypeOfProjection; // 1 == Geostationary, Earth centered in grid
  double LongitudeOfSSP;
  ReferenceGridRecord * ReferenceGridVIS_IR;
  ReferenceGridRecord * ReferenceGridHRV;
  PlannedCoverageVIS_IRRecord * PlannedCoverageVIS_IR;
  PlannedCoverageHRVRecord * PlannedCoverageHRV;
  unsigned char ImageProcDirection; // 0 == north-south; 1 == south-north
  unsigned char PixelGenDirection; // 0 == east-west; 1 == west-east;
  unsigned char PlannedChannelProcessing [12];
};

class RadiometricProcessingRecord
{
public:
  explicit RadiometricProcessingRecord(std::ifstream & ifile);

  double Cal_Slope [12];
  double Cal_Offset [12];
};

class Prologue
{
public:
  Prologue();
  virtual ~Prologue();

  void read(std::ifstream & ifile);

  const ImageDescriptionRecord * idr() const
  {
    return m_idr;
  }

  const RadiometricProcessingRecord * rpr() const
  {
    return m_rpr;
  }

private:
  ImageDescriptionRecord * m_idr;
  RadiometricProcessingRecord * m_rpr;
};

#endif // !defined(AFX_PROLOGUE_H__777B5B86_04F4_4A01_86F6_24615DCD8446__INCLUDED_)
