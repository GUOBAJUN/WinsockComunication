/*
* SOCKET CHATROOM CLIENT
* Copyright ©GUOBAJUN 2022
* USE TCP/IPv4
*/

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <iostream>
#include <cstring>
#include <ctime>

#pragma comment(lib, "Ws2_32.lib")

#define SERVER_PORT "10086"
#define DEFAULT_SERVER "127.0.0.1"
#define DEFAULT_BUFLEN 4096
#define UserNameLen 512

using std::cin;
using std::cout;
using std::cerr;
using std::endl;

HANDLE hChatThread[2]; // Reserver线程, Sender线程

/*
* CmdCheck：检查输入是否为客户端命令
* 参数：客户端输入的文本
* 当前只存在退出(>exit)命令
*/
DWORD WINAPI CmdCheck(char* Txt) {
	if (strcmp(Txt, ">exit") == 0)
		return 1;
	return 0;
}

/*
* Receiver：接收信息线程所调用的函数
* 参数：服务器Socket
* 会以收到信息的本地时间打上时间戳
*/
DWORD WINAPI Receiver(LPVOID lpParam) {
	SOCKET *ServerSocket = (SOCKET*)lpParam; // 服务器Socket
	char Buffer[DEFAULT_BUFLEN], strNow[DEFAULT_BUFLEN]; // 接收缓存、时间戳
	int byteCount;
	time_t Now;
	struct tm ptm;
	while (*ServerSocket != INVALID_SOCKET) {
		byteCount = recv(*ServerSocket, Buffer, DEFAULT_BUFLEN, 0);
		if (byteCount == SOCKET_ERROR) {
			cerr << "recv failed with Code: " << WSAGetLastError() << endl;
			closesocket(*ServerSocket);
			*ServerSocket = INVALID_SOCKET;
			return 1;
		}
		else if (byteCount == 0) {
			cout << "Connection ended!" << endl;
			*ServerSocket = INVALID_SOCKET;
			break;
		}
		time(&Now);
		localtime_s(&ptm, &Now);
		strftime(strNow, DEFAULT_BUFLEN, "[%x %X] ", &ptm);
		Buffer[byteCount] = '\0';

		//TODO:添加解密

		cout << strNow << Buffer << endl;
	}
	return 0;
}

/*
* Sender：发送信息进程所调用的函数
* 参数：服务器Socket
* 发送非客户端命令的文本（包括对服务器的命令）
*/
DWORD WINAPI Sender(LPVOID lpParam) {
	SOCKET* ServerSocket = (SOCKET*)lpParam; // 服务器Socket
	char Buffer[DEFAULT_BUFLEN]; // 发送缓存
	int byteCount;
	while (*ServerSocket != INVALID_SOCKET) {
		cin.getline(Buffer, DEFAULT_BUFLEN); // 一次读取一行

		//TODO:添加加密信息

		if (CmdCheck(Buffer)) {
			shutdown(*ServerSocket, SD_SEND);
			*ServerSocket = INVALID_SOCKET;
			break;
		}
		byteCount = send(*ServerSocket, Buffer, (int)lstrlenA(Buffer) + 1, 0);
		if (byteCount == SOCKET_ERROR) {
			cerr << "send failed with Code: " << WSAGetLastError() << endl;
			*ServerSocket = INVALID_SOCKET;
			return 1;
		}
	}
	return 0;
}

int main(int argc, char **argv) {
	WSADATA wsaData;
	SOCKET ConnectSocket = INVALID_SOCKET;
	addrinfo* results = NULL, * ptr = NULL, hints;
	char Buffer[DEFAULT_BUFLEN], hoststr[NI_MAXHOST], servstr[NI_MAXSERV], UserName[UserNameLen], Server[NI_MAXHOST];
	int iResult;

	// 配置客户端参数
	switch (argc) {
	case 1:
		cout << "Server: ";
		cin >> Server;
		cout << "UserName: ";
		cin >> UserName;
		iResult = getchar(); // 过滤换行符
		break;
	case 2:
		strcpy_s(Server, argv[1]);
		cout << "UserName: ";
		cin >> UserName;
		iResult = getchar();
		break;
	case 3:
		strcpy_s(Server, argv[1]);
		strcpy_s(UserName, argv[2]);
		break;
	default:
		cerr << "Usage: " << argv[0] << " [ServerIP] [UserName]" << endl;
		return 1;
	}

	// 初始化WSADATA
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		cout << "WSAStartup failed with Code: " << iResult << endl;
		WSACleanup();
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	
	
	iResult = getaddrinfo(Server, SERVER_PORT, &hints, &results);
	if (iResult != 0) {
		cout << "getaddrinfo failed with Code: " << iResult << endl;
		WSACleanup();
		return 1;
	}
	if (results == NULL) {
		cout << "Server " << argv[1] << " could not be resolved!" << endl;
		WSACleanup();
		return 1;
	}
	
	// 连接到服务器
	ptr = results;
	while (ptr) {
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol); // 创建服务器Socket
		if (ConnectSocket == INVALID_SOCKET) {
			cout << "socket failed with Code: " << WSAGetLastError() << endl;
			WSACleanup();
			return 1;
		}
		iResult = getnameinfo(ptr->ai_addr, (socklen_t)ptr->ai_addrlen, hoststr, NI_MAXHOST, servstr, NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV);
		if (iResult != 0) {
			cout << "getnameinfo failed with Code: " << iResult << endl;
			WSACleanup();
			return 1;
		}
		cout << "Client attempting connection to " << hoststr << " port " << servstr << endl;

		iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen); // 连接到服务器
		if (iResult == SOCKET_ERROR) {
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
			ptr = ptr->ai_next;
		}
		else {
			break;
		}
	}

	freeaddrinfo(results); // 连接后可释放results
	results = NULL;

	if (ConnectSocket == INVALID_SOCKET) {
		cout << "connect failed with Code: " << WSAGetLastError() << endl;
		WSACleanup();
		return 1;
	}
	else {
		cout << "Successfully connected to Server!" << endl;
	}

	// 发送试探信息
	sprintf_s(Buffer, UserName);
	iResult = send(ConnectSocket, Buffer, lstrlenA(Buffer) + 1, 0);
	if (iResult == SOCKET_ERROR) {
		cout << "send failed with Code: " << WSAGetLastError() << endl;
		closesocket(ConnectSocket);
		WSACleanup();
		return 1;
	}
	iResult = recv(ConnectSocket, Buffer, DEFAULT_BUFLEN, 0);
	if (iResult == SOCKET_ERROR) {
		cout << "recv failed with Code: " << WSAGetLastError() << endl;
		closesocket(ConnectSocket);
		WSACleanup();
		return 1;
	}
	if (iResult == 0) {
		cout << "Server closed connection" << endl;
		return 0;
	}

	cout << "\nHello " << UserName << ". Client is ready now!!!\n" << endl;

	// 建立收信线程
	hChatThread[0] = CreateThread(NULL, 0, Receiver, &ConnectSocket, 0, NULL);
	if (hChatThread[0] == NULL) {
		cerr << "CreateThread for Receiver failed with Code: " << GetLastError() << endl;
		closesocket(ConnectSocket);
		WSACleanup();
		return 1;
	}
	//建立送信线程
	hChatThread[1] = CreateThread(NULL, 0, Sender, &ConnectSocket, 0, NULL);
	if (hChatThread[1] == NULL) {
		cerr << "CreateThread for Sender failed with Code: " << GetLastError() << endl;
		closesocket(ConnectSocket);
		WSACleanup();
		return 1;
	}
	
	iResult = WaitForMultipleObjects(2, hChatThread, TRUE, INFINITE); // 等待所有线程结束，设置超时时间为无线
																	  // 注意是INT的INFINITE而不是float的INFINITY
	if ((iResult == WAIT_FAILED) || (iResult == WAIT_TIMEOUT)) {
		cerr << "WaitForMultipleObjects failed with Code: " << GetLastError() << endl;
		WSACleanup();
		return 1;
	}

	closesocket(ConnectSocket);
	WSACleanup();
	return 0;
}
