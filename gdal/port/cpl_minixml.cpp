/**********************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implementation of MiniXML Parser and handling.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2001, Frank Warmerdam
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
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
 * Independent Security Audit 2003/04/05 Andrey Kiselev:
 *   Completed audit of this module. Any documents may be parsed without
 *   buffer overflows and stack corruptions.
 *
 * Security Audit 2003/03/28 warmerda:
 *   Completed security audit.  I believe that this module may be safely used
 *   to parse, and serialize arbitrary documents provided by a potentially
 *   hostile source.
 *
 */

#include "cpl_minixml.h"

#include <cctype>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include <algorithm>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"

CPL_CVSID("$Id$")

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
} XMLTokenType;

typedef struct
{
    CPLXMLNode *psFirstNode;
    CPLXMLNode *psLastChild;
} StackContext;

typedef struct {
    const char *pszInput;
    int        nInputOffset;
    int        nInputLine;
    bool       bInElement;
    XMLTokenType  eTokenType;
    char       *pszToken;
    size_t     nTokenMaxSize;
    size_t     nTokenSize;

    int        nStackMaxSize;
    int        nStackSize;
    StackContext *papsStack;

    CPLXMLNode *psFirstNode;
    CPLXMLNode *psLastNode;
} ParseContext;

static CPLXMLNode *_CPLCreateXMLNode( CPLXMLNode *poParent,
                                      CPLXMLNodeType eType,
                                      const char *pszText );

/************************************************************************/
/*                              ReadChar()                              */
/************************************************************************/

static CPL_INLINE char ReadChar( ParseContext *psContext )

{
    const char chReturn = psContext->pszInput[psContext->nInputOffset++];

    if( chReturn == '\0' )
        psContext->nInputOffset--;
    else if( chReturn == 10 )
        psContext->nInputLine++;

    return chReturn;
}

/************************************************************************/
/*                             UnreadChar()                             */
/************************************************************************/

static CPL_INLINE void UnreadChar( ParseContext *psContext, char chToUnread )

{
    if( chToUnread == '\0' )
        return;

    CPLAssert( chToUnread
               == psContext->pszInput[psContext->nInputOffset-1] );

    psContext->nInputOffset--;

    if( chToUnread == 10 )
        psContext->nInputLine--;
}

/************************************************************************/
/*                           ReallocToken()                             */
/************************************************************************/

static bool ReallocToken( ParseContext *psContext )
{
    if( psContext->nTokenMaxSize > INT_MAX / 2 )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Out of memory allocating %d*2 bytes",
                 static_cast<int>(psContext->nTokenMaxSize));
        VSIFree(psContext->pszToken);
        psContext->pszToken = nullptr;
        return false;
    }

    psContext->nTokenMaxSize *= 2;
    char* pszToken = static_cast<char *>(
        VSIRealloc(psContext->pszToken, psContext->nTokenMaxSize));
    if( pszToken == nullptr )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Out of memory allocating %d bytes",
                 static_cast<int>(psContext->nTokenMaxSize));
        VSIFree(psContext->pszToken);
        psContext->pszToken = nullptr;
        return false;
    }
    psContext->pszToken = pszToken;
    return true;
}

/************************************************************************/
/*                             AddToToken()                             */
/************************************************************************/

static CPL_INLINE bool _AddToToken( ParseContext *psContext, char chNewChar )

{
    if( psContext->nTokenSize >= psContext->nTokenMaxSize - 2 )
    {
        if( !ReallocToken(psContext) )
            return false;
    }

    psContext->pszToken[psContext->nTokenSize++] = chNewChar;
    psContext->pszToken[psContext->nTokenSize] = '\0';
    return true;
}

// TODO(schwehr): Remove the goto.
#define AddToToken(psContext, chNewChar) \
    if( !_AddToToken(psContext, chNewChar)) goto fail;

/************************************************************************/
/*                             ReadToken()                              */
/************************************************************************/

static XMLTokenType ReadToken( ParseContext *psContext, CPLErr& eLastErrorType )

{
    psContext->nTokenSize = 0;
    psContext->pszToken[0] = '\0';

    char chNext = ReadChar( psContext );
    while( isspace(static_cast<unsigned char>(chNext)) )
        chNext = ReadChar( psContext );

/* -------------------------------------------------------------------- */
/*      Handle comments.                                                */
/* -------------------------------------------------------------------- */
    if( chNext == '<'
        && STARTS_WITH_CI(psContext->pszInput+psContext->nInputOffset, "!--") )
    {
        psContext->eTokenType = TComment;

        // Skip "!--" characters.
        ReadChar(psContext);
        ReadChar(psContext);
        ReadChar(psContext);

        while( !STARTS_WITH_CI(psContext->pszInput+psContext->nInputOffset,
                               "-->")
               && (chNext = ReadChar(psContext)) != '\0' )
            AddToToken( psContext, chNext );

        // Skip "-->" characters.
        ReadChar(psContext);
        ReadChar(psContext);
        ReadChar(psContext);
    }
/* -------------------------------------------------------------------- */
/*      Handle DOCTYPE.                                                 */
/* -------------------------------------------------------------------- */
    else if( chNext == '<' &&
             STARTS_WITH_CI(psContext->pszInput+psContext->nInputOffset,
                            "!DOCTYPE") )
    {
        bool bInQuotes = false;
        psContext->eTokenType = TLiteral;

        AddToToken( psContext, '<' );
        do {
            chNext = ReadChar(psContext);
            if( chNext == '\0' )
            {
                eLastErrorType = CE_Failure;
                CPLError( eLastErrorType, CPLE_AppDefined,
                          "Parse error in DOCTYPE on or before line %d, "
                          "reached end of file without '>'.",
                          psContext->nInputLine );

                break;
            }

            /* The markup declaration block within a DOCTYPE tag consists of:
             * - a left square bracket [
             * - a list of declarations
             * - a right square bracket ]
             * Example:
             * <!DOCTYPE RootElement [ ...declarations... ]>
             */
            if( chNext == '[' )
            {
                AddToToken( psContext, chNext );

                do
                {
                    chNext = ReadChar( psContext );
                    if( chNext == ']' )
                        break;
                    AddToToken( psContext, chNext );
                }
                while( chNext != '\0' &&
                       !STARTS_WITH_CI(
                           psContext->pszInput+psContext->nInputOffset, "]>") );

                if( chNext == '\0' )
                {
                    eLastErrorType = CE_Failure;
                    CPLError( eLastErrorType, CPLE_AppDefined,
                              "Parse error in DOCTYPE on or before line %d, "
                              "reached end of file without ']'.",
                          psContext->nInputLine );
                    break;
                }

                if( chNext != ']' )
                {
                    chNext = ReadChar( psContext );
                    AddToToken( psContext, chNext );

                    // Skip ">" character, will be consumed below.
                    chNext = ReadChar( psContext );
                }
            }

            if( chNext == '\"' )
                bInQuotes = !bInQuotes;

            if( chNext == '>' && !bInQuotes )
            {
                AddToToken( psContext, '>' );
                break;
            }

            AddToToken( psContext, chNext );
        } while( true );
    }
/* -------------------------------------------------------------------- */
/*      Handle CDATA.                                                   */
/* -------------------------------------------------------------------- */
    else if( chNext == '<' &&
             STARTS_WITH_CI(
                 psContext->pszInput+psContext->nInputOffset, "![CDATA[") )
    {
        psContext->eTokenType = TString;

        // Skip !CDATA[
        ReadChar( psContext );
        ReadChar( psContext );
        ReadChar( psContext );
        ReadChar( psContext );
        ReadChar( psContext );
        ReadChar( psContext );
        ReadChar( psContext );
        ReadChar( psContext );

        while( !STARTS_WITH_CI(psContext->pszInput+psContext->nInputOffset,
                               "]]>")
               && (chNext = ReadChar(psContext)) != '\0' )
            AddToToken( psContext, chNext );

        // Skip "]]>" characters.
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
        psContext->bInElement = true;
    }
    else if( chNext == '>' && psContext->bInElement )
    {
        psContext->eTokenType = TClose;
        psContext->bInElement = false;
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
        (void)chNext;
        CPLAssert( chNext == '>' );

        psContext->eTokenType = TSlashClose;
        psContext->bInElement = false;
    }
/* -------------------------------------------------------------------- */
/*      Handle the ?> token terminator.                                 */
/* -------------------------------------------------------------------- */
    else if( chNext == '?' && psContext->bInElement
             && psContext->pszInput[psContext->nInputOffset] == '>' )
    {
        chNext = ReadChar( psContext );
        (void)chNext;
        CPLAssert( chNext == '>' );

        psContext->eTokenType = TQuestionClose;
        psContext->bInElement = false;
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
            eLastErrorType = CE_Failure;
            CPLError( eLastErrorType, CPLE_AppDefined,
                "Parse error on line %d, reached EOF before closing quote.",
                psContext->nInputLine);
        }

        // Do we need to unescape it?
        if( strchr(psContext->pszToken, '&') != nullptr )
        {
            int nLength = 0;
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
            eLastErrorType = CE_Failure;
            CPLError( eLastErrorType, CPLE_AppDefined,
                "Parse error on line %d, reached EOF before closing quote.",
                psContext->nInputLine);
        }

        // Do we need to unescape it?
        if( strchr(psContext->pszToken, '&') != nullptr )
        {
            int nLength = 0;
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

        // Do we need to unescape it?
        if( strchr(psContext->pszToken, '&') != nullptr )
        {
            int nLength = 0;
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

        // Add the first character to the token regardless of what it is.
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

fail:
    psContext->eTokenType = TNone;
    return TNone;
}

/************************************************************************/
/*                              PushNode()                              */
/************************************************************************/

static bool PushNode( ParseContext *psContext, CPLXMLNode *psNode,
                      CPLErr& eLastErrorType )

{
    if( psContext->nStackMaxSize <= psContext->nStackSize )
    {
        // Somewhat arbitrary number.
        if( psContext->nStackMaxSize >= 10000 )
        {
            eLastErrorType = CE_Failure;
            CPLError(CE_Failure, CPLE_NotSupported,
                     "XML element depth beyond 10000. Giving up");
            VSIFree(psContext->papsStack);
            psContext->papsStack = nullptr;
            return false;
        }
        psContext->nStackMaxSize += 10;

        StackContext* papsStack = static_cast<StackContext *>(
            VSIRealloc(psContext->papsStack,
                       sizeof(StackContext) * psContext->nStackMaxSize));
        if( papsStack == nullptr )
        {
            eLastErrorType = CE_Failure;
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Out of memory allocating %d bytes",
                     static_cast<int>(sizeof(StackContext)) *
                     psContext->nStackMaxSize);
            VSIFree(psContext->papsStack);
            psContext->papsStack = nullptr;
            return false;
        }
        psContext->papsStack = papsStack;
    }
#ifdef DEBUG
    // To make Coverity happy, but cannot happen.
    if( psContext->papsStack == nullptr )
        return false;
#endif

    psContext->papsStack[psContext->nStackSize].psFirstNode = psNode;
    psContext->papsStack[psContext->nStackSize].psLastChild = nullptr;
    psContext->nStackSize++;

    return true;
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
    if( psContext->psFirstNode == nullptr )
    {
        psContext->psFirstNode = psNode;
        psContext->psLastNode = psNode;
    }
    else if( psContext->nStackSize == 0 )
    {
        psContext->psLastNode->psNext = psNode;
        psContext->psLastNode = psNode;
    }
    else
    {
        if( psContext->papsStack[psContext->nStackSize-1].psFirstNode->psChild
            == nullptr )
        {
            psContext->papsStack[psContext->nStackSize-1].psFirstNode->psChild =
                psNode;
        }
        else
        {
            psContext->papsStack[psContext->nStackSize-1].psLastChild->psNext =
                psNode;
        }
        psContext->papsStack[psContext->nStackSize-1].psLastChild = psNode;
    }
}

/************************************************************************/
/*                         CPLParseXMLString()                          */
/************************************************************************/

/**
 * \brief Parse an XML string into tree form.
 *
 * The passed document is parsed into a CPLXMLNode tree representation.
 * If the document is not well formed XML then NULL is returned, and errors
 * are reported via CPLError().  No validation beyond wellformedness is
 * done.  The CPLParseXMLFile() convenience function can be used to parse
 * from a file.
 *
 * The returned document tree is owned by the caller and should be freed
 * with CPLDestroyXMLNode() when no longer needed.
 *
 * If the document has more than one "root level" element then those after the
 * first will be attached to the first as siblings (via the psNext pointers)
 * even though there is no common parent.  A document with no XML structure
 * (no angle brackets for instance) would be considered well formed, and
 * returned as a single CXT_Text node.
 *
 * @param pszString the document to parse.
 *
 * @return parsed tree or NULL on error.
 */

CPLXMLNode *CPLParseXMLString( const char *pszString )

{
    if( pszString == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "CPLParseXMLString() called with NULL pointer." );
        return nullptr;
    }

    // Save back error context.
    const CPLErr eErrClass = CPLGetLastErrorType();
    const CPLErrorNum nErrNum = CPLGetLastErrorNo();
    const CPLString osErrMsg = CPLGetLastErrorMsg();

    // Reset it now.
    CPLErrorReset();

/* -------------------------------------------------------------------- */
/*      Check for a UTF-8 BOM and skip if found                         */
/*                                                                      */
/*      TODO: BOM is variable-length parameter and depends on encoding. */
/*            Add BOM detection for other encodings.                    */
/* -------------------------------------------------------------------- */

    // Used to skip to actual beginning of XML data.
    if( ( static_cast<unsigned char>(pszString[0]) == 0xEF )
        && ( static_cast<unsigned char>(pszString[1]) == 0xBB )
        && ( static_cast<unsigned char>(pszString[2]) == 0xBF) )
    {
        pszString += 3;
    }

/* -------------------------------------------------------------------- */
/*      Initialize parse context.                                       */
/* -------------------------------------------------------------------- */
    ParseContext sContext;
    sContext.pszInput = pszString;
    sContext.nInputOffset = 0;
    sContext.nInputLine = 0;
    sContext.bInElement = false;
    sContext.nTokenMaxSize = 10;
    sContext.pszToken = static_cast<char *>(VSIMalloc(sContext.nTokenMaxSize));
    if( sContext.pszToken == nullptr )
        return nullptr;
    sContext.nTokenSize = 0;
    sContext.eTokenType = TNone;
    sContext.nStackMaxSize = 0;
    sContext.nStackSize = 0;
    sContext.papsStack = nullptr;
    sContext.psFirstNode = nullptr;
    sContext.psLastNode = nullptr;

#ifdef DEBUG
    bool bRecoverableError = true;
#endif
    CPLErr eLastErrorType = CE_None;

/* ==================================================================== */
/*      Loop reading tokens.                                            */
/* ==================================================================== */
    while( ReadToken( &sContext, eLastErrorType ) != TNone )
    {
/* -------------------------------------------------------------------- */
/*      Create a new element.                                           */
/* -------------------------------------------------------------------- */
        if( sContext.eTokenType == TOpen )
        {
            if( ReadToken(&sContext, eLastErrorType) != TToken )
            {
                eLastErrorType = CE_Failure;
                CPLError( eLastErrorType, CPLE_AppDefined,
                          "Line %d: Didn't find element token after "
                          "open angle bracket.",
                          sContext.nInputLine );
                break;
            }

            CPLXMLNode *psElement = nullptr;
            if( sContext.pszToken[0] != '/' )
            {
                psElement = _CPLCreateXMLNode( nullptr, CXT_Element,
                                              sContext.pszToken );
                if( !psElement ) break;
                AttachNode( &sContext, psElement );
                if( !PushNode( &sContext, psElement, eLastErrorType ) )
                    break;
            }
            else
            {
                if( sContext.nStackSize == 0 ||
                    !EQUAL(sContext.pszToken+1,
                           sContext.papsStack[sContext.nStackSize-1].
                               psFirstNode->pszValue) )
                {
#ifdef DEBUG
                    // Makes life of fuzzers easier if we accept somewhat
                    // corrupted XML like <foo> ... </not_foo>.
                    if( CPLTestBool(CPLGetConfigOption("CPL_MINIXML_RELAXED",
                                                       "FALSE")) )
                    {
                        eLastErrorType = CE_Warning;
                        CPLError(
                            eLastErrorType, CPLE_AppDefined,
                            "Line %d: <%.500s> doesn't have matching <%.500s>.",
                            sContext.nInputLine,
                            sContext.pszToken, sContext.pszToken + 1 );
                        if( sContext.nStackSize == 0 )
                            break;
                        goto end_processing_close;
                    }
                    else
#endif
                    {
                        eLastErrorType = CE_Failure;
                        CPLError(
                            eLastErrorType, CPLE_AppDefined,
                            "Line %d: <%.500s> doesn't have matching <%.500s>.",
                            sContext.nInputLine,
                            sContext.pszToken, sContext.pszToken + 1 );
                        break;
                    }
                }
                else
                {
                    if( strcmp(sContext.pszToken + 1,
                               sContext.papsStack[sContext.nStackSize-1].
                                   psFirstNode->pszValue) != 0)
                    {
                        // TODO: At some point we could just error out like any
                        // other sane XML parser would do.
                        eLastErrorType = CE_Warning;
                        CPLError(
                            eLastErrorType, CPLE_AppDefined,
                            "Line %d: <%.500s> matches <%.500s>, but the case "
                            "isn't the same.  Going on, but this is invalid "
                            "XML that might be rejected in future versions.",
                            sContext.nInputLine,
                            sContext.papsStack[sContext.nStackSize-1].
                                psFirstNode->pszValue,
                            sContext.pszToken );
                    }
#ifdef DEBUG
end_processing_close:
#endif
                    if( ReadToken(&sContext, eLastErrorType) != TClose )
                    {
                        eLastErrorType = CE_Failure;
                        CPLError( eLastErrorType, CPLE_AppDefined,
                                  "Line %d: Missing close angle bracket "
                                  "after <%.500s.",
                                  sContext.nInputLine,
                                  sContext.pszToken );
                        break;
                    }

                    // Pop element off stack
                    sContext.nStackSize--;
                }
            }
        }

/* -------------------------------------------------------------------- */
/*      Add an attribute to a token.                                    */
/* -------------------------------------------------------------------- */
        else if( sContext.eTokenType == TToken )
        {
            CPLXMLNode *psAttr =
                _CPLCreateXMLNode(nullptr, CXT_Attribute, sContext.pszToken);
            if( !psAttr ) break;
            AttachNode( &sContext, psAttr );

            if( ReadToken(&sContext, eLastErrorType) != TEqual )
            {
                // Parse stuff like <?valbuddy_schematron
                // ../wmtsSimpleGetCapabilities.sch?>
                if( sContext.nStackSize > 0 &&
                      sContext.papsStack[sContext.nStackSize - 1]
                              .psFirstNode->pszValue[0] == '?' &&
                      sContext.papsStack[sContext.nStackSize - 1]
                              .psFirstNode->psChild == psAttr )
                {
                    CPLDestroyXMLNode(psAttr);
                    sContext.papsStack[sContext.nStackSize - 1]
                        .psFirstNode->psChild = nullptr;
                    sContext.papsStack[sContext.nStackSize - 1].psLastChild =
                        nullptr;

                    sContext.papsStack[sContext.nStackSize - 1]
                        .psFirstNode->pszValue = static_cast<char *>(CPLRealloc(
                        sContext.papsStack[sContext.nStackSize - 1]
                            .psFirstNode->pszValue,
                        strlen(sContext.papsStack[sContext.nStackSize - 1]
                                   .psFirstNode->pszValue) +
                            1 + strlen(sContext.pszToken) + 1));
                    strcat(sContext.papsStack[sContext.nStackSize - 1]
                               .psFirstNode->pszValue,
                           " ");
                    strcat(sContext.papsStack[sContext.nStackSize - 1]
                               .psFirstNode->pszValue,
                           sContext.pszToken);
                    continue;
                }

                eLastErrorType = CE_Failure;
                CPLError( eLastErrorType, CPLE_AppDefined,
                          "Line %d: Didn't find expected '=' for value of "
                          "attribute '%.500s'.",
                          sContext.nInputLine, psAttr->pszValue );
#ifdef DEBUG
                // Accepting an attribute without child text
                // would break too much assumptions in driver code
                bRecoverableError = false;
#endif
                break;
            }

            if( ReadToken(&sContext, eLastErrorType) == TToken )
            {
                /* TODO: at some point we could just error out like any other */
                /* sane XML parser would do */
                eLastErrorType = CE_Warning;
                CPLError( eLastErrorType, CPLE_AppDefined,
                          "Line %d: Attribute value should be single or double "
                          "quoted.  Going on, but this is invalid XML that "
                          "might be rejected in future versions.",
                          sContext.nInputLine );
            }
            else if( sContext.eTokenType != TString )
            {
                eLastErrorType = CE_Failure;
                CPLError( eLastErrorType, CPLE_AppDefined,
                          "Line %d: Didn't find expected attribute value.",
                          sContext.nInputLine );
#ifdef DEBUG
                // Accepting an attribute without child text
                // would break too much assumptions in driver code
                bRecoverableError = false;
#endif
                break;
            }

            if( !_CPLCreateXMLNode( psAttr, CXT_Text, sContext.pszToken ) )
                break;
        }

/* -------------------------------------------------------------------- */
/*      Close the start section of an element.                          */
/* -------------------------------------------------------------------- */
        else if( sContext.eTokenType == TClose )
        {
            if( sContext.nStackSize == 0 )
            {
                eLastErrorType = CE_Failure;
                CPLError( eLastErrorType, CPLE_AppDefined,
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
                eLastErrorType = CE_Failure;
                CPLError( eLastErrorType, CPLE_AppDefined,
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
                eLastErrorType = CE_Failure;
                CPLError( eLastErrorType, CPLE_AppDefined,
                          "Line %d: Found unbalanced '?>'.",
                          sContext.nInputLine );
                break;
            }
            else if( sContext.papsStack[sContext.nStackSize-1].
                         psFirstNode->pszValue[0] != '?' )
            {
                eLastErrorType = CE_Failure;
                CPLError( eLastErrorType, CPLE_AppDefined,
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
            CPLXMLNode *psValue =
                _CPLCreateXMLNode(nullptr, CXT_Comment, sContext.pszToken);
            if( !psValue ) break;
            AttachNode( &sContext, psValue );
        }
/* -------------------------------------------------------------------- */
/*      Handle literals.  They are returned without processing.         */
/* -------------------------------------------------------------------- */
        else if( sContext.eTokenType == TLiteral )
        {
            CPLXMLNode *psValue =
                _CPLCreateXMLNode(nullptr, CXT_Literal, sContext.pszToken);
            if( !psValue ) break;
            AttachNode( &sContext, psValue );
        }
/* -------------------------------------------------------------------- */
/*      Add a text value node as a child of the current element.        */
/* -------------------------------------------------------------------- */
        else if( sContext.eTokenType == TString && !sContext.bInElement )
        {
            CPLXMLNode *psValue =
                _CPLCreateXMLNode(nullptr, CXT_Text, sContext.pszToken);
            if( !psValue ) break;
            AttachNode( &sContext, psValue );
        }
/* -------------------------------------------------------------------- */
/*      Anything else is an error.                                      */
/* -------------------------------------------------------------------- */
        else
        {
            eLastErrorType = CE_Failure;
            CPLError( eLastErrorType, CPLE_AppDefined,
                      "Parse error at line %d, unexpected token:%.500s",
                      sContext.nInputLine, sContext.pszToken );
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      Did we pop all the way out of our stack?                        */
/* -------------------------------------------------------------------- */
    if( CPLGetLastErrorType() != CE_Failure && sContext.nStackSize > 0 &&
        sContext.papsStack != nullptr )
    {
#ifdef DEBUG
        // Makes life of fuzzers easier if we accept somewhat corrupted XML
        // like <x> ...
        if( bRecoverableError &&
            CPLTestBool(CPLGetConfigOption("CPL_MINIXML_RELAXED", "FALSE")) )
        {
            eLastErrorType = CE_Warning;
        }
        else
#endif
        {
            eLastErrorType = CE_Failure;
        }
        CPLError( eLastErrorType, CPLE_AppDefined,
                    "Parse error at EOF, not all elements have been closed, "
                    "starting with %.500s",
                    sContext.papsStack[sContext.nStackSize-1].
                        psFirstNode->pszValue );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPLFree( sContext.pszToken );
    if( sContext.papsStack != nullptr )
        CPLFree( sContext.papsStack );

    // We do not trust CPLGetLastErrorType() as if CPLTurnFailureIntoWarning()
    // has been set we would never get failures
    if( eLastErrorType == CE_Failure )
    {
        CPLDestroyXMLNode( sContext.psFirstNode );
        sContext.psFirstNode = nullptr;
        sContext.psLastNode = nullptr;
    }

    if( eLastErrorType == CE_None )
    {
        // Restore initial error state.
        CPLErrorSetState(eErrClass, nErrNum, osErrMsg);
    }

    return sContext.psFirstNode;
}

/************************************************************************/
/*                            _GrowBuffer()                             */
/************************************************************************/

static bool _GrowBuffer( size_t nNeeded,
                         char **ppszText, size_t *pnMaxLength )

{
    if( nNeeded+1 >= *pnMaxLength )
    {
        *pnMaxLength = std::max(*pnMaxLength * 2, nNeeded + 1);
        char* pszTextNew =
            static_cast<char *>(VSIRealloc(*ppszText, *pnMaxLength));
        if( pszTextNew == nullptr )
            return false;
        *ppszText = pszTextNew;
    }
    return true;
}

/************************************************************************/
/*                        CPLSerializeXMLNode()                         */
/************************************************************************/

// TODO(schwehr): Rewrite this whole thing using C++ string.
// CPLSerializeXMLNode has buffer overflows.
static bool
CPLSerializeXMLNode( const CPLXMLNode *psNode, int nIndent,
                     char **ppszText, size_t *pnLength,
                     size_t *pnMaxLength )

{
    if( psNode == nullptr )
        return true;

/* -------------------------------------------------------------------- */
/*      Ensure the buffer is plenty large to hold this additional       */
/*      string.                                                         */
/* -------------------------------------------------------------------- */
    *pnLength += strlen(*ppszText + *pnLength);
    if( !_GrowBuffer( strlen(psNode->pszValue) + *pnLength + 40 + nIndent,
                      ppszText, pnMaxLength ) )
        return false;

/* -------------------------------------------------------------------- */
/*      Text is just directly emitted.                                  */
/* -------------------------------------------------------------------- */
    if( psNode->eType == CXT_Text )
    {
        char *pszEscaped =
            CPLEscapeString( psNode->pszValue, -1, CPLES_XML_BUT_QUOTES );

        CPLAssert( psNode->psChild == nullptr );

        // Escaped text might be bigger than expected.
        if( !_GrowBuffer( strlen(pszEscaped) + *pnLength,
                          ppszText, pnMaxLength ) )
        {
            CPLFree( pszEscaped );
            return false;
        }
        strcat( *ppszText + *pnLength, pszEscaped );

        CPLFree( pszEscaped );
    }

/* -------------------------------------------------------------------- */
/*      Attributes require a little formatting.                         */
/* -------------------------------------------------------------------- */
    else if( psNode->eType == CXT_Attribute )
    {
        CPLAssert( psNode->psChild != nullptr
                   && psNode->psChild->eType == CXT_Text );

        snprintf( *ppszText + *pnLength, *pnMaxLength - *pnLength,
                  " %s=\"", psNode->pszValue );
        *pnLength += strlen(*ppszText + *pnLength);

        char *pszEscaped =
            CPLEscapeString( psNode->psChild->pszValue, -1, CPLES_XML );

        if( !_GrowBuffer( strlen(pszEscaped) + *pnLength,
                          ppszText, pnMaxLength ) )
        {
            CPLFree( pszEscaped );
            return false;
        }
        strcat( *ppszText + *pnLength, pszEscaped );

        CPLFree( pszEscaped );

        *pnLength += strlen(*ppszText + *pnLength);
        if( !_GrowBuffer( 3 + *pnLength, ppszText, pnMaxLength ) )
            return false;
        strcat( *ppszText + *pnLength, "\"" );
    }

/* -------------------------------------------------------------------- */
/*      Handle comment output.                                          */
/* -------------------------------------------------------------------- */
    else if( psNode->eType == CXT_Comment )
    {
        CPLAssert( psNode->psChild == nullptr );

        for( int i = 0; i < nIndent; i++ )
            (*ppszText)[(*pnLength)++] = ' ';

        snprintf( *ppszText + *pnLength, *pnMaxLength - *pnLength,
                  "<!--%s-->\n",
                  psNode->pszValue );
    }

/* -------------------------------------------------------------------- */
/*      Handle literal output (like <!DOCTYPE...>)                      */
/* -------------------------------------------------------------------- */
    else if( psNode->eType == CXT_Literal )
    {
        CPLAssert( psNode->psChild == nullptr );

        for( int i = 0; i < nIndent; i++ )
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
        bool bHasNonAttributeChildren = false;

        if( nIndent )
            memset( *ppszText + *pnLength, ' ', nIndent );
        *pnLength += nIndent;
        (*ppszText)[*pnLength] = '\0';

        snprintf( *ppszText + *pnLength, *pnMaxLength - *pnLength,
                  "<%s", psNode->pszValue );

        // Serialize *all* the attribute children, regardless of order
        CPLXMLNode *psChild = nullptr;
        for( psChild = psNode->psChild;
             psChild != nullptr;
             psChild = psChild->psNext )
        {
            if( psChild->eType == CXT_Attribute )
            {
                if( !CPLSerializeXMLNode( psChild, 0, ppszText, pnLength,
                                          pnMaxLength ) )
                    return false;
            }
            else
                bHasNonAttributeChildren = true;
        }

        if( !bHasNonAttributeChildren )
        {
            if( !_GrowBuffer( *pnLength + 40,
                              ppszText, pnMaxLength ) )
                return false;

            if( psNode->pszValue[0] == '?' )
                strcat( *ppszText + *pnLength, "?>\n" );
            else
                strcat( *ppszText + *pnLength, " />\n" );
        }
        else
        {
            bool bJustText = true;

            strcat( *ppszText + *pnLength, ">" );

            for( psChild = psNode->psChild;
                 psChild != nullptr;
                 psChild = psChild->psNext )
            {
                if( psChild->eType == CXT_Attribute )
                    continue;

                if( psChild->eType != CXT_Text && bJustText )
                {
                    bJustText = false;
                    *pnLength += strlen(*ppszText + *pnLength);
                    if( !_GrowBuffer( 1 + *pnLength, ppszText, pnMaxLength ) )
                        return false;
                    strcat( *ppszText + *pnLength, "\n" );
                }

                if( !CPLSerializeXMLNode( psChild, nIndent + 2,
                                          ppszText, pnLength,
                                          pnMaxLength ) )
                    return false;
            }

            *pnLength += strlen(*ppszText + *pnLength);
            if( !_GrowBuffer( strlen(psNode->pszValue) +
                              *pnLength + 40 + nIndent,
                              ppszText, pnMaxLength ) )
                return false;

            if( !bJustText )
            {
                if( nIndent )
                    memset( *ppszText + *pnLength, ' ', nIndent );
                *pnLength += nIndent;
                (*ppszText)[*pnLength] = '\0';
            }

            *pnLength += strlen(*ppszText + *pnLength);
            snprintf( *ppszText + *pnLength, *pnMaxLength - *pnLength,
                      "</%s>\n", psNode->pszValue );
        }
    }

    return true;
}

/************************************************************************/
/*                        CPLSerializeXMLTree()                         */
/************************************************************************/

/**
 * \brief Convert tree into string document.
 *
 * This function converts a CPLXMLNode tree representation of a document
 * into a flat string representation.  White space indentation is used
 * visually preserve the tree structure of the document.  The returned
 * document becomes owned by the caller and should be freed with CPLFree()
 * when no longer needed.
 *
 * @param psNode the node to serialize.
 *
 * @return the document on success or NULL on failure.
 */

char *CPLSerializeXMLTree( const CPLXMLNode *psNode )

{
    size_t nMaxLength = 100;
    char *pszText = static_cast<char *>(CPLCalloc(nMaxLength, sizeof(char)));
    if( pszText == nullptr )
        return nullptr;

    size_t nLength = 0;
    for( const CPLXMLNode *psThis = psNode;
         psThis != nullptr;
         psThis = psThis->psNext )
    {
        if( !CPLSerializeXMLNode( psThis, 0, &pszText, &nLength, &nMaxLength ) )
        {
            VSIFree(pszText);
            return nullptr;
        }
    }

    return pszText;
}

/************************************************************************/
/*                          CPLCreateXMLNode()                          */
/************************************************************************/

#ifdef DEBUG
static CPLXMLNode* psDummyStaticNode;
#endif

/**
 * \brief Create an document tree item.
 *
 * Create a single CPLXMLNode object with the desired value and type, and
 * attach it as a child of the indicated parent.
 *
 * @param poParent the parent to which this node should be attached as a
 * child.  May be NULL to keep as free standing.
 * @param eType the type of the newly created node
 * @param pszText the value of the newly created node
 *
 * @return the newly created node, now owned by the caller (or parent node).
 */

CPLXMLNode *CPLCreateXMLNode( CPLXMLNode *poParent, CPLXMLNodeType eType,
                              const char *pszText )

{
    auto ret = _CPLCreateXMLNode(poParent, eType, pszText);
    if( !ret )
    {
        CPLError(CE_Fatal, CPLE_OutOfMemory, "CPLCreateXMLNode() failed");
    }
    return ret;
}

/************************************************************************/
/*                         _CPLCreateXMLNode()                          */
/************************************************************************/

/* Same as CPLCreateXMLNode() but can return NULL in case of out-of-memory */
/* situation */

static CPLXMLNode *_CPLCreateXMLNode( CPLXMLNode *poParent,
                                      CPLXMLNodeType eType,
                                      const char *pszText )

{

/* -------------------------------------------------------------------- */
/*      Create new node.                                                */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psNode =
        static_cast<CPLXMLNode *>(VSICalloc(sizeof(CPLXMLNode), 1));
    if( psNode == nullptr )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate CPLXMLNode");
        return nullptr;
    }

    psNode->eType = eType;
    psNode->pszValue = VSIStrdup( pszText ? pszText : "" );
    if( psNode->pszValue == nullptr )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Cannot allocate psNode->pszValue");
        VSIFree(psNode);
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Attach to parent, if provided.                                  */
/* -------------------------------------------------------------------- */
    if( poParent != nullptr )
    {
        if( poParent->psChild == nullptr )
            poParent->psChild = psNode;
        else
        {
            CPLXMLNode *psLink = poParent->psChild;
            if( psLink->psNext == nullptr &&
                eType == CXT_Attribute &&
                psLink->eType == CXT_Text )
            {
                psNode->psNext = psLink;
                poParent->psChild = psNode;
            }
            else
            {
                while( psLink->psNext != nullptr )
                {
                    if( eType == CXT_Attribute &&
                        psLink->psNext->eType == CXT_Text )
                    {
                        psNode->psNext = psLink->psNext;
                        break;
                    }

                    psLink = psLink->psNext;
                }

                psLink->psNext = psNode;
            }
        }
    }
#ifdef DEBUG
    else
    {
        // Coverity sometimes doesn't realize that this function is passed
        // with a non NULL parent and thinks that this branch is taken, leading
        // to creating object being leak by caller. This ugly hack hopefully
        // makes it believe that someone will reference it.
        psDummyStaticNode = psNode;
    }
#endif

    return psNode;
}

/************************************************************************/
/*                         CPLDestroyXMLNode()                          */
/************************************************************************/

/**
 * \brief Destroy a tree.
 *
 * This function frees resources associated with a CPLXMLNode and all its
 * children nodes.
 *
 * @param psNode the tree to free.
 */

void CPLDestroyXMLNode( CPLXMLNode *psNode )

{
    while( psNode != nullptr )
    {
        if( psNode->pszValue != nullptr )
            CPLFree( psNode->pszValue );

        if( psNode->psChild != nullptr )
        {
            CPLXMLNode* psNext = psNode->psNext;
            psNode->psNext = psNode->psChild;
            // Move the child and its siblings as the next
            // siblings of the current node.
            if( psNext != nullptr )
            {
                CPLXMLNode* psIter = psNode->psChild;
                while( psIter->psNext != nullptr )
                    psIter = psIter->psNext;
                psIter->psNext = psNext;
            }
        }

        CPLXMLNode* psNext = psNode->psNext;

        CPLFree( psNode );

        psNode = psNext;
    }
}

/************************************************************************/
/*                           CPLSearchXMLNode()                         */
/************************************************************************/

/**
 * \brief Search for a node in document.
 *
 * Searches the children (and potentially siblings) of the documented
 * passed in for the named element or attribute.  To search following
 * siblings as well as children, prefix the pszElement name with an equal
 * sign.  This function does an in-order traversal of the document tree.
 * So it will first match against the current node, then its first child,
 * that child's first child, and so on.
 *
 * Use CPLGetXMLNode() to find a specific child, or along a specific
 * node path.
 *
 * @param psRoot the subtree to search.  This should be a node of type
 * CXT_Element.  NULL is safe.
 *
 * @param pszElement the name of the element or attribute to search for.
 *
 * @return The matching node or NULL on failure.
 */

CPLXMLNode *CPLSearchXMLNode( CPLXMLNode *psRoot, const char *pszElement )

{
    if( psRoot == nullptr || pszElement == nullptr )
        return nullptr;

    bool bSideSearch = false;

    if( *pszElement == '=' )
    {
        bSideSearch = true;
        pszElement++;
    }

/* -------------------------------------------------------------------- */
/*      Does this node match?                                           */
/* -------------------------------------------------------------------- */
    if( (psRoot->eType == CXT_Element
         || psRoot->eType == CXT_Attribute)
        && EQUAL(pszElement, psRoot->pszValue) )
        return psRoot;

/* -------------------------------------------------------------------- */
/*      Search children.                                                */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psChild = nullptr;
    for( psChild = psRoot->psChild; psChild != nullptr; psChild = psChild->psNext)
    {
        if( (psChild->eType == CXT_Element
             || psChild->eType == CXT_Attribute)
            && EQUAL(pszElement, psChild->pszValue) )
            return psChild;

        if( psChild->psChild != nullptr )
        {
            CPLXMLNode *psResult = CPLSearchXMLNode( psChild, pszElement );
            if( psResult != nullptr )
                return psResult;
        }
    }

/* -------------------------------------------------------------------- */
/*      Search siblings if we are in side search mode.                  */
/* -------------------------------------------------------------------- */
    if( bSideSearch )
    {
        for( psRoot = psRoot->psNext; psRoot != nullptr; psRoot = psRoot->psNext )
        {
            CPLXMLNode *psResult = CPLSearchXMLNode( psRoot, pszElement );
            if( psResult != nullptr )
                return psResult;
        }
    }

    return nullptr;
}

/************************************************************************/
/*                           CPLGetXMLNode()                            */
/************************************************************************/

/**
 * \brief Find node by path.
 *
 * Searches the document or subdocument indicated by psRoot for an element
 * (or attribute) with the given path.  The path should consist of a set of
 * element names separated by dots, not including the name of the root
 * element (psRoot).  If the requested element is not found NULL is returned.
 *
 * Attribute names may only appear as the last item in the path.
 *
 * The search is done from the root nodes children, but all intermediate
 * nodes in the path must be specified.  Searching for "name" would only find
 * a name element or attribute if it is a direct child of the root, not at any
 * level in the subdocument.
 *
 * If the pszPath is prefixed by "=" then the search will begin with the
 * root node, and its siblings, instead of the root nodes children.  This
 * is particularly useful when searching within a whole document which is
 * often prefixed by one or more "junk" nodes like the <?xml> declaration.
 *
 * @param psRoot the subtree in which to search.  This should be a node of
 * type CXT_Element.  NULL is safe.
 *
 * @param pszPath the list of element names in the path (dot separated).
 *
 * @return the requested element node, or NULL if not found.
 */

CPLXMLNode *CPLGetXMLNode( CPLXMLNode *psRoot, const char *pszPath )

{
    if( psRoot == nullptr || pszPath == nullptr )
        return nullptr;

    bool bSideSearch = false;

    if( *pszPath == '=' )
    {
        bSideSearch = true;
        pszPath++;
    }

    const char * const apszTokens[2] = { pszPath, nullptr };

    // Slight optimization: avoid using CSLTokenizeStringComplex that
    // does memory allocations when it is not really necessary.
    char **papszTokensToFree = nullptr;
    const char* const* papszTokens;
    if( strchr(pszPath, '.') )
    {
        papszTokensToFree = CSLTokenizeStringComplex( pszPath, ".", FALSE, FALSE );
        papszTokens = papszTokensToFree;
    }
    else
    {
        papszTokens = apszTokens;
    }

    int iToken = 0;
    while( papszTokens[iToken] != nullptr && psRoot != nullptr )
    {
        CPLXMLNode *psChild = nullptr;

        if( bSideSearch )
        {
            psChild = psRoot;
            bSideSearch = false;
        }
        else
            psChild = psRoot->psChild;

        for( ; psChild != nullptr; psChild = psChild->psNext )
        {
            if( psChild->eType != CXT_Text
                && EQUAL(papszTokens[iToken], psChild->pszValue) )
                break;
        }

        if( psChild == nullptr )
        {
            psRoot = nullptr;
            break;
        }

        psRoot = psChild;
        iToken++;
    }

    if( papszTokensToFree )
        CSLDestroy( papszTokensToFree );
    return psRoot;
}

/************************************************************************/
/*                           CPLGetXMLValue()                           */
/************************************************************************/

/**
 * \brief Fetch element/attribute value.
 *
 * Searches the document for the element/attribute value associated with
 * the path.  The corresponding node is internally found with CPLGetXMLNode()
 * (see there for details on path handling).  Once found, the value is
 * considered to be the first CXT_Text child of the node.
 *
 * If the attribute/element search fails, or if the found node has no
 * value then the passed default value is returned.
 *
 * The returned value points to memory within the document tree, and should
 * not be altered or freed.
 *
 * @param psRoot the subtree in which to search.  This should be a node of
 * type CXT_Element.  NULL is safe.
 *
 * @param pszPath the list of element names in the path (dot separated).  An
 * empty path means get the value of the psRoot node.
 *
 * @param pszDefault the value to return if a corresponding value is not
 * found, may be NULL.
 *
 * @return the requested value or pszDefault if not found.
 */

const char *CPLGetXMLValue( const CPLXMLNode *psRoot, const char *pszPath,
                            const char *pszDefault )

{
    const CPLXMLNode *psTarget = nullptr;

    if( pszPath == nullptr || *pszPath == '\0' )
        psTarget = psRoot;
    else
        psTarget = CPLGetXMLNode( psRoot, pszPath );

    if( psTarget == nullptr )
        return pszDefault;

    if( psTarget->eType == CXT_Attribute )
    {
        CPLAssert( psTarget->psChild != nullptr
                   && psTarget->psChild->eType == CXT_Text );

        return psTarget->psChild->pszValue;
    }

    if( psTarget->eType == CXT_Element )
    {
        // Find first non-attribute child, and verify it is a single text
        // with no siblings.

        psTarget = psTarget->psChild;

        while( psTarget != nullptr && psTarget->eType == CXT_Attribute )
            psTarget = psTarget->psNext;

        if( psTarget != nullptr
            && psTarget->eType == CXT_Text
            && psTarget->psNext == nullptr )
            return psTarget->pszValue;
    }

    return pszDefault;
}

/************************************************************************/
/*                           CPLAddXMLChild()                           */
/************************************************************************/

/**
 * \brief Add child node to parent.
 *
 * The passed child is added to the list of children of the indicated
 * parent.  Normally the child is added at the end of the parents child
 * list, but attributes (CXT_Attribute) will be inserted after any other
 * attributes but before any other element type.  Ownership of the child
 * node is effectively assumed by the parent node.   If the child has
 * siblings (its psNext is not NULL) they will be trimmed, but if the child
 * has children they are carried with it.
 *
 * @param psParent the node to attach the child to.  May not be NULL.
 *
 * @param psChild the child to add to the parent.  May not be NULL.  Should
 * not be a child of any other parent.
 */

void CPLAddXMLChild( CPLXMLNode *psParent, CPLXMLNode *psChild )

{
    if( psParent->psChild == nullptr )
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
    CPLXMLNode *psSib = nullptr;
    for( psSib = psParent->psChild;
         psSib->psNext != nullptr;
         psSib = psSib->psNext )
    {
        // Insert attributes if the next node is not an attribute.
        if( psChild->eType == CXT_Attribute
            && psSib->psNext != nullptr
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
/*                        CPLRemoveXMLChild()                           */
/************************************************************************/

/**
 * \brief Remove child node from parent.
 *
 * The passed child is removed from the child list of the passed parent,
 * but the child is not destroyed.  The child retains ownership of its
 * own children, but is cleanly removed from the child list of the parent.
 *
 * @param psParent the node to the child is attached to.
 *
 * @param psChild the child to remove.
 *
 * @return TRUE on success or FALSE if the child was not found.
 */

int CPLRemoveXMLChild( CPLXMLNode *psParent, CPLXMLNode *psChild )

{
    if( psParent == nullptr )
        return FALSE;

    CPLXMLNode *psLast = nullptr;
    CPLXMLNode *psThis = nullptr;
    for( psThis = psParent->psChild;
         psThis != nullptr;
         psThis = psThis->psNext )
    {
        if( psThis == psChild )
        {
            if( psLast == nullptr )
                psParent->psChild = psThis->psNext;
            else
                psLast->psNext = psThis->psNext;

            psThis->psNext = nullptr;
            return TRUE;
        }
        psLast = psThis;
    }

    return FALSE;
}

/************************************************************************/
/*                          CPLAddXMLSibling()                          */
/************************************************************************/

/**
 * \brief Add new sibling.
 *
 * The passed psNewSibling is added to the end of siblings of the
 * psOlderSibling node.  That is, it is added to the end of the psNext
 * chain.  There is no special handling if psNewSibling is an attribute.
 * If this is required, use CPLAddXMLChild().
 *
 * @param psOlderSibling the node to attach the sibling after.
 *
 * @param psNewSibling the node to add at the end of psOlderSiblings psNext
 * chain.
 */

void CPLAddXMLSibling( CPLXMLNode *psOlderSibling, CPLXMLNode *psNewSibling )

{
    if( psOlderSibling == nullptr )
        return;

    while( psOlderSibling->psNext != nullptr )
        psOlderSibling = psOlderSibling->psNext;

    psOlderSibling->psNext = psNewSibling;
}

/************************************************************************/
/*                    CPLCreateXMLElementAndValue()                     */
/************************************************************************/

/**
 * \brief Create an element and text value.
 *
 * This is function is a convenient short form for:
 *
 * \code
 *     CPLXMLNode *psTextNode;
 *     CPLXMLNode *psElementNode;
 *
 *     psElementNode = CPLCreateXMLNode( psParent, CXT_Element, pszName );
 *     psTextNode = CPLCreateXMLNode( psElementNode, CXT_Text, pszValue );
 *
 *     return psElementNode;
 * \endcode
 *
 * It creates a CXT_Element node, with a CXT_Text child, and
 * attaches the element to the passed parent.
 *
 * @param psParent the parent node to which the resulting node should
 * be attached.  May be NULL to keep as freestanding.
 *
 * @param pszName the element name to create.
 * @param pszValue the text to attach to the element. Must not be NULL.
 *
 * @return the pointer to the new element node.
 */

CPLXMLNode *CPLCreateXMLElementAndValue( CPLXMLNode *psParent,
                                         const char *pszName,
                                         const char *pszValue )

{
    CPLXMLNode *psElementNode =
        CPLCreateXMLNode( psParent, CXT_Element, pszName );
    CPLCreateXMLNode( psElementNode, CXT_Text, pszValue );

    return psElementNode;
}

/************************************************************************/
/*                    CPLCreateXMLElementAndValue()                     */
/************************************************************************/

/**
 * \brief Create an attribute and text value.
 *
 * This is function is a convenient short form for:
 *
 * \code
 *   CPLXMLNode *psAttributeNode;
 *
 *   psAttributeNode = CPLCreateXMLNode( psParent, CXT_Attribute, pszName );
 *   CPLCreateXMLNode( psAttributeNode, CXT_Text, pszValue );
 * \endcode
 *
 * It creates a CXT_Attribute node, with a CXT_Text child, and
 * attaches the element to the passed parent.
 *
 * @param psParent the parent node to which the resulting node should
 * be attached.  Must not be NULL.
 * @param pszName the attribute name to create.
 * @param pszValue the text to attach to the attribute. Must not be NULL.
 *
 * @since GDAL 2.0
 */

void CPLAddXMLAttributeAndValue( CPLXMLNode *psParent,
                                 const char *pszName,
                                 const char *pszValue )
{
    CPLAssert(psParent != nullptr);
    CPLXMLNode *psAttributeNode =
        CPLCreateXMLNode( psParent, CXT_Attribute, pszName );
    CPLCreateXMLNode( psAttributeNode, CXT_Text, pszValue );
}

/************************************************************************/
/*                          CPLCloneXMLTree()                           */
/************************************************************************/

/**
 * \brief Copy tree.
 *
 * Creates a deep copy of a CPLXMLNode tree.
 *
 * @param psTree the tree to duplicate.
 *
 * @return a copy of the whole tree.
 */

CPLXMLNode *CPLCloneXMLTree( const CPLXMLNode *psTree )

{
    CPLXMLNode *psPrevious = nullptr;
    CPLXMLNode *psReturn = nullptr;

    while( psTree != nullptr )
    {
        CPLXMLNode *psCopy =
            CPLCreateXMLNode( nullptr, psTree->eType, psTree->pszValue );
        if( psReturn == nullptr )
            psReturn = psCopy;
        if( psPrevious != nullptr )
            psPrevious->psNext = psCopy;

        if( psTree->psChild != nullptr )
            psCopy->psChild = CPLCloneXMLTree( psTree->psChild );

        psPrevious = psCopy;
        psTree = psTree->psNext;
    }

    return psReturn;
}

/************************************************************************/
/*                           CPLSetXMLValue()                           */
/************************************************************************/

/**
 * \brief Set element value by path.
 *
 * Find (or create) the target element or attribute specified in the
 * path, and assign it the indicated value.
 *
 * Any path elements that do not already exist will be created.  The target
 * nodes value (the first CXT_Text child) will be replaced with the provided
 * value.
 *
 * If the target node is an attribute instead of an element, the name
 * should be prefixed with a #.
 *
 * Example:
 *   CPLSetXMLValue( "Citation.Id.Description", "DOQ dataset" );
 *   CPLSetXMLValue( "Citation.Id.Description.#name", "doq" );
 *
 * @param psRoot the subdocument to be updated.
 *
 * @param pszPath the dot separated path to the target element/attribute.
 *
 * @param pszValue the text value to assign.
 *
 * @return TRUE on success.
 */

int CPLSetXMLValue( CPLXMLNode *psRoot, const char *pszPath,
                    const char *pszValue )

{
    char **papszTokens = CSLTokenizeStringComplex( pszPath, ".", FALSE, FALSE );
    int iToken = 0;

    while( papszTokens[iToken] != nullptr )
    {
        bool bIsAttribute = false;
        const char *pszName = papszTokens[iToken];

        if( pszName[0] == '#' )
        {
            bIsAttribute = true;
            pszName++;
        }

        if( psRoot->eType != CXT_Element )
        {
            CSLDestroy( papszTokens );
            return FALSE;
        }

        CPLXMLNode *psChild = nullptr;
        for( psChild = psRoot->psChild; psChild != nullptr;
             psChild = psChild->psNext )
        {
            if( psChild->eType != CXT_Text
                && EQUAL(pszName, psChild->pszValue) )
                break;
        }

        if( psChild == nullptr )
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
/*      Find the "text" child if there is one.                          */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psTextChild = psRoot->psChild;

    while( psTextChild != nullptr && psTextChild->eType != CXT_Text )
        psTextChild = psTextChild->psNext;

/* -------------------------------------------------------------------- */
/*      Now set a value node under this node.                           */
/* -------------------------------------------------------------------- */

    if( psTextChild == nullptr )
        CPLCreateXMLNode( psRoot, CXT_Text, pszValue );
    else
    {
        CPLFree( psTextChild->pszValue );
        psTextChild->pszValue = CPLStrdup( pszValue );
    }

    return TRUE;
}

/************************************************************************/
/*                        CPLStripXMLNamespace()                        */
/************************************************************************/

/**
 * \brief Strip indicated namespaces.
 *
 * The subdocument (psRoot) is recursively examined, and any elements
 * with the indicated namespace prefix will have the namespace prefix
 * stripped from the element names.  If the passed namespace is NULL, then
 * all namespace prefixes will be stripped.
 *
 * Nodes other than elements should remain unaffected.  The changes are
 * made "in place", and should not alter any node locations, only the
 * pszValue field of affected nodes.
 *
 * @param psRoot the document to operate on.
 * @param pszNamespace the name space prefix (not including colon), or NULL.
 * @param bRecurse TRUE to recurse over whole document, or FALSE to only
 * operate on the passed node.
 */

void CPLStripXMLNamespace( CPLXMLNode *psRoot,
                           const char *pszNamespace,
                           int bRecurse )

{
    size_t nNameSpaceLen = (pszNamespace) ? strlen(pszNamespace) : 0;

    while( psRoot != nullptr )
    {
        if( psRoot->eType == CXT_Element || psRoot->eType == CXT_Attribute )
        {
            if( pszNamespace != nullptr )
            {
                if( EQUALN(pszNamespace, psRoot->pszValue, nNameSpaceLen)
                    && psRoot->pszValue[nNameSpaceLen] == ':' )
                {
                    memmove(psRoot->pszValue, psRoot->pszValue+nNameSpaceLen+1,
                           strlen(psRoot->pszValue+nNameSpaceLen+1) + 1);
                }
            }
            else
            {
                for( const char *pszCheck = psRoot->pszValue;
                     *pszCheck != '\0';
                     pszCheck++ )
                {
                    if( *pszCheck == ':' )
                    {
                        memmove(psRoot->pszValue,
                                pszCheck + 1,
                                strlen(pszCheck + 1) + 1);
                        break;
                    }
                }
            }
        }

        if( bRecurse )
        {
            if( psRoot->psChild != nullptr )
                CPLStripXMLNamespace( psRoot->psChild, pszNamespace, 1 );

            psRoot = psRoot->psNext;
        }
        else
        {
            break;
        }
    }
}

/************************************************************************/
/*                          CPLParseXMLFile()                           */
/************************************************************************/

/**
 * \brief Parse XML file into tree.
 *
 * The named file is opened, loaded into memory as a big string, and
 * parsed with CPLParseXMLString().  Errors in reading the file or parsing
 * the XML will be reported by CPLError().
 *
 * The "large file" API is used, so XML files can come from virtualized
 * files.
 *
 * @param pszFilename the file to open.
 *
 * @return NULL on failure, or the document tree on success.
 */

CPLXMLNode *CPLParseXMLFile( const char *pszFilename )

{
/* -------------------------------------------------------------------- */
/*      Ingest the file.                                                */
/* -------------------------------------------------------------------- */
    GByte *pabyOut = nullptr;
    if( !VSIIngestFile( nullptr, pszFilename, &pabyOut, nullptr, -1 ) )
        return nullptr;

    char *pszDoc = reinterpret_cast<char *>(pabyOut);

/* -------------------------------------------------------------------- */
/*      Parse it.                                                       */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psTree = CPLParseXMLString( pszDoc );
    CPLFree( pszDoc );

    return psTree;
}

/************************************************************************/
/*                     CPLSerializeXMLTreeToFile()                      */
/************************************************************************/

/**
 * \brief Write document tree to a file.
 *
 * The passed document tree is converted into one big string (with
 * CPLSerializeXMLTree()) and then written to the named file.  Errors writing
 * the file will be reported by CPLError().  The source document tree is
 * not altered.  If the output file already exists it will be overwritten.
 *
 * @param psTree the document tree to write.
 * @param pszFilename the name of the file to write to.
 * @return TRUE on success, FALSE otherwise.
 */

int CPLSerializeXMLTreeToFile( const CPLXMLNode *psTree,
                               const char *pszFilename )

{
/* -------------------------------------------------------------------- */
/*      Serialize document.                                             */
/* -------------------------------------------------------------------- */
    char *pszDoc = CPLSerializeXMLTree( psTree );
    if( pszDoc == nullptr )
        return FALSE;

    const vsi_l_offset nLength = strlen(pszDoc);

/* -------------------------------------------------------------------- */
/*      Create file.                                                    */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL( pszFilename, "wt" );
    if( fp == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to open %.500s to write.", pszFilename );
        CPLFree( pszDoc );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Write file.                                                     */
/* -------------------------------------------------------------------- */
    if( VSIFWriteL(pszDoc, 1, static_cast<size_t>(nLength), fp ) != nLength )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to write whole XML document (%.500s).",
                  pszFilename );
        CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));
        CPLFree( pszDoc );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    const bool bRet = VSIFCloseL( fp ) == 0;
    if( !bRet )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to write whole XML document (%.500s).",
                  pszFilename );
    }
    CPLFree( pszDoc );

    return bRet;
}

/************************************************************************/
/*                       CPLCleanXMLElementName()                       */
/************************************************************************/

/**
 * \brief Make string into safe XML token.
 *
 * Modifies a string in place to try and make it into a legal
 * XML token that can be used as an element name.   This is accomplished
 * by changing any characters not legal in a token into an underscore.
 *
 * NOTE: This function should implement the rules in section 2.3 of
 * http://www.w3.org/TR/xml11/ but it doesn't yet do that properly.  We
 * only do a rough approximation of that.
 *
 * @param pszTarget the string to be adjusted.  It is altered in place.
 */

void CPLCleanXMLElementName( char *pszTarget )
{
    if( pszTarget == nullptr )
        return;

    for( ; *pszTarget != '\0'; pszTarget++ )
    {
        if( (*(reinterpret_cast<unsigned char *>(pszTarget)) & 0x80) || isalnum( *pszTarget )
            || *pszTarget == '_' || *pszTarget == '.' )
        {
            // Ok.
        }
        else
        {
            *pszTarget = '_';
        }
    }
}

/************************************************************************/
/*            CPLXMLTreeCloser::getDocumentElement()                    */
/************************************************************************/

CPLXMLNode* CPLXMLTreeCloser::getDocumentElement()
{
    CPLXMLNode *doc = get();
    // skip the Declaration and assume the next is the root element
    while (doc != nullptr
           && (doc->eType != CXT_Element
               || doc->pszValue[0] == '?') )
    {
        doc = doc->psNext;
    }
    return doc;
}
