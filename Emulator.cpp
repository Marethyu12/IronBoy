#include "Config.h"
#include "Emulator.h"

#include <algorithm>

#define VERTICAL_BLANK_SCAN_LINE 0x90
#define VERTICAL_BLANK_SCAN_LINE_MAX 0x99
#define RETRACE_START 456

//////////////////////////////////////////////////////////////////

Emulator::Emulator(void) :
    m_GameLoaded(false)
    ,m_CyclesThisUpdate(0)
    ,m_UsingMBC1(false)
    ,m_EnableRamBank(false)
    ,m_UsingMemoryModel16_8(true)
    ,m_EnableInterupts(false)
    ,m_PendingInteruptDisabled(false)
    ,m_PendingInteruptEnabled(false)
    ,m_RetraceLY(RETRACE_START)
    ,m_JoypadState(0)
    ,m_Halted(false)
    ,m_TimerVariable(0)
    ,m_CurrentClockSpeed(1024)
    ,m_DividerVariable(0)
    ,m_CurrentRamBank(0)
    ,m_DebugPause(false)
    ,m_DebugPausePending(false)
    ,m_TimeToPause(NULL)
    ,m_TotalOpcodes(0)
    ,m_DoLogging(false) {
    ResetScreen( );
}

//////////////////////////////////////////////////////////////////

Emulator::~Emulator(void) {
    for (std::vector<BYTE*>::iterator it = m_RamBank.begin(); it != m_RamBank.end(); it++)
        delete[] (*it) ;
}

//////////////////////////////////////////////////////////////////

bool Emulator::LoadRom(const std::string& romName) {
    if (m_GameLoaded)
        StopGame( );

    m_GameLoaded = true ;

    memset(m_Rom,0,sizeof(m_Rom)) ;
    memset(m_GameBank,0,sizeof(m_GameBank)) ;

    FILE *in;
    in = fopen( romName.c_str(), "rb" );
    fread(m_GameBank, 1, 0x200000, in);
    fclose(in);

    memcpy(&m_Rom[0x0], &m_GameBank[0], 0x8000) ; // this is read only and never changes

    m_CurrentRomBank = 1;
    m_DoLogging = false;

    /* if you want boot ROM on startup */
    //m_BootMode = true;

    /* no boot ROM on startup */
    m_BootMode = false;
    ResetCPU();

    return true ;
}

//////////////////////////////////////////////////////////////////

void Emulator::SetRenderFunc(RenderFunc func) {
    m_RenderFunc = func;
}

//////////////////////////////////////////////////////////////////

void Emulator::ResetScreen( ) {
    m_ScreenData.resize(160 * 144 * 4);
    std::fill(m_ScreenData.begin(), m_ScreenData.end(), 0);
}

//////////////////////////////////////////////////////////////////

bool Emulator::ResetCPU( ) {
    ResetScreen( ) ;
    m_CurrentRamBank = 0 ;
    m_TimerVariable = 0 ;
    m_CurrentClockSpeed = 1024 ;
    m_DividerVariable = 0 ;
    m_Halted = false ;
    m_TotalOpcodes = 0 ;
    m_JoypadState = 0xFF ;
    m_CyclesThisUpdate = 0 ;
    m_ProgramCounter = 0x100 ;
    m_RegisterAF.hi = 0x1;
    m_RegisterAF.lo = 0xB0 ;
    m_RegisterBC.reg = 0x0013 ;
    m_RegisterDE.reg = 0x00D8 ;
    m_RegisterHL.reg = 0x014D ;
    m_StackPointer.reg = 0xFFFE ;

    m_Rom[0xFF00] = 0xFF ;
    m_Rom[0xFF05] = 0x00   ;
    m_Rom[0xFF06] = 0x00   ;
    m_Rom[0xFF07] = 0x00   ;
    m_Rom[0xFF10] = 0x80   ;
    m_Rom[0xFF11] = 0xBF   ;
    m_Rom[0xFF12] = 0xF3   ;
    m_Rom[0xFF14] = 0xBF   ;
    m_Rom[0xFF16] = 0x3F   ;
    m_Rom[0xFF17] = 0x00   ;
    m_Rom[0xFF19] = 0xBF   ;
    m_Rom[0xFF1A] = 0x7F   ;
    m_Rom[0xFF1B] = 0xFF   ;
    m_Rom[0xFF1C] = 0x9F   ;
    m_Rom[0xFF1E] = 0xBF   ;
    m_Rom[0xFF20] = 0xFF   ;
    m_Rom[0xFF21] = 0x00   ;
    m_Rom[0xFF22] = 0x00   ;
    m_Rom[0xFF23] = 0xBF   ;
    m_Rom[0xFF24] = 0x77   ;
    m_Rom[0xFF25] = 0xF3   ;
    m_Rom[0xFF26] = 0xF1	;
    m_Rom[0xFF40] = 0x91   ;
    m_Rom[0xFF42] = 0x00   ;
    m_Rom[0xFF43] = 0x00   ;
    m_Rom[0xFF45] = 0x00   ;
    m_Rom[0xFF47] = 0xFC   ;
    m_Rom[0xFF48] = 0xFF   ;
    m_Rom[0xFF49] = 0xFF   ;
    m_Rom[0xFF4A] = 0x00   ;
    m_Rom[0xFF4B] = 0x00   ;
    m_Rom[0xFFFF] = 0x00   ;
    m_RetraceLY = RETRACE_START ;

    m_DebugValue = m_Rom[0x40] ;

    m_EnableRamBank = false ;

    m_UsingMBC2 = false;

    // what kinda rom switching are we using, if any?
    switch(ReadMemory(0x147)) {
    case 0:
        m_UsingMBC1 = false ;
        break ; // not using any memory swapping
    case 1:
    case 2:
    case 3 :
        m_UsingMBC1 = true ;
        break ;
    case 5 :
        m_UsingMBC2 = true ;
        break ;
    case 6 :
        m_UsingMBC2 = true ;
        break ;
    default:
        return false ; // unhandled memory swappping, probably MBC2
    }

    // how many ram banks do we neeed, if any?
    int numRamBanks = 0 ;
    switch (ReadMemory(0x149)) {
    case 0:
        numRamBanks = 0 ;
        break ;
    case 1:
        numRamBanks = 1 ;
        break ;
    case 2:
        numRamBanks = 1 ;
        break ;
    case 3:
        numRamBanks = 4 ;
        break ;
    case 4:
        numRamBanks = 16 ;
        break ;
    }

    CreateRamBanks(numRamBanks) ;

    return true ;

}

//////////////////////////////////////////////////////////////////

static int hack = 0 ;
static long long counter9 = 0 ;

// remember this update function is not the same as the virtual update function. This gets specifically
// called by the Game::Update function. This way I have control over when to execute the next opcode. Mainly for the debug window
void Emulator::Update( ) {
    hack++ ;

    m_CyclesThisUpdate = 0 ;
    const int m_TargetCycles = 70221 ;

    while ((m_CyclesThisUpdate < m_TargetCycles)) { //||(ReadMemory(0xFF44) < 144))
        if (m_DebugPause)
            return ;
        if (m_DebugPausePending) {
            if ( m_TimeToPause && (m_TimeToPause() == true)) {
                m_DebugPausePending = false ;
                m_DebugPause = true ;
                return ;
            }
        }

        int currentCycle = m_CyclesThisUpdate ;
        BYTE opcode = ExecuteNextOpcode();
        int cycles = m_CyclesThisUpdate - currentCycle ;

        DoTimers(cycles);
        DoGraphics(cycles);
        DoInterupts();
    }

    counter9 += m_CyclesThisUpdate ;
    m_RenderFunc() ;
}

//////////////////////////////////////////////////////////////////

void Emulator::DoGraphics(int cycles) {
    SetLCDStatus();

    // is LCD enabled?
    if (TestBit(ReadMemory(0xFF40), 7)) {
        m_RetraceLY -= cycles; // keep track of cycles accumulated
    }

    if (m_Rom[0xFF44] > VERTICAL_BLANK_SCAN_LINE_MAX) {
        m_Rom[0xFF44] = 0; // reset
    }

    if (m_RetraceLY <= 0) {
        DrawCurrentLine();
    }
}

//////////////////////////////////////////////////////////////////

BYTE Emulator::ExecuteNextOpcode( ) {
    BYTE opcode = m_BootMode ? bootROM[m_ProgramCounter] : m_Rom[m_ProgramCounter];

    if ((m_ProgramCounter >= 0x4000 && m_ProgramCounter <= 0x7FFF) || (m_ProgramCounter >= 0xA000 && m_ProgramCounter <= 0xBFFF))
        opcode = ReadMemory(m_ProgramCounter);

    if (!m_Halted) {
        if (false) {
            char buffer[200] ;
            sprintf(buffer, "OP = %x PC = %x\n", opcode, m_ProgramCounter) ;
            LogMessage::GetSingleton()->DoLogMessage(buffer,false) ;
        }

        m_ProgramCounter++ ;
        m_TotalOpcodes++ ;

        ExecuteOpcode(opcode);
    } else {
        m_CyclesThisUpdate += 4;
    }

    // we are trying to disable interupts, however interupts get disabled after the next instruction
    // 0xF3 is the opcode for disabling interupt
    if (m_PendingInteruptDisabled) {
        if (ReadMemory(m_ProgramCounter-1) != 0xF3) {
            m_PendingInteruptDisabled = false ;
            m_EnableInterupts = false ;
        }
    }

    if (m_PendingInteruptEnabled) {
        if (ReadMemory(m_ProgramCounter-1) != 0xFB) {
            m_PendingInteruptEnabled = false ;
            m_EnableInterupts = true ;
        }
    }

    // blarggs test - serial output
    if (m_Rom[0xFF02] == 0x81) {
        char c = m_Rom[0xFF01];
        LogMessage::GetSingleton()->LogCharacter(c);
        m_Rom[0xFF02] = 0x0;
    }

    return opcode ;

}

//////////////////////////////////////////////////////////////////

void Emulator::StopGame( ) {
    m_GameLoaded = false ;
}

//////////////////////////////////////////////////////////////////

std::string Emulator::GetCurrentOpcode( ) const {
    return std::string("%x", m_Rom[m_ProgramCounter]) ;
}

//////////////////////////////////////////////////////////////////


std::string Emulator::GetImmediateData1( ) const {
    return std::string("%x", m_Rom[m_ProgramCounter+1]) ;
}

//////////////////////////////////////////////////////////////////

std::string Emulator::GetImmediateData2( ) const {
    return std::string("%x", m_Rom[m_ProgramCounter+2]) ;
}

//////////////////////////////////////////////////////////////////

// all reading of rom should go through here so I can trap it.
BYTE Emulator::ReadMemory(WORD memory)const {
    if (m_BootMode && memory <= 0xFF) {
        return bootROM[memory];
    }

    // reading from rom bank
    if (memory >= 0x4000 && memory <= 0x7FFF) {
        unsigned int newAddress = memory ;
        newAddress += ((m_CurrentRomBank-1)*0x4000) ;
        return m_GameBank[newAddress];
    }

    // reading from RAM Bank
    else if (memory >= 0xA000 && memory <= 0xBFFF) {
        WORD newAddress = memory - 0xA000 ;
        return m_RamBank.at(m_CurrentRamBank)[newAddress] ;
    }
    // trying to read joypad state
    else if (memory == 0xFF00)
        return GetJoypadState( );

    return m_Rom[memory];
}

//////////////////////////////////////////////////////////////////

WORD Emulator::ReadWord( ) const {
    WORD res = ReadMemory(m_ProgramCounter+1) ;
    res = res << 8 ;
    res |= ReadMemory(m_ProgramCounter) ;
    return res ;
}

//////////////////////////////////////////////////////////////////

WORD Emulator::ReadLSWord( ) const {
    WORD res = ReadMemory(m_ProgramCounter+1) ;
    res = res << 8 ;
    res |= ReadMemory(m_ProgramCounter) ;
    return res ;
}

//////////////////////////////////////////////////////////////////

// writes a byte to memory. Remember that address 0 - 07FFF is rom so we cant write to this address
void Emulator::WriteByte(WORD address, BYTE data) {
    if (m_BootMode && address == 0xFF50) {
        m_BootMode = false;
        ResetCPU();
    }

    // writing to memory address 0x0 to 0x1FFF this disables writing to the ram bank. 0 disables, 0xA enables
    if (address <= 0x1FFF) {
        if (m_UsingMBC1) {
            if ((data & 0xF) == 0xA)
                m_EnableRamBank = true ;
            else if (data == 0x0)
                m_EnableRamBank = false ;
        } else if (m_UsingMBC2) {
            //bit 0 of upper byte must be 0
            if (false == TestBit(address,8)) {
                if ((data & 0xF) == 0xA)
                    m_EnableRamBank = true ;
                else if (data == 0x0)
                    m_EnableRamBank = false ;
            }
        }

    }

    // if writing to a memory address between 2000 and 3FFF then we need to change rom bank
    else if ( (address >= 0x2000) && (address <= 0x3FFF) ) {
        if (m_UsingMBC1) {
            if (data == 0x00)
                data++;

            data &= 31;

            // Turn off the lower 5-bits.
            m_CurrentRomBank &= 224;

            // Combine the written data with the register.
            m_CurrentRomBank |= data;

            if (false) {
                char buffer[256] ;
                sprintf(buffer, "Chaning Rom Bank to %d", m_CurrentRomBank) ;
                LogMessage::GetSingleton()->DoLogMessage(buffer, false) ;
            }

        } else if (m_UsingMBC2) {
            data &= 0xF ;
            m_CurrentRomBank = data ;
        }
    }

    // writing to address 0x4000 to 0x5FFF switches ram banks (if enabled of course)
    else if ( (address >= 0x4000) && (address <= 0x5FFF)) {
        if (m_UsingMBC1) {
            // are we using memory model 16/8
            if (m_UsingMemoryModel16_8) {
                // in this mode we can only use Ram Bank 0
                m_CurrentRamBank = 0 ;

                data &= 3;
                data <<= 5;

                if ((m_CurrentRomBank & 31) == 0) {
                    data++;
                }

                // Turn off bits 5 and 6, and 7 if it somehow got turned on.
                m_CurrentRomBank &= 31;

                // Combine the written data with the register.
                m_CurrentRomBank |= data;

                if (false) {
                    char buffer[256] ;
                    sprintf(buffer, "Chaning Rom Bank to %d", m_CurrentRomBank) ;
                    LogMessage::GetSingleton()->DoLogMessage(buffer, false) ;
                }

            } else {
                m_CurrentRamBank = data & 0x3 ;

                if (false) {
                    char buffer[256] ;
                    sprintf(buffer, "=====Chaning Ram Bank to %d=====", m_CurrentRamBank) ;
                    LogMessage::GetSingleton()->DoLogMessage(buffer, false) ;
                }
            }
        }
    }

    // writing to address 0x6000 to 0x7FFF switches memory model
    else if ( (address >= 0x6000) && (address <= 0x7FFF)) {
        if (m_UsingMBC1) {
            // we're only interested in the first bit
            data &= 1 ;
            if (data == 1) {
                m_CurrentRamBank = 0 ;
                m_UsingMemoryModel16_8 = false ;
            } else
                m_UsingMemoryModel16_8 = true ;
        }
    }

    // from now on we're writing to RAM

    else if ((address >= 0xA000) && (address <= 0xBFFF)) {
        if (m_EnableRamBank) {
            if (m_UsingMBC1) {
                WORD newAddress = address - 0xA000 ;
                m_RamBank.at(m_CurrentRamBank)[newAddress] = data;
            }
        } else if (m_UsingMBC2 && (address < 0xA200)) {
            WORD newAddress = address - 0xA000 ;
            m_RamBank.at(m_CurrentRamBank)[newAddress] = data;
        }

    }


    // we're right to internal RAM, remember that it needs to echo it
    else if ( (address >= 0xC000) && (address <= 0xDFFF) ) {
        m_Rom[address] = data ;
    }

    // echo memory. Writes here and into the internal ram. Same as above
    else if ( (address >= 0xE000) && (address <= 0xFDFF) ) {
        m_Rom[address] = data ;
        m_Rom[address -0x2000] = data ; // echo data into ram address
    }

    // This area is restricted.
    else if ((address >= 0xFEA0) && (address <= 0xFEFF)) {
    }

    // reset the divider register
    else if (address == 0xFF04) {
        m_Rom[0xFF04] = 0 ;
        m_DividerVariable = 0 ;
    }

    // not sure if this is correct
    else if (address == 0xFF07) {
        m_Rom[address] = data ;

        int timerVal = data & 0x03 ;

        int clockSpeed = 0 ;

        switch(timerVal) {
        case 0:
            clockSpeed = 1024 ;
            break ;
        case 1:
            clockSpeed = 16;
            break ;
        case 2:
            clockSpeed = 64 ;
            break ;
        case 3:
            clockSpeed = 256 ;
            break ; // 256
        default:
            assert(false);
            break ; // weird timer val
        }

        if (clockSpeed != m_CurrentClockSpeed) {
            m_TimerVariable = 0 ;
            m_CurrentClockSpeed = clockSpeed ;
        }
    }


    // FF44 shows which horizontal scanline is currently being draw. Writing here resets it
    else if (address == 0xFF44) {
        m_Rom[0xFF44] = 0 ;
    }

    else if (address == 0xFF45) {
        m_Rom[address] = data ;
    }
    // DMA transfer
    else if (address == 0xFF46) {
        WORD newAddress = (data << 8) ;
        for (int i = 0; i < 0xA0; i++) {
            m_Rom[0xFE00 + i] = ReadMemory(newAddress + i);
        }
    }

    // This area is restricted.
    else if ((address >= 0xFF4C) && (address <= 0xFF7F)) {
    }


    // I guess we're ok to write to memory... gulp
    else {
        m_Rom[address] = data ;
    }
}

//////////////////////////////////////////////////////////////////

int Emulator::GetCarryFlag( ) const {
    if (TestBit(m_RegisterAF.lo, FLAG_C))
        return 1 ;

    return 0 ;
}

//////////////////////////////////////////////////////////////////

int Emulator::GetZeroFlag( ) const {
    if (TestBit(m_RegisterAF.lo, FLAG_Z))
        return 1 ;

    return 0 ;
}

//////////////////////////////////////////////////////////////////

int Emulator::GetHalfCarryFlag( ) const {
    if (TestBit(m_RegisterAF.lo, FLAG_H))
        return 1 ;

    return 0 ;
}

//////////////////////////////////////////////////////////////////

int Emulator::GetSubtractFlag( ) const {
    if (TestBit(m_RegisterAF.lo, FLAG_N))
        return 1 ;

    return 0 ;
}

//////////////////////////////////////////////////////////////////

static int vblankcount = 0 ;

void Emulator::IssueVerticalBlank( ) {
    vblankcount++ ;
    RequestInterupt(0) ;
    if (hack == 60) {
        //OutputDebugStr(STR::Format("Total VBlanks was: %d\n", vblankcount)) ;
        vblankcount = 0 ;
    }
}

//////////////////////////////////////////////////////////////////

void Emulator::DrawCurrentLine() {
    // LCD must be enabled
    if (!TestBit(ReadMemory(0xFF40), 7)) {
        return;
    }

    m_Rom[0xFF44]++; // increment LY
    m_RetraceLY = RETRACE_START; // draw another line if this counter reaches 0

    BYTE Ly = ReadMemory(0xFF44);

    // V-Blank occurs if LY is between 144 and 153
    if (Ly == VERTICAL_BLANK_SCAN_LINE) {
        IssueVerticalBlank();
    }

    if (Ly > VERTICAL_BLANK_SCAN_LINE_MAX) {
        m_Rom[0xFF44] = 0;
    }

    if (Ly < VERTICAL_BLANK_SCAN_LINE) {
        DrawScanLine();
    }
}

//////////////////////////////////////////////////////////////////

void Emulator::PushWordOntoStack(WORD word) {
    BYTE hi = word >> 8 ;
    BYTE lo = word & 0xFF;
    m_StackPointer.reg-- ;
    WriteByte(m_StackPointer.reg, hi) ;
    m_StackPointer.reg-- ;
    WriteByte(m_StackPointer.reg, lo) ;
}

//////////////////////////////////////////////////////////////////

WORD Emulator::PopWordOffStack( ) {
    WORD word = ReadMemory(m_StackPointer.reg+1) << 8 ;
    word |= ReadMemory(m_StackPointer.reg) ;
    m_StackPointer.reg+=2 ;

    return word ;
}

//////////////////////////////////////////////////////////////////

void Emulator::DoInterupts( ) {
    // are interrupts enabled
    if (m_EnableInterupts) {
        // has anything requested an interrupt?
        BYTE requestFlag = ReadMemory(0xFF0F);
        if (requestFlag > 0) {
            // which requested interrupt has the lowest priority?
            for (int bit = 0; bit < 8; bit++) {
                if (TestBit(requestFlag, bit)) {
                    // this interupt has been requested. But is it enabled?
                    BYTE enabledReg = ReadMemory(0xFFFF);
                    if (TestBit(enabledReg,bit)) {
                        // yup it is enabled, so lets DOOOOO ITTTTT
                        ServiceInterrupt(bit) ;
                    }
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////

void Emulator::ServiceInterrupt( int num ) {
    // save current program counter
    PushWordOntoStack(m_ProgramCounter) ;
    m_Halted = false ;

    if (false) {
        char buffer[200] ;
        sprintf(buffer, "servicing interupt %d", num) ;
        LogMessage::GetSingleton()->DoLogMessage(buffer, false) ;
    }

//	unsigned long long limit =(8000000);
//	if (m_TotalOpcodes > limit)
//		LOGMESSAGE(Logging::MSG_INFO, STR::Format("Servicing interrupt %d", num).ConstCharPtr() ) ;

    switch(num) {
    case 0:
        m_ProgramCounter = 0x40 ;
        break ;// V-Blank
    case 1:
        m_ProgramCounter = 0x48 ;
        break ;// LCD-STATE
    case 2:
        m_ProgramCounter = 0x50 ;
        break ;// Timer
    case 4:
        m_ProgramCounter = 0x60 ;
        break ;// JoyPad
    default:
        assert(false) ;
        break ;
    }

    m_EnableInterupts = false ;
    m_Rom[0xFF0F] = BitReset(m_Rom[0xFF0F], num) ;
}

//////////////////////////////////////////////////////////////////

void Emulator::DrawScanLine() {
    BYTE LCDControl = ReadMemory(0xFF40);

    // LCD must be enabled
    if (TestBit(LCDControl, 7)) {
        RenderBackground(LCDControl);
        RenderSprites(LCDControl);
    }
}

//////////////////////////////////////////////////////////////////

void Emulator::RenderBackground(BYTE LCDControl) {
    // lets draw the background (however it does need to be enabled)
    if (!TestBit(LCDControl, 0)) {
        return;
    }

    BYTE ScY = ReadMemory(0xFF42);
    BYTE ScX = ReadMemory(0xFF43);
    BYTE WndY = ReadMemory(0xFF4A);
    BYTE WndX = ReadMemory(0xFF4B) - 7;
    BYTE Ly = ReadMemory(0xFF44);

    WORD wndTileMem = TestBit(LCDControl, 4) ? 0x8000 : 0x8800;

    bool _signed = wndTileMem == 0x8800;
    bool usingWnd = TestBit(LCDControl, 5) && WndY <= Ly; // true if window display is enabled (specified in LCDC) and WndY <= LY

    WORD bkgdTileMem = TestBit(LCDControl, usingWnd ? 6 : 3) ? 0x9C00 : 0x9800;

    // the y-position is used to determine which of 32 (256 / 8) vertical tiles will used (background map y)
    BYTE yPos = !usingWnd ? ScY + Ly : Ly - WndY; // map to window coordinates if necessary
    WORD tileRow = ((BYTE) (yPos / 8)) * 32; // which of 8 vertical pixels of the current tile is the scanline on?

    // time to draw a scanline which consists of 160 pixels
    for (int pixel = 0; pixel < 160; pixel++) {
        BYTE xPos = usingWnd && pixel >= WndX ? pixel - WndX : ScX + pixel;
        WORD tileCol = xPos / 8; // which of horizontal pixels of the current tile does xPos fall in?
        WORD tileAddr = bkgdTileMem + tileRow + tileCol;
        SIGNED_WORD tileNum = _signed ? (SIGNED_BYTE) ReadMemory(tileAddr) : (BYTE) ReadMemory(tileAddr);
        WORD tileLocation = wndTileMem + (_signed ? (tileNum + 128) * 16 : tileNum * 16);

        // determine the correct row of pixels
        BYTE line = yPos % 8;
        line *= 2; // each line needs 2 bytes of memory so...

        BYTE data1 = ReadMemory(tileLocation + line);
        BYTE data2 = ReadMemory(tileLocation + line + 1);

        int bit = 7 - (xPos % 8); // bit position in data1 and data2 (because pixel 0 corresponds to bit 7 and pixel 1 corresponds to bit 6 and so on)
        int colourNum = (BitGetVal(data2, bit) << 1) | BitGetVal(data1, bit);

        COLOUR col = GetColour(colourNum, 0xFF47);
        int red;
        int green;
        int blue;

        switch (col) {
        case LIGHTEST_GREEN:
            red = 155;
            green = 188;
            blue = 15;
            break;
        case LIGHT_GREEN:
            red = 139;
            green = 172;
            blue = 15;
            break;
        case DARK_GREEN:
            red = 48;
            green = 98;
            blue = 48;
            break;
        case DARKEST_GREEN:
            red = 15;
            green = 56;
            blue = 15;
            break;
        }

        if ((Ly < 0) || (Ly > 143) || (pixel < 0) || (pixel > 159)) {
            assert(false);
            continue;
        }

        int offset = Ly * 160 * 4 + pixel * 4;

        m_ScreenData[offset] = blue;
        m_ScreenData[offset + 1] = green;
        m_ScreenData[offset + 2] = red;
        m_ScreenData[offset + 3] = 255;
    }
}

//////////////////////////////////////////////////////////////////

void Emulator::RenderSprites(BYTE LCDControl) {
    // lets draw the sprites (however it does need to be enabled)
    if (!TestBit(LCDControl, 1)) {
        return;
    }

    bool use8x16 = TestBit(LCDControl, 2); // determine the sprite size

    // the sprite layer can display up to 40 sprites
    for (int i = 0; i < 40; i++) {
        int index = i * 4; // each sprite takes 4 bytes of OAM space (0xFE00-0xFE9F)

        BYTE spriteY = ReadMemory(0xFE00 + index) - 16;
        BYTE spriteX = ReadMemory(0xFE00 + index + 1) - 8;
        BYTE patternNumber = ReadMemory(0xFE00 + index + 2);
        BYTE attributes = ReadMemory(0xFE00 + index + 3);

        bool xFlip = TestBit(attributes, 5);
        bool yFlip = TestBit(attributes, 6);

        int Ly = ReadMemory(0xFF44);
        int height = use8x16 ? 16 : 8;

        // does the scanline intercept this sprite?
        if ((Ly >= spriteY) && (Ly < (spriteY + height))) {
            int line = Ly - spriteY;
            if (yFlip) {
                line = height - line; // read backwards if y flipping is allowed
            }
            line *= 2; // each line takes 2 bytes of memory

            WORD tileLocation = 0x8000 + (patternNumber * 16);

            BYTE data1 = ReadMemory(tileLocation + line);
            BYTE data2 = ReadMemory(tileLocation + line + 1);

            for (int tilePixel = 7; tilePixel >= 0; tilePixel--) {
                int bit = tilePixel;
                if (xFlip) {
                    bit = 7 - bit; // read backwards if x flipping is allowed
                }

                int colourNum = (BitGetVal(data2, bit) << 1) | BitGetVal(data1, bit);
                COLOUR col = GetColour(colourNum, TestBit(attributes, 4) ? 0xFF49 : 0xFF48);

                // it's transparent for sprites
                if (col == LIGHTEST_GREEN) {
                    continue;
                }

                int red;
                int green;
                int blue;

                switch (col) {
                case LIGHT_GREEN:
                    red = 139;
                    green = 172;
                    blue = 15;
                    break;
                case DARK_GREEN:
                    red = 48;
                    green = 98;
                    blue = 48;
                    break;
                case DARKEST_GREEN:
                    red = 15;
                    green = 56;
                    blue = 15;
                    break;
                }

                int pixel = spriteX + (7 - tilePixel);

                if ((Ly < 0) || (Ly > 143) || (pixel < 0) || (pixel > 159)) {
                    continue;
                }

                int offset = Ly * 160 * 4 + pixel * 4;

                // check if pixel is hidden behind background
                // if the bit 7 of attributes is set then the screen pixel colour must be lightest green before changing colour
                if (TestBit(attributes, 7) && ((m_ScreenData[offset + 2] != 155) || (m_ScreenData[offset + 1] != 188) || (m_ScreenData[offset] != 15))) {
                    continue;
                }

                m_ScreenData[offset] = blue;
                m_ScreenData[offset + 1] = green;
                m_ScreenData[offset + 2] = red;
                m_ScreenData[offset + 3] = 255;
            }
        }
    }
}

//////////////////////////////////////////////////////////////////

Emulator::COLOUR Emulator::GetColour(BYTE colourNum, WORD address) const {
    COLOUR res = LIGHTEST_GREEN;
    BYTE palette = ReadMemory(address);
    int hi = 0;
    int lo = 0;

    switch (colourNum) {
    case 0:
        hi = 1;
        lo = 0;
        break;
    case 1:
        hi = 3;
        lo = 2;
        break;
    case 2:
        hi = 5;
        lo = 4;
        break;
    case 3:
        hi = 7;
        lo = 6;
        break;
    default:
        assert(false);
        break;
    }

    int colour = 0;
    colour = BitGetVal(palette, hi) << 1;
    colour |= BitGetVal(palette, lo);

    switch (colour) {
    case 0:
        res = LIGHTEST_GREEN;
        break;
    case 1:
        res = LIGHT_GREEN;
        break;
    case 2:
        res = DARK_GREEN;
        break;
    case 3:
        res = DARKEST_GREEN;
        break;
    default:
        assert(false) ;
        break;
    }

    return res;
}

//////////////////////////////////////////////////////////////////

void Emulator::SetLCDStatus() {
    BYTE LCDStatus = ReadMemory(0xFF41);

    // if LCD is turned off
    if (!TestBit(ReadMemory(0xFF40), 7)) {
        m_RetraceLY = RETRACE_START;
        m_Rom[0xFF44] = 0;

        // LCD status must be in mode 1 when LCD is disabled
        LCDStatus = BitSet(LCDStatus, 0);
        LCDStatus = BitReset(LCDStatus, 1);

        WriteByte(0xFF41, LCDStatus);
        return;
    }

    BYTE Ly = ReadMemory(0xFF44);
    BYTE oldMode = GetLCDMode();

    BYTE mode = 0;
    bool reqInt = false;

    // if in the V-Blank period
    if (Ly >= VERTICAL_BLANK_SCAN_LINE) {
        mode = 1;

        // set to mode 1
        LCDStatus = BitSet(LCDStatus, 0);
        LCDStatus = BitReset(LCDStatus, 1);

        reqInt = TestBit(LCDStatus, 4); // is mode 1 V-Blank interrupt enabled?
    } else {
        int mode2Bounds = RETRACE_START - 80;
        int mode3Bounds = mode2Bounds - 172;

        if (m_RetraceLY >= mode2Bounds) {
            mode = 2;

            // set to mode 2
            LCDStatus = BitReset(LCDStatus, 0);
            LCDStatus = BitSet(LCDStatus, 1);

            reqInt = TestBit(LCDStatus, 5); // is mode 2 OAM interrupt enabled?
        } else if (m_RetraceLY >= mode3Bounds) {
            mode = 3;

            // set to mode 3
            LCDStatus = BitSet(LCDStatus, 0);
            LCDStatus = BitSet(LCDStatus, 1);
        } else {
            mode = 0;

            // set to mode 0
            LCDStatus = BitReset(LCDStatus, 0);
            LCDStatus = BitReset(LCDStatus, 1);

            reqInt = TestBit(LCDStatus, 3); // is mode 0 H-Blank interrupt enabled?
        }
    }

    if (reqInt && mode != oldMode) {
        RequestInterupt(1); // request LCDStat interrupt
    }

    BYTE Lyc = ReadMemory(0xFF45);

    if (Lyc == Ly) {
        LCDStatus = BitSet(LCDStatus, 2);

        // is LYC=LY coincidence interrupt enabled?
        if (TestBit(LCDStatus, 6)) {
            RequestInterupt(1); // then request LCDStat interrupt
        }
    } else {
        LCDStatus = BitReset(LCDStatus, 2);
    }

    WriteByte(0xFF41, LCDStatus);
}

//////////////////////////////////////////////////////////////////

BYTE Emulator::GetJoypadState( ) const {
    // this function CANNOT call ReadMemory(0xFF00) it must access it directly from m_Rom[0xFF00]
    // because ReadMemory traps this address

    BYTE res = m_Rom[0xFF00] ;
    res ^= 0xFF ;

    if (!TestBit(res, 4)) {
        BYTE topJoypad = m_JoypadState >> 4 ;
        topJoypad |= 0xF0 ;
        res &= topJoypad ;
    } else if (!TestBit(res,5)) {
        BYTE bottomJoypad = m_JoypadState & 0xF ;
        bottomJoypad |= 0xF0 ;
        res &= bottomJoypad ;
    }
    return res ;
}

//////////////////////////////////////////////////////////////////

void Emulator::KeyPressed(int key) {
    // this function CANNOT call ReadMemory(0xFF00) it must access it directly from m_Rom[0xFF00]
    // because ReadMemory traps this address

    bool previouslyUnset = false ;

    if (TestBit(m_JoypadState, key) == false)
        previouslyUnset = true ;

    m_JoypadState = BitReset(m_JoypadState, key) ;

    // button pressed
    bool button = true ;

    if (key > 3)
        button = true ;
    else // directional button pressed
        button = false ;

    BYTE keyReq = m_Rom[0xFF00] ;
    bool requestInterupt = false ;

    // player pressed button and programmer intersted in button
    if (button && !TestBit(keyReq,5)) {
        requestInterupt = true ;
    }
    // player pressed directional and programmer interested
    else if (!button && !TestBit(keyReq,4)) {
        requestInterupt = true ;
    }

    if (requestInterupt && !previouslyUnset) {
        RequestInterupt(4) ;
    }
}

//////////////////////////////////////////////////////////////////

void Emulator::KeyReleased(int key) {
    // this function CANNOT call ReadMemory(0xFF00) it must access it directly from m_Rom[0xFF00]
    // because ReadMemory traps this address

    m_JoypadState = BitSet(m_JoypadState, key) ;
}

//////////////////////////////////////////////////////////////////

static int timerhack = 0 ;

void Emulator::DoTimers( int cycles ) {
    BYTE timerAtts = m_Rom[0xFF07];

    m_DividerVariable += cycles ;

    if (TestBit(timerAtts, 2)) {
        m_TimerVariable += cycles ;

        // time to increment the timer
        if (m_TimerVariable >= m_CurrentClockSpeed) {
            m_TimerVariable = 0 ;
            bool overflow = false ;
            if (m_Rom[0xFF05] == 0xFF) {
                overflow = true ;
            }
            m_Rom[0xFF05]++ ;

            if (overflow) {
                timerhack++ ;

                m_Rom[0xFF05] = m_Rom[0xFF06] ;

                // request the interupt
                RequestInterupt(2) ;
            }
        }
    }

    // do divider register
    if (m_DividerVariable >= 256) {
        m_DividerVariable = 0;
        m_Rom[0xFF04]++ ;
    }
}

//////////////////////////////////////////////////////////////////

void Emulator::CreateRamBanks(int numBanks) {
    // DOES THE FIRST RAM BANK NEED TO BE SET TO THE CONTENTS of m_Rom[0xA000] - m_Rom[0xC000]?
    for (int i = 0; i < 17; i++) {
        BYTE* ram = new BYTE[0x2000] ;
        memset(ram, 0, sizeof(ram)) ;
        m_RamBank.push_back(ram) ;
    }

    for (int i = 0 ; i < 0x2000; i++)
        m_RamBank[0][i] = m_Rom[0xA000+i] ;
}

//////////////////////////////////////////////////////////////////

void Emulator::RequestInterupt(int bit) {
    BYTE requestFlag = ReadMemory(0xFF0F) ;
    requestFlag = BitSet(requestFlag,bit) ;
    WriteByte(0xFF0F, requestFlag) ;
}

//////////////////////////////////////////////////////////////////

BYTE Emulator::GetLCDMode() const {
    BYTE lcdStatus = m_Rom[0xFF41] ;
    return lcdStatus & 0x3 ;
}

//////////////////////////////////////////////////////////////////

