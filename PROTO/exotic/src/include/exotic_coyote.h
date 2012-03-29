#ifndef EXOTIC_COYOTE_H_
# define EXOTIC_COYOTE_H_

#ifdef EXOTIC_USE_COYOTE

# define EXOTIC_PROVIDE_CLOCK_GETTIME
# define EXOTIC_PROVIDE_GETTIMEOFDAY
# define EXOTIC_PROVIDE_USLEEP
# define EXOTIC_PROVIDE_STRINGS
# define EXOTIC_PROVIDE_FNMATCH
# define EXOTIC_PROVIDE_STDLIB
# define EXOTIC_PROVIDE_STDIO
# define EXOTIC_PROVIDE_MMAP
# define EXOTIC_PROVIDE_INET
# define EXOTIC_PROVIDE_REALLOC
# define EXOTIC_PROVIDE_FREE
# define EXOTIC_PROVIDE_BASENAME
# define EXOTIC_PROVIDE_DIRNAME
# define EXOTIC_NO_SIGNAL
# define EXOTIC_NO_SELECT

# define LONG_BIT 32
# define CLOCK_MONOTONIC 0

#endif /* EXOTIC_USE_COYOTE */

#endif /* EXOTIC_COYOTE_H_ */
