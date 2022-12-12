#define  _WINSOCK_DEPRECATED_NO_WARNINGS
//
#include <WinSock2.h>
#include <iostream>
#include <string>
#include <thread>
//
#include "jdbc/mysql_connection.h"
#include "jdbc/cppconn/driver.h"
#include "jdbc/cppconn/exception.h"
#include "jdbc/cppconn/prepared_statement.h"
//
#pragma comment (lib, "WS2_32.lib")
#pragma comment (lib, "debug/mysqlcppconn.lib")
//
#define SERVER_IPV4 "0.0.0.0"
#define SERVER_PORT 4949
#define PACKET_SIZE 33
//
using namespace std;
//
const string server = "tcp://127.0.0.1:3306";
const string username = "root";
const string password = "1234";


// cpp -> 언리얼 
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


// 언리얼 -> cpp
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


int main(int argc, char* argv[])
{
    // sql 세팅
    sql::Driver* driver = nullptr;
    sql::Connection* con = nullptr;
    sql::Statement* stmt = nullptr;
    sql::PreparedStatement* pstmt = nullptr;
    sql::ResultSet* rs = nullptr;
    // sql 연결
    driver = get_driver_instance();
    con = driver->connect(server, username, password);
    con->setSchema("chatting");
    cout << "데이터베이스 접속성공!" << endl;

    bool bProgramRunning = true;
    bool bNetworkConnected = false;
    // winsock 세팅
    WSAData WSAdata = { 0, };
    if (WSAStartup(MAKEWORD(2, 2), &WSAdata) != 0)
        exit(-1);

    SOCKET ServerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (ServerSocket == INVALID_SOCKET)
        exit(-2);

    SOCKADDR_IN ServerSockAddr = { 0, };

    ServerSockAddr.sin_family = AF_INET; // IPV4
    ServerSockAddr.sin_addr.S_un.S_addr = inet_addr(SERVER_IPV4);
    ServerSockAddr.sin_port = htons(SERVER_PORT);

    if (::bind(ServerSocket, (SOCKADDR*)&ServerSockAddr, sizeof(ServerSockAddr)) != 0)
    {
        exit(-3);
    }

    if (listen(ServerSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        exit(-4);
    }

    SOCKET ClientSocket = NULL;

    while (bProgramRunning)
    {
        if (!bNetworkConnected)
        {
            // 클라 접속 세팅, 대기
            SOCKADDR_IN ClientAddrIn = { 0, };
            int ClientLength = sizeof(ClientAddrIn);
            cout << "클라이언트 접속대기!" << endl;
            ClientSocket = accept(ServerSocket, (SOCKADDR*)&ClientAddrIn, &ClientLength);
            if (ClientSocket == INVALID_SOCKET)
            {
                closesocket(ClientSocket);
                continue;
            }
            else
            {
                // 클라 접속 완료
                cout << "클라이언트 접속완료!!!" << endl;
                bNetworkConnected = true;
            }
        }
        // 메시지 수신 세팅
        char Buffer[PACKET_SIZE] = { 0, };
        int RecvBytes = recv(ClientSocket, Buffer, sizeof(Buffer), 0);

        //유저네임
        char UserName[PACKET_SIZE] = { 0, };
        int RecvName = recv(ClientSocket, UserName, sizeof(UserName), 0);

        if (RecvBytes <= 0)
        {
            // 연결 끊김
            bNetworkConnected = false;
            continue;
        }
        UserName[PACKET_SIZE - 1] = '\0';
        Buffer[PACKET_SIZE - 1] = '\0';
        // 수신 메시지 string 변환
        string strCONTENTS = Utf8ToMultiByte(Buffer);
        string strUserName = Utf8ToMultiByte(UserName);

        if (strCONTENTS == "EXITSERVER")
        {
            // 수신 메시지 EXITSERVER 일때 종료
            bProgramRunning = false;
            break;
        }

        // 수신 메시지 DB 저장
        pstmt = con->prepareStatement("INSERT INTO CHAT(`CHAT`,`USERNAME`)VALUES(?,?)");
        pstmt->setString(1, MultiByteToUtf8(strCONTENTS));
        pstmt->setString(2, MultiByteToUtf8(strUserName));
        pstmt->execute();

        cout << "수신 메시지 : " << strCONTENTS << endl;

        int SendBytes = send(ClientSocket, Buffer, sizeof(Buffer), 0);
        cout << "SendBytes :" << SendBytes << endl;

    }
    closesocket(ClientSocket);
    closesocket(ServerSocket);

    WSACleanup();
    return 0;
}
