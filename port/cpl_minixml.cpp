/**********************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implementation of MiniXML Parser and handling.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2001, Frank Warmerdam
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************
 *
 * Security Audit 2003/03/28 warmerda:
 *   Completed security audit.  I believe that this module may be safely used 
 *   to parse, and serialize arbitrary documents provided by a potentially 
 *   hostile source.
 *
 * $Log$
 * Revision 1.21  2003/03/28 17:38:39  warmerda
 * Added NULL check in CPLParseXMLString().
 *
 * Revision 1.20  2003/03/28 05:05:18  warmerda
 * Completed security audit. Several bugs related to possible buffer
 * overruns correct, notably with regard to CPLError() calls.
 *
 * Revision 1.19  2003/03/27 18:12:41  warmerda
 * Added NULL pszNameSpace support in namespace stripper (all namespaces).
 * Added XML file read/write functions.
 *
 * Revision 1.18  2003/03/24 16:47:30  warmerda
 * Added CPLStripXMLNamespace().
 * CPLAddXMLChild() will now ensure that attributes are inserted before
 * non-attributes nodes.
 *
 * Revision 1.17  2003/02/14 18:44:29  warmerda
 * proper tokens may include a dash
 *
 * Revision 1.16  2002/11/16 20:42:40  warmerda
 * improved inline comments
 *
 * Revision 1.15  2002/11/16 20:38:34  warmerda
 * added support for literals like DOCTYPE
 *
 * Revision 1.14  2002/07/16 15:06:26  warmerda
 * ensure that attributes are serialized properly regardless of their order
 *
 * Revision 1.13  2002/07/09 20:25:25  warmerda
 * expand tabs
 *
 * Revision 1.12  2002/05/28 18:54:05  warmerda
 * added escaping/unescaping support
 *
 * Revision 1.11  2002/05/24 04:09:10  warmerda
 * added clone and SetXMLValue functions
 *
 * Revision 1.10  2002/04/01 16:08:21  warmerda
 * allow periods in tokens
 *
 * Revision 1.9  2002/03/07 22:19:20  warmerda
 * don't do operations within CPLAssert(), in UnreadChar()
 *
 * Revision 1.8  2002/03/05 14:26:57  warmerda
 * expanded tabs
 *
 * Revision 1.7  2002/01/23 20:45:05  warmerda
 * handle <?...?> and comment elements
 *
 * Revision 1.6  2002/01/22 18:54:48  warmerda
 * ensure text is property initialized when serializing
 *
 * Revision 1.5  2002/01/16 03:58:51  warmerda
 * support single quotes as well as double quotes
 *
 * Revision 1.4  2001/12/06 18:13:49  warmerda
 * added CPLAddXMLChild and CPLCreateElmentAndValue
 *
 * Revision 1.3  2001/11/16 21:20:16  warmerda
 * fixed typo
 *
 * Revision 1.2  2001/11/16 20:29:58  warmerda
 * fixed lost char in normal CString tokens
 *
 * Revision 1.1  2001/11/16 15:39:48  warmerda
 * New
 */

#include <ctype.h>
#include "cpl_minixml.h"
#include "cpl_error.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

typedef enum {
    TNone,
    TString, 
    TOpen, 
    TClose,
    TEqual,
    TToken,
    TSlashClose,
    TQuestionClose,
    TComment,
    TLiteral
} TokenType;

typedef struct {
    const char *pszInput;
    int        nInputOffset;
    int        nInputLine;

    int        bInElement;
    TokenType  eTokenType;
    char       *pszToken;
    int        nTokenMaxSize;
    int        nTokenSize;

    int        nStackMaxSize;
    int        nStackSize;
    CPLXMLNode **papsStack;

    CPLXMLNode *psFirstNode;
} ParseContext;

/************************************************************************/
/*                              ReadChar()                              */
/************************************************************************/

static char ReadChar( ParseContext *psContext )

{
    char        chReturn;

    chReturn = psContext->pszInput[psContext->nInputOffset++];

    if( chReturn == '\0' )
        psContext->nInputOffset--;
    else if( chReturn == 10 )
        psContext->nInputLine++;
    
    return chReturn;
}

/************************************************************************/
/*                             UnreadChar()                             */
/************************************************************************/

static void UnreadChar( ParseContext *psContext, char chToUnread )

{
    if( chToUnread == '\0' )
    {
        /* do nothing */
    }
    else
    {
        CPLAssert( chToUnread 
                   == psContext->pszInput[psContext->nInputOffset-1] );

        psContext->nInputOffset--;

        if( chToUnread == 10 )
            psContext->nInputLine--;
    }
}

/************************************************************************/
/*                             AddToToken()                             */
/************************************************************************/

static void AddToToken( ParseContext *psContext, char chNewChar )

{
    if( psContext->pszToken == NULL )
    {
        psContext->nTokenMaxSize = 10;
        psContext->pszToken = (char *) CPLMalloc(psContext->nTokenMaxSize);
    }
    else if( psContext->nTokenSize >= psContext->nTokenMaxSize - 2 )
    {
        psContext->nTokenMaxSize *= 2;
        psContext->pszToken = (char *) 
            CPLRealloc(psContext->pszToken,psContext->nTokenMaxSize);
    }

    psContext->pszToken[psContext->nTokenSize++] = chNewChar;
    psContext->pszToken[psContext->nTokenSize] = '\0';
}

/************************************************************************/
/*                             ReadToken()                              */
/************************************************************************/

static TokenType ReadToken( ParseContext *psContext )

{
    char        chNext;

    psContext->nTokenSize = 0;
    psContext->pszToken[0] = '\0';
    
    chNext = ReadChar( psContext );
    while( isspace(chNext) )
        chNext = ReadChar( psContext );

/* -------------------------------------------------------------------- */
/*      Handle comments.                                                */
/* -------------------------------------------------------------------- */
    if( chNext == '<' 
        && EQUALN(psContext->pszInput+psContext->nInputOffset,"!--",3) )
    {
        psContext->eTokenType = TComment;

        // Skip "!--" characters
        ReadChar(psContext);
        ReadChar(psContext);
        ReadChar(psContext);

        while( !EQUALN(psContext->pszInput+psContext->nInputOffset,"-->",3)
               && (chNext = ReadChar(psContext)) != '\0' )
            AddToToken( psContext, chNext );

        // Skip "-->" characters
        ReadChar(psContext);
        ReadChar(psContext);
        ReadChar(psContext);
    }
/* -------------------------------------------------------------------- */
/*      Handle DOCTYPE or other literals.                               */
/* -------------------------------------------------------------------- */
    else if( chNext == '<' 
          && EQUALN(psContext->pszInput+psContext->nInputOffset,"!DOCTYPE",8) )
    {
        int   bInQuotes = FALSE;
        psContext->eTokenType = TLiteral;
        
        AddToToken( psContext, '<' );
        do { 
            chNext = ReadChar(psContext);
            if( chNext == '\0' )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Parse error in DOCTYPE on or before line %d, reached end of file without '>'.", 
                          psContext->nInputLine );
                
                break;
            }

            if( chNext == '\"' )
                bInQuotes = !bInQuotes;

             if( chNext == '>' && !bInQuotes )
            {
                AddToToken( psContext, '>' );
                break;
            }

            AddToToken( psContext, chNext );
        } while( TRUE );
    }
/* -------------------------------------------------------------------- */
/*      Simple single tokens of interest.                               */
/* -------------------------------------------------------------------- */
    else if( chNext == '<' && !psContext->bInElement )
    {
        psContext->eTokenType = TOpen;
        psContext->bInElement = TRUE;
    }
    else if( chNext == '>' && psContext->bInElement )
    {
        psContext->eTokenType = TClose;
        psContext->bInElement = FALSE;
    }
    else if( chNext == '=' && psContext->bInElement )
    {
        psContext->eTokenType = TEqual;
    }
    else if( chNext == '\0' )
    {
        psContext->eTokenType = TNone;
    }
/* -------------------------------------------------------------------- */
/*      Handle the /> token terminator.                                 */
/* -------------------------------------------------------------------- */
    else if( chNext == '/' && psContext->bInElement 
             && psContext->pszInput[psContext->nInputOffset] == '>' )
    {
        chNext = ReadChar( psContext );
        CPLAssert( chNext == '>' );

        psContext->eTokenType = TSlashClose;
        psContext->bInElement = FALSE;
    }
/* -------------------------------------------------------------------- */
/*      Handle the ?> token terminator.                                 */
/* -------------------------------------------------------------------- */
    else if( chNext == '?' && psContext->bInElement 
             && psContext->pszInput[psContext->nInputOffset] == '>' )
    {
        chNext = ReadChar( psContext );
        
        CPLAssert( chNext == '>' );

        psContext->eTokenType = TQuestionClose;
        psContext->bInElement = FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Collect a quoted string.                                        */
/* -------------------------------------------------------------------- */
    else if( psContext->bInElement && chNext == '"' )
    {
        psContext->eTokenType = TString;

        while( (chNext = ReadChar(psContext)) != '"' 
               && chNext != '\0' )
            AddToToken( psContext, chNext );
        
        if( chNext != '"' )
        {
            psContext->eTokenType = TNone;
            CPLError( CE_Failure, CPLE_AppDefined, 
                  "Parse error on line %d, reached EOF before closing quote.", 
                      psContext->nInputLine );
        }

        /* Do we need to unescape it? */
        if( strchr(psContext->pszToken,'&') != NULL )
        {
            int  nLength;
            char *pszUnescaped = CPLUnescapeString( psContext->pszToken, 
                                                    &nLength, CPLES_XML );
            strcpy( psContext->pszToken, pszUnescaped );
            CPLFree( pszUnescaped );
            psContext->nTokenSize = strlen(psContext->pszToken );
        }
    }

    else if( psContext->bInElement && chNext == '\'' )
    {
        psContext->eTokenType = TString;

        while( (chNext = ReadChar(psContext)) != '\'' 
               && chNext != '\0' )
            AddToToken( psContext, chNext );
        
        if( chNext != '\'' )
        {
            psContext->eTokenType = TNone;
            CPLError( CE_Failure, CPLE_AppDefined, 
                  "Parse error on line %d, reached EOF before closing quote.", 
                      psContext->nInputLine );
        }

        /* Do we need to unescape it? */
        if( strchr(psContext->pszToken,'&') != NULL )
        {
            int  nLength;
            char *pszUnescaped = CPLUnescapeString( psContext->pszToken, 
                                                    &nLength, CPLES_XML );
            strcpy( psContext->pszToken, pszUnescaped );
            CPLFree( pszUnescaped );
            psContext->nTokenSize = strlen(psContext->pszToken );
        }
    }

/* -------------------------------------------------------------------- */
/*      Collect an unquoted string, terminated by a open angle          */
/*      bracket.                                                        */
/* -------------------------------------------------------------------- */
    else if( !psContext->bInElement )
    {
        psContext->eTokenType = TString;

        AddToToken( psContext, chNext );
        while( (chNext = ReadChar(psContext)) != '<' 
               && chNext != '\0' )
            AddToToken( psContext, chNext );
        UnreadChar( psContext, chNext );

        /* Do we need to unescape it? */
        if( strchr(psContext->pszToken,'&') != NULL )
        {
            int  nLength;
            char *pszUnescaped = CPLUnescapeString( psContext->pszToken, 
                                                    &nLength, CPLES_XML );
            strcpy( psContext->pszToken, pszUnescaped );
            CPLFree( pszUnescaped );
            psContext->nTokenSize = strlen(psContext->pszToken );
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Collect a regular token terminated by white space, or           */
/*      special character(s) like an equal sign.                        */
/* -------------------------------------------------------------------- */
    else
    {
        psContext->eTokenType = TToken;

        /* add the first character to the token regardless of what it is */
        AddToToken( psContext, chNext );

        for( chNext = ReadChar(psContext); 
             (chNext >= 'A' && chNext <= 'Z')
                 || (chNext >= 'a' && chNext <= 'z')
                 || chNext == '-'
                 || chNext == '_'
                 || chNext == '.'
                 || chNext == ':'
                 || (chNext >= '0' && chNext <= '9');
             chNext = ReadChar(psContext) ) 
        {
            AddToToken( psContext, chNext );
        }

        UnreadChar(psContext, chNext);
    }
    
    return psContext->eTokenType;
}
    
/************************************************************************/
/*                              PushNode()                              */
/************************************************************************/

static void PushNode( ParseContext *psContext, CPLXMLNode *psNode )

{
    if( psContext->nStackMaxSize <= psContext->nStackSize )
    {
        psContext->nStackMaxSize += 10;
        psContext->papsStack = (CPLXMLNode **)
            CPLRealloc(psContext->papsStack, 
                       sizeof(CPLXMLNode*) * psContext->nStackMaxSize);
    }

    psContext->papsStack[psContext->nStackSize++] = psNode;
}
    
/************************************************************************/
/*                             AttachNode()                             */
/*                                                                      */
/*      Attach the passed node as a child of the current node.          */
/*      Special handling exists for adding siblings to psFirst if       */
/*      there is nothing on the stack.                                  */
/************************************************************************/

static void AttachNode( ParseContext *psContext, CPLXMLNode *psNode )

{
    if( psContext->psFirstNode == NULL )
        psContext->psFirstNode = psNode;
    else if( psContext->nStackSize == 0 )
    {
        CPLXMLNode *psSibling;

        psSibling = psContext->psFirstNode;
        while( psSibling->psNext != NULL )
            psSibling = psSibling->psNext;
        psSibling->psNext = psNode;
    }
    else if( psContext->papsStack[psContext->nStackSize-1]->psChild == NULL )
    {
        psContext->papsStack[psContext->nStackSize-1]->psChild = psNode;
    }
    else
    {
        CPLXMLNode *psSibling;

        psSibling = psContext->papsStack[psContext->nStackSize-1]->psChild;
        while( psSibling->psNext != NULL )
            psSibling = psSibling->psNext;
        psSibling->psNext = psNode;
    }
}

/************************************************************************/
/*                         CPLParseXMLString()                          */
/************************************************************************/

CPLXMLNode *CPLParseXMLString( const char *pszString )

{
    ParseContext sContext;

    CPLErrorReset();

    if( pszString == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "CPLParseXMLString() called with NULL pointer." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Initialize parse context.                                       */
/* -------------------------------------------------------------------- */
    sContext.pszInput = pszString;
    sContext.nInputOffset = 0;
    sContext.nInputLine = 0;
    sContext.bInElement = FALSE;
    sContext.pszToken = NULL;
    sContext.nTokenMaxSize = 0;
    sContext.nTokenSize = 0;
    sContext.eTokenType = TNone;
    sContext.nStackMaxSize = 0;
    sContext.nStackSize = 0;
    sContext.papsStack = NULL;
    sContext.psFirstNode = NULL;

    /* ensure token is initialized */
    AddToToken( &sContext, ' ' );
    
/* ==================================================================== */
/*      Loop reading tokens.                                            */
/* ==================================================================== */
    while( ReadToken( &sContext ) != TNone )
    {
/* -------------------------------------------------------------------- */
/*      Create a new element.                                           */
/* -------------------------------------------------------------------- */
        if( sContext.eTokenType == TOpen )
        {
            CPLXMLNode *psElement;

            if( ReadToken(&sContext) != TToken )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Line %d: Didn't find element token after open angle bracket.",
                          sContext.nInputLine );
                break;
            }

            if( sContext.pszToken[0] != '/' )
            {
                psElement = CPLCreateXMLNode( NULL, CXT_Element,
                                              sContext.pszToken );
                AttachNode( &sContext, psElement );
                PushNode( &sContext, psElement );
            }
            else 
            {
                if( sContext.nStackSize == 0
                    || !EQUAL(sContext.pszToken+1,
                         sContext.papsStack[sContext.nStackSize-1]->pszValue) )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "Line %d: <%.500s> doesn't have matching <%.500s>.",
                              sContext.nInputLine,
                              sContext.pszToken, sContext.pszToken+1 );
                    break;
                }
                else
                {
                    if( ReadToken(&sContext) != TClose )
                    {
                        CPLError( CE_Failure, CPLE_AppDefined, 
                                  "Line %d: Missing close angle bracket after <%.500s.",
                                  sContext.nInputLine,
                                  sContext.pszToken );
                        break;
                    }

                    /* pop element off stack */
                    sContext.nStackSize--;
                }
            }
        }

/* -------------------------------------------------------------------- */
/*      Add an attribute to a token.                                    */
/* -------------------------------------------------------------------- */
        else if( sContext.eTokenType == TToken )
        {
            CPLXMLNode *psAttr;

            psAttr = CPLCreateXMLNode(NULL, CXT_Attribute, sContext.pszToken);
            AttachNode( &sContext, psAttr );
            
            if( ReadToken(&sContext) != TEqual )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Line %d: Didn't find expected '=' for value of attribute '%.500s'.",
                          sContext.nInputLine, psAttr->pszValue );
                break;
            }

            if( ReadToken(&sContext) != TString 
                && sContext.eTokenType != TToken )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Line %d: Didn't find expected attribute value.",
                          sContext.nInputLine );
                break;
            }

            CPLCreateXMLNode( psAttr, CXT_Text, sContext.pszToken );
        }

/* -------------------------------------------------------------------- */
/*      Close the start section of an element.                          */
/* -------------------------------------------------------------------- */
        else if( sContext.eTokenType == TClose )
        {
            if( sContext.nStackSize == 0 )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Line %d: Found unbalanced '>'.",
                          sContext.nInputLine );
                break;
            }
        }

/* -------------------------------------------------------------------- */
/*      Close the start section of an element, and pop it               */
/*      immediately.                                                    */
/* -------------------------------------------------------------------- */
        else if( sContext.eTokenType == TSlashClose )
        {
            if( sContext.nStackSize == 0 )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Line %d: Found unbalanced '/>'.",
                          sContext.nInputLine );
                break;
            }

            sContext.nStackSize--;
        }

/* -------------------------------------------------------------------- */
/*      Close the start section of a <?...?> element, and pop it        */
/*      immediately.                                                    */
/* -------------------------------------------------------------------- */
        else if( sContext.eTokenType == TQuestionClose )
        {
            if( sContext.nStackSize == 0 )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Line %d: Found unbalanced '?>'.",
                          sContext.nInputLine );
                break;
            }
            else if( sContext.papsStack[sContext.nStackSize-1]->pszValue[0] != '?' )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Line %d: Found '?>' without matching '<?'.",
                          sContext.nInputLine );
                break;
            }

            sContext.nStackSize--;
        }

/* -------------------------------------------------------------------- */
/*      Handle comments.  They are returned as a whole token with the     */
/*      prefix and postfix omitted.  No processing of white space       */
/*      will be done.                                                   */
/* -------------------------------------------------------------------- */
        else if( sContext.eTokenType == TComment )
        {
            CPLXMLNode *psValue;

            psValue = CPLCreateXMLNode(NULL, CXT_Comment, sContext.pszToken);
            AttachNode( &sContext, psValue );
        }

/* -------------------------------------------------------------------- */
/*      Handle literals.  They are returned without processing.         */
/* -------------------------------------------------------------------- */
        else if( sContext.eTokenType == TLiteral )
        {
            CPLXMLNode *psValue;

            psValue = CPLCreateXMLNode(NULL, CXT_Literal, sContext.pszToken);
            AttachNode( &sContext, psValue );
        }

/* -------------------------------------------------------------------- */
/*      Add a text value node as a child of the current element.        */
/* -------------------------------------------------------------------- */
        else if( sContext.eTokenType == TString && !sContext.bInElement )
        {
            CPLXMLNode *psValue;

            psValue = CPLCreateXMLNode(NULL, CXT_Text, sContext.pszToken);
            AttachNode( &sContext, psValue );
        }
/* -------------------------------------------------------------------- */
/*      Anything else is an error.                                      */
/* -------------------------------------------------------------------- */
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Parse error at line %d, unexpected token:%.500s\n", 
                      sContext.nInputLine, sContext.pszToken );
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      Did we pop all the way out of our stack?                        */
/* -------------------------------------------------------------------- */
    if( CPLGetLastErrorType() == CE_None && sContext.nStackSize != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Parse error at EOF, not all elements have been closed,\n"
                  "starting with %.500s\n", 
                  sContext.papsStack[sContext.nStackSize-1]->pszValue );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPLFree( sContext.pszToken );
    if( sContext.papsStack != NULL )
        CPLFree( sContext.papsStack );

    if( CPLGetLastErrorType() != CE_None )
    {
        CPLDestroyXMLNode( sContext.psFirstNode );
        sContext.psFirstNode = NULL;
    }

    return sContext.psFirstNode;
}

/************************************************************************/
/*                            _GrowBuffer()                             */
/************************************************************************/

static void _GrowBuffer( unsigned int nNeeded, 
                         char **ppszText, unsigned int *pnMaxLength )

{
    if( nNeeded+1 >= *pnMaxLength )
    {
        *pnMaxLength = MAX(*pnMaxLength * 2,nNeeded+1);
        *ppszText = (char *) CPLRealloc(*ppszText, *pnMaxLength);
    }
}

/************************************************************************/
/*                        CPLSerializeXMLNode()                         */
/************************************************************************/

static void
CPLSerializeXMLNode( CPLXMLNode *psNode, int nIndent, 
                     char **ppszText, unsigned int *pnLength, 
                     unsigned int *pnMaxLength )

{
    if( psNode == NULL )
        return;
    
/* -------------------------------------------------------------------- */
/*      Ensure the buffer is plenty large to hold this additional       */
/*      string.                                                         */
/* -------------------------------------------------------------------- */
    *pnLength += strlen(*ppszText + *pnLength);
    _GrowBuffer( strlen(psNode->pszValue) + *pnLength + 40 + nIndent, 
                 ppszText, pnMaxLength );
    
/* -------------------------------------------------------------------- */
/*      Text is just directly emitted.                                  */
/* -------------------------------------------------------------------- */
    if( psNode->eType == CXT_Text )
    {
        char *pszEscaped = CPLEscapeString( psNode->pszValue, -1, CPLES_XML );

        CPLAssert( psNode->psChild == NULL );

        /* Escaped text might be bigger than expected. */
        _GrowBuffer( strlen(pszEscaped) + *pnLength,
                     ppszText, pnMaxLength );
        strcat( *ppszText + *pnLength, pszEscaped );

        CPLFree( pszEscaped );
    }

/* -------------------------------------------------------------------- */
/*      Attributes require a little formatting.                         */
/* -------------------------------------------------------------------- */
    else if( psNode->eType == CXT_Attribute )
    {
        CPLAssert( psNode->psChild != NULL 
                   && psNode->psChild->eType == CXT_Text );

        sprintf( *ppszText + *pnLength, " %s=\"", psNode->pszValue );
        CPLSerializeXMLNode( psNode->psChild, 0, ppszText, 
                             pnLength, pnMaxLength );
        strcat( *ppszText + *pnLength, "\"" );
    }

/* -------------------------------------------------------------------- */
/*      Handle comment output.                                          */
/* -------------------------------------------------------------------- */
    else if( psNode->eType == CXT_Comment )
    {
        int     i;

        CPLAssert( psNode->psChild == NULL );

        for( i = 0; i < nIndent; i++ )
            (*ppszText)[(*pnLength)++] = ' ';

        sprintf( *ppszText + *pnLength, "<!--%s-->\n", 
                 psNode->pszValue );
    }

/* -------------------------------------------------------------------- */
/*      Handle literal output (like <!DOCTYPE...>)                      */
/* -------------------------------------------------------------------- */
    else if( psNode->eType == CXT_Literal )
    {
        int     i;

        CPLAssert( psNode->psChild == NULL );

        for( i = 0; i < nIndent; i++ )
            (*ppszText)[(*pnLength)++] = ' ';

        strcpy( *ppszText + *pnLength, psNode->pszValue );
        strcat( *ppszText + *pnLength, "\n" );
    }

/* -------------------------------------------------------------------- */
/*      Elements actually have to deal with general children, and       */
/*      various formatting issues.                                      */
/* -------------------------------------------------------------------- */
    else if( psNode->eType == CXT_Element )
    {
        int             bHasNonAttributeChildren = FALSE;
        CPLXMLNode      *psChild;
        
        memset( *ppszText + *pnLength, ' ', nIndent );
        *pnLength += nIndent;
        (*ppszText)[*pnLength] = '\0';

        sprintf( *ppszText + *pnLength, "<%s", psNode->pszValue );

        /* Serialize *all* the attribute children, regardless of order */
        for( psChild = psNode->psChild; 
             psChild != NULL; 
             psChild = psChild->psNext )
        {
            if( psChild->eType == CXT_Attribute )
                CPLSerializeXMLNode( psChild, 0, ppszText, pnLength, 
                                     pnMaxLength );
            else
                bHasNonAttributeChildren = TRUE;
        }
        
        if( !bHasNonAttributeChildren )
        {
            if( psNode->pszValue[0] == '?' )
                strcat( *ppszText + *pnLength, "?>\n" );
            else
                strcat( *ppszText + *pnLength, "/>\n" );
        }
        else
        {
            int         bJustText = TRUE;

            strcat( *ppszText + *pnLength, ">" );

            for( psChild = psNode->psChild; 
                 psChild != NULL; 
                 psChild = psChild->psNext )
            {
                if( psChild->eType == CXT_Attribute )
                    continue;

                if( psChild->eType != CXT_Text && bJustText )
                {
                    bJustText = FALSE;
                    strcat( *ppszText + *pnLength, "\n" );
                }

                CPLSerializeXMLNode( psChild, nIndent + 2, ppszText, pnLength, 
                                     pnMaxLength );
            }
        
            *pnLength += strlen(*ppszText + *pnLength);
            _GrowBuffer( strlen(psNode->pszValue) + *pnLength + 40 + nIndent, 
                         ppszText, pnMaxLength );

            if( !bJustText )
            {
                memset( *ppszText + *pnLength, ' ', nIndent );
                *pnLength += nIndent;
                (*ppszText)[*pnLength] = '\0';
            }

            *pnLength += strlen(*ppszText + *pnLength);
            sprintf( *ppszText + *pnLength, "</%s>\n", psNode->pszValue );
        }
    }
}
                                
/************************************************************************/
/*                        CPLSerializeXMLTree()                         */
/************************************************************************/

char *CPLSerializeXMLTree( CPLXMLNode *psNode )

{
    unsigned int nMaxLength = 100, nLength = 0;
    char *pszText = NULL;
    CPLXMLNode *psThis;

    pszText = (char *) CPLMalloc(nMaxLength);
    pszText[0] = '\0';

    for( psThis = psNode; psThis != NULL; psThis = psThis->psNext )
        CPLSerializeXMLNode( psThis, 0, &pszText, &nLength, &nMaxLength );

    return pszText;
}

/************************************************************************/
/*                          CPLCreateXMLNode()                          */
/************************************************************************/

CPLXMLNode *CPLCreateXMLNode( CPLXMLNode *poParent, CPLXMLNodeType eType, 
                              const char *pszText )

{
    CPLXMLNode  *psNode;

/* -------------------------------------------------------------------- */
/*      Create new node.                                                */
/* -------------------------------------------------------------------- */
    psNode = (CPLXMLNode *) CPLCalloc(sizeof(CPLXMLNode),1);
    
    psNode->eType = eType;
    psNode->pszValue = CPLStrdup( pszText );

/* -------------------------------------------------------------------- */
/*      Attach to parent, if provided.                                  */
/* -------------------------------------------------------------------- */
    if( poParent != NULL )
    {
        if( poParent->psChild == NULL )
            poParent->psChild = psNode;
        else
        {
            CPLXMLNode  *psLink = poParent->psChild;

            while( psLink->psNext != NULL )
                psLink = psLink->psNext;

            psLink->psNext = psNode;
        }
    }
    
    return psNode;
}

/************************************************************************/
/*                         CPLDestroyXMLNode()                          */
/************************************************************************/

void CPLDestroyXMLNode( CPLXMLNode *psNode )

{
    if( psNode->psChild != NULL )
        CPLDestroyXMLNode( psNode->psChild );
    
    if( psNode->psNext != NULL )
        CPLDestroyXMLNode( psNode->psNext );

    CPLFree( psNode->pszValue );
    CPLFree( psNode );
}

/************************************************************************/
/*                           CPLGetXMLNode()                            */
/************************************************************************/

CPLXMLNode *CPLGetXMLNode( CPLXMLNode *poRoot, const char *pszPath )

{
    char        **papszTokens;
    int         iToken = 0;

    papszTokens = CSLTokenizeStringComplex( pszPath, ".", FALSE, FALSE );

    while( papszTokens[iToken] != NULL && poRoot != NULL )
    {
        CPLXMLNode *psChild;

        for( psChild = poRoot->psChild; psChild != NULL; 
             psChild = psChild->psNext ) 
        {
            if( psChild->eType != CXT_Text 
                && EQUAL(papszTokens[iToken],psChild->pszValue) )
                break;
        }

        if( psChild == NULL )
        {
            poRoot = NULL;
            break;
        }

        poRoot = psChild;
        iToken++;
    }

    CSLDestroy( papszTokens );
    return poRoot;
}

/************************************************************************/
/*                           CPLGetXMLValue()                           */
/************************************************************************/

const char *CPLGetXMLValue( CPLXMLNode *poRoot, const char *pszPath, 
                            const char *pszDefault )

{
    CPLXMLNode  *psTarget;

    psTarget = CPLGetXMLNode( poRoot, pszPath );
    if( psTarget == NULL )
        return pszDefault;

    if( psTarget->eType == CXT_Attribute )
    {
        CPLAssert( psTarget->psChild != NULL 
                   && psTarget->psChild->eType == CXT_Text );

        return psTarget->psChild->pszValue;
    }

    if( psTarget->eType == CXT_Element 
        && psTarget->psChild != NULL 
        && psTarget->psChild->eType == CXT_Text
        && psTarget->psChild->psNext == NULL )
    {
        return psTarget->psChild->pszValue;
    }
        
    return pszDefault;
}

/************************************************************************/
/*                           CPLAddXMLChild()                           */
/*                                                                      */
/*      Add a node as a child of another.  Ensure that attributes       */
/*      are inserted ahead of element children, or Text data.           */
/************************************************************************/

void CPLAddXMLChild( CPLXMLNode *psParent, CPLXMLNode *psChild )

{
    CPLXMLNode *psSib;

    CPLAssert( psChild->psNext == NULL );
    psChild->psNext = NULL;

    if( psParent->psChild == NULL )
    {
        psParent->psChild = psChild;
        return;
    }

    // Insert at head of list if first child is not attribute.
    if( psChild->eType == CXT_Attribute 
        && psParent->psChild->eType != CXT_Attribute )
    {
        psChild->psNext = psParent->psChild;
        psParent->psChild = psChild;
        return;
    }

    // Search for end of list.
    for( psSib = psParent->psChild; 
         psSib->psNext != NULL; 
         psSib = psSib->psNext ) 
    {
        // Insert attributes if the next node is not an attribute.
        if( psChild->eType == CXT_Attribute 
            && psSib->psNext != NULL 
            && psSib->psNext->eType != CXT_Attribute )
        {
            psChild->psNext = psSib->psNext;
            psSib->psNext = psChild;
            return;
        }
    }

    psSib->psNext = psChild;
}

/************************************************************************/
/*                    CPLCreateXMLElementAndValue()                     */
/************************************************************************/

CPLXMLNode *CPLCreateXMLElementAndValue( CPLXMLNode *psParent, 
                                         const char *pszName, 
                                         const char *pszValue )

{
    return CPLCreateXMLNode( 
        CPLCreateXMLNode( psParent, CXT_Element, pszName ),
        CXT_Text, pszValue );
}

/************************************************************************/
/*                          CPLCloneXMLTree()                           */
/*                                                                      */
/*      Clone an XML Tree.  We use recursion to handle children, but    */
/*      we do siblings by looping.  This means we can handle very       */
/*      long lists of elements, but great depth may cause stack         */
/*      overflow problems on some systems.                              */
/************************************************************************/

CPLXMLNode *CPLCloneXMLTree( CPLXMLNode *psTree )

{
    CPLXMLNode *psPrevious = NULL;
    CPLXMLNode *psReturn = NULL;

    while( psTree != NULL )
    {
        CPLXMLNode *psCopy;

        psCopy = CPLCreateXMLNode( NULL, psTree->eType, psTree->pszValue );
        if( psReturn == NULL )
            psReturn = psCopy;
        if( psPrevious != NULL )
            psPrevious->psNext = psCopy;

        if( psTree->psChild != NULL )
            psCopy->psChild = CPLCloneXMLTree( psTree->psChild );

        psPrevious = psCopy;
        psTree = psTree->psNext;
    }

    return psReturn;
}

/************************************************************************/
/*                           CPLSetXMLValue()                           */
/*                                                                      */
/*      Set the text value of an XML element to the suggested           */
/*      value.  Intermediate element nodes are created if               */
/*      an existing component is missing.                               */
/************************************************************************/

int CPLSetXMLValue( CPLXMLNode *psRoot,  const char *pszPath,
                    const char *pszValue )

{
    char        **papszTokens;
    int         iToken = 0;

    papszTokens = CSLTokenizeStringComplex( pszPath, ".", FALSE, FALSE );

    while( papszTokens[iToken] != NULL && psRoot != NULL )
    {
        CPLXMLNode *psChild;
        int        bIsAttribute = FALSE;
        const char *pszName = papszTokens[iToken];

        if( pszName[0] == '#' )
        {
            bIsAttribute = TRUE;
            pszName++;
        }

        if( psRoot->eType != CXT_Element )
            return FALSE;

        for( psChild = psRoot->psChild; psChild != NULL; 
             psChild = psChild->psNext ) 
        {
            if( psChild->eType != CXT_Text 
                && EQUAL(pszName,psChild->pszValue) )
                break;
        }

        if( psChild == NULL )
        {
            if( bIsAttribute )
                psChild = CPLCreateXMLNode( psRoot, CXT_Attribute, pszName );
            else
                psChild = CPLCreateXMLNode( psRoot, CXT_Element, pszName );
        }

        psRoot = psChild;
        iToken++;
    }

    CSLDestroy( papszTokens );

/* -------------------------------------------------------------------- */
/*      Now set a value node under this node.                           */
/* -------------------------------------------------------------------- */
    if( psRoot->psChild == NULL )
        CPLCreateXMLNode( psRoot, CXT_Text, pszValue );
    else if( psRoot->psChild->eType != CXT_Text )
        return FALSE;
    else 
    {
        CPLFree( psRoot->psChild->pszValue );
        psRoot->psChild->pszValue = CPLStrdup( pszValue );
    }

    return TRUE;
}

/************************************************************************/
/*                        CPLStripXMLNamespace()                        */
/************************************************************************/

void CPLStripXMLNamespace( CPLXMLNode *psRoot, 
                           const char *pszNamespace, 
                           int bRecurse )

{
    if( psRoot == NULL )
        return;

    if( pszNamespace != NULL )
    {
        if( psRoot->eType == CXT_Element 
            && EQUALN(pszNamespace,psRoot->pszValue,strlen(pszNamespace)) 
            && psRoot->pszValue[strlen(pszNamespace)] == ':' )
        {
            char *pszNewValue = 
                CPLStrdup(psRoot->pszValue+strlen(pszNamespace)+1);
            
            CPLFree( psRoot->pszValue );
            psRoot->pszValue = pszNewValue;
        }
    }
    else
    {
        const char *pszCheck;

        for( pszCheck = psRoot->pszValue; *pszCheck != '\0'; pszCheck++ )
        {
            if( *pszCheck == ':' )
            {
                char *pszNewValue = CPLStrdup( pszCheck+1 );
            
                CPLFree( psRoot->pszValue );
                psRoot->pszValue = pszNewValue;
                break;
            }
        }
    }

    if( bRecurse )
    {
        if( psRoot->psChild != NULL )
            CPLStripXMLNamespace( psRoot->psChild, pszNamespace, 1 );
        if( psRoot->psNext != NULL )
            CPLStripXMLNamespace( psRoot->psNext, pszNamespace, 1 );
    }
}

/************************************************************************/
/*                          CPLParseXMLFile()                           */
/************************************************************************/

CPLXMLNode *CPLParseXMLFile( const char *pszFilename )

{
    FILE 	*fp;
    int nLen;
    char *pszDoc;
    CPLXMLNode *psTree;

/* -------------------------------------------------------------------- */
/*      Read the file.                                                  */
/* -------------------------------------------------------------------- */
    fp = VSIFOpen( pszFilename, "rb" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to open %.500s to read.", pszFilename );
        return NULL;
    }

    VSIFSeek( fp, 0, SEEK_END );
    nLen = VSIFTell( fp );
    VSIFSeek( fp, 0, SEEK_SET );
    
    pszDoc = (char *) VSIMalloc(nLen+1);
    if( pszDoc == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, 
                  "Out of memory allocating space for %d byte buffer in\n"
                  "CPLParseXMLFile(%.500s).", 
                  nLen+1, pszFilename );
        VSIFClose( fp );
        return NULL;
    }
    if( (int) VSIFRead( pszDoc, 1, nLen, fp ) < nLen )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "VSIFRead() result short of expected %d bytes from %.500s.", 
                  nLen, pszFilename );
        pszDoc[0] = '\0';
    }
    VSIFClose( fp );

    pszDoc[nLen] = '\0';

/* -------------------------------------------------------------------- */
/*      Parse it.                                                       */
/* -------------------------------------------------------------------- */
    psTree = CPLParseXMLString( pszDoc );
    CPLFree( pszDoc );

    return psTree;
}

/************************************************************************/
/*                     CPLSerializeXMLTreeToFile()                      */
/************************************************************************/

int CPLSerializeXMLTreeToFile( CPLXMLNode *psTree, const char *pszFilename )

{
    char *pszDoc;
    FILE *fp;
    int  nLength;

/* -------------------------------------------------------------------- */
/*      Serialize document.                                             */
/* -------------------------------------------------------------------- */
    pszDoc = CPLSerializeXMLTree( psTree );
    if( pszDoc == NULL )
        return FALSE;

    nLength = strlen(pszDoc);

/* -------------------------------------------------------------------- */
/*      Create file.                                                    */
/* -------------------------------------------------------------------- */
    fp = VSIFOpen( pszFilename, "wt" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to open %.500s to write.", pszFilename );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Write file.                                                     */
/* -------------------------------------------------------------------- */
    if( (int) VSIFWrite( pszDoc, 1, nLength, fp ) != nLength )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Failed to write whole XML document (%.500s).",
                  pszFilename );
        VSIFClose( fp );
        CPLFree( pszDoc );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    VSIFClose( fp );
    CPLFree( pszDoc );

    return TRUE;
}


