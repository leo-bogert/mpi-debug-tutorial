#ifndef SELECTIVE_DEBUG_H_INCLUDED
#define SELECTIVE_DEBUG_H_INCLUDED

#define _POSIX_SOURCE // for kill()
#include <sys/types.h> // for getppid(), kill()
#include <signal.h> // for kill()
#include <unistd.h> // for getppid()

#define BREAKPOINT_AND_SLEEP(x) { kill(getppid(), SIGUSR1); sleep(x); }

#endif
