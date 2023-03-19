#include "test_example_functions.h"
#include "remove_duplicates.h"
#include "log_duration.h"
#include <assert.h>

using namespace std::string_literals;
void TestRemoveDuplicates() {
    SearchServer search_server("and with"s);

    AddDocument(search_server, 1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    AddDocument(search_server, 2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });
    AddDocument(search_server, 3, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    RemoveDuplicates(search_server);

    assert(search_server.GetDocumentCount() == 2);
}

void TestRemoveDuplicatesTime() {
    SearchServer search_server("and with"s);

    AddDocument(search_server, 1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    AddDocument(search_server, 2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });
    AddDocument(search_server, 3, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    LOG_DURATION_STREAM("TimeRemoveDuplicate", std::cerr);
    RemoveDuplicates(search_server);

}

void TestAddDocument() {
	const int doc_id1 = 42;
	const int doc_id2 = 43;
	const int doc_id3 = 44;
	const std::string content1 = "cat in the city"s;
	const std::string content2 = "city is big"s;
	const std::string content3 = "dog is beautiful"s;
	const std::vector<int> ratings = { 1, 2, 3 };

	{
		SearchServer server("and with"s);
		server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings);
		assert(server.GetDocumentCount() == 3);
	}
}

void TestAddDocumentTime() {
	const int doc_id1 = 42;
	const std::string content1 = "cat in the city"s;
	const std::vector<int> ratings = { 1, 2, 3 };

	{
		SearchServer server("and with"s);

		LOG_DURATION_STREAM("TimeAddDocument", std::cerr);
		server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
	}
}
