CC = gcc
FLAGS = -pedantic -g -Wall -lpthread -pthread -lrt -lm
BIN = hw3unnamed
BIN2 = hw3named
SOURCE = hw3unnamed.c
SOURCE2 = hw3named.c
HEADER = 

target : all

all: $(HEADER) $(SOURCE) $(SOURCE2)
	$(CC) $(SOURCE) $(FLAGS) -o $(BIN)
	$(CC) $(SOURCE2) $(FLAGS) -o $(BIN2)

clean :
	rm $(BIN) $(BIN2)
