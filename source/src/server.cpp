#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <csignal>
#include <fstream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sstream>
#define DEFAULTFDVALUE -1
#define NUMBOFARGS 2
#define BUFFERSIZE 1024
#define EOFINDICATOR "EOF\n"

using namespace std;

class Server{
public:
    // class members
    int socketFD;
    const int port;

    // Constructor
    Server(const int port) : socketFD(DEFAULTFDVALUE),port(port){}
    // Destructor
    ~Server(){ close_socket(socketFD);}

    // function prototypes
    int create_socket();
    void bind_socket(int socketFD);
    void listen_on_socket(int socketFD,int maxConnections);
    int accept_connection(int socketFD);
    vector<string> handle_request(int clientFD);
    int count_alphabetic_letters(vector<string> fileContents);
    void send_response(int clientFD,int numb_of_alphabets);
    void close_socket(int socketFD);
};

// flag to decide when program is done
volatile sig_atomic_t exit_flag = 0;

// Signal handler function
void sigint_handler(int signum) {
    exit_flag = 1;
}

// Function that sets up signal handler
// This function is purely based off of the example code from
// https://github.com/programming101dev/c-examples/blob/main/domain-socket/read-write/server.c
void setup_signal_handler() {
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    // Install the SIGINT handler
    // if this fails, exit program right here
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char*argv[]) {

    // Setup signal handler
    setup_signal_handler();

    // validate command line arguments
    if(argc != NUMBOFARGS){
        cerr << "Incorrect amount of command line arguments, only provide port number!" << endl;
        return EXIT_FAILURE;
    }
    int port = stoi(argv[1]);

    cout << "Server has started" << endl;

    Server *server = new Server(port);
    int socketFD = server->create_socket();
    server->bind_socket(socketFD);
    server->listen_on_socket(socketFD, SOMAXCONN);

    // Continuously accept clients until signal doesn't go off
    while (!exit_flag) {
        int clientFD = server->accept_connection(socketFD);

        if (clientFD == -1) {
            // ensure the signal isn't flagged
            if (exit_flag) {
                break;
            }
            // If there was an error accepting a client then try again
            continue;
        }

        vector<string> fileContents = server->handle_request(clientFD);
        if (fileContents.empty()) {
            continue;
        }

        int alphabetCount = server->count_alphabetic_letters(fileContents);
        if (alphabetCount < 0) {
            cerr << "Error counting alphabets from file contents: " << strerror(errno) << endl;
        } else {
            cout << "Number of alphabets counted successfully" << endl;
        }

        // client socket will shut itself down after receiving a response
        server->send_response(clientFD, alphabetCount);
    }

    // if exit flag reached
    delete(server);

    return 0;
}

int Server::create_socket() {

    // initialize the socket as over the network
    socketFD = socket(AF_INET,SOCK_STREAM,0);
    // check for error
    if(socketFD == -1){
        cerr << "Error in creating socket: " << strerror(errno) <<endl;
        exit(EXIT_FAILURE);
    }
    cout << "Socket created succesfully" << endl;
    return socketFD;
}

void Server::bind_socket(int socketFD) {

    // initialize sockaddr_in structure
    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port); 

    // attempt binding and check for error
    if(bind(socketFD,(struct sockaddr*)&addr,sizeof(addr))==-1){
        cerr << "Error in binding socket: " << strerror(errno) << endl;
        exit(EXIT_FAILURE);
    }

    cout << "Socket successfully bound at port " << port << endl;
}

void Server::listen_on_socket(int socketFD, int maxConnections) {
    if(listen(socketFD,maxConnections) == -1){
        cerr << "Error in listening on socket: " << strerror(errno) << endl;
        exit(EXIT_FAILURE);
    }
    cout << "Listening on socket for connections" << endl;
}

int Server::accept_connection(int socketFD) {

    // store client info
    struct sockaddr_in client_addr;
    socklen_t client_addr_size = sizeof(client_addr);

    int clientFD = accept(socketFD,(struct sockaddr*)&client_addr,&client_addr_size);
    if(clientFD == -1){
        cerr << "Error connecting to a new client: " << strerror(errno) << endl;
        return -1;
    }

    cout << "New client connected successfully!" << endl;
    return clientFD;
}

vector<string> Server::handle_request(int clientFD) {
    vector<string> fileContents;
    char buffer[BUFFERSIZE];
    string data;
    ssize_t requestData;

    // Read the data sent by the client
    while (true) {
        // store into buffer
        requestData = read(clientFD, buffer, BUFFERSIZE - 1);
        if (requestData <= 0) {
            if (requestData == 0) {
                // Connection closed by client
                cout << "Client disconnected." << endl;
                break;
            } else {
                cerr << "Error reading from client: " << strerror(errno) << endl;
                exit(EXIT_FAILURE);
            }
        }

        buffer[requestData] = '\0';
        data += buffer;

        // Check if we received the EOF indicator
        if (data.find(EOFINDICATOR) != string::npos) {
            // Remove the EOF indicator from the data
            data.erase(data.find(EOFINDICATOR));
            break;
        }
    }

    // Split the received data into a vector of strings and as file contents
    istringstream dataStream(data);
    string line;
    while (getline(dataStream, line)) {
        fileContents.push_back(line);
    }

    if (!(fileContents.empty())) {
        return fileContents;
    }
    // empty
    return fileContents;
}

int Server::count_alphabetic_letters(vector<string> fileContents) {
    int alphabetCount = 0;

    for (const auto& line : fileContents) {
        for (char c : line) {
            if (isalpha(c)) {
                alphabetCount++;
            }
        }
    }
    return alphabetCount;
}

void Server::send_response(int clientFD, int alphabetCount) {
    // Convert the integer to network byte order (big-endian)
    int network_order_numb = htonl(alphabetCount);

    // Send the integer to the client
    ssize_t responseBytes = send(clientFD, &network_order_numb, sizeof(network_order_numb), 0);

    if (responseBytes == -1) {
        cerr << "Error sending response: " << strerror(errno) << endl;
        exit(EXIT_FAILURE);
    }

    cout << "Response sent successfully to client" << endl;
}

void Server::close_socket(int socketFD) {
    if(close(socketFD) == -1){
        cerr << "Error closing socket: " << strerror(errno) << endl;
        exit(EXIT_FAILURE);
    }

    cout << "Socket closed successfully" << endl;
}