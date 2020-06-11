CXX = g++
CXXFLAGS = -Wall -mwindows -Wl,-subsystem,windows
LIBS = -lSDL2 -l:libcomdlg32.a
SRCS = WinMain.cpp Config.cpp Emulator.cpp Emulator.i8080Cpu.cpp Emulator.JumpTable.cpp GameBoy.cpp GameSettings.cpp LogMessages.cpp
OBJS = $(SRCS:.cpp=.o)

EXECUTABLE = IronBoy.exe

all: $(EXECUTABLE)
	@echo Done!

$(EXECUTABLE): $(OBJS)
	$(CXX) -o $(EXECUTABLE) $(OBJS) $(LIBS)

.cpp.o:
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) *.o $(EXECUTABLE)