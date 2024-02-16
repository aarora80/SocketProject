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
#include <dirent.h>
#include <sys/stat.h>

#define ECHOMAX 255     // Longest string to echo
#define ITERATIONS	5   // Number of iterations the client executes
#define ECHOMAX 255     // Longest string to echo
#define MAX_PEERS 10 // Maximum number of peers that can be registered
#define MAX_NAME_LENGTH 15 // Maximum length of peer names
#define MAX_MSG_LENGTH 255 // Maximum length of messages
#define MANAGER_PORT 5000 // Port number for manager
#define MAX_COMMAND_LENGTH 10
#define MAX_MESSAGE_SIZE 256
#define MAX_RING_SIZE 10

// Structure to represent a storm event
typedef struct {
    long event_id;
    char state[100];
    int year;
    char month[20];
    char event_type[100];
    char cz_type;
    char cz_name[100];
    int injuries_direct;
    int injuries_indirect;
    int deaths_direct;
    int deaths_indirect;
    char damage_property[20];
    int damage_crops;
    //char tor_f_scale[5];
} StormEvent;

typedef struct HashNode{
    StormEvent data;
    struct HashNode *n;
}HashNode;

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

struct Node {
    struct PeerTuple data;
    struct Node* next;
};

int is_prime(int n) {
    if (n <= 1) return 0;
    if (n <= 3) return 1;
    if (n % 2 == 0 || n % 3 == 0) return 0;
    for (int i = 5; i * i <= n; i += 6)
        if (n % i == 0 || n % (i + 2) == 0)
            return 0;
    return 1;
}

int find_prime(int l) {
    int n = 2 * l;
    while (is_prime(n) == 0) {
        n++;
    }
    return n;
}

struct Node* createNode(struct PeerTuple newTuple) {
    struct Node* newNode = (struct Node*)malloc(sizeof(struct Node));
    if (newNode == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }
    newNode->data = newTuple;
    newNode->next = NULL;
    return newNode;
}

void freeList(struct Node* head) {
    struct Node* current = head;
    while (current != NULL) {
        struct Node* temp = current;
        current = current->next;
        free(temp);
    }
}

void insertEnd(struct Node** headRef, struct PeerTuple newTuple) {
    struct Node* newNode = createNode(newTuple);
    if (*headRef == NULL) {
        *headRef = newNode;
        return;
    }
    struct Node* current = *headRef;
    while (current->next != NULL) {
        current = current->next;
    }
    current->next = newNode;
}

void printList(struct Node* head) {
    struct Node* current = head;
    do {
        printf("%s ", current->data.name);
        current = current->next;
    } while (current != head); // Loop until we reach the head node again
    printf("\n");
}

// Function to make the last element of the linked list point to the head
void makeLastPointToHead(struct Node* head) {
    struct Node* current = head;
    while (current->next != NULL) {
        current = current->next;
    }
    current->next = head;
}

void send_id(const char *peer_name, const char *ip_address, unsigned int peer_port, int indentifier, int ring_size, const char *response)
{
    printf("I have entered send_id!\n");

    int send_sock;
    struct sockaddr_in echoPeerAddr;
    struct sockaddr_in echoLeaderAddr;
    char message[MAX_MESSAGE_SIZE];

    if((send_sock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&echoPeerAddr, 0, sizeof(echoPeerAddr));
    echoPeerAddr.sin_family = AF_INET;
    echoPeerAddr.sin_addr.s_addr = inet_addr(ip_address);
    echoPeerAddr.sin_port = htons(peer_port);
    if(inet_pton(AF_INET, ip_address, &echoPeerAddr.sin_addr) <=0)
    {
        perror("invalid address");
        exit(EXIT_FAILURE);
    }

    snprintf(message, sizeof(message), "set_id %s %s %u %d", peer_name, ip_address, 
    peer_port, indentifier);

    if(sendto(send_sock, message, strlen(message), 0, (const struct sockaddr*)&echoPeerAddr, sizeof(echoPeerAddr)) < 0)
    {
        perror("send failed");
        exit(EXIT_FAILURE);
    }

    printf("Message sent!\n\n");

    close(send_sock);
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
    int year;
    struct Peer peer;
    struct PeerTuple ring[MAX_RING_SIZE];
    char filename[256];
    char year_2_str[50]; 
    HashNode *hash_table[5000] = {NULL};


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

            printf("Enter peer name: ");
            scanf("%49s", leaderPeerName);
            printf("Enter 'n' value: ");
            scanf("%d", &n);
            printf("Enter the year for data: ");
            scanf("%d", &year);
            
            char message[MAX_MESSAGE_SIZE];
            sprintf(message,"setup-dht %s %d %d", leaderPeerName, n, year);

            //printf(message);

            if (sendto(sock, message, strlen(message), 0, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) < 0) {
                    perror("sendto failed");
                    exit(1);
            }
        }
        else if(strcmp(command, "set-id") == 0)
        {
            int sockfd_server, sockfd_client;
            struct sockaddr_in my_addr, client_addr;
            char message[MAX_MSG_LENGTH];

            // Create UDP socket for server
            if ((sockfd_server = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
                perror("socket");
                exit(1);
            }

            // Set up own address structure
            memset(&my_addr, 0, sizeof(my_addr));
            my_addr.sin_family = AF_INET;
            my_addr.sin_port = htons(peer.p_port);
            my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

            // Bind socket to the port
            if (bind(sockfd_server, (struct sockaddr *)&my_addr, sizeof(my_addr)) == -1) {
                perror("bind");
                exit(1);
            }

            // Receive message from Peer 
            printf("Waiting for message from Leader...\n");
            socklen_t client_addr_len = sizeof(client_addr);
            ssize_t recv_len = recvfrom(sockfd_server, message, MAX_MSG_LENGTH, 0, (struct sockaddr *)&client_addr, &client_addr_len);
            if (recv_len == -1) {
                perror("recvfrom");
                exit(1);
            }

        message[recv_len] = '\0';  // Null-terminate the received message
        printf("Received message from Leader: %s\n", message);

        // Close server socket
        close(sockfd_server);

        continue;
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
        printf("waitng for response\n\n");
        if (recvfrom(sock, echoBuffer, ECHOMAX, 0, NULL, NULL) < 0)
            DieWithError("client: recvfrom() failed");

        printf("Received response from server: %s\n", echoBuffer);

        char *token = strtok(echoBuffer, "\n"); //Split the response into lines

        // Parse the line to extract peer name, IP address, and port
        char State[50];
        struct Node* head = NULL;
        sscanf(token, "%s", State);

        if(strcmp(State,"Leader") == 0)
        {
            struct PeerTuple Leader;
            char name[MAX_NAME_LENGTH];
            char ip_address[16];
            unsigned int port;
            
            token = strtok(NULL, "\n");
            sscanf(token,"%s %s %u", name, ip_address, &port);
            
            printf("Leader Tuple: %s, %s, %u\n\n", name,ip_address,port);
            Leader.indentifier =0;
            strcpy(Leader.ip_address,ip_address);
            strcpy(Leader.name,name);
            Leader.p_port = port;

            //Insert into Linked List
            insertEnd(&head, Leader);
        }
        else
        {
            continue;
        }

        int i = 1; //Skip the leaders tuple, start from the second line
        while ((token = strtok(NULL, "\n")) != NULL){
            char peer_name[MAX_NAME_LENGTH + 1];
            char ip_address[16];
            unsigned int p_port;
            struct PeerTuple newTuple;

            //Parse the peer information from the token
            sscanf(token, "%s %s %u", peer_name, ip_address, &p_port);
            
            //Send set_id command to peer
            printf("Name: %s\nAddress: %s\nPort: %u\n",peer_name,ip_address,p_port);
            
            newTuple.indentifier = i;
            strcpy(newTuple.ip_address,ip_address);
            strcpy(newTuple.name,peer_name);
            newTuple.p_port = p_port;

            insertEnd(&head,newTuple);
            send_id(peer_name, ip_address, p_port, i, n, token);

            i++;
        }


        makeLastPointToHead(head);

        printList(head);

        DIR *dir;
        struct dirent *entry;

        //Open the directory 
        if((dir = opendir("."))==NULL){
            perror("opendir");
            exit(EXIT_FAILURE);
        }

        //Iterate through each entry in the directory 
        while((entry = readdir(dir)) != NULL){
            //Skip "." and ".." entries 
            if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
                continue;
            }


            //Construct the full path of the file
            char filepath[256];
            snprintf(filepath, sizeof(filepath), "%s", entry->d_name);

            //check if the entry is a reguar file
            struct stat statbuf;
            if(stat(filepath, &statbuf) == -1){
                perror("stat");
                continue;
            }

            if(!S_ISREG(statbuf.st_mode)){
                continue;
            }

            sprintf(year_2_str,"%d", year);
            sprintf(filename, "details-%s.csv", year_2_str);

            
            //Process the file (here we have do our performed)
            if(strcmp(filename,entry->d_name) == 0){
                printf("Processing file: %s\n", entry->d_name);
                break;
            }     

        }


        //----------------------- Storm Event Computing ------------------------
        FILE *file = fopen(filename,"r");
        if (file == NULL)
        {
            fprintf(stderr, "Error opening file %s\n", filename);
            exit(EXIT_FAILURE);
        }

        char c;
        int count = 0;
        // Skip the first line
        while ((c = getc(file)) != '\n' && c != EOF);

    // Extract characters from file and store in character c
        for (c = getc(file); c != EOF; c = getc(file))
            if (c == '\n') // Increment count if this character is newline
                count = count + 1;


        count++; //account for the last \n in the file


        



        int s = find_prime(count);
        printf("the file has %d lines\n", count);
        printf("This is s: %d\n", s);

        fseek(file,0,SEEK_SET);
        char line[1024];

        // Skip the first line
        fgets(line, 1024, file);
        //printf("I made it past the first line\n");
        // Read and parse each line
        while (fgets(line, sizeof(line), file) != NULL) {
            //printf("I made it into the while loop\n");
            StormEvent event;
            //Without the Fcategory at the end 
            if (sscanf(line, "%lu,%99[^,],%d,%19[^,],%99[^,],%c,%99[^,],%d,%d,%d,%d,%19[^,],%d",
                    &event.event_id, event.state, &event.year, event.month,
                    event.event_type, &event.cz_type, event.cz_name,
                    &event.injuries_direct, &event.injuries_indirect,
                    &event.deaths_direct, &event.deaths_indirect,
                    event.damage_property, &event.damage_crops
                    /*event.tor_f_scale*/) != 13/*14*/) 
            {
                fprintf(stderr, "Failed to parse line: %s\n", line);
                continue;
            }

            printf("Event ID: %lu\n", event.event_id);
        }
        //printf("I made it past the while loop\n");

        fclose(file); 
        closedir(dir);

    }

    close(sock);
    return 0;
}