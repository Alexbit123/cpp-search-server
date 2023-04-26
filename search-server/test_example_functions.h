#pragma once

#include "search_server.h"
#include <assert.h>

using namespace std;

/* Реализация макросов и перегрузки операторов вывода */
template <typename T, typename U>
ostream& operator<<(ostream& out, const map<T, U> m) {
	bool is_first = true;
	out << "{";
	for (const auto& [key, element] : m) {

		if (!is_first) {
			out << ", ";
		}
		is_first = false;
		out << key << ": " << element;
	}
	out << "}";
	return out;
}

template <typename T>
ostream& operator<<(ostream& out, const set<T> s) {
	bool is_first = true;
	out << "{";
	for (const T& element : s) {
		if (!is_first) {
			out << ", ";
		}
		is_first = false;
		out << element;
	}
	out << "}";
	return out;
}

template <typename T>
ostream& operator<<(ostream& out, const vector<T> v) {
	bool is_first = true;
	out << "[";
	for (const T& element : v) {
		if (!is_first) {
			out << ", ";
		}
		is_first = false;
		out << element;
	}
	out << "]";
	return out;
}


template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
	const string& func, unsigned line, const string& hint) {
	if (t != u) {
		cerr << boolalpha;
		cerr << file << "("s << line << "): "s << func << ": "s;
		cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
		cerr << t << " != "s << u << "."s;
		if (!hint.empty()) {
			cerr << " Hint: "s << hint;
		}
		cerr << endl;
		abort();
	}
}

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
	const string& hint) {
	if (!value) {
		cerr << file << "("s << line << "): "s << func << ": "s;
		cerr << "ASSERT("s << expr_str << ") failed."s;
		if (!hint.empty()) {
			cerr << " Hint: "s << hint;
		}
		cerr << endl;
		abort();
	}
}

template <typename T>
void RunTestImpl(const T func, const string& func_str) {
	func();
	cerr << func_str << " OK" << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)
#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))
#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

void TestExcludeStopWordsFromAddedDocumentContent() {
	const int doc_id = 42;
	const string content = "cat in the city"s;
	const vector<int> ratings = { 1, 2, 3 };
	{
		SearchServer server(""s);
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		const auto found_docs = server.FindTopDocuments("in"s);
		ASSERT_EQUAL(found_docs.size(), 1u);
		const Document& doc0 = found_docs[0];
		ASSERT_EQUAL(doc0.id, doc_id);
	}

	{
		SearchServer server("in the"s);
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
			"Stop words must be excluded from documents"s);
	}
}

void TestAddDocument() {
	const int doc_id1 = 42;
	const int doc_id2 = 43;
	const int doc_id3 = 44;
	const string content1 = "cat in the city"s;
	const string content2 = "city is big"s;
	const string content3 = "dog is beautiful"s;
	const vector<int> ratings = { 1, 2, 3 };

	{
		SearchServer server("in the"s);
		server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings);
		ASSERT_EQUAL(server.GetDocumentCount(), 3);
	}
}

void TestMinusWord() {
	const int doc_id1 = 42;
	const int doc_id2 = 43;
	const int doc_id3 = 44;
	const string content1 = "cat in the city"s;
	const string content2 = "city is big"s;
	const string content3 = "dog is beautiful"s;
	const vector<int> ratings = { 1, 2, 3 };

	{
		SearchServer server("in the"s);
		server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings);
		const auto found_docs = server.FindTopDocuments("dog -city"s);
		ASSERT_EQUAL_HINT(found_docs.size(), 1, "Documents with minus words should be excluded"s);
		const Document& doc0 = found_docs[0];
		ASSERT_EQUAL(doc0.id, doc_id3);
	}
}

void TestMatchDocument() {
	const int doc_id1 = 42;
	const int doc_id2 = 43;
	const int doc_id3 = 44;
	const string content1 = "cat in the city"s;
	const string content2 = "city is big"s;
	const string content3 = "dog is beautiful a color the best"s;
	const vector<int> ratings = { 1, 2, 3 };

	std::string query = "cat dog big"s;

	{
		SearchServer server("in the"s);
		server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings);

		const auto [found_words1, found_status1] = server.MatchDocument(query, doc_id1);
		ASSERT_EQUAL(found_words1.size(), 1);
		ASSERT_EQUAL(found_words1[0], "cat");

		const auto [found_words2, found_status2] = server.MatchDocument("dog color -best"s, doc_id3);
		ASSERT(found_words2.size() == 0);
	}
}

void TestSortRelevance() {
	const int doc_id1 = 42;
	const int doc_id2 = 43;
	const int doc_id3 = 44;


	const std::vector<int> ratings1 = { 1, 2, 3, 4, 5 };
	const std::vector<int> ratings2 = { -1, -2, 30, -3, 44, 5 };
	const std::vector<int> ratings3 = { 12, -20, 80, 0, 8, 0, 0, 9, 67 };

	std::string query = "city"s;

	{
		const string content1 = "cat in the city"s;
		const string content2 = "city is big"s;
		const string content3 = "dog is beautiful a color the best"s;

		SearchServer server("in the"s);
		server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings1);
		server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings1);
		server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings1);

		const auto found_docs = server.FindTopDocuments(query);
		ASSERT_EQUAL(found_docs.size(), 2);
		const Document& doc0 = found_docs[0];
		const Document& doc1 = found_docs[1];
		ASSERT(doc0.relevance > doc1.relevance);
	}

	{
		const string content1 = "cat in the city"s;
		const string content2 = "cat in the city"s;
		const string content3 = "cat in the city"s;

		SearchServer server("in the"s);
		server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings1);
		server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
		server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings3);

		const auto found_docs = server.FindTopDocuments(query);
		for (const Document& document : found_docs) {
			std::cout << "{ "
				<< "document_id = " << document.id << ", "
				<< "relevance = " << document.relevance << ", "
				<< "rating = " << document.rating << " }" << std::endl;
		}
	}
}

void TestComputeAverageRating() {
	const int doc_id1 = 42;
	const int doc_id2 = 43;
	const int doc_id3 = 44;
	const string content1 = "cat in the city"s;
	const string content2 = "city is big"s;
	const string content3 = "dog is beautiful a color the best city"s;
	const vector<int> ratings1 = { 1, 2, 3 };
	const vector<int> ratings2 = { 5, 6, 7, 8 };
	const vector<int> ratings3 = { 10, 15, 3 };

	{
		SearchServer server("in the"s);
		server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings1);
		server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
		server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings3);
		const auto found_docs = server.FindTopDocuments("city"s);
		const Document& doc0 = found_docs[0];
		const Document& doc1 = found_docs[1];
		const Document& doc2 = found_docs[2];
		ASSERT_EQUAL(doc0.rating, 9);
		ASSERT_EQUAL(doc1.rating, 6);
		ASSERT_EQUAL(doc2.rating, 2);
	}
}

void TestResultFilterPredicate() {
	const int doc_id1 = 42;
	const int doc_id2 = 43;
	const int doc_id3 = 44;
	const string content1 = "cat in the city"s;
	const string content2 = "city is big"s;
	const string content3 = "dog is beautiful a color the best city"s;
	const vector<int> ratings = { 1, 2, 3 };

	{
		SearchServer server("in the"s);
		server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(doc_id2, content2, DocumentStatus::BANNED, ratings);
		server.AddDocument(doc_id3, content3, DocumentStatus::IRRELEVANT, ratings);
		const auto found_docs = server.FindTopDocuments("city"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
		ASSERT_EQUAL_HINT(found_docs.size(), 2, "All documents with an odd id must be excluded");
		const Document& doc0 = found_docs[0];
		const Document& doc1 = found_docs[1];
		ASSERT(doc0.id % 2 == 0);
		ASSERT(doc1.id % 2 == 0);
	}

	{
		SearchServer server("in the"s);
		server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(doc_id2, content2, DocumentStatus::BANNED, ratings);
		server.AddDocument(doc_id3, content3, DocumentStatus::IRRELEVANT, ratings);
		const auto found_docs = server.FindTopDocuments("city"s);
		ASSERT_EQUAL_HINT(found_docs.size(), 1, "All documents with an odd id must be excluded");
		const Document& doc0 = found_docs[0];
		ASSERT(doc0.id == doc_id1);
	}

	{
		SearchServer server("in the"s);
		server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(doc_id2, content2, DocumentStatus::BANNED, ratings);
		server.AddDocument(doc_id3, content3, DocumentStatus::IRRELEVANT, ratings);
		const auto found_docs = server.FindTopDocuments("city"s, DocumentStatus::BANNED);
		ASSERT_EQUAL_HINT(found_docs.size(), 1, "All documents with an odd id must be excluded");
		const Document& doc0 = found_docs[0];
		ASSERT_EQUAL(doc0.id, doc_id2);
	}
}

void TestFindAllDocument() {
	const int doc_id1 = 42;
	const int doc_id2 = 43;
	const int doc_id3 = 44;
	const string content1 = "cat in the city"s;
	const string content2 = "city is big"s;
	const string content3 = "dog is beautiful a color the best city"s;
	const vector<int> ratings = { 1, 2, 3 };

	{
		SearchServer server("in the"s);
		server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(doc_id2, content2, DocumentStatus::BANNED, ratings);
		server.AddDocument(doc_id3, content3, DocumentStatus::IRRELEVANT, ratings);
		const auto found_docs = server.FindTopDocuments("city"s, DocumentStatus::BANNED);
		ASSERT(found_docs.size() == 1);
		const Document& doc0 = found_docs[0];
		ASSERT_EQUAL(doc0.id, doc_id2);
	}

	{
		SearchServer server("in the"s);
		server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(doc_id2, content2, DocumentStatus::BANNED, ratings);
		server.AddDocument(doc_id3, content3, DocumentStatus::IRRELEVANT, ratings);
		const auto found_docs = server.FindTopDocuments("city"s, DocumentStatus::ACTUAL);
		ASSERT(found_docs.size() == 1);
		const Document& doc0 = found_docs[0];
		ASSERT_EQUAL(doc0.id, doc_id1);
	}

	{
		SearchServer server("in the"s);
		server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(doc_id2, content2, DocumentStatus::BANNED, ratings);
		server.AddDocument(doc_id3, content3, DocumentStatus::IRRELEVANT, ratings);
		const auto found_docs = server.FindTopDocuments("city"s, DocumentStatus::IRRELEVANT);
		ASSERT(found_docs.size() == 1);
		const Document& doc0 = found_docs[0];
		ASSERT_EQUAL(doc0.id, doc_id3);
	}
}

void TestCorrectRelevance() {
	const int doc_id1 = 42;
	const int doc_id2 = 43;
	const int doc_id3 = 44;
	const string content1 = "cat in the city"s;
	const string content2 = "city is big"s;
	const string content3 = "dog is beautiful a color the best"s;
	const vector<int> ratings = { 1, 2, 3 };

	{
		SearchServer server("in the"s);
		server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings);
		const auto found_docs = server.FindTopDocuments("city"s);
		ASSERT(found_docs.size() == 2);
		const Document& doc0 = found_docs[0];
		const Document& doc1 = found_docs[1];
		double computed_relevance = log(3 * 1.0 / 2) * 1.0 / 3;
		ASSERT((computed_relevance - doc0.relevance) < 0.00001);
		computed_relevance = log(3 * 1.0 / 2) * 1.0 / 4;
		ASSERT((computed_relevance - doc1.relevance) < 0.00001);
	}

}



void Test() {
	const std::vector<int> ratings1 = { 1, 2, 3, 4, 5 };
	const std::vector<int> ratings2 = { -1, -2, 30, -3, 44, 5 };
	const std::vector<int> ratings3 = { 12, -20, 80, 0, 8, 0, 0, 9, 67 };

	std::string stop_words;
	SearchServer search_server(stop_words);

	search_server.AddDocument(0, "белый кот и модный ошейник", DocumentStatus::ACTUAL, ratings1);
	search_server.AddDocument(1, "пушистый кот пушистый хвост", DocumentStatus::ACTUAL, ratings2);
	search_server.AddDocument(2, "ухоженный пёс выразительные глаза", DocumentStatus::ACTUAL,
		ratings3);

	SearchServer const_search_server = search_server;

	const std::string query = "пушистый и ухоженный кот";
	const auto documents = const_search_server.FindTopDocuments(query, DocumentStatus::ACTUAL);
	for (const Document& document : documents) {
		std::cout << "{ "
			<< "document_id = " << document.id << ", "
			<< "relevance = " << document.relevance << ", "
			<< "rating = " << document.rating << " }" << std::endl;
	}
}

//Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
	RUN_TEST(Test);
	RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
	RUN_TEST(TestAddDocument); // Тест проверяет, что поисковая система ищет документы только соответствующие поисковому запросу
	RUN_TEST(TestMinusWord); // Тест роверяет, что документы, содержащие минус-слова поискового запроса, не должны включаться в результаты поиска.
	RUN_TEST(TestMatchDocument); // Тест проверяет, что при матчинге документа по поисковому запросу должны быть возвращены все слова из поискового запроса, присутствующие в документе. Если есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой список слов.
	RUN_TEST(TestSortRelevance); // Тест проверяет на правильность сортировки по релевантности
	RUN_TEST(TestComputeAverageRating); // Тест проверяет на правильность высчитывания среднего рейтинга
	RUN_TEST(TestResultFilterPredicate); // Тест проверяет на фильтрацию результатов поиска с использованием предиката, задаваемого пользователем
	RUN_TEST(TestFindAllDocument);  // Тест проверяет на правильность поиска документов
	RUN_TEST(TestCorrectRelevance);  // Тест проверяет на правильность высчитывания релевантности
}
