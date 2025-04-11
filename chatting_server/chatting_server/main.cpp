#include <iostream>
#include <string>
#include <vector>
#include <WinSock2.h>
#include <thread>
#include <mysql/jdbc.h>

#pragma comment(lib, "ws2_32.lib")

using namespace sql;
using namespace sql::mysql;

const std::string DB_HOST = "tcp://127.0.0.1:3306";
const std::string DB_USER = "root";
const std::string DB_PASS = "1234"; // 비밀번호 사용
const std::string DB_NAME = "chatting"; // 테이블 이름 사용

void handleClient(SOCKET clientSocket) {
	std::vector<char> buffer(1024);
	bool keepConnection = true;

	while (keepConnection) {
		int recvLen = recv(clientSocket, buffer.data(), buffer.size(), 0);

		if (recvLen <= 0) {
			std::cout << "CLIENT DISCONNECTED" << std::endl;
			break;
		}

		std::string msg(buffer.data(), recvLen);
		std::cout << "[RECV] " << msg << std::endl;

		if (msg.rfind("EXIT:", 0) == 0) {
			keepConnection = false;
			std::string goodbye = "Goodbye!";
			send(clientSocket, goodbye.c_str(), goodbye.length(), 0);
			continue;
		}


		if (msg.rfind("REGISTER:", 0) == 0) {
			size_t pos1 = msg.find(":", 9);
			size_t pos2 = msg.find(":", pos1 + 1);

			if (pos1 == std::string::npos || pos2 == std::string::npos) {
				std::string error = "INVALID FORMAT";
				send(clientSocket, error.c_str(), error.length(), 0);
				closesocket(clientSocket);
				return;
			}

			std::string username = msg.substr(9, pos1 - 9);
			std::string password = msg.substr(pos1 + 1, pos2 - (pos1 + 1));

			try {
				MySQL_Driver* driver = get_mysql_driver_instance();
				std::unique_ptr<Connection> conn(
					driver->connect(DB_HOST, DB_USER, DB_PASS)
				);
				conn->setSchema(DB_NAME);

				std::unique_ptr<PreparedStatement> checkStmt(
					conn->prepareStatement("SELECT username FROM users WHERE username=?")
				);
				checkStmt->setString(1, username);

				std::unique_ptr<ResultSet> res1(checkStmt->executeQuery());
				if (res1->next()) {
					std::string exists = "USERNAME EXISTS";
					send(clientSocket, exists.c_str(), exists.length(), 0);
					return;
				}

				std::unique_ptr<PreparedStatement> pstmt(
					conn->prepareStatement("INSERT INTO users (username, password) VALUES(?, ?)")
				);

				pstmt->setString(1, username);
				pstmt->setString(2, password);

				int res2 = pstmt->executeUpdate();

				std::string result = res2 > 0 ? "REGISTER SUCCESSED" : "REGISTER FAILED";
				send(clientSocket, result.c_str(), result.length(), 0);
			}
			catch (SQLException& e) {
				std::cerr << "MySQL Error : " << e.what() << std::endl;
				std::string err = "DB Error";
				send(clientSocket, err.c_str(), err.length(), 0);
			}
		}

		else if (msg.rfind("LOGIN:", 0) == 0) {
			size_t pos1 = msg.find(":", 6);
			size_t pos2 = msg.find(":", pos1 + 1);
			size_t pos3 = msg.find(":", pos2 + 1);


			if (pos1 == std::string::npos || pos2 == std::string::npos || pos3 == std::string::npos) {
				std::string error = "INVALID FORMAT";
				send(clientSocket, error.c_str(), error.length(), 0);
				closesocket(clientSocket);
				return;
			}

			std::string username = msg.substr(6, pos1 - 6);
			std::string password = msg.substr(pos1 + 1, pos2 - (pos1 + 1));
			std::string ip = msg.substr(pos2 + 1, pos3 - (pos2 + 1));

			try {
				MySQL_Driver* driver = get_mysql_driver_instance();
				std::unique_ptr<Connection> conn(
					driver->connect(DB_HOST, DB_USER, DB_PASS)
				);
				conn->setSchema(DB_NAME);

				std::unique_ptr<PreparedStatement> pstmt(
					conn->prepareStatement("SELECT user_id FROM users WHERE username=? AND password=?")
				);

				pstmt->setString(1, username);
				pstmt->setString(2, password);
				std::unique_ptr<ResultSet> res(pstmt->executeQuery());

				bool hasResult = res->next();
				if (hasResult) {
					int user_id = res->getInt("user_id");
					std::unique_ptr<PreparedStatement> loginLog(
						conn->prepareStatement("INSERT INTO user_sessions (user_id, ip_address) VALUES(?, ?)")
					);

					loginLog->setInt(1, user_id);
					loginLog->setString(2, ip);

					loginLog->executeUpdate();
				}

				std::string result = hasResult ? "LOGIN SUCCESSED" : "LOGIN FAILED";

				send(clientSocket, result.c_str(), result.length(), 0);
			}
			catch (SQLException& e) {
				std::cerr << "MySQL Error : " << e.what() << std::endl;
				std::string err = "DB Error";
				send(clientSocket, err.c_str(), err.length(), 0);
			}
		}
		else if (msg.rfind("CHAT:", 0) == 0) {
			size_t pos1 = msg.find(":", 5);
			size_t pos2 = msg.find(":", pos1 + 1);

			if (pos1 == std::string::npos || pos2 == std::string::npos) {
				std::string error = "INVALID FORMAT";
				send(clientSocket, error.c_str(), error.length(), 0);
				return;
			}

			std::string username = msg.substr(5, pos1 - 5);
			std::string chat = msg.substr(pos1 + 1, pos2 - (pos1 + 1));

			try {
				MySQL_Driver* driver = get_mysql_driver_instance();
				std::unique_ptr<Connection> conn(
					driver->connect(DB_HOST, DB_USER, DB_PASS)
				);
				conn->setSchema(DB_NAME);

				// 사용자 ID 조회
				std::unique_ptr<PreparedStatement> pstmt(
					conn->prepareStatement("SELECT user_id FROM users WHERE username=?")
				);
				pstmt->setString(1, username);
				std::unique_ptr<ResultSet> res(pstmt->executeQuery());

				if (res->next()) {
					int user_id = res->getInt("user_id");

					// 메시지를 DB에 저장
					std::unique_ptr<PreparedStatement> messageLog(
						conn->prepareStatement("INSERT INTO message_log (sender_id, content) VALUES(?, ?)")
					);
					messageLog->setInt(1, user_id);
					messageLog->setString(2, chat);
					messageLog->executeUpdate();

					// 단일 응답 메시지 전송
					std::string successMsg = "MESSAGE SAVED : " + chat;
					send(clientSocket, successMsg.c_str(), successMsg.length(), 0);
				}
				else {
					std::string errorMsg = "USER NOT FOUND";
					send(clientSocket, errorMsg.c_str(), errorMsg.length(), 0);
				}
			}
			catch (SQLException& e) {
				std::cerr << "MySQL Error: " << e.what() << std::endl;
				std::string err = "DB Error: " + std::string(e.what());
				send(clientSocket, err.c_str(), err.length(), 0);
			}
		}
		else if (msg.rfind("LOGOUT:", 0) == 0) {
			size_t pos1 = msg.find(":", 7);

			if (pos1 == std::string::npos) {
				std::string error = "INVALID FORMAT";
				send(clientSocket, error.c_str(), error.length(), 0);
				return;
			}

			std::string username = msg.substr(7, pos1 - 7);

			try {
				MySQL_Driver* driver = get_mysql_driver_instance();
				std::unique_ptr<Connection> conn(
					driver->connect(DB_HOST, DB_USER, DB_PASS)
				);
				conn->setSchema(DB_NAME);

				std::unique_ptr<PreparedStatement> userStmt(
					conn->prepareStatement("SELECT user_id FROM users WHERE username = ?")
				);
				userStmt->setString(1, username);
				std::unique_ptr<ResultSet> res(userStmt->executeQuery());

				if (res->next()) {
					int user_id = res->getInt("user_id");

					// 2. 해당 사용자의 가장 최근 로그인 세션 중 로그아웃 시간이 NULL인 세션 찾기
					std::unique_ptr<PreparedStatement> updateStmt(
						conn->prepareStatement(
							"UPDATE user_sessions SET logout_time = CURRENT_TIMESTAMP "
							"WHERE user_id = ? AND logout_time IS NULL "
							"ORDER BY login_time DESC LIMIT 1"
						)
					);
					updateStmt->setInt(1, user_id);

					int updateCount = updateStmt->executeUpdate();

					if (updateCount > 0) {
						std::string result = "LOGOUT_SUCCESS";
						send(clientSocket, result.c_str(), result.length(), 0);
					}
				}
			}
			catch (SQLException& e) {
				std::cerr << "MySQL Error: " << e.what() << std::endl;
				std::string err = "DB Error";
				send(clientSocket, err.c_str(), err.length(), 0);
			}
		}
	}
	closesocket(clientSocket);
}

int main() {
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cerr << "WSAStartup FAILED" << std::endl;
		return 1;
	}

	SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocket == INVALID_SOCKET) {
		std::cerr << "Socket FAILED" << std::endl;
		WSACleanup();
		return 1;
	}

	sockaddr_in serverAddr, clientAddr;
	int clientSize = sizeof(clientAddr);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(8888);
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	
	if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		std::cerr << "BINDING FAILED" << WSAGetLastError() << std::endl;
		closesocket(serverSocket);
		WSACleanup();
		return 1;
	}

	listen(serverSocket, SOMAXCONN);
	std::cout << "Server Listen in " << ntohs(serverAddr.sin_port) << std::endl;

	while (true) {
		SOCKET clientSocket = accept(serverSocket, (SOCKADDR*)&clientAddr, &clientSize);

		if (clientSocket != INVALID_SOCKET) {
			std::thread clientThread(handleClient, clientSocket);
			clientThread.detach();
		}
	}

	closesocket(serverSocket);
	WSACleanup();

	return 0;
}