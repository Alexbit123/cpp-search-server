#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <numeric>

constexpr auto EPS = 1e-6;
using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
	string s;
	getline(cin, s);
	return s;
}

int ReadLineWithNumber() {
	int result;
	cin >> result;
	ReadLine();
	return result;
}

vector<string> SplitIntoWords(const string& text) {
	vector<string> words;
	string word;
	for (const char c : text) {
		if (c == ' ') {
			if (!word.empty()) {
				words.push_back(word);
				word.clear();
			}
		}
		else {
			word += c;
		}
	}
	if (!word.empty()) {
		words.push_back(word);
	}

	return words;
}

struct Document {
	int id;
	double relevance;
	int rating;
};

enum class DocumentStatus {
	ACTUAL,
	IRRELEVANT,
	BANNED,
	REMOVED,
};

class SearchServer {
public:
	void SetStopWords(const string& text) {
		for (const string& word : SplitIntoWords(text)) {
			stop_words_.insert(word);
		}
	}

	void AddDocument(int document_id, const string& document, DocumentStatus status,
		const vector<int>& ratings) {
		const vector<string> words = SplitIntoWordsNoStop(document);
		const double inv_word_count = 1.0 / words.size();
		for (const string& word : words) {
			word_to_document_freqs_[word][document_id] += inv_word_count;
		}
		documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
	}

	vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus document_status) const {
		return FindTopDocuments(raw_query, [document_status](int document_id, DocumentStatus status, int rating) {return status == document_status; });

	}

	template<typename DocumentPredicate>
	vector<Document> FindTopDocuments(const string& raw_query, const DocumentPredicate document_predicate) const {
		const Query query = ParseQuery(raw_query);
		auto matched_documents = FindAllDocuments(query, document_predicate);
		sort(matched_documents.begin(), matched_documents.end(),
			[](const auto& lhs, const auto& rhs) {
				if (abs(lhs.relevance - rhs.relevance) < EPS) {
					return lhs.rating > rhs.rating;
				}
				else {
					return lhs.relevance > rhs.relevance;
				}
			});
		if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
			matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
		}
		return matched_documents;
	}

	vector<Document> FindTopDocuments(const string& raw_query) const {
		return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
	}

	int GetDocumentCount() const {
		return documents_.size();
	}

	tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
		int document_id) const {
		const Query query = ParseQuery(raw_query);
		vector<string> matched_words;
		for (const string& word : query.plus_words) {
			if (word_to_document_freqs_.count(word) == 0) {
				continue;
			}
			if (word_to_document_freqs_.at(word).count(document_id)) {
				matched_words.push_back(word);
			}
		}
		for (const string& word : query.minus_words) {
			if (word_to_document_freqs_.count(word) == 0) {
				continue;
			}
			if (word_to_document_freqs_.at(word).count(document_id)) {
				matched_words.clear();
				break;
			}
		}
		return { matched_words, documents_.at(document_id).status };
	}

private:
	struct DocumentData {
		int rating;
		DocumentStatus status;
	};

	set<string> stop_words_;
	map<string, map<int, double>> word_to_document_freqs_;
	map<int, DocumentData> documents_;

	bool IsStopWord(const string& word) const {
		return stop_words_.count(word) > 0;
	}

	vector<string> SplitIntoWordsNoStop(const string& text) const {
		vector<string> words;
		for (const string& word : SplitIntoWords(text)) {
			if (!IsStopWord(word)) {
				words.push_back(word);
			}
		}
		return words;
	}

	static int ComputeAverageRating(const vector<int>& ratings) {
		if (ratings.empty()) {
			return 0;
		}
		return accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
	}

	struct QueryWord {
		string data;
		bool is_minus;
		bool is_stop;
	};

	QueryWord ParseQueryWord(string text) const {
		bool is_minus = false;
		// Word shouldn't be empty
		if (text[0] == '-') {
			is_minus = true;
			text = text.substr(1);
		}
		return { text, is_minus, IsStopWord(text) };
	}

	struct Query {
		set<string> plus_words;
		set<string> minus_words;
	};

	Query ParseQuery(const string& text) const {
		Query query;
		for (const string& word : SplitIntoWords(text)) {
			const QueryWord query_word = ParseQueryWord(word);
			if (!query_word.is_stop) {
				if (query_word.is_minus) {
					query.minus_words.insert(query_word.data);
				}
				else {
					query.plus_words.insert(query_word.data);
				}
			}
		}
		return query;
	}

	// Existence required
	double ComputeWordInverseDocumentFreq(const string& word) const {
		return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
	}

	template<typename DocumentPredicate>
	vector<Document> FindAllDocuments(const Query& query, const DocumentPredicate document_predicate) const {
		map<int, double> document_to_relevance;
		for (const string& word : query.plus_words) {
			if (word_to_document_freqs_.count(word) == 0) {
				continue;
			}
			const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
			for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
				if (document_predicate(document_id, documents_.at(document_id).status, documents_.at(document_id).rating))
				{
					document_to_relevance[document_id] += term_freq * inverse_document_freq;
				}
			}
		}

		for (const string& word : query.minus_words) {
			if (word_to_document_freqs_.count(word) == 0) {
				continue;
			}
			for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
				document_to_relevance.erase(document_id);
			}
		}

		vector<Document> matched_documents;
		for (const auto [document_id, relevance] : document_to_relevance) {
			matched_documents.push_back(
				{ document_id, relevance, documents_.at(document_id).rating });
		}
		return matched_documents;
	}
};


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

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
	const int doc_id = 42;
	const string content = "cat in the city"s;
	const vector<int> ratings = { 1, 2, 3 };
	{
		SearchServer server;
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		const auto found_docs = server.FindTopDocuments("in"s);
		ASSERT_EQUAL(found_docs.size(), 1u);
		const Document& doc0 = found_docs[0];
		ASSERT_EQUAL(doc0.id, doc_id);
	}

	{
		SearchServer server;
		server.SetStopWords("in the"s);
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
		SearchServer server;
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
		SearchServer server;
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

	{
		SearchServer server;
		server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings);

		const auto [found_words1, found_status1] = server.MatchDocument("cat dog big"s, doc_id1);
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
	const string content1 = "cat in the city"s;
	const string content2 = "city is big"s;
	const string content3 = "dog is beautiful a color the best"s;
	const vector<int> ratings = { 1, 2, 3 };

	{
		SearchServer server;
		server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings);

		const auto found_docs = server.FindTopDocuments("city"s);
		ASSERT_EQUAL(found_docs.size(), 2);
		const Document& doc0 = found_docs[0];
		const Document& doc1 = found_docs[1];
		ASSERT(doc0.relevance > doc1.relevance);
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
		SearchServer server;
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
		SearchServer server;
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
		SearchServer server;
		server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(doc_id2, content2, DocumentStatus::BANNED, ratings);
		server.AddDocument(doc_id3, content3, DocumentStatus::IRRELEVANT, ratings);
		const auto found_docs = server.FindTopDocuments("city"s);
		ASSERT_EQUAL_HINT(found_docs.size(), 1, "All documents with an odd id must be excluded");
		const Document& doc0 = found_docs[0];
		ASSERT(doc0.id == doc_id1);
	}

	{
		SearchServer server;
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
		SearchServer server;
		server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(doc_id2, content2, DocumentStatus::BANNED, ratings);
		server.AddDocument(doc_id3, content3, DocumentStatus::IRRELEVANT, ratings);
		const auto found_docs = server.FindTopDocuments("city"s, DocumentStatus::BANNED);
		ASSERT(found_docs.size() == 1);
		const Document& doc0 = found_docs[0];
		ASSERT_EQUAL(doc0.id, doc_id2);
	}

	{
		SearchServer server;
		server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(doc_id2, content2, DocumentStatus::BANNED, ratings);
		server.AddDocument(doc_id3, content3, DocumentStatus::IRRELEVANT, ratings);
		const auto found_docs = server.FindTopDocuments("city"s, DocumentStatus::ACTUAL);
		ASSERT(found_docs.size() == 1);
		const Document& doc0 = found_docs[0];
		ASSERT_EQUAL(doc0.id, doc_id1);
	}

	{
		SearchServer server;
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
		SearchServer server;
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

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
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

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
	TestSearchServer();
	// Если вы видите эту строку, значит все тесты прошли успешно
	cout << "Search server testing finished"s << endl;
}