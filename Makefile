.DEFAULT_GOAL := m3
CC=g++
CPPFLAGS = -O3 -Wall -Wextra -Wno-effc++ -std=gnu++2a -I/opt/glog/include -I/home/pi/workspace/github/aditof_sdk/sdk/include
CFLAGS = -O3 -Wall -Wextra -pedantic
#LDFLAGS = -O3 -L/opt/glog/lib -L/home/pi/workspace/github/aditof_sdk/build/sdk/
LDFLAGS = -O3 -L/opt/glog/lib -L/usr/local/lib
LIBS = -lm -lglog -lpng -lwiringPi -lpthread -laditof
OBJS = m4.o m5.o borg_driver.o fb_driver.o input.o system.o objects.o
OBJS2 = borg_driver.o fb_driver.o input.o system.o objects.o

%.o: %.cpp
	$(CC) -c $(CPPFLAGS) -o $@ $<

fb_driver.o: fb_driver.c
	$(CC) -c $(CPPFLAGS) -Wno-pedantic -o $@ $<

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $< 

# aditof uses weird passing conventions ....
#main.o: main.c
#	$(CC) -c $(CPPFLAGS) -o $@ $< $(CFLAGS)


m3: $(OBJS)
	$(CC) $(LDFLAGS) -o m3 $(OBJS2) m4.o $(LIBS) 

m5: $(OBJS)
	$(CC) $(LDFLAGS) -o m5 $(OBJS2) m5.o $(LIBS) 



clean:
	rm -f $(OBJS) $(OBJS2)
	rm -f m3
