#include "privileged_core_scripts.hpp"

#include <ratio>

std::map<std::string, std::vector<script_arg>> privileged_args = construct_core_args();

std::vector<script_arg> make_cary()
{
    return std::vector<script_arg>();
}

template<typename V, typename W, typename... T>
std::vector<script_arg> make_cary(V&& v, W&& w, T&&... t)
{
    std::vector<script_arg> args{{v, w}};

    auto next = make_cary(t...);

    args.insert(args.end(), next.begin(), next.end());

    return args;
}

template<typename... T>
std::vector<script_arg> make_carg(T&&... t)
{
    return make_cary(t...);
}

std::map<std::string, std::vector<script_arg>> construct_core_args()
{
    std::map<std::string, std::vector<script_arg>> ret;

    ret["cash.balance"] = make_cary();
    ret["cash.expose"] = make_cary("user", "\"\"");
    ret["scripts.get_level"] = make_cary("name", "\"\"");
    ret["scripts.me"] = make_cary();
    ret["scripts.public"] = make_cary();
    ret["cash.xfer_to"] = make_cary("user", "\"\"", "amount", "0");
    ret["cash.xfer_to_caller"] = make_cary();
    ret["scripts.core"] = make_cary();
    ret["msg.manage"] = make_cary();
    ret["msg.send"] = make_cary("channel", "\"0000\"", "msg", "\"\"");
    ret["msg.recent"] = make_cary("channel", "\"0000\"", "count", "99");
    ret["users.me"] = make_cary();
    ret["item.steal"] = make_cary("user", "\"\"", "idx", "0");
    ret["item.expose"] = make_cary("user", "\"\"");
    ret["item.manage"] = make_cary();
    ret["item.xfer_to"] = make_cary("idx", "0", "user", "\"\"");
    ret["item.bundle_script"] = make_cary("idx", "0", "name", "\"\"");
    ret["item.register_bundle"] = make_cary("idx", "0", "name", "\"\"");
    ret["cash.steal"] = make_cary("user", "\"\"", "amount", "0");
    //ret["user.port"] = make_cary();
    ret["nodes.manage"] = make_cary();
    ret["nodes.port"] = make_cary();
    ret["nodes.view_log"] = make_cary("user", "\"\"", "NID", "-1");
    ret["net.view"] = make_cary("user", "\"\"");
    ret["net.map"] = make_cary("user", "\"\"", "n", "6");
    ret["net.hack"] = make_cary("user", "\"\"");
    ret["net.access"] = make_cary("user", "\"\"");
    ret["net.switch"] = make_cary("user", "\"\"");

    return ret;
}

std::string prettify_chat_strings(std::vector<mongo_requester>& found)
{
    std::string str;

    ///STD::CHRONO PLS
    for(mongo_requester& i : found)
    {
        int64_t time_code_ms = i.get_prop_as_integer("time_ms");

        std::chrono::system_clock::time_point chron(std::chrono::seconds(time_code_ms / 1000));

        typedef std::chrono::duration<int, std::ratio_multiply<std::chrono::hours::period, std::ratio<24> >::type> days;

        std::chrono::system_clock::duration tp = chron.time_since_epoch();
        days d = std::chrono::duration_cast<days>(tp);
        tp -= d;
        std::chrono::hours h = std::chrono::duration_cast<std::chrono::hours>(tp);
        tp -= h;
        std::chrono::minutes m = std::chrono::duration_cast<std::chrono::minutes>(tp);
        tp -= m;
        std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(tp);
        tp -= s;

        int hour = h.count() % 24;
        int minute = m.count() % 60;

        std::string tstr = "`b" + format_tim(std::to_string(hour)) + format_tim(std::to_string(minute)) + "`";

        std::string msg = tstr + " `P" + i.get_prop("channel") + "`" + " " + colour_string(i.get_prop("user")) + " "  + i.get_prop("msg");

        str = msg + "\n" + str;
    }

    return str;
}

size_t get_wall_time()
{
    auto now = std::chrono::system_clock::now();
    std::chrono::duration<double, std::milli> duration = now.time_since_epoch();
    size_t real_time = duration.count();

    return real_time;
}

double get_wall_time_s()
{
    auto now = std::chrono::system_clock::now();
    std::chrono::duration<double, std::ratio<1, 1>> duration = now.time_since_epoch();
    double real_time = duration.count();

    return real_time;
}
