#include "smarttype/corrector.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: smarttype-eval <corpus.tsv>\n";
        return 2;
    }
    std::ifstream input(argv[1]);
    if (!input) {
        std::cerr << "cannot open " << argv[1] << '\n';
        return 2;
    }

    smarttype::Corrector corrector;
    std::size_t total = 0, passed = 0;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line.front() == '#') continue;
        std::istringstream row(line);
        std::string context, typed, expected, expected_action;
        if (!std::getline(row, context, '\t') || !std::getline(row, typed, '\t') ||
            !std::getline(row, expected, '\t') || !std::getline(row, expected_action)) {
            std::cerr << "invalid TSV row: " << line << '\n';
            return 2;
        }
        std::vector<std::string> words;
        std::istringstream context_stream(context);
        for (std::string word; context_stream >> word;) words.push_back(word);
        const auto decision = corrector.decide(typed, words);
        const bool ok = decision.candidate == expected && smarttype::action_name(decision.action) == expected_action;
        ++total;
        passed += ok;
        std::cout << (ok ? "PASS" : "FAIL") << '\t' << typed << '\t'
                  << smarttype::action_name(decision.action) << '\t' << decision.candidate
                  << '\t' << std::fixed << std::setprecision(2) << decision.confidence << '\n';
    }
    std::cout << "score " << passed << '/' << total << '\n';
    return passed == total ? 0 : 1;
}

