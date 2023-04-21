#pragma once

#include <restinio/all.hpp>

#include <restinio/sync_chain/fixed_size.hpp>

#include <restinio/helpers/http_field_parsers/basic_auth.hpp>
#include <restinio/helpers/http_field_parsers/try_parse_field.hpp>
#include <restinio/router/easy_parser_router.hpp>

#include <json_dto/pub.hpp>

#include "jwt-cpp/jwt.h"
#include "jwt-cpp/traits/boost-json/traits.h"

#include <sqlite_modern_cpp.h>
#include <fmt/core.h>

#include "KeyGeneration.h"
#include "../User/User.h"

class auth_handler_t
{
public:
	explicit auth_handler_t(std::shared_ptr<sqlite::database> db, const std::vector<std::string>& allowedOrigins, const std::string& refresh_token, const std::string& access_token, const std::string& issuer);
	auth_handler_t(const auth_handler_t&) = delete;
	auth_handler_t(auth_handler_t&&) = delete;
	auto verify_identity(
		const restinio::request_handle_t& req,
		restinio::router::route_params_t) const;
	auto refresh_identity(
		const restinio::request_handle_t& req,
		restinio::router::route_params_t) const;
	auto login(
		const restinio::request_handle_t& req,
		restinio::router::route_params_t) const;
	auto on_cors(
		const restinio::request_handle_t& req,
		restinio::router::route_params_t) const;
private:
	std::weak_ptr<sqlite::database> m_db;
	const std::vector<std::string>& m_allowedOrigins;
	const std::string& m_refresh_token;
	const std::string& m_access_token;
	const std::string& m_issuer;
	//const std::vector<std::string>& m_allowedOrigins;

	template < typename RESP >
	static RESP
		init_resp(RESP resp/*, const std::vector<std::string>& allowedOrigins, const std::string& origin*/);
};

std::function<const restinio::request_handling_status_t& (const restinio::request_handle_t& req)> auth_handler(std::shared_ptr<sqlite::database> db, const std::vector<std::string>& allowedOrigins, const std::string& refresh_token, const std::string& access_token, const std::string& issuer);