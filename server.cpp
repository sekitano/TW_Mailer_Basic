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
        cerr << "Error creaing Socket!" << endl;
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

        string receivedLine;

        if (readLine(client, receivedLine)) // textzeile wird von client empfangen, wenn erfolgreich dann == true, nachricht wird in receivedLine gespeichert
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