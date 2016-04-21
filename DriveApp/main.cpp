#include <fstream>
#include <iostream>
#include <vector>
#include <regex>

#include <curl/curl.h>
#include "json/json.h"
#include "tcp_communicator/tcp_communicator.h"
#include "curl_cpp/curl.h"

// input is json file from Google API
// https://developers.google.com/identity/protocols/OAuth2InstalledApp
// https://developers.google.com/drive/v3/web/about-auth


int local_port_number;
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

// fwrite signature
size_t WriteVectorCallback(void *ptr, size_t size, size_t count, std::vector<char> *mem)
{
	mem->insert(mem->end(), static_cast<char*>(ptr), static_cast<char*>(ptr) + (size * count));
	return count;
}

Json::Value Authenticate(_In_ const std::string& authorization_code, _In_ const std::string& client_id, _In_ const std::string& client_secret)
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

Json::Value RefreshToken(_In_ const std::string& refresh_token, _In_ const std::string& client_id, _In_ const std::string& client_secret)
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
	int http_code = curl.send(post.str(), response);
	if (http_code < 200 || http_code > 299)
		throw std::runtime_error{ "Unable to authenticate" };

	Json::Reader reader;

	Json::Value ret_val;
	reader.parse(&response[0], &response[0] + response.size(), ret_val);
	return ret_val;
}
std::string get_access_token(_In_ const std::string refresh_token, _In_ const std::string client_id, _In_ const std::string client_secret)
{
	Json::Value refresh_token_reply = RefreshToken(refresh_token, client_id, client_secret);
	std::string access_token = refresh_token_reply["access_token"].asString();
	if (access_token == "")
		throw std::runtime_error{ "Unable to get access_token" };
	return access_token;
}
std::string get_refresh_token(_In_ const std::string& file_name, _In_ const std::string& auth_url, _In_ const std::string& client_id, _In_ const std::string& client_secret)
{
	Json::Reader reader;
	Json::Value settings_json;
	try
	{
		reader.parse(std::ifstream(file_name), settings_json);
		if (settings_json["refresh_token"].isString())
		{
			std::string  refresh_token = settings_json["refresh_token"].asString();
			std::string access_token = get_access_token(refresh_token, client_id, client_secret);
			return refresh_token;
		}
		throw std::runtime_error{ "" }; // Go to catch
	}
	catch (const std::exception&)
	{
		std::string authorization_code = GetAuthorizationCode(auth_url, client_id);
		Json::Value auth_reply = Authenticate(authorization_code, client_id, client_secret);
		Json::Value settings;
		settings["refresh_token"] = auth_reply["refresh_token"].asString();
		Json::StyledStreamWriter().write(std::ofstream(file_name), settings);
		return settings["refresh_token"].asString();
	}


}
bool upload_file(_In_ const std::string& file_name, _In_ const std::string& refresh_token, _In_ const std::string& client_id, _In_ const std::string& client_secret)
{
	std::string file_id;
	{
		std::ifstream upload_file{ file_name };
		if (!upload_file.is_open())
			throw std::runtime_error{ "Bad file name" };
		std::string upload_file_content((std::istreambuf_iterator<char>(upload_file)),
			std::istreambuf_iterator<char>());

		std::string access_token = get_access_token(refresh_token, client_id, client_secret);

		std::vector<std::string> http_fields =
		{
			"Authorization: Bearer " + access_token,
			"Content-Type: text/plain",
			"Content-Length: " + std::to_string(upload_file_content.size()),
		};

		std::string url = "https://www.googleapis.com/upload/drive/v3/files?uploadType=media";

		Curl curl{};

		for (const auto& i : http_fields)
			curl.add_header(i);
		curl.use_ssl(false).set_type(Curl::Request_type::post).set_url(url);

		std::vector<char> response;

		int http_code = curl.send(upload_file_content, response);
		if (http_code < 200 || http_code > 299)
			throw std::runtime_error{ "Upload unsuccessfull" };

		std::string file_stats{ response.data() , response.size() };
		Json::Value file_id_json;
		Json::Reader reader;
		reader.parse(file_stats.c_str(), file_id_json);
		if (file_id_json["id"].asString() == "")
		{
			std::cerr << "Something went wrong" << std::endl;
			throw std::runtime_error{ "Unable to read file id" };
		}
		file_id = file_id_json["id"].asString();
	}

	{ // Rename file block

		std::string up_url = "https://www.googleapis.com/drive/v2/files/";
		up_url += file_id + "?key=" + client_id;

		Json::Value file_name;
		file_name["title"] = file_name;

		std::string access_token = get_access_token(refresh_token, client_id, client_secret);

		std::vector<char> res;
		Curl curl2;
		curl2.set_type(Curl::Request_type::put).use_ssl(true).set_url(up_url).\
			add_header(std::string{ "Authorization: Bearer " } +access_token).add_header("Content-type: application/json");

		int http = curl2.send(file_name.toStyledString(), res);
		if (http < 200 || http > 299)
			throw std::runtime_error{ "Rename unsuccessful" };
		return true;

	}
}

int main(int argc, char* argv[])
try
{
	if (argc != 2 && argc != 1)
		return 1;

	std::string this_dir(argv[0]);
	this_dir.erase(this_dir.rfind('\\') + 1, std::string::npos);


	Json::Reader reader;
	Json::Value client_secret_json;

	if (!reader.parse(std::ifstream("client_secret.json"), client_secret_json))
		throw std::runtime_error{ "invalid client_secret.json" };

	std::string client_id = client_secret_json["installed"]["client_id"].asString();
	std::string client_secret = client_secret_json["installed"]["client_secret"].asString();
	std::string auth_url = client_secret_json["installed"]["auth_uri"].asString();

	std::string refresh_token = get_refresh_token(this_dir + "settings.json", auth_url, client_id, client_secret);

	std::string access_token = get_access_token(refresh_token, client_id, client_secret);

	std::string file_name;
	if (argc == 1) // Get name from input
	{
		std::cout << "Name of file:" << std::endl;
		getline(std::cin, file_name);
	}
	else // File was dropped on executable
		file_name = argv[1];
	if (upload_file(file_name, refresh_token, client_id, client_secret))
		std::cout << "Upload successfull" << std::endl;

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