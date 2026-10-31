// Kullanım: popup'a "example" yaz, File'a bas
#include "exec_api.h"
#include <cstdio>

static float t = 0;

static ExecAPI* g;

static void tick2(float dt) {
    t += dt;
    if (t > 5.f) {
        t = 0;
        float x, y;
        if (g->getPos(&x, &y)) {
            char buf[64];
            std::snprintf(buf, sizeof buf, "x=%d y=%d", (int)x, (int)y);
            g->notify(buf);
        }
    }
}

EXEC_EXPORT void run(ExecAPI* api) {
    g = api;
    api->notify(api->levelName());
    api->onUpdate(tick2);
}
