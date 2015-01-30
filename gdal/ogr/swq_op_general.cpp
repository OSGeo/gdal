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
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_conv.h"
#include "swq.h"
#include "ogr_geometry.h"

/************************************************************************/
/*                           swq_test_like()                            */
/*                                                                      */
/*      Does input match pattern?                                       */
/************************************************************************/

int swq_test_like( const char *input, const char *pattern, char chEscape )

{
    if( input == NULL || pattern == NULL )
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
            if( tolower(*pattern) != tolower(*input) )
                return 0;
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
            int   eat;

            if( pattern[1] == '\0' )
                return 1;

            /* try eating varying amounts of the input till we get a positive*/
            for( eat = 0; input[eat] != '\0'; eat++ )
            {
                if( swq_test_like(input+eat,pattern+1, chEscape) )
                    return 1;
            }

            return 0;
        }
        else
        {
            if( tolower(*pattern) != tolower(*input) )
                return 0;
            else
            {
                input++;
                pattern++;
            }
        }
    }

    if( *pattern != '\0' && strcmp(pattern,"%") != 0 )
        return 0;
    else
        return 1;
}

/************************************************************************/
/*                        OGRHStoreGetValue()                           */
/************************************************************************/

static char* OGRHStoreCheckEnd(char* pszIter, int bIsKey)
{
    pszIter ++;
    for( ; *pszIter != '\0'; pszIter ++ )
    {
        if( bIsKey )
        {
            if( *pszIter == ' ' )
                ;
            else if( *pszIter == '=' && pszIter[1] == '>' )
                return pszIter + 2;
            else
                return NULL;
        }
        else
        {
            if( *pszIter == ' ' )
                ;
            else if( *pszIter == ',' )
                return pszIter + 1;
            else
                return NULL;
        }
    }
    return pszIter;
}

static char* OGRHStoreGetNextString(char* pszIter,
                                    char** ppszOut,
                                    int bIsKey)
{
    char ch;
    int bInString = FALSE;
    char* pszOut = NULL;
    *ppszOut = NULL;
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
                pszIter ++;
                if( (ch = *pszIter) == '\0' )
                    return NULL;
            }
            *pszOut = ch;
            pszOut ++;
        }
        else
        {
            if( ch == ' ' )
            {
                if( pszOut != NULL )
                {
                    *pszIter = '\0';
                    return OGRHStoreCheckEnd(pszIter, bIsKey);
                }
            }
            else if( bIsKey && ch == '=' && pszIter[1] == '>' )
            {
                if( pszOut != NULL )
                {
                    *pszIter = '\0';
                    return pszIter + 2;
                }
            }
            else if( !bIsKey && ch == ',' )
            {
                if( pszOut != NULL )
                {
                    *pszIter = '\0';
                    return pszIter + 1;
                }
            }
            else if( ch == '"' )
            {
                pszOut = *ppszOut = pszIter + 1;
                bInString = TRUE;
            }
            else if( pszOut == NULL )
                pszOut = *ppszOut = pszIter;
        }
    }

    if( !bInString && pszOut != NULL )
    {
        return pszIter;
    }
    return NULL;
}

static char* OGRHStoreGetNextKeyValue(char* pszHStore,
                                      char** ppszKey,
                                       char** ppszValue)
{
    char* pszNext = OGRHStoreGetNextString(pszHStore, ppszKey, TRUE);
    if( pszNext == NULL || *pszNext == '\0' )
        return NULL;
    return OGRHStoreGetNextString(pszNext, ppszValue, FALSE);
}

char* OGRHStoreGetValue(const char* pszHStore, const char* pszSearchedKey)
{
    char* pszHStoreDup = CPLStrdup(pszHStore);
    char* pszHStoreIter = pszHStoreDup;
    char* pszRet = NULL;

    while( TRUE )
    {
        char* pszKey, *pszValue;
        pszHStoreIter = OGRHStoreGetNextKeyValue(pszHStoreIter, &pszKey, &pszValue);
        if( pszHStoreIter == NULL )
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

/************************************************************************/
/*                        SWQGeneralEvaluator()                         */
/************************************************************************/

swq_expr_node *SWQGeneralEvaluator( swq_expr_node *node,
                                    swq_expr_node **sub_node_values )

{
    swq_expr_node *poRet = NULL;

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
            sub_node_values[0]->float_value = sub_node_values[0]->int_value;
        if( node->nSubExprCount > 1 &&
            SWQ_IS_INTEGER(sub_node_values[1]->field_type) )
            sub_node_values[1]->float_value = sub_node_values[1]->int_value;

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
                    else if( SWQ_IS_INTEGER(poRet->field_type) ||
                             node->nOperation == SWQ_MODULUS )
                    {
                        poRet->field_type = SWQ_INTEGER;
                        poRet->int_value = 0;
                        poRet->is_null = 1;
                        return poRet;
                    }
                }
            }
        }

        switch( (swq_op) node->nOperation )
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
              int i;
              poRet->int_value = 0;
              for( i = 1; i < node->nSubExprCount; i++ )
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
            GIntBig nRight = (GIntBig) sub_node_values[1]->float_value;
            poRet->field_type = SWQ_INTEGER;
            if (nRight == 0)
                poRet->int_value = INT_MAX;
            else
                poRet->int_value = ((GIntBig) sub_node_values[0]->float_value)
                    % nRight;
            break;
          }

          default:
            CPLAssert( FALSE );
            delete poRet;
            poRet = NULL;
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

        switch( (swq_op) node->nOperation )
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
              int i;
              poRet->int_value = 0;
              for( i = 1; i < node->nSubExprCount; i++ )
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
            poRet->int_value = sub_node_values[0]->int_value 
                + sub_node_values[1]->int_value;
            break;
            
          case SWQ_SUBTRACT:
            poRet->int_value = sub_node_values[0]->int_value 
                - sub_node_values[1]->int_value;
            break;
            
          case SWQ_MULTIPLY:
            poRet->int_value = sub_node_values[0]->int_value 
                * sub_node_values[1]->int_value;
            break;
            
          case SWQ_DIVIDE:
            if( sub_node_values[1]->int_value == 0 )
                poRet->int_value = INT_MAX;
            else
                poRet->int_value = sub_node_values[0]->int_value 
                    / sub_node_values[1]->int_value;
            break;
            
          case SWQ_MODULUS:
            if( sub_node_values[1]->int_value == 0 )
                poRet->int_value = INT_MAX;
            else
                poRet->int_value = sub_node_values[0]->int_value
                    % sub_node_values[1]->int_value;
            break;
            
          default:
            CPLAssert( FALSE );
            delete poRet;
            poRet = NULL;
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

        switch( (swq_op) node->nOperation )
        {
          case SWQ_EQ:
            poRet->int_value = 
                strcasecmp(sub_node_values[0]->string_value,
                           sub_node_values[1]->string_value) == 0;
            break;

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
              int i;
              poRet->int_value = 0;
              for( i = 1; i < node->nSubExprCount; i++ )
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
            poRet->int_value = swq_test_like(sub_node_values[0]->string_value,
                                             sub_node_values[1]->string_value,
                                             chEscape);
            break;
          }

          case SWQ_ISNULL:
            poRet->int_value = sub_node_values[0]->is_null;
            break;

          case SWQ_CONCAT:
          case SWQ_ADD:
          {
              CPLString osResult = sub_node_values[0]->string_value;
              int i;

              for( i = 1; i < node->nSubExprCount; i++ )
                  osResult += sub_node_values[i]->string_value;
              
              poRet->string_value = CPLStrdup(osResult);
              poRet->is_null = sub_node_values[0]->is_null;
              break;
          }
            
          case SWQ_SUBSTR:
          {
              int nOffset, nSize;
              const char *pszSrcStr = sub_node_values[0]->string_value;

              if( SWQ_IS_INTEGER(sub_node_values[1]->field_type) )
                  nOffset = (int)sub_node_values[1]->int_value;
              else if( sub_node_values[1]->field_type == SWQ_FLOAT )
                  nOffset = (int) sub_node_values[1]->float_value; 
              else
                  nOffset = 0;

              if( node->nSubExprCount < 3 )
                  nSize = 100000;
              else if( SWQ_IS_INTEGER(sub_node_values[2]->field_type) )
                  nSize = (int)sub_node_values[2]->int_value;
              else if( sub_node_values[2]->field_type == SWQ_FLOAT )
                  nSize = (int) sub_node_values[2]->float_value; 
              else
                  nSize = 0;

              int nSrcStrLen = (int)strlen(pszSrcStr);


              /* In SQL, the first character is at offset 1 */
              /* And 0 is considered as 1 */
              if (nOffset > 0)
                  nOffset --;
              /* Some implementations allow negative offsets, to start */
              /* from the end of the string */
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
              if( (int)osResult.size() > nSize )
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
              poRet->is_null = (pszRet == NULL);
              break;
          }

          default:
            CPLAssert( FALSE );
            delete poRet;
            poRet = NULL;
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
    int i;

    // We allow mixes of integer, integer64 and float, and string and dates.
    // When encountered, we promote integers/integer64 to floats, 
    // integer to integer64 and strings to dates.  We do that now.
    for( i = 1; i < poNode->nSubExprCount; i++ )
    {
        swq_expr_node *poSubNode = poNode->papoSubExpr[i];
        if( SWQ_IS_INTEGER(eArgType)
            && poSubNode->field_type == SWQ_FLOAT )
            eArgType = SWQ_FLOAT;
        else if( eArgType == SWQ_INTEGER
                 && poSubNode->field_type == SWQ_INTEGER64 )
            eArgType = SWQ_INTEGER64;
    }
    
    for( i = 0; i < poNode->nSubExprCount; i++ )
    {
        swq_expr_node *poSubNode = poNode->papoSubExpr[i];

        if( eArgType == SWQ_FLOAT
            && SWQ_IS_INTEGER(poSubNode->field_type) )
        {
            if( poSubNode->eNodeType == SNT_CONSTANT )
            {
                poSubNode->float_value = (double) poSubNode->int_value;
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
    int i;

    // We allow mixes of integer and float, and string and dates.
    // When encountered, we promote integers to floats, and strings to
    // dates.  We do that now.
    for( i = 1; i < poNode->nSubExprCount; i++ )
    {
        swq_expr_node *poSubNode = poNode->papoSubExpr[i];

        if( eArgType == SWQ_STRING
            && (poSubNode->field_type == SWQ_DATE
                || poSubNode->field_type == SWQ_TIME
                || poSubNode->field_type == SWQ_TIMESTAMP) )
            eArgType = SWQ_TIMESTAMP;
    }
    
    for( i = 0; i < poNode->nSubExprCount; i++ )
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
    int i;

    for( i = 1; i < poNode->nSubExprCount; i++ )
    {
        swq_expr_node *poSubNode = poNode->papoSubExpr[i];

        /* identify the mixture of the argument type */
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
    
    for( i = 0; i < poNode->nSubExprCount; i++ )
    {
        swq_expr_node *poSubNode = poNode->papoSubExpr[i];

        if( eArgType == SWQ_FLOAT
            && poSubNode->field_type == SWQ_STRING )
        {
            if( poSubNode->eNodeType == SNT_CONSTANT )
            {
                /* apply the string to numeric conversion */
                char* endPtr = NULL;
                poSubNode->float_value = CPLStrtod(poSubNode->string_value, &endPtr);
                if ( !(endPtr == NULL || *endPtr == '\0') )
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                             "Conversion failed when converting the string value '%s' to data type float.",
                             poSubNode->string_value);
                    continue;
                }

                /* we should also fill the integer value in this case */
                poSubNode->int_value = (GIntBig)poSubNode->float_value;
                poSubNode->field_type = SWQ_FLOAT;
            }
        }
    }
}

/************************************************************************/
/*                   SWQCheckSubExprAreNotGeometries()                  */
/************************************************************************/

static int SWQCheckSubExprAreNotGeometries( swq_expr_node *poNode )
{
    for( int i = 0; i < poNode->nSubExprCount; i++ )
    {
        if( poNode->papoSubExpr[i]->field_type == SWQ_GEOMETRY )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                        "Cannot use geometry field in this operation." );
            return FALSE;
        }
    }
    return TRUE;
}

/************************************************************************/
/*                         SWQGeneralChecker()                          */
/*                                                                      */
/*      Check the general purpose functions have appropriate types,     */
/*      and count and indicate the function return type under the       */
/*      circumstances.                                                  */
/************************************************************************/

swq_field_type SWQGeneralChecker( swq_expr_node *poNode )

{									
    swq_field_type eRetType = SWQ_ERROR;
    swq_field_type eArgType = SWQ_OTHER;
    int nArgCount = -1;

    switch( (swq_op) poNode->nOperation )
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
        if( !SWQCheckSubExprAreNotGeometries(poNode) )
            return SWQ_ERROR;
        eRetType = SWQ_BOOLEAN;
        eArgType = SWQ_STRING;
        break;

      case SWQ_MODULUS:
        if( !SWQCheckSubExprAreNotGeometries(poNode) )
            return SWQ_ERROR;
        eRetType = SWQ_INTEGER;
        eArgType = SWQ_INTEGER;
        break;

      case SWQ_ADD:
        if( !SWQCheckSubExprAreNotGeometries(poNode) )
            return SWQ_ERROR;
        SWQAutoPromoteIntegerToInteger64OrFloat( poNode );
        if( poNode->papoSubExpr[0]->field_type == SWQ_STRING )
            eRetType = eArgType = SWQ_STRING;
        else if( poNode->papoSubExpr[0]->field_type == SWQ_FLOAT )
            eRetType = eArgType = SWQ_FLOAT;
        else if( poNode->papoSubExpr[0]->field_type == SWQ_INTEGER64 )
            eRetType = eArgType = SWQ_INTEGER64;
        else
            eRetType = eArgType = SWQ_INTEGER;
        break;

      case SWQ_SUBTRACT:
      case SWQ_MULTIPLY:
      case SWQ_DIVIDE:
        if( !SWQCheckSubExprAreNotGeometries(poNode) )
            return SWQ_ERROR;
        SWQAutoPromoteIntegerToInteger64OrFloat( poNode );
        if( poNode->papoSubExpr[0]->field_type == SWQ_FLOAT )
            eRetType = eArgType = SWQ_FLOAT;
        else if( poNode->papoSubExpr[0]->field_type == SWQ_INTEGER64 )
            eRetType = eArgType = SWQ_INTEGER64;
        else
            eRetType = eArgType = SWQ_INTEGER;
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
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Wrong argument type for SUBSTR(), expected SUBSTR(string,int,int) or SUBSTR(string,int)." );
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
                      "Wrong argument type for hstore_get_value(), expected hstore_get_value(string,string)." );
            return SWQ_ERROR;
        }
        break;
        
      default:
      {
          const swq_operation *poOp = 
              swq_op_registrar::GetOperator((swq_op)poNode->nOperation);
          
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
        int i;

        if( SWQ_IS_INTEGER(eArgType) || eArgType == SWQ_BOOLEAN )
            eArgType = SWQ_FLOAT;

        for( i = 0; i < poNode->nSubExprCount; i++ )
        {
            swq_field_type eThisArgType = poNode->papoSubExpr[i]->field_type;
            if( SWQ_IS_INTEGER(eThisArgType) ||  eThisArgType == SWQ_BOOLEAN )
                eThisArgType = SWQ_FLOAT;

            if( eArgType != eThisArgType )
            {
                const swq_operation *poOp = 
                    swq_op_registrar::GetOperator((swq_op)poNode->nOperation);
                
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Type mismatch or improper type of arguments to %s operator.",
                          poOp->pszName );
                return SWQ_ERROR;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Validate the arg count if requested.                            */
/* -------------------------------------------------------------------- */
    if( nArgCount != -1 
        && nArgCount != poNode->nSubExprCount )
    {
        const swq_operation *poOp = 
            swq_op_registrar::GetOperator((swq_op)poNode->nOperation);
                
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Expected %d arguments to %s, but got %d arguments.",
                  nArgCount,
                  poOp->pszName,
                  poNode->nSubExprCount );
        return SWQ_ERROR;
    }

    return eRetType;
}

/************************************************************************/
/*                          SWQCastEvaluator()                          */
/************************************************************************/

swq_expr_node *SWQCastEvaluator( swq_expr_node *node,
                                 swq_expr_node **sub_node_values )

{
    swq_expr_node *poRetNode = NULL;
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
                    // TODO: warn in case of overflow ?
                    poRetNode->int_value = (int) poSrcNode->int_value;
                    break;

                case SWQ_FLOAT:
                    poRetNode->int_value = (int) poSrcNode->float_value;
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

            switch( poSrcNode->field_type )
            {
                case SWQ_INTEGER:
                case SWQ_INTEGER64:
                case SWQ_BOOLEAN:
                    poRetNode->int_value = poSrcNode->int_value;
                    break;

                case SWQ_FLOAT:
                    poRetNode->int_value = (GIntBig) poSrcNode->float_value;
                    break;

                default:
                    poRetNode->int_value = CPLAtoGIntBig(poSrcNode->string_value);
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
                    poRetNode->float_value = poSrcNode->int_value;
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
            poRetNode = new swq_expr_node( (OGRGeometry*) NULL );
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
                        char* pszTmp = poSrcNode->string_value;
                        OGRGeometryFactory::createFromWkt(&pszTmp, NULL,
                            &(poRetNode->geometry_value));
                        if( poRetNode->geometry_value != NULL )
                            poRetNode->is_null = FALSE;
                        break;
                    }

                    default:
                        break;
                }
            }
            break;
        }

        // everything else is a string.
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
                    if( poSrcNode->geometry_value != NULL )
                    {
                        char* pszWKT;
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
                int nWidth;

                nWidth = sub_node_values[2]->int_value;
                if( nWidth > 0 && (int) strlen(osRet) > nWidth )
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

swq_field_type SWQCastChecker( swq_expr_node *poNode )

{									
    swq_field_type eType = SWQ_ERROR;
    const char *pszTypeName = poNode->papoSubExpr[1]->string_value;

    if( poNode->papoSubExpr[0]->field_type == SWQ_GEOMETRY &&
        !(EQUAL(pszTypeName,"character") ||
          EQUAL(pszTypeName,"geometry")) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Cannot cast geometry to %s",
                  pszTypeName );
    }

    else if( EQUAL(pszTypeName,"boolean") )
        eType = SWQ_BOOLEAN;
    else if( EQUAL(pszTypeName,"character") )
        eType = SWQ_STRING;
    else if( EQUAL(pszTypeName,"integer") )
        eType = SWQ_INTEGER;
    else if( EQUAL(pszTypeName,"bigint") )
        eType = SWQ_INTEGER64;
    else if( EQUAL(pszTypeName,"smallint") )
        eType = SWQ_INTEGER;
    else if( EQUAL(pszTypeName,"float") )
        eType = SWQ_FLOAT;
    else if( EQUAL(pszTypeName,"numeric") )
        eType = SWQ_FLOAT;
    else if( EQUAL(pszTypeName,"timestamp") )
        eType = SWQ_TIMESTAMP;
    else if( EQUAL(pszTypeName,"date") )
        eType = SWQ_DATE;
    else if( EQUAL(pszTypeName,"time") )
        eType = SWQ_TIME;
    else if( EQUAL(pszTypeName,"geometry") )
    {
        if( !(poNode->papoSubExpr[0]->field_type == SWQ_GEOMETRY ||
              poNode->papoSubExpr[0]->field_type == SWQ_STRING) )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Cannot cast %s to geometry",
                      SWQFieldTypeToString(poNode->papoSubExpr[0]->field_type) );
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
