#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h> 

// Function declarations:
void SIGINT_handler(int signum);
void print_statistics();
uint16_t calculate_printable_count(char *buffer, size_t len);

// Global variables:
uint32_t pcc_total[95];
int connect_fd = -1;
int is_waiting = 1;

void SIGINT_handler(int signum) {
    if (connect_fd == -1) {
        print_statistics();
    }
    is_waiting = 0;
}

void print_statistics() {
    int i;
    for (i = 0; i < 95; i++) {
        printf("char '%c' : %u times\n", (char) (i + 32), pcc_total[i]);
    }
    exit(0);
}

uint16_t calculate_printable_count(char *buffer, size_t len) {
    uint16_t count = 0;
    for (size_t i = 0; i < len; i++) {
        if (buffer[i] >= 32 && buffer[i] <= 126) {
            count++;
            pcc_total[buffer[i] - 32]++;
        }
    }
    return count;
}


int main(int argc, char *argv[]) {
    int sent_sum, unsent;
    int listen_fd = -1;
    size_t bytes_read, total_read;
    const int on = 1;
    uint16_t N, C;
    char *buffer;
    C = 0;

    if(argc != 2) {
        perror("Wrong number of arguments!\n");
        exit(1);
    }

    
    uint16_t temp_pcc_total[95];
    struct sigaction sig_action = {
            .sa_handler = SIGINT_handler,
            .sa_flags = SA_RESTART};
    struct sockaddr_in server_addr;

    // Initialize pcc_total:
    memset(pcc_total, 0, sizeof(pcc_total));

    // Handling of SIGINT:
    if (sigaction(SIGINT, &sig_action, NULL) == -1) {
        perror("Signal handle registration failed");
        exit(1);
    }

    // create the TCP connection (socket + listen + bind):
    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Could not create socket. \n");
        exit(1);
    }
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int)) < 0) {
        perror("setsockopt failed. \n");
        exit(1);
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htons(INADDR_ANY);
    server_addr.sin_port = htons((uint16_t)atoi(argv[1]));

    if (bind(listen_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
        perror("Bind Failed. \n");
        exit(1);
    }
    if (listen(listen_fd, 10) != 0) {
        perror("Listen Failed. \n");
        exit(1);
    }

    while (is_waiting) {
        // Accept TCP connection:
        connect_fd = accept(listen_fd, NULL, NULL);
        if (connect_fd < 0) {
            perror("Accept Failed. \n");
            exit(1);
        }

        // Receive N 
        sent_sum = 0;
        unsent = 2; // 2 bytes
        while (unsent > 0) {
            bytes_read = read(connect_fd, (char *) &N + sent_sum, unsent);
            if (bytes_read == 0 || (bytes_read < 0 && (errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE))) {
                perror("Failed to receive N from the client (EOF, ETIMEDOUT, ECONNRESET, EPIPE). \n");
                close(connect_fd);
                unsent = 0;
                connect_fd = -1;
            } else if (bytes_read < 0) {
                perror("Failed to receive N from the client (Not handled error condition). \n");
                close(connect_fd);
                exit(1);
            } else {
                sent_sum += bytes_read;
                unsent -= bytes_read;
            }
        }
        if (connect_fd == -1) {
            continue;
        }

        N = ntohs(N);
        buffer = malloc(N);
        if(buffer == NULL) {
            perror("Failed to allocate memory for the file content. \n");
            close(connect_fd);
            exit(1);
        }

        // Receive file content and calculate statistics:
        memset(temp_pcc_total, 0, sizeof(temp_pcc_total));
        sent_sum = 0;
        unsent = N;
        total_read = 0;

        // Read the file content from the client:
        while (total_read < N) {
            bytes_read = read(connect_fd, buffer + total_read, N - total_read);
            if (bytes_read <= 0) { // Handle error or end of data
                perror("Failed to read file content from client");
                break;
            }
            total_read += bytes_read;
        }

        // After successfully reading all data
        if (total_read == N) { // Ensure we've read all data
            C = calculate_printable_count(buffer, N); // Calculate and update global counts
            C = htons(C); // Convert count to network byte order
            if (write(connect_fd, &C, sizeof(C)) != sizeof(C)) {
                perror("Failed to send count of printable characters to client");
            }
        }
        free(buffer);
        close(connect_fd);
        connect_fd = -1;
        C = 0;
    }
    print_statistics();
}
