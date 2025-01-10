CC = gcc
FLAGS = -pedantic -g -Wall -lpthread -pthread -lrt -lm
BIN = servant
BIN2 = server
BIN3 = client
SOURCE = servant.c
SOURCE2 = server.c
SOURCE3 = client.c
HEADER = 

target : all

all: $(HEADER) $(SOURCE) $(SOURCE2) $(SOURCE3)
	 $(CC) $(SOURCE) $(FLAGS) -o $(BIN)
	 $(CC) $(SOURCE2) $(FLAGS) -o $(BIN2)
	 $(CC) $(SOURCE3) $(FLAGS) -o $(BIN3)

clean :
	rm $(BIN) $(BIN2) $(BIN3)
