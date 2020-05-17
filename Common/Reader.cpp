#include "Reader.h"

int Reader::splitLine(string& line, vector<string>& vec, int n) {
    int i = 0;
    std::regex r("\\S+");
    std::sregex_iterator s;
    for (s = std::sregex_iterator(line.begin(), line.end(), r);
                                        i < n && s != std::sregex_iterator();
                                        s++, i++) {
        string str = s -> str();
        if (i != n - 1) {
            if (str.length() < 2 || str[str.length() - 1] != ',') { return false; }
            str.pop_back();
        }
        vec[i] = str;
    }

    return (i == n) && (s == std::sregex_iterator());
}

bool Reader::convertVectorToInt(vector<int>& int_vec, vector<string>& str_vec) {
    try {
        for (unsigned long long i = 0; i < str_vec.size(); i++) {
            int number = stoi(str_vec[i]);
            int_vec[i] = number;
        }
    }
    catch (std::invalid_argument const &e) {
        return false;
    }
    return true;
}

bool Reader::ignoreLine(string& line) {
    std::regex format("^#.*|^\\s*$");
    return std::regex_match(line, format);
}

int Reader::splitCargoLine(string& line, string& id, int& weight, string& destination) {
    vector<string> vec(3);
    int errors = 0;
    splitLine(line, vec, 3);
    id = vec[0];
    if (id == "") { errors |= pow2(14);}
    else if (!legalContainerId(id)) { errors |= pow2(15); } //TODO: changed
    try {
        weight = stoi(vec[1]);
        if (weight < 0) { errors |= pow2(12); weight = -1; }
    }
    catch (std::invalid_argument const &e) { errors |= pow2(12); weight = -1; }
    destination = vec[2];
    if (!legalPortSymbol(destination)) { errors |= pow2(13); destination = ""; }
    return errors;
}

bool Reader::splitPlanLine(string& line, vector<int>& vec) {
    vector<string> str_vec(3);
    if(!splitLine(line, str_vec, 3)) {
        return false;
    }
    return convertVectorToInt(vec, str_vec) && vec[0] >= 0 && vec[1] >= 0 && vec[2] >= 0;
}

bool Reader::splitInstructionLine(string& line, char& op, string& id, int& floor, int& x, int& y) {
    vector<string> str_vec(5);
    if (!splitLine(line, str_vec, 5)) { return false; }
    if (str_vec[0] != "L" && str_vec[0] != "U" && str_vec[0] != "R") { return false; }
    op = str_vec[0][0];
    //if(!legalContainerId(str_vec[1])) { return false; } // TODO: changed
    id = str_vec[1];
    vector<int> int_vec(3);
    vector<string> sub_str_vec;
    std::copy(str_vec.begin() + 2, str_vec.end(), std::back_inserter(sub_str_vec));
    if (!convertVectorToInt(int_vec, sub_str_vec)) { return false; }
    floor = int_vec[0];
    x = int_vec[1];
    y = int_vec[2];
    return true;
}

bool Reader::legalPortSymbol(const string& symbol) {
    std::regex format("^[A-Z]{5}$");
    return std::regex_match(symbol, format);
}

bool Reader::legalContainerId(const string& id) {
    std::regex format("^[A-Z]{3}[UJZ][0-9]{7}$");
    return std::regex_match(id, format) && legalCheckDigit(id);
}

bool Reader::legalCheckDigit(const string& id) {
    int sum = 0, i = 0, temp;
    for(char ch : id) {
        if (i <= 3) {
            int add = static_cast<int>(floor((double) (ch - 'A' + 10) / 11));
            temp = ch - 'A' + 10 + add;
        }
        else if (i < 10){
            temp = ch - '0';
        }
        else { break; }
        sum += (int)pow(2, i) * temp;
        i++;
    }
    sum -= 11 * (int)floor((double)sum / 11);
    return sum % 10 == (id[10] - '0');
}

int Reader::readCargoLoad(const string &path, vector<unique_ptr<Container>>& list) {
    //TODO: if weight or port don't exist, put -1/"". these are the cases to handle:
//    2^12 - containers at port: bad line format, missing or bad weight (ID rejected) - put -1
//    2^13 - containers at port: bad line format, missing or bad port dest (ID rejected) - put ""
//    2^14 - containers at port: bad line format, ID cannot be read (ignored) - put ""
//    2^16 - containers at port: file cannot be read altogether (assuming no cargo to be loaded at this port) - return false;
    vector<unique_ptr<Container>> cargo;
    std::string line, destination,  id;
    int weight, errors = 0;
    fs::path file_path = path;
    if(path.empty() || !fs::exists(file_path)) { return  pow2(16); }
    std::ifstream file(path);
    while (std::getline(file, line)) {
        if (ignoreLine(line)) { continue; }
        errors |= splitCargoLine(line, id, weight, destination);
        unique_ptr<Container> container = make_unique<Container>(weight, destination, id);
        list.emplace_back(std::move(container));
    }
    return errors;
}

int Reader::readShipPlan(const string& path, ShipPlan& plan) {
    int errors = 0, x, x1, y, y1, num_floors, num_floors1;
    fs::path file_path = path;
    if(path.empty() || !fs::exists(file_path)) { return  pow2(3); }
    std::string line; std::ifstream file(path); // todo: file_path or path?
    if (!file || file.peek() == std::ifstream::traits_type::eof()) { return pow2(3); }
    vector<int> vec(3);
    do {
        if (!std::getline(file, line) || !Reader::splitPlanLine(line, vec)) {
            return pow2(3);
        }
    }
    while (ignoreLine(line));
    num_floors = vec[0]; x = vec[1]; y = vec[2];
    bool fatal = false;
    map< pair<int,int>, int > m_plan;
    while (std::getline(file, line)) {
        if (ignoreLine(line)) { continue; }
        if(!Reader::splitPlanLine(line, vec)) { // wrong format
            errors |= pow2(2);
            continue;
        }
        x1 = vec[0]; y1 = vec[1]; num_floors1 = vec[2];
        if (x < x1 || y < y1 || num_floors <= num_floors1) { // wrong values
            errors |= pow2(2);
            continue;
        }
        if (m_plan.find({x1, y1}) != m_plan.end()) { // duplicate x,y appearance
            if (m_plan[{x1, y1}] == num_floors1) { // same data
                errors |= pow2(2);
            }
            else { // different data
                errors |= pow2(4);
                fatal = true;
            }
            continue; // todo: or just break? do we need to find ALL errors?
        }
        m_plan[{x1, y1}] = num_floors1;
    }
    if (!fatal) {
        plan = ShipPlan(num_floors, std::move(m_plan)); // why not?
    }
    return errors;
}

int Reader::readShipRoute(const string &path, ShipRoute& route) {
    fs::path file_path = path;
    if(path.empty() || !fs::exists(file_path)) {
        return pow2(7);
    }
    int errors = 0;
    string curr_port, prev_port;
    vector<string> ports;
    std::ifstream file(path);
    if (!file || file.peek() == std::ifstream::traits_type::eof()) { return pow2(7); }
    while (std::getline(file, curr_port)) {
        if (ignoreLine(curr_port)) { continue; }
        if (curr_port == prev_port) {
            errors |= pow2(5);
            continue;
        }
        if(!Reader::legalPortSymbol(curr_port)) {
            errors |= pow2(6);
            continue;
        }
        ports.emplace_back(curr_port);
        prev_port = curr_port;
    }
    if (ports.size() == 1) { errors |= pow2(8); }
    else { route = ShipRoute(ports); }
    return errors;
}

bool Reader::checkDirPath(const string& pathName) {
    fs::path path = pathName;
    return fs::is_directory(path);
}

vector<string> Reader::getTravels(const string &dir) {
    vector<string> travels;
    std::regex format("(.*)_travel");
    for(const auto & entry : fs::directory_iterator(dir)) {
        if(std::regex_match(entry.path().stem().string(), format)) {
            travels.emplace_back(entry.path().stem().string());
        }
    }
    return travels;
}

vector<Operation> Reader::getInstructionsVector(const string &path) {
    vector<Operation> ops;
    char op_char;
    int floor, x, y;
    string line, id;
    std::ifstream file(path);
    while (std::getline(file, line)) {
        if (Reader::ignoreLine(line)) { continue; }
        if (Reader::splitInstructionLine(line, op_char, id, floor, x, y)) {
            Operation op = Operation(op_char, id, Position(floor, x, y));
            ops.push_back(op);
        }
    }
    return ops;
}