// ConsoleApplication3.cpp : Defines the entry point for the console application.
//

#include <windows.h>

#include <string>
#include <iostream>
#include <thread>
#include <mutex>
#include <assert.h>
#include "hidapi.h"


std::string GetLastErrorAsString()
{
    //Get the error message, if any.
    DWORD errorMessageID = ::GetLastError();

    if (errorMessageID == 0)
        return std::string(); //No error message has been recorded

    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
    std::string message(messageBuffer, size);
    //Free the buffer.
    LocalFree(messageBuffer);
    return message;
}

void send(hid_device* device, bool* enabled, unsigned char color)
{
    unsigned char data[9];
    data[0] = 0;
    data[1] = (enabled[0] * 0x10) | (enabled[1] * 0x20) | (enabled[2] * 0x40) | (enabled[3] * 0x80) | 0x01; //0x03 for dimming
    data[2] = color;
    data[3] = 0;
    data[4] = 0;
    data[5] = 0;
    data[6] = 0x11;
    data[7] = 0;
    data[8] = 0;
    int written = hid_write(device, data, 9);
    assert(written > 0);
}
void send(hid_device* device, int i, unsigned char color)
{
    unsigned char data[9];
    data[0] = 0;
    data[1] = (16 * (1 << i)) | 0x01; //0x03 for dimming
    data[2] = color;
    data[3] = 0;
    data[4] = 0;
    data[5] = 0;
    data[6] = 0x11;
    data[7] = 0;
    data[8] = 0;
    int written = hid_write(device, data, 9);
    assert(written > 0);
}

enum class TYPE : int
{
    CIRCULAR,
    CIRCULAR_LINEAR,
    BEAMING,
    BOOL,
    FLASHING,
    PRESS_BEAM
};


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR    lpCmdLine,
                      _In_ int       nCmdShow)
{
    auto list = hid_enumerate(0x07ca, 0x9850);
    auto device = hid_open(0x07ca, 0x9850, nullptr);
    auto str = GetLastErrorAsString();
    std::cout << str;

    if (!device)
        return -1;

    bool pressed = false;
    std::condition_variable s;
    std::mutex m;
    TYPE type = TYPE::PRESS_BEAM;

    switch (type)
    {
        case TYPE::CIRCULAR:
        {
            std::thread([device, &pressed]()
            {
                float i = 0;
                float speed = 0.05f;

                while (true)
                {
                    if (pressed)
                        speed += 0.0004f;

                    bool enabled[4] = { false, true, false, false };
                    int s = int(i);

                    for (int j = 0; j < 4; j++)
                        enabled[j] = j == s;

                    float delta = fmod(i, 1.0f);
                    send(device, enabled, 50 * (1.0f - 2.0f * abs(delta - 0.5f)));
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    i += speed;
                    i = fmod(i, 4.0f);
                }
            }).detach();
            break;
        }

        case TYPE::CIRCULAR_LINEAR:
        {
            std::thread([device, &pressed]()
            {
                float speed = 0.1f;
                float i = 0;

                while (true)
                {
                    if (pressed)
                        speed += 0.0004f;

                    int f = int(i);
                    float l = (i) - f;
                    send(device, int(i), 55.0f * pow(1 - l, 2));
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    send(device, (int(i) + 1) % 4, 55.0f * pow(l, 2));
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    i += speed;
                    i = fmod(i, 4.0f);
                }
            }).detach();
            break;
        }

        case TYPE::BEAMING:
        {
            std::thread([device, &pressed]()
            {
                float i = 0;
                float speed = 0.05f;

                while (true)
                {
                    if (pressed)
                        speed += 0.0004f;

                    bool enabled[4] = { true, true, true, true };
                    send(device, enabled, 50 * (0.5f + 0.5f * sin(i)));
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    i += speed;
                    i = fmod(i, 4.0f);
                }
            }).detach();
            break;
        }

        case TYPE::BOOL:
        {
            std::thread([device, &pressed]()
            {
                float i = 0;
                float speed = 0.05f;

                while (true)
                {
                    if (pressed)
                        speed += 0.0004f;

                    bool enabled[4] = { true, true, true, true };
                    send(device, enabled, 55.0f * (i < 2));
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    i += speed;
                    i = fmod(i, 4.0f);
                }
            }).detach();
            break;
        }

        case TYPE::FLASHING:
        {
            std::thread([device, &pressed]()
            {
                float i = 0;
                float speed = 0.05f;
                float color = 0;

                while (true)
                {
                    if (pressed)
                        speed += 0.0004f;

                    color *= exp(-0.1f);
                    bool enabled[4] = { true, true, true, true };
                    send(device, enabled, 55 * color);
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    i += speed;

                    if (i > 16)
                        color = 1;

                    i = fmod(i, 16.0f);
                }
            }).detach();
            break;
        }

        case TYPE::PRESS_BEAM:
        {
            std::thread([device, &pressed, &s, &m]()
            {
                float i = 0;
                float speed = 0.05f;
                float color = 0;

                while (true)
                {
                    color *= exp(-0.1f);
                    bool enabled[4] = { true, true, true, true };
                    send(device, enabled, 55 * color);
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    i += speed;

                    if (pressed)
                        color = 1;

                    i = fmod(i, 16.0f);

                    if (color < 0.01)
                    {
                        auto lock = std::unique_lock<std::mutex>(m);
                        s.wait(lock);
                    }
                }
            }).detach();
            break;
        }
    }

    unsigned char data[9];

    while (true)
    {
        int read = hid_read_timeout(device, data, 8, -1);

        if (read)
            std::cout << "event! " << (data[1] ? "pressed" : "released") << std::endl;

        pressed = !!data[1];

        if (data[1])
        {
            std::unique_lock<std::mutex> lock(m);
            s.notify_all();
        }
    }

    return 0;
}