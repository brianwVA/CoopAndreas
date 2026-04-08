#include "stdafx.h"
#include "CCommandGetPlayerFloatStat.h"

void CCommandGetPlayerFloatStat::Process(CRunningScript* script)
{
	script->CollectParameters(1);

	const int statIndex = ScriptParams[0];
	const float value = CStatsSync::GetFloatStat(statIndex);
	ScriptParams[0] = *reinterpret_cast<const int*>(&value);

	script->StoreParameters(1);
}
