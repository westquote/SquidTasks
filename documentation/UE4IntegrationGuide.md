This guide will show you how to integrate Squid::Tasks into an Unreal Engine 4 game project.

Prerequisites
-------------

To use this guide, you must have:

 * An installation of Unreal Engine 4, built from source code. If you don't have this yet, [This Guide](https://docs.unrealengine.com/4.27/en-US/ProgrammingAndScripting/ProgrammingWithCPP/DownloadingSourceCode/) published by Epic will walk you through the process of downloading the source code, doing the initial setup, and compiling the source code.
 * An Unreal Engine game project, configured to use C++. If you don't have this yet, [This Guide](https://docs.unrealengine.com/4.27/en-US/Basics/Projects/Browser/) published by Epic will guide you though the process of creating a new project. Remember to configure your project to use C++, not blueprints!

Step 1: Enabling Compiler Support
---------------------------------
(__NOTE: If you're compiling with C++20, you should skip this step and go directly to Step 2__. If you're not sure which language standard you're compiling with, it's best to assume you're using an earlier standard than C++20.)

Before we can use C++ coroutines, we need to tell the compiler to enable support for coroutines. In Unreal Engine 5, this can be done easily through user configuration files, but in Unreal Engine 4, we will have to make a small edit to the engine source code.

Find `<Ue4 Source Root>/Engine/Source/Programs/UnrealBuildTool/Platform/Windows/VCToolChain.cs`, and look for the function `AppendCLArguments_Global`.

We're going to add code to the beginning of this function so that it looks like this:

```c#
protected virtual void AppendCLArguments_Global(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
{
	// Enable compilation of C++ coroutines. If we're using C++20, we don't need this
	if (CompileEnvironment.CppStandard <= CppStandardVersion.Cpp17)
	{
		if (Target.WindowsPlatform.Compiler == WindowsCompiler.Clang)
		{
			Arguments.Add("-fcoroutines-ts");
		}
		else
		{
			Arguments.Add("/await:strict");
		}
	}

	// Leave the rest of AppendCLArguments_Global unchanged
```

Note that each platform has its own `___ToolChain.cs` file. For example, there's an `AndroidToolChain.cs`, a `LinuxToolChain.cs`, and a `TVOSToolChain.cs`, so if you intend to ship on multiple platforms, you will have to make similar changes in each of the files corresponding to your target platforms.

One more note: The flag `/await:strict` is only fully supported in visual Studio 16.11 and newer. If you're using an older version of Visual Studio, use the `/await` flag instead.

Step 2: Adding the Squid::Tasks Plugin
--------------------------------------

The Squid::Tasks source code is maintained in two different formats: One for standalone projects, and one as an Unreal Engine plugin. Since we're integrating into Unreal Engine 4, we'll use the Unreal Engine plugin.

 1. Locate your game project's directory (It's the directory that contains the file `<project name>.uproject`).
 2. Within your project directory, find and open the directory called `Plugins`. If it doesn't exist yet, create it.
 3. In a separate window, open the SquidTasks source code and navigate into the `unreal/Plugins` directory.
 4. Copy the `SquidTasks` directory from the Squid::Tasks source into your project's `Plugins` directory.

Step 3: Add SquidTasks as a dependency of our game module
---------------------------------------------------------
Now that we've added our plugin, we need to tell our game's [module](https://docs.unrealengine.com/4.27/en-US/ProgrammingAndScripting/ProgrammingWithCPP/Modules/) to rely on it. 

The name of the file we need to edit is different from game to game. If our game project was called MyGame, then we'd need to edit a file called `MyGame.Build.cs`, and it will probably be located at `<project directory>/Source/MyGame/MyGame.Build.cs`.

The contents of this file should look something like this, although again it will differ from game to game, and from module to module:

```c#
using UnrealBuildTool;

public class MyGame : ModuleRules
{
	public MyGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });
	}
}
```

To add a dependency to SquidTasks, all we need to do is add an entry to `PublicDependencyModuleNames` so that it looks like this:

```c#
PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "SquidTasks" });
```

Step 4: Write a Task
--------------------
Squid::Tasks has now been integrated into our game module. To verify that it works, let's make a new actor and verify that tasks compile and run.

In the same directory as the `____.Build.cs` file we just edited, make a new file called `TaskHelloWorld.h`, and paste in the following code:

```c++
#pragma once
#include "SquidTasks/Task.h"
#include "TaskHelloWorld.generated.h"

UCLASS()
class ATaskHelloWorld : public AActor
{
	GENERATED_BODY()
public:
	ATaskHelloWorld() {
		PrimaryActorTick.bCanEverTick = true;
	}

	virtual void BeginPlay() override {
		Super::BeginPlay();
		MyTaskInstance = HelloWorldTask();
	}

	virtual void Tick(float DT) override {
		Super::Tick(DT);
		MyTaskInstance.Resume();
	}

private:
	Task<> MyTaskInstance;

	Task<> HelloWorldTask() {
		UE_LOG(LogTemp, Log, TEXT("Hello world at the beginning of the task!"));

		co_await Suspend();

		UE_LOG(LogTemp, Log, TEXT("Hello world on the next frame!"));

		while (true) {
			UE_LOG(LogTemp, Log, TEXT("Hello world inside the while loop!"));

			co_await Suspend();
		}
	}
};
```

In this example code, the `HelloWorldTask()` member function is a C++ coroutine written using the Squid::Tasks library.

In `BeginPlay()`, we call our `HelloWorldTask()` and store the task instance in the `MyTaskInstance` member variable. In `Tick()`, all we do is call `Resume()` on our stored task instance. 

We can now write stateful game code inside of `HelloWorldTask()`, and our actor will resume our task once per frame.

Step 5: Test our Task
---------------------

To test our task, start up the Unreal Editor, and load the game project that contains your `ATaskHelloWorld` actor.

First, open the "Place Actors" Window:

![Navigating the UE4 Menu to find the Place Actors Window](images/PlaceActors_Dropdown_UE4.png "Navigating the UE4 Menu to find the Place Actors Window")

Next, search for the "Task Hello World" actor, and drag one into the scene:

![Searching for the Task Hello World actor within the Place Actors Window](images/PlaceActors_Dropdown_UE4.png "Searching for the Task Hello World actor within the Place Actors Window")

In order to see the "hello" world text we're logging, we need to open the Output Log:

![Navigating the UE4 Menu to find the Output Log Window](images/OutputLog_Dropdown_UE4.png "Navigating the UE4 Menu to find the Output Log Window")

Finally, we're ready to hit Play to test our actor. We'll know it worked if we see this text in the Output Log:

```
LogTemp: Hello world at the beginning of the task!
LogTemp: Hello world on the next frame!
LogTemp: Hello world inside the while loop!
LogTemp: Hello world inside the while loop!
LogTemp: Hello world inside the while loop!
LogTemp: Hello world inside the while loop!
LogTemp: Hello world inside the while loop!
LogTemp: Hello world inside the while loop!
```

Squid::Tasks has now been successfully integrated into our game project!