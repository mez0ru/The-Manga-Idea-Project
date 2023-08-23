#pragma once

#include "Auth/KeyGeneration.h"

struct settings_t
{
	settings_t() = default;
	settings_t(bool readSettingsFile) {
		std::ofstream settingsFile;
		settingsFile.open("settings.json", std::ios::in);
		std::stringstream buffer;
		buffer << settingsFile.rdbuf();
		settingsFile.close();
		*this = json_dto::from_json<settings_t>(buffer.str());
		if (!m_access_token_secret.has_value() || !m_refresh_token_secret.has_value())
		{
			m_access_token_secret = Random64Hex();
			m_refresh_token_secret = Random64Hex();

			fmt::print("Generated a new Access Token Secret: {}\n", m_access_token_secret.value());
			fmt::print("Generated a new Refresh Token Secret: {}\n", m_refresh_token_secret.value());

			settingsFile.open("settings.json", std::ios::out | std::ios::trunc);
			settingsFile << json_dto::to_json(*this,
				json_dto::pretty_writer_params_t{}
				.indent_char(' ')
				.indent_char_count(4u)
				.format_options(rapidjson::kFormatSingleLineArray));
			settingsFile.close();

			fmt::print("Token secrets are now saved in the file \"settings.json\"\n");

		}
	}

	/*settings_t(
		std::string regex_name,
		std::string regex_email,
		std::string regex_password,
		std::string jwt_issuer,
		std::string path_to_7z,
		bool multithreaded = false,
		int chapters_cache_size = 1
	)
		: m_regex_name{ std::move(regex_name) }
		, m_regex_email{ std::move(regex_email) }
		, m_regex_password{ std::move(regex_password) }
		, m_jwt_issuer{ std::move(jwt_issuer) }
		, m_path_to_7z{ std::move(path_to_7z) }
		, m_multithreaded{ multithreaded }
		, m_chapters_cache_size{ std::move(chapters_cache_size) }
	{}

	settings_t(
		std::string access_token_secret,
		std::string refresh_token_secret,
		std::string regex_name,
		std::string regex_email,
		std::string regex_password,
		std::string jwt_issuer,
		std::string path_to_7z,
		bool multithreaded = false,
		int chapters_cache_size = 1
		)
		: m_access_token_secret{ std::move(access_token_secret) }
		, m_refresh_token_secret{ std::move(refresh_token_secret) }
		, m_regex_name{ std::move(regex_name) }
		, m_regex_email{ std::move(regex_email) }
		, m_regex_password{ std::move(regex_password) }
		, m_jwt_issuer{ std::move(jwt_issuer) }
		, m_path_to_7z{ std::move(path_to_7z) }
		, m_multithreaded{ multithreaded }
		, m_chapters_cache_size{ std::move(chapters_cache_size) }
	{}*/


	template < typename JSON_IO >
	void
		json_io(JSON_IO& io)
	{
		io
			& json_dto::optional("ACCESS_TOKEN_SECRET", m_access_token_secret, std::nullopt)
			& json_dto::optional("REFRESH_TOKEN_SECRET", m_refresh_token_secret, std::nullopt)
			& json_dto::mandatory("regex_name", m_regex_name)
			& json_dto::mandatory("regex_email", m_regex_email)
			& json_dto::mandatory("regex_password", m_regex_password)
			& json_dto::mandatory("JWT_ISSUER", m_jwt_issuer)
			& json_dto::optional("multithreaded", m_multithreaded, false)
			& json_dto::optional("chapters_cache_size", m_chapters_cache_size, 1)
			& json_dto::optional("allowed_origins", m_allowed_origins, std::vector<std::string>{})
			& json_dto::optional("port", m_port, 4000);
	}

	std::optional < std::string > m_access_token_secret;
	std::optional < std::string > m_refresh_token_secret;
	std::string m_regex_name;
	std::string m_regex_email;
	std::string m_regex_password;
	std::string m_jwt_issuer;
	std::vector<std::string> m_allowed_origins;
	uint16_t m_port;
	bool m_multithreaded;
	int m_chapters_cache_size;
};