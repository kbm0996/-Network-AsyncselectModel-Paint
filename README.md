# 네트워크 프로그래밍 Asyncselect 모델
## 📢 개요
 Asyncselect 모델은 윈도우 메시지 형태로 소켓과 관련된 네트워크 이벤트를 처리할 수 있다. 그리고 멀티스레드를 사용하지 않고도 여러 개의 소켓을 처리하는 것이 가능하며 윈도우 메시지를 통하여 비동기적으로 소켓을 활용할 수 있다.

 소켓 이벤트를 윈도우 메시지 형태로 처리하므로 GUI 애플리케이션과 결합하기 용이하다. 그러나 반대로 하나의 윈도우 프로시저에서 일반 윈도우 메시지와 소켓 메시지를 처리해야 하므로 성능저하의 요인이 되기도 하고, 윈도우가 없는 콘솔 프로그램에서는 사용할 수 없다.


## 💻 간단한 멀티플레이 그림판

 다수의 클라이언트가 참여할 수 있는 1:N 서버-클라이언트 그림판 프로그램. 
클라이언트에서는 마우스 드래그로 선을 그을 수 있다.

  ![capture](https://github.com/kbm0996/Network-Programming-AsyncselectModel/blob/master/GIF.gif)
  
  **figure 1. Multi Paint(animated)*

## 📌 동작 원리

 ![capture](https://github.com/kbm0996/Network-Programming-AsyncselectModel/blob/master/figure.png)
  
  **figure 2. mechanism of asyncselect model*
  
 ① WSAAsyncSelect() 함수를 이용하여 소켓을 위한 윈도우 메시지와 처리할 네트워크 이벤트를 등록
 
    WSAAsyncSelect(g_ListenSocket, hWnd, UM_NETWORK, FD_ACCEPT | FD_READ | FD_WRITE | FD_CLOSE);
    // 접속 요청(FD_ACCEPT), 데이터 전송(FD_WRITE), 데이터 수신(FD_READ), 접속 종료(FD_CLOSE) 시
    // 특정 윈도우 메시지(UM_NETWORK)로 특정 윈도우(hWnd)에 알려달라는 내용을 등록

 ② 등록한 네트워크 이벤트가 발생하면 윈도우 메시지가 발생하고 윈도우 프로시저가 호출
 
 ③ 윈도우 프로시저에서는 받은 메시지 종류에 따라 적절한 소켓 함수를 호출하여 처리
 
    LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
      switch (message)
      {
      case WM_INITDIALOG:
      {
        ..서버 초기화..
        return TRUE;
      }

      case UM_NETWORK:
        switch (WSAGETSELECTEVENT(lParam))
        {
        case FD_ACCEPT:
          ..ACCEPT 처리..
          break;
        case FD_READ:
          ..Recv 처리..
          break;
        case FD_WRITE:
          ..Send 처리..
          break;
        case FD_CLOSE:
          ..클라이언트 정리..
          break;
        }
        break;
      case WM_DESTROY:
        ..메모리 정리..
        break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        return 0;
    }


## 📌 주요 레퍼런스
### 1. int WSAAsyncSelect(SOCKET s, HWND hWnd, unsigned int wMsg, long lEvent);
* SOCKET s : 처리하고자 하는 소켓
* HWND hWnd : 메시지를 수신할 윈도우 핸들
* unsigned int wMsg : 윈도우가 받을 메시지 (사용자 정의메시지)
* long lEvent : 처리할 네트워크 이벤트 종류  (비트 마스크 조합)
* 리턴값 : 성공 시 준비된 0, 실패 시 SOCKET_ERROR

▶ 주의 사항
>1. 해당 함수 호출 시 해당 소켓은 자동으로 Non-blocking 상태로 전환됨 → blocking 상태라면 윈도우 메시지 루프를 정지시키기 때문
>2. 윈도우 메시지 수신 시 적절한 소켓 함수를 호출하지 않으면, 윈도우 메시지가 다시 발생하지 않음
> ex) FD_READ 이벤트에 대응하여 recv()를 호출하지 않으면 동일한 소켓에 대한 FD_READ 이벤트는 다시 발생하지 않음

   | network event | mean |
   |:--------|:--------|
   | FD_ACCEPT	| 클라이언트 접속 시도 시 윈도우 메세지 발생	|
   | FD_READ	| 데이터 수신 가능 시 윈도우 메세지 발생	|
   | FD_WRITE	| 데이터 송신 가능 시 윈도우 메세지 발생	|
   | FD_CLOSE	| 클라이언트 접속 종료 시 윈도우 메세지 발생	|
   | FD_CONNECT	| 서버 접속 완료 시 윈도우 메세지 발생	|
   | FD_OOB	| OOB(Out Of Band) 데이터 도착	시 윈도우 메세지 발생 |
   
  **table 1. network event list*
  
### 2. 윈도우 프로시저 : LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
* HWND hwnd : 메시지가 발생한 윈도우
* UINT msg : WSAAsyncSelect() 함수 호출시 등록한 사용자 정의 메시지
* WPARAM wParam : 네트워크 이벤트가 발생한 소켓
* LPARAM lParam : 하위 16비트는 발생한 네트워크 이벤트, 상위 16비트는 오류 코드

      g_ListenSocket = socket(AF_INET, SOCK_STREAM, 0);
      if (WSAAsyncSelect(g_ListenSocket, hWnd, UM_NETWORK, FD_ACCEPT | FD_READ | FD_WRITE | FD_CLOSE) == SOCKET_ERROR) 
      { ..에러 처리.. }
      .
      .
      .
      LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
      {
        switch(iMessage)
        {
        case WM_CREATE:	
          On_Create(hWnd);	
          break;
        case UM_NETWORK:
          On_Socket(hWnd, (SOCKET)wParam, LOWORD(lParam), HIWORD(lParam));
          break;
        case WM_DESTROY:
          On_Destroy();
        }
        return (DefWindowProc(hWnd, iMessage, wParam, lParam));
      }
