#include "process_queries.h"
#include <algorithm>
#include <execution>

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server,
	const std::vector<std::string>& queries)
{
	std::vector<std::vector<Document>> result(queries.size());
	std::transform(std::execution::par, queries.begin(), queries.end(), result.begin(), [&search_server](std::string_view str) {
		return search_server.FindTopDocuments(str);
		});

	return result;
}

std::vector<Document> ProcessQueriesJoined(const SearchServer& search_server,
	const std::vector<std::string>& queries)
{
	std::vector<std::vector<Document>> result_last = ProcessQueries(search_server, queries);
	size_t size = 0;
	for (std::vector<Document>& Document : result_last) {
		size += Document.size();
	}
	std::vector<Document> result;
	result.reserve(size);
	
	for (std::vector<Document>& Document : result_last) {
		result.insert(result.end(), Document.begin(), Document.end());
	}

	return result;
}
