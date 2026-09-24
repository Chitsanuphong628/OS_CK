CXX      := g++
CXXFLAGS := -Wall -Wextra -pthread -std=c++17

SERVER_BIN := server_bin
CLIENT_BIN := client_bin

.PHONY: all clean

all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): server.cpp common.h
	$(CXX) $(CXXFLAGS) -o $@ server.cpp

$(CLIENT_BIN): client.cpp common.h
	$(CXX) $(CXXFLAGS) -o $@ client.cpp

clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN)
