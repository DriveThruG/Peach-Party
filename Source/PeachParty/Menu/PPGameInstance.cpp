#include "Menu/PPGameInstance.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

// Custom session setting key holding the human-readable server name.
static const FName SETTING_SERVERNAME(TEXT("PP_SERVERNAME"));

IOnlineSessionPtr UPPGameInstance::GetSession() const
{
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
	return OSS ? OSS->GetSessionInterface() : nullptr;
}

void UPPGameInstance::HostGame(const FString& ServerName, int32 MaxPlayers, bool bLAN)
{
	IOnlineSessionPtr Session = GetSession();
	if (!Session.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PeachParty] No session interface (Online Subsystem missing)."));
		OnCreateComplete.Broadcast(false);
		return;
	}

	// Remember the params; CreateSessionNow() uses them (possibly after a destroy completes).
	PendingServerName = ServerName;
	PendingMaxPlayers = MaxPlayers;
	bPendingLAN = bLAN;

	// If a stale session exists, destroy it and create ONLY when that finishes (NULL races otherwise).
	if (Session->GetNamedSession(NAME_GameSession))
	{
		UE_LOG(LogTemp, Log, TEXT("[PeachParty] Existing session found — destroying before re-host."));
		DestroyHandle = Session->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &UPPGameInstance::HandleDestroyForHost));
		Session->DestroySession(NAME_GameSession);
		return;
	}

	CreateSessionNow();
}

void UPPGameInstance::HandleDestroyForHost(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Session = GetSession())
	{
		Session->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	}
	CreateSessionNow(); // now the old session is gone — safe to create
}

void UPPGameInstance::CreateSessionNow()
{
	IOnlineSessionPtr Session = GetSession();
	if (!Session.IsValid())
	{
		OnCreateComplete.Broadcast(false);
		return;
	}

	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch = bPendingLAN;
	Settings.NumPublicConnections = FMath::Clamp(PendingMaxPlayers, 2, 8);
	Settings.bShouldAdvertise = true;          // show up in searches
	Settings.bAllowJoinInProgress = true;
	Settings.bUsesPresence = !bPendingLAN;     // presence is for online; LAN doesn't need it
	Settings.bAllowJoinViaPresence = !bPendingLAN;
	Settings.bUseLobbiesIfAvailable = !bPendingLAN;
	Settings.Set(SETTING_SERVERNAME, PendingServerName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	CreateHandle = Session->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UPPGameInstance::HandleCreateComplete));
	Session->CreateSession(0, NAME_GameSession, Settings);
}

void UPPGameInstance::HandleCreateComplete(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Session = GetSession())
	{
		Session->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
	}
	OnCreateComplete.Broadcast(bWasSuccessful);

	if (bWasSuccessful && GetWorld())
	{
		// Listen-server travel to the lobby (everyone who joins follows the host here).
		const FString URL = LobbyMapName + TEXT("?listen");
		UE_LOG(LogTemp, Log, TEXT("[PeachParty] Session created — ServerTravel to %s"), *URL);
		GetWorld()->ServerTravel(URL);
	}
	else if (!bWasSuccessful)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PeachParty] CreateSession failed."));
	}
}

void UPPGameInstance::FindGames(bool bLAN)
{
	IOnlineSessionPtr Session = GetSession();
	if (!Session.IsValid())
	{
		return;
	}

	SearchSettings = MakeShareable(new FOnlineSessionSearch());
	SearchSettings->bIsLanQuery = bLAN;
	SearchSettings->MaxSearchResults = 50;
	// NOTE: no SEARCH_PRESENCE query setting — this project uses OnlineSubsystem Null + LAN, where
	// presence is never used (bUsesPresence=false on host). The SEARCH_PRESENCE constant also moved
	// between headers across UE versions, so depending on it only adds a fragile build dependency.

	UE_LOG(LogTemp, Log, TEXT("[PeachParty] FindGames: searching (LAN=%d)…"), bLAN ? 1 : 0);
	FindHandle = Session->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UPPGameInstance::HandleFindComplete));
	Session->FindSessions(0, SearchSettings.ToSharedRef());
}

void UPPGameInstance::HandleFindComplete(bool bWasSuccessful)
{
	if (IOnlineSessionPtr Session = GetSession())
	{
		Session->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
	}

	ServerList.Reset();
	const int32 Found = (bWasSuccessful && SearchSettings.IsValid()) ? SearchSettings->SearchResults.Num() : 0;
	UE_LOG(LogTemp, Log, TEXT("[PeachParty] FindGames complete: success=%d, found=%d session(s)."),
		bWasSuccessful ? 1 : 0, Found);
	if (bWasSuccessful && SearchSettings.IsValid())
	{
		for (int32 i = 0; i < SearchSettings->SearchResults.Num(); ++i)
		{
			const FOnlineSessionSearchResult& R = SearchSettings->SearchResults[i];
			FPPServerEntry Entry;
			Entry.Index = i;
			FString Name;
			Entry.ServerName = R.Session.SessionSettings.Get(SETTING_SERVERNAME, Name) ? Name : TEXT("Peach Party Lobby");
			Entry.MaxPlayers = R.Session.SessionSettings.NumPublicConnections;
			Entry.CurrentPlayers = Entry.MaxPlayers - R.Session.NumOpenPublicConnections;
			Entry.PingMs = R.PingInMs;
			ServerList.Add(Entry);
		}
	}
	OnFindComplete.Broadcast(bWasSuccessful);
}

void UPPGameInstance::JoinGameByIndex(int32 Index)
{
	IOnlineSessionPtr Session = GetSession();
	if (!Session.IsValid() || !SearchSettings.IsValid() || !SearchSettings->SearchResults.IsValidIndex(Index))
	{
		return;
	}

	JoinHandle = Session->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UPPGameInstance::HandleJoinComplete));
	Session->JoinSession(0, NAME_GameSession, SearchSettings->SearchResults[Index]);
}

void UPPGameInstance::HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSessionPtr Session = GetSession();
	if (Session.IsValid())
	{
		Session->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	}

	const bool bSuccess = (Result == EOnJoinSessionCompleteResult::Success);
	OnJoinComplete.Broadcast(bSuccess);

	FString ConnectString;
	if (bSuccess && Session.IsValid() && Session->GetResolvedConnectString(NAME_GameSession, ConnectString))
	{
		if (APlayerController* PC = GetFirstLocalPlayerController())
		{
			PC->ClientTravel(ConnectString, TRAVEL_Absolute); // travel to the host's lobby
		}
	}
	else if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PeachParty] JoinSession failed (%d)."), (int32)Result);
	}
}
