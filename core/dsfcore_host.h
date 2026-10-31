/* dsfcore_host.h -- host services for the hooks inside the game (core thread).
 *
 * dsfcore_embed.cpp implements these on top of the host's DsfCoreHost
 * callbacks (StepMania); dsfcore_host_stub.cpp implements them for the
 * stand-alone Linux test build, which has no host.
 */
#ifndef DSFCORE_HOST_H
#define DSFCORE_HOST_H

#include <stdint.h>

#include "dsfcore_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Both players' buttons right now (active-low PS1 pad words).  Returns 0
 * when there is no host input (the emulated controller port stays in use). */
int dsfcore_host_read_pads( uint16_t pads_out[2] );

/* PC settings (DSFCORE_SETTING_*, see dsfcore_api.h). */
int dsfcore_host_get_setting( int id, DsfCoreSetting *out );
void dsfcore_host_set_setting( int id, int choice );
void dsfcore_host_commit_settings( void );

/* Key / pad mapping (see DsfCoreHost). */
int dsfcore_host_get_mapping( int player, int button, DsfCoreMapping *out );
void dsfcore_host_begin_mapping( int player, int button );
int dsfcore_host_mapping_state( void );
void dsfcore_host_clear_mapping( int player, int button );

/* QUIT on the main menu (see DsfCoreHost.quit). */
void dsfcore_host_quit( void );

/* The PC's sound and picture delays (see DsfCoreHost.measure_latency). */
int dsfcore_host_measure_latency( DsfCoreLatency *out );

#ifdef __cplusplus
}
#endif

#endif
