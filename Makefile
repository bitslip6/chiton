.DEFAULT_GOAL := fb
CC=g++
CPPFLAGS = -Wall -Wextra -Wno-effc++ -pedantic -I/opt/glog/include -I/home/pi/workspace/github/aditof_sdk/sdk/include
LDFLAGS = -O3 -L/opt/glog/lib -L/home/pi/workspace/github/aditof_sdk/build/sdk/
LIBS = -lm -lglog -lpng -lwiringPi -lpthread -laditof
# OBJS = projection.o main.o fb_driver.o adi.o
OBJS = fb_driver.o main.o

%.o: %.cpp
	$(CC) -c $(CPPFLAGS) -o $@ $< $(CFLAGS)

fb_driver.o: fb_driver.cpp
	$(CC) -c $(CPPFLAGS) -std=gnu++2a  -o $@ $< $(CFLAGS)

# projection.cpp has memvers out of order, compiler will fix for us.
projection.o: projection.cpp
	$(CC) -c $(CPPFLAGS) -Wno-reorder -o $@ $< $(CFLAGS)

# aditof uses weird passing conventions ....
adi.o: adi.cpp
	$(CC) -c $(CPPFLAGS) -std=gnu++11 -o $@ $< $(CFLAGS)
main.o: main.cpp
	$(CC) -c $(CPPFLAGS) -std=gnu++11 -o $@ $< $(CFLAGS)


fb: $(OBJS)
	$(CC) $(LDFLAGS) -o fb $(OBJS) $(LIBS) 


clean:
	rm -f $(OBJS)
	rm -f fb
