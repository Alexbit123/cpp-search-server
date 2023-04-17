#include "process_queries.h"
#include <algorithm>
#include <execution>

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, 
	const std::vector<std::string>& queries)
{
	std::vector<std::vector<Document>> result(queries.size());
	std::transform(std::execution::par, queries.begin(), queries.end(), result.begin(), [&search_server](std::string str){
		return search_server.FindTopDocuments(str);
		});

	return result;
}

std::vector<Document> ProcessQueriesJoined(const SearchServer& search_server,
	const std::vector<std::string>& queries)
{
	std::vector<std::vector<Document>> result_last = ProcessQueries(search_server, queries);
	size_t size = 0;
	for (auto& i : result_last) {
		size += i.size();
	}
	std::vector<Document> result;
	result.reserve(size);
	
	for (auto& v : result_last) {
		for (auto& i : v) {
			result.push_back(i);
		}
	}

	return result;
}