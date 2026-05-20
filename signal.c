#define _POSIX_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdbool.h>

// Child Signal Handler
void sig_child(int sig_code) {
    if (sig_code == SIGINT) fprintf(stdout, "Child: SIGINT received but continuing...");
    else if(sig_code == SIGCONT) fprintf(stdout, "Child: Process resumed");
    else fprintf(stderr, "Child: Unhandled signal code '%d'", sig_code);
}

pid_t child_pid;
unsigned int timer = 0;
bool child_running = true;

// Parent Signal Handler
void sig_parent(int sig_code) {
    if (sig_code == SIGALRM) {
        if (child_running) {
            timer += 3;

            fprintf(stdout, "Parent: Stopping child...");
            kill(child_pid, SIGSTOP);
            child_running = false;

            alarm(2);
        }
        else {
            timer += 2;

            fprintf(stdout, "Parent: Continuing child...");
            kill(child_pid, SIGCONT);
            child_running = true;

            alarm(3);
        }
    }
    else fprintf(stderr, "Parent: Unhandled signal code '%d'", sig_code);
}

int main(void) {
    if ((child_pid = fork()) == -1) { // Fork failure
        fprintf(stderr, "error: Could not fork process");
        return EXIT_FAILURE;
    }

    // Don't buffer output
    setvbuf(stdout, NULL, _IONBF, 0);

    if (child_pid == 0) { // Child Process
        signal(SIGINT, sig_child);
        signal(SIGCONT, sig_child);

        while(true);
    }
    else { // Parent Process
        signal(SIGALRM, sig_parent);

        alarm(3);
        while(timer < 10);
        while(!child_running);
        alarm(0);

        fprintf(stdout, "Parent: Sending SIGINT...");
        kill(child_pid, SIGINT);

        fprintf(stdout, "Parent: Sending SIGTERM...");
        kill(child_pid, SIGTERM);
    }

    return EXIT_SUCCESS;
}
