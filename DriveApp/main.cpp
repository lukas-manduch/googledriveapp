#include <fstream>
#include <iostream>
#include <vector>
#include <regex>
#include <filesystem>
#include <codecvt>

#include <curl/curl.h>
#include "json/json.h"
#include "tcp_communicator/tcp_communicator.h"
#include "curl_cpp/curl.h"
#include "cloud_uploader/google_drive_uploader.h"

namespace fs = std::experimental::filesystem;

// input is json file from Google API
// https://developers.google.com/identity/protocols/OAuth2InstalledApp
// https://developers.google.com/drive/v3/web/about-auth

//	---------------------------------------------------------------------------------------------
namespace
{
	int local_port_number;
}
//	---------------------------------------------------------------------------------------------
std::string GetAuthorizationCode( _In_ const std::string& auth_url, _In_ const std::string& client_id)
{

	std::string url = auth_url;
	url += std::string("?scope=") + "https://www.googleapis.com/auth/drive"; // scope (our permissions)
	url += "&response_type=code";
	url += std::string("&client_id=") + client_id;

	Tcp_communicator tcp{};

	url += "&redirect_uri=http://localhost:"; // prompt user to enter code
	url += std::to_string(tcp.get_port_number());
	local_port_number = tcp.get_port_number();
	std::wstring wurl(url.begin(), url.end());

	ShellExecute(NULL, L"open", wurl.c_str(), NULL, NULL, SW_SHOWNORMAL);

	tcp.set_timeout(10);
	if (!tcp.accept_connection())
	{
		std::cerr << "Timeout" << std::endl;
		std::abort();
	}
	std::vector<byte> raw_data = tcp.get_data();

	static_assert(sizeof(byte) == sizeof(char), "Size of char != size of byte");

	std::string request(reinterpret_cast<char*>(raw_data.data()), raw_data.size());
	tcp.send_data("HTTP / 1.1 200 OK\r\nContent - Type: text / html\r\nContent - Length: 37\r\n\r\nThank you!You can close the browser.");
	tcp.close_connection();

	std::smatch matches;
	std::regex_search(request, matches, std::regex{ R"(code=\S*)" });

	return std::string{ matches[0] }.erase(0, 5);// erase 'code='
}
//	---------------------------------------------------------------------------------------------
Json::Value Authenticate(_In_ const std::string& authorization_code, _In_ const std::string& client_id, _In_ const std::string& client_secret)
// Use authorization code to get refresh token
{
	std::stringstream post;
	post << "code=" << authorization_code << "&"
		<< "client_id=" << client_id << "&"
		<< "client_secret=" << client_secret << "&"
		<< "redirect_uri=http://localhost:" << local_port_number << "&"
		<< "grant_type=authorization_code";

	std::string post_str = post.str();
	std::string token_uri = "https://accounts.google.com/o/oauth2/token";


	Curl curl{};
	curl.use_ssl(false).set_url(token_uri).set_type(Curl::Request_type::post);

	std::vector<char> response;

	int http_code = curl.send(post.str(), response);;
	std::cout << "Authenticate - " << http_code << std::endl;

	Json::Reader reader;

	Json::Value ret_val;
	reader.parse(&response[0], &response[0] + response.size(), ret_val);
	return ret_val;
}

//	---------------------------------------------------------------------------------------------
std::string get_refresh_token(_In_ const std::string& file_name, _In_ const std::string& auth_url, _In_ const std::string& client_id, _In_ const std::string& client_secret)
{
	try // Try to use refresh token from file_name
	{
		Json::Reader reader;
		Json::Value settings_json;
		reader.parse(std::ifstream(file_name), settings_json);
		if (settings_json["refresh_token"].isString())
		{
			std::string  refresh_token = settings_json["refresh_token"].asString();
			if ( Google_drive_uploader::is_refresh_token_valid(refresh_token, client_id, client_secret) )
				return refresh_token;
		}
		throw std::runtime_error{ "" }; // Go to catch
	}
	catch (const std::exception&) // Get new refresh token
	{
		std::string authorization_code = GetAuthorizationCode(auth_url, client_id);
		Json::Value auth_reply = Authenticate(authorization_code, client_id, client_secret);
		Json::Value settings;
		settings["refresh_token"] = auth_reply["refresh_token"].asString();
		Json::StyledStreamWriter().write(std::ofstream(file_name), settings);
		return settings["refresh_token"].asString();
	}
}
//	---------------------------------------------------------------------------------------------
//	---------------------------------------------------------------------------------------------
int wmain(int argc, wchar_t* argv[])
try
{
	if (argc != 2 && argc != 1)
		return 1;

    std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8cvt;
    
    std::string this_dir = utf8cvt.to_bytes(argv[0]);
	this_dir.erase(this_dir.rfind('\\') + 1, std::string::npos);

	Json::Reader reader;
	Json::Value client_secret_json;

	if (!reader.parse(std::ifstream(this_dir + "client_secret.json"), client_secret_json))
		throw std::runtime_error{ "invalid client_secret.json" };

	std::string client_id = client_secret_json["installed"]["client_id"].asString();
	std::string client_secret = client_secret_json["installed"]["client_secret"].asString();
	std::string auth_url = client_secret_json["installed"]["auth_uri"].asString();

	std::string refresh_token = get_refresh_token(this_dir + "settings.json", auth_url, client_id, client_secret);



	// ------------------------------------------------------------------

	Google_drive_uploader gdu{ refresh_token , client_id , client_secret };

	std::wstring file_name;
	
	if (argc == 1) // Get name from input
	{
		std::cout << "Name of file:" << std::endl;
		std::wcin >> file_name;
	}
	else // File was dropped on executable
		file_name = argv[1];
	
    switch (fs::status(file_name).type())
    {
        case fs::file_type::regular:
        {
			if (gdu.upload_file(file_name, Cloud_uploader::File_type::text))
			{
				std::cout << "Upload file successful" << std::endl;
			}
            break;
        }
        case fs::file_type::directory:
        {
			if ( gdu.upload_folder(file_name) )
				std::cout << "Upload folder successful" << std::endl;
            break;
        }
        default:
        {
            std::cout << "Invalid name or directory: " <<  std::endl;
            break;
        }
    }
	
	// Wait for input
	char c;
	std::cin >> c;

	return 0;
}
catch (const std::exception &e)
{
	std::cerr << e.what() << std::endl;
	std::cin.clear();
	char c;
	std::cin >> c;
	return 1;
}
