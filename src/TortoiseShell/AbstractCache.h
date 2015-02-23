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

#include "IntegritySession.h"

template<class CacheValueType> class AbstractCache {
public:
	AbstractCache(const IntegritySession& integritySession) 
		: refreshInProgress(false),
		integritySession(integritySession) {

		forceRefresh();
	}

	void forceRefresh();
	CacheValueType getValue();

protected:
	virtual std::chrono::seconds getCacheExpiryDuration() = 0;
	virtual CacheValueType fetchNewValue() = 0;
	virtual void cachedValueUpdated(const CacheValueType& /*oldValue*/, const CacheValueType& /*newValue*/){ };

	const IntegritySession& integritySession;

private:
	bool refreshInProgress;

	typedef std::recursive_mutex MutexType;
	typedef std::lock_guard<MutexType> LockGuard;

	MutexType lockObject;
	std::chrono::time_point<std::chrono::system_clock> lastRefresh;
	CacheValueType cachedValue;

	bool refreshIfStale();
	void updateCacheValue();
};

template<class CacheValueType>
CacheValueType AbstractCache<CacheValueType>::getValue()
{
	LockGuard lock(lockObject);

	refreshIfStale();

	return cachedValue;
};

template<class CacheValueType>
bool AbstractCache<CacheValueType>::refreshIfStale()
{
	LockGuard lock(lockObject);

	auto now = std::chrono::system_clock::now();

	if (now - lastRefresh > std::chrono::seconds(getCacheExpiryDuration()) && !refreshInProgress) {
		refreshInProgress = true;
		std::async(std::launch::async, [&]{ this->updateCacheValue(); });
		return true;
	}
	else {
		return false;
	}
}

template<class CacheValueType>
void AbstractCache<CacheValueType>::forceRefresh()
{
	{
		LockGuard lock(lockObject);

		if (refreshInProgress) {
			return;
		}
		refreshInProgress = true;
	}

	std::async(std::launch::async, [&]{ this->updateCacheValue(); });
}

template<class CacheValueType>
void AbstractCache<CacheValueType>::updateCacheValue()
{
	CacheValueType newValue = fetchNewValue();

	// lock cach and copy back
	std::vector<std::wstring> oldValue;

	{
		LockGuard lock(lockObject);

		oldValue = this->cachedValue;
		this->cachedValue = newValue;
		this->lastRefresh = std::chrono::system_clock::now();
		this->refreshInProgress = false;
	}

	cachedValueUpdated(oldValue, newValue);
}