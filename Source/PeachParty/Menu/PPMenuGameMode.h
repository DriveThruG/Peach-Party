#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PPMenuGameMode.generated.h"

/**
 * Minimal GameMode for the MainMenu map: NO pawn, no placeholder hub, plain PlayerController. Keeps the
 * menu clean — the player has no character falling through the void and nothing fights the widget's
 * "UI Only + cursor" input mode. Set this as the MainMenu level's GameMode Override (World Settings).
 * Gameplay maps (PeachPartyHub etc.) keep APPGameMode.
 */
UCLASS()
class PEACHPARTY_API APPMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APPMenuGameMode();
};
