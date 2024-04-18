CC=gcc
CXX=g++
CFLAGS=-g -Wall

# Object files
OBJS=drone3.o Message.o Utility.o

# Executable name
EXEC=drone3

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CXX) -std=c++11 -o $(EXEC) $(OBJS)

drone3.o: drone3.cpp Utility.h Message.h ConfigEntry.h
	$(CXX) -std=c++11 -c drone3.cpp

Message.o: Message.cpp Message.h Utility.h
	$(CXX) -std=c++11 -c Message.cpp

Utility.o: Utility.cpp Utility.h ConfigEntry.h Message.h
	$(CXX) -std=c++11 -c Utility.cpp

clean:
	rm -f $(EXEC) $(OBJS)