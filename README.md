# Lab2

오픈소스프로젝트 실습 2: 파일 처리, 프로세스 및 프로세스간 통신

## 프로그램

| 번호 | 파일 | 설명 |
|---|---|---|
| 5.2 | `(5.2)matrix_add.c` | 행렬 크기를 입력받아 `malloc` 으로 할당하고 두 행렬을 더함 |
| 5.3 | `(5.3)file_ops.c` | 파일·디렉토리 함수 실습 (open, lseek, stat, link, rename 등) |
| 5.4 | `(5.4)myls_R.c` | `ls -R` 구현 (scandir + 재귀) |
| 5.5 | `(5.5)typing.c` | 타자 연습 (오타 횟수, 분당 타자수 측정) |
| 5.6 | `(5.6)proc_signal.c` | 프로세스(fork, exec, wait)와 시그널 실습 |
| 5.7 | `(5.7)ipc_demo.c` | pipe, FIFO, System V 메시지 큐·공유 메모리·세마포어 |
| 5.8 | `(5.8)pipechat.c` | FIFO 를 이용한 텍스트 채팅 |
| 5.9 | `(5.9)mqchat.c` | POSIX 메시지 큐를 이용한 텍스트 채팅 |
| 5.10 | `(5.10)shmcopy.c` | POSIX 공유 메모리와 mmap 을 이용한 파일 복사 |
| 5.11 | `(5.11)notify_server.c`, `(5.11)notify_client.c` | D-Bus(sd-bus) 알림 서버-클라이언트 |
| 5.12 | 팀과제 쉘 프로그램 | https://github.com/SexyJaeseok/mini-shell-team2_1 |

## 빌드

```
cd Lab 2      # Lab 2 폴더로 이동
make          # 전체 컴파일
make clean    # 실행 파일 삭제
```

## 실행

```
./matrix_add
./file_ops
make testtree && ./myls_R test_tree        # ls -R test_tree 와 비교
./typing
./proc_signal
./ipc_demo
./shmcopy 원본파일 복사본파일
```

채팅과 D-Bus 는 터미널을 여러 개 열어서 실행한다.

```
# 5.8 / 5.9  (터미널 2개, 종료: /quit)
./pipechat A      ./pipechat B
./mqchat A        ./mqchat B

# 5.11  (터미널 3개)
./notify_server
./notify_client listen 30
./notify_client send "제목" "내용"
./notify_client count
```

## 팀 과제

5.12 팀 과제: 리눅스 기본 명령어와 파이프를 지원하는 미니 셸 (C, Ubuntu 24.04, GCC)

- 저장소: https://github.com/SexyJaeseok/mini-shell-team2_1
- 팀원: 임채민, 박재석, 김시현

| 담당 | 구현한 명령 |
|---|---|
| 임채민 | `cat`, `grep`, `pwd` |
| 박재석 | `ls`, `mkdir`, `rmdir`, `rm` |
| 김시현 | `cd`, `cp`, `mv`, `ln` |
| 공동 | 명령 분석, `exit`, `&`(백그라운드), 시그널(Ctrl-C, Ctrl-Z), 재지향(`<`, `>`), 파이프(`\|`), 외부 프로그램 실행 |

저장소의 `mini-shell-team2_1` 폴더에서 `make` 를 실행하면 `myshell` 실행 파일이 만들어진다.

```
cat input.txt | grep hello > result.txt     # 파이프와 재지향
sleep 15 &                                  # 백그라운드 실행
exit                                        # 종료
```

## 환경

Ubuntu (WSL), gcc, make
