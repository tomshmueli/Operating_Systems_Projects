#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h> // For file size retrieval

int main(int argc, char *argv[]) {
    int socket_fd ,file_fd;
    uint16_t N_net, C_net, server_port;
    struct sockaddr_in serv_addr;
    char *buffer;
    struct stat st;
    size_t bytes_read;
    char *file_path, *server_ip_address;

    if (argc != 4) {
        perror("Wrong number of arguments!\n");
        exit(1);
    }
    server_ip_address = argv[1];
    server_port = (uint16_t)atoi(argv[2]);
    file_path = argv[3];
    
    file_fd = open(file_path, O_RDONLY);
    if (file_fd == -1) {
        perror("Failed to open the file.\n");
        exit(1);
    }

    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Could not create socket.\n");
        exit(1);
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip_address, &serv_addr.sin_addr) <= 0) {
        perror("IP address is not valid.\n");
        exit(1);
    }

    if (connect(socket_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connect Failed.\n");
        exit(1);
    }

    // Get file size (N)
    if (stat(file_path, &st) != 0) {
        perror("Failed to get file size.\n");
        exit(1);
    }
    N_net = htons(st.st_size); // Convert N to network byte order

    // Send the file size N to the server
    if (send(socket_fd, &N_net, sizeof(uint16_t), 0) < 0) {
        perror("Failed to send file size to the server.\n");
        exit(1);
    }

    buffer = malloc(st.st_size);
    if(buffer == NULL) {
        perror("Failed to allocate memory for the file content.\n");
        exit(1);
    }

    while((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
        send(socket_fd, buffer, bytes_read, 0);
    }
    if (bytes_read < 0) {
        perror("Failed reading from file");
        exit(1);
    }
    recv(socket_fd, &C_net, sizeof(C_net), 0);

    printf("# of printable characters: %hu\n", ntohs(C_net));

    close(socket_fd);

    return 0;
}
