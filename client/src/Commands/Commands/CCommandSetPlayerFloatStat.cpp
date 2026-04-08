#include "stdafx.h"
#include "CCommandSetPlayerFloatStat.h"

void CCommandSetPlayerFloatStat::Process(CRunningScript* script)
{
	script->CollectParameters(2);

	const int statIndex = ScriptParams[0];
	const float value = *reinterpret_cast<float*>(&ScriptParams[1]);

	CStatsSync::SetFloatStat(statIndex, value);
}
