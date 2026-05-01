all: execute

execute:
	cc main.c -o Editor.out
	clear && ./Editor.out