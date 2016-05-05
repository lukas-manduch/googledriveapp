#pragma once
#include "cloud_uploader/cloud_uploader.h"
#include <string>
#include <fstream>

class Google_drive_uploader : public Cloud_uploader
{
public:
	Google_drive_uploader(_In_ const std::string& refresh_token, _In_  const std::string& client_id , _In_ const std::string& client_secret);
	~Google_drive_uploader() override {};

	_Check_return_  bool upload_file(_In_ const std::wstring& path) override;
	_Check_return_  bool upload_file(_In_ const std::wstring& path, File_type type) override;
	_Check_return_  bool upload_folder(_In_ const std::wstring& path) override;
	
	// Static method
	_Check_return_ static bool is_refresh_token_valid(_In_ const std::string& refresh_token, _In_ const std::string& client_id, _In_ const std::string& client_secret);
private:
	_Check_return_ std::string get_access_token();
	_Check_return_ std::string single_upload( _In_ std::string file_content , _In_ std::string file_type ); // Returns file id
	_Check_return_ void rename_file( _In_ std::string& file_id, _In_ std::string& file_name); // takes utf8 formatted file name
	_Check_return_ std::string create_directory(_In_ std::string& directory_name); // takes utf8 formatted directory name, returns directory id

	std::string file_type_to_string(Cloud_uploader::File_type type);

	// MEMBERS
	std::string m_refresh_token;
	std::string m_client_id;
	std::string m_client_secret;
};
