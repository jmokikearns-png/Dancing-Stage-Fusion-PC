/* dlxbuild.c -- Dancing Stage Fusion Deluxe disc builder.
 *
 * Runs on the player's machine on their own disc image of Dancing Stage
 * Fusion (Europe, SLES-04163; raw .bin/.cue, MODE2/2352) and writes
 * "Dancing Stage Fusion Deluxe.bin/.cue" beside it: the same disc with this
 * project's patches (patches.h, make_patch.py).  It contains no Konami data;
 * the image it writes is the player's own and not to be shared.
 *
 * Patched sectors get their EDC and ECC recomputed (Mode 2 Form 1).
 * Before writing anything it checks every word it replaces, the free block
 * the payload goes into, and each sector's EDC.
 *
 * With a song pack (the PC version's dsf.extrasongs data: extrasongs.txt,
 * gfx.bin, media.bin) in a "songs" folder, it also adds those songs: their
 * sectors after the end of the disc, the menus' packs rebuilt with their
 * pictures in, the song tables and the code that answers for them in the
 * executable's free block (payload.c).
 *
 * usage: dlxbuild [DISC.cue | DISC.bin] [--songs DIR] [--out DIR]
 *   with no disc: the .cue in this folder or its "disc" folder. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

struct DlxWord { uint32_t va, stock, word; };
#include "patches.h"

#define RAW 2352
#define DATA 2048
#define OUT_NAME "Dancing Stage Fusion Deluxe"

#ifdef _WIN32
#define SEP "\\"
#else
#define SEP "/"
#endif

static int g_pause = 0;
static FILE *g_img;			/* the image being read or written */
static const char *g_out_path;		/* a partial image, removed on failure */
static char *g_out_dir_arg;

static void finish( int code )
{
	if( g_pause )
	{
		printf( "\nPress Enter to close.\n" );
		getchar();
	}
	exit( code );
}

static void fail( const char *fmt, const char *arg )
{
	printf( "\nERROR: " );
	printf( fmt, arg ? arg : "" );
	if( g_out_path )
	{
		fclose( g_img );
		remove( g_out_path );
	}
	printf( "\nNothing was written.\n" );
	finish( 1 );
}

/* ---- EDC / ECC (CD-ROM Mode 2 Form 1) ---- */
static uint8_t ecc_f[256], ecc_b[256];
static uint32_t edc_lut[256];

static void ecc_init( void )
{
	uint32_t i, j, e;
	for( i = 0; i < 256; ++i )
	{
		j = (i << 1) ^ (i & 0x80 ? 0x11D : 0);
		ecc_f[i] = (uint8_t)j;
		ecc_b[i ^ j] = (uint8_t)i;
		e = i;
		for( j = 0; j < 8; ++j )
			e = (e >> 1) ^ (e & 1 ? 0xD8018001u : 0);
		edc_lut[i] = e;
	}
}

static uint32_t edc( const uint8_t *p, size_t n )
{
	uint32_t e = 0;
	while( n-- )
		e = (e >> 8) ^ edc_lut[(e ^ *p++) & 0xFF];
	return e;
}

static void ecc_block( const uint8_t *src, uint32_t major_count, uint32_t minor_count,
	uint32_t major_mult, uint32_t minor_inc, uint8_t *dest )
{
	const uint32_t size = major_count * minor_count;
	uint32_t major, minor;
	for( major = 0; major < major_count; ++major )
	{
		uint32_t index = (major >> 1) * major_mult + (major & 1);
		uint8_t a = 0, b = 0;
		for( minor = 0; minor < minor_count; ++minor )
		{
			const uint8_t t = src[index];
			index += minor_inc;
			if( index >= size )
				index -= size;
			a ^= t;
			b ^= t;
			a = ecc_f[a];
		}
		a = ecc_b[ecc_f[a] ^ b];
		dest[major] = a;
		dest[major + major_count] = a ^ b;
	}
}

static void put32( uint8_t *p, uint32_t v )
{
	p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static uint32_t get32( const uint8_t *p )
{
	return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* EDC and ECC of a Mode 2 Form 1 sector (the header counts as zero for ECC). */
static void seal( uint8_t *s )
{
	uint8_t hdr[4];
	put32( s + 0x818, edc(s + 0x10, 0x808) );
	memcpy( hdr, s + 12, 4 );
	memset( s + 12, 0, 4 );
	ecc_block( s + 0xC, 86, 24, 2, 86, s + 0x81C );
	ecc_block( s + 0xC, 52, 43, 86, 88, s + 0x8C8 );
	memcpy( s + 12, hdr, 4 );
}

/* ---- the image ---- */
static void *xmalloc( size_t n )
{
	void *p = calloc( 1, n ? n : 1 );
	if( !p )
		fail( "out of memory.%s", "" );
	return p;
}

/* ---- the image ---- */
static uint32_t g_sectors;	/* sectors in the image now */

static void raw_read( uint32_t lba, uint8_t *s )
{
	char where[32];
	snprintf( where, sizeof where, "%u", (unsigned)lba );
	if( lba >= g_sectors || fseek(g_img, (long)lba * RAW, SEEK_SET) != 0 || fread(s, 1, RAW, g_img) != RAW )
		fail( "cannot read sector %s of the image.", where );
}

static void raw_write( uint32_t lba, const uint8_t *s )
{
	if( fseek(g_img, (long)lba * RAW, SEEK_SET) != 0 || fwrite(s, 1, RAW, g_img) != RAW )
		fail( "cannot write the new image (disk full?).%s", "" );
	if( lba >= g_sectors )
		g_sectors = lba + 1;
}

/* A Mode 2 Form 1 sector of the original, checked against its EDC/ECC. */
static void read_sector( uint32_t lba, uint8_t *s )
{
	static const uint8_t sync[12] = { 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0 };
	uint8_t check[RAW];
	char where[32];
	snprintf( where, sizeof where, "%u", (unsigned)lba );
	raw_read( lba, s );
	if( memcmp(s, sync, 12) != 0 || s[15] != 2 || (s[18] & 0x20) != 0 )
		fail( "sector %s is not a Mode 2 Form 1 data sector: is this a raw (2352-byte) image?", where );
	memcpy( check, s, RAW );
	seal( check );
	if( memcmp(check, s, RAW) != 0 )
		fail( "sector %s fails its error check: the image is damaged or not a clean dump.", where );
}

/* A file's bytes from its first sector. */
static uint8_t *read_file( uint32_t lba, uint32_t size )
{
	const uint32_t n = (size + DATA - 1) / DATA;
	uint8_t *buf = xmalloc( (size_t)n * DATA ), s[RAW];
	uint32_t i;
	for( i = 0; i < n; ++i )
	{
		read_sector( lba + i, s );
		memcpy( buf + (size_t)i * DATA, s + 24, DATA );
	}
	return buf;
}

/* Write back the sectors of a file that changed. */
static void write_file( uint32_t lba, const uint8_t *buf, const uint8_t *old, uint32_t size )
{
	const uint32_t n = (size + DATA - 1) / DATA;
	uint32_t i;
	for( i = 0; i < n; ++i )
	{
		uint8_t s[RAW];
		if( memcmp(buf + (size_t)i * DATA, old + (size_t)i * DATA, DATA) == 0 )
			continue;
		read_sector( lba + i, s );
		memcpy( s + 24, buf + (size_t)i * DATA, DATA );
		seal( s );
		raw_write( lba + i, s );
	}
}

static void bcd_msf( uint32_t lba, uint8_t *h )
{
	const uint32_t a = lba + 150;
	const uint32_t m = a / 4500, sec = (a / 75) % 60, f = a % 75;
	h[0] = (uint8_t)((m / 10) << 4 | m % 10);
	h[1] = (uint8_t)((sec / 10) << 4 | sec % 10);
	h[2] = (uint8_t)((f / 10) << 4 | f % 10);
	h[3] = 2;
}

/* A new sector after the end from its 2336 bytes (subheader on): Form 1
 * sealed with EDC and ECC, Form 2 with its EDC. */
static void append_sector( const uint8_t *m2336 )
{
	static const uint8_t sync[12] = { 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0 };
	uint8_t s[RAW];
	const uint32_t lba = g_sectors;
	memcpy( s, sync, 12 );
	bcd_msf( lba, s + 12 );
	memcpy( s + 16, m2336, 2336 );
	if( s[18] & 0x20 )
		put32( s + 0x92C, edc(s + 0x10, 0x91C) );
	else
		seal( s );
	raw_write( lba, s );
}

/* Data sectors (Form 1) for a blob; returns the first LBA. */
static uint32_t append_data( const uint8_t *data, uint32_t size )
{
	const uint32_t at = g_sectors;
	uint32_t i;
	for( i = 0; i * DATA < size || i == 0; ++i )
	{
		uint8_t m[2336];
		uint32_t n = size - i * DATA;
		memset( m, 0, sizeof m );
		m[2] = m[6] = 0x08;	/* submode: data */
		if( n > DATA )
			n = DATA;
		if( size )
			memcpy( m + 8, data + (size_t)i * DATA, n );
		append_sector( m );
	}
	return at;
}

/* ---- patches over a file in memory ---- */
static void patch_words( uint8_t *buf, uint32_t base, uint32_t size, const struct DlxWord *w, int nw, const char *what )
{
	int i;
	for( i = 0; i < nw; ++i )
	{
		char a[64];
		if( w[i].va < base || w[i].va + 4 > base + size || get32(buf + (w[i].va - base)) != w[i].stock )
		{
			snprintf( a, sizeof a, "%s at %08X", what, (unsigned)w[i].va );
			fail( "the %s is not as expected: this is not the PAL Dancing Stage Fusion "
			      "(SLES-04163), or it is already patched.", a );
		}
	}
	for( i = 0; i < nw; ++i )
		put32( buf + (w[i].va - base), w[i].word );
}

/* ---- Konami LZ (0x8002E6F0 decodes it): greedy with hash chains ---- */
struct Out { uint8_t *p; size_t n, cap; };

static void out_byte( struct Out *o, uint8_t b )
{
	if( o->n == o->cap )
	{
		o->cap = o->cap ? o->cap * 2 : 4096;
		o->p = realloc( o->p, o->cap );
		if( !o->p )
			fail( "out of memory.%s", "" );
	}
	o->p[o->n++] = b;
}

static uint8_t *lz_encode( const uint8_t *d, uint32_t n, uint32_t *out_n )
{
	struct Out o = { 0, 0, 0 };
	int32_t *head = xmalloc( sizeof(int32_t) * 65536 ), *prev = xmalloc( sizeof(int32_t) * (n + 1) );
	size_t flags_at = 0;
	int nflags = 8;
	uint32_t i = 0, lit_start = 0, lit_n = 0;
	memset( head, 0xFF, sizeof(int32_t) * 65536 );
#define LZ_FLAG(bit) do { if( nflags == 8 ) { flags_at = o.n; out_byte( &o, 0 ); nflags = 0; } \
	if( bit ) { o.p[flags_at] |= (uint8_t)(1 << nflags); } ++nflags; } while( 0 )
#define LZ_HASH(k) ((uint32_t)(d[k] | d[(k) + 1] << 8) ^ ((uint32_t)d[(k) + 2] << 5)) & 0xFFFF
#define LZ_REMEMBER(k) do { if( (k) + 3 <= n ) { const uint32_t h_ = LZ_HASH(k); prev[k] = head[h_]; head[h_] = (int32_t)(k); } } while( 0 )
#define LZ_FLUSH() do { while( lit_n >= 8 ) { uint32_t c_ = lit_n < 70 ? lit_n : 70, q_; LZ_FLAG(1); out_byte( &o, (uint8_t)(0xB8 + c_) ); \
		for( q_ = 0; q_ < c_; ++q_ ) { out_byte( &o, d[lit_start + q_] ); } lit_start += c_; lit_n -= c_; } \
	while( lit_n ) { LZ_FLAG(0); out_byte( &o, d[lit_start] ); ++lit_start; --lit_n; } } while( 0 )
	while( i < n )
	{
		uint32_t best_len = 0, best_dist = 0, dist, l;
		for( dist = 1; dist <= 16 && dist <= i; ++dist )
		{
			for( l = 0; l < 5 && i + l < n && d[i + l] == d[i + l - dist]; ++l )
				;
			if( l >= 2 && l > best_len ) { best_len = l; best_dist = dist; }
		}
		if( i + 3 <= n )
		{
			int32_t cand = head[LZ_HASH(i)];
			int steps = 0;
			while( cand >= 0 && steps++ < 64 )
			{
				dist = i - (uint32_t)cand;
				if( dist > 1023 )
					break;
				for( l = 0; l < 34 && i + l < n && d[i + l] == d[(uint32_t)cand + l]; ++l )
					;
				if( l >= 3 && l > best_len ) { best_len = l; best_dist = dist; if( l == 34 ) break; }
				cand = prev[cand];
			}
		}
		if( best_len >= 3 || (best_len == 2 && best_dist <= 16) )
		{
			const int is_short = best_len <= 5 && best_dist <= 16;
			LZ_FLUSH();
			LZ_FLAG(1);
			if( is_short )
				out_byte( &o, (uint8_t)(((best_len + 6) << 4) | (best_dist - 1)) );
			else
			{
				out_byte( &o, (uint8_t)(((best_len - 3) << 2) | (best_dist >> 8)) );
				out_byte( &o, (uint8_t)(best_dist & 0xFF) );
			}
			for( l = 0; l < best_len; ++l )
				LZ_REMEMBER( i + l );
			i += best_len;
			lit_start = i;
		}
		else
		{
			if( !lit_n )
				lit_start = i;
			++lit_n;
			LZ_REMEMBER( i );
			++i;
		}
	}
	LZ_FLUSH();
	LZ_FLAG(1);
	out_byte( &o, 0xFF );
	free( head );
	free( prev );
	*out_n = (uint32_t)o.n;
	return o.p;
}

/* The game's decoder, to check every picture round-trips. */
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

/* ---- the extra songs: the PC song pack (port/tools/extrasong) ---- */
#define MAX_EXTRA 12
#define SONGTAB 0x800AFE38u
#define STRTAB 0x800932B8u
#define GFXTAB 0x800AA154u
#define XATAB 0x800934E4u
#define MASTER 0x800A3EA0u
#define NUM_MASTER 0x252
#define TOC 0x80092B50u
#define NUM_TOC 235
#define MUSIC_ID_LIMIT 0x800296E0u
#define MAX_STREAM_ID 80

struct Song
{
	char title[96], artist[96];
	uint32_t record[11], stream[4], entries[5][2], gfx[2], xa[3], workout, unlock_id, unlock_mask;
	int have;	/* bit per field read */
};
struct Blob { uint32_t offset, size; };
struct TocLine { uint32_t entry, size, lba; };

static struct Song g_songs[MAX_EXTRA];
static int g_num_songs;
static struct Blob *g_blobs;
static int g_num_blobs;
static struct TocLine g_toc[64];
static int g_num_toc;
static uint32_t g_media_base, g_media_sectors;
static uint8_t *g_gfx;
static uint32_t g_gfx_size;
static char g_pack_dir[1024];

static int hex_words( const char *s, uint32_t *w, int max )
{
	int n = 0;
	while( n < max )
	{
		char *end;
		const unsigned long v = strtoul( s, &end, 16 );
		if( end == s )
			break;
		w[n++] = (uint32_t)v;
		s = end;
	}
	return n;
}

static uint8_t *slurp( const char *path, uint32_t *size )
{
	FILE *f = fopen( path, "rb" );
	long n;
	uint8_t *b;
	if( !f )
		return NULL;
	fseek( f, 0, SEEK_END );
	n = ftell( f );
	fseek( f, 0, SEEK_SET );
	b = xmalloc( (size_t)n );
	if( n > 0 && fread(b, 1, (size_t)n, f) != (size_t)n )
		fail( "cannot read %s", path );
	fclose( f );
	*size = (uint32_t)n;
	return b;
}

static void pack_path( char *out, size_t n, const char *name )
{
	snprintf( out, n, "%s%s", g_pack_dir, name );
}

/* extrasongs.txt (dsfcore_extrasong.cpp's format). */
static void read_song_list( void )
{
	char path[1200], line[1024];
	FILE *f;
	struct Song s;
	int in_song = 0, complete = 1, max_blobs = 0;
	pack_path( path, sizeof path, "extrasongs.txt" );
	if( !(f = fopen(path, "r")) )
		fail( "cannot open %s", path );
	memset( &s, 0, sizeof s );
	while( fgets(line, sizeof line, f) )
	{
		char *nl = strpbrk( line, "\r\n" ), *sp, *rest;
		uint32_t w[16];
		int nw;
		if( nl ) *nl = 0;
		sp = strchr( line, ' ' );
		rest = sp ? sp + 1 : line + strlen( line );
		if( sp ) *sp = 0;
		nw = hex_words( rest, w, 16 );
		if( !strcmp(line, "song") ) { memset( &s, 0, sizeof s ); snprintf( s.title, sizeof s.title, "%s", rest ); in_song = 1; complete = 1; }
		else if( !in_song && !strcmp(line, "media") && nw == 2 ) { g_media_base = w[0]; g_media_sectors = w[1]; }
		else if( !in_song && !strcmp(line, "blob") && nw == 3 && (int)w[0] == g_num_blobs )
		{
			if( g_num_blobs == max_blobs )
			{
				max_blobs = max_blobs ? 2 * max_blobs : 64;
				g_blobs = realloc( g_blobs, sizeof(struct Blob) * (size_t)max_blobs );
			}
			g_blobs[g_num_blobs].offset = w[1];
			g_blobs[g_num_blobs++].size = w[2];
		}
		else if( !in_song && !strcmp(line, "toc") && nw == 3 && g_num_toc < 64 )
		{
			g_toc[g_num_toc].entry = w[0]; g_toc[g_num_toc].size = w[1]; g_toc[g_num_toc++].lba = w[2];
		}
		else if( !in_song )
			continue;
		else if( !strcmp(line, "artist") ) snprintf( s.artist, sizeof s.artist, "%s", rest );
		else if( !strcmp(line, "record") && nw == 11 ) { memcpy( s.record, w, sizeof s.record ); s.have |= 1; }
		else if( !strcmp(line, "stream") && nw == 4 ) { memcpy( s.stream, w, sizeof s.stream ); s.have |= 2; }
		else if( !strcmp(line, "entries") && nw == 10 ) { memcpy( s.entries, w, sizeof s.entries ); s.have |= 4; }
		else if( !strcmp(line, "gfx") && nw == 2 ) { memcpy( s.gfx, w, sizeof s.gfx ); s.have |= 8; }
		else if( !strcmp(line, "xa") && nw == 3 ) { memcpy( s.xa, w, sizeof s.xa ); s.have |= 16; }
		else if( !strcmp(line, "workout") && nw == 1 ) s.workout = w[0];
		else if( !strcmp(line, "unlock") && nw == 2 ) { s.unlock_id = w[0]; s.unlock_mask = w[1]; }
		else if( !strcmp(line, "end") )
		{
			if( complete && s.have == 31 && g_num_songs < MAX_EXTRA && s.record[2] >= 1 && s.record[2] <= MAX_STREAM_ID &&
			    s.record[3] >= 24 && s.record[3] < 24 + 38 )
				g_songs[g_num_songs++] = s;
			else
				printf( "  skipped (incomplete): %s\n", s.title );
			in_song = 0;
		}
		else if( line[0] != '#' )
			complete = 0;
	}
	fclose( f );
	if( !g_num_songs || !g_media_sectors )
		fail( "%s lists no songs.", path );
}

/* A picture marker (16 bytes, build_extrasong.py's Blobs.marker) -> blob index. */
static int marker_blob( const uint8_t *p, uint32_t room )
{
	uint32_t k;
	if( room < 16 || memcmp(p, "DSFX", 4) != 0 )
		return -1;
	k = get32( p + 4 );
	if( get32(p + 8) != (k ^ 0xFFFFFFFFu) || (int)k >= g_num_blobs || get32(p + 12) != g_blobs[k].size )
		return -1;
	return (int)k;
}

static uint8_t *g_lz_cache[1024];
static uint32_t g_lz_size[1024];

/* A picture as the game's packs hold it: LZ-compressed, checked. */
static const uint8_t *packed_blob( int k, uint32_t *n )
{
	if( k >= 1024 )
		fail( "too many pictures in the song pack.%s", "" );
	if( !g_lz_cache[k] )
	{
		const struct Blob *b = &g_blobs[k];
		uint8_t *check;
		if( (uint64_t)b->offset + b->size > g_gfx_size )
			fail( "gfx.bin is shorter than extrasongs.txt says.%s", "" );
		g_lz_cache[k] = lz_encode( g_gfx + b->offset, b->size, &g_lz_size[k] );
		check = xmalloc( b->size + 16 );
		if( lz_decode(g_lz_cache[k], g_lz_size[k], check, b->size + 16) != b->size ||
		    memcmp(check, g_gfx + b->offset, b->size) != 0 )
			fail( "internal error: a picture does not compress back.%s", "" );
		free( check );
	}
	*n = g_lz_size[k];
	return g_lz_cache[k];
}

/* An item with markers inside: RECORDS' song table (slot 1: {id, offA, offB}
 * records to a zero one, then the pictures the offsets point to, relative
 * to the item) -- each picture segment that is a marker becomes the picture. */
static void rebuild_record_table( const uint8_t *it, uint32_t n, struct Out *o )
{
	uint32_t m = 0, bounds[128], nb = 0, i, j, map_old[128], map_new[128], start = (uint32_t)o->n;
	while( 12 * (m + 1) <= n && (get32(it + 12 * m) | get32(it + 12 * m + 4) | get32(it + 12 * m + 8)) )
		++m;
	if( 12 * (m + 1) > n || 2 * m + 1 > 128 )
		fail( "a RECORDS pack in the song pack is not as expected.%s", "" );
	for( i = 0; i < m; ++i )
		for( j = 1; j <= 2; ++j )
		{
			const uint32_t off = get32( it + 12 * i + 4 * j );
			uint32_t q, have = 0;
			if( off < 12 * (m + 1) || off >= n )
				fail( "a RECORDS pack in the song pack is not as expected.%s", "" );
			for( q = 0; q < nb; ++q ) have |= bounds[q] == off;
			if( !have ) bounds[nb++] = off;
		}
	for( i = 0; i < nb; ++i )	/* sort */
		for( j = i + 1; j < nb; ++j )
			if( bounds[j] < bounds[i] ) { uint32_t t = bounds[i]; bounds[i] = bounds[j]; bounds[j] = t; }
	/* the table, then everything before the first picture, then each segment */
	for( i = 0; i < bounds[0]; ++i )
		out_byte( o, it[i] );
	for( i = 0; i < nb; ++i )
	{
		const uint32_t s = bounds[i], e = i + 1 < nb ? bounds[i + 1] : n;
		const int k = marker_blob( it + s, e - s );
		map_old[i] = s;
		map_new[i] = (uint32_t)o->n - start;
		if( k >= 0 )
		{
			uint32_t pn, q;
			const uint8_t *p = packed_blob( k, &pn );
			for( q = 0; q < pn; ++q ) out_byte( o, p[q] );
			while( (o->n - start) & 3 ) out_byte( o, 0 );
		}
		else
			for( j = s; j < e; ++j ) out_byte( o, it[j] );
	}
	for( i = 0; i < m; ++i )
		for( j = 1; j <= 2; ++j )
		{
			const uint32_t off = get32( it + 12 * i + 4 * j );
			uint32_t q;
			for( q = 0; q < nb; ++q )
				if( map_old[q] == off )
					put32( o->p + start + 12 * i + 4 * j, map_new[q] );
		}
}

/* A pack (u32 item offsets, 0 = empty; items 4-aligned) with its marker
 * items and markers replaced by the pictures. */
static uint8_t *rebuild_pack( const uint8_t *b, uint32_t size, uint32_t *out_n, int *markers )
{
	struct Out o = { 0, 0, 0 };
	uint32_t count, i, *offs;
	*markers = 0;
	if( size < 4 || (get32(b) & 3) || get32(b) / 4 == 0 || get32(b) > size )
		fail( "a pack in the song pack is not as expected.%s", "" );
	count = get32( b ) / 4;
	offs = xmalloc( 4 * count );
	for( i = 0; i < count; ++i )
		offs[i] = get32( b + 4 * i );
	for( i = 0; i < 4 * count; ++i )
		out_byte( &o, 0 );
	for( i = 0; i < count; ++i )
	{
		uint32_t s = offs[i], e = size, j, q;
		const uint8_t *it;
		int k;
		if( !s )
			continue;
		if( s >= size )
			fail( "a pack in the song pack is not as expected.%s", "" );
		for( j = 0; j < count; ++j )
			if( offs[j] > s && offs[j] < e )
				e = offs[j];
		it = b + s;
		put32( o.p + 4 * i, (uint32_t)o.n );
		if( (k = marker_blob(it, e - s)) >= 0 )
		{
			uint32_t pn;
			const uint8_t *p = packed_blob( k, &pn );
			for( q = 0; q < pn; ++q ) out_byte( &o, p[q] );
			++*markers;
		}
		else
		{
			int inner = 0;
			for( q = 0; q + 16 <= e - s; q += 4 )
				inner |= marker_blob( it + q, e - s - q ) >= 0;
			if( inner )
			{
				rebuild_record_table( it, e - s, &o );
				++*markers;
			}
			else
				for( q = 0; q < e - s; ++q ) out_byte( &o, it[q] );
		}
		while( o.n & 3 ) out_byte( &o, 0 );
	}
	free( offs );
	*out_n = (uint32_t)o.n;
	return o.p;
}

/* Where each rebuilt pack loads, and its room (the game's buffers). */
static uint32_t pack_room( uint32_t entry )
{
	if( entry >= 201 && entry <= 205 ) return 0x801A4000u - DLX_SELECT_BASE;	/* music select */
	if( entry >= 28 && entry <= 32 ) return 0x80126000u - 0x80100000u;		/* RECORDS */
	if( entry == 181 ) return 0x80198000u - 0x3400u - 0x80174000u;	/* workout results: up to its decode buffer, with a margin */
	if( entry >= 206 && entry <= 210 ) return 0x8018C000u - 0x80174000u;	/* results: below the dancer's decode area */
	return 0;	/* not one of the menus' packs: as it is in the song pack */
}

static uint32_t hi16( uint32_t a ) { return (a + 0x8000u) >> 16; }

static void patch_pair( uint8_t *exe, uint32_t lui, uint32_t lo, uint32_t base )
{
	put32( exe + (lui - DLX_EXE_BASE), (get32(exe + (lui - DLX_EXE_BASE)) & 0xFFFF0000u) | hi16(base) );
	put32( exe + (lo - DLX_EXE_BASE), (get32(exe + (lo - DLX_EXE_BASE)) & 0xFFFF0000u) | (base & 0xFFFF) );
}

static void patch_count( uint8_t *exe, uint32_t va, uint32_t n )
{
	put32( exe + (va - DLX_EXE_BASE), (get32(exe + (va - DLX_EXE_BASE)) & 0xFFFF0000u) | n );
}

static uint32_t rd( const uint8_t *exe, uint32_t va ) { return get32( exe + (va - DLX_EXE_BASE) ); }
static void wr( uint8_t *exe, uint32_t va, uint32_t v ) { put32( exe + (va - DLX_EXE_BASE), v ); }

/* The extra songs: their sectors after the end, the packs rebuilt with
 * their pictures, the tables (dsfcore_extrasong.cpp's install(), done once
 * here), and the code that answers for them. */
static void add_songs( uint8_t *exe, uint32_t exe_size, uint8_t *rec_ovl, uint32_t rec_size )
{
	char path[1200];
	FILE *media;
	uint32_t i, k, delta;
	uint8_t *buf;
	const uint32_t tables = DLX_SYM_TABLES;
	const uint32_t songs = tables + 0x000, streams = tables + 0x580, gfx = tables + 0xA80;
	const uint32_t groups = tables + 0xC00, entries = tables + 0xD20, xa = tables + 0xF00, strings = tables + 0x10D0;
	uint32_t n, max_id = 0x31, blank = 0;
	(void)exe_size;

	read_song_list();
	n = 20 + (uint32_t)g_num_songs;
	pack_path( path, sizeof path, "gfx.bin" );
	if( !(g_gfx = slurp(path, &g_gfx_size)) )
		fail( "cannot open %s", path );
	pack_path( path, sizeof path, "media.bin" );
	if( !(media = fopen(path, "rb")) )
		fail( "cannot open %s", path );
	fseek( media, 0, SEEK_END );
	if( ftell(media) != (long)g_media_sectors * 2336 )
		fail( "media.bin is not the size extrasongs.txt says: an incomplete song pack?%s", "" );

	/* the media: all of it, as it is, after the end of the disc */
	delta = g_sectors - g_media_base;
	printf( "Adding:     %d songs (%u sectors of music and charts)\n", g_num_songs, (unsigned)g_media_sectors );
	buf = xmalloc( 2336 );
	fseek( media, 0, SEEK_SET );
	for( i = 0; i < g_media_sectors; ++i )
	{
		if( fread(buf, 1, 2336, media) != 2336 )
			fail( "cannot read media.bin%s", "" );
		append_sector( buf );
	}
	free( buf );
#define MOVED(l) ((uint32_t)((l) + delta))

	/* the menus' packs with the pictures in them */
	for( i = 0; i < (uint32_t)g_num_toc; ++i )
	{
		struct TocLine *t = &g_toc[i];
		const uint32_t room = pack_room( t->entry );
		uint8_t *pack, *raw;
		uint32_t newn, s, sectors = (t->size + DATA - 1) / DATA;
		int markers;
		if( t->entry >= NUM_TOC )
			continue;
		if( t->lba < g_media_base || t->lba + sectors > g_media_base + g_media_sectors )
			fail( "a pack in extrasongs.txt is outside media.bin.%s", "" );
		pack = xmalloc( (size_t)sectors * DATA );
		raw = xmalloc( 2336 );
		for( s = 0; s < sectors; ++s )
		{
			fseek( media, (long)(t->lba - g_media_base + s) * 2336, SEEK_SET );
			if( fread(raw, 1, 2336, media) != 2336 )
				fail( "cannot read media.bin%s", "" );
			memcpy( pack + (size_t)s * DATA, raw + 8, DATA );
		}
		free( raw );
		if( !room )
		{
			t->lba = MOVED( t->lba );
			free( pack );
			continue;
		}
		{
			uint8_t *np = rebuild_pack( pack, t->size, &newn, &markers );
			char e[64];
			snprintf( e, sizeof e, "%u (%u bytes, room %u)", (unsigned)t->entry, (unsigned)newn, (unsigned)room );
			if( newn > room )
				fail( "pack %s does not fit with the pictures in.", e );
			t->size = newn;
			t->lba = append_data( np, newn );
			free( np );
		}
		free( pack );
	}
	fclose( media );

	/* the tables */
	memset( exe + (tables - DLX_EXE_BASE), 0, 0x1580 );
	memcpy( exe + (songs - DLX_EXE_BASE), exe + (SONGTAB - DLX_EXE_BASE), 20 * 44 );
	memcpy( exe + (streams - DLX_EXE_BASE), exe + (STRTAB - DLX_EXE_BASE), 23 * 16 );
	memcpy( exe + (gfx - DLX_EXE_BASE), exe + (GFXTAB - DLX_EXE_BASE), 20 * 12 );
	memcpy( exe + (xa - DLX_EXE_BASE), exe + (XATAB - DLX_EXE_BASE), 26 * 12 );
	wr( exe, DLX_SYM_NUM_EXTRA, (uint32_t)g_num_songs );
	for( k = 0; k < (uint32_t)g_num_songs; ++k )
	{
		const struct Song *s = &g_songs[k];
		const uint32_t idx = 20 + k, rec = songs + idx * 44, g = groups + k * 24, e = entries + k * 40;
		const uint32_t st = streams + (s->record[2] - 1) * 16, gf = gfx + idx * 12, xe = xa + (s->record[3] - 24) * 12;
		const uint32_t ex = DLX_SYM_EXTRA + k * 20, str = strings + k * 96;
		int l;
		for( l = 0; l < 11; ++l )
			wr( exe, rec + 4 * (uint32_t)l, s->record[l] );
		for( l = 0; l < 5; ++l )
		{
			wr( exe, e + 8 * (uint32_t)l, s->entries[l][0] );
			wr( exe, e + 8 * (uint32_t)l + 4, MOVED(s->entries[l][1]) );
			wr( exe, g + 4 + 4 * (uint32_t)l, e + 8 * (uint32_t)l );
		}
		wr( exe, g, e );	/* JA: the English data, as the stock songs */
		wr( exe, rec + 36, g );
		wr( exe, st, MOVED(s->stream[0]) );
		for( l = 1; l < 4; ++l )
			wr( exe, st + 4 * (uint32_t)l, s->stream[l] );
		wr( exe, gf, s->record[0] );
		wr( exe, gf + 4, s->gfx[0] );
		wr( exe, gf + 8, s->gfx[1] );
		wr( exe, xe, (s->xa[0] & 0xFF000000u) | (MOVED(s->xa[0] & 0xFFFFFFu) & 0xFFFFFFu) );
		wr( exe, xe + 4, (s->xa[1] & 0xFF000000u) | (MOVED(s->xa[1] & 0xFFFFFFu) & 0xFFFFFFu) );
		wr( exe, xe + 8, s->xa[2] );
		wr( exe, ex, s->record[0] );
		wr( exe, ex + 4, s->record[1] );
		wr( exe, ex + 8, s->record[2] );
		wr( exe, ex + 12, s->unlock_id );
		wr( exe, ex + 16, s->unlock_mask );
		if( s->record[2] > max_id ) max_id = s->record[2];
		if( s->record[3] > max_id ) max_id = s->record[3];
		/* a blank master-table slot (no short name) takes its names */
		while( blank < NUM_MASTER && rd(exe, MASTER + blank * 20 + 4) != 0 && rd(exe, MASTER + blank * 20) != s->record[0] )
			++blank;
		if( blank < NUM_MASTER )
		{
			const uint32_t m = MASTER + blank * 20;
			uint8_t *p = exe + (str - DLX_EXE_BASE);
			int c;
			for( c = 0; c < 4 && s->title[c]; ++c )
				p[c] = (uint8_t)(s->title[c] >= 'A' && s->title[c] <= 'Z' ? s->title[c] + 32 : s->title[c]);
			snprintf( (char *)p + 8, 56, "%s", s->title );
			snprintf( (char *)p + 64, 32, "%s", s->artist );
			wr( exe, m, s->record[0] );
			wr( exe, m + 4, str );
			wr( exe, m + 8, str + 8 );
			wr( exe, m + 12, 0 );
			wr( exe, m + 16, str + 64 );
			++blank;
		}
	}
	for( i = 0; i < (uint32_t)g_num_toc; ++i )
		if( g_toc[i].entry < NUM_TOC )
		{
			wr( exe, TOC + 8 * g_toc[i].entry, g_toc[i].size );
			wr( exe, TOC + 8 * g_toc[i].entry + 4, g_toc[i].lba );
		}

	/* the code that uses each table: bases and counts */
	{
		static const uint32_t song_pairs[][2] = {
			{ 0x800759E0, 0x800759E4 }, { 0x80075B18, 0x80075B1C }, { 0x80075B4C, 0x80075B54 },
			{ 0x80075B58, 0x80075B60 }, { 0x80075B64, 0x80075B6C }, { 0x80075B8C, 0x80075B90 },
			{ 0x80075BA8, 0x80075BAC }, { 0x80075BE8, 0x80075BEC }, { 0x80075E78, 0x80075E7C } };
		static const uint32_t song_counts[] = { 0x80075A30, 0x80075A74, 0x80075B38, 0x80075B70, 0x80075BC8, 0x80075BE0, 0x80075BFC, 0x80075E94 };
		static const uint32_t gfx_pairs[][2] = { { 0x80050BCC, 0x80050BD0 }, { 0x80050BEC, 0x80050BF0 } };
		static const uint32_t xa_pairs[][2] = { { 0x80028E50, 0x80028E58 }, { 0x8002E26C, 0x8002E27C } };
		/* stock words that say this is the code these addresses are for */
		static const uint32_t check[][2] = {
			{ 0x80075BDC, 0x03E00008 }, { 0x80075BE0, 0x24020014 }, { 0x8002CA48, 0x244232A8 },
			{ 0x80028E58, 0x24C634E4 }, { 0x8002E27C, 0x248434E4 }, { 0x80050C24, 0x2CA20014 },
			{ MUSIC_ID_LIMIT, 0x2C420031 } };
		for( i = 0; i < sizeof check / sizeof *check; ++i )
			if( rd(exe, check[i][0]) != check[i][1] )
				fail( "the game program is not as expected (song tables).%s", "" );
		for( i = 0; i < sizeof song_pairs / sizeof *song_pairs; ++i ) patch_pair( exe, song_pairs[i][0], song_pairs[i][1], songs );
		for( i = 0; i < sizeof song_counts / sizeof *song_counts; ++i ) patch_count( exe, song_counts[i], n );
		for( i = 0; i < sizeof gfx_pairs / sizeof *gfx_pairs; ++i ) patch_pair( exe, gfx_pairs[i][0], gfx_pairs[i][1], gfx );
		patch_count( exe, 0x80050C24, n );
		patch_pair( exe, 0x8002CA44, 0x8002CA48, streams - 16 );	/* 1-based */
		for( i = 0; i < sizeof xa_pairs / sizeof *xa_pairs; ++i ) patch_pair( exe, xa_pairs[i][0], xa_pairs[i][1], xa );
		patch_count( exe, MUSIC_ID_LIMIT, max_id );
	}
	patch_words( exe, DLX_EXE_BASE, exe_size, dlx_song_exe_words, sizeof dlx_song_exe_words / sizeof *dlx_song_exe_words, "game program" );
	patch_words( rec_ovl, DLX_OVL_BASE, rec_size, dlx_song_rec_words, sizeof dlx_song_rec_words / sizeof *dlx_song_rec_words, "RECORDS code" );
	for( k = 0; k < (uint32_t)g_num_songs; ++k )
		printf( "  %2u  %s - %s\n", (unsigned)(21 + k), g_songs[k].title, g_songs[k].artist );
}
static char *dir_of( const char *path )
{
	char *d = strdup( path ), *p = d + strlen( d );
	while( p > d && p[-1] != '/' && p[-1] != '\\' )
		--p;
	*p = 0;
	return d;
}

/* The .bin a single-track .cue names. */
static char *bin_from_cue( const char *cue )
{
	FILE *f = fopen( cue, "r" );
	char line[1024], *bin = NULL;
	int tracks = 0;
	if( !f )
		fail( "cannot open %s", cue );
	while( fgets(line, sizeof line, f) )
	{
		char *q1, *q2;
		if( strstr(line, "TRACK") )
		{
			++tracks;
			if( !strstr(line, "MODE2/2352") )
				fail( "the .cue's track is not MODE2/2352: %s", line );
		}
		if( !strstr(line, "FILE") || !(q1 = strchr(line, '"')) || !(q2 = strchr(q1 + 1, '"')) )
			continue;
		*q2 = 0;
		{
			char *d = dir_of( cue );
			bin = malloc( strlen(d) + strlen(q1 + 1) + 1 );
			strcpy( bin, d );
			strcat( bin, q1 + 1 );
			free( d );
		}
	}
	fclose( f );
	if( !bin || tracks != 1 )
		fail( "%s is not a single-track .cue for a .bin.", cue );
	return bin;
}

static int ends_with( const char *s, const char *e )
{
	size_t a = strlen( s ), b = strlen( e );
	if( a < b )
		return 0;
	for( ; b; --a, --b )
	{
		char x = s[a - 1], y = e[b - 1];
		if( x >= 'A' && x <= 'Z' ) x += 32;
		if( x != y )
			return 0;
	}
	return 1;
}

#ifdef _WIN32
#include <windows.h>
static char *find_cue( const char *dir )
{
	WIN32_FIND_DATAA fd;
	char pat[1024];
	HANDLE h;
	snprintf( pat, sizeof pat, "%s*.cue", dir );
	h = FindFirstFileA( pat, &fd );
	while( h != INVALID_HANDLE_VALUE )
	{
		if( strstr(fd.cFileName, OUT_NAME) == NULL )
		{
			char *r = malloc( strlen(dir) + strlen(fd.cFileName) + 1 );
			strcpy( r, dir );
			strcat( r, fd.cFileName );
			FindClose( h );
			return r;
		}
		if( !FindNextFileA(h, &fd) )
		{
			FindClose( h );
			break;
		}
	}
	return NULL;
}
#else
#include <dirent.h>
static char *find_cue( const char *dir )
{
	DIR *d = opendir( *dir ? dir : "." );
	struct dirent *e;
	char *r = NULL;
	if( !d )
		return NULL;
	while( !r && (e = readdir(d)) != NULL )
		if( ends_with(e->d_name, ".cue") && strstr(e->d_name, OUT_NAME) == NULL )
		{
			r = malloc( strlen(dir) + strlen(e->d_name) + 1 );
			strcpy( r, dir );
			strcat( r, e->d_name );
		}
	closedir( d );
	return r;
}
#endif

static int dir_has( const char *dir, const char *name )
{
	char p[1200];
	FILE *f;
	snprintf( p, sizeof p, "%s%s", dir, name );
	if( !(f = fopen(p, "rb")) )
		return 0;
	fclose( f );
	return 1;
}

/* The song pack: a "songs" folder beside the program (or given), holding
 * extrasongs.txt, gfx.bin and media.bin (or a "data" folder with them, as
 * in the PC version's mods\installed\dsf.extrasongs\1.0.0). */
static int find_songs( const char *given )
{
	static const char *const tries[] = { "songs" SEP, "songs" SEP "data" SEP, "" };
	char d[1024];
	int i;
	for( i = 0; i < 3; ++i )
	{
		if( given )
			snprintf( d, sizeof d, "%s%s%s", given, ends_with(given, SEP) ? "" : SEP, i < 2 ? tries[i] + 6 : "" );
		else if( i == 2 )
			break;
		else
			snprintf( d, sizeof d, "%s", tries[i] );
		if( dir_has(d, "extrasongs.txt") )
		{
			snprintf( g_pack_dir, sizeof g_pack_dir, "%s", d );
			return 1;
		}
	}
	if( given )
		fail( "no extrasongs.txt in %s", given );
	return 0;
}

int main( int argc, char **argv )
{
	char *bin, out_bin[1200], out_cue[1200], *out_dir, *disc_arg = NULL, *songs_arg = NULL;
	static char buf[1 << 20];
	FILE *in, *cue;
	size_t nr;
	long long total = 0, size;
	uint8_t *exe, *exe_old, *ovl, *ovl_old, *rec, *rec_old, s0[RAW];
	uint32_t exe_size, ovl_size, rec_size, va;
	int i, songs;

	printf( "Dancing Stage Fusion Deluxe - disc builder (test build 2)\n"
	        "Makes a Deluxe disc image from YOUR Dancing Stage Fusion (Europe) image.\n\n" );
	ecc_init();
	for( i = 1; i < argc; ++i )
	{
		if( !strcmp(argv[i], "--songs") && i + 1 < argc ) songs_arg = argv[++i];
		else if( !strcmp(argv[i], "--out") && i + 1 < argc ) g_out_dir_arg = argv[++i];
		else disc_arg = argv[i];
	}
	g_pause = argc == 1 || (argc == 2 && disc_arg);	/* double-clicked, or a disc dropped on it */
	if( !disc_arg )
	{
		disc_arg = find_cue( "" );
		if( !disc_arg )
			disc_arg = find_cue( "disc" SEP );
		if( !disc_arg )
			fail( "no disc image found. Put your Dancing Stage Fusion .bin and .cue in the \"disc\" folder "
			      "(or next to this program), or drop the .cue onto it.%s", "" );
	}
	songs = find_songs( songs_arg );
	bin = ends_with( disc_arg, ".cue" ) ? bin_from_cue( disc_arg ) : disc_arg;
	out_dir = g_out_dir_arg ? g_out_dir_arg : dir_of( disc_arg );
	snprintf( out_bin, sizeof out_bin, "%s%s%s.bin", out_dir, *out_dir && !ends_with(out_dir, SEP) && !ends_with(out_dir, "/") ? SEP : "", OUT_NAME );
	snprintf( out_cue, sizeof out_cue, "%s%s%s.cue", out_dir, *out_dir && !ends_with(out_dir, SEP) && !ends_with(out_dir, "/") ? SEP : "", OUT_NAME );
	printf( "Your disc:  %s\n", bin );
	printf( "Songs:      %s\n", songs ? g_pack_dir : "none (no \"songs\" folder): TYPE3 colours only" );

	/* Read and patch everything in memory first: nothing is written until it all checks out. */
	if( !(g_img = fopen(bin, "rb")) )
		fail( "cannot open %s", bin );
	fseek( g_img, 0, SEEK_END );
	size = ftell( g_img );
	if( size <= 0 || size % RAW != 0 )
		fail( "%s is not a raw 2352-byte-sector image.", bin );
	g_sectors = (uint32_t)(size / RAW);
	read_sector( DLX_EXE_LBA, s0 );
	if( memcmp(s0 + 24, "PS-X EXE", 8) != 0 || get32(s0 + 24 + 0x18) != 0x8001A000u )
		fail( "the game program is not where it should be: this is not the PAL Dancing Stage Fusion.%s", "" );
	exe_size = 0x800 + get32( s0 + 24 + 0x1C );
	exe = read_file( DLX_EXE_LBA, exe_size );
	exe_old = xmalloc( exe_size ); memcpy( exe_old, exe, exe_size );
	ovl_size = get32( exe + (TOC + 8 * 227 - DLX_EXE_BASE) );
	rec_size = get32( exe + (TOC + 8 * 228 - DLX_EXE_BASE) );
	if( get32(exe + (TOC + 8 * 227 + 4 - DLX_EXE_BASE)) != DLX_OVL_LBA || get32(exe + (TOC + 8 * 228 + 4 - DLX_EXE_BASE)) != DLX_REC_LBA ||
	    ovl_size > 0x10000 || rec_size > 0x10000 )
		fail( "the game's table of contents is not as expected: unexpected version, or already patched.%s", "" );
	ovl = read_file( DLX_OVL_LBA, ovl_size );
	ovl_old = xmalloc( ovl_size ); memcpy( ovl_old, ovl, ovl_size );
	rec = read_file( DLX_REC_LBA, rec_size );
	rec_old = xmalloc( rec_size ); memcpy( rec_old, rec, rec_size );

	/* TYPE3 and the resident code */
	patch_words( exe, DLX_EXE_BASE, exe_size, dlx_exe_words, sizeof dlx_exe_words / sizeof *dlx_exe_words, "game program" );
	patch_words( ovl, DLX_OVL_BASE, ovl_size, dlx_ovl_words, sizeof dlx_ovl_words / sizeof *dlx_ovl_words, "options code" );
	for( va = DLX_BLOCK; va < DLX_BLOCK_END; ++va )
		if( exe[va - DLX_EXE_BASE] != 0 )
			fail( "the free block in the game program is not free: unexpected version of the game.%s", "" );
	memcpy( exe + (DLX_BLOCK - DLX_EXE_BASE), dlx_payload, sizeof dlx_payload );
	if( songs )
		read_song_list();
	printf( "Checked:    the PAL game (SLES-04163), unpatched.\n" );

	/* Copy, then add to and patch the copy. */
	printf( "Writing:    %s\n", out_bin );
	in = g_img;
	if( !(g_img = fopen(out_bin, "wb")) )
		fail( "cannot create %s", out_bin );
	g_out_path = out_bin;
	fseek( in, 0, SEEK_SET );
	while( (nr = fread(buf, 1, sizeof buf, in)) > 0 )
	{
		if( fwrite(buf, 1, nr, g_img) != nr )
			fail( "cannot write %s (disk full?)", out_bin );
		total += (long long)nr;
		if( total % (64 << 20) < (1 << 20) )
			printf( "  %lld MB\r", total >> 20 ), fflush( stdout );
	}
	fclose( in );
	fclose( g_img );
	if( total != size )
		fail( "could not copy all of %s", bin );
	if( !(g_img = fopen(out_bin, "r+b")) )
		fail( "cannot reopen %s", out_bin );
	if( songs )
	{
		g_num_songs = g_num_blobs = g_num_toc = 0;	/* read again by add_songs */
		add_songs( exe, exe_size, rec, rec_size );
	}
	write_file( DLX_EXE_LBA, exe, exe_old, exe_size );
	write_file( DLX_OVL_LBA, ovl, ovl_old, ovl_size );
	write_file( DLX_REC_LBA, rec, rec_old, rec_size );
	fclose( g_img );
	g_out_path = NULL;

	if( !(cue = fopen(out_cue, "w")) )
		fail( "cannot create %s", out_cue );
	fprintf( cue, "FILE \"%s.bin\" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n", OUT_NAME );
	fclose( cue );
	printf( "Done:       %s (%u MB)\n\nOpen the .cue in DuckStation. For your own use only: do not share the image.\n",
	        out_cue, (unsigned)((long long)g_sectors * RAW >> 20) );
	finish( 0 );
	return 0;
}
