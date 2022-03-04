#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "SquidTasksLog.h"

DEFINE_LOG_CATEGORY(LogSquidTasks);

//////////////////////////////////////////////////////////////////////////
class FSquidTasksModule : public IModuleInterface
{
public:
	void StartupModule() override
	{
	}
	void ShutdownModule() override
	{
	}
};

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_MODULE(FSquidTasksModule, SquidTasks);
