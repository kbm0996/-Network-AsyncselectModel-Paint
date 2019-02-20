#include "stdafx.h"
#include "Server.h"

// TODO: 사용자 정의 메세지 영역
// 0 ~ WM_USER -1 : 시스템에서 사용하기 위해 예약된 메시지
// WM_USER ~ 0x7FFF : 임의로 사용할 수 있는 메시지
#define UM_NETWORK (WM_USER+1)
#define df_SERVER_PORT 25000
#define df_BUFFER_SIZE 1000

using namespace mylib;

// 클라이언트 구조체
struct stCLIENT
{
	SOCKET Socket;
	CRingBuffer* RecvQ;
	CRingBuffer* SendQ;
	WCHAR szIP[INET_ADDRSTRLEN];
};

list<stCLIENT*> g_Clientlist;

// 클라이언트 관리 함수
stCLIENT* AddClient(SOCKET sock, WCHAR* szIP);
void RemoveClient(SOCKET sock);
void RemoveClient(list<stCLIENT*>::iterator iter);

// 네트워크 함수
SOCKET g_ListenSocket;
BOOL Start(HWND hWnd);
void Stop();
void ProcRecv(SOCKET sock);
void ProcSend(SOCKET sock);
void SendBroadcast(char* pBuffer, int iSize);

//////////////////////////////////////////////////////////////////////////////////
// TODO: 다이얼로그 프로시저 콜백 함수
// :: INT_PTR DlgProc(HWND hWnd, UINT iMsg, WPARAM wordParam, LPARAM longParam)
// https://msdn.microsoft.com/en-us/library/ms645469(v=VS.85).aspx
//
// Parameter : HWND(대화상자 핸들), UINT(메세지), WPARAM, LPARAM
// Return : 메세지 처리 시 TRUE, 반대의 경우 FALSE
//////////////////////////////////////////////////////////////////////////////////
// TODO: 콜백(Callback) 함수 
//  다른 함수의 인자로서 이용되는 함수. **어떤 이벤트에 의해 호출**되어지는 함수
// TODO: 함수호출규약 CALLBACK
//  callee(호출된 함수)가 자신의 stack frame 공간을 직접 정리. 어셈코드는 ret(== _stdcall)
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,  _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR    lpCmdLine, _In_ int       nCmdShow)
{
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, WndProc);

    MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

    return (int) msg.wParam;
}



LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// ListBox 핸들 얻기
	HWND	hListBox = GetDlgItem(hWnd, IDC_LIST1);
    switch (message)
    {
	// WM_INITDIALOG : 대화 상자가 표시되기 바로 전에 대화 상자 프로시저로 보내지는 메세지
	case WM_INITDIALOG:
	{
		if (Start(hWnd) != TRUE)
		{
			Stop();
			exit(-1);
			break;
		}

		SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)L"서버 초기화 성공");
		return TRUE;
	}

	// TODO : 윈도우 메세지 수신 시 주의점
	// 윈도우 메시지 수신 시 적절한 소켓 함수를 호출하지 않으면, 윈도우 메시지가 다시 발생하지 않음
	//ex) FD_READ 이벤트에 대응하여 recv()를 호출하지 않으면 동일한 소켓에 대한 FD_READ 이벤트는 다시 발생하지 않음

	case UM_NETWORK:
		// TODO: WSAAsyncSelect()가 생성하는 메세지의 LPARAM 값
		//  WSAGETSELECTERROR(LPARAM) : LPARAM의 상위 16비트. 에러 코드 == HIWORD(LPARAM)
		//  WSAGETSELECTEVENT(LPARAM) : LPARAM의 하위 16비트. WSAAsyncSelect() 마지막 인자에서 지정한 이벤트 중 발생한 이벤트 == LOWORD(LPARAM)
		switch (WSAGETSELECTEVENT(lParam))
		{
		case FD_ACCEPT:
		{
			int err;
			SOCKADDR_IN clientAddr;
			int addrlen = sizeof(clientAddr);

			// accept() 함수가 리턴하는 소켓은 연결 대기 소켓과 동일한 속성을 지님
			// → 연결 대기 소켓은 직접 데이터 송수신을 하지 않으므로 FD_READ, FD_WRITE 이벤트를 처리하지 않음
			SOCKET clientSocket = accept(g_ListenSocket, (SOCKADDR*)&clientAddr, &addrlen);
			if (clientSocket == INVALID_SOCKET)
			{
				err = WSAGetLastError();
				if (err != WSAEWOULDBLOCK)
				{
					closesocket(clientSocket);
					break;
				}
			}

			// accept() 함수가 리턴하는 소켓은 이벤트 처리를 해야하기 때문에 WSAAsyncSelect() 함수를 다시 호출하여 속성 변경이 필요함
			if (WSAAsyncSelect(clientSocket, hWnd, UM_NETWORK, FD_READ | FD_WRITE | FD_CLOSE) == SOCKET_ERROR)
			{
				err = WSAGetLastError();
				if (err != WSAEWOULDBLOCK)
				{
					closesocket(clientSocket);
					break;
				}
			}

			WCHAR szIP[INET_ADDRSTRLEN];
			DWORD dwIPlen = sizeof(szIP);
			WSAAddressToString((sockaddr*)&clientAddr, sizeof(SOCKADDR_IN), NULL, szIP, &dwIPlen);

			AddClient(clientSocket, szIP);

			WCHAR szComment[INET_ADDRSTRLEN + 10] = L"접속 : ";
			wcscat_s(szComment, szIP);
			SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)szComment);
			break;
		}
		case FD_READ:
			// TODO: WSAAsyncSelect()가 생성하는 메세지의 WPARAM 값
			//  현재 네트워크 이벤트가 발생된 소켓
			ProcRecv(wParam);
			ProcSend(wParam);
			break;

		case FD_WRITE:
			ProcSend(wParam);
			break;
		case FD_CLOSE:
			WCHAR szIP[INET_ADDRSTRLEN];
			for (list<stCLIENT*>::iterator iter = g_Clientlist.begin(); iter != g_Clientlist.end(); ++iter)
			{
				if ((*iter)->Socket == wParam)
				{
					wcscpy_s(szIP, (*iter)->szIP);
					RemoveClient(iter);
					break;
				}
			}
			WCHAR szComment[INET_ADDRSTRLEN + 10] = L"퇴장 : ";
			wcscat_s(szComment, szIP);
			SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)szComment);
			break;
		}
		break;
	case WM_COMMAND:
		switch (wParam)
		{
		case IDCANCEL:
			Stop();
			EndDialog(hWnd, 99939);
			return TRUE;
		}
		break;

	case WM_DESTROY:
		Stop();
		PostQuitMessage(0);
		break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}



stCLIENT* AddClient(SOCKET sock, WCHAR* szIP)
{
	stCLIENT* p = new stCLIENT;
	p->Socket = sock;
	p->RecvQ = new CRingBuffer();
	p->SendQ = new CRingBuffer();
	wcscpy_s(p->szIP, szIP);
	g_Clientlist.push_back(p);

	return p;
}

void RemoveClient(SOCKET sock)
{
	for (list<stCLIENT*>::iterator iter = g_Clientlist.begin(); iter != g_Clientlist.end(); ++iter)
	{
		if ((*iter)->Socket == sock)
		{
			delete (*iter)->RecvQ;
			delete (*iter)->SendQ;
			shutdown(sock, SD_BOTH);
			g_Clientlist.erase(iter);
			return;
		}
	}
}

void RemoveClient(list<stCLIENT*>::iterator iter)
{
	delete (*iter)->RecvQ;
	delete (*iter)->SendQ;
	shutdown((*iter)->Socket, SD_BOTH);
	g_Clientlist.erase(iter);
}

BOOL Start(HWND hWnd)
{
	int err;
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		err = WSAGetLastError();
		return FALSE;
	}

	g_ListenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (g_ListenSocket == INVALID_SOCKET)
	{
		err = WSAGetLastError();
		return FALSE;
	}

	// Nagle 알고리즘 끄기
	BOOL bEnable = TRUE;
	setsockopt(g_ListenSocket, IPPROTO_TCP, TCP_NODELAY, (char *)&bEnable, sizeof(bEnable));

	SOCKADDR_IN serveraddr;
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(df_SERVER_PORT);
	//serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	InetPton(AF_INET, L"0.0.0.0", &serveraddr.sin_addr);
	if (bind(g_ListenSocket, (SOCKADDR *)&serveraddr, sizeof(serveraddr)) == SOCKET_ERROR)
	{
		err = WSAGetLastError();
		return FALSE;
	}

	//////////////////////////////////////////////////////////////////////////////////
	// TODO: WSAAsyncSelect(s, hWnd, wMsg, lEvent)
	//
	// s - 이벤트 통지가 필요한 소켓 디스크립터
	// hWnd - 네트워크 이벤트가 발생할 때 메시지를 받을 윈도우 핸들
	// wMsg - 네트워크 이벤트가 발생할 때 수신할 메시지. 
	//		 추가 정보(LPARAM : 상위 16비트 = 오류코드, 하위 16비트 = 4번째 인자에서 지정한이벤트 / WPARAM : 이벤트 발생 소켓)도 함께 전달
	// lEvent - 네트워크 이벤트 (비트 마스크) 
	//		 FD_READ, FD_WRITE, FD_OOB, FD_ACCEPT, FD_CONNECT, FD_CLOSE, FD_QOS, FD_GROUP_QOS, FD_ROUTING_INTERFACE_CHANGE, FD_ADDRESS_LIST_CHANGE
	//////////////////////////////////////////////////////////////////////////////////
	//  해당 함수 호출 시 해당 소켓은 자동으로 Non-blocking 상태로 전환
	// (blocking 소켓은 윈도우 메시지 루프를 정지시키기 때문)
	if (WSAAsyncSelect(g_ListenSocket, hWnd, UM_NETWORK, FD_ACCEPT | FD_READ | FD_WRITE | FD_CLOSE) == SOCKET_ERROR)
	{
		err = WSAGetLastError();
		return FALSE;
	}

	if (listen(g_ListenSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		err = WSAGetLastError();
		return FALSE;
	}

	return TRUE;
}

void Stop()
{
	closesocket(g_ListenSocket);
	WSACleanup();
}

void ProcRecv(SOCKET sock)
{
	char Buffer[df_BUFFER_SIZE];
	for (list<stCLIENT*>::iterator iter = g_Clientlist.begin(); iter != g_Clientlist.end(); ++iter)
	{
		if ((*iter)->Socket == sock)
		{
			int retval = recv(sock, Buffer, (*iter)->RecvQ->GetFreeSize(), 0);
			if (retval == 0)
				return;

			if (retval == SOCKET_ERROR)
			{
				int err = WSAGetLastError();
				if (err != WSAEWOULDBLOCK)
				{
					RemoveClient(iter);
					return;
				}
			}
			if ((*iter)->RecvQ->GetFreeSize() <= 0)
				return;

			(*iter)->RecvQ->Enqueue(Buffer, retval);
			return;
		}
	}
}

void ProcSend(SOCKET sock)
{
	char Buffer[df_BUFFER_SIZE];
	for (list<stCLIENT*>::iterator iter = g_Clientlist.begin(); iter != g_Clientlist.end(); ++iter)
	{
		if ((*iter)->Socket == sock)
		{
			if ((*iter)->RecvQ->GetUseSize() <= 0)
				return;

			int retval = (*iter)->RecvQ->Peek(Buffer, (*iter)->RecvQ->GetUseSize());
			SendBroadcast(Buffer, (*iter)->RecvQ->GetUseSize());
			(*iter)->RecvQ->MoveReadPos(retval);

			return;
		}
	}
}

void SendBroadcast(char *pBuffer, int iSize)
{
	for (list<stCLIENT*>::iterator iter = g_Clientlist.begin(); iter != g_Clientlist.end(); ++iter)
	{
		int retval = send((*iter)->Socket, pBuffer, iSize, 0);
		if (retval == SOCKET_ERROR)
		{
			int err = WSAGetLastError();
			if (err != WSAEWOULDBLOCK)
			{
				RemoveClient(iter);
				return;
			}
		}
	}
}
