/*-
 * main.c
 * Minishell C source
 * Shows how to use "obtain_order" input interface function.
 */

#include <stddef.h>         /* NULL */
#include <stdio.h>          /* setbuf, printf */
#include <stdlib.h>
#include <string.h>         /* strcmp */
#include <unistd.h>         /* chdir, fork */
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>       /* umask */
#include <values.h>         /* LONG_MIN, LONG_MAX */
#include <sys/times.h>      /* times */
#include <fcntl.h>          /* open */
#include <pwd.h>            /* struct passwd */
#include <glob.h>


extern int obtain_order();        /* See parser.y for description */


int getCommandName(char *commandName) {
    int res = 0;
    if (!strcmp(commandName, "cd"))
        res = 1;
    else if (!strcmp(commandName, "umask"))
        res = 2;
    else if (!strcmp(commandName, "time"))
        res = 3;
    else if (!strcmp(commandName, "read"))
        res = 4;

    return res;
}


int getArgc(char **argv) {
    int i;
    for (i = 0; argv[i]; i++);
    return i;
}

int externalCommand(char **argv, int bg) {
    int errno = -10;

    if (bg) {
        execvp(argv[0], &argv[0]);
        perror("Exec()");
        exit(errno);
    }

    int status;
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork()");
        return errno;
    }
    if (pid == 0) {
        // Proceso hijo, dejamos las señales activas si estamos en primer plano
        struct sigaction act;
        act.sa_flags = 0;
        act.sa_handler = SIG_DFL;
        sigaction(SIGINT, &act, NULL);
        sigaction(SIGQUIT, &act, NULL);

        execvp(argv[0], &argv[0]);
        perror("Exec()");
        exit(errno);
    } else {
        int res = waitpid(pid, &status, 0);
        if (res == -1) {
            perror("waitpid():");
            return errno;
        }
    }
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    else
        return errno;
}

int cdCommand(int argc, char **argv) {
    int errno = 10;
    if (argc > 2) {
        fprintf(stderr, "Error: No puedes poner mas de 1 argumento en el mandato \"cdCommand\"\n");
        return errno;
    } else {
        char *targetDir;
        if (argc == 1) {
            targetDir = getenv("HOME");
        } else if (argc == 2) {
            targetDir = argv[1];
        }
        char *cwd;
        switch (chdir(targetDir)) {
            case -1:
                perror("Directorio invalido");
                return errno;
            default:
                cwd = getcwd(NULL, 100);
                if (!cwd) {
                    perror(NULL);
                    return errno;
                }
                printf("%s\n", cwd);
        }
    }
    return 0;
}

int umaskCommand(int argc, char **argv) {
    int errno = 20;
    if (argc > 2) {
        fprintf(stderr, "Error: No puedes poner mas de 1 argumento en el mandato \"umask\"\n");
        return errno;
    } else if (argc == 2) {
        char *endptr = NULL;
        long int mask = strtol(argv[1], &endptr, 8);
        if (argv[1] == endptr || mask == LONG_MIN || mask == LONG_MAX) { //Underflow, overflow
            perror("Mascara invalida");
            return errno;
        }
        mode_t prevMask = umask(mask);
        printf("%o\n", prevMask);
    } else if (argc == 1) {
        mode_t prevMask = umask(0);
        umask(prevMask);
        printf("%o\n", prevMask);
    }
    return 0;
}

int timeCommand(int argc, char **argv) {
    int errno = 30;

    struct tms time;
    clock_t clock;

    if (argc == 1) {

        clock = times(&time);
        if (clock == -1) {
            perror("timeCommand");
            return errno;
        }
        printf("%d.%03du %d.%03ds %d.%03dr\n", clock * 0.01, clock % 1000, time.tms_utime * 0.01, time.tms_utime % 1000,
               time.tms_stime * 0.01,
               time.tms_stime % 1000);
    } else {
        clock = times(&time);
        if (clock == -1) {
            perror("timeCommand");
            return errno;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("timeCommand: fork()");
            return errno;
        }
        if (pid == 0) {
            execvp(argv[1], &argv[1]);
            perror("timeCommand: exec()");
            exit(errno);
        } else
            wait(NULL);

        clock_t clockpost;

        clockpost = times(&time);
        if (clockpost == -1) {
            perror("timeCommand");
            return errno;
        }
        int difference = (clockpost - clock) * 0.01;
        printf("Consumo: %ds\n", difference);
    }
    return 0;
}

int readCommand(int argc, char **argv) {
    int errno = 40;

    char *buffer = (char *) calloc(300, sizeof(char));
    fgets(buffer, 299, stdin);

    if (argc < 2) {
        fprintf(stderr, "Error: Debes poner argumentos en el mandato \"read\"\n");
        return errno;
    }

    if (strlen(buffer) <= 1) {
        fprintf(stderr, "read: linea vacia\n");
        return errno;
    }

    char *token = strtok(buffer, " \n\t");
    int i;
    for (i = 1; i < argc && token != NULL; i++) {
        char *var = (char *) calloc(strlen(argv[i]) + strlen(token) + 2, sizeof(char));
        sprintf(var, "%s=%s", argv[i], token);

        if (putenv(var)) {
            perror("putenv() read");
            return errno;
        }
        if (i == argc - 2)
            token = strtok(NULL, "\n");
        else
            token = strtok(NULL, " \n\t");


    }
    free(buffer);
    return 0;
}

int doOneCommand(char **argv, int bg) {
    int argc = getArgc(argv);
    int commandName = getCommandName(argv[0]);
    int res = 0;
    switch (commandName) {
        case 0:
            res = externalCommand(argv, bg);
            break;
        case 1:
            res = cdCommand(argc, argv);
            break;
        case 2:
            res = umaskCommand(argc, argv);
            break;
        case 3:
            res = timeCommand(argc, argv);
            break;
        case 4:
            res = readCommand(argc, argv);
            break;
    }
    return res;
}

int doAllCommands(char ***argvv, int argvc, int bg) {
    if (argvc == 1) {
        return doOneCommand(argvv[0], bg);
    }

    int errno = 3;
    char **argv = NULL;
    int pipes[argvc - 1][2];
    int i;
    for (i = 0; i < argvc - 1; i++) {
        if (pipe(pipes[i])) {
            perror("pipe():");
            return errno;
        }
    }

    int j;
    pid_t pid;
    int k;
    for (j = 0; j < argvc - 1; j++) {
        pid = fork();
        if (pid < 0) {
            perror("fork()");
            return errno;
        }
        if (pid != 0) {
            continue;
        }
        if (j == 0) {
            if (close(1)) {
                perror("close() j == 0:");
                return errno;
            }
            if (dup(pipes[j][1]) == -1) {
                perror("dup() j == 0:");
                return errno;
            }
        }
        else {
            if (close(0)) {
                perror("close() else:");
                return errno;
            }
            if (dup(pipes[j - 1][0]) == -1) {
                perror("dup() else:");
                return errno;
            }

            if (close(1)) {
                perror("close() else:");
                return errno;
            }
            if (dup(pipes[j][1]) == -1) {
                perror("dup() else:");
                return errno;
            }
        }

        for (k = 0; k < argvc - 1; k++) {//Cerramos todos los demas descriptores
            if (close(pipes[k][1])) {
                perror("close() all child:");
                return errno;
            }
            if (close(pipes[k][0])) {
                perror("close() all child:");
                return errno;
            }
        }
        argv = argvv[j];
        int ret = doOneCommand(argv, bg);

        exit(ret);

    }

    int oldfd;
    oldfd = dup(0);
    if (oldfd == -1) {
        perror("doAllCommands: dup():");
        return errno;
    }

    if (close(0)) {
        perror("close() j == argvc - 1:");
        return errno;
    }
    if (dup(pipes[argvc - 1 - 1][0]) == -1) {
        perror("dup() j == argvc - 1:");
        return errno;
    }

    for (k = 0; k < argvc - 1; k++) {//Cerramos todos los demas descriptores
        if (close(pipes[k][1])) {
            perror("close() all parent:");
            return errno;
        }
        if (close(pipes[k][0])) {
            perror("close() all parent:");
            return errno;
        }
    }

    argv = argvv[argvc - 1];
    int ret = doOneCommand(argv, bg);

    if (close(0)) {
        perror("doAllCommands: close(0)");
        return errno;
    }
    if (dup(oldfd) == -1) {
        perror("doAllCommands: dup()");
        return errno;
    }
    if (close(oldfd)) {
        perror("doAllCommands: close()");
        return errno;
    }
    return ret;

}

int redirect(char *filev[], int oldfd[]) {
    int errno = -2;
    int i;
    char *file;
    for (i = 0; i < 3; i++)
        oldfd[i] = -1;
    int newfd;
    for (i = 0; i < 3; i++) {
        file = filev[i];
        if (file != NULL) {
            oldfd[i] = dup(i);
            if (oldfd[i] == -1) {
                perror("redirect: dup():");
                return errno;
            }
            switch (i) {
                case 0:
                    newfd = open(file, O_RDONLY);
                    if (newfd == -1) {
                        perror("redirect: open() i == 0");
                        return errno;
                    }
                    if (close(i)) {
                        perror("redirect: close() i == 0");
                        return errno;
                    }

                    if (dup(newfd) == -1) {
                        perror("redirect: dup()");
                        return errno;
                    }
                    if (close(newfd)) {
                        perror("redirect: close() i == 0");
                        return errno;
                    }
                    break;
                case 1:
                    newfd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    if (newfd == -1) {
                        perror("redirect: open() i == 1");
                        return errno;
                    }
                    if (close(i)) {
                        perror("redirect: close() i == 1");
                        return errno;
                    }
                    if (dup(newfd) == -1) {
                        perror("redirect: dup()");
                        return errno;
                    }
                    if (close(newfd)) {
                        perror("redirect: close() i == 1");
                        return errno;
                    }
                    break;
                case 2:
                    newfd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0666);

                    if (newfd == -1) {
                        perror("redirect: open() i == 2");
                        return errno;
                    }
                    if (close(i)) {
                        perror("redirect: close() i == 2");
                        return errno;
                    }
                    if (dup(newfd) == -1) {
                        perror("redirect: dup()");
                        return errno;
                    }
                    if (close(newfd)) {
                        perror("redirect: close() i == 2");
                        return errno;
                    }
                    break;

            }

        }
    }
    return 0;
}

int restoreRedirection(int oldfd[]) {
    int errno = 4;
    int i;
    int fd;

    for (i = 0; i < 3; i++) {
        fd = oldfd[i];
        if (fd != -1) {
            if (close(i)) {
                perror("restoreRedirection: close(i)");
                return errno;
            }
            if (dup(fd) == -1) {
                perror("restoreRedirection: dup()");
                return errno;
            }
            if (close(fd)) {
                perror("restoreRedirection: close()");
                return errno;
            }
        }
    }
    return 0;
}


int metacharacters(char ***argvv) {
    char **argv = NULL;
    char *commandPart = NULL;
    char character;

    glob_t paths;
    int retval;

    paths.gl_pathc = 0;
    paths.gl_pathv = NULL;
    paths.gl_offs = 0;


    int i;
    int j;
    int k;
    for (i = 0; (argv = argvv[i]); i++) {
        for (j = 0; (commandPart = argv[j]); j++) {

            retval = glob(commandPart, GLOB_NOCHECK,
                          NULL, &paths);
            if (retval == 0) {
                char *bufferr = (char *) calloc(300, sizeof(char));

                for (k = 0; k < paths.gl_pathc; k++) {
                    strcat(bufferr, paths.gl_pathv[k]);
                    if (k != paths.gl_pathc - 1)
                        strcat(bufferr, " ");
                }

                int length = strlen(bufferr) + 1;
                char *changedCommandPart = (char *) calloc(length, sizeof(char));
                strcpy(changedCommandPart, bufferr);

                argv[j] = changedCommandPart;
                free(bufferr);

            }

            for (k = 0; (character = commandPart[k]); k++) {
                if (character == '$') {
                    char buffer[100];
                    sscanf(commandPart + k, " %*[$~]%[_a-zA-Z0-9]", buffer);

                    char variable[strlen(buffer) + 1];
                    strcpy(variable, buffer);

                    char *variableValue;
                    variableValue = getenv(variable);


                    int length = strlen(commandPart) - strlen(variable) - 1 + strlen(variableValue) + 1;
                    char *changedCommandPart = (char *) calloc(length, sizeof(char));
                    int l;
                    for (l = 0; l < k; l++) {
                        changedCommandPart[l] = commandPart[l];
                    }

                    char aux;
                    for (l = 0; (aux = variableValue[l]); l++) {
                        changedCommandPart[k + l] = aux;
                    }

                    int m;
                    for (m = 0; (aux = commandPart[k + strlen(variable) + 1 + m]); m++) {
                        changedCommandPart[k + l + m] = aux;
                    }

                    argv[j] = changedCommandPart;

                } else if (character == '~') {
                    if (commandPart[k + 1] && commandPart[k + 1] != ' ') {
                        char buffer[100];
                        sscanf(commandPart + k, " %*[$~]%[_a-zA-Z0-9]", buffer);

                        char user[strlen(buffer) + 1];
                        strcpy(user, buffer);

                        struct passwd *passwd;
                        passwd = getpwnam(buffer);
                        char *userHome = passwd->pw_dir;

                        int length = strlen(commandPart) - strlen(buffer) - 1 + strlen(userHome) + 1;
                        char *changedCommandPart = (char *) calloc(length, sizeof(char));
                        char aux;
                        int l;
                        for (l = 0; (aux = userHome[l]); l++) {
                            changedCommandPart[k + l] = aux;
                        }
                        argv[j] = changedCommandPart;
                    } else {
                        char *userHome = getenv("HOME");

                        char *changedCommandPart = (char *) calloc(strlen(userHome) + 1, sizeof(char));
                        int l;
                        char aux;
                        for (l = 0; (aux = userHome[l]); l++) {
                            changedCommandPart[l] = aux;
                        }
                        argv[j] = changedCommandPart;

                    }
                }
            }
        }
    }
    return 0;
}


int main(void) {

    int errno = 1;

    setbuf(stdout, NULL);
    setbuf(stdin, NULL);


    pid_t myPid = getpid();
    int ncaracteres = 1;
    int num = myPid;

    while (num / 10 > 0) {
        num = num / 10;
        ncaracteres++;
    }
    char *varPid = (char *) calloc(6 + ncaracteres + 1, sizeof(char));
    sprintf(varPid, "mypid=%d", myPid);
    if (putenv(varPid)) {
        perror("putenv() myPid");
        return errno;
    }

    char *varPrompt = "prompt=\"msh> \"";
    if (putenv(varPrompt)) {
        perror("putenv() myPrompt");
        return errno;
    }

    struct sigaction act;
    act.sa_flags = 0;
    act.sa_handler = SIG_IGN;
    sigaction(SIGINT, &act, NULL);  // Ordena ignorar señal INT
    sigaction(SIGQUIT, &act, NULL); // Ordena ignorar señal QUIT

    char ***argvv = NULL;
    int argvc;
    char *filev[3] = {NULL, NULL, NULL};
    int bg;
    int ret;

    int oldfd[3];
    int ret2;

    pid_t pid;
    char *varBgpid;
    int commandStatus;
    char *varStatus;

    int k;

    while (1) {

        fprintf(stderr, "%s", "msh> ");    /* Prompt */
        ret = obtain_order(&argvv, filev, &bg);
        if (ret == 0) break;        /* EOF */
        if (ret == -1) continue;    /* Syntax error */
        argvc = ret - 1;        /* Line */
        if (argvc == 0) continue;    /* Empty line */

        if (redirect(filev, oldfd) == -2) { //error opening
            for (k = 0; k < 3; k++)
                filev[k] = NULL;

            ret2 = restoreRedirection(oldfd);
            if (ret2)
                return ret2;

            continue;
        }

        ret = metacharacters(argvv);
        if (ret)
            return ret;


        if (bg) {
            pid = fork();
            if (pid < 0) {
                perror("fork()");
                return errno;
            }
            if (pid == 0) { //HIJO
                return doAllCommands(argvv, argvc, bg);
            } else { //PADRE
                ncaracteres = 1;
                num = pid;
                while (num / 10 > 0) {
                    num = num / 10;
                    ncaracteres++;
                }
                varBgpid = (char *) calloc(6 + ncaracteres + 1, sizeof(char));
                sprintf(varBgpid, "bgpid=%d", pid);
                if (putenv(varBgpid)) {
                    perror("putenv() bgpid");
                    return errno;
                }
                fprintf(stderr, "[%d]\n", pid);
            }
        } else {
            commandStatus = doAllCommands(argvv, argvc, bg);

            ncaracteres = 1;
            num = pid;
            while (num / 10 > 0) {
                num = num / 10;
                ncaracteres++;
            }

            varStatus = (char *) calloc(7 + ncaracteres + 1, sizeof(char));
            sprintf(varStatus, "status=%d", commandStatus);
            if (putenv(varStatus)) {
                perror("putenv() commandStatus");
                return errno;
            }
        }

        for (k = 0; k < 3; k++)
            filev[k] = NULL;

        ret2 = restoreRedirection(oldfd);
        if (ret2)
            return ret2;

    }
    return 0;
}
