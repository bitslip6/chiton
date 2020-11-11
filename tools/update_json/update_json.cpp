#include <iostream>
#include <iomanip>
#include <fstream>
#include "../include/json.hpp"

using json = nlohmann::json;

int main()
{
    // create two JSON objects
    std::ifstream file_old("config.json");
    std::ifstream file_new("config1.json");
    
    json new_json = json::parse(file_new);
    json old_json = json::parse(file_old);

    old_json.update(new_json);

    std::cout << std::setw(2) << new_json << std::endl;
    std::cout << "==============================================" << std::endl;
    std::cout << std::setw(2) << old_json << std::endl;

    std::cout <<  "old_json size: " << old_json.size() << std::endl;
    std::cout <<  "new_json size: " << new_json.size() << std::endl;

    if (new_json == old_json)
    {
        std::cout << "properly converted" << std::endl;
    }
    else
    {
        std::cout << "conversion error" << std::endl;
    }
}