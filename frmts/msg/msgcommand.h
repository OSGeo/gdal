/******************************************************************************
 *
 * Purpose:  Interface of MSGCommand class. Parse the src_dataset string
 *           that is meant for the MSG driver.
 * Author:   Bas Retsios, retsios@itc.nl
 *
 ******************************************************************************
 * Copyright (c) 2004, ITC
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef GDAL_MSG_MSGCOMMAND_H_INCLUDED
#define GDAL_MSG_MSGCOMMAND_H_INCLUDED

#include <string>

class MSGCommand
{
  public:
    MSGCommand();
    virtual ~MSGCommand();

    std::string parse(std::string const &command_line);
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
    static std::string sTrimSpaces(std::string const &str);
    static std::string sNextTerm(std::string const &str, int &iPos);
    static int iDaysInMonth(int iMonth, int iYear);
    static std::string sChannel(int iChannel);
    static int iChannel(std::string const &sChannel);
    static std::string sTimeStampToFolder(const std::string &sTimeStamp);
    std::string sRootFolder;
    std::string sTimeStamp;
    int iStep;
    bool fUseTimestampFolder;
};

#endif  // GDAL_MSG_MSGCOMMAND_H_INCLUDED
