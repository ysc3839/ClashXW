/*
 * Copyright (C) 2020 Richard Yu <yurichard3839@gmail.com>
 *
 * This file is part of ClashXW.
 *
 * ClashXW is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ClashXW is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with ClashXW.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

class ProcessManager
{
public:
	ProcessManager(fs::path exePath, fs::path homeDir, fs::path configFile, fs::path uiDir, std::wstring ctlAddr, std::wstring ctlSecret) :
		m_running(false), m_exePath(exePath), m_homeDir(homeDir), m_configFile(configFile), m_uiDir(uiDir), m_ctlAddr(ctlAddr), m_ctlSecret(ctlSecret) {}

	~ProcessManager()
	{
		ForceStop();
	}

	bool Start()
	{
		if (m_running)
			return false;

		try
		{
			m_hJob.reset(CreateJobObjectW(nullptr, nullptr));
			THROW_LAST_ERROR_IF_NULL(m_hJob);

			JOBOBJECT_EXTENDED_LIMIT_INFORMATION eli = {
				.BasicLimitInformation = {
					.LimitFlags = JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION | JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
				}
			};
			THROW_IF_WIN32_BOOL_FALSE(SetInformationJobObject(m_hJob.get(), JobObjectExtendedLimitInformation, &eli, sizeof(eli)));

			std::wstring cmd(m_exePath.filename());
			if (!m_homeDir.empty())
			{
				cmd.append(LR"( -d ")");
				cmd.append(m_homeDir);
				cmd.append(LR"(")");
			}
			if (!m_configFile.empty())
			{
				cmd.append(LR"( -f ")");
				cmd.append(m_configFile);
				cmd.append(LR"(")");
			}
			if (!m_uiDir.empty())
			{
				cmd.append(LR"( -ext-ui ")");
				cmd.append(m_uiDir);
				cmd.append(LR"(")");
			}
			if (!m_ctlAddr.empty())
			{
				cmd.append(LR"( -ext-ctl ")");
				cmd.append(m_ctlAddr);
				cmd.append(LR"(")");
			}

			// Override secret even if empty
			cmd.append(LR"( -secret ")");
			cmd.append(m_ctlSecret);
			cmd.append(LR"(")");

			STARTUPINFO si = { .cb = sizeof(si) };
			THROW_IF_WIN32_BOOL_FALSE(CreateProcessW(m_exePath.c_str(), cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &m_procInfo));

			THROW_IF_WIN32_BOOL_FALSE(AssignProcessToJobObject(m_hJob.get(), m_procInfo.hProcess));

			m_monitorThread = std::thread(&ProcessManager::Monitor, this);
		}
		catch (...)
		{
			m_hJob.reset();
			LOG_CAUGHT_EXCEPTION();
			return false;
		}
		m_running = true;
		return true;
	}

	void ForceStop()
	{
		if (m_running)
		{
			m_running = false;
			m_hJob.reset();
			m_monitorThread.join();
		}
	}

	const PROCESS_INFORMATION& GetProcessInfo() const
	{
		return m_procInfo;
	}

	void SetConfigFile(fs::path configFile)
	{
		m_configFile = configFile;
	}

private:
	void Monitor()
	{
		while (m_running)
		{
			auto ret = WaitForSingleObject(m_procInfo.hProcess, INFINITE);
			switch (ret)
			{
			case WAIT_OBJECT_0:
				if (m_running)
				{
					m_running = false;
					m_monitorThread.detach();
					DWORD exitCode;
					LOG_IF_WIN32_BOOL_FALSE(GetExitCodeProcess(m_procInfo.hProcess, &exitCode));
					PostMessageW(g_hWnd, WM_PROCESSNOTIFY, exitCode, 0);
				}
				m_hJob.reset();
				m_procInfo.reset();
				break;
			case WAIT_FAILED:
				LOG_LAST_ERROR();
				[[fallthrough]];
			case WAIT_TIMEOUT:
				// continue waiting
				break;
			}
		}
	}

	bool m_running;
	fs::path m_exePath, m_homeDir, m_configFile, m_uiDir;
	std::wstring m_ctlAddr, m_ctlSecret;
	wil::unique_handle m_hJob;
	wil::unique_process_information m_procInfo;
	std::thread m_monitorThread;
};
