#include "stdafx.h"
#include "Server.h"

// TODO: ����� ���� �޼��� ����
// 0 ~ WM_USER -1 : �ý��ۿ��� ����ϱ� ���� ����� �޽���
// WM_USER ~ 0x7FFF : ���Ƿ� ����� �� �ִ� �޽���
#define UM_NETWORK (WM_USER+1)
#define df_SERVER_PORT 25000
#define df_BUFFER_SIZE 1000

using namespace mylib;

// Ŭ���̾�Ʈ ����ü
struct stCLIENT
{
	SOCKET Socket;
	CRingBuffer* RecvQ;
	CRingBuffer* SendQ;
	WCHAR szIP[INET_ADDRSTRLEN];
};

list<stCLIENT*> g_Clientlist;

// Ŭ���̾�Ʈ ���� �Լ�
stCLIENT* AddClient(SOCKET sock, WCHAR* szIP);
void RemoveClient(SOCKET sock);
void RemoveClient(list<stCLIENT*>::iterator iter);

// ��Ʈ��ũ �Լ�
SOCKET g_ListenSocket;
BOOL Start(HWND hWnd);
void Stop();
void ProcRecv(SOCKET sock);
void ProcSend(SOCKET sock);
void SendBroadcast(char* pBuffer, int iSize);

//////////////////////////////////////////////////////////////////////////////////
// TODO: ���̾�α� ���ν��� �ݹ� �Լ�
// :: INT_PTR DlgProc(HWND hWnd, UINT iMsg, WPARAM wordParam, LPARAM longParam)
// https://msdn.microsoft.com/en-us/library/ms645469(v=VS.85).aspx
//
// Parameter : HWND(��ȭ���� �ڵ�), UINT(�޼���), WPARAM, LPARAM
// Return : �޼��� ó�� �� TRUE, �ݴ��� ��� FALSE
//////////////////////////////////////////////////////////////////////////////////
// TODO: �ݹ�(Callback) �Լ� 
//  �ٸ� �Լ��� ���ڷμ� �̿�Ǵ� �Լ�. **� �̺�Ʈ�� ���� ȣ��**�Ǿ����� �Լ�
// TODO: �Լ�ȣ��Ծ� CALLBACK
//  callee(ȣ��� �Լ�)�� �ڽ��� stack frame ������ ���� ����. ����ڵ�� ret(== _stdcall)
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
	// ListBox �ڵ� ���
	HWND	hListBox = GetDlgItem(hWnd, IDC_LIST1);
    switch (message)
    {
	// WM_INITDIALOG : ��ȭ ���ڰ� ǥ�õǱ� �ٷ� ���� ��ȭ ���� ���ν����� �������� �޼���
	case WM_INITDIALOG:
	{
		if (Start(hWnd) != TRUE)
		{
			Stop();
			exit(-1);
			break;
		}

		SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)L"���� �ʱ�ȭ ����");
		return TRUE;
	}

	// TODO : ������ �޼��� ���� �� ������
	// ������ �޽��� ���� �� ������ ���� �Լ��� ȣ������ ������, ������ �޽����� �ٽ� �߻����� ����
	//ex) FD_READ �̺�Ʈ�� �����Ͽ� recv()�� ȣ������ ������ ������ ���Ͽ� ���� FD_READ �̺�Ʈ�� �ٽ� �߻����� ����

	case UM_NETWORK:
		// TODO: WSAAsyncSelect()�� �����ϴ� �޼����� LPARAM ��
		//  WSAGETSELECTERROR(LPARAM) : LPARAM�� ���� 16��Ʈ. ���� �ڵ� == HIWORD(LPARAM)
		//  WSAGETSELECTEVENT(LPARAM) : LPARAM�� ���� 16��Ʈ. WSAAsyncSelect() ������ ���ڿ��� ������ �̺�Ʈ �� �߻��� �̺�Ʈ == LOWORD(LPARAM)
		switch (WSAGETSELECTEVENT(lParam))
		{
		case FD_ACCEPT:
		{
			int err;
			SOCKADDR_IN clientAddr;
			int addrlen = sizeof(clientAddr);

			// accept() �Լ��� �����ϴ� ������ ���� ��� ���ϰ� ������ �Ӽ��� ����
			// �� ���� ��� ������ ���� ������ �ۼ����� ���� �����Ƿ� FD_READ, FD_WRITE �̺�Ʈ�� ó������ ����
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

			// accept() �Լ��� �����ϴ� ������ �̺�Ʈ ó���� �ؾ��ϱ� ������ WSAAsyncSelect() �Լ��� �ٽ� ȣ���Ͽ� �Ӽ� ������ �ʿ���
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

			WCHAR szComment[INET_ADDRSTRLEN + 10] = L"���� : ";
			wcscat_s(szComment, szIP);
			SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)szComment);
			break;
		}
		case FD_READ:
			// TODO: WSAAsyncSelect()�� �����ϴ� �޼����� WPARAM ��
			//  ���� ��Ʈ��ũ �̺�Ʈ�� �߻��� ����
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
			WCHAR szComment[INET_ADDRSTRLEN + 10] = L"���� : ";
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

	// Nagle �˰��� ����
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
	// s - �̺�Ʈ ������ �ʿ��� ���� ��ũ����
	// hWnd - ��Ʈ��ũ �̺�Ʈ�� �߻��� �� �޽����� ���� ������ �ڵ�
	// wMsg - ��Ʈ��ũ �̺�Ʈ�� �߻��� �� ������ �޽���. 
	//		 �߰� ����(LPARAM : ���� 16��Ʈ = �����ڵ�, ���� 16��Ʈ = 4��° ���ڿ��� �������̺�Ʈ / WPARAM : �̺�Ʈ �߻� ����)�� �Բ� ����
	// lEvent - ��Ʈ��ũ �̺�Ʈ (��Ʈ ����ũ) 
	//		 FD_READ, FD_WRITE, FD_OOB, FD_ACCEPT, FD_CONNECT, FD_CLOSE, FD_QOS, FD_GROUP_QOS, FD_ROUTING_INTERFACE_CHANGE, FD_ADDRESS_LIST_CHANGE
	//////////////////////////////////////////////////////////////////////////////////
	//  �ش� �Լ� ȣ�� �� �ش� ������ �ڵ����� Non-blocking ���·� ��ȯ
	// (blocking ������ ������ �޽��� ������ ������Ű�� ����)
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
