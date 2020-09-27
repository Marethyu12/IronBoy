CXX = g++
CXXFLAGS =  -mwindows -Wl,-subsystem,windows --machine-windows
LIBS = -lSDL2 -lcomdlg32
SRCS = WinMain.cpp Config.cpp Emulator.cpp Emulator.i8080Cpu.cpp Emulator.JumpTable.cpp GameBoy.cpp GameSettings.cpp LogMessages.cpp
OBJS = $(SRCS:.cpp=.o)
RM = del

EXECUTABLE = IronBoy.exe

all: $(EXECUTABLE)
	@echo Done!

$(EXECUTABLE): $(OBJS)
	$(CXX) -o $(EXECUTABLE) $(CXXFLAGS) $(OBJS) $(LIBS)

.cpp.o:
	$(CXX) -Wall -fmax-errors=5 -c $< -o $@

clean:
	$(RM) *.o $(EXECUTABLE)