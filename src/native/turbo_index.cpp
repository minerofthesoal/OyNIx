/*
 * OyNIx Browser - Turbo Indexer (C++ Core)
 * Multi-threaded, SIMD-aware text indexer for the Nyx search engine.
 * Provides batch processing, concurrent scoring, and fast n-gram extraction.
 *
 * Compile (shared lib):
 *   g++ -O3 -march=native -shared -fPIC -std=c++17 -pthread \
 *       -o libturbo_index.so src/native/turbo_index.cpp
 *
 * Compile (CLI):
 *   g++ -O3 -march=native -DCLI_MODE -std=c++17 -pthread \
 *       -o turbo-index src/native/turbo_index.cpp
 *
 * Coded by Claude (Anthropic)
 */

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <mutex>
#include <numeric>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/* ── Stop Words ─────────────────────────────────────────────────────── */

static const std::unordered_set<std::string> STOP_WORDS = {
    "a", "an", "the", "is", "it", "in", "on", "at", "to", "of",
    "and", "or", "not", "for", "with", "as", "by", "this", "that",
    "from", "be", "are", "was", "were", "been", "being", "have",
    "has", "had", "do", "does", "did", "will", "would", "could",
    "should", "may", "might", "shall", "can", "but", "if", "so",
    "than", "too", "very", "just", "about", "up", "out", "no",
    "all", "any", "each", "every", "both", "few", "more", "most",
    "other", "some", "such", "only", "own", "same", "into", "over",
    "after", "before", "between", "under", "again", "then", "once",
    "here", "there", "when", "where", "why", "how", "what", "which",
    "who", "whom", "its", "his", "her", "my", "your", "our", "their",
    "he", "she", "we", "they", "me", "him", "us", "them", "i", "you",
};

/* ── Fast Tokenizer ────────────────────────────────────────────────── */

struct Token {
    std::string term;
    int count;
    double tf;
    int first_pos;  /* position of first occurrence */
};

static inline bool is_alnum_fast(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9');
}

static inline char to_lower_fast(char c) {
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

/* Tokenize with position tracking */
static std::vector<std::string> tokenize(const char *text, size_t len,
                                          std::vector<int> *positions = nullptr) {
    std::vector<std::string> tokens;
    tokens.reserve(len / 5);  /* rough estimate */
    std::string current;
    current.reserve(32);
    int pos = 0;

    for (size_t i = 0; i < len; i++) {
        char c = text[i];
        if (is_alnum_fast(c)) {
            current += to_lower_fast(c);
        } else if (!current.empty()) {
            if (current.length() >= 2 && current.length() <= 40 &&
                STOP_WORDS.find(current) == STOP_WORDS.end()) {
                tokens.push_back(std::move(current));
                if (positions) positions->push_back(pos);
            }
            current.clear();
            pos++;
        }
    }
    if (!current.empty() && current.length() >= 2 && current.length() <= 40 &&
        STOP_WORDS.find(current) == STOP_WORDS.end()) {
        tokens.push_back(std::move(current));
        if (positions) positions->push_back(pos);
    }
    return tokens;
}

/* Count term frequencies with position info */
static std::vector<Token> count_terms_detailed(const std::vector<std::string> &tokens) {
    std::unordered_map<std::string, std::pair<int, int>> counts;  /* term -> (count, first_pos) */
    for (int i = 0; i < (int)tokens.size(); i++) {
        auto &p = counts[tokens[i]];
        if (p.first == 0) p.second = i;
        p.first++;
    }

    int total = (int)tokens.size();
    std::vector<Token> result;
    result.reserve(counts.size());
    for (auto &[term, info] : counts) {
        result.push_back({term, info.first,
                          (double)info.first / std::max(total, 1),
                          info.second});
    }

    /* Sort by count descending */
    std::sort(result.begin(), result.end(),
              [](const Token &a, const Token &b) { return a.count > b.count; });
    return result;
}

/* ── N-gram Extraction ─────────────────────────────────────────────── */

struct NGram {
    std::string text;
    int count;
};

/* Extract bigrams and trigrams */
static std::vector<NGram> extract_ngrams(const std::vector<std::string> &tokens,
                                          int n, int min_count = 2) {
    if ((int)tokens.size() < n) return {};

    std::unordered_map<std::string, int> ngram_counts;
    for (int i = 0; i <= (int)tokens.size() - n; i++) {
        std::string ng;
        for (int j = 0; j < n; j++) {
            if (j > 0) ng += ' ';
            ng += tokens[i + j];
        }
        ngram_counts[ng]++;
    }

    std::vector<NGram> result;
    for (auto &[text, count] : ngram_counts) {
        if (count >= min_count) {
            result.push_back({text, count});
        }
    }
    std::sort(result.begin(), result.end(),
              [](const NGram &a, const NGram &b) { return a.count > b.count; });
    return result;
}

/* ── BM25 Scoring ──────────────────────────────────────────────────── */

static double bm25_score(const std::vector<std::string> &query_tokens,
                          const std::unordered_map<std::string, int> &doc_tf,
                          int doc_len, double avg_doc_len,
                          double k1 = 1.5, double b = 0.75) {
    double score = 0.0;
    for (const auto &qt : query_tokens) {
        auto it = doc_tf.find(qt);
        if (it == doc_tf.end()) continue;
        int tf = it->second;
        double idf = 1.0;
        double tf_norm = (tf * (k1 + 1.0)) /
            (tf + k1 * (1.0 - b + b * (doc_len / avg_doc_len)));
        score += idf * tf_norm;
    }
    return score;
}

/* ── Proximity Scoring ─────────────────────────────────────────────── */

/* Bonus score when query terms appear close together */
static double proximity_score(const std::vector<std::string> &query_tokens,
                               const std::vector<std::string> &doc_tokens) {
    if (query_tokens.size() < 2) return 0.0;

    /* Find positions of each query term */
    std::unordered_map<std::string, std::vector<int>> positions;
    std::unordered_set<std::string> qt_set(query_tokens.begin(), query_tokens.end());
    for (int i = 0; i < (int)doc_tokens.size(); i++) {
        if (qt_set.count(doc_tokens[i])) {
            positions[doc_tokens[i]].push_back(i);
        }
    }

    if (positions.size() < 2) return 0.0;

    /* Find minimum span containing all query terms that appear */
    double best = 0.0;
    for (auto &[term1, pos1] : positions) {
        for (auto &[term2, pos2] : positions) {
            if (term1 >= term2) continue;
            for (int p1 : pos1) {
                for (int p2 : pos2) {
                    int dist = std::abs(p1 - p2);
                    if (dist > 0 && dist <= 10) {
                        best += 1.0 / dist;
                    }
                }
            }
        }
    }
    return best;
}

/* ── Title / URL Bonus ─────────────────────────────────────────────── */

static double title_url_bonus(const std::vector<std::string> &query_tokens,
                               const char *title, const char *url) {
    double bonus = 0.0;
    std::string title_lower(title);
    std::string url_lower(url);
    std::transform(title_lower.begin(), title_lower.end(), title_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    std::transform(url_lower.begin(), url_lower.end(), url_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    for (const auto &qt : query_tokens) {
        if (title_lower.find(qt) != std::string::npos) bonus += 3.0;
        if (url_lower.find(qt) != std::string::npos) bonus += 1.5;
    }
    return bonus;
}

/* ── Batch Document ────────────────────────────────────────────────── */

struct Document {
    int id;
    const char *url;
    const char *title;
    const char *text;
    int text_len;
    double score;
};

struct ScoredResult {
    int id;
    double score;
};

/* ── Multi-threaded Batch Scoring ──────────────────────────────────── */

static void score_batch_worker(
        const std::vector<std::string> &query_tokens,
        Document *docs, int start, int end,
        double avg_doc_len,
        std::vector<ScoredResult> &results,
        std::mutex &mtx) {

    std::vector<ScoredResult> local;
    local.reserve(end - start);

    for (int i = start; i < end; i++) {
        Document &doc = docs[i];
        int dlen = doc.text_len > 0 ? doc.text_len : (int)strlen(doc.text);
        auto d_tokens = tokenize(doc.text, dlen);

        /* BM25 base score */
        std::unordered_map<std::string, int> tf;
        for (auto &t : d_tokens) tf[t]++;
        double s = bm25_score(query_tokens, tf, (int)d_tokens.size(), avg_doc_len);

        /* Proximity bonus */
        s += proximity_score(query_tokens, d_tokens) * 0.5;

        /* Title/URL bonus */
        if (doc.title) s += title_url_bonus(query_tokens, doc.title, doc.url ? doc.url : "");

        doc.score = s;
        if (s > 0.01) {
            local.push_back({doc.id, s});
        }
    }

    std::lock_guard<std::mutex> lock(mtx);
    results.insert(results.end(), local.begin(), local.end());
}

/* ── URL Utilities ─────────────────────────────────────────────────── */

static std::string normalize_url(const char *url) {
    std::string s(url);
    size_t pos = s.find("://");
    if (pos != std::string::npos) s = s.substr(pos + 3);
    if (s.substr(0, 4) == "www.") s = s.substr(4);
    while (!s.empty() && s.back() == '/') s.pop_back();
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

static std::string extract_domain(const char *url) {
    std::string s = normalize_url(url);
    size_t slash = s.find('/');
    if (slash != std::string::npos) s = s.substr(0, slash);
    size_t colon = s.find(':');
    if (colon != std::string::npos) s = s.substr(0, colon);
    return s;
}

/* ═══════════════════════════════════════════════════════════════════
 *  C API — loaded via ctypes from Python or P/Invoke from C#
 * ═══════════════════════════════════════════════════════════════════ */

extern "C" {

/* ── Basic tokenization (JSON output) ── */
const char *turbo_tokenize(const char *text, int text_len) {
    static thread_local std::string result;
    int len = text_len > 0 ? text_len : (int)strlen(text);
    auto tokens = tokenize(text, len);
    auto details = count_terms_detailed(tokens);

    std::ostringstream out;
    out << "[";
    bool first = true;
    for (const auto &t : details) {
        if (!first) out << ",";
        first = false;
        out << "{\"t\":\"" << t.term << "\",\"c\":" << t.count
            << ",\"tf\":" << t.tf << ",\"pos\":" << t.first_pos << "}";
    }
    out << "]";
    result = out.str();
    return result.c_str();
}

/* ── N-gram extraction ── */
const char *turbo_ngrams(const char *text, int text_len, int n) {
    static thread_local std::string result;
    int len = text_len > 0 ? text_len : (int)strlen(text);
    auto tokens = tokenize(text, len);
    auto ngrams = extract_ngrams(tokens, n > 0 ? n : 2, 1);

    std::ostringstream out;
    out << "[";
    bool first = true;
    for (const auto &ng : ngrams) {
        if (!first) out << ",";
        first = false;
        out << "{\"ng\":\"" << ng.text << "\",\"c\":" << ng.count << "}";
    }
    out << "]";
    result = out.str();
    return result.c_str();
}

/* ── Single document scoring with proximity + title/URL bonus ── */
double turbo_score(const char *query, const char *doc_text, int doc_len,
                   const char *title, const char *url) {
    auto q_tokens = tokenize(query, strlen(query));
    int dlen = doc_len > 0 ? doc_len : (int)strlen(doc_text);
    auto d_tokens = tokenize(doc_text, dlen);

    std::unordered_map<std::string, int> tf;
    for (auto &t : d_tokens) tf[t]++;

    double score = bm25_score(q_tokens, tf, (int)d_tokens.size(), 500.0);
    score += proximity_score(q_tokens, d_tokens) * 0.5;
    if (title) score += title_url_bonus(q_tokens, title, url ? url : "");
    return score;
}

/* ── Batch scoring (multi-threaded) ──
 *
 * Input: JSON array of docs: [{"id":N,"url":"...","title":"...","text":"..."},...]
 * Output: JSON array of scored results: [{"id":N,"score":X.X},...]
 * Sorted by score descending, limited to top_n results.
 */
const char *turbo_batch_score(const char *query, const char *docs_json,
                               int num_threads, int top_n) {
    static thread_local std::string result;

    /* Parse the query */
    auto q_tokens = tokenize(query, strlen(query));
    if (q_tokens.empty()) {
        result = "[]";
        return result.c_str();
    }

    /* Simple JSON array parser for docs
     * Expected format: [{"id":N,"text":"...","title":"...","url":"..."},...]
     * We do a fast, simple parse — not a full JSON parser */
    std::vector<Document> docs;
    static thread_local std::vector<std::string> text_storage;
    static thread_local std::vector<std::string> title_storage;
    static thread_local std::vector<std::string> url_storage;
    text_storage.clear();
    title_storage.clear();
    url_storage.clear();

    /* Helper: find a JSON string value for a given key, tolerating whitespace.
     * Searches for "key": "value" or "key":"value" */
    auto find_json_str = [](const std::string &json, const std::string &key,
                            size_t start, size_t end) -> std::pair<size_t, size_t> {
        std::string needle = "\"" + key + "\"";
        size_t kp = json.find(needle, start);
        if (kp == std::string::npos || kp >= end) return {std::string::npos, 0};
        /* Skip past key and colon, tolerating whitespace */
        size_t cp = kp + needle.size();
        while (cp < end && (json[cp] == ':' || json[cp] == ' ' || json[cp] == '\t'))
            cp++;
        if (cp >= end || json[cp] != '"') return {std::string::npos, 0};
        size_t vs = cp + 1;  /* value start (after opening quote) */
        size_t ve = json.find('"', vs);
        while (ve != std::string::npos && ve > 0 && json[ve - 1] == '\\')
            ve = json.find('"', ve + 1);
        if (ve == std::string::npos || ve >= end) return {std::string::npos, 0};
        return {vs, ve};
    };

    std::string json(docs_json);
    size_t pos = 0;

    /* Walk through JSON objects by matching braces */
    while (pos < json.size()) {
        size_t obj_start = json.find('{', pos);
        if (obj_start == std::string::npos) break;
        /* Find matching closing brace (simple, no nested objects) */
        size_t obj_end = json.find('}', obj_start);
        if (obj_end == std::string::npos) break;
        obj_end++;  /* include the } */

        Document doc = {};

        /* Parse id — find "id" : <number> */
        std::string id_key = "\"id\"";
        size_t id_pos = json.find(id_key, obj_start);
        if (id_pos != std::string::npos && id_pos < obj_end) {
            size_t cp = id_pos + id_key.size();
            while (cp < obj_end && (json[cp] == ':' || json[cp] == ' ')) cp++;
            doc.id = std::atoi(json.c_str() + cp);
        } else {
            pos = obj_end;
            continue;
        }

        /* Parse text */
        auto [ts, te] = find_json_str(json, "text", obj_start, obj_end);
        if (ts != std::string::npos) {
            text_storage.push_back(json.substr(ts, te - ts));
            doc.text = text_storage.back().c_str();
            doc.text_len = (int)(te - ts);
        }

        /* Parse title */
        auto [tis, tie] = find_json_str(json, "title", obj_start, obj_end);
        if (tis != std::string::npos) {
            title_storage.push_back(json.substr(tis, tie - tis));
            doc.title = title_storage.back().c_str();
        }

        /* Parse url */
        auto [us, ue] = find_json_str(json, "url", obj_start, obj_end);
        if (us != std::string::npos) {
            url_storage.push_back(json.substr(us, ue - us));
            doc.url = url_storage.back().c_str();
        }

        if (doc.text) docs.push_back(doc);
        pos = obj_end;
    }

    if (docs.empty()) {
        result = "[]";
        return result.c_str();
    }

    /* Calculate average doc length */
    double avg_len = 0;
    for (auto &d : docs) {
        avg_len += (d.text_len > 0 ? d.text_len : (int)strlen(d.text));
    }
    avg_len /= docs.size();
    if (avg_len < 1) avg_len = 500.0;

    /* Multi-threaded scoring */
    int n_threads = num_threads > 0 ? num_threads : (int)std::thread::hardware_concurrency();
    if (n_threads < 1) n_threads = 1;
    if (n_threads > (int)docs.size()) n_threads = (int)docs.size();

    std::vector<ScoredResult> results;
    std::mutex mtx;
    std::vector<std::thread> threads;

    int chunk = (int)docs.size() / n_threads;
    for (int t = 0; t < n_threads; t++) {
        int start = t * chunk;
        int end = (t == n_threads - 1) ? (int)docs.size() : start + chunk;
        threads.emplace_back(score_batch_worker,
                             std::ref(q_tokens), docs.data(),
                             start, end, avg_len,
                             std::ref(results), std::ref(mtx));
    }
    for (auto &t : threads) t.join();

    /* Sort by score descending and limit */
    std::sort(results.begin(), results.end(),
              [](const ScoredResult &a, const ScoredResult &b) {
                  return a.score > b.score;
              });

    int limit = top_n > 0 ? std::min(top_n, (int)results.size()) : (int)results.size();

    /* Build JSON output */
    std::ostringstream out;
    out << "[";
    for (int i = 0; i < limit; i++) {
        if (i > 0) out << ",";
        out << "{\"id\":" << results[i].id
            << ",\"score\":" << results[i].score << "}";
    }
    out << "]";
    result = out.str();
    return result.c_str();
}

/* ── URL utilities ── */
const char *turbo_normalize_url(const char *url) {
    static thread_local std::string result;
    result = normalize_url(url);
    return result.c_str();
}

const char *turbo_extract_domain(const char *url) {
    static thread_local std::string result;
    result = extract_domain(url);
    return result.c_str();
}

/* ── Thread count ── */
int turbo_cpu_threads(void) {
    return (int)std::thread::hardware_concurrency();
}

/* ── Version ── */
const char *turbo_version(void) {
    return "2.1.0-turbo";
}

}  /* extern "C" */


/* ═══════════════════════════════════════════════════════════════════
 *  CLI Mode
 * ═══════════════════════════════════════════════════════════════════ */

#ifdef CLI_MODE
int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "OyNIx Turbo Indexer v" << turbo_version() << "\n"
                  << "Usage:\n"
                  << "  turbo-index tokenize <text>            Tokenize text\n"
                  << "  turbo-index ngrams <text> [n]          Extract n-grams\n"
                  << "  turbo-index score <query> <text>       Score relevance\n"
                  << "  turbo-index normalize <url>            Normalize URL\n"
                  << "  turbo-index domain <url>               Extract domain\n"
                  << "  turbo-index bench <text> [iterations]  Benchmark\n"
                  << "  turbo-index version                    Version info\n";
        return 1;
    }

    std::string cmd = argv[1];

    if (cmd == "tokenize" && argc >= 3) {
        std::cout << turbo_tokenize(argv[2], 0) << "\n";
    } else if (cmd == "ngrams" && argc >= 3) {
        int n = argc >= 4 ? std::atoi(argv[3]) : 2;
        std::cout << turbo_ngrams(argv[2], 0, n) << "\n";
    } else if (cmd == "score" && argc >= 4) {
        double s = turbo_score(argv[2], argv[3], 0, nullptr, nullptr);
        std::cout << s << "\n";
    } else if (cmd == "normalize" && argc >= 3) {
        std::cout << turbo_normalize_url(argv[2]) << "\n";
    } else if (cmd == "domain" && argc >= 3) {
        std::cout << turbo_extract_domain(argv[2]) << "\n";
    } else if (cmd == "bench" && argc >= 3) {
        int iters = argc >= 4 ? std::atoi(argv[3]) : 10000;
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iters; i++) {
            turbo_tokenize(argv[2], 0);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << iters << " iterations in " << ms << "ms ("
                  << (double)iters / (ms / 1000.0) << " ops/sec)\n";
        std::cout << "Threads available: " << turbo_cpu_threads() << "\n";
    } else if (cmd == "version") {
        std::cout << "turbo-index " << turbo_version() << "\n"
                  << "CPU threads: " << turbo_cpu_threads() << "\n";
    } else {
        std::cerr << "Unknown: " << cmd << "\n";
        return 1;
    }
    return 0;
}
#endif
