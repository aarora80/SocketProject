// Implements the client side of an echo client-server application program.
// The client reads ITERATIONS strings from stdin, passes the string to the
// server, which simply echoes it back to the client.
//
// Compile on general.asu.edu as:
//   g++ -o client UDPEchoClient.c
//
// Only on general3 and general4 have the ports >= 1024 been opened for
// application programs.
#include <stdio.h>      // for printf() and fprintf()
#include <sys/socket.h> // for socket(), connect(), sendto(), and recvfrom()
#include <arpa/inet.h>  // for sockaddr_in and inet_addr()
#include <stdlib.h>     // for atoi() and exit()
#include <string.h>     // for memset()
#include <unistd.h>     // for close()
#include <string.h>

#define ECHOMAX 255     // Longest string to echo
#define ITERATIONS	5   // Number of iterations the client executes
#define ECHOMAX 255     // Longest string to echo
#define MAX_PEERS 10 // Maximum number of peers that can be registered
#define MAX_NAME_LENGTH 15 // Maximum length of peer names
#define MAX_MSG_LENGTH 255 // Maximum length of messages
#define MANAGER_PORT 5000 // Port number for manager
#define MAX_COMMAND_LENGTH 10
#define MAX_MESSAGE_SIZE 256


struct Peer {
    char command[MAX_COMMAND_LENGTH + 1]; // Command string
    char name[MAX_NAME_LENGTH + 1];
    char ip_address[16]; // IPv4 address format "xxx.xxx.xxx.xxx\0"
    unsigned int m_port; // Port for communication between peer and manager
    unsigned int p_port; // Port for communication between peers
    char state[10]; // State of the peer: Free, Leader, InDHT
};

struct PeerTuple
{
    int indentifier;
    char name[MAX_NAME_LENGTH+1];
    char ip_address[16];
    unsigned int p_port;
};

void DieWithError( const char *errorMessage ) // External error handling function
{
    perror( errorMessage );
    exit(1);
}

void send_id(const char *peer_name, const char *ip_address, unsigned int p_port, int identifier, int ring_size, const char *response) {
    int sock;                         // Socket descriptor
    struct sockaddr_in peer_addr;     // Peer address
    char message[MAX_MESSAGE_SIZE];   // Message to send

    // Create a UDP socket
    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up the peer address structure
    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(p_port);
    if (inet_pton(AF_INET, ip_address, &peer_addr.sin_addr) <= 0) {
        perror("invalid address");
        exit(EXIT_FAILURE);
    }

    // Create the set-id command message
    snprintf(message, sizeof(message), "set-id %s %s %d %d %s", peer_name, ip_address, p_port, identifier, response);

    // Send the message to the peer
    if (sendto(sock, message, strlen(message), 0, (const struct sockaddr *)&peer_addr, sizeof(peer_addr)) < 0) {
        perror("sendto failed");
        exit(EXIT_FAILURE);
    }

    // Close the socket
    close(sock);
}
int main( int argc, char *argv[] )
{
    size_t nread;
    int sock;                        // Socket descriptor
    struct sockaddr_in echoServAddr; // Echo server address
    struct sockaddr_in fromAddr;     // Source address of echo
    unsigned short echoServPort;     // Echo server port
    unsigned int fromSize;           // In-out of address size for recvfrom()
    char *servIP;                    // IP address of server
    // char *echoString = NULL;         // String to send to echo server
    size_t echoStringLen = ECHOMAX;               // Length of string to echo
    int respStringLen;               // Length of received response
    char echoBuffer[ECHOMAX];
    int n;                           //Size of ring 


    if (argc < 3)    // Test for correct number of arguments
    {
        fprintf( stderr, "Usage: %s <Server IP address> <Echo Port>\n", argv[0] );
        exit( 1 );
    }


    servIP = argv[ 1 ];  // First arg: server IP address (dotted decimal)
    echoServPort = atoi( argv[2] );  // Second arg: Use given port

    
    printf( "client: Arguments passed: server IP %s, port %d\n", servIP, echoServPort );
    
    //printf("CREATING the socket");
    // Create a datagram/UDP socket
    if( ( sock = socket( PF_INET, SOCK_DGRAM, IPPROTO_UDP ) ) < 0 )
        DieWithError( "client: socket() failed" );

    //printf("I have created the socket");
    // Construct the server address structure
    memset( &echoServAddr, 0, sizeof( echoServAddr ) ); // Zero out structure
    echoServAddr.sin_family = AF_INET;                  // Use internet addr family
    echoServAddr.sin_addr.s_addr = inet_addr( servIP ); // Set server's IP address
    echoServAddr.sin_port = htons( echoServPort );      // Set server's port

    //printf("I have made it before the while loop");
  
    while(1){
    //printf("I have made it into the while loop");

    printf("Enter a command: ");
        char command[10];
        if (scanf("%9s", command) != 1) {
            fprintf(stderr, "Error reading command.\n");
            break; // Exit the loop if there's an error
        }
    // Check if the command is "register"
    if (strcmp(command, "register") == 0) {
        //printf("I have made it into register");
        struct Peer peer;
        // If it's a registration command
        strcpy(peer.command, "register");
        printf("Enter peer name: ");
            scanf("%49s", peer.name);
            printf("Enter peer IP address: ");
            scanf("%15s", peer.ip_address);
            printf("Enter manager port: ");
            scanf("%d", &peer.m_port);
            printf("Enter peer port: ");
            scanf("%d", &peer.p_port);

            // Print the registration information
            printf("Registered:\n");
            printf("Peer: %s\n", peer.name);
            printf("IP Address: %s\n", peer.ip_address);
            printf("Manager Port: %d\n", peer.m_port);
            printf("Peer Port: %d\n", peer.p_port);

            char message[MAX_MESSAGE_SIZE];
            sprintf(message, "register %s %s %d %d", peer.name, peer.ip_address, peer.m_port, peer.p_port);

            // Send the peer information to the server
            if (sendto(sock, message, strlen(message), 0, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) < 0) {
                perror("sendto failed");
                exit(1);
        }

        
    }
    else if (strcmp(command, "setup-dht") == 0) {
        char leaderPeerName[MAX_NAME_LENGTH];
        int year;

        printf("Enter peer name: ");
        scanf("%49s", leaderPeerName);
        printf("Enter 'n' value: ");
        scanf("%d", &n);
        printf("Enter the year for data: ");
        scanf("%d", &year);
        
        char message[MAX_MESSAGE_SIZE];
        sprintf(message,"setup-dht %s %d %d", leaderPeerName, n, year);

        printf(message);

        if (sendto(sock, message, strlen(message), 0, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) < 0) {
                perror("sendto failed");
                exit(1);
        }
    }
    else if (strcmp(command, "exit") == 0) {
        // If the command is "exit"
        exit(0);
    }
     else {
        // If the command is invalid
        printf("Invalid command.\n");
    }

    // Send the commands to the server
    // if (sendto(sock, echoBuffer, echoStringLen, 0, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) != echoStringLen)
    //     DieWithError("client: sendto() sent a different number of bytes than expected");

    // Receive a response
    if (recvfrom(sock, echoBuffer, ECHOMAX, 0, NULL, NULL) < 0)
        DieWithError("client: recvfrom() failed");

   

    if (strcmp(command, "set-id") == 0) {
        // Parse the "set-id" command and its parameters
        char peer_name[MAX_NAME_LENGTH + 1];
        char ip_address[16];
        unsigned int p_port;
        int identifier, ring_size;
        char response[MAX_MESSAGE_SIZE];
        sscanf(echoBuffer, "%s %s %s %u %d %d %[^\n]", command, peer_name, ip_address, &p_port, &identifier, &ring_size, response);
        
        // Call the function to handle the "set-id" command
        //handle_set_id_command(peer_name, ip_address, p_port, identifier, ring_size, response);

        char neighbor_name[MAX_NAME_LENGTH + 1];
        char neighbor_ip[16];
        unsigned int neighbor_port;
        //sscanf(echoBuffer, "%s %s %u", neighbor_name, neighbor_ip, &neighbor_port);

        // Set the neighbor's information
        set_neighbor_info(neighbor_name, neighbor_ip, neighbor_port);

        // Print information for verification
        printf("Assigned identifier: %d\n", identifier);
        printf("Ring size: %d\n", ring_size);
        printf("Neighbor: %s, IP: %s, Port: %u\n", neighbor_name, neighbor_ip, neighbor_port);

    }

    printf("Received response from server: %s\n", echoBuffer);

    if(strncmp(echoBuffer,"SENDING", strlen("SENDING"))){
        char *token = strtok(echoBuffer, "\n"); //Split the response into lines
        int i = 1; //Skip the leaders tuple, start from the second line
        while ((token = strtok(NULL, "\n")) != NULL){
            char peer_name[MAX_NAME_LENGTH + 1];
            char ip_address[16];
            unsigned int p_port;

            //Parse the peer information from the token
            sscanf(token, "%s %s %u", peer_name, ip_address, &p_port);

            //Send set_id command to peer
            set_id(peer_name, ip_address, p_port, i, n, echoBuffer);

            i++;
        }
    }
    else if(strncmp(echoBuffer,"set-id",strlen("set-id")))
    {

    }

    }

    
    close( sock );
    return 0;
}