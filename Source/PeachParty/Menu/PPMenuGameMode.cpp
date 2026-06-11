#include "Menu/PPMenuGameMode.h"
#include "GameFramework/PlayerController.h"

APPMenuGameMode::APPMenuGameMode()
{
	DefaultPawnClass = nullptr;                                // no character spawns in the menu
	PlayerControllerClass = APlayerController::StaticClass();  // plain PC — no gameplay/interaction logic
}
