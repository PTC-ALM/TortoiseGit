
#include "stdafx.h"
#include "StatusCache.h"
#include "RootFolderCache.h"
#include "IntegritySession.h"
#include "IntegrityActions.h"
#include "ServerConnections.h"
#include "globals.h"
#include "EventLog.h"
#include "DebugEventLog.h"

#undef min
#undef max

class SmallFixedInProcessCache : public IStatusCache {
public:
	SmallFixedInProcessCache();

	virtual FileStatusFlags getFileStatus(std::wstring fileName);
	virtual void clear(std::wstring path);

	virtual RootFolderCache& getRootFolderCache() { return *rootFolderCache;  };
	virtual IntegritySession& getIntegritySession() { return *integritySession; };
	
private:
	std::unique_ptr<RootFolderCache> rootFolderCache;
	std::unique_ptr<ServerConnections> serverConnectionsCache;
	std::unique_ptr<IntegritySession> integritySession;

	struct CachedEntry {
		std::wstring path;
		FileStatusFlags fileStatus;
		std::chrono::time_point<std::chrono::system_clock> entryCreationTime;
	};

	struct CachedResult {
		FileStatusFlags fileStatus;
		bool valid;

		CachedResult(FileStatusFlags fileStatus) : fileStatus(fileStatus), valid(true) {};
		CachedResult() : valid(false) {};
	};

	static const int cacheSize = 10;
	static const std::chrono::seconds entryExpiryTime;
	CachedEntry cachedStatus[cacheSize];
	std::mutex cacheLockObject;

	CachedResult findCachedStatus(const std::wstring& path);
	void addCachedStatus(const std::wstring& path, FileStatusFlags fileStatus);
};

SmallFixedInProcessCache* cacheInstance = NULL;
const std::chrono::seconds SmallFixedInProcessCache::entryExpiryTime = std::chrono::seconds(30);

int getIntegrationPoint() 
{
	CRegStdDWORD integrationPointKey(L"Software\\TortoiseSI\\IntegrationPoint", -1);
	integrationPointKey.read();

	int port = (DWORD)integrationPointKey;
	return port;
}

SmallFixedInProcessCache::SmallFixedInProcessCache() 
{
	int port = getIntegrationPoint();

	if (port > 0 && port <= std::numeric_limits<unsigned short>::max()) {
		EventLog::writeInformation(L"connecting to Integrity client via localhost:"+std::to_wstring(port));

		integritySession = std::unique_ptr<IntegritySession>(new IntegritySession("localhost", port));
	} else {
		EventLog::writeInformation(L"connecting to Integrity client via localintegration point");

		integritySession = std::unique_ptr<IntegritySession>(new IntegritySession());
	}
	rootFolderCache = std::unique_ptr<RootFolderCache>(new RootFolderCache(*integritySession));
	serverConnectionsCache = std::unique_ptr<ServerConnections>(new ServerConnections(*integritySession));
}

SmallFixedInProcessCache::CachedResult SmallFixedInProcessCache::findCachedStatus(const std::wstring& path) {
	std::lock_guard<std::mutex> lock(cacheLockObject);

	for (CachedEntry& entry : cachedStatus) {
		if (entry.path == path && (std::chrono::system_clock::now() - entry.entryCreationTime) < entryExpiryTime) {
			entry.entryCreationTime = std::chrono::system_clock::now();
			return CachedResult(entry.fileStatus);
		}
	}
	return CachedResult();
}

void SmallFixedInProcessCache::addCachedStatus(const std::wstring& path, FileStatusFlags fileStatus) {
	std::lock_guard<std::mutex> lock(cacheLockObject);

	// find oldest
	int oldestEntryIndex = -1;
	for (int i = 0; i < cacheSize; i++) {
		// unused entry
		if (cachedStatus[i].path.size() == 0) {
			oldestEntryIndex = i;
			break;
		}

		if ((cachedStatus[i].entryCreationTime - std::chrono::system_clock::now()) < entryExpiryTime) {
			oldestEntryIndex = i;
		}
	}

	cachedStatus[oldestEntryIndex].path = path;
	cachedStatus[oldestEntryIndex].fileStatus = fileStatus;
	cachedStatus[oldestEntryIndex].entryCreationTime = std::chrono::system_clock::now();
}

FileStatusFlags SmallFixedInProcessCache::getFileStatus(std::wstring fileName)
{
	if (!serverConnectionsCache->isOnline()) {
		EventLog::writeDebug(L"status cache bailing out, not connected to server");
		return (FileStatusFlags)FileStatus::None;
	}

	CachedResult result = findCachedStatus(fileName);
	if (result.valid) {
		return result.fileStatus;
	}

	FileStatusFlags status = IntegrityActions::fileInfo(*integritySession, fileName);

	if ((status & FileStatus::ErrorMask) == FileStatus::None) {
		addCachedStatus(fileName, status);
	}
	return status;
}

void SmallFixedInProcessCache::clear(std::wstring path) {
	std::lock_guard<std::mutex> lock(cacheLockObject);

	for (CachedEntry& entry : cachedStatus) {
		if (entry.path == path) {
			entry.path = L"";
			return;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// ICache

IStatusCache& IStatusCache::getInstance() {
	AutoLocker lock(g_csGlobalCOMGuard);
	if(cacheInstance == NULL) {
		cacheInstance = new SmallFixedInProcessCache();
	}

	return *cacheInstance;
}