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

// TODO: ����� ���� �޼��� ����
// 0 ~ WM_USER -1 : �ý��ۿ��� ����ϱ� ���� ����� �޽���
// WM_USER ~ 0x7FFF : ���Ƿ� ����� �� �ִ� �޽���
#define UM_NETWORK (WM_USER+1)
#define df_SERVER_PORT 25000
#define df_BUFFER_SIZE 1000

// ��Ŷ ����
unsigned short	g_sHeader = 16;	//��� 2 byte - ��Ŷ���� 16
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

// ������ �޽��� ó�� �Լ�
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

// ��Ʈ��ũ �Լ�
void ProcRecv();
void ProcSend();
void OnRecv(HWND hWnd);
void SendPacket(char* pBuffer, int iSize);
void ProcSend(char* pBuffer, int iSize); // test

// �׸��� �Լ�
void DrawLine(HWND hWnd, int iStartX, int iStartY, int iEndX, int iEndY);

int main(int argc, char *argv[])
{
	/*----------------------------------------------------------------------------*/
	////////////////////////////////////////////////////////////////
	// TODO : WINAPI ������ �����ϱ�
	//
	////////////////////////////////////////////////////////////////

	// 1. WNDCLASSEXW ����ü : RegisterClassEx()�� GetClassInfoEx() �Լ��� parameter�� ���Ǵ� ����ü
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
	
	// 2. RegisterClassEx() : CreateWindowEx() �Լ� ȣ���� ���� ����ϴ� �Լ�
	if(!RegisterClassExW(&wcex))	
		return 1;

	// 3. CreateWindowW() : ������ �����ϴ� ��ũ�� �Լ�
	HWND hWnd = CreateWindowW(L"WindowClass", L"Network Paint", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, 800, 600, 0, 0, 0, 0);
	if (hWnd == NULL) 
		return -1;

	// 4. ShowWindow() : ������ ǥ�� ���� ����
	ShowWindow(hWnd, SW_SHOWNORMAL); 

	// 5. UpdateWindow(hWnd) : WM_PAINT �޼����� �ش� �����쿡 ���۽��� �����츦 ���Ž�Ű�� �Լ�
	UpdateWindow(hWnd);

	// �ܼ�â ���߱�
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

	// Nagle �˰��� ����
	BOOL bEnable = TRUE;
	setsockopt(g_Socket, IPPROTO_TCP, TCP_NODELAY, (char *)&bEnable, sizeof(bEnable));

	SOCKADDR_IN serveraddr;
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(df_SERVER_PORT);
	InetPton(AF_INET, L"127.0.0.1", &serveraddr.sin_addr);

	//////////////////////////////////////////////////////////////////////////////////
	// TODO: WSAAsyncSelect(s, hWnd, wMsg, lEvent)
	//
	// s - �̺�Ʈ ������ �ʿ��� ���� ��ũ����
	// hWnd - ��Ʈ��ũ �̺�Ʈ�� �߻��� �� �޽����� ���� ������ �ڵ�
	// wMsg - ��Ʈ��ũ �̺�Ʈ�� �߻��� �� ������ �޽���. 
	//		 �߰� ����(LPARAM : ���� 16��Ʈ = �����ڵ�, ���� 16��Ʈ = 4��° ���ڿ��� �������̺�Ʈ / WPARAM : �̺�Ʈ �߻� ����)�� �Բ� ����
	// lEvent - ��Ʈ��ũ �̺�Ʈ (��Ʈ ����ũ) 
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
	// TODO: Call Thread�� Message Queue�κ��� �޼��� �˻�
	// :: BOOL GetMessage(LPMSG, HWND, UINT, UINT)
	// https://msdn.microsoft.com/en-us/library/ms645469(v=VS.85).aspx
	//
	// Parameter : HWND(��ȭ���� �ڵ�), UINT(�޼���), WPARAM(�߰� �޼��� ���� ����), LPARAM(�߰� �޼��� ���� ����)
	//            * WPARAM, LPARAM�� �޼��� ������ ���� �ٸ� ������ ����
	// Return : �޼��� ó�� �� TRUE, �ݴ��� ��� FALSE
	//////////////////////////////////////////////////////////////////////////////////
	MSG msg;
	while (GetMessage(&msg, 0, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// ���� ����
	closesocket(g_Socket);
	WSACleanup();

	return 0;
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;
	
	// ���콺 ��ǥ ��� ��ũ�� �Լ�
	g_stDraw.iStartX = GET_X_LPARAM(lParam);
	g_stDraw.iStartY = GET_Y_LPARAM(lParam);

	switch (message)
	{
	case WM_CREATE:	
		g_RecvQ = new CRingBuffer(100);
		g_SendQ = new CRingBuffer(20);
		break;
	case UM_NETWORK:
		// TODO: WSAAsyncSelect()�� �����ϴ� �޼����� LPARAM ��
		//  WSAGETSELECTERROR(LPARAM) : LPARAM�� ���� 16��Ʈ. ���� �ڵ� == HIWORD(LPARAM)
		//  WSAGETSELECTEVENT(LPARAM) : LPARAM�� ���� 16��Ʈ. WSAAsyncSelect() ������ ���ڿ��� ������ �̺�Ʈ �� �߻��� �̺�Ʈ == LOWORD(LPARAM)
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
			SendPacket((char*)&g_sHeader, sizeof(g_sHeader)); // ��� ����
			SendPacket((char*)&g_stDraw, sizeof(g_stDraw)); // ���̷ε� ����

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
		// RecvQ�� Header���̸�ŭ �ִ��� Ȯ��
		if (g_RecvQ->GetUseSize() < sizeof(Header))
			break;

		// Packet ���� Ȯ�� (Headerũ�� + Payload����)
		retval = g_RecvQ->Peek((char*)&Header, sizeof(Header));
		if (g_RecvQ->GetUseSize() < Header + sizeof(Header))
			break;

		// --HEADER 
		// ���� ��Ŷ���κ��� Header ����
		g_RecvQ->MoveReadPos(sizeof(Header)); // Dequeue�� �� �� ������ memcpy�� �ʹ� ���̱� ����

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
	SelectObject(hdc, hPen);	// DC�� �� ���
	HPEN hOldPen = (HPEN)SelectObject(hdc, NULL);

	MoveToEx(hdc, iStartX, iStartY, NULL);
	LineTo(hdc, iEndX, iEndY);

	hPen = hOldPen;
	DeleteObject(hPen);
	ReleaseDC(hWnd, hdc);
}

