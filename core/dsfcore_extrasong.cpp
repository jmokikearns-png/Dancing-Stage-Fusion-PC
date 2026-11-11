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
 * stock ones in the table of contents.  Everything the songs bring comes
 * from a separate local package made by port/tools/extrasong/build_extrasong.py
 * from the player's own files and disc (never part of this project):
 * extrasongs.txt (the list), gfx.bin (pictures) and media.bin, the new disc
 * sectors, which activation appends after the end of the mounted disc.
 *
 * The menus' packs hold a 16-byte marker for each extra picture: every pack
 * item goes through the game's LZ decoder (0x8002E6F0), which is replaced by
 * LZ_GUEST, the same decoder that serves a marker's picture from gfx.bin.
 * That keeps the packs, and the RAM they are loaded to, at the stock size.
 *
 * The music select lists a song when 0x80050C40 accepts it (in DOUBLE mode
 * only songs with double charts); AVAIL_GUEST adds the unlock condition.
 * Its arrows are drawn by 0x80027C84 from 0x80050364, which calls NEW_GUEST
 * instead: with a newly unlocked song under the cursor that has never been
 * played, it also draws the game's unused "New!!" picture from the same
 * sheet above the right arrow, like the Deluxe disc.
 *
 * Workout results show each song's 176x55 banner (READ_DT e181, item idx+1)
 * from a VRAM grid: x = 640 + 64*(idx/4), y = 256 + 55*(idx%4), CLUT at
 * (640, 476+idx), computed by 0x8008CE50 for drawing and inline by the
 * loader at 0x8008A9A0.  Index 20 would land on the font page at x = 960,
 * so both go to WORKOUT_SLOT, a plugin-defined guest function that keeps the
 * stock grid for the 20 songs.  That screen has room for 8 more banners, so
 * when it loads they go to the extra songs of the workout history (a ring of
 * 50 {.., number at +8} entries at 0x800DE67C, head 0x800DE668, count
 * 0x800DE66C), most recent first, after the song just played (not yet in
 * the history when the banners load); any other extra song loads a 2x1 stand-in
 * into an unused corner.
 *
 * RECORDS (an overlay at 0x800E0000) loads every song's 88x28 mini banner
 * in a loop at 0x800E8F78 into 20-byte slots at [0x800F2140] + 0x7C, count
 * at +0x2FC, and stores each next id before testing it for the table's end:
 * with 32 songs that end mark lands on the count and empties the list, so
 * no banner or title shows.  When the loop decodes its first banner,
 * on_lz moves that store after the test.  The loop places banner n at
 * x = 832 + 21*(n%3), y = 256 + 26*(n/3), which past n = 26 runs into the
 * palettes at y = 496 and off VRAM; on_lz moves those to the free page at
 * x = 960, y = 0.
 */
#include "mod_plugins.h"
#include "mod_packages.h"
#include "cpu_state.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace {

uint32_t rd32( uint32_t a ) { return psx_mod_read_word( a ); }
uint8_t rd8( uint32_t a ) { return psx_mod_read_byte( a ); }
uint32_t rd32u( uint32_t a ) { return rd8(a) | uint32_t(rd8(a + 1)) << 8 | uint32_t(rd8(a + 2)) << 16 | uint32_t(rd8(a + 3)) << 24; }
void wr32( uint32_t a, uint32_t v ) { psx_mod_write_word( a, v ); }
void wr8( uint32_t a, uint8_t v ) { psx_mod_write_byte( a, v ); }

const uint32_t SONGTAB = 0x800AFE38, SONG_SIZE = 44, NUM_STOCK = 20;
const int32_t NUM_STOCK_INT = 20;
const uint32_t STRTAB = 0x800932B8, STR_SIZE = 16, NUM_STOCK_STR = 23;
const uint32_t GFXTAB = 0x800AA154, GFX_SIZE = 12;
const uint32_t XATAB = 0x800934E4, XA_SIZE = 12, NUM_STOCK_XA = 26;	/* ids 24..49 */
const uint32_t MASTER = 0x800A3EA0, MASTER_SIZE = 20, NUM_MASTER = 0x252;
const uint32_t TOC = 0x80092B50, NUM_TOC = 235;
const int MAX_EXTRA = 12;		/* the save keeps records for 32 songs */

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
/* The sound driver plays music ids 1..49 only: sltiu v0,(id-1),0x31. */
const uint32_t MUSIC_ID_LIMIT = 0x800296E0, MUSIC_ID_LIMIT_STOCK = 0x2C420031;

/* Replaced functions (see the header). */
const uint32_t LZ_FN = 0x8002E6F0, LZ_GUEST = 0x8F000020;
/* The sound driver plays music ids below 25 as STR streams (by number in
 * the stream table) and the others as XA (0x8002C398); KIND_GUEST adds the
 * extra songs' stream ids, which follow their preview ids. */
const uint32_t KIND_FN = 0x8002C398, KIND_GUEST = 0x8F000040;
const uint32_t MAX_STREAM_ID = 80;
const uint32_t AVAIL_FN = 0x80050C40, AVAIL_GUEST = 0x8F000030;
const uint32_t PLAY_STYLE = 0x800BA328;	/* byte; 2 = DOUBLE */
/* NEW!! (see the header): the music select's right arrow, drawn by
 * SPRT(x, y, w, h, u, v, clut x, clut y, colour, ot) = 0x80027C84 from
 * 0x80050364 (u 52 is the right arrow); the unused "New!!" picture is on the
 * same sheet at u 8, v 96, 64x25.  The list is at [0x800A9E10], the song
 * under the cursor at +0x18 + 4 * [+0x14]. */
const uint32_t SPRT_FN = 0x80027C84, ARROW_CALL = 0x80050364, NEW_GUEST = 0x8F000050;
const uint32_t SELECT_LIST = 0x800A9E10;
const uint32_t SAVE_PTR = 0x800B1AFC;	/* -> the save: song records at +0x90, 0x50 bytes each */
const int SAVE_RECORDS = 32;
const uint32_t MARKER_WORD = 0x58465344;	/* "DSFX" */

/* RECORDS overlay: its mini-banner loop (see the header). */
const uint32_t REC_LZ_RA = 0x800E90C4, REC_OBJ = 0x800F2140;
const uint32_t REC_SLOTS = 0x7C, REC_SLOT_SIZE = 20, REC_COUNT = 0x2FC;
const uint32_t REC_STORE = 0x800E8FA4, REC_LOAD = 0x800E8FA8, REC_SPARE = 0x800E8FAC;
const uint32_t REC_STOCK[][2] = {
	{ REC_STORE, 0xAE020000 },	/* sw v0, 0(s0): the id, in the end test's delay slot */
	{ REC_LOAD, 0x8E22FFFC },	/* lw v0, -4(s1) */
	{ REC_SPARE, 0x00000000 },	/* nop (load delay) */
	{ 0x800E90BC, 0x0C00B9BC },	/* jal 0x8002E6F0 */
};
const uint32_t REC_FIRST_MOVED = 27;	/* banners from here on go to the free page */
const uint16_t REC_FREE_X = 960, REC_FREE_Y = 0;

/* Workout results banner slots (see the header). */
const uint32_t WORKOUT_SLOT_FN = 0x8008CE50, WORKOUT_LOADER = 0x8008A9A0, WORKOUT_SLOT = 0x8F000010;
const uint32_t WORKOUT_LOADER_RA = WORKOUT_LOADER + 7 * 4;	/* after the patched block's jal + delay slot */
const uint32_t HISTORY = 0x800DE668, HISTORY_ROWS = 0x800DE67C, HISTORY_SIZE = 50;
/* The song just played (player 1's song number): the results screen loads
 * its banners before 0x8008FC5C adds that song to the history. */
const uint32_t CURRENT_SONG = 0x800BA32A;
/* Free on that screen (VRAM map of the workout results, 2026-10-17); each
 * keeps the 176-pixel banner inside one texture page (x%64 <= 20). */
const uint16_t EXTRA_WORKOUT_SLOTS[][2] = {
	{ 768, 0 }, { 768, 55 }, { 768, 110 }, { 768, 165 },
	{ 847, 156 }, { 896, 50 }, { 896, 105 }, { 896, 160 },
};
const int NUM_WORKOUT_SLOTS = int( sizeof(EXTRA_WORKOUT_SLOTS) / sizeof(EXTRA_WORKOUT_SLOTS[0]) );
const uint16_t WORKOUT_SCRATCH[2] = { 1000, 511 };	/* unused on that screen */
inline uint32_t jal( uint32_t target ) { return 0x0C000000u | ((target >> 2) & 0x03FFFFFFu); }
inline uint32_t j( uint32_t target ) { return 0x08000000u | ((target >> 2) & 0x03FFFFFFu); }
void patch_workout_slots();

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
	{ WORKOUT_SLOT_FN, 0x04810002 }, { WORKOUT_LOADER, 0x06010002 }, { 0x8008A9F0, 0xAFA20034 },
	{ LZ_FN, 0x00A05021 }, { LZ_FN + 4, 0x90890000 }, { KIND_FN, 0x3084FFFF }, { KIND_FN + 4, 0x2C840019 }, { AVAIL_FN, 0x3C02800C }, { AVAIL_FN + 4, 0x8043A328 },
};

struct Song
{
	std::string title, artist;
	uint32_t record[11];
	uint32_t stream[4];
	uint32_t entries[5][2];	/* {size, lba}: en, fr, de, it, es */
	uint32_t gfx[2];	/* banner and title plate items of the music-select pack */
	uint32_t xa[3];		/* the preview's XA table entry */
	uint32_t workout;	/* gfx.bin picture of the workout-results banner */
	uint32_t unlock_id, unlock_mask;	/* listed once that song is cleared there (0: always) */
};
struct Blob { uint32_t offset, size; };
std::vector<Song> g_songs;
std::vector<std::pair<uint32_t, std::pair<uint32_t, uint32_t> > > g_toc;	/* entry -> {size, lba} */
std::vector<Blob> g_blobs;
std::vector<uint8_t> g_gfx;
uint32_t g_media_base = 0, g_media_sectors = 0;
int32_t g_delta = 0;		/* where the media landed minus where the list expects it */
bool g_loaded = false, g_media_ok = false;

/* Enhancement memory, for up to MAX_EXTRA extra songs. */
uint32_t g_mem = 0;
const uint32_t M_SONGS = 0x000;		/* 32 x 44 */
const uint32_t M_STREAMS = 0x580;	/* MAX_STREAM_ID x 16 */
const uint32_t M_GFX = 0xA80;		/* 32 x 12 */
const uint32_t M_GROUPS = 0xC00;	/* 12 x 24 */
const uint32_t M_ENTRIES = 0xD20;	/* 12 x 40 */
const uint32_t M_XA = 0xF00;		/* 38 x 12 */
const uint32_t M_STRINGS = 0x10D0;	/* 12 x 96 */
const uint32_t M_SIZE = 0x1580;

/* Workout slot assignment for the current visit of the screen. */
int g_slot_song[NUM_WORKOUT_SLOTS];	/* extra song index per slot, -1 free */
int32_t g_stand_in_blob = -1;		/* the next decode of this picture is the 2x1 stand-in */

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

/* extrasongs.txt: "media BASE SECTORS", "blob K OFFSET SIZE" (gfx.bin);
 * "song TITLE" starts a song; "artist", "record" (11 words), "stream" (4),
 * "entries" (5 x size lba), "gfx" (2), "xa" (3), "workout" (blob),
 * optionally "unlock ID MASK", then "end"; "toc ENTRY SIZE LBA" lines
 * replace table-of-contents entries.  Hex; LBAs as if the media started at
 * BASE. */
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

bool read_file( const std::string &path, std::vector<uint8_t> &out )
{
	FILE *f = fopen( path.c_str(), "rb" );
	if( !f )
		return false;
	uint8_t buf[65536];
	size_t n;
	while( (n = fread(buf, 1, sizeof(buf), f)) > 0 )
		out.insert( out.end(), buf, buf + n );
	fclose( f );
	return true;
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
		else if( !in_song && key == "media" && w.size() == 2 ) { g_media_base = w[0]; g_media_sectors = w[1]; }
		else if( !in_song && key == "blob" && w.size() == 3 && w[0] == g_blobs.size() ) g_blobs.push_back( Blob{w[1], w[2]} );
		else if( !in_song && key == "toc" && w.size() == 3 ) g_toc.push_back( std::make_pair(w[0], std::make_pair(w[1], w[2])) );
		else if( !in_song ) continue;
		else if( key == "artist" ) s.artist = rest;
		else if( key == "record" && w.size() == 11 ) memcpy( s.record, w.data(), sizeof(s.record) );
		else if( key == "stream" && w.size() == 4 ) memcpy( s.stream, w.data(), sizeof(s.stream) );
		else if( key == "entries" && w.size() == 10 ) memcpy( s.entries, w.data(), sizeof(s.entries) );
		else if( key == "gfx" && w.size() == 2 ) memcpy( s.gfx, w.data(), sizeof(s.gfx) );
		else if( key == "xa" && w.size() == 3 ) memcpy( s.xa, w.data(), sizeof(s.xa) );
		else if( key == "workout" && w.size() == 1 ) s.workout = w[0];
		else if( key == "unlock" && w.size() == 2 ) { s.unlock_id = w[0]; s.unlock_mask = w[1]; }
		else if( key == "end" )
		{
			if( complete && g_songs.size() < size_t(MAX_EXTRA) && s.record[2] >= 1 &&
			    s.record[2] <= MAX_STREAM_ID && s.record[3] >= 24 && s.record[3] < 24 + 38 )
				g_songs.push_back( s );
			in_song = false;
		}
		else if( key != "#" && key[0] != '#' )
			complete = false;
	}
	fclose( f );
	if( !read_file(std::string(dir) + "/gfx.bin", g_gfx) )
		g_blobs.clear();
	for( const Blob &b : g_blobs )
		if( uint64_t(b.offset) + b.size > g_gfx.size() )
		{
			g_blobs.clear();
			break;
		}
	if( g_blobs.empty() )
	{
		psx_mod_counter_add( "dsf.extrasong.no_pictures", 1 );
		g_songs.clear();
	}
	psx_mod_counter_add( "dsf.extrasong.songs", uint32_t(g_songs.size()) );
}

/* Activation: mount the songs' sectors after the disc. */
void on_activate()
{
	g_loaded = false;
	g_media_ok = false;
	g_songs.clear(); g_toc.clear(); g_blobs.clear(); g_gfx.clear();
	g_media_base = g_media_sectors = 0;
	load_songs();
	uint32_t lba = 0;
	if( g_media_sectors && psx_mod_append_disc_extent("media", 0, g_media_sectors, &lba) )
	{
		g_delta = int32_t( lba - g_media_base );
		g_media_ok = true;
	}
	else
		psx_mod_counter_add( "dsf.extrasong.no_media", 1 );
}

uint32_t moved( uint32_t lba ) { return uint32_t( int32_t(lba) + g_delta ); }
uint32_t moved_xa( uint32_t w ) { return (w & 0xFF000000u) | (moved(w & 0xFFFFFFu) & 0xFFFFFFu); }

bool stock_code_present()
{
	for( const auto &c : STOCK_CHECK )
		if( rd32(c[0]) != c[1] )
			return false;
	return true;
}

void install()
{
	if( g_songs.empty() || !g_media_ok )
		return;
	if( g_mem == 0 )
		g_mem = psx_mod_alloc_guest_memory( M_SIZE, 16 );
	if( g_mem == 0 )
		return;
	const uint32_t songs = g_mem + M_SONGS, streams = g_mem + M_STREAMS, gfx = g_mem + M_GFX;
	const uint32_t xa = g_mem + M_XA;
	const uint32_t n = NUM_STOCK + uint32_t(g_songs.size());

	copy_words( songs, SONGTAB, NUM_STOCK * SONG_SIZE );
	for( uint32_t a = 0; a < MAX_STREAM_ID * STR_SIZE; a += 4 )
		wr32( streams + a, 0 );
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
			wr32( e + 8u*uint32_t(l) + 4, moved(s.entries[l][1]) );
			wr32( g + 4u + 4u*uint32_t(l), e + 8u*uint32_t(l) );
		}
		wr32( g, e );		/* JA: the English data, as the stock songs */
		wr32( rec + 36, g );
		const uint32_t st = streams + (s.record[2] - 1) * STR_SIZE;
		wr32( st, moved(s.stream[0]) );
		for( int i = 1; i < 4; ++i )
			wr32( st + 4u*uint32_t(i), s.stream[i] );
		const uint32_t gf = gfx + idx * GFX_SIZE;
		wr32( gf, s.record[0] );
		wr32( gf + 4, s.gfx[0] );
		wr32( gf + 8, s.gfx[1] );
		const uint32_t xe = xa + (s.record[3] - 24) * XA_SIZE;
		wr32( xe, moved_xa(s.xa[0]) );
		wr32( xe + 4, moved_xa(s.xa[1]) );
		wr32( xe + 8, s.xa[2] );

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
			wr32( TOC + 8 * t.first + 4, moved(t.second.second) );
		}

	patch_base( SONG_PAIRS, sizeof(SONG_PAIRS)/sizeof(SONG_PAIRS[0]), songs );
	patch_count( SONG_COUNTS, sizeof(SONG_COUNTS)/sizeof(SONG_COUNTS[0]), n );
	patch_base( GFX_PAIRS, sizeof(GFX_PAIRS)/sizeof(GFX_PAIRS[0]), gfx );
	patch_count( GFX_COUNTS, sizeof(GFX_COUNTS)/sizeof(GFX_COUNTS[0]), n );
	patch_base( STR_PAIRS, sizeof(STR_PAIRS)/sizeof(STR_PAIRS[0]), streams - STR_SIZE );
	patch_base( XA_PAIRS, sizeof(XA_PAIRS)/sizeof(XA_PAIRS[0]), xa );
	/* every play command passes this check, streams and previews alike */
	uint32_t max_id = 0x31;
	for( const Song &s : g_songs )
		for( uint32_t id : { s.record[2], s.record[3] } )
			if( id > max_id )
				max_id = id;
	psx_mod_write_code_word( MUSIC_ID_LIMIT, (MUSIC_ID_LIMIT_STOCK & 0xFFFF0000u) | max_id );
	/* the LZ decoder and the music select's song filter: j GUEST; nop */
	psx_mod_write_code_word( LZ_FN, j(LZ_GUEST) );
	psx_mod_write_code_word( LZ_FN + 4, 0 );
	psx_mod_write_code_word( AVAIL_FN, j(AVAIL_GUEST) );
	psx_mod_write_code_word( AVAIL_FN + 4, 0 );
	psx_mod_write_code_word( KIND_FN, j(KIND_GUEST) );
	psx_mod_write_code_word( KIND_FN + 4, 0 );
	patch_workout_slots();
	if( rd32(ARROW_CALL) == jal(SPRT_FN) )
		psx_mod_write_code_word( ARROW_CALL, jal(NEW_GUEST) );
	psx_mod_counter_add( "dsf.extrasong.installed", 1 );
}

/* ---- LZ_GUEST(src, dst) -> bytes written: 0x8002E6F0's contract ---- */
const Blob *marker_blob( uint32_t src )
{
	if( rd32u(src) != MARKER_WORD )
		return NULL;
	const uint32_t k = rd32u( src + 4 );
	if( rd32u(src + 8) != (k ^ 0xFFFFFFFFu) || k >= g_blobs.size() || rd32u(src + 12) != g_blobs[k].size )
		return NULL;
	return &g_blobs[k];
}

/* A 4bpp TIM of 8x1 pixels with the picture's own CLUT, for an extra song
 * whose workout banner has no slot on this visit. */
uint32_t write_stand_in( const Blob &b, uint32_t dst )
{
	const uint8_t *t = g_gfx.data() + b.offset;
	uint32_t clut_block;
	memcpy( &clut_block, t + 8, 4 );
	uint32_t n = 0;
	for( ; n < 8 + clut_block; ++n )
		wr8( dst + n, t[n] );
	const uint8_t image[16] = { 16, 0, 0, 0,  0, 0, 0, 0,  2, 0, 1, 0,  0, 0, 0, 0 };	/* 12 + 4 bytes, w 2, h 1 */
	for( uint32_t i = 0; i < sizeof(image); ++i )
		wr8( dst + n + i, image[i] );
	return n + uint32_t( sizeof(image) );
}

/* Called from the RECORDS mini-banner loop, before it uploads banner n
 * from the slot s0 points to. */
void records_slot( CPUState *cpu )
{
	bool stock = true;
	for( const auto &c : REC_STOCK )
		stock = stock && rd32( c[0] ) == c[1];
	if( stock )
	{
		psx_mod_write_code_word( REC_STORE, 0x00404821 );	/* move t1, v0 */
		psx_mod_write_code_word( REC_SPARE, 0xAE090000 );	/* sw t1, 0(s0) */
	}
	const uint32_t obj = rd32( REC_OBJ );
	const uint32_t n = rd32( obj + REC_COUNT ) - 1;	/* counted before the decode */
	const uint32_t slot = cpu->gpr[16];
	if( n < REC_FIRST_MOVED || n >= 32 || slot != obj + REC_SLOTS + REC_SLOT_SIZE * n )
		return;
	const uint32_t k = n - REC_FIRST_MOVED;
	const uint16_t x = uint16_t( REC_FREE_X + 21 * (k % 3) ), y = uint16_t( REC_FREE_Y + 26 * (k / 3) );
	wr8( slot + 0xC, uint8_t(x) ); wr8( slot + 0xD, uint8_t(x >> 8) );
	wr8( slot + 0xE, uint8_t(y) ); wr8( slot + 0xF, uint8_t(y >> 8) );
}

void on_lz( CPUState *cpu, uint32_t )
{
	if( cpu->gpr[31] == REC_LZ_RA )
		records_slot( cpu );
	uint32_t src = cpu->gpr[4], dst = cpu->gpr[5];
	const uint32_t start = dst;
	if( const Blob *b = marker_blob(src) )
	{
		if( g_stand_in_blob >= 0 && b == &g_blobs[uint32_t(g_stand_in_blob)] )
			dst += write_stand_in( *b, dst );
		else
			for( uint32_t i = 0; i < b->size; ++i )
				wr8( dst++, g_gfx[b->offset + i] );
		g_stand_in_blob = -1;
		src += 16;
	}
	else
	{
		/* Konami LZ: a control byte gives 8 flags, LSB first; 0 copies a
		 * byte, 1 reads a code: 0xxxxxxx yyyyyyyy copies (x>>2)+3 bytes from
		 * ((x&3)<<8|y) back, 10llxxxx copies l+2 from x+1 back, 11xxxxxx
		 * copies x+8 bytes as they are, 0xFF ends. */
		for( ;; )
		{
			uint32_t ctrl = rd8( src++ );
			for( int bit = 0; bit < 8; ++bit, ctrl >>= 1 )
			{
				if( !(ctrl & 1) )
				{
					wr8( dst++, rd8(src++) );
					continue;
				}
				const uint32_t c = rd8( src++ );
				uint32_t len, from;
				if( !(c & 0x80) )
				{
					const uint32_t lo = rd8( src++ );
					len = (c >> 2) + 3;
					from = dst - (((c & 3) << 8) | lo);
				}
				else if( !(c & 0x40) )
				{
					len = ((c >> 4) & 3) + 2;
					from = dst - ((c & 0xF) + 1);
				}
				else if( c == 0xFF )
					goto done;
				else
				{
					for( uint32_t i = (c & 0x3F) + 8; i; --i )
						wr8( dst++, rd8(src++) );
					continue;
				}
				for( ; len; --len )
					wr8( dst++, rd8(from++) );
			}
		}
	}
done:
	cpu->gpr[4] = src;
	cpu->gpr[5] = dst;
	cpu->gpr[10] = start;
	cpu->gpr[2] = dst - start;
}

/* ---- KIND_GUEST(music id) -> 2 stream, 1 XA: 0x8002C398's contract ---- */
void on_kind( CPUState *cpu, uint32_t )
{
	const uint32_t id = cpu->gpr[4] & 0xFFFFu;
	bool stream = id < 25;
	cpu->gpr[4] = stream;
	for( const Song &s : g_songs )
		if( s.record[2] == id )
			stream = true;
	cpu->gpr[2] = stream? 2: 1;
}

/* ---- AVAIL_GUEST(record) -> listed: 0x80050C40's contract plus unlocks ---- */
bool unlocked( const Song &s );

void on_avail( CPUState *cpu, uint32_t )
{
	const uint32_t rec = cpu->gpr[4];
	const int32_t style = int8_t( rd8(PLAY_STYLE) );
	uint32_t listed = 1;
	if( style == 2 )
		listed = (rd32(rec + 0x14) & 0xF) != 0;
	if( listed )
		for( const Song &s : g_songs )
			if( s.record[0] == rd32(rec) && s.unlock_id && !unlocked(s) )
				listed = 0;
	cpu->gpr[3] = uint32_t( style );
	cpu->gpr[5] = listed;
	cpu->gpr[2] = listed;
}

/* ---- WORKOUT_SLOT(idx, &x, &y, &clut_x, &clut_y): 0x8008CE50's contract ---- */
int extra_by_number( uint32_t number )
{
	for( size_t k = 0; k < g_songs.size(); ++k )
		if( g_songs[k].record[1] == number )
			return int( k );
	return -1;
}

/* ---- NEW_GUEST: SPRT for the music select's arrows, plus NEW!! ----
 * A song is new once it is unlocked and until the save has a record for it
 * (the results code adds one the first time the song is played). */
bool played( const Song &s )
{
	const uint32_t save = rd32( SAVE_PTR );
	if( (save & 0xFFE00000u) != 0x80000000u )
		return true;
	for( int i = 0; i < SAVE_RECORDS; ++i )
		if( (rd32(save + 0x90 + 0x50 * uint32_t(i) + 0x24) & 0xFFFF) == (s.record[0] & 0xFFFF) )
			return true;
	return false;
}

bool cursor_is_new()
{
	const uint32_t list = rd32( SELECT_LIST );
	if( (list & 0xFFE00000u) != 0x80000000u )
		return false;
	const int k = extra_by_number( rd32(list + 0x18 + 4 * rd32(list + 0x14)) );
	return k >= 0 && g_songs[k].unlock_id && unlocked( g_songs[k] ) && !played( g_songs[k] );
}

void on_new_arrow( CPUState *cpu, uint32_t )
{
	const uint32_t sp = cpu->gpr[29], ra = cpu->gpr[31];
	const uint32_t y = cpu->gpr[5], u = rd32( sp + 0x10 ), v = rd32( sp + 0x14 );
	cpu->gpr[2] = psx_mod_call_guest( cpu, SPRT_FN, ra, cpu->gpr[4], y, cpu->gpr[6], cpu->gpr[7] );
	if( u == 52 && cursor_is_new() )
	{
		/* the same call's clut, colour and ot; u and v put back after */
		wr32( sp + 0x10, 8 );
		wr32( sp + 0x14, 96 );
		cpu->gpr[2] = psx_mod_call_guest( cpu, SPRT_FN, ra, uint32_t(-136), y - 34, 64, 25 );
		wr32( sp + 0x10, u );
		wr32( sp + 0x14, v );
	}
	cpu->gpr[31] = ra;
}

void assign_workout_slots()
{
	for( int &s : g_slot_song ) s = -1;
	uint32_t count = rd32( HISTORY + 4 ), head = rd32( HISTORY );
	if( count > HISTORY_SIZE ) count = HISTORY_SIZE;
	if( head >= HISTORY_SIZE ) head = 0;
	int used = 0;
	for( uint32_t r = 0; r <= count && used < NUM_WORKOUT_SLOTS; ++r )
	{
		const uint32_t row = (head + HISTORY_SIZE - r) % HISTORY_SIZE;	/* newest first */
		const int k = extra_by_number( r == 0? rd8(CURRENT_SONG): rd8(HISTORY_ROWS + 16 * row + 8) );
		bool have = k < 0;
		for( int s = 0; s < used && !have; ++s )
			have = g_slot_song[s] == k;
		if( !have )
			g_slot_song[used++] = k;
	}
}

void on_workout_slot( CPUState *cpu, uint32_t )
{
	const int32_t i = int32_t( cpu->gpr[4] );
	const bool loading = cpu->gpr[31] == WORKOUT_LOADER_RA;
	uint32_t x = 640 + 64 * uint32_t(i / 4), y = 256 + 55 * uint32_t(i % 4);
	if( i >= NUM_STOCK_INT && i - NUM_STOCK_INT < int(g_songs.size()) )
	{
		const int k = i - NUM_STOCK_INT;
		if( loading && k == 0 )
			assign_workout_slots();
		x = WORKOUT_SCRATCH[0];
		y = WORKOUT_SCRATCH[1];
		bool slot = false;
		for( int s = 0; s < NUM_WORKOUT_SLOTS; ++s )
			if( g_slot_song[s] == k )
			{
				x = EXTRA_WORKOUT_SLOTS[s][0];
				y = EXTRA_WORKOUT_SLOTS[s][1];
				slot = true;
			}
		if( loading && !slot )
			g_stand_in_blob = int32_t( g_songs[size_t(k)].workout );
	}
	wr32( cpu->gpr[5], x );
	wr32( cpu->gpr[6], y );
	wr32( cpu->gpr[7], 640 );
	wr32( rd32(cpu->gpr[29] + 0x10), uint32_t(476 + i) );
}

uint32_t i_addiu( int rt, int rs, int imm ) { return (9u << 26) | (uint32_t(rs) << 21) | (uint32_t(rt) << 16) | (uint32_t(imm) & 0xFFFF); }
uint32_t i_sw( int rt, int off, int base ) { return (0x2Bu << 26) | (uint32_t(base) << 21) | (uint32_t(rt) << 16) | (uint32_t(off) & 0xFFFF); }

void patch_workout_slots()
{
	enum { V0 = 2, A0 = 4, A1 = 5, A2 = 6, A3 = 7, S0 = 16, SP = 29 };
	/* the slot function: j WORKOUT_SLOT; nop */
	psx_mod_write_code_word( WORKOUT_SLOT_FN, j(WORKOUT_SLOT) );
	psx_mod_write_code_word( WORKOUT_SLOT_FN + 4, 0 );
	/* the loader's inline formula (x, y, clut x, clut y to 0x28..0x34(sp)) */
	const uint32_t code[] = {
		i_addiu( V0, SP, 0x34 ), i_sw( V0, 0x10, SP ),
		(uint32_t(S0) << 21) | (uint32_t(A0) << 11) | 0x21,	/* move a0, s0 */
		i_addiu( A1, SP, 0x28 ), i_addiu( A2, SP, 0x2C ),
		jal( WORKOUT_SLOT ), i_addiu( A3, SP, 0x30 ),
		(0x0Fu << 26) | (uint32_t(A1) << 16) | 0x8018,		/* lui a1, 0x8018 */
		(0x0Du << 26) | (uint32_t(A1) << 21) | (uint32_t(A1) << 16) | 0xC000,	/* ori a1, a1, 0xc000 */
	};
	uint32_t a = WORKOUT_LOADER;
	for( uint32_t w : code ) { psx_mod_write_code_word( a, w ); a += 4; }
	for( ; a <= 0x8008A9F0; a += 4 ) psx_mod_write_code_word( a, 0 );
}

/* ---- unlocks ----
 * The results code (0x80089880..0x80089B10) keeps one save record per song:
 * +0x24 the song id, and per chart d (single Beginner, Light, Standard,
 * Heavy = 0..3, double +4) the score at +4d and the grade at +0x46+d.  It
 * writes them only after a cleared play (a failed one leaves the record as
 * it was), so a grade means that chart was cleared. */
bool unlocked( const Song &s )
{
	const uint32_t save = rd32( SAVE_PTR );
	if( (save & 0xFFE00000u) != 0x80000000u )
		return false;
	for( int i = 0; i < SAVE_RECORDS; ++i )
	{
		const uint32_t rec = save + 0x90 + 0x50 * i;
		if( (rd32(rec + 0x24) & 0xFFFF) != s.unlock_id )
			continue;
		for( int d = 0; d < 8; ++d )
			if( (s.unlock_mask >> (d & 3) & 1) && rd8(rec + 0x46 + d) )
				return true;
	}
	return false;
}

void on_vblank()
{
	if( stock_code_present() )
		install();
}

/* The media file ships in the package; the runtime verifies it (size and
 * SHA-256 in the manifest) before activation mounts it. */
bool prepare_media( const PSXRecompV4::ModPrepareContext &ctx,
                    std::map<std::string, std::filesystem::path> &outputs, std::string &reason )
{
	const std::filesystem::path media = ctx.package.root / "data" / "media.bin";
	std::error_code ec;
	if( !std::filesystem::is_regular_file(media, ec) )
	{
		reason = "data/media.bin is missing";
		return false;
	}
	outputs["media"] = media;
	return true;
}

} // namespace

PSX_MOD_CONSTRUCTOR( dsf_register_extrasong )
{
	(void) psx_mod_register_activation_plugin( "dsf.extrasong", on_activate );
	(void) psx_mod_register_vblank_plugin( "dsf.extrasong", on_vblank );
	(void) psx_mod_register_guest_function_plugin( "dsf.extrasong", WORKOUT_SLOT, on_workout_slot );
	(void) psx_mod_register_guest_function_plugin( "dsf.extrasong", LZ_GUEST, on_lz );
	(void) psx_mod_register_guest_function_plugin( "dsf.extrasong", AVAIL_GUEST, on_avail );
	(void) psx_mod_register_guest_function_plugin( "dsf.extrasong", KIND_GUEST, on_kind );
	(void) psx_mod_register_guest_function_plugin( "dsf.extrasong", NEW_GUEST, on_new_arrow );
	(void) PSXRecompV4::mod_register_media_preparer( "dsf.extrasongs.media", prepare_media, prepare_media );
}
