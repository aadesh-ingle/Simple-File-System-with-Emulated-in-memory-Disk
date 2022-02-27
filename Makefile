a: sfs.o disk.o main.o
	gcc -Wall -o a main.o disk.o sfs.o -lm

main.o: main.c
	gcc -c -Wall main.c	
disk.o: disk.c
	gcc -c -Wall disk.c
	
sfs.o: sfs.c
	gcc -c -Wall sfs.c
clean:
	rm disk.o
	rm sfs.o
	rm main.o

