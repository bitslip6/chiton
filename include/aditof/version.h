#ifndef VERSION_H
#define VERSION_H

#include <string>

#define ADITOF_API_VERSION_MAJOR "1"
#define ADITOF_API_VERSION_MINOR "5"
#define ADITOF_API_VERSION_PATCH "0"
#define ADITOF_API_VERSION                                                     \
    (ADITOF_API_VERSION_MAJOR "." ADITOF_API_VERSION_MINOR                     \
                              "." ADITOF_API_VERSION_PATCH)

namespace aditof {
const std::string getApiVersion() { return std::string(ADITOF_API_VERSION); }
} // namespace aditof

#endif // VERSION_H
