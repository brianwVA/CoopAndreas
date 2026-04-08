#include "stdafx.h"
#include "CCommandSetPlayerSchoolProgress.h"

void CCommandSetPlayerSchoolProgress::Process(CRunningScript* script)
{
	script->CollectParameters(2);

	const int schoolIndex = ScriptParams[0];
	const uint8_t value = static_cast<uint8_t>(std::clamp(ScriptParams[1], 0, 255));

	CStatsSync::SetSchoolProgress(schoolIndex, value);
}
