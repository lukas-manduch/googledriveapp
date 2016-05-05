#include <curl/curl.h>
#include <iostream>
#include <stdexcept>

#include "curl.h"

Curl::Curl()
{
	m_curl = curl_easy_init();
    if (m_curl == nullptr)
    {
        throw std::runtime_error("Unable to initialize curl.");
    }
}

Curl::~Curl()
{
	curl_easy_cleanup(m_curl);
	if (m_list != nullptr)
		curl_slist_free_all(m_list);
}

Curl & Curl::set_type(Curl::Request_type type)
{
	curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, get_method(type).c_str());
	return *this;
}

Curl & Curl::set_url( _In_ const std::string & url)
{
	curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
	return *this;
}

Curl & Curl::use_ssl( _In_ bool ssl)
{
	curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, ssl);
	return *this;
}

Curl & Curl::add_header( _In_ const std::string & header)
{
	m_list = curl_slist_append(m_list, header.c_str());
	return *this;
}

long Curl::send(_In_ const std::string& message, _Out_ std::vector<char>& response)
{
	if (m_used)
		throw std::runtime_error{"Curl cannot be reused"};
	m_used = true;

	if (m_list != nullptr)
		curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_list);
	
	curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, message.c_str());
	curl_easy_setopt(m_curl, CURLOPT_POSTFIELDSIZE, message.size());
	curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, Curl::WriteVectorCallback);
	curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, (void *)&response);
    
	if (curl_easy_perform(m_curl) != 0)
		throw std::exception{ "Error while performing request" };
	long http_code;
	curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &http_code);
	return http_code;
}

size_t Curl::WriteVectorCallback(_In_ void * ptr, _In_ size_t size, _In_ size_t count, _Inout_ std::vector<char>* mem)
{
	try
	{
		mem->insert(mem->end(), static_cast<char*>(ptr), static_cast<char*>(ptr) + (size * count));
	}
	catch (const std::exception&)
	{
		return 0; // Generate error
	}
	
	return size*count;
}

const std::string Curl::get_method(Request_type type) const
{
	switch (type)
	{
		case Request_type::post:
			return std::string { "POST" };
			break;
		case Request_type::put:
			return std::string { "PUT" };
			break;
		case Request_type::get:
			return std::string { "GET" };
			break;
	}

    throw std::runtime_error("Invalid request type.");
}
