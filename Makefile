CXX = g++
LD = g++
CXXFLAGS := -std=c++11-c -g -O0 -Wall -Wextra -pedantic -lpthread
LDFLAGS := -std=c++11 -lpthread

all : sender_main.o reliable_sender receiver_main.o reliable_receiver encryption.o enc decryption.o dec

.PHONY: clean all

sender_main.o: src/sender_main.cpp src/dataStructure.h
	$(CXX) -c src/sender_main.cpp -std=c++11

receiver_main.o: src/receiver_main.cpp src/dataStructure.h
	$(CXX) -c src/receiver_main.cpp -std=c++11

encryption.o: src/encryption.cpp ssrc/aes.h
	$(CXX) -c src/encryption.cpp -std=c++11

decryption.o: src/decryption.cpp src/aes.h
	$(CXX) -c src/decryption.cpp -std=c++11

reliable_sender : sender_main.o
	$(LD) sender_main.o $(LDFLAGS) -o reliable_sender

reliable_receiver : receiver_main.o 
	$(LD) receiver_main.o $(LDFLAGS) -o reliable_receiver

enc : encryption.o
	$(LD) encryption.o $(LDFLAGS) -o enc

dec : decryption.o
	$(LD) decryption.o $(LDFLAGS) -o dec

obj/%.o : src/%.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<
obj:
	mkdir -p obj

clean:
	@rm -f *.o reliable_sender reliable_receiver
