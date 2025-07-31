all: llc

llc: llc.c
	cc -o llc llc.c -Wall

clean:
	rm -f llc *.o