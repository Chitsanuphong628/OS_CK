# Dockerfile — คนที่ 5 (Docker+Integration) ดูแล
# ใช้ container เดียว รันทั้ง server + client หลายตัวผ่าน `docker exec -it <name> bash` หลาย terminal
# เหตุผลที่ไม่แยกหลาย container: System V message queue อยู่ใน IPC namespace ของ container
# ถ้าแยก container กัน queue จะมองไม่เห็นกัน (ต้องเซ็ต --ipc=container:<name> หรือ --ipc=host เพิ่ม ซึ่งไม่จำเป็นสำหรับงานนี้)

FROM ubuntu:22.04

RUN apt-get update && \
    apt-get install -y build-essential make util-linux && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN make

# container ต้องมีชีวิตอยู่เฉยๆ เพื่อให้ docker exec หลาย terminal เข้ามาสั่ง server/client เองได้
CMD ["tail", "-f", "/dev/null"]
