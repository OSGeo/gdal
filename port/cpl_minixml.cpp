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
 * $Log$
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
    TComment
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
        if( chNext != '>' )
        {
            psContext->eTokenType = TNone;
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Parse error at '/' on line %d, expected '>'.", 
                      psContext->nInputLine );
        }
        else
        {
            psContext->eTokenType = TSlashClose;
            psContext->bInElement = FALSE;
        }
    }
/* -------------------------------------------------------------------- */
/*      Handle the ?> token terminator.                                 */
/* -------------------------------------------------------------------- */
    else if( chNext == '?' && psContext->bInElement 
             && psContext->pszInput[psContext->nInputOffset] == '>' )
    {
        chNext = ReadChar( psContext );
        if( chNext != '>' )
        {
            psContext->eTokenType = TNone;
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Parse error at '?' on line %d, expected '>'.", 
                      psContext->nInputLine );
        }
        else
        {
            psContext->eTokenType = TQuestionClose;
            psContext->bInElement = FALSE;
        }
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
                              "Line %d: <%s> doesn't have matching <%s>.",
                              sContext.nInputLine,
                              sContext.pszToken, sContext.pszToken+1 );
                    break;
                }
                else
                {
                    if( ReadToken(&sContext) != TClose )
                    {
                        CPLError( CE_Failure, CPLE_AppDefined, 
                                  "Line %d: Missing close angle bracket after <%s.",
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
                          "Line %d: Didn't find expected '=' for attribute value.",
                          sContext.nInputLine );
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
                      "Parse error at line %d, unexpected token:%s\n", 
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
                  "starting with %s\n",
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
    if(strlen(psNode->pszValue) + *pnLength + 40 + nIndent > *pnMaxLength)
        _GrowBuffer( strlen(psNode->pszValue) + *pnLength + 40 + nIndent, 
                     ppszText, pnMaxLength );
    
/* -------------------------------------------------------------------- */
/*      Text is just directly emitted.                                  */
/* -------------------------------------------------------------------- */
    if( psNode->eType == CXT_Text )
    {
        CPLAssert( psNode->psChild == NULL );

        strcat( *ppszText + *pnLength, psNode->pszValue );
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
/*      Attributes require a little formatting.                         */
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
/*      Elements actually have to deal with general children, and       */
/*      various formatting issues.                                      */
/* -------------------------------------------------------------------- */
    else if( psNode->eType == CXT_Element )
    {
        CPLXMLNode      *psChild = psNode->psChild;
        char *pszIndent;
        
        pszIndent =  (char *) CPLCalloc(nIndent + 1,1);
        memset(pszIndent, ' ', nIndent );

        strcat( *ppszText + *pnLength, pszIndent );
        *pnLength += nIndent;
        sprintf( *ppszText + *pnLength, "<%s", psNode->pszValue );
        
        while( psChild != NULL && psChild->eType == CXT_Attribute )
        {
            CPLSerializeXMLNode( psChild, 0, ppszText, pnLength, 
                                 pnMaxLength );
            psChild = psChild->psNext;
        }
        
        if( psChild == NULL )
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

            while( psChild != NULL )
            {
                if( psChild->eType != CXT_Text && bJustText )
                {
                    bJustText = FALSE;
                    strcat( *ppszText + *pnLength, "\n" );
                }

                CPLSerializeXMLNode( psChild, nIndent + 2, ppszText, pnLength, 
                                     pnMaxLength );
                psChild = psChild->psNext;
            }

            if( strlen(psNode->pszValue)+*pnLength+40+nIndent > *pnMaxLength)
                _GrowBuffer( strlen(psNode->pszValue)+*pnLength+40+nIndent, 
                             ppszText, pnMaxLength );

            if( !bJustText )
                strcat( *ppszText + *pnLength, pszIndent );

            *pnLength += strlen(*ppszText + *pnLength);
            sprintf( *ppszText + *pnLength, "</%s>\n", psNode->pszValue );
        }

        CPLFree( pszIndent );
    }
}
                                
/************************************************************************/
/*                        CPLSerializeXMLTree()                         */
/************************************************************************/

char *CPLSerializeXMLTree( CPLXMLNode *psNode )

{
    unsigned int nMaxLength = 10000, nLength = 0;
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
/*      Add a node as a child of another.                               */
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

    for( psSib = psParent->psChild; 
         psSib->psNext != NULL; 
         psSib = psSib->psNext ) {}

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

