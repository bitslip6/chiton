#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

#include "../include/json.hpp"

using json = nlohmann::ordered_json;
//TODO: make it a template
bool check_vector_same_content(std::vector<std::string> v1, std::vector<std::string> v2) {
    return std::all_of(v1.begin(), v1.end(), [v2](std::string v1_element) {
        return std::find(v2.begin(), v2.end(), v1_element) != v2.end();
    });
}
void fill_str_vector(json *orig_object, json *new_object, std::vector<std::string> *v_old, std::vector<std::string> *v_new) {
    for (int i = 0; i < new_object->size(); i++) {
        v_new->push_back(std::string(*new_object->at(i).begin()));
        //std::cout << std::string(*new_object->at(i).begin()) << std::endl;
    }

    for (int i = 0; i < orig_object->size(); i++) {
        v_old->push_back(std::string(*orig_object->at(i).begin()));
        //std::cout << std::string(*orig_object->at(i).begin()) << std::endl;
    }
}

void update_json_difference(json *orig_object, json *new_object, std::vector<std::string> *v_old, std::vector<std::string> *v_new) {
    sort(v_old->begin(), v_old->end());
    sort(v_new->begin(), v_new->end());

    std::vector<std::string> diff;

    set_difference(v_new->begin(), v_new->end(), v_old->begin(), v_old->end(), back_inserter(diff));

    for (auto str : diff) {
        std::vector<std::string>::iterator it_index = std::find(v_new->begin(), v_new->end(), str);
        int index = std::distance(v_new->begin(), it_index);

        orig_object->insert(orig_object->end(), new_object->at(index));
    }
}

void update_json_difference(json *orig_object, json *new_object) {
    json temp_bkp = *orig_object;

    orig_object->update(*new_object);
    orig_object->update(temp_bkp);
}

bool run_tests(json orig_json, json new_json, json gen_json) {
    if (gen_json["measurements"].size() != 8) {
        return false;
    }

    if (gen_json.empty()) {
        return false;
    }

    if (gen_json["measurements"] != orig_json["measurements"]) {
        return false;
    }

    return true;
}

int main(int argc, char **argv) {
    std::ifstream file_orig;
    std::ifstream file_new;
    //for debugging
    std::ofstream file_output("config.json.new");
    std::ofstream file_bkp("config.json.old");

    if (argc == 3) {
        file_orig.open(argv[1]);
        file_new.open(argv[2]);
    } else {
        std::cout << "Error: Usage ./bin/update_json <original_json_file> <new_json_file>" << std::endl;
        return EXIT_FAILURE;
    }

    json new_json = json::parse(file_new);
    json orig_json = json::parse(file_orig);
    json temp_json = orig_json;

    file_bkp << std::setw(4) << orig_json << std::endl;

    orig_json.update(new_json);
    orig_json.update(temp_json);

    for (json::iterator it = orig_json.begin(); it != orig_json.end(); ++it) {
        if (new_json[it.key()].size() != it.value().size()) {
            json orig_object = it.value();
            json new_object = new_json[it.key()];

            std::vector<std::string> v_old;
            std::vector<std::string> v_new;

            fill_str_vector(&orig_object, &new_object, &v_old, &v_new);

            update_json_difference(&orig_object, &new_object, &v_old, &v_new);

            std::string key_str = it.key();
            orig_json[key_str] = orig_object;
        } 
        else if (new_json[it.key()].size() > 1) { // equal size and greater than 1
            std::vector<std::string> v_old;
            std::vector<std::string> v_new;

            fill_str_vector(&it.value(), &new_json[it.key()], &v_old, &v_new);

            if (false == check_vector_same_content(v_old, v_new))  // same size but different content
            {
                update_json_difference(&it.value(), &new_json[it.key()], &v_old, &v_new);
                std::string key_str = it.key();
                orig_json[key_str] = it.value();
            } else {
                json *old_vixel = &(it.value());
                json *new_vixel = &(new_json[it.key()]);

                for (int i = 0; i < new_vixel->size(); i++) {
                    update_json_difference(&old_vixel->at(i), &new_vixel->at(i));
                }
            }
        }
    }

    // std::cout << "================ new_json ==============================" << std::endl;
    // std::cout << std::setw(2) << new_json << std::endl;
    // std::cout << "================ orig_json ==============================" << std::endl;
    // std::cout << std::setw(2) << orig_json << std::endl;

    file_output << std::setw(4) << orig_json << std::endl;

    return EXIT_SUCCESS;
}