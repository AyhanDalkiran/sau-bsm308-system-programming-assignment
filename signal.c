#define _POSIX_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdbool.h>

// Child Signal Handler
void sig_int(int signum) {
    (void)signum; // Unused parameter, compiler does not shut up about it
    fprintf(stdout, "Child: SIGINT received but continuing...\n");

    // Override SIG_DFL behaviour
    signal(SIGINT, sig_int);
}

void sig_cont(int signum) {
    (void)signum; // Unused parameter, compiler does not shut up about it
    fprintf(stdout, "Child: Process resumed\n");

    // Override SIG_DFL behaviour
    signal(SIGCONT, sig_cont);
}

void sig_alrm_child(int signum) {
    (void)signum; // Unused parameter, compiler does not shut up about it
    static unsigned int child_timer = 0;
    fprintf(stdout, "Child counter: %u\n", child_timer);
    child_timer++;
    alarm(1);

    // Override SIG_DFL behaviour
    signal(SIGALRM, sig_alrm_child);
}

pid_t child_pid;
unsigned int parent_timer = 0;
bool child_running = true;

// Parent Signal Handler
void sig_alrm_parent(int signum) {
    (void)signum; // Unused parameter, compiler does not shut up about it
    
    if (parent_timer >= 10) { // 10 seconds passed, terminate child
        kill(child_pid, SIGTERM);
        fprintf(stdout, "Parent: Child terminated.\n");
        parent_timer += 1;
    }
    else if (child_running) { // Stop child
        fprintf(stdout, "Parent: Stopping child...\n");
        kill(child_pid, SIGSTOP);
        child_running = false;
        parent_timer += 3;

        alarm(2);
    }
    else { // Continuing child
        fprintf(stdout, "Parent: Continuing child...\n");
        kill(child_pid, SIGCONT);
        child_running = true;
        parent_timer += 2;

        alarm(3);
    }

    signal(SIGALRM, sig_alrm_parent);
}

int main(void) {
    if ((child_pid = fork()) == -1) { // Fork failure
        fprintf(stderr, "error: Could not fork process\n");
        return EXIT_FAILURE;
    }

    if (child_pid == 0) { // Child Process
        signal(SIGINT, sig_int);
        signal(SIGCONT, sig_cont);
        signal(SIGALRM, sig_alrm_child);
        alarm(1);

        while(true);
    }
    else { // Parent Process
        signal(SIGALRM, sig_alrm_parent);

        alarm(3);
        while(parent_timer < 10 || !child_running);
        alarm(0);

        fprintf(stdout, "Parent: Sending SIGINT...\n");
        kill(child_pid, SIGINT);

        alarm(1);
        while (parent_timer < 11);
        alarm(0);
    }

    return EXIT_SUCCESS;
}
