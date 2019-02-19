# 네트워크 프로그래밍 Asyncselect 모델
## 📢 개요
 어싱크셀렉트(Asyncselect) 모델을 사용한 하나의 서버에 다수의 플레이어가 참여할 수 있는 네트워크 프로그램. 각각의 클라이언트는 마우스 클릭으로 선을 그을 수 있다.

  ![capture](https://github.com/kbm0996/Network-Programming-AsyncselectModel/blob/master/GIF.gif)
  
  **figure 1. Multi Paint(animated)*

## 📌 동작 원리
 Asyncselect 모델은 윈도우 메시지 형태로 소켓과 관련된 네트워크 이벤트를 처리할 수 있다. 그리고 멀티스레드를 사용하지 않고도 여러 개의 소켓을 처리하는 것이 가능하며 윈도우 메시지를 통하여 비동기적으로 소켓을 활용할 수 있다.

 소켓 이벤트를 윈도우 메시지 형태로 처리하므로 GUI 애플리케이션과 결합하기 용이하다. 그러나 반대로 하나의 윈도우 프로시저에서 일반 윈도우 메시지와 소켓 메시지를 처리해야 하므로 성능저하의 요인이 되기도 하고, 윈도우가 없는 콘솔 프로그램에서는 사용할 수 없다.

 다음 그림은 Select 모델의 동작 원리를 보여준다. Select 모델을 사용하려면 소켓 셋(socket set)을 준비해야 한다. 소켓 셋은 소켓 디스크립터의 집합을 의미하며, 호출할 함수의 종류에 따라 소켓을 적당한 셋에 넣어두어야 한다. 예를 들면, 어떤 소켓에 대해 recv() 함수를 호출하고 싶다면 읽기 셋에 넣고, send() 함수를 호출하고 싶다면 쓰기 셋에 넣으면 된다.
 
 ![capture](https://github.com/kbm0996/Network-Programming-AsyncselectModel/blob/master/figure.png)
  
  **figure 2. Before select()*
  
  \

>  소켓 셋을 세 개 준비하여 select() 함수를 호출하면(그림 9-3). select() 함수는 소켓 셋에 포함된 소켓이 입출력을 위한 준비가 될 떄까지 대기한다. 적어도 한 소켓이 준비되면 select(0 함수는 리턴한다(그림 9-4). 이때 소켓 셋에는 입출력이 가능한 소켓만 남고 나머지는 모두 제거된다. 그림 9-4는 두 소켓에 대해 읽기가 가능하고 한 소켓에 대해 쓰기가 가능함을 나타낸다. 따라서 이 세 소켓에 대해서는 소켓 함수를 호출하여 원하는 작업을 할 수 있다.
>
>  응용 프로그램은 소켓 셋을 통해 소켓 함수를 성공적으로 호출할 수 있는 지점을 알아낼 수 있고, 드물지만 소켓 함수의 호출 결과를 확인할 수도 있다. 다음은 소켓 셋의 역할을 정리한 것이다
  
   | 시점 | 읽기 셋(read set) |
   |:--------|:--------|
   | 함수 호출 시점	| - 클라이언트 접속 요청이 있으면 FD_ISSET()이 TRUE 리턴 → accept() 호출  <br/> - 소켓 수신 버퍼에 데이터가 있으면 TRUE 리턴 → recv(), recvfrom() 호출하여 데이터 읽기 <br/> - TCP 연결 종료시 TRUE 리턴 → recv(), recvfrom() 호출하여 연결 종료 감지	|

  **table 1. role of read set*

   | 시점 | 읽기 셋(write set) |
   |:--------|:--------|
   | 함수 호출 시점	| - 소켓 송신 버퍼의 여유 공간이 충분하면 send(), sendto()로 데이터 전송	| 
   | 함수 호출 결과 | - 넌블로킹 소켓을 사용한 connect() 함수 호출 성공	|

  **table 2. role of write set*
  
   | 시점 | 예외 셋(exception set) |
   |:--------|:--------|
   | 함수 호출 시점	| - OOB(Out-Of-Band) 데이터가 도착했으므로 recv(), recvfrom() 등의 함수를 <br/> 호출하여 OOB 데이터 받기 가능	| 
   | 함수 호출 결과 | - 넌블로킹 소켓을 사용한 connect() 함수 호출 실패	|

  **table 3. role of exception set*
  
- FD(File Descriptor; 파일 디스크립터) : 파일 또는 장치의 ID값. 중복되지 않으므로 unique 값으로 사용 가능


## 📌 주요 레퍼런스
### 1. int select(nfds, fd_set * readfds, fd_set * writefds, fd_set * exceptfds, const timeval * timeout);
* nfds : berkeley socket 호환성 (미사용)
* readfs : 읽기 가능 여부를 검사 할 소켓 세트
* writefds : 쓰기 가능 여부를 검사 할 소켓 세트
* timeout : select 함수 최대 대기 시간
* 리턴값 : 성공 시 준비된 fd 개수, 시간 만료 시 0, 오류 발생 시 -1

> 동기 I/O를 수행하기 위해 최대 FD_SETSIZE(default 값 64)개의 FD(File Descriptor)를 등록한다. FD_ISSET() 함수로 등록한 FD의 상태 변화를 감지할 수 있다

      FD_SET ReadSet;	 // FD_SET 구조체 : Select 모델에서 Socket들을 관리하는 구조체
      FD_ZERO(&ReadSet);	 // FD_ZERO() : FD_SET 구조체 초기화
      FD_SET(g_ListenSocket, &ReadSet);	// FD_SET() : FD_SET 구조체 내 SOCKET 배열에 g_ListenSocket 삽입
      for (int iCnt = 0; iCnt < df_USER_MAX; ++iCnt)
      {
        if (g_pPlayer[iCnt].iID != -1) // 접속이 된 유저
          FD_SET(g_pPlayer[iCnt].Socket, &ReadSet);
      }
      timeval Time;
      Time.tv_sec = 0; // 초
      Time.tv_usec = 0; // 마이크로초
      select(0, &ReadSet, NULL, NULL, &Time);

### 2. FD_ISSET(fd, *fdset);
* fd : 확인할 소켓
* fdset : 검사 할 소켓 세트
* 리턴값 : fd의 비트가 fdset 내에서 세트로 설정되어 있으면 0이 아닌 값, 그렇지 않으면 0


      // 서버 Accept 처리
      if (FD_ISSET(g_ListenSocket, &ReadSet))
       ProcAccept();
      // 클라이언트 수신 패킷 처리
      for (int iCnt = 0; iCnt < df_USER_MAX; ++iCnt)
      {
       if (FD_ISSET(g_pPlayer[iCnt].Socket, &ReadSet))
        ProcRecv(&g_pPlayer[iCnt]);
      }

