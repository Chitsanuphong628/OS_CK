# Makefile — คนที่ 5 (Docker+Integration) ดูแล
# โครงสร้างไฟล์แบบ flat อยู่ root ทั้งหมด
# ตอนนี้ server.cpp include "common.h" และ client.cpp include "protocol.h" แยกคนละไฟล์กัน
# (ยังไม่ได้รวมเป็น header เดียว — ทีมต้องตกลงกันก่อน ดู README/พูดคุยในกลุ่ม)
# แก้ CXXFLAGS ตรงนี้ถ้าใครต้องการ debug flag เพิ่ม (เช่น -g -fsanitize=thread เช็ค race condition)

CXX      := g++
CXXFLAGS := -Wall -Wextra -pthread -std=c++17

SERVER_BIN := server_bin
CLIENT_BIN := client_bin

.PHONY: all clean

all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): server.cpp common.h
	$(CXX) $(CXXFLAGS) -o $@ server.cpp

$(CLIENT_BIN): client.cpp protocol.h
	$(CXX) $(CXXFLAGS) -o $@ client.cpp

clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN)
