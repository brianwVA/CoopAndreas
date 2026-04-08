#pragma once
#include "../CCustomCommand.h"

class CCommandSetPlayerPropertyState : public CCustomCommand
{
	void Process(CRunningScript* script) override;
};
