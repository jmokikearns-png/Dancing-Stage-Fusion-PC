/*
 * Dancing Stage Fusion PC - Setup
 *
 * Builds the game core (dsfcore.dll) from the player's own disc image:
 *   1. checks the image in the "disc" folder is the PAL Dancing Stage Fusion
 *      (SLES-04163, Redump);
 *   2. reads the game program off it;
 *   3. recompiles it to C with the bundled PSXRecomp (setup\psxrecomp-game.exe);
 *   4. compiles that with the bundled GCC (setup\compiler) and links it with
 *      the prebuilt rest of the game core (setup\kit) into dsfcore.dll.
 * The recompiled code is deleted afterwards: dsfcore.dll is the only thing
 * left, and like the disc image it is the player's own, not to be shared.
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "logo.h"

#define RAW 2352
#define DATA 2048
#define DISC_SIZE 248103072LL
#define DISC_CRC 0x78592E6Bu
#define EXE_LBA 24
#define EXE_NAME "SLES_041.63"
#define EXE_LOAD 0x8001A000u
#define EXE_ENTRY 0x8002397Cu
#define EXE_SIZE 636928u
#define DELUXE_NAME "Dancing Stage Fusion Deluxe"
#define CARD_ICON 0x983C0u	/* the save icon's palette (16 colours) and 16x16 4-bit pixels, in the program file */
#define PROGRAM "DancingStageFusionPC.exe"
#define GAME_FILES 0x80092B50u	/* the game's file table: size and first sector of each file */
#define ICON_GROUP 1007		/* the program's icon (StepMania's IDI_ICON) */
#define MAX_FILES 128

static char g_root[MAX_PATH], g_setup[MAX_PATH], g_work[MAX_PATH], g_kit[MAX_PATH];

static void pause_and_exit( int code )
{
	char line[8];
	printf( "\nPress Enter to close this window." );
	fflush( stdout );
	fgets( line, sizeof line, stdin );
	exit( code );
}

static void fail( const char *fmt, const char *arg )
{
	printf( "\nSetup could not finish:\n  " );
	printf( fmt, arg );
	printf( "\n" );
	pause_and_exit( 1 );
}

static void join( char *out, const char *a, const char *b )
{
	snprintf( out, MAX_PATH, "%s\\%s", a, b );
}

static int exists( const char *p )
{
	return GetFileAttributesA( p ) != INVALID_FILE_ATTRIBUTES;
}

static void need( const char *p )
{
	if( !exists(p) )
		fail( "%s is missing. Unpack the whole release again, keeping its folders.", p );
}

/* ---- the disc ---- */

static uint32_t g_crc_table[256];

static void crc_init( void )
{
	uint32_t i, k, c;
	for( i = 0; i < 256; ++i )
	{
		for( c = i, k = 0; k < 8; ++k )
			c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
		g_crc_table[i] = c;
	}
}

/* The player's .cue in disc\ (not a Deluxe image made from it). */
static int find_cue( char *out )
{
	WIN32_FIND_DATAA fd;
	char pat[MAX_PATH], dir[MAX_PATH];
	HANDLE h;
	join( dir, g_root, "disc" );
	join( pat, dir, "*.cue" );
	h = FindFirstFileA( pat, &fd );
	if( h == INVALID_HANDLE_VALUE )
		return 0;
	do
	{
		if( strstr(fd.cFileName, DELUXE_NAME) == NULL )
		{
			join( out, dir, fd.cFileName );
			FindClose( h );
			return 1;
		}
	} while( FindNextFileA(h, &fd) );
	FindClose( h );
	return 0;
}

/* The .bin a single-track MODE2/2352 .cue names. */
static void bin_from_cue( const char *cue, char *bin )
{
	FILE *f = fopen( cue, "r" );
	char line[1024], *q1, *q2, *slash;
	int tracks = 0, files = 0;
	if( !f )
		fail( "cannot open %s", cue );
	while( fgets(line, sizeof line, f) )
	{
		if( strstr(line, "TRACK") )
		{
			++tracks;
			if( !strstr(line, "MODE2/2352") )
				fail( "the .cue's track is not MODE2/2352: %s", line );
		}
		if( strstr(line, "FILE") && (q1 = strchr(line, '"')) != NULL && (q2 = strchr(q1 + 1, '"')) != NULL )
		{
			*q2 = 0;
			snprintf( bin, MAX_PATH, "%s", cue );
			slash = strrchr( bin, '\\' );
			snprintf( slash + 1, MAX_PATH - (size_t)(slash + 1 - bin), "%s", q1 + 1 );
			++files;
		}
	}
	fclose( f );
	if( tracks != 1 || files != 1 )
		fail( "%s is not a single-track .cue for one .bin.", cue );
}

static uint32_t get32( const uint8_t *p )
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* Check the whole image against the Redump dump and read the game program. */
static uint8_t *read_game_program( const char *bin )
{
	static uint8_t buf[1 << 20];
	FILE *f = fopen( bin, "rb" );
	long long total = 0;
	uint32_t crc = 0xFFFFFFFFu, i, n;
	uint8_t *exe;
	size_t got;
	int last = -1;
	if( !f )
		fail( "cannot open %s (is it next to the .cue, with the name the .cue gives?)", bin );
	_fseeki64( f, 0, SEEK_END );
	if( _ftelli64(f) != DISC_SIZE )
		fail( "%s is not the expected size: it must be the Redump dump of Dancing Stage Fusion "
		      "(Europe) (En,Fr,De,Es,It), SLES-04163, as a raw .bin.", bin );
	_fseeki64( f, 0, SEEK_SET );
	while( (got = fread(buf, 1, sizeof buf, f)) > 0 )
	{
		size_t k;
		int pct;
		for( k = 0; k < got; ++k )
			crc = g_crc_table[(crc ^ buf[k]) & 0xFF] ^ (crc >> 8);
		total += (long long)got;
		pct = (int)(total * 100 / DISC_SIZE);
		if( pct != last )
			printf( "\r  Checking your disc image... %3d%%", pct ), fflush( stdout ), last = pct;
	}
	printf( "\n" );
	if( total != DISC_SIZE || (crc ^ 0xFFFFFFFFu) != DISC_CRC )
		fail( "%s does not match the Redump dump of Dancing Stage Fusion (Europe) (SLES-04163). "
		      "It may be damaged, a different version, or already modified.", bin );

	n = (EXE_SIZE + DATA - 1) / DATA;
	exe = malloc( (size_t)n * DATA );
	for( i = 0; i < n; ++i )
	{
		_fseeki64( f, (long long)(EXE_LBA + i) * RAW, SEEK_SET );
		if( fread(buf, 1, RAW, f) != RAW )
			fail( "cannot read %s", bin );
		memcpy( exe + (size_t)i * DATA, buf + 24, DATA );
	}
	fclose( f );
	if( memcmp(exe, "PS-X EXE", 8) != 0 || get32(exe + 0x18) != EXE_LOAD || get32(exe + 0x10) != EXE_ENTRY ||
	    0x800 + get32(exe + 0x1C) != EXE_SIZE )
		fail( "the game program on the disc is not as expected.%s", "" );
	return exe;
}

/* One of the game's files (its table in the program: size, first sector). */
static uint8_t *read_game_file( const char *bin, const uint8_t *exe, int entry, uint32_t *size )
{
	static uint8_t sector[RAW];
	const uint32_t at = 0x800 + (GAME_FILES - EXE_LOAD) + 8 * (uint32_t)entry;
	const uint32_t n = get32( exe + at ), lba = get32( exe + at + 4 ), sectors = (n + DATA - 1) / DATA;
	uint8_t *buf = malloc( (size_t)sectors * DATA );
	FILE *f = fopen( bin, "rb" );
	uint32_t i;
	if( !f )
		fail( "cannot open %s", bin );
	for( i = 0; i < sectors; ++i )
	{
		_fseeki64( f, (long long)(lba + i) * RAW, SEEK_SET );
		if( fread(sector, 1, RAW, f) != RAW )
			fail( "cannot read %s", bin );
		memcpy( buf + (size_t)i * DATA, sector + 24, DATA );
	}
	fclose( f );
	*size = n;
	return buf;
}

static int copy_file( const char *from, const char *to )
{
	return CopyFileA( from, to, FALSE ) != 0;
}

/* The title screen's logo: the game's own, from the disc (logo.c), with the
 * project's "PC" and animation (setup\logo) beside it. Loose files in
 * Themes\ take the place of the ones in DancingStageFusionPC.dat. */
static void set_title_logo( const char *bin, const uint8_t *exe )
{
	static const char *const ours[] = { "_title logo pc (res 126x104).png", "ScreenTitleMenu logo.xml" };
	char dir[MAX_PATH], src[MAX_PATH], p[MAX_PATH], q[MAX_PATH];
	uint32_t n;
	uint8_t *ovl = read_game_file( bin, exe, LOGO_OVERLAY, &n );
	int i, ok;
	join( dir, g_root, "Themes" ); CreateDirectoryA( dir, NULL );
	join( dir, g_root, "Themes\\DSF PC" ); CreateDirectoryA( dir, NULL );
	join( dir, g_root, "Themes\\DSF PC\\Graphics" ); CreateDirectoryA( dir, NULL );
	join( p, dir, "_title logo dsf (res 374x176).png" );
	ok = make_title_logo( ovl, n, p );
	join( src, g_setup, "logo" );
	for( i = 0; ok && i < 2; ++i )
	{
		join( q, src, ours[i] );
		join( p, dir, ours[i] );
		ok = copy_file( q, p );
	}
	free( ovl );
	if( !ok )
	{
		/* without all three, the title keeps the lettering in DancingStageFusionPC.dat */
		join( p, dir, "ScreenTitleMenu logo.xml" );
		DeleteFileA( p );
		printf( "  (Could not make the title screen's logo: the title shows the plain lettering.)\n" );
		return;
	}
	printf( "  The title screen now has the game's logo.\n" );
}

static void write_file( const char *path, const void *data, size_t n )
{
	FILE *f = fopen( path, "wb" );
	if( !f || fwrite(data, 1, n, f) != n || fclose(f) != 0 )
		fail( "cannot write %s (disk full, or a read-only folder?)", path );
}

static int cmp_u32( const void *a, const void *b )
{
	const uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
	return x < y ? -1 : x > y;
}

static int cmp_name( const void *a, const void *b )
{
	return strcmp( (const char *)a, (const char *)b );
}

/* The recompiler's starting points: every jal target in the program, and the entry. */
static void write_seeds( const uint8_t *exe, const char *path )
{
	const uint32_t text = EXE_SIZE - 0x800;
	uint32_t *t = malloc( (text / 4 + 1) * sizeof *t ), n = 0, i;
	FILE *f;
	for( i = 0; i < text; i += 4 )
	{
		const uint32_t w = get32( exe + 0x800 + i );
		if( w >> 26 == 3 )
		{
			const uint32_t pc = EXE_LOAD + i, to = ((pc + 4) & 0xF0000000u) | (w & 0x3FFFFFFu) << 2;
			if( to >= EXE_LOAD && to < EXE_LOAD + text )
				t[n++] = to;
		}
	}
	t[n++] = EXE_ENTRY;
	qsort( t, n, sizeof *t, cmp_u32 );
	if( !(f = fopen(path, "w")) )
		fail( "cannot write %s", path );
	fprintf( f, "# Auto-scanned JAL targets (+ entry) from " EXE_NAME "\n" );
	for( i = 0; i < n; ++i )
		if( i == 0 || t[i] != t[i - 1] )
			fprintf( f, "0x%08X\n", t[i] );
	fclose( f );
	free( t );
}

/* game.toml for the recompiler: the template, pointed at the player's disc. */
static void write_project( const char *cue )
{
	char tmpl[MAX_PATH], out[MAX_PATH], line[2048], disc[MAX_PATH], *p;
	FILE *in, *o;
	join( tmpl, g_setup, "recomp\\game.toml" );
	join( out, g_work, "game.toml" );
	snprintf( disc, sizeof disc, "%s", cue );
	for( p = disc; *p; ++p )
		if( *p == '\\' )
			*p = '/';
	if( !(in = fopen(tmpl, "r")) )
		fail( "%s is missing. Unpack the whole release again, keeping its folders.", tmpl );
	if( !(o = fopen(out, "w")) )
		fail( "cannot write %s", out );
	while( fgets(line, sizeof line, in) )
	{
		if( strncmp(line, "disc = ", 7) == 0 )
			fprintf( o, "disc = \"%s\"\n", disc );
		else
			fputs( line, o );
	}
	fclose( in );
	fclose( o );
}

/* ---- the program's icon: the game's memory card icon with a "PC" badge ---- */

static const int k_icon_sizes[] = { 16, 32, 48, 64, 128, 256 };
#define NUM_ICON_SIZES 6

static void put16( uint8_t *p, uint32_t v ) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32( uint8_t *p, uint32_t v ) { put16( p, v ); put16( p + 2, v >> 16 ); }

/* One icon image (a 32-bit DIB with an empty AND mask) of the card icon,
 * scaled up without smoothing, with the badge (RGBA, or none) over it. */
static uint8_t *icon_image( const uint8_t *card, int size, const uint8_t *badge, uint32_t *out_n )
{
	const uint32_t mask_row = ((uint32_t)size + 31) / 32 * 4;
	const uint32_t n = 40 + (uint32_t)size * size * 4 + mask_row * size;
	uint8_t *img = calloc( 1, n ), *px = img + 40;
	int x, y;
	put32( img, 40 );
	put32( img + 4, (uint32_t)size );
	put32( img + 8, (uint32_t)size * 2 );
	put16( img + 12, 1 );
	put16( img + 14, 32 );
	put32( img + 20, (uint32_t)size * size * 4 + mask_row * size );
	for( y = 0; y < size; ++y )
		for( x = 0; x < size; ++x )
		{
			const int cx = x * 16 / size, cy = y * 16 / size;
			const uint8_t b = card[32 + cy * 8 + cx / 2];
			const uint32_t c = card[((cx & 1) ? b >> 4 : b & 15) * 2] | (uint32_t)card[((cx & 1) ? b >> 4 : b & 15) * 2 + 1] << 8;
			uint32_t r = (c & 31) * 255 / 31, g = ((c >> 5) & 31) * 255 / 31, bl = ((c >> 10) & 31) * 255 / 31, a = c ? 255 : 0;
			uint8_t *o = px + ((size - 1 - y) * size + x) * 4;	/* bottom-up */
			if( badge )
			{
				const uint8_t *s = badge + (y * size + x) * 4;
				const uint32_t ba = s[3];
				r = (s[0] * ba + r * (255 - ba)) / 255;
				g = (s[1] * ba + g * (255 - ba)) / 255;
				bl = (s[2] * ba + bl * (255 - ba)) / 255;
				a = ba + a * (255 - ba) / 255;
			}
			o[0] = (uint8_t)bl; o[1] = (uint8_t)g; o[2] = (uint8_t)r; o[3] = (uint8_t)a;
		}
	*out_n = n;
	return img;
}

/* Put the icon in the program.  Not worth stopping for if it can't be done. */
static void set_icon( const uint8_t *exe )
{
	char prog[MAX_PATH], path[MAX_PATH];
	uint8_t *badges, *group, *img[NUM_ICON_SIZES];
	uint32_t n[NUM_ICON_SIZES], need = 0, got;
	const uint8_t *card = exe + CARD_ICON, *b;
	FILE *f;
	HANDLE h;
	int i, ok = 1;
	for( i = 1; i < NUM_ICON_SIZES; ++i )
		need += (uint32_t)(k_icon_sizes[i] * k_icon_sizes[i] * 4);
	join( path, g_setup, "icon\\badge.rgba" );
	join( prog, g_root, PROGRAM );
	badges = malloc( need );
	if( !(f = fopen(path, "rb")) )
	{
		printf( "  (No setup\\icon\\badge.rgba: the program keeps its old icon.)\n" );
		free( badges );
		return;
	}
	got = (uint32_t)fread( badges, 1, need, f );
	fclose( f );
	if( got != need )
	{
		printf( "  (setup\\icon\\badge.rgba is damaged: the program keeps its old icon.)\n" );
		free( badges );
		return;
	}
	group = calloc( 1, 6 + 14 * NUM_ICON_SIZES );
	put16( group + 2, 1 );
	put16( group + 4, NUM_ICON_SIZES );
	for( i = 0, b = badges; i < NUM_ICON_SIZES; ++i )
	{
		const int s = k_icon_sizes[i];
		uint8_t *e = group + 6 + 14 * i;
		img[i] = icon_image( card, s, i ? b : NULL, &n[i] );
		if( i )
			b += s * s * 4;
		e[0] = (uint8_t)(s == 256 ? 0 : s);
		e[1] = (uint8_t)(s == 256 ? 0 : s);
		put16( e + 4, 1 );
		put16( e + 6, 32 );
		put32( e + 8, n[i] );
		put16( e + 12, (uint32_t)(i + 1) );
	}
	h = BeginUpdateResourceA( prog, FALSE );
	if( !h )
		ok = 0;
	for( i = 0; ok && i < NUM_ICON_SIZES; ++i )
		ok = UpdateResourceA( h, RT_ICON, MAKEINTRESOURCEA(i + 1), MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), img[i], n[i] );
	if( ok )
		ok = UpdateResourceA( h, RT_GROUP_ICON, MAKEINTRESOURCEA(ICON_GROUP), MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
		                      group, 6 + 14 * NUM_ICON_SIZES );
	if( h )
		ok = EndUpdateResourceA( h, !ok ) && ok;
	printf( ok ? "  The program now has the game's icon.\n" : "  (Could not change the program's icon: it keeps its old one.)\n" );
	for( i = 0; i < NUM_ICON_SIZES; ++i )
		free( img[i] );
	free( group );
	free( badges );
}

/* ---- running the tools ---- */

struct Job
{
	PROCESS_INFORMATION pi;
	HANDLE log;
	char log_path[MAX_PATH];
	char name[MAX_PATH];
	int running;
};

static int start( struct Job *j, const char *cmd, const char *cwd, const char *log_path )
{
	STARTUPINFOA si;
	SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
	char *line = _strdup( cmd );
	BOOL ok;
	snprintf( j->log_path, sizeof j->log_path, "%s", log_path );
	j->log = CreateFileA( log_path, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
	if( j->log == INVALID_HANDLE_VALUE )
		fail( "cannot write %s", log_path );
	memset( &si, 0, sizeof si );
	si.cb = sizeof si;
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = GetStdHandle( STD_INPUT_HANDLE );
	si.hStdOutput = j->log;
	si.hStdError = j->log;
	ok = CreateProcessA( NULL, line, NULL, NULL, TRUE, CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS, NULL, cwd, &si, &j->pi );
	free( line );
	if( !ok )
	{
		CloseHandle( j->log );
		return 0;
	}
	j->running = 1;
	return 1;
}

static DWORD finish( struct Job *j )
{
	DWORD code = 1;
	WaitForSingleObject( j->pi.hProcess, INFINITE );
	GetExitCodeProcess( j->pi.hProcess, &code );
	CloseHandle( j->pi.hProcess );
	CloseHandle( j->pi.hThread );
	CloseHandle( j->log );
	j->running = 0;
	return code;
}

static void show_log( const char *path )
{
	FILE *f = fopen( path, "r" );
	char line[1024];
	int n = 0;
	if( !f )
		return;
	printf( "\n---- %s ----\n", path );
	while( fgets(line, sizeof line, f) && n++ < 40 )
		fputs( line, stdout );
	fclose( f );
}

static void run( const char *what, const char *cmd, const char *cwd, const char *log_path )
{
	struct Job j;
	if( !start(&j, cmd, cwd, log_path) )
		fail( "cannot start %s.", what );
	if( finish(&j) != 0 )
	{
		show_log( log_path );
		fail( "%s failed (the full log is above and in the setup\\work folder).", what );
	}
}

/* How many compilers to run at once: the processors, and about 1.5 GB of memory each. */
static int parallel_jobs( void )
{
	SYSTEM_INFO si;
	MEMORYSTATUSEX ms;
	int n, by_mem;
	GetSystemInfo( &si );
	n = (int)si.dwNumberOfProcessors;
	ms.dwLength = sizeof ms;
	if( GlobalMemoryStatusEx(&ms) )
	{
		by_mem = (int)(ms.ullAvailPhys / (1536ull << 20));
		if( by_mem < n )
			n = by_mem;
	}
	if( n > 16 )
		n = 16;
	return n < 1 ? 1 : n;
}

static void delete_tree( const char *dir )
{
	WIN32_FIND_DATAA fd;
	char pat[MAX_PATH], p[MAX_PATH];
	HANDLE h;
	join( pat, dir, "*" );
	h = FindFirstFileA( pat, &fd );
	if( h != INVALID_HANDLE_VALUE )
	{
		do
		{
			if( !strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..") )
				continue;
			join( p, dir, fd.cFileName );
			if( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
				delete_tree( p );
			else
				DeleteFileA( p );
		} while( FindNextFileA(h, &fd) );
		FindClose( h );
	}
	RemoveDirectoryA( dir );
}

int main( void )
{
	char cue[MAX_PATH], bin[MAX_PATH], p[MAX_PATH], q[MAX_PATH], gcc[MAX_PATH], gxx[MAX_PATH], recomp[MAX_PATH];
	char cmd[32768], gen[MAX_PATH], obj[MAX_PATH], dll_new[MAX_PATH], dll[MAX_PATH];
	static char files[MAX_FILES][MAX_PATH];
	static struct Job jobs[16];
	WIN32_FIND_DATAA fd;
	HANDLE h;
	uint8_t *exe;
	int nfiles = 0, next = 0, done = 0, running = 0, i, nj, failed = -1;
	DWORD t0 = GetTickCount();

	printf( "Dancing Stage Fusion PC - Setup\n"
	        "Builds the game from YOUR copy of Dancing Stage Fusion (Europe).\n\n" );
	crc_init();
	GetModuleFileNameA( NULL, g_root, sizeof g_root );
	*strrchr( g_root, '\\' ) = 0;
	join( g_setup, g_root, "setup" );
	join( g_work, g_setup, "work" );
	join( g_kit, g_setup, "kit" );
	join( recomp, g_setup, "psxrecomp-game.exe" );
	join( gcc, g_setup, "compiler\\bin\\x86_64-w64-mingw32-gcc.exe" );
	join( gxx, g_setup, "compiler\\bin\\x86_64-w64-mingw32-g++.exe" );
	need( recomp );
	need( gcc );
	need( gxx );
	join( p, g_kit, "lib\\libdsfcore_kit.a" );
	need( p );

	/* 1-2: the disc and its game program */
	if( !find_cue(cue) )
		fail( "no disc image found. Put your Dancing Stage Fusion .bin and .cue in the \"disc\" folder:\n  %s\\disc",
		      g_root );
	bin_from_cue( cue, bin );
	printf( "Your disc: %s\n", bin );
	exe = read_game_program( bin );
	printf( "  It is the PAL Dancing Stage Fusion (SLES-04163).\n" );

	delete_tree( g_work );
	CreateDirectoryA( g_work, NULL );
	join( p, g_work, "disc_exe" ); CreateDirectoryA( p, NULL );
	join( p, g_work, "seeds" ); CreateDirectoryA( p, NULL );
	join( p, g_work, "obj" ); CreateDirectoryA( p, NULL );
	join( p, g_work, "disc_exe\\" EXE_NAME );
	write_file( p, exe, EXE_SIZE );
	join( p, g_work, "seeds\\ghidra_funcs.txt" );
	write_seeds( exe, p );
	write_project( cue );
	set_icon( exe );
	set_title_logo( bin, exe );
	free( exe );

	/* 3: recompile */
	printf( "Recompiling the game program (about a minute)...\n" );
	snprintf( cmd, sizeof cmd, "\"%s\" --config game.toml --project-root \"%s\\recomp\"", recomp, g_setup );
	join( p, g_work, "recompile.log" );
	run( "Recompiling", cmd, g_work, p );

	join( gen, g_work, "generated" );
	join( p, gen, "*.c" );
	h = FindFirstFileA( p, &fd );
	if( h != INVALID_HANDLE_VALUE )
	{
		do
			if( nfiles < MAX_FILES )
				snprintf( files[nfiles++], MAX_PATH, "%.*s", (int)strlen(fd.cFileName) - 2, fd.cFileName );
		while( FindNextFileA(h, &fd) );
		FindClose( h );
	}
	qsort( files, (size_t)nfiles, sizeof *files, cmp_name );	/* the same build every time */
	if( nfiles < 2 )
		fail( "the recompiler wrote no code (see %s\\recompile.log).", g_work );

	/* 4: compile, several at once */
	nj = parallel_jobs();
	printf( "Compiling the game (%d files, %d at a time; a few minutes)...\n", nfiles, nj );
	while( done < nfiles && failed < 0 )
	{
		while( running < nj && next < nfiles )
		{
			for( i = 0; i < nj && jobs[i].running; ++i )
				;
			join( p, gen, files[next] );
			join( obj, g_work, "obj\\" );
			snprintf( cmd, sizeof cmd, "\"%s\" @compile.rsp -c \"%s.c\" -o \"%s%s.o\"", gcc, p, obj, files[next] );
			snprintf( q, sizeof q, "%s\\obj\\%s.log", g_work, files[next] );
			snprintf( jobs[i].name, sizeof jobs[i].name, "%d", next );
			if( !start(&jobs[i], cmd, g_kit, q) )
				fail( "cannot start the compiler (%s).", gcc );
			++next;
			++running;
		}
		{
			HANDLE hs[16];
			int map[16], k = 0;
			DWORD r;
			for( i = 0; i < nj; ++i )
				if( jobs[i].running )
					hs[k] = jobs[i].pi.hProcess, map[k++] = i;
			r = WaitForMultipleObjects( (DWORD)k, hs, FALSE, INFINITE );
			if( r < WAIT_OBJECT_0 + (DWORD)k )
			{
				struct Job *j = &jobs[map[r - WAIT_OBJECT_0]];
				if( finish(j) != 0 )
					failed = atoi( j->name );
				--running;
				++done;
				printf( "\r  %d of %d", done, nfiles );
				fflush( stdout );
			}
		}
	}
	for( i = 0; i < nj; ++i )
		if( jobs[i].running )
			finish( &jobs[i] );
	printf( "\n" );
	if( failed >= 0 )
	{
		snprintf( q, sizeof q, "%s\\obj\\%s.log", g_work, files[failed] );
		show_log( q );
		fail( "compiling %s failed.", files[failed] );
	}

	/* 5: link */
	printf( "Linking dsfcore.dll...\n" );
	join( dll, g_root, "dsfcore.dll" );
	join( dll_new, g_work, "dsfcore.dll" );	/* the name sets the image base: keep it */
	snprintf( cmd, sizeof cmd, "\"%s\" -o \"%s\"", gxx, dll_new );
	for( i = 0; i < nfiles; ++i )
	{
		size_t n = strlen( cmd );
		snprintf( cmd + n, sizeof cmd - n, " \"%s\\obj\\%s.o\"", g_work, files[i] );
	}
	strncat( cmd, " @link.rsp", sizeof cmd - strlen(cmd) - 1 );
	join( p, g_work, "link.log" );
	run( "Linking", cmd, g_kit, p );
	if( !MoveFileExA(dll_new, dll, MOVEFILE_REPLACE_EXISTING) )
		fail( "cannot replace %s (is the game still running?)", dll );

	/* The recompiled code is the game's own: leave none of it behind
	 * (unless DSF_SETUP_KEEP is set, for looking into a problem). */
	if( !getenv("DSF_SETUP_KEEP") )
		delete_tree( g_work );
	printf( "\nDone in %lu seconds. Start the game with DancingStageFusionPC.exe.\n"
	        "dsfcore.dll is built from your disc: for your own use only, do not share it.\n",
	        (unsigned long)((GetTickCount() - t0) / 1000) );
	pause_and_exit( 0 );
	return 0;
}
