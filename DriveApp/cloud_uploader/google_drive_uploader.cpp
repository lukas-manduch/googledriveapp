#include "cloud_uploader/google_drive_uploader.h"

#include <sstream>
#include <fstream>
#include "curl_cpp/curl.h"
#include "json/json.h"
#include <filesystem>

namespace fs = std::experimental::filesystem;

// -------------------------------------------------------------------------------------------
Google_drive_uploader::Google_drive_uploader
(
	const std::string & refresh_token, 
	const std::string & client_id, 
	const std::string & client_secret
)
	: m_refresh_token{refresh_token},
	m_client_id{client_id},
	m_client_secret{client_secret}
{
	if (!is_refresh_token_valid(m_refresh_token, m_client_id, m_client_secret))
		throw std::runtime_error{ "Invalid refresh token" };
}
// -------------------------------------------------------------------------------------------
bool Google_drive_uploader::upload_file(const std::wstring & path)
{
	
	return false;
}
// -------------------------------------------------------------------------------------------
bool Google_drive_uploader::upload_file(const std::wstring & path, File_type type)
{
	std::ifstream upload_file{ path };
	if (!upload_file.is_open())
		throw std::runtime_error{ "Invalid file name or permissions" };
	std::string upload_file_content((std::istreambuf_iterator<char>(upload_file)),
		std::istreambuf_iterator<char>());
	std::string file_id = single_upload(upload_file_content, file_type_to_string(type));
	if (file_id.empty())
		return false;
	std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8cvt;
	rename_file(file_id, utf8cvt.to_bytes(path));
	return true;
}
// -------------------------------------------------------------------------------------------
bool Google_drive_uploader::upload_folder(const std::wstring & path)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8cvt;
	create_directory(utf8cvt.to_bytes(path) );

	// Process the list of files found in the directory.
	for (const auto& p : fs::directory_iterator(path))
	{
		if (p.status().type() == fs::file_type::directory)
		{
			upload_folder (p.path().wstring());
		}
		else
		{
			std::wstring file_name = p.path().wstring();
			if (!upload_file(file_name, Cloud_uploader::File_type::text))
			{
				throw std::runtime_error{ "Upload unsuccessful" };
			}
		}
	}
	return true;
}
// -------------------------------------------------------------------------------------------
bool Google_drive_uploader::is_refresh_token_valid
(
	const std::string & refresh_token, 
	const std::string & client_id, 
	const std::string & client_secret
)
{
	std::stringstream post;
	post << "client_id=" << client_id << "&"
		<< "client_secret=" << client_secret << "&"
		<< "refresh_token=" << refresh_token << "&"
		<< "grant_type=refresh_token";

	std::string post_str = post.str();
	std::string token_uri = "https://www.googleapis.com/oauth2/v4/token";


	Curl curl{};
	curl.use_ssl(false).set_type(Curl::Request_type::post).set_url(token_uri);

	std::vector<char> response;
	curl.send(post.str(), response);
	
	Json::Reader reader;

	Json::Value ret_val;
	reader.parse(&response[0], &response[0] + response.size(), ret_val);
	if (ret_val["access_token"].asString().empty())
		return false;
	return true;
}
// -------------------------------------------------------------------------------------------
std::string Google_drive_uploader::get_access_token()
{
	std::stringstream post;
	post << "client_id=" << m_client_id << "&"
		<< "client_secret=" << m_client_secret << "&"
		<< "refresh_token=" << m_refresh_token << "&"
		<< "grant_type=refresh_token";

	std::string post_str = post.str();
	std::string token_uri = "https://www.googleapis.com/oauth2/v4/token";


	Curl curl{};
	curl.use_ssl(false).set_type(Curl::Request_type::post).set_url(token_uri);

	std::vector<char> response;
	int http = curl.send(post.str(), response);
	if (http < 200 || http > 299)
		throw std::runtime_error{ "Probably invalid refresh token" };

	Json::Reader reader;

	Json::Value ret_val;
	reader.parse(&response[0], &response[0] + response.size(), ret_val);
	std::string access_token = ret_val["access_token"].asString();
	if (access_token.empty())
		throw std::runtime_error{"Empty access token"};
	return access_token;
}
// -------------------------------------------------------------------------------------------
std::string Google_drive_uploader::single_upload(std::string file_content, std::string file_type)
{
	std::string access_token = get_access_token();

	std::vector<std::string> http_fields =
	{
		"Authorization: Bearer " + access_token,
		"Content-Type: " + file_type,
		"Content-Length: " + std::to_string(file_content.size()),
	};

	std::string url = "https://www.googleapis.com/upload/drive/v3/files?uploadType=media";

	Curl curl{};

	for (const auto& i : http_fields)
		curl.add_header(i);
	curl.use_ssl(false).set_type(Curl::Request_type::post).set_url(url);

	std::vector<char> response;

	int http_code = curl.send(file_content, response);
	if (http_code < 200 || http_code > 299)
		throw std::runtime_error{ "Upload unsuccessfull" };

	std::string file_stats{ response.data() , response.size() };
	Json::Value file_id_json;
	Json::Reader reader;
	reader.parse(file_stats.c_str(), file_id_json);
	if (file_id_json["id"].asString() == "")
	{
		throw std::runtime_error{ "Unable to read file id" };
	}
	return file_id_json["id"].asString();
}
// -------------------------------------------------------------------------------------------
void Google_drive_uploader::rename_file(std::string & file_id, std::string & file_name)
{
	std::string up_url = "https://www.googleapis.com/drive/v2/files/";
	up_url += file_id + "?key=" + m_client_id;

	Json::Value file_name_json;
	file_name_json["title"] = file_name;

	std::string access_token = get_access_token();

	std::vector<char> res;
	Curl curl;
	curl.set_type(Curl::Request_type::put).use_ssl(true).set_url(up_url).\
		add_header(std::string{ "Authorization: Bearer " } +access_token).add_header("Content-type: application/json");

	int http = curl.send(file_name_json.toStyledString(), res);
	if (http < 200 || http > 299)
		throw std::runtime_error{ "Rename unsuccessful" };

}
// -------------------------------------------------------------------------------------------
std::string Google_drive_uploader::create_directory(std::string & directory_name)
{
	std::string access_token = get_access_token();

	Json::Value folder_json;
	folder_json["name"] = directory_name;
	folder_json["mimeType"] = "application/vnd.google-apps.folder";

	std::vector<std::string> http_fields =
	{
		"Authorization: Bearer " + access_token,
		"Content-Type: application/json",
		"Content-Length: " + std::to_string(folder_json.toStyledString().size()),
	};

	Curl curl{};
	curl.set_url("https://www.googleapis.com/drive/v3/files").set_type(Curl::Request_type::post).use_ssl(true);
	for (const auto& i : http_fields)
		curl.add_header(i);

	std::vector<char> response;
	int http_code = curl.send(folder_json.toStyledString(), response);
	if (http_code < 200 || http_code > 299)
		throw std::runtime_error{ "Directory create unsuccessfull" };

	std::string folder_stats{ response.data() , response.size() };
	Json::Value folder_id_json;
	Json::Reader reader;
	reader.parse(folder_stats.c_str(), folder_id_json);
	if (folder_id_json["id"].asString() == "")
	{
		throw std::runtime_error{ "Unable to read folder id" };
	}
	return folder_id_json["id"].asString();
}
// -------------------------------------------------------------------------------------------
std::string Google_drive_uploader::file_type_to_string(Cloud_uploader::File_type type)
{
	switch (type)
	{
	case Cloud_uploader::File_type::text:
		return std::string{ "text/plain" };
	default:
		throw std::runtime_error{ "Undefined file type" };
	}
	return std::string();
}
// -------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------