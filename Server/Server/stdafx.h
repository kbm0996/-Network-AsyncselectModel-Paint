// stdafx.h : ���� ��������� ���� ��������� �ʴ�
// ǥ�� �ý��� ���� ���� �Ǵ� ������Ʈ ���� ���� ������
// ��� �ִ� ���� �����Դϴ�.
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // ���� ������ �ʴ� ������ Windows ������� �����մϴ�.
// Windows ��� ����:
#include <windows.h>

// C ��Ÿ�� ��� �����Դϴ�.
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>


// TODO: ���α׷��� �ʿ��� �߰� ����� ���⿡�� �����մϴ�.
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winSock2.h>
#include <Ws2tcpip.h>
#include <cstdio>
#include <windowsx.h>
#include "CRingBuffer.h"
#include "CLinkedlist.h"
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "Winmm.lib")