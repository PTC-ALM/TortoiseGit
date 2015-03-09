// TortoiseGit - a Windows shell extension for easy version control

// Copyright (C) 2013-2014 - TortoiseGit

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
#include "UpdateDownloader.h"
#include "..\version.h"
#include "SysInfo.h"

CUpdateDownloader::CUpdateDownloader(HWND hwnd, bool force, UINT msg, CEvent *eventStop)
: m_hWnd(hwnd)
, m_bForce(force)
, m_uiMsg(msg)
, m_eventStop(eventStop)
{
	OSVERSIONINFOEX inf = {0};
	BruteforceGetWindowsVersionNumber(inf);

	m_sWindowsPlatform = (inf.dwPlatformId == VER_PLATFORM_WIN32_NT) ? _T("NT") : _T("");
	m_sWindowsVersion.Format(L"%ld.%ld", inf.dwMajorVersion, inf.dwMinorVersion);
	if (inf.wServicePackMajor)
		m_sWindowsServicePack.Format(L"SP%ld", inf.wServicePackMajor);

	CString userAgent;
	userAgent.Format(L"TortoiseGit %s; %s; Windows%s%s %s%s%s", _T(STRFILEVER), _T(TGIT_PLATFORM), m_sWindowsPlatform.IsEmpty() ? _T("") : _T(" "), m_sWindowsPlatform, m_sWindowsVersion, m_sWindowsServicePack.IsEmpty() ? _T("") : _T(" "), m_sWindowsServicePack);
	hOpenHandle = InternetOpen(userAgent, INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
}

CUpdateDownloader::~CUpdateDownloader(void)
{
	if (hOpenHandle)
		InternetCloseHandle(hOpenHandle);
}

void CUpdateDownloader::BruteforceGetWindowsVersionNumber(OSVERSIONINFOEX& osVersionInfo)
{
	osVersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	GetVersionEx((OSVERSIONINFO *)&osVersionInfo);

	ULONGLONG maskConditioMajor = ::VerSetConditionMask(0, VER_MAJORVERSION, VER_LESS);
	ULONGLONG maskConditioMinor = ::VerSetConditionMask(0, VER_MINORVERSION, VER_LESS);
	ULONGLONG maskConditioSPMajor = ::VerSetConditionMask(0, VER_SERVICEPACKMAJOR, VER_LESS);
	while (!::VerifyVersionInfo(&osVersionInfo, VER_MAJORVERSION, maskConditioMajor))
	{
		++osVersionInfo.dwMajorVersion;
		osVersionInfo.dwMinorVersion = 0;
		osVersionInfo.wServicePackMajor = 0;
		osVersionInfo.wServicePackMinor = 0;
	}
	while (!::VerifyVersionInfo(&osVersionInfo, VER_MINORVERSION, maskConditioMinor))
	{
		++osVersionInfo.dwMinorVersion;
		osVersionInfo.wServicePackMajor = 0;
		osVersionInfo.wServicePackMinor = 0;
	}
	while (!::VerifyVersionInfo(&osVersionInfo, VER_SERVICEPACKMAJOR, maskConditioSPMajor))
		++osVersionInfo.wServicePackMajor;
	// detection of VER_SERVICEPACKMINOR doesn't work reliably
}

DWORD CUpdateDownloader::DownloadFile(const CString& url, const CString& dest, bool showProgress) const
{
	CString hostname;
	CString urlpath;
	URL_COMPONENTS urlComponents = {0};
	urlComponents.dwStructSize = sizeof(urlComponents);
	urlComponents.lpszHostName = hostname.GetBufferSetLength(INTERNET_MAX_HOST_NAME_LENGTH);
	urlComponents.dwHostNameLength = INTERNET_MAX_HOST_NAME_LENGTH;
	urlComponents.lpszUrlPath = urlpath.GetBufferSetLength(INTERNET_MAX_PATH_LENGTH);
	urlComponents.dwUrlPathLength = INTERNET_MAX_PATH_LENGTH;
	if (!InternetCrackUrl(url, url.GetLength(), 0, &urlComponents))
		return GetLastError();
	hostname.ReleaseBuffer();
	urlpath.ReleaseBuffer();

	if (m_bForce)
		DeleteUrlCacheEntry(url);

	BOOL bTrue = TRUE;
	BOOL enableDecoding = InternetSetOption(hOpenHandle, INTERNET_OPTION_HTTP_DECODING, &bTrue, sizeof(bTrue));

	bool isHttps = urlComponents.nScheme == INTERNET_SCHEME_HTTPS;
	HINTERNET hConnectHandle = InternetConnect(hOpenHandle, hostname, urlComponents.nPort, nullptr, nullptr, isHttps ? INTERNET_SCHEME_HTTP : urlComponents.nScheme, 0, 0);
	if (!hConnectHandle)
	{
		DWORD err = GetLastError();
		CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": Download of %s failed on InternetConnect: %d\n"), url, err);
		return err;
	}
	HINTERNET hResourceHandle = HttpOpenRequest(hConnectHandle, nullptr, urlpath, nullptr, nullptr, nullptr, INTERNET_FLAG_KEEP_CONNECTION | (isHttps ? INTERNET_FLAG_SECURE : 0), 0);
	if (!hResourceHandle)
	{
		DWORD err = GetLastError();
		CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": Download of %s failed on HttpOpenRequest: %d\n"), url, err);
		InternetCloseHandle(hConnectHandle);
		return err;
	}

	if (enableDecoding && SysInfo::Instance().IsVistaOrLater())
		HttpAddRequestHeaders(hResourceHandle, L"Accept-Encoding: gzip, deflate\r\n", (DWORD)-1, HTTP_ADDREQ_FLAG_ADD);

	{
resend:
		BOOL httpsendrequest = HttpSendRequest(hResourceHandle, nullptr, 0, nullptr, 0);

		DWORD dwError = InternetErrorDlg(m_hWnd, hResourceHandle, ERROR_SUCCESS, FLAGS_ERROR_UI_FILTER_FOR_ERRORS | FLAGS_ERROR_UI_FLAGS_CHANGE_OPTIONS | FLAGS_ERROR_UI_FLAGS_GENERATE_DATA, nullptr);

		if (dwError == ERROR_INTERNET_FORCE_RETRY)
			goto resend;
		else if (!httpsendrequest)
		{
			DWORD err = GetLastError();
			CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": Download of %s failed: %d, %d\n"), url, httpsendrequest, err);
			InternetCloseHandle(hResourceHandle);
			InternetCloseHandle(hConnectHandle);
			return err;
		}
	}

	DWORD contentLength = 0;
	{
		DWORD length = sizeof(contentLength);
		HttpQueryInfo(hResourceHandle, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, (LPVOID)&contentLength, &length, NULL);
	}
	{
		DWORD statusCode = 0;
		DWORD length = sizeof(statusCode);
		if (!HttpQueryInfo(hResourceHandle, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, (LPVOID)&statusCode, &length, NULL) || statusCode != 200)
		{
			CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": Download of %s returned %d\n"), url, statusCode);
			InternetCloseHandle(hResourceHandle);
			InternetCloseHandle(hConnectHandle);
			if (statusCode == 404)
				return ERROR_FILE_NOT_FOUND;
			else if (statusCode == 403)
				return ERROR_ACCESS_DENIED;
			return (DWORD)INET_E_DOWNLOAD_FAILURE;
		}
	}

	CFile destinationFile;
	if (!destinationFile.Open(dest, CFile::modeCreate | CFile::modeWrite))
	{
		InternetCloseHandle(hResourceHandle);
		InternetCloseHandle(hConnectHandle);
		return ERROR_ACCESS_DENIED;
	}
	DWORD downloadedSum = 0; // sum of bytes downloaded so far
	do
	{
		DWORD size; // size of the data available
		if (!InternetQueryDataAvailable(hResourceHandle, &size, 0, 0))
		{
			DWORD err = GetLastError();
			CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": Download of %s failed on InternetQueryDataAvailable: %d\n"), url, err);
			InternetCloseHandle(hResourceHandle);
			InternetCloseHandle(hConnectHandle);
			return err;
		}

		DWORD downloaded; // size of the downloaded data
		LPTSTR lpszData = new TCHAR[size + 1];
		if (!InternetReadFile(hResourceHandle, (LPVOID)lpszData, size, &downloaded))
		{
			delete[] lpszData;
			DWORD err = GetLastError();
			CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": Download of %s failed on InternetReadFile: %d\n"), url, err);
			InternetCloseHandle(hResourceHandle);
			InternetCloseHandle(hConnectHandle);
			return err;
		}

		if (downloaded == 0)
		{
			delete[] lpszData;
			break;
		}

		lpszData[downloaded] = '\0';
		destinationFile.Write(lpszData, downloaded);
		delete[] lpszData;

		downloadedSum += downloaded;

		if (!showProgress)
			continue;

		ASSERT(m_uiMsg && m_eventStop);

		if (contentLength == 0) // got no content-length from webserver
		{
			DOWNLOADSTATUS downloadStatus = { 0, 1 + 1 }; // + 1 for download of signature file
			::SendMessage(m_hWnd, m_uiMsg, 0, reinterpret_cast<LPARAM>(&downloadStatus));
		}
		else
		{
			if (downloadedSum > contentLength)
				downloadedSum = contentLength - 1;
			DOWNLOADSTATUS downloadStatus = { downloadedSum, contentLength + 1 }; // + 1 for download of signature file
			::SendMessage(m_hWnd, m_uiMsg, 0, reinterpret_cast<LPARAM>(&downloadStatus));
		}

		if (::WaitForSingleObject(*m_eventStop, 0) == WAIT_OBJECT_0)
		{
			InternetCloseHandle(hResourceHandle);
			InternetCloseHandle(hConnectHandle);
			return (DWORD)E_ABORT; // canceled by the user
		}
	}
	while (true);
	destinationFile.Close();
	InternetCloseHandle(hResourceHandle);
	InternetCloseHandle(hConnectHandle);
	if (downloadedSum == 0)
	{
		CTraceToOutputDebugString::Instance()(_T(__FUNCTION__) _T(": Download size of %s was zero.\n"), url);
		return (DWORD)INET_E_DOWNLOAD_FAILURE;
	}
	return ERROR_SUCCESS;
}
