/*-
 * Copyright 2005-2016 Colin Percival
 * Copyright 2016-2018 Alexander Peslyak
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "sysendian.h"
#include "sha256.h"

#ifdef __ICC
/* Miscompile with icc 14.0.0 (at least), so don't use restrict there */
#define restrict
#define static_restrict static
#elif defined(_MSC_VER)
#define restrict
#define static_restrict
#elif __STDC_VERSION__ >= 199901L
/* Have restrict */
#define static_restrict static restrict
#elif defined(__GNUC__)
#define restrict __restrict
#define static_restrict static __restrict
#else
#define restrict
#define static_restrict
#endif

/*
 * Encode a length len*2 vector of (uint32_t) into a length len*8 vector of
 * (uint8_t) in big-endian form.
 */
static void
be32enc_vect(uint8_t * dst, const uint32_t * src, size_t len)
{

    /* Encode vector, two words at a time. */
    do {
        be32enc(&dst[0], src[0]);
        be32enc(&dst[4], src[1]);
        src += 2;
        dst += 8;
    } while (--len);
}

/*
 * Decode a big-endian length len*8 vector of (uint8_t) into a length
 * len*2 vector of (uint32_t).
 */
static void
be32dec_vect(uint32_t * dst, const uint8_t * src, size_t len)
{

    /* Decode vector, two words at a time. */
    do {
        dst[0] = be32dec(&src[0]);
        dst[1] = be32dec(&src[4]);
        src += 8;
        dst += 2;
    } while (--len);
}

/* SHA256 round constants. */
static const uint32_t Krnd[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

/* Elementary functions used by SHA256 */
#define Ch(x, y, z)	((x & (y ^ z)) ^ z)
#define Maj(x, y, z)	((x & (y | z)) | (y & z))
#define SHR(x, n)	(x >> n)
#define ROTR(x, n)	((x >> n) | (x << (32 - n)))
#define S0(x)		(ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define S1(x)		(ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define s0(x)		(ROTR(x, 7) ^ ROTR(x, 18) ^ SHR(x, 3))
#define s1(x)		(ROTR(x, 17) ^ ROTR(x, 19) ^ SHR(x, 10))

/* SHA256 round function */
#define RND(a, b, c, d, e, f, g, h, k)			\
	h += S1(e) + Ch(e, f, g) + k;			\
	d += h;						\
	h += S0(a) + Maj(a, b, c);

/* Adjusted round function for rotating state */
#define RNDr(S, W, i, ii)			\
	RND(S[(64 - i) % 8], S[(65 - i) % 8],	\
	    S[(66 - i) % 8], S[(67 - i) % 8],	\
	    S[(68 - i) % 8], S[(69 - i) % 8],	\
	    S[(70 - i) % 8], S[(71 - i) % 8],	\
	    W[i + ii] + Krnd[i + ii])

/* Message schedule computation */
#define MSCH(W, ii, i)				\
	W[i + ii + 16] = s1(W[i + ii + 14]) + W[i + ii + 9] + s0(W[i + ii + 1]) + W[i + ii]

/*
 * SHA256 block compression function.  The 256-bit state is transformed via
 * the 512-bit input block to produce a new state.
 */
static void
SHA256_Transform(uint32_t state[static_restrict 8],
                 const uint8_t block[static_restrict 64],
                 uint32_t W[static_restrict 64], uint32_t S[static_restrict 8])
{
    int i;

    /* 1. Prepare the first part of the message schedule W. */
    be32dec_vect(W, block, 8);

    /* 2. Initialize working variables. */
    memcpy(S, state, 32);

    /* 3. Mix. */
    for (i = 0; i < 64; i += 16) {
        RNDr(S, W, 0, i);
        RNDr(S, W, 1, i);
        RNDr(S, W, 2, i);
        RNDr(S, W, 3, i);
        RNDr(S, W, 4, i);
        RNDr(S, W, 5, i);
        RNDr(S, W, 6, i);
        RNDr(S, W, 7, i);
        RNDr(S, W, 8, i);
        RNDr(S, W, 9, i);
        RNDr(S, W, 10, i);
        RNDr(S, W, 11, i);
        RNDr(S, W, 12, i);
        RNDr(S, W, 13, i);
        RNDr(S, W, 14, i);
        RNDr(S, W, 15, i);

        if (i == 48)
            break;
        MSCH(W, 0, i);
        MSCH(W, 1, i);
        MSCH(W, 2, i);
        MSCH(W, 3, i);
        MSCH(W, 4, i);
        MSCH(W, 5, i);
        MSCH(W, 6, i);
        MSCH(W, 7, i);
        MSCH(W, 8, i);
        MSCH(W, 9, i);
        MSCH(W, 10, i);
        MSCH(W, 11, i);
        MSCH(W, 12, i);
        MSCH(W, 13, i);
        MSCH(W, 14, i);
        MSCH(W, 15, i);
    }

    /* 4. Mix local working variables into global state. */
    state[0] += S[0];
    state[1] += S[1];
    state[2] += S[2];
    state[3] += S[3];
    state[4] += S[4];
    state[5] += S[5];
    state[6] += S[6];
    state[7] += S[7];
}

static const uint8_t PAD[64] = {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* Add padding and terminating bit-count. */
static void
SHA256_Pad(SHA256_CTX * ctx, uint32_t tmp32[static_restrict 72])
{
    size_t r;

    /* Figure out how many bytes we have buffered. */
    r = (ctx->count >> 3) & 0x3f;

    /* Pad to 56 mod 64, transforming if we finish a block en route. */
    if (r < 56) {
        /* Pad to 56 mod 64. */
        memcpy(&ctx->buf[r], PAD, 56 - r);
    } else {
        /* Finish the current block and mix. */
        memcpy(&ctx->buf[r], PAD, 64 - r);
        SHA256_Transform(ctx->state, ctx->buf, &tmp32[0], &tmp32[64]);

        /* The start of the final block is all zeroes. */
        memset(&ctx->buf[0], 0, 56);
    }

    /* Add the terminating bit-count. */
    be64enc(&ctx->buf[56], ctx->count);

    /* Mix in the final block. */
    SHA256_Transform(ctx->state, ctx->buf, &tmp32[0], &tmp32[64]);
}

/* Magic initialization constants. */
static const uint32_t initial_state[8] = {
    0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
    0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19
};

/**
 * SHA256_Init(ctx):
 * Initialize the SHA256 context ${ctx}.
 */
void
SHA256_Init(SHA256_CTX * ctx)
{

    /* Zero bits processed so far. */
    ctx->count = 0;

    /* Initialize state. */
    memcpy(ctx->state, initial_state, sizeof(initial_state));
}

/**
 * SHA256_Update(ctx, in, len):
 * Input ${len} bytes from ${in} into the SHA256 context ${ctx}.
 */
static void
_SHA256_Update(SHA256_CTX * ctx, const void * in, size_t len,
               uint32_t tmp32[static_restrict 72])
{
    uint32_t r;
    const uint8_t * src = in;

    /* Return immediately if we have nothing to do. */
    if (len == 0)
        return;

    /* Number of bytes left in the buffer from previous updates. */
    r = (ctx->count >> 3) & 0x3f;

    /* Update number of bits. */
    ctx->count += (uint64_t)(len) << 3;

    /* Handle the case where we don't need to perform any transforms. */
    if (len < 64 - r) {
        memcpy(&ctx->buf[r], src, len);
        return;
    }

    /* Finish the current block. */
    memcpy(&ctx->buf[r], src, 64 - r);
    SHA256_Transform(ctx->state, ctx->buf, &tmp32[0], &tmp32[64]);
    src += 64 - r;
    len -= 64 - r;

    /* Perform complete blocks. */
    while (len >= 64) {
        SHA256_Transform(ctx->state, src, &tmp32[0], &tmp32[64]);
        src += 64;
        len -= 64;
    }

    /* Copy left over data into buffer. */
    memcpy(ctx->buf, src, len);
}

/**
 * SHA256_Final(digest, ctx):
 * Output the SHA256 hash of the data input to the context ${ctx} into the
 * buffer ${digest}.
 */
static void
_SHA256_Final(uint8_t digest[32], SHA256_CTX * ctx,
              uint32_t tmp32[static_restrict 72])
{

    /* Add padding. */
    SHA256_Pad(ctx, tmp32);

    /* Write the hash. */
    be32enc_vect(digest, ctx->state, 4);
}

void
SHA256_Buf(const void * in, size_t len, uint8_t digest[32])
{
    SHA256_CTX ctx;
    uint32_t tmp32[72];

    SHA256_Init(&ctx);
    _SHA256_Update(&ctx, in, len, tmp32);
    _SHA256_Final(digest, &ctx, tmp32);
}

void
SHA256d_Buf(const void * in, size_t len, uint8_t digest[32])
{
    SHA256_CTX ctx;
    uint32_t tmp32[72];

    SHA256_Init(&ctx);
    _SHA256_Update(&ctx, in, len, tmp32);
    _SHA256_Final(digest, &ctx, tmp32);

    SHA256_Init(&ctx);
    _SHA256_Update(&ctx, digest, len, tmp32);
    _SHA256_Final(digest, &ctx, tmp32);
}
