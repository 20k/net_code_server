#include "scheduled_tasks.hpp"
#include <windows.h>

void task_thread(scheduled_tasks& tasks)
{
    sf::Clock clk;

    while(1)
    {
        tasks.check_all_tasks(clk.getElapsedTime().asMicroseconds() / 1000. / 1000.);

        Sleep(1000);
    }
}

void on_finish_relink(int cnt, const std::vector<std::string>& data)
{
    if(data.size() != 2)
        return;

    std::cout << "FINISHED RELINK timeout yay! " << data[0] << " " << data[1] << std::endl;
}
