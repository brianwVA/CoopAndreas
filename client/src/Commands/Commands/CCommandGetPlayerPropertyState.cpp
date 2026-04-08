#include "stdafx.h"
#include "CCommandGetPlayerPropertyState.h"

void CCommandGetPlayerPropertyState::Process(CRunningScript* script)
{
	script->CollectParameters(1);

	const int propertyIndex = ScriptParams[0];
	ScriptParams[0] = CStatsSync::GetOwnedProperty(propertyIndex);

	script->StoreParameters(1);
}
