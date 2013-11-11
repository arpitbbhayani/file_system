ro:
	g++ -Wall -g -O0 -D_FILE_OFFSET_BITS=64 ro.cpp -o ro -lfuse

rw:
	g++ -Wall -g -O0 -D_FILE_OFFSET_BITS=64 rw.cpp -o rw -lfuse

hello:
	gcc -Wall -g -O0 -D_FILE_OFFSET_BITS=64 hello.c -o hello -lfuse

clean:
	rm -f *.o ro rw hello
