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

	/** RELIABLE fallback: connect straight to a host's IP (e.g. "192.168.1.42" or "127.0.0.1" for local
	 *  2-process testing). Bypasses LAN session discovery entirely — best for a demo when the beacon is
	 *  blocked by a firewall. The host must already be hosting (listen server). */
	UFUNCTION(BlueprintCallable, Category = "PeachParty|Session")
	void JoinByIP(const FString& IpAddress);

	/** Results of the last FindGames — bind a ListView/ForEach in WBP_ServerBrowser to this. */
	UPROPERTY(BlueprintReadOnly, Category = "PeachParty|Session")
	TArray<FPPServerEntry> ServerList;

	UPROPERTY(BlueprintAssignable, Category = "PeachParty|Session") FPPSessionEvent OnCreateComplete;
	UPROPERTY(BlueprintAssignable, Category = "PeachParty|Session") FPPSessionEvent OnFindComplete;
	UPROPERTY(BlueprintAssignable, Category = "PeachParty|Session") FPPSessionEvent OnJoinComplete;

	/** Full package path is safest for ServerTravel (a short name can be ambiguous / unresolved). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PeachParty|Session")
	FString LobbyMapName = TEXT("/Game/PeachParty/Maps/PeachPartyHub");

protected:
	void HandleCreateComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindComplete(bool bWasSuccessful);
	void HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

	/** Destroy-then-create: a stale session must finish destroying before CreateSession, or NULL races
	 *  ("already in session") and hosting a second time fails. We chain create onto destroy-complete. */
	void CreateSessionNow();
	void HandleDestroyForHost(FName SessionName, bool bWasSuccessful);

	IOnlineSessionPtr GetSession() const;

	TSharedPtr<FOnlineSessionSearch> SearchSettings;
	FDelegateHandle CreateHandle;
	FDelegateHandle FindHandle;
	FDelegateHandle JoinHandle;
	FDelegateHandle DestroyHandle;

	// Host params remembered while we wait for the old session to destroy.
	FString PendingServerName;
	int32   PendingMaxPlayers = 4;
	bool    bPendingLAN = true;
};
