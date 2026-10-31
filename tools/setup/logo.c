/*
 * The title screen's logo, made by Setup.exe from the player's own disc.
 *
 * The game's logo is on the disc as a 128x128 16-colour sprite (shown on the
 * language screen), compressed inside overlay 226. It is Konami's artwork, so
 * the release does not contain it: Setup takes it from the disc, smooths out
 * its dithering, scales it up into the theme's title logo frame (the 400x175
 * logo art the theme is laid out for, 3x) and gives it a clean edge. The "PC"
 * beside it and the animation are the project's own (setup\logo).
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "logo.h"

#define SP 128			/* the sprite: 128x128, 4 bits a pixel */
#define CANVAS_W 1122		/* "_title logo dsf (res 374x176).png", 3x */
#define CANVAS_H 528
#define SCALE 3.0
#define CROP_X 62.0		/* the art's columns kept: 62..336 */
#define CROP_END 336.0
#define SPRITE_X 67.0		/* where the sprite sits on the 400x175 art, and its size there */
#define SPRITE_Y -48.0
#define SPRITE_K 2.12

/* The game's LZ (the same as tools/ps1disc/deluxe/dlxbuild.c's). */
static uint32_t lz_decode( const uint8_t *s, uint32_t sn, uint8_t *out, uint32_t cap )
{
	uint32_t i = 0, o = 0, ctl = 0;
	for( ;; )
	{
		uint32_t dd;
		ctl >>= 1;
		if( !(ctl & 0x100) )
		{
			if( i >= sn ) return 0xFFFFFFFFu;
			ctl = s[i++] | 0xFF00;
		}
		if( i >= sn ) return 0xFFFFFFFFu;
		dd = s[i++];
		if( !(ctl & 1) ) { if( o >= cap ) return 0xFFFFFFFFu; out[o++] = (uint8_t)dd; continue; }
		if( dd == 0xFF ) return o;
		if( dd >= 0xC0 )
		{
			uint32_t k = dd - 0xB8;
			if( i + k > sn || o + k > cap ) return 0xFFFFFFFFu;
			memcpy( out + o, s + i, k ); i += k; o += k;
			continue;
		}
		{
			uint32_t dist, len, k;
			if( dd < 0x80 ) { if( i >= sn ) return 0xFFFFFFFFu; dist = ((dd & 3) << 8) | s[i++]; len = (dd >> 2) + 3; }
			else { dist = (dd & 0xF) + 1; len = (dd >> 4) - 6; }
			if( dist > o || o + len > cap ) return 0xFFFFFFFFu;
			for( k = 0; k < len; ++k, ++o ) out[o] = out[o - dist];
		}
	}
}

static uint32_t rd32( const uint8_t *p ) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t rd16( const uint8_t *p ) { return p[0] | (uint32_t)p[1] << 8; }

/* Catmull-Rom weight */
static double cr( double t )
{
	t = fabs( t );
	return t < 1 ? 1.5 * t * t * t - 2.5 * t * t + 1 : t < 2 ? -0.5 * t * t * t + 2.5 * t * t - 4 * t + 2 : 0;
}

/* Sample a w x h, 3-channel float image at (u, v) (pixel centres at +0.5). */
static void bicubic( const float *img, int w, int h, double u, double v, double *out )
{
	const int x0 = (int)floor( u - 0.5 ), y0 = (int)floor( v - 0.5 );
	const double fx = u - 0.5 - x0, fy = v - 0.5 - y0;
	double sum[3] = { 0, 0, 0 }, ws = 0;
	int i, j, c;
	for( j = -1; j <= 2; ++j )
		for( i = -1; i <= 2; ++i )
		{
			const double wgt = cr( i - fx ) * cr( j - fy );
			int x = x0 + i, y = y0 + j;
			x = x < 0 ? 0 : x >= w ? w - 1 : x;
			y = y < 0 ? 0 : y >= h ? h - 1 : y;
			for( c = 0; c < 3; ++c )
				sum[c] += wgt * img[(y * w + x) * 3 + c];
			ws += wgt;
		}
	for( c = 0; c < 3; ++c )
		out[c] = sum[c] / ws;
}

static int cmp_float( const void *a, const void *b )
{
	const float x = *(const float *)a, y = *(const float *)b;
	return x < y ? -1 : x > y;
}

/* ---- PNG (stored deflate: no compression library needed) ---- */

static uint32_t g_crc[256];

static uint32_t crc_update( uint32_t c, const uint8_t *p, size_t n )
{
	while( n-- )
		c = g_crc[(c ^ *p++) & 0xFF] ^ (c >> 8);
	return c;
}

static void put_be32( uint8_t *p, uint32_t v ) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }

static int chunk( FILE *f, const char *type, const uint8_t *data, uint32_t n )
{
	uint8_t hdr[8], tail[4];
	uint32_t c;
	put_be32( hdr, n );
	memcpy( hdr + 4, type, 4 );
	c = crc_update( 0xFFFFFFFFu, hdr + 4, 4 );
	c = crc_update( c, data, n ) ^ 0xFFFFFFFFu;
	put_be32( tail, c );
	return fwrite( hdr, 1, 8, f ) == 8 && (n == 0 || fwrite( data, 1, n, f ) == n) && fwrite( tail, 1, 4, f ) == 4;
}

static int write_png( const char *path, const uint8_t *rgba, int w, int h )
{
	static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
	const size_t raw_n = (size_t)h * (1 + (size_t)w * 4);
	const size_t blocks = (raw_n + 65534) / 65535;
	uint8_t ihdr[13], *raw = malloc( raw_n ), *z = malloc( 2 + raw_n + blocks * 5 + 4 );
	uint32_t a = 1, b = 0, k;
	size_t i, zn = 0, pos;
	FILE *f;
	int ok, y;
	for( k = 0; k < 256; ++k )
	{
		uint32_t c = k, j;
		for( j = 0; j < 8; ++j )
			c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
		g_crc[k] = c;
	}
	for( y = 0; y < h; ++y )
	{
		raw[(size_t)y * (1 + w * 4)] = 0;
		memcpy( raw + (size_t)y * (1 + w * 4) + 1, rgba + (size_t)y * w * 4, (size_t)w * 4 );
	}
	z[zn++] = 0x78;
	z[zn++] = 0x01;
	for( pos = 0; pos < raw_n; pos += 65535 )
	{
		const size_t n = raw_n - pos < 65535 ? raw_n - pos : 65535;
		z[zn++] = pos + n == raw_n;
		z[zn++] = (uint8_t)n; z[zn++] = (uint8_t)(n >> 8);
		z[zn++] = (uint8_t)~n; z[zn++] = (uint8_t)(~n >> 8);
		memcpy( z + zn, raw + pos, n );
		zn += n;
	}
	for( i = 0; i < raw_n; ++i )
	{
		a = (a + raw[i]) % 65521;
		b = (b + a) % 65521;
	}
	put_be32( z + zn, b << 16 | a );
	zn += 4;
	put_be32( ihdr, (uint32_t)w );
	put_be32( ihdr + 4, (uint32_t)h );
	ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = ihdr[11] = ihdr[12] = 0;
	ok = (f = fopen( path, "wb" )) != NULL;
	ok = ok && fwrite( sig, 1, 8, f ) == 8 && chunk( f, "IHDR", ihdr, 13 ) && chunk( f, "IDAT", z, (uint32_t)zn ) && chunk( f, "IEND", NULL, 0 );
	if( f && fclose( f ) != 0 )
		ok = 0;
	free( raw );
	free( z );
	return ok;
}

/* ---- the logo ---- */

int make_title_logo( const uint8_t *overlay, uint32_t overlay_n, const char *out_png )
{
	static uint8_t tim[16384];
	const uint8_t *px;
	uint32_t n, clut[16], i;
	int opaque[SP * SP], index[SP * SP], x, y, c, pass, changed;
	float col[SP * SP * 3], *up, *med, *alpha, *tmp;
	uint8_t *rgba;
	const int UW = SP * 2;
	double sigma = 5.0, kern[24], ks = 0;
	int kr = 15, ok;

	if( overlay_n <= LOGO_OFFSET )
		return 0;
	n = lz_decode( overlay + LOGO_OFFSET, overlay_n - LOGO_OFFSET, tim, sizeof tim );
	/* a 4-bit TIM, 16-colour palette, 128x128 */
	if( n != 8256 || rd32( tim ) != 0x10 || rd32( tim + 4 ) != 8 || rd32( tim + 8 ) != 0x2C || rd16( tim + 16 ) != 16 ||
	    rd16( tim + 8 + 0x2C + 8 ) != 32 || rd16( tim + 8 + 0x2C + 10 ) != 128 )
		return 0;
	for( i = 0; i < 16; ++i )
		clut[i] = rd16( tim + 20 + 2 * i );
	px = tim + 8 + rd32( tim + 8 ) + 12;

	/* Colours; transparent = the magenta key. The sprite's thin dark outline
	 * (palette 1) keeps its place in the shape but takes its neighbours'
	 * colour, so the edge is the logo's green, as on the logo art. */
	for( y = 0; y < SP; ++y )
		for( x = 0; x < SP; ++x )
		{
			const int k = (px[y * 64 + x / 2] >> ((x & 1) * 4)) & 15;
			index[y * SP + x] = k;
			const uint32_t v = clut[k];
			const int key = (v & 0x7FFF) == 0x7C1F || v == 0;
			opaque[y * SP + x] = key ? 0 : k == 1 ? 2 : 1;	/* 2: shape, but no colour of its own */
			col[(y * SP + x) * 3 + 0] = (float)((v & 31) * 255 / 31);
			col[(y * SP + x) * 3 + 1] = (float)(((v >> 5) & 31) * 255 / 31);
			col[(y * SP + x) * 3 + 2] = (float)(((v >> 10) & 31) * 255 / 31);
		}
	/* The logo's outer band is one even green on the art; on the sprite it
	 * is dithered with darker greens, which would fringe the edge. The band
	 * (2 pixels in from the edge) takes its most common colour. */
	{
		static int dist[SP * SP];
		int count[16] = { 0 }, d, best = -1, b = 0;
		for( i = 0; i < SP * SP; ++i )
			dist[i] = opaque[i] ? 99 : 0;
		for( d = 1; d <= 2; ++d )
			for( y = 0; y < SP; ++y )
				for( x = 0; x < SP; ++x )
				{
					const int k = y * SP + x;
					if( dist[k] != 99 )
						continue;
					if( (x > 0 && dist[k - 1] == d - 1) || (x < SP - 1 && dist[k + 1] == d - 1) ||
					    (y > 0 && dist[k - SP] == d - 1) || (y < SP - 1 && dist[k + SP] == d - 1) ||
					    x == 0 || y == 0 || x == SP - 1 || y == SP - 1 )
						dist[k] = d;
				}
		for( i = 0; i < SP * SP; ++i )
			if( dist[i] >= 1 && dist[i] <= 2 && opaque[i] == 1 )
				++count[index[i]];
		for( i = 0; i < 16; ++i )
			if( count[i] > b )
				b = count[i], best = (int)i;
		if( best >= 0 )
			for( i = 0; i < SP * SP; ++i )
				if( dist[i] >= 1 && dist[i] <= 2 )
				{
					col[i * 3 + 0] = (float)((clut[best] & 31) * 255 / 31);
					col[i * 3 + 1] = (float)(((clut[best] >> 5) & 31) * 255 / 31);
					col[i * 3 + 2] = (float)(((clut[best] >> 10) & 31) * 255 / 31);
					opaque[i] = 1;
				}
	}
	/* Pixels without a colour of their own take the nearest one's, so
	 * scaling up pulls no magenta or black into the edge. */
	{
		int *have = malloc( sizeof(int) * SP * SP ), *next = malloc( sizeof(int) * SP * SP );
		for( i = 0; i < SP * SP; ++i )
			have[i] = opaque[i] == 1;
		for( pass = 0, changed = 1; changed && pass < SP * 2; ++pass )
		{
			memcpy( next, have, sizeof(int) * SP * SP );
			changed = 0;
			for( y = 0; y < SP; ++y )
				for( x = 0; x < SP; ++x )
				{
					static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
					int d;
					if( have[y * SP + x] )
						continue;
					for( d = 0; d < 4; ++d )
					{
						const int xx = x + dx[d], yy = y + dy[d];
						if( xx < 0 || yy < 0 || xx >= SP || yy >= SP || !have[yy * SP + xx] )
							continue;
						for( c = 0; c < 3; ++c )
							col[(y * SP + x) * 3 + c] = col[(yy * SP + xx) * 3 + c];
						next[y * SP + x] = 1;
						changed = 1;
						break;
					}
				}
			memcpy( have, next, sizeof(int) * SP * SP );
		}
		free( have );
		free( next );
	}

	/* 2x, then a 3x3 median: the dithering melts into its shades. */
	up = malloc( sizeof(float) * UW * UW * 3 );
	med = malloc( sizeof(float) * UW * UW * 3 );
	for( y = 0; y < UW; ++y )
		for( x = 0; x < UW; ++x )
		{
			double o[3];
			bicubic( col, SP, SP, (x + 0.5) / 2.0, (y + 0.5) / 2.0, o );
			for( c = 0; c < 3; ++c )
				up[(y * UW + x) * 3 + c] = (float)(o[c] < 0 ? 0 : o[c] > 255 ? 255 : o[c]);
		}
	for( y = 0; y < UW; ++y )
		for( x = 0; x < UW; ++x )
			for( c = 0; c < 3; ++c )
			{
				float v[9];
				int k = 0, i2, j2;
				for( j2 = -1; j2 <= 1; ++j2 )
					for( i2 = -1; i2 <= 1; ++i2 )
					{
						int xx = x + i2, yy = y + j2;
						xx = xx < 0 ? 0 : xx >= UW ? UW - 1 : xx;
						yy = yy < 0 ? 0 : yy >= UW ? UW - 1 : yy;
						v[k++] = up[(yy * UW + xx) * 3 + c];
					}
				qsort( v, 9, sizeof *v, cmp_float );
				med[(y * UW + x) * 3 + c] = v[4];
			}

	/* Into the frame; the shape: the sprite's, scaled up, blurred a little
	 * and cut again, for a smooth, slightly anti-aliased edge. */
	rgba = malloc( (size_t)CANVAS_W * CANVAS_H * 4 );
	alpha = malloc( sizeof(float) * CANVAS_W * CANVAS_H );
	tmp = malloc( sizeof(float) * CANVAS_W * CANVAS_H );
	for( y = 0; y < CANVAS_H; ++y )
		for( x = 0; x < CANVAS_W; ++x )
		{
			const double ax = (x + 0.5) / SCALE + CROP_X, ay = (y + 0.5) / SCALE;
			const double u = (ax - SPRITE_X) / SPRITE_K, v = (ay - SPRITE_Y) / SPRITE_K;	/* sprite pixels */
			double o[3], fx, fy, s = 0;
			int x0, y0, i2, j2;
			bicubic( med, UW, UW, u * 2, v * 2, o );
			for( c = 0; c < 3; ++c )
				rgba[((size_t)y * CANVAS_W + x) * 4 + c] = (uint8_t)(o[c] < 0 ? 0 : o[c] > 255 ? 255 : o[c] + 0.5);
			x0 = (int)floor( u - 0.5 );
			y0 = (int)floor( v - 0.5 );
			fx = u - 0.5 - x0;
			fy = v - 0.5 - y0;
			for( j2 = 0; j2 <= 1; ++j2 )
				for( i2 = 0; i2 <= 1; ++i2 )
				{
					const int xx = x0 + i2, yy = y0 + j2;
					const int in = xx >= 0 && yy >= 0 && xx < SP && yy < SP && opaque[yy * SP + xx];
					s += (i2 ? fx : 1 - fx) * (j2 ? fy : 1 - fy) * in;
				}
			alpha[y * CANVAS_W + x] = (float)s;
		}
	for( i = 0; i <= (uint32_t)kr; ++i )
		ks += (kern[i] = exp( -(double)(i * i) / (2 * sigma * sigma) )) * (i ? 2 : 1);
	for( y = 0; y < CANVAS_H; ++y )
		for( x = 0; x < CANVAS_W; ++x )
		{
			double s = 0;
			int k;
			for( k = -kr; k <= kr; ++k )
			{
				int xx = x + k;
				xx = xx < 0 ? 0 : xx >= CANVAS_W ? CANVAS_W - 1 : xx;
				s += kern[k < 0 ? -k : k] * alpha[y * CANVAS_W + xx];
			}
			tmp[y * CANVAS_W + x] = (float)(s / ks);
		}
	for( y = 0; y < CANVAS_H; ++y )
		for( x = 0; x < CANVAS_W; ++x )
		{
			double s = 0, a;
			int k;
			for( k = -kr; k <= kr; ++k )
			{
				int yy = y + k;
				yy = yy < 0 ? 0 : yy >= CANVAS_H ? CANVAS_H - 1 : yy;
				s += kern[k < 0 ? -k : k] * tmp[yy * CANVAS_W + x];
			}
			a = (s / ks - 0.5) * 4.0 + 0.5;
			if( x >= (CROP_END - CROP_X) * SCALE )
				a = 0;
			a = a < 0 ? 0 : a > 1 ? 1 : a;
			rgba[((size_t)y * CANVAS_W + x) * 4 + 3] = (uint8_t)(a * 255 + 0.5);
		}
	ok = write_png( out_png, rgba, CANVAS_W, CANVAS_H );
	free( up );
	free( med );
	free( alpha );
	free( tmp );
	free( rgba );
	return ok;
}
