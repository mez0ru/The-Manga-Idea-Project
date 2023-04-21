#pragma once

#include <restinio/router/express.hpp>
#include <restinio/cast_to.hpp>
#include <json_dto/pub.hpp>
#include "../Auth/KeyGeneration.h"
#include <sqlite_modern_cpp.h>
#include <algorithm>
#include <regex>
#include "../Auth/UserRoles.h"

struct user_t
{
	user_t() = default;

	user_t(
		uint64_t id,
		std::optional < std::string > name,
		std::string email = NULL,
		int role = 0,
		std::string created_at = NULL)
		: m_id{ id }
		, m_name{ std::move(name) }
		, m_email{ std::move(email) }
		, m_role{ role }
		, m_created_at{ std::move(created_at) }
	{}

	/*user_t(
		uint64_t id,
		std::string name = NULL,
		std::string email = NULL,
		int role = 0,
		std::string created_at = NULL,
		std::string password = NULL,
		std::string updated_at = NULL,
		std::string refresh_token = NULL)
		: m_id{ id }
		, m_name{ std::move(name) }
		, m_email{ std::move(email) }
		, m_password{ std::move(password) }
		, m_role{ role }
		, m_refresh_token{ std::move(refresh_token) }
		, m_created_at{ std::move(created_at) }
		, m_updated_at{ std::move(updated_at) }
	{}*/

	template < typename JSON_IO >
	void
		json_io(JSON_IO& io)
	{
		io
			& json_dto::optional("id", m_id, std::nullopt)
			& json_dto::optional("name", m_name, std::nullopt)
			& json_dto::mandatory("email", m_email)
			& json_dto::optional("password", m_password, std::nullopt)
			& json_dto::optional("role", m_role, std::nullopt)
			& json_dto::optional("refresh_token", m_refresh_token, std::nullopt)
			& json_dto::optional("created_at", m_created_at, std::nullopt)
			& json_dto::optional("updated_at", m_updated_at, std::nullopt)
			& json_dto::optional("message", m_message, std::nullopt);
	}

	std::optional < uint64_t > m_id;
	std::optional < std::string > m_name;
	std::string m_email;
	std::optional < std::string > m_password;
	std::optional < int > m_role;
	std::optional < std::string > m_refresh_token;
	std::optional < std::string > m_created_at;
	std::optional < std::string > m_updated_at;
	std::optional<std::string> m_message;
};

class users_handler_t
{
public:
	explicit users_handler_t(std::shared_ptr<sqlite::database> db, const std::vector<std::string>& allowedOrigins, const std::regex& nameReg, const std::regex& emailReg, const std::regex& passwordReg, const std::string& refresh_token);
	users_handler_t(const users_handler_t&) = delete;
	users_handler_t(users_handler_t&&) = delete;
	auto on_users_list(
		const restinio::request_handle_t& req,
		restinio::router::route_params_t) const;
	auto on_user_get(
		const restinio::request_handle_t& req,
		restinio::router::route_params_t params) const;
	auto on_new_user(
		const restinio::request_handle_t& req,
		restinio::router::route_params_t);
private:
	std::weak_ptr<sqlite::database> m_db;
	const std::vector<std::string>& m_allowedOrigins;
	const std::regex& m_nameReg;
	const std::regex& m_emailReg;
	const std::regex& m_passwordReg;
	const std::string& m_refresh_token;
	
	template < typename RESP >
	static RESP
		init_resp(RESP resp, const std::vector<std::string>& allowedOrigins, const std::string& origin);
	template < typename RESP >
	static void
		mark_as_bad_request(RESP& resp);
};

void user_handler(restinio::router::express_router_t<>& router, std::shared_ptr<sqlite::database> db, const std::vector<std::string>& allowedOrigins, const std::regex& nameReg, const std::regex& emailReg, const std::regex& passwordReg, const std::string& refresh_token);