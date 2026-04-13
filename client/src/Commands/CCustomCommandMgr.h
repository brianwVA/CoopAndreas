#pragma once

#include "CCustomCommand.h"

class CCustomCommandMgr
{
public:
	static constexpr uint16_t MIN_CUSTOM_COMMAND = 0x1D00;
	static constexpr uint16_t MAX_CUSTOM_COMMAND = 0x1DFF;
	static constexpr uint16_t MAX_CUSTOM_COMMAND_COUNT = MAX_CUSTOM_COMMAND - MIN_CUSTOM_COMMAND + 1;

	static inline CCustomCommand* m_commands[MAX_CUSTOM_COMMAND_COUNT];

	static void RegisterCommand(uint16_t opcode, CCustomCommand* command)
	{
		if (opcode < MIN_CUSTOM_COMMAND || opcode > MAX_CUSTOM_COMMAND || command == nullptr)
		{
			return;
		}

		size_t idx = opcode - MIN_CUSTOM_COMMAND;

		if (idx >= MAX_CUSTOM_COMMAND_COUNT)
		{
			return;
		}

		if (m_commands[idx] != nullptr)
		{
			return;
		}

		m_commands[idx] = command;
	}

	static void ProcessCommand(uint16_t opcode, CRunningScript* script)
	{
		bool inRange = opcode >= MIN_CUSTOM_COMMAND && opcode <= MAX_CUSTOM_COMMAND;

		if (!inRange)
		{
			char message[128];
			sprintf_s(message, sizeof message, "Invalid custom opcode [%X] script name '%s', base ip '%d', cur ip '%d'", opcode, script->m_szName, script->m_pBaseIP, script->m_pCurrentIP);

			MessageBoxA(0, message, "Invalid opcode processing", MB_ICONERROR);
			return;
		}

		size_t idx = opcode - MIN_CUSTOM_COMMAND;

		if (idx >= MAX_CUSTOM_COMMAND_COUNT)
		{
			return;
		}

		CCustomCommand* command = m_commands[idx];
		if (command == nullptr)
		{
			// Do not crash on unknown custom opcode in release builds.
			// We skip it and keep script execution alive.
			return;
		}

		// Guard against corrupted command pointers (e.g. 0x1) in release builds.
		if ((uintptr_t)command < 0x10000)
		{
			return;
		}

		command->Process(script);
	}
};
