#include <winsock2.h> //include order matters
#include <windows.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>

// Need to link with Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#define MSG_LEN 65536 //max size of message
#define DEFAULT_PORT "52673" //arbitrary port number

std::atomic<bool> persist_read = true; //flag for reader thread
std::mutex console_lock; //lock to prevent the console from mixing output and input

//change console color by inputting a hex value
void set_console_color()
{
	std::string cmd = "color ";
	std::string color;
	std::cout << "Enter console color hex #(0-F): ";
	std::cin >> color;
	cmd.append(color);
	system(cmd.c_str());
}

//Reader thread
void reader(SOCKET* cli, char* buf)
{
	while (persist_read)
	{
		memset(buf, 0, MSG_LEN);
		int iResult = recv(*cli, buf, MSG_LEN, NULL);
		if (iResult > 0)
		{
			console_lock.lock();
			std::cout << buf << '\n'; //lock to print cleanly
			console_lock.unlock();
		}

		if (strstr(buf, "*OTHER USER DISCONNECTED*" + 0x01))
		{
			persist_read = false;
			shutdown(*cli, SD_BOTH);
		}
	}
}

void writer(SOCKET* cli, std::string& uname)
{
	int iResult;
	std::string msg;
	while (msg != "/exit" && msg != "/dc")
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
			iResult = send(*cli, "*OTHER USER DISCONNECTED*" + 0x01, 27, NULL); //send exit code to other socket
			shutdown(*cli, SD_BOTH); //shutdown socket
			break;
		}
		else
		{
			iResult = send(*cli, full_msg.c_str(), full_msg.size(), NULL); //normal message send
			full_msg.clear();
		}
	}
}

int main()
{
	std::string msg;
	WSAData wsaData;
	int iResult = 0;

	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0)
	{
		std::cout << "WSAStartup failed with error: " << iResult << '\n';
		return -1;
	}
	
	SOCKET Listener = INVALID_SOCKET;
	SOCKET Client = INVALID_SOCKET;

	struct addrinfo* res = NULL;
	struct addrinfo hints;

	char recvbuf[MSG_LEN];
	int recvlen = MSG_LEN;
	memset(recvbuf, 0, MSG_LEN);

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	//resolve server address and port 
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &res); //ws2tcpip.h
	if (iResult != 0)
	{
		std::cout << "getaddrinfo failed with: " << iResult << '\n';
		WSACleanup();
		return -2;
	}

	Listener = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (Listener == INVALID_SOCKET)
	{
		std::cout << "Listener failed with error: " << WSAGetLastError() << '\n';
		freeaddrinfo(res);
		WSACleanup();
		return -3;
	}

	//Set up the TCP listening socket
	iResult = bind(Listener, res->ai_addr, (int)res->ai_addrlen);
	if (iResult == SOCKET_ERROR)
	{
		std::cout << "bind() failed with error: " << WSAGetLastError() << '\n';
		freeaddrinfo(res);
		closesocket(Listener);
		WSACleanup();
		return -4;
	}

	freeaddrinfo(res);

	std::cout << "Listening for incoming connection\n";
	iResult = listen(Listener, 1);
	if (iResult == SOCKET_ERROR)
	{
		std::cout << "listen() failed with error: " << WSAGetLastError() << '\n';
		closesocket(Listener);
		WSACleanup();
		return -5;
	}

	//Accept incoming client
	std::cout << "Accepting client...\n";
	Client = accept(Listener, NULL, NULL);
	if (Client == INVALID_SOCKET)
	{
		std::cout << "accept() failed with error: " << WSAGetLastError() << '\n';
		closesocket(Listener);
		WSACleanup();
		return -6;
	}
	
	std::string name;
	std::cout << "Enter username: ";
	std::cin >> name;

	std::string username = "[" + name + "] ";
	
	system("cls");
	std::cout << "[Connection Established Welcome to the Chatroom]\n";

	//listen socket can be destroyed
	closesocket(Listener);

	//thread for sending/recving
	std::thread r(reader, &Client, recvbuf);
	std::thread w(writer, &Client, std::ref(username));

	//join reader/writer threads
	r.join();
	w.join();

	//cleanup
	std::cout << "Closing chatroom\n";
	shutdown(Client, SD_BOTH);
	closesocket(Client);

	WSACleanup();
	return 0;
}
