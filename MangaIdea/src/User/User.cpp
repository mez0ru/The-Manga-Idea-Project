#include "User.h"

users_handler_t::users_handler_t(std::shared_ptr<sqlite::database> db, const std::vector<std::string>& allowedOrigins, const std::regex& nameReg, const std::regex& emailReg, const std::regex& passwordReg, const std::string& refresh_token)
		: m_db{ db }
		, m_allowedOrigins { allowedOrigins }
		, m_nameReg { nameReg}
		, m_emailReg { emailReg}
		, m_passwordReg { passwordReg}
		, m_refresh_token { refresh_token }
	{}

	auto users_handler_t::on_users_list(
		const restinio::request_handle_t& req,
		restinio::router::route_params_t) const
	{
		auto resp = init_resp(req->create_response(), m_allowedOrigins, req->header().try_get_field("origin") ? *req->header().try_get_field("origin") : "");
		req->header().try_get_field("cookie");
		resp.append_header("Authorization", "");

		uint64_t size;
		*m_db.lock() << "select count(id) from user;"
			>> size;
		std::vector<user_t> users;
		users.reserve(size);
		*m_db.lock() << "select id, name, email, role, created_at from user;"
			>> [&users](uint64_t id, std::string name, std::string email, int role, std::string created_at) {
			user_t user{ id, name.size() ? std::optional<std::string>(name) : std::nullopt, email, role, created_at};
			users.emplace_back(user);
		};

		resp.set_body(json_dto::to_json< std::vector<user_t>>(users));

		return resp.done();
	}

	auto users_handler_t::on_user_get(
		const restinio::request_handle_t& req,
		restinio::router::route_params_t params) const
	{
		const auto usernum = restinio::cast_to< std::uint32_t >(params["id"]);
		auto resp = init_resp(req->create_response(), m_allowedOrigins, req->header().try_get_field("origin") ? *req->header().try_get_field("origin") : "");
	

		*m_db.lock() << "select id, name, email, role, created_at from user where id = ? limit 1;"
			<< usernum
			>> [&resp](uint64_t id, std::string name, std::string email, int role, std::string created_at) {
			user_t user{ id, name.size() ? std::optional<std::string>(name) : std::nullopt, email, role, created_at };
			resp.set_body(json_dto::to_json<user_t>(user));
		};

		return resp.done();
	}

	auto users_handler_t::on_new_user(
		const restinio::request_handle_t& req,
		restinio::router::route_params_t)
	{
		auto resp = init_resp(req->create_response(restinio::status_no_content()), m_allowedOrigins, req->header().try_get_field("origin") ? *req->header().try_get_field("origin") : "");

		try
		{
			auto json = json_dto::from_json< user_t >(req->body());

			std::smatch match;
			if (!std::regex_search(json.m_email, match, m_emailReg) || !std::regex_search(json.m_password.value(), match, m_passwordReg)) {
				mark_as_bad_request(resp);
			}
			else {
				bool exists = false;
				*m_db.lock() << "select exists(select 1 from user where email = ? limit 1);"
					<< json.m_email
					>> exists;
				if (exists)
					resp.header().status_line(restinio::status_conflict());
				else
				{
					*m_db.lock() << "insert into user (email, password, role) values (?, ?, ?);"
						<< json.m_email
						<< CalcHmacSHA512(m_refresh_token, json.m_password.value())
						<< ROLES::ADMIN;
				}
			}
		}
		catch (const std::exception& /*ex*/)
		{
			mark_as_bad_request(resp);
		}

		return resp.done();
	}

	

	template < typename RESP >
	static RESP
		users_handler_t::init_resp(RESP resp, const std::vector<std::string>& allowedOrigins, const std::string& origin)
	{
		resp
			.append_header("Server", "RESTinio server /v.0.6.17")
			.append_header_date_field()
			.append_header("Content-Type", "application/json")

			// CORS
			.append_header("Access-Control-Allow-Origin", std::find(std::begin(allowedOrigins), std::end(allowedOrigins), origin) != std::end(allowedOrigins) ? origin.data() : allowedOrigins.at(0))
			.append_header("Access-Control-Allow-Credentials", "true");

		return resp;
	}

	template < typename RESP >
	static void
		users_handler_t::mark_as_bad_request(RESP& resp)
	{
		resp.header().status_line(restinio::status_bad_request());
	}

void user_handler(restinio::router::express_router_t<>& router, std::shared_ptr<sqlite::database> db, const std::vector<std::string>& allowedOrigins, const std::regex& nameReg, const std::regex& emailReg, const std::regex& passwordReg, const std::string& refresh_token)
{
	auto handler = std::make_shared< users_handler_t >(std::ref(db), std::ref(allowedOrigins), std::ref(nameReg), std::ref(emailReg), std::ref(passwordReg), std::ref(refresh_token));

	auto by = [&](auto method) {
		using namespace std::placeholders;
		return std::bind(method, handler, _1, _2);
	};

	auto method_not_allowed = [](const auto& req, const auto&) {
		return req->create_response(restinio::status_method_not_allowed())
			.connection_close()
			.done();
	};

	// Get all users (For admins only).
	router.http_get("/api/v2/user",
		by(&users_handler_t::on_users_list));
	

	// create new user
	router.http_post("/api/v2/user",
		by(&users_handler_t::on_new_user));

	// Disable all other methods for '/'.
	router.add_handler(
		restinio::router::none_of_methods(
			restinio::http_method_get(), restinio::http_method_post()), "/api/v2/user",
		method_not_allowed);

	// Handlers for '/:booknum' path.
	router.http_get("/api/v2/user/:id",
		by(&users_handler_t::on_user_get));

	router.add_handler(
		restinio::router::none_of_methods(
			restinio::http_method_get()),"/api/v2/user/:id",
		method_not_allowed);
}