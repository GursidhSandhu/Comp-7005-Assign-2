#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fstream>
#include <cstring>
#define DEFAULTFDVALUE -1
#define NUMBOFARGS 4
#define BUFFERSIZE 1024
#define EOFINDICATOR "EOF\n"

using namespace std;

class Client{
public:
    // class members
    int socketFD;
    const int port;
    const string ipAddress, fileName;

    // Constructor
    Client(const string ipAddress, const int port, const string fileName):
    socketFD(DEFAULTFDVALUE),ipAddress(ipAddress),port(port),fileName(fileName){}
    // Destructor
    ~Client(){ close_socket(socketFD);}

    // function prototypes
    int create_socket();
    void attempt_socket_connection(int socketFD);
    vector<string> locate_file_contents();
    void send_request(int socketFD, vector <string> fileContents);
    void handle_response(int socketFD);
    void close_socket(int socketFD);
};
int main(int argc, char*argv[]) {
    // validate command line arguments
    if(argc != NUMBOFARGS){
        cerr << "Incorrect amount of command line arguments,only provide socketPath and fileName" << endl;
        return EXIT_FAILURE;
    }

    string ipAddress = argv[1];
    int port = stoi(argv[2]);
    string fileName = argv[3];

    cout << "Client has started" << endl;

    Client *client = new Client(ipAddress,port,fileName);
    int socketFD = client->create_socket();
    client->attempt_socket_connection(socketFD);
    vector<string> fileContents = client->locate_file_contents();
    client->send_request(socketFD,fileContents);
    client->handle_response(socketFD);
    delete(client);

    // Return 0 to indicate the program ended successfully
    return 0;
}

int Client::create_socket() {

    // initialize the socket
    socketFD = socket(AF_INET,SOCK_STREAM,0);
    // check for error
    if(socketFD == -1){
        cerr << "Error in creating socket: " << strerror(errno) <<endl;
        exit(EXIT_FAILURE);
    }
    cout << "Socket created successfully" << endl;
    return socketFD;
}

void Client::attempt_socket_connection(int socketFD) {

    // initialize sockaddr_in structure
    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET;

    if (inet_pton(AF_INET, ipAddress.c_str(), &addr.sin_addr) <= 0) {
        cerr << "Invalid IP address" << endl;
        exit(EXIT_FAILURE);
    }
    addr.sin_port = htons(port);

    // attempt connecting
    if(connect(socketFD,(struct sockaddr*)&addr,sizeof(addr)) == -1){
        cerr << "Error in connecting to server socket: "<< strerror(errno) <<endl;
        exit(EXIT_FAILURE);
    }

    cout << "Successfully connected at IP: " << ipAddress <<" and port: "<< port << endl;
}

vector<string> Client::locate_file_contents() {

    vector<string> fileContents;

    // construct the relative path to include directory then find file
    string filePath = "../include/" + fileName;
    ifstream file(filePath);

    if(!file.is_open()){
        cerr << "Error finding or opening "<< fileName << strerror(errno) << endl;
        exit(EXIT_FAILURE);
    }

    // extract each line and copy over
    string line;
    while(getline(file,line)){
        fileContents.push_back(line);
    }

    file.close();
    return fileContents;
}

void Client::send_request(int socketFD, vector <string> fileContents) {

    // construct request and send
    string requestContents;
    // loop through vector and copy over contents
    for(const string& line:fileContents){
        requestContents += line + "\n";
    }
    requestContents += EOFINDICATOR;

    // send request
    ssize_t request = send(socketFD,requestContents.c_str(),requestContents.size(),0);

    if(request == -1){
        cerr << "Error in sending request to server: "<< strerror(errno) <<endl;
        exit(EXIT_FAILURE);
    }

    cout << "Successfully sent request to the server" << endl;
}

void Client::handle_response(int socketFD) {
    int numb_of_alphabets;
    ssize_t responseData;

    // Read the integer sent by the server
    responseData = recv(socketFD, &numb_of_alphabets, sizeof(numb_of_alphabets), 0);
    
    if (responseData == -1) {
        cerr << "Error reading response from server: " << strerror(errno) << endl;
        exit(EXIT_FAILURE);
    } else if (responseData == 0) {
        cerr << "Server closed the connection" << endl;
        exit(EXIT_FAILURE);
    }

    // Convert the number from network byte order to host byte order
    numb_of_alphabets = ntohl(numb_of_alphabets);

    // Print the number of alphabetic letters received
    cout << "Number of alphabetic letters in " << fileName << " is: " << numb_of_alphabets << endl;
}


void Client::close_socket(int socketFD) {
    if(close(socketFD) == -1){
        cerr << "Error closing socket: " << strerror(errno) << endl;
        exit(EXIT_FAILURE);
    }
    cout << "Socket closed successfully" << endl;
}