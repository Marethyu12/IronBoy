#include "Config.h"
#include "GameBoy.h"
#include "GameSettings.h"


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    
	LogMessage* log = LogMessage::CreateInstance() ;
	GameSettings* settings = GameSettings::CreateInstance();
	GameBoy* gb = GameBoy::CreateInstance( ) ;

	gb->StartEmulation( ) ;

	delete gb ;
	delete settings ;
	delete log ;

	return 0;
}
