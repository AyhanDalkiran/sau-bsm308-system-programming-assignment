#define _POSIX_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdbool.h>

// Child Signal Handler
void sig_int() {
    fprintf(stdout, "Child: SIGINT received but continuing...\n");

    // Override SIG_DFL behaviour
    signal(SIGINT, sig_int);
}

void sig_cont() {
    fprintf(stdout, "Child: Process resumed\n");

    // Override SIG_DFL behaviour
    signal(SIGCONT, sig_cont);
}

pid_t child_pid;
unsigned int timer = 0;
bool child_running = true;

// Parent Signal Handler
void sig_alrm() {
    alarm(0); // Cancel any pending alarms

    if (child_running) {
        timer += 3;

        fprintf(stdout, "Parent: Stopping child...\n");
        kill(child_pid, SIGSTOP);
        child_running = false;

        alarm(2);
    }
    else {
        timer += 2;

        fprintf(stdout, "Parent: Continuing child...\n");
        kill(child_pid, SIGCONT);
        child_running = true;

        alarm(3);
    }

    signal(SIGALRM, sig_alrm);
}

int main(void) {
    if ((child_pid = fork()) == -1) { // Fork failure
        fprintf(stderr, "error: Could not fork process");
        return EXIT_FAILURE;
    }

    // Don't buffer output
    setvbuf(stdout, NULL, _IONBF, 0);

    if (child_pid == 0) { // Child Process
        signal(SIGINT, sig_int);
        signal(SIGCONT, sig_cont);

        while(true);
    }
    else { // Parent Process
        signal(SIGALRM, sig_alrm);

        alarm(3);
        while(timer <= 10 || !child_running);

        fprintf(stdout, "Parent: Sending SIGINT...\n");
        kill(child_pid, SIGINT);

        fprintf(stdout, "Parent: Sending SIGTERM...\n");
        kill(child_pid, SIGTERM);
    }

    return EXIT_SUCCESS;
}
