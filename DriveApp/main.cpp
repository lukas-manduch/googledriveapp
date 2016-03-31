#include <fstream>
#include <iostream>
#include <vector>
#include <regex>

#include <curl/curl.h>
#include "json/json.h"
#include "tcp_communicator\tcp_communicator.h"
#include "curl_cpp/curl.h"

// input is json file from Google API
// https://developers.google.com/identity/protocols/OAuth2InstalledApp
// https://developers.google.com/drive/v3/web/about-auth

std::string GetAuthorizationCode(const std::string& auth_url, const std::string& client_id)
{
    
    std::string url = auth_url;    
    url += std::string("?scope=") + "https://www.googleapis.com/auth/drive"; // scope (our permissions)
    url += "&response_type=code";
    url += std::string("&client_id=") + client_id;
    // url += "&redirect_uri=urn:ietf:wg:oauth:2.0:oob"; // prompt user to enter code
    url += "&redirect_uri=http://localhost:3537"; // prompt user to enter code

    std::wstring wurl(url.begin(), url.end());

    ShellExecute(NULL, L"open", wurl.c_str(), NULL, NULL, SW_SHOWNORMAL);


    ADDRINFOA *result = nullptr;
    ADDRINFOA *ptr = nullptr;

    ADDRINFOA hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    int res = getaddrinfo("127.0.0.1", "3537", &hints, &result);
    if (res != 0)
    {
        throw std::runtime_error((std::string("getaddrinfo failed with error: ") + std::to_string(res)).c_str());
    }

    SOCKET sock = NULL;
    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != nullptr; ptr = ptr->ai_next)
    {
        // Create a SOCKET for connecting to server
        sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    }

    // Connect to server.
    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = inet_addr("127.0.0.1");
    service.sin_port = htons(3537);

    if (bind(sock,
        (SOCKADDR *)& service, sizeof(service)) == SOCKET_ERROR) {
        wprintf(L"bind failed with error: %ld\n", WSAGetLastError());
    }

    if (listen(sock, 1) == SOCKET_ERROR) {
        wprintf(L"listen failed with error: %ld\n", WSAGetLastError());
    }

    SOCKET AcceptSocket = accept(sock, NULL, NULL);

    char recvbuf[2048];
    int iResult = recv(AcceptSocket, recvbuf, sizeof(recvbuf), 0);

    std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 37\r\n\r\nThank you! You can close the browser.";
    send(AcceptSocket, response.c_str(), response.size(), 0);

    closesocket(sock);

    freeaddrinfo(result);

    std::string request(recvbuf, iResult);
    request.erase(request.find('\r'), std::string::npos);
    // GET /?code=4/Kmh-xmTb-QCFrWmYcUKGHY54wBBgM4crsibZ_StR6a0 HTTP/1.1
    request.erase(0, 11);
    request.erase(request.size() - 9, std::string::npos);

    return request;
}

// fwrite signature
size_t WriteVectorCallback(void *ptr, size_t size, size_t count, std::vector<char> *mem)
{
    mem->insert(mem->end(), static_cast<char*>(ptr), static_cast<char*>(ptr) + (size * count));
    return count;
}

Json::Value Authenticate(const std::string& authorization_code, const std::string& client_id, const std::string& client_secret)
{
    std::stringstream post;
    post << "code=" << authorization_code << "&"
        << "client_id=" << client_id << "&"
        << "client_secret=" << client_secret << "&"
        << "redirect_uri=http://localhost:3537&"
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

Json::Value RefreshToken(const std::string& refresh_token, const std::string& client_id, const std::string& client_secret)
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
    int http_code = curl.send(post.str() , response);
	std::cout << "Refresh token - " << http_code << std::endl;
  
	Json::Reader reader;

    Json::Value ret_val;
    reader.parse(&response[0], &response[0] + response.size(), ret_val);
    return ret_val;
}

int main(int argc, char* argv[])
{
	
    if (argc != 2 && argc != 1 )
        return 1;

    std::string this_dir(argv[0]);
    this_dir.erase(this_dir.rfind('\\') + 1, std::string::npos);

    // Initialize Winsock
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) 
    {
        std::cout << "WSAStartup failed: " << result << std::endl;
        return 1;
    }

    Json::Reader reader;

    Json::Value client_secret_json;
    reader.parse(std::ifstream( "client_secret.json"), client_secret_json);

    std::string client_id = client_secret_json["installed"]["client_id"].asString();
    std::string client_secret = client_secret_json["installed"]["client_secret"].asString();
    std::string auth_url = client_secret_json["installed"]["auth_uri"].asString();
	std::cout << auth_url << std::endl;
    // read our settings
    Json::Value settings_json;
    reader.parse(std::ifstream(this_dir + "settings.json"), settings_json);
    
    std::string access_token;
    std::string token_type;

    if (!settings_json["refresh_token"].isString())
    {
        // we do not have refresh token, do the handshake
        std::string authorization_code = GetAuthorizationCode(auth_url, client_id);

        Json::Value auth_reply = Authenticate(authorization_code, client_id, client_secret);

        Json::Value settings;
        settings["refresh_token"] = auth_reply["refresh_token"].asString();
        Json::StyledStreamWriter().write(std::ofstream(this_dir + "settings.json"), settings);

        access_token = auth_reply["access_token"].asString();
        token_type = auth_reply["token_type"].asString();
    }
    else
    {
        // refresh access token
        // https://developers.google.com/identity/protocols/OAuth2InstalledApp#refresh
        Json::Value refresh_token_reply = RefreshToken(settings_json["refresh_token"].asString(), client_id, client_secret);

        access_token = refresh_token_reply["access_token"].asString();
        token_type = refresh_token_reply["token_type"].asString();
    }

	std::ifstream upload_file;
	if (argc == 1) // Get name from input
	{
		std::cout << "Name of file:" << std::endl;
		std::string tmp_file_name;
		getline(std::cin, tmp_file_name);
		upload_file.open(tmp_file_name);
	}
	else // File was dropped on executable
		upload_file.open(argv[1]);

	if (!upload_file.is_open())
	{
		std::cerr << "Bad file name" << std::endl;
		std::abort();
	}

    std::string upload_file_content((std::istreambuf_iterator<char>(upload_file)),
        std::istreambuf_iterator<char>());

    std::vector<std::string> http_fields = 
    {
        "Authorization: " + token_type + " " + access_token, 
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
	std::cout << http_code << std::endl;
	
	// Wait for input
	char c;
	std::cin >> c;
    
    return 0;
}