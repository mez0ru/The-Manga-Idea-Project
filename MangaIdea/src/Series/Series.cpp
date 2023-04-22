#include "Series.h"

seriess_handler_t::seriess_handler_t(std::shared_ptr<sqlite::database> db, const std::vector<std::string>& allowedOrigins)
	: m_db{ db }
	, m_allowedOrigins{ allowedOrigins }
{}

auto seriess_handler_t::on_series_get(
	const restinio::request_handle_t& req,
	restinio::router::route_params_t params)
{
	auto resp = init_resp(req->create_response(), m_allowedOrigins, req->header().try_get_field("origin") ? *req->header().try_get_field("origin") : "", "application/json");
	const auto seriesnum = restinio::cast_to< std::uint32_t >(params["series_id"]);

	try {
		std::vector<chapter_t> chapters{};
		std::string name;
		*m_db.lock() << "select name, count(chapter.id) from series inner join chapter on chapter.series_id = series.id where series_id = ?;"
			<< seriesnum
			>> [&name, &chapters](std::string n, uint32_t count) mutable {
			name = n;
			chapters.reserve(count);
		};
		

		*m_db.lock() <<  "select chapter.id, chapter.file_name, count(page.chapter_id) as pages from chapter inner join series on series.id = chapter.series_id inner join page on page.chapter_id = chapter.id where series_id = ? group by chapter.id;"
			                    << seriesnum
			            >> [&chapters](uint64_t id, std::string file_name, uint32_t pages) {
			chapters.emplace_back(id, file_name, pages);
			                };
		resp.set_body(json_dto::to_json<series_t>({seriesnum, name, chapters}));
	}
	catch (std::exception&) {
		mark_as_bad_request(resp);
	}

	return resp.done();
}

auto seriess_handler_t::on_series_delete(
	const restinio::request_handle_t& req,
	restinio::router::route_params_t params)
{
	auto resp = init_resp(req->create_response(restinio::status_no_content()), m_allowedOrigins, req->header().try_get_field("origin") ? *req->header().try_get_field("origin") : "", "application/json");
	const auto seriesnum = restinio::cast_to< std::uint32_t >(params["series_id"]);

	try {
		std::vector<chapter_t> chapters{};
		std::string name;
		*m_db.lock() << "delete from series where id = ?;"
			 << seriesnum;
	}
	catch (std::exception&) {
		mark_as_bad_request(resp);
	}

	return resp.done();
}

auto seriess_handler_t::on_series_update(
	const restinio::request_handle_t& req,
	restinio::router::route_params_t)
{
	auto resp = init_resp(req->create_response(restinio::status_no_content()), m_allowedOrigins, req->header().try_get_field("origin") ? *req->header().try_get_field("origin") : "", "application/json");


	try {
		auto json = json_dto::from_json< series_t >(req->body());
		if (json.m_id.has_value()) {
			if (json.m_name.has_value())
			{
				// Change series name, folder, etc...
			}
			else {
				// Update chapters only

				*m_db.lock() << "select folder_path from series where id = ? limit 1;"
				    << json.m_id.value()
					>> [&](std::string path) {
					std::vector<chapter_t> chapters{};
					AddNewChapters(*m_db.lock(), chapters, path, json.m_id.value());
				};
				//resp.set_body(json_dto::to_json<series_t>({ json.m_id.value(), json.m_name.value(), chapters}));
			}
		}
	}
	catch (std::exception&) {
		mark_as_bad_request(resp);
	}

	return resp.done();
}

auto seriess_handler_t::on_new_series(
	const restinio::request_handle_t& req,
	restinio::router::route_params_t params)
{
	auto resp = init_resp(req->create_response(), m_allowedOrigins, req->header().try_get_field("origin") ? *req->header().try_get_field("origin") : "", "application/json");

	try {
		auto json = json_dto::from_json< series_t >(req->body());
		if (json.m_name && json.m_folder_path) {
			                if (!std::filesystem::exists(json.m_folder_path.value())) {
								mark_as_bad_request(resp);
								return resp.set_body(json_dto::to_json<series_t>({ "Folder path does not exist." })).done();
			                }

			                *m_db.lock() << "select exists(select 1 from series where folder_path = ? limit 1);"
			                    << json.m_folder_path.value()
								>> [&resp](bool exists) {
								if (exists) {
									resp.header().status_line(restinio::status_conflict());
									return resp.set_body(json_dto::to_json<series_t>({ "Duplicate Folder path." })).done();
								}
							};

							*m_db.lock() << "select exists(select 1 from series where name = ? limit 1);"
								<< json.m_name.value()
								>> [&resp](bool exists) {
								if (exists) {
									resp.header().status_line(restinio::status_conflict());
									return resp.set_body(json_dto::to_json<series_t>({ "The same name is already used, confusing..." })).done();
								}
							};

							*m_db.lock() << "insert into series (name, folder_path) values (?, ?);"
								<< json.m_name.value()
								<< json.m_folder_path.value();
							int64_t seriesInserted{ m_db.lock()->last_insert_rowid() }; // get series id

							std::vector<chapter_t> chapters{};

			                AddNewChapters(*m_db.lock(), chapters, json.m_folder_path.value().insert(0, R"(\\?\)"), seriesInserted);
							resp.set_body(json_dto::to_json<series_t>({ seriesInserted, json.m_name.value(), chapters }));
		}
		else
		{
			mark_as_bad_request(resp);
			resp.set_body(json_dto::to_json<series_t>({ "incomplete fields." }));
		}
	}
	catch (std::exception&) {
		mark_as_bad_request(resp);
		resp.set_body(json_dto::to_json<series_t>({ "Error adding series." }));
	}

	return resp.done();
}

auto seriess_handler_t::on_series_list(
	const restinio::request_handle_t& req,
	restinio::router::route_params_t)
{
	auto resp = init_resp(req->create_response(), m_allowedOrigins, req->header().try_get_field("origin") ? *req->header().try_get_field("origin") : "", "application/json");

	try {
		std::vector<series_t> series{};
		uint32_t size;
		*m_db.lock() << "select count(id) from series;"
			>> size;
		series.reserve(size);

		*m_db.lock() << "select series.id, name, count(chapter.id) as chapters from series left join chapter on chapter.series_id = series.id group by series.id;"
			>> [&resp, &series](uint64_t id, std::string name, uint32_t chapters) {
			series.emplace_back(id, name, chapters);
		};
		resp.set_body(json_dto::to_json<std::vector<series_t>>(series));
	}
	catch (std::exception&) {
		mark_as_bad_request(resp);
	}

	return resp.done();
}

auto seriess_handler_t::on_series_cover_get(
	const restinio::request_handle_t& req,
	restinio::router::route_params_t params)
{
	const auto seriesnum = restinio::cast_to< std::uint32_t >(params["series_id"]);

	auto resp = init_resp(req->create_response(), m_allowedOrigins, req->header().try_get_field("origin") ? *req->header().try_get_field("origin") : "", "image/jpeg");

	try
	{
		*m_db.lock() << "select cover from chapter where series_id = ? order by id desc limit 1;"
			        << seriesnum
			        >> [&resp](std::vector<uint8_t> blob) {
			        resp.set_body(blob);
			    };
	}
	catch (const std::exception& /*ex*/)
	{
		mark_as_bad_request(resp);
	}

	return resp.done();
}


// TODO: update cover page of a series PER user.
auto seriess_handler_t::on_series_cover_update(
	const restinio::request_handle_t& req,
	restinio::router::route_params_t params)
{
	const auto seriesnum = restinio::cast_to< std::uint32_t >(params["series_id"]);

	auto resp = init_resp(req->create_response(restinio::status_no_content()), m_allowedOrigins, req->header().try_get_field("origin") ? *req->header().try_get_field("origin") : "", "application/json");

	try
	{
	}
	catch (const std::exception& /*ex*/)
	{
		mark_as_bad_request(resp);
	}

	return resp.done();
}

//auto on_series_delete(
//	const restinio::request_handle_t& req,
//	const std::uint32_t seriesnum)
//{
//	auto resp = init_resp(req->create_response());

//	if (0 != seriesnum && seriesnum <= m_seriess.size())
//	{
//		const auto& b = m_seriess[seriesnum - 1];
//		resp.set_body(
//			"Delete series #" + std::to_string(seriesnum) + ": " +
//			b.m_title + "[" + b.m_author + "]\n");

//		m_seriess.erase(m_seriess.begin() + (seriesnum - 1));
//	}
//	else
//	{
//		resp.set_body(
//			"No series with #" + std::to_string(seriesnum) + "\n");
//	}

//	return resp.done();
//}

template < typename RESP >
static RESP
seriess_handler_t::init_resp(RESP resp, const std::vector<std::string>& allowedOrigins, const std::string& origin, const std::string& contentType)
{
	resp
		.append_header("Server", "RESTinio server /v.0.6.17")
		.append_header_date_field()
		.append_header("Content-Type", contentType)

		// CORS
		.append_header("Access-Control-Allow-Origin", std::find(std::begin(allowedOrigins), std::end(allowedOrigins), origin) != std::end(allowedOrigins) ? origin.data() : allowedOrigins.at(0))
		.append_header("Access-Control-Allow-Credentials", "true");

	return resp;
}

template < typename RESP >
static void
seriess_handler_t::mark_as_bad_request(RESP& resp)
{
	resp.header().status_line(restinio::status_bad_request());
}

//std::unique_ptr< easy_parser_router_t, std::default_delete<easy_parser_router_t> >
void series_handler(restinio::router::express_router_t<>& router, std::shared_ptr<sqlite::database> db, const std::vector<std::string>& allowedOrigins)
{
	auto handler = std::make_shared< seriess_handler_t >(std::ref(db), std::ref(allowedOrigins));

	auto by = [&](auto method) {
		using namespace std::placeholders;
		return std::bind(method, handler, _1, _2);
	};

	auto method_not_allowed = [](const auto& req, const auto&) {
		return req->create_response(restinio::status_method_not_allowed())
			.connection_close()
			.done();
	};

	router.http_get("/api/v2/series/cover/:series_id", by(&seriess_handler_t::on_series_cover_get));

	router.http_get("/api/v2/series/:series_id",
		by(&seriess_handler_t::on_series_get));
	router.http_delete("/api/v2/series/:series_id",
		by(&seriess_handler_t::on_series_delete));

	router.http_get("/api/v2/series",
		by(&seriess_handler_t::on_series_list));
	router.http_post("/api/v2/series",
		by(&seriess_handler_t::on_new_series));
	router.http_put("/api/v2/series",
		by(&seriess_handler_t::on_series_update));

	router.add_handler(
		restinio::router::none_of_methods(
			restinio::http_method_get(),
			restinio::http_method_post(),
			restinio::http_method_put()), "/api/v2/series",
		method_not_allowed);

	router.add_handler(
		restinio::router::none_of_methods(
			restinio::http_method_get(),
			restinio::http_method_delete()), "/api/v2/series/cover/:series_id",
		method_not_allowed);
}