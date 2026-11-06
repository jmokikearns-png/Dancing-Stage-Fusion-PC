/* Updater.exe - installs the latest Dancing Stage Fusion PC release from
 * GitHub into the game folder it is in.
 *
 *   Updater.exe            ask, then update (a console window)
 *   Updater.exe --yes      update without asking
 *   Updater.exe --restart  start the game when done (with --yes: from the game)
 *   Updater.exe --check    only report: prints "UPDATE <version>" and exits 0
 *                          when a newer release is out, "LATEST <version>"
 *                          (exit 1), or "ERROR <reason>" (exit 2)
 *
 * What it keeps: everything of the player's. A release holds none of it
 * (no disc image, dsfcore.dll, memory cards, settings, key maps or song
 * packs), so the new files are copied over the old ones and nothing is
 * deleted. If the files Setup builds the game core from changed, it runs
 * Setup again (with the disc image in "disc"); otherwise it only gives the
 * new DancingStageFusionPC.exe the game's icon again (Setup --icon).
 *
 * DSF_UPDATE_API overrides the release URL and DSF_UPDATE_PROXY (host:port)
 * the proxy, for testing. */
#include <windows.h>
#include <winhttp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define API_URL "https://api.github.com/repos/jmokikearns-png/Dancing-Stage-Fusion-PC/releases/latest"
#define PROGRAM "DancingStageFusionPC.exe"
#define FIRST_VERSION "1.0"	/* 1.0 had no VERSION file */

static char g_root[MAX_PATH], g_stage[MAX_PATH];
static int g_check, g_yes, g_restart;

static void join( char *out, const char *a, const char *b )
{
	snprintf( out, MAX_PATH, "%s\\%s", a, b );
}

static void finish( int code )
{
	char line[8];
	if( !g_check && !(g_yes && g_restart) )
	{
		printf( "\nPress Enter to close this window." );
		fflush( stdout );
		fgets( line, sizeof line, stdin );
	}
	exit( code );
}

static void fail( const char *fmt, const char *arg )
{
	if( g_check )
	{
		printf( "ERROR " );
		printf( fmt, arg );
		printf( "\n" );
		exit( 2 );
	}
	printf( "\nThe update could not finish:\n  " );
	printf( fmt, arg );
	printf( "\nYour game is unchanged unless it says otherwise above.\n" );
	g_restart = 0;
	finish( 1 );
}

/* ---- HTTP (WinHTTP: the system's proxy settings and certificates) ---- */

static HINTERNET open_session( void )
{
	/* DSF_UPDATE_PROXY=host:port: a proxy to use instead of the system's (testing) */
	const char *px = getenv( "DSF_UPDATE_PROXY" );
	HINTERNET s;
	if( px && *px )
	{
		wchar_t wpx[256];
		MultiByteToWideChar( CP_UTF8, 0, px, -1, wpx, 256 );
		if( (s = WinHttpOpen(L"DancingStageFusionPC-Updater", WINHTTP_ACCESS_TYPE_NAMED_PROXY, wpx, WINHTTP_NO_PROXY_BYPASS, 0)) )
			return s;
	}
	s = WinHttpOpen( L"DancingStageFusionPC-Updater", 4 /* WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY */,
	                           WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0 );
	if( !s )
		s = WinHttpOpen( L"DancingStageFusionPC-Updater", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		                 WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0 );
	return s;
}

/* GET url: into memory (*mem, *len) or, with out, into a file. Redirects are followed. */
static int http_get( const char *url, FILE *out, char **mem, size_t *len, int progress, int api )
{
	wchar_t wurl[4096], host[256], path[3072];
	URL_COMPONENTS uc;
	HINTERNET s, c = NULL, r = NULL;
	DWORD status = 0, sz = sizeof status, total = 0, got = 0, avail, n;
	char buf[65536];
	size_t cap = 0;
	int ok = 0, last = -1;

	MultiByteToWideChar( CP_UTF8, 0, url, -1, wurl, 4096 );
	memset( &uc, 0, sizeof uc );
	uc.dwStructSize = sizeof uc;
	uc.lpszHostName = host; uc.dwHostNameLength = 256;
	uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
	uc.dwExtraInfoLength = 1;
	if( !WinHttpCrackUrl(wurl, 0, 0, &uc) )
		return 0;
	if( uc.lpszExtraInfo && uc.dwExtraInfoLength )
		wcsncat( path, uc.lpszExtraInfo, 3072 - wcslen(path) - 1 );
	if( !(s = open_session()) )
		return 0;
	if( (c = WinHttpConnect(s, host, uc.nPort, 0)) &&
	    (r = WinHttpOpenRequest(c, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
	                            uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)) &&
	    WinHttpSendRequest(r, api ? L"Accept: application/vnd.github+json\r\n" : WINHTTP_NO_ADDITIONAL_HEADERS, api ? (DWORD)-1 : 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
	    WinHttpReceiveResponse(r, NULL) &&
	    WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &sz, NULL) &&
	    status == 200 )
	{
		sz = sizeof total;
		if( !WinHttpQueryHeaders(r, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, NULL, &total, &sz, NULL) )
			total = 0;
		ok = 1;
		for( ;; )
		{
			if( !WinHttpQueryDataAvailable(r, &avail) ) { ok = 0; break; }
			if( avail == 0 )
				break;
			if( avail > sizeof buf )
				avail = sizeof buf;
			if( !WinHttpReadData(r, buf, avail, &n) ) { ok = 0; break; }
			if( n == 0 )
				break;
			if( out )
			{
				if( fwrite(buf, 1, n, out) != n ) { ok = 0; break; }
			}
			else
			{
				if( *len + n + 1 > cap )
				{
					cap = (*len + n + 1) * 2;
					*mem = realloc( *mem, cap );
				}
				memcpy( *mem + *len, buf, n );
				*len += n;
				(*mem)[*len] = 0;
			}
			got += n;
			if( progress && total )
			{
				int pct = (int)((uint64_t)got * 100 / total);
				if( pct != last )
					printf( "\r  Downloading... %3d%%", pct ), fflush( stdout ), last = pct;
			}
		}
		if( progress )
			printf( "\n" );
	}
	if( r ) WinHttpCloseHandle( r );
	if( c ) WinHttpCloseHandle( c );
	WinHttpCloseHandle( s );
	return ok && (out || *mem);
}

/* ---- the release: its version and zip ---- */

/* The JSON string value after "key": (no escapes in the values we read). */
static int json_string( const char *json, const char *key, char *out, size_t n, const char **after )
{
	char pat[64];
	const char *p, *e;
	snprintf( pat, sizeof pat, "\"%s\"", key );
	if( !(p = strstr(json, pat)) )
		return 0;
	p += strlen( pat );
	while( *p == ' ' || *p == ':' || *p == '\t' || *p == '\r' || *p == '\n' )
		++p;
	if( *p != '"' || !(e = strchr(p + 1, '"')) || (size_t)(e - p - 1) >= n )
		return 0;
	memcpy( out, p + 1, e - p - 1 );
	out[e - p - 1] = 0;
	if( after )
		*after = e + 1;
	return 1;
}

static void parse_version( const char *v, int out[4] )
{
	int i;
	memset( out, 0, 4 * sizeof(int) );
	while( *v && (*v < '0' || *v > '9') )
		++v;
	for( i = 0; i < 4 && *v; ++i )
	{
		out[i] = atoi( v );
		while( *v >= '0' && *v <= '9' )
			++v;
		if( *v != '.' )
			break;
		++v;
	}
}

static int newer( const char *a, const char *b )	/* a > b */
{
	int x[4], y[4], i;
	parse_version( a, x );
	parse_version( b, y );
	for( i = 0; i < 4; ++i )
		if( x[i] != y[i] )
			return x[i] > y[i];
	return 0;
}

static void installed_version( char *out, size_t n )
{
	char p[MAX_PATH];
	FILE *f;
	join( p, g_root, "VERSION" );
	snprintf( out, n, "%s", FIRST_VERSION );
	if( (f = fopen(p, "r")) )
	{
		if( fgets(out, (int)n, f) )
			out[strcspn(out, "\r\n \t")] = 0;
		fclose( f );
	}
}

/* ---- the zip (deflate or stored; the release is far below zip64 sizes) ---- */

static uint32_t rd16( const uint8_t *p ) { return p[0] | (p[1] << 8); }
static uint32_t rd32( const uint8_t *p ) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

static void make_dirs( char *path )	/* every folder of path (a file path) */
{
	char *p;
	for( p = path + 3; *p; ++p )
		if( *p == '\\' )
		{
			*p = 0;
			CreateDirectoryA( path, NULL );
			*p = '\\';
		}
}

static int unzip( const char *zip, const char *dest )
{
	FILE *f = fopen( zip, "rb" );
	uint8_t tail[65536 + 22], h[46], lh[30];
	long size, from;
	size_t got;
	uint32_t cd_off, cd_n, i, k, top_len = 0;
	char name[1024], top[256] = "", out[MAX_PATH];
	int found = 0;

	if( !f )
		return 0;
	fseek( f, 0, SEEK_END );
	size = ftell( f );
	from = size > (long)sizeof tail ? size - (long)sizeof tail : 0;
	fseek( f, from, SEEK_SET );
	got = fread( tail, 1, sizeof tail, f );
	for( k = (uint32_t)got >= 22 ? (uint32_t)got - 22 : 0; ; --k )
	{
		if( rd32(tail + k) == 0x06054b50 ) { found = 1; break; }
		if( k == 0 ) break;
	}
	if( !found ) { fclose( f ); return 0; }
	cd_n = rd16( tail + k + 10 );
	cd_off = rd32( tail + k + 16 );
	fseek( f, cd_off, SEEK_SET );
	for( i = 0; i < cd_n; ++i )
	{
		uint32_t method, csize, nlen, xlen, clen, loff, crc;
		long next;
		if( fread(h, 1, 46, f) != 46 || rd32(h) != 0x02014b50 ) { fclose( f ); return 0; }
		method = rd16( h + 10 ); crc = rd32( h + 16 );
		csize = rd32( h + 20 );
		nlen = rd16( h + 28 ); xlen = rd16( h + 30 ); clen = rd16( h + 32 );
		loff = rd32( h + 42 );
		if( nlen >= sizeof name ) { fclose( f ); return 0; }
		if( fread(name, 1, nlen, f) != nlen ) { fclose( f ); return 0; }
		name[nlen] = 0;
		next = ftell( f ) + xlen + clen;
		/* the release's top folder (DancingStageFusionPC-<version>/) is left out */
		if( !top_len )
		{
			char *s = strchr( name, '/' );
			if( !s ) { fclose( f ); return 0; }
			top_len = (uint32_t)(s - name + 1);
			memcpy( top, name, top_len );
			top[top_len] = 0;
		}
		if( strncmp(name, top, top_len) != 0 || strstr(name, "..") || strchr(name, ':') || name[top_len] == '/' )
		{ fclose( f ); return 0; }
		if( name[nlen - 1] != '/' && name[top_len] )
		{
			FILE *o;
			uint32_t ucrc = 0;
			char *q;
			join( out, dest, name + top_len );
			for( q = out; *q; ++q )
				if( *q == '/' ) *q = '\\';
			make_dirs( out );
			if( !(o = fopen(out, "wb")) ) { fclose( f ); return 0; }
			fseek( f, loff, SEEK_SET );
			if( fread(lh, 1, 30, f) != 30 || rd32(lh) != 0x04034b50 ) { fclose( o ); fclose( f ); return 0; }
			fseek( f, rd16(lh + 26) + rd16(lh + 28), SEEK_CUR );
			ucrc = crc32( 0, Z_NULL, 0 );
			if( method == 0 )
			{
				uint8_t b[65536];
				uint32_t left = csize;
				while( left )
				{
					size_t m = left > sizeof b ? sizeof b : left;
					if( fread(b, 1, m, f) != m ) { fclose( o ); fclose( f ); return 0; }
					ucrc = crc32( ucrc, b, (uInt)m );
					fwrite( b, 1, m, o );
					left -= (uint32_t)m;
				}
			}
			else if( method == 8 )
			{
				static uint8_t bin[65536], bout[65536];
				z_stream z;
				uint32_t left = csize;
				int zr = Z_OK;
				memset( &z, 0, sizeof z );
				if( inflateInit2(&z, -MAX_WBITS) != Z_OK ) { fclose( o ); fclose( f ); return 0; }
				while( zr != Z_STREAM_END )
				{
					if( z.avail_in == 0 && left )
					{
						size_t m = left > sizeof bin ? sizeof bin : left;
						if( fread(bin, 1, m, f) != m ) break;
						z.next_in = bin; z.avail_in = (uInt)m;
						left -= (uint32_t)m;
					}
					z.next_out = bout; z.avail_out = sizeof bout;
					zr = inflate( &z, Z_NO_FLUSH );
					if( zr != Z_OK && zr != Z_STREAM_END ) break;
					ucrc = crc32( ucrc, bout, (uInt)(sizeof bout - z.avail_out) );
					fwrite( bout, 1, sizeof bout - z.avail_out, o );
					if( zr == Z_OK && z.avail_in == 0 && !left && z.avail_out != 0 ) break;
				}
				inflateEnd( &z );
				if( zr != Z_STREAM_END ) { fclose( o ); fclose( f ); return 0; }
			}
			else { fclose( o ); fclose( f ); return 0; }
			fclose( o );
			if( ucrc != crc ) { fclose( f ); return 0; }
		}
		fseek( f, next, SEEK_SET );
	}
	fclose( f );
	return 1;
}

/* ---- comparing and copying ---- */

/* The bytes of a file; for a static library (ar archive) only its members'
 * names and contents, not the dates in their headers. */
static uint64_t content_hash( const char *path, int *exists )
{
	FILE *f = fopen( path, "rb" );
	uint64_t hsh = 1469598103934665603ull;
	uint8_t *d;
	long n, i = 0;
	*exists = f != NULL;
	if( !f )
		return 0;
	fseek( f, 0, SEEK_END );
	n = ftell( f );
	fseek( f, 0, SEEK_SET );
	d = malloc( n ? n : 1 );
	if( fread(d, 1, n, f) != (size_t)n ) n = 0;
	fclose( f );
	if( n > 8 && memcmp(d, "!<arch>\n", 8) == 0 )
	{
		i = 8;
		while( i + 60 <= n )
		{
			long m = atol( (const char *)d + i + 48 ), j;
			for( j = 0; j < 16; ++j )
				hsh = (hsh ^ d[i + j]) * 1099511628211ull;
			for( j = 0; j < m && i + 60 + j < n; ++j )
				hsh = (hsh ^ d[i + 60 + j]) * 1099511628211ull;
			i += 60 + m + (m & 1);
		}
	}
	else
		for( i = 0; i < n; ++i )
			hsh = (hsh ^ d[i]) * 1099511628211ull;
	free( d );
	return hsh;
}

static int g_setup_changed, g_copied;

/* Copy the tree under src (relative path rel) over the game folder. */
static void copy_tree( const char *src, const char *rel )
{
	char pat[MAX_PATH], s[MAX_PATH], r[MAX_PATH], d[MAX_PATH];
	WIN32_FIND_DATAA fd;
	HANDLE h;
	join( pat, src, "*" );
	if( (h = FindFirstFileA(pat, &fd)) == INVALID_HANDLE_VALUE )
		return;
	do
	{
		if( strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0 )
			continue;
		join( s, src, fd.cFileName );
		if( *rel ) join( r, rel, fd.cFileName ); else snprintf( r, MAX_PATH, "%s", fd.cFileName );
		join( d, g_root, r );
		if( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
		{
			CreateDirectoryA( d, NULL );
			copy_tree( s, r );
			continue;
		}
		if( _strnicmp(r, "setup\\", 6) == 0 )
		{
			int e1, e2;
			uint64_t a = content_hash( s, &e1 ), b = content_hash( d, &e2 );
			if( !e2 || a != b )
				g_setup_changed = 1;
		}
		if( _stricmp(r, "Updater.exe") == 0 )
		{
			/* this program is running: Windows lets it be renamed, not replaced */
			char old[MAX_PATH];
			join( old, g_root, "Updater.exe.old" );
			MoveFileExA( d, old, MOVEFILE_REPLACE_EXISTING );
		}
		SetFileAttributesA( d, FILE_ATTRIBUTE_NORMAL );
		if( !CopyFileA(s, d, FALSE) )
			fail( "cannot write %s (is the game still open?)", d );
		++g_copied;
	} while( FindNextFileA(h, &fd) );
	FindClose( h );
}

static void delete_tree( const char *dir )
{
	char pat[MAX_PATH], p[MAX_PATH];
	WIN32_FIND_DATAA fd;
	HANDLE h;
	join( pat, dir, "*" );
	if( (h = FindFirstFileA(pat, &fd)) != INVALID_HANDLE_VALUE )
	{
		do
		{
			if( strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0 )
				continue;
			join( p, dir, fd.cFileName );
			if( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
				delete_tree( p );
			else
			{
				SetFileAttributesA( p, FILE_ATTRIBUTE_NORMAL );
				DeleteFileA( p );
			}
		} while( FindNextFileA(h, &fd) );
		FindClose( h );
	}
	RemoveDirectoryA( dir );
}

static int run( const char *exe, const char *args, int wait )
{
	char cmd[2 * MAX_PATH];
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	DWORD code = 1;
	snprintf( cmd, sizeof cmd, "\"%s\" %s", exe, args );
	memset( &si, 0, sizeof si );
	si.cb = sizeof si;
	if( !CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, g_root, &si, &pi) )
		return -1;
	if( wait )
	{
		WaitForSingleObject( pi.hProcess, INFINITE );
		GetExitCodeProcess( pi.hProcess, &code );
	}
	else
		code = 0;
	CloseHandle( pi.hThread );
	CloseHandle( pi.hProcess );
	return (int)code;
}

/* The game holds its program file open while it runs. */
static int game_running( void )
{
	char p[MAX_PATH];
	HANDLE h;
	join( p, g_root, PROGRAM );
	h = CreateFileA( p, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL );
	if( h == INVALID_HANDLE_VALUE )
		return GetLastError() == ERROR_SHARING_VIOLATION;
	CloseHandle( h );
	return 0;
}

int main( int argc, char **argv )
{
	char p[MAX_PATH], have[64], tag[64], url[2048], zip[MAX_PATH], dll[MAX_PATH], name[MAX_PATH];
	const char *api = getenv( "DSF_UPDATE_API" ), *at;
	char *json = NULL;
	size_t len = 0;
	int i, found = 0, had_core;
	FILE *f;

	for( i = 1; i < argc; ++i )
	{
		if( strcmp(argv[i], "--check") == 0 ) g_check = 1;
		else if( strcmp(argv[i], "--yes") == 0 ) g_yes = 1;
		else if( strcmp(argv[i], "--restart") == 0 ) g_restart = 1;
	}
	GetModuleFileNameA( NULL, g_root, sizeof g_root );
	*strrchr( g_root, '\\' ) = 0;
	join( p, g_root, "Updater.exe.old" );
	DeleteFileA( p );
	join( p, g_root, PROGRAM );
	if( GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES )
		fail( "put Updater.exe in your Dancing Stage Fusion PC folder (beside %s) and run it there.", PROGRAM );

	if( !g_check )
		printf( "Dancing Stage Fusion PC - Updater\n\n" );
	installed_version( have, sizeof have );
	if( !http_get(api && *api ? api : API_URL, NULL, &json, &len, 0, 1) )
		fail( "%s", "cannot reach GitHub to look for updates. Check your internet connection." );
	if( !json_string(json, "tag_name", tag, sizeof tag, NULL) )
		fail( "%s", "GitHub's answer has no version in it." );
	for( at = json; json_string(at, "browser_download_url", url, sizeof url, &at); )
	{
		const char *base = strrchr( url, '/' );
		base = base ? base + 1 : url;
		if( strncmp(base, "DancingStageFusionPC-", 21) == 0 && strlen(base) > 4 &&
		    _stricmp(base + strlen(base) - 4, ".zip") == 0 && !strstr(base, "Updater") )
		{
			snprintf( name, sizeof name, "%s", base );
			found = 1;
			break;
		}
	}
	if( !newer(tag, have) )
	{
		if( g_check ) { printf( "LATEST %s\n", have ); exit( 1 ); }
		printf( "You have the latest version (%s).\n", have );
		finish( 0 );
	}
	if( !found )
		fail( "%s", "the newest release on GitHub has no game download yet. Try again later." );
	if( g_check ) { printf( "UPDATE %s\n", tag + (tag[0] == 'v') ); exit( 0 ); }

	printf( "Installed: %s\nOn GitHub: %s\n\n", have, tag + (tag[0] == 'v') );
	if( !g_yes )
	{
		char line[16];
		printf( "Update now? Your disc image, game core, songs, saves and settings stay. [Y/n] " );
		fflush( stdout );
		if( !fgets(line, sizeof line, stdin) || line[0] == 'n' || line[0] == 'N' )
			finish( 0 );
	}
	for( i = 0; game_running(); ++i )
	{
		if( i == 0 ) printf( "Waiting for the game to close...\n" );
		if( i > 60 ) fail( "%s", "the game is still open. Close it and run Updater.exe again." );
		Sleep( 500 );
	}

	join( g_stage, g_root, "setup\\update" );
	delete_tree( g_stage );
	join( p, g_root, "setup" );
	CreateDirectoryA( p, NULL );
	CreateDirectoryA( g_stage, NULL );
	join( zip, g_stage, name );
	if( !(f = fopen(zip, "wb")) )
		fail( "cannot write %s", zip );
	printf( "%s\n", name );
	if( !http_get(url, f, NULL, NULL, 1, 0) )
	{
		fclose( f );
		delete_tree( g_stage );
		fail( "%s", "the download did not finish. Check your internet connection and try again." );
	}
	fclose( f );
	printf( "  Unpacking...\n" );
	join( p, g_stage, "new" );
	CreateDirectoryA( p, NULL );
	if( !unzip(zip, p) )
	{
		delete_tree( g_stage );
		fail( "%s", "the download is damaged. Try again." );
	}
	join( dll, g_root, "dsfcore.dll" );
	had_core = GetFileAttributesA( dll ) != INVALID_FILE_ATTRIBUTES;
	printf( "  Installing...\n" );
	copy_tree( p, "" );
	delete_tree( g_stage );
	printf( "  %d files updated.\n\n", g_copied );

	join( p, g_root, "Setup.exe" );
	if( had_core && g_setup_changed )
	{
		printf( "The game core is built differently in this version: running Setup again.\n\n" );
		if( run(p, "--no-pause", 1) != 0 )
			fail( "%s", "Setup did not finish (see above). Your old game core is still in place: run Setup.exe again." );
	}
	else if( had_core )
		run( p, "--icon", 1 );
	else
		printf( "Run Setup.exe next to build the game from your disc.\n" );

	printf( "\nUpdated to %s.\n", tag + (tag[0] == 'v') );
	if( g_restart && had_core )
	{
		join( p, g_root, PROGRAM );
		run( p, "", 0 );
		exit( 0 );
	}
	finish( 0 );
	return 0;
}
