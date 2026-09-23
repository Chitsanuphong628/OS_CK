FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    util-linux \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . /app


RUN g++ -std=c++11 -pthread server.cpp -o server

CMD ["/bin/bash"]