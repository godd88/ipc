all:
	gcc -O0 -g -pthread main.c
	gcc -O0 -g -pthread lwp.c -o lwp.out
