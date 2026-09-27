// p4.c —— 原书 Figure 5.4：在 fork 与 exec 之间做重定向，相当于 shell 的 wc p4.c > p4.output
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    int rc = fork();
    if (rc < 0) {
        // fork failed
        fprintf(stderr, "fork failed\n");
        exit(1);
    } else if (rc == 0) {
        // child: redirect standard output to a file
        close(STDOUT_FILENO);                       // 腾出 fd 1
        open("./p4.output", O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU); // 最小空闲 fd 正是 1
        // now exec "wc"...
        char *myargs[3];
        myargs[0] = strdup("wc");       // program: wc
        myargs[1] = strdup("p4.c");     // arg: file to count
        myargs[2] = NULL;               // mark end of array
        execvp(myargs[0], myargs);      // runs word count
    } else {
        // parent goes down this path (main)
        int rc_wait = wait(NULL);
        (void) rc_wait;
    }
    return 0;
}
