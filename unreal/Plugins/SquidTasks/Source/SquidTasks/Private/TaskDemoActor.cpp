#include "TaskDemoActor.h"

#include "SquidTasksLog.h"
#include "SquidTasks/FunctionGuard.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"

// Utility tasks (local to this compilation unit)
namespace
{
	Task<> CountSeconds(UBoxComponent* BoxComp, UTextRenderComponent* TextRenderComp)
	{
		// Restore text state when this coroutine collapses
		auto RestoreTextGuard = MakeFnGuard([TextRenderComp, Text = TextRenderComp->Text, Color = TextRenderComp->TextRenderColor] {
			TextRenderComp->SetText(Text);
			TextRenderComp->SetTextRenderColor(Color);
		});

		// Count the seconds
		float SecondsElapsed = 0.0f;
		TextRenderComp->SetTextRenderColor(FColor::Green);
		while(true)
		{
			TextRenderComp->SetText(FText::FromString(FString::SanitizeFloat(SecondsElapsed)));
			co_await WaitSeconds(1.0f, GameTime(BoxComp));
			++SecondsElapsed;
		}
	}
	Task<> DebugDrawBoxComponent(UBoxComponent* BoxComp)
	{
		while(true)
		{
			// Calculate correct color (green when overlapping, otherwise red)
			bool bIsOverlapping = BoxComp->GetOverlapInfos().Num() > 0;
			FColor Color = bIsOverlapping ? FColor::Green : FColor::Red;

			// Draw the box
			DrawDebugBox(BoxComp->GetWorld(), BoxComp->GetCenterOfMass(), BoxComp->GetScaledBoxExtent(), Color);

			// Wait until next frame
			co_await Suspend();
		}
	}
}

//--- ATaskDemoActor ---//
ATaskDemoActor::ATaskDemoActor()
{
	// Create box component (as root)
	BoxComp = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComp"));
	BoxComp->SetBoxExtent(FVector{ 200.0f, 200.0f, 200.0f }, false);
	RootComponent = BoxComp;

	// Create text render component
	TextRenderComp = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TextRenderComp"));
	TextRenderComp->SetupAttachment(BoxComp);
	TextRenderComp->HorizontalAlignment = EHTA_Center;
	TextRenderComp->VerticalAlignment = EVRTA_TextCenter;
	TextRenderComp->Text = FText::FromString(TEXT("Enter the box"));
	TextRenderComp->SetWorldSize(52.0f);

	// Make this actor tick
	PrimaryActorTick.bCanEverTick = true;
}
void ATaskDemoActor::BeginPlay()
{
	Super::BeginPlay();
	TaskMgr.RunManaged(ManageActor());
	TaskMgr.RunManaged(DebugDrawBoxComponent(BoxComp));
}
void ATaskDemoActor::Tick(float DT)
{
	Super::Tick(DT);
	TaskMgr.Update();
}
void ATaskDemoActor::EndPlay(const EEndPlayReason::Type EPR)
{
	TaskMgr.KillAllTasks();
	Super::EndPlay(EPR);
}
Task<> ATaskDemoActor::ManageActor()
{
	TASK_NAME(__FUNCTION__);

	while(true)
	{
		// Wait until anything overlaps this volume
		co_await WaitUntil([BoxComp = BoxComp] {
			return BoxComp->GetOverlapInfos().Num() > 0;
		});

		// Run a sub-task that counts off seconds while something is overlapping the volume
		co_await CountSeconds(BoxComp, TextRenderComp).CancelIf([BoxComp = BoxComp] {
			return BoxComp->GetOverlapInfos().Num() == 0;
		});
	}
}