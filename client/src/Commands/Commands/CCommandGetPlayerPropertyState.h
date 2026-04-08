#pragma once
#include "../CCustomCommand.h"

class CCommandGetPlayerPropertyState : public CCustomCommand
{
	void Process(CRunningScript* script) override;
};
