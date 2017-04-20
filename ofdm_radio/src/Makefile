# Makefile for U1 application
#
# Distribution Statement “A” (Approved for Public Release, Distribution Unlimited)
#
##################################################

CXX 				:= g++
CXXFLAGS			:= -I./ -I../src_reusable/ -I/root/vt_radios/dependencies/liquid-usrp/ -I/root/vt_radios/dependencies/liquid-dsp -O2 -g3 -Wall -pedantic -ansi  -fPIC  -std=c++0x 
LIBS				:= -lc -lconfig -lfftw3f -lliquid -lm -lpthread -luhd -lliquidusrp
LDFLAGS             := -L/opt/SDR/XSeries/lib
RM				:= rm -f
BINS				:= U4

CC_OBJS_MAIN 		:= main.o 
CC_OBJS_APP		:= AppManager.o ../src_reusable/Logger.o ../src_reusable/RadioConfig.o
CC_OBJS_PHY		:= FhSeqGenerator.o FreqTableGenerator.o RadioHardwareConfig.o RadioScheduler.o RadioTaskManager.o
CC_OBJS_MAC		:= Phy2Mac.o
CC_OBJS_NET		:= ../src_reusable/PacketStore.o  ../src_reusable/RxPayload.o  ../src_reusable/TunTap.o ../src_reusable/TxPayload.o

#CC_OBJS			:= $(CC_OBJS_MAIN) $(CC_OBJS_APP) $(CC_OBJS_PHY) $(CC_OBJS_MAC) 
CC_OBJS		:= $(CC_OBJS_MAIN) $(CC_OBJS_APP) $(CC_OBJS_PHY) $(CC_OBJS_MAC) $(CC_OBJS_NET)


U4 : $(CC_OBJS)
	$(CXX) $(CXXFLAGS) $(CC_OBJS) $(LIBS) $(LDFLAGS)   -o $@

$(CC_OBJS) : %.o : %.cc


.PHONY : clean
clean :
	$(RM) *.o $(BINS) *.log ../src_reusable/*.o
