#ifndef CHITON_RESOURCES_H
#define CHITON_RESOURCES_H
#include <string>
namespace resources {
// Empty result means no readable file was found. Paths never depend solely on cwd.
std::string executable_directory();
std::string find(const std::string &relative);
std::string asset(const std::string &filename);
}
#endif
