#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <algorithm>
#include "../include/json.hpp"

using json = nlohmann::ordered_json;
//TODO: make template
bool check_vector_same_content(std::vector<std::string> v1, std::vector<std::string> v2)
{
    return std::all_of(v1.begin(), v1.end(), [v2](std::string v1_element) {
        return std::find(v2.begin(), v2.end(), v1_element) != v2.end();
    });
}

int main()
{
    // create two JSON objects
    std::ifstream file_old("config.json");
    std::ifstream file_new("config1.json");
    std::ofstream file_updated("config_updated.json");
    std::ofstream file_middle("config_middle.json");

    json new_json = json::parse(file_new);
    json old_json = json::parse(file_old);
    json temp_json = old_json;

    old_json.update(new_json);
    old_json.update(temp_json);

    file_middle << std::setw(4) << old_json << std::endl;

    std::cout << "================ ==============================" << std::endl;
    for (json::iterator it = old_json.begin(); it != old_json.end(); ++it)
    {
        std::cout << "old_json ================ " << std::endl;
        std::cout << it.key() << " " << it.value() << std::endl;
        std::cout << "old_json ================ " << std::endl;
        if (new_json[it.key()].size() != it.value().size())
        {
            json temp = it.value();
            json temp2 = new_json[it.key()];
            std::cout << "temp ================ " << std::endl;
            std::cout << std::setw(2) << temp << std::endl;
            std::cout << "temp2 ================" << std::endl;
            std::cout << std::setw(2) << temp2 << std::endl;
            std::vector<std::string> v_old;
            std::vector<std::string> v_new;

            for (int i = 0; i < temp2.size(); i++)
            {
                for (json::iterator it3 = temp2.at(i).begin(); it3 != temp2.at(i).end(); ++it3)
                {
                    std::cout << "it3: " << (*it3) << std::endl;
                    v_new.push_back(std::string(*it3));
                    break;
                }
            }

            for (int i = 0; i < temp.size(); i++)
            {
                for (json::iterator it4 = temp.at(i).begin(); it4 != temp.at(i).end(); ++it4)
                {
                    std::cout << "it4: " << (*it4) << std::endl;
                    v_old.push_back(std::string(*it4));
                    break;
                }
            }

            for (auto str : v_old)
            {
                std::cout << "v_old " << str << std::endl;
            }

            for (auto str : v_new)
            {
                std::cout << "v_new " << str << std::endl;
            }

            //TODO: sort the original vector
            std::vector<std::string> sortedV1(v_old);
            std::vector<std::string> sortedV2(v_new);

            sort(sortedV1.begin(), sortedV1.end());
            sort(sortedV2.begin(), sortedV2.end());

            std::vector<std::string> diff;

            set_difference(
                sortedV2.begin(),
                sortedV2.end(),
                sortedV1.begin(),
                sortedV1.end(),
                back_inserter(diff));

            for (auto str : diff)
                std::cout << "--- diff " << str << std::endl;

            std::vector<std::string> new_fields;
            std::vector<int> found_indexes;

            for (auto str : diff)
            {
                std::cout << "--- str " << str << std::endl;
                new_fields.emplace_back(str);
                std::vector<std::string>::iterator it_index = std::find(v_new.begin(), v_new.end(), str);
                int index = std::distance(v_new.begin(), it_index);
                found_indexes.emplace_back(index);

                std::cout << "index " << index << std::endl;
                std::cout << "temp2.at(i) " << temp2.at(index) << std::endl;
                temp.insert(temp.end(), temp2.at(index));
                std::cout << std::setw(2) << "temp updated " << temp << std::endl;
                std::cout << "--- str " << str << std::endl;
            }

            std::string key_str = it.key();
            old_json[key_str] = temp;
        }
        else if (new_json[it.key()].size() > 1)
        {
            std::vector<std::string> v_old;
            std::vector<std::string> v_new;

            for (int i = 0; i < new_json[it.key()].size(); i++)
            {
                for (json::iterator it3 = new_json[it.key()].at(i).begin(); it3 != new_json[it.key()].at(i).end(); ++it3)
                {
                    std::cout << "it3e: " << (*it3) << std::endl;
                    v_new.push_back(std::string(*it3));
                    break;
                }
            }

            for (int i = 0; i < it.value().size(); i++)
            {
                for (json::iterator it4 = it.value().at(i).begin(); it4 != it.value().at(i).end(); ++it4)
                {
                    std::cout << "it4e: " << (*it4) << std::endl;
                    v_old.push_back(std::string(*it4));
                    break;
                }
            }

            if (!check_vector_same_content(v_old, v_new))
            {
                std::cout << "Vectors have same size but are not equal!" << std::endl;

                std::vector<std::string> diff;

                sort(v_old.begin(), v_old.end());
                sort(v_new.begin(), v_new.end());

                set_difference(
                    v_new.begin(),
                    v_new.end(),
                    v_old.begin(),
                    v_old.end(),
                    back_inserter(diff));

                for (auto str : diff)
                    std::cout << "--- diff " << str << std::endl;

                for (auto str : diff)
                {
                    std::cout << "--- str " << str << std::endl;
                    std::vector<std::string>::iterator it_index = std::find(v_new.begin(), v_new.end(), str);
                    int index = std::distance(v_new.begin(), it_index);

                    std::cout << "index " << index << std::endl;
                    std::cout << "temp2.at(i) " << new_json[it.key()].at(index) << std::endl;
                    it.value().insert(it.value().end(), new_json[it.key()].at(index));
                    std::cout << std::setw(2) << "temp updated " << it.value() << std::endl;
                    std::cout << "--- str " << str << std::endl;
                }

                std::string key_str = it.key();
                old_json[key_str] = it.value();
            }
        }
    }

    std::cout << "================ ==============================" << std::endl;

    std::cout << "================ new_json ==============================" << std::endl;
    std::cout << std::setw(2) << new_json << std::endl;
    std::cout << "================ old_json ==============================" << std::endl;
    std::cout << std::setw(2) << old_json << std::endl;

    file_updated << std::setw(4) << old_json << std::endl;

    std::cout << "old_json size: " << old_json.size() << std::endl;
    std::cout << "new_json size: " << new_json.size() << std::endl;
}