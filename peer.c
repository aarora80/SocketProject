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

void DieWithError( const char *errorMessage ) // External error handling function
{
    perror( errorMessage );
    exit(1);
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
    //char message[MAX_MESSAGE_SIZE];

    // echoString = (char *) malloc( ECHOMAX );

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
        int n;
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

    printf("Received response from server: %s\n", echoBuffer);

    }
	// Pass string back and forth between server ITERATIONS times

	// printf( "client: Echoing strings for %d iterations\n", ITERATIONS );

    // for( int i = 0; i < ITERATIONS; i++ )
    // {
    //     printf( "\nEnter string to echo: \n" );
    //     if( ( nread = getline( &echoString, &echoStringLen, stdin ) ) != -1 )
    //     {
    //         echoString[ (int) strlen( echoString) - 1 ] = '\0'; // Overwrite newline
    //         printf( "\nclient: reads string ``%s''\n", echoString );
    //     }
    //     else
    //         DieWithError( "client: error reading string to echo\n" );

    //     // Send the string to the server
    //     if( sendto( sock, echoString, strlen( echoString ), 0, (struct sockaddr *) &echoServAddr, sizeof( echoServAddr ) ) != strlen(echoString) )
    //    		DieWithError( "client: sendto() sent a different number of bytes than expected" );

    //     // Receive a response
    //     fromSize = sizeof( fromAddr );

    //     if( ( respStringLen = recvfrom( sock, echoString, ECHOMAX, 0, (struct sockaddr *) &fromAddr, &fromSize ) ) > ECHOMAX )
    //         DieWithError( "client: recvfrom() failed" );

    //     echoString[ respStringLen ] = '\0';

    //     if( echoServAddr.sin_addr.s_addr != fromAddr.sin_addr.s_addr )
    //         DieWithError( "client: Error: received a packet from unknown source.\n" );

 	// 	printf( "client: received string ``%s'' from server on IP address %s\n", echoString, inet_ntoa( fromAddr.sin_addr ) );
    // }
    
    close( sock );
    return 0;
}