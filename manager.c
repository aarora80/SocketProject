// Implements the server side of an echo client-server application program.
// The client reads ITERATIONS strings from stdin, passes the string to the
// this server, which simply sends the string back to the client.
//
// Compile on general.asu.edu as:
//   g++ -o server UDPEchoServer.c
//
// Only on general3 and general4 have the ports >= 1024 been opened for
// application programs.
#include <stdio.h>      // for printf() and fprintf()
#include <sys/socket.h> // for socket() and bind()
#include <arpa/inet.h>  // for sockaddr_in and inet_ntoa()
#include <stdlib.h>     // for atoi() and exit()
#include <string.h>     // for memset()
#include <unistd.h>     // for close()
#include <time.h>       // for rand()

#define ECHOMAX 255     // Longest string to echo
#define MAX_PEERS 10 // Maximum number of peers that can be registered
#define MAX_NAME_LENGTH 15 // Maximum length of peer names
#define MAX_MSG_LENGTH 255 // Maximum length of messages
#define MANAGER_PORT 5000 // Port number for manager

struct Peer {
    char name[MAX_NAME_LENGTH + 1];
    char ip_address[16]; // IPv4 address format "xxx.xxx.xxx.xxx\0"
    unsigned int m_port; // Port for communication between peer and manager
    unsigned int p_port; // Port for communication between peers
    char state[10]; // State of the peer: Free, Leader, InDHT
};

struct PeerTuple
{
    char name[MAX_NAME_LENGTH+1];
    char ip_address[16];
    unsigned int p_port;
};

struct Peer registered_peers[MAX_PEERS]; // Array to store registered peers
int num_registered_peers = 0; // Counter for registered peers

void DieWithError( const char *errorMessage ) // External error handling function
{
    perror( errorMessage );
    exit( 1 );
}

int peerExists(char peerName[MAX_NAME_LENGTH])
{
    int peer_index = -1;
    for(int i=0; i < num_registered_peers; i++)
    {
        if(strcmp(registered_peers[i].name, peerName) == 0)
        {
            peer_index = i;
            break;
        }
    }


    if(peer_index == -1)
    {
        return -1;
    }
    else
    {
        // printf("I entered the return peer index condition\n");
        // printf("%d\n",peer_index);
        return peer_index;
    }
}

void setRandPeerToDHT(int leader_index, int n)
{
    //printf("Leader index: %d\n",leader_index);
    srand(time(NULL));
    
    int selected_count =0;

    while(selected_count < (n-1))
    {
        //printf("Entered for loop in rand function\n");
        int random_index = rand() % num_registered_peers;
        //printf("random index: %d\n",random_index);
        if(random_index != leader_index && strcmp(registered_peers[random_index].state, "Free") == 0) {
            strcpy(registered_peers[random_index].state, "InDHT");
            selected_count++;
        }
    }
}
    

//register Peer
int register_peer(char *peer_name, char *ip_address, unsigned int m_port, unsigned int p_port, struct sockaddr_in echoClntAddr, int sock) {
    // The function now directly accepts peer information instead of the message
    // No need to parse the message, as the information is provided directly

    // Debugging: Print parsed values
    printf("Parsed peer name: %s\n", peer_name);
    printf("Parsed IP address: %s\n", ip_address);
    printf("Parsed manager port: %u\n", m_port);
    printf("Parsed peer port: %u\n", p_port);

    // Check if peer name is already registered
    for (int i = 0; i < num_registered_peers; i++) {
        if (strcmp(registered_peers[i].name, peer_name) == 0) {
            // Send failure response if peer name is already registered
            sendto(sock, "FAILURE: Peer name already registered", strlen("FAILURE: Peer name already registered"), 0,
                   (struct sockaddr *) &echoClntAddr, sizeof(echoClntAddr));
            return 0;
        }
    }

    // Check if ports are already in use by other peers
    for (int i = 0; i < num_registered_peers; i++) {
        if (strcmp(registered_peers[i].ip_address, ip_address) == 0 && 
            (registered_peers[i].m_port == m_port || registered_peers[i].p_port == p_port)) {
            // Send failure response if ports are not unique
            sendto(sock, "FAILURE: Duplicate IP address and ports combination", 
                   strlen("FAILURE: Duplicate IP address and ports combination"), 0,
                   (struct sockaddr *) &echoClntAddr, sizeof(echoClntAddr));
            return 0;
        }
    }

    // Register the new peer
    strcpy(registered_peers[num_registered_peers].name, peer_name);
    strcpy(registered_peers[num_registered_peers].ip_address, ip_address);
    registered_peers[num_registered_peers].m_port = m_port;
    registered_peers[num_registered_peers].p_port = p_port;
    strcpy(registered_peers[num_registered_peers].state, "Free");

    // Increment the counter for registered peers
    num_registered_peers++;

    // Send success response to the peer
    sendto(sock, "SUCCESS!", strlen("SUCCESS"), 0, (struct sockaddr *) &echoClntAddr, sizeof(echoClntAddr));
    return 1;
}

void print_registered_peers() {
    printf("Registered Peers:\n");
    for (int i = 0; i < num_registered_peers; i++) {
        printf("Peer %d:\n", i);
        printf("Name: %s\n", registered_peers[i].name);
        printf("IP Address: %s\n", registered_peers[i].ip_address);
        printf("Manager Port: %d\n", registered_peers[i].m_port);
        printf("Peer Port: %d\n", registered_peers[i].p_port);
        printf("State: %s\n", registered_peers[i].state);
        printf("\n");
    }
}

int main( int argc, char *argv[] )
{
    int sock;                        // Socket
    struct sockaddr_in echoServAddr; // Local address of server
    struct sockaddr_in echoClntAddr; // Client address
    unsigned int cliAddrLen;         // Length of incoming message
    char echoBuffer[ ECHOMAX ];      // Buffer for echo string
    unsigned short echoServPort;     // Server port
    int recvMsgSize;                 // Size of received message
    int isDHTComplete;               // Check if table is complete 

    if( argc != 2 )         // Test for correct number of parameters
    {
        fprintf( stderr, "Usage:  %s <UDP SERVER PORT>\n", argv[ 0 ] );
        exit( 1 );
    }

    echoServPort = atoi(argv[1]);  // First arg: local port

    // Create socket for sending/receiving datagrams
    if( ( sock = socket( PF_INET, SOCK_DGRAM, IPPROTO_UDP ) ) < 0 )
        DieWithError( "server: socket() failed" );

    // Construct local address structure */
    memset( &echoServAddr, 0, sizeof( echoServAddr ) ); // Zero out structure
    echoServAddr.sin_family = AF_INET;                  // Internet address family
    echoServAddr.sin_addr.s_addr = htonl( INADDR_ANY ); // Any incoming interface
    echoServAddr.sin_port = htons( echoServPort );      // Local port

    // Bind to the local address
    if( bind( sock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0 )
        DieWithError( "server: bind() failed" );

	printf( "server: Port server is listening to is: %d\n", echoServPort );

    for (;;) { // Run forever
    cliAddrLen = sizeof(echoClntAddr);

    // Block until receive message from a client
    if ((recvMsgSize = recvfrom(sock, echoBuffer, ECHOMAX, 0, (struct sockaddr *)&echoClntAddr, &cliAddrLen)) < 0)
        DieWithError("server: recvfrom() failed");

    echoBuffer[recvMsgSize] = '\0';

    //printf("server: received string ``%s'' from client on IP address %s\n", echoBuffer, inet_ntoa(echoClntAddr.sin_addr));

    // Extract and print peer information
    if(strncmp(echoBuffer,"register",strlen("register")) == 0){
    
        char command[10];
        char peer_name[MAX_NAME_LENGTH + 1];
        char ip_address[16];
        unsigned int m_port, p_port;
        sscanf(echoBuffer, "%9s %s %s %u %u", command, peer_name, ip_address, &m_port, &p_port);
    
        printf("Peer information received:\n");
        printf("Command: %s\n", command);
        printf("Name: %s\n", peer_name);
        printf("IP Address: %s\n", ip_address);
        printf("Manager Port: %u\n", m_port);
        printf("Peer Port: %u\n", p_port);
        if (register_peer(peer_name, ip_address, m_port, p_port, echoClntAddr, sock)) {
            printf("Peer registered successfully.\n");
            print_registered_peers();
        } else {
            printf("Failed to register peer.\n");
        }
    } else if(strncmp(echoBuffer,"setup-dht",strlen("setup-dht")) == 0) 
    {
        char command[10];
        char peer_name[MAX_NAME_LENGTH+1];
        int n;
        int year;

        sscanf(echoBuffer, "%9s %49s %d %d", command, peer_name, &n, &year);

        printf("Recieved %s %d %d \n", peer_name, n, year);

        int leaderIndex = peerExists(peer_name);

        if(leaderIndex > -1)
        {
            // printf("I entered the peerexists condition\n");
            // printf("%d\n",(peerExists(peer_name)));
            printf("%s is leader\n",registered_peers[leaderIndex].name);
        }
        else
        {
            DieWithError("Peer not registered");
        }

        if(n > num_registered_peers || isDHTComplete == 1 || n < 3)
        {
            DieWithError("Failure");
        }

        strcpy(registered_peers[peerExists(peer_name)].state, "Leader");

        setRandPeerToDHT(leaderIndex, n);
        
        print_registered_peers();

        char response[MAX_MSG_LENGTH];
        sprintf(response, "SUCCESS\n%s %s %u\n", registered_peers[leaderIndex].name,
        registered_peers[leaderIndex].ip_address,registered_peers[leaderIndex].p_port);

        for(int i =0; i < n;i++)
        {
            if(i == leaderIndex)
            {
                continue;
            }

            sprintf(response + strlen(response), "%s %s %u\n", registered_peers[i].name,
            registered_peers[i].ip_address,registered_peers[i].p_port);
        }

        if(sendto(sock,response,strlen(response),0, 
        (struct sockaddr*)&echoClntAddr, sizeof(echoClntAddr)) != strlen(response))
        {
            DieWithError("Response did not work");
        }
    }
    else {
        // Handle other types of messages here
        // For now, just echo back the message to the client
        close(sock);
        if (sendto(sock, echoBuffer, recvMsgSize, 0, (struct sockaddr *)&echoClntAddr,
                   sizeof(echoClntAddr)) != recvMsgSize)
            DieWithError("server: sendto() sent a different number of bytes than expected");
    }
}

    // NOT REACHED */
}