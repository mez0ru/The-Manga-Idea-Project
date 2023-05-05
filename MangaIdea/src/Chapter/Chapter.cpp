#include "Chapter.h"

chapters_handler_t::chapters_handler_t(std::shared_ptr<sqlite::database> db, const std::vector<std::string>& allowedOrigins)
	: m_db{ db }
	, m_allowedOrigins{ allowedOrigins }
{}

auto chapters_handler_t::on_page_get(
	const restinio::request_handle_t& req,
	restinio::router::route_params_t params)
{
	const auto chapternum = restinio::cast_to< std::uint64_t >(params["chapter_id"]);
	const auto pagenum = restinio::cast_to< std::uint32_t >(params["page_index"]);
	std::string contentType;
	char* buff = nullptr;
	//Magick::Blob* out = new Magick::Blob{};

	auto resp = init_resp(req->create_response(), m_allowedOrigins, req->header().try_get_field("origin") ? *req->header().try_get_field("origin") : "", "application/json");
	
	*m_db.lock() << "select folder_path, chapter.file_name from series inner join chapter on chapter.series_id = series.id inner join page on page.chapter_id = chapter.id where chapter.id = ? and i = ?;"
	<< chapternum
	<< pagenum
	>> [&, pagenum](std::string f_path, std::string f_name) {
	std::filesystem::path series_path = f_path;
	std::filesystem::path file_name = f_name;

	struct archive* a;
	struct archive_entry* entry = NULL;
	la_ssize_t r;
	a = archive_read_new();
	archive_read_support_filter_all(a);
	archive_read_support_format_all(a);

	r = archive_read_open_filename(a, (series_path / file_name).generic_string().c_str(), 10240);
	if (r == ARCHIVE_OK) {
		for (int i = 0; i <= pagenum; i++)
			if ((r = archive_read_next_header(a, &entry)) != ARCHIVE_OK)
			{
				mark_as_bad_request(resp);
				resp.done();
				break;
			}
		if (r == ARCHIVE_OK) {
			std::string name{ archive_entry_pathname(entry) };
			
			// From the docs, it seems that "size" is not a reliable way to get
			// the size of the file, but so far it's the easiest for now.
			// in the future I will use a more reliable way to do this.
			size_t size = archive_entry_size(entry);
			resp.header().set_field("Content-Type", MimeTypes::getType(name.c_str()));
			
			buff = new char[size];
			int64_t offset = 0;
			// Read chunks from the archive
			do {
				offset += archive_read_data(a, buff + offset, 10240);
			} while ((unsigned)(offset - 0) < (size - 0));
			// Since this is a signed integer, we want to prevent bugs that could terminate the program,
			// So we have to check if a number is between 0 and size, with only one comparison.
			// https://stackoverflow.com/questions/17095324/fastest-way-to-determine-if-an-integer-is-between-two-integers-inclusive-with/17095534#17095534

			// 
			//Magick::Image img{ Magick::Blob{buff, size} };
			//img.filterType(MagickCore::LanczosSharpFilter);
			//img.quality(92);
			//img.resize(resizeWithAspectRatioFit(img.baseColumns(), img.baseRows(), 1000, 1700));
			//
			//img.write(out, "jpeg");
			
			if (offset < 1) // Could not read the entry.
				mark_as_bad_request(resp);
			else
				resp.set_body(std::string_view{ buff, size});
				//resp.set_body(std::string_view{(char*)out->data(), out->length()});
		}
	}
	archive_read_free(a);
};

	//return response, callback to release any resources left.
	return resp.done([buff/*, out*/](const asio::error_code&) {
		if (buff != nullptr)
		{
			delete[] buff;
			//delete out;
		}
		});
}

auto chapters_handler_t::on_chapter_get(
	const restinio::request_handle_t& req,
	restinio::router::route_params_t params)
{
	const auto chapternum = restinio::cast_to< std::uint32_t >(params["chapter_id"]);

	auto resp = init_resp(req->create_response(), m_allowedOrigins, req->header().try_get_field("origin") ? *req->header().try_get_field("origin") : "", "application/json");
	
	try {
		std::vector<page_t> pages{};
		std::string name;
		*m_db.lock() << "select chapter.file_name, count(page.id) from page inner join chapter on chapter.id = page.chapter_id where chapter_id = ?;"
			<< chapternum
			>> [&name, &pages](std::string file_name, uint32_t count) {
				pages.reserve(count);
				name = file_name.substr(0, file_name.find_last_of("."));
			};
		
		*m_db.lock() << "select i, (width > height) as isWide from page where chapter_id = ? ORDER BY page.file_name;"
			<< chapternum
			>> [&pages](uint32_t index, bool isWide) {
			pages.push_back({ index, isWide });
		};

		resp.set_body(json_dto::to_json<chapter_t>(chapter_t{ chapternum, name, pages }));
	} catch (std::exception&) {
			mark_as_bad_request(resp);
		}

	return resp.done();
}

auto chapters_handler_t::on_chapter_cover_get(
	const restinio::request_handle_t& req,
	restinio::router::route_params_t params)
{
	const auto chapternum = restinio::cast_to< std::uint32_t >(params["chapter_id"]);

	auto resp = init_resp(req->create_response(), m_allowedOrigins, req->header().try_get_field("origin") ? *req->header().try_get_field("origin") : "", "image/jpeg");

	try
	{
		*m_db.lock() << "select cover from chapter where id = ?;"
			<< chapternum
			>> [&resp](std::unique_ptr<std::vector<char>> cover) {
			if (cover != nullptr) {
				resp.set_body(*cover);
			}
		};
	}
	catch (const std::exception& /*ex*/)
	{
		mark_as_bad_request(resp);
	}

	return resp.done();
}


// TODO: update cover page of a chapter PER user.
auto chapters_handler_t::on_chapter_cover_update(
	const restinio::request_handle_t& req,
	restinio::router::route_params_t params)
{
	const auto chapternum = restinio::cast_to< std::uint32_t >(params["chapter_id"]);

	auto resp = init_resp(req->create_response(restinio::status_no_content()), m_allowedOrigins, req->header().try_get_field("origin") ? *req->header().try_get_field("origin") : "", "application/json");

	try
	{
		auto c = json_dto::from_json< chapter_t >(req->body());
		uint32_t max_index = 0;
		*m_db.lock() << "select max(i) from page where chapter_id = ?;"
			<< chapternum
			>> max_index;

		if (max_index >= c.m_progress.value())
		{
		//	auto cookie = req->header().try_get_field("cookie");
		//	int cookie_offset = cookie->find("session=") + 8;
		//	std::string cookie_session{ cookie->c_str() + cookie_offset, cookie->find(";", cookie_offset) - cookie_offset };
		//	auto decoded = jwt::decode<jwt::traits::boost_json>(cookie_session);
		//	auto xxx = decoded.get_payload_claim("email").as_string();
		//	*m_db.lock() << "update progress set page = ? from (select id from user where user.email = ?) as user where user_id = user.id and chapter_id = ?;"
		//		<< c.m_progress
		//		<< decoded.get_payload_claim("email").as_string()
		//		<< chapternum;
		}
		else {
			mark_as_bad_request(resp);
		}
	}
	catch (const std::exception& /*ex*/)
	{
		mark_as_bad_request(resp);
	}

	return resp.done();
}


auto chapters_handler_t::on_chapter_progress_get(
	const restinio::request_handle_t& req,
	restinio::router::route_params_t params)
{
	const auto chapternum = restinio::cast_to< std::uint32_t >(params["chapter_id"]);

	auto resp = init_resp(req->create_response(), m_allowedOrigins, req->header().try_get_field("origin") ? *req->header().try_get_field("origin") : "", "application/json");

	try
	{
			auto cookie = req->header().try_get_field("cookie");
			int cookie_offset = cookie->find("session=") + 8;
			std::string cookie_session{ cookie->c_str() + cookie_offset, cookie->find(";", cookie_offset) - cookie_offset };
			auto decoded = jwt::decode<jwt::traits::boost_json>(cookie_session);
			auto xxx = decoded.get_payload_claim("email").as_string();

			uint32_t progress = UINT32_MAX; // MAX VALUE

			*m_db.lock() << "select page from progress inner join user on user.id = progress.user_id where chapter_id = ? and user.email = ?;"
				<< chapternum
				<< decoded.get_payload_claim("email").as_string()
				>> [&progress](uint32_t page) {
				progress = page;
				};

			// If the record doesn't exist, then create one
			if (progress == UINT32_MAX)
			{
				progress = 0;
				*m_db.lock() << "select exists(select 1 from chapter where id = ? limit 1);"
					<< chapternum
					>> cookie_offset;
				if (cookie_offset)
				*m_db.lock() << "insert into progress (user_id, chapter_id, page) select id, ?, 0 from user where user.email = ?;"
					<< chapternum
					<< decoded.get_payload_claim("email").as_string();
			}

			resp.set_body(json_dto::to_json<chapter_t>({ chapternum, progress }));
	}
	catch (const std::exception& /*ex*/)
	{
		mark_as_bad_request(resp);
	}

	return resp.done();
}

auto chapters_handler_t::on_chapter_progress_update(
	const restinio::request_handle_t& req,
	restinio::router::route_params_t params)
{
	const auto chapternum = restinio::cast_to< std::uint32_t >(params["chapter_id"]);

	auto resp = init_resp(req->create_response(restinio::status_no_content()), m_allowedOrigins, req->header().try_get_field("origin") ? *req->header().try_get_field("origin") : "", "application/json");

	try
	{
		auto c = json_dto::from_json< chapter_t >(req->body());
		int max_index = 0;
		*m_db.lock() << "select max(i) from page where chapter_id = ?;"
			<< chapternum
			>> max_index;

		if (max_index >= c.m_progress.value())
		{
			auto cookie = req->header().try_get_field("cookie");
			int cookie_offset = cookie->find("session=") + 8;
			std::string cookie_session{ cookie->c_str() + cookie_offset, cookie->find(";", cookie_offset) - cookie_offset };
			auto decoded = jwt::decode<jwt::traits::boost_json>(cookie_session);
			auto xxx = decoded.get_payload_claim("email").as_string();
			*m_db.lock() << "update progress set page = ? from (select id from user where user.email = ?) as user where user_id = user.id and chapter_id = ?;"
				<< c.m_progress.value()
				<< decoded.get_payload_claim("email").as_string()
				<< chapternum;
		}
		else {
			mark_as_bad_request(resp);
		}
	}
	catch (const std::exception& /*ex*/)
	{
		mark_as_bad_request(resp);
	}

	return resp.done();
}

//auto on_chapter_delete(
//	const restinio::request_handle_t& req,
//	const std::uint32_t chapternum)
//{
//	auto resp = init_resp(req->create_response());

//	if (0 != chapternum && chapternum <= m_chapters.size())
//	{
//		const auto& b = m_chapters[chapternum - 1];
//		resp.set_body(
//			"Delete chapter #" + std::to_string(chapternum) + ": " +
//			b.m_title + "[" + b.m_author + "]\n");

//		m_chapters.erase(m_chapters.begin() + (chapternum - 1));
//	}
//	else
//	{
//		resp.set_body(
//			"No chapter with #" + std::to_string(chapternum) + "\n");
//	}

//	return resp.done();
//}

template < typename RESP >
static RESP
chapters_handler_t::init_resp(RESP resp, const std::vector<std::string>& allowedOrigins, const std::string& origin, const std::string& contentType)
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
chapters_handler_t::mark_as_bad_request(RESP& resp)
{
	resp.header().status_line(restinio::status_bad_request());
}

//std::unique_ptr< easy_parser_router_t, std::default_delete<easy_parser_router_t> >
void chapter_handler(restinio::router::express_router_t<>& router, std::shared_ptr<sqlite::database> db, const std::vector<std::string>& allowedOrigins)
{
	auto handler = std::make_shared< chapters_handler_t >(std::ref(db), std::ref(allowedOrigins));

	auto by = [&](auto method) {
		using namespace std::placeholders;
		return std::bind(method, handler, _1, _2);
	};

	auto method_not_allowed = [](const auto& req, const auto&) {
		return req->create_response(restinio::status_method_not_allowed())
			.connection_close()
			.done();
	};

	router.http_get("/api/v2/chapter/cover/:chapter_id", by(&chapters_handler_t::on_chapter_cover_get));

	router.http_get("/api/v2/chapter/progress/:chapter_id",
		by(&chapters_handler_t::on_chapter_progress_get));
	router.http_put("/api/v2/chapter/progress/:chapter_id",
		by(&chapters_handler_t::on_chapter_progress_update));

	router.http_get("/api/v2/chapter/:chapter_id",
		by(&chapters_handler_t::on_chapter_get));
	router.http_get("/api/v2/chapter/:chapter_id/page/:page_index",
		by(&chapters_handler_t::on_page_get));

	router.add_handler(
		restinio::router::none_of_methods(
			restinio::http_method_get()),"/api/v2/chapter/:chapter_id",
		method_not_allowed);

	router.add_handler(
		restinio::router::none_of_methods(
			restinio::http_method_get()),"/api/v2/chapter/:chapter_id/page/:page_index",
		method_not_allowed);

	router.add_handler(
		restinio::router::none_of_methods(
			restinio::http_method_get()),"/api/v2/chapter/cover/:chapter_id",
		method_not_allowed);

	router.add_handler(
		restinio::router::none_of_methods(
			restinio::http_method_get(),
			restinio::http_method_put()),"/api/v2/chapter/progress/:chapter_id",
		method_not_allowed);
}