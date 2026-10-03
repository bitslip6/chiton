#include "webserver.h"
#include "request_validation.h"

namespace web {
HttpResponse rover_fn(std::vector<std::string> params) {
    if (params.size() < 2) throw std::invalid_argument("missing rover action");
    if (params[0] == "motorbegin" || params[0] == "motorend" || params[0] == "restart")
        return error_response(501, "rover control is not implemented");
    return error_response(404, "unknown rover action");
}
}
