#include "stdafx.h"
#include "CCommandSetPlayerPropertyState.h"

void CCommandSetPlayerPropertyState::Process(CRunningScript* script)
{
	script->CollectParameters(2);

	const int propertyIndex = ScriptParams[0];
	const bool isOwned = ScriptParams[1] != 0;

	CStatsSync::SetOwnedProperty(propertyIndex, isOwned);
}
