#pragma once

//#include <restinio/router/easy_parser_router.hpp>
#include <restinio/router/express.hpp>
#include <restinio/cast_to.hpp>
#include <json_dto/pub.hpp>
#include <sqlite_modern_cpp.h>
#include "jwt-cpp/jwt.h"
#include "jwt-cpp/traits/boost-json/traits.h"
#include "boost/json/parser.hpp"
#include <Magick++/Functions.h>
#include <Magick++/Image.h>
#include "../MimeTypes.h"
#include <filesystem>
#include <archive.h>
#include <archive_entry.h>

struct page_t {
	page_t() = default;

	page_t(
		uint32_t index,
		std::optional < bool > isWide
	) : m_index{ index },
		m_isWide{ isWide.value() ? isWide : std::nullopt} {}


	template < typename JSON_IO >
	void
		json_io(JSON_IO& io)
	{
		io
			& json_dto::mandatory("i", m_index)
			& json_dto::optional("isWide", m_isWide, std::nullopt);
	}

	uint32_t m_index;
	std::optional < bool > m_isWide;
};

struct chapter_t
{
	chapter_t() = default;

	chapter_t(
		uint64_t id,
		std::string file_name,
		std::string error)
		: m_id{ id }
		, m_file_name{ std::move(file_name) }
		, m_error{ std::move(error) }
	{}

	// progress
	chapter_t(
		uint64_t id,
		uint32_t progress)
		: m_id{ id }
		, m_progress{ std::move(progress) }
	{}

	// chapter
	chapter_t(
		uint64_t id,
		std::string name,
		std::vector<page_t> pages)
		: m_id{ id }
		, m_name{ std::move(name) }
		, m_pages{ std::move(pages) }
	{}

	chapter_t(
		uint64_t id,
		std::string name,
		uint32_t pages)
		: m_id{ id }
		, m_name{ std::move(name) }
		, m_pagesCount{ pages }
	{}

	chapter_t(
		uint64_t id,
		std::string file_name,
		uint32_t pages,
		std::optional<std::string> error)
		: m_id{ id }
		, m_file_name{ std::move(file_name) }
		, m_pagesCount{ pages }
		, m_error{ std::move(error) }
	{}

	template < typename JSON_IO >
	void
		json_io(JSON_IO& io)
	{
		io
			& json_dto::mandatory("id", m_id)
			& json_dto::optional("name", m_name, std::nullopt)
			& json_dto::optional("series_id", m_series_id, std::nullopt)
			& json_dto::optional("md5", m_md5, std::nullopt)
			& json_dto::optional("file_name", m_file_name, std::nullopt)
			& json_dto::optional("created_at", m_created_at, std::nullopt)
			& json_dto::optional("updated_at", m_updated_at, std::nullopt)
			& json_dto::optional("error", m_error, std::nullopt)
			& json_dto::optional("progress", m_progress, std::nullopt)
			& json_dto::optional("pages", m_pages, std::nullopt)
			& json_dto::optional("pages_count", m_pagesCount, std::nullopt);
	}

	uint64_t m_id;
	std::optional < std::string > m_name;
	std::optional < uint64_t >    m_series_id;
	std::optional < std::string > m_md5;
	std::optional < std::string > m_file_name;
	std::optional < std::string > m_created_at;
	std::optional < std::string > m_updated_at;
	std::optional < uint32_t > m_progress;
	std::optional < uint32_t > m_page;
	std::optional < uint32_t > m_pagesCount;
	std::optional < std::string > m_error;
	std::optional < std::vector<page_t> > m_pages;
};

class chapters_handler_t
{
public:
	explicit chapters_handler_t(std::shared_ptr<sqlite::database> db, const std::vector<std::string>& allowedOrigins);

	chapters_handler_t(const chapters_handler_t&) = delete;
	chapters_handler_t(chapters_handler_t&&) = delete;

	//auto on_chapters_list(
	//	const restinio::request_handle_t& req,
	//	const std::uint32_t seriesnum) const;
	auto on_chapter_get(
		const restinio::request_handle_t& req,
		restinio::router::route_params_t params);
	auto on_page_get(
		const restinio::request_handle_t& req,
		restinio::router::route_params_t params);

	auto on_chapter_cover_get(
		const restinio::request_handle_t& req,
		restinio::router::route_params_t params);
	auto on_chapter_cover_update(
		const restinio::request_handle_t& req,
		restinio::router::route_params_t params);

	//auto on_chapter_page_get(
	//	const restinio::request_handle_t& req,
	//	const std::uint32_t chapternum,
	//	const std::uint32_t pagenum);

	auto on_chapter_progress_get(
		const restinio::request_handle_t& req,
		restinio::router::route_params_t params);
	auto on_chapter_progress_update(
		const restinio::request_handle_t& req,
		restinio::router::route_params_t params);
private:
	std::weak_ptr<sqlite::database> m_db;
	const std::vector<std::string>& m_allowedOrigins;

	template < typename RESP >
	static RESP
		init_resp(RESP resp, const std::vector<std::string>& allowedOrigins, const std::string& origin, const std::string& contentType);
	template < typename RESP >
	static void
		mark_as_bad_request(RESP& resp);
};

void chapter_handler(restinio::router::express_router_t<>& router, std::shared_ptr<sqlite::database> db, const std::vector<std::string>& allowedOrigins);