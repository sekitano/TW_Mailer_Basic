#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <cerrno>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;
namespace fs = std::filesystem;



struct ClientConnection
{
    int socket;
    string receiveBuffer;
};

bool readLine(ClientConnection& client, string& line);
bool sendAll(int clientSocket, const string& data);
bool sendResponse(int clientSocket, const string& response);

int main (int argc, char* argv[])
{

    if (argc != 3) 
    {
        cerr << "Usage: ./twmailer-server <port> <mail-spool-directoryname>" << endl;
        return 1;
    }

    int port = atoi(argv[1]);
    string spoolDirectory = argv[2];

    if (port < 1 || port > 65535)
    {
        cerr << "Invalid port number!" << endl;
        return 1;
    }

    try 
    {
        fs::create_directories(spoolDirectory);
    }
    catch (const fs::filesystem_error& e)
    {
        cerr << "Cannot create mail spool directory: " << e.what() << endl;
        return 1;
    }

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket == -1)
    {
        cerr << "Error creaing Socket!" << endl;
        return 1;
    }

    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        cerr << "Error setting socket options!" << endl;
        close(serverSocket);
        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(port);

    if (bind(serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == -1)
    {
        cerr << "Error binding socket to port " << port << endl;
        close(serverSocket);
        return 1;
    }

    if (listen(serverSocket, 5) == -1)
    {
        cerr << "Error listening on socket!" << endl;
        close(serverSocket);
        return 1;
    }

    cout << "TW-Mailer Server started on port " << port << endl;
    cout << "Waiting for client..." << endl;

    while (true) 
    {
        sockaddr_in clientAddress{};
        socklen_t clientAddressLength = sizeof(clientAddress);

        int clientSocket = accept(serverSocket, (sockaddr*)&clientAddress, &clientAddressLength);

        if (clientSocket == -1)
        {
            cerr << "Error accepting client connection!" << endl;
            continue;
        }

        char clientIP[INET_ADDRSTRLEN];

        inet_ntop(AF_INET, &clientAddress.sin_addr, clientIP, sizeof(clientIP));

        cout << "Client connected: " << clientIP << endl;

        ClientConnection client{clientSocket, ""};

        string receivedLine;

        if (readLine(client, receivedLine))
        {
            cout << "Received: " << receivedLine << endl;

            if (!sendResponse(client.socket, "OK\n"))
            {
                cerr << "Error sending response!" << endl;
            }
        }
        else
        {
            cout << "Client closed connection or receiving failed." << endl;
        }
        

        close(clientSocket);

        cout << "Client disconnected." << endl;
    }

    close(serverSocket);

    return 0;
}

bool sendAll(int clientSocket, const string& data)
{
    size_t totalSent = 0;

    while (totalSent < data.size())
    {
        ssize_t bytesSent = send(clientSocket, data.data() + totalSent, data.size() - totalSent, MSG_NOSIGNAL);

        if (bytesSent > 0)
        {
            totalSent += bytesSent;
        }
        else if (bytesSent == -1 && errno == EINTR)
        {
            continue;
        }
        else
        {
            return false;
        }
    }

    return true;
}

bool sendResponse(int clientSocket, const string& response)
{
    return sendAll(clientSocket, response);
}

bool readLine(ClientConnection& client, string& line)
{
    while (true)
    {
        size_t position = client.receiveBuffer.find('\n');

        if (position != string::npos)
        {
            line = client.receiveBuffer.substr(0, position);

            client.receiveBuffer.erase(0, position + 1);

            if (!line.empty() && line.back() == '\r') 
             {
                line.pop_back();
             }

             return true;
        }

        char buffer[1024];

        ssize_t bytesReceived = recv(client.socket, buffer, sizeof(buffer), 0);

        if (bytesReceived > 0)
         {
            client.receiveBuffer.append(buffer, bytesReceived);
         }
         else if (bytesReceived == 0)
         {
            return false;
         }
         else 
         {
            if (errno == EINTR) 
            {
                continue;
            }

            return false;
         }
    }
}