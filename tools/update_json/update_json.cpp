#include <iostream>
#include <cstdlib>
#include <iomanip>
#include <fstream>
#include <vector>
#include <algorithm>
#include "../include/json.hpp"

using json = nlohmann::ordered_json;
//TODO: make it a template
bool check_vector_same_content(std::vector<std::string> v1, std::vector<std::string> v2)
{
    return std::all_of(v1.begin(), v1.end(), [v2](std::string v1_element) {
        return std::find(v2.begin(), v2.end(), v1_element) != v2.end();
    });
}

bool run_tests(json old_json, json new_json, json gen_json)
{
    if(gen_json["measurements"].size() != 8)
    {
        return false;   
    }

    if(gen_json.size() != old_json.size() )
    {
        return false;
    }

    return true;
}

int main(int argc, char** argv)
{
    std::ifstream file_orig;
    std::ifstream file_new;
    //for debugging     
    std::ofstream file_updated("config_updated.json");
    std::ofstream file_middle("config_middle.json");

    if (argc == 3)
    {
        file_orig.open(argv[1]);
        file_new.open(argv[2]);
    }
    else
    {
        std::cout << "Error: Usage ./bin/update_json <original_json_file> <new_json_file>" << std::endl;
        return EXIT_FAILURE;
    }

    json new_json = json::parse(file_new);
    json orig_json = json::parse(file_orig);
    json temp_json = orig_json;

    orig_json.update(new_json);
    orig_json.update(temp_json);

    file_middle << std::setw(4) << orig_json << std::endl;

    for (json::iterator it = orig_json.begin(); it != orig_json.end(); ++it)
    {
        if (new_json[it.key()].size() != it.value().size())
        {
            json orig_object = it.value();
            json new_object = new_json[it.key()];

            std::vector<std::string> v_old;
            std::vector<std::string> v_new;

            for (int i = 0; i < new_object.size(); i++)
            {
                for (json::iterator it3 = new_object.at(i).begin(); it3 != new_object.at(i).end(); ++it3)
                {
                    v_new.push_back(std::string(*it3));
                    break;
                }
            }

            for (int i = 0; i < orig_object.size(); i++)
            {
                for (json::iterator it4 = orig_object.at(i).begin(); it4 != orig_object.at(i).end(); ++it4)
                {
                    v_old.push_back(std::string(*it4));
                    break;
                }
            }

            sort(v_old.begin(), v_old.end());
            sort(v_new.begin(), v_new.end());

            std::vector<std::string> diff;

            set_difference(v_new.begin(), v_new.end(), v_old.begin(), v_old.end(), back_inserter(diff));

            for (auto str : diff)
            {
                std::vector<std::string>::iterator it_index = std::find(v_new.begin(), v_new.end(), str);
                int index = std::distance(v_new.begin(), it_index);

                orig_object.insert(orig_object.end(), new_object.at(index));
            }

            std::string key_str = it.key();
            orig_json[key_str] = orig_object;
        }
        else if (new_json[it.key()].size() > 1)
        {
            std::vector<std::string> v_old;
            std::vector<std::string> v_new;

            for (int i = 0; i < new_json[it.key()].size(); i++)
            {
                for (json::iterator it3 = new_json[it.key()].at(i).begin(); it3 != new_json[it.key()].at(i).end(); ++it3)
                {
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

            if (false == check_vector_same_content(v_old, v_new) ) // same size but different content
            {
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
                {
                    std::vector<std::string>::iterator it_index = std::find(v_new.begin(), v_new.end(), str);
                    int index = std::distance(v_new.begin(), it_index);

                    it.value().insert(it.value().end(), new_json[it.key()].at(index));
                }

                std::string key_str = it.key();
                orig_json[key_str] = it.value();
            }
        }
    }

    std::cout << "================ new_json ==============================" << std::endl;
    std::cout << std::setw(2) << new_json << std::endl;
    std::cout << "================ orig_json ==============================" << std::endl;
    std::cout << std::setw(2) << orig_json << std::endl;

    file_updated << std::setw(4) << orig_json << std::endl;

    std::cout << "orig_json size: " << orig_json.size() << std::endl;
    std::cout << "new_json size: " << new_json.size() << std::endl;

    return EXIT_SUCCESS;
}