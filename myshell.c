#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <wait.h>
#include <fcntl.h>
#include <signal.h>


/*
 * In this assignment I implemented a linux shell behaviour and gained experience with process management,
 * pipes, signals, and the relevant system calls.
 * credit- for all the code I have writen in this assignment I have to give credit for Code Vault YouTube videos.
 * link- https://www.youtube.com/playlist?list=PLfqABt5AS4FkW5mOn2Tn9ZZLLDwA3kZUY
 * in this Unix Processes in C playlist, I have learned all the methods and structures I have needed for this assignment.
 * except for dealing with zombies processes which I gave credit locally.
 */


int prepare(void);
int process_arglist(int count, char **arglist);
int finalize(void);
void reset_handler_of_sigint(void);
static void child_handler(int sig);
sigset_t signals; //the set of signals whose delivery is currently blocked for the caller

/*
 * this function modifies handling of SIGINT and SIGCHLD signals.
 */
int prepare(void){
    // Establish handler.  //Eran's trick
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = child_handler;

    if(sigaction(SIGCHLD, &sa, NULL) == -1){
        fprintf(stderr, "sigchild handler change " "failed. %s\n", strerror(errno));
        exit(1);
    }

    sigemptyset(&signals); //initializes the signal set pointed to by signals
    sigaddset(&signals, SIGINT); //sends SIGINT to signals
    struct sigaction ignore_sigint;
    memset(&ignore_sigint,0,sizeof(ignore_sigint));
    ignore_sigint.sa_handler = SIG_IGN;
    ignore_sigint.sa_flags = SA_RESTART;
    if(sigaction(SIGINT, &ignore_sigint,NULL) == -1){ // changing the handler to SIGINT for all the processes.
        fprintf(stderr, "sigint handler change " "failed. %s\n", strerror(errno)); //https://stackoverflow.com/questions/39002052/how-i-can-print-to-stderr-in-c
        exit(1);
    }
    return 0;
}

/*
 * custom handler for SIGCHLD signals.
 */
static void child_handler(int sig) //Eran's trick https://stackoverflow.com/questions/7171722/how-can-i-handle-sigchld/7171836#7171836
{
    int status,pid;
    int errno_before = errno;


    // EEEEXTEERMINAAATE!

    while((pid = waitpid(-1, &status, WNOHANG)) > 0)
        ;
    if(pid == -1 && errno!=EINTR && errno!=ECHILD){
        fprintf(stderr, "failed with reaping zombies %s\n", strerror(errno));
        exit(1);
    }
    errno = errno_before;
}


/*
 * default modifier handler for SIGINT signals
 */
void reset_handler_of_sigint(void){
    struct sigaction def_sigint;
    def_sigint.sa_handler = SIG_DFL;
    def_sigint.sa_flags = SA_RESTART;
    if(sigaction(SIGINT, &def_sigint,NULL) == -1){ // changing the handler to SIGINT for all the processes to ignore.
        fprintf(stderr, "failed with handling sigint %s\n", strerror(errno));
        exit(1);
    }
}

/*
 * Input-
 * This function receives an array arglist with count non-NULL words. This array contains
 * the parsed command line. The last entry in the array, arglist[count], is NULL.
 * Output-
 * 1. The process_arglist() function should not return until every foreground child process it
 * created exits.
 * 2. In the original (shell/parent) process, process_arglist() should return 1 if no error occurs. (This
 * makes sure the shell continues processing user commands.) If process_arglist() encounters an
 * error, it should print an error message and return 0. (See below for what constitutes an error.)
 */
int process_arglist(int count, char **arglist) {

    int kind_of_command = 0;
    int index_for_pipe = 0;
    if (!strcmp(arglist[count - 1],
                "&")) { kind_of_command = 1; } //in this case we need to execute the cmd in the background.
    if (count >= 2){
        if (!strcmp(arglist[count - 2],">")){
            kind_of_command = 3; }//in this case we need to execute the cmd in output redirecting order.
        }
    for (int i = 0; i < count; ++i) {
        if (!strcmp(arglist[i], "|")) {
            kind_of_command = 2;
            index_for_pipe = i;
            arglist[i] = NULL;
        }//in this case we need to execute the cmd in single piping format.
    }
    int pid,pid1,pid2,status_ptr,wait_value;
    int fd[2] = {0,0};
    // else- kind_of_command = 0 , and we need to execute the cmd regularly.
    switch (kind_of_command) {

        case 0:  // executing commands regularly

            sigprocmask(SIG_BLOCK, &signals, NULL);
            pid1 = fork();
            if (pid1 == 0) {
                // Child Process
                reset_handler_of_sigint(); //this process is foreground and therefor we need to change back his SIGINT handling to default.
                sigprocmask(SIG_UNBLOCK, &signals, NULL);
                int status_code = execvp(arglist[0], arglist);
                if (status_code == -1) {
                    fprintf(stderr, "didnt execute command, with error : %s\n", strerror(errno));
                    exit(1);
                }
            } else {
                // Parent process.
                sigprocmask(SIG_UNBLOCK, &signals, NULL);
                if (pid1 == -1) {
                    fprintf(stderr, "failed on fork, with error: %s\n", strerror(errno));
                    return 0; }
                wait_value = waitpid(pid1, &status_ptr, 0);
                if(wait_value == -1 && errno!=EINTR && errno!=ECHILD){
                    fprintf(stderr, "wait failed, with error: %s\n", strerror(errno));
                    return 0;
                }
                return 1;
            }
            break;



        case 1: // executing commands in the background

            pid1 = fork();
            if (pid1 == 0) {
                // Child Process
                struct sigaction ignore_sigint;
                memset(&ignore_sigint,0,sizeof(ignore_sigint));
                ignore_sigint.sa_handler = SIG_IGN;
                ignore_sigint.sa_flags = SA_RESTART;
                if(sigaction(SIGINT, &ignore_sigint,NULL) == -1){ // changing the handler to SIGINT for all the processes.
                    fprintf(stderr, "sigint handler failed, with error : %s\n", strerror(errno));
                    exit(1);
                }
                arglist[count - 1] = NULL; // removing the &
                int status_code = execvp(arglist[0], arglist);
                if (status_code == -1) {
                    fprintf(stderr, "didnt execute command, with error : %s\n", strerror(errno));
                    exit(1);
                }
            }
            if (pid1 == -1) {
                fprintf(stderr, "failed on fork, with error: %s\n", strerror(errno));
                return 0; }
            // Parent process.
            return 1;
            break;




        case 2: //single piping

            sigprocmask(SIG_BLOCK, &signals, NULL);
            if (pipe(fd) == -1) {
                fprintf(stderr, "failed on single piping, with error: %s\n", strerror(errno));
                return 0;
            }
            pid1 = fork();
            if (pid1 == 0) {
                //Child1 process
                sigprocmask(SIG_UNBLOCK, &signals, NULL);
                reset_handler_of_sigint();//this process is foreground and therefor we need to change back his SIGINT handling to default.
                close(fd[0]);
                if(dup2(fd[1], STDOUT_FILENO) == -1){
                    fprintf(stderr, "dup2 failed, with error: %s\n", strerror(errno));
                    exit(1);
                }
                close(fd[1]);
                int status_code1 = execvp(arglist[0], arglist);
                if (status_code1 == -1) {
                    fprintf(stderr, "didnt execute command, with error : %s\n", strerror(errno));
                    exit(1);}
                } else { // Parent process

                    sigprocmask(SIG_UNBLOCK, &signals, NULL);
                    if (pid1 == -1) {
                        fprintf(stderr, "failed on fork, with error: %s\n", strerror(errno));
                        return 0;
                    }
                    sigprocmask(SIG_BLOCK, &signals, NULL);
                    pid2 = fork();
                    if (pid2 == 0) { //Child2 process
                        sigprocmask(SIG_UNBLOCK, &signals, NULL);
                        reset_handler_of_sigint();//this process is foreground and therefor we need to change back his SIGINT handling to default.
                        close(fd[1]);
                        if(dup2(fd[0], STDIN_FILENO) == -1){
                            fprintf(stderr, "dup2 failed, with error: %s\n", strerror(errno));
                            exit(1);
                        }
                        close(fd[0]);
                        int status_code2 = execvp(arglist[index_for_pipe + 1], arglist + index_for_pipe + 1);
                        if (status_code2 == -1) {
                            fprintf(stderr, "didnt execute command, with error : %s\n", strerror(errno));
                            exit(1);
                        }
                    }
                    sigprocmask(SIG_UNBLOCK, &signals, NULL);

                    if (pid2 == -1) {
                        fprintf(stderr, "failed on fork, with error: %s\n", strerror(errno));
                        return 0;
                    }
                    sigprocmask(SIG_BLOCK, &signals, NULL);

                    close(fd[0]);
                    close(fd[1]);
                    wait_value = waitpid(pid1, &status_ptr, 0);
                    if(wait_value == -1 && errno!=EINTR && errno!=ECHILD){
                        fprintf(stderr, "wait failed, with error: %s\n", strerror(errno));
                        return 0;
                    }
                    wait_value = waitpid(pid2, &status_ptr, 0);
                    if(wait_value == -1 && errno!=EINTR && errno!=ECHILD){
                        fprintf(stderr, "wait failed, with error: %s\n", strerror(errno));
                        return 0;
                    }
                    return 1;
                    }
            break;



                case 3: // output redirecting

                    sigprocmask(SIG_BLOCK, &signals, NULL);
                    pid = fork();

                    if (pid == 0) { // Child process
                        sigprocmask(SIG_UNBLOCK, &signals, NULL);
                        reset_handler_of_sigint();//this process is foreground and therefor we need to change back his SIGINT handling to default.
                        int file = open(arglist[count - 1],O_RDWR | O_CREAT | O_TRUNC, 0664);
                        if (file == -1) {
                            fprintf(stderr, "didnt open file, with error : %s\n", strerror(errno));
                            exit(1);
                        }
                        if(dup2(file, STDOUT_FILENO) == -1){
                            fprintf(stderr, "dup2 failed, with error: %s\n", strerror(errno));
                            exit(1);
                        }
                        close(file);
                        arglist[count - 2] = NULL;
                        int status_code = execvp(arglist[0], arglist);
                        if (status_code == -1) {
                            fprintf(stderr, "didnt execute command, with error : %s\n", strerror(errno));
                            exit(1);
                        }
                    } else{
                        // Parent process
                        sigprocmask(SIG_UNBLOCK, &signals, NULL);
                        if (pid == -1) {
                            fprintf(stderr, "failed on fork, with error: %s\n", strerror(errno));
                            return 0; }
                        wait_value = waitpid(pid, &status_ptr, 0);
                        if(wait_value == -1 && errno!=EINTR && errno!=ECHILD){
                            fprintf(stderr, "wait failed, with error: %s\n", strerror(errno));
                            return 0;
                        }
                        return 1;
                    }
                    break;

    }
    return 1; //should never get here
}

/*
 * no need for any finalization.
 */
int finalize(void){
    return 0;}