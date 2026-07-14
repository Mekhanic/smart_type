#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

namespace {
void emit(int device, unsigned short type, unsigned short code, int value) {
    input_event event{};
    event.type = type;
    event.code = code;
    event.value = value;
    if (write(device, &event, sizeof(event)) != sizeof(event)) {
        throw std::runtime_error("failed to write uinput event");
    }
}
}

int main() try {
    const int device = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (device < 0) throw std::runtime_error("cannot open /dev/uinput");
    ioctl(device, UI_SET_EVBIT, EV_KEY);
    ioctl(device, UI_SET_KEYBIT, KEY_LEFTALT);
    ioctl(device, UI_SET_KEYBIT, KEY_LEFTSHIFT);

    uinput_setup setup{};
    std::strcpy(setup.name, "SmartType Alt Shift Test");
    setup.id.bustype = BUS_USB;
    setup.id.vendor = 0x1209;
    setup.id.product = 0x0001;
    if (ioctl(device, UI_DEV_SETUP, &setup) < 0 || ioctl(device, UI_DEV_CREATE) < 0) {
        close(device);
        throw std::runtime_error("cannot create uinput keyboard");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    emit(device, EV_KEY, KEY_LEFTALT, 1);
    emit(device, EV_SYN, SYN_REPORT, 0);
    emit(device, EV_KEY, KEY_LEFTSHIFT, 1);
    emit(device, EV_SYN, SYN_REPORT, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    emit(device, EV_KEY, KEY_LEFTSHIFT, 0);
    emit(device, EV_SYN, SYN_REPORT, 0);
    emit(device, EV_KEY, KEY_LEFTALT, 0);
    emit(device, EV_SYN, SYN_REPORT, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ioctl(device, UI_DEV_DESTROY);
    close(device);
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
