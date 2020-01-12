/*
===========================================================================

Daemon GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.

This file is part of the Daemon GPL Source Code (Daemon Source Code).

Daemon Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Daemon Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Daemon Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Daemon Source Code is also subject to certain additional terms.
You should have received a copy of these additional terms immediately following the
terms and conditions of the GNU General Public License which accompanied the Daemon
Source Code.  If not, please request a copy in writing from id Software at the address
below.

If you have questions concerning this license or the applicable additional terms, you
may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville,
Maryland 20850 USA.

===========================================================================
*/

/* Additional features that would be nice for this code:
        * Only display <gamepath>/<file>, i.e., etpro/etpro-3_0_1.pk3 in the UI.
        * Add server as referring URL
*/

#include "common/Common.h"

#include <curl/curl.h>

#include "common/FileSystem.h"
#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"

extern Log::Logger downloadLogger; // cl_download.cpp
extern Cvar::Cvar<int> cl_downloadCount; // cl_download.cpp

namespace {

class CurlDownload {
	CURLM* multi_ = nullptr;
	CURL* request_ = nullptr;
	dlStatus_t status_ = dlStatus_t::DL_FAILED;

public:
	CurlDownload(Str::StringRef url) {
		multi_ = curl_multi_init();
		if (!multi_) {
			downloadLogger.Warn("curl_multi_init returned null");
			return;
		}
		request_ = curl_easy_init();
		if (!request_) {
			downloadLogger.Warn( "curl_easy_init returned null" );
			return;
		}
		if (!SetOptions(url)) {
			return;
		}
		CURLMcode err = curl_multi_add_handle(multi_, request_);
		if (err != CURLM_OK)
		{
			downloadLogger.Warn("curl_multi_add_handle error: %s", curl_multi_strerror(err));
			curl_easy_cleanup(request_);
			request_ = nullptr;
			return;
		}
		status_ = dlStatus_t::DL_CONTINUE;
	}

	CurlDownload(CurlDownload&&) = delete; // Disallow copy construction and assignment (yes this is a move constructor)

	void Advance() {
		if (status_ != dlStatus_t::DL_CONTINUE) {
			return;
		}
		int numRunningTransfers;
		CURLMcode err = curl_multi_perform(multi_, &numRunningTransfers);
		if (err != CURLM_OK) {
			downloadLogger.Warn("curl_multi_perform error: %s", curl_multi_strerror(err));
			status_ = dlStatus_t::DL_FAILED;
		}
		if (status_ != dlStatus_t::DL_CONTINUE) { // may be set by callback
			return;
		}
		if (numRunningTransfers > 0) {
			return;
		}
		CURLMsg* msg;
		do {
			int ignored;
			msg = curl_multi_info_read(multi_, &ignored);
		} while (msg && msg->msg != CURLMSG_DONE);

		if (!msg) {
			status_ = dlStatus_t::DL_FAILED;
			downloadLogger.Warn("Unexpected lack of CURLMSG_DONE");
		}

		if (msg->data.result != CURLE_OK) {
			downloadLogger.Notice("Download request terminated with failure status '%s'", err);
			status_ = dlStatus_t::DL_FAILED;
			return;
		}
		long httpStatus = -1;
		curl_easy_getinfo(request_, CURLINFO_RESPONSE_CODE, &httpStatus); // ignore return code and fail due to httpStatus = -1
		if (httpStatus != 200) {
			// We don't follow redirects, so report a failure if we get one
			// (they're not considered an error for CURLOPT_FAILONERROR purposes).
			downloadLogger.Notice("Download failed: returned HTTP %d", httpStatus);
			status_ = dlStatus_t::DL_FAILED;
			return;
		}
		status_ = dlStatus_t::DL_DONE;
	}

	dlStatus_t Status() {
		return status_;
	}

protected:
	// return DL_CONTINUE to continue, DL_DONE or DL_FAILED to stop
	virtual dlStatus_t WriteCallback(const char* data, size_t len) = 0;

	virtual ~CurlDownload() {
		if (request_) {
			CURLMcode err = curl_multi_remove_handle(multi_, request_);
			if (err != CURLM_OK) {
				downloadLogger.Warn("curl_multi_remove_handle error: %s", curl_multi_strerror(err));
			}
			curl_easy_cleanup(request_);
		}
		if (multi_) {
			CURLMcode err = curl_multi_cleanup(multi_);
			if (err != CURLM_OK) {
				downloadLogger.Warn("curl_multi_cleanup error: %s", curl_multi_strerror(err));
			}
		}
	}

private:
	static size_t LibcurlWriteCallback(char* data, size_t, size_t len, void* object) {
		auto* download = static_cast<CurlDownload*>(object);
		download->status_ = download->WriteCallback(data, len);
		return download->status_ == dlStatus_t::DL_CONTINUE ? len : ~size_t(0);
	}

	bool SetOptions(Str::StringRef url) {
#define SETOPT(option, value) \
if (curl_easy_setopt(request_, option, value) != CURLE_OK) { \
	downloadLogger.Warn("Setting " #option " failed"); \
	return false; \
}

		SETOPT( CURLOPT_USERAGENT, Str::Format( "%s %s", ENGINE_NAME "/" ENGINE_VERSION, curl_version() ).c_str() )
		SETOPT( CURLOPT_REFERER, Str::Format("%s%s", URI_SCHEME, Cvar::GetValue("cl_currentServerIP")).c_str() )
		SETOPT( CURLOPT_URL, url.c_str() )
		SETOPT( CURLOPT_PROTOCOLS, long(CURLPROTO_HTTP) )
		SETOPT( CURLOPT_WRITEFUNCTION, curl_write_callback(LibcurlWriteCallback) )
		SETOPT( CURLOPT_WRITEDATA, static_cast<void*>(this) )
		SETOPT( CURLOPT_FAILONERROR, 1L )
		return true;
	}
};

class FileDownload : public CurlDownload {
	FS::File file_;

	dlStatus_t WriteCallback(const char* data, size_t len) override {
		try {
			file_.Write(data, len);
		} catch (std::system_error& e) {
			downloadLogger.Notice("Error writing to download file: %s", e.what());
			return dlStatus_t::DL_FAILED;
		}
		cl_downloadCount.Set(cl_downloadCount.Get() + len);
		return dlStatus_t::DL_CONTINUE;
	}

public:
	FileDownload(Str::StringRef url, FS::File file) : CurlDownload(url), file_(std::move(file)) {}
};

// If servers could ask the client to download any URL, there would be a security issue: the URL
// could point to something on a private network that the server shouldn't have access to. Therefore,
// before an HTTP download commences, the client checks for a magic file named PAKSERVER in the same
// directory where the desired file is stored. If the file begins with the right magic string (we
// check only a prefix to avoid any fiddly newline issues), this is interpreted as permission
// Daemon clients to download anything from that directory.
//
// Note that this is the same issue addressed by the Same-Origin Policy in web browsers. In that
// model, CORS headers are used to give scripts permission to access resources. We couldn't really
// implement that here since there is no origin tracking of cgame binaries.
const Str::StringRef PAKSERVER_FILE_NAME = "PAKSERVER";
const Str::StringRef PAKSERVER_FILE_CONTENT_PREFIX = "ALLOW_UNRESTRICTED_DOWNLOAD";
class PakserverCheck : public CurlDownload {
	Str::StringRef unmatchedPrefix_ = PAKSERVER_FILE_CONTENT_PREFIX;

	dlStatus_t WriteCallback(const char* data, size_t len) override {
		size_t n = std::min(len, unmatchedPrefix_.size());
		if (0 != memcmp(unmatchedPrefix_.c_str(), data, n)) {
			return dlStatus_t::DL_FAILED;
		}
		unmatchedPrefix_ = unmatchedPrefix_.suffix(n);
		return unmatchedPrefix_.empty() ? dlStatus_t::DL_DONE : dlStatus_t::DL_CONTINUE;
	}

public:
	PakserverCheck(Str::StringRef url) : CurlDownload(url) {}
};

struct DownloadState {
	Util::optional<PakserverCheck> pakserverCheck;
	Util::optional<FileDownload> actualDownload;
	std::string url;
	std::string homepathPath; // should begin with pkg/
};

} // namespace


// initialize once
static int   dl_initialized = 0;

static DownloadState download;


void DL_InitDownload()
{
	if ( dl_initialized )
	{
		return;
	}

	/* Make sure curl has initialized, so the cleanup doesn't get confused */
	if ( curl_global_init( CURL_GLOBAL_ALL ) == CURLE_OK )
	{
		downloadLogger.Debug( "Client download subsystem initialized" );
		dl_initialized = 1;
	}
	else
	{
		downloadLogger.Warn( "Error initializing libcurl" );
	}
}

// TODO: call this function whenever a download is cancelled
static void DL_StopDownload()
{
	if (download.actualDownload)
	{
		download.actualDownload = Util::nullopt;
		// TODO: kill temp file
	}
	download.~DownloadState();
	new (&download) DownloadState();
}

/*
================
DL_Shutdown

================
*/
void DL_Shutdown()
{
	if ( !dl_initialized )
	{
		return;
	}

	DL_StopDownload();

	curl_global_cleanup();

	dl_initialized = 0;
}

/*
===============
inspired from http://www.w3.org/Library/Examples/LoadToFile.c
setup the download, return once we have a connection
===============
*/
int DL_BeginDownload( const char *localName, const char *remoteName, int basePathLen )
{
	DL_StopDownload();

	DL_InitDownload();
	if ( !dl_initialized )
	{
		return 0;
	}

	// This URL parsing code is naive as it doesn't consider the possibility of params,
	// anchors, or whatever, but regardless of what comes out, it should do the job of
	// preventing downloading things that shouldn't be accessed.
	std::string urlDir = remoteName;
#if 0 // TODO(0.52) switch on
	if (basePathLen < 2 || static_cast<size_t>(basePathLen) + 1 >= urlDir.size() || urlDir[basePathLen - 1] != '/') {
		downloadLogger.Notice("Bad download base path specification");
		return 0;
	}
	urlDir = urlDir.substr(0, basePathLen);
#else
	Q_UNUSED(basePathLen);
	size_t slash = urlDir.rfind('/');
	if (slash == 0 || slash == std::string::npos || slash + 1 == urlDir.size()) {
		downloadLogger.Notice("Bogus download url '%s'", remoteName);
		return 0;
	}
	urlDir = urlDir.substr(0, slash + 1);
#endif

	downloadLogger.Debug("Checking for PAKSERVER file in %s", urlDir);
	download.pakserverCheck.emplace(urlDir + PAKSERVER_FILE_NAME);
	download.url = remoteName;
	download.homepathPath = localName;
	Cvar_Set( "cl_downloadName", remoteName );
	return 1;
}

static void StartRealDownload() {
	FS::File file;
	try {
		file = FS::HomePath::OpenWrite(download.homepathPath);
	} catch (std::system_error& e) {
		downloadLogger.Notice( "DL_BeginDownload unable to open '%s' for writing: %s", download.homepathPath, e.what() );
		return;
	}
	downloadLogger.Debug("Starting HTTP download of %s", download.url);
	download.actualDownload.emplace(download.url, std::move(file));
}

// (maybe this should be CL_DL_DownloadLoop)
dlStatus_t DL_DownloadLoop()
{
	if ( !download.pakserverCheck && !download.actualDownload )
	{
		downloadLogger.Warn( "DL_DownloadLoop: unexpected call with no active request" );
		return dlStatus_t::DL_DONE;
	}

	if ( download.pakserverCheck )
	{
		download.pakserverCheck->Advance();
		switch (download.pakserverCheck->Status()) {
		case dlStatus_t::DL_CONTINUE:
			return dlStatus_t::DL_CONTINUE;
		case dlStatus_t::DL_DONE:
			download.pakserverCheck = Util::nullopt;
			StartRealDownload();
			if (!download.actualDownload)
			{
				DL_StopDownload();
				return dlStatus_t::DL_FAILED;
			}
			break;
		case dlStatus_t::DL_FAILED:
			DL_StopDownload();
			downloadLogger.Notice("Download server failed PAKSERVER check");
			return dlStatus_t::DL_FAILED;
		}
	}

	download.actualDownload->Advance();
	dlStatus_t status = download.actualDownload->Status();
	if ( status != dlStatus_t::DL_CONTINUE )
	{
		DL_StopDownload();
	}

	return status;
}
