all: execute

execute:
	cc main.c -W -Wall -Wsign-conversion -o Editor.out
	clear && ./Editor.out