/* Native POSIX configuration for real-state map tests, independent of configure.
 * No DOS libc, socket, compression, audio-plugin or translation features. */
#ifndef FC__DOS_MAP_NATIVE_CONFIG_H
#define FC__DOS_MAP_NATIVE_CONFIG_H

#define FC_CONFIG_H 1
#define FC_NO_SOCKET_API 1
#define STDC_HEADERS 1
#define TIME_WITH_SYS_TIME 1

#define HAVE_ALLOCA 1
#define HAVE_ALLOCA_H 1
#define HAVE_ARPA_INET_H 1
#define HAVE_FCNTL_H 1
#define HAVE_FDOPEN 1
#define HAVE_FILENO 1
#define HAVE_GETCWD 1
#define HAVE_GETHOSTNAME 1
#define HAVE_GETPWUID 1
#define HAVE_GETTIMEOFDAY 1
#define HAVE_INTTYPES_H 1
#define HAVE_LIMITS_H 1
#define HAVE_LOCALE_H 1
#define HAVE_NETINET_IN_H 1
#define HAVE_PWD_H 1
#define HAVE_SETLOCALE 1
#define HAVE_STDDEF_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRCASECMP 1
#define HAVE_STRERROR 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_STRNCASECMP 1
#define HAVE_SYS_SELECT_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1
#define HAVE_USLEEP 1
#define HAVE_VSNPRINTF 1
#define HAVE_WORKING_VSNPRINTF 1

#define SIZEOF_INT __SIZEOF_INT__
#define SIZEOF_LONG __SIZEOF_LONG__
#define SIZEOF_SHORT __SIZEOF_SHORT__
#define SIZEOF_SIZE_T __SIZEOF_SIZE_T__
#define SIZEOF_VOID_P __SIZEOF_POINTER__

typedef char fc_map_native_int_size[(sizeof(int) == SIZEOF_INT) ? 1 : -1];
typedef char fc_map_native_long_size[(sizeof(long) == SIZEOF_LONG) ? 1 : -1];
typedef char fc_map_native_short_size[(sizeof(short) == SIZEOF_SHORT) ? 1 : -1];
typedef char fc_map_native_pointer_size[(sizeof(void *) == SIZEOF_VOID_P) ? 1 : -1];

#define MAJOR_VERSION 1
#define MINOR_VERSION 14
#define PATCH_VERSION 1
#define VERSION_STRING "1.14.1"
#define PACKAGE "freeciv"
#define RETSIGTYPE void

#endif
