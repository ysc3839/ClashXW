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

#define OBJPREFIX LR"(Local\)"
constexpr size_t guidSize = std::size(L"{00000000-0000-0000-0000-000000000000}"); // including the null terminator
constexpr size_t prefixSize = std::size(OBJPREFIX) - 1;
constexpr size_t objNameSize = prefixSize + guidSize;

struct ProcessInfo
{
	DWORD processId;
	DWORD threadId;
	HWND hWndConsole;
};

namespace ProcessManager
{
	enum struct State
	{
		Stop,
		Running,
		WaitStop
	};

	namespace // private
	{
		State _state = State::Stop;
		fs::path _exePath, _homeDir, _configFile, _uiDir;
		std::wstring _ctlAddr, _ctlSecret, _clashCmd;
		wil::unique_handle _hJob;
		wil::unique_process_information _subProcInfo, _clashProcInfo;
		HWND _hWndConsole = nullptr;
		wil::unique_handle _hEvent;

		void _UpdateClashCmd()
		{
			_clashCmd.assign(LR"(")");
			_clashCmd.append(_exePath.filename());
			_clashCmd.append(LR"(")");
			if (!_homeDir.empty())
			{
				_clashCmd.append(LR"( -d ")");
				_clashCmd.append(_homeDir);
				_clashCmd.append(LR"(")");
			}
			if (!_configFile.empty())
			{
				_clashCmd.append(LR"( -f ")");
				_clashCmd.append(_configFile);
				_clashCmd.append(LR"(")");
			}
			if (!_uiDir.empty())
			{
				_clashCmd.append(LR"( -ext-ui ")");
				_clashCmd.append(_uiDir);
				_clashCmd.append(LR"(")");
			}
			if (!_ctlAddr.empty())
			{
				_clashCmd.append(LR"( -ext-ctl ")");
				_clashCmd.append(_ctlAddr);
				_clashCmd.append(LR"(")");
			}

			// Override secret even if empty
			_clashCmd.append(LR"( -secret ")");
			_clashCmd.append(_ctlSecret);
			_clashCmd.append(LR"(")");
		}
	}

	void SetArgs(fs::path exePath, fs::path homeDir, fs::path configFile, fs::path uiDir, std::wstring ctlAddr, std::wstring ctlSecret)
	{
		_exePath = exePath;
		_homeDir = homeDir;
		_configFile = configFile;
		_uiDir = uiDir;
		_ctlAddr = ctlAddr;
		_ctlSecret = ctlSecret;

		_UpdateClashCmd();
	}

	void SetConfigFile(fs::path configFile)
	{
		_configFile = configFile;

		_UpdateClashCmd();
	}

	State IsRunning() { return _state; }
	const PROCESS_INFORMATION& GetSubProcessInfo() { return _subProcInfo; }
	const PROCESS_INFORMATION& GetClashProcessInfo() { return _clashProcInfo; }
	HWND GetConsoleWindow() { return _hWndConsole; }

	bool Start()
	{
		if (_state != State::Stop)
			return false;

		try
		{
			_hJob.reset(CreateJobObjectW(nullptr, nullptr));
			THROW_LAST_ERROR_IF_NULL(_hJob);

			JOBOBJECT_EXTENDED_LIMIT_INFORMATION eli = {
				.BasicLimitInformation = {
					.LimitFlags = JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION | JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
				}
			};
			THROW_IF_WIN32_BOOL_FALSE(SetInformationJobObject(_hJob.get(), JobObjectExtendedLimitInformation, &eli, sizeof(eli)));

			// Local\{00000000-0000-0000-0000-000000000000}F
			wchar_t objName[objNameSize + 1] = OBJPREFIX;

			GUID guid = {};
			THROW_IF_FAILED(CoCreateGuid(&guid));
			THROW_HR_IF(E_OUTOFMEMORY, StringFromGUID2(guid, objName + prefixSize, guidSize) != guidSize);

			size_t size = std::max((_exePath.native().size() + 1 + _clashCmd.size() + 1) * sizeof(wchar_t), sizeof(ProcessInfo));
			objName[objNameSize - 1] = L'F';
			wil::unique_handle hFileMapping(CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, static_cast<DWORD>(size), objName));
			THROW_LAST_ERROR_IF_NULL(hFileMapping);
			auto error = GetLastError();
			if (error == ERROR_ALREADY_EXISTS) THROW_WIN32(error);

			wil::unique_mapview_ptr<wchar_t> buffer(reinterpret_cast<wchar_t*>(MapViewOfFile(hFileMapping.get(), FILE_MAP_WRITE | FILE_MAP_READ, 0, 0, 0)));
			THROW_LAST_ERROR_IF_NULL(buffer);

			objName[objNameSize - 1] = L'E';
			wil::unique_handle hEvent(CreateEventW(nullptr, FALSE, FALSE, objName));
			THROW_LAST_ERROR_IF_NULL(hEvent);
			error = GetLastError();
			if (error == ERROR_ALREADY_EXISTS) THROW_WIN32(error);

			auto exePathPtr = buffer.get();
			size_t i = _exePath.native().copy(exePathPtr, fs::path::string_type::npos);
			exePathPtr[i] = 0;

			auto cmdPtr = buffer.get() + i + 1;
			i = _clashCmd.copy(cmdPtr, std::wstring::npos);
			cmdPtr[i] = 0;

			auto selfPath = GetModuleFsPath(g_hInst);
			auto guidStr = objName + prefixSize;
			std::wstring cmd(LR"(")");
			cmd.append(selfPath);
			cmd.append(LR"(" --pm=)");
			cmd.append(guidStr, guidSize - 1);

			{
				// Disable Windows Error Reporting for subprocess
				auto lastErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
				auto restoreErrorMode = wil::scope_exit([=]() { // Restore error mode after CreateProcessW
					SetErrorMode(lastErrorMode);
				});
				wil::unique_cotaskmem_string appId;
				THROW_IF_FAILED(GetCurrentProcessExplicitAppUserModelID(&appId));
				STARTUPINFOW si = {
					.cb = sizeof(si),
					.lpTitle = appId.get(),
					.dwFlags = STARTF_FORCEOFFFEEDBACK | STARTF_PREVENTPINNING | STARTF_TITLEISAPPID | STARTF_USESHOWWINDOW,
					.wShowWindow = SW_HIDE
				};
				THROW_IF_WIN32_BOOL_FALSE(CreateProcessW(selfPath.c_str(), cmd.data(), nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr, nullptr, &si, &_subProcInfo));
			}

			// Ensure process in job before start
			THROW_IF_WIN32_BOOL_FALSE(AssignProcessToJobObject(_hJob.get(), _subProcInfo.hProcess));
			THROW_LAST_ERROR_IF(ResumeThread(_subProcInfo.hThread) == static_cast<DWORD>(-1));

			HANDLE handles[] = { hEvent.get(), _subProcInfo.hProcess };
			auto ret = WaitForMultipleObjects(static_cast<DWORD>(std::size(handles)), handles, FALSE, INFINITE);
			error = ERROR_TIMEOUT;
			switch (ret)
			{
			case WAIT_OBJECT_0: // Event signaled, clash process started suspended
			{
				auto info = reinterpret_cast<ProcessInfo*>(buffer.get());
				_clashProcInfo.dwProcessId = info->processId;
				_clashProcInfo.hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, _clashProcInfo.dwProcessId);
				THROW_LAST_ERROR_IF_NULL(_clashProcInfo.hProcess);

				_clashProcInfo.dwThreadId = info->threadId;
				_clashProcInfo.hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, _clashProcInfo.dwThreadId);
				THROW_LAST_ERROR_IF_NULL(_clashProcInfo.hThread);

				_hWndConsole = info->hWndConsole;
			}
			break;
			case WAIT_OBJECT_0 + 1: // Sub process exited before event signaled
			{
				DWORD exitCode;
				THROW_IF_WIN32_BOOL_FALSE(GetExitCodeProcess(_subProcInfo.hProcess, &exitCode));
				HRESULT hr = static_cast<HRESULT>(exitCode); // Treat exit code as hresult
				if (SUCCEEDED(hr)) hr = E_UNEXPECTED;
				THROW_HR(hr);
			}
			return false;
			case WAIT_FAILED:
				error = GetLastError();
				[[fallthrough]];
			case WAIT_TIMEOUT:
				THROW_WIN32(error);
				return false;
			}

			_hEvent = std::move(hEvent);
			ResumeThread(_clashProcInfo.hThread);
		}
		catch (...)
		{
			_hJob.reset();
			LOG_CAUGHT_EXCEPTION();
			return false;
		}
		_state = State::Running;
		return true;
	}

	void Stop()
	{
		if (_state != State::Stop)
		{
			_state = State::Stop;
			_hJob.reset();
			_subProcInfo.reset();
			_clashProcInfo.reset();
			_hWndConsole = nullptr;
			_hEvent.reset();
		}
	}

	void SendStopSignal()
	{
		if (_state == State::Running)
		{
			_state = State::WaitStop;
			SetEvent(_hEvent.get());
		}
	}

	int SubProcess(std::wstring_view guid)
	{
		try
		{
			wchar_t objName[objNameSize + 1] = OBJPREFIX;
			size_t i = guid.copy(objName + prefixSize, guidSize - 1);
			objName[prefixSize + i] = L'F';

			wil::unique_handle hFileMapping(OpenFileMappingW(FILE_MAP_WRITE | FILE_MAP_READ, FALSE, objName));
			THROW_LAST_ERROR_IF_NULL(hFileMapping);

			wil::unique_mapview_ptr<wchar_t> buffer(reinterpret_cast<wchar_t*>(MapViewOfFile(hFileMapping.get(), FILE_MAP_WRITE | FILE_MAP_READ, 0, 0, 0)));
			THROW_LAST_ERROR_IF_NULL(buffer);

			objName[prefixSize + i] = L'E';
			wil::unique_handle hEvent(OpenEventW(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, objName));
			THROW_LAST_ERROR_IF_NULL(hEvent);

			THROW_IF_WIN32_BOOL_FALSE(AllocConsole());
			HWND hWndConsole = ::GetConsoleWindow();
			THROW_LAST_ERROR_IF_NULL(hWndConsole);
			ShowWindow(hWndConsole, SW_HIDE);

			auto exePath = buffer.get();
			auto clashCmd = buffer.get() + wcslen(exePath) + 1;

			STARTUPINFOW si = { .cb = sizeof(si) };
			wil::unique_process_information procInfo;
			THROW_IF_WIN32_BOOL_FALSE(CreateProcessW(exePath, clashCmd, nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr, nullptr, &si, &procInfo));

			auto info = reinterpret_cast<ProcessInfo*>(buffer.get());
			*info = {
				.processId = procInfo.dwProcessId,
				.threadId = procInfo.dwThreadId,
				.hWndConsole = hWndConsole
			};

			SetConsoleCtrlHandler(nullptr, TRUE); // Ignores Ctrl+C

			THROW_IF_WIN32_BOOL_FALSE(SetEvent(hEvent.get()));

			while (true)
			{
				HANDLE handles[] = { hEvent.get(), procInfo.hProcess };
				auto ret = WaitForMultipleObjects(static_cast<DWORD>(std::size(handles)), handles, FALSE, INFINITE);
				if (ret == WAIT_OBJECT_0) // Event signaled
				{
					GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
				}
				else if (ret == WAIT_OBJECT_0 + 1) // Clash process exited
				{
					break;
				}
				else if (ret == WAIT_FAILED)
				{
					THROW_LAST_ERROR();
				}
				else if (ret == WAIT_TIMEOUT)
				{
					THROW_WIN32(ERROR_TIMEOUT);
				}
			}

			const std::wstring_view msg = _(
				L"\n"
				L"[Process completed]\n");
			THROW_IF_WIN32_BOOL_FALSE(WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), msg.data(), static_cast<DWORD>(msg.size()), nullptr, nullptr));
			static_cast<void>(_getch());
		}
		CATCH_RETURN();
		return S_OK;
	}
}

#undef OBJPREFIX
