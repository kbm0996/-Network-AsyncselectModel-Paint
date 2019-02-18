#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winSock2.h>
#include <Ws2tcpip.h>
#include <cstdlib>
#include <cstdio>
#include <windowsx.h>
#include "CRingBuffer.h"
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "Winmm.lib")

using namespace mylib;

// TODO: 사용자 정의 메세지 영역
// 0 ~ WM_USER -1 : 시스템에서 사용하기 위해 예약된 메시지
// WM_USER ~ 0x7FFF : 임의로 사용할 수 있는 메시지
#define UM_NETWORK (WM_USER+1)
#define df_SERVER_PORT 25000
#define df_BUFFER_SIZE 1000

// 패킷 형태
unsigned short	g_sHeader = 16;	//헤더 2 byte - 패킷길이 16
struct st_DRAW
{
	//ushort		//2 byte (Header)
	int	iStartX;	//4 byte
	int	iStartY;	//4 byte
	int	iEndX;		//4 byte
	int	iEndY;		//4 byte
};					//total 18 byte   

st_DRAW g_stDraw;

SOCKET g_Socket;
BOOL g_bConnect = false;
BOOL g_bSendFlag = false;
CRingBuffer* g_RecvQ;
CRingBuffer* g_SendQ;

// 윈도우 메시지 처리 함수
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

// 네트워크 함수
void ProcRecv();
void ProcSend();
void OnRecv(HWND hWnd);
void SendPacket(char* pBuffer, int iSize);
void ProcSend(char* pBuffer, int iSize); // test

// 그리기 함수
void DrawLine(HWND hWnd, int iStartX, int iStartY, int iEndX, int iEndY);

int main(int argc, char *argv[])
{
	/*----------------------------------------------------------------------------*/
	////////////////////////////////////////////////////////////////
	// TODO : WINAPI 윈도우 생성하기
	//
	////////////////////////////////////////////////////////////////

	// 1. WNDCLASSEXW 구조체 : RegisterClassEx()나 GetClassInfoEx() 함수의 parameter로 사용되는 구조체
	WNDCLASSEXW wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);		// he size, in bytes, of this structure. Set this member to sizeof(WNDCLASSEX)
	wcex.style = CS_HREDRAW | CS_VREDRAW;	// The class style(s). https://docs.microsoft.com/ko-kr/windows/desktop/winmsg/window-class-styles
	wcex.lpfnWndProc = WndProc;				// A pointer to the window procedure
	wcex.cbClsExtra = 0;					// The number of extra bytes to allocate following the window-class structure
	wcex.cbWndExtra = 0;					// The number of extra bytes to allocate following the window instance
	wcex.hInstance = 0;						// A handle to the instance that contains the window procedure for the class
	wcex.hIcon = NULL;						// A handle to the class icon. NULL = default icon
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);	// A handle to the class cursor
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);	// A handle to the class background brush. https://docs.microsoft.com/en-us/windows/desktop/api/winuser/ns-winuser-tagwndclassexa
	wcex.lpszMenuName = NULL;				// Pointer to a null-terminated character string that specifies the resource name of the class menu
	wcex.lpszClassName = L"WindowClass";	// lpszClassName is a string, it specifies the window class name. max length = 256
	wcex.hIconSm = NULL;					// A handle to the class small icon. NULL = default icon
	
	// 2. RegisterClassEx() : CreateWindowEx() 함수 호출을 위해 등록하는 함수
	if(!RegisterClassExW(&wcex))	
		return 1;

	// 3. CreateWindowW() : 윈도우 생성하는 매크로 함수
	HWND hWnd = CreateWindowW(L"WindowClass", L"Network Paint", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, 800, 600, 0, 0, 0, 0);
	if (hWnd == NULL) 
		return -1;

	// 4. ShowWindow() : 윈도우 표시 상태 설정
	ShowWindow(hWnd, SW_SHOWNORMAL); 

	// 5. UpdateWindow(hWnd) : WM_PAINT 메세지를 해당 윈도우에 전송시켜 윈도우를 갱신시키는 함수
	UpdateWindow(hWnd);

	// 콘솔창 감추기
	HWND hWndConsole = GetConsoleWindow();
	ShowWindow(hWndConsole, SW_HIDE);
	/*----------------------------------------------------------------------------*/

	int err;
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		err = WSAGetLastError();
		return -1;
	}

	g_Socket = socket(AF_INET, SOCK_STREAM, 0);
	if (g_Socket == INVALID_SOCKET)
	{
		err = WSAGetLastError();
		return -1;
	}

	// Nagle 알고리즘 끄기
	BOOL bEnable = TRUE;
	setsockopt(g_Socket, IPPROTO_TCP, TCP_NODELAY, (char *)&bEnable, sizeof(bEnable));

	SOCKADDR_IN serveraddr;
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(df_SERVER_PORT);
	InetPton(AF_INET, L"127.0.0.1", &serveraddr.sin_addr);

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
	if(WSAAsyncSelect(g_Socket, hWnd, UM_NETWORK, FD_CONNECT | FD_READ | FD_WRITE | FD_CLOSE) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSAEWOULDBLOCK)
			return -1;
	}

	if (connect(g_Socket, (SOCKADDR *)&serveraddr, sizeof(serveraddr)) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSAEWOULDBLOCK)
			return -1;
	}
	
	//////////////////////////////////////////////////////////////////////////////////
	// TODO: Call Thread의 Message Queue로부터 메세지 검색
	// :: BOOL GetMessage(LPMSG, HWND, UINT, UINT)
	// https://msdn.microsoft.com/en-us/library/ms645469(v=VS.85).aspx
	//
	// Parameter : HWND(대화상자 핸들), UINT(메세지), WPARAM(추가 메세지 관련 정보), LPARAM(추가 메세지 관련 정보)
	//            * WPARAM, LPARAM은 메세지 종류에 따라 다른 정보가 들어옴
	// Return : 메세지 처리 시 TRUE, 반대의 경우 FALSE
	//////////////////////////////////////////////////////////////////////////////////
	MSG msg;
	while (GetMessage(&msg, 0, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// 윈속 종료
	closesocket(g_Socket);
	WSACleanup();

	return 0;
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;
	
	// 마우스 좌표 얻는 매크로 함수
	g_stDraw.iStartX = GET_X_LPARAM(lParam);
	g_stDraw.iStartY = GET_Y_LPARAM(lParam);

	switch (message)
	{
	case WM_CREATE:	
		g_RecvQ = new CRingBuffer(100);
		g_SendQ = new CRingBuffer(20);
		break;
	case UM_NETWORK:
		// TODO: WSAAsyncSelect()가 생성하는 메세지의 LPARAM 값
		//  WSAGETSELECTERROR(LPARAM) : LPARAM의 상위 16비트. 에러 코드 == HIWORD(LPARAM)
		//  WSAGETSELECTEVENT(LPARAM) : LPARAM의 하위 16비트. WSAAsyncSelect() 마지막 인자에서 지정한 이벤트 중 발생한 이벤트 == LOWORD(LPARAM)
		switch (WSAGETSELECTEVENT(lParam))
		{
		case FD_CONNECT:
			g_bConnect = true;
			break;
		case FD_READ:
			ProcRecv();
			OnRecv(hWnd);
			break;
		case FD_WRITE:
			ProcSend();
			break;
		case FD_CLOSE:
			g_bConnect = false;
			break;
		}
		break;
	case WM_MOUSEMOVE:
		if (wParam & MK_LBUTTON)
		{
			SendPacket((char*)&g_sHeader, sizeof(g_sHeader)); // 헤더 전송
			SendPacket((char*)&g_stDraw, sizeof(g_stDraw)); // 페이로드 전송

			g_stDraw.iEndX = g_stDraw.iStartX;
			g_stDraw.iEndY = g_stDraw.iStartY;
		}
		break;
	case WM_LBUTTONDOWN:
		g_stDraw.iEndX = g_stDraw.iStartX;
		g_stDraw.iEndY = g_stDraw.iStartY;
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);	

		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:	
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

void ProcRecv()
{
	char Buffer[df_BUFFER_SIZE];
	int retval = recv(g_Socket, Buffer, g_RecvQ->GetUnbrokenEnqueueSize(), 0);
	if (retval == 0)
		return;

	if (retval == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != WSAEWOULDBLOCK)
			return;
	}

	if (g_RecvQ->GetFreeSize() <= 0) 
		return;

	g_RecvQ->Enqueue(Buffer, retval);
}
void ProcSend()
{
	int iUsingSize = g_SendQ->GetUseSize();

	char Buffer[100];
	g_SendQ->Peek(Buffer, iUsingSize);

	g_bSendFlag = true;
	if (iUsingSize <= 0)
	{
		g_bSendFlag = false;
		return;
	}
	int retval = send(g_Socket, Buffer, g_SendQ->GetUseSize(), 0);
	if (retval == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSAEWOULDBLOCK)
		{
			g_bSendFlag = false;
			return;
		}
	}
	g_SendQ->MoveReadPos(retval);
}

void OnRecv(HWND hWnd)
{
	int retval;
	st_DRAW Packet;
	unsigned short	Header;
	while (1)
	{
		// RecvQ에 Header길이만큼 있는지 확인
		if (g_RecvQ->GetUseSize() < sizeof(Header))
			break;

		// Packet 길이 확인 (Header크기 + Payload길이)
		retval = g_RecvQ->Peek((char*)&Header, sizeof(Header));
		if (g_RecvQ->GetUseSize() < Header + sizeof(Header))
			break;

		// --HEADER 
		// 받은 패킷으로부터 Header 제거
		g_RecvQ->MoveReadPos(sizeof(Header)); // Dequeue를 안 쓴 이유는 memcpy가 너무 무겁기 때문

		// -- PAYLOAD
		retval = g_RecvQ->Dequeue((char*)&Packet, Header);
		if (retval == 0)
			break;

		DrawLine(hWnd, Packet.iStartX, Packet.iStartY, Packet.iEndX, Packet.iEndY);
	}
}

void SendPacket(char* pBuffer, int iSize)
{
	if (g_SendQ->Enqueue(pBuffer, iSize) != iSize)
		return;
	
	ProcSend();
}

void ProcSend(char* pBuffer, int iSize)
{
	g_bSendFlag = true;
	if (g_SendQ->GetUseSize() <= 0)
	{
		g_bSendFlag = false;
		return;
	}
	int retval = send(g_Socket, g_SendQ->GetReadBufferPtr(), g_SendQ->GetUnbrokenDequeueSize(), 0);
	if (retval == 0) 
		return;

	if (retval == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSAEWOULDBLOCK)
		{
			g_bSendFlag = false;
			return;
		}
	}
	g_SendQ->MoveReadPos(retval);
}

void DrawLine(HWND hWnd, int iStartX, int iStartY, int iEndX, int iEndY)
{
	HPEN hPen = CreatePen(PS_SOLID, 1, 0);
	HDC hdc = GetDC(hWnd);
	SelectObject(hdc, hPen);	// DC에 펜 등록
	HPEN hOldPen = (HPEN)SelectObject(hdc, NULL);

	MoveToEx(hdc, iStartX, iStartY, NULL);
	LineTo(hdc, iEndX, iEndY);

	hPen = hOldPen;
	DeleteObject(hPen);
	ReleaseDC(hWnd, hdc);
}

