#define  _WINSOCK_DEPRECATED_NO_WARNINGS
//
#include <WinSock2.h>
#include <iostream>
#include <string>
#include <Windows.h>
#include <vector>
#include <process.h>
#include <memory>
#include <format>
//
#include "jdbc/mysql_connection.h"
#include "jdbc/cppconn/driver.h"
#include "jdbc/cppconn/exception.h"
#include "jdbc/cppconn/prepared_statement.h"
//
using namespace std;
#pragma comment (lib, "WS2_32.lib")
#pragma comment (lib, "debug/mysqlcppconn.lib")
//
#define SERVER_IPV4 "0.0.0.0"
#define SERVER_PORT 5959
#define PACKET_SIZE 100
#define PACKET_ID 20
#define PACKET_PWD 25


//

//
const string server = "tcp://127.0.0.1:3306";
const string username = "root";
const string password = "1234";

vector<SOCKET> userlist;
CRITICAL_SECTION ServerCS;

// sql ����
sql::Driver* driver = nullptr;
sql::Connection* con = nullptr;
sql::Statement* stmt = nullptr;
sql::PreparedStatement* pstmt = nullptr;
sql::ResultSet* rs = nullptr;

bool bProgramRunning = true;
bool bNetworkConnected = false;

// �𸮾� -> cpp �ѱ�
std::string Utf8ToMultiByte(std::string utf8_str)
{
	std::string resultString; char* pszIn = new char[utf8_str.length() + 1];
	strncpy_s(pszIn, utf8_str.length() + 1, utf8_str.c_str(), utf8_str.length());
	int nLenOfUni = 0, nLenOfANSI = 0; wchar_t* uni_wchar = NULL;
	char* pszOut = NULL;
	// 1. utf8 Length
	if ((nLenOfUni = MultiByteToWideChar(CP_UTF8, 0, pszIn, (int)strlen(pszIn), NULL, 0)) <= 0)
		return nullptr;
	uni_wchar = new wchar_t[nLenOfUni + 1];
	memset(uni_wchar, 0x00, sizeof(wchar_t) * (nLenOfUni + 1));
	// 2. utf8 --> unicode
	nLenOfUni = MultiByteToWideChar(CP_UTF8, 0, pszIn, (int)strlen(pszIn), uni_wchar, nLenOfUni);
	// 3. ANSI(multibyte) Length
	if ((nLenOfANSI = WideCharToMultiByte(CP_ACP, 0, uni_wchar, nLenOfUni, NULL, 0, NULL, NULL)) <= 0)
	{
		delete[] uni_wchar; return 0;
	}
	pszOut = new char[nLenOfANSI + 1];
	memset(pszOut, 0x00, sizeof(char) * (nLenOfANSI + 1));
	// 4. unicode --> ANSI(multibyte)
	nLenOfANSI = WideCharToMultiByte(CP_ACP, 0, uni_wchar, nLenOfUni, pszOut, nLenOfANSI, NULL, NULL);
	pszOut[nLenOfANSI] = 0;
	resultString = pszOut;
	delete[] uni_wchar;
	delete[] pszOut;
	return resultString;
}
// cpp -> �𸮾� 
std::string MultiByteToUtf8(std::string multibyte_str)
{
	char* pszIn = new char[multibyte_str.length() + 1];
	strncpy_s(pszIn, multibyte_str.length() + 1, multibyte_str.c_str(), multibyte_str.length());

	std::string resultString;

	int nLenOfUni = 0, nLenOfUTF = 0;
	wchar_t* uni_wchar = NULL;
	char* pszOut = NULL;

	// 1. ANSI(multibyte) Length
	if ((nLenOfUni = MultiByteToWideChar(CP_ACP, 0, pszIn, (int)strlen(pszIn), NULL, 0)) <= 0)
		return 0;

	uni_wchar = new wchar_t[nLenOfUni + 1];
	memset(uni_wchar, 0x00, sizeof(wchar_t) * (nLenOfUni + 1));

	// 2. ANSI(multibyte) ---> unicode
	nLenOfUni = MultiByteToWideChar(CP_ACP, 0, pszIn, (int)strlen(pszIn), uni_wchar, nLenOfUni);

	// 3. utf8 Length
	if ((nLenOfUTF = WideCharToMultiByte(CP_UTF8, 0, uni_wchar, nLenOfUni, NULL, 0, NULL, NULL)) <= 0)
	{
		delete[] uni_wchar;
		return 0;
	}

	pszOut = new char[nLenOfUTF + 1];
	memset(pszOut, 0, sizeof(char) * (nLenOfUTF + 1));

	// 4. unicode ---> utf8
	nLenOfUTF = WideCharToMultiByte(CP_UTF8, 0, uni_wchar, nLenOfUni, pszOut, nLenOfUTF, NULL, NULL);
	pszOut[nLenOfUTF] = 0;
	resultString = pszOut;

	delete[] uni_wchar;
	delete[] pszOut;

	return resultString;
}

unsigned WINAPI Chatting(void* arg)
{

	SOCKET ClientSocket = *(SOCKET*)arg;

	
	char SendBuffer[PACKET_SIZE] = { 0, };
	char RecvID[PACKET_SIZE] = { 0, };
	char RecvPWD[PACKET_SIZE] = { 0, };
	char Initial_Array[PACKET_SIZE] = { 0, };
	char RecvNameBuffer[PACKET_SIZE] = { 0, };

	int RecvIDBytes = 0;
	int RecvPWDBytes = 0;
	int SendBytes = 0;
	int RecvUserName = 0;

	
	SOCKET LogoutSocket;
	//�α���
	while (true)
	{
		RecvIDBytes = recv(ClientSocket, RecvID, sizeof(RecvID), 0);
		

		RecvPWDBytes = recv(ClientSocket, RecvPWD, sizeof(RecvPWD), 0);
		
		
		string sID = RecvID;
		char ID_Buffer[100];
		//memcpy(ID_Buffer, &sID, 100);
		string sPWD = RecvPWD;
		//cout << "ID : " << sID << endl;
		//cout << "ID : " << (int)ID_Buffer[0] << endl;

		//char test[33] = { 0 ,};

		if (RecvIDBytes > 0 && RecvPWDBytes > 0) 
		{
			if (sID[0] == '0') 
			{
				
				sID.erase(sID.begin());
				pstmt = con->prepareStatement("select * from memberinpo where memberid = ? and memberpwd = ?");
				pstmt->setString(1, sID);
				pstmt->setString(2, sPWD);
				pstmt->execute();

				sql::ResultSet* RS = pstmt->executeQuery();

				if (RS->rowsCount() > 0)
				{
					cout << "����" << endl;
					send(ClientSocket, "1", 1, 0);
				}
				else
				{
					cout << "����" << endl;
					send(ClientSocket, "2", 1, 0);
				}
				/*memcpy(&RecvID, Initial_Array, PACKET_SIZE);
				memcpy(&RecvPWD, Initial_Array, PACKET_SIZE);*/
				memset(RecvID, NULL, PACKET_SIZE);
				memset(RecvPWD, NULL, PACKET_SIZE);
			}

			if (sID[0] == '1')
			{
				
				sID.erase(sID.begin());
				pstmt = con->prepareStatement("select * from memberinpo where memberid = ?");
				pstmt->setString(1, sID);
				pstmt->execute();
				sql::ResultSet* RS = pstmt->executeQuery();
				if (RS->rowsCount() > 0)
				{

					cout << "�ߺ��� ���̵��Դϴ�" << endl;
					send(ClientSocket, "3", 1, 0);
					
				}
				else
				{
					send(ClientSocket, "4", 1, 0);
					cout << "���� �Ǿ����ϴ�" << endl;
					pstmt = con->prepareStatement("insert into memberinpo (MemberID,MemberPWD) values(?,?)");
					pstmt->setString(1, sID);
					pstmt->setString(2, sPWD);
					pstmt->execute();

				}


				memset(RecvID, NULL, PACKET_SIZE);
				memset(RecvPWD, NULL, PACKET_SIZE);
				/*memcpy(&RecvID, Initial_Array, PACKET_SIZE);
				memcpy(&RecvPWD, Initial_Array, PACKET_SIZE);*/
			}

			
		}

		
		if (RecvIDBytes <= 0)
		{
			LogoutSocket = ClientSocket;
			//string msg = format("SocketID : [%d]�� �α׾ƿ��߽��ϴ�!", ClientSocket);
			//msg = MultiByteToUtf8(msg);
			//memcpy(SendBuffer, msg.c_str(), msg.length());
			// 
			//EnterCriticalSection(&ServerCS);
			//for (int i = 0; i < userlist.size(); ++i)
			//{
			//    if (userlist[i] != ClientSocket)
			//    {
			//        const char SendLogout[PACKET_SIZE] = { "another players logout." };
			//        SendBytes = send(userlist[i], SendLogout, sizeof(SendLogout), 0);
			//        cout << SendLogout << " : " << LogoutSocket << " TO : " << userlist[i] << endl;
			//    }
			//}
			//LeaveCriticalSection(&ServerCS);
			break;
		}
		//RecvID[PACKET_SIZE-1] = '\0';

		// ���� �޽��� string ��ȯ
		string strCONTENTS = RecvID;

		string UserName = RecvNameBuffer;

		
		//if (strCONTENTS == "EXITSERVER")
		//{
		//	// ���� �޽��� EXITSERVER �϶� ����
		//	bProgramRunning = false;
		//	break;
		//}

		
	}

	EnterCriticalSection(&ServerCS);
	userlist.erase(find(userlist.begin(), userlist.end(), LogoutSocket));
	closesocket(ClientSocket);
	LeaveCriticalSection(&ServerCS);
	return bProgramRunning = true;
}

int main(int argc, char* argv[])
{
	InitializeCriticalSection(&ServerCS);

	// sql ����
	driver = get_driver_instance();
	con = driver->connect(server, username, password);
	con->setSchema("server");
	cout << "�����ͺ��̽� ���Ӽ���!" << endl;

	bool bProgramRunning = true;
	bool bNetworkConnected = false;
	// winsock ����
	WSAData WSAdata = { 0, };
	if (WSAStartup(MAKEWORD(2, 2), &WSAdata) != 0) { exit(-1); }
	SOCKET ServerSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (ServerSocket == INVALID_SOCKET) { exit(-2); }
	SOCKADDR_IN ServerSockAddr = { 0, };
	ServerSockAddr.sin_family = AF_INET; // IPV4
	ServerSockAddr.sin_addr.S_un.S_addr = inet_addr(SERVER_IPV4);
	ServerSockAddr.sin_port = htons(SERVER_PORT);
	if (::bind(ServerSocket, (SOCKADDR*)&ServerSockAddr, sizeof(ServerSockAddr)) != 0) { exit(-3); };
	if (listen(ServerSocket, SOMAXCONN) == SOCKET_ERROR) { exit(-4); };

	SOCKET ClientSocket = NULL;

	while (bProgramRunning)
	{
		// Ŭ�� ���� ����, ���
		SOCKADDR_IN ClientAddrIn = { 0, };
		int ClientLength = sizeof(ClientAddrIn);
		cout << "Ŭ���̾�Ʈ ���Ӵ��!" << endl;
		ClientSocket = accept(ServerSocket, (SOCKADDR*)&ClientAddrIn, &ClientLength);
		if (ClientSocket == INVALID_SOCKET)
		{
			closesocket(ClientSocket);
			continue;
		}
		else
		{
			// Ŭ�� ���� �Ϸ�
			cout << "Ŭ���̾�Ʈ ���ӿϷ�!!!" << endl;

			cout << "connect : " << ClientSocket << endl;
			EnterCriticalSection(&ServerCS);
			userlist.push_back(ClientSocket);
			LeaveCriticalSection(&ServerCS);
			HANDLE ThreadHandle = (HANDLE)_beginthreadex(nullptr, 0, Chatting, (void*)&ClientSocket, 0, nullptr);
		}
	}



	closesocket(ServerSocket);
	DeleteCriticalSection(&ServerCS);
	
	WSACleanup();
	return 0;
}