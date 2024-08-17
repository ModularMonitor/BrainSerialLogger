#pragma once

#include <windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <functional>
#include <deque>

class SerialException {
public:
    enum class type { CREATE_FILE, STATE_FAIL, STATE_SET_FAIL, WRITE_FILE_FAILED, BUFFER_TIMEOUT_SET_FAILED };
private:
    const type m_type;
    const std::string m_desc;
public:
    SerialException(const type&, std::string);

    type get_type() const;
    const std::string& get_message() const;
};

class SerialReader {
    struct comm_data {
        HANDLE handler = nullptr;
        DWORD err{};
        COMSTAT st{};
    } m_com;

    std::deque<std::string> m_lines_read;
    std::string m_lines_buf; // used to buffer, not on getline

    std::mutex m_lines_mtx;
    std::thread m_lines_async_reader;

    std::mutex m_liner_mtx;
    std::function<void(std::string)> m_liner_replace;

    bool m_keep_running = false;

    void thr_read();
public:
    SerialReader(const SerialReader&) = delete;
    SerialReader(SerialReader&&) = delete;
    void operator=(const SerialReader&) = delete;
    void operator=(SerialReader&&) = delete;

    SerialReader(const size_t port, const DWORD baud = CBR_19200);
    ~SerialReader();

    void set_callback(const std::function<void(std::string)>);

    bool has_line() const;
    std::string get_line();
    void put_string(const std::string&);

    bool valid() const;
    operator bool() const;
};
