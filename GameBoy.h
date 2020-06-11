#pragma once
#ifndef _GAMEBOY_H
#define _GAMEBOY_H

class Emulator ;

#include "Emulator.h"
#include <Windows.h>
#include <SDL2/SDL.h>
class GameBoy
{
public:
	static				GameBoy*				CreateInstance				( ) ;
	static				GameBoy*				GetSingleton				( ) ;

												~GameBoy					(void);

                        SDL_Renderer*           GetRenderer                 ();
                        SDL_Texture*            GetTexture                  ();
						void					RenderGame					(SDL_Renderer*, SDL_Texture*);
						void					Initialize					(char *);
						void					SetKeyPressed				( int key ) ;
						void					SetKeyReleased				( int key ) ;
						void					StartEmulation				( ) ;
						void					HandleInput					( SDL_Event& event ) ;
private:
												GameBoy						(void);

						bool					CreateSDLWindow				( ) ;


	static				GameBoy*				m_Instance ;


						Emulator*				m_Emulator ;
                        SDL_Window*             m_window;
                        SDL_Renderer*           m_renderer;
                        SDL_Texture*            m_texture;
                        HWND                    hWnd;
};

#endif
