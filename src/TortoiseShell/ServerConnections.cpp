// TortoiseSI - a Windows shell extension for easy version control

// Copyright (C) 2015 - TortoiseSI

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//

#include "stdafx.h"
#include "ServerConnections.h"
#include "IntegrityActions.h"
#include "IntegrityResponse.h"
#include "DebugEventLog.h"
#include "registry.h"

class MutexLocker {
public:
	MutexLocker(CHandle& handle) : handle(handle) {
		WaitForSingleObject((HANDLE)handle, INFINITE);
	}
	~MutexLocker() {
		ReleaseMutex((HANDLE)handle);
	}

private:
	CHandle& handle;
};


ServerConnections::ServerConnections(IntegritySession& integritySession)
	: AbstractCache(integritySession),
	connectionState(L"Software\\TortoiseSI\\__InternalConnectionState", (DWORD)ServerConnections::ConnectionState::Uninitialized, true)
{
	sharedInterProcessMutex.Attach(CreateMutex(NULL, TRUE, L"TortoiseSI Connection Mutex"));

	integritySession.getErrorHandlers().push_back(
		[&](mksAPIException exception) {  
			this->onConnectionError(exception);
		});
};

void ServerConnections::transitionToPromptingState()
{
	MutexLocker lock(sharedInterProcessMutex);

	connectionState = (DWORD)ConnectionState::Prompting;

	std::async(std::launch::async, [&]{ this->tryToConnect(); });
}

void ServerConnections::transitionToOnlineState()
{
	MutexLocker lock(sharedInterProcessMutex);

	connectionState = (DWORD)ConnectionState::Online;
}

void ServerConnections::transitionToOfflineState()
{
	MutexLocker lock(sharedInterProcessMutex);

	connectionState = (DWORD)ConnectionState::Offline;
}

void ServerConnections::onConnectionError(mksAPIException exception) 
{
	if (getId(exception) == L"ApplicationConnectionException") {
		MutexLocker lock(sharedInterProcessMutex);
		if (ConnectionState::Online == getState()) {
			transitionToPromptingState();
		}
	}
}

void ServerConnections::tryToConnect()
{
	EventLog::writeDebug(L"attempting to connect = ");
	bool success = IntegrityActions::connect(this->integritySession);
	EventLog::writeWarning(L"done trying to connect");

	if (success) {
		EventLog::writeWarning(L"done trying to connect-> forcing refresh");

		forceRefresh();
		transitionToOnlineState();
	} else {
		transitionToOfflineState();
	}
}

bool ServerConnections::isOnline()
{
	bool online;
	{
		MutexLocker lock(sharedInterProcessMutex);
		ConnectionState state = getState();

		if (state == ConnectionState::Uninitialized) {
			if (!getValue().empty()) {
				transitionToOnlineState();
				state = ConnectionState::Online;
			} else {
				transitionToPromptingState();
			}
		} else if (state == ConnectionState::Offline) {
			if (!getValue().empty()) {
				transitionToOnlineState();
				state = ConnectionState::Online;
			}
		}

		online = state == ConnectionState::Online;
	}	
	EventLog::writeDebug(std::wstring(L"isOnline = ") + (online ? L"true" : L"false"));

	return online;
}

ServerConnections::ConnectionState ServerConnections::getState()
{
	MutexLocker lock(sharedInterProcessMutex);

	DWORD regValue = (DWORD)connectionState;
	if (regValue < 0 || regValue >= ConnectionState::NumStates) {
		return ConnectionState::Uninitialized;
	} else {
		return (ConnectionState)(DWORD)connectionState;
	}
}

std::vector<std::wstring> ServerConnections::fetchNewValue()
{
	std::vector<std::wstring> result = IntegrityActions::servers(integritySession);

	if (EventLog::isDebugLoggingEnabled()) {
		std::wstring message;
		for (std::wstring connectionId : result) {
			message += L" \n" + connectionId;
		}
		EventLog::writeDebug(L"server connections: " + message);
	}
	return result;
}

std::chrono::seconds ServerConnections::getCacheExpiryDuration() 
{
	return std::chrono::seconds(30); // TODO configure from registry?
};

void ServerConnections::cachedValueUpdated(const std::vector<std::wstring>& /*oldValue*/, const std::vector<std::wstring>& newValue)
{
	if (!newValue.empty()) {
		MutexLocker lock(sharedInterProcessMutex);

		ConnectionState state = getState();
		if (ConnectionState::Offline == state || 
			ConnectionState::Uninitialized == state) {
			transitionToOnlineState();
		}
	}
}