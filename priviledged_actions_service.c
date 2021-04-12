/*
 * This program is intended to be run as a background service(daem
 *
 * It listens to messages sent to "localhost:PORT" (PORT is defined below)
 * It is currently configured to toggle wlan state or to start wpa_supplicant
 *
 * To toggle wlan, the message must be of the format: "toggle MODE",
 *      where MODE is 0 -> when wlan is to be turned on
 *                    1 -> when wlan is to be turned off
 *
 * To start wpa_supplicant, message must be just the exact command, you'd execute to start it
 *      e.g: "wpa_supplicant -D nl80211 -i wlan0 -c wpa_supplicant.conf"
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

#define TRUE    1
#define MAX_CONNECTIONS 2
#define MAX_BUFFER_SIZE 1024

// Address to listen on
#define ADDRESS "127.0.0.1"
#define PORT    14465   // just a random value

// Prefixes to identify the type of command issued
#define EXEC_PREFIX         "exec "
#define WLAN_TOGGLE_PREFIX  "toggle "

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

/* to start wpa_supplicant */
void execute_cmd(char *buffer) {

    pid_t pid;

    /* Now execute the command issued */
    if((pid = fork()) < 0) {
        perror("fork() failed!");
        exit(EXIT_FAILURE);
    }

    if(pid == 0) {
        // It's the child process, execute the command and terminate
        system(buffer);
        exit(EXIT_SUCCESS);
    }
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
            //start wpa_supplicant
            execute_cmd(buffer);
        }
    }

    return 0;
}
