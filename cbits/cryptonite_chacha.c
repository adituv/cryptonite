/*
 * Copyright (c) 2014 Vincent Hanquez <vincent@snarc.org>
 *
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
 * 3. Neither the name of the author nor the names of his contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdint.h>
#include <string.h>
#include "cryptonite_chacha.h"
#include "cryptonite_bitfn.h"
#include <stdio.h>

#define USE_8BITS 0

#define QR(a,b,c,d) \
	a += b; d = rol32(d ^ a,16); \
	c += d; b = rol32(b ^ c,12); \
	a += b; d = rol32(d ^ a, 8); \
	c += d; b = rol32(b ^ c, 7);

static const uint8_t sigma[16] = "expand 32-byte k";
static const uint8_t tau[16] = "expand 16-byte k";

static inline uint32_t load32(const uint8_t *p)
{
	return le32_to_cpu(*((uint32_t *) p));
}

static void chacha_core(int rounds, block *out, const cryptonite_chacha_state *in)
{
	uint32_t x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15;
	int i;

	x0 = in->d[0]; x1 = in->d[1]; x2 = in->d[2]; x3 = in->d[3];
	x4 = in->d[4]; x5 = in->d[5]; x6 = in->d[6]; x7 = in->d[7];
	x8 = in->d[8]; x9 = in->d[9]; x10 = in->d[10]; x11 = in->d[11];
	x12 = in->d[12]; x13 = in->d[13]; x14 = in->d[14]; x15 = in->d[15];

	for (i = rounds; i > 0; i -= 2) {
		QR(x0, x4, x8, x12);
		QR(x1, x5, x9, x13);
		QR(x2, x6, x10, x14);
		QR(x3, x7, x11, x15);

		QR(x0, x5, x10, x15);
		QR(x1, x6, x11, x12);
		QR(x2, x7, x8, x13);
		QR(x3, x4, x9, x14);
	}

	x0 += in->d[0]; x1 += in->d[1]; x2 += in->d[2]; x3 += in->d[3];
	x4 += in->d[4]; x5 += in->d[5]; x6 += in->d[6]; x7 += in->d[7];
	x8 += in->d[8]; x9 += in->d[9]; x10 += in->d[10]; x11 += in->d[11];
	x12 += in->d[12]; x13 += in->d[13]; x14 += in->d[14]; x15 += in->d[15];

	out->d[0] = cpu_to_le32(x0);
	out->d[1] = cpu_to_le32(x1);
	out->d[2] = cpu_to_le32(x2);
	out->d[3] = cpu_to_le32(x3);
	out->d[4] = cpu_to_le32(x4);
	out->d[5] = cpu_to_le32(x5);
	out->d[6] = cpu_to_le32(x6);
	out->d[7] = cpu_to_le32(x7);
	out->d[8] = cpu_to_le32(x8);
	out->d[9] = cpu_to_le32(x9);
	out->d[10] = cpu_to_le32(x10);
	out->d[11] = cpu_to_le32(x11);
	out->d[12] = cpu_to_le32(x12);
	out->d[13] = cpu_to_le32(x13);
	out->d[14] = cpu_to_le32(x14);
	out->d[15] = cpu_to_le32(x15);
}

/* only 2 valids values are 256 (32) and 128 (16) */
void cryptonite_chacha_init(cryptonite_chacha_state *st,
                            uint32_t keylen, const uint8_t *key,
                            uint32_t ivlen, const uint8_t *iv)
{
	const uint8_t *constants = (keylen == 32) ? sigma : tau;
	int i;

	st->d[0] = load32(constants + 0);
	st->d[1] = load32(constants + 4);
	st->d[2] = load32(constants + 8);
	st->d[3] = load32(constants + 12);

	st->d[4] = load32(key + 0);
	st->d[5] = load32(key + 4);
	st->d[6] = load32(key + 8);
	st->d[7] = load32(key + 12);
	/* we repeat the key on 128 bits */
	if (keylen == 32)
		key += 16;
	st->d[8] = load32(key + 0);
	st->d[9] = load32(key + 4);
	st->d[10] = load32(key + 8);
	st->d[11] = load32(key + 12);
	st->d[12] = 0;
	switch (ivlen) {
	case 8:
		st->d[13] = 0;
		st->d[14] = load32(iv + 0);
		st->d[15] = load32(iv + 4);
		break;
	case 12:
		st->d[13] = load32(iv + 0);
		st->d[14] = load32(iv + 4);
		st->d[15] = load32(iv + 8);
	default:
		return;
	}
}

void cryptonite_chacha_combine(uint32_t rounds, block *dst, cryptonite_chacha_state *st, const block *src, uint32_t bytes)
{
	block out;
	int i;

	if (!bytes)
		return;

	for (;; bytes -= 64, src += 1, dst += 1) {
		chacha_core(rounds, &out, st);

		st->d[12] += 1;
		if (st->d[12] == 0)
			st->d[13] += 1;

		if (bytes <= 64) {
			for (i = 0; i < bytes; i++)
				dst->b[i] = src->b[i] ^ out.b[i];
			for (; i < 64; i++)
				dst->b[i] = out.b[i];
			return;
		}

#if USE_8BITS
		for (i = 0; i < 64; ++i)
			dst->b[i] = src->b[i] ^ out.b[i];
#else
		/* fast copy using 64 bits */
		for (i = 0; i < 8; i++)
			dst->q[i] = src->q[i] ^ out.q[i];
#endif
	}
}

void cryptonite_chacha_generate(uint32_t rounds, block *dst, cryptonite_chacha_state *st, uint32_t bytes)
{
	block out;
	int i;

	if (!bytes)
		return;

	for (;; bytes -= 64, dst += 1) {
		chacha_core(rounds, &out, st);

		st->d[12] += 1;
		if (st->d[12] == 0)
			st->d[13] += 1;
		if (bytes <= 64) {
			for (i = 0; i < bytes; ++i)
				dst->b[i] = out.b[i];
			return;
		}
#if USE_8BITS
		for (i = 0; i < 64; ++i)
			dst->b[i] = out.b[i];
#else
		for (i = 0; i < 8; i++)
			dst->q[i] = out.q[i];
#endif
	}
}

void cryptonite_chacha_random(uint32_t rounds, uint8_t *dst, cryptonite_chacha_state *st, uint32_t bytes)
{
	block out;

	if (!bytes)
		return;
	for (; bytes >= 16; bytes -= 16, dst += 16) {
		chacha_core(rounds, &out, st);
		memcpy(dst, out.b + 40, 16);
		cryptonite_chacha_init(st, 32, out.b, 8, out.b + 32);
	}
	if (bytes) {
		chacha_core(rounds, &out, st);
		memcpy(dst, out.b + 40, bytes);
		cryptonite_chacha_init(st, 32, out.b, 8, out.b + 32);
	}
}
