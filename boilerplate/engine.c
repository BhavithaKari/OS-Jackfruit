#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#define STACK_SIZE (1024 * 1024)

struct container_args {
    char *rootfs;
    char **cmd;
};

static int child_fn(void *arg)
{
    struct container_args *args = (struct container_args *)arg;

    /* Set hostname */
    if (sethostname("mini-container", 14) != 0) {
        perror("sethostname");
        return 1;
    }

    /* Change root */
    if (chroot(args->rootfs) != 0) {
        perror("chroot failed");
        return 1;
    }

    if (chdir("/") != 0) {
        perror("chdir failed");
        return 1;
    }

    /* Mount required filesystems */
    if (mount("proc", "/proc", "proc", 0, NULL) != 0)
        perror("mount /proc");

    if (mount("sysfs", "/sys", "sysfs", 0, NULL) != 0)
        perror("mount /sys");

    if (mount("/dev", "/dev", NULL, MS_BIND | MS_REC, NULL) != 0)
        perror("bind /dev");

    /* Execute command */
    execvp(args->cmd[0], args->cmd);
    perror("exec failed");
    return 1;
}

int main(int argc, char *argv[])
{
    if (argc < 5) {
        fprintf(stderr,
            "Usage: %s run <id> <container-rootfs> <command>\n",
            argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "run") != 0) {
        fprintf(stderr, "Unknown command\n");
        return 1;
    }

    char *container_id = argv[2];      // not used yet
    char *rootfs = argv[3];
    char **cmd = &argv[4];

    (void)container_id;  // avoid unused warning

    struct container_args args;
    args.rootfs = rootfs;
    args.cmd = cmd;

    char *stack = malloc(STACK_SIZE);
    if (!stack) {
        perror("malloc");
        return 1;
    }

    char *stack_top = stack + STACK_SIZE;

    int flags = CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS | SIGCHLD;

    pid_t pid = clone(child_fn, stack_top, flags, &args);
    if (pid < 0) {
        perror("clone failed");
        free(stack);
        return 1;
    }

    waitpid(pid, NULL, 0);
    free(stack);

    printf("Container exited.\n");
    return 0;
}
