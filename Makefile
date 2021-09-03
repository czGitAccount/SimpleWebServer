CXX = g++
CFLAGS = -std=c++14 -O2 -Wall -g 
PACKAGE_PATH := $(shell pwd)

TARGET = server
OBJS = log/*.cpp timer/*.cpp http/*.cpp server/*.cpp buffer/*.cpp main.cpp

$(shell mkdir -p $(PACKAGE_PATH)/bin/Exe)

all: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o bin/Exe/$(TARGET)  -pthread 

clean:
	rm -rf $(OBJS) bin/Exe/$(TARGET)