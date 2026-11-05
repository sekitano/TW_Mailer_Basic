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

const size_t MAX_USERNAME_LENGTH = 8;
const size_t MAX_SUBJECT_LENGTH = 80;

void printHelp();

bool readInput(string& input);
bool readUsername(string& input);
bool readMessageNumber(string& input);

bool handleSend(Connection& connection);
bool handleList(Connection& connection);
bool handleRead(Connection& connection);
bool handleDelete(Connection& connection);

bool printOkOrError(Connection& connection);
bool readLine(Connection& connection, string& line);
bool sendAll(int socket, const string& data);



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
    serverAddress.sin_family = AF_INET; // uses IPv4
    serverAddress.sin_port = htons(port); // htons (host to network short) converts from host byte order to network byte order

    if (inet_pton(AF_INET, argv[1], &serverAddress.sin_addr) != 1) 
    {
        cerr << "Invalid IPv4 address!" << endl;
        return EXIT_FAILURE;
    }

    // create socket and connect
    int clientSocket;
    if ((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) // socket is created
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

    // connection worked
    cout << "Connected to server " << argv[1] << " on port " << port << endl; 
    printHelp();

    Connection connection{clientSocket, ""};

    // MAIN LOOP
    bool running = true;
    while(running)
    {
        string command; 
        cout << "Enter a command: " << flush;
        if(!readInput(command))
        {
            command = "QUIT";
        }
        if(command.empty())
        {
            continue; 
        }

        bool connectionOk = true; // handle functions only return false if a connection error / problem with the server happens

        if (command == "SEND")
        {
            connectionOk = handleSend(connection);
        }
        else if (command == "LIST")
        {
            connectionOk = handleList(connection);
        }
        else if (command == "READ")
        {
            connectionOk = handleRead(connection);
        }
        else if (command == "DEL")
        {
            connectionOk = handleDelete(connection);
        }
        else if (command == "QUIT")
        {
            // server does not respond to QUIT
            sendAll(connection.socket, "QUIT\n");
            running = false;
        }
        else if (command == "HELP")
        {
            printHelp();
        }
        else
        {
            cerr << "Unknown command, type HELP for command list" << endl;
        }

        if (!connectionOk) // only happens if one of the handle functions goes wrong and returns false
        {
            cerr << "Connection to server lost." << endl;
            running = false;
        }
    }

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
    return true;
}

bool readUsername(string& input)
{
    if(!getline(cin, input))
    {
        return false;
    }
    if(input.empty() || input.length() > MAX_USERNAME_LENGTH)
    {
        cerr << "Usernames are between 1 and " << MAX_USERNAME_LENGTH << " characters long" << endl;
        return false;
    }
    // making sure only a-z and 0-9 are used
    for(char c : input)
    {
        if(!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')))
        {
            cerr << "Usernames can only contain a-z and 0-9" << endl;
            return false; 
        }
    }
    return true;
}

bool readMessageNumber(string& number)
{
    if(!getline(cin, number))
    {
        return false;
    }   

    int value = atoi(number.c_str()); // message numbers can't be negative or 0
    if(value < 1)
    {
        cerr << "Message number must be a number >= 1" << endl;
        return false; 
    }

    number = to_string(value); // make sure no letters are left in number (clean up with atoi)
    return true;
}

////////////////////////////////////////////////////////////////////////////
// server communication 
// handle functions only return false if something with the server connection went wrong
// for other issues they return with true

// SEND\n<Sender>\n<Receiver>\n<Subject (max. 80 chars)>\n<message (multi-line; no length restrictions)\n>.\n 
// server responds with OK\n or ERR\n
bool handleSend(Connection& connection)
{
    string sender;
    string receiver;
    string subject;

    cout << "Sender: " << flush;
    if (!readUsername(sender)) return true;
    cout << "Receiver: " << flush;
    if (!readUsername(receiver)) return true;

    cout << "Subject: " << flush;
    if(!readInput(subject)) return true;

    if (subject.empty() || subject.length() > MAX_SUBJECT_LENGTH)
    {
        cerr << "Subject must be between 1 and " << MAX_SUBJECT_LENGTH << " characters long." << endl;
        return true;
    }

    cout << "Message (end with a line only containing \".\"):" << endl;

    string message;
    string line;

    while (true)
    {
        if (!readInput(line))
        {
            cerr << "Input ended before \".\" line, message not sent" << endl;
            return true;
        }

        if (line == ".") // stop reading text when a line only containing "." is read
        {
            break;
        }

        message += line + "\n"; // each typed line added to the message string
    }

    // buliding the whole message before it can be sent
    string request = "SEND\n" + sender + "\n" + receiver + "\n" + subject + "\n" + message + ".\n";

    if (!sendAll(connection.socket, request)) // send the SEND request
    {
        return false; // sending didn't work, only return false because something is wrong with the connection
    }

    return printOkOrError(connection);
}

// LIST\n<Username>\n 
// server response: <count of messages of the user (0 if no message or user unknown)>\n<subject 1>\n<subject N>\n 
bool handleList(Connection& connection)
{
    string username; 

    cout << "Username: " << flush;
    if(!readUsername(username)) return true;

    if(!sendAll(connection.socket, "LIST\n" + username + "\n")) // send LIST request to server
    {
        return false;
    }

    string countLine;

    if(!readLine(connection, countLine)) // get the first reponse line (number: amount of messages)
    {
        return false;
    }

    int count = atoi(countLine.c_str()); // cast string to real number with atoi (c_str needed for atoi)
    cout << count << " message(s)" << endl;

    for(int i = 1; i <= count; i++) // goes through specified number of messages (count) and prints their subject
    {
        string subject;

        if(!readLine(connection, subject))
        {
            return false; 
        }

        cout << "   [" << i << "] " << subject << endl; // format: [i] message-subject
    }
    
    return true;
}

// READ\n<Username>\n<Message-Number>\n 
// server responds with: OK\n<complete message content (as defined in SEND)>\n
// or ERR\n
bool handleRead(Connection& connection)
{
    string username;
    string number; 

    cout << "Username: " << flush;
    if(!readUsername(username)) return true; 
    cout << "Message number: " << flush;
    if(!readMessageNumber(number)) return true;

    // send READ request to server
    if(!sendAll(connection.socket, "READ\n" + username + "\n" + number + "\n")) // send actual READ request
    {
        return false;
    }

    string status; 
    if(!readLine(connection, status)) // get the Ok or Err confirmation from receivedBuffer
    {
        return false; 
    }
    if(status != "OK")
    {
        cout << status << endl;
        return true;
    }

    cout << status << endl; // "OK"
    cout << "-------------------------------" << endl;

    string line;
    while (true) // get the actual message body
    {
        if(!readLine(connection, line))
        {
            return false; 
        }

        if(line == ".") // reached end of message
        {
            break;
        }

        cout << line << endl;
    }
    
    cout << "-------------------------------" << endl;
    return true;
}

// DEL\n<Username>\n<Message-Number>\n 
// server responds with OK\n or ERR\n
bool handleDelete(Connection& connection)
{
    string username;
    string number; 

    cout << "Username (inbox): " << flush; 
    if(!readUsername(username)) return true;
    cout << "Number of message to delete: " << flush; 
    if(!readMessageNumber(number)) return true; 

    // DELETE request ist sent to server
    if(!sendAll(connection.socket, "DEL\n" + username + "\n" + number + "\n")) 
    {
        return false; 
    }

    return printOkOrError(connection);
}


////////////////////////////////////////////////////////////////////////////

bool printOkOrError(Connection& connection)
{
    string answer;

    if(!readLine(connection, answer)) 
    {
        return false;
    }
    
    cout << answer << endl;
    return true;
}

// repeats send() until everythign is sent
bool sendAll(int socket, const string& data)
{
    size_t totalSent = 0;

    while(totalSent < data.size())
    {
        // ssize_t send(int socket, const void *buffer, size_t length, int flags);
        ssize_t bytesSent = send(socket, data.data() + totalSent, data.size() - totalSent, MSG_NOSIGNAL);
        // data.data() points to beginning of the message, + totalSent moves it along to where in the message
        // it should continue sending

        if(bytesSent > 0)
        {
            totalSent += static_cast<size_t>(bytesSent);
        }
        else if (bytesSent == -1 && errno == EINTR) 
        {
            continue;
        }
        else
        {
            cerr << "Send error" << endl;
            return false;
        }
    }
    return true;
}

// returns next line without "\n" from the connection, returns false if server/connectino fails
bool readLine(Connection& connection, string& line)
{
    while(true)
    {
        size_t position = connection.receiveBuffer.find('\n'); // looking for first \n and returns it's position

        if(position != string::npos) // if(a complete line ending in \n is already in the buffer)
        {
            line = connection.receiveBuffer.substr(0, position); // returns everything before position(\n)
            connection.receiveBuffer.erase(0, position +1); // deletes everything inkluding \n from buffer

            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            return true;
        }

        char buffer[1024];
        ssize_t bytesReceived = recv(connection.socket, buffer, sizeof(buffer), 0);
        if(bytesReceived > 0)
        {
            connection.receiveBuffer.append(buffer, bytesReceived); // adds local buffer contents to receiveBuffer
        }
        else if(bytesReceived == 0)
        {
            cerr << "Server connection is closed" << endl;
            return false; 
        }
        else if(errno == EINTR)
        {
            continue;
        }
        else
        {
            cerr << "Receive error" << endl;
            return false;
        }
    }
}