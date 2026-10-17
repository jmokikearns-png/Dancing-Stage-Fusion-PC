/* dsfcore_menus.cpp -- StepMania's settings inside Dancing Stage Fusion's own
 * OPTIONS screens.
 *
 * The OPTIONS mode is code overlay READ_DT e227 at 0x800E8000 (mode 1); its
 * screens and the routines used here are documented in
 * notes/dsf-options-menu.md.  Each list screen draws its rows itself: label
 * right-aligned at x = -width - 0x14, value at x = 0x14, rows 0x22 apart from
 * y = -0xA4, with the game's text routine 0x80024CB4; the cursor row gets the
 * highlight bar (0x80027BA4) and a star (0x800EFBF4).
 *
 * New rows are added around the original code, not instead of it: the
 * original draw and input functions still run for the original rows (one
 * constant is patched to move EXIT down), and the new rows are drawn and
 * driven here with the same calls, constants and input rules.  A row can
 * also open a page of PC settings, drawn the same way on the same screen.
 * While the cursor is on a new row or a page, the screen's cursor byte holds
 * a value no original row has (CURSOR_ROW / CURSOR_PAGE), so the original
 * code highlights nothing and all state survives save states.
 *
 * Help lines for the new rows are string ids >= VIRTUAL_STRING, which the
 * hook on the game's string lookup (0x800E8EF4) answers, so the game's own
 * help panel types them out.  All text is in the game's five languages.
 *
 * SOUND OPTIONS gets the PC sound rows, GRAPHIC OPTIONS a DISPLAY SETTINGS
 * page, CONTROLLER SETTINGS a KEY CONFIG page per player (the host's key /
 * pad mapping; a page longer than the screen scrolls).
 */
#include "dsfcore_host.h"
#include "mod_plugins.h"
#include "cpu_state.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

/* ---- guest access ---------------------------------------------------- */

uint32_t rd32( uint32_t a ) { return psx_mod_read_word( a ); }
uint8_t rd8( uint32_t a ) { return psx_mod_read_byte( a ); }
void wr32( uint32_t a, uint32_t v ) { psx_mod_write_word( a, v ); }
void wr8( uint32_t a, uint8_t v ) { psx_mod_write_byte( a, v ); }

/* Call a guest function with up to 9 word arguments (o32: a0-a3, then the
 * stack from sp+0x10).  ret: an address the callee returns to and nothing
 * else reaches -- the hooked function's own $ra. */
uint32_t call_guest( CPUState *cpu, uint32_t ret, uint32_t fn, int argc = 0, const uint32_t *argv = NULL )
{
	uint32_t a[9] = { 0 };
	for( int i = 0; i < argc && i < 9; ++i )
		a[i] = argv[i];
	const uint32_t sp = cpu->gpr[29];
	const uint32_t frame = (sp - 0x40u) & ~7u;
	for( int i = 4; i < argc && i < 9; ++i )
		wr32( frame + 4u*uint32_t(i), a[i] );
	cpu->gpr[29] = frame;
	const uint32_t v0 = psx_mod_call_guest( cpu, fn, ret, a[0], a[1], a[2], a[3] );
	cpu->gpr[29] = sp;
	return v0;
}

/* Guest copies of the strings handed to the game's routines (enhancement
 * memory; rewritten before every use). */
enum { STR_LABEL, STR_VALUE, STR_HELP, STR_SLOTS };
const uint32_t STR_SIZE = 256;
uint32_t g_strings = 0;

uint32_t guest_string( int slot, const char *s )
{
	if( g_strings == 0 )
		g_strings = psx_mod_alloc_guest_memory( STR_SIZE * STR_SLOTS, 4 );
	if( g_strings == 0 )
		return 0;
	const uint32_t at = g_strings + STR_SIZE * uint32_t(slot);
	uint32_t n = 0;
	for( ; s[n] && n < STR_SIZE - 1; ++n )
		wr8( at + n, uint8_t(s[n]) );
	wr8( at + n, 0 );
	return at;
}

/* ---- the game ---------------------------------------------------------- */

const uint32_t OPT_STATE_PTR = 0x800F0E00u;	/* -> options screen state */
const uint32_t PADS_PTR = 0x80092A28u;		/* -> pads (+0x50 held, +0x54 pressed, 0x10 per player) */
const uint32_t GAME_PTR = 0x80092A2Cu;		/* -> game state (+0x14 language) */
const uint32_t FADE_PTR = 0x800B3314u;		/* -> screen fade (+1) */

const uint32_t FN_TEXT = 0x80024CB4u;		/* (x, y, str, flags) */
const uint32_t FN_TEXT_WIDTH = 0x800256C4u;	/* (str) -> pixels */
const uint32_t FN_BAR = 0x80027BA4u;		/* cursor highlight bar */
const uint32_t FN_STAR = 0x800EFBF4u;		/* (x, y, side, fade, 8) */
const uint32_t FN_HELP_TEXT = 0x800EFEDCu;
const uint32_t FN_EXIT_ICON = 0x800F003Cu;
const uint32_t FN_HELP_PANEL = 0x800EF0B4u;
const uint32_t FN_SET_SCREEN = 0x800E8E04u;	/* (3, screen) */
const uint32_t FN_STRING = 0x800E8EF4u;		/* (id) -> str */
const uint32_t SND_MOVE = 0x80089DCCu, SND_CHANGE = 0x80089E4Cu;
const uint32_t SND_DECIDE = 0x80089DECu, SND_BACK = 0x80089E2Cu;

const int ROW_Y0 = -0xA4, ROW_STEP = 0x22, MAX_ROWS = 8;
const uint32_t VIRTUAL_STRING = 0x1000u;	/* string ids from here are ours */
const int SCREEN_TOP = 2;
const uint32_t STR_ON = 205, STR_OFF = 206, STR_EXIT = 204, STR_BACK_HELP = 203;

/* Pad bits as the game stores them. */
const uint32_t PAD_UP = 0x1000, PAD_DOWN = 0x4000, PAD_LEFT = 0x8000, PAD_RIGHT = 0x2000;
const uint32_t PAD_CONFIRM = 0x60, PAD_BACK = 0x110, PAD_SQUARE = 0x80;

/* Cursor byte values no original row uses. */
const uint8_t CURSOR_ROW = 0x40;	/* + new row */
const uint8_t CURSOR_PAGE = 0x80;	/* + page row */

uint32_t opt_state() { return rd32( OPT_STATE_PTR ); }

/* The overlay is e227 when its entry points hold the expected code. */
bool options_overlay_resident()
{
	return rd32( 0x800E8300u ) == 0x3C028009u &&	/* mode 1 entry */
	       rd32( 0x800E8000u ) == 0x800E8354u &&	/* its jump table */
	       opt_state() == 0x800F155Cu;
}

/* The language byte goes through the overlay's table at 0x800F1194 to the
 * string pack (0 English .. 4 Spanish), as 0x800E8EAC does. */
int language()
{
	const uint32_t l = rd8( rd32(GAME_PTR) + 0x14 );
	const uint32_t pack = l < 6? rd32( 0x800F1194u + 4*l ): 0;
	return pack < 5? int(pack): 0;
}

/* One of the game's own strings, from the loaded table (state + 0x20). */
void game_string( uint32_t id, char *out, size_t n )
{
	out[0] = 0;
	const uint32_t base = rd32( opt_state() + 0x20 );
	if( base == 0 )
		return;
	const uint32_t at = base + rd32( base + 4*id );
	size_t i = 0;
	for( ; i + 1 < n; ++i )
		if( (out[i] = char(rd8(at + uint32_t(i)))) == 0 )
			break;
	out[i] = 0;
}

/* ---- text -------------------------------------------------------------- */

/* English, French, German, Italian, Spanish (the game's order).  Labels and
 * values: capitals, Latin-1, like the game's own.  Help: up to 3 lines of
 * about 50 characters. */
struct Text { const char *t[5]; };
const char *tr( const Text &x ) { const char *s = x.t[language()]; return s? s: x.t[0]; }

#define L1 "\xC0"	/* À */
#define LE "\xC9"	/* É */
#define LE2 "\xC8"	/* È */
#define LO "\xD3"	/* Ó */
#define LU "\xDA"	/* Ú */
#define LA "\xC4"	/* Ä */
#define LOE "\xD6"	/* Ö */
#define e1 "\xE9"	/* é */
#define e2 "\xE8"	/* è */
#define a2 "\xE0"	/* à */
#define o1 "\xF3"	/* ó */
#define i1 "\xED"	/* í */
#define u2 "\xFC"	/* ü */
#define a3 "\xE4"	/* ä */
#define o3 "\xF6"	/* ö */
#define LU2 "\xDC"	/* Ü */
#define LU3 "\xD9"	/* Ù */
#define LI "\xCD"	/* Í */
#define LA2 "\xC1"	/* Á */

const Text T_SILENT = {{ "SILENT", "MUET", "STUMM", "MUTO", "SILENCIO" }};
const Text T_AUTO = {{ "AUTO", "AUTO", "AUTO", "AUTO", "AUTO" }};
const Text T_DEFAULT = {{ "DEFAULT", "PAR D" LE "FAUT", "STANDARD", "PREDEFINITA", "PREDETERMINADA" }};
const Text T_FULLSCREEN = {{ "FULL SCREEN", "PLEIN " LE "CRAN", "VOLLBILD", "SCHERMO INTERO", "PANTALLA COMPLETA" }};
const Text T_WINDOW = {{ "WINDOW", "FEN" "\xCA" "TRE", "FENSTER", "FINESTRA", "VENTANA" }};
const Text T_NATIVE = {{ "NATIVE", "NATIVE", "NATIV", "NATIVA", "NATIVA" }};
const Text T_SOFTWARE = {{ "SOFTWARE", "LOGICIEL", "SOFTWARE", "SOFTWARE", "SOFTWARE" }};
const Text T_PRESS = {{ "PRESS A KEY / BUTTON", "APPUIE SUR UNE TOUCHE", "TASTE DR" LU2 "CKEN", "PREMI UN TASTO", "PULSA UNA TECLA" }};

/* ---- rows ------------------------------------------------------------- */

enum RowKind { ROW_SETTING, ROW_PAGE, ROW_KEY };

struct Row
{
	RowKind kind;
	int id;		/* DSFCORE_SETTING_*, a page index, or DSFCORE_BUTTON_* */
	Text label, help;
	bool clamp;	/* stop at the ends instead of wrapping (scales) */
};

const Row SOUND_ROWS[] = {
	{ ROW_SETTING, DSFCORE_SETTING_VOLUME,
	  {{ "VOLUME", "VOLUME", "LAUTST" LA "RKE", "VOLUME", "VOLUMEN" }},
	  {{ "Volume of the PC's sound.",
	     "Volume du son du PC.",
	     "Lautst" a3 "rke des PC-Sounds.",
	     "Volume dell'audio del PC.",
	     "Volumen del sonido del PC." }}, true },
	{ ROW_SETTING, DSFCORE_SETTING_AUDIO_BUFFER,
	  {{ "AUDIO BUFFER", "TAMPON AUDIO", "AUDIOPUFFER", "BUFFER AUDIO", "B" LU "FER DE AUDIO" }},
	  {{ "Smaller is less delay. Raise it if the sound\ncrackles. (Applies the next time you start.)",
	     "Plus petit = moins de retard. Augmente-le si le\nson gr" e1 "sille. (Au prochain d" e1 "marrage.)",
	     "Kleiner = weniger Verz" o3 "gerung. Erh" o3 "hen, wenn der\nTon knackt. (Gilt ab dem n" a3 "chsten Start.)",
	     "Pi" "\xF9" " piccolo = meno ritardo. Aumentalo se\nl'audio gracchia. (Dal prossimo avvio.)",
	     "Menor = menos retardo. Aum" e1 "ntalo si el sonido\nchisporrotea. (Al pr" o1 "ximo inicio.)" }}, true },
	{ ROW_SETTING, DSFCORE_SETTING_SYNC_PICTURE,
	  {{ "PICTURE SYNC", "SYNCHRO IMAGE", "BILD-SYNC", "SINCRO IMMAGINE", "SINCRO. IMAGEN" }},
	  {{ "OFF: least delay (best for timing).\nON: each picture is shown when its sound is heard.",
	     "D" LE "SACTIV" LE " : retard minimal (id" e1 "al pour le timing).\nACTIV" LE "E : chaque image avec son propre son.",
	     "AUS: geringste Verz" o3 "gerung (am besten f" u2 "rs Timing).\nEIN: jedes Bild erscheint mit seinem Ton.",
	     "OFF: ritardo minimo (ideale per il tempo).\nON: ogni immagine appare con il suo suono.",
	     "DESACTIVAR: m" i1 "nimo retardo (mejor sincron" i1 "a).\nACTIVAR: cada imagen se muestra con su sonido." }}, false },
	{ ROW_SETTING, DSFCORE_SETTING_PICTURE_OFFSET,
	  {{ "PICTURE DELAY", "D" LE "LAI IMAGE", "BILDVERZ" LOE "GERUNG", "RITARDO IMMAGINE", "RETARDO IMAGEN" }},
	  {{ "Delays the picture by this much. Try +10 to +40 ms\nif the arrows look early against the music.",
	     "Retarde l'image d'autant. Essaie +10 " a2 " +40 ms\nsi les fl" e2 "ches semblent en avance.",
	     "Verz" o3 "gert das Bild um diesen Wert. +10 bis\n+40 ms probieren, wenn die Pfeile zu fr" u2 "h wirken.",
	     "Ritarda l'immagine di questo valore. Prova\nda +10 a +40 ms se le frecce sono in anticipo.",
	     "Retrasa la imagen esta cantidad. Prueba de +10\na +40 ms si las flechas van adelantadas." }}, true },
};

enum { PAGE_DISPLAY, PAGE_KEYS_1P, PAGE_KEYS_2P, NUM_PAGES };

const Row GRAPHIC_ROWS[] = {
	{ ROW_PAGE, PAGE_DISPLAY,
	  {{ "DISPLAY SETTINGS", "PARAM" LE2 "TRES D'AFFICHAGE", "ANZEIGE-EINSTELLUNGEN", "IMPOSTAZIONI SCHERMO", "AJUSTES DE PANTALLA" }},
	  {{ "PC display settings: screen mode, resolution\nand the game's picture quality.",
	     "Affichage du PC : mode d'" e1 "cran, r" e1 "solution\net qualit" e1 " d'image du jeu.",
	     "PC-Anzeige: Bildschirmmodus, Aufl" o3 "sung\nund Bildqualit" a3 "t des Spiels.",
	     "Schermo del PC: modalit" a2 ", risoluzione\ne qualit" a2 " d'immagine del gioco.",
	     "Pantalla del PC: modo, resoluci" o1 "n y\ncalidad de imagen del juego." }}, false },
};

const Row DISPLAY_ROWS[] = {
	{ ROW_SETTING, DSFCORE_SETTING_SCREEN_MODE,
	  {{ "SCREEN MODE", "MODE D'" LE "CRAN", "BILDSCHIRMMODUS", "MODALIT" L1 " SCHERMO", "MODO DE PANTALLA" }},
	  {{ "Full screen or a window.",
	     "Plein " e1 "cran ou fen" "\xEA" "tre.",
	     "Vollbild oder Fenster.",
	     "Schermo intero o finestra.",
	     "Pantalla completa o ventana." }}, false },
	{ ROW_SETTING, DSFCORE_SETTING_RESOLUTION,
	  {{ "RESOLUTION", "R" LE "SOLUTION", "AUFL" LOE "SUNG", "RISOLUZIONE", "RESOLUCI" LO "N" }},
	  {{ "Resolution of the PC's screen or window.",
	     "R" e1 "solution de l'" e1 "cran ou de la fen" "\xEA" "tre.",
	     "Aufl" o3 "sung von Bildschirm oder Fenster.",
	     "Risoluzione dello schermo o della finestra.",
	     "Resoluci" o1 "n de la pantalla o la ventana." }}, false },
	{ ROW_SETTING, DSFCORE_SETTING_REFRESH_RATE,
	  {{ "REFRESH RATE", "FR" LE "QUENCE", "BILDWIEDERHOLRATE", "FREQUENZA", "FRECUENCIA" }},
	  {{ "Refresh rate in full screen.",
	     "Fr" e1 "quence de rafra" "\xEE" "chissement en plein " e1 "cran.",
	     "Bildwiederholrate im Vollbild.",
	     "Frequenza di aggiornamento a schermo intero.",
	     "Frecuencia de actualizaci" o1 "n en pantalla completa." }}, false },
	{ ROW_SETTING, DSFCORE_SETTING_VSYNC,
	  {{ "VSYNC", "VSYNC", "VSYNC", "VSYNC", "VSYNC" }},
	  {{ "ON: no tearing. OFF: slightly less delay.",
	     "ACTIV" LE "E : pas de d" e1 "chirure. D" LE "SACTIV" LE " : un peu\nmoins de retard.",
	     "EIN: kein Tearing. AUS: etwas weniger\nVerz" o3 "gerung.",
	     "ON: nessuno strappo. OFF: un po' meno ritardo.",
	     "ACTIVAR: sin cortes. DESACTIVAR: algo menos\nde retardo." }}, false },
	{ ROW_SETTING, DSFCORE_SETTING_RENDER_QUALITY,
	  {{ "GAME RESOLUTION", "R" LE "SOLUTION DU JEU", "SPIELAUFL" LOE "SUNG", "RISOLUZIONE GIOCO", "RESOLUCI" LO "N DEL JUEGO" }},
	  {{ "The game's 3D at a higher resolution; its 2D art\nkeeps its pixels. (Applies the next time you start.)",
	     "La 3D du jeu en plus haute r" e1 "solution ; la 2D garde\nses pixels. (Au prochain d" e1 "marrage.)",
	     "3D des Spiels in h" o3 "herer Aufl" o3 "sung; 2D beh" a3 "lt\nihre Pixel. (Gilt ab dem n" a3 "chsten Start.)",
	     "Il 3D del gioco a risoluzione pi" "\xF9" " alta; il 2D\nmantiene i pixel. (Dal prossimo avvio.)",
	     "El 3D del juego a mayor resoluci" o1 "n; el 2D\nconserva sus p" i1 "xeles. (Al pr" o1 "ximo inicio.)" }}, false },
	{ ROW_SETTING, DSFCORE_SETTING_RENDERER,
	  {{ "RENDERER", "RENDU", "RENDERER", "RENDERER", "RENDERIZADOR" }},
	  {{ "OPENGL uses the graphics card (recommended).\n(Applies the next time you start.)",
	     "OPENGL utilise la carte graphique (conseill" e1 ").\n(Au prochain d" e1 "marrage.)",
	     "OPENGL nutzt die Grafikkarte (empfohlen).\n(Gilt ab dem n" a3 "chsten Start.)",
	     "OPENGL usa la scheda grafica (consigliato).\n(Dal prossimo avvio.)",
	     "OPENGL usa la tarjeta gr" "\xE1" "fica (recomendado).\n(Al pr" o1 "ximo inicio.)" }}, false },
};

const Row CONTROLLER_ROWS[] = {
	{ ROW_PAGE, PAGE_KEYS_1P,
	  {{ "KEY CONFIG 1P", "CONFIG. TOUCHES 1P", "TASTENBELEGUNG 1P", "CONFIG. TASTI 1P", "CONFIG. TECLAS 1P" }},
	  {{ "Keys, dance mat and joystick buttons for 1P\n(StepMania's input mapping).",
	     "Touches, tapis de danse et manette pour 1P\n(configuration des entr" e1 "es de StepMania).",
	     "Tasten, Tanzmatte und Joystick f" u2 "r 1P\n(Eingabebelegung von StepMania).",
	     "Tasti, tappetino e joystick per 1P\n(mappatura dei comandi di StepMania).",
	     "Teclas, alfombra de baile y mando para 1P\n(asignaci" o1 "n de controles de StepMania)." }}, false },
	{ ROW_PAGE, PAGE_KEYS_2P,
	  {{ "KEY CONFIG 2P", "CONFIG. TOUCHES 2P", "TASTENBELEGUNG 2P", "CONFIG. TASTI 2P", "CONFIG. TECLAS 2P" }},
	  {{ "Keys, dance mat and joystick buttons for 2P\n(StepMania's input mapping).",
	     "Touches, tapis de danse et manette pour 2P\n(configuration des entr" e1 "es de StepMania).",
	     "Tasten, Tanzmatte und Joystick f" u2 "r 2P\n(Eingabebelegung von StepMania).",
	     "Tasti, tappetino e joystick per 2P\n(mappatura dei comandi di StepMania).",
	     "Teclas, alfombra de baile y mando para 2P\n(asignaci" o1 "n de controles de StepMania)." }}, false },
};

#define KEY_HELP {{ \
	"Cross: set a key or pad button.  Square: clear\nyour own (the default stays).", \
	"Croix : choisir une touche ou un bouton. Carr" e1 " :\neffacer les tiens (celle par d" e1 "faut reste).", \
	"Kreuz: Taste oder Knopf w" a3 "hlen.  Quadrat: eigene\nl" o3 "schen (die Standardtaste bleibt).", \
	"Croce: scegli un tasto o un pulsante.  Quadrato:\ncancella i tuoi (resta quello predefinito).", \
	"Equis: elegir una tecla o bot" o1 "n.  Cuadrado: borrar\nlos tuyos (el predeterminado se queda)." }}
#define KEY_ROW( b, en, fr, de, it, es )	{ ROW_KEY, b, {{ en, fr, de, it, es }}, KEY_HELP, false }
const Row KEY_ROWS[] = {
	KEY_ROW( DSFCORE_BUTTON_UP, "UP", "HAUT", "OBEN", "SU", "ARRIBA" ),
	KEY_ROW( DSFCORE_BUTTON_DOWN, "DOWN", "BAS", "UNTEN", "GI" LU3, "ABAJO" ),
	KEY_ROW( DSFCORE_BUTTON_LEFT, "LEFT", "GAUCHE", "LINKS", "SINISTRA", "IZQUIERDA" ),
	KEY_ROW( DSFCORE_BUTTON_RIGHT, "RIGHT", "DROITE", "RECHTS", "DESTRA", "DERECHA" ),
	KEY_ROW( DSFCORE_BUTTON_CROSS, "CROSS", "CROIX", "KREUZ", "CROCE", "EQUIS" ),
	KEY_ROW( DSFCORE_BUTTON_CIRCLE, "CIRCLE", "ROND", "KREIS", "CERCHIO", "C" LI "RCULO" ),
	KEY_ROW( DSFCORE_BUTTON_SQUARE, "SQUARE", "CARR" LE, "QUADRAT", "QUADRATO", "CUADRADO" ),
	KEY_ROW( DSFCORE_BUTTON_TRIANGLE, "TRIANGLE", "TRIANGLE", "DREIECK", "TRIANGOLO", "TRI" LA2 "NGULO" ),
	KEY_ROW( DSFCORE_BUTTON_L1, "L1", "L1", "L1", "L1", "L1" ),
	KEY_ROW( DSFCORE_BUTTON_L2, "L2", "L2", "L2", "L2", "L2" ),
	KEY_ROW( DSFCORE_BUTTON_R1, "R1", "R1", "R1", "R1", "R1" ),
	KEY_ROW( DSFCORE_BUTTON_R2, "R2", "R2", "R2", "R2", "R2" ),
	KEY_ROW( DSFCORE_BUTTON_START, "START", "START", "START", "START", "START" ),
	KEY_ROW( DSFCORE_BUTTON_SELECT, "SELECT", "SELECT", "SELECT", "SELECT", "SELECT" ),
	KEY_ROW( DSFCORE_BUTTON_PC_MENU, "PC MENU", "MENU PC", "PC-MEN" LU2, "MENU PC", "MEN" LU " PC" ),
};

/* The help line while a key capture waits. */
const Row CAPTURE_HELP = { ROW_KEY, -1, {{ "", "", "", "", "" }},
	{{ "Press the key, dance mat or joystick button for\nthis button now.  Esc: cancel.",
	   "Appuie maintenant sur la touche, le tapis ou le\nbouton de manette voulu.  " LE "chap : annuler.",
	   "Jetzt die Taste, Tanzmatte oder den Joystick-Knopf\ndr" u2 "cken.  Esc: abbrechen.",
	   "Premi ora il tasto, il tappetino o il pulsante\ndel joystick.  Esc: annulla.",
	   "Pulsa ahora la tecla, la alfombra o el bot" o1 "n\ndel mando.  Esc: cancelar." }}, false };

struct Page
{
	const Row *rows;
	int nrows;	/* without EXIT */
	int player;	/* key pages: 0/1 */
};
const Page PAGES[NUM_PAGES] = {
	{ DISPLAY_ROWS, int(sizeof(DISPLAY_ROWS)/sizeof(DISPLAY_ROWS[0])), -1 },
	{ KEY_ROWS, int(sizeof(KEY_ROWS)/sizeof(KEY_ROWS[0])), 0 },
	{ KEY_ROWS, int(sizeof(KEY_ROWS)/sizeof(KEY_ROWS[0])), 1 },
};

/* An original list screen that gets new rows before its EXIT. */
struct ListScreen
{
	uint32_t input_fn, input_word;	/* hooked; first instruction as a guard */
	uint32_t draw_fn, draw_word;
	int depth;			/* cursor byte = state + 0x10 + depth */
	int exit_row;			/* EXIT's original index (= original rows - 1) */
	uint32_t help_table;		/* original rows' help string ids */
	bool keep_gap;			/* keep the blank row before EXIT */
	struct Patch { uint32_t addr, stock; } gap[2];	/* the code adding the blank row */
	const Row *rows;
	int nrows;
	uint32_t gap_code( int i, int rows_down ) const;
};

const ListScreen SCREENS[] = {
	/* SOUND OPTIONS: SOUND, VOICE, TIMING, (gap), EXIT */
	{ 0x800ED124u, 0x27BDFFE0u, 0x800ED5BCu, 0x3C03800Fu, 1, 3, 0x800F144Cu, false,
	  {{ 0x800EDBFCu, 0x26940022u }, { 0, 0 }},	/* addiu s4, s4, 0x22 */
	  SOUND_ROWS, int(sizeof(SOUND_ROWS)/sizeof(SOUND_ROWS[0])) },
	/* GRAPHIC OPTIONS: DANCER DISPLAY, BG FX, BG BRIGHTNESS, STEP MARK COLORS, (gap), EXIT */
	{ 0x800E9A48u, 0x27BDFFE0u, 0x800E9E90u, 0x3C03800Fu, 1, 4, 0x800F122Cu, true,
	  {{ 0x800EA2F0u, 0x02A3A821u }, { 0x800EA2F4u, 0x26D60022u }},	/* addu s5, s5, v1; addiu s6, s6, 0x22 */
	  GRAPHIC_ROWS, int(sizeof(GRAPHIC_ROWS)/sizeof(GRAPHIC_ROWS[0])) },
	/* CONTROLLER SETTINGS: DOUBLE MODE SETTINGS, DANCE PLAY SETTINGS 1/2, (gap), EXIT */
	{ 0x800EA4E4u, 0x27BDFFE0u, 0x800EA804u, 0x27BDFEA0u, 1, 3, 0x800F1324u, true,
	  {{ 0x800EAB64u, 0x26940022u }, { 0, 0 }},	/* addiu s4, s4, 0x22 */
	  CONTROLLER_ROWS, int(sizeof(CONTROLLER_ROWS)/sizeof(CONTROLLER_ROWS[0])) },
};
const int NUM_SCREENS = int(sizeof(SCREENS)/sizeof(SCREENS[0]));

/* The patched blank-row code: EXIT moves down by rows_down rows. */
uint32_t ListScreen::gap_code( int i, int rows_down ) const
{
	const uint32_t step = uint32_t(ROW_STEP * rows_down) & 0xFFFFu;
	if( gap[1].addr == 0 )
		return (gap[0].stock & 0xFFFF0000u) | step;	/* addiu rN, rN, step */
	/* Two row registers (GRAPHIC: s6, and s5 = s6 << 16). */
	return i == 0? 0x26D60000u | step:	/* addiu s6, s6, step */
	               0x0016AC00u;		/* sll s5, s6, 16 */
}

/* ---- settings ---------------------------------------------------------- */

bool str_ieq( const char *a, const char *b )
{
	for( ; *a && *b; ++a, ++b )
		if( (*a | 0x20) != (*b | 0x20) )
			return false;
	return *a == *b;
}

/* The value as the game would print it. */
void value_text( int id, const DsfCoreSetting &s, char *out, size_t n )
{
	const char *t = s.text;
	const Text *word = NULL;
	if( str_ieq(t, "ON") || str_ieq(t, "YES") ) { game_string( STR_ON, out, n ); return; }
	if( str_ieq(t, "OFF") || str_ieq(t, "NO") ) { game_string( STR_OFF, out, n ); return; }
	if( str_ieq(t, "SILENT") ) word = &T_SILENT;
	else if( str_ieq(t, "AUTO") ) word = &T_AUTO;
	else if( str_ieq(t, "DEFAULT") ) word = &T_DEFAULT;
	else if( str_ieq(t, "FULLSCREEN") ) word = &T_FULLSCREEN;
	else if( str_ieq(t, "WINDOWED") ) word = &T_WINDOW;
	else if( str_ieq(t, "NATIVE") ) word = &T_NATIVE;
	else if( str_ieq(t, "SOFTWARE") ) word = &T_SOFTWARE;
	if( word ) { std::snprintf( out, n, "%s", tr(*word) ); return; }
	size_t i = 0;
	for( ; t[i] && i + 1 < n; ++i )
		out[i] = (t[i] >= 'a' && t[i] <= 'z')? char(t[i] - 32): t[i];
	out[i] = 0;
	if( id == DSFCORE_SETTING_REFRESH_RATE && t[0] >= '0' && t[0] <= '9' )
		std::snprintf( out + i, n - i, " HZ" );
}

/* Left/Right on a setting row.  Returns true when the value changed. */
bool change_setting( const Row &r, int dir )
{
	DsfCoreSetting s;
	if( !dsfcore_host_get_setting(r.id, &s) || s.num_choices <= 1 )
		return false;
	int c = s.choice + dir;
	if( r.clamp )
		c = c < 0? 0: c >= s.num_choices? s.num_choices - 1: c;
	else
		c = (c + s.num_choices) % s.num_choices;
	if( c == s.choice )
		return false;
	dsfcore_host_set_setting( r.id, c );
	return true;
}

/* ---- the help line ---------------------------------------------------- */

/* Virtual string ids: VIRTUAL_STRING + screen * 0x100 + row (0x80: page rows). */
const Row *g_help_rows[0x400];

uint32_t help_id_for( const Row *r )
{
	for( uint32_t i = 0; i < 0x400; ++i )
	{
		if( g_help_rows[i] == r )
			return VIRTUAL_STRING + i;
		if( g_help_rows[i] == NULL )
		{
			g_help_rows[i] = r;
			return VIRTUAL_STRING + i;
		}
	}
	return 0;
}

int on_game_string( CPUState *cpu, uint32_t )
{
	const uint32_t id = cpu->gpr[4];
	if( id < VIRTUAL_STRING || id >= VIRTUAL_STRING + 0x400 || !g_help_rows[id - VIRTUAL_STRING] ||
	    !options_overlay_resident() )
		return 0;	/* the game's own string */
	cpu->gpr[2] = guest_string( STR_HELP, tr(g_help_rows[id - VIRTUAL_STRING]->help) );
	return 1;
}

void set_help( uint32_t id )
{
	const uint32_t st = opt_state();
	wr32( st + 0x0C, 0 );	/* restart the help text */
	wr32( st + 0x24, id );
}

/* ---- input, as the original screens read it ---------------------------- */

enum Action { ACT_NONE, ACT_IGNORED, ACT_BACK, ACT_CONFIRM, ACT_UP, ACT_DOWN, ACT_LEFT, ACT_RIGHT, ACT_CLEAR };

struct Input
{
	Action act;
	int player;
	bool repeat;	/* Up/Down accepted by auto-repeat */
};

/* What the original input loop would do this frame (no side effects): the
 * first player with any of these inputs decides; triggers need 8 frames
 * since the last accepted input; held Up/Down repeats.  Square (ACT_CLEAR)
 * is ours: the original screens ignore it, so it comes last. */
Input read_input()
{
	const uint32_t st = opt_state();
	const uint32_t pads = rd32( PADS_PTR );
	const bool ready = rd32( st ) >= 8;
	for( int p = 0; p < 2; ++p )
	{
		const uint32_t held = rd32( pads + 0x50 + 0x10*p );
		const uint32_t trig = rd32( pads + 0x54 + 0x10*p );
		Input in = { ACT_IGNORED, p, false };
		if( trig & PAD_BACK ) { if( ready ) in.act = ACT_BACK; return in; }
		if( trig & PAD_CONFIRM ) { if( ready ) in.act = ACT_CONFIRM; return in; }
		const uint32_t dirs[2] = { PAD_UP, PAD_DOWN };
		for( int d = 0; d < 2; ++d )
		{
			if( !(held & dirs[d]) )
				continue;
			if( trig & dirs[d] )
			{
				if( ready ) in.act = d? ACT_DOWN: ACT_UP;
			}
			else if( rd32(st + 4 + 4*p) > 0x20 )
			{
				in.act = d? ACT_DOWN: ACT_UP;
				in.repeat = true;
			}
			return in;
		}
		if( trig & PAD_LEFT ) { if( ready ) in.act = ACT_LEFT; return in; }
		if( trig & PAD_RIGHT ) { if( ready ) in.act = ACT_RIGHT; return in; }
		if( trig & PAD_SQUARE ) { if( ready ) in.act = ACT_CLEAR; return in; }
	}
	Input none = { ACT_NONE, 0, false };
	return none;
}

/* The bookkeeping of an accepted input. */
void accept( const Input &in )
{
	const uint32_t st = opt_state();
	wr32( st, 0 );
	if( in.repeat )
		wr32( st + 4 + 4*in.player, 0x1A );
}

/* ---- drawing, as the original screens draw ----------------------------- */

struct Draw
{
	CPUState *cpu;
	uint32_t ret;
	uint32_t call( uint32_t fn, int argc = 0, const uint32_t *argv = NULL ) { return call_guest( cpu, ret, fn, argc, argv ); }
	uint32_t fade() { return rd8( rd32(FADE_PTR) + 1 ); }

	void begin()
	{
		/* Each list screen's draw starts by copying the screen fade. */
		wr8( rd32(PADS_PTR) + 0x17, uint8_t(fade()) );
	}
	void depth()
	{
		psx_mod_write_half( rd32(GAME_PTR) + 8, 8 );
	}
	uint32_t width( uint32_t s ) { return call( FN_TEXT_WIDTH, 1, &s ); }
	void text( int x, int y, uint32_t s, uint32_t flags )
	{
		const uint32_t a[4] = { uint32_t(x), uint32_t(y), s, flags };
		call( FN_TEXT, 4, a );
	}
	void bar( int y )
	{
		const uint32_t alpha = ((rd32(opt_state() + 0x60) * fade()) >> 7) & 0xFF;
		const uint32_t a[9] = { uint32_t(-0x140), uint32_t(y - 2), 0x280, 0x1E, 0xED, 0x9F, 0x1C, alpha, 0x80000009u };
		call( FN_BAR, 9, a );
	}
	void star( int x, int y, int side )
	{
		const uint32_t a[5] = { uint32_t(x), uint32_t(y + 0xD), uint32_t(side), fade(), 8 };
		call( FN_STAR, 5, a );
	}

	/* A setting row (or a page link: no value). */
	void row( int r, const Row &rw, bool selected )
	{
		const int y = ROW_Y0 + ROW_STEP*r;
		depth();
		const uint32_t ls = guest_string( STR_LABEL, tr(rw.label) );
		text( -int(width(ls)) - 0x14, y, ls, 0xC000u | (selected? 0u: 1u) );
		if( rw.kind == ROW_SETTING )
		{
			DsfCoreSetting s;
			if( dsfcore_host_get_setting(rw.id, &s) )
			{
				char v[64];
				value_text( rw.id, s, v, sizeof(v) );
				const uint32_t vs = guest_string( STR_VALUE, v );
				text( 0x14, y, vs, 0xC000u | (s.choice == s.default_choice? 2u: 3u) );
			}
		}
		if( selected )
		{
			bar( y );
			star( 0, y, 1 );
		}
	}

	/* A KEY CONFIG row: what is mapped to the button (as much as fits), or
	 * the blinking prompt while a capture waits for it. */
	void key_row( int r, const Row &rw, int player, bool selected, bool capturing )
	{
		const int y = ROW_Y0 + ROW_STEP*r;
		depth();
		const uint32_t ls = guest_string( STR_LABEL, tr(rw.label) );
		text( -int(width(ls)) - 0x14, y, ls, 0xC000u | (selected? 0u: 1u) );
		char v[96] = "";
		bool own = false;
		if( capturing )
		{
			if( (rd32(opt_state() + 0x28) >> 4) & 1 )
				std::snprintf( v, sizeof(v), "%s", tr(T_PRESS) );
		}
		else
		{
			DsfCoreMapping m;
			if( dsfcore_host_get_mapping(player, rw.id, &m) )
			{
				own = m.slot[0][0] || m.slot[1][0];
				for( int k = 0; k < DSFCORE_MAPPING_SLOTS; ++k )
				{
					if( !m.slot[k][0] )
						continue;
					char t[96];
					std::snprintf( t, sizeof(t), "%s%s%s", v, v[0]? " / ": "", m.slot[k] );
					if( v[0] && width(guest_string(STR_VALUE, t)) > 270 )
					{
						std::snprintf( t, sizeof(t), "%s +", v );
						std::snprintf( v, sizeof(v), "%s", t );
						break;
					}
					std::snprintf( v, sizeof(v), "%s", t );
				}
				if( !v[0] )
					std::snprintf( v, sizeof(v), "-" );
			}
		}
		if( v[0] )
			text( 0x14, y, guest_string(STR_VALUE, v), 0xC000u | (capturing || own? 3u: 2u) );
		if( selected )
		{
			bar( y );
			star( 0, y, 1 );
		}
	}

	/* EXIT, centred, with a star on each side when selected. */
	void exit_row( int r, bool selected )
	{
		const int y = ROW_Y0 + ROW_STEP*r;
		char e[32];
		game_string( STR_EXIT, e, sizeof(e) );
		depth();
		const uint32_t es = guest_string( STR_LABEL, e );
		text( 0, y, es, 0xC000u | 0x2000u | (selected? 0u: 1u) );
		if( selected )
		{
			bar( y );
			const int w = int(width( guest_string(STR_LABEL, e) )) + 0x28;
			int x = -w/2;
			for( int side = 0; side < 2; ++side, x += w )
				star( x, y, side );
		}
	}

	/* What every list screen draws after its rows. */
	void footer( bool on_exit )
	{
		call( FN_HELP_TEXT );
		if( on_exit )
		{
			const uint32_t icon_fade = (rd8( rd32(PADS_PTR) + 0x90 ) * fade()) >> 7;
			const uint32_t a[4] = { 0x10E, 0xCA, icon_fade, 8 };
			call( FN_EXIT_ICON, 4, a );
		}
		call( FN_HELP_PANEL );
	}
};

/* ---- list screens ------------------------------------------------------ */

bool g_in_original = false;

/* A key capture in progress (the host's state says when it ends). */
struct Capture { bool active; int page, sel; };
Capture g_capture = { false, 0, 0 };

/* Pages longer than the screen scroll: the rows above EXIT show a window
 * that keeps the cursor in the middle, so it follows from the cursor alone. */
const int PAGE_VISIBLE = MAX_ROWS - 1;
int page_top( const Page &p, int sel )
{
	if( p.nrows <= PAGE_VISIBLE )
		return 0;
	int top = (sel < p.nrows? sel: p.nrows) - PAGE_VISIBLE/2;
	if( top > p.nrows - PAGE_VISIBLE ) top = p.nrows - PAGE_VISIBLE;
	return top < 0? 0: top;
}

const ListScreen *screen_for( uint32_t fn, bool draw )
{
	for( int i = 0; i < NUM_SCREENS; ++i )
	{
		const ListScreen &s = SCREENS[i];
		if( (draw? s.draw_fn: s.input_fn) != fn )
			continue;
		if( rd32(fn) != (draw? s.draw_word: s.input_word) || !options_overlay_resident() )
			return NULL;
		return &s;
	}
	return NULL;
}

uint32_t cursor_addr( const ListScreen &s ) { return opt_state() + 0x10 + uint32_t(s.depth); }

/* EXIT's row with the new rows in place. */
int exit_slot( const ListScreen &s ) { return s.exit_row + s.nrows + (s.keep_gap? 1: 0); }
int page_exit_slot( const Page &p ) { return p.nrows < PAGE_VISIBLE? p.nrows + 1: PAGE_VISIBLE; }

void patch_gap( const ListScreen &s )
{
	const int down = s.nrows + (s.keep_gap? 1: 0);
	for( int i = 0; i < 2 && s.gap[i].addr; ++i )
	{
		const uint32_t want = s.gap_code( i, down );
		const uint32_t have = rd32( s.gap[i].addr );
		if( have == s.gap[i].stock )
			psx_mod_write_code_word( s.gap[i].addr, want );
	}
}

void leave_to_top( Draw &d )
{
	d.call( SND_BACK );
	dsfcore_host_commit_settings();
	const uint32_t a[2] = { 3, SCREEN_TOP };
	d.call( FN_SET_SCREEN, 2, a );
}

void on_list_draw( CPUState *cpu, uint32_t fn )
{
	if( g_in_original )
		return;
	const ListScreen *s = screen_for( fn, true );
	if( !s )
		return;
	Draw d = { cpu, cpu->gpr[31] };
	const uint8_t cur = rd8( cursor_addr(*s) );
	if( cur >= CURSOR_PAGE )
	{
		/* A page of PC settings in place of the screen's rows. */
		const int page = (cur - CURSOR_PAGE) >> 4, sel = (cur - CURSOR_PAGE) & 15;
		if( page < NUM_PAGES )
		{
			const Page &p = PAGES[page];
			d.begin();
			const int top = page_top( p, sel );
			for( int r = top; r < p.nrows && r - top < PAGE_VISIBLE; ++r )
			{
				if( p.rows[r].kind == ROW_KEY )
					d.key_row( r - top, p.rows[r], p.player, sel == r,
						g_capture.active && g_capture.page == page && g_capture.sel == r );
				else
					d.row( r - top, p.rows[r], sel == r );
			}
			d.exit_row( page_exit_slot(p), sel == p.nrows );
			d.footer( sel == p.nrows );
			psx_mod_finish_function( cpu );
			return;
		}
	}
	patch_gap( *s );
	g_in_original = true;
	d.call( fn );
	g_in_original = false;
	for( int k = 0; k < s->nrows; ++k )
		d.row( s->exit_row + k, s->rows[k], cur == CURSOR_ROW + k );
	psx_mod_finish_function( cpu );
}

/* Move the cursor and show the new row's help. */
void move_cursor( Draw &d, const ListScreen &s, uint8_t to, const Row *help_row )
{
	wr8( cursor_addr(s), to );
	d.call( SND_MOVE );
	if( help_row )
		set_help( help_id_for(help_row) );
	else if( to < CURSOR_ROW )
		set_help( rd32(s.help_table + 4u*to) );
}

void page_input( Draw &d, const ListScreen &s, int page, int sel, const Input &in )
{
	const Page &p = PAGES[page];
	const uint8_t base = uint8_t( CURSOR_PAGE + (page << 4) );
	if( g_capture.active )
	{
		if( g_capture.page == page && g_capture.sel == sel )
		{
			const int state = dsfcore_host_mapping_state();
			if( state == DSFCORE_CAPTURE_WAITING )
				return;	/* the host keeps the game's input away meanwhile */
			d.call( state == DSFCORE_CAPTURE_DONE? SND_DECIDE: SND_BACK );
			set_help( help_id_for(&p.rows[sel]) );
			wr32( opt_state(), 0 );	/* the usual 8 frames before the next input */
		}
		g_capture.active = false;
		return;
	}
	switch( in.act )
	{
	case ACT_UP:
	case ACT_DOWN:
	{
		accept( in );
		const int n = p.nrows + 1;
		const int to = (sel + (in.act == ACT_UP? n - 1: 1)) % n;
		wr8( cursor_addr(s), uint8_t(base + to) );
		d.call( SND_MOVE );
		set_help( to < p.nrows? help_id_for(&p.rows[to]): STR_BACK_HELP );
		break;
	}
	case ACT_LEFT:
	case ACT_RIGHT:
		accept( in );
		if( sel < p.nrows && p.rows[sel].kind == ROW_SETTING && change_setting(p.rows[sel], in.act == ACT_LEFT? -1: 1) )
			d.call( SND_CHANGE );
		break;
	case ACT_CLEAR:
		accept( in );
		if( sel < p.nrows && p.rows[sel].kind == ROW_KEY )
		{
			DsfCoreMapping m;
			if( dsfcore_host_get_mapping(p.player, p.rows[sel].id, &m) && (m.slot[0][0] || m.slot[1][0]) )
			{
				dsfcore_host_clear_mapping( p.player, p.rows[sel].id );
				d.call( SND_CHANGE );
			}
		}
		break;
	case ACT_CONFIRM:
		accept( in );
		if( sel < p.nrows )
		{
			if( p.rows[sel].kind == ROW_KEY )
			{
				dsfcore_host_begin_mapping( p.player, p.rows[sel].id );
				g_capture.active = true;
				g_capture.page = page;
				g_capture.sel = sel;
				d.call( SND_DECIDE );
				set_help( help_id_for(&CAPTURE_HELP) );
			}
			break;
		}
		/* fall through: EXIT */
	case ACT_BACK:
	{
		accept( in );
		d.call( SND_BACK );
		dsfcore_host_commit_settings();
		/* Back on the row that opened the page. */
		for( int k = 0; k < s.nrows; ++k )
			if( s.rows[k].kind == ROW_PAGE && s.rows[k].id == page )
			{
				wr8( cursor_addr(s), uint8_t(CURSOR_ROW + k) );
				set_help( help_id_for(&s.rows[k]) );
			}
		break;
	}
	default:
		break;
	}
}

void on_list_input( CPUState *cpu, uint32_t fn )
{
	const ListScreen *s = screen_for( fn, false );
	if( !s )
		return;
	Draw d = { cpu, cpu->gpr[31] };
	const uint8_t cur = rd8( cursor_addr(*s) );
	const Input in = read_input();

	if( cur >= CURSOR_PAGE && ((cur - CURSOR_PAGE) >> 4) < NUM_PAGES )
	{
		page_input( d, *s, (cur - CURSOR_PAGE) >> 4, (cur - CURSOR_PAGE) & 15, in );
		psx_mod_finish_function( cpu );
		return;
	}

	if( cur < CURSOR_ROW )
	{
		/* An original row: the original code runs, except where the cursor
		 * crosses into the new rows. */
		if( in.act == ACT_DOWN && cur == s->exit_row - 1 )
		{
			accept( in );
			move_cursor( d, *s, CURSOR_ROW, &s->rows[0] );
			psx_mod_finish_function( cpu );
		}
		else if( in.act == ACT_UP && cur == s->exit_row )
		{
			accept( in );
			move_cursor( d, *s, uint8_t(CURSOR_ROW + s->nrows - 1), &s->rows[s->nrows - 1] );
			psx_mod_finish_function( cpu );
		}
		else if( in.act == ACT_BACK || (in.act == ACT_CONFIRM && cur == s->exit_row) )
			dsfcore_host_commit_settings();	/* the original leaves the screen */
		return;
	}

	const int k = cur - CURSOR_ROW;
	if( k >= s->nrows )
		return;	/* not ours: let the original code recover */
	const Row &row = s->rows[k];
	switch( in.act )
	{
	case ACT_UP:
		accept( in );
		if( k == 0 )
			move_cursor( d, *s, uint8_t(s->exit_row - 1), NULL );
		else
			move_cursor( d, *s, uint8_t(cur - 1), &s->rows[k - 1] );
		break;
	case ACT_DOWN:
		accept( in );
		if( k == s->nrows - 1 )
			move_cursor( d, *s, uint8_t(s->exit_row), NULL );
		else
			move_cursor( d, *s, uint8_t(cur + 1), &s->rows[k + 1] );
		break;
	case ACT_LEFT:
	case ACT_RIGHT:
		accept( in );
		if( row.kind == ROW_SETTING && change_setting(row, in.act == ACT_LEFT? -1: 1) )
			d.call( SND_CHANGE );
		break;
	case ACT_CONFIRM:
		accept( in );
		if( row.kind == ROW_PAGE && row.id < NUM_PAGES )
		{
			d.call( SND_DECIDE );
			wr8( cursor_addr(*s), uint8_t(CURSOR_PAGE + (row.id << 4)) );
			set_help( help_id_for(&PAGES[row.id].rows[0]) );
		}
		break;
	case ACT_BACK:
		accept( in );
		leave_to_top( d );
		break;
	default:
		break;
	}
	psx_mod_finish_function( cpu );
}

} // namespace

PSX_MOD_CONSTRUCTOR( dsf_register_stepmania_menus )
{
	const char *id = "dsf.stepmania.menus";
	for( int i = 0; i < NUM_SCREENS; ++i )
	{
		(void) psx_mod_register_function_entry_plugin( id, SCREENS[i].draw_fn, on_list_draw );
		(void) psx_mod_register_function_entry_plugin( id, SCREENS[i].input_fn, on_list_input );
	}
	(void) psx_mod_register_function_filter_plugin( id, FN_STRING, on_game_string );
}
