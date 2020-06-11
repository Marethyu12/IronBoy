#include "Config.h"
#include "Emulator.h"
#include "GameBoy.h"

#include <cstdlib>
#include <SDL2/SDL_syswm.h>

#define ID_LOADROM 0
#define ID_EXIT 1
#define ID_ABOUT 2

static const int screenWidth = 160;
static const int screenHeight = 144;
static const int scale = 2;

///////////////////////////////////////////////////////////////////////////////////////

static int total = 0 ;
static int timer = 0 ;
static int current = 0 ;
static int counter = 0 ;
static bool first = true ;

///////////////////////////////////////////////////////////////////////////////////////

static void DoRender( )
{
	GameBoy* gb = GameBoy::GetSingleton() ;
	gb->RenderGame(gb->GetRenderer(), gb->GetTexture());
}

///////////////////////////////////////////////////////////////////////////////////////

GameBoy* GameBoy::m_Instance = 0 ;

GameBoy* GameBoy::CreateInstance( )
{
	if (0 == m_Instance)
	{
		m_Instance = new GameBoy();
	}
	return m_Instance ;
}

//////////////////////////////////////////////////////////////////////////////////////////

GameBoy* GameBoy::GetSingleton( )
{
	return m_Instance ;
}

//////////////////////////////////////////////////////////////////////////////////////////

GameBoy::GameBoy(void) :
	m_Emulator(NULL)
{
	m_Emulator = new Emulator();
    m_Emulator->SetRenderFunc(DoRender);
    
    if (!CreateSDLWindow())
    {
        MessageBox(NULL, TEXT("SDL window cannot be created."), TEXT("Error"), MB_OK | MB_ICONERROR);
        std::exit(1);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////

SDL_Renderer *GameBoy::GetRenderer()
{
    return m_renderer;
}

//////////////////////////////////////////////////////////////////////////////////////////

SDL_Texture *GameBoy::GetTexture()
{
    return m_texture;
}

//////////////////////////////////////////////////////////////////////////////////////////

void GameBoy::Initialize(char *romFile)
{
    m_Emulator->LoadRom(romFile);
}

//////////////////////////////////////////////////////////////////////////////////////////
void GameBoy::StartEmulation( )
{
    bool bFirstTime = true;
    bool bROMLoaded = false;
    bool quit = false;
	SDL_Event evt;

	float fps = 59.73 ;
	float interval = 1000 ;
	interval /= fps ;

	unsigned int time2;

	while (!quit)
	{
        while (SDL_PollEvent(&evt) != 0)
        {
            switch (evt.type)
            {
            case SDL_SYSWMEVENT:
                switch (evt.syswm.msg->msg.win.msg)
                {
                case WM_COMMAND:
                    switch (LOWORD(evt.syswm.msg->msg.win.wParam))
                    {
                    case ID_LOADROM:
                    {
                        OPENFILENAME ofn;
                        char szFileName[MAX_PATH] = "";
                        ZeroMemory(&ofn, sizeof(ofn));
                        
                        ofn.lStructSize = sizeof(ofn);
                        ofn.hwndOwner = hWnd;
                        ofn.lpstrFilter = "ROM files (*.gb)\0*.gb\0";
                        ofn.lpstrFile = szFileName;
                        ofn.nMaxFile = MAX_PATH;
                        ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
                        ofn.lpstrDefExt = "gb";
                        
                        if(GetOpenFileName(&ofn))
                        {
                            Initialize(szFileName);
                            bROMLoaded = true;
                            bFirstTime = true;
                        }
                        break;
                    }
                    case ID_EXIT:
                        quit = true;
                        break;
                    case ID_ABOUT:
                        MessageBox(hWnd, TEXT("um..."), TEXT("About IronBoy"), MB_ICONINFORMATION | MB_OK);
                        break;
                    }
                    break;
                }
                break;
                break;
            case SDL_WINDOWEVENT_CLOSE:
                evt.type = SDL_QUIT;
                SDL_PushEvent(&evt);
                break;
            case SDL_QUIT:
                quit = true;
                break;
            default:
                HandleInput(evt);
                break;
            }
        }
        
        if (bROMLoaded)
        {
            if (bFirstTime)
            {
                time2 = SDL_GetTicks();
                bFirstTime = false;
            }
            
            unsigned int current = SDL_GetTicks( ) ;
            
            if( (time2 + interval) < current )
            {
                m_Emulator->Update();
                time2 = current ;
            }
        }
        else
        {
            DoRender();
        }
	}
}

//////////////////////////////////////////////////////////////////////////////////////////

GameBoy::~GameBoy(void)
{
	delete m_Emulator ;
    
    SDL_DestroyTexture(m_texture);
    m_texture = NULL;
    
    SDL_DestroyRenderer(m_renderer);
    m_renderer = NULL;
    
    SDL_DestroyWindow(m_window);
    m_window = NULL;
    
    SDL_Quit();
}

//////////////////////////////////////////////////////////////////////////////////////////

void GameBoy::RenderGame(SDL_Renderer *renderer, SDL_Texture *texture)
{
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_UpdateTexture(texture, NULL, &m_Emulator->m_ScreenData[0], screenWidth * 4);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

//////////////////////////////////////////////////////////////////////////////////////////

void GameBoy::SetKeyPressed(int key)
{
	m_Emulator->KeyPressed(key) ;
}

//////////////////////////////////////////////////////////////////////////////////////////

void GameBoy::SetKeyReleased(int key)
{
	m_Emulator->KeyReleased(key) ;
}

//////////////////////////////////////////////////////////////////////////////////////////

bool GameBoy::CreateSDLWindow( )
{
	if( SDL_Init( SDL_INIT_EVERYTHING ) < 0 )
	{
		return false ;
	}
	
    m_window = SDL_CreateWindow("IronBoy",
                                SDL_WINDOWPOS_UNDEFINED,
                                SDL_WINDOWPOS_UNDEFINED,
                                screenWidth * scale,
                                screenHeight * scale,
                                SDL_WINDOW_SHOWN);
    
    if (m_window == NULL)
    {
        return false;
    }
    
    m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
    
    if (m_renderer == NULL)
    {
        return false;
    }
    
    m_texture = SDL_CreateTexture(m_renderer,
                                  SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_STREAMING,
                                  screenWidth,
                                  screenHeight);
    
    if (m_texture == NULL)
    {
        return false;
    }
    
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    
    if (!SDL_GetWindowWMInfo(m_window, &info))
    {
        return false;
    }
    
    hWnd = info.info.win.window;
    
    HMENU hMenuBar = CreateMenu();
    HMENU hFile = CreatePopupMenu();
    HMENU hHelp = CreatePopupMenu();
    
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR) hFile, "File");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR) hHelp, "Help");
    
    AppendMenu(hFile, MF_STRING, ID_LOADROM, "Load ROM");
    AppendMenu(hFile, MF_STRING, ID_EXIT, "Exit");
    
    AppendMenu(hHelp, MF_STRING, ID_ABOUT, "About");
    
    SetMenu(hWnd, hMenuBar);
    
    SDL_SetWindowSize(m_window, screenWidth * scale, screenHeight * scale); // resize because we just added the menubar
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
    
	return true ;
}

//////////////////////////////////////////////////////////////////////////////////////////

void GameBoy::HandleInput(SDL_Event& event)
{
	if( event.type == SDL_KEYDOWN )
	{
		int key = -1 ;
		switch( event.key.keysym.sym )
		{
			case SDLK_a : key = 4 ; break ;
			case SDLK_s : key = 5 ; break ;
			case SDLK_RETURN : key = 7 ; break ;
			case SDLK_SPACE : key = 6; break ;
			case SDLK_RIGHT : key = 0 ; break ;
			case SDLK_LEFT : key = 1 ; break ;
			case SDLK_UP : key = 2 ; break ;
			case SDLK_DOWN : key = 3 ; break ;
		}
		if (key != -1)
		{
			SetKeyPressed(key) ;
		}
	}
	//If a key was released
	else if( event.type == SDL_KEYUP )
	{
		int key = -1 ;
		switch( event.key.keysym.sym )
		{
			case SDLK_a : key = 4 ; break ;
			case SDLK_s : key = 5 ; break ;
			case SDLK_RETURN : key = 7 ; break ;
			case SDLK_SPACE : key = 6; break ;
			case SDLK_RIGHT : key = 0 ; break ;
			case SDLK_LEFT : key = 1 ; break ;
			case SDLK_UP : key = 2 ; break ;
			case SDLK_DOWN : key = 3 ; break ;
		}
		if (key != -1)
		{
			SetKeyReleased(key) ;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
