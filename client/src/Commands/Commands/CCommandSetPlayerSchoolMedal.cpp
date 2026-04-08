#include "stdafx.h"
#include "CCommandSetPlayerSchoolMedal.h"

void CCommandSetPlayerSchoolMedal::Process(CRunningScript* script)
{
	script->CollectParameters(2);

	const int medalIndex = ScriptParams[0];
	const uint8_t value = static_cast<uint8_t>(std::clamp(ScriptParams[1], 0, 255));

	CStatsSync::SetSchoolMedal(medalIndex, value);
}
