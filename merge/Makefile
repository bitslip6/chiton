.DEFAULT_GOAL := fb
CC=g++
CPPFLAGS = -O3 -Wall -Wextra -Wno-effc++ -std=gnu++2a -I/opt/glog/include -I/home/pi/workspace/github/aditof_sdk/sdk/include
CFLAGS = -O3 -Wall -Wextra -pedantic
LDFLAGS = -O3 -L/opt/glog/lib -L/home/pi/workspace/github/aditof_sdk/build/sdk/
LIBS = -lm -lglog -lpng -lwiringPi -lpthread -laditof
OBJS =  m4.o fb_driver.o input.o system.o

%.o: %.cpp
	$(CC) -c $(CPPFLAGS) -o $@ $<

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $< 

# aditof uses weird passing conventions ....
#main.o: main.c
#	$(CC) -c $(CPPFLAGS) -o $@ $< $(CFLAGS)


fb: $(OBJS)
	$(CC) $(LDFLAGS) -o fb $(OBJS) $(LIBS) 


clean:
	rm -f $(OBJS)
	rm -f fb
