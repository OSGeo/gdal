/******************************************************************************
 * $Id$
 *
 * Purpose:  Implementation of MSGCommand class. Parse the src_dataset
 *           string that is meant for the MSG driver.
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

#include "msgcommand.h"
#include <cstdlib>
#include <cstdio>
using namespace std;

#ifdef _WIN32
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

#define min(a,b) (((a)<(b))?(a):(b))

MSGCommand::MSGCommand() :
    cDataConversion('N'),
    iNrCycles(1),
    sRootFolder(""),
    sTimeStamp(""),
    iStep(1),
    fUseTimestampFolder(true)
{
  for (int i = 0; i < 12; ++i)
    channel[i] = 0;
}

MSGCommand::~MSGCommand()
{

}

std::string MSGCommand::sTrimSpaces(std::string const& str)
{
  std::string::size_type iStart = 0;

  while ((iStart < str.length()) && (str[iStart] == ' '))
    ++iStart;

  std::string::size_type iLength = str.length() - iStart;

  while ((iLength > 0) && (str[iStart + iLength - 1] == ' '))
    --iLength;

  return str.substr(iStart, iLength);
}

std::string MSGCommand::sNextTerm(std::string const& str, int & iPos)
{
  std::string::size_type iOldPos = iPos;
  iPos = str.find(',', iOldPos);
  iPos = min(iPos, str.find(')', iOldPos));
  if (iPos > iOldPos)
  {
    std::string sRet = str.substr(iOldPos, iPos - iOldPos);
    if (str[iPos] != ')')
      ++iPos;
    return sTrimSpaces(sRet);
  }
  else
    return "";
}

bool fTimeStampCorrect(std::string const& sTimeStamp)
{
  if (sTimeStamp.length() != 12)
    return false;

  for (int i = 0; i < 12; ++i)
  {
    if (sTimeStamp[i] < '0' || sTimeStamp[i] > '9')
      return false;
  }

  return true;
}

std::string MSGCommand::parse(std::string const& command_line)
{
  // expected:
  // MSG(folder,timestamp,channel,in_same_folder,data_conversion,nr_cycles,step)
  // or
  // MSG(folder,timestamp,(channel,channel,...,channel),in_same_folder,data_conversion,nr_cycles,step)
  // or
  // <path>\H-000-MSG1__-MSG1________.....

  std::string sErr ("");

  std::string sID = command_line.substr(0, 4);
  if (sID.compare("MSG(") == 0)
  {
    int iPos = 4; // after bracket open
    sRootFolder = sNextTerm(command_line, iPos);
    if (sRootFolder.length() > 0)
    {
      if (sRootFolder[sRootFolder.length() - 1] != PATH_SEP)
      sRootFolder += PATH_SEP;
      sTimeStamp = sNextTerm(command_line, iPos);
      if (fTimeStampCorrect(sTimeStamp))
      {
        try // for eventual exceptions
        {
          while ((iPos < command_line.length()) && (command_line[iPos] == ' '))
            ++iPos;
          if (command_line[iPos] == '(')
          {
            ++iPos; // skip the ( bracket
            int i = 1;
            std::string sChannel = sNextTerm(command_line, iPos);
            while (command_line[iPos] != ')')
            {
              int iChan = atoi(sChannel.c_str());
              if (iChan >= 1 && iChan <= 12)
                channel[iChan - 1] = i;
              else
                sErr = "Channel numbers must be between 1 and 12";
              sChannel = sNextTerm(command_line, iPos);
              ++i;
            }
            int iChan = atoi(sChannel.c_str());
            if (iChan >= 1 && iChan <= 12)
              channel[iChan - 1] = i;
            else
              sErr = "Channel numbers must be between 1 and 12";
            ++iPos; // skip the ) bracket
            while ((iPos < command_line.length()) && (command_line[iPos] == ' '))
              ++iPos;
            if (command_line[iPos] == ',')
              ++iPos;
          }
          else
          {
            std::string sChannel = sNextTerm(command_line, iPos);
            int iChan = atoi(sChannel.c_str());
            if (iChan >= 1 && iChan <= 12)
              channel[iChan - 1] = 1;
            else
              sErr = "Channel numbers must be between 1 and 12";
          }
          std::string sInRootFolder = sNextTerm(command_line, iPos);
          if ((sInRootFolder.compare("N") != 0) && (sInRootFolder.compare("Y") != 0))
            sErr = "Please specify N for data that is in a date dependent folder or Y for data that is in specified folder.";
          else
            fUseTimestampFolder = (sInRootFolder.compare("N") == 0);
          std::string sDataConversion = sNextTerm(command_line, iPos);
          cDataConversion = (sDataConversion.length()>0)?sDataConversion[0]:'N';
          std::string sNrCycles = sNextTerm(command_line, iPos);
          iNrCycles = atoi(sNrCycles.c_str());
          if (iNrCycles < 1)
            iNrCycles = 1;
          std::string sStep = sNextTerm(command_line, iPos);
          iStep = atoi(sStep.c_str());
          if (iStep < 1)
            iStep = 1;
          while ((iPos < command_line.length()) && (command_line[iPos] == ' '))
            ++iPos;
          // additional correctness checks
          if (command_line[iPos] != ')')
            sErr = "Invalid syntax. Please review the MSG(...) statement.";
          else if ((iNrChannels() > 1) && (channel[11] != 0))
            sErr = "It is not possible to combine channel 12 (HRV) with the other channels.";
          else if (iNrChannels() == 0 && sErr.length() == 0)
            sErr = "At least one channel should be specified.";
          else if ((cDataConversion != 'N') && (cDataConversion != 'B') && (cDataConversion != 'R') && (cDataConversion != 'L') && (cDataConversion != 'T'))
            sErr = "Please specify N(o change), B(yte conversion), R(adiometric calibration), L(radiometric using central wavelength) or T(reflectance or temperature) for data conversion.";
        }
        catch(...)
        {
          sErr = "Invalid syntax. Please review the MSG(...) statement.";
        }
      }
      else
        sErr = "Timestamp should be exactly 12 digits.";
    }
    else
      sErr = "A folder must be filled in indicating the root of the image data folders.";
  }
  else if (command_line.find("H-000-MSG") >= 0)
  {
    int iPos = command_line.find("H-000-MSG");
    if ((command_line.length() - iPos) == 61)
    {
      fUseTimestampFolder = false;
      sRootFolder = command_line.substr(0, iPos);
      sTimeStamp = command_line.substr(iPos + 46, 12);
      if (fTimeStampCorrect(sTimeStamp))
      {
        int iChan = iChannel(command_line.substr(iPos + 26, 9));
        if (iChan >= 1 && iChan <= 12)
        {
          channel[iChan - 1] = 1;
          cDataConversion = 'N';
          iNrCycles = 1;
          iStep = 1;
        }
        else
          sErr = "Channel numbers must be between 1 and 12";
      }
      else
        sErr = "Timestamp should be exactly 12 digits.";
    }
    else
      sErr = "-"; // the source data set it is not for the MSG driver
  }
  else
    sErr = "-"; // the source data set it is not for the MSG driver
  return sErr;
}

int MSGCommand::iNrChannels()
{
  int iRet = 0;
  for (int i=0; i<12; ++i)
    if (channel[i] != 0)
      ++iRet;

  return iRet;
}

int MSGCommand::iChannel(int iChannelNumber)
{
  // return the iChannelNumber-th channel
  // iChannelNumber is a value between 1 and 12
  // note that channels are ordered. their order number is the value in the array
  // As we can't combine channel 12 with channels 1 to 11, it does not make sense to inquire for iNr == 12
  int iRet = 0;
  if (iChannelNumber <= iNrChannels())
  {
    while ((iRet < 12) && (channel[iRet] != iChannelNumber))
      ++iRet;
  }

  // will return a number between 1 and 12
  return (iRet + 1);
}

int MSGCommand::iNrStrips(int iChannel)
{
  if (iChannel == 12)
    return 24;
  else if (iChannel >= 1 && iChannel <= 11)
    return 8;
  else
    return 0;
}

int MSGCommand::iChannel(std::string const& sChannel)
{
  if (sChannel.compare("VIS006___") == 0)
    return 1;
  else if (sChannel.compare("VIS008___") == 0)
    return 2;
  else if (sChannel.compare("IR_016___") == 0)
    return 3;
  else if (sChannel.compare("IR_039___") == 0)
    return 4;
  else if (sChannel.compare("WV_062___") == 0)
    return 5;
  else if (sChannel.compare("WV_073___") == 0)
    return 6;
  else if (sChannel.compare("IR_087___") == 0)
    return 7;
  else if (sChannel.compare("IR_097___") == 0)
    return 8;
  else if (sChannel.compare("IR_108___") == 0)
    return 9;
  else if (sChannel.compare("IR_120___") == 0)
    return 10;
  else if (sChannel.compare("IR_134___") == 0)
    return 11;
  else if (sChannel.compare("HRV______") == 0)
    return 12;
  else
    return 0;
}

std::string MSGCommand::sChannel(int iChannel)
{
  switch (iChannel)
  {
    case 1:
      return "VIS006___";
      break;
    case 2:
      return "VIS008___";
      break;
    case 3:
      return "IR_016___";
      break;
    case 4:
      return "IR_039___";
      break;
    case 5:
      return "WV_062___";
      break;
    case 6:
      return "WV_073___";
      break;
    case 7:
      return "IR_087___";
      break;
    case 8:
      return "IR_097___";
      break;
    case 9:
      return "IR_108___";
      break;
    case 10:
      return "IR_120___";
      break;
    case 11:
      return "IR_134___";
      break;
    case 12:
      return "HRV______";
      break;
    default:
      return "_________";
      break;
  }
}

std::string MSGCommand::sTimeStampToFolder(std::string & sTimeStamp)
{
  std::string sYear (sTimeStamp.substr(0,4));
  std::string sMonth (sTimeStamp.substr(4, 2));
  std::string sDay (sTimeStamp.substr(6, 2));
  return (sYear + PATH_SEP + sMonth + PATH_SEP + sDay + PATH_SEP);
}

int MSGCommand::iDaysInMonth(int iMonth, int iYear)
{
  int iDays;

  if ((iMonth == 4) || (iMonth == 6) || (iMonth == 9) || (iMonth == 11))
    iDays = 30;
  else if (iMonth == 2)
  {
    iDays = 28;
    if (iYear % 100 == 0) // century year
    {
      if (iYear % 400 == 0) // century leap year
        ++iDays;
    }
    else
    {
      if (iYear % 4 == 0) // normal leap year
        ++iDays;
    }
  }
  else
    iDays = 31;

  return iDays;
}

std::string MSGCommand::sCycle(int iCycle)
{
  // find nth full quarter
  // e.g. for n = 1, 200405311114 should result in 200405311115
  // 200405311115 should result in 200405311130
  // 200405311101 should result in 200405311115
  // 200412312345 should result in 200501010000
  
  std::string sYear (sTimeStamp.substr(0, 4));
  std::string sMonth (sTimeStamp.substr(4, 2));
  std::string sDay (sTimeStamp.substr(6, 2));
  std::string sHours (sTimeStamp.substr(8, 2));
  std::string sMins (sTimeStamp.substr(10, 2));

  int iYear = atoi(sYear.c_str());
  int iMonth = atoi(sMonth.c_str());
  int iDay = atoi(sDay.c_str());
  int iHours = atoi(sHours.c_str());
  int iMins = atoi(sMins.c_str());

  iMins += (iCycle - 1)*15*iStep;

  // round off the mins found down to a multiple of 15 mins
  iMins = ((int)(iMins / 15)) * 15;
  // now handle the whole chain back to the year ...
  while (iMins >= 60)
  {
    iMins -= 60;
    ++iHours;
  }
  while (iHours >= 24)
  {
    iHours -= 24;
    ++iDay;
  }
  while (iDay > iDaysInMonth(iMonth, iYear))
  {
    iDay -= iDaysInMonth(iMonth, iYear);
    ++iMonth;
  }
  while (iMonth > 12)
  {
    iMonth -= 12;
    ++iYear;
  }

  char sRet [100];
  sprintf(sRet, "%.4d%.2d%.2d%.2d%.2d", iYear, iMonth, iDay, iHours, iMins);

  return sRet;  
}

std::string MSGCommand::sFileName(int iSatellite, int iSequence, int iStrip)
{
  int iNr = iNrChannels();
  int iChannelNumber = 1 + (iSequence - 1) % iNr;;
  int iCycle = 1 + (iSequence - 1) / iNr;
  char sRet [4096];
  std::string siThCycle (sCycle(iCycle));
  if (fUseTimestampFolder)
    sprintf(sRet, "%s%sH-000-MSG%d__-MSG%d________-%s-%.6d___-%s-C_", sRootFolder.c_str(), sTimeStampToFolder(siThCycle).c_str(), iSatellite, iSatellite, sChannel(iChannel(iChannelNumber)).c_str(), iStrip, siThCycle.c_str());
  else
    sprintf(sRet, "%sH-000-MSG%d__-MSG%d________-%s-%.6d___-%s-C_", sRootFolder.c_str(), iSatellite, iSatellite, sChannel(iChannel(iChannelNumber)).c_str(), iStrip, siThCycle.c_str());
  return sRet;
}

std::string MSGCommand::sPrologueFileName(int iSatellite, int iSequence)
{
  int iCycle = 1 + (iSequence - 1) / iNrChannels();
  char sRet [4096];
  std::string siThCycle (sCycle(iCycle));
  if (fUseTimestampFolder)
    sprintf(sRet, "%s%sH-000-MSG%d__-MSG%d________-_________-PRO______-%s-__", sRootFolder.c_str(), sTimeStampToFolder(siThCycle).c_str(), iSatellite, iSatellite, siThCycle.c_str());
  else
    sprintf(sRet, "%sH-000-MSG%d__-MSG%d________-_________-PRO______-%s-__", sRootFolder.c_str(), iSatellite, iSatellite, siThCycle.c_str());
  return sRet;
}

