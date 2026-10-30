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
 * usage: dlxbuild [DISC.cue | DISC.bin] [OUT_DIR]
 *   with no arguments: the .cue in this folder or its "disc" folder. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

struct DlxWord { uint32_t va, stock, word; };
#include "patches.h"

#define RAW 2352
#define DATA 2048
#define OUT_NAME "Dancing Stage Fusion Deluxe"

static int g_pause = 0;

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
static FILE *g_img;

static void read_sector( uint32_t lba, uint8_t *s )
{
	static const uint8_t sync[12] = { 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0 };
	uint8_t check[RAW];
	char where[32];
	snprintf( where, sizeof where, "%u", (unsigned)lba );
	if( fseek(g_img, (long)lba * RAW, SEEK_SET) != 0 || fread(s, 1, RAW, g_img) != RAW )
		fail( "cannot read sector %s of the image.", where );
	if( memcmp(s, sync, 12) != 0 || s[15] != 2 || (s[18] & 0x20) != 0 )
		fail( "sector %s is not a Mode 2 Form 1 data sector: is this a raw (2352-byte) image?", where );
	memcpy( check, s, RAW );
	seal( check );
	if( memcmp(check, s, RAW) != 0 )
		fail( "sector %s fails its error check: the image is damaged or not a clean dump.", where );
}

static void write_sector( uint32_t lba, uint8_t *s )
{
	seal( s );
	if( fseek(g_img, (long)lba * RAW, SEEK_SET) != 0 || fwrite(s, 1, RAW, g_img) != RAW )
		fail( "cannot write the new image.%s", "" );
}

/* Patches over a file on the disc that loads at `base` from sector `lba`:
 * words (each checked), and optionally a blob into a block that must be zero. */
struct Edit { uint32_t va; const uint8_t *bytes; uint32_t n; };

static void apply( uint32_t lba, uint32_t base, const struct DlxWord *w, int nw, const uint8_t *blob, uint32_t blob_va,
	uint32_t blob_n, uint32_t zero_end, int dry )
{
	uint32_t first = 0xFFFFFFFFu, last = 0, sec, va;
	int i;
	for( i = 0; i < nw; ++i )
	{
		const uint32_t s = (w[i].va - base) / DATA;
		if( s < first ) first = s;
		if( s > last ) last = s;
	}
	if( blob_n )
	{
		if( (blob_va - base) / DATA < first ) first = (blob_va - base) / DATA;
		if( (zero_end - 1 - base) / DATA > last ) last = (zero_end - 1 - base) / DATA;
	}
	for( sec = first; sec <= last; ++sec )
	{
		uint8_t s[RAW];
		uint8_t *d = s + 24;
		const uint32_t lo = base + sec * DATA, hi = lo + DATA;
		int changed = 0;
		read_sector( lba + sec, s );
		for( i = 0; i < nw; ++i )
		{
			if( w[i].va < lo || w[i].va >= hi )
				continue;
			if( get32(d + (w[i].va - lo)) != w[i].stock )
			{
				char a[16];
				snprintf( a, sizeof a, "%08X", (unsigned)w[i].va );
				fail( "the game code at %s is not as expected: this is not the PAL Dancing Stage Fusion "
				      "(SLES-04163), or it is already patched.", a );
			}
			put32( d + (w[i].va - lo), w[i].word );
			changed = 1;
		}
		for( va = lo > blob_va ? lo : blob_va; blob_n && va < hi && va < zero_end; ++va )
		{
			if( d[va - lo] != 0 )
				fail( "the free block in the game program is not free: unexpected version of the game.%s", "" );
			if( va < blob_va + blob_n )
			{
				d[va - lo] = blob[va - blob_va];
				changed = 1;
			}
		}
		if( changed && !dry )
			write_sector( lba + sec, s );
	}
}

/* ---- the .cue ---- */
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

int main( int argc, char **argv )
{
	char *bin, out_bin[1200], out_cue[1200], *out_dir;
	static char buf[1 << 20];
	FILE *in, *cue;
	size_t n;
	long long total = 0, size;

	printf( "Dancing Stage Fusion Deluxe - disc builder (test build 1)\n"
	        "Makes a Deluxe disc image from YOUR Dancing Stage Fusion (Europe) image.\n\n" );
	ecc_init();
	if( argc < 2 )
	{
		char *c;
		g_pause = 1;
		c = find_cue( "" );
		if( !c )
#ifdef _WIN32
			c = find_cue( "disc\\" );
#else
			c = find_cue( "disc/" );
#endif
		if( !c )
			fail( "no disc image found. Put your Dancing Stage Fusion .bin and .cue in this folder "
			      "(or its \"disc\" folder), or drop the .cue onto dlxbuild.exe.%s", "" );
		argv[1] = c;
		argc = 2;
	}
	else if( argc == 2 )
		g_pause = 1;	/* dropped onto the program */
	bin = ends_with( argv[1], ".cue" ) ? bin_from_cue( argv[1] ) : argv[1];
	out_dir = argc > 2 ? argv[2] : dir_of( argv[1] );
	if( argc > 2 && *out_dir && !ends_with(out_dir, "/") && !ends_with(out_dir, "\\") )
	{
		char *d = malloc( strlen(out_dir) + 2 );
		strcpy( d, out_dir );
		strcat( d, "/" );
		out_dir = d;
	}
	snprintf( out_bin, sizeof out_bin, "%s%s.bin", out_dir, OUT_NAME );
	snprintf( out_cue, sizeof out_cue, "%s%s.cue", out_dir, OUT_NAME );
	printf( "Your disc:  %s\n", bin );

	/* Check everything on the original first. */
	if( !(g_img = fopen(bin, "rb")) )
		fail( "cannot open %s", bin );
	fseek( g_img, 0, SEEK_END );
	size = ftell( g_img );
	if( size <= 0 || size % RAW != 0 )
		fail( "%s is not a raw 2352-byte-sector image.", bin );
	apply( DLX_EXE_LBA, DLX_EXE_BASE, dlx_exe_words, sizeof dlx_exe_words / sizeof *dlx_exe_words,
	       dlx_payload, DLX_BLOCK, sizeof dlx_payload, DLX_BLOCK_END, 1 );
	apply( DLX_OVL_LBA, DLX_OVL_BASE, dlx_ovl_words, sizeof dlx_ovl_words / sizeof *dlx_ovl_words, NULL, 0, 0, 0, 1 );
	printf( "Checked:    the PAL game (SLES-04163), unpatched.\n" );

	/* Copy, then patch the copy. */
	printf( "Writing:    %s\n", out_bin );
	in = g_img;
	if( !(g_img = fopen(out_bin, "wb")) )
		fail( "cannot create %s", out_bin );
	fseek( in, 0, SEEK_SET );
	while( (n = fread(buf, 1, sizeof buf, in)) > 0 )
	{
		if( fwrite(buf, 1, n, g_img) != n )
			fail( "cannot write %s (disk full?)", out_bin );
		total += (long long)n;
		if( total % (64 << 20) < (1 << 20) )
			printf( "  %lld MB\r", total >> 20 ), fflush( stdout );
	}
	fclose( in );
	fclose( g_img );
	if( total != size )
		fail( "could not copy all of %s", bin );
	if( !(g_img = fopen(out_bin, "r+b")) )
		fail( "cannot reopen %s", out_bin );
	apply( DLX_EXE_LBA, DLX_EXE_BASE, dlx_exe_words, sizeof dlx_exe_words / sizeof *dlx_exe_words,
	       dlx_payload, DLX_BLOCK, sizeof dlx_payload, DLX_BLOCK_END, 0 );
	apply( DLX_OVL_LBA, DLX_OVL_BASE, dlx_ovl_words, sizeof dlx_ovl_words / sizeof *dlx_ovl_words, NULL, 0, 0, 0, 0 );
	fclose( g_img );

	if( !(cue = fopen(out_cue, "w")) )
		fail( "cannot create %s", out_cue );
	fprintf( cue, "FILE \"%s.bin\" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n", OUT_NAME );
	fclose( cue );
	printf( "Done:       %s\n\nOpen the .cue in DuckStation. For your own use only: do not share the image.\n", out_cue );
	finish( 0 );
	return 0;
}
