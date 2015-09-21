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
#include "IntegrityActions.h"
#include "IntegrityResponse.h"
#include "EventLog.h"
#include "CrashReport.h"
#include "DebugEventLog.h"
#include <sstream>
#include <chrono>
#include <future>
#include <mutex>

#define DEFAULT_BUFFER_SIZE 1024

namespace IntegrityActions {
	std::mutex actionMutex;

	void displayException(const IntegrityResponse& response);
	void logAnyExceptions(const IntegrityResponse& response);
	void executeUserCommand(const IntegritySession& session, const IntegrityCommand& command, std::function<void()>);

	void launchSandboxView(const IntegritySession& session, std::wstring path)
	{
		IntegrityCommand command(L"si", L"viewsandbox");
		command.addOption(L"cwd", path);
		command.addOption(L"g");

		executeUserCommand(session, command, nullptr);
	}
	void viewMyChangePackages(const IntegritySession& session, std::wstring path)
	{
		IntegrityCommand command(L"si", L"viewcps");
		command.addOption(L"g");
		command.addOption(L"fields", L"id,summary,description,cptype,creationdate,issue");
		
		executeUserCommand(session, command, nullptr);
	}

	void viewRelatedChangePackages(const IntegritySession& session, std::wstring path)
	{
		/*const std::wstring folder;*/
		std::shared_ptr<IntegrityActions::FolderProperties> folderProp =
			IntegrityActions::getFolderInfo(session, path);

		// Can't retrieve properties
		if (!folderProp) {
			return;
		}

		// Extract properties to be displayed from properties object
		std::wstring developmentPath = folderProp->getDevelopmentPath();
		std::wstring projectName = folderProp->getProjectName();

		std::wstring variantFilter = L"variant:" + developmentPath;
		std::wstring projectFilter = L"project:" + projectName;

		//Filter for Variant 
		if (!developmentPath.empty())
		{
			IntegrityCommand command(L"si", L"viewcps");
			command.addOption(L"filter", variantFilter);
			command.addOption(L"g");
			command.addOption(L"fields", L"id,summary,description,cptype,creationdate,issue");

			executeUserCommand(session, command, nullptr);
		}

		//Filter for Mainline (Project)
		else
		{
			IntegrityCommand command(L"si", L"viewcps");
			command.addOption(L"filter", projectFilter);
			command.addOption(L"g");
			command.addOption(L"fields", L"id,summary,description,cptype,creationdate,issue");

			executeUserCommand(session, command, nullptr);
		}

	}

	void viewMyProjectHistory(const IntegritySession& session, std::wstring path)
	{
		IntegrityCommand command(L"si", L"viewprojecthistory");
		command.addOption(L"cwd", path);
		command.addOption(L"g");

		executeUserCommand(session, command, nullptr);
	}

	void viewMyProjectDifferences(const IntegritySession& session, std::wstring path)
	{
		IntegrityCommand command(L"si", L"mods");
		command.addOption(L"cwd", path);
		command.addOption(L"g");

		executeUserCommand(session, command, nullptr);
	}

	void viewMyReviews(const IntegritySession& session, std::wstring path)
	{
		IntegrityCommand command(L"si", L"viewcps");
		command.addOption(L"g");
		command.addOption(L"myReviews");

		executeUserCommand(session, command, nullptr);
	}

	void mergeConflicts(const IntegritySession& session, std::wstring path)
	{
		IntegrityCommand command(L"si", L"merge");
		command.addOption(L"cwd", path);
		command.addOption(L"g");
		command.addSelection(path);

		executeUserCommand(session, command, nullptr);
	}

	void createSandbox(const IntegritySession& session, std::wstring path, std::function<void()> onDone)
	{
		IntegrityCommand command(L"si", L"createsandbox");
		command.addOption(L"g");
		command.addOption(L"noOpenView");
		command.addSelection(path);

		executeUserCommand(session, command, onDone);
	}

	void addFiles(const IntegritySession& session, std::wstring sandbox, std::vector<std::wstring> paths, std::function<void()> onDone) 
	{
		// note this command presumes that the vector of paths (non-members) are in the same sandbox
		IntegrityCommand command(L"si", L"add");
		command.addOption(L"g");
		command.addOption(L"sandbox", sandbox);
		
		for (std::wstring path : paths) {
			command.addSelection(path);
		}

		executeUserCommand(session, initializeWFExecute(command), onDone);

	}

	void checkoutFiles(const IntegritySession& session, std::wstring sandbox, std::vector<std::wstring> paths, std::function<void()> onDone) 
	{
		// note this command presumes that the array of files are in the same sandbox
		IntegrityCommand command(L"si", L"co");
		command.addOption(L"g");
		command.addOption(L"sandbox", sandbox);
		command.addOption(L"revision", L":working");

		for (std::wstring path : paths) {
			command.addSelection(path);
		}

		executeUserCommand(session, initializeWFExecute(command), onDone);
	}

	void checkinFiles(const IntegritySession& session, std::wstring sandbox, std::vector<std::wstring> paths, std::function<void()> onDone)
	{
		// note this command presumes that the array of files are in the same sandbox
		IntegrityCommand command(L"si", L"ci");
		command.addOption(L"g");
		command.addOption(L"sandbox", sandbox);

		for (std::wstring path : paths) {
			command.addSelection(path);
		}

		executeUserCommand(session, initializeWFExecute(command), onDone);
	}

	void dropPaths(const IntegritySession& session, std::wstring sandbox, std::vector<std::wstring> paths, std::function<void()> onDone)
	{
		// note this command presumes that the array of files are in the same sandbox
		IntegrityCommand command(L"si", L"drop");
		command.addOption(L"g");
		command.addOption(L"sandbox", sandbox);

		for (std::wstring path : paths) {
			command.addSelection(path);
		}

		executeUserCommand(session, initializeWFExecute(command), onDone);
	}

	void lockFiles(const IntegritySession& session, std::vector<std::wstring> paths, std::function<void()> onDone)
	{
		IntegrityCommand command(L"si", L"lock");
		command.addOption(L"revision", L":working");
		command.addOption(L"g");

		for (std::wstring path : paths) {
			command.addSelection(path);
		}

		executeUserCommand(session, initializeWFExecute(command), onDone);
	}

	void revertFiles(const IntegritySession& session, std::vector<std::wstring> paths, std::function<void()> onDone)
	{
		IntegrityCommand command(L"si", L"revert");
		command.addOption(L"g");

		for (std::wstring path : paths) {
			command.addSelection(path);
		}

		executeUserCommand(session, initializeWFExecute(command), onDone);
	}

	void moveFiles(const IntegritySession& session, std::vector<std::wstring> paths, std::function<void()> onDone) 
	{
		IntegrityCommand command(L"si", L"move");
		command.addOption(L"g");
		
		for (std::wstring path : paths) {
			command.addSelection(path);
		}

		executeUserCommand(session, initializeWFExecute(command), onDone);
	}

	void renameFiles(const IntegritySession& session, std::vector<std::wstring> paths, std::function<void()> onDone) 
	{
		IntegrityCommand command(L"si", L"rename");
		command.addOption(L"g");

		for (std::wstring path : paths) {
			command.addSelection(path);
		}

		executeUserCommand(session, initializeWFExecute(command), onDone);
	}

	void resyncFiles(const IntegritySession& session, std::vector<std::wstring> paths, std::function<void()> onDone)
	{
		IntegrityCommand command(L"si", L"resync");

		command.addOption(L"g");
		command.addOption(L"byCP");

		for (std::wstring path : paths) {
			command.addSelection(path);
		}

		executeUserCommand(session, initializeWFExecute(command), onDone);
	}

	void resyncEntireSandbox(const IntegritySession& session, std::wstring path, std::function<void()> onDone)
	{
		IntegrityCommand command(L"si", L"resync");
		command.addOption(L"g");
		command.addOption(L"sandbox", path);

		executeUserCommand(session, command, onDone);
	}

	void dropSandbox(const IntegritySession& session, std::wstring path, std::function<void()> onDone)
	{
		IntegrityCommand command(L"si", L"dropsandbox");
		command.addOption(L"g");
		command.addSelection(path);

		executeUserCommand(session, command, onDone);
	}

	void retargetSandbox(const IntegritySession& session, std::wstring path, std::function<void()> onDone)
	{
		IntegrityCommand command(L"si", L"retargetsandbox");
		command.addOption(L"g");
		command.addSelection(path);

		executeUserCommand(session, command, onDone);
	}

	/*bool submitCP(const IntegritySession &session, std::wstring cpid)
	{
		IntegrityCommand command(L"si", L"submitcp");
		command.addOption(L"g");
		command.addSelection(cpid);

		std::unique_ptr<IntegrityResponse> response = session.execute(command);

		if (response->getException() != NULL) {
			logAnyExceptions(*response);
			return false;
		}

		if (response->getExitCode() != 0) {
			return false;
		}

		return true;
	}*/

	void setExcludeFileFilter(const IntegritySession& session, std::vector<std::wstring> patterns, std::function<void()> onDone)
	{
		IntegrityCommand command(L"si", L"setprefs");
		command.addOption(L"command", L"viewnonmembers");

		std::wstring patternString;
		if (!patterns.empty()) {
			std::wostringstream ss;

			// build list of filter patterns, separating with commas. Add last one manually so we don't have trailing comma
			std::copy(patterns.begin(), patterns.end() - 1, std::ostream_iterator<std::wstring, wchar_t>(ss, L","));
			ss << patterns.back();

			patternString = ss.str();
		}

		command.addSelection(L"excludeFilter=" + patternString);

		executeUserCommand(session, command, onDone);
	}


	// get status flags for a set of files...
	FileStatusFlags fileInfo(const IntegritySession& session, const std::wstring& file) 
	{
		// Added mutex to prevent situation where multiple Integrity client sessions are
		// started by the file status cache, server connection cache and root folder cache
		std::lock_guard<std::mutex> lock(actionMutex);

		IntegrityCommand command(L"wf", L"fileinfo");
		command.addSelection(file);

		std::unique_ptr<IntegrityResponse> response = session.execute(command);

		if (response->getException() != NULL) {
			logAnyExceptions(*response);
			return NO_STATUS;
		} 

		if (response->getExitCode() != 0) { 
			EventLog::writeWarning(L"get status has non zero exit code");
		}

		for (mksWorkItem item : *response) {
			int status = getIntegerFieldValue(item, L"status", NO_STATUS);

			EventLog::writeInformation(std::wstring(L"wf fileInfo ") + file + L" has status " +  std::to_wstring(status));
			return status;
		}
		return NO_STATUS;
	}


	/**
	*  Retrieve the name of the sandbox using the 'sandboxinfo' command
	*/
	std::wstring getSandboxName(const IntegritySession& session, std::wstring path) {

		std::wstring sandboxName;

		IntegrityCommand command(L"si", L"sandboxinfo");
		command.addOption(L"cwd", path);
		std::unique_ptr<IntegrityResponse> response = session.execute(command);

		if (response->getException() != NULL) {
			logAnyExceptions(*response);
			displayException(*response);
			return sandboxName;
		}

		for (mksWorkItem item : *response) {
			return getId(item);
		}

		return sandboxName;
	}

	/**
	*  Retrieve a list of all sandboxes using the 'sandboxes' command
	*/
	std::vector<std::wstring> getSandboxList(const IntegritySession& session) {

		std::vector<std::wstring> sandboxPaths;
		IntegrityCommand command(L"si", L"sandboxes");

		std::unique_ptr<IntegrityResponse> response = session.execute(command);

		if (response->getException() != NULL) {
			logAnyExceptions(*response);
			displayException(*response);
			return sandboxPaths;
		}

		for (mksWorkItem item : *response) {

			// Retrieve sandbox name - sandbox directory including
			// associated .pj file
			std::wstring id = getId(item);

			// Convert all to lowercase
			std::transform(id.begin(), id.end(), id.begin(), ::tolower);

			sandboxPaths.push_back(id);
		}

		return sandboxPaths;
	}

	/**
	* Retrieve member properties
	*/
	std::shared_ptr<IntegrityActions::MemberProperties> getMemberInfo(const IntegritySession& session, const std::wstring& file) {

		std::wstring folder;
		std::vector<std::wstring> memberInfo;

		mksItem revItem = NULL;
		mksItem workRevItem = NULL;
		mksItem cpIdItem = NULL;
		mksItemList lockItemList = NULL;

		// Create member properties object
		std::shared_ptr<IntegrityActions::MemberProperties> memberProps = 
			std::shared_ptr<IntegrityActions::MemberProperties>(new IntegrityActions::MemberProperties());

		IntegrityCommand command(L"si", L"viewsandbox");

		folder = file.substr(0, file.find_last_of('\\'));

		command.addOption(L"cwd", folder);
		command.addOption(L"fields", L"cpid,lockrecord,memberrev,workingrev");
		command.addSelection(file);

		// Wrap command with wf execute
		IntegrityCommand wfCommand = initializeWFExecute(command);

		std::unique_ptr<IntegrityResponse> response = session.execute(wfCommand);

		if (response->getException() != NULL) {
			logAnyExceptions(*response);
			displayException(*response);
			return NULL;
		}

		// Retrieve property values and populate member prop object
		for (mksWorkItem workItem : *response) {

			revItem = getItemFieldValue(workItem, L"memberrev");
			workRevItem = getItemFieldValue(workItem, L"workingrev");
			cpIdItem = getItemFieldValue(workItem, L"cpid");
			lockItemList = getItemListFieldValue(workItem, L"lockrecord");
		}

		// Set the member properties
		memberProps->setMemberRev(revItem ? getId(revItem) : _T(""));
		memberProps->setWorkingRev(workRevItem ? getId(workRevItem) : _T(""));
		memberProps->setChangePackageId(cpIdItem ? getId(cpIdItem) : _T(""));

		for (mksItem lockItem = mksItemListGetFirst(lockItemList); lockItemList && lockItem; lockItem = mksItemListGetNext(lockItemList)) {

			std::wstring lockType;
			mksItem locker;
			mksItem lockcpid;

			locker = getItemFieldValue(lockItem, L"locker");
			lockcpid = getItemFieldValue(lockItem, L"lockcpid");
			lockType = getStringFieldValue(lockItem, L"locktype");

			// Create locker properties and add to member properties
			memberProps->addLockerProperties(std::shared_ptr<IntegrityActions::LockProperties>
				(new IntegrityActions::LockProperties(locker ? getId(locker) : _T(""), lockcpid ? getId(lockcpid) : _T(""), lockType)));
		}

		// Retrieve sandbox name
		memberProps->setSandboxName(getSandboxName(session, file));

		return memberProps;
	}

	/**
	* Retrieve folder properties to be displayed in the properties sheet
	*/
	std::shared_ptr<IntegrityActions::FolderProperties> getFolderInfo(const IntegritySession& session, const std::wstring& folder) {
	
		mksItem devPathItem = NULL;
		mksItem revItem = NULL;

		// Create folder properties object which will be populated with data
		// and read when folder properties sheet is created
		std::shared_ptr<IntegrityActions::FolderProperties> folderProps =
			std::shared_ptr<IntegrityActions::FolderProperties>(new IntegrityActions::FolderProperties());

		// Use sandboxinfo Integrity command to retrieve folder properties
		IntegrityCommand command(L"si", L"sandboxinfo");
		command.addOption(L"cwd", folder);

		std::unique_ptr<IntegrityResponse> response = session.execute(command);

		// Return NULL if there is a problem running the command
		if (response->getException() != NULL) {
			logAnyExceptions(*response);
			displayException(*response);
			return NULL;
		}

		// Retrieve data from Integrity response
		for (mksWorkItem workItem : *response) {

			// Populate folder properties object
			folderProps->setProjectName(getStringFieldValue(workItem, L"projectName"));
			folderProps->setSandboxName(getStringFieldValue(workItem, L"sandboxName"));
			folderProps->setServerName(getStringFieldValue(workItem, L"server"));
			folderProps->setServerPort(getStringFieldValue(workItem, L"serverPort"));
			folderProps->setLastCheckpoint(getDateTimeFieldValue(workItem, L"lastCheckpoint"));

			// Retrieve items from response
			revItem = getItemFieldValue(workItem, L"revision");
			devPathItem = getItemFieldValue(workItem, L"developmentPath");
		}

		// Populate folder properties with checkpoint and development path info
		folderProps->setRevision(revItem ? getId(revItem) : _T(""));
		folderProps->setDevelopmentPath(devPathItem ? getId(devPathItem) : _T(""));

		// Populated folder properties object
		return folderProps;
	}

	/**
	*  Retrieve current excludeFilter contents using 'viewprefs' command
	*/
	std::vector<std::wstring> getExcludeFilterContents(const IntegritySession& session) {

		std::vector<std::wstring> filterContents;
		IntegrityCommand command(L"si", L"viewprefs");
		command.addOption(L"command", L"viewnonmembers");

		std::unique_ptr<IntegrityResponse> response = session.execute(command);

		if (response->getException() != NULL) {
			logAnyExceptions(*response);
			displayException(*response);
			return filterContents;
		}

		std::wstring contents;
		for (mksWorkItem item : *response) {
			std::wstring id = getId(item);

			mksItem setting = getItemFieldValue(item, L"excludeFilter");
			if (setting != NULL) {
				contents = getStringFieldValue(setting, L"default");
			}

		}

		if (!contents.empty()) {
			wchar_t* token = wcstok((wchar_t*)contents.c_str(), L",");
			while (token != NULL) {
				filterContents.push_back(token);
				token = wcstok(NULL, L",");
			}
		}

		return filterContents;

	}

	/**
	 * Retrieve list of change packages
	 */
	std::vector<std::shared_ptr<IntegrityActions::ChangePackage> *> getChangePackageList(const IntegritySession& session) {
		std::vector<std::shared_ptr<IntegrityActions::ChangePackage> *> cps;
		IntegrityCommand command(L"si", L"viewcps");
		command.addOption(L"fields", L"id,summary,description,cptype,creationdate,issue");
		

		std::unique_ptr<IntegrityResponse> response = session.execute(command);

		if (response->getException() != NULL) {
			logAnyExceptions(*response);
			displayException(*response);
			return cps;
		}

		for (mksWorkItem item : *response) {
			std::wstring id = getId(item);
			std::wstring summary = getStringFieldValue(item, L"summary");
			std::wstring description = getStringFieldValue(item, L"description");
			std::wstring cptype = getStringFieldValue(item, L"cptype");
			time_t creationDate = getDateTimeFieldValue(item, L"creationdate");
			std::wstring issueId = getStringFieldValue(item, L"issue");

			// Create change package object
			std::shared_ptr<IntegrityActions::ChangePackage> *cp = new std::shared_ptr<IntegrityActions::ChangePackage>(IntegrityActions::ChangePackage::ChangePackageBuilder()
				.setId(id)
				.setSummary(summary)
				.setDescription(description)
				.setType(cptype)
				.setCreationDate(creationDate)
				.setIssueId(issueId)
				.build());

			cps.push_back(cp);
		}

		// Sort by cps creation date (see overloaded < operator for ChangePackage class)
		std::sort(cps.begin(), cps.end());

		return cps;
	}

	/**
	 * Retrieve list of working file changes for path 
	 */
	std::vector<std::shared_ptr<IntegrityActions::WorkingFileChange>> getWorkingFileChanges(const IntegritySession& session, std::wstring path) {
		std::vector<std::shared_ptr<IntegrityActions::WorkingFileChange>> changes;
		IntegrityCommand command(L"wf", L"changes");
		command.addSelection(path);

		std::unique_ptr<IntegrityResponse> response = session.execute(command);
	
		if (response->getException() != NULL) {
			logAnyExceptions(*response);
			displayException(*response);
			return changes;
		}
		
		for (mksWorkItem item : *response) {

			std::wstring id = getId(item);
			int status = getIntegerFieldValue(item, L"status", NO_STATUS);

			// Create change package object
			std::shared_ptr<IntegrityActions::WorkingFileChange> change = std::shared_ptr<IntegrityActions::WorkingFileChange>(IntegrityActions::WorkingFileChange::WorkingFileChangeBuilder()
				.setId(id)
				.setStatus(status)
				.build());

			changes.push_back(change);
		}

		return changes;
	}

	/**
	 * Launch Create Change Package View
	 */
	bool launchCreateCPView(const IntegritySession& session, std::wstring& cpid) {
		IntegrityCommand command(L"si", L"createcp");
		command.addOption(L"g");

		std::unique_ptr<IntegrityResponse> response = session.execute(command);

		if (response->getException() != NULL) {
			logAnyExceptions(*response);
			return false;
		}

		if (response->getExitCode() != 0) {
			return false;
		}
 
		/**
		 * Interrogate the response to determine what cpid was created:
		 *
		 * <?xml version="1.1" ?>
		 * <Response app="si" version="4.14.0 0-0 0" command="createcp">
		 *     <App-Connection server="BLESKOWSKY0D" port="7001" userID="test1"></App-Connection>
		 *     <Result>
		 *         <Message>Created new change package 5:2</Message>
		 *         <Field name="resultant">
		 *             <Item id="5:2" modelType="si.ChangePackage" displayId="5:2">
		 *             </Item>
		 *         </Field>
		 *     </Result>
		 *     <ExitCode>0</ExitCode>
		 * </Response>
		 */
		mksResult result = mksResponseGetResult(response->getResponse());

		if (result != NULL) {
			wchar_t buffer[DEFAULT_BUFFER_SIZE] = { '\0' };
			mksField field = mksResultGetField(result, L"resultant");
			mksItem item;

			mksFieldGetItemValue(field, &item);
			mksItemGetId(item, buffer, DEFAULT_BUFFER_SIZE);
			cpid = buffer;
		} else {
			return false;
		}

		return true;
	}

	/**
	*  Launch Member History View
	*/
	void launchMemberHistoryView(const IntegritySession& session, std::wstring path) {
		IntegrityCommand command(L"si", L"viewhistory");
		command.addOption(L"g");
		command.addSelection(path);

		IntegrityCommand wfCommand = initializeWFExecute(command);

		executeUserCommand(session, wfCommand, nullptr);
	}

	/**
	*  Launch Annotated Revision View
	*/
	void launchAnnotatedRevisionView(const IntegritySession& session, std::wstring path) {
		IntegrityCommand command(L"si", L"annotate");
		command.addOption(L"g");
		command.addSelection(path);

		IntegrityCommand wfCommand = initializeWFExecute(command);

		executeUserCommand(session, wfCommand, nullptr);
	}

	/**
	*  Launch Member Information View
	*/
	void launchMemberInfoView(const IntegritySession& session, std::wstring path) {
		IntegrityCommand command(L"si", L"memberinfo");
		command.addOption(L"g");
		command.addSelection(path);

		IntegrityCommand wfCommand = initializeWFExecute(command);

		executeUserCommand(session, wfCommand, nullptr);
	}

	/**
	*  Launch Differences View on local member
	*/
	void launchLocalChangesDiffView(const IntegritySession& session, std::wstring path) {
		IntegrityCommand command(L"si", L"diff");
		command.addOption(L"g");
		command.addSelection(path);

		IntegrityCommand wfCommand = initializeWFExecute(command);

		executeUserCommand(session, wfCommand, nullptr);
	}
	/**
	*  Launch WorkingFileChanges View
	*/
	void launchWorkingFileChangesView(const IntegritySession& session, std::wstring path) {
		IntegrityCommand command(L"si", L"si.WorkingFileChangesView");
		command.addOption(L"cwd", path);
		command.addOption(L"g");

		executeUserCommand(session, command, nullptr);
	}


	std::vector<std::wstring> folders(const IntegritySession& session)
	{
		// Added mutex to prevent situation where multiple Integrity client sessions are
		// started by the file status cache, server connection cache and root folder cache
		std::lock_guard<std::mutex> lock(actionMutex);

		IntegrityCommand command(L"wf", L"folders");
		std::vector<std::wstring> rootPaths;

		std::unique_ptr<IntegrityResponse> response = session.execute(command);

		if (response->getException() != NULL) {
			logAnyExceptions(*response);
			return rootPaths;
		}

		// Retrieve the id of the returned sandbox
		for (mksWorkItem item : *response) {
			rootPaths.push_back(getId(item));
		}

		return rootPaths;
	}

	std::vector<std::wstring> servers(const IntegritySession& session)
	{
		// Added mutex to prevent situation where multiple Integrity client sessions are
		// started by the file status cache, server connection cache and root folder cache
		std::lock_guard<std::mutex> lock(actionMutex);

		IntegrityCommand command(L"si", L"servers");
		std::vector<std::wstring> servers;

		std::unique_ptr<IntegrityResponse> response = session.execute(command);

		if (response->getException() != NULL) {
			logAnyExceptions(*response);
			return servers;
		}

		for (mksWorkItem item : *response) {
			servers.push_back(getId(item));
		}

		return servers;
	}

	bool connect(const IntegritySession& session) 
	{
		IntegrityCommand command(L"si", L"connect");
		command.addOption(L"g");

		std::unique_ptr<IntegrityResponse> response = session.execute(command);

		if (response->getException() != NULL) {
			logAnyExceptions(*response);
			return false;
		}

		if (response->getExitCode() != 0) {
			return false;
		}

		return true;
	}



	void logAnyExceptions(const IntegrityResponse& response) {
		if (response.getException() != NULL) {
			std::wstring message = std::wstring(L"Error encountered running command '")
				+ response.getCommand().getApp() + L" " + response.getCommand().getName()
				+ L"': \n\terror id = "
				+ getId(response.getException()) + L"\n\t message = '"
				+ getExceptionMessage(response.getException()) + L"'";

			for (mksWorkItem item : response) {
				mksAPIException workItemException = mksWorkItemGetAPIException(item);

				if (workItemException != NULL) {
					message = std::wstring(L"\nError processing item '")
						+ getId(item) + L"' with type '" + getModelType(item)
						+ L"': \n\terror id = "
						+ getId(response.getException()) + L"\n\t message = '"
						+ getExceptionMessage(response.getException()) + L"'";
				}
			}

			EventLog::writeError(message);
		}
	}

	void displayException(const IntegrityCommand& command, std::wstring errorId, std::wstring msg) {
		std::wstring message = std::wstring(L"Error encountered running command '")  
			+ command.getApp() + L" " + command.getName()
			+ L"': \n\terror id = " + errorId + L"\n\t message = '" + msg + L"'";

		MessageBoxW(NULL, message.c_str(), L"Error", MB_OK | MB_ICONEXCLAMATION);
	}

	void executeUserCommand(const IntegritySession& session, const IntegrityCommand& command, std::function<void()> onDone) {
		std::async(std::launch::async, [&](IntegrityCommand command, std::function<void()> onDone){
			try {
				std::unique_ptr<IntegrityResponse> response = session.execute(command);

				logAnyExceptions(*response);


				if (onDone != nullptr) {
					onDone();
				}
			}
			catch (std::exception)
			{
			}
		}, command, onDone);
	}

	void displayException(const IntegrityResponse& response) {
		mksAPIException exception = response.getException();
		if (exception != NULL) {
			for (mksWorkItem item : response) {
				mksAPIException workItemException = mksWorkItemGetAPIException(item);

				if (workItemException != NULL) {
					displayException(response.getCommand(), getId(workItemException),
						getExceptionMessage(workItemException));
					return;
				}
			}
			
			displayException(response.getCommand(), getId(exception),
				getExceptionMessage(exception));
		}
	}

	IntegrityCommand initializeWFExecute(const IntegrityCommand& command) {
		IntegrityCommand wfcommand(L"wf", L"execute");

		wfcommand.addSelection(command.getName());

		for (IntegrityCommand::Option option : command.options) {
			wfcommand.addSelection(option.getAsString());
		}

		// explicitly mark end of options and start of selection 
		wfcommand.addSelection(L"--");

		for (std::wstring selectionItem : command.selection) {
			wfcommand.addSelection(selectionItem);
		}
		return wfcommand;
	}

}