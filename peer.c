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

//Local Hash table of Peer
HashNode *hash_table[5000] = {NULL};

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
    int record_size;
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

//-------------------Linked List Function For Nodes in Ring------------------------

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

//-----------------------------------------------------Function for sending messages from Leader to Peer---------------------------------------------

void send_id(const char *peer_name, const char *ip_address, unsigned int peer_port, char *state, int indentifier, int ring_size, const char *response)
{
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

    snprintf(message, sizeof(message), "set_id %s %s %u %s %d", peer_name, ip_address, 
    peer_port, state, indentifier);

    if(sendto(send_sock, message, strlen(message), 0, (const struct sockaddr*)&echoPeerAddr, sizeof(echoPeerAddr)) < 0)
    {
        perror("send failed");
        exit(EXIT_FAILURE);
    }

    printf("Message sent!\n\n");

    close(send_sock);
}

void send_tuple(const char *peer_name, const char *ip_address, unsigned int peer_port, int indentifier, int ring_size,  StormEvent event)
{
    if(indentifier == 0)
    {
        HashNode *new_node = (HashNode*)malloc(sizeof(HashNode));
        if (new_node == NULL) {
            perror("Memory allocation failed");
            exit(EXIT_FAILURE);
        }

        // Fill in the StormEvent data
        new_node->data.event_id = event.event_id;
        strcpy(new_node->data.state, event.state);
        new_node->data.year = event.year;
        strcpy(new_node->data.month, event.month);
        strcpy(new_node->data.event_type, event.event_type);
        new_node->data.cz_type = event.cz_type;
        strcpy(new_node->data.cz_name, event.cz_name);
        new_node->data.injuries_direct = event.injuries_direct;
        new_node->data.injuries_indirect = event.injuries_indirect;
        new_node->data.deaths_direct = event.deaths_direct;
        new_node->data.deaths_indirect = event.deaths_indirect;
        strcpy(new_node->data.damage_property, event.damage_property);
        new_node->data.damage_crops = event.damage_crops;
        // Calculate hash index
        int hash_index = event.event_id % 5000;

        // Insert the new node into the hash table
        new_node->n = hash_table[hash_index];
        hash_table[hash_index] = new_node;
    }

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

    // Serialize the StormEvent struct into a string representation
    snprintf(message, sizeof(message), "store %lu,%s,%d,%s,%s,%c,%s,%d,%d,%d,%d,%s,%d", 
            event.event_id, event.state, event.year, event.month,
            event.event_type, event.cz_type, event.cz_name,
            event.injuries_direct, event.injuries_indirect,
            event.deaths_direct, event.deaths_indirect,
            event.damage_property, event.damage_crops);

    if(sendto(send_sock, message, strlen(message), 0, (const struct sockaddr*)&echoPeerAddr, sizeof(echoPeerAddr)) < 0)
    {
        perror("send failed");
        exit(EXIT_FAILURE);
    }

    printf("Record Stored!\n\n");

    close(send_sock);
}

void send_stop(const char *peer_name, const char *ip_address, unsigned int peer_port)
{
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

    // Serialize the StormEvent struct into a string representation
    snprintf(message, sizeof(message), "stop listening");
    if(sendto(send_sock, message, strlen(message), 0, (const struct sockaddr*)&echoPeerAddr, sizeof(echoPeerAddr)) < 0)
    {
        perror("send failed");
        exit(EXIT_FAILURE);
    }

    close(send_sock);
}

//Hash table functions

void printHashTable(HashNode *hash_table[], int table_size) {
    for (int i = 0; i < table_size; i++) {
        HashNode *current = hash_table[i];
        if(current == NULL)
        {
            continue;
        }
        else
        {
            printf("Bucket %d: ", i);
        }
        while (current != NULL) {
            printf("(%lu, %s, %d, %s, %s, %c, %s, %d, %d, %d, %d, %s, %d) -> ", 
                   current->data.event_id, current->data.state, current->data.year, 
                   current->data.month, current->data.event_type, current->data.cz_type, 
                   current->data.cz_name, current->data.injuries_direct, 
                   current->data.injuries_indirect, current->data.deaths_direct, 
                   current->data.deaths_indirect, current->data.damage_property, 
                   current->data.damage_crops);
            current = current->n;
        }
        printf("NULL\n");
    }
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
    size_t echoStringLen = ECHOMAX;               // Length of string to echo
    int respStringLen;               // Length of received response
    char echoBuffer[ECHOMAX];
    int n;                           //Size of ring 
    int year;                        //Year to parse data
    struct Peer peer;                //Store peer info
    char filename[256];              //File name to parse
    char year_2_str[50];             //Convert year to string
    int isLeader=0;                  //Check if peer is leader


    if (argc < 3)    // Test for correct number of arguments
    {
        fprintf( stderr, "Usage: %s <Server IP address> <Echo Port>\n", argv[0] );
        exit( 1 );
    }


    servIP = argv[ 1 ];  // First arg: server IP address (dotted decimal)
    echoServPort = atoi( argv[2] );  // Second arg: Use given port

    
    printf( "client: Arguments passed: server IP %s, port %d\n", servIP, echoServPort );
    
    // Create a datagram/UDP socket
    if( ( sock = socket( PF_INET, SOCK_DGRAM, IPPROTO_UDP ) ) < 0 )
        DieWithError( "client: socket() failed" );

    // Construct the server address structure
    memset( &echoServAddr, 0, sizeof( echoServAddr ) ); // Zero out structure
    echoServAddr.sin_family = AF_INET;                  // Use internet addr family
    echoServAddr.sin_addr.s_addr = inet_addr( servIP ); // Set server's IP address
    echoServAddr.sin_port = htons( echoServPort );      // Set server's port
  
    while(1){

        printf("Enter a command: ");
            char command[20];
            if (scanf("%19s", command) != 1) {
                fprintf(stderr, "Error reading command.\n");
                break; // Exit the loop if there's an error
            }
        // Check if the command is "register"
        if (strcmp(command, "register") == 0) {
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
                strcpy(peer.state, "Free");

                char message[MAX_MESSAGE_SIZE];
                sprintf(message, "register %s %s %d %d", peer.name, peer.ip_address, peer.m_port, peer.p_port);

                // Send the peer information to the server
                if (sendto(sock, message, strlen(message), 0, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) < 0) {
                    perror("sendto failed");
                    exit(1);
            }

            
        }
        else if (strcmp(command, "setup-dht") == 0) {
            //If command is setup-dht
            
            char leaderPeerName[MAX_NAME_LENGTH];
            isLeader =1;
            
            printf("Enter peer name: ");
            scanf("%49s", leaderPeerName);
            printf("Enter 'n' value: ");
            scanf("%d", &n);
            printf("Enter the year for data: ");
            scanf("%d", &year);
            
            char message[MAX_MESSAGE_SIZE];
            sprintf(message,"setup-dht %s %d %d", leaderPeerName, n, year);
            if (sendto(sock, message, strlen(message), 0, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) < 0) {
                    perror("sendto failed");
                    exit(1);
            }
        }
        else if(strcmp(command, "set-id") == 0)
        {
            /*
            Command to set id of peers
            None leader peer enters this command to wait for set-id message from Leader
            */
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
            //Free state peers will stop listening 
            if(strcmp(message, "stop listening") == 0)
            {
                continue;
            }

            do {
                // Receive message from Peer 
                printf("Waiting for record from Leader...\n");
                socklen_t client_addr_len = sizeof(client_addr);
                ssize_t recv_len = recvfrom(sockfd_server, message, MAX_MSG_LENGTH, 0, (struct sockaddr *)&client_addr, &client_addr_len);
                if (recv_len == -1) {
                    perror("recvfrom");
                    exit(1);
                }

                message[recv_len] = '\0';  // Null-terminate the received message
                printf("Received record from Leader: %s\n", message);


                //DESRALIZE AND STORE IN LOCAL HASHTABLE OF NONLEADER PEER
                 // Extract StormEvent fields from the serialized message
                long event_id;
                char state[100], month[20], event_type[100], cz_name[100], damage_property[20];
                int year, injuries_direct, injuries_indirect, deaths_direct, deaths_indirect, damage_crops;
                char cz_type;

                // Parse the message
                if (sscanf(message, "store %lu,%99[^,],%d,%19[^,],%99[^,],%c,%99[^,],%d,%d,%d,%d,%19[^,],%d",
                    &event_id, state, &year, month, event_type, &cz_type, cz_name,
                    &injuries_direct, &injuries_indirect, &deaths_direct, &deaths_indirect,
                    damage_property, &damage_crops) == 13) {
                    printf("Parsed Correctly: %s\n", message);
                }
                else if(strcmp(message, "stop listening") == 0){
                    printf("Stopped Listening: %s\n", message);
                }
                else{
                     fprintf(stderr, "Failed to parse message: %s\n", message);
                }

                // Create a new HashNode
                HashNode *new_node = (HashNode*)malloc(sizeof(HashNode));
                if (new_node == NULL) {
                    perror("Memory allocation failed");
                    exit(EXIT_FAILURE);
                }

                // Fill in the StormEvent data
                new_node->data.event_id = event_id;
                strcpy(new_node->data.state, state);
                new_node->data.year = year;
                strcpy(new_node->data.month, month);
                strcpy(new_node->data.event_type, event_type);
                new_node->data.cz_type = cz_type;
                strcpy(new_node->data.cz_name, cz_name);
                new_node->data.injuries_direct = injuries_direct;
                new_node->data.injuries_indirect = injuries_indirect;
                new_node->data.deaths_direct = deaths_direct;
                new_node->data.deaths_indirect = deaths_indirect;
                strcpy(new_node->data.damage_property, damage_property);
                new_node->data.damage_crops = damage_crops;

                // Calculate hash index
                int hash_index = event_id % 5000;

                // Insert the new node into the hash table
                new_node->n = hash_table[hash_index];
                hash_table[hash_index] = new_node;


                

            }while(strncmp(message,"store",strlen("store")) == 0);

            // Close server socket
            close(sockfd_server);
            continue;
        }
        else if(strcmp(command, "dht-complete") == 0)
        {
            //Command to signal dht is complete
            char leaderPeerName1[MAX_NAME_LENGTH];

            printf("Enter peer name: ");
            scanf("%49s", leaderPeerName1);

            
            //Check if peer is leader before sending dht complete
            if(isLeader == 0)
            {
                DieWithError("Peer can't send this command");
            }

            char message[MAX_MESSAGE_SIZE];
            sprintf(message,"dht-complete %s", leaderPeerName1);

            if (sendto(sock, message, strlen(message), 0, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) < 0) {
                    perror("sendto failed");
                    exit(1);
            }
            
        }
        else if (strcmp(command, "query-dht") == 0)
        {
            char message[MAX_MSG_LENGTH];
            char PeerName[MAX_NAME_LENGTH];
            long event_id;

            printf("Enter peer name: ");
            scanf("%49s", PeerName);

            printf("Enter Event ID: ");
            scanf("%lu", &event_id);

            //Check for if peer is able to execute the command
            if(strcmp(peer.state, "Free") != 0)
            {
                DieWithError("Not a free peer");
            }

            sprintf(message,"find-event %lu", event_id);           
            
            if (sendto(sock, message, strlen(message), 0, (struct sockaddr *)&echoServAddr, 
            sizeof(echoServAddr)) < 0) 
            {
                    perror("sendto failed");
                    exit(1);
            }            
        }
        else if(strcmp(command, "find-event") == 0)
        {
            /*
            Find event is running the hot potato protocol
            */
            int sockfd_server, sockfd_client;
            struct sockaddr_in my_addr, client_addr;
            char message[MAX_MSG_LENGTH];

            // Create UDP socket for peer
            if ((sockfd_server = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
                perror("socket");
                exit(1);
            }
            
            // Receive message from Peer 
            printf("Finding Event...\n");

            close(sockfd_server);

        }
        else if (strcmp(command, "exit") == 0) {
            // If the command is "exit"
            exit(0);
        }
        else {
            // If the command is invalid
            printf("Invalid command.\n");
        }

        // Receive a response from manager 
        // Leader is the only one to enter this section
        // of code. Non Leader are never to enter this
        // section

        printf("waitng for response!!!\n\n");
        if (recvfrom(sock, echoBuffer, ECHOMAX, 0, NULL, NULL) < 0)
            DieWithError("client: recvfrom() failed");

        printf("Received response from server: %s\n", echoBuffer);

        char *token = strtok(echoBuffer, "\n"); //Split the response into lines

        // Parse the line to extract peer name, IP address, and port
        char State[50];
        struct Node* head = NULL;
        sscanf(token, "%s", State);

        //If peer is not Leader than continue back to beginning of while loop
        if(strcmp(State,"Leader") == 0)
        {
            strcpy(peer.state, "Leader");
            struct PeerTuple Leader;
            char name[MAX_NAME_LENGTH];
            char ip_address[16];
            char p_state[10];
            unsigned int port;
            
            token = strtok(NULL, "\n");
            sscanf(token,"%s %s %u %s", name, ip_address, &port, p_state);
            
            printf("Leader Tuple: %s, %s, %u\n\n", name,ip_address,port);
            Leader.indentifier =0;
            strcpy(Leader.ip_address,ip_address);
            strcpy(Leader.name,name);
            Leader.p_port = port;
            Leader.record_size = 0;

            //Insert into Linked List
            insertEnd(&head, Leader);
        }
        else
        {
            continue;
        }

        //Create the ring and store it in linked list 
        
        int i = 1; //Skip the leaders tuple, start from the second line
        
        while ((token = strtok(NULL, "\n")) != NULL){
            char peer_name[MAX_NAME_LENGTH + 1];
            char ip_address[16];
            unsigned int p_port;
            char p_state[10];
            struct PeerTuple newTuple;

            //Parse the peer information from the token
            sscanf(token, "%s %s %u %s", peer_name, ip_address, &p_port, p_state);

            if(strcmp(p_state, "Free") == 0)
            {
                send_stop(peer_name, ip_address, p_port);
                continue;
            }
            
            //Send set_id command to peer
            printf("Name: %s\nAddress: %s\nPort: %u\nState: %s\n",peer_name,ip_address,p_port,p_state);
            
            newTuple.indentifier = i;
            strcpy(newTuple.ip_address,ip_address);
            strcpy(newTuple.name,peer_name);
            newTuple.p_port = p_port;
            newTuple.record_size = 0;

            insertEnd(&head,newTuple);
            send_id(peer_name, ip_address, p_port,p_state, i, n, token);

            i++;
        }

        //Make linked list a cycle
        makeLastPointToHead(head);

        //Print the ring order 
        printList(head);

//------------------Parsing Data File-------------------------------
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

        // Read the file line by line and count lines
        char buffer[1024]; // Adjust buffer size as needed
        // Skip the first line
        fgets(buffer, 1024, file);
        while (fgets(buffer, sizeof(buffer), file) != NULL) {
            count++;
        }

        //Determine the first prime number that is twice the line count 

        int s = find_prime(count);
        printf("the file has %d lines\n", count);

        fseek(file,0,SEEK_SET);
        char line[1024];

        struct Node* current = head;
        while (fgets(line, sizeof(line), file) != NULL) {
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

            //printf("Event ID: %lu\n", event.event_id);


            int pos = event.event_id % s;
            int id = pos % n;
            
            do{
                if(current->data.indentifier == id){
                    printf("Send Record to Peer: %d\n", current->data.indentifier);
                    current->data.record_size++;
                    send_tuple(current->data.name, current->data.ip_address, current->data.p_port, current->data.indentifier, n, event);
                    current = current->next;
                }
                else{
                    current = current->next;
                }
                
            }while(current != head);
        }

        //printHashTable(hash_table, 5000);

        fclose(file); 
        closedir(dir);

        //Leader record size
        printf("The record size for peer %s is: %d\n", head->data.name, head->data.record_size);
        current = head->next;

        //Iterate through ring and ouput the amount of records each peer is storing
        do{
            
            send_stop(current->data.name, current->data.ip_address, current->data.p_port);
            printf("The record size for peer %s is: %d\n", current->data.name, current->data.record_size);
            current = current->next;
            
        }while(current!=head);
    }

    close(sock);
    return 0;
}