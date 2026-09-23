# Makefile — คนที่ 5 (Docker+Integration) ดูแล
# โครงสร้างไฟล์แบบ flat: protocol.h / server.cpp / client.cpp อยู่ root ทั้งหมด
# (ชื่อจริงใน repo คือ protocol.h ไม่ใช่ common.h ตามที่คุยกันตอนแรก — เช็คจาก origin/main แล้ว)
# แก้ CXXFLAGS ตรงนี้ถ้าใครต้องการ debug flag เพิ่ม (เช่น -g -fsanitize=thread เช็ค race condition)

CXX      := g++
CXXFLAGS := -Wall -Wextra -pthread -std=c++17

SERVER_BIN := server_bin
CLIENT_BIN := client_bin

.PHONY: all clean

all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): server.cpp protocol.h
	$(CXX) $(CXXFLAGS) -o $@ server.cpp

$(CLIENT_BIN): client.cpp protocol.h
	$(CXX) $(CXXFLAGS) -o $@ client.cpp

clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN)
