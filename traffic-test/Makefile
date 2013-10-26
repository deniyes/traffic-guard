object := main.o conf.o average.o read.o master.o log.o socket.o signal_t.o
objs := demo
cc=gcc
flags := -O0 -Wall -g 
LIBS := -lpthread

main:$(object)
	$(cc) $(flags) $(object) -o $(objs) $(LIBS)
$(object):%o:%c
	$(cc) $(flags) -c $< -o $@ $(LIBS)
.PHONE:clean
clean:
	-rm *.o $(objs)
