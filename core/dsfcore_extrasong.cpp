/* dsfcore_extrasong.cpp -- extra songs in Dancing Stage Fusion's own song
 * list, played by the game's own code like its 20 songs.
 *
 * The game keeps its songs in a table of 20 44-byte records at 0x800AFE38,
 * used only through a small module of accessors (0x800759A0..0x80075EAC:
 * count, by index / number / id, sort ...) with the base and the count 20
 * as constants.  A record holds the song's master-table id, its number, its
 * audio stream (1-based index into the 16-byte stream table at 0x800932B8,
 * read by select_stream 0x8002CA40), its preview (an id - 24 into the XA
 * table at 0x800934E4; the sound driver takes ids up to 49, 0x800296E0), its difficulty levels, and a group of six pointers
 * (JA, EN, FR, DE, IT, ES) to {size, lba} READ_DT entries holding its
 * charts, dancer timeline, background and language UI.  The music select
 * finds the song's banner and title plate in its pack through a 20-entry
 * {id, banner, plate} table at 0x800AA154 (0x80050BAC).  Scores are kept
 * per song index in save slots for up to 32 songs.  notes/dsf-extra-songs.md.
 *
 * Once the executable is in RAM, those tables are copied into enhancement
 * memory with the extra songs after the stock entries, and the constants
 * that hold their bases and counts are patched to them; the extra songs'
 * names go into blank slots of the master table and the READ_DT entries
 * rebuilt with their graphics (music select, results, records) replace the
 * stock ones in the table of contents.  Everything the songs bring -- data,
 * audio, the list in extrasongs.txt -- comes from a separate local package
 * made by port/tools/extrasong/build_extrasong.py from the player's own
 * files and disc (never part of this project); it writes the data into
 * free space of the disc image with disc overlays and selects this plugin.
 */
#include "mod_plugins.h"
#include "cpu_state.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

uint32_t rd32( uint32_t a ) { return psx_mod_read_word( a ); }
void wr32( uint32_t a, uint32_t v ) { psx_mod_write_word( a, v ); }
void wr8( uint32_t a, uint8_t v ) { psx_mod_write_byte( a, v ); }

const uint32_t SONGTAB = 0x800AFE38, SONG_SIZE = 44, NUM_STOCK = 20;
const uint32_t STRTAB = 0x800932B8, STR_SIZE = 16, NUM_STOCK_STR = 23;
const uint32_t GFXTAB = 0x800AA154, GFX_SIZE = 12;
const uint32_t XATAB = 0x800934E4, XA_SIZE = 12, NUM_STOCK_XA = 26;	/* ids 24..49 */
const uint32_t MASTER = 0x800A3EA0, MASTER_SIZE = 20, NUM_MASTER = 0x252;
const uint32_t TOC = 0x80092B50, NUM_TOC = 235;
const int MAX_EXTRA = 8;

/* Constants of the code that uses each table: lui/addiu pairs holding the
 * base, and loop bounds / counts holding 20. */
typedef uint32_t Pair[2];
const Pair SONG_PAIRS[] = {
	{ 0x800759E0, 0x800759E4 }, { 0x80075B18, 0x80075B1C }, { 0x80075B4C, 0x80075B54 },
	{ 0x80075B58, 0x80075B60 }, { 0x80075B64, 0x80075B6C }, { 0x80075B8C, 0x80075B90 },
	{ 0x80075BA8, 0x80075BAC }, { 0x80075BE8, 0x80075BEC }, { 0x80075E78, 0x80075E7C },
};
const uint32_t SONG_COUNTS[] = {
	0x80075A30, 0x80075A74, 0x80075B38, 0x80075B70, 0x80075BC8, 0x80075BE0, 0x80075BFC, 0x80075E94,
};
const Pair GFX_PAIRS[] = { { 0x80050BCC, 0x80050BD0 }, { 0x80050BEC, 0x80050BF0 } };
const uint32_t GFX_COUNTS[] = { 0x80050C24 };
const Pair STR_PAIRS[] = { { 0x8002CA44, 0x8002CA48 } };	/* the base is the table - 16 (1-based) */
const Pair XA_PAIRS[] = { { 0x80028E50, 0x80028E58 }, { 0x8002E26C, 0x8002E27C } };
/* The sound driver takes music ids 1..49 only: sltiu v0,(id-1),0x31. */
const uint32_t MUSIC_ID_LIMIT = 0x800296E0, MUSIC_ID_LIMIT_STOCK = 0x2C420031;

/* Stock words that tell the executable is in RAM and not yet patched.  The
 * executable arrives from the disc over several frames in address order, so
 * the last three (the last TOC entry, the last song record, the image's last
 * non-zero word) keep the tables from being copied or patched before they
 * are loaded and then overwritten by the rest of the load. */
const uint32_t STOCK_CHECK[][2] = {
	{ 0x80075BDC, 0x03E00008 }, { 0x80075BE0, 0x24020014 },
	{ 0x8002CA48, 0x244232A8 }, { 0x80028E58, 0x24C634E4 }, { 0x8002E27C, 0x248434E4 },
	{ 0x80050C24, 0x2CA20014 }, { MUSIC_ID_LIMIT, MUSIC_ID_LIMIT_STOCK },
	{ 0x800932A4, 0x00006F91 }, { 0x800B017C, 0x00008015 }, { 0x800B4C68, 0x80023944 },
};

struct Song
{
	std::string title, artist;
	uint32_t record[11];
	uint32_t stream[4];
	uint32_t entries[5][2];	/* {size, lba}: en, fr, de, it, es */
	uint32_t gfx[2];	/* banner and title plate items of the music-select pack */
	uint32_t xa[3];		/* the preview's XA table entry */
};
std::vector<Song> g_songs;
std::vector<std::pair<uint32_t, std::pair<uint32_t, uint32_t> > > g_toc;	/* entry -> {size, lba} */
bool g_loaded = false;

/* Enhancement memory, for up to MAX_EXTRA extra songs. */
uint32_t g_mem = 0;
const uint32_t M_SONGS = 0x000;		/* 28 x 44 */
const uint32_t M_STREAMS = 0x500;	/* 31 x 16 */
const uint32_t M_GFX = 0x700;		/* 28 x 12 */
const uint32_t M_GROUPS = 0x880;	/* 8 x 24 */
const uint32_t M_ENTRIES = 0x980;	/* 8 x 40 */
const uint32_t M_XA = 0xB00;		/* 34 x 12 */
const uint32_t M_STRINGS = 0xD00;	/* 8 x 96 */
const uint32_t M_SIZE = 0x1000;

uint32_t hi16( uint32_t a ) { return (a + 0x8000u) >> 16; }
uint32_t lo16( uint32_t a ) { return a & 0xFFFFu; }

void patch_base( const Pair *pairs, size_t n, uint32_t base )
{
	for( size_t i = 0; i < n; ++i )
	{
		psx_mod_write_code_word( pairs[i][0], (rd32(pairs[i][0]) & 0xFFFF0000u) | hi16(base) );
		psx_mod_write_code_word( pairs[i][1], (rd32(pairs[i][1]) & 0xFFFF0000u) | lo16(base) );
	}
}

void patch_count( const uint32_t *sites, size_t n, uint32_t count )
{
	for( size_t i = 0; i < n; ++i )
		psx_mod_write_code_word( sites[i], (rd32(sites[i]) & 0xFFFF0000u) | count );
}

void copy_words( uint32_t to, uint32_t from, uint32_t bytes )
{
	for( uint32_t i = 0; i < bytes; i += 4 )
		wr32( to + i, rd32(from + i) );
}

uint32_t put_string( uint32_t at, const std::string &s, uint32_t room )
{
	uint32_t n = 0;
	for( ; n < s.size() && n + 1 < room; ++n )
		wr8( at + n, uint8_t(s[n]) );
	wr8( at + n, 0 );
	return at;
}

/* extrasongs.txt: "song TITLE" starts a song; "artist", "record" (11 words),
 * "stream" (4), "entries" (5 x size lba), "gfx" (2), "xa" (3), "end";
 * "toc ENTRY SIZE LBA" lines replace table-of-contents entries.  Hex. */
std::vector<uint32_t> hex_words( const char *s )
{
	std::vector<uint32_t> out;
	char *end = NULL;
	for( ;; )
	{
		const unsigned long v = strtoul( s, &end, 16 );
		if( end == s )
			break;
		out.push_back( uint32_t(v) );
		s = end;
	}
	return out;
}

void load_songs()
{
	if( g_loaded )
		return;
	g_loaded = true;
	char dir[1024];
	if( !psx_mod_current_resource_path("data", dir, sizeof(dir)) )
		return;
	const std::string path = std::string( dir ) + "/extrasongs.txt";
	FILE *f = fopen( path.c_str(), "r" );
	if( !f )
	{
		psx_mod_counter_add( "dsf.extrasong.no_list", 1 );
		return;
	}
	char line[1024];
	Song s = Song();
	bool in_song = false, complete = true;
	while( fgets(line, sizeof(line), f) )
	{
		char *nl = strpbrk( line, "\r\n" );
		if( nl ) *nl = 0;
		const char *sp = strchr( line, ' ' );
		const std::string key( line, sp? size_t(sp - line): strlen(line) );
		const char *rest = sp? sp + 1: "";
		const std::vector<uint32_t> w = hex_words( rest );
		if( key == "song" ) { s = Song(); s.title = rest; in_song = true; complete = true; }
		else if( !in_song && key == "toc" && w.size() == 3 ) g_toc.push_back( std::make_pair(w[0], std::make_pair(w[1], w[2])) );
		else if( !in_song ) continue;
		else if( key == "artist" ) s.artist = rest;
		else if( key == "record" && w.size() == 11 ) memcpy( s.record, w.data(), sizeof(s.record) );
		else if( key == "stream" && w.size() == 4 ) memcpy( s.stream, w.data(), sizeof(s.stream) );
		else if( key == "entries" && w.size() == 10 ) memcpy( s.entries, w.data(), sizeof(s.entries) );
		else if( key == "gfx" && w.size() == 2 ) memcpy( s.gfx, w.data(), sizeof(s.gfx) );
		else if( key == "xa" && w.size() == 3 ) memcpy( s.xa, w.data(), sizeof(s.xa) );
		else if( key == "end" )
		{
			if( complete && g_songs.size() < size_t(MAX_EXTRA) )
				g_songs.push_back( s );
			in_song = false;
		}
		else if( key != "#" && key[0] != '#' )
			complete = false;
	}
	fclose( f );
	psx_mod_counter_add( "dsf.extrasong.songs", uint32_t(g_songs.size()) );
}

bool stock_code_present()
{
	for( const auto &c : STOCK_CHECK )
		if( rd32(c[0]) != c[1] )
			return false;
	return true;
}

void install()
{
	load_songs();
	if( g_songs.empty() )
		return;
	if( g_mem == 0 )
		g_mem = psx_mod_alloc_guest_memory( M_SIZE, 16 );
	if( g_mem == 0 )
		return;
	const uint32_t songs = g_mem + M_SONGS, streams = g_mem + M_STREAMS, gfx = g_mem + M_GFX;
	const uint32_t xa = g_mem + M_XA;
	const uint32_t n = NUM_STOCK + uint32_t(g_songs.size());

	copy_words( songs, SONGTAB, NUM_STOCK * SONG_SIZE );
	copy_words( streams, STRTAB, NUM_STOCK_STR * STR_SIZE );
	copy_words( gfx, GFXTAB, NUM_STOCK * GFX_SIZE );
	copy_words( xa, XATAB, NUM_STOCK_XA * XA_SIZE );

	/* blank master-table slots (no short name) take the extra songs' names */
	uint32_t blank = 0;
	for( size_t k = 0; k < g_songs.size(); ++k )
	{
		const Song &s = g_songs[k];
		const uint32_t idx = NUM_STOCK + uint32_t(k);
		const uint32_t rec = songs + idx * SONG_SIZE;
		for( int i = 0; i < 11; ++i )
			wr32( rec + 4u*uint32_t(i), s.record[i] );
		const uint32_t g = g_mem + M_GROUPS + uint32_t(k) * 24, e = g_mem + M_ENTRIES + uint32_t(k) * 40;
		for( int l = 0; l < 5; ++l )
		{
			wr32( e + 8u*uint32_t(l), s.entries[l][0] );
			wr32( e + 8u*uint32_t(l) + 4, s.entries[l][1] );
			wr32( g + 4u + 4u*uint32_t(l), e + 8u*uint32_t(l) );
		}
		wr32( g, e );		/* JA: the English data, as the stock songs */
		wr32( rec + 36, g );
		const uint32_t st = streams + (s.record[2] - 1) * STR_SIZE;
		for( int i = 0; i < 4; ++i )
			wr32( st + 4u*uint32_t(i), s.stream[i] );
		const uint32_t gf = gfx + idx * GFX_SIZE;
		wr32( gf, s.record[0] );
		wr32( gf + 4, s.gfx[0] );
		wr32( gf + 8, s.gfx[1] );
		const uint32_t xe = xa + (s.record[3] - 24) * XA_SIZE;
		for( int i = 0; i < 3; ++i )
			wr32( xe + 4u*uint32_t(i), s.xa[i] );

		while( blank < NUM_MASTER && rd32(MASTER + blank * MASTER_SIZE + 4) != 0 &&
		       rd32(MASTER + blank * MASTER_SIZE) != s.record[0] )
			++blank;
		if( blank < NUM_MASTER )
		{
			const uint32_t m = MASTER + blank * MASTER_SIZE;
			const uint32_t str = g_mem + M_STRINGS + uint32_t(k) * 96;
			std::string shortname = s.title.substr( 0, 4 );
			for( char &c : shortname ) c = char( tolower((unsigned char) c) );
			wr32( m, s.record[0] );
			wr32( m + 4, put_string(str, shortname, 8) );
			wr32( m + 8, put_string(str + 8, s.title, 56) );
			wr32( m + 12, 0 );
			wr32( m + 16, put_string(str + 64, s.artist, 32) );
			++blank;
		}
	}
	for( const auto &t : g_toc )
		if( t.first < NUM_TOC )
		{
			wr32( TOC + 8 * t.first, t.second.first );
			wr32( TOC + 8 * t.first + 4, t.second.second );
		}

	patch_base( SONG_PAIRS, sizeof(SONG_PAIRS)/sizeof(SONG_PAIRS[0]), songs );
	patch_count( SONG_COUNTS, sizeof(SONG_COUNTS)/sizeof(SONG_COUNTS[0]), n );
	patch_base( GFX_PAIRS, sizeof(GFX_PAIRS)/sizeof(GFX_PAIRS[0]), gfx );
	patch_count( GFX_COUNTS, sizeof(GFX_COUNTS)/sizeof(GFX_COUNTS[0]), n );
	patch_base( STR_PAIRS, sizeof(STR_PAIRS)/sizeof(STR_PAIRS[0]), streams - STR_SIZE );
	patch_base( XA_PAIRS, sizeof(XA_PAIRS)/sizeof(XA_PAIRS[0]), xa );
	uint32_t max_id = 0x31;
	for( const Song &s : g_songs )
		if( s.record[3] > max_id )
			max_id = s.record[3];
	psx_mod_write_code_word( MUSIC_ID_LIMIT, (MUSIC_ID_LIMIT_STOCK & 0xFFFF0000u) | max_id );
	psx_mod_counter_add( "dsf.extrasong.installed", 1 );
}

void on_vblank()
{
	if( stock_code_present() )
		install();
}

} // namespace

PSX_MOD_CONSTRUCTOR( dsf_register_extrasong )
{
	(void) psx_mod_register_vblank_plugin( "dsf.extrasong", on_vblank );
}
