# Lab2 실습 Makefile - "(5.2)matrix_add.c" 처럼 앞에 (번호)가 붙은 파일명도 처리한다.
# 실행 파일 이름은 번호를 뺀 이름이다. 예) (5.2)matrix_add.c -> ./matrix_add
CC     = gcc
CFLAGS = -Wall -Wextra -O2

all:
	@for f in *.c; do \
	  n="$${f#*)}"; n="$${n%.c}"; \
	  case "$$n" in \
	    mqchat) libs="-lrt -lpthread" ;; \
	    shmcopy) libs="-lrt" ;; \
	    notify_server|notify_client) libs="-lsystemd" ;; \
	    *) libs="" ;; \
	  esac; \
	  echo "$(CC) $(CFLAGS) -o $$n $$f $$libs"; \
	  $(CC) $(CFLAGS) -o "$$n" "$$f" $$libs || exit 1; \
	done

# myls_R 시험용 디렉토리 트리 만들기
testtree:
	mkdir -p test_tree/docs/img test_tree/src/lib test_tree/empty
	echo hello > test_tree/a.txt
	echo world > test_tree/b.txt
	echo doc > test_tree/docs/intro.txt
	echo png > test_tree/docs/img/logo.png
	echo c > test_tree/src/main.c
	echo c > test_tree/src/lib/util.c
	ln -sf src test_tree/srclink

clean:
	@for f in *.c; do n="$${f#*)}"; rm -f "$${n%.c}"; done
	rm -rf test_tree demo_dir

.PHONY: all testtree clean
