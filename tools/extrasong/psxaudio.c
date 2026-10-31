/* psxaudio -- PlayStation ADPCM encoders for the extra-song builder.
 *
 *   psxaudio spu IN.s16 OUT.bin      stereo 44.1 kHz PCM (s16le, interleaved)
 *                                    -> Dancing Stage Fusion's STR.BIN stream
 *                                       layout (see build_extrasong.py)
 *   psxaudio xa IN.s16 OUT.bin       stereo 37.8 kHz PCM -> XA ADPCM sound
 *                                    groups, 2304 bytes per sector (18 x 128)
 *
 * Both search every filter and shift per unit and keep the decoder's state,
 * so the result decodes exactly as the encoder predicted.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int F0[5] = { 0, 60, 115, 98, 122 };
static const int F1[5] = { 0, 0, -52, -55, -60 };

static int16_t clamp16( int v ) { return (int16_t)(v < -32768? -32768: v > 32767? 32767: v); }

/* Encode 28 samples: best (filter, shift) with the decoder's arithmetic.
 * nfilters: 5 for SPU, 4 for XA.  Returns the header byte, writes nibbles. */
static uint8_t encode_unit( const int16_t *in, int nfilters, int *s1, int *s2, uint8_t nib[28] )
{
	double best = -1;
	int best_f = 0, best_sh = 0, best_s1 = *s1, best_s2 = *s2;
	uint8_t best_nib[28];
	for( int f = 0; f < nfilters; ++f )
	{
		for( int sh = 0; sh <= 12; ++sh )
		{
			int p1 = *s1, p2 = *s2;
			double err = 0;
			uint8_t n[28];
			for( int i = 0; i < 28; ++i )
			{
				const int pred = (p1 * F0[f] + p2 * F1[f] + 32) >> 6;
				const int diff = in[i] - pred;
				/* nibble q such that (q << 12 >> sh) ~ diff */
				int q = (diff * (1 << sh) + (diff >= 0? 2048: -2048)) / 4096;
				if( q > 7 ) q = 7;
				if( q < -8 ) q = -8;
				const int out = clamp16( ((q << 12) >> sh) + pred );
				const double e = (double)(in[i] - out);
				err += e * e;
				n[i] = (uint8_t)(q & 15);
				p2 = p1; p1 = out;
			}
			if( best < 0 || err < best )
			{
				best = err; best_f = f; best_sh = sh; best_s1 = p1; best_s2 = p2;
				memcpy( best_nib, n, 28 );
			}
		}
	}
	*s1 = best_s1; *s2 = best_s2;
	memcpy( nib, best_nib, 28 );
	return (uint8_t)((best_f << 4) | best_sh);
}

static int16_t *read_pcm( const char *path, size_t *frames )
{
	FILE *f = fopen( path, "rb" );
	if( !f ) { perror( path ); exit( 1 ); }
	fseek( f, 0, SEEK_END );
	const long n = ftell( f );
	fseek( f, 0, SEEK_SET );
	int16_t *p = (int16_t *) malloc( (size_t) n + 64 * 4 );
	if( fread( p, 1, (size_t) n, f ) != (size_t) n ) { perror( path ); exit( 1 ); }
	fclose( f );
	*frames = (size_t) n / 4;
	return p;
}

/* STR.BIN stream: 0x4000-byte blocks alternating left, right (1024 frames of
 * 16 bytes, 28 samples each).  Each channel's first frame is silent with
 * flags 0, its second has flags 6, all later ones flags 2; the stream ends
 * with one left and one right block of silent frames with flags 1. */
static int do_spu( const char *in, const char *out )
{
	size_t frames;
	int16_t *pcm = read_pcm( in, &frames );
	const size_t per_block = 1024 * 28;
	/* channel samples: one silent frame first */
	const size_t total = 28 + frames;
	const size_t blocks = (total + per_block - 1) / per_block;
	FILE *f = fopen( out, "wb" );
	if( !f ) { perror( out ); return 1; }
	int s1[2] = { 0, 0 }, s2[2] = { 0, 0 };
	int16_t unit[28];
	uint8_t nib[28], frame[16];
	for( size_t b = 0; b < blocks; ++b )
	{
		for( int ch = 0; ch < 2; ++ch )
		{
			for( size_t k = 0; k < 1024; ++k )
			{
				const size_t pos = b * per_block + k * 28;	/* in channel samples */
				for( int i = 0; i < 28; ++i )
				{
					const size_t s = pos + (size_t) i;
					unit[i] = (s >= 28 && s - 28 < frames)? pcm[(s - 28) * 2 + (size_t) ch]: 0;
				}
				memset( frame, 0, sizeof(frame) );
				if( pos == 0 )
				{
					/* silent lead frame, flags 0 */
				}
				else
				{
					frame[0] = encode_unit( unit, 5, &s1[ch], &s2[ch], nib );
					frame[1] = (uint8_t)(pos == 28? 6: 2);
					for( int i = 0; i < 14; ++i )
						frame[2 + i] = (uint8_t)(nib[2*i] | (nib[2*i + 1] << 4));
				}
				fwrite( frame, 1, 16, f );
			}
		}
	}
	memset( frame, 0, sizeof(frame) );
	frame[1] = 1;
	for( int k = 0; k < 2048; ++k )
		fwrite( frame, 1, 16, f );
	fclose( f );
	free( pcm );
	fprintf( stdout, "spu: %zu frames -> %zu block pairs + end pair\n", frames, blocks );
	return 0;
}

/* XA ADPCM, 4-bit stereo: 18 sound groups of 128 bytes per sector.  A group
 * holds 8 units of 28 samples (even units left, odd right): 16 header bytes
 * (params of units 0-3, the same again, units 4-7, the same again), then 28
 * words whose byte k packs unit 2k (low nibble) and unit 2k+1 (high). */
static int do_xa( const char *in, const char *out )
{
	size_t frames;
	int16_t *pcm = read_pcm( in, &frames );
	const size_t per_group = 4 * 28;	/* stereo sample frames per group */
	const size_t groups = (frames + per_group - 1) / per_group;
	const size_t sectors = (groups + 17) / 18;
	FILE *f = fopen( out, "wb" );
	if( !f ) { perror( out ); return 1; }
	int s1[2] = { 0, 0 }, s2[2] = { 0, 0 };
	int16_t unit[28];
	uint8_t nib[8][28], hdr[8], g[128];
	for( size_t gi = 0; gi < sectors * 18; ++gi )
	{
		for( int u = 0; u < 8; ++u )
		{
			const int ch = u & 1;
			const size_t base = gi * per_group + (size_t)(u >> 1) * 28;
			for( int i = 0; i < 28; ++i )
			{
				const size_t s = base + (size_t) i;
				unit[i] = s < frames? pcm[s * 2 + (size_t) ch]: 0;
			}
			hdr[u] = encode_unit( unit, 4, &s1[ch], &s2[ch], nib[u] );
		}
		for( int i = 0; i < 4; ++i )
		{
			g[i] = hdr[i]; g[4 + i] = hdr[i];
			g[8 + i] = hdr[4 + i]; g[12 + i] = hdr[4 + i];
		}
		for( int w = 0; w < 28; ++w )
			for( int k = 0; k < 4; ++k )
				g[16 + 4*w + k] = (uint8_t)(nib[2*k][w] | (nib[2*k + 1][w] << 4));
		fwrite( g, 1, 128, f );
	}
	fclose( f );
	free( pcm );
	fprintf( stdout, "xa: %zu frames -> %zu sectors\n", frames, sectors );
	return 0;
}

int main( int argc, char **argv )
{
	if( argc == 4 && !strcmp(argv[1], "spu") ) return do_spu( argv[2], argv[3] );
	if( argc == 4 && !strcmp(argv[1], "xa") ) return do_xa( argv[2], argv[3] );
	fprintf( stderr, "usage: psxaudio spu|xa IN.s16 OUT.bin\n" );
	return 2;
}
