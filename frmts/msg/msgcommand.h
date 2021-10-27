/******************************************************************************
 * $Id$
 *
 * Purpose:  Interface of MSGCommand class. Parse the src_dataset string
 *           that is meant for the MSG driver.
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

#ifndef GDAL_MSG_MSGCOMMAND_H_INCLUDED
#define GDAL_MSG_MSGCOMMAND_H_INCLUDED

#include <string>

class MSGCommand
{
public:
  MSGCommand();
  virtual ~MSGCommand();

  std::string parse(std::string const& command_line);
  std::string sFileName(int iSatellite, int iSequence, int iStrip);
  std::string sPrologueFileName(int iSatellite, int iSequence);
  std::string sCycle(int iCycle);
  int iNrChannels() const;
  int iChannel(int iNr) const;

  static int iNrStrips(int iChannel);

  char cDataConversion;
  int iNrCycles;
  int channel[12];

private:
  static std::string sTrimSpaces(std::string const& str);
  static std::string sNextTerm(std::string const& str, int & iPos);
  static int iDaysInMonth(int iMonth, int iYear);
  static std::string sChannel(int iChannel);
  static int iChannel(std::string const& sChannel);
  static std::string sTimeStampToFolder(std::string& sTimeStamp);
  std::string sRootFolder;
  std::string sTimeStamp;
  int iStep;
  bool fUseTimestampFolder;
};

#endif // GDAL_MSG_MSGCOMMAND_H_INCLUDED
