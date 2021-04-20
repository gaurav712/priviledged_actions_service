/*
 * This program is intended to be run as a background service(daemon)
 *
 * It listens to messages sent to "localhost:PORT" (PORT is defined below)
 * It is currently configured to toggle wlan state
 *
 * To toggle wlan, the message must be of the format: "MODE",
 *      where MODE is 0 -> when wlan is to be turned on
 *                    1 -> when wlan is to be turned off
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

#define MAX_CONNECTIONS         2
#define MAX_BUFFER_SIZE         3 // it is more than enough

// Address to listen on
#define ADDRESS "127.0.0.1"
#define PORT    14465   // just a random value

// Global, so the signal handler can close it
int server_sock, client_sock;

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

void signal_handler(int signum) {
    close(server_sock);
    close(client_sock);
    exit(EXIT_SUCCESS);
}

int main(void) {

    char buffer[MAX_BUFFER_SIZE];
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

    while (1) {
        if((listen(server_sock, MAX_CONNECTIONS)) != 0) {
            perror("Error While Listening!");
            exit(EXIT_FAILURE);
        }

        /* Accept the connection */
        client_sock = accept(server_sock,(struct sockaddr *) &client_address, &buffer_len);

        recv(client_sock, buffer, buffer_len, 0);

        /* Now process the message */
        if(buffer[0] == '1' || buffer[0] == '0')
            toggle_wlan(buffer[0] - '0');
    }

    return 0;
}
