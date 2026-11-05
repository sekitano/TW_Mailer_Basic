#include <iostream>
#include <string>
#include <cstdlib>
#include <cerrno>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

struct Connection
{
    int socket;
    string receiveBuffer;
};

bool readLine(Connection& connection, string& line);
bool sendAll(int socket, const string& data);

void printHelp();

bool readInput(string& input);



int main (int argc, char* argv[])
{
    // get and validate arguments 
    if (argc != 3)
    {
        cerr << "Usage: ./client <ip> <port>" << endl;
        return EXIT_FAILURE;
    }

    int port = atoi(argv[2]);

    if (port < 1 || port > 65535)
    {
        cerr << "Invalid port number!" << endl;
        return EXIT_FAILURE;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    if (inet_pton(AF_INET, argv[1], &serverAddress.sin_addr) != 1) 
    {
        cerr << "Invalid IPv4 address!" << endl;
        return EXIT_FAILURE;
    }

    ////////////////////////////////////////////////////////////////////////////
    // create socket and connect
    int clientSocket;
    if ((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        cerr << "Error creating socket" << endl;
        return EXIT_FAILURE;
    }

    if (connect(clientSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == -1)
    {
        cerr << "Error connecting socket to server" << endl;
        close(clientSocket);
        return EXIT_FAILURE;
    }

    cout << "Connected to server " << argv[1] << " on port " << port << endl;
    printHelp();

    Connection connection{cleintSocket, ""};

    ////////////////////////////////////////////////////////////////////////////
    // main loop

    bool running = true;
    while(running)
    {
        string command; 
        cout << "Enter a command: " << flush;
        







    }


    ////////////////////////////////////////////////////////////////////////////
    // close socket
    if (close(clientSocket) == -1)
    {
        cerr << "error when closing socket" << endl;
        return EXIT_FAILURE;
    }

    cout << "Goodbye!" << endl;
    return EXIT_SUCCESS;
}

void printHelp()
{
    cout << "Commands: SEND, LIST, READ, DEL, QUIT, HELP" << endl;
}

bool readInput(string& input)
{
    if(!getline(cin, input))
    {
        return false;
    }


}
