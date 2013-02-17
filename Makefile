CC = gcc

CFLAGS = -Wall -c -g -O3
LDFLAGS = -Wall -g -O3 -pthread
EXE = walce

OBJS = \
	position.o search.o move.o stats.o bitboard.o log.o movepick.o prnd.o \
	history.o table.o eval.o tcontrol.o utils.o proto_uci.o proto_xboard.o \
	proto_console.o interface.o thread.o

C_OBJS = $(OBJS) main.o
T_OBJS = $(OBJS) test.o

LBITS := $(shell getconf LONG_BIT)
ifeq ($(LBITS),64)
	CFLAGS += -DIS_64BIT
endif


all: test $(EXE)

$(EXE): $(C_OBJS)
	$(CC) $(LDFLAGS) -o $(EXE) $(C_OBJS)

test: $(T_OBJS)
	$(CC) $(LDFLAGS) -o test $(T_OBJS)

%.o: %.c %.h
	$(CC) $(CFLAGS) $<

clean:
	rm *.o


