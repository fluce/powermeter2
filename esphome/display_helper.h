#include "esphome.h"

void display_helper(display::Display &it, font::Font *font, std::string &state, float power, char* channel, int x, int y)
{
    if (state != "DISCONNECTED")
    {
        it.rectangle(x, y, 41, 17);
        it.print(x + 3, y + 1, roboto, channel);
        if (state != "IDLE")
        {
            if (power==power) {
                if (power >= 1000)
                {
                    it.printf(x + 12, y + 1, roboto, "%.1fkW", power / 1000);
                }
                else
                    it.printf(x + 12, y + 1, roboto, "%.0fW", power);
            }
        }
        else
        {
            it.print(x + 12, y + 1, roboto, "IDLE");
        }
    }
}
