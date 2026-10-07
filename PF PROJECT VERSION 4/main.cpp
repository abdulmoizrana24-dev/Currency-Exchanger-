#include "httplib.h"
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <map>
#include <vector>

using namespace std;
using namespace httplib;


map<string, float> currencyRates;



void fetchAPI() {
    system("curl -s \"https://v6.exchangerate-api.com/v6/305a7459221168512b498023/latest/USD\" -o api_rates.txt");
    ifstream file("api_rates.txt");
    if (!file) return;
    string content, line;
    while(getline(file, line)) content += line;
    file.close();

    size_t ratesPos = content.find("\"conversion_rates\":");
    if (ratesPos == string::npos) return;
    ratesPos += 18;
    size_t endBrace = content.find('}', ratesPos);
    string ratesStr = content.substr(ratesPos, endBrace - ratesPos);

    currencyRates.clear();
    stringstream ss(ratesStr);
    string pair;
    while(getline(ss, pair, ',')) {
        size_t colon = pair.find(':');
        if(colon != string::npos) {
            string key = pair.substr(0, colon);
            key.erase(remove(key.begin(), key.end(), '\"'), key.end());
            key.erase(remove(key.begin(), key.end(), ' '), key.end());
            string value = pair.substr(colon + 1);
            string cleanValue;
            for(char c : value) if(isdigit(c) || c == '.' || c == '-') cleanValue += c;
            if(!cleanValue.empty()) currencyRates[key] = stof(cleanValue);
        }
    }
    currencyRates["USD"] = 1.0f;
}

void saveHistory(const string &from, const string &to, float amount, float result) {
    ofstream file("history.txt", ios::app);
    file << fixed << setprecision(2) << amount << " " << from << " = " << result << " " << to << endl;
    file.close();
}


void processFromFile() {
    ifstream in("input.txt");
    ofstream out("output.txt");
    if (!in || !out) return;
    string from, to; float amount;
    if (in >> from >> to >> amount) {
        transform(from.begin(), from.end(), from.begin(), ::toupper);
        transform(to.begin(), to.end(), to.begin(), ::toupper);
        if (currencyRates.count(from) && currencyRates.count(to)) {
            float result = amount * (currencyRates[to] / currencyRates[from]);
            out << fixed << setprecision(2) << amount << " " << from << " = " << result << " " << to;
        } else { out << "Invalid currency"; }
    }
    in.close(); out.close();
}


int main() {
    Server svr;
    fetchAPI();

    
    svr.Get("/", [](const Request&, Response& res) {
        ifstream ifs("index.html");
        stringstream ss; ss << ifs.rdbuf();
        res.set_content(ss.str(), "text/html");
    });

    

svr.Get("/convert", [](const Request& req, Response& res) {
    // Add CORS headers so the browser allows the data through
    res.set_header("Access-Control-Allow-Origin", "*");

    if (req.has_param("amount") && req.has_param("from") && req.has_param("to")) {
        try {
            float amount = stof(req.get_param_value("amount"));
            string from = req.get_param_value("from");
            string to = req.get_param_value("to");

            
            transform(from.begin(), from.end(), from.begin(), ::toupper);
            transform(to.begin(), to.end(), to.begin(), ::toupper);

            if (currencyRates.count(from) && currencyRates.count(to)) {
                float result = amount * (currencyRates[to] / currencyRates[from]);
                
                
                saveHistory(from, to, amount, result);

            
                stringstream json;
                json << "{\"res\":" << fixed << setprecision(2) << result << "}";
                res.set_content(json.str(), "application/json");
            } else {
                res.set_content("{\"res\": 0, \"err\":\"Currency not found\"}", "application/json");
            }
        } catch (...) {
            res.set_content("{\"res\": 0, \"err\":\"Invalid input\"}", "application/json");
        }
    }
});

    
    svr.Get("/data", [](const Request&, Response& res) {
        stringstream json;
        json << "{\"rates\":{";
        for (auto it = currencyRates.begin(); it != currencyRates.end(); ++it) {
            json << "\"" << it->first << "\":" << it->second << (next(it) == currencyRates.end() ? "" : ",");
        }
        
        vector<pair<string, float>> list(currencyRates.begin(), currencyRates.end());
        sort(list.begin(), list.end(), [](auto& a, auto& b){ return a.second < b.second; });
        json << "}, \"strong\":[";
        for(int i=0; i<10; i++) json << "{\"c\":\"" << list[i].first << "\",\"r\":" << list[i].second << "}" << (i==9?"":",");
        
        sort(list.begin(), list.end(), [](auto& a, auto& b){ return a.second > b.second; });
        json << "], \"weak\":[";
        for(int i=0; i<10; i++) json << "{\"c\":\"" << list[i].first << "\",\"r\":" << list[i].second << "}" << (i==9?"":",");
        
        json << "]}";
        res.set_content(json.str(), "application/json");
    });

    // API: View History (
    svr.Get("/history", [](const Request&, Response& res) {
        ifstream file("history.txt");
        string content((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
        res.set_content(content, "text/plain");
    });

    // API: Process File 
    svr.Get("/process-file", [](const Request&, Response& res) {
        processFromFile();
        ifstream file("output.txt");
        string content; getline(file, content);
        res.set_content("{\"msg\":\"" + content + "\"}", "application/json");
    });

    cout << "SERVER WORKING ON http://localhost:8080" << endl;
    svr.listen("0.0.0.0", 8080);
    return 0;
}
