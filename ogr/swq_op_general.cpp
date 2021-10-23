/******************************************************************************
 *
 * Component: OGR SQL Engine
 * Purpose: Implementation of SWQGeneralEvaluator and SWQGeneralChecker
 *          functions used to represent functions during evaluation and
 *          parsing.
 * Author: Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (C) 2010 Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"
#include "ogr_swq.h"

#include <cctype>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_safemaths.hpp"
#include "cpl_string.h"
#include "ogr_geometry.h"
#include "ogr_p.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                           swq_test_like()                            */
/*                                                                      */
/*      Does input match pattern?                                       */
/************************************************************************/

static int swq_test_like( const char *input, const char *pattern,
                          char chEscape, bool insensitive )

{
    if( input == nullptr || pattern == nullptr )
        return 0;

    while( *input != '\0' )
    {
        if( *pattern == '\0' )
            return 0;

        else if( *pattern == chEscape )
        {
            pattern++;
            if( *pattern == '\0' )
                return 0;
            if( (!insensitive && *pattern != *input) ||
                (insensitive && tolower(*pattern) != tolower(*input)) )
            {
                return 0;
            }
            else
            {
                input++;
                pattern++;
            }
        }

        else if( *pattern == '_' )
        {
            input++;
            pattern++;
        }
        else if( *pattern == '%' )
        {
            if( pattern[1] == '\0' )
                return 1;

            // Try eating varying amounts of the input till we get a positive.
            for( int eat = 0; input[eat] != '\0'; eat++ )
            {
                if( swq_test_like(input + eat, pattern + 1, chEscape, insensitive) )
                    return 1;
            }

            return 0;
        }
        else
        {
            if( (!insensitive && *pattern != *input) ||
                (insensitive && tolower(*pattern) != tolower(*input)) )
            {
                return 0;
            }
            else
            {
                input++;
                pattern++;
            }
        }
    }

    if( *pattern != '\0' && strcmp(pattern, "%") != 0 )
        return 0;
    else
        return 1;
}

/************************************************************************/
/*                        OGRHStoreGetValue()                           */
/************************************************************************/

static char* OGRHStoreCheckEnd( char* pszIter, int bIsKey )
{
    pszIter++;
    for( ; *pszIter != '\0'; pszIter ++ )
    {
        if( bIsKey )
        {
            if( *pszIter == ' ' )
            {
                ;
            }
            else if( *pszIter == '=' && pszIter[1] == '>' )
            {
                return pszIter + 2;
            }
            else
            {
                return nullptr;
            }
        }
        else
        {
            if( *pszIter == ' ' )
            {
                ;
            }
            else if( *pszIter == ',' )
            {
                return pszIter + 1;
            }
            else
            {
                return nullptr;
            }
        }
    }
    return pszIter;
}

static char* OGRHStoreGetNextString( char* pszIter,
                                     char** ppszOut,
                                     int bIsKey )
{
    char ch;
    bool bInString = false;
    char* pszOut = nullptr;
    *ppszOut = nullptr;
    for( ; (ch = *pszIter) != '\0'; pszIter ++ )
    {
        if( bInString )
        {
            if( ch == '"' )
            {
                *pszOut = '\0';
                return OGRHStoreCheckEnd(pszIter, bIsKey);
            }
            else if( ch == '\\')
            {
                pszIter++;
                if( (ch = *pszIter) == '\0' )
                    return nullptr;
            }
            *pszOut = ch;
            pszOut++;
        }
        else
        {
            if( ch == ' ' )
            {
                if( pszOut != nullptr )
                {
                    *pszIter = '\0';
                    return OGRHStoreCheckEnd(pszIter, bIsKey);
                }
            }
            else if( bIsKey && ch == '=' && pszIter[1] == '>' )
            {
                if( pszOut != nullptr )
                {
                    *pszIter = '\0';
                    return pszIter + 2;
                }
            }
            else if( !bIsKey && ch == ',' )
            {
                if( pszOut != nullptr )
                {
                    *pszIter = '\0';
                    return pszIter + 1;
                }
            }
            else if( ch == '"' )
            {
                pszOut = pszIter + 1;
                *ppszOut = pszOut;
                bInString = true;
            }
            else if( pszOut == nullptr )
            {
                pszOut = pszIter;
                *ppszOut = pszIter;
            }
        }
    }

    if( !bInString && pszOut != nullptr )
    {
        return pszIter;
    }
    return nullptr;
}

static char* OGRHStoreGetNextKeyValue(char* pszHStore,
                                      char** ppszKey,
                                       char** ppszValue)
{
    char* pszNext = OGRHStoreGetNextString(pszHStore, ppszKey, TRUE);
    if( pszNext == nullptr || *pszNext == '\0' )
        return nullptr;
    return OGRHStoreGetNextString(pszNext, ppszValue, FALSE);
}

char* OGRHStoreGetValue(const char* pszHStore, const char* pszSearchedKey)
{
    char* pszHStoreDup = CPLStrdup(pszHStore);
    char* pszHStoreIter = pszHStoreDup;
    char* pszRet = nullptr;

    while( true )
    {
        char* pszKey, *pszValue;
        pszHStoreIter = OGRHStoreGetNextKeyValue(pszHStoreIter, &pszKey, &pszValue);
        if( pszHStoreIter == nullptr )
        {
            break;
        }
        if( strcmp(pszKey, pszSearchedKey) == 0 )
        {
            pszRet = CPLStrdup(pszValue);
            break;
        }
        if( *pszHStoreIter == '\0' )
        {
            break;
        }
    }
    CPLFree(pszHStoreDup);
    return pszRet;
}

#ifdef DEBUG_VERBOSE
/************************************************************************/
/*                         OGRFormatDate()                              */
/************************************************************************/

static const char * OGRFormatDate(const OGRField *psField)
{
    return CPLSPrintf("%04d/%02d/%02d %02d:%02d:%06.3f",
                    psField->Date.Year,
                    psField->Date.Month,
                    psField->Date.Day,
                    psField->Date.Hour,
                    psField->Date.Minute,
                    psField->Date.Second );
}
#endif

/************************************************************************/
/*                        SWQGeneralEvaluator()                         */
/************************************************************************/

swq_expr_node *SWQGeneralEvaluator( swq_expr_node *node,
                                    swq_expr_node **sub_node_values )

{
    swq_expr_node *poRet = nullptr;

/* -------------------------------------------------------------------- */
/*      Floating point operations.                                      */
/* -------------------------------------------------------------------- */
    if( sub_node_values[0]->field_type == SWQ_FLOAT
        || (node->nSubExprCount > 1
            && sub_node_values[1]->field_type == SWQ_FLOAT) )
    {
        poRet = new swq_expr_node(0);
        poRet->field_type = node->field_type;

        if( SWQ_IS_INTEGER(sub_node_values[0]->field_type) )
            sub_node_values[0]->float_value =
                static_cast<double>(sub_node_values[0]->int_value);
        if( node->nSubExprCount > 1 &&
            SWQ_IS_INTEGER(sub_node_values[1]->field_type) )
            sub_node_values[1]->float_value =
                static_cast<double>(sub_node_values[1]->int_value);

        if( node->nOperation != SWQ_ISNULL )
        {
            for( int i = 0; i < node->nSubExprCount; i++ )
            {
                if( sub_node_values[i]->is_null )
                {
                    if( poRet->field_type == SWQ_BOOLEAN )
                    {
                        poRet->int_value = FALSE;
                        return poRet;
                    }
                    else if( poRet->field_type == SWQ_FLOAT )
                    {
                        poRet->float_value = 0;
                        poRet->is_null = 1;
                        return poRet;
                    }
                    else if( SWQ_IS_INTEGER(poRet->field_type) )
                    {
                        poRet->field_type = SWQ_INTEGER;
                        poRet->int_value = 0;
                        poRet->is_null = 1;
                        return poRet;
                    }
                }
            }
        }

        switch( node->nOperation )
        {
          case SWQ_EQ:
            poRet->int_value = sub_node_values[0]->float_value
                == sub_node_values[1]->float_value;
            break;

          case SWQ_NE:
            poRet->int_value = sub_node_values[0]->float_value
                != sub_node_values[1]->float_value;
            break;

          case SWQ_GT:
            poRet->int_value = sub_node_values[0]->float_value
                > sub_node_values[1]->float_value;
            break;

          case SWQ_LT:
            poRet->int_value = sub_node_values[0]->float_value
                < sub_node_values[1]->float_value;
            break;

          case SWQ_GE:
            poRet->int_value = sub_node_values[0]->float_value
                >= sub_node_values[1]->float_value;
            break;

          case SWQ_LE:
            poRet->int_value = sub_node_values[0]->float_value
                <= sub_node_values[1]->float_value;
            break;

          case SWQ_IN:
          {
              poRet->int_value = 0;
              for( int i = 1; i < node->nSubExprCount; i++ )
              {
                  if( sub_node_values[0]->float_value
                      == sub_node_values[i]->float_value )
                  {
                      poRet->int_value = 1;
                      break;
                  }
              }
          }
          break;

          case SWQ_BETWEEN:
            poRet->int_value = sub_node_values[0]->float_value
                                >= sub_node_values[1]->float_value &&
                               sub_node_values[0]->float_value
                                <= sub_node_values[2]->float_value;
            break;

          case SWQ_ISNULL:
            poRet->int_value = sub_node_values[0]->is_null;
            break;

          case SWQ_ADD:
            poRet->float_value = sub_node_values[0]->float_value
                + sub_node_values[1]->float_value;
            break;

          case SWQ_SUBTRACT:
            poRet->float_value = sub_node_values[0]->float_value
                - sub_node_values[1]->float_value;
            break;

          case SWQ_MULTIPLY:
            poRet->float_value = sub_node_values[0]->float_value
                * sub_node_values[1]->float_value;
            break;

          case SWQ_DIVIDE:
            if( sub_node_values[1]->float_value == 0 )
                poRet->float_value = INT_MAX;
            else
                poRet->float_value = sub_node_values[0]->float_value
                    / sub_node_values[1]->float_value;
            break;

          case SWQ_MODULUS:
          {
            if( sub_node_values[1]->float_value == 0 )
                poRet->float_value = INT_MAX;
            else
                poRet->float_value = fmod(sub_node_values[0]->float_value,
                                        sub_node_values[1]->float_value);
            break;
          }

          default:
            CPLAssert( false );
            delete poRet;
            poRet = nullptr;
            break;
        }
    }
/* -------------------------------------------------------------------- */
/*      integer/boolean operations.                                     */
/* -------------------------------------------------------------------- */
    else if( SWQ_IS_INTEGER(sub_node_values[0]->field_type)
        || sub_node_values[0]->field_type == SWQ_BOOLEAN )
    {
        poRet = new swq_expr_node(0);
        poRet->field_type = node->field_type;

        if( node->nOperation != SWQ_ISNULL )
        {
            for( int i = 0; i < node->nSubExprCount; i++ )
            {
                if( sub_node_values[i]->is_null )
                {
                    if( poRet->field_type == SWQ_BOOLEAN )
                    {
                        poRet->int_value = FALSE;
                        return poRet;
                    }
                    else if( SWQ_IS_INTEGER(poRet->field_type) )
                    {
                        poRet->int_value = 0;
                        poRet->is_null = 1;
                        return poRet;
                    }
                }
            }
        }

        switch( node->nOperation )
        {
          case SWQ_AND:
            poRet->int_value = sub_node_values[0]->int_value
                && sub_node_values[1]->int_value;
            break;

          case SWQ_OR:
            poRet->int_value = sub_node_values[0]->int_value
                || sub_node_values[1]->int_value;
            break;

          case SWQ_NOT:
            poRet->int_value = !sub_node_values[0]->int_value;
            break;

          case SWQ_EQ:
            poRet->int_value = sub_node_values[0]->int_value
                == sub_node_values[1]->int_value;
            break;

          case SWQ_NE:
            poRet->int_value = sub_node_values[0]->int_value
                != sub_node_values[1]->int_value;
            break;

          case SWQ_GT:
            poRet->int_value = sub_node_values[0]->int_value
                > sub_node_values[1]->int_value;
            break;

          case SWQ_LT:
            poRet->int_value = sub_node_values[0]->int_value
                < sub_node_values[1]->int_value;
            break;

          case SWQ_GE:
            poRet->int_value = sub_node_values[0]->int_value
                >= sub_node_values[1]->int_value;
            break;

          case SWQ_LE:
            poRet->int_value = sub_node_values[0]->int_value
                <= sub_node_values[1]->int_value;
            break;

          case SWQ_IN:
          {
              poRet->int_value = 0;
              for( int i = 1; i < node->nSubExprCount; i++ )
              {
                  if( sub_node_values[0]->int_value
                      == sub_node_values[i]->int_value )
                  {
                      poRet->int_value = 1;
                      break;
                  }
              }
          }
          break;

          case SWQ_BETWEEN:
            poRet->int_value = sub_node_values[0]->int_value
                                >= sub_node_values[1]->int_value &&
                               sub_node_values[0]->int_value
                                <= sub_node_values[2]->int_value;
            break;

          case SWQ_ISNULL:
            poRet->int_value = sub_node_values[0]->is_null;
            break;

          case SWQ_ADD:
            try
            {
                poRet->int_value = (CPLSM(sub_node_values[0]->int_value)
                                  + CPLSM(sub_node_values[1]->int_value)).v();
            }
            catch( const std::exception& )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Int overflow");
                poRet->is_null = true;
            }
            break;

          case SWQ_SUBTRACT:
            try
            {
                poRet->int_value = (CPLSM(sub_node_values[0]->int_value)
                                  - CPLSM(sub_node_values[1]->int_value)).v();
            }
            catch( const std::exception& )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Int overflow");
                poRet->is_null = true;
            }
            break;

          case SWQ_MULTIPLY:
            try
            {
                poRet->int_value = (CPLSM(sub_node_values[0]->int_value)
                                  * CPLSM(sub_node_values[1]->int_value)).v();
            }
            catch( const std::exception& )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Int overflow");
                poRet->is_null = true;
            }
            break;

          case SWQ_DIVIDE:
            if( sub_node_values[1]->int_value == 0 )
                poRet->int_value = INT_MAX;
            else
            {
                try
                {
                    poRet->int_value = (CPLSM(sub_node_values[0]->int_value)
                                    / CPLSM(sub_node_values[1]->int_value)).v();
                }
                catch( const std::exception& )
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Int overflow");
                    poRet->is_null = true;
                }
            }
            break;

          case SWQ_MODULUS:
            if( sub_node_values[1]->int_value == 0 )
                poRet->int_value = INT_MAX;
            else
                poRet->int_value = sub_node_values[0]->int_value
                    % sub_node_values[1]->int_value;
            break;

          default:
            CPLAssert( false );
            delete poRet;
            poRet = nullptr;
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      datetime                                                        */
/* -------------------------------------------------------------------- */
    else if( sub_node_values[0]->field_type == SWQ_TIMESTAMP
             && (node->nOperation == SWQ_EQ
                 || node->nOperation == SWQ_GT
                 || node->nOperation == SWQ_GE
                 || node->nOperation == SWQ_LT
                 || node->nOperation == SWQ_LE
                 || node->nOperation == SWQ_IN
                 || node->nOperation == SWQ_BETWEEN) )
    {
        OGRField sField0, sField1;
        poRet = new swq_expr_node(0);
        poRet->field_type = node->field_type;

        if( !OGRParseDate(sub_node_values[0]->string_value, &sField0, 0))
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Failed to parse date '%s' evaluating OGR WHERE expression",
                sub_node_values[0]->string_value);
            delete poRet;
            return nullptr;
        }
        if( !OGRParseDate(sub_node_values[1]->string_value, &sField1, 0))
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Failed to parse date '%s' evaluating OGR WHERE expression",
                sub_node_values[1]->string_value);
            delete poRet;
            return nullptr;
        }

        switch( node->nOperation )
        {
          case SWQ_GT:
            poRet->int_value = OGRCompareDate(&sField0, &sField1) > 0;
            break;

          case SWQ_GE:
            poRet->int_value = OGRCompareDate(&sField0, &sField1) >= 0;
            break;

          case SWQ_LT:
            poRet->int_value = OGRCompareDate(&sField0, &sField1) < 0;
            break;

          case SWQ_LE:
            poRet->int_value = OGRCompareDate(&sField0, &sField1) <= 0;
            break;

          case SWQ_EQ:
            poRet->int_value = OGRCompareDate(&sField0, &sField1) == 0;
            break;

          case SWQ_BETWEEN:
          {
              OGRField sField2;
              if( !OGRParseDate(sub_node_values[2]->string_value, &sField2, 0) )
              {
                  CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Failed to parse date '%s' evaluating OGR WHERE expression",
                    sub_node_values[2]->string_value);
                  delete poRet;
                  return nullptr;
              }

              poRet->int_value =
                  (OGRCompareDate(&sField0, &sField1) >= 0)
                  && (OGRCompareDate(&sField0, &sField2) <= 0);
          }
          break;

          case SWQ_IN:
          {
            OGRField sFieldIn;
            bool bFound = false;
            for( int i = 1; i < node->nSubExprCount; ++i )
            {
              if( !OGRParseDate(sub_node_values[i]->string_value, &sFieldIn, 0) )
              {
                  CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Failed to parse date '%s' evaluating OGR WHERE expression",
                    sub_node_values[i]->string_value);
                  delete poRet;
                  return nullptr;
              }
              if ( OGRCompareDate(&sField0, &sFieldIn) == 0 )
              {
                bFound = true;
                break;
              }
            }
            poRet->int_value = bFound;
          }
          break;

          default:
            CPLAssert( false );
            delete poRet;
            poRet = nullptr;
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      String operations.                                              */
/* -------------------------------------------------------------------- */
    else
    {
        poRet = new swq_expr_node(0);
        poRet->field_type = node->field_type;

        if( node->nOperation != SWQ_ISNULL )
        {
            for( int i = 0; i < node->nSubExprCount; i++ )
            {
                if( sub_node_values[i]->is_null )
                {
                    if( poRet->field_type == SWQ_BOOLEAN )
                    {
                        poRet->int_value = FALSE;
                        return poRet;
                    }
                    else if( poRet->field_type == SWQ_STRING )
                    {
                        poRet->string_value = CPLStrdup("");
                        poRet->is_null = 1;
                        return poRet;
                    }
                }
            }
        }

        switch( node->nOperation )
        {
          case SWQ_EQ:
          {
            // When comparing timestamps, the +00 at the end might be discarded
            // if the other member has no explicit timezone.
            if( (sub_node_values[0]->field_type == SWQ_TIMESTAMP ||
                 sub_node_values[0]->field_type == SWQ_STRING) &&
                (sub_node_values[1]->field_type == SWQ_TIMESTAMP ||
                 sub_node_values[1]->field_type == SWQ_STRING) &&
                strlen(sub_node_values[0]->string_value) > 3 &&
                strlen(sub_node_values[1]->string_value) > 3 &&
                (strcmp(sub_node_values[0]->string_value + strlen(sub_node_values[0]->string_value)-3, "+00") == 0 &&
                 sub_node_values[1]->string_value[strlen(sub_node_values[1]->string_value)-3] == ':') )
            {
                poRet->int_value =
                    EQUALN(sub_node_values[0]->string_value,
                           sub_node_values[1]->string_value,
                           strlen(sub_node_values[1]->string_value));
            }
            else if( (sub_node_values[0]->field_type == SWQ_TIMESTAMP ||
                      sub_node_values[0]->field_type == SWQ_STRING) &&
                     (sub_node_values[1]->field_type == SWQ_TIMESTAMP ||
                      sub_node_values[1]->field_type == SWQ_STRING) &&
                     strlen(sub_node_values[0]->string_value) > 3 &&
                     strlen(sub_node_values[1]->string_value) > 3 &&
                     (sub_node_values[0]->string_value[strlen(sub_node_values[0]->string_value)-3] == ':') &&
                      strcmp(sub_node_values[1]->string_value + strlen(sub_node_values[1]->string_value)-3, "+00") == 0)
            {
                poRet->int_value =
                    EQUALN(sub_node_values[0]->string_value,
                           sub_node_values[1]->string_value,
                           strlen(sub_node_values[0]->string_value));
            }
            else
            {
                poRet->int_value =
                    strcasecmp(sub_node_values[0]->string_value,
                            sub_node_values[1]->string_value) == 0;
            }
            break;
          }

          case SWQ_NE:
            poRet->int_value =
                strcasecmp(sub_node_values[0]->string_value,
                           sub_node_values[1]->string_value) != 0;
            break;

          case SWQ_GT:
            poRet->int_value =
                strcasecmp(sub_node_values[0]->string_value,
                           sub_node_values[1]->string_value) > 0;
            break;

          case SWQ_LT:
            poRet->int_value =
                strcasecmp(sub_node_values[0]->string_value,
                           sub_node_values[1]->string_value) < 0;
            break;

          case SWQ_GE:
            poRet->int_value =
                strcasecmp(sub_node_values[0]->string_value,
                           sub_node_values[1]->string_value) >= 0;
            break;

          case SWQ_LE:
            poRet->int_value =
                strcasecmp(sub_node_values[0]->string_value,
                           sub_node_values[1]->string_value) <= 0;
            break;

          case SWQ_IN:
          {
              poRet->int_value = 0;
              for( int i = 1; i < node->nSubExprCount; i++ )
              {
                  if( strcasecmp(sub_node_values[0]->string_value,
                                 sub_node_values[i]->string_value) == 0 )
                  {
                      poRet->int_value = 1;
                      break;
                  }
              }
          }
          break;

          case SWQ_BETWEEN:
            poRet->int_value =
                strcasecmp(sub_node_values[0]->string_value,
                           sub_node_values[1]->string_value) >= 0 &&
                strcasecmp(sub_node_values[0]->string_value,
                           sub_node_values[2]->string_value) <= 0;
            break;

          case SWQ_LIKE:
          {
            char chEscape = '\0';
            if( node->nSubExprCount == 3 )
                chEscape = sub_node_values[2]->string_value[0];
            const bool bInsensitive =
                CPLTestBool(CPLGetConfigOption("OGR_SQL_LIKE_AS_ILIKE", "FALSE"));
            poRet->int_value = swq_test_like(sub_node_values[0]->string_value,
                                             sub_node_values[1]->string_value,
                                             chEscape, bInsensitive);
            break;
          }

          case SWQ_ILIKE:
          {
            char chEscape = '\0';
            if( node->nSubExprCount == 3 )
                chEscape = sub_node_values[2]->string_value[0];
            poRet->int_value = swq_test_like(sub_node_values[0]->string_value,
                                             sub_node_values[1]->string_value,
                                             chEscape, true);
            break;
          }

          case SWQ_ISNULL:
            poRet->int_value = sub_node_values[0]->is_null;
            break;

          case SWQ_CONCAT:
          case SWQ_ADD:
          {
              CPLString osResult = sub_node_values[0]->string_value;

              for( int i = 1; i < node->nSubExprCount; i++ )
                  osResult += sub_node_values[i]->string_value;

              poRet->string_value = CPLStrdup(osResult);
              poRet->is_null = sub_node_values[0]->is_null;
              break;
          }

          case SWQ_SUBSTR:
          {
              const char *pszSrcStr = sub_node_values[0]->string_value;

              int nOffset = 0;
              if( SWQ_IS_INTEGER(sub_node_values[1]->field_type) )
                  nOffset = static_cast<int>(sub_node_values[1]->int_value);
              else if( sub_node_values[1]->field_type == SWQ_FLOAT )
                  nOffset = static_cast<int>(sub_node_values[1]->float_value);
              // else
              //     nOffset = 0;

              int nSize = 0;
              if( node->nSubExprCount < 3 )
                  nSize = 100000;
              else if( SWQ_IS_INTEGER(sub_node_values[2]->field_type) )
                  nSize = static_cast<int>(sub_node_values[2]->int_value);
              else if( sub_node_values[2]->field_type == SWQ_FLOAT )
                  nSize = static_cast<int>(sub_node_values[2]->float_value);
              // else
              //    nSize = 0;

              const int nSrcStrLen = static_cast<int>(strlen(pszSrcStr));

              // In SQL, the first character is at offset 1.
              // 0 is considered as 1.
              if( nOffset > 0 )
                  nOffset--;
              // Some implementations allow negative offsets, to start
              // from the end of the string.
              else if( nOffset < 0 )
              {
                  if( nSrcStrLen + nOffset >= 0 )
                      nOffset = nSrcStrLen + nOffset;
                  else
                      nOffset = 0;
              }

              if( nSize < 0 || nOffset > nSrcStrLen )
              {
                  nOffset = 0;
                  nSize = 0;
              }
              else if( nOffset + nSize > nSrcStrLen )
                  nSize = nSrcStrLen - nOffset;

              CPLString osResult = pszSrcStr + nOffset;
              if( static_cast<int>(osResult.size()) > nSize )
                  osResult.resize( nSize );

              poRet->string_value = CPLStrdup(osResult);
              poRet->is_null = sub_node_values[0]->is_null;
              break;
          }

          case SWQ_HSTORE_GET_VALUE:
          {
              const char *pszHStore = sub_node_values[0]->string_value;
              const char *pszSearchedKey = sub_node_values[1]->string_value;
              char* pszRet = OGRHStoreGetValue(pszHStore, pszSearchedKey);
              poRet->string_value = pszRet ? pszRet : CPLStrdup("");
              poRet->is_null = (pszRet == nullptr);
              break;
          }

          default:
            CPLAssert( false );
            delete poRet;
            poRet = nullptr;
            break;
        }
    }

    return poRet;
}

/************************************************************************/
/*                SWQAutoPromoteIntegerToInteger64OrFloat()             */
/************************************************************************/

static void SWQAutoPromoteIntegerToInteger64OrFloat( swq_expr_node *poNode )

{
    if( poNode->nSubExprCount < 2 )
        return;

    swq_field_type eArgType = poNode->papoSubExpr[0]->field_type;

    // We allow mixes of integer, integer64 and float, and string and dates.
    // When encountered, we promote integers/integer64 to floats,
    // integer to integer64 and strings to dates.  We do that now.
    for( int i = 1; i < poNode->nSubExprCount; i++ )
    {
        swq_expr_node *poSubNode = poNode->papoSubExpr[i];
        if( SWQ_IS_INTEGER(eArgType)
            && poSubNode->field_type == SWQ_FLOAT )
            eArgType = SWQ_FLOAT;
        else if( eArgType == SWQ_INTEGER
                 && poSubNode->field_type == SWQ_INTEGER64 )
            eArgType = SWQ_INTEGER64;
    }

    for( int i = 0; i < poNode->nSubExprCount; i++ )
    {
        swq_expr_node *poSubNode = poNode->papoSubExpr[i];

        if( eArgType == SWQ_FLOAT
            && SWQ_IS_INTEGER(poSubNode->field_type) )
        {
            if( poSubNode->eNodeType == SNT_CONSTANT )
            {
                poSubNode->float_value = static_cast<double>(poSubNode->int_value);
                poSubNode->field_type = SWQ_FLOAT;
            }
        }
        else if( eArgType == SWQ_INTEGER64 && poSubNode->field_type == SWQ_INTEGER )
        {
            if( poSubNode->eNodeType == SNT_CONSTANT )
            {
                poSubNode->field_type = SWQ_INTEGER64;
            }
        }
    }
}

/************************************************************************/
/*                    SWQAutoPromoteStringToDateTime()                  */
/************************************************************************/

static void SWQAutoPromoteStringToDateTime( swq_expr_node *poNode )

{
    if( poNode->nSubExprCount < 2 )
        return;

    swq_field_type eArgType = poNode->papoSubExpr[0]->field_type;

    // We allow mixes of integer and float, and string and dates.
    // When encountered, we promote integers to floats, and strings to
    // dates.  We do that now.
    for( int i = 1; i < poNode->nSubExprCount; i++ )
    {
        swq_expr_node *poSubNode = poNode->papoSubExpr[i];

        if( eArgType == SWQ_STRING
            && (poSubNode->field_type == SWQ_DATE
                || poSubNode->field_type == SWQ_TIME
                || poSubNode->field_type == SWQ_TIMESTAMP) )
            eArgType = SWQ_TIMESTAMP;
    }

    for( int i = 0; i < poNode->nSubExprCount; i++ )
    {
        swq_expr_node *poSubNode = poNode->papoSubExpr[i];

        if( eArgType == SWQ_TIMESTAMP
            && (poSubNode->field_type == SWQ_STRING
                || poSubNode->field_type == SWQ_DATE
                || poSubNode->field_type == SWQ_TIME) )
        {
            if( poSubNode->eNodeType == SNT_CONSTANT )
            {
                poSubNode->field_type = SWQ_TIMESTAMP;
            }
        }
    }
}

/************************************************************************/
/*                    SWQAutoConvertStringToNumeric()                   */
/*                                                                      */
/*      Convert string constants to integer or float constants          */
/*      when there is a mix of arguments of type numeric and string     */
/************************************************************************/

static void SWQAutoConvertStringToNumeric( swq_expr_node *poNode )

{
    if( poNode->nSubExprCount < 2 )
        return;

    swq_field_type eArgType = poNode->papoSubExpr[0]->field_type;

    for( int i = 1; i < poNode->nSubExprCount; i++ )
    {
        swq_expr_node *poSubNode = poNode->papoSubExpr[i];

        // Identify the mixture of the argument type.
        if( (eArgType == SWQ_STRING
            && (SWQ_IS_INTEGER(poSubNode->field_type)
               || poSubNode->field_type == SWQ_FLOAT)) ||
            (SWQ_IS_INTEGER(eArgType)
            && poSubNode->field_type == SWQ_STRING) )
        {
            eArgType = SWQ_FLOAT;
            break;
        }
    }

    for( int i = 0; i < poNode->nSubExprCount; i++ )
    {
        swq_expr_node *poSubNode = poNode->papoSubExpr[i];

        if( eArgType == SWQ_FLOAT
            && poSubNode->field_type == SWQ_STRING )
        {
            if( poSubNode->eNodeType == SNT_CONSTANT )
            {
                // Apply the string to numeric conversion.
                char* endPtr = nullptr;
                poSubNode->float_value =
                    CPLStrtod(poSubNode->string_value, &endPtr);
                if( !(endPtr == nullptr || *endPtr == '\0') )
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                             "Conversion failed when converting the string "
                             "value '%s' to data type float.",
                             poSubNode->string_value);
                    continue;
                }

                // Should also fill the integer value in this case.
                poSubNode->int_value = static_cast<GIntBig>(poSubNode->float_value);
                poSubNode->field_type = SWQ_FLOAT;
            }
        }
    }
}

/************************************************************************/
/*                   SWQCheckSubExprAreNotGeometries()                  */
/************************************************************************/

static bool SWQCheckSubExprAreNotGeometries( swq_expr_node *poNode )
{
    for( int i = 0; i < poNode->nSubExprCount; i++ )
    {
        if( poNode->papoSubExpr[i]->field_type == SWQ_GEOMETRY )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                        "Cannot use geometry field in this operation." );
            return false;
        }
    }
    return true;
}

/************************************************************************/
/*                         SWQGeneralChecker()                          */
/*                                                                      */
/*      Check the general purpose functions have appropriate types,     */
/*      and count and indicate the function return type under the       */
/*      circumstances.                                                  */
/************************************************************************/

swq_field_type SWQGeneralChecker( swq_expr_node *poNode,
                                  int bAllowMismatchTypeOnFieldComparison )

{
    swq_field_type eRetType = SWQ_ERROR;
    swq_field_type eArgType = SWQ_OTHER;
    // int nArgCount = -1;

    switch( poNode->nOperation )
    {
      case SWQ_AND:
      case SWQ_OR:
      case SWQ_NOT:
        if( !SWQCheckSubExprAreNotGeometries(poNode) )
            return SWQ_ERROR;
        eRetType = SWQ_BOOLEAN;
        break;

      case SWQ_EQ:
      case SWQ_NE:
      case SWQ_GT:
      case SWQ_LT:
      case SWQ_GE:
      case SWQ_LE:
      case SWQ_IN:
      case SWQ_BETWEEN:
        if( !SWQCheckSubExprAreNotGeometries(poNode) )
            return SWQ_ERROR;
        eRetType = SWQ_BOOLEAN;
        SWQAutoConvertStringToNumeric( poNode );
        SWQAutoPromoteIntegerToInteger64OrFloat( poNode );
        SWQAutoPromoteStringToDateTime( poNode );
        eArgType = poNode->papoSubExpr[0]->field_type;
        break;

      case SWQ_ISNULL:
        eRetType = SWQ_BOOLEAN;
        break;

      case SWQ_LIKE:
      case SWQ_ILIKE:
        if( !SWQCheckSubExprAreNotGeometries(poNode) )
            return SWQ_ERROR;
        eRetType = SWQ_BOOLEAN;
        eArgType = SWQ_STRING;
        break;

      case SWQ_ADD:
        if( !SWQCheckSubExprAreNotGeometries(poNode) )
            return SWQ_ERROR;
        SWQAutoPromoteIntegerToInteger64OrFloat( poNode );
        if( poNode->papoSubExpr[0]->field_type == SWQ_STRING )
        {
            eRetType = SWQ_STRING;
            eArgType = SWQ_STRING;
        }
        else if( poNode->papoSubExpr[0]->field_type == SWQ_FLOAT || poNode->papoSubExpr[1]->field_type == SWQ_FLOAT )
        {
            eRetType = SWQ_FLOAT;
            eArgType = SWQ_FLOAT;
        }
        else if( poNode->papoSubExpr[0]->field_type == SWQ_INTEGER64 || poNode->papoSubExpr[1]->field_type == SWQ_INTEGER64 )
        {
            eRetType = SWQ_INTEGER64;
            eArgType = SWQ_INTEGER64;
        }
        else
        {
            eRetType = SWQ_INTEGER;
            eArgType = SWQ_INTEGER;
        }
        break;

      case SWQ_SUBTRACT:
      case SWQ_MULTIPLY:
      case SWQ_DIVIDE:
      case SWQ_MODULUS:
        if( !SWQCheckSubExprAreNotGeometries(poNode) )
            return SWQ_ERROR;
        SWQAutoPromoteIntegerToInteger64OrFloat( poNode );
        if( poNode->papoSubExpr[0]->field_type == SWQ_FLOAT || poNode->papoSubExpr[1]->field_type == SWQ_FLOAT )
        {
            eRetType = SWQ_FLOAT;
            eArgType = SWQ_FLOAT;
        }
        else if( poNode->papoSubExpr[0]->field_type == SWQ_INTEGER64 || poNode->papoSubExpr[1]->field_type == SWQ_INTEGER64 )
        {
            eRetType = SWQ_INTEGER64;
            eArgType = SWQ_INTEGER64;
        }
        else
        {
            eRetType = SWQ_INTEGER;
            eArgType = SWQ_INTEGER;
        }
        break;

      case SWQ_CONCAT:
        if( !SWQCheckSubExprAreNotGeometries(poNode) )
            return SWQ_ERROR;
        eRetType = SWQ_STRING;
        eArgType = SWQ_STRING;
        break;

      case SWQ_SUBSTR:
        if( !SWQCheckSubExprAreNotGeometries(poNode) )
            return SWQ_ERROR;
        eRetType = SWQ_STRING;
        if( poNode->nSubExprCount > 3 || poNode->nSubExprCount < 2 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Expected 2 or 3 arguments to SUBSTR(), but got %d.",
                      poNode->nSubExprCount );
            return SWQ_ERROR;
        }
        if( poNode->papoSubExpr[0]->field_type != SWQ_STRING
            || poNode->papoSubExpr[1]->field_type != SWQ_INTEGER
            || (poNode->nSubExprCount > 2
                && poNode->papoSubExpr[2]->field_type != SWQ_INTEGER) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Wrong argument type for SUBSTR(), "
                     "expected SUBSTR(string,int,int) or SUBSTR(string,int).");
            return SWQ_ERROR;
        }
        break;

      case SWQ_HSTORE_GET_VALUE:
        if( !SWQCheckSubExprAreNotGeometries(poNode) )
            return SWQ_ERROR;
        eRetType = SWQ_STRING;
        if( poNode->nSubExprCount != 2 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Expected 2 arguments to hstore_get_value(), but got %d.",
                      poNode->nSubExprCount );
            return SWQ_ERROR;
        }
        if( poNode->papoSubExpr[0]->field_type != SWQ_STRING
            || poNode->papoSubExpr[1]->field_type != SWQ_STRING )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Wrong argument type for hstore_get_value(), "
                      "expected hstore_get_value(string,string)." );
            return SWQ_ERROR;
        }
        break;

      default:
      {
          const swq_operation *poOp =
              swq_op_registrar::GetOperator(poNode->nOperation);

          CPLError( CE_Failure, CPLE_AppDefined,
                    "SWQGeneralChecker() called on unsupported operation %s.",
                    poOp->pszName);
          return SWQ_ERROR;
      }
    }
/* -------------------------------------------------------------------- */
/*      Check argument types.                                           */
/* -------------------------------------------------------------------- */
    if( eArgType != SWQ_OTHER )
    {
        if( SWQ_IS_INTEGER(eArgType) || eArgType == SWQ_BOOLEAN )
            eArgType = SWQ_FLOAT;

        for( int i = 0; i < poNode->nSubExprCount; i++ )
        {
            swq_field_type eThisArgType = poNode->papoSubExpr[i]->field_type;
            if( SWQ_IS_INTEGER(eThisArgType) || eThisArgType == SWQ_BOOLEAN )
                eThisArgType = SWQ_FLOAT;

            if( eArgType != eThisArgType )
            {
                // Convenience for join. We allow comparing numeric columns
                // and string columns, by casting string columns to numeric.
                if( bAllowMismatchTypeOnFieldComparison &&
                    poNode->nSubExprCount == 2 &&
                    poNode->nOperation == SWQ_EQ &&
                    poNode->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
                    poNode->papoSubExpr[i]->eNodeType == SNT_COLUMN &&
                    eArgType == SWQ_FLOAT && eThisArgType == SWQ_STRING )
                {
                    swq_expr_node* poNewNode = new swq_expr_node(SWQ_CAST);
                    poNewNode->PushSubExpression(poNode->papoSubExpr[i]);
                    poNewNode->PushSubExpression(new swq_expr_node("FLOAT"));
                    SWQCastChecker(poNewNode, FALSE);
                    poNode->papoSubExpr[i] = poNewNode;
                    break;
                }
                if( bAllowMismatchTypeOnFieldComparison &&
                    poNode->nSubExprCount == 2 &&
                    poNode->nOperation == SWQ_EQ &&
                    poNode->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
                    poNode->papoSubExpr[i]->eNodeType == SNT_COLUMN &&
                    eThisArgType == SWQ_FLOAT && eArgType == SWQ_STRING )
                {
                    swq_expr_node* poNewNode = new swq_expr_node(SWQ_CAST);
                    poNewNode->PushSubExpression(poNode->papoSubExpr[0]);
                    poNewNode->PushSubExpression(new swq_expr_node("FLOAT"));
                    SWQCastChecker(poNewNode, FALSE);
                    poNode->papoSubExpr[0] = poNewNode;
                    break;
                }

                const swq_operation *poOp =
                    swq_op_registrar::GetOperator(poNode->nOperation);

                CPLError( CE_Failure, CPLE_AppDefined,
                          "Type mismatch or improper type of arguments "
                          "to %s operator.",
                          poOp->pszName );
                return SWQ_ERROR;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Validate the arg count if requested.                            */
/* -------------------------------------------------------------------- */
#if 0
    // nArgCount was always -1, so this block was never executed.
    if( nArgCount != -1
        && nArgCount != poNode->nSubExprCount )
    {
        const swq_operation *poOp =
            swq_op_registrar::GetOperator(poNode->nOperation);

        CPLError( CE_Failure, CPLE_AppDefined,
                  "Expected %d arguments to %s, but got %d arguments.",
                  nArgCount,
                  poOp->pszName,
                  poNode->nSubExprCount );
        return SWQ_ERROR;
    }
#endif

    return eRetType;
}

/************************************************************************/
/*                          SWQCastEvaluator()                          */
/************************************************************************/

swq_expr_node *SWQCastEvaluator( swq_expr_node *node,
                                 swq_expr_node **sub_node_values )

{
    swq_expr_node *poRetNode = nullptr;
    swq_expr_node *poSrcNode = sub_node_values[0];

    switch( node->field_type )
    {
        case SWQ_INTEGER:
        {
            poRetNode = new swq_expr_node( 0 );
            poRetNode->is_null = poSrcNode->is_null;

            switch( poSrcNode->field_type )
            {
                case SWQ_INTEGER:
                case SWQ_BOOLEAN:
                    poRetNode->int_value = poSrcNode->int_value;
                    break;

                case SWQ_INTEGER64:
                    // TODO: Warn in case of overflow?
                    poRetNode->int_value =
                      static_cast<int>(poSrcNode->int_value);
                    break;

                case SWQ_FLOAT:
                    // TODO: Warn in case of overflow?
                    poRetNode->int_value =
                        static_cast<int>(poSrcNode->float_value);
                    break;

                default:
                    poRetNode->int_value = atoi(poSrcNode->string_value);
                    break;
            }
        }
        break;

        case SWQ_INTEGER64:
        {
            poRetNode = new swq_expr_node( 0 );
            poRetNode->is_null = poSrcNode->is_null;
            poRetNode->field_type = SWQ_INTEGER64;

            switch( poSrcNode->field_type )
            {
                case SWQ_INTEGER:
                case SWQ_INTEGER64:
                case SWQ_BOOLEAN:
                    poRetNode->int_value = poSrcNode->int_value;
                    break;

                case SWQ_FLOAT:
                    poRetNode->int_value =
                        static_cast<GIntBig>(poSrcNode->float_value);
                    break;

                default:
                    poRetNode->int_value =
                        CPLAtoGIntBig(poSrcNode->string_value);
                    break;
            }
        }
        break;

        case SWQ_FLOAT:
        {
            poRetNode = new swq_expr_node( 0.0 );
            poRetNode->is_null = poSrcNode->is_null;

            switch( poSrcNode->field_type )
            {
                case SWQ_INTEGER:
                case SWQ_INTEGER64:
                case SWQ_BOOLEAN:
                    poRetNode->float_value =
                        static_cast<double>(poSrcNode->int_value);
                    break;

                case SWQ_FLOAT:
                    poRetNode->float_value = poSrcNode->float_value;
                    break;

                default:
                    poRetNode->float_value = CPLAtof(poSrcNode->string_value);
                    break;
            }
        }
        break;

        case SWQ_GEOMETRY:
        {
            poRetNode = new swq_expr_node( static_cast<OGRGeometry*>(nullptr) );
            if( !poSrcNode->is_null )
            {
                switch( poSrcNode->field_type )
                {
                    case SWQ_GEOMETRY:
                    {
                        poRetNode->geometry_value =
                            poSrcNode->geometry_value->clone();
                        poRetNode->is_null = FALSE;
                        break;
                    }

                    case SWQ_STRING:
                    {
                        OGRGeometryFactory::createFromWkt(
                            poSrcNode->string_value, nullptr,
                            &(poRetNode->geometry_value));
                        if( poRetNode->geometry_value != nullptr )
                            poRetNode->is_null = FALSE;
                        break;
                    }

                    default:
                        break;
                }
            }
            break;
        }

        // Everything else is a string.
        default:
        {
            CPLString osRet;

            switch( poSrcNode->field_type )
            {
                case SWQ_INTEGER:
                case SWQ_BOOLEAN:
                case SWQ_INTEGER64:
                    osRet.Printf( CPL_FRMT_GIB, poSrcNode->int_value );
                    break;

                case SWQ_FLOAT:
                    osRet.Printf( "%.15g", poSrcNode->float_value );
                    break;

                case SWQ_GEOMETRY:
                {
                    if( poSrcNode->geometry_value != nullptr )
                    {
                        char* pszWKT = nullptr;
                        poSrcNode->geometry_value->exportToWkt(&pszWKT);
                        osRet = pszWKT;
                        CPLFree(pszWKT);
                    }
                    else
                        osRet = "";
                    break;
                }

                default:
                    osRet = poSrcNode->string_value;
                    break;
            }

            if( node->nSubExprCount > 2 )
            {
                int nWidth = static_cast<int>(sub_node_values[2]->int_value);
                if( nWidth > 0 && static_cast<int>(osRet.size()) > nWidth )
                    osRet.resize(nWidth);
            }

            poRetNode = new swq_expr_node( osRet.c_str() );
            poRetNode->is_null = poSrcNode->is_null;
        }
    }

    return poRetNode;
}

/************************************************************************/
/*                           SWQCastChecker()                           */
/************************************************************************/

swq_field_type SWQCastChecker( swq_expr_node *poNode,
                               int /* bAllowMismatchTypeOnFieldComparison */ )

{
    swq_field_type eType = SWQ_ERROR;
    const char *pszTypeName = poNode->papoSubExpr[1]->string_value;

    if( poNode->papoSubExpr[0]->field_type == SWQ_GEOMETRY &&
        !(EQUAL(pszTypeName, "character") ||
          EQUAL(pszTypeName, "geometry")) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Cannot cast geometry to %s",
                  pszTypeName );
    }
    else if( EQUAL(pszTypeName, "boolean") )
    {
        eType = SWQ_BOOLEAN;
    }
    else if( EQUAL(pszTypeName, "character") )
    {
        eType = SWQ_STRING;
    }
    else if( EQUAL(pszTypeName, "integer") )
    {
        eType = SWQ_INTEGER;
    }
    else if( EQUAL(pszTypeName, "bigint") )
    {
        // Handle CAST(fid AS bigint) by changing the field_type of fid to
        // Integer64.  A bit of a hack.
        if( poNode->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
            poNode->papoSubExpr[0]->field_type == SWQ_INTEGER &&
            strcmp(poNode->papoSubExpr[0]->string_value, "fid") == 0 )
        {
            poNode->papoSubExpr[0]->field_type = SWQ_INTEGER64;
        }
        eType = SWQ_INTEGER64;
    }
    else if( EQUAL(pszTypeName, "smallint") )
    {
        eType = SWQ_INTEGER;
    }
    else if( EQUAL(pszTypeName, "float") )
    {
        eType = SWQ_FLOAT;
    }
    else if( EQUAL(pszTypeName, "numeric") )
    {
        eType = SWQ_FLOAT;
    }
    else if( EQUAL(pszTypeName, "timestamp") )
    {
        eType = SWQ_TIMESTAMP;
    }
    else if( EQUAL(pszTypeName, "date") )
    {
        eType = SWQ_DATE;
    }
    else if( EQUAL(pszTypeName, "time") )
    {
        eType = SWQ_TIME;
    }
    else if( EQUAL(pszTypeName, "geometry") )
    {
        if( !(poNode->papoSubExpr[0]->field_type == SWQ_GEOMETRY ||
              poNode->papoSubExpr[0]->field_type == SWQ_STRING) )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot cast %s to geometry",
                     SWQFieldTypeToString(poNode->papoSubExpr[0]->field_type));
        }
        else
            eType = SWQ_GEOMETRY;
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unrecognized typename %s in CAST operator.",
                  pszTypeName );
    }

    poNode->field_type = eType;

    return eType;
}
