#include "scheduled_tasks.hpp"
#include <windows.h>

void task_thread(scheduled_tasks& tasks)
{
    sf::Clock clk;

    while(1)
    {
        tasks.check_all_tasks(clk.getElapsedTime().asMicroseconds() / 1000. / 1000.);

        Sleep(100);
    }
}

void on_finish_relink(int cnt, const std::vector<std::string>& data)
{

}
