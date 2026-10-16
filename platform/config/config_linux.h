/* config.h for the dsfplatform CMake build on Linux (x86-64 / x86).
 * Subset of what the autotools build of wip/linux-build generates
 * (builds/linux-build/obj/src/config.h), restricted to what the platform
 * layer's sources test.  CRASH_HANDLER is deliberately not defined: the
 * StepMania crash handler (fork+backtrace child) is not part of the layer. */
#ifndef DSF_CONFIG_H
#define DSF_CONFIG_H
#define UNIX 1
#define LINUX 1
#define HAVE_X11 1
#define ENDIAN_LITTLE 1
#define HAVE_ENDIAN_H 1
#define HAVE_STDINT_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_UNISTD_H 1
#define HAVE_LIBPTHREAD 1
#define HAVE_PTHREAD_COND_TIMEDWAIT 1
#define HAVE_PTHREAD_MUTEX_TIMEDLOCK 1
#if defined(__x86_64__)
#define CPU_X86_64 1
#elif defined(__i386__)
#define CPU_X86 1
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#endif
