#pragma once

#include "../Chapter/Chapter.h"
#include "../Chapter/ProcessChapters.h"

struct series_t
{
	series_t() = default;

	series_t(
		int64_t id,
		std::string name,
		std::vector<chapter_t> chapters)
		: m_id{ id }
		, m_name{ std::move(name) }
		, m_chapters{ std::move(chapters) }
	{}

	series_t(
		int64_t id,
		std::string name,
		uint32_t chapters)
		: m_id{ id }
		, m_name{ std::move(name) }
		, m_chaptersCount{ chapters }
	{}

	series_t(
		std::string error)
		: m_error{ std::move(error) }
	{}

	template < typename JSON_IO >
	void
		json_io(JSON_IO& io)
	{
		io
			& json_dto::optional("id", m_id, std::nullopt)
			& json_dto::optional("name", m_name, std::nullopt)
			& json_dto::optional("md5", m_md5, std::nullopt)
			& json_dto::optional("folder_path", m_folder_path, std::nullopt)
			& json_dto::optional("isBlackList", m_isBlackList, std::nullopt)
			& json_dto::optional("created_at", m_createdAt, std::nullopt)
			& json_dto::optional("updated_at", m_updatedAt, std::nullopt)
			& json_dto::optional("chapters", m_chapters, std::nullopt)
			& json_dto::optional("chapters_count", m_chaptersCount, std::nullopt)
		& json_dto::optional("error", m_error, std::nullopt);
	}

	std::optional < int64_t > m_id;
	std::optional < std::string > m_name;
	std::optional < std::string > m_md5;
	std::optional < std::string > m_folder_path;
	std::optional < bool > m_isBlackList;
	std::optional < std::string > m_createdAt;
	std::optional < uint32_t > m_updatedAt;
	std::optional < std::vector<chapter_t> > m_chapters;
	std::optional < std::string > m_error;
	std::optional < uint32_t > m_chaptersCount;
};

class seriess_handler_t
{
public:
	explicit seriess_handler_t(std::shared_ptr<sqlite::database> db, const std::vector<std::string>& allowedOrigins);

	seriess_handler_t(const seriess_handler_t&) = delete;
	seriess_handler_t(seriess_handler_t&&) = delete;

	auto on_series_get(
		const restinio::request_handle_t& req,
		restinio::router::route_params_t params);
	auto on_series_delete(
		const restinio::request_handle_t& req,
		restinio::router::route_params_t params);
	auto on_new_series(
		const restinio::request_handle_t& req,
		restinio::router::route_params_t params);
	auto on_series_update(
		const restinio::request_handle_t& req,
		restinio::router::route_params_t);
	auto on_series_list(
		const restinio::request_handle_t& req,
		restinio::router::route_params_t);

	auto on_series_cover_get(
		const restinio::request_handle_t& req,
		restinio::router::route_params_t params);
	auto on_series_cover_update(
		const restinio::request_handle_t& req,
		restinio::router::route_params_t params);

	//auto on_series_page_get(
	//	const restinio::request_handle_t& req,
	//	const std::uint32_t seriesnum,
	//	const std::uint32_t pagenum);
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

void series_handler(restinio::router::express_router_t<>& router, std::shared_ptr<sqlite::database> db, const std::vector<std::string>& allowedOrigins);