#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PPPlaceholderBlock.generated.h"

class UStaticMeshComponent;

/**
 * Cosmetic + collision placeholder (a cube). Used to build a throwaway test hub at runtime
 * (floor, walls) until a real level exists. Server-spawned and replicated so clients see it and
 * the server can collide against it for authoritative movement. Scale is set via the spawn transform.
 */
UCLASS()
class PEACHPARTY_API APPPlaceholderBlock : public AActor
{
	GENERATED_BODY()

public:
	APPPlaceholderBlock();

protected:
	UPROPERTY(VisibleAnywhere, Category = "PeachParty|Placeholder")
	UStaticMeshComponent* Mesh;
};
