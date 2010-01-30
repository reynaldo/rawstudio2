/*
 * Copyright (C) 2006-2009 Anders Brander <anders@brander.dk> and
 * Anders Kvist <akv@lnxbx.dk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/* Plugin tmpl version 4 */

#include <rawstudio.h>
#include <lensfun.h>

#if defined (__SSE2__)

#include <emmintrin.h>

static gfloat twofiftytwo_ps[4] __attribute__ ((aligned (16))) = {256.0f, 256.0f, 256.0f, 0.0f};

gboolean is_sse2_compiled()
{
	return TRUE;
}

void
rs_image16_bilinear_full_sse2(RS_IMAGE16 *in, gushort *out, gfloat *pos)
{
	const gint m_w = (in->w-1);
	const gint m_h = (in->h-1);

	__m128 p0, p1;
	if ((uintptr_t)pos & 15)
	{
		p0 = _mm_loadu_ps(pos);		// y1x1 y0x0
		p1 = _mm_loadu_ps(pos+4);	// ---- y2x2
	} else 
	{
		p0 = _mm_load_ps(pos);		// y1x1 y0x0
		p1 = _mm_load_ps(pos+4);	// ---- y2x2
	}
		
	__m128 xf = _mm_shuffle_ps(p1, p0, _MM_SHUFFLE(0,2,2,0));
	__m128 yf = _mm_shuffle_ps(p1, p0, _MM_SHUFFLE(1,3,1,1));
			
	__m128 fl256 = _mm_load_ps(twofiftytwo_ps);
	xf = _mm_mul_ps(xf, fl256);
	yf = _mm_mul_ps(yf, fl256);
	__m128i x = _mm_cvttps_epi32(xf);
	__m128i y = _mm_cvttps_epi32(yf);

	__m128i _m_w = _mm_slli_epi32(_mm_set1_epi32(m_w), 8);
	__m128i _m_h = _mm_slli_epi32(_mm_set1_epi32(m_h), 8);
	
	__m128i x_gt, y_gt;
	
	/* If positions from lensfun is properly clamped this should not be needed */
	/* Clamping */
	x_gt = _mm_cmpgt_epi32(x, _m_w);
	y_gt = _mm_cmpgt_epi32(y, _m_h);
	
	x = _mm_or_si128(_mm_andnot_si128(x_gt, x), _mm_and_si128(_m_w, x_gt));
	y = _mm_or_si128(_mm_andnot_si128(y_gt, y), _mm_and_si128(_m_h, y_gt));

	__m128i zero = _mm_setzero_si128();
	__m128i x_lt = _mm_cmplt_epi32(x, zero);
	__m128i y_lt = _mm_cmplt_epi32(y, zero);
	x = _mm_andnot_si128(x_lt, x);
	y = _mm_andnot_si128(y_lt, y);
	__m128i one = _mm_set1_epi32(1);
	__m128i nx = _mm_add_epi32(one, _mm_srai_epi32(x, 8));
	__m128i ny = _mm_add_epi32(one, _mm_srai_epi32(y, 8));

	/* Check that 'next' pixels are in bounds */
	_m_w = _mm_srai_epi32(_m_w, 8);
	_m_h = _mm_srai_epi32(_m_h, 8);

	x_gt = _mm_cmpgt_epi32(nx, _m_w);
	y_gt = _mm_cmpgt_epi32(ny, _m_h);
	
	nx = _mm_or_si128(_mm_andnot_si128(x_gt, nx), _mm_and_si128(_m_w, x_gt));
	ny = _mm_or_si128(_mm_andnot_si128(y_gt, ny), _mm_and_si128(_m_h, y_gt));

	int xfer[16] __attribute__ ((aligned (16)));

	_mm_store_si128((__m128i*)xfer, _mm_srai_epi32(x, 8));
	_mm_store_si128((__m128i*)&xfer[4], _mm_srai_epi32(y, 8));
	_mm_store_si128((__m128i*)&xfer[8], nx);
	_mm_store_si128((__m128i*)&xfer[12], ny);
	
	gushort* pixels[12];
	
	/* Loop unrolled, allows agressive instruction reordering */
	/* Red, then G & B */
	pixels[0] = GET_PIXEL(in, xfer[0], xfer[4]); 	// a
	pixels[1] = GET_PIXEL(in, xfer[8], xfer[4]);	// b
	pixels[2] = GET_PIXEL(in, xfer[0], xfer[12]);	// c
	pixels[3] = GET_PIXEL(in, xfer[8], xfer[12]);	// d
		
	pixels[4] = GET_PIXEL(in, xfer[1], xfer[1+4]) + 1; 		// a
	pixels[4+1] = GET_PIXEL(in, xfer[1+8], xfer[1+4]) + 1;	// b
	pixels[4+2] = GET_PIXEL(in, xfer[1], xfer[1+12]) + 1;	// c
	pixels[4+3] = GET_PIXEL(in, xfer[1+8], xfer[1+12]) + 1;	// d

	pixels[2*4] = GET_PIXEL(in, xfer[2], xfer[2+4]) + 2; 		// a
	pixels[2*4+1] = GET_PIXEL(in, xfer[2+8], xfer[2+4]) + 2;	// b
	pixels[2*4+2] = GET_PIXEL(in, xfer[2], xfer[2+12]) + 2;		// c
	pixels[2*4+3] = GET_PIXEL(in, xfer[2+8], xfer[2+12]) + 2;	// d

	/* Calculate distances */
	__m128i twofiftyfive = _mm_set1_epi32(255);
	__m128i diffx = _mm_and_si128(x, twofiftyfive);	
	__m128i diffy = _mm_and_si128(y, twofiftyfive);	
	__m128i inv_diffx = _mm_andnot_si128(diffx, twofiftyfive);
	__m128i inv_diffy = _mm_andnot_si128(diffy, twofiftyfive);

	/* Calculate weights */
	__m128i aw = _mm_srai_epi32(_mm_mullo_epi16(inv_diffx, inv_diffy),1);
	__m128i bw = _mm_srai_epi32(_mm_mullo_epi16(diffx, inv_diffy),1);
	__m128i cw = _mm_srai_epi32(_mm_mullo_epi16(inv_diffx, diffy),1);
	__m128i dw = _mm_srai_epi32(_mm_mullo_epi16(diffx, diffy),1);

	_mm_store_si128((__m128i*)xfer, aw);
	_mm_store_si128((__m128i*)&xfer[4], bw);
	_mm_store_si128((__m128i*)&xfer[8], cw);
	_mm_store_si128((__m128i*)&xfer[12], dw);
	
	gushort** p = pixels;
	/* Loop unrolled */
	out[0]  = (gushort) ((xfer[0] * *p[0] + xfer[4] * *p[1] + xfer[8] * *p[2] + xfer[12] * *p[3]  + 16384) >> 15 );
	p+=4;
	out[1]  = (gushort) ((xfer[1] * *p[0] + xfer[1+4] * *p[1] + xfer[1+8] * *p[2] + xfer[1+12] * *p[3]  + 16384) >> 15 );
	p+=4;
	out[2]  = (gushort) ((xfer[2] * *p[0] + xfer[2+4] * *p[1] + xfer[2+8] * *p[2] + xfer[2+12] * *p[3]  + 16384) >> 15 );
}
#else
gboolean is_sse2_compiled()
{
	return FALSE;
}

void
rs_image16_bilinear_full_sse2(RS_IMAGE16 *in, gushort *out, gfloat *pos)
{
}
#endif // defined (__SSE2__)
