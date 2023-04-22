#include "Auth.h"

auth_handler_t::auth_handler_t(std::shared_ptr<sqlite::database> db, const std::vector<std::string>& allowedOrigins, const std::string& refresh_token, const std::string& access_token, const std::string& issuer)
	: m_db{ db }
	, m_allowedOrigins{allowedOrigins}
	, m_refresh_token{ refresh_token }
	, m_access_token {access_token}
	, m_issuer {issuer}
{}

auto auth_handler_t::verify_identity(
	const restinio::request_handle_t& req,
	restinio::router::route_params_t) const
{
		auto cookie = req->header().try_get_field("cookie");
		if (cookie) {
			try {
				int cookie_offset = cookie->find("session=") + 8;
				std::string cookie_session{ cookie->c_str() + cookie_offset, cookie->find(";", cookie_offset) - cookie_offset };

				auto jwt_verify = jwt::verify<jwt::traits::boost_json>().allow_algorithm(jwt::algorithm::hs256(m_access_token)).with_issuer(m_issuer);
				auto decoded = jwt::decode<jwt::traits::boost_json>(cookie_session);
				jwt_verify.verify(decoded);

				// User authentificated. The further processing can be
				// delegated to the next handler in the chain.
				return restinio::request_not_handled();
			}
			catch (std::exception& e) {}
		}
			// Try to parse Authorization header.
			using namespace restinio::http_field_parsers::basic_auth;

			const auto result = try_extract_params(*req,
				restinio::http_field::authorization);
			if (result)
			{
				bool valid = false;
				*m_db.lock() << "SELECT EXISTS(SELECT 1 FROM user WHERE email = ? and password = ? LIMIT 1);"
					<< result->username
					<< CalcHmacSHA512(m_refresh_token, result->password)
					>> valid;
				// Authorization header is present and contains some value.
				if (valid)
				{
					// User authentificated. The further processing can be
					// delegated to the next handler in the chain.
					return restinio::request_not_handled();
				}
			}

	// Otherwise we ask for credentials.
	init_resp(req->create_response(restinio::status_unauthorized()))
		.append_header(restinio::http_field::content_type, "text/plain; charset=utf-8")
		.append_header(restinio::http_field::www_authenticate,
			R"(Basic realm="auth", charset="utf-8")")
		.set_body("Unauthorized access forbidden")
		.done();

	// Mark the request as processed.
	return restinio::request_accepted();
}

auto auth_handler_t::refresh_identity(
	const restinio::request_handle_t& req,
	restinio::router::route_params_t) const
{
	auto cookie = req->header().try_get_field("cookie");
	if (cookie) {
		try {
			// find refresh token
			int cookie_offset = cookie->find("jwt=") + 4;
			std::string cookie_session{ cookie->c_str() + cookie_offset, cookie->find(";", cookie_offset) - cookie_offset };

			// verify the integrity of the token
			auto jwt_verify = jwt::verify<jwt::traits::boost_json>().allow_algorithm(jwt::algorithm::hs256(m_refresh_token)).with_issuer(m_issuer);
			auto decoded = jwt::decode<jwt::traits::boost_json>(cookie_session);
			jwt_verify.verify(decoded);

			// create a new access token
			auto reg_access_token = jwt::create<jwt::traits::boost_json>()
				.set_issuer(m_issuer)
				.set_type("JWT")
				.set_issued_at(std::chrono::system_clock::now())
				.set_expires_at(std::chrono::system_clock::now() + std::chrono::seconds{ 36000 })
				.set_payload_claim("email", decoded.get_payload_claim("email"))
				.sign(jwt::algorithm::hs256{ m_access_token });

			// place it and respond with no content
			return init_resp(req->create_response(restinio::status_no_content()))
				.append_header(restinio::http_field::set_cookie, fmt::format("session={}; Max-Age={}; HttpOnly", reg_access_token, 36000 * 1000))
				.done();
		}
		catch (std::exception&) {}
	}

	// Otherwise reply with unauthorized.
	return init_resp(req->create_response(restinio::status_unauthorized()))
		.set_body("Unauthorized access forbidden")
		.done();
}

auto auth_handler_t::login(
	const restinio::request_handle_t& req,
	restinio::router::route_params_t) const
{
	try {
		auto credentials = json_dto::from_json<user_t>(req->body());

		if (credentials.m_password.has_value()) {
			bool exists = false;
			auto xxx = CalcHmacSHA512(m_refresh_token, credentials.m_password.value());
			*m_db.lock() << "SELECT EXISTS(SELECT 1 FROM user WHERE email = ? and password = ? LIMIT 1);"
				<< credentials.m_email
				<< CalcHmacSHA512(m_refresh_token, credentials.m_password.value())
				>> exists;

			if (exists) {
				auto reg_refresh_token = jwt::create<jwt::traits::boost_json>()
                        .set_issuer(m_issuer)
                        .set_type("JWT")
                        .set_issued_at(std::chrono::system_clock::now())
                        .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours{ 168 })
                        .set_payload_claim("email", boost::json::value(credentials.m_email))
                        .sign(jwt::algorithm::hs256{ m_refresh_token });

				auto reg_access_token = jwt::create<jwt::traits::boost_json>()
					                        .set_issuer(m_issuer)
					                        .set_type("JWT")
					                        .set_issued_at(std::chrono::system_clock::now())
					                        .set_expires_at(std::chrono::system_clock::now() + std::chrono::seconds{ 36000 })
					                        .set_payload_claim("email", boost::json::value(credentials.m_email))
					                        .sign(jwt::algorithm::hs256{ m_access_token });

				return init_resp(req->create_response(restinio::status_no_content()))
					.append_header(restinio::http_field::set_cookie, fmt::format("jwt={}; Max-Age={}; HttpOnly", reg_refresh_token, 7 * 24 * 60 * 60 * 1000))
					.append_header(restinio::http_field::set_cookie, fmt::format("session={}; Max-Age={}; HttpOnly", reg_access_token, 36000 * 1000))
					.done();
			}
		}
	}
	catch (std::exception&) {

	}

	// Otherwise reply with unauthorized.
	return init_resp(req->create_response(restinio::status_unauthorized()))
		.set_body("Unauthorized access forbidden")
		.done();
}

auto auth_handler_t::on_cors(
	const restinio::request_handle_t& req,
	restinio::router::route_params_t) const
{
	std::string origin{ req->header().try_get_field("origin") ? *req->header().try_get_field("origin") : "" };
	return init_resp(req->create_response())
		// CORS
		.append_header("Access-Control-Allow-Origin", std::find(std::begin(m_allowedOrigins), std::end(m_allowedOrigins), origin) != std::end(m_allowedOrigins) ? origin.data() : m_allowedOrigins.at(0))
		.append_header("Access-Control-Allow-Credentials", "true")
		.append_header("Access-Control-Allow-Headers", "X-PINGOTHER, Content-Type, Origin, X-Requested-With, Content-Type, Accept, Authorization")
		.append_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS, PUT, DELETE")
		.append_header("Access-Control-Max-Age", "86400")
		.done();
}

template < typename RESP >
static RESP
auth_handler_t::init_resp(RESP resp)
{
	resp
		.append_header("Server", "RESTinio server /v.0.6.17")
		.append_header_date_field();

	return resp;
}

using express_router_t = restinio::router::express_router_t<>;

std::function<const restinio::request_handling_status_t& (const restinio::request_handle_t& req)> auth_handler(std::shared_ptr<sqlite::database> db, const std::vector<std::string>& allowedOrigins, const std::string& refresh_token, const std::string& access_token, const std::string& issuer)
{
	auto router = std::make_shared< express_router_t >();
	auto handler = std::make_shared< auth_handler_t >(std::ref(db), std::ref(allowedOrigins), std::ref(refresh_token), std::ref(access_token), std::ref(issuer));

	//auto user_num = restinio::router::easy_parser_router::non_negative_decimal_number_p< std::uint32_t >();

	//auto by = [&](auto method) {
	//	using namespace std::placeholders;
	//	return std::bind(method, handler, _1, _2);
	//};
	auto by = [&](auto method) {
		using namespace std::placeholders;
		return std::bind(method, handler, _1, _2);
	};

	// CORS
	// User
	router->add_handler(restinio::http_method_options(),
		"/api/v2/user",
		by(&auth_handler_t::on_cors));
	router->add_handler(restinio::http_method_options(),
		"/api/v2/user/:id",
		by(&auth_handler_t::on_cors));

	// Chapter
	router->add_handler(restinio::http_method_options(),
		"/api/v2/chapter",
		by(&auth_handler_t::on_cors));
	router->add_handler(restinio::http_method_options(),
		"/api/v2/chapter/:id",
		by(&auth_handler_t::on_cors));
	router->add_handler(restinio::http_method_options(),
		"/api/v2/chapter/:chapter_id/page/:page_index",
		by(&auth_handler_t::on_cors));
	router->add_handler(restinio::http_method_options(),
		"/api/v2/chapter/cover/:chapter_id",
		by(&auth_handler_t::on_cors));
	router->add_handler(restinio::http_method_options(),
		"/api/v2/chapter/progress/:chapter_id",
		by(&auth_handler_t::on_cors));

	// Series
	router->add_handler(restinio::http_method_options(),
		"/api/v2/series",
		by(&auth_handler_t::on_cors));
	router->add_handler(restinio::http_method_options(),
		"/api/v2/series/:id",
		by(&auth_handler_t::on_cors));
	router->add_handler(restinio::http_method_options(),
		"/api/v2/series/cover/:id",
		by(&auth_handler_t::on_cors));

	// Auth
	router->add_handler(restinio::http_method_options(),
		"/api/v2/auth/refresh",
		by(&auth_handler_t::on_cors));
	router->add_handler(restinio::http_method_options(),
		"/api/v2/auth/login",
		by(&auth_handler_t::on_cors));

	// Routes >>>
	router->http_get("/api/v2/user",
		by(&auth_handler_t::verify_identity));
	router->http_get("/api/v2/user/:id",
		by(&auth_handler_t::verify_identity));
	router->http_get("/api/v2/auth/refresh",
		by(&auth_handler_t::refresh_identity));
	router->http_post("/api/v2/auth/login",
		by(&auth_handler_t::login));

	return [handler = std::move(router)](const auto& req) {
		return (*handler)(req);
	};
}