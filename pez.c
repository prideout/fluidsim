// Pez was developed by Philip Rideout and released under the MIT License.

#include "pez.h"
#include "bstrlib.h"
#include <stdlib.h>
#include <stdio.h>

///////////////////////////////////////////////////////////////////////////////
// PRIVATE TYPES

typedef struct pezListRec
{
    bstring Key;
    bstring Value;
    struct pezListRec* Next;
} pezList;

typedef struct pezContextRec
{
    bstring ErrorMessage;
    bstring KeyPrefix;
    pezList* TokenMap;
    pezList* ShaderMap;
    pezList* LoadedEffects;
    pezList* PathList;
} pezContext;

///////////////////////////////////////////////////////////////////////////////
// PRIVATE GLOBALS

static pezContext* __pez__Context = 0;

///////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS

static int __pez__Alphanumeric(char c)
{
    return
        (c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') ||
        c == '_' || c == '.';
}

static void __pez__FreeList(pezList* pNode)
{
    while (pNode)
    {
        pezList* pNext = pNode->Next;
        bdestroy(pNode->Key);
        bdestroy(pNode->Value);
        free(pNode);
        pNode = pNext;
    }
}

static bstring __pez__LoadEffectContents(pezContext* gc, bstring effectName)
{
    FILE* fp = 0;
    bstring effectFile, effectContents;
    pezList* pPathList = gc->PathList;
    
    while (pPathList)
    {
        effectFile = bstrcpy(effectName);
        binsert(effectFile, 0, pPathList->Key, '?');
        bconcat(effectFile, pPathList->Value);

        fp = fopen((const char*) effectFile->data, "rb");
        if (fp)
        {
            break;
        }
            
        pPathList = pPathList->Next;
    }

    if (!fp)
    {
        bdestroy(gc->ErrorMessage);
        gc->ErrorMessage = bformat("Unable to open effect file '%s'.", effectFile->data);
        bdestroy(effectFile);
        return 0;
    }
    
    // Add a new entry to the front of gc->LoadedEffects
    {
        pezList* temp = gc->LoadedEffects;
        gc->LoadedEffects = (pezList*) calloc(sizeof(pezList), 1);
        gc->LoadedEffects->Key = bstrcpy(effectName);
        gc->LoadedEffects->Next = temp;
    }
    
    // Read in the effect file
    effectContents = bread((bNread) fread, fp);
    fclose(fp);
    bdestroy(effectFile);
    return effectContents;
}

///////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS

int pezSwInit(const char* keyPrefix)
{
    if (__pez__Context)
    {
        bdestroy(__pez__Context->ErrorMessage);
        __pez__Context->ErrorMessage = bfromcstr("Already initialized.");
        return 0;
    }

    __pez__Context = (pezContext*) calloc(sizeof(pezContext), 1);
    __pez__Context->KeyPrefix = bfromcstr(keyPrefix);
    
    pezSwAddPath("", "");

    return 1;
}

int pezSwShutdown()
{
    pezContext* gc = __pez__Context;

    if (!gc)
    {
        return 0;
    }

    bdestroy(gc->ErrorMessage);
    bdestroy(gc->KeyPrefix);

    __pez__FreeList(gc->TokenMap);
    __pez__FreeList(gc->ShaderMap);
    __pez__FreeList(gc->LoadedEffects);
    __pez__FreeList(gc->PathList);

    free(gc);
    __pez__Context = 0;

    return 1;
}

int pezSwAddPath(const char* pathPrefix, const char* pathSuffix)
{
    pezContext* gc = __pez__Context;
    pezList* temp;

    if (!gc)
    {
        return 0;
    }

    temp = gc->PathList;
    gc->PathList = (pezList*) calloc(sizeof(pezList), 1);
    gc->PathList->Key = bfromcstr(pathPrefix);
    gc->PathList->Value = bfromcstr(pathSuffix);
    gc->PathList->Next = temp;

    return 1;
}

const char* pezGetShader(const char* pEffectKey)
{
    pezContext* gc = __pez__Context;
    bstring effectKey;
    pezList* closestMatch = 0;
    struct bstrList* tokens;
    bstring effectName;
    pezList* pLoadedEffect;
    pezList* pShaderEntry;
    bstring shaderKey = 0;

    if (!gc)
    {
        return 0;
    }

    // Extract the effect name from the effect key
    effectKey = bfromcstr(pEffectKey);
    binsert(effectKey, 0, gc->KeyPrefix, '?');
    tokens = bsplit(effectKey, '.');
    if (!tokens || !tokens->qty)
    {
        bdestroy(gc->ErrorMessage);
        gc->ErrorMessage = bformat("Malformed effect key key '%s'.", pEffectKey);
        bstrListDestroy(tokens);
        bdestroy(effectKey);
        return 0;
    }
    effectName = tokens->entry[0];

    // Check if we already loaded this effect file
    pLoadedEffect = gc->LoadedEffects;
    while (pLoadedEffect)
    {
        if (1 == biseq(pLoadedEffect->Key, effectName))
        {
            break;
        }
        pLoadedEffect = pLoadedEffect->Next;
    }

    // If we haven't loaded this file yet, load it in
    if (!pLoadedEffect)
    {
        bstring effectContents = __pez__LoadEffectContents(gc, effectName);
        struct bstrList* lines = bsplit(effectContents, '\n');
        int lineNo;

        bdestroy(effectContents);
        effectContents = 0;

        for (lineNo = 0; lines && lineNo < lines->qty; lineNo++)
        {
            bstring line = lines->entry[lineNo];

            // If the line starts with "--", then it marks a new section
            if (blength(line) >= 2 && line->data[0] == '-' && line->data[1] == '-')
            {
                // Find the first character in [A-Za-z0-9_].
                int colNo;
                for (colNo = 2; colNo < blength(line); colNo++)
                {
                    char c = line->data[colNo];
                    if (__pez__Alphanumeric(c))
                    {
                        break;
                    }
                }

                // If there's no alphanumeric character,
                // then this marks the start of a new comment block.
                if (colNo >= blength(line))
                {
                    bdestroy(shaderKey);
                    shaderKey = 0;
                }
                else
                {
                    // Keep reading until a non-alphanumeric character is found.
                    int endCol;
                    for (endCol = colNo; endCol < blength(line); endCol++)
                    {
                        char c = line->data[endCol];
                        if (!__pez__Alphanumeric(c))
                        {
                            break;
                        }
                    }

                    bdestroy(shaderKey);
                    shaderKey = bmidstr(line, colNo, endCol - colNo);

                    // Add a new entry to the shader map.
                    {
                        pezList* temp = gc->ShaderMap;
                        gc->ShaderMap = (pezList*) calloc(sizeof(pezList), 1);
                        gc->ShaderMap->Key = bstrcpy(shaderKey);
                        gc->ShaderMap->Next = temp;
                        gc->ShaderMap->Value = bformat("#line %d\n", lineNo);

                        binsertch(gc->ShaderMap->Key, 0, 1, '.');
                        binsert(gc->ShaderMap->Key, 0, effectName, '?');
                    }

                    // Check for a version mapping.
                    if (gc->TokenMap)
                    {
                        struct bstrList* tokens = bsplit(shaderKey, '.');
                        pezList* pTokenMapping = gc->TokenMap;

                        while (pTokenMapping)
                        {
                            bstring directive = 0;
                            int tokenIndex;

                            // An empty key in the token mapping means "always prepend this directive".
                            // The effect name itself is also checked against the token mapping.
                            if (0 == blength(pTokenMapping->Key) ||
                                (1 == blength(pTokenMapping->Key) && '*' == bchar(pTokenMapping->Key, 0)) ||
                                1 == biseq(pTokenMapping->Key, effectName))
                            {
                                directive = pTokenMapping->Value;
                                binsert(gc->ShaderMap->Value, 0, directive, '?');
                            }

                            // Check all tokens in the current section divider for a mapped token.
                            for (tokenIndex = 0; tokenIndex < tokens->qty && !directive; tokenIndex++)
                            {
                                bstring token = tokens->entry[tokenIndex];
                                if (1 == biseq(pTokenMapping->Key, token))
                                {
                                    directive = pTokenMapping->Value;
                                    binsert(gc->ShaderMap->Value, 0, directive, '?');
                                }
                            }

                            pTokenMapping = pTokenMapping->Next;
                        }

                        bstrListDestroy(tokens);
                    }
                }

                continue;
            }
            if (shaderKey)
            {
                bconcat(gc->ShaderMap->Value, line);
                bconchar(gc->ShaderMap->Value, '\n');
            }
        }

        // Cleanup
        bstrListDestroy(lines);
        bdestroy(shaderKey);
    }

    // Find the longest matching shader key
    pShaderEntry = gc->ShaderMap;

    while (pShaderEntry)
    {
        if (binstr(effectKey, 0, pShaderEntry->Key) == 0 &&
            (!closestMatch || blength(pShaderEntry->Key) > blength(closestMatch->Key)))
        {
            closestMatch = pShaderEntry;
        }

        pShaderEntry = pShaderEntry->Next;
    }

    bstrListDestroy(tokens);
    bdestroy(effectKey);

    if (!closestMatch)
    {
        bdestroy(gc->ErrorMessage);
        gc->ErrorMessage = bformat("Could not find shader with key '%s'.", pEffectKey);
        return 0;
    }

    return (const char*) closestMatch->Value->data;
}

const char* pezSwGetError()
{
    pezContext* gc = __pez__Context;

    if (!gc)
    {
        return "The pez API has not been initialized.";
    }

    return (const char*) (gc->ErrorMessage ? gc->ErrorMessage->data : 0);
}

int pezSwAddDirective(const char* token, const char* directive)
{
    pezContext* gc = __pez__Context;
    pezList* temp;

    if (!gc)
    {
        return 0;
    }

    temp = gc->TokenMap;
    gc->TokenMap = (pezList*) calloc(sizeof(pezList), 1);
    gc->TokenMap->Key = bfromcstr(token);
    gc->TokenMap->Value = bfromcstr(directive);
    gc->TokenMap->Next = temp;

    bconchar(gc->TokenMap->Value, '\n');

    return 1;
}
/*
 * Copyright (c) 2009 Andrew Collette <andrew.collette at gmail.com>
 * http://lzfx.googlecode.com
 *
 * Implements an LZF-compatible compressor/decompressor based on the liblzf
 * codebase written by Marc Lehmann.  This code is released under the BSD
 * license.  License and original copyright statement follow.
 *
 * 
 * Copyright (c) 2000-2008 Marc Alexander Lehmann <schmorp@schmorp.de>
 * 
 * Redistribution and use in source and binary forms, with or without modifica-
 * tion, are permitted provided that the following conditions are met:
 * 
 *   1.  Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 * 
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MER-
 * CHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPE-
 * CIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTH-
 * ERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef LZFX_H
#define LZFX_H

#ifdef __cplusplus
extern "C" {
#endif

/*  Documented behavior, including function signatures and error codes,
    is guaranteed to remain unchanged for releases with the same major
    version number.  Releases of the same major version are also able
    to read each other's output, although the output itself is not
    guaranteed to be byte-for-byte identical.
*/
#define LZFX_VERSION_MAJOR      0
#define LZFX_VERSION_MINOR      1
#define LZFX_VERSION_STRING     "0.1"

/* Hashtable size (2**LZFX_HLOG entries) */
#ifndef LZFX_HLOG
# define LZFX_HLOG 16
#endif

/* Predefined errors. */
#define LZFX_ESIZE      -1      /* Output buffer too small */
#define LZFX_ECORRUPT   -2      /* Invalid data for decompression */
#define LZFX_EARGS      -3      /* Arguments invalid (NULL) */

/*  Buffer-to buffer compression.

    Supply pre-allocated input and output buffers via ibuf and obuf, and
    their size in bytes via ilen and olen.  Buffers may not overlap.

    On success, the function returns a non-negative value and the argument
    olen contains the compressed size in bytes.  On failure, a negative
    value is returned and olen is not modified.
*/
int lzfx_compress(const void* ibuf, unsigned int ilen,
                        void* obuf, unsigned int *olen);

/*  Buffer-to-buffer decompression.

    Supply pre-allocated input and output buffers via ibuf and obuf, and
    their size in bytes via ilen and olen.  Buffers may not overlap.

    On success, the function returns a non-negative value and the argument
    olen contains the uncompressed size in bytes.  On failure, a negative
    value is returned.

    If the failure code is LZFX_ESIZE, olen contains the minimum buffer size
    required to hold the decompressed data.  Otherwise, olen is not modified.

    Supplying a zero *olen is a valid and supported strategy to determine the
    required buffer size.  This does not require decompression of the entire
    stream and is consequently very fast.  Argument obuf may be NULL in
    this case only.
*/
int lzfx_decompress(const void* ibuf, unsigned int ilen,
                          void* obuf, unsigned int *olen);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
/*
 * Copyright (c) 2009 Andrew Collette <andrew.collette at gmail.com>
 * http://lzfx.googlecode.com
 *
 * Implements an LZF-compatible compressor/decompressor based on the liblzf
 * codebase written by Marc Lehmann.  This code is released under the BSD
 * license.  License and original copyright statement follow.
 *
 * 
 * Copyright (c) 2000-2008 Marc Alexander Lehmann <schmorp@schmorp.de>
 * 
 * Redistribution and use in source and binary forms, with or without modifica-
 * tion, are permitted provided that the following conditions are met:
 * 
 *   1.  Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 * 
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MER-
 * CHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPE-
 * CIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTH-
 * ERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define LZFX_HSIZE (1 << (LZFX_HLOG))

/* We need this for memset */
#ifdef __cplusplus
# include <cstring>
#else
# include <string.h>
#endif

#if __GNUC__ >= 3 && !DISABLE_EXPECT
# define fx_expect_false(expr)  __builtin_expect((expr) != 0, 0)
# define fx_expect_true(expr)   __builtin_expect((expr) != 0, 1)
#else
# define fx_expect_false(expr)  (expr)
# define fx_expect_true(expr)   (expr)
#endif

typedef unsigned char u8;
typedef const u8 *LZSTATE[LZFX_HSIZE];

/* Define the hash function */
#define LZFX_FRST(p)     (((p[0]) << 8) | p[1])
#define LZFX_NEXT(v,p)   (((v) << 8) | p[2])
#define LZFX_IDX(h)      ((( h >> (3*8 - LZFX_HLOG)) - h  ) & (LZFX_HSIZE - 1))

/* These cannot be changed, as they are related to the compressed format. */
#define LZFX_MAX_LIT        (1 <<  5)
#define LZFX_MAX_OFF        (1 << 13)
#define LZFX_MAX_REF        ((1 << 8) + (1 << 3))

static
int lzfx_getsize(const void* ibuf, unsigned int ilen, unsigned int *olen);

/* Compressed format

    There are two kinds of structures in LZF/LZFX: literal runs and back
    references. The length of a literal run is encoded as L - 1, as it must
    contain at least one byte.  Literals are encoded as follows:

    000LLLLL <L+1 bytes>

    Back references are encoded as follows.  The smallest possible encoded
    length value is 1, as otherwise the control byte would be recognized as
    a literal run.  Since at least three bytes must match for a back reference
    to be inserted, the length is encoded as L - 2 instead of L - 1.  The
    offset (distance to the desired data in the output buffer) is encoded as
    o - 1, as all offsets are at least 1.  The binary format is:

    LLLooooo oooooooo           for backrefs of real length < 9   (1 <= L < 7)
    111ooooo LLLLLLLL oooooooo  for backrefs of real length >= 9  (L > 7)  
*/
#include <stdio.h>
int lzfx_compress(const void *ibuf, unsigned int ilen,
                              void *obuf, unsigned int * olen){

    /* Hash table; an array of u8*'s which point
       to various locations in the input buffer */
    u8 *htab[LZFX_HSIZE];

    u8 **hslot;       /* Pointer to entry in hash table */
    unsigned int hval;      /* Hash value generated by macros above */
    const u8 *ref;          /* Pointer to candidate match location in input */

    u8 *ip = (u8 *)ibuf;
    const u8 *const in_end = ip + ilen;

    u8 *op = (u8 *)obuf;
    const u8 *const out_end = (olen == NULL ? NULL : op + *olen);

    int lit;    /* # of bytes in current literal run */

#if defined (WIN32) && defined (_M_X64)
    unsigned _int64 off; /* workaround for missing POSIX compliance */
#else
    unsigned long off;
#endif

    if(olen == NULL) return LZFX_EARGS;
    if(ibuf == NULL){
        if(ilen != 0) return LZFX_EARGS;
        *olen = 0;
        return 0;
    }
    if(obuf == NULL){
        if(olen != 0) return LZFX_EARGS;
        return lzfx_getsize(ibuf, ilen, olen);
    }

    memset(htab, 0, sizeof(htab));

    /*  Start a literal run.  Whenever we do this the output pointer is
        advanced because the current byte will hold the encoded length. */
    lit = 0; op++;

    hval = LZFX_FRST(ip);

    while(ip + 2 < in_end){   /* The NEXT macro reads 2 bytes ahead */

        hval = LZFX_NEXT(hval, ip);
        hslot = htab + LZFX_IDX(hval);

        ref = *hslot; *hslot = ip;

        if( ref < ip
        &&  (off = ip - ref - 1) < LZFX_MAX_OFF
        &&  ip + 4 < in_end  /* Backref takes up to 3 bytes, so don't bother */
        &&  ref > (u8 *)ibuf
        &&  ref[0] == ip[0]
        &&  ref[1] == ip[1]
        &&  ref[2] == ip[2] ) {

            unsigned int len = 3;   /* We already know 3 bytes match */
            const unsigned int maxlen = in_end - ip - 2 > LZFX_MAX_REF ?
                                        LZFX_MAX_REF : in_end - ip - 2;

            /* lit == 0:  op + 3 must be < out_end (because we undo the run)
               lit != 0:  op + 3 + 1 must be < out_end */
            if(fx_expect_false(op - !lit + 3 + 1 >= out_end))
                return LZFX_ESIZE;
            
            op [- lit - 1] = lit - 1; /* Terminate literal run */
            op -= !lit;               /* Undo run if length is zero */

            /*  Start checking at the fourth byte */
            while (len < maxlen && ref[len] == ip[len])
                len++;

            len -= 2;  /* We encode the length as #octets - 2 */

            /* Format 1: [LLLooooo oooooooo] */
            if (len < 7) {
              *op++ = (u8) ((off >> 8) + (len << 5));
              *op++ = (u8) off;

            /* Format 2: [111ooooo LLLLLLLL oooooooo] */
            } else {
              *op++ = (u8) ((off >> 8) + (7 << 5));
              *op++ = (u8) (len - 7);
              *op++ = (u8) off;
            }

            lit = 0; op++;

            ip += len + 1;  /* ip = initial ip + #octets -1 */

            if (fx_expect_false (ip + 3 >= in_end)){
                ip++;   /* Code following expects exit at bottom of loop */
                break;
            }

            hval = LZFX_FRST (ip);
            hval = LZFX_NEXT (hval, ip);
            htab[LZFX_IDX (hval)] = ip;

            ip++;   /* ip = initial ip + #octets */

        } else {
              /* Keep copying literal bytes */

              if (fx_expect_false (op >= out_end)) return LZFX_ESIZE;

              lit++; *op++ = *ip++;

              if (fx_expect_false (lit == LZFX_MAX_LIT)) {
                  op [- lit - 1] = lit - 1; /* stop run */
                  lit = 0; op++; /* start run */
              }

        } /* if() found match in htab */

    } /* while(ip < ilen -2) */

    /*  At most 3 bytes remain in input.  We therefore need 4 bytes available
        in the output buffer to store them (3 data + ctrl byte).*/
    if (op + 3 > out_end) return LZFX_ESIZE;

    while (ip < in_end) {

        lit++; *op++ = *ip++;

        if (fx_expect_false (lit == LZFX_MAX_LIT)){
            op [- lit - 1] = lit - 1;
            lit = 0; op++;
        }
    }

    op [- lit - 1] = lit - 1;
    op -= !lit;

    *olen = op - (u8 *)obuf;
    return 0;
}

/* Decompressor */
int lzfx_decompress(const void* ibuf, unsigned int ilen,
                          void* obuf, unsigned int *olen){

    u8 const *ip = (const u8 *)ibuf;
    u8 const *const in_end = ip + ilen;
    u8 *op = (u8 *)obuf;
    u8 const *const out_end = (olen == NULL ? NULL : op + *olen);
    
    unsigned int remain_len = 0;
    int rc;

    if(olen == NULL) return LZFX_EARGS;
    if(ibuf == NULL){
        if(ilen != 0) return LZFX_EARGS;
        *olen = 0;
        return 0;
    }
    if(obuf == NULL){
        if(*olen != 0) return LZFX_EARGS;
        return lzfx_getsize(ibuf, ilen, olen);
    }

    do {
        unsigned int ctrl = *ip++;

        /* Format 000LLLLL: a literal byte string follows, of length L+1 */
        if(ctrl < (1 << 5)) {

            ctrl++;

            if(fx_expect_false(op + ctrl > out_end)){
                --ip;       /* Rewind to control byte */
                goto guess;
            }
            if(fx_expect_false(ip + ctrl > in_end)) return LZFX_ECORRUPT;

            do
                *op++ = *ip++;
            while(--ctrl);

        /*  Format #1 [LLLooooo oooooooo]: backref of length L+1+2
                          ^^^^^ ^^^^^^^^
                            A      B
                   #2 [111ooooo LLLLLLLL oooooooo] backref of length L+7+2
                          ^^^^^          ^^^^^^^^
                            A               B
            In both cases the location of the backref is computed from the
            remaining part of the data as follows:

                location = op - A*256 - B - 1
        */
        } else {

            unsigned int len = (ctrl >> 5);
            u8 *ref = op - ((ctrl & 0x1f) << 8) -1;

            if(len==7) len += *ip++;    /* i.e. format #2 */

            len += 2;    /* len is now #octets */

            if(fx_expect_false(op + len > out_end)){
                ip -= (len >= 9) ? 2 : 1;   /* Rewind to control byte */
                goto guess;
            }
            if(fx_expect_false(ip >= in_end)) return LZFX_ECORRUPT;

            ref -= *ip++;

            if(fx_expect_false(ref < (u8*)obuf)) return LZFX_ECORRUPT;

            do
                *op++ = *ref++;
            while (--len);
        }

    } while (ip < in_end);

    *olen = op - (u8 *)obuf;

    return 0;

guess:
    rc = lzfx_getsize(ip, ilen - (ip-(u8*)ibuf), &remain_len);
    if(rc>=0) *olen = remain_len + (op - (u8*)obuf);
    return rc;
}

/* Guess len. No parameters may be NULL; this is not checked. */
static
int lzfx_getsize(const void* ibuf, unsigned int ilen, unsigned int *olen){

    u8 const *ip = (const u8 *)ibuf;
    u8 const *const in_end = ip + ilen;
    int tot_len = 0;
    
    while (ip < in_end) {

        unsigned int ctrl = *ip++;

        if(ctrl < (1 << 5)) {

            ctrl++;

            if(ip + ctrl > in_end)
                return LZFX_ECORRUPT;

            tot_len += ctrl;
            ip += ctrl;

        } else {

            unsigned int len = (ctrl >> 5);

            if(len==7){     /* i.e. format #2 */
                len += *ip++;
            }

            len += 2;    /* len is now #octets */

            if(ip >= in_end) return LZFX_ECORRUPT;

            ip++; /* skip the ref byte */

            tot_len += len;

        }

    }

    *olen = tot_len;

    return 0;
}





#include <stdio.h>

PezPixels pezLoadPixels(const char* filename)
{
    FILE* file = fopen(filename, "rb");
    unsigned int compressedSize, decompressedSize;
    unsigned char* compressed;
    unsigned int headerSize;
    PezPixels pixels;

    fseek(file, 0, SEEK_END);
    compressedSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    compressed = (unsigned char*) malloc(compressedSize);
    fread(compressed, 1, compressedSize, file);
    fclose(file);

    decompressedSize = 0;
    lzfx_decompress(compressed, compressedSize, 0, &decompressedSize);

    pixels.RawHeader = (void*) malloc(decompressedSize);
    lzfx_decompress(compressed, compressedSize, pixels.RawHeader, &decompressedSize);
    free(compressed);

    headerSize = sizeof(struct PezPixelsRec);
    memcpy(&pixels, pixels.RawHeader, headerSize - 2 * sizeof(void*));
    pixels.Frames = (char*) pixels.RawHeader + headerSize;

    return pixels;
}

void pezFreePixels(PezPixels pixels)
{
    free(pixels.RawHeader);
}

void pezFreeVerts(PezVerts verts)
{
    free(verts.RawHeader);
}

void pezSavePixels(PezPixels pixels, const char* filename)
{
    unsigned int headerSize = sizeof(struct PezPixelsRec);
    unsigned int contentSize = pixels.FrameCount * pixels.BytesPerFrame;
    unsigned int decompressedSize = headerSize + contentSize;
    unsigned char* decompressed;
    unsigned int compressedSize;
    unsigned char* compressed;
    FILE* file;
    
    decompressed = (unsigned char*) malloc(decompressedSize);
    memcpy(decompressed, &pixels, headerSize);
    memcpy(decompressed + headerSize, pixels.Frames, contentSize);

    compressedSize = decompressedSize;
    compressed = (unsigned char*) malloc(decompressedSize);
    lzfx_compress(decompressed, decompressedSize, compressed, &compressedSize);
    
    free(decompressed);

    file = fopen(filename, "wb");
    fwrite(compressed, 1, compressedSize, file);
    free(compressed);
    fclose(file);
}

PezVerts pezLoadVerts(const char* filename)
{
    FILE* file = fopen(filename, "rb");
    
    unsigned int compressedSize;
    fseek(file, 0, SEEK_END);
    compressedSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    unsigned char* compressed = (unsigned char*) malloc(compressedSize);
    fread(compressed, 1, compressedSize, file);
    fclose(file);

    unsigned int decompressedSize = 0;
    lzfx_decompress(compressed, compressedSize, 0, &decompressedSize);

    PezVerts verts;
    verts.RawHeader = (void*) malloc(decompressedSize);
    lzfx_decompress(compressed, compressedSize, verts.RawHeader, &decompressedSize);
    free(compressed);

    unsigned int headerSize = sizeof(struct PezVertsRec);
    memcpy(&verts, verts.RawHeader, headerSize - sizeof(void*));

    unsigned int attribTableSize = sizeof(struct PezAttribRec) * verts.AttribCount;
    unsigned int indexTableSize = verts.IndexBufferSize;
    
    verts.Attribs = (PezAttrib*) ((char*) verts.RawHeader + headerSize);
    verts.Indices = (GLvoid*) ((char*) verts.RawHeader + headerSize + attribTableSize);
    
    char* f = (char*) verts.RawHeader + headerSize + attribTableSize + indexTableSize;
    for (int attrib = 0; attrib < verts.AttribCount; attrib++) {
        verts.Attribs[attrib].Frames = (GLvoid*) f;
        f += verts.VertexCount * verts.Attribs[attrib].FrameCount * verts.Attribs[attrib].Stride;
    }

    const char* s = f;
    for (int attrib = 0; attrib < verts.AttribCount; attrib++) {
        verts.Attribs[attrib].Name = s;
        s += strlen(s) + 1;
    }

    return verts;
}

void pezSaveVerts(PezVerts verts, const char* filename)
{
    unsigned int headerSize = sizeof(struct PezVertsRec);
    unsigned int attribTableSize = sizeof(struct PezAttribRec) * verts.AttribCount;
    unsigned int indexTableSize = verts.IndexBufferSize;

    unsigned int stringTableSize = 0;
    unsigned int frameTableSize = 0;
    for (int attrib = 0; attrib < verts.AttribCount; attrib++) {
        stringTableSize += strlen(verts.Attribs[attrib].Name) + 1;
        frameTableSize += verts.VertexCount * verts.Attribs[attrib].FrameCount * verts.Attribs[attrib].Stride;
    }

    unsigned int decompressedSize = headerSize + attribTableSize + indexTableSize + frameTableSize + stringTableSize;

    unsigned char* decompressed = (unsigned char*) malloc(decompressedSize);
    memcpy(decompressed, &verts, headerSize);
    memcpy(decompressed + headerSize, verts.Attribs, attribTableSize);
    memcpy(decompressed + headerSize + attribTableSize, verts.Indices, indexTableSize);
    
    unsigned char* frameTable = decompressed + headerSize + attribTableSize + indexTableSize;
    for (int attrib = 0; attrib < verts.AttribCount; attrib++) {
        GLsizei frameSize = verts.VertexCount * verts.Attribs[attrib].FrameCount * verts.Attribs[attrib].Stride;
        memcpy(frameTable, verts.Attribs[attrib].Frames, frameSize);
        frameTable += frameSize;
    }

    char* stringTable = (char*) frameTable;
    for (int attrib = 0; attrib < verts.AttribCount; attrib++) {
        const char* s = verts.Attribs[attrib].Name;
        strcpy(stringTable, s);
        stringTable += strlen(s) + 1;
    }

    unsigned int compressedSize = decompressedSize;
    unsigned char* compressed = (unsigned char*) malloc(decompressedSize);
    lzfx_compress(decompressed, decompressedSize, compressed, &compressedSize);
    
    free(decompressed);

    FILE* file = fopen(filename, "wb");
    fwrite(compressed, 1, compressedSize, file);
    free(compressed);
    fclose(file);
}
