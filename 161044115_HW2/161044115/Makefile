CC = gcc
FLAGS = -pedantic -g -Wall -lpthread -pthread -lrt -lm
BIN = P
BIN2 = R
SOURCE = P.c
SOURCE2 = R.c
HEADER = 

target : all

all: $(HEADER) $(SOURCE) $(SOURCE2)
	$(CC) $(SOURCE) $(FLAGS) -o $(BIN)
	$(CC) $(SOURCE2) $(FLAGS) -o $(BIN2)

clean :
	rm $(BIN) $(BIN2)
