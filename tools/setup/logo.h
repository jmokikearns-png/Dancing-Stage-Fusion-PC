/* The title screen's logo, from the player's disc (logo.c). */
#ifndef DSF_SETUP_LOGO_H
#define DSF_SETUP_LOGO_H
#include <stdint.h>

#define LOGO_OVERLAY 226	/* the game's file (READ_DT.BIN table entry) holding the logo sprite */
#define LOGO_OFFSET 246040u	/* where in it the sprite's compressed data starts */

/* Writes the theme's "_title logo dsf (res 374x176).png" from that file's
 * bytes; 0 if it isn't as expected or can't be written. */
int make_title_logo( const uint8_t *overlay, uint32_t overlay_n, const char *out_png );

#endif
