#include <winsock2.h> //include order matters
#include <windows.h>
#include <ws2tcpip.h>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")

#define MSG_LEN 65536 //max size of message
#define DEFAULT_PORT "52673" //arbitrary port number

std::atomic<bool> persist_read = true; //flag for reader thread
std::mutex console_lock; //lock to prevent the console from mixing output and input

//change console color by inputting a hex value
void set_console_color()
{
	std::string cmd = "color ";
	std::string color;
	std::cout << "Enter console color #(0-F): ";
	std::cin >> color;
	cmd.append(color);
	system(cmd.c_str());
}

//Reader thread
void reader(SOCKET* serv, char* buf)
{
	while (persist_read)
	{
		memset(buf, 0, MSG_LEN);
		int iResult = recv(*serv, buf, MSG_LEN, NULL);
		if (iResult > 0)
		{
			console_lock.lock();
			std::cout << buf << '\n'; //lock to print cleanly
			console_lock.unlock();
		}

		if (strstr(buf, "*OTHER USER DISCONNECTED*" + 0x01)) //exit code received
		{
			persist_read = false; //reader thread shutdown
			shutdown(*serv, SD_BOTH);//shutdown socket
			break;
		}
	}
}

void writer(SOCKET* serv, std::string& uname)
{
	int iResult;
	std::string msg;
	while (true)
	{
		msg = "";
		getline(std::cin, msg);
		std::string full_msg = uname + msg;
		if (msg == "/color") //lock the console to go to command menu
		{
			console_lock.lock();
			set_console_color();
			console_lock.unlock();
		}
		else if (msg == "/exit" || msg == "/dc") //exit sent
		{
			persist_read = false; //shut off reader thread
			iResult = send(*serv, "*OTHER USER DISCONNECTED*" + 0x01, 27, NULL); //send exit code to other socket
			shutdown(*serv, SD_BOTH); //shutdown socket
			break;
		}
		else
		{
			iResult = send(*serv, full_msg.c_str(), full_msg.size(), NULL); //normal message send
		}
	}
}

int main()
{
	//Socket initialization
	WSADATA wsaData;
	SOCKET ServerSocket = INVALID_SOCKET;
	struct addrinfo* result = NULL, hints;
	char recvbuf[MSG_LEN]; //reader buffer initialization
	int iResult = 0;
	int recvbuflen = MSG_LEN;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) 
	{
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	//Zero out the memory
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET; //AF_UNSPEC
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	std::string server_ip = "";
	std::cout << "Enter IP Address to connect to: ";
	std::cin >> server_ip;
	
	iResult = getaddrinfo(server_ip.c_str(), DEFAULT_PORT, &hints, &result);
	if (iResult != 0)
	{
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	// Create Socket Object
	ServerSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ServerSocket == INVALID_SOCKET)
	{
		printf("socket failed with error: %ld\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	// Connect to server.
	iResult = connect(ServerSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR)
	{
		closesocket(ServerSocket);
		ServerSocket = INVALID_SOCKET;
	}

	freeaddrinfo(result);

	//Make sure socket is not invalid
	if (ServerSocket == INVALID_SOCKET)
	{
		printf("Unable to connect to server!\n");
		WSACleanup();
		return 1;
	}

	std::string name;
	std::cout << "Enter username: ";
	std::cin >> name;
	std::string username = "[" + name + "] ";

	system("cls");

	std::cout << "[Connection Established Welcome to the Chatroom]\n";

	// Receive until the peer closes the connection
	std::thread r(reader, &ServerSocket, recvbuf);
	std::thread w(writer, &ServerSocket, std::ref(username));

	//join reader/writer threads
	r.join();
	w.join();
	
	// cleanup
	closesocket(ServerSocket);
	WSACleanup();

	return 0;
}
