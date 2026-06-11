#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "PPGameInstance.generated.h"

/** One row in the server browser (Blueprint-friendly). */
USTRUCT(BlueprintType)
struct FPPServerEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PeachParty|Session") int32 Index = 0;
	UPROPERTY(BlueprintReadOnly, Category = "PeachParty|Session") FString ServerName = TEXT("Peach Party Lobby");
	UPROPERTY(BlueprintReadOnly, Category = "PeachParty|Session") int32 CurrentPlayers = 0;
	UPROPERTY(BlueprintReadOnly, Category = "PeachParty|Session") int32 MaxPlayers = 0;
	UPROPERTY(BlueprintReadOnly, Category = "PeachParty|Session") int32 PingMs = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPPSessionEvent, bool, bSuccess);

/**
 * Session handling for the main menu / lobby flow. All the error-prone Online Subsystem work lives
 * here (C++ is much cleaner than the Blueprint session nodes); the UMG widgets just call HostGame /
 * FindGames / JoinGameByIndex and bind to the OnX events. Defaults to the NULL subsystem for LAN.
 *
 * Flow: Host -> CreateSession -> OnCreateComplete -> ServerTravel(Lobby?listen).
 *       Join -> FindGames -> OnFindComplete (fills ServerList) -> JoinGameByIndex -> OnJoinComplete
 *               -> ClientTravel(resolved connect string).
 */
UCLASS()
class PEACHPARTY_API UPPGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	/** SERVER name is just a label; MaxPlayers 2-8; bLAN true for local testing (NULL subsystem). */
	UFUNCTION(BlueprintCallable, Category = "PeachParty|Session")
	void HostGame(const FString& ServerName, int32 MaxPlayers, bool bLAN);

	UFUNCTION(BlueprintCallable, Category = "PeachParty|Session")
	void FindGames(bool bLAN);

	UFUNCTION(BlueprintCallable, Category = "PeachParty|Session")
	void JoinGameByIndex(int32 Index);

	/** Results of the last FindGames — bind a ListView/ForEach in WBP_ServerBrowser to this. */
	UPROPERTY(BlueprintReadOnly, Category = "PeachParty|Session")
	TArray<FPPServerEntry> ServerList;

	UPROPERTY(BlueprintAssignable, Category = "PeachParty|Session") FPPSessionEvent OnCreateComplete;
	UPROPERTY(BlueprintAssignable, Category = "PeachParty|Session") FPPSessionEvent OnFindComplete;
	UPROPERTY(BlueprintAssignable, Category = "PeachParty|Session") FPPSessionEvent OnJoinComplete;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PeachParty|Session")
	FString LobbyMapName = TEXT("PeachPartyHub");

protected:
	void HandleCreateComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindComplete(bool bWasSuccessful);
	void HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

	IOnlineSessionPtr GetSession() const;

	TSharedPtr<FOnlineSessionSearch> SearchSettings;
	FDelegateHandle CreateHandle;
	FDelegateHandle FindHandle;
	FDelegateHandle JoinHandle;
};
