#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include<vector>
#include <string>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

bool isLoggedIn = false;
string currentUsername = "";

SOCKET connectToServer(const char* ip, int port) {
	WSADATA wsaData;
	SOCKET clientSocket;
	SOCKADDR_IN serverAddr;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		cerr << "WSAStartup Failed" << endl;
		exit(1);
	}

	clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (clientSocket == INVALID_SOCKET) {
		cerr << "SOCKET CREATE FAILED" << endl;
		WSACleanup();
		exit(1);
	}

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(8888);
	inet_pton(AF_INET, ip, &serverAddr.sin_addr);

	if (connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		cerr << "SERVER CONNECT FAILED" << endl;
		closesocket(clientSocket);
		WSACleanup();
		exit(1);
	}

	return clientSocket;
}

string sendCommand(SOCKET socket, const string& cmd) {
	send(socket, cmd.c_str(), cmd.length(), 0);

	vector<char> buffer(1024);

	int byteReceived = recv(socket, buffer.data(), static_cast<int>(buffer.size()), 0);


	if (byteReceived > 0) {
		string response(buffer.data(), byteReceived);
		return response;
	}

	return "RESPONSE FAILED";
}

void sendMessage(SOCKET socket) {
	if (!isLoggedIn) {
		cout << "[ERROR] LOGIN REQUIRED!" << endl;
		return;
	}

	vector<char> buffer(1024);
	string chatMessage;

	cout << "ENTER 'exit' TO RETURN TO MENU" << endl;

	while (true) {
		cout << "[" << currentUsername << "]: ";
		getline(cin, chatMessage);

		if (chatMessage == "exit") {
			break;
		}

		// 메시지 형식 구성: CHAT:username:message: (공백 제거)
		string fullMessage = "CHAT:" + currentUsername + ":" + chatMessage + ":";

		//cout << "Sending: " << fullMessage << endl; // 디버깅 정보 추가

		// 서버로 메시지 전송
		int sendResult = send(socket, fullMessage.c_str(), fullMessage.length(), 0);
		if (sendResult == SOCKET_ERROR) {
			int error = WSAGetLastError();
			cout << "[ERROR] FAIL TO SEND MESSAGE : " << error << endl;

			// 심각한 오류인 경우 루프 종료
			if (error == WSAECONNRESET || error == WSAECONNABORTED) {
				cout << "[ERROR] CONNECTION TO SERVER LOST" << endl;
				return;
			}
			continue; // 다른 오류는 계속 시도
		}

		// 서버 응답 수신 - 타임아웃 설정 추가
		fd_set readSet;
		FD_ZERO(&readSet);
		FD_SET(socket, &readSet);

		struct timeval timeout;
		timeout.tv_sec = 5;  // 5초 타임아웃
		timeout.tv_usec = 0;

		if (select(socket + 1, &readSet, NULL, NULL, &timeout) > 0) {
			int recvLen = recv(socket, buffer.data(), buffer.size(), 0);
			if (recvLen > 0) {
				buffer[recvLen] = '\0';
				cout << "[SERVER RESPONSE] : " << buffer.data() << endl;
			}
			else {
				int error = WSAGetLastError();
				cout << "[ERROR] : " << error << endl;
				return;
			}
		}
		else {
			cout << "[ERROR] RESPONSE TIMEOUT" << endl;
		}
	}
}


void handleRegister(SOCKET socket) {
	string username, password;
	cout << "[New username] : ";
	getline(cin, username);
	cout << "[New password] : ";
	getline(cin, password);

	string message = "REGISTER:" + username + ":" + password + ":";
	string response = sendCommand(socket, message);

	cout << "[SERVER RESPONSE] : " << response << endl;
}

void handleLogin(SOCKET socket, bool& isLoggedInFlag, string ip) {
	string username, password;
	cout << "[username] : ";
	getline(cin, username);
	cout << "[password] : ";
	getline(cin, password);

	string message = "LOGIN:" + username + ":" + password + ":" + ip + ":";
	string response = sendCommand(socket, message);
	cout << "[SERVER RESPONSE] : " << response << endl;

	if (response == "LOGIN SUCCESSED") {
		isLoggedInFlag = true;
		currentUsername = username;
		cout << "LOGGED IN [" << currentUsername << "]" << endl;
	}
	else {
		isLoggedInFlag = false;
		currentUsername = "";
		cout << "[ERROR] LOGIN FAILED" << endl;
	}
}

void handleChat(SOCKET socket) {
	if (!isLoggedIn) {
		cout << "[ERROR] LOGIN REQUIRED!" << endl;
		return;
	}
	sendMessage(socket);
}

void handleLogout(SOCKET socket, bool& isLoggedInFlag) {
	if (!isLoggedIn) {
		cout << "[ERROR] LOGIN REQUIRED!" << endl;
		return;
	}

	cout << "LOGGED OUT [" << currentUsername << "]" << endl;
	string message = "LOGOUT:" + currentUsername + ":";
	string response = sendCommand(socket, message);

	if (response == "LOGOUT_SUCCESS") {
		isLoggedInFlag = false;
		currentUsername = "";
	}
}

void showMenu1() {
	cout << "===== MENU =====" << endl
		<< "1. Member Register" << endl
		<< "2. Log_In" << endl
		<< "3. Exit" << endl
		<< "=================" << endl
		<< "Choice : ";
}

void showMenu2() {
	cout << "===== MENU =====" << endl
		<< "1. Chatting" << endl
		<< "2. Log_Out" << endl
		<< "=================" << endl
		<< "Choice : ";
}

string getLocalIPAddress() {
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		cerr << "WSAStartup Failed" << endl;
		return "127.0.0.1"; // 실패 시 localhost 반환
	}

	char hostName[256];
	if (gethostname(hostName, sizeof(hostName)) != 0) {
		cerr << "Failed to get hostname" << endl;
		WSACleanup();
		return "127.0.0.1";
	}

	struct addrinfo hints, * result = NULL;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET; // IPv4
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	if (getaddrinfo(hostName, NULL, &hints, &result) != 0) {
		cerr << "Failed to get address info" << endl;
		WSACleanup();
		return "127.0.0.1";
	}

	char ipAddress[INET_ADDRSTRLEN];
	for (struct addrinfo* ptr = result; ptr != NULL; ptr = ptr->ai_next) {
		struct sockaddr_in* sockaddr_ipv4 = (struct sockaddr_in*)ptr->ai_addr;
		inet_ntop(AF_INET, &sockaddr_ipv4->sin_addr, ipAddress, sizeof(ipAddress));

		// 루프백 주소(127.0.0.1)가 아닌 주소 찾기
		if (string(ipAddress) != "127.0.0.1") {
			freeaddrinfo(result);
			WSACleanup();
			return string(ipAddress);
		}
	}

	freeaddrinfo(result);
	WSACleanup();
	return "127.0.0.1"; // 적절한 IP를 찾지 못한 경우
}

int main() {
	string ip;
	bool exitProgram = false;
	ip = getLocalIPAddress();
	SOCKET clientSocket = connectToServer(ip.c_str(), 8888);

	cout << "CONNECTED TO SERVER" << endl;

	int choice;

	do {
		if (currentUsername == "") {
			showMenu1();
			cin >> choice;
			cin.ignore();
			
			switch (choice) {
			case 1:
				handleRegister(clientSocket);
				break;
			case 2:
				handleLogin(clientSocket, isLoggedIn, ip);
				break;
			case 3:
				if (currentUsername != "") {
					cout << "YOU MUST LOGOUT FIRST" << endl;
					continue;
				}
				exitProgram = true;
				sendCommand(clientSocket, "EXIT:");
				break;
			default:
				cout << "INVALID NUMBER" << endl;
				continue;
			}
		}
		else {
			showMenu2();
			cin >> choice;
			cin.ignore();

			switch (choice) {
			case 1:
				handleChat(clientSocket);
				break;
			case 2:
				handleLogout(clientSocket, isLoggedIn);
				break;
			default:
				cout << "INVALID NUMBER" << endl;
				continue;
			}
		}

	} while (!exitProgram);

	

	closesocket(clientSocket);
	WSACleanup();

	return 0;
}