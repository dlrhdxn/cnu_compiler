#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "parser/json.hpp"

using namespace std;
using json = nlohmann::json;

bool compareLines(const string& expectedPath, const string& actualPath) {
    ifstream expectedFile(expectedPath);
    ifstream actualFile(actualPath);
    if (!expectedFile) {
        cerr << "Cannot open expected file: " << expectedPath << '\n';
        return false;
    }
    if (!actualFile) {
        cerr << "Cannot open actual file: " << actualPath << '\n';
        return false;
    }

    string expectedLine;
    string actualLine;
    size_t lineNumber = 0;
    size_t differences = 0;

    while (true) {
        const bool hasExpected = static_cast<bool>(getline(expectedFile, expectedLine));
        const bool hasActual = static_cast<bool>(getline(actualFile, actualLine));
        if (!hasExpected && !hasActual) break;

        ++lineNumber;
        if (!hasExpected || !hasActual || expectedLine != actualLine) {
            ++differences;
            cout << "Line " << lineNumber << " differs:\n"
                 << "  expected: " << (hasExpected ? expectedLine : "<missing>") << '\n'
                 << "  actual:   " << (hasActual ? actualLine : "<missing>") << '\n';
        }
    }

    if (differences == 0) {
        cout << "MATCH (" << lineNumber << " lines)\n";
        return true;
    }

    cout << differences << " differing line(s)\n";
    return false;
}

void reportJsonDifferences(const json& expected, const json& actual,
                           const string& path, size_t& differences) {
    if (expected.type() != actual.type()) {
        cout << path << ": expected " << expected.dump()
             << ", actual " << actual.dump() << '\n';
        ++differences;
        return;
    }

    if (expected.is_object()) {
        set<string> keys;
        for (auto it = expected.begin(); it != expected.end(); ++it) keys.insert(it.key());
        for (auto it = actual.begin(); it != actual.end(); ++it) keys.insert(it.key());

        for (const string& key : keys) {
            const string childPath = path + "." + key;
            const auto expectedIt = expected.find(key);
            const auto actualIt = actual.find(key);
            if (expectedIt == expected.end()) {
                cout << childPath << ": expected <missing>, actual "
                     << actualIt->dump() << '\n';
                ++differences;
            } else if (actualIt == actual.end()) {
                cout << childPath << ": expected " << expectedIt->dump()
                     << ", actual <missing>\n";
                ++differences;
            } else {
                reportJsonDifferences(*expectedIt, *actualIt, childPath, differences);
            }
        }
        return;
    }

    if (expected.is_array()) {
        const size_t commonSize = min(expected.size(), actual.size());
        for (size_t i = 0; i < commonSize; ++i) {
            reportJsonDifferences(expected[i], actual[i],
                                  path + "[" + to_string(i) + "]", differences);
        }
        for (size_t i = commonSize; i < expected.size(); ++i) {
            cout << path << "[" << i << "]: expected " << expected[i].dump()
                 << ", actual <missing>\n";
            ++differences;
        }
        for (size_t i = commonSize; i < actual.size(); ++i) {
            cout << path << "[" << i << "]: expected <missing>, actual "
                 << actual[i].dump() << '\n';
            ++differences;
        }
        return;
    }

    if (expected != actual) {
        cout << path << ": expected " << expected.dump()
             << ", actual " << actual.dump() << '\n';
        ++differences;
    }
}

bool compareJson(const string& expectedPath, const string& actualPath) {
    ifstream expectedFile(expectedPath);
    ifstream actualFile(actualPath);
    if (!expectedFile) {
        cerr << "Cannot open expected file: " << expectedPath << '\n';
        return false;
    }
    if (!actualFile) {
        cerr << "Cannot open actual file: " << actualPath << '\n';
        return false;
    }

    json expected;
    json actual;
    try {
        expectedFile >> expected;
        actualFile >> actual;
    } catch (const json::parse_error& error) {
        cerr << "Invalid JSON: " << error.what() << '\n';
        return false;
    }

    size_t differences = 0;
    reportJsonDifferences(expected, actual, "$", differences);
    if (differences == 0) {
        cout << "MATCH (JSON values are equal)\n";
        return true;
    }

    cout << differences << " JSON difference(s)\n";
    return false;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        cerr << "Usage: " << argv[0]
             << " <token|log|json> <expected-file> <actual-file>\n";
        return 2;
    }

    const string mode = argv[1];
    bool matched;
    if (mode == "token" || mode == "log") {
        matched = compareLines(argv[2], argv[3]);
    } else if (mode == "json") {
        matched = compareJson(argv[2], argv[3]);
    } else {
        cerr << "Unknown comparison mode: " << mode << '\n'
             << "Use token, log, or json.\n";
        return 2;
    }

    return matched ? 0 : 1;
}
