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

bool handleSend(ClientConnection& client, const string& spoolDirectory);
bool handleList(ClientConnection& client, const string& spoolDirectory);
bool handleRead(ClientConnection& client, const string& spoolDirectory);
bool handleDelete(ClientConnection& client, const string& spoolDirectory);

int main (int argc, char* argv[])
{

    if (argc != 3) 
    {
        cerr << "Usage: ./server <port> <mail-spool-directoryname>" << endl;
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

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0); // IPv4, TCP, passendes Protokoll von Betriebssystem gewählt

    if (serverSocket == -1)
    {
        cerr << "Error creating Socket!" << endl;
        return 1;
    }

    int opt = 1;
    // socket einstellungen werden festgelegt
    // params: für welchen socket, welche einstellung verändern (allgemeine), welche einstellung, 1, größe in bytes
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) // ??????
    {
        cerr << "Error setting socket options!" << endl;
        close(serverSocket);
        return 1;
    }

    sockaddr_in serverAddress{}; // erstellt sturktur für IPv4 addressen -> mit nullwerten initialisiert
    serverAddress.sin_family = AF_INET; // festgelegt dass IPv4 verwendet wird
    serverAddress.sin_addr.s_addr = INADDR_ANY; // -> hört auf alle verfügbare IPv4 schnittstelle ab
    serverAddress.sin_port = htons(port); // port wird festgelegt  ---  htons = host to network short, wandelt portnumber in richtige byte reihenfolgen für netzwerk um

    if (bind(serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) // socket an port binden, params: socket von server, netzwerkaddresse, größe der addressstruktur
    {
        cerr << "Error binding socket to port " << port << endl;
        close(serverSocket);
        return 1;
    }

    if (listen(serverSocket, 5) == -1) // listen mode sozusagen, warten auf connection requests, 5 == warteschlange für not accepted connections
    {
        cerr << "Error listening on socket!" << endl;
        close(serverSocket);
        return 1;
    }

    cout << "TW-Mailer Server started on port " << port << endl; // wenn connection erfolgreich ist wird das geprintet
    cout << "Waiting for client..." << endl;

    while (true) // server läuft
    {
        sockaddr_in clientAddress{};
        socklen_t clientAddressLength = sizeof(clientAddress);

        int clientSocket = accept(serverSocket, (sockaddr*)&clientAddress, &clientAddressLength); // client verbindung wird angenommen

        if (clientSocket == -1)
        {
            cerr << "Error accepting client connection!" << endl;
            continue;
        }

        char clientIP[INET_ADDRSTRLEN];

        inet_ntop(AF_INET, &clientAddress.sin_addr, clientIP, sizeof(clientIP)); // IP Address von client wird in Zeichenkette umgewandelt

        cout << "Client connected: " << clientIP << endl;

        ClientConnection client{clientSocket, ""}; // erstellen ClientConnection var, clientSocket -> socket vom verbundenen client, "" -> leerer empfangsbuffer

        while (true)
        {
            string command;

            if (!readLine(client, command))
            {
                cout << "Client closed connection or receiving failed." << endl;
                break;
            }

            cout << "Received command: " << command << endl;

            if (command == "SEND")
            {
                if (!handleSend(client, spoolDirectory))
                {
                    cerr << "Error handling SEND command!" << endl;
                    break;
                }
            }

            else if (command == "LIST")
            {
                if (!handleList(client, spoolDirectory))
                {
                    cerr << "Error handling LIST command!" << endl;
                    break;
                }
            }

            else if (command == "READ")
            {
                if (!handleRead(client, spoolDirectory))
                {
                    cerr << "Error handling READ command!" << endl;
                    break;
                }
            }

            else if (command == "DEL")
            {
                if (!handleDelete(client, spoolDirectory))
                {
                    cerr << "Error handling DEL command!" << endl;
                    break;
                }
            }

            else if (command == "QUIT")
            {
                cout << "Client requested to quit." << endl;
                break;
            }
            else
            {
                cerr << "Unknown command: " << command << endl;

                if (!sendResponse(client.socket, "ERR\n"))
                {
                    cerr << "Error sending response!" << endl;
                    break;
                }
            }
        }

        close(clientSocket);

        cout << "Client disconnected." << endl;
    }

    close(serverSocket);

    return 0;
}

bool sendAll(int clientSocket, const string& data)
{
    size_t totalSent = 0; // hier wird anzahl an erfolgreich gesendeter bytes gespeichert

    while (totalSent < data.size()) // läuft bis alle daten gesendet wurden
    {
        // params: client an dem gesendet wird, wo die noch nicht gesendeten daten beginnen, wie viele bytes noch gesendet werden sollen,
        // MSG_NOSIGNAL: verhindert SIGPIPE signal bei geschlossener verbindung
        ssize_t bytesSent = send(clientSocket, data.data() + totalSent, data.size() - totalSent, MSG_NOSIGNAL); 

        if (bytesSent > 0)
        {
            totalSent += bytesSent;
        }
        else if (bytesSent == -1 && errno == EINTR) // falls systemaufruf durch signal unterbrochen wird, schleife neu durchlaufen
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
    while (true) // läuft bis leere zeile, connection stopped oder error
    {
        size_t position = client.receiveBuffer.find('\n'); // parsen von \n

        if (position != string::npos)
        {
            line = client.receiveBuffer.substr(0, position); // line ohne \n hier rein

            client.receiveBuffer.erase(0, position + 1); // daten aus buffer raus

            if (!line.empty() && line.back() == '\r') // windows zeilenumbrüche berücksichtigt
             {
                line.pop_back();
             }

             return true;
        }

        char buffer[1024]; // empfangene daten zuerst hier reingespeichert

        // params: client von dem empfangen wird, speicherort der daten, max. größe pro empfang, 0 == keine speziellen flags setzen
        ssize_t bytesReceived = recv(client.socket, buffer, sizeof(buffer), 0); // daten empfangen

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

bool handleSend(ClientConnection& client, const string& spoolDirectory)
{
    string sender;
    string receiver;
    string subject;
    string message;
    string line;

    if (!readLine(client, sender)) 
    {
        return false;
    }
    if (!readLine(client, receiver)) 
    {
        return false;
    }
    if (!readLine(client, subject)) 
    {
        return false;
    }

    while (true)
    {
        if (!readLine(client, line)) 
        {
            return false;
        }

        if (line == ".") 
        {
            break;
        }

        message += line + "\n";
    }

    auto isValidUsername = [](const string& username)
    {
        if (username.empty() || username.length() > 8)
        {
            return false;
        }

        for (char c : username)
        {
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')))
            {
                return false;
            }
        }

        return true;
    };

    if (!isValidUsername(sender) || !isValidUsername(receiver) || subject.empty() || subject.length() > 80)
    {
        return sendResponse(client.socket, "ERR\n");
    }

    fs::path userDirectory = fs::path(spoolDirectory) / receiver;

    try
    {
        fs::create_directories(userDirectory);
    }
    catch (const fs::filesystem_error& e)
    {
        cerr << "Cannot create user directory: " << e.what() << endl;
        return sendResponse(client.socket, "ERR\n");
    }

    size_t messageNumber = 1;

    while (fs::exists(userDirectory / (to_string(messageNumber) + ".txt")))
    {
        messageNumber++;
    }

    fs::path messageFile = userDirectory / (to_string(messageNumber) + ".txt");

    ofstream file(messageFile);

    if (!file)
    {
        cerr << "Cannot create message file!" << endl;
        return sendResponse(client.socket, "ERR\n");
    }

    file << sender << "\n";
    file << receiver << "\n";
    file << subject << "\n";
    file << message;

    file.close();

    if (!file)
    {
        cerr << "Error writing message file!" << endl;
        fs::remove(messageFile);
        return sendResponse(client.socket, "ERR\n");
    }

    return sendResponse(client.socket, "OK\n");

}

bool handleList(ClientConnection& client, const string& spoolDirectory)
{
    string username;

    if (!readLine(client, username))
    {
        return false;
    }

    if (username.empty() || username.length() > 8)
    {
        return sendResponse(client.socket, "0\n");
    }

    for (char c : username)
    {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')))
        {
            return sendResponse(client.socket, "0\n");
        }
    }

    fs::path userDirectory = fs::path(spoolDirectory) / username;

    if (!fs::exists(userDirectory))
    {
        return sendResponse(client.socket, "0\n");
    }

    string response;
    size_t messageCount = 0;

    while (true)
    {
        fs::path messageFile = userDirectory / (to_string(messageCount + 1) + ".txt");

        if (!fs::exists(messageFile))
        {
            break;
        }

        ifstream file(messageFile);

        if (!file)
        {
            return false;
        }

        string sender;
        string receiver;
        string subject;

        getline(file, sender);
        getline(file, receiver);
        getline(file, subject);

        response += subject + "\n";
        messageCount++;
    }

    string countResponse = to_string(messageCount) + "\n";

    return sendResponse(client.socket, countResponse + response);
}

bool handleRead(ClientConnection& client, const string& spoolDirectory)
{
    string username;
    string numberLine;

    if (!readLine(client, username))
    {
        return false;
    }

    if (!readLine(client, numberLine))
    {
        return false;
    }

    if (username.empty() || username.length() > 8)
    {
        return sendResponse(client.socket, "ERR\n");
    }

    for (char c : username)
    {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')))
        {
            return sendResponse(client.socket, "ERR\n");
        }
    }

    if (numberLine.empty())
    {
        return sendResponse(client.socket, "ERR\n");
    }

    for (char c : numberLine)
    {
        if (c < '0' || c > '9')
        {
            return sendResponse(client.socket, "ERR\n");
        }
    }

    size_t messageNumber;

    try
    {
        messageNumber = stoull(numberLine);
    }
    catch (const exception&)
    {
        return sendResponse(client.socket, "ERR\n");
    }

    if (messageNumber == 0)
    {
        return sendResponse(client.socket, "ERR\n");
    }

    fs::path messageFile = fs::path(spoolDirectory) / username / (to_string(messageNumber) + ".txt");

    if (!fs::exists(messageFile))
    {
        return sendResponse(client.socket, "ERR\n");
    }

    ifstream file(messageFile);

    if (!file)
    {
        return sendResponse(client.socket, "ERR\n");
    }

    string response = "OK\n";
    string line;

    while (getline(file, line))
    {
        response += line + "\n";
    }

    if (file.bad())
    {
        return sendResponse(client.socket, "ERR\n");
    }

    response += ".\n";

    return sendResponse(client.socket, response);
}

bool handleDelete(ClientConnection& client, const string& spoolDirectory)
{
    string username;
    string numberLine;

    if (!readLine(client, username))
    {
        return false;
    }

    if (!readLine(client, numberLine))
    {
        return false;
    }

    if (username.empty() || username.length() > 8)
    {
        return sendResponse(client.socket, "ERR\n");
    }

    for (char c : username)
    {
        if(!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')))
        {
            return sendResponse(client.socket, "ERR\n");
        }
    }

    if (numberLine.empty())
    {
        return sendResponse(client.socket, "ERR\n");
    }

    for (char c : numberLine)
    {
        if (c < '0' || c > '9')
        {
            return sendResponse(client.socket, "ERR\n");
        }
    }

    size_t messageNumber;

    try
    {
        messageNumber = stoull(numberLine);
    }
    catch (const exception&)
    {
        return sendResponse(client.socket, "ERR\n");
    }

    if (messageNumber == 0)
    {
        return sendResponse(client.socket, "ERR\n");
    }

    fs::path userDirectory = fs::path(spoolDirectory) / username;

    if (!fs::exists(userDirectory))
    {
        return sendResponse(client.socket, "ERR\n");
    }

    fs::path messageFile = userDirectory / (to_string(messageNumber) + ".txt");

    if (!fs::exists(messageFile))
    {
        return sendResponse(client.socket, "ERR\n");
    }

    try
    {
        size_t messageCount = 0;

        while (fs::exists(userDirectory / (to_string(messageCount + 1) + ".txt")))
        {
            messageCount++;
        }

        fs::remove(messageFile);

        for (size_t i = messageNumber + 1; i <= messageCount; i++)
        {
            fs::path oldFile = userDirectory / (to_string(i) + ".txt");

            fs::path newFile = userDirectory / (to_string(i - 1) + ".txt");

            fs::rename(oldFile, newFile);
        }
    }
    catch (const fs::filesystem_error& e)
    {
        cerr << "Error deleting message: " << e.what() << endl;
        return sendResponse(client.socket, "ERR\n");
    }

    return sendResponse(client.socket, "OK\n");
}