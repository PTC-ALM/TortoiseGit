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

#pragma once

#include "AbstractCache.h"

class ServerConnections : protected AbstractCache<std::vector<std::wstring>>{
public:
	ServerConnections(IntegritySession& integritySession);

	bool isOnline();

protected:
	virtual std::vector<std::wstring> fetchNewValue();
	virtual std::chrono::seconds getCacheExpiryDuration();
	virtual void cachedValueUpdated(const std::vector<std::wstring>& oldValue, const std::vector<std::wstring>& newValue);

private:
	void tryToConnect();
	void onConnectionError(mksAPIException exception);
	void transitionToPromptingState();
	void transitionToOnlineState();
	void transitionToOfflineState();

	CHandle sharedInterProcessMutex;

	enum ConnectionState { Online, Offline, Prompting, Uninitialized, NumStates /* must be last*/ };

	CRegDWORD connectionState;

	ConnectionState getState();
};

