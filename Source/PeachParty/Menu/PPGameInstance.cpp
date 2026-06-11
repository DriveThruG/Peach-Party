#include "Menu/PPGameInstance.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"  // SEARCH_PRESENCE et al. (moved out of OnlineSessionSettings.h in UE5)
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
		return;
	}

	// If a stale session exists, destroy it first to avoid "already in session" errors.
	if (Session->GetNamedSession(NAME_GameSession))
	{
		Session->DestroySession(NAME_GameSession);
	}

	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch = bLAN;
	Settings.NumPublicConnections = FMath::Clamp(MaxPlayers, 2, 8);
	Settings.bShouldAdvertise = true;          // show up in searches
	Settings.bAllowJoinInProgress = true;
	Settings.bUsesPresence = !bLAN;            // presence is for online; LAN doesn't need it
	Settings.bAllowJoinViaPresence = !bLAN;
	Settings.bUseLobbiesIfAvailable = !bLAN;
	Settings.Set(SETTING_SERVERNAME, ServerName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

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
		GetWorld()->ServerTravel(LobbyMapName + TEXT("?listen"));
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
	if (!bLAN)
	{
		SearchSettings->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);
	}

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
