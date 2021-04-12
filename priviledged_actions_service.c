/*
 * This program is intended to be run as a background service(daemon)
 *
 * It listens to messages sent to "localhost:PORT" (PORT is defined below)
 * It is currently configured to toggle wlan state or to execute any other command
 *
 * To toggle wlan, the message must be of the format: "toggle MODE",
 *      where MODE is 0 -> when wlan is to be turned on
 *                    1 -> when wlan is to be turned off
 *
 * To execute a command, the message must be of the format: "exec command"
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <linux/rfkill.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define TRUE                    1
#define FALSE                   0
#define MAX_CONNECTIONS         2
#define MAX_BUFFER_SIZE         1024
#define MAX_PROGRAM_NAME_LEN    50

// Address to listen on
#define ADDRESS "127.0.0.1"
#define PORT    14465   // just a random value

// Prefixes to identify the type of command issued
#define EXEC_PREFIX         "exec "
#define WLAN_TOGGLE_PREFIX  "toggle "

/*
 * Following is a list of programs that are allowed to run
 *
 * This is done to avoid security issues, like of course rm -rf /
 */

// number of allowed programs
short allowed_programs_num = 2;

char *allowed_programs[] = {
    "wpa_cli",
    "wpa_supplicant"
};

/* to toggle wlan state */
void toggle_wlan(int mode) {

    int fd;
    struct rfkill_event event;

    if ((fd = open("/dev/rfkill", O_RDWR)) < 1) {
        perror("Can't open RFKILL control device");
        exit(EXIT_FAILURE);
    }

    memset(&event, 0, sizeof(event));
    event.op = RFKILL_OP_CHANGE_ALL;
    event.type = RFKILL_TYPE_WLAN;
    event.soft = mode;

    if ((write(fd, &event, sizeof(event))) < 0) {
        perror("Failed to change RFKILL state");
    }

    close(fd);
}

/* To check if programs is allowed to run */
int program_is_valid(char *buffer) {

    char program_name[MAX_PROGRAM_NAME_LEN];
    short count, index;

    /* Parse program name from buffer */
    index = strlen(EXEC_PREFIX);

    for(count = 0; buffer[index] != '\0' && buffer[index] != ' '; count++) {
        program_name[count] = buffer[index];
        index++;
    }

    program_name[count] = '\0';

    for(short program_count = 0; program_count < allowed_programs_num; program_count++)
        if(!(strncmp(program_name, allowed_programs[program_count], count)))
            return TRUE;

    fprintf(stderr, "%s: program specified is not allowed!\n", program_name);
    return FALSE;
}

/* to start wpa_supplicant */
void execute_cmd(char *buffer, int buffer_len) {

    pid_t pid;
    char *cmd;
    short index = strlen(EXEC_PREFIX);

    /*
     *  Allocate memory for the cmd
     *  To basically just strip off the EXEC_PREFIX
     */

    cmd = (char *)malloc(buffer_len - index);
    for(short count = 0; buffer[index] != '\0'; count++) {
        cmd[count] = buffer[index];
        index++;
    }

    /* Now execute the command issued */
    if((pid = fork()) < 0) {
        perror("fork() failed!");
        exit(EXIT_FAILURE);
    }

    if(pid == 0) {
        // It's the child process, execute the command and terminate
        system(cmd);
        exit(EXIT_SUCCESS);
    }

    free(cmd);
}

void signal_handler(int signum) {
    printf("SIGNAL %d received, now exiting the daemon..\n", signum);
    exit(EXIT_SUCCESS);
}

int main(void) {

    int server_sock, client_sock;
    char buffer[1024];
    socklen_t buffer_len;
    struct sockaddr_in server_address, client_address;

    /* Set up signal handlers so it is not a forever loop */
    signal(SIGTERM, signal_handler);

    /* Initialize the sockets and listen to events */
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket() failed!");
        exit(1);
    }

    /* Set address and port to listen to */
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = inet_addr(ADDRESS);
    memset(server_address.sin_zero, '\0', sizeof(server_address.sin_zero));

    /* bind to the above address */
    bind(server_sock, (struct sockaddr *) &server_address, sizeof(server_address));

    while (TRUE) {
        if((listen(server_sock, MAX_CONNECTIONS)) != 0) {
            perror("Error While Listening!");
            exit(EXIT_FAILURE);
        }

        /* Accept the connection */
        client_sock = accept(server_sock,(struct sockaddr *) &client_address, &buffer_len);

        recv(client_sock, buffer, buffer_len, 0);

        /* Now process the message */

        /*
         * !!DUMB TERRITORY AHEAD!!
         *
         * These are very dumb statements.
         * But since this is just a personal project for now,
         * let it just be efficient. It doesn't have to be foolproof.
         */

        if(!(strncmp(WLAN_TOGGLE_PREFIX, buffer, strlen(EXEC_PREFIX)))) {
            // toggle wlan
            toggle_wlan(buffer[7] - '0');   // mode is supposed to be at index 7
        } else {
            // execute command
            if(program_is_valid(buffer)) {
                execute_cmd(buffer, buffer_len);
            }
        }
    }

    return 0;
}
