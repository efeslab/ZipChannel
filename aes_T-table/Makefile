all: spy

spy: spy.cpp 
	g++ -std=gnu++11 -O2 -o $@ $< -Iopenssl -L./ -lcrypto -g -ggdb

clean:
	rm -f spy
