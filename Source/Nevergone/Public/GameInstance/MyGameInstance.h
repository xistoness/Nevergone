// Copyright Xyzto Works

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Data/PartyData.h"
#include "Data/ProgressionData.h"
#include "Data/LevelTransitionContext.h"
#include "MyGameInstance.generated.h"

struct FActorSaveData;
class UMySaveGame;

UCLASS()
class NEVERGONE_API UMyGameInstance : public UGameInstance
{
	GENERATED_BODY()
public:
	
	virtual void Init() override;
	virtual void Shutdown() override;
	
	UFUNCTION(BlueprintCallable)
	void RequestSaveGame();
	UFUNCTION(BlueprintCallable)
	void RequestLoadGame();
	
	
	// TODO
	// Party
	const FPartyData& GetPartyData() const;
	void SetPartyData(const FPartyData& NewData);
	
	// Progression
	const FProgressionData& GetProgressionData() const;
	void SetProgressionData(const FProgressionData& NewData);
		
	// Global Flags
	bool GetGlobalFlag(FName Flag) const;
	void SetGlobalFlag(FName Flag, bool bValue);
	
	// Saved Actors
	const TArray<FActorSaveData>& GetSavedActors() const;
	void SetSavedActors(const TArray<FActorSaveData>& NewActors) const;
	void AddOrUpdateSavedActor(const FActorSaveData& ActorData) const;
	
	// Level Transition
	void RequestLevelChange(
		FName TargetLevel,
		const FLevelTransitionContext& Context
	);
	
	// Callbacks
	void OnPreLevelUnload() const;
	void OnPostLevelLoad() const;
	
	// Called after the level has successfully loaded
	void ApplyPersistentStateToWorld(UWorld* World) const;
	
	// Event Broadcasting
	//OnRestoreParty.Broadcast(PartyData);
	//OnRestoreProgress.Broadcast(ProgressionData);
	
	DECLARE_MULTICAST_DELEGATE(FOnSaveRequested);
	DECLARE_MULTICAST_DELEGATE(FOnSaveLoaded);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPartyChanged, const FPartyData&);

	FOnSaveLoaded OnSaveLoaded;
	FOnPartyChanged OnPartyChanged;
	FOnSaveRequested OnSaveRequested;
	
	
	// Debugging
	void Debug_PrintGlobalState() const;
	void Debug_ResetProgression();

protected:
	
	void SetActiveSave(UMySaveGame* Save);
	UMySaveGame* GetActiveSave() const;
	
	bool LoadSaveSlot(const FString& SlotName);
	void ApplySaveToWorld(UWorld* World) const;
	bool CreateNewSave(const FString& SlotName);
	void ClearActiveSave();
	
	void CommitSave() const;
	
private:
	UPROPERTY()
	UMySaveGame* ActiveSave;

	UPROPERTY()
	FPartyData PartyData;

	UPROPERTY()
	FProgressionData ProgressionData;
	
	UPROPERTY()
	TMap<FName, bool> GlobalFlags;

	UPROPERTY()
	FLevelTransitionContext PendingLevelTransition;	
	
};
