// TortoiseGit - a Windows shell extension for easy version control

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
#include "RootFolderCache.h"
#include "IntegrityActions.h"
#include "ShellExt.h"

bool startsWith(std::wstring path, std::wstring potentialAncestor) 
{
	if (path.at(path.size() - 1) != '\\') {
		path += L"\\";
	}

	return path.length() >= potentialAncestor.length()
		&&
		path.substr(0, potentialAncestor.length()) == potentialAncestor;
}

bool RootFolderCache::isPathControlled(std::wstring path)
{
	std::transform(path.begin(), path.end(), path.begin(), ::tolower);

	// TODO binary search...?
	for (std::wstring rootPath : getRootFolders()) {
		if (startsWith(path, rootPath)) {
			return true;
		}
	}
	return false;
}

std::vector<std::wstring> RootFolderCache::fetchNewValue() 
{
	std::vector<std::wstring> rootFolders = IntegrityActions::getControlledPaths(integritySession);

	// to lower case everything
	for (std::wstring& rootPath : rootFolders) {
		if (rootPath.at(rootPath.size() - 1) != '\\') {
			rootPath += L"\\";
		}

		std::transform(rootPath.begin(), rootPath.end(), rootPath.begin(), ::tolower);
	}

	return rootFolders;
}

void RootFolderCache::cachedValueUpdated(const std::vector<std::wstring> oldValue, std::vector<std::wstring> newValue)
{
	std::vector<std::wstring> foldersAddedOrRemoved;

	std::set_symmetric_difference(oldValue.begin(), oldValue.end(),
		newValue.begin(), newValue.end(), std::back_inserter(foldersAddedOrRemoved));

	// update shell with root folders that were added or removed
	for (std::wstring rootFolder : foldersAddedOrRemoved) {
		EventLog::writeDebug(L"sending update notification for " + rootFolder);

		SHChangeNotify(SHCNE_ATTRIBUTES, SHCNF_PATH | SHCNF_FLUSH, (LPCVOID)rootFolder.c_str(), NULL);
	}
};


std::chrono::seconds RootFolderCache::getCacheExpiryDuration()
{
	return std::chrono::seconds(60); // TODO confirable via registry?
}