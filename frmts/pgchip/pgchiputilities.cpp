/******************************************************************************
 *
 * File :    pgchiputilities.cpp
 * Project:  PGCHIP Driver
 * Purpose:  Utility functions for POSTGIS CHIP/GDAL Driver 
 * Author:   Benjamin Simon, noumayoss@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Benjamin Simon, noumayoss@gmail.com
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
 * Revision 1.1  2005/08/29 bsimon
 * New
 *
 */
 
#include "pgchip.h"

/************************************************************************/
/* ==================================================================== */
/*				Utility Hex Functions                   */
/* ==================================================================== */
/************************************************************************/

void pgch_deparse_hex(unsigned char str, unsigned char *result){

	int	input_high;
	int  input_low;

	input_high = (str>>4);
	input_low = (str & 0x0F);

	switch (input_high)
	{
		case 0:
			result[0] = '0';
			break;
		case 1:
			result[0] = '1';
			break;
		case 2:
			result[0] = '2';
			break;
		case 3:
			result[0] = '3';
			break;
		case 4:
			result[0] = '4';
			break;
		case 5:
			result[0] = '5';
			break;
		case 6:
			result[0] = '6';
			break;
		case 7:
			result[0] = '7';
			break;
		case 8:
			result[0] = '8';
			break;
		case 9:
			result[0] = '9';
			break;
		case 10:
			result[0] = 'A';
			break;
		case 11:
			result[0] = 'B';
			break;
		case 12:
			result[0] = 'C';
			break;
		case 13:
			result[0] = 'D';
			break;
		case 14:
			result[0] = 'E';
			break;
		case 15:
			result[0] = 'F';
			break;
	}

	switch (input_low)
	{
		case 0:
			result[1] = '0';
			break;
		case 1:
			result[1] = '1';
			break;
		case 2:
			result[1] = '2';
			break;
		case 3:
			result[1] = '3';
			break;
		case 4:
			result[1] = '4';
			break;
		case 5:
			result[1] = '5';
			break;
		case 6:
			result[1] = '6';
			break;
		case 7:
			result[1] = '7';
			break;
		case 8:
			result[1] = '8';
			break;
		case 9:
			result[1] = '9';
			break;
		case 10:
			result[1] = 'A';
			break;
		case 11:
			result[1] = 'B';
			break;
		case 12:
			result[1] = 'C';
			break;
		case 13:
			result[1] = 'D';
			break;
		case 14:
			result[1] = 'E';
			break;
		case 15:
			result[1] = 'F';
			break;
	}
}

//given a string with at least 2 chars in it, convert them to
// a byte value.  No error checking done!
unsigned char parse_hex(char *str){

	//do this a little brute force to make it faster

	unsigned char		result_high = 0;
	unsigned char		result_low = 0;

	switch (str[0])
	{
		case '0' :
			result_high = 0;
			break;
		case '1' :
			result_high = 1;
			break;
		case '2' :
			result_high = 2;
			break;
		case '3' :
			result_high = 3;
			break;
		case '4' :
			result_high = 4;
			break;
		case '5' :
			result_high = 5;
			break;
		case '6' :
			result_high = 6;
			break;
		case '7' :
			result_high = 7;
			break;
		case '8' :
			result_high = 8;
			break;
		case '9' :
			result_high = 9;
			break;
		case 'A' :
			result_high = 10;
			break;
		case 'B' :
			result_high = 11;
			break;
		case 'C' :
			result_high = 12;
			break;
		case 'D' :
			result_high = 13;
			break;
		case 'E' :
			result_high = 14;
			break;
		case 'F' :
			result_high = 15;
			break;
	}
	switch (str[1])
	{
		case '0' :
			result_low = 0;
			break;
		case '1' :
			result_low = 1;
			break;
		case '2' :
			result_low = 2;
			break;
		case '3' :
			result_low = 3;
			break;
		case '4' :
			result_low = 4;
			break;
		case '5' :
			result_low = 5;
			break;
		case '6' :
			result_low = 6;
			break;
		case '7' :
			result_low = 7;
			break;
		case '8' :
			result_low = 8;
			break;
		case '9' :
			result_low = 9;
			break;
		case 'A' :
			result_low = 10;
			break;
		case 'B' :
			result_low = 11;
			break;
		case 'C' :
			result_low = 12;
			break;
		case 'D' :
			result_low = 13;
			break;
		case 'E' :
			result_low = 14;
			break;
		case 'F' :
			result_low = 15;
			break;
	}
	return (unsigned char) ((result_high<<4) + result_low);
}

/* Parse an hex string */
void parse_hex_string(unsigned char *strOut,char *strIn,int length){
    
    int i;
    for(i=0;i<length;i++){
        //printf("Before = %c\n",strIn[i]);
        strOut[i] = parse_hex(&strIn[i]);
        //printf("After = %c\n",strOut[i]);
        }

}

/* Deparse an hex string */
void deparse_hex_string(unsigned char *strOut,char *strIn,int length){
    
    int i;
    
    for(i=0;i<length;i++)
        pgch_deparse_hex(strIn[i],&strOut[i]);

}
