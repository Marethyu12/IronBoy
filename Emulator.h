#pragma once
#ifndef _EMULATOR_H
#define _EMULATOR_H

#include <vector>

typedef unsigned char BYTE ;
typedef char SIGNED_BYTE ;
typedef unsigned short WORD ;
typedef signed short SIGNED_WORD ;

#define FLAG_MASK_Z 128
#define FLAG_MASK_N 64
#define FLAG_MASK_H 32
#define FLAG_MASK_C 16
#define FLAG_Z 7
#define FLAG_N 6
#define FLAG_H 5
#define FLAG_C 4

typedef bool (*PauseFunc)() ;
typedef void (*RenderFunc)() ;

const int bootROM[] = {
    0x31, 0xFE, 0xFF, 0xAF, 0x21, 0xFF, 0x9F, 0x32, 0xCB, 0x7C, 0x20, 0xFB, 0x21, 0x26, 0xFF, 0x0E,
    0x11, 0x3E, 0x80, 0x32, 0xE2, 0x0C, 0x3E, 0xF3, 0xE2, 0x32, 0x3E, 0x77, 0x77, 0x3E, 0xFC, 0xE0,
    0x47, 0x11, 0x04, 0x01, 0x21, 0x10, 0x80, 0x1A, 0xCD, 0x95, 0x00, 0xCD, 0x96, 0x00, 0x13, 0x7B,
    0xFE, 0x34, 0x20, 0xF3, 0x11, 0xD8, 0x00, 0x06, 0x08, 0x1A, 0x13, 0x22, 0x23, 0x05, 0x20, 0xF9,
    0x3E, 0x19, 0xEA, 0x10, 0x99, 0x21, 0x2F, 0x99, 0x0E, 0x0C, 0x3D, 0x28, 0x08, 0x32, 0x0D, 0x20,
    0xF9, 0x2E, 0x0F, 0x18, 0xF3, 0x67, 0x3E, 0x64, 0x57, 0xE0, 0x42, 0x3E, 0x91, 0xE0, 0x40, 0x04,
    0x1E, 0x02, 0x0E, 0x0C, 0xF0, 0x44, 0xFE, 0x90, 0x20, 0xFA, 0x0D, 0x20, 0xF7, 0x1D, 0x20, 0xF2,
    0x0E, 0x13, 0x24, 0x7C, 0x1E, 0x83, 0xFE, 0x62, 0x28, 0x06, 0x1E, 0xC1, 0xFE, 0x64, 0x20, 0x06,
    0x7B, 0xE2, 0x0C, 0x3E, 0x87, 0xE2, 0xF0, 0x42, 0x90, 0xE0, 0x42, 0x15, 0x20, 0xD2, 0x05, 0x20,
    0x4F, 0x16, 0x20, 0x18, 0xCB, 0x4F, 0x06, 0x04, 0xC5, 0xCB, 0x11, 0x17, 0xC1, 0xCB, 0x11, 0x17,
    0x05, 0x20, 0xF5, 0x22, 0x23, 0x22, 0x23, 0xC9, 0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
    0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D, 0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
    0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99, 0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
    0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E, 0x3C, 0x42, 0xB9, 0xA5, 0xB9, 0xA5, 0x42, 0x3C,
    0x21, 0x04, 0x01, 0x11, 0xA8, 0x00, 0x1A, 0x13, 0xBE, 0x00, 0x00, 0x23, 0x7D, 0xFE, 0x34, 0x20,
    0xF5, 0x06, 0x19, 0x78, 0x86, 0x23, 0x05, 0x20, 0xFB, 0x86, 0x00, 0x00, 0x3E, 0x01, 0xE0, 0x50
};

class Emulator {
  public:
    Emulator			( bool enableBootROM );
    ~Emulator			(void);

    bool				LoadRom				(const std::string& romName) ;
    void				SetRenderFunc       ( RenderFunc func ) ;
    void				Update				( ) ;
    void				StopGame			( ) ;
    std::string			GetCurrentOpcode	( ) const ;
    std::string			GetImmediateData1	( ) const ;
    std::string			GetImmediateData2	( ) const ;
    WORD				GetRegisterAF		( ) const {
        return m_RegisterAF.reg;
    }
    WORD				GetRegisterBC		( ) const {
        return m_RegisterBC.reg;
    }
    WORD				GetRegisterDE		( ) const {
        return m_RegisterDE.reg;
    }
    WORD				GetRegisterHL		( ) const {
        return m_RegisterHL.reg;
    }
    int					GetProgramCounter	( ) const {
        return m_ProgramCounter;
    }
    bool				IsGameLoaded		( ) const {
        return m_GameLoaded ;
    }
    BYTE				ExecuteNextOpcode	( ) ;
    int					GetZeroFlag			( )  const;
    int					GetSubtractFlag		( )  const ;
    int					GetCarryFlag		( )  const ;
    int					GetHalfCarryFlag	( )  const ;
    WORD				GetStackPointer		( ) const {
        return m_StackPointer.reg;
    }
    void				KeyPressed			( int key ) ;
    void				KeyReleased			( int key ) ;
    void				SetPauseFunction	( PauseFunc func ) {
        m_TimeToPause = func ;
    }
    unsigned long long	GetTotalOpcodes		( ) const {
        return m_TotalOpcodes;
    }
    void				SetPausePending		( bool pending ) {
        m_DebugPause = false ;
        m_DebugPausePending = pending ;
    }
    void				SetPause			( bool pause ) {
        m_DebugPause = pause;
    }


    std::vector<BYTE>   m_ScreenData;

  private:
    enum COLOUR {
        LIGHTEST_GREEN,
        LIGHT_GREEN,
        DARK_GREEN,
        DARKEST_GREEN
    };

    BYTE				GetLCDMode			( ) const ;
    void				SetLCDStatus		( ) ;
    BYTE				GetJoypadState		( ) const ;
    void				CreateRamBanks		( int numBanks ) ;

    BYTE				ReadMemory			( WORD memory ) const;
    bool				ResetCPU			( ) ;
    void				ResetScreen			( ) ;
    void				DoInterupts			( ) ;
    void				DoGraphics			( int cycles ) ;
    void				ServiceInterrupt	( int num) ;
    void				DrawScanLine		( ) ;
    COLOUR				GetColour			( BYTE colourNumber, WORD address ) const ;
    void				DoTimers			( int cycles ) ;

    void				RenderBackground	( BYTE lcdControl ) ;
    void				RenderSprites		( BYTE lcdControl ) ;

    void				ExecuteOpcode		( BYTE opcode ) ;
    void				ExecuteExtendedOpcode( ) ;

    void				CPU_8BIT_LOAD		( BYTE& reg ) ;
    void				CPU_16BIT_LOAD		( WORD& reg ) ;
    void				CPU_REG_LOAD		( BYTE& reg, BYTE load, int cycles) ;
    void				CPU_REG_LOAD_ROM	( BYTE& reg, WORD address ) ;
    void				CPU_8BIT_ADD		( BYTE& reg, BYTE toAdd, int cycles, bool useImmediate, bool addCarry ) ;
    void				CPU_8BIT_SUB		( BYTE& reg, BYTE toSubtract, int cycles, bool useImmediate, bool subCarry ) ;
    void				CPU_8BIT_AND		( BYTE& reg, BYTE toAnd, int cycles, bool useImmediate ) ;
    void				CPU_8BIT_OR			( BYTE& reg, BYTE toOr, int cycles, bool useImmediate ) ;
    void				CPU_8BIT_XOR		( BYTE& reg, BYTE toXOr, int cycles, bool useImmediate ) ;
    void				CPU_8BIT_COMPARE	( BYTE reg, BYTE toSubtract, int cycles, bool useImmediate) ; //dont pass a reference
    void				CPU_8BIT_INC		( BYTE& reg, int cycles ) ;
    void				CPU_8BIT_DEC		( BYTE& reg, int cycles ) ;
    void				CPU_8BIT_MEMORY_INC	( WORD address, int cycles ) ;
    void				CPU_8BIT_MEMORY_DEC	( WORD address, int cycles ) ;
    void				CPU_RESTARTS		( BYTE n ) ;

    void				CPU_16BIT_DEC		( WORD& word, int cycles ) ;
    void				CPU_16BIT_INC		( WORD& word, int cycles ) ;
    void				CPU_16BIT_ADD		( WORD& reg, WORD toAdd, int cycles ) ;

    void				CPU_JUMP			( bool useCondition, int flag, bool condition ) ;
    void				CPU_JUMP_IMMEDIATE	( bool useCondition, int flag, bool condition ) ;
    void				CPU_CALL			( bool useCondition, int flag, bool condition ) ;
    void				CPU_RETURN			( bool useCondition, int flag, bool condition ) ;

    void				CPU_SWAP_NIBBLES	( BYTE& reg ) ;
    void				CPU_SWAP_NIB_MEM	( WORD address ) ;
    void				CPU_SHIFT_LEFT_CARRY( BYTE& reg ) ;
    void				CPU_SHIFT_LEFT_CARRY_MEMORY( WORD address ) ;
    void				CPU_SHIFT_RIGHT_CARRY( BYTE& reg, bool resetMSB ) ;
    void				CPU_SHIFT_RIGHT_CARRY_MEMORY( WORD address, bool resetMSB  ) ;

    void				CPU_RESET_BIT		( BYTE& reg, int bit ) ;
    void				CPU_RESET_BIT_MEMORY( WORD address, int bit ) ;
    void				CPU_TEST_BIT		( BYTE reg, int bit, int cycles ) ;
    void				CPU_SET_BIT			( BYTE& reg, int bit ) ;
    void				CPU_SET_BIT_MEMORY	( WORD address, int bit ) ;

    void				CPU_DAA				( ) ;
    void                CPU_LOAD_SP_PLUS_SBYTE( WORD& reg ) ;

    void				CPU_RLC				( BYTE& reg ) ;
    void				CPU_RLC_MEMORY		( WORD address ) ;
    void				CPU_RRC				( BYTE& reg ) ;
    void				CPU_RRC_MEMORY		( WORD address ) ;
    void				CPU_RL				( BYTE& reg ) ;
    void				CPU_RL_MEMORY		( WORD address ) ;
    void				CPU_RR				( BYTE& reg ) ;
    void				CPU_RR_MEMORY		( WORD address ) ;

    void				CPU_SLA				( BYTE& reg );
    void				CPU_SLA_MEMORY		( WORD address ) ;
    void				CPU_SRA				( BYTE& reg );
    void				CPU_SRA_MEMORY		( WORD address ) ;
    void				CPU_SRL				( BYTE& reg );
    void				CPU_SRL_MEMORY		( WORD address ) ;

    PauseFunc			m_TimeToPause ;
    unsigned long long	m_TotalOpcodes ;

    bool				m_DoLogging ;
    bool                m_BootROMEnabled;
    bool                m_BootMode;

    RenderFunc			m_RenderFunc ;


    union Register {
        WORD reg ;
        struct {
            BYTE lo ;
            BYTE hi ;
        };
    };

    BYTE				m_JoypadState ;

    bool				m_GameLoaded ;
    BYTE				m_Rom[0x10000] ;
    BYTE				m_GameBank[0x200000] ;
    std::vector<BYTE*>	m_RamBank ;
    WORD				m_ProgramCounter ;
    bool				m_EnableRamBank ;

    Register			m_RegisterAF ;
    Register			m_RegisterBC ;
    Register			m_RegisterDE ;
    Register			m_RegisterHL ;
    int					m_CyclesThisUpdate ;

    Register			m_StackPointer ;
    int					m_CurrentRomBank ;
    bool				m_UsingMemoryModel16_8 ;
    bool				m_EnableInterupts ;
    bool				m_PendingInteruptDisabled ;
    bool				m_PendingInteruptEnabled ;
    int					m_CurrentRamBank ;
    int					m_RetraceLY ;
    bool				m_DebugPause ;
    bool				m_DebugPausePending ;

    void				RequestInterupt( int bit ) ;

    WORD				ReadWord			( ) const ;
    WORD				ReadLSWord			( ) const ;
    void				WriteByte			(WORD address, BYTE data) ;
    void				IssueVerticalBlank	( ) ;
    void				DrawCurrentLine		( ) ;
    void				PushWordOntoStack	( WORD word ) ;
    WORD				PopWordOffStack		( ) ;

    BYTE				m_DebugValue ;




    bool				m_UsingMBC1	 ;
    bool                m_UsingMBC2 ;
    bool				m_Halted ;
    int					m_TimerVariable ;
    int					m_DividerVariable ;
    int					m_CurrentClockSpeed ;


};

#endif
