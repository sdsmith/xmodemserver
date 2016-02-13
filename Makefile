PORT=53800
CFLAGS= -DPORT=\$(PORT) -g -Wall

all: xmodemserver xmodemserver.o clientmanagement.o clientstateprocess.o crc16.o helper.o

xmodemserver: xmodemserver.o clientmanagement.o clientstateprocess.o crc16.o helper.o
	gcc ${CFLAGS} -o xmodemserver $^

xmodemserver.o: xmodemserver.c xmodemserver.h
	gcc ${CFLAGS} -c $<

clientmanagement.o: clientmanagement.c xmodemserver.h
	gcc ${CFLAGS} -c $<

clientstateprocess.o: clientstateprocess.c xmodemserver.h
	gcc ${CFLAGS} -c $<

crc16.o: crc16.c crc16.h
	gcc ${CFLAGS} -c $<

helper.o: helper.c
	gcc ${FLAGS} -c $<

clean:
	rm *.?~ *.o xmodemserver
