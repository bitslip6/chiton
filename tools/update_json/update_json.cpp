#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>
#include <fplus/fplus.hpp>

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

   /**
     * run command, store return code in retcode and return vector of command output string lines
     */
    std::vector<std::string> get_command_output_lines(std::string cmd, int *retcode) {
        std::string stderr_route = " 2>&1";
        //INFO("popen(%s)", cmd.c_str());

        if (cmd.rfind(stderr_route) == std::string::npos) {
            cmd.append(" 2>&1");
        }

        FILE *pipe = popen(cmd.c_str(), "r");
        //Return_If(pipe == NULL, "unable to run command: ", {});

        std::array<char, 128>    a_buffer;
        std::vector<std::string> v_output;

        while (fgets(a_buffer.data(), 128, pipe) != NULL) {
            // For now this assumes each data is a full line. Could be improved
            // to section by newlines present in string, track the post-newline
            // remainder, and append next data to it as the next actual line.
            std::string data = a_buffer.data();
            //INFO("popen [%s]", data.c_str());
            v_output.push_back(fplus::trim_right(' ', data));
        }
        //INFO("buffer done");
        //INFO("%s", fplus::show_cont(v_output).c_str());

        int ret = pclose(pipe);
        //WARN_IF(ret != 0, "return code was not 0! [%d]", ret);
        //INFO("ret code");
        //INFO("ret code: %d", ret);
        if (retcode != nullptr) { *retcode = ret; }
        //Warn_If((*retcode != 0), "command returned non-zero [%d]:", *retcode);
        //INFO("returning!");
        return v_output;
    }

    int execute_command(std::string cmd) {
        std::string null_route = " > /dev/null 2>&1";
        if (cmd.rfind(null_route) == std::string::npos) {
            cmd.append(null_route);
        }

        FILE * pipe = popen(cmd.c_str(), "r");
        //Return_IfNot(pipe, "unable to run cmd", 1);
        std::cout << "execute_command" << std::endl;

        std::array<char, 256> a_buffer;
        while (fgets(a_buffer.data(), 256, pipe) != NULL) {
            // do nothing
            std::cout << "data ----- " << a_buffer.data() << std::endl;
        }

        return pclose(pipe);
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
void adapt_to_new_json_version(json &config_json) {
    for (int vixel_index = 0; vixel_index < 8; vixel_index++) {
        std::string vixel_id = config_json["measurements"].at(vixel_index)["vixel"].dump();

        std::string vixel_id_str = vixel_id;
        config_json["measurements"].at(vixel_index)["vixel"] = vixel_id_str;
    }
}

void adapt_to_old_json_version(json &config_json) {
    for (int vixel_index = 0; vixel_index < 8; vixel_index++) {
        std::string vixel_id = config_json["measurements"].at(vixel_index)["vixel"].dump();
        char vixel_id_char = vixel_id.c_str()[1];

        config_json["measurements"].at(vixel_index)["vixel"] = static_cast<int>(vixel_id_char) - 48;
    }
} 

int main(int argc, char **argv) {
    std::ifstream file_orig;
    std::ifstream file_new;
    //for debugging
    std::ofstream file_output("config.json.new");
    std::ofstream file_bkp("config.json.old");
    int retcode = 0;

    std::vector<std::string> result_cmd;
    result_cmd = get_command_output_lines("ls -l /usr/local/chiton/current", &retcode);
    int version = std::stoi(result_cmd.at(0).substr(result_cmd.at(0).size() - 3, 3));
    std::cout << version << std::endl;

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

    if (version <= 25) {
        adapt_to_new_json_version(orig_json);
    }
    //std::cout << std::setw(4) << orig_json << std::endl;

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

    // if (version <= 25) {
    //     adapt_to_old_json_version(orig_json);
    // }

    //std::cout << std::setw(4) << orig_json << std::endl;

    file_output << std::setw(4) << orig_json << std::endl;

    return EXIT_SUCCESS;
}