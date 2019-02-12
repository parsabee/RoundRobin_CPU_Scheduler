CFLAG= -W -Wall -g
PROGS= uspsv1 uspsv2 uspsv3
OBJECTS= p1fxns.o uspsv1.o uspsv2.o uspsv3.o iterator.o bqueue.o

all:$(PROGS)
uspsv1:p1fxns.o uspsv1.o
	cc -o uspsv1 $^
uspsv2:p1fxns.o uspsv2.o
	cc -o uspsv2 $^
uspsv3:p1fxns.o uspsv3.o bqueue.o iterator.o
	cc -o uspsv3 $^
p1fxns.o:p1fxns.c p1fxns.h
iterator.o:iterator.c iterator.h
bqueue.o:bqueue.c bqueue.h
uspsv1.o:uspsv1.c p1fxns.h
uspsv2.o:uspsv2.c p1fxns.h
uspsv3.o:uspsv3.c p1fxns.h bqueue.h

clean:
	rm $(OBJECTS) $(PROGS)