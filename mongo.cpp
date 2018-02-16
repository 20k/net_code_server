#include "mongo.hpp"

bool mongo_context::mongo_is_init = false;

std::mutex mongo_context::global_lock;
