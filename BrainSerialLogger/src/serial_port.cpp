#include "serial_port.h"

SerialException::SerialException(const type& t, std::string s)
    : m_type(t), m_desc(std::move(s))
{
}

SerialException::type SerialException::get_type() const
{
    return m_type;
}

const std::string& SerialException::get_message() const
{
    return m_desc;
}

void SerialReader::thr_read()
{
    DWORD error{};

    while (m_keep_running && !error && !m_com.st.fEof) {
        if (!ClearCommError(m_com.handler, &m_com.err, &m_com.st)) {
            m_keep_running = false;
            continue;
        }

        const DWORD& to_read = m_com.st.cbInQue;
        if (to_read == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            std::this_thread::yield();
            continue;
        }

        auto buffer = std::unique_ptr<char[]>(new char[to_read]);
        DWORD real_read = 0;

        if (ReadFile(m_com.handler, buffer.get(), to_read, &real_read, NULL)) {
            m_lines_buf.insert(
                m_lines_buf.end(),
                buffer.get(),
                buffer.get() + static_cast<size_t>(real_read)
            );

            auto breakline_point = std::find(m_lines_buf.begin(), m_lines_buf.end(), '\n');

            if (breakline_point != m_lines_buf.end()) {
                std::string popped{ m_lines_buf.begin(), breakline_point };

                m_lines_buf.erase(m_lines_buf.begin(), breakline_point);

                while (m_lines_buf.size() && m_lines_buf.front() == '\n')
                    m_lines_buf.erase(m_lines_buf.begin());

                if (m_liner_replace) {
                    std::lock_guard<std::mutex> l(m_liner_mtx);
                    if (m_liner_replace) m_liner_replace(popped);
                }
                else {
                    std::lock_guard<std::mutex> l(m_lines_mtx);
                    m_lines_read.push_back(std::move(popped));
                }
            }
        }
    }
}

SerialReader::SerialReader(const size_t port, const DWORD baud)
{
    // Start comm
    const std::string port_string = ("\\\\.\\COM" + std::to_string(port));
    m_com.handler = CreateFileA(
        static_cast<LPCSTR>(port_string.c_str()),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (!m_com.handler) {
        throw SerialException(
            SerialException::type::CREATE_FILE,
            "Could not connect to port #" + std::to_string(port) + " with baud " + std::to_string(baud) + "."
        );
    }

    DCB dcbSerialParameters{};
    if (!GetCommState(m_com.handler, &dcbSerialParameters)) {
        CloseHandle(m_com.handler);
        m_com.handler = nullptr;

        throw SerialException(
            SerialException::type::STATE_FAIL,
            "Serial comm state was invalid."
        );
    }

    dcbSerialParameters.BaudRate = baud;
    dcbSerialParameters.ByteSize = 8;
    dcbSerialParameters.StopBits = ONESTOPBIT;
    dcbSerialParameters.Parity = NOPARITY;
    dcbSerialParameters.fDtrControl = DTR_CONTROL_ENABLE;

    //COMMTIMEOUTS cto;
    COMMTIMEOUTS timeouts = { 0, // interval timeout. 0 = not used
                              0, // read multiplier
                             10, // read constant (milliseconds)
                              0, // Write multiplier
                              0  // Write Constant
    };

    //cto.ReadIntervalTimeout = MAXDWORD;
    //cto.ReadTotalTimeoutConstant = 0;
    //cto.ReadTotalTimeoutMultiplier = 0;
    //cto.WriteTotalTimeoutMultiplier = 20;
    //cto.WriteTotalTimeoutConstant = 200;

    if (!SetCommState(m_com.handler, &dcbSerialParameters)) {
        CloseHandle(m_com.handler);
        m_com.handler = nullptr;

        throw SerialException(
            SerialException::type::STATE_SET_FAIL,
            "Could not apply baud and other communication settings."
        );
    }

    if (!SetCommTimeouts(m_com.handler, &timeouts)) {
        CloseHandle(m_com.handler);
        m_com.handler = nullptr;

        throw SerialException(
            SerialException::type::BUFFER_TIMEOUT_SET_FAILED,
            "Could not apply timeouts rules."
        );
    }

    PurgeComm(m_com.handler, PURGE_RXCLEAR | PURGE_TXCLEAR);

    // Start reading
    m_keep_running = true;
    m_lines_async_reader = std::thread([&] {thr_read(); });
}

SerialReader::~SerialReader()
{
    // Stop reading
    m_keep_running = false;
    m_lines_async_reader.join();

    // Stop comm
    CloseHandle(m_com.handler);
}

void SerialReader::set_callback(const std::function<void(std::string)> fcn)
{
    std::lock_guard<std::mutex> l(m_liner_mtx);
    m_liner_replace = fcn;
}

bool SerialReader::has_line() const
{
    return m_lines_read.size() > 0;
}

std::string SerialReader::get_line()
{
    if (m_lines_read.size() == 0) return {};

    std::lock_guard<std::mutex> l(m_lines_mtx);

    std::string popped = std::move(m_lines_read.front());
    m_lines_read.pop_front();

    return popped;
}

void SerialReader::put_string(const std::string& s)
{
    const char* buf = s.data();
    size_t total = 0;

    while (total < s.length()) {
        DWORD _sent{};
        if (!WriteFile(m_com.handler, buf + total, static_cast<DWORD>(s.size() - total), &_sent, NULL)) {
            throw SerialException(
                SerialException::type::WRITE_FILE_FAILED,
                "WriteFile function returned false. Try again or reconnect device?"
            );
        }
        total += static_cast<size_t>(_sent);
    }
}

bool SerialReader::valid() const
{
    return m_com.handler != nullptr && m_keep_running;
}

SerialReader::operator bool() const
{
    return valid();
}