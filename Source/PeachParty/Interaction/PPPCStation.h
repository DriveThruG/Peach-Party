#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PPPCStation.generated.h"

class UStaticMeshComponent;
class USceneComponent;

/**
 * Decorative PC station prop. Purely scenery now — no sitting / ready / camera / minigame.
 * Place these in the level to dress the room. Assign your own model to StationMesh (per instance)
 * and set bHidePlaceholderBlocks=true to hide the placeholder desk + screen cubes.
 */
UCLASS()
class PEACHPARTY_API APPPCStation : public AActor
{
	GENERATED_BODY()

public:
	APPPCStation();

	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override; // applies bHidePlaceholderBlocks live

protected:
	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Station")
	USceneComponent* SceneRoot;

	/** Optional custom model for the whole station (assign per placed instance in Details). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PeachParty|Station")
	UStaticMeshComponent* StationMesh;

	/** Hide the placeholder desk + screen cubes (set true once you assign your own StationMesh). */
	UPROPERTY(EditAnywhere, Category = "PeachParty|Station")
	bool bHidePlaceholderBlocks = false;

	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Station")
	UStaticMeshComponent* DeskMesh;

	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Station")
	UStaticMeshComponent* ScreenMesh;

	/** Hide/show the placeholder cubes based on bHidePlaceholderBlocks AND whether a model is present. */
	void ApplyPlaceholderVisibility();
};
