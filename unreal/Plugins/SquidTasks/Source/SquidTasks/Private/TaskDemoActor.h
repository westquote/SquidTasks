#pragma once

#include "SquidTasks/Task.h"
#include "SquidTasks/TaskManager.h"

#include "TaskDemoActor.generated.h"

class UBoxComponent;
class UTextRenderComponent;

// Task Demo Actor
UCLASS()
class ATaskDemoActor : public AActor
{
	GENERATED_BODY()
public:
	ATaskDemoActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DT) override;
	virtual void EndPlay(const EEndPlayReason::Type EPR) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Task Demo") UBoxComponent* BoxComp = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Task Demo") UTextRenderComponent* TextRenderComp = nullptr;

private:
	TaskManager TaskMgr;
	Task<> ManageActor();
};
