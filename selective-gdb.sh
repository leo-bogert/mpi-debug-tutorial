#!/bin/bash

print_syntax() {
cat >&2 <<EOF
Syntax: $0 command [argument*]

This script will execute the given command with the given arguments.
It will wait for the command to exit OR send SIGUSR1 to the script.
If SIGUSR1 is sent, it will attach gdb to the command interactively so you can debug.

In C, to send SIGUSR1 to the script, use "kill(getppid(), SIGUSR1);".
If you want gdb to be attached at the precise location of kill(), add a "sleep(10);" after it:
kill() does not wait for the signal to be delivered, execution would continue without sleep().
To make those functions available, add the following BEFORE your includes:

#define _POSIX_SOURCE
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
EOF
}

# Returns EXIT_SUCCESS (= 0) if a process has been paused
is_process_suspended() {
    process_state="$(ps --pid "$1" --no-headers -o state)"

    if [ "$process_state" = "T" ] ; then
	return 0
    else
	return 1
    fi
}

main() {
    if [ $# -lt 1 ] ; then
	print_syntax
	exit 1
    fi

    command="$1"
    
    # Remove command from the parameter list
    shift 1

    # Be careful to understand what the following line does:
    # - It starts a subshell as a backround job
    # - That subshell immediately suspends itself by sending SIGSTOP to itself.
    #   (This means that execution of the subprocess is paused until it receives SIGCONT)
    # - If we sent SIGCONT to the subshell in the future, it will use "exec" to execute the
    #   actual command. Notice that exec replaces the whole process with the command instead
    #   of starting a new one. This is needed so the PID stays the same and we can tell the
    #   PID to GDB. Also, it is faster.
    # Why do we do this? Imagine we started the actual command's execution immediately in background:
    # It might happen that it sent the "debug me" signal to us before we could set up the trap on it.
    # We cannot just set up the trap before the process is started because we need to know the PID
    # for GDB.
    ( kill -SIGSTOP "$BASHPID" ; exec "$command" "$@" ) &
    command_pid="$!"

    # Trap signal SIGUSR1. Upon receiving it, execute gdb with command name and PID as parameters.
    # Notice:
    #    The gdb manpage specifies that it wants the command name even when being given a PID.
    #    This seems weird but it clearly states that it will attach to the existing PID instead
    #    of executing the command again.
    trap "gdb '$command' '$command_pid'" SIGUSR1

    # Now that we have trapped SIGUSR1, the subshell can continue execution to run the main command
    # But first, we must avoid a race condition: It might take some time for the subshell to send
    # suspend itself. So we cannot immediately send SIGCONT for it, we first must check whether
    # the process is actually suspended
    while ! is_process_suspended "$command_pid" ; do
	sleep 0.1
    done

    # Resume the subshell to run the command
    kill -SIGCONT "$command_pid"

    # Wait for the command to exit
    wait
}

main "$@"